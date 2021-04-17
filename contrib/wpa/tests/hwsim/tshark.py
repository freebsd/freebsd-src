#
# tshark module - refactored from test_scan.py
#
# Copyright (c) 2014, Qualcomm Atheros, Inc.
# Copyright (c) 2015, Intel Mobile Communications GmbH
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

import time
import subprocess
import logging
logger = logging.getLogger()

from utils import *

class UnknownFieldsException(Exception):
    def __init__(self, fields):
        Exception.__init__(self, "unknown tshark fields %s" % ','.join(fields))
        self.fields = fields

_tshark_filter_arg = '-Y'

def _run_tshark(filename, filter, display=None, wait=True):
    global _tshark_filter_arg

    if wait:
        # wait a bit to make it more likely for wlantest sniffer to have
        # captured and written the results into a file that we can process here
        time.sleep(0.1)

    try:
        arg = ["tshark", "-r", filename,
               _tshark_filter_arg, filter]
        if display:
            arg.append('-Tfields')
            for d in display:
                arg.append('-e')
                arg.append(d)
        else:
            arg.append('-V')
        cmd = subprocess.Popen(arg, stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE)
    except Exception as e:
        logger.info("Could run run tshark check: " + str(e))
        if "No such file or directory: 'tshark'" in str(e):
            raise HwsimSkip("No tshark available")
        cmd = None
        return None

    output = cmd.communicate()
    out = output[0].decode(errors='ignore')
    out1 = output[1].decode()
    res = cmd.wait()
    if res == 1:
        errmsg = "Some fields aren't valid"
        if errmsg in out1:
            errors = out1.split('\n')
            fields = []
            collect = False
            for f in errors:
                if collect:
                    f = f.strip()
                    if f:
                        fields.append(f)
                    continue
                if errmsg in f:
                    collect = True
                    continue
            raise UnknownFieldsException(fields)
        # remember this for efficiency
        _tshark_filter_arg = '-R'
        arg[3] = '-R'
        cmd = subprocess.Popen(arg, stdout=subprocess.PIPE,
                               stderr=open('/dev/null', 'w'))
        out = cmd.communicate()[0].decode()
        cmd.wait()
    if res == 2:
        if "tshark: Neither" in out1 and "are field or protocol names" in out1:
            errors = out1.split('\n')
            fields = []
            for f in errors:
                if f.startswith("tshark: Neither "):
                    f = f.split(' ')[2].strip('"')
                    if f:
                        fields.append(f)
                    continue
            raise UnknownFieldsException(fields)

    return out

def run_tshark(filename, filter, display=None, wait=True):
    if display is None: display = []
    try:
        return _run_tshark(filename, filter.replace('wlan_mgt', 'wlan'),
                           [x.replace('wlan_mgt', 'wlan') for x in display],
                           wait)
    except UnknownFieldsException as e:
        all_wlan_mgt = True
        for f in e.fields:
            if not f.startswith('wlan_mgt.'):
                all_wlan_mgt = False
                break
        if not all_wlan_mgt:
            raise
        return _run_tshark(filename, filter, display, wait)

def run_tshark_json(filename, filter):
    arg = ["tshark", "-r", filename,
           _tshark_filter_arg, filter]
    arg.append('-Tjson')
    arg.append('-x')
    try:
        cmd = subprocess.Popen(arg, stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE)
    except Exception as e:
        logger.info("Could run run tshark: " + str(e))
        if "No such file or directory: 'tshark'" in str(e):
            raise HwsimSkip("No tshark available")
        return None
    output = cmd.communicate()
    out = output[0].decode()
    res = cmd.wait()
    return out
