# Test cases for SCS
# Copyright (c) 2021, Jouni Malinen <j@w1.fi>
# Copyright (c) 2021, The Linux Foundation
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

import struct
import time

import hostapd
from utils import *

def register_scs_req(hapd):
    type = 0x00d0
    match = "1300"
    if "OK" not in hapd.request("REGISTER_FRAME %04x %s" % (type, match)):
        raise Exception("Could not register frame reception for Robust AV Streaming")

def handle_scs_req(hapd, wrong_dialog=False, status_code=0, twice=False,
                   short=False, scsid=1):
    msg = hapd.mgmt_rx()
    if msg['subtype'] != 13:
        logger.info("RX:" + str(msg))
        raise Exception("Received unexpected Management frame")
    categ, act, dialog_token = struct.unpack('BBB', msg['payload'][0:3])
    if categ != 19 or act != 0:
        logger.info("RX:" + str(msg))
        raise Exception("Received unexpected Action frame")

    if wrong_dialog:
        dialog_token = (dialog_token + 1) % 256
    msg['da'] = msg['sa']
    msg['sa'] = hapd.own_addr()
    count = 1
    if short:
        resp = struct.pack('BBB', 19, 1, dialog_token)
    else:
        resp = struct.pack('BBBB', 19, 1, dialog_token, count)
        resp += struct.pack('<BH', scsid, status_code)
    msg['payload'] = resp
    hapd.mgmt_tx(msg)
    ev = hapd.wait_event(["MGMT-TX-STATUS"], timeout=5)
    if ev is None or "stype=13 ok=1" not in ev:
        raise Exception("No TX status reported")
    if twice:
        hapd.mgmt_tx(msg)
        ev = hapd.wait_event(["MGMT-TX-STATUS"], timeout=5)
        if ev is None or "stype=13 ok=1" not in ev:
            raise Exception("No TX status reported")

def wait_scs_result(dev, expect_status="0"):
    ev = dev.wait_event(["CTRL-EVENT-SCS-RESULT"], timeout=2)
    if ev is None:
        raise Exception("No SCS result reported")
    if "status_code=%s" % expect_status not in ev:
        raise Exception("Unexpected SCS result: " + ev)

def test_scs_invalid_params(dev, apdev):
    """SCS command invalid parameters"""
    tests = ["",
             "scs_id=1",
             "scs_id=1 foo",
             "scs_id=1 add ",
             "scs_id=1 add scs_up=8",
             "scs_id=1 add scs_up=7",
             "scs_id=1 add scs_up=7 classifier_type=1",
             "scs_id=1 add scs_up=7 classifier_type=4",
             "scs_id=1 add scs_up=7 classifier_type=4 ip_version=ipv4",
             "scs_id=1 add scs_up=7 classifier_type=4 ip_version=ipv4 src_ip=q",
             "scs_id=1 add scs_up=7 classifier_type=4 ip_version=ipv4 dst_ip=q",
             "scs_id=1 add scs_up=7 classifier_type=4 ip_version=ipv4 src_port=q",
             "scs_id=1 add scs_up=7 classifier_type=4 ip_version=ipv4 dst_port=q",
             "scs_id=1 add scs_up=7 classifier_type=4 ip_version=ipv4 protocol=foo",
             "scs_id=1 add scs_up=7 classifier_type=4 ip_version=ipv6 protocol=foo",
             "scs_id=1 add scs_up=7 classifier_type=4 ip_version=ipv6 next_header=foo",
             "scs_id=1 add scs_up=7 classifier_type=4 ip_version=ipv6 flow_label=ffffff",
             "scs_id=1 add scs_up=7 classifier_type=10",
             "scs_id=1 add scs_up=7 classifier_type=10 prot_instance=1",
             "scs_id=1 add scs_up=7 classifier_type=10 prot_instance=1 prot_number=udp",
             "scs_id=1 add scs_up=7 classifier_type=10 prot_instance=1 prot_number=udp filter_value=11223344",
             "scs_id=1 add scs_up=7 classifier_type=10 prot_instance=1 prot_number=udp filter_value=qq",
             "scs_id=1 add scs_up=7 classifier_type=10 prot_instance=1 prot_number=udp filter_value=11223344 filter_mask=ffffff",
             "scs_id=1 add scs_up=7 classifier_type=10 prot_instance=1 prot_number=udp filter_value=11223344 filter_mask=qqqqqqqq",
             "scs_id=1 add scs_up=7 classifier_type=10 prot_instance=1 prot_number=udp filter_value=11223344 filter_mask=ffffffff",
             "scs_id=1 add scs_up=7 classifier_type=10 prot_instance=1 prot_number=foo filter_value=11223344 filter_mask=ffffffff",
             "scs_id=1 add scs_up=7 classifier_type=10 prot_instance=1 prot_number=udp filter_value=11 filter_mask=ee classifier_type=10 prot_instance=2 prot_number=udp filter_value=22 filter_mask=ff",
             "scs_id=1 add scs_up=7 classifier_type=10 prot_instance=1 prot_number=udp filter_value=11 filter_mask=ee classifier_type=10 prot_instance=2 prot_number=udp filter_value=22 filter_mask=ff tclas_processing=2",
             "scs_id=1 add scs_up=7 classifier_type=10 prot_instance=1 prot_number=udp filter_value=11 filter_mask=ee classifier_type=10 prot_instance=2 prot_number=udp filter_value=22 filter_mask=ff tclas_processing=0",
             "scs_id=1 add scs_up=6 classifier_type=4 ip_version=ipv4 src_ip=1.2.3.4 dst_ip=5.6.7.8 src_port=12345 dst_port=23456 dscp=5 protocol=udp scs_id=1 add scs_up=6 classifier_type=4 ip_version=ipv4 src_ip=1.2.3.4 dst_ip=5.6.7.8 src_port=12345 dst_port=23456 dscp=5 protocol=tcp"
             "scs_id=1 remove",
             "scs_id=1 change "]
    for t in tests:
        if "FAIL" not in dev[0].request("SCS " + t):
            raise Exception("Invalid SCS parameters accepted: " + t)

def test_scs_request(dev, apdev):
    """SCS Request"""
    params = {"ssid": "scs",
              "ext_capa": 6*"00" + "40"}
    hapd = hostapd.add_ap(apdev[0], params)
    register_scs_req(hapd)

    dev[0].connect("scs", key_mgmt="NONE", scan_freq="2412")

    hapd.dump_monitor()
    hapd.set("ext_mgmt_frame_handling", "1")

    cmd = "SCS scs_id=1 add scs_up=7 classifier_type=10 prot_instance=1 prot_number=udp filter_value=11223344 filter_mask=ffffffff"
    if "OK" not in dev[0].request(cmd):
        raise Exception("SCS add failed")

    handle_scs_req(hapd)
    wait_scs_result(dev[0])

    cmd = "SCS scs_id=2 add scs_up=5 classifier_type=10 prot_instance=1 prot_number=udp filter_value=11223344 filter_mask=ffffffff"
    if "OK" not in dev[0].request(cmd):
        raise Exception("SCS add failed")

    handle_scs_req(hapd, wrong_dialog=True)
    ev = dev[0].wait_event(["CTRL-EVENT-SCS-RESULT"], timeout=2)
    if ev is None:
        raise Exception("No SCS result reported")
    if "status_code=timedout" not in ev:
        raise Exception("Timeout not reported: " + ev)

    cmd = "SCS scs_id=1 add scs_up=5 classifier_type=10 prot_instance=1 prot_number=udp filter_value=11223344 filter_mask=ffffffff"
    if "FAIL" not in dev[0].request(cmd):
        raise Exception("SCS add for already configured scs_id did not fail")

    cmd = "SCS scs_id=1 remove"
    if "OK" not in dev[0].request(cmd):
        raise Exception("SCS remove failed")
    handle_scs_req(hapd)
    wait_scs_result(dev[0])

    tests = ["scs_up=6 classifier_type=4 ip_version=ipv4 src_ip=1.2.3.4 dst_ip=5.6.7.8 src_port=12345 dst_port=23456 dscp=5 protocol=udp",
             "scs_up=6 classifier_type=4 ip_version=ipv4 src_ip=1.2.3.4 dst_ip=5.6.7.8 src_port=12345 dst_port=23456 dscp=5 protocol=tcp",
             "scs_up=6 classifier_type=4 ip_version=ipv4 src_ip=1.2.3.4 dst_ip=5.6.7.8 src_port=12345 dst_port=23456 dscp=5 protocol=esp",
             "scs_up=6 classifier_type=4 ip_version=ipv6 src_ip=::1 dst_ip=::1 src_port=12345 dst_port=23456 dscp=5 next_header=udp",
             "scs_up=6 classifier_type=4 ip_version=ipv6 src_ip=::1 dst_ip=::1 src_port=12345 dst_port=23456 dscp=5 next_header=tcp",
             "scs_up=6 classifier_type=4 ip_version=ipv6 src_ip=::1 dst_ip=::1 src_port=12345 dst_port=23456 dscp=5 next_header=esp flow_label=012345",
             "scs_up=6 classifier_type=10 prot_instance=1 prot_number=tcp filter_value=11223344 filter_mask=ffffffff",
             "scs_up=6 classifier_type=10 prot_instance=1 prot_number=esp filter_value=11223344 filter_mask=ffffffff",
             "scs_up=6 classifier_type=10 prot_instance=1 prot_number=udp filter_value=11223344 filter_mask=ffffffff",
             "scs_up=6 classifier_type=4 ip_version=ipv4 src_ip=1.2.3.4 dst_ip=5.6.7.8 src_port=12345 dst_port=23456 dscp=5 protocol=udp classifier_type=4 ip_version=ipv4 src_ip=1.2.3.4 dst_ip=5.6.7.8 src_port=12345 dst_port=23456 dscp=5 protocol=tcp tclas_processing=1",
             "scs_up=6 classifier_type=4 ip_version=ipv4 src_ip=1.2.3.4 dst_ip=5.6.7.8 src_port=12345 dst_port=23456 dscp=5 protocol=udp scs_id=10 add scs_up=6 classifier_type=4 ip_version=ipv4 src_ip=1.2.3.4 dst_ip=5.6.7.8 src_port=12345 dst_port=23456 dscp=5 protocol=tcp"]
    for t in tests:
        cmd = "SCS scs_id=1 change " + t
        if "OK" not in dev[0].request(cmd):
            raise Exception("SCS change failed: " + t)
        handle_scs_req(hapd)
        wait_scs_result(dev[0])
        if "scs_id=" in t:
            wait_scs_result(dev[0], expect_status="response_not_received")

    cmd = "SCS scs_id=1 change scs_up=6 classifier_type=4 ip_version=ipv4 src_ip=1.2.3.4 dst_ip=5.6.7.8 src_port=12345 dst_port=23456 dscp=5 protocol=udp"
    if "OK" not in dev[0].request(cmd):
        raise Exception("SCS change failed: " + t)
    if "FAIL" not in dev[0].request(cmd):
        raise Exception("SCS change failed: " + t)
    handle_scs_req(hapd, twice=True)
    wait_scs_result(dev[0])
    ev = dev[0].wait_event(["CTRL-EVENT-SCS-RESULT"], timeout=0.1)
    if ev is not None:
        raise Exception("Unexpected SCS result reported(1)")

    if "OK" not in dev[0].request(cmd):
        raise Exception("SCS change failed: " + t)
    handle_scs_req(hapd, short=True)
    ev = dev[0].wait_event(["CTRL-EVENT-SCS-RESULT"], timeout=3)
    if ev is not None:
        raise Exception("Unexpected SCS result reported(2)")

    cmd = "SCS scs_id=123 add scs_up=6 classifier_type=4 ip_version=ipv4 src_ip=1.2.3.4 dst_ip=5.6.7.8 src_port=12345 dst_port=23456 dscp=5 protocol=udp"
    if "OK" not in dev[0].request(cmd):
        raise Exception("SCS add failed: " + t)
    handle_scs_req(hapd, scsid=34)
    wait_scs_result(dev[0], expect_status="response_not_received")

    cmd = "SCS scs_id=33 add scs_up=6 classifier_type=4 ip_version=ipv4 src_ip=1.2.3.4 dst_ip=5.6.7.8 src_port=12345 dst_port=23456 dscp=5 protocol=udp"
    if "OK" not in dev[0].request(cmd):
        raise Exception("SCS add failed: " + t)
    handle_scs_req(hapd, scsid=33, status_code=123)
    wait_scs_result(dev[0], expect_status="123")

def test_scs_request_without_ap_capa(dev, apdev):
    """SCS Request without AP capability"""
    params = {"ssid": "scs"}
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].connect("scs", key_mgmt="NONE", scan_freq="2412")

    cmd = "SCS scs_id=1 add scs_up=7 classifier_type=10 prot_instance=1 prot_number=udp filter_value=11223344 filter_mask=ffffffff"
    if "FAIL" not in dev[0].request(cmd):
        raise Exception("SCS add accepted")
