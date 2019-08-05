// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

#define INFOLOGGER_DEFAULT_CONFIG_PATH "/etc/infoLogger.cfg"
#define INFOLOGGER_ENV_CONFIG_PATH "INFOLOGGER_CONFIG"

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
