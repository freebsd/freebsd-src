# Test cases for wpa_supplicant WMM-AC operations
# Copyright (c) 2014, Intel Corporation
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

from remotehost import remote_compatible
import logging
logger = logging.getLogger()
import struct
import sys

import hwsim_utils
import hostapd
from utils import fail_test

def add_wmm_ap(apdev, acm_list):
    params = {"ssid": "wmm_ac",
              "hw_mode": "g",
              "channel": "11",
              "wmm_enabled": "1"}

    for ac in acm_list:
        params["wmm_ac_%s_acm" % (ac.lower())] = "1"

    return hostapd.add_ap(apdev, params)

def test_tspec(dev, apdev):
    """Basic addts/delts tests"""
    # configure ap with VO and VI requiring admission-control
    hapd = add_wmm_ap(apdev[0], ["VO", "VI"])
    dev[0].connect("wmm_ac", key_mgmt="NONE", scan_freq="2462")
    hwsim_utils.test_connectivity(dev[0], hapd)
    status = dev[0].request("WMM_AC_STATUS")
    if "WMM AC is Enabled" not in status:
        raise Exception("WMM-AC not enabled")
    if "TSID" in status:
        raise Exception("Unexpected TSID info")
    if "BK: acm=0 uapsd=0" not in status:
        raise Exception("Unexpected BK info" + status)
    if "BE: acm=0 uapsd=0" not in status:
        raise Exception("Unexpected BE info" + status)
    if "VI: acm=1 uapsd=0" not in status:
        raise Exception("Unexpected VI info" + status)
    if "VO: acm=1 uapsd=0" not in status:
        raise Exception("Unexpected VO info" + status)

    # no tsid --> tsid out of range
    if "FAIL" not in dev[0].request("WMM_AC_ADDTS downlink"):
        raise Exception("Invalid WMM_AC_ADDTS accepted")
    # no direction
    if "FAIL" not in dev[0].request("WMM_AC_ADDTS tsid=5"):
        raise Exception("Invalid WMM_AC_ADDTS accepted")
    # param out of range
    if "FAIL" not in dev[0].request("WMM_AC_ADDTS tsid=5 downlink"):
        raise Exception("Invalid WMM_AC_ADDTS accepted")

    tsid = 5

    # make sure we fail when the ac is not configured for acm
    try:
        dev[0].add_ts(tsid, 3)
        raise Exception("ADDTS succeeded although it should have failed")
    except Exception as e:
        if not str(e).startswith("ADDTS failed"):
            raise
    status = dev[0].request("WMM_AC_STATUS")
    if "TSID" in status:
        raise Exception("Unexpected TSID info")

    # add tspec for UP=6
    dev[0].add_ts(tsid, 6)
    status = dev[0].request("WMM_AC_STATUS")
    if "TSID" not in status:
        raise Exception("Missing TSID info")

    # using the same tsid for a different ac is invalid
    try:
        dev[0].add_ts(tsid, 5)
        raise Exception("ADDTS succeeded although it should have failed")
    except Exception as e:
        if not str(e).startswith("ADDTS failed"):
            raise

    # update the tspec for a different UP of the same ac
    dev[0].add_ts(tsid, 7, extra="fixed_nominal_msdu")
    dev[0].del_ts(tsid)
    status = dev[0].request("WMM_AC_STATUS")
    if "TSID" in status:
        raise Exception("Unexpected TSID info")

    # verify failure on uplink/bidi without driver support
    tsid = 6
    try:
        dev[0].add_ts(tsid, 7, direction="uplink")
        raise Exception("ADDTS succeeded although it should have failed")
    except Exception as e:
        if not str(e).startswith("ADDTS failed"):
            raise
    try:
        dev[0].add_ts(tsid, 7, direction="bidi")
        raise Exception("ADDTS succeeded although it should have failed")
    except Exception as e:
        if not str(e).startswith("ADDTS failed"):
            raise

    # attempt to delete non-existing tsid
    try:
        dev[0].del_ts(tsid)
        raise Exception("DELTS succeeded although it should have failed")
    except Exception as e:
        if not str(e).startswith("DELTS failed"):
            raise

    # "CTRL: Invalid WMM_AC_ADDTS parameter: 'foo'
    if "FAIL" not in dev[0].request("WMM_AC_ADDTS foo"):
        raise Exception("Invalid WMM_AC_ADDTS command accepted")

def test_tspec_protocol(dev, apdev):
    """Protocol tests for addts/delts"""
    # configure ap with VO and VI requiring admission-control
    hapd = add_wmm_ap(apdev[0], ["VO", "VI"])
    dev[0].connect("wmm_ac", key_mgmt="NONE", scan_freq="2462")

    dev[0].dump_monitor()
    hapd.set("ext_mgmt_frame_handling", "1")

    tsid = 6

    # timeout on ADDTS response
    dev[0].add_ts(tsid, 7, expect_failure=True)

    hapd.dump_monitor()
    req = "WMM_AC_ADDTS downlink tsid=6 up=7 nominal_msdu_size=1500 sba=9000 mean_data_rate=1500 min_phy_rate=6000000"
    if "OK" not in dev[0].request(req):
        raise Exception("WMM_AC_ADDTS failed")
    # a new request while previous is still pending
    if "FAIL" not in dev[0].request(req):
        raise Exception("WMM_AC_ADDTS accepted while oen was still pending")
    msg = hapd.mgmt_rx()
    payload = msg['payload']
    (categ, action, dialog, status) = struct.unpack('BBBB', payload[0:4])
    if action != 0:
        raise Exception("Unexpected Action code: %d" % action)

    msg['da'] = msg['sa']
    msg['sa'] = apdev[0]['bssid']

    # unexpected dialog token
    msg['payload'] = struct.pack('BBBB', 17, 1, (dialog + 1) & 0xff, 0) + payload[4:]
    hapd.mgmt_tx(msg)

    # valid response
    msg['payload'] = struct.pack('BBBB', 17, 1, dialog, 0) + payload[4:]
    hapd.mgmt_tx(msg)
    ev = dev[0].wait_event(["TSPEC-ADDED"], timeout=10)
    if ev is None:
        raise Exception("Timeout on TSPEC-ADDED")
    if "tsid=%d" % tsid not in ev:
        raise Exception("Unexpected TSPEC-ADDED contents: " + ev)

    # duplicated response
    msg['payload'] = struct.pack('BBBB', 17, 1, dialog, 0) + payload[4:]
    hapd.mgmt_tx(msg)

    # too short ADDTS
    msg['payload'] = struct.pack('BBBB', 17, 1, dialog, 0)
    hapd.mgmt_tx(msg)

    # invalid IE
    msg['payload'] = struct.pack('BBBB', 17, 1, dialog, 0) + payload[4:] + struct.pack('BB', 0xdd, 100)
    hapd.mgmt_tx(msg)

    # too short WMM element
    msg['payload'] = struct.pack('BBBB', 17, 1, dialog, 0) + payload[4:] + b'\xdd\x06\x00\x50\xf2\x02\x02\x01'
    hapd.mgmt_tx(msg)

    # DELTS
    dev[0].dump_monitor()
    msg['payload'] = struct.pack('BBBB', 17, 2, 0, 0) + payload[4:]
    hapd.mgmt_tx(msg)
    ev = dev[0].wait_event(['TSPEC-REMOVED'], timeout=6)
    if ev is None:
        raise Exception("Timeout on TSPEC-REMOVED event")
    if "tsid=%d" % tsid not in ev:
        raise Exception("Unexpected TSPEC-REMOVED contents: " + ev)
    # DELTS duplicated
    msg['payload'] = struct.pack('BBBB', 17, 2, 0, 0) + payload[4:]
    hapd.mgmt_tx(msg)

    # start a new request
    hapd.dump_monitor()
    if "OK" not in dev[0].request(req):
        raise Exception("WMM_AC_ADDTS failed")
    msg = hapd.mgmt_rx()
    payload = msg['payload']
    (categ, action, dialog, status) = struct.unpack('BBBB', payload[0:4])
    if action != 0:
        raise Exception("Unexpected Action code: %d" % action)

    msg['da'] = msg['sa']
    msg['sa'] = apdev[0]['bssid']

    # modified parameters
    p12int = payload[12] if sys.version_info[0] > 2 else ord(payload[12])
    msg['payload'] = struct.pack('BBBB', 17, 1, dialog, 1) + payload[4:12] + struct.pack('B', p12int & ~0x60) + payload[13:]
    hapd.mgmt_tx(msg)

    # reject request
    msg['payload'] = struct.pack('BBBB', 17, 1, dialog, 1) + payload[4:]
    hapd.mgmt_tx(msg)
    ev = dev[0].wait_event(["TSPEC-REQ-FAILED"], timeout=10)
    if ev is None:
        raise Exception("Timeout on TSPEC-REQ-FAILED")
    if "tsid=%d" % tsid not in ev:
        raise Exception("Unexpected TSPEC-REQ-FAILED contents: " + ev)

    hapd.set("ext_mgmt_frame_handling", "0")

@remote_compatible
def test_tspec_not_enabled(dev, apdev):
    """addts failing if AP does not support WMM"""
    params = {"ssid": "wmm_no_ac",
              "hw_mode": "g",
              "channel": "11",
              "wmm_enabled": "0"}
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].connect("wmm_no_ac", key_mgmt="NONE", scan_freq="2462")
    status = dev[0].request("WMM_AC_STATUS")
    if "Not associated to a WMM AP, WMM AC is Disabled" not in status:
        raise Exception("Unexpected WMM_AC_STATUS: " + status)

    try:
        dev[0].add_ts(5, 6)
        raise Exception("ADDTS succeeded although it should have failed")
    except Exception as e:
        if not str(e).startswith("ADDTS failed"):
            raise

    # attempt to delete non-existing tsid
    try:
        dev[0].del_ts(5)
        raise Exception("DELTS succeeded although it should have failed")
    except Exception as e:
        if not str(e).startswith("DELTS failed"):
            raise

    # unexpected Action frame when WMM is disabled
    MGMT_SUBTYPE_ACTION = 13
    msg = {}
    msg['fc'] = MGMT_SUBTYPE_ACTION << 4
    msg['da'] = dev[0].p2p_interface_addr()
    msg['sa'] = apdev[0]['bssid']
    msg['bssid'] = apdev[0]['bssid']
    msg['payload'] = struct.pack('BBBB', 17, 2, 0, 0)
    hapd.mgmt_tx(msg)

@remote_compatible
def test_tspec_ap_roam_open(dev, apdev):
    """Roam between two open APs while having tspecs"""
    hapd0 = add_wmm_ap(apdev[0], ["VO", "VI"])
    dev[0].connect("wmm_ac", key_mgmt="NONE")
    hwsim_utils.test_connectivity(dev[0], hapd0)
    dev[0].add_ts(5, 6)

    hapd1 = add_wmm_ap(apdev[1], ["VO", "VI"])
    dev[0].scan_for_bss(apdev[1]['bssid'], freq=2462)
    dev[0].roam(apdev[1]['bssid'])
    hwsim_utils.test_connectivity(dev[0], hapd1)
    if dev[0].tspecs():
        raise Exception("TSPECs weren't deleted on roaming")

    dev[0].scan_for_bss(apdev[0]['bssid'], freq=2462)
    dev[0].roam(apdev[0]['bssid'])
    hwsim_utils.test_connectivity(dev[0], hapd0)

@remote_compatible
def test_tspec_reassoc(dev, apdev):
    """Reassociation to same BSS while having tspecs"""
    hapd0 = add_wmm_ap(apdev[0], ["VO", "VI"])
    dev[0].connect("wmm_ac", key_mgmt="NONE")
    hwsim_utils.test_connectivity(dev[0], hapd0)
    dev[0].add_ts(5, 6)
    last_tspecs = dev[0].tspecs()

    dev[0].request("REASSOCIATE")
    dev[0].wait_connected()

    hwsim_utils.test_connectivity(dev[0], hapd0)
    if dev[0].tspecs() != last_tspecs:
        raise Exception("TSPECs weren't saved on reassociation")

def test_wmm_element(dev, apdev):
    """hostapd FTM range request timeout"""
    try:
        run_wmm_element(dev, apdev)
    finally:
        dev[0].request("VENDOR_ELEM_REMOVE 13 *")

def run_wmm_element(dev, apdev):
    params = {"ssid": "wmm"}
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)
    bssid = hapd.own_addr()

    # Too short WMM IE
    dev[0].request("VENDOR_ELEM_ADD 13 dd060050f2020001")
    dev[0].scan_for_bss(bssid, freq=2412)
    dev[0].connect("wmm", key_mgmt="NONE", scan_freq="2412", wait_connect=False)
    ev = dev[0].wait_event(["CTRL-EVENT-ASSOC-REJECT"], timeout=10)
    if ev is None:
        raise Exception("Association not rejected")
    dev[0].request("REMOVE_NETWORK all")

    # Unsupported WMM IE Subtype/Version
    dev[0].request("VENDOR_ELEM_ADD 13 dd070050f202000000")
    dev[0].connect("wmm", key_mgmt="NONE", scan_freq="2412", wait_connect=False)
    ev = dev[0].wait_event(["CTRL-EVENT-ASSOC-REJECT"], timeout=10)
    if ev is None:
        raise Exception("Association not rejected")
    dev[0].request("REMOVE_NETWORK all")

    # Unsupported WMM IE Subtype/Version
    dev[0].request("VENDOR_ELEM_ADD 13 dd070050f202010100")
    dev[0].connect("wmm", key_mgmt="NONE", scan_freq="2412", wait_connect=False)
    ev = dev[0].wait_event(["CTRL-EVENT-ASSOC-REJECT"], timeout=10)
    if ev is None:
        raise Exception("Association not rejected")
    dev[0].request("REMOVE_NETWORK all")

def test_tspec_ap_fail(dev, apdev):
    """AP failing to send tspec response"""
    # configure ap with VO and VI requiring admission-control
    hapd = add_wmm_ap(apdev[0], ["VO", "VI"])
    dev[0].connect("wmm_ac", key_mgmt="NONE", scan_freq="2462")
    tsid = 5

    with fail_test(hapd, 1, "wmm_send_action"):
        try:
            # add tspec for UP=6
            dev[0].add_ts(tsid, 6)
        except:
            pass

def test_tspec_ap_parsing(dev, apdev):
    """TSPEC AP parsing tests"""
    # configure ap with VO and VI requiring admission-control
    hapd = add_wmm_ap(apdev[0], ["VO", "VI"])
    bssid = hapd.own_addr()
    dev[0].connect("wmm_ac", key_mgmt="NONE", scan_freq="2462")
    addr = dev[0].own_addr()

    tests = ["WMM_AC_ADDTS downlink tsid=5 up=6 nominal_msdu_size=1500 sba=9000 mean_data_rate=1500 min_phy_rate=600000",
             "WMM_AC_ADDTS downlink tsid=5 up=6 nominal_msdu_size=1500 sba=8192 mean_data_rate=1500 min_phy_rate=6000000",
             "WMM_AC_ADDTS downlink tsid=5 up=6 nominal_msdu_size=32767 sba=65535 mean_data_rate=1500 min_phy_rate=1000000",
             "WMM_AC_ADDTS downlink tsid=5 up=6 nominal_msdu_size=10000 sba=65535 mean_data_rate=2147483647 min_phy_rate=1000000"]
    for t in tests:
        if "OK" not in dev[0].request(t):
            raise Exception("WMM_AC_ADDTS failed")
        ev = dev[0].wait_event(["TSPEC-REQ-FAILED"], timeout=1)
        if ev is None:
            raise Exception("No response")

    tests = []
    # WMM: Invalid Nominal MSDU Size (0)
    tests += ["11000400dd3d0050f2020201aa300000000000000000000000000000000000000000000000000000000000ffffff7f00000000000000000000000040420f00ffff0000"]
    # hostapd_wmm_action - missing or wrong length tspec
    tests += ["11000400dd3e0050f2020201aa300010270000000000000000000000000000000000000000000000000000ffffff7f00000000000000000000000040420f00ffff000000"]
    # hostapd_wmm_action - could not parse wmm action
    tests += ["11000400dd3d0050f2020201aa300010270000000000000000000000000000000000000000000000000000ffffff7f00000000000000000000000040420f00ffff00"]
    # valid form
    tests += ["11000400dd3d0050f2020201aa300010270000000000000000000000000000000000000000000000000000ffffff7f00000000000000000000000040420f00ffff0000"]

    hdr = "d0003a01" + bssid.replace(':', '') + addr.replace(':', '') + bssid.replace(':', '') + "1000"
    hapd.set("ext_mgmt_frame_handling", "1")
    for t in tests:
        if "OK" not in hapd.request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=" + hdr + t):
            raise Exception("MGMT_RX_PROCESS failed")

    hapd.set("ext_mgmt_frame_handling", "0")

def test_wmm_disabled(dev, apdev):
    """WMM disabled and unexpected TSPEC"""
    params = {"ssid": "no-wmm", "ieee80211n": "0", "wmm_enabled": "0"}
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)
    bssid = hapd.own_addr()
    dev[0].connect("no-wmm", key_mgmt="NONE", scan_freq="2412")
    addr = dev[0].own_addr()

    # wmm action received is not from associated wmm station
    hdr = "d0003a01" + bssid.replace(':', '') + addr.replace(':', '') + bssid.replace(':', '') + "1000"
    hapd.set("ext_mgmt_frame_handling", "1")
    if "OK" not in hapd.request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=" + hdr + "11000400dd3d0050f2020201aa300010270000000000000000000000000000000000000000000000000000ffffff7f00000000000000000000000040420f00ffff0000"):
        raise Exception("MGMT_RX_PROCESS failed")

    # IEEE 802.11: Ignored Action frame (category=17) from unassociated STA
    hdr = "d0003a01" + bssid.replace(':', '') + "112233445566" + bssid.replace(':', '') + "1000"
    if "OK" not in hapd.request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=" + hdr + "11000400dd3d0050f2020201aa300010270000000000000000000000000000000000000000000000000000ffffff7f00000000000000000000000040420f00ffff0000"):
        raise Exception("MGMT_RX_PROCESS failed")

    hapd.set("ext_mgmt_frame_handling", "0")
