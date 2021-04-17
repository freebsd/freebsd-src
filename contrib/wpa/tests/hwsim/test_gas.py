# GAS tests
# Copyright (c) 2013, Qualcomm Atheros, Inc.
# Copyright (c) 2013-2015, Jouni Malinen <j@w1.fi>
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

from remotehost import remote_compatible
import time
import binascii
import logging
logger = logging.getLogger()
import os
import re
import struct

import hostapd
from wpasupplicant import WpaSupplicant
from tshark import run_tshark
from utils import alloc_fail, wait_fail_trigger, skip_with_fips, HwsimSkip
from hwsim import HWSimRadio

def hs20_ap_params():
    params = hostapd.wpa2_params(ssid="test-gas")
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
    params['anqp_3gpp_cell_net'] = "244,91"
    params['network_auth_type'] = "02http://www.example.com/redirect/me/here/"
    params['ipaddr_type_availability'] = "14"
    params['hs20'] = "1"
    params['hs20_oper_friendly_name'] = ["eng:Example operator", "fin:Esimerkkioperaattori"]
    params['hs20_wan_metrics'] = "01:8000:1000:80:240:3000"
    params['hs20_conn_capab'] = ["1:0:2", "6:22:1", "17:5060:0"]
    params['hs20_operating_class'] = "5173"
    return params

def start_ap(ap):
    params = hs20_ap_params()
    params['hessid'] = ap['bssid']
    return hostapd.add_ap(ap, params)

def get_gas_response(dev, bssid, info, allow_fetch_failure=False,
                     extra_test=False):
    exp = r'<.>(GAS-RESPONSE-INFO) addr=([0-9a-f:]*) dialog_token=([0-9]*) status_code=([0-9]*) resp_len=([\-0-9]*)'
    res = re.split(exp, info)
    if len(res) < 6:
        raise Exception("Could not parse GAS-RESPONSE-INFO")
    if res[2] != bssid:
        raise Exception("Unexpected BSSID in response")
    token = res[3]
    status = res[4]
    if status != "0":
        raise Exception("GAS query failed")
    resp_len = res[5]
    if resp_len == "-1":
        raise Exception("GAS query reported invalid response length")
    if int(resp_len) > 2000:
        raise Exception("Unexpected long GAS response")

    if extra_test:
        if "FAIL" not in dev.request("GAS_RESPONSE_GET " + bssid + " 123456"):
            raise Exception("Invalid dialog token accepted")
        if "FAIL-Invalid range" not in dev.request("GAS_RESPONSE_GET " + bssid + " " + token + " 10000,10001"):
            raise Exception("Invalid range accepted")
        if "FAIL-Invalid range" not in dev.request("GAS_RESPONSE_GET " + bssid + " " + token + " 0,10000"):
            raise Exception("Invalid range accepted")
        if "FAIL" not in dev.request("GAS_RESPONSE_GET " + bssid + " " + token + " 0"):
            raise Exception("Invalid GAS_RESPONSE_GET accepted")

        res1_2 = dev.request("GAS_RESPONSE_GET " + bssid + " " + token + " 1,2")
        res5_3 = dev.request("GAS_RESPONSE_GET " + bssid + " " + token + " 5,3")

    resp = dev.request("GAS_RESPONSE_GET " + bssid + " " + token)
    if "FAIL" in resp:
        if allow_fetch_failure:
            logger.debug("GAS response was not available anymore")
            return
        raise Exception("Could not fetch GAS response")
    if len(resp) != int(resp_len) * 2:
        raise Exception("Unexpected GAS response length")
    logger.debug("GAS response: " + resp)
    if extra_test:
        if resp[2:6] != res1_2:
            raise Exception("Unexpected response substring res1_2: " + res1_2)
        if resp[10:16] != res5_3:
            raise Exception("Unexpected response substring res5_3: " + res5_3)

def test_gas_generic(dev, apdev):
    """Generic GAS query"""
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid
    hostapd.add_ap(apdev[0], params)

    cmds = ["foo",
            "00:11:22:33:44:55",
            "00:11:22:33:44:55 ",
            "00:11:22:33:44:55  ",
            "00:11:22:33:44:55 1",
            "00:11:22:33:44:55 1 1234",
            "00:11:22:33:44:55 qq",
            "00:11:22:33:44:55 qq 1234",
            "00:11:22:33:44:55 00      1",
            "00:11:22:33:44:55 00 123",
            "00:11:22:33:44:55 00 ",
            "00:11:22:33:44:55 00 qq"]
    for cmd in cmds:
        if "FAIL" not in dev[0].request("GAS_REQUEST " + cmd):
            raise Exception("Invalid GAS_REQUEST accepted: " + cmd)

    dev[0].scan_for_bss(bssid, freq="2412", force_scan=True)
    req = dev[0].request("GAS_REQUEST " + bssid + " 00 000102000101")
    if "FAIL" in req:
        raise Exception("GAS query request rejected")
    ev = dev[0].wait_event(["GAS-RESPONSE-INFO"], timeout=10)
    if ev is None:
        raise Exception("GAS query timed out")
    get_gas_response(dev[0], bssid, ev, extra_test=True)

    if "FAIL" not in dev[0].request("GAS_RESPONSE_GET ff"):
        raise Exception("Invalid GAS_RESPONSE_GET accepted")

def test_gas_rand_ta(dev, apdev, params):
    """Generic GAS query with random TA"""
    flags = int(dev[0].get_driver_status_field('capa.flags'), 16)
    if flags & 0x0000400000000000 == 0:
        raise HwsimSkip("Driver does not support random GAS TA")

    try:
        _test_gas_rand_ta(dev, apdev, params['logdir'])
    finally:
        dev[0].request("SET gas_rand_mac_addr 0")

def _test_gas_rand_ta(dev, apdev, logdir):
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid
    hostapd.add_ap(apdev[0], params)

    dev[0].scan_for_bss(bssid, freq="2412", force_scan=True)
    req = dev[0].request("SET gas_rand_mac_addr 1")
    if "FAIL" in req:
        raise Exception("Failed to set gas_rand_mac_addr")

    dev[0].scan_for_bss(bssid, freq="2412", force_scan=True)
    req = dev[0].request("GAS_REQUEST " + bssid + " 00 000102000101")
    if "FAIL" in req:
        raise Exception("GAS query request rejected")
    ev = dev[0].wait_event(["GAS-RESPONSE-INFO"], timeout=10)
    if ev is None:
        raise Exception("GAS query timed out")
    get_gas_response(dev[0], bssid, ev, extra_test=True)

    out = run_tshark(os.path.join(logdir, "hwsim0.pcapng"),
                     "wlan_mgt.fixed.category_code == 4 && (wlan_mgt.fixed.publicact == 0x0a || wlan_mgt.fixed.publicact == 0x0b)",
                     display=["wlan.ta", "wlan.ra"])
    res = out.splitlines()
    if len(res) != 2:
        raise Exception("Unexpected number of GAS frames")
    req_ta = res[0].split('\t')[0]
    resp_ra = res[1].split('\t')[1]
    logger.info("Request TA: %s, Response RA: %s" % (req_ta, resp_ra))
    if req_ta != resp_ra:
        raise Exception("Request TA does not match response RA")
    if req_ta == dev[0].own_addr():
        raise Exception("Request TA was own permanent MAC address, not random")

def test_gas_concurrent_scan(dev, apdev):
    """Generic GAS queries with concurrent scan operation"""
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid
    hostapd.add_ap(apdev[0], params)

    # get BSS entry available to allow GAS query
    dev[0].scan_for_bss(bssid, freq="2412", force_scan=True)

    logger.info("Request concurrent operations")
    req = dev[0].request("GAS_REQUEST " + bssid + " 00 000102000101")
    if "FAIL" in req:
        raise Exception("GAS query request rejected")
    req = dev[0].request("GAS_REQUEST " + bssid + " 00 000102000801")
    if "FAIL" in req:
        raise Exception("GAS query request rejected")
    dev[0].scan(no_wait=True)
    req = dev[0].request("GAS_REQUEST " + bssid + " 00 000102000201")
    if "FAIL" in req:
        raise Exception("GAS query request rejected")
    req = dev[0].request("GAS_REQUEST " + bssid + " 00 000102000501")
    if "FAIL" in req:
        raise Exception("GAS query request rejected")

    responses = 0
    for i in range(0, 5):
        ev = dev[0].wait_event(["GAS-RESPONSE-INFO", "CTRL-EVENT-SCAN-RESULTS"],
                               timeout=10)
        if ev is None:
            raise Exception("Operation timed out")
        if "GAS-RESPONSE-INFO" in ev:
            responses = responses + 1
            get_gas_response(dev[0], bssid, ev, allow_fetch_failure=True)

    if responses != 4:
        raise Exception("Unexpected number of GAS responses")

def test_gas_concurrent_connect(dev, apdev):
    """Generic GAS queries with concurrent connection operation"""
    skip_with_fips(dev[0])
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid
    hostapd.add_ap(apdev[0], params)

    dev[0].scan_for_bss(bssid, freq="2412", force_scan=True)

    logger.debug("Start concurrent connect and GAS request")
    dev[0].connect("test-gas", key_mgmt="WPA-EAP", eap="TTLS",
                   identity="DOMAIN\mschapv2 user", anonymous_identity="ttls",
                   password="password", phase2="auth=MSCHAPV2",
                   ca_cert="auth_serv/ca.pem", wait_connect=False,
                   scan_freq="2412")
    req = dev[0].request("GAS_REQUEST " + bssid + " 00 000102000101")
    if "FAIL" in req:
        raise Exception("GAS query request rejected")

    ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED", "GAS-RESPONSE-INFO"],
                           timeout=20)
    if ev is None:
        raise Exception("Operation timed out")
    if "CTRL-EVENT-CONNECTED" not in ev:
        raise Exception("Unexpected operation order")

    ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED", "GAS-RESPONSE-INFO"],
                           timeout=20)
    if ev is None:
        raise Exception("Operation timed out")
    if "GAS-RESPONSE-INFO" not in ev:
        raise Exception("Unexpected operation order")
    get_gas_response(dev[0], bssid, ev)

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected(timeout=5)

    logger.debug("Wait six seconds for expiration of connect-without-scan")
    time.sleep(6)
    dev[0].dump_monitor()

    logger.debug("Start concurrent GAS request and connect")
    req = dev[0].request("GAS_REQUEST " + bssid + " 00 000102000101")
    if "FAIL" in req:
        raise Exception("GAS query request rejected")
    dev[0].request("RECONNECT")

    ev = dev[0].wait_event(["GAS-RESPONSE-INFO"], timeout=10)
    if ev is None:
        raise Exception("Operation timed out")
    get_gas_response(dev[0], bssid, ev)

    ev = dev[0].wait_event(["CTRL-EVENT-SCAN-RESULTS"], timeout=20)
    if ev is None:
        raise Exception("No new scan results reported")

    ev = dev[0].wait_connected(timeout=20, error="Operation tiemd out")
    if "CTRL-EVENT-CONNECTED" not in ev:
        raise Exception("Unexpected operation order")

def gas_fragment_and_comeback(dev, apdev, frag_limit=0, comeback_delay=0):
    hapd = start_ap(apdev)
    if frag_limit:
        hapd.set("gas_frag_limit", str(frag_limit))
    if comeback_delay:
        hapd.set("gas_comeback_delay", str(comeback_delay))

    dev.scan_for_bss(apdev['bssid'], freq="2412", force_scan=True)
    dev.request("FETCH_ANQP")
    ev = dev.wait_event(["GAS-QUERY-DONE"], timeout=5)
    if ev is None:
        raise Exception("No GAS-QUERY-DONE event")
    if "result=SUCCESS" not in ev:
        raise Exception("Unexpected GAS result: " + ev)
    for i in range(0, 13):
        ev = dev.wait_event(["RX-ANQP", "RX-HS20-ANQP"], timeout=5)
        if ev is None:
            raise Exception("Operation timed out")
    ev = dev.wait_event(["ANQP-QUERY-DONE"], timeout=1)
    if ev is None:
        raise Exception("No ANQP-QUERY-DONE event")
    if "result=SUCCESS" not in ev:
        raise Exception("Unexpected ANQP result: " + ev)

def test_gas_fragment(dev, apdev):
    """GAS fragmentation"""
    gas_fragment_and_comeback(dev[0], apdev[0], frag_limit=50)

def test_gas_fragment_mcc(dev, apdev):
    """GAS fragmentation with mac80211_hwsim MCC enabled"""
    with HWSimRadio(n_channels=2) as (radio, iface):
        wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
        wpas.interface_add(iface)
        gas_fragment_and_comeback(wpas, apdev[0], frag_limit=50)

def test_gas_fragment_with_comeback_delay(dev, apdev):
    """GAS fragmentation and comeback delay"""
    gas_fragment_and_comeback(dev[0], apdev[0], frag_limit=50,
                              comeback_delay=500)

def test_gas_fragment_with_comeback_delay_mcc(dev, apdev):
    """GAS fragmentation and comeback delay with mac80211_hwsim MCC enabled"""
    with HWSimRadio(n_channels=2) as (radio, iface):
        wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
        wpas.interface_add(iface)
        gas_fragment_and_comeback(wpas, apdev[0], frag_limit=50,
                                  comeback_delay=500)

def test_gas_comeback_delay(dev, apdev):
    """GAS comeback delay"""
    run_gas_comeback_delay(dev, apdev, 500)

def test_gas_comeback_delay_long(dev, apdev):
    """GAS long comeback delay"""
    run_gas_comeback_delay(dev, apdev, 2500)

def test_gas_comeback_delay_long2(dev, apdev):
    """GAS long comeback delay over default STA timeout"""
    run_gas_comeback_delay(dev, apdev, 6000)

def run_gas_comeback_delay(dev, apdev, delay):
    hapd = start_ap(apdev[0])
    hapd.set("gas_comeback_delay", str(delay))

    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412", force_scan=True)
    dev[0].request("FETCH_ANQP")
    if "FAIL-BUSY" not in dev[0].request("SCAN"):
        raise Exception("SCAN accepted during FETCH_ANQP")
    for i in range(0, 6):
        ev = dev[0].wait_event(["RX-ANQP"], timeout=10)
        if ev is None:
            raise Exception("Operation timed out")

@remote_compatible
def test_gas_stop_fetch_anqp(dev, apdev):
    """Stop FETCH_ANQP operation"""
    hapd = start_ap(apdev[0])

    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412", force_scan=True)
    hapd.set("ext_mgmt_frame_handling", "1")
    dev[0].request("FETCH_ANQP")
    dev[0].request("STOP_FETCH_ANQP")
    hapd.set("ext_mgmt_frame_handling", "0")
    ev = dev[0].wait_event(["RX-ANQP", "GAS-QUERY-DONE"], timeout=10)
    if ev is None:
        raise Exception("GAS-QUERY-DONE timed out")
    if "RX-ANQP" in ev:
        raise Exception("Unexpected ANQP response received")

def test_gas_anqp_get(dev, apdev):
    """GAS/ANQP query for both IEEE 802.11 and Hotspot 2.0 elements"""
    hapd = start_ap(apdev[0])
    bssid = apdev[0]['bssid']

    dev[0].scan_for_bss(bssid, freq="2412", force_scan=True)
    if "OK" not in dev[0].request("ANQP_GET " + bssid + " 258,268,hs20:3,hs20:4"):
        raise Exception("ANQP_GET command failed")

    ev = dev[0].wait_event(["GAS-QUERY-START"], timeout=5)
    if ev is None:
        raise Exception("GAS query start timed out")

    ev = dev[0].wait_event(["GAS-QUERY-DONE"], timeout=10)
    if ev is None:
        raise Exception("GAS query timed out")

    ev = dev[0].wait_event(["RX-ANQP"], timeout=1)
    if ev is None or "Venue Name" not in ev:
        raise Exception("Did not receive Venue Name")

    ev = dev[0].wait_event(["RX-ANQP"], timeout=1)
    if ev is None or "Domain Name list" not in ev:
        raise Exception("Did not receive Domain Name list")

    ev = dev[0].wait_event(["RX-HS20-ANQP"], timeout=1)
    if ev is None or "Operator Friendly Name" not in ev:
        raise Exception("Did not receive Operator Friendly Name")

    ev = dev[0].wait_event(["RX-HS20-ANQP"], timeout=1)
    if ev is None or "WAN Metrics" not in ev:
        raise Exception("Did not receive WAN Metrics")

    ev = dev[0].wait_event(["ANQP-QUERY-DONE"], timeout=10)
    if ev is None:
        raise Exception("ANQP-QUERY-DONE event not seen")
    if "result=SUCCESS" not in ev:
        raise Exception("Unexpected result: " + ev)

    if "OK" not in dev[0].request("ANQP_GET " + bssid + " hs20:3"):
        raise Exception("ANQP_GET command failed")

    ev = dev[0].wait_event(["GAS-QUERY-START"], timeout=5)
    if ev is None:
        raise Exception("GAS query start timed out")

    ev = dev[0].wait_event(["GAS-QUERY-DONE"], timeout=10)
    if ev is None:
        raise Exception("GAS query timed out")

    ev = dev[0].wait_event(["RX-HS20-ANQP"], timeout=1)
    if ev is None or "Operator Friendly Name" not in ev:
        raise Exception("Did not receive Operator Friendly Name")

    ev = dev[0].wait_event(["ANQP-QUERY-DONE"], timeout=10)
    if ev is None:
        raise Exception("ANQP-QUERY-DONE event not seen")
    if "result=SUCCESS" not in ev:
        raise Exception("Unexpected result: " + ev)

    if "OK" not in dev[0].request("HS20_ANQP_GET " + bssid + " 3,4"):
        raise Exception("ANQP_GET command failed")

    ev = dev[0].wait_event(["RX-HS20-ANQP"], timeout=1)
    if ev is None or "Operator Friendly Name" not in ev:
        raise Exception("Did not receive Operator Friendly Name")

    ev = dev[0].wait_event(["RX-HS20-ANQP"], timeout=1)
    if ev is None or "WAN Metrics" not in ev:
        raise Exception("Did not receive WAN Metrics")

    logger.info("Attempt an MBO request with an AP that does not support MBO")
    if "OK" not in dev[0].request("ANQP_GET " + bssid + " 272,mbo:2"):
        raise Exception("ANQP_GET command failed (2)")

    ev = dev[0].wait_event(["GAS-QUERY-START"], timeout=5)
    if ev is None:
        raise Exception("GAS query start timed out (2)")

    ev = dev[0].wait_event(["GAS-QUERY-DONE"], timeout=10)
    if ev is None:
        raise Exception("GAS query timed out (2)")

    cmds = ["",
            "foo",
            "00:11:22:33:44:55 258,hs20:-1",
            "00:11:22:33:44:55 258,hs20:0",
            "00:11:22:33:44:55 258,hs20:32",
            "00:11:22:33:44:55 hs20:-1",
            "00:11:22:33:44:55 hs20:0",
            "00:11:22:33:44:55 hs20:32",
            "00:11:22:33:44:55 mbo:-1",
            "00:11:22:33:44:55 mbo:0",
            "00:11:22:33:44:55 mbo:999",
            "00:11:22:33:44:55 mbo:1,258,mbo:2,mbo:3,259",
            "00:11:22:33:44:55",
            "00:11:22:33:44:55 ",
            "00:11:22:33:44:55 0",
            "00:11:22:33:44:55 1"]
    for cmd in cmds:
        if "FAIL" not in dev[0].request("ANQP_GET " + cmd):
            raise Exception("Invalid ANQP_GET accepted")

    cmds = ["",
            "foo",
            "00:11:22:33:44:55 -1",
            "00:11:22:33:44:55 0",
            "00:11:22:33:44:55 32",
            "00:11:22:33:44:55",
            "00:11:22:33:44:55 ",
            "00:11:22:33:44:55 0",
            "00:11:22:33:44:55 1"]
    for cmd in cmds:
        if "FAIL" not in dev[0].request("HS20_ANQP_GET " + cmd):
            raise Exception("Invalid HS20_ANQP_GET accepted")

def test_gas_anqp_get_no_scan(dev, apdev):
    """GAS/ANQP query without scan"""
    hapd = start_ap(apdev[0])
    bssid = apdev[0]['bssid']
    if "OK" not in dev[0].request("ANQP_GET " + bssid + " freq=2412 258"):
        raise Exception("ANQP_GET command failed")
    ev = dev[0].wait_event(["ANQP-QUERY-DONE"], timeout=10)
    if ev is None:
        raise Exception("ANQP query timed out")
    dev[0].dump_monitor()

    if "OK" not in dev[0].request("ANQP_GET 02:11:22:33:44:55 freq=2417 258"):
        raise Exception("ANQP_GET command failed")
    ev = dev[0].wait_event(["ANQP-QUERY-DONE"], timeout=10)
    if ev is None:
        raise Exception("ANQP query timed out [2]")
    if "result=FAILURE" not in ev:
        raise Exception("Unexpected result: " + ev)

def test_gas_anqp_get_oom(dev, apdev):
    """GAS/ANQP query OOM"""
    hapd = start_ap(apdev[0])
    bssid = apdev[0]['bssid']

    dev[0].scan_for_bss(bssid, freq="2412", force_scan=True)
    with alloc_fail(dev[0], 1, "wpabuf_alloc;anqp_send_req"):
        if "FAIL" not in dev[0].request("ANQP_GET " + bssid + " 258,268,hs20:3,hs20:4"):
            raise Exception("ANQP_GET command accepted during OOM")
    with alloc_fail(dev[0], 1, "hs20_build_anqp_req;hs20_anqp_send_req"):
        if "FAIL" not in dev[0].request("HS20_ANQP_GET " + bssid + " 1"):
            raise Exception("HS20_ANQP_GET command accepted during OOM")
    with alloc_fail(dev[0], 1, "gas_query_req;hs20_anqp_send_req"):
        if "FAIL" not in dev[0].request("HS20_ANQP_GET " + bssid + " 1"):
            raise Exception("HS20_ANQP_GET command accepted during OOM")
    with alloc_fail(dev[0], 1, "=hs20_anqp_send_req"):
        if "FAIL" not in dev[0].request("REQ_HS20_ICON " + bssid + " w1fi_logo"):
            raise Exception("REQ_HS20_ICON command accepted during OOM")
    with alloc_fail(dev[0], 2, "=hs20_anqp_send_req"):
        if "FAIL" not in dev[0].request("REQ_HS20_ICON " + bssid + " w1fi_logo"):
            raise Exception("REQ_HS20_ICON command accepted during OOM")

def test_gas_anqp_icon_binary_proto(dev, apdev):
    """GAS/ANQP and icon binary protocol testing"""
    hapd = start_ap(apdev[0])
    bssid = apdev[0]['bssid']

    dev[0].scan_for_bss(bssid, freq="2412", force_scan=True)
    hapd.set("ext_mgmt_frame_handling", "1")

    tests = ['010000', '01000000', '00000000', '00030000', '00020000',
             '00000100', '0001ff0100ee', '0001ff0200ee']
    for test in tests:
        dev[0].request("HS20_ICON_REQUEST " + bssid + " w1fi_logo")
        query = gas_rx(hapd)
        gas = parse_gas(query['payload'])
        resp = action_response(query)
        data = binascii.unhexlify(test)
        data = binascii.unhexlify('506f9a110b00') + data
        data = struct.pack('<HHH', len(data) + 4, 0xdddd, len(data)) + data
        resp['payload'] = anqp_initial_resp(gas['dialog_token'], 0) + data
        send_gas_resp(hapd, resp)
        expect_gas_result(dev[0], "SUCCESS")

def test_gas_anqp_hs20_proto(dev, apdev):
    """GAS/ANQP and Hotspot 2.0 element protocol testing"""
    hapd = start_ap(apdev[0])
    bssid = apdev[0]['bssid']

    dev[0].scan_for_bss(bssid, freq="2412", force_scan=True)
    hapd.set("ext_mgmt_frame_handling", "1")

    tests = ['00', '0100', '0201', '0300', '0400', '0500', '0600', '0700',
             '0800', '0900', '0a00', '0b0000000000']
    for test in tests:
        dev[0].request("HS20_ANQP_GET " + bssid + " 3,4")
        query = gas_rx(hapd)
        gas = parse_gas(query['payload'])
        resp = action_response(query)
        data = binascii.unhexlify(test)
        data = binascii.unhexlify('506f9a11') + data
        data = struct.pack('<HHH', len(data) + 4, 0xdddd, len(data)) + data
        resp['payload'] = anqp_initial_resp(gas['dialog_token'], 0) + data
        send_gas_resp(hapd, resp)
        expect_gas_result(dev[0], "SUCCESS")

def expect_gas_result(dev, result, status=None):
    ev = dev.wait_event(["GAS-QUERY-DONE"], timeout=10)
    if ev is None:
        raise Exception("GAS query timed out")
    if "result=" + result not in ev:
        raise Exception("Unexpected GAS query result")
    if status and "status_code=" + str(status) + ' ' not in ev:
        raise Exception("Unexpected GAS status code")

def anqp_get(dev, bssid, id):
    if "OK" not in dev.request("ANQP_GET " + bssid + " " + str(id)):
        raise Exception("ANQP_GET command failed")
    ev = dev.wait_event(["GAS-QUERY-START"], timeout=5)
    if ev is None:
        raise Exception("GAS query start timed out")

def test_gas_timeout(dev, apdev):
    """GAS timeout"""
    hapd = start_ap(apdev[0])
    bssid = apdev[0]['bssid']

    dev[0].scan_for_bss(bssid, freq="2412", force_scan=True)
    hapd.set("ext_mgmt_frame_handling", "1")

    anqp_get(dev[0], bssid, 263)

    ev = hapd.wait_event(["MGMT-RX"], timeout=5)
    if ev is None:
        raise Exception("MGMT RX wait timed out")

    expect_gas_result(dev[0], "TIMEOUT")

MGMT_SUBTYPE_ACTION = 13
ACTION_CATEG_PUBLIC = 4

GAS_INITIAL_REQUEST = 10
GAS_INITIAL_RESPONSE = 11
GAS_COMEBACK_REQUEST = 12
GAS_COMEBACK_RESPONSE = 13
GAS_ACTIONS = [GAS_INITIAL_REQUEST, GAS_INITIAL_RESPONSE,
               GAS_COMEBACK_REQUEST, GAS_COMEBACK_RESPONSE]

def anqp_adv_proto():
    return struct.pack('BBBB', 108, 2, 127, 0)

def anqp_initial_resp(dialog_token, status_code, comeback_delay=0):
    return struct.pack('<BBBHH', ACTION_CATEG_PUBLIC, GAS_INITIAL_RESPONSE,
                       dialog_token, status_code, comeback_delay) + anqp_adv_proto()

def anqp_comeback_resp(dialog_token, status_code=0, id=0, more=False, comeback_delay=0, bogus_adv_proto=False):
    if more:
        id |= 0x80
    if bogus_adv_proto:
        adv = struct.pack('BBBB', 108, 2, 127, 1)
    else:
        adv = anqp_adv_proto()
    return struct.pack('<BBBHBH', ACTION_CATEG_PUBLIC, GAS_COMEBACK_RESPONSE,
                       dialog_token, status_code, id, comeback_delay) + adv

def gas_rx(hapd):
    count = 0
    while count < 30:
        count = count + 1
        query = hapd.mgmt_rx()
        if query is None:
            raise Exception("Action frame not received")
        if query['subtype'] != MGMT_SUBTYPE_ACTION:
            continue
        payload = query['payload']
        if len(payload) < 2:
            continue
        (category, action) = struct.unpack('BB', payload[0:2])
        if category != ACTION_CATEG_PUBLIC or action not in GAS_ACTIONS:
            continue
        return query
    raise Exception("No Action frame received")

def parse_gas(payload):
    pos = payload
    (category, action, dialog_token) = struct.unpack('BBB', pos[0:3])
    if category != ACTION_CATEG_PUBLIC:
        return None
    if action not in GAS_ACTIONS:
        return None
    gas = {}
    gas['action'] = action
    pos = pos[3:]

    if len(pos) < 1 and action != GAS_COMEBACK_REQUEST:
        return None

    gas['dialog_token'] = dialog_token

    if action == GAS_INITIAL_RESPONSE:
        if len(pos) < 4:
            return None
        (status_code, comeback_delay) = struct.unpack('<HH', pos[0:4])
        gas['status_code'] = status_code
        gas['comeback_delay'] = comeback_delay

    if action == GAS_COMEBACK_RESPONSE:
        if len(pos) < 5:
            return None
        (status_code, frag, comeback_delay) = struct.unpack('<HBH', pos[0:5])
        gas['status_code'] = status_code
        gas['frag'] = frag
        gas['comeback_delay'] = comeback_delay

    return gas

def action_response(req):
    resp = {}
    resp['fc'] = req['fc']
    resp['da'] = req['sa']
    resp['sa'] = req['da']
    resp['bssid'] = req['bssid']
    return resp

def send_gas_resp(hapd, resp):
    hapd.mgmt_tx(resp)
    ev = hapd.wait_event(["MGMT-TX-STATUS"], timeout=5)
    if ev is None:
        raise Exception("Missing TX status for GAS response")
    if "ok=1" not in ev:
        raise Exception("GAS response not acknowledged")

def test_gas_invalid_response_type(dev, apdev):
    """GAS invalid response type"""
    hapd = start_ap(apdev[0])
    bssid = apdev[0]['bssid']

    dev[0].scan_for_bss(bssid, freq="2412", force_scan=True)
    hapd.set("ext_mgmt_frame_handling", "1")

    anqp_get(dev[0], bssid, 263)

    query = gas_rx(hapd)
    gas = parse_gas(query['payload'])

    resp = action_response(query)
    # GAS Comeback Response instead of GAS Initial Response
    resp['payload'] = anqp_comeback_resp(gas['dialog_token']) + struct.pack('<H', 0)
    send_gas_resp(hapd, resp)

    # station drops the invalid frame, so this needs to result in GAS timeout
    expect_gas_result(dev[0], "TIMEOUT")

def test_gas_failure_status_code(dev, apdev):
    """GAS failure status code"""
    hapd = start_ap(apdev[0])
    bssid = apdev[0]['bssid']

    dev[0].scan_for_bss(bssid, freq="2412", force_scan=True)
    hapd.set("ext_mgmt_frame_handling", "1")

    anqp_get(dev[0], bssid, 263)

    query = gas_rx(hapd)
    gas = parse_gas(query['payload'])

    resp = action_response(query)
    resp['payload'] = anqp_initial_resp(gas['dialog_token'], 61) + struct.pack('<H', 0)
    send_gas_resp(hapd, resp)

    expect_gas_result(dev[0], "FAILURE")

def test_gas_malformed(dev, apdev):
    """GAS malformed response frames"""
    hapd = start_ap(apdev[0])
    bssid = apdev[0]['bssid']

    dev[0].scan_for_bss(bssid, freq="2412", force_scan=True)
    hapd.set("ext_mgmt_frame_handling", "1")

    anqp_get(dev[0], bssid, 263)

    query = gas_rx(hapd)
    gas = parse_gas(query['payload'])

    resp = action_response(query)

    resp['payload'] = struct.pack('<BBBH', ACTION_CATEG_PUBLIC,
                                  GAS_COMEBACK_RESPONSE,
                                  gas['dialog_token'], 0)
    hapd.mgmt_tx(resp)

    resp['payload'] = struct.pack('<BBBHB', ACTION_CATEG_PUBLIC,
                                  GAS_COMEBACK_RESPONSE,
                                  gas['dialog_token'], 0, 0)
    hapd.mgmt_tx(resp)

    hdr = struct.pack('<BBBHH', ACTION_CATEG_PUBLIC, GAS_INITIAL_RESPONSE,
                      gas['dialog_token'], 0, 0)
    resp['payload'] = hdr + struct.pack('B', 108)
    hapd.mgmt_tx(resp)
    resp['payload'] = hdr + struct.pack('BB', 108, 0)
    hapd.mgmt_tx(resp)
    resp['payload'] = hdr + struct.pack('BB', 108, 1)
    hapd.mgmt_tx(resp)
    resp['payload'] = hdr + struct.pack('BB', 108, 255)
    hapd.mgmt_tx(resp)
    resp['payload'] = hdr + struct.pack('BBB', 108, 1, 127)
    hapd.mgmt_tx(resp)
    resp['payload'] = hdr + struct.pack('BBB', 108, 2, 127)
    hapd.mgmt_tx(resp)
    resp['payload'] = hdr + struct.pack('BBBB', 0, 2, 127, 0)
    hapd.mgmt_tx(resp)

    resp['payload'] = anqp_initial_resp(gas['dialog_token'], 0) + struct.pack('<H', 1)
    hapd.mgmt_tx(resp)

    resp['payload'] = anqp_initial_resp(gas['dialog_token'], 0) + struct.pack('<HB', 2, 0)
    hapd.mgmt_tx(resp)

    resp['payload'] = anqp_initial_resp(gas['dialog_token'], 0) + struct.pack('<H', 65535)
    hapd.mgmt_tx(resp)

    resp['payload'] = anqp_initial_resp(gas['dialog_token'], 0) + struct.pack('<HBB', 1, 0, 0)
    hapd.mgmt_tx(resp)

    # Station drops invalid frames, but the last of the responses is valid from
    # GAS view point even though it has an extra octet in the end and the ANQP
    # part of the response is not valid. This is reported as successfully
    # completed GAS exchange.
    expect_gas_result(dev[0], "SUCCESS")

    ev = dev[0].wait_event(["ANQP-QUERY-DONE"], timeout=5)
    if ev is None:
        raise Exception("ANQP-QUERY-DONE not reported")
    if "result=INVALID_FRAME" not in ev:
        raise Exception("Unexpected result: " + ev)

def init_gas(hapd, bssid, dev):
    anqp_get(dev, bssid, 263)
    query = gas_rx(hapd)
    gas = parse_gas(query['payload'])
    dialog_token = gas['dialog_token']

    resp = action_response(query)
    resp['payload'] = anqp_initial_resp(dialog_token, 0, comeback_delay=1) + struct.pack('<H', 0)
    send_gas_resp(hapd, resp)

    query = gas_rx(hapd)
    gas = parse_gas(query['payload'])
    if gas['action'] != GAS_COMEBACK_REQUEST:
        raise Exception("Unexpected request action")
    if gas['dialog_token'] != dialog_token:
        raise Exception("Unexpected dialog token change")
    return query, dialog_token

def allow_gas_initial_req(hapd, dialog_token):
    msg = hapd.mgmt_rx(timeout=1)
    if msg is not None:
        gas = parse_gas(msg['payload'])
        if gas['action'] != GAS_INITIAL_REQUEST or dialog_token == gas['dialog_token']:
            raise Exception("Unexpected management frame")

def test_gas_malformed_comeback_resp(dev, apdev):
    """GAS malformed comeback response frames"""
    hapd = start_ap(apdev[0])
    bssid = apdev[0]['bssid']

    dev[0].scan_for_bss(bssid, freq="2412", force_scan=True)
    hapd.set("ext_mgmt_frame_handling", "1")

    logger.debug("Non-zero status code in comeback response")
    query, dialog_token = init_gas(hapd, bssid, dev[0])
    resp = action_response(query)
    resp['payload'] = anqp_comeback_resp(dialog_token, status_code=2) + struct.pack('<H', 0)
    send_gas_resp(hapd, resp)
    expect_gas_result(dev[0], "FAILURE", status=2)

    logger.debug("Different advertisement protocol in comeback response")
    query, dialog_token = init_gas(hapd, bssid, dev[0])
    resp = action_response(query)
    resp['payload'] = anqp_comeback_resp(dialog_token, bogus_adv_proto=True) + struct.pack('<H', 0)
    send_gas_resp(hapd, resp)
    expect_gas_result(dev[0], "PEER_ERROR")

    logger.debug("Non-zero frag id and comeback delay in comeback response")
    query, dialog_token = init_gas(hapd, bssid, dev[0])
    resp = action_response(query)
    resp['payload'] = anqp_comeback_resp(dialog_token, id=1, comeback_delay=1) + struct.pack('<H', 0)
    send_gas_resp(hapd, resp)
    expect_gas_result(dev[0], "PEER_ERROR")

    logger.debug("Unexpected frag id in comeback response")
    query, dialog_token = init_gas(hapd, bssid, dev[0])
    resp = action_response(query)
    resp['payload'] = anqp_comeback_resp(dialog_token, id=1) + struct.pack('<H', 0)
    send_gas_resp(hapd, resp)
    expect_gas_result(dev[0], "PEER_ERROR")

    logger.debug("Empty fragment and replay in comeback response")
    query, dialog_token = init_gas(hapd, bssid, dev[0])
    resp = action_response(query)
    resp['payload'] = anqp_comeback_resp(dialog_token, more=True) + struct.pack('<H', 0)
    send_gas_resp(hapd, resp)
    query = gas_rx(hapd)
    gas = parse_gas(query['payload'])
    if gas['action'] != GAS_COMEBACK_REQUEST:
        raise Exception("Unexpected request action")
    if gas['dialog_token'] != dialog_token:
        raise Exception("Unexpected dialog token change")
    resp = action_response(query)
    resp['payload'] = anqp_comeback_resp(dialog_token) + struct.pack('<H', 0)
    send_gas_resp(hapd, resp)
    resp['payload'] = anqp_comeback_resp(dialog_token, id=1) + struct.pack('<H', 0)
    send_gas_resp(hapd, resp)
    expect_gas_result(dev[0], "SUCCESS")

    logger.debug("Unexpected initial response when waiting for comeback response")
    query, dialog_token = init_gas(hapd, bssid, dev[0])
    resp = action_response(query)
    resp['payload'] = anqp_initial_resp(dialog_token, 0) + struct.pack('<H', 0)
    send_gas_resp(hapd, resp)
    allow_gas_initial_req(hapd, dialog_token)
    expect_gas_result(dev[0], "TIMEOUT")

    logger.debug("Too short comeback response")
    query, dialog_token = init_gas(hapd, bssid, dev[0])
    resp = action_response(query)
    resp['payload'] = struct.pack('<BBBH', ACTION_CATEG_PUBLIC,
                                  GAS_COMEBACK_RESPONSE, dialog_token, 0)
    send_gas_resp(hapd, resp)
    allow_gas_initial_req(hapd, dialog_token)
    expect_gas_result(dev[0], "TIMEOUT")

    logger.debug("Too short comeback response(2)")
    query, dialog_token = init_gas(hapd, bssid, dev[0])
    resp = action_response(query)
    resp['payload'] = struct.pack('<BBBHBB', ACTION_CATEG_PUBLIC,
                                  GAS_COMEBACK_RESPONSE, dialog_token, 0, 0x80,
                                  0)
    send_gas_resp(hapd, resp)
    allow_gas_initial_req(hapd, dialog_token)
    expect_gas_result(dev[0], "TIMEOUT")

    logger.debug("Maximum comeback response fragment claiming more fragments")
    query, dialog_token = init_gas(hapd, bssid, dev[0])
    resp = action_response(query)
    resp['payload'] = anqp_comeback_resp(dialog_token, more=True) + struct.pack('<H', 0)
    send_gas_resp(hapd, resp)
    for i in range(1, 129):
        query = gas_rx(hapd)
        gas = parse_gas(query['payload'])
        if gas['action'] != GAS_COMEBACK_REQUEST:
            raise Exception("Unexpected request action")
        if gas['dialog_token'] != dialog_token:
            raise Exception("Unexpected dialog token change")
        resp = action_response(query)
        resp['payload'] = anqp_comeback_resp(dialog_token, id=i, more=True) + struct.pack('<H', 0)
        send_gas_resp(hapd, resp)
    expect_gas_result(dev[0], "PEER_ERROR")

def test_gas_comeback_resp_additional_delay(dev, apdev):
    """GAS comeback response requesting additional delay"""
    hapd = start_ap(apdev[0])
    bssid = apdev[0]['bssid']

    dev[0].scan_for_bss(bssid, freq="2412", force_scan=True)
    hapd.set("ext_mgmt_frame_handling", "1")

    query, dialog_token = init_gas(hapd, bssid, dev[0])
    for i in range(0, 2):
        resp = action_response(query)
        resp['payload'] = anqp_comeback_resp(dialog_token, status_code=95, comeback_delay=50) + struct.pack('<H', 0)
        send_gas_resp(hapd, resp)
        query = gas_rx(hapd)
        gas = parse_gas(query['payload'])
        if gas['action'] != GAS_COMEBACK_REQUEST:
            raise Exception("Unexpected request action")
        if gas['dialog_token'] != dialog_token:
            raise Exception("Unexpected dialog token change")
    resp = action_response(query)
    resp['payload'] = anqp_comeback_resp(dialog_token, status_code=0) + struct.pack('<H', 0)
    send_gas_resp(hapd, resp)
    expect_gas_result(dev[0], "SUCCESS")

def test_gas_unknown_adv_proto(dev, apdev):
    """Unknown advertisement protocol id"""
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid
    hostapd.add_ap(apdev[0], params)

    dev[0].scan_for_bss(bssid, freq="2412", force_scan=True)
    req = dev[0].request("GAS_REQUEST " + bssid + " 42 000102000101")
    if "FAIL" in req:
        raise Exception("GAS query request rejected")
    expect_gas_result(dev[0], "FAILURE", "59")
    ev = dev[0].wait_event(["GAS-RESPONSE-INFO"], timeout=10)
    if ev is None:
        raise Exception("GAS query timed out")
    exp = r'<.>(GAS-RESPONSE-INFO) addr=([0-9a-f:]*) dialog_token=([0-9]*) status_code=([0-9]*) resp_len=([\-0-9]*)'
    res = re.split(exp, ev)
    if len(res) < 6:
        raise Exception("Could not parse GAS-RESPONSE-INFO")
    if res[2] != bssid:
        raise Exception("Unexpected BSSID in response")
    status = res[4]
    if status != "59":
        raise Exception("Unexpected GAS-RESPONSE-INFO status")

def test_gas_request_oom(dev, apdev):
    """GAS_REQUEST OOM"""
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid
    hostapd.add_ap(apdev[0], params)

    dev[0].scan_for_bss(bssid, freq="2412", force_scan=True)

    with alloc_fail(dev[0], 1, "gas_build_req;gas_send_request"):
        if "FAIL" not in dev[0].request("GAS_REQUEST " + bssid + " 42"):
            raise Exception("GAS query request rejected")

    with alloc_fail(dev[0], 1, "gas_query_req;gas_send_request"):
        if "FAIL" not in dev[0].request("GAS_REQUEST " + bssid + " 42"):
            raise Exception("GAS query request rejected")

    with alloc_fail(dev[0], 1, "wpabuf_dup;gas_resp_cb"):
        if "OK" not in dev[0].request("GAS_REQUEST " + bssid + " 00 000102000101"):
            raise Exception("GAS query request rejected")
        ev = dev[0].wait_event(["GAS-RESPONSE-INFO"], timeout=10)
        if ev is None:
            raise Exception("No GAS response")
        if "status_code=0" not in ev:
            raise Exception("GAS response indicated a failure")

def test_gas_max_pending(dev, apdev):
    """GAS and maximum pending query limit"""
    hapd = start_ap(apdev[0])
    hapd.set("gas_frag_limit", "50")
    bssid = apdev[0]['bssid']

    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    wpas.interface_add("wlan5")
    if "OK" not in wpas.request("P2P_SET listen_channel 1"):
        raise Exception("Failed to set listen channel")
    if "OK" not in wpas.p2p_listen():
        raise Exception("Failed to start listen state")
    if "FAIL" in wpas.request("SET ext_mgmt_frame_handling 1"):
        raise Exception("Failed to enable external management frame handling")

    anqp_query = struct.pack('<HHHHHHHHHH', 256, 16, 257, 258, 260, 261, 262, 263, 264, 268)
    gas = struct.pack('<H', len(anqp_query)) + anqp_query

    for dialog_token in range(1, 10):
        msg = struct.pack('<BBB', ACTION_CATEG_PUBLIC, GAS_INITIAL_REQUEST,
                          dialog_token) + anqp_adv_proto() + gas
        req = "MGMT_TX {} {} freq=2412 wait_time=10 action={}".format(bssid, bssid, binascii.hexlify(msg).decode())
        if "OK" not in wpas.request(req):
            raise Exception("Could not send management frame")
        resp = wpas.mgmt_rx()
        if resp is None:
            raise Exception("MGMT-RX timeout")
        if 'payload' not in resp:
            raise Exception("Missing payload")
        gresp = parse_gas(resp['payload'])
        if gresp['dialog_token'] != dialog_token:
            raise Exception("Dialog token mismatch")
        status_code = gresp['status_code']
        if dialog_token < 9 and status_code != 0:
            raise Exception("Unexpected failure status code {} for dialog token {}".format(status_code, dialog_token))
        if dialog_token > 8 and status_code == 0:
            raise Exception("Unexpected success status code {} for dialog token {}".format(status_code, dialog_token))

def test_gas_no_pending(dev, apdev):
    """GAS and no pending query for comeback request"""
    hapd = start_ap(apdev[0])
    bssid = apdev[0]['bssid']

    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    wpas.interface_add("wlan5")
    if "OK" not in wpas.request("P2P_SET listen_channel 1"):
        raise Exception("Failed to set listen channel")
    if "OK" not in wpas.p2p_listen():
        raise Exception("Failed to start listen state")
    if "FAIL" in wpas.request("SET ext_mgmt_frame_handling 1"):
        raise Exception("Failed to enable external management frame handling")

    msg = struct.pack('<BBB', ACTION_CATEG_PUBLIC, GAS_COMEBACK_REQUEST, 1)
    req = "MGMT_TX {} {} freq=2412 wait_time=10 action={}".format(bssid, bssid, binascii.hexlify(msg).decode())
    if "OK" not in wpas.request(req):
        raise Exception("Could not send management frame")
    resp = wpas.mgmt_rx()
    if resp is None:
        raise Exception("MGMT-RX timeout")
    if 'payload' not in resp:
        raise Exception("Missing payload")
    gresp = parse_gas(resp['payload'])
    status_code = gresp['status_code']
    if status_code != 60:
        raise Exception("Unexpected status code {} (expected 60)".format(status_code))

def test_gas_delete_at_deinit(dev, apdev):
    """GAS query deleted at deinit"""
    hapd = start_ap(apdev[0])
    hapd.set("gas_comeback_delay", "1000")
    bssid = apdev[0]['bssid']

    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    wpas.interface_add("wlan5")
    wpas.scan_for_bss(apdev[0]['bssid'], freq="2412", force_scan=True)
    wpas.request("ANQP_GET " + bssid + " 258")

    wpas.global_request("INTERFACE_REMOVE " + wpas.ifname)
    ev = wpas.wait_event(["GAS-QUERY-DONE"], timeout=2)
    del wpas
    if ev is None:
        raise Exception("GAS-QUERY-DONE not seen")
    if "result=DELETED_AT_DEINIT" not in ev:
        raise Exception("Unexpected result code: " + ev)

def test_gas_missing_payload(dev, apdev):
    """No action code in the query frame"""
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid
    hostapd.add_ap(apdev[0], params)

    dev[0].scan_for_bss(bssid, freq="2412", force_scan=True)

    cmd = "MGMT_TX {} {} freq=2412 action=040A".format(bssid, bssid)
    if "FAIL" in dev[0].request(cmd):
        raise Exception("Could not send test Action frame")
    ev = dev[0].wait_event(["MGMT-TX-STATUS"], timeout=10)
    if ev is None:
        raise Exception("Timeout on MGMT-TX-STATUS")
    if "result=SUCCESS" not in ev:
        raise Exception("AP did not ack Action frame")

    cmd = "MGMT_TX {} {} freq=2412 action=04".format(bssid, bssid)
    if "FAIL" in dev[0].request(cmd):
        raise Exception("Could not send test Action frame")
    ev = dev[0].wait_event(["MGMT-TX-STATUS"], timeout=10)
    if ev is None:
        raise Exception("Timeout on MGMT-TX-STATUS")
    if "result=SUCCESS" not in ev:
        raise Exception("AP did not ack Action frame")

def test_gas_query_deinit(dev, apdev):
    """Pending GAS/ANQP query during deinit"""
    hapd = start_ap(apdev[0])
    bssid = apdev[0]['bssid']

    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    wpas.interface_add("wlan5")

    wpas.scan_for_bss(bssid, freq="2412", force_scan=True)
    id = wpas.request("RADIO_WORK add block-work")
    if "OK" not in wpas.request("ANQP_GET " + bssid + " 258"):
        raise Exception("ANQP_GET command failed")

    ev = wpas.wait_event(["GAS-QUERY-START", "EXT-RADIO-WORK-START"], timeout=5)
    if ev is None:
        raise Exception("Timeout while waiting radio work to start")
    ev = wpas.wait_event(["GAS-QUERY-START", "EXT-RADIO-WORK-START"], timeout=5)
    if ev is None:
        raise Exception("Timeout while waiting radio work to start (2)")

    # Remove the interface while the gas-query radio work is still pending and
    # GAS query has not yet been started.
    wpas.interface_remove("wlan5")

@remote_compatible
def test_gas_anqp_oom_wpas(dev, apdev):
    """GAS/ANQP query and OOM in wpa_supplicant"""
    hapd = start_ap(apdev[0])
    bssid = apdev[0]['bssid']

    dev[0].scan_for_bss(bssid, freq="2412", force_scan=True)

    with alloc_fail(dev[0], 1, "wpa_bss_anqp_alloc"):
        if "OK" not in dev[0].request("ANQP_GET " + bssid + " 258"):
            raise Exception("ANQP_GET command failed")
        ev = dev[0].wait_event(["ANQP-QUERY-DONE"], timeout=5)
        if ev is None:
            raise Exception("ANQP query did not complete")

    with alloc_fail(dev[0], 1, "gas_build_req"):
        if "FAIL" not in dev[0].request("ANQP_GET " + bssid + " 258"):
            raise Exception("Unexpected ANQP_GET command success (OOM)")

def test_gas_anqp_oom_hapd(dev, apdev):
    """GAS/ANQP query and OOM in hostapd"""
    hapd = start_ap(apdev[0])
    bssid = apdev[0]['bssid']

    dev[0].scan_for_bss(bssid, freq="2412", force_scan=True)

    with alloc_fail(hapd, 1, "gas_build_resp"):
        # This query will time out due to the AP not sending a response (OOM).
        if "OK" not in dev[0].request("ANQP_GET " + bssid + " 258"):
            raise Exception("ANQP_GET command failed")

        ev = dev[0].wait_event(["GAS-QUERY-START"], timeout=5)
        if ev is None:
            raise Exception("GAS query start timed out")

        ev = dev[0].wait_event(["GAS-QUERY-DONE"], timeout=10)
        if ev is None:
            raise Exception("GAS query timed out")
        if "result=TIMEOUT" not in ev:
            raise Exception("Unexpected result: " + ev)

        ev = dev[0].wait_event(["ANQP-QUERY-DONE"], timeout=10)
        if ev is None:
            raise Exception("ANQP-QUERY-DONE event not seen")
        if "result=FAILURE" not in ev:
            raise Exception("Unexpected result: " + ev)

    with alloc_fail(hapd, 1, "gas_anqp_build_comeback_resp"):
        hapd.set("gas_frag_limit", "50")

        # The first attempt of this query will time out due to the AP not
        # sending a response (OOM), but the retry succeeds.
        dev[0].request("FETCH_ANQP")
        ev = dev[0].wait_event(["GAS-QUERY-START"], timeout=5)
        if ev is None:
            raise Exception("GAS query start timed out")

        ev = dev[0].wait_event(["GAS-QUERY-DONE"], timeout=10)
        if ev is None:
            raise Exception("GAS query timed out")
        if "result=SUCCESS" not in ev:
            raise Exception("Unexpected result: " + ev)

        ev = dev[0].wait_event(["ANQP-QUERY-DONE"], timeout=10)
        if ev is None:
            raise Exception("ANQP-QUERY-DONE event not seen")
        if "result=SUCCESS" not in ev:
            raise Exception("Unexpected result: " + ev)

def test_gas_anqp_extra_elements(dev, apdev):
    """GAS/ANQP and extra ANQP elements"""
    geo_loc = "001052834d12efd2b08b9b4bf1cc2c00004104050000000000060100"
    civic_loc = "0000f9555302f50102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f606162636465666768696a6b6c6d6e6f707172737475767778797a7b7c7d7e7f808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9fa0a1a2a3a4a5a6a7a8a9aaabacadaeafb0b1b2b3b4b5b6b7b8b9babbbcbdbebfc0c1c2c3c4c5c6c7c8c9cacbcccdcecfd0d1d2d3d4d5d6d7d8d9dadbdcdddedfe0e1e2e3e4e5e6e7e8e9eaebecedeeeff0f1f2f3f4f5"
    held_uri = "https://held.example.com/location"
    held = struct.pack('BBB', 0, 1 + len(held_uri), 1) + held_uri.encode()
    supl_fqdn = "supl.example.com"
    supl = struct.pack('BBB', 0, 1 + len(supl_fqdn), 1) + supl_fqdn.encode()
    public_id = binascii.hexlify(held + supl).decode()
    params = {"ssid": "gas/anqp",
              "interworking": "1",
              "anqp_elem": ["265:" + geo_loc,
                            "266:" + civic_loc,
                            "262:1122334455",
                            "267:" + public_id,
                            "279:01020304",
                            "60000:01",
                            "299:0102"]}
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = apdev[0]['bssid']

    dev[0].flush_scan_cache()
    dev[0].scan_for_bss(bssid, freq="2412", force_scan=True)
    if "OK" not in dev[0].request("ANQP_GET " + bssid + " 265,266"):
        raise Exception("ANQP_GET command failed")

    ev = dev[0].wait_event(["GAS-QUERY-DONE"], timeout=10)
    if ev is None:
        raise Exception("GAS query timed out")

    bss = dev[0].get_bss(bssid)

    if 'anqp[265]' not in bss:
        raise Exception("AP Geospatial Location ANQP-element not seen")
    if bss['anqp[265]'] != geo_loc:
        raise Exception("Unexpected AP Geospatial Location ANQP-element value: " + bss['anqp[265]'])

    if 'anqp[266]' not in bss:
        raise Exception("AP Civic Location ANQP-element not seen")
    if bss['anqp[266]'] != civic_loc:
        raise Exception("Unexpected AP Civic Location ANQP-element value: " + bss['anqp[266]'])

    dev[1].scan_for_bss(bssid, freq="2412", force_scan=True)
    if "OK" not in dev[1].request("ANQP_GET " + bssid + " 257,258,259,260,261,262,263,264,265,267,268,269,270,271,272,273,274,275,276,277,278,279,280,281,282,283,284,285,286,287,288,289,290,291,292,293,294,295,296,297,298,299"):
        raise Exception("ANQP_GET command failed")

    ev = dev[1].wait_event(["GAS-QUERY-DONE"], timeout=10)
    if ev is None:
        raise Exception("GAS query timed out")

    bss = dev[1].get_bss(bssid)

    if 'anqp[265]' not in bss:
        raise Exception("AP Geospatial Location ANQP-element not seen")
    if bss['anqp[265]'] != geo_loc:
        raise Exception("Unexpected AP Geospatial Location ANQP-element value: " + bss['anqp[265]'])

    if 'anqp[266]' in bss:
        raise Exception("AP Civic Location ANQP-element unexpectedly seen")

    if 'anqp[267]' not in bss:
        raise Exception("AP Location Public Identifier ANQP-element not seen")
    if bss['anqp[267]'] != public_id:
        raise Exception("Unexpected AP Location Public Identifier ANQP-element value: " + bss['anqp[267]'])

    if 'anqp[279]' not in bss:
        raise Exception("ANQP-element Info ID 279 not seen")
    if bss['anqp[279]'] != "01020304":
        raise Exception("Unexpected AP ANQP-element Info ID 279 value: " + bss['anqp[279]'])

    if 'anqp[299]' not in bss:
        raise Exception("ANQP-element Info ID 299 not seen")
    if bss['anqp[299]'] != "0102":
        raise Exception("Unexpected AP ANQP-element Info ID 299 value: " + bss['anqp[299]'])

    if 'anqp_ip_addr_type_availability' not in bss:
        raise Exception("ANQP-element Info ID 292 not seen")
    if bss['anqp_ip_addr_type_availability'] != "1122334455":
        raise Exception("Unexpected AP ANQP-element Info ID 262 value: " + bss['anqp_ip_addr_type_availability'])

def test_gas_anqp_address3_not_assoc(dev, apdev, params):
    """GAS/ANQP query using IEEE 802.11 compliant Address 3 value when not associated"""
    try:
        _test_gas_anqp_address3_not_assoc(dev, apdev, params)
    finally:
        dev[0].request("SET gas_address3 0")

def _test_gas_anqp_address3_not_assoc(dev, apdev, params):
    hapd = start_ap(apdev[0])
    bssid = apdev[0]['bssid']

    if "OK" not in dev[0].request("SET gas_address3 1"):
        raise Exception("Failed to set gas_address3")

    dev[0].scan_for_bss(bssid, freq="2412", force_scan=True)
    if "OK" not in dev[0].request("ANQP_GET " + bssid + " 258"):
        raise Exception("ANQP_GET command failed")

    ev = dev[0].wait_event(["GAS-QUERY-START"], timeout=5)
    if ev is None:
        raise Exception("GAS query start timed out")

    ev = dev[0].wait_event(["GAS-QUERY-DONE"], timeout=10)
    if ev is None:
        raise Exception("GAS query timed out")

    ev = dev[0].wait_event(["RX-ANQP"], timeout=1)
    if ev is None or "Venue Name" not in ev:
        raise Exception("Did not receive Venue Name")

    ev = dev[0].wait_event(["ANQP-QUERY-DONE"], timeout=10)
    if ev is None:
        raise Exception("ANQP-QUERY-DONE event not seen")
    if "result=SUCCESS" not in ev:
        raise Exception("Unexpected result: " + ev)

    out = run_tshark(os.path.join(params['logdir'], "hwsim0.pcapng"),
                     "wlan_mgt.fixed.category_code == 4 && (wlan_mgt.fixed.publicact == 0x0a || wlan_mgt.fixed.publicact == 0x0b)",
                     display=["wlan.bssid"])
    res = out.splitlines()
    if len(res) != 2:
        raise Exception("Unexpected number of GAS frames")
    if res[0] != 'ff:ff:ff:ff:ff:ff':
        raise Exception("GAS request used unexpected Address3 field value: " + res[0])
    if res[1] != 'ff:ff:ff:ff:ff:ff':
        raise Exception("GAS response used unexpected Address3 field value: " + res[1])

def test_gas_anqp_address3_assoc(dev, apdev, params):
    """GAS/ANQP query using IEEE 802.11 compliant Address 3 value when associated"""
    try:
        _test_gas_anqp_address3_assoc(dev, apdev, params)
    finally:
        dev[0].request("SET gas_address3 0")

def _test_gas_anqp_address3_assoc(dev, apdev, params):
    hapd = start_ap(apdev[0])
    bssid = apdev[0]['bssid']

    if "OK" not in dev[0].request("SET gas_address3 1"):
        raise Exception("Failed to set gas_address3")

    dev[0].scan_for_bss(bssid, freq="2412")
    dev[0].connect("test-gas", key_mgmt="WPA-EAP", eap="TTLS",
                   identity="DOMAIN\mschapv2 user", anonymous_identity="ttls",
                   password="password", phase2="auth=MSCHAPV2",
                   ca_cert="auth_serv/ca.pem", scan_freq="2412")

    if "OK" not in dev[0].request("ANQP_GET " + bssid + " 258"):
        raise Exception("ANQP_GET command failed")

    ev = dev[0].wait_event(["GAS-QUERY-START"], timeout=5)
    if ev is None:
        raise Exception("GAS query start timed out")

    ev = dev[0].wait_event(["GAS-QUERY-DONE"], timeout=10)
    if ev is None:
        raise Exception("GAS query timed out")

    ev = dev[0].wait_event(["RX-ANQP"], timeout=1)
    if ev is None or "Venue Name" not in ev:
        raise Exception("Did not receive Venue Name")

    ev = dev[0].wait_event(["ANQP-QUERY-DONE"], timeout=10)
    if ev is None:
        raise Exception("ANQP-QUERY-DONE event not seen")
    if "result=SUCCESS" not in ev:
        raise Exception("Unexpected result: " + ev)

    out = run_tshark(os.path.join(params['logdir'], "hwsim0.pcapng"),
                     "wlan_mgt.fixed.category_code == 4 && (wlan_mgt.fixed.publicact == 0x0a || wlan_mgt.fixed.publicact == 0x0b)",
                     display=["wlan.bssid"])
    res = out.splitlines()
    if len(res) != 2:
        raise Exception("Unexpected number of GAS frames")
    if res[0] != bssid:
        raise Exception("GAS request used unexpected Address3 field value: " + res[0])
    if res[1] != bssid:
        raise Exception("GAS response used unexpected Address3 field value: " + res[1])

def test_gas_anqp_address3_ap_forced(dev, apdev, params):
    """GAS/ANQP query using IEEE 802.11 compliant Address 3 value on AP"""
    hapd = start_ap(apdev[0])
    bssid = apdev[0]['bssid']
    hapd.set("gas_address3", "1")

    dev[0].scan_for_bss(bssid, freq="2412", force_scan=True)
    if "OK" not in dev[0].request("ANQP_GET " + bssid + " 258"):
        raise Exception("ANQP_GET command failed")

    ev = dev[0].wait_event(["GAS-QUERY-START"], timeout=5)
    if ev is None:
        raise Exception("GAS query start timed out")

    ev = dev[0].wait_event(["GAS-QUERY-DONE"], timeout=10)
    if ev is None:
        raise Exception("GAS query timed out")

    ev = dev[0].wait_event(["RX-ANQP"], timeout=1)
    if ev is None or "Venue Name" not in ev:
        raise Exception("Did not receive Venue Name")

    ev = dev[0].wait_event(["ANQP-QUERY-DONE"], timeout=10)
    if ev is None:
        raise Exception("ANQP-QUERY-DONE event not seen")
    if "result=SUCCESS" not in ev:
        raise Exception("Unexpected result: " + ev)

    out = run_tshark(os.path.join(params['logdir'], "hwsim0.pcapng"),
                     "wlan_mgt.fixed.category_code == 4 && (wlan_mgt.fixed.publicact == 0x0a || wlan_mgt.fixed.publicact == 0x0b)",
                     display=["wlan.bssid"])
    res = out.splitlines()
    if len(res) != 2:
        raise Exception("Unexpected number of GAS frames")
    if res[0] != bssid:
        raise Exception("GAS request used unexpected Address3 field value: " + res[0])
    if res[1] != 'ff:ff:ff:ff:ff:ff':
        raise Exception("GAS response used unexpected Address3 field value: " + res[1])

def test_gas_anqp_address3_ap_non_compliant(dev, apdev, params):
    """GAS/ANQP query using IEEE 802.11 non-compliant Address 3 (AP)"""
    try:
        _test_gas_anqp_address3_ap_non_compliant(dev, apdev, params)
    finally:
        dev[0].request("SET gas_address3 0")

def _test_gas_anqp_address3_ap_non_compliant(dev, apdev, params):
    hapd = start_ap(apdev[0])
    bssid = apdev[0]['bssid']
    hapd.set("gas_address3", "2")

    if "OK" not in dev[0].request("SET gas_address3 1"):
        raise Exception("Failed to set gas_address3")

    dev[0].scan_for_bss(bssid, freq="2412", force_scan=True)
    if "OK" not in dev[0].request("ANQP_GET " + bssid + " 258"):
        raise Exception("ANQP_GET command failed")

    ev = dev[0].wait_event(["GAS-QUERY-START"], timeout=5)
    if ev is None:
        raise Exception("GAS query start timed out")

    ev = dev[0].wait_event(["GAS-QUERY-DONE"], timeout=10)
    if ev is None:
        raise Exception("GAS query timed out")

    ev = dev[0].wait_event(["RX-ANQP"], timeout=1)
    if ev is None or "Venue Name" not in ev:
        raise Exception("Did not receive Venue Name")

    ev = dev[0].wait_event(["ANQP-QUERY-DONE"], timeout=10)
    if ev is None:
        raise Exception("ANQP-QUERY-DONE event not seen")
    if "result=SUCCESS" not in ev:
        raise Exception("Unexpected result: " + ev)

    out = run_tshark(os.path.join(params['logdir'], "hwsim0.pcapng"),
                     "wlan_mgt.fixed.category_code == 4 && (wlan_mgt.fixed.publicact == 0x0a || wlan_mgt.fixed.publicact == 0x0b)",
                     display=["wlan.bssid"])
    res = out.splitlines()
    if len(res) != 2:
        raise Exception("Unexpected number of GAS frames")
    if res[0] != 'ff:ff:ff:ff:ff:ff':
        raise Exception("GAS request used unexpected Address3 field value: " + res[0])
    if res[1] != bssid:
        raise Exception("GAS response used unexpected Address3 field value: " + res[1])

def test_gas_anqp_address3_pmf(dev, apdev):
    """GAS/ANQP query using IEEE 802.11 compliant Address 3 value with PMF"""
    try:
        _test_gas_anqp_address3_pmf(dev, apdev)
    finally:
        dev[0].request("SET gas_address3 0")

def _test_gas_anqp_address3_pmf(dev, apdev):
    hapd = start_ap(apdev[0])
    bssid = apdev[0]['bssid']
    hapd.set("gas_comeback_delay", "2")
    hapd.set("gas_address3", "1")

    if "OK" not in dev[0].request("SET gas_address3 1"):
        raise Exception("Failed to set gas_address3")

    dev[0].scan_for_bss(bssid, freq="2412")
    dev[0].connect("test-gas", key_mgmt="WPA-EAP", eap="TTLS",
                   identity="DOMAIN\mschapv2 user", anonymous_identity="ttls",
                   password="password", phase2="auth=MSCHAPV2",
                   ca_cert="auth_serv/ca.pem", scan_freq="2412",
                   ieee80211w="2")

    if "OK" not in dev[0].request("ANQP_GET " + bssid + " 258"):
        raise Exception("ANQP_GET command failed")

    ev = dev[0].wait_event(["GAS-QUERY-START"], timeout=5)
    if ev is None:
        raise Exception("GAS query start timed out")

    ev = dev[0].wait_event(["GAS-QUERY-DONE"], timeout=10)
    if ev is None:
        raise Exception("GAS query timed out")

    ev = dev[0].wait_event(["RX-ANQP"], timeout=1)
    if ev is None or "Venue Name" not in ev:
        raise Exception("Did not receive Venue Name")

    ev = dev[0].wait_event(["ANQP-QUERY-DONE"], timeout=10)
    if ev is None:
        raise Exception("ANQP-QUERY-DONE event not seen")
    if "result=SUCCESS" not in ev:
        raise Exception("Unexpected result: " + ev)

    req = dev[0].request("GAS_REQUEST " + bssid + " 42 000102000101")
    if "FAIL" in req:
        raise Exception("GAS query request rejected")
    expect_gas_result(dev[0], "FAILURE", "59")

def test_gas_prot_vs_not_prot(dev, apdev, params):
    """GAS/ANQP query protected vs. not protected"""
    hapd = start_ap(apdev[0])
    bssid = apdev[0]['bssid']

    dev[0].scan_for_bss(bssid, freq="2412")
    dev[0].connect("test-gas", key_mgmt="WPA-EAP", eap="TTLS",
                   identity="DOMAIN\mschapv2 user", anonymous_identity="ttls",
                   password="password", phase2="auth=MSCHAPV2",
                   ca_cert="auth_serv/ca.pem", scan_freq="2412",
                   ieee80211w="2")

    if "OK" not in dev[0].request("ANQP_GET " + bssid + " 258"):
        raise Exception("ANQP_GET command failed")

    ev = dev[0].wait_event(["GAS-QUERY-DONE"], timeout=5)
    if ev is None:
        raise Exception("No GAS-QUERY-DONE event")
    if "result=SUCCESS" not in ev:
        raise Exception("Unexpected GAS result: " + ev)

    # GAS: Drop unexpected unprotected GAS frame when PMF is enabled
    dev[0].request("SET ext_mgmt_frame_handling 1")
    res = dev[0].request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=d0003a010200000000000200000003000200000003001000040b00000005006c027f000000")
    dev[0].request("SET ext_mgmt_frame_handling 0")
    if "OK" not in res:
        raise Exception("MGMT_RX_PROCESS failed")

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    # GAS: No pending query found for 02:00:00:00:03:00 dialog token 0
    dev[0].request("SET ext_mgmt_frame_handling 1")
    res = dev[0].request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=d0003a010200000000000200000003000200000003001000040b00000005006c027f000000")
    dev[0].request("SET ext_mgmt_frame_handling 0")
    if "OK" not in res:
        raise Exception("MGMT_RX_PROCESS failed")

    # GAS: Drop unexpected protected GAS frame when PMF is disabled
    dev[0].request("SET ext_mgmt_frame_handling 1")
    res = dev[0].request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=d0003a010200000000000200000003000200000003001000090b00000005006c027f000000")
    dev[0].request("SET ext_mgmt_frame_handling 0")
    if "OK" not in res:
        raise Exception("MGMT_RX_PROCESS failed")

def test_gas_failures(dev, apdev):
    """GAS failure cases"""
    hapd = start_ap(apdev[0])
    hapd.set("gas_comeback_delay", "5")
    bssid = apdev[0]['bssid']

    hapd2 = start_ap(apdev[1])
    bssid2 = apdev[1]['bssid']

    dev[0].scan_for_bss(bssid, freq="2412", force_scan=True)
    dev[0].scan_for_bss(bssid2, freq="2412")

    tests = [(bssid, "gas_build_req;gas_query_tx_comeback_req"),
             (bssid, "gas_query_tx;gas_query_tx_comeback_req"),
             (bssid, "gas_query_append;gas_query_rx_comeback"),
             (bssid2, "gas_query_append;gas_query_rx_initial"),
             (bssid2, "wpabuf_alloc_copy;gas_query_rx_initial"),
             (bssid, "gas_query_tx;gas_query_tx_initial_req")]
    for addr, func in tests:
        with alloc_fail(dev[0], 1, func):
            dev[0].request("ANQP_GET " + addr + " 258")
            ev = dev[0].wait_event(["GAS-QUERY-DONE"], timeout=5)
            if ev is None:
                raise Exception("No GAS-QUERY-DONE seen")
            if "result=INTERNAL_ERROR" not in ev:
                raise Exception("Unexpected result code: " + ev)
        dev[0].dump_monitor()

    tests = ["=gas_query_req", "radio_add_work;gas_query_req"]
    for func in tests:
        with alloc_fail(dev[0], 1, func):
            if "FAIL" not in dev[0].request("ANQP_GET " + bssid + " 258"):
                raise Exception("ANQP_GET succeeded unexpectedly during OOM")
        dev[0].dump_monitor()

    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    wpas.interface_add("wlan5")
    wpas.scan_for_bss(bssid2, freq="2412")
    wpas.request("SET preassoc_mac_addr 1111")
    wpas.request("ANQP_GET " + bssid2 + " 258")
    ev = wpas.wait_event(["Failed to assign random MAC address for GAS"],
                         timeout=5)
    wpas.request("SET preassoc_mac_addr 0")
    if ev is None:
        raise Exception("No random MAC address error seen")

def test_gas_anqp_venue_url(dev, apdev):
    """GAS/ANQP and Venue URL"""
    venue_group = 1
    venue_type = 13
    venue_info = struct.pack('BB', venue_group, venue_type)
    lang1 = "eng"
    name1 = "Example venue"
    lang2 = "fin"
    name2 = "Esimerkkipaikka"
    venue1 = struct.pack('B', len(lang1 + name1)) + lang1.encode() + name1.encode()
    venue2 = struct.pack('B', len(lang2 + name2)) + lang2.encode() + name2.encode()
    venue_name = binascii.hexlify(venue_info + venue1 + venue2).decode()

    url1 = b"http://example.com/venue"
    url2 = b"https://example.org/venue-info/"
    duple1 = struct.pack('BB', 1 + len(url1), 1) + url1
    duple2 = struct.pack('BB', 1 + len(url2), 2) + url2
    venue_url = binascii.hexlify(duple1 + duple2).decode()

    params = {"ssid": "gas/anqp",
              "interworking": "1",
              "venue_group": str(venue_group),
              "venue_type": str(venue_type),
              "venue_name": [lang1 + ":" + name1, lang2 + ":" + name2],
              "anqp_elem": ["277:" + venue_url]}
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = apdev[0]['bssid']

    dev[0].scan_for_bss(bssid, freq="2412", force_scan=True)
    if "OK" not in dev[0].request("ANQP_GET " + bssid + " 257,258,277"):
        raise Exception("ANQP_GET command failed")

    ev = dev[0].wait_event(["GAS-QUERY-DONE"], timeout=10)
    if ev is None:
        raise Exception("GAS query timed out")

    ev = dev[0].wait_event(["RX-VENUE-URL"], timeout=0.1)
    if ev is not None:
        raise Exception("Unexpected Venue URL indication without PMF")

    bss = dev[0].get_bss(bssid)

    if 'anqp_venue_name' not in bss:
        raise Exception("Venue Name ANQP-element not seen")
    if bss['anqp_venue_name'] != venue_name:
        raise Exception("Unexpected Venue Name ANQP-element value: " + bss['anqp_venue_name'])
    if 'anqp[277]' not in bss:
        raise Exception("Venue URL ANQP-element not seen")
    if bss['anqp[277]'] != venue_url:
        raise Exception("Unexpected Venue URL ANQP-element value: " + bss['anqp[277]'])

    if 'anqp_capability_list' not in bss:
        raise Exception("Capability List ANQP-element not seen")
    ids = struct.pack('<HHH', 257, 258, 277)
    if not bss['anqp_capability_list'].startswith(binascii.hexlify(ids).decode()):
        raise Exception("Unexpected Capability List ANQP-element value: " + bss['anqp_capability_list'])

    if "anqp[277]" not in bss:
        raise Exception("Venue-URL ANQP info not available")
    if "protected-anqp-info[277]" in bss:
        raise Exception("Unexpected Venue-URL protection info")

def test_gas_anqp_venue_url2(dev, apdev):
    """GAS/ANQP and Venue URL (hostapd venue_url)"""
    venue_group = 1
    venue_type = 13
    venue_info = struct.pack('BB', venue_group, venue_type)
    lang1 = "eng"
    name1 = "Example venue"
    lang2 = "fin"
    name2 = "Esimerkkipaikka"
    venue1 = struct.pack('B', len(lang1 + name1)) + lang1.encode() + name1.encode()
    venue2 = struct.pack('B', len(lang2 + name2)) + lang2.encode() + name2.encode()
    venue_name = binascii.hexlify(venue_info + venue1 + venue2).decode()

    url1 = "http://example.com/venue"
    url2 = "https://example.org/venue-info/"
    duple1 = struct.pack('BB', 1 + len(url1.encode()), 1) + url1.encode()
    duple2 = struct.pack('BB', 1 + len(url2.encode()), 2) + url2.encode()
    venue_url = binascii.hexlify(duple1 + duple2).decode()

    params = {"ssid": "gas/anqp",
              "interworking": "1",
              "venue_group": str(venue_group),
              "venue_type": str(venue_type),
              "venue_name": [lang1 + ":" + name1, lang2 + ":" + name2],
              "venue_url": ["1:" + url1, "2:" + url2]}
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = apdev[0]['bssid']

    dev[0].scan_for_bss(bssid, freq="2412", force_scan=True)
    if "OK" not in dev[0].request("ANQP_GET " + bssid + " 257,258,277"):
        raise Exception("ANQP_GET command failed")

    ev = dev[0].wait_event(["GAS-QUERY-DONE"], timeout=10)
    if ev is None:
        raise Exception("GAS query timed out")

    bss = dev[0].get_bss(bssid)

    if 'anqp_venue_name' not in bss:
        raise Exception("Venue Name ANQP-element not seen")
    if bss['anqp_venue_name'] != venue_name:
        raise Exception("Unexpected Venue Name ANQP-element value: " + bss['anqp_venue_name'])
    if 'anqp[277]' not in bss:
        raise Exception("Venue URL ANQP-element not seen")
    if bss['anqp[277]'] != venue_url:
        print(venue_url)
        raise Exception("Unexpected Venue URL ANQP-element value: " + bss['anqp[277]'])

    if 'anqp_capability_list' not in bss:
        raise Exception("Capability List ANQP-element not seen")
    ids = struct.pack('<HHH', 257, 258, 277)
    if not bss['anqp_capability_list'].startswith(binascii.hexlify(ids).decode()):
        raise Exception("Unexpected Capability List ANQP-element value: " + bss['anqp_capability_list'])

def test_gas_anqp_venue_url_pmf(dev, apdev):
    """GAS/ANQP and Venue URL with PMF"""
    venue_group = 1
    venue_type = 13
    venue_info = struct.pack('BB', venue_group, venue_type)
    lang1 = "eng"
    name1 = "Example venue"
    lang2 = "fin"
    name2 = "Esimerkkipaikka"
    venue1 = struct.pack('B', len(lang1 + name1)) + lang1.encode() + name1.encode()
    venue2 = struct.pack('B', len(lang2 + name2)) + lang2.encode() + name2.encode()
    venue_name = binascii.hexlify(venue_info + venue1 + venue2)

    url1 = "http://example.com/venue"
    url2 = "https://example.org/venue-info/"

    params = {"ssid": "gas/anqp/pmf",
              "wpa": "2",
              "wpa_key_mgmt": "WPA-PSK",
              "rsn_pairwise": "CCMP",
              "wpa_passphrase": "12345678",
              "ieee80211w": "2",
              "interworking": "1",
              "venue_group": str(venue_group),
              "venue_type": str(venue_type),
              "venue_name": [lang1 + ":" + name1, lang2 + ":" + name2],
              "venue_url": ["1:" + url1, "2:" + url2]}
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = apdev[0]['bssid']

    dev[0].connect("gas/anqp/pmf", psk="12345678", ieee80211w="2",
                   scan_freq="2412")
    if "OK" not in dev[0].request("ANQP_GET " + bssid + " 277"):
        raise Exception("ANQP_GET command failed")

    ev = dev[0].wait_event(["GAS-QUERY-DONE"], timeout=10)
    if ev is None:
        raise Exception("GAS query timed out")

    ev = dev[0].wait_event(["RX-VENUE-URL"], timeout=5)
    if ev is None:
        raise Exception("No Venue URL indication seen")
    if "1 " + url1 not in ev:
        raise Exception("Unexpected Venue URL information: " + ev)

    ev = dev[0].wait_event(["RX-VENUE-URL"], timeout=5)
    if ev is None:
        raise Exception("No Venue URL indication seen (2)")
    if "2 " + url2 not in ev:
        raise Exception("Unexpected Venue URL information (2): " + ev)

    bss = dev[0].get_bss(bssid)
    if "anqp[277]" not in bss:
        raise Exception("Venue-URL ANQP info not available")
    if "protected-anqp-info[277]" not in bss:
        raise Exception("Venue-URL protection info not available")
    if bss["protected-anqp-info[277]"] != "1":
        raise Exception("Venue-URL was not indicated to be protected")

def test_gas_anqp_capab_list(dev, apdev):
    """GAS/ANQP and Capability List ANQP-element"""
    params = {"ssid": "gas/anqp",
              "interworking": "1"}
    params["anqp_elem"] = []
    for i in range(0, 400):
        if i not in [257]:
            params["anqp_elem"] += ["%d:010203" % i]
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = apdev[0]['bssid']

    dev[0].scan_for_bss(bssid, freq="2412", force_scan=True)
    if "OK" not in dev[0].request("ANQP_GET " + bssid + " 257"):
        raise Exception("ANQP_GET command failed")

    ev = dev[0].wait_event(["GAS-QUERY-DONE"], timeout=10)
    if ev is None:
        raise Exception("GAS query timed out")

    bss = dev[0].get_bss(bssid)

    if 'anqp_capability_list' not in bss:
        raise Exception("Capability List ANQP-element not seen")
    val = bss['anqp_capability_list']
    logger.info("anqp_capability_list: " + val)
    ids = []
    while len(val) >= 4:
        id_bin = binascii.unhexlify(val[0:4])
        id = struct.unpack('<H', id_bin)[0]
        if id == 0xdddd:
            break
        ids.append(id)
        val = val[4:]
    logger.info("InfoIDs: " + str(ids))
    for i in range(257, 300):
        if i in [273, 274]:
            continue
        if i not in ids:
            raise Exception("Unexpected Capability List ANQP-element value (missing %d): %s" % (i, bss['anqp_capability_list']))

def test_gas_server_oom(dev, apdev):
    """GAS server OOM"""
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid
    params['gas_comeback_delay'] = "5"
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].scan_for_bss(bssid, freq="2412", force_scan=True)

    tests = ["ap_sta_add;gas_dialog_create",
             "=gas_dialog_create",
             "wpabuf_alloc_copy;gas_serv_rx_gas_comeback_req"]
    for t in tests:
        with alloc_fail(hapd, 1, t):
            if "OK" not in dev[0].request("ANQP_GET " + bssid + " 258"):
                raise Exception("ANQP_GET command failed")
            ev = dev[0].wait_event(["GAS-QUERY-DONE"], timeout=5)
            if ev is None:
                raise Exception("No GAS-QUERY-DONE seen")
            dev[0].dump_monitor()

    hapd.set("gas_comeback_delay", "0")

    tests = ["gas_serv_build_gas_resp_payload"]
    for t in tests:
        with alloc_fail(hapd, 1, t):
            if "OK" not in dev[0].request("ANQP_GET " + bssid + " 258"):
                raise Exception("ANQP_GET command failed")
            ev = dev[0].wait_event(["GAS-QUERY-DONE"], timeout=5)
            if ev is None:
                raise Exception("No GAS-QUERY-DONE seen")
            dev[0].dump_monitor()

    with alloc_fail(hapd, 1,
                    "gas_build_initial_resp;gas_serv_rx_gas_initial_req"):
        req = dev[0].request("GAS_REQUEST " + bssid + " 42 000102000101")
        if "FAIL" in req:
            raise Exception("GAS query request rejected")
        ev = dev[0].wait_event(["GAS-QUERY-DONE"], timeout=5)
        if ev is None:
            raise Exception("No GAS-QUERY-DONE seen")
        dev[0].dump_monitor()

    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    wpas.interface_add("wlan5")
    if "OK" not in wpas.request("P2P_SET listen_channel 1"):
        raise Exception("Failed to set listen channel")
    if "OK" not in wpas.p2p_listen():
        raise Exception("Failed to start listen state")
    if "FAIL" in wpas.request("SET ext_mgmt_frame_handling 1"):
        raise Exception("Failed to enable external management frame handling")

    msg = struct.pack('<BBB', ACTION_CATEG_PUBLIC, GAS_COMEBACK_REQUEST, 1)
    req = "MGMT_TX {} {} freq=2412 wait_time=10 action={}".format(bssid, bssid, binascii.hexlify(msg).decode())
    with alloc_fail(hapd, 1,
                    "gas_anqp_build_comeback_resp_buf;gas_serv_rx_gas_comeback_req"):
        if "OK" not in wpas.request(req):
            raise Exception("Could not send management frame")
        wait_fail_trigger(hapd, "GET_ALLOC_FAIL")

def test_gas_anqp_overrides(dev, apdev):
    """GAS and ANQP overrides"""
    params = {"ssid": "gas/anqp",
              "interworking": "1",
              "anqp_elem": ["257:111111",
                            "258:222222",
                            "260:333333",
                            "261:444444",
                            "262:555555",
                            "263:666666",
                            "264:777777",
                            "268:888888",
                            "275:999999"]}
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = apdev[0]['bssid']

    dev[0].scan_for_bss(bssid, freq="2412", force_scan=True)
    if "OK" not in dev[0].request("ANQP_GET " + bssid + " 257,258,260,261,262,263,264,268,275"):
        raise Exception("ANQP_GET command failed")

    ev = dev[0].wait_event(["GAS-QUERY-DONE"], timeout=10)
    if ev is None:
        raise Exception("GAS query timed out")
    elems = 9
    capa = dev[0].get_capability("fils")
    if capa is None or "FILS" not in capa:
        # FILS Realm Info not supported in the build
        elems -= 1
    for i in range(elems):
        ev = dev[0].wait_event(["RX-ANQP"], timeout=5)
        if ev is None:
            raise Exception("ANQP response not seen")

def test_gas_no_dialog_token_match(dev, apdev):
    """GAS and no dialog token match for comeback request"""
    hapd = start_ap(apdev[0])
    hapd.set("gas_frag_limit", "50")
    bssid = apdev[0]['bssid']

    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    wpas.interface_add("wlan5")
    if "OK" not in wpas.request("P2P_SET listen_channel 1"):
        raise Exception("Failed to set listen channel")
    if "OK" not in wpas.p2p_listen():
        raise Exception("Failed to start listen state")
    if "FAIL" in wpas.request("SET ext_mgmt_frame_handling 1"):
        raise Exception("Failed to enable external management frame handling")

    anqp_query = struct.pack('<HHHHHHHHHH', 256, 16, 257, 258, 260, 261, 262, 263, 264, 268)
    gas = struct.pack('<H', len(anqp_query)) + anqp_query

    dialog_token = 100
    msg = struct.pack('<BBB', ACTION_CATEG_PUBLIC, GAS_INITIAL_REQUEST,
                      dialog_token) + anqp_adv_proto() + gas
    req = "MGMT_TX {} {} freq=2412 wait_time=10 action={}".format(bssid, bssid, binascii.hexlify(msg).decode())
    if "OK" not in wpas.request(req):
        raise Exception("Could not send management frame")
    resp = wpas.mgmt_rx()
    if resp is None:
        raise Exception("MGMT-RX timeout")
    if 'payload' not in resp:
        raise Exception("Missing payload")
    gresp = parse_gas(resp['payload'])
    if gresp['dialog_token'] != dialog_token:
        raise Exception("Dialog token mismatch")
    status_code = gresp['status_code']
    if status_code != 0:
        raise Exception("Unexpected status code {}".format(status_code))

    msg = struct.pack('<BBB', ACTION_CATEG_PUBLIC, GAS_COMEBACK_REQUEST,
                      dialog_token + 1)
    req = "MGMT_TX {} {} freq=2412 wait_time=10 action={}".format(bssid, bssid, binascii.hexlify(msg).decode())
    if "OK" not in wpas.request(req):
        raise Exception("Could not send management frame")
    resp = wpas.mgmt_rx()
    if resp is None:
        raise Exception("MGMT-RX timeout")
    if 'payload' not in resp:
        raise Exception("Missing payload")
    gresp = parse_gas(resp['payload'])
    status_code = gresp['status_code']
    if status_code != 60:
        raise Exception("Unexpected failure status code {}".format(status_code))

def test_gas_vendor_spec_errors(dev, apdev):
    """GAS and vendor specific request error cases"""
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid
    params['osu_server_uri'] = "uri"
    params['hs20_icon'] = "32:32:eng:image/png:icon32:/tmp/icon32.png"
    del params['nai_realm']
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].scan_for_bss(bssid, freq="2412", force_scan=True)
    tests = ["00 12340000",
             "00 dddd0300506fff",
             "00 dddd0400506fffff",
             "00 dddd0400506f9aff",
             "00 dddd0400506f9a11",
             "00 dddd0600506f9a11ff00",
             "00 dddd0600506f9a110600",
             "00 dddd0600506f9a110600",
             "00 dddd0700506f9a11060000",
             "00 dddd0700506f9a110600ff",
             "00 dddd0800506f9a110600ff00",
             "00 dddd0900506f9a110600ff0000",
             "00 dddd0900506f9a110600ff0001",
             "00 dddd0900506f9a110600ffff00",
             "00 dddd0a00506f9a110600ff00013b",
             "00 dddd0700506f9a110100ff",
             "00 dddd0700506f9a11010008",
             "00 dddd14",
             "00 dddd1400506f9a11"]
    for t in tests:
        req = dev[0].request("GAS_REQUEST " + bssid + " " + t)
        if "FAIL" in req:
            raise Exception("GAS query request rejected")
        ev = dev[0].wait_event(["GAS-QUERY-START"], timeout=5)
        if ev is None:
            raise Exception("GAS query did not start")
        ev = dev[0].wait_event(["GAS-QUERY-DONE"], timeout=5)
        if ev is None:
            raise Exception("GAS query did not complete")
        if t == "00 dddd0600506f9a110600":
            hapd.set("nai_realm", "0,another.example.com")

    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    wpas.interface_add("wlan5")
    if "OK" not in wpas.request("P2P_SET listen_channel 1"):
        raise Exception("Failed to set listen channel")
    if "OK" not in wpas.p2p_listen():
        raise Exception("Failed to start listen state")
    if "FAIL" in wpas.request("SET ext_mgmt_frame_handling 1"):
        raise Exception("Failed to enable external management frame handling")

    anqp_query = struct.pack('<HHHHHHHHHH', 256, 16, 257, 258, 260, 261, 262, 263, 264, 268)
    gas = struct.pack('<H', len(anqp_query)) + anqp_query

    dialog_token = 100
    adv = struct.pack('BBBB', 109, 2, 0, 0)
    adv2 = struct.pack('BBB', 108, 1, 0)
    adv3 = struct.pack('BBBB', 108, 3, 0, 0)
    msg = struct.pack('<BBB', ACTION_CATEG_PUBLIC, GAS_INITIAL_REQUEST,
                      dialog_token) + adv + gas
    msg2 = struct.pack('<BBB', ACTION_CATEG_PUBLIC, GAS_INITIAL_REQUEST,
                       dialog_token) + adv2 + gas
    msg3 = struct.pack('<BBB', ACTION_CATEG_PUBLIC, GAS_INITIAL_REQUEST,
                       dialog_token) + adv3
    msg4 = struct.pack('<BBB', ACTION_CATEG_PUBLIC, GAS_INITIAL_REQUEST,
                       dialog_token) + anqp_adv_proto()
    msg5 = struct.pack('<BBB', ACTION_CATEG_PUBLIC, GAS_INITIAL_REQUEST,
                       dialog_token) + anqp_adv_proto() + struct.pack('<H', 1)
    msg6 = struct.pack('<BB', ACTION_CATEG_PUBLIC, GAS_COMEBACK_REQUEST)
    tests = [msg, msg2, msg3, msg4, msg5, msg6]
    for t in tests:
        req = "MGMT_TX {} {} freq=2412 wait_time=10 action={}".format(bssid, bssid, binascii.hexlify(t).decode())
        if "OK" not in wpas.request(req):
            raise Exception("Could not send management frame")
        ev = wpas.wait_event(["MGMT-TX-STATUS"], timeout=5)
        if ev is None:
            raise Exception("No ACK frame seen")
