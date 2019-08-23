# InfoLogger

This package provides means to inject, collect, store, and display log messages
from processes.


## Architecture

The InfoLogger system consists of several components:

* libInfoLogger.so: C and C++ client APIs to create and inject log messages. It features printf-like
formatting or c++ stream syntax.
* log: a command line tool to inject log messages e.g. from shell scripts, or to redirect program output to InfoLogger.
* infoLoggerD: a daemon running in the background and collecting
all messages on the local node. They are then transported to a central server.
* infoLoggerServer: a daemon collecting centrally the messages sent by
remote infoLoggerD processes. Messages are stored in a MySQL database. It also serves the infoBrowser online clients.
* infoBrowser: a GUI to display online log messages as they arrive on the central server, or to query
them from the database. Messages can be filtered based on their tags.
* infoLoggerAdminDB: a command line tool to create, archive, clean or destroy
the logging database content.


![infoLogger architecture](infoLogger_architecture.png "Components of InfoLogger system")



## Installation

Installation described here is for standard [CERN CentOS 7 (CC7)](http://linux.web.cern.ch/linux/centos7/) operating system.
It was tested fine on machines installed with the default 'desktop recommended setup' selected during CERN PXE-boot setup phase.
Some minor tweaks might be needed on different systems.


Infologger RPM packages can be installed through yum repository.
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
mysql in the infoLoggerServer.service file, the default one being located in
/usr/lib/systemd/system/)

  * Setup package:
     `yum install -y mariadb-server`
  * Configure service (start now and at boot time):
     ```
     systemctl start mariadb.service
     systemctl enable mariadb.service
     ```
     
  * Create empty database and accounts for InfoLogger (interactive script, defaults should be fine):
     `/opt/o2-InfoLogger/bin/newMysql.sh`
  * Create InfoLogger configuration, and fill with DB access parameters returned by previous script:
     `vi /etc/infoLogger.cfg`

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
    `/opt/o2-InfoLogger/bin/infoLoggerAdminDB -c create`

* Configure services to start now and at boot time:
    ```
    systemctl enable infoLoggerServer.service
    systemctl enable infoLoggerD.service
    systemctl start infoLoggerServer.service
    systemctl start infoLoggerD.service
    ```

   Default InfoLogger service files for systemd can be found in /usr/lib/systemd/system/ if need to be edited.

* Check status of services:
    ```
    systemctl status infoLoggerServer.service
    systemctl status infoLoggerD.service
    ```


For systems with several nodes, only infoLoggerD and infoBrowser (if necessary) need to be configured (as above) on all nodes. The infoLoggerServer and SQL database
are needed only on one node. Appropriate settings should be defined in infoLoggerD configuration to specify infoLoggerServer host, so that messages can be
collected centrally.


## Usage
* Start infoBrowser:
  `/opt/o2-InfoLogger/bin/infoBrowser &`
    * When launched, it goes in "online" mode, i.e. it connects to the infoLoggerServer and displays messages in real time.
    * To browse previously stored messages, click the green "online" button (to exit online mode), fill-in selection filters, and push "query".
    * Detailed usage of infoBrowser can be found in the historical [ALICE DAQ documentation](https://alice-daq.web.cern.ch/operations/infobrowser).
      The interface has not changed.

* Log a test message from command line:
  `/opt/o2-InfoLogger/bin/log test`
    * See command line options with `/opt/o2-InfoLogger/bin/log -h`
    * It can be used to redirect output of a process to InfoLogger with e.g. `myProcess | /opt/o2-InfoLogger/bin/log -x`.
    * This utility uses pid/username of parent process to identify message source. In case of pipe redirection, it detects pid/username of process injecting logs, for a better tagging of messages.
    * Most message fields can be explicitely set with the -o command line option.      

* Archive table of messages and create fresh one:
  `/opt/o2-InfoLogger/bin/infoLoggerAdminDB -c archive`
    * Archived messages can still be accessed from infoBrowser through the
    Archive menu.
    * See other administrative commands possible with `/opt/o2-InfoLogger/bin/infoLoggerAdminDB -h`


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
  g++ myLog.cpp -I/opt/o2-InfoLogger/include/ -lInfoLogger-standalone -L/opt/o2-InfoLogger/lib -Wl,-rpath=/opt/o2-InfoLogger/lib
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
  gcc myLog.c -I/opt/o2-InfoLogger/include/ -lInfoLogger-standalone -L/opt/o2-InfoLogger/lib -Wl,-rpath=/opt/o2-InfoLogger/lib
  ./a.out
  ```

 * More information on the API can be found in the headers /opt/o2-InfoLogger/include and the corresponding
 documentation generated by Doxygen.
 
 * Some example calls are available in [the source code](/test/testInfoLogger.cxx)
 
 * There is the possibility to easily redirect FairLogger messages (see InfoLoggerFMQ.hxx) and process stdout/stderr to infologger (see InfoLogger.hxx setStandardRedirection())
   without changing the code from where they are issued. Although practical to get all output in InfoLogger, the messages captured this way are injected with a reduced number of tags,
   so it is recommended to use the native InfoLogger API to inject messages whenever possible.
 
 * The tags associated to a message consist of the following fields (which may be left undefined):
   * Severity: the kind of message, one of: Info (default), Error, Fatal, Warning, Debug
   * Level: the relative visibility of the message and associated target audience to whom is addressed the message: from 1 (important, visible to all) to 99 (low-level debug message, for experts only).
      For ALICE Run 2, the following ranges were used to categorize messages in DAQ/ECS: operations (1-5) support (6-10) developer (11-20) debug (21-99).
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
   


## Configuration

* Description and example of parameters for each InfoLogger component can be found in /opt/o2-InfoLogger/etc/*.cfg.
  The parameters can usually be mixed in a single configuration file, as they are grouped in sections ([infoLoggerServer], [infoLoggerD], [infoBrowser], [client] ...).

* Path to configuration file can be defined as startup parameter for the infoLoggerServer and infoLoggerD daemons. Processes using the client library can also be
  configured by defining the INFOLOGGER_CONFIG environment variable, pointing to the selected config file. The path should be in the form: "file:/etc/infoLogger.cfg"

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

  Behavior of infoLogger library can be configured with INFOLOGGER_MODE environment variable.
  This defines where to inject messages at runtime.
  Possible values are:
  * infoLoggerD = inject messages to infoLogger system, through infoLoggerD process (this is the default mode). If infoLoggerD is not found, an error message is printed on stderr and further logs are output to stdout.
  * stdout = print messages to stdout/stderr (severity error and fatal)
  * file = print messages to a file. By default, "./log.txt". Specific file can be set with e.g. INFOLOGGER_MODE=file:/path/to/my/logfile.txt
  * none = messages are discarded
  
  During development phase, it can be useful to set mode to "stdout", to allow using the infoLogger interface
  and printing messages without infoLoggerD/infoLoggerServer.
  When "infoLoggerD" mode is selected and no infoLoggerD connection can be established, the mode falls back to "stdout".

* infoLoggerD local socket
  
  The connection between a client process using infoLogger API and the local infoLoggerD is done through a UNIX named socket.
  By default, it uses a Linux abstract socket name.
  On systems where such sockets are not available, one can configure it to a named socket associated with the filesystem.
  This can be done by specifying in infoLoggerD configuration the rxSocketPath parameter, and name it with a path starting with '/', e.g. '/tmp/infoLoggerD.socket'.
  The same value has to be set for the client configuration, in the txSocketPath key.
  User is responsible to ensure that file access permissions are configured properly.
