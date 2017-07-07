#include <memory>
#include <Common/SimpleLog.h>
#include <Common/Daemon.h>
#include "transport_server.h"
#include "simplelog.h"
#include "infoLoggerMessageDecode.h"
#include "InfoLoggerMessageList.h"
#include "InfoLoggerDispatch.h"

#include "ConfigInfoLoggerServer.h"


/*

// todo:

decode_file -> return shared_ptr object


generic class for decoded message consuming
input : FIFO
running loop in separate thread
fifo mode: blocking, or drop if full ?

factory for: file rec, db rec, online-dispatch, proxy? filter? 

config: list of extra consumers? always enable online dispatch?
what ports?

class interface: get a pointer to configuration entry ("section")

tool to read TR_fifo files on disk and re-inject them.
or to translate local-written files to encoded messages (or display encoded files)

plugin for monitoring: check what's going on in infologgerserver
to avoid linking: socket for stats? external monitoring client? ...



#include "InfoLoggerDispatch.h"

class InfoLoggerRawMessage {
  InfoLoggerRawMessage(InfoLoggerRawMessage *file);
  ~InfoLoggerRawMessage();
  
  private:
    
}

// one thread per transport?
class InfoLoggerTransportServer {
  InfoLoggerTransportServer();
  ~InfoLoggerTransportServer();
  
}

simplelog: default log
*/





class InfoLoggerServer:public Daemon {
  public:
  InfoLoggerServer(int argc,char * argv[]);
  ~InfoLoggerServer();
  Daemon::LoopStatus doLoop();
  
  private:
  
  ConfigInfoLoggerServer configInfoLoggerServer;   // object for configuration parameters

  TR_server_configuration tcpServerConfig;  
  TR_server_handle tcpServerHandle=nullptr;
  
  int isInitialized;
  
  std::vector<std::unique_ptr<InfoLoggerDispatch>> dispatchEngines;

  std::vector<std::unique_ptr<InfoLoggerDispatch>> dispatchEnginesDB;
  unsigned int dbRoundRobinIx=0;
  
  unsigned long long msgCount=0;
};

InfoLoggerServer::InfoLoggerServer(int argc,char * argv[]):Daemon(argc,argv) {
  if (isOk()) { // proceed only if base daemon init was a success
    isInitialized=0;

    // redirect legacy simplelog interface to SimpleLog
    setSimpleLog(&log);

    // retrieve configuration parameters from config file
    configInfoLoggerServer.readFromConfigFile(config);


    try {
      tcpServerConfig.server_type  = TR_SERVER_TCP;                           // TCP server
      tcpServerConfig.server_port  = configInfoLoggerServer.serverPortRx;     // server port
      tcpServerConfig.max_clients  = configInfoLoggerServer.maxClientsRx;     // max clients
      tcpServerConfig.queue_length = configInfoLoggerServer.msgQueueLengthRx; // queue size  

      tcpServerHandle=TR_server_start(&tcpServerConfig);
      if (tcpServerHandle==NULL) { throw __LINE__; }
      
      // create dispatch engines
      try {
        //dispatchEngines.push_back(std::make_unique<InfoLoggerDispatchPrint>(&log));
        dispatchEngines.push_back(std::make_unique<InfoLoggerDispatchOnlineBrowser>(&configInfoLoggerServer,&log));

        if (configInfoLoggerServer.dbEnabled) {
          #ifdef _WITH_MYSQL
            log.info("SQL DB initialization");
            for (int i=0;i<configInfoLoggerServer.dbNThreads;i++) {
              dispatchEnginesDB.push_back(std::make_unique<InfoLoggerDispatchSQL>(&configInfoLoggerServer,&log));
            }
          #else
            log.error("Not built with MySQL support - can not enable DB");
          #endif
        } else {
          log.info("DB disabled");
        }
      } catch (int err) {
        printf("Failed to initialize dispatch engines: error %d\n",err);
      }
      
      isInitialized=1;
    }
    catch (int errLine) {
      log.error("Server can not start - error %d",errLine);
    }
  }
}
 

InfoLoggerServer::~InfoLoggerServer() {
  if (isOk()) { // proceed only if base daemon init was a success
    if (tcpServerHandle!=NULL) {
      TR_server_stop(tcpServerHandle);  
    }
    
    log.info("Received %llu messages",msgCount);
  }
}


Daemon::LoopStatus InfoLoggerServer::doLoop() {
  if (!isInitialized) {
    return LoopStatus::Error;
  }
  
  // read a file from transport (collection of messages, with a format depending on the transport used)
  TR_file *newFile;
  newFile=TR_server_get_file(tcpServerHandle, 0);

  if (newFile!=NULL) {  
    //fflush(stdout);
    //TR_file_dump(newFile);
    
    // decode raw message
    std::shared_ptr<InfoLoggerMessageList> msgList=nullptr;    
    try {
      msgList=std::make_shared<InfoLoggerMessageList>(newFile);
    } catch(...) {
      //printf("decoding failed\n");
    }
    
    if (msgList!=nullptr) {
      //printf("got message\n");
      
      // base dispatch engines
      for(const auto &dispatch : dispatchEngines){
        dispatch->pushMessage(msgList);
      }
      
      // DB dispatch engine ... find one available, or wait
      unsigned int nThreads=dispatchEnginesDB.size();
      unsigned int nTry=1;
      int pushOk=0;
      for (;nTry<=nThreads*3;nTry++) {
        int err=dispatchEnginesDB[dbRoundRobinIx]->pushMessage(msgList);
        dbRoundRobinIx++;
        if (dbRoundRobinIx>=nThreads) {
          dbRoundRobinIx=0;
        }
        if (err==0) {
          pushOk=1;
          break;
        }
        if (nTry%nThreads==0) {
          //log.warning("Warning, DB busy, waiting...");
          usleep(10000);
          // todo: keep newFile for next loop iteration, in order not to get stuck in sleep here
        }
      }
      if (!pushOk) {
        log.warning("Warning DB dispatch full, 1 message lost");
      }      
      

       // count messages
      for (infoLog_msg_t *m=msgList->msg;m!=nullptr;m=m->next) {
        msgCount++;
        /*
        if (msgCount%1000==0) {
          log.info("msg %llu",msgCount);
        }
        */
      }
      // todo: online analysis of message: set extra field (e.g. partition based on detector name and run, etc)      
      // dispatch message to online clients           
      // dispatch message to database, etc.
    }
    
  }

  if (newFile==NULL) {
    return LoopStatus::Idle;
  } else {
    TR_server_ack_file(tcpServerHandle,&newFile->id);
    TR_file_destroy(newFile);
    return LoopStatus::Ok;
  }
}


int main(int argc, char * argv[]) {
  InfoLoggerServer d(argc,argv);
  return d.run();
}
