# Module tests
# Copyright (c) 2014, Jouni Malinen <j@w1.fi>
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

import os
import time

import hostapd

def test_module_wpa_supplicant(dev, apdev, params):
    """wpa_supplicant module tests"""
    if "OK" not in dev[0].global_request("MODULE_TESTS"):
        raise Exception("Module tests failed")
    # allow eloop test to complete
    time.sleep(0.75)
    dev[0].relog()
    with open(os.path.join(params['logdir'], 'log0'), 'r') as f:
        res = f.read()
        if "FAIL - should not have called this function" in res:
            raise Exception("eloop test failed")

def test_module_hostapd(dev):
    """hostapd module tests"""
    hapd_global = hostapd.HostapdGlobal()
    if "OK" not in hapd_global.ctrl.request("MODULE_TESTS"):
        raise Exception("Module tests failed")
