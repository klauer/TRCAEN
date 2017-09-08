/* This file is part of the CAEN Digitizer Driver.
 * It is subject to the license terms in the LICENSE.txt file found in the
 * top-level directory of this distribution and at
 * https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html. No part
 * of the CAEN Digitizer Driver, including this file, may be copied,
 * modified, propagated, or distributed except according to the terms
 * contained in the LICENSE.txt file.
 */

#include <stddef.h>
#include <stdio.h>

#include <epicsThread.h>
#include <epicsExport.h>
#include <iocsh.h>

#include "TR_CAEN.h"

extern "C" int TR_CAEN_InitDevice(
    char const *port_name, char const *device_addr_str,
    int read_thread_prio_epics, int read_thread_stack_size,
    int max_ad_buffers, size_t max_ad_memory)
{
    if (port_name == NULL || device_addr_str == NULL ||
        read_thread_prio_epics < epicsThreadPriorityMin || read_thread_prio_epics > epicsThreadPriorityMax)
    { 
        fprintf(stderr, "TR_CAEN_InitDevice Error: parameters are not valid.\n");
        return 1;
    }
    
    TR_CAEN *driver = new TR_CAEN(
        port_name, device_addr_str, read_thread_prio_epics, read_thread_stack_size,
        max_ad_buffers, max_ad_memory);
    
    driver->completeInit();
    
#if 0
    if (!driver->Open()) {
        fprintf(stderr, "TR_CAEN_InitDevice Error: Failed to connect with driver.\n");
        // Must not delete driver to avoid crash in findAsynPortDriver.
        return 1;
    }
#endif
    
    return 0;
}

static const iocshArg initArg0 = {"port name", iocshArgString};
static const iocshArg initArg1 = {"device node", iocshArgString};
static const iocshArg initArg2 = {"read thread priority (EPICS units)", iocshArgInt};
static const iocshArg initArg3 = {"read thread stack size", iocshArgInt};
static const iocshArg initArg4 = {"max AreaDetector buffers", iocshArgInt};
static const iocshArg initArg5 = {"max AreaDetector memory", iocshArgInt};
static const iocshArg * const initArgs[] = {&initArg0, &initArg1, &initArg2, &initArg3, &initArg4, &initArg5};
static const iocshFuncDef initFuncDef = {"TR_CAEN_InitDevice", 6, initArgs};

static void initCallFunc(const iocshArgBuf *args)
{
    TR_CAEN_InitDevice(args[0].sval, args[1].sval, args[2].ival,
                       args[3].ival, args[4].ival, args[5].ival);
}

extern "C" {
    void TR_CAEN_Register(void)
    {
        iocshRegister(&initFuncDef, initCallFunc);
    }
    epicsExportRegistrar(TR_CAEN_Register);
}
