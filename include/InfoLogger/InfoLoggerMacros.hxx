// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

/// \file InfoLoggerMacros.hxx
/// \brief Extensive preprocessor macros definitions for compact expression
///        of log messages.
///
/// \author Sylvain Chapeland, CERN
///

#ifndef INFOLOGGER_INFOLOGGERMACROS_HXX
#define INFOLOGGER_INFOLOGGERMACROS_HXX

#include <InfoLogger/InfoLogger.hxx>

// For readability, using CamelCase convention for the macros' names
// Each macro evaluates to the definition of a static InfoLoggerMessageOption
// that can be used directly in a call to InfoLogger::log() function,
// setting all the relevant options.
// e.g. myLogger.log(Log_Info_Ops,"Hello")
//
// One macro is generated for each combination of Severity and Level,
// with and without error code.
//
// The following macros are defined:
//
// LogInfoOps
// LogInfoSupport
// LogInfoDevel
// LogInfoTrace
// LogErrorOps
// LogErrorSupport
// LogErrorDevel
// LogErrorTrace
// LogWarningOps
// LogWarningSupport
// LogWarningDevel
// LogWarningTrace
// LogFatalOps
// LogFatalSupport
// LogFatalDevel
// LogFatalTrace
// LogDebugOps
// LogDebugSupport
// LogDebugDevel
// LogDebugTrace
//
// LogInfoOps_(errno)
// LogInfoSupport_(errno)
// LogInfoDevel_(errno)
// LogInfoTrace_(errno)
// LogErrorOps_(errno)
// LogErrorSupport_(errno)
// LogErrorDevel_(errno)
// LogErrorTrace_(errno)
// LogWarningOps_(errno)
// LogWarningSupport_(errno)
// LogWarningDevel_(errno)
// LogWarningTrace_(errno)
// LogFatalOps_(errno)
// LogFatalSupport_(errno)
// LogFatalDevel_(errno)
// LogFatalTrace_(errno)
// LogDebugOps_(errno)
// LogDebugSupport_(errno)
// LogDebugDevel_(errno)
// LogDebugTrace_(errno)


#define LogInfoOps AliceO2::InfoLogger::InfoLogger::InfoLoggerMessageOption { AliceO2::InfoLogger::InfoLogger::Severity::Info, AliceO2::InfoLogger::InfoLogger::Level::Ops, AliceO2::InfoLogger::InfoLogger::undefinedMessageOption.errorCode, __FILE__, __LINE__ }
#define LogInfoOps_(errno) AliceO2::InfoLogger::InfoLogger::InfoLoggerMessageOption { AliceO2::InfoLogger::InfoLogger::Severity::Info, AliceO2::InfoLogger::InfoLogger::Level::Ops, errno, __FILE__, __LINE__ }
#define LogInfoSupport AliceO2::InfoLogger::InfoLogger::InfoLoggerMessageOption { AliceO2::InfoLogger::InfoLogger::Severity::Info, AliceO2::InfoLogger::InfoLogger::Level::Support, AliceO2::InfoLogger::InfoLogger::undefinedMessageOption.errorCode, __FILE__, __LINE__ }
#define LogInfoSupport_(errno) AliceO2::InfoLogger::InfoLogger::InfoLoggerMessageOption { AliceO2::InfoLogger::InfoLogger::Severity::Info, AliceO2::InfoLogger::InfoLogger::Level::Support, errno, __FILE__, __LINE__ }
#define LogInfoDevel AliceO2::InfoLogger::InfoLogger::InfoLoggerMessageOption { AliceO2::InfoLogger::InfoLogger::Severity::Info, AliceO2::InfoLogger::InfoLogger::Level::Devel, AliceO2::InfoLogger::InfoLogger::undefinedMessageOption.errorCode, __FILE__, __LINE__ }
#define LogInfoDevel_(errno) AliceO2::InfoLogger::InfoLogger::InfoLoggerMessageOption { AliceO2::InfoLogger::InfoLogger::Severity::Info, AliceO2::InfoLogger::InfoLogger::Level::Devel, errno, __FILE__, __LINE__ }
#define LogInfoTrace AliceO2::InfoLogger::InfoLogger::InfoLoggerMessageOption { AliceO2::InfoLogger::InfoLogger::Severity::Info, AliceO2::InfoLogger::InfoLogger::Level::Trace, AliceO2::InfoLogger::InfoLogger::undefinedMessageOption.errorCode, __FILE__, __LINE__ }
#define LogInfoTrace_(errno) AliceO2::InfoLogger::InfoLogger::InfoLoggerMessageOption { AliceO2::InfoLogger::InfoLogger::Severity::Info, AliceO2::InfoLogger::InfoLogger::Level::Trace, errno, __FILE__, __LINE__ }


#define LogErrorOps AliceO2::InfoLogger::InfoLogger::InfoLoggerMessageOption { AliceO2::InfoLogger::InfoLogger::Severity::Error, AliceO2::InfoLogger::InfoLogger::Level::Ops, AliceO2::InfoLogger::InfoLogger::undefinedMessageOption.errorCode, __FILE__, __LINE__ }
#define LogErrorOps_(errno) AliceO2::InfoLogger::InfoLogger::InfoLoggerMessageOption { AliceO2::InfoLogger::InfoLogger::Severity::Error, AliceO2::InfoLogger::InfoLogger::Level::Ops, errno, __FILE__, __LINE__ }
#define LogErrorSupport AliceO2::InfoLogger::InfoLogger::InfoLoggerMessageOption { AliceO2::InfoLogger::InfoLogger::Severity::Error, AliceO2::InfoLogger::InfoLogger::Level::Support, AliceO2::InfoLogger::InfoLogger::undefinedMessageOption.errorCode, __FILE__, __LINE__ }
#define LogErrorSupport_(errno) AliceO2::InfoLogger::InfoLogger::InfoLoggerMessageOption { AliceO2::InfoLogger::InfoLogger::Severity::Error, AliceO2::InfoLogger::InfoLogger::Level::Support, errno, __FILE__, __LINE__ }
#define LogErrorDevel AliceO2::InfoLogger::InfoLogger::InfoLoggerMessageOption { AliceO2::InfoLogger::InfoLogger::Severity::Error, AliceO2::InfoLogger::InfoLogger::Level::Devel, AliceO2::InfoLogger::InfoLogger::undefinedMessageOption.errorCode, __FILE__, __LINE__ }
#define LogErrorDevel_(errno) AliceO2::InfoLogger::InfoLogger::InfoLoggerMessageOption { AliceO2::InfoLogger::InfoLogger::Severity::Error, AliceO2::InfoLogger::InfoLogger::Level::Devel, errno, __FILE__, __LINE__ }
#define LogErrorTrace AliceO2::InfoLogger::InfoLogger::InfoLoggerMessageOption { AliceO2::InfoLogger::InfoLogger::Severity::Error, AliceO2::InfoLogger::InfoLogger::Level::Trace, AliceO2::InfoLogger::InfoLogger::undefinedMessageOption.errorCode, __FILE__, __LINE__ }
#define LogErrorTrace_(errno) AliceO2::InfoLogger::InfoLogger::InfoLoggerMessageOption { AliceO2::InfoLogger::InfoLogger::Severity::Error, AliceO2::InfoLogger::InfoLogger::Level::Trace, errno, __FILE__, __LINE__ }


#define LogWarningOps AliceO2::InfoLogger::InfoLogger::InfoLoggerMessageOption { AliceO2::InfoLogger::InfoLogger::Severity::Warning, AliceO2::InfoLogger::InfoLogger::Level::Ops, AliceO2::InfoLogger::InfoLogger::undefinedMessageOption.errorCode, __FILE__, __LINE__ }
#define LogWarningOps_(errno) AliceO2::InfoLogger::InfoLogger::InfoLoggerMessageOption { AliceO2::InfoLogger::InfoLogger::Severity::Warning, AliceO2::InfoLogger::InfoLogger::Level::Ops, errno, __FILE__, __LINE__ }
#define LogWarningSupport AliceO2::InfoLogger::InfoLogger::InfoLoggerMessageOption { AliceO2::InfoLogger::InfoLogger::Severity::Warning, AliceO2::InfoLogger::InfoLogger::Level::Support, AliceO2::InfoLogger::InfoLogger::undefinedMessageOption.errorCode, __FILE__, __LINE__ }
#define LogWarningSupport_(errno) AliceO2::InfoLogger::InfoLogger::InfoLoggerMessageOption { AliceO2::InfoLogger::InfoLogger::Severity::Warning, AliceO2::InfoLogger::InfoLogger::Level::Support, errno, __FILE__, __LINE__ }
#define LogWarningDevel AliceO2::InfoLogger::InfoLogger::InfoLoggerMessageOption { AliceO2::InfoLogger::InfoLogger::Severity::Warning, AliceO2::InfoLogger::InfoLogger::Level::Devel, AliceO2::InfoLogger::InfoLogger::undefinedMessageOption.errorCode, __FILE__, __LINE__ }
#define LogWarningDevel_(errno) AliceO2::InfoLogger::InfoLogger::InfoLoggerMessageOption { AliceO2::InfoLogger::InfoLogger::Severity::Warning, AliceO2::InfoLogger::InfoLogger::Level::Devel, errno, __FILE__, __LINE__ }
#define LogWarningTrace AliceO2::InfoLogger::InfoLogger::InfoLoggerMessageOption { AliceO2::InfoLogger::InfoLogger::Severity::Warning, AliceO2::InfoLogger::InfoLogger::Level::Trace, AliceO2::InfoLogger::InfoLogger::undefinedMessageOption.errorCode, __FILE__, __LINE__ }
#define LogWarningTrace_(errno) AliceO2::InfoLogger::InfoLogger::InfoLoggerMessageOption { AliceO2::InfoLogger::InfoLogger::Severity::Warning, AliceO2::InfoLogger::InfoLogger::Level::Trace, errno, __FILE__, __LINE__ }


#define LogFatalOps AliceO2::InfoLogger::InfoLogger::InfoLoggerMessageOption { AliceO2::InfoLogger::InfoLogger::Severity::Fatal, AliceO2::InfoLogger::InfoLogger::Level::Ops, AliceO2::InfoLogger::InfoLogger::undefinedMessageOption.errorCode, __FILE__, __LINE__ }
#define LogFatalOps_(errno) AliceO2::InfoLogger::InfoLogger::InfoLoggerMessageOption { AliceO2::InfoLogger::InfoLogger::Severity::Fatal, AliceO2::InfoLogger::InfoLogger::Level::Ops, errno, __FILE__, __LINE__ }
#define LogFatalSupport AliceO2::InfoLogger::InfoLogger::InfoLoggerMessageOption { AliceO2::InfoLogger::InfoLogger::Severity::Fatal, AliceO2::InfoLogger::InfoLogger::Level::Support, AliceO2::InfoLogger::InfoLogger::undefinedMessageOption.errorCode, __FILE__, __LINE__ }
#define LogFatalSupport_(errno) AliceO2::InfoLogger::InfoLogger::InfoLoggerMessageOption { AliceO2::InfoLogger::InfoLogger::Severity::Fatal, AliceO2::InfoLogger::InfoLogger::Level::Support, errno, __FILE__, __LINE__ }
#define LogFatalDevel AliceO2::InfoLogger::InfoLogger::InfoLoggerMessageOption { AliceO2::InfoLogger::InfoLogger::Severity::Fatal, AliceO2::InfoLogger::InfoLogger::Level::Devel, AliceO2::InfoLogger::InfoLogger::undefinedMessageOption.errorCode, __FILE__, __LINE__ }
#define LogFatalDevel_(errno) AliceO2::InfoLogger::InfoLogger::InfoLoggerMessageOption { AliceO2::InfoLogger::InfoLogger::Severity::Fatal, AliceO2::InfoLogger::InfoLogger::Level::Devel, errno, __FILE__, __LINE__ }
#define LogFatalTrace AliceO2::InfoLogger::InfoLogger::InfoLoggerMessageOption { AliceO2::InfoLogger::InfoLogger::Severity::Fatal, AliceO2::InfoLogger::InfoLogger::Level::Trace, AliceO2::InfoLogger::InfoLogger::undefinedMessageOption.errorCode, __FILE__, __LINE__ }
#define LogFatalTrace_(errno) AliceO2::InfoLogger::InfoLogger::InfoLoggerMessageOption { AliceO2::InfoLogger::InfoLogger::Severity::Fatal, AliceO2::InfoLogger::InfoLogger::Level::Trace, errno, __FILE__, __LINE__ }


#define LogDebugOps AliceO2::InfoLogger::InfoLogger::InfoLoggerMessageOption { AliceO2::InfoLogger::InfoLogger::Severity::Debug, AliceO2::InfoLogger::InfoLogger::Level::Ops, AliceO2::InfoLogger::InfoLogger::undefinedMessageOption.errorCode, __FILE__, __LINE__ }
#define LogDebugOps_(errno) AliceO2::InfoLogger::InfoLogger::InfoLoggerMessageOption { AliceO2::InfoLogger::InfoLogger::Severity::Debug, AliceO2::InfoLogger::InfoLogger::Level::Ops, errno, __FILE__, __LINE__ }
#define LogDebugSupport AliceO2::InfoLogger::InfoLogger::InfoLoggerMessageOption { AliceO2::InfoLogger::InfoLogger::Severity::Debug, AliceO2::InfoLogger::InfoLogger::Level::Support, AliceO2::InfoLogger::InfoLogger::undefinedMessageOption.errorCode, __FILE__, __LINE__ }
#define LogDebugSupport_(errno) AliceO2::InfoLogger::InfoLogger::InfoLoggerMessageOption { AliceO2::InfoLogger::InfoLogger::Severity::Debug, AliceO2::InfoLogger::InfoLogger::Level::Support, errno, __FILE__, __LINE__ }
#define LogDebugDevel AliceO2::InfoLogger::InfoLogger::InfoLoggerMessageOption { AliceO2::InfoLogger::InfoLogger::Severity::Debug, AliceO2::InfoLogger::InfoLogger::Level::Devel, AliceO2::InfoLogger::InfoLogger::undefinedMessageOption.errorCode, __FILE__, __LINE__ }
#define LogDebugDevel_(errno) AliceO2::InfoLogger::InfoLogger::InfoLoggerMessageOption { AliceO2::InfoLogger::InfoLogger::Severity::Debug, AliceO2::InfoLogger::InfoLogger::Level::Devel, errno, __FILE__, __LINE__ }
#define LogDebugTrace AliceO2::InfoLogger::InfoLogger::InfoLoggerMessageOption { AliceO2::InfoLogger::InfoLogger::Severity::Debug, AliceO2::InfoLogger::InfoLogger::Level::Trace, AliceO2::InfoLogger::InfoLogger::undefinedMessageOption.errorCode, __FILE__, __LINE__ }
#define LogDebugTrace_(errno) AliceO2::InfoLogger::InfoLogger::InfoLoggerMessageOption { AliceO2::InfoLogger::InfoLogger::Severity::Debug, AliceO2::InfoLogger::InfoLogger::Level::Trace, errno, __FILE__, __LINE__ }


#endif //INFOLOGGER_INFOLOGGERMACROS_HXX


