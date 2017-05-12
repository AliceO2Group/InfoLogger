#ifndef _INFOLOGGER_MESSAGE_DECODE_H
#define _INFOLOGGER_MESSAGE_DECODE_H

#ifdef __cplusplus
extern "C" {
#endif


#include "transport_server.h"
#include "infoLoggerMessage.h"

infoLog_msg_t * infoLog_decode(TR_file *f);

#ifdef __cplusplus
}
#endif


// _INFOLOGGER_MESSAGE_DECODE_H
#endif
