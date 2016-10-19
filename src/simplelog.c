/** Simple logging facility.
  *
  * @file       simplelog.c
  * @see        simplelog.h
  * @author     sylvain.chapeland@cern.ch
  *
  * History:
  *   - 29/10/2004 File created.
*/

#include "simplelog.h"

//#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>
#include <pthread.h>

#include <Common/SimpleLog.h>

SimpleLog *theLog=NULL;


void setSimpleLog(void *ptr) {
  theLog=(SimpleLog *)ptr;
}


/*************/
/* Constants */
/*************/
#define SLOG_MAX_SIZE 300                                /**< Maximum size of a formatted log message */

#define SLOG_STR_FATAL    "[FATAL]"                      /**< String for severity FATAL */
#define SLOG_STR_ERROR    "[ERROR]"                      /**< String for severity ERROR */
#define SLOG_STR_WARNING  "[WARN] "                      /**< String for severity WARNING */
#define SLOG_STR_INFO     "[INFO] "                      /**< String for severity INFO */
#define SLOG_STR_DEBUG    "[DEBUG]"                      /**< String for severity DEBUG */
#define SLOG_STR_UNKNOWN  "[????] "                      /**< String for unknown severity */


/********************/
/* Global variables */
/********************/
int             slog_no_debug=1;                         /**< DEBUG messages discarded if non-zero */
FILE*           slog_file=NULL;	                         /**< Log file */
int             slog_file_opened=0;                      /**< set to 1 when file opened */
int             slog_file_options;                       /**< file creation options */
char*           slog_file_path;                          /**< file path */
pthread_mutex_t mutex_log=PTHREAD_MUTEX_INITIALIZER;     /**< Mutex so that only one thread can configure slog() at a time */
void           (*slog_exit)(void);                       /**< Callback for FATAL messages before exit() */


/******************/
/* Implementation */
/******************/

/* create log file - uses slog_file_path and slog_file_options */
static int slog_file_open();


/** Format a log message.
  * @param    buffer    char buffer to store the message in.
  * @param    size      buffer size. Should be at least SLOG_MAX_SIZE.
  * @param    severity  message severity.
  * @param    message   message to format.
  * @param    ap        additionnal parameters (if message printf-like formatted).
  * @return             0 if success, or the number of characters discarded because buffer too small (-1 if unknown).
  *
  * buffer is filled with a formatted log message.
*/
int slog_vformat(char *buffer, size_t size, slog_Severity severity, const char *message, va_list ap){

  const char        *level_msg;             /* message severity string */
  time_t      now;                          /* time now */
  struct tm   tm_str;                       /* time struct for now */
  int         len;                          /* current message length */    
  int         len_available;                /* number of characters left in buffer */         
  int         n;                            /* counter */

  /* select string for message severity */
  switch (severity) {
    case SLOG_FATAL:
       level_msg=SLOG_STR_FATAL;
       break;
    case SLOG_ERROR:
       level_msg=SLOG_STR_ERROR;
       break;
    case SLOG_WARNING:
       level_msg=SLOG_STR_WARNING;
       break;      
    case SLOG_INFO:
       level_msg=SLOG_STR_INFO;
       break;
    case SLOG_DEBUG:
       level_msg=SLOG_STR_DEBUG;
       break;
    default:
       level_msg=SLOG_STR_UNKNOWN;
       break;
  }

  /* format message timestamp */
  now = time(NULL);
  localtime_r(&now,&tm_str);
  len=strftime(buffer, size, "%Y-%m-%d %T ", &tm_str);
  if ((len>=(int)size)||(len<=0)) return -1;
  snprintf(&buffer[len],size-len,"%s ",level_msg);
  len+=strlen(level_msg)+1;
  
  /* keep space for \n */
  len_available = size-len-1;
  if (len_available<=0) return -1;

  /* Format message */
  n=vsnprintf(&buffer[len],len_available,message,ap);   

  /* Add return char if none */
  len=strlen(buffer);
  if (buffer[len-1]!='\n') {
    sprintf(&buffer[len],"\n");
  }

  /* return missing space, or -1 if unknown */
  if (n<0) return -1;
  if (n>=len_available) return (n-len_available+1);
  return 0;
}


/** Format a log message.
  * @param    buffer    char buffer to store the message in.
  * @param    size      buffer size. Should be at least SLOG_MAX_SIZE.
  * @param    severity  message severity.
  * @param    message   message to format.
  * @param    ..        additionnal parameters (if message printf-like formatted).
  * @return             0 if success, or the number of characters discarded because buffer too small (-1 if unknown).
  *
  * buffer is filled with a formatted log message.
*/
int slog_format(char *buffer, size_t size, slog_Severity severity, const char *message, ...){

  int         err;                          /* error code */
  va_list     ap;                           /* list of additionnal params */
  
  va_start(ap, message);
  err=slog_vformat(buffer,size,severity,message, ap);
  va_end(ap);
  return err;
}


/** Log a message.
*/
void slog(slog_Severity severity, char *message, ...){

  va_list     ap;                           /* list of additionnal params */
  char        buffer[SLOG_MAX_SIZE];        /* buffer to format message */
  int         buffer_short;                 /* missing space in buffer */
  FILE        *fp;                          /* output file */
  void        (*checked_exit)(void);             /* callback for FATAL messages */
  
  /* skip debug messages if needed */
  if ((severity==SLOG_DEBUG) && slog_no_debug) return;

  /* special handling - redirection to SimpleLog when available */
  if (theLog!=NULL) {
    va_start(ap, message);
    vsnprintf(buffer,sizeof(buffer),message,ap);
    va_end(ap);
    switch(severity) {
      case SLOG_WARNING:
        theLog->warning("%s",buffer);
        break;
      case SLOG_ERROR:
      case SLOG_FATAL:
        theLog->warning("%s",buffer);
        break;
      default:
        theLog->info("%s",buffer);
        break;
    }                
    return;
  }


  /* format message */
  va_start(ap, message);
  buffer_short=slog_vformat(buffer,sizeof(buffer),severity,message,ap);
  va_end(ap);

  /* CRITICAL SECTION - start */
  pthread_mutex_lock(&mutex_log);

  /* create file if not done yet */
  if (slog_file_opened==0) {
    slog_file_open();
  }
  
  fp=slog_file;
  if (fp==NULL) fp=stdout;

  /* print message */
  fprintf(fp,"%s",buffer);

  /* check if the buffer was big enough for the message */
  if (buffer_short) {
    slog_format(buffer,sizeof(buffer),SLOG_WARNING,"Last log truncated - %d bytes lost",buffer_short);
    fprintf(fp,"%s",buffer);
  }

  /* flush file */
  fflush(fp);

  /* exit on fatal messages */
  if (severity==SLOG_FATAL) {
    if (slog_exit!=NULL) {
      checked_exit=slog_exit;
      slog_exit=NULL;       /* to avoid recursive calls of slog(FATAL) */
      checked_exit();
    }
    if (slog_file!=NULL) {
      slog_format(buffer,sizeof(buffer),SLOG_INFO,"Closing log file");
      fprintf(slog_file,"%s",buffer);
      fclose(slog_file);
    }
    exit(1);
  }
  
  /* CRITICAL SECTION - end */
  pthread_mutex_unlock(&mutex_log);
    
  return;
}


/** Turns on logging of debugging messages.
*/
void slog_debug_on(){
  pthread_mutex_lock(&mutex_log);
  slog_no_debug=0;
  pthread_mutex_unlock(&mutex_log);
}


/** Turns off logging of debugging messages. This is the default mode.
*/
void slog_debug_off(){
  pthread_mutex_lock(&mutex_log);
  slog_no_debug=1;
  pthread_mutex_unlock(&mutex_log);
}



/* uses slog_file_options */
static int slog_file_open() {
  char        buffer[SLOG_MAX_SIZE];        /* buffer for log messages */
  int         err=0;                        /* error code */
  
  /* Open new log file */
  if (slog_file_path!=NULL) {
    if (slog_file_options & SLOG_FILE_OPT_CLEAR_IF_EXISTS) {
      slog_file=fopen(slog_file_path,"w");
    } else {
      slog_file=fopen(slog_file_path,"a");
    }
    if (slog_file==NULL) err=-1;
  }

  /* Info on the new log file */
  if (err==-1) {
    slog_format(buffer,sizeof(buffer),SLOG_ERROR,"Could not open log file %s, using stdout",slog_file_path);
    fprintf(stdout,"%s",buffer);
    fflush(stdout);
  } else {
    if (slog_file!=NULL) {
      slog_format(buffer,sizeof(buffer),SLOG_INFO,"Opening log file");
      fprintf(slog_file,"%s",buffer);
      fflush(slog_file);
    }
  }
  
  slog_file_opened=1;
  return err;  
}



/** Set file where log messages are written. By default, stdout is used.
*/
int slog_set_file(char *file, int options){
  char        buffer[SLOG_MAX_SIZE];        /* buffer for log messages */
  int         err=0;                        /* error code */
    
  /* CRITICAL SECTION - start */
  pthread_mutex_lock(&mutex_log);

  if ((options & SLOG_FILE_OPT_IF_UNSET) && (slog_file!=NULL)) {
    pthread_mutex_unlock(&mutex_log);
    return err;
  }

  /* First close existing file */
  if (slog_file!=NULL) {
    slog_format(buffer,sizeof(buffer),SLOG_INFO,"Closing log file");
    fprintf(slog_file,"%s",buffer);
    fclose(slog_file);
    slog_file=NULL;
  }
  slog_file_opened=0;
  slog_file_options=options;
  if (slog_file_path!=NULL) {
    free(slog_file_path);
    slog_file_path=NULL;
  }

  if (file!=NULL) {
    if (strlen(file)!=0) {
      slog_file_path=strdup(file);
    }
  }
  
  if (!(options & SLOG_FILE_OPT_DONT_CREATE_NOW)) {
    err=slog_file_open();
  }

  /* CRITICAL SECTION - end */
  pthread_mutex_unlock(&mutex_log);

  return err;
}


/** Define callback function to be called before exiting, when a FATAL message is received.
*/
void slog_set_exit(void (*callback)(void )) {
  pthread_mutex_lock(&mutex_log); 
  slog_exit=callback;
  pthread_mutex_unlock(&mutex_log);
}
