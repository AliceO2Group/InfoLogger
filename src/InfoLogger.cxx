/// \file InfoLogger.cxx
/// \brief C++ wrapper for the InfoLogger logging interface.
///
/// \author Sylvain Chapeland, CERN

#include "InfoLogger/InfoLogger.hxx"
#include "InfoLogger/InfoLogger.h"

namespace AliceO2 {
namespace InfoLogger {

// private class to isolate internal data from external interface
class InfoLoggerPrivate
{
  InfoLoggerPrivate();   //< constructor
  ~InfoLoggerPrivate();  //< destructor

  friend class InfoLogger;  //< give access to this data from InfoLogger class

  protected:
  InfoLoggerHandle hLog;  //< handle to InfoLogger C API 
  std::string currentStreamMessage; //< temporary variable to store message when concatenating << operations, until "endm" is received  
};

InfoLoggerPrivate::InfoLoggerPrivate()
{
  // open handle to C API
  int err;
  hLog = NULL;
  err = infoLoggerOpen(&hLog);
  if (err) {
    throw err;
  }
  // initiate internal members
  currentStreamMessage.clear();
}

InfoLoggerPrivate::~InfoLoggerPrivate()
{
  // release handle to C API
  infoLoggerClose(hLog);
}

InfoLogger::InfoLogger()
{
  // instantiate private data
  dPtr = NULL;
  dPtr = new InfoLoggerPrivate();
  if (dPtr == NULL) { throw __LINE__; }
}


InfoLogger::~InfoLogger()
{
  // release private data
  if (dPtr != NULL) {
    delete dPtr;
  }
}

int InfoLogger::log(const char *message, ...)
{
  // wrapper to C API, forward variable list of arguments
  int err;
  va_list ap;
  va_start(ap, message);
  err = infoLoggerLogV(dPtr->hLog, message, ap);
  va_end(ap);
  return err;
}

InfoLogger &InfoLogger::operator<<(const std::string message)
{
  // store data in internal variable
  dPtr->currentStreamMessage.append(message);
  return *this;
}

InfoLogger &InfoLogger::operator<<(InfoLogger::StreamOps op)
{
  // process special commands received by << operator

  // end of message: flush current buffer in a single message
  if (op == endm) {
    log(dPtr->currentStreamMessage.c_str());
    dPtr->currentStreamMessage.clear();
  }
  return *this;
}

}
}