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

/// \file log.cxx
/// \brief Command-line utility to inject log messages in infoLogger
///
///
/// \author Sylvain Chapeland, CERN

#include "InfoLogger/InfoLogger.hxx"
#include "Common/LineBuffer.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

using namespace AliceO2::InfoLogger;

void print_usage()
{
  printf("InfoLogger command line utility to inject messages.\n");
  printf("Usage: log [options] message(s)\n");
  printf("Options: \n");
  printf("  -s [severity]    Possible values: Info (default), Error, Fatal, Warning, Debug.\n");
  printf("  -o[key]=[value]  Set a message field. Valid keys: context (Facility, Role, System, Detector, Partition, Run) and message options (Severity, Level, ErrorCode, SourceFile, SourceLine).\n");
  printf("  -x               If set, read data coming on stdin line by line.\n");
  printf("                   and transmit them as messages (1 line = 1 message).\n");
  printf("  -f [file]        Same as -x, but from a file.\n");
  printf("  -c               When -f selected, create a FIFO (mkfifo) to read from. It is removed after use.\n");
  printf("  -l               When -f selected, loop over file continuously.\n");
  printf("  -v               Verbose mode (file operations, etc).\n");
  printf("  -h               This help.\n");
}

// reads an input string option, and sets accordingly msgOptions or msgContext variable.
// returns 0 on success, -1 on error
int processOption(const char* option, InfoLogger::InfoLoggerMessageOption& msgOptions, InfoLoggerContext& msgContext)
{
  std::string s(option);
  size_t separatorPosition = s.find('=');
  if (separatorPosition == std::string::npos) {
    return -1;
  }
  s[separatorPosition] = 0;
  const char* key = s.c_str();
  const char* value = &(option[separatorPosition + 1]); // use input string instead of s, so that it's persistent

  InfoLoggerContext::FieldName contextField;
  if (InfoLoggerContext::getFieldNameFromString(key, contextField) == 0) {
    return msgContext.setField(contextField, value);
  } else {
    return InfoLogger::setMessageOption(key, value, msgOptions);
  }

  return 0;
}

// find process id on other side of input pipe, if any.
// this may not work if process has already exited.
// returns PID, or zero if not found.
pid_t findPipeProcess()
{
  pid_t thePid = 0;

  // is stdin a pipe?
  pid_t myPid = getpid();
  char stdinPath[PATH_MAX];
  snprintf(stdinPath, sizeof(stdinPath), "/proc/%d/fd/0", (int)myPid);
  struct stat stdinStat;
  if (lstat(stdinPath, &stdinStat) == 0) {
    // should be a symlink
    if (S_ISLNK(stdinStat.st_mode)) {
      char ofd[PATH_MAX];
      // read link name
      int i = readlink(stdinPath, ofd, sizeof(ofd));
      if (i > 0) {
        ofd[i] = 0; // set NUL-terminated string
        // we could check if name looks like:  pipe:[231550]
        // but let's not restrict, it can work also for sockets, /dev/pts, etc

        // browse process list to find other side process
        // which has a file descriptor open which is also a symlink, matching pipe name we use as stdin
        DIR* dp;
        struct dirent* ep;
        chdir("/proc");
        dp = opendir("./");
        if (dp != NULL) {
          while ((ep = readdir(dp))) {
            int p = 0;
            p = atoi(ep->d_name);
            if (p == myPid) {
              continue;
            }
            if (p != 0) {
              DIR* pid_dp;
              struct dirent* pid_ep;
              struct stat b;
              char fp[1024];
              snprintf(fp, sizeof(fp), "/proc/%d/fd", p);
              pid_dp = opendir(fp);
              if (pid_dp != NULL) {
                while ((pid_ep = readdir(pid_dp))) {
                  char fp2[1024];
                  snprintf(fp2, sizeof(fp2), "/proc/%d/fd/%s", p, pid_ep->d_name);
                  if (lstat(fp2, &b) == 0) {
                    if (S_ISLNK(b.st_mode)) {
                      char ofd2[1024];
                      int i;
                      i = readlink(fp2, ofd2, sizeof(ofd2));
                      if (i > 0) {
                        ofd2[i] = 0; // set NUL-terminated string
                        if (!strcmp(ofd2, ofd)) {
                          thePid = p;
                          break;
                        }
                      }
                    }
                  }
                  if (thePid) {
                    break;
                  }
                }
                closedir(pid_dp);
              }
            }
            if (thePid) {
              break;
            }
          }
          closedir(dp);
        }
      }
    }
  }
  return thePid;
}

int main(int argc, char** argv)
{

  int optFromFile = 0; // 1 if logging from a file (stdin or pipe), 0 when taking input from command line
  int inputFd = -1; // file descriptor used for input, if any
  std::string inputPath = ""; // path to file to read from, if any
  bool createFifo = 0; // if set, create a FIFO
  bool loopInput = 0; // if set, loop on file (useful for fifo mode)
  bool verbose = 0; // if set, prints messages about file operations

  InfoLogger::InfoLoggerMessageOption msgOptions = InfoLogger::undefinedMessageOption;
  msgOptions.severity = InfoLogger::Severity::Info;
  InfoLoggerContext msgContext;

  // by default, use log context of parent process
  msgContext.refresh(getppid());

  char option;

  // read options
  while ((option = getopt(argc, argv, "s:xo:hf:clv")) != -1) {
    switch (option) {

      case 's': {
        InfoLogger::Severity severity = InfoLogger::Severity::Undefined;
        severity = InfoLogger::getSeverityFromString(optarg);
        if (severity == InfoLogger::Severity::Undefined) {
          printf("Bad severity\n");
          return -1;
        }
        msgOptions.severity = severity;
      } break;

      case 'x': {
        optFromFile = 1;
        inputFd = fileno(stdin);
        // try to find process sending messages to stdin
        pid_t pid = findPipeProcess();
        if (pid != 0) {
          msgContext.refresh(pid);
        }
      } break;

      case 'o':
        if (processOption(optarg, msgOptions, msgContext)) {
          printf("Can not parse option %s\n", optarg);
          return -1;
        }
        break;

        /*
// old options
      case 'z':
        configFile=optarg;
        break;
*/

      case 'c': {
        createFifo = 1;
      } break;

      case 'l': {
        loopInput = 1;
      } break;

      case 'v': {
        verbose = 1;
      } break;

      case 'f': {
        inputPath = optarg;
	optFromFile = 1;
      } break;

      case 'h':
        print_usage();
        return 0;

      default:
        print_usage();
        return -1;
    }
  }

  std::unique_ptr<InfoLogger> theLog;
  try {
    theLog=std::make_unique<InfoLogger>();
  }
  catch(int err) {
    printf("Failed to initialize infoLogger: exception %d\n",err);
    return -1;
  }
  

  // additionnal args = messages to send
  int i;
  for (i = optind; i < argc; i++) {
    theLog->log(msgOptions, msgContext, "%s", argv[i]);
  }

  // todo: catch exceptions

  // also read from file if option set */
  if (optFromFile) {

    if (createFifo) {
      bool isOk = 0;
      int err = 0;
      if (mkfifo(inputPath.c_str(), 0666)) {
        err = errno;
	if (err == EEXIST) {
	  // is the existing file a fifo ?
	  struct stat statbuf;
	  if (stat(inputPath.c_str(), &statbuf) == 0) {
	    if (S_ISFIFO(statbuf.st_mode)) {
	      // the FIFO already exists
	      isOk = 1;
	    }
	  }
	}
      } else {
        if (verbose) printf("FIFO %s created\n", inputPath.c_str());
        isOk = 1;
      }
      if (!isOk) {
        printf("Failed to create FIFO %s : %s\n", inputPath.c_str(), strerror(err));
        return -1;
      }
    }

    for (;;) {

      if (inputFd < 0) {
	inputFd = open(inputPath.c_str(), O_RDONLY);
	if (inputFd < 0) {
	  printf("Failed to open %s : %s\n", inputPath.c_str(), strerror(errno));
	  return -1;
	}
	if (verbose) printf("Input file opened = %d\n",inputFd);
      }

      LineBuffer lb; // buffer for input lines
      std::string msg;
      int eof;

      // read lines from stdin until EOF, and send them as log messages
      for (;;) {
	eof = lb.appendFromFileDescriptor(inputFd, -1);
	for (;;) {
          if (lb.getNextLine(msg)) {
            // no line left
            break;
          }
          //infoLogger_msg_xt(UNDEFINED_STRING,UNDEFINED_INT,UNDEFINED_INT,facility,severity,level,msg);
          theLog->log(msgOptions, msgContext, "%s", msg.c_str());
	}
	if (eof)
          break;
      }

      if (inputFd>0) {
        if (verbose) printf("Input file closed\n");
	close(inputFd);
	inputFd = -1;
      }

      if (!loopInput) {
        break;
      }
    }

  }

  // remove fifo after use
  if (createFifo) {
    if (unlink(inputPath.c_str())) {
      printf("Failed to remove %s\n", inputPath.c_str());
    } else {
      if (verbose) printf("FIFO %s removed\n", inputPath.c_str());
    }
  }

  return 0;
}

