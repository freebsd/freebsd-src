# P2P vendor specific extension tests
# Copyright (c) 2014-2015, Qualcomm Atheros, Inc.
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

from remotehost import remote_compatible
import logging
logger = logging.getLogger()
import os

from tshark import run_tshark
from p2p_utils import *

@remote_compatible
def test_p2p_ext_discovery(dev):
    """P2P device discovery with vendor specific extensions"""
    addr0 = dev[0].p2p_dev_addr()
    addr1 = dev[1].p2p_dev_addr()

    try:
        if "OK" not in dev[0].request("VENDOR_ELEM_ADD 1 dd050011223344"):
            raise Exception("VENDOR_ELEM_ADD failed")
        res = dev[0].request("VENDOR_ELEM_GET 1")
        if res != "dd050011223344":
            raise Exception("Unexpected VENDOR_ELEM_GET result: " + res)
        if "OK" not in dev[0].request("VENDOR_ELEM_ADD 1 dd06001122335566"):
            raise Exception("VENDOR_ELEM_ADD failed")
        res = dev[0].request("VENDOR_ELEM_GET 1")
        if res != "dd050011223344dd06001122335566":
            raise Exception("Unexpected VENDOR_ELEM_GET result(2): " + res)
        res = dev[0].request("VENDOR_ELEM_GET 2")
        if res != "":
            raise Exception("Unexpected VENDOR_ELEM_GET result(3): " + res)
        if "OK" not in dev[0].request("VENDOR_ELEM_REMOVE 1 dd050011223344"):
            raise Exception("VENDOR_ELEM_REMOVE failed")
        res = dev[0].request("VENDOR_ELEM_GET 1")
        if res != "dd06001122335566":
            raise Exception("Unexpected VENDOR_ELEM_GET result(4): " + res)
        if "OK" not in dev[0].request("VENDOR_ELEM_REMOVE 1 dd06001122335566"):
            raise Exception("VENDOR_ELEM_REMOVE failed")
        res = dev[0].request("VENDOR_ELEM_GET 1")
        if res != "":
            raise Exception("Unexpected VENDOR_ELEM_GET result(5): " + res)
        if "OK" not in dev[0].request("VENDOR_ELEM_ADD 1 dd050011223344dd06001122335566"):
            raise Exception("VENDOR_ELEM_ADD failed(2)")

        if "FAIL" not in dev[0].request("VENDOR_ELEM_REMOVE 1 dd051122334455"):
            raise Exception("Unexpected VENDOR_ELEM_REMOVE success")
        if "FAIL" not in dev[0].request("VENDOR_ELEM_REMOVE 1 dd"):
            raise Exception("Unexpected VENDOR_ELEM_REMOVE success(2)")
        if "FAIL" not in dev[0].request("VENDOR_ELEM_ADD 1 ddff"):
            raise Exception("Unexpected VENDOR_ELEM_ADD success(3)")

        dev[0].p2p_listen()
        if not dev[1].discover_peer(addr0):
            raise Exception("Device discovery timed out")
        if not dev[0].discover_peer(addr1):
            raise Exception("Device discovery timed out")

        peer = dev[1].get_peer(addr0)
        if peer['vendor_elems'] != "dd050011223344dd06001122335566":
            raise Exception("Vendor elements not reported correctly")

        res = dev[0].request("VENDOR_ELEM_GET 1")
        if res != "dd050011223344dd06001122335566":
            raise Exception("Unexpected VENDOR_ELEM_GET result(6): " + res)
        if "OK" not in dev[0].request("VENDOR_ELEM_REMOVE 1 dd06001122335566"):
            raise Exception("VENDOR_ELEM_REMOVE failed")
        res = dev[0].request("VENDOR_ELEM_GET 1")
        if res != "dd050011223344":
            raise Exception("Unexpected VENDOR_ELEM_GET result(7): " + res)
    finally:
        dev[0].request("VENDOR_ELEM_REMOVE 1 *")

@remote_compatible
def test_p2p_ext_discovery_go(dev):
    """P2P device discovery with vendor specific extensions for GO"""
    addr0 = dev[0].p2p_dev_addr()
    addr1 = dev[1].p2p_dev_addr()

    try:
        if "OK" not in dev[0].request("VENDOR_ELEM_ADD 2 dd050011223344dd06001122335566"):
            raise Exception("VENDOR_ELEM_ADD failed")
        if "OK" not in dev[0].request("VENDOR_ELEM_ADD 3 dd050011223344dd06001122335566"):
            raise Exception("VENDOR_ELEM_ADD failed")
        if "OK" not in dev[0].request("VENDOR_ELEM_ADD 12 dd050011223344dd06001122335566"):
            raise Exception("VENDOR_ELEM_ADD failed")

        dev[0].p2p_start_go(freq="2412")
        if not dev[1].discover_peer(addr0):
            raise Exception("Device discovery timed out")
        peer = dev[1].get_peer(addr0)
        if peer['vendor_elems'] != "dd050011223344dd06001122335566":
            logger.info("Peer vendor_elems: " + peer['vendor_elems'])
            raise Exception("Vendor elements not reported correctly")
    finally:
        dev[0].request("VENDOR_ELEM_REMOVE 2 *")
        dev[0].request("VENDOR_ELEM_REMOVE 3 *")
        dev[0].request("VENDOR_ELEM_REMOVE 12 *")

def test_p2p_ext_vendor_elem_probe_req(dev):
    """VENDOR_ELEM in P2P Probe Request frames"""
    try:
        _test_p2p_ext_vendor_elem_probe_req(dev)
    finally:
        dev[0].request("VENDOR_ELEM_REMOVE 0 *")

def _test_p2p_ext_vendor_elem_probe_req(dev):
    addr1 = dev[1].p2p_dev_addr()
    if "OK" not in dev[0].request("VENDOR_ELEM_ADD 0 dd050011223300"):
        raise Exception("VENDOR_ELEM_ADD failed")
    dev[1].p2p_listen()
    if not dev[0].discover_peer(addr1):
        raise Exception("Device discovery timed out")
    if "FAIL" in dev[1].request("SET ext_mgmt_frame_handling 1"):
        raise Exception("Failed to enable external management frame handling")
    ev = dev[1].wait_event(["MGMT-RX"], timeout=5)
    if ev is None:
        raise Exception("MGMT-RX timeout")
    if " 40" not in ev:
        raise Exception("Not a Probe Request frame")
    if "dd050011223300" not in ev:
        raise Exception("Vendor element not found from Probe Request frame")
    dev[0].p2p_stop_find()
    dev[1].p2p_stop_find()

def test_p2p_ext_vendor_elem_pd_req(dev):
    """VENDOR_ELEM in PD Request frames"""
    try:
        _test_p2p_ext_vendor_elem_pd_req(dev)
    finally:
        dev[0].request("VENDOR_ELEM_REMOVE 4 *")

def _test_p2p_ext_vendor_elem_pd_req(dev):
    addr0 = dev[0].p2p_dev_addr()
    addr1 = dev[1].p2p_dev_addr()
    if "OK" not in dev[0].request("VENDOR_ELEM_ADD 4 dd050011223301"):
        raise Exception("VENDOR_ELEM_ADD failed")
    dev[1].p2p_listen()
    if not dev[0].discover_peer(addr1):
        raise Exception("Device discovery timed out")
    dev[0].p2p_stop_find()
    if "FAIL" in dev[1].request("SET ext_mgmt_frame_handling 1"):
        raise Exception("Failed to enable external management frame handling")
    dev[0].global_request("P2P_PROV_DISC " + addr1 + " display")
    for i in range(5):
        ev = dev[1].wait_event(["MGMT-RX"], timeout=5)
        if ev is None:
            raise Exception("MGMT-RX timeout")
        if " d0" in ev:
            break
    if "dd050011223301" not in ev:
        raise Exception("Vendor element not found from PD Request frame")
    dev[1].p2p_stop_find()
    dev[0].p2p_stop_find()

def test_p2p_ext_vendor_elem_pd_resp(dev):
    """VENDOR_ELEM in PD Response frames"""
    try:
        _test_p2p_ext_vendor_elem_pd_resp(dev)
    finally:
        dev[0].request("VENDOR_ELEM_REMOVE 5 *")

def _test_p2p_ext_vendor_elem_pd_resp(dev):
    addr0 = dev[0].p2p_dev_addr()
    addr1 = dev[1].p2p_dev_addr()
    if "OK" not in dev[0].request("VENDOR_ELEM_ADD 5 dd050011223302"):
        raise Exception("VENDOR_ELEM_ADD failed")
    dev[0].p2p_listen()
    if not dev[1].discover_peer(addr0):
        raise Exception("Device discovery timed out")
    dev[1].p2p_stop_find()
    if "FAIL" in dev[1].request("SET ext_mgmt_frame_handling 1"):
        raise Exception("Failed to enable external management frame handling")
    dev[1].global_request("P2P_PROV_DISC " + addr0 + " display")
    for i in range(5):
        ev = dev[1].wait_event(["MGMT-RX"], timeout=5)
        if ev is None:
            raise Exception("MGMT-RX timeout")
        if " d0" in ev:
            break
    if "dd050011223302" not in ev:
        raise Exception("Vendor element not found from PD Response frame")
    dev[0].p2p_stop_find()
    dev[1].p2p_stop_find()

def test_p2p_ext_vendor_elem_go_neg_req(dev):
    """VENDOR_ELEM in GO Negotiation Request frames"""
    try:
        _test_p2p_ext_vendor_elem_go_neg_req(dev)
    finally:
        dev[0].request("VENDOR_ELEM_REMOVE 6 *")

def _test_p2p_ext_vendor_elem_go_neg_req(dev):
    addr0 = dev[0].p2p_dev_addr()
    addr1 = dev[1].p2p_dev_addr()
    if "OK" not in dev[0].request("VENDOR_ELEM_ADD 6 dd050011223303"):
        raise Exception("VENDOR_ELEM_ADD failed")
    dev[1].p2p_listen()
    if not dev[0].discover_peer(addr1):
        raise Exception("Device discovery timed out")
    dev[0].p2p_stop_find()
    if "FAIL" in dev[1].request("SET ext_mgmt_frame_handling 1"):
        raise Exception("Failed to enable external management frame handling")
    dev[0].global_request("P2P_CONNECT " + addr1 + " 12345670 display")
    for i in range(5):
        ev = dev[1].wait_event(["MGMT-RX"], timeout=5)
        if ev is None:
            raise Exception("MGMT-RX timeout")
        if " d0" in ev:
            break
    if "dd050011223303" not in ev:
        raise Exception("Vendor element not found from GO Negotiation Request frame")
    dev[1].p2p_stop_find()
    dev[0].p2p_stop_find()

def test_p2p_ext_vendor_elem_go_neg_resp(dev):
    """VENDOR_ELEM in GO Negotiation Response frames"""
    try:
        _test_p2p_ext_vendor_elem_go_neg_resp(dev)
    finally:
        dev[0].request("VENDOR_ELEM_REMOVE 7 *")

def _test_p2p_ext_vendor_elem_go_neg_resp(dev):
    addr0 = dev[0].p2p_dev_addr()
    addr1 = dev[1].p2p_dev_addr()
    if "OK" not in dev[0].request("VENDOR_ELEM_ADD 7 dd050011223304"):
        raise Exception("VENDOR_ELEM_ADD failed")
    dev[0].p2p_listen()
    if not dev[1].discover_peer(addr0):
        raise Exception("Device discovery timed out")
    dev[1].p2p_stop_find()
    if "FAIL" in dev[1].request("SET ext_mgmt_frame_handling 1"):
        raise Exception("Failed to enable external management frame handling")
    dev[1].global_request("P2P_CONNECT " + addr0 + " 12345670 display")
    for i in range(5):
        ev = dev[1].wait_event(["MGMT-RX"], timeout=5)
        if ev is None:
            raise Exception("MGMT-RX timeout")
        if " d0" in ev:
            break
    if "dd050011223304" not in ev:
        raise Exception("Vendor element not found from GO Negotiation Response frame")
    dev[0].p2p_stop_find()
    dev[1].p2p_stop_find()

def test_p2p_ext_vendor_elem_go_neg_conf(dev, apdev, params):
    """VENDOR_ELEM in GO Negotiation Confirm frames"""
    try:
        _test_p2p_ext_vendor_elem_go_neg_conf(dev, apdev, params)
    finally:
        dev[0].request("VENDOR_ELEM_REMOVE 8 *")

def _test_p2p_ext_vendor_elem_go_neg_conf(dev, apdev, params):
    addr0 = dev[0].p2p_dev_addr()
    addr1 = dev[1].p2p_dev_addr()
    if "OK" not in dev[0].request("VENDOR_ELEM_ADD 8 dd050011223305"):
        raise Exception("VENDOR_ELEM_ADD failed")
    dev[0].p2p_listen()
    dev[1].p2p_go_neg_auth(addr0, "12345670", "enter")
    dev[1].p2p_listen()
    dev[0].p2p_go_neg_init(addr1, "12345678", "display")
    ev = dev[0].wait_global_event(["P2P-GO-NEG-SUCCESS"], timeout=15)
    if ev is None:
        raise Exception("GO negotiation timed out")
    ev = dev[0].wait_global_event(["P2P-GROUP-FORMATION-FAILURE"], timeout=15)
    if ev is None:
        raise Exception("Group formation failure not indicated")
    dev[0].dump_monitor()
    dev[1].p2p_go_neg_auth_result(expect_failure=True)
    dev[1].dump_monitor()

    out = run_tshark(os.path.join(params['logdir'], "hwsim0.pcapng"),
                     "wifi_p2p.public_action.subtype == 2")
    if "Vendor Specific Data: 3305" not in out:
        raise Exception("Vendor element not found from GO Negotiation Confirm frame")

def test_p2p_ext_vendor_elem_invitation(dev):
    """VENDOR_ELEM in Invitation frames"""
    try:
        _test_p2p_ext_vendor_elem_invitation(dev)
    finally:
        dev[0].request("VENDOR_ELEM_REMOVE 9 *")
        dev[0].request("VENDOR_ELEM_REMOVE 10 *")

def _test_p2p_ext_vendor_elem_invitation(dev):
    addr0 = dev[0].p2p_dev_addr()
    addr1 = dev[1].p2p_dev_addr()
    form(dev[0], dev[1])
    if "OK" not in dev[0].request("VENDOR_ELEM_ADD 9 dd050011223306"):
        raise Exception("VENDOR_ELEM_ADD failed")
    if "OK" not in dev[0].request("VENDOR_ELEM_ADD 10 dd050011223307"):
        raise Exception("VENDOR_ELEM_ADD failed")
    dev[1].p2p_listen()
    if not dev[0].discover_peer(addr1):
        raise Exception("Device discovery timed out")
    peer = dev[0].get_peer(addr1)
    dev[0].p2p_stop_find()
    if "FAIL" in dev[1].request("SET ext_mgmt_frame_handling 1"):
        raise Exception("Failed to enable external management frame handling")
    dev[0].global_request("P2P_INVITE persistent=" + peer['persistent'] + " peer=" + addr1)
    for i in range(5):
        ev = dev[1].wait_event(["MGMT-RX"], timeout=5)
        if ev is None:
            raise Exception("MGMT-RX timeout")
        if " d0" in ev:
            break
    if "dd050011223306" not in ev:
        raise Exception("Vendor element not found from Invitation Request frame")
    dev[0].p2p_stop_find()
    dev[1].p2p_stop_find()

    dev[0].dump_monitor()
    dev[1].dump_monitor()

    dev[0].p2p_listen()
    if "FAIL" in dev[1].request("SET ext_mgmt_frame_handling 0"):
        raise Exception("Failed to disable external management frame handling")
    if not dev[1].discover_peer(addr0):
        raise Exception("Device discovery timed out")
    peer = dev[1].get_peer(addr0)
    dev[1].p2p_stop_find()
    if "FAIL" in dev[1].request("SET ext_mgmt_frame_handling 1"):
        raise Exception("Failed to enable external management frame handling")
    dev[1].global_request("P2P_INVITE persistent=" + peer['persistent'] + " peer=" + addr0)
    for i in range(5):
        ev = dev[1].wait_event(["MGMT-RX"], timeout=5)
        if ev is None:
            raise Exception("MGMT-RX timeout")
        if " d0" in ev:
            break
    if "dd050011223307" not in ev:
        raise Exception("Vendor element not found from Invitation Response frame")
    ev = dev[0].wait_global_event(["P2P-GROUP-STARTED"], timeout=5)
    if ev is None:
        raise Exception("Group start not reported")
    dev[0].group_form_result(ev)
    dev[0].remove_group()
    dev[0].p2p_stop_find()
    dev[1].p2p_stop_find()

def test_p2p_ext_vendor_elem_assoc(dev, apdev, params):
    """VENDOR_ELEM in Association frames"""
    try:
        _test_p2p_ext_vendor_elem_assoc(dev, apdev, params)
    finally:
        dev[0].request("VENDOR_ELEM_REMOVE 11 *")
        dev[1].request("VENDOR_ELEM_REMOVE 12 *")
        dev[0].request("VENDOR_ELEM_REMOVE 13 *")

def _test_p2p_ext_vendor_elem_assoc(dev, apdev, params):
    addr0 = dev[0].p2p_dev_addr()
    addr1 = dev[1].p2p_dev_addr()

    res = dev[0].get_driver_status()
    p2p_device = True if (int(res['capa.flags'], 0) & 0x20000000) else False

    if "OK" not in dev[0].request("VENDOR_ELEM_ADD 11 dd050011223308"):
        raise Exception("VENDOR_ELEM_ADD failed")
    if "OK" not in dev[1].request("VENDOR_ELEM_ADD 12 dd050011223309"):
        raise Exception("VENDOR_ELEM_ADD failed")
    if not p2p_device and "OK" not in dev[0].request("VENDOR_ELEM_ADD 13 dd05001122330a"):
        raise Exception("VENDOR_ELEM_ADD failed")
    dev[0].p2p_listen()
    dev[1].p2p_listen()
    dev[1].p2p_go_neg_auth(addr0, "12345670", "enter", go_intent=15)
    dev[0].p2p_go_neg_init(addr1, "12345670", "display", go_intent=0,
                           timeout=15)
    dev[1].p2p_go_neg_auth_result()
    dev[1].remove_group()
    dev[0].wait_go_ending_session()

    out = run_tshark(os.path.join(params['logdir'], "hwsim0.pcapng"),
                     "wlan.fc.type_subtype == 0x00", wait=False)
    if "Vendor Specific Data: 3308" not in out:
        raise Exception("Vendor element (P2P) not found from Association Request frame")
    if not p2p_device and "Vendor Specific Data: 330a" not in out:
        raise Exception("Vendor element (non-P2P) not found from Association Request frame")

    out = run_tshark(os.path.join(params['logdir'], "hwsim0.pcapng"),
                     "wlan.fc.type_subtype == 0x01", wait=False)
    if "Vendor Specific Data: 3309" not in out:
        raise Exception("Vendor element not found from Association Response frame")
