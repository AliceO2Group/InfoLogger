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

## next version
- added process stdout/stderr capture and redirection to infologger
