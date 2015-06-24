#include <InfoLogger.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>

typedef struct
{
} InfoLoggerHandleData;

int infoLoggerOpen(InfoLoggerHandle *handle)
{
  if (handle == NULL) {
    return __LINE__;
  }
  return 0;
}

int infoLoggerClose(InfoLoggerHandle handle)
{
  InfoLoggerHandleData *handleData;
  handleData = (InfoLoggerHandleData *) handle;
  if (handleData == NULL) {
    return __LINE__;
  }
  return 0;
}

int infoLoggerLog(InfoLoggerHandle handle, const char *message, ...)
{
  InfoLoggerHandleData *handleData;
  handleData = (InfoLoggerHandleData *) handle;
  if (handleData == NULL) {
    return __LINE__;
  }

  char buffer[1024];
  int len;

  time_t now;
  struct tm tm_str;
  now = time(NULL);
  localtime_r(&now, &tm_str);
  len = strftime(buffer, sizeof(buffer), "%Y-%m-%d %T\t", &tm_str);

  va_list ap;
  va_start(ap, message);
  vsnprintf(&buffer[len], sizeof(buffer) - len, message, ap);
  va_end(ap);

  printf("%s\n", buffer);

  return 0;
}
