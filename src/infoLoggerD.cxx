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

/// \file infoLoggerD.cxx
/// \brief infoLoggerD process
///
/// This is the implementation for infoLoggerD, the local daemon collecting log messages
/// (replacement for old "infoLoggerReader")
//  It acts as a bridge between local clients and server, and provides persistent buffer capabilities.
///
/// \author Sylvain Chapeland, CERN

#include <string>

#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <time.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/poll.h>

#include <sys/stat.h>
#include <string.h>

#include <signal.h>
#include <limits.h>

#include <list>
#include <filesystem>
#include <sys/resource.h>

#include <Common/Daemon.h>
#include <Common/SimpleLog.h>
#include "transport_client.h"

#include "simplelog.h"
#include "infoLoggerDefaults.h"

//////////////////////////////////////////////////////
// class ConfigInfoLoggerD
// stores configuration params for infologgerD
//////////////////////////////////////////////////////

class ConfigInfoLoggerD
{
 public:
  ConfigInfoLoggerD();
  ~ConfigInfoLoggerD();

  void readFromConfigFile(ConfigFile& cfg); // load settings from ConfigFile object

  // assign default values to all config parameters

  // settings for local incoming messages
  std::string localLogDirectory = "/tmp/infoLoggerD";         // local directory to be used for log storage, whenever necessary
  std::string rxSocketPath = INFOLOGGER_DEFAULT_LOCAL_SOCKET; // name of socket used to receive log messages from clients
  int rxSocketInBufferSize = -1;                              // size of socket receiving buffer. -1 will leave to sys default.
  int rxMaxConnections = 2048;                                // maximum number of incoming connections

  // settings for remote infoLoggerServer access
  std::string serverHost = "localhost";                // IP name to connect infoLoggerServer
  int serverPort = INFOLOGGER_DEFAULT_SERVER_RX_PORT;  // IP port number to connect infoLoggerServer
  int msgQueueLength = 10000;                           // transmission queue size
  std::string msgQueuePath = localLogDirectory + "/infoLoggerD.queue"; // path to temp file storing messages
  int msgQueueReset = 0;                                               // when set, existing temp file is cleared (and pending messages lost)
  std::string clientName = "infoLoggerD";              // name identifying client to infoLoggerServer
  int isProxy = 0;                                     // flag set to allow infoLoggerD to be a transport proxy to infoLoggerServer

  // settings for output
  int outputToServer = 1; // enable output to infoLoggerServer
  int outputToLog = 0;    // enable output to log
};

ConfigInfoLoggerD::ConfigInfoLoggerD()
{
}

ConfigInfoLoggerD::~ConfigInfoLoggerD()
{
}

void ConfigInfoLoggerD::readFromConfigFile(ConfigFile& config)
{

  config.getOptionalValue<std::string>(INFOLOGGER_CONFIG_SECTION_NAME_INFOLOGGERD ".localLogDirectory", localLogDirectory);
  config.getOptionalValue<std::string>(INFOLOGGER_CONFIG_SECTION_NAME_INFOLOGGERD ".rxSocketPath", rxSocketPath);
  config.getOptionalValue<int>(INFOLOGGER_CONFIG_SECTION_NAME_INFOLOGGERD ".rxSocketInBufferSize", rxSocketInBufferSize);
  config.getOptionalValue<int>(INFOLOGGER_CONFIG_SECTION_NAME_INFOLOGGERD ".rxMaxConnections", rxMaxConnections);

  config.getOptionalValue<std::string>(INFOLOGGER_CONFIG_SECTION_NAME_INFOLOGGERD ".serverHost", serverHost);
  config.getOptionalValue<int>(INFOLOGGER_CONFIG_SECTION_NAME_INFOLOGGERD ".serverPort", serverPort);
  config.getOptionalValue<int>(INFOLOGGER_CONFIG_SECTION_NAME_INFOLOGGERD ".msgQueueLength", msgQueueLength);
  config.getOptionalValue<std::string>(INFOLOGGER_CONFIG_SECTION_NAME_INFOLOGGERD ".msgQueuePath", msgQueuePath);
  config.getOptionalValue<int>(INFOLOGGER_CONFIG_SECTION_NAME_INFOLOGGERD ".msgQueueReset", msgQueueReset);
  config.getOptionalValue<std::string>(INFOLOGGER_CONFIG_SECTION_NAME_INFOLOGGERD ".clientName", clientName);
  config.getOptionalValue<int>(INFOLOGGER_CONFIG_SECTION_NAME_INFOLOGGERD ".isProxy", isProxy);

  config.getOptionalValue<int>(INFOLOGGER_CONFIG_SECTION_NAME_INFOLOGGERD ".outputToServer", outputToServer);
  config.getOptionalValue<int>(INFOLOGGER_CONFIG_SECTION_NAME_INFOLOGGERD ".outputToLog", outputToLog);
}

//////////////////////////////////////////////////////
// end of class ConfigInfoLoggerD
//////////////////////////////////////////////////////

//////////////////////////////////////////////////////
// checkDirAndCreate()
//
// utility function to create a directory
// and parents if necessary (like mkdir -p)
//////////////////////////////////////////////////////

int checkDirAndCreate(const char* dir, SimpleLog* log = NULL)
{
  char path[PATH_MAX];
  struct stat info;

  if (lstat(dir, &info) == 0) {
    if (S_ISLNK(info.st_mode)) {
      char resname[PATH_MAX] = "";
      realpath(dir, resname);
      if (strlen(resname))
        return checkDirAndCreate(resname, log);
      return -1;
    }
  }

  if (stat(dir, &info) == 0) {
    if (S_ISDIR(info.st_mode)) {
      // already a directory
      return 0;
    }
    // other file type
    return -1;
  }

  // check directory tree
  unsigned int i, j;
  for (i = strlen(dir); i >= 0; i--) {
    if (dir[i] == '/') {
      if (i > 0) {
        if (dir[i - 1] == '/') {
          continue;
        }
      }
      break;
    }
  }
  if (i >= PATH_MAX) {
    return -1;
  }
  for (j = 0; j < i; j++) {
    path[j] = dir[j];
  }
  path[i] = 0;
  if (j > 1) {
    // create parent directory
    if (checkDirAndCreate(path, log)) {
      return -1;
    }
  }
  // then last dir entry
  if (mkdir(dir, 0777)) {
    if (log != NULL) {
      log->error("Failed to create directory %s : %s", dir, strerror(errno));
    }
    return -1;
  }
  if (log != NULL) {
    log->info("Created directory %s", dir);
  }
  return 0;
}

//////////////////////////////////////////////////////
// end of checkDirAndCreate()
//////////////////////////////////////////////////////

//////////////////////////////////////////////////////
// class InfoLoggerD
// implements infologgerD daemon process

typedef struct {
  int socket;
  std::string buffer; // currently pending data
  int pollIx; // index of client in poll structure
} t_clientConnection;

class InfoLoggerD : public Daemon
{
 public:
  InfoLoggerD(int argc, char* argv[]);
  ~InfoLoggerD();
  Daemon::LoopStatus doLoop();

 private:
  int isInitialized = 0;

  ConfigInfoLoggerD configInfoLoggerD; // object for configuration parameters
  int rxSocket = -1;                   // socket for incoming messages

  unsigned long long numberOfMessagesReceived = 0;
  std::list<t_clientConnection> clients;
  struct pollfd *fds = nullptr;  // array for poll()
  int nfds = 0;  // size of array

  TR_client_configuration cfgCx;  // config for transport
  TR_client_handle hCx = nullptr; // handle to server transport

  FILE* logOutput = nullptr; // handle to local log file where to copy incoming messages, if configured to do so

  bool stateAcceptFailed = 0; // flag to keep track accept() failing, and avoid flooding log output with errors in case of e.g. reaching max number of open files
};

// list of extra keys accepted on the command line (-o key=value entries)
#define EXTRA_OPTIONS \
  {                   \
    "msgQueueReset"   \
  }

InfoLoggerD::InfoLoggerD(int argc, char* argv[]) : Daemon(argc, argv, nullptr, EXTRA_OPTIONS)
{
  if (isOk()) { // proceed only if base daemon init was a success

    isInitialized = 0;

    try {

      numberOfMessagesReceived = 0;
      rxSocket = -1;

      // redirect legacy simplelog interface to SimpleLog
      setSimpleLog(&log);

      // retrieve configuration parameters from command line
      for (auto const& o : execOptions) {
        if (o.key == "msgQueueReset") {
          configInfoLoggerD.msgQueueReset = atoi(o.value.c_str());
        }
      }

      // retrieve configuration parameters from config file
      configInfoLoggerD.readFromConfigFile(config);

      // check directory for local storage
      log.info("Using directory %s for local storage", configInfoLoggerD.localLogDirectory.c_str());
      if (checkDirAndCreate(configInfoLoggerD.localLogDirectory.c_str(), &log)) {
        throw __LINE__;
      }
      // create receiving socket for incoming messages
      log.info("Creating receiving socket on %s", configInfoLoggerD.rxSocketPath.c_str());
      rxSocket = socket(PF_LOCAL, SOCK_STREAM, 0);
      if (rxSocket == -1) {
        log.error("Could not create receiving socket: %s", strerror(errno));
        throw __LINE__;
      }
      // configure socket mode
      int socketMode = fcntl(rxSocket, F_GETFL);
      if (socketMode == -1) {
        log.error("fcntl(F_GETFL) failed: %s", strerror(errno));
        throw __LINE__;
      }
      socketMode = (socketMode | SO_REUSEADDR | O_NONBLOCK); // quickly reusable address, non-blocking
      if (fcntl(rxSocket, F_SETFL, socketMode) == -1) {
        log.error("fcntl(F_SETFL) failed: %s", strerror(errno));
        throw __LINE__;
      }
      // configure socket RX buffer size
      if (configInfoLoggerD.rxSocketInBufferSize != -1) {
        int rxBufSize = configInfoLoggerD.rxSocketInBufferSize;
        socklen_t optLen = sizeof(rxBufSize);
        if (setsockopt(rxSocket, SOL_SOCKET, SO_RCVBUF, &rxBufSize, optLen) == -1) {
          log.error("setsockopt() failed: %s", strerror(errno));
          throw __LINE__;
        }
        if (getsockopt(rxSocket, SOL_SOCKET, SO_RCVBUF, &rxBufSize, &optLen) == -1) {
          log.error("getsockopt() failed: %s", strerror(errno));
          throw __LINE__;
        }
        if (rxBufSize != configInfoLoggerD.rxSocketInBufferSize) {
          log.warning("Could not set desired buffer size: got %d bytes instead of %d", rxBufSize, configInfoLoggerD.rxSocketInBufferSize);
        }
      }

      // connect receiving socket (for local clients)
      struct sockaddr_un socketAddress;
      bzero(&socketAddress, sizeof(socketAddress));
      socketAddress.sun_family = PF_LOCAL;
      if (configInfoLoggerD.rxSocketPath.length() + 2 > sizeof(socketAddress.sun_path)) {
        log.error("Socket name too long: max allowed is %d", (int)sizeof(socketAddress.sun_path) - 2);
        throw __LINE__;
      }
      // if name starts with '/', use normal socket name. if not, use an abstract socket name
      // this is to allow non-abstract sockets on systems not supporting them.
      if (configInfoLoggerD.rxSocketPath.c_str()[0] == '/') {
        strncpy(&socketAddress.sun_path[0], configInfoLoggerD.rxSocketPath.c_str(), configInfoLoggerD.rxSocketPath.length());
      } else {
        // leave first char 0, to get abstract socket name - see man 7 unix
        strncpy(&socketAddress.sun_path[1], configInfoLoggerD.rxSocketPath.c_str(), configInfoLoggerD.rxSocketPath.length());
      }
      if (bind(rxSocket, (struct sockaddr*)&socketAddress, sizeof(socketAddress)) == -1) {
        log.error("bind() failed: %s", strerror(errno));
        throw __LINE__;
      }
      // listen to socket
      if (listen(rxSocket, configInfoLoggerD.rxMaxConnections) == -1) {
        log.error("listen() failed: %s", strerror(errno));
        throw __LINE__;
      }

      if (configInfoLoggerD.outputToServer) {
        // create transport handle (to central server)
        cfgCx.server_name = configInfoLoggerD.serverHost.c_str();
        cfgCx.server_port = configInfoLoggerD.serverPort;
        cfgCx.queue_length = configInfoLoggerD.msgQueueLength;
        cfgCx.msg_queue_path = configInfoLoggerD.msgQueuePath.c_str();
        cfgCx.client_name = configInfoLoggerD.clientName.c_str();
        if (configInfoLoggerD.isProxy) {
          cfgCx.proxy_state = TR_PROXY_CAN_NOT_BE_PROXY;
        } else {
          cfgCx.proxy_state = TR_PROXY_CAN_BE_PROXY;
        }

        // check message queue
        struct stat queueInfo;
        std::string msgQueuePathFifo = configInfoLoggerD.msgQueuePath + ".fifo";
        log.info("Checking message queue file %s", msgQueuePathFifo.c_str());
        if (lstat(msgQueuePathFifo.c_str(), &queueInfo) == 0) {
          if (S_ISREG(queueInfo.st_mode)) {
            log.info("Pending messages = %ld bytes", (long)queueInfo.st_size);
          } else {
            log.error("This is not a regular file");
          }
        } else {
          log.info("No pending messages");
        }

        // if the queue.reset file exists, remove it and reset queue
        // (so it's done once only, on this startup)
        std::string msgQueuePathReset = configInfoLoggerD.msgQueuePath + ".reset";
        if (lstat(msgQueuePathReset.c_str(), &queueInfo) == 0) {
          if (S_ISREG(queueInfo.st_mode)) {
            log.info("Detected file %s, forcing queue reset", msgQueuePathReset.c_str());
            remove(msgQueuePathReset.c_str());
            configInfoLoggerD.msgQueueReset = 1;
          }
        }

        if (configInfoLoggerD.msgQueueReset) {
          log.info("Clearing queue");
          if (remove(msgQueuePathFifo.c_str())) {
            log.info("Failed to delete %s", msgQueuePathFifo.c_str());
          }
        }

        hCx = TR_client_start(&cfgCx);
        if (hCx == nullptr) {
          throw __LINE__;
        }
      } else {
        log.info("Output to infoLoggerServer disabled");
      }

      if (configInfoLoggerD.outputToLog) {
        log.info("Output to log file enabled");
        // todo: open a log file!
      }

      // check consistency of settings for max number of incoming connections
      if (1) {
        log.info("Checking resources for rxMaxConnections = %d", configInfoLoggerD.rxMaxConnections);
	long fileDescriptorCount = 0;
	long fileDescriptorMax = 0;
	struct rlimit rlim;
	bool limitOk = 0;
	try {
	  fileDescriptorCount = std::distance(std::filesystem::directory_iterator("/proc/self/fd"), std::filesystem::directory_iterator{});
	  log.info("At the moment, having %ld files opened", fileDescriptorCount);
	}
	catch(...) {
	}
	// iterate a couple of time to get/set rlimit if needed
	for (int i=0; i<2; i++) {
	  int err = getrlimit(RLIMIT_NOFILE, &rlim);
	  if (err) {
            log.error("getrlimit() failed: %s", strerror(errno));
	    break;
	  }
	  fileDescriptorMax = (long) rlim.rlim_cur;
	  log.info("getrlimit(): soft = %lu, hard = %lu", (unsigned long)rlim.rlim_cur, (unsigned long)rlim.rlim_max);
	  if (fileDescriptorCount + configInfoLoggerD.rxMaxConnections > (long)rlim.rlim_cur) {
            log.info("Current limits are not compatible with rxMaxConnections = %d", configInfoLoggerD.rxMaxConnections);

	    // trying to increase limits once
            if (i) break;
	    rlim.rlim_cur = (unsigned long)(fileDescriptorCount + configInfoLoggerD.rxMaxConnections + 1);
	    log.info("Trying to increase limit to %ld", (long)rlim.rlim_cur);
	    if (rlim.rlim_cur > rlim.rlim_max) {
	      rlim.rlim_cur = rlim.rlim_max;
	    }
	    err = setrlimit(RLIMIT_NOFILE, &rlim);
	    if (err) {
              log.error("setrlimit() failed: %s", strerror(errno));
	      break;
	    }
	  } else {
	    log.info("Limits ok, should be able to cope with %d max connections", configInfoLoggerD.rxMaxConnections);
	    limitOk = 1;
	    break;
	  }
	}
        if (!limitOk) {
          configInfoLoggerD.rxMaxConnections = fileDescriptorMax - fileDescriptorCount - 1;
	  if (configInfoLoggerD.rxMaxConnections <= 0) {
            configInfoLoggerD.rxMaxConnections = 1;
	  }
	  log.info("Reducing rxMaxConnections to %d", configInfoLoggerD.rxMaxConnections);
        }
      }

      isInitialized = 1;
      log.info("infoLoggerD started");
    }

    catch (int errLine) {
      log.error("InfoLoggerD can not start - error %d", errLine);
    }
  }
}

InfoLoggerD::~InfoLoggerD()
{
  if (isInitialized) {
    log.info("Received %llu messages", numberOfMessagesReceived);
  }

  if (rxSocket >= 0) {
    close(rxSocket);
  }
  for (auto c : clients) {
    close(c.socket);
  }
  if (fds) {
    free(fds);
  }
  if (hCx != nullptr) {
    TR_client_stop(hCx);
  }
}

Daemon::LoopStatus InfoLoggerD::doLoop()
{
  if (!isInitialized) {
    return LoopStatus::Error;
  }

  bool updateFds = false; // raise this flag to renew structure (change in client list)
  if (fds == nullptr)  {
    // create new poll structure based on current list of clients
    size_t sz = sizeof(struct pollfd) * (clients.size() + 1);
    fds = (struct pollfd *)malloc(sz);
    if (fds == nullptr) {
      return LoopStatus::Error;
    }
    memset(fds, 0, sz);
    nfds = 1;
    fds[0].fd = rxSocket;
    fds[0].events = POLLIN;

    for (auto &client : clients) {
      if (client.socket<0) continue;
      fds[nfds].fd = client.socket;
      fds[nfds].events = POLLIN;
      fds[nfds].revents = 0;
      client.pollIx = nfds; // keep index of fds for this client, to be able to check poll result
      nfds++;
    }
  }

  // poll with 1 second timeout
  if (poll(fds, nfds, 1000) > 0) {

    // check existing clients
    int cleanupNeeded = 0;
    for (auto& client : clients) {
      if (client.socket<0) continue;
      if ((client.pollIx >= 0) && (fds[client.pollIx].revents)) {
        char newData[1024000]; // never read more than 10k at a time... allows to round-robin through clients
        int bytesRead = read(client.socket, newData, sizeof(newData));
        if (bytesRead > 0) {
          int ix = 0;
          for (int i = 0; i < bytesRead; i++) {

            //break; // skip processing

            int nToPush = 0;
            int endOfLine = (newData[i] == '\n');
            int endOfBuffer = ((i + 1) == bytesRead);

            if (endOfLine) {
              nToPush = i - ix;
            } else if (endOfBuffer) {
              nToPush = i - ix + 1;
            }
            if (nToPush) {
              // push new data to buffer
              client.buffer.append(&newData[ix], nToPush);
              if (endOfLine) {
                //printf("received new message: %s\n",client.buffer.c_str());
                numberOfMessagesReceived++;
                //if (numberOfMessagesReceived%100000==0) {
                //  printf("received %lld messages\n",numberOfMessagesReceived);
                //}

                // todo: add mode "to file"

                if (configInfoLoggerD.outputToLog) {
                  if (logOutput == nullptr) {
                    printf("%s\n", client.buffer.c_str());
                  }
                }

                if (configInfoLoggerD.outputToServer) {
                  TR_client_send_msg(hCx, client.buffer.c_str());
                }
                client.buffer.clear();
              }

              ix = i + 1;
              // todo: add protection max size on client.buffer
            }
          }
        } else {
          int nDropped = client.buffer.length();
          if (nDropped) {
            log.info("partial data dropped:%s\n", client.buffer.c_str());
          }
          close(client.socket);
          client.socket = -1;
          cleanupNeeded++;
        }
      }
    }
    // cleanup list: remove items with invalid socket
    if (cleanupNeeded) {
      clients.remove_if([](t_clientConnection& c) { return (c.socket == -1); });
      log.info("%d clients disconnected, now having %d/%d", cleanupNeeded, (int)clients.size(), configInfoLoggerD.rxMaxConnections);
      updateFds = 1;
    }

    // handle new connection requests
    if (fds[0].revents) {
      struct sockaddr_un socketAddress;
      socklen_t socketAddressLen = sizeof(socketAddress);
      int tmpSocket = accept(rxSocket, (struct sockaddr*)&socketAddress, &socketAddressLen);
      if (tmpSocket == -1) {
        if (!stateAcceptFailed) {
          stateAcceptFailed = 1;
	  log.error("accept() failed: %s", strerror(errno));
	}
      } else {
        if (stateAcceptFailed) {
          log.info("accept() failure now recovered");
          stateAcceptFailed = 0;
	}
        if (((int)clients.size() >= configInfoLoggerD.rxMaxConnections) && (configInfoLoggerD.rxMaxConnections > 0)) {
          log.warning("Closing new client, maximum number of connections reached (%d)", configInfoLoggerD.rxMaxConnections);
          close(tmpSocket);
        } else {
          int socketMode = fcntl(rxSocket, F_GETFL);
          socketMode = (socketMode | O_NONBLOCK);
          fcntl(rxSocket, F_SETFL, socketMode);

          t_clientConnection newClient;
          newClient.socket = tmpSocket;
          newClient.buffer.clear();
          clients.push_back(newClient);
          log.info("New client: %d/%d", (int)clients.size(), configInfoLoggerD.rxMaxConnections);
	  updateFds = 1;
        }
      }
    }

    if (updateFds) {
      if (fds != nullptr) {
        free(fds);
      }
      fds = nullptr;
    }
  }

  return LoopStatus::Ok;
}

//////////////////////////////////////////////////////
// end of class InfoLoggerD
//////////////////////////////////////////////////////

//todo: lineBuffer struct:
// char array + isEmpty,truncated,etc

int main(int argc, char* argv[])
{
  InfoLoggerD d(argc, argv);
  return d.run();
}

