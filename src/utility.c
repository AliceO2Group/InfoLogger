/** Various utility functions.
  *
  * @file       utility.c
  * @see        utility.h
  * @author     sylvain.chapeland@cern.ch
  *
  * History:
  *   - 29/10/2004 File created.
  *   - 20/04/2005 Added environment functions
*/

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
#include <time.h>


#include "utility.h"
//#include "simplelog.h"


/**************************/
/* Memory functions       */
/**************************/

/* uncomment the following lines to dump alloc/free addresses to files */
/*
#define DEBUG_ALLOC   "alloc.txt"
#define DEBUG_STRDUP  "strdup.txt"
#define DEBUG_FREE    "free.txt"
*/

/* Global variables */
long              checked_mem_count=0;                                /**< Number of currently allocated memory slots */
pthread_mutex_t   checked_mem_count_mutex=PTHREAD_MUTEX_INITIALIZER;  /**< Exclusive access to checked_mem_count variable  */


/* Memory allocation */
void *checked_malloc(size_t size){
  void *ptr;

  #ifdef DEBUG_ALLOC
  FILE *fp;
  #endif
  
  /* allocate memory */
  ptr=malloc(size);
  if (ptr==NULL) {
    /* Log a fatal error. This should terminate the process. */
    //slog((SLOG_FATAL,"Can not allocate memory - malloc(%ld) failed",(long)size);
  }

  /* increase counter*/
  pthread_mutex_lock(&checked_mem_count_mutex);
  checked_mem_count++;
  pthread_mutex_unlock(&checked_mem_count_mutex);

  #ifdef DEBUG_ALLOC
  fp=fopen(DEBUG_ALLOC,"a");
  fprintf(fp,"%lp\n",ptr);
  fclose(fp);
  #endif

  /* return allocated slot */
  return ptr;
}


/* String duplication */
char *checked_strdup(const char *s){
  char *ptr;

  #ifdef DEBUG_STRDUP
  FILE *fp;
  #endif
  
  /* copy string */
  if (s==NULL) return NULL;
  ptr=strdup(s);
  if (ptr==NULL) {
    /* Log a fatal error. This should terminate the process. */
    //slog((SLOG_FATAL,"Can not allocate memory - strdtup(%ld) failed",(long)strlen(s));
  }
  
  /* increase counter*/
  pthread_mutex_lock(&checked_mem_count_mutex);
  checked_mem_count++;
  pthread_mutex_unlock(&checked_mem_count_mutex);

  #ifdef DEBUG_STRDUP
  fp=fopen(DEBUG_STRDUP,"a");
  fprintf(fp,"%lp (%s)\n",ptr,ptr);
  fclose(fp);
  #endif

  /* return allocated slot */
  return ptr;
}


/* Free allocated slot */
void checked_free(void *ptr){
  #ifdef DEBUG_FREE
  FILE *fp;
  #endif
  
  if (ptr!=NULL) {

    #ifdef DEBUG_FREE
    fp=fopen(DEBUG_FREE,"a");
    fprintf(fp,"%lp (%10s)\n",ptr,ptr);
    fclose(fp);
    #endif

    /* free pointer */
    free(ptr);
    
    /* decrease counter*/
    pthread_mutex_lock(&checked_mem_count_mutex);
    checked_mem_count--;
    pthread_mutex_unlock(&checked_mem_count_mutex);
  }
}


/* Get statistics */
long checked_memstat(){
  long i;

  /* retrieve counter */
  pthread_mutex_lock(&checked_mem_count_mutex);
  i=checked_mem_count;
  pthread_mutex_unlock(&checked_mem_count_mutex);

  return i;
}






/**************************/
/* Line buffering         */
/**************************/

#define bufferChunkSize   100       /**< Size by which buffer is increased by default */
#define bufferMaxSize     200000    /**< Maximum size of the buffer */

/** C implementation of a line buffer class */
struct lineBuffer {
  char *  buf_content;    /**< Buffer content */
  int     buf_used;       /**< Buffer size used */
  int     buf_size;       /**< Buffer size */
};

/** Constructor - allocates and init structure */
struct lineBuffer *lineBuffer_new(){
  struct lineBuffer *buf;
  buf=(struct lineBuffer *)checked_malloc(sizeof(struct lineBuffer));
  buf->buf_content=NULL;
  buf->buf_used=0;
  buf->buf_size=0;  
  return buf;
}

/** Destructor - free resources */
void lineBuffer_destroy(struct lineBuffer *buf){
  if (buf!=NULL) {

    /* use free instead of checked_free here because allocated
      or reallocated with malloc and not checked_malloc */
    free(buf->buf_content);

    checked_free(buf);
  }
}

/** Increase buffer size */
int lineBuffer_allocate(struct lineBuffer *buf, int size){
  char *newptr;

  /* Check max size of buffer */
  if (size+buf->buf_size>bufferMaxSize) {
    //slog((SLOG_WARNING,"lineBuffer_allocate() : buffer full");
    return -1;
  }

  /* Extend buffer */
  // use malloc instead of realloc when NULL, suggested by insure++ for portability
  if (buf->buf_content==NULL) {
    newptr=(char *)malloc(size);
  } else {
    newptr=(char *)realloc(buf->buf_content,size+buf->buf_size);
  }

  /* Check result */
  if (newptr!=NULL) {
    buf->buf_content=newptr;
    buf->buf_size+=size;
    return 0;
  } else {
    //slog((SLOG_WARNING,"lineBuffer_allocate() : realloc(%d) failed",size+buf->buf_size);
    return -1;
  }
}

/** add socket content to buffer (timeout in milliseconds, -1 for blocking call) */
/* return 0 on success, -1 if EOF */
int lineBuffer_add(struct lineBuffer *buf, int fd, int timeout){
  fd_set rfds;
  int ret;
  struct timeval tv;
  
  for(;;) {

    FD_ZERO(&rfds);
    FD_SET(fd,&rfds);

    /* wait new data until timeout, if any */
    if (timeout>=0) {
      tv.tv_sec=timeout/1000;
      tv.tv_usec=(timeout-tv.tv_sec*1000)*1000;
      ret=select(fd+1,&rfds,NULL,NULL,&tv);
    } else {
      ret=select(fd+1,&rfds,NULL,NULL,NULL);
    }
    
    /* check for errors */
    if (ret==-1) {
      //slog((SLOG_WARNING,"select() : %s",strerror(errno));
      break;
    }
    
    if (FD_ISSET(fd,&rfds)) {

      /* increase buffer size if needed */
      if (buf->buf_size-buf->buf_used<bufferChunkSize) {
        if (lineBuffer_allocate(buf,bufferChunkSize)<0) {
          break;
        }  
      }

      /* read data */
      ret=read(fd,&buf->buf_content[buf->buf_used],buf->buf_size-buf->buf_used-1);
      if (ret>0) {
        buf->buf_used+=ret;
        buf->buf_content[buf->buf_used]=0;
      } else if (ret==0) {
        return -1;
      } else {
        //slog((SLOG_WARNING,"read() : %s",strerror(errno));
        break;
      }
    } else {
      break;
    }  
    
    timeout=0;
  }
  return 0;
}

  
/** remove and get first line from buffer */
char* lineBuffer_getLine(struct lineBuffer *buf){
  char *endline;
  int end_pos;
  char *line;
  char *newbuf;
      
  /* buffer empty? */
  if (buf->buf_used==0) return NULL;
  
  /* where's the end of line? */
  endline=strchr(buf->buf_content,'\n');
  if (endline==NULL) return NULL;

  /* how long is it? */
  end_pos=endline-buf->buf_content+1;

  /* exactly one line... no need to copy */
  if (end_pos==buf->buf_used) {
    line=buf->buf_content;
    /* remove \n at the end */
    line[buf->buf_used-1]=0;

    buf->buf_content=NULL;
    buf->buf_used=0;
    buf->buf_size=0;
    
    return line;
  }

  /* need to copy / realloc */
  
  /* copy buffer in excess of first line */
  newbuf=(char *)malloc(buf->buf_size-end_pos);
  if (newbuf==NULL) return NULL;
  memcpy(newbuf,&buf->buf_content[end_pos],buf->buf_size-end_pos);
  
  /* realloc buffer for just first line */
  line=(char *)realloc(buf->buf_content,end_pos);
  if (line==NULL) {
    free(newbuf);
    return NULL;
  }
  /* null terminated line */
  line[end_pos-1]=0;

  /* commit buffer change */
  buf->buf_content=newbuf;
  buf->buf_size-=end_pos;
  buf->buf_used-=end_pos;

  return line;
}


/** Add NULL-terminated string to buffer. */
int lineBuffer_addstring(struct lineBuffer *buf,char *s){
  int s_size,s_avail;

  s_size=strlen(s)+1;
  s_avail=buf->buf_size-buf->buf_used;

  /* allocate memory if needed */
  if (s_avail<s_size) {
    if (lineBuffer_allocate(buf,s_size-s_avail)<0) {
      return -1;
    }
  }

  /* copy null terminated string */
  memcpy(&buf->buf_content[buf->buf_used],s,s_size);
  buf->buf_used+=s_size-1;
  return 0;
}

/** print buffer content on stdout */
void lineBuffer_print(struct lineBuffer *buf){
  int i;

  printf("Buffer : %d / %d\n",buf->buf_used,buf->buf_size);
  for(i=0;i<buf->buf_used;i++){
    printf("%c",buf->buf_content[i]);      
  }
  printf("\n---\n");
  
}






/********************************/
/* FIFO (single thread access)  */
/********************************/

/** C implementation of a FIFO class (storing pointers to void) */
struct FIFO{
  int size;
  int index_start;
  int index_end;
  void **data;
  
  int total_files;  /* a counter of the total number of files going through the FIFO */
};


/** Constructor */
struct FIFO *FIFO_new(int size){
  struct FIFO *new;
  int i;
  
  new=checked_malloc(sizeof(struct FIFO));
  new->size=size;
  new->data=checked_malloc(sizeof(void *)*(size+1));
  for(i=0;i<size;i++) {
    new->data[i]=NULL;
  }
  new->index_start=0;
  new->index_end=0;
  new->total_files=0;
  return new;
}


/** Destructor */
void FIFO_destroy(struct FIFO *f){
  if (f==NULL) return;

  /* if not empty, warning */
  if (f->index_start!=f->index_end) {
    //slog((SLOG_WARNING,"FIFO_destroy: not empty, possible memory leak");
  }

  checked_free(f->data);
  checked_free(f);
}


/** Read FIFO */
void* FIFO_read(struct FIFO *f){
  void *ret_val;

  if (f==NULL) return NULL;

  if (f->index_start==f->index_end) {
    ret_val=NULL;
  } else {
    ret_val=f->data[f->index_start];
    f->data[f->index_start]=NULL;
    f->index_start++;
    if (f->index_start>f->size) {
      f->index_start=0;
    }
  }
  return ret_val;
}

/** Write FIFO
 * @return : 0 on success, -1 on error
 */
int FIFO_write(struct FIFO *f, void *item){
  int new_index_end;

  if (f==NULL) return -1;
  if (item==NULL) return -1;  
  
  /* new end buffer position */
  new_index_end=f->index_end;
  new_index_end++;
  if (new_index_end>f->size) {
    new_index_end=0;
  }
  
  /* append new item only if space left */
  if (new_index_end==f->index_start) {
    return -1;
  }

  f->data[f->index_end]=item;
  f->index_end=new_index_end;

  f->total_files++;

  return 0;
}


/** Is FIFO full? */
int FIFO_is_full(struct FIFO *f){
  int new_index_end;
  
  if (f==NULL) return -1;
  
  /* new end buffer position */
  new_index_end=f->index_end;
  new_index_end++;
  if (new_index_end>f->size) {
    new_index_end=0;
  }
  
  if (new_index_end==f->index_start) {
    return 1;
  } else {
    return 0;
  }
}


/** Is FIFO empty? */
int FIFO_is_empty(struct FIFO *f){
  if (f==NULL) return -1;

  if (f->index_start==f->index_end) {
    return 1;
  } else {
    return 0;
  }
}


/** Space left in FIFO */
int FIFO_get_space_left(struct FIFO *f){
  int su;

  if (f==NULL) return -1;
  
  su=f->index_end-f->index_start;
  if (f->index_end<f->index_start) {
    su+=f->size+1;
  }
  
  return f->size-su;
}


/** Access first item in buffer without removing it */
void* FIFO_read_index(struct FIFO *f,int index){
  int i,j;

  if (f==NULL) return NULL;

  j=f->index_start;
/*  printf("s:%d e:%d\n",f->index_start,f->index_end);
*/
  for(i=0;i<=index;i++) {
/*    printf("%d %d\n",i,j);
    printf("-> %lp\n",f->data[j]);
*/
    if (j==f->index_end) {
      return NULL;
    }
    if (i==index) {      
      return f->data[j];
    }

    j++;
    if (j>f->size) {
      j=0;
    }
  }

  return NULL;
}


/** Get the FIFO counter */
int FIFO_get_total_files(struct FIFO *f){
  return f->total_files;
}






/********************************/
/* FIFO (thread safe)           */
/********************************/

/** implementation of a thread safe FIFO with wait / sync, storing pointers to void */

struct ptFIFO{
  int size;
  int index_start;
  int index_end;
  void **data;
  
  pthread_mutex_t   mutex;
  pthread_cond_t     cond;
};


/** Constructor */
struct ptFIFO *ptFIFO_new(int size){
  struct ptFIFO *new;
  int i;
  
  new=checked_malloc(sizeof(struct ptFIFO));
  new->size=size;
  new->data=checked_malloc(sizeof(void *)*(size+1));
  for(i=0;i<size;i++) {
    new->data[i]=NULL;
  }
  new->index_start=0;
  new->index_end=0;
  
  pthread_mutex_init(&new->mutex,NULL);
  pthread_cond_init(&new->cond,NULL);
  
  return new;
}

/** Destructor */
void ptFIFO_destroy(struct ptFIFO *f){
  if (f==NULL) return;

  /* if not empty, warning */
  if (f->index_start!=f->index_end) {
    //slog((SLOG_WARNING,"ptFIFO_destroy: not empty, possible memory leak");
  }

  pthread_mutex_destroy(&f->mutex);
  pthread_cond_destroy(&f->cond);
  
  checked_free(f->data);
  checked_free(f);
}

/** Read FIFO 
  returns on timeout (if timout >0) or immediately (timeout == 0)
  or when an item is available.
  this item is returned (or NULL if timeout).
*/
void* ptFIFO_read(struct ptFIFO *f,int timeout){
  void *ret_val;
  struct timeval now;
  struct timezone tz;
  struct timespec t;
  int t_set=0;
  int retcode;

  if (f==NULL) return NULL;

  pthread_mutex_lock(&f->mutex);

  /* wait for the FIFO to have something in */
  while (f->index_start==f->index_end) {

    /* nothing in the FIFO */  

    if (timeout==0) {
      /* no wait requested, return */
      pthread_mutex_unlock(&f->mutex);
      return NULL;      
      
    } else if (timeout>0) {
      /* timeout defined */
      if (!t_set) {
        gettimeofday(&now,&tz);
        t.tv_sec = now.tv_sec + timeout;
        t.tv_nsec = now.tv_usec * 1000;
        t_set=1;
      }

      /*
      slog(SLOG_INFO,"FIFO wait");
      */
      
      retcode = pthread_cond_timedwait(&f->cond, &f->mutex, &t);
      
      if (retcode==ETIMEDOUT) {
        /*
        slog(SLOG_INFO,"FIFO timeout");
        */
        
        /* timeout : return */
        pthread_mutex_unlock(&f->mutex);
        return NULL;      
      }

      /*
      slog(SLOG_INFO,"FIFO awaken");
      */

    } else {  
      /* wait forever */
      pthread_cond_wait(&f->cond, &f->mutex);  
    }
  }

  /* get the first item from FIFO */
  ret_val=f->data[f->index_start];
  f->data[f->index_start]=NULL;
  f->index_start++;
  if (f->index_start>f->size) {
    f->index_start=0;
  }

  /* notify FIFO update */
  pthread_cond_broadcast(&f->cond);
  
  pthread_mutex_unlock(&f->mutex);
  
  return ret_val;
}

/** Write FIFO
  returns on timeout (if timout >0) or immediately (timeout == 0)
  or when the item is written.
  returns 0 on success or -1 on timeout.
*/

int ptFIFO_write(struct ptFIFO *f, void *item, int timeout){
  int new_index_end;
  struct timeval now;
  struct timezone tz;
  struct timespec t;
  int    t_set=0;
  int    retcode;
    
  if (f==NULL) return -1;
  if (item==NULL) return -1;
  
  pthread_mutex_lock(&f->mutex);
  
  /* wait the FIFO has a free slot */
  for(;;) {
    /* new end buffer position */
    new_index_end=f->index_end;
    new_index_end++;
    if (new_index_end>f->size) {
      new_index_end=0;
    }

    /* append new item only if space left */
    if (new_index_end!=f->index_start) {
      break;
    }
    
    /* no space in FIFO */  
    if (timeout==0) {
      /* no wait requested, return */
      pthread_mutex_unlock(&f->mutex);
      return -1;      
      
    } else if (timeout>0) {
      /* timeout defined */
      if (!t_set) {
        gettimeofday(&now,&tz);
        t.tv_sec = now.tv_sec + timeout;
        t.tv_nsec = now.tv_usec * 1000;
        t_set=1;
      }
      
      retcode = pthread_cond_timedwait(&f->cond, &f->mutex, &t);
      if (retcode==ETIMEDOUT) {
        /* timeout : return */
        pthread_mutex_unlock(&f->mutex);
        return -1;      
      }

    } else {
      /* wait forever */
      pthread_cond_wait(&f->cond, &f->mutex);
    }      
  }

  f->data[f->index_end]=item;
  f->index_end=new_index_end;

  /* notify FIFO update */
  pthread_cond_broadcast(&f->cond);
  
  pthread_mutex_unlock(&f->mutex);

  return 0;
}

/** Get ratio of FIFO buffer used (0 to 1) */
float ptFIFO_space_used(struct ptFIFO *f){
  int su,size;

  if (f==NULL) return -1;
  
  pthread_mutex_lock(&f->mutex);
  su=f->index_end-f->index_start;
  size=f->size;
  if (f->index_end<f->index_start) {
    su+=f->size+1;
  }
  pthread_mutex_unlock(&f->mutex);
    
  return su*1.0/size;
}

/** Is FIFO empty? */
int ptFIFO_is_empty(struct ptFIFO *f){
  int empty=-1;
  
  if (f==NULL) return -1;

  pthread_mutex_lock(&f->mutex);
  if (f->index_start==f->index_end) {
    empty=1;
  } else {
    empty=0;
  }
  pthread_mutex_unlock(&f->mutex);
  
  return empty;
}


/*************************/
/* Environment functions */
/*************************/

/** Parse environment variable "name" into an integer stored in "value" */
/* returns 0 (and store value) if success, -1 otherwise (and error message) */
int getenv_int(const char *name, int *value){
  char *var;
  
  var=getenv(name);
  if (var==NULL) {
    //slog(SLOG_ERROR,"Variable %s undefined",name);
    return -1;
  }
  if (sscanf(var,"%d",value)!=1) {
    //slog(SLOG_ERROR,"Variable %s is not an integer: %s",name,var);
    return -1;
  }
  return 0;
}
