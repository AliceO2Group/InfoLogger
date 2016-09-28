/**
 * This file defines an interface to a FIFO cache. The FIFO is filled with files
 * to transport, and in normal mode is just a memory cache. But if it is filled
 * or if the data in it is too old, files are flushed to disk.
 * 
 * This is intended to be a persistent safe FIFO cache: 
 * everything entering goes out in the same order, even if there is a shutdown in
 * the mean time. However for performance reasons, files are actually flushed to
 * disk after a while (~15s, time to fix according to the usual 'consumption rate'
 * of outgoing files).
 * 
 * One can insert a new file in the cache, get the next file to transmit, or
 * delete a file.
 * 
 *
 * @file	transport_cache.h
 * @author	sylvain.chapeland@cern.ch
*/

/* Avoid multiple includes */
#ifndef transport_cache_h
#define transport_cache_h

#include "transport_files.h"

#ifdef __cplusplus
extern "C" {
#endif


/* all functions return 0 on success, -1 on error */

/* defines a handle to a cache */
typedef void* TR_cache_handle;

/* open the cache using given directory to store persistent files.
   Cache handle is stored in the 1st argument. */
int TR_cache_open(TR_cache_handle *h, char *directory);

/* close the cache */
int TR_cache_close(TR_cache_handle *h);

/* add a file to the fifo */
int TR_cache_insert(TR_cache_handle h, TR_file *f);

/* get next file */
int TR_cache_get_next(TR_cache_handle h, TR_file **f);

/* delete files with id lesser or equal than given file id */
int TR_cache_delete(TR_cache_handle h, TR_file_id fid);

/* flush files to disk*/
int TR_cache_save(TR_cache_handle h);

/* update FIFO (timeout, etc) */
int TR_cache_update(TR_cache_handle h);

#ifdef __cplusplus
}
#endif

#endif
