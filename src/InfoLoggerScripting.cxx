// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

/// \file InfoLoggerScripting.cxx
/// \brief This implements the simplified C++ interface for the InfoLogger logging library, as defined in InfoLoggerScripting.hxx
///
///
/// \author Sylvain Chapeland, CERN
///

#include "InfoLoggerScripting.hxx"
using namespace AliceO2::InfoLogger::Scripting;

InfoLogger::InfoLogger()
{
  logHandle = std::make_unique<baseInfoClass>();
}

InfoLogger::~InfoLogger()
{
  logHandle = nullptr;
}

int InfoLogger::log(const baseInfoClass::Severity severity, const std::string& message)
{
  if (logHandle == nullptr) {
    return -1;
  }
  // use default metadata, if defined
  if (isDefinedDefaultMetadata) {
    baseInfoClass::InfoLoggerMessageOption msgOpt;
    msgOpt = defaultMetadata.msgOpt;
    // if set, use the explicitely provided severity instead of default
    if (severity != baseInfoClass::Severity::Undefined) {
      msgOpt.severity = severity;
    }
    return logHandle->log(msgOpt, defaultMetadata.context, "%s", message.c_str());
  } else {
    // no default metadata stored, just log message with appropriate severity
    if (severity != baseInfoClass::Severity::Undefined) {
      return logHandle->log(severity, "%s", message.c_str());
    }
    // use Info severity if not set
    return logHandle->log(baseInfoClass::Severity::Info, "%s", message.c_str());
  }
}

int InfoLogger::log(const std::string& message)
{
  // wrap to the main call, without severity defined
  return log(baseInfoClass::Severity::Undefined, message);
}

int InfoLogger::log(const InfoLoggerMetadata& metadata, const std::string& message)
{
  if (logHandle == nullptr) {
    return -1;
  }
  return logHandle->log(metadata.msgOpt, metadata.context, "%s", message.c_str());
}

int InfoLogger::logInfo(const std::string& message)
{
  return log(baseInfoClass::Info, message);
}

int InfoLogger::logError(const std::string& message)
{
  return log(baseInfoClass::Error, message);
}

int InfoLogger::logWarning(const std::string& message)
{
  return log(baseInfoClass::Warning, message);
}

int InfoLogger::logFatal(const std::string& message)
{
  return log(baseInfoClass::Fatal, message);
}

int InfoLogger::logDebug(const std::string& message)
{
  return log(baseInfoClass::Debug, message);
}

int InfoLogger::setDefaultMetadata(const InfoLoggerMetadata& md)
{
  isDefinedDefaultMetadata = true;
  defaultMetadata = md;
  return 0;
}

int InfoLogger::unsetDefaultMetadata()
{
  isDefinedDefaultMetadata = false;
  defaultMetadata.reset();
  return 0;
}

InfoLoggerMetadata::InfoLoggerMetadata()
{
  reset();
}

InfoLoggerMetadata::InfoLoggerMetadata(const InfoLoggerMetadata& ref)
{
  context = ref.context;
  msgOpt = ref.msgOpt;
}

InfoLoggerMetadata::~InfoLoggerMetadata()
{
}

int InfoLoggerMetadata::setField(const std::string& key, const std::string& value)
{
  // try to set context
  baseInfo::InfoLoggerContext::FieldName contextKey;
  // convert string to field enum
  if (baseInfo::InfoLoggerContext::getFieldNameFromString(key, contextKey) == 0) {
    if (context.setField(contextKey, value) == 0) {
      return 0;
    }
  }

  // try to set message option
  if (baseInfo::InfoLogger::setMessageOption(key.c_str(), value.c_str(), msgOpt) == 0) {
    return 0;
  }

  // key not found
  return -1;
}

void InfoLoggerMetadata::reset()
{
  // cleanup InfoLoggerContext. it is refreshed from current environment.
  context.reset();
  context.refresh();
  // cleanup InfoLoggerMessageOption
  msgOpt = baseInfo::InfoLogger::undefinedMessageOption;
  // use Info as default severity
  msgOpt.severity = baseInfo::InfoLogger::Severity::Info;
}
