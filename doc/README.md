# infoLogger

This package provides means to inject, collect, store, and display log messages
from processes.


## Architecture

It consists of several components:
* libInfoLogger.so: C and C++ client APIs to create and inject log messages. It features printf-like
formatting or c++ stream syntax.
* libtclConfiguration.so: Tcl binding for the client API.
* log: a command line tool to inject log messages e.g. from shell scripts.
* infoLoggerD: a daemon running in the background and collecting
all messages on the local node. They are then transported to a central server.
* infoLoggerServer: a daemon collecting centrally the messages sent by
remote infoLoggerD processes. Messages are stored in a MySQL database. It also serves the infoBrowser online clients.
* infoBrowser: a GUI to display online log messages as they arrive on the central server, or to query
them from a database. Messages can be filtered based on their tags.
* infoLoggerAdminDB: a command line tool to create, archive, clean or destroy
the logging database content.

(picture TBD)


## Installation

Installation described here is for standard CERN CentOS 7 (CC7) operating system
(http://linux.web.cern.ch/linux/centos7/).

* Infologger RPM packages can be installed through yum repository

* Client setup
  
* Server setup
  * infoLoggerServer
  * MySQL setup
    * To store messages, infoLoggerServer relies on a MySQL database.
      To install MySQL on Linux CC7:
      * Install RPMs:
      * Create database and credentials
      

* Development libs



## Configuration

* Client library
  Behavior of infoLogger library is configured with INFOLOGGER_MODE environment variable.
  This defines where to inject messages at runtime.
  Possible values are:
  * infoLoggerD = inject messages to infoLogger system
  * stdout = print messages to stdout/stderr (severity error and fatal)
  * file = print messages to a file. By default, "./log.txt". Specific file can be set with e.g. INFOLOGGER_MODE=file:/path/to/my/logfile.txt
  * none = messages are discarded
  
  During initial development phase, this is by default "stdout", to allow using the infoLogger interface
  and printing messages without infoLoggerD/infoLoggerServer. This will be changed at a later stage to infoLoggerD.

* infoLoggerD

* infoBrowser

* infoLoggerServer


## References

## Support
