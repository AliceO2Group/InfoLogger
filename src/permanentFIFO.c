/* Implementation of a permanent FIFO */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <sys/stat.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdarg.h>

#include "permanentFIFO.h"
#include "utility.h"
#include "simplelog.h"

// gcc permanentFIFO.c -lpthread -Wall ./Linux/libInfo.a


/* debug routine for tests purposes */
void debug( const char * const msg, ... ) {
  va_list     ap;                           /* list of additionnal params */
  char out[1000];

  return;
  va_start(ap, msg);
  vsnprintf(out,1000,msg,ap);
  va_end(ap);

  slog(SLOG_INFO,"%s",out);
}




/**************************/
/* structures definitions */
/**************************/

/** implementation of a thread safe FIFO with wait / sync, storing pointers to void */

#define FIFO_FILE_TAG  1234     /* a static header to mark fifo structs */
#define FIFO_FILE_PERM 0666   /* file permission for fifo - use all r/w as infologgerreader started by unknown user */

/* FIFO file format stored on disk:
   - main header
   - sub header 1
   - data 1
   - sub header 2
   - data 2
   - ...
*/

/* struct at the beginning of a FIFO file */
struct FIFO_file_main_header{
  int            tag;             /* a static header */
  unsigned long  lastAckId;       /* last id acknowledged */
  unsigned long  currentId;       /* biggest item id used */
};

/* a variable to define default value */
static struct FIFO_file_main_header FIFOFILE_DEFAULT_MAIN_HEADER={FIFO_FILE_TAG,0,0};

/* struct delimiting data blocks in a FIFO file */
struct FIFO_file_sub_header{
  int            tag;      /* a static header */
  unsigned long  size;     /* data size after this header */
  unsigned long  id;       /* item id */  
};


/* a variable to define default value */
static struct FIFO_item EMPTY_FIFO_ITEM={0,NULL,0};

/* definition of circular table, used to implement FIFO structs in memory */
struct circular_table {
  int size;                  /* FIFO size */
  int index_first;           /* index of first item in table */
  int n_items;               /* number of items currently in FIFO */
  struct FIFO_item *items;   /* table containing data to be stored */
};

/* safe FIFO: save on disk on request/timeout/FIFO full, with ACK of data */
/* this struct is thread-safe */
struct permFIFO {
  struct circular_table *table_client;   /* table containing data available for FIFO consumer */
  struct circular_table *table_disk;     /* table containing data to be saved on disk */
  
  int fd;                                /* file descriptor of FIFO file buffer */  
  unsigned long currentId;               /* last id used */
    
  pthread_mutex_t   mutex;               /* mutex to access FIFO */
  pthread_cond_t     cond;               /* condition signaled when data available to read in FIFO */
  
  unsigned long lastIdOut;               /* id of the last item read from FIFO by client */
  off_t         lastOutOffset;           /* position in file of the next item after the last one read by client */
  unsigned long nextIdInClientTable;     /* id of the next item that should be inserted in client table */  
  unsigned long lastIdOnDisk;            /* id of the last item stored on disk */
  int           ackOnDisk;               /* when some ack should be done on disk */

  time_t        lastFlush;               /* time of last flush to disk */
};






/* This function creates FIFO file names from path. Result stored as a NULL terminated string in current, old, new pointers. */
/* Strings are allocated with checked_malloc, to be release with checked_free */

void permFIFO_getFileName(char *path, char **current, char **old, char **new) {
  int size;
  
  size=strlen(path)+10;
  
  if (current!=NULL) {
    *current=(char *)checked_malloc(sizeof(char)*size);
    snprintf(*current,size,"%s.fifo",path);
  }
  if (old!=NULL) {
    *old=(char *)checked_malloc(sizeof(char)*size);
    snprintf(*old,size,"%s.fifo.old",path);
  }
  if (new!=NULL) {
    *new=(char *)checked_malloc(sizeof(char)*size);
    snprintf(*new,size,"%s.fifo.new",path);
  }

}



/* Dumps the content of "on disk" FIFO file, which is used for permanent storage of a memory FIFO */
int permFIFO_file_dump(char *path){
  char *filename;
  int fd;
  struct FIFO_file_main_header hm;
  struct FIFO_file_sub_header hs;
  unsigned int bytes;
  void *data;
  
  /* open FIFO file */
  permFIFO_getFileName(path,&filename,NULL,NULL);
  fd=open(filename,O_RDONLY);
  checked_free(filename);
  if (fd==-1) return 1;
  
  /* read file header */
  hm.tag=0;
  hm.lastAckId=0;
  if (read(fd,&hm,sizeof(hm))!=sizeof(hm)) return 2;
  if (hm.tag!=FIFO_FILE_TAG) return 3;

  printf("FIFO file dump %s\n",path);
  printf("Header ok : lastAckId = %ld, currentId = %ld\n",hm.lastAckId,hm.currentId);

  /* read file data */
  for(;;) {
    /* read sub header */
    bytes=read(fd,&hs,sizeof(hs));
    if (bytes==0) break;  /* end of file */
    if (bytes!=sizeof(hs)) return 4;
    if (hs.tag!=FIFO_FILE_TAG) return 5;
    
    /* read data */
    data=checked_malloc(hs.size);
    printf("pos = %ld\n",(long)lseek(fd,0,SEEK_CUR));
    bytes=read(fd,data,hs.size);
    if (bytes!=hs.size) return 6;
    printf("Data : id = %ld, size = %ld, value = %s\n",hs.id,hs.size,(char *)data);
    checked_free(data);    
  }
  
  /* close file */
  close(fd);
  
  printf ("FIFO file is ok\n");
  
  return 0;
}





/******************************************************************/
/* The construction block of a FIFO is a circular table structure */
/******************************************************************/


/** Create circular table */
struct circular_table *ct_new(int size) {
  struct circular_table *new;
  int i;

  if (size<=0) return NULL;
      
  new=checked_malloc(sizeof(struct circular_table));
  new->size=size;
  new->index_first=0;
  new->n_items=0;
  new->items=checked_malloc(sizeof(struct FIFO_item)*size);
  for(i=0;i<size;i++) {
    new->items[i]=EMPTY_FIFO_ITEM;
  }
  
  return new;
}


/** Destroy circular table and its content */
/*  Items that have size != 0 are deleted with checked_free() */
void ct_destroy(struct circular_table *t) {
  int i;
  
  if (t==NULL) return;

  /* if not empty, delete items */
  for(i=0;i<t->size;i++) {
    if (t->items[i].size!=0) checked_free(t->items[i].data);
  }
  checked_free(t->items);
  checked_free(t);
}


/** Write in FIFO table
    Returns 0 on success or -1 on error. */
int ct_write(struct circular_table *t, struct FIFO_item *item){
  int index_new;
    
  if (t==NULL) return -1;
  if (item==NULL) return -1;
  if (t->size==t->n_items) return -1;
  if (item->data==NULL) return -1;
  
  index_new=t->index_first+t->n_items;
  if (index_new>=t->size) {
    index_new-=t->size;
  }
  t->n_items++;
  t->items[index_new]=*item;
  
  return 0;
}


/** Read from FIFO  table
    Returns 0 on success or -1 on error (or if no item available). */
int ct_read(struct circular_table *t, struct FIFO_item *item){

  if (t==NULL) return -1;
  if (item==NULL) return -1;
  
  /* empty FIFO ? */
  if (t->n_items==0) return -1;

  /* get the first item from FIFO */
  *item=t->items[t->index_first];

  /* delete from FIFO */
  t->items[t->index_first]=EMPTY_FIFO_ITEM;
  t->n_items--;
  t->index_first++;
  if (t->index_first>=t->size) {
    t->index_first-=t->size;
  }

  return 0;
}


/* save disk buffer to a file */
/* fd is positionned at the end of the file */
int ct_save(int fd,struct circular_table *t){
  struct FIFO_item item;
  struct FIFO_file_sub_header hs;
    
  if (t==NULL) return -1;
  if (fd==-1) return -1;

  hs.tag=FIFO_FILE_TAG;  

  if (lseek(fd,0,SEEK_END)==-1) return -2;

  for (;;) {
    if (ct_read(t,&item)) break;
//  debug("save: id=%ld val=*%s* size=%ld\n",item.id,(char *)item.data,item.size);
    hs.size=item.size;
    hs.id=item.id;
    if ((item.size==0)||(item.data==NULL)) continue;
    if (write(fd,&hs,sizeof(hs))!=sizeof(hs)) {
    }
    if (write(fd,item.data,hs.size)!=(long long)hs.size) {
    }
    checked_free(item.data);
  }
  
  return 0;
}


/* read table from a file */
/* fd should be positionned with correct offset to read from, should point at a "FIFO_file_sub_header" structure */
/* if "id" non zero, skip records with lower id until the one is found */
int ct_load(int fd,struct circular_table *t, unsigned long id){
  struct FIFO_item item;
  struct FIFO_file_sub_header hs;
  unsigned int bytes;
  int n_read=0;
      
  if (t==NULL) return -1;
  if (fd==-1) return -1;
  
  /* read file data */
  for(;;) {
  
    /* check space in table */
    if (t->n_items==t->size) return -3;
    
    /* read sub header */
    bytes=read(fd,&hs,sizeof(hs));
    if (bytes==0) break;  /* end of file */
    if (bytes!=sizeof(hs)) {slog(SLOG_ERROR,"read %d bytes %s\n",bytes,strerror(errno)); return 4;}
    
int i;
    for (i=0;i<(int)sizeof(hs);i++) {
	debug("%d \n",(int)(((char *)&hs)[i]));
   }
    debug ("FIFO TAG=%d\n",FIFO_FILE_TAG);
    debug ("read tag: %d\n",hs.tag);
    debug ("read size: %u\n",hs.size);
    debug ("read id: %u\n",hs.id);
    if (hs.tag!=FIFO_FILE_TAG) return 5;

    /* check/look for good id */
    if (id) {
      if (hs.id<id) {
        /* skip this one */
        if (lseek(fd,hs.size,SEEK_CUR)==(off_t)-1) {return 7;}
        continue;
      }
    }
      
    /* read data */
    item.data=checked_malloc(hs.size);
    item.size=hs.size;
    item.id=hs.id;
    
    if (item.data==NULL) {
      return -2;
    }
    bytes=read(fd,item.data,hs.size);   
    // debug("data read (%d bytes) : %s\n",bytes,(char *)item.data);
    
    if (bytes!=hs.size) {
      checked_free(item.data);
      return 6;
    }

    // debug("disk read : found item %lu, size=%lu, data = %s\n",item.id,item.size,(char *)item.data);
    
    if (ct_write(t,&item)) {
      // debug("data %s failed to table\n",(char *)item.data);
      checked_free(item.data);
      break;
    }
    n_read++;
  }
  
  debug("%d items loaded from disk\n",n_read);
  
  return 0;
}


/******************************************************************/
/* End of circular table functions                                */
/******************************************************************/








/* Clean fifo file:
     - keep only non-ack data
     - reassign id numbers starting from zero
   It is required that the fifo is not in use at the time this function is called.
*/
int permFIFO_file_clean(char *path){
  char *f1,*f2,*f3;
  int fd1,fd3;
  struct stat statbuf;

  struct FIFO_file_main_header hm;
  struct FIFO_file_sub_header hs;
  void*  buffer=NULL;
  unsigned int    buffer_size=0;
  unsigned int    bytes;
  unsigned long    currentId=0;

  /* retrieve filenames associated to FIFO */      
  permFIFO_getFileName(path,&f1,&f2,&f3);

  for(;;) {
    if (stat(f1, &statbuf)) {
      /* file does not exists */
      if (stat(f2, &statbuf)) {
        /* old file not available either, we start a blank new one */
        fd1=creat(f1,FIFO_FILE_PERM);
        if (fd1==-1) return 1;
        hm=FIFOFILE_DEFAULT_MAIN_HEADER;
        if (write(fd1,&hm,sizeof(hm))!=sizeof(hm)) return 2;
        close(fd1);       
        break;
      } else {
        /* the old file is here, rename it and clean it */
        if (rename(f2,f1)!=0) return 3;
        continue;
      }
    } else {
      /* file exists, clean it */
      fd1=open(f1,O_RDWR);
      if (fd1==-1) return 4;
      /* first remove temp files if any */
      unlink(f2);
      unlink(f3);

      /* read file header */
      if (read(fd1,&hm,sizeof(hm))!=sizeof(hm)) return 5;
      if (hm.tag!=FIFO_FILE_TAG) return 6;
      if (!hm.lastAckId) { /* no acknowledged data in the file */
        /* look for last id in file */
        for(;;){
          /* read sub header */
          bytes=read(fd1,&hs,sizeof(hs));
          if (bytes==0) break;  /* end of file */
          if (bytes!=sizeof(hs)) return 31;
          if (hs.tag!=FIFO_FILE_TAG) return 32;
          currentId=hs.id;
          /* skip to next */
          if (lseek(fd1,hs.size,SEEK_CUR)==(off_t)-1) return 34;
        }
        /* if relevant, write id in main header */
        if (currentId) {
          if (lseek(fd1,0,SEEK_SET)==(off_t)-1) return 35;
          hm.currentId=currentId;
          if (write(fd1,&hm,sizeof(hm))!=sizeof(hm)) return 36;
        }
        close(fd1);
        break;             /* FIFO file is clean */
      }

      /* copy unacknowledged data only, so start copy from "lastAckId+1" */      
      debug("cleaning !!!: copy from id=%lu\n",hm.lastAckId+1);
      debug("tag=%d  lastack=%lu   currentId=%lu\n",hm.tag,hm.lastAckId,hm.currentId);
      if (hm.lastAckId) {
       for(;;){
          /* read sub header */
          bytes=read(fd1,&hs,sizeof(hs));
          if (bytes==0) break;  /* end of file */
          if (bytes!=sizeof(hs)) return 50;
          if (hs.tag!=FIFO_FILE_TAG) return 51;
          if (hs.id>hm.lastAckId) {
            /* this one is not acknowledged, skip back and stop search */
            if (lseek(fd1,-sizeof(hs),SEEK_CUR)==(off_t)-1) return 52;
            break;
          }
          if (lseek(fd1,hs.size,SEEK_CUR)==(off_t)-1) return 53;
        }
        // debug("copying from id=%lu\n",hs.id);
      }

      
      /* create clean copy */
      fd3=creat(f3,FIFO_FILE_PERM);
      if (fd3==-1) return 7; 
      hm=FIFOFILE_DEFAULT_MAIN_HEADER;
      if (write(fd3,&hm,sizeof(hm))!=sizeof(hm)) return 8;

      /* copy data from file to file */
      for(;;){
        /* read sub header */
        bytes=read(fd1,&hs,sizeof(hs));
        if (bytes==0) break;  /* end of file */
        if (bytes!=sizeof(hs)) return 10;
        if (hs.tag!=FIFO_FILE_TAG) return 11;
        /* allocate buffer big enough to read data */
        if (hs.size>buffer_size) {
          checked_free(buffer);
          buffer_size=hs.size;
          buffer=(char *)checked_malloc(buffer_size);         
        }
        /* read data */
        bytes=read(fd1,buffer,hs.size);
        if (bytes!=hs.size) return 12;
        /* copy data */
        currentId++;
        hs.id=currentId;
        if (write(fd3,&hs,sizeof(hs))!=sizeof(hs)) return 13;
        if ((unsigned int)write(fd3,buffer,hs.size)!=hs.size) return 14;
      }
      checked_free(buffer);

      /* if relevant, write id in main header */
      if (currentId) {
        if (lseek(fd3,0,SEEK_SET)==(off_t)-1) return 41;
        hm.currentId=currentId;
        if (write(fd3,&hm,sizeof(hm))!=sizeof(hm)) return 42;
      }

      /* close files */
      close(fd1);
      close(fd3);

      /* rename files */
      if (rename(f1,f2)) return 15;
      if (rename(f3,f1)) return 16;
      if (unlink(f2))    return 17;
            
      break;
    }
  }

  checked_free(f1);
  checked_free(f2);
  checked_free(f3);
  
  return 0;
}



/* save items to disk and update "lastIdOnDisk" */
int permFIFO_save_tabledisk(struct permFIFO *f){
  int index;
  
  if (f->table_disk->n_items) {
    index=f->table_disk->index_first+f->table_disk->n_items-1;
    if (index>=f->table_disk->size) index-=f->table_disk->size;
    f->lastIdOnDisk=f->table_disk->items[index].id;
    
    /* we could compute offset of next to read from disk, but hard to find */
    
    if (ct_save(f->fd,f->table_disk)) return -1;
    f->ackOnDisk=1; /* there is now data to acknowledge on disk */
    
    debug("last Id on disk: %ld",f->lastIdOnDisk);
  }
  
  f->lastFlush=time(NULL);
  
  return 0;
}




/** permFIFO constructor */
/*
    - size : size of FIFO
    - path : path to file to be used as permanent storage (can be NULL)
*/
struct permFIFO *permFIFO_new(int size,char *path){
  char *filename=NULL;
  struct permFIFO *new;
  int fd=-1;
  int err;

  struct FIFO_file_main_header hm;
 
  if (path!=NULL) {
    /* first clean FIFO file */
    err=permFIFO_file_clean(path);
    if (err) {
      slog(SLOG_ERROR,"FIFO file cleaning failed : error %d",err);
      return NULL;
    }
    /* then open FIFO file for use */
    permFIFO_getFileName(path,&filename,NULL,NULL);
    fd=open(filename,O_RDWR);
    if (fd==-1) {
      slog(SLOG_ERROR,"Failed to open %s for writting",filename);
      checked_free(filename);
      return NULL;
    }
    checked_free(filename);
    fchmod(fd,FIFO_FILE_PERM);
  }

  new=checked_malloc(sizeof(struct permFIFO));
  new->fd=fd;
  new->currentId=0;
  new->lastIdOut=0;
  new->lastOutOffset=sizeof(struct FIFO_file_main_header);
  new->lastIdOnDisk=0;
  new->ackOnDisk=0;
  new->lastFlush=time(NULL);
        
  new->table_client=ct_new(size);

  if (path!=NULL) {
    new->table_disk=ct_new(size);
  } else {
    new->table_disk=NULL;
  }
  
  new->nextIdInClientTable=1;
    
  pthread_mutex_init(&new->mutex,NULL);
  pthread_cond_init(&new->cond,NULL);
  
  if (fd!=-1) {
    /* read file header */
    lseek(new->fd,0,SEEK_SET);
    if (read(fd,&hm,sizeof(hm))!=sizeof(hm)) return NULL;
    if (hm.tag!=FIFO_FILE_TAG) return NULL;
    new->currentId=hm.currentId;
    new->lastIdOnDisk=hm.currentId;
    if (hm.currentId) new->ackOnDisk=1;
    debug("cur=%lu, lastId=%lu\n",new->currentId, new->lastIdOnDisk);
  }
  
  return new;
}


/** permFIFO destructor */
int permFIFO_destroy(struct permFIFO *f){
  
  if (f==NULL) return -1;

  /* save FIFO */
  if (f->fd!=-1) {
    /* flush data to disk if any */
    permFIFO_save_tabledisk(f);
    /* close file */
    close(f->fd);
  }
  
  /* TODO */
  /* clean file */

  /* free structures */
  ct_destroy(f->table_client);
  ct_destroy(f->table_disk);

  pthread_mutex_destroy(&f->mutex);
  pthread_cond_destroy(&f->cond);
  
  checked_free(f);
  return 0;
}









/** Read FIFO 
  returns on timeout (if timout >0) or immediately (timeout == 0)
  or when an item is available (timeout<0).
  Timeout is in seconds.
  This item is returned in variable "item", with return code 0.
  If nothing is available upon timeout, 1 is returned.
  On error, -1 is returned.
*/
int permFIFO_read(struct permFIFO *f, struct FIFO_item *item, int timeout){
  int retcode=0;
  int i,j,k;
  unsigned int nextid;
  struct circular_table *t;
  struct FIFO_item item_copy;
  struct timeval now;
  struct timezone tz;
  struct timespec ts;
  int t_set=0;
          
  if (f==NULL) return -1;
  if (item==NULL) return -1;

  /* init return variable */
  *item=EMPTY_FIFO_ITEM;

debug("read?\n");

  /* loop until timeout or something arrived */
  pthread_mutex_lock(&f->mutex);

  /* wait for the FIFO to have something in */
  while (f->lastIdOut>=f->currentId) {

    debug("read: nothing in FIFO: last=%lu, cur=%lu\n",f->lastIdOut,f->currentId);
    /* nothing in the FIFO */  
    if (timeout==0) {
      /* no wait requested, return now */
      pthread_mutex_unlock(&f->mutex);
      return 1;     
    } else if (timeout>0) {
      /* timeout defined */
      if (!t_set) {
        gettimeofday(&now,&tz);
        ts.tv_sec = now.tv_sec + timeout;
        ts.tv_nsec = now.tv_usec * 1000;
        t_set=1;
      }      
      retcode = pthread_cond_timedwait(&f->cond, &f->mutex, &ts);
      if (retcode==ETIMEDOUT) {
        /* timeout : return */
        pthread_mutex_unlock(&f->mutex);
        return 1;
      }
    } else {  
      /* wait forever */
      pthread_cond_wait(&f->cond, &f->mutex);  
    }
    /* give no other chance */
    timeout=0;
  }

debug("read from table\n");

  /* try to read from memory first */   
  if (ct_read(f->table_client,item)) {

    /* in case we use a disk FIFO */
    if (f->table_disk) {

      debug("nothing in client buffer, look elsewhere for id=%lu\n",f->lastIdOut+1);
      /* failed, try to load from disk buffer */
      t=f->table_disk;
      nextid=f->lastIdOut+1;
      k=0;
      for (i=0;i<t->n_items;i++) {
        j=t->index_first+i;
        if (j>=t->size) {
          j-=t->size;
        }
        debug("disk buffer: found id %lu : %s\n",t->items[j].id,(char *)t->items[j].data);
        if (t->items[j].id<nextid) {continue;}
        if (t->items[j].id!=nextid) {break;}
        debug("that's the good one \n");

        /* copy item */
        item_copy=t->items[j];
        item_copy.data=checked_malloc(t->items[j].size);
        if (item_copy.data==NULL) {
          retcode=1;
          break;
        }  
        memcpy(item_copy.data,t->items[j].data,t->items[j].size);
        debug("copy: %lu = (%lu bytes) %s\n",item_copy.id,item_copy.size,(char *)item_copy.data);

        /* the first one we return, the following we insert in client table */
        if (k==0) {
          *item=item_copy;
          debug("copied item: %lu = (%lu bytes) %s\n",item->id,item->size,(char *)item->data);            
        } else {
          if (ct_write(f->table_client,&item_copy)) {
            checked_free(item_copy.data);
            break;
          }
        }

        /* one more item found */
        k++;
        f->nextIdInClientTable=item_copy.id+1;

        nextid++;
      }
      debug("loaded %d items from disk buffer\n",k);

      /* read to disk only if nothing valuable in memory disk buffer */
      if ((k==0)&&(retcode==0)) {
        debug("nothing in mem, load from disk buffer\n");

        retcode=lseek(f->fd,f->lastOutOffset,SEEK_SET);
        debug("read disk: lseek %ld : %d\n",f->lastOutOffset,retcode);
        retcode=ct_load(f->fd,f->table_client,f->lastIdOut+1);
        debug("ct_load : %d\n",retcode);
        if (!retcode) {
	  f->lastOutOffset=lseek(f->fd,0,SEEK_CUR);
	}
                
        /* TODO: check if FIFO corruption ... in this case, try to recover */

        /* update f->nextIdInClientTable */
        if (f->table_client->n_items>0) {
          i=f->table_client->index_first+f->table_client->n_items-1;
          if (i>=f->table_client->size) {
            i-=f->table_client->size;
          }
          f->nextIdInClientTable=f->table_client->items[i].id+1;
        }

        retcode=ct_read(f->table_client,item);
      }
    } else {
      retcode=1;
    }
  }

  /* an item was read from FIFO */
  if (!retcode) {
    /* keep id of last item read by client */
    f->lastIdOut=item->id;
  }
   
  /* unlock mutex */
  pthread_mutex_unlock(&f->mutex);

  return retcode;
}









/** Write item in FIFO
    if size is 0, assumes it is a NULL terminated chain and duplicates it.
    returns 0 on success or -1 on error.
*/

int permFIFO_write(struct permFIFO *f, void *data, int size){
  struct FIFO_item item;
  struct FIFO_item item_copy;
  int retcode=0;
  int do_copy=0;
        
  /* check parameters */
  if (f==NULL) return -1;
  if (data==NULL) return -1;
  if (size<0) return -1;
  
  /* create a FIFO item with given data */
  item=EMPTY_FIFO_ITEM;
  if (size==0) {
    item.size=strlen((char *)data)+1;
    item.data=checked_strdup((char *)data);
    if (item.data==NULL) return -1;
  } else {
    item.data=data;
    item.size=size;
  }

  pthread_mutex_lock(&f->mutex);
  f->currentId++;
  item.id=f->currentId;
  debug("ITEM ID=%lu\n",item.id);
  if (f->table_disk!=NULL) {
    /* try to write to client table if no data to be read from disk first */
    if (item.id==f->nextIdInClientTable) {
      if (ct_write(f->table_client,&item)) {
        /* no more items in client table */
        f->nextIdInClientTable=0;
        /* flush disk buffer */
        permFIFO_save_tabledisk(f);
      } else {
        /* make a copy only if inserted in client table */
        do_copy=1;
        /* update next Id that should come in */
        f->nextIdInClientTable++;
        
        debug ("item %lu (%s) written in client table\n",item.id,(char *)item.data);
      }
    }
    /* write data to disk buffer */
    item_copy=item;
    if (do_copy) {
      item_copy.data=checked_malloc(item.size);
      if (item_copy.data!=NULL) {
        memcpy(item_copy.data,item.data,item.size);
      }
    }
    if (ct_write(f->table_disk,&item_copy)) {
      /* flush table to disk and retry */
      permFIFO_save_tabledisk(f);
      retcode=ct_write(f->table_disk,&item_copy);
      if ((retcode)&&(do_copy)) {
        checked_free(item_copy.data);
        /* remove from client table as well */
        f->table_client->n_items--;
      }
    }
  } else {
    retcode=ct_write(f->table_client,&item);
  }

  /* notify FIFO update */
  pthread_cond_broadcast(&f->cond);
 
  pthread_mutex_unlock(&f->mutex);

  return retcode;
}






int permFIFO_ack(struct permFIFO *f, unsigned long id){
  int i;
  struct circular_table *t;
  struct FIFO_file_main_header hm;
  
  /* check parameters */
  if (f==NULL) return -1;
  pthread_mutex_lock(&f->mutex);
  
  /* first delete from disk buffer acknowledged items */
  t=f->table_disk;
  if (t==NULL) {
    pthread_mutex_unlock(&f->mutex);
    return -1;
  }
  for(i=t->index_first;t->n_items;){   
    /* id ok? */
    if (t->items[i].id>id) break;
    /* remove item */
    debug("ACK : delete %lu\n",t->items[i].id);
    if (t->items[i].size!=0) checked_free(t->items[i].data);
    t->items[i]=EMPTY_FIFO_ITEM;
    /* next item */
    i++;
    if (i>=t->size) {
      i-=t->size;
    }
    t->n_items--;
    t->index_first=i;
  }

  debug("ACK on disk for %lu ?\n",id);

  /* if nothing worth on disk, return */
  if (!f->ackOnDisk) {
    pthread_mutex_unlock(&f->mutex);
    return 0;
  }

  debug("ACK on disk: going on \n",id);
  
  /* ack all ? */
  if (f->lastIdOnDisk<=id) {
    debug("Reset fifo file");
    /* then we just drop the file */
    if (ftruncate(f->fd,sizeof(struct FIFO_file_main_header))) {
      slog(SLOG_ERROR,"Could not truncate fifo file");
    } else {
      f->lastOutOffset=sizeof(struct FIFO_file_main_header);
    }
    /* no need to further ack on disk if it's the last one */
    f->ackOnDisk=0;
  }
  
  hm.tag=FIFO_FILE_TAG;
  hm.lastAckId=id;
  hm.currentId=f->lastIdOnDisk;
  /* if not found, then update disk info */
  if (lseek(f->fd,0,SEEK_SET)==(off_t)-1) {
    pthread_mutex_unlock(&f->mutex);
    return 1;
  }
  if (write(f->fd,&hm,sizeof(hm))!=sizeof(hm)) {
    pthread_mutex_unlock(&f->mutex);
    return 2;  
  }
  
  /* write id of last acknowledged, because offset is hard to remember... */
  /* will just take more time at startup when cleaning */
    
  /* TODO */
  /* if currentid=id+1 ... grand menage */

  pthread_mutex_unlock(&f->mutex);
  return 0;  
}






/** Flush data to disk */
int permFIFO_flush(struct permFIFO *f, int timeout){
  int retcode;
  
  if (f==NULL) return -1;
  pthread_mutex_lock(&f->mutex);
  if (time(NULL)>=f->lastFlush+timeout) {
    retcode=permFIFO_save_tabledisk(f);
//    slog(SLOG_INFO,"FIFO flushed to disk after timeout");
  } else {
//    slog(SLOG_INFO,"FIFO flush - timeout not reached yet");
    retcode=0;
  }
  pthread_mutex_unlock(&f->mutex);
  return retcode;
}









/***************************************/
/* test functions for various features */
/***************************************/


int test1() {
  char *path="/tmp/fifotest";

  struct circular_table *fifo;
  int i;
  char *buf;
  struct FIFO_item item;
  
  printf("Path=%s\n",path);
  printf("FIFO file dump  : %d\n",permFIFO_file_dump(path));  
  printf("FIFO file clean : %d\n",permFIFO_file_clean(path));
  printf("FIFO file dump  : %d\n",permFIFO_file_dump(path));


  fifo=ct_new(10);
  printf("FIFO=%p\bn",(void *)fifo);
  for(i=0;i<15;i++){
    buf=malloc(1000);
    snprintf(buf,1000,"Test %d",i);
    item=EMPTY_FIFO_ITEM;
    item.size=strlen(buf);
    item.data=buf;
    
    if (ct_write(fifo,&item)) {
      free(buf);
      printf("%d -> insert failed\n",i);
    } else {
      printf("%d -> insert ok\n",i);
    }
  }
  for(;;) {
    if (ct_read(fifo,&item)) break;
    printf("%ld : %s\n",item.id,(char *)item.data);
    free(item.data);
  }
  ct_destroy(fifo);
  
  return 0;
}


int test2() {
  char *path="/tmp/fifotest";
  struct permFIFO *f;
  int i,j,k;
  char *buf;
  struct FIFO_item item;


  f=permFIFO_new(100,path);
  if (f==NULL) {printf("failed to create FIFO\n"); return 1;}
  
  if (!permFIFO_read(f,&item,0)) {printf("read something from empty fifo!\n"); return 1;}

  k=0;
  for (j=0;j<1;j++){
    for(i=0;i<70.0*rand()/RAND_MAX;i++){
      k++;      
      buf=malloc(1000);
      snprintf(buf,1000,"Test %d",k);
      if (permFIFO_write(f,buf,strlen(buf)+1)) {
        free(buf);
        printf("%d -> insert failed\n",k);
      } else {
        printf("%d -> insert ok\n",k);
      }
    }
    for(i=0;i<70.0*rand()/RAND_MAX;i++) {
      if (permFIFO_read(f,&item,0)) break;
      printf("%ld : %s\n",item.id,(char *)item.data);
      free(item.data);
    }
  }
  printf ("emptying:\n");
  /* empty remaining stuff */
  for(;;) {
    if (permFIFO_read(f,&item,0)) break;
    printf("%ld : %s\n",item.id,(char *)item.data);
    free(item.data);
  }
  permFIFO_destroy(f);
//  printf("FIFO file dump  : %d\n",permFIFO_file_dump(path));
  return 0;
}

int test3() {
  char *path="/tmp/fifotest";
  struct permFIFO *f;
  int i,k;
  char *buf;
  struct FIFO_item item;


  f=permFIFO_new(5,path);
  if (f==NULL) {printf("failed to create FIFO\n"); return 1;}
  
  if (!permFIFO_read(f,&item,0)) {printf("read something from empty fifo!\n"); return 1;}

  k=0;
  for(i=0;i<20;i++){
    k++;
    buf=malloc(1000);
    snprintf(buf,1000,"Test %d",k);
    if (permFIFO_write(f,buf,strlen(buf)+1)) {
      free(buf);
      printf("%d -> insert failed\n",k);
    } else {
      printf("%d -> insert ok\n",k);
    }
  }
  for(i=0;i<15.0;i++) {
    if (permFIFO_read(f,&item,0)) break;
    printf("READ = %ld : %s\n",item.id,(char *)item.data);
    free(item.data);
  }
  for(i=0;i<5;i++){
    k++;
    buf=malloc(1000);
    snprintf(buf,1000,"Test %d",k);
    if (permFIFO_write(f,buf,strlen(buf)+1)) {
      free(buf);
      printf("%d -> insert failed\n",k);
    } else {
      printf("%d -> insert ok\n",k);
    }
  }

  printf ("emptying:\n");
  /* empty remaining stuff */
  for(;;) {
    if (permFIFO_read(f,&item,0)) break;
    printf("READ = %ld : %s\n",item.id,(char *)item.data);
    free(item.data);
  }
  permFIFO_destroy(f);
//  printf("FIFO file dump  : %d\n",permFIFO_file_dump(path));
  return 0;
}


/* try insertion */
int test4() {
  char *path="/tmp/fifotest";
  struct permFIFO *f;
  int i,k;
  char *buf;
  struct FIFO_item item;  

  f=permFIFO_new(5,path);
  if (f==NULL) {printf("failed to create FIFO\n"); return 1;}

  k=0;
  for(i=0;i<10;i++){
    k++;
    buf=malloc(1000);
    snprintf(buf,1000,"Test %d",k);
    if (permFIFO_write(f,buf,strlen(buf)+1)) {
      free(buf);
      printf("%d -> insert failed\n",k);
    } else {
      printf("%d -> insert ok\n",k);
    }
  }
  permFIFO_destroy(f);
  
  printf("FIFO file dump  : %d\n",permFIFO_file_dump(path));  
  
  f=permFIFO_new(7,path);
  if (f==NULL) {printf("failed to create FIFO\n"); return 1;}
  printf ("emptying:\n");
  /* empty remaining stuff */
  for(;;) {
    if (permFIFO_read(f,&item,0)) break;
    printf("READ = %ld : %s\n",item.id,(char *)item.data);
    free(item.data);
  }
  permFIFO_destroy(f);
//  printf("FIFO file dump  : %d\n",permFIFO_file_dump(path));

  printf("FIFO file dump  : %d\n",permFIFO_file_dump(path));  
  return 0;
}


/* try ack */
int test5() {
  char *path="/tmp/fifotest";
  struct permFIFO *f;
  int i,k;
  char *buf;
  struct FIFO_item item;  

  f=permFIFO_new(10,path);
  if (f==NULL) {printf("failed to create FIFO\n"); return 1;}

  k=0;
  for(i=0;i<50;i++){
    k++;
    buf=malloc(1000);
    snprintf(buf,1000,"Test %d",k);
    if (permFIFO_write(f,buf,strlen(buf)+1)) {
      free(buf);
      printf("%d -> insert failed\n",k);
    } else {
      printf("%d -> insert ok\n",k);
    }
  }
  /* read buffer */
  for(;;) {
    if (permFIFO_read(f,&item,0)) break;
    printf("READ = %ld : %s\n",item.id,(char *)item.data);
    free(item.data);
  }
  permFIFO_ack(f,49);
  permFIFO_destroy(f);
  
  printf("FIFO file dump  : %d\n",permFIFO_file_dump(path));  
  
  f=permFIFO_new(7,path);
  if (f==NULL) {printf("failed to create FIFO\n"); return 1;}
  printf ("emptying:\n");
  /* empty remaining stuff */
  for(;;) {
    if (permFIFO_read(f,&item,0)) break;
    printf("READ = %ld : %s\n",item.id,(char *)item.data);
    free(item.data);
  }
  permFIFO_destroy(f);
//  printf("FIFO file dump  : %d\n",permFIFO_file_dump(path));

  printf("FIFO file dump  : %d\n",permFIFO_file_dump(path));  
  return 0;
}


/* try full loop*/
int test6() {
  char *path="/tmp/fifotest";
  struct permFIFO *f;
  int i,k;
  char *buf;
  struct FIFO_item item;  
  unsigned long rid=0;

  f=permFIFO_new(10,path);
  if (f==NULL) {printf("failed to create FIFO\n"); return 1;}

  k=0;
  
  for(;;) {
    for(i=0;i<rand()*15.0/RAND_MAX;i++){
      k++;
      buf=malloc(1000);
      snprintf(buf,1000,"Test %d",k);
      if (permFIFO_write(f,buf,strlen(buf)+1)) {
        free(buf);
        printf("%d -> insert failed\n",k);
        exit(0);
      }
    }
    /* read buffer */
    for(i=0;i<rand()*10050.0/RAND_MAX;i++){
      if (permFIFO_read(f,&item,0)) {
        printf("done\n");
        break;
      }
      printf("READ = %ld : %s\n",item.id,(char *)item.data);
      rid=item.id;
      free(item.data);
    }

    permFIFO_ack(f,rid-2);
    
    if (k>10) break;
  }
  permFIFO_destroy(f);

  permFIFO_file_clean(path);  
  printf("FIFO file dump  : %d\n",permFIFO_file_dump(path));  
  return 0;
}


int _main() {
  test6();
  return 0;
}
