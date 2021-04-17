# Test cases for MSCS
# Copyright (c) 2021, Jouni Malinen <j@w1.fi>
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

import struct
import time

import hostapd
from utils import *

def register_mcsc_req(hapd):
    type = 0x00d0
    match = "1304"
    if "OK" not in hapd.request("REGISTER_FRAME %04x %s" % (type, match)):
        raise Exception("Could not register frame reception for Robust AV Streaming")

def handle_mscs_req(hapd, wrong_dialog=False, status_code=0):
    msg = hapd.mgmt_rx()
    if msg['subtype'] != 13:
        logger.info("RX:" + str(msg))
        raise Exception("Received unexpected Management frame")
    categ, act, dialog_token = struct.unpack('BBB', msg['payload'][0:3])
    if categ != 19 or act != 4:
        logger.info("RX:" + str(msg))
        raise Exception("Received unexpected Action frame")

    if wrong_dialog:
        dialog_token = (dialog_token + 1) % 256
    msg['da'] = msg['sa']
    msg['sa'] = hapd.own_addr()
    msg['payload'] = struct.pack('<BBBH', 19, 5, dialog_token, status_code)
    hapd.mgmt_tx(msg)
    ev = hapd.wait_event(["MGMT-TX-STATUS"], timeout=5)
    if ev is None or "stype=13 ok=1" not in ev:
        raise Exception("No TX status reported")

def wait_mscs_result(dev, expect_status=0):
    ev = dev.wait_event(["CTRL-EVENT-MSCS-RESULT"], timeout=1)
    if ev is None:
        raise Exception("No MSCS result reported")
    if "status_code=%d" % expect_status not in ev:
        raise Exception("Unexpected MSCS result: " + ev)

def test_mscs_invalid_params(dev, apdev):
    """MSCS command invalid parameters"""
    tests = ["",
             "add Xp_bitmap=F0 up_limit=7 stream_timeout=12345 frame_classifier=045F",
             "add up_bitmap=F0 Xp_limit=7 stream_timeout=12345 frame_classifier=045F",
             "add up_bitmap=F0 up_limit=7 Xtream_timeout=12345 frame_classifier=045F",
             "add up_bitmap=F0 up_limit=7 stream_timeout=12345 Xrame_classifier=045F",
             "add up_bitmap=X0 up_limit=7 stream_timeout=12345 frame_classifier=045F",
             "add up_bitmap=F0 up_limit=7 stream_timeout=0 frame_classifier=045F",
             "add up_bitmap=F0 up_limit=7 stream_timeout=12345 frame_classifier=X45F",
             "change "]
    for t in tests:
        if "FAIL" not in dev[0].request("MSCS " + t):
            raise Exception("Invalid MSCS parameters accepted: " + t)

def test_mscs_without_ap_support(dev, apdev):
    """MSCS without AP support"""
    try:
        run_mscs_without_ap_support(dev, apdev)
    finally:
        dev[0].request("MSCS remove")

def run_mscs_without_ap_support(dev, apdev):
    params = {"ssid": "mscs",
              "ext_capa_mask": 10*"00" + "20"}
    hapd = hostapd.add_ap(apdev[0], params)

    cmd = "MSCS add up_bitmap=F0 up_limit=7 stream_timeout=12345 frame_classifier=045F"
    if "OK" not in dev[0].request(cmd):
        raise Exception("Failed to configure MSCS")

    dev[0].connect("mscs", key_mgmt="NONE", scan_freq="2412")

    cmd = "MSCS change up_bitmap=F0 up_limit=7 stream_timeout=12345 frame_classifier=045F"
    if "FAIL" not in dev[0].request(cmd):
        raise Exception("MSCS change accepted unexpectedly")

    cmd = "MSCS add up_bitmap=F0 up_limit=7 stream_timeout=12345 frame_classifier=045F"
    if "FAIL" not in dev[0].request(cmd):
        raise Exception("MSCS add accepted unexpectedly")

def test_mscs_post_assoc(dev, apdev):
    """MSCS configuration post-association"""
    try:
        run_mscs_post_assoc(dev, apdev)
    finally:
        dev[0].request("MSCS remove")

def run_mscs_post_assoc(dev, apdev):
    params = {"ssid": "mscs",
              "ext_capa": 10*"00" + "20"}
    hapd = hostapd.add_ap(apdev[0], params)
    register_mcsc_req(hapd)

    dev[0].connect("mscs", key_mgmt="NONE", scan_freq="2412")

    hapd.dump_monitor()
    hapd.set("ext_mgmt_frame_handling", "1")

    cmd = "MSCS change up_bitmap=F0 up_limit=7 stream_timeout=12345 frame_classifier=045F"
    if "FAIL" not in dev[0].request(cmd):
        raise Exception("MSCS change accepted unexpectedly")

    cmd = "MSCS add up_bitmap=F0 up_limit=7 stream_timeout=12345 frame_classifier=045F"
    if "OK" not in dev[0].request(cmd):
        raise Exception("MSCS add failed")

    handle_mscs_req(hapd)
    wait_mscs_result(dev[0])

    cmd = "MSCS change up_bitmap=F0 up_limit=7 stream_timeout=12345 frame_classifier=045F"
    if "OK" not in dev[0].request(cmd):
        raise Exception("MSCS change failed")

    handle_mscs_req(hapd)
    wait_mscs_result(dev[0])

    cmd = "MSCS change up_bitmap=F0 up_limit=7 stream_timeout=12345 frame_classifier=045F"
    if "OK" not in dev[0].request(cmd):
        raise Exception("MSCS change failed")

    handle_mscs_req(hapd, status_code=23456)
    wait_mscs_result(dev[0], expect_status=23456)

def test_mscs_pre_assoc(dev, apdev):
    """MSCS configuration pre-association"""
    try:
        run_mscs_pre_assoc(dev, apdev)
    finally:
        dev[0].request("MSCS remove")

def run_mscs_pre_assoc(dev, apdev):
    params = {"ssid": "mscs",
              "ext_capa": 10*"00" + "20",
              "assocresp_elements": "ff0c5800000000000000" + "01020000"}
    hapd = hostapd.add_ap(apdev[0], params)
    register_mcsc_req(hapd)

    cmd = "MSCS add up_bitmap=F0 up_limit=7 stream_timeout=12345 frame_classifier=045F"
    if "OK" not in dev[0].request(cmd):
        raise Exception("MSCS add failed")

    dev[0].connect("mscs", key_mgmt="NONE", scan_freq="2412",
                   wait_connect=False)
    wait_mscs_result(dev[0])
    dev[0].wait_connected()

    hapd.set("ext_mgmt_frame_handling", "1")

    cmd = "MSCS change up_bitmap=F0 up_limit=7 stream_timeout=12345 frame_classifier=045F"
    if "OK" not in dev[0].request(cmd):
        raise Exception("MSCS change failed")

    handle_mscs_req(hapd)
    wait_mscs_result(dev[0])

    cmd = "MSCS change up_bitmap=F0 up_limit=7 stream_timeout=12345 frame_classifier=045F"
    if "OK" not in dev[0].request(cmd):
        raise Exception("MSCS change failed")

    handle_mscs_req(hapd, wrong_dialog=True)

    ev = dev[0].wait_event(["CTRL-EVENT-MSCS-RESULT"], timeout=1)
    if ev is not None:
        raise Exception("Unexpected MSCS result reported")

def test_mscs_assoc_failure(dev, apdev):
    """MSCS configuration failure during association exchange"""
    try:
        run_mscs_assoc_failure(dev, apdev)
    finally:
        dev[0].request("MSCS remove")

def run_mscs_assoc_failure(dev, apdev):
    params = {"ssid": "mscs",
              "ext_capa": 10*"00" + "20",
              "assocresp_elements": "ff0c5800000000000000" + "01020001"}
    hapd = hostapd.add_ap(apdev[0], params)
    register_mcsc_req(hapd)

    cmd = "MSCS add up_bitmap=F0 up_limit=7 stream_timeout=12345 frame_classifier=045F"
    if "OK" not in dev[0].request(cmd):
        raise Exception("MSCS add failed")

    dev[0].connect("mscs", key_mgmt="NONE", scan_freq="2412",
                   wait_connect=False)
    wait_mscs_result(dev[0], expect_status=256)
    dev[0].wait_connected()
    dev[0].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected()

    hapd.dump_monitor()
    # No MSCS Status subelement
    hapd.set("assocresp_elements", "ff085800000000000000")
    dev[0].connect("mscs", key_mgmt="NONE", scan_freq="2412",
                   wait_connect=False)
    ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED", "CTRL-EVENT-MSCS-RESULT"],
                           timeout=10)
    if ev is None:
        raise Exception("No connection event")
    if "CTRL-EVENT-MSCS-RESULT" in ev:
        raise Exception("Unexpected MSCS result")

def test_mscs_local_errors(dev, apdev):
    """MSCS configuration local errors"""
    try:
        run_mscs_local_errors(dev, apdev)
    finally:
        dev[0].request("MSCS remove")

def run_mscs_local_errors(dev, apdev):
    params = {"ssid": "mscs",
              "ext_capa": 10*"00" + "20"}
    hapd = hostapd.add_ap(apdev[0], params)
    register_mcsc_req(hapd)

    dev[0].connect("mscs", key_mgmt="NONE", scan_freq="2412")

    hapd.dump_monitor()
    hapd.set("ext_mgmt_frame_handling", "1")

    for count in range(1, 3):
        with alloc_fail(dev[0], count, "wpas_send_mscs_req"):
            cmd = "MSCS add up_bitmap=F0 up_limit=7 stream_timeout=12345 frame_classifier=045F"
            if "FAIL" not in dev[0].request(cmd):
                raise Exception("MSCS add succeeded in error case")
