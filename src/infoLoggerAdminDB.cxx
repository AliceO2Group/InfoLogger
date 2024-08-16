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
#include <vector>

void printUsage()
{
  printf("Usage: infoLoggerAdminDB ...\n");
  printf("  -c command : action to execute. One of archive (archive main table and create new one), create (create main table), clear (delete main table content), destroy (destroy all tables, including archives), list (show existing message tables), status (show database content info)\n");
  printf("  [-z pathToConfigurationFile] : sets which configuration to use. By default %s\n", INFOLOGGER_DEFAULT_CONFIG_PATH);
  printf("  [-o optionName] : enable options not set by default. Possible values: partitioning (create new tables with partition by day), noWarning (disable interactive confirmation in delete operations).\n");
  printf("  [-h] : print this help\n");
}

int main(int argc, char* argv[])
{

  SimpleLog log;                                          // handle to log
  ConfigFile config;                                      // handle to configuration
  std::string configPath(INFOLOGGER_DEFAULT_CONFIG_PATH); // path to configuration
  std::string command;                                    // command to be executed

  bool optCreate = 0;       // create main message table
  bool optDelete = 0;       // delete content of main message table
  bool optDestroy = 0;      // destroy all message tables
  bool optPartitioning = 0; // use partitioning (e.g. message by day) for tables created
  bool optArchive = 0;      // move content of main message table to (time-based name) archive table
  bool optList = 0;         // list current tables
  bool optNone = 0;         // no command specified - test DB access only
  bool optShowCreate = 0;   // print command used to create table
  bool optStatus = 0;       // get a summary of database content
  bool optStatusMore = 0;   // print a summary of database content
  bool optNoWarning = 0;    // when set, there is no warning/interactive confirmation when data to be deleted

  // configure log output
  log.setOutputFormat(SimpleLog::FormatOption::ShowSeverityTxt | SimpleLog::FormatOption::ShowMessage);
  log.info("infoLoggerAdminDB");

  // parse command line parameters
  int option;
  while ((option = getopt(argc, argv, "z:c:o:h")) != -1) {

    switch (option) {

      case 'z':
        configPath = optarg;
        break;

      case 'c':
        command = optarg;
        break;

      case 'h':
      case '?':
        printUsage();
        return 0;
        break;

      case 'o': {
        std::string optString = optarg;
        if (optString == "partitioning") {
          optPartitioning = 1;
        } else if (optString == "noWarning") {
          optNoWarning = 1;
        } else {
          log.error("Wrong option %s", optString.c_str());
	  return -1;
        }
      } break;

      default:
        log.error("Invalid command line argument %c", option);
        return -1;
    }
  }

  if (command == "archive") {
    optArchive = 1;
  } else if (command == "create") {
    optCreate = 1;
  } else if (command == "clear") {
    optDelete = 1;
    optStatus = 1;
  } else if (command == "destroy") {
    optDestroy = 1;
    optStatus = 1;
  } else if (command == "list") {
    optList = 1;
  } else if (command == "showCreate") {
    optShowCreate = 1;
  } else if (command == "status") {
    optStatus = 1;
    optStatusMore = 1;
  } else if (command == "") {
    optNone = 1;
  } else {
    log.error("Unkown command %s", command.c_str());
    return -1;
  }

  // load infoLogger configuration file
  try {
    config.load("file:" + configPath);
  } catch (std::string err) {
    log.error("Failed to load configuration: %s", err.c_str());
    return -1;
  }

  // load configuration file
  std::string dbHost, dbUser, dbPwd, dbName;
  config.getOptionalValue(INFOLOGGER_CONFIG_SECTION_NAME_ADMIN ".dbUser", dbUser);
  config.getOptionalValue(INFOLOGGER_CONFIG_SECTION_NAME_ADMIN ".dbPassword", dbPwd);
  config.getOptionalValue(INFOLOGGER_CONFIG_SECTION_NAME_ADMIN ".dbHost", dbHost);
  config.getOptionalValue(INFOLOGGER_CONFIG_SECTION_NAME_ADMIN ".dbName", dbName);

  // init mysql lib
  MYSQL db; // handle to mysql db
  if (mysql_init(&db) == NULL) {
    log.error("Failed to init MySQL library");
    return -1;
  }

  // connect database
  if (mysql_real_connect(&db, dbHost.c_str(), dbUser.c_str(), dbPwd.c_str(), dbName.c_str(), 0, NULL, 0) != 0) {
    log.info("Database connected");
  } else {
    log.error("Failed to connect database : %s", mysql_error(&db));
    return -1;
  }

  // if no command defined, stop here
  if (optNone) {
    log.info("No command specified, exiting.");
    return 0;
  }

  // info on current database content
  struct infoTable {
    std::string name;
    unsigned long rows;
    unsigned long size;
  };
  std::vector<infoTable> tables;
  unsigned long long totalTables = 0; // number of tables
  unsigned long long totalRows = 0; // number of rows
  unsigned long long totalBytes = 0; // number of bytes
  unsigned long long mainRows = 0; // number of messages in main table
  unsigned long long mainBytes = 0; // number of bytes in main table

  // execute command(s)
  if (optStatus) {
    std::string sqlQuery = "select TABLE_NAME,TABLE_ROWS,DATA_LENGTH from information_schema.TABLES where table_name like '" INFOLOGGER_TABLE_MESSAGES "%' order by table_name";
    if (mysql_query(&db, sqlQuery.c_str())) {
      log.error("Failed to execute %s\n%s", sqlQuery.c_str(), mysql_error(&db));
      return -1;
    }
    MYSQL_RES* res;
    res = mysql_store_result(&db);
    if (res == NULL) {
      log.error("Failed to retrieve results from query %s", sqlQuery.c_str());
      return -1;
    }
    const int numFields = 3;
    if (mysql_num_fields(res) != numFields) {
      log.error("Failed to retrieve %d fields from query %s", numFields, sqlQuery.c_str());
      return -1;
    }
    MYSQL_ROW row;
    for (int n=0;;n++) {
      row = mysql_fetch_row(res);
      if (row == NULL) {
        break;
      }
      for (int i=0; i<numFields; i++) {
	if (row[i] == NULL) {
          log.error("No field[%d] in row %d returned for query %s", i, n, sqlQuery.c_str());
          return -1;
	}
      }
      unsigned long long nRows = strtoul(row[1], NULL, 10);
      unsigned long long nBytes = strtoul(row[2], NULL, 10);
      tables.push_back({row[0], nRows, nBytes});
      totalTables++;
      totalRows += nRows;
      totalBytes += nBytes;
      if (!strcmp(row[0], INFOLOGGER_TABLE_MESSAGES)) {
	mainRows = nRows;
	mainBytes = nBytes;
      }
    }
    mysql_free_result(res);
    if (optStatusMore) {
      log.info("Found following tables:");
      for (const auto &i: tables) {
	log.info("  %s : %lu rows, %lu bytes", i.name.c_str(), i.rows, i.size);	
      }
      log.info("Total: %llu tables, %llu rows, %llu bytes", totalTables, totalRows, totalBytes);
    }
  }

  if (optList) {
    // list all messages tables
    std::string sqlQuery = "show tables like '" INFOLOGGER_TABLE_MESSAGES "%'";
    if (mysql_query(&db, sqlQuery.c_str())) {
      log.error("Failed to execute %s\n%s", sqlQuery.c_str(), mysql_error(&db));
      return -1;
    }
    MYSQL_RES* res;
    res = mysql_store_result(&db);
    if (res == NULL) {
      log.error("Failed to retrieve results from query %s", sqlQuery.c_str());
      return -1;
    }
    MYSQL_ROW row;
    log.info("Found following tables:");
    for (;;) {
      row = mysql_fetch_row(res);
      if (row == NULL) {
        break;
      }
      if (row[0] == NULL) {
        log.error("No field in row returned for query %s", sqlQuery.c_str());
        return -1;
      }
      log.info("%s", row[0]);
    }
    mysql_free_result(res);
  }

  auto confirm = [&]() {
    if (!optNoWarning) {
      log.info("Please confirm: type 'yes'");
      char buf[5]="";
      fgets(buf, 5, stdin);
      if (strcmp("yes\n", buf)) {
        log.info("Operation aborted %s",buf);
	return 0;
      }
    }
    return 1;
  };

  if (optDelete) {
    log.info("Delete main table content");
    if ((mainRows)&&(!optNoWarning)) {
      log.warning("This table is not empty ! %llu rows (%llu bytes) will be deleted", mainRows, mainBytes);
      if (!confirm()) {
	return -1;
      }
    }
    std::string sqlQuery = "truncate table " INFOLOGGER_TABLE_MESSAGES;
    if (mysql_query(&db, sqlQuery.c_str())) {
      log.error("Failed to execute %s\n%s", sqlQuery.c_str(), mysql_error(&db));
      return -1;
    }
  }

  if (optDestroy) {
    log.info("Destroy all tables");
    if ((totalRows)&&(!optNoWarning)) {
      log.warning("The tables are not empty ! %llu tables %llu rows (%llu bytes) will be deleted", totalTables, totalRows, totalBytes);
      if (!confirm()) {
	return -1;
      }
    }
    // destroy all messages tables
    std::string sqlQuery = "show tables like '" INFOLOGGER_TABLE_MESSAGES "%'";
    if (mysql_query(&db, sqlQuery.c_str())) {
      log.error("Failed to execute %s\n%s", sqlQuery.c_str(), mysql_error(&db));
      return -1;
    }
    MYSQL_RES* res;
    res = mysql_store_result(&db);
    if (res == NULL) {
      log.error("Failed to retrieve results from query %s", sqlQuery.c_str());
      return -1;
    }
    MYSQL_ROW row;
    for (;;) {
      row = mysql_fetch_row(res);
      if (row == NULL) {
        break;
      }
      if (row[0] == NULL) {
        log.error("No field in row returned for query %s", sqlQuery.c_str());
        return -1;
      }
      std::string sqlDrop = "drop table `";
      sqlDrop += row[0];
      sqlDrop += "`";
      if (mysql_query(&db, sqlDrop.c_str())) {
        log.error("Failed to execute %s\n%s", sqlDrop.c_str(), mysql_error(&db));
        return -1;
      }
      log.info("Dropped table %s", row[0]);
    }
    mysql_free_result(res);
  }

  std::string sqlTableDesriptionMessages =
    "(severity char(1), level tinyint unsigned, timestamp double(16,6), hostname varchar(32), rolename varchar(32), pid mediumint \
    unsigned, username varchar(32), `system` varchar(32), facility varchar(32), detector varchar(32), `partition` varchar(32), run int unsigned, errcode int unsigned, \
    errline smallint unsigned, errsource varchar(32), message text, index ix_severity(severity), index ix_level(level), index ix_timestamp(timestamp), index \
    ix_hostname(hostname(14)), index ix_rolename(rolename(20)), index ix_system(`system`(3)), index ix_facility(facility(20)), index ix_detector(detector(8)), index \
    ix_partition(`partition`(10)), index ix_run(run), index ix_errcode(errcode), index ix_errline(errline), index ix_errsource(errsource(20)))";

  if (optPartitioning) {
    log.info("Using partitioning");
    sqlTableDesriptionMessages += " partition by hash(timestamp div 86400) partitions 365";
  }

  if (optShowCreate) {
    log.info("SQL create statement:\n\ncreate table " INFOLOGGER_TABLE_MESSAGES " %s;\n", sqlTableDesriptionMessages.c_str());
  }

  if (optArchive) {
    log.info("Archiving main infoLogger table");
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    std::ostringstream now;
    now << std::put_time(&tm, "%Y_%m_%d__%H_%M_%S");
    std::string sqlTableNameArchive = INFOLOGGER_TABLE_MESSAGES "__" + now.str();

    std::string sqlQuery;
    sqlQuery = "drop table if exists `new`";
    if (mysql_query(&db, sqlQuery.c_str())) {
      log.error("Failed to execute %s\n%s", sqlQuery.c_str(), mysql_error(&db));
      return -1;
    }
    sqlQuery = "create table `new`" + sqlTableDesriptionMessages;
    if (mysql_query(&db, sqlQuery.c_str())) {
      log.error("Failed to execute %s\n%s", sqlQuery.c_str(), mysql_error(&db));
      return -1;
    }
    sqlQuery = "rename table `" INFOLOGGER_TABLE_MESSAGES "` to `" + sqlTableNameArchive + "`, `new` to `" INFOLOGGER_TABLE_MESSAGES "`;";
    if (mysql_query(&db, sqlQuery.c_str())) {
      log.error("Failed to execute %s\n%s", sqlQuery.c_str(), mysql_error(&db));
      return -1;
    }
  }

  if (optCreate) {
    log.info("Creating infoLogger table");
    // prepare insert query corresponding to 1st protocol definition
    // create table messages(severity char(1), level tinyint unsigned, timestamp double(16,6), hostname varchar(32), rolename varchar(32), pid mediumint unsigned, username varchar(32), system varchar(32), facility varchar(32), detector varchar(32), `partition` varchar(32), run int unsigned, errcode int unsigned, errline smallint unsigned, errsource varchar(32), message text, index ix_severity(severity), index ix_level(level), index ix_timestamp(timestamp), index ix_hostname(hostname(14)), index ix_rolename(rolename(20)), index ix_system(system(3)), index ix_facility(facility(20)), index ix_detector(detector(8)), index ix_partition(`partition`(10)), index ix_run(run), index ix_errcode(errcode), index ix_errline(errline), index ix_errsource(errsource(20)));
    std::string sqlQuery;
    sqlQuery = "create table " INFOLOGGER_TABLE_MESSAGES + sqlTableDesriptionMessages;
    if (mysql_query(&db, sqlQuery.c_str())) {
      log.error("Failed to execute %s\n%s", sqlQuery.c_str(), mysql_error(&db));
      return -1;
    }
  }

  // report
  log.info("Operations completed");

  // close database
  mysql_close(&db);

  return 0;
}

