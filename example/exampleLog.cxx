// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

#include <InfoLogger/InfoLogger.hxx>

// optionnaly, use the helper macros
#include <InfoLogger/InfoLoggerMacros.hxx>
      
using namespace AliceO2::InfoLogger;
int main()
{
  // create handle to infologger system
  InfoLogger theLog;

  // or, alternatively, to start infologger library in debug mode on stdout
  // (can also be done at runtime by setting environment O2_INFOLOGGER_MODE)
  // outputMode can be one of stdout, debug, infoLoggerD, ...
  // InfoLogger theLog("outputMode=debug");

  // optionnaly, customize some of the "static" fields
  // to be applied to all messages
  InfoLoggerContext theLogContext;
  theLogContext.setField(InfoLoggerContext::FieldName::Facility, "myTestFacility");
  theLog.setContext(theLogContext);

  // log a message
  theLog.log("infoLogger message test");
    
  // log a warning with code 1234 for developers
  theLog.log(LogWarningDevel_(1234), "this is a test warning message");
  
  return 0;
}
