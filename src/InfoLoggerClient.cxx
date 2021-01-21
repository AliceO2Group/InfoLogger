// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

#include <string>

#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <time.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <sys/stat.h>
#include <string.h>

#include <Common/SimpleLog.h>
#include <Common/Configuration.h>

#include "InfoLoggerClient.h"
#include "infoLoggerDefaults.h"

#ifndef MSG_NOSIGNAL
  #define MSG_NOSIGNAL 0
#endif
const int sendFlags = MSG_NOSIGNAL;

//////////////////////////////////////////////////////
// class ConfigInfoLoggerClient
// stores configuration params for infologgerD clients
//////////////////////////////////////////////////////

ConfigInfoLoggerClient::ConfigInfoLoggerClient(const char* configPath)
{
  // setup default configuration
  resetConfig();
  // load configuration, if defined
  if (configPath == nullptr) {
    // when not specified directly, check environment to get path to config file
    configPath = getenv(INFOLOGGER_ENV_CONFIG_PATH);
  }
  if (configPath != nullptr) {
    //printf("Using infoLogger configuration %s\n",configPath);
    // get configuration information from file
    ConfigFile config;
    try {
      config.load(configPath);
    } catch (std::string err) {
      printf("Error reading configuration file %s : %s", configPath, err.c_str());
      return;
    }
    config.getOptionalValue<std::string>(INFOLOGGER_CONFIG_SECTION_NAME_CLIENT ".txSocketPath", txSocketPath);
    config.getOptionalValue<int>(INFOLOGGER_CONFIG_SECTION_NAME_CLIENT ".txSocketOutBufferSize", txSocketOutBufferSize);
    config.getOptionalValue<std::string>(INFOLOGGER_CONFIG_SECTION_NAME_CLIENT ".logFile", logFile);
    config.getOptionalValue<double>(INFOLOGGER_CONFIG_SECTION_NAME_CLIENT ".reconnectTimeout", reconnectTimeout);
    config.getOptionalValue<int>(INFOLOGGER_CONFIG_SECTION_NAME_CLIENT ".maxMessagesBuffered", maxMessagesBuffered);
  }
}

ConfigInfoLoggerClient::~ConfigInfoLoggerClient()
{
}

void ConfigInfoLoggerClient::resetConfig()
{
  // assign default values to all config parameters
  txSocketPath = INFOLOGGER_DEFAULT_LOCAL_SOCKET;
  txSocketOutBufferSize = -1;
  logFile = "/dev/null";
  reconnectTimeout = 5.0;
  maxMessagesBuffered = 1000;
}

//////////////////////////////////////////////////////
// end of class ConfigInfoLoggerClient
//////////////////////////////////////////////////////

/////////////////////////////////////////////////////////
// class InfoLoggerClient
// handles connection from a process to local infoLoggerD
/////////////////////////////////////////////////////////
int InfoLoggerClient::connect() {
  txSocket = -1;
  isInitialized = 0;

  try {

    // create receiving socket for incoming messages
    if (isVerbose) log.info("Creating transmission socket named %s", cfg.txSocketPath.c_str());
    txSocket = socket(PF_LOCAL, SOCK_STREAM, 0);
    if (txSocket == -1) {
      if (isVerbose) log.error("Could not create socket: %s", strerror(errno));
      throw __LINE__;
    }

    /*
    // configure socket mode
    int socketMode = fcntl(txSocket, F_GETFL);
    if (socketMode == -1) {
      if (isVerbose) log.error("fcntl(F_GETFL) failed: %s", strerror(errno));
      throw __LINE__;
    }
    //socketMode=(socketMode | O_NONBLOCK); // non-blocking
    if (fcntl(txSocket, F_SETFL, socketMode) == -1) {
      if (isVerbose) log.error("fcntl(F_SETFL) failed: %s", strerror(errno));
      throw __LINE__;
    }
    */

    // configure socket to avoid SIGPIPE (when possible)
    #ifdef SO_NOSIGPIPE
    {
      int optValue = 1;
      socklen_t optLen = sizeof(optValue);
      if (setsockopt(txSocket, SOL_SOCKET, SO_NOSIGPIPE, &optValue, optLen) == -1) {
        if (isVerbose) log.error("setsockopt() failed: %s", strerror(errno));
        throw __LINE__;
      }
    }
    #endif
    
    
    // configure socket TX buffer size
    if (cfg.txSocketOutBufferSize != -1) {
      int txBufSize = cfg.txSocketOutBufferSize;
      socklen_t optLen = sizeof(txBufSize);
      if (setsockopt(txSocket, SOL_SOCKET, SO_RCVBUF, &txBufSize, optLen) == -1) {
        if (isVerbose) log.error("setsockopt() failed: %s", strerror(errno));
        throw __LINE__;
      }
      if (getsockopt(txSocket, SOL_SOCKET, SO_RCVBUF, &txBufSize, &optLen) == -1) {
        if (isVerbose) log.error("getsockopt() failed: %s", strerror(errno));
        throw __LINE__;
      }
      txBufSize /= 2; // SO_SNDBUF Linux doubles the value requested.
      if (txBufSize != cfg.txSocketOutBufferSize) {
        if (isVerbose) log.warning("Could not set desired buffer size: got %d bytes instead of %d", txBufSize, cfg.txSocketOutBufferSize);
        throw __LINE__;
      }
    }

    // connect socket
    struct sockaddr_un socketAddrr;
    bzero(&socketAddrr, sizeof(socketAddrr));
    socketAddrr.sun_family = PF_LOCAL;
    if (cfg.txSocketPath.length() + 2 > sizeof(socketAddrr.sun_path)) {
      if (isVerbose) log.error("Socket name too long: max allowed is %d", (int)sizeof(socketAddrr.sun_path) - 2);
      throw __LINE__;
    }
    // if name starts with '/', use normal socket name. if not, use an abstract socket name
    // this is to allow non-abstract sockets on systems not supporting them.
    if (cfg.txSocketPath.c_str()[0] == '/') {
      strncpy(&socketAddrr.sun_path[0], cfg.txSocketPath.c_str(), cfg.txSocketPath.length());
    } else {
      // leave first char 0, to get abstract socket name - see man 7 unix
      strncpy(&socketAddrr.sun_path[1], cfg.txSocketPath.c_str(), cfg.txSocketPath.length());
    }
    if (::connect(txSocket, (struct sockaddr*)&socketAddrr, sizeof(socketAddrr))) {
      if (isVerbose) log.error("Failed to connect to infoLoggerD: %s", strerror(errno));
      throw __LINE__;
    }

    isInitialized = 1;
    if (isVerbose) log.info("Connected to infoLoggerD");
  }

  catch (int errLine) {
    if (!isInitialized) {
      disconnect();
    }
    return errLine;
  }
  return 0;
}

void InfoLoggerClient::disconnect() {
  if (txSocket >= 0) {
    if (isVerbose) log.info("Closing transmission socket");
    close(txSocket);
  }
  txSocket = -1;
  isInitialized = 0;
}

InfoLoggerClient::InfoLoggerClient()
{
  log.setLogFile(cfg.logFile.c_str());
  reconnectThreadCleanup();
  int errLine = connect();
  // shutdown detail messages after first connect attempt
  isVerbose = 0;
  if (errLine) {
      log.error("infoLoggerClient failed to initialize - error %d", errLine);
      reconnectThreadStart();
  }
}

int InfoLoggerClient::isOk()
{
  return isInitialized;
}

InfoLoggerClient::~InfoLoggerClient()
{
  isVerbose = 1;
  reconnectThreadCleanup();
  disconnect();
  if (messageBuffer.size()) {
    log.error("Some messages still in buffer, %d will be lost",(int)messageBuffer.size());
  }
}

int InfoLoggerClient::send(const char* message, unsigned int messageSize)
{
  mutex.lock();
  if (txSocket >= 0) {
    // if there was a previous reconnection thread, it has now finished
    // otherwise there would not be a socket to write to
    // so, clean it up if needed
    reconnectThreadCleanup();


    int bytesWritten = ::send(txSocket, message, messageSize, sendFlags);
    if (bytesWritten == (int)messageSize) {
      mutex.unlock();
      return 0;
    }
    log.error("Failed to send message");
    // launch a thread for automatic reconnect, and buffer (reasonably) messages until then
    reconnectThreadStart();
  }
  if ((int)messageBuffer.size()<cfg.maxMessagesBuffered) {
    messageBuffer.push(message);
    if ((int)messageBuffer.size()==cfg.maxMessagesBuffered) {
      log.warning("Max buffer size reached, next messages will be lost until reconnect");
    }
  }
  mutex.unlock();
  return -1;
}

void InfoLoggerClient::reconnect() {
  for (;!reconnectAbort;) {    
    bool isOk = 0;
    if (reconnectTimer.isTimeout()) {
      if (mutex.try_lock()) {
       if (connect() == 0) {
          isOk = 1;
          log.info("Reconnection successful");
          int nFlushed = 0;
          while(!messageBuffer.empty()) {
            size_t messageSize = messageBuffer.front().size();
            int bytesWritten = ::send(txSocket, messageBuffer.front().c_str(), messageSize, sendFlags);
            if (bytesWritten == (int)messageSize) {
               messageBuffer.pop();
               nFlushed++;
            } else {
              log.info("Failed to flush buffer, will reconnect");
              disconnect();
              isOk = 0;
            }
          }
          if (nFlushed) {
            log.info("%d messages flushed from buffer", nFlushed);
          }
        }
        mutex.unlock();
        if (isOk) {  
          break;
        } else {
          if (isVerbose) log.info("reconnect failed");
        }
        reconnectTimer.reset(cfg.reconnectTimeout);
      }
    }
    usleep(200000);
  }
}

void InfoLoggerClient::reconnectThreadCleanup() {
  if (reconnectThread != nullptr) {
    reconnectAbort = 1;
    reconnectThread->join();
    reconnectThread = nullptr;
  }
}
void InfoLoggerClient::reconnectThreadStart() {
    disconnect();    
    reconnectAbort = 0;
    std::function<void(void)> loop = std::bind(&InfoLoggerClient::reconnect, this);
    log.info("Will try reconnecting at %.2lfs interval", cfg.reconnectTimeout);
    isVerbose = 0;
    reconnectTimer.reset(cfg.reconnectTimeout*1000000.0);
    reconnectThread = std::make_unique<std::thread>(loop);
}

/////////////////////////////////////////////////////////
// end of class InfoLoggerClient
/////////////////////////////////////////////////////////
