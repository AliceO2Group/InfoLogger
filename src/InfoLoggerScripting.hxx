// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

/// \file InfoLoggerScripting.hxx
/// \brief a simplified C++ interface for the InfoLogger logging library.
///
///  This provides a string-only access to (a subset of) the InfoLogger API features.
///  In particular, it provides means to set the message metadata.
///  It aims at having a coherent and universal interface using simple types for the various language bindings supported by InfoLogger.
///  C++ users concerned by possible performance overhead implied by strings and extra parsing should use the main interface instead.
///
///  See inline documentation.
///
///  Warning: this is not the public InfoLogger interface.
///  Although some class names are identical, this internal interface has a specific namespace
///  and is dedicated to easily generate SWIG wrappers for various (scripting or not) languages.
///
/// \author Sylvain Chapeland, CERN
///

#ifndef INFOLOGGER_INFOLOGGERSCRIPTING_HXX
#define INFOLOGGER_INFOLOGGERSCRIPTING_HXX

#include <InfoLogger/InfoLogger.hxx>
#include <memory>

namespace AliceO2
{
namespace InfoLogger
{
namespace Scripting
{

namespace baseInfo = AliceO2::InfoLogger;
using baseInfoClass = baseInfo::InfoLogger;

/// A class to store metadata associated to a message
class InfoLoggerMetadata
{
 public:
  // constructor
  // metadata initialized with default values or, if available, from the runtime environment
  InfoLoggerMetadata();

  // copy-constructor
  // all metadata copied from existing instance
  InfoLoggerMetadata(const InfoLoggerMetadata&);

  // destructor
  ~InfoLoggerMetadata();

  // Set a field in a message metadata, based on its name.
  // It can be any field from InfoLoggerMessageOption (1) or InfoLoggerContext (2)
  // If value is empty (zero-length string), the default value is used for the corresponding field.
  // Valid values: (1) Severity, Level, ErrorCode, SourceFile, SourceLine (2) Facility, Role, System, Detector, Partition, Run
  int setField(const std::string& key, const std::string& value);

 protected:
  baseInfo::InfoLoggerContext context;
  baseInfoClass::InfoLoggerMessageOption msgOpt;
  void reset(); // init fields to suitable default values

  friend class InfoLogger;
};

/// A class with simplified API for logging
/// We use the same name as the base InfoLogger class on purpose.
class InfoLogger
{
 public:
  InfoLogger();
  ~InfoLogger();

  // log with default metadata fields and specified severity
  int logInfo(const std::string& message);
  int logError(const std::string& message);
  int logWarning(const std::string& message);
  int logFatal(const std::string& message);
  int logDebug(const std::string& message);

  // log with specified metadata
  int log(const InfoLoggerMetadata& metadata, const std::string& message);

  // log with default metadata
  int log(const std::string& message);

  // set / unset metadata to be used for each message, if none is specified
  int setDefaultMetadata(const InfoLoggerMetadata&);
  int unsetDefaultMetadata();

  // aliases for the log(...) functions, for those languages unfriendly with overloading
  int logS(const std::string& message)
  {
    return log(message);
  }
  int logM(const InfoLoggerMetadata& metadata, const std::string& message)
  {
    return log(metadata, message);
  }

 private:
  std::unique_ptr<baseInfoClass> logHandle;
  bool isDefinedDefaultMetadata = false;
  InfoLoggerMetadata defaultMetadata;
  inline int log(const baseInfoClass::Severity severity, const std::string& message);
};

} // namespace Scripting
} // namespace InfoLogger
} // namespace AliceO2
#endif //INFOLOGGER_INFOLOGGERSCRIPTING_HXX
