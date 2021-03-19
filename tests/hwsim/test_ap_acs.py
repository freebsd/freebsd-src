# Test cases for automatic channel selection with hostapd
# Copyright (c) 2013-2018, Jouni Malinen <j@w1.fi>
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

import logging
logger = logging.getLogger()
import time

import hostapd
from utils import *
from test_dfs import wait_dfs_event

def force_prev_ap_on_24g(ap):
    # For now, make sure the last operating channel was on 2.4 GHz band to get
    # sufficient survey data from mac80211_hwsim.
    hostapd.add_ap(ap, {"ssid": "open"})
    time.sleep(0.1)
    hostapd.remove_bss(ap)

def force_prev_ap_on_5g(ap):
    # For now, make sure the last operating channel was on 5 GHz band to get
    # sufficient survey data from mac80211_hwsim.
    hostapd.add_ap(ap, {"ssid": "open", "hw_mode": "a",
                        "channel": "36", "country_code": "US"})
    time.sleep(0.1)
    hostapd.remove_bss(ap)

def wait_acs(hapd, return_after_acs=False):
    ev = hapd.wait_event(["ACS-STARTED", "ACS-COMPLETED", "ACS-FAILED",
                          "AP-ENABLED", "AP-DISABLED"], timeout=5)
    if not ev:
        raise Exception("ACS start timed out")
    if "ACS-STARTED" not in ev:
        raise Exception("Unexpected ACS event: " + ev)

    state = hapd.get_status_field("state")
    if state != "ACS":
        raise Exception("Unexpected interface state %s (expected ACS)" % state)

    ev = hapd.wait_event(["ACS-COMPLETED", "ACS-FAILED", "AP-ENABLED",
                          "AP-DISABLED"], timeout=20)
    if not ev:
        raise Exception("ACS timed out")
    if "ACS-COMPLETED" not in ev:
        raise Exception("Unexpected ACS event: " + ev)

    if return_after_acs:
        return

    ev = hapd.wait_event(["AP-ENABLED", "AP-DISABLED"], timeout=5)
    if not ev:
        raise Exception("AP setup timed out")
    if "AP-ENABLED" not in ev:
        raise Exception("Unexpected ACS event: " + ev)

    state = hapd.get_status_field("state")
    if state != "ENABLED":
        raise Exception("Unexpected interface state %s (expected ENABLED)" % state)

def test_ap_acs(dev, apdev):
    """Automatic channel selection"""
    force_prev_ap_on_24g(apdev[0])
    params = hostapd.wpa2_params(ssid="test-acs", passphrase="12345678")
    params['channel'] = '0'
    hapd = hostapd.add_ap(apdev[0], params, wait_enabled=False)
    wait_acs(hapd)

    freq = hapd.get_status_field("freq")
    if int(freq) < 2400:
        raise Exception("Unexpected frequency")

    dev[0].connect("test-acs", psk="12345678", scan_freq=freq)

def test_ap_acs_chanlist(dev, apdev):
    """Automatic channel selection with chanlist set"""
    force_prev_ap_on_24g(apdev[0])
    params = hostapd.wpa2_params(ssid="test-acs", passphrase="12345678")
    params['channel'] = '0'
    params['chanlist'] = '1 6 11'
    hapd = hostapd.add_ap(apdev[0], params, wait_enabled=False)
    wait_acs(hapd)

    freq = hapd.get_status_field("freq")
    if int(freq) < 2400:
        raise Exception("Unexpected frequency")

    dev[0].connect("test-acs", psk="12345678", scan_freq=freq)

def test_ap_acs_freqlist(dev, apdev):
    """Automatic channel selection with freqlist set"""
    run_ap_acs_freqlist(dev, apdev, [2412, 2437, 2462])

def test_ap_acs_freqlist2(dev, apdev):
    """Automatic channel selection with freqlist set"""
    run_ap_acs_freqlist(dev, apdev, [2417, 2432, 2457])

def run_ap_acs_freqlist(dev, apdev, freqlist):
    force_prev_ap_on_24g(apdev[0])
    params = hostapd.wpa2_params(ssid="test-acs", passphrase="12345678")
    params['channel'] = '0'
    params['freqlist'] = ','.join([str(x) for x in freqlist])
    hapd = hostapd.add_ap(apdev[0], params, wait_enabled=False)
    wait_acs(hapd)

    freq = int(hapd.get_status_field("freq"))
    if freq not in freqlist:
        raise Exception("Unexpected frequency: %d" % freq)

    dev[0].connect("test-acs", psk="12345678", scan_freq=str(freq))

def test_ap_acs_invalid_chanlist(dev, apdev):
    """Automatic channel selection with invalid chanlist"""
    force_prev_ap_on_24g(apdev[0])
    params = hostapd.wpa2_params(ssid="test-acs", passphrase="12345678")
    params['channel'] = '0'
    params['chanlist'] = '15-18'
    hapd = hostapd.add_ap(apdev[0], params, no_enable=True)
    res = hapd.request("ENABLE")
    if "OK" in res:
        raise Exception("ENABLE command succeeded unexpectedly")

def test_ap_multi_bss_acs(dev, apdev):
    """hostapd start with a multi-BSS configuration file using ACS"""
    skip_with_fips(dev[0])
    check_sae_capab(dev[2])
    force_prev_ap_on_24g(apdev[0])

    # start the actual test
    hapd = hostapd.add_iface(apdev[0], 'multi-bss-acs.conf')
    hapd.enable()
    wait_acs(hapd)

    freq = hapd.get_status_field("freq")
    if int(freq) < 2400:
        raise Exception("Unexpected frequency")

    dev[0].connect("bss-1", key_mgmt="NONE", scan_freq=freq)
    dev[1].connect("bss-2", psk="12345678", scan_freq=freq)
    dev[2].set("sae_groups", "")
    dev[2].connect("bss-3", key_mgmt="SAE", psk="qwertyuiop", scan_freq=freq)

def test_ap_acs_40mhz(dev, apdev):
    """Automatic channel selection for 40 MHz channel"""
    run_ap_acs_40mhz(dev, apdev, '[HT40+]')

def test_ap_acs_40mhz_plus_or_minus(dev, apdev):
    """Automatic channel selection for 40 MHz channel (plus or minus)"""
    run_ap_acs_40mhz(dev, apdev, '[HT40+][HT40-]')

def run_ap_acs_40mhz(dev, apdev, ht_capab):
    clear_scan_cache(apdev[0])
    force_prev_ap_on_24g(apdev[0])
    params = hostapd.wpa2_params(ssid="test-acs", passphrase="12345678")
    params['channel'] = '0'
    params['ht_capab'] = ht_capab
    hapd = hostapd.add_ap(apdev[0], params, wait_enabled=False)
    wait_acs(hapd)

    freq = hapd.get_status_field("freq")
    if int(freq) < 2400:
        raise Exception("Unexpected frequency")
    sec = hapd.get_status_field("secondary_channel")
    if int(sec) == 0:
        raise Exception("Secondary channel not set")

    dev[0].connect("test-acs", psk="12345678", scan_freq=freq)

def test_ap_acs_40mhz_minus(dev, apdev):
    """Automatic channel selection for HT40- channel"""
    clear_scan_cache(apdev[0])
    force_prev_ap_on_24g(apdev[0])
    params = hostapd.wpa2_params(ssid="test-acs", passphrase="12345678")
    params['channel'] = '0'
    params['ht_capab'] = '[HT40-]'
    params['acs_num_scans'] = '1'
    params['chanlist'] = '1 11'
    hapd = hostapd.add_ap(apdev[0], params, wait_enabled=False)
    ev = hapd.wait_event(["AP-ENABLED", "AP-DISABLED"], timeout=10)
    if not ev:
        raise Exception("ACS start timed out")
    # HT40- is not currently supported in hostapd ACS, so do not try to connect
    # or verify that this operation succeeded.

def test_ap_acs_5ghz(dev, apdev):
    """Automatic channel selection on 5 GHz"""
    try:
        hapd = None
        force_prev_ap_on_5g(apdev[0])
        params = hostapd.wpa2_params(ssid="test-acs", passphrase="12345678")
        params['hw_mode'] = 'a'
        params['channel'] = '0'
        params['country_code'] = 'US'
        hapd = hostapd.add_ap(apdev[0], params, wait_enabled=False)
        wait_acs(hapd)
        freq = hapd.get_status_field("freq")
        if int(freq) < 5000:
            raise Exception("Unexpected frequency")

        dev[0].connect("test-acs", psk="12345678", scan_freq=freq)
        dev[0].wait_regdom(country_ie=True)
    finally:
        clear_regdom(hapd, dev)

def test_ap_acs_5ghz_40mhz(dev, apdev):
    """Automatic channel selection on 5 GHz for 40 MHz channel"""
    try:
        hapd = None
        force_prev_ap_on_5g(apdev[0])
        params = hostapd.wpa2_params(ssid="test-acs", passphrase="12345678")
        params['hw_mode'] = 'a'
        params['channel'] = '0'
        params['ht_capab'] = '[HT40+]'
        params['country_code'] = 'US'
        hapd = hostapd.add_ap(apdev[0], params, wait_enabled=False)
        wait_acs(hapd)
        freq = hapd.get_status_field("freq")
        if int(freq) < 5000:
            raise Exception("Unexpected frequency")

        sec = hapd.get_status_field("secondary_channel")
        if int(sec) == 0:
            raise Exception("Secondary channel not set")

        dev[0].connect("test-acs", psk="12345678", scan_freq=freq)
        dev[0].wait_regdom(country_ie=True)
    finally:
        clear_regdom(hapd, dev)

def test_ap_acs_vht(dev, apdev):
    """Automatic channel selection for VHT"""
    try:
        hapd = None
        force_prev_ap_on_5g(apdev[0])
        params = hostapd.wpa2_params(ssid="test-acs", passphrase="12345678")
        params['hw_mode'] = 'a'
        params['channel'] = '0'
        params['ht_capab'] = '[HT40+]'
        params['country_code'] = 'US'
        params['ieee80211ac'] = '1'
        params['vht_oper_chwidth'] = '1'
        hapd = hostapd.add_ap(apdev[0], params, wait_enabled=False)
        wait_acs(hapd)
        freq = hapd.get_status_field("freq")
        if int(freq) < 5000:
            raise Exception("Unexpected frequency")

        sec = hapd.get_status_field("secondary_channel")
        if int(sec) == 0:
            raise Exception("Secondary channel not set")

        dev[0].connect("test-acs", psk="12345678", scan_freq=freq)
        dev[0].wait_regdom(country_ie=True)
    finally:
        clear_regdom(hapd, dev)

def test_ap_acs_vht40(dev, apdev):
    """Automatic channel selection for VHT40"""
    try:
        hapd = None
        force_prev_ap_on_5g(apdev[0])
        params = hostapd.wpa2_params(ssid="test-acs", passphrase="12345678")
        params['hw_mode'] = 'a'
        params['channel'] = '0'
        params['ht_capab'] = '[HT40+]'
        params['country_code'] = 'US'
        params['ieee80211ac'] = '1'
        params['vht_oper_chwidth'] = '0'
        params['acs_num_scans'] = '1'
        params['chanlist'] = '36 149'
        hapd = hostapd.add_ap(apdev[0], params, wait_enabled=False)
        wait_acs(hapd)
        freq = hapd.get_status_field("freq")
        if int(freq) < 5000:
            raise Exception("Unexpected frequency")

        sec = hapd.get_status_field("secondary_channel")
        if int(sec) == 0:
            raise Exception("Secondary channel not set")

        dev[0].connect("test-acs", psk="12345678", scan_freq=freq)
        dev[0].wait_regdom(country_ie=True)
    finally:
        clear_regdom(hapd, dev)

def test_ap_acs_vht80p80(dev, apdev):
    """Automatic channel selection for VHT 80+80"""
    try:
        hapd = None
        force_prev_ap_on_5g(apdev[0])
        params = hostapd.wpa2_params(ssid="test-acs", passphrase="12345678")
        params['hw_mode'] = 'a'
        params['channel'] = '0'
        params['ht_capab'] = '[HT40+]'
        params['country_code'] = 'US'
        params['ieee80211ac'] = '1'
        params['vht_oper_chwidth'] = '3'
        hapd = hostapd.add_ap(apdev[0], params, wait_enabled=False)
        ev = hapd.wait_event(["ACS-COMPLETED"], timeout=20)
        if ev is None:
            raise Exception("ACS did not complete")
        # ACS for 80+80 is not yet supported, so the AP setup itself will fail.
        # Do not try to connection before this gets fully supported.
        ev = hapd.wait_event(["AP-ENABLED", "AP-DISABLED"], timeout=10)
        if ev is None:
            raise Exception("AP enabled/disabled not reported")
    finally:
        clear_regdom(hapd, dev)

def test_ap_acs_vht160(dev, apdev):
    """Automatic channel selection for VHT160"""
    try:
        hapd = None
        force_prev_ap_on_5g(apdev[0])
        params = hostapd.wpa2_params(ssid="test-acs", passphrase="12345678")
        params['hw_mode'] = 'a'
        params['channel'] = '0'
        params['ht_capab'] = '[HT40+]'
        params['country_code'] = 'ZA'
        params['ieee80211ac'] = '1'
        params['vht_oper_chwidth'] = '2'
        params['ieee80211d'] = '1'
        params['ieee80211h'] = '1'
        params['chanlist'] = '100'
        params['acs_num_scans'] = '1'
        hapd = hostapd.add_ap(apdev[0], params, wait_enabled=False)
        ev = hapd.wait_event(["AP-ENABLED", "AP-DISABLED"], timeout=10)
        if not ev:
            raise Exception("ACS start timed out")
        # VHT160 is not currently supported in hostapd ACS, so do not try to
        # enforce successful AP start.
        if "AP-ENABLED" in ev:
            freq = hapd.get_status_field("freq")
            if int(freq) < 5000:
                raise Exception("Unexpected frequency")
            dev[0].connect("test-acs", psk="12345678", scan_freq=freq)
            dev[0].wait_regdom(country_ie=True)
    finally:
        clear_regdom(hapd, dev)

def test_ap_acs_vht160_scan_disable(dev, apdev):
    """Automatic channel selection for VHT160 and DISABLE during scan"""
    force_prev_ap_on_5g(apdev[0])
    params = hostapd.wpa2_params(ssid="test-acs", passphrase="12345678")
    params['hw_mode'] = 'a'
    params['channel'] = '0'
    params['ht_capab'] = '[HT40+]'
    params['country_code'] = 'ZA'
    params['ieee80211ac'] = '1'
    params['vht_oper_chwidth'] = '2'
    params["vht_oper_centr_freq_seg0_idx"] = "114"
    params['ieee80211d'] = '1'
    params['ieee80211h'] = '1'
    hapd = hostapd.add_ap(apdev[0], params, wait_enabled=False)
    time.sleep(3)
    clear_regdom(hapd, dev)

def test_ap_acs_bias(dev, apdev):
    """Automatic channel selection with bias values"""
    force_prev_ap_on_24g(apdev[0])
    params = hostapd.wpa2_params(ssid="test-acs", passphrase="12345678")
    params['channel'] = '0'
    params['acs_chan_bias'] = '1:0.8 3:1.2 6:0.7 11:0.8'
    hapd = hostapd.add_ap(apdev[0], params, wait_enabled=False)
    wait_acs(hapd)

    freq = hapd.get_status_field("freq")
    if int(freq) < 2400:
        raise Exception("Unexpected frequency")

    dev[0].connect("test-acs", psk="12345678", scan_freq=freq)

def test_ap_acs_survey(dev, apdev):
    """Automatic channel selection using acs_survey parameter"""
    force_prev_ap_on_24g(apdev[0])
    params = hostapd.wpa2_params(ssid="test-acs", passphrase="12345678")
    params['channel'] = 'acs_survey'
    params['acs_num_scans'] = '1'
    hapd = hostapd.add_ap(apdev[0], params, wait_enabled=False)
    wait_acs(hapd)

    freq = hapd.get_status_field("freq")
    if int(freq) < 2400:
        raise Exception("Unexpected frequency")

    dev[0].connect("test-acs", psk="12345678", scan_freq=freq)

def test_ap_acs_errors(dev, apdev):
    """Automatic channel selection failures"""
    clear_scan_cache(apdev[0])
    force_prev_ap_on_24g(apdev[0])
    params = hostapd.wpa2_params(ssid="test-acs", passphrase="12345678")
    params['channel'] = '0'
    params['acs_num_scans'] = '2'
    params['chanlist'] = '1'
    hapd = hostapd.add_ap(apdev[0], params, no_enable=True)

    with alloc_fail(hapd, 1, "acs_request_scan"):
        if "FAIL" not in hapd.request("ENABLE"):
            raise Exception("Unexpected success for ENABLE")

    hapd.dump_monitor()
    with fail_test(hapd, 1, "acs_request_scan"):
        if "FAIL" not in hapd.request("ENABLE"):
            raise Exception("Unexpected success for ENABLE")

    hapd.dump_monitor()
    with fail_test(hapd, 1, "acs_scan_complete"):
        hapd.enable()
        ev = hapd.wait_event(["AP-ENABLED", "AP-DISABLED"], timeout=10)
        if not ev:
            raise Exception("ACS start timed out")

    hapd.dump_monitor()
    with fail_test(hapd, 1, "acs_request_scan;acs_scan_complete"):
        hapd.enable()
        ev = hapd.wait_event(["AP-ENABLED", "AP-DISABLED"], timeout=10)
        if not ev:
            raise Exception("ACS start timed out")

@long_duration_test
def test_ap_acs_dfs(dev, apdev):
    """Automatic channel selection, HT scan, and DFS"""
    try:
        hapd = None
        force_prev_ap_on_5g(apdev[0])
        params = hostapd.wpa2_params(ssid="test-acs", passphrase="12345678")
        params['hw_mode'] = 'a'
        params['channel'] = '0'
        params['ht_capab'] = '[HT40+]'
        params['country_code'] = 'US'
        params['ieee80211d'] = '1'
        params['ieee80211h'] = '1'
        params['acs_num_scans'] = '1'
        params['chanlist'] = '52 56 60 64'
        hapd = hostapd.add_ap(apdev[0], params, wait_enabled=False)
        wait_acs(hapd, return_after_acs=True)

        wait_dfs_event(hapd, "DFS-CAC-START", 5)
        ev = wait_dfs_event(hapd, "DFS-CAC-COMPLETED", 70)
        if "success=1" not in ev:
            raise Exception("CAC failed")

        ev = hapd.wait_event(["AP-ENABLED"], timeout=5)
        if not ev:
            raise Exception("AP setup timed out")

        state = hapd.get_status_field("state")
        if state != "ENABLED":
            raise Exception("Unexpected interface state")

        freq = int(hapd.get_status_field("freq"))
        if freq not in [5260, 5280, 5300, 5320]:
            raise Exception("Unexpected frequency: %d" % freq)

        dev[0].connect("test-acs", psk="12345678", scan_freq=str(freq))
        dev[0].wait_regdom(country_ie=True)
    finally:
        if hapd:
            hapd.request("DISABLE")
        dev[0].disconnect_and_stop_scan()
        hostapd.cmd_execute(apdev[0], ['iw', 'reg', 'set', '00'])
        dev[0].wait_event(["CTRL-EVENT-REGDOM-CHANGE"], timeout=0.5)
        dev[0].flush_scan_cache()

def test_ap_acs_exclude_dfs(dev, apdev, params):
    """Automatic channel selection, exclude DFS"""
    try:
        hapd = None
        force_prev_ap_on_5g(apdev[0])
        params = hostapd.wpa2_params(ssid="test-acs", passphrase="12345678")
        params['hw_mode'] = 'a'
        params['channel'] = '0'
        params['ht_capab'] = '[HT40+]'
        params['country_code'] = 'US'
        params['ieee80211d'] = '1'
        params['ieee80211h'] = '1'
        params['acs_num_scans'] = '1'
        params['acs_exclude_dfs'] = '1'
        hapd = hostapd.add_ap(apdev[0], params, wait_enabled=False)
        wait_acs(hapd)

        state = hapd.get_status_field("state")
        if state != "ENABLED":
            raise Exception("Unexpected interface state")

        freq = int(hapd.get_status_field("freq"))
        if freq in [5260, 5280, 5300, 5320,
                    5500, 5520, 5540, 5560, 5580, 5600, 5620, 5640, 5660, 5680]:
            raise Exception("Unexpected frequency: %d" % freq)

        dev[0].connect("test-acs", psk="12345678", scan_freq=str(freq))
        dev[0].wait_regdom(country_ie=True)
    finally:
        if hapd:
            hapd.request("DISABLE")
        dev[0].disconnect_and_stop_scan()
        hostapd.cmd_execute(apdev[0], ['iw', 'reg', 'set', '00'])
        dev[0].wait_event(["CTRL-EVENT-REGDOM-CHANGE"], timeout=0.5)
        dev[0].flush_scan_cache()

@long_duration_test
def test_ap_acs_vht160_dfs(dev, apdev):
    """Automatic channel selection 160 MHz, HT scan, and DFS"""
    try:
        hapd = None
        force_prev_ap_on_5g(apdev[0])
        params = hostapd.wpa2_params(ssid="test-acs", passphrase="12345678")
        params['hw_mode'] = 'a'
        params['channel'] = '0'
        params['ht_capab'] = '[HT40+]'
        params['country_code'] = 'US'
        params['ieee80211ac'] = '1'
        params['vht_oper_chwidth'] = '2'
        params['ieee80211d'] = '1'
        params['ieee80211h'] = '1'
        params['acs_num_scans'] = '1'
        hapd = hostapd.add_ap(apdev[0], params, wait_enabled=False)
        wait_acs(hapd, return_after_acs=True)

        wait_dfs_event(hapd, "DFS-CAC-START", 5)
        ev = wait_dfs_event(hapd, "DFS-CAC-COMPLETED", 70)
        if "success=1" not in ev:
            raise Exception("CAC failed")

        ev = hapd.wait_event(["AP-ENABLED"], timeout=5)
        if not ev:
            raise Exception("AP setup timed out")

        state = hapd.get_status_field("state")
        if state != "ENABLED":
            raise Exception("Unexpected interface state")

        freq = int(hapd.get_status_field("freq"))
        if freq not in [5180, 5500]:
            raise Exception("Unexpected frequency: %d" % freq)

        dev[0].connect("test-acs", psk="12345678", scan_freq=str(freq))
        dev[0].wait_regdom(country_ie=True)
    finally:
        if hapd:
            hapd.request("DISABLE")
        dev[0].disconnect_and_stop_scan()
        hostapd.cmd_execute(apdev[0], ['iw', 'reg', 'set', '00'])
        dev[0].wait_event(["CTRL-EVENT-REGDOM-CHANGE"], timeout=0.5)
        dev[0].flush_scan_cache()

def test_ap_acs_hw_mode_any(dev, apdev):
    """Automatic channel selection with hw_mode=any"""
    force_prev_ap_on_24g(apdev[0])
    params = hostapd.wpa2_params(ssid="test-acs", passphrase="12345678")
    params['channel'] = '0'
    params['hw_mode'] = 'any'
    hapd = hostapd.add_ap(apdev[0], params, wait_enabled=False)
    wait_acs(hapd)

    freq = hapd.get_status_field("freq")
    if int(freq) < 2400:
        raise Exception("Unexpected frequency")

    dev[0].connect("test-acs", psk="12345678", scan_freq=freq)

def test_ap_acs_hw_mode_any_5ghz(dev, apdev):
    """Automatic channel selection with hw_mode=any and 5 GHz"""
    try:
        hapd = None
        force_prev_ap_on_5g(apdev[0])
        params = hostapd.wpa2_params(ssid="test-acs", passphrase="12345678")
        params['hw_mode'] = 'any'
        params['channel'] = '0'
        params['country_code'] = 'US'
        params['acs_chan_bias'] = '36:0.7 40:0.7 44:0.7 48:0.7'
        hapd = hostapd.add_ap(apdev[0], params, wait_enabled=False)
        wait_acs(hapd)
        freq = hapd.get_status_field("freq")
        if int(freq) < 5000:
            raise Exception("Unexpected frequency")

        dev[0].connect("test-acs", psk="12345678", scan_freq=freq)
        dev[0].wait_regdom(country_ie=True)
    finally:
        clear_regdom(hapd, dev)

def test_ap_acs_with_fallback_to_20(dev, apdev):
    """Automatic channel selection with fallback to 20 MHz"""
    force_prev_ap_on_24g(apdev[0])
    params = {"ssid": "legacy-20",
              "channel": "7", "ieee80211n": "0"}
    hostapd.add_ap(apdev[1], params)
    params = hostapd.wpa2_params(ssid="test-acs", passphrase="12345678")
    params['channel'] = '0'
    params['acs_chan_bias'] = '6:0.1'
    params['ht_capab'] = '[HT40+]'
    hapd = hostapd.add_ap(apdev[0], params, wait_enabled=False)
    wait_acs(hapd)

    freq = hapd.get_status_field("freq")
    if int(freq) < 2400:
        raise Exception("Unexpected frequency")

    dev[0].connect("test-acs", psk="12345678", scan_freq=freq)
    sig = dev[0].request("SIGNAL_POLL").splitlines()
    logger.info("SIGNAL_POLL: " + str(sig))
    if "WIDTH=20 MHz" not in sig:
        raise Exception("Station did not report 20 MHz bandwidth")

def test_ap_acs_rx_during(dev, apdev):
    """Automatic channel selection and RX during ACS"""
    force_prev_ap_on_24g(apdev[0])
    params = hostapd.wpa2_params(ssid="test-acs", passphrase="12345678")
    params['channel'] = '0'
    params['chanlist'] = '1 6 11'
    hapd = hostapd.add_ap(apdev[0], params, wait_enabled=False)

    time.sleep(0.1)
    hapd.set("ext_mgmt_frame_handling", "1")
    bssid = hapd.own_addr().replace(':', '')
    addr = "020304050607"
    broadcast = 6*"ff"

    probereq = "40000000" + broadcast + addr + broadcast + "1000"
    probereq += "0000" + "010802040b160c121824" + "32043048606c" + "030100"
    if "OK" not in hapd.request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=%s" % probereq):
        raise Exception("MGMT_RX_PROCESS failed")

    probereq = "40000000" + broadcast + addr + broadcast + "1000"
    probereq += "0000" + "010102"
    if "OK" not in hapd.request("MGMT_RX_PROCESS freq=2437 datarate=0 ssi_signal=-30 frame=%s" % probereq):
        raise Exception("MGMT_RX_PROCESS failed")

    auth = "b0003a01" + bssid + addr + bssid + '1000000001000000'
    if "OK" not in hapd.request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=%s" % auth):
        raise Exception("MGMT_RX_PROCESS failed")
    hapd.set("ext_mgmt_frame_handling", "0")

    time.sleep(0.2)
    try:
        for i in range(3):
            dev[i].request("SCAN_INTERVAL 1")
            dev[i].connect("test-acs", psk="12345678",
                           scan_freq="2412 2437 2462", wait_connect=False)
        wait_acs(hapd)
        for i in range(3):
            dev[i].wait_connected()
    finally:
        for i in range(3):
            dev[i].request("SCAN_INTERVAL 5")

def test_ap_acs_he_24g(dev, apdev):
    """Automatic channel selection on 2.4 GHz with HE"""
    clear_scan_cache(apdev[0])
    force_prev_ap_on_24g(apdev[0])

    params = hostapd.wpa2_params(ssid="test-acs", passphrase="12345678")
    params['channel'] = '0'
    params['ieee80211ax'] = '1'
    params['ht_capab'] = '[HT40+]'
    hapd = hostapd.add_ap(apdev[0], params, wait_enabled=False)
    wait_acs(hapd)

    freq = hapd.get_status_field("freq")
    if int(freq) < 2400:
        raise Exception("Unexpected frequency")

    dev[0].connect("test-acs", psk="12345678", scan_freq=freq)

def test_ap_acs_he_24g_overlap(dev, apdev):
    """Automatic channel selection on 2.4 GHz with HE (overlap)"""
    clear_scan_cache(apdev[0])
    force_prev_ap_on_24g(apdev[0])

    params = {"ssid": "overlapping",
              "channel": "6", "ieee80211n": "1"}
    hostapd.add_ap(apdev[1], params)

    params = hostapd.wpa2_params(ssid="test-acs", passphrase="12345678")
    params['channel'] = '0'
    params['ieee80211ax'] = '1'
    params['ht_capab'] = '[HT40+]'
    hapd = hostapd.add_ap(apdev[0], params, wait_enabled=False)
    wait_acs(hapd)

    freq = hapd.get_status_field("freq")
    if int(freq) < 2400:
        raise Exception("Unexpected frequency")

    dev[0].connect("test-acs", psk="12345678", scan_freq=freq)
