/** Simple logging facility
  *
  * This interface defines functions to log messages.
  * Each message has a log level.
  * Output format is standardized (includes timestamp and message severity).
  * Messages are printed to stdout, or a file.
  * Debugging messages can be discarded.
  * Fatal messages cause exit (after custom callback if defined).
  *
  *
  * @file       simplelog.h
  * @author     sylvain.chapeland@cern.ch
  *
  * History:
  *   - 28/10/2004 File created.
  *   - 16/12/2004 slog_set_file() call changed (added options), but backward compatible
*/

/* Avoid multiple includes */
#ifndef simplelog_h
#define simplelog_h

#ifdef __cplusplus
extern "C" {
#endif


/* Message severity definitions */
typedef enum {
  SLOG_FATAL,         /**< Log severity of a fatal error message  */
  SLOG_ERROR,         /**< Log severity of an error message       */
  SLOG_WARNING,       /**< Log severity of a warning message      */
  SLOG_INFO,          /**< Log severity of an information message */
  SLOG_DEBUG,         /**< Log severity of a debugging message    */
} slog_Severity;


/** Log a message.
  * @param    severity     Message severity. FATAL messages trigger exit, DEBUG messages can be filtered out.
  * @param    message      Null-terminated message, printf-like formatted (%%s, %%d ... possible).
  * @param    ...          Optionnal arguments used in message formatting.
*/
void slog(slog_Severity severity, char *message, ...)  __attribute__ ((format (printf, 2,3))); /* printf-like format parameters */


/** Turns on logging of debugging messages. */
void slog_debug_on();

/** Turns off logging of debugging messages. This is the default mode. */
void slog_debug_off();


/** Set file where log messages are written. By default, stdout is used.
  *
  * @param    file    Null-terminated path to the file.
  * @param    options Set options (See below).
  * @return           0 if success, -1 on error;
  *
  * If path is NULL or the file can not be accessed, stdout is used.
  * The file is kept open until a slog_set_file(NULL,0) is called. (and some memory is allocated until then)
  * If the given file can not be opened, stdout is used,and -1 is returned.
*/
#define SLOG_FILE_OPT_NONE                 0    // default value (file appended, opened right now)
#define SLOG_FILE_OPT_CLEAR_IF_EXISTS      1    // file is truncated if exists
#define SLOG_FILE_OPT_DONT_CREATE_NOW      2    // file is created only when first message arrives
#define SLOG_FILE_OPT_IF_UNSET             4    // change log file only if unset yet
int slog_set_file(char *file, int options);


/** Define callback function to be called before exiting, when a FATAL message is received.
  * By default, no function is called before exit().
  *
  * @param    callback    Address of the callback function.
*/
void slog_set_exit(void (*callback)(void ));


void setSimpleLog(void *ptr);


#ifdef __cplusplus
}
#endif

/* END #ifndef simplelog_h */
#endif
