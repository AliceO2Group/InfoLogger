// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

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
#include <ctype.h>
#include <thread>
#include <memory>
#include <functional>

#include "InfoLoggerMessageHelper.h"
#include "Common/LineBuffer.h"

#define InfoLoggerMagicNumber (int)0xABABAC00

//////////////////////////////////////////////////////////
/// implementation of the C API
/// (wrapper to C++ interface)
/// No c++ exceptions are thrown by these functions.

using namespace AliceO2::InfoLogger;

int infoLoggerOpen(InfoLoggerHandle* handle)
{
  if (handle == NULL) {
    return __LINE__;
  }
  *handle = NULL;

  InfoLogger* log = NULL;
  try {
    log = new InfoLogger();
  } catch (...) {
  }
  if (log == NULL) {
    return __LINE__;
  }
  *handle = (InfoLoggerHandle*)log;
  return 0;
}

int infoLoggerClose(InfoLoggerHandle handle)
{
  InfoLogger* log = (InfoLogger*)handle;
  if (log == NULL) {
    return __LINE__;
  }
  delete log;
  return 0;
}

int infoLoggerLogV(InfoLoggerHandle handle, const char* message, va_list ap)
{
  InfoLogger* log = (InfoLogger*)handle;
  if (log == NULL) {
    return __LINE__;
  }

  return log->logV(InfoLogger::Severity::Info, message, ap);
}

int infoLoggerLog(InfoLoggerHandle handle, const char* message, ...)
{
  InfoLogger* log = (InfoLogger*)handle;
  if (log == NULL) {
    return __LINE__;
  }

  int err = 0;
  va_list ap;
  va_start(ap, message);
  err = log->logV(InfoLogger::Severity::Info, message, ap);
  va_end(ap);
  return err;
}

int infoLoggerLogInfo(InfoLoggerHandle handle, const char* message, ...)
{
  InfoLogger* log = (InfoLogger*)handle;
  if (log == NULL) {
    return __LINE__;
  }
  int err = 0;
  va_list ap;
  va_start(ap, message);
  err = log->logV(InfoLogger::Severity::Info, message, ap);
  va_end(ap);
  return err;
}
int infoLoggerLogWarning(InfoLoggerHandle handle, const char* message, ...)
{
  InfoLogger* log = (InfoLogger*)handle;
  if (log == NULL) {
    return __LINE__;
  }
  int err = 0;
  va_list ap;
  va_start(ap, message);
  err = log->logV(InfoLogger::Severity::Warning, message, ap);
  va_end(ap);
  return err;
}
int infoLoggerLogError(InfoLoggerHandle handle, const char* message, ...)
{
  InfoLogger* log = (InfoLogger*)handle;
  if (log == NULL) {
    return __LINE__;
  }
  int err = 0;
  va_list ap;
  va_start(ap, message);
  err = log->logV(InfoLogger::Severity::Error, message, ap);
  va_end(ap);
  return err;
}
int infoLoggerLogFatal(InfoLoggerHandle handle, const char* message, ...)
{
  InfoLogger* log = (InfoLogger*)handle;
  if (log == NULL) {
    return __LINE__;
  }
  int err = 0;
  va_list ap;
  va_start(ap, message);
  err = log->logV(InfoLogger::Severity::Fatal, message, ap);
  va_end(ap);
  return err;
}
int infoLoggerLogDebug(InfoLoggerHandle handle, const char* message, ...)
{
  InfoLogger* log = (InfoLogger*)handle;
  if (log == NULL) {
    return __LINE__;
  }
  int err = 0;
  va_list ap;
  va_start(ap, message);
  err = log->logV(InfoLogger::Severity::Debug, message, ap);
  va_end(ap);
  return err;
}

/////////////////////////////////////////////////////////

namespace AliceO2
{
namespace InfoLogger
{

int infoLoggerDWarningDone = 0;

// private class to isolate internal data from external interface
class InfoLogger::Impl
{
 public:
  Impl()
  {
    // initiate internal members
    magicTag = InfoLoggerMagicNumber;
    numberOfMessages = 0;
    currentStreamMessage.clear();
    currentStreamOptions = undefinedMessageOption;

    if (infoLog_proto_init()) {
      throw __LINE__;
    }
    refreshDefaultMsg();
    currentMode = OutputMode::infoLoggerD;

    const char* confEnv = getenv("INFOLOGGER_MODE");
    if (confEnv != NULL) {
      if (!strcmp(confEnv, "stdout")) {
        currentMode = OutputMode::stdout;
      } else if (!strcmp(confEnv, "infoLoggerD")) {
        currentMode = OutputMode::infoLoggerD;
      } else if (!strncmp(confEnv, "file", 4)) {
        currentMode = OutputMode::file;
        const char* logFile = "./log.txt";
        if (confEnv[4] == ':') {
          logFile = &confEnv[5];
        }
        printf("Logging to file %s\n", logFile);
        stdLog.setLogFile(logFile);
      } else if (!strcmp(confEnv, "raw")) {
        currentMode = OutputMode::raw;
      } else if (!strcmp(confEnv, "none")) {
        currentMode = OutputMode::none; // useful for benchmarks
      }
    }
    client = nullptr;
    if (currentMode == OutputMode::infoLoggerD) {
      client = new InfoLoggerClient;
      if ((client == nullptr) || (!client->isOk())) {
        // fallback to stdout if infoLoggerD not available
        if (!infoLoggerDWarningDone) {
          infoLoggerDWarningDone = 1;
          //fprintf(stderr,"infoLoggerD not available, falling back to stdout logging\n");
        }
        currentMode = OutputMode::stdout;
      }
    }
    // todo
    // switch mode based on configuration / environment
    // connect to client only on first message (or try again after timeout)
    // - mode: can be OR of several modes?
  }
  ~Impl()
  {
    magicTag = 0;
    if (client != nullptr) {
      delete client;
    }
  }

  int pushMessage(InfoLogger::Severity severity, const char* msg); // todo: add extra "configurable" fields, e.g. line, etc

  int pushMessage(const InfoLoggerMessageOption& options, const InfoLoggerContext& context, const char* msg);

  friend class InfoLogger; //< give access to this data from InfoLogger class

  // available options for output
  // stdout: write all messages to stdout (human-readable)
  // file: write messages to a file (human-readable)
  // infoLoggerD: write messages to infoLoggerD
  // raw: write messages to stdout (encoded as for infoLoggerD)
  // none: output disabled
  enum OutputMode { stdout,
                    file,
                    infoLoggerD,
                    raw,
                    none };

  OutputMode currentMode; // current option for output

  /// Log a message, with a list of arguments of type va_list.
  /// \param message  NUL-terminated string message to push to the log system. It uses the same format as specified for printf(), and the function accepts additionnal formatting parameters.
  /// \param ap       Variable list of arguments (c.f. vprintf)
  /// \return         0 on success, an error code otherwise (but never throw exceptions).
  int logV(InfoLogger::Severity severity, const char* message, va_list ap) __attribute__((format(printf, 3, 0)));

  /// extended log function, with all extra fields, including a specific context
  /// \return         0 on success, an error code otherwise (but never throw exceptions)..
  int logV(const InfoLoggerMessageOption& options, const InfoLoggerContext& context, const char* message, va_list ap) __attribute__((format(printf, 4, 0)));

  // main loop of collecting thread, reading incoming messages from a pipe and redirecting to infoLogger
  void redirectThreadLoop();

 protected:
  int magicTag;                                 //< A static tag used for handle validity cross-check
  int numberOfMessages;                         //< number of messages received by this object
  std::string currentStreamMessage;             //< temporary variable to store message when concatenating << operations, until "endm" is received
  InfoLoggerMessageOption currentStreamOptions; //< temporary variable to store message options when concatenating << operations, until "endm" is received

  InfoLoggerContext currentContext;

  InfoLoggerMessageHelper msgHelper;
  void refreshDefaultMsg();
  infoLog_msg_t defaultMsg; //< default log message (in particular, to complete optionnal fields)

  InfoLoggerClient* client; //< entity to communicate with local infoLoggerD
  SimpleLog stdLog;         //< object to output messages to stdout/file

  bool isRedirecting = false;                  // state of stdout/stderr redirection
  int fdStdout = -1;                           // initial stdout file descriptor, if redirection active
  int fdStderr = -1;                           // initial stderr file descriptor, if redirection active
  int pipeStdout[2];                           // a pipe to redirect stdout to collecting thread
  int pipeStderr[2];                           // a pipe to redirect stderr to collecting thread
  std::unique_ptr<std::thread> redirectThread; // the thread handling the redirection
  bool redirectThreadShutdown;                 // flag to ask the thread to stop
};

void InfoLogger::Impl::refreshDefaultMsg()
{
  int i;
  for (i = 0; i < protocols[0].numberOfFields; i++) {
    defaultMsg.values[i].isUndefined = 1;
    defaultMsg.values[i].length = 0;
    bzero(&defaultMsg.values[i].value, sizeof(defaultMsg.values[i].value));
  }

  defaultMsg.protocol = &protocols[0];
  defaultMsg.next = NULL;
  defaultMsg.data = NULL;

  static char str_severity[2] = { InfoLogger::Severity::Info, 0 };
  InfoLoggerMessageHelperSetValue(defaultMsg, msgHelper.ix_severity, String, str_severity);

  // apply non-empty strings from context

  if (currentContext.facility.length() > 0) {
    InfoLoggerMessageHelperSetValue(defaultMsg, msgHelper.ix_facility, String, currentContext.facility.c_str());
  }
  if (currentContext.role.length() > 0) {
    InfoLoggerMessageHelperSetValue(defaultMsg, msgHelper.ix_rolename, String, currentContext.role.c_str());
  }
  if (currentContext.system.length() > 0) {
    InfoLoggerMessageHelperSetValue(defaultMsg, msgHelper.ix_system, String, currentContext.system.c_str());
  }
  if (currentContext.detector.length() > 0) {
    InfoLoggerMessageHelperSetValue(defaultMsg, msgHelper.ix_detector, String, currentContext.detector.c_str());
  }
  if (currentContext.partition.length() > 0) {
    InfoLoggerMessageHelperSetValue(defaultMsg, msgHelper.ix_partition, String, currentContext.partition.c_str());
  }
  if (currentContext.run != -1) {
    InfoLoggerMessageHelperSetValue(defaultMsg, msgHelper.ix_run, Int, currentContext.run);
  }

  if (currentContext.processId != -1) {
    InfoLoggerMessageHelperSetValue(defaultMsg, msgHelper.ix_pid, Int, currentContext.processId);
  }
  if (currentContext.hostName.length() > 0) {
    InfoLoggerMessageHelperSetValue(defaultMsg, msgHelper.ix_hostname, String, currentContext.hostName.c_str());
  }
  if (currentContext.userName.length() > 0) {
    InfoLoggerMessageHelperSetValue(defaultMsg, msgHelper.ix_username, String, currentContext.userName.c_str());
  }
}

#define LOG_MAX_SIZE 1024

int InfoLogger::Impl::pushMessage(InfoLogger::Severity severity, const char* messageBody)
{
  InfoLoggerMessageOption options = undefinedMessageOption;
  options.severity = severity;

  return pushMessage(options, currentContext, messageBody);
}

int InfoLogger::Impl::pushMessage(const InfoLoggerMessageOption& options, const InfoLoggerContext& context, const char* messageBody)
{
  infoLog_msg_t msg = defaultMsg;

  struct timeval tv;
  if (gettimeofday(&tv, NULL) == 0) {
    double now = (double)tv.tv_sec + (double)tv.tv_usec / 1000000;
    InfoLoggerMessageHelperSetValue(msg, msgHelper.ix_timestamp, Double, now);
  }

  if (messageBody != NULL) {
    InfoLoggerMessageHelperSetValue(msg, msgHelper.ix_message, String, messageBody);
  }

  // update message from options
  // todo: possibly add checks on parameters validity

  // convert severity enum to a string
  char strSeverity[2] = { (char)(options.severity), 0 }; // if used, this variable shall live until msg variable not used any more
  if (options.severity != undefinedMessageOption.severity) {
    InfoLoggerMessageHelperSetValue(msg, msgHelper.ix_severity, String, strSeverity);
  }
  if (options.level != undefinedMessageOption.level) {
    InfoLoggerMessageHelperSetValue(msg, msgHelper.ix_level, Int, options.level);
  }
  if (options.errorCode != undefinedMessageOption.errorCode) {
    InfoLoggerMessageHelperSetValue(msg, msgHelper.ix_errcode, Int, options.errorCode);
  }
  if (options.sourceFile != undefinedMessageOption.sourceFile) {
    InfoLoggerMessageHelperSetValue(msg, msgHelper.ix_errsource, String, options.sourceFile);
  }
  if (options.sourceLine != undefinedMessageOption.sourceLine) {
    InfoLoggerMessageHelperSetValue(msg, msgHelper.ix_errline, Int, options.sourceLine);
  }

  // update message from context (set only non-empty fields - others left to what was set by default context)
  if (context.facility.length() > 0) {
    InfoLoggerMessageHelperSetValue(msg, msgHelper.ix_facility, String, context.facility.c_str());
  }
  if (context.role.length() > 0) {
    InfoLoggerMessageHelperSetValue(msg, msgHelper.ix_rolename, String, context.role.c_str());
  }
  if (context.system.length() > 0) {
    InfoLoggerMessageHelperSetValue(msg, msgHelper.ix_system, String, context.system.c_str());
  }
  if (context.detector.length() > 0) {
    InfoLoggerMessageHelperSetValue(msg, msgHelper.ix_detector, String, context.detector.c_str());
  }
  if (context.partition.length() > 0) {
    InfoLoggerMessageHelperSetValue(msg, msgHelper.ix_partition, String, context.partition.c_str());
  }
  if (context.run != -1) {
    InfoLoggerMessageHelperSetValue(msg, msgHelper.ix_run, Int, context.run);
  }
  if (context.processId != -1) {
    InfoLoggerMessageHelperSetValue(msg, msgHelper.ix_pid, Int, context.processId);
  }
  if (context.hostName.length() > 0) {
    InfoLoggerMessageHelperSetValue(msg, msgHelper.ix_hostname, String, context.hostName.c_str());
  }
  if (context.userName.length() > 0) {
    InfoLoggerMessageHelperSetValue(msg, msgHelper.ix_username, String, context.userName.c_str());
  }

  //  printf("%s\n",buffer);

  if (client != nullptr) {
    char buffer[LOG_MAX_SIZE];
    msgHelper.MessageToText(&msg, buffer, sizeof(buffer), InfoLoggerMessageHelper::Format::Encoded);
    client->send(buffer, strlen(buffer));

    // todo
    // on error, close connection / use stdout / buffer messages in memory ?
  }

  if ((currentMode == OutputMode::stdout) || (currentMode == OutputMode::file)) {
    char buffer[LOG_MAX_SIZE];
    msgHelper.MessageToText(&msg, buffer, sizeof(buffer), InfoLoggerMessageHelper::Format::Simple);

    switch (options.severity) {
      case (InfoLogger::Severity::Fatal):
      case (InfoLogger::Severity::Error):
        stdLog.error("\033[1;31m%s\033[0m", buffer);
        break;
      case (InfoLogger::Severity::Warning):
        stdLog.warning("\033[1;33m%s\033[0m", buffer);
        break;
      case (InfoLogger::Severity::Info):
      case (InfoLogger::Severity::Debug):
      default:
        stdLog.info("%s", buffer);
        break;
    }
  }

  // raw output: infoLogger protocol to stdout
  if (currentMode == OutputMode::raw) {
    char buffer[LOG_MAX_SIZE];
    msgHelper.MessageToText(&msg, buffer, sizeof(buffer), InfoLoggerMessageHelper::Format::Encoded);
    puts(buffer);
  }

  return 0;
}

int InfoLogger::Impl::logV(InfoLogger::Severity severity, const char* message, va_list ap)
{

  // make sure this function never throw c++ exceptions, as logV is called from the C API wrapper
  try {
    char buffer[1024] = "";
    vsnprintf(buffer, sizeof(buffer), message, ap);
    pushMessage(severity, buffer);
    numberOfMessages++;
  } catch (...) {
    return __LINE__;
  }

  return 0;
}

int InfoLogger::Impl::logV(const InfoLoggerMessageOption& options, const InfoLoggerContext& context, const char* message, va_list ap)
{

  // make sure this function never throw c++ exceptions, as logV is called from the C API wrapper
  try {
    char buffer[1024] = "";
    vsnprintf(buffer, sizeof(buffer), message, ap);
    pushMessage(options, context, buffer);
    numberOfMessages++;
  } catch (...) {
    return __LINE__;
  }

  return 0;
}

InfoLogger::InfoLogger()
{
  mPimpl = std::make_unique<InfoLogger::Impl>();
  if (mPimpl == NULL) {
    throw __LINE__;
  }
}

InfoLogger::~InfoLogger()
{
  // mPimpl is automatically destroyed
}

int InfoLogger::logInfo(const std::string& message)
{
  return log(Severity::Info, "%s", message.c_str());
}

int InfoLogger::logError(const std::string& message)
{
  return log(Severity::Error, "%s", message.c_str());
}

int InfoLogger::log(const char* message, ...)
{
  // forward variable list of arguments to logV method
  int err;
  va_list ap;
  va_start(ap, message);
  err = logV(InfoLogger::Severity::Info, message, ap);
  va_end(ap);
  return err;
}

int InfoLogger::log(Severity severity, const char* message, ...)
{
  // forward variable list of arguments to logV method
  int err;
  va_list ap;
  va_start(ap, message);
  err = logV(severity, message, ap);
  va_end(ap);
  return err;
}

int InfoLogger::logV(Severity severity, const char* message, va_list ap)
{
  if (mPimpl->magicTag != InfoLoggerMagicNumber) {
    return __LINE__;
  }
  return mPimpl->logV(severity, message, ap);
}

/*
int InfoLogger::log(Severity severity, int level, int errorCode, const char * sourceFile, int sourceLine, const char *message, ...)
{
  // forward variable list of arguments to logV method
  int err;
  va_list ap;
  va_start(ap, message);
  err = logV(severity,message, ap);
  va_end(ap);
  return err;
}

int InfoLogger::log(Severity severity, int level, int errorCode, const char * sourceFile, int sourceLine, const InfoLoggerContext& context, const char *message, ...)
{
  // forward variable list of arguments to logV method
  int err;
  va_list ap;
  va_start(ap, message);
  err = logV(severity,message, ap);
  va_end(ap);
  return err;
}

int InfoLogger::logV(Severity severity, int level, int errorCode, const char * sourceFile, int sourceLine, const InfoLoggerContext& context,  const char *message, va_list ap)
{
  if (mPimpl->magicTag != InfoLoggerMagicNumber) { return __LINE__; }
  return mPimpl->logV(severity, message, ap);
}
*/

int InfoLogger::setContext(const InfoLoggerContext& context)
{
  if (mPimpl->magicTag != InfoLoggerMagicNumber) {
    return __LINE__;
  }
  mPimpl->currentContext = context;
  mPimpl->refreshDefaultMsg();
  return 0;
}

int InfoLogger::unsetContext()
{
  if (mPimpl->magicTag != InfoLoggerMagicNumber) {
    return __LINE__;
  }
  mPimpl->currentContext.reset();
  mPimpl->refreshDefaultMsg();
  return 0;
}

int InfoLogger::log(const InfoLoggerMessageOption& options, const InfoLoggerContext& context, const char* message, ...)
{
  if (mPimpl->magicTag != InfoLoggerMagicNumber) {
    return __LINE__;
  }

  // forward variable list of arguments to logV method
  int err;
  va_list ap;
  va_start(ap, message);
  err = mPimpl->logV(options, context, message, ap);
  va_end(ap);
  return err;
}

int InfoLogger::log(const InfoLoggerMessageOption& options, const char* message, ...)
{
  if (mPimpl->magicTag != InfoLoggerMagicNumber) {
    return __LINE__;
  }

  // forward variable list of arguments to logV method
  int err;
  va_list ap;
  va_start(ap, message);
  err = mPimpl->logV(options, mPimpl->currentContext, message, ap);
  va_end(ap);
  return err;
}

InfoLogger& InfoLogger::operator<<(const std::string& message)
{
  // store data in internal variable
  mPimpl->currentStreamMessage.append(message);
  return *this;
}

InfoLogger& InfoLogger::operator<<(InfoLogger::StreamOps op)
{
  // process special commands received by << operator

  // end of message: flush current buffer in a single message
  if (op == endm) {
    log(mPimpl->currentStreamOptions, "%s", mPimpl->currentStreamMessage.c_str());
    mPimpl->currentStreamMessage.clear();
    mPimpl->currentStreamOptions = undefinedMessageOption;
  }
  return *this;
}

InfoLogger& InfoLogger::operator<<(const InfoLogger::Severity severity)
{
  mPimpl->currentStreamOptions.severity = severity;
  return *this;
}

InfoLogger& InfoLogger::operator<<(const InfoLogger::InfoLoggerMessageOption options)
{
  mPimpl->currentStreamOptions = options;
  return *this;
}

InfoLogger::Severity InfoLogger::getSeverityFromString(const char* txt)
{
  // permissive implementation
  switch (tolower(txt[0])) {
    case 'i':
      return InfoLogger::Severity::Info;
    case 'e':
      return InfoLogger::Severity::Error;
    case 'f':
      return InfoLogger::Severity::Fatal;
    case 'w':
      return InfoLogger::Severity::Warning;
    case 'd':
      return InfoLogger::Severity::Debug;
    default:
      return InfoLogger::Severity::Undefined;
  }

  /*
  // strict implementation  
  if (!strcmp(txt,"Info")) {
    return InfoLogger::Severity::Info;
  } else if (!strcmp(txt,"Error")) {
    return InfoLogger::Severity::Error;
  } else if (!strcmp(txt,"Fatal")) {
    return InfoLogger::Severity::Fatal;
  } else if (!strcmp(txt,"Warning")) {
    return InfoLogger::Severity::Warning;
  } else if (!strcmp(txt,"Debug")) {
    return InfoLogger::Severity::Debug;
  }
*/
  return InfoLogger::Severity::Undefined;
}

int InfoLogger::setMessageOption(const char* fieldName, const char* fieldValue, InfoLoggerMessageOption& output)
{
  if (fieldName == NULL) {
    return -1;
  }
  if (fieldValue == NULL) {
    return -1;
  }

  // in case of blank string, use default value
  bool useDefault = false;
  if (strlen(fieldValue) == 0) {
    useDefault = true;
  }

  if (!strcmp(fieldName, "Severity")) {
    if (useDefault) {
      output.severity = undefinedMessageOption.severity;
    } else {
      output.severity = getSeverityFromString(fieldValue);
    }
  } else if (!strcmp(fieldName, "Level")) {
    if (useDefault) {
      output.level = undefinedMessageOption.level;
    } else {
      output.level = atoi(fieldValue);
    }
  } else if (!strcmp(fieldName, "ErrorCode")) {
    if (useDefault) {
      output.errorCode = undefinedMessageOption.errorCode;
    } else {
      output.errorCode = atoi(fieldValue);
    }
  } else if (!strcmp(fieldName, "SourceFile")) {
    if (useDefault) {
      output.sourceFile = undefinedMessageOption.sourceFile;
    } else {
      output.sourceFile = fieldValue;
    }
  } else if (!strcmp(fieldName, "SourceLine")) {
    if (useDefault) {
      output.sourceLine = undefinedMessageOption.sourceLine;
    } else {
      output.sourceLine = atoi(fieldValue);
    }
  } else {
    return -1;
  }
  return 0;
}

// required with some compilers to avoid linking errors
constexpr InfoLogger::InfoLoggerMessageOption InfoLogger::undefinedMessageOption;

void InfoLogger::Impl::redirectThreadLoop()
{
  LineBuffer lbStdout; // buffer for input lines
  LineBuffer lbStderr; // buffer for input lines
  std::string msg;
  bool isActive = 0;
  while (!redirectThreadShutdown) {
    isActive = 0;
    lbStdout.appendFromFileDescriptor(pipeStdout[0], 0);
    for (;;) {
      if (lbStdout.getNextLine(msg)) {
        // no line left
        break;
      }
      isActive = 1;
      pushMessage(InfoLogger::Severity::Info, msg.c_str());
    }
    lbStderr.appendFromFileDescriptor(pipeStderr[0], 0);
    for (;;) {
      if (lbStderr.getNextLine(msg)) {
        // no line left
        break;
      }
      isActive = 1;
      pushMessage(InfoLogger::Severity::Error, msg.c_str());
    }

    if (!isActive) {
      usleep(50000);
    }
  }
  //  fprintf(fp,"thread stopped\n");
  //  fclose(fp);
}

int InfoLogger::setStandardRedirection(bool state)
{
  if (state == mPimpl->isRedirecting) {
    // we are already in the requested redirection state
    return -1;
  }

  if (state) {
    // turn ON redirection

    // create a pipe
    if (pipe(mPimpl->pipeStdout) != 0) {
      return -1;
    }
    if (pipe(mPimpl->pipeStderr) != 0) {
      return -1;
    }

    // save current stdout/stderr
    mPimpl->fdStdout = dup(STDOUT_FILENO);
    dup2(mPimpl->pipeStdout[1], STDOUT_FILENO);
    mPimpl->fdStderr = dup(STDERR_FILENO);
    dup2(mPimpl->pipeStderr[1], STDERR_FILENO);

    mPimpl->stdLog.setFileDescriptors(mPimpl->fdStdout, mPimpl->fdStderr);

    // create collecting thread
    mPimpl->redirectThreadShutdown = 0;
    std::function<void(void)> l = std::bind(&InfoLogger::Impl::redirectThreadLoop, mPimpl.get());
    mPimpl->redirectThread = std::make_unique<std::thread>(l);

  } else {
    // turn OFF redirection

    // stop collecting thread
    mPimpl->redirectThreadShutdown = 1;
    mPimpl->redirectThread->join();
    mPimpl->redirectThread = nullptr;

    mPimpl->stdLog.setFileDescriptors(STDOUT_FILENO, STDERR_FILENO);

    dup2(mPimpl->fdStdout, STDOUT_FILENO);
    close(mPimpl->fdStdout);
    close(mPimpl->pipeStdout[0]);
    close(mPimpl->pipeStdout[1]);
    dup2(mPimpl->fdStderr, STDERR_FILENO);
    close(mPimpl->fdStderr);
    close(mPimpl->pipeStderr[0]);
    close(mPimpl->pipeStderr[1]);
  }

  mPimpl->isRedirecting = state;
  return 0;
}

// end of namespace
} // namespace InfoLogger
} // namespace AliceO2
