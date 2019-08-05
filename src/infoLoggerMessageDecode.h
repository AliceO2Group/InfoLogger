// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

#ifndef _INFOLOGGER_MESSAGE_DECODE_H
#define _INFOLOGGER_MESSAGE_DECODE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "transport_server.h"
#include "infoLoggerMessage.h"

infoLog_msg_t* infoLog_decode(TR_file* f);

#ifdef __cplusplus
}
#endif

// _INFOLOGGER_MESSAGE_DECODE_H
#endif
