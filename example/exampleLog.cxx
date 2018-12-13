#include <InfoLogger/InfoLogger.hxx>
using namespace AliceO2::InfoLogger;
int main()
{
  InfoLogger theLog;

  theLog.log("infoLogger message test");
  return 0;
}
