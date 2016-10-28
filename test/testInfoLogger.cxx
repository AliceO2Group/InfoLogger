/// \file testInfoLogger.cxx
/// \brief Example usage of the c++ InfoLogger logging interface.
///
/// \author Sylvain Chapeland, CERN

#include "InfoLogger/InfoLogger.hxx"
#include <boost/format.hpp>

using namespace AliceO2::InfoLogger;

int main()
{
  InfoLogger theLog;

  theLog.log("infoLogger message test");
  theLog.log(InfoLogger::Severity::Info,"infoLogger INFO message test");
  theLog.log(InfoLogger::Severity::Warning,"infoLogger WARNING message test");
  theLog.log(InfoLogger::Severity::Error,"infoLogger ERROR message test");
  theLog.log(InfoLogger::Severity::Fatal,"infoLogger FATAL message test");
  theLog.log(InfoLogger::Severity::Debug,"infoLogger DEBUG message test");
        
  theLog << "another test message " << InfoLogger::endm;
  theLog << InfoLogger::Severity::Error << "another (stream error) message " << InfoLogger::endm;
  theLog << InfoLogger::Severity::Warning << "another (stream warning) message " << InfoLogger::endm;
  theLog << "yet another test message " << "(in " << 2 << " parts)" << InfoLogger::endm;
  theLog << "message with formatting: " << boost::format("%+5d %.3f") % 12345 % 5.4321 << InfoLogger::endm;

  return 0;
}
