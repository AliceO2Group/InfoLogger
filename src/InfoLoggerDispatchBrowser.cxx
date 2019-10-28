// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

#include "InfoLoggerDispatch.h"

#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "ConfigInfoLoggerServer.h"

////////////////////////////////////////////////////////
// class InfoLoggerDispatchOnlineBrowser implementation
////////////////////////////////////////////////////////

// size of a general purpose buffer
#define DISPATCH_BUFFER_SIZE 200

class InfoLoggerDispatchOnlineBrowserImpl
{
 public:
  int listen_sock = -1;     // listening socket
  std::vector<int> clients; // connected clients
};

InfoLoggerDispatchOnlineBrowser::InfoLoggerDispatchOnlineBrowser(ConfigInfoLoggerServer* config, SimpleLog* log) : InfoLoggerDispatch(config, log)
{
  dPtr = std::make_unique<InfoLoggerDispatchOnlineBrowserImpl>();

  dPtr->clients.resize(theConfig->maxClientsTx);
  for (int i = 0; i < theConfig->maxClientsTx; i++) {
    dPtr->clients[i] = -1;
  }

  // initialize listening socket
  // create a socket
  if ((dPtr->listen_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    throw __LINE__;
    //"socket - error %d",errno
  }

  int opts; // to set socket options

  // make sure re-bind possible without TIME_WAIT problems
  opts = 1;
  setsockopt(dPtr->listen_sock, SOL_SOCKET, SO_REUSEADDR, &opts, sizeof(opts));
  // use keep alive messages
  opts = 1;
  setsockopt(dPtr->listen_sock, SOL_SOCKET, SO_KEEPALIVE, &opts, sizeof(opts));

  // define server address / port
  struct sockaddr_in srv_addr;
  bzero((char*)&srv_addr, sizeof(srv_addr));
  srv_addr.sin_family = AF_INET;
  srv_addr.sin_addr.s_addr = INADDR_ANY;
  srv_addr.sin_port = htons(theConfig->serverPortTx); // todo: how to configure through?

  // bind socket
  if (bind(dPtr->listen_sock, (struct sockaddr*)&srv_addr, sizeof(srv_addr)) < 0) {
    theLog->error("bind port %d - error %d = %s", theConfig->serverPortTx, errno, strerror(errno));
    close(dPtr->listen_sock);
    throw __LINE__;
  }

  // queue length for incoming connections
  if (listen(dPtr->listen_sock, theConfig->maxClientsTx) < 0) {
    theLog->error("listen - error %d = %s", errno, strerror(errno));
    close(dPtr->listen_sock);
    throw __LINE__;
  }
  //theLog.info("%s() success\n",__FUNCTION__);
  
  // enable customloop callback
  isReady = true;
}
InfoLoggerDispatchOnlineBrowser::~InfoLoggerDispatchOnlineBrowser()
{
  // close sockets
  if (dPtr->listen_sock >= 0) {
    close(dPtr->listen_sock);
  }
  for (int i = 0; i < theConfig->maxClientsTx; i++) {
    if (dPtr->clients[i] != -1) {
      close(dPtr->clients[i]);
    }
  }
}
int InfoLoggerDispatchOnlineBrowser::customMessageProcess(std::shared_ptr<InfoLoggerMessageList> msg)
{

#define MAX_MSG_LENGTH 32768
  char onlineMsg[MAX_MSG_LENGTH];
  infoLog_msg_t* lmsg;
  int size_m;          /* message size */
  fd_set select_write; /* List of sockets to select */
  int highest_sock;
  struct timeval tv;
  int result;
  //theLog->info("dispatching a message\n");

  for (lmsg = msg->msg; lmsg != NULL; lmsg = lmsg->next) {
    if (infoLog_msg_encode(lmsg, onlineMsg, MAX_MSG_LENGTH, -1) == 0) {
      size_m = strlen(onlineMsg);
      for (int i = 0; i < theConfig->maxClientsTx; i++) {
        if (dPtr->clients[i] == -1)
          continue;

        //theLog->info("dispatch msg to client %d\n%s\n",i,onlineMsg);

        /* check that the client is writable */
        FD_ZERO(&select_write);
        FD_SET(dPtr->clients[i], &select_write);
        highest_sock = dPtr->clients[i];
        /* define timeout for message dispatch possible */
        tv.tv_sec = 3;
        tv.tv_usec = 0;
        result = select(highest_sock + 1, NULL, &select_write, NULL, &tv);
        if (result <= 0) {
          theLog->info("Message dispatch timeout, connection %d closed", i + 1);
          close(dPtr->clients[i]);
          dPtr->clients[i] = -1;
          continue;
        }

        int writeok = 0;
        result = write(dPtr->clients[i], onlineMsg, size_m);
        if (result == size_m) {
          /* no need to add \n, included in message */
          writeok = 1;
        }
        if (!writeok) {
          theLog->info("Write failed - connection %d closed", i + 1);
          close(dPtr->clients[i]);
          dPtr->clients[i] = -1;
        }
      }
    }
  }

  return 0;
}

// todo: socket recv thread c++

int InfoLoggerDispatchOnlineBrowser::customLoop()
{

  char buffer[DISPATCH_BUFFER_SIZE]; /* generic purpose buffer */

  int i; /* counter */

  fd_set select_read; /* List of sockets to select */
  int highest_sock;
  struct timeval tv;
  int result;
  int new_cl_sock;                /* socket address for new client */
  struct sockaddr_in new_cl_addr; /* address of new client */
  socklen_t cl_addr_len;          /* address length */

  // create a 'select' list, with listening socket (we don't care what clients send) */
  FD_ZERO(&select_read);
  // listening socket
  FD_SET(dPtr->listen_sock, &select_read);
  highest_sock = dPtr->listen_sock;
  // add the connected clients
  for (i = 0; i < theConfig->maxClientsTx; i++) {
    if (dPtr->clients[i] != -1) {
      FD_SET(dPtr->clients[i], &select_read);
      if (dPtr->clients[i] > highest_sock) {
        highest_sock = dPtr->clients[i];
      }
    }
  }

  // select returns immediately
  tv.tv_sec = 0;
  tv.tv_usec = 0;
  result = select(highest_sock + 1, &select_read, NULL, NULL, &tv);

  if (result < 0) {
    // an error occurred
    theLog->info("select - error %d = %s", errno, strerror(errno));
  } else if (result > 0) {

    // read from clients
    for (i = 0; i < theConfig->maxClientsTx; i++) {
      if (dPtr->clients[i] != -1) {
        if (FD_ISSET(dPtr->clients[i], &select_read)) {
          result = read(dPtr->clients[i], buffer, DISPATCH_BUFFER_SIZE);
          if (result > 0) {
            /* success */
            /* we don't use the input */
            buffer[result] = 0;
            //theLog->info("connected client: init buffer: %s\n",buffer);
          }
          /* error reading socket ? */
          if (result < 0) {
            theLog->info("read - error %d", errno);
            result = 0;
          }
          /* close connection on EOF / error */
          if (result == 0) {
            theLog->info("Connection %d closed", i + 1);
            close(dPtr->clients[i]);
            dPtr->clients[i] = -1;
          }
        }
      }
    }

    /* new connection ? */
    if (FD_ISSET(dPtr->listen_sock, &select_read)) {
      cl_addr_len = sizeof(new_cl_addr);
      new_cl_sock = accept(dPtr->listen_sock, (struct sockaddr*)&new_cl_addr, &cl_addr_len);
      if (new_cl_sock < 0) {
        theLog->info("accept - error %d", errno);
      } else {
        theLog->info("%s connected on port %d", inet_ntoa(new_cl_addr.sin_addr), new_cl_addr.sin_port);
        for (i = 0; i < theConfig->maxClientsTx; i++) {
          if (dPtr->clients[i] == -1)
            break;
        }
        if (i == theConfig->maxClientsTx) {
          theLog->info("Too many online (e.g. infoBrowser) connections, max=%d - closing", theConfig->maxClientsTx);
          close(new_cl_sock);
        } else {
          /* Non blocking connection */
          int opts;
          opts = fcntl(new_cl_sock, F_GETFL);
          if (opts == -1) {
            theLog->error("fcntl - F_GETFL");
            close(new_cl_sock);
          } else {
            opts = (opts | O_NONBLOCK);
            if (fcntl(new_cl_sock, F_SETFL, opts) == -1) {
              theLog->error("fcntl - F_SETFL");
              close(new_cl_sock);
            } else {
              theLog->info("Assigned connection %d", i + 1);
              dPtr->clients[i] = new_cl_sock;
            }
          }
        }
      }
    }
  }

  return 0;
}

//////////////////////////////////////////////////////////////
// end of class InfoLoggerDispatchOnlineBrowser implementation
////////////////......////////////////////////////////////////
