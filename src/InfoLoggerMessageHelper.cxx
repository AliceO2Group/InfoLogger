#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <stdio.h>

#include "InfoLoggerMessageHelper.h"
#include "InfoLogger/InfoLogger.hxx"

using namespace AliceO2::InfoLogger;



/* following macro is for easy filling of the index fields "ix_*" */
#define findIndex(X) (ix_##X=infoLog_msg_findField(#X))

InfoLoggerMessageHelper::InfoLoggerMessageHelper() {

  // create index for fields
  if (findIndex(severity)==-1) throw __LINE__;
  if (findIndex(level)==-1) throw __LINE__;
  if (findIndex(timestamp)==-1) throw __LINE__;
  if (findIndex(hostname)==-1) throw __LINE__;
  if (findIndex(rolename)==-1) throw __LINE__;
  if (findIndex(pid)==-1) throw __LINE__;
  if (findIndex(username)==-1) throw __LINE__;
  if (findIndex(system)==-1) throw __LINE__;
  if (findIndex(facility)==-1) throw __LINE__;
  if (findIndex(detector)==-1) throw __LINE__;
  if (findIndex(partition)==-1) throw __LINE__;
  if (findIndex(run)==-1) throw __LINE__;
  if (findIndex(errcode)==-1) throw __LINE__;
  if (findIndex(errline)==-1) throw __LINE__;
  if (findIndex(errsource)==-1) throw __LINE__;
  if (findIndex(message)==-1) throw __LINE__; 

}



InfoLoggerMessageHelper::~InfoLoggerMessageHelper() {
}



/* append a string at the given index of given buffer (of given size). Accepts printf-like formatting.
Index is updated accordingly, so that the function can be iteratively called.
Index should be initialized to zero the first time. Buffer will be truncated to bufferSize in case index exceeds buffer size.
Special index value -1 means append at end of buffer (\0).
Returns 0 on success, -1 if buffer too short (last message may be truncated) */
int  appendString(char *buffer, int bufferSize, int *index, const char *msg, ...)  __attribute__ ((format (printf, 4,5)));
int  appendString(char *buffer, int bufferSize, int *index, const char *msg, ...) {
  int l,r;

  l=*index;
  if (l==-1) {
    l=strlen(buffer);
  }
  if (l>=bufferSize) return -1;
         
  va_list args;
  va_start( args, msg );
  r=vsnprintf(&buffer[l],bufferSize-l,msg,args);
  va_end( args );
  
  if (r<0) {
    buffer[l]=0;
    return -1;
  }
  l+=r;
  if (l<=bufferSize) {
   *index=l;
   return 0;
  } else {
   *index=bufferSize;
   return -1;
  }
}

/* same as above, but append a simple separation tab */
int  appendStringSeparator(char *buffer, int bufferSize, int *index) {
  int l;

  l=*index;
  if (l==-1) {
    l=strlen(buffer);
  }
  if (l>=bufferSize-1) return -1;
  
  buffer[l]='\t';
  buffer[l+1]=0;
  *index=l+1;
  return 0;
}



/* format a message into a text buffer, according to specified format
  Returns 0 on success, or an error code. Truncated messages are not errors.
*/
int InfoLoggerMessageHelper::MessageToText(infoLog_msg_t *msg, char *buffer, int bufferSize, InfoLoggerMessageHelper::Format format) {

  if (msg==NULL) return __LINE__;
  if (buffer==NULL) return __LINE__;
  if (bufferSize<=0) return __LINE__;
  
  int ix=0;
  buffer[0]=0;
  
  int all=1;
  int status;
  
  // todo: escape %... & other printf formats ?
  
  switch (format) {

     case InfoLoggerMessageHelper::Format::Simple :
       if (!msg->values[ix_severity].isUndefined) {
         switch (msg->values[ix_severity].value.vString[0]) {
           case InfoLogger::Severity::Error :
             appendString(buffer,bufferSize,&ix,"Error - ");
             break;
           case InfoLogger::Severity::Warning :
             appendString(buffer,bufferSize,&ix,"Warning - ");
             break;
           case InfoLogger::Severity::Fatal :
             appendString(buffer,bufferSize,&ix,"Fatal - ");
             break;
           default:
             break;
         }
       }
       appendString(buffer,bufferSize,&ix,"%s",msg->values[ix_message].value.vString);
       break;
       
     case InfoLoggerMessageHelper::Format::Detailed :
       /* limit output to some fields: timestamp / severity / ... */
       all=0;
       /* continue with 'full format' processing, only subset of fields taken when all=0 */

     case InfoLoggerMessageHelper::Format::Full :            
       /* all fields (if set) */

       /* timestamp */
       if (!msg->values[ix_timestamp].isUndefined) {
         // timestamp (microsecond)
         struct timeval tv;
         double fullTimeNow=-1;
         if(gettimeofday(&tv,NULL) == -1){
           fullTimeNow = time(NULL);
         } else {
           fullTimeNow = (double)tv.tv_sec + (double)tv.tv_usec/1000000;
         }
         time_t now;
         struct tm tm_str;
         now = (time_t)fullTimeNow;
         localtime_r(&now, &tm_str);
         double fractionOfSecond=fullTimeNow-now;
         ix+=strftime(&buffer[ix],bufferSize-ix, "%Y-%m-%d %T", &tm_str);
         ix+=snprintf(&buffer[ix],bufferSize-ix,".%.3lf\t",fractionOfSecond);
         if (ix>bufferSize) {
           ix=bufferSize;
         }
       }
       
       /* severity */
       appendStringSeparator(buffer,bufferSize,&ix);
       if (!msg->values[ix_severity].isUndefined) {
         appendString(buffer,bufferSize,&ix,"%c",msg->values[ix_severity].value.vString[0]);
       }      
       
       /* other fields */
       /* if not all fields set: only facility,pid,run */
       int i;
       for (i=0;i<protocols[0].numberOfFields;i++) {
         if ((i==ix_timestamp) || (i==ix_severity) || (i==ix_message)) {continue;}
         if ((i!=ix_facility)&&(i!=ix_pid)&&(i!=ix_run)&&(!all)) {continue;}
         appendStringSeparator(buffer,bufferSize,&ix);
         if (!msg->values[i].isUndefined) {
           switch (protocols[0].fields[i].type) {
             case infoLog_msgField_def_t::ILOG_TYPE_STRING:
               appendString(buffer,bufferSize,&ix,"%s",msg->values[i].value.vString);
               break;
             case infoLog_msgField_def_t::ILOG_TYPE_INT:
               appendString(buffer,bufferSize,&ix,"%d",msg->values[i].value.vInt);
               break;
             case infoLog_msgField_def_t::ILOG_TYPE_DOUBLE:
               appendString(buffer,bufferSize,&ix,"%lf",msg->values[i].value.vDouble);
               break;
             default:
               break;
            }
         }
       }

       /* message in last position */
       appendStringSeparator(buffer,bufferSize,&ix);
       appendString(buffer,bufferSize,&ix,"%s",msg->values[ix_message].value.vString);
       break;
       
     case InfoLoggerMessageHelper::Format::Encoded :
       /* format to be shipped to reader / server */      
       /* format the message - protocol 1.3 */
       /* multiple lines messages are automatically splitted in several concatenated records (if buffer size allows) */  
       /* special result -1 means message truncated, do not take it into account */
       status=infoLog_msg_encode(msg, buffer, bufferSize, ix_message);
       if ((status)&&(status!=-1)) { return __LINE__; }
       break;

    default:
      return __LINE__;
  }  
  
  return 0;
}
