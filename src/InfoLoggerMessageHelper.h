/*
   utilities to help using the infoLog_msg_t type (e.g. convert to text (different modes), set field, etc)
*/


#ifndef _INFOLOGGER_MESSAGE_HELPER_H
#define _INFOLOGGER_MESSAGE_HELPER_H

#include "infoLoggerMessage.h"


// macro to quickly set field in a message
// example usage:
// InfoLoggerMessageHelperSetValue(theMsg,msgHelper.ix_severity,String,severity);
// InfoLoggerMessageHelperSetValue(theMsg,msgHelper.ix_pid,Int,pid);  

#define InfoLoggerMessageHelperSetValue(VAR,IX,TYPE,VAL) VAR.values[IX].value.v##TYPE=VAL; VAR.values[IX].isUndefined=0;



// class to be instanciated once in order to use helper functions (for any number of infoLog_msg_t variables)

class InfoLoggerMessageHelper {

  public:
  InfoLoggerMessageHelper();
  ~InfoLoggerMessageHelper();
  
  enum Format {Simple, Detailed, Full, Encoded};
  int MessageToText(infoLog_msg_t *msg, char *buffer, int bufferSize, InfoLoggerMessageHelper::Format format);
  
   
  // indexes to access a given field in msg struct (protocol independent)
  int ix_severity;
  int ix_level;
  int ix_timestamp;
  int ix_hostname;
  int ix_rolename;
  int ix_pid;
  int ix_username;
  int ix_system;
  int ix_facility;
  int ix_detector;
  int ix_partition;
  int ix_run;
  int ix_errcode;
  int ix_errline;
  int ix_errsource;
  int ix_message; 

};

// _INFOLOGGER_MESSAGE_HELPER_H
#endif
