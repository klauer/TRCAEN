/* This file is part of the CAEN Digitizer Driver.
 * It is subject to the license terms in the LICENSE.txt file found in the
 * top-level directory of this distribution and at
 * https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html. No part
 * of the CAEN Digitizer Driver, including this file, may be copied,
 * modified, propagated, or distributed except according to the terms
 * contained in the LICENSE.txt file.
 */

#include "TR_CAEN_Registers.h"

TR_CAEN_Register const TR_CAEN_Registers::AcqControl = {"AcqControl", 0x8100u};

TR_CAEN_Register const TR_CAEN_Registers::RunStartStopDelay = {"RunStartStopDelay", 0x8170u};

TR_CAEN_Register const TR_CAEN_Registers::BoardInfo = {"BoardInfo", 0x8140u};

TR_CAEN_Register const TR_CAEN_Registers::FanSpeedControl = {"FanSpeedControl", 0x8168u};

TR_CAEN_Register const TR_CAEN_Registers::ChannelGain[8] = {
    {"Channel0Gain", 0x1028u},
    {"Channel1Gain", 0x1128u},
    {"Channel2Gain", 0x1228u},
    {"Channel3Gain", 0x1328u},
    {"Channel4Gain", 0x1428u},
    {"Channel5Gain", 0x1528u},
    {"Channel6Gain", 0x1628u},
    {"Channel7Gain", 0x1728u}
};

TR_CAEN_Register const TR_CAEN_Registers::ChannelPulseWidth[8] = {
    {"Channel0PulseWidth", 0x1070u},
    {"Channel1PulseWidth", 0x1170u},
    {"Channel2PulseWidth", 0x1270u},
    {"Channel3PulseWidth", 0x1370u},
    {"Channel4PulseWidth", 0x1470u},
    {"Channel5PulseWidth", 0x1570u},
    {"Channel6PulseWidth", 0x1670u},
    {"Channel7PulseWidth", 0x1770u},
};

TR_CAEN_Register const TR_CAEN_Registers::TriggerSourceEnableMask = {"TriggerSourceEnableMask", 0x810Cu};
