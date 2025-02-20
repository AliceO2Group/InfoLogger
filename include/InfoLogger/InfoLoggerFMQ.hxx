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

#ifndef INFOLOGGER_INFOLOGGERFMQ_HXX
#define INFOLOGGER_INFOLOGGERFMQ_HXX

#include <fairmq/FairMQLogger.h>
#include <InfoLogger/InfoLogger.hxx>

#define INFOLOGGER_FMQ_SINK_NAME "infoLogger"

// This function setup a custom sink for FMQ logs so that they are redirected to infoLogger
// The infologger context is set when invoked. Corresponding fields are fixed afterwards.
// The infoLogger handle given as parameter should remain valid for the lifetime of FMQ,
// or until unsetFMQLogsToInfoLogger() is called
void setFMQLogsToInfoLogger(AliceO2::InfoLogger::InfoLogger* logPtr = nullptr)
{

  static AliceO2::InfoLogger::InfoLoggerContext ctx;
  // set facility = FMQ? better to add dedicated "module" field
  //ctx.setField(AliceO2::InfoLogger::InfoLoggerContext::FieldName::Facility,"FMQ");

  static AliceO2::InfoLogger::InfoLogger* theLogPtr = logPtr;
  if (logPtr == nullptr) {
    static AliceO2::InfoLogger::InfoLogger theLog;
    logPtr = &theLog;
  }

  fair::Logger::AddCustomSink(
    INFOLOGGER_FMQ_SINK_NAME, "trace", [&](const std::string& content, const fair::LogMetaData& metadata) {
      // todo: update context from time to time?
      // ctx.refresh();

      // translate FMQ metadata
      AliceO2::InfoLogger::InfoLogger::InfoLogger::Severity severity = AliceO2::InfoLogger::InfoLogger::Severity::Undefined;
      int level = AliceO2::InfoLogger::InfoLogger::undefinedMessageOption.level;
      const char *prefix = NULL;

      if (metadata.severity_name == fair::Logger::SeverityName(fair::Severity::nolog)) {
        // discard
        return;
      } else if (metadata.severity_name == fair::Logger::SeverityName(fair::Severity::fatal)) {
        severity = AliceO2::InfoLogger::InfoLogger::Severity::Fatal;
      } else if (metadata.severity_name == fair::Logger::SeverityName(fair::Severity::critical)) {
        severity = AliceO2::InfoLogger::InfoLogger::Severity::Error;
        prefix = "Critical";
      } else if (metadata.severity_name == fair::Logger::SeverityName(fair::Severity::error)) {
        severity = AliceO2::InfoLogger::InfoLogger::Severity::Error;
      } else if (metadata.severity_name == fair::Logger::SeverityName(fair::Severity::alarm)) {
        severity = AliceO2::InfoLogger::InfoLogger::Severity::Info;
        prefix = "Alarm";
      } else if (metadata.severity_name == fair::Logger::SeverityName(fair::Severity::important)) {
        severity = AliceO2::InfoLogger::InfoLogger::Severity::Info;
        prefix = "Important";
      } else if (metadata.severity_name == fair::Logger::SeverityName(fair::Severity::warn)) {
        severity = AliceO2::InfoLogger::InfoLogger::Severity::Warning;
      } else if (metadata.severity_name == fair::Logger::SeverityName(fair::Severity::state)) {
        severity = AliceO2::InfoLogger::InfoLogger::Severity::Info;
        level = 10;
        prefix = "State";
      } else if (metadata.severity_name == fair::Logger::SeverityName(fair::Severity::info)) {
        severity = AliceO2::InfoLogger::InfoLogger::Severity::Info;
      } else if (metadata.severity_name == fair::Logger::SeverityName(fair::Severity::detail)) {
        severity = AliceO2::InfoLogger::InfoLogger::Severity::Info;
        level = 10;
      } else if (metadata.severity_name == fair::Logger::SeverityName(fair::Severity::debug)) {
        severity = AliceO2::InfoLogger::InfoLogger::Severity::Debug;
      } else if (metadata.severity_name == fair::Logger::SeverityName(fair::Severity::debug1)) {
        severity = AliceO2::InfoLogger::InfoLogger::Severity::Debug;
        level = 10;
      } else if (metadata.severity_name == fair::Logger::SeverityName(fair::Severity::debug2)) {
        severity = AliceO2::InfoLogger::InfoLogger::Severity::Debug;
        level = 20;
      } else if (metadata.severity_name == fair::Logger::SeverityName(fair::Severity::debug3)) {
        severity = AliceO2::InfoLogger::InfoLogger::Severity::Debug;
        level = 30;
      } else if (metadata.severity_name == fair::Logger::SeverityName(fair::Severity::debug4)) {
        severity = AliceO2::InfoLogger::InfoLogger::Severity::Debug;
        level = 40;
      } else if (metadata.severity_name == fair::Logger::SeverityName(fair::Severity::trace)) {
        severity = AliceO2::InfoLogger::InfoLogger::Severity::Debug;
        level = 50;
      }

      std::string fileName = std::string(metadata.file);
      int fileLine = atoi(std::string(metadata.line).c_str());
      AliceO2::InfoLogger::InfoLogger::InfoLoggerMessageOption opt = {
        severity,
        level,
        AliceO2::InfoLogger::InfoLogger::undefinedMessageOption.errorCode,
        fileName.c_str(),
        fileLine
      };
      if (prefix == NULL) {
        theLogPtr->log(opt, ctx, "FMQ: %s", content.c_str());
      } else {
        theLogPtr->log(opt, ctx, "FMQ: %s - %s", prefix, content.c_str());
      }
    });

  fair::Logger::SetCustomSeverity(INFOLOGGER_FMQ_SINK_NAME, fair::Logger::GetConsoleSeverity());
  fair::Logger::SetConsoleSeverity(fair::Severity::nolog);
}

// unregister FMQ to InfoLogger redirection
void unsetFMQLogsToInfoLogger() {
  fair::Logger::RemoveCustomSink(INFOLOGGER_FMQ_SINK_NAME); 
}

#endif //INFOLOGGER_INFOLOGGERFMQ_HXX

