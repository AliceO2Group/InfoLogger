// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

/// \file InfoLoggerDispatch.h
/// \brief Definition of a base class used to distribute messages received by the infoLoggerServer to consumer threads (DB storage, clients, ...)
/// \author Sylvain Chapeland (sylvain.chapeland@cern.ch)

#ifndef _INFOLOGGER_DISPATCH_H
#define _INFOLOGGER_DISPATCH_H

#include "InfoLoggerMessageList.h"
#include <Common/Fifo.h>
#include <Common/Thread.h>
#include <Common/SimpleLog.h>
#include <Common/Configuration.h>
#include <memory>

#include "ConfigInfoLoggerServer.h"

using namespace AliceO2::Common;

class InfoLoggerDispatch
{

 public:
  InfoLoggerDispatch(ConfigInfoLoggerServer* theConfig = NULL, SimpleLog* theLog = NULL);
  virtual ~InfoLoggerDispatch();

  // todo: define settings: non-blocking, etc

  // returns 0 on success, -1 on error (e.g. queue full)
  int pushMessage(const std::shared_ptr<InfoLoggerMessageList>& msg);

  static Thread::CallbackResult threadCallback(void* arg);

  virtual int customMessageProcess(std::shared_ptr<InfoLoggerMessageList> msg) = 0;
  virtual int customLoop();

  //    virtual customDispatch...

 protected:
  std::unique_ptr<AliceO2::Common::Fifo<std::shared_ptr<InfoLoggerMessageList>>> input;
  std::unique_ptr<AliceO2::Common::Thread> dispatchThread;
  SimpleLog* theLog;
  SimpleLog defaultLog;

  ConfigInfoLoggerServer* theConfig;

  bool isReady = false; // this flag must be set to true by derived instances when ready.
                        // customloop will not be called unless set.
};

class InfoLoggerDispatchPrint : public InfoLoggerDispatch
{
  int customMessageProcess(std::shared_ptr<InfoLoggerMessageList> msg);
};

// a class to dispatch online messages to infoBrowser
class InfoLoggerDispatchOnlineBrowserImpl;
class InfoLoggerDispatchOnlineBrowser : public InfoLoggerDispatch
{
 public:
  InfoLoggerDispatchOnlineBrowser(ConfigInfoLoggerServer* theConfig, SimpleLog* theLog);
  ~InfoLoggerDispatchOnlineBrowser();
  int customMessageProcess(std::shared_ptr<InfoLoggerMessageList> msg);
  int customLoop();

 private:
  std::unique_ptr<InfoLoggerDispatchOnlineBrowserImpl> dPtr;
};

// a class to dispatch online messages to SQL database
class InfoLoggerDispatchSQLImpl;
class InfoLoggerDispatchSQL : public InfoLoggerDispatch
{
 public:
  InfoLoggerDispatchSQL(ConfigInfoLoggerServer* theConfig, SimpleLog* theLog);
  ~InfoLoggerDispatchSQL();
  int customMessageProcess(std::shared_ptr<InfoLoggerMessageList> msg);
  int customLoop();

 private:
  std::unique_ptr<InfoLoggerDispatchSQLImpl> dPtr;
};

#endif
