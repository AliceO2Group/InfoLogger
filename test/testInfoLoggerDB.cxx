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

#include <mysql.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#if LIBMYSQL_VERSION_ID >= 80000
typedef bool my_bool;
#endif

int main(int argc, char** argv)
{

  int count = 10000;
  int transactNMsg = 0;
  if (argc >= 2) {
    count = atoi(argv[1]);
  }
  if (argc >= 3) {
    transactNMsg = atoi(argv[2]);
  }

  MYSQL* db = nullptr;
  MYSQL_STMT* stmt = nullptr;
  MYSQL_BIND bind[16];

  std::string sql_insert = "INSERT INTO messages(`severity`,`level`,`timestamp`,`hostname`,`rolename`,`pid`,`username`,`system`,`facility`,`detector`,`partition`,`run`,`errcode`,`errline`,`errsource`,`message`) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)";

  const char* db_user = "";
  const char* db_pwd = "";
  const char* db_host = "localhost";
  const char* db_db = "INFOLOGGER";

  db = mysql_init(db);
  if (db == nullptr) {
    return -1;
  }

  if (!mysql_real_connect(db, db_host, db_user, db_pwd, db_db, 0, nullptr, 0)) {
    fprintf(stderr, "DB connect failed\n");
    return -1;
  }

  stmt = mysql_stmt_init(db);
  if (stmt == nullptr) {
    return -1;
  }

  fprintf(stderr, "query = %s\n", sql_insert.c_str());

  if (mysql_stmt_prepare(stmt, sql_insert.c_str(), sql_insert.length())) {
    return -1;
  }

  memset(bind, 0, sizeof(bind));
  bind[0].buffer_type = MYSQL_TYPE_STRING;
  bind[1].buffer_type = MYSQL_TYPE_LONG;
  bind[2].buffer_type = MYSQL_TYPE_DOUBLE;
  bind[3].buffer_type = MYSQL_TYPE_STRING;
  bind[4].buffer_type = MYSQL_TYPE_STRING;
  bind[5].buffer_type = MYSQL_TYPE_LONG;
  bind[6].buffer_type = MYSQL_TYPE_STRING;
  bind[7].buffer_type = MYSQL_TYPE_STRING;
  bind[8].buffer_type = MYSQL_TYPE_STRING;
  bind[9].buffer_type = MYSQL_TYPE_STRING;
  bind[10].buffer_type = MYSQL_TYPE_STRING;
  bind[11].buffer_type = MYSQL_TYPE_LONG;
  bind[12].buffer_type = MYSQL_TYPE_LONG;
  bind[13].buffer_type = MYSQL_TYPE_LONG;
  bind[14].buffer_type = MYSQL_TYPE_STRING;
  bind[15].buffer_type = MYSQL_TYPE_STRING;

  my_bool param_isnull = 1;
  my_bool param_isNOTnull = 0;

  const char* m_severity = "I";
  int m_level = 1;
  double m_timestamp = time(NULL);
  const char* m_hostname = "localhost";
  const char* m_rolename = "";
  int m_pid = getpid();
  const char* m_username = getlogin();
  const char* m_system = "FLP";
  const char* m_facility = "infoLogger";
  const char* m_detector = "TST";
  const char* m_partition = "";
  int m_run = 12345;
  int m_errcode = 0;
  int m_errline = __LINE__;
  const char* m_errsource = "this file";
  char m_message[256] = "This is a test message";

  bind[0].buffer = (void*)m_severity;
  bind[1].buffer = &m_level;
  bind[2].buffer = &m_timestamp;
  bind[3].buffer = (void*)m_hostname;
  bind[4].buffer = (void*)m_rolename;
  bind[5].buffer = &m_pid;
  bind[6].buffer = (void*)m_username;
  bind[7].buffer = (void*)m_system;
  bind[8].buffer = (void*)m_facility;
  bind[9].buffer = (void*)m_detector;
  bind[10].buffer = (void*)m_partition;
  bind[11].buffer = &m_run;
  bind[12].buffer = &m_errcode;
  bind[13].buffer = &m_errline;
  bind[14].buffer = (void*)m_errsource;
  bind[15].buffer = (void*)m_message;

  for (int i = 0; i <= 15; i++) {
    bind[i].is_null = &param_isNOTnull;
    bind[i].buffer_length = 0;
  }

  bind[0].buffer_length = strlen((char*)bind[0].buffer);
  bind[3].buffer_length = strlen((char*)bind[3].buffer);
  bind[4].buffer_length = strlen((char*)bind[4].buffer);
  //bind[4].is_null = &param_isnull;
  bind[6].buffer_length = strlen((char*)bind[6].buffer);
  bind[7].buffer_length = strlen((char*)bind[7].buffer);
  bind[8].buffer_length = strlen((char*)bind[8].buffer);
  bind[9].buffer_length = strlen((char*)bind[9].buffer);
  bind[10].buffer_length = strlen((char*)bind[10].buffer);
  bind[14].buffer_length = strlen((char*)bind[14].buffer);
  bind[15].buffer_length = strlen((char*)bind[15].buffer);

  if (mysql_stmt_bind_param(stmt, bind)) {
    return -1;
  }

  fprintf(stderr, "inserting %d messages (group in transactions of %d)\n", count, transactNMsg);

  for (int i = 0; i < count; i++) {

    if (transactNMsg > 0) {
      if (mysql_query(db, "START TRANSACTION")) {
        return -1;
      }
      for (int j = 0; j < transactNMsg; j++) {
        snprintf(m_message, 256, "This is test message number %d", i);
        bind[15].buffer_length = strlen((char*)bind[15].buffer);

        if (mysql_stmt_execute(stmt)) {
          fprintf(stderr, "insert failed:\n%s\n", mysql_error(db));
          break;
        }
        i++;
      }
      if (mysql_query(db, "COMMIT")) {
        return -1;
      }
    } else if (transactNMsg == 0) {
      snprintf(m_message, 256, "This is test message number %d", i);
      bind[15].buffer_length = strlen((char*)bind[15].buffer);
      if (mysql_stmt_execute(stmt)) {
        fprintf(stderr, "insert failed:\n%s\n", mysql_error(db));
        break;
      }
    } else {
      snprintf(m_message, 256, "This is test message number %d", i);
      printf("%s\t%d\t%f\t%s\t%s\t%d\t%s\t%s\t%s\t%s\t%s\t%d\t%d\t%d\t%s\t%s\n",
             m_severity, m_level, m_timestamp, m_hostname, m_rolename, m_pid, m_username,
             m_system, m_facility, m_detector, m_partition, m_run, m_errcode, m_errline, m_errsource, m_message);
      // rolename NULL -> \\N
    }
  }

  mysql_stmt_close(stmt);
  mysql_close(db);

  fprintf(stderr, "done\n");
  return 0;
}

