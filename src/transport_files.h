/**
 * This is the definition of a 'file' class (with methods) to store
 * data to be transmitted.
 *
 * @file  transport_files.h
 * @author  sylvain.chapeland@cern.ch
*/

/* Avoid multiple includes */
#ifndef transport_files_h
#define transport_files_h

#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif


/** File identifier structure definition. 
  * A major and a minor number are used.
*/
typedef struct {
  char*  source;         /**< where does the file come from? (originally) 
                              should be allocated with checked_malloc() or checked_strdup() */
                        
  int    majId;          /**< major id */
  int    minId;          /**< minor id */
  
  int    sender;         /**< reserved for server use (to know from where the files comes from) */
  void*  sender_magic;   /**< reserved for server use (fast access to sender) */
} TR_file_id;



/** The base data structure is a blob. It is part of a list */
typedef struct _TR_blob{
  void    *value;               /**< data value*/
  int     size;                 /**< data size */
  struct  _TR_blob *next;       /**< next in the list */
} TR_blob;




#define TR_FILE_STATUS_NONE 0
#define TR_FILE_STATUS_TRANSMITTED 1


/** A file is a succession of blobs */
typedef struct {
  TR_file_id id;
  
  /* the list of blobs must be allocated with checked_malloc() */
  struct _TR_blob *first;  
  struct _TR_blob *last;
  int size;                     /** Total data size */


  /* internal vars */
  char *  path;           /** Path to the directory where it is stored. NULL if not on disk 
                            should be allocated with checked_malloc() or checked_strdup() */
  int     clock;          /** A user time */  
  int     n_user;         /** The number of users of this file */
  pthread_mutex_t mutex;  /**< Mutex */
  
} TR_file;




/** File id comparison function.
  *
  * @return   1 if fid1 > fid2 , 0 otherwise.
  */
int TR_file_id_compare(TR_file_id f1, TR_file_id f2);



/** Create a new TR_file structure.
  * Uses checked_malloc().
  * @return : an initialized TR_file. NULL if failure.
*/
TR_file* TR_file_new();


/** Destroy a TR_file structure.
  * Resources (including list of TR_blobs) are deallocated with checked_free().
  * @return : nothing.
*/
void TR_file_destroy(TR_file *f);


/** Increment the counter of users for the file.
*/
void TR_file_inc_usage(TR_file *f);
/** Decrement the counter of users for the file.
  * When it reaches 0, the file is destroyed.
  * It is usefull to free automatically resources used for this shared structure.
*/
void TR_file_dec_usage(TR_file *f);


/** Log information about the file */
void TR_file_dump(TR_file *f);


#ifdef __cplusplus
}
#endif


/* END #ifndef transport_files_h */
#endif
