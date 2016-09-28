/**
 * This is the definition of a 'file' class (with methods) to store
 * data to be transmitted.
 *
 * @file    transport_files.c
 * @see     transport_files.h
 * @author  sylvain.chapeland@cern.ch
*/


#include "transport_files.h"
#include "utility.h"
//#include "simplelog.h"

#include <stdio.h>




/** File id comparison function.
  *
  * @return   1 if fid1 > fid2 , 0 otherwise.
  */
int TR_file_id_compare(TR_file_id f1, TR_file_id f2){

  if (f1.majId<f2.majId) return 0;
  if (f1.majId>f2.majId) return 1;
  if (f1.minId<=f2.minId) return 0;
  return 1;
}




/** Create a new TR_file structure.
  * Uses checked_malloc().
*/
TR_file* TR_file_new(){
  TR_file *new_file;
  
  new_file=(TR_file *)checked_malloc(sizeof(TR_file));
  new_file->id.source=NULL;
  new_file->id.minId=-1;
  new_file->id.majId=-1;    
  new_file->path=NULL;
  new_file->first=NULL;
  new_file->last=NULL;
  new_file->size=0;

  new_file->clock=0;

  new_file->n_user=1;  /* By default, someone uses this file */
  pthread_mutex_init(&new_file->mutex,NULL);
  
  return new_file;
}




/** Destroy a TR_file structure
  * Resources (including list of TR_blobs) are deallocated with checked_free().
  * Files on disk are not destroyed, just the structure in memory.
*/
void TR_file_destroy(TR_file *f){
  TR_blob *blob_record;
  TR_blob *tmp;

  if (f==NULL) return;

  pthread_mutex_lock(&f->mutex);  

  /* should not happen, but a check does not harm */
  if (f->n_user>1) {
    //slog(SLOG_WARNING,"transport_files.c : destroying shared file");
  }

  /* destroy blob list*/
  for(blob_record=f->first;blob_record!=NULL;) {
    tmp=blob_record->next;
    checked_free(blob_record->value);
    checked_free(blob_record);
    blob_record=tmp;
  }

  /* destroy path */
  checked_free(f->path);
  
  /* destroy fid */
  checked_free(f->id.source);
  
  pthread_mutex_unlock(&f->mutex);

  pthread_mutex_destroy(&f->mutex);

  checked_free(f);

  return;
}




/** Increment the counter of users for the file.
*/
void TR_file_inc_usage(TR_file *f){
  
  pthread_mutex_lock(&f->mutex);
  f->n_user++;
  pthread_mutex_unlock(&f->mutex);
}




/** Decrement the counter of users for the file.
  * When it reaches 0, the file is destroyed.
  * It is usefull to free automatically resources used for this shared structure.
*/
void TR_file_dec_usage(TR_file *f){

  pthread_mutex_lock(&f->mutex);
  f->n_user--;
  
  if (f->n_user<=0) {
    pthread_mutex_unlock(&f->mutex);
    TR_file_destroy(f);
  } else {
    pthread_mutex_unlock(&f->mutex);
  }
}




/** Log information about the file.
    Works correctly only with NULL terminated strings in blobs.
*/
void TR_file_dump(TR_file *f){
  TR_blob *blob_record;

  //slog(SLOG_INFO,"TR_file info");
  //slog(SLOG_INFO,"File %d.%d from  %s, size %d",f->id.majId,f->id.minId,f->id.source,f->size);

  printf("File %d.%d from  %s, size %d\n",f->id.majId,f->id.minId,f->id.source,f->size);

  for(blob_record=f->first;blob_record!=NULL;blob_record=blob_record->next) {
    printf("Blob (size %d) : %s\n",blob_record->size,(char *)blob_record->value);
  }
}
