# infoLogger

This package provides means to inject, collect, store, and display log messages
from processes.

## Architecture (picture TBD)

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



## Installation

## Configuration

## References

## Support
