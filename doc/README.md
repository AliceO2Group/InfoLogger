# InfoLogger

The InfoLogger package provides means to inject, collect, store, and display log messages from processes running on multiple machines.

The following executables, presented with the _nicknames_ used below, are part of the InfoLogger package:

  - o2-infologger-log or _log_: command line tool to inject log messages e.g. from shell scripts, or to redirect program output to InfoLogger. The preferred way to inject data is using directly one of the InfoLogger libraries.
  - o2-infologger-daemon or _infoLoggerD_: a daemon process collecting all local messages and sending them to _infoLoggerServer_.
  - o2-infologger-server or _infoLoggerServer_: the central process collecting, archiving, and distributing messages from multiple hosts.
  - o2-infologger-browser or [_infoBrowser_](infoBrowser.md): native GUI to display log messages in real time or from database archive. Messages can be filtered based on their tags.
  - o2-infologger-admindb or _infoLoggerAdminDB_: to maintain the logging database, i.e. create, archive, clean or destroy the database content.
  - o2-infologger-newdb : helper script for the initial set-up of the logging database, in particular for the definition of access credentials.
  - o2-infologger-tester : a tool to check the logging chain, from injection to DB storage and online subscription.

The following libraries are also provided, to inject logs into the system:

  - libO2Infologger (static and dynamic versions), for C and C++. Provided with include files and ad-hoc CMake exports.
  - Language-specific libraries, for the support of Tcl, Python, Go, and nodejs languages.
  
There are also some InfoLogger internal test components, not used in normal runtime conditions, for development and debugging purpose (o2-infologger-test-*).
The source code repository is <https://github.com/AliceO2Group/InfoLogger>.


## Architecture

![infoLogger architecture](infoLogger_architecture.png "Components of InfoLogger system")



## Installation

Installation is usually done as part of the [FLP Suite](https://alice-o2-project.web.cern.ch/flp-suite).

The procedure described here can be used to setup a standalone InfoLogger system.
It shows the steps involved for a standard [CERN CentOS 7 (CC7)](http://linux.web.cern.ch/linux/centos7/) operating system.
It was tested fine on machines installed with the default 'desktop recommended setup' selected during CERN PXE-boot setup phase.
Some minor tweaks might be needed on different systems.

Infologger RPM packages can be installed through a yum repository (e.g. the [O2/FLP yum repository](https://alice-flp.docs.cern.ch/Advanced/KB/yum/))
At the moment, everything is bundled in a single RPM named o2-InfoLogger-standalone, containing all components described above.
To quickly test the infoLogger API, the installation of this RPM is enough, the InfoLogger library will use stdout as fallback if
the central services are not available, so one can play with basic functionality without setting up the full infoLoggerD/infoLoggerServer/database chain.


Here is the complete procedure of an example installation for a full-feature standalone setup,
on a single node (commands executed as root).


* Install InfoLogger RPM:
   * from a local RPM file: `yum install -y ./o2-InfoLogger-standalone*.rpm`      
   * or from a remote YUM repo: `yum install -y o2-InfoLogger-standalone`

* Install a MySQL database. Here we use mariadb, as it's the standard one available in base CC7
install, but other MySQL versions would work just as fine (just make sure to replace mariadb by
mysql in the o2-infologger-server.service file, the default one being located in
/usr/lib/systemd/system/)

  * Setup package:
     `yum install -y mariadb-server`
  * Configure service (start now and at boot time):
     ```
     systemctl start mariadb.service
     systemctl enable mariadb.service
     ```
     
  * Create empty database and accounts for InfoLogger (interactive script, defaults should be fine):
     `/opt/o2-InfoLogger/bin/o2-infologger-newdb`
  * Create InfoLogger configuration, and fill with DB access parameters returned by previous script:
     `mkdir -p /etc/o2.d/infologger/
      vi /etc/o2.d/infologger/infoLogger.cfg`

      ```
      [infoLoggerServer]
      dbUser=infoLoggerServer
      dbPassword=uUPrVIY7
      dbHost=localhost
      dbName=INFOLOGGER

      [admin]
      dbUser=infoLoggerAdmin
      dbPassword=eSxUzzlZ
      dbHost=localhost
      dbName=INFOLOGGER

      [infoBrowser]
      dbUser=infoBrowser
      dbPassword=iEJ4w4Yl
      dbHost=localhost
      dbName=INFOLOGGER
      ```

    See also notes below in the "Configuration" section for custom configuration of each component (not necessary for this standalone setup example).

* Create an empty message table in database for infoLoggerServer:
    `/opt/o2-InfoLogger/bin/o2-infologger-admindb -c create`

* Configure services to start now and at boot time:
    ```
    systemctl enable o2-infologger-server.service
    systemctl enable o2-infologger-daemon.service
    systemctl start o2-infologger-server.service
    systemctl start o2-infologger-daemon.service
    ```

   Default InfoLogger service files for systemd can be found in /usr/lib/systemd/system/ if need to be edited.

* Check status of services:
    ```
    systemctl status o2-infologger-server.service
    systemctl status o2-infologger-daemon.service
    ```


For systems with several nodes, only infoLoggerD and infoBrowser (if necessary) need to be configured (as above) on all nodes. The infoLoggerServer and SQL database
are needed only on one node. Appropriate settings should be defined in infoLoggerD configuration to specify infoLoggerServer host, so that messages can be
collected centrally.


## Usage
* Start infoBrowser:
  `/opt/o2-InfoLogger/bin/o2-infologger-browser &`
    * When launched, it goes in "online" mode, i.e. it connects to the infoLoggerServer and displays messages in real time.
    * To browse previously stored messages, click the green "online" button (to exit online mode), fill-in selection filters, and push "query".
    * Detailed usage of infoBrowser can be found in the [separate document](infoBrowser.md).

* Log a test message from command line:
  `/opt/o2-InfoLogger/bin/o2-infologger-log test`
    * See command line options with `/opt/o2-InfoLogger/bin/o2-infologger-log -h`
    * It can be used to redirect output of a process to InfoLogger with e.g. `myProcess | /opt/o2-InfoLogger/bin/o2-infologger-log -x`.
    * This utility uses pid/username of parent process to identify message source. In case of pipe redirection, it detects pid/username of process injecting logs, for a better tagging of messages.
    * Most message fields can be explicitely set with the -o command line option.      

* Archive table of messages and create fresh one:
  `/opt/o2-InfoLogger/bin/o2-infologger-admindb -c archive`
    * Archived messages can still be accessed from infoBrowser through the
    Archive menu.
    * See other administrative commands possible with `/opt/o2-InfoLogger/bin/o2-infologger-admindb -h`


## API for developers

The InfoLogger library allows to inject messages directly from programs, as shown in the examples below.

* Compile a sample program using InfoLogger library:
  * (C++ 14):
  `vi myLog.cpp`
  ```
   #include "InfoLogger/InfoLogger.hxx"
   using namespace AliceO2::InfoLogger;
   int main()
   {
     InfoLogger theLog;

     theLog.log("infoLogger message test");
     return 0;
   }
   ```

  ```
  g++ myLog.cpp -I/opt/o2-InfoLogger/include/ -lO2Infologger -L/opt/o2-InfoLogger/lib -Wl,-rpath=/opt/o2-InfoLogger/lib
  ./a.out
  ```

  * (C):
  `vi myLog.c`
  ```
   #include "InfoLogger/InfoLogger.h"
   int main() {
     InfoLoggerHandle logH;
     infoLoggerOpen(&logH);
     infoLoggerLog(logH, "infoLogger message test");
     return 0;
   }
  ```
  ```
  gcc myLog.c -I/opt/o2-InfoLogger/include/ -lO2Infologger -L/opt/o2-InfoLogger/lib -Wl,-rpath=/opt/o2-InfoLogger/lib
  ./a.out
  ```

 * More information on the API can be found in the headers /opt/o2-InfoLogger/include and the corresponding
 documentation generated by Doxygen.
 
 * When using the C++ API, it is recommended to use the macros defined in InfoLoggerMacros.hxx to have the full context defined (severity, level, errno, source file/line) for each message in a compact way.
 
 * Some example calls are available in the source code: [1](/example/exampleLog.cxx) [2](/test/testInfoLogger.cxx)
 
 * There is the possibility to easily redirect FairLogger messages (see InfoLoggerFMQ.hxx) and process stdout/stderr to infologger (see InfoLogger.hxx setStandardRedirection())
   without changing the code from where they are issued. Although practical to get all output in InfoLogger, the messages captured this way are injected with a reduced number of tags,
   so it is recommended to use the native InfoLogger API to inject messages whenever possible.
 
 * The tags associated to a message consist of the following fields (which may be left undefined):
   * Severity: the kind of message, one of: Info (default), Error, Fatal, Warning, Debug
   * Level: the relative visibility of the message and associated target audience to whom is addressed the message: from 1 (important, visible to all) to 99 (low-level debug message, for experts only). The following ranges are defined to categorize messages for a typical production environment: operations (1-5) support (6-10) developer (11-20) trace (21-99). Trace messages are usually not enabled in normal running conditions, and relate to debugging activities (also akin of the 'Debug' severity).
   * Timestamp: time at which message was injected (Unix time in seconds since 1.1.1970). Precision goes to the microsecond.
   * Hostname: the name of the machine where the message is issued. For concision, the domain name is excluded from this tag, only the base IP host name is kept.
   * Rolename: the role name associated to the entity running the process. The same host might run several different roles. Example role name: FLP-TPC-1, EPN-223.
   * PID: the system process ID injecting the message.
   * Username: the system user name running the process injecting the message.
   * System: the name of the system running the process. In Run 2, it would be ECS,DAQ,HLT,TRG, ... To be adapted/defined for Run 3.
   * Facility: the name of the module/library injecting the message. It should give an indication about the component issuing the message or feature associated to it, e.g. readout, dataSampling, monitoring, ...
   * Detector: the 3-letter code of the detector associated to the message, e.g. TPC, MCH, TRI, ...
   * Partition: the name of the partition running the process, e.g. PHYSICS_1. To be adapted to run 3 depending on control system.
   * Run: the run number associated to the process. To be adapted to run 3 depending on control system.
   * ErrorCode: an error code associated to the message. Useful. A unique O2-wide namespace should be used to allocate error code ranges to different components, so that they are unique
   and can be associated to documentation. An error code can also be associated to an information or other severity message, to give (if necessary) operators details about a given message.
   * ErrorSource: the name of the file from which the message is injected (meaningful in case of native c/c++ interface calls).
   * ErrorLine: the source line number from which message is injected. Useful to trace back low-level debug messages.
   * Message: the message content itself. Free text. Multiple-line messages are split at each end-of-line. Some characters are not allowed and replaced (\* and \#).
   
   It is important to set the fields as precisely as possible, so that messages can be searched and filtered efficiently.
   The initial list of fields has been taken from the Run 2 vocabulary. It may be adapted, in particular depending on the implementation and naming conventions of the control system.
   
   The fields are grouped in different categories.
   * The base 'message' field. It's the message content, and needs to be explicitely specified for each message.
   * The timestamp is set automatically with sub-second precision at time of message creation, and never needs to be specified.
   * The 'dynamic' fields, which are likely different and unique from one message to the other, are grouped in a 'messageOptions' struct which can be specified for each message. It includes:
     Severity, Level, ErrorCode, ErrorSource, ErrorLine. There are macros defined to help concisely set them in the C++ interface function calls.
   * The 'static' fields, usually stable for the lifetime of the process, are grouped in a 'Context' object. These fields are automatically set from runtime environment.
     Some of them can be explicitely redefined by user: Facility, Role, System, Detector, Partition, Run.
     Some of them can not be redefined by user: PID, Hostname, Username.
     A default context can be defined for all messages, but it can also be specified on a message-by-message base.
     The context can be explicitely refreshed when necessary, in case of runtime environment has changed.
     At the moment, the following environment variables are used as input: O2_ROLE, O2_SYSTEM, O2_DETECTOR, O2_PARTITION, O2_RUN... but it will be adapted to the conventions of the run control system when
     defined.
      
    Usually, one will only take care of defining the per-message messageOptions struct and a context with appropriate Facility field set, all other being set automatically.
 
 * On the server side, messages are stored in a database, consisting of a single "messages" table (and possibly some archived versions of this table).
   The reference for the data structure is the table description itself, with one column per message tag, trivially matching the list of tags described above.
   Fields which are undefined appear as NULL (unless the library automatically sets a value for them).
   String fields usually have a maximum size (and are truncated if exceeding this limit, typically 32 characters).
   
 * InfoLogger library is also available for: Tcl, Python, Go, Node.js.
   It allows to log message from scripting languages. A simplified subset of the InfoLogger C++ API is made available through SWIG-generated modules.
   Details of the functions accessible from the scripting interface are provided in [a separate document](scriptingAPI.md).

 * C++ API provides a mean to automatically control / reduce the verbosity of some log messages.
   Auto-mute occurs when a defined amount of messages using the same token in a period of time is exceeded.
   The token parameter defines the limits for this maximum verbosity (max messages + time interval), and keeps internal count of usage.
   * the time interval starts on the first message, and is reset on the next message after an interval completed. (it is not equal to "X seconds before last message").
   * when the number of messages counted in an interval exceeds the threshold, auto-mute triggers ON: next messages (with this token) are discarded.
   * when auto-mute is ON, one message is still printed for each time interval, with statistics about the number of discarded messages. The logging rate is effectively limited to a couple of messages per interval.
   * the auto-mute triggers OFF when there was no message received for a duration equal to the interval time. (this is equal to "X seconds before last message").
   


## Configuration

* Description and example of parameters for each InfoLogger component can be found in /opt/o2-InfoLogger/etc/o2.d/infologger/*.cfg.
  The parameters can usually be mixed in a single configuration file, as they are grouped in sections ([infoLoggerServer], [infoLoggerD], [infoBrowser], [client] ...).

* Path to configuration file can be defined as startup parameter for the infoLoggerServer and infoLoggerD daemons. Processes using the client library can also be
  configured by defining the O2_INFOLOGGER_CONFIG environment variable, pointing to the selected config file. The path should be in the form: "file:/etc/o2.d/infologger/infoLogger.cfg"

* On multiple-hosts systems, the serverHost configuration key should be set for infoLoggerD and infoBrowser, so that they are able to connect infoLoggerServer
  if not running locally.
  
* On the infoLoggerServer host, the firewall should allow incoming connections
on ports 6006 (infoLoggerD), 6102 (infoBrowser) and 3306 (MySQL). It can be
achieved on CentOS 7 with e.g. (as root):
  ```
   firewall-cmd --permanent --zone=public --add-port=6006/tcp
   firewall-cmd --permanent --zone=public --add-port=6102/tcp
   firewall-cmd --permanent --zone=public --add-port=3306/tcp
   firewall-cmd --reload
   ```

* Client library

  Behavior of infoLogger library can be configured with O2_INFOLOGGER_MODE environment variable.
  This defines where to inject messages at runtime.
  Possible values are:
  * infoLoggerD = inject messages to infoLogger system, through infoLoggerD process (this is the default mode). If infoLoggerD is not found, an error message is printed on stderr and further logs are output to stdout.
  * stdout = print messages to stdout/stderr (severity error and fatal)
  * file = print messages to a file. By default, "./log.txt". Specific file can be set with e.g. O2_INFOLOGGER_MODE=file:/path/to/my/logfile.txt
  * none = messages are discarded
  
  During development phase, it can be useful to set mode to "stdout", to allow using the infoLogger interface
  and printing messages without infoLoggerD/infoLoggerServer.
  When "infoLoggerD" mode is selected and no infoLoggerD connection can be established, the mode falls back to "stdout".
  
  There is a built-in protection in the logging API to avoid message floods. If a client tries to send more than 500 messages in one second, or more than 1000 messages in one minute, further messages are redirected to a local file in /tmp. When the number of messages in this overflow file exceeds 1000 (or if can't be created), further messages are dropped. Normal behavior resumes when the message rate is reduced below 10 messages per minute. Some warning messages are logged by the library itself when this situation occurs.
  
  Constructor of the InfoLogger accepts an optional string parameter, used to override some defaults.
  The same settings can also be defined using the O2_INFOLOGGER_OPTIONS environment variable.
    
  The order of evaluation is done in this order: constructor options, environment variable options, configuration file.
  Each step may overwrite what was set by the previous one.
  
  The option string consists of a comma-separated list of key=value pairs, e.g "param1=value1, param2=value2".
  The configuration parameters accepted in this option string are:
   - outputMode: the main output mode of the library. As accepted by O2_INFOLOGGER_MODE. Default: infoLoggerD.
   - outputModeFallback: the fallback output mode of the library. As accepted by O2_INFOLOGGER_MODE. Default: stdout. The fallback mode is selected on initialization only (not later at runtime), if the main mode fails on first attempt.
   - verbose: 0 or 1. Default: 0. If 1, extra information is printed on stdout, e.g. to report the selected output.
   - floodProtection: 0 or 1. Default: 1. Enable(1)/disable(0) the message flood protection.



* infoLoggerD local socket
  
  The connection between a client process using infoLogger API and the local infoLoggerD is done through a UNIX named socket.
  By default, it uses a Linux abstract socket name.
  On systems where such sockets are not available, one can configure it to a named socket associated with the filesystem.
  This can be done by specifying in infoLoggerD configuration the rxSocketPath parameter, and name it with a path starting with '/', e.g. '/tmp/infoLoggerD.socket'.
  The same value has to be set for the client configuration, in the txSocketPath key.
  User is responsible to ensure that file access permissions are configured properly.
  
* infoLoggerD local cache

  infoLoggerD stores messages in a persistent local file until messages are successfully transmitted and acknowledged by infoLoggerServer.
  When infoLoggerServer is unavailable for a long time, messages may accumulate locally. They will all be transmitted to infoLoggerServer when available again.
  In some cases, one may want to delete this local cache.
  This can be configured on startup of infoLoggerD by one of the following ways:
  - in the infoLoggerD configuration section, set: `msgQueueReset=1` (this is permanent, done on each startup of infoLoggerD, which might not be what you want)
  - when starting infoLoggerD process from the command line (not with the systemctl service), add option: `-o msgQueueReset=1`
  - create a file named [msgQueuePath].reset (by default, msgQueuePath=/tmp/infoLoggerD/infoLoggerD.queue), e.g. `touch /tmp/infoLoggerD/infoLoggerD.queue.reset`. This will reset the queue on next startup (by hand or with e.g. service infoLoggerD restart), and the reset file will also be deleted (which ensures cleanup is done once only).
