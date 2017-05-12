#include <memory>
#include <Common/SimpleLog.h>
#include <Common/Daemon.h>
#include "transport_server.h"
#include "simplelog.h"
#include "infoLoggerMessageDecode.h"
#include "InfoLoggerMessageList.h"
#include "InfoLoggerDispatch.h"

#define DEFAULT_RX_PORT 6006

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
  
  TR_server_configuration tcpServerConfig;  
  TR_server_handle tcpServerHandle;
  
  int isInitialized;
  
  std::vector<std::unique_ptr<InfoLoggerDispatch>> dispatchEngines;
};

InfoLoggerServer::InfoLoggerServer(int argc,char * argv[]):Daemon(argc,argv) {
  if (isOk()) { // proceed only if base daemon init was a success
    isInitialized=0;
    tcpServerHandle=NULL;
    
    setSimpleLog(&log);

    try {
      tcpServerConfig.server_type  = TR_SERVER_TCP;     // TCP server
      tcpServerConfig.server_port  = DEFAULT_RX_PORT;   // server port
      tcpServerConfig.max_clients  = 3000;              // max clients
      tcpServerConfig.queue_length = 10000;             // queue size  

      try { tcpServerConfig.server_port=config.getValue<int>("port"); } catch (...) {}


      tcpServerHandle=TR_server_start(&tcpServerConfig);
      if (tcpServerHandle==NULL) { throw __LINE__; }
      
      // create dispatch engines
      try {
        //dispatchEngines.push_back(std::make_unique<InfoLoggerDispatchPrint>(&log));
        dispatchEngines.push_back(std::make_unique<InfoLoggerDispatchOnlineBrowser>(&log));      
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
      
      for(const auto &dispatch : dispatchEngines){
        dispatch->pushMessage(msgList);
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
