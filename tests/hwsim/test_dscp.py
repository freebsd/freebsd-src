# Test cases for dscp policy
# Copyright (c) 2021, Jouni Malinen <j@w1.fi>
# Copyright (c) 2021, The Linux Foundation
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

import struct
import time
import sys
import socket

import hostapd
from wpasupplicant import WpaSupplicant
from utils import *

def register_dscp_req(hapd):
    type = 0x00d0
    match = "7e506f9a1a"
    if "OK" not in hapd.request("REGISTER_FRAME %04x %s" % (type, match)):
        raise Exception("Could not register frame reception for Vendor specific protected type")

def send_dscp_req(hapd, da, oui_subtype, dialog_token, req_control, qos_ie,
                  truncate=False):
    type = 0
    subtype = 13
    category = 126
    oui_type = 0x506f9a1a
    if truncate:
        req = struct.pack('>BLBB', category, oui_type, oui_subtype,
                          dialog_token)
    else:
        req = struct.pack('>BLBBB', category, oui_type, oui_subtype,
                          dialog_token, req_control)
        if qos_ie:
            req += qos_ie

    msg = {}
    msg['fc'] = 0x00d0
    msg['sa'] = hapd.own_addr()
    msg['da'] = da
    msg['bssid'] = hapd.own_addr()
    msg['type'] = type
    msg['subtype'] = subtype
    msg['payload'] = req

    hapd.mgmt_tx(msg)
    ev = hapd.wait_event(["MGMT-TX-STATUS"], timeout=5)
    if ev is None or "stype=13 ok=1" not in ev:
        raise Exception("No DSCP Policy Request sent")

def prepare_qos_ie(policy_id, req_type, dscp, start_port=0, end_port=0,
                   frame_classifier=None, frame_class_len=0, domain_name=None):
    qos_elem_oui_type = 0x229a6f50
    qos_elem_id = 221

    if policy_id:
        qos_attr = struct.pack('BBBBB', 2, 3, policy_id, req_type, dscp)
        qos_attr_len = 5
    else:
        qos_attr = 0
        qos_attr_len = 0

    if start_port and end_port:
        port_range_attr = struct.pack('>BBHH', 1, 4, start_port, end_port)
        if qos_attr:
            qos_attr += port_range_attr
        else:
            qos_attr = port_range_attr
        qos_attr_len += 6

    if frame_classifier and frame_class_len:
        tclas_attr = struct.pack('>BB%ds' % (len(frame_classifier),), 3,
                                 len(frame_classifier), frame_classifier)
        if qos_attr:
            qos_attr += tclas_attr
        else:
            qos_attr = tclas_attr
        qos_attr_len += 2 + len(frame_classifier)

    if domain_name:
        s = bytes(domain_name, 'utf-8')
        domain_name_attr = struct.pack('>BB%ds' % (len(s),), 4, len(s), s)
        if qos_attr:
            qos_attr += domain_name_attr
        else:
            qos_attr = domain_name_attr
        qos_attr_len += 2 + len(s)

    qos_attr_len += 4
    qos_ie = struct.pack('<BBL', qos_elem_id, qos_attr_len,
                         qos_elem_oui_type) + qos_attr

    return qos_ie

def validate_dscp_req_event(dev, event):
    ev = dev.wait_event(["CTRL-EVENT-DSCP-POLICY"], timeout=2)
    if ev is None:
        raise Exception("No DSCP request reported")
    if ev != event:
        raise Exception("Invalid DSCP event received (%s; expected: %s)" % (ev, event))

def handle_dscp_query(hapd, query):
    msg = hapd.mgmt_rx()
    if msg['payload'] != query:
        raise Exception("Invalid DSCP Query received at AP")

def handle_dscp_response(hapd, response):
    msg = hapd.mgmt_rx()
    if msg['payload'] != response:
        raise Exception("Invalid DSCP Response received at AP")

def ap_sta_connectivity(dev, apdev, params):
    p = hostapd.wpa2_params(passphrase="12345678")
    p["wpa_key_mgmt"] = "WPA-PSK"
    p["ieee80211w"] = "1"
    p.update(params)
    hapd = hostapd.add_ap(apdev[0], p)
    register_dscp_req(hapd)

    dev[0].request("SET enable_dscp_policy_capa 1")
    dev[0].connect("dscp", psk="12345678", ieee80211w="1",
                   key_mgmt="WPA-PSK WPA-PSK-SHA256", scan_freq="2412")
    hapd.wait_sta()

    hapd.dump_monitor()
    hapd.set("ext_mgmt_frame_handling", "1")
    return hapd

def test_dscp_query(dev, apdev):
    """DSCP Policy Query"""

    # Positive tests
    #AP with DSCP Capabilities
    params = {"ssid": "dscp",
              "ext_capa": 6*"00" + "40",
              "assocresp_elements": "dd06506f9a230101",
              "vendor_elements": "dd06506f9a230101"}

    hapd = ap_sta_connectivity(dev, apdev, params)
    da = dev[0].own_addr()

    # Query 1
    cmd = "DSCP_QUERY wildcard"
    if "OK" not in dev[0].request(cmd):
        raise Exception("Sending DSCP Query failed")
    query = b'\x7e\x50\x6f\x9a\x1a\x00\x01'
    handle_dscp_query(hapd, query)

    # Query 2
    cmd = "DSCP_QUERY domain_name=example.com"
    if "OK" not in dev[0].request(cmd):
        raise Exception("Sending DSCP Query failed")
    query = b'\x7e\x50\x6f\x9a\x1a\x00\x02\xdd\x11\x50\x6f\x9a\x22\x04\x0b\x65\x78\x61\x6d\x70\x6c\x65\x2e\x63\x6f\x6d'
    handle_dscp_query(hapd, query)

    # Negative tests

    cmd = "DSCP_QUERY domain_name=" + 250*'a' + ".example.com"
    if "FAIL" not in dev[0].request(cmd):
        raise Exception("Invalid DSCP_QUERY accepted")

    dev[0].disconnect_and_stop_scan()
    # AP without DSCP Capabilities
    params = {"ssid": "dscp",
              "ext_capa": 6*"00" + "40"}
    hapd = ap_sta_connectivity(dev, apdev, params)

    # Query 3
    cmd = "DSCP_QUERY wildcard"
    if "FAIL" not in dev[0].request(cmd):
        raise Exception("Able to send invalid DSCP Query")

def test_dscp_request(dev, apdev):
    """DSCP Policy Request"""

    # Positive tests

    #AP with DSCP Capabilities
    params = {"ssid": "dscp",
              "ext_capa": 6*"00" + "40",
              "assocresp_elements": "dd06506f9a230101",
              "vendor_elements": "dd06506f9a230101"}

    hapd = ap_sta_connectivity(dev, apdev, params)
    da = dev[0].own_addr()

    # Request 1
    dialog_token = 5
    send_dscp_req(hapd, da, 1, dialog_token, 2, 0)
    event = "<3>CTRL-EVENT-DSCP-POLICY request_start clear_all"
    validate_dscp_req_event(dev[0], event)
    event = "<3>CTRL-EVENT-DSCP-POLICY request_end"
    validate_dscp_req_event(dev[0], event)

    # DSCP Request with multiple QoS IEs
    # QoS IE 1
    dialog_token = 1
    domain_name = "example.com"
    ipv4_src_addr = socket.inet_pton(socket.AF_INET, "192.168.0.1")
    ipv4_dest_addr = socket.inet_pton(socket.AF_INET, "192.168.0.2")
    frame_classifier_start = [4, 91, 4]
    frame_classifier_end = [12, 34, 12, 34, 0, 17, 0]
    frame_classifier = bytes(frame_classifier_start) + ipv4_src_addr + ipv4_dest_addr + bytes(frame_classifier_end)
    frame_len = len(frame_classifier)
    qos_ie = prepare_qos_ie(1, 0, 22, 0, 0, frame_classifier, frame_len, domain_name)

    # QoS IE 2
    ipv6_src_addr = socket.inet_pton(socket.AF_INET6, "aaaa:bbbb:cccc::1")
    ipv6_dest_addr = socket.inet_pton(socket.AF_INET6, "aaaa:bbbb:cccc::2")
    frame_classifier_start = [4, 79, 6]
    frame_classifier_end = [0, 12, 34, 0, 0, 17, 0, 0, 0]
    frame_classifier = bytes(frame_classifier_start) + ipv6_src_addr + ipv6_dest_addr + bytes(frame_classifier_end)
    frame_len = len(frame_classifier)
    ie = prepare_qos_ie(5, 0, 48, 12345, 23456, frame_classifier, frame_len,
                        None)
    qos_ie += ie

    # QoS IE 3
    ie = prepare_qos_ie(4, 0, 32, 12345, 23456, 0, 0, domain_name)
    qos_ie += ie
    send_dscp_req(hapd, da, 1, dialog_token, 0, qos_ie)

    event = "<3>CTRL-EVENT-DSCP-POLICY request_start"
    validate_dscp_req_event(dev[0], event)
    event = "<3>CTRL-EVENT-DSCP-POLICY add policy_id=1 dscp=22 ip_version=4 src_ip=192.168.0.1 src_port=3106 dst_port=3106 protocol=17 domain_name=example.com"
    validate_dscp_req_event(dev[0], event)
    event = "<3>CTRL-EVENT-DSCP-POLICY add policy_id=5 dscp=48 ip_version=6 src_ip=aaaa:bbbb:cccc::1 dst_ip=aaaa:bbbb:cccc::2 src_port=12 protocol=17 start_port=12345 end_port=23456"
    validate_dscp_req_event(dev[0], event)
    event = "<3>CTRL-EVENT-DSCP-POLICY add policy_id=4 dscp=32 ip_version=0 start_port=12345 end_port=23456 domain_name=example.com"
    validate_dscp_req_event(dev[0], event)
    event = "<3>CTRL-EVENT-DSCP-POLICY request_end"
    validate_dscp_req_event(dev[0], event)

    # Negative Tests

    # No DSCP policy attribute
    dialog_token = 4
    domain_name = "example.com"
    qos_ie = prepare_qos_ie(0, 0, 0, 12345, 23456, frame_classifier, frame_len,
                            domain_name)
    send_dscp_req(hapd, da, 1, dialog_token, 0, qos_ie)
    event = "<3>CTRL-EVENT-DSCP-POLICY request_start"
    validate_dscp_req_event(dev[0], event)
    event = "<3>CTRL-EVENT-DSCP-POLICY request_end"
    validate_dscp_req_event(dev[0], event)

    # No DSCP stream classifier params
    dialog_token = 6
    qos_ie = prepare_qos_ie(1, 0, 32, 0, 0, 0, 0, None)
    send_dscp_req(hapd, da, 1, dialog_token, 0, qos_ie)
    event = "<3>CTRL-EVENT-DSCP-POLICY request_start"
    validate_dscp_req_event(dev[0], event)
    event = "<3>CTRL-EVENT-DSCP-POLICY reject policy_id=1"
    validate_dscp_req_event(dev[0], event)
    event = "<3>CTRL-EVENT-DSCP-POLICY request_end"
    validate_dscp_req_event(dev[0], event)

    # DSCP request with both destination and domain name
    dialog_token = 7
    domain_name = "example.com"
    ipv4_src_addr = socket.inet_pton(socket.AF_INET, "192.168.0.1")
    ipv4_dest_addr = socket.inet_pton(socket.AF_INET, "192.168.0.2")
    frame_classifier_start = [4, 69, 4]
    frame_classifier_end = [0, 0, 0, 0, 0, 17, 0]
    frame_classifier = bytes(frame_classifier_start) + ipv4_src_addr + ipv4_dest_addr + bytes(frame_classifier_end)
    frame_len = len(frame_classifier)
    qos_ie = prepare_qos_ie(1, 0, 36, 0, 0, frame_classifier, frame_len,
                            domain_name)
    send_dscp_req(hapd, da, 1, dialog_token, 0, qos_ie)
    event  = "<3>CTRL-EVENT-DSCP-POLICY request_start"
    validate_dscp_req_event(dev[0], event)
    event = "<3>CTRL-EVENT-DSCP-POLICY reject policy_id=1"
    validate_dscp_req_event(dev[0], event)
    event = "<3>CTRL-EVENT-DSCP-POLICY request_end"
    validate_dscp_req_event(dev[0], event)

    # DSCP request with both port range and destination port
    frame_classifier_start = [4, 81, 4]
    frame_classifier_end = [0, 0, 23, 45, 0, 17, 0]
    frame_classifier = bytes(frame_classifier_start) + ipv4_src_addr + ipv4_dest_addr + bytes(frame_classifier_end)
    frame_len = len(frame_classifier)
    qos_ie = prepare_qos_ie(1, 0, 36, 12345, 23456, frame_classifier, frame_len,
                            None)
    send_dscp_req(hapd, da, 1, dialog_token, 0, qos_ie)
    event = "<3>CTRL-EVENT-DSCP-POLICY request_start"
    validate_dscp_req_event(dev[0], event)
    event = "<3>CTRL-EVENT-DSCP-POLICY reject policy_id=1"
    validate_dscp_req_event(dev[0], event)
    event = "<3>CTRL-EVENT-DSCP-POLICY request_end"
    validate_dscp_req_event(dev[0], event)

    # Too short DSCP Policy Request frame
    dialog_token += 1
    send_dscp_req(hapd, da, 1, dialog_token, 0, None, truncate=True)

    # Request Type: Remove
    dialog_token += 1
    qos_ie = prepare_qos_ie(1, 1, 36)
    send_dscp_req(hapd, da, 1, dialog_token, 0, qos_ie)
    validate_dscp_req_event(dev[0], "<3>CTRL-EVENT-DSCP-POLICY request_start")
    validate_dscp_req_event(dev[0],
                            "<3>CTRL-EVENT-DSCP-POLICY remove policy_id=1")
    validate_dscp_req_event(dev[0], "<3>CTRL-EVENT-DSCP-POLICY request_end")

    # Request Type: Reserved
    dialog_token += 1
    qos_ie = prepare_qos_ie(1, 2, 36)
    send_dscp_req(hapd, da, 1, dialog_token, 0, qos_ie)
    validate_dscp_req_event(dev[0], "<3>CTRL-EVENT-DSCP-POLICY request_start")
    validate_dscp_req_event(dev[0],
                            "<3>CTRL-EVENT-DSCP-POLICY reject policy_id=1")
    validate_dscp_req_event(dev[0], "<3>CTRL-EVENT-DSCP-POLICY request_end")

def test_dscp_response(dev, apdev):
    """DSCP Policy Response"""

    # Positive tests

    # AP with DSCP Capabilities
    params = {"ssid": "dscp",
              "ext_capa": 6*"00" + "40",
              "assocresp_elements": "dd06506f9a230101",
              "vendor_elements": "dd06506f9a230101"}
    hapd = ap_sta_connectivity(dev, apdev, params)
    da = dev[0].own_addr()

    # Sending solicited DSCP response after receiving DSCP request
    dialog_token = 1
    domain_name = "example.com"
    ipv4_src_addr = socket.inet_pton(socket.AF_INET, "192.168.0.1")
    ipv4_dest_addr = socket.inet_pton(socket.AF_INET, "192.168.0.2")
    frame_classifier_start = [4,91,4]
    frame_classifier_end = [12,34,12,34,0,17,0]
    frame_classifier = bytes(frame_classifier_start) + ipv4_src_addr + ipv4_dest_addr + bytes(frame_classifier_end)
    frame_len = len(frame_classifier)
    qos_ie = prepare_qos_ie(1, 0, 22, 0, 0, frame_classifier, frame_len,
                            domain_name)
    ie = prepare_qos_ie(4, 0, 32, 12345, 23456, 0, 0, domain_name)
    qos_ie += ie
    send_dscp_req(hapd, da, 1, dialog_token, 0, qos_ie)

    cmd = "DSCP_RESP solicited policy_id=1 status=0 policy_id=4 status=0"
    if "OK" not in dev[0].request(cmd):
        raise Exception("Sending DSCP Response failed")
    response = b'\x7e\x50\x6f\x9a\x1a\x02\x01\x00\x02\x01\x00\x04\x00'
    handle_dscp_response(hapd, response)

    # Unsolicited DSCP Response without status duples
    cmd = "DSCP_RESP reset more"
    if "OK" not in dev[0].request(cmd):
        raise Exception("Sending DSCP Response failed")
    response = b'\x7e\x50\x6f\x9a\x1a\x02\x00\x03\x00'
    handle_dscp_response(hapd, response)

    # Unsolicited DSCP Response with one status duple
    cmd = "DSCP_RESP policy_id=2 status=0"
    if "OK" not in dev[0].request(cmd):
        raise Exception("Sending DSCP Response failed")
    response = b'\x7e\x50\x6f\x9a\x1a\x02\x00\x00\x01\x02\x00'
    handle_dscp_response(hapd, response)

    # Negative tests

    # Send solicited DSCP Response without prior DSCP request
    cmd = "DSCP_RESP solicited policy_id=1 status=0 policy_id=5 status=0"
    if "FAIL" not in dev[0].request(cmd):
        raise Exception("Able to send invalid DSCP response")

def test_dscp_unsolicited_req_at_assoc(dev, apdev):
    """DSCP Policy and unsolicited request at association"""
    params = {"ssid": "dscp",
              "ext_capa": 6*"00" + "40",
              "assocresp_elements": "dd06506f9a230103",
              "vendor_elements": "dd06506f9a230103"}
    hapd = ap_sta_connectivity(dev, apdev, params)
    da = dev[0].own_addr()

    dialog_token = 1
    qos_ie = prepare_qos_ie(1, 0, 36, 12345, 23456)
    send_dscp_req(hapd, da, 1, dialog_token, 0, qos_ie)
    validate_dscp_req_event(dev[0], "<3>CTRL-EVENT-DSCP-POLICY request_start")
    validate_dscp_req_event(dev[0], "<3>CTRL-EVENT-DSCP-POLICY add policy_id=1 dscp=36 ip_version=0 start_port=12345 end_port=23456")
    validate_dscp_req_event(dev[0], "<3>CTRL-EVENT-DSCP-POLICY request_end")

    cmd = "DSCP_QUERY wildcard"
    if "OK" not in dev[0].request(cmd):
        raise Exception("Sending DSCP Query failed")

def test_dscp_missing_unsolicited_req_at_assoc(dev, apdev):
    """DSCP Policy and missing unsolicited request at association"""
    params = {"ssid": "dscp",
              "ext_capa": 6*"00" + "40",
              "assocresp_elements": "dd06506f9a230103",
              "vendor_elements": "dd06506f9a230103"}
    hapd = ap_sta_connectivity(dev, apdev, params)
    da = dev[0].own_addr()

    cmd = "DSCP_QUERY wildcard"
    if "FAIL" not in dev[0].request(cmd):
        raise Exception("DSCP_QUERY accepted during wait for unsolicited requesdt")
    time.sleep(5)
    validate_dscp_req_event(dev[0], "<3>CTRL-EVENT-DSCP-POLICY request_wait end")

    cmd = "DSCP_QUERY wildcard"
    if "OK" not in dev[0].request(cmd):
        raise Exception("Sending DSCP Query failed")
