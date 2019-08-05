// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

#ifndef _INFOLOGGER_MESSAGE_LIST_H
#define _INFOLOGGER_MESSAGE_LIST_H

#include "transport_files.h"
#include "infoLoggerMessage.h"

class InfoLoggerMessageList
{
 public:
  InfoLoggerMessageList(TR_file* rawMessageList);
  ~InfoLoggerMessageList();
  infoLog_msg_t* msg;
};

// _INFOLOGGER_MESSAGE_LIST_H
#endif
