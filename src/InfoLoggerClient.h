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

#include <Common/SimpleLog.h>
#include <Common/Timer.h>
#include <queue>
#include <mutex>
#include <thread>

// class to communicate with local infoLoggerD process

class ConfigInfoLoggerClient
{
 public:
  ConfigInfoLoggerClient(const char* configPath = nullptr);
  ~ConfigInfoLoggerClient();

  void resetConfig(); // set default configuration parameters

  std::string txSocketPath; // name of socket used to receive log messages from clients
  int txSocketOutBufferSize; // buffer size of outgoing socket. -1 for system default.
  std::string logFile; // log file for internal library logs
  double reconnectTimeout; // retry timeout for infoLoggerD reconnect (seconds)
  int maxMessagesBuffered; // max messages in buffer when reconnect pending
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

  int connect();     // connect to infoLoggerD. returns 0 on success, or an error code.
  void disconnect();  // disconnect infoLoggerD.
  int isInitialized; // set to 1 when object initialized with success, 0 otherwise
  SimpleLog log;     // object for daemon logging, as defined in config
  int txSocket;      // socket to infoLoggerD. >=0 if set.
  
  AliceO2::Common::Timer reconnectTimer; // try to reconnect
  bool reconnectNeeded; // set when need to reconnect after timeout
  std::queue<std::string> messageBuffer; // pending messages
  std::mutex mutex; // lock for exclusive access to buffer
  std::unique_ptr<std::thread> reconnectThread; // thread trying to reconnect to infoLoggerD
  int reconnectAbort; // flag set to stop thread
  void reconnect(); // thread loop
  void reconnectThreadCleanup(); // cleanup thread resources
  void reconnectThreadStart(); // start reconnection thread
  bool isVerbose = true;
};

