// infoLoggerAdminDB
// A command line utility to admin infoLogger database

#include <Common/Configuration.h>
#include <Common/SimpleLog.h>
#include <mysql.h>

#include "infoLoggerDefaults.h"
#include "infoLoggerMessage.h"

#include <iostream>
#include <iomanip>
#include <ctime>
#include <sstream>

void printUsage() {
    printf("Usage: infoLoggerAdminDB ...\n");
    printf("  -c command : action to execute. One of archive (archive main table and create new one), create (create main table), clear (delete main table content), destroy (destroy all tables, including archives), list (show existing message tables)\n");
    printf("  [-z pathToConfigurationFile] : sets which configuration to use. By default %s\n",INFOLOGGER_DEFAULT_CONFIG_PATH);
    printf("  [-o optionName] : enable options not set by default. Possible values: partitioning (create new tables with partition by day)\n");
    printf("  [-h] : print this help\n");
}

int main(int argc, char * argv[]) {

  SimpleLog log;        // handle to log
  ConfigFile config;    // handle to configuration
  std::string configPath(INFOLOGGER_DEFAULT_CONFIG_PATH);  // path to configuration
  std::string command;  // command to be executed

  bool optCreate=0;         // create main message table
  bool optDelete=0;         // delete content of main message table
  bool optDestroy=0;        // destroy all message tables
  bool optPartitioning=0;   // use partitioning (e.g. message by day) for tables created
  bool optArchive=0;        // move content of main message table to (time-based name) archive table
  bool optList=0;           // list current tables
  
  // configure log output
  log.setOutputFormat(SimpleLog::FormatOption::ShowSeverityTxt | SimpleLog::FormatOption::ShowMessage);
  log.info("infoLoggerAdminDB");
  
  // parse command line parameters
  int option;
  while((option = getopt(argc, argv, "z:c:o:h")) != -1){         

      switch(option) {

        case 'z':
          configPath=optarg;
          break;

        case 'c':
          command=optarg;
          break;

        case 'h':
        case '?':
          printUsage();
          return 0;
          break;
          
        case 'o':
        {
          std::string optString=optarg;
          if (optString=="partitioning") {
            optPartitioning=1;
          } else {
            log.error("Wrong option %s",optString.c_str());
          }
        }
          break;

        default:
          log.error("Invalid command line argument %c",option);
          return -1;

      }
  }
     
  if (command=="archive") {
    optArchive=1;
  } else if (command=="create") {
    optCreate=1;
  } else if (command=="clear") {
    optDelete=1;
  } else if (command=="destroy") {
    optDestroy=1;
  } else if (command=="list") {
    optList=1;
  } else {
    log.error("Unkown command %s",command.c_str());
    return -1;
  }
  
  // load infoLogger configuration file
  try {
    config.load("file:" + configPath);
  }
  catch (std::string err) {
    log.error("Failed to load configuration: %s",err.c_str());
    return -1;
  }

  // load configuration file
  std::string dbHost,dbUser,dbPwd,dbName;
  config.getOptionalValue(INFOLOGGER_CONFIG_SECTION_NAME_ADMIN ".dbUser",dbUser);
  config.getOptionalValue(INFOLOGGER_CONFIG_SECTION_NAME_ADMIN ".dbPwd",dbPwd);
  config.getOptionalValue(INFOLOGGER_CONFIG_SECTION_NAME_ADMIN ".dbHost",dbHost);
  config.getOptionalValue(INFOLOGGER_CONFIG_SECTION_NAME_ADMIN ".dbName",dbName);
  
  // init mysql lib
  MYSQL db;   // handle to mysql db
  if (mysql_init(&db)==NULL) {
    log.error("Failed to init MySQL library");
    return -1;
  }

  // connect database
  if (mysql_real_connect(&db,dbHost.c_str(),dbUser.c_str(),dbPwd.c_str(),dbName.c_str(),0,NULL,0)!=0) {
    log.info("Database connected");
  } else {
    log.error("Failed to connect database : %s",mysql_error(&db));
    return -1;
  }

  // execute command(s)
  if (optList) {
    // destroy all messages tables
    std::string sqlQuery="show tables like '" INFOLOGGER_TABLE_MESSAGES "%'";
    if (mysql_query(&db,sqlQuery.c_str())) {
      log.error("Failed to execute %s\n%s",sqlQuery.c_str(),mysql_error(&db));
      return -1;
    }
    MYSQL_RES *res;
    res=mysql_store_result(&db);
    if (res==NULL) {
      log.error("Failed to retrieve results from query %s",sqlQuery.c_str());
      return -1;
    }
    MYSQL_ROW row;
    log.info("Found following tables:");
    for (;;) {
      row=mysql_fetch_row(res);
      if (row==NULL) {
        break;
      }
      if (row[0]==NULL) {
        log.error("No field in row returned for query %s",sqlQuery.c_str());
        return -1;
      }
      log.info("%s",row[0]);
    }
    mysql_free_result(res);  
  }
  
  if (optDelete) {
    log.info("Delete main table content");
    std::string sqlQuery="truncate table " INFOLOGGER_TABLE_MESSAGES;
    if (mysql_query(&db,sqlQuery.c_str())) {
      log.error("Failed to execute %s\n%s",sqlQuery.c_str(),mysql_error(&db));
      return -1;
    }
  }
  
  if (optDestroy) {
    log.info("Destroy all tables");

    // destroy all messages tables
    std::string sqlQuery="show tables like '" INFOLOGGER_TABLE_MESSAGES "%'";
    if (mysql_query(&db,sqlQuery.c_str())) {
      log.error("Failed to execute %s\n%s",sqlQuery.c_str(),mysql_error(&db));
      return -1;
    }
    MYSQL_RES *res;
    res=mysql_store_result(&db);
    if (res==NULL) {
      log.error("Failed to retrieve results from query %s",sqlQuery.c_str());
      return -1;
    }
    MYSQL_ROW row;
    for (;;) {
      row=mysql_fetch_row(res);
      if (row==NULL) {
        break;
      }
      if (row[0]==NULL) {
        log.error("No field in row returned for query %s",sqlQuery.c_str());
        return -1;
      }
      std::string sqlDrop="drop table `";
      sqlDrop+=row[0];
      sqlDrop+="`";
      if (mysql_query(&db,sqlDrop.c_str())) {
        log.error("Failed to execute %s\n%s",sqlDrop.c_str(),mysql_error(&db));
        return -1;
      }
      log.info("Dropped table %s",row[0]);
    }
    mysql_free_result(res);
  }

  std::string sqlTableDesriptionMessages="(severity char(1), level tinyint unsigned, timestamp double(16,6), hostname varchar(32), rolename varchar(32), pid smallint \
    unsigned, username varchar(32), system varchar(32), facility varchar(32), detector varchar(32), `partition` varchar(32), run int unsigned, errcode int unsigned, \
    errline smallint unsigned, errsource varchar(32), message text, index ix_severity(severity), index ix_level(level), index ix_timestamp(timestamp), index \
    ix_hostname(hostname(14)), index ix_rolename(rolename(20)), index ix_system(system(3)), index ix_facility(facility(20)), index ix_detector(detector(8)), index \
    ix_partition(`partition`(10)), index ix_run(run), index ix_errcode(errcode), index ix_errline(errline), index ix_errsource(errsource(20))) ENGINE=MyISAM";

  if (optPartitioning) {
    log.info("Using partitioning");
    sqlTableDesriptionMessages+=" partition by hash(timestamp div 86400) partitions 365";
  }

  if (optArchive) {
    log.info("Archiving main infoLogger table");
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    std::ostringstream now;
    now << std::put_time(&tm, "%Y_%m_%d__%H_%M_%S");
    std::string sqlTableNameArchive=INFOLOGGER_TABLE_MESSAGES "__" + now.str();
    
    std::string sqlQuery;
    sqlQuery="drop table if exists `new`";
    if (mysql_query(&db,sqlQuery.c_str())) {
      log.error("Failed to execute %s\n%s",sqlQuery.c_str(),mysql_error(&db));
      return -1;
    }
    sqlQuery="create table `new`" + sqlTableDesriptionMessages;
    if (mysql_query(&db,sqlQuery.c_str())) {
      log.error("Failed to execute %s\n%s",sqlQuery.c_str(),mysql_error(&db));
      return -1;
    }
    sqlQuery="rename table `" INFOLOGGER_TABLE_MESSAGES "` to `" + sqlTableNameArchive + "`, `new` to `" INFOLOGGER_TABLE_MESSAGES "`;";
    if (mysql_query(&db,sqlQuery.c_str())) {
      log.error("Failed to execute %s\n%s",sqlQuery.c_str(),mysql_error(&db));
      return -1;
    }
  }
  
  if (optCreate) {
    log.info("Creating infoLogger table");    
    // prepare insert query corresponding to 1st protocol definition
    // create table messages(severity char(1), level tinyint unsigned, timestamp double(16,6), hostname varchar(32), rolename varchar(32), pid smallint unsigned, username varchar(32), system varchar(32), facility varchar(32), detector varchar(32), `partition` varchar(32), run int unsigned, errcode int unsigned, errline smallint unsigned, errsource varchar(32), message text, index ix_severity(severity), index ix_level(level), index ix_timestamp(timestamp), index ix_hostname(hostname(14)), index ix_rolename(rolename(20)), index ix_system(system(3)), index ix_facility(facility(20)), index ix_detector(detector(8)), index ix_partition(`partition`(10)), index ix_run(run), index ix_errcode(errcode), index ix_errline(errline), index ix_errsource(errsource(20)));
    std::string sqlQuery;    
    sqlQuery="create table " INFOLOGGER_TABLE_MESSAGES + sqlTableDesriptionMessages;
    if (mysql_query(&db,sqlQuery.c_str())) {
      log.error("Failed to execute %s\n%s",sqlQuery.c_str(),mysql_error(&db));
      return -1;
    }    
  }

  // report
  log.info("Operations completed");

  // close database
  mysql_close(&db);

  return 0;
}
