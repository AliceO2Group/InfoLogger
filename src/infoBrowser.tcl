#!/usr/bin/wish -f

################################################################
# infoBrowser
#
# This GUI allows to browse the log messages of the infoLogger system.
# command line options:
# -p partition : launch with filter on partition
# -d detector : launch with filter on detector
# -f facility : launch with filter on facility
# -l level : launch with filter on level (same string as found in select box)
# -full : launch with archive management enabled (not by default)
#
# requires tcl/tk 8.4 (panedwindow widget)
#
# 12/05/2017  SC  - Import infoBrowser from DATE 7.108 to O2 FLP prototype
#                   Protocol v1.4
#                   Updated default configuration settings to split from DATE
################################################################

# check Tcl/Tk version, it would be nice to find 8.4 or higher
set version_ok [split [info tclversion] "."]
if {[llength $version_ok]==2} {
  if {([lindex $version_ok 0]==8) && ([lindex $version_ok 1]>=4)} {
    set version_ok 1
  } else {
    set version_ok 0
  }
} else {
  set version_ok 0
}
if {!$version_ok} {
  #puts "You should use Tcl/Tk 8.4 or higher for better display"
}

# Create busy flag to avoid starting different things at the same time
# only one guy (timeformat, query, online) should edit the message lists at a time
set busy 0

# Define online server timeouts (seconds)
set onlineserver(retry) 5
set onlineserver(max) 640
set onlineserver(timeout) $onlineserver(retry)
set onlineserver(timer) ""

set maxmess 10000

set configFile ""

catch {
  set configFile $env(INFOLOGGER_CONFIG)
}

# process command line arguments (pre-init)
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

# some defaults
set wikiURLbase ""
set wikiURLerrors ""
set wikiURLquery ""
set proxyHost ""
set proxyPort 8080

# helpAvailable: -1 unknown (e.g. not tried yet) 0 not available (no wiki connection) 1 available (wiki cx ok)
set helpAvailable -1     


set defaultDir "/tmp"
set infoDir ""
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
        puts "Configuration variable $keyname undefined"
        set cfgok 0
      }
    }
  }  
  if {!$cfgok} {
    puts "Wrong configuration in $configFile, exiting"
    exit -1
  }
  
} else {

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
        puts "Environment variable $envname undefined"
        set envok 0
      }
    }
  }
   
  if {!$envok} {
    puts "Wrong environment, exiting"
    exit -1
  }
}

##############################
# init http for wiki
##############################
package require http
if {$proxyHost!=""} {
  ::http::config -proxyhost $proxyHost -proxyport $proxyPort
}


##############################
# init window
##############################
wm title . "infoBrowser - $configName"

font create filterfont -family Arial -size 9 -weight bold
font create timefont -family Arial -size 9 -weight bold
font create buttonfont -family Arial -size 9 -weight bold
font create titlefont -family Arial -size 10 -weight bold

##############################
# menu
##############################
menu .menubar -type menubar -relief groove
.menubar add command -label "Quit" -underline 0 -command {exit 0}
.menubar add cascade -label "Archive" -menu .menubar.archive -underline 0
.menubar add cascade -label "Filters" -menu .menubar.filter -underline 0 -state disabled
.menubar add cascade -label "Export" -menu .menubar.export -underline 0
menu .menubar.archive -tearoff 0
.menubar.archive add command -label "Select" -underline 0 -command select_archive
.menubar.archive add command -label "Create" -underline 0 -command create_archive -state disabled
.menubar.archive add command -label "Delete" -underline 0 -command delete_archive -state disabled
menu .menubar.filter -tearoff 0
.menubar.filter add command -label "Clear" -underline 0 -command filter_clear
.menubar.filter add command -label "Save" -underline 0 -command filter_save
.menubar.filter add command -label "Load" -underline 0 -command filter_load
menu .menubar.export -tearoff 0
.menubar.export add command -label "All messages displayed" -underline 0 -command {export_messages all}
.menubar.export add command -label "Selected messages only" -underline 0 -command {export_messages selected}

pack .menubar -fill x -expand 0


##############################
# create filter selection area
##############################
frame .select
frame .select.filter

label .select.filter.lnone    -text "" -font filterfont
label .select.filter.lonly    -text "match" -font filterfont
label .select.filter.lexclude -text "exclude" -font filterfont

frame .select.time
label .select.time.l -text "Time" -font timefont
label .select.time.lnone -text "" -font timefont
label .select.time.lstart -text "min." -font timefont
entry .select.time.vstart -width 8 -font timefont
label .select.time.lend -text "max." -font timefont
entry .select.time.vend -width 8 -font timefont

frame .select.level
label .select.level.l -text "Level" -font timefont
label .select.level.lnone -text "" -font timefont
menubutton .select.level.v -menu .select.level.v.menu -relief raised -width 10 -font timefont
menu .select.level.v.menu -tearoff 0
set llevel { "shifter" 1 "oncall" 6 "devel" 11 "debug" 21 "any" -1 "custom" -1}
foreach {slimit ilimit} $llevel {
  if {$slimit!="custom"} {
    set lcmd ".select.level.v configure -text \"$slimit\"; global vfilter_level; set vfilter_level $ilimit"
    .select.level.v.menu add command -label "$slimit" -command $lcmd
  } else {
    .select.level.v.menu add command -label "$slimit" -command {select_level}
  }
}
# procedure to select a filter level from list
proc setFilterLevelByName {lName} {
  global llevel
  global vfilter_level
  foreach {slimit ilimit} $llevel {
    if {$lName==$slimit} {
      .select.level.v configure -text "$slimit"
      set vfilter_level $ilimit
      break
    }
  }
}
#default filter level
setFilterLevelByName "any"

set filters {
  {"Severity" "severity"}
  {"Hostname" "hostname"}
  {"Rolename" "rolename"}
  {"Username" "username"}
  {"System" "system"}
  {"Facility" "facility"}
  {"Detector" "detector"}
  {"Partition" "partition"}
  {"Run" "run"}
  {"ErrCode" "errcode"}
  {"Message" "message"}
}

foreach f $filters {
  set l [lindex $f 0]
  set c [lindex $f 1]
  label .select.filter.l_$c -text $l  -font filterfont
  entry .select.filter.vin_$c -width 9  -font filterfont
  entry .select.filter.vex_$c -width 9  -font filterfont
  lappend filter_c $c
}


proc filter_clear {} {
  global filter_c
  foreach c $filter_c {
    .select.filter.vin_$c delete 0 end
    .select.filter.vex_$c delete 0 end
  }
  .select.time.vstart delete 0 end
  .select.time.vend delete 0 end

  global vfilter_level llevel
  .select.level.v configure -text [lindex $llevel 0]
  set vfilter_level [lindex $llevel 1]
}

proc filter_save {} {
  global filter_c
  global defaultDir
  
  set filename [tk_getSaveFile -initialdir $defaultDir -title "Save filters to" -defaultextension ".ibf" -filetypes {{{infoBrowser filters} {.ibf}}}]
  if {$filename != ""} {
    set fd [open "$filename" "w"]
    foreach c $filter_c {
      puts $fd ".select.filter.vin_$c [.select.filter.vin_$c get]"
      puts $fd ".select.filter.vex_$c [.select.filter.vex_$c get]"
    }
    puts $fd ".select.time.vstart [.select.time.vstart get]"
    puts $fd ".select.time.vend [.select.time.vend get]"
    close $fd
  }
}

proc filter_load {} {
  global filter_c
  global defaultDir
  set filename [tk_getOpenFile -initialdir $defaultDir -title "Load filters from"  -filetypes {{{infoBrowser filters} {.ibf}}}]
  if {$filename != ""} {
    set fd [open "$filename" "r"]
    while {1} {
      gets $fd line
      if {[eof $fd]} {break}
      set l [split $line]
      if {[llength $l] < 2} {continue}
      set w [lindex $l 0]
      set v [join [lrange $l 1 end]]
      if {[string first ".select." $w]!=0} {continue}
      eval "$w delete 0 end"
      if {[string length $v]==0} {continue}
      eval "$w insert 0 \"$v\""
    }
    close $fd    
  }
}

# update the statistics on the number of messages
# actions: reset, display
proc update_stats {action} {
  global n_msgs
  global n_msgs_war
  global n_msgs_err
  global n_msgs_fat
  global n_msgs_bad
  global n_msgs_fullQuery
  global maxmess
  
  set m {}
  if {$action=="reset"} {
    set n_msgs 0
    set n_msgs_war 0
    set n_msgs_err 0
    set n_msgs_fat 0
    set n_msgs_bad 0
    set n_msgs_fullQuery 0
    
    # no message highlighted yet
    global log_selected
    set log_selected -1
    global log_selected_max
    set log_selected_max -1
    log_highlight
    
    # clear lists
    global log_fields
    foreach item $log_fields {
      global log_val_$item
      set log_val_$item {}
    }
    
  }
  if {$n_msgs} {
    lappend m "$n_msgs messages"
    if {$n_msgs_war} {lappend m "$n_msgs_war warnings"}
    if {$n_msgs_err} {lappend m "$n_msgs_err errors"}
    if {$n_msgs_fat} {lappend m "$n_msgs_fat fatals"}
    if {$n_msgs_bad} {lappend m "$n_msgs_bad corrupted"}
  
    global maxmess
    if {$n_msgs>$maxmess} {
      lappend m " only last $maxmess displayed"
    }

    if {$n_msgs_fullQuery>$n_msgs} {lappend m " only first $n_msgs displayed (out of $n_msgs_fullQuery)"}
    if {($n_msgs_fullQuery==-1)&&($n_msgs>=$maxmess)} {lappend m " only first $n_msgs displayed"}

  } else {
    lappend m "$n_msgs message"
  }
  
  .stat_result configure -text [join $m ", "]
  update
}

proc select_level {} {
  if {[winfo exists .levelselect]} {return}

  toplevel .levelselect
  frame .levelselect.f
  label .levelselect.f.l -text "Enter custom level: "
  entry .levelselect.f.e -width 8
  button .levelselect.f.b -text "Ok" -command {
    set lv [.levelselect.f.e get]
    if {$lv!=""} {
      .select.level.v configure -text "$lv"
      global vfilter_level
      set vfilter_level $lv
    }
    destroy .levelselect
  }
  set g [split [wm geometry .] "+"]
  set x [expr [lindex $g 1] +100 ]
  set y [expr [lindex $g 2] +100 ]
  pack .levelselect.f.l .levelselect.f.e .levelselect.f.b -in .levelselect.f -fill both -expand 1
  pack .levelselect.f -fill x -expand 1

  wm geometry  .levelselect +$x+$y
  wm title .levelselect "Set level filter"
}



#############################
# archive commands
#############################

proc select_archive {} {
  if {[winfo exists .logselect]} {return}

  toplevel .logselect
  button .logselect.select -text "Select" -width 5 -command {
    global lpath
    global logfile
    set logfile [lindex $lpath [.logselect.listbox curselection]]
    destroy .logselect
  }
  frame .logselect.f
  listbox   .logselect.listbox -width 40 -height 10 -xscrollcommand ".logselect.scrollx set" -yscrollcommand ".logselect.scrolly set" -selectmode single -selectbackground yellow -selectforeground black -exportselection no 
  scrollbar .logselect.scrollx -orient horizontal -command ".logselect.listbox xview" -width 10
  scrollbar .logselect.scrolly -orient vertical -command ".logselect.listbox yview" -width 10
    
  global defaultlogfile

  set nl 0
    set nl [mysqlquery "select count(*) from $defaultlogfile"]

  .logselect.listbox insert end "Latest logs                  (${nl} messages)"
  global lpath
  set lpath {}
  lappend lpath $defaultlogfile

    set files [lsort -decreasing [mysqlquery "show tables"]]
    foreach f $files {
      if {$f == $defaultlogfile} {continue}
      set n [file tail $f]
      set s [split $n "_"]
      if {[llength $s] != 9} {continue}
      set d "[lindex $s 4]/[lindex $s 3]/[lindex $s 2] [lindex $s 6]:[lindex $s 7]:[lindex $s 8]"
      set nl [mysqlquery "select count(*) from $f"]
      .logselect.listbox insert end "$d     (${nl} messages)"
      lappend lpath $f
    }
  
  
  # select the previously selected item
  global logfile
  set index [lsearch $lpath $logfile]
  if {$index<0} {set index 0}
    
  .logselect.listbox selection set $index
  
  set g [split [wm geometry .] "+"]
  set x [expr [lindex $g 1] +100 ]
  set y [expr [lindex $g 2] +100 ]
  pack .logselect.listbox -side left -in .logselect.f -fill both -expand 1
  pack .logselect.scrolly -side left -in .logselect.f -fill y
  pack .logselect.f -fill x -expand 1
  pack .logselect.scrollx -fill x
  pack .logselect.select

  wm geometry  .logselect +$x+$y
  wm title .logselect "Select log"
}

proc create_archive {} {
  global env
  if {[winfo exists .boxmsg]} {return}
  toplevel .boxmsg
  label .boxmsg.l -text "Creating archive ..." -width 30
  set g [split [wm geometry .] "+"]
  set x [expr [lindex $g 1] +100 ]
  set y [expr [lindex $g 2] +100 ]
  wm geometry  .boxmsg +$x+$y
  wm title .boxmsg "Archive"
  pack .boxmsg.l -padx 10 -pady 10
  update
  global infoDir
  exec $infoDir/newDateLogs.sh
  .boxmsg.l configure -text "Creating archive ... done!"
  after 1000 {set done 1}
  vwait done
  destroy .boxmsg
}

proc delete_archive {} {
  global env
  if {[winfo exists .boxmsg]} {return}
  toplevel .boxmsg
  label .boxmsg.l -text "Are you sure you want to delete \n latest (unarchived) messages ?" -width 50
  set g [split [wm geometry .] "+"]
  set x [expr [lindex $g 1] +100 ]
  set y [expr [lindex $g 2] +100 ]
  wm geometry  .boxmsg +$x+$y
  wm title .boxmsg "Archive"
  button .boxmsg.yes -text "Yes" -width 5 -command {
    destroy .boxmsg.yes
    destroy .boxmsg.no
    .boxmsg.l configure -text "Deleting messages ..." -width 50
    update
    global infoDir
    exec $infoDir/newDateLogs.sh -d
    .boxmsg.l configure -text "Deleting messages ... done!"
    after 1000 {set done 1}
    vwait done
    destroy .boxmsg
  }
  button .boxmsg.no -text "No" -width 5 -command {destroy .boxmsg}
  pack .boxmsg.l -padx 10 -pady 10
  pack .boxmsg.yes .boxmsg.no -side left -fill x -expand 1 -padx 10 -pady 10
}


proc getList {s result} {
  upvar $result r
  set r {}
  set val [$s get]
  if {[catch {set r [eval "list $val"]} err]} {
    tk_messageBox -message "Wrong argument $val\n\n$err" -title "Error" -type ok
    return 1
  }
  return 0
}

proc browseErrors {step} {
  global log_selected
  global log_selected_max
  global n_msgs
  global log_val_Severity
  
  set j0 -1
  if {($log_selected>=0) && ($log_selected<$n_msgs)} {
    set j0 $log_selected
  }

  set i0 0
  set s 1
  switch $step {
    "first" {
      set i0 0
      set s 1
    }
    "next" {
      set i0 [expr $j0 + 1]
      set s 1
    }
    "previous" {
      if {$j0>=0} {
        set i0 [expr $j0 - 1]
      } else {
        set i0 [expr $n_msgs - 1]
      }
      set s -1
    }
    "last" {
      set i0 [expr $n_msgs - 1]
      set s -1
    }
  }

  set j -1
  for {set i $i0}  {($i < $n_msgs) && ($i >= 0)}  {incr i $s} {
    set v "[lindex $log_val_Severity $i]"
    if {($v == "ERROR") || ($v == "FATAL")} {
      set j $i
      break
    }
  }
  if {$j>=0} {
    set log_selected $j
    set log_selected_max $j
    log_highlight
    global log_visible_fields
    foreach item $log_visible_fields {
      .messages.pw.frame_$item.listbox see $j
    }
    return
  }
  set log_selected -1
  set log_selected_max -1
  log_highlight
  puts -nonewline "\a"
  flush stdout
}


##############################
# query window
##############################
frame .cmd
button .cmd.query -text "Query" -width 10  -padx 6 -pady 6 -command {doQuery}

checkbutton .cmd.online -text "Online" -width 10 -padx 6 -pady 6  -selectcolor "green" -command {doOnline} -indicatoron 0

frame .frview -relief groove -border 2
checkbutton .frview.autoclean -text "Auto clean" -font buttonfont -height 1 -padx 4 -pady 4 -selectcolor "gray" -state disabled -indicatoron 0
checkbutton .frview.autoscroll -text "Auto scroll" -font buttonfont -height 1 -padx 4 -pady 4 -selectcolor "gray" -state disabled -indicatoron 0 -command {if {$autoscroll} {viewEndMsg}}
set autoscroll 1
set autoclean 1
button .frview.clean -text "Clean now" -font buttonfont -height 1 -padx 4 -pady 4 -command {doClean}
label .frview.l -text "Messages: " -font buttonfont

button .frview.errFirst -text "<<" -command {browseErrors first} -font buttonfont
button .frview.errPrevious -text "<" -command {browseErrors previous} -font buttonfont
button .frview.errNext -text ">" -command {browseErrors next} -font buttonfont
button .frview.errLast -text ">>" -command {browseErrors last} -font buttonfont
label .frview.errL -text "Browse errors:" -font buttonfont

frame .stat_action -background darkgrey
frame .stat_query  -background darkgrey

label .stat_action.l -text "Status : " -background darkgrey
label .stat_action.v -text "Idle" -wraplength 800 -justify left -background darkgrey -fg black
label .stat_query.l -text "Query  : "  -background darkgrey
label .stat_query.v -text "" -wraplength 800 -justify left  -background darkgrey
label .stat_result -text "0 message"


##############################
# save messages to file
##############################

proc export_messages {which} {
  global defaultDir
  set filename [tk_getSaveFile -initialdir $defaultDir -title "Save log messages to"]
  if {$filename != ""} {
    set fd [open "$filename" "w"]

    # define print command
    global log_visible_fields
    global n_msgs

    set insert_cmd {}
    lappend insert_cmd "puts \$fd \""
    set i 0
    foreach item $log_visible_fields {
      if {$i!=0} {
        lappend insert_cmd "\\t"
      }
      lappend insert_cmd "\[lindex \$log_val_$item \$index\]"
      global log_val_$item
      incr i
    }
    lappend insert_cmd "\""
    set insert_cmd [join $insert_cmd ""]

    if {$which=="all"} {   
      for {set index 0} {$index<$n_msgs} {incr index} {
        eval $insert_cmd
      }
    } elseif {$which=="selected"} {
      global log_selected
      global log_selected_max
      for {set index $log_selected} {$index<=$log_selected_max} {incr index} {
        eval $insert_cmd
      }
    }
    close $fd
  }
}



##############################
# message display
##############################
frame .messages -borderwidth 2 -height 300

# display message with configured log fields
set log_fields {Severity Level Date Time Subsecond Host Role Pid Username System Facility Detector Partition Run ErrCode srcLine srcFile Message}
set log_fields_def_size {7 3 7 7 12 12 12 5 8 5 9 9 9 6 5 5 12 30}
set log_visible_fields {Severity Time Host Facility ErrCode Message}

# real protocol used
# number of fields
set log_protocol_nFields 16
set log_protocol_version 1.4

# One global for each field contains list of items - create empty
foreach item $log_fields {
  set log_val_$item {}
}


# this global holds the current selected message
set log_selected -1
set log_selected_max -1

# highlight selected log, defined in global variable log_selected
set n_msgs 0
set n_msgs_war 0
set n_msgs_err 0
set n_msgs_fat 0
set n_msgs_bad 0
proc log_highlight {} {
  global log_selected
  global log_selected_max
  global log_visible_fields
  global n_msgs  

  set y -1
  clipboard clear
  if {($log_selected>=0) && ($log_selected<$n_msgs)} {
    foreach item $log_visible_fields {
      if {$y==-1} {
        set y [lindex [.messages.pw.frame_$item.listbox yview] 0]
      }
      .messages.pw.frame_$item.listbox selection clear 0 end
      .messages.pw.frame_$item.listbox selection set $log_selected $log_selected_max
      .messages.pw.frame_$item.listbox yview moveto $y
    }
    # fill clipboard
    set vclip {}
    for {set i $log_selected} {$i<=$log_selected_max} {incr i} {
      set j 0
      foreach item $log_visible_fields {
          set v [.messages.pw.frame_$item.listbox get $i]
          if {$j} {lappend vclip "\t"}
          lappend vclip $v
          incr j
      }
      lappend vclip "\n"
    }
    clipboard append [join $vclip ""]    
  } else {
    foreach item $log_visible_fields {
      .messages.pw.frame_$item.listbox selection clear 0 end
    }
  }
}

# selection of a field sets the global log_selected value and update display
proc on_log_select {w} {
  global log_selected
  global log_selected_max
  set i [lindex [$w curselection] 0]
  set j [lindex [$w curselection] 0]
  foreach v [$w curselection] {
    if {$v<$i} {set i $v}
    if {$v>$j} {set j $v}
  }
  if {[string length $i]>0} {
    if {($i==$log_selected)&&($j==$log_selected_max)} {
      set log_selected -1
      set log_selected_max -1
    } else {
      set log_selected $i
      set log_selected_max $j
    }
    log_highlight
  }
}



# update log level colors
# takes fractionnal range of items to update (between 0.0 and 1.0)
set severity_visible 0
set severity_colorize 1

proc colorize_severity {start_f end_f} {
  global severity_visible
  global severity_colorize

  if {!$severity_visible} {return}
  if {!$severity_colorize} {return}

  global n_msgs
  
  if {($start_f<0)||($end_f<0)} {
    #set r [.messages.sb get]
    set r [.messages.pw.frame_Severity.listbox yview]
    if {[llength $r]!=2} {return}
    set start_f [lindex $r 0]
    set end_f [lindex $r 1]
  }
  set min [expr int(floor($n_msgs*$start_f))]
  set max [expr int(ceil($n_msgs*$end_f))]
  
  global log_val_Severity
  
  # update listbox attributes
  set i $min
  foreach s [lrange $log_val_Severity $min $max] {
    set color gray
    set txtcolor black
    switch $s {
      "WARNING" {
        set txtcolor "#FFDD00"
      }
      "ERROR" {
        set color "#FF2000"
        set txtcolor white
      }
      "FATAL" {
        set color purple
        set txtcolor white
      }
    }
    .messages.pw.frame_Severity.listbox itemconfigure $i -background $color -fg $txtcolor

# To colorize all the line:
#    global log_visible_fields
#    foreach item $log_visible_fields {
#      .messages.pw.frame_$item.listbox itemconfigure $i -background $color
#    }

    incr i
  }
}


# create the pannel with messages
# each field is displayed in a different resizable column
proc show_messages {} {
  global log_fields
  global log_visible_fields
  global log_fields_def_size
    
  if {[winfo exists .messages.pw]} {destroy .messages.pw}
  
  global version_ok
  if {!$version_ok} {
    frame .messages.pw
  } else {
    panedwindow .messages.pw  -orient horizontal -sashwidth 5 -showhandle false -sashpad 0
  }

  global severity_visible
  set severity_visible 0

  # fields to be displayed
  foreach item $log_visible_fields {
    frame .messages.pw.frame_$item 
    label .messages.pw.frame_$item.label -text "$item"
    listbox .messages.pw.frame_$item.listbox -listvariable log_val_$item -yscrollcommand ".messages.sb set" -xscrollcommand ".messages.pw.frame_$item.scroll set"  -selectmode extended -selectbackground yellow -selectforeground black -exportselection no 
    scrollbar .messages.pw.frame_$item.scroll -orient horizontal -command ".messages.pw.frame_$item.listbox xview" -width 10

    set pwidth "[expr 8*[lindex $log_fields_def_size [lsearch $log_fields $item]]]"
    if {$version_ok} {
      .messages.pw add .messages.pw.frame_$item -width "${pwidth}p"
    } else {
      if {$item=="Message"} {
        pack .messages.pw.frame_$item -side left -fill both -expand 1
      } else {
        .messages.pw.frame_$item.listbox configure -width [expr $pwidth/8]
        pack .messages.pw.frame_$item -side left -fill y -expand 0
      }
    }
    
    if {$item=="Severity"} {
      .messages.pw.frame_$item.listbox configure -yscrollcommand "_listbox_severity_update"
      set severity_visible 1
    }
    
    pack .messages.pw.frame_$item.label
    pack .messages.pw.frame_$item.listbox -fill both -expand 1
    pack .messages.pw.frame_$item.scroll -fill x
    bind .messages.pw.frame_$item.listbox <<ListboxSelect>> {on_log_select %W}
    bind .messages.pw.frame_$item.listbox <ButtonPress-3> {
        set i [%W nearest %y]
        global log_val_ErrCode
        set err [lindex $log_val_ErrCode $i]
        if {$err!=""} {
          # absolute screen position of the click
          set x [expr [winfo rootx %W] + %x]
          set y [expr [winfo rooty %W] + %y]
          display_tooltip $err $x $y
        } else {
          display_tooltip 0 0 0
        }
    }

    # following code: release tooltip on right-click release
    #bind .messages.pw.frame_$item.listbox <ButtonRelease-3> {
    #  display_tooltip 0 0 0
    #}
     
    pack .messages.pw -fill both -side left -expand 1
  }

  # highlight selected message if any
  log_highlight
}


# mousewheel bindings
bind all <Button-4> {_msgs_scroll scroll -1 page}
bind all <Button-5> {_msgs_scroll scroll +1 page}

# update in the severity area
proc _listbox_severity_update {a b} {
  # match scrollbar
  .messages.sb set $a $b
  # colorize items
  colorize_severity $a $b
}


# scrollbar and associated procedure
scrollbar .messages.sb -orient vertical -command {_msgs_scroll}
proc _msgs_scroll {args} {
  global log_fields
  global log_visible_fields

  switch [lindex $args 0] {
    scroll {
      if {[llength $args]!=3} return
      
      # realign all items (because of default mousewheel binding)
      # take the median value of the yview 
      foreach item $log_visible_fields {
        lappend a [lindex [.messages.pw.frame_$item.listbox yview] 0]
      }
      set a [lsort $a]
      set a [lindex $a [expr int([llength $a]/2)]]
      foreach item $log_visible_fields {
        .messages.pw.frame_$item.listbox yview moveto $a
      }
              
      foreach item $log_visible_fields {
        .messages.pw.frame_$item.listbox yview scroll [lindex $args 1] [lindex $args 2] 
      }
    }
    moveto {
      if {[llength $args]!=2} return
      foreach item $log_visible_fields {
        .messages.pw.frame_$item.listbox yview moveto [lindex $args 1]
      }
    }
  }
}
show_messages

# options
frame .msg_options

# number of decimals for timestamp
label .msg_options.subsec_l -text " decimals"
set subsecond_decimal 0
set subsecond_decimal_l 0
entry .msg_options.subsec_v -width 1 -textvariable subsecond_decimal -validate all -vcmd {
  if {"%S"!="{}"} {
    if {("%S"<0)|| "%S">6} {
      after idle {set subsecond_decimal 0}
    } else {
      after idle {set subsecond_decimal "%S"}
    }
  }
  if {$subsecond_decimal_l!=$subsecond_decimal} {after idle {update_time}}
  set subsecond_decimal_l $subsecond_decimal
  return 1
}
#if {$version_ok} {
#  spinbox .msg_options.subsec_v -from 0 -to 6 -increment 1 -textvariable subsecond_decimal -width 2 -state readonly
#}

# select column to be displayed
foreach item $log_fields {
  checkbutton .msg_options.check_$item -text $item -selectcolor "green"
  if {[lsearch $log_visible_fields $item]>=0} {.msg_options.check_$item select}
  .msg_options.check_$item configure -command {update_visible_fields}
  if {$item=="Subsecond"} {continue}
  pack .msg_options.check_$item -side left
  if {$item=="Time"} {
    pack .msg_options.subsec_v .msg_options.subsec_l  -side left
  }
}


pack .msg_options
proc update_visible_fields {} {
  global log_fields
  global log_visible_fields
  set log_visible_fields {}

  scan [.messages.sb get] "%f %f" pos_saved_begin pos_saved_end
  foreach item $log_fields {
    global check_$item
    set exp "set v \$check_$item"
    eval $exp
    if {$v} {lappend log_visible_fields $item} 
  }
  show_messages  
  _msgs_scroll moveto $pos_saved_begin
}


###############################
# layout
###############################
pack .messages -fill both -expand 1
pack .messages.sb -side right -fill y

pack .select -ipady 10 -fill x

pack .select.time -side left -padx 5
pack .select.level -side left -padx 15 
pack .select.filter -side left


# filters 
set l "grid .select.filter.lnone"
foreach c $filter_c {
  lappend l ".select.filter.l_$c"
}
lappend l "-in .select.filter"
eval "[join $l]"

set l "grid .select.filter.lonly"
foreach c $filter_c {
  lappend l ".select.filter.vin_$c"
}
lappend l "-in .select.filter"
eval "[join $l]"

set l "grid .select.filter.lexclude"
foreach c $filter_c {
  lappend l ".select.filter.vex_$c"
}
lappend l "-in .select.filter"
eval "[join $l]"

# time filter
grid .select.time.lnone .select.time.l
grid .select.time.lstart .select.time.vstart
grid .select.time.lend .select.time.vend
#pack .select.time.lstart .select.time.vstart .select.time.lend .select.time.vend

grid .select.level.l
grid .select.level.lnone
grid .select.level.v


pack .frview.autoclean .frview.autoscroll .frview.clean .frview.l -side right
pack .frview.errL .frview.errFirst .frview.errPrevious .frview.errNext .frview.errLast -side left
pack .frview -fill x



pack .stat_action -fill x
pack .stat_query -fill x 
pack .stat_result -fill x -side left

button .stat_searchb -text "Find" -command {
  global log_selected
  global log_selected_max
  global n_msgs
  global log_val_Message
  
  set sval [.stat_searchv get]
  if {[string length $sval>0]} {
    if {($log_selected>=0) && ($log_selected<$n_msgs)} {
      set j0 [ expr $log_selected + 1 ]
    } else {
      set j0 0
    }
    for {set i 0}  {$i < $n_msgs}  {incr i} {
      set j [expr ($i + $j0 ) % $n_msgs]
    
      if {[string first $sval [lindex $log_val_Message $j]]!=-1} {
        set log_selected $j
        set log_selected_max $j
        log_highlight
        global log_visible_fields
        foreach item $log_visible_fields {
          .messages.pw.frame_$item.listbox see $j
        }
        return
      }
    }
  }
  set log_selected -1
  set log_selected_max -1
  log_highlight
  puts -nonewline "\a"
  flush stdout
}
entry .stat_searchv -width 15
pack .stat_searchb .stat_searchv -side right

pack .stat_action.l .stat_action.v -in .stat_action -side left
pack .stat_query.l .stat_query.v -in .stat_query -side left

pack .cmd.query .cmd.online -fill x
pack .cmd -side right -padx 20 -in .select -fill x





##############################
# Database query
##############################


# Command to create tables (check in newDateLogs.sh):
# definition of message table


# connect database
if [ catch {package require mysqltcl} ] {
  puts "Error - package mysqltcl required"
  exit 1
}

if [ catch {set db [mysql::connect -host $db_host -user $db_user -password $db_pwd -db $db_db]} ] {
  puts "Error - failed to connect to database"
  exit 1
}

# this is the table for current log messages
set defaultlogfile "messages"


# set log to default (no archive selected)
set logfile $defaultlogfile



##############################
# context help (right click)
##############################

set tooltip_button_txt "Wiki: $wikiURLbase"
set tooltip_szx [font measure default "$tooltip_button_txt"]

toplevel .tooltip -bd 1 -background "lightyellow" -relief solid
frame .tooltip.fr -width 600 -height 400 -padx 10 -pady 10 -background "lightyellow"
pack .tooltip.fr -expand 1
wm geometry .tooltip 640x480

button .tooltip.fr.help -text "Wiki: $wikiURLbase" -command {
  global tooltip_err
  global wikiURLbase
  global wikiURLquery
  if {$tooltip_err>0} {
    set url "${wikiURLerrors}/${tooltip_err}"
  } else {
    set url "${wikiURLerrors}"
  }
  exec firefox "$url" &
  display_tooltip 0 0 0
}
bind .tooltip <ButtonPress-3> {
  display_tooltip 0 0 0
}
set tooltip_err 0

set w_tooltip {
"" 0 ""
"component" 1 "Component"
"description" 2 "Description"
"details" 3 "Details"
"urgency" 4 "Urgency"
"level" 5 "Difficulty"
"shifter action" 6 "Shifter action"
"notification" 7 "How to notify"
"contact" 8 "Contact"
}

set ixt 0
foreach {kn kix kl}  ${w_tooltip} {
  if {"$kl"!=""} {set kl "${kl}:"}
  if {$ixt==0} {
    set vpady 15
  } else {
    set vpady 5
  }
  label .tooltip.fr.l_${kix} -text "${kl}" -justify right -background "lightyellow" -wraplength 200 -font titlefont 
  label .tooltip.fr.v_${kix} -text "" -justify left -background "lightyellow" -wraplength 400
  if {"$kl"!=""} {
    grid .tooltip.fr.l_${kix} -column 0 -row $ixt -sticky e -padx 10 -pady $vpady
    grid .tooltip.fr.v_${kix} -column 1 -row $ixt -sticky w -padx 10 -pady $vpady
  } else {
    grid .tooltip.fr.l_${kix} -column 0 -columnspan 2 -row $ixt -sticky ew -padx 10 -ipady $vpady
  }
  incr ixt
}

grid .tooltip.fr.help -row $ixt -columnspan 2 -pady 20 -padx 20


wm state .tooltip withdrawn
wm transient .tooltip .
wm overrideredirect .tooltip 1


proc getPage { url } {
  set token [::http::geturl $url -timeout 2000]
  set data [::http::data $token]
  ::http::cleanup $token          
  return $data
}

proc clean_tooltip {} {
  # clean values
  global w_tooltip
  foreach {kn kix kl}  ${w_tooltip} {
    .tooltip.fr.v_${kix} configure -text ""
  }  
}

proc display_tooltip {errCode x y} {

  # if same tooltip already visible, close it
  global tooltip_err
  if {$errCode==$tooltip_err} {
    set errCode 0
  }
  set tooltip_err $errCode


  # close tooltip when no errcode
  if {$errCode==0} {
    # hide window
    wm state .tooltip withdrawn
    clean_tooltip
    return
  }

  global helpAvailable

  # try to connect db if not connected (and if not already unsuccessfuly tried to get errcode help)
  if {$helpAvailable!=0} {

    global wikiURLquery
    set rp ""
    set rp [getPage "${wikiURLquery}/${errCode}"]

    if {$rp!=""} {
      set helpAvailable 1
    } else {
      set helpAvailable -1
    }
  }
  set h "Online help is not available"

  clean_tooltip

  # get online help
  if {$helpAvailable==1} {

    set kvl [split $rp "|"]
    foreach kv [split $rp "|"] {
      set kv [split $kv ":"]
      set k [lindex $kv 0]
      set v [string trim [join [lrange $kv 1 end] ":"]]
  
      set helptxt ""
      global w_tooltip
      set ix [lsearch $w_tooltip $k]
      if {$ix==-1} { continue }
      set helptxt "${helptxt}\n$k\t$v"
      set kix [lindex $w_tooltip [expr $ix+1]]
      .tooltip.fr.v_${kix} configure -text "$v"
      set h "Online help for error code $errCode"
    }
  }
  
  .tooltip.fr.l_0 configure -text "$h"
  
  incr x 15
  incr y -15
  wm geometry .tooltip +${x}+${y}
  wm state .tooltip normal
}



#########################################################
# Filter handling
#########################################################

# enable filters
proc filter_on {} {
  global filter_c
  .select.time.l configure -state normal
  .select.time.lstart configure -state normal
  .select.time.vstart configure -state normal
  .select.time.lend configure -state normal
  .select.time.vend configure -state normal
  
  .select.level.l configure -state normal
  .select.level.v configure -state normal

  foreach c $filter_c {
    .select.filter.vin_$c configure -state normal
    .select.filter.vex_$c configure -state normal    
  }

  # enable filter operations
  .menubar entryconfigure 3 -state normal
}


# disable filters
proc filter_off {} {
  global filter_c
  .select.time.l configure -state disabled
  .select.time.lstart configure -state disabled
  .select.time.vstart configure -state disabled
  .select.time.lend configure -state disabled
  .select.time.vend configure -state disabled

  .select.level.l configure -state disabled
  .select.level.v configure -state disabled

  # correct state is readonly, but only available in 8.4
  global version_ok
  if {$version_ok} {
    set newstate readonly
  } else {
    set newstate disabled
  }

  foreach c $filter_c {
    .select.filter.vin_$c configure -state $newstate
    .select.filter.vex_$c configure -state $newstate
  }
  
  # disable filters operations
  .menubar entryconfigure 3 -state disabled
}





#########################################################
# Query processing
#########################################################

# query messages corresponding to the defined filter settings
set querying 0
proc doQuery {} {
  global db
  global querying
  global filter_c
  global maxmess

  global busy
  if {$busy} {return}
  set busy 1

    
  # disable query while this one is not processed
  if {$querying} {set busy 0; return}
  set querying 1
  
  # reset query
  set msgs {}
  set bad_query 0
  
  # create query based on filters
  # use good table (can be an archive)
  global logfile
  set query "SELECT `severity`,`level`,`timestamp`,`hostname`,`rolename`,`pid`,`username`,`system`,`facility`,`detector`,`partition`,`run`,`errcode`,`errline`,`errsource`,`message` from $logfile"

  # build SQL filter command    
  set sql_filters {}
  foreach field $filter_c {
    if {[getList .select.filter.vin_${field} filtered_items]} {
      incr bad_query
    }
    if {[llength $filtered_items]} {      
      set count 0
      set subfilter {}
      foreach i $filtered_items {
          if {[string length $i]<=0} {continue}
          if {$count > 0} {
            lappend subfilter "OR"
          } else {
            lappend subfilter "("
          }
            set i [mysql::escape $i]
          if {[string first "%" $i]!=-1} {
            lappend subfilter "`$field` LIKE \"$i\""
          } else {
            lappend subfilter "`$field`=\"$i\""
          }
          incr count
      }
      lappend subfilter ")"
      lappend sql_filters [join $subfilter]
    }

    if {[getList .select.filter.vex_${field} filtered_items]} {
      incr badquery 1
    }
    if {[llength $filtered_items]} {      
      set count 0
      set subfilter {}
      foreach i $filtered_items {
          if {[string length $i]<=0} {continue}
          if {$count > 0} {
            lappend subfilter "OR"
          } else {
            lappend subfilter "NOT ("
          }
            set i [mysql::escape $i]
          if {[string first "%" $i]!=-1} {
            lappend subfilter "`$field` LIKE \"$i\""
          } else {
            lappend subfilter "`$field`=\"$i\""
          }
          lappend subfilter "and `$field` IS NOT NULL"
          incr count
      }
      lappend subfilter ")"
      lappend sql_filters [join $subfilter]
    }
  }
  
  
  # start time
  .select.time.vstart configure -bg [.select.time cget -bg]
  set filter_tmin 0
  if {[.select.time.vstart get] != ""} {
    if {([scan [.select.time.vstart get] "%lf" t]==1)&&($t>1000000000)} {
      lappend sql_filters "timestamp>$t"
      set filter_tmin $t
    } elseif {![catch {set t [clock scan [.select.time.vstart get]]}]} {
      lappend sql_filters "timestamp>$t"
      set filter_tmin $t
    } else {
      .select.time.vstart configure -background red
      set bad_query 1
    }
  }
  
  # end time
  set filter_tmax 0
  .select.time.vend configure -bg [.select.time cget -bg]
  if {[.select.time.vend get] != ""} {
    if {([scan [.select.time.vend get] "%lf" t]==1)&&($t>1000000000)} {
      lappend sql_filters "timestamp<$t"
      set filter_tmax $t
    } elseif {![catch {set t [clock scan [.select.time.vend get]]}]} {
      lappend sql_filters "timestamp<$t"
      set filter_tmax $t
    } else {
      .select.time.vend configure -background red
      set bad_query 1
    }
  }

  # level
  global vfilter_level
  if ($vfilter_level>=0) {
    lappend sql_filters "level<=$vfilter_level and level is not null"
  }

  # any error?
  if {$bad_query} {
    .stat_query.v configure -text ""
    .stat_action.v configure -text "Bad query" -fg darkred
    update_stats reset
    set querying 0
    set busy 0
    return
  }

  # build the query string based on processed input  
  set i 0
  foreach filter $sql_filters {
    if {$i == 0} {
      lappend query "WHERE"
    } else {
      lappend query "AND"
    }
    lappend query "$filter"
    incr i
  }

  # count messages expected first if no filter defined
  set nGrandTotal -1
  set queryCount "SELECT count(*) from $logfile"
  set queryCount [join [concat [list $queryCount] [lrange $query 4 end]]]
if {$i==0} {
  .stat_query.v configure -text "$queryCount"
  .stat_action.v configure -text "Counting messages..." -fg black
  .stat_result configure -text ""
  update
  set nGrandTotal [mysqlquery $queryCount]
  if {$nGrandTotal > [expr 50 * $maxmess]} {
    .stat_query.v configure -text ""
    .stat_action.v configure -text "Your query would return $nGrandTotal messages! Please define more restrictive filters." -fg darkred
    update_stats reset
    set querying 0
    set busy 0
    return
  }
}  

  # order result by timestamp
  lappend query "ORDER BY timestamp"

  # limit query to maximum messages to be displayed
  lappend query "limit $maxmess"
  
  # build query
  set query [join $query]

  # update status display
  .stat_query.v configure -text "$query"
  .stat_action.v configure -text "Retrieving data..." -fg black
  .stat_result configure -text ""
  update
  
    # launch query
    set msgs [mysqlquery $query]

  # number of messages
  update_stats reset
  global n_msgs
  global n_msgs_war
  global n_msgs_err
  global n_msgs_fat
  global n_msgs_fullQuery
  set n_msgs [llength $msgs]

  # get total number of messages if count limit exceeded
  if {($n_msgs>=$maxmess)&&($nGrandTotal<0)} {
    set nGrandTotal [mysqlquery $queryCount]
  }
    
  # update status display  
  .stat_action.v configure -text "Processing data ..." -fg black

  # update stats on the number of messages
  update_stats display
  set n_msgs_fullQuery $nGrandTotal

  # some counters for status display
  set i 0
  set l [llength $msgs]
  set step [expr int($l/10)]
  if {$step==0} {set step 10}

  # variables for fast time formatting
  set lastt_val -1
  set lastt_str_d ""
  set lastt_str_t ""
  global subsecond_decimal
  set subsecond_format "%0[expr ${subsecond_decimal}+3].${subsecond_decimal}f"

  # clear lists
  global log_fields
  foreach item $log_fields {
    set l_$item {}
  }

  # process result msg by msg
  foreach item $msgs {

    # severity
      set s [lindex $item 0]

    switch $s {
    "I" {set ss "Info"}
    "W" {set ss "WARNING"; incr n_msgs_war}
    "E" {set ss "ERROR"; incr n_msgs_err}
    "D" {set ss "Debug"}
    "F" {set ss "FATAL"; incr n_msgs_fat}
    default {set ss "? $s"}
    }

    # date/time : format string only if not in same second as previous      
      set tmicro [lindex $item 2]

    set tval [expr int($tmicro)]
    if {$tval!=$lastt_val} {
      set the_clock_format [split [clock format $tval -format "%d/%m/%y %H:%M:%S %H:%M"]]
      set lastt_str_d [lindex $the_clock_format 0]
      set lastt_str_t [lindex $the_clock_format 1]
      set lastt_str_hm [lindex $the_clock_format 2]
      # expr does not like 1-digit numbers with a zero before: e.g. "09" is an invalid octal number
      set lastt_str_s [expr $tval % 60]
      set lastt_val $tval
    }

    lappend l_Severity "$ss"
    lappend l_Date $lastt_str_d
    if {$subsecond_decimal} {
      lappend l_Time [format "${lastt_str_hm}:${subsecond_format}" [expr $tmicro - floor($tmicro) + $lastt_str_s]]
    } else {
      lappend l_Time $lastt_str_t
    }
    lappend l_Subsecond $tmicro

    # other fields
    lappend l_Level [lindex $item 1]
    lappend l_Host [lindex $item 3]
    lappend l_Role [lindex $item 4]    
    lappend l_Pid [lindex $item 5]
    lappend l_Username [lindex $item 6]
    lappend l_System [lindex $item 7]
    lappend l_Facility [lindex $item 8]
    lappend l_Detector [lindex $item 9]
    lappend l_Partition [lindex $item 10]
    lappend l_Run [lindex $item 11]
    lappend l_ErrCode [lindex $item 12]
    lappend l_srcLine [lindex $item 13]
    lappend l_srcFile [lindex $item 14]
    lappend l_Message [lindex $item 15]


    # update status display
    incr i
    if {[expr $i % $step]==0} {
      .stat_action.v configure -text "Processing data ... [expr round($i*100.0/$l)]%" -fg black
      update
    }
  }


  # update status display
  .stat_action.v configure -text "Processing data ... 100%" -fg black

  # update stats on the number of messages
  update_stats display
     
  # assign values
  global log_fields
  foreach item $log_fields {
    global log_val_$item
    set a "set log_val_$item \$l_$item"
    eval $a
  }

  # update status display
  .stat_action.v configure -text "Idle" -fg black
  update

  # scroll to last entry
  _msgs_scroll moveto $n_msgs

  # colorize log levels items
  colorize_severity -1 -1
  
  # re-enable queries
  set querying 0
  
  set busy 0
}




############################################################
# create "online_filter" proc
# this new proc returns 1 if message should be discarded
############################################################
proc create_online_filter {} {

  global filter_c  
  
  set online_filter {}
  lappend online_filter "proc online_filter {} {"

  set bad_query 0

  # level
  global vfilter_level
  if ($vfilter_level>=0) {
    lappend online_filter "upvar v_level v_level"
    lappend online_filter "if {(\$v_level==\"\")||(\$v_level>$vfilter_level)} {return 1}"
  }

  
  # match filter
  foreach field $filter_c {
    incr bad_query [getList .select.filter.vin_${field} filtered_items]
    if {[llength $filtered_items]} {
      lappend online_filter "set reject 1" 
      lappend online_filter "upvar v_$field v_$field"
      # add dummy loop to break if match (OR)
      lappend online_filter "for {set k 0} {\$k==0} {incr k} {"

      set count 0
      foreach i $filtered_items {
          if {[string length $i]<=0} {continue}
          if {[string first "%" $i]!=-1} {
            set pattern [string map {% *} $i]
            lappend online_filter "if {\[string match \"$pattern\" \$v_$field \]} {set reject 0; break;}"
            incr count
          } else {
            lappend online_filter "if {\$v_$field==\"$i\"} {set reject 0; break;}"
            incr count
          }          
      }

      lappend online_filter "}"
      if {$count} {
        lappend online_filter "if {\$reject==1} {return 1}"
      }
    }
  }

  # exclude filter
  foreach field $filter_c {
    incr bad_query [getList .select.filter.vex_${field} filtered_items]
    if {[llength $filtered_items]} {
      lappend online_filter "set reject 0" 
      lappend online_filter "upvar v_$field v_$field"
      # add dummy loop to break if match (OR)
      lappend online_filter "for {set k 0} {\$k==0} {incr k} {"

      set count 0
      foreach i $filtered_items {
          if {[string length $i]<=0} {continue}
          if {[string first "%" $i]!=-1} {
            set pattern [string map {% *} $i]
            lappend online_filter "if {\[string match \"$pattern\" \$v_$field \]} {set reject 1; break;}"
            incr count
          } else {
            lappend online_filter "if {\$v_$field==\"$i\"} {set reject 1; break;}"
            incr count
          }          
      }

      lappend online_filter "}"
      if {$count} {
        lappend online_filter "if {\$reject==1} {return 1}"
      }
    }
  }


  lappend online_filter "return 0"
  lappend online_filter "}"
  set online_filter [join $online_filter "\n"]
  
  if {$bad_query} {
    .stat_query.v configure -text ""
    .stat_action.v configure -text "Bad query" -fg darkred
    update_stats reset
    return 1
  }
  eval $online_filter
  return 0
}



#################################
# display online data
#################################
set server_fd -1

# process an event on the socket
proc server_event {} {
  global server_fd
  global n_msgs
  global n_msgs_war
  global n_msgs_err
  global n_msgs_fat
  global n_msgs_bad  

  global log_fields
  global log_protocol_nFields
  global log_protocol_version

  fileevent $server_fd readable ""
  set n_loop 0
  set n_msgs_start $n_msgs
    
  # init empty fields
  foreach f $log_fields {
    set l_$f {}
  }
    
  while {1} {
    
    # connection closed?
    if {[eof $server_fd]} {
      close $server_fd
      set server_fd -1
      global onlineserver
      .stat_action.v configure -text "Connection closed. Reconnecting in $onlineserver(timeout) seconds" -fg darkred
      .cmd.online configure -selectcolor "red"
      update
      set onlineserver(timer) [after [expr $onlineserver(timeout)*1000] {server_connect}]
      break
    }

    # read and decode message
    if {[gets $server_fd msg]==-1} {break}
       
    while {1} {
      set item [split $msg "#"]
      if {[llength $item]<$log_protocol_nFields} {
    	  incr n_msgs_bad
	  break
      }
      set v1 [lindex $item 0]

     #puts "online msg=${msg}(end of msg)"

      if {[string equal $v1 "*${log_protocol_version}"]} {

        set v_severity [lindex $item 1]
        set v_level [lindex $item 2]
        set tmicro [lindex $item 3]
        set v_hostname [lindex $item 4]
        set v_rolename [lindex $item 5]  
        set v_pid [lindex $item 6]
        set v_username [lindex $item 7]
        set v_system [lindex $item 8]
        set v_facility [lindex $item 9]
        set v_detector [lindex $item 10]
        set v_partition [lindex $item 11]       
        set v_run [lindex $item 12]
        set v_errcode [lindex $item 13]
        set v_errline [lindex $item 14]
        set v_errsource [lindex $item 15]
        set v_message [join [lrange $item 16 end] "#"]

      } else {
    	  incr n_msgs_bad
	  break
      }

      # autoclean
      global autoclean
      if {$autoclean} {
	if {[string equal "$v_facility" "runControl"] && [string equal -length 26 "$v_message" "Starting processes for run" ]} {
          # clean only if start of run of selected partition
          set clean_it 0
          set nomatch 1
          foreach f [eval "list [.select.filter.vin_partition get]"] {
            if {[string length $f]<=0} {continue}
            set nomatch 0
            if {[string first "%" $f]!=-1} {
              set pattern [string map {% *} $f]
              if {[string match $pattern $v_partition]} {set clean_it 1; break;}
            } else {
              if {$v_partition==$f} {set clean_it 1; break;}
            }
          }
          if {$nomatch} {set clean_it 1}
          foreach f [eval "list [.select.filter.vex_partition get]"] {
            if {[string length $f]<=0} {continue}
            if {[string first "%" $f]!=-1} {
              set pattern [string map {% *} $f]
              if {[string match $pattern $v_partition]} {set clean_it 0; break;}
            } else {
              if {$v_partition==$f} {set clean_it 0; break;}
            }
          }
          if {$clean_it} {
            doClean
            set n_msgs_start 0
            foreach f $log_fields {
              set l_$f {}
            }
          }
	}
      }

      if {[online_filter]==1} {break}
      
      # severity
      switch $v_severity {
      "I" {set ss "Info"}
      "W" {set ss "WARNING"; incr n_msgs_war}
      "E" {set ss "ERROR"; incr n_msgs_err}
      "D" {set ss "Debug"}
      "F" {set ss "FATAL"; incr n_msgs_fat}
      default {set ss "? $v_severity"}
      }

      # date/time : could format string only if not in same second as previous -> TODO      
      set tval [expr int($tmicro)]

      global subsecond_decimal
      if {$subsecond_decimal} {
	set subsecond_format "%0[expr ${subsecond_decimal}+3].${subsecond_decimal}f"
	set the_clock_format [split [clock format $tval -format "%d/%m/%y %H:%M:%S %H:%M"]]
	set lastt_str_d [lindex $the_clock_format 0]
	set lastt_str_t [lindex $the_clock_format 1]
	set lastt_str_hm [lindex $the_clock_format 2]
	# expr does not like 1-digit numbers with a zero before: e.g. "09" is an invalid octal number
	set lastt_str_s [expr $tval % 60]     
	set lastt_str_t [format "${lastt_str_hm}:${subsecond_format}" [expr $tmicro - floor($tmicro) + $lastt_str_s]]
      } else {
	set lastt_str_d [clock format $tval -format "%d/%m/%y"]
	set lastt_str_t [clock format $tval -format "%H:%M:%S"]
      }

      # re-format multiple line messages
      foreach m [split $v_message "\f"] {

      lappend l_Severity "$ss"
      lappend l_Date $lastt_str_d
      lappend l_Time $lastt_str_t
      lappend l_Subsecond $tmicro

      # other fields
      lappend l_Level $v_level
      lappend l_Host $v_hostname
      lappend l_Role $v_rolename
      lappend l_Pid $v_pid
      lappend l_Username $v_username
      lappend l_System $v_system
      lappend l_Facility $v_facility
      lappend l_Detector $v_detector
      lappend l_Partition $v_partition
      lappend l_ErrCode $v_errcode
      lappend l_srcLine $v_errline
      lappend l_srcFile $v_errsource
      lappend l_Run $v_run
      lappend l_Message $m

      incr n_msgs

      }
      break
    }
    
    incr n_loop
    if {$n_loop==1000} {break}
  }

  if {$n_msgs>$n_msgs_start} {
    # assign values
    global log_fields
    global maxmess
    foreach item $log_fields {
      global log_val_$item
      set a "set log_val_$item \[lrange \[concat \$log_val_$item \$l_$item\] end-[expr $maxmess-1] end\]"
      eval $a
    }

    # scroll
    global autoscroll
    if {$autoscroll} {    
      viewEndMsg
    }
  }
  
  # update stats on the number of messages
  update_stats display

  
  if {$server_fd!=-1} {  
    fileevent $server_fd readable server_event
  }
}


# open socket to data server
proc server_connect {} {
  global server_fd
  global env
  global onlineserver
  
  set onlinetimer(timer) ""
    
  # connect only if not connected yet
  if {$server_fd!=-1} {return}

  global loghost
  global logport

  .stat_action.v configure -text "Connecting $loghost ..." -fg black
  update

  # open socket
  if {[catch {set server_fd [socket $loghost $logport]} err]} {
    .stat_action.v configure -text "while connecting $loghost:$logport - $err. Will retry in $onlineserver(timeout) seconds" -fg darkred
    .cmd.online configure -selectcolor "red"
    set onlineserver(timer) [after [expr $onlineserver(timeout)*1000] {server_connect}]
    if {$onlineserver(timeout)<$onlineserver(max)} {set onlineserver(timeout) [expr $onlineserver(timeout) * 2]}
    update
    return
  }
  set onlineserver(timeout) $onlineserver(retry)
  fconfigure $server_fd -blocking false   
  fconfigure $server_fd -buffersize 1000000
  fileevent $server_fd readable server_event
  
  .cmd.online configure -selectcolor "green"  
  .stat_action.v configure -text "Connected" -fg black
  
  
  update
}


proc doOnline {} {
  global online
  global server_fd

  global busy
    
  if {$online} {

    if {$busy} {return}
    set busy 1

    # Go Online

    # create filter
    filter_off
    if {[create_online_filter]} {
      .cmd.online toggle
      filter_on
      .cmd.query configure -state normal
      .frview.autoclean configure -state disabled
      .frview.autoscroll configure -state disabled
      set busy 0
      update
      return
    }

    # update status display
    set t0 [clock format [clock seconds] -format "%d/%m/%Y %H:%M:%S"]
    .stat_query.v configure -text "Online data - from $t0"
    .cmd.query configure -state disabled
    .frview.autoclean configure -state normal
    .frview.autoscroll configure -state normal
    
    # connect server
    server_connect
    
    update_stats reset
    update   
    
  } else {
    # close server
    if {$server_fd!=-1} {
      close $server_fd
      set server_fd -1
    }

    # Go Offline
    filter_on
    .cmd.query configure -state normal
    .frview.autoclean configure -state disabled
    .frview.autoscroll configure -state disabled
    .stat_action.v configure -text "Idle" -fg black
    set t0 [clock format [clock seconds] -format "%d/%m/%Y %H:%M:%S"]
    .stat_query.v configure -text "[.stat_query.v cget -text] to $t0"

    global onlineserver
    if {"$onlineserver(timer)"!=""} {
      after cancel $onlineserver(timer)
      set onlineserver(timer) ""
    }
    
    update    
    set busy 0
  }
}

proc update_time {} {
  global busy
  if {$busy} {return}
  set busy 1

  global subsecond_decimal
 
  # variables for fast time formatting
  set lastt_val -1
  global subsecond_decimal
  set subsecond_format "%0[expr ${subsecond_decimal}+3].${subsecond_decimal}f"
  
  # update time column
  set new {}
  global log_val_Subsecond

  # some counters for status display
  set i 0
  set l [llength $log_val_Subsecond]
  set step [expr int($l/10)]
  if {$step==0} {set step 10}


  if {$subsecond_decimal} {
    foreach tmicro $log_val_Subsecond {
      set tval [expr int($tmicro)]
      if {$tval!=$lastt_val} {
        set lastt_str_hm [clock format $tval -format "%H:%M"]
        set lastt_str_s  [expr $tval % 60]
      }
      lappend new [format "${lastt_str_hm}:${subsecond_format}" [expr $tmicro - floor($tmicro) + $lastt_str_s]]
      
      # update status display
      incr i
      if {[expr $i % $step]==0} {
        .stat_action.v configure -text "Processing data ... [expr round($i*100.0/$l)]%" -fg black
        update
      }
    }
  } else {
    foreach tmicro $log_val_Subsecond {
      set tval [expr int($tmicro)]
      if {$tval!=$lastt_val} {
        set lastt_str_t [clock format $tval -format "%H:%M:%S"]
      }
      lappend new $lastt_str_t
      # update status display
      incr i
      if {[expr $i % $step]==0} {
        .stat_action.v configure -text "Processing data ... [expr round($i*100.0/$l)]%" -fg black
        update
      }
    }
  }

  # update status display
  .stat_action.v configure -text "Processing data ... 100%" -fg black
  update


  global log_val_Time
  set log_val_Time $new

  # update column width
  global log_fields
  global log_fields_def_size
  set i [lsearch $log_fields "Time"]
  set log_fields_def_size [lreplace $log_fields_def_size $i $i [expr $subsecond_decimal + 8]]
  show_messages

  # update status display
  .stat_action.v configure -text "Idle" -fg black
  update
  
  set busy 0
}

proc doClean {} {
  global online
  global busy
    
  # update status display
  if {!$online} {
    if {$busy} {return}
    set busy 1
    .stat_action.v configure -text "Idle" -fg black
    .stat_query.v configure -text ""
  } else {
    global t0_connect
    set t0_connect [clock seconds]
  }
  update_stats reset
  
  if {!$online} {
    set busy 0
  }
  
  update
}

# scroll to last message
proc viewEndMsg {} {
      global log_visible_fields
      foreach item $log_visible_fields {
        .messages.pw.frame_$item.listbox see end
      }
}


##########################################
# mysql query with autoreconnect
##########################################
proc mysqlquery {query} {
  global db

  # ping db if connected
  if {"$db"!=""} {
    if {![mysql::ping $db]} {
      mysql::close $db
      set db ""
    }
  }
  
  # connect db if necessary
  if {"$db"==""} {
    .stat_action.v configure -text "Database not connected - trying to connect ..." -fg black
    global db_host db_user db_pwd db_db
    if [ catch {set db [mysql::connect -host $db_host -user $db_user -password $db_pwd -db $db_db]} ] {
      after 200 {.stat_action.v configure -text "Database unavailable" -fg darkred}
      return
    }
  }
  
  # discard empty queries
  if {"$query"==""} {return}
  
  # do query
  set r [mysql::sel $db $query -list]

  # free resources if any
  mysql::endquery $db

  return $r
}




# process command line arguments
set x 0
while {[set opt [lindex $argv $x]] != ""} {
 switch -exact -- $opt {
   -s {
        set task [string toupper [lindex $argv [expr $x + 1]]]	
        incr x
	set box .select.filter.vin_partition
	if {[winfo exists $box]} {
          $box delete 0 end
	  $box insert 0 $task
	}
      }
   -d {
        set task [string toupper [lindex $argv [expr $x + 1]]]	
        incr x
	set box .select.filter.vin_detector
	if {[winfo exists $box]} {
          $box delete 0 end
	  $box insert 0 $task
	}
      }
   -p {
        set task [string toupper [lindex $argv [expr $x + 1]]]	
        incr x
	set box .select.filter.vin_partition
	if {[winfo exists $box]} {
          $box delete 0 end
	  $box insert 0 $task
	}
      }
   -f {
        set task [lindex $argv [expr $x + 1]]
        incr x
	set box .select.filter.vin_facility
	if {[winfo exists $box]} {
          $box delete 0 end
	  $box insert 0 $task
	}
      }
   -l {
        set vfilter_level_arg [lindex $argv [expr $x + 1]]
        incr x
        setFilterLevelByName $vfilter_level_arg
   }   
   -full {
      # enable archive commands
      .menubar.archive entryconfigure [.menubar.archive index 1] -state normal
      .menubar.archive entryconfigure [.menubar.archive index 2] -state normal
   }
   -z {
        set configFile [lindex $argv [expr $x + 1]]
        incr x   
   }
 }
 incr x
}


# start infoBrowser in Online mode
.cmd.online select
doOnline
