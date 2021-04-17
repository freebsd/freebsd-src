#!/usr/bin/env python3
#
# Remote test case executor
# Copyright (c) 2016, Tieto Corporation
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

import os
import re
import sys
import time
import traceback
import getopt
from datetime import datetime
from random import shuffle

import logging
logger = logging.getLogger()

scriptsdir = os.path.dirname(os.path.realpath(sys.modules[__name__].__file__))
sys.path.append(os.path.join(scriptsdir, '..', '..', 'wpaspy'))
sys.path.append(os.path.join(scriptsdir, '..', 'hwsim'))

import wpaspy
import config
from test_devices import show_devices
from test_devices import check_devices
from rutils import TestSkip
from utils import HwsimSkip
from hwsim_wrapper import run_hwsim_test

def usage():
    print("USAGE: " + sys.argv[0] + " -t devices")
    print("USAGE: " + sys.argv[0] + " -t check_devices")
    print("USAGE: " + sys.argv[0] + " -d <dut_name> -t <all|sanity|tests_to_run> [-r <ref_name>] [-c <cfg_file.py>] [-m <all|monitor_name>] [-h hwsim_tests] [-f hwsim_modules][-R][-T][-P][-S][-v]")
    print("USAGE: " + sys.argv[0])

def get_devices(devices, duts, refs, monitors):
    for dut in duts:
        config.get_device(devices, dut, lock=True)
    for ref in refs:
        config.get_device(devices, ref, lock=True)
    for monitor in monitors:
        if monitor == "all":
            continue
        if monitor in duts:
            continue
        if monitor in refs:
            continue
        config.get_device(devices, monitor, lock=True)

def put_devices(devices, duts, refs, monitors):
    for dut in duts:
        config.put_device(devices, dut)
    for ref in refs:
        config.put_device(devices, ref)
    for monitor in monitors:
        if monitor == "all":
            continue
        if monitor in duts:
            continue
        if monitor in refs:
            continue
        config.put_device(devices, monitor)

def main():
    duts = []
    refs = []
    monitors = []
    filter_keys = []
    requested_tests = ["help"]
    requested_hwsim_tests = []
    hwsim_tests = []
    requested_modules = []
    modules_tests = []
    cfg_file = "cfg.py"
    log_dir = "./logs/"
    verbose = False
    trace = False
    restart = False
    perf = False
    shuffle_tests = False

    # parse input parameters
    try:
        opts, args = getopt.getopt(sys.argv[1:], "d:f:r:t:l:k:c:m:h:vRPTS",
                                   ["dut=", "modules=", "ref=", "tests=",
                                    "log-dir=",
                                    "cfg=", "key=", "monitor=", "hwsim="])
    except getopt.GetoptError as err:
        print(err)
        usage()
        sys.exit(2)

    for option, argument in opts:
        if option == "-v":
            verbose = True
        elif option == "-R":
            restart = True
        elif option == "-T":
            trace = True
        elif option == "-P":
            perf = True
        elif option == "-S":
            shuffle_tests = True
        elif option in ("-d", "--dut"):
            duts.append(argument)
        elif option in ("-r", "--ref"):
            refs.append(argument)
        elif option in ("-t", "--tests"):
            requested_tests = re.split('; | |, ', argument)
        elif option in ("-l", "--log-dir"):
            log_dir = argument
        elif option in ("-k", "--key"):
            filter_keys.append(argument)
        elif option in ("-m", "--monitor"):
            monitors.append(argument)
        elif option in ("-c", "--cfg"):
            cfg_file = argument
        elif option in ("-h", "--hwsim"):
            requested_hwsim_tests = re.split('; | |, ', argument)
        elif option in ("-f", "--modules"):
            requested_modules = re.split('; | |, ', argument)
        else:
            assert False, "unhandled option"

    # get env configuration
    setup_params = config.get_setup_params(cfg_file)
    devices = config.get_devices(cfg_file)

    # put logs in log_dir
    symlink = os.path.join(log_dir, "current");
    if os.path.exists(symlink):
        os.unlink(symlink)
    log_dir = os.path.join(log_dir, time.strftime("%Y_%m_%d_%H_%M_%S"))
    if not os.path.exists(log_dir):
        os.makedirs(log_dir)
    os.symlink(os.path.join("../", log_dir), symlink)

    # setup restart/trace/perf request
    setup_params['local_log_dir'] = log_dir
    setup_params['restart_device'] = restart
    setup_params['trace'] = trace
    setup_params['perf'] = perf

    # configure logger
    logger.setLevel(logging.DEBUG)

    stdout_handler = logging.StreamHandler()
    stdout_handler.setLevel(logging.WARNING)
    if verbose:
        stdout_handler.setLevel(logging.DEBUG)
    logger.addHandler(stdout_handler)

    formatter = logging.Formatter('%(asctime)s - %(message)s')
    file_name = os.path.join(log_dir, 'run-tests.log')
    log_handler = logging.FileHandler(file_name)
    log_handler.setLevel(logging.DEBUG)
    log_handler.setFormatter(formatter)
    logger.addHandler(log_handler)

    # import available tests
    tests = []
    failed = []
    test_modules = []
    files = os.listdir(scriptsdir)
    for t in files:
        m = re.match(r'(test_.*)\.py$', t)
        if m:
            mod = __import__(m.group(1))
            test_modules.append(mod.__name__.replace('test_', '', 1))
            for key, val in mod.__dict__.items():
                if key.startswith("test_"):
                    tests.append(val)
    test_names = list(set([t.__name__.replace('test_', '', 1) for t in tests]))

    # import test_*
    files = os.listdir("../hwsim/")
    for t in files:
        m = re.match(r'(test_.*)\.py$', t)
        if m:
            mod = __import__(m.group(1))
            test_modules.append(mod.__name__.replace('test_', '', 1))
            for key, val in mod.__dict__.items():
                if key.startswith("test_"):
                    hwsim_tests.append(val)

    # setup hwsim tests
    hwsim_tests_to_run = []
    if len(requested_hwsim_tests) > 0:
        # apply filters
        for filter_key in filter_keys:
            filtered_tests = []
            for hwsim_test in hwsim_tests:
                if re.search(filter_key, hwsim_test.__name__):
                    filtered_tests.append(hwsim_test)
            hwsim_tests = filtered_tests

        # setup hwsim_test we should run
        if requested_hwsim_tests[0] == "all":
            hwsim_tests_to_run = hwsim_tests
        elif requested_hwsim_tests[0] == "remote":
            hwsim_tests_to_run = [t for t in hwsim_tests
                                  if hasattr(t, "remote_compatible") and
                                     t.remote_compatible]
        else:
            for test in requested_hwsim_tests:
                t = None
                for tt in hwsim_tests:
                    name = tt.__name__.replace('test_', '', 1)
                    if name == test:
                        t = tt
                        break
                if not t:
                    logger.warning("hwsim test case: " + test + " NOT-FOUND")
                    continue
                hwsim_tests_to_run.append(t)

    # import test_* from modules
    files = os.listdir("../hwsim/")
    for t in files:
        m = re.match(r'(test_.*)\.py$', t)
        if m:
            mod = __import__(m.group(1))
            if mod.__name__.replace('test_', '', 1) not in requested_modules:
                continue
            for key, val in mod.__dict__.items():
                if key.startswith("test_"):
                    modules_tests.append(val)

    if len(requested_modules) > 0:
        requested_hwsim_tests = modules_tests
        hwsim_tests_to_run = modules_tests

    # sort the list
    test_names.sort()
    tests.sort(key=lambda t: t.__name__)

    # print help
    if requested_tests[0] == "help" and len(requested_hwsim_tests) == 0:
        usage()
        print("\nAvailable Devices:")
        for device in devices:
            print("\t", device['name'])
        print("\nAvailable tests:")
        for test in test_names:
            print("\t", test)
        print("\nAvailable hwsim tests:")
        for hwsim_test in hwsim_tests:
            print("\t", hwsim_test.__name__.replace('test_', '', 1))
        return

    # show/check devices
    if requested_tests[0] == "devices":
        show_devices(devices, setup_params)
        return

    # apply filters
    for filter_key in filter_keys:
        filtered_tests = []
        for test in tests:
            if re.search(filter_key, test.__name__):
                filtered_tests.append(test)
        tests = filtered_tests

    # setup test we should run
    tests_to_run = []
    if requested_tests[0] == "all":
        tests_to_run = tests
    if requested_tests[0] == "help":
        pass
    elif requested_tests[0] == "sanity":
        for test in tests:
            if test.__name__.startswith("test_sanity_"):
                tests_to_run.append(test)
    else:
        for test in requested_tests:
            t = None
            for tt in tests:
                name = tt.__name__.replace('test_', '', 1)
                if name == test:
                    t = tt
                    break
            if not t:
                logger.warning("test case: " + test + " NOT-FOUND")
                continue
            tests_to_run.append(t)

    if shuffle_tests:
        shuffle(tests_to_run)
        shuffle(hwsim_tests_to_run)

    # lock devices
    try:
        get_devices(devices, duts, refs, monitors)
    except Exception as e:
        logger.warning("get devices failed: " + str(e))
        logger.info(traceback.format_exc())
        put_devices(devices, duts, refs, monitors)
        return
    except:
        logger.warning("get devices failed")
        logger.info(traceback.format_exc())
        put_devices(devices, duts, refs, monitors)
        return

    # now run test cases
    for dut in duts:
        if len(requested_hwsim_tests) > 0:
            logger.warning("DUT (apdev): " + str(dut))
        else:
            logger.warning("DUT: " + str(dut))
    for ref in refs:
        if len(requested_hwsim_tests) > 0:
            logger.warning("REF   (dev): " + str(ref))
        else:
            logger.warning("REF: " + str(ref))
    for monitor in monitors:
        logger.warning("MON: " + str(monitor))

    # run check_devices at beginning
    logger.warning("RUN check_devices")
    try:
        check_devices(devices, setup_params, refs, duts, monitors)
    except Exception as e:
        logger.warning("FAILED: " + str(e))
        logger.info(traceback.format_exc())
        put_devices(devices, duts, refs, monitors)
        return
    except:
        logger.warning("FAILED")
        logger.info(traceback.format_exc())
        put_devices(devices, duts, refs, monitors)
        return
    logger.warning("PASS")

    test_no = 1
    for test in tests_to_run:
        try:
            start = datetime.now()
            setup_params['tc_name'] = test.__name__.replace('test_', '', 1)
            logger.warning("START - " + setup_params['tc_name'] + " (" + str(test_no) + "/" + str(len(tests_to_run)) + ")")
            if test.__doc__:
                logger.info("Test: " + test.__doc__)

            # run tc
            res = test(devices, setup_params, refs, duts, monitors)

            end = datetime.now()
            logger.warning("PASS (" + res + ") - " + str((end - start).total_seconds()) + "s")
        except KeyboardInterrupt:
            put_devices(devices, duts, refs, monitors)
            raise
        except TestSkip as e:
            end = datetime.now()
            logger.warning("SKIP (" + str(e) + ") - " + str((end - start).total_seconds()) + "s")
        except Exception as e:
            end = datetime.now()
            logger.warning("FAILED (" + str(e) + ") - " + str((end - start).total_seconds()) + "s")
            logger.info(traceback.format_exc())
            failed.append(test.__name__.replace('test_', '', 1))
        except:
            end = datetime.now()
            logger.warning("FAILED - " + str((end - start).total_seconds()) + "s")
            logger.info(traceback.format_exc())
            failed.append(test.__name__.replace('test_', '', 1))
        test_no += 1

    test_no = 1
    for hwsim_test in hwsim_tests_to_run:
        try:
            start = datetime.now()
            setup_params['tc_name'] = hwsim_test.__name__.replace('test_', '', 1)
            logger.warning("START - " + setup_params['tc_name'] + " (" + str(test_no) + "/" + str(len(hwsim_tests_to_run)) + ")")
            res = run_hwsim_test(devices, setup_params, refs, duts, monitors, hwsim_test)
            end = datetime.now()
            logger.warning("PASS (" + res + ") - " + str((end - start).total_seconds()) + "s")
        except KeyboardInterrupt:
            put_devices(devices, duts, refs, monitors)
            raise
        except HwsimSkip as e:
            end = datetime.now()
            logger.warning("SKIP (" + str(e) + ") - " + str((end - start).total_seconds()) + "s")
            failed.append(hwsim_test.__name__.replace('test_', '', 1))
        except Exception as e:
            end = datetime.now()
            logger.warning("FAILED (" + str(e) + ") - " + str((end - start).total_seconds()) + "s")
            logger.info(traceback.format_exc())
            failed.append(hwsim_test.__name__.replace('test_', '', 1))
        except:
            end = datetime.now()
            logger.warning("FAILED - " + str((end - start).total_seconds()) + "s")
            logger.info(traceback.format_exc())
            failed.append(hwsim_test.__name__.replace('test_', '', 1))
        test_no += 1

    # unlock devices
    put_devices(devices, duts, refs, monitors)

    if len(failed) > 0:
        logger.warning("Failed test cases:")
        for test in failed:
            logger.warning("\t" + test)


if __name__ == "__main__":
        main()
