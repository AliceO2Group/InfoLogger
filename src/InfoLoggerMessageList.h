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

