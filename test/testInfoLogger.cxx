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

  theLog << "another test message " << InfoLogger::endm;
  theLog << "yet another test message " << "(in " << 2 << " parts)" << InfoLogger::endm;
  theLog << "message with formatting: " << boost::format("%+5d %.3f") % 12345 % 5.4321 << InfoLogger::endm;

  return 0;
}
