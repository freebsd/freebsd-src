# Radio work tests
# Copyright (c) 2014, Jouni Malinen <j@w1.fi>
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

import time
import logging
logger = logging.getLogger()

import hostapd
from wpasupplicant import WpaSupplicant

def test_ext_radio_work(dev, apdev):
    """External radio work item"""
    id = dev[0].request("RADIO_WORK add test-work-a")
    if "FAIL" in id:
        raise Exception("Failed to add radio work")
    id2 = dev[0].request("RADIO_WORK add test-work-b freq=2417")
    if "FAIL" in id2:
        raise Exception("Failed to add radio work")
    id3 = dev[0].request("RADIO_WORK add test-work-c")
    if "FAIL" in id3:
        raise Exception("Failed to add radio work")

    ev = dev[0].wait_event(["EXT-RADIO-WORK-START"])
    if ev is None:
        raise Exception("Timeout while waiting radio work to start")
    if "EXT-RADIO-WORK-START " + id not in ev:
        raise Exception("Unexpected radio work start id")

    items = dev[0].request("RADIO_WORK show")
    if "ext:test-work-a@wlan0:0:1:" not in items:
        logger.info("Pending radio work items:\n" + items)
        raise Exception("Radio work item(a) missing from the list")
    if "ext:test-work-b@wlan0:2417:0:" not in items:
        logger.info("Pending radio work items:\n" + items)
        raise Exception("Radio work item(b) missing from the list")
    if "ext:test-work-c@wlan0:0:0:" not in items:
        logger.info("Pending radio work items:\n" + items)
        raise Exception("Radio work item(c) missing from the list")

    dev[0].request("RADIO_WORK done " + id2)
    dev[0].request("RADIO_WORK done " + id)

    ev = dev[0].wait_event(["EXT-RADIO-WORK-START"])
    if ev is None:
        raise Exception("Timeout while waiting radio work to start")
    if "EXT-RADIO-WORK-START " + id3 not in ev:
        raise Exception("Unexpected radio work start id")
    dev[0].request("RADIO_WORK done " + id3)
    items = dev[0].request("RADIO_WORK show")
    if "ext:" in items:
        logger.info("Pending radio work items:\n" + items)
        raise Exception("Unexpected remaining radio work item")

    id = dev[0].request("RADIO_WORK add test-work timeout=1")
    ev = dev[0].wait_event(["EXT-RADIO-WORK-START"])
    if ev is None:
        raise Exception("Timeout while waiting radio work to start")
    ev = dev[0].wait_event(["EXT-RADIO-WORK-TIMEOUT"], timeout=2)
    if ev is None:
        raise Exception("Timeout while waiting radio work to time out")
    if id not in ev:
        raise Exception("Radio work id mismatch")

    for i in range(5):
        dev[0].request(("RADIO_WORK add test-work-%d-" % i) + 100*'a')
    ev = dev[0].wait_event(["EXT-RADIO-WORK-START"])
    if ev is None:
        raise Exception("Timeout while waiting radio work to start")
    if "FAIL" not in dev[0].request("RADIO_WORK done 12345678"):
        raise Exception("Invalid RADIO_WORK done accepted")
    if "FAIL" not in dev[0].request("RADIO_WORK foo"):
        raise Exception("Invalid RADIO_WORK accepted")
    dev[0].request("FLUSH")
    items = dev[0].request("RADIO_WORK show")
    if items != "":
        raise Exception("Unexpected radio work remaining after FLUSH: " + items)

def test_radio_work_cancel(dev, apdev):
    """Radio work items cancelled on interface removal"""
    params = hostapd.wpa2_params(ssid="radio", passphrase="12345678")
    hostapd.add_ap(apdev[0], params)
    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    wpas.interface_add("wlan5")
    wpas.scan(freq="2412")

    id = wpas.request("RADIO_WORK add test-work-a")
    if "FAIL" in id:
        raise Exception("Failed to add radio work")
    ev = wpas.wait_event(["EXT-RADIO-WORK-START"])
    if ev is None:
        raise Exception("Timeout while waiting radio work to start")
    if "EXT-RADIO-WORK-START " + id not in ev:
        raise Exception("Unexpected radio work start id")

    wpas.connect("radio", psk="12345678", scan_freq="2412",
                   wait_connect=False)
    time.sleep(1)
    wpas.interface_remove("wlan5")
    # add to allow log file renaming
    wpas.interface_add("wlan5")

def test_ext_radio_work_disconnect_connect(dev, apdev):
    """External radio work and DISCONNECT clearing connection attempt"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": "open"})
    dev[0].scan_for_bss(hapd.own_addr(), freq=2412)

    # Start a radio work to block connection attempt
    id1 = dev[0].request("RADIO_WORK add test-work-a")
    if "FAIL" in id1:
        raise Exception("Failed to add radio work")
    ev = dev[0].wait_event(["EXT-RADIO-WORK-START"])
    if ev is None:
        raise Exception("Timeout while waiting radio work to start")

    dev[0].connect("open", key_mgmt="NONE", scan_freq="2412",
                   wait_connect=False)
    items = dev[0].request("RADIO_WORK show")
    if "connect" not in items:
        raise Exception("Connection radio work not scheduled")
    dev[0].request("DISCONNECT")
    items = dev[0].request("RADIO_WORK show")
    if "connect" in items:
        raise Exception("Connection radio work not removed on DISCONNECT")

    # Clear radio work to allow any pending work to be started
    dev[0].request("RADIO_WORK done " + id1)

    ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED"], timeout=0.5)
    if ev is not None:
        raise Exception("Unexpected connection seen")
