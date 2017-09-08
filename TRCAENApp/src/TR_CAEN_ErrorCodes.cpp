/* This file is part of the CAEN Digitizer Driver.
 * It is subject to the license terms in the LICENSE.txt file found in the
 * top-level directory of this distribution and at
 * https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html. No part
 * of the CAEN Digitizer Driver, including this file, may be copied,
 * modified, propagated, or distributed except according to the terms
 * contained in the LICENSE.txt file.
 */

#include "TR_CAEN_ErrorCodes.h"

TR_CAEN_ErrorCodes::TR_CAEN_ErrorCodes ()
{
    add_error(CAEN_DGTZ_Success, "Operation completed successfully");
    add_error(CAEN_DGTZ_CommError, "Communication error");
    add_error(CAEN_DGTZ_GenericError, "Unspecified error");
    add_error(CAEN_DGTZ_InvalidParam, "Invalid parameter");
    add_error(CAEN_DGTZ_InvalidLinkType, "Invalid Link Type");
    add_error(CAEN_DGTZ_InvalidHandle, "Invalid device handle");
    add_error(CAEN_DGTZ_MaxDevicesError, "Maximum number of devices exceeded");
    add_error(CAEN_DGTZ_BadBoardType, "The operation is not allowed on this type of board");
    add_error(CAEN_DGTZ_BadInterruptLev, "The interrupt level is not allowed");
    add_error(CAEN_DGTZ_BadEventNumber, "The event number is bad");
    add_error(CAEN_DGTZ_ReadDeviceRegisterFail, "Unable to read the registry");
    add_error(CAEN_DGTZ_WriteDeviceRegisterFail, "Unable to write into the registry");
    add_error(CAEN_DGTZ_InvalidChannelNumber, "The channel number is invalid");
    add_error(CAEN_DGTZ_ChannelBusy, "The Channel is busy");
    add_error(CAEN_DGTZ_FPIOModeInvalid, "Invalid FPIO Mode");
    add_error(CAEN_DGTZ_WrongAcqMode, "Wrong acquisition mode");
    add_error(CAEN_DGTZ_FunctionNotAllowed, "This function is not allowed for this module");
    add_error(CAEN_DGTZ_Timeout, "Communication Timeout");
    add_error(CAEN_DGTZ_InvalidBuffer, "The buffer is invalid");
    add_error(CAEN_DGTZ_EventNotFound, "The event is not found");
    add_error(CAEN_DGTZ_InvalidEvent, "The event is invalid");
    add_error(CAEN_DGTZ_OutOfMemory, "Out of memory");
    add_error(CAEN_DGTZ_CalibrationError, "Unable to calibrate the board");
    add_error(CAEN_DGTZ_DigitizerNotFound, "Unable to open the digitizer");
    add_error(CAEN_DGTZ_DigitizerAlreadyOpen, "The Digitizer is already open");
    add_error(CAEN_DGTZ_DigitizerNotReady, "The Digitizer is not ready to operate");
    add_error(CAEN_DGTZ_InterruptNotConfigured, "The Digitizer has not the IRQ configured");
    add_error(CAEN_DGTZ_DigitizerMemoryCorrupted, "The digitizer flash memory is corrupted");
    add_error(CAEN_DGTZ_DPPFirmwareNotSupported, "The digitizer dpp firmware is not supported in this lib version");
    add_error(CAEN_DGTZ_InvalidLicense, "Invalid Firmware License");
    add_error(CAEN_DGTZ_InvalidDigitizerStatus, "The digitizer is found in a corrupted status");
    add_error(CAEN_DGTZ_UnsupportedTrace, "The given trace is not supported by the digitizer");
    add_error(CAEN_DGTZ_InvalidProbe, "The given probe is not supported for the given digitizer's trace");
    add_error(CAEN_DGTZ_NotYetImplemented, "The function is not yet implemented");
}

char const * TR_CAEN_ErrorCodes::getErrorText (CAEN_DGTZ_ErrorCode error_code)
{
    ErrorDescMap::iterator it = m_error_desc_map.find(error_code);
    if (it == m_error_desc_map.end()) {
        return "(unknown error code)";
    }
    return it->second;
}

inline void TR_CAEN_ErrorCodes::add_error (CAEN_DGTZ_ErrorCode error_code, char const *error_desc)
{
    m_error_desc_map.insert(ErrorDescMap::value_type(error_code, error_desc));
}
