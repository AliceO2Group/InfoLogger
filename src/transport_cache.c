#include <stdio.h>
#include <pthread.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>


#include "utility.h"
#include "log.h"
#include "transport_cache.h"

/* TODO : also save 'source fid' when saving file: information currently lost */


/* debug variable */
int TR_cache_debug=0;


#define SPOOL_FILE_PREFIX "samples_"
#define TR_CACHE_SAVE_TIMEOUT	10	/* files are saved after 10 calls to TR_cache_update maximum */

/* structure holding info about a TR cache */
#define TR_CACHE_FIFO_SIZE 100
struct TR_cache_vars{
	void* chk;	/* this stores the adress of the structure, used to check handle validity */
	
	TR_file* cache[TR_CACHE_FIFO_SIZE];		/* cache in memory - circular buffer */
	int cache_start;				/* index of the first item */
	int cache_end;					/* index of the last item */
							/* 
								no item : start=end
								buffer full : end=start-1 (modulo cache_size) -> cache size - 1 items max
								last item : end-1 (modulo cache_size)
								first item : start
								there is always NOTHING at index 'end'
							*/
	
	int cache_first_new;				/* index of the first new file (not been read yet). -1 if none */
	  
	TR_file_id ondisk_maxfid;	/* max file id on disk */
	TR_file_id intable_maxfid;	/* max file id in FIFO */

	char *spooldir;	  		/* path to cache directory */
};


/* function to increment an index in the circular buffer */
void inc_cache_index(int *index){
	*index=*index+1;
	if (*index >= TR_CACHE_FIFO_SIZE) {
		*index=0;
	}
}


const char *TR_cache_file_name(TR_file_id *fid,const char* spooldir);
int TR_cache_file_write(TR_cache_handle h, TR_file *sample_file);
int TR_cache_update(TR_cache_handle h);



void TR_print_cache(struct TR_cache_vars *data){
	int i;
	for (i=0;i<TR_CACHE_FIFO_SIZE;i++) {
		printf("i=%d -> %p\n",i,data->cache[i]);
	}
}



/* open the cache using given directory to store persistent files.
   Cache handle is stored in the 1st argument. */
int TR_cache_open(TR_cache_handle *h, char *directory){

	int i;
	struct TR_cache_vars *data;
	
	*h=NULL;
	data=checked_malloc(sizeof(struct TR_cache_vars));
	if (data==NULL) {
		return -1;
	}

	data->chk=(void *)data-1;	// just a dummy code to check handle integrity

	for (i=0;i<TR_CACHE_FIFO_SIZE;i++){
		data->cache[i]=NULL;
	}
	data->cache_start=0;
	data->cache_end=0;
	data->cache_first_new=-1;

	data->ondisk_maxfid.minId=-1;
	data->ondisk_maxfid.majId=-1;

	data->intable_maxfid.minId=-1;
	data->intable_maxfid.majId=-1;

	data->spooldir=checked_strdup(directory);
	mlog(MLOG_INFO,"TR_cache %s opened",directory);

	*h=(void *)data;
	
	/* put existing files from disk in buffer */
	TR_cache_update(*h);	
	
	return 0;
}





/* close the cache */
int TR_cache_close(TR_cache_handle *hptr){

	int i;
	struct TR_cache_vars *data;
	data=(struct TR_cache_vars*)*hptr;

	/* check handle */
	if (data->chk != *hptr - 1) {
		mlog(MLOG_ERROR,"Bad TR_cache handle");		
		return -1;
	}
	
	/* flush files to disk */
	TR_cache_save(*hptr);
	
	for (i=0;i<TR_CACHE_FIFO_SIZE;i++){
		if (data->cache[i]!=NULL) {
			TR_file_dec_usage(data->cache[i]);
		}
	}

	mlog(MLOG_INFO,"TR_cache %s closed",data->spooldir);
	
	checked_free(data->spooldir);
	checked_free(data);
	*hptr=NULL;
	return 0;	
}


/* get space left in memory buffer */
int TR_cache_space_left(TR_cache_handle h){

	struct TR_cache_vars *data;
	data=(struct TR_cache_vars*)h;

	/* check handle */
	if (data->chk != h - 1) {
		mlog(MLOG_ERROR,"Bad TR_cache handle");		
		return -1;
	}


	if (data->cache_start <= data->cache_end) {
		return (TR_CACHE_FIFO_SIZE - (data->cache_end - data->cache_start) - 1);
	} else {
		/*
		return (TR_CACHE_FIFO_SIZE - (TR_CACHE_FIFO_SIZE + data->cache_end - data->cache_start) - 1);
		*/
		return data->cache_start - data->cache_end - 1;
	}
}




/* add a file to the fifo */
/* this function locks the file */
int TR_cache_insert(TR_cache_handle h, TR_file *f){

	struct TR_cache_vars *data;
	data=(struct TR_cache_vars*)h;

	/* check handle */
	if (data->chk != h - 1) {
		mlog(MLOG_ERROR,"Bad TR_cache handle");		
		return -1;
	}

	// update buffer first
	// TR_cache_update(h);

	// lock file
	pthread_mutex_lock(&f->mutex);
	
	// the file is not already in buffer I hope?
	if (!TR_file_id_compare(f->id,data->intable_maxfid) || !TR_file_id_compare(f->id,data->ondisk_maxfid)) {
		mlog(MLOG_WARNING,"File %d.%d already buffered (or not respecting order)",f->id.majId,f->id.minId);
		pthread_mutex_unlock(&f->mutex);	
		return -1;
	}


	// put in FIFO if nothing pending on disk
	if (TR_file_id_compare(data->ondisk_maxfid,data->intable_maxfid)) {

		// write file to disk
		TR_cache_file_write(h,f);		

	} else {

		// is there still some place in the FIFO?
		if (TR_cache_space_left(h)==0) {

			// FIFO full, write on disk (write first unsaved data from fifo)
			TR_cache_save(h);
			TR_cache_file_write(h,f);

		} else {

			// insert file in FIFO
			data->cache[data->cache_end]=f;

			/* this is a new file in cache, keep a track of it */
			if (data->cache_first_new == -1) {
				data->cache_first_new=data->cache_end;
			}
			
			inc_cache_index(&data->cache_end);
			
			// update max in table fid/
			if (TR_file_id_compare(f->id,data->intable_maxfid)) {
				data->intable_maxfid=f->id;
			}
			
			// we keep a reference to the file, so be sure nobody destroys it
			f->n_user++;
			
			// create timeout
			f->clock=TR_CACHE_SAVE_TIMEOUT;

		}
	}
	
	// unlock file
	pthread_mutex_unlock(&f->mutex);
	
	return 0;
}




/* get next file */
/* user must not free resource with TR_file_dec_usage() because file still in cache until delete command */
int TR_cache_get_next(TR_cache_handle h, TR_file **f){

	struct TR_cache_vars *data;
	data=(struct TR_cache_vars*)h;

	/* check handle */
	if (data->chk != h - 1) {
		mlog(MLOG_ERROR,"Bad TR_cache handle");		
		return -1;
	}


	*f=NULL;
	
	if (data->cache_first_new == -1) return -1;
	
	*f = data->cache[data->cache_first_new];
	
	/* update counter */
	inc_cache_index(&data->cache_first_new);

	/* was it the last one? */
	if (data->cache_first_new == data->cache_end) {
		data->cache_first_new = -1;
	}
	
	return 0;
}








/* select files on disk */
/* not thread safe !!! */
// global variables to change before calling is_spool_file to filter files upon their fid

TR_file_id filter_cutlow_fid;	// max fid allowed (excluded), -1 if filter disabled
TR_file_id filter_cuthigh_fid;	// min fid allowed (excluded), -1 if filter disabled
TR_file_id ondisk_maxfid;	// a variable updated while reading on disk

/* the file is matched if cuthigh<fid<cutlow */

int is_spool_file(const struct dirent *entry){
	TR_file_id fid;
	int result;
	
	if (strncmp(entry->d_name,SPOOL_FILE_PREFIX,strlen(SPOOL_FILE_PREFIX))!=0) {
		return 0;
	} else {
		fid.minId=-1;
		fid.majId=-1;
		if (sscanf(entry->d_name,SPOOL_FILE_PREFIX "%d_%d",&fid.majId,&fid.minId)!=2) {
			return 0;
		}

		/* update max on disk fid if needed */
		if (TR_file_id_compare(fid,ondisk_maxfid)) {
			ondisk_maxfid=fid;
		}

//		printf("cutlow: %d.%d ? %d.%d = %d\n",fid.majId,fid.minId,filter_cutlow_fid.majId,filter_cutlow_fid.minId,TR_file_id_compare(filter_cutlow_fid,fid));

		if ((filter_cutlow_fid.majId == -1) && (filter_cuthigh_fid.majId == -1)) {
			result = 1;
		} else if (filter_cutlow_fid.majId == -1) {
			result = TR_file_id_compare(fid,filter_cuthigh_fid);
		} else if (filter_cuthigh_fid.majId == -1) {
			result = TR_file_id_compare(filter_cutlow_fid,fid);
		} else {
			result=TR_file_id_compare(filter_cutlow_fid,fid) & TR_file_id_compare(fid,filter_cuthigh_fid);
		}
					
		return result;
	}
}



/* Reimplementation of alphasort function.
   The one from the library is unknown on Solaris.
*/

int checked_alphasort(const void *d1, const void *d2)
{   
    return strcmp(((struct dirent **)d1)[0]->d_name, ((struct dirent **)d2)[0]->d_name);
}



/* delete files with id lesser or equal than given file id */
int TR_cache_delete(TR_cache_handle h, TR_file_id fid){ 

	int ndir,i;
	struct dirent **namelist;
	static char filepath[256];
	
	struct TR_cache_vars *data;
	data=(struct TR_cache_vars*)h;

	/* check handle */
	if (data->chk != h - 1) {
		mlog(MLOG_ERROR,"Bad TR_cache handle");		
		return -1;
	}


	/* delete first those in circular buffer */
	/* they are ordered so we start from the first one */

	if (TR_cache_debug) {	
		mlog(MLOG_INFO,"TR_cache delete -> %d.%d",fid.majId,fid.minId);
	}
	
	for(;;) {
		if (data->cache_start == data->cache_end) {
			/* end of circulat buffer */
			break;
		}
		
		if (TR_file_id_compare(data->cache[data->cache_start]->id, fid) == 1) {
			/* all files in buffer now newer than threshold given */
			break;
		}
		
		// destroy file
		TR_file_dec_usage(data->cache[data->cache_start]);
		data->cache[data->cache_start]=NULL;

		/* update 'first new item' index if needed */
		if (data->cache_start == data->cache_first_new) {
			inc_cache_index(&data->cache_first_new);
			if (data->cache_first_new == data->cache_end) {
				data->cache_first_new = -1;
			}
		}

		/* update start index */
		inc_cache_index(&data->cache_start);
	}


	/* than those on disk which id <= given id */	
	filter_cutlow_fid=fid;
	filter_cutlow_fid.minId++;
	filter_cuthigh_fid.majId=-1;
	
	ndir=scandir(data->spooldir, &namelist, is_spool_file, checked_alphasort);
	if (ndir<0) {
		mlog(MLOG_ERROR,"Can't read directory %s : %s",data->spooldir,strerror(errno));
		return -1;
	}

	for(i=0;i<ndir;i++) {
		if (TR_cache_debug) {
			mlog(MLOG_INFO,"TR_cache %s : deleting %s",data->spooldir,namelist[i]->d_name);
		}
		snprintf(filepath,256,"%s/%s",data->spooldir,namelist[i]->d_name);
		unlink(filepath);
		free(namelist[i]);
	}
	free(namelist);


	/* now fill cache again */
	// TR_cache_update(h);
		
	return 0;
}







/* fill fifo with files from disk */
int TR_cache_update(TR_cache_handle h){

	int space_left,ndir,i;
	struct dirent **namelist;
	struct stat file_stat;
	TR_file *f;
	TR_file_id fid;
	
	struct TR_cache_vars *data;
	data=(struct TR_cache_vars*)h;

	/* check handle */
	if (data->chk != h - 1) {
		mlog(MLOG_ERROR,"Bad TR_cache handle");		
		return -1;
	}


	/* save if timeout on one of the files */
	i=data->cache_start;
	while (i!= data->cache_end) {
		f=data->cache[i];
		
		/* check timeout */
		if (f->clock!=0) {
			f->clock--;
			if (f->clock==0) {
				mlog(MLOG_INFO,"Saving cache %s",data->spooldir);
				TR_cache_save(h);
				break;
			}	
		}
		
		inc_cache_index(&i);
	}
	


	// get space in FIFO
	space_left = TR_cache_space_left(h);
/*
	mlog(MLOG_INFO,"fifo space left %d",space_left);
*/
	
	/* here make hysteresis to fill only after buffer has been emptied a bit */
	/*
	if (data->first_new!=-1) {
		// get new files in FIFO	
		new_files = (data->cache_end - data->cache_first_new) % TR_CACHE_FIFO_SIZE;
	}
	*/

	/* read file from disk */
	filter_cutlow_fid.majId=-1;
	filter_cuthigh_fid=data->intable_maxfid;
	ondisk_maxfid=data->ondisk_maxfid;
	
	ndir=scandir(data->spooldir, &namelist, is_spool_file, checked_alphasort);
	if (ndir<0) {
		mlog(MLOG_ERROR,"Can't read directory %s : %s",data->spooldir,strerror(errno));
		return -1;
	}

	// update maxondisk fid
	data->ondisk_maxfid=ondisk_maxfid;

	for(i=0;i<ndir;i++) {
		if (space_left) {			
					
			fid.source=NULL;
			fid.minId=-1;
			fid.majId=-1;
			sscanf(namelist[i]->d_name,SPOOL_FILE_PREFIX "%d_%d",&fid.majId,&fid.minId);
			if (TR_cache_debug) {
				mlog(MLOG_INFO,"TR_cache %s : adding %s",data->spooldir,namelist[i]->d_name);
			}

			f=TR_file_new();
			f->id=fid;
			f->path=checked_strdup(TR_cache_file_name(&f->id,data->spooldir));
			
			if (stat(f->path, &file_stat)) {
				mlog(MLOG_ERROR,"stat(\"%s\") failed",f->path);
				TR_file_destroy(f);
			} else {
				f->size=file_stat.st_size;
			
				// insert file in FIFO
				data->cache[data->cache_end]=f;

				/* this is a new file in cache, keep a track of it */
				if (data->cache_first_new == -1) {
					data->cache_first_new=data->cache_end;
				}

				inc_cache_index(&data->cache_end);

				if (data->cache_end==data->cache_start) {
					mlog(MLOG_ERROR,"cache_update : buffer limit exceeded");
				}

				// update max in table fid/
				if (TR_file_id_compare(f->id,data->intable_maxfid)) {
					data->intable_maxfid=f->id;
				}
			}

			space_left--;		
		}
		free(namelist[i]);
	}
	free(namelist);
			
	return 0;
}





/* flush files to disk*/
/* beware, may lock some files */
int TR_cache_save(TR_cache_handle h){
	
	int i;
	TR_file *f;
	
	struct TR_cache_vars *data;
	data=(struct TR_cache_vars*)h;

	/* check handle */
	if (data->chk != h - 1) {
		mlog(MLOG_ERROR,"Bad TR_cache handle");		
		return -1;
	}

	//TR_print_cache(data);
				
	i=data->cache_start;
	while (i!= data->cache_end) {
		f=data->cache[i];

		//printf("i=%d file %p\n",i,f);
		
		// lock file
		pthread_mutex_lock(&f->mutex);

		// save to disk
		TR_cache_file_write(h,f);
		
		// unlock file
		pthread_mutex_unlock(&f->mutex);
		
		inc_cache_index(&i);
	}

	return 0;
}







/* write the given file on disk */
/* file must be locked before calling the function */
int TR_cache_file_write(TR_cache_handle h, TR_file *sample_file) {

	FILE *fp;
	TR_blob *blob;

	struct TR_cache_vars *data;
	data=(struct TR_cache_vars*)h;

	// todo
	// if the file is already on disk, make a sympolic link with fid (to be configurable)
	// if (sample_file->path!=NULL)...

	// don't save if already on disk
	if (sample_file->path!=NULL) {
		return 0;
	}
	
	sample_file->path=checked_strdup(TR_cache_file_name(&sample_file->id,data->spooldir));
	fp=fopen(sample_file->path,"w");

	if (fp!=NULL) {

		if (TR_cache_debug) {
			mlog(MLOG_INFO,"Writing %s",sample_file->path);
		}

		for(blob=sample_file->first;blob!=NULL;blob=blob->next) {
			if (fwrite(blob->value,1,blob->size,fp)<=0) {
				mlog(MLOG_ERROR,"Error writing %s ... samples may be lost",sample_file->path);
			}

			/* Here, free memory once it's on disk if needed */
			
			/* no more timeout on this file */
			sample_file->clock=0;
		}

		fclose(fp);
		
		/* update max on disk fid */
		if (TR_file_id_compare(sample_file->id,data->ondisk_maxfid)) {
			data->ondisk_maxfid=sample_file->id;
		}

		
	} else {
		mlog(MLOG_ERROR,"Can not create spool file %s ... samples may be lost!",sample_file->path);
		return -1;
	}
	
	return 0;
}



/** Returns path to filename with a given fid. */
const char *TR_cache_file_name(TR_file_id *fid,const char *spooldir){
	static char filepath[256];
	
	snprintf(filepath,256,"%s/%s%08d_%010d",spooldir,SPOOL_FILE_PREFIX,fid->majId,fid->minId);

	//printf("filename : %s\n",filepath);
	
	return filepath;
}

