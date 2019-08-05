// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

#include <Common/SimpleLog.h>

// class to communicate with local infoLoggerD process

class ConfigInfoLoggerClient
{
 public:
  ConfigInfoLoggerClient(const char* configPath = nullptr);
  ~ConfigInfoLoggerClient();

  void resetConfig(); // set default configuration parameters

  std::string txSocketPath; // name of socket used to receive log messages from clients
  int txSocketOutBufferSize;
  std::string logFile; // log file for internal library logs
};

class InfoLoggerClient
{
 public:
  InfoLoggerClient();
  ~InfoLoggerClient();

  // status on infoLoggerD connection
  // returns 1 if client ok, 0 if not
  int isOk();

  // sends (already encoded) message to infoLoggerD
  // returns 0 on success, an error code otherwise
  int send(const char* message, unsigned int messageSize);

 private:
  ConfigInfoLoggerClient cfg;

  int isInitialized; // set to 1 when object initialized with success, 0 otherwise
  SimpleLog log;     // object for daemon logging, as defined in config
  int txSocket;      // socket to infoLoggerD. >=0 if set.
};
