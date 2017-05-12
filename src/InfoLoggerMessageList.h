#ifndef _INFOLOGGER_MESSAGE_LIST_H
#define _INFOLOGGER_MESSAGE_LIST_H

#include "transport_files.h"
#include "infoLoggerMessage.h"


class InfoLoggerMessageList {
  public:
  InfoLoggerMessageList(TR_file *rawMessageList);
  ~InfoLoggerMessageList();
  infoLog_msg_t *msg;
};

// _INFOLOGGER_MESSAGE_LIST_H
#endif
