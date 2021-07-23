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

/// \file testInfoLoggerPerf.cxx
/// \brief infoLogger message generator for perf benchmarks
///
/// \author Sylvain Chapeland, CERN

#include <InfoLogger/InfoLogger.hxx>
#include <Common/Timer.h>

using namespace AliceO2::InfoLogger;
using namespace AliceO2::Common;

int main(int argc, char* argv[])
{
  InfoLogger theLog;
  Timer theTimer;

  int maxMsgCount = 1000; // number of message to send
  int maxMsgSize = 100;   // max size of message to send
  int sizeIsRandom = 0;   // flag set to get a random message size (up to maxMsgSize)
  int noOutput = 0;       // when set, messages are created but not sent
  int delay = 0;          // delay in microseconds between messages
  
  // parse command line parameters
  int option;
  while ((option = getopt(argc, argv, "c:s:rnd:")) != -1) {
    switch (option) {
      case 'c':
        maxMsgCount = atoi(optarg);
        break;
      case 's':
        maxMsgSize = atoi(optarg);
        break;
      case 'r':
        sizeIsRandom = 1;
        break;
      case 'n':
        noOutput = 1;
        break;
      case 'd':
        delay = atoi(optarg);
	break;
    }
  }

  printf("Generating log messages: maxMsgCount=%d maxMsgSize=%d sizeIsRandom=%d delay=%d\n", maxMsgCount, maxMsgSize, sizeIsRandom, delay);

  char* msgBuffer = (char*)malloc(maxMsgSize + 1);
  if (msgBuffer == nullptr)
    return -1;

  char varBuf[100];
  snprintf(varBuf, sizeof(varBuf), "test message %09d ", 0);
  int fixSz = strlen(varBuf);
  for (int i = fixSz; i < maxMsgSize; i++) {
    msgBuffer[i] = ' ' + (i - fixSz) % ('~' - ' ');
  }

  theTimer.reset();
  // generate log messages
  for (int i = 1; i <= maxMsgCount; i++) {
    int sz = maxMsgSize;
    if (sizeIsRandom) {
      sz = maxMsgSize * (rand() * 1.0 / RAND_MAX);
    }

    snprintf(varBuf, sizeof(varBuf), "test message %09d ", i);
    memcpy(msgBuffer, varBuf, fixSz);

    char cBak = msgBuffer[sz];
    msgBuffer[sz] = 0;
    if (!noOutput) {
      theLog.log("%s", msgBuffer);
    }
    msgBuffer[sz] = cBak;
    if (delay > 0) {
      usleep(delay);
    }
  }
  double t = theTimer.getTime();
  printf("Done in %lf seconds\n", t);
  printf("%.2lf msg/s\n", maxMsgCount / t);
  if (msgBuffer != nullptr) {
    free(msgBuffer);
  }
  
  return 0;
}

