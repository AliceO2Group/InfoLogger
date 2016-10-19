/// \file infoLoggerD.cxx
/// \brief infoLoggerD process
///
/// This is the implementation for infoLoggerD, the local daemon collecting log messages
/// (replacement for old "infoLoggerReader")
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

#include <sys/stat.h>
#include <string.h>

#include <signal.h>
#include <limits.h>

#include <list>

#include <Common/Daemon.h>
#include <Common/SimpleLog.h>
#include "transport_client.h"

#include "simplelog.h"


//////////////////////////////////////////////////////
// class ConfigInfoLoggerD
// stores configuration params for infologgerD
//////////////////////////////////////////////////////

class ConfigInfoLoggerD {
  public:
  ConfigInfoLoggerD(const char*configPath=nullptr);
  ~ConfigInfoLoggerD();

  void resetConfig(); // set default configuration parameters


  std::string *userName;           // user name under which the process shall run
  std::string *logFile;            // local log file for infoLoggerD output
  std::string *localLogDirectory;  // local directory to be used for log storage, whenever necessary
  
  bool isInteractive;              // 1 if process should be run interactively, 0 to run as daemon
  
  std::string *rxSocketPath;       // name of socket used to receive log messages from clients
  int rxSocketInBufferSize;        // size of socket receiving buffer. -1 will leave to sys default.
  int rxMaxConnections;            // maximum number of incoming connections
};

ConfigInfoLoggerD::ConfigInfoLoggerD(const char*configPath) {
  // setup default configuration
  resetConfig();
  // load configuration, if defined
  if (configPath!=nullptr) {
  }
}

ConfigInfoLoggerD::~ConfigInfoLoggerD(){
  // release resources
 delete userName;
 delete logFile;
 delete localLogDirectory;
 delete rxSocketPath;
}

void ConfigInfoLoggerD::resetConfig() {
  // assign default values to all config parameters
  userName=nullptr;
  logFile=new std::string("/tmp/infoLoggerD.log");
  localLogDirectory=new std::string("/tmp/infoLoggerD");  
  isInteractive=1;
  rxSocketPath=new std::string("infoLoggerD");
  rxSocketInBufferSize=-1;
  rxMaxConnections=1024;
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


int checkDirAndCreate(const char *dir, SimpleLog *log=NULL) {
  char path[PATH_MAX];
  struct stat info;

  if (lstat(dir,&info)==0) {
    if (S_ISLNK(info.st_mode)) {
      char resname[PATH_MAX]="";
      realpath(dir,resname);
      if (strlen(resname)) return checkDirAndCreate(resname,log);
      return -1;
    }
  }

  
  if (stat(dir,&info)==0) {
    if (S_ISDIR(info.st_mode)) {
      // already a directory
      return 0;
    }
    // other file type
    return -1;
  }
  // check directory tree
  int i,j;
  for (i=strlen(dir);i>=0;i--) {
    if (dir[i]=='/') {
      if (i>0) {
        if (dir[i-1]=='/') {continue;}
      }
      break;
    }
  }
  for (j=0;j<i;j++) {
    path[j]=dir[j];
  }  
  path[i]=0;
  if (j>1) {
    // create parent directory
    if (checkDirAndCreate(path,log)) {return -1;}
  }
  // then last dir entry
  if (mkdir(dir,0777)) {
    if (log!=NULL) {
      log->error("Failed to create directory %s : %s",dir,strerror(errno));
    }
    return -1;
  }
  if (log!=NULL) {
    log->info("Created directory %s",dir);
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
} t_clientConnection;




class InfoLoggerD:public Daemon {
  public:
  InfoLoggerD();
  ~InfoLoggerD();
  Daemon::LoopStatus doLoop();
  
  private:
  
  int isInitialized;
  
  ConfigInfoLoggerD cfg;                 // object for configuration parameters
  SimpleLog log;                         // object for daemon logging, as defined in config

  int rxSocket;                          // socket for incoming messages
 
  unsigned long long numberOfMessagesReceived;
  std::list<t_clientConnection> clients;
  
  TR_client_configuration   cfgCx;  // config for transport
  TR_client_handle          hCx;    // handle to server transport
  
  
};



InfoLoggerD::InfoLoggerD() {

  isInitialized=0;
  
  try {
 
    numberOfMessagesReceived=0;
    rxSocket=-1; 
   
    // redirect legacy simplelog interface to SimpleLog
    setSimpleLog(&log);
   
    // check directory for local storage
    log.info("Using directory %s for local storage",cfg.localLogDirectory->c_str());
    if (checkDirAndCreate(cfg.localLogDirectory->c_str(),&log)) {
      throw __LINE__;
    }
    // create receiving socket for incoming messages
    log.info("Creating receiving socket on %s",cfg.rxSocketPath->c_str());
    rxSocket=socket(PF_LOCAL,SOCK_STREAM,0);
    if (rxSocket==-1) {
      log.error("Could not create receiving socket: %s",strerror(errno));
      throw __LINE__;
    }
    // configure socket mode
    int socketMode=fcntl(rxSocket,F_GETFL);
    if (socketMode==-1) {
      log.error("fcntl(F_GETFL) failed: %s",strerror(errno));
      throw __LINE__;
    }
    socketMode=(socketMode | SO_REUSEADDR | O_NONBLOCK); // quickly reusable address, non-blocking
    if (fcntl(rxSocket,F_SETFL,socketMode) == -1) {
      log.error("fcntl(F_SETFL) failed: %s",strerror(errno));
      throw __LINE__;
    }
    // configure socket RX buffer size
    if (cfg.rxSocketInBufferSize!=-1) {
      int rxBufSize=cfg.rxSocketInBufferSize;
      socklen_t optLen=sizeof(rxBufSize);
      if (setsockopt(rxSocket,SOL_SOCKET,SO_RCVBUF,&rxBufSize,optLen)==-1) {
        log.error("setsockopt() failed: %s",strerror(errno));
        throw __LINE__;
      }
      if (getsockopt(rxSocket,SOL_SOCKET,SO_RCVBUF,&rxBufSize,&optLen)==-1) {
        log.error("getsockopt() failed: %s",strerror(errno));
        throw __LINE__;
      }
      if (rxBufSize!=cfg.rxSocketInBufferSize) {
        log.warning("Could not set desired buffer size: got %d bytes instead of %d",rxBufSize,cfg.rxSocketInBufferSize);
      }
    }

    // connect receiving socket (for local clients)
    struct sockaddr_un socketAddress;
    bzero(&socketAddress, sizeof(socketAddress));      
    socketAddress.sun_family = PF_LOCAL;
    if (cfg.rxSocketPath->length()>sizeof(socketAddress.sun_path)) {
      log.error("Socket name too long: max allowed is %d",(int)sizeof(socketAddress.sun_path));
      throw __LINE__;
    }
    // leave first char 0, to get abstract socket name - see man 7 unix
    strncpy(&socketAddress.sun_path[1], cfg.rxSocketPath->c_str(), cfg.rxSocketPath->length());
    if (bind(rxSocket, (struct sockaddr *)&socketAddress, sizeof(socketAddress))==-1) {
      log.error("bind() failed: %s",strerror(errno));
      throw __LINE__;
    }
    // listen to socket
    if (listen(rxSocket,cfg.rxMaxConnections)==-1) {
       log.error("listen() failed: %s",strerror(errno));
       throw __LINE__;
    }
    
    
    // create transport handle (to central server)
    cfgCx.server_name    = "localhost";
    cfgCx.server_port    = 6006;
    cfgCx.queue_length   = 1000;
    cfgCx.client_name    = "infoLoggerD";
    cfgCx.proxy_state    = TR_PROXY_CAN_NOT_BE_PROXY;
    cfgCx.msg_queue_path = "/tmp/infoLoggerD.queue";

    hCx = TR_client_start(&cfgCx);
    if (hCx==NULL) {
      throw __LINE__;
    }

    
    
    isInitialized=1;
    log.info("Starting infoLoggerD");
  }
  
  catch (int errLine) {
    log.error("InfoLoggerD can not start - error %d",errLine);
  }
  

}


InfoLoggerD::~InfoLoggerD() {
  log.info("Received %lld messages",numberOfMessagesReceived);

  if (rxSocket>=0) {
    close(rxSocket);    
  }
  for (auto c : clients) {
    close(c.socket);
  }

  log.info("Exiting infoLoggerD");
}


Daemon::LoopStatus InfoLoggerD::doLoop() {
  if (!isInitialized) {
    return LoopStatus::Error;
  }

  fd_set  readfds;
  FD_ZERO(&readfds);
  FD_SET(rxSocket,&readfds);
  int nfds=rxSocket;
  for(auto client : clients) {
    FD_SET(client.socket,&readfds);
    if (client.socket>nfds) {nfds=client.socket;}
  }
  nfds++;

  struct timeval selectTimeout;
  selectTimeout.tv_sec=1;
  selectTimeout.tv_usec=0;    

  if (select(nfds,&readfds,NULL,NULL,&selectTimeout)>0) {

    // check existing clients
    int cleanupNeeded=0;
    for(auto &client : clients) {
      if (FD_ISSET(client.socket,&readfds)) {
        char newData[1024000]; // never read more than 10k at a time... allows to round-robin through clients
        int bytesRead=read(client.socket,newData,sizeof(newData));
        if (bytesRead>0) {
          int ix=0;
          for (int i=0;i<bytesRead;i++) {

          //break; // skip processing

            int nToPush=0;
            int endOfLine=(newData[i]=='\n');
            int endOfBuffer=((i+1)==bytesRead);

            if (endOfLine) {nToPush=i-ix;}
            else if (endOfBuffer) {nToPush=i-ix+1;}
            if (nToPush) {
              // push new data to buffer
              client.buffer.append(&newData[ix],nToPush);
              if (endOfLine) {
                printf("received new message: %s\n",client.buffer.c_str());
                numberOfMessagesReceived++;
                if (numberOfMessagesReceived%100000==0) {
                  printf("received %lld messages\n",numberOfMessagesReceived);
                }
                
                TR_client_send_msg(hCx, client.buffer.c_str());
                
                client.buffer.clear();
              }

              ix=i+1;                
              // todo: add protection max size on client.buffer
            }
          }              
        } else {
          int nDropped=client.buffer.length();
          if (nDropped) {
            printf("partial data dropped:%s\n",client.buffer.c_str());
          }
          close(client.socket);
          client.socket=-1;
          cleanupNeeded++;
        }
      }
    }
    // cleanup list: remove items with invalid socket
    if (cleanupNeeded) {
       clients.remove_if([](t_clientConnection &c){ return (c.socket==-1); });
       log.info("%d clients disconnected, now having %d/%d",cleanupNeeded,(int)clients.size(),cfg.rxMaxConnections);
    }

    // handle new connection requests
    if (FD_ISSET(rxSocket,&readfds)) {
      struct sockaddr_un socketAddress;
      socklen_t socketAddressLen=sizeof(socketAddress);
      int tmpSocket=accept(rxSocket,(struct sockaddr *)&socketAddress,&socketAddressLen);
      if(tmpSocket==-1) {
        log.error("accept() failed: %s",strerror(errno));
      } else {
        if (((int)clients.size()>=cfg.rxMaxConnections)&&(cfg.rxMaxConnections>0)) {
          log.warning("Closing new client, maximum number of connections reached (%d)", cfg.rxMaxConnections);
          close(tmpSocket);
        } else {
          int socketMode=fcntl(rxSocket,F_GETFL);
          socketMode=(socketMode | O_NONBLOCK);
          fcntl(rxSocket,F_SETFL,socketMode);

          t_clientConnection newClient;
          newClient.socket=tmpSocket;
          newClient.buffer.clear();
          clients.push_back(newClient);
          log.info("New client: %d/%d",(int)clients.size(),cfg.rxMaxConnections);
        }
      }
    }      

  } 
  
  return LoopStatus::Ok;  
}


//////////////////////////////////////////////////////
// end of class InfoLoggerD
//////////////////////////////////////////////////////


















//todo: lineBuffer struct:
// char array + isEmpty,truncated,etc




int main() {
  InfoLoggerD d;
  return d.run();
}
