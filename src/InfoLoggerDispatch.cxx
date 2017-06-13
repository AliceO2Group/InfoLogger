#include "InfoLoggerDispatch.h"


///////////////////////////////////////////
// class InfoLoggerDispatch implementation
///////////////////////////////////////////


InfoLoggerDispatch::InfoLoggerDispatch(ConfigInfoLoggerServer *vConfig, SimpleLog *vLog) {
  dispatchThread=std::make_unique<Thread>(InfoLoggerDispatch::threadCallback,this);
  input=std::make_unique<AliceO2::Common::Fifo<std::shared_ptr<InfoLoggerMessageList>>>(1000);
  dispatchThread->start();  
  if (vLog!=NULL) {
    theLog=vLog;
  } else {
    theLog=&defaultLog;
  }
  theConfig=vConfig;
}

InfoLoggerDispatch::~InfoLoggerDispatch() {
  dispatchThread->stop();
}

int InfoLoggerDispatch::pushMessage(const std::shared_ptr<InfoLoggerMessageList> &msg) {
  if (input->isFull()) {
    return -1;
  }
  input->push(msg);
  //theLog->info("push message\n");
  return 0;
}

int InfoLoggerDispatch::customLoop() {  
  return 0;
}

Thread::CallbackResult InfoLoggerDispatch::threadCallback(void *arg) {
  InfoLoggerDispatch *dPtr=(InfoLoggerDispatch *)arg;
  if (dPtr==NULL) {
    return Thread::CallbackResult::Error;
  }
  
  int nMsgProcessed=0;
  
  if (dPtr->customLoop()) {
    return Thread::CallbackResult::Error;
  }
  
  while (!dPtr->input->isEmpty()) {
    std::shared_ptr<InfoLoggerMessageList> nextMessage=nullptr;
    dPtr->input->pop(nextMessage);
    dPtr->customMessageProcess(nextMessage);
    nMsgProcessed++;
  }

  if (nMsgProcessed==0) {
    return Thread::CallbackResult::Idle;
  }
  return Thread::CallbackResult::Ok;
}

int InfoLoggerDispatchPrint::customMessageProcess(std::shared_ptr<InfoLoggerMessageList> msg) {
  infoLog_msg_print(msg->msg);
  return 0;
}


/////////////////////////////////////////////////
// end of class InfoLoggerDispatch implementation
/////////////////////////////////////////////////
