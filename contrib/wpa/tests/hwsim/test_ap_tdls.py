# TDLS tests
# Copyright (c) 2013-2014, Jouni Malinen <j@w1.fi>
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

from remotehost import remote_compatible
import time
import logging
logger = logging.getLogger()
import subprocess

import hwsim_utils
from hostapd import HostapdGlobal
from hostapd import Hostapd
import hostapd
from utils import *
from wlantest import Wlantest

def start_ap_wpa2_psk(ap):
    params = hostapd.wpa2_params(ssid="test-wpa2-psk", passphrase="12345678")
    return hostapd.add_ap(ap, params)

def connectivity(dev, hapd):
    hwsim_utils.test_connectivity_sta(dev[0], dev[1])
    hwsim_utils.test_connectivity(dev[0], hapd)
    hwsim_utils.test_connectivity(dev[1], hapd)

def connect_2sta(dev, ssid, hapd, sae=False):
    key_mgmt = "SAE" if sae else "WPA-PSK"
    ieee80211w = "2" if sae else "1"
    dev[0].connect(ssid, key_mgmt=key_mgmt, psk="12345678",
                   ieee80211w=ieee80211w, scan_freq="2412")
    dev[1].connect(ssid, key_mgmt=key_mgmt, psk="12345678",
                   ieee80211w=ieee80211w, scan_freq="2412")
    hapd.wait_sta()
    hapd.wait_sta()
    connectivity(dev, hapd)

def connect_2sta_wpa2_psk(dev, hapd):
    connect_2sta(dev, "test-wpa2-psk", hapd)

def connect_2sta_wpa_psk(dev, hapd):
    connect_2sta(dev, "test-wpa-psk", hapd)

def connect_2sta_wpa_psk_mixed(dev, hapd):
    dev[0].connect("test-wpa-mixed-psk", psk="12345678", proto="WPA",
                   scan_freq="2412")
    dev[1].connect("test-wpa-mixed-psk", psk="12345678", proto="WPA2",
                   scan_freq="2412")
    hapd.wait_sta()
    hapd.wait_sta()
    connectivity(dev, hapd)

def connect_2sta_wep(dev, hapd):
    dev[0].connect("test-wep", key_mgmt="NONE", wep_key0='"hello"',
                   scan_freq="2412")
    dev[1].connect("test-wep", key_mgmt="NONE", wep_key0='"hello"',
                   scan_freq="2412")
    hapd.wait_sta()
    hapd.wait_sta()
    connectivity(dev, hapd)

def connect_2sta_open(dev, hapd, scan_freq="2412"):
    dev[0].connect("test-open", key_mgmt="NONE", scan_freq=scan_freq)
    dev[1].connect("test-open", key_mgmt="NONE", scan_freq=scan_freq)
    hapd.wait_sta()
    hapd.wait_sta()
    connectivity(dev, hapd)

def wlantest_setup(hapd):
    Wlantest.setup(hapd)
    wt = Wlantest()
    wt.flush()
    wt.add_passphrase("12345678")
    wt.add_wepkey("68656c6c6f")

def wlantest_tdls_packet_counters(bssid, addr0, addr1):
    wt = Wlantest()
    dl = wt.get_tdls_counter("valid_direct_link", bssid, addr0, addr1)
    inv_dl = wt.get_tdls_counter("invalid_direct_link", bssid, addr0, addr1)
    ap = wt.get_tdls_counter("valid_ap_path", bssid, addr0, addr1)
    inv_ap = wt.get_tdls_counter("invalid_ap_path", bssid, addr0, addr1)
    return [dl, inv_dl, ap, inv_ap]

def tdls_check_dl(sta0, sta1, bssid, addr0, addr1):
    wt = Wlantest()
    wt.tdls_clear(bssid, addr0, addr1)
    hwsim_utils.test_connectivity_sta(sta0, sta1)
    [dl, inv_dl, ap, inv_ap] = wlantest_tdls_packet_counters(bssid, addr0, addr1)
    if dl == 0:
        raise Exception("No valid frames through direct link")
    if inv_dl > 0:
        raise Exception("Invalid frames through direct link")
    if ap > 0:
        raise Exception("Unexpected frames through AP path")
    if inv_ap > 0:
        raise Exception("Invalid frames through AP path")

def tdls_check_ap(sta0, sta1, bssid, addr0, addr1):
    wt = Wlantest()
    wt.tdls_clear(bssid, addr0, addr1)
    hwsim_utils.test_connectivity_sta(sta0, sta1)
    [dl, inv_dl, ap, inv_ap] = wlantest_tdls_packet_counters(bssid, addr0, addr1)
    if dl > 0:
        raise Exception("Unexpected frames through direct link")
    if inv_dl > 0:
        raise Exception("Invalid frames through direct link")
    if ap == 0:
        raise Exception("No valid frames through AP path")
    if inv_ap > 0:
        raise Exception("Invalid frames through AP path")

def check_connectivity(sta0, sta1, hapd):
    hwsim_utils.test_connectivity_sta(sta0, sta1)
    hwsim_utils.test_connectivity(sta0, hapd)
    hwsim_utils.test_connectivity(sta1, hapd)

def setup_tdls(sta0, sta1, hapd, reverse=False, expect_fail=False, sae=False):
    logger.info("Setup TDLS")
    check_connectivity(sta0, sta1, hapd)
    bssid = hapd.own_addr()
    addr0 = sta0.p2p_interface_addr()
    addr1 = sta1.p2p_interface_addr()
    wt = Wlantest()
    wt.tdls_clear(bssid, addr0, addr1)
    wt.tdls_clear(bssid, addr1, addr0)
    sta0.tdls_setup(addr1)
    time.sleep(1)
    if expect_fail:
        if not sae:
            tdls_check_ap(sta0, sta1, bssid, addr0, addr1)
        return
    if reverse:
        addr1 = sta0.p2p_interface_addr()
        addr0 = sta1.p2p_interface_addr()
    if not sae:
        conf = wt.get_tdls_counter("setup_conf_ok", bssid, addr0, addr1)
        if conf == 0:
            raise Exception("No TDLS Setup Confirm (success) seen")
        tdls_check_dl(sta0, sta1, bssid, addr0, addr1)
    check_connectivity(sta0, sta1, hapd)

def teardown_tdls(sta0, sta1, hapd, responder=False, wildcard=False, sae=False):
    logger.info("Teardown TDLS")
    check_connectivity(sta0, sta1, hapd)
    bssid = hapd.own_addr()
    addr0 = sta0.p2p_interface_addr()
    addr1 = sta1.p2p_interface_addr()
    if responder:
        sta1.tdls_teardown(addr0)
    elif wildcard:
        sta0.tdls_teardown("*")
    else:
        sta0.tdls_teardown(addr1)
    time.sleep(1)
    if not sae:
        wt = Wlantest()
        teardown = wt.get_tdls_counter("teardown", bssid, addr0, addr1)
        if teardown == 0:
            raise Exception("No TDLS Setup Teardown seen")
        tdls_check_ap(sta0, sta1, bssid, addr0, addr1)
    check_connectivity(sta0, sta1, hapd)

def check_tdls_link(sta0, sta1, connected=True):
    addr0 = sta0.own_addr()
    addr1 = sta1.own_addr()
    status0 = sta0.tdls_link_status(addr1).rstrip()
    status1 = sta1.tdls_link_status(addr0).rstrip()
    logger.info("%s: %s" % (sta0.ifname, status0))
    logger.info("%s: %s" % (sta1.ifname, status1))
    if status0 != status1:
        raise Exception("TDLS link status differs between stations")
    if "status: connected" in status0:
        if not connected:
            raise Exception("Expected TDLS link status NOT to be connected")
    else:
        if connected:
            raise Exception("Expected TDLS link status to be connected")

@remote_compatible
def test_ap_tdls_discovery(dev, apdev):
    """WPA2-PSK AP and two stations using TDLS discovery"""
    hapd = start_ap_wpa2_psk(apdev[0])
    wlantest_setup(hapd)
    connect_2sta_wpa2_psk(dev, hapd)
    dev[0].request("TDLS_DISCOVER " + dev[1].p2p_interface_addr())
    time.sleep(0.2)

def test_ap_wpa2_tdls(dev, apdev):
    """WPA2-PSK AP and two stations using TDLS"""
    hapd = start_ap_wpa2_psk(apdev[0])
    wlantest_setup(hapd)
    connect_2sta_wpa2_psk(dev, hapd)
    setup_tdls(dev[0], dev[1], hapd)
    teardown_tdls(dev[0], dev[1], hapd)
    setup_tdls(dev[1], dev[0], hapd)
    #teardown_tdls(dev[0], dev[1], hapd)

def test_ap_wpa2_tdls_concurrent_init(dev, apdev):
    """Concurrent TDLS setup initiation"""
    hapd = start_ap_wpa2_psk(apdev[0])
    wlantest_setup(hapd)
    connect_2sta_wpa2_psk(dev, hapd)
    dev[0].request("SET tdls_testing 0x80")
    setup_tdls(dev[1], dev[0], hapd, reverse=True)

def test_ap_wpa2_tdls_concurrent_init2(dev, apdev):
    """Concurrent TDLS setup initiation (reverse)"""
    hapd = start_ap_wpa2_psk(apdev[0])
    wlantest_setup(hapd)
    connect_2sta_wpa2_psk(dev, hapd)
    dev[1].request("SET tdls_testing 0x80")
    setup_tdls(dev[0], dev[1], hapd)

def test_ap_wpa2_tdls_decline_resp(dev, apdev):
    """Decline TDLS Setup Response"""
    hapd = start_ap_wpa2_psk(apdev[0])
    wlantest_setup(hapd)
    connect_2sta_wpa2_psk(dev, hapd)
    dev[1].request("SET tdls_testing 0x200")
    setup_tdls(dev[1], dev[0], hapd, expect_fail=True)

def test_ap_wpa2_tdls_long_lifetime(dev, apdev):
    """TDLS with long TPK lifetime"""
    hapd = start_ap_wpa2_psk(apdev[0])
    wlantest_setup(hapd)
    connect_2sta_wpa2_psk(dev, hapd)
    dev[1].request("SET tdls_testing 0x40")
    setup_tdls(dev[1], dev[0], hapd)

def test_ap_wpa2_tdls_long_frame(dev, apdev):
    """TDLS with long setup/teardown frames"""
    hapd = start_ap_wpa2_psk(apdev[0])
    wlantest_setup(hapd)
    connect_2sta_wpa2_psk(dev, hapd)
    dev[0].request("SET tdls_testing 0x1")
    dev[1].request("SET tdls_testing 0x1")
    setup_tdls(dev[1], dev[0], hapd)
    teardown_tdls(dev[1], dev[0], hapd)
    setup_tdls(dev[0], dev[1], hapd)

def test_ap_wpa2_tdls_reneg(dev, apdev):
    """Renegotiate TDLS link"""
    hapd = start_ap_wpa2_psk(apdev[0])
    wlantest_setup(hapd)
    connect_2sta_wpa2_psk(dev, hapd)
    setup_tdls(dev[1], dev[0], hapd)
    setup_tdls(dev[0], dev[1], hapd)

def test_ap_wpa2_tdls_wrong_lifetime_resp(dev, apdev):
    """Incorrect TPK lifetime in TDLS Setup Response"""
    hapd = start_ap_wpa2_psk(apdev[0])
    wlantest_setup(hapd)
    connect_2sta_wpa2_psk(dev, hapd)
    dev[1].request("SET tdls_testing 0x10")
    setup_tdls(dev[0], dev[1], hapd, expect_fail=True)

def test_ap_wpa2_tdls_diff_rsnie(dev, apdev):
    """TDLS with different RSN IEs"""
    hapd = start_ap_wpa2_psk(apdev[0])
    wlantest_setup(hapd)
    connect_2sta_wpa2_psk(dev, hapd)
    dev[1].request("SET tdls_testing 0x2")
    setup_tdls(dev[1], dev[0], hapd)
    teardown_tdls(dev[1], dev[0], hapd)

def test_ap_wpa2_tdls_wrong_tpk_m2_mic(dev, apdev):
    """Incorrect MIC in TDLS Setup Response"""
    hapd = start_ap_wpa2_psk(apdev[0])
    wlantest_setup(hapd)
    connect_2sta_wpa2_psk(dev, hapd)
    dev[0].request("SET tdls_testing 0x800")
    addr0 = dev[0].p2p_interface_addr()
    dev[1].tdls_setup(addr0)
    time.sleep(1)

def test_ap_wpa2_tdls_wrong_tpk_m3_mic(dev, apdev):
    """Incorrect MIC in TDLS Setup Confirm"""
    hapd = start_ap_wpa2_psk(apdev[0])
    wlantest_setup(hapd)
    connect_2sta_wpa2_psk(dev, hapd)
    dev[1].request("SET tdls_testing 0x800")
    addr0 = dev[0].p2p_interface_addr()
    dev[1].tdls_setup(addr0)
    time.sleep(1)

def test_ap_wpa2_tdls_double_tpk_m2(dev, apdev):
    """Double TPK M2 during TDLS setup initiation"""
    hapd = start_ap_wpa2_psk(apdev[0])
    wlantest_setup(hapd)
    connect_2sta_wpa2_psk(dev, hapd)
    dev[0].request("SET tdls_testing 0x1000")
    setup_tdls(dev[1], dev[0], hapd)

def test_ap_wpa_tdls(dev, apdev):
    """WPA-PSK AP and two stations using TDLS"""
    skip_with_fips(dev[0])
    skip_without_tkip(dev[0])
    skip_without_tkip(dev[1])
    hapd = hostapd.add_ap(apdev[0],
                          hostapd.wpa_params(ssid="test-wpa-psk",
                                             passphrase="12345678"))
    wlantest_setup(hapd)
    connect_2sta_wpa_psk(dev, hapd)
    setup_tdls(dev[0], dev[1], hapd)
    teardown_tdls(dev[0], dev[1], hapd)
    setup_tdls(dev[1], dev[0], hapd)

def test_ap_wpa_mixed_tdls(dev, apdev):
    """WPA+WPA2-PSK AP and two stations using TDLS"""
    skip_with_fips(dev[0])
    skip_without_tkip(dev[0])
    hapd = hostapd.add_ap(apdev[0],
                          hostapd.wpa_mixed_params(ssid="test-wpa-mixed-psk",
                                                   passphrase="12345678"))
    wlantest_setup(hapd)
    connect_2sta_wpa_psk_mixed(dev, hapd)
    setup_tdls(dev[0], dev[1], hapd)
    teardown_tdls(dev[0], dev[1], hapd)
    setup_tdls(dev[1], dev[0], hapd)

def test_ap_wep_tdls(dev, apdev):
    """WEP AP and two stations using TDLS"""
    check_wep_capa(dev[0])
    check_wep_capa(dev[1])
    hapd = hostapd.add_ap(apdev[0],
                          {"ssid": "test-wep", "wep_key0": '"hello"'})
    wlantest_setup(hapd)
    connect_2sta_wep(dev, hapd)
    setup_tdls(dev[0], dev[1], hapd)
    teardown_tdls(dev[0], dev[1], hapd)
    setup_tdls(dev[1], dev[0], hapd)

def test_ap_open_tdls(dev, apdev):
    """Open AP and two stations using TDLS"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": "test-open"})
    wlantest_setup(hapd)
    connect_2sta_open(dev, hapd)
    setup_tdls(dev[0], dev[1], hapd)
    teardown_tdls(dev[0], dev[1], hapd)
    setup_tdls(dev[1], dev[0], hapd)
    teardown_tdls(dev[1], dev[0], hapd, wildcard=True)

def test_ap_wpa2_tdls_bssid_mismatch(dev, apdev):
    """TDLS failure due to BSSID mismatch"""
    try:
        ssid = "test-wpa2-psk"
        passphrase = "12345678"
        params = hostapd.wpa2_params(ssid=ssid, passphrase=passphrase)
        params['bridge'] = 'ap-br0'
        hapd = hostapd.add_ap(apdev[0], params)
        hostapd.add_ap(apdev[1], params)
        wlantest_setup(hapd)
        subprocess.call(['brctl', 'setfd', 'ap-br0', '0'])
        subprocess.call(['ip', 'link', 'set', 'dev', 'ap-br0', 'up'])
        dev[0].connect(ssid, psk=passphrase, scan_freq="2412",
                       bssid=apdev[0]['bssid'])
        dev[1].connect(ssid, psk=passphrase, scan_freq="2412",
                       bssid=apdev[1]['bssid'])
        hwsim_utils.test_connectivity_sta(dev[0], dev[1])
        hwsim_utils.test_connectivity_iface(dev[0], hapd, "ap-br0")
        hwsim_utils.test_connectivity_iface(dev[1], hapd, "ap-br0")

        addr0 = dev[0].p2p_interface_addr()
        dev[1].tdls_setup(addr0)
        time.sleep(1)
        hwsim_utils.test_connectivity_sta(dev[0], dev[1])
    finally:
        subprocess.call(['ip', 'link', 'set', 'dev', 'ap-br0', 'down'])
        subprocess.call(['brctl', 'delbr', 'ap-br0'])

def test_ap_wpa2_tdls_responder_teardown(dev, apdev):
    """TDLS teardown from responder with WPA2-PSK AP"""
    hapd = start_ap_wpa2_psk(apdev[0])
    wlantest_setup(hapd)
    connect_2sta_wpa2_psk(dev, hapd)
    setup_tdls(dev[0], dev[1], hapd)
    teardown_tdls(dev[0], dev[1], hapd, responder=True)

def tdls_clear_reg(hapd, dev):
    if hapd:
        hapd.request("DISABLE")
    dev[1].request("DISCONNECT")
    dev[0].disconnect_and_stop_scan()
    dev[1].disconnect_and_stop_scan()
    subprocess.call(['iw', 'reg', 'set', '00'])
    dev[0].wait_event(["CTRL-EVENT-REGDOM-CHANGE"], timeout=0.5)
    dev[0].flush_scan_cache()
    dev[1].flush_scan_cache()

def test_ap_open_tdls_vht(dev, apdev):
    """Open AP and two stations using TDLS"""
    params = {"ssid": "test-open",
              "country_code": "DE",
              "hw_mode": "a",
              "channel": "36",
              "ieee80211n": "1",
              "ieee80211ac": "1",
              "ht_capab": "",
              "vht_capab": "",
              "vht_oper_chwidth": "0",
              "vht_oper_centr_freq_seg0_idx": "0"}
    hapd = None
    try:
        hapd = hostapd.add_ap(apdev[0], params)
        wlantest_setup(hapd)
        connect_2sta_open(dev, hapd, scan_freq="5180")
        setup_tdls(dev[0], dev[1], hapd)
        teardown_tdls(dev[0], dev[1], hapd)
        setup_tdls(dev[1], dev[0], hapd)
        teardown_tdls(dev[1], dev[0], hapd, wildcard=True)
    finally:
        tdls_clear_reg(hapd, dev)

def test_ap_open_tdls_vht80(dev, apdev):
    """Open AP and two stations using TDLS with VHT 80"""
    params = {"ssid": "test-open",
              "country_code": "US",
              "hw_mode": "a",
              "channel": "36",
              "ht_capab": "[HT40+]",
              "vht_capab": "[VHT160]",
              "ieee80211n": "1",
              "ieee80211ac": "1",
              "vht_capab": "",
              "vht_oper_chwidth": "1",
              "vht_oper_centr_freq_seg0_idx": "42"}
    try:
        hapd = None
        hapd = hostapd.add_ap(apdev[0], params)
        wlantest_setup(hapd)
        connect_2sta_open(dev, hapd, scan_freq="5180")
        sig = dev[0].request("SIGNAL_POLL").splitlines()
        if "WIDTH=80 MHz" not in sig:
            raise Exception("Unexpected SIGNAL_POLL value(2): " + str(sig))
        setup_tdls(dev[0], dev[1], hapd)
        for i in range(10):
            check_connectivity(dev[0], dev[1], hapd)
        for i in range(2):
            cmd = subprocess.Popen(['iw', dev[0].ifname, 'station', 'dump'],
                                   stdout=subprocess.PIPE)
            res = cmd.stdout.read()
            cmd.stdout.close()
            logger.info("Station dump on dev[%d]:\n%s" % (i, res.decode()))
    except Exception as e:
        if isinstance(e, Exception) and str(e) == "AP startup failed":
            if not vht_supported():
                raise HwsimSkip("80/160 MHz channel not supported in regulatory information")
        raise
    finally:
        tdls_clear_reg(hapd, dev)

def test_ap_open_tdls_vht80plus80(dev, apdev):
    """Open AP and two stations using TDLS with VHT 80+80"""
    params = {"ssid": "test-open",
              "country_code": "US",
              "hw_mode": "a",
              "channel": "36",
              "ht_capab": "[HT40+]",
              "ieee80211n": "1",
              "ieee80211ac": "1",
              "vht_capab": "[VHT160-80PLUS80]",
              "vht_oper_chwidth": "3",
              "vht_oper_centr_freq_seg0_idx": "42",
              "vht_oper_centr_freq_seg1_idx": "155"}
    try:
        hapd = None
        hapd = hostapd.add_ap(apdev[0], params)
        wlantest_setup(hapd)
        connect_2sta_open(dev, hapd, scan_freq="5180")
        sig = dev[0].request("SIGNAL_POLL").splitlines()
        if "FREQUENCY=5180" not in sig:
            raise Exception("Unexpected SIGNAL_POLL value(1): " + str(sig))
        if "WIDTH=80+80 MHz" not in sig:
            raise Exception("Unexpected SIGNAL_POLL value(2): " + str(sig))
        if "CENTER_FRQ1=5210" not in sig:
            raise Exception("Unexpected SIGNAL_POLL value(3): " + str(sig))
        if "CENTER_FRQ2=5775" not in sig:
            raise Exception("Unexpected SIGNAL_POLL value(4): " + str(sig))
        setup_tdls(dev[0], dev[1], hapd)
        for i in range(10):
            check_connectivity(dev[0], dev[1], hapd)
        for i in range(2):
            cmd = subprocess.Popen(['iw', dev[0].ifname, 'station', 'dump'],
                                   stdout=subprocess.PIPE)
            res = cmd.stdout.read()
            cmd.stdout.close()
            logger.info("Station dump on dev[%d]:\n%s" % (i, res.decode()))
    except Exception as e:
        if isinstance(e, Exception) and str(e) == "AP startup failed":
            if not vht_supported():
                raise HwsimSkip("80/160 MHz channel not supported in regulatory information")
        raise
    finally:
        tdls_clear_reg(hapd, dev)

def test_ap_open_tdls_vht160(dev, apdev):
    """Open AP and two stations using TDLS with VHT 160"""
    params = {"ssid": "test-open",
              "country_code": "ZA",
              "hw_mode": "a",
              "channel": "104",
              "ht_capab": "[HT40-]",
              "vht_capab": "[VHT160]",
              "ieee80211n": "1",
              "ieee80211ac": "1",
              "vht_oper_chwidth": "2",
              "vht_oper_centr_freq_seg0_idx": "114"}
    try:
        hapd = None
        hapd = hostapd.add_ap(apdev[0], params, wait_enabled=False)
        ev = hapd.wait_event(["AP-ENABLED"], timeout=2)
        if not ev:
            cmd = subprocess.Popen(["iw", "reg", "get"], stdout=subprocess.PIPE)
            reg = cmd.stdout.readlines()
            for r in reg:
                if "5490" in r and "DFS" in r:
                    raise HwsimSkip("ZA regulatory rule did not have DFS requirement removed")
            raise Exception("AP setup timed out")
        wlantest_setup(hapd)
        connect_2sta_open(dev, hapd, scan_freq="5520")
        sig = dev[0].request("SIGNAL_POLL").splitlines()
        if "WIDTH=160 MHz" not in sig:
            raise Exception("Unexpected SIGNAL_POLL value(2): " + str(sig))
        setup_tdls(dev[0], dev[1], hapd)
        for i in range(10):
            check_connectivity(dev[0], dev[1], hapd)
        for i in range(2):
            cmd = subprocess.Popen(['iw', dev[0].ifname, 'station', 'dump'],
                                   stdout=subprocess.PIPE)
            res = cmd.stdout.read()
            cmd.stdout.close()
            logger.info("Station dump on dev[%d]:\n%s" % (i, res.decode()))
    except Exception as e:
        if isinstance(e, Exception) and str(e) == "AP startup failed":
            if not vht_supported():
                raise HwsimSkip("80/160 MHz channel not supported in regulatory information")
        raise
    finally:
        tdls_clear_reg(hapd, dev)

def test_tdls_chan_switch(dev, apdev):
    """Open AP and two stations using TDLS"""
    flags = int(dev[0].get_driver_status_field('capa.flags'), 16)
    if flags & 0x800000000 == 0:
        raise HwsimSkip("Driver does not support TDLS channel switching")

    hapd = hostapd.add_ap(apdev[0], {"ssid": "test-open"})
    wlantest_setup(hapd)
    connect_2sta_open(dev, hapd)
    setup_tdls(dev[0], dev[1], hapd)
    if "OK" not in dev[0].request("TDLS_CHAN_SWITCH " + dev[1].own_addr() + " 81 2462"):
        raise Exception("Failed to enable TDLS channel switching")
    if "OK" not in dev[0].request("TDLS_CANCEL_CHAN_SWITCH " + dev[1].own_addr()):
        raise Exception("Could not disable TDLS channel switching")
    if "FAIL" not in dev[0].request("TDLS_CANCEL_CHAN_SWITCH " + dev[1].own_addr()):
        raise Exception("TDLS_CANCEL_CHAN_SWITCH accepted even though channel switching was already disabled")
    if "FAIL" not in dev[0].request("TDLS_CHAN_SWITCH foo 81 2462"):
        raise Exception("Invalid TDLS channel switching command accepted")

def test_ap_tdls_link_status(dev, apdev):
    """Check TDLS link status between two stations"""
    hapd = start_ap_wpa2_psk(apdev[0])
    wlantest_setup(hapd)
    connect_2sta_wpa2_psk(dev, hapd)
    check_tdls_link(dev[0], dev[1], connected=False)
    setup_tdls(dev[0], dev[1], hapd)
    check_tdls_link(dev[0], dev[1], connected=True)
    teardown_tdls(dev[0], dev[1], hapd)
    check_tdls_link(dev[0], dev[1], connected=False)
    if "FAIL" not in dev[0].request("TDLS_LINK_STATUS foo"):
        raise Exception("Unexpected TDLS_LINK_STATUS response for invalid argument")

def test_ap_tdls_prohibit(dev, apdev):
    """Open AP and TDLS prohibited"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": "test-open",
                                     "tdls_prohibit": "1"})
    connect_2sta_open(dev, hapd)
    if "FAIL" not in dev[0].request("TDLS_SETUP " + dev[1].own_addr()):
        raise Exception("TDLS_SETUP accepted unexpectedly")

def test_ap_tdls_chan_switch_prohibit(dev, apdev):
    """Open AP and TDLS channel switch prohibited"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": "test-open",
                                     "tdls_prohibit_chan_switch": "1"})
    wlantest_setup(hapd)
    connect_2sta_open(dev, hapd)
    setup_tdls(dev[0], dev[1], hapd)

def test_ap_open_tdls_external_control(dev, apdev):
    """TDLS and tdls_external_control"""
    try:
        _test_ap_open_tdls_external_control(dev, apdev)
    finally:
        dev[0].set("tdls_external_control", "0")

def _test_ap_open_tdls_external_control(dev, apdev):
    hapd = hostapd.add_ap(apdev[0], {"ssid": "test-open"})
    dev[0].connect("test-open", key_mgmt="NONE", scan_freq="2412")
    dev[1].connect("test-open", key_mgmt="NONE", scan_freq="2412")
    addr0 = dev[0].own_addr()
    addr1 = dev[1].own_addr()

    dev[0].set("tdls_external_control", "1")
    if "FAIL" in dev[0].request("TDLS_SETUP " + addr1):
        # tdls_external_control not supported; try without it
        dev[0].set("tdls_external_control", "0")
        if "FAIL" in dev[0].request("TDLS_SETUP " + addr1):
            raise Exception("TDLS_SETUP failed")
    connected = False
    for i in range(50):
        res0 = dev[0].request("TDLS_LINK_STATUS " + addr1)
        res1 = dev[1].request("TDLS_LINK_STATUS " + addr0)
        if "TDLS link status: connected" in res0 and "TDLS link status: connected" in res1:
            connected = True
            break
        time.sleep(0.1)
    if not connected:
        raise Exception("TDLS setup did not complete")

    dev[0].set("tdls_external_control", "1")
    if "FAIL" in dev[0].request("TDLS_TEARDOWN " + addr1):
        # tdls_external_control not supported; try without it
        dev[0].set("tdls_external_control", "0")
        if "FAIL" in dev[0].request("TDLS_TEARDOWN " + addr1):
            raise Exception("TDLS_TEARDOWN failed")
    for i in range(50):
        res0 = dev[0].request("TDLS_LINK_STATUS " + addr1)
        res1 = dev[1].request("TDLS_LINK_STATUS " + addr0)
        if "TDLS link status: connected" not in res0 and "TDLS link status: connected" not in res1:
            connected = False
            break
        time.sleep(0.1)
    if connected:
        raise Exception("TDLS teardown did not complete")

def test_ap_sae_tdls(dev, apdev):
    """SAE AP and two stations using TDLS"""
    check_sae_capab(dev[0])
    check_sae_capab(dev[1])
    dev[0].request("SET sae_groups ")
    dev[1].request("SET sae_groups ")
    params = hostapd.wpa2_params(ssid="test-wpa2-psk", passphrase="12345678")
    params['wpa_key_mgmt'] = 'SAE'
    params["ieee80211w"] = "2"
    hapd = hostapd.add_ap(apdev[0], params)
    wlantest_setup(hapd)
    connect_2sta(dev, "test-wpa2-psk", hapd, sae=True)
    setup_tdls(dev[0], dev[1], hapd, sae=True)
    teardown_tdls(dev[0], dev[1], hapd, sae=True)
    setup_tdls(dev[1], dev[0], hapd, sae=True)
