# Radio measurement
# Copyright(c) 2013 - 2016 Intel Mobile Communications GmbH.
# Copyright(c) 2011 - 2016 Intel Corporation. All rights reserved.
# Copyright (c) 2017, Jouni Malinen <j@w1.fi>
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

import binascii
import re
import logging
logger = logging.getLogger()
import struct
import subprocess
import time

import hostapd
from wpasupplicant import WpaSupplicant
from utils import *
from remotehost import remote_compatible

def check_rrm_support(dev):
    rrm = int(dev.get_driver_status_field("capa.rrm_flags"), 16)
    if rrm & 0x5 != 0x5 and rrm & 0x10 != 0x10:
        raise HwsimSkip("Required RRM capabilities are not supported")

def check_tx_power_support(dev):
    rrm = int(dev.get_driver_status_field("capa.rrm_flags"), 16)
    if rrm & 0x8 != 0x8:
        raise HwsimSkip("Required RRM capabilities are not supported")

nr = "00112233445500000000510107"
lci = "01000800101298c0b512926666f6c2f1001c00004104050000c00012"
civic = "01000b0011223344556677889900998877665544332211aabbccddeeff"

def check_nr_results(dev, bssids=None, lci=False, civic=False):
    if bssids is None:
        ev = dev.wait_event(["RRM-NEIGHBOR-REP-REQUEST-FAILED"], timeout=10)
        if ev is None:
            raise Exception("RRM neighbor report failure not received")
        return

    received = []
    for bssid in bssids:
        ev = dev.wait_event(["RRM-NEIGHBOR-REP-RECEIVED"], timeout=10)
        if ev is None:
            raise Exception("RRM report result not indicated")
        received.append(ev)

    for bssid in bssids:
        found = False
        for r in received:
            if "RRM-NEIGHBOR-REP-RECEIVED bssid=" + bssid in r:
                if lci and "lci=" not in r:
                    raise Exception("LCI data not reported for %s" % bssid)
                if civic and "civic=" not in r:
                    raise Exception("civic data not reported for %s" % bssid)
                received.remove(r)
                found = True
                break
        if not found:
            raise Exception("RRM report result for %s not indicated" % bssid)

def test_rrm_neighbor_db(dev, apdev):
    """hostapd ctrl_iface SET_NEIGHBOR"""
    params = {"ssid": "test", "rrm_neighbor_report": "1"}
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)
    params = {"ssid": "test2", "rrm_neighbor_report": "1"}
    hapd2 = hostapd.add_ap(apdev[1]['ifname'], params)

    res = hapd.request("SHOW_NEIGHBOR")
    if len(res.splitlines()) != 1:
        raise Exception("Unexpected SHOW_NEIGHBOR output(1): " + res)
    if apdev[0]['bssid'] not in res:
        raise Exception("Own BSS not visible in SHOW_NEIGHBOR output")

    if "OK" not in hapd2.request("SET_NEIGHBOR " + res.strip()):
        raise Exception("Failed to copy neighbor entry to another hostapd")
    res2 = hapd2.request("SHOW_NEIGHBOR")
    if len(res2.splitlines()) != 2:
        raise Exception("Unexpected SHOW_NEIGHBOR output: " + res2)
    if res not in res2:
        raise Exception("Copied entry not visible")

    # Bad BSSID
    if "FAIL" not in hapd.request("SET_NEIGHBOR 00:11:22:33:44:gg ssid=\"test1\" nr=" + nr):
        raise Exception("Set neighbor succeeded unexpectedly")

    # Bad SSID
    if "FAIL" not in hapd.request("SET_NEIGHBOR 00:11:22:33:44:55 ssid=test1 nr=" + nr):
        raise Exception("Set neighbor succeeded unexpectedly")

    # Bad SSID end
    if "FAIL" not in hapd.request("SET_NEIGHBOR 00:11:22:33:44:55 ssid=\"test1 nr=" + nr):
        raise Exception("Set neighbor succeeded unexpectedly")

    # No SSID
    if "FAIL" not in hapd.request("SET_NEIGHBOR 00:11:22:33:44:55 nr=" + nr):
        raise Exception("Set neighbor succeeded unexpectedly")

    # No NR
    if "FAIL" not in hapd.request("SET_NEIGHBOR 00:11:22:33:44:55 ssid=\"test1\""):
        raise Exception("Set neighbor succeeded unexpectedly")

    # Odd length of NR
    if "FAIL" not in hapd.request("SET_NEIGHBOR 00:11:22:33:44:55 ssid=\"test1\" nr=" + nr[:-1]):
        raise Exception("Set neighbor succeeded unexpectedly")

    # Invalid lci
    if "FAIL" not in hapd.request("SET_NEIGHBOR 00:11:22:33:44:55 ssid=\"test1\" nr=" + nr + " lci=1"):
        raise Exception("Set neighbor succeeded unexpectedly")

    # Invalid civic
    if "FAIL" not in hapd.request("SET_NEIGHBOR 00:11:22:33:44:55 ssid=\"test1\" nr=" + nr + " civic=1"):
        raise Exception("Set neighbor succeeded unexpectedly")

    # No entry yet in database
    if "FAIL" not in hapd.request("REMOVE_NEIGHBOR 00:11:22:33:44:55 ssid=\"test1\""):
        raise Exception("Remove neighbor succeeded unexpectedly")

    # Add a neighbor entry
    if "OK" not in hapd.request("SET_NEIGHBOR 00:11:22:33:44:55 ssid=\"test1\" nr=" + nr + " lci=" + lci + " civic=" + civic):
        raise Exception("Set neighbor failed")

    res = hapd.request("SHOW_NEIGHBOR")
    if len(res.splitlines()) != 2:
        raise Exception("Unexpected SHOW_NEIGHBOR output(2): " + res)
    if apdev[0]['bssid'] not in res:
        raise Exception("Own BSS not visible in SHOW_NEIGHBOR output")
    if "00:11:22:33:44:55" not in res:
        raise Exception("Added BSS not visible in SHOW_NEIGHBOR output")

    # Another BSSID with the same SSID
    if "OK" not in hapd.request("SET_NEIGHBOR 00:11:22:33:44:56 ssid=\"test1\" nr=" + nr + " lci=" + lci + " civic=" + civic):
        raise Exception("Set neighbor failed")

    res = hapd.request("SHOW_NEIGHBOR")
    if len(res.splitlines()) != 3:
        raise Exception("Unexpected SHOW_NEIGHBOR output(3): " + res)
    if apdev[0]['bssid'] not in res:
        raise Exception("Own BSS not visible in SHOW_NEIGHBOR output")
    if "00:11:22:33:44:55" not in res:
        raise Exception("Added BSS not visible in SHOW_NEIGHBOR output")
    if "00:11:22:33:44:56" not in res:
        raise Exception("Second added BSS not visible in SHOW_NEIGHBOR output")

    # Fewer parameters
    if "OK" not in hapd.request("SET_NEIGHBOR 00:11:22:33:44:55 ssid=\"test1\" nr=" + nr):
        raise Exception("Set neighbor failed")

    # SSID in hex format
    if "OK" not in hapd.request("SET_NEIGHBOR 00:11:22:33:44:55 ssid=7465737431 nr=" + nr):
        raise Exception("Set neighbor failed")

    # With more parameters
    if "OK" not in hapd.request("SET_NEIGHBOR 00:11:22:33:44:55 ssid=\"test1\" nr=" + nr + " civic=" + civic):
        raise Exception("Set neighbor failed")

    # With all parameters
    if "OK" not in hapd.request("SET_NEIGHBOR 00:11:22:33:44:55 ssid=\"test1\" nr=" + nr + " lci=" + lci + " civic=" + civic):
        raise Exception("Set neighbor failed")

    # Another SSID on the same BSSID
    if "OK" not in hapd.request("SET_NEIGHBOR 00:11:22:33:44:55 ssid=\"test2\" nr=" + nr + " lci=" + lci):
        raise Exception("Set neighbor failed")

    if "OK" not in hapd.request("REMOVE_NEIGHBOR 00:11:22:33:44:55 ssid=\"test1\""):
        raise Exception("Remove neighbor failed")

    if "OK" not in hapd.request("REMOVE_NEIGHBOR 00:11:22:33:44:56 ssid=\"test1\""):
        raise Exception("Remove neighbor failed")

    if "OK" not in hapd.request("REMOVE_NEIGHBOR 00:11:22:33:44:55 ssid=\"test2\""):
        raise Exception("Remove neighbor failed")

    # Double remove
    if "FAIL" not in hapd.request("REMOVE_NEIGHBOR 00:11:22:33:44:55 ssid=\"test1\""):
        raise Exception("Remove neighbor succeeded unexpectedly")

    # Stationary AP
    if "OK" not in hapd.request("SET_NEIGHBOR 00:11:22:33:44:55 ssid=\"test3\" nr=" + nr + " lci=" + lci + " civic=" + civic + " stat"):
        raise Exception("Set neighbor failed")

    res = hapd.request("SHOW_NEIGHBOR")
    if len(res.splitlines()) != 2:
        raise Exception("Unexpected SHOW_NEIGHBOR output(4): " + res)
    if "00:11:22:33:44:55" not in res or " stat" not in res:
        raise Exception("Unexpected SHOW_NEIGHBOR output(4b): " + res)

    if "OK" not in hapd.request("REMOVE_NEIGHBOR 00:11:22:33:44:55 ssid=\"test3\""):
        raise Exception("Remove neighbor failed")

    # Add an entry for following REMOVE_NEIGHBOR tests
    if "OK" not in hapd.request("SET_NEIGHBOR 00:11:22:33:44:55 ssid=7465737431 nr=" + nr):
        raise Exception("Set neighbor failed")

    # Invalid remove - bad BSSID
    if "FAIL" not in hapd.request("REMOVE_NEIGHBOR 00:11:22:33:44:5 ssid=\"test1\""):
        raise Exception("Remove neighbor succeeded unexpectedly")

    # Invalid remove - bad SSID
    if "FAIL" not in hapd.request("REMOVE_NEIGHBOR 00:11:22:33:44:55 ssid=\"test1"):
        raise Exception("Remove neighbor succeeded unexpectedly")

    # Remove without specifying SSID
    if "OK" not in hapd.request("REMOVE_NEIGHBOR 00:11:22:33:44:55"):
        raise Exception("Remove neighbor without SSID failed")

    res = hapd.request("SHOW_NEIGHBOR")
    if len(res.splitlines()) != 1:
        raise Exception("Unexpected SHOW_NEIGHBOR output(5): " + res)
    if apdev[0]['bssid'] not in res:
        raise Exception("Own BSS not visible in SHOW_NEIGHBOR output")

def test_rrm_neighbor_db_failures(dev, apdev):
    """hostapd ctrl_iface SET_NEIGHBOR failures"""
    params = {"ssid": "test", "rrm_neighbor_report": "1"}
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)
    cmd = "SET_NEIGHBOR 00:11:22:33:44:55 ssid=\"test1\" nr=" + nr + " lci=" + lci + " civic=" + civic
    tests = [(1, "hostapd_neighbor_add"),
             (1, "wpabuf_dup;hostapd_neighbor_set"),
             (2, "wpabuf_dup;hostapd_neighbor_set"),
             (3, "wpabuf_dup;hostapd_neighbor_set")]
    for count, func in tests:
        with alloc_fail(hapd, count, func):
            if "FAIL" not in hapd.request(cmd):
                raise Exception("Set neighbor succeeded")

def test_rrm_neighbor_db_disabled(dev, apdev):
    """hostapd ctrl_iface SHOW_NEIGHBOR while neighbor report disabled"""
    params = {"ssid": "test"}
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)
    if "FAIL" not in hapd.request("SHOW_NEIGHBOR"):
        raise Exception("SHOW_NEIGHBOR accepted")

def test_rrm_neighbor_rep_req(dev, apdev):
    """wpa_supplicant ctrl_iface NEIGHBOR_REP_REQUEST"""
    check_rrm_support(dev[0])

    nr1 = "00112233445500000000510107"
    nr2 = "00112233445600000000510107"
    nr3 = "dd112233445500000000510107"

    params = {"ssid": "test"}
    hostapd.add_ap(apdev[0]['ifname'], params)
    params = {"ssid": "test2", "rrm_neighbor_report": "1"}
    hapd = hostapd.add_ap(apdev[1]['ifname'], params)

    bssid1 = apdev[1]['bssid']

    dev[0].connect("test", key_mgmt="NONE", scan_freq="2412")
    if "FAIL" not in dev[0].request("NEIGHBOR_REP_REQUEST"):
        raise Exception("Request succeeded unexpectedly (AP without RRM)")
    if "FAIL" not in dev[0].request("NEIGHBOR_REP_REQUEST ssid=\"abcdef\""):
        raise Exception("Request succeeded unexpectedly (AP without RRM 2)")
    dev[0].request("DISCONNECT")

    dev[0].connect("test2", key_mgmt="NONE", scan_freq="2412")

    if "OK" not in dev[0].request("NEIGHBOR_REP_REQUEST"):
        raise Exception("Request failed")
    check_nr_results(dev[0], [bssid1])

    if "OK" not in dev[0].request("NEIGHBOR_REP_REQUEST lci"):
        raise Exception("Request failed")
    check_nr_results(dev[0], [bssid1])

    if "OK" not in dev[0].request("NEIGHBOR_REP_REQUEST lci civic"):
        raise Exception("Request failed")
    check_nr_results(dev[0], [bssid1])

    if "OK" not in dev[0].request("NEIGHBOR_REP_REQUEST ssid=\"test3\""):
        raise Exception("Request failed")
    check_nr_results(dev[0])

    if "OK" not in dev[0].request("NEIGHBOR_REP_REQUEST ssid=\"test3\" lci civic"):
        raise Exception("Request failed")
    check_nr_results(dev[0])

    if "OK" not in hapd.request("SET_NEIGHBOR 00:11:22:33:44:55 ssid=\"test3\" nr=" + nr1 + " lci=" + lci + " civic=" + civic):
        raise Exception("Set neighbor failed")
    if "OK" not in hapd.request("SET_NEIGHBOR 00:11:22:33:44:56 ssid=\"test3\" nr=" + nr2 + " lci=" + lci + " civic=" + civic):
        raise Exception("Set neighbor failed")
    if "OK" not in hapd.request("SET_NEIGHBOR 00:11:22:33:44:56 ssid=\"test4\" nr=" + nr2 + " lci=" + lci + " civic=" + civic):
        raise Exception("Set neighbor failed")
    if "OK" not in hapd.request("SET_NEIGHBOR dd:11:22:33:44:55 ssid=\"test5\" nr=" + nr3 + " lci=" + lci):
        raise Exception("Set neighbor failed")

    if "OK" not in dev[0].request("NEIGHBOR_REP_REQUEST ssid=\"test3\""):
        raise Exception("Request failed")
    check_nr_results(dev[0], ["00:11:22:33:44:55", "00:11:22:33:44:56"])

    if "OK" not in dev[0].request("NEIGHBOR_REP_REQUEST ssid=\"test3\" lci"):
        raise Exception("Request failed")
    check_nr_results(dev[0], ["00:11:22:33:44:55", "00:11:22:33:44:56"],
                     lci=True)

    if "OK" not in dev[0].request("NEIGHBOR_REP_REQUEST ssid=\"test3\" civic"):
        raise Exception("Request failed")
    check_nr_results(dev[0], ["00:11:22:33:44:55", "00:11:22:33:44:56"],
                     civic=True)

    if "OK" not in dev[0].request("NEIGHBOR_REP_REQUEST ssid=\"test3\" lci civic"):
        raise Exception("Request failed")
    check_nr_results(dev[0], ["00:11:22:33:44:55", "00:11:22:33:44:56"],
                     lci=True, civic=True)

    if "OK" not in dev[0].request("NEIGHBOR_REP_REQUEST ssid=\"test4\""):
        raise Exception("Request failed")
    check_nr_results(dev[0], ["00:11:22:33:44:56"])

    if "OK" not in dev[0].request("NEIGHBOR_REP_REQUEST ssid=\"test4\" lci"):
        raise Exception("Request failed")
    check_nr_results(dev[0], ["00:11:22:33:44:56"], lci=True)

    if "OK" not in dev[0].request("NEIGHBOR_REP_REQUEST ssid=\"test4\" civic"):
        raise Exception("Request failed")
    check_nr_results(dev[0], ["00:11:22:33:44:56"], civic=True)

    if "OK" not in dev[0].request("NEIGHBOR_REP_REQUEST ssid=\"test4\" lci civic"):
        raise Exception("Request failed")
    check_nr_results(dev[0], ["00:11:22:33:44:56"], lci=True, civic=True)

    if "OK" not in dev[0].request("NEIGHBOR_REP_REQUEST ssid=\"test5\""):
        raise Exception("Request failed")
    check_nr_results(dev[0], ["dd:11:22:33:44:55"])

    if "OK" not in dev[0].request("NEIGHBOR_REP_REQUEST ssid=\"test5\" lci"):
        raise Exception("Request failed")
    check_nr_results(dev[0], ["dd:11:22:33:44:55"], lci=True)

    if "OK" not in dev[0].request("NEIGHBOR_REP_REQUEST ssid=\"test5\" civic"):
        raise Exception("Request failed")
    check_nr_results(dev[0], ["dd:11:22:33:44:55"])

    if "OK" not in dev[0].request("NEIGHBOR_REP_REQUEST ssid=\"test5\" lci civic"):
        raise Exception("Request failed")
    check_nr_results(dev[0], ["dd:11:22:33:44:55"], lci=True)

def test_rrm_neighbor_rep_oom(dev, apdev):
    """hostapd neighbor report OOM"""
    check_rrm_support(dev[0])

    nr1 = "00112233445500000000510107"
    nr2 = "00112233445600000000510107"
    nr3 = "dd112233445500000000510107"

    params = {"ssid": "test", "rrm_neighbor_report": "1"}
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    dev[0].connect("test", key_mgmt="NONE", scan_freq="2412")

    with alloc_fail(hapd, 1, "hostapd_send_nei_report_resp"):
        if "OK" not in dev[0].request("NEIGHBOR_REP_REQUEST"):
            raise Exception("Request failed")
        ev = dev[0].wait_event(["RRM-NEIGHBOR-REP-REQUEST-FAILED"], timeout=5)
        if ev is None:
            raise Exception("Neighbor report failure not reported")

def test_rrm_lci_req(dev, apdev):
    """hostapd lci request"""
    check_rrm_support(dev[0])

    params = {"ssid": "rrm", "rrm_neighbor_report": "1"}
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    # station not specified
    if "FAIL" not in hapd.request("REQ_LCI "):
        raise Exception("REQ_LCI with no station succeeded unexpectedly")

    # station that is not connected specified
    if "FAIL" not in hapd.request("REQ_LCI " + dev[0].own_addr()):
        raise Exception("REQ_LCI succeeded unexpectedly (station not connected)")

    dev[0].request("SET LCI ")
    dev[0].connect("rrm", key_mgmt="NONE", scan_freq="2412")

    # station connected without LCI
    if "FAIL" not in hapd.request("REQ_LCI " + dev[0].own_addr()):
        raise Exception("REQ_LCI succeeded unexpectedly (station without lci)")

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected(timeout=2)

    dev[0].request("SET LCI " + lci)

    dev[0].connect("rrm", key_mgmt="NONE", scan_freq="2412")

    # station connected with LCI
    if "OK" not in hapd.request("REQ_LCI " + dev[0].own_addr()):
        raise Exception("REQ_LCI failed unexpectedly")

def test_rrm_lci_req_timeout(dev, apdev):
    """hostapd lci request timeout"""
    check_rrm_support(dev[0])

    params = {"ssid": "rrm", "rrm_neighbor_report": "1"}
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    dev[0].request("SET LCI " + lci)
    dev[0].connect("rrm", key_mgmt="NONE", scan_freq="2412")
    addr = dev[0].own_addr()

    hapd.set("ext_mgmt_frame_handling", "1")
    if "OK" not in hapd.request("REQ_LCI " + addr):
        raise Exception("REQ_LCI failed unexpectedly")
    ev = hapd.wait_event(["MGMT-RX"], timeout=5)
    if ev is None:
        raise Exception("No response seen at the AP")
    # Ignore response and wait for HOSTAPD_RRM_REQUEST_TIMEOUT
    time.sleep(5.1)
    # Process response after timeout
    if "OK" not in hapd.request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=%s" % ev.split(' ')[1]):
        raise Exception("MGMT_RX_PROCESS failed")
    for i in range(257):
        if "OK" not in hapd.request("REQ_LCI " + addr):
            raise Exception("REQ_LCI failed unexpectedly")
        dev[0].dump_monitor()
        hapd.dump_monitor()
    hapd.set("ext_mgmt_frame_handling", "0")
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

def test_rrm_lci_req_oom(dev, apdev):
    """LCI report generation OOM"""
    check_rrm_support(dev[0])

    params = {"ssid": "rrm", "rrm_neighbor_report": "1"}
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    dev[0].request("SET LCI " + lci)
    dev[0].connect("rrm", key_mgmt="NONE", scan_freq="2412")

    with alloc_fail(dev[0], 1, "wpabuf_resize;wpas_rrm_build_lci_report"):
        if "OK" not in hapd.request("REQ_LCI " + dev[0].own_addr()):
            raise Exception("REQ_LCI failed unexpectedly")
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")

    dev[0].request("SET LCI ")
    # This in in wpas_rrm_build_lci_report(), but backtrace may not always work
    # for the "reject" label there.
    with alloc_fail(dev[0], 1, "wpabuf_resize;wpas_rrm_handle_msr_req_element"):
        if "OK" not in hapd.request("REQ_LCI " + dev[0].own_addr()):
            raise Exception("REQ_LCI failed unexpectedly")
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")

def test_rrm_lci_req_ap_oom(dev, apdev):
    """LCI report generation AP OOM and failure"""
    check_rrm_support(dev[0])

    params = {"ssid": "rrm", "rrm_neighbor_report": "1"}
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    dev[0].request("SET LCI " + lci)
    dev[0].connect("rrm", key_mgmt="NONE", scan_freq="2412")

    with alloc_fail(hapd, 1, "wpabuf_alloc;hostapd_send_lci_req"):
        if "FAIL" not in hapd.request("REQ_LCI " + dev[0].own_addr()):
            raise Exception("REQ_LCI succeeded during OOM")

    with fail_test(hapd, 1, "nl80211_send_frame_cmd;hostapd_send_lci_req"):
        if "FAIL" not in hapd.request("REQ_LCI " + dev[0].own_addr()):
            raise Exception("REQ_LCI succeeded during failure testing")

def test_rrm_lci_req_get_reltime_failure(dev, apdev):
    """LCI report generation and os_get_reltime() failure"""
    check_rrm_support(dev[0])

    params = {"ssid": "rrm", "rrm_neighbor_report": "1"}
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    dev[0].request("SET LCI " + lci)
    dev[0].connect("rrm", key_mgmt="NONE", scan_freq="2412")

    with fail_test(dev[0], 1, "os_get_reltime;wpas_rrm_build_lci_report"):
        if "OK" not in hapd.request("REQ_LCI " + dev[0].own_addr()):
            raise Exception("REQ_LCI failed unexpectedly")
        wait_fail_trigger(dev[0], "GET_FAIL")

def test_rrm_neighbor_rep_req_from_conf(dev, apdev):
    """wpa_supplicant ctrl_iface NEIGHBOR_REP_REQUEST and hostapd config"""
    check_rrm_support(dev[0])

    params = {"ssid": "test2", "rrm_neighbor_report": "1",
              "stationary_ap": "1", "lci": lci, "civic": civic}
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    bssid = apdev[0]['bssid']

    dev[0].connect("test2", key_mgmt="NONE", scan_freq="2412")

    if "OK" not in dev[0].request("NEIGHBOR_REP_REQUEST"):
        raise Exception("Request failed")
    check_nr_results(dev[0], [bssid])

def test_rrm_neighbor_rep_req_timeout(dev, apdev):
    """wpa_supplicant behavior on NEIGHBOR_REP_REQUEST response timeout"""
    check_rrm_support(dev[0])

    params = {"ssid": "test2", "rrm_neighbor_report": "1",
              "stationary_ap": "1", "lci": lci, "civic": civic}
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    dev[0].connect("test2", key_mgmt="NONE", scan_freq="2412")

    hapd.set("ext_mgmt_frame_handling", "1")

    if "OK" not in dev[0].request("NEIGHBOR_REP_REQUEST"):
        raise Exception("Request failed")
    msg = hapd.mgmt_rx()
    if msg is None:
        raise Exception("Neighbor report request not seen")
    check_nr_results(dev[0])

def test_rrm_neighbor_rep_req_oom(dev, apdev):
    """wpa_supplicant ctrl_iface NEIGHBOR_REP_REQUEST OOM"""
    check_rrm_support(dev[0])

    params = {"ssid": "test2", "rrm_neighbor_report": "1",
              "stationary_ap": "1", "lci": lci, "civic": civic}
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    dev[0].connect("test2", key_mgmt="NONE", scan_freq="2412")

    with alloc_fail(dev[0], 1, "wpabuf_alloc;wpas_rrm_process_neighbor_rep"):
        if "OK" not in dev[0].request("NEIGHBOR_REP_REQUEST"):
            raise Exception("Request failed")
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")

    with fail_test(dev[0], 1,
                    "wpa_driver_nl80211_send_action;wpas_rrm_send_neighbor_rep_request"):
        if "FAIL" not in dev[0].request("NEIGHBOR_REP_REQUEST"):
            raise Exception("Request succeeded unexpectedly")

    with alloc_fail(dev[0], 1,
                    "wpabuf_alloc;wpas_rrm_send_neighbor_rep_request"):
        if "FAIL" not in dev[0].request("NEIGHBOR_REP_REQUEST"):
            raise Exception("Request succeeded unexpectedly")

def test_rrm_neighbor_rep_req_disconnect(dev, apdev):
    """wpa_supplicant behavior on disconnection during NEIGHBOR_REP_REQUEST"""
    check_rrm_support(dev[0])

    params = {"ssid": "test2", "rrm_neighbor_report": "1",
              "stationary_ap": "1", "lci": lci, "civic": civic}
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    if "FAIL" not in dev[0].request("NEIGHBOR_REP_REQUEST"):
        raise Exception("Request accepted while disconnected")

    dev[0].connect("test2", key_mgmt="NONE", scan_freq="2412")

    hapd.set("ext_mgmt_frame_handling", "1")

    if "OK" not in dev[0].request("NEIGHBOR_REP_REQUEST"):
        raise Exception("Request failed")
    msg = hapd.mgmt_rx()
    if msg is None:
        raise Exception("Neighbor report request not seen")
    dev[0].request("DISCONNECT")
    check_nr_results(dev[0])

def test_rrm_neighbor_rep_req_not_supported(dev, apdev):
    """NEIGHBOR_REP_REQUEST for AP not supporting neighbor report"""
    check_rrm_support(dev[0])

    params = {"ssid": "test2", "rrm_beacon_report": "1"}
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    dev[0].connect("test2", key_mgmt="NONE", scan_freq="2412")

    if "FAIL" not in dev[0].request("NEIGHBOR_REP_REQUEST"):
        raise Exception("Request accepted unexpectedly")

def test_rrm_neighbor_rep_req_busy(dev, apdev):
    """wpa_supplicant and concurrent NEIGHBOR_REP_REQUEST commands"""
    check_rrm_support(dev[0])

    params = {"ssid": "test2", "rrm_neighbor_report": "1",
              "stationary_ap": "1", "lci": lci, "civic": civic}
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    dev[0].connect("test2", key_mgmt="NONE", scan_freq="2412")

    hapd.set("ext_mgmt_frame_handling", "1")

    if "OK" not in dev[0].request("NEIGHBOR_REP_REQUEST"):
        raise Exception("Request failed")
    msg = hapd.mgmt_rx()
    if msg is None:
        raise Exception("Neighbor report request not seen")

    if "FAIL" not in dev[0].request("NEIGHBOR_REP_REQUEST"):
        raise Exception("Request accepted while disconnected")

def test_rrm_ftm_range_req(dev, apdev):
    """hostapd FTM range request command"""
    check_rrm_support(dev[0])
    try:
        run_rrm_ftm_range_req(dev, apdev)
    finally:
        dev[1].request("VENDOR_ELEM_REMOVE 13 *")

def run_rrm_ftm_range_req(dev, apdev):
    params = {"ssid": "rrm", "rrm_neighbor_report": "1"}
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)
    bssid = hapd.own_addr()

    # station not specified
    if "FAIL" not in hapd.request("REQ_RANGE "):
        raise Exception("REQ_RANGE with no station succeeded unexpectedly")

    # station that is not connected specified
    if "FAIL" not in hapd.request("REQ_RANGE " + dev[0].own_addr()):
        raise Exception("REQ_RANGE succeeded unexpectedly (station not connected)")

    # No responders specified
    if "FAIL" not in hapd.request("REQ_RANGE " + dev[0].own_addr() + " 10 10"):
        raise Exception("REQ_RANGE succeeded unexpectedly (no responder)")

    # Bad responder address
    if "FAIL" not in hapd.request("REQ_RANGE " + dev[0].own_addr() + " 10 10 00:11:22:33:44:"):
        raise Exception("REQ_RANGE succeeded unexpectedly (bad responder address)")

    # Bad responder address
    if "FAIL" not in hapd.request("REQ_RANGE " + dev[0].own_addr() + " 10 10 00:11:22:33:44:55 00:11:22:33:44"):
        raise Exception("REQ_RANGE succeeded unexpectedly (bad responder address 2)")

    # Bad min_ap value
    if "FAIL" not in hapd.request("REQ_RANGE " + dev[0].own_addr() + " 10 300 00:11:22:33:44:55"):
        raise Exception("REQ_RANGE succeeded unexpectedly (invalid min_ap value)")

    # Bad rand value
    if "FAIL" not in hapd.request("REQ_RANGE " + dev[0].own_addr() + " -1 10 00:11:22:33:44:55"):
        raise Exception("REQ_RANGE succeeded unexpectedly (invalid rand value)")
    if "FAIL" not in hapd.request("REQ_RANGE " + dev[0].own_addr() + " 65536 10 00:11:22:33:44:55"):
        raise Exception("REQ_RANGE succeeded unexpectedly (invalid rand value)")

    # Missing min_ap value
    if "FAIL" not in hapd.request("REQ_RANGE " + dev[0].own_addr() + " 10"):
        raise Exception("REQ_RANGE succeeded unexpectedly (missing min_ap value)")

    # Too many responders
    if "FAIL" not in hapd.request("REQ_RANGE " + dev[0].own_addr() + " 10 10" + 20*" 00:11:22:33:44:55"):
        raise Exception("REQ_RANGE succeeded unexpectedly (too many responders)")
    # Wrong min AP count
    if "FAIL" not in hapd.request("REQ_RANGE " + dev[0].own_addr() + " 10 10 00:11:22:33:44:55"):
        raise Exception("REQ_RANGE succeeded unexpectedly (responder not in database)")

    dev[0].connect("rrm", key_mgmt="NONE", scan_freq="2412")
    # Override RM capabilities to include FTM range report
    dev[1].request("VENDOR_ELEM_ADD 13 46057100000004")
    dev[1].connect("rrm", key_mgmt="NONE", scan_freq="2412")

    # Request range: Destination address is not connected
    if "FAIL" not in hapd.request("REQ_RANGE 11:22:33:44:55:66 10 1 00:11:22:33:44:55"):
        raise Exception("REQ_RANGE succeeded unexpectedly (responder not in database)")

    # Responder not in database
    # Note: this check would pass since the station does not support FTM range
    # request and not because the responder is not in the database.
    if "FAIL" not in hapd.request("REQ_RANGE " + dev[0].own_addr() + " 10 1 00:11:22:33:44:55"):
        raise Exception("REQ_RANGE succeeded unexpectedly (responder not in database)")

    # Missing neighbor report for 00:11:22:33:44:55
    if "FAIL" not in hapd.request("REQ_RANGE " + dev[1].own_addr() + " 10 1 00:11:22:33:44:55"):
        raise Exception("REQ_RANGE succeeded unexpectedly (responder not in database)")

    # Send request
    if "OK" not in hapd.request("REQ_RANGE " + dev[1].own_addr() + " 10 1 " + bssid):
        raise Exception("REQ_RANGE failed unexpectedly")

    # Too long range request
    if "FAIL" not in hapd.request("REQ_RANGE " + dev[1].own_addr() + " 10 1" + 16*(" " + bssid)):
        raise Exception("REQ_RANGE accepted for too long range request")

    time.sleep(0.1)
    dev[0].request("DISCONNECT")
    dev[1].request("DISCONNECT")
    dev[1].wait_disconnected()

def test_rrm_ftm_range_req_timeout(dev, apdev):
    """hostapd FTM range request timeout"""
    check_rrm_support(dev[0])
    try:
        run_rrm_ftm_range_req_timeout(dev, apdev)
    finally:
        dev[1].request("VENDOR_ELEM_REMOVE 13 *")

def run_rrm_ftm_range_req_timeout(dev, apdev):
    params = {"ssid": "rrm", "rrm_neighbor_report": "1"}
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)
    bssid = hapd.own_addr()

    # Override RM capabilities to include FTM range report
    dev[1].request("VENDOR_ELEM_ADD 13 46057100000004")
    dev[1].connect("rrm", key_mgmt="NONE", scan_freq="2412")
    addr = dev[1].own_addr()

    hapd.set("ext_mgmt_frame_handling", "1")
    if "OK" not in hapd.request("REQ_RANGE " + addr + " 10 1 " + bssid):
        raise Exception("REQ_RANGE failed")
    ev = hapd.wait_event(["MGMT-RX"], timeout=5)
    if ev is None:
        raise Exception("No response seen at the AP")
    # Ignore response and wait for HOSTAPD_RRM_REQUEST_TIMEOUT
    time.sleep(5.1)
    # Process response after timeout
    if "OK" not in hapd.request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=%s" % ev.split(' ')[1]):
        raise Exception("MGMT_RX_PROCESS failed")

    for i in range(257):
        if "OK" not in hapd.request("REQ_RANGE " + addr + " 10 1 " + bssid):
            raise Exception("REQ_RANGE failed")
        dev[1].dump_monitor()
        hapd.dump_monitor()

    hapd.set("ext_mgmt_frame_handling", "0")
    dev[1].request("DISCONNECT")
    dev[1].wait_disconnected()

def test_rrm_ftm_range_req_failure(dev, apdev):
    """hostapd FTM range request failure"""
    check_rrm_support(dev[0])
    try:
        run_rrm_ftm_range_req_failure(dev, apdev)
    finally:
        dev[1].request("VENDOR_ELEM_REMOVE 13 *")

def run_rrm_ftm_range_req_failure(dev, apdev):
    params = {"ssid": "rrm", "rrm_neighbor_report": "1"}
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)
    bssid = hapd.own_addr()

    # Override RM capabilities to include FTM range report
    dev[1].request("VENDOR_ELEM_ADD 13 46057100000004")
    dev[1].connect("rrm", key_mgmt="NONE", scan_freq="2412")

    with alloc_fail(hapd, 1, "wpabuf_alloc;hostapd_send_range_req"):
        if "FAIL" not in hapd.request("REQ_RANGE " + dev[1].own_addr() + " 10 1 " + bssid):
            raise Exception("REQ_RANGE succeeded during OOM")

    with fail_test(hapd, 1, "nl80211_send_frame_cmd;hostapd_send_range_req"):
        if "FAIL" not in hapd.request("REQ_RANGE " + dev[1].own_addr() + " 10 1 " + bssid):
            raise Exception("REQ_RANGE succeeded during failure testing")

    dev[1].request("DISCONNECT")
    dev[1].wait_disconnected()

def test_rrm_ftm_capa_indication(dev, apdev):
    """FTM capability indication"""
    try:
        _test_rrm_ftm_capa_indication(dev, apdev)
    finally:
        dev[0].request("SET ftm_initiator 0")
        dev[0].request("SET ftm_responder 0")

def _test_rrm_ftm_capa_indication(dev, apdev):
    params = {"ssid": "ftm",
              "ftm_responder": "1",
              "ftm_initiator": "1",}
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    if "OK" not in dev[0].request("SET ftm_initiator 1"):
        raise Exception("could not set ftm_initiator")
    if "OK" not in dev[0].request("SET ftm_responder 1"):
        raise Exception("could not set ftm_responder")
    dev[0].scan_for_bss(apdev[0]['bssid'], freq=2412, force_scan=True)

class BeaconReport:
    def __init__(self, report):
        self.opclass, self.channel, self.start, self.duration, self.frame_info, self.rcpi, self.rsni = struct.unpack("<BBQHBBB", report[0:15])
        report = report[15:]
        self.bssid = report[0:6]
        self.bssid_str = "%02x:%02x:%02x:%02x:%02x:%02x" % (struct.unpack('6B', self.bssid))
        report = report[6:]
        self.antenna_id, self.parent_tsf = struct.unpack("<BI", report[0:5])
        report = report[5:]
        self.subelems = report
        self.frame_body = None
        self.frame_body_fragment_id = None
        self.last_indication = None
        while len(report) >= 2:
            eid, elen = struct.unpack('BB', report[0:2])
            report = report[2:]
            if len(report) < elen:
                raise Exception("Invalid subelement in beacon report")
            if eid == 1:
                # Reported Frame Body
                # Contents depends on the reporting detail request:
                # 0 = no Reported Frame Body subelement
                # 1 = all fixed fields and any elements identified in Request
                #     element
                # 2 = all fixed fields and all elements
                # Fixed fields: Timestamp[8] BeaconInt[2] CapabInfo[2]
                self.frame_body = report[0:elen]
            if eid == 2:
                self.frame_body_fragment_id = report[0:elen]
            if eid == 164:
                self.last_indication = report[0:elen]
            report = report[elen:]
    def __str__(self):
        txt = "opclass={} channel={} start={} duration={} frame_info={} rcpi={} rsni={} bssid={} antenna_id={} parent_tsf={}".format(self.opclass, self.channel, self.start, self.duration, self.frame_info, self.rcpi, self.rsni, self.bssid_str, self.antenna_id, self.parent_tsf)
        if self.frame_body:
            txt += " frame_body=" + binascii.hexlify(self.frame_body).decode()
        if self.frame_body_fragment_id:
            txt += " fragment_id=" + binascii.hexlify(self.frame_body_fragment_id).decode()
        if self.last_indication:
            txt += " last_indication=" + binascii.hexlify(self.last_indication).decode()

        return txt

def run_req_beacon(hapd, addr, request):
    token = hapd.request("REQ_BEACON " + addr + " " + request)
    if "FAIL" in token:
        raise Exception("REQ_BEACON failed")

    ev = hapd.wait_event(["BEACON-REQ-TX-STATUS"], timeout=5)
    if ev is None:
        raise Exception("No TX status event for beacon request received")
    fields = ev.split(' ')
    if fields[1] != addr:
        raise Exception("Unexpected STA address in TX status: " + fields[1])
    if fields[2] != token:
        raise Exception("Unexpected dialog token in TX status: " + fields[2] + " (expected " + token + ")")
    if fields[3] != "ack=1":
        raise Exception("Unexected ACK status in TX status: " + fields[3])
    return token

@remote_compatible
def test_rrm_beacon_req_table(dev, apdev):
    """Beacon request - beacon table mode"""
    params = {"ssid": "rrm", "rrm_beacon_report": "1"}
    hapd = hostapd.add_ap(apdev[0], params)
    hapd2 = hostapd.add_ap(apdev[1], {"ssid": "another"})

    tests = ["REQ_BEACON ",
             "REQ_BEACON q",
             "REQ_BEACON 11:22:33:44:55:66",
             "REQ_BEACON 11:22:33:44:55:66 req_mode=q",
             "REQ_BEACON 11:22:33:44:55:66 req_mode=11",
             "REQ_BEACON 11:22:33:44:55:66 1",
             "REQ_BEACON 11:22:33:44:55:66 1q",
             "REQ_BEACON 11:22:33:44:55:66 11223344556677889900aabbccddeeff"]
    for t in tests:
        if "FAIL" not in hapd.request(t):
            raise Exception("Invalid command accepted: " + t)

    dev[0].scan_for_bss(apdev[1]['bssid'], freq=2412)
    dev[0].connect("rrm", key_mgmt="NONE", scan_freq="2412")
    addr = dev[0].own_addr()

    token = run_req_beacon(hapd, addr, "51000000000002ffffffffffff")

    for i in range(1, 3):
        ev = hapd.wait_event(["BEACON-RESP-RX"], timeout=10)
        if ev is None:
            raise Exception("Beacon report %d response not received" % i)
        fields = ev.split(' ')
        if fields[1] != addr:
            raise Exception("Unexpected STA address in beacon report response: " + fields[1])
        if fields[2] != token:
            raise Exception("Unexpected dialog token in beacon report response: " + fields[2] + " (expected " + token + ")")
        if fields[3] != "00":
            raise Exception("Unexpected measurement report mode")

        report = BeaconReport(binascii.unhexlify(fields[4]))
        logger.info("Received beacon report: " + str(report))

        # Default reporting detail is 2, i.e., all fixed fields and elements.
        if not report.frame_body:
            raise Exception("Reported Frame Body subelement missing")
        if len(report.frame_body) <= 12:
            raise Exception("Too short Reported Frame Body subelement")

def test_rrm_beacon_req_frame_body_fragmentation(dev, apdev):
    """Beacon request - beacon table mode - frame body fragmentation"""
    params = {"ssid": "rrm", "rrm_beacon_report": "1"}

    hapd = hostapd.add_ap(apdev[0], params)
    hapd.set('vendor_elements', ("dd051122330203dd0400137400dd04001374ffdd0511"
              "22330203dd0400137400dd04001374ffdd051122330203dd0400137400dd04001"
              "374ffdd051122330203dd0400137400dd04001374ffdd051122330203dd040013"
              "7400dd04001374ffdd051122330203dd0400137400dd04001374ffdd051122330"
              "203dd0400137400dd04001374ffdd051122330203dd0400137400dd04001374ff"
              "dd051122330203dd0400137400dd04001374ff"))

    dev[0].connect("rrm", key_mgmt="NONE", scan_freq="2412")
    addr = dev[0].own_addr()

    token = run_req_beacon(hapd, addr, "51000000000002ffffffffffff")

    # 2 beacon reports elements are expected because of fragmentation
    for i in range(0, 2):
        ev = hapd.wait_event(["BEACON-RESP-RX"], timeout=10)
        if ev is None:
            raise Exception("Beacon report %d response not received" % i)
        fields = ev.split(' ')
        if fields[1] != addr:
            raise Exception("Unexpected STA address in beacon report response: " + fields[1])
        if fields[2] != token:
            raise Exception("Unexpected dialog token in beacon report response: " + fields[2] + " (expected " + token + ")")
        if fields[3] != "00":
            raise Exception("Unexpected measurement report mode")

        report = BeaconReport(binascii.unhexlify(fields[4]))
        logger.info("Received beacon report: " + str(report))

        # Default reporting detail is 2, i.e., all fixed fields and elements.
        if not report.frame_body_fragment_id:
            raise Exception("Reported Frame Body Fragment ID subelement missing")
        fragment_id = binascii.hexlify(report.frame_body_fragment_id)
        frag_number = int(fragment_id[2:], 16) & int(0x7f)
        if frag_number != i:
            raise Exception("Incorrect fragment number: %d" % frag_number)
        more_frags = int(fragment_id[2:], 16) >> 7
        if i == 0 and more_frags != 1:
            raise Exception("more fragments bit is not set on first fragment")
        if i == 1 and more_frags != 0:
            raise Exception("more fragments bit is set on last fragment")

def test_rrm_beacon_req_last_frame_indication(dev, apdev):
    """Beacon request - beacon table mode - last frame indication"""
    params = {"ssid": "rrm", "rrm_beacon_report": "1"}

    hapd = hostapd.add_ap(apdev[0], params)
    hapd2 = hostapd.add_ap(apdev[1], {"ssid": "another"})

    dev[0].connect("rrm", key_mgmt="NONE", scan_freq="2412")
    addr = dev[0].own_addr()

    # The request contains the last beacon report indication subelement
    token = run_req_beacon(hapd, addr, "51000000000002ffffffffffffa40101")

    for i in range(1, 3):
        ev = hapd.wait_event(["BEACON-RESP-RX"], timeout=10)
        if ev is None:
            raise Exception("Beacon report %d response not received" % i)
        fields = ev.split(' ')
        if fields[1] != addr:
            raise Exception("Unexpected STA address in beacon report response: " + fields[1])
        if fields[2] != token:
            raise Exception("Unexpected dialog token in beacon report response: " + fields[2] + " (expected " + token + ")")
        if fields[3] != "00":
            raise Exception("Unexpected measurement report mode")

        report = BeaconReport(binascii.unhexlify(fields[4]))
        logger.info("Received beacon report: " + str(report))

        if not report.last_indication:
            raise Exception("Last Beacon Report Indication subelement missing")

        last = binascii.hexlify(report.last_indication).decode()
        if (i == 2 and last != '01') or (i != 2 and last != '00'):
            raise Exception("last beacon report indication is not set on last frame")

    # The request does not contain the last beacon report indication subelement
    token = run_req_beacon(hapd, addr, "51000000000002ffffffffffff")

    for i in range(1, 3):
        ev = hapd.wait_event(["BEACON-RESP-RX"], timeout=10)
        if ev is None:
            raise Exception("Beacon report %d response not received" % i)
        fields = ev.split(' ')
        if fields[1] != addr:
            raise Exception("Unexpected STA address in beacon report response: " + fields[1])
        if fields[2] != token:
            raise Exception("Unexpected dialog token in beacon report response: " + fields[2] + " (expected " + token + ")")
        if fields[3] != "00":
            raise Exception("Unexpected measurement report mode")

        report = BeaconReport(binascii.unhexlify(fields[4]))
        logger.info("Received beacon report: " + str(report))

        if report.last_indication:
            raise Exception("Last Beacon Report Indication subelement present but not requested")

@remote_compatible
def test_rrm_beacon_req_table_detail(dev, apdev):
    """Beacon request - beacon table mode - reporting detail"""
    params = {"ssid": "rrm", "rrm_beacon_report": "1"}
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].connect("rrm", key_mgmt="NONE", scan_freq="2412")
    addr = dev[0].own_addr()

    logger.info("Reporting Detail 0")
    token = run_req_beacon(hapd, addr, "51000000000002ffffffffffff" + "020100")
    ev = hapd.wait_event(["BEACON-RESP-RX"], timeout=10)
    if ev is None:
        raise Exception("Beacon report response not received")
    fields = ev.split(' ')
    report = BeaconReport(binascii.unhexlify(fields[4]))
    logger.info("Received beacon report: " + str(report))
    if report.frame_body:
        raise Exception("Reported Frame Body subelement included with Reporting Detail 0")
    hapd.dump_monitor()

    logger.info("Reporting Detail 1")
    token = run_req_beacon(hapd, addr, "51000000000002ffffffffffff" + "020101")
    ev = hapd.wait_event(["BEACON-RESP-RX"], timeout=10)
    if ev is None:
        raise Exception("Beacon report response not received")
    fields = ev.split(' ')
    report = BeaconReport(binascii.unhexlify(fields[4]))
    logger.info("Received beacon report: " + str(report))
    if not report.frame_body:
        raise Exception("Reported Frame Body subelement missing")
    if len(report.frame_body) != 12:
        raise Exception("Unexpected Reported Frame Body subelement length with Reporting Detail 1")
    hapd.dump_monitor()

    logger.info("Reporting Detail 2")
    token = run_req_beacon(hapd, addr, "51000000000002ffffffffffff" + "020102")
    ev = hapd.wait_event(["BEACON-RESP-RX"], timeout=10)
    if ev is None:
        raise Exception("Beacon report response not received")
    fields = ev.split(' ')
    report = BeaconReport(binascii.unhexlify(fields[4]))
    logger.info("Received beacon report: " + str(report))
    if not report.frame_body:
        raise Exception("Reported Frame Body subelement missing")
    if len(report.frame_body) <= 12:
        raise Exception("Unexpected Reported Frame Body subelement length with Reporting Detail 2")
    hapd.dump_monitor()

    logger.info("Reporting Detail 3 (invalid)")
    token = run_req_beacon(hapd, addr, "51000000000002ffffffffffff" + "020103")
    ev = hapd.wait_event(["BEACON-RESP-RX"], timeout=0.2)
    if ev is not None:
        raise Exception("Unexpected beacon report response to invalid reporting detail 3")
    hapd.dump_monitor()

    logger.info("Reporting Detail (too short)")
    token = run_req_beacon(hapd, addr, "51000000000002ffffffffffff" + "0200")
    ev = hapd.wait_event(["BEACON-RESP-RX"], timeout=0.2)
    if ev is not None:
        raise Exception("Unexpected beacon report response to invalid reporting detail")
    hapd.dump_monitor()

@remote_compatible
def test_rrm_beacon_req_table_request(dev, apdev):
    """Beacon request - beacon table mode - request element"""
    params = {"ssid": "rrm", "rrm_beacon_report": "1"}
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].flush_scan_cache()
    dev[0].connect("rrm", key_mgmt="NONE", scan_freq="2412")
    addr = dev[0].own_addr()

    token = run_req_beacon(hapd, addr, "51000000000002ffffffffffff" + "020101" + "0a03000106")
    ev = hapd.wait_event(["BEACON-RESP-RX"], timeout=10)
    if ev is None:
        raise Exception("Beacon report response not received")
    fields = ev.split(' ')
    report = BeaconReport(binascii.unhexlify(fields[4]))
    logger.info("Received beacon report: " + str(report))
    if not report.frame_body:
        raise Exception("Reported Frame Body subelement missing")
    if len(report.frame_body) != 12 + 5 + 10:
        raise Exception("Unexpected Reported Frame Body subelement length with Reporting Detail 1 and requested elements SSID + SuppRates")
    hapd.dump_monitor()

    logger.info("Incorrect reporting detail with request subelement")
    token = run_req_beacon(hapd, addr, "51000000000002ffffffffffff" + "020102" + "0a03000106")
    ev = hapd.wait_event(["BEACON-RESP-RX"], timeout=0.2)
    if ev is not None:
        raise Exception("Unexpected beacon report response (invalid reporting detail)")
    hapd.dump_monitor()

    logger.info("Invalid request subelement length")
    token = run_req_beacon(hapd, addr, "51000000000002ffffffffffff" + "020101" + "0a00")
    ev = hapd.wait_event(["BEACON-RESP-RX"], timeout=0.2)
    if ev is not None:
        raise Exception("Unexpected beacon report response (invalid request subelement length)")
    hapd.dump_monitor()

    logger.info("Multiple request subelements")
    token = run_req_beacon(hapd, addr, "51000000000002ffffffffffff" + "020101" + "0a0100" + "0a0101")
    ev = hapd.wait_event(["BEACON-RESP-RX"], timeout=0.2)
    if ev is not None:
        raise Exception("Unexpected beacon report response (multiple request subelements)")
    hapd.dump_monitor()

@remote_compatible
def test_rrm_beacon_req_table_request_oom(dev, apdev):
    """Beacon request - beacon table mode - request element OOM"""
    params = {"ssid": "rrm", "rrm_beacon_report": "1"}
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].connect("rrm", key_mgmt="NONE", scan_freq="2412")
    addr = dev[0].own_addr()

    with alloc_fail(dev[0], 1,
                    "bitfield_alloc;wpas_rm_handle_beacon_req_subelem"):
        token = run_req_beacon(hapd, addr, "51000000000002ffffffffffff" + "020101" + "0a03000106")
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
        ev = hapd.wait_event(["BEACON-RESP-RX"], timeout=0.1)
        if ev is not None:
            raise Exception("Unexpected beacon report response received (OOM)")

    with alloc_fail(dev[0], 1,
                    "wpabuf_alloc;wpas_rrm_send_msr_report_mpdu"):
        token = run_req_beacon(hapd, addr, "51000000000002ffffffffffff" + "020101" + "0a03000106")
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
        ev = hapd.wait_event(["BEACON-RESP-RX"], timeout=0.1)
        if ev is not None:
            raise Exception("Unexpected beacon report response received (OOM)")

    with fail_test(dev[0], 1,
                    "wpa_driver_nl80211_send_action;wpas_rrm_send_msr_report_mpdu"):
        token = run_req_beacon(hapd, addr, "51000000000002ffffffffffff" + "020101" + "0a03000106")
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
        ev = hapd.wait_event(["BEACON-RESP-RX"], timeout=0.1)
        if ev is not None:
            raise Exception("Unexpected beacon report response received (OOM)")

    with alloc_fail(dev[0], 1,
                    "wpabuf_resize;wpas_add_beacon_rep"):
        token = run_req_beacon(hapd, addr, "51000000000002ffffffffffff" + "020101" + "0a03000106")
        ev = hapd.wait_event(["BEACON-RESP-RX"], timeout=10)
        if ev is None:
            raise Exception("Beacon report response not received (OOM -> empty report)")
        fields = ev.split(' ')
        if len(fields[4]) > 0:
            raise Exception("Unexpected beacon report received")

@remote_compatible
def test_rrm_beacon_req_table_bssid(dev, apdev):
    """Beacon request - beacon table mode - specific BSSID"""
    params = {"ssid": "rrm", "rrm_beacon_report": "1"}
    hapd = hostapd.add_ap(apdev[0], params)
    hapd2 = hostapd.add_ap(apdev[1], {"ssid": "another"})

    dev[0].connect("rrm", key_mgmt="NONE", scan_freq="2412")
    addr = dev[0].own_addr()

    bssid2 = hapd2.own_addr()
    token = run_req_beacon(hapd, addr, "51000000000002" + bssid2.replace(':', ''))
    ev = hapd.wait_event(["BEACON-RESP-RX"], timeout=10)
    if ev is None:
        raise Exception("Beacon report response not received")
    fields = ev.split(' ')
    report = BeaconReport(binascii.unhexlify(fields[4]))
    logger.info("Received beacon report: " + str(report))
    if "bssid=" + bssid2 not in str(report):
        raise Exception("Report for unexpected BSS")
    ev = hapd.wait_event(["BEACON-RESP-RX"], timeout=0.1)
    if ev is not None:
        raise Exception("Unexpected beacon report response")

@remote_compatible
def test_rrm_beacon_req_table_ssid(dev, apdev):
    """Beacon request - beacon table mode - specific SSID"""
    params = {"ssid": "rrm", "rrm_beacon_report": "1"}
    hapd = hostapd.add_ap(apdev[0], params)
    hapd2 = hostapd.add_ap(apdev[1], {"ssid": "another"})

    dev[0].connect("rrm", key_mgmt="NONE", scan_freq="2412")
    addr = dev[0].own_addr()

    bssid2 = hapd2.own_addr()
    token = run_req_beacon(hapd, addr, "51000000000002ffffffffffff" + "0007" + binascii.hexlify(b"another").decode())
    ev = hapd.wait_event(["BEACON-RESP-RX"], timeout=10)
    if ev is None:
        raise Exception("Beacon report response not received")
    fields = ev.split(' ')
    report = BeaconReport(binascii.unhexlify(fields[4]))
    logger.info("Received beacon report: " + str(report))
    if "bssid=" + bssid2 not in str(report):
        raise Exception("Report for unexpected BSS")
    ev = hapd.wait_event(["BEACON-RESP-RX"], timeout=0.1)
    if ev is not None:
        raise Exception("Unexpected beacon report response")
    hapd.dump_monitor()

    logger.info("Wildcard SSID")
    token = run_req_beacon(hapd, addr, "51000000000002ffffffffffff" + "0000")
    for i in range(2):
        ev = hapd.wait_event(["BEACON-RESP-RX"], timeout=10)
        if ev is None:
            raise Exception("Beacon report response not received")
        fields = ev.split(' ')
        report = BeaconReport(binascii.unhexlify(fields[4]))
        logger.info("Received beacon report: " + str(report))
    hapd.dump_monitor()

    logger.info("Too long SSID")
    token = run_req_beacon(hapd, addr, "51000000000002ffffffffffff" + "0021" + 33*"00")
    ev = hapd.wait_event(["BEACON-RESP-RX"], timeout=0.2)
    if ev is not None:
        raise Exception("Unexpected beacon report response (invalid SSID subelement in request)")
    hapd.dump_monitor()

@remote_compatible
def test_rrm_beacon_req_table_info(dev, apdev):
    """Beacon request - beacon table mode - Reporting Information subelement"""
    params = {"ssid": "rrm", "rrm_beacon_report": "1"}
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].connect("rrm", key_mgmt="NONE", scan_freq="2412")
    addr = dev[0].own_addr()

    logger.info("Unsupported reporting information 1")
    token = run_req_beacon(hapd, addr, "51000000000002ffffffffffff" + "01020100")
    ev = hapd.wait_event(["BEACON-RESP-RX"], timeout=10)
    if ev is None:
        raise Exception("Beacon report response (incapable) is not received")

    fields = ev.split(' ')
    if fields[3] != "02":
        raise Exception("Beacon report response - unexpected mode (" + fields[3] + ")")
    hapd.dump_monitor()

    logger.info("Invalid reporting information length")
    token = run_req_beacon(hapd, addr, "51000000000002ffffffffffff" + "010100")
    ev = hapd.wait_event(["BEACON-RESP-RX"], timeout=0.2)
    if ev is not None:
        raise Exception("Unexpected beacon report response (invalid reporting information length)")
    hapd.dump_monitor()

@remote_compatible
def test_rrm_beacon_req_table_unknown_subelem(dev, apdev):
    """Beacon request - beacon table mode - unknown subelement"""
    params = {"ssid": "rrm", "rrm_beacon_report": "1"}
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].connect("rrm", key_mgmt="NONE", scan_freq="2412")
    addr = dev[0].own_addr()

    token = run_req_beacon(hapd, addr, "51000000000002ffffffffffff" + "330101" + "fe00")
    ev = hapd.wait_event(["BEACON-RESP-RX"], timeout=10)
    if ev is None:
        raise Exception("Beacon report response not received")
    fields = ev.split(' ')
    report = BeaconReport(binascii.unhexlify(fields[4]))
    logger.info("Received beacon report: " + str(report))

@remote_compatible
def test_rrm_beacon_req_table_truncated_subelem(dev, apdev):
    """Beacon request - beacon table mode - Truncated subelement"""
    params = {"ssid": "rrm", "rrm_beacon_report": "1"}
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].connect("rrm", key_mgmt="NONE", scan_freq="2412")
    addr = dev[0].own_addr()

    token = run_req_beacon(hapd, addr, "51000000000002ffffffffffff" + "0001")
    ev = hapd.wait_event(["BEACON-RESP-RX"], timeout=0.2)
    if ev is not None:
        raise Exception("Unexpected beacon report response (truncated subelement)")
    hapd.dump_monitor()

@remote_compatible
def test_rrm_beacon_req_table_rsne(dev, apdev):
    """Beacon request - beacon table mode - RSNE reporting"""
    params = hostapd.wpa2_params(ssid="rrm-rsn", passphrase="12345678")
    params["rrm_beacon_report"] = "1"
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].connect("rrm-rsn", psk="12345678", scan_freq="2412")
    addr = dev[0].own_addr()

    token = run_req_beacon(hapd, addr, "51000000000002ffffffffffff" + "020101" + "0a0130")
    ev = hapd.wait_event(["BEACON-RESP-RX"], timeout=10)
    if ev is None:
        raise Exception("Beacon report response not received")
    fields = ev.split(' ')
    report = BeaconReport(binascii.unhexlify(fields[4]))
    logger.info("Received beacon report: " + str(report))
    if not report.frame_body:
        raise Exception("Reported Frame Body subelement missing")
    if len(report.frame_body) != 12 + 22:
        raise Exception("Unexpected Reported Frame Body subelement length with Reporting Detail 1 and requested element RSNE")
    if binascii.unhexlify("30140100000fac040100000fac040100000fac020c00") not in report.frame_body:
        raise Exception("Full RSNE not found")

def test_rrm_beacon_req_table_vht(dev, apdev):
    """Beacon request - beacon table mode - VHT"""
    clear_scan_cache(apdev[0])
    try:
        hapd = None
        params = {"ssid": "rrm-vht",
                  "country_code": "FI",
                  "hw_mode": "a",
                  "channel": "36",
                  "ht_capab": "[HT40+]",
                  "ieee80211n": "1",
                  "ieee80211ac": "1",
                  "vht_oper_chwidth": "1",
                  "vht_oper_centr_freq_seg0_idx": "42",
                  "rrm_beacon_report": "1"}
        hapd = hostapd.add_ap(apdev[0], params)

        params = {"ssid": "test-vht40",
                  "country_code": "FI",
                  "hw_mode": "a",
                  "channel": "48",
                  "ieee80211n": "1",
                  "ieee80211ac": "1",
                  "ht_capab": "[HT40-]",
                  "vht_capab": "",
                  "vht_oper_chwidth": "0",
                  "vht_oper_centr_freq_seg0_idx": "0",
                }
        hapd2 = hostapd.add_ap(apdev[1], params)

        dev[0].scan_for_bss(apdev[1]['bssid'], freq=5240)
        dev[0].connect("rrm-vht", key_mgmt="NONE", scan_freq="5180")

        addr = dev[0].own_addr()

        token = run_req_beacon(hapd, addr, "f0000000000002ffffffffffff")
        for i in range(2):
            ev = hapd.wait_event(["BEACON-RESP-RX"], timeout=10)
            if ev is None:
                raise Exception("Beacon report %d response not received" % i)
            fields = ev.split(' ')
            report = BeaconReport(binascii.unhexlify(fields[4]))
            logger.info("Received beacon report: " + str(report))
            if report.bssid_str == apdev[0]['bssid']:
                if report.opclass != 128 or report.channel != 36:
                    raise Exception("Incorrect opclass/channel for AP0")
            elif report.bssid_str == apdev[1]['bssid']:
                if report.opclass != 117 or report.channel != 48:
                    raise Exception("Incorrect opclass/channel for AP1")
    except Exception as e:
        if isinstance(e, Exception) and str(e) == "AP startup failed":
            if not vht_supported():
                raise HwsimSkip("80 MHz channel not supported in regulatory information")
        raise
    finally:
        dev[0].request("DISCONNECT")
        disable_hapd(hapd)
        disable_hapd(hapd2)
        clear_regdom_dev(dev)

@remote_compatible
def test_rrm_beacon_req_active(dev, apdev):
    """Beacon request - active scan mode"""
    params = {"ssid": "rrm", "rrm_beacon_report": "1"}
    hapd = hostapd.add_ap(apdev[0], params)
    hapd2 = hostapd.add_ap(apdev[1], {"ssid": "another", "channel": "11"})

    dev[0].connect("rrm", key_mgmt="NONE", scan_freq="2412")
    addr = dev[0].own_addr()

    token = run_req_beacon(hapd, addr, "51000000640001ffffffffffff")

    for i in range(1, 3):
        ev = hapd.wait_event(["BEACON-RESP-RX"], timeout=10)
        if ev is None:
            raise Exception("Beacon report %d response not received" % i)
        fields = ev.split(' ')
        report = BeaconReport(binascii.unhexlify(fields[4]))
        logger.info("Received beacon report: " + str(report))
        if report.bssid_str == apdev[0]['bssid']:
            if report.opclass != 81 or report.channel != 1:
                raise Exception("Incorrect opclass/channel for AP0")
        elif report.bssid_str == apdev[1]['bssid']:
            if report.opclass != 81 or report.channel != 11:
                raise Exception("Incorrect opclass/channel for AP1")

@remote_compatible
def test_rrm_beacon_req_active_ignore_old_result(dev, apdev):
    """Beacon request - active scan mode and old scan result"""
    hapd2 = hostapd.add_ap(apdev[1], {"ssid": "another"})
    dev[0].scan_for_bss(apdev[1]['bssid'], freq=2412)
    hapd2.disable()

    params = {"ssid": "rrm", "rrm_beacon_report": "1"}
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].connect("rrm", key_mgmt="NONE", scan_freq="2412")
    addr = dev[0].own_addr()

    token = run_req_beacon(hapd, addr, "51010000640001ffffffffffff")

    ev = hapd.wait_event(["BEACON-RESP-RX"], timeout=10)
    if ev is None:
        raise Exception("Beacon report response not received")
    fields = ev.split(' ')
    report = BeaconReport(binascii.unhexlify(fields[4]))
    logger.info("Received beacon report: " + str(report))
    if report.bssid_str == apdev[1]['bssid']:
        raise Exception("Old BSS reported")

    ev = hapd.wait_event(["BEACON-RESP-RX"], timeout=0.2)
    if ev is not None:
        raise Exception("Unexpected beacon report response")

def start_ap(dev):
    id = dev.add_network()
    dev.set_network(id, "mode", "2")
    dev.set_network_quoted(id, "ssid", 32*'A')
    dev.set_network_quoted(id, "psk", "1234567890")
    dev.set_network(id, "frequency", "2412")
    dev.set_network(id, "scan_freq", "2412")
    dev.select_network(id)
    dev.wait_connected()

def test_rrm_beacon_req_active_many(dev, apdev):
    """Beacon request - active scan mode and many BSSs"""
    for i in range(1, 7):
        ifname = apdev[0]['ifname'] if i == 1 else apdev[0]['ifname'] + "-%d" % i
        hapd1 = hostapd.add_bss(apdev[0], ifname, 'bss-%i.conf' % i)
        hapd1.set('vendor_elements', "dd50" + 80*'bb')
        hapd1.request("UPDATE_BEACON")

    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    wpas.interface_add("wlan5")
    wpas.request("SET device_name " + 20*'a')
    start_ap(wpas)
    start_ap(dev[1])
    start_ap(dev[2])

    params = {"ssid": "rrm", "rrm_beacon_report": "1"}
    params['vendor_elements'] = "dd50" + 80*'aa'
    hapd = hostapd.add_ap(apdev[1]['ifname'], params)

    dev[0].connect("rrm", key_mgmt="NONE", scan_freq="2412")
    addr = dev[0].own_addr()

    ok = False
    for j in range(3):
        token = run_req_beacon(hapd, addr, "51010000640001ffffffffffff")

        for i in range(10):
            ev = hapd.wait_event(["BEACON-RESP-RX"], timeout=10)
            if ev is None:
                raise Exception("Beacon report %d response not received" % i)
            fields = ev.split(' ')
            if len(fields[4]) == 0:
                break
            report = BeaconReport(binascii.unhexlify(fields[4]))
            logger.info("Received beacon report: " + str(report))
            if i == 9:
                ok = True
        if ok:
            break

@remote_compatible
def test_rrm_beacon_req_active_ap_channels(dev, apdev):
    """Beacon request - active scan mode with AP Channel Report subelement"""
    params = {"ssid": "rrm", "rrm_beacon_report": "1"}
    hapd = hostapd.add_ap(apdev[0], params)
    hapd2 = hostapd.add_ap(apdev[1], {"ssid": "another", "channel": "11"})

    dev[0].connect("rrm", key_mgmt="NONE", scan_freq="2412")
    addr = dev[0].own_addr()

    token = run_req_beacon(hapd, addr, "51ff0000640001ffffffffffff" + "dd0111" + "330351010b" + "dd0111")

    for i in range(1, 3):
        ev = hapd.wait_event(["BEACON-RESP-RX"], timeout=10)
        if ev is None:
            raise Exception("Beacon report %d response not received" % i)
        fields = ev.split(' ')
        report = BeaconReport(binascii.unhexlify(fields[4]))
        logger.info("Received beacon report: " + str(report))
        if report.bssid_str == apdev[0]['bssid']:
            if report.opclass != 81 or report.channel != 1:
                raise Exception("Incorrect opclass/channel for AP0")
        elif report.bssid_str == apdev[1]['bssid']:
            if report.opclass != 81 or report.channel != 11:
                raise Exception("Incorrect opclass/channel for AP1")

@remote_compatible
def test_rrm_beacon_req_passive_ap_channels(dev, apdev):
    """Beacon request - passive scan mode with AP Channel Report subelement"""
    params = {"ssid": "rrm", "rrm_beacon_report": "1"}
    hapd = hostapd.add_ap(apdev[0], params)
    hapd2 = hostapd.add_ap(apdev[1], {"ssid": "another", "channel": "11"})

    dev[0].connect("rrm", key_mgmt="NONE", scan_freq="2412")
    addr = dev[0].own_addr()

    token = run_req_beacon(hapd, addr, "51ff0000640000ffffffffffff" + "330351010b" + "3300" + "dd00")

    for i in range(1, 3):
        ev = hapd.wait_event(["BEACON-RESP-RX"], timeout=10)
        if ev is None:
            raise Exception("Beacon report %d response not received" % i)
        fields = ev.split(' ')
        report = BeaconReport(binascii.unhexlify(fields[4]))
        logger.info("Received beacon report: " + str(report))
        if report.bssid_str == apdev[0]['bssid']:
            if report.opclass != 81 or report.channel != 1:
                raise Exception("Incorrect opclass/channel for AP0")
        elif report.bssid_str == apdev[1]['bssid']:
            if report.opclass != 81 or report.channel != 11:
                raise Exception("Incorrect opclass/channel for AP1")

@remote_compatible
def test_rrm_beacon_req_active_single_channel(dev, apdev):
    """Beacon request - active scan mode with single channel"""
    params = {"ssid": "rrm", "rrm_beacon_report": "1"}
    hapd = hostapd.add_ap(apdev[0], params)
    hapd2 = hostapd.add_ap(apdev[1], {"ssid": "another", "channel": "11"})

    dev[0].connect("rrm", key_mgmt="NONE", scan_freq="2412")
    addr = dev[0].own_addr()

    token = run_req_beacon(hapd, addr, "510b0000640001ffffffffffff")

    ev = hapd.wait_event(["BEACON-RESP-RX"], timeout=10)
    if ev is None:
        raise Exception("Beacon report response not received")
    fields = ev.split(' ')
    report = BeaconReport(binascii.unhexlify(fields[4]))
    logger.info("Received beacon report: " + str(report))

@remote_compatible
def test_rrm_beacon_req_active_ap_channels_unknown_opclass(dev, apdev):
    """Beacon request - active scan mode with AP Channel Report subelement and unknown opclass"""
    params = {"ssid": "rrm", "rrm_beacon_report": "1"}
    hapd = hostapd.add_ap(apdev[0], params)
    hapd2 = hostapd.add_ap(apdev[1], {"ssid": "another", "channel": "11"})

    dev[0].connect("rrm", key_mgmt="NONE", scan_freq="2412")
    addr = dev[0].own_addr()

    token = run_req_beacon(hapd, addr, "51ff0000640001ffffffffffff" + "3303ff010b")

    ev = hapd.wait_event(["BEACON-RESP-RX"], timeout=10)
    if ev is None:
        raise Exception("Beacon report response (refused) not received")

    fields = ev.split(' ')
    if fields[3] != "04":
        raise Exception("Unexpected beacon report mode: " + fields[3])

@remote_compatible
def test_rrm_beacon_req_active_ap_channel_oom(dev, apdev):
    """Beacon request - AP Channel Report subelement and OOM"""
    params = {"ssid": "rrm", "rrm_beacon_report": "1"}
    hapd = hostapd.add_ap(apdev[0], params)
    hapd2 = hostapd.add_ap(apdev[1], {"ssid": "another", "channel": "11"})

    dev[0].connect("rrm", key_mgmt="NONE", scan_freq="2412")
    addr = dev[0].own_addr()

    with alloc_fail(dev[0], 1, "wpas_add_channels"):
        token = run_req_beacon(hapd, addr, "51ff0000640001ffffffffffff" + "330351010b")
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
        ev = hapd.wait_event(["BEACON-RESP-RX"], timeout=10)
        # allow either not to respond or send refused response
        if ev is not None:
            fields = ev.split(' ')
            if fields[3] != "04":
                raise Exception("Unexpected Beacon report during OOM with mode: " + fields[3])

@remote_compatible
def test_rrm_beacon_req_active_scan_fail(dev, apdev):
    """Beacon request - Active scan failure"""
    params = {"ssid": "rrm", "rrm_beacon_report": "1"}
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].connect("rrm", key_mgmt="NONE", scan_freq="2412")
    addr = dev[0].own_addr()

    with alloc_fail(dev[0], 1, "wpa_supplicant_trigger_scan"):
        token = run_req_beacon(hapd, addr, "51ff0000640001ffffffffffff" + "330351010b")
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
        ev = hapd.wait_event(["BEACON-RESP-RX"], timeout=10)
        if ev is None:
            raise Exception("No Beacon report")
        fields = ev.split(' ')
        if fields[3] != "04":
            raise Exception("Unexpected Beacon report contents: " + ev)

@remote_compatible
def test_rrm_beacon_req_active_zero_duration(dev, apdev):
    """Beacon request - Action scan and zero duration"""
    params = {"ssid": "rrm", "rrm_beacon_report": "1"}
    hapd = hostapd.add_ap(apdev[0], params)
    hapd2 = hostapd.add_ap(apdev[1], {"ssid": "another", "channel": "11"})

    dev[0].connect("rrm", key_mgmt="NONE", scan_freq="2412")
    addr = dev[0].own_addr()

    token = run_req_beacon(hapd, addr, "51000000000001ffffffffffff")
    ev = hapd.wait_event(["BEACON-RESP-RX"], timeout=0.2)
    if ev is not None:
        raise Exception("Unexpected Beacon report")

@remote_compatible
def test_rrm_beacon_req_active_fail_random(dev, apdev):
    """Beacon request - active scan mode os_get_random failure"""
    params = {"ssid": "rrm", "rrm_beacon_report": "1"}
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].connect("rrm", key_mgmt="NONE", scan_freq="2412")
    addr = dev[0].own_addr()

    with fail_test(dev[0], 1, "os_get_random;wpas_rm_handle_beacon_req"):
        token = run_req_beacon(hapd, addr, "51000000640001ffffffffffff")
        ev = hapd.wait_event(["BEACON-RESP-RX"], timeout=10)
        if ev is None:
            raise Exception("Beacon report response not received")
        fields = ev.split(' ')
        report = BeaconReport(binascii.unhexlify(fields[4]))
        logger.info("Received beacon report: " + str(report))

@remote_compatible
def test_rrm_beacon_req_passive(dev, apdev):
    """Beacon request - passive scan mode"""
    params = {"ssid": "rrm", "rrm_beacon_report": "1"}
    hapd = hostapd.add_ap(apdev[0], params)
    hapd2 = hostapd.add_ap(apdev[1], {"ssid": "another", "channel": "11"})

    dev[0].connect("rrm", key_mgmt="NONE", scan_freq="2412")
    addr = dev[0].own_addr()

    token = run_req_beacon(hapd, addr, "51000000640000ffffffffffff")

    for i in range(1, 3):
        ev = hapd.wait_event(["BEACON-RESP-RX"], timeout=10)
        if ev is None:
            raise Exception("Beacon report %d response not received" % i)
        fields = ev.split(' ')
        report = BeaconReport(binascii.unhexlify(fields[4]))
        logger.info("Received beacon report: " + str(report))
        if report.bssid_str == apdev[0]['bssid']:
            if report.opclass != 81 or report.channel != 1:
                raise Exception("Incorrect opclass/channel for AP0")
        elif report.bssid_str == apdev[1]['bssid']:
            if report.opclass != 81 or report.channel != 11:
                raise Exception("Incorrect opclass/channel for AP1")

@remote_compatible
def test_rrm_beacon_req_passive_no_match(dev, apdev):
    """Beacon request - passive scan mode and no matching BSS"""
    params = {"ssid": "rrm", "rrm_beacon_report": "1"}
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].connect("rrm", key_mgmt="NONE", scan_freq="2412")
    addr = dev[0].own_addr()

    token = run_req_beacon(hapd, addr, "51010000640000021122334455")
    ev = hapd.wait_event(["BEACON-RESP-RX"], timeout=10)
    if ev is None:
        raise Exception("Beacon report %d response not received" % i)
    fields = ev.split(' ')
    if len(fields[4]) > 0:
        raise Exception("Unexpected beacon report BSS")

@remote_compatible
def test_rrm_beacon_req_passive_no_match_oom(dev, apdev):
    """Beacon request - passive scan mode and no matching BSS (OOM)"""
    params = {"ssid": "rrm", "rrm_beacon_report": "1"}
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].connect("rrm", key_mgmt="NONE", scan_freq="2412")
    addr = dev[0].own_addr()

    with alloc_fail(dev[0], 1, "wpabuf_resize;wpas_beacon_rep_scan_process"):
        token = run_req_beacon(hapd, addr, "51010000640000021122334455")
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
        ev = hapd.wait_event(["BEACON-RESP-RX"], timeout=0.2)
        if ev is not None:
            raise Exception("Unexpected Beacon report response during OOM")

    # verify reporting is still functional
    token = run_req_beacon(hapd, addr, "51010000640000021122334455")
    ev = hapd.wait_event(["BEACON-RESP-RX"], timeout=10)
    if ev is None:
        raise Exception("Beacon report %d response not received" % i)
    fields = ev.split(' ')
    if len(fields[4]) > 0:
        raise Exception("Unexpected beacon report BSS")

@remote_compatible
def test_rrm_beacon_req_active_duration_mandatory(dev, apdev):
    """Beacon request - Action scan and duration mandatory"""
    params = {"ssid": "rrm", "rrm_beacon_report": "1"}
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].connect("rrm", key_mgmt="NONE", scan_freq="2412")
    addr = dev[0].own_addr()

    token = run_req_beacon(hapd, addr, "req_mode=10 51000000640001ffffffffffff")
    ev = hapd.wait_event(["BEACON-RESP-RX"], timeout=10)
    if ev is None:
        raise Exception("No Beacon report response")
    fields = ev.split(' ')
    rrm = int(dev[0].get_driver_status_field("capa.rrm_flags"), 16)
    if rrm & 0x20 == 0x20:
        report = BeaconReport(binascii.unhexlify(fields[4]))
        logger.info("Received beacon report: " + str(report))
    else:
        # Driver does not support scan dwell time setting, so wpa_supplicant
        # rejects the measurement request due to the mandatory duration using
        # Measurement Report Mode field Incapable=1.
        if fields[3] != '02':
            raise Exception("Unexpected Measurement Report Mode: " + fields[3])
        if len(fields[4]) > 0:
            raise Exception("Unexpected beacon report received")

def test_rrm_beacon_req_passive_scan_vht(dev, apdev):
    """Beacon request - passive scan mode - VHT"""
    clear_scan_cache(apdev[0])
    try:
        hapd = None
        params = {"ssid": "rrm-vht",
                  "country_code": "FI",
                  'ieee80211d': '1',
                  "hw_mode": "a",
                  "channel": "36",
                  "ht_capab": "[HT40+]",
                  "ieee80211n": "1",
                  "ieee80211ac": "1",
                  "vht_oper_chwidth": "1",
                  "vht_oper_centr_freq_seg0_idx": "42",
                  "rrm_beacon_report": "1"}
        hapd = hostapd.add_ap(apdev[0], params)

        dev[0].scan_for_bss(apdev[0]['bssid'], freq=5180)
        dev[0].connect("rrm-vht", key_mgmt="NONE", scan_freq="5180")

        addr = dev[0].own_addr()

        token = run_req_beacon(hapd, addr, "80000000640000ffffffffffff")
        ev = hapd.wait_event(["BEACON-RESP-RX"], timeout=10)
        if ev is None:
            raise Exception("Beacon report response not received")
        fields = ev.split(' ')
        report = BeaconReport(binascii.unhexlify(fields[4]))
        logger.info("Received beacon report: " + str(report))
        if report.opclass != 128 or report.channel != 36:
            raise Exception("Incorrect opclass/channel for AP")

        token = run_req_beacon(hapd, addr, "82000000640000ffffffffffff")
        ev = hapd.wait_event(["BEACON-RESP-RX"], timeout=10)
        if ev is None:
            raise Exception("Beacon report response not received")
        fields = ev.split(' ')
        report = BeaconReport(binascii.unhexlify(fields[4]))
        logger.info("Received beacon report: " + str(report))
        if report.opclass != 128 or report.channel != 36:
            raise Exception("Incorrect opclass/channel for AP")
    except Exception as e:
        if isinstance(e, Exception) and str(e) == "AP startup failed":
            if not vht_supported():
                raise HwsimSkip("80 MHz channel not supported in regulatory information")
        raise
    finally:
        clear_regdom(hapd, dev)

def test_rrm_beacon_req_passive_scan_vht160(dev, apdev):
    """Beacon request - passive scan mode - VHT160"""
    clear_scan_cache(apdev[0])
    try:
        hapd = None
        params = {"ssid": "rrm-vht",
                  "country_code": "ZA",
                  'ieee80211d': '1',
                  "hw_mode": "a",
                  "channel": "104",
                  "ht_capab": "[HT40-]",
                  "vht_capab": "[VHT160]",
                  "ieee80211n": "1",
                  "ieee80211ac": "1",
                  "vht_oper_chwidth": "2",
                  "vht_oper_centr_freq_seg0_idx": "114",
                  "rrm_beacon_report": "1"}
        hapd = hostapd.add_ap(apdev[0], params)

        dev[0].scan_for_bss(apdev[0]['bssid'], freq=5520)
        dev[0].connect("rrm-vht", key_mgmt="NONE", scan_freq="5520")
        sig = dev[0].request("SIGNAL_POLL").splitlines()
        if "WIDTH=160 MHz" not in sig:
            raise Exception("Unexpected SIGNAL_POLL value: " + str(sig))

        addr = dev[0].own_addr()

        token = run_req_beacon(hapd, addr, "81000000640000ffffffffffff")
        ev = hapd.wait_event(["BEACON-RESP-RX"], timeout=10)
        if ev is None:
            raise Exception("Beacon report response not received")
        fields = ev.split(' ')
        report = BeaconReport(binascii.unhexlify(fields[4]))
        logger.info("Received beacon report: " + str(report))
        if report.opclass != 129 or report.channel != 104:
            raise Exception("Incorrect opclass/channel for AP")
    except Exception as e:
        if isinstance(e, Exception) and str(e) == "AP startup failed":
            raise HwsimSkip("ZA regulatory rule likely did not have DFS requirement removed")
        raise
    finally:
        clear_regdom(hapd, dev)

def test_rrm_beacon_req_ap_errors(dev, apdev):
    """Beacon request - AP error cases"""
    try:
        run_rrm_beacon_req_ap_errors(dev, apdev)
    finally:
        dev[1].request("VENDOR_ELEM_REMOVE 13 *")

def run_rrm_beacon_req_ap_errors(dev, apdev):
    params = {"ssid": "rrm", "rrm_beacon_report": "1"}
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)
    bssid = hapd.own_addr()
    dev[0].scan_for_bss(bssid, freq=2412)
    dev[0].connect("rrm", key_mgmt="NONE", scan_freq="2412")
    addr = dev[0].own_addr()
    # Override RM capabilities (remove all)
    dev[1].request("VENDOR_ELEM_ADD 13 46050000000000")
    dev[1].connect("rrm", key_mgmt="NONE", scan_freq="2412")
    addr1 = dev[1].own_addr()

    # Beacon request: Too short request data
    if "FAIL" not in hapd.request("REQ_BEACON " + addr + " 11"):
        raise Exception("Invalid REQ_BEACON accepted")

    # Beacon request: 02:00:00:00:01:00 does not support table beacon report
    if "FAIL" not in hapd.request("REQ_BEACON " + addr1 + " 51000000000002ffffffffffff"):
        raise Exception("Invalid REQ_BEACON accepted")

    # Beacon request: 02:00:00:00:01:00 does not support active beacon report
    if "FAIL" not in hapd.request("REQ_BEACON " + addr1 + " 51000000640001ffffffffffff"):
        raise Exception("Invalid REQ_BEACON accepted")

    # Beacon request: 02:00:00:00:01:00 does not support passive beacon report
    if "FAIL" not in hapd.request("REQ_BEACON " + addr1 + " 510b0000640000ffffffffffff"):
        raise Exception("Invalid REQ_BEACON accepted")

    # Beacon request: Unknown measurement mode 3
    if "FAIL" not in hapd.request("REQ_BEACON " + addr1 + " 510b0000640003ffffffffffff"):
        raise Exception("Invalid REQ_BEACON accepted")

    for i in range(257):
        if "FAIL" in hapd.request("REQ_BEACON " + addr + " 510b0000640000ffffffffffff"):
            raise Exception("REQ_BEACON failed")
        dev[0].dump_monitor()
        hapd.dump_monitor()

    with alloc_fail(hapd, 1, "wpabuf_alloc;hostapd_send_beacon_req"):
        if "FAIL" not in hapd.request("REQ_BEACON " + addr + " 510b0000640000ffffffffffff"):
            raise Exception("REQ_BEACON accepted during OOM")

    with fail_test(hapd, 1, "nl80211_send_frame_cmd;hostapd_send_beacon_req"):
        if "FAIL" not in hapd.request("REQ_BEACON " + addr + " 510b0000640000ffffffffffff"):
            raise Exception("REQ_BEACON accepted during failure testing")

def test_rrm_req_reject_oom(dev, apdev):
    """Radio measurement request - OOM while rejecting a request"""
    params = {"ssid": "rrm", "rrm_beacon_report": "1"}
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)
    bssid = hapd.own_addr()

    dev[0].connect("rrm", key_mgmt="NONE", scan_freq="2412")
    addr = dev[0].own_addr()

    hdr = "d0003a01" + addr.replace(':', '') + 2*bssid.replace(':', '') + "1000"

    hapd.set("ext_mgmt_frame_handling", "1")
    dev[0].request("SET ext_mgmt_frame_handling 1")

    with alloc_fail(dev[0], 1, "wpabuf_resize;wpas_rrm_handle_msr_req_element"):
        # "RRM: Parallel measurements are not supported, reject"
        if "OK" not in dev[0].request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=" + hdr + "05000100002603010105"):
            raise Exception("MGMT_RX_PROCESS failed")
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
        ev = hapd.wait_event(["MGMT-RX"], timeout=0.2)
        if ev is not None:
            raise Exception("Unexpected beacon report response during OOM")

def test_rrm_req_when_rrm_not_used(dev, apdev):
    """Radio/link measurement request for non-RRM association"""
    params = {"ssid": "rrm"}
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)
    bssid = hapd.own_addr()

    dev[0].connect("rrm", key_mgmt="NONE", scan_freq="2412")
    addr = dev[0].own_addr()

    hdr = "d0003a01" + addr.replace(':', '') + 2*bssid.replace(':', '') + "1000"

    hapd.set("ext_mgmt_frame_handling", "1")
    dev[0].request("SET ext_mgmt_frame_handling 1")

    if "OK" not in dev[0].request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=" + hdr + "050001000026030100fe"):
        raise Exception("MGMT_RX_PROCESS failed")
    if "OK" not in dev[0].request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=" + hdr + "0502000000"):
        raise Exception("MGMT_RX_PROCESS failed")
    ev = hapd.wait_event(["MGMT-RX"], timeout=0.2)
    if ev is not None:
        raise Exception("Unexpected beacon report response when RRM is disabled")

    dev[0].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected()
    if "OK" not in dev[0].request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=" + hdr + "050001000026030100fe"):
        raise Exception("MGMT_RX_PROCESS failed")
    if "OK" not in dev[0].request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=" + hdr + "0502000000"):
        raise Exception("MGMT_RX_PROCESS failed")

@remote_compatible
def test_rrm_req_proto(dev, apdev):
    """Radio measurement request - protocol testing"""
    params = {"ssid": "rrm", "rrm_beacon_report": "1"}
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = hapd.own_addr()

    dev[0].request("SET LCI ")
    dev[0].connect("rrm", key_mgmt="NONE", scan_freq="2412")
    addr = dev[0].own_addr()

    hdr = "d0003a01" + addr.replace(':', '') + 2*bssid.replace(':', '') + "1000"

    hapd.set("ext_mgmt_frame_handling", "1")
    dev[0].request("SET ext_mgmt_frame_handling 1")

    tests = []
    # "RRM: Ignoring too short radio measurement request"
    tests += ["0500", "050001", "05000100"]
    # No measurement request element at all
    tests += ["0500010000"]
    # "RRM: Truncated element"
    tests += ["050001000026"]
    # "RRM: Element length too short"
    tests += ["05000100002600", "0500010000260111", "050001000026021122"]
    # "RRM: Element length too long"
    tests += ["05000100002603", "0500010000260311", "050001000026031122"]
    # "RRM: Enable bit not supported, ignore"
    tests += ["05000100002603010200"]
    # "RRM: Measurement report failed. TX power insertion not supported"
    #    OR
    # "RRM: Link measurement report failed. Request too short"
    tests += ["0502"]
    # Too short LCI request
    tests += ["05000100002603010008"]
    # Too short neighbor report response
    tests += ["0505"]
    # Unexpected neighbor report response
    tests += ["050500", "050501", "050502", "050503", "050504", "050505"]
    # Too short beacon request
    tests += ["05000100002603010005",
              "0500010000260f010005112233445566778899aabbcc"]
    # Unknown beacon report mode
    tests += ["05000100002610010005112233445566778899aabbccdd"]
    # "RRM: Expected Measurement Request element, but EID is 0"
    tests += ["05000100000000"]
    for t in tests:
        if "OK" not in dev[0].request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=" + hdr + t):
            raise Exception("MGMT_RX_PROCESS failed")
    ev = hapd.wait_event(["MGMT-RX"], timeout=0.2)
    if ev is not None:
        raise Exception("Unexpected response seen at the AP: " + ev)

    tests = []
    # "RRM: Parallel measurements are not supported, reject"
    tests += ["05000100002603010105"]
    # "RRM: Unsupported radio measurement type 254"
    tests += ["050001000026030100fe"]
    # Reject LCI request
    tests += ["0500010000260701000811223344"]
    # Beacon report info subelement; no valid channels
    tests += ["05000100002614010005112233445566008899aabbccdd01020000"]
    for t in tests:
        if "OK" not in dev[0].request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=" + hdr + t):
            raise Exception("MGMT_RX_PROCESS failed")
        ev = hapd.wait_event(["MGMT-RX"], timeout=5)
        if ev is None:
            raise Exception("No response seen at the AP")
        hapd.dump_monitor()

    dev[0].request("SET LCI " + lci)
    tests = []
    # "Not building LCI report - bad location subject"
    tests += ["0500010000260701000811223344"]
    for t in tests:
        if "OK" not in dev[0].request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=" + hdr + t):
            raise Exception("MGMT_RX_PROCESS failed")
    ev = hapd.wait_event(["MGMT-RX"], timeout=0.2)
    if ev is not None:
        raise Exception("Unexpected response seen at the AP: " + ev)

    tests = []
    # LCI report or reject
    tests += ["0500010000260701000801223344",
              "05000100002607010008010402ff",
              "05000100002608010008010402ffff"]
    for t in tests:
        if "OK" not in dev[0].request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=" + hdr + t):
            raise Exception("MGMT_RX_PROCESS failed")
        ev = hapd.wait_event(["MGMT-RX"], timeout=5)
        if ev is None:
            raise Exception("No response seen at the AP")
        hapd.dump_monitor()

    # Verify rejection of a group-addressed request frame
    hdr = "d0003a01" + "ffffffffffff" + 2*bssid.replace(':', '') + "1000"
    # "RRM: Parallel measurements are not supported, reject"
    t = "05000100002603010105"
    if "OK" not in dev[0].request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=" + hdr + t):
        raise Exception("MGMT_RX_PROCESS failed")
    ev = hapd.wait_event(["MGMT-RX"], timeout=0.1)
    if ev is not None:
        raise Exception("Unexpected response seen at the AP (broadcast request rejected)")
    hapd.dump_monitor()

    hapd.set("ext_mgmt_frame_handling", "0")
    dev[0].request("SET ext_mgmt_frame_handling 0")
    dev[0].request("SET LCI ")

def test_rrm_link_measurement(dev, apdev):
    """Radio measurement request - link measurement"""
    check_tx_power_support(dev[0])
    params = {"ssid": "rrm", "rrm_beacon_report": "1"}
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)
    bssid = hapd.own_addr()

    dev[0].connect("rrm", key_mgmt="NONE", scan_freq="2412")
    addr = dev[0].own_addr()

    hdr = "d0003a01" + addr.replace(':', '') + 2*bssid.replace(':', '') + "1000"

    hapd.set("ext_mgmt_frame_handling", "1")
    dev[0].request("SET ext_mgmt_frame_handling 1")

    if "OK" not in dev[0].request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=" + hdr + "0502000000"):
        raise Exception("MGMT_RX_PROCESS failed")
    ev = hapd.wait_event(["MGMT-RX"], timeout=5)
    if ev is None:
        raise Exception("No link measurement report seen")

def test_rrm_link_measurement_oom(dev, apdev):
    """Radio measurement request - link measurement OOM"""
    check_tx_power_support(dev[0])
    params = {"ssid": "rrm", "rrm_beacon_report": "1"}
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)
    bssid = hapd.own_addr()

    dev[0].connect("rrm", key_mgmt="NONE", scan_freq="2412")
    addr = dev[0].own_addr()

    hdr = "d0003a01" + addr.replace(':', '') + 2*bssid.replace(':', '') + "1000"

    hapd.set("ext_mgmt_frame_handling", "1")
    dev[0].request("SET ext_mgmt_frame_handling 1")

    with alloc_fail(dev[0], 1, "wpabuf_alloc;wpas_rrm_handle_link_measurement_request"):
        if "OK" not in dev[0].request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=" + hdr + "0502000000"):
            raise Exception("MGMT_RX_PROCESS failed")
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")

    with fail_test(dev[0], 1, "wpas_rrm_handle_link_measurement_request"):
        if "OK" not in dev[0].request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=" + hdr + "0502000000"):
            raise Exception("MGMT_RX_PROCESS failed")
        wait_fail_trigger(dev[0], "GET_FAIL")

    ev = hapd.wait_event(["MGMT-RX"], timeout=0.1)
    if ev is not None:
        raise Exception("Unexpected beacon report response during OOM")

def test_rrm_rep_parse_proto(dev, apdev):
    """hostapd rrm report parsing protocol testing"""
    check_rrm_support(dev[0])

    params = {"ssid": "rrm", "rrm_neighbor_report": "1"}
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)
    bssid = hapd.own_addr()

    dev[0].request("SET LCI " + lci)
    dev[0].connect("rrm", key_mgmt="NONE", scan_freq="2412")
    addr = dev[0].own_addr()

    hdr = "d0003a01" + bssid.replace(':', '') + addr.replace(':', '') + bssid.replace(':', '') + "1000"
    hapd.set("ext_mgmt_frame_handling", "1")

    tests = ["0501",
             "05ff01",
             "0501012703fffffe2700",
             "0501012703ffff05",
             "05010127ffffff05" + 252*"00",
             "0504012603ffffff2600",
             "0504012603ffff08",
             "0504012608ffff08ffffffffff",
             "0504012608ffff08ff04021234",
             "0504012608ffff08ff04020100",
             "0504012608ffff08ff0402ffff"]
    for t in tests:
        if "OK" not in hapd.request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=" + hdr + t):
            raise Exception("MGMT_RX_PROCESS failed for " + t)

    if "OK" not in hapd.request("SET_NEIGHBOR 00:11:22:33:44:55 ssid=\"rrm\" nr=" + nr + " lci=" + lci):
        raise Exception("Set neighbor failed")
    if "OK" not in hapd.request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=" + hdr + "0504012608ffff08ff04021000"):
        raise Exception("MGMT_RX_PROCESS failed")

def test_rrm_unexpected(dev, apdev):
    """hostapd unexpected rrm"""
    check_rrm_support(dev[0])

    params = {"ssid": "rrm", "rrm_neighbor_report": "0"}
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)
    bssid = hapd.own_addr()

    dev[0].connect("rrm", key_mgmt="NONE", scan_freq="2412")
    addr = dev[0].own_addr()

    hdr = "d0003a01" + bssid.replace(':', '') + addr.replace(':', '') + bssid.replace(':', '') + "1000"
    hapd.set("ext_mgmt_frame_handling", "1")

    tests = ["050401"]
    for t in tests:
        if "OK" not in hapd.request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=" + hdr + t):
            raise Exception("MGMT_RX_PROCESS failed for " + t)

def check_beacon_req(hapd, addr, idx):
    request = "51000000000002ffffffffffff" + "020100"
    token = hapd.request("REQ_BEACON " + addr + " " + request)
    if "FAIL" in token:
        raise Exception("REQ_BEACON failed (%d)" % idx)
    ev = hapd.wait_event(["BEACON-RESP-RX"], timeout=10)
    if ev is None:
        raise Exception("Beacon report response not received (%d)" % idx)

def test_rrm_reassociation(dev, apdev):
    """Radio measurement request - reassociation"""
    params = {"ssid": "rrm", "rrm_beacon_report": "1"}
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)
    bssid = hapd.own_addr()

    addr = dev[0].own_addr()
    dev[0].flush_scan_cache()
    dev[0].connect("rrm", key_mgmt="NONE", scan_freq="2412")
    check_beacon_req(hapd, addr, 1)

    dev[0].request("REASSOCIATE")
    dev[0].wait_connected()
    check_beacon_req(hapd, addr, 1)

    hapd2 = hostapd.add_ap(apdev[1]['ifname'], params)
    bssid2 = hapd2.own_addr()
    dev[0].scan_for_bss(bssid2, freq=2412)
    dev[0].roam(bssid2)
    check_beacon_req(hapd2, addr, 2)

    dev[0].scan_for_bss(bssid, freq=2412)
    dev[0].roam(bssid)
    check_beacon_req(hapd, addr, 3)
