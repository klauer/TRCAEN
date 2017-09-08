/* This file is part of the CAEN Digitizer Driver.
 * It is subject to the license terms in the LICENSE.txt file found in the
 * top-level directory of this distribution and at
 * https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html. No part
 * of the CAEN Digitizer Driver, including this file, may be copied,
 * modified, propagated, or distributed except according to the terms
 * contained in the LICENSE.txt file.
 */

#include <stddef.h>
#include <stdlib.h>

#include <string>

#include "TR_CAEN_DevAddrStr.h"

static long int str_strtol (std::string const &str, size_t *idx, int base)
{
    char const *cstr = str.c_str();
    char *end;
    long int res = ::strtol(cstr, &end, base);
    if (idx != NULL) {
        *idx = end - cstr;
    }
    return res;
}

bool TR_CAEN_ParseAddrStr (std::string const &addr_str, int *link_number, int *conet_node)
{
    size_t idx;
    
    size_t delim_pos = addr_str.find(':');
    if (delim_pos == std::string::npos) {
        return false;
    }
    
    std::string link_node_str = addr_str.substr(0, delim_pos);
    *link_number = str_strtol(link_node_str, &idx, 0);
    if (idx != link_node_str.size()) {
        return false;
    }
    
    std::string conet_node_str = addr_str.substr(delim_pos + 1);
    *conet_node = str_strtol(conet_node_str, &idx, 0);
    if (idx != conet_node_str.size()) {
        return false;
    }
    
    return true;
}
