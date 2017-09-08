/* This file is part of the CAEN Digitizer Driver.
 * It is subject to the license terms in the LICENSE.txt file found in the
 * top-level directory of this distribution and at
 * https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html. No part
 * of the CAEN Digitizer Driver, including this file, may be copied,
 * modified, propagated, or distributed except according to the terms
 * contained in the LICENSE.txt file.
 */

#ifndef TR_CAEN_REGISTERS_H
#define TR_CAEN_REGISTERS_H

#include <stdint.h>

struct TR_CAEN_Register {
    char const *reg_name;
    uint32_t reg_addr;
};

class TR_CAEN_Registers {
public:
    static TR_CAEN_Register const AcqControl;
    static TR_CAEN_Register const RunStartStopDelay;
    static TR_CAEN_Register const BoardInfo;
    static TR_CAEN_Register const FanSpeedControl;
    static TR_CAEN_Register const ChannelGain[8];
    static TR_CAEN_Register const ChannelPulseWidth[8];
    static TR_CAEN_Register const TriggerSourceEnableMask;
};

#endif
