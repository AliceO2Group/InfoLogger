#include "InfoLoggerDispatch.h"
#include <mysql.h>
#include "utility.h"
#include "infoLoggerMessage.h"
#include <unistd.h>
#include <string.h>


// some constants
#define SQL_RETRY_CONNECT 3     // base retry time, will be sleeping up to 10x this value



class InfoLoggerDispatchSQLImpl {
  public:
    
  SimpleLog *theLog; // handle for logging
  ConfigInfoLoggerServer *theConfig;   // struct with config parameters

  void  start();
  void stop();
  int customLoop();
  int customMessageProcess(std::shared_ptr<InfoLoggerMessageList> msg);
  
  
  private:
  MYSQL db;   // handle to mysql db
  MYSQL_STMT      *stmt=NULL;                 // prepared insertion query
  MYSQL_BIND      bind[INFOLOG_FIELDS_MAX];   // parameters bound to variables

  int nFields=0;

  int dbIsConnected=0;    // flag set when db was connected with success
  int dbLastConnectTry=0; // time of last connect attempt
  int dbConnectTrials=0;  // number of connection attempts since last success
  
  std::string sql_insert;
  
};

void InfoLoggerDispatchSQLImpl::start() {

  // init mysql lib
  if (mysql_init(&db)==NULL) {
    theLog->error("mysql_init() failed");
    throw __LINE__;
  }
  
  // prepare insert query from 1st protocol definition
  // e.g. INSERT INTO messages(severity,level,timestamp,hostname,rolename,pid,username,system,facility,detector,partition,run,errcode,errLine,errsource) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?) */
  nFields=0;
  int errLine=0;
  sql_insert+="INSERT INTO messages(";
  for(int i=0;i<INFOLOG_FIELDS_MAX;i++) {
    if (protocols[0].fields[i].type==infoLog_msgField_def_t::ILOG_TYPE_NULL) {break;}
    if (i) {
     sql_insert+=",";
    }
    sql_insert+="`";
    sql_insert+=protocols[0].fields[i].name;
    sql_insert+="`";
    nFields++;
  }
  if (nFields==INFOLOG_FIELDS_MAX) {
    errLine=__LINE__;      // INFOLOG_FIELDS_MAX is too small, increase it
  }
  if (nFields==0) {
    errLine=__LINE__;      // protocol is empty !
  }
  sql_insert+=") VALUES(";
  for(int i=nFields;i>0;i--) {
    if (i>1) {
      sql_insert+="?,";
    } else {
      sql_insert+="?";
    }
  }
  sql_insert+=")";
  theLog->info("insert query = %s",sql_insert.c_str());
  if (errLine) {
    theLog->error("Failed to initialize db query: error %d",errLine);
  }

}

InfoLoggerDispatchSQL::InfoLoggerDispatchSQL(ConfigInfoLoggerServer *config, SimpleLog *log): InfoLoggerDispatch(config,log) {
  dPtr=std::make_unique<InfoLoggerDispatchSQLImpl>();
  dPtr->theLog=log;
  dPtr->theConfig=config; 
  dPtr->start();
}

void InfoLoggerDispatchSQLImpl::stop() {
  if (dbIsConnected) {
    mysql_stmt_close(stmt);
    theLog->info("DB disconnected");    
  }
  mysql_close(&db);
}


InfoLoggerDispatchSQL::~InfoLoggerDispatchSQL() {
  dPtr->stop();
}

int InfoLoggerDispatchSQL::customMessageProcess(std::shared_ptr<InfoLoggerMessageList> msg) {
  return dPtr->customMessageProcess(msg);
}



int InfoLoggerDispatchSQL::customLoop() {
  return dPtr->customLoop();
}



int InfoLoggerDispatchSQLImpl::customLoop() {

    if (!dbIsConnected) {
      time_t now=time(NULL);
      if (now<dbLastConnectTry+SQL_RETRY_CONNECT) {
        // wait before reconnecting
        return 0;
      }
      dbLastConnectTry=now;
      if (mysql_real_connect(&db,theConfig->dbHost.c_str(),theConfig->dbUser.c_str(),theConfig->dbPassword.c_str(),theConfig->dbName.c_str(),0,NULL,0)) {
        theLog->info("DB connected");
        dbIsConnected=1;
        dbConnectTrials=0;
      } else {
        if (dbConnectTrials==1) {
          theLog->error("DB connection Failed");
        }
        dbConnectTrials++;
        return 0;
      }
      
      // create prepared insert statement
      stmt=mysql_stmt_init(&db);
      if (stmt==NULL) {
        theLog->error("mysql_stmt_init() failed: %s",mysql_error(&db));
        return -1;
      }

      if (mysql_stmt_prepare(stmt,sql_insert.c_str(),sql_insert.length())) {
        theLog->error("mysql_stmt_prepare() failed: %s\n",mysql_error(&db));
        return -1;
      }

      // bind variables depending on type
      memset(bind, 0, sizeof(bind));
      int errline=0;
      for(int i=0;i<nFields;i++) {
        switch (protocols[0].fields[i].type) {
          case infoLog_msgField_def_t::ILOG_TYPE_STRING:
            bind[i].buffer_type=MYSQL_TYPE_STRING;
            break;
          case infoLog_msgField_def_t::ILOG_TYPE_INT:
            bind[i].buffer_type=MYSQL_TYPE_LONG;
            break;
          case infoLog_msgField_def_t::ILOG_TYPE_DOUBLE:
            bind[i].buffer_type=MYSQL_TYPE_DOUBLE;
            break;
          default:
            theLog->error("undefined field type %d",protocols[0].fields[i].type);
            errline=__LINE__;
            break;
        }
      }
      if (errline) {      
        return -1;
      }
    }


  return 0;
}


int InfoLoggerDispatchSQLImpl::customMessageProcess(std::shared_ptr<InfoLoggerMessageList> lmsg) {
 // todo: keep message in queue on error!

   infoLog_msg_t *m;
   my_bool param_isnull=1; // boolean telling if a parameter is NULL
   my_bool param_isNOTnull=0; // boolean telling if a parameter is not NULL
   char *msg;
   char *nl;    // variables used to reformat multiple-line messages

   for (m=lmsg->msg;m!=NULL;m=m->next){  

         for(int i=0;i<nFields;i++) {
          switch (protocols[0].fields[i].type) {
            case infoLog_msgField_def_t::ILOG_TYPE_STRING:
              bind[i].buffer=(void *)m->values[i].value.vString;
              break;
            case infoLog_msgField_def_t::ILOG_TYPE_INT:
              bind[i].buffer=&m->values[i].value.vInt;
              break;
            case infoLog_msgField_def_t::ILOG_TYPE_DOUBLE:
              bind[i].buffer=&m->values[i].value.vDouble;
              break;
            default:
              bind[i].buffer = NULL;
              break;
          }
          if ((m->values[i].isUndefined)||(bind[i].buffer == NULL)) {
            bind[i].is_null = &param_isnull;
            bind[i].buffer_length = 0;
          } else {
            bind[i].is_null = &param_isNOTnull;
            bind[i].buffer_length = m->values[i].length;
          }
        }
 
        // re-format message with multiple line - assumes it is the LAST field in the protocol
        for(msg=(char *)m->values[nFields-1].value.vString;msg!=NULL;msg=nl) {
          nl=strchr(msg,'\f');
          if (nl!=NULL) {
            *nl=0;
            nl++;
          }

          // copy msg line
          bind[nFields-1].buffer = msg;

          // update bind variables
          if (mysql_stmt_bind_param(stmt,bind)) {
            theLog->error("mysql_stmt_bind() failed: %s\n",mysql_error(&db));
            mysql_stmt_close(stmt);
            mysql_close(&db);
            theLog->info("DB disconnected");
            return -1;
          }

          // Do the insertion
          if (mysql_stmt_execute(stmt)) {
            theLog->error("mysql_stmt_exec() failed: %s\n",mysql_error(&db));
            mysql_stmt_close(stmt);
            mysql_close(&db);
            theLog->info("DB disconnected");
            // retry with new connection - usually it means server was down
            dbIsConnected=0;
            break;
          }
       }
    }
  return 0;
}
