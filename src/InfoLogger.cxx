/// \file InfoLogger.cxx
/// \brief C++ wrapper for the InfoLogger logging interface.
///
/// \author Sylvain Chapeland, CERN

#include "InfoLogger/InfoLogger.hxx"
#include "InfoLogger/InfoLogger.h"

#include "infoLoggerMessage.h"
#include "InfoLoggerClient.h"


#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>
#include <sys/types.h>
#include <pwd.h>

#include "InfoLoggerMessageHelper.h"


#define InfoLoggerMagicNumber (int)0xABABAC00

//////////////////////////////////////////////////////////
/// implementation of the C API
/// (wrapper to C++ interface)
using namespace AliceO2::InfoLogger;

int infoLoggerOpen(InfoLoggerHandle *handle)
{
  if (handle == NULL) {
    return __LINE__;
  }
  *handle = NULL;
  
  InfoLogger *log=new InfoLogger();
  if (log==NULL) {return __LINE__;}
  *handle = (InfoLoggerHandle *) log;  
  return 0;
}

int infoLoggerClose(InfoLoggerHandle handle)
{
  InfoLogger *log= (InfoLogger *) handle;
  if (log == NULL) { return __LINE__; }
  delete log;
  return 0;
}

int infoLoggerLogV(InfoLoggerHandle handle, const char *message, va_list ap)
{
  InfoLogger *log= (InfoLogger *) handle;
  if (log == NULL) { return __LINE__; }

  return log->logV(InfoLogger::Severity::Info,message,ap);
}


int infoLoggerLog(InfoLoggerHandle handle, const char *message, ...)
{
  InfoLogger *log= (InfoLogger *) handle;
  if (log == NULL) { return __LINE__; }

  int err = 0;
  va_list ap;
  va_start(ap, message);
  err = log->logV(InfoLogger::Severity::Info,message, ap);
  va_end(ap);
  return err;
}


int infoLoggerLogInfo(InfoLoggerHandle handle, const char *message, ...) {
  InfoLogger *log= (InfoLogger *) handle;
  if (log == NULL) { return __LINE__; }
  int err = 0;
  va_list ap;
  va_start(ap, message);
  err = log->logV(InfoLogger::Severity::Info,message, ap);
  va_end(ap);
  return err;
}
int infoLoggerLogWarning(InfoLoggerHandle handle, const char *message, ...) {
  InfoLogger *log= (InfoLogger *) handle;
  if (log == NULL) { return __LINE__; }
  int err = 0;
  va_list ap;
  va_start(ap, message);
  err = log->logV(InfoLogger::Severity::Warning,message, ap);
  va_end(ap);
  return err;
}
int infoLoggerLogError(InfoLoggerHandle handle, const char *message, ...) {
  InfoLogger *log= (InfoLogger *) handle;
  if (log == NULL) { return __LINE__; }
  int err = 0;
  va_list ap;
  va_start(ap, message);
  err = log->logV(InfoLogger::Severity::Error,message, ap);
  va_end(ap);
  return err;
}
int infoLoggerLogFatal(InfoLoggerHandle handle, const char *message, ...) {
  InfoLogger *log= (InfoLogger *) handle;
  if (log == NULL) { return __LINE__; }
  int err = 0;
  va_list ap;
  va_start(ap, message);
  err = log->logV(InfoLogger::Severity::Fatal,message, ap);
  va_end(ap);
  return err;
}
int infoLoggerLogDebug(InfoLoggerHandle handle, const char *message, ...) {
  InfoLogger *log= (InfoLogger *) handle;
  if (log == NULL) { return __LINE__; }
  int err = 0;
  va_list ap;
  va_start(ap, message);
  err = log->logV(InfoLogger::Severity::Debug,message, ap);
  va_end(ap);
  return err;
}

/////////////////////////////////////////////////////////




namespace AliceO2 {
namespace InfoLogger {

class ProcessInfo {
  public:
  
  int processId;
  std::string hostName;
  std::string userName;

/*  std::string roleName;
  std::string system;
  std::string facility;
  std::string detector;
  std::string partition;
  int runNumber;
*/

  ProcessInfo();
  ~ProcessInfo();
  void refresh();
};


ProcessInfo::ProcessInfo() {
  refresh();
}

ProcessInfo::~ProcessInfo() {
}

void ProcessInfo::refresh() {
  processId=-1;
  processId=(int)getpid();

  hostName.clear();
  char hostNameTmp[256]="";
  if (!gethostname(hostNameTmp,sizeof(hostNameTmp))) {
    char * dotptr;
    dotptr=strchr(hostNameTmp,'.');
    if (dotptr!=NULL) *dotptr=0;
    hostName=hostNameTmp;
  }

  userName.clear();
  struct passwd *passent;
  passent = getpwuid(getuid());
  if(passent != NULL){
    userName=passent->pw_name;
  }
 
  // for the other fields, try to automatically extract from environment, e.g. O2_FIELDNAME
}





// private class to isolate internal data from external interface
class InfoLogger::Impl {
  public:
  Impl() {
    // initiate internal members
    magicTag = InfoLoggerMagicNumber;
    numberOfMessages = 0;
    currentStreamMessage.clear();
    currentStreamSeverity=Severity::Info;
    
    if (infoLog_proto_init()) {
      throw __LINE__;
    }
    refreshDefaultMsg();
    currentMode=OutputMode::stdout;
    client=nullptr;
    if (currentMode==OutputMode::infoLoggerD) {
      client=new InfoLoggerClient;
    }
    // todo
    // switch mode based on configuration / environment
    // connect to client only on first message (or try again after timeout)
  }
  ~Impl() {
    magicTag = 0;
    if (client!=nullptr) {
      delete client;
    }
  }

  int pushMessage(InfoLogger::Severity severity, const char *msg); // todo: add extra "configurable" fields, e.g. line, etc

  friend class InfoLogger;  //< give access to this data from InfoLogger class

  enum OutputMode {stdout, file, infoLoggerD};  // available options for output
  OutputMode currentMode; // current option for output
  
  

  /// Log a message, with a list of arguments of type va_list.
  /// \param message  NUL-terminated string message to push to the log system. It uses the same format as specified for printf(), and the function accepts additionnal formatting parameters.
  /// \param ap       Variable list of arguments (c.f. vprintf)
  /// \return         0 on success, an error code otherwise.
  int logV(InfoLogger::Severity severity, const char *message, va_list ap) __attribute__((format(printf, 3, 0)));
  
  
  protected:
  int magicTag;                     //< A static tag used for handle validity cross-check
  int numberOfMessages;             //< number of messages received by this object
  std::string currentStreamMessage; //< temporary variable to store message when concatenating << operations, until "endm" is received
  Severity currentStreamSeverity;  //< temporary variable to store message severity when concatenating << operations, until "endm" is received
    
  ProcessInfo processInfo;
  InfoLoggerMessageHelper msgHelper; 
  void refreshDefaultMsg();  
  infoLog_msg_t defaultMsg;         //< default log message (in particular, to complete optionnal fields)
  
  InfoLoggerClient *client; //< entity to communicate with local infoLoggerD
  SimpleLog stdLog;  //< object to output messages to stdout/file
};



void InfoLogger::Impl::refreshDefaultMsg() {
  processInfo.refresh();

  int i;
  for (i=0;i<protocols[0].numberOfFields;i++) {
    defaultMsg.values[i].isUndefined=1;
    defaultMsg.values[i].length=0;
    bzero(&defaultMsg.values[i].value,sizeof(defaultMsg.values[i].value));
  }

  defaultMsg.protocol=&protocols[0];
  defaultMsg.next=NULL;
  defaultMsg.data=NULL;

  static char str_severity[2]={InfoLogger::Severity::Info,0};

  InfoLoggerMessageHelperSetValue(defaultMsg,msgHelper.ix_severity,String,str_severity);
  InfoLoggerMessageHelperSetValue(defaultMsg,msgHelper.ix_hostname,String,processInfo.hostName.c_str());
  InfoLoggerMessageHelperSetValue(defaultMsg,msgHelper.ix_username,String,processInfo.userName.c_str());  
  InfoLoggerMessageHelperSetValue(defaultMsg,msgHelper.ix_pid,Int,processInfo.processId);  
}

#define LOG_MAX_SIZE 1024

int InfoLogger::Impl::pushMessage(InfoLogger::Severity severity, const char *messageBody) {
  infoLog_msg_t msg=defaultMsg;

  struct timeval tv;
  if(gettimeofday(&tv,NULL) == 0){
    double now = (double)tv.tv_sec + (double)tv.tv_usec/1000000;
    InfoLoggerMessageHelperSetValue(msg,msgHelper.ix_timestamp,Double,now);
  }
  
  if (messageBody!=NULL) {
    InfoLoggerMessageHelperSetValue(msg,msgHelper.ix_message,String,messageBody);
  }
  
  char strSeverity[2]={(char)(severity),0};
  InfoLoggerMessageHelperSetValue(msg,msgHelper.ix_severity,String,strSeverity);
   
//  printf("%s\n",buffer);

  if (client!=nullptr) {
    char buffer[LOG_MAX_SIZE];
    msgHelper.MessageToText(&msg,buffer,sizeof(buffer),InfoLoggerMessageHelper::Format::Encoded);
    client->send(buffer,strlen(buffer));
    
    // todo
    // on error, close connection / use stdout / buffer messages in memory ?
  }
  
  if (currentMode==OutputMode::stdout) {
      char buffer[LOG_MAX_SIZE];
      msgHelper.MessageToText(&msg,buffer,sizeof(buffer),InfoLoggerMessageHelper::Format::Simple);

    switch(severity) {
      case(InfoLogger::Severity::Fatal):
      case(InfoLogger::Severity::Error):
        stdLog.error("\033[1;31m%s\033[0m",buffer);
        break;
      case(InfoLogger::Severity::Warning):
        stdLog.warning("\033[1;33m%s\033[0m",buffer);
        break;
      case(InfoLogger::Severity::Info):
      case(InfoLogger::Severity::Debug):
      default:
        stdLog.info("%s",buffer);
        break;
    }
  }
  
  return 0;
}

int InfoLogger::Impl::logV(InfoLogger::Severity severity, const char *message, va_list ap)
{
  char buffer[1024] = "";
  size_t len = 0;
/*
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
  len = strftime(buffer, sizeof(buffer), "%Y-%m-%d %T", &tm_str);
  len+=snprintf(&buffer[len],sizeof(buffer)-len,".%.3lf\t",fractionOfSecond);
  if (len>sizeof(buffer)) {
    len=sizeof(buffer);
  }
*/
  vsnprintf(&buffer[len], sizeof(buffer) - len, message, ap);

  //printf("%s\n", buffer);
  
  pushMessage(severity,buffer);
  
  numberOfMessages++;
  return 0;
}






InfoLogger::InfoLogger()
{
  pImpl=std::make_unique<InfoLogger::Impl>();
  if (pImpl==NULL) { throw __LINE__; }
}


InfoLogger::~InfoLogger()
{
  // pImpl is automatically destroyed
}

int InfoLogger::log(const char *message, ...)
{
  // forward variable list of arguments to logV method
  int err;
  va_list ap;
  va_start(ap, message);
  err = logV(InfoLogger::Severity::Info,message, ap);
  va_end(ap);
  return err;
}


int InfoLogger::log(Severity severity, const char *message, ...)
{
  // forward variable list of arguments to logV method
  int err;
  va_list ap;
  va_start(ap, message);
  err = logV(severity,message, ap);
  va_end(ap);
  return err;
}


int InfoLogger::logV(Severity severity, const char *message, va_list ap) {
  if (pImpl->magicTag != InfoLoggerMagicNumber) { return __LINE__; }
  return pImpl->logV(severity, message, ap);
}


InfoLogger &InfoLogger::operator<<(const std::string message)
{
  // store data in internal variable
  pImpl->currentStreamMessage.append(message);
  return *this;
}

InfoLogger &InfoLogger::operator<<(InfoLogger::StreamOps op)
{
  // process special commands received by << operator

  // end of message: flush current buffer in a single message
  if (op == endm) {
    log(pImpl->currentStreamSeverity,pImpl->currentStreamMessage.c_str());
    pImpl->currentStreamMessage.clear();
    pImpl->currentStreamSeverity=Severity::Info;
  }
  return *this;
}

InfoLogger &InfoLogger::operator<<(const InfoLogger::Severity severity)
{
  pImpl->currentStreamSeverity=severity;
  return *this;
}



// end of namespace
}
}
