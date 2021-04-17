# bgscan tests
# Copyright (c) 2014-2017, Jouni Malinen <j@w1.fi>
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

import time
import logging
logger = logging.getLogger()
import os

import hostapd
from utils import alloc_fail, fail_test

def test_bgscan_simple(dev, apdev):
    """bgscan_simple"""
    hostapd.add_ap(apdev[0], {"ssid": "bgscan"})
    hostapd.add_ap(apdev[1], {"ssid": "bgscan"})

    dev[0].connect("bgscan", key_mgmt="NONE", scan_freq="2412",
                   bgscan="simple:1:-20:2")
    dev[1].connect("bgscan", key_mgmt="NONE", scan_freq="2412",
                   bgscan="simple:1:-45:2")

    dev[2].connect("bgscan", key_mgmt="NONE", scan_freq="2412",
                   bgscan="simple:1:-45")
    dev[2].request("REMOVE_NETWORK all")
    dev[2].wait_disconnected()

    dev[2].connect("bgscan", key_mgmt="NONE", scan_freq="2412",
                   bgscan="simple:0:0")
    dev[2].request("REMOVE_NETWORK all")
    dev[2].wait_disconnected()

    dev[2].connect("bgscan", key_mgmt="NONE", scan_freq="2412",
                   bgscan="simple")
    dev[2].request("REMOVE_NETWORK all")
    dev[2].wait_disconnected()

    dev[2].connect("bgscan", key_mgmt="NONE", scan_freq="2412",
                   bgscan="simple:1")
    dev[2].request("REMOVE_NETWORK all")
    dev[2].wait_disconnected()

    ev = dev[0].wait_event(["CTRL-EVENT-SIGNAL-CHANGE"], timeout=10)
    if ev is None:
        raise Exception("dev0 did not indicate signal change event")
    if "above=0" not in ev:
        raise Exception("Unexpected signal change event contents from dev0: " + ev)

    ev = dev[1].wait_event(["CTRL-EVENT-SIGNAL-CHANGE"], timeout=10)
    if ev is None:
        raise Exception("dev1 did not indicate signal change event")
    if "above=1" not in ev:
        raise Exception("Unexpected signal change event contents from dev1: " + ev)

    ev = dev[0].wait_event(["CTRL-EVENT-SCAN-STARTED"], timeout=3)
    if ev is None:
        raise Exception("dev0 did not start a scan")

    ev = dev[1].wait_event(["CTRL-EVENT-SCAN-STARTED"], timeout=3)
    if ev is None:
        raise Exception("dev1 did not start a scan")

    ev = dev[0].wait_event(["CTRL-EVENT-SCAN-RESULTS"], 5)
    if ev is None:
        raise Exception("dev0 did not complete a scan")
    ev = dev[1].wait_event(["CTRL-EVENT-SCAN-RESULTS"], 5)
    if ev is None:
        raise Exception("dev1 did not complete a scan")

def test_bgscan_simple_beacon_loss(dev, apdev):
    """bgscan_simple and beacon loss"""
    params = hostapd.wpa2_params(ssid="bgscan", passphrase="12345678")
    params["wpa_key_mgmt"] = "WPA-PSK-SHA256"
    params["ieee80211w"] = "2"
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].set("disable_sa_query", "1")
    dev[0].connect("bgscan", ieee80211w="2", key_mgmt="WPA-PSK-SHA256",
                   psk="12345678", scan_freq="2412",
                   bgscan="simple:100:-20:200")
    hapd.set("ext_mgmt_frame_handling", "1")
    if "OK" not in hapd.request("STOP_AP"):
        raise Exception("Failed to stop AP")
    hapd.disable()
    hapd.set("ssid", "foo")
    hapd.set("beacon_int", "10000")
    hapd.enable()
    ev = dev[0].wait_event(["CTRL-EVENT-BEACON-LOSS"], timeout=10)
    if ev is None:
        raise Exception("Beacon loss not reported")

def test_bgscan_simple_scan_failure(dev, apdev):
    """bgscan_simple and scan failure"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": "bgscan"})

    dev[0].connect("bgscan", key_mgmt="NONE", scan_freq="2412",
                   bgscan="simple:1:-20:2")
    with alloc_fail(dev[0], 1,
                    "wpa_supplicant_trigger_scan;bgscan_simple_timeout"):
        ev = dev[0].wait_event(["CTRL-EVENT-SCAN-FAILED"], timeout=10)
        if ev is None:
            raise Exception("No scan failure reported")
    ev = dev[0].wait_event(["CTRL-EVENT-SCAN-RESULTS"], 10)
    if ev is None:
        raise Exception("Scanning not continued after failure")

def test_bgscan_simple_scanning(dev, apdev):
    """bgscan_simple and scanning behavior"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": "bgscan"})

    dev[0].connect("bgscan", key_mgmt="NONE", scan_freq="2412",
                   bgscan="simple:1:-20:2")
    # Go through seven bgscan_simple_timeout calls for code coverage. This falls
    # back from short to long scan interval and then reduces short_scan_count
    # back to zero.
    for i in range(7):
        ev = dev[0].wait_event(["CTRL-EVENT-SCAN-RESULTS"], 10)
        if ev is None:
            raise Exception("Scanning not continued")

def test_bgscan_simple_same_scan_int(dev, apdev):
    """bgscan_simple and same short/long scan interval"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": "bgscan"})

    dev[0].connect("bgscan", key_mgmt="NONE", scan_freq="2412",
                   bgscan="simple:1:-20:1")
    for i in range(2):
        ev = dev[0].wait_event(["CTRL-EVENT-SCAN-RESULTS"], 10)
        if ev is None:
            raise Exception("Scanning not continued")

def test_bgscan_simple_oom(dev, apdev):
    """bgscan_simple OOM"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": "bgscan"})

    with alloc_fail(dev[0], 1, "bgscan_simple_init"):
        dev[0].connect("bgscan", key_mgmt="NONE", scan_freq="2412",
                       bgscan="simple:1:-20:2")

def test_bgscan_simple_driver_conf_failure(dev, apdev):
    """bgscan_simple driver configuration failure"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": "bgscan"})

    with fail_test(dev[0], 1, "bgscan_simple_init"):
        dev[0].connect("bgscan", key_mgmt="NONE", scan_freq="2412",
                       bgscan="simple:1:-20:2")

def test_bgscan_learn(dev, apdev):
    """bgscan_learn"""
    hostapd.add_ap(apdev[0], {"ssid": "bgscan"})
    hostapd.add_ap(apdev[1], {"ssid": "bgscan"})

    try:
        os.remove("/tmp/test_bgscan_learn.bgscan")
    except:
        pass

    try:
        dev[0].connect("bgscan", key_mgmt="NONE", scan_freq="2412",
                       bgscan="learn:1:-20:2")
        id = dev[1].connect("bgscan", key_mgmt="NONE", scan_freq="2412",
                            bgscan="learn:1:-45:2:/tmp/test_bgscan_learn.bgscan")

        dev[2].connect("bgscan", key_mgmt="NONE", scan_freq="2412",
                       bgscan="learn:1:-45")
        dev[2].request("REMOVE_NETWORK all")
        dev[2].wait_disconnected()

        dev[2].connect("bgscan", key_mgmt="NONE", scan_freq="2412",
                       bgscan="learn:0:0")
        dev[2].request("REMOVE_NETWORK all")
        dev[2].wait_disconnected()

        dev[2].connect("bgscan", key_mgmt="NONE", scan_freq="2412",
                       bgscan="learn")
        dev[2].request("REMOVE_NETWORK all")
        dev[2].wait_disconnected()

        dev[2].connect("bgscan", key_mgmt="NONE", scan_freq="2412",
                       bgscan="learn:1")
        dev[2].request("REMOVE_NETWORK all")
        dev[2].wait_disconnected()

        ev = dev[0].wait_event(["CTRL-EVENT-SIGNAL-CHANGE"], timeout=10)
        if ev is None:
            raise Exception("dev0 did not indicate signal change event")
        if "above=0" not in ev:
            raise Exception("Unexpected signal change event contents from dev0: " + ev)

        ev = dev[1].wait_event(["CTRL-EVENT-SIGNAL-CHANGE"], timeout=10)
        if ev is None:
            raise Exception("dev1 did not indicate signal change event")
        if "above=1" not in ev:
            raise Exception("Unexpected signal change event contents from dev1: " + ev)

        ev = dev[0].wait_event(["CTRL-EVENT-SCAN-STARTED"], timeout=3)
        if ev is None:
            raise Exception("dev0 did not start a scan")

        ev = dev[1].wait_event(["CTRL-EVENT-SCAN-STARTED"], timeout=3)
        if ev is None:
            raise Exception("dev1 did not start a scan")

        ev = dev[0].wait_event(["CTRL-EVENT-SCAN-RESULTS"], 5)
        if ev is None:
            raise Exception("dev0 did not complete a scan")
        ev = dev[1].wait_event(["CTRL-EVENT-SCAN-RESULTS"], 5)
        if ev is None:
            raise Exception("dev1 did not complete a scan")

        dev[0].request("DISCONNECT")
        dev[1].request("DISCONNECT")
        dev[0].request("REMOVE_NETWORK all")

        with open("/tmp/test_bgscan_learn.bgscan", "r") as f:
            lines = f.read().splitlines()
        if lines[0] != "wpa_supplicant-bgscan-learn":
            raise Exception("Unexpected bgscan header line")
        if 'BSS 02:00:00:00:03:00 2412' not in lines:
            raise Exception("Missing BSS1")
        if 'BSS 02:00:00:00:04:00 2412' not in lines:
            raise Exception("Missing BSS2")
        if 'NEIGHBOR 02:00:00:00:03:00 02:00:00:00:04:00' not in lines:
            raise Exception("Missing BSS1->BSS2 neighbor entry")
        if 'NEIGHBOR 02:00:00:00:04:00 02:00:00:00:03:00' not in lines:
            raise Exception("Missing BSS2->BSS1 neighbor entry")

        dev[1].set_network(id, "scan_freq", "")
        dev[1].connect_network(id)

        ev = dev[1].wait_event(["CTRL-EVENT-SCAN-STARTED"], timeout=10)
        if ev is None:
            raise Exception("dev1 did not start a scan")

        ev = dev[1].wait_event(["CTRL-EVENT-SCAN-RESULTS"], 10)
        if ev is None:
            raise Exception("dev1 did not complete a scan")

        dev[1].request("REMOVE_NETWORK all")
    finally:
        try:
            os.remove("/tmp/test_bgscan_learn.bgscan")
        except:
            pass

def test_bgscan_learn_beacon_loss(dev, apdev):
    """bgscan_simple and beacon loss"""
    params = hostapd.wpa2_params(ssid="bgscan", passphrase="12345678")
    params["wpa_key_mgmt"] = "WPA-PSK-SHA256"
    params["ieee80211w"] = "2"
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].set("disable_sa_query", "1")
    dev[0].connect("bgscan", ieee80211w="2", key_mgmt="WPA-PSK-SHA256",
                   psk="12345678", scan_freq="2412", bgscan="learn:100:-20:200")
    hapd.set("ext_mgmt_frame_handling", "1")
    if "OK" not in hapd.request("STOP_AP"):
        raise Exception("Failed to stop AP")
    hapd.disable()
    hapd.set("ssid", "foo")
    hapd.set("beacon_int", "10000")
    hapd.enable()
    ev = dev[0].wait_event(["CTRL-EVENT-BEACON-LOSS"], timeout=10)
    if ev is None:
        raise Exception("Beacon loss not reported")

def test_bgscan_learn_scan_failure(dev, apdev):
    """bgscan_learn and scan failure"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": "bgscan"})

    dev[0].connect("bgscan", key_mgmt="NONE", scan_freq="2412",
                   bgscan="learn:1:-20:2")
    with alloc_fail(dev[0], 1,
                    "wpa_supplicant_trigger_scan;bgscan_learn_timeout"):
        ev = dev[0].wait_event(["CTRL-EVENT-SCAN-FAILED"], timeout=10)
        if ev is None:
            raise Exception("No scan failure reported")
    ev = dev[0].wait_event(["CTRL-EVENT-SCAN-RESULTS"], 10)
    if ev is None:
        raise Exception("Scanning not continued after failure")

def test_bgscan_learn_oom(dev, apdev):
    """bgscan_learn OOM"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": "bgscan"})

    with alloc_fail(dev[0], 1, "bgscan_learn_init"):
        dev[0].connect("bgscan", key_mgmt="NONE", scan_freq="2412",
                       bgscan="learn:1:-20:2")

def test_bgscan_learn_driver_conf_failure(dev, apdev):
    """bgscan_learn driver configuration failure"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": "bgscan"})

    with fail_test(dev[0], 1, "bgscan_learn_init"):
        dev[0].connect("bgscan", key_mgmt="NONE", scan_freq="2412",
                       bgscan="learn:1:-20:2")

def test_bgscan_unknown_module(dev, apdev):
    """bgscan init failing due to unknown module"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": "bgscan"})
    dev[0].connect("bgscan", key_mgmt="NONE", scan_freq="2412",
                   bgscan="unknown:-20:2")

def test_bgscan_reconfig(dev, apdev):
    """bgscan parameter update"""
    hostapd.add_ap(apdev[0], {"ssid": "bgscan"})
    hostapd.add_ap(apdev[1], {"ssid": "bgscan"})

    id = dev[0].connect("bgscan", key_mgmt="NONE", scan_freq="2412",
                        bgscan="simple:1:-20:2")
    dev[0].set_network_quoted(id, "bgscan", "simple:1:-45:2")
    dev[0].set_network_quoted(id, "bgscan", "learn:1:-20:2")
    dev[0].set_network_quoted(id, "bgscan", "")
