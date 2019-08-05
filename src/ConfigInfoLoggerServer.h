// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

#ifndef SRC_CONFIGINFOLOGGERSERVER_H_
#define SRC_CONFIGINFOLOGGERSERVER_H_

#include "infoLoggerDefaults.h"
#include <Common/Configuration.h>

//////////////////////////////////////////////////////
// class ConfigInfoLoggerServer
// stores configuration params for infoLoggerServer
//////////////////////////////////////////////////////

class ConfigInfoLoggerServer
{
 public:
  ConfigInfoLoggerServer();
  ~ConfigInfoLoggerServer();

  void readFromConfigFile(ConfigFile& cfg); // load settings from ConfigFile object

  // assign default values to all config parameters

  // settings for incoming messages
  int serverPortRx = INFOLOGGER_DEFAULT_SERVER_RX_PORT; // IP port number to receive incoming messages, where infoLoggerD clients connect
  int maxClientsRx = 3000;                              // maximum number of connected infoLoggerD clients
  int msgQueueLengthRx = 10000;                         // reception queue size

  // settings for database connection
  std::string dbHost = "localhost";  // database host name
  std::string dbUser = "o2";         // database user name
  std::string dbPassword = "o2";     // database password
  std::string dbName = "INFOLOGGER"; // database name
  int dbEnabled = 1;                 // flag to enable/disable db
  int dbNThreads = 1;                // number of insert threads
  int dbDispatchQueueSize = 10000;   // max number of messages buffered in memory before DB insert

  // settings for infoBrowser clients
  int serverPortTx = INFOLOGGER_DEFAULT_SERVER_TX_PORT;
  int maxClientsTx = 100;
};

#endif // SRC_CONFIGINFOLOGGERSERVER_H_
