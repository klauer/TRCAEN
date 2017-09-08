#!/usr/bin/env python2.7

from __future__ import print_function

import sys
sys.dont_write_bytecode = True

import os
import sys
import argparse
from prepare_framework import Env, BaseModule
from prepare_framework import main as prepare_main


class Top(object):
    def __init__(self, top_path, args):
        self._args = args
        
        if args['base_path'] is not None:
            base_path = args['base_path']
            dont_make_base = True
        else:
            base_path = os.path.join(top_path, 'base')
            dont_make_base = False
        
        self.env = Env(top_path, base_path=base_path, dont_make_base=dont_make_base)
        
        asyn_args = dict()
        if args['asyn_path'] is not None:
            asyn_args.update(args_to_dict(path=args['asyn_path'], prebuilt=True))
        
        self.asyn = BaseModule(self.env, 'asyn', **asyn_args)
        
        adcore_args = dict()
        if args['adcore_path'] is not None:
            adcore_args.update(args_to_dict(path=args['adcore_path'], prebuilt=True))
        
        self.ADCore = BaseModule(self.env, 'ADCore',
            deps=[self.asyn], **adcore_args)
        
        self.TRCore = BaseModule(self.env, 'TRCore',
            release_name='TR_CORE',
            deps=[self.asyn, self.ADCore])
        
        self.TRGeneralStandards = BaseModule(self.env, 'TRGeneralStandards',
            release_name='TR_GENERAL_STANDARDS',
            deps=[self.asyn, self.ADCore, self.TRCore],
            extra_write_files=[
                ['configure/CONFIG_SITE.local', [
                    'GENSTDS_DRIVER = "{}"'.format(args['genstds_driver_path']),
                ]],
            ])
        
        self.TRCAEN = BaseModule(self.env, 'TRCAEN',
            release_name='TR_CAEN',
            deps=[self.asyn, self.ADCore, self.TRCore],
            extra_write_files=[
                ['configure/CONFIG_SITE.local', [
                    'CAEN_VMELIB_PATH = "{}"'.format(args['caen_vmelib_path']),
                    'CAEN_COMM_PATH = "{}"'.format(args['caen_comm_path']),
                    'CAEN_DIGITIZER_PATH = "{}"'.format(args['caen_digitizer_path']),
                ]],
            ])

        dmasup_args = dict()
        if args['dmasup_path'] is not None:
            dmasup_args.update(args_to_dict(path=args['dmasup_path'], prebuilt=True))
        
        self.rtemsVmeDmaSup = BaseModule(self.env, 'rtemsVmeDmaSup', **dmasup_args)

        self.TRSIS = BaseModule(self.env, 'TRSIS',
            release_name='TR_SIS',
            deps=[self.asyn, self.ADCore, self.TRCore, self.rtemsVmeDmaSup])
        
    def targets(self):
        targets = []
        if self._args['build_genstds']:
            targets.append(self.TRGeneralStandards)
        if self._args['build_caen']:
            targets.append(self.TRCAEN)
        if self._args['build_sis']:
            targets.append(self.TRSIS)
        return targets
    
    @staticmethod
    def add_args(parser):
        parser.add_argument('-b', '--base-path', help='Path to EPICS base (pre-built) to use instead of bundled')
        parser.add_argument('--asyn-path', help='Path to asyn (pre-built) to use instead of bundled')
        parser.add_argument('--adcore-path', help='Path to ADCore (pre-built) to use instead of bundled')
        parser.add_argument('--genstds', action='store_true', help='Build the General Standards digitizer code')
        parser.add_argument('-d', '--driver-path', help='Path to 16ai64ssa driver directory with headers')
        parser.add_argument('--caen', action='store_true', help='Build the CAEN digitizer code')
        parser.add_argument('--caen-vmelib-path', help='Path to CAENVMELib')
        parser.add_argument('--caen-comm-path', help='Path to CAENComm')
        parser.add_argument('--caen-digitizer-path', help='Path to CAENDigitizer')
        parser.add_argument('--sis', action='store_true', help='Build the SIS digitizer code')
        parser.add_argument('--dmasup-path', help='Path to pre-built module with rtemsVmeDmaSup to use instead '
                                                  'of bundled (to be used in RELEASE)')
    
    @staticmethod
    def process_args(args):
        base_path = check_directory_if_given(args.base_path, 'Base')
        asyn_path = check_directory_if_given(args.asyn_path, 'Asyn')
        adcore_path = check_directory_if_given(args.adcore_path, 'ADCore')
        
        build_genstds = args.genstds
        build_caen = args.caen
        build_sis = args.sis
        
        genstds_driver_path = 'GENSTDS_DRIVER_NOT_DEFINED'
        if build_genstds:
            genstds_driver_path = query_path('16ai64ssa driver', args.driver_path, '16ai64ssa.h')
        
        caen_vmelib_path = 'CAENVMELIB_NOT_DEFINED'
        caen_comm_path = 'CAENCOMM_NOT_DEFINED'
        caen_digitizer_path = 'CAENDIGITIZER_NOT_DEFINED'
        if build_caen:
            caen_vmelib_path = query_path('CAENVMELib', args.caen_vmelib_path, 'include/CAENVMElib.h')
            caen_comm_path = query_path('CAENComm', args.caen_comm_path, 'include/CAENComm.h')
            caen_digitizer_path = query_path('CAENDigitizer', args.caen_digitizer_path, 'include/CAENDigitizer.h')
        
        dmasup_path = args.dmasup_path
        if dmasup_path is not None:
            if not os.path.isdir(dmasup_path):
                print('rtemsVmeDmaSup path is incorrect (not a directory). Exiting.')
                sys.exit(1)
        
        return {
            'base_path': base_path,
            'asyn_path': asyn_path,
            'adcore_path': adcore_path,
            'build_genstds': build_genstds,
            'build_caen': build_caen,
            'build_sis': build_sis,
            'genstds_driver_path': genstds_driver_path,
            'caen_vmelib_path': caen_vmelib_path,
            'caen_comm_path': caen_comm_path,
            'caen_digitizer_path': caen_digitizer_path,
            'dmasup_path': dmasup_path,
        }


def query_path(name, arg_value, expected_file):
    if arg_value is None:
        print('Please enter the path to the {}: '.format(name))
        path = raw_input()
        if path is None:
            print('Nothing entered. Exiting.')
            sys.exit(1)
    else:
        path = arg_value
    
    if not os.path.isfile(os.path.join(path, expected_file)):
        print('The {} ({}) is incorrect, it must contain {}. Exiting.'.format(name, path, expected_file))
        sys.exit(1)
    
    return path


def check_directory_if_given(path, name):
    if path is not None:
        if not os.path.isdir(path):
            print('{} path is incorrect (not a directory). Exiting.'.format(name))
            sys.exit(1)
    return path


def args_to_dict(**kwargs):
    return kwargs


if __name__ == '__main__':
    prepare_main(Top)
