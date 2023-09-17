# Test cases for Device Provisioning Protocol (DPP) version 3
# Copyright (c) 2021, Qualcomm Innovation Center, Inc.
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

from test_dpp import check_dpp_capab, run_dpp_auto_connect

def test_dpp_network_intro_version(dev, apdev):
    """DPP Network Introduction and protocol version"""
    check_dpp_capab(dev[0], min_ver=3)

    try:
        id, hapd = run_dpp_auto_connect(dev, apdev, 1, stop_after_prov=True)
        dev[0].select_network(id, freq=2412)
        dev[0].wait_connected()
    finally:
        dev[0].set("dpp_config_processing", "0", allow_fail=True)

def test_dpp_network_intro_version_change(dev, apdev):
    """DPP Network Introduction and protocol version change"""
    check_dpp_capab(dev[0], min_ver=3)

    try:
        dev[0].set("dpp_version_override", "2")
        id, hapd = run_dpp_auto_connect(dev, apdev, 1, stop_after_prov=True)
        dev[0].set("dpp_version_override", "3")
        dev[0].select_network(id, freq=2412)
        dev[0].wait_connected()
    finally:
        dev[0].set("dpp_config_processing", "0", allow_fail=True)

def test_dpp_network_intro_version_missing_req(dev, apdev):
    """DPP Network Introduction and protocol version missing from request"""
    check_dpp_capab(dev[0], min_ver=3)

    try:
        dev[0].set("dpp_version_override", "2")
        id, hapd = run_dpp_auto_connect(dev, apdev, 1, stop_after_prov=True)
        dev[0].set("dpp_version_override", "3")
        dev[0].set("dpp_test", "92")
        dev[0].select_network(id, freq=2412)
        ev = dev[0].wait_event(["DPP-INTRO"], timeout=10)
        if ev is None:
            raise Exception("DPP network introduction result not seen on STA")
        if "status=8" not in ev:
            raise Exception("Unexpected network introduction result on STA: " + ev)
    finally:
        dev[0].set("dpp_config_processing", "0", allow_fail=True)
