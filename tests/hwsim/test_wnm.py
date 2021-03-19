# WNM tests
# Copyright (c) 2013-2019, Jouni Malinen <j@w1.fi>
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

from remotehost import remote_compatible
import binascii
import struct
import time
import logging
logger = logging.getLogger()
import subprocess

import hostapd
from wpasupplicant import WpaSupplicant
from utils import *
from wlantest import Wlantest
from datetime import datetime

def clear_regdom_state(dev, hapd, hapd2):
        for i in range(0, 3):
            ev = dev[0].wait_event(["CTRL-EVENT-REGDOM-CHANGE"], timeout=0.5)
            if ev is None or "init=COUNTRY_IE" in ev:
                break
        if hapd:
            hapd.request("DISABLE")
        if hapd2:
            hapd2.request("DISABLE")
        subprocess.call(['iw', 'reg', 'set', '00'])
        dev[0].disconnect_and_stop_scan()
        subprocess.call(['iw', 'reg', 'set', '00'])
        dev[0].wait_event(["CTRL-EVENT-REGDOM-CHANGE"], timeout=0.5)
        dev[0].flush_scan_cache()

def start_wnm_ap(apdev, bss_transition=True, time_adv=False, ssid=None,
                 wnm_sleep_mode=False, wnm_sleep_mode_no_keys=False, rsn=False,
                 ocv=False, ap_max_inactivity=0, coloc_intf_reporting=False,
                 hw_mode=None, channel=None, country_code=None, country3=None,
                 pmf=True, passphrase=None, ht=True, vht=False, mbo=False):
    if rsn:
        if not ssid:
            ssid = "test-wnm-rsn"
        if not passphrase:
            passphrase = "12345678"
        params = hostapd.wpa2_params(ssid, passphrase)
        if pmf:
            params["wpa_key_mgmt"] = "WPA-PSK-SHA256"
            params["ieee80211w"] = "2"
    else:
        params = {"ssid": "test-wnm"}
    if bss_transition:
        params["bss_transition"] = "1"
    if time_adv:
        params["time_advertisement"] = "2"
        params["time_zone"] = "EST5"
    if wnm_sleep_mode:
        params["wnm_sleep_mode"] = "1"
    if wnm_sleep_mode_no_keys:
        params["wnm_sleep_mode_no_keys"] = "1"
    if ocv:
        params["ocv"] = "1"
    if ap_max_inactivity:
        params["ap_max_inactivity"] = str(ap_max_inactivity)
    if coloc_intf_reporting:
        params["coloc_intf_reporting"] = "1"
    if hw_mode:
        params["hw_mode"] = hw_mode
    if channel:
        params["channel"] = channel
    if country_code:
        params["country_code"] = country_code
        params["ieee80211d"] = "1"
    if country3:
        params["country3"] = country3
    if not ht:
        params['ieee80211n'] = '0'
    if vht:
        params['ieee80211ac'] = "1"
        params["vht_oper_chwidth"] = "0"
        params["vht_oper_centr_freq_seg0_idx"] = "0"
    if mbo:
        params["mbo"] = "1"
    try:
        hapd = hostapd.add_ap(apdev, params)
    except Exception as e:
        if "Failed to set hostapd parameter ocv" in str(e):
            raise HwsimSkip("OCV not supported")
        raise
    if rsn:
        Wlantest.setup(hapd)
        wt = Wlantest()
        wt.flush()
        wt.add_passphrase("12345678")
    return hapd

@remote_compatible
def test_wnm_bss_transition_mgmt(dev, apdev):
    """WNM BSS Transition Management"""
    start_wnm_ap(apdev[0], time_adv=True, wnm_sleep_mode=True)
    dev[0].connect("test-wnm", key_mgmt="NONE", scan_freq="2412")
    dev[0].request("WNM_BSS_QUERY 0")

def test_wnm_bss_transition_mgmt_oom(dev, apdev):
    """WNM BSS Transition Management OOM"""
    hapd = start_wnm_ap(apdev[0])
    dev[0].connect("test-wnm", key_mgmt="NONE", scan_freq="2412")
    with alloc_fail(hapd, 1, "ieee802_11_send_bss_trans_mgmt_request"):
        dev[0].request("WNM_BSS_QUERY 0")
        wait_fail_trigger(hapd, "GET_ALLOC_FAIL")

@remote_compatible
def test_wnm_disassoc_imminent(dev, apdev):
    """WNM Disassociation Imminent"""
    hapd = start_wnm_ap(apdev[0], time_adv=True, wnm_sleep_mode=True)
    dev[0].connect("test-wnm", key_mgmt="NONE", scan_freq="2412")
    addr = dev[0].p2p_interface_addr()
    hapd.request("DISASSOC_IMMINENT " + addr + " 10")
    ev = dev[0].wait_event(["WNM: Disassociation Imminent"])
    if ev is None:
        raise Exception("Timeout while waiting for disassociation imminent")
    if "Disassociation Timer 10" not in ev:
        raise Exception("Unexpected disassociation imminent contents")
    ev = dev[0].wait_event(["CTRL-EVENT-SCAN-RESULTS"])
    if ev is None:
        raise Exception("Timeout while waiting for re-connection scan")

def test_wnm_disassoc_imminent_fail(dev, apdev):
    """WNM Disassociation Imminent failure"""
    hapd = start_wnm_ap(apdev[0])
    dev[0].connect("test-wnm", key_mgmt="NONE", scan_freq="2412")
    addr = dev[0].own_addr()
    with fail_test(hapd, 1, "wnm_send_disassoc_imminent"):
        if "FAIL" not in hapd.request("DISASSOC_IMMINENT " + addr + " 10"):
            raise Exception("DISASSOC_IMMINENT succeeded during failure testing")

@remote_compatible
def test_wnm_ess_disassoc_imminent(dev, apdev):
    """WNM ESS Disassociation Imminent"""
    hapd = start_wnm_ap(apdev[0], time_adv=True, wnm_sleep_mode=True)
    dev[0].connect("test-wnm", key_mgmt="NONE", scan_freq="2412")
    addr = dev[0].p2p_interface_addr()
    hapd.request("ESS_DISASSOC " + addr + " 10 http://example.com/session-info")
    ev = dev[0].wait_event(["ESS-DISASSOC-IMMINENT"])
    if ev is None:
        raise Exception("Timeout while waiting for ESS disassociation imminent")
    if "0 1024 http://example.com/session-info" not in ev:
        raise Exception("Unexpected ESS disassociation imminent message contents")
    ev = dev[0].wait_event(["CTRL-EVENT-SCAN-RESULTS"])
    if ev is None:
        raise Exception("Timeout while waiting for re-connection scan")

def test_wnm_ess_disassoc_imminent_fail(dev, apdev):
    """WNM ESS Disassociation Imminent failure"""
    hapd = start_wnm_ap(apdev[0])
    dev[0].connect("test-wnm", key_mgmt="NONE", scan_freq="2412")
    addr = dev[0].own_addr()
    if "FAIL" not in hapd.request("ESS_DISASSOC " + addr + " 10 http://" + 256*'a'):
        raise Exception("Invalid ESS_DISASSOC URL accepted")
    with fail_test(hapd, 1, "wnm_send_ess_disassoc_imminent"):
        if "FAIL" not in hapd.request("ESS_DISASSOC " + addr + " 10 http://example.com/session-info"):
            raise Exception("ESS_DISASSOC succeeded during failure testing")

def test_wnm_ess_disassoc_imminent_reject(dev, apdev):
    """WNM ESS Disassociation Imminent getting rejected"""
    hapd = start_wnm_ap(apdev[0])
    dev[0].connect("test-wnm", key_mgmt="NONE", scan_freq="2412")
    addr = dev[0].own_addr()
    if "OK" not in dev[0].request("SET reject_btm_req_reason 123"):
        raise Exception("Failed to set reject_btm_req_reason")

    hapd.request("ESS_DISASSOC " + addr + " 1 http://example.com/session-info")
    ev = hapd.wait_event(["BSS-TM-RESP"], timeout=10)
    if ev is None:
        raise Exception("BSS-TM-RESP not seen")
    if "status_code=123" not in ev:
        raise Exception("Unexpected response status: " + ev)
    dev[0].wait_disconnected()
    dev[0].request("DISCONNECT")

@remote_compatible
def test_wnm_ess_disassoc_imminent_pmf(dev, apdev):
    """WNM ESS Disassociation Imminent"""
    hapd = start_wnm_ap(apdev[0], rsn=True)
    dev[0].connect("test-wnm-rsn", psk="12345678", ieee80211w="2",
                   key_mgmt="WPA-PSK-SHA256", proto="WPA2", scan_freq="2412")
    addr = dev[0].p2p_interface_addr()
    hapd.request("ESS_DISASSOC " + addr + " 10 http://example.com/session-info")
    ev = dev[0].wait_event(["ESS-DISASSOC-IMMINENT"])
    if ev is None:
        raise Exception("Timeout while waiting for ESS disassociation imminent")
    if "1 1024 http://example.com/session-info" not in ev:
        raise Exception("Unexpected ESS disassociation imminent message contents")
    ev = dev[0].wait_event(["CTRL-EVENT-SCAN-RESULTS"])
    if ev is None:
        raise Exception("Timeout while waiting for re-connection scan")

def check_wnm_sleep_mode_enter_exit(hapd, dev, interval=None, tfs_req=None):
    addr = dev.p2p_interface_addr()
    sta = hapd.get_sta(addr)
    if "[WNM_SLEEP_MODE]" in sta['flags']:
        raise Exception("Station unexpectedly in WNM-Sleep Mode")

    logger.info("Going to WNM Sleep Mode")
    extra = ""
    if interval is not None:
        extra += " interval=" + str(interval)
    if tfs_req:
        extra += " tfs_req=" + tfs_req
    if "OK" not in dev.request("WNM_SLEEP enter" + extra):
        raise Exception("WNM_SLEEP failed")
    ok = False
    for i in range(20):
        time.sleep(0.1)
        sta = hapd.get_sta(addr)
        if "[WNM_SLEEP_MODE]" in sta['flags']:
            ok = True
            break
    if not ok:
        raise Exception("Station failed to enter WNM-Sleep Mode")

    logger.info("Waking up from WNM Sleep Mode")
    ok = False
    dev.request("WNM_SLEEP exit")
    for i in range(20):
        time.sleep(0.1)
        sta = hapd.get_sta(addr)
        if "[WNM_SLEEP_MODE]" not in sta['flags']:
            ok = True
            break
    if not ok:
        raise Exception("Station failed to exit WNM-Sleep Mode")

@remote_compatible
def test_wnm_sleep_mode_open(dev, apdev):
    """WNM Sleep Mode - open"""
    hapd = start_wnm_ap(apdev[0], time_adv=True, wnm_sleep_mode=True)
    dev[0].connect("test-wnm", key_mgmt="NONE", scan_freq="2412")
    ev = hapd.wait_event(["AP-STA-CONNECTED"], timeout=5)
    if ev is None:
        raise Exception("No connection event received from hostapd")
    check_wnm_sleep_mode_enter_exit(hapd, dev[0])
    check_wnm_sleep_mode_enter_exit(hapd, dev[0], interval=100)
    check_wnm_sleep_mode_enter_exit(hapd, dev[0], tfs_req="5b17010001130e110000071122334455661122334455661234")

    cmds = ["foo",
            "exit tfs_req=123 interval=10",
            "enter tfs_req=qq interval=10"]
    for cmd in cmds:
        if "FAIL" not in dev[0].request("WNM_SLEEP " + cmd):
            raise Exception("Invalid WNM_SLEEP accepted")

def test_wnm_sleep_mode_open_fail(dev, apdev):
    """WNM Sleep Mode - open (fail)"""
    hapd = start_wnm_ap(apdev[0], wnm_sleep_mode=True)
    dev[0].connect("test-wnm", key_mgmt="NONE", scan_freq="2412")
    with fail_test(hapd, 1, "nl80211_send_frame_cmd;ieee802_11_send_wnmsleep_resp"):
        dev[0].request("WNM_SLEEP enter")
        wait_fail_trigger(hapd, "GET_FAIL")

def test_wnm_sleep_mode_disabled_on_ap(dev, apdev):
    """WNM Sleep Mode disabled on AP"""
    hapd = start_wnm_ap(apdev[0], wnm_sleep_mode=False)
    dev[0].connect("test-wnm", key_mgmt="NONE", scan_freq="2412")
    # Ignore WNM-Sleep Mode Request from 02:00:00:00:00:00 since WNM-Sleep Mode is disabled
    dev[0].request("WNM_SLEEP enter")
    time.sleep(0.1)

@remote_compatible
def test_wnm_sleep_mode_rsn(dev, apdev):
    """WNM Sleep Mode - RSN"""
    hapd = start_wnm_ap(apdev[0], time_adv=True, wnm_sleep_mode=True, rsn=True,
                        pmf=False)
    dev[0].connect("test-wnm-rsn", psk="12345678", scan_freq="2412")
    ev = hapd.wait_event(["AP-STA-CONNECTED"], timeout=5)
    if ev is None:
        raise Exception("No connection event received from hostapd")
    check_wnm_sleep_mode_enter_exit(hapd, dev[0])

@remote_compatible
def test_wnm_sleep_mode_ap_oom(dev, apdev):
    """WNM Sleep Mode - AP side OOM"""
    hapd = start_wnm_ap(apdev[0], bss_transition=False, wnm_sleep_mode=True)
    dev[0].connect("test-wnm", key_mgmt="NONE", scan_freq="2412")
    ev = hapd.wait_event(["AP-STA-CONNECTED"], timeout=5)
    if ev is None:
        raise Exception("No connection event received from hostapd")
    with alloc_fail(hapd, 1, "ieee802_11_send_wnmsleep_resp"):
        dev[0].request("WNM_SLEEP enter")
        wait_fail_trigger(hapd, "GET_ALLOC_FAIL")
    with alloc_fail(hapd, 2, "ieee802_11_send_wnmsleep_resp"):
        dev[0].request("WNM_SLEEP exit")
        wait_fail_trigger(hapd, "GET_ALLOC_FAIL")

@remote_compatible
def test_wnm_sleep_mode_rsn_pmf(dev, apdev):
    """WNM Sleep Mode - RSN with PMF"""
    hapd = start_wnm_ap(apdev[0], rsn=True, wnm_sleep_mode=True, time_adv=True)
    dev[0].connect("test-wnm-rsn", psk="12345678", ieee80211w="2",
                   key_mgmt="WPA-PSK-SHA256", proto="WPA2", scan_freq="2412")
    ev = hapd.wait_event(["AP-STA-CONNECTED"], timeout=5)
    if ev is None:
        raise Exception("No connection event received from hostapd")
    check_wnm_sleep_mode_enter_exit(hapd, dev[0])

@remote_compatible
def test_wnm_sleep_mode_rsn_ocv(dev, apdev):
    """WNM Sleep Mode - RSN with OCV"""
    hapd = start_wnm_ap(apdev[0], rsn=True, wnm_sleep_mode=True,
                        time_adv=True, ocv=True)

    dev[0].connect("test-wnm-rsn", psk="12345678", ieee80211w="2", ocv="1",
                   key_mgmt="WPA-PSK-SHA256", proto="WPA2", scan_freq="2412")
    ev = hapd.wait_event(["AP-STA-CONNECTED"], timeout=5)
    if ev is None:
        raise Exception("No connection event received from hostapd")
    check_wnm_sleep_mode_enter_exit(hapd, dev[0])

    # Check if OCV succeeded or failed
    ev = dev[0].wait_event(["OCV failed"], timeout=1)
    if ev is not None:
        raise Exception("OCI verification failed: " + ev)

@remote_compatible
def test_wnm_sleep_mode_rsn_badocv(dev, apdev):
    """WNM Sleep Mode - RSN with OCV and bad OCI elements"""
    ssid = "test-wnm-rsn"
    hapd = start_wnm_ap(apdev[0], rsn=True, wnm_sleep_mode=True, ocv=True)
    bssid = apdev[0]['bssid']
    dev[0].connect(ssid, psk="12345678", key_mgmt="WPA-PSK-SHA256", ocv="1",
                   proto="WPA2", ieee80211w="2", scan_freq="2412")
    dev[0].request("WNM_SLEEP enter")
    time.sleep(0.1)

    msg = {'fc': MGMT_SUBTYPE_ACTION << 4,
           'da': bssid,
           'sa': dev[0].own_addr(),
           'bssid': bssid}

    logger.debug("WNM Sleep Mode Request - Missing OCI element")
    msg['payload'] = struct.pack("<BBBBBBBHBB",
                                 ACTION_CATEG_WNM, WNM_ACT_SLEEP_MODE_REQ, 0,
                                 WLAN_EID_WNMSLEEP, 4, WNM_SLEEP_MODE_EXIT, 0, 0,
                                 WLAN_EID_TFS_REQ, 0)
    mgmt_tx(dev[0], "MGMT_TX {} {} freq=2412 wait_time=200 no_cck=1 action={}".format(
        msg['da'], msg['bssid'], binascii.hexlify(msg['payload']).decode()))
    ev = hapd.wait_event(["OCV failed"], timeout=5)
    if ev is None:
        raise Exception("AP did not report missing OCI element")

    logger.debug("WNM Sleep Mode Request - Bad OCI element")
    msg['payload'] = struct.pack("<BBBBBBBHBB",
                                 ACTION_CATEG_WNM, WNM_ACT_SLEEP_MODE_REQ, 0,
                                 WLAN_EID_WNMSLEEP, 4, WNM_SLEEP_MODE_EXIT, 0,
                                 0,
                                 WLAN_EID_TFS_REQ, 0)
    oci_ie = struct.pack("<BBB", 81, 2, 0)
    msg['payload'] += struct.pack("<BBB", WLAN_EID_EXTENSION, 1 + len(oci_ie),
                                  WLAN_EID_EXT_OCV_OCI) + oci_ie
    mgmt_tx(dev[0], "MGMT_TX {} {} freq=2412 wait_time=200 no_cck=1 action={}".format(
        msg['da'], msg['bssid'], binascii.hexlify(msg['payload']).decode()))
    ev = hapd.wait_event(["OCV failed"], timeout=5)
    if ev is None:
        raise Exception("AP did not report bad OCI element")

    msg = {'fc': MGMT_SUBTYPE_ACTION << 4,
           'da': dev[0].own_addr(),
           'sa': bssid,
           'bssid': bssid}
    hapd.set("ext_mgmt_frame_handling", "1")

    logger.debug("WNM Sleep Mode Response - Missing OCI element")
    msg['payload'] = struct.pack("<BBBHBBBBHBB",
                                 ACTION_CATEG_WNM, WNM_ACT_SLEEP_MODE_RESP, 0,
                                 0,
                                 WLAN_EID_WNMSLEEP, 4, WNM_SLEEP_MODE_EXIT,
                                 WNM_STATUS_SLEEP_ACCEPT, 0,
                                 WLAN_EID_TFS_RESP, 0)
    dev[0].request("WNM_SLEEP exit")
    hapd.mgmt_tx(msg)
    expect_ack(hapd)
    ev = dev[0].wait_event(["OCV failed"], timeout=5)
    if ev is None:
        raise Exception("STA did not report missing OCI element")

    logger.debug("WNM Sleep Mode Response - Bad OCI element")
    msg['payload'] = struct.pack("<BBBHBBBBHBB",
                                 ACTION_CATEG_WNM, WNM_ACT_SLEEP_MODE_RESP, 0,
                                 0,
                                 WLAN_EID_WNMSLEEP, 4, WNM_SLEEP_MODE_EXIT,
                                 WNM_STATUS_SLEEP_ACCEPT, 0,
                                 WLAN_EID_TFS_RESP, 0)
    oci_ie = struct.pack("<BBB", 81, 2, 0)
    msg['payload'] += struct.pack("<BBB", WLAN_EID_EXTENSION, 1 + len(oci_ie),
                                  WLAN_EID_EXT_OCV_OCI) + oci_ie
    hapd.mgmt_tx(msg)
    expect_ack(hapd)
    ev = dev[0].wait_event(["OCV failed"], timeout=5)
    if ev is None:
        raise Exception("STA did not report bad OCI element")

def test_wnm_sleep_mode_rsn_ocv_failure(dev, apdev):
    """WNM Sleep Mode - RSN with OCV - local failure"""
    hapd = start_wnm_ap(apdev[0], rsn=True, wnm_sleep_mode=True,
                        time_adv=True, ocv=True)

    dev[0].connect("test-wnm-rsn", psk="12345678", ieee80211w="2", ocv="1",
                   key_mgmt="WPA-PSK-SHA256", proto="WPA2", scan_freq="2412")
    # Failed to allocate buffer for OCI element in WNM-Sleep Mode frame
    with alloc_fail(hapd, 2, "ieee802_11_send_wnmsleep_resp"):
            if "OK" not in dev[0].request("WNM_SLEEP enter"):
                    raise Exception("WNM_SLEEP failed")
            wait_fail_trigger(hapd, "GET_ALLOC_FAIL")

def test_wnm_sleep_mode_rsn_pmf_key_workaround(dev, apdev):
    """WNM Sleep Mode - RSN with PMF and GTK/IGTK workaround"""
    hapd = start_wnm_ap(apdev[0], rsn=True, wnm_sleep_mode=True,
                        wnm_sleep_mode_no_keys=True,
                        time_adv=True, ocv=True)
    dev[0].connect("test-wnm-rsn", psk="12345678", ieee80211w="2",
                   key_mgmt="WPA-PSK-SHA256", proto="WPA2", scan_freq="2412")
    ev = hapd.wait_event(["AP-STA-CONNECTED"], timeout=5)
    if ev is None:
        raise Exception("No connection event received from hostapd")
    check_wnm_sleep_mode_enter_exit(hapd, dev[0])

def test_wnm_sleep_mode_proto(dev, apdev):
    """WNM Sleep Mode - protocol testing"""
    hapd = start_wnm_ap(apdev[0], wnm_sleep_mode=True, bss_transition=False)
    bssid = hapd.own_addr()
    dev[0].connect("test-wnm", key_mgmt="NONE", scan_freq="2412")
    addr = dev[0].own_addr()

    hdr = "d0003a01" + bssid.replace(':', '') + addr.replace(':', '') + bssid.replace(':', '') + "1000"
    hapd.set("ext_mgmt_frame_handling", "1")
    tests = ["0a10",
             "0a1001",
             "0a10015d00",
             "0a10015d01",
             "0a10015d0400000000",
             "0a1001" + 7*("5bff" + 255*"00") + "5d00",
             "0a1001ff00"]
    for t in tests:
        if "OK" not in hapd.request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=" + hdr + t):
            raise Exception("MGMT_RX_PROCESS failed")

    hapd.set("ext_mgmt_frame_handling", "0")

MGMT_SUBTYPE_ACTION = 13
ACTION_CATEG_WNM = 10
WNM_ACT_BSS_TM_REQ = 7
WNM_ACT_BSS_TM_RESP = 8
WNM_ACT_SLEEP_MODE_REQ = 16
WNM_ACT_SLEEP_MODE_RESP = 17
WNM_ACT_NOTIFICATION_REQ = 26
WNM_ACT_NOTIFICATION_RESP = 27
WNM_NOTIF_TYPE_FW_UPGRADE = 0
WNM_NOTIF_TYPE_WFA = 1
WLAN_EID_TFS_REQ = 91
WLAN_EID_TFS_RESP = 92
WLAN_EID_WNMSLEEP = 93
WLAN_EID_EXTENSION = 255
WLAN_EID_EXT_OCV_OCI = 54
WNM_SLEEP_MODE_ENTER = 0
WNM_SLEEP_MODE_EXIT = 1
WNM_STATUS_SLEEP_ACCEPT = 0
WNM_STATUS_SLEEP_EXIT_ACCEPT_GTK_UPDATE = 1
WNM_STATUS_DENIED_ACTION = 2
WNM_STATUS_DENIED_TMP = 3
WNM_STATUS_DENIED_KEY = 4
WNM_STATUS_DENIED_OTHER_WNM_SERVICE = 5
WNM_SLEEP_SUBELEM_GTK = 0
WNM_SLEEP_SUBELEM_IGTK = 1

def bss_tm_req(dst, src, dialog_token=1, req_mode=0, disassoc_timer=0,
               validity_interval=1):
    msg = {}
    msg['fc'] = MGMT_SUBTYPE_ACTION << 4
    msg['da'] = dst
    msg['sa'] = src
    msg['bssid'] = src
    msg['payload'] = struct.pack("<BBBBHB",
                                 ACTION_CATEG_WNM, WNM_ACT_BSS_TM_REQ,
                                 dialog_token, req_mode, disassoc_timer,
                                 validity_interval)
    return msg

def rx_bss_tm_resp(hapd, expect_dialog=None, expect_status=None):
    for i in range(0, 100):
        resp = hapd.mgmt_rx()
        if resp is None:
            raise Exception("No BSS TM Response received")
        if resp['subtype'] == MGMT_SUBTYPE_ACTION:
            break
    if i == 99:
        raise Exception("Not an Action frame")
    payload = resp['payload']
    if len(payload) < 2 + 3:
        raise Exception("Too short payload")
    (category, action) = struct.unpack('BB', payload[0:2])
    if category != ACTION_CATEG_WNM or action != WNM_ACT_BSS_TM_RESP:
        raise Exception("Not a BSS TM Response")
    pos = payload[2:]
    (dialog, status, bss_term_delay) = struct.unpack('BBB', pos[0:3])
    resp['dialog'] = dialog
    resp['status'] = status
    resp['bss_term_delay'] = bss_term_delay
    pos = pos[3:]
    if len(pos) >= 6 and status == 0:
        resp['target_bssid'] = binascii.hexlify(pos[0:6])
        pos = pos[6:]
    resp['candidates'] = pos
    if expect_dialog is not None and dialog != expect_dialog:
        raise Exception("Unexpected dialog token")
    if expect_status is not None and status != expect_status:
        raise Exception("Unexpected status code %d" % status)
    return resp

def expect_ack(hapd):
    ev = hapd.wait_event(["MGMT-TX-STATUS"], timeout=5)
    if ev is None:
        raise Exception("Missing TX status")
    if "ok=1" not in ev:
        raise Exception("Action frame not acknowledged")

def mgmt_tx(dev, msg):
    if "FAIL" in dev.request(msg):
        raise Exception("Failed to send Action frame")
    ev = dev.wait_event(["MGMT-TX-STATUS"], timeout=10)
    if ev is None:
        raise Exception("Timeout on MGMT-TX-STATUS")
    if "result=SUCCESS" not in ev:
        raise Exception("Peer did not ack Action frame")

@remote_compatible
def test_wnm_bss_tm_req(dev, apdev):
    """BSS Transition Management Request"""
    hapd = start_wnm_ap(apdev[0])
    dev[0].connect("test-wnm", key_mgmt="NONE", scan_freq="2412")
    addr = dev[0].own_addr()

    hapd.set("ext_mgmt_frame_handling", "1")

    # truncated BSS TM Request
    req = bss_tm_req(addr, apdev[0]['bssid'],
                     req_mode=0x08)
    req['payload'] = struct.pack("<BBBBH",
                                 ACTION_CATEG_WNM, WNM_ACT_BSS_TM_REQ,
                                 1, 0, 0)
    hapd.mgmt_tx(req)
    expect_ack(hapd)
    dev[0].dump_monitor()

    # no disassociation and no candidate list
    req = bss_tm_req(addr, apdev[0]['bssid'],
                     dialog_token=2)
    hapd.mgmt_tx(req)
    resp = rx_bss_tm_resp(hapd, expect_dialog=2, expect_status=1)
    dev[0].dump_monitor()

    # truncated BSS Termination Duration
    req = bss_tm_req(addr, apdev[0]['bssid'],
                     req_mode=0x08)
    hapd.mgmt_tx(req)
    expect_ack(hapd)
    dev[0].dump_monitor()

    # BSS Termination Duration with TSF=0 and Duration=10
    req = bss_tm_req(addr, apdev[0]['bssid'],
                     req_mode=0x08, dialog_token=3)
    req['payload'] += struct.pack("<BBQH", 4, 10, 0, 10)
    hapd.mgmt_tx(req)
    resp = rx_bss_tm_resp(hapd, expect_dialog=3, expect_status=1)
    dev[0].dump_monitor()

    # truncated Session Information URL
    req = bss_tm_req(addr, apdev[0]['bssid'],
                     req_mode=0x10)
    hapd.mgmt_tx(req)
    expect_ack(hapd)
    req = bss_tm_req(addr, apdev[0]['bssid'],
                     req_mode=0x10)
    req['payload'] += struct.pack("<BBB", 3, 65, 66)
    hapd.mgmt_tx(req)
    expect_ack(hapd)
    dev[0].dump_monitor()

    # Session Information URL
    req = bss_tm_req(addr, apdev[0]['bssid'],
                     req_mode=0x10, dialog_token=4)
    req['payload'] += struct.pack("<BBB", 2, 65, 66)
    hapd.mgmt_tx(req)
    resp = rx_bss_tm_resp(hapd, expect_dialog=4, expect_status=0)
    dev[0].dump_monitor()

    # Preferred Candidate List without any entries
    req = bss_tm_req(addr, apdev[0]['bssid'],
                     req_mode=0x01, dialog_token=5)
    hapd.mgmt_tx(req)
    resp = rx_bss_tm_resp(hapd, expect_dialog=5, expect_status=7)
    dev[0].dump_monitor()

    # Preferred Candidate List with a truncated entry
    req = bss_tm_req(addr, apdev[0]['bssid'],
                     req_mode=0x01)
    req['payload'] += struct.pack("<BB", 52, 1)
    hapd.mgmt_tx(req)
    expect_ack(hapd)
    dev[0].dump_monitor()

    # Preferred Candidate List with a too short entry
    req = bss_tm_req(addr, apdev[0]['bssid'],
                     req_mode=0x01, dialog_token=6)
    req['payload'] += struct.pack("<BB", 52, 0)
    hapd.mgmt_tx(req)
    resp = rx_bss_tm_resp(hapd, expect_dialog=6, expect_status=7)
    dev[0].dump_monitor()

    # Preferred Candidate List with a non-matching entry
    req = bss_tm_req(addr, apdev[0]['bssid'],
                     req_mode=0x01, dialog_token=6)
    req['payload'] += struct.pack("<BB6BLBBB", 52, 13,
                                  1, 2, 3, 4, 5, 6,
                                  0, 81, 1, 7)
    hapd.mgmt_tx(req)
    resp = rx_bss_tm_resp(hapd, expect_dialog=6, expect_status=7)
    dev[0].dump_monitor()

    # Preferred Candidate List with a truncated subelement
    req = bss_tm_req(addr, apdev[0]['bssid'],
                     req_mode=0x01, dialog_token=7)
    req['payload'] += struct.pack("<BB6BLBBBBB", 52, 13 + 2,
                                  1, 2, 3, 4, 5, 6,
                                  0, 81, 1, 7,
                                  1, 1)
    hapd.mgmt_tx(req)
    resp = rx_bss_tm_resp(hapd, expect_dialog=7, expect_status=7)
    dev[0].dump_monitor()

    # Preferred Candidate List with lots of invalid optional subelements
    req = bss_tm_req(addr, apdev[0]['bssid'],
                     req_mode=0x01, dialog_token=8)
    subelems = struct.pack("<BBHB", 1, 3, 0, 100)
    subelems += struct.pack("<BBB", 2, 1, 65)
    subelems += struct.pack("<BB", 3, 0)
    subelems += struct.pack("<BBQB", 4, 9, 0, 10)
    subelems += struct.pack("<BBHLB", 5, 7, 0, 0, 0)
    subelems += struct.pack("<BB", 66, 0)
    subelems += struct.pack("<BBBBBB", 70, 4, 0, 0, 0, 0)
    subelems += struct.pack("<BB", 71, 0)
    req['payload'] += struct.pack("<BB6BLBBB", 52, 13 + len(subelems),
                                  1, 2, 3, 4, 5, 6,
                                  0, 81, 1, 7) + subelems
    hapd.mgmt_tx(req)
    resp = rx_bss_tm_resp(hapd, expect_dialog=8, expect_status=7)
    dev[0].dump_monitor()

    # Preferred Candidate List with lots of valid optional subelements (twice)
    req = bss_tm_req(addr, apdev[0]['bssid'],
                     req_mode=0x01, dialog_token=8)
    # TSF Information
    subelems = struct.pack("<BBHH", 1, 4, 0, 100)
    # Condensed Country String
    subelems += struct.pack("<BBBB", 2, 2, 65, 66)
    # BSS Transition Candidate Preference
    subelems += struct.pack("<BBB", 3, 1, 100)
    # BSS Termination Duration
    subelems += struct.pack("<BBQH", 4, 10, 0, 10)
    # Bearing
    subelems += struct.pack("<BBHLH", 5, 8, 0, 0, 0)
    # Measurement Pilot Transmission
    subelems += struct.pack("<BBBBB", 66, 3, 0, 0, 0)
    # RM Enabled Capabilities
    subelems += struct.pack("<BBBBBBB", 70, 5, 0, 0, 0, 0, 0)
    # Multiple BSSID
    subelems += struct.pack("<BBBB", 71, 2, 0, 0)
    req['payload'] += struct.pack("<BB6BLBBB", 52, 13 + len(subelems) * 2,
                                  1, 2, 3, 4, 5, 6,
                                  0, 81, 1, 7) + subelems + subelems
    hapd.mgmt_tx(req)
    resp = rx_bss_tm_resp(hapd, expect_dialog=8, expect_status=7)
    dev[0].dump_monitor()

    # Preferred Candidate List with truncated BSS Termination Duration
    # WNM: Too short BSS termination duration
    req = bss_tm_req(addr, apdev[0]['bssid'],
                     req_mode=0x01, dialog_token=8)
    # BSS Termination Duration (truncated)
    subelems = struct.pack("<BBQB", 4, 9, 0, 10)
    req['payload'] += struct.pack("<BB6BLBBB", 52, 13 + len(subelems),
                                  1, 2, 3, 4, 5, 6,
                                  0, 81, 1, 7) + subelems
    hapd.mgmt_tx(req)
    resp = rx_bss_tm_resp(hapd, expect_dialog=8, expect_status=7)
    dev[0].dump_monitor()

    # Preferred Candidate List followed by vendor element
    req = bss_tm_req(addr, apdev[0]['bssid'],
                     req_mode=0x01, dialog_token=8)
    subelems = b''
    req['payload'] += struct.pack("<BB6BLBBB", 52, 13 + len(subelems),
                                  1, 2, 3, 4, 5, 6,
                                  0, 81, 1, 7) + subelems
    req['payload'] += binascii.unhexlify("DD0411223344")
    hapd.mgmt_tx(req)
    resp = rx_bss_tm_resp(hapd, expect_dialog=8, expect_status=7)
    dev[0].dump_monitor()

@remote_compatible
def test_wnm_bss_keep_alive(dev, apdev):
    """WNM keep-alive"""
    hapd = start_wnm_ap(apdev[0], bss_transition=False, ap_max_inactivity=1)
    addr = dev[0].p2p_interface_addr()
    dev[0].connect("test-wnm", key_mgmt="NONE", scan_freq="2412")
    start = hapd.get_sta(addr)
    ev = dev[0].wait_event(["CTRL-EVENT-DISCONNECTED"], timeout=2)
    if ev is not None:
        raise Exception("Unexpected disconnection")
    end = hapd.get_sta(addr)
    if int(end['rx_packets']) <= int(start['rx_packets']):
        raise Exception("No keep-alive packets received")
    try:
        # Disable client keep-alive so that hostapd will verify connection
        # with client poll
        dev[0].request("SET no_keep_alive 1")
        for i in range(60):
            sta = hapd.get_sta(addr)
            logger.info("timeout_next=%s rx_packets=%s tx_packets=%s" % (sta['timeout_next'], sta['rx_packets'], sta['tx_packets']))
            if i > 1 and sta['timeout_next'] != "NULLFUNC POLL" and int(sta['tx_packets']) > int(end['tx_packets']):
                break
            ev = dev[0].wait_event(["CTRL-EVENT-DISCONNECTED"], timeout=0.5)
            if ev is not None:
                raise Exception("Unexpected disconnection (client poll expected)")
    finally:
        dev[0].request("SET no_keep_alive 0")
    if int(sta['tx_packets']) <= int(end['tx_packets']):
        raise Exception("No client poll packet seen")

def test_wnm_bss_tm(dev, apdev):
    """WNM BSS Transition Management"""
    try:
        hapd = None
        hapd2 = None
        hapd = start_wnm_ap(apdev[0], country_code="FI")
        id = dev[0].connect("test-wnm", key_mgmt="NONE", scan_freq="2412")
        dev[0].set_network(id, "scan_freq", "")

        hapd2 = start_wnm_ap(apdev[1], country_code="FI", hw_mode="a",
                             channel="36")

        addr = dev[0].p2p_interface_addr()
        dev[0].dump_monitor()

        logger.info("No neighbor list entries")
        if "OK" not in hapd.request("BSS_TM_REQ " + addr):
            raise Exception("BSS_TM_REQ command failed")
        ev = hapd.wait_event(['BSS-TM-RESP'], timeout=10)
        if ev is None:
            raise Exception("No BSS Transition Management Response")
        if addr not in ev:
            raise Exception("Unexpected BSS Transition Management Response address")
        if "status_code=0" in ev:
            raise Exception("BSS transition accepted unexpectedly")
        dev[0].dump_monitor()

        logger.info("Neighbor list entry, but not claimed as Preferred Candidate List")
        if "OK" not in hapd.request("BSS_TM_REQ " + addr + " neighbor=11:22:33:44:55:66,0x0000,81,3,7"):
            raise Exception("BSS_TM_REQ command failed")
        ev = hapd.wait_event(['BSS-TM-RESP'], timeout=10)
        if ev is None:
            raise Exception("No BSS Transition Management Response")
        if "status_code=0" in ev:
            raise Exception("BSS transition accepted unexpectedly")
        dev[0].dump_monitor()

        logger.info("Preferred Candidate List (no matching neighbor) without Disassociation Imminent")
        if "OK" not in hapd.request("BSS_TM_REQ " + addr + " pref=1 neighbor=11:22:33:44:55:66,0x0000,81,3,7,0301ff neighbor=22:33:44:55:66:77,0x0000,1,44,7 neighbor=00:11:22:33:44:55,0x0000,81,4,7,03010a"):
            raise Exception("BSS_TM_REQ command failed")
        ev = hapd.wait_event(['BSS-TM-RESP'], timeout=10)
        if ev is None:
            raise Exception("No BSS Transition Management Response")
        if "status_code=0" in ev:
            raise Exception("BSS transition accepted unexpectedly")
        ev = dev[0].wait_event(["CTRL-EVENT-SCAN-STARTED"], timeout=5)
        if ev is None:
            raise Exception("No scan started")
        dev[0].dump_monitor()

        logger.info("Preferred Candidate List (matching neighbor for another BSS) without Disassociation Imminent")
        if "OK" not in hapd.request("BSS_TM_REQ " + addr + " pref=1 abridged=1 valid_int=255 neighbor=" + apdev[1]['bssid'] + ",0x0000,115,36,7,0301ff"):
            raise Exception("BSS_TM_REQ command failed")
        ev = hapd.wait_event(['BSS-TM-RESP'], timeout=10)
        if ev is None:
            raise Exception("No BSS Transition Management Response")
        if "status_code=0" not in ev:
            raise Exception("BSS transition request was not accepted: " + ev)
        if "target_bssid=" + apdev[1]['bssid'] not in ev:
            raise Exception("Unexpected target BSS: " + ev)
        dev[0].wait_connected(timeout=15, error="No reassociation seen")
        if apdev[1]['bssid'] not in ev:
            raise Exception("Unexpected reassociation target: " + ev)
        ev = dev[0].wait_event(["CTRL-EVENT-SCAN-STARTED"], timeout=0.1)
        if ev is not None:
            raise Exception("Unexpected scan started")
        dev[0].dump_monitor()

        logger.info("Preferred Candidate List with two matches, no roam needed")
        if "OK" not in hapd2.request("BSS_TM_REQ " + addr + " pref=1 abridged=1 valid_int=255 neighbor=" + apdev[0]['bssid'] + ",0x0000,81,1,7,030101 neighbor=" + apdev[1]['bssid'] + ",0x0000,115,36,7,0301ff"):
            raise Exception("BSS_TM_REQ command failed")
        ev = hapd2.wait_event(['BSS-TM-RESP'], timeout=10)
        if ev is None:
            raise Exception("No BSS Transition Management Response")
        if "status_code=0" not in ev:
            raise Exception("BSS transition request was not accepted: " + ev)
        if "target_bssid=" + apdev[1]['bssid'] not in ev:
            raise Exception("Unexpected target BSS: " + ev)
        ev = dev[0].wait_event(["CTRL-EVENT-SCAN-STARTED"], timeout=0.1)
        if ev is not None:
            raise Exception("Unexpected scan started")
        ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED"], timeout=0.5)
        if ev is not None:
            raise Exception("Unexpected reassociation")

        logger.info("Preferred Candidate List with two matches and extra frequency (160 MHz), no roam needed")
        if "OK" not in hapd2.request("BSS_TM_REQ " + addr + " pref=1 abridged=1 valid_int=255 neighbor=" + apdev[0]['bssid'] + ",0x0000,81,1,7,030101 neighbor=" + apdev[1]['bssid'] + ",0x0000,115,36,7,0301ff" + ' neighbor=00:11:22:33:44:55,0x0000,129,36,7'):
            raise Exception("BSS_TM_REQ command failed")
        ev = hapd2.wait_event(['BSS-TM-RESP'], timeout=10)
        if ev is None:
            raise Exception("No BSS Transition Management Response")
        if "status_code=0" not in ev:
            raise Exception("BSS transition request was not accepted: " + ev)
        if "target_bssid=" + apdev[1]['bssid'] not in ev:
            raise Exception("Unexpected target BSS: " + ev)
        ev = dev[0].wait_event(["CTRL-EVENT-SCAN-STARTED"], timeout=0.1)
        if ev is not None:
            raise Exception("Unexpected scan started")
        ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED"], timeout=0.5)
        if ev is not None:
            raise Exception("Unexpected reassociation")
    finally:
        clear_regdom_state(dev, hapd, hapd2)

def test_wnm_bss_tm_steering_timeout(dev, apdev):
    """WNM BSS Transition Management and steering timeout"""
    hapd = start_wnm_ap(apdev[0])
    id = dev[0].connect("test-wnm", key_mgmt="NONE", scan_freq="2412")
    hapd2 = start_wnm_ap(apdev[1])
    dev[0].scan_for_bss(apdev[1]['bssid'], 2412)
    hapd2.disable()
    addr = dev[0].own_addr()
    if "OK" not in hapd.request("BSS_TM_REQ " + addr + " pref=1 abridged=1 valid_int=255 neighbor=" + apdev[1]['bssid'] + ",0x0000,81,1,7,0301ff"):
        raise Exception("BSS_TM_REQ command failed")
    ev = hapd.wait_event(['BSS-TM-RESP'], timeout=5)
    if ev is None:
        raise Exception("No BSS Transition Management Response")
    if "status_code=0" not in ev:
        raise Exception("BSS transition request was not accepted: " + ev)
    # Wait for the ap_sta_reset_steer_flag_timer timeout to occur
    # "Reset steering flag for STA 02:00:00:00:00:00"
    time.sleep(2.1)

    ev = dev[0].wait_event(["Trying to authenticate"], timeout=5)
    if ev is None:
        raise Exception("No authentication attempt seen")
    if hapd2.own_addr() not in ev:
        raise Exception("Unexpected authentication target: " + ev)
    # Wait for return back to the previous AP
    dev[0].wait_connected()

def test_wnm_bss_tm_errors(dev, apdev):
    """WNM BSS Transition Management errors"""
    hapd = start_wnm_ap(apdev[0])
    id = dev[0].connect("test-wnm", key_mgmt="NONE", scan_freq="2412")
    addr = dev[0].own_addr()

    tests = ["BSS_TM_REQ q",
             "BSS_TM_REQ 22:22:22:22:22:22",
             "BSS_TM_REQ %s disassoc_timer=-1" % addr,
             "BSS_TM_REQ %s disassoc_timer=65536" % addr,
             "BSS_TM_REQ %s bss_term=foo" % addr,
             "BSS_TM_REQ %s neighbor=q" % addr,
             "BSS_TM_REQ %s neighbor=02:11:22:33:44:55" % addr,
             "BSS_TM_REQ %s neighbor=02:11:22:33:44:55,0" % addr,
             "BSS_TM_REQ %s neighbor=02:11:22:33:44:55,0,0" % addr,
             "BSS_TM_REQ %s neighbor=02:11:22:33:44:55,0,0,0" % addr,
             "BSS_TM_REQ %s neighbor=02:11:22:33:44:55,0,0,0,0,q" % addr,
             "BSS_TM_REQ %s neighbor=02:11:22:33:44:55,0,0,0,0,0q" % addr,
             "BSS_TM_REQ " + addr + " url=" + 256*'a',
             "BSS_TM_REQ %s url=foo mbo=1:2" % addr,
             "BSS_TM_REQ %s url=foo mbo=100000:0:0" % addr,
             "BSS_TM_REQ %s url=foo mbo=0:0:254" % addr,
             "BSS_TM_REQ %s url=foo mbo=0:100000:0" % addr]
    for t in tests:
        if "FAIL" not in hapd.request(t):
            raise Exception("Invalid command accepted: %s" % t)

    with alloc_fail(hapd, 1, "=hostapd_ctrl_iface_bss_tm_req"):
        if "FAIL" not in hapd.request("BSS_TM_REQ %s url=http://foo" % addr):
            raise Exception("BSS_TM_REQ accepted during OOM")

    with alloc_fail(hapd, 1, "=wnm_send_bss_tm_req"):
        if "FAIL" not in hapd.request("BSS_TM_REQ %s url=http://foo" % addr):
            raise Exception("BSS_TM_REQ accepted during OOM")

    with fail_test(hapd, 1, "wnm_send_bss_tm_req"):
        if "FAIL" not in hapd.request("BSS_TM_REQ %s url=http://foo" % addr):
            raise Exception("BSS_TM_REQ accepted during failure testing")

def test_wnm_bss_tm_termination(dev, apdev):
    """WNM BSS Transition Management and BSS termination"""
    hapd = start_wnm_ap(apdev[0])
    id = dev[0].connect("test-wnm", key_mgmt="NONE", scan_freq="2412")
    addr = dev[0].own_addr()

    if "OK" not in hapd.request("BSS_TM_REQ %s bss_term=0,1" % addr):
        raise Exception("BSS_TM_REQ failed")
    ev = hapd.wait_event(["BSS-TM-RESP"], timeout=5)
    if ev is None:
        raise Exception("No BSS-TM-RESP event seen")

    if "OK" not in hapd.request("BSS_TM_REQ %s url=http://example.com/" % addr):
        raise Exception("BSS_TM_REQ failed")
    ev = hapd.wait_event(["BSS-TM-RESP"], timeout=5)
    if ev is None:
        raise Exception("No BSS-TM-RESP event seen")

def test_wnm_bss_tm_scan_not_needed(dev, apdev):
    """WNM BSS Transition Management and scan not needed"""
    run_wnm_bss_tm_scan_not_needed(dev, apdev)

def test_wnm_bss_tm_nei_vht(dev, apdev):
    """WNM BSS Transition Management and VHT neighbor"""
    run_wnm_bss_tm_scan_not_needed(dev, apdev, vht=True, nei_info="115,36,9")

def test_wnm_bss_tm_nei_11a(dev, apdev):
    """WNM BSS Transition Management and 11a neighbor"""
    run_wnm_bss_tm_scan_not_needed(dev, apdev, ht=False, nei_info="115,36,4")

def test_wnm_bss_tm_nei_11g(dev, apdev):
    """WNM BSS Transition Management and 11g neighbor"""
    run_wnm_bss_tm_scan_not_needed(dev, apdev, ht=False, hwmode='g',
                                   channel='2', freq=2417, nei_info="81,2,6")

def test_wnm_bss_tm_nei_11b(dev, apdev):
    """WNM BSS Transition Management and 11g neighbor"""
    run_wnm_bss_tm_scan_not_needed(dev, apdev, ht=False, hwmode='b',
                                   channel='3', freq=2422, nei_info="81,2,5")

def run_wnm_bss_tm_scan_not_needed(dev, apdev, ht=True, vht=False, hwmode='a',
                                   channel='36', freq=5180,
                                   nei_info="115,36,7,0301ff"):
    try:
        hapd = None
        hapd2 = None
        hapd = start_wnm_ap(apdev[0], country_code="FI", hw_mode="g",
                            channel="1")
        hapd2 = start_wnm_ap(apdev[1], country_code="FI", hw_mode=hwmode,
                             channel=channel, ht=ht, vht=vht)
        dev[0].scan_for_bss(apdev[1]['bssid'], freq)

        id = dev[0].connect("test-wnm", key_mgmt="NONE",
                            bssid=apdev[0]['bssid'], scan_freq="2412")
        dev[0].set_network(id, "scan_freq", "")
        dev[0].set_network(id, "bssid", "")

        addr = dev[0].own_addr()
        dev[0].dump_monitor()

        logger.info("Preferred Candidate List (matching neighbor for another BSS) without Disassociation Imminent")
        if "OK" not in hapd.request("BSS_TM_REQ " + addr + " pref=1 abridged=1 valid_int=255 neighbor=" + apdev[1]['bssid'] + ",0x0000," + nei_info):
            raise Exception("BSS_TM_REQ command failed")
        ev = hapd.wait_event(['BSS-TM-RESP'], timeout=10)
        if ev is None:
            raise Exception("No BSS Transition Management Response")
        if "status_code=0" not in ev:
            raise Exception("BSS transition request was not accepted: " + ev)
        if "target_bssid=" + apdev[1]['bssid'] not in ev:
            raise Exception("Unexpected target BSS: " + ev)
        dev[0].wait_connected(timeout=15, error="No reassociation seen")
        if apdev[1]['bssid'] not in ev:
            raise Exception("Unexpected reassociation target: " + ev)
        ev = dev[0].wait_event(["CTRL-EVENT-SCAN-STARTED"], timeout=0.1)
        if ev is not None:
            raise Exception("Unexpected scan started")
        dev[0].dump_monitor()
    finally:
        clear_regdom_state(dev, hapd, hapd2)

def test_wnm_bss_tm_scan_needed(dev, apdev):
    """WNM BSS Transition Management and scan needed"""
    try:
        hapd = None
        hapd2 = None
        hapd = start_wnm_ap(apdev[0], country_code="FI", hw_mode="g",
                            channel="1")
        hapd2 = start_wnm_ap(apdev[1], country_code="FI", hw_mode="a",
                             channel="36")

        dev[0].scan_for_bss(apdev[1]['bssid'], 5180)

        id = dev[0].connect("test-wnm", key_mgmt="NONE",
                            bssid=apdev[0]['bssid'], scan_freq="2412")
        dev[0].set_network(id, "scan_freq", "")
        dev[0].set_network(id, "bssid", "")

        addr = dev[0].own_addr()
        dev[0].dump_monitor()

        logger.info("Wait 11 seconds for the last scan result to be too old, but still present in BSS table")
        time.sleep(11)
        logger.info("Preferred Candidate List (matching neighbor for another BSS) without Disassociation Imminent")
        if "OK" not in hapd.request("BSS_TM_REQ " + addr + " pref=1 abridged=1 valid_int=255 neighbor=" + apdev[1]['bssid'] + ",0x0000,115,36,7,0301ff"):
            raise Exception("BSS_TM_REQ command failed")
        ev = hapd.wait_event(['BSS-TM-RESP'], timeout=10)
        if ev is None:
            raise Exception("No BSS Transition Management Response")
        if "status_code=0" not in ev:
            raise Exception("BSS transition request was not accepted: " + ev)
        if "target_bssid=" + apdev[1]['bssid'] not in ev:
            raise Exception("Unexpected target BSS: " + ev)
        dev[0].wait_connected(timeout=15, error="No reassociation seen")
        if apdev[1]['bssid'] not in ev:
            raise Exception("Unexpected reassociation target: " + ev)
        ev = dev[0].wait_event(["CTRL-EVENT-SCAN-STARTED"], timeout=0.1)
        if ev is not None:
            raise Exception("Unexpected scan started")
        dev[0].dump_monitor()
    finally:
        clear_regdom_state(dev, hapd, hapd2)

def test_wnm_bss_tm_scan_needed_e4(dev, apdev):
    """WNM BSS Transition Management and scan needed (Table E-4)"""
    try:
        hapd = None
        hapd2 = None
        hapd = start_wnm_ap(apdev[0], country_code="FI", country3="0x04",
                            hw_mode="g", channel="1")
        hapd2 = start_wnm_ap(apdev[1], country_code="FI", country3="0x04",
                             hw_mode="a", channel="36")
        id = dev[0].connect("test-wnm", key_mgmt="NONE",
                            bssid=apdev[0]['bssid'], scan_freq="2412")
        dev[0].set_network(id, "scan_freq", "")
        dev[0].set_network(id, "bssid", "")

        addr = dev[0].own_addr()
        dev[0].dump_monitor()

        logger.info("Preferred Candidate List (matching neighbor for another BSS) without Disassociation Imminent")
        if "OK" not in hapd.request("BSS_TM_REQ " + addr + " pref=1 abridged=1 valid_int=255 neighbor=" + apdev[1]['bssid'] + ",0x0000,115,36,7,0301ff"):
            raise Exception("BSS_TM_REQ command failed")
        ev = hapd.wait_event(['BSS-TM-RESP'], timeout=4)
        if ev is None:
            raise Exception("No BSS Transition Management Response seen quickly enough - did scan optimization fail?")
        if "status_code=0" not in ev:
            raise Exception("BSS transition request was not accepted: " + ev)
        dev[0].wait_connected(timeout=15, error="No reassociation seen")
        # Wait for regdom change due to country IE to avoid issues with that
        # processing happening only after the disconnection and cfg80211 ending
        # up intersecting regdoms when we try to clear state back to world (00)
        # regdom below.
        while True:
            ev = dev[0].wait_event(["CTRL-EVENT-REGDOM-CHANGE"], timeout=0.5)
            if not ev or "COUNTRY_IE" in ev:
                break
        dev[0].dump_monitor()
    finally:
        clear_regdom_state(dev, hapd, hapd2)

def start_wnm_tm(ap, country, dev, country3=None):
    hapd = start_wnm_ap(ap, country_code=country, country3=country3)
    id = dev.connect("test-wnm", key_mgmt="NONE", scan_freq="2412")
    wait_regdom_changes(dev)
    dev.dump_monitor()
    dev.set_network(id, "scan_freq", "")
    return hapd, id

def stop_wnm_tm(hapd, dev):
    if hapd:
        hapd.request("DISABLE")
        time.sleep(0.1)
    dev[0].disconnect_and_stop_scan()
    subprocess.call(['iw', 'reg', 'set', '00'])
    wait_regdom_changes(dev[0])
    country = dev[0].get_driver_status_field("country")
    logger.info("Country code at the end: " + country)
    if country != "00":
        clear_country(dev)

    dev[0].flush_scan_cache()

def wnm_bss_tm_check(hapd, dev, data):
    addr = dev.p2p_interface_addr()
    if "OK" not in hapd.request("BSS_TM_REQ " + addr + " " + data):
        raise Exception("BSS_TM_REQ command failed")
    ev = dev.wait_event(["CTRL-EVENT-SCAN-STARTED"], timeout=5)
    if ev is None:
        raise Exception("No scan started")
    ev = dev.wait_event(["CTRL-EVENT-SCAN-RESULTS"], 15)
    if ev is None:
        raise Exception("Scan did not complete")

    ev = hapd.wait_event(['BSS-TM-RESP'], timeout=10)
    if ev is None:
        raise Exception("No BSS Transition Management Response")
    if "status_code=7" not in ev:
        raise Exception("Unexpected response: " + ev)

def test_wnm_bss_tm_country_us(dev, apdev):
    """WNM BSS Transition Management (US)"""
    try:
        hapd = None
        hapd, id = start_wnm_tm(apdev[0], "US", dev[0])

        logger.info("Preferred Candidate List (no matching neighbor, known channels)")
        wnm_bss_tm_check(hapd, dev[0], "pref=1 neighbor=11:22:33:44:55:66,0x0000,12,3,7,0301ff neighbor=00:11:22:33:44:55,0x0000,2,52,7,03010a neighbor=00:11:22:33:44:57,0x0000,4,100,7 neighbor=00:11:22:33:44:59,0x0000,3,149,7 neighbor=00:11:22:33:44:5b,0x0000,34,1,7 neighbor=00:11:22:33:44:5d,0x0000,5,149,7")

        # Make the test take less time by limiting full scans
        dev[0].set_network(id, "scan_freq", "2412")
        logger.info("Preferred Candidate List (no matching neighbor, unknown channels)")
        wnm_bss_tm_check(hapd, dev[0], "pref=1 neighbor=11:22:33:44:55:66,0x0000,12,0,7,0301ff neighbor=22:33:44:55:66:77,0x0000,12,12,7 neighbor=00:11:22:33:44:55,0x0000,2,35,7,03010a neighbor=00:11:22:33:44:56,0x0000,2,65,7 neighbor=00:11:22:33:44:57,0x0000,4,99,7 neighbor=00:11:22:33:44:58,0x0000,4,145,7")

        logger.info("Preferred Candidate List (no matching neighbor, unknown channels 2)")
        wnm_bss_tm_check(hapd, dev[0], "pref=1 neighbor=00:11:22:33:44:59,0x0000,3,148,7 neighbor=00:11:22:33:44:5a,0x0000,3,162,7 neighbor=00:11:22:33:44:5b,0x0000,34,0,7 neighbor=00:11:22:33:44:5c,0x0000,34,4,7 neighbor=00:11:22:33:44:5d,0x0000,5,148,7 neighbor=00:11:22:33:44:5e,0x0000,5,166,7 neighbor=00:11:22:33:44:5f,0x0000,0,0,7")
    finally:
        stop_wnm_tm(hapd, dev)

def test_wnm_bss_tm_country_fi(dev, apdev):
    """WNM BSS Transition Management (FI)"""
    addr = dev[0].p2p_interface_addr()
    try:
        hapd = None
        hapd, id = start_wnm_tm(apdev[0], "FI", dev[0])

        logger.info("Preferred Candidate List (no matching neighbor, known channels)")
        wnm_bss_tm_check(hapd, dev[0], "pref=1 neighbor=11:22:33:44:55:66,0x0000,4,3,7,0301ff neighbor=00:11:22:33:44:55,0x0000,1,36,7,03010a neighbor=00:11:22:33:44:57,0x0000,3,100,7 neighbor=00:11:22:33:44:59,0x0000,17,149,7 neighbor=00:11:22:33:44:5c,0x0000,18,1,7")

        # Make the test take less time by limiting full scans
        dev[0].set_network(id, "scan_freq", "2412")
        logger.info("Preferred Candidate List (no matching neighbor, unknown channels)")
        wnm_bss_tm_check(hapd, dev[0], "pref=1 neighbor=00:11:22:33:44:00,0x0000,4,0,7 neighbor=00:11:22:33:44:01,0x0000,4,14,7 neighbor=00:11:22:33:44:02,0x0000,1,35,7 neighbor=00:11:22:33:44:03,0x0000,1,65,7 neighbor=00:11:22:33:44:04,0x0000,3,99,7 neighbor=00:11:22:33:44:05,0x0000,3,141,7 neighbor=00:11:22:33:44:06,0x0000,17,148,7 neighbor=00:11:22:33:44:07,0x0000,17,170,7 neighbor=00:11:22:33:44:08,0x0000,18,0,7 neighbor=00:11:22:33:44:09,0x0000,18,5,7")

        logger.info("Preferred Candidate List (no matching neighbor, unknown channels 2)")
        wnm_bss_tm_check(hapd, dev[0], "pref=1 neighbor=00:11:22:33:44:00,0x0000,0,0,7")
    finally:
        stop_wnm_tm(hapd, dev)

def test_wnm_bss_tm_country_jp(dev, apdev):
    """WNM BSS Transition Management (JP)"""
    addr = dev[0].p2p_interface_addr()
    try:
        hapd = None
        hapd, id = start_wnm_tm(apdev[0], "JP", dev[0])

        logger.info("Preferred Candidate List (no matching neighbor, known channels)")
        wnm_bss_tm_check(hapd, dev[0], "pref=1 neighbor=11:22:33:44:55:66,0x0000,30,3,7,0301ff neighbor=00:11:22:33:44:55,0x0000,31,14,7,03010a neighbor=00:11:22:33:44:57,0x0000,1,36,7 neighbor=00:11:22:33:44:59,0x0000,34,100,7 neighbor=00:11:22:33:44:5c,0x0000,59,1,7")

        # Make the test take less time by limiting full scans
        dev[0].set_network(id, "scan_freq", "2412")
        logger.info("Preferred Candidate List (no matching neighbor, unknown channels)")
        wnm_bss_tm_check(hapd, dev[0], "pref=1 neighbor=11:22:33:44:55:66,0x0000,30,0,7,0301ff neighbor=22:33:44:55:66:77,0x0000,30,14,7 neighbor=00:11:22:33:44:56,0x0000,31,13,7 neighbor=00:11:22:33:44:57,0x0000,1,33,7 neighbor=00:11:22:33:44:58,0x0000,1,65,7 neighbor=00:11:22:33:44:5a,0x0000,34,99,7 neighbor=00:11:22:33:44:5b,0x0000,34,141,7 neighbor=00:11:22:33:44:5d,0x0000,59,0,7 neighbor=00:11:22:33:44:5e,0x0000,59,4,7 neighbor=00:11:22:33:44:5f,0x0000,0,0,7")
    finally:
        stop_wnm_tm(hapd, dev)

def test_wnm_bss_tm_country_cn(dev, apdev):
    """WNM BSS Transition Management (CN)"""
    addr = dev[0].p2p_interface_addr()
    try:
        hapd = None
        hapd, id = start_wnm_tm(apdev[0], "CN", dev[0])

        logger.info("Preferred Candidate List (no matching neighbor, known channels)")
        wnm_bss_tm_check(hapd, dev[0], "pref=1 neighbor=11:22:33:44:55:66,0x0000,7,3,7,0301ff neighbor=00:11:22:33:44:55,0x0000,1,36,7,03010a neighbor=00:11:22:33:44:57,0x0000,3,149,7 neighbor=00:11:22:33:44:59,0x0000,6,149,7")

        # Make the test take less time by limiting full scans
        dev[0].set_network(id, "scan_freq", "2412")
        logger.info("Preferred Candidate List (no matching neighbor, unknown channels)")
        wnm_bss_tm_check(hapd, dev[0], "pref=1 neighbor=11:22:33:44:55:66,0x0000,7,0,7,0301ff neighbor=22:33:44:55:66:77,0x0000,7,14,7 neighbor=00:11:22:33:44:56,0x0000,1,35,7 neighbor=00:11:22:33:44:57,0x0000,1,65,7 neighbor=00:11:22:33:44:58,0x0000,3,148,7 neighbor=00:11:22:33:44:5a,0x0000,3,166,7 neighbor=00:11:22:33:44:5f,0x0000,0,0,7")
    finally:
        stop_wnm_tm(hapd, dev)

def test_wnm_bss_tm_global(dev, apdev):
    """WNM BSS Transition Management (global)"""
    run_wnm_bss_tm_global(dev, apdev, "XX", None)

def test_wnm_bss_tm_global4(dev, apdev):
    """WNM BSS Transition Management (global; indicate table E-4)"""
    run_wnm_bss_tm_global(dev, apdev, "FI", "0x04")

def run_wnm_bss_tm_global(dev, apdev, country, country3):
    addr = dev[0].p2p_interface_addr()
    try:
        hapd = None
        hapd, id = start_wnm_tm(apdev[0], country, dev[0], country3=country3)

        logger.info("Preferred Candidate List (no matching neighbor, known channels)")
        wnm_bss_tm_check(hapd, dev[0], "pref=1 neighbor=11:22:33:44:55:66,0x0000,81,3,7,0301ff neighbor=00:11:22:33:44:55,0x0000,82,14,7,03010a neighbor=00:11:22:33:44:57,0x0000,83,1,7 neighbor=00:11:22:33:44:59,0x0000,115,36,7 neighbor=00:11:22:33:44:5a,0x0000,121,100,7 neighbor=00:11:22:33:44:5c,0x0000,124,149,7 neighbor=00:11:22:33:44:5d,0x0000,125,149,7 neighbor=00:11:22:33:44:5e,0x0000,128,42,7 neighbor=00:11:22:33:44:5f,0x0000,129,50,7 neighbor=00:11:22:33:44:60,0x0000,180,1,7")

        # Make the test take less time by limiting full scans
        dev[0].set_network(id, "scan_freq", "2412")
        logger.info("Preferred Candidate List (no matching neighbor, unknown channels)")
        wnm_bss_tm_check(hapd, dev[0], "pref=1 neighbor=00:11:22:33:44:00,0x0000,81,0,7 neighbor=00:11:22:33:44:01,0x0000,81,14,7 neighbor=00:11:22:33:44:02,0x0000,82,13,7 neighbor=00:11:22:33:44:03,0x0000,83,0,7 neighbor=00:11:22:33:44:04,0x0000,83,14,7 neighbor=00:11:22:33:44:05,0x0000,115,35,7 neighbor=00:11:22:33:44:06,0x0000,115,65,7 neighbor=00:11:22:33:44:07,0x0000,121,99,7 neighbor=00:11:22:33:44:08,0x0000,121,141,7 neighbor=00:11:22:33:44:09,0x0000,124,148,7")

        logger.info("Preferred Candidate List (no matching neighbor, unknown channels 2)")
        wnm_bss_tm_check(hapd, dev[0], "pref=1 neighbor=00:11:22:33:44:00,0x0000,124,162,7 neighbor=00:11:22:33:44:01,0x0000,125,148,7 neighbor=00:11:22:33:44:02,0x0000,125,170,7 neighbor=00:11:22:33:44:03,0x0000,128,35,7 neighbor=00:11:22:33:44:04,0x0000,128,162,7 neighbor=00:11:22:33:44:05,0x0000,129,49,7 neighbor=00:11:22:33:44:06,0x0000,129,115,7 neighbor=00:11:22:33:44:07,0x0000,180,0,7 neighbor=00:11:22:33:44:08,0x0000,180,5,7 neighbor=00:11:22:33:44:09,0x0000,0,0,7")
    finally:
        stop_wnm_tm(hapd, dev)

def test_wnm_bss_tm_op_class_0(dev, apdev):
    """WNM BSS Transition Management with invalid operating class"""
    try:
        hapd = None
        hapd, id = start_wnm_tm(apdev[0], "US", dev[0])

        logger.info("Preferred Candidate List (no matching neighbor, invalid op class specified for channels)")
        wnm_bss_tm_check(hapd, dev[0], "pref=1 neighbor=00:11:22:33:44:59,0x0000,0,149,7 neighbor=00:11:22:33:44:5b,0x0000,0,1,7")
    finally:
        stop_wnm_tm(hapd, dev)

def test_wnm_bss_tm_rsn(dev, apdev):
    """WNM BSS Transition Management with RSN"""
    passphrase = "zxcvbnm,.-"
    try:
        hapd = None
        hapd2 = None
        hapd = start_wnm_ap(apdev[0], country_code="FI", hw_mode="g",
                            channel="1",
                            rsn=True, pmf=False, passphrase=passphrase)
        hapd2 = start_wnm_ap(apdev[1], country_code="FI", hw_mode="a",
                             channel="36",
                             rsn=True, pmf=False, passphrase=passphrase)
        dev[0].scan_for_bss(apdev[1]['bssid'], 5180)

        id = dev[0].connect("test-wnm-rsn", psk=passphrase,
                            bssid=apdev[0]['bssid'], scan_freq="2412")
        dev[0].set_network(id, "scan_freq", "")
        dev[0].set_network(id, "bssid", "")

        addr = dev[0].own_addr()
        dev[0].dump_monitor()

        time.sleep(0.5)
        logger.info("Preferred Candidate List (matching neighbor for another BSS) without Disassociation Imminent")
        if "OK" not in hapd.request("BSS_TM_REQ " + addr + " pref=1 abridged=1 valid_int=255 neighbor=" + apdev[1]['bssid'] + ",0x0000," + "115,36,7,0301ff"):
            raise Exception("BSS_TM_REQ command failed")
        ev = hapd.wait_event(['BSS-TM-RESP'], timeout=10)
        if ev is None:
            raise Exception("No BSS Transition Management Response")
        if "status_code=0" not in ev:
            raise Exception("BSS transition request was not accepted: " + ev)
        if "target_bssid=" + apdev[1]['bssid'] not in ev:
            raise Exception("Unexpected target BSS: " + ev)
        dev[0].wait_connected(timeout=15, error="No reassociation seen")
        if apdev[1]['bssid'] not in ev:
            raise Exception("Unexpected reassociation target: " + ev)
    finally:
        clear_regdom_state(dev, hapd, hapd2)

def test_wnm_action_proto(dev, apdev):
    """WNM Action protocol testing"""
    hapd = start_wnm_ap(apdev[0], bss_transition=False, wnm_sleep_mode=True)
    bssid = apdev[0]['bssid']
    dev[0].connect("test-wnm", key_mgmt="NONE", scan_freq="2412")
    dev[0].request("WNM_SLEEP enter")
    time.sleep(0.1)
    hapd.set("ext_mgmt_frame_handling", "1")

    msg = {}
    msg['fc'] = MGMT_SUBTYPE_ACTION << 4
    msg['da'] = dev[0].own_addr()
    msg['sa'] = bssid
    msg['bssid'] = bssid

    dialog_token = 1

    logger.debug("Unexpected WNM-Notification Response")
    # Note: This is actually not registered for user space processing in
    # driver_nl80211.c nl80211_mgmt_subscribe_non_ap() and as such, won't make
    # it to wpa_supplicant.
    msg['payload'] = struct.pack("<BBBB",
                                 ACTION_CATEG_WNM, WNM_ACT_NOTIFICATION_RESP,
                                 dialog_token, 0)
    hapd.mgmt_tx(msg)
    expect_ack(hapd)

    logger.debug("Truncated WNM-Notification Request (no Type field)")
    msg['payload'] = struct.pack("<BBB",
                                 ACTION_CATEG_WNM, WNM_ACT_NOTIFICATION_REQ,
                                 dialog_token)
    hapd.mgmt_tx(msg)
    expect_ack(hapd)

    logger.debug("WFA WNM-Notification Request with truncated IE (min)")
    msg['payload'] = struct.pack("<BBBBBB",
                                 ACTION_CATEG_WNM, WNM_ACT_NOTIFICATION_REQ,
                                 dialog_token, WNM_NOTIF_TYPE_WFA, 0, 1)
    hapd.mgmt_tx(msg)
    expect_ack(hapd)

    logger.debug("WFA WNM-Notification Request with truncated IE (max)")
    msg['payload'] = struct.pack("<BBBBBB",
                                 ACTION_CATEG_WNM, WNM_ACT_NOTIFICATION_REQ,
                                 dialog_token, WNM_NOTIF_TYPE_WFA, 0, 255)
    hapd.mgmt_tx(msg)
    expect_ack(hapd)

    logger.debug("WFA WNM-Notification Request with too short IE")
    msg['payload'] = struct.pack("<BBBBBB",
                                 ACTION_CATEG_WNM, WNM_ACT_NOTIFICATION_REQ,
                                 dialog_token, WNM_NOTIF_TYPE_WFA, 0, 0)
    hapd.mgmt_tx(msg)
    expect_ack(hapd)

    logger.debug("WFA WNM-Notification Request with truncated Sub Rem URL")
    msg['payload'] = struct.pack(">BBBBBBLB",
                                 ACTION_CATEG_WNM, WNM_ACT_NOTIFICATION_REQ,
                                 dialog_token, WNM_NOTIF_TYPE_WFA, 0xdd, 5,
                                 0x506f9a00, 1)
    hapd.mgmt_tx(msg)
    expect_ack(hapd)

    logger.debug("WFA WNM-Notification Request with truncated Sub Rem URL(2)")
    msg['payload'] = struct.pack(">BBBBBBLBB",
                                 ACTION_CATEG_WNM, WNM_ACT_NOTIFICATION_REQ,
                                 dialog_token, WNM_NOTIF_TYPE_WFA, 0xdd, 6,
                                 0x506f9a00, 1, 0)
    hapd.mgmt_tx(msg)
    expect_ack(hapd)

    logger.debug("WFA WNM-Notification Request with truncated Sub Rem URL(3)")
    msg['payload'] = struct.pack(">BBBBBBLB",
                                 ACTION_CATEG_WNM, WNM_ACT_NOTIFICATION_REQ,
                                 dialog_token, WNM_NOTIF_TYPE_WFA, 0xdd, 5,
                                 0x506f9a00, 0xff)
    hapd.mgmt_tx(msg)
    expect_ack(hapd)

    logger.debug("WFA WNM-Notification Request with truncated Deauth Imminent URL(min)")
    msg['payload'] = struct.pack(">BBBBBBLBHB",
                                 ACTION_CATEG_WNM, WNM_ACT_NOTIFICATION_REQ,
                                 dialog_token, WNM_NOTIF_TYPE_WFA, 0xdd, 8,
                                 0x506f9a01, 0, 0, 1)
    hapd.mgmt_tx(msg)
    expect_ack(hapd)

    logger.debug("WFA WNM-Notification Request with truncated Deauth Imminent URL(max)")
    msg['payload'] = struct.pack(">BBBBBBLBHB",
                                 ACTION_CATEG_WNM, WNM_ACT_NOTIFICATION_REQ,
                                 dialog_token, WNM_NOTIF_TYPE_WFA, 0xdd, 8,
                                 0x506f9a01, 0, 0, 0xff)
    hapd.mgmt_tx(msg)
    expect_ack(hapd)

    logger.debug("WFA WNM-Notification Request with unsupported IE")
    msg['payload'] = struct.pack("<BBBBBBL",
                                 ACTION_CATEG_WNM, WNM_ACT_NOTIFICATION_REQ,
                                 dialog_token, WNM_NOTIF_TYPE_WFA, 0xdd, 4, 0)
    hapd.mgmt_tx(msg)
    expect_ack(hapd)

    logger.debug("WNM-Notification Request with unknown WNM-Notification type 0")
    msg['payload'] = struct.pack("<BBBB",
                                 ACTION_CATEG_WNM, WNM_ACT_NOTIFICATION_REQ,
                                 dialog_token, WNM_NOTIF_TYPE_FW_UPGRADE)
    hapd.mgmt_tx(msg)
    expect_ack(hapd)

    logger.debug("Truncated WNM Sleep Mode Response - no Dialog Token")
    msg['payload'] = struct.pack("<BB",
                                 ACTION_CATEG_WNM, WNM_ACT_SLEEP_MODE_RESP)
    hapd.mgmt_tx(msg)
    expect_ack(hapd)

    logger.debug("Truncated WNM Sleep Mode Response - no Key Data Length")
    msg['payload'] = struct.pack("<BBB",
                                 ACTION_CATEG_WNM, WNM_ACT_SLEEP_MODE_RESP, 0)
    hapd.mgmt_tx(msg)
    expect_ack(hapd)

    logger.debug("Truncated WNM Sleep Mode Response - truncated Key Data (min)")
    msg['payload'] = struct.pack("<BBBH",
                                 ACTION_CATEG_WNM, WNM_ACT_SLEEP_MODE_RESP, 0,
                                 1)
    hapd.mgmt_tx(msg)
    expect_ack(hapd)

    logger.debug("Truncated WNM Sleep Mode Response - truncated Key Data (max)")
    msg['payload'] = struct.pack("<BBBH",
                                 ACTION_CATEG_WNM, WNM_ACT_SLEEP_MODE_RESP, 0,
                                 0xffff)
    hapd.mgmt_tx(msg)
    expect_ack(hapd)

    logger.debug("WNM Sleep Mode Response - truncated IE header")
    msg['payload'] = struct.pack("<BBBHB",
                                 ACTION_CATEG_WNM, WNM_ACT_SLEEP_MODE_RESP, 0,
                                 0, 0)
    hapd.mgmt_tx(msg)
    expect_ack(hapd)

    logger.debug("WNM Sleep Mode Response - truncated IE")
    msg['payload'] = struct.pack("<BBBHBB",
                                 ACTION_CATEG_WNM, WNM_ACT_SLEEP_MODE_RESP, 0,
                                 0, 0, 1)
    hapd.mgmt_tx(msg)
    expect_ack(hapd)

    logger.debug("WNM Sleep Mode Response - Empty TFS Response")
    msg['payload'] = struct.pack("<BBBHBB",
                                 ACTION_CATEG_WNM, WNM_ACT_SLEEP_MODE_RESP, 0,
                                 0, WLAN_EID_TFS_RESP, 0)
    hapd.mgmt_tx(msg)
    expect_ack(hapd)

    logger.debug("WNM Sleep Mode Response - EID 0 not recognized")
    msg['payload'] = struct.pack("<BBBHBB",
                                 ACTION_CATEG_WNM, WNM_ACT_SLEEP_MODE_RESP, 0,
                                 0, 0, 0)
    hapd.mgmt_tx(msg)
    expect_ack(hapd)

    logger.debug("WNM Sleep Mode Response - Empty WNM Sleep Mode element and TFS Response element")
    msg['payload'] = struct.pack("<BBBHBBBB",
                                 ACTION_CATEG_WNM, WNM_ACT_SLEEP_MODE_RESP, 0,
                                 0, WLAN_EID_WNMSLEEP, 0, WLAN_EID_TFS_RESP, 0)
    hapd.mgmt_tx(msg)
    expect_ack(hapd)

    logger.debug("WNM Sleep Mode Response - WNM Sleep Mode element and empty TFS Response element")
    msg['payload'] = struct.pack("<BBBHBBBBHBB",
                                 ACTION_CATEG_WNM, WNM_ACT_SLEEP_MODE_RESP, 0,
                                 0, WLAN_EID_WNMSLEEP, 4, WNM_SLEEP_MODE_ENTER,
                                 WNM_STATUS_SLEEP_ACCEPT, 0,
                                 WLAN_EID_TFS_RESP, 0)
    hapd.mgmt_tx(msg)
    expect_ack(hapd)

    logger.debug("WNM Sleep Mode Response - WNM Sleep Mode element(exit, deny key) and empty TFS Response element")
    msg['payload'] = struct.pack("<BBBHBBBBHBB",
                                 ACTION_CATEG_WNM, WNM_ACT_SLEEP_MODE_RESP, 0,
                                 0, WLAN_EID_WNMSLEEP, 4, WNM_SLEEP_MODE_EXIT,
                                 WNM_STATUS_DENIED_KEY, 0,
                                 WLAN_EID_TFS_RESP, 0)
    hapd.mgmt_tx(msg)
    expect_ack(hapd)

    logger.debug("WNM Sleep Mode Response - WNM Sleep Mode element(enter, deny key) and empty TFS Response element")
    msg['payload'] = struct.pack("<BBBHBBBBHBB",
                                 ACTION_CATEG_WNM, WNM_ACT_SLEEP_MODE_RESP, 0,
                                 0, WLAN_EID_WNMSLEEP, 4, WNM_SLEEP_MODE_ENTER,
                                 WNM_STATUS_DENIED_KEY, 0,
                                 WLAN_EID_TFS_RESP, 0)
    hapd.mgmt_tx(msg)
    expect_ack(hapd)

@remote_compatible
def test_wnm_action_proto_pmf(dev, apdev):
    """WNM Action protocol testing (PMF enabled)"""
    ssid = "test-wnm-pmf"
    hapd = start_wnm_ap(apdev[0], rsn=True, wnm_sleep_mode=True, ssid=ssid)
    bssid = apdev[0]['bssid']
    dev[0].connect(ssid, psk="12345678", key_mgmt="WPA-PSK-SHA256",
                   proto="WPA2", ieee80211w="2", scan_freq="2412")
    dev[0].request("WNM_SLEEP enter")
    time.sleep(0.1)
    hapd.set("ext_mgmt_frame_handling", "1")

    msg = {}
    msg['fc'] = MGMT_SUBTYPE_ACTION << 4
    msg['da'] = dev[0].own_addr()
    msg['sa'] = bssid
    msg['bssid'] = bssid

    logger.debug("WNM Sleep Mode Response - Invalid Key Data element length")
    keydata = struct.pack("<BB", 0, 1)
    msg['payload'] = struct.pack("<BBBH",
                                 ACTION_CATEG_WNM, WNM_ACT_SLEEP_MODE_RESP, 0,
                                 len(keydata))
    msg['payload'] += keydata
    msg['payload'] += struct.pack("<BBBBHBB",
                                  WLAN_EID_WNMSLEEP, 4, WNM_SLEEP_MODE_EXIT,
                                  WNM_STATUS_SLEEP_ACCEPT, 0,
                                  WLAN_EID_TFS_RESP, 0)
    hapd.mgmt_tx(msg)
    expect_ack(hapd)

    logger.debug("WNM Sleep Mode Response - Too short GTK subelem")
    keydata = struct.pack("<BB", WNM_SLEEP_SUBELEM_GTK, 0)
    msg['payload'] = struct.pack("<BBBH",
                                 ACTION_CATEG_WNM, WNM_ACT_SLEEP_MODE_RESP, 0,
                                 len(keydata))
    msg['payload'] += keydata
    msg['payload'] += struct.pack("<BBBBHBB",
                                  WLAN_EID_WNMSLEEP, 4, WNM_SLEEP_MODE_EXIT,
                                  WNM_STATUS_SLEEP_ACCEPT, 0,
                                  WLAN_EID_TFS_RESP, 0)
    hapd.mgmt_tx(msg)
    expect_ack(hapd)

    logger.debug("WNM Sleep Mode Response - Invalid GTK subelem")
    keydata = struct.pack("<BBHB2L4L", WNM_SLEEP_SUBELEM_GTK, 11 + 16,
                          0, 17, 0, 0, 0, 0, 0, 0)
    msg['payload'] = struct.pack("<BBBH",
                                 ACTION_CATEG_WNM, WNM_ACT_SLEEP_MODE_RESP, 0,
                                 len(keydata))
    msg['payload'] += keydata
    msg['payload'] += struct.pack("<BBBBHBB",
                                  WLAN_EID_WNMSLEEP, 4, WNM_SLEEP_MODE_EXIT,
                                  WNM_STATUS_SLEEP_ACCEPT, 0,
                                  WLAN_EID_TFS_RESP, 0)
    hapd.mgmt_tx(msg)
    expect_ack(hapd)

    logger.debug("WNM Sleep Mode Response - Invalid GTK subelem (2)")
    keydata = struct.pack("<BBHB2L4L", WNM_SLEEP_SUBELEM_GTK, 11 + 16,
                          0, 0, 0, 0, 0, 0, 0, 0)
    msg['payload'] = struct.pack("<BBBH",
                                 ACTION_CATEG_WNM, WNM_ACT_SLEEP_MODE_RESP, 0,
                                 len(keydata))
    msg['payload'] += keydata
    msg['payload'] += struct.pack("<BBBBHBB",
                                  WLAN_EID_WNMSLEEP, 4, WNM_SLEEP_MODE_EXIT,
                                  WNM_STATUS_SLEEP_ACCEPT, 0,
                                  WLAN_EID_TFS_RESP, 0)
    hapd.mgmt_tx(msg)
    expect_ack(hapd)

    logger.debug("WNM Sleep Mode Response - GTK subelem and too short IGTK subelem")
    keydata = struct.pack("<BBHB", WNM_SLEEP_SUBELEM_GTK, 11 + 16, 0, 16)
    keydata += struct.pack(">2L4L", 0x01020304, 0x05060708,
                           0x11223344, 0x55667788, 0x9900aabb, 0xccddeeff)
    keydata += struct.pack("<BB", WNM_SLEEP_SUBELEM_IGTK, 0)
    msg['payload'] = struct.pack("<BBBH",
                                 ACTION_CATEG_WNM, WNM_ACT_SLEEP_MODE_RESP, 0,
                                 len(keydata))
    msg['payload'] += keydata
    msg['payload'] += struct.pack("<BBBBHBB",
                                  WLAN_EID_WNMSLEEP, 4, WNM_SLEEP_MODE_EXIT,
                                  WNM_STATUS_SLEEP_ACCEPT, 0,
                                  WLAN_EID_TFS_RESP, 0)
    hapd.mgmt_tx(msg)
    expect_ack(hapd)

    logger.debug("WNM Sleep Mode Response - Unknown subelem")
    keydata = struct.pack("<BB", 255, 0)
    msg['payload'] = struct.pack("<BBBH",
                                 ACTION_CATEG_WNM, WNM_ACT_SLEEP_MODE_RESP, 0,
                                 len(keydata))
    msg['payload'] += keydata
    msg['payload'] += struct.pack("<BBBBHBB",
                                  WLAN_EID_WNMSLEEP, 4, WNM_SLEEP_MODE_EXIT,
                                  WNM_STATUS_SLEEP_ACCEPT, 0,
                                  WLAN_EID_TFS_RESP, 0)
    hapd.mgmt_tx(msg)
    expect_ack(hapd)

@remote_compatible
def test_wnm_action_proto_no_pmf(dev, apdev):
    """WNM Action protocol testing (PMF disabled)"""
    ssid = "test-wnm-no-pmf"
    hapd = start_wnm_ap(apdev[0], rsn=True, pmf=False, bss_transition=False,
                        wnm_sleep_mode=True, ssid=ssid)
    bssid = apdev[0]['bssid']
    dev[0].connect(ssid, psk="12345678", key_mgmt="WPA-PSK",
                   proto="WPA2", ieee80211w="0", scan_freq="2412")
    dev[0].request("WNM_SLEEP enter")
    time.sleep(0.1)
    hapd.set("ext_mgmt_frame_handling", "1")
    hapd.dump_monitor()
    dev[0].request("WNM_SLEEP exit")
    ev = hapd.wait_event(['MGMT-RX'], timeout=5)
    if ev is None:
        raise Exception("WNM-Sleep Mode Request not seen")

    msg = {}
    msg['fc'] = MGMT_SUBTYPE_ACTION << 4
    msg['da'] = dev[0].own_addr()
    msg['sa'] = bssid
    msg['bssid'] = bssid

    logger.debug("WNM Sleep Mode Response - GTK subelem and IGTK subelem")
    keydata = struct.pack("<BBHB", WNM_SLEEP_SUBELEM_GTK, 11 + 16, 0, 16)
    keydata += struct.pack(">2L4L", 0x01020304, 0x05060708,
                           0x11223344, 0x55667788, 0x9900aabb, 0xccddeeff)
    keydata += struct.pack("<BBHLH4L", WNM_SLEEP_SUBELEM_IGTK, 2 + 6 + 16, 0,
                           0x10203040, 0x5060,
                           0xf1f2f3f4, 0xf5f6f7f8, 0xf9f0fafb, 0xfcfdfeff)
    msg['payload'] = struct.pack("<BBBH",
                                 ACTION_CATEG_WNM, WNM_ACT_SLEEP_MODE_RESP, 0,
                                 len(keydata))
    msg['payload'] += keydata
    msg['payload'] += struct.pack("<BBBBHBB",
                                  WLAN_EID_WNMSLEEP, 4, WNM_SLEEP_MODE_EXIT,
                                  WNM_STATUS_SLEEP_ACCEPT, 0,
                                  WLAN_EID_TFS_RESP, 0)
    hapd.mgmt_tx(msg)
    expect_ack(hapd)

    ev = dev[0].wait_event(["WNM: Ignore Key Data"], timeout=5)
    if ev is None:
        raise Exception("Key Data not ignored")

def test_wnm_bss_tm_req_with_mbo_ie(dev, apdev):
    """WNM BSS transition request with MBO IE and reassociation delay attribute"""
    ssid = "test-wnm-mbo"
    hapd = start_wnm_ap(apdev[0], rsn=True, pmf=False, ssid=ssid)
    bssid = apdev[0]['bssid']
    if "OK" not in dev[0].request("SET mbo_cell_capa 1"):
        raise Exception("Failed to set STA as cellular data capable")

    dev[0].connect(ssid, psk="12345678", key_mgmt="WPA-PSK",
                   proto="WPA2", ieee80211w="0", scan_freq="2412")

    logger.debug("BTM request with MBO reassociation delay when disassoc imminent is not set")
    if 'FAIL' not in hapd.request("BSS_TM_REQ " + dev[0].own_addr() + " mbo=3:2:1"):
        raise Exception("BSS transition management succeeded unexpectedly")

    logger.debug("BTM request with invalid MBO transition reason code")
    if 'FAIL' not in hapd.request("BSS_TM_REQ " + dev[0].own_addr() + " mbo=10:2:1"):
        raise Exception("BSS transition management succeeded unexpectedly")

    logger.debug("BTM request with MBO reassociation retry delay of 5 seconds")
    if 'OK' not in hapd.request("BSS_TM_REQ " + dev[0].own_addr() + " disassoc_imminent=1 disassoc_timer=3 mbo=3:5:1"):
        raise Exception("BSS transition management command failed")

    ev = dev[0].wait_event(['MBO-CELL-PREFERENCE'], 1)
    if ev is None or "preference=1" not in ev:
        raise Exception("Timeout waiting for MBO-CELL-PREFERENCE event")

    ev = dev[0].wait_event(['MBO-TRANSITION-REASON'], 1)
    if ev is None or "reason=3" not in ev:
        raise Exception("Timeout waiting for MBO-TRANSITION-REASON event")

    t0 = datetime.now()

    ev = hapd.wait_event(['BSS-TM-RESP'], timeout=10)
    if ev is None:
        raise Exception("No BSS Transition Management Response")
    if dev[0].own_addr() not in ev:
        raise Exception("Unexpected BSS Transition Management Response address")

    ev = dev[0].wait_event(['CTRL-EVENT-DISCONNECTED'], 5)
    if ev is None:
        raise Exception("Station did not disconnect although disassoc imminent was set")

    # Set the scan interval to make dev[0] look for connections
    if 'OK' not in dev[0].request("SCAN_INTERVAL 1"):
        raise Exception("Failed to set scan interval")

    # Wait until connected
    ev = dev[0].wait_event(['CTRL-EVENT-CONNECTED'], 10)
    if ev is None:
        raise Exception("Station did not connect")

    # Make sure no connection is made during the retry delay
    time_diff = datetime.now() - t0
    if time_diff.total_seconds() < 5:
        raise Exception("Station connected before assoc retry delay was over")

    if "OK" not in dev[0].request("SET mbo_cell_capa 3"):
        raise Exception("Failed to set STA as cellular data not-capable")

@remote_compatible
def test_wnm_bss_transition_mgmt_query(dev, apdev):
    """WNM BSS Transition Management query"""
    hapd = start_wnm_ap(apdev[0])
    params = {"ssid": "another"}
    hapd2 = hostapd.add_ap(apdev[1], params)

    dev[0].scan_for_bss(apdev[1]['bssid'], 2412)
    dev[0].scan_for_bss(apdev[0]['bssid'], 2412)

    dev[0].connect("test-wnm", key_mgmt="NONE", scan_freq="2412")
    dev[0].request("WNM_BSS_QUERY 0 list")

    ev = dev[0].wait_event(["WNM: BSS Transition Management Request"],
                           timeout=5)
    if ev is None:
        raise Exception("No BSS Transition Management Request frame seen")

    ev = hapd.wait_event(["BSS-TM-RESP"], timeout=5)
    if ev is None:
        raise Exception("No BSS Transition Management Response frame seen")

def test_wnm_bss_transition_mgmt_query_disabled_on_ap(dev, apdev):
    """WNM BSS Transition Management query - TM disabled on AP"""
    hapd = start_wnm_ap(apdev[0], bss_transition=False)
    dev[0].connect("test-wnm", key_mgmt="NONE", scan_freq="2412")
    # Ignore BSS Transition Management Query from 02:00:00:00:00:00 since BSS Transition Management is disabled
    dev[0].request("WNM_BSS_QUERY 0 list")
    ev = hapd.wait_event(["BSS-TM-RESP"], timeout=0.1)
    if ev is not None:
        raise Exception("Unexpected BSS TM Response reported")

def test_wnm_bss_transition_mgmt_query_mbo(dev, apdev):
    """WNM BSS Transition Management query - TM only due to MBO on AP"""
    hapd = start_wnm_ap(apdev[0], bss_transition=False, mbo=True)
    dev[0].connect("test-wnm", key_mgmt="NONE", scan_freq="2412")
    dev[0].request("WNM_BSS_QUERY 0 list")
    ev = hapd.wait_event(["BSS-TM-RESP"], timeout=5)
    if ev is None:
        raise Exception("No BSS TM Response reported")

@remote_compatible
def test_wnm_bss_tm_security_mismatch(dev, apdev):
    """WNM BSS Transition Management and security mismatch"""
    hapd = start_wnm_ap(apdev[0], hw_mode="g", channel="1", ssid="test-wnm",
                        rsn=True, pmf=False)
    hapd2 = start_wnm_ap(apdev[1], hw_mode="g", channel="11")
    dev[0].scan_for_bss(apdev[1]['bssid'], 2462)

    id = dev[0].connect("test-wnm", psk="12345678",
                        bssid=apdev[0]['bssid'], scan_freq="2412")
    dev[0].set_network(id, "scan_freq", "")
    dev[0].set_network(id, "bssid", "")

    addr = dev[0].own_addr()
    dev[0].dump_monitor()

    logger.info("Preferred Candidate List (matching neighbor for another BSS) without Disassociation Imminent")
    if "OK" not in hapd.request("BSS_TM_REQ " + addr + " pref=1 abridged=1 valid_int=255 neighbor=" + apdev[1]['bssid'] + ",0x0000,115,36,7,0301ff"):
        raise Exception("BSS_TM_REQ command failed")
    ev = hapd.wait_event(['BSS-TM-RESP'], timeout=10)
    if ev is None:
        raise Exception("No BSS Transition Management Response")
    if "status_code=7" not in ev:
        raise Exception("Unexpected BSS transition request response: " + ev)

def test_wnm_bss_tm_connect_cmd(dev, apdev):
    """WNM BSS Transition Management and cfg80211 connect command"""
    hapd = start_wnm_ap(apdev[0], hw_mode="g", channel="1")
    hapd2 = start_wnm_ap(apdev[1], hw_mode="g", channel="11")

    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    wpas.interface_add("wlan5", drv_params="force_connect_cmd=1")

    wpas.scan_for_bss(apdev[1]['bssid'], 2462)

    id = wpas.connect("test-wnm", key_mgmt="NONE",
                      bssid=apdev[0]['bssid'], scan_freq="2412")
    wpas.set_network(id, "scan_freq", "")
    wpas.set_network(id, "bssid", "")

    addr = wpas.own_addr()
    wpas.dump_monitor()

    logger.info("Preferred Candidate List (matching neighbor for another BSS) without Disassociation Imminent")
    if "OK" not in hapd.request("BSS_TM_REQ " + addr + " pref=1 abridged=1 valid_int=255 neighbor=" + apdev[1]['bssid'] + ",0x0000,115,36,7,0301ff"):
        raise Exception("BSS_TM_REQ command failed")
    ev = hapd.wait_event(['BSS-TM-RESP'], timeout=10)
    if ev is None:
        raise Exception("No BSS Transition Management Response")
    if "status_code=0" not in ev:
        raise Exception("BSS transition request was not accepted: " + ev)
    if "target_bssid=" + apdev[1]['bssid'] not in ev:
        raise Exception("Unexpected target BSS: " + ev)
    ev = wpas.wait_event(["CTRL-EVENT-CONNECTED",
                          "CTRL-EVENT-DISCONNECTED"], timeout=10)
    if ev is None:
        raise Exception("No reassociation seen")
    if "CTRL-EVENT-DISCONNECTED" in ev:
        raise Exception("Unexpected disconnection reported")
    if apdev[1]['bssid'] not in ev:
        raise Exception("Unexpected reassociation target: " + ev)

def test_wnm_bss_tm_reject(dev, apdev):
    """WNM BSS Transition Management request getting rejected"""
    try:
        hapd = None
        hapd = start_wnm_ap(apdev[0], country_code="FI", hw_mode="g",
                            channel="1")
        id = dev[0].connect("test-wnm", key_mgmt="NONE", scan_freq="2412")
        addr = dev[0].own_addr()
        dev[0].dump_monitor()

        if "OK" not in dev[0].request("SET reject_btm_req_reason 123"):
            raise Exception("Failed to set reject_btm_req_reason")

        if "OK" not in hapd.request("BSS_TM_REQ " + addr + " disassoc_timer=1"):
            raise Exception("BSS_TM_REQ command failed")
        ev = hapd.wait_event(['BSS-TM-RESP'], timeout=10)
        if ev is None:
            raise Exception("No BSS Transition Management Response")
        if addr not in ev:
            raise Exception("Unexpected BSS Transition Management Response address")
        if "status_code=123" not in ev:
            raise Exception("Unexpected BSS Transition Management Response status: " + ev)
        dev[0].wait_disconnected()
        dev[0].wait_connected()
    finally:
        if hapd:
            hapd.request("DISABLE")
        dev[0].disconnect_and_stop_scan()
        subprocess.call(['iw', 'reg', 'set', '00'])
        dev[0].wait_event(["CTRL-EVENT-REGDOM-CHANGE"], timeout=0.5)
        dev[0].flush_scan_cache()

def test_wnm_bss_tm_ap_proto(dev, apdev):
    """WNM BSS TM - protocol testing for AP message parsing"""
    hapd = start_wnm_ap(apdev[0])
    bssid = hapd.own_addr()
    dev[0].connect("test-wnm", key_mgmt="NONE", scan_freq="2412")
    addr = dev[0].own_addr()

    hdr = "d0003a01" + bssid.replace(':', '') + addr.replace(':', '') + bssid.replace(':', '') + "1000"
    hapd.set("ext_mgmt_frame_handling", "1")
    tests = ["0a",
             "0a06",
             "0a0601",
             "0a060100",
             "0a080000",
             "0a08000000",
             "0a080000001122334455",
             "0a08000000112233445566",
             "0a08000000112233445566112233445566778899",
             "0a08ffffff",
             "0a08ffffff112233445566778899",
             "0a1a",
             "0a1a00",
             "0a1a0000",
             "0a0c016015007f0f000000000000000000000000000000000000",
             "0a0700",
             "0aff00",
             "0aff"]
    for t in tests:
        if "OK" not in hapd.request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=" + hdr + t):
            raise Exception("MGMT_RX_PROCESS failed")

    hapd.set("ext_mgmt_frame_handling", "0")

def test_wnm_bss_transition_mgmt_query_with_unknown_candidates(dev, apdev):
    """WNM BSS Transition Management query with unknown candidates"""
    hapd = start_wnm_ap(apdev[0])
    dev[0].scan_for_bss(apdev[0]['bssid'], 2412)

    dev[0].connect("test-wnm", key_mgmt="NONE", scan_freq="2412")
    dev[0].request("WNM_BSS_QUERY 0 neighbor=00:11:22:33:44:55,0,81,1,4")

    ev = dev[0].wait_event(["WNM: BSS Transition Management Request"],
                           timeout=5)
    if ev is None:
        raise Exception("No BSS Transition Management Request frame seen")

    ev = hapd.wait_event(["BSS-TM-RESP"], timeout=5)
    if ev is None:
        raise Exception("No BSS Transition Management Response frame seen")

def test_wnm_time_adv_without_time_zone(dev, apdev):
    """WNM Time Advertisement without time zone configuration"""
    params = {"ssid": "test-wnm",
              "time_advertisement": "2"}
    hostapd.add_ap(apdev[0], params)

    dev[0].connect("test-wnm", key_mgmt="NONE", scan_freq="2412")

def test_wnm_coloc_intf_reporting(dev, apdev):
    """WNM Collocated Interference Reporting"""
    hapd = start_wnm_ap(apdev[0], bss_transition=False,
                        coloc_intf_reporting=True)

    no_intf = struct.pack("<BBBBBLLLLH", 96, 21, 0, 127, 0x0f, 0, 0, 0, 0, 0)

    try:
        dev[0].set("coloc_intf_reporting", "1")
        dev[0].connect("test-wnm", key_mgmt="NONE", scan_freq="2412")
        addr = dev[0].own_addr()
        if "OK" not in hapd.request("COLOC_INTF_REQ %s 1 5" % addr):
            raise Exception("Could not send Collocated Interference Request")
        ev = dev[0].wait_event(["COLOC-INTF-REQ"], timeout=2)
        if ev is None:
            raise Exception("No Collocated Interference Request frame seen")
        vals = ev.split(' ')
        if vals[2] != '1' or vals[3] != '5':
            raise Exception("Unexpected request values: " + ev)
        dev[0].set("coloc_intf_elems", binascii.hexlify(no_intf).decode())
        ev = hapd.wait_event(["COLOC-INTF-REPORT"], timeout=1)
        if ev is None:
            raise Exception("No Collocated Interference Report frame seen")
        if addr + " 1 " + binascii.hexlify(no_intf).decode() not in ev:
            raise Exception("Unexpected report values: " + ev)

        if "OK" not in hapd.request("COLOC_INTF_REQ %s 0 0" % addr):
            raise Exception("Could not send Collocated Interference Request")
        ev = dev[0].wait_event(["COLOC-INTF-REQ"], timeout=2)
        if ev is None:
            raise Exception("No Collocated Interference Request frame seen")
        vals = ev.split(' ')
        if vals[2] != '0' or vals[3] != '0':
            raise Exception("Unexpected request values: " + ev)

        res = dev[0].request("COLOC_INTF_REPORT " + binascii.hexlify(no_intf).decode())
        if "OK" not in res:
            raise Exception("Could not send unsolicited report")
        ev = hapd.wait_event(["COLOC-INTF-REPORT"], timeout=1)
        if ev is None:
            raise Exception("No Collocated Interference Report frame seen")
        if addr + " 0 " + binascii.hexlify(no_intf).decode() not in ev:
            raise Exception("Unexpected report values: " + ev)

        if "FAIL" not in hapd.request("COLOC_INTF_REQ foo 1 5"):
            raise Exception("Invalid COLOC_INTF_REQ accepted")
        if "FAIL" not in hapd.request("COLOC_INTF_REQ 02:ff:ff:ff:ff:ff 1 5"):
            raise Exception("COLOC_INTF_REQ for unknown STA accepted")
        if "FAIL" not in hapd.request("COLOC_INTF_REQ %s 1" % addr):
            raise Exception("Invalid COLOC_INTF_REQ accepted")
        if "FAIL" not in hapd.request("COLOC_INTF_REQ %s" % addr):
            raise Exception("Invalid COLOC_INTF_REQ accepted")
    finally:
        dev[0].set("coloc_intf_reporting", "0")
        dev[0].set("coloc_intf_elems", "")

def test_wnm_coloc_intf_reporting_errors(dev, apdev):
    """WNM Collocated Interference Reporting errors"""
    hapd = start_wnm_ap(apdev[0], bss_transition=False,
                        coloc_intf_reporting=True)
    bssid = hapd.own_addr()
    dev[0].set("coloc_intf_reporting", "1")
    dev[0].connect("test-wnm", key_mgmt="NONE", scan_freq="2412")
    addr = dev[0].own_addr()
    if "FAIL" not in hapd.request("COLOC_INTF_REQ %s 4 5" % addr):
        raise Exception("Invalid Collocated Interference Request accepted")
    hdr = "d0003a01" + bssid.replace(':', '') + addr.replace(':', '') + bssid.replace(':', '') + "1000"
    hapd.set("ext_mgmt_frame_handling", "1")
    tests = ["0a0c016015007f0f000000000000000000000000000000000000",
             "0a0c"]
    with alloc_fail(hapd, 1, "ieee802_11_rx_wnm_coloc_intf_report"):
        for t in tests:
            if "OK" not in hapd.request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=" + hdr + t):
                raise Exception("MGMT_RX_PROCESS failed")

    hapd.set("ext_mgmt_frame_handling", "0")

def test_wnm_bss_transition_mgmt_disabled(dev, apdev):
    """WNM BSS Transition Management disabled"""
    hapd = start_wnm_ap(apdev[0])
    try:
        dev[0].set("disable_btm", "1")
        dev[0].connect("test-wnm", key_mgmt="NONE", scan_freq="2412")
        addr = dev[0].own_addr()
        hapd.request("BSS_TM_REQ " + addr)
        ev = hapd.wait_event(['BSS-TM-RESP'], timeout=0.5)
        if ev is not None:
            raise Exception("Unexpected BSS Transition Management Response")
    finally:
        dev[0].set("disable_btm", "0")

def test_wnm_time_adv_restart(dev, apdev):
    """WNM time advertisement and interface restart"""
    hapd = start_wnm_ap(apdev[0], time_adv=True)
    hapd.disable()
    hapd.enable()
    dev[0].connect("test-wnm", key_mgmt="NONE", scan_freq="2412")
