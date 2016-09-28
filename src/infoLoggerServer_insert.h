/* Log message interface */
/* 07 Apr 2005 - Added run number to fields */
/* 02 Dec 2005 - Added system to fields */
/* 16 Aug 2010 - Added fields: detector, partition, errcode/line/source. */
/* fields def moved to infoLoggerMessage.h */

#include <pthread.h>


#define INSERT_TH_QUEUE_SIZE 1000

/* structure used to communicate with an insertion thread */
typedef struct _insert_th{
  struct ptFIFO *     queue;            /**< The queue of messages to be inserted */
  pthread_t           thread;           /**< Handle to the thread */  

  int                 shutdown;         /**< set to 1 to stop thread */
  pthread_mutex_t     shutdown_mutex;   /**< lock on shutdown variable */

} insert_th;


int insert_th_start(insert_th * t);
int insert_th_stop(insert_th * t);
int insert_th_loop(void* arg);
int insert_th_loop_nosql(void* arg);

