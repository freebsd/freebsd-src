# Test cases for HT operations with hostapd
# Copyright (c) 2013-2014, Jouni Malinen <j@w1.fi>
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

from remotehost import remote_compatible
import time
import logging
logger = logging.getLogger()
import struct

import hostapd
from wpasupplicant import WpaSupplicant
from utils import *
import hwsim_utils

def test_ap_ht40_scan(dev, apdev):
    """HT40 co-ex scan"""
    clear_scan_cache(apdev[0])
    params = {"ssid": "test-ht40",
              "channel": "5",
              "ht_capab": "[HT40-]"}
    hapd = hostapd.add_ap(apdev[0], params, wait_enabled=False)

    state = hapd.get_status_field("state")
    if state != "HT_SCAN":
        time.sleep(0.1)
        state = hapd.get_status_field("state")
        if state != "HT_SCAN":
            raise Exception("Unexpected interface state - expected HT_SCAN")

    ev = hapd.wait_event(["AP-ENABLED"], timeout=10)
    if not ev:
        raise Exception("AP setup timed out")

    state = hapd.get_status_field("state")
    if state != "ENABLED":
        raise Exception("Unexpected interface state - expected ENABLED")

    freq = hapd.get_status_field("freq")
    if freq != "2432":
        raise Exception("Unexpected frequency")
    pri = hapd.get_status_field("channel")
    if pri != "5":
        raise Exception("Unexpected primary channel")
    sec = hapd.get_status_field("secondary_channel")
    if sec != "-1":
        raise Exception("Unexpected secondary channel")

    status = hapd.get_status()
    logger.info("hostapd STATUS: " + str(status))

    dev[0].connect("test-ht40", key_mgmt="NONE", scan_freq=freq)
    sta = hapd.get_sta(dev[0].own_addr())
    logger.info("hostapd STA: " + str(sta))

    res = dev[0].request("SIGNAL_POLL")
    logger.info("STA SIGNAL_POLL:\n" + res.strip())
    sig = res.splitlines()
    if "WIDTH=40 MHz" not in sig:
        raise Exception("Not a 40 MHz connection")

    if 'supp_op_classes' not in sta or len(sta['supp_op_classes']) < 2:
        raise Exception("No Supported Operating Classes information for STA")
    opclass = int(sta['supp_op_classes'][0:2], 16)
    if opclass != 84:
        raise Exception("Unexpected Current Operating Class from STA: %d" % opclass)

def test_ap_ht_wifi_generation(dev, apdev):
    """HT and wifi_generation"""
    clear_scan_cache(apdev[0])
    params = {"ssid": "test-ht",
              "channel": "6"}
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].connect("test-ht", key_mgmt="NONE", scan_freq="2437")
    status = dev[0].get_status()
    if 'wifi_generation' not in status:
        # For now, assume this is because of missing kernel support
        raise HwsimSkip("Association Request IE reporting not supported")
        #raise Exception("Missing wifi_generation information")
    if status['wifi_generation'] != "4":
        raise Exception("Unexpected wifi_generation value: " + status['wifi_generation'])

    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    wpas.interface_add("wlan5", drv_params="force_connect_cmd=1")
    wpas.connect("test-ht", key_mgmt="NONE", scan_freq="2437")
    status = wpas.get_status()
    if 'wifi_generation' not in status:
        # For now, assume this is because of missing kernel support
        raise HwsimSkip("Association Request IE reporting not supported")
        #raise Exception("Missing wifi_generation information (connect)")
    if status['wifi_generation'] != "4":
        raise Exception("Unexpected wifi_generation value (connect): " + status['wifi_generation'])

@remote_compatible
def test_ap_ht40_scan_conflict(dev, apdev):
    """HT40 co-ex scan conflict"""
    clear_scan_cache(apdev[0])
    params = {"ssid": "test-ht40",
              "channel": "6",
              "ht_capab": "[HT40+]"}
    hostapd.add_ap(apdev[1], params)

    params = {"ssid": "test-ht40",
              "channel": "5",
              "ht_capab": "[HT40-]"}
    hapd = hostapd.add_ap(apdev[0], params, wait_enabled=False)

    state = hapd.get_status_field("state")
    if state != "HT_SCAN":
        time.sleep(0.1)
        state = hapd.get_status_field("state")
        if state != "HT_SCAN":
            raise Exception("Unexpected interface state - expected HT_SCAN")

    ev = hapd.wait_event(["AP-ENABLED"], timeout=10)
    if not ev:
        raise Exception("AP setup timed out")

    state = hapd.get_status_field("state")
    if state != "ENABLED":
        raise Exception("Unexpected interface state - expected ENABLED")

    freq = hapd.get_status_field("freq")
    if freq != "2432":
        raise Exception("Unexpected frequency")
    pri = hapd.get_status_field("channel")
    if pri != "5":
        raise Exception("Unexpected primary channel")
    sec = hapd.get_status_field("secondary_channel")
    if sec != "0":
        raise Exception("Unexpected secondary channel: " + sec)

    dev[0].connect("test-ht40", key_mgmt="NONE", scan_freq=freq)

@remote_compatible
def test_ap_ht40_scan_conflict2(dev, apdev):
    """HT40 co-ex scan conflict (HT40-)"""
    clear_scan_cache(apdev[0])
    params = {"ssid": "test-ht40",
              "channel": "11",
              "ht_capab": "[HT40-]"}
    hostapd.add_ap(apdev[1], params)

    params = {"ssid": "test-ht40",
              "channel": "1",
              "ht_capab": "[HT40+]"}
    hapd = hostapd.add_ap(apdev[0], params, wait_enabled=False)

    state = hapd.get_status_field("state")
    if state != "HT_SCAN":
        time.sleep(0.1)
        state = hapd.get_status_field("state")
        if state != "HT_SCAN":
            raise Exception("Unexpected interface state - expected HT_SCAN")

    ev = hapd.wait_event(["AP-ENABLED"], timeout=10)
    if not ev:
        raise Exception("AP setup timed out")

    state = hapd.get_status_field("state")
    if state != "ENABLED":
        raise Exception("Unexpected interface state - expected ENABLED")

    freq = hapd.get_status_field("freq")
    if freq != "2412":
        raise Exception("Unexpected frequency")
    pri = hapd.get_status_field("channel")
    if pri != "1":
        raise Exception("Unexpected primary channel")
    sec = hapd.get_status_field("secondary_channel")
    if sec != "0":
        raise Exception("Unexpected secondary channel: " + sec)

    dev[0].connect("test-ht40", key_mgmt="NONE", scan_freq=freq)

def test_ap_ht40_scan_not_affected(dev, apdev):
    """HT40 co-ex scan and other BSS not affected"""
    clear_scan_cache(apdev[0])
    params = {"ssid": "test-ht20",
              "channel": "11"}
    hostapd.add_ap(apdev[1], params)

    hostapd.cmd_execute(apdev[0], ['ifconfig', apdev[0]['ifname'], 'up'])
    hostapd.cmd_execute(apdev[0], ['iw', apdev[0]['ifname'], 'scan', 'trigger',
                                   'freq', '2462'])
    time.sleep(0.5)
    hostapd.cmd_execute(apdev[0], ['iw', apdev[0]['ifname'], 'scan', 'dump'])
    time.sleep(0.1)
    hostapd.cmd_execute(apdev[0], ['ifconfig', apdev[0]['ifname'], 'down'])

    params = {"ssid": "test-ht40",
              "channel": "1",
              "ht_capab": "[HT40+]"}
    hapd = hostapd.add_ap(apdev[0], params, wait_enabled=False)

    state = hapd.get_status_field("state")
    if state != "HT_SCAN":
        time.sleep(0.1)
        state = hapd.get_status_field("state")
        if state != "HT_SCAN":
            raise Exception("Unexpected interface state - expected HT_SCAN")

    ev = hapd.wait_event(["AP-ENABLED"], timeout=10)
    if not ev:
        raise Exception("AP setup timed out")

    state = hapd.get_status_field("state")
    if state != "ENABLED":
        raise Exception("Unexpected interface state - expected ENABLED")

    freq = hapd.get_status_field("freq")
    if freq != "2412":
        raise Exception("Unexpected frequency")
    pri = hapd.get_status_field("channel")
    if pri != "1":
        raise Exception("Unexpected primary channel")
    sec = hapd.get_status_field("secondary_channel")
    if sec != "1":
        raise Exception("Unexpected secondary channel: " + sec)

    dev[0].connect("test-ht40", key_mgmt="NONE", scan_freq=freq)

@remote_compatible
def test_ap_ht40_scan_legacy_conflict(dev, apdev):
    """HT40 co-ex scan conflict with legacy 20 MHz AP"""
    clear_scan_cache(apdev[0])
    params = {"ssid": "legacy-20",
              "channel": "7", "ieee80211n": "0"}
    hostapd.add_ap(apdev[1], params)

    params = {"ssid": "test-ht40",
              "channel": "5",
              "ht_capab": "[HT40-]"}
    hapd = hostapd.add_ap(apdev[0], params, wait_enabled=False)

    state = hapd.get_status_field("state")
    if state != "HT_SCAN":
        time.sleep(0.1)
        state = hapd.get_status_field("state")
        if state != "HT_SCAN":
            raise Exception("Unexpected interface state - expected HT_SCAN")

    ev = hapd.wait_event(["AP-ENABLED"], timeout=10)
    if not ev:
        raise Exception("AP setup timed out")

    state = hapd.get_status_field("state")
    if state != "ENABLED":
        raise Exception("Unexpected interface state - expected ENABLED")

    freq = hapd.get_status_field("freq")
    if freq != "2432":
        raise Exception("Unexpected frequency: " + freq)
    pri = hapd.get_status_field("channel")
    if pri != "5":
        raise Exception("Unexpected primary channel: " + pri)
    sec = hapd.get_status_field("secondary_channel")
    if sec != "0":
        raise Exception("Unexpected secondary channel: " + sec)

    dev[0].connect("test-ht40", key_mgmt="NONE", scan_freq=freq)

@remote_compatible
def test_ap_ht40_scan_ht20_conflict(dev, apdev):
    """HT40 co-ex scan conflict with HT 20 MHz AP"""
    clear_scan_cache(apdev[0])
    params = {"ssid": "ht-20",
              "channel": "7", "ieee80211n": "1"}
    hostapd.add_ap(apdev[1], params)

    params = {"ssid": "test-ht40",
              "channel": "5",
              "ht_capab": "[HT40-]"}
    hapd = hostapd.add_ap(apdev[0], params, wait_enabled=False)

    state = hapd.get_status_field("state")
    if state != "HT_SCAN":
        time.sleep(0.1)
        state = hapd.get_status_field("state")
        if state != "HT_SCAN":
            raise Exception("Unexpected interface state - expected HT_SCAN")

    ev = hapd.wait_event(["AP-ENABLED"], timeout=10)
    if not ev:
        raise Exception("AP setup timed out")

    state = hapd.get_status_field("state")
    if state != "ENABLED":
        raise Exception("Unexpected interface state - expected ENABLED")

    freq = hapd.get_status_field("freq")
    if freq != "2432":
        raise Exception("Unexpected frequency: " + freq)
    pri = hapd.get_status_field("channel")
    if pri != "5":
        raise Exception("Unexpected primary channel: " + pri)
    sec = hapd.get_status_field("secondary_channel")
    if sec != "0":
        raise Exception("Unexpected secondary channel: " + sec)

    dev[0].connect("test-ht40", key_mgmt="NONE", scan_freq=freq)

def test_ap_ht40_scan_intolerant(dev, apdev):
    """HT40 co-ex scan finding an AP advertising 40 MHz intolerant"""
    clear_scan_cache(apdev[0])
    params = {"ssid": "another-bss",
              "channel": "1",
              "ht_capab": "[40-INTOLERANT]"}
    hostapd.add_ap(apdev[1], params)

    params = {"ssid": "test-ht40",
              "channel": "1",
              "ht_capab": "[HT40+]"}
    hapd = hostapd.add_ap(apdev[0], params, wait_enabled=False)

    state = hapd.get_status_field("state")
    if state != "HT_SCAN":
        time.sleep(0.1)
        state = hapd.get_status_field("state")
        if state != "HT_SCAN":
            raise Exception("Unexpected interface state - expected HT_SCAN")

    ev = hapd.wait_event(["AP-ENABLED"], timeout=10)
    if not ev:
        raise Exception("AP setup timed out")

    state = hapd.get_status_field("state")
    if state != "ENABLED":
        raise Exception("Unexpected interface state - expected ENABLED")

    freq = hapd.get_status_field("freq")
    if freq != "2412":
        raise Exception("Unexpected frequency: " + freq)
    pri = hapd.get_status_field("channel")
    if pri != "1":
        raise Exception("Unexpected primary channel: " + pri)
    sec = hapd.get_status_field("secondary_channel")
    if sec != "0":
        raise Exception("Unexpected secondary channel: " + sec)

    dev[0].connect("test-ht40", key_mgmt="NONE", scan_freq=freq)

def test_ap_ht40_scan_match(dev, apdev):
    """HT40 co-ex scan matching configuration"""
    clear_scan_cache(apdev[0])
    params = {"ssid": "test-ht40",
              "channel": "5",
              "ht_capab": "[HT40-]"}
    hostapd.add_ap(apdev[1], params)

    params = {"ssid": "test-ht40",
              "channel": "5",
              "ht_capab": "[HT40-]"}
    hapd = hostapd.add_ap(apdev[0], params, wait_enabled=False)

    state = hapd.get_status_field("state")
    if state != "HT_SCAN":
        time.sleep(0.1)
        state = hapd.get_status_field("state")
        if state != "HT_SCAN":
            raise Exception("Unexpected interface state - expected HT_SCAN")

    ev = hapd.wait_event(["AP-ENABLED"], timeout=10)
    if not ev:
        raise Exception("AP setup timed out")

    state = hapd.get_status_field("state")
    if state != "ENABLED":
        raise Exception("Unexpected interface state - expected ENABLED")

    freq = hapd.get_status_field("freq")
    if freq != "2432":
        raise Exception("Unexpected frequency")
    pri = hapd.get_status_field("channel")
    if pri != "5":
        raise Exception("Unexpected primary channel")
    sec = hapd.get_status_field("secondary_channel")
    if sec != "-1":
        raise Exception("Unexpected secondary channel: " + sec)

    dev[0].connect("test-ht40", key_mgmt="NONE", scan_freq=freq)

def test_ap_ht40_5ghz_match(dev, apdev):
    """HT40 co-ex scan on 5 GHz with matching pri/sec channel"""
    clear_scan_cache(apdev[0])
    try:
        hapd = None
        hapd2 = None
        params = {"ssid": "test-ht40",
                  "hw_mode": "a",
                  "channel": "36",
                  "country_code": "US",
                  "ht_capab": "[HT40+]"}
        hapd2 = hostapd.add_ap(apdev[1], params)

        params = {"ssid": "test-ht40",
                  "hw_mode": "a",
                  "channel": "36",
                  "ht_capab": "[HT40+]"}
        hapd = hostapd.add_ap(apdev[0], params, wait_enabled=False)

        state = hapd.get_status_field("state")
        if state != "HT_SCAN":
            time.sleep(0.1)
            state = hapd.get_status_field("state")
            if state != "HT_SCAN":
                raise Exception("Unexpected interface state - expected HT_SCAN")

        ev = hapd.wait_event(["AP-ENABLED"], timeout=10)
        if not ev:
            raise Exception("AP setup timed out")

        state = hapd.get_status_field("state")
        if state != "ENABLED":
            raise Exception("Unexpected interface state - expected ENABLED")

        freq = hapd.get_status_field("freq")
        if freq != "5180":
            raise Exception("Unexpected frequency")
        pri = hapd.get_status_field("channel")
        if pri != "36":
            raise Exception("Unexpected primary channel")
        sec = hapd.get_status_field("secondary_channel")
        if sec != "1":
            raise Exception("Unexpected secondary channel: " + sec)

        dev[0].connect("test-ht40", key_mgmt="NONE", scan_freq=freq)
    finally:
        dev[0].request("DISCONNECT")
        if hapd:
            hapd.request("DISABLE")
        if hapd2:
            hapd2.request("DISABLE")
        set_world_reg(apdev[0], apdev[1], dev[0])
        dev[0].flush_scan_cache()

def test_ap_ht40_5ghz_switch(dev, apdev):
    """HT40 co-ex scan on 5 GHz switching pri/sec channel"""
    clear_scan_cache(apdev[0])
    try:
        hapd = None
        hapd2 = None
        params = {"ssid": "test-ht40",
                  "hw_mode": "a",
                  "channel": "36",
                  "country_code": "US",
                  "ht_capab": "[HT40+]"}
        hapd2 = hostapd.add_ap(apdev[1], params)

        params = {"ssid": "test-ht40",
                  "hw_mode": "a",
                  "channel": "40",
                  "ht_capab": "[HT40-]"}
        hapd = hostapd.add_ap(apdev[0], params, wait_enabled=False)

        state = hapd.get_status_field("state")
        if state != "HT_SCAN":
            time.sleep(0.1)
            state = hapd.get_status_field("state")
            if state != "HT_SCAN":
                raise Exception("Unexpected interface state - expected HT_SCAN")

        ev = hapd.wait_event(["AP-ENABLED"], timeout=10)
        if not ev:
            raise Exception("AP setup timed out")

        state = hapd.get_status_field("state")
        if state != "ENABLED":
            raise Exception("Unexpected interface state - expected ENABLED")

        freq = hapd.get_status_field("freq")
        if freq != "5180":
            raise Exception("Unexpected frequency: " + freq)
        pri = hapd.get_status_field("channel")
        if pri != "36":
            raise Exception("Unexpected primary channel: " + pri)
        sec = hapd.get_status_field("secondary_channel")
        if sec != "1":
            raise Exception("Unexpected secondary channel: " + sec)

        dev[0].connect("test-ht40", key_mgmt="NONE", scan_freq=freq)
    finally:
        dev[0].request("DISCONNECT")
        if hapd:
            hapd.request("DISABLE")
        if hapd2:
            hapd2.request("DISABLE")
        set_world_reg(apdev[0], apdev[1], dev[0])

def test_ap_ht40_5ghz_switch2(dev, apdev):
    """HT40 co-ex scan on 5 GHz switching pri/sec channel (2)"""
    clear_scan_cache(apdev[0])
    try:
        hapd = None
        hapd2 = None
        params = {"ssid": "test-ht40",
                  "hw_mode": "a",
                  "channel": "36",
                  "country_code": "US",
                  "ht_capab": "[HT40+]"}
        hapd2 = hostapd.add_ap(apdev[1], params)

        id = dev[0].add_network()
        dev[0].set_network(id, "mode", "2")
        dev[0].set_network_quoted(id, "ssid", "wpas-ap-open")
        dev[0].set_network(id, "key_mgmt", "NONE")
        dev[0].set_network(id, "frequency", "5200")
        dev[0].set_network(id, "scan_freq", "5200")
        dev[0].select_network(id)
        time.sleep(1)

        params = {"ssid": "test-ht40",
                  "hw_mode": "a",
                  "channel": "40",
                  "ht_capab": "[HT40-]"}
        hapd = hostapd.add_ap(apdev[0], params, wait_enabled=False)

        state = hapd.get_status_field("state")
        if state != "HT_SCAN":
            time.sleep(0.1)
            state = hapd.get_status_field("state")
            if state != "HT_SCAN":
                raise Exception("Unexpected interface state - expected HT_SCAN")

        ev = hapd.wait_event(["AP-ENABLED"], timeout=10)
        if not ev:
            raise Exception("AP setup timed out")

        state = hapd.get_status_field("state")
        if state != "ENABLED":
            raise Exception("Unexpected interface state - expected ENABLED")

        freq = hapd.get_status_field("freq")
        if freq != "5180":
            raise Exception("Unexpected frequency: " + freq)
        pri = hapd.get_status_field("channel")
        if pri != "36":
            raise Exception("Unexpected primary channel: " + pri)
        sec = hapd.get_status_field("secondary_channel")
        if sec != "1":
            raise Exception("Unexpected secondary channel: " + sec)

        dev[0].connect("test-ht40", key_mgmt="NONE", scan_freq=freq)
    finally:
        dev[0].request("DISCONNECT")
        if hapd:
            hapd.request("DISABLE")
        if hapd2:
            hapd2.request("DISABLE")
        set_world_reg(apdev[0], apdev[1], dev[0])
        dev[0].flush_scan_cache()

def test_obss_scan(dev, apdev):
    """Overlapping BSS scan request"""
    clear_scan_cache(apdev[0])
    params = {"ssid": "obss-scan",
              "channel": "6",
              "ht_capab": "[HT40-]",
              "obss_interval": "10"}
    hapd = hostapd.add_ap(apdev[0], params)

    params = {"ssid": "another-bss",
              "channel": "9",
              "ieee80211n": "0"}
    hostapd.add_ap(apdev[1], params)
    run_obss_scan(hapd, dev)

def test_obss_scan_ht40_plus(dev, apdev):
    """Overlapping BSS scan request (HT40+)"""
    clear_scan_cache(apdev[0])
    params = {"ssid": "obss-scan",
              "channel": "6",
              "ht_capab": "[HT40+]",
              "obss_interval": "10"}
    hapd = hostapd.add_ap(apdev[0], params)

    params = {"ssid": "another-bss",
              "channel": "9",
              "ieee80211n": "0"}
    hostapd.add_ap(apdev[1], params)
    run_obss_scan(hapd, dev, ht40plus=True)

def run_obss_scan(hapd, dev, ht40plus=False):
    dev[0].connect("obss-scan", key_mgmt="NONE", scan_freq="2437")
    res = dev[0].request("SIGNAL_POLL")
    logger.info("SIGNAL_POLL:\n" + res)
    sig = res.splitlines()
    if "FREQUENCY=2437" not in sig:
        raise Exception("Unexpected frequency")
    if "WIDTH=40 MHz" not in sig:
        raise Exception("Not a 40 MHz connection")
    if ht40plus and "CENTER_FRQ1=2447" not in sig:
        raise Exception("Not HT40+")
    if not ht40plus and "CENTER_FRQ1=2427" not in sig:
        raise Exception("Not HT40-")
    hapd.set("ext_mgmt_frame_handling", "1")
    logger.info("Waiting for OBSS scan to occur")
    ev = dev[0].wait_event(["CTRL-EVENT-SCAN-STARTED"], timeout=15)
    if ev is None:
        raise Exception("Timed out while waiting for OBSS scan to start")
    ev = dev[0].wait_event(["CTRL-EVENT-SCAN-RESULTS"], timeout=10)
    if ev is None:
        raise Exception("Timed out while waiting for OBSS scan results")
    received = False
    for i in range(0, 4):
        frame = hapd.mgmt_rx(timeout=5)
        if frame is None:
            raise Exception("MGMT RX wait timed out")
        if frame['subtype'] != 13:
            continue
        payload = frame['payload']
        if len(payload) < 3:
            continue
        (category, action, ie) = struct.unpack('BBB', payload[0:3])
        if category != 4:
            continue
        if action != 0:
            continue
        if ie == 72:
            logger.info("20/40 BSS Coexistence report received")
            received = True
            break
    if not received:
        raise Exception("20/40 BSS Coexistence report not seen")

def test_obss_scan_40_intolerant(dev, apdev):
    """Overlapping BSS scan request with 40 MHz intolerant AP"""
    params = {"ssid": "obss-scan",
              "channel": "6",
              "ht_capab": "[HT40-]",
              "obss_interval": "10"}
    hapd = hostapd.add_ap(apdev[0], params)

    params = {"ssid": "another-bss",
              "channel": "7",
              "ht_capab": "[40-INTOLERANT]"}
    hostapd.add_ap(apdev[1], params)

    dev[0].connect("obss-scan", key_mgmt="NONE", scan_freq="2437")
    hapd.set("ext_mgmt_frame_handling", "1")
    logger.info("Waiting for OBSS scan to occur")
    ev = dev[0].wait_event(["CTRL-EVENT-SCAN-STARTED"], timeout=15)
    if ev is None:
        raise Exception("Timed out while waiting for OBSS scan to start")
    ev = dev[0].wait_event(["CTRL-EVENT-SCAN-RESULTS"], timeout=10)
    if ev is None:
        raise Exception("Timed out while waiting for OBSS scan results")
    received = False
    for i in range(0, 4):
        frame = hapd.mgmt_rx(timeout=5)
        if frame is None:
            raise Exception("MGMT RX wait timed out")
        if frame['subtype'] != 13:
            continue
        payload = frame['payload']
        if len(payload) < 3:
            continue
        (category, action, ie) = struct.unpack('BBB', payload[0:3])
        if category != 4:
            continue
        if action != 0:
            continue
        if ie == 72:
            logger.info("20/40 BSS Coexistence report received")
            received = True
            break
    if not received:
        raise Exception("20/40 BSS Coexistence report not seen")

def test_obss_coex_report_handling(dev, apdev):
    """Overlapping BSS scan report handling with obss_interval=0"""
    clear_scan_cache(apdev[0])
    params = {"ssid": "obss-scan",
              "channel": "6",
              "ht_capab": "[HT40-]"}
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = apdev[0]['bssid']
    dev[0].connect("obss-scan", key_mgmt="NONE", scan_freq="2437")

    sec = hapd.get_status_field("secondary_channel")
    if sec != "-1":
        raise Exception("AP is not using 40 MHz channel")

    # 20/40 MHz co-ex report tests: number of invalid reports and a valid report
    # that forces 20 MHz channel.
    tests = ['0400', '040048', '04004801', '0400480000', '0400490100',
             '040048ff0000', '04004801ff49ff00', '04004801004900',
             '0400480100490101', '0400480100490201ff',
             '040048010449020005']
    for msg in tests:
        req = "MGMT_TX {} {} freq=2437 action={}".format(bssid, bssid, msg)
        if "OK" not in dev[0].request(req):
            raise Exception("Could not send management frame")
    time.sleep(0.5)
    sec = hapd.get_status_field("secondary_channel")
    if sec != "0":
        raise Exception("AP did not move to 20 MHz channel")

def test_obss_coex_report_handling1(dev, apdev):
    """Overlapping BSS scan report handling with obss_interval=1"""
    clear_scan_cache(apdev[0])
    params = {"ssid": "obss-scan",
              "channel": "6",
              "ht_capab": "[HT40+]",
              "obss_interval": "1"}
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = apdev[0]['bssid']
    dev[0].connect("obss-scan", key_mgmt="NONE", scan_freq="2437")

    sec = hapd.get_status_field("secondary_channel")
    if sec != "1":
        raise Exception("AP is not using 40 MHz channel")

    # 20/40 MHz co-ex report forcing 20 MHz channel
    msg = '040048010449020005'
    req = "MGMT_TX {} {} freq=2437 action={}".format(bssid, bssid, msg)
    if "OK" not in dev[0].request(req):
        raise Exception("Could not send management frame")
    time.sleep(0.5)
    sec = hapd.get_status_field("secondary_channel")
    if sec != "0":
        raise Exception("AP did not move to 20 MHz channel")

    # No 20/40 MHz co-ex reports forcing 20 MHz channel during next interval
    for i in range(20):
        sec = hapd.get_status_field("secondary_channel")
        if sec == "1":
            break
        time.sleep(0.5)
    if sec != "1":
        raise Exception("AP did not return to 40 MHz channel")

def test_obss_coex_report_handling2(dev, apdev):
    """Overlapping BSS scan report handling with obss_interval=1 and no overlap"""
    clear_scan_cache(apdev[0])
    params = {"ssid": "obss-scan",
              "channel": "6",
              "ht_capab": "[HT40+]",
              "obss_interval": "1"}
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = apdev[0]['bssid']
    dev[0].connect("obss-scan", key_mgmt="NONE", scan_freq="2437")

    sec = hapd.get_status_field("secondary_channel")
    if sec != "1":
        raise Exception("AP is not using 40 MHz channel")

    # 20/40 MHz co-ex report that does not force a move to 20 MHz channel
    # (out of affected range and matching primary channel cases)
    msg = '0400' + '480100' + '49020001' + '49020006'
    req = "MGMT_TX {} {} freq=2437 action={}".format(bssid, bssid, msg)
    if "OK" not in dev[0].request(req):
        raise Exception("Could not send management frame")
    time.sleep(0.5)
    sec = hapd.get_status_field("secondary_channel")
    if sec != "1":
        raise Exception("Unexpected move to 20 MHz channel")

    # 20/40 MHz co-ex report forcing 20 MHz channel
    # (out of affected range and in affected range but not matching primary)
    msg = '0400' + '480100' + '4903000105'
    req = "MGMT_TX {} {} freq=2437 action={}".format(bssid, bssid, msg)
    if "OK" not in dev[0].request(req):
        raise Exception("Could not send management frame")
    time.sleep(0.5)
    sec = hapd.get_status_field("secondary_channel")
    if sec != "0":
        raise Exception("AP did not move to 20 MHz channel")

def test_olbc(dev, apdev):
    """OLBC detection"""
    params = {"ssid": "test-olbc",
              "channel": "6",
              "ht_capab": "[HT40-]",
              "ap_table_expiration_time": "2"}
    hapd = hostapd.add_ap(apdev[0], params)
    status = hapd.get_status()
    if status['olbc'] != '0' or status['olbc_ht'] != '0':
        raise Exception("Unexpected OLBC information")

    params = {"ssid": "olbc-ap",
              "hw_mode": "b",
              "channel": "6",
              "wmm_enabled": "0"}
    hostapd.add_ap(apdev[1], params)
    time.sleep(0.5)
    status = hapd.get_status()
    if status['olbc'] != '1' or status['olbc_ht'] != '1':
        raise Exception("Missing OLBC information")

    hostapd.remove_bss(apdev[1])

    logger.info("Waiting for OLBC state to time out")
    cleared = False
    for i in range(0, 15):
        time.sleep(1)
        status = hapd.get_status()
        if status['olbc'] == '0' and status['olbc_ht'] == '0':
            cleared = True
            break
    if not cleared:
        raise Exception("OLBC state did nto time out")

def test_olbc_table_limit(dev, apdev):
    """OLBC AP table size limit"""
    ifname1 = apdev[0]['ifname']
    ifname2 = apdev[0]['ifname'] + '-2'
    ifname3 = apdev[0]['ifname'] + '-3'
    hostapd.add_bss(apdev[0], ifname1, 'bss-1.conf')
    hostapd.add_bss(apdev[0], ifname2, 'bss-2.conf')
    hostapd.add_bss(apdev[0], ifname3, 'bss-3.conf')

    params = {"ssid": "test-olbc",
              "channel": "1",
              "ap_table_max_size": "2"}
    hapd = hostapd.add_ap(apdev[1], params)

    time.sleep(0.3)
    with alloc_fail(hapd, 1, "ap_list_process_beacon"):
        time.sleep(0.3)
    hapd.set("ap_table_max_size", "1")
    time.sleep(0.3)
    hapd.set("ap_table_max_size", "0")
    time.sleep(0.3)

def test_olbc_5ghz(dev, apdev):
    """OLBC detection on 5 GHz"""
    try:
        hapd = None
        hapd2 = None
        params = {"ssid": "test-olbc",
                  "country_code": "FI",
                  "hw_mode": "a",
                  "channel": "36",
                  "ht_capab": "[HT40+]"}
        hapd = hostapd.add_ap(apdev[0], params)
        status = hapd.get_status()
        if status['olbc'] != '0' or status['olbc_ht'] != '0':
            raise Exception("Unexpected OLBC information")

        params = {"ssid": "olbc-ap",
                  "country_code": "FI",
                  "hw_mode": "a",
                  "channel": "36",
                  "ieee80211n": "0",
                  "wmm_enabled": "0"}
        hapd2 = hostapd.add_ap(apdev[1], params)
        found = False
        for i in range(20):
            time.sleep(0.1)
            status = hapd.get_status()
            logger.debug('olbc_ht: ' + status['olbc_ht'])
            if status['olbc_ht'] == '1':
                found = True
                break
        if not found:
            raise Exception("Missing OLBC information")
    finally:
        if hapd:
            hapd.request("DISABLE")
        if hapd2:
            hapd2.request("DISABLE")
        set_world_reg(apdev[0], apdev[1], None)

def test_ap_require_ht(dev, apdev):
    """Require HT"""
    params = {"ssid": "require-ht",
              "require_ht": "1"}
    hapd = hostapd.add_ap(apdev[0], params)

    dev[1].connect("require-ht", key_mgmt="NONE", scan_freq="2412",
                   disable_ht="1", wait_connect=False)
    dev[0].connect("require-ht", key_mgmt="NONE", scan_freq="2412")
    ev = dev[1].wait_event(["CTRL-EVENT-ASSOC-REJECT"])
    dev[1].request("DISCONNECT")
    if ev is None:
        raise Exception("Association rejection timed out")
    if "status_code=27" not in ev:
        raise Exception("Unexpected rejection status code")
    dev[2].connect("require-ht", key_mgmt="NONE", scan_freq="2412",
                   ht_mcs="0x01 00 00 00 00 00 00 00 00 00",
                   disable_max_amsdu="1", ampdu_factor="2",
                   ampdu_density="1", disable_ht40="1", disable_sgi="1",
                   disable_ldpc="1", rx_stbc="2", tx_stbc="1")

    sta = hapd.get_sta(dev[0].own_addr())
    if 'supp_op_classes' not in sta or len(sta['supp_op_classes']) < 2:
        raise Exception("No Supported Operating Classes information for STA")
    opclass = int(sta['supp_op_classes'][0:2], 16)
    if opclass != 81:
        raise Exception("Unexpected Current Operating Class from STA: %d" % opclass)

def test_ap_ht_stbc(dev, apdev):
    """HT STBC overrides"""
    params = {"ssid": "ht"}
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].connect("ht", key_mgmt="NONE", scan_freq="2412")
    dev[1].connect("ht", key_mgmt="NONE", scan_freq="2412",
                   rx_stbc="0", tx_stbc="0")
    dev[2].connect("ht", key_mgmt="NONE", scan_freq="2412",
                   rx_stbc="1", tx_stbc="1")

@remote_compatible
def test_ap_require_ht_limited_rates(dev, apdev):
    """Require HT with limited supported rates"""
    params = {"ssid": "require-ht",
              "supported_rates": "60 120 240 360 480 540",
              "require_ht": "1"}
    hapd = hostapd.add_ap(apdev[0], params)

    dev[1].connect("require-ht", key_mgmt="NONE", scan_freq="2412",
                   disable_ht="1", wait_connect=False)
    dev[0].connect("require-ht", key_mgmt="NONE", scan_freq="2412")
    ev = dev[1].wait_event(["CTRL-EVENT-ASSOC-REJECT"])
    dev[1].request("DISCONNECT")
    if ev is None:
        raise Exception("Association rejection timed out")
    if "status_code=27" not in ev:
        raise Exception("Unexpected rejection status code")

@remote_compatible
def test_ap_ht_capab_not_supported(dev, apdev):
    """HT configuration with driver not supporting all ht_capab entries"""
    params = {"ssid": "test-ht40",
              "channel": "5",
              "ht_capab": "[HT40-][LDPC][SMPS-STATIC][SMPS-DYNAMIC][GF][SHORT-GI-20][SHORT-GI-40][TX-STBC][RX-STBC1][RX-STBC12][RX-STBC123][DELAYED-BA][MAX-AMSDU-7935][DSSS_CCK-40][LSIG-TXOP-PROT]"}
    hapd = hostapd.add_ap(apdev[0], params, no_enable=True)
    if "FAIL" not in hapd.request("ENABLE"):
        raise Exception("Unexpected ENABLE success")

def test_ap_ht_40mhz_intolerant_sta(dev, apdev):
    """Associated STA indicating 40 MHz intolerant"""
    clear_scan_cache(apdev[0])
    params = {"ssid": "intolerant",
              "channel": "6",
              "ht_capab": "[HT40-]"}
    hapd = hostapd.add_ap(apdev[0], params)
    if hapd.get_status_field("num_sta_ht40_intolerant") != "0":
        raise Exception("Unexpected num_sta_ht40_intolerant value")
    if hapd.get_status_field("secondary_channel") != "-1":
        raise Exception("Unexpected secondary_channel")

    dev[0].connect("intolerant", key_mgmt="NONE", scan_freq="2437")
    if hapd.get_status_field("num_sta_ht40_intolerant") != "0":
        raise Exception("Unexpected num_sta_ht40_intolerant value")
    if hapd.get_status_field("secondary_channel") != "-1":
        raise Exception("Unexpected secondary_channel")

    dev[2].connect("intolerant", key_mgmt="NONE", scan_freq="2437",
                   ht40_intolerant="1")
    time.sleep(1)
    if hapd.get_status_field("num_sta_ht40_intolerant") != "1":
        raise Exception("Unexpected num_sta_ht40_intolerant value (expected 1)")
    if hapd.get_status_field("secondary_channel") != "0":
        raise Exception("Unexpected secondary_channel (did not disable 40 MHz)")

    dev[2].request("DISCONNECT")
    time.sleep(1)
    if hapd.get_status_field("num_sta_ht40_intolerant") != "0":
        raise Exception("Unexpected num_sta_ht40_intolerant value (expected 0)")
    if hapd.get_status_field("secondary_channel") != "-1":
        raise Exception("Unexpected secondary_channel (did not re-enable 40 MHz)")

def test_ap_ht_40mhz_intolerant_sta_deinit(dev, apdev):
    """Associated STA indicating 40 MHz intolerant and hostapd deinit"""
    clear_scan_cache(apdev[0])
    params = {"ssid": "intolerant",
              "channel": "6",
              "ht_capab": "[HT40-]",
              "obss_interval": "0"}
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].connect("intolerant", key_mgmt="NONE", scan_freq="2437",
                   ht40_intolerant="1")
    time.sleep(1)
    if hapd.get_status_field("num_sta_ht40_intolerant") != "1":
        raise Exception("Unexpected num_sta_ht40_intolerant value (expected 1)")
    hglobal = hostapd.HostapdGlobal()
    hglobal.remove(apdev[0]['ifname'])

    dev[0].request("DISCONNECT")

def test_ap_ht_40mhz_intolerant_ap(dev, apdev):
    """Associated STA reports 40 MHz intolerant AP after association"""
    clear_scan_cache(apdev[0])
    params = {"ssid": "ht",
              "channel": "6",
              "ht_capab": "[HT40-]",
              "obss_interval": "3"}
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].connect("ht", key_mgmt="NONE", scan_freq="2437")

    if hapd.get_status_field("secondary_channel") != "-1":
        raise Exception("Unexpected secondary channel information")

    logger.info("Start 40 MHz intolerant AP")
    params = {"ssid": "intolerant",
              "channel": "5",
              "ht_capab": "[40-INTOLERANT]"}
    hapd2 = hostapd.add_ap(apdev[1], params)

    logger.info("Waiting for co-ex report from STA")
    ok = False
    for i in range(4):
        ev = dev[0].wait_event(['CTRL-EVENT-SCAN-RESULTS'], timeout=20)
        if ev is None:
            raise Exception("No OBSS scan seen")
        time.sleep(1)
        if hapd.get_status_field("secondary_channel") == "0":
            logger.info("AP moved to 20 MHz channel")
            ok = True
            break
    if not ok:
        raise Exception("AP did not move to 20 MHz channel")

    if "OK" not in hapd2.request("DISABLE"):
        raise Exception("Failed to disable 40 MHz intolerant AP")

    # make sure the intolerant AP disappears from scan results more quickly
    dev[0].scan(type="ONLY", freq="2432", only_new=True)
    dev[0].scan(type="ONLY", freq="2432", only_new=True)
    dev[0].dump_monitor()

    logger.info("Waiting for AP to move back to 40 MHz channel")
    ok = False
    for i in range(0, 30):
        time.sleep(1)
        if hapd.get_status_field("secondary_channel") == "-1":
            logger.info("AP moved to 40 MHz channel")
            ok = True
            break
    if not ok:
        raise Exception("AP did not move to 40 MHz channel")

def test_ap_ht40_csa(dev, apdev):
    """HT with 40 MHz channel width and CSA"""
    csa_supported(dev[0])
    try:
        hapd = None
        params = {"ssid": "ht",
                  "country_code": "US",
                  "hw_mode": "a",
                  "channel": "36",
                  "ht_capab": "[HT40+]",
                  "ieee80211n": "1"}
        hapd = hostapd.add_ap(apdev[0], params)

        dev[0].connect("ht", key_mgmt="NONE", scan_freq="5180")
        hwsim_utils.test_connectivity(dev[0], hapd)

        hapd.request("CHAN_SWITCH 5 5200 ht sec_channel_offset=-1 bandwidth=40")
        ev = hapd.wait_event(["AP-CSA-FINISHED"], timeout=10)
        if ev is None:
            raise Exception("CSA finished event timed out")
        if "freq=5200" not in ev:
            raise Exception("Unexpected channel in CSA finished event")
        ev = dev[0].wait_event(["CTRL-EVENT-DISCONNECTED"], timeout=0.5)
        if ev is not None:
            raise Exception("Unexpected STA disconnection during CSA")
        hwsim_utils.test_connectivity(dev[0], hapd)

        hapd.request("CHAN_SWITCH 5 5180 ht sec_channel_offset=1 bandwidth=40")
        ev = hapd.wait_event(["AP-CSA-FINISHED"], timeout=10)
        if ev is None:
            raise Exception("CSA finished event timed out")
        if "freq=5180" not in ev:
            raise Exception("Unexpected channel in CSA finished event")
        ev = dev[0].wait_event(["CTRL-EVENT-DISCONNECTED"], timeout=0.5)
        if ev is not None:
            raise Exception("Unexpected STA disconnection during CSA")
        hwsim_utils.test_connectivity(dev[0], hapd)
    finally:
        dev[0].request("DISCONNECT")
        if hapd:
            hapd.request("DISABLE")
        set_world_reg(apdev[0], None, dev[0])
        dev[0].flush_scan_cache()

def test_ap_ht40_csa2(dev, apdev):
    """HT with 40 MHz channel width and CSA"""
    csa_supported(dev[0])
    try:
        hapd = None
        params = {"ssid": "ht",
                  "country_code": "US",
                  "hw_mode": "a",
                  "channel": "36",
                  "ht_capab": "[HT40+]",
                  "ieee80211n": "1"}
        hapd = hostapd.add_ap(apdev[0], params)

        dev[0].connect("ht", key_mgmt="NONE", scan_freq="5180")
        hwsim_utils.test_connectivity(dev[0], hapd)

        hapd.request("CHAN_SWITCH 5 5220 ht sec_channel_offset=1 bandwidth=40")
        ev = hapd.wait_event(["AP-CSA-FINISHED"], timeout=10)
        if ev is None:
            raise Exception("CSA finished event timed out")
        if "freq=5220" not in ev:
            raise Exception("Unexpected channel in CSA finished event")
        ev = dev[0].wait_event(["CTRL-EVENT-DISCONNECTED"], timeout=0.5)
        if ev is not None:
            raise Exception("Unexpected STA disconnection during CSA")
        hwsim_utils.test_connectivity(dev[0], hapd)

        hapd.request("CHAN_SWITCH 5 5180 ht sec_channel_offset=1 bandwidth=40")
        ev = hapd.wait_event(["AP-CSA-FINISHED"], timeout=10)
        if ev is None:
            raise Exception("CSA finished event timed out")
        if "freq=5180" not in ev:
            raise Exception("Unexpected channel in CSA finished event")
        ev = dev[0].wait_event(["CTRL-EVENT-DISCONNECTED"], timeout=0.5)
        if ev is not None:
            raise Exception("Unexpected STA disconnection during CSA")
        hwsim_utils.test_connectivity(dev[0], hapd)
    finally:
        dev[0].request("DISCONNECT")
        if hapd:
            hapd.request("DISABLE")
        set_world_reg(apdev[0], None, dev[0])
        dev[0].flush_scan_cache()

def test_ap_ht40_csa3(dev, apdev):
    """HT with 40 MHz channel width and CSA"""
    csa_supported(dev[0])
    try:
        hapd = None
        params = {"ssid": "ht",
                  "country_code": "US",
                  "hw_mode": "a",
                  "channel": "36",
                  "ht_capab": "[HT40+]",
                  "ieee80211n": "1"}
        hapd = hostapd.add_ap(apdev[0], params)

        dev[0].connect("ht", key_mgmt="NONE", scan_freq="5180")
        hwsim_utils.test_connectivity(dev[0], hapd)

        hapd.request("CHAN_SWITCH 5 5240 ht sec_channel_offset=-1 bandwidth=40")
        ev = hapd.wait_event(["AP-CSA-FINISHED"], timeout=10)
        if ev is None:
            raise Exception("CSA finished event timed out")
        if "freq=5240" not in ev:
            raise Exception("Unexpected channel in CSA finished event")
        ev = dev[0].wait_event(["CTRL-EVENT-DISCONNECTED"], timeout=0.5)
        if ev is not None:
            raise Exception("Unexpected STA disconnection during CSA")
        hwsim_utils.test_connectivity(dev[0], hapd)

        hapd.request("CHAN_SWITCH 5 5180 ht sec_channel_offset=1 bandwidth=40")
        ev = hapd.wait_event(["AP-CSA-FINISHED"], timeout=10)
        if ev is None:
            raise Exception("CSA finished event timed out")
        if "freq=5180" not in ev:
            raise Exception("Unexpected channel in CSA finished event")
        ev = dev[0].wait_event(["CTRL-EVENT-DISCONNECTED"], timeout=0.5)
        if ev is not None:
            raise Exception("Unexpected STA disconnection during CSA")
        hwsim_utils.test_connectivity(dev[0], hapd)
    finally:
        dev[0].request("DISCONNECT")
        if hapd:
            hapd.request("DISABLE")
        set_world_reg(apdev[0], None, dev[0])
        dev[0].flush_scan_cache()

def test_ap_ht_20_to_40_csa(dev, apdev):
    """HT with 20 MHz channel width doing CSA to 40 MHz"""
    csa_supported(dev[0])

    params = {"ssid": "ht",
              "channel": "1",
              "ieee80211n": "1"}
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].connect("ht", key_mgmt="NONE", scan_freq="2412")
    hapd.wait_sta()
    res = dev[0].request("SIGNAL_POLL")
    logger.info("SIGNAL_POLL:\n" + res)
    sig = res.splitlines()
    if 'WIDTH=20 MHz' not in sig:
        raise Exception("20 MHz channel bandwidth not used on the original channel")

    hapd.request("CHAN_SWITCH 5 2462 ht sec_channel_offset=-1 bandwidth=40")
    ev = hapd.wait_event(["AP-CSA-FINISHED"], timeout=10)
    if ev is None:
        raise Exception("CSA finished event timed out")
    if "freq=2462" not in ev:
        raise Exception("Unexpected channel in CSA finished event")
    ev = dev[0].wait_event(["CTRL-EVENT-DISCONNECTED"], timeout=0.5)
    if ev is not None:
        raise Exception("Unexpected STA disconnection during CSA")
    res = dev[0].request("SIGNAL_POLL")
    logger.info("SIGNAL_POLL:\n" + res)
    sig = res.splitlines()
    if 'WIDTH=40 MHz' not in sig:
        raise Exception("40 MHz channel bandwidth not used on the new channel")

@remote_compatible
def test_prefer_ht20(dev, apdev):
    """Preference on HT20 over no-HT"""
    params = {"ssid": "test",
              "channel": "1",
              "ieee80211n": "0"}
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = apdev[0]['bssid']
    params = {"ssid": "test",
              "channel": "1",
              "ieee80211n": "1"}
    hapd2 = hostapd.add_ap(apdev[1], params)
    bssid2 = apdev[1]['bssid']

    dev[0].scan_for_bss(bssid, freq=2412)
    dev[0].scan_for_bss(bssid2, freq=2412)
    dev[0].connect("test", key_mgmt="NONE", scan_freq="2412")
    if dev[0].get_status_field('bssid') != bssid2:
        raise Exception("Unexpected BSS selected")

    est = dev[0].get_bss(bssid)['est_throughput']
    if est != "54000":
        raise Exception("Unexpected BSS0 est_throughput: " + est)

    est = dev[0].get_bss(bssid2)['est_throughput']
    if est != "65000":
        raise Exception("Unexpected BSS1 est_throughput: " + est)

def test_prefer_ht40(dev, apdev):
    """Preference on HT40 over HT20"""
    params = {"ssid": "test",
              "channel": "1",
              "ieee80211n": "1"}
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = apdev[0]['bssid']
    params = {"ssid": "test",
              "channel": "1",
              "ieee80211n": "1",
              "ht_capab": "[HT40+]"}
    hapd2 = hostapd.add_ap(apdev[1], params)
    bssid2 = apdev[1]['bssid']

    dev[0].scan_for_bss(bssid, freq=2412)
    dev[0].scan_for_bss(bssid2, freq=2412)
    dev[0].connect("test", key_mgmt="NONE", scan_freq="2412")
    if dev[0].get_status_field('bssid') != bssid2:
        raise Exception("Unexpected BSS selected")

    est = dev[0].get_bss(bssid)['est_throughput']
    if est != "65000":
        raise Exception("Unexpected BSS0 est_throughput: " + est)

    est = dev[0].get_bss(bssid2)['est_throughput']
    if est != "135000":
        raise Exception("Unexpected BSS1 est_throughput: " + est)

@remote_compatible
def test_prefer_ht20_during_roam(dev, apdev):
    """Preference on HT20 over no-HT in roaming consideration"""
    params = {"ssid": "test",
              "channel": "1",
              "ieee80211n": "0"}
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = apdev[0]['bssid']

    dev[0].scan_for_bss(bssid, freq=2412)
    dev[0].connect("test", key_mgmt="NONE", scan_freq="2412")

    params = {"ssid": "test",
              "channel": "1",
              "ieee80211n": "1"}
    hapd2 = hostapd.add_ap(apdev[1], params)
    bssid2 = apdev[1]['bssid']
    dev[0].scan_for_bss(bssid2, freq=2412)
    dev[0].scan(freq=2412)
    dev[0].wait_connected()

    if dev[0].get_status_field('bssid') != bssid2:
        raise Exception("Unexpected BSS selected")

@remote_compatible
def test_ap_ht40_5ghz_invalid_pair(dev, apdev):
    """HT40 on 5 GHz with invalid channel pair"""
    clear_scan_cache(apdev[0])
    try:
        params = {"ssid": "test-ht40",
                  "hw_mode": "a",
                  "channel": "40",
                  "country_code": "US",
                  "ht_capab": "[HT40+]"}
        hapd = hostapd.add_ap(apdev[0], params, wait_enabled=False)
        ev = hapd.wait_event(["AP-DISABLED", "AP-ENABLED"], timeout=10)
        if not ev:
            raise Exception("AP setup failure timed out")
        if "AP-ENABLED" in ev:
            sec = hapd.get_status_field("secondary_channel")
            if sec != "0":
                raise Exception("Invalid 40 MHz channel accepted")
    finally:
        clear_regdom(hapd, dev)

@remote_compatible
def test_ap_ht40_5ghz_disabled_sec(dev, apdev):
    """HT40 on 5 GHz with disabled secondary channel"""
    clear_scan_cache(apdev[0])
    try:
        params = {"ssid": "test-ht40",
                  "hw_mode": "a",
                  "channel": "48",
                  "country_code": "US",
                  "ht_capab": "[HT40+]"}
        hapd = hostapd.add_ap(apdev[0], params, wait_enabled=False)
        ev = hapd.wait_event(["AP-DISABLED", "AP-ENABLED"], timeout=10)
        if not ev:
            raise Exception("AP setup failure timed out")
        if "AP-ENABLED" in ev:
            sec = hapd.get_status_field("secondary_channel")
            if sec != "0":
                raise Exception("Invalid 40 MHz channel accepted")
    finally:
        clear_regdom(hapd, dev)

def test_ap_ht40_scan_broken_ap(dev, apdev):
    """HT40 co-ex scan and broken legacy/HT AP"""
    clear_scan_cache(apdev[0])

    # Broken AP: Include HT Capabilities element but not HT Operation element
    params = {"ssid": "legacy-20",
              "channel": "7", "ieee80211n": "0",
              "wmm_enabled": "1",
              "vendor_elements": "2d1a0e001bffff000000000000000000000100000000000000000000"}
    hapd2 = hostapd.add_ap(apdev[1], params)

    params = {"ssid": "test-ht40",
              "channel": "5",
              "ht_capab": "[HT40-]"}
    hapd = hostapd.add_ap(apdev[0], params, wait_enabled=False)

    state = hapd.get_status_field("state")
    if state != "HT_SCAN":
        time.sleep(0.1)
        state = hapd.get_status_field("state")
        if state != "HT_SCAN":
            raise Exception("Unexpected interface state - expected HT_SCAN")

    ev = hapd.wait_event(["AP-ENABLED"], timeout=10)
    if not ev:
        raise Exception("AP setup timed out")

    state = hapd.get_status_field("state")
    if state != "ENABLED":
        raise Exception("Unexpected interface state - expected ENABLED")

    freq = hapd.get_status_field("freq")
    if freq != "2432":
        raise Exception("Unexpected frequency: " + freq)
    pri = hapd.get_status_field("channel")
    if pri != "5":
        raise Exception("Unexpected primary channel: " + pri)
    sec = hapd.get_status_field("secondary_channel")
    if sec != "-1":
        raise Exception("Unexpected secondary channel: " + sec)

    dev[0].connect("test-ht40", key_mgmt="NONE", scan_freq=freq)
    dev[1].connect("legacy-20", key_mgmt="NONE", scan_freq="2442")
    hwsim_utils.test_connectivity(dev[0], hapd)
    hwsim_utils.test_connectivity(dev[1], hapd2)

def run_op_class(dev, apdev, hw_mode, channel, country, ht_capab, sec_chan,
                 freq, opclass, use_op_class=False):
    clear_scan_cache(apdev[0])
    try:
        params = {"ssid": "test-ht40",
                  "hw_mode": hw_mode,
                  "channel": channel,
                  "ht_capab": ht_capab}
        if use_op_class:
            params['op_class'] = str(opclass)
        if country:
            params['country_code'] = country
        hapd = hostapd.add_ap(apdev[0], params, wait_enabled=False)
        ev = hapd.wait_event(["AP-DISABLED", "AP-ENABLED"], timeout=10)
        if not ev:
            raise Exception("AP setup failure timed out")
        if "AP-DISABLED" in ev:
            raise HwsimSkip("Channel not supported")
        sec = hapd.get_status_field("secondary_channel")
        if sec != sec_chan:
            raise Exception("Unexpected secondary_channel: " + sec)
        dev[0].connect("test-ht40", key_mgmt="NONE", scan_freq=freq)
        bss = dev[0].get_bss(hapd.own_addr())
        ie = parse_ie(bss['ie'])
        if 59 not in ie:
            raise Exception("Missing Supported Operating Classes element")
        rx_opclass, = struct.unpack('B', ie[59][0:1])
        if rx_opclass != opclass:
            raise Exception("Unexpected operating class: %d" % rx_opclass)
        hapd.disable()
        hapd.dump_monitor()
        dev[0].request("REMOVE_NETWORK all")
        dev[0].request("ABORT_SCAN")
        dev[0].wait_disconnected()
        dev[0].dump_monitor()
    finally:
        set_world_reg(apdev[0], None, dev[0])
        time.sleep(0.1)

def test_ap_ht_op_class_81(dev, apdev):
    """HT20 on operationg class 81"""
    for o in [False, True]:
        run_op_class(dev, apdev, "g", "1", None, "", "0", "2412", 81,
                     use_op_class=o)

def test_ap_ht_op_class_83(dev, apdev):
    """HT40 on operationg class 83"""
    for o in [False, True]:
        run_op_class(dev, apdev, "g", "1", None, "[HT40+]", "1", "2412", 83,
                     use_op_class=o)

def test_ap_ht_op_class_84(dev, apdev):
    """HT40 on operationg class 84"""
    for o in [False, True]:
        run_op_class(dev, apdev, "g", "11", None, "[HT40-]", "-1", "2462", 84,
                     use_op_class=o)

def test_ap_ht_op_class_115(dev, apdev):
    """HT20 on operationg class 115"""
    for o in [False, True]:
        run_op_class(dev, apdev, "a", "36", "FI", "", "0", "5180", 115,
                     use_op_class=o)

def test_ap_ht_op_class_116(dev, apdev):
    """HT40 on operationg class 116"""
    for o in [False, True]:
        run_op_class(dev, apdev, "a", "36", "FI", "[HT40+]", "1", "5180", 116,
                     use_op_class=o)

def test_ap_ht_op_class_117(dev, apdev):
    """HT40 on operationg class 117"""
    for o in [False, True]:
        run_op_class(dev, apdev, "a", "40", "FI", "[HT40-]", "-1", "5200", 117,
                     use_op_class=o)

def test_ap_ht_op_class_118(dev, apdev):
    """HT20 on operationg class 118"""
    for o in [False, True]:
        run_op_class(dev, apdev, "a", "60", "PA", "", "0", "5300", 118,
                     use_op_class=o)

def test_ap_ht_op_class_119(dev, apdev):
    """HT40 on operationg class 119"""
    for o in [False, True]:
        run_op_class(dev, apdev, "a", "60", "PA", "[HT40+]", "1", "5300", 119,
                     use_op_class=o)

def test_ap_ht_op_class_120(dev, apdev):
    """HT40 on operationg class 120"""
    for o in [False, True]:
        run_op_class(dev, apdev, "a", "64", "PA", "[HT40-]", "-1", "5320", 120,
                     use_op_class=o)

def test_ap_ht_op_class_121(dev, apdev):
    """HT20 on operationg class 121"""
    for o in [False, True]:
        run_op_class(dev, apdev, "a", "100", "ZA", "", "0", "5500", 121,
                     use_op_class=o)

def test_ap_ht_op_class_122(dev, apdev):
    """HT40 on operationg class 122"""
    for o in [False, True]:
        run_op_class(dev, apdev, "a", "100", "ZA", "[HT40+]", "1", "5500", 122,
                     use_op_class=o)

def test_ap_ht_op_class_123(dev, apdev):
    """HT40 on operationg class 123"""
    for o in [False, True]:
        run_op_class(dev, apdev, "a", "104", "ZA", "[HT40-]", "-1", "5520", 123,
                     use_op_class=o)

def test_ap_ht_op_class_124(dev, apdev):
    """HT20 on operationg class 124"""
    for o in [False, True]:
        run_op_class(dev, apdev, "a", "149", "US", "", "0", "5745", 124,
                     use_op_class=o)

def test_ap_ht_op_class_125(dev, apdev):
    """HT20 on operationg class 125"""
    for o in [False, True]:
        run_op_class(dev, apdev, "a", "169", "NL", "", "0", "5845", 125,
                     use_op_class=o)

def test_ap_ht_op_class_126(dev, apdev):
    """HT40 on operationg class 126"""
    for o in [False, True]:
        run_op_class(dev, apdev, "a", "149", "US", "[HT40+]", "1", "5745", 126,
                     use_op_class=o)

def test_ap_ht_op_class_127(dev, apdev):
    """HT40 on operationg class 127"""
    for o in [False, True]:
        run_op_class(dev, apdev, "a", "153", "US", "[HT40-]", "-1", "5765", 127,
                     use_op_class=o)

def test_ap_ht40_plus_minus1(dev, apdev):
    """HT40 with both plus and minus allowed (1)"""
    clear_scan_cache(apdev[0])
    params = {"ssid": "test-ht40",
              "channel": "11",
              "ht_capab": "[HT40+][HT40-]"}
    hapd = hostapd.add_ap(apdev[0], params)

    freq = hapd.get_status_field("freq")
    if freq != "2462":
        raise Exception("Unexpected frequency: " + freq)
    pri = hapd.get_status_field("channel")
    if pri != "11":
        raise Exception("Unexpected primary channel: " + pri)
    sec = hapd.get_status_field("secondary_channel")
    if sec != "-1":
        raise Exception("Unexpected secondary channel: " + sec)

    dev[0].connect("test-ht40", key_mgmt="NONE", scan_freq=freq)

def test_ap_ht40_plus_minus2(dev, apdev):
    """HT40 with both plus and minus allowed (2)"""
    clear_scan_cache(apdev[0])
    params = {"ssid": "test-ht40",
              "channel": "1",
              "ht_capab": "[HT40+][HT40-]"}
    hapd = hostapd.add_ap(apdev[0], params)

    freq = hapd.get_status_field("freq")
    if freq != "2412":
        raise Exception("Unexpected frequency: " + freq)
    pri = hapd.get_status_field("channel")
    if pri != "1":
        raise Exception("Unexpected primary channel: " + pri)
    sec = hapd.get_status_field("secondary_channel")
    if sec != "1":
        raise Exception("Unexpected secondary channel: " + sec)

    dev[0].connect("test-ht40", key_mgmt="NONE", scan_freq=freq)

def test_ap_ht40_disable(dev, apdev):
    """HT40 disabling"""
    clear_scan_cache(apdev[0])
    params = {"ssid": "test-ht40",
              "channel": "6",
              "ht_capab": "[HT40-]"}
    hapd = hostapd.add_ap(apdev[0], params)

    sec = hapd.get_status_field("secondary_channel")
    if sec != "-1":
        raise Exception("Unexpected secondary channel: " + sec)

    id = dev[0].connect("test-ht40", key_mgmt="NONE", scan_freq="2437")
    sig = dev[0].request("SIGNAL_POLL").splitlines()
    logger.info("SIGNAL_POLL: " + str(sig))
    if "WIDTH=40 MHz" not in sig:
        raise Exception("Station did not report 40 MHz bandwidth")
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    hapd.disable()
    hapd.set("ht_capab", "")
    hapd.enable()
    sec = hapd.get_status_field("secondary_channel")
    if sec != "0":
        raise Exception("Unexpected secondary channel(2): " + sec)

    dev[0].flush_scan_cache()
    dev[0].select_network(id, freq=2437)
    dev[0].wait_connected()
    sig = dev[0].request("SIGNAL_POLL").splitlines()
    logger.info("SIGNAL_POLL: " + str(sig))
    if "WIDTH=20 MHz" not in sig:
        raise Exception("Station did not report 20 MHz bandwidth")

def test_ap_ht_wmm_etsi(dev, apdev):
    """HT and WMM contents in ETSI"""
    run_ap_ht_wmm(dev, apdev, "FI")

def test_ap_ht_wmm_fcc(dev, apdev):
    """HT and WMM contents in FCC"""
    run_ap_ht_wmm(dev, apdev, "US")

def run_ap_ht_wmm(dev, apdev, country):
    clear_scan_cache(apdev[0])
    try:
        hapd = None
        params = {"ssid": "test",
                  "hw_mode": "a",
                  "channel": "36",
                  "country_code": country}
        hapd = hostapd.add_ap(apdev[0], params)
        freq = hapd.get_status_field("freq")
        bssid = hapd.own_addr()
        dev[0].connect("test", key_mgmt="NONE", scan_freq=freq)
        bss = dev[0].get_bss(bssid)
        ie = parse_ie(bss['ie'])
        if 221 not in ie:
            raise Exception("Could not find WMM IE")
        wmm = ie[221]
        if len(wmm) != 24:
            raise Exception("Unexpected WMM IE length")
        id, subtype, version, info, reserved = struct.unpack('>LBBBB', wmm[0:8])
        if id != 0x0050f202 or subtype != 1 or version != 1:
            raise Exception("Not a WMM IE")
        ac = []
        for i in range(4):
            ac.append(struct.unpack('<BBH', wmm[8 + i * 4: 12 + i * 4]))
        logger.info("WMM AC info: " + str(ac))

        aifsn = (ac[0][0] & 0x0f, ac[1][0] & 0x0f,
                 ac[2][0] & 0x0f, ac[3][0] & 0x0f)
        logger.info("AIFSN: " + str(aifsn))
        if aifsn != (3, 7, 2, 2):
            raise Exception("Unexpected AIFSN value: " + str(aifsn))

        ecw_min = (ac[0][1] & 0x0f, ac[1][1] & 0x0f,
                   ac[2][1] & 0x0f, ac[3][1] & 0x0f)
        logger.info("ECW min: " + str(ecw_min))
        if ecw_min != (4, 4, 3, 2):
            raise Exception("Unexpected ECW min value: " + str(ecw_min))

        ecw_max = ((ac[0][1] & 0xf0) >> 4, (ac[1][1] & 0xf0) >> 4,
                   (ac[2][1] & 0xf0) >> 4, (ac[3][1] & 0xf0) >> 4)
        logger.info("ECW max: " + str(ecw_max))
        if ecw_max != (10, 10, 4, 3):
            raise Exception("Unexpected ECW max value: " + str(ecw_max))

        txop = (ac[0][2], ac[1][2], ac[2][2], ac[3][2])
        logger.info("TXOP: " + str(txop))
        if txop != (0, 0, 94, 47):
            raise Exception("Unexpected TXOP value: " + str(txop))
    finally:
        dev[0].request("DISCONNECT")
        if hapd:
            hapd.request("DISABLE")
        set_world_reg(apdev[0], None, dev[0])
        dev[0].flush_scan_cache()
