/// \file InfoLogger.c
/// \brief Implementation of the InfoLogger logging interface in C.
///
/// \author Sylvain Chapeland, CERN

#include <InfoLogger.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define InfoLoggerMagicNumber 0xABABAC00

typedef struct
{
  int magicTag;
  int numberOfMessages;
} InfoLoggerHandleData;

int infoLoggerOpen(InfoLoggerHandle *handle)
{
  if (handle == NULL) {
    return __LINE__;
  }
  *handle = NULL;
  InfoLoggerHandleData *handleData;
  handleData = (InfoLoggerHandleData *) malloc(sizeof(InfoLoggerHandleData));
  if (handleData == NULL) { return __LINE__; }
  handleData->magicTag = InfoLoggerMagicNumber;
  handleData->numberOfMessages = 0;
  *handle = handleData;
  return 0;
}

int infoLoggerClose(InfoLoggerHandle handle)
{
  InfoLoggerHandleData *handleData = (InfoLoggerHandleData *) handle;
  if (handleData == NULL) { return __LINE__; }
  if (handleData->magicTag != InfoLoggerMagicNumber) { return __LINE__; }
  handleData->magicTag = 0;
  free(handleData);
  return 0;
}

int infoLoggerLogV(InfoLoggerHandle handle, const char *message, va_list ap)
{
  InfoLoggerHandleData *handleData = (InfoLoggerHandleData *) handle;
  if (handleData == NULL) { return __LINE__; }
  if (handleData->magicTag != InfoLoggerMagicNumber) { return __LINE__; }

  char buffer[1024] = "";
  size_t len = 0;

  time_t now;
  struct tm tm_str;
  now = time(NULL);
  localtime_r(&now, &tm_str);
  len = strftime(buffer, sizeof(buffer), "%Y-%m-%d %T\t", &tm_str);

  vsnprintf(&buffer[len], sizeof(buffer) - len, message, ap);

  printf("%s\n", buffer);
  handleData->numberOfMessages++;
  return 0;
}


int infoLoggerLog(InfoLoggerHandle handle, const char *message, ...)
{
  int err = 0;

  va_list ap;
  va_start(ap, message);
  err = infoLoggerLogV(handle, message, ap);
  va_end(ap);

  return err;
}
