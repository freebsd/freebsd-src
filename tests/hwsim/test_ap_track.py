# Test cases for hostapd tracking unconnected stations
# Copyright (c) 2015, Jouni Malinen <j@w1.fi>
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

import logging
logger = logging.getLogger()
import subprocess
import time

import hostapd
from wpasupplicant import WpaSupplicant
from utils import parse_ie, disable_hapd, clear_regdom_dev

def test_ap_track_sta(dev, apdev):
    """Dualband AP tracking unconnected stations"""

    try:
        params = {"ssid": "track",
                  "country_code": "US",
                  "hw_mode": "g",
                  "channel": "6",
                  "track_sta_max_num": "2"}
        hapd = hostapd.add_ap(apdev[0], params)

        params = {"ssid": "track",
                  "country_code": "US",
                  "hw_mode": "a",
                  "channel": "40",
                  "track_sta_max_num": "100",
                  "track_sta_max_age": "1"}
        hapd2 = hostapd.add_ap(apdev[1], params)

        _test_ap_track_sta(dev, hapd, apdev[0]['bssid'], hapd2,
                           apdev[1]['bssid'])
    finally:
        disable_hapd(hapd)
        disable_hapd(hapd2)
        clear_regdom_dev(dev, 3)

def _test_ap_track_sta(dev, hapd, bssid, hapd2, bssid2):
    for i in range(2):
        dev[0].scan_for_bss(bssid, freq=2437, force_scan=True)
        dev[0].scan_for_bss(bssid2, freq=5200, force_scan=True)
        dev[1].scan_for_bss(bssid, freq=2437, force_scan=True)
        dev[2].scan_for_bss(bssid2, freq=5200, force_scan=True)

    addr0 = dev[0].own_addr()
    addr1 = dev[1].own_addr()
    addr2 = dev[2].own_addr()

    track = hapd.request("TRACK_STA_LIST")
    if addr0 not in track or addr1 not in track:
        raise Exception("Station missing from 2.4 GHz tracking")
    if addr2 in track:
        raise Exception("Unexpected station included in 2.4 GHz tracking")

    track = hapd2.request("TRACK_STA_LIST")
    if addr0 not in track or addr2 not in track:
        raise Exception("Station missing from 5 GHz tracking")
    if addr1 in track:
        raise Exception("Unexpected station included in 5 GHz tracking")

    # Test expiration
    time.sleep(1.1)
    track = hapd.request("TRACK_STA_LIST")
    if addr0 not in track or addr1 not in track:
        raise Exception("Station missing from 2.4 GHz tracking (expiration)")
    track = hapd2.request("TRACK_STA_LIST")
    if addr0 in track or addr2 in track:
        raise Exception("Station not expired from 5 GHz tracking")

    # Test maximum list length
    dev[0].scan_for_bss(bssid, freq=2437, force_scan=True)
    dev[1].scan_for_bss(bssid, freq=2437, force_scan=True)
    dev[2].scan_for_bss(bssid, freq=2437, force_scan=True)
    track = hapd.request("TRACK_STA_LIST")
    if len(track.splitlines()) != 2:
        raise Exception("Unexpected number of entries: %d" % len(track.splitlines()))
    if addr1 not in track or addr2 not in track:
        raise Exception("Station missing from 2.4 GHz tracking (max limit)")

def test_ap_track_sta_no_probe_resp(dev, apdev):
    """Dualband AP not replying to probes from dualband STA on 2.4 GHz"""
    try:
        params = {"ssid": "track",
                  "country_code": "US",
                  "hw_mode": "g",
                  "channel": "6",
                  "beacon_int": "10000",
                  "no_probe_resp_if_seen_on": apdev[1]['ifname']}
        hapd = hostapd.add_ap(apdev[0], params)

        params = {"ssid": "track",
                  "country_code": "US",
                  "hw_mode": "a",
                  "channel": "40",
                  "track_sta_max_num": "100"}
        hapd2 = hostapd.add_ap(apdev[1], params)

        _test_ap_track_sta_no_probe_resp(dev, apdev[0]['bssid'],
                                         apdev[1]['bssid'])
    finally:
        disable_hapd(hapd)
        disable_hapd(hapd2)
        clear_regdom_dev(dev, 2)

def _test_ap_track_sta_no_probe_resp(dev, bssid, bssid2):
    dev[0].flush_scan_cache()

    dev[0].scan_for_bss(bssid2, freq=5200, force_scan=True)
    dev[1].scan_for_bss(bssid, freq=2437, force_scan=True)
    dev[0].scan(freq=2437, type="ONLY")
    dev[0].scan(freq=2437, type="ONLY")

    bss = dev[0].get_bss(bssid)
    if bss:
        ie = parse_ie(bss['ie'])
        # Check whether this is from a Beacon frame (TIM element included) since
        # it is possible that a Beacon frame was received during the active
        # scan. This test should fail only if a Probe Response frame was
        # received.
        if 5 not in ie:
            raise Exception("2.4 GHz AP found unexpectedly")

def test_ap_track_sta_no_auth(dev, apdev):
    """Dualband AP rejecting authentication from dualband STA on 2.4 GHz"""
    try:
        params = {"ssid": "track",
                  "country_code": "US",
                  "hw_mode": "g",
                  "channel": "6",
                  "track_sta_max_num": "100",
                  "no_auth_if_seen_on": apdev[1]['ifname']}
        hapd = hostapd.add_ap(apdev[0], params)

        params = {"ssid": "track",
                  "country_code": "US",
                  "hw_mode": "a",
                  "channel": "40",
                  "track_sta_max_num": "100"}
        hapd2 = hostapd.add_ap(apdev[1], params)

        _test_ap_track_sta_no_auth(dev, apdev[0]['bssid'], apdev[1]['bssid'])
    finally:
        disable_hapd(hapd)
        disable_hapd(hapd2)
        clear_regdom_dev(dev, 2)

def _test_ap_track_sta_no_auth(dev, bssid, bssid2):
    dev[0].scan_for_bss(bssid, freq=2437, force_scan=True)
    dev[0].scan_for_bss(bssid2, freq=5200, force_scan=True)
    dev[1].scan_for_bss(bssid, freq=2437, force_scan=True)

    dev[1].connect("track", key_mgmt="NONE", scan_freq="2437")

    dev[0].connect("track", key_mgmt="NONE", scan_freq="2437",
                   freq_list="2437", wait_connect=False)
    dev[1].request("DISCONNECT")
    ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED",
                            "CTRL-EVENT-AUTH-REJECT"], timeout=10)
    if ev is None:
        raise Exception("Unknown connection result")
    if "CTRL-EVENT-CONNECTED" in ev:
        raise Exception("Unexpected connection")
    if "status_code=82" not in ev:
        raise Exception("Unexpected rejection reason: " + ev)
    if "ie=34" not in ev:
        raise Exception("No Neighbor Report element: " + ev)
    dev[0].request("DISCONNECT")

def test_ap_track_sta_no_auth_passive(dev, apdev):
    """AP rejecting authentication from dualband STA on 2.4 GHz (passive)"""
    try:
        params = {"ssid": "track",
                  "country_code": "US",
                  "hw_mode": "g",
                  "channel": "6",
                  "no_auth_if_seen_on": apdev[1]['ifname']}
        hapd = hostapd.add_ap(apdev[0], params)

        params = {"ssid": "track",
                  "country_code": "US",
                  "hw_mode": "a",
                  "channel": "40",
                  "interworking": "1",
                  "venue_name": "eng:Venue",
                  "track_sta_max_num": "100"}
        hapd2 = hostapd.add_ap(apdev[1], params)

        _test_ap_track_sta_no_auth_passive(dev, apdev[0]['bssid'],
                                           apdev[1]['bssid'])
    finally:
        disable_hapd(hapd)
        disable_hapd(hapd2)
        clear_regdom_dev(dev)

def _test_ap_track_sta_no_auth_passive(dev, bssid, bssid2):
    dev[0].flush_scan_cache()

    dev[0].scan_for_bss(bssid, freq=2437, force_scan=True)
    for i in range(10):
        dev[0].request("SCAN freq=5200 passive=1")
        ev = dev[0].wait_event(["CTRL-EVENT-SCAN-RESULTS"], timeout=5)
        if ev is None:
            raise Exception("Scan did not complete")
        if dev[0].get_bss(bssid2):
            break
        if i == 9:
            raise Exception("AP not found with passive scans")

    if "OK" not in dev[0].request("ANQP_GET " + bssid2 + " 258"):
        raise Exception("ANQP_GET command failed")
    ev = dev[0].wait_event(["RX-ANQP"], timeout=1)
    if ev is None or "Venue Name" not in ev:
        raise Exception("Did not receive Venue Name")

    dev[0].connect("track", key_mgmt="NONE", scan_freq="2437",
                   freq_list="2437", wait_connect=False)
    ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED",
                            "CTRL-EVENT-AUTH-REJECT"], timeout=10)
    if ev is None:
        raise Exception("Unknown connection result")
    if "CTRL-EVENT-CONNECTED" in ev:
        raise Exception("Unexpected connection")
    if "status_code=82" not in ev:
        raise Exception("Unexpected rejection reason: " + ev)
    dev[0].request("DISCONNECT")

def test_ap_track_sta_force_5ghz(dev, apdev):
    """Dualband AP forcing dualband STA to connect on 5 GHz"""
    try:
        params = {"ssid": "track",
                  "country_code": "US",
                  "hw_mode": "g",
                  "channel": "6",
                  "no_probe_resp_if_seen_on": apdev[1]['ifname'],
                  "no_auth_if_seen_on": apdev[1]['ifname']}
        hapd = hostapd.add_ap(apdev[0], params)

        params = {"ssid": "track",
                  "country_code": "US",
                  "hw_mode": "a",
                  "channel": "40",
                  "track_sta_max_num": "100"}
        hapd2 = hostapd.add_ap(apdev[1], params)

        _test_ap_track_sta_force_5ghz(dev, apdev[0]['bssid'], apdev[1]['bssid'])
    finally:
        disable_hapd(hapd)
        disable_hapd(hapd2)
        clear_regdom_dev(dev)

def _test_ap_track_sta_force_5ghz(dev, bssid, bssid2):
    dev[0].scan_for_bss(bssid, freq=2437, force_scan=True)
    dev[0].scan_for_bss(bssid2, freq=5200, force_scan=True)

    dev[0].connect("track", key_mgmt="NONE", scan_freq="2437 5200")
    freq = dev[0].get_status_field('freq')
    if freq != '5200':
        raise Exception("Unexpected operating channel")
    dev[0].request("DISCONNECT")

def test_ap_track_sta_force_2ghz(dev, apdev):
    """Dualband AP forcing dualband STA to connect on 2.4 GHz"""
    try:
        params = {"ssid": "track",
                  "country_code": "US",
                  "hw_mode": "g",
                  "channel": "6",
                  "track_sta_max_num": "100"}
        hapd = hostapd.add_ap(apdev[0], params)

        params = {"ssid": "track",
                  "country_code": "US",
                  "hw_mode": "a",
                  "channel": "40",
                  "no_probe_resp_if_seen_on": apdev[0]['ifname'],
                  "no_auth_if_seen_on": apdev[0]['ifname']}
        hapd2 = hostapd.add_ap(apdev[1], params)

        _test_ap_track_sta_force_2ghz(dev, apdev[0]['bssid'], apdev[1]['bssid'])
    finally:
        disable_hapd(hapd)
        disable_hapd(hapd2)
        clear_regdom_dev(dev)

def _test_ap_track_sta_force_2ghz(dev, bssid, bssid2):
    dev[0].scan_for_bss(bssid2, freq=5200, force_scan=True)
    dev[0].scan_for_bss(bssid, freq=2437, force_scan=True)

    dev[0].connect("track", key_mgmt="NONE", scan_freq="2437 5200")
    freq = dev[0].get_status_field('freq')
    if freq != '2437':
        raise Exception("Unexpected operating channel")
    dev[0].request("DISCONNECT")

def test_ap_track_taxonomy(dev, apdev):
    """AP tracking STA taxonomy"""
    try:
        _test_ap_track_taxonomy(dev, apdev)
    finally:
        dev[1].request("SET p2p_disabled 0")
        subprocess.call(['iw', 'reg', 'set', '00'])
        dev[0].flush_scan_cache()
        dev[1].flush_scan_cache()
        dev[2].flush_scan_cache()

def _test_ap_track_taxonomy(dev, apdev):
    params = {"ssid": "track",
              "country_code": "US",
              "hw_mode": "g",
              "channel": "6",
              "track_sta_max_num": "2"}
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = apdev[0]['bssid']

    dev[0].scan_for_bss(bssid, freq=2437, force_scan=True)
    addr0 = dev[0].own_addr()
    dev[0].connect("track", key_mgmt="NONE", scan_freq="2437")

    dev[1].request("SET p2p_disabled 1")
    dev[1].scan_for_bss(bssid, freq=2437, force_scan=True)
    addr1 = dev[1].own_addr()
    dev[1].connect("track", key_mgmt="NONE", scan_freq="2437")

    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    wpas.interface_add("wlan5")
    wpas.request("SET model_name track test")
    wpas.scan_for_bss(bssid, freq=2437, force_scan=True)
    addr = wpas.own_addr()
    wpas.connect("track", key_mgmt="NONE", scan_freq="2437")

    if "FAIL" not in hapd.request("SIGNATURE abc"):
        raise Exception("SIGNATURE failure not reported (1)")
    if "FAIL" not in hapd.request("SIGNATURE 22:33:44:55:66:77"):
        raise Exception("SIGNATURE failure not reported (2)")

    res = hapd.request("SIGNATURE " + addr0)
    logger.info("sta0: " + res)
    if not res.startswith("wifi4|probe:"):
        raise Exception("Unexpected SIGNATURE prefix")
    if "|assoc:" not in res:
        raise Exception("Missing assoc info in SIGNATURE")
    if "wps:track_test" in res:
        raise Exception("Unexpected WPS model name")

    res = hapd.request("SIGNATURE " + addr1)
    logger.info("sta1: " + res)
    if not res.startswith("wifi4|probe:"):
        raise Exception("Unexpected SIGNATURE prefix")
    if "|assoc:" not in res:
        raise Exception("Missing assoc info in SIGNATURE")
    if "wps:" in res:
        raise Exception("Unexpected WPS info")
    if ",221(0050f2,4)," in res:
        raise Exception("Unexpected WPS IE info")
    if ",221(506f9a,9)," in res:
        raise Exception("Unexpected P2P IE info")

    res = hapd.request("SIGNATURE " + addr)
    logger.info("sta: " + res)
    if not res.startswith("wifi4|probe:"):
        raise Exception("Unexpected SIGNATURE prefix")
    if "|assoc:" not in res:
        raise Exception("Missing assoc info in SIGNATURE")
    if "wps:track_test" not in res:
        raise Exception("Missing WPS model name")
    if ",221(0050f2,4)," not in res:
        raise Exception("Missing WPS IE info")
    if ",221(506f9a,9)," not in res:
        raise Exception("Missing P2P IE info")

    addr2 = dev[2].own_addr()
    res = hapd.request("SIGNATURE " + addr2)
    if "FAIL" not in res:
        raise Exception("Unexpected SIGNATURE success for sta2 (1)")

    for i in range(10):
        dev[2].request("SCAN freq=2437 passive=1")
        ev = dev[2].wait_event(["CTRL-EVENT-SCAN-RESULTS"], timeout=10)
        if ev is None:
            raise Exception("Scan did not complete")
        if dev[2].get_bss(bssid):
            break

    res = hapd.request("SIGNATURE " + addr2)
    if "FAIL" not in res:
        raise Exception("Unexpected SIGNATURE success for sta2 (2)")

    dev[2].connect("track", key_mgmt="NONE", scan_freq="2437")

    res = hapd.request("SIGNATURE " + addr2)
    if "FAIL" not in res and len(res) > 0:
        raise Exception("Unexpected SIGNATURE success for sta2 (3)")

    dev[2].scan_for_bss(bssid, freq=2437, force_scan=True)

    res = hapd.request("SIGNATURE " + addr2)
    logger.info("sta2: " + res)
    if not res.startswith("wifi4|probe:"):
        raise Exception("Unexpected SIGNATURE prefix")
    if "|assoc:" not in res:
        raise Exception("Missing assoc info in SIGNATURE")
