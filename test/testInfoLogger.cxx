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

/// \file testInfoLogger.cxx
/// \brief Example usage of the c++ InfoLogger logging interface.
///
/// \author Sylvain Chapeland, CERN

#include <InfoLogger/InfoLogger.hxx>
#include <boost/format.hpp>
#include <InfoLoggerErrorCodes.h>
#include <InfoLogger/InfoLoggerMacros.hxx>

using namespace AliceO2::InfoLogger;

int main()
{
  if (0) {
    InfoLogger theLog("verbose=1");
    theLog.log("infoLogger message test");
    sleep(1);
    return 0;
  }

  InfoLogger theLog;

  if (0) {
    theLog.historyReset(2);
    theLog.registerErrorCodes({{123, "error"}, {124, "fatal"}});
    theLog.log(LogInfoSupport_(100), "test info");
    theLog.log(LogErrorSupport_(123), "test error 123");
    theLog.log(LogInfoDevel_(100), "test info");
    theLog.log(LogFatalSupport_(124), "test fatal 124");
    std::vector<std::string> m;
    theLog.historyGetSummary(m);
    printf("log summary:\n");
    for(const auto&s: m) {
      printf("  %s\n",s.c_str());
    }
    printf("\n");
    return 0;
  }

  theLog.log("infoLogger message test");
  printf("Message on stdout (initial stdout)\n");
  theLog.setStandardRedirection(1);
  printf("Message on stdout (redirection enabled)\n");
  fprintf(stderr, "Message on stderr (redirection enabled)\n");
  usleep(100000);
  theLog.setStandardRedirection(0);
  printf("Message on stdout (redirection stopped)\n");

  theLog.log("infoLogger message test");
  theLog.log(InfoLogger::Severity::Info, "infoLogger INFO message test");
  theLog.log(InfoLogger::Severity::Warning, "infoLogger WARNING message test");
  theLog.log(InfoLogger::Severity::Error, "infoLogger ERROR message test");
  theLog.log(InfoLogger::Severity::Fatal, "infoLogger FATAL message test");
  theLog.log(InfoLogger::Severity::Debug, "infoLogger DEBUG message test");

  theLog.log(LogInfoDevel, "Number of messages logged so far: %lu I=%lu", theLog.getMessageCount(InfoLogger::Severity::Undefined), theLog.getMessageCount(InfoLogger::Severity::Info));

  InfoLogger::InfoLoggerMessageOption options = InfoLogger::undefinedMessageOption;
  options.sourceLine = __LINE__;
  options.sourceFile = __FILE__;
  theLog.log(options, "infoLogger message test with source code info");
  theLog.log({ InfoLogger::Severity::Info, 123, 456, __FILE__, __LINE__ }, "infoLogger message test with source code info");
  theLog.log(LOGINFO(123), "infoLogger message test with source code info");

  // example use of context
  InfoLoggerContext ctxt({{InfoLoggerContext::FieldName::Facility, std::string("test1")}});
  theLog.log({InfoLogger::Severity::Info},ctxt,"infoLogger message - facility test1");

  // reuse a context and overwrite some fields
  theLog.log({InfoLogger::Severity::Info},InfoLoggerContext(ctxt,{{InfoLoggerContext::FieldName::Facility, std::string("test2")}}),"infoLogger message - facility test2");

  // c++ style
  theLog << "another test message " << InfoLogger::endm;
  theLog << InfoLogger::Severity::Error << "another (stream error) message " << InfoLogger::endm;
  theLog << InfoLogger::Severity::Warning << "another (stream warning) message " << InfoLogger::endm;
  theLog << "yet another test message "
         << "(in " << 2 << " parts)" << InfoLogger::endm;
  theLog << "message with formatting: " << boost::format("%+5d %.3f") % 12345 % 5.4321 << InfoLogger::endm;
  theLog << LOGINFO(10) << "infoLogger message test with source code info, level 10" << InfoLogger::endm;
  theLog << LOGERROR(10, 999) << "infoLogger error message test with source code info, level 10 errcode 999" << InfoLogger::endm;

  theLog.log(LogInfoDevel, "Test message with InfoLoggerMessageOption macro");
  theLog << LogInfoOps << "Test message with InfoLoggerMessageOption macro" << InfoLogger::endm;
  
  
  // local filtering of messages  
  theLog.log(LogInfoDevel, "Devel message without filter");
  theLog.filterDiscardLevel(InfoLogger::Level::Devel);
  theLog.log(LogInfoDevel, "Devel message with level filter - you should not see this");
  theLog.filterReset();
  theLog.log(LogInfoDevel, "Devel message with level filter reset - you should see this again");
  theLog.log(LogDebugDevel, "Debug message without filter");
  theLog.filterDiscardDebug(1);
  theLog.log(LogDebugDevel, "Debug message with severity filter - you should not see this");
  theLog.log(LogInfoDevel, "Info message with severity filter - you should see this");
  theLog.filterReset();
  theLog.log(LogDebugDevel, "Debug message with severity filter - you should see this again");

  #define PATH_LOGS_DISCARDED "/tmp/logsdiscarded.txt"
  theLog.log(LogInfoDevel, "Discarding messages to file " PATH_LOGS_DISCARDED);
  theLog.filterDiscardLevel(InfoLogger::Level::Devel);
  theLog.filterDiscardDebug(1);
  theLog.filterDiscardSetFile(PATH_LOGS_DISCARDED);
  theLog.log(LogInfoDevel, "Devel message with level filter - you should see this only in file");
  theLog.log(LogDebugDevel, "Debug message with severity filter - you should see this only in file");
  theLog.log(LogErrorDevel, "Devel error message with level filter - you should see this only in file");
  theLog.filterReset();

  // message verbosity control with auto-mute
  const int limitN = 5; // max number of messages ...
  const int limitT = 3; // ... for given time interval
  theLog.log("Will now test auto-mute: limit = %d msg / %d s", limitN, limitT);
  // define a static variable token, and pass it to all relevant log() calls
  static InfoLogger::AutoMuteToken msgLimit(LogInfoDevel_(1234), limitN, limitT);  
  theLog << &msgLimit << "This is message loop 0 (c++ style)" << InfoLogger::endm;
  // a couple of loops to show behavior
  for (int j=0; j<2; j++) {
    const int nmsg = 20 * limitN;
    const float msgRate = 10;
    theLog.log("Injecting %d msgs at %.2f Hz", nmsg, msgRate);
    for (int i=1; i<= nmsg; i++) {
      theLog.log(msgLimit, "This is message loop %d", i);
      usleep((int)(1000000/msgRate));
    }
    const int pauseTime = limitT+1;
    theLog.log("Pause %d s", pauseTime);
    usleep(pauseTime*1000000);
  }
  theLog.log(msgLimit, "Final message test for auto-mute");
  theLog.log("End of test");  
  return 0;
}

