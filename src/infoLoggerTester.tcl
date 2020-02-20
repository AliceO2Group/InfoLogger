#!/usr/bin/tclsh

# This tool tests the infologger message pipeline
# API -> infoLoggerD -> infoLoggerServer -> DB + online clients

puts "Testing infoLogger log messages pipeline...\n"

set now [clock seconds]
set testString [format  "Test message %X : $now" [expr int(rand()*1000000000)]]

# flag to notify errors
set isError 0
# list with description of errors
set errorTxt {}

proc addError {errmsg} {
  global isError
  global errorTxt
  incr isError
  lappend errorTxt $errmsg
}

if {[catch {

  set configFile "/etc/infoLogger.cfg"
  catch {
    set configFile $env(INFOLOGGER_CONFIG)
  }
  set x 0
  while {[set opt [lindex $argv $x]] != ""} {
   switch -exact -- $opt {
     -z {
          set configFile [lindex $argv [expr $x + 1]]
          incr x   
     }
   }
   incr x
  }
 
  set configName "\[default config\]"
  set default_db_user ""
  set default_db_pwd ""
  set default_db_host ""
  set default_db_db ""
  set default_loghost "localhost"
  set default_logport "6102"

  set configFileSection "\[infoBrowser\]"
  set configFileSectionFound 0
  if {$configFile!=""} {

    puts -nonewline "Test config file:              "
  
    set fd [open $configFile "r"]
    set keyfound {}
    while {1} {
      gets $fd line
      if {[eof $fd]} {break}
      # remove leading/trailing blanks
      set line [string trim $line]
      if {$line==$configFileSection} {
	# entering infoBrowser section
	set configFileSectionFound 1
      } elseif {[regexp {^\[.*\]} $line]} {
	# entering another section
	set configFileSectionFound 0
      }
      if {!$configFileSectionFound} {
	continue
      }
      set lv [split [string trim $line] "="]
      set lkey [lindex $lv 0]
      if {[string range $lkey 0 1]=="#"} {continue}
      set lv [string trim [join [lrange $lv 1 end] "="]]

      lappend keyfound $lkey
      set cfgvals($lkey) $lv
    }
    close $fd

    set cfgok 1
    foreach {keyname isoptionnal varname defval} [list \
      dbUser 0 db_user "$default_db_user" \
      dbPassword 0 db_pwd "$default_db_pwd" \
      dbHost 0 db_host "$default_db_host" \
      dbName 0 db_db "$default_db_db" \
      serverHost 1 loghost "$default_loghost" \
      serverPortTx 1 logport "$default_logport" \
      configName 1 configName "$configFile" \
    ] {
      set $varname $defval
      if {[catch { set $varname $cfgvals($keyname) }]} {   
	if {!$isoptionnal} {
          addError "Configuration variable $keyname undefined"
          set cfgok 0
	}
      }
    }  
    if {!$cfgok} {
      addError "Wrong configuration in $configFile, exiting"
      break
    }
    
    puts "ok"
    
  } else {
    puts -nonewline "Test environment :             "
    
    set envok 1
    foreach {envname isoptionnal varname defval} [list \
      INFOLOGGER_MYSQL_USER 1 db_user "$default_db_user" \
      INFOLOGGER_MYSQL_PWD 1 db_pwd "$default_db_pwd" \
      INFOLOGGER_MYSQL_HOST 1 db_host "$default_db_host" \
      INFOLOGGER_MYSQL_DB 1 db_db "$default_db_db" \
      INFOLOGGER_SERVER_HOST 1 loghost "${default_loghost}" \
      INFOLOGGER_SERVER_PORT_TX 1 logport "${default_logport}" \
    ] {
      set $varname $defval
      if {[catch { set $varname $env($envname) }]} {      
	if {!$isoptionnal} {
          addError "Environment variable $envname undefined"
          set envok 0
	}
      }
    }

    if {!$envok} {
      addError "Wrong environment, exiting"
      break
    }
    
    puts "ok"
  }

  # connect database
  puts -nonewline "Test database access :         "
  set db -1
  if [ catch {package require mysqltcl} ] {
    addError "Package mysqltcl required"
  }
  if [ catch {set db [mysql::connect -host "$db_host" -user $db_user -password $db_pwd -db $db_db]} err ] {
    addError "connect failed"
  }
  if {$db!=-1} {
    puts "ok"
  } else {
    puts "failed"
  }

  # check infoLoggerD is listening
  puts -nonewline "Test infoLoggerD socket :      "
  set nInfoLoggerD 0
  catch {set nInfoLoggerD [exec netstat -l -p  | grep @infoLoggerD | wc -l]}
  if {$nInfoLoggerD==1} {
    puts "ok"
  } else {
    puts "failed"
    addError "infoLoggerD local socket not found"
  }

  # check infoLoggerServer is listening
  puts -nonewline "Test infoLoggerServer socket : "
  set server_fd -1
  if {[catch {set server_fd [socket "$loghost" "$logport"]} err]} {
    puts "failed"
    incr isError
    lappend errorTxt "Failed to connect to infoLoggerServer online socket ($loghost:$logport): $err"
  } else {
    puts "ok"
    fconfigure $server_fd -blocking false   
  }

  # inject message
  puts -nonewline "Test message injection:        "
  set status 0
  if {[catch {exec /opt/o2-InfoLogger/bin/log -s Debug -oFacility=test "$testString"} results]} {
   if {[lindex $::errorCode 0] eq "CHILDSTATUS"} {
      set status [lindex $::errorCode 2]
   } else {
      set status "$results"
   }
  }
  if {$status==0} {
    puts "ok"
  } else {
    puts "failed"
    incr isError
    lappend errorTxt "Message injection failed: $status"
    break
  }
    
  # wait a bit
  after 1500 { set timeout 1}
  vwait timeout
  
  # query DB for message
  puts -nonewline "Test database query:           "
  set imsg [mysql::escape $testString]
  set query "SELECT count(*) from messages where timestamp>=${now} and message='$imsg'"
  set r [mysql::sel $db $query -list]
  if {$r==1} {
    puts "ok"
  } else {
    puts "failed"
    addError "Message not found in DB"
  }
  mysql::endquery $db

  # query online messages
  puts -nonewline "Test online server query:      "  
  if {$server_fd!=-1} {
    set found 0
    while {1} {
      set nc [gets $server_fd line]
      if {$nc<=0} {
	break
      }
      if {[string first $testString $line]>=0} {
        set found 1
	break
      }
    }
    if {$found} {
      puts "ok"
    } else {
      puts "failed"
      addError "Message not found in infoLoggerServer online socket"
    }
  }

} err]} {
  puts "failed"
  addError "Exception : $err"
}

if {$isError} {
  puts "\nSome errors were found:"
  foreach e $errorTxt {
    puts "   - $e"
  }    
  exit -1
}

puts "\nAll tests successful"

exit 0
