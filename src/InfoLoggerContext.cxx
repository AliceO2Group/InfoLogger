// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

#include <sys/types.h>
#include <pwd.h>

#include <InfoLogger/InfoLogger.hxx>

using namespace AliceO2::InfoLogger;

InfoLoggerContext::InfoLoggerContext()
{
  // initialize and update fields
  reset();
  refresh();
}

InfoLoggerContext::InfoLoggerContext(const std::list<std::pair<FieldName, const std::string&>>& listOfKeyValuePairs) : InfoLoggerContext()
{
  // members initialization has been delegated to InfoLoggerContext() in declaration
  // now set fields provided as arguments
  setField(listOfKeyValuePairs);
}

InfoLoggerContext::~InfoLoggerContext()
{
}

void InfoLoggerContext::reset()
{
  facility.clear();
  role.clear();
  system.clear();
  detector.clear();
  partition.clear();
  run = -1;

  processId = -1;
  hostName.clear();
  userName.clear();
}

void InfoLoggerContext::refresh()
{

  // clear existing info
  reset();

  // PID
  processId = (int)getpid();

  // host name
  char hostNameTmp[256] = "";
  if (!gethostname(hostNameTmp, sizeof(hostNameTmp))) {
    // take short name only, not domain
    char* dotptr;
    dotptr = strchr(hostNameTmp, '.');
    if (dotptr != NULL)
      *dotptr = 0;
    hostName = hostNameTmp;
  }

  // user name
  struct passwd* passent;
  passent = getpwuid(getuid());
  if (passent != NULL) {
    userName = passent->pw_name;
  }

  // read from runtime environment other fields
  // todo: or from other place...

  const char* confEnv = nullptr;
  confEnv = getenv("O2_ROLE");
  if (confEnv != NULL) {
    role = confEnv;
  }
  confEnv = getenv("O2_SYSTEM");
  if (confEnv != NULL) {
    system = confEnv;
  }
  confEnv = getenv("O2_DETECTOR");
  if (confEnv != NULL) {
    detector = confEnv;
  }
  confEnv = getenv("O2_PARTITION");
  if (confEnv != NULL) {
    partition = confEnv;
  }
  confEnv = getenv("O2_RUN");
  if (confEnv != NULL) {
    run = atoi(confEnv);
    if (run <= 0) {
      run = -1;
    }
  }
}

void InfoLoggerContext::refresh(pid_t pid)
{

  // if zero, use current process info
  if (pid == 0) {
    refresh();
    return;
  }

  // clear existing info
  reset();

  // PID
  processId = (int)pid;

  // host name - this is local
  char hostNameTmp[256] = "";
  if (!gethostname(hostNameTmp, sizeof(hostNameTmp))) {
    // take short name only, not domain
    char* dotptr;
    dotptr = strchr(hostNameTmp, '.');
    if (dotptr != NULL)
      *dotptr = 0;
    hostName = hostNameTmp;
  }

  /*
  // user name
  userName.clear();
  struct passwd *passent;
  passent = getpwuid(getuid());
  if(passent != NULL){
    userName=passent->pw_name;
  }

  // read from runtime environment other fields
  // todo: or from other place...

  const char* confEnv=nullptr;
  confEnv=getenv("O2_ROLE");
  if (confEnv!=NULL) {role=confEnv;}
  confEnv=getenv("O2_SYSTEM");
  if (confEnv!=NULL) {system=confEnv;}
  confEnv=getenv("O2_DETECTOR");
  if (confEnv!=NULL) {detector=confEnv;}
  confEnv=getenv("O2_PARTITION");
  if (confEnv!=NULL) {partition=confEnv;}
  confEnv=getenv("O2_RUN");
  if (confEnv!=NULL) {
    run=atoi(confEnv);
    if (run<=0) {
      run=-1;
    }
  }  
  */
}

int InfoLoggerContext::setField(FieldName key, const std::string& value)
{
  if (key == FieldName::Facility) {
    facility = value;
  } else if (key == FieldName::Role) {
    role = value;
  } else if (key == FieldName::System) {
    system = value;
  } else if (key == FieldName::Detector) {
    detector = value;
  } else if (key == FieldName::Partition) {
    partition = value;
  } else if (key == FieldName::Run) {
    run = -1;
    if (value.length() == 0) {
      // blank value is valid -> leave field unset
      return 0;
    }
    // check input string is a valid run number
    int v_run = atoi(value.c_str());
    if (v_run <= 0) {
      return -1;
    }
    run = v_run;
  } else {
    return -1;
  }
  return 0;
}

int InfoLoggerContext::setField(const std::list<std::pair<FieldName, const std::string&>>& listOfKeyValuePairs)
{
  int numberOfErrors = 0;

  // iterate input list to set corresponding context fields
  for (auto& kv : listOfKeyValuePairs) {
    if (setField(kv.first, kv.second)) {
      numberOfErrors++;
    }
  }

  return numberOfErrors;
}

int InfoLoggerContext::getFieldNameFromString(const std::string& input, FieldName& output)
{
  const char* key = input.c_str();

  if (!strcmp(key, "Facility")) {
    output = InfoLoggerContext::FieldName::Facility;
  } else if (!strcmp(key, "Role")) {
    output = InfoLoggerContext::FieldName::Role;
  } else if (!strcmp(key, "System")) {
    output = InfoLoggerContext::FieldName::System;
  } else if (!strcmp(key, "Detector")) {
    output = InfoLoggerContext::FieldName::Detector;
  } else if (!strcmp(key, "Partition")) {
    output = InfoLoggerContext::FieldName::Partition;
  } else if (!strcmp(key, "Run")) {
    output = InfoLoggerContext::FieldName::Run;
  } else {
    return -1;
  }
  return 0;
}
