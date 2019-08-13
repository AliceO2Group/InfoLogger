
# Scripting API

This document describes the interface to be used to inject messages into the InfoLogger logging system from languages other than C or C++.

Languages currently supported are: Tcl, Python, Go, Javascript (node.js/v8).

A subset of the InfoLogger C++ API (covering most use-cases) is exported using SWIG-generated modules. This avoids the tedious work of manually writing native bindings calling the C++ library for each language (although this is still possible for the brave). InfoLogger provides them ready to use!

This documentation includes a reference of the exported functions, and language-specific example code calling the infoLogger module.

As the C++ interface uses some complex types which can not be transparently converted to all languages, we defined this simplified API with string-only parameters.  Although some overhead is added by the generalized used of strings to access the logging message structure fields (compared to the native C++ API), the performance loss is not significant for most situations. A typical machine can still log over 100000 messages per second from a script.



## Classes reference

Default constructor and destructor are not shown here.

* `class InfoLogger`

  The main class, to create a handle to the logging system. It provides the methods to inject log messages:
  * `logInfo(string message)`
    Sends a message with severity Info.
  * `logError(string message)`
    Sends a message with severity Error.
  * `logWarning(string message)`
    Sends a message with severity Warning.
  * `logFatal(string message)`
    Sends a message with severity Fatal.
  * `logDebug(string message)`
    Sends a message with severity Debug.
  * `log(message)`
    Sends a message with default severity and metadata. Alias: `logS()`.
  * `log(InfoLoggerMetadata metadata, string message)`
    Send a message with specific metadata (instead of the default, if one defined). Alias: `logM()`.
  * `setDefaultMetadata(InfoLoggerMetadata metadata)`
    Define some metadata to be used by default for all messages.
  * `unsetDefaultMetadata()`
    Reset the default metadata.


* `class InfoLoggerMetadata`

  It is used to specify extra fields associated to a log message. An object of this type can be optionnaly provided to the logging function to set some specific message fields. 
  * `setField(string key, string value)`
    Set one of the field to a specific value. An empty value reset this field to the default. List of valid fields include: *Severity, Level, ErrorCode, SourceFile, SourceLine, Facility, Role, System, Detector, Partition, Run*
  * `InfoLoggerMetadata(InfoLoggerMetadata)`
    Copy-constructor, to copy an existing InfoLoggerMetadata object to a new one.


## Example usage

Example usage is shown below. Library files listed there are available from the infoLogger library installation directory.

* **Tcl**
   * Library files: `infoLoggerForTcl.so`
   * Code example: (interactive interpreter from the command line)
	 ```
	   tclsh
	   load infoLoggerForTcl.so
	   set logHandle [InfoLogger]
	   $logHandle logInfo "This is a test"
	   $logHandle logError "Something went wrong"
	 ```      
* **Python**
  * Library files: `_infoLoggerForPython.so` and `infoLoggerForPython.py`
  * Code example: (interactive interpreter from the command line)
	```
	  python
	  import infoLoggerForPython
	  logHandle=infoLoggerForPython.InfoLogger()
	  logHandle.logInfo("This is a test")
	  logHandle.logError("Something went wrong")
	```
* **Go**
  * Library files: `infoLoggerForGo.a` and `infoLoggerForGo.go`, to be copied to your `${GOPATH}/src/infoLoggerForGo`
  * Build: `CGO_LDFLAGS="${GOPATH}/src/infoLoggerForGo/infoLoggerForGo.a -lstdc++" go build`
    If the c++ library version used at compile time is not available at runtime, you may include a static version of the lib in the executable by replacing `-lstdc++` with `/full/path/to/libstdc++.a`.
  * Code example: 
    ```
      package main
      import "infoLoggerForGo"
      func main() {
        var logHandle=infoLoggerForGo.NewInfoLogger()
        logHandle.LogInfo("This is a test")
        logHandle.LogError("Something went wrong")
      }
    ```

* **Node.js**
  * Library files: `infoLogger.node`
  * Code example: (interactive interpreter from the command line)
	```
	  node
	  var infoLoggerModule=require("infoLogger");
	  var logHandle=new infoLoggerModule.InfoLogger();
	  logHandle.logInfo("This is a test");
	  logHandle.logError("Something went wrong");
	```          
    


## Tips
Don't forget to delete the objects if necessary!
