TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

# Build only for Linux (check OS part of T_A, e.g. linux in linux-x86_64).
ifeq ($(firstword $(subst -, ,$(T_A))),linux)

# CAEN library includes
USR_INCLUDES += -I$(CAEN_VMELIB_PATH)/include
USR_INCLUDES += -I$(CAEN_COMM_PATH)/include
USR_INCLUDES += -I$(CAEN_DIGITIZER_PATH)/include

# CAEN shared libraries
# These will be symlinked to the lib folder so they can be
# found when specified in the SYS_LIBS directives.
CAEN_VMELIB_LIB = $(shell echo "$(CAEN_VMELIB_PATH)/lib/x64/libCAENVME.so."*)
CAEN_COMM_LIB = $(shell echo "$(CAEN_COMM_PATH)/lib/x64/libCAENComm.so."*)
CAEN_DIGITIZER_LIB = $(shell echo "$(CAEN_DIGITIZER_PATH)/lib/x86_64/libCAENDigitizer.so."*)

# CAEN libraries for use in SYS_LIBS.
CAEN_SYS_LIBS = CAENDigitizer CAENComm CAENVME

#==================================================
# build a support library

LIBRARY_IOC += trCAEN

DBD += trCAEN.dbd

trCAEN_SRCS += TR_CAEN.cpp TR_CAEN_DevAddrStr.cpp TR_CAEN_ErrorCodes.cpp \
               TR_CAEN_Init.cpp TR_CAEN_Registers.cpp

# Hacky dependency to create library symlinks.
trCAEN_SRCS += CreateCaenLibLinks.cpp

trCAEN_LIBS += trCore ADBase asyn $(EPICS_BASE_IOC_LIBS)
trCAEN_SYS_LIBS += $(CAEN_SYS_LIBS)

#=============================
# Build the IOC application

PROD_IOC = CAENTestIoc
# CAENTestIoc.dbd will be created and installed
DBD += CAENTestIoc.dbd

# CAENTestIoc.dbd will be made up from these files:
CAENTestIoc_DBD += base.dbd

# Include dbd files from all support applications:
CAENTestIoc_DBD += asyn.dbd ADSupport.dbd NDPluginSupport.dbd trCAEN.dbd

# Add all the support libraries needed by this IOC
CAENTestIoc_LIBS += trCAEN trCore NDPlugin ADBase asyn
CAENTestIoc_SYS_LIBS += $(CAEN_SYS_LIBS)

# CAENTestIoc_registerRecordDeviceDriver.cpp derives from CAENTestIoc.dbd
CAENTestIoc_SRCS += CAENTestIoc_registerRecordDeviceDriver.cpp

# Build the main IOC entry point on workstation OSs.
CAENTestIoc_SRCS_DEFAULT += CAENTestIocMain.cpp
CAENTestIoc_SRCS_vxWorks += -nil-

# Add support from base/src/vxWorks if needed
#CAENTestIoc_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

# Finally link to the EPICS Base libraries
CAENTestIoc_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

# End confitional build.
endif

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

# Create symlinks for CAEN libraries.
CreateCaenLibLinks.cpp:
	mkdir -p ../../../lib/$(T_A)
	ln -s $(CAEN_VMELIB_LIB) ../../../lib/$(T_A)/libCAENVME.so
	ln -s $(CAEN_COMM_LIB) ../../../lib/$(T_A)/libCAENComm.so
	ln -s $(CAEN_DIGITIZER_LIB) ../../../lib/$(T_A)/libCAENDigitizer.so
	touch CreateCaenLibLinks.cpp
