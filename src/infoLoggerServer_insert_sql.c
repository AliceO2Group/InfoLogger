/** Infologger server
  *
  * This file implements the MySQL insert function.
  *
  * History:
  *  - 07/04/2005  Added Run Number field
  *  - 16/08/2010  Added fields: detector, partition, errcode/line/source.
                   Use generic message fields description.
  *  - 20/02/2013  Escape the column name in quotes (e.g. `partition` is keyword from mysql 5.6)
*/


#include <stdlib.h>

#include "infoLoggerServer_insert.h"
#include "simplelog.h"

#include <string.h>
#include "utility.h"
#include <stdio.h>
#include <unistd.h>

#include <mysql.h>
#include <sys/time.h>

#define SQL_RETRY_CONNECT 10     /* base retry time, will be sleeping up to 10x this value */

#include "infoLoggerMessage.h"
#include "infoLoggerConfig.h"

extern void infoLog_msg_destroy(infoLog_msg_t *m);
extern t_infoLoggerConfig cfg;              /* infoLoggerServer configuration */


/* insertion thread main loop */
int insert_th_loop(void *arg){
  insert_th       *t;                         /* thread handle */
  MYSQL           db;                         /* database handle */
  MYSQL_STMT      *stmt=NULL;                 /* prepared insertion query */
  MYSQL_BIND      bind[INFOLOG_FIELDS_MAX];   /* parameters bound to variables */
  infoLog_msg_t   *m=NULL;                    /* list of messages received */

  char *db_host;    /* MySQL server host */
  char *db_user;    /* MySQL user */
  char *db_pwd;     /* MySQL password */
  char *db_infodb;  /* MySQL infologger db */

  int n_retry=0;    /* number of connection retry */
  int i;          /* simple counter */  
  int errline=0;    /* error number */
  int nFields=0;      /* number of fields in a message */
  
  char *msg,*nl;    /* variables used to reformat multiple-line messages */
  int reconnect;    /* set to 1 to ask db reconnection */

  int reconnect_interval=0;             /* sleep time (milliseconds) for next reconnect. Starts from 0, then increases. Reset on 'clear' */
  time_t reconnect_last_clear=0;        /* time of last reset of reconnection stats */
  int reconnect_clear_timeout=900;      /* timeout to reset reconnection stats (in seconds) */
  int reconnect_count=0;                /* number of reconnects since last 'reset' */
    
  my_bool param_isnull=1; /* boolean telling if a parameter is NULL */
  my_bool param_isNOTnull=0; /* boolean telling if a parameter is not NULL */

  /* arg = insert thread struct */
  if (arg==NULL) {
    slog(SLOG_ERROR,"insert_th_loop(): bad parameters");
    return -1;
  }
  t=(insert_th *)arg;

  /* take connection parameters from environment */
  /* mysql_connect handles correctly NULL values, no need to check */
  db_host=cfg.serverMysqlHost;
  db_user=cfg.serverMysqlUser;
  db_pwd=cfg.serverMysqlPwd;
  db_infodb=cfg.serverMysqlDb;

  /* set default database */
  if (db_infodb==NULL) db_infodb="infologger";

  /* log connection parameters */
  slog(SLOG_INFO,"connecting MySQL: host=%s, user=%s, pwd=%s, db=%s",
    db_host ? db_host : "[localhost]",
    db_user ? db_user : "[default]",
    db_pwd ? "[YES]" : "[NO]",
    db_infodb ? db_infodb : "[default]");

  /* init mysql lib */
  if (mysql_init(&db)==NULL) {
    slog(SLOG_ERROR,"mysql_init() failed");
    return -1;
  } 
  
  /* prepare insert query from 1st protocol definition */
  /* e.g. INSERT INTO messages(severity,level,timestamp,hostname,rolename,pid,username,system,facility,detector,partition,dest,run,errcode,errline,errsource) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?) */
  mysprintf_t sql_insert;
  if (!mysprintf_init(&sql_insert, NULL, 500)) {
    mysprintf(&sql_insert,"INSERT INTO messages(");
    for(i=0;i<INFOLOG_FIELDS_MAX;i++) {
      if (protocols[0].fields[i].type==ILOG_TYPE_NULL) {break;}
      if (i) {
        mysprintf(&sql_insert,",");
      }
      mysprintf(&sql_insert,"`%s`",protocols[0].fields[i].name);
      nFields++;
    }
    if (i==INFOLOG_FIELDS_MAX) {
      errline=__LINE__;      /* INFOLOG_FIELDS_MAX is too small, increase it */
    }
    if (nFields==0) {
      errline=__LINE__;      /* protocol is empty ! */
    }
    mysprintf(&sql_insert,") VALUES(");
    for(;i>0;i--) {
      if (i>1) {
        mysprintf(&sql_insert,"?,");
      } else {
        mysprintf(&sql_insert,"?");
      }
    }
    mysprintf(&sql_insert,")");
    slog(SLOG_INFO,"insert query = %s",sql_insert.buffer);
    if (sql_insert.isError) {
      errline=__LINE__;
    }
  } else {
    errline=__LINE__;
  }
  if (errline) {
    slog(SLOG_ERROR,"Failed to initialize db query: error %d",errline);
  }

  
  /* connection loop */
  for (;errline==0;) {
    
    if (reconnect_last_clear+reconnect_clear_timeout<time(NULL)) {
      reconnect_interval=0;
      reconnect_count=0;
      reconnect_last_clear=time(NULL);
    }
    if (reconnect) {
      reconnect_count++;
      if (reconnect_interval) {     
        usleep(reconnect_interval*1000);
        reconnect_interval*=4;
      } else {
        reconnect_interval=1;
      }
    }
 
    /* connect database (keep trying) */
    while (!mysql_real_connect(&db,db_host,db_user,db_pwd,db_infodb,0,NULL,0)) {
      if (!n_retry) {
        slog(SLOG_ERROR,"Failed to connect to database: %s",mysql_error(&db));
      }
      if (n_retry<10) {
        n_retry++;
      }
      for (i=0;i<n_retry*SQL_RETRY_CONNECT;i++) {
        /* shutdown requested? */
        pthread_mutex_lock(&t->shutdown_mutex);
        if (t->shutdown) {
          pthread_mutex_unlock(&t->shutdown_mutex);
          mysprintf_destroy(&sql_insert);
          return 0;
        }
        pthread_mutex_unlock(&t->shutdown_mutex);
        sleep(1);
      }
    }
    n_retry=0;
    slog(SLOG_INFO,"DB connected");
    
    /* create prepared insert statement */
    stmt=mysql_stmt_init(&db);
    if (stmt==NULL) {
      slog(SLOG_ERROR,"mysql_stmt_init() failed: %s",mysql_error(&db));
      mysql_close(&db);
      slog(SLOG_INFO,"DB disconnected");
      mysprintf_destroy(&sql_insert);
      return -1;
    }
        
    if (mysql_stmt_prepare(stmt,sql_insert.buffer,sql_insert.strLength)) {
      slog(SLOG_ERROR,"mysql_stmt_prepare() failed: %s\n",mysql_error(&db));
      mysql_stmt_close(stmt);
      mysql_close(&db);
      slog(SLOG_INFO,"DB disconnected");
      mysprintf_destroy(&sql_insert);
      return -1;
    }

    /* bind variables depending on type */
    memset(bind, 0, sizeof(bind));
    for(i=0;i<nFields;i++) {
      switch (protocols[0].fields[i].type) {
        case ILOG_TYPE_STRING:
          bind[i].buffer_type=MYSQL_TYPE_STRING;
          break;
        case ILOG_TYPE_INT:
          bind[i].buffer_type=MYSQL_TYPE_LONG;
          break;
        case ILOG_TYPE_DOUBLE:
          bind[i].buffer_type=MYSQL_TYPE_DOUBLE;
          break;
        default:
          slog(SLOG_ERROR,"undefined field type %d",protocols[0].fields[i].type);
          errline=__LINE__;
          break;
      }
    }
    if (errline) {break;}


    /* receive and store messages */
    for(reconnect=0;;){

      /* read sample from FIFO - timeout 1 sec */
      /* if not already one pending */
      if (m==NULL) {
        m=(infoLog_msg_t *)ptFIFO_read(t->queue,1);
      }

      /* got a message? */
      if (m!=NULL) {

        for(i=0;i<nFields;i++) {
          switch (protocols[0].fields[i].type) {
            case ILOG_TYPE_STRING:
              bind[i].buffer=m->values[i].value.vString;
              break;
            case ILOG_TYPE_INT:
              bind[i].buffer=&m->values[i].value.vInt;
              break;
            case ILOG_TYPE_DOUBLE:
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
 
        /* re-format message with multiple line - assumes it is the LAST field in the protocol */
        for(msg=m->values[nFields-1].value.vString;msg!=NULL;msg=nl) {
          nl=strchr(msg,'\f');
          if (nl!=NULL) {
            *nl=0;
            nl++;
          }

          /* copy msg line */
          bind[nFields-1].buffer = msg;

          /* update bind variables */
          if (mysql_stmt_bind_param(stmt,bind)) {
            slog(SLOG_ERROR,"mysql_stmt_bind() failed: %s\n",mysql_error(&db));
            mysql_stmt_close(stmt);
            mysql_close(&db);
            slog(SLOG_INFO,"DB disconnected");
            mysprintf_destroy(&sql_insert);
            return -1;
          }

          /* Do the insertion */
          if (mysql_stmt_execute(stmt)) {
            slog(SLOG_ERROR,"mysql_stmt_exec() failed: %s\n",mysql_error(&db));
            mysql_stmt_close(stmt);
            mysql_close(&db);
            slog(SLOG_INFO,"DB disconnected");
            /* retry with new connection - usually it means server was down */
            reconnect=1;
            break;
          }
          
          /*
          struct timeval  tv;
          gettimeofday(&tv,NULL);
          slog(SLOG_INFO,"insert @ %d.%d\n",(int)tv.tv_sec,(int)tv.tv_usec);
          */

        }
        
        /* reconnect db if needed */
        if (reconnect) break;
               
        /* free message */
        infoLog_msg_destroy(m);
        m=NULL;

        /* loop until empty queue */
        continue;
      }

      /* shutdown requested? */
      pthread_mutex_lock(&t->shutdown_mutex);
      if (t->shutdown) {
        pthread_mutex_unlock(&t->shutdown_mutex);
        /* disconnect database */
        mysql_stmt_close(stmt);
        mysql_close(&db);
        slog(SLOG_INFO,"DB disconnected");
        mysprintf_destroy(&sql_insert);
        return 0;
      }
      pthread_mutex_unlock(&t->shutdown_mutex);
    }
  }
  
  mysprintf_destroy(&sql_insert);


  /* log to file not to loose messages */
  //  emergency_mode:
  slog(SLOG_ERROR,"Switching to emergency mode, logging to file");
  return insert_th_loop_nosql(arg);
}
