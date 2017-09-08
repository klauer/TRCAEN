/* This file is part of the CAEN Digitizer Driver.
 * It is subject to the license terms in the LICENSE.txt file found in the
 * top-level directory of this distribution and at
 * https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html. No part
 * of the CAEN Digitizer Driver, including this file, may be copied,
 * modified, propagated, or distributed except according to the terms
 * contained in the LICENSE.txt file.
 */

#ifndef TR_CAEN_ERROR_CODES_H
#define TR_CAEN_ERROR_CODES_H

#include <map>

#include <CAENDigitizerType.h>

class TR_CAEN_ErrorCodes {
public:
    TR_CAEN_ErrorCodes ();
    char const * getErrorText (CAEN_DGTZ_ErrorCode error_code);
    
private:
    typedef std::map<CAEN_DGTZ_ErrorCode, char const *> ErrorDescMap;
    
    inline void add_error (CAEN_DGTZ_ErrorCode error_code, char const *error_desc);
    
    ErrorDescMap m_error_desc_map;
};

#endif
