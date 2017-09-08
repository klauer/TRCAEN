#!../../bin/linux-x86_64/CAENTestIoc

< envPaths

cd ${TOP}

## Basic configuration
# device identification
epicsEnvSet("CAEN_DEVICE", "0:0")
# prefix of all records
epicsEnvSet("PREFIX", "CAEN")
# Size (NELM) of waveform records.
epicsEnvSet("WAVEFORM_SIZE", "2048")
# Increase CA buffer sizes, needed for larger waveforms.
epicsEnvSet("EPICS_CA_MAX_ARRAY_BYTES", "65535")
# Device name (used in port identifiers and :name record).
epicsEnvSet("DEVICE_NAME", "CAEN0")
# The sample rate of the digitizer (specify correct value).
epicsEnvSet("HW_SAMPLE_RATE", "500000000")

## Thread configuration
# Priority of read thread (EPICS units 0-99)
epicsEnvSet("READ_THREAD_PRIORITY_EPICS", "50")
# Stack size of read thread (default medium).
epicsEnvSet("READ_THREAD_STACK_SIZE", "0")

## AreaDetector configuration
# Max buffers for the channel port
epicsEnvSet("MAX_AD_BUFFERS", "256")
# Max memory for the channels port
epicsEnvSet("MAX_AD_MEMORY", "0")

## Stdarrays plugins configuration.
# Blocking-callbacks for stdarrays ports.
epicsEnvSet("STDARRAYS_BLOCKING_CALLBACKS", "1")
# Queue size for stdarrays plugins (relevant only if blocking-callbacks is 0).
epicsEnvSet("STDARRAYS_QUEUE_SIZE", "3")
# Max-memory for stdarrays plugins.
epicsEnvSet("STDARRAYS_MAX_MEMORY", "-1")
# Thread priority for stdarrays plugins.
epicsEnvSet("STDARRAYS_PRIORITY", "0")
# Thread stack size for stdarrays plugins.
epicsEnvSet("STDARRAYS_STACK_SIZE", "16384")

## Scan configuration.
# SCAN for refreshing device states
epicsEnvSet("REFRESH_STATES_SCAN", "1 second")
# SCAN for slow snapshot records
epicsEnvSet("SNAP_SCAN", "1 second")


## Register all support components
dbLoadDatabase "dbd/CAENTestIoc.dbd"
CAENTestIoc_registerRecordDeviceDriver pdbbase

# Initialize the main port.
TR_CAEN_InitDevice("$(DEVICE_NAME)", "$(CAEN_DEVICE)", "$(READ_THREAD_PRIORITY_EPICS)", "$(READ_THREAD_STACK_SIZE)", "$(MAX_AD_BUFFERS)", "$(MAX_AD_MEMORY)")

# Initialize the channel ports (generated using gen_channels.py).
< iocBoot/iocCAENTestIoc/CAENInitChannels.cmd

# Load main records.
dbLoadRecords("$(TR_CORE)/db/TRBase.db", "PREFIX=$(PREFIX), PORT=$(DEVICE_NAME), SIZE=$(WAVEFORM_SIZE), PRESAMPLES=#")
dbLoadRecords("db/TRCAEN.db", "PREFIX=$(PREFIX), PORT=$(DEVICE_NAME), REFRESH_STATES_SCAN=$(REFRESH_STATES_SCAN), HW_SAMPLE_RATE=$(HW_SAMPLE_RATE)")
dbLoadRecords("$(TR_CORE)/db/TRGenericRequest.db", "PREFIX=$(PREFIX), PORT=$(DEVICE_NAME), REQUEST=RESET")
dbLoadRecords("$(TR_CORE)/db/TRGenericRequest.db", "PREFIX=$(PREFIX), PORT=$(DEVICE_NAME), REQUEST=CALIBRATE")
dbLoadRecords("$(TR_CORE)/db/TRGenericRequest.db", "PREFIX=$(PREFIX), PORT=$(DEVICE_NAME), REQUEST=REFRESH")

# Load channel-specific records (generated using gen_channels.py).
< iocBoot/iocCAENTestIoc/CAENLoadChannelsDb.cmd

# Initialize IOC.
cd ${TOP}/iocBoot/${IOC}
iocInit
