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


using namespace AliceO2::InfoLogger;

void print_usage(){
  printf("InfoLogger command line utility to inject messages.\n");
  printf("Usage: log [options] message(s)\n");
  printf("Options: \n");
  printf("  -s [severity]    Possible values: Info (default), Error, Fatal, Warning, Debug.\n");
  printf("  -x               If set, reads data coming on stdin line by line\n");
  printf("                   and transmit them as messages (1 line = 1 message).\n");
  printf("  -h               This help.\n");
}



int main(int argc, char **argv){
  
  int optFromStdin=0; // 1 if logging from stdin, 0 when taking input from command line

  InfoLogger::Severity severity=InfoLogger::Severity::Info;

  char option;

  /* read options */
  while((option = getopt(argc, argv, "s:xh")) != -1){
    switch(option){

      case 's':
        severity=getSeverityFromString(optarg);
        if (severity==InfoLogger::Severity::Undefined) {
          printf("Bad severity\n");
          print_usage();
          return -1;
        }
        break;

      case 'x':
        optFromStdin=1;
        break;
      
/*
// old options
      case 'f': 
        facility=optarg;
        break;


      case 'l':
        level=infoLogger_getLevelFromString(optarg);
        if (level==UNDEFINED_INT) {
          printf("Bad level\n");
          print_usage();
          return -1;
        }
        break;
      
      case 'd':
        detector=optarg;
        break;
      
      case 'p':
        partition=optarg;
        break;
      

      case 'c':
        errcode=0;
        errcode=atoi(optarg);
        if (errcode<=0) {
          printf("Bad error code\n");
          print_usage();
          return -1;
        }
        break;
      
      case 'o':
        extraconfig=optarg;
        break;

      case 'z':
        configFile=optarg;
        break;
*/
          
      case 'h':
        print_usage();
        return 0;
          
      default:
        print_usage();
        return -1;
    }
  }

  InfoLogger theLog;

   /* additionnal args = messages to send */
  int i;
  for (i=optind; i < argc; i++) {
     theLog.log(severity,"%s",argv[i]);
  }

// todo: catch exceptions

    /* also read from stdin if option set */
  if (optFromStdin) {
    LineBuffer lb;  // buffer for input lines
    std::string msg;
    int  eof;

    // read lines from stdin until EOF, and send them as log messages
    for(;;) {
      eof=lb.appendFromFileDescriptor(fileno(stdin),-1);
      for(;;) {
        if (lb.getNextLine(msg)) {
          // no line left
          break;
        }
        //infoLogger_msg_xt(UNDEFINED_STRING,UNDEFINED_INT,UNDEFINED_INT,facility,severity,level,msg);
        theLog.log(severity,"%s",msg.c_str());
      }
      if (eof) break;
    }
  }

  return 0;
}
