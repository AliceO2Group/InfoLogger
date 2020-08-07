#!/usr/bin/tclsh

# generate macros for compact writing of infologger expressions


# namespace
set ns "AliceO2::InfoLogger::" 

# severity
set lSeverity {Info Error Warning Fatal Debug}

# level
set lLevel {Ops Support Devel Trace}


foreach s $lSeverity {
  foreach l $lLevel {
    puts "// Log${s}${l}"
  }
}
puts "//"
foreach s $lSeverity {
  foreach l $lLevel {
    puts "// Log${s}${l}_(errno)"
  }
}


foreach s $lSeverity {
  puts "\n"
  foreach l $lLevel {
    # one without error code
    puts "#define Log${s}${l} ${ns}InfoLogger::InfoLoggerMessageOption { ${ns}InfoLogger::Severity::${s}, ${ns}InfoLogger::Level::${l}, ${ns}InfoLogger::undefinedMessageOption.errorCode, __FILE__, __LINE__ }"
    # one with error code
    puts "#define Log${s}${l}_(errno) ${ns}InfoLogger::InfoLoggerMessageOption { ${ns}InfoLogger::Severity::${s}, ${ns}InfoLogger::Level::${l}, errno, __FILE__, __LINE__ }"    
  }
}
