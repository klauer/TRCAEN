/* This file is part of the CAEN Digitizer Driver.
 * It is subject to the license terms in the LICENSE.txt file found in the
 * top-level directory of this distribution and at
 * https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html. No part
 * of the CAEN Digitizer Driver, including this file, may be copied,
 * modified, propagated, or distributed except according to the terms
 * contained in the LICENSE.txt file.
 */

#ifndef TR_CAEN_DEV_ADDR_STR_H
#define TR_CAEN_DEV_ADDR_STR_H

#include <string>

bool TR_CAEN_ParseAddrStr (std::string const &addr_str, int *link_number, int *conet_node);

#endif
