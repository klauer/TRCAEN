CAEN Digitizer EPICS Driver
---------------------------

A work-in-progress driver

Building
--------

To build the driver:

Clone TRCore and TRCAEN into the same directory.

```bash
$ git clone https://github.com/klauer/TRCore TRCore
$ git clone https://github.com/klauer/TRCAEN TRCAEN

$ python2 -B TRCAEN/prepare.py --caen \
    --base-path <path> --asyn-path <path> --adcore-path <path> \
    --caen-vmelib-path <path> --caen-comm-path <path> --caen-digitizer-path <path>
$ make
```

To run the IOC:
```bash
$ bash TRCAEN/iocBoot/iocCAENTestIoc/run.sh
```
