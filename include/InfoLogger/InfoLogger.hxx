// Copyright 2019-2020 CERN and copyright holders of ALICE O2.
// See https://alice-o2.web.cern.ch/copyright for details of the copyright holders.
// All rights not expressly granted are reserved.
//
// This software is distributed under the terms of the GNU General Public
// License v3 (GPL Version 3), copied verbatim in the file "COPYING".
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

/// \file InfoLogger.hxx
/// \brief C++ interface for the InfoLogger logging library.
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
#include <list>
#include <utility>
#include <type_traits>
#include <atomic>
#include <chrono>

// here are some macros to help including source code info in infologger messages
// to be used to quickly specify "infoLoggerMessageOption" argument in some logging functions

// this one sets an info message with provided log level
#define LOGINFO(level) \
  InfoLogger::InfoLoggerMessageOption { InfoLogger::Severity::Info, level, -1, __FILE__, __LINE__ }
// this one sets an error message with provided log level and error code
#define LOGERROR(level, errno) \
  InfoLogger::InfoLoggerMessageOption { InfoLogger::Severity::Error, level, errno, __FILE__, __LINE__ }

namespace AliceO2
{
namespace InfoLogger
{

// a class to define a context for infoLogger messages
// i.e. extra fields to be associated to an infoLogger message
// these are mostly static fields, usually stable from one message to another for a given infoLogger source
// Some fields are not part of the context: message specific fields (timestamp, severity, level, error code, source line/file)
// Some fields are not accessible for writing: process-related information (pid, hostname, etc)
// Main fields to be defined by users: facility, etc
// The others fields should be set automatically from runtime context

class InfoLoggerContext final
{

 public:
  /// Definition of field names (keys) which are available/settable in InfoLoggerContext
  enum class FieldName { Facility,
                         Role,
                         System,
                         Detector,
                         Partition,
                         Run };

  /// Function to parse input string and find matching FieldName.
  /// On success, output variable is set accordingly.
  /// \return         0 on success, -1 if no match found.
  static int getFieldNameFromString(const std::string& input, FieldName& output);

  /// Create new context.
  /// The context is updated from available environment information, if any.
  InfoLoggerContext();

  /// Create new context.
  /// The context is updated from available environment information, if any.
  /// Additionnaly, listed fields are set with provided value.
  /// \param listOfKeyValuePairs      A list of fieldName / value pairs to be set. Fields which were set automatically from environment are overwritten.
  InfoLoggerContext(const std::list<std::pair<FieldName, const std::string&>>& listOfKeyValuePairs);

  /// Create new context.
  /// The context is created as a copy of an existing context.
  /// Additionnaly, listed fields are set with provided value.
  /// \param listOfKeyValuePairs      A list of fieldName / value pairs to be set. Fields which were set from copy source are overwritten.
  InfoLoggerContext(const InfoLoggerContext &sourceContext, const std::list<std::pair<FieldName, const std::string&>>& listOfKeyValuePairs);

  /// Update context with default values
  /// All fields are cleared with default values.
  void reset();

  /// Update context from current environment information (of current process).
  /// Fields previously set by user may be overwritten.
  void refresh();

  /// Update context from current environment information of a different process.
  /// This is used internally by command line 'log' utility to tag messages issued by parent process
  /// or on stdout by a program which output is piped to stdin of current process and injected in infologger.
  /// Fields previously set by user are overwritten.
  void refresh(pid_t pid);

  /// Destroy context
  ~InfoLoggerContext();

  /// Set given field (provided value is a string)
  /// \param key      The field name to be set.
  /// \param value    The value of the field, as a string. This function can be called for any field (string or integer). Conversion will be done whenever needed (string->int).
  /// \return         0 on success, an error code otherwise (but never throw exceptions).
  int setField(FieldName key, const std::string& value);

  /// Set list of given fields (provided value is a string)
  /// \param listOfKeyValuePairs      A list of fieldName / value pairs to be set.
  /// \return         0 on success, an error code otherwise (but never throw exceptions).
  int setField(const std::list<std::pair<FieldName, const std::string&>>& listOfKeyValuePairs);

  // this simplifies interface to have a single type
  // some fields (e.g. run, pid) are stored as integers, but conversion is done by setField
  // A field with a blank value (zero-length string) is undefined.

 private:
  // field undefined: empty string (for strings) or -1 (for integers)

  // application-level fields, can be set manually
  std::string facility;  // facility (or module: readout, monitoring, ...)
  std::string role;      // role name of the running process (e.g. FLP-TPC-1, ...)
  std::string system;    // system name (DAQ,ECS,TRG,...)
  std::string detector;  // detector name (internally converted to 3-letter detector code if found)
  std::string partition; // partition name (if concept kept for run 3)
  int run;               // run number (if concept kept for run 3)

  // non-writable fields, set automatically
  int processId;        // PID of the message source process
  std::string hostName; // name of host running the message source process
  std::string userName; // user running the message source process

  // ideas of other possible fields: thread id, exe name, ...

  friend class InfoLogger;
};

/// This class instanciates an infoLogger connection.
class InfoLogger
{

 public:
  /// Constructor
  /// May throw exceptions on failure.
  InfoLogger();

  // Constructor
  // with optional options
  // provided as a comma-separated list of key=value pairs
  InfoLogger(const std::string& options);

  /// Destructor
  virtual ~InfoLogger();

  ////////////////////////////////
  /// Internal types and constants
  ////////////////////////////////

  /// Definition of possible message severities
  enum Severity { Info = 'I',
                  Error = 'E',
                  Fatal = 'F',
                  Warning = 'W',
                  Debug = 'D',
                  Undefined = 'U' };

  /// Some predefined constants that may be used to set the message level
  /// (i.e. visibility of the message, based on who reads it)
  /// The level is an integer in the 1-99 range (1: topmost visibility)
  /// The enum below provides the main boundaries for typical operations,
  /// and one may use finer granularity within each range.
  /// operations (1-5) support (6-10) developer (11-20) trace (21-99).
  /// Trace messages should typically not be enabled in normal running conditions,
  /// and usually related to debugging activities (also akin to the 'Debug' severity).
  enum Level { 
  	Ops = 1,
	Support = 6,
	Devel = 11,
	Trace = 21
  };

  ///////////////////////
  /// C-style interface
  ///////////////////////

  /// Log a message. (severity set to info)
  /// \param message  message to push to the log system.
  /// \return         0 on success, an error code otherwise (but never throw exceptions)..
  int logInfo(const std::string& message);

  /// Log a message. (severity set to error)
  /// \param message  message to push to the log system.
  /// \return         0 on success, an error code otherwise (but never throw exceptions)..
  int logError(const std::string& message);

  /// Log a message. (severity set to info)
  /// \param message  NUL-terminated string message to push to the log system. It uses the same format as specified for printf(), and the function accepts additionnal formatting parameters.
  /// \param ...      Extra optionnal parameters for formatting.
  /// \return         0 on success, an error code otherwise (but never throw exceptions)..
  int log(const char* message, ...) __attribute__((format(printf, 2, 3)));

  /// Log a message.
  /// \param severity Message severity (info, error, ...)
  /// \param message  NUL-terminated string message to push to the log system. It uses the same format as specified for printf(), and the function accepts additionnal formatting parameters.
  /// \param ...      Extra optionnal parameters for formatting.
  /// \return         0 on success, an error code otherwise (but never throw exceptions)..
  int log(Severity severity, const char* message, ...) __attribute__((format(printf, 3, 4)));

  /// Log a message, with a list of arguments of type va_list.
  /// \param severity Message severity (info, error, ...)
  /// \param message  NUL-terminated string message to push to the log system. It uses the same format as specified for printf(), and the function accepts additionnal formatting parameters.
  /// \param ap       Variable list of arguments (c.f. vprintf)
  /// \return         0 on success, an error code otherwise (but never throw exceptions).
  int logV(Severity severity, const char* message, va_list ap) __attribute__((format(printf, 3, 0)));

  // set default context
  // it will be used for all messages without an explicit context argument.
  int setContext(const InfoLoggerContext&);

  // unset default context
  // all context fields will be set to default unless an explicit context argument is given for each message.
  int unsetContext();

  /// Turn on/off stdout/stderr redirection
  /// true -> turn ON. Stdout/Stderr are redirected to internal pipes and a dedicated thread captures the stream and redirects to this instance of infologger (with corresponding severity).
  /// false -> turn OFF (if it was ON). Stdout/Stderr are redirected to previous outputs, and capture thread is stopped.
  /// Return 0 on success, or an error code otherwise
  int setStandardRedirection(bool state);

  // todo: common handling of error codes in O2?
  // 0-9999: "shared" -> can be reused accross modules
  // 99.9999: 2-digit 'error namespace' + local error id

  // Need something lightweight, just to pass parameters to logging function.
  // Instances of this class are ephemeral, and should mostly be statically allocated at compile time.
  struct InfoLoggerMessageOption {
    InfoLogger::Severity severity;
    int level;
    int errorCode;
    const char* sourceFile;
    int sourceLine;
  };

  // make sure options are a POD struct
  static_assert(std::is_pod<InfoLoggerMessageOption>::value, "struct InfoLoggerMessageOption is not POD");

  /// Definition of a constant, to be used for corresponding fields when not defined
  static constexpr InfoLoggerMessageOption undefinedMessageOption = {
    Severity::Undefined, // severity
    -1,                  // level
    -1,                  // errorCode
    nullptr,             // sourceFile
    -1,                  // sourceLine
  };

  // Token used to control the flow of messages.
  // Allows auto-mute when defined amount of messages using same token in a period of time is exceeded.
  //
  // Constructor parameters:
  // logOptions: Logging options used for the log calls associated to this token.
  // maxMsg: Maximum number of messages for the selected time interval, triggering auto-mute.
  // interval: Time interval (duration in seconds) used for counting the number of messages.
  struct AutoMuteToken {
    public:
    AutoMuteToken(const InfoLogger::InfoLoggerMessageOption vLogOptions, unsigned int vMaxMsg = 10, unsigned int vInterval = 5) {
      count = 0;
      logOptions = vLogOptions;
      maxMsg = vMaxMsg;
      interval = vInterval;
    };
    ~AutoMuteToken() {
    };
    
    protected:
    friend class InfoLogger;
    InfoLogger::InfoLoggerMessageOption logOptions; // options used for associated logs
    unsigned int maxMsg; // maximum number of messages for the selected time interval
    unsigned int interval; // duration of interval (seconds)
    std::atomic<unsigned int> count; // number of messages so far in interval
    std::chrono::time_point<std::chrono::steady_clock> t0; // time of beginning of interval
    std::chrono::time_point<std::chrono::steady_clock> t1; // time of previous message
    unsigned int ndiscarded; // number of messages discarded in last interval
    unsigned int ndiscardedtotal; // number of messages discarded since beginning of verbose phase    
  };

  /// Convert a string to an infologger severity
  /// \param text  NUL-terminated word to convert to InfoLogger severity type. Current implementation is not exact-match, it takes closest based on first-letter value
  /// \return      Corresponding severity (InfoLogger::Undefined if no match found)
  static InfoLogger::Severity getSeverityFromString(const char* text);

  /// Set a field in a message option struct based on its name.
  /// If input fieldName valid, and input fieldValue can be parsed, ouput variable is modified accordingly.
  /// If fieldValue is blank (zero-length string), the default value is used for the corresponding field.
  /// List of valid field names are the strings (1st letter capitalized) corresponding to what is defined in the InfoLoggerMessageOption struct: Severity, Level, ErrorCode, SourceFile, SourceLine
  /// \return         0 on success, an error code otherwise.
  static int setMessageOption(const char* fieldName, const char* fieldValue, InfoLoggerMessageOption& output);

  /// extended log function, with all extra fields, including a specific context
  /// \return         0 on success, an error code otherwise (but never throw exceptions).
  int log(const InfoLoggerMessageOption& options, const InfoLoggerContext& context, const char* message, ...) __attribute__((format(printf, 4, 5)));

  /// extended log function, with all extra fields, using default context
  /// \return         0 on success, an error code otherwise (but never throw exceptions).
  int log(const InfoLoggerMessageOption& options, const char* message, ...) __attribute__((format(printf, 3, 4)));

  /// extended log function, with all extra fields, using default context
  /// with automatic message muting, according to the usage of the token parameter provided.
  /// Auto-mute occurs when defined amount of messages using same token in a period of time is exceeded.
  /// The token parameter defines the limits for this maximum verbosity, and keeps internal count of usage.
  /// The token should be the same variable used between consecutive calls for a given type of messages,
  /// typically a static variable properly initialized with desired settings, close to the log call(s) to be controlled.
  ///
  ///
  /// Auto-mute behavior is as follows:
  /// - the time interval starts on the first message, and is reset on the next message after an interval completed. (it is not equal to "X seconds before last message").
  /// - when the number of messages counted in an interval exceeds the threshold, auto-mute triggers ON: next messages (with this token) are discarded
  /// - when auto-mute is on, one message is still printed for each time interval, with statistics about the number of discarded messages -> the logging rate is effectively limited to a couple of messages per interval
  /// - the auto-mute triggers off when there was no message received for a duration equal to the interval time. (this is equal to "X seconds before last message").
  /// 
  /// \return         0 on success, an error code otherwise (but never throw exceptions).
  int log(AutoMuteToken &token, const char* message, ...) __attribute__((format(printf, 3, 4)));

  //////////////////////////
  // iostream-like interface
  //////////////////////////

  /// Control commands for infoLogger stream (accepted by << operator)
  enum StreamOps {
    endm ///< tag to mark end of message, and flush current buffer
  };

  /// Specialized << version for std::string
  InfoLogger& operator<<(const std::string& message);

  /// Specialized << version for control commands
  /// In particular, to mark (and flush) the end of a message.
  InfoLogger& operator<<(const InfoLogger::StreamOps op);

  /// Specialized << version to set message severity
  InfoLogger& operator<<(const InfoLogger::Severity severity);

  /// Specialized << version to set message options
  InfoLogger& operator<<(const InfoLogger::InfoLoggerMessageOption options);

  /// Log a message using the << operator, like for std::cout.
  /// All messages must be ended with the InfoLogger::StreamOps::endm tag.
  /// Severity/options can be set at any point in the stream (before endm). Severity set to Info by default.
  template <typename T>
  InfoLogger& operator<<(const T& message)
  {
    *this << boost::lexical_cast<std::string>(message);
    return *this;
  }


  /// Functions to define a local log filtering policy, to discard immediately any log message
  /// matching criteria.
  /// This allows an application to self-configure its verbosity.
  /// Messages are dropped immediately in the process, they do not reach infoLoggerD.

  /// Select discarding of messages with DEBUG severity.
  /// parameter: 0 (default, debug messages kept) or 1 (debug messages discarded)
  void filterDiscardDebug(bool enable);
  
  /// Select discarding of messages with higher levels.
  /// parameter: level from which messages are discarded (bigger than or equal to this level).
  /// Can be InfoLogger::undefinedMessageOption.level, to keep messages of all levels (default)
  /// Messages with undefined level are never filtered out.
  void filterDiscardLevel(int excludeLevel);

  /// Reset defined filters.
  /// All messages are then kept and handled normally.
  void filterReset();
  
  ///////////////////////
  /// internals
  ///////////////////////

 private:
  class Impl;                   // private class for implementation
  std::unique_ptr<Impl> mPimpl; // handle to private class instance at runtime
};

} // namespace InfoLogger
} // namespace AliceO2

#endif //INFOLOGGER_INFOLOGGER_HXX

