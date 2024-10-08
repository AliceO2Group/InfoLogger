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
#include <mysql.h>
#include <mysqld_error.h>
#include "utility.h"
#include "infoLoggerMessage.h"
#include <unistd.h>
#include <string.h>
#include <Common/Timer.h>

#if LIBMYSQL_VERSION_ID >= 80000
typedef bool my_bool;
#endif

// some constants
#define SQL_RETRY_CONNECT 1 // SQL database connect retry time

class InfoLoggerDispatchSQLImpl
{
 public:
  ConfigInfoLoggerServer* theConfig; // struct with config parameters
  InfoLoggerDispatchSQL* parent;

  void start();
  void stop();
  int customLoop();
  int customMessageProcess(std::shared_ptr<InfoLoggerMessageList> msg);

 private:
  MYSQL *db = NULL;                    // handle to mysql db
  MYSQL_STMT* stmt = NULL;             // prepared insertion query
  MYSQL_BIND bind[INFOLOG_FIELDS_MAX]; // parameters bound to variables

  int nFields = 0;

  int dbIsConnected = 0;    // flag set when db was connected with success
  int dbLastConnectTry = 0; // time of last connect attempt
  int dbConnectTrials = 0;  // number of connection attempts since last success

  std::string sql_insert;

  unsigned long long insertCount = 0;     // counter for number of queries executed
  unsigned long long msgDelayedCount = 0; // counter for number of messages delayed (insert failed, retry)
  unsigned long long msgDroppedCount = 0; // counter for number of messages dropped (insert failed, dropped)

  int connectDB(); // function to connect to database
  int disconnectDB(); // disconnect/cleanup DB connection

  int commitEnabled = 1;       // flag to enable transactions
  int commitDebug = 0;         // log transactions
  int commitTimeout = 1000000; // time between commits
  Timer commitTimer;           // timer for transaction
  int commitNumberOfMsg;       // number of messages since last commit
};

void InfoLoggerDispatchSQLImpl::start()
{

  // log DB params
  parent->logInfo("Using DB %s@%s:%s", theConfig->dbUser.c_str(), theConfig->dbHost.c_str(), theConfig->dbName.c_str());

  // prepare insert query from 1st protocol definition
  // e.g. INSERT INTO messages(severity,level,timestamp,hostname,rolename,pid,username,system,facility,detector,partition,run,errcode,errLine,errsource) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?) */
  nFields = 0;
  int errLine = 0;
  sql_insert += "INSERT INTO messages(";
  for (int i = 0; i < INFOLOG_FIELDS_MAX; i++) {
    if (protocols[0].fields[i].type == infoLog_msgField_def_t::ILOG_TYPE_NULL) {
      break;
    }
    if (i) {
      sql_insert += ",";
    }
    sql_insert += "`";
    sql_insert += protocols[0].fields[i].name;
    sql_insert += "`";
    nFields++;
  }
  if (nFields == INFOLOG_FIELDS_MAX) {
    errLine = __LINE__; // INFOLOG_FIELDS_MAX is too small, increase it
  }
  if (nFields == 0) {
    errLine = __LINE__; // protocol is empty !
  }
  sql_insert += ") VALUES(";
  for (int i = nFields; i > 0; i--) {
    if (i > 1) {
      sql_insert += "?,";
    } else {
      sql_insert += "?";
    }
  }
  sql_insert += ")";
  parent->logInfo("insert query = %s", sql_insert.c_str());
  if (errLine) {
    parent->logError("Failed to initialize db query: error %d", errLine);
  }

  // try to connect DB
  // done automatically in customloop
}

InfoLoggerDispatchSQL::InfoLoggerDispatchSQL(ConfigInfoLoggerServer* config, SimpleLog* log, std::string prefix) : InfoLoggerDispatch(config, log, prefix)
{
  dPtr = std::make_unique<InfoLoggerDispatchSQLImpl>();
  dPtr->theConfig = config;
  dPtr->parent = this;
  dPtr->start();

  // enable customloop callback
  isReady = true;
}

void InfoLoggerDispatchSQLImpl::stop()
{
  disconnectDB();
  parent->logInfo("DB thread insert count = %llu, delayed msg count = %llu, dropped msg count = %llu", insertCount, msgDelayedCount, msgDroppedCount);
}

InfoLoggerDispatchSQL::~InfoLoggerDispatchSQL()
{
  dPtr->stop();
}

int InfoLoggerDispatchSQL::customMessageProcess(std::shared_ptr<InfoLoggerMessageList> msg)
{
  return dPtr->customMessageProcess(msg);
}

int InfoLoggerDispatchSQL::customLoop()
{
  return dPtr->customLoop();
}

int InfoLoggerDispatchSQLImpl::connectDB()
{
  if (!dbIsConnected) {
    time_t now = time(NULL);
    if (now < dbLastConnectTry + SQL_RETRY_CONNECT) {
      // wait before reconnecting
      return 1;
    }
    dbLastConnectTry = now;

    if (db == NULL) {
      // init mysql handle
      db = mysql_init(db);
      if (db == NULL) {
        parent->logError("mysql_init() failed");
        return 1;
      }
    }
    parent->logInfo("DB connecting: %s@%s:%s", theConfig->dbUser.c_str(), theConfig->dbHost.c_str(), theConfig->dbName.c_str());
    if (mysql_real_connect(db, theConfig->dbHost.c_str(), theConfig->dbUser.c_str(), theConfig->dbPassword.c_str(), theConfig->dbName.c_str(), 0, NULL, 0)) {
      parent->logInfo("DB connected");
      dbIsConnected = 1;
      dbConnectTrials = 0;
    } else {
      if (dbConnectTrials == 0) { // log only first attempt
        parent->logError("DB connection failed: %s", mysql_error(db));
      }
      dbConnectTrials++;
      return 1;
    }

    // create prepared insert statement
    stmt = mysql_stmt_init(db);
    if (stmt == NULL) {
      parent->logError("mysql_stmt_init() failed: %s", mysql_error(db));
      disconnectDB();
      return -1;
    }

    if (mysql_stmt_prepare(stmt, sql_insert.c_str(), sql_insert.length())) {
      parent->logError("mysql_stmt_prepare() failed: %s", mysql_error(db));
      disconnectDB();
      return -1;
    }

    // bind variables depending on type
    memset(bind, 0, sizeof(bind));
    int errline = 0;
    for (int i = 0; i < nFields; i++) {
      switch (protocols[0].fields[i].type) {
        case infoLog_msgField_def_t::ILOG_TYPE_STRING:
          bind[i].buffer_type = MYSQL_TYPE_STRING;
          break;
        case infoLog_msgField_def_t::ILOG_TYPE_INT:
          bind[i].buffer_type = MYSQL_TYPE_LONG;
          break;
        case infoLog_msgField_def_t::ILOG_TYPE_DOUBLE:
          bind[i].buffer_type = MYSQL_TYPE_DOUBLE;
          break;
        default:
          parent->logError("undefined field type %d", protocols[0].fields[i].type);
          errline = __LINE__;
          break;
      }
    }
    if (errline) {
      disconnectDB();
      return -1;
    }

    // reset transactions
    commitNumberOfMsg = 0;
  }

  return 0;
}

int InfoLoggerDispatchSQLImpl::disconnectDB()
{
  if (stmt != NULL) {
    mysql_stmt_close(stmt);
    stmt = NULL;
  }
  if (db != NULL) {
    mysql_close(db);
    db = NULL;
  }
  if (dbIsConnected) {    
    parent->logInfo("DB disconnected");
  }
  dbIsConnected = 0;
  return 0;
}

int InfoLoggerDispatchSQLImpl::customLoop()
{
  int err = connectDB();
  if (err) {
    // temporization to avoid immediate retry
    sleep(SQL_RETRY_CONNECT);
  } else if (commitEnabled) {
    // complete pending transactions
    if (commitNumberOfMsg) {
      if (commitTimer.isTimeout()) {
        if (mysql_query(db, "COMMIT")) {
          parent->logError("DB transaction commit failed: %s", mysql_error(db));
          commitEnabled = 0;
        } else {
          if (commitDebug) {
            parent->logInfo("DB commit - %d msgs", commitNumberOfMsg);
          }
        }
        commitNumberOfMsg = 0;
      }
    }
  }

  return err;
}

int InfoLoggerDispatchSQLImpl::customMessageProcess(std::shared_ptr<InfoLoggerMessageList> lmsg)
{
  // procedure for dropped messages and keep count of them
  auto returnDroppedMessage = [&](const char* message, infoLog_msg_t* m) {
    // log bad message content (truncated)
    const int maxLen = 200;

    // verbose logging of the other fields
    std::string logDetails;
    for (int i = 0; i < nFields - 1; i++) {
      logDetails += protocols[0].fields[i].name;
      logDetails += "=";
      if (!m->values[i].isUndefined) {
	switch (protocols[0].fields[i].type) {
          case infoLog_msgField_def_t::ILOG_TYPE_STRING:
	    if (m->values[i].value.vString != nullptr) {
	      std::string ss = m->values[i].value.vString;
	      logDetails += ss.substr(0,maxLen);
	      if (ss.length() > maxLen) {
	        logDetails += "...";
	      }
	    }
            break;
          case infoLog_msgField_def_t::ILOG_TYPE_INT:
            logDetails += std::to_string(m->values[i].value.vInt);
            break;
          case infoLog_msgField_def_t::ILOG_TYPE_DOUBLE:
            logDetails += std::to_string(m->values[i].value.vDouble);
            break;
          default:
            break;
	}
      }
      logDetails += " ";
    }

    int msgLen = (int)strlen(message);
    parent->logError("Dropping message (%d bytes): %.*s%s", msgLen, maxLen, message, (msgLen > maxLen) ? "..." : "");
    parent->logError("                 %s", logDetails.c_str());
    msgDroppedCount++;
    return 0; // remove message from queue
  };

  // procedure for delayed messages and keep count of them
  auto returnDelayedMessage = [&]() {
    msgDelayedCount++;
    return -1; // keep message in queue
  };

  if (!dbIsConnected) {
    return returnDelayedMessage();
  }

  infoLog_msg_t* m;
  my_bool param_isnull = 1;    // boolean telling if a parameter is NULL
  my_bool param_isNOTnull = 0; // boolean telling if a parameter is not NULL
  char* msg;
  char* nl; // variables used to reformat multiple-line messages

  for (m = lmsg->msg; m != NULL; m = m->next) {

    for (int i = 0; i < nFields; i++) {
      switch (protocols[0].fields[i].type) {
        case infoLog_msgField_def_t::ILOG_TYPE_STRING:
          bind[i].buffer = (void*)m->values[i].value.vString;
          break;
        case infoLog_msgField_def_t::ILOG_TYPE_INT:
          bind[i].buffer = &m->values[i].value.vInt;
          break;
        case infoLog_msgField_def_t::ILOG_TYPE_DOUBLE:
          bind[i].buffer = &m->values[i].value.vDouble;
          break;
        default:
          bind[i].buffer = NULL;
          break;
      }
      if ((m->values[i].isUndefined) || (bind[i].buffer == NULL)) {
        bind[i].is_null = &param_isnull;
        bind[i].buffer_length = 0;
      } else {
        bind[i].is_null = &param_isNOTnull;
        bind[i].buffer_length = m->values[i].length;
      }
    }

    if (commitEnabled) {
      if (commitNumberOfMsg == 0) {
        if (mysql_query(db, "START TRANSACTION")) {
          parent->logError("DB start transaction failed: %s", mysql_error(db));
          commitEnabled = 0;
          return returnDelayedMessage();
        } else {
          if (commitDebug) {
            parent->logInfo("DB transaction started");
          }
        }
        commitTimer.reset(commitTimeout);
      }
    }

    // re-format message with multiple line - assumes it is the LAST field in the protocol
    for (msg = (char*)m->values[nFields - 1].value.vString; msg != NULL; msg = nl) {
      nl = strchr(msg, '\f');
      if (nl != NULL) {
        *nl = 0;
        nl++;
      }

      // copy msg line
      bind[nFields - 1].buffer = msg;

      // update bind variables
      if (mysql_stmt_bind_param(stmt, bind)) {
        parent->logError("mysql_stmt_bind() failed: %s", mysql_error(db));
        parent->logError("message: %s", msg);
        // if can not bind, message malformed, drop it
        return returnDroppedMessage(msg, m);
      }

      // Do the insertion
      if (mysql_stmt_execute(stmt)) {
        parent->logError("mysql_stmt_exec() failed: (%d) %s", mysql_errno(db), mysql_error(db));
        // column too long
        if (mysql_errno(db) == ER_DATA_TOO_LONG) {
          return returnDroppedMessage(msg, m);
        }
        // retry with new connection - usually it means server was down
        disconnectDB();
        return returnDelayedMessage();
      }

      insertCount++;
      commitNumberOfMsg++;

      if (commitDebug) {
        if (insertCount % 1000 == 0) {
          parent->logInfo("insert count = %llu", insertCount);
        }
      }
    }
  }
  // report message success, it will be removed from queue
  return 0;
}

