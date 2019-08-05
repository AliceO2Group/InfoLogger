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
}

//////////////////////////////////////////////////////
// end of class ConfigInfoLoggerClient
//////////////////////////////////////////////////////

/////////////////////////////////////////////////////////
// class InfoLoggerClient
// handles connection from a process to local infoLoggerD
/////////////////////////////////////////////////////////

InfoLoggerClient::InfoLoggerClient()
{
  txSocket = -1;

  isInitialized = 0;
  log.setLogFile(cfg.logFile.c_str());

  try {

    // create receiving socket for incoming messages
    log.info("Creating transmission socket named %s", cfg.txSocketPath.c_str());
    txSocket = socket(PF_LOCAL, SOCK_STREAM, 0);
    if (txSocket == -1) {
      log.error("Could not create socket: %s", strerror(errno));
      throw __LINE__;
    }

    // configure socket mode
    int socketMode = fcntl(txSocket, F_GETFL);
    if (socketMode == -1) {
      log.error("fcntl(F_GETFL) failed: %s", strerror(errno));
      throw __LINE__;
    }
    //socketMode=(socketMode | O_NONBLOCK); // non-blocking
    if (fcntl(txSocket, F_SETFL, socketMode) == -1) {
      log.error("fcntl(F_SETFL) failed: %s", strerror(errno));
      throw __LINE__;
    }

    // configure socket TX buffer size
    if (cfg.txSocketOutBufferSize != -1) {
      int txBufSize = cfg.txSocketOutBufferSize;
      socklen_t optLen = sizeof(txBufSize);
      if (setsockopt(txSocket, SOL_SOCKET, SO_RCVBUF, &txBufSize, optLen) == -1) {
        log.error("setsockopt() failed: %s", strerror(errno));
        throw __LINE__;
      }
      if (getsockopt(txSocket, SOL_SOCKET, SO_RCVBUF, &txBufSize, &optLen) == -1) {
        log.error("getsockopt() failed: %s", strerror(errno));
        throw __LINE__;
      }
      txBufSize /= 2; // SO_SNDBUF Linux doubles the value requested.
      if (txBufSize != cfg.txSocketOutBufferSize) {
        log.warning("Could not set desired buffer size: got %d bytes instead of %d", txBufSize, cfg.txSocketOutBufferSize);
        throw __LINE__;
      }
    }

    // connect socket
    struct sockaddr_un socketAddrr;
    bzero(&socketAddrr, sizeof(socketAddrr));
    socketAddrr.sun_family = PF_LOCAL;
    if (cfg.txSocketPath.length() + 2 > sizeof(socketAddrr.sun_path)) {
      log.error("Socket name too long: max allowed is %d", (int)sizeof(socketAddrr.sun_path) - 2);
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
    if (connect(txSocket, (struct sockaddr*)&socketAddrr, sizeof(socketAddrr))) {
      log.error("Failed to connect to infoLoggerD: %s", strerror(errno));
      throw __LINE__;
    }

    isInitialized = 1;
    log.info("Connected to infoLoggerD");
  }

  catch (int errLine) {
    log.error("infoLoggerClient failed to initialize - error %d", errLine);
  }
}

int InfoLoggerClient::isOk()
{
  return isInitialized;
}

InfoLoggerClient::~InfoLoggerClient()
{
  if (txSocket >= 0) {
    log.info("Closing transmission socket");
    close(txSocket);
  }
}

int InfoLoggerClient::send(const char* message, unsigned int messageSize)
{
  if (txSocket >= 0) {
    int bytesWritten = write(txSocket, message, messageSize);
    if (bytesWritten == (int)messageSize) {
      return 0;
    }
  }
  return -1;
}

/////////////////////////////////////////////////////////
// end of class InfoLoggerClient
/////////////////////////////////////////////////////////
