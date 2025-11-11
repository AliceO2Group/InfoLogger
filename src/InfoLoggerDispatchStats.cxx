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

#include "InfoLoggerDispatch.h"

#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <unordered_map>
#include <string>
#include <cstdint>
#include <thread>
#include <chrono>
#include <map>

#include "ConfigInfoLoggerServer.h"
#include "infoLoggerMessage.h"

using namespace std::chrono;

////////////////////////////////////////////////////////
// class InfoLoggerDispatchStats implementation
////////////////////////////////////////////////////////

using CountMap = std::unordered_map<std::string, uint64_t>;

// for each infologger field, keep a map counting occurence of each value
// std::vector index is the field index from infoLog_msgProtocol_t
// this is tracked for a given timespan
struct FieldStats {
  uint64_t timeBegin;
  uint64_t timeEnd;
  std::vector<CountMap> fieldCounts;
  uint64_t totalMessages = 0;
};

// structure to store index/count of values for each log field for a given time window
struct Window {
    uint64_t timeBegin;   // inclusive
    uint64_t timeEnd;     // exclusive
    std::vector<CountMap> fieldCounts;
    size_t totalMessages = 0;
};


// size of a general purpose buffer
#define DISPATCH_BUFFER_SIZE 200

class InfoLoggerDispatchStatsImpl
{
 public:
  int listen_sock = -1;     // listening socket
  std::vector<int> clients; // connected clients
  
  int publishInterval = 5; // interval of publication time
  uint64_t lastTimePublished = 0; // unixtime of last publish
  int resetInterval = 30; // interval of statistics tracking, new one created periodically
  uint64_t lastTimeReset = 0; // unixtime of last reset;
  uint64_t maxHistory = 600; // keep stats for this amount of time (seconds)

  // stats
  std::map<uint64_t, FieldStats> windows; // we index with the window begin timestamp
  FieldStats *currentWindow = nullptr; // the one currently in use, where we will likely insert new values
  std::vector<std::pair<std::string, std::vector<int>>> ilgFieldsToIndex; // the list of infologger fields (or combination of them) to be indexed (only for protocol[0]). First = name of the index. Second = list of field indices to be combined.
};

InfoLoggerDispatchStats::InfoLoggerDispatchStats(ConfigInfoLoggerServer* config, SimpleLog* log) : InfoLoggerDispatch(config, log)
{
  dPtr = std::make_unique<InfoLoggerDispatchStatsImpl>();

  auto addCombination = [&](std::vector<std::string> ixs) {
    std::vector<int> lix;
    std::string nix;
    for (const auto &name : ixs) {
      int i = infoLog_msg_findField(name.c_str());
      if (i<0) {
        return; // invalid index name
      }
      lix.push_back(i);
      if (lix.size()>1) nix += "-";
      nix += name;
    }
    dPtr->ilgFieldsToIndex.push_back(std::pair(std::move(nix), std::move(lix)));
    return;
  };

  // define what we want to index in ilgFieldsToIndex
  addCombination({"severity"});
  addCombination({"level"});
  addCombination({"hostname"});
  addCombination({"rolename"});
  addCombination({"hostname","pid"});
  addCombination({"system"});
  addCombination({"facility"});
  addCombination({"detector"});
  addCombination({"partition"});
  addCombination({"run"});
  addCombination({"run","detector","severity","level"});
  addCombination({"run","hostname","severity","level"});
  addCombination({"run","hostname","facility"});
  addCombination({"errcode"});
  addCombination({"errsource","errline"});
  addCombination({"hostname","pid","errsource","errline"});

  dPtr->clients.resize(theConfig->statsMaxClients);
  for (int i = 0; i < theConfig->statsMaxClients; i++) {
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
  srv_addr.sin_port = htons(theConfig->statsPort); // todo: how to configure through?


  // bind socket
  if (bind(dPtr->listen_sock, (struct sockaddr*)&srv_addr, sizeof(srv_addr)) < 0) {
    theLog->error("bind port %d - error %d = %s", theConfig->statsPort, errno, strerror(errno));
    close(dPtr->listen_sock);
    throw __LINE__;
  }

  // queue length for incoming connections
  if (listen(dPtr->listen_sock, theConfig->statsMaxClients) < 0) {
    theLog->error("listen - error %d = %s", errno, strerror(errno));
    close(dPtr->listen_sock);
    throw __LINE__;
  }
  //theLog.info("%s() success\n",__FUNCTION__);
  theLog->info("Publishing stats on port %d - every %ds, window size %ds, history %ds", theConfig->statsPort, theConfig->statsPublishInterval, theConfig->statsResetInterval, theConfig->statsHistory);
  
  dPtr->publishInterval = theConfig->statsPublishInterval;
  dPtr->resetInterval = theConfig->statsResetInterval;
  dPtr->maxHistory = theConfig->statsHistory;  

  // enable customloop callback
  isReady = true;
}
InfoLoggerDispatchStats::~InfoLoggerDispatchStats()
{
  // close sockets
  if (dPtr->listen_sock >= 0) {
    close(dPtr->listen_sock);
  }
  for (int i = 0; i < theConfig->statsMaxClients; i++) {
    if (dPtr->clients[i] != -1) {
      close(dPtr->clients[i]);
    }
  }
}
int InfoLoggerDispatchStats::customMessageProcess(std::shared_ptr<InfoLoggerMessageList> msg)
{

#define MAX_MSG_LENGTH 32768
  char onlineMsg[MAX_MSG_LENGTH];
  infoLog_msg_t* lmsg;
  int size_m;          /* message size */

  //theLog->info("dispatching a message\n");

  for (lmsg = msg->msg; lmsg != NULL; lmsg = lmsg->next) {
    if (infoLog_msg_encode(lmsg, onlineMsg, MAX_MSG_LENGTH, -1) == 0) {
        //infoLog_msg_print(lmsg);     
  	//printf( "protocol=%s\n", lmsg->protocol->version);

        // filter out some messages ? (eg debug)
	// ...
	// TBD
	
	// don't even care about message original timestamp, we just insert in current time window (ie using message reception time here)
	if (dPtr->currentWindow != nullptr) {
            dPtr->currentWindow->totalMessages++;
	    
	    auto getStringValue = [](infoLog_msg_t *m, int i) -> std::string {
	      if (m->values[i].isUndefined) {
	          return "";
	      }
	      switch (m->protocol->fields[i].type) {
		case infoLog_msgField_def_t::ILOG_TYPE_STRING:
		  return m->values[i].value.vString;
		  break;
		case infoLog_msgField_def_t::ILOG_TYPE_INT:
		  return std::to_string(m->values[i].value.vInt);
		  break;
		case infoLog_msgField_def_t::ILOG_TYPE_DOUBLE:		    
		  return std::to_string(m->values[i].value.vDouble);
		  break;
		default:
		  break;
	      }
	      return "";
	    };
	    
	    constexpr bool debug = 0;
	    
	    for(int id = 0; id < (int)dPtr->ilgFieldsToIndex.size(); id++) {
	    
              bool incomplete = 0;
	      std::string v;
	           
	      if (debug) {printf("indexing %s\n",dPtr->ilgFieldsToIndex[id].first.c_str());}
	      
	      for (int ii = 0 ; ii < (int)dPtr->ilgFieldsToIndex[id].second.size(); ii++) {
	        if (debug) {printf("  get [%d]\n",  dPtr->ilgFieldsToIndex[id].second[ii]);}
	        if (ii) {
		  v += "-";
		}
		std::string fv = getStringValue(lmsg, dPtr->ilgFieldsToIndex[id].second[ii]);
		if (debug) {printf("    = %s\n",fv.c_str());}
		if (fv == "") {
	          incomplete = 1;
		  break;
	        }
		v += fv;
	      }
	      
	      if (!incomplete) {
	        if (debug) {printf("  -> %s\n",v.c_str());}
	        dPtr->currentWindow->fieldCounts[id][v]++;
	      }
	    }
    
/*	    
	    for (int i = 0; i < lmsg->protocol->numberOfFields - 1; i++) {
	      // skip message field
//	       printf("%s = ", m->protocol->fields[i].name);
	      if (lmsg->values[i].isUndefined) {
	        continue;
	      } else {
		v="";
		switch (lmsg->protocol->fields[i].type) {
        	  case infoLog_msgField_def_t::ILOG_TYPE_STRING:
        	    v=lmsg->values[i].value.vString;
        	    break;
        	  case infoLog_msgField_def_t::ILOG_TYPE_INT:
        	    v=std::to_string(lmsg->values[i].value.vInt);
        	    break;
        	  case infoLog_msgField_def_t::ILOG_TYPE_DOUBLE:		    
		    v=std::to_string(lmsg->values[i].value.vDouble);
        	    break;
        	  default:
        	    break;
		}
		if (v!="") {
		  dPtr->currentWindow->fieldCounts[i][v]++;
		}
	      }
	   }
	   */
	}
    }
  }

  return 0;
}

// todo: socket recv thread c++

int InfoLoggerDispatchStats::customLoop()
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
  
  fd_set select_write; /* List of sockets to select */
  
  
  // create a 'select' list, with listening socket (we don't care what clients send) */
  FD_ZERO(&select_read);
  // listening socket
  FD_SET(dPtr->listen_sock, &select_read);
  highest_sock = dPtr->listen_sock;
  // add the connected clients
  for (i = 0; i < theConfig->statsMaxClients; i++) {
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
    for (i = 0; i < theConfig->statsMaxClients; i++) {
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
            theLog->info("Connection stats-%d closed", i + 1);
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
        for (i = 0; i < theConfig->statsMaxClients; i++) {
          if (dPtr->clients[i] == -1)
            break;
        }
        if (i == theConfig->statsMaxClients) {
          theLog->info("Too many online stats connections, max=%d - closing", theConfig->statsMaxClients);
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
              theLog->info("Assigned connection stats-%d", i + 1);
              dPtr->clients[i] = new_cl_sock;
            }
          }
        }
      }
    }
  }
    
  //auto now = std::chrono::steady_clock::now();
  auto now = duration_cast<seconds>(system_clock::now().time_since_epoch()).count(); 
  if (now - dPtr->lastTimePublished > dPtr->publishInterval) {
    dPtr->lastTimePublished = now;
    
    //printf("publish\n");
    
    
    // create a TCL list style output for statistics
    auto toTclList = [&] (const FieldStats& w) -> std::string {
	std::ostringstream out;
	
	out << "{timeBegin " << w.timeBegin
            << " timeEnd " << w.timeEnd
            << " totalMessages " << w.totalMessages
            << " fieldCounts {";

	for (int i=0; i < (int)w.fieldCounts.size(); i++) {
            if (w.fieldCounts[i].size()>0) {
              //out << protocols[0].fields[i].name << " {";
	      out << dPtr->ilgFieldsToIndex[i].first << " {";
	      bool isFirst = 1;
              for (const auto& [key, val] : w.fieldCounts[i]) {
        	  
		  if (isFirst) {
		    isFirst = 0;
		  } else {
                    out << " ";
		  }
		  out << key << " " << val;
              }
              out << "} ";
	    }
	}

	out << "}}"; // close fieldCounts and outer list

	return out.str();
    };
    
    std::string txt;
    for (const auto& [timestamp, window] : dPtr->windows) {
        //txt += "{ " + std::to_string(timestamp) +  " " + toTclList(window) + " }";
	txt += toTclList(window);
    }
    txt += "\n";
    
    //printf("%s",txt.c_str());

    // to test RX:  socat - TCP:127.0.0.1:6103
    
    // publish stats
      for (int i = 0; i < theConfig->statsMaxClients; i++) {
        if (dPtr->clients[i] == -1)
          continue;

        // check that the client is writable
        FD_ZERO(&select_write);
        FD_SET(dPtr->clients[i], &select_write);
        highest_sock = dPtr->clients[i];
        // define timeout for message dispatch possible
        tv.tv_sec = 3;
        tv.tv_usec = 0;
        result = select(highest_sock + 1, NULL, &select_write, NULL, &tv);
        if (result <= 0) {
          theLog->info("Dispatch timeout, connection stats-%d closed", i + 1);
          close(dPtr->clients[i]);
          dPtr->clients[i] = -1;
          continue;
        }

        int writeok = 0;
	int size_m = (int)txt.length();
        result = write(dPtr->clients[i], txt.c_str(), size_m);
        if (result == size_m) {
          // no need to add \n, included in message
          writeok = 1;
        }
        if (!writeok) {
          theLog->info("Write failed - connection stats-%d closed", i + 1);
          close(dPtr->clients[i]);
          dPtr->clients[i] = -1;
        }
      }
  }
  
  // create new window when needed
  if ((now - dPtr->lastTimeReset >= dPtr->resetInterval)||(dPtr->currentWindow == nullptr)) {
    dPtr->lastTimeReset = now;
    // printf("reset\n");
    // if first reset, just align to first minute system clock

    // close previous time window
    if (dPtr->currentWindow) {
      dPtr->currentWindow->timeEnd = now;
    }

    // add a new time window
    FieldStats w;
    w.timeBegin = now;
    w.timeEnd = w.timeBegin + dPtr->resetInterval;
    w.totalMessages = 0;
    w.fieldCounts.resize(dPtr->ilgFieldsToIndex.size());

    auto it = dPtr->windows.emplace(now, std::move(w)).first;
    dPtr->currentWindow = &it->second;

    // remove old windows
    for (auto it = dPtr->windows.begin(); it != dPtr->windows.end(); ) {
	if (it->second.timeEnd < now - dPtr->maxHistory) {
            it = dPtr->windows.erase(it);  // erase() returns the next iterator
	} else {
            ++it;
	}
    }
  }

  return 0;
}

//////////////////////////////////////////////////////////////
// end of class InfoLoggerDispatchStats implementation
////////////////......////////////////////////////////////////

