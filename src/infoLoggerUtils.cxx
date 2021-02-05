// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

#include "infoLoggerUtils.h"



int getKeyValuePairsFromString(const std::string& input, std::map<std::string, std::string>& output)
{
  output.clear();
  std::size_t ix0 = 0;                 // begin of pair in string
  std::size_t ix1 = std::string::npos; // end of pair in string
  std::size_t ix2 = std::string::npos; // position of '='
  for (;;) {
    ix1 = input.find(",", ix0);
    ix2 = input.find("=", ix0);
    if (ix2 >= ix1) {
      break;
    } // end of string
    
    const std::string& trimchars = "\t\n\v\f\r ";
 
    // trim key
    std::string key = input.substr(ix0, ix2 - ix0);
    key.erase(key.find_last_not_of(trimchars) + 1);
    key.erase(0, key.find_first_not_of(trimchars));
    
    // trim value
    std::string value;    
    if (ix1 == std::string::npos) {
      value = input.substr(ix2 + 1);
    } else {
      value = input.substr(ix2 + 1, ix1 - (ix2 + 1));
    }
    value.erase(value.find_last_not_of(trimchars) + 1);
    value.erase(0, value.find_first_not_of(trimchars));

    // insert in map
    output.insert(std::pair<std::string, std::string>(key, value));
    
    // iterate next pair unless end of string
    if (ix1 == std::string::npos) {
      break;
    }
    ix0 = ix1 + 1;
  }
  return 0;
}
