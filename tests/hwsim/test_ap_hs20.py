# Hotspot 2.0 tests
# Copyright (c) 2013-2019, Jouni Malinen <j@w1.fi>
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

from remotehost import remote_compatible
import base64
import binascii
import struct
import time
import logging
logger = logging.getLogger()
import os
import os.path
import socket
import subprocess

import hostapd
from utils import *
import hwsim_utils
from tshark import run_tshark
from wlantest import Wlantest
from wpasupplicant import WpaSupplicant
from wlantest import WlantestCapture
from test_ap_eap import check_eap_capa, check_domain_match_full
from test_gas import gas_rx, parse_gas, action_response, anqp_initial_resp, send_gas_resp, ACTION_CATEG_PUBLIC, GAS_INITIAL_RESPONSE

def hs20_ap_params(ssid="test-hs20"):
    params = hostapd.wpa2_params(ssid=ssid)
    params['wpa_key_mgmt'] = "WPA-EAP"
    params['ieee80211w'] = "1"
    params['ieee8021x'] = "1"
    params['auth_server_addr'] = "127.0.0.1"
    params['auth_server_port'] = "1812"
    params['auth_server_shared_secret'] = "radius"
    params['interworking'] = "1"
    params['access_network_type'] = "14"
    params['internet'] = "1"
    params['asra'] = "0"
    params['esr'] = "0"
    params['uesa'] = "0"
    params['venue_group'] = "7"
    params['venue_type'] = "1"
    params['venue_name'] = ["eng:Example venue", "fin:Esimerkkipaikka"]
    params['roaming_consortium'] = ["112233", "1020304050", "010203040506",
                                    "fedcba"]
    params['domain_name'] = "example.com,another.example.com"
    params['nai_realm'] = ["0,example.com,13[5:6],21[2:4][5:7]",
                           "0,another.example.com"]
    params['hs20'] = "1"
    params['hs20_wan_metrics'] = "01:8000:1000:80:240:3000"
    params['hs20_conn_capab'] = ["1:0:2", "6:22:1", "17:5060:0"]
    params['hs20_operating_class'] = "5173"
    params['anqp_3gpp_cell_net'] = "244,91"
    return params

def check_auto_select(dev, bssid):
    dev.scan_for_bss(bssid, freq="2412")
    dev.request("INTERWORKING_SELECT auto freq=2412")
    ev = dev.wait_connected(timeout=15)
    if bssid not in ev:
        raise Exception("Connected to incorrect network")
    dev.request("REMOVE_NETWORK all")
    dev.wait_disconnected()
    dev.dump_monitor()

def interworking_select(dev, bssid, type=None, no_match=False, freq=None):
    dev.dump_monitor()
    if bssid and freq and not no_match:
        dev.scan_for_bss(bssid, freq=freq)
    freq_extra = " freq=" + str(freq) if freq else ""
    dev.request("INTERWORKING_SELECT" + freq_extra)
    ev = dev.wait_event(["INTERWORKING-AP", "INTERWORKING-NO-MATCH"],
                        timeout=15)
    if ev is None:
        raise Exception("Network selection timed out")
    if no_match:
        if "INTERWORKING-NO-MATCH" not in ev:
            raise Exception("Unexpected network match")
        return
    if "INTERWORKING-NO-MATCH" in ev:
        logger.info("Matching network not found - try again")
        dev.dump_monitor()
        dev.request("INTERWORKING_SELECT" + freq_extra)
        ev = dev.wait_event(["INTERWORKING-AP", "INTERWORKING-NO-MATCH"],
                            timeout=15)
        if ev is None:
            raise Exception("Network selection timed out")
        if "INTERWORKING-NO-MATCH" in ev:
            raise Exception("Matching network not found")
    if bssid and bssid not in ev:
        raise Exception("Unexpected BSSID in match")
    if type and "type=" + type not in ev:
        raise Exception("Network type not recognized correctly")

def check_sp_type(dev, sp_type):
    type = dev.get_status_field("sp_type")
    if type is None:
        raise Exception("sp_type not available")
    if type != sp_type:
        raise Exception("sp_type did not indicate %s network" % sp_type)

def hlr_auc_gw_available():
    if not os.path.exists("/tmp/hlr_auc_gw.sock"):
        raise HwsimSkip("No hlr_auc_gw socket available")
    if not os.path.exists("../../hostapd/hlr_auc_gw"):
        raise HwsimSkip("No hlr_auc_gw available")

def interworking_ext_sim_connect(dev, bssid, method):
    dev.request("INTERWORKING_CONNECT " + bssid)
    interworking_ext_sim_auth(dev, method)

def interworking_ext_sim_auth(dev, method):
    ev = dev.wait_event(["CTRL-EVENT-EAP-METHOD"], timeout=15)
    if ev is None:
        raise Exception("Network connected timed out")
    if "(" + method + ")" not in ev:
        raise Exception("Unexpected EAP method selection")

    ev = dev.wait_event(["CTRL-REQ-SIM"], timeout=15)
    if ev is None:
        raise Exception("Wait for external SIM processing request timed out")
    p = ev.split(':', 2)
    if p[1] != "GSM-AUTH":
        raise Exception("Unexpected CTRL-REQ-SIM type")
    id = p[0].split('-')[3]
    rand = p[2].split(' ')[0]

    res = subprocess.check_output(["../../hostapd/hlr_auc_gw",
                                   "-m",
                                   "auth_serv/hlr_auc_gw.milenage_db",
                                   "GSM-AUTH-REQ 232010000000000 " + rand]).decode()
    if "GSM-AUTH-RESP" not in res:
        raise Exception("Unexpected hlr_auc_gw response")
    resp = res.split(' ')[2].rstrip()

    dev.request("CTRL-RSP-SIM-" + id + ":GSM-AUTH:" + resp)
    dev.wait_connected(timeout=15)

def interworking_connect(dev, bssid, method):
    dev.request("INTERWORKING_CONNECT " + bssid)
    interworking_auth(dev, method)

def interworking_auth(dev, method):
    ev = dev.wait_event(["CTRL-EVENT-EAP-METHOD"], timeout=15)
    if ev is None:
        raise Exception("Network connected timed out")
    if "(" + method + ")" not in ev:
        raise Exception("Unexpected EAP method selection")

    dev.wait_connected(timeout=15)

def check_probe_resp(wt, bssid_unexpected, bssid_expected):
    if bssid_unexpected:
        count = wt.get_bss_counter("probe_response", bssid_unexpected)
        if count > 0:
            raise Exception("Unexpected Probe Response frame from AP")

    if bssid_expected:
        count = wt.get_bss_counter("probe_response", bssid_expected)
        if count == 0:
            raise Exception("No Probe Response frame from AP")

def test_ap_anqp_sharing(dev, apdev):
    """ANQP sharing within ESS and explicit unshare"""
    check_eap_capa(dev[0], "MSCHAPV2")
    dev[0].flush_scan_cache()

    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid
    hostapd.add_ap(apdev[0], params)

    bssid2 = apdev[1]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid
    params['nai_realm'] = ["0,example.com,13[5:6],21[2:4][5:7]"]
    hostapd.add_ap(apdev[1], params)

    dev[0].hs20_enable()
    id = dev[0].add_cred_values({'realm': "example.com", 'username': "test",
                                 'password': "secret",
                                 'domain': "example.com"})
    logger.info("Normal network selection with shared ANQP results")
    dev[0].scan_for_bss(bssid, freq="2412")
    dev[0].scan_for_bss(bssid2, freq="2412")
    interworking_select(dev[0], None, "home", freq="2412")
    dev[0].dump_monitor()
    state = dev[0].get_status_field('wpa_state')
    if state != "DISCONNECTED":
        raise Exception("Unexpected wpa_state after INTERWORKING_SELECT: " + state)

    logger.debug("BSS entries:\n" + dev[0].request("BSS RANGE=ALL"))
    res1 = dev[0].get_bss(bssid)
    res2 = dev[0].get_bss(bssid2)
    if 'anqp_nai_realm' not in res1:
        raise Exception("anqp_nai_realm not found for AP1")
    if 'anqp_nai_realm' not in res2:
        raise Exception("anqp_nai_realm not found for AP2")
    if res1['anqp_nai_realm'] != res2['anqp_nai_realm']:
        raise Exception("ANQP results were not shared between BSSes")

    logger.info("Explicit ANQP request to unshare ANQP results")
    dev[0].request("ANQP_GET " + bssid + " 263")
    ev = dev[0].wait_event(["RX-ANQP"], timeout=5)
    if ev is None:
        raise Exception("ANQP operation timed out")

    dev[0].request("ANQP_GET " + bssid2 + " 263")
    ev = dev[0].wait_event(["RX-ANQP"], timeout=5)
    if ev is None:
        raise Exception("ANQP operation timed out")

    res1 = dev[0].get_bss(bssid)
    res2 = dev[0].get_bss(bssid2)
    if res1['anqp_nai_realm'] == res2['anqp_nai_realm']:
        raise Exception("ANQP results were not unshared")

def test_ap_anqp_domain_id(dev, apdev):
    """ANQP Domain ID"""
    check_eap_capa(dev[0], "MSCHAPV2")
    dev[0].flush_scan_cache()

    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid
    params['anqp_domain_id'] = '1234'
    hostapd.add_ap(apdev[0], params)

    bssid2 = apdev[1]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid
    params['anqp_domain_id'] = '1234'
    hostapd.add_ap(apdev[1], params)

    dev[0].hs20_enable()
    id = dev[0].add_cred_values({'realm': "example.com", 'username': "test",
                                 'password': "secret",
                                 'domain': "example.com"})
    dev[0].scan_for_bss(bssid, freq="2412")
    dev[0].scan_for_bss(bssid2, freq="2412")
    interworking_select(dev[0], None, "home", freq="2412")

def test_ap_anqp_no_sharing_diff_ess(dev, apdev):
    """ANQP no sharing between ESSs"""
    check_eap_capa(dev[0], "MSCHAPV2")
    dev[0].flush_scan_cache()

    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid
    hostapd.add_ap(apdev[0], params)

    bssid2 = apdev[1]['bssid']
    params = hs20_ap_params(ssid="test-hs20-another")
    params['hessid'] = bssid
    params['nai_realm'] = ["0,example.com,13[5:6],21[2:4][5:7]"]
    hostapd.add_ap(apdev[1], params)

    dev[0].hs20_enable()
    id = dev[0].add_cred_values({'realm': "example.com", 'username': "test",
                                 'password': "secret",
                                 'domain': "example.com"})
    logger.info("Normal network selection with shared ANQP results")
    dev[0].scan_for_bss(bssid, freq="2412")
    dev[0].scan_for_bss(bssid2, freq="2412")
    interworking_select(dev[0], None, "home", freq="2412")

def test_ap_anqp_no_sharing_missing_info(dev, apdev):
    """ANQP no sharing due to missing information"""
    check_eap_capa(dev[0], "MSCHAPV2")
    dev[0].flush_scan_cache()

    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid
    del params['roaming_consortium']
    del params['domain_name']
    del params['anqp_3gpp_cell_net']
    del params['nai_realm']
    hostapd.add_ap(apdev[0], params)

    bssid2 = apdev[1]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid
    params['nai_realm'] = ["0,example.com,13[5:6],21[2:4][5:7]"]
    hostapd.add_ap(apdev[1], params)

    dev[0].hs20_enable()
    id = dev[0].add_cred_values({'realm': "example.com", 'username': "test",
                                 'password': "secret",
                                 'domain': "example.com"})
    logger.info("Normal network selection with shared ANQP results")
    dev[0].scan_for_bss(bssid, freq="2412")
    dev[0].scan_for_bss(bssid2, freq="2412")
    interworking_select(dev[0], None, "home", freq="2412")

def test_ap_anqp_sharing_oom(dev, apdev):
    """ANQP sharing within ESS and explicit unshare OOM"""
    check_eap_capa(dev[0], "MSCHAPV2")
    dev[0].flush_scan_cache()

    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid
    hostapd.add_ap(apdev[0], params)

    bssid2 = apdev[1]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid
    params['nai_realm'] = ["0,example.com,13[5:6],21[2:4][5:7]"]
    hostapd.add_ap(apdev[1], params)

    dev[0].hs20_enable()
    id = dev[0].add_cred_values({'realm': "example.com", 'username': "test",
                                 'password': "secret",
                                 'domain': "example.com"})
    dev[0].scan_for_bss(bssid, freq="2412")
    dev[0].scan_for_bss(bssid2, freq="2412")
    interworking_select(dev[0], None, "home", freq="2412")
    dev[0].dump_monitor()

    with alloc_fail(dev[0], 1, "wpa_bss_anqp_clone"):
        dev[0].request("ANQP_GET " + bssid + " 263")
        ev = dev[0].wait_event(["RX-ANQP"], timeout=5)
        if ev is None:
            raise Exception("ANQP operation timed out")

def test_ap_nai_home_realm_query(dev, apdev):
    """NAI Home Realm Query"""
    check_eap_capa(dev[0], "MSCHAPV2")
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['nai_realm'] = ["0,example.com,13[5:6],21[2:4][5:7]",
                           "0,another.example.org"]
    hostapd.add_ap(apdev[0], params)

    dev[0].scan(freq="2412")
    dev[0].request("HS20_GET_NAI_HOME_REALM_LIST " + bssid + " realm=example.com")
    ev = dev[0].wait_event(["RX-ANQP"], timeout=5)
    if ev is None:
        raise Exception("ANQP operation timed out")
    nai1 = dev[0].get_bss(bssid)['anqp_nai_realm']
    dev[0].dump_monitor()

    dev[0].request("ANQP_GET " + bssid + " 263")
    ev = dev[0].wait_event(["RX-ANQP"], timeout=5)
    if ev is None:
        raise Exception("ANQP operation timed out")
    nai2 = dev[0].get_bss(bssid)['anqp_nai_realm']

    if len(nai1) >= len(nai2):
        raise Exception("Unexpected NAI Realm list response lengths")
    if binascii.hexlify(b"example.com").decode() not in nai1:
        raise Exception("Home realm not reported")
    if binascii.hexlify(b"example.org").decode() in nai1:
        raise Exception("Non-home realm reported")
    if binascii.hexlify(b"example.com").decode() not in nai2:
        raise Exception("Home realm not reported in wildcard query")
    if binascii.hexlify(b"example.org").decode() not in nai2:
        raise Exception("Non-home realm not reported in wildcard query ")

    cmds = ["foo",
            "00:11:22:33:44:55 123",
            "00:11:22:33:44:55 qq"]
    for cmd in cmds:
        if "FAIL" not in dev[0].request("HS20_GET_NAI_HOME_REALM_LIST " + cmd):
            raise Exception("Invalid HS20_GET_NAI_HOME_REALM_LIST accepted: " + cmd)

    dev[0].dump_monitor()
    if "OK" not in dev[0].request("HS20_GET_NAI_HOME_REALM_LIST " + bssid):
        raise Exception("HS20_GET_NAI_HOME_REALM_LIST failed")
    ev = dev[0].wait_event(["GAS-QUERY-DONE"], timeout=10)
    if ev is None:
        raise Exception("ANQP operation timed out")
    ev = dev[0].wait_event(["RX-ANQP"], timeout=0.1)
    if ev is not None:
        raise Exception("Unexpected ANQP response: " + ev)

    dev[0].dump_monitor()
    if "OK" not in dev[0].request("HS20_GET_NAI_HOME_REALM_LIST " + bssid + " 01000b6578616d706c652e636f6d"):
        raise Exception("HS20_GET_NAI_HOME_REALM_LIST failed")
    ev = dev[0].wait_event(["RX-ANQP"], timeout=10)
    if ev is None:
        raise Exception("No ANQP response")
    if "NAI Realm list" not in ev:
        raise Exception("Missing NAI Realm list: " + ev)

    dev[0].add_cred_values({'realm': "example.com", 'username': "test",
                            'password': "secret",
                            'domain': "example.com"})
    dev[0].dump_monitor()
    if "OK" not in dev[0].request("HS20_GET_NAI_HOME_REALM_LIST " + bssid):
        raise Exception("HS20_GET_NAI_HOME_REALM_LIST failed")
    ev = dev[0].wait_event(["RX-ANQP"], timeout=10)
    if ev is None:
        raise Exception("No ANQP response")
    if "NAI Realm list" not in ev:
        raise Exception("Missing NAI Realm list: " + ev)

@remote_compatible
def test_ap_interworking_scan_filtering(dev, apdev):
    """Interworking scan filtering with HESSID and access network type"""
    try:
        _test_ap_interworking_scan_filtering(dev, apdev)
    finally:
        dev[0].request("SET hessid 00:00:00:00:00:00")
        dev[0].request("SET access_network_type 15")

def _test_ap_interworking_scan_filtering(dev, apdev):
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    ssid = "test-hs20-ap1"
    params['ssid'] = ssid
    params['hessid'] = bssid
    hapd0 = hostapd.add_ap(apdev[0], params)

    bssid2 = apdev[1]['bssid']
    params = hs20_ap_params()
    ssid2 = "test-hs20-ap2"
    params['ssid'] = ssid2
    params['hessid'] = bssid2
    params['access_network_type'] = "1"
    del params['venue_group']
    del params['venue_type']
    hostapd.add_ap(apdev[1], params)

    dev[0].hs20_enable()

    Wlantest.setup(hapd0)
    wt = Wlantest()
    wt.flush()

    # Make sure wlantest has seen both BSSs to avoid issues in trying to clear
    # counters for non-existing BSS.
    dev[0].scan_for_bss(bssid, freq="2412")
    dev[0].scan_for_bss(bssid2, freq="2412")
    wt.clear_bss_counters(bssid)
    wt.clear_bss_counters(bssid2)

    logger.info("Check probe request filtering based on HESSID")

    dev[0].request("SET hessid " + bssid2)
    dev[0].scan(freq="2412")
    time.sleep(0.03)
    check_probe_resp(wt, bssid, bssid2)

    logger.info("Check probe request filtering based on access network type")

    wt.clear_bss_counters(bssid)
    wt.clear_bss_counters(bssid2)
    dev[0].request("SET hessid 00:00:00:00:00:00")
    dev[0].request("SET access_network_type 14")
    dev[0].scan(freq="2412")
    time.sleep(0.03)
    check_probe_resp(wt, bssid2, bssid)

    wt.clear_bss_counters(bssid)
    wt.clear_bss_counters(bssid2)
    dev[0].request("SET hessid 00:00:00:00:00:00")
    dev[0].request("SET access_network_type 1")
    dev[0].scan(freq="2412")
    time.sleep(0.03)
    check_probe_resp(wt, bssid, bssid2)

    logger.info("Check probe request filtering based on HESSID and ANT")

    wt.clear_bss_counters(bssid)
    wt.clear_bss_counters(bssid2)
    dev[0].request("SET hessid " + bssid)
    dev[0].request("SET access_network_type 14")
    dev[0].scan(freq="2412")
    time.sleep(0.03)
    check_probe_resp(wt, bssid2, bssid)

    wt.clear_bss_counters(bssid)
    wt.clear_bss_counters(bssid2)
    dev[0].request("SET hessid " + bssid2)
    dev[0].request("SET access_network_type 14")
    dev[0].scan(freq="2412")
    time.sleep(0.03)
    check_probe_resp(wt, bssid, None)
    check_probe_resp(wt, bssid2, None)

    wt.clear_bss_counters(bssid)
    wt.clear_bss_counters(bssid2)
    dev[0].request("SET hessid " + bssid)
    dev[0].request("SET access_network_type 1")
    dev[0].scan(freq="2412")
    time.sleep(0.03)
    check_probe_resp(wt, bssid, None)
    check_probe_resp(wt, bssid2, None)

def test_ap_hs20_select(dev, apdev):
    """Hotspot 2.0 network selection"""
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid
    hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable()
    id = dev[0].add_cred_values({'realm': "example.com", 'username': "test",
                                 'password': "secret",
                                 'domain': "example.com"})
    interworking_select(dev[0], bssid, "home")

    dev[0].remove_cred(id)
    id = dev[0].add_cred_values({'realm': "example.com", 'username': "test",
                                 'password': "secret",
                                 'domain': "no.match.example.com"})
    interworking_select(dev[0], bssid, "roaming", freq="2412")

    dev[0].set_cred_quoted(id, "realm", "no.match.example.com")
    interworking_select(dev[0], bssid, no_match=True, freq="2412")

    res = dev[0].request("SCAN_RESULTS")
    if "[HS20]" not in res:
        raise Exception("HS20 flag missing from scan results: " + res)

    bssid2 = apdev[1]['bssid']
    params = hs20_ap_params()
    params['nai_realm'] = ["0,example.org,21"]
    params['hessid'] = bssid2
    params['domain_name'] = "example.org"
    hostapd.add_ap(apdev[1], params)
    dev[0].remove_cred(id)
    id = dev[0].add_cred_values({'realm': "example.org", 'username': "test",
                                 'password': "secret",
                                 'domain': "example.org"})
    interworking_select(dev[0], bssid2, "home", freq="2412")

def hs20_simulated_sim(dev, ap, method):
    bssid = ap['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid
    params['anqp_3gpp_cell_net'] = "555,444"
    params['domain_name'] = "wlan.mnc444.mcc555.3gppnetwork.org"
    hostapd.add_ap(ap, params)

    dev.hs20_enable()
    dev.add_cred_values({'imsi': "555444-333222111", 'eap': method,
                         'milenage': "5122250214c33e723a5dd523fc145fc0:981d464c7c52eb6e5036234984ad0bcf:000000000123"})
    interworking_select(dev, bssid, "home", freq="2412")
    interworking_connect(dev, bssid, method)
    check_sp_type(dev, "home")

def test_ap_hs20_sim(dev, apdev):
    """Hotspot 2.0 with simulated SIM and EAP-SIM"""
    hlr_auc_gw_available()
    hs20_simulated_sim(dev[0], apdev[0], "SIM")
    dev[0].request("INTERWORKING_SELECT auto freq=2412")
    ev = dev[0].wait_event(["INTERWORKING-ALREADY-CONNECTED"], timeout=15)
    if ev is None:
        raise Exception("Timeout on already-connected event")

def test_ap_hs20_sim_invalid(dev, apdev):
    """Hotspot 2.0 with simulated SIM and EAP-SIM - invalid IMSI"""
    hlr_auc_gw_available()
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid
    params['anqp_3gpp_cell_net'] = "555,444"
    params['domain_name'] = "wlan.mnc444.mcc555.3gppnetwork.org"
    hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable()
    dev[0].add_cred_values({'imsi': "555444-3332221110", 'eap': "SIM",
                            'milenage': "5122250214c33e723a5dd523fc145fc0:981d464c7c52eb6e5036234984ad0bcf:000000000123"})
    # This hits "No valid IMSI available" in build_root_nai()
    interworking_select(dev[0], bssid, freq="2412")

def test_ap_hs20_sim_oom(dev, apdev):
    """Hotspot 2.0 with simulated SIM and EAP-SIM - OOM"""
    hlr_auc_gw_available()
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid
    params['anqp_3gpp_cell_net'] = "555,444"
    params['domain_name'] = "wlan.mnc444.mcc555.3gppnetwork.org"
    hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable()
    dev[0].add_cred_values({'imsi': "555444-333222111", 'eap': "SIM",
                            'milenage': "5122250214c33e723a5dd523fc145fc0:981d464c7c52eb6e5036234984ad0bcf:000000000123"})
    dev[0].scan_for_bss(bssid, freq=2412)
    interworking_select(dev[0], bssid, freq="2412")

    with alloc_fail(dev[0], 1, "wpa_config_add_network;interworking_connect_3gpp"):
        dev[0].request("INTERWORKING_CONNECT " + bssid)
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")

    with alloc_fail(dev[0], 1, "=interworking_connect_3gpp"):
        dev[0].request("INTERWORKING_CONNECT " + bssid)
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")

def test_ap_hs20_aka(dev, apdev):
    """Hotspot 2.0 with simulated USIM and EAP-AKA"""
    hlr_auc_gw_available()
    hs20_simulated_sim(dev[0], apdev[0], "AKA")

def test_ap_hs20_aka_prime(dev, apdev):
    """Hotspot 2.0 with simulated USIM and EAP-AKA'"""
    hlr_auc_gw_available()
    hs20_simulated_sim(dev[0], apdev[0], "AKA'")

def test_ap_hs20_ext_sim(dev, apdev):
    """Hotspot 2.0 with external SIM processing"""
    hlr_auc_gw_available()
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid
    params['anqp_3gpp_cell_net'] = "232,01"
    params['domain_name'] = "wlan.mnc001.mcc232.3gppnetwork.org"
    hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable()
    try:
        dev[0].request("SET external_sim 1")
        dev[0].add_cred_values({'imsi': "23201-0000000000", 'eap': "SIM"})
        interworking_select(dev[0], bssid, "home", freq="2412")
        interworking_ext_sim_connect(dev[0], bssid, "SIM")
        check_sp_type(dev[0], "home")
    finally:
        dev[0].request("SET external_sim 0")

def test_ap_hs20_ext_sim_roaming(dev, apdev):
    """Hotspot 2.0 with external SIM processing in roaming network"""
    hlr_auc_gw_available()
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid
    params['anqp_3gpp_cell_net'] = "244,91;310,026;232,01;234,56"
    params['domain_name'] = "wlan.mnc091.mcc244.3gppnetwork.org"
    hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable()
    try:
        dev[0].request("SET external_sim 1")
        dev[0].add_cred_values({'imsi': "23201-0000000000", 'eap': "SIM"})
        interworking_select(dev[0], bssid, "roaming", freq="2412")
        interworking_ext_sim_connect(dev[0], bssid, "SIM")
        check_sp_type(dev[0], "roaming")
    finally:
        dev[0].request("SET external_sim 0")

def test_ap_hs20_username(dev, apdev):
    """Hotspot 2.0 connection in username/password credential"""
    check_eap_capa(dev[0], "MSCHAPV2")
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid
    params['disable_dgaf'] = '1'
    hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable()
    id = dev[0].add_cred_values({'realm': "example.com",
                                 'username': "hs20-test",
                                 'password': "password",
                                 'ca_cert': "auth_serv/ca.pem",
                                 'domain': "example.com",
                                 'update_identifier': "1234"})
    interworking_select(dev[0], bssid, "home", freq="2412")
    interworking_connect(dev[0], bssid, "TTLS")
    check_sp_type(dev[0], "home")
    status = dev[0].get_status()
    if status['pairwise_cipher'] != "CCMP":
        raise Exception("Unexpected pairwise cipher")
    if status['hs20'] != "3":
        raise Exception("Unexpected HS 2.0 support indication")

    dev[1].connect("test-hs20", key_mgmt="WPA-EAP", eap="TTLS",
                   identity="hs20-test", password="password",
                   ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2",
                   scan_freq="2412")

def test_ap_hs20_connect_api(dev, apdev):
    """Hotspot 2.0 connection with connect API"""
    check_eap_capa(dev[0], "MSCHAPV2")
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid
    params['disable_dgaf'] = '1'
    hostapd.add_ap(apdev[0], params)

    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    wpas.interface_add("wlan5", drv_params="force_connect_cmd=1")
    wpas.hs20_enable()
    wpas.flush_scan_cache()
    id = wpas.add_cred_values({'realm': "example.com",
                               'username': "hs20-test",
                               'password': "password",
                               'ca_cert': "auth_serv/ca.pem",
                               'domain': "example.com",
                               'update_identifier': "1234"})
    interworking_select(wpas, bssid, "home", freq="2412")
    interworking_connect(wpas, bssid, "TTLS")
    check_sp_type(wpas, "home")
    status = wpas.get_status()
    if status['pairwise_cipher'] != "CCMP":
        raise Exception("Unexpected pairwise cipher")
    if status['hs20'] != "3":
        raise Exception("Unexpected HS 2.0 support indication")

def test_ap_hs20_auto_interworking(dev, apdev):
    """Hotspot 2.0 connection with auto_interworking=1"""
    check_eap_capa(dev[0], "MSCHAPV2")
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid
    params['disable_dgaf'] = '1'
    hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable(auto_interworking=True)
    id = dev[0].add_cred_values({'realm': "example.com",
                                 'username': "hs20-test",
                                 'password': "password",
                                 'ca_cert': "auth_serv/ca.pem",
                                 'domain': "example.com",
                                 'update_identifier': "1234"})
    dev[0].request("REASSOCIATE")
    dev[0].wait_connected(timeout=15)
    check_sp_type(dev[0], "home")
    status = dev[0].get_status()
    if status['pairwise_cipher'] != "CCMP":
        raise Exception("Unexpected pairwise cipher")
    if status['hs20'] != "3":
        raise Exception("Unexpected HS 2.0 support indication")

def test_ap_hs20_auto_interworking_global_pmf(dev, apdev):
    """Hotspot 2.0 connection with auto_interworking=1 and pmf=2"""
    check_eap_capa(dev[0], "MSCHAPV2")
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid
    hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable(auto_interworking=True)
    id = dev[0].add_cred_values({'realm': "example.com",
                                 'username': "hs20-test",
                                 'password': "password",
                                 'ca_cert': "auth_serv/ca.pem",
                                 'domain': "example.com",
                                 'update_identifier': "1234"})
    try:
        dev[0].set("pmf", "2")
        dev[0].request("REASSOCIATE")
        dev[0].wait_connected(timeout=15)
        pmf = dev[0].get_status_field("pmf")
        if pmf != "1":
            raise Exception("Unexpected PMF state: " + str(pmf))
    finally:
        dev[0].set("pmf", "0")

def test_ap_hs20_auto_interworking_global_pmf_fail(dev, apdev):
    """Hotspot 2.0 connection with auto_interworking=1 and pmf=2 failure"""
    check_eap_capa(dev[0], "MSCHAPV2")
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['ieee80211w'] = "0"
    params['hessid'] = bssid
    hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable(auto_interworking=True)
    id = dev[0].add_cred_values({'realm': "example.com",
                                 'username': "hs20-test",
                                 'password': "password",
                                 'ca_cert': "auth_serv/ca.pem",
                                 'domain': "example.com",
                                 'update_identifier': "1234"})
    try:
        dev[0].set("pmf", "2")
        dev[0].request("REASSOCIATE")
        for i in range(2):
            ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED",
                                    "INTERWORKING-SELECTED"], timeout=15)
            if ev is None:
                raise Exception("Connection result not reported")
            if "CTRL-EVENT-CONNECTED" in ev:
                raise Exception("Unexpected connection")
        dev[0].request("DISCONNECT")
    finally:
        dev[0].set("pmf", "0")

@remote_compatible
def test_ap_hs20_auto_interworking_no_match(dev, apdev):
    """Hotspot 2.0 connection with auto_interworking=1 and no matching network"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": "mismatch"})

    dev[0].hs20_enable(auto_interworking=True)
    id = dev[0].connect("mismatch", psk="12345678", scan_freq="2412",
                        only_add_network=True)
    dev[0].request("ENABLE_NETWORK " + str(id) + " no-connect")

    id = dev[0].add_cred_values({'realm': "example.com",
                                 'username': "hs20-test",
                                 'password': "password",
                                 'ca_cert': "auth_serv/ca.pem",
                                 'domain': "example.com",
                                 'update_identifier': "1234"})
    dev[0].request("INTERWORKING_SELECT auto freq=2412")
    time.sleep(0.1)
    dev[0].dump_monitor()
    for i in range(5):
        logger.info("start ping")
        if "PONG" not in dev[0].ctrl.request("PING", timeout=2):
            raise Exception("PING failed")
        logger.info("ping done")
        fetch = 0
        scan = 0
        for j in range(15):
            ev = dev[0].wait_event(["ANQP fetch completed",
                                    "CTRL-EVENT-SCAN-RESULTS"], timeout=0.05)
            if ev is None:
                break
            if "ANQP fetch completed" in ev:
                fetch += 1
            else:
                scan += 1
        if fetch > 2 * scan + 3:
            raise Exception("Too many ANQP fetch iterations")
        dev[0].dump_monitor()
    dev[0].request("DISCONNECT")

@remote_compatible
def test_ap_hs20_auto_interworking_no_cred_match(dev, apdev):
    """Hotspot 2.0 connection with auto_interworking=1 but no cred match"""
    bssid = apdev[0]['bssid']
    params = {"ssid": "test"}
    hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable(auto_interworking=True)
    dev[0].add_cred_values({'realm': "example.com",
                            'username': "hs20-test",
                            'password': "password",
                            'ca_cert': "auth_serv/ca.pem",
                            'domain': "example.com"})

    id = dev[0].connect("test", psk="12345678", only_add_network=True)
    dev[0].request("ENABLE_NETWORK %s" % id)
    logger.info("Verify that scanning continues when there is partial network block match")
    for i in range(0, 2):
        ev = dev[0].wait_event(["CTRL-EVENT-SCAN-RESULTS"], 10)
        if ev is None:
            raise Exception("Scan timed out")
        logger.info("Scan completed")

def eap_test(dev, ap, eap_params, method, user, release=0):
    bssid = ap['bssid']
    params = hs20_ap_params()
    params['nai_realm'] = ["0,example.com," + eap_params]
    if release > 0:
        params['hs20_release'] = str(release)
    hapd = hostapd.add_ap(ap, params)

    dev.flush_scan_cache()
    dev.hs20_enable()
    dev.add_cred_values({'realm': "example.com",
                         'ca_cert': "auth_serv/ca.pem",
                         'username': user,
                         'password': "password"})
    interworking_select(dev, bssid, freq="2412")
    interworking_connect(dev, bssid, method)
    return hapd

@remote_compatible
def test_ap_hs20_eap_unknown(dev, apdev):
    """Hotspot 2.0 connection with unknown EAP method"""
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['nai_realm'] = "0,example.com,99"
    hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable()
    dev[0].add_cred_values(default_cred())
    interworking_select(dev[0], None, no_match=True, freq="2412")

def test_ap_hs20_eap_peap_mschapv2(dev, apdev):
    """Hotspot 2.0 connection with PEAP/MSCHAPV2"""
    check_eap_capa(dev[0], "MSCHAPV2")
    eap_test(dev[0], apdev[0], "25[3:26]", "PEAP", "user")

def test_ap_hs20_eap_peap_default(dev, apdev):
    """Hotspot 2.0 connection with PEAP/MSCHAPV2 (as default)"""
    check_eap_capa(dev[0], "MSCHAPV2")
    eap_test(dev[0], apdev[0], "25", "PEAP", "user")

def test_ap_hs20_eap_peap_gtc(dev, apdev):
    """Hotspot 2.0 connection with PEAP/GTC"""
    eap_test(dev[0], apdev[0], "25[3:6]", "PEAP", "user")

@remote_compatible
def test_ap_hs20_eap_peap_unknown(dev, apdev):
    """Hotspot 2.0 connection with PEAP/unknown"""
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['nai_realm'] = "0,example.com,25[3:99]"
    hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable()
    dev[0].add_cred_values(default_cred())
    interworking_select(dev[0], None, no_match=True, freq="2412")

def test_ap_hs20_eap_ttls_chap(dev, apdev):
    """Hotspot 2.0 connection with TTLS/CHAP"""
    skip_with_fips(dev[0])
    eap_test(dev[0], apdev[0], "21[2:2]", "TTLS", "chap user")

def test_ap_hs20_eap_ttls_mschap(dev, apdev):
    """Hotspot 2.0 connection with TTLS/MSCHAP"""
    skip_with_fips(dev[0])
    eap_test(dev[0], apdev[0], "21[2:3]", "TTLS", "mschap user")

def test_ap_hs20_eap_ttls_default(dev, apdev):
    """Hotspot 2.0 connection with TTLS/default"""
    skip_with_fips(dev[0])
    eap_test(dev[0], apdev[0], "21", "TTLS", "hs20-test")

def test_ap_hs20_eap_ttls_eap_mschapv2(dev, apdev):
    """Hotspot 2.0 connection with TTLS/EAP-MSCHAPv2"""
    check_eap_capa(dev[0], "MSCHAPV2")
    eap_test(dev[0], apdev[0], "21[3:26][6:7][99:99]", "TTLS", "user")

@remote_compatible
def test_ap_hs20_eap_ttls_eap_unknown(dev, apdev):
    """Hotspot 2.0 connection with TTLS/EAP-unknown"""
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['nai_realm'] = "0,example.com,21[3:99]"
    hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable()
    dev[0].add_cred_values(default_cred())
    interworking_select(dev[0], None, no_match=True, freq="2412")

@remote_compatible
def test_ap_hs20_eap_ttls_eap_unsupported(dev, apdev):
    """Hotspot 2.0 connection with TTLS/EAP-OTP(unsupported)"""
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['nai_realm'] = "0,example.com,21[3:5]"
    hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable()
    dev[0].add_cred_values(default_cred())
    interworking_select(dev[0], None, no_match=True, freq="2412")

@remote_compatible
def test_ap_hs20_eap_ttls_unknown(dev, apdev):
    """Hotspot 2.0 connection with TTLS/unknown"""
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['nai_realm'] = "0,example.com,21[2:5]"
    hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable()
    dev[0].add_cred_values(default_cred())
    interworking_select(dev[0], None, no_match=True, freq="2412")

def test_ap_hs20_eap_fast_mschapv2(dev, apdev):
    """Hotspot 2.0 connection with FAST/EAP-MSCHAPV2"""
    check_eap_capa(dev[0], "FAST")
    eap_test(dev[0], apdev[0], "43[3:26]", "FAST", "user")

def test_ap_hs20_eap_fast_gtc(dev, apdev):
    """Hotspot 2.0 connection with FAST/EAP-GTC"""
    check_eap_capa(dev[0], "FAST")
    eap_test(dev[0], apdev[0], "43[3:6]", "FAST", "user")

def test_ap_hs20_eap_tls(dev, apdev):
    """Hotspot 2.0 connection with EAP-TLS"""
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['nai_realm'] = ["0,example.com,13[5:6]"]
    hostapd.add_ap(apdev[0], params)

    dev[0].flush_scan_cache()
    dev[0].hs20_enable()
    dev[0].add_cred_values({'realm': "example.com",
                            'username': "certificate-user",
                            'ca_cert': "auth_serv/ca.pem",
                            'client_cert': "auth_serv/user.pem",
                            'private_key': "auth_serv/user.key"})
    interworking_select(dev[0], bssid, freq="2412")
    interworking_connect(dev[0], bssid, "TLS")

@remote_compatible
def test_ap_hs20_eap_cert_unknown(dev, apdev):
    """Hotspot 2.0 connection with certificate, but unknown EAP method"""
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['nai_realm'] = ["0,example.com,99[5:6]"]
    hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable()
    dev[0].add_cred_values({'realm': "example.com",
                            'username': "certificate-user",
                            'ca_cert': "auth_serv/ca.pem",
                            'client_cert': "auth_serv/user.pem",
                            'private_key': "auth_serv/user.key"})
    interworking_select(dev[0], None, no_match=True, freq="2412")

@remote_compatible
def test_ap_hs20_eap_cert_unsupported(dev, apdev):
    """Hotspot 2.0 connection with certificate, but unsupported TTLS"""
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['nai_realm'] = ["0,example.com,21[5:6]"]
    hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable()
    dev[0].add_cred_values({'realm': "example.com",
                            'username': "certificate-user",
                            'ca_cert': "auth_serv/ca.pem",
                            'client_cert': "auth_serv/user.pem",
                            'private_key': "auth_serv/user.key"})
    interworking_select(dev[0], None, no_match=True, freq="2412")

@remote_compatible
def test_ap_hs20_eap_invalid_cred(dev, apdev):
    """Hotspot 2.0 connection with invalid cred configuration"""
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable()
    dev[0].add_cred_values({'realm': "example.com",
                            'username': "certificate-user",
                            'client_cert': "auth_serv/user.pem"})
    interworking_select(dev[0], None, no_match=True, freq="2412")

def test_ap_hs20_nai_realms(dev, apdev):
    """Hotspot 2.0 connection and multiple NAI realms and TTLS/PAP"""
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid
    params['nai_realm'] = ["0,no.match.here;example.com;no.match.here.either,21[2:1][5:7]"]
    hostapd.add_ap(apdev[0], params)

    dev[0].flush_scan_cache()
    dev[0].hs20_enable()
    id = dev[0].add_cred_values({'realm': "example.com",
                                 'ca_cert': "auth_serv/ca.pem",
                                 'username': "pap user",
                                 'password': "password",
                                 'domain': "example.com"})
    interworking_select(dev[0], bssid, "home", freq="2412")
    interworking_connect(dev[0], bssid, "TTLS")
    check_sp_type(dev[0], "home")

def test_ap_hs20_roaming_consortium(dev, apdev):
    """Hotspot 2.0 connection based on roaming consortium match"""
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid
    hostapd.add_ap(apdev[0], params)

    dev[0].flush_scan_cache()
    dev[0].hs20_enable()
    for consortium in ["112233", "1020304050", "010203040506", "fedcba"]:
        id = dev[0].add_cred_values({'username': "user",
                                     'password': "password",
                                     'domain': "example.com",
                                     'ca_cert': "auth_serv/ca.pem",
                                     'roaming_consortium': consortium,
                                     'eap': "PEAP"})
        interworking_select(dev[0], bssid, "home", freq="2412")
        interworking_connect(dev[0], bssid, "PEAP")
        check_sp_type(dev[0], "home")
        dev[0].request("INTERWORKING_SELECT auto freq=2412")
        ev = dev[0].wait_event(["INTERWORKING-ALREADY-CONNECTED"], timeout=15)
        if ev is None:
            raise Exception("Timeout on already-connected event")
        dev[0].remove_cred(id)

def test_ap_hs20_roaming_consortiums_match(dev, apdev):
    """Hotspot 2.0 connection based on roaming_consortiums match"""
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid
    hostapd.add_ap(apdev[0], params)

    dev[0].flush_scan_cache()
    dev[0].hs20_enable()
    tests = [("112233", "112233"),
             ("ffffff,1020304050,eeeeee", "1020304050")]
    for consortium, selected in tests:
        id = dev[0].add_cred_values({'username': "user",
                                     'password': "password",
                                     'domain': "my.home.example.com",
                                     'ca_cert': "auth_serv/ca.pem",
                                     'roaming_consortiums': consortium,
                                     'eap': "PEAP"})
        interworking_select(dev[0], bssid, "roaming", freq="2412")
        interworking_connect(dev[0], bssid, "PEAP")
        check_sp_type(dev[0], "roaming")
        network_id = dev[0].get_status_field("id")
        sel = dev[0].get_network(network_id, "roaming_consortium_selection")
        if sel != selected:
            raise Exception("Unexpected roaming_consortium_selection value: " +
                            sel)
        dev[0].request("INTERWORKING_SELECT auto freq=2412")
        ev = dev[0].wait_event(["INTERWORKING-ALREADY-CONNECTED"], timeout=15)
        if ev is None:
            raise Exception("Timeout on already-connected event")
        dev[0].remove_cred(id)

def test_ap_hs20_max_roaming_consortiums(dev, apdev):
    """Maximum number of cred roaming_consortiums"""
    id = dev[0].add_cred()
    consortium = (36*",ffffff")[1:]
    if "OK" not in dev[0].request('SET_CRED %d roaming_consortiums "%s"' % (id, consortium)):
        raise Exception("Maximum number of consortium OIs rejected")
    consortium = (37*",ffffff")[1:]
    if "FAIL" not in dev[0].request('SET_CRED %d roaming_consortiums "%s"' % (id, consortium)):
        raise Exception("Over maximum number of consortium OIs accepted")
    dev[0].remove_cred(id)

def test_ap_hs20_roaming_consortium_invalid(dev, apdev):
    """Hotspot 2.0 connection and invalid roaming consortium ANQP-element"""
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid
    # Override Roaming Consortium ANQP-element with an incorrectly encoded
    # value.
    params['anqp_elem'] = "261:04fedcba"
    hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable()
    id = dev[0].add_cred_values({'username': "user",
                                 'password': "password",
                                 'domain': "example.com",
                                 'ca_cert': "auth_serv/ca.pem",
                                 'roaming_consortium': "fedcba",
                                 'eap': "PEAP"})
    interworking_select(dev[0], bssid, "home", freq="2412", no_match=True)

def test_ap_hs20_roaming_consortium_element(dev, apdev):
    """Hotspot 2.0 connection and invalid roaming consortium element"""
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid
    del params['roaming_consortium']
    params['vendor_elements'] = '6f00'
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable()
    dev[0].scan_for_bss(bssid, freq="2412")
    id = dev[0].add_cred_values({'username': "user",
                                 'password': "password",
                                 'domain': "example.com",
                                 'ca_cert': "auth_serv/ca.pem",
                                 'roaming_consortium': "112233",
                                 'eap': "PEAP"})
    interworking_select(dev[0], bssid, freq="2412", no_match=True)

    hapd.set('vendor_elements', '6f020001')
    if "OK" not in hapd.request("UPDATE_BEACON"):
        raise Exception("UPDATE_BEACON failed")
    dev[0].request("BSS_FLUSH 0")
    dev[0].scan_for_bss(bssid, freq="2412", force_scan=True)
    interworking_select(dev[0], bssid, freq="2412", no_match=True)

def test_ap_hs20_roaming_consortium_constraints(dev, apdev):
    """Hotspot 2.0 connection and roaming consortium constraints"""
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid
    params['bss_load_test'] = "12:200:20000"
    hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable()

    vals = {'username': "user",
            'password': "password",
            'domain': "example.com",
            'ca_cert': "auth_serv/ca.pem",
            'roaming_consortium': "fedcba",
            'eap': "TTLS"}
    vals2 = vals.copy()
    vals2['required_roaming_consortium'] = "223344"
    id = dev[0].add_cred_values(vals2)
    interworking_select(dev[0], bssid, "home", freq="2412", no_match=True)
    dev[0].remove_cred(id)

    vals2 = vals.copy()
    vals2['min_dl_bandwidth_home'] = "65500"
    id = dev[0].add_cred_values(vals2)
    dev[0].request("INTERWORKING_SELECT freq=2412")
    ev = dev[0].wait_event(["INTERWORKING-AP"], timeout=15)
    if ev is None:
        raise Exception("No AP found")
    if "below_min_backhaul=1" not in ev:
        raise Exception("below_min_backhaul not reported")
    dev[0].remove_cred(id)

    vals2 = vals.copy()
    vals2['max_bss_load'] = "100"
    id = dev[0].add_cred_values(vals2)
    dev[0].request("INTERWORKING_SELECT freq=2412")
    ev = dev[0].wait_event(["INTERWORKING-AP"], timeout=15)
    if ev is None:
        raise Exception("No AP found")
    if "over_max_bss_load=1" not in ev:
        raise Exception("over_max_bss_load not reported")
    dev[0].remove_cred(id)

    vals2 = vals.copy()
    vals2['req_conn_capab'] = "6:1234"
    vals2['domain'] = 'example.org'
    id = dev[0].add_cred_values(vals2)

    dev[0].request("INTERWORKING_SELECT freq=2412")
    ev = dev[0].wait_event(["INTERWORKING-AP"], timeout=15)
    if ev is None:
        raise Exception("No AP found")
    if "conn_capab_missing=1" not in ev:
        raise Exception("conn_capab_missing not reported")
    dev[0].remove_cred(id)

    values = default_cred()
    values['roaming_consortium'] = "fedcba"
    id3 = dev[0].add_cred_values(values)

    vals2 = vals.copy()
    vals2['roaming_consortium'] = "fedcba"
    vals2['priority'] = "2"
    id = dev[0].add_cred_values(vals2)

    values = default_cred()
    values['roaming_consortium'] = "fedcba"
    id2 = dev[0].add_cred_values(values)

    dev[0].request("INTERWORKING_SELECT freq=2412")
    ev = dev[0].wait_event(["INTERWORKING-AP"], timeout=15)
    if ev is None:
        raise Exception("No AP found")
    dev[0].remove_cred(id)
    dev[0].remove_cred(id2)
    dev[0].remove_cred(id3)

def test_ap_hs20_3gpp_constraints(dev, apdev):
    """Hotspot 2.0 connection and 3GPP credential constraints"""
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid
    params['anqp_3gpp_cell_net'] = "555,444"
    params['domain_name'] = "wlan.mnc444.mcc555.3gppnetwork.org"
    params['bss_load_test'] = "12:200:20000"
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable()

    vals = {'imsi': "555444-333222111",
            'eap': "SIM",
            'milenage': "5122250214c33e723a5dd523fc145fc0:981d464c7c52eb6e5036234984ad0bcf:000000000123"}
    vals2 = vals.copy()
    vals2['required_roaming_consortium'] = "223344"
    id = dev[0].add_cred_values(vals2)
    interworking_select(dev[0], bssid, "home", freq="2412", no_match=True)
    dev[0].remove_cred(id)

    vals2 = vals.copy()
    vals2['min_dl_bandwidth_home'] = "65500"
    id = dev[0].add_cred_values(vals2)
    dev[0].request("INTERWORKING_SELECT freq=2412")
    ev = dev[0].wait_event(["INTERWORKING-AP"], timeout=15)
    if ev is None:
        raise Exception("No AP found")
    if "below_min_backhaul=1" not in ev:
        raise Exception("below_min_backhaul not reported")
    dev[0].remove_cred(id)

    vals2 = vals.copy()
    vals2['max_bss_load'] = "100"
    id = dev[0].add_cred_values(vals2)
    dev[0].request("INTERWORKING_SELECT freq=2412")
    ev = dev[0].wait_event(["INTERWORKING-AP"], timeout=15)
    if ev is None:
        raise Exception("No AP found")
    if "over_max_bss_load=1" not in ev:
        raise Exception("over_max_bss_load not reported")
    dev[0].remove_cred(id)

    values = default_cred()
    values['roaming_consortium'] = "fedcba"
    id3 = dev[0].add_cred_values(values)

    vals2 = vals.copy()
    vals2['roaming_consortium'] = "fedcba"
    vals2['priority'] = "2"
    id = dev[0].add_cred_values(vals2)

    values = default_cred()
    values['roaming_consortium'] = "fedcba"
    id2 = dev[0].add_cred_values(values)

    dev[0].request("INTERWORKING_SELECT freq=2412")
    ev = dev[0].wait_event(["INTERWORKING-AP"], timeout=15)
    if ev is None:
        raise Exception("No AP found")
    dev[0].remove_cred(id)
    dev[0].remove_cred(id2)
    dev[0].remove_cred(id3)

    hapd.disable()
    params = hs20_ap_params()
    params['hessid'] = bssid
    params['anqp_3gpp_cell_net'] = "555,444"
    params['bss_load_test'] = "12:200:20000"
    hapd = hostapd.add_ap(apdev[0], params)
    vals2 = vals.copy()
    vals2['req_conn_capab'] = "6:1234"
    id = dev[0].add_cred_values(vals2)
    dev[0].request("INTERWORKING_SELECT freq=2412")
    ev = dev[0].wait_event(["INTERWORKING-AP"], timeout=15)
    if ev is None:
        raise Exception("No AP found")
    if "conn_capab_missing=1" not in ev:
        raise Exception("conn_capab_missing not reported")
    dev[0].remove_cred(id)

def test_ap_hs20_connect_no_full_match(dev, apdev):
    """Hotspot 2.0 connection and no full match"""
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid
    params['anqp_3gpp_cell_net'] = "555,444"
    hostapd.add_ap(apdev[0], params)

    dev[0].flush_scan_cache()
    dev[0].hs20_enable()

    vals = {'username': "user",
            'password': "password",
            'domain': "example.com",
            'ca_cert': "auth_serv/ca.pem",
            'roaming_consortium': "fedcba",
            'eap': "TTLS",
            'min_dl_bandwidth_home': "65500"}
    id = dev[0].add_cred_values(vals)
    dev[0].request("INTERWORKING_SELECT freq=2412")
    ev = dev[0].wait_event(["INTERWORKING-AP"], timeout=15)
    if ev is None:
        raise Exception("No AP found")
    if "below_min_backhaul=1" not in ev:
        raise Exception("below_min_backhaul not reported")
    interworking_connect(dev[0], bssid, "TTLS")
    dev[0].remove_cred(id)
    dev[0].wait_disconnected()

    vals = {'imsi': "555444-333222111", 'eap': "SIM",
            'milenage': "5122250214c33e723a5dd523fc145fc0:981d464c7c52eb6e5036234984ad0bcf:000000000123",
            'min_dl_bandwidth_roaming': "65500"}
    id = dev[0].add_cred_values(vals)
    dev[0].request("INTERWORKING_SELECT freq=2412")
    ev = dev[0].wait_event(["INTERWORKING-AP"], timeout=15)
    if ev is None:
        raise Exception("No AP found")
    if "below_min_backhaul=1" not in ev:
        raise Exception("below_min_backhaul not reported")
    interworking_connect(dev[0], bssid, "SIM")
    dev[0].remove_cred(id)
    dev[0].wait_disconnected()

def test_ap_hs20_username_roaming(dev, apdev):
    """Hotspot 2.0 connection in username/password credential (roaming)"""
    check_eap_capa(dev[0], "MSCHAPV2")
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['nai_realm'] = ["0,example.com,13[5:6],21[2:4][5:7]",
                           "0,roaming.example.com,21[2:4][5:7]",
                           "0,another.example.com"]
    params['domain_name'] = "another.example.com"
    params['hessid'] = bssid
    hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable()
    id = dev[0].add_cred_values({'realm': "roaming.example.com",
                                 'username': "hs20-test",
                                 'password': "password",
                                 'ca_cert': "auth_serv/ca.pem",
                                 'domain': "example.com"})
    interworking_select(dev[0], bssid, "roaming", freq="2412")
    interworking_connect(dev[0], bssid, "TTLS")
    check_sp_type(dev[0], "roaming")

def test_ap_hs20_username_unknown(dev, apdev):
    """Hotspot 2.0 connection in username/password credential (no domain in cred)"""
    check_eap_capa(dev[0], "MSCHAPV2")
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid
    hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable()
    id = dev[0].add_cred_values({'realm': "example.com",
                                 'ca_cert': "auth_serv/ca.pem",
                                 'username': "hs20-test",
                                 'password': "password"})
    interworking_select(dev[0], bssid, "unknown", freq="2412")
    interworking_connect(dev[0], bssid, "TTLS")
    check_sp_type(dev[0], "unknown")

def test_ap_hs20_username_unknown2(dev, apdev):
    """Hotspot 2.0 connection in username/password credential (no domain advertized)"""
    check_eap_capa(dev[0], "MSCHAPV2")
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid
    del params['domain_name']
    hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable()
    id = dev[0].add_cred_values({'realm': "example.com",
                                 'ca_cert': "auth_serv/ca.pem",
                                 'username': "hs20-test",
                                 'password': "password",
                                 'domain': "example.com"})
    interworking_select(dev[0], bssid, "unknown", freq="2412")
    interworking_connect(dev[0], bssid, "TTLS")
    check_sp_type(dev[0], "unknown")

def test_ap_hs20_gas_while_associated(dev, apdev):
    """Hotspot 2.0 connection with GAS query while associated"""
    check_eap_capa(dev[0], "MSCHAPV2")
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid
    hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable()
    id = dev[0].add_cred_values({'realm': "example.com",
                                 'ca_cert': "auth_serv/ca.pem",
                                 'username': "hs20-test",
                                 'password': "password",
                                 'domain': "example.com"})
    interworking_select(dev[0], bssid, "home", freq="2412")
    interworking_connect(dev[0], bssid, "TTLS")

    logger.info("Verifying GAS query while associated")
    dev[0].request("FETCH_ANQP")
    for i in range(0, 6):
        ev = dev[0].wait_event(["RX-ANQP"], timeout=5)
        if ev is None:
            raise Exception("Operation timed out")

def test_ap_hs20_gas_with_another_ap_while_associated(dev, apdev):
    """GAS query with another AP while associated"""
    check_eap_capa(dev[0], "MSCHAPV2")
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid
    hostapd.add_ap(apdev[0], params)

    bssid2 = apdev[1]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid2
    params['nai_realm'] = ["0,no-match.example.org,13[5:6],21[2:4][5:7]"]
    hostapd.add_ap(apdev[1], params)

    dev[0].hs20_enable()
    id = dev[0].add_cred_values({'realm': "example.com",
                                 'ca_cert': "auth_serv/ca.pem",
                                 'username': "hs20-test",
                                 'password': "password",
                                 'domain': "example.com"})
    interworking_select(dev[0], bssid, "home", freq="2412")
    interworking_connect(dev[0], bssid, "TTLS")
    dev[0].dump_monitor()

    logger.info("Verifying GAS query with same AP while associated")
    dev[0].request("ANQP_GET " + bssid + " 263")
    ev = dev[0].wait_event(["RX-ANQP"], timeout=5)
    if ev is None:
        raise Exception("ANQP operation timed out")
    dev[0].dump_monitor()

    logger.info("Verifying GAS query with another AP while associated")
    dev[0].scan_for_bss(bssid2, 2412)
    dev[0].request("ANQP_GET " + bssid2 + " 263")
    ev = dev[0].wait_event(["RX-ANQP"], timeout=5)
    if ev is None:
        raise Exception("ANQP operation timed out")

def test_ap_hs20_gas_while_associated_with_pmf(dev, apdev):
    """Hotspot 2.0 connection with GAS query while associated and using PMF"""
    check_eap_capa(dev[0], "MSCHAPV2")
    try:
        _test_ap_hs20_gas_while_associated_with_pmf(dev, apdev)
    finally:
        dev[0].request("SET pmf 0")

def _test_ap_hs20_gas_while_associated_with_pmf(dev, apdev):
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid
    hostapd.add_ap(apdev[0], params)

    bssid2 = apdev[1]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid2
    params['nai_realm'] = ["0,no-match.example.org,13[5:6],21[2:4][5:7]"]
    hostapd.add_ap(apdev[1], params)

    dev[0].flush_scan_cache()
    dev[0].hs20_enable()
    dev[0].request("SET pmf 2")
    id = dev[0].add_cred_values({'realm': "example.com",
                                 'ca_cert': "auth_serv/ca.pem",
                                 'username': "hs20-test",
                                 'password': "password",
                                 'domain': "example.com"})
    interworking_select(dev[0], bssid, "home", freq="2412")
    interworking_connect(dev[0], bssid, "TTLS")

    logger.info("Verifying GAS query while associated")
    dev[0].request("FETCH_ANQP")
    for i in range(0, 2 * 6):
        ev = dev[0].wait_event(["RX-ANQP"], timeout=5)
        if ev is None:
            raise Exception("Operation timed out")

def test_ap_hs20_gas_with_another_ap_while_using_pmf(dev, apdev):
    """GAS query with another AP while associated and using PMF"""
    check_eap_capa(dev[0], "MSCHAPV2")
    try:
        _test_ap_hs20_gas_with_another_ap_while_using_pmf(dev, apdev)
    finally:
        dev[0].request("SET pmf 0")

def _test_ap_hs20_gas_with_another_ap_while_using_pmf(dev, apdev):
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid
    hapd = hostapd.add_ap(apdev[0], params)

    bssid2 = apdev[1]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid2
    params['nai_realm'] = ["0,no-match.example.org,13[5:6],21[2:4][5:7]"]
    hostapd.add_ap(apdev[1], params)

    dev[0].hs20_enable()
    dev[0].request("SET pmf 2")
    id = dev[0].add_cred_values({'realm': "example.com",
                                 'ca_cert': "auth_serv/ca.pem",
                                 'username': "hs20-test",
                                 'password': "password",
                                 'domain': "example.com"})
    interworking_select(dev[0], bssid, "home", freq="2412")
    interworking_connect(dev[0], bssid, "TTLS")
    dev[0].dump_monitor()
    hapd.wait_sta()

    logger.info("Verifying GAS query with same AP while associated")
    dev[0].request("ANQP_GET " + bssid + " 263")
    ev = dev[0].wait_event(["RX-ANQP"], timeout=5)
    if ev is None:
        raise Exception("ANQP operation timed out")
    dev[0].dump_monitor()

    logger.info("Verifying GAS query with another AP while associated")
    dev[0].scan_for_bss(bssid2, 2412)
    dev[0].request("ANQP_GET " + bssid2 + " 263")
    ev = dev[0].wait_event(["RX-ANQP"], timeout=5)
    if ev is None:
        raise Exception("ANQP operation timed out")

def test_ap_hs20_gas_frag_while_associated(dev, apdev):
    """Hotspot 2.0 connection with fragmented GAS query while associated"""
    check_eap_capa(dev[0], "MSCHAPV2")
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid
    hapd = hostapd.add_ap(apdev[0], params)
    hapd.set("gas_frag_limit", "50")

    dev[0].hs20_enable()
    id = dev[0].add_cred_values({'realm': "example.com",
                                 'ca_cert': "auth_serv/ca.pem",
                                 'username': "hs20-test",
                                 'password': "password",
                                 'domain': "example.com"})
    interworking_select(dev[0], bssid, "home", freq="2412")
    interworking_connect(dev[0], bssid, "TTLS")
    hapd.wait_sta()

    logger.info("Verifying GAS query while associated")
    dev[0].request("FETCH_ANQP")
    for i in range(0, 6):
        ev = dev[0].wait_event(["RX-ANQP"], timeout=5)
        if ev is None:
            raise Exception("Operation timed out")

def test_ap_hs20_multiple_connects(dev, apdev):
    """Hotspot 2.0 connection through multiple network selections"""
    check_eap_capa(dev[0], "MSCHAPV2")
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid
    hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable()
    values = {'realm': "example.com",
              'ca_cert': "auth_serv/ca.pem",
              'username': "hs20-test",
              'password': "password",
              'domain': "example.com"}
    id = dev[0].add_cred_values(values)

    dev[0].scan_for_bss(bssid, freq="2412")

    for i in range(0, 3):
        logger.info("Starting Interworking network selection")
        dev[0].request("INTERWORKING_SELECT auto freq=2412")
        while True:
            ev = dev[0].wait_event(["INTERWORKING-NO-MATCH",
                                    "INTERWORKING-ALREADY-CONNECTED",
                                    "CTRL-EVENT-CONNECTED"], timeout=15)
            if ev is None:
                raise Exception("Connection timed out")
            if "INTERWORKING-NO-MATCH" in ev:
                raise Exception("Matching AP not found")
            if "CTRL-EVENT-CONNECTED" in ev:
                break
            if i == 2 and "INTERWORKING-ALREADY-CONNECTED" in ev:
                break
        if i == 0:
            dev[0].request("DISCONNECT")
        dev[0].dump_monitor()

    networks = dev[0].list_networks()
    if len(networks) > 1:
        raise Exception("Duplicated network block detected")

def test_ap_hs20_disallow_aps(dev, apdev):
    """Hotspot 2.0 connection and disallow_aps"""
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid
    hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable()
    values = {'realm': "example.com",
              'ca_cert': "auth_serv/ca.pem",
              'username': "hs20-test",
              'password': "password",
              'domain': "example.com"}
    id = dev[0].add_cred_values(values)

    dev[0].scan_for_bss(bssid, freq="2412")

    logger.info("Verify disallow_aps bssid")
    dev[0].request("SET disallow_aps bssid " + bssid.replace(':', ''))
    dev[0].request("INTERWORKING_SELECT auto")
    ev = dev[0].wait_event(["INTERWORKING-NO-MATCH"], timeout=15)
    if ev is None:
        raise Exception("Network selection timed out")
    dev[0].dump_monitor()

    logger.info("Verify disallow_aps ssid")
    dev[0].request("SET disallow_aps ssid 746573742d68733230")
    dev[0].request("INTERWORKING_SELECT auto freq=2412")
    ev = dev[0].wait_event(["INTERWORKING-NO-MATCH"], timeout=15)
    if ev is None:
        raise Exception("Network selection timed out")
    dev[0].dump_monitor()

    logger.info("Verify disallow_aps clear")
    dev[0].request("SET disallow_aps ")
    interworking_select(dev[0], bssid, "home", freq="2412")

    dev[0].request("SET disallow_aps bssid " + bssid.replace(':', ''))
    ret = dev[0].request("INTERWORKING_CONNECT " + bssid)
    if "FAIL" not in ret:
        raise Exception("INTERWORKING_CONNECT to disallowed BSS not rejected")

    if "FAIL" not in dev[0].request("INTERWORKING_CONNECT foo"):
        raise Exception("Invalid INTERWORKING_CONNECT not rejected")
    if "FAIL" not in dev[0].request("INTERWORKING_CONNECT 00:11:22:33:44:55"):
        raise Exception("Invalid INTERWORKING_CONNECT not rejected")

def policy_test(dev, ap, values, only_one=True):
    dev.dump_monitor()
    if ap:
        logger.info("Verify network selection to AP " + ap['ifname'])
        bssid = ap['bssid']
        dev.scan_for_bss(bssid, freq="2412")
    else:
        logger.info("Verify network selection")
        bssid = None
    dev.hs20_enable()
    id = dev.add_cred_values(values)
    dev.request("INTERWORKING_SELECT auto freq=2412")
    events = []
    while True:
        ev = dev.wait_event(["INTERWORKING-AP", "INTERWORKING-NO-MATCH",
                             "INTERWORKING-BLACKLISTED",
                             "INTERWORKING-SELECTED"], timeout=15)
        if ev is None:
            raise Exception("Network selection timed out")
        events.append(ev)
        if "INTERWORKING-NO-MATCH" in ev:
            raise Exception("Matching AP not found")
        if bssid and only_one and "INTERWORKING-AP" in ev and bssid not in ev:
            raise Exception("Unexpected AP claimed acceptable")
        if "INTERWORKING-SELECTED" in ev:
            if bssid and bssid not in ev:
                raise Exception("Selected incorrect BSS")
            break

    ev = dev.wait_connected(timeout=15)
    if bssid and bssid not in ev:
        raise Exception("Connected to incorrect BSS")

    conn_bssid = dev.get_status_field("bssid")
    if bssid and conn_bssid != bssid:
        raise Exception("bssid information points to incorrect BSS")

    dev.remove_cred(id)
    dev.dump_monitor()
    return events

def default_cred(domain=None, user="hs20-test"):
    cred = {'realm': "example.com",
            'ca_cert': "auth_serv/ca.pem",
            'username': user,
            'password': "password"}
    if domain:
        cred['domain'] = domain
    return cred

def test_ap_hs20_prefer_home(dev, apdev):
    """Hotspot 2.0 required roaming consortium"""
    check_eap_capa(dev[0], "MSCHAPV2")
    params = hs20_ap_params()
    params['domain_name'] = "example.org"
    hostapd.add_ap(apdev[0], params)

    params = hs20_ap_params()
    params['ssid'] = "test-hs20-other"
    params['domain_name'] = "example.com"
    hostapd.add_ap(apdev[1], params)

    values = default_cred()
    values['domain'] = "example.com"
    policy_test(dev[0], apdev[1], values, only_one=False)
    values['domain'] = "example.org"
    policy_test(dev[0], apdev[0], values, only_one=False)

def test_ap_hs20_req_roaming_consortium(dev, apdev):
    """Hotspot 2.0 required roaming consortium"""
    check_eap_capa(dev[0], "MSCHAPV2")
    params = hs20_ap_params()
    hostapd.add_ap(apdev[0], params)

    params = hs20_ap_params()
    params['ssid'] = "test-hs20-other"
    params['roaming_consortium'] = ["223344"]
    hostapd.add_ap(apdev[1], params)

    values = default_cred()
    values['required_roaming_consortium'] = "223344"
    policy_test(dev[0], apdev[1], values)
    values['required_roaming_consortium'] = "112233"
    policy_test(dev[0], apdev[0], values)

    id = dev[0].add_cred()
    dev[0].set_cred(id, "required_roaming_consortium", "112233")
    dev[0].set_cred(id, "required_roaming_consortium", "112233445566778899aabbccddeeff")

    for val in ["", "1", "11", "1122", "1122334",
                "112233445566778899aabbccddeeff00"]:
        if "FAIL" not in dev[0].request('SET_CRED {} required_roaming_consortium {}'.format(id, val)):
            raise Exception("Invalid roaming consortium value accepted: " + val)

def test_ap_hs20_req_roaming_consortium_no_match(dev, apdev):
    """Hotspot 2.0 required roaming consortium and no match"""
    check_eap_capa(dev[0], "MSCHAPV2")
    params = hs20_ap_params()
    del params['roaming_consortium']
    hostapd.add_ap(apdev[0], params)

    params = hs20_ap_params()
    params['ssid'] = "test-hs20-other"
    params['roaming_consortium'] = ["223345"]
    hostapd.add_ap(apdev[1], params)

    values = default_cred()
    values['required_roaming_consortium'] = "223344"
    dev[0].hs20_enable()
    id = dev[0].add_cred_values(values)
    dev[0].request("INTERWORKING_SELECT auto freq=2412")
    ev = dev[0].wait_event(["INTERWORKING-NO-MATCH"], timeout=10)
    if ev is None:
        raise Exception("INTERWORKING-NO-MATCH not reported")

def test_ap_hs20_excluded_ssid(dev, apdev):
    """Hotspot 2.0 exclusion based on SSID"""
    check_eap_capa(dev[0], "MSCHAPV2")
    params = hs20_ap_params()
    params['roaming_consortium'] = ["223344"]
    params['anqp_3gpp_cell_net'] = "555,444"
    hostapd.add_ap(apdev[0], params)

    params = hs20_ap_params()
    params['ssid'] = "test-hs20-other"
    params['roaming_consortium'] = ["223344"]
    params['anqp_3gpp_cell_net'] = "555,444"
    hostapd.add_ap(apdev[1], params)

    values = default_cred()
    values['excluded_ssid'] = "test-hs20"
    events = policy_test(dev[0], apdev[1], values)
    ev = [e for e in events if "INTERWORKING-BLACKLISTED " + apdev[0]['bssid'] in e]
    if len(ev) != 1:
        raise Exception("Excluded network not reported")
    values['excluded_ssid'] = "test-hs20-other"
    events = policy_test(dev[0], apdev[0], values)
    ev = [e for e in events if "INTERWORKING-BLACKLISTED " + apdev[1]['bssid'] in e]
    if len(ev) != 1:
        raise Exception("Excluded network not reported")

    values = default_cred()
    values['roaming_consortium'] = "223344"
    values['eap'] = "TTLS"
    values['phase2'] = "auth=MSCHAPV2"
    values['excluded_ssid'] = "test-hs20"
    events = policy_test(dev[0], apdev[1], values)
    ev = [e for e in events if "INTERWORKING-BLACKLISTED " + apdev[0]['bssid'] in e]
    if len(ev) != 1:
        raise Exception("Excluded network not reported")

    values = {'imsi': "555444-333222111", 'eap': "SIM",
              'milenage': "5122250214c33e723a5dd523fc145fc0:981d464c7c52eb6e5036234984ad0bcf:000000000123",
              'excluded_ssid': "test-hs20"}
    events = policy_test(dev[0], apdev[1], values)
    ev = [e for e in events if "INTERWORKING-BLACKLISTED " + apdev[0]['bssid'] in e]
    if len(ev) != 1:
        raise Exception("Excluded network not reported")

def test_ap_hs20_roam_to_higher_prio(dev, apdev):
    """Hotspot 2.0 and roaming from current to higher priority network"""
    check_eap_capa(dev[0], "MSCHAPV2")
    bssid = apdev[0]['bssid']
    params = hs20_ap_params(ssid="test-hs20-visited")
    params['domain_name'] = "visited.example.org"
    hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable()
    id = dev[0].add_cred_values({'realm': "example.com",
                                 'ca_cert': "auth_serv/ca.pem",
                                 'username': "hs20-test",
                                 'password': "password",
                                 'domain': "example.com"})
    logger.info("Connect to the only network option")
    interworking_select(dev[0], bssid, "roaming", freq="2412")
    dev[0].dump_monitor()
    interworking_connect(dev[0], bssid, "TTLS")

    logger.info("Start another AP (home operator) and reconnect")
    bssid2 = apdev[1]['bssid']
    params = hs20_ap_params(ssid="test-hs20-home")
    params['domain_name'] = "example.com"
    hostapd.add_ap(apdev[1], params)

    dev[0].scan_for_bss(bssid2, freq="2412", force_scan=True)
    dev[0].request("INTERWORKING_SELECT auto freq=2412")
    ev = dev[0].wait_event(["INTERWORKING-NO-MATCH",
                            "INTERWORKING-ALREADY-CONNECTED",
                            "CTRL-EVENT-CONNECTED"], timeout=15)
    if ev is None:
        raise Exception("Connection timed out")
    if "INTERWORKING-NO-MATCH" in ev:
        raise Exception("Matching AP not found")
    if "INTERWORKING-ALREADY-CONNECTED" in ev:
        raise Exception("Unexpected AP selected")
    if bssid2 not in ev:
        raise Exception("Unexpected BSSID after reconnection")

def test_ap_hs20_domain_suffix_match_full(dev, apdev):
    """Hotspot 2.0 and domain_suffix_match"""
    check_domain_match_full(dev[0])
    check_eap_capa(dev[0], "MSCHAPV2")
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable()
    id = dev[0].add_cred_values({'realm': "example.com",
                                 'username': "hs20-test",
                                 'password': "password",
                                 'ca_cert': "auth_serv/ca.pem",
                                 'domain': "example.com",
                                 'domain_suffix_match': "server.w1.fi"})
    interworking_select(dev[0], bssid, "home", freq="2412")
    dev[0].dump_monitor()
    interworking_connect(dev[0], bssid, "TTLS")
    dev[0].request("REMOVE_NETWORK all")
    dev[0].dump_monitor()

    dev[0].set_cred_quoted(id, "domain_suffix_match", "no-match.example.com")
    interworking_select(dev[0], bssid, "home", freq="2412")
    dev[0].dump_monitor()
    dev[0].request("INTERWORKING_CONNECT " + bssid)
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-TLS-CERT-ERROR"])
    if ev is None:
        raise Exception("TLS certificate error not reported")
    if "Domain suffix mismatch" not in ev:
        raise Exception("Domain suffix mismatch not reported")

def test_ap_hs20_domain_suffix_match(dev, apdev):
    """Hotspot 2.0 and domain_suffix_match"""
    check_eap_capa(dev[0], "MSCHAPV2")
    check_domain_match_full(dev[0])
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable()
    id = dev[0].add_cred_values({'realm': "example.com",
                                 'username': "hs20-test",
                                 'password': "password",
                                 'ca_cert': "auth_serv/ca.pem",
                                 'domain': "example.com",
                                 'domain_suffix_match': "w1.fi"})
    interworking_select(dev[0], bssid, "home", freq="2412")
    dev[0].dump_monitor()
    interworking_connect(dev[0], bssid, "TTLS")

def test_ap_hs20_roaming_partner_preference(dev, apdev):
    """Hotspot 2.0 and roaming partner preference"""
    check_eap_capa(dev[0], "MSCHAPV2")
    params = hs20_ap_params()
    params['domain_name'] = "roaming.example.org"
    hostapd.add_ap(apdev[0], params)

    params = hs20_ap_params()
    params['ssid'] = "test-hs20-other"
    params['domain_name'] = "roaming.example.net"
    hostapd.add_ap(apdev[1], params)

    logger.info("Verify default vs. specified preference")
    values = default_cred()
    values['roaming_partner'] = "roaming.example.net,1,127,*"
    policy_test(dev[0], apdev[1], values, only_one=False)
    values['roaming_partner'] = "roaming.example.net,1,129,*"
    policy_test(dev[0], apdev[0], values, only_one=False)

    logger.info("Verify partial FQDN match")
    values['roaming_partner'] = "example.net,0,0,*"
    policy_test(dev[0], apdev[1], values, only_one=False)
    values['roaming_partner'] = "example.net,0,255,*"
    policy_test(dev[0], apdev[0], values, only_one=False)

def test_ap_hs20_max_bss_load(dev, apdev):
    """Hotspot 2.0 and maximum BSS load"""
    check_eap_capa(dev[0], "MSCHAPV2")
    params = hs20_ap_params()
    params['bss_load_test'] = "12:200:20000"
    hostapd.add_ap(apdev[0], params)

    params = hs20_ap_params()
    params['ssid'] = "test-hs20-other"
    params['bss_load_test'] = "5:20:10000"
    hostapd.add_ap(apdev[1], params)

    logger.info("Verify maximum BSS load constraint")
    values = default_cred()
    values['domain'] = "example.com"
    values['max_bss_load'] = "100"
    events = policy_test(dev[0], apdev[1], values, only_one=False)

    ev = [e for e in events if "INTERWORKING-AP " + apdev[0]['bssid'] in e]
    if len(ev) != 1 or "over_max_bss_load=1" not in ev[0]:
        raise Exception("Maximum BSS Load case not noticed")
    ev = [e for e in events if "INTERWORKING-AP " + apdev[1]['bssid'] in e]
    if len(ev) != 1 or "over_max_bss_load=1" in ev[0]:
        raise Exception("Maximum BSS Load case reported incorrectly")

    logger.info("Verify maximum BSS load does not prevent connection")
    values['max_bss_load'] = "1"
    events = policy_test(dev[0], None, values)

    ev = [e for e in events if "INTERWORKING-AP " + apdev[0]['bssid'] in e]
    if len(ev) != 1 or "over_max_bss_load=1" not in ev[0]:
        raise Exception("Maximum BSS Load case not noticed")
    ev = [e for e in events if "INTERWORKING-AP " + apdev[1]['bssid'] in e]
    if len(ev) != 1 or "over_max_bss_load=1" not in ev[0]:
        raise Exception("Maximum BSS Load case not noticed")

def test_ap_hs20_max_bss_load2(dev, apdev):
    """Hotspot 2.0 and maximum BSS load with one AP not advertising"""
    check_eap_capa(dev[0], "MSCHAPV2")
    params = hs20_ap_params()
    params['bss_load_test'] = "12:200:20000"
    hostapd.add_ap(apdev[0], params)

    params = hs20_ap_params()
    params['ssid'] = "test-hs20-other"
    hostapd.add_ap(apdev[1], params)

    logger.info("Verify maximum BSS load constraint with AP advertisement")
    values = default_cred()
    values['domain'] = "example.com"
    values['max_bss_load'] = "100"
    events = policy_test(dev[0], apdev[1], values, only_one=False)

    ev = [e for e in events if "INTERWORKING-AP " + apdev[0]['bssid'] in e]
    if len(ev) != 1 or "over_max_bss_load=1" not in ev[0]:
        raise Exception("Maximum BSS Load case not noticed")
    ev = [e for e in events if "INTERWORKING-AP " + apdev[1]['bssid'] in e]
    if len(ev) != 1 or "over_max_bss_load=1" in ev[0]:
        raise Exception("Maximum BSS Load case reported incorrectly")

def test_ap_hs20_max_bss_load_roaming(dev, apdev):
    """Hotspot 2.0 and maximum BSS load (roaming)"""
    check_eap_capa(dev[0], "MSCHAPV2")
    params = hs20_ap_params()
    params['bss_load_test'] = "12:200:20000"
    hostapd.add_ap(apdev[0], params)

    values = default_cred()
    values['domain'] = "roaming.example.com"
    values['max_bss_load'] = "100"
    events = policy_test(dev[0], apdev[0], values, only_one=True)
    ev = [e for e in events if "INTERWORKING-AP " + apdev[0]['bssid'] in e]
    if len(ev) != 1:
        raise Exception("No INTERWORKING-AP event")
    if "over_max_bss_load=1" in ev[0]:
        raise Exception("Maximum BSS Load reported for roaming")

def test_ap_hs20_multi_cred_sp_prio(dev, apdev):
    """Hotspot 2.0 multi-cred sp_priority"""
    check_eap_capa(dev[0], "MSCHAPV2")
    try:
        _test_ap_hs20_multi_cred_sp_prio(dev, apdev)
    finally:
        dev[0].request("SET external_sim 0")

def _test_ap_hs20_multi_cred_sp_prio(dev, apdev):
    hlr_auc_gw_available()
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid
    del params['domain_name']
    params['anqp_3gpp_cell_net'] = "232,01"
    hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable()
    dev[0].scan_for_bss(bssid, freq="2412")
    dev[0].request("SET external_sim 1")
    id1 = dev[0].add_cred_values({'imsi': "23201-0000000000", 'eap': "SIM",
                                  'provisioning_sp': "example.com",
                                  'sp_priority' :"1"})
    id2 = dev[0].add_cred_values({'realm': "example.com",
                                  'ca_cert': "auth_serv/ca.pem",
                                  'username': "hs20-test",
                                  'password': "password",
                                  'domain': "example.com",
                                  'provisioning_sp': "example.com",
                                  'sp_priority': "2"})
    dev[0].dump_monitor()
    dev[0].scan_for_bss(bssid, freq="2412")
    dev[0].request("INTERWORKING_SELECT auto freq=2412")
    interworking_ext_sim_auth(dev[0], "SIM")
    check_sp_type(dev[0], "unknown")
    dev[0].request("REMOVE_NETWORK all")

    dev[0].set_cred(id1, "sp_priority", "2")
    dev[0].set_cred(id2, "sp_priority", "1")
    dev[0].dump_monitor()
    dev[0].request("INTERWORKING_SELECT auto freq=2412")
    interworking_auth(dev[0], "TTLS")
    check_sp_type(dev[0], "unknown")

def test_ap_hs20_multi_cred_sp_prio2(dev, apdev):
    """Hotspot 2.0 multi-cred sp_priority with two BSSes"""
    check_eap_capa(dev[0], "MSCHAPV2")
    try:
        _test_ap_hs20_multi_cred_sp_prio2(dev, apdev)
    finally:
        dev[0].request("SET external_sim 0")

def _test_ap_hs20_multi_cred_sp_prio2(dev, apdev):
    hlr_auc_gw_available()
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid
    del params['nai_realm']
    del params['domain_name']
    params['anqp_3gpp_cell_net'] = "232,01"
    hostapd.add_ap(apdev[0], params)

    bssid2 = apdev[1]['bssid']
    params = hs20_ap_params()
    params['ssid'] = "test-hs20-other"
    params['hessid'] = bssid2
    del params['domain_name']
    del params['anqp_3gpp_cell_net']
    hostapd.add_ap(apdev[1], params)

    dev[0].hs20_enable()
    dev[0].request("SET external_sim 1")
    id1 = dev[0].add_cred_values({'imsi': "23201-0000000000", 'eap': "SIM",
                                  'provisioning_sp': "example.com",
                                  'sp_priority': "1"})
    id2 = dev[0].add_cred_values({'realm': "example.com",
                                  'ca_cert': "auth_serv/ca.pem",
                                  'username': "hs20-test",
                                  'password': "password",
                                  'domain': "example.com",
                                  'provisioning_sp': "example.com",
                                  'sp_priority': "2"})
    dev[0].dump_monitor()
    dev[0].scan_for_bss(bssid, freq="2412")
    dev[0].scan_for_bss(bssid2, freq="2412")
    dev[0].request("INTERWORKING_SELECT auto freq=2412")
    interworking_ext_sim_auth(dev[0], "SIM")
    check_sp_type(dev[0], "unknown")
    conn_bssid = dev[0].get_status_field("bssid")
    if conn_bssid != bssid:
        raise Exception("Connected to incorrect BSS")
    dev[0].request("REMOVE_NETWORK all")

    dev[0].set_cred(id1, "sp_priority", "2")
    dev[0].set_cred(id2, "sp_priority", "1")
    dev[0].dump_monitor()
    dev[0].request("INTERWORKING_SELECT auto freq=2412")
    interworking_auth(dev[0], "TTLS")
    check_sp_type(dev[0], "unknown")
    conn_bssid = dev[0].get_status_field("bssid")
    if conn_bssid != bssid2:
        raise Exception("Connected to incorrect BSS")

def test_ap_hs20_multi_cred_sp_prio_same(dev, apdev):
    """Hotspot 2.0 multi-cred and same sp_priority"""
    check_eap_capa(dev[0], "MSCHAPV2")
    hlr_auc_gw_available()
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid
    del params['domain_name']
    params['anqp_3gpp_cell_net'] = "232,01"
    hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable()
    dev[0].scan_for_bss(bssid, freq="2412")
    id1 = dev[0].add_cred_values({'realm': "example.com",
                                  'ca_cert': "auth_serv/ca.pem",
                                  'username': "hs20-test",
                                  'password': "password",
                                  'domain': "domain1.example.com",
                                  'provisioning_sp': "example.com",
                                  'sp_priority': "1"})
    id2 = dev[0].add_cred_values({'realm': "example.com",
                                  'ca_cert': "auth_serv/ca.pem",
                                  'username': "hs20-test",
                                  'password': "password",
                                  'domain': "domain2.example.com",
                                  'provisioning_sp': "example.com",
                                  'sp_priority': "1"})
    dev[0].dump_monitor()
    dev[0].scan_for_bss(bssid, freq="2412")
    check_auto_select(dev[0], bssid)

def check_conn_capab_selection(dev, type, missing):
    dev.request("INTERWORKING_SELECT freq=2412")
    ev = dev.wait_event(["INTERWORKING-AP"])
    if ev is None:
        raise Exception("Network selection timed out")
    if "type=" + type not in ev:
        raise Exception("Unexpected network type")
    if missing and "conn_capab_missing=1" not in ev:
        raise Exception("conn_capab_missing not reported")
    if not missing and "conn_capab_missing=1" in ev:
        raise Exception("conn_capab_missing reported unexpectedly")

def conn_capab_cred(domain=None, req_conn_capab=None):
    cred = default_cred(domain=domain)
    if req_conn_capab:
        cred['req_conn_capab'] = req_conn_capab
    return cred

def test_ap_hs20_req_conn_capab(dev, apdev):
    """Hotspot 2.0 network selection with req_conn_capab"""
    check_eap_capa(dev[0], "MSCHAPV2")
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable()
    dev[0].scan_for_bss(bssid, freq="2412")
    logger.info("Not used in home network")
    values = conn_capab_cred(domain="example.com", req_conn_capab="6:1234")
    id = dev[0].add_cred_values(values)
    check_conn_capab_selection(dev[0], "home", False)

    logger.info("Used in roaming network")
    dev[0].remove_cred(id)
    values = conn_capab_cred(domain="example.org", req_conn_capab="6:1234")
    id = dev[0].add_cred_values(values)
    check_conn_capab_selection(dev[0], "roaming", True)

    logger.info("Verify that req_conn_capab does not prevent connection if no other network is available")
    check_auto_select(dev[0], bssid)

    logger.info("Additional req_conn_capab checks")

    dev[0].remove_cred(id)
    values = conn_capab_cred(domain="example.org", req_conn_capab="1:0")
    id = dev[0].add_cred_values(values)
    check_conn_capab_selection(dev[0], "roaming", True)

    dev[0].remove_cred(id)
    values = conn_capab_cred(domain="example.org", req_conn_capab="17:5060")
    id = dev[0].add_cred_values(values)
    check_conn_capab_selection(dev[0], "roaming", True)

    bssid2 = apdev[1]['bssid']
    params = hs20_ap_params(ssid="test-hs20b")
    params['hs20_conn_capab'] = ["1:0:2", "6:22:1", "17:5060:0", "50:0:1"]
    hostapd.add_ap(apdev[1], params)

    dev[0].remove_cred(id)
    values = conn_capab_cred(domain="example.org", req_conn_capab="50")
    id = dev[0].add_cred_values(values)
    dev[0].set_cred(id, "req_conn_capab", "6:22")
    dev[0].scan_for_bss(bssid2, freq="2412")
    dev[0].request("INTERWORKING_SELECT freq=2412")
    for i in range(0, 2):
        ev = dev[0].wait_event(["INTERWORKING-AP"])
        if ev is None:
            raise Exception("Network selection timed out")
        if bssid in ev and "conn_capab_missing=1" not in ev:
            raise Exception("Missing protocol connection capability not reported")
        if bssid2 in ev and "conn_capab_missing=1" in ev:
            raise Exception("Protocol connection capability not reported correctly")

def test_ap_hs20_req_conn_capab2(dev, apdev):
    """Hotspot 2.0 network selection with req_conn_capab (not present)"""
    check_eap_capa(dev[0], "MSCHAPV2")
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    del params['hs20_conn_capab']
    hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable()
    dev[0].scan_for_bss(bssid, freq="2412")
    values = conn_capab_cred(domain="example.org", req_conn_capab="6:1234")
    id = dev[0].add_cred_values(values)
    check_conn_capab_selection(dev[0], "roaming", False)

def test_ap_hs20_req_conn_capab_and_roaming_partner_preference(dev, apdev):
    """Hotspot 2.0 and req_conn_capab with roaming partner preference"""
    check_eap_capa(dev[0], "MSCHAPV2")
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['domain_name'] = "roaming.example.org"
    params['hs20_conn_capab'] = ["1:0:2", "6:22:1", "17:5060:0", "50:0:1"]
    hostapd.add_ap(apdev[0], params)

    bssid2 = apdev[1]['bssid']
    params = hs20_ap_params(ssid="test-hs20-b")
    params['domain_name'] = "roaming.example.net"
    hostapd.add_ap(apdev[1], params)

    values = default_cred()
    values['roaming_partner'] = "roaming.example.net,1,127,*"
    id = dev[0].add_cred_values(values)
    check_auto_select(dev[0], bssid2)

    dev[0].set_cred(id, "req_conn_capab", "50")
    check_auto_select(dev[0], bssid)

    dev[0].remove_cred(id)
    id = dev[0].add_cred_values(values)
    dev[0].set_cred(id, "req_conn_capab", "51")
    check_auto_select(dev[0], bssid2)

def check_bandwidth_selection(dev, type, below):
    dev.request("INTERWORKING_SELECT freq=2412")
    ev = dev.wait_event(["INTERWORKING-AP"])
    if ev is None:
        raise Exception("Network selection timed out")
    logger.debug("BSS entries:\n" + dev.request("BSS RANGE=ALL"))
    if "type=" + type not in ev:
        raise Exception("Unexpected network type")
    if below and "below_min_backhaul=1" not in ev:
        raise Exception("below_min_backhaul not reported")
    if not below and "below_min_backhaul=1" in ev:
        raise Exception("below_min_backhaul reported unexpectedly")

def bw_cred(domain=None, dl_home=None, ul_home=None, dl_roaming=None, ul_roaming=None):
    cred = default_cred(domain=domain)
    if dl_home:
        cred['min_dl_bandwidth_home'] = str(dl_home)
    if ul_home:
        cred['min_ul_bandwidth_home'] = str(ul_home)
    if dl_roaming:
        cred['min_dl_bandwidth_roaming'] = str(dl_roaming)
    if ul_roaming:
        cred['min_ul_bandwidth_roaming'] = str(ul_roaming)
    return cred

def test_ap_hs20_min_bandwidth_home(dev, apdev):
    """Hotspot 2.0 network selection with min bandwidth (home)"""
    check_eap_capa(dev[0], "MSCHAPV2")
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable()
    dev[0].scan_for_bss(bssid, freq="2412")
    values = bw_cred(domain="example.com", dl_home=5490, ul_home=58)
    id = dev[0].add_cred_values(values)
    check_bandwidth_selection(dev[0], "home", False)
    dev[0].remove_cred(id)

    values = bw_cred(domain="example.com", dl_home=5491, ul_home=58)
    id = dev[0].add_cred_values(values)
    check_bandwidth_selection(dev[0], "home", True)
    dev[0].remove_cred(id)

    values = bw_cred(domain="example.com", dl_home=5490, ul_home=59)
    id = dev[0].add_cred_values(values)
    check_bandwidth_selection(dev[0], "home", True)
    dev[0].remove_cred(id)

    values = bw_cred(domain="example.com", dl_home=5491, ul_home=59)
    id = dev[0].add_cred_values(values)
    check_bandwidth_selection(dev[0], "home", True)
    check_auto_select(dev[0], bssid)

    bssid2 = apdev[1]['bssid']
    params = hs20_ap_params(ssid="test-hs20-b")
    params['hs20_wan_metrics'] = "01:8000:1000:1:1:3000"
    hostapd.add_ap(apdev[1], params)

    check_auto_select(dev[0], bssid2)

def test_ap_hs20_min_bandwidth_home2(dev, apdev):
    """Hotspot 2.0 network selection with min bandwidth - special cases"""
    check_eap_capa(dev[0], "MSCHAPV2")
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable()
    dev[0].scan_for_bss(bssid, freq="2412")
    values = bw_cred(domain="example.com", dl_home=5490, ul_home=58)
    id = dev[0].add_cred_values(values)
    check_bandwidth_selection(dev[0], "home", False)

    logger.info("WAN link at capacity")
    hapd.set('hs20_wan_metrics', "09:8000:1000:80:240:3000")
    check_bandwidth_selection(dev[0], "home", True)

    logger.info("Downlink/Uplink Load was not measured")
    hapd.set('hs20_wan_metrics', "01:8000:1000:80:240:0")
    check_bandwidth_selection(dev[0], "home", False)

    logger.info("Uplink and Downlink max values")
    hapd.set('hs20_wan_metrics', "01:4294967295:4294967295:80:240:3000")
    check_bandwidth_selection(dev[0], "home", False)

    dev[0].remove_cred(id)

def test_ap_hs20_min_bandwidth_home_hidden_ssid_in_scan_res(dev, apdev):
    """Hotspot 2.0 network selection with min bandwidth (home) while hidden SSID is included in scan results"""
    check_eap_capa(dev[0], "MSCHAPV2")
    bssid = apdev[0]['bssid']

    hapd = hostapd.add_ap(apdev[0], {"ssid": 'secret',
                                     "ignore_broadcast_ssid": "1"})
    dev[0].scan_for_bss(bssid, freq=2412)
    hapd.disable()
    hapd_global = hostapd.HostapdGlobal(apdev[0])
    hapd_global.flush()
    hapd_global.remove(apdev[0]['ifname'])

    params = hs20_ap_params()
    hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable()
    dev[0].scan_for_bss(bssid, freq="2412")
    values = bw_cred(domain="example.com", dl_home=5490, ul_home=58)
    id = dev[0].add_cred_values(values)
    check_bandwidth_selection(dev[0], "home", False)
    dev[0].remove_cred(id)

    values = bw_cred(domain="example.com", dl_home=5491, ul_home=58)
    id = dev[0].add_cred_values(values)
    check_bandwidth_selection(dev[0], "home", True)
    dev[0].remove_cred(id)

    values = bw_cred(domain="example.com", dl_home=5490, ul_home=59)
    id = dev[0].add_cred_values(values)
    check_bandwidth_selection(dev[0], "home", True)
    dev[0].remove_cred(id)

    values = bw_cred(domain="example.com", dl_home=5491, ul_home=59)
    id = dev[0].add_cred_values(values)
    check_bandwidth_selection(dev[0], "home", True)
    check_auto_select(dev[0], bssid)

    bssid2 = apdev[1]['bssid']
    params = hs20_ap_params(ssid="test-hs20-b")
    params['hs20_wan_metrics'] = "01:8000:1000:1:1:3000"
    hostapd.add_ap(apdev[1], params)

    check_auto_select(dev[0], bssid2)

    dev[0].flush_scan_cache()

def test_ap_hs20_min_bandwidth_roaming(dev, apdev):
    """Hotspot 2.0 network selection with min bandwidth (roaming)"""
    check_eap_capa(dev[0], "MSCHAPV2")
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable()
    dev[0].scan_for_bss(bssid, freq="2412")
    values = bw_cred(domain="example.org", dl_roaming=5490, ul_roaming=58)
    id = dev[0].add_cred_values(values)
    check_bandwidth_selection(dev[0], "roaming", False)
    dev[0].remove_cred(id)

    values = bw_cred(domain="example.org", dl_roaming=5491, ul_roaming=58)
    id = dev[0].add_cred_values(values)
    check_bandwidth_selection(dev[0], "roaming", True)
    dev[0].remove_cred(id)

    values = bw_cred(domain="example.org", dl_roaming=5490, ul_roaming=59)
    id = dev[0].add_cred_values(values)
    check_bandwidth_selection(dev[0], "roaming", True)
    dev[0].remove_cred(id)

    values = bw_cred(domain="example.org", dl_roaming=5491, ul_roaming=59)
    id = dev[0].add_cred_values(values)
    check_bandwidth_selection(dev[0], "roaming", True)
    check_auto_select(dev[0], bssid)

    bssid2 = apdev[1]['bssid']
    params = hs20_ap_params(ssid="test-hs20-b")
    params['hs20_wan_metrics'] = "01:8000:1000:1:1:3000"
    hostapd.add_ap(apdev[1], params)

    check_auto_select(dev[0], bssid2)

def test_ap_hs20_min_bandwidth_and_roaming_partner_preference(dev, apdev):
    """Hotspot 2.0 and minimum bandwidth with roaming partner preference"""
    check_eap_capa(dev[0], "MSCHAPV2")
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['domain_name'] = "roaming.example.org"
    params['hs20_wan_metrics'] = "01:8000:1000:1:1:3000"
    hostapd.add_ap(apdev[0], params)

    bssid2 = apdev[1]['bssid']
    params = hs20_ap_params(ssid="test-hs20-b")
    params['domain_name'] = "roaming.example.net"
    hostapd.add_ap(apdev[1], params)

    values = default_cred()
    values['roaming_partner'] = "roaming.example.net,1,127,*"
    id = dev[0].add_cred_values(values)
    check_auto_select(dev[0], bssid2)

    dev[0].set_cred(id, "min_dl_bandwidth_roaming", "6000")
    check_auto_select(dev[0], bssid)

    dev[0].set_cred(id, "min_dl_bandwidth_roaming", "10000")
    check_auto_select(dev[0], bssid2)

def test_ap_hs20_min_bandwidth_no_wan_metrics(dev, apdev):
    """Hotspot 2.0 network selection with min bandwidth but no WAN Metrics"""
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    del params['hs20_wan_metrics']
    hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable()
    dev[0].scan_for_bss(bssid, freq="2412")
    values = bw_cred(domain="example.com", dl_home=10000, ul_home=10000,
                     dl_roaming=10000, ul_roaming=10000)
    dev[0].add_cred_values(values)
    check_bandwidth_selection(dev[0], "home", False)

def test_ap_hs20_deauth_req_ess(dev, apdev):
    """Hotspot 2.0 connection and deauthentication request for ESS"""
    check_eap_capa(dev[0], "MSCHAPV2")
    try:
        _test_ap_hs20_deauth_req_ess(dev, apdev)
    finally:
        dev[0].request("SET pmf 0")

def _test_ap_hs20_deauth_req_ess(dev, apdev):
    dev[0].request("SET pmf 2")
    hapd = eap_test(dev[0], apdev[0], "21[3:26]", "TTLS", "user")
    dev[0].dump_monitor()
    addr = dev[0].p2p_interface_addr()
    hapd.wait_sta()
    hapd.request("HS20_DEAUTH_REQ " + addr + " 1 120 http://example.com/")
    ev = dev[0].wait_event(["HS20-DEAUTH-IMMINENT-NOTICE"])
    if ev is None:
        raise Exception("Timeout on deauth imminent notice")
    if "1 120 http://example.com/" not in ev:
        raise Exception("Unexpected deauth imminent notice: " + ev)
    hapd.request("DEAUTHENTICATE " + addr)
    dev[0].wait_disconnected(timeout=10)
    if "[TEMP-DISABLED]" not in dev[0].list_networks()[0]['flags']:
        raise Exception("Network not marked temporarily disabled")
    ev = dev[0].wait_event(["SME: Trying to authenticate",
                            "Trying to associate",
                            "CTRL-EVENT-CONNECTED"], timeout=5)
    if ev is not None:
        raise Exception("Unexpected connection attempt")

def test_ap_hs20_deauth_req_bss(dev, apdev):
    """Hotspot 2.0 connection and deauthentication request for BSS"""
    check_eap_capa(dev[0], "MSCHAPV2")
    try:
        _test_ap_hs20_deauth_req_bss(dev, apdev)
    finally:
        dev[0].request("SET pmf 0")

def _test_ap_hs20_deauth_req_bss(dev, apdev):
    dev[0].request("SET pmf 2")
    hapd = eap_test(dev[0], apdev[0], "21[3:26]", "TTLS", "user")
    dev[0].dump_monitor()
    addr = dev[0].p2p_interface_addr()
    hapd.wait_sta()
    hapd.request("HS20_DEAUTH_REQ " + addr + " 0 120 http://example.com/")
    ev = dev[0].wait_event(["HS20-DEAUTH-IMMINENT-NOTICE"])
    if ev is None:
        raise Exception("Timeout on deauth imminent notice")
    if "0 120 http://example.com/" not in ev:
        raise Exception("Unexpected deauth imminent notice: " + ev)
    hapd.request("DEAUTHENTICATE " + addr + " reason=4")
    ev = dev[0].wait_disconnected(timeout=10)
    if "reason=4" not in ev:
        raise Exception("Unexpected disconnection reason")
    if "[TEMP-DISABLED]" not in dev[0].list_networks()[0]['flags']:
        raise Exception("Network not marked temporarily disabled")
    ev = dev[0].wait_event(["SME: Trying to authenticate",
                            "Trying to associate",
                            "CTRL-EVENT-CONNECTED"], timeout=5)
    if ev is not None:
        raise Exception("Unexpected connection attempt")

def test_ap_hs20_deauth_req_from_radius(dev, apdev):
    """Hotspot 2.0 connection and deauthentication request from RADIUS"""
    check_eap_capa(dev[0], "MSCHAPV2")
    try:
        _test_ap_hs20_deauth_req_from_radius(dev, apdev)
    finally:
        dev[0].request("SET pmf 0")

def _test_ap_hs20_deauth_req_from_radius(dev, apdev):
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['nai_realm'] = ["0,example.com,21[2:4]"]
    params['hs20_deauth_req_timeout'] = "2"
    hostapd.add_ap(apdev[0], params)

    dev[0].request("SET pmf 2")
    dev[0].hs20_enable()
    dev[0].add_cred_values({'realm': "example.com",
                            'username': "hs20-deauth-test",
                            'password': "password"})
    interworking_select(dev[0], bssid, freq="2412")
    interworking_connect(dev[0], bssid, "TTLS")
    ev = dev[0].wait_event(["HS20-DEAUTH-IMMINENT-NOTICE"], timeout=5)
    if ev is None:
        raise Exception("Timeout on deauth imminent notice")
    if " 1 100" not in ev:
        raise Exception("Unexpected deauth imminent contents")
    dev[0].wait_disconnected(timeout=3)

def test_ap_hs20_deauth_req_without_pmf(dev, apdev):
    """Hotspot 2.0 connection and deauthentication request without PMF"""
    check_eap_capa(dev[0], "MSCHAPV2")
    dev[0].request("SET pmf 0")
    hapd = eap_test(dev[0], apdev[0], "21[3:26]", "TTLS", "user", release=1)
    dev[0].dump_monitor()
    id = int(dev[0].get_status_field("id"))
    dev[0].set_network(id, "ieee80211w", "0")
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()
    dev[0].select_network(id, freq=2412)
    dev[0].wait_connected()
    addr = dev[0].own_addr()
    hapd.wait_sta()
    hapd.request("HS20_DEAUTH_REQ " + addr + " 1 120 http://example.com/")
    ev = dev[0].wait_event(["HS20-DEAUTH-IMMINENT-NOTICE"], timeout=0.2)
    if ev is not None:
        raise Exception("Deauth imminent notice without PMF accepted")
    with alloc_fail(hapd, 1, "wpabuf_alloc;hostapd_ctrl_iface_hs20_deauth_req"):
        if "FAIL" not in hapd.request("HS20_DEAUTH_REQ " + addr + " 1 120 http://example.com/"):
            raise Exception("HS20_DEAUTH_REQ accepted during OOM")

def test_ap_hs20_deauth_req_pmf_htc(dev, apdev):
    """Hotspot 2.0 connection and deauthentication request PMF misbehavior (+HTC)"""
    try:
        run_ap_hs20_deauth_req_pmf_htc(dev, apdev)
    finally:
        stop_monitor(apdev[1]["ifname"])

def run_ap_hs20_deauth_req_pmf_htc(dev, apdev):
    check_eap_capa(dev[0], "MSCHAPV2")
    dev[0].request("SET pmf 0")
    hapd = eap_test(dev[0], apdev[0], "21[3:26]", "TTLS", "user", release=1)
    dev[0].dump_monitor()
    addr = dev[0].own_addr()
    hapd.wait_sta()

    sock = start_monitor(apdev[1]["ifname"])
    radiotap = radiotap_build()
    bssid = hapd.own_addr().replace(':', '')
    addr = dev[0].own_addr().replace(':', '')
    payload = "0a1a0101dd1b506f9a0101780013687474703a2f2f6578616d706c652e636f6d2f"
    # Claim there is a HT Control field, but then start the frame body from
    # there and do not encrypt the Robust Action frame.
    frame = binascii.unhexlify("d0803a01" + addr + 2 * bssid + "0000" + payload)
    # Claim there is a HT Control field and start the frame body in the correct
    # location, but do not encrypt the Robust Action frame. Make the first octet
    # of HT Control field use a non-robust Action Category value.
    frame2 = binascii.unhexlify("d0803a01" + addr + 2 * bssid + "0000" + "04000000" + payload)

    sock.send(radiotap + frame)
    sock.send(radiotap + frame2)

    ev = dev[0].wait_event(["HS20-DEAUTH-IMMINENT-NOTICE"], timeout=1)
    if ev is not None:
        raise Exception("Deauth imminent notice without PMF accepted")

def test_ap_hs20_remediation_required(dev, apdev):
    """Hotspot 2.0 connection and remediation required from RADIUS"""
    check_eap_capa(dev[0], "MSCHAPV2")
    try:
        _test_ap_hs20_remediation_required(dev, apdev)
    finally:
        dev[0].request("SET pmf 0")

def _test_ap_hs20_remediation_required(dev, apdev):
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['nai_realm'] = ["0,example.com,21[2:4]"]
    hostapd.add_ap(apdev[0], params)

    dev[0].request("SET pmf 1")
    dev[0].hs20_enable()
    dev[0].add_cred_values({'realm': "example.com",
                            'username': "hs20-subrem-test",
                            'password': "password"})
    interworking_select(dev[0], bssid, freq="2412")
    interworking_connect(dev[0], bssid, "TTLS")
    ev = dev[0].wait_event(["HS20-SUBSCRIPTION-REMEDIATION"], timeout=5)
    if ev is None:
        raise Exception("Timeout on subscription remediation notice")
    if " 1 https://example.com/" not in ev:
        raise Exception("Unexpected subscription remediation event contents")

def test_ap_hs20_remediation_required_ctrl(dev, apdev):
    """Hotspot 2.0 connection and subrem from ctrl_iface"""
    check_eap_capa(dev[0], "MSCHAPV2")
    try:
        _test_ap_hs20_remediation_required_ctrl(dev, apdev)
    finally:
        dev[0].request("SET pmf 0")

def _test_ap_hs20_remediation_required_ctrl(dev, apdev):
    bssid = apdev[0]['bssid']
    addr = dev[0].own_addr()
    params = hs20_ap_params()
    params['nai_realm'] = ["0,example.com,21[2:4]"]
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].request("SET pmf 1")
    dev[0].hs20_enable()
    dev[0].add_cred_values(default_cred())
    interworking_select(dev[0], bssid, freq="2412")
    interworking_connect(dev[0], bssid, "TTLS")

    hapd.request("HS20_WNM_NOTIF " + addr + " https://example.com/")
    ev = dev[0].wait_event(["HS20-SUBSCRIPTION-REMEDIATION"], timeout=5)
    if ev is None:
        raise Exception("Timeout on subscription remediation notice")
    if " 1 https://example.com/" not in ev:
        raise Exception("Unexpected subscription remediation event contents")

    hapd.request("HS20_WNM_NOTIF " + addr)
    ev = dev[0].wait_event(["HS20-SUBSCRIPTION-REMEDIATION"], timeout=5)
    if ev is None:
        raise Exception("Timeout on subscription remediation notice")
    if not ev.endswith("HS20-SUBSCRIPTION-REMEDIATION "):
        raise Exception("Unexpected subscription remediation event contents: " + ev)

    if "FAIL" not in hapd.request("HS20_WNM_NOTIF "):
        raise Exception("Unexpected HS20_WNM_NOTIF success")
    if "FAIL" not in hapd.request("HS20_WNM_NOTIF foo"):
        raise Exception("Unexpected HS20_WNM_NOTIF success")
    if "FAIL" not in hapd.request("HS20_WNM_NOTIF " + addr + " https://12345678923456789842345678456783456712345678923456789842345678456783456712345678923456789842345678456783456712345678923456789842345678456783456712345678923456789842345678456783456712345678923456789842345678456783456712345678923456789842345678456783456712345678927.very.long.example.com/"):
        raise Exception("Unexpected HS20_WNM_NOTIF success")
    if "OK" not in hapd.request("HS20_WNM_NOTIF " + addr + " "):
        raise Exception("HS20_WNM_NOTIF failed with empty URL")

def test_ap_hs20_session_info(dev, apdev):
    """Hotspot 2.0 connection and session information from RADIUS"""
    check_eap_capa(dev[0], "MSCHAPV2")
    try:
        _test_ap_hs20_session_info(dev, apdev)
    finally:
        dev[0].request("SET pmf 0")

def _test_ap_hs20_session_info(dev, apdev):
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['nai_realm'] = ["0,example.com,21[2:4]"]
    hostapd.add_ap(apdev[0], params)

    dev[0].request("SET pmf 1")
    dev[0].hs20_enable()
    dev[0].add_cred_values({'realm': "example.com",
                            'username': "hs20-session-info-test",
                            'password': "password"})
    interworking_select(dev[0], bssid, freq="2412")
    interworking_connect(dev[0], bssid, "TTLS")
    ev = dev[0].wait_event(["ESS-DISASSOC-IMMINENT"], timeout=10)
    if ev is None:
        raise Exception("Timeout on ESS disassociation imminent notice")
    if " 1 59904 https://example.com/" not in ev:
        raise Exception("Unexpected ESS disassociation imminent event contents")
    ev = dev[0].wait_event(["CTRL-EVENT-SCAN-STARTED"])
    if ev is None:
        raise Exception("Scan not started")
    ev = dev[0].wait_event(["CTRL-EVENT-SCAN-RESULTS"], timeout=30)
    if ev is None:
        raise Exception("Scan not completed")

def test_ap_hs20_osen(dev, apdev):
    """Hotspot 2.0 OSEN connection"""
    params = {'ssid': "osen",
              'osen': "1",
              'auth_server_addr': "127.0.0.1",
              'auth_server_port': "1812",
              'auth_server_shared_secret': "radius"}
    hostapd.add_ap(apdev[0], params)

    dev[1].connect("osen", key_mgmt="NONE", scan_freq="2412",
                   wait_connect=False)
    if "WEP40" in dev[2].get_capability("group"):
        dev[2].connect("osen", key_mgmt="NONE", wep_key0='"hello"',
                       scan_freq="2412", wait_connect=False)
    dev[0].flush_scan_cache()
    dev[0].connect("osen", proto="OSEN", key_mgmt="OSEN", pairwise="CCMP",
                   group="GTK_NOT_USED CCMP",
                   eap="WFA-UNAUTH-TLS", identity="osen@example.com",
                   ca_cert="auth_serv/ca.pem",
                   scan_freq="2412")
    res = dev[0].get_bss(apdev[0]['bssid'])['flags']
    if "[OSEN-OSEN-CCMP]" not in res:
        raise Exception("OSEN not reported in BSS")
    if "[WEP]" in res:
        raise Exception("WEP reported in BSS")
    res = dev[0].request("SCAN_RESULTS")
    if "[OSEN-OSEN-CCMP]" not in res:
        raise Exception("OSEN not reported in SCAN_RESULTS")

    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    wpas.interface_add("wlan5", drv_params="force_connect_cmd=1")
    wpas.connect("osen", proto="OSEN", key_mgmt="OSEN", pairwise="CCMP",
                 group="GTK_NOT_USED CCMP",
                 eap="WFA-UNAUTH-TLS", identity="osen@example.com",
                 ca_cert="auth_serv/ca.pem",
                 scan_freq="2412")
    wpas.request("DISCONNECT")

def test_ap_hs20_osen_single_ssid(dev, apdev):
    """Hotspot 2.0 OSEN-single-SSID connection"""
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['wpa_key_mgmt'] = "WPA-EAP OSEN"
    params['hessid'] = bssid
    hapd = hostapd.add_ap(apdev[0], params)

    # RSN-OSEN (for OSU)
    dev[0].connect("test-hs20", proto="OSEN", key_mgmt="OSEN", pairwise="CCMP",
                   group="CCMP GTK_NOT_USED",
                   eap="WFA-UNAUTH-TLS", identity="osen@example.com",
                   ca_cert="auth_serv/ca.pem", ieee80211w='2',
                   scan_freq="2412")
    # RSN-EAP (for data connection)
    dev[1].connect("test-hs20", key_mgmt="WPA-EAP", eap="TTLS",
                   identity="hs20-test", password="password",
                   ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2",
                   pairwise="CCMP", group="CCMP",
                   ieee80211w='2', scan_freq="2412")

    res = dev[0].get_bss(apdev[0]['bssid'])['flags']
    if "[WPA2-EAP+OSEN-CCMP]" not in res:
        raise Exception("OSEN not reported in BSS")
    if "[WEP]" in res:
        raise Exception("WEP reported in BSS")
    res = dev[0].request("SCAN_RESULTS")
    if "[WPA2-EAP+OSEN-CCMP]" not in res:
        raise Exception("OSEN not reported in SCAN_RESULTS")

    hwsim_utils.test_connectivity(dev[1], hapd)
    hwsim_utils.test_connectivity(dev[0], hapd, broadcast=False)
    hwsim_utils.test_connectivity(dev[0], hapd, timeout=1,
                                  success_expected=False)

def test_ap_hs20_network_preference(dev, apdev):
    """Hotspot 2.0 network selection with preferred home network"""
    check_eap_capa(dev[0], "MSCHAPV2")
    dev[0].flush_scan_cache()
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable()
    values = {'realm': "example.com",
              'username': "hs20-test",
              'password': "password",
              'domain': "example.com"}
    dev[0].add_cred_values(values)

    id = dev[0].add_network()
    dev[0].set_network_quoted(id, "ssid", "home")
    dev[0].set_network_quoted(id, "psk", "12345678")
    dev[0].set_network(id, "priority", "1")
    dev[0].request("ENABLE_NETWORK %s no-connect" % id)

    dev[0].scan_for_bss(bssid, freq="2412")
    dev[0].request("INTERWORKING_SELECT auto freq=2412")
    ev = dev[0].wait_connected(timeout=15)
    if bssid not in ev:
        raise Exception("Unexpected network selected")

    bssid2 = apdev[1]['bssid']
    params = hostapd.wpa2_params(ssid="home", passphrase="12345678")
    hostapd.add_ap(apdev[1], params)

    dev[0].scan_for_bss(bssid2, freq="2412")
    dev[0].request("INTERWORKING_SELECT auto freq=2412")
    ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED",
                            "INTERWORKING-ALREADY-CONNECTED"], timeout=15)
    if ev is None:
        raise Exception("Connection timed out")
    if "INTERWORKING-ALREADY-CONNECTED" in ev:
        raise Exception("No roam to higher priority network")
    if bssid2 not in ev:
        raise Exception("Unexpected network selected")

def test_ap_hs20_network_preference2(dev, apdev):
    """Hotspot 2.0 network selection with preferred credential"""
    check_eap_capa(dev[0], "MSCHAPV2")
    dev[0].flush_scan_cache()
    bssid2 = apdev[1]['bssid']
    params = hostapd.wpa2_params(ssid="home", passphrase="12345678")
    hostapd.add_ap(apdev[1], params)

    dev[0].hs20_enable()
    values = {'realm': "example.com",
              'username': "hs20-test",
              'password': "password",
              'domain': "example.com",
              'priority': "1"}
    dev[0].add_cred_values(values)

    id = dev[0].add_network()
    dev[0].set_network_quoted(id, "ssid", "home")
    dev[0].set_network_quoted(id, "psk", "12345678")
    dev[0].request("ENABLE_NETWORK %s no-connect" % id)

    dev[0].scan_for_bss(bssid2, freq="2412")
    dev[0].request("INTERWORKING_SELECT auto freq=2412")
    ev = dev[0].wait_connected(timeout=15)
    if bssid2 not in ev:
        raise Exception("Unexpected network selected")

    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    hostapd.add_ap(apdev[0], params)

    dev[0].scan_for_bss(bssid, freq="2412")
    dev[0].request("INTERWORKING_SELECT auto freq=2412")
    ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED",
                            "INTERWORKING-ALREADY-CONNECTED"], timeout=15)
    if ev is None:
        raise Exception("Connection timed out")
    if "INTERWORKING-ALREADY-CONNECTED" in ev:
        raise Exception("No roam to higher priority network")
    if bssid not in ev:
        raise Exception("Unexpected network selected")

def test_ap_hs20_network_preference3(dev, apdev):
    """Hotspot 2.0 network selection with two credential (one preferred)"""
    check_eap_capa(dev[0], "MSCHAPV2")
    dev[0].flush_scan_cache()
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    hostapd.add_ap(apdev[0], params)

    bssid2 = apdev[1]['bssid']
    params = hs20_ap_params(ssid="test-hs20b")
    params['nai_realm'] = "0,example.org,13[5:6],21[2:4][5:7]"
    hostapd.add_ap(apdev[1], params)

    dev[0].hs20_enable()
    values = {'realm': "example.com",
              'username': "hs20-test",
              'password': "password",
              'priority': "1"}
    dev[0].add_cred_values(values)
    values = {'realm': "example.org",
              'username': "hs20-test",
              'password': "password"}
    id = dev[0].add_cred_values(values)

    dev[0].scan_for_bss(bssid, freq="2412")
    dev[0].scan_for_bss(bssid2, freq="2412")
    dev[0].request("INTERWORKING_SELECT auto freq=2412")
    ev = dev[0].wait_connected(timeout=15)
    if bssid not in ev:
        raise Exception("Unexpected network selected")

    dev[0].set_cred(id, "priority", "2")
    dev[0].request("INTERWORKING_SELECT auto freq=2412")
    ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED",
                            "INTERWORKING-ALREADY-CONNECTED"], timeout=15)
    if ev is None:
        raise Exception("Connection timed out")
    if "INTERWORKING-ALREADY-CONNECTED" in ev:
        raise Exception("No roam to higher priority network")
    if bssid2 not in ev:
        raise Exception("Unexpected network selected")

def test_ap_hs20_network_preference4(dev, apdev):
    """Hotspot 2.0 network selection with username vs. SIM credential"""
    check_eap_capa(dev[0], "MSCHAPV2")
    dev[0].flush_scan_cache()
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    hostapd.add_ap(apdev[0], params)

    bssid2 = apdev[1]['bssid']
    params = hs20_ap_params(ssid="test-hs20b")
    params['hessid'] = bssid2
    params['anqp_3gpp_cell_net'] = "555,444"
    params['domain_name'] = "wlan.mnc444.mcc555.3gppnetwork.org"
    hostapd.add_ap(apdev[1], params)

    dev[0].hs20_enable()
    values = {'realm': "example.com",
              'username': "hs20-test",
              'password': "password",
              'priority': "1"}
    dev[0].add_cred_values(values)
    values = {'imsi': "555444-333222111",
              'eap': "SIM",
              'milenage': "5122250214c33e723a5dd523fc145fc0:981d464c7c52eb6e5036234984ad0bcf:000000000123"}
    id = dev[0].add_cred_values(values)

    dev[0].scan_for_bss(bssid, freq="2412")
    dev[0].scan_for_bss(bssid2, freq="2412")
    dev[0].request("INTERWORKING_SELECT auto freq=2412")
    ev = dev[0].wait_connected(timeout=15)
    if bssid not in ev:
        raise Exception("Unexpected network selected")

    dev[0].set_cred(id, "priority", "2")
    dev[0].request("INTERWORKING_SELECT auto freq=2412")
    ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED",
                            "INTERWORKING-ALREADY-CONNECTED"], timeout=15)
    if ev is None:
        raise Exception("Connection timed out")
    if "INTERWORKING-ALREADY-CONNECTED" in ev:
        raise Exception("No roam to higher priority network")
    if bssid2 not in ev:
        raise Exception("Unexpected network selected")

def test_ap_hs20_interworking_select_blocking_scan(dev, apdev):
    """Ongoing INTERWORKING_SELECT blocking SCAN"""
    check_eap_capa(dev[0], "MSCHAPV2")
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable()
    values = {'realm': "example.com",
              'username': "hs20-test",
              'password': "password",
              'domain': "example.com"}
    dev[0].add_cred_values(values)

    dev[0].scan_for_bss(bssid, freq="2412")
    dev[0].request("INTERWORKING_SELECT auto freq=2412")
    if "FAIL-BUSY" not in dev[0].request("SCAN"):
        raise Exception("Unexpected SCAN command result")
    dev[0].wait_connected(timeout=15)

def test_ap_hs20_fetch_osu(dev, apdev):
    """Hotspot 2.0 OSU provider and icon fetch"""
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hs20_icon'] = "128:80:zxx:image/png:w1fi_logo:w1fi_logo.png"
    params['osu_ssid'] = '"HS 2.0 OSU open"'
    params['osu_method_list'] = "1"
    params['osu_friendly_name'] = ["eng:Test OSU", "fin:Testi-OSU"]
    params['osu_icon'] = "w1fi_logo"
    params['osu_service_desc'] = ["eng:Example services", "fin:Esimerkkipalveluja"]
    params['osu_server_uri'] = "https://example.com/osu/"
    hostapd.add_ap(apdev[0], params)

    bssid2 = apdev[1]['bssid']
    params = hs20_ap_params(ssid="test-hs20b")
    params['hessid'] = bssid2
    params['hs20_icon'] = "128:80:zxx:image/png:w1fi_logo:w1fi_logo.png"
    params['osu_ssid'] = '"HS 2.0 OSU OSEN"'
    params['osu_method_list'] = "0"
    params['osu_nai'] = "osen@example.com"
    params['osu_friendly_name'] = ["eng:Test2 OSU", "fin:Testi2-OSU"]
    params['osu_icon'] = "w1fi_logo"
    params['osu_service_desc'] = ["eng:Example services2", "fin:Esimerkkipalveluja2"]
    params['osu_server_uri'] = "https://example.org/osu/"
    hostapd.add_ap(apdev[1], params)

    with open("w1fi_logo.png", "rb") as f:
        orig_logo = f.read()
    dev[0].hs20_enable()
    dir = "/tmp/osu-fetch"
    if os.path.isdir(dir):
       files = [f for f in os.listdir(dir) if f.startswith("osu-")]
       for f in files:
           os.remove(dir + "/" + f)
    else:
        try:
            os.makedirs(dir)
        except:
            pass
    try:
        dev[1].scan_for_bss(bssid, freq="2412")
        dev[2].scan_for_bss(bssid, freq="2412")
        dev[0].request("SET osu_dir " + dir)
        dev[0].request("FETCH_OSU")
        if "FAIL" not in dev[1].request("HS20_ICON_REQUEST foo w1fi_logo"):
            raise Exception("Invalid HS20_ICON_REQUEST accepted")
        if "OK" not in dev[1].request("HS20_ICON_REQUEST " + bssid + " w1fi_logo"):
            raise Exception("HS20_ICON_REQUEST failed")
        if "OK" not in dev[2].request("REQ_HS20_ICON " + bssid + " w1fi_logo"):
            raise Exception("REQ_HS20_ICON failed")
        icons = 0
        while True:
            ev = dev[0].wait_event(["OSU provider fetch completed",
                                    "RX-HS20-ANQP-ICON"], timeout=15)
            if ev is None:
                raise Exception("Timeout on OSU fetch")
            if "OSU provider fetch completed" in ev:
                break
            if "RX-HS20-ANQP-ICON" in ev:
                with open(ev.split(' ')[1], "rb") as f:
                    logo = f.read()
                    if logo == orig_logo:
                        icons += 1

        with open(dir + "/osu-providers.txt", "r") as f:
            prov = f.read()
            logger.debug("osu-providers.txt: " + prov)
        if "OSU-PROVIDER " + bssid not in prov:
            raise Exception("Missing OSU_PROVIDER(1)")
        if "OSU-PROVIDER " + bssid2 not in prov:
            raise Exception("Missing OSU_PROVIDER(2)")
    finally:
        files = [f for f in os.listdir(dir) if f.startswith("osu-")]
        for f in files:
            os.remove(dir + "/" + f)
        os.rmdir(dir)

    if icons != 2:
        raise Exception("Unexpected number of icons fetched")

    ev = dev[1].wait_event(["GAS-QUERY-START"], timeout=5)
    if ev is None:
        raise Exception("Timeout on GAS-QUERY-DONE")
    ev = dev[1].wait_event(["GAS-QUERY-DONE"], timeout=5)
    if ev is None:
        raise Exception("Timeout on GAS-QUERY-DONE")
    if "freq=2412 status_code=0 result=SUCCESS" not in ev:
        raise Exception("Unexpected GAS-QUERY-DONE: " + ev)
    ev = dev[1].wait_event(["RX-HS20-ANQP"], timeout=15)
    if ev is None:
        raise Exception("Timeout on icon fetch")
    if "Icon Binary File" not in ev:
        raise Exception("Unexpected ANQP element")

    ev = dev[2].wait_event(["RX-HS20-ICON"], timeout=5)
    if ev is None:
        raise Exception("Timeout on RX-HS20-ICON")
    event_icon_len = ev.split(' ')[3]
    if " w1fi_logo " not in ev:
        raise Exception("RX-HS20-ICON did not have the expected file name")
    if bssid not in ev:
        raise Exception("RX-HS20-ICON did not have the expected BSSID")
    if "FAIL" in dev[2].request("GET_HS20_ICON " + bssid + " w1fi_logo 0 10"):
        raise Exception("GET_HS20_ICON 0..10 failed")
    if "FAIL" in dev[2].request("GET_HS20_ICON " + bssid + " w1fi_logo 5 10"):
        raise Exception("GET_HS20_ICON 5..15 failed")
    if "FAIL" not in  dev[2].request("GET_HS20_ICON " + bssid + " w1fi_logo 100000 10"):
        raise Exception("Unexpected success of GET_HS20_ICON with too large offset")
    if "FAIL" not in dev[2].request("GET_HS20_ICON " + bssid + " no_such_logo 0 10"):
        raise Exception("GET_HS20_ICON for not existing icon succeeded")
    if "FAIL" not in dev[2].request("GET_HS20_ICON " + bssid + " w1fi_logo 0 3070"):
        raise Exception("GET_HS20_ICON with too many output bytes to fit the buffer succeeded")
    if "FAIL" not in dev[2].request("GET_HS20_ICON " + bssid + " w1fi_logo 0 0"):
        raise Exception("GET_HS20_ICON 0..0 succeeded")
    icon = b''
    pos = 0
    while True:
        if pos > 100000:
            raise Exception("Unexpectedly long icon")
        res = dev[2].request("GET_HS20_ICON " + bssid + " w1fi_logo %d 1000" % pos)
        if res.startswith("FAIL"):
            break
        icon += base64.b64decode(res)
        pos += 1000
    hex = binascii.hexlify(icon).decode()
    if not hex.startswith("0009696d6167652f706e677d1d"):
        raise Exception("Unexpected beacon binary header: " + hex)
    with open('w1fi_logo.png', 'rb') as f:
        data = f.read()
        if icon[13:] != data:
            raise Exception("Unexpected icon data")
    if len(icon) != int(event_icon_len):
        raise Exception("Unexpected RX-HS20-ICON event length: " + event_icon_len)

    for i in range(3):
        if "OK" not in dev[i].request("REQ_HS20_ICON " + bssid + " w1fi_logo"):
            raise Exception("REQ_HS20_ICON failed [2]")
    for i in range(3):
        ev = dev[i].wait_event(["RX-HS20-ICON"], timeout=5)
        if ev is None:
            raise Exception("Timeout on RX-HS20-ICON [2]")

    if "FAIL" not in dev[2].request("DEL_HS20_ICON foo w1fi_logo"):
        raise Exception("Invalid DEL_HS20_ICON accepted")
    if "OK" not in dev[2].request("DEL_HS20_ICON " + bssid + " w1fi_logo"):
        raise Exception("DEL_HS20_ICON failed")
    if "OK" not in dev[1].request("DEL_HS20_ICON " + bssid):
        raise Exception("DEL_HS20_ICON failed")
    if "OK" not in dev[0].request("DEL_HS20_ICON "):
        raise Exception("DEL_HS20_ICON failed")
    for i in range(3):
        if "FAIL" not in dev[i].request("DEL_HS20_ICON "):
            raise Exception("DEL_HS20_ICON accepted when no icons left")

def test_ap_hs20_fetch_osu_no_info(dev, apdev):
    """Hotspot 2.0 OSU provider and no AP with info"""
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable()
    dir = "/tmp/osu-fetch"
    if os.path.isdir(dir):
       files = [f for f in os.listdir(dir) if f.startswith("osu-")]
       for f in files:
           os.remove(dir + "/" + f)
    else:
        try:
            os.makedirs(dir)
        except:
            pass
    dev[0].scan_for_bss(bssid, freq="2412")
    try:
        dev[0].request("SET osu_dir " + dir)
        dev[0].request("FETCH_OSU")
        ev = dev[0].wait_event(["OSU provider fetch completed"], timeout=30)
        if ev is None:
            raise Exception("Timeout on OSU fetch")
    finally:
        files = [f for f in os.listdir(dir) if f.startswith("osu-")]
        for f in files:
            os.remove(dir + "/" + f)
        os.rmdir(dir)

def test_ap_hs20_fetch_osu_no_icon(dev, apdev):
    """Hotspot 2.0 OSU provider and no icon found"""
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hs20_icon'] = "128:80:zxx:image/png:w1fi_logo:w1fi_logo-no-file.png"
    params['osu_ssid'] = '"HS 2.0 OSU open"'
    params['osu_method_list'] = "1"
    params['osu_friendly_name'] = ["eng:Test OSU", "fin:Testi-OSU"]
    params['osu_icon'] = "w1fi_logo"
    params['osu_service_desc'] = ["eng:Example services",
                                  "fin:Esimerkkipalveluja"]
    params['osu_server_uri'] = "https://example.com/osu/"
    hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable()
    dir = "/tmp/osu-fetch"
    if os.path.isdir(dir):
       files = [f for f in os.listdir(dir) if f.startswith("osu-")]
       for f in files:
           os.remove(dir + "/" + f)
    else:
        try:
            os.makedirs(dir)
        except:
            pass
    dev[0].scan_for_bss(bssid, freq="2412")
    try:
        dev[0].request("SET osu_dir " + dir)
        dev[0].request("FETCH_OSU")
        ev = dev[0].wait_event(["OSU provider fetch completed"], timeout=30)
        if ev is None:
            raise Exception("Timeout on OSU fetch")
    finally:
        files = [f for f in os.listdir(dir) if f.startswith("osu-")]
        for f in files:
            os.remove(dir + "/" + f)
        os.rmdir(dir)

def test_ap_hs20_fetch_osu_single_ssid(dev, apdev):
    """Hotspot 2.0 OSU provider and single SSID"""
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hs20_icon'] = "128:80:zxx:image/png:w1fi_logo:w1fi_logo-no-file.png"
    params['osu_ssid'] = '"HS 2.0 OSU open"'
    params['osu_method_list'] = "1"
    params['osu_friendly_name'] = ["eng:Test OSU", "fin:Testi-OSU"]
    params['osu_nai2'] = "osen@example.com"
    params['osu_icon'] = "w1fi_logo"
    params['osu_service_desc'] = ["eng:Example services",
                                  "fin:Esimerkkipalveluja"]
    params['osu_server_uri'] = "https://example.com/osu/"
    params['wpa_key_mgmt'] = "WPA-EAP OSEN"
    hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable()
    dir = "/tmp/osu-fetch"
    if os.path.isdir(dir):
       files = [f for f in os.listdir(dir) if f.startswith("osu-")]
       for f in files:
           os.remove(dir + "/" + f)
    else:
        try:
            os.makedirs(dir)
        except:
            pass
    dev[0].scan_for_bss(bssid, freq="2412")
    try:
        dev[0].request("SET osu_dir " + dir)
        dev[0].request("FETCH_OSU")
        ev = dev[0].wait_event(["OSU provider fetch completed"], timeout=30)
        if ev is None:
            raise Exception("Timeout on OSU fetch")
        osu_ssid = False
        osu_ssid2 = False
        osu_nai = False
        osu_nai2 = False
        with open(os.path.join(dir, "osu-providers.txt"), "r") as f:
            for l in f.readlines():
                logger.info(l.strip())
                if l.strip() == "osu_ssid=HS 2.0 OSU open":
                    osu_ssid = True
                if l.strip() == "osu_ssid2=test-hs20":
                    osu_ssid2 = True
                if l.strip().startswith("osu_nai="):
                    osu_nai = True
                if l.strip() == "osu_nai2=osen@example.com":
                    osu_nai2 = True
        if not osu_ssid:
            raise Exception("osu_ssid not reported")
        if not osu_ssid2:
            raise Exception("osu_ssid2 not reported")
        if osu_nai:
            raise Exception("osu_nai reported unexpectedly")
        if not osu_nai2:
            raise Exception("osu_nai2 not reported")
    finally:
        files = [f for f in os.listdir(dir) if f.startswith("osu-")]
        for f in files:
            os.remove(dir + "/" + f)
        os.rmdir(dir)

def test_ap_hs20_fetch_osu_single_ssid2(dev, apdev):
    """Hotspot 2.0 OSU provider and single SSID (two OSU providers)"""
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hs20_icon'] = "128:80:zxx:image/png:w1fi_logo:w1fi_logo-no-file.png"
    params['osu_ssid'] = '"HS 2.0 OSU open"'
    params['osu_method_list'] = "1"
    params['osu_friendly_name'] = ["eng:Test OSU", "fin:Testi-OSU"]
    params['osu_nai2'] = "osen@example.com"
    params['osu_icon'] = "w1fi_logo"
    params['osu_service_desc'] = ["eng:Example services",
                                  "fin:Esimerkkipalveluja"]
    params['osu_server_uri'] = "https://example.com/osu/"
    params['wpa_key_mgmt'] = "WPA-EAP OSEN"
    hapd = hostapd.add_ap(apdev[0], params, no_enable=True)

    hapd.set('osu_server_uri', 'https://another.example.com/osu/')
    hapd.set('osu_method_list', "1")
    hapd.set('osu_nai2', "osen@another.example.com")
    hapd.enable()

    dev[0].hs20_enable()
    dir = "/tmp/osu-fetch"
    if os.path.isdir(dir):
       files = [f for f in os.listdir(dir) if f.startswith("osu-")]
       for f in files:
           os.remove(dir + "/" + f)
    else:
        try:
            os.makedirs(dir)
        except:
            pass
    dev[0].scan_for_bss(bssid, freq="2412")
    try:
        dev[0].request("SET osu_dir " + dir)
        dev[0].request("FETCH_OSU")
        ev = dev[0].wait_event(["OSU provider fetch completed"], timeout=30)
        if ev is None:
            raise Exception("Timeout on OSU fetch")
        osu_ssid = False
        osu_ssid2 = False
        osu_nai = False
        osu_nai2 = False
        osu_nai2b = False
        with open(os.path.join(dir, "osu-providers.txt"), "r") as f:
            for l in f.readlines():
                logger.info(l.strip())
                if l.strip() == "osu_ssid=HS 2.0 OSU open":
                    osu_ssid = True
                if l.strip() == "osu_ssid2=test-hs20":
                    osu_ssid2 = True
                if l.strip().startswith("osu_nai="):
                    osu_nai = True
                if l.strip() == "osu_nai2=osen@example.com":
                    osu_nai2 = True
                if l.strip() == "osu_nai2=osen@another.example.com":
                    osu_nai2b = True
        if not osu_ssid:
            raise Exception("osu_ssid not reported")
        if not osu_ssid2:
            raise Exception("osu_ssid2 not reported")
        if osu_nai:
            raise Exception("osu_nai reported unexpectedly")
        if not osu_nai2:
            raise Exception("osu_nai2 not reported")
        if not osu_nai2b:
            raise Exception("osu_nai2b not reported")
    finally:
        files = [f for f in os.listdir(dir) if f.startswith("osu-")]
        for f in files:
            os.remove(dir + "/" + f)
        os.rmdir(dir)

def get_icon(dev, bssid, iconname):
    icon = b''
    pos = 0
    while True:
        if pos > 100000:
            raise Exception("Unexpectedly long icon")
        res = dev.request("GET_HS20_ICON " + bssid + " " + iconname + " %d 3000" % pos)
        if res.startswith("FAIL"):
            break
        icon += base64.b64decode(res)
        pos += 3000
    if len(icon) < 13:
        raise Exception("Too short GET_HS20_ICON response")
    return icon[0:13], icon[13:]

def test_ap_hs20_req_hs20_icon(dev, apdev):
    """Hotspot 2.0 OSU provider and multi-icon fetch with REQ_HS20_ICON"""
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hs20_icon'] = ["128:80:zxx:image/png:w1fi_logo:w1fi_logo.png",
                           "128:80:zxx:image/png:test_logo:auth_serv/sha512-server.pem"]
    params['osu_ssid'] = '"HS 2.0 OSU open"'
    params['osu_method_list'] = "1"
    params['osu_friendly_name'] = ["eng:Test OSU", "fin:Testi-OSU"]
    params['osu_icon'] = ["w1fi_logo", "w1fi_logo2"]
    params['osu_service_desc'] = ["eng:Example services",
                                  "fin:Esimerkkipalveluja"]
    params['osu_server_uri'] = "https://example.com/osu/"
    hostapd.add_ap(apdev[0], params)

    dev[0].scan_for_bss(bssid, freq="2412")
    run_req_hs20_icon(dev, bssid)

def run_req_hs20_icon(dev, bssid):
    # First, fetch two icons from the AP to wpa_supplicant

    if "OK" not in dev[0].request("REQ_HS20_ICON " + bssid + " w1fi_logo"):
        raise Exception("REQ_HS20_ICON failed")
    ev = dev[0].wait_event(["RX-HS20-ICON"], timeout=5)
    if ev is None:
        raise Exception("Timeout on RX-HS20-ICON (1)")

    if "OK" not in dev[0].request("REQ_HS20_ICON " + bssid + " test_logo"):
        raise Exception("REQ_HS20_ICON failed")
    ev = dev[0].wait_event(["RX-HS20-ICON"], timeout=5)
    if ev is None:
        raise Exception("Timeout on RX-HS20-ICON (2)")

    # Then, fetch the icons from wpa_supplicant for validation

    hdr, data1 = get_icon(dev[0], bssid, "w1fi_logo")
    hdr, data2 = get_icon(dev[0], bssid, "test_logo")

    with open('w1fi_logo.png', 'rb') as f:
        data = f.read()
        if data1 != data:
            raise Exception("Unexpected icon data (1)")

    with open('auth_serv/sha512-server.pem', 'rb') as f:
        data = f.read()
        if data2 != data:
            raise Exception("Unexpected icon data (2)")

    # Finally, delete the icons from wpa_supplicant

    if "OK" not in dev[0].request("DEL_HS20_ICON " + bssid + " w1fi_logo"):
        raise Exception("DEL_HS20_ICON failed")
    if "OK" not in dev[0].request("DEL_HS20_ICON " + bssid + " test_logo"):
        raise Exception("DEL_HS20_ICON failed")

def test_ap_hs20_req_operator_icon(dev, apdev):
    """Hotspot 2.0 operator icons"""
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hs20_icon'] = ["128:80:zxx:image/png:w1fi_logo:w1fi_logo.png",
                           "500:300:fi:image/png:test_logo:auth_serv/sha512-server.pem"]
    params['operator_icon'] = ["w1fi_logo", "unknown_logo", "test_logo"]
    hostapd.add_ap(apdev[0], params)

    value = struct.pack('<HH', 128, 80) + b"zxx"
    value += struct.pack('B', 9) + b"image/png"
    value += struct.pack('B', 9) + b"w1fi_logo"

    value += struct.pack('<HH', 500, 300) + b"fi\0"
    value += struct.pack('B', 9) + b"image/png"
    value += struct.pack('B', 9) + b"test_logo"

    dev[0].scan_for_bss(bssid, freq="2412")

    if "OK" not in dev[0].request("ANQP_GET " + bssid + " hs20:12"):
        raise Exception("ANQP_GET command failed")

    ev = dev[0].wait_event(["GAS-QUERY-START"], timeout=5)
    if ev is None:
        raise Exception("GAS query start timed out")

    ev = dev[0].wait_event(["GAS-QUERY-DONE"], timeout=10)
    if ev is None:
        raise Exception("GAS query timed out")

    ev = dev[0].wait_event(["RX-HS20-ANQP"], timeout=1)
    if ev is None or "Operator Icon Metadata" not in ev:
        raise Exception("Did not receive Operator Icon Metadata")

    ev = dev[0].wait_event(["ANQP-QUERY-DONE"], timeout=10)
    if ev is None:
        raise Exception("ANQP-QUERY-DONE event not seen")
    if "result=SUCCESS" not in ev:
        raise Exception("Unexpected result: " + ev)

    bss = dev[0].get_bss(bssid)
    if "hs20_operator_icon_metadata" not in bss:
        raise Exception("hs20_operator_icon_metadata missing from BSS entry")
    if bss["hs20_operator_icon_metadata"] != binascii.hexlify(value).decode():
        raise Exception("Unexpected hs20_operator_icon_metadata value: " +
                        bss["hs20_operator_icon_metadata"])

    run_req_hs20_icon(dev, bssid)

def test_ap_hs20_req_hs20_icon_oom(dev, apdev):
    """Hotspot 2.0 icon fetch OOM with REQ_HS20_ICON"""
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hs20_icon'] = ["128:80:zxx:image/png:w1fi_logo:w1fi_logo.png",
                           "128:80:zxx:image/png:test_logo:auth_serv/sha512-server.pem"]
    params['osu_ssid'] = '"HS 2.0 OSU open"'
    params['osu_method_list'] = "1"
    params['osu_friendly_name'] = ["eng:Test OSU", "fin:Testi-OSU"]
    params['osu_icon'] = ["w1fi_logo", "w1fi_logo2"]
    params['osu_service_desc'] = ["eng:Example services",
                                  "fin:Esimerkkipalveluja"]
    params['osu_server_uri'] = "https://example.com/osu/"
    hostapd.add_ap(apdev[0], params)

    dev[0].scan_for_bss(bssid, freq="2412")

    if "FAIL" not in dev[0].request("REQ_HS20_ICON 11:22:33:44:55:66 w1fi_logo"):
        raise Exception("REQ_HS20_ICON succeeded with unknown BSSID")

    with alloc_fail(dev[0], 1, "hs20_build_anqp_req;hs20_anqp_send_req"):
        if "FAIL" not in dev[0].request("REQ_HS20_ICON " + bssid + " w1fi_logo"):
            raise Exception("REQ_HS20_ICON succeeded during OOM")

    with alloc_fail(dev[0], 1, "gas_query_req;hs20_anqp_send_req"):
        if "FAIL" not in dev[0].request("REQ_HS20_ICON " + bssid + " w1fi_logo"):
            raise Exception("REQ_HS20_ICON succeeded during OOM")

    with alloc_fail(dev[0], 1, "=hs20_anqp_send_req"):
        if "FAIL" not in dev[0].request("REQ_HS20_ICON " + bssid + " w1fi_logo"):
            raise Exception("REQ_HS20_ICON succeeded during OOM")
    with alloc_fail(dev[0], 2, "=hs20_anqp_send_req"):
        if "FAIL" not in dev[0].request("REQ_HS20_ICON " + bssid + " w1fi_logo"):
            raise Exception("REQ_HS20_ICON succeeded during OOM")

    if "OK" not in dev[0].request("REQ_HS20_ICON " + bssid + " w1fi_logo"):
        raise Exception("REQ_HS20_ICON failed")
    ev = dev[0].wait_event(["RX-HS20-ICON"], timeout=5)
    if ev is None:
        raise Exception("Timeout on RX-HS20-ICON (1)")

    with alloc_fail(dev[0], 1, "hs20_get_icon"):
        if "FAIL" not in dev[0].request("GET_HS20_ICON " + bssid + "w1fi_logo 0 100"):
            raise Exception("GET_HS20_ICON succeeded during OOM")

    if "OK" not in dev[0].request("DEL_HS20_ICON " + bssid + " w1fi_logo"):
        raise Exception("DEL_HS20_ICON failed")

    with alloc_fail(dev[0], 1, "=hs20_process_icon_binary_file"):
        if "OK" not in dev[0].request("REQ_HS20_ICON " + bssid + " w1fi_logo"):
            raise Exception("REQ_HS20_ICON failed")
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")

def test_ap_hs20_req_hs20_icon_parallel(dev, apdev):
    """Hotspot 2.0 OSU provider and multi-icon parallel fetch with REQ_HS20_ICON"""
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hs20_icon'] = ["128:80:zxx:image/png:w1fi_logo:w1fi_logo.png",
                           "128:80:zxx:image/png:test_logo:auth_serv/sha512-server.pem"]
    params['osu_ssid'] = '"HS 2.0 OSU open"'
    params['osu_method_list'] = "1"
    params['osu_friendly_name'] = ["eng:Test OSU", "fin:Testi-OSU"]
    params['osu_icon'] = ["w1fi_logo", "w1fi_logo2"]
    params['osu_service_desc'] = ["eng:Example services",
                                  "fin:Esimerkkipalveluja"]
    params['osu_server_uri'] = "https://example.com/osu/"
    hostapd.add_ap(apdev[0], params)

    dev[0].scan_for_bss(bssid, freq="2412")

    # First, fetch two icons from the AP to wpa_supplicant

    if "OK" not in dev[0].request("REQ_HS20_ICON " + bssid + " w1fi_logo"):
        raise Exception("REQ_HS20_ICON failed")

    if "OK" not in dev[0].request("REQ_HS20_ICON " + bssid + " test_logo"):
        raise Exception("REQ_HS20_ICON failed")
    ev = dev[0].wait_event(["RX-HS20-ICON"], timeout=5)
    if ev is None:
        raise Exception("Timeout on RX-HS20-ICON (1)")
    ev = dev[0].wait_event(["RX-HS20-ICON"], timeout=5)
    if ev is None:
        raise Exception("Timeout on RX-HS20-ICON (2)")

    # Then, fetch the icons from wpa_supplicant for validation

    hdr, data1 = get_icon(dev[0], bssid, "w1fi_logo")
    hdr, data2 = get_icon(dev[0], bssid, "test_logo")

    with open('w1fi_logo.png', 'rb') as f:
        data = f.read()
        if data1 != data:
            raise Exception("Unexpected icon data (1)")

    with open('auth_serv/sha512-server.pem', 'rb') as f:
        data = f.read()
        if data2 != data:
            raise Exception("Unexpected icon data (2)")

    # Finally, delete the icons from wpa_supplicant

    if "OK" not in dev[0].request("DEL_HS20_ICON " + bssid + " w1fi_logo"):
        raise Exception("DEL_HS20_ICON failed")
    if "OK" not in dev[0].request("DEL_HS20_ICON " + bssid + " test_logo"):
        raise Exception("DEL_HS20_ICON failed")

def test_ap_hs20_fetch_osu_stop(dev, apdev):
    """Hotspot 2.0 OSU provider fetch stopped"""
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hs20_icon'] = "128:80:zxx:image/png:w1fi_logo:w1fi_logo.png"
    params['osu_ssid'] = '"HS 2.0 OSU open"'
    params['osu_method_list'] = "1"
    params['osu_friendly_name'] = ["eng:Test OSU", "fin:Testi-OSU"]
    params['osu_icon'] = "w1fi_logo"
    params['osu_service_desc'] = ["eng:Example services",
                                  "fin:Esimerkkipalveluja"]
    params['osu_server_uri'] = "https://example.com/osu/"
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable()
    dir = "/tmp/osu-fetch"
    if os.path.isdir(dir):
       files = [f for f in os.listdir(dir) if f.startswith("osu-")]
       for f in files:
           os.remove(dir + "/" + f)
    else:
        try:
            os.makedirs(dir)
        except:
            pass
    try:
        dev[0].request("SET osu_dir " + dir)
        dev[0].request("SCAN freq=2412-2462")
        ev = dev[0].wait_event(["CTRL-EVENT-SCAN-STARTED"], timeout=10)
        if ev is None:
            raise Exception("Scan did not start")
        if "FAIL" not in dev[0].request("FETCH_OSU"):
            raise Exception("FETCH_OSU accepted while scanning")
        ev = dev[0].wait_event(["CTRL-EVENT-SCAN-RESULTS"], 10)
        if ev is None:
            raise Exception("Scan timed out")
        hapd.set("ext_mgmt_frame_handling", "1")
        dev[0].request("FETCH_ANQP")
        if "FAIL" not in dev[0].request("FETCH_OSU"):
            raise Exception("FETCH_OSU accepted while in FETCH_ANQP")
        dev[0].request("STOP_FETCH_ANQP")
        dev[0].wait_event(["GAS-QUERY-DONE"], timeout=5)
        dev[0].dump_monitor()
        hapd.dump_monitor()
        dev[0].request("INTERWORKING_SELECT freq=2412")
        for i in range(5):
            msg = hapd.mgmt_rx()
            if msg['subtype'] == 13:
                break
        if "FAIL" not in dev[0].request("FETCH_OSU"):
            raise Exception("FETCH_OSU accepted while in INTERWORKING_SELECT")
        ev = dev[0].wait_event(["INTERWORKING-AP", "INTERWORKING-NO-MATCH"],
                               timeout=15)
        if ev is None:
            raise Exception("Network selection timed out")

        dev[0].dump_monitor()
        if "OK" not in dev[0].request("FETCH_OSU"):
            raise Exception("FETCH_OSU failed")
        dev[0].request("CANCEL_FETCH_OSU")

        for i in range(15):
            time.sleep(0.5)
            if dev[0].get_driver_status_field("scan_state") == "SCAN_COMPLETED":
                break

        dev[0].dump_monitor()
        if "OK" not in dev[0].request("FETCH_OSU"):
            raise Exception("FETCH_OSU failed")
        if "FAIL" not in dev[0].request("FETCH_OSU"):
            raise Exception("FETCH_OSU accepted while in FETCH_OSU")
        ev = dev[0].wait_event(["GAS-QUERY-START"], 10)
        if ev is None:
            raise Exception("GAS timed out")
        if "FAIL" not in dev[0].request("FETCH_OSU"):
            raise Exception("FETCH_OSU accepted while in FETCH_OSU")
        dev[0].request("CANCEL_FETCH_OSU")
        ev = dev[0].wait_event(["GAS-QUERY-DONE"], 10)
        if ev is None:
            raise Exception("GAS event timed out after CANCEL_FETCH_OSU")
    finally:
        files = [f for f in os.listdir(dir) if f.startswith("osu-")]
        for f in files:
            os.remove(dir + "/" + f)
        os.rmdir(dir)

def test_ap_hs20_fetch_osu_proto(dev, apdev):
    """Hotspot 2.0 OSU provider and protocol testing"""
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable()
    dir = "/tmp/osu-fetch"
    if os.path.isdir(dir):
       files = [f for f in os.listdir(dir) if f.startswith("osu-")]
       for f in files:
           os.remove(dir + "/" + f)
    else:
        try:
            os.makedirs(dir)
        except:
            pass

    tests = [("Empty provider list (no OSU SSID field)", b''),
             ("HS 2.0: Not enough room for OSU SSID",
              binascii.unhexlify('01')),
             ("HS 2.0: Invalid OSU SSID Length 33",
              binascii.unhexlify('21') + 33*b'A'),
             ("HS 2.0: Not enough room for Number of OSU Providers",
              binascii.unhexlify('0130')),
             ("Truncated OSU Provider",
              binascii.unhexlify('013001020000')),
             ("HS 2.0: Ignored 5 bytes of extra data after OSU Providers",
              binascii.unhexlify('0130001122334455')),
             ("HS 2.0: Not enough room for OSU Friendly Name Length",
              binascii.unhexlify('013001000000')),
             ("HS 2.0: Not enough room for OSU Friendly Name Duples",
              build_prov('0100')),
             ("Invalid OSU Friendly Name", build_prov('040000000000')),
             ("Invalid OSU Friendly Name(2)", build_prov('040004000000')),
             ("HS 2.0: Not enough room for OSU Server URI length",
              build_prov('0000')),
             ("HS 2.0: Not enough room for OSU Server URI",
              build_prov('000001')),
             ("HS 2.0: Not enough room for OSU Method list length",
              build_prov('000000')),
             ("HS 2.0: Not enough room for OSU Method list",
              build_prov('00000001')),
             ("HS 2.0: Not enough room for Icons Available Length",
              build_prov('00000000')),
             ("HS 2.0: Not enough room for Icons Available Length(2)",
              build_prov('00000001ff00')),
             ("HS 2.0: Not enough room for Icons Available",
              build_prov('000000000100')),
             ("HS 2.0: Invalid Icon Metadata",
              build_prov('00000000010000')),
             ("HS 2.0: Not room for Icon Type",
              build_prov('000000000900111122223333330200')),
             ("HS 2.0: Not room for Icon Filename length",
              build_prov('000000000900111122223333330100')),
             ("HS 2.0: Not room for Icon Filename",
              build_prov('000000000900111122223333330001')),
             ("HS 2.0: Not enough room for OSU_NAI",
              build_prov('000000000000')),
             ("HS 2.0: Not enough room for OSU_NAI(2)",
              build_prov('00000000000001')),
             ("HS 2.0: Not enough room for OSU Service Description Length",
              build_prov('00000000000000')),
             ("HS 2.0: Not enough room for OSU Service Description Length(2)",
              build_prov('0000000000000000')),
             ("HS 2.0: Not enough room for OSU Service Description Duples",
              build_prov('000000000000000100')),
             ("Invalid OSU Service Description",
              build_prov('00000000000000040000000000')),
             ("Invalid OSU Service Description(2)",
              build_prov('00000000000000040004000000'))]

    try:
        dev[0].request("SET osu_dir " + dir)
        run_fetch_osu_icon_failure(hapd, dev, bssid)
        for note, prov in tests:
            run_fetch_osu(hapd, dev, bssid, note, prov)
    finally:
        files = [f for f in os.listdir(dir) if f.startswith("osu-")]
        for f in files:
            os.remove(dir + "/" + f)
        os.rmdir(dir)

def test_ap_hs20_fetch_osu_invalid_dir(dev, apdev):
    """Hotspot 2.0 OSU provider and invalid directory"""
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hs20_icon'] = "128:80:zxx:image/png:w1fi_logo:w1fi_logo.png"
    params['osu_ssid'] = '"HS 2.0 OSU open"'
    params['osu_method_list'] = "1"
    params['osu_friendly_name'] = ["eng:Test OSU", "fin:Testi-OSU"]
    params['osu_icon'] = "w1fi_logo"
    params['osu_service_desc'] = ["eng:Example services",
                                  "fin:Esimerkkipalveluja"]
    params['osu_server_uri'] = "https://example.com/osu/"
    hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable()
    dir = "/tmp/osu-fetch-no-such-dir"
    dev[0].scan_for_bss(bssid, freq="2412")
    dev[0].request("SET osu_dir " + dir)
    dev[0].request("FETCH_OSU no-scan")
    ev = dev[0].wait_event(["Could not write OSU provider information"],
                           timeout=15)
    if ev is None:
        raise Exception("Timeout on OSU fetch")

def test_ap_hs20_fetch_osu_oom(dev, apdev):
    """Hotspot 2.0 OSU provider and OOM"""
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hs20_icon'] = "128:80:zxx:image/png:w1fi_logo:w1fi_logo.png"
    params['osu_ssid'] = '"HS 2.0 OSU open"'
    params['osu_method_list'] = "1"
    params['osu_friendly_name'] = ["eng:Test OSU", "fin:Testi-OSU"]
    params['osu_icon'] = "w1fi_logo"
    params['osu_service_desc'] = ["eng:Example services",
                                  "fin:Esimerkkipalveluja"]
    params['osu_server_uri'] = "https://example.com/osu/"
    hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable()
    dir = "/tmp/osu-fetch"
    if os.path.isdir(dir):
       files = [f for f in os.listdir(dir) if f.startswith("osu-")]
       for f in files:
           os.remove(dir + "/" + f)
    else:
        try:
            os.makedirs(dir)
        except:
            pass
    dev[0].scan_for_bss(bssid, freq="2412")
    try:
        dev[0].request("SET osu_dir " + dir)
        with alloc_fail(dev[0], 1, "=hs20_osu_add_prov"):
            dev[0].request("FETCH_OSU no-scan")
            ev = dev[0].wait_event(["OSU provider fetch completed"], timeout=30)
            if ev is None:
                raise Exception("Timeout on OSU fetch")
        with alloc_fail(dev[0], 1, "hs20_anqp_send_req;hs20_next_osu_icon"):
            dev[0].request("FETCH_OSU no-scan")
            ev = dev[0].wait_event(["OSU provider fetch completed"], timeout=30)
            if ev is None:
                raise Exception("Timeout on OSU fetch")
    finally:
        files = [f for f in os.listdir(dir) if f.startswith("osu-")]
        for f in files:
            os.remove(dir + "/" + f)
        os.rmdir(dir)

def build_prov(prov):
    data = binascii.unhexlify(prov)
    return binascii.unhexlify('013001') + struct.pack('<H', len(data)) + data

def handle_osu_prov_fetch(hapd, dev, prov):
    # GAS/ANQP query for OSU Providers List
    query = gas_rx(hapd)
    gas = parse_gas(query['payload'])
    dialog_token = gas['dialog_token']

    resp = action_response(query)
    osu_prov = struct.pack('<HH', 0xdddd, len(prov) + 6) + binascii.unhexlify('506f9a110800') + prov
    data = struct.pack('<H', len(osu_prov)) + osu_prov
    resp['payload'] = anqp_initial_resp(dialog_token, 0) + data
    send_gas_resp(hapd, resp)

    ev = dev[0].wait_event(["RX-HS20-ANQP"], timeout=5)
    if ev is None:
        raise Exception("ANQP query response for OSU Providers not received")
    if "OSU Providers list" not in ev:
        raise Exception("ANQP query response for OSU Providers not received(2)")
    ev = dev[0].wait_event(["ANQP-QUERY-DONE"], timeout=5)
    if ev is None:
        raise Exception("ANQP query for OSU Providers list not completed")

def start_osu_fetch(hapd, dev, bssid, note):
    hapd.set("ext_mgmt_frame_handling", "0")
    dev[0].request("BSS_FLUSH 0")
    dev[0].scan_for_bss(bssid, freq="2412")
    hapd.set("ext_mgmt_frame_handling", "1")
    dev[0].dump_monitor()
    dev[0].request("NOTE " + note)
    dev[0].request("FETCH_OSU no-scan")

def wait_osu_fetch_completed(dev):
    ev = dev[0].wait_event(["OSU provider fetch completed"], timeout=5)
    if ev is None:
        raise Exception("Timeout on OSU fetch")

def run_fetch_osu_icon_failure(hapd, dev, bssid):
    start_osu_fetch(hapd, dev, bssid, "Icon fetch failure")

    prov = binascii.unhexlify('01ff' + '01' + '800019000b656e6754657374204f53550c66696e54657374692d4f53551868747470733a2f2f6578616d706c652e636f6d2f6f73752f01011b00800050007a787809696d6167652f706e6709773166695f6c6f676f002a0013656e674578616d706c652073657276696365731566696e4573696d65726b6b6970616c76656c756a61')
    handle_osu_prov_fetch(hapd, dev, prov)

    # GAS/ANQP query for icon
    query = gas_rx(hapd)
    gas = parse_gas(query['payload'])
    dialog_token = gas['dialog_token']

    resp = action_response(query)
    # Unexpected Advertisement Protocol in response
    adv_proto = struct.pack('8B', 108, 6, 127, 0xdd, 0x00, 0x11, 0x22, 0x33)
    data = struct.pack('<H', 0)
    resp['payload'] = struct.pack('<BBBHH', ACTION_CATEG_PUBLIC,
                                  GAS_INITIAL_RESPONSE,
                                  gas['dialog_token'], 0, 0) + adv_proto + data
    send_gas_resp(hapd, resp)

    ev = dev[0].wait_event(["ANQP-QUERY-DONE"], timeout=5)
    if ev is None:
        raise Exception("ANQP query for icon not completed")

    wait_osu_fetch_completed(dev)

def run_fetch_osu(hapd, dev, bssid, note, prov):
    start_osu_fetch(hapd, dev, bssid, note)
    handle_osu_prov_fetch(hapd, dev, prov)
    wait_osu_fetch_completed(dev)

def test_ap_hs20_ft(dev, apdev):
    """Hotspot 2.0 connection with FT"""
    check_eap_capa(dev[0], "MSCHAPV2")
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['wpa_key_mgmt'] = "FT-EAP"
    params['nas_identifier'] = "nas1.w1.fi"
    params['r1_key_holder'] = "000102030405"
    params["mobility_domain"] = "a1b2"
    params["reassociation_deadline"] = "1000"
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable()
    id = dev[0].add_cred_values({'realm': "example.com",
                                 'username': "hs20-test",
                                 'password': "password",
                                 'ca_cert': "auth_serv/ca.pem",
                                 'domain': "example.com",
                                 'update_identifier': "1234"})
    interworking_select(dev[0], bssid, "home", freq="2412")
    interworking_connect(dev[0], bssid, "TTLS")
    dev[0].dump_monitor()
    key_mgmt = dev[0].get_status_field("key_mgmt")
    if key_mgmt != "FT-EAP":
        raise Exception("Unexpected key_mgmt: " + key_mgmt)
    # speed up testing by avoiding unnecessary scanning of other channels
    nid = dev[0].get_status_field("id")
    dev[0].set_network(nid, "scan_freq", "2412")

    params = hs20_ap_params()
    hapd2 = hostapd.add_ap(apdev[1], params)

    hapd.disable()
    ev = dev[0].wait_event(["CTRL-EVENT-DISCONNECTED"], timeout=10)
    if ev is None:
        raise Exception("Disconnection not reported")
    ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED"], timeout=5)
    if ev is None:
        raise Exception("Connection to AP2 not reported")
    key_mgmt = dev[0].get_status_field("key_mgmt")
    if key_mgmt != "WPA2/IEEE 802.1X/EAP":
        raise Exception("Unexpected key_mgmt: " + key_mgmt)

def test_ap_hs20_remediation_sql(dev, apdev, params):
    """Hotspot 2.0 connection and remediation required using SQLite for user DB"""
    check_eap_capa(dev[0], "MSCHAPV2")
    try:
        import sqlite3
    except ImportError:
        raise HwsimSkip("No sqlite3 module available")
    dbfile = params['prefix'] + ".eap-user.db"
    try:
        os.remove(dbfile)
    except:
        pass
    con = sqlite3.connect(dbfile)
    with con:
        cur = con.cursor()
        cur.execute("CREATE TABLE users(identity TEXT PRIMARY KEY, methods TEXT, password TEXT, remediation TEXT, phase2 INTEGER)")
        cur.execute("CREATE TABLE wildcards(identity TEXT PRIMARY KEY, methods TEXT)")
        cur.execute("INSERT INTO users(identity,methods,password,phase2,remediation) VALUES ('user-mschapv2','TTLS-MSCHAPV2','password',1,'user')")
        cur.execute("INSERT INTO wildcards(identity,methods) VALUES ('','TTLS,TLS')")
        cur.execute("CREATE TABLE authlog(timestamp TEXT, session TEXT, nas_ip TEXT, username TEXT, note TEXT)")

    try:
        params = {"ssid": "as", "beacon_int": "2000",
                  "radius_server_clients": "auth_serv/radius_clients.conf",
                  "radius_server_auth_port": '18128',
                  "eap_server": "1",
                  "eap_user_file": "sqlite:" + dbfile,
                  "ca_cert": "auth_serv/ca.pem",
                  "server_cert": "auth_serv/server.pem",
                  "private_key": "auth_serv/server.key",
                  "subscr_remediation_url": "https://example.org/",
                  "subscr_remediation_method": "1"}
        hostapd.add_ap(apdev[1], params)

        bssid = apdev[0]['bssid']
        params = hs20_ap_params()
        params['auth_server_port'] = "18128"
        hostapd.add_ap(apdev[0], params)

        dev[0].request("SET pmf 1")
        dev[0].hs20_enable()
        id = dev[0].add_cred_values({'realm': "example.com",
                                     'username': "user-mschapv2",
                                     'password': "password",
                                     'ca_cert': "auth_serv/ca.pem"})
        interworking_select(dev[0], bssid, freq="2412")
        interworking_connect(dev[0], bssid, "TTLS")
        ev = dev[0].wait_event(["HS20-SUBSCRIPTION-REMEDIATION"], timeout=5)
        if ev is None:
            raise Exception("Timeout on subscription remediation notice")
        if " 1 https://example.org/" not in ev:
            raise Exception("Unexpected subscription remediation event contents")

        with con:
            cur = con.cursor()
            cur.execute("SELECT * from authlog")
            rows = cur.fetchall()
            if len(rows) < 1:
                raise Exception("No authlog entries")

    finally:
        os.remove(dbfile)
        dev[0].request("SET pmf 0")

def test_ap_hs20_sim_provisioning(dev, apdev, params):
    """Hotspot 2.0 AAA server behavior for SIM provisioning"""
    check_eap_capa(dev[0], "SIM")
    try:
        import sqlite3
    except ImportError:
        raise HwsimSkip("No sqlite3 module available")
    dbfile = params['prefix'] + ".eap-user.db"
    try:
        os.remove(dbfile)
    except:
        pass
    con = sqlite3.connect(dbfile)
    with con:
        cur = con.cursor()
        cur.execute("CREATE TABLE users(identity TEXT PRIMARY KEY, methods TEXT, password TEXT, remediation TEXT, phase2 INTEGER, last_msk TEXT)")
        cur.execute("CREATE TABLE wildcards(identity TEXT PRIMARY KEY, methods TEXT)")
        cur.execute("INSERT INTO wildcards(identity,methods) VALUES ('1','SIM')")
        cur.execute("CREATE TABLE authlog(timestamp TEXT, session TEXT, nas_ip TEXT, username TEXT, note TEXT)")
        cur.execute("CREATE TABLE current_sessions(mac_addr TEXT PRIMARY KEY, identity TEXT, start_time TEXT, nas TEXT, hs20_t_c_filtering BOOLEAN, waiting_coa_ack BOOLEAN, coa_ack_received BOOLEAN)")

    try:
        params = {"ssid": "as", "beacon_int": "2000",
                  "radius_server_clients": "auth_serv/radius_clients.conf",
                  "radius_server_auth_port": '18128',
                  "eap_server": "1",
                  "eap_user_file": "sqlite:" + dbfile,
                  "eap_sim_db": "unix:/tmp/hlr_auc_gw.sock",
                  "ca_cert": "auth_serv/ca.pem",
                  "server_cert": "auth_serv/server.pem",
                  "private_key": "auth_serv/server.key",
                  "hs20_sim_provisioning_url":
                  "https://example.org/?hotspot2dot0-mobile-identifier-hash=",
                  "subscr_remediation_method": "1"}
        hostapd.add_ap(apdev[1], params)

        bssid = apdev[0]['bssid']
        params = hs20_ap_params()
        params['auth_server_port'] = "18128"
        hostapd.add_ap(apdev[0], params)

        dev[0].request("SET pmf 1")
        dev[0].hs20_enable()
        dev[0].connect("test-hs20", proto="RSN", key_mgmt="WPA-EAP", eap="SIM",
                       ieee80211w="1",
                       identity="1232010000000000",
                       password="90dca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb82581",
                   scan_freq="2412", update_identifier="54321")
        ev = dev[0].wait_event(["HS20-SUBSCRIPTION-REMEDIATION"], timeout=0.5)
        if ev is not None:
            raise Exception("Unexpected subscription remediation notice")
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()
        dev[0].dump_monitor()

        dev[0].connect("test-hs20", proto="RSN", key_mgmt="WPA-EAP", eap="SIM",
                       ieee80211w="1",
                       identity="1232010000000000",
                       password="90dca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb82581",
                   scan_freq="2412", update_identifier="0")
        ev = dev[0].wait_event(["HS20-SUBSCRIPTION-REMEDIATION"], timeout=5)
        if ev is None:
            raise Exception("Timeout on subscription remediation notice")
        if " 1 https://example.org/?hotspot2dot0-mobile-identifier-hash=" not in ev:
            raise Exception("Unexpected subscription remediation event contents: " + ev)
        id_hash = ev.split(' ')[2].split('=')[1]

        with con:
            cur = con.cursor()
            cur.execute("SELECT * from authlog")
            rows = cur.fetchall()
            if len(rows) < 1:
                raise Exception("No authlog entries")

        with con:
            cur = con.cursor()
            cur.execute("SELECT * from sim_provisioning")
            rows = cur.fetchall()
            if len(rows) != 1:
                raise Exeception("Unexpected number of rows in sim_provisioning (%d; expected %d)" % (len(rows), 1))
            logger.info("sim_provisioning: " + str(rows))
            if len(rows[0][0]) != 32:
                raise Exception("Unexpected mobile_identifier_hash length in DB")
            if rows[0][1] != "232010000000000":
                raise Exception("Unexpected IMSI in DB")
            if rows[0][2] != dev[0].own_addr():
                raise Exception("Unexpected MAC address in DB")
            if rows[0][0] != id_hash:
                raise Exception("hotspot2dot0-mobile-identifier-hash mismatch")
    finally:
        dev[0].request("SET pmf 0")

def test_ap_hs20_external_selection(dev, apdev):
    """Hotspot 2.0 connection using external network selection and creation"""
    check_eap_capa(dev[0], "MSCHAPV2")
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid
    params['disable_dgaf'] = '1'
    hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable()
    dev[0].connect("test-hs20", proto="RSN", key_mgmt="WPA-EAP", eap="TTLS",
                   ieee80211w="1",
                   identity="hs20-test", password="password",
                   ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2",
                   scan_freq="2412", update_identifier="54321",
                   roaming_consortium_selection="1020304050")
    if dev[0].get_status_field("hs20") != "3":
        raise Exception("Unexpected hs20 indication")
    network_id = dev[0].get_status_field("id")
    sel = dev[0].get_network(network_id, "roaming_consortium_selection")
    if sel != "1020304050":
        raise Exception("Unexpected roaming_consortium_selection value: " + sel)

def test_ap_hs20_random_mac_addr(dev, apdev):
    """Hotspot 2.0 connection with random MAC address"""
    check_eap_capa(dev[0], "MSCHAPV2")
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid
    params['disable_dgaf'] = '1'
    hapd = hostapd.add_ap(apdev[0], params)

    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    wpas.interface_add("wlan5")
    addr = wpas.p2p_interface_addr()
    wpas.request("SET mac_addr 1")
    wpas.request("SET preassoc_mac_addr 1")
    wpas.request("SET rand_addr_lifetime 60")
    wpas.hs20_enable()
    wpas.flush_scan_cache()
    id = wpas.add_cred_values({'realm': "example.com",
                               'username': "hs20-test",
                               'password': "password",
                               'ca_cert': "auth_serv/ca.pem",
                               'domain': "example.com",
                               'update_identifier': "1234"})
    interworking_select(wpas, bssid, "home", freq="2412")
    interworking_connect(wpas, bssid, "TTLS")
    addr1 = wpas.get_driver_status_field("addr")
    if addr == addr1:
        raise Exception("Did not use random MAC address")

    sta = hapd.get_sta(addr)
    if sta['addr'] != "FAIL":
        raise Exception("Unexpected STA association with permanent address")
    sta = hapd.get_sta(addr1)
    if sta['addr'] != addr1:
        raise Exception("STA association with random address not found")

def test_ap_hs20_multi_network_and_cred_removal(dev, apdev):
    """Multiple networks and cred removal"""
    check_eap_capa(dev[0], "MSCHAPV2")
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['nai_realm'] = ["0,example.com,25[3:26]"]
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].add_network()
    dev[0].hs20_enable()
    id = dev[0].add_cred_values({'realm': "example.com",
                                 'username': "user",
                                 'password': "password"})
    interworking_select(dev[0], bssid, freq="2412")
    interworking_connect(dev[0], bssid, "PEAP")
    dev[0].add_network()

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected(timeout=10)

    hapd.disable()
    hapd.set("ssid", "another ssid")
    hapd.enable()

    interworking_select(dev[0], bssid, freq="2412")
    interworking_connect(dev[0], bssid, "PEAP")
    dev[0].add_network()
    if len(dev[0].list_networks()) != 5:
        raise Exception("Unexpected number of networks prior to remove_cred")

    dev[0].dump_monitor()
    dev[0].remove_cred(id)
    if len(dev[0].list_networks()) != 3:
        raise Exception("Unexpected number of networks after to remove_cred")
    dev[0].wait_disconnected(timeout=10)

def test_ap_hs20_interworking_add_network(dev, apdev):
    """Hotspot 2.0 connection using INTERWORKING_ADD_NETWORK"""
    check_eap_capa(dev[0], "MSCHAPV2")
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['nai_realm'] = ["0,example.com,21[3:26][6:7][99:99]"]
    hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable()
    dev[0].add_cred_values(default_cred(user="user"))
    interworking_select(dev[0], bssid, freq=2412)
    id = dev[0].interworking_add_network(bssid)
    dev[0].select_network(id, freq=2412)
    dev[0].wait_connected()

def _test_ap_hs20_proxyarp(dev, apdev):
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid
    params['disable_dgaf'] = '0'
    params['proxy_arp'] = '1'
    hapd = hostapd.add_ap(apdev[0], params, no_enable=True)
    if "OK" in hapd.request("ENABLE"):
        raise Exception("Incomplete hostapd configuration was accepted")
    hapd.set("ap_isolate", "1")
    if "OK" in hapd.request("ENABLE"):
        raise Exception("Incomplete hostapd configuration was accepted")
    hapd.set('bridge', 'ap-br0')
    hapd.dump_monitor()
    try:
        hapd.enable()
    except:
        # For now, do not report failures due to missing kernel support
        raise HwsimSkip("Could not start hostapd - assume proxyarp not supported in kernel version")
    ev = hapd.wait_event(["AP-ENABLED", "AP-DISABLED"], timeout=10)
    if ev is None:
        raise Exception("AP startup timed out")
    if "AP-ENABLED" not in ev:
        raise Exception("AP startup failed")

    dev[0].hs20_enable()
    subprocess.call(['brctl', 'setfd', 'ap-br0', '0'])
    subprocess.call(['ip', 'link', 'set', 'dev', 'ap-br0', 'up'])

    id = dev[0].add_cred_values({'realm': "example.com",
                                 'username': "hs20-test",
                                 'password': "password",
                                 'ca_cert': "auth_serv/ca.pem",
                                 'domain': "example.com",
                                 'update_identifier': "1234"})
    interworking_select(dev[0], bssid, "home", freq="2412")
    interworking_connect(dev[0], bssid, "TTLS")

    dev[1].connect("test-hs20", key_mgmt="WPA-EAP", eap="TTLS",
                   identity="hs20-test", password="password",
                   ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2",
                   scan_freq="2412")
    time.sleep(0.1)

    addr0 = dev[0].p2p_interface_addr()
    addr1 = dev[1].p2p_interface_addr()

    src_ll_opt0 = b"\x01\x01" + binascii.unhexlify(addr0.replace(':', ''))
    src_ll_opt1 = b"\x01\x01" + binascii.unhexlify(addr1.replace(':', ''))

    pkt = build_ns(src_ll=addr0, ip_src="aaaa:bbbb:cccc::2",
                   ip_dst="ff02::1:ff00:2", target="aaaa:bbbb:cccc::2",
                   opt=src_ll_opt0)
    if "OK" not in dev[0].request("DATA_TEST_FRAME " + binascii.hexlify(pkt).decode()):
        raise Exception("DATA_TEST_FRAME failed")

    pkt = build_ns(src_ll=addr1, ip_src="aaaa:bbbb:dddd::2",
                   ip_dst="ff02::1:ff00:2", target="aaaa:bbbb:dddd::2",
                   opt=src_ll_opt1)
    if "OK" not in dev[1].request("DATA_TEST_FRAME " + binascii.hexlify(pkt).decode()):
        raise Exception("DATA_TEST_FRAME failed")

    pkt = build_ns(src_ll=addr1, ip_src="aaaa:bbbb:eeee::2",
                   ip_dst="ff02::1:ff00:2", target="aaaa:bbbb:eeee::2",
                   opt=src_ll_opt1)
    if "OK" not in dev[1].request("DATA_TEST_FRAME " + binascii.hexlify(pkt).decode()):
        raise Exception("DATA_TEST_FRAME failed")

    matches = get_permanent_neighbors("ap-br0")
    logger.info("After connect: " + str(matches))
    if len(matches) != 3:
        raise Exception("Unexpected number of neighbor entries after connect")
    if 'aaaa:bbbb:cccc::2 dev ap-br0 lladdr 02:00:00:00:00:00 PERMANENT' not in matches:
        raise Exception("dev0 addr missing")
    if 'aaaa:bbbb:dddd::2 dev ap-br0 lladdr 02:00:00:00:01:00 PERMANENT' not in matches:
        raise Exception("dev1 addr(1) missing")
    if 'aaaa:bbbb:eeee::2 dev ap-br0 lladdr 02:00:00:00:01:00 PERMANENT' not in matches:
        raise Exception("dev1 addr(2) missing")
    dev[0].request("DISCONNECT")
    dev[1].request("DISCONNECT")
    time.sleep(0.5)
    matches = get_permanent_neighbors("ap-br0")
    logger.info("After disconnect: " + str(matches))
    if len(matches) > 0:
        raise Exception("Unexpected neighbor entries after disconnect")

def test_ap_hs20_hidden_ssid_in_scan_res(dev, apdev):
    """Hotspot 2.0 connection with hidden SSId in scan results"""
    check_eap_capa(dev[0], "MSCHAPV2")
    bssid = apdev[0]['bssid']

    hapd = hostapd.add_ap(apdev[0], {"ssid": 'secret',
                                     "ignore_broadcast_ssid": "1"})
    dev[0].scan_for_bss(bssid, freq=2412)
    hapd.disable()
    hapd_global = hostapd.HostapdGlobal(apdev[0])
    hapd_global.flush()
    hapd_global.remove(apdev[0]['ifname'])

    params = hs20_ap_params()
    params['hessid'] = bssid
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable()
    id = dev[0].add_cred_values({'realm': "example.com",
                                 'username': "hs20-test",
                                 'password': "password",
                                 'ca_cert': "auth_serv/ca.pem",
                                 'domain': "example.com"})
    interworking_select(dev[0], bssid, "home", freq="2412")
    interworking_connect(dev[0], bssid, "TTLS")

    # clear BSS table to avoid issues in following test cases
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()
    hapd.disable()
    dev[0].flush_scan_cache()
    dev[0].flush_scan_cache()

def test_ap_hs20_proxyarp(dev, apdev):
    """Hotspot 2.0 and ProxyARP"""
    check_eap_capa(dev[0], "MSCHAPV2")
    try:
        _test_ap_hs20_proxyarp(dev, apdev)
    finally:
        subprocess.call(['ip', 'link', 'set', 'dev', 'ap-br0', 'down'],
                        stderr=open('/dev/null', 'w'))
        subprocess.call(['brctl', 'delbr', 'ap-br0'],
                        stderr=open('/dev/null', 'w'))

def _test_ap_hs20_proxyarp_dgaf(dev, apdev, disabled):
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid
    params['disable_dgaf'] = '1' if disabled else '0'
    params['proxy_arp'] = '1'
    params['na_mcast_to_ucast'] = '1'
    params['ap_isolate'] = '1'
    params['bridge'] = 'ap-br0'
    hapd = hostapd.add_ap(apdev[0], params, no_enable=True)
    try:
        hapd.enable()
    except:
        # For now, do not report failures due to missing kernel support
        raise HwsimSkip("Could not start hostapd - assume proxyarp not supported in kernel version")
    ev = hapd.wait_event(["AP-ENABLED"], timeout=10)
    if ev is None:
        raise Exception("AP startup timed out")

    dev[0].hs20_enable()
    subprocess.call(['brctl', 'setfd', 'ap-br0', '0'])
    subprocess.call(['ip', 'link', 'set', 'dev', 'ap-br0', 'up'])

    id = dev[0].add_cred_values({'realm': "example.com",
                                 'username': "hs20-test",
                                 'password': "password",
                                 'ca_cert': "auth_serv/ca.pem",
                                 'domain': "example.com",
                                 'update_identifier': "1234"})
    interworking_select(dev[0], bssid, "home", freq="2412")
    interworking_connect(dev[0], bssid, "TTLS")

    dev[1].connect("test-hs20", key_mgmt="WPA-EAP", eap="TTLS",
                   identity="hs20-test", password="password",
                   ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2",
                   scan_freq="2412")
    time.sleep(0.1)

    addr0 = dev[0].p2p_interface_addr()

    src_ll_opt0 = b"\x01\x01" + binascii.unhexlify(addr0.replace(':', ''))

    pkt = build_ns(src_ll=addr0, ip_src="aaaa:bbbb:cccc::2",
                   ip_dst="ff02::1:ff00:2", target="aaaa:bbbb:cccc::2",
                   opt=src_ll_opt0)
    if "OK" not in dev[0].request("DATA_TEST_FRAME " + binascii.hexlify(pkt).decode()):
        raise Exception("DATA_TEST_FRAME failed")

    pkt = build_ra(src_ll=apdev[0]['bssid'], ip_src="aaaa:bbbb:cccc::33",
                   ip_dst="ff01::1")
    if "OK" not in hapd.request("DATA_TEST_FRAME ifname=ap-br0 " + binascii.hexlify(pkt).decode()):
        raise Exception("DATA_TEST_FRAME failed")

    pkt = build_na(src_ll=apdev[0]['bssid'], ip_src="aaaa:bbbb:cccc::44",
                   ip_dst="ff01::1", target="aaaa:bbbb:cccc::55")
    if "OK" not in hapd.request("DATA_TEST_FRAME ifname=ap-br0 " + binascii.hexlify(pkt).decode()):
        raise Exception("DATA_TEST_FRAME failed")

    pkt = build_dhcp_ack(dst_ll="ff:ff:ff:ff:ff:ff", src_ll=bssid,
                         ip_src="192.168.1.1", ip_dst="255.255.255.255",
                         yiaddr="192.168.1.123", chaddr=addr0)
    if "OK" not in hapd.request("DATA_TEST_FRAME ifname=ap-br0 " + binascii.hexlify(pkt).decode()):
        raise Exception("DATA_TEST_FRAME failed")
    # another copy for additional code coverage
    pkt = build_dhcp_ack(dst_ll=addr0, src_ll=bssid,
                         ip_src="192.168.1.1", ip_dst="255.255.255.255",
                         yiaddr="192.168.1.123", chaddr=addr0)
    if "OK" not in hapd.request("DATA_TEST_FRAME ifname=ap-br0 " + binascii.hexlify(pkt).decode()):
        raise Exception("DATA_TEST_FRAME failed")

    matches = get_permanent_neighbors("ap-br0")
    logger.info("After connect: " + str(matches))
    if len(matches) != 2:
        raise Exception("Unexpected number of neighbor entries after connect")
    if 'aaaa:bbbb:cccc::2 dev ap-br0 lladdr 02:00:00:00:00:00 PERMANENT' not in matches:
        raise Exception("dev0 addr missing")
    if '192.168.1.123 dev ap-br0 lladdr 02:00:00:00:00:00 PERMANENT' not in matches:
        raise Exception("dev0 IPv4 addr missing")
    dev[0].request("DISCONNECT")
    dev[1].request("DISCONNECT")
    time.sleep(0.5)
    matches = get_permanent_neighbors("ap-br0")
    logger.info("After disconnect: " + str(matches))
    if len(matches) > 0:
        raise Exception("Unexpected neighbor entries after disconnect")

def test_ap_hs20_proxyarp_disable_dgaf(dev, apdev):
    """Hotspot 2.0 and ProxyARP with DGAF disabled"""
    check_eap_capa(dev[0], "MSCHAPV2")
    try:
        _test_ap_hs20_proxyarp_dgaf(dev, apdev, True)
    finally:
        subprocess.call(['ip', 'link', 'set', 'dev', 'ap-br0', 'down'],
                        stderr=open('/dev/null', 'w'))
        subprocess.call(['brctl', 'delbr', 'ap-br0'],
                        stderr=open('/dev/null', 'w'))

def test_ap_hs20_proxyarp_enable_dgaf(dev, apdev):
    """Hotspot 2.0 and ProxyARP with DGAF enabled"""
    check_eap_capa(dev[0], "MSCHAPV2")
    try:
        _test_ap_hs20_proxyarp_dgaf(dev, apdev, False)
    finally:
        subprocess.call(['ip', 'link', 'set', 'dev', 'ap-br0', 'down'],
                        stderr=open('/dev/null', 'w'))
        subprocess.call(['brctl', 'delbr', 'ap-br0'],
                        stderr=open('/dev/null', 'w'))

def ip_checksum(buf):
    sum = 0
    if len(buf) & 0x01:
        buf += b'\x00'
    for i in range(0, len(buf), 2):
        val, = struct.unpack('H', buf[i:i+2])
        sum += val
    while (sum >> 16):
        sum = (sum & 0xffff) + (sum >> 16)
    return struct.pack('H', ~sum & 0xffff)

def ipv6_solicited_node_mcaddr(target):
    prefix = socket.inet_pton(socket.AF_INET6, "ff02::1:ff00:0")
    mask = socket.inet_pton(socket.AF_INET6, "::ff:ffff")
    _target = socket.inet_pton(socket.AF_INET6, target)
    p = struct.unpack('4I', prefix)
    m = struct.unpack('4I', mask)
    t = struct.unpack('4I', _target)
    res = (p[0] | (t[0] & m[0]),
           p[1] | (t[1] & m[1]),
           p[2] | (t[2] & m[2]),
           p[3] | (t[3] & m[3]))
    return socket.inet_ntop(socket.AF_INET6, struct.pack('4I', *res))

def build_icmpv6(ipv6_addrs, type, code, payload):
    start = struct.pack("BB", type, code)
    end = payload
    icmp = start + b'\x00\x00' + end
    pseudo = ipv6_addrs + struct.pack(">LBBBB", len(icmp), 0, 0, 0, 58)
    csum = ip_checksum(pseudo + icmp)
    return start + csum + end

def build_ra(src_ll, ip_src, ip_dst, cur_hop_limit=0, router_lifetime=0,
             reachable_time=0, retrans_timer=0, opt=None):
    link_mc = binascii.unhexlify("3333ff000002")
    _src_ll = binascii.unhexlify(src_ll.replace(':', ''))
    proto = b'\x86\xdd'
    ehdr = link_mc + _src_ll + proto
    _ip_src = socket.inet_pton(socket.AF_INET6, ip_src)
    _ip_dst = socket.inet_pton(socket.AF_INET6, ip_dst)

    adv = struct.pack('>BBHLL', cur_hop_limit, 0, router_lifetime,
                      reachable_time, retrans_timer)
    if opt:
        payload = adv + opt
    else:
        payload = adv
    icmp = build_icmpv6(_ip_src + _ip_dst, 134, 0, payload)

    ipv6 = struct.pack('>BBBBHBB', 0x60, 0, 0, 0, len(icmp), 58, 255)
    ipv6 += _ip_src + _ip_dst

    return ehdr + ipv6 + icmp

def build_ns(src_ll, ip_src, ip_dst, target, opt=None):
    link_mc = binascii.unhexlify("3333ff000002")
    _src_ll = binascii.unhexlify(src_ll.replace(':', ''))
    proto = b'\x86\xdd'
    ehdr = link_mc + _src_ll + proto
    _ip_src = socket.inet_pton(socket.AF_INET6, ip_src)
    if ip_dst is None:
        ip_dst = ipv6_solicited_node_mcaddr(target)
    _ip_dst = socket.inet_pton(socket.AF_INET6, ip_dst)

    reserved = b'\x00\x00\x00\x00'
    _target = socket.inet_pton(socket.AF_INET6, target)
    if opt:
        payload = reserved + _target + opt
    else:
        payload = reserved + _target
    icmp = build_icmpv6(_ip_src + _ip_dst, 135, 0, payload)

    ipv6 = struct.pack('>BBBBHBB', 0x60, 0, 0, 0, len(icmp), 58, 255)
    ipv6 += _ip_src + _ip_dst

    return ehdr + ipv6 + icmp

def send_ns(dev, src_ll=None, target=None, ip_src=None, ip_dst=None, opt=None,
            hapd_bssid=None):
    if hapd_bssid:
        if src_ll is None:
            src_ll = hapd_bssid
        cmd = "DATA_TEST_FRAME ifname=ap-br0 "
    else:
        if src_ll is None:
            src_ll = dev.p2p_interface_addr()
        cmd = "DATA_TEST_FRAME "

    if opt is None:
        opt = b"\x01\x01" + binascii.unhexlify(src_ll.replace(':', ''))

    pkt = build_ns(src_ll=src_ll, ip_src=ip_src, ip_dst=ip_dst, target=target,
                   opt=opt)
    if "OK" not in dev.request(cmd + binascii.hexlify(pkt).decode()):
        raise Exception("DATA_TEST_FRAME failed")

def build_na(src_ll, ip_src, ip_dst, target, opt=None, flags=0):
    link_mc = binascii.unhexlify("3333ff000002")
    _src_ll = binascii.unhexlify(src_ll.replace(':', ''))
    proto = b'\x86\xdd'
    ehdr = link_mc + _src_ll + proto
    _ip_src = socket.inet_pton(socket.AF_INET6, ip_src)
    _ip_dst = socket.inet_pton(socket.AF_INET6, ip_dst)

    _target = socket.inet_pton(socket.AF_INET6, target)
    if opt:
        payload = struct.pack('>Bxxx', flags) + _target + opt
    else:
        payload = struct.pack('>Bxxx', flags) + _target
    icmp = build_icmpv6(_ip_src + _ip_dst, 136, 0, payload)

    ipv6 = struct.pack('>BBBBHBB', 0x60, 0, 0, 0, len(icmp), 58, 255)
    ipv6 += _ip_src + _ip_dst

    return ehdr + ipv6 + icmp

def send_na(dev, src_ll=None, target=None, ip_src=None, ip_dst=None, opt=None,
            hapd_bssid=None):
    if hapd_bssid:
        if src_ll is None:
            src_ll = hapd_bssid
        cmd = "DATA_TEST_FRAME ifname=ap-br0 "
    else:
        if src_ll is None:
            src_ll = dev.p2p_interface_addr()
        cmd = "DATA_TEST_FRAME "

    pkt = build_na(src_ll=src_ll, ip_src=ip_src, ip_dst=ip_dst, target=target,
                   opt=opt)
    if "OK" not in dev.request(cmd + binascii.hexlify(pkt).decode()):
        raise Exception("DATA_TEST_FRAME failed")

def build_dhcp_ack(dst_ll, src_ll, ip_src, ip_dst, yiaddr, chaddr,
                   subnet_mask="255.255.255.0", truncated_opt=False,
                   wrong_magic=False, force_tot_len=None, no_dhcp=False,
                   udp_checksum=True):
    _dst_ll = binascii.unhexlify(dst_ll.replace(':', ''))
    _src_ll = binascii.unhexlify(src_ll.replace(':', ''))
    proto = b'\x08\x00'
    ehdr = _dst_ll + _src_ll + proto
    _ip_src = socket.inet_pton(socket.AF_INET, ip_src)
    _ip_dst = socket.inet_pton(socket.AF_INET, ip_dst)
    _subnet_mask = socket.inet_pton(socket.AF_INET, subnet_mask)

    _ciaddr = b'\x00\x00\x00\x00'
    _yiaddr = socket.inet_pton(socket.AF_INET, yiaddr)
    _siaddr = b'\x00\x00\x00\x00'
    _giaddr = b'\x00\x00\x00\x00'
    _chaddr = binascii.unhexlify(chaddr.replace(':', '') + "00000000000000000000")
    payload = struct.pack('>BBBBL3BB', 2, 1, 6, 0, 12345, 0, 0, 0, 0)
    payload += _ciaddr + _yiaddr + _siaddr + _giaddr + _chaddr + 192*b'\x00'
    # magic
    if wrong_magic:
        payload += b'\x63\x82\x53\x00'
    else:
        payload += b'\x63\x82\x53\x63'
    if truncated_opt:
        payload += b'\x22\xff\x00'
    # Option: DHCP Message Type = ACK
    payload += b'\x35\x01\x05'
    # Pad Option
    payload += b'\x00'
    # Option: Subnet Mask
    payload += b'\x01\x04' + _subnet_mask
    # Option: Time Offset
    payload += struct.pack('>BBL', 2, 4, 0)
    # End Option
    payload += b'\xff'
    # Pad Option
    payload += b'\x00\x00\x00\x00'

    if no_dhcp:
        payload = struct.pack('>BBBBL3BB', 2, 1, 6, 0, 12345, 0, 0, 0, 0)
        payload += _ciaddr + _yiaddr + _siaddr + _giaddr + _chaddr + 192*b'\x00'

    if udp_checksum:
        pseudohdr = _ip_src + _ip_dst + struct.pack('>BBH', 0, 17,
                                                    8 + len(payload))
        udphdr = struct.pack('>HHHH', 67, 68, 8 + len(payload), 0)
        checksum, = struct.unpack('>H', ip_checksum(pseudohdr + udphdr + payload))
    else:
        checksum = 0
    udp = struct.pack('>HHHH', 67, 68, 8 + len(payload), checksum) + payload

    if force_tot_len:
        tot_len = force_tot_len
    else:
        tot_len = 20 + len(udp)
    start = struct.pack('>BBHHBBBB', 0x45, 0, tot_len, 0, 0, 0, 128, 17)
    ipv4 = start + b'\x00\x00' + _ip_src + _ip_dst
    csum = ip_checksum(ipv4)
    ipv4 = start + csum + _ip_src + _ip_dst

    return ehdr + ipv4 + udp

def build_arp(dst_ll, src_ll, opcode, sender_mac, sender_ip,
              target_mac, target_ip):
    _dst_ll = binascii.unhexlify(dst_ll.replace(':', ''))
    _src_ll = binascii.unhexlify(src_ll.replace(':', ''))
    proto = b'\x08\x06'
    ehdr = _dst_ll + _src_ll + proto

    _sender_mac = binascii.unhexlify(sender_mac.replace(':', ''))
    _sender_ip = socket.inet_pton(socket.AF_INET, sender_ip)
    _target_mac = binascii.unhexlify(target_mac.replace(':', ''))
    _target_ip = socket.inet_pton(socket.AF_INET, target_ip)

    arp = struct.pack('>HHBBH', 1, 0x0800, 6, 4, opcode)
    arp += _sender_mac + _sender_ip
    arp += _target_mac + _target_ip

    return ehdr + arp

def send_arp(dev, dst_ll="ff:ff:ff:ff:ff:ff", src_ll=None, opcode=1,
             sender_mac=None, sender_ip="0.0.0.0",
             target_mac="00:00:00:00:00:00", target_ip="0.0.0.0",
             hapd_bssid=None):
    if hapd_bssid:
        if src_ll is None:
            src_ll = hapd_bssid
        if sender_mac is None:
            sender_mac = hapd_bssid
        cmd = "DATA_TEST_FRAME ifname=ap-br0 "
    else:
        if src_ll is None:
            src_ll = dev.p2p_interface_addr()
        if sender_mac is None:
            sender_mac = dev.p2p_interface_addr()
        cmd = "DATA_TEST_FRAME "

    pkt = build_arp(dst_ll=dst_ll, src_ll=src_ll, opcode=opcode,
                    sender_mac=sender_mac, sender_ip=sender_ip,
                    target_mac=target_mac, target_ip=target_ip)
    if "OK" not in dev.request(cmd + binascii.hexlify(pkt).decode()):
        raise Exception("DATA_TEST_FRAME failed")

def get_permanent_neighbors(ifname):
    cmd = subprocess.Popen(['ip', 'nei'], stdout=subprocess.PIPE)
    res = cmd.stdout.read().decode()
    cmd.stdout.close()
    return [line for line in res.splitlines() if "PERMANENT" in line and ifname in line]

def get_bridge_macs(ifname):
    cmd = subprocess.Popen(['brctl', 'showmacs', ifname],
                           stdout=subprocess.PIPE)
    res = cmd.stdout.read()
    cmd.stdout.close()
    return res.decode()

def tshark_get_arp(cap, filter):
    res = run_tshark(cap, filter,
                     ["eth.dst", "eth.src",
                      "arp.src.hw_mac", "arp.src.proto_ipv4",
                      "arp.dst.hw_mac", "arp.dst.proto_ipv4"],
                     wait=False)
    frames = []
    for l in res.splitlines():
        frames.append(l.split('\t'))
    return frames

def tshark_get_ns(cap):
    res = run_tshark(cap, "icmpv6.type == 135",
                     ["eth.dst", "eth.src",
                      "ipv6.src", "ipv6.dst",
                      "icmpv6.nd.ns.target_address",
                      "icmpv6.opt.linkaddr"],
                     wait=False)
    frames = []
    for l in res.splitlines():
        frames.append(l.split('\t'))
    return frames

def tshark_get_na(cap):
    res = run_tshark(cap, "icmpv6.type == 136",
                     ["eth.dst", "eth.src",
                      "ipv6.src", "ipv6.dst",
                      "icmpv6.nd.na.target_address",
                      "icmpv6.opt.linkaddr"],
                     wait=False)
    frames = []
    for l in res.splitlines():
        frames.append(l.split('\t'))
    return frames

def _test_proxyarp_open(dev, apdev, params, ebtables=False):
    cap_br = params['prefix'] + ".ap-br0.pcap"
    cap_dev0 = params['prefix'] + ".%s.pcap" % dev[0].ifname
    cap_dev1 = params['prefix'] + ".%s.pcap" % dev[1].ifname
    cap_dev2 = params['prefix'] + ".%s.pcap" % dev[2].ifname

    bssid = apdev[0]['bssid']
    params = {'ssid': 'open'}
    params['proxy_arp'] = '1'
    hapd = hostapd.add_ap(apdev[0], params, no_enable=True)
    hapd.set("ap_isolate", "1")
    hapd.set('bridge', 'ap-br0')
    hapd.dump_monitor()
    try:
        hapd.enable()
    except:
        # For now, do not report failures due to missing kernel support
        raise HwsimSkip("Could not start hostapd - assume proxyarp not supported in kernel version")
    ev = hapd.wait_event(["AP-ENABLED", "AP-DISABLED"], timeout=10)
    if ev is None:
        raise Exception("AP startup timed out")
    if "AP-ENABLED" not in ev:
        raise Exception("AP startup failed")

    params2 = {'ssid': 'another'}
    hapd2 = hostapd.add_ap(apdev[1], params2, no_enable=True)
    hapd2.set('bridge', 'ap-br0')
    hapd2.enable()

    subprocess.call(['brctl', 'setfd', 'ap-br0', '0'])
    subprocess.call(['ip', 'link', 'set', 'dev', 'ap-br0', 'up'])

    if ebtables:
        for chain in ['FORWARD', 'OUTPUT']:
            try:
                err = subprocess.call(['ebtables', '-A', chain, '-p', 'ARP',
                                       '-d', 'Broadcast',
                                       '-o', apdev[0]['ifname'],
                                       '-j', 'DROP'])
                if err != 0:
                    raise
            except:
                raise HwsimSkip("No ebtables available")

    time.sleep(0.5)
    cmd = {}
    cmd[0] = WlantestCapture('ap-br0', cap_br)
    cmd[1] = WlantestCapture(dev[0].ifname, cap_dev0)
    cmd[2] = WlantestCapture(dev[1].ifname, cap_dev1)
    cmd[3] = WlantestCapture(dev[2].ifname, cap_dev2)

    dev[0].connect("open", key_mgmt="NONE", scan_freq="2412")
    dev[1].connect("open", key_mgmt="NONE", scan_freq="2412")
    dev[2].connect("another", key_mgmt="NONE", scan_freq="2412")
    time.sleep(1.1)

    brcmd = subprocess.Popen(['brctl', 'show'], stdout=subprocess.PIPE)
    res = brcmd.stdout.read().decode()
    brcmd.stdout.close()
    logger.info("Bridge setup: " + res)

    brcmd = subprocess.Popen(['brctl', 'showstp', 'ap-br0'],
                             stdout=subprocess.PIPE)
    res = brcmd.stdout.read().decode()
    brcmd.stdout.close()
    logger.info("Bridge showstp: " + res)

    addr0 = dev[0].p2p_interface_addr()
    addr1 = dev[1].p2p_interface_addr()
    addr2 = dev[2].p2p_interface_addr()

    pkt = build_dhcp_ack(dst_ll="ff:ff:ff:ff:ff:ff", src_ll=bssid,
                         ip_src="192.168.1.1", ip_dst="255.255.255.255",
                         yiaddr="192.168.1.124", chaddr=addr0)
    if "OK" not in hapd.request("DATA_TEST_FRAME ifname=ap-br0 " + binascii.hexlify(pkt).decode()):
        raise Exception("DATA_TEST_FRAME failed")
    # Change address and verify unicast
    pkt = build_dhcp_ack(dst_ll=addr0, src_ll=bssid,
                         ip_src="192.168.1.1", ip_dst="255.255.255.255",
                         yiaddr="192.168.1.123", chaddr=addr0,
                         udp_checksum=False)
    if "OK" not in hapd.request("DATA_TEST_FRAME ifname=ap-br0 " + binascii.hexlify(pkt).decode()):
        raise Exception("DATA_TEST_FRAME failed")

    # Not-associated client MAC address
    pkt = build_dhcp_ack(dst_ll="ff:ff:ff:ff:ff:ff", src_ll=bssid,
                         ip_src="192.168.1.1", ip_dst="255.255.255.255",
                         yiaddr="192.168.1.125", chaddr="22:33:44:55:66:77")
    if "OK" not in hapd.request("DATA_TEST_FRAME ifname=ap-br0 " + binascii.hexlify(pkt).decode()):
        raise Exception("DATA_TEST_FRAME failed")

    # No IP address
    pkt = build_dhcp_ack(dst_ll=addr1, src_ll=bssid,
                         ip_src="192.168.1.1", ip_dst="255.255.255.255",
                         yiaddr="0.0.0.0", chaddr=addr1)
    if "OK" not in hapd.request("DATA_TEST_FRAME ifname=ap-br0 " + binascii.hexlify(pkt).decode()):
        raise Exception("DATA_TEST_FRAME failed")

    # Zero subnet mask
    pkt = build_dhcp_ack(dst_ll=addr1, src_ll=bssid,
                         ip_src="192.168.1.1", ip_dst="255.255.255.255",
                         yiaddr="192.168.1.126", chaddr=addr1,
                         subnet_mask="0.0.0.0")
    if "OK" not in hapd.request("DATA_TEST_FRAME ifname=ap-br0 " + binascii.hexlify(pkt).decode()):
        raise Exception("DATA_TEST_FRAME failed")

    # Truncated option
    pkt = build_dhcp_ack(dst_ll=addr1, src_ll=bssid,
                         ip_src="192.168.1.1", ip_dst="255.255.255.255",
                         yiaddr="192.168.1.127", chaddr=addr1,
                         truncated_opt=True)
    if "OK" not in hapd.request("DATA_TEST_FRAME ifname=ap-br0 " + binascii.hexlify(pkt).decode()):
        raise Exception("DATA_TEST_FRAME failed")

    # Wrong magic
    pkt = build_dhcp_ack(dst_ll=addr1, src_ll=bssid,
                         ip_src="192.168.1.1", ip_dst="255.255.255.255",
                         yiaddr="192.168.1.128", chaddr=addr1,
                         wrong_magic=True)
    if "OK" not in hapd.request("DATA_TEST_FRAME ifname=ap-br0 " + binascii.hexlify(pkt).decode()):
        raise Exception("DATA_TEST_FRAME failed")

    # Wrong IPv4 total length
    pkt = build_dhcp_ack(dst_ll=addr1, src_ll=bssid,
                         ip_src="192.168.1.1", ip_dst="255.255.255.255",
                         yiaddr="192.168.1.129", chaddr=addr1,
                         force_tot_len=1000)
    if "OK" not in hapd.request("DATA_TEST_FRAME ifname=ap-br0 " + binascii.hexlify(pkt).decode()):
        raise Exception("DATA_TEST_FRAME failed")

    # BOOTP
    pkt = build_dhcp_ack(dst_ll=addr1, src_ll=bssid,
                         ip_src="192.168.1.1", ip_dst="255.255.255.255",
                         yiaddr="192.168.1.129", chaddr=addr1,
                         no_dhcp=True)
    if "OK" not in hapd.request("DATA_TEST_FRAME ifname=ap-br0 " + binascii.hexlify(pkt).decode()):
        raise Exception("DATA_TEST_FRAME failed")

    macs = get_bridge_macs("ap-br0")
    logger.info("After connect (showmacs): " + str(macs))

    matches = get_permanent_neighbors("ap-br0")
    logger.info("After connect: " + str(matches))
    if len(matches) != 1:
        raise Exception("Unexpected number of neighbor entries after connect")
    if '192.168.1.123 dev ap-br0 lladdr 02:00:00:00:00:00 PERMANENT' not in matches:
        raise Exception("dev0 IPv4 addr missing")

    targets = ["192.168.1.123", "192.168.1.124", "192.168.1.125",
               "192.168.1.126"]
    for target in targets:
        send_arp(dev[1], sender_ip="192.168.1.100", target_ip=target)

    for target in targets:
        send_arp(hapd, hapd_bssid=bssid, sender_ip="192.168.1.101",
                 target_ip=target)

    for target in targets:
        send_arp(dev[2], sender_ip="192.168.1.103", target_ip=target)

    # ARP Probe from wireless STA
    send_arp(dev[1], target_ip="192.168.1.127")
    # ARP Announcement from wireless STA
    send_arp(dev[1], sender_ip="192.168.1.127", target_ip="192.168.1.127")
    send_arp(dev[1], sender_ip="192.168.1.127", target_ip="192.168.1.127",
             opcode=2)

    macs = get_bridge_macs("ap-br0")
    logger.info("After ARP Probe + Announcement (showmacs): " + str(macs))

    matches = get_permanent_neighbors("ap-br0")
    logger.info("After ARP Probe + Announcement: " + str(matches))

    # ARP Request for the newly introduced IP address from wireless STA
    send_arp(dev[0], sender_ip="192.168.1.123", target_ip="192.168.1.127")

    # ARP Request for the newly introduced IP address from bridge
    send_arp(hapd, hapd_bssid=bssid, sender_ip="192.168.1.102",
             target_ip="192.168.1.127")
    send_arp(dev[2], sender_ip="192.168.1.103", target_ip="192.168.1.127")

    # ARP Probe from bridge
    send_arp(hapd, hapd_bssid=bssid, target_ip="192.168.1.130")
    send_arp(dev[2], target_ip="192.168.1.131")
    # ARP Announcement from bridge (not to be learned by AP for proxyarp)
    send_arp(hapd, hapd_bssid=bssid, sender_ip="192.168.1.130",
             target_ip="192.168.1.130")
    send_arp(hapd, hapd_bssid=bssid, sender_ip="192.168.1.130",
             target_ip="192.168.1.130", opcode=2)
    send_arp(dev[2], sender_ip="192.168.1.131", target_ip="192.168.1.131")
    send_arp(dev[2], sender_ip="192.168.1.131", target_ip="192.168.1.131",
             opcode=2)

    macs = get_bridge_macs("ap-br0")
    logger.info("After ARP Probe + Announcement (showmacs): " + str(macs))

    matches = get_permanent_neighbors("ap-br0")
    logger.info("After ARP Probe + Announcement: " + str(matches))

    # ARP Request for the newly introduced IP address from wireless STA
    send_arp(dev[0], sender_ip="192.168.1.123", target_ip="192.168.1.130")
    # ARP Response from bridge (AP does not proxy for non-wireless devices)
    send_arp(hapd, hapd_bssid=bssid, dst_ll=addr0, sender_ip="192.168.1.130",
             target_ip="192.168.1.123", opcode=2)

    # ARP Request for the newly introduced IP address from wireless STA
    send_arp(dev[0], sender_ip="192.168.1.123", target_ip="192.168.1.131")
    # ARP Response from bridge (AP does not proxy for non-wireless devices)
    send_arp(dev[2], dst_ll=addr0, sender_ip="192.168.1.131",
             target_ip="192.168.1.123", opcode=2)

    # ARP Request for the newly introduced IP address from bridge
    send_arp(hapd, hapd_bssid=bssid, sender_ip="192.168.1.102",
             target_ip="192.168.1.130")
    send_arp(dev[2], sender_ip="192.168.1.104", target_ip="192.168.1.131")

    # ARP Probe from wireless STA (duplicate address; learned through DHCP)
    send_arp(dev[1], target_ip="192.168.1.123")
    # ARP Probe from wireless STA (duplicate address; learned through ARP)
    send_arp(dev[0], target_ip="192.168.1.127")

    # Gratuitous ARP Reply for another STA's IP address
    send_arp(dev[0], opcode=2, sender_mac=addr0, sender_ip="192.168.1.127",
             target_mac=addr1, target_ip="192.168.1.127")
    send_arp(dev[1], opcode=2, sender_mac=addr1, sender_ip="192.168.1.123",
             target_mac=addr0, target_ip="192.168.1.123")
    # ARP Request to verify previous mapping
    send_arp(dev[1], sender_ip="192.168.1.127", target_ip="192.168.1.123")
    send_arp(dev[0], sender_ip="192.168.1.123", target_ip="192.168.1.127")

    try:
        hwsim_utils.test_connectivity_iface(dev[0], hapd, "ap-br0")
    except Exception as e:
        logger.info("test_connectibity_iface failed: " + str(e))
        raise HwsimSkip("Assume kernel did not have the required patches for proxyarp")
    hwsim_utils.test_connectivity_iface(dev[1], hapd, "ap-br0")
    hwsim_utils.test_connectivity(dev[0], dev[1])

    dev[0].request("DISCONNECT")
    dev[1].request("DISCONNECT")
    time.sleep(1.5)
    for i in range(len(cmd)):
        cmd[i].close()
    time.sleep(0.1)
    macs = get_bridge_macs("ap-br0")
    logger.info("After disconnect (showmacs): " + str(macs))
    matches = get_permanent_neighbors("ap-br0")
    logger.info("After disconnect: " + str(matches))
    if len(matches) > 0:
        raise Exception("Unexpected neighbor entries after disconnect")
    if ebtables:
        cmd = subprocess.Popen(['ebtables', '-L', '--Lc'],
                               stdout=subprocess.PIPE)
        res = cmd.stdout.read().decode()
        cmd.stdout.close()
        logger.info("ebtables results:\n" + res)

    # Verify that expected ARP messages were seen and no unexpected
    # ARP messages were seen.

    arp_req = tshark_get_arp(cap_dev0, "arp.opcode == 1")
    arp_reply = tshark_get_arp(cap_dev0, "arp.opcode == 2")
    logger.info("dev0 seen ARP requests:\n" + str(arp_req))
    logger.info("dev0 seen ARP replies:\n" + str(arp_reply))

    if ['ff:ff:ff:ff:ff:ff', addr1,
        addr1, '192.168.1.100',
        '00:00:00:00:00:00', '192.168.1.123'] in arp_req:
        raise Exception("dev0 saw ARP request from dev1")
    if ['ff:ff:ff:ff:ff:ff', addr2,
        addr2, '192.168.1.103',
        '00:00:00:00:00:00', '192.168.1.123'] in arp_req:
        raise Exception("dev0 saw ARP request from dev2")
    # TODO: Uncomment once fixed in kernel
    #if ['ff:ff:ff:ff:ff:ff', bssid,
    #    bssid, '192.168.1.101',
    #    '00:00:00:00:00:00', '192.168.1.123'] in arp_req:
    #    raise Exception("dev0 saw ARP request from br")

    if ebtables:
        for req in arp_req:
            if req[1] != addr0:
                raise Exception("Unexpected foreign ARP request on dev0")

    arp_req = tshark_get_arp(cap_dev1, "arp.opcode == 1")
    arp_reply = tshark_get_arp(cap_dev1, "arp.opcode == 2")
    logger.info("dev1 seen ARP requests:\n" + str(arp_req))
    logger.info("dev1 seen ARP replies:\n" + str(arp_reply))

    if ['ff:ff:ff:ff:ff:ff', addr2,
        addr2, '192.168.1.103',
        '00:00:00:00:00:00', '192.168.1.123'] in arp_req:
        raise Exception("dev1 saw ARP request from dev2")
    if [addr1, addr0, addr0, '192.168.1.123', addr1, '192.168.1.100'] not in arp_reply:
        raise Exception("dev1 did not get ARP response for 192.168.1.123")

    if ebtables:
        for req in arp_req:
            if req[1] != addr1:
                raise Exception("Unexpected foreign ARP request on dev1")

    arp_req = tshark_get_arp(cap_dev2, "arp.opcode == 1")
    arp_reply = tshark_get_arp(cap_dev2, "arp.opcode == 2")
    logger.info("dev2 seen ARP requests:\n" + str(arp_req))
    logger.info("dev2 seen ARP replies:\n" + str(arp_reply))

    if [addr2, addr0,
        addr0, '192.168.1.123',
        addr2, '192.168.1.103'] not in arp_reply:
        raise Exception("dev2 did not get ARP response for 192.168.1.123")

    arp_req = tshark_get_arp(cap_br, "arp.opcode == 1")
    arp_reply = tshark_get_arp(cap_br, "arp.opcode == 2")
    logger.info("br seen ARP requests:\n" + str(arp_req))
    logger.info("br seen ARP replies:\n" + str(arp_reply))

    # TODO: Uncomment once fixed in kernel
    #if [bssid, addr0,
    #    addr0, '192.168.1.123',
    #    bssid, '192.168.1.101'] not in arp_reply:
    #    raise Exception("br did not get ARP response for 192.168.1.123")

def _test_proxyarp_open_ipv6(dev, apdev, params, ebtables=False):
    cap_br = params['prefix'] + ".ap-br0.pcap"
    cap_dev0 = params['prefix'] + ".%s.pcap" % dev[0].ifname
    cap_dev1 = params['prefix'] + ".%s.pcap" % dev[1].ifname
    cap_dev2 = params['prefix'] + ".%s.pcap" % dev[2].ifname

    bssid = apdev[0]['bssid']
    params = {'ssid': 'open'}
    params['proxy_arp'] = '1'
    hapd = hostapd.add_ap(apdev[0], params, no_enable=True)
    hapd.set("ap_isolate", "1")
    hapd.set('bridge', 'ap-br0')
    hapd.dump_monitor()
    try:
        hapd.enable()
    except:
        # For now, do not report failures due to missing kernel support
        raise HwsimSkip("Could not start hostapd - assume proxyarp not supported in kernel version")
    ev = hapd.wait_event(["AP-ENABLED", "AP-DISABLED"], timeout=10)
    if ev is None:
        raise Exception("AP startup timed out")
    if "AP-ENABLED" not in ev:
        raise Exception("AP startup failed")

    params2 = {'ssid': 'another'}
    hapd2 = hostapd.add_ap(apdev[1], params2, no_enable=True)
    hapd2.set('bridge', 'ap-br0')
    hapd2.enable()

    subprocess.call(['brctl', 'setfd', 'ap-br0', '0'])
    subprocess.call(['ip', 'link', 'set', 'dev', 'ap-br0', 'up'])

    if ebtables:
        for chain in ['FORWARD', 'OUTPUT']:
            try:
                err = subprocess.call(['ebtables', '-A', chain,
                                       '-d', 'Multicast',
                                       '-p', 'IPv6',
                                       '--ip6-protocol', 'ipv6-icmp',
                                       '--ip6-icmp-type',
                                       'neighbor-solicitation',
                                       '-o', apdev[0]['ifname'], '-j', 'DROP'])
                if err != 0:
                    raise
                subprocess.call(['ebtables', '-A', chain, '-d', 'Multicast',
                                 '-p', 'IPv6', '--ip6-protocol', 'ipv6-icmp',
                                 '--ip6-icmp-type', 'neighbor-advertisement',
                                 '-o', apdev[0]['ifname'], '-j', 'DROP'])
                subprocess.call(['ebtables', '-A', chain,
                                 '-p', 'IPv6', '--ip6-protocol', 'ipv6-icmp',
                                 '--ip6-icmp-type', 'router-solicitation',
                                 '-o', apdev[0]['ifname'], '-j', 'DROP'])
                # Multicast Listener Report Message
                subprocess.call(['ebtables', '-A', chain, '-d', 'Multicast',
                                 '-p', 'IPv6', '--ip6-protocol', 'ipv6-icmp',
                                 '--ip6-icmp-type', '143',
                                 '-o', apdev[0]['ifname'], '-j', 'DROP'])
            except:
                raise HwsimSkip("No ebtables available")

    time.sleep(0.5)
    cmd = {}
    cmd[0] = WlantestCapture('ap-br0', cap_br)
    cmd[1] = WlantestCapture(dev[0].ifname, cap_dev0)
    cmd[2] = WlantestCapture(dev[1].ifname, cap_dev1)
    cmd[3] = WlantestCapture(dev[2].ifname, cap_dev2)

    dev[0].connect("open", key_mgmt="NONE", scan_freq="2412")
    dev[1].connect("open", key_mgmt="NONE", scan_freq="2412")
    dev[2].connect("another", key_mgmt="NONE", scan_freq="2412")
    time.sleep(0.1)

    brcmd = subprocess.Popen(['brctl', 'show'], stdout=subprocess.PIPE)
    res = brcmd.stdout.read().decode()
    brcmd.stdout.close()
    logger.info("Bridge setup: " + res)

    brcmd = subprocess.Popen(['brctl', 'showstp', 'ap-br0'],
                             stdout=subprocess.PIPE)
    res = brcmd.stdout.read().decode()
    brcmd.stdout.close()
    logger.info("Bridge showstp: " + res)

    addr0 = dev[0].p2p_interface_addr()
    addr1 = dev[1].p2p_interface_addr()
    addr2 = dev[2].p2p_interface_addr()

    src_ll_opt0 = b"\x01\x01" + binascii.unhexlify(addr0.replace(':', ''))
    src_ll_opt1 = b"\x01\x01" + binascii.unhexlify(addr1.replace(':', ''))

    # DAD NS
    send_ns(dev[0], ip_src="::", target="aaaa:bbbb:cccc::2")

    send_ns(dev[0], ip_src="aaaa:bbbb:cccc::2", target="aaaa:bbbb:cccc::2")
    # test frame without source link-layer address option
    send_ns(dev[0], ip_src="aaaa:bbbb:cccc::2", target="aaaa:bbbb:cccc::2",
            opt='')
    # test frame with bogus option
    send_ns(dev[0], ip_src="aaaa:bbbb:cccc::2", target="aaaa:bbbb:cccc::2",
            opt=b"\x70\x01\x01\x02\x03\x04\x05\x05")
    # test frame with truncated source link-layer address option
    send_ns(dev[0], ip_src="aaaa:bbbb:cccc::2", target="aaaa:bbbb:cccc::2",
            opt=b"\x01\x01\x01\x02\x03\x04")
    # test frame with foreign source link-layer address option
    send_ns(dev[0], ip_src="aaaa:bbbb:cccc::2", target="aaaa:bbbb:cccc::2",
            opt=b"\x01\x01\x01\x02\x03\x04\x05\x06")

    send_ns(dev[1], ip_src="aaaa:bbbb:dddd::2", target="aaaa:bbbb:dddd::2")

    send_ns(dev[1], ip_src="aaaa:bbbb:eeee::2", target="aaaa:bbbb:eeee::2")
    # another copy for additional code coverage
    send_ns(dev[1], ip_src="aaaa:bbbb:eeee::2", target="aaaa:bbbb:eeee::2")

    macs = get_bridge_macs("ap-br0")
    logger.info("After connect (showmacs): " + str(macs))

    matches = get_permanent_neighbors("ap-br0")
    logger.info("After connect: " + str(matches))
    if len(matches) != 3:
        raise Exception("Unexpected number of neighbor entries after connect")
    if 'aaaa:bbbb:cccc::2 dev ap-br0 lladdr 02:00:00:00:00:00 PERMANENT' not in matches:
        raise Exception("dev0 addr missing")
    if 'aaaa:bbbb:dddd::2 dev ap-br0 lladdr 02:00:00:00:01:00 PERMANENT' not in matches:
        raise Exception("dev1 addr(1) missing")
    if 'aaaa:bbbb:eeee::2 dev ap-br0 lladdr 02:00:00:00:01:00 PERMANENT' not in matches:
        raise Exception("dev1 addr(2) missing")

    send_ns(dev[0], target="aaaa:bbbb:dddd::2", ip_src="aaaa:bbbb:cccc::2")
    time.sleep(0.1)
    send_ns(dev[1], target="aaaa:bbbb:cccc::2", ip_src="aaaa:bbbb:dddd::2")
    time.sleep(0.1)
    send_ns(hapd, hapd_bssid=bssid, target="aaaa:bbbb:dddd::2",
            ip_src="aaaa:bbbb:ffff::2")
    time.sleep(0.1)
    send_ns(dev[2], target="aaaa:bbbb:cccc::2", ip_src="aaaa:bbbb:ff00::2")
    time.sleep(0.1)
    send_ns(dev[2], target="aaaa:bbbb:dddd::2", ip_src="aaaa:bbbb:ff00::2")
    time.sleep(0.1)
    send_ns(dev[2], target="aaaa:bbbb:eeee::2", ip_src="aaaa:bbbb:ff00::2")
    time.sleep(0.1)

    # Try to probe for an already assigned address
    send_ns(dev[1], target="aaaa:bbbb:cccc::2", ip_src="::")
    time.sleep(0.1)
    send_ns(hapd, hapd_bssid=bssid, target="aaaa:bbbb:cccc::2", ip_src="::")
    time.sleep(0.1)
    send_ns(dev[2], target="aaaa:bbbb:cccc::2", ip_src="::")
    time.sleep(0.1)

    # Unsolicited NA
    send_na(dev[1], target="aaaa:bbbb:cccc:aeae::3",
            ip_src="aaaa:bbbb:cccc:aeae::3", ip_dst="ff02::1")
    send_na(hapd, hapd_bssid=bssid, target="aaaa:bbbb:cccc:aeae::4",
            ip_src="aaaa:bbbb:cccc:aeae::4", ip_dst="ff02::1")
    send_na(dev[2], target="aaaa:bbbb:cccc:aeae::5",
            ip_src="aaaa:bbbb:cccc:aeae::5", ip_dst="ff02::1")

    try:
        hwsim_utils.test_connectivity_iface(dev[0], hapd, "ap-br0")
    except Exception as e:
        logger.info("test_connectibity_iface failed: " + str(e))
        raise HwsimSkip("Assume kernel did not have the required patches for proxyarp")
    hwsim_utils.test_connectivity_iface(dev[1], hapd, "ap-br0")
    hwsim_utils.test_connectivity(dev[0], dev[1])

    dev[0].request("DISCONNECT")
    dev[1].request("DISCONNECT")
    time.sleep(0.5)
    for i in range(len(cmd)):
        cmd[i].close()
    macs = get_bridge_macs("ap-br0")
    logger.info("After disconnect (showmacs): " + str(macs))
    matches = get_permanent_neighbors("ap-br0")
    logger.info("After disconnect: " + str(matches))
    if len(matches) > 0:
        raise Exception("Unexpected neighbor entries after disconnect")
    if ebtables:
        cmd = subprocess.Popen(['ebtables', '-L', '--Lc'],
                               stdout=subprocess.PIPE)
        res = cmd.stdout.read().decode()
        cmd.stdout.close()
        logger.info("ebtables results:\n" + res)

    ns = tshark_get_ns(cap_dev0)
    logger.info("dev0 seen NS: " + str(ns))
    na = tshark_get_na(cap_dev0)
    logger.info("dev0 seen NA: " + str(na))

    if [addr0, addr1, 'aaaa:bbbb:dddd::2', 'aaaa:bbbb:cccc::2',
        'aaaa:bbbb:dddd::2', addr1] not in na:
        # For now, skip the test instead of reporting the error since the IPv6
        # proxyarp support is not yet in the upstream kernel tree.
        #raise Exception("dev0 did not get NA for aaaa:bbbb:dddd::2")
        raise HwsimSkip("Assume kernel did not have the required patches for proxyarp (IPv6)")

    if ebtables:
        for req in ns:
            if req[1] == bssid and req[0] == "33:33:ff:" + bssid[9:] and \
               req[3] == 'ff02::1:ff00:300' and req[4] == 'fe80::ff:fe00:300':
                # At least for now, ignore this special case until the kernel
                # can be prevented from sending it out.
                logger.info("dev0: Ignore NS from AP to own local addr: " + str(req))
            elif req[1] != addr0:
                raise Exception("Unexpected foreign NS on dev0: " + str(req))

    ns = tshark_get_ns(cap_dev1)
    logger.info("dev1 seen NS: " + str(ns))
    na = tshark_get_na(cap_dev1)
    logger.info("dev1 seen NA: " + str(na))

    if [addr1, addr0, 'aaaa:bbbb:cccc::2', 'aaaa:bbbb:dddd::2',
        'aaaa:bbbb:cccc::2', addr0] not in na:
        raise Exception("dev1 did not get NA for aaaa:bbbb:cccc::2")

    if ebtables:
        for req in ns:
            if req[1] == bssid and req[0] == "33:33:ff:" + bssid[9:] and \
               req[3] == 'ff02::1:ff00:300' and req[4] == 'fe80::ff:fe00:300':
                # At least for now, ignore this special case until the kernel
                # can be prevented from sending it out.
                logger.info("dev1: Ignore NS from AP to own local addr: " + str(req))
            elif req[1] != addr1:
                raise Exception("Unexpected foreign NS on dev1: " + str(req))

    ns = tshark_get_ns(cap_dev2)
    logger.info("dev2 seen NS: " + str(ns))
    na = tshark_get_na(cap_dev2)
    logger.info("dev2 seen NA: " + str(na))

    # FIX: enable once kernel implementation for proxyarp IPv6 is fixed
    #if [addr2, addr0, 'aaaa:bbbb:cccc::2', 'aaaa:bbbb:ff00::2',
    #    'aaaa:bbbb:cccc::2', addr0] not in na:
    #    raise Exception("dev2 did not get NA for aaaa:bbbb:cccc::2")
    #if [addr2, addr1, 'aaaa:bbbb:dddd::2', 'aaaa:bbbb:ff00::2',
    #    'aaaa:bbbb:dddd::2', addr1] not in na:
    #    raise Exception("dev2 did not get NA for aaaa:bbbb:dddd::2")
    #if [addr2, addr1, 'aaaa:bbbb:eeee::2', 'aaaa:bbbb:ff00::2',
    #    'aaaa:bbbb:eeee::2', addr1] not in na:
    #    raise Exception("dev2 did not get NA for aaaa:bbbb:eeee::2")

def test_proxyarp_open(dev, apdev, params):
    """ProxyARP with open network"""
    try:
        _test_proxyarp_open(dev, apdev, params)
    finally:
        subprocess.call(['ip', 'link', 'set', 'dev', 'ap-br0', 'down'],
                        stderr=open('/dev/null', 'w'))
        subprocess.call(['brctl', 'delbr', 'ap-br0'],
                        stderr=open('/dev/null', 'w'))

def test_proxyarp_open_ipv6(dev, apdev, params):
    """ProxyARP with open network (IPv6)"""
    try:
        _test_proxyarp_open_ipv6(dev, apdev, params)
    finally:
        subprocess.call(['ip', 'link', 'set', 'dev', 'ap-br0', 'down'],
                        stderr=open('/dev/null', 'w'))
        subprocess.call(['brctl', 'delbr', 'ap-br0'],
                        stderr=open('/dev/null', 'w'))

def test_proxyarp_open_ebtables(dev, apdev, params):
    """ProxyARP with open network"""
    try:
        _test_proxyarp_open(dev, apdev, params, ebtables=True)
    finally:
        try:
            subprocess.call(['ebtables', '-F', 'FORWARD'])
            subprocess.call(['ebtables', '-F', 'OUTPUT'])
        except:
            pass
        subprocess.call(['ip', 'link', 'set', 'dev', 'ap-br0', 'down'],
                        stderr=open('/dev/null', 'w'))
        subprocess.call(['brctl', 'delbr', 'ap-br0'],
                        stderr=open('/dev/null', 'w'))

def test_proxyarp_open_ebtables_ipv6(dev, apdev, params):
    """ProxyARP with open network (IPv6)"""
    try:
        _test_proxyarp_open_ipv6(dev, apdev, params, ebtables=True)
    finally:
        try:
            subprocess.call(['ebtables', '-F', 'FORWARD'])
            subprocess.call(['ebtables', '-F', 'OUTPUT'])
        except:
            pass
        subprocess.call(['ip', 'link', 'set', 'dev', 'ap-br0', 'down'],
                        stderr=open('/dev/null', 'w'))
        subprocess.call(['brctl', 'delbr', 'ap-br0'],
                        stderr=open('/dev/null', 'w'))

def test_proxyarp_errors(dev, apdev, params):
    """ProxyARP error cases"""
    try:
        run_proxyarp_errors(dev, apdev, params)
    finally:
        subprocess.call(['ip', 'link', 'set', 'dev', 'ap-br0', 'down'],
                        stderr=open('/dev/null', 'w'))
        subprocess.call(['brctl', 'delbr', 'ap-br0'],
                        stderr=open('/dev/null', 'w'))

def run_proxyarp_errors(dev, apdev, params):
    params = {'ssid': 'open',
              'proxy_arp': '1',
              'ap_isolate': '1',
              'bridge': 'ap-br0',
              'disable_dgaf': '1'}
    hapd = hostapd.add_ap(apdev[0], params, no_enable=True)
    try:
        hapd.enable()
    except:
        # For now, do not report failures due to missing kernel support
        raise HwsimSkip("Could not start hostapd - assume proxyarp not supported in kernel version")
    ev = hapd.wait_event(["AP-ENABLED", "AP-DISABLED"], timeout=10)
    if ev is None:
        raise Exception("AP startup timed out")
    if "AP-ENABLED" not in ev:
        raise Exception("AP startup failed")

    hapd.disable()
    with alloc_fail(hapd, 1, "l2_packet_init;x_snoop_get_l2_packet;dhcp_snoop_init"):
        if "FAIL" not in hapd.request("ENABLE"):
            raise Exception("ENABLE accepted unexpectedly")
    with alloc_fail(hapd, 1, "l2_packet_init;x_snoop_get_l2_packet;ndisc_snoop_init"):
        if "FAIL" not in hapd.request("ENABLE"):
            raise Exception("ENABLE accepted unexpectedly")
    with fail_test(hapd, 1, "l2_packet_set_packet_filter;x_snoop_get_l2_packet;ndisc_snoop_init"):
        if "FAIL" not in hapd.request("ENABLE"):
            raise Exception("ENABLE accepted unexpectedly")
    with fail_test(hapd, 1, "l2_packet_set_packet_filter;x_snoop_get_l2_packet;dhcp_snoop_init"):
        if "FAIL" not in hapd.request("ENABLE"):
            raise Exception("ENABLE accepted unexpectedly")
    hapd.enable()

    subprocess.call(['brctl', 'setfd', 'ap-br0', '0'])
    subprocess.call(['ip', 'link', 'set', 'dev', 'ap-br0', 'up'])

    dev[0].connect("open", key_mgmt="NONE", scan_freq="2412")
    addr0 = dev[0].own_addr()

    pkt = build_ra(src_ll=apdev[0]['bssid'], ip_src="aaaa:bbbb:cccc::33",
                   ip_dst="ff01::1")
    with fail_test(hapd, 1, "x_snoop_mcast_to_ucast_convert_send"):
        if "OK" not in hapd.request("DATA_TEST_FRAME ifname=ap-br0 " + binascii.hexlify(pkt).decode()):
            raise Exception("DATA_TEST_FRAME failed")
        wait_fail_trigger(dev[0], "GET_FAIL")

    with alloc_fail(hapd, 1, "sta_ip6addr_add"):
        src_ll_opt0 = b"\x01\x01" + binascii.unhexlify(addr0.replace(':', ''))
        pkt = build_ns(src_ll=addr0, ip_src="aaaa:bbbb:cccc::2",
                       ip_dst="ff02::1:ff00:2", target="aaaa:bbbb:cccc::2",
                       opt=src_ll_opt0)
        if "OK" not in dev[0].request("DATA_TEST_FRAME " + binascii.hexlify(pkt).decode()):
            raise Exception("DATA_TEST_FRAME failed")
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")

def test_ap_hs20_connect_deinit(dev, apdev):
    """Hotspot 2.0 connection interrupted with deinit"""
    check_eap_capa(dev[0], "MSCHAPV2")
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid
    hapd = hostapd.add_ap(apdev[0], params)

    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    wpas.interface_add("wlan5", drv_params="")
    wpas.hs20_enable()
    wpas.flush_scan_cache()
    wpas.add_cred_values({'realm': "example.com",
                          'username': "hs20-test",
                          'password': "password",
                          'ca_cert': "auth_serv/ca.pem",
                          'domain': "example.com"})

    wpas.scan_for_bss(bssid, freq=2412)
    hapd.disable()

    wpas.request("INTERWORKING_SELECT freq=2412")

    id = wpas.request("RADIO_WORK add block-work")
    ev = wpas.wait_event(["GAS-QUERY-START", "EXT-RADIO-WORK-START"], timeout=5)
    if ev is None:
        raise Exception("Timeout while waiting radio work to start")
    ev = wpas.wait_event(["GAS-QUERY-START", "EXT-RADIO-WORK-START"], timeout=5)
    if ev is None:
        raise Exception("Timeout while waiting radio work to start (2)")

    # Remove the interface while the gas-query radio work is still pending and
    # GAS query has not yet been started.
    wpas.interface_remove("wlan5")

def test_ap_hs20_anqp_format_errors(dev, apdev):
    """Interworking network selection and ANQP format errors"""
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable()
    values = {'realm': "example.com",
              'ca_cert': "auth_serv/ca.pem",
              'username': "hs20-test",
              'password': "password",
              'domain': "example.com"}
    id = dev[0].add_cred_values(values)

    dev[0].scan_for_bss(bssid, freq="2412")

    tests = ["00", "ffff", "010011223344", "020008000005112233445500",
             "01000400000000", "01000000000000",
             "01000300000200", "0100040000ff0000", "01000300000100",
             "01000300000001",
             "01000600000056112233",
             "01000900000002050001000111",
             "01000600000001000000", "01000600000001ff0000",
             "01000600000001020001",
             "010008000000010400010001", "0100080000000104000100ff",
             "010011000000010d00050200020100030005000600",
             "0000"]
    for t in tests:
        hapd.set("anqp_elem", "263:" + t)
        dev[0].request("INTERWORKING_SELECT freq=2412")
        ev = dev[0].wait_event(["INTERWORKING-NO-MATCH"], timeout=5)
        if ev is None:
            raise Exception("Network selection timed out")
        dev[0].dump_monitor()

    dev[0].remove_cred(id)
    id = dev[0].add_cred_values({'imsi': "555444-333222111", 'eap': "AKA",
                                 'milenage': "5122250214c33e723a5dd523fc145fc0:981d464c7c52eb6e5036234984ad0bcf:000000000123"})

    tests = ["00", "0100", "0001", "00ff", "000200ff", "0003000101",
             "00020100"]
    for t in tests:
        hapd.set("anqp_elem", "264:" + t)
        dev[0].request("INTERWORKING_SELECT freq=2412")
        ev = dev[0].wait_event(["INTERWORKING-NO-MATCH"], timeout=5)
        if ev is None:
            raise Exception("Network selection timed out")
        dev[0].dump_monitor()

def test_ap_hs20_cred_with_nai_realm(dev, apdev):
    """Hotspot 2.0 network selection and cred_with_nai_realm cred->realm"""
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid
    hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable()

    id = dev[0].add_cred_values({'realm': "example.com",
                                 'username': "test",
                                 'password': "secret",
                                 'domain': "example.com",
                                 'eap': 'TTLS'})
    interworking_select(dev[0], bssid, "home", freq=2412)
    dev[0].remove_cred(id)

    id = dev[0].add_cred_values({'realm': "foo.com",
                                 'username': "test",
                                 'password': "secret",
                                 'domain': "example.com",
                                 'roaming_consortium': "112234",
                                 'eap': 'TTLS'})
    interworking_select(dev[0], bssid, "home", freq=2412, no_match=True)
    dev[0].remove_cred(id)

def test_ap_hs20_cred_and_no_roaming_consortium(dev, apdev):
    """Hotspot 2.0 network selection and no roaming consortium"""
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid
    del params['roaming_consortium']
    hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable()

    id = dev[0].add_cred_values({'realm': "example.com",
                                 'username': "test",
                                 'password': "secret",
                                 'domain': "example.com",
                                 'roaming_consortium': "112234",
                                 'eap': 'TTLS'})
    interworking_select(dev[0], bssid, "home", freq=2412)

def test_ap_hs20_interworking_oom(dev, apdev):
    """Hotspot 2.0 network selection and OOM"""
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid
    params['nai_realm'] = ["0,no.match.here;example.com;no.match.here.either,21[2:1][5:7]",
                           "0,example.com,13[5:6],21[2:4][5:7]",
                           "0,another.example.com"]
    hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable()

    id = dev[0].add_cred_values({'realm': "example.com",
                                 'username': "test",
                                 'password': "secret",
                                 'domain': "example.com",
                                 'eap': 'TTLS'})

    dev[0].scan_for_bss(bssid, freq="2412")

    funcs = ["wpabuf_alloc;interworking_anqp_send_req",
             "anqp_build_req;interworking_anqp_send_req",
             "gas_query_req;interworking_anqp_send_req",
             "dup_binstr;nai_realm_parse_realm",
             "=nai_realm_parse_realm",
             "=nai_realm_parse",
             "=nai_realm_match"]
    for func in funcs:
        with alloc_fail(dev[0], 1, func):
            dev[0].request("INTERWORKING_SELECT auto freq=2412")
            ev = dev[0].wait_event(["Starting ANQP"], timeout=5)
            if ev is None:
                raise Exception("ANQP did not start")
            wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
            dev[0].dump_monitor()

def test_ap_hs20_no_cred_connect(dev, apdev):
    """Hotspot 2.0 and connect attempt without credential"""
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable()
    dev[0].scan_for_bss(bssid, freq="2412")
    if "FAIL" not in dev[0].request("INTERWORKING_CONNECT " + bssid):
        raise Exception("Unexpected INTERWORKING_CONNECT success")

def test_ap_hs20_no_rsn_connect(dev, apdev):
    """Hotspot 2.0 and connect attempt without RSN"""
    bssid = apdev[0]['bssid']
    params = hostapd.wpa_params(ssid="test-hs20")
    params['wpa_key_mgmt'] = "WPA-EAP"
    params['ieee80211w'] = "1"
    params['ieee8021x'] = "1"
    params['auth_server_addr'] = "127.0.0.1"
    params['auth_server_port'] = "1812"
    params['auth_server_shared_secret'] = "radius"
    params['interworking'] = "1"
    params['roaming_consortium'] = ["112233", "1020304050", "010203040506",
                                    "fedcba"]
    params['nai_realm'] = ["0,example.com,13[5:6],21[2:4][5:7]",
                           "0,another.example.com"]
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable()
    dev[0].scan_for_bss(bssid, freq="2412")

    id = dev[0].add_cred_values({'realm': "example.com",
                                 'username': "test",
                                 'password': "secret",
                                 'domain': "example.com",
                                 'roaming_consortium': "112233",
                                 'eap': 'TTLS'})

    interworking_select(dev[0], bssid, freq=2412, no_match=True)
    if "FAIL" not in dev[0].request("INTERWORKING_CONNECT " + bssid):
        raise Exception("Unexpected INTERWORKING_CONNECT success")

def test_ap_hs20_no_match_connect(dev, apdev):
    """Hotspot 2.0 and connect attempt without matching cred"""
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable()
    dev[0].scan_for_bss(bssid, freq="2412")

    id = dev[0].add_cred_values({'realm': "example.org",
                                 'username': "test",
                                 'password': "secret",
                                 'domain': "example.org",
                                 'roaming_consortium': "112234",
                                 'eap': 'TTLS'})

    interworking_select(dev[0], bssid, freq=2412, no_match=True)
    if "FAIL" not in dev[0].request("INTERWORKING_CONNECT " + bssid):
        raise Exception("Unexpected INTERWORKING_CONNECT success")

def test_ap_hs20_multiple_home_cred(dev, apdev):
    """Hotspot 2.0 and select with multiple matching home credentials"""
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid
    params['nai_realm'] = ["0,example.com,13[5:6],21[2:4][5:7]"]
    params['domain_name'] = "example.com"
    hapd = hostapd.add_ap(apdev[0], params)

    bssid2 = apdev[1]['bssid']
    params = hs20_ap_params(ssid="test-hs20-other")
    params['hessid'] = bssid2
    params['nai_realm'] = ["0,example.org,13[5:6],21[2:4][5:7]"]
    params['domain_name'] = "example.org"
    hapd2 = hostapd.add_ap(apdev[1], params)

    dev[0].hs20_enable()
    dev[0].scan_for_bss(bssid2, freq="2412")
    dev[0].scan_for_bss(bssid, freq="2412")
    id = dev[0].add_cred_values({'realm': "example.com",
                                 'priority': '2',
                                 'username': "hs20-test",
                                 'password': "password",
                                 'domain': "example.com"})
    id2 = dev[0].add_cred_values({'realm': "example.org",
                                  'priority': '3',
                                  'username': "hs20-test",
                                  'password': "password",
                                  'domain': "example.org"})
    dev[0].request("INTERWORKING_SELECT auto freq=2412")
    ev = dev[0].wait_connected(timeout=15)
    if bssid2 not in ev:
        raise Exception("Connected to incorrect network")

def test_ap_hs20_anqp_invalid_gas_response(dev, apdev):
    """Hotspot 2.0 network selection and invalid GAS response"""
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].scan_for_bss(bssid, freq="2412")
    hapd.set("ext_mgmt_frame_handling", "1")

    dev[0].hs20_enable()

    id = dev[0].add_cred_values({'realm': "example.com",
                                 'username': "test",
                                 'password': "secret",
                                 'domain': "example.com",
                                 'roaming_consortium': "112234",
                                 'eap': 'TTLS'})
    dev[0].request("INTERWORKING_SELECT freq=2412")

    query = gas_rx(hapd)
    gas = parse_gas(query['payload'])

    logger.info("ANQP: Unexpected Advertisement Protocol in response")
    resp = action_response(query)
    adv_proto = struct.pack('8B', 108, 6, 127, 0xdd, 0x00, 0x11, 0x22, 0x33)
    data = struct.pack('<H', 0)
    resp['payload'] = struct.pack('<BBBHH', ACTION_CATEG_PUBLIC,
                                  GAS_INITIAL_RESPONSE,
                                  gas['dialog_token'], 0, 0) + adv_proto + data
    send_gas_resp(hapd, resp)

    ev = dev[0].wait_event(["ANQP-QUERY-DONE"], timeout=5)
    if ev is None:
        raise Exception("No ANQP-QUERY-DONE seen")
    if "result=INVALID_FRAME" not in ev:
        raise Exception("Unexpected result: " + ev)

    dev[0].request("INTERWORKING_SELECT freq=2412")

    query = gas_rx(hapd)
    gas = parse_gas(query['payload'])

    logger.info("ANQP: Invalid element length for Info ID 1234")
    resp = action_response(query)
    adv_proto = struct.pack('BBBB', 108, 2, 127, 0)
    elements = struct.pack('<HH', 1234, 1)
    data = struct.pack('<H', len(elements)) + elements
    resp['payload'] = struct.pack('<BBBHH', ACTION_CATEG_PUBLIC,
                                  GAS_INITIAL_RESPONSE,
                                  gas['dialog_token'], 0, 0) + adv_proto + data
    send_gas_resp(hapd, resp)

    ev = dev[0].wait_event(["ANQP-QUERY-DONE"], timeout=5)
    if ev is None:
        raise Exception("No ANQP-QUERY-DONE seen")
    if "result=INVALID_FRAME" not in ev:
        raise Exception("Unexpected result: " + ev)

    with alloc_fail(dev[0], 1, "=anqp_add_extra"):
        dev[0].request("INTERWORKING_SELECT freq=2412")

        query = gas_rx(hapd)
        gas = parse_gas(query['payload'])

        resp = action_response(query)
        elements = struct.pack('<HHHH', 1, 0, 1, 0)
        data = struct.pack('<H', len(elements)) + elements
        resp['payload'] = struct.pack('<BBBHH', ACTION_CATEG_PUBLIC,
                                      GAS_INITIAL_RESPONSE,
                                      gas['dialog_token'], 0, 0) + adv_proto + data
        send_gas_resp(hapd, resp)

        ev = dev[0].wait_event(["ANQP-QUERY-DONE"], timeout=5)
        if ev is None:
            raise Exception("No ANQP-QUERY-DONE seen")
        if "result=SUCCESS" not in ev:
            raise Exception("Unexpected result: " + ev)

    with alloc_fail(dev[0], 1, "wpabuf_alloc_copy;anqp_add_extra"):
        dev[0].request("INTERWORKING_SELECT freq=2412")

        query = gas_rx(hapd)
        gas = parse_gas(query['payload'])

        resp = action_response(query)
        elements = struct.pack('<HHHH', 1, 0, 1, 0)
        data = struct.pack('<H', len(elements)) + elements
        resp['payload'] = struct.pack('<BBBHH', ACTION_CATEG_PUBLIC,
                                      GAS_INITIAL_RESPONSE,
                                      gas['dialog_token'], 0, 0) + adv_proto + data
        send_gas_resp(hapd, resp)

        ev = dev[0].wait_event(["ANQP-QUERY-DONE"], timeout=5)
        if ev is None:
            raise Exception("No ANQP-QUERY-DONE seen")
        if "result=SUCCESS" not in ev:
            raise Exception("Unexpected result: " + ev)

    tests = [struct.pack('<HH', 0xdddd, 0),
             struct.pack('<HH3B', 0xdddd, 3, 0x50, 0x6f, 0x9a),
             struct.pack('<HH4B', 0xdddd, 4, 0x50, 0x6f, 0x9a, 0),
             struct.pack('<HH4B', 0xdddd, 4, 0x11, 0x22, 0x33, 0),
             struct.pack('<HHHH', 1, 0, 1, 0)]
    for elements in tests:
        dev[0].request("INTERWORKING_SELECT freq=2412")

        query = gas_rx(hapd)
        gas = parse_gas(query['payload'])

        resp = action_response(query)
        data = struct.pack('<H', len(elements)) + elements
        resp['payload'] = struct.pack('<BBBHH', ACTION_CATEG_PUBLIC,
                                      GAS_INITIAL_RESPONSE,
                                      gas['dialog_token'], 0, 0) + adv_proto + data
        send_gas_resp(hapd, resp)

        ev = dev[0].wait_event(["ANQP-QUERY-DONE"], timeout=5)
        if ev is None:
            raise Exception("No ANQP-QUERY-DONE seen")
        if "result=SUCCESS" not in ev:
            raise Exception("Unexpected result: " + ev)

def test_ap_hs20_set_profile_failures(dev, apdev):
    """Hotspot 2.0 and failures during profile configuration"""
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid
    params['anqp_3gpp_cell_net'] = "555,444"
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable()
    dev[0].scan_for_bss(bssid, freq="2412")

    id = dev[0].add_cred_values({'realm': "example.com",
                                 'domain': "example.com",
                                 'username': "test",
                                 'password': "secret",
                                 'eap': 'TTLS'})
    interworking_select(dev[0], bssid, "home", freq=2412)
    dev[0].dump_monitor()
    dev[0].request("NOTE ssid->eap.eap_methods = os_malloc()")
    with alloc_fail(dev[0], 1, "interworking_set_eap_params"):
        dev[0].request("INTERWORKING_CONNECT " + bssid)
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
    dev[0].remove_cred(id)

    id = dev[0].add_cred_values({'realm': "example.com",
                                 'domain': "example.com",
                                 'username': "hs20-test-with-domain@example.com",
                                 'password': "password"})
    interworking_select(dev[0], bssid, "home", freq=2412)
    dev[0].dump_monitor()
    dev[0].request("NOTE anon = os_malloc()")
    with alloc_fail(dev[0], 1, "interworking_set_eap_params"):
        dev[0].request("INTERWORKING_CONNECT " + bssid)
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
    dev[0].request("NOTE Successful connection with cred->username including realm")
    dev[0].request("INTERWORKING_CONNECT " + bssid)
    dev[0].wait_connected()
    dev[0].remove_cred(id)
    dev[0].wait_disconnected()

    id = dev[0].add_cred_values({'realm': "example.com",
                                 'domain': "example.com",
                                 'username': "hs20-test",
                                 'password': "password"})
    interworking_select(dev[0], bssid, "home", freq=2412)
    dev[0].dump_monitor()
    dev[0].request("NOTE anon = os_malloc() (second)")
    with alloc_fail(dev[0], 1, "interworking_set_eap_params"):
        dev[0].request("INTERWORKING_CONNECT " + bssid)
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
    with alloc_fail(dev[0], 1, "wpa_config_add_network;interworking_connect"):
        dev[0].request("INTERWORKING_CONNECT " + bssid)
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
    with alloc_fail(dev[0], 1, "=interworking_connect"):
        dev[0].request("INTERWORKING_CONNECT " + bssid)
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
    dev[0].request("NOTE wpa_config_set(eap)")
    with alloc_fail(dev[0], 1, "wpa_config_parse_eap;wpa_config_set;interworking_connect"):
        dev[0].request("INTERWORKING_CONNECT " + bssid)
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
    dev[0].request("NOTE wpa_config_set(TTLS-NON_EAP_MSCHAPV2-phase2)")
    with alloc_fail(dev[0], 1, "wpa_config_parse_str;wpa_config_set;interworking_connect"):
        dev[0].request("INTERWORKING_CONNECT " + bssid)
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
    dev[0].remove_cred(id)

    id = dev[0].add_cred_values({'roaming_consortium': "112233",
                                 'domain': "example.com",
                                 'username': "hs20-test",
                                 'password': "password",
                                 'eap': 'TTLS',
                                 'phase2': "auth=MSCHAPV2"})
    interworking_select(dev[0], bssid, "home", freq=2412)
    dev[0].dump_monitor()
    dev[0].request("NOTE anon = os_strdup()")
    with alloc_fail(dev[0], 2, "interworking_set_eap_params"):
        dev[0].request("INTERWORKING_CONNECT " + bssid)
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
    dev[0].request("NOTE wpa_config_set_quoted(anonymous_identity)")
    with alloc_fail(dev[0], 1, "=wpa_config_set_quoted;interworking_set_eap_params"):
        dev[0].request("INTERWORKING_CONNECT " + bssid)
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
    dev[0].request("NOTE Successful connection with cred->realm not included")
    dev[0].request("INTERWORKING_CONNECT " + bssid)
    dev[0].wait_connected()
    dev[0].remove_cred(id)
    dev[0].wait_disconnected()

    id = dev[0].add_cred_values({'roaming_consortium': "112233",
                                 'domain': "example.com",
                                 'realm': "example.com",
                                 'username': "user",
                                 'password': "password",
                                 'eap': 'PEAP'})
    interworking_select(dev[0], bssid, "home", freq=2412)
    dev[0].dump_monitor()
    dev[0].request("NOTE id = os_strdup()")
    with alloc_fail(dev[0], 2, "interworking_set_eap_params"):
        dev[0].request("INTERWORKING_CONNECT " + bssid)
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
    dev[0].request("NOTE wpa_config_set_quoted(identity)")
    with alloc_fail(dev[0], 1, "=wpa_config_set_quoted;interworking_set_eap_params"):
        dev[0].request("INTERWORKING_CONNECT " + bssid)
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
    dev[0].remove_cred(id)

    id = dev[0].add_cred_values({'roaming_consortium': "112233",
                                 'domain': "example.com",
                                 'realm': "example.com",
                                 'username': "user",
                                 'password': "password",
                                 'eap': "TTLS"})
    interworking_select(dev[0], bssid, "home", freq=2412)
    dev[0].dump_monitor()
    dev[0].request("NOTE wpa_config_set_quoted(identity) (second)")
    with alloc_fail(dev[0], 2, "=wpa_config_set_quoted;interworking_set_eap_params"):
        dev[0].request("INTERWORKING_CONNECT " + bssid)
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
    dev[0].request("NOTE wpa_config_set_quoted(password)")
    with alloc_fail(dev[0], 3, "=wpa_config_set_quoted;interworking_set_eap_params"):
        dev[0].request("INTERWORKING_CONNECT " + bssid)
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
    with alloc_fail(dev[0], 1, "wpa_config_add_network;interworking_connect_roaming_consortium"):
        dev[0].request("INTERWORKING_CONNECT " + bssid)
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
    with alloc_fail(dev[0], 1, "=interworking_connect_roaming_consortium"):
        dev[0].request("INTERWORKING_CONNECT " + bssid)
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
    dev[0].remove_cred(id)

    id = dev[0].add_cred_values({'roaming_consortium': "112233",
                                 'domain': "example.com",
                                 'realm': "example.com",
                                 'username': "user",
                                 'eap': "PEAP"})
    dev[0].set_cred(id, "password", "ext:password")
    interworking_select(dev[0], bssid, "home", freq=2412)
    dev[0].dump_monitor()
    dev[0].request("NOTE wpa_config_set(password)")
    with alloc_fail(dev[0], 3, "wpa_config_set;interworking_set_eap_params"):
        dev[0].request("INTERWORKING_CONNECT " + bssid)
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
    with alloc_fail(dev[0], 1, "interworking_set_hs20_params"):
        dev[0].request("INTERWORKING_CONNECT " + bssid)
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
    dev[0].remove_cred(id)

    id = dev[0].add_cred_values({'realm': "example.com",
                                 'domain': "example.com",
                                 'username': "certificate-user",
                                 'phase1': "include_tls_length=0",
                                 'domain_suffix_match': "example.com",
                                 'ca_cert': "auth_serv/ca.pem",
                                 'client_cert': "auth_serv/user.pem",
                                 'private_key': "auth_serv/user.key",
                                 'private_key_passwd': "secret"})
    interworking_select(dev[0], bssid, "home", freq=2412)
    dev[0].dump_monitor()
    dev[0].request("NOTE wpa_config_set_quoted(client_cert)")
    with alloc_fail(dev[0], 2, "=wpa_config_set_quoted;interworking_set_eap_params"):
        dev[0].request("INTERWORKING_CONNECT " + bssid)
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
    dev[0].request("NOTE wpa_config_set_quoted(private_key)")
    with alloc_fail(dev[0], 3, "=wpa_config_set_quoted;interworking_set_eap_params"):
        dev[0].request("INTERWORKING_CONNECT " + bssid)
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
    dev[0].request("NOTE wpa_config_set_quoted(private_key_passwd)")
    with alloc_fail(dev[0], 4, "=wpa_config_set_quoted;interworking_set_eap_params"):
        dev[0].request("INTERWORKING_CONNECT " + bssid)
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
    dev[0].request("NOTE wpa_config_set_quoted(ca_cert)")
    with alloc_fail(dev[0], 5, "=wpa_config_set_quoted;interworking_set_eap_params"):
        dev[0].request("INTERWORKING_CONNECT " + bssid)
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
    dev[0].request("NOTE wpa_config_set_quoted(domain_suffix_match)")
    with alloc_fail(dev[0], 6, "=wpa_config_set_quoted;interworking_set_eap_params"):
        dev[0].request("INTERWORKING_CONNECT " + bssid)
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
    with alloc_fail(dev[0], 1, "interworking_set_hs20_params"):
        dev[0].request("INTERWORKING_CONNECT " + bssid)
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
    dev[0].remove_cred(id)

    id = dev[0].add_cred_values({'imsi': "555444-333222111", 'eap': "SIM",
                                 'milenage': "5122250214c33e723a5dd523fc145fc0:981d464c7c52eb6e5036234984ad0bcf:000000000123"})
    interworking_select(dev[0], bssid, freq=2412)
    dev[0].dump_monitor()
    with alloc_fail(dev[0], 1, "interworking_set_hs20_params"):
        dev[0].request("INTERWORKING_CONNECT " + bssid)
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
    dev[0].request("NOTE wpa_config_set_quoted(password;milenage)")
    with alloc_fail(dev[0], 2, "=wpa_config_set_quoted;interworking_connect_3gpp"):
        dev[0].request("INTERWORKING_CONNECT " + bssid)
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
    dev[0].request("NOTE wpa_config_set(eap)")
    with alloc_fail(dev[0], 1, "wpa_config_parse_eap;wpa_config_set;interworking_connect_3gpp"):
        dev[0].request("INTERWORKING_CONNECT " + bssid)
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
    dev[0].request("NOTE set_root_nai:wpa_config_set(identity)")
    with alloc_fail(dev[0], 1, "wpa_config_parse_str;interworking_connect_3gpp"):
            dev[0].request("INTERWORKING_CONNECT " + bssid)
            wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
    dev[0].remove_cred(id)

    id = dev[0].add_cred_values({'roaming_consortium': "112233",
                                 'eap': 'TTLS',
                                 'username': "user@example.com",
                                 'password': "password"})
    interworking_select(dev[0], bssid, freq=2412)
    dev[0].dump_monitor()
    dev[0].request("NOTE Interworking: No EAP method set for credential using roaming consortium")
    dev[0].request("INTERWORKING_CONNECT " + bssid)
    dev[0].remove_cred(id)

    hapd.disable()
    params = hs20_ap_params()
    params['nai_realm'] = "0,example.com,25[3:26]"
    hapd = hostapd.add_ap(apdev[0], params)
    id = dev[0].add_cred_values({'realm': "example.com",
                                 'domain': "example.com",
                                 'username': "hs20-test",
                                 'password': "password"})
    interworking_select(dev[0], bssid, freq=2412)
    dev[0].dump_monitor()
    dev[0].request("NOTE wpa_config_set(PEAP/FAST-phase1)")
    with alloc_fail(dev[0], 1, "wpa_config_parse_str;wpa_config_set;interworking_connect"):
        dev[0].request("INTERWORKING_CONNECT " + bssid)
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
    dev[0].request("NOTE wpa_config_set(PEAP/FAST-pac_interworking)")
    with alloc_fail(dev[0], 2, "wpa_config_parse_str;wpa_config_set;interworking_connect"):
        dev[0].request("INTERWORKING_CONNECT " + bssid)
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
    dev[0].request("NOTE wpa_config_set(PEAP/FAST-phase2)")
    with alloc_fail(dev[0], 3, "wpa_config_parse_str;wpa_config_set;interworking_connect"):
        dev[0].request("INTERWORKING_CONNECT " + bssid)
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")

    hapd.disable()
    params = hs20_ap_params()
    params['nai_realm'] = "0,example.com,21"
    hapd = hostapd.add_ap(apdev[0], params)
    interworking_select(dev[0], bssid, freq=2412)
    dev[0].request("NOTE wpa_config_set(TTLS-defaults-phase2)")
    with alloc_fail(dev[0], 1, "wpa_config_parse_str;wpa_config_set;interworking_connect"):
        dev[0].request("INTERWORKING_CONNECT " + bssid)
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")

    hapd.disable()
    params = hs20_ap_params()
    params['nai_realm'] = "0,example.com,21[2:3]"
    hapd = hostapd.add_ap(apdev[0], params)
    interworking_select(dev[0], bssid, freq=2412)
    dev[0].request("NOTE wpa_config_set(TTLS-NON_EAP_MSCHAP-phase2)")
    with alloc_fail(dev[0], 1, "wpa_config_parse_str;wpa_config_set;interworking_connect"):
        dev[0].request("INTERWORKING_CONNECT " + bssid)
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")

    hapd.disable()
    params = hs20_ap_params()
    params['nai_realm'] = "0,example.com,21[2:2]"
    hapd = hostapd.add_ap(apdev[0], params)
    interworking_select(dev[0], bssid, freq=2412)
    dev[0].request("NOTE wpa_config_set(TTLS-NON_EAP_CHAP-phase2)")
    with alloc_fail(dev[0], 1, "wpa_config_parse_str;wpa_config_set;interworking_connect"):
        dev[0].request("INTERWORKING_CONNECT " + bssid)
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")

    hapd.disable()
    params = hs20_ap_params()
    params['nai_realm'] = "0,example.com,21[2:1]"
    hapd = hostapd.add_ap(apdev[0], params)
    interworking_select(dev[0], bssid, freq=2412)
    dev[0].request("NOTE wpa_config_set(TTLS-NON_EAP_PAP-phase2)")
    with alloc_fail(dev[0], 1, "wpa_config_parse_str;wpa_config_set;interworking_connect"):
        dev[0].request("INTERWORKING_CONNECT " + bssid)
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")

    hapd.disable()
    params = hs20_ap_params()
    params['nai_realm'] = "0,example.com,21[3:26]"
    hapd = hostapd.add_ap(apdev[0], params)
    interworking_select(dev[0], bssid, freq=2412)
    dev[0].request("NOTE wpa_config_set(TTLS-EAP-MSCHAPV2-phase2)")
    with alloc_fail(dev[0], 1, "wpa_config_parse_str;wpa_config_set;interworking_connect"):
        dev[0].request("INTERWORKING_CONNECT " + bssid)
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")

    dev[0].remove_cred(id)

def test_ap_hs20_unexpected(dev, apdev):
    """Unexpected Hotspot 2.0 AP configuration"""
    skip_without_tkip(dev[0])
    skip_without_tkip(dev[1])
    skip_without_tkip(dev[2])
    check_eap_capa(dev[0], "MSCHAPV2")
    bssid = apdev[0]['bssid']
    params = hostapd.wpa_eap_params(ssid="test-hs20-fake")
    params['wpa'] = "3"
    params['wpa_pairwise'] = "TKIP CCMP"
    params['rsn_pairwise'] = "CCMP"
    params['ieee80211w'] = "1"
    #params['vendor_elements'] = 'dd07506f9a10140000'
    params['vendor_elements'] = 'dd04506f9a10'
    hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable()
    dev[0].scan_for_bss(bssid, freq="2412")
    dev[0].connect("test-hs20-fake", key_mgmt="WPA-EAP", eap="TTLS",
                   pairwise="TKIP",
                   identity="hs20-test", password="password",
                   ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2",
                   scan_freq="2412")

    dev[1].hs20_enable()
    dev[1].scan_for_bss(bssid, freq="2412")
    dev[1].connect("test-hs20-fake", key_mgmt="WPA-EAP", eap="TTLS",
                   proto="WPA",
                   identity="hs20-test", password="password",
                   ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2",
                   scan_freq="2412")

    dev[2].hs20_enable()
    dev[2].scan_for_bss(bssid, freq="2412")
    dev[2].connect("test-hs20-fake", key_mgmt="WPA-EAP", eap="TTLS",
                   ieee80211w="1",
                   proto="RSN", pairwise="CCMP",
                   identity="hs20-test", password="password",
                   ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2",
                   scan_freq="2412")

def test_ap_interworking_element_update(dev, apdev):
    """Dynamic Interworking element update"""
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable()
    dev[0].scan_for_bss(bssid, freq="2412")
    bss = dev[0].get_bss(bssid)
    logger.info("Before update: " + str(bss))
    if '6b091e0701020000000300' not in bss['ie']:
        raise Exception("Expected Interworking element not seen before update")

    # Update configuration parameters related to Interworking element
    hapd.set('access_network_type', '2')
    hapd.set('asra', '1')
    hapd.set('esr', '1')
    hapd.set('uesa', '1')
    hapd.set('venue_group', '2')
    hapd.set('venue_type', '8')
    if "OK" not in hapd.request("UPDATE_BEACON"):
        raise Exception("UPDATE_BEACON failed")
    dev[0].request("BSS_FLUSH 0")
    dev[0].scan_for_bss(bssid, freq="2412", force_scan=True)
    bss = dev[0].get_bss(bssid)
    logger.info("After update: " + str(bss))
    if '6b09f20208020000000300' not in bss['ie']:
        raise Exception("Expected Interworking element not seen after update")

def test_ap_hs20_terms_and_conditions(dev, apdev):
    """Hotspot 2.0 Terms and Conditions signaling"""
    check_eap_capa(dev[0], "MSCHAPV2")
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid
    params['hs20_t_c_filename'] = 'terms-and-conditions'
    params['hs20_t_c_timestamp'] = '123456789'

    hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable()
    dev[0].connect("test-hs20", proto="RSN", key_mgmt="WPA-EAP", eap="TTLS",
                   identity="hs20-t-c-test", password="password",
                   ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2",
                   ieee80211w='2', scan_freq="2412")
    ev = dev[0].wait_event(["HS20-T-C-ACCEPTANCE"], timeout=5)
    if ev is None:
        raise Exception("Terms and Conditions Acceptance notification not received")
    url = "https://example.com/t_and_c?addr=%s&ap=123" % dev[0].own_addr()
    if url not in ev:
        raise Exception("Unexpected URL: " + ev)

def test_ap_hs20_terms_and_conditions_coa(dev, apdev):
    """Hotspot 2.0 Terms and Conditions signaling - CoA"""
    try:
        import pyrad.client
        import pyrad.packet
        import pyrad.dictionary
        import radius_das
    except ImportError:
        raise HwsimSkip("No pyrad modules available")

    check_eap_capa(dev[0], "MSCHAPV2")
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid
    params['hs20_t_c_filename'] = 'terms-and-conditions'
    params['hs20_t_c_timestamp'] = '123456789'
    params['own_ip_addr'] = "127.0.0.1"
    params['radius_das_port'] = "3799"
    params['radius_das_client'] = "127.0.0.1 secret"
    params['radius_das_require_event_timestamp'] = "1"
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable()
    dev[0].connect("test-hs20", proto="RSN", key_mgmt="WPA-EAP", eap="TTLS",
                   identity="hs20-t-c-test", password="password",
                   ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2",
                   ieee80211w='2', scan_freq="2412")

    ev = hapd.wait_event(["HS20-T-C-FILTERING-ADD"], timeout=5)
    if ev is None:
        raise Exception("Terms and Conditions filtering not enabled")
    if ev.split(' ')[1] != dev[0].own_addr():
        raise Exception("Unexpected STA address for filtering: " + ev)

    ev = dev[0].wait_event(["HS20-T-C-ACCEPTANCE"], timeout=5)
    if ev is None:
        raise Exception("Terms and Conditions Acceptance notification not received")
    url = "https://example.com/t_and_c?addr=%s&ap=123" % dev[0].own_addr()
    if url not in ev:
        raise Exception("Unexpected URL: " + ev)

    dict = pyrad.dictionary.Dictionary("dictionary.radius")

    srv = pyrad.client.Client(server="127.0.0.1", acctport=3799,
                              secret=b"secret", dict=dict)
    srv.retries = 1
    srv.timeout = 1

    sta = hapd.get_sta(dev[0].own_addr())
    multi_sess_id = sta['authMultiSessionId']

    logger.info("CoA-Request with matching Acct-Session-Id")
    vsa = binascii.unhexlify('00009f68090600000000')
    req = radius_das.CoAPacket(dict=dict, secret=b"secret",
                               NAS_IP_Address="127.0.0.1",
                               Acct_Multi_Session_Id=multi_sess_id,
                               Chargeable_User_Identity="hs20-cui",
                               Event_Timestamp=int(time.time()),
                               Vendor_Specific=vsa)
    reply = srv.SendPacket(req)
    logger.debug("RADIUS response from hostapd")
    for i in list(reply.keys()):
        logger.debug("%s: %s" % (i, reply[i]))
    if reply.code != pyrad.packet.CoAACK:
        raise Exception("CoA-Request failed")

    ev = hapd.wait_event(["HS20-T-C-FILTERING-REMOVE"], timeout=5)
    if ev is None:
        raise Exception("Terms and Conditions filtering not disabled")
    if ev.split(' ')[1] != dev[0].own_addr():
        raise Exception("Unexpected STA address for filtering: " + ev)

def test_ap_hs20_terms_and_conditions_sql(dev, apdev, params):
    """Hotspot 2.0 Terms and Conditions using SQLite for user DB"""
    addr = dev[0].own_addr()
    run_ap_hs20_terms_and_conditions_sql(dev, apdev, params,
                                         "https://example.com/t_and_c?addr=@1@&ap=123",
                                         "https://example.com/t_and_c?addr=" + addr + "&ap=123")

def test_ap_hs20_terms_and_conditions_sql2(dev, apdev, params):
    """Hotspot 2.0 Terms and Conditions using SQLite for user DB"""
    addr = dev[0].own_addr()
    run_ap_hs20_terms_and_conditions_sql(dev, apdev, params,
                                         "https://example.com/t_and_c?addr=@1@",
                                         "https://example.com/t_and_c?addr=" + addr)

def run_ap_hs20_terms_and_conditions_sql(dev, apdev, params, url_template,
                                         url_expected):
    check_eap_capa(dev[0], "MSCHAPV2")
    try:
        import sqlite3
    except ImportError:
        raise HwsimSkip("No sqlite3 module available")
    dbfile = params['prefix'] + ".eap-user.db"
    try:
        os.remove(dbfile)
    except:
        pass
    con = sqlite3.connect(dbfile)
    with con:
        cur = con.cursor()
        cur.execute("CREATE TABLE users(identity TEXT PRIMARY KEY, methods TEXT, password TEXT, remediation TEXT, phase2 INTEGER, t_c_timestamp INTEGER)")
        cur.execute("CREATE TABLE wildcards(identity TEXT PRIMARY KEY, methods TEXT)")
        cur.execute("INSERT INTO users(identity,methods,password,phase2) VALUES ('user-mschapv2','TTLS-MSCHAPV2','password',1)")
        cur.execute("INSERT INTO wildcards(identity,methods) VALUES ('','TTLS,TLS')")
        cur.execute("CREATE TABLE authlog(timestamp TEXT, session TEXT, nas_ip TEXT, username TEXT, note TEXT)")
        cur.execute("CREATE TABLE pending_tc(mac_addr TEXT PRIMARY KEY, identity TEXT)")
        cur.execute("CREATE TABLE current_sessions(mac_addr TEXT PRIMARY KEY, identity TEXT, start_time TEXT, nas TEXT, hs20_t_c_filtering BOOLEAN, waiting_coa_ack BOOLEAN, coa_ack_received BOOLEAN)")


    try:
        params = {"ssid": "as", "beacon_int": "2000",
                  "radius_server_clients": "auth_serv/radius_clients.conf",
                  "radius_server_auth_port": '18128',
                  "eap_server": "1",
                  "eap_user_file": "sqlite:" + dbfile,
                  "ca_cert": "auth_serv/ca.pem",
                  "server_cert": "auth_serv/server.pem",
                  "private_key": "auth_serv/server.key"}
        params['hs20_t_c_server_url'] = url_template
        authsrv = hostapd.add_ap(apdev[1], params)

        bssid = apdev[0]['bssid']
        params = hs20_ap_params()
        params['auth_server_port'] = "18128"
        params['hs20_t_c_filename'] = 'terms-and-conditions'
        params['hs20_t_c_timestamp'] = '123456789'
        params['own_ip_addr'] = "127.0.0.1"
        params['radius_das_port'] = "3799"
        params['radius_das_client'] = "127.0.0.1 radius"
        params['radius_das_require_event_timestamp'] = "1"
        params['disable_pmksa_caching'] = '1'
        hapd = hostapd.add_ap(apdev[0], params)

        dev[0].request("SET pmf 1")
        dev[0].hs20_enable()
        id = dev[0].add_cred_values({'realm': "example.com",
                                     'username': "user-mschapv2",
                                     'password': "password",
                                     'ca_cert': "auth_serv/ca.pem"})
        interworking_select(dev[0], bssid, freq="2412")
        interworking_connect(dev[0], bssid, "TTLS")

        ev = hapd.wait_event(["HS20-T-C-FILTERING-ADD"], timeout=5)
        if ev is None:
            raise Exception("Terms and Conditions filtering not enabled")
        hapd.dump_monitor()

        ev = dev[0].wait_event(["HS20-T-C-ACCEPTANCE"], timeout=5)
        if ev is None:
            raise Exception("Terms and Conditions Acceptance notification not received")
        url = ev.split(' ')[1]
        if url != url_expected:
            raise Exception("Unexpected URL delivered to the client: %s (expected %s)" % (url, url_expected))
        dev[0].dump_monitor()

        with con:
            cur = con.cursor()
            cur.execute("SELECT * from current_sessions")
            rows = cur.fetchall()
            if len(rows) != 1:
                raise Exeception("Unexpected number of rows in current_sessions (%d; expected %d)" % (len(rows), 1))
            logger.info("current_sessions: " + str(rows))

        tests = ["foo", "disconnect q", "coa %s" % dev[0].own_addr()]
        for t in tests:
            if "FAIL" not in authsrv.request("DAC_REQUEST " + t):
                raise Exception("Invalid DAC_REQUEST accepted: " + t)
        if "OK" not in authsrv.request("DAC_REQUEST coa %s t_c_clear" % dev[0].own_addr()):
            raise Exception("DAC_REQUEST failed")

        ev = hapd.wait_event(["HS20-T-C-FILTERING-REMOVE"], timeout=5)
        if ev is None:
            raise Exception("Terms and Conditions filtering not disabled")
        if ev.split(' ')[1] != dev[0].own_addr():
            raise Exception("Unexpected STA address for filtering: " + ev)

        time.sleep(0.2)
        with con:
            cur = con.cursor()
            cur.execute("SELECT * from current_sessions")
            rows = cur.fetchall()
            if len(rows) != 1:
                raise Exeception("Unexpected number of rows in current_sessions (%d; expected %d)" % (len(rows), 1))
            logger.info("current_sessions: " + str(rows))
            if rows[0][4] != 0 or rows[0][5] != 0 or rows[0][6] != 1:
                raise Exception("Unexpected current_sessions information after CoA-ACK")

        dev[0].request("DISCONNECT")
        dev[0].wait_disconnected()
        dev[0].dump_monitor()

        # Simulate T&C server operation on user reading the updated version
        with con:
            cur = con.cursor()
            cur.execute("SELECT identity FROM pending_tc WHERE mac_addr='" +
                        dev[0].own_addr() + "'")
            rows = cur.fetchall()
            if len(rows) != 1:
                raise Exception("No pending_tc entry found")
            if rows[0][0] != 'user-mschapv2':
                raise Exception("Unexpected pending_tc identity value")

            cur.execute("UPDATE users SET t_c_timestamp=123456789 WHERE identity='user-mschapv2'")

        dev[0].request("RECONNECT")
        dev[0].wait_connected()

        ev = hapd.wait_event(["HS20-T-C-FILTERING-ADD"], timeout=0.1)
        if ev is not None:
            raise Exception("Terms and Conditions filtering enabled unexpectedly")
        hapd.dump_monitor()

        ev = dev[0].wait_event(["HS20-T-C-ACCEPTANCE"], timeout=0.1)
        if ev is not None:
            raise Exception("Unexpected Terms and Conditions Acceptance notification")
        dev[0].dump_monitor()

        dev[0].request("DISCONNECT")
        dev[0].wait_disconnected()
        dev[0].dump_monitor()

        # New T&C available
        hapd.set('hs20_t_c_timestamp', '123456790')

        dev[0].request("RECONNECT")
        dev[0].wait_connected()

        ev = hapd.wait_event(["HS20-T-C-FILTERING-ADD"], timeout=5)
        if ev is None:
            raise Exception("Terms and Conditions filtering not enabled")
        hapd.dump_monitor()

        ev = dev[0].wait_event(["HS20-T-C-ACCEPTANCE"], timeout=5)
        if ev is None:
            raise Exception("Terms and Conditions Acceptance notification not received (2)")
        dev[0].dump_monitor()

        dev[0].request("DISCONNECT")
        dev[0].wait_disconnected()
        dev[0].dump_monitor()

        # Simulate T&C server operation on user reading the updated version
        with con:
            cur = con.cursor()
            cur.execute("UPDATE users SET t_c_timestamp=123456790 WHERE identity='user-mschapv2'")

        dev[0].request("RECONNECT")
        dev[0].wait_connected()

        ev = hapd.wait_event(["HS20-T-C-FILTERING-ADD"], timeout=0.1)
        if ev is not None:
            raise Exception("Terms and Conditions filtering enabled unexpectedly")
        hapd.dump_monitor()

        ev = dev[0].wait_event(["HS20-T-C-ACCEPTANCE"], timeout=0.1)
        if ev is not None:
            raise Exception("Unexpected Terms and Conditions Acceptance notification (2)")
        dev[0].dump_monitor()
    finally:
        os.remove(dbfile)
        dev[0].request("SET pmf 0")

def test_ap_hs20_release_number_1(dev, apdev):
    """Hotspot 2.0 with AP claiming support for Release 1"""
    run_ap_hs20_release_number(dev, apdev, 1)

def test_ap_hs20_release_number_2(dev, apdev):
    """Hotspot 2.0 with AP claiming support for Release 2"""
    run_ap_hs20_release_number(dev, apdev, 2)

def test_ap_hs20_release_number_3(dev, apdev):
    """Hotspot 2.0 with AP claiming support for Release 3"""
    run_ap_hs20_release_number(dev, apdev, 3)

def run_ap_hs20_release_number(dev, apdev, release):
    check_eap_capa(dev[0], "MSCHAPV2")
    eap_test(dev[0], apdev[0], "21[3:26][6:7][99:99]", "TTLS", "user",
             release=release)
    rel = dev[0].get_status_field('hs20')
    if rel != str(release):
        raise Exception("Unexpected release number indicated: " + rel)

def test_ap_hs20_missing_pmf(dev, apdev):
    """Hotspot 2.0 connection attempt without PMF"""
    check_eap_capa(dev[0], "MSCHAPV2")
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid
    params['disable_dgaf'] = '1'
    hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable()
    dev[0].connect("test-hs20", proto="RSN", key_mgmt="WPA-EAP", eap="TTLS",
                   ieee80211w="0",
                   identity="hs20-test", password="password",
                   ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2",
                   scan_freq="2412", update_identifier="54321",
                   roaming_consortium_selection="1020304050",
                   wait_connect=False)
    ev = dev[0].wait_event(["CTRL-EVENT-ASSOC-REJECT"], timeout=10)
    dev[0].request("DISCONNECT")
    if ev is None:
        raise Exception("Association rejection not reported")
    if "status_code=31" not in ev:
        raise Exception("Unexpected rejection reason: " + ev)

def test_ap_hs20_open_osu_association(dev, apdev):
    """Hotspot 2.0 open OSU association"""
    try:
        run_ap_hs20_open_osu_association(dev, apdev)
    finally:
        dev[0].request("VENDOR_ELEM_REMOVE 13 *")

def run_ap_hs20_open_osu_association(dev, apdev):
    params = {"ssid": "HS 2.0 OSU open"}
    hostapd.add_ap(apdev[0], params)
    dev[0].connect("HS 2.0 OSU open", key_mgmt="NONE", scan_freq="2412")
    dev[0].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected()
    dev[0].dump_monitor()
    # Test with unexpected Hotspot 2.0 Indication element in Assoc Req
    dev[0].request("VENDOR_ELEM_ADD 13 dd07506f9a10220000")
    dev[0].connect("HS 2.0 OSU open", key_mgmt="NONE", scan_freq="2412")
