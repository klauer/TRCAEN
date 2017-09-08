/* This file is part of the CAEN Digitizer Driver.
 * It is subject to the license terms in the LICENSE.txt file found in the
 * top-level directory of this distribution and at
 * https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html. No part
 * of the CAEN Digitizer Driver, including this file, may be copied,
 * modified, propagated, or distributed except according to the terms
 * contained in the LICENSE.txt file.
 */

#ifndef TR_CAEN_H
#define TR_CAEN_H

#include <stddef.h>
#include <stdint.h>

#include <string>

#include <epicsEvent.h>
#include <epicsMutex.h>

#include <TRBaseDriver.h>
#include <TRWorkerThread.h>

#include "TR_CAEN_ErrorCodes.h"
#include "TR_CAEN_Registers.h"

class TR_CAEN;

class TR_CAEN : public TRBaseDriver, private TRWorkerThreadRunnable
{
public:
    TR_CAEN (
        char const *port_name, char const *device_addr_str,
        int read_thread_prio_epics, int read_thread_stack_size,
        int max_ad_buffers, size_t max_ad_memory);

private:
    // Typedef for less typing.
    typedef TR_CAEN_Registers Registers;
    
    // Maximum number of channels.
    static int const MaxNumChannels = 8;
    
    // Number of channel pairs.
    static int const NumChannelPairs = 4;
    
    // Enumeration of regular parameters
    enum CAENAsynParams {
        FIRST_PARAM,
        
        // Reports the current open state and when written requests opening/closing.
        OPEN_STATE = FIRST_PARAM,
        
        // The actual sample rate of the hardware, must be set correctly from EPICS.
        HW_SAMPLE_RATE,
        
        // These parameters implement asynchronous operations.
        // When read they report the current state (enum RequestState),
        // when written the operation is started.
        RESET,     // reset digitizer
        CALIBRATE, // perform autocalibration
        REFRESH,   // update states, check self disarm
        
        // Static information about the digitizer.
        INFO_MODEL_NAME,
        INFO_ROC_FW_REV,
        INFO_AMC_FW_REV,
        INFO_PCB_REVISION,
        INFO_SERIAL_NUM,
        INFO_NUM_CHANNELS,
        INFO_FAMILY,
        INFO_CH_MEM_SIZE,
        
        // Fan control mode control and readback.
        FAN_CONTROL_MODE,
        FAN_CONTROL_MODE_RB,
        
        // Clock source control and readback.
        CLOCK_SOURCE,
        CLOCK_SOURCE_RB,
        
        // Software trigger control and readback.
        SW_TRIGGER,
        SW_TRIGGER_RB,
        
        // External trigger control and readback.
        EXT_TRIGGER,
        EXT_TRIGGER_RB,
        
        // Channel self-trigger control and readback.
        CH_SELF_TRIGGER_01,
        CH_SELF_TRIGGER_23,
        CH_SELF_TRIGGER_45,
        CH_SELF_TRIGGER_67,
        CH_SELF_TRIGGER_01_RB,
        CH_SELF_TRIGGER_23_RB,
        CH_SELF_TRIGGER_45_RB,
        CH_SELF_TRIGGER_67_RB,
        
        NUM_CAEN_ASYN_PARAMS
    };
    
    // Enumeration of our worker thread task IDs.
    enum WorkerTask {
        WorkerTaskOpenClose,
        WorkerTaskReset,
        WorkerTaskCalibrate,
        WorkerTaskRefresh,
        WorkerTaskSetFan,
        WorkerTaskSetClockSource,
        WorkerTaskSetSwTrigger,
        WorkerTaskSetExtTrigger,
        WorkerTaskSetChSelfTrigger01,
        WorkerTaskSetChSelfTrigger23,
        WorkerTaskSetChSelfTrigger45,
        WorkerTaskSetChSelfTrigger67,
        NumWorkerTasks
    };
    
    // Enumeration of device opening states.
    enum OpenState {OpenStateClosed, OpenStateOpening, OpenStateOpened, OpenStateClosing};
    
    // States for requests.
    enum RequestState {RequestStateFailed, RequestStateSucceeded, RequestStateRunning};
    
    // Enumeration of clock sources.
    enum ClockSource {ClockSourceInternal, ClockSourceExternal};
    
    // Enumeration of fan control modes.
    enum FanControlMode {FanControlModeSlowAuto, FanControlModeFullSpeed};
    
    // Enumeration of input ranges.
    enum InputRange {InputRange2V, InputRange05V};
    
    // Enumeration of trigger enable/disable.
    enum TriggerMode {TriggerModeEnable, TriggerModeDisable};
    
    // List of regular parameters' asyn indices
    int m_asyn_params[NUM_CAEN_ASYN_PARAMS];

    // Concrete configuration parameters
    // NOTE: update NumCAENConfigParams on any change!
    TRConfigParam<int>         m_param_start_stop_mode;
    TRConfigParam<double>      m_param_run_start_stop_delay;
    struct {
        TRConfigParam<int>         input_range;
        TRConfigParam<int, double> pulse_width;
    } m_param_channel[MaxNumChannels];
    
    static int const NumCAENConfigParams = 2 + (MaxNumChannels * 2);

    // Map of CAEN error codes to descriptions.
    TR_CAEN_ErrorCodes m_error_codes;
    
    // Worker thread.
    TRWorkerThread m_worker;
    
    // Worker thread tasks.
    TRWorkerThreadTask m_worker_task[NumWorkerTasks];
    
    // Device address string.
    std::string m_device_addr_str;
    
    // Open state.
    OpenState m_open_state;
    
    // Whether we are resetting the digitizer.
    bool m_resetting;
    
    // Whether we are calibrating.
    bool m_calibrating;
    
    // Whether we are refreshing.
    bool m_refreshing;
    
    // Event signaled when reset or calibrate completes.
    epicsEvent m_async_op_completed;
    
    // Mutex used to prevent concurrent modification of the AcqControl register.
    epicsMutex m_acq_control_mutex;
    
    // Device handle (if any depending on OpenState).
    int m_dev_handle;

private:
    asynStatus writeInt32 (asynUser *pasynUser, int value); // override
    asynStatus readInt32 (asynUser *pasynUser, int32_t *value); // override

    asynStatus handleOpenStateRequest (int32_t request);
    asynStatus handleResetRequest ();
    asynStatus handleCalibrateRequest ();
    asynStatus handleRefreshRequest ();
    asynStatus handleFanControlModeRequest (int32_t value);
    asynStatus handleClockSourceRequest (int32_t value);
    asynStatus handleSwTriggerRequest (int32_t value);
    asynStatus handleExtTriggerRequest (int32_t value);
    asynStatus handleChSelfTriggerRequest (int32_t value, int ch_pair);
    
    void setOpenState (OpenState open_state);
    
    void requestedSampleRateChanged (); // override
    
    bool waitForPreconditions (); // override
    
    bool checkSettings (TRArmInfo &arm_info); // override

    bool startAcquisition (bool had_overflow); // override

    //bool readBurst (); // override

    //bool checkOverflow (bool *had_overflow, int *num_buffer_bursts); // override

    //bool processBurstData (); // override
    
    //void interruptReading (); // override
    
    void stopAcquisition (); // override
    
    void runWorkerThreadTask (int id); // override
    
    void assertOpenFromWorker ();
    
    void handleWorkerTaskOpenClose ();
    void handleWorkerTaskReset ();
    void handleWorkerTaskCalibrate ();
    void handleWorkerTaskRefresh ();
    void handleWorkerTaskSetFan ();
    void handleWorkerTaskSetClockSource ();
    void handleWorkerTaskSetSwTrigger ();
    void handleWorkerTaskSetExtTrigger ();
    void handleWorkerTaskSetChSelfTrigger (int ch_pair);
    
    bool openDigitizer ();
    bool closeDigitizer ();
    
    bool refreshDigitizerInfo ();
    void clearDigitizerInfo ();
    
    void readFanControlMode ();
    void readClockSource ();
    void readSwTrigger ();
    void readExtTrigger ();
    void readChSelfTrigger (int ch_pair);
    
    bool readRegister (char const *function, TR_CAEN_Register const &reg, uint32_t *out_value);
    bool writeRegister (char const *function, TR_CAEN_Register const &reg, uint32_t value);
    bool modifyRegister (char const *function, TR_CAEN_Register const &reg, int bit_offset, int num_bits, uint32_t value);
    
    void setIntegerParamSuccess (int param, int value);
};

#endif
