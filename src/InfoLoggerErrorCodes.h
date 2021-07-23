// Copyright 2019-2020 CERN and copyright holders of ALICE O2.
// See https://alice-o2.web.cern.ch/copyright for details of the copyright holders.
// All rights not expressly granted are reserved.
//
// This software is distributed under the terms of the GNU General Public
// License v3 (GPL Version 3), copied verbatim in the file "COPYING".
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.


// This file is used to define the error codes used in InfoLogger
// Some ranges are reserved for each external module making use of error codes.
// Error codes are referenced and documented centrally here.


namespace AliceO2
{
namespace InfoLogger
{


// Structure to describe the ranges of error codes
typedef struct {
  const int errorCodeMin; // minimum error code
  const int errorCodeMax; // maximum error code max
  const char *component; // component associated to this error code range
  const char *url; // link to corresponding troubleshooting documentation
} ErrorCodeRange;

// Static array defining reserved error code ranges to be used by the o2 modules.
// Error code definitions within each range is left to the corresponding package.
// Terminated by a null entry
static constexpr ErrorCodeRange errorCodeRanges[]={
  { 0, 999, "reserved for general purpose", "" },
  { 1000, 1999, "infoLogger", "" },
  { 2000, 2999, "aliECS", "" },
  { 3000, 3999, "readout", "" },
  { 4000, 4999, "readoutCard", "" },
  { 5000, 5999, "ALF", "" },
  { 0, 0, nullptr, nullptr}  
};

// Constant giving the number of error code ranges defined in errorCodeRanges[]
static int numberOfErrorCodeRanges = sizeof(errorCodeRanges)/sizeof(ErrorCodeRange)-1;

// Structure to describe each error code
typedef struct {
  const int errorCode; // minimum error code
  const char *description; // short description of the error / hint
  const char *url; // link to corresponding troubleshooting documentation
} ErrorCode;

// Static array defining available error codes
// Terminated by a null entry
static constexpr ErrorCode errorCodes[]={
  // infoLogger error codes
  { 1101, "Message flood detected", nullptr},
  { 1102, "End of message flood", nullptr},    
  { 0, nullptr, nullptr}
};

// Constant giving the number of error codes defined in errorCodes[]
static int numberOfErrorCodes = sizeof(errorCodes)/sizeof(ErrorCode)-1;


// end namespace InfoLogger
}
// end namespace AliceO2
}

