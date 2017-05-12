#include "InfoLoggerMessageList.h"
#include "infoLoggerMessageDecode.h"

InfoLoggerMessageList::InfoLoggerMessageList(TR_file *rawMessageList) {
  msg=infoLog_decode(rawMessageList);
  if (msg==nullptr) {
    throw __LINE__;
  }
}

InfoLoggerMessageList::~InfoLoggerMessageList() {
  infoLog_msg_destroy(msg); // destroy list (ok with null)
}
