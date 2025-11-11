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

#include "ConfigInfoLoggerServer.h"

ConfigInfoLoggerServer::ConfigInfoLoggerServer()
{
}

ConfigInfoLoggerServer::~ConfigInfoLoggerServer()
{
}

void ConfigInfoLoggerServer::readFromConfigFile(ConfigFile& config)
{
  config.getOptionalValue<int>(INFOLOGGER_CONFIG_SECTION_NAME_SERVER ".serverPortRx", serverPortRx);
  config.getOptionalValue<int>(INFOLOGGER_CONFIG_SECTION_NAME_SERVER ".maxClientsRx", maxClientsRx);
  config.getOptionalValue<int>(INFOLOGGER_CONFIG_SECTION_NAME_SERVER ".msgQueueLengthRx", msgQueueLengthRx);
  config.getOptionalValue<std::string>(INFOLOGGER_CONFIG_SECTION_NAME_SERVER ".msgDumpFile", msgDumpFile);
      
  config.getOptionalValue<std::string>(INFOLOGGER_CONFIG_SECTION_NAME_SERVER ".dbHost", dbHost);
  config.getOptionalValue<std::string>(INFOLOGGER_CONFIG_SECTION_NAME_SERVER ".dbUser", dbUser);
  config.getOptionalValue<std::string>(INFOLOGGER_CONFIG_SECTION_NAME_SERVER ".dbPassword", dbPassword);
  config.getOptionalValue<std::string>(INFOLOGGER_CONFIG_SECTION_NAME_SERVER ".dbName", dbName);
  config.getOptionalValue<int>(INFOLOGGER_CONFIG_SECTION_NAME_SERVER ".dbEnabled", dbEnabled);
  config.getOptionalValue<int>(INFOLOGGER_CONFIG_SECTION_NAME_SERVER ".dbNThreads", dbNThreads);
  config.getOptionalValue<int>(INFOLOGGER_CONFIG_SECTION_NAME_SERVER ".dbDispatchQueueSize", dbDispatchQueueSize);

  config.getOptionalValue<std::string>(INFOLOGGER_CONFIG_SECTION_NAME_SERVER ".dbReplica", dbReplica);

  config.getOptionalValue<int>(INFOLOGGER_CONFIG_SECTION_NAME_SERVER ".serverPortTx", serverPortTx);
  config.getOptionalValue<int>(INFOLOGGER_CONFIG_SECTION_NAME_SERVER ".maxClientsTx", maxClientsTx);
  
  config.getOptionalValue<int>(INFOLOGGER_CONFIG_SECTION_NAME_SERVER ".statsEnabled", statsEnabled);
  config.getOptionalValue<int>(INFOLOGGER_CONFIG_SECTION_NAME_SERVER ".statsPort", statsPort);
  config.getOptionalValue<int>(INFOLOGGER_CONFIG_SECTION_NAME_SERVER ".statsMaxClients", statsMaxClients);
  config.getOptionalValue<int>(INFOLOGGER_CONFIG_SECTION_NAME_SERVER ".statsPublishInterval", statsPublishInterval);
  config.getOptionalValue<int>(INFOLOGGER_CONFIG_SECTION_NAME_SERVER ".statsResetInterval", statsResetInterval);
  config.getOptionalValue<int>(INFOLOGGER_CONFIG_SECTION_NAME_SERVER ".statsHistory", statsHistory);
  
}

