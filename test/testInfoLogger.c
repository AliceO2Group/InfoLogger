// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

/// \file testInfoLogger.c
/// \brief Example usage of the C InfoLogger logging interface.
///
/// \author Sylvain Chapeland, CERN

#include <InfoLogger/InfoLogger.h>

#define _BSD_SOURCE

#include <stdio.h>
#include <unistd.h>

int main()
{
  InfoLoggerHandle logH;

  int err;
  err = infoLoggerOpen(&logH);
  printf("infoLoggerOpen() = %d\n", err);
  for (int i = 0; i < 3; i++) {
    err = infoLoggerLog(logH, "infoLogger message test");
    usleep(100000);
  }
  printf("infoLoggerLog() = %d\n", err);
  err = infoLoggerClose(logH);
  printf("infoLoggerClose() = %d\n", err);

  return 0;
}
