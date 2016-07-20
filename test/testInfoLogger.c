/// \file testInfoLogger.c
/// \brief Example usage of the C InfoLogger logging interface.
///
/// \author Sylvain Chapeland, CERN

#include "InfoLogger/InfoLogger.h"

#define _BSD_SOURCE 

#include <stdio.h>
#include <unistd.h>

int main()
{
  InfoLoggerHandle logH;

  int err;
  err = infoLoggerOpen(&logH);
  printf("infoLoggerOpen() = %d\n", err);
  for (int i=0;i<3;i++) {
    err = infoLoggerLog(logH, "infoLogger message test");
    usleep(100000);
  }
  printf("infoLoggerLog() = %d\n", err);
  err = infoLoggerClose(logH);
  printf("infoLoggerClose() = %d\n", err);

  return 0;
}
