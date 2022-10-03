# InfoLogger release notes

This file describes the main feature changes for each InfoLogger released version.

## v1.3 - 05/06/2019
- ScriptingAPI: added logS() and logM() aliases for the overloaded log() functions.

## v1.3.1 - 14/06/2019
- Fix in infoBrowser level filtering to match documentation.
- Added documentation on the database structure.

## v1.3.2 - 17/06/2019
- Added systemd install target for infoLogger services, to start them at boot time.

## v1.3.3 - 26/06/2019
- Disable the javascript binding on Mac because it does not compile. 

## v1.3.4 - 13/08/2019
- node.js binding updated for Mac.
- Compatibility with mysql v8.
- Code cleanup (Warnings, clang-format, copyright).

## v1.3.5 - 06/09/2019
- added process stdout/stderr capture and redirection to infologger
- added infoLoggerTester to check full message pipeline from client to server and database.

## v1.3.6 - 31/10/2019
- added header file InfoLoggerErrorCodes.h to reference centrally the definition of error code ranges reserved specifically for each o2 module, and possibly the individual description / documentation of each error code.
- fix in infoLoggerServer threads initialization.

## v1.3.7 - 13/11/2019
- newMysql.sh: no changes done if SQL settings already ok.

## v1.3.8 - 20/02/2020
- infoLoggerTester: increased delay for query test.

## v1.3.9 - 09/03/2020
- upgrading to Common v1.4.9, providing support for log rotate daemon logs.

## v1.3.10 - 04/08/2020
- added automatic reconnection from clients to infoLoggerD

## v1.3.11 - 07/08/2020
- added helper definition for typical levels (operations, support, developer, trace)
- added macros for compact writing of log commands with all fields set (severity, level, errno, source file/line). See InfoLoggerMacros.hxx.
- sourceFile field: leading path is removed to keep only short filename.

## v1.3.12 - 31/08/2020
- fix in infoBrowser for level filter names

## v1.3.13 - 03/09/2020
- added unsetFMQLogsToInfoLogger() function, to be called before destroying infoLogger handles used for FMQ redirection.

## v1.3.14 - 02/10/2020
- added functions to discard locally (in client) messages with debug severity or high level. See filterDiscardDebug(), filterDiscardLevel() and filterReset().

## v1.3.15 - 06/10/2020
- increased width of pid column in database, for machines with pid counters exceeding the usual 32765 maximum.

## v1.3.16 - 28/10/2020
- added message flood protection.

## v1.3.17 - 24/11/2020
- added test mode for infoLoggerAdminDB: call it without arguments to check if connection to infoLogger database works.
- added infoBrowser "-admin" command-line option to enable shortcuts to archive commands in "Archive" menu.
- infoBrowser documentation updated.
- added infoBrowser "Display" menu to save/load window geometry and filters (replaces and extends the "Filters" menu). Possibily to start infoBrowser with settings loaded from file with -prefs command-line option.

## v1.3.18 - 08/01/2021
- improved DB connection failure recovery

## v1.3.19 - 22/01/2021
- added infoLoggerServer debug options
- added option to reset local message queue on infoLoggerD startup

## v1.3.20 - 01/02/2021
- infoLoggerServer:
  - bulk insert mode (transactions).
  - drop messages too long for DB.

## v1.3.21 - 08/02/2021
- infoLogger API: added optional string parameter to the constructor, for custom configuration.

## v1.3.22 - 15/02/2021
- increased test timeout for slow machines

## v1.3.23 - 09/03/2021
- added INFOLOGGER_MODE=debug. In this mode, infoLogger library prints on stdout one line per message, with a list of comma-separated field = value pairs. Undefined fields are not shown.
- added INFOLOGGER_OPTIONS environment variable, which allows overriding some of the built-in client defaults: outputMode, outputModeFallback, verbose, floodProtection.

## v2.0.0 - 06/04/2021
- doxygen doc cleanup
- Adapted to follow o2-flp conventions: renaming of executables, libraries, services, paths.

## v2.0.1 - 05/05/2021
- fix reconnect timeout in stdout fallback mode (now 5s).

## v2.1.0 - 11/05/2021
- API: added auto-mute feature for potentially verbose/repetitive messages. To be enabled message per message, using an AutoMuteToken (c++ only, see InfoLogger.hxx and testInfoLogger.cxx).

## v2.1.1 - 28/06/2021
- minor cosmetics for auto-mute feature.

## v2.2.0 - 06/10/2021
- Added option for o2-infologger-log to collect logs from a named pipe (multiple clients possible). Pipe can be created and listened to continuously. e.g. `o2-infologger-log -f /tmp/log-pipe -c -l`.
- API:
  - Added InfoLoggerContext light copy constructor, with fields overwrite. See example use in <../test/testInfoLogger.cxx>.

## v2.3.0 - 15/10/2021
- Added possibility to use an AutoMuteToken in the c++ stream operator.

## v2.4.0 - 19/10/2021
- Added getMessageCount() / resetMessageCount() to keep track of count of messages, by severity.

## v2.4.1 - 26/10/2021
- Added a cleanup/retry in case of corrupted cache of infoLoggerD, to allow immediate restart. The corrupted files are saved and available for later debugging.

## v2.4.2 - 11/01/2022
- Do not count number of messages for large tables (>5M rows) in Archive Select menu. Would otherwise be very long.

## v2.4.3 - 01/02/2022
- infoBrowser configuration: added parameter "queryLimit" to change the maximum number of messages returned by a query. Default is 10000.

## v2.4.4 - 09/05/2022
- changed algorithm of DB threads message distribution, now based on timestamp to help keeping multi-line split messages in same order.

## v2.5.0 - 17/06/2022
- added InfoLogger::filterDiscardSetFile(), to allow saving to local file the messages that would be discarded with filterDiscard(...) settings.

## v2.5.1 - 13/07/2022
- do not try to log to file if the path is empty.

## v2.5.2 - 03/10/2022
- added optional parameter to InfoLogger::filterDiscardSetFile(), to ignore debug messages when set.
