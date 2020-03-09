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
