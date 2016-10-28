/// \file InfoLogger.hxx
/// \brief C++ interface for the InfoLogger logging interface.
///
///  See inline documentation, and testInfoLogger.cxx for example usage.
///
/// \author Sylvain Chapeland, CERN
///

#ifndef INFOLOGGER_INFOLOGGER_HXX
#define INFOLOGGER_INFOLOGGER_HXX

#include <string>
#include <iostream>
#include <boost/lexical_cast.hpp>
#include <memory>

namespace AliceO2 {
namespace InfoLogger {


/// This class instanciates an infoLogger connection.
class InfoLogger
{

  public:

  /// Constructor
  /// May throw exceptions on failure.
  InfoLogger();

  /// Destructor
  ~InfoLogger();


  /// Definition of possible message severities
  enum Severity {Info='I', Error='E', Fatal='F', Warning='W', Debug='D'};


  /// Log a message. (severity set to info)
  /// \param message  NUL-terminated string message to push to the log system. It uses the same format as specified for printf(), and the function accepts additionnal formatting parameters.
  /// \param ...      Extra optionnal parameters for formatting.
  /// \return         0 on success, an error code otherwise (but never throw exceptions)..
  int log(const char *message, ...) __attribute__((format(printf, 2, 3)));

  /// Log a message.
  /// \param severity Message severity (info, error, ...)
  /// \param message  NUL-terminated string message to push to the log system. It uses the same format as specified for printf(), and the function accepts additionnal formatting parameters.
  /// \param ...      Extra optionnal parameters for formatting.
  /// \return         0 on success, an error code otherwise (but never throw exceptions)..
  int log(Severity severity, const char *message, ...) __attribute__((format(printf, 3, 4)));

  /// Log a message, with a list of arguments of type va_list.
  /// \param severity Message severity (info, error, ...)
  /// \param message  NUL-terminated string message to push to the log system. It uses the same format as specified for printf(), and the function accepts additionnal formatting parameters.
  /// \param ap       Variable list of arguments (c.f. vprintf)
  /// \return         0 on success, an error code otherwise (but never throw exceptions).
  int logV(Severity severity, const char *message, va_list ap) __attribute__((format(printf, 3, 0)));


  /// Control commands for infoLogger stream (accepted by << operator)
  enum StreamOps
  {
    endm    ///< tag to mark end of message, and flush current buffer
  };


  /// Specialized << version for std::string  
  InfoLogger &operator<<(const std::string message);

  /// Specialized << version for control commands
  /// In particular, to mark (and flush) the end of a message.
  InfoLogger &operator<<(const InfoLogger::StreamOps op);

  /// Specialized << version to set message severity
  InfoLogger &operator<<(const InfoLogger::Severity severity);


  /// Log a message using the << operator, like for std::cout.
  /// All messages must be ended with the InfoLogger::StreamOps::endm tag.
  /// Severity can be set at any point in the stream (before endm). Set to Info by default.
  template<typename T>
  InfoLogger &operator<<(const T &message)
  {
    *this << boost::lexical_cast<std::string>(message);
    return *this;
  }


  private:
  class Impl;                       // private class for implementation
  std::unique_ptr<Impl> pImpl;      // handle to private class instance at runtime
};

}
}

#endif //INFOLOGGER_INFOLOGGER_HXX
