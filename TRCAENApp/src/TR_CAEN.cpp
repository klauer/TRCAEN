/* This file is part of the CAEN Digitizer Driver.
 * It is subject to the license terms in the LICENSE.txt file found in the
 * top-level directory of this distribution and at
 * https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html. No part
 * of the CAEN Digitizer Driver, including this file, may be copied,
 * modified, propagated, or distributed except according to the terms
 * contained in the LICENSE.txt file.
 */

#include <limits.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include <cmath>
#include <string>
#include <limits>
#include <algorithm>

#include <epicsGuard.h>
#include <epicsThread.h>
#include <epicsAssert.h>

#include <TRChannelDataSubmit.h>

#include <CAENDigitizer.h>
#include <CAENDigitizerType.h>

#include "TR_CAEN.h"
#include "TR_CAEN_DevAddrStr.h"
#include "TR_CAEN_BitUtils.h"

TR_CAEN::TR_CAEN (
    char const *port_name, char const *device_addr_str,
    int read_thread_prio_epics, int read_thread_stack_size,
    int max_ad_buffers, size_t max_ad_memory)
:
    TRBaseDriver(TRBaseConfig()
        .set(&TRBaseConfig::port_name, std::string(port_name))
        .set(&TRBaseConfig::num_channels, MaxNumChannels)
        .set(&TRBaseConfig::num_asyn_params, (int)NUM_CAEN_ASYN_PARAMS)
        .set(&TRBaseConfig::interface_mask, asynInt32Mask|asynFloat64Mask|asynOctetMask)
        .set(&TRBaseConfig::interrupt_mask, asynInt32Mask|asynFloat64Mask|asynOctetMask)
        .set(&TRBaseConfig::num_config_params, NumCAENConfigParams)
        .set(&TRBaseConfig::read_thread_prio, read_thread_prio_epics)
        .set(&TRBaseConfig::read_thread_stack_size, read_thread_stack_size)
        .set(&TRBaseConfig::max_ad_buffers, max_ad_buffers)
        .set(&TRBaseConfig::max_ad_memory, max_ad_memory)
    ),
    m_worker((std::string("TRwork:") + port_name)),
    m_device_addr_str(device_addr_str),
    m_open_state(OpenStateClosed),
    m_resetting(false),
    m_calibrating(false),
    m_refreshing(false)
{
    char param_name[40];
    
    // Initialize worker thread tasks.
    for (int task = 0; task < NumWorkerTasks; task++) {
        m_worker_task[task].init(&m_worker, this, (WorkerTask)task);
    }
    
    // Non-channel-specific configuration parameters.
    initConfigParam(m_param_start_stop_mode,      "START_STOP_MODE",      -1);
    initConfigParam(m_param_run_start_stop_delay, "RUN_START_STOP_DELAY", (double)NAN);
    
    // Channel-specific configuration parameters.
    for (int ch = 0; ch < MaxNumChannels; ch++) {
        char ch_prefix_arr[10];
        ::sprintf(ch_prefix_arr, "CH%d_", ch);
        std::string ch_prefix(ch_prefix_arr);
        
        initConfigParam(m_param_channel[ch].input_range, (ch_prefix+"INPUT_RANGE").c_str(), -1);
        initConfigParam(m_param_channel[ch].pulse_width, (ch_prefix+"PULSE_WIDTH").c_str(), (double)NAN);
    }

    // NOTE: All initConfigParam/initInternalParam must be before all createParam
    // so that the parameter index comparison in writeInt32/readInt32 works as expected.
    
    createParam("OPEN_STATE",     asynParamInt32,   &m_asyn_params[OPEN_STATE]);
    createParam("HW_SAMPLE_RATE", asynParamFloat64, &m_asyn_params[HW_SAMPLE_RATE]);
    createParam("RESET",          asynParamInt32,   &m_asyn_params[RESET]);
    createParam("CALIBRATE",      asynParamInt32,   &m_asyn_params[CALIBRATE]);
    createParam("REFRESH",        asynParamInt32,   &m_asyn_params[REFRESH]);
    
    createParam("INFO_MODEL_NAME",   asynParamOctet,   &m_asyn_params[INFO_MODEL_NAME]);
    createParam("INFO_ROC_FW_REV",   asynParamOctet,   &m_asyn_params[INFO_ROC_FW_REV]);
    createParam("INFO_AMC_FW_REV",   asynParamOctet,   &m_asyn_params[INFO_AMC_FW_REV]);
    createParam("INFO_PCB_REVISION", asynParamInt32,   &m_asyn_params[INFO_PCB_REVISION]);
    createParam("INFO_SERIAL_NUM",   asynParamInt32,   &m_asyn_params[INFO_SERIAL_NUM]);
    createParam("INFO_NUM_CHANNELS", asynParamInt32,   &m_asyn_params[INFO_NUM_CHANNELS]);
    createParam("INFO_FAMILY",       asynParamInt32,   &m_asyn_params[INFO_FAMILY]);
    createParam("INFO_CH_MEM_SIZE",  asynParamInt32,   &m_asyn_params[INFO_CH_MEM_SIZE]);
    
    createParam("FAN_CONTROL_MODE",     asynParamInt32,   &m_asyn_params[FAN_CONTROL_MODE]);
    createParam("FAN_CONTROL_MODE_RB",  asynParamInt32,   &m_asyn_params[FAN_CONTROL_MODE_RB]);
    createParam("CLOCK_SOURCE",         asynParamInt32,   &m_asyn_params[CLOCK_SOURCE]);
    createParam("CLOCK_SOURCE_RB",      asynParamInt32,   &m_asyn_params[CLOCK_SOURCE_RB]);
    createParam("SW_TRIGGER",           asynParamInt32,   &m_asyn_params[SW_TRIGGER]);
    createParam("SW_TRIGGER_RB",        asynParamInt32,   &m_asyn_params[SW_TRIGGER_RB]);
    createParam("EXT_TRIGGER",          asynParamInt32,   &m_asyn_params[EXT_TRIGGER]);
    createParam("EXT_TRIGGER_RB",       asynParamInt32,   &m_asyn_params[EXT_TRIGGER_RB]);
    
    for (int i = 0; i < NumChannelPairs; i++) {
        ::sprintf(param_name, "CH_SELF_TRIGGER_%d%d", 2*i, 2*i+1);
        createParam(param_name, asynParamInt32, &m_asyn_params[CH_SELF_TRIGGER_01+i]);
    }
    for (int i = 0; i < NumChannelPairs; i++) {
        ::sprintf(param_name, "CH_SELF_TRIGGER_%d%d_RB", 2*i, 2*i+1);
        createParam(param_name, asynParamInt32, &m_asyn_params[CH_SELF_TRIGGER_01_RB+i]);
    }
    
    // Set initial parameter values.
    setIntegerParam(m_asyn_params[OPEN_STATE], m_open_state);
    setIntegerParam(m_asyn_params[RESET],      RequestStateFailed);
    setIntegerParam(m_asyn_params[CALIBRATE],  RequestStateFailed);
    setIntegerParam(m_asyn_params[REFRESH],    RequestStateFailed);
    setIntegerParam(m_asyn_params[FAN_CONTROL_MODE_RB], -1);
    setIntegerParam(m_asyn_params[CLOCK_SOURCE_RB],     -1);
    setIntegerParam(m_asyn_params[SW_TRIGGER_RB],       -1);
    setIntegerParam(m_asyn_params[EXT_TRIGGER_RB],      -1);
    for (int i = 0; i < NumChannelPairs; i++) {
        setIntegerParam(m_asyn_params[CH_SELF_TRIGGER_01_RB+i], -1);
    }
    
    // Start the worker thread.
    m_worker.start();
}

asynStatus TR_CAEN::writeInt32 (asynUser *pasynUser, int32_t value)
{
    int reason = pasynUser->reason;

    // If the parameter is not ours or a config parameter, delegate to base class
    if (reason < m_asyn_params[FIRST_PARAM]) {
        return TRBaseDriver::writeInt32(pasynUser, value);
    }
    
    // Handle specific parameters which do not need the device to be open.
    if (reason == m_asyn_params[OPEN_STATE]) {
        return handleOpenStateRequest(value);
    }
    
    // Handle parameters which are just written to the parameter cache.
    if (reason == m_asyn_params[HW_SAMPLE_RATE]) {
        return asynPortDriver::writeInt32(pasynUser, value);
    }
    
    // Handle requests that don't strictly require the device to be open.
    if (reason == m_asyn_params[FAN_CONTROL_MODE]) {
        return handleFanControlModeRequest(value);
    }
    else if (reason == m_asyn_params[CLOCK_SOURCE]) {
        return handleClockSourceRequest(value);
    }
    else if (reason == m_asyn_params[SW_TRIGGER]) {
        return handleSwTriggerRequest(value);
    }
    else if (reason == m_asyn_params[EXT_TRIGGER]) {
        return handleExtTriggerRequest(value);
    }
    else if (reason >= m_asyn_params[CH_SELF_TRIGGER_01] && reason <= m_asyn_params[CH_SELF_TRIGGER_67]) {
        return handleChSelfTriggerRequest(value, reason - m_asyn_params[CH_SELF_TRIGGER_01]);
    }
    
    // Check that the device has been opened.
    if (m_open_state != OpenStateOpened) {
        char const *param_name = "";
        getParamName(reason, &param_name);
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "%s writeInt32 %s: Device is not open.\n",
            portName, param_name);
        return asynError;
    }
    
    // Handle requests.
    if (reason == m_asyn_params[RESET]) {
        return handleResetRequest();
    }
    else if (reason == m_asyn_params[CALIBRATE]) {
        return handleCalibrateRequest();
    }
    else if (reason == m_asyn_params[REFRESH]) {
        return handleRefreshRequest();
    }
    
    return asynError;
}

asynStatus TR_CAEN::readInt32 (asynUser *pasynUser, int32_t *value)
{
    int reason = pasynUser->reason;

    // If the parameter is not ours or a config parameter, delegate to base class
    if (reason < m_asyn_params[FIRST_PARAM]) {
        return TRBaseDriver::readInt32(pasynUser, value);
    }
    
    // All other parameters are read from the parameter cache.
    return asynPortDriver::readInt32(pasynUser, value);
}

asynStatus TR_CAEN::handleOpenStateRequest (int32_t request)
{
    char const *function = "handleOpenStateRequest";
    
    // Sanity check written value.
    if (request != OpenStateOpened && request != OpenStateClosed) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "%s %s: Bad value written.\n",
            portName, function);
        return asynError;
    }
    
    // Check that we are not already busy opening or closing.
    if (m_open_state != OpenStateOpened && m_open_state != OpenStateClosed) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "%s %s: Already opening or closing.\n",
            portName, function);
        return asynError;
    }
    
    // Check if we are already in the desired state.
    if (request == m_open_state) {
        return asynSuccess;
    }
    
    // Check that we are disarmed.
    if (isArmed()) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "%s %s: Must be disarmed.\n",
            portName, function);
        return asynError;
    }
    
    // Set the open state to opening/closing.
    setOpenState((request == OpenStateOpened) ? OpenStateOpening : OpenStateClosing);
    
    // Start the worker thread task.
    bool started = m_worker_task[WorkerTaskOpenClose].start();
    assert(started);
    
    return asynSuccess;
}

asynStatus TR_CAEN::handleResetRequest ()
{
    assert(m_open_state == OpenStateOpened);
    
    char const *function = "handleResetRequest";
    
    // Check if we are already resetting.
    if (m_resetting) {
        return asynSuccess;
    }
    
    // Check that we are disarmed.
    if (isArmed()) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "%s %s: Must be disarmed.\n",
            portName, function);
        return asynError;
    }
    
    // Set resetting to true, start the worker thread task.
    m_resetting = true;
    m_worker_task[WorkerTaskReset].start();
    
    // Set the RESET parameter to running.
    setIntegerParam(m_asyn_params[RESET], RequestStateRunning);
    callParamCallbacks();
    
    return asynSuccess;
}

asynStatus TR_CAEN::handleCalibrateRequest ()
{
    assert(m_open_state == OpenStateOpened);
    
    char const *function = "handleCalibrateRequest";
    
    // Check if we are already calibrating.
    if (m_calibrating) {
        return asynSuccess;
    }
    
    // Check that we are disarmed.
    if (isArmed()) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "%s %s: Must be disarmed.\n",
            portName, function);
        return asynError;
    }
    
    // Set calibrating to true, start the worker thread task.
    m_calibrating = true;
    m_worker_task[WorkerTaskCalibrate].start();
    
    // Set the CALIBRATE parameter to running.
    setIntegerParam(m_asyn_params[CALIBRATE], RequestStateRunning);
    callParamCallbacks();
    
    return asynSuccess;
}

asynStatus TR_CAEN::handleRefreshRequest ()
{
    assert(m_open_state == OpenStateOpened);
    
    // Check if we are already refreshing.
    if (m_refreshing) {
        return asynSuccess;
    }
    
    // Set refreshing to true, start the worker thread task.
    m_refreshing = true;
    m_worker_task[WorkerTaskRefresh].start();
    
    // Set the REFRESH parameter to running.
    setIntegerParam(m_asyn_params[REFRESH], RequestStateRunning);
    callParamCallbacks();
    
    return asynSuccess;
}

asynStatus TR_CAEN::handleFanControlModeRequest (int32_t value)
{
    char const *function = "handleFanControlModeRequest";
    
    if (value != FanControlModeSlowAuto && value != FanControlModeFullSpeed) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "%s %s: Bad value written.\n",
            portName, function);
        return asynError;
    }
    
    // Update parameter value.
    setIntegerParam(m_asyn_params[FAN_CONTROL_MODE], value);
    callParamCallbacks();
    
    // If the device is open, queue worker thread task.
    // There is no problem if already queued.
    if (m_open_state == OpenStateOpened) {
        m_worker_task[WorkerTaskSetFan].start();
    }
    
    return asynSuccess;
}

asynStatus TR_CAEN::handleClockSourceRequest (int32_t value)
{
    char const *function = "handleClockSourceRequest";
    
    if (value != ClockSourceInternal && value != ClockSourceExternal) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "%s %s: Bad value written.\n",
            portName, function);
        return asynError;
    }
    
    // Update parameter value.
    setIntegerParam(m_asyn_params[CLOCK_SOURCE], value);
    callParamCallbacks();
    
    // If the device is open, queue worker thread task.
    // There is no problem if already queued.
    if (m_open_state == OpenStateOpened) {
        m_worker_task[WorkerTaskSetClockSource].start();
    }
    
    return asynSuccess;
}

asynStatus TR_CAEN::handleSwTriggerRequest (int32_t value)
{
    char const *function = "handleSwTriggerRequest";
    
    if (value != TriggerModeEnable && value != TriggerModeDisable) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "%s %s: Bad value written.\n",
            portName, function);
        return asynError;
    }
    
    // Update parameter value.
    setIntegerParam(m_asyn_params[SW_TRIGGER], value);
    callParamCallbacks();
    
    // If the device is open, queue worker thread task.
    // There is no problem if already queued.
    if (m_open_state == OpenStateOpened) {
        m_worker_task[WorkerTaskSetSwTrigger].start();
    }
    
    return asynSuccess;
}

asynStatus TR_CAEN::handleExtTriggerRequest (int32_t value)
{
    char const *function = "handleExtTriggerRequest";
    
    if (value != TriggerModeEnable && value != TriggerModeDisable) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "%s %s: Bad value written.\n",
            portName, function);
        return asynError;
    }
    
    // Update parameter value.
    setIntegerParam(m_asyn_params[EXT_TRIGGER], value);
    callParamCallbacks();
    
    // If the device is open, queue worker thread task.
    // There is no problem if already queued.
    if (m_open_state == OpenStateOpened) {
        m_worker_task[WorkerTaskSetExtTrigger].start();
    }
    
    return asynSuccess;
}

asynStatus TR_CAEN::handleChSelfTriggerRequest (int32_t value, int ch_pair)
{
    assert(ch_pair >= 0 && ch_pair < NumChannelPairs);
    
    char const *function = "handleChSelfTriggerRequest";
    
    if (value != TriggerModeEnable && value != TriggerModeDisable) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "%s %s: Bad value written.\n",
            portName, function);
        return asynError;
    }
    
    // Update parameter value.
    setIntegerParam(m_asyn_params[CH_SELF_TRIGGER_01+ch_pair], value);
    callParamCallbacks();
    
    // If the device is open, queue worker thread task.
    // There is no problem if already queued.
    if (m_open_state == OpenStateOpened) {
        m_worker_task[WorkerTaskSetChSelfTrigger01+ch_pair].start();
    }
    
    return asynSuccess;
}

void TR_CAEN::setOpenState (OpenState open_state)
{
    // Remember the new open state.
    m_open_state = open_state;
    
    // Update the parameter value.
    setIntegerParam(m_asyn_params[OPEN_STATE], open_state);
    callParamCallbacks();
}

void TR_CAEN::requestedSampleRateChanged ()
{
    // The hardware does not support adjustment of the sample rate.
    // Just resport what the sample rate is, as we were told via the
    // HW_SAMPLE_RATE parameter.
    
    double sample_rate;
    getDoubleParam(m_asyn_params[HW_SAMPLE_RATE], &sample_rate);
    
    setAchievableSampleRate(sample_rate);
}

bool TR_CAEN::waitForPreconditions ()
{
    char const *function = "waitForPreconditions";
    
    // Check that the device is open.
    if (m_open_state != OpenStateOpened) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "%s %s: Device is not open.\n",
            portName, function);
        return false;
    }
    
    // Wait until any reset or calibrate request is completed.
    while (m_resetting || m_calibrating) {
        m_async_op_completed.tryWait(); // avoid unneeded wakeup next wait
        unlock();
        m_async_op_completed.wait();
        lock();
    }
    
    // After this, the device will remain open and calibrate/request will
    // not be done until disarming is completed. This is guaranteed by
    // the isArmed check in handleOpenStateRequest, handleResetRequest
    // and handleCalibrateRequest.
    
    return true;
}

bool TR_CAEN::checkSettings (TRArmInfo &arm_info)
{
    // Check the start/stop mode.
    int start_stop_mode = m_param_start_stop_mode.getSnapshot();
    if (start_stop_mode != CAEN_DGTZ_SW_CONTROLLED &&
        start_stop_mode != CAEN_DGTZ_S_IN_CONTROLLED &&
        start_stop_mode != CAEN_DGTZ_FIRST_TRG_CONTROLLED)
    {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "%s checkSettings: Invalid START_STOP_MODE.\n",
            portName);
        return false;
    }
    
    // Check the run/start/stop delay.
    double delay = m_param_run_start_stop_delay.getSnapshot();
    if (!(delay >= 0 || delay <= std::numeric_limits<uint32_t>::max())) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "%s checkSettings: Invalid RUN_START_STOP_DELAY.\n",
            portName);
        return false;
    }
    
    // Check channel-specific settings.
    for (int ch = 0; ch < MaxNumChannels; ch++) {
        // Check the input range.
        int input_range = m_param_channel[ch].input_range.getSnapshot();
        if (input_range != InputRange2V && input_range != InputRange05V) {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "%s checkSettings: Invalid CH%d_INPUT_RANGE.\n",
                portName, ch);
            return false;
        }
        
        // Check the pulse width.
        int pulse_width = m_param_channel[ch].pulse_width.getSnapshot();
        if (!(pulse_width >= 0 && pulse_width <= 255)) {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "%s checkSettings: Invalid CH%d_PULSE_WIDTH.\n",
                portName, ch);
            return false;
        }
    }
    
    // Return the sample rate for display.
    arm_info.rate_for_display = getAchievableSampleRateSnapshot();
    
    // TODO: Check for too large number of post samples.
    
    return true;
}

bool TR_CAEN::startAcquisition (bool had_overflow)
{
    assert(m_open_state == OpenStateOpened);
    
    char const *function = "startAcquisition";
    CAEN_DGTZ_ErrorCode err;
    
    // Sync with other code that modifies the AcqControl register.
    epicsGuard<epicsMutex> lock(m_acq_control_mutex);
    
    err = CAEN_DGTZ_SetAcquisitionMode(m_dev_handle, (CAEN_DGTZ_AcqMode_t)m_param_start_stop_mode.getSnapshot());
    if (err != CAEN_DGTZ_Success) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "%s %s: SetAcquisitionMode failed with error %d: %s.\n",
            portName, function, (int)err, m_error_codes.getErrorText(err));
        return false;
    }
    
    uint32_t delay_int = m_param_run_start_stop_delay.getSnapshot();
    if (!writeRegister(function, Registers::RunStartStopDelay, delay_int)) {
        return false;
    }
    
    int num_post_samples = getNumPostSamplesSnapshot();
    int remainder = num_post_samples % 4;
    if (remainder != 0) {
        // Round up to a multiple of 4.
        int incr = 4 - remainder;
        num_post_samples += std::min(incr, std::numeric_limits<int>::max() - num_post_samples);
    }
    
    err = CAEN_DGTZ_SetRecordLength(m_dev_handle, num_post_samples);
    if (err != CAEN_DGTZ_Success) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "%s %s: SetRecordLength failed with error %d: %s.\n",
            portName, function, (int)err, m_error_codes.getErrorText(err));
        return false;
    }
    
    for (int ch = 0; ch < MaxNumChannels; ch++) {
        uint32_t gain_value = m_param_channel[ch].input_range.getSnapshot() == InputRange05V;
        if (!writeRegister(function, Registers::ChannelGain[ch], gain_value)) {
            return false;
        }
        
        uint32_t pulse_width_value = m_param_channel[ch].pulse_width.getSnapshot();
        if (!modifyRegister(function, Registers::ChannelPulseWidth[ch], 0, 8, pulse_width_value)) {
            return false;
        }
    }
    
    err = CAEN_DGTZ_SWStartAcquisition(m_dev_handle);
    if (err != CAEN_DGTZ_Success) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "%s %s: SWStartAcquisition failed with error %d: %s.\n",
            portName, function, (int)err, m_error_codes.getErrorText(err));
        return false;
    }
    
    return true;
}

#if 0
bool TR_CAEN::readBurst ()
{
    // TODO
    return false;
}

bool TR_CAEN::checkOverflow (bool* had_overflow, int *num_buffer_bursts)
{
    // TODO
    return false;
}

bool TR_CAEN::processBurstData ()
{
    // TODO
    return false;
}

void TR_CAEN::interruptReading ()
{
    // TODO
}
#endif

void TR_CAEN::stopAcquisition ()
{
    assert(m_open_state == OpenStateOpened);
    
    CAEN_DGTZ_ErrorCode err;
    
    err = CAEN_DGTZ_SWStopAcquisition(m_dev_handle);
    if (err != CAEN_DGTZ_Success) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "%s stopAcquisition: SWStopAcquisition failed with error %d: %s.\n",
            portName, (int)err, m_error_codes.getErrorText(err));
    }
}

void TR_CAEN::runWorkerThreadTask (int id)
{
    #define TASK_CASE(id) case id: return handle##id();
    
    switch (id) {
        TASK_CASE(WorkerTaskOpenClose)
        TASK_CASE(WorkerTaskReset)
        TASK_CASE(WorkerTaskCalibrate)
        TASK_CASE(WorkerTaskRefresh)
        TASK_CASE(WorkerTaskSetFan)
        TASK_CASE(WorkerTaskSetClockSource)
        TASK_CASE(WorkerTaskSetSwTrigger)
        TASK_CASE(WorkerTaskSetExtTrigger)
        
        case WorkerTaskSetChSelfTrigger01:
        case WorkerTaskSetChSelfTrigger23:
        case WorkerTaskSetChSelfTrigger45:
        case WorkerTaskSetChSelfTrigger67:
            return handleWorkerTaskSetChSelfTrigger(id - WorkerTaskSetChSelfTrigger01);
        
        default: assert(false);
    }
    
    #undef TASK_CASE
}

void TR_CAEN::assertOpenFromWorker ()
{
    // The open state was OpenStateOpened when the request was requested,
    // at worst closing was requested by now but the close was not done
    // due to FIFO queuing of tasks.
    {
        epicsGuard<asynPortDriver> lock(*this);
        assert(m_open_state == OpenStateOpened || m_open_state == OpenStateClosing);
    }
}

void TR_CAEN::handleWorkerTaskOpenClose ()
{
    assert(m_open_state == OpenStateOpening || m_open_state == OpenStateClosing);
    
    bool opening = m_open_state == OpenStateOpening;
    
    // Do the open/close.
    bool success = opening ? openDigitizer() : closeDigitizer();
        
    {
        epicsGuard<asynPortDriver> lock(*this);
        
        // If the operation was successful go to the desired state,
        // otherwise go to the previous state.
        setOpenState((opening == success) ? OpenStateOpened : OpenStateClosed);
        
        if (success) {
            if (opening) {
                // When the device is opened, start tasks to apply some settings.
                m_worker_task[WorkerTaskSetFan].start();
                m_worker_task[WorkerTaskSetClockSource].start();
                m_worker_task[WorkerTaskSetSwTrigger].start();
                m_worker_task[WorkerTaskSetExtTrigger].start();
                for (int i = 0; i < NumChannelPairs; i++) {
                    m_worker_task[WorkerTaskSetChSelfTrigger01+i].start();
                }
            }
            else {
                // When the device is closed, reset readbacks.
                clearDigitizerInfo();
                setIntegerParamSuccess(m_asyn_params[FAN_CONTROL_MODE_RB], -1);
                setIntegerParamSuccess(m_asyn_params[CLOCK_SOURCE_RB],     -1);
                setIntegerParamSuccess(m_asyn_params[SW_TRIGGER_RB],       -1);
                setIntegerParamSuccess(m_asyn_params[EXT_TRIGGER_RB],      -1);
                for (int i = 0; i < NumChannelPairs; i++) {
                    setIntegerParamSuccess(m_asyn_params[CH_SELF_TRIGGER_01_RB+i], -1);
                }
                callParamCallbacks();
            }
        }
    }
}

void TR_CAEN::handleWorkerTaskReset ()
{
    assert(m_resetting);
    assertOpenFromWorker();
    
    // Do the reset.
    CAEN_DGTZ_ErrorCode ret = CAEN_DGTZ_Reset(m_dev_handle);
    
    bool success = ret == CAEN_DGTZ_Success;
    if (!success) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "%s resetDigitizer: Failed with error %d: %s.\n",
            portName, (int)ret, m_error_codes.getErrorText(ret));
    }
    
    {
        epicsGuard<asynPortDriver> lock(*this);
        
        // Set resetting back to false.
        m_resetting = false;
        
        // Report the result via the RESET parameter.
        setIntegerParam(m_asyn_params[RESET], success ? RequestStateSucceeded : RequestStateFailed);
        callParamCallbacks();
    }
    
    // Signal the event.
    m_async_op_completed.signal();
}

void TR_CAEN::handleWorkerTaskCalibrate ()
{
    assert(m_calibrating);
    assertOpenFromWorker();
    
    // Do the calibration.
    CAEN_DGTZ_ErrorCode ret = CAEN_DGTZ_Calibrate(m_dev_handle);
    
    bool success = ret == CAEN_DGTZ_Success;
    if (!success) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "%s calibrateDigitizer: Failed with error %d: %s.\n",
            portName, (int)ret, m_error_codes.getErrorText(ret));
    }
    
    {
        epicsGuard<asynPortDriver> lock(*this);
        
        // Set calibrating back to false.
        m_calibrating = false;
        
        // Report the result via the CALIBRATE parameter.
        setIntegerParam(m_asyn_params[CALIBRATE], success ? RequestStateSucceeded : RequestStateFailed);
        callParamCallbacks();
    }
    
    // Signal the event.
    m_async_op_completed.signal();
}

void TR_CAEN::handleWorkerTaskRefresh ()
{
    assert(m_refreshing);
    assertOpenFromWorker();
    
    // TODO
    bool success = true;
    
    {
        epicsGuard<asynPortDriver> lock(*this);
        
        // Set refreshing back to false.
        m_refreshing = false;
        
        // Report the result via the REFRESH parameter.
        setIntegerParam(m_asyn_params[REFRESH], success ? RequestStateSucceeded : RequestStateFailed);
        callParamCallbacks();
    }
}

void TR_CAEN::handleWorkerTaskSetFan ()
{
    assertOpenFromWorker();
    
    char const *function = "handleSetFanFromWorker";
    
    // Get the desired value from the parameter.
    int request;
    {
        epicsGuard<asynPortDriver> lock(*this);
        getIntegerParam(m_asyn_params[FAN_CONTROL_MODE], &request);
    }
    
    // Apply value to hardware.
    uint32_t fan_control_bit = request == FanControlModeFullSpeed;
    modifyRegister(function, Registers::FanSpeedControl, 3, 1, fan_control_bit);
    
    // Update the readback.
    readFanControlMode();
}

void TR_CAEN::handleWorkerTaskSetClockSource ()
{
    assertOpenFromWorker();
    
    char const *function = "handleSetClockSourceFromWorker";
    
    // Get the desired value from the parameter.
    int request;
    {
        epicsGuard<asynPortDriver> lock(*this);
        getIntegerParam(m_asyn_params[CLOCK_SOURCE], &request);
    }
    
    // Apply value to hardware.
    {
        epicsGuard<epicsMutex> lock(m_acq_control_mutex);
        uint32_t clock_source_value = request == ClockSourceExternal;
        modifyRegister(function, Registers::AcqControl, 6, 1, clock_source_value);
    }
    
    // Update the readback.
    readClockSource();
}

void TR_CAEN::handleWorkerTaskSetSwTrigger ()
{
    assertOpenFromWorker();
    
    char const *function = "handleWorkerTaskSetSwTrigger";
    
    // Get the desired value from the parameter.
    int request;
    {
        epicsGuard<asynPortDriver> lock(*this);
        getIntegerParam(m_asyn_params[SW_TRIGGER], &request);
    }
    
    // Apply value to hardware.
    uint32_t trigger_value = request == TriggerModeEnable;
    modifyRegister(function, Registers::TriggerSourceEnableMask, 31, 1, trigger_value);
    
    // Update the readback.
    readSwTrigger();
}

void TR_CAEN::handleWorkerTaskSetExtTrigger ()
{
    assertOpenFromWorker();
    
    char const *function = "handleWorkerTaskSetExtTrigger";
    
    // Get the desired value from the parameter.
    int request;
    {
        epicsGuard<asynPortDriver> lock(*this);
        getIntegerParam(m_asyn_params[EXT_TRIGGER], &request);
    }
    
    // Apply value to hardware.
    uint32_t trigger_value = request == TriggerModeEnable;
    modifyRegister(function, Registers::TriggerSourceEnableMask, 30, 1, trigger_value);
    
    // Update the readback.
    readExtTrigger();
}

void TR_CAEN::handleWorkerTaskSetChSelfTrigger (int ch_pair)
{
    assert(ch_pair >= 0 && ch_pair < NumChannelPairs);
    assertOpenFromWorker();
    
    char const *function = "handleWorkerTaskSetChSelfTrigger";
    
    // Get the desired value from the parameter.
    int request;
    {
        epicsGuard<asynPortDriver> lock(*this);
        getIntegerParam(m_asyn_params[CH_SELF_TRIGGER_01+ch_pair], &request);
    }
    
    // Apply value to hardware.
    uint32_t trigger_value = request == TriggerModeEnable;
    modifyRegister(function, Registers::TriggerSourceEnableMask, ch_pair, 1, trigger_value);
    
    // Update the readback.
    readChSelfTrigger(ch_pair);
}

bool TR_CAEN::openDigitizer ()
{
    // Parse the address string.
    int link_number;
    int conet_node;
    if (!TR_CAEN_ParseAddrStr(m_device_addr_str, &link_number, &conet_node)) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "%s openDigitizer: Bad device address string.\n",
            portName);
        return false;
    }
    
    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, "%s Opening digitizer (link_number=%d conet_node=%d).\n",
        portName, link_number, conet_node);
    
    CAEN_DGTZ_ErrorCode ret = CAEN_DGTZ_OpenDigitizer(
        CAEN_DGTZ_OpticalLink, link_number, conet_node, 0, &m_dev_handle);
    
    if (ret != CAEN_DGTZ_Success) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "%s openDigitizer: Failed with error %d: %s.\n",
            portName, (int)ret, m_error_codes.getErrorText(ret));
        return false;
    }
    
    // Read the digitizer information.
    refreshDigitizerInfo();
    
    return true;
}

bool TR_CAEN::closeDigitizer ()
{
    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, "%s Closing digitizer.\n", portName);
    
    CAEN_DGTZ_ErrorCode ret = CAEN_DGTZ_CloseDigitizer(m_dev_handle);
    
    if (ret != CAEN_DGTZ_Success) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "%s closeDigitizer: Failed with error %d: %s.\n",
            portName, (int)ret, m_error_codes.getErrorText(ret));
        // For now we are assuming that the device handle is still valid.
        // If issues turn up this can be changed.
        return false;
    }
    
    return true;
}

bool TR_CAEN::refreshDigitizerInfo ()
{
    char const *function = "refreshDigitizerInfo";
    CAEN_DGTZ_ErrorCode err;
    
    // Call the GetInfo function to get what the driver gives us directly.
    CAEN_DGTZ_BoardInfo_t info;
    err = CAEN_DGTZ_GetInfo(m_dev_handle, &info);
    if (err != CAEN_DGTZ_Success) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "%s %s: GetInfo failed with error %d: %s.\n",
            portName, function, (int)err, m_error_codes.getErrorText(err));
        return false;
    }
    
    // Read the board info register so we can find the memory size.
    uint32_t board_info_reg;
    if (!readRegister(function, Registers::BoardInfo, &board_info_reg)) {
        return false;
    }
    
    // The memory size is encoded in the register in units of 640000 samples.
    int32_t ch_mem_size = 640000 * ((board_info_reg >> 8) & 0xFF);
    
    // Update parameters.
    {
        epicsGuard<asynPortDriver> lock(*this);
        
        setStringParam(m_asyn_params[INFO_MODEL_NAME], info.ModelName);
        setStringParam(m_asyn_params[INFO_ROC_FW_REV], info.ROC_FirmwareRel);
        setStringParam(m_asyn_params[INFO_AMC_FW_REV], info.AMC_FirmwareRel);
        setIntegerParam(m_asyn_params[INFO_PCB_REVISION], info.PCB_Revision);
        setIntegerParam(m_asyn_params[INFO_SERIAL_NUM], info.SerialNumber);
        setIntegerParam(m_asyn_params[INFO_NUM_CHANNELS], info.Channels);
        setIntegerParam(m_asyn_params[INFO_FAMILY], info.FamilyCode);
        setIntegerParam(m_asyn_params[INFO_CH_MEM_SIZE], ch_mem_size);
        
        callParamCallbacks();
    }
    
    return true;
}

void TR_CAEN::clearDigitizerInfo ()
{
    setStringParam(m_asyn_params[INFO_MODEL_NAME], "");
    setStringParam(m_asyn_params[INFO_ROC_FW_REV], "");
    setStringParam(m_asyn_params[INFO_AMC_FW_REV], "");
    setIntegerParam(m_asyn_params[INFO_PCB_REVISION], 0);
    setIntegerParam(m_asyn_params[INFO_SERIAL_NUM], 0);
    setIntegerParam(m_asyn_params[INFO_NUM_CHANNELS], 0);
    setIntegerParam(m_asyn_params[INFO_FAMILY], 0);
    setIntegerParam(m_asyn_params[INFO_CH_MEM_SIZE], 0);
}

void TR_CAEN::readFanControlMode ()
{
    char const *function = "readFanControlMode";
    
    asynStatus status = asynError;
    FanControlMode value;
    
    uint32_t fan_speed_control;
    if (readRegister(function, Registers::FanSpeedControl, &fan_speed_control)) {
        status = asynSuccess;
        value = TR_CAEN_GetBit(fan_speed_control, 3) ? FanControlModeFullSpeed : FanControlModeSlowAuto;
    }
    
    // Update the readback parameter.
    {
        epicsGuard<asynPortDriver> lock(*this);
        if (status == asynSuccess) {
            setIntegerParam(m_asyn_params[FAN_CONTROL_MODE_RB], value);
        }
        setParamStatus(m_asyn_params[FAN_CONTROL_MODE_RB], status);
        callParamCallbacks();
    }
}

void TR_CAEN::readClockSource ()
{
    char const *function = "readClockSource";
    
    asynStatus status = asynError;
    ClockSource value;
    
    uint32_t acq_control;
    if (readRegister(function, Registers::AcqControl, &acq_control)) {
        status = asynSuccess;
        value = TR_CAEN_GetBit(acq_control, 6) ? ClockSourceExternal : ClockSourceInternal;
    }
    
    // Update the readback parameter.
    {
        epicsGuard<asynPortDriver> lock(*this);
        if (status == asynSuccess) {
            setIntegerParam(m_asyn_params[CLOCK_SOURCE_RB], value);
        }
        setParamStatus(m_asyn_params[CLOCK_SOURCE_RB], status);
        callParamCallbacks();
    }
}

void TR_CAEN::readSwTrigger ()
{
    char const *function = "readSwTrigger";
    
    asynStatus status = asynError;
    TriggerMode value;
    
    uint32_t trigger_src_en_mask;
    if (readRegister(function, Registers::TriggerSourceEnableMask, &trigger_src_en_mask)) {
        status = asynSuccess;
        value = TR_CAEN_GetBit(trigger_src_en_mask, 31) ? TriggerModeEnable : TriggerModeDisable;
    }
    
    // Update the readback parameter.
    {
        epicsGuard<asynPortDriver> lock(*this);
        if (status == asynSuccess) {
            setIntegerParam(m_asyn_params[SW_TRIGGER_RB], value);
        }
        setParamStatus(m_asyn_params[SW_TRIGGER_RB], status);
        callParamCallbacks();
    }
}

void TR_CAEN::readExtTrigger ()
{
    char const *function = "readExtTrigger";
    
    asynStatus status = asynError;
    TriggerMode value;
    
    uint32_t trigger_src_en_mask;
    if (readRegister(function, Registers::TriggerSourceEnableMask, &trigger_src_en_mask)) {
        status = asynSuccess;
        value = TR_CAEN_GetBit(trigger_src_en_mask, 30) ? TriggerModeEnable : TriggerModeDisable;
    }
    
    // Update the readback parameter.
    {
        epicsGuard<asynPortDriver> lock(*this);
        if (status == asynSuccess) {
            setIntegerParam(m_asyn_params[EXT_TRIGGER_RB], value);
        }
        setParamStatus(m_asyn_params[EXT_TRIGGER_RB], status);
        callParamCallbacks();
    }
}

void TR_CAEN::readChSelfTrigger (int ch_pair)
{
    assert(ch_pair >= 0 && ch_pair < NumChannelPairs);
    
    char const *function = "readChSelfTrigger";
    
    asynStatus status = asynError;
    TriggerMode value;
    
    uint32_t trigger_src_en_mask;
    if (readRegister(function, Registers::TriggerSourceEnableMask, &trigger_src_en_mask)) {
        status = asynSuccess;
        value = TR_CAEN_GetBit(trigger_src_en_mask, ch_pair) ? TriggerModeEnable : TriggerModeDisable;
    }
    
    // Update the readback parameter.
    {
        epicsGuard<asynPortDriver> lock(*this);
        if (status == asynSuccess) {
            setIntegerParam(m_asyn_params[CH_SELF_TRIGGER_01_RB+ch_pair], value);
        }
        setParamStatus(m_asyn_params[CH_SELF_TRIGGER_01_RB+ch_pair], status);
        callParamCallbacks();
    }
}

bool TR_CAEN::readRegister (char const *function, TR_CAEN_Register const &reg, uint32_t *out_value)
{
    CAEN_DGTZ_ErrorCode err = CAEN_DGTZ_ReadRegister(m_dev_handle, reg.reg_addr, out_value);
    
    if (err != CAEN_DGTZ_Success) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "%s %s: ReadRegister(%s) failed with error %d: %s.\n",
            portName, function, reg.reg_name, (int)err, m_error_codes.getErrorText(err));
        return false;
    }
    
    return true;
}

bool TR_CAEN::writeRegister (char const *function, TR_CAEN_Register const &reg, uint32_t value)
{
    CAEN_DGTZ_ErrorCode err = CAEN_DGTZ_WriteRegister(m_dev_handle, reg.reg_addr, value);
    
    if (err != CAEN_DGTZ_Success) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "%s %s: WriteRegister(%s) failed with error %d: %s.\n",
            portName, function, reg.reg_name, (int)err, m_error_codes.getErrorText(err));
        return false;
    }
    
    return true;
}

bool TR_CAEN::modifyRegister (char const *function, TR_CAEN_Register const &reg, int bit_offset, int num_bits, uint32_t value)
{
    uint32_t reg_value;
    if (!readRegister(function, reg, &reg_value)) {
        return false;
    }
    
    TR_CAEN_SetBits(&reg_value, bit_offset, num_bits, value);
    
    return writeRegister(function, reg, reg_value);
}

void TR_CAEN::setIntegerParamSuccess (int param, int value)
{
    setIntegerParam(param, value);
    setParamStatus(param, asynSuccess);
}

