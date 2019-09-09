// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
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

using namespace AliceO2::InfoLogger;

int main()
{
  InfoLogger theLog;

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

  InfoLogger::InfoLoggerMessageOption options = InfoLogger::undefinedMessageOption;
  options.sourceLine = __LINE__;
  options.sourceFile = __FILE__;
  theLog.log(options, "infoLogger message test with source code info");
  theLog.log({ InfoLogger::Severity::Info, 123, 456, __FILE__, __LINE__ }, "infoLogger message test with source code info");
  theLog.log(LOGINFO(123), "infoLogger message test with source code info");

  theLog << "another test message " << InfoLogger::endm;
  theLog << InfoLogger::Severity::Error << "another (stream error) message " << InfoLogger::endm;
  theLog << InfoLogger::Severity::Warning << "another (stream warning) message " << InfoLogger::endm;
  theLog << "yet another test message "
         << "(in " << 2 << " parts)" << InfoLogger::endm;
  theLog << "message with formatting: " << boost::format("%+5d %.3f") % 12345 % 5.4321 << InfoLogger::endm;
  theLog << LOGINFO(10) << "infoLogger message test with source code info, level 10" << InfoLogger::endm;
  theLog << LOGERROR(10, 999) << "infoLogger error message test with source code info, level 10 errcode 999" << InfoLogger::endm;

  return 0;
}
