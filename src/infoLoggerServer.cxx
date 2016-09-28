#include <Common/SimpleLog.h>
#include <Common/Daemon.h>
#include "transport_server.h"
#include "simplelog.h"

#define DEFAULT_RX_PORT 6006


class InfoLoggerServer:public Daemon {
  public:
  InfoLoggerServer(int argc,char * argv[]);
  ~InfoLoggerServer();
  Daemon::LoopStatus doLoop();
  
  private:
  
  TR_server_configuration tcpServerConfig;  
  TR_server_handle tcpServerHandle;
  
  int isInitialized;
};

InfoLoggerServer::InfoLoggerServer(int argc,char * argv[]):Daemon(argc,argv) {
  if (isOk()) { // proceed only if base daemon init was a success
    isInitialized=0;
    
    setSimpleLog(&log);

    try {
      tcpServerConfig.server_type  = TR_SERVER_TCP;     // TCP server
      tcpServerConfig.server_port  = DEFAULT_RX_PORT;   // server port
      tcpServerConfig.max_clients  = 3000;              // max clients
      tcpServerConfig.queue_length = 10000;             // queue size  

      try { tcpServerConfig.server_port=config.getValue<int>("port"); } catch (...) {}


      tcpServerHandle=TR_server_start(&tcpServerConfig);
      if (tcpServerHandle==NULL) { throw __LINE__; }
      isInitialized=1;
    }
    catch (int errLine) {
      log.error("Server can not start - error %d",errLine);
    }
  }
}
 

InfoLoggerServer::~InfoLoggerServer() {
  TR_server_stop(tcpServerHandle);  
}


Daemon::LoopStatus InfoLoggerServer::doLoop() {
  if (!isInitialized) {
    return LoopStatus::Error;
  }
  
  TR_file *newFile;
  newFile=TR_server_get_file(tcpServerHandle, 0);

  if (newFile!=NULL) {  
    printf("recv:%p\n",(void *)newFile);
    fflush(stdout);
    TR_file_dump(newFile);
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
