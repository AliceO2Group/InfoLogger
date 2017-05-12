/// \file InfoLoggerDispatch.h
/// \brief Definition of a base class used to distribute messages received by the infoLoggerServer to consumer threads (DB storage, clients, ...)
/// \author Sylvain Chapeland (sylvain.chapeland@cern.ch)


#ifndef _INFOLOGGER_DISPATCH_H
#define _INFOLOGGER_DISPATCH_H

#include "InfoLoggerMessageList.h"
#include <Common/Fifo.h>
#include <Common/Thread.h>
#include <Common/SimpleLog.h>
#include <memory>

using namespace AliceO2::Common;

class InfoLoggerDispatch {

  public:
    InfoLoggerDispatch(SimpleLog  *theLog=NULL);
    virtual ~InfoLoggerDispatch();
  
    // todo: define settings: non-blocking, etc
  
    // returns 0 on success, -1 on error (e.g. queue full)
    int pushMessage(const std::shared_ptr<InfoLoggerMessageList> &msg);
    
    static Thread::CallbackResult threadCallback(void *arg);  
    
    virtual int customMessageProcess(std::shared_ptr<InfoLoggerMessageList> msg)=0;
    virtual int customLoop();
    
    
//    virtual customDispatch...

  protected:
    std::unique_ptr<AliceO2::Common::Fifo<std::shared_ptr<InfoLoggerMessageList>>> input;
    std::unique_ptr<AliceO2::Common::Thread> dispatchThread;
    SimpleLog *theLog;
    SimpleLog defaultLog;
    
};


class InfoLoggerDispatchPrint: public InfoLoggerDispatch{
  int customMessageProcess(std::shared_ptr<InfoLoggerMessageList> msg);
};

class InfoLoggerDispatchOnlineBrowserImpl;

class InfoLoggerDispatchOnlineBrowser: public InfoLoggerDispatch{
  public:
  InfoLoggerDispatchOnlineBrowser(SimpleLog *theLog);
  ~InfoLoggerDispatchOnlineBrowser();
  int customMessageProcess(std::shared_ptr<InfoLoggerMessageList> msg);
  int customLoop();
  private:
    std::unique_ptr<InfoLoggerDispatchOnlineBrowserImpl> dPtr;
};
#endif
