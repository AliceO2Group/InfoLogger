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

#define INFOLOGGER_DEFAULT_CONFIG_PATH "/etc/o2.d/infologger/infoLogger.cfg"
#define INFOLOGGER_ENV_CONFIG_PATH "O2_INFOLOGGER_CONFIG"
#define INFOLOGGER_ENV_OPTIONS "O2_INFOLOGGER_OPTIONS"
#define INFOLOGGER_ENV_MODE "O2_INFOLOGGER_MODE"

// name of the section where to find parameters
#define INFOLOGGER_CONFIG_SECTION_NAME_ADMIN "admin"
#define INFOLOGGER_CONFIG_SECTION_NAME_SERVER "infoLoggerServer"
#define INFOLOGGER_CONFIG_SECTION_NAME_INFOLOGGERD "infoLoggerD"
#define INFOLOGGER_CONFIG_SECTION_NAME_CLIENT "client"

// Infologger DB: name of the main message table
#define INFOLOGGER_TABLE_MESSAGES "messages"

// default infoLoggerServer listening port for infoLoggerD
#define INFOLOGGER_DEFAULT_SERVER_RX_PORT 6006

// default infoLoggerServer listening port for infoBrowser
#define INFOLOGGER_DEFAULT_SERVER_TX_PORT 6102

// default listening socket name for infoLoggerD
#define INFOLOGGER_DEFAULT_LOCAL_SOCKET "infoLoggerD"

