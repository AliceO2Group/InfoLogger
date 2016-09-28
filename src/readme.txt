O2 logging facility
*******************

Implementation of a logging service based on ALICE DAQ Infologger.



*** Architecture

- one collector process per node (infoLoggerD = infoLoggerReader)
  local caching of messages
  overflow protection (per client?)
  listen to pipe
- one central server collecting data from readers (infoLoggerServer)
  backends: MySQL, file, ...
  automatic grouping/collapsing of similar messages
- a client API to inject logs (c, c++)
- a GUI (infoBrowser)

- Flexible list of fields (single file definition?)
- Interface to collect numbers? ("monitoring-like")


  
*** Classes

InfoLoggerMessageHelper.h          Utility class to help using the infoLog_msg_t type (e.g. convert to text (different modes), set field, etc)
InfoLoggerMessageHelper.cxx

InfoLoggerClient.h                 Class to communicate with local infoLoggerD process
InfoLoggerClient.cxx

InfoLogger.cxx	                   Implementation of the logging API
infoLoggerD.cxx                    Local daemon collecting logs
infoLoggerServer.cxx		   Central daemon collecting logs

testInfoLoggerClient.cxx           perf test for client->local infoLoggerD connection
readme.txt	  	    



- Legacy code                        Mostly unchanged, some small language/declaration fixes to be ported back for c++ compatibility

infoLoggerMessage.h                   Defines message format (which fields) and structure
infoLoggerMessage.c
transport_cache.h
transport_cache.c
transport_proxy.h
transport_proxy.c
transport_server.h
transport_server.c
transport_files.h
transport_files.c
utility.h
utility.c
transport_client.h
transport_client.c
simplelog.h                           Logging function - remapped to SimpleLog.h
simplelog.c                           Replacement for original simplelog.c, using SimpleLog.h interface
permanentFIFO.h
permanentFIFO.c
infoLoggerServer_insert.h
infoLoggerServer_insert_sql.c


- Moved to 'Common'

Daemon.h                              To implement a generic daemon (command line options, config, log)
Daemon.cxx
Configuration.h                       Read configuration data from file
Configuration.cxx
SimpleLog.h                           Log formatted messages to stdout/stderr/file
SimpleLog.cxx





*** Todo

- measure performance
  - encoding messages. Try different protocols: XML, custom, msgpack ...
  - comm client-infoLoggerD
  - comm infoLoggerD-Server
  - insert messages to DB
  - transport cache
  
Rename files ?
Configuration -> FileConfiguration
SimpleLog -> FileLog

Create new files (to move defined classes:
- ProcessInfo 
- new class for client/server socket? (local & remote?)

Move to utils:
- checkDirAndCreate()

Extra features
  - boost exceptions with message details (error, etc), global handler (set_terminate)
  std::exception_ptr exptr = std::current_exception();
try {
    std::rethrow_exception(exptr);
}
catch (std::exception &ex) {
    std::fprintf(stderr, "Terminated due to exception: %s\n", ex.what());
}
http://stackoverflow.com/questions/14112702/catching-boostexception-for-logging
