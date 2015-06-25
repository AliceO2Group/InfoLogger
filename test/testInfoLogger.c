/// \file testInfoLogger.c
/// \brief Example usage of the C InfoLogger logging interface.
///
/// \author Sylvain Chapeland, CERN

#include <InfoLogger.h>
#include <stdio.h>

int main()
{
  InfoLoggerHandle logH;

  int err;
  err = infoLoggerOpen(&logH);
  printf("infoLoggerOpen() = %d\n", err);
  err = infoLoggerLog(logH, "infoLogger message test");
  printf("infoLoggerLog() = %d\n", err);
  err = infoLoggerClose(logH);
  printf("infoLoggerClose() = %d\n", err);

  return 0;
}
