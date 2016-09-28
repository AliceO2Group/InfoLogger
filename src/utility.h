/** Various utility functions.
  *
  * Memory allocation and string duplication with integrated error handling.
  * Line-by-line reading from a file descriptor.
  * FIFO buffer.  
  *
  * This library uses simplelog functions to report errors.
  *
  * @file       utility.h
  * @see        simplelog.h
  * @author     sylvain.chapeland@cern.ch
  *
  * History:
  *   - 29/10/2004 File created.
  *   - 20/04/2005 Added environment functions
*/

/* Avoid multiple includes */
#ifndef utility_h
#define utility_h

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif


/**************************/
/* Memory functions       */
/**************************/

/** Allocate memory.
  * The same as malloc() with result checking.
  * On error, program exits by a SLOG_FATAL message.
  * Use checked_free() to free resource once unused.
  *
  * @see malloc()
*/
void *checked_malloc(size_t size);

/** Duplicate string.
  * The same as strdup() with result checking.
  * On error, program exits by a SLOG_FATAL message.
  * If string is NULL, returns NULL.
  * Use checked_free() to free resource once unused.
  *
  * @see strdup()
*/
char *checked_strdup(const char *string);

/** Free memory allocated by checked_malloc() or checked_strdup().
  *
  * @see checked_malloc()
  * @see checked_strdup()
*/
void checked_free(void *ptr);

/** Counts the number of allocations to be freed by checked_free().
*/
long checked_memstat();




/**************************/
/* Line buffering         */
/**************************/

/** C implementation of a line buffer class */
struct lineBuffer;

/** Constructor - allocates and init structure */
struct lineBuffer *lineBuffer_new();

/** Destructor - free resources */
void lineBuffer_destroy(struct lineBuffer *buf);

/** add socket content to buffer (timeout in milliseconds, -1 for blocking call) */
int lineBuffer_add(struct lineBuffer *buf, int fd, int timeout);

/** remove and get first line from buffer
 * Remove first line of buffer and returns it (null terminated)
 * User has to free it when not used any more (with free())
*/
char* lineBuffer_getLine(struct lineBuffer *buf);

/** Add NULL-terminated string to buffer. Returns 0 on success, -1 on failure (buffer full) */
int lineBuffer_addstring(struct lineBuffer *buf,char *s);


/********************************/
/* FIFO (single thread access)  */
/********************************/

/** C implementation of a FIFO class (storing pointers to void) */
struct FIFO;

/** Constructor */
struct FIFO *FIFO_new(int size);

/** Destructor */
void FIFO_destroy(struct FIFO *);

/** Read FIFO */
void* FIFO_read(struct FIFO *);

/** Write FIFO */
int FIFO_write(struct FIFO *, void *item);

/** Is FIFO full? */
int FIFO_is_full(struct FIFO *);

/** Is FIFO empty? */
int FIFO_is_empty(struct FIFO *);

/** Space left in FIFO (number of items that could be appended) */
int FIFO_get_space_left(struct FIFO *f);

/** Access first item in buffer without removing it */
void* FIFO_read_index(struct FIFO *f,int index);

/** Get the FIFO counter (number of files that have been written to it) */
int FIFO_get_total_files(struct FIFO *f);




/********************************/
/* FIFO (thread safe)           */
/********************************/

/** implementation of a thread safe FIFO with wait / sync, storing pointers to void */
struct ptFIFO;

/** Constructor */
struct ptFIFO *ptFIFO_new(int size);

/** Destructor */
void ptFIFO_destroy(struct ptFIFO *);

/** Read FIFO */
void* ptFIFO_read(struct ptFIFO *,int timeout);

/** Write FIFO */
int ptFIFO_write(struct ptFIFO *, void *item, int timeout);

/** Get ratio of FIFO buffer used (0 to 1) */
float ptFIFO_space_used(struct ptFIFO *);

/** Is FIFO empty? */
int ptFIFO_is_empty(struct ptFIFO *);


/*************************/
/* Environment functions */
/*************************/

/** Parse environment variable "name" into an integer stored in "value" */
int getenv_int(const char *name, int *value);


#ifdef __cplusplus
}
#endif


/* END #ifndef utility_h */
#endif

