# Test cases for dynamic BSS changes with hostapd
# Copyright (c) 2013, Qualcomm Atheros, Inc.
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

from remotehost import remote_compatible
import time
import subprocess
import logging
logger = logging.getLogger()
import os

import hwsim_utils
import hostapd
from utils import *
from test_ap_acs import force_prev_ap_on_24g

@remote_compatible
def test_ap_change_ssid(dev, apdev):
    """Dynamic SSID change with hostapd and WPA2-PSK"""
    params = hostapd.wpa2_params(ssid="test-wpa2-psk-start",
                                 passphrase="12345678")
    hapd = hostapd.add_ap(apdev[0], params)
    id = dev[0].connect("test-wpa2-psk-start", psk="12345678",
                        scan_freq="2412")
    dev[0].request("DISCONNECT")

    logger.info("Change SSID dynamically")
    res = hapd.request("SET ssid test-wpa2-psk-new")
    if "OK" not in res:
        raise Exception("SET command failed")
    res = hapd.request("RELOAD")
    if "OK" not in res:
        raise Exception("RELOAD command failed")

    dev[0].set_network_quoted(id, "ssid", "test-wpa2-psk-new")
    dev[0].connect_network(id)

def test_ap_change_ssid_wps(dev, apdev):
    """Dynamic SSID change with hostapd and WPA2-PSK using WPS"""
    params = hostapd.wpa2_params(ssid="test-wpa2-psk-start",
                                 passphrase="12345678")
    # Use a PSK and not the passphrase, because the PSK will have to be computed
    # again if we use a passphrase.
    del params["wpa_passphrase"]
    params["wpa_psk"] = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"

    params.update({"wps_state": "2", "eap_server": "1"})
    bssid = apdev[0]['bssid']
    hapd = hostapd.add_ap(apdev[0], params)

    new_ssid = "test-wpa2-psk-new"
    logger.info("Change SSID dynamically (WPS)")
    res = hapd.request("SET ssid " + new_ssid)
    if "OK" not in res:
        raise Exception("SET command failed")
    res = hapd.request("RELOAD")
    if "OK" not in res:
        raise Exception("RELOAD command failed")

    # Connect to the new ssid using wps:
    hapd.request("WPS_PBC")
    if "PBC Status: Active" not in hapd.request("WPS_GET_STATUS"):
        raise Exception("PBC status not shown correctly")

    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412", force_scan=True)
    dev[0].request("WPS_PBC")
    dev[0].wait_connected(timeout=20)
    status = dev[0].get_status()
    if status['wpa_state'] != 'COMPLETED' or status['bssid'] != bssid:
        raise Exception("Not fully connected")
    if status['ssid'] != new_ssid:
        raise Exception("Unexpected SSID %s != %s" % (status['ssid'], new_ssid))
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

def test_ap_reload_invalid(dev, apdev):
    """hostapd RELOAD with invalid configuration"""
    params = hostapd.wpa2_params(ssid="test-wpa2-psk-start",
                                 passphrase="12345678")
    hapd = hostapd.add_ap(apdev[0], params)
    # Enable IEEE 802.11d without specifying country code
    hapd.set("ieee80211d", "1")
    if "FAIL" not in hapd.request("RELOAD"):
        raise Exception("RELOAD command succeeded")
    dev[0].connect("test-wpa2-psk-start", psk="12345678", scan_freq="2412")

def multi_check(apdev, dev, check, scan_opt=True):
    id = []
    num_bss = len(check)
    for i in range(0, num_bss):
        dev[i].request("BSS_FLUSH 0")
        dev[i].dump_monitor()
    for i in range(0, num_bss):
        if check[i]:
            continue
        id.append(dev[i].connect("bss-" + str(i + 1), key_mgmt="NONE",
                                 scan_freq="2412", wait_connect=False))
    for i in range(num_bss):
        if not check[i]:
            continue
        bssid = hostapd.bssid_inc(apdev, i)
        if scan_opt:
            dev[i].scan_for_bss(bssid, freq=2412)
        id.append(dev[i].connect("bss-" + str(i + 1), key_mgmt="NONE",
                                 scan_freq="2412", wait_connect=True))
    first = True
    for i in range(num_bss):
        if not check[i]:
            timeout = 0.2 if first else 0.01
            first = False
            ev = dev[i].wait_event(["CTRL-EVENT-CONNECTED"], timeout=timeout)
            if ev:
                raise Exception("Unexpected connection")

    for i in range(0, num_bss):
        dev[i].remove_network(id[i])
    for i in range(num_bss):
        if check[i]:
            dev[i].wait_disconnected(timeout=5)

    res = ''
    for i in range(0, num_bss):
        res = res + dev[i].request("BSS RANGE=ALL MASK=0x2")

    for i in range(0, num_bss):
        if not check[i]:
            bssid = '02:00:00:00:03:0' + str(i)
            if bssid in res:
                raise Exception("Unexpected BSS" + str(i) + " in scan results")

def test_ap_bss_add_remove(dev, apdev):
    """Dynamic BSS add/remove operations with hostapd"""
    try:
        _test_ap_bss_add_remove(dev, apdev)
    finally:
        for i in range(3):
            dev[i].request("SCAN_INTERVAL 5")

def _test_ap_bss_add_remove(dev, apdev):
    for i in range(3):
        dev[i].flush_scan_cache()
        dev[i].request("SCAN_INTERVAL 1")
    ifname1 = apdev[0]['ifname']
    ifname2 = apdev[0]['ifname'] + '-2'
    ifname3 = apdev[0]['ifname'] + '-3'
    logger.info("Set up three BSSes one by one")
    hostapd.add_bss(apdev[0], ifname1, 'bss-1.conf')
    multi_check(apdev[0], dev, [True, False, False])
    hostapd.add_bss(apdev[0], ifname2, 'bss-2.conf')
    multi_check(apdev[0], dev, [True, True, False])
    hostapd.add_bss(apdev[0], ifname3, 'bss-3.conf')
    multi_check(apdev[0], dev, [True, True, True])

    logger.info("Remove the last BSS and re-add it")
    hostapd.remove_bss(apdev[0], ifname3)
    multi_check(apdev[0], dev, [True, True, False])
    hostapd.add_bss(apdev[0], ifname3, 'bss-3.conf')
    multi_check(apdev[0], dev, [True, True, True])

    logger.info("Remove the middle BSS and re-add it")
    hostapd.remove_bss(apdev[0], ifname2)
    multi_check(apdev[0], dev, [True, False, True])
    hostapd.add_bss(apdev[0], ifname2, 'bss-2.conf')
    multi_check(apdev[0], dev, [True, True, True])

    logger.info("Remove the first BSS and re-add it and other BSSs")
    hostapd.remove_bss(apdev[0], ifname1)
    multi_check(apdev[0], dev, [False, False, False])
    hostapd.add_bss(apdev[0], ifname1, 'bss-1.conf')
    hostapd.add_bss(apdev[0], ifname2, 'bss-2.conf')
    hostapd.add_bss(apdev[0], ifname3, 'bss-3.conf')
    multi_check(apdev[0], dev, [True, True, True])

    logger.info("Remove two BSSes and re-add them")
    hostapd.remove_bss(apdev[0], ifname2)
    multi_check(apdev[0], dev, [True, False, True])
    hostapd.remove_bss(apdev[0], ifname3)
    multi_check(apdev[0], dev, [True, False, False])
    hostapd.add_bss(apdev[0], ifname2, 'bss-2.conf')
    multi_check(apdev[0], dev, [True, True, False])
    hostapd.add_bss(apdev[0], ifname3, 'bss-3.conf')
    multi_check(apdev[0], dev, [True, True, True])

    logger.info("Remove three BSSes in and re-add them")
    hostapd.remove_bss(apdev[0], ifname3)
    multi_check(apdev[0], dev, [True, True, False])
    hostapd.remove_bss(apdev[0], ifname2)
    multi_check(apdev[0], dev, [True, False, False])
    hostapd.remove_bss(apdev[0], ifname1)
    multi_check(apdev[0], dev, [False, False, False])
    hostapd.add_bss(apdev[0], ifname1, 'bss-1.conf')
    multi_check(apdev[0], dev, [True, False, False])
    hostapd.add_bss(apdev[0], ifname2, 'bss-2.conf')
    multi_check(apdev[0], dev, [True, True, False])
    hostapd.add_bss(apdev[0], ifname3, 'bss-3.conf')
    multi_check(apdev[0], dev, [True, True, True])

    logger.info("Test error handling if a duplicate ifname is tried")
    hostapd.add_bss(apdev[0], ifname3, 'bss-3.conf', ignore_error=True)
    multi_check(apdev[0], dev, [True, True, True])

def test_ap_bss_add_remove_during_ht_scan(dev, apdev):
    """Dynamic BSS add during HT40 co-ex scan"""
    for i in range(3):
        dev[i].flush_scan_cache()
    ifname1 = apdev[0]['ifname']
    ifname2 = apdev[0]['ifname'] + '-2'
    confname1 = hostapd.cfg_file(apdev[0], "bss-ht40-1.conf")
    confname2 = hostapd.cfg_file(apdev[0], "bss-ht40-2.conf")
    hapd_global = hostapd.HostapdGlobal(apdev)
    hapd_global.send_file(confname1, confname1)
    hapd_global.send_file(confname2, confname2)
    hostapd.add_bss(apdev[0], ifname1, confname1)
    hostapd.add_bss(apdev[0], ifname2, confname2)
    multi_check(apdev[0], dev, [True, True], scan_opt=False)
    hostapd.remove_bss(apdev[0], ifname2)
    hostapd.remove_bss(apdev[0], ifname1)

    hostapd.add_bss(apdev[0], ifname1, confname1)
    hostapd.add_bss(apdev[0], ifname2, confname2)
    hostapd.remove_bss(apdev[0], ifname2)
    multi_check(apdev[0], dev, [True, False], scan_opt=False)
    hostapd.remove_bss(apdev[0], ifname1)

    hostapd.add_bss(apdev[0], ifname1, confname1)
    hostapd.add_bss(apdev[0], ifname2, confname2)
    hostapd.remove_bss(apdev[0], ifname1)
    multi_check(apdev[0], dev, [False, False])

def test_ap_multi_bss_config(dev, apdev):
    """hostapd start with a multi-BSS configuration file"""
    for i in range(3):
        dev[i].flush_scan_cache()
    ifname1 = apdev[0]['ifname']
    ifname2 = apdev[0]['ifname'] + '-2'
    ifname3 = apdev[0]['ifname'] + '-3'
    logger.info("Set up three BSSes with one configuration file")
    hapd = hostapd.add_iface(apdev[0], 'multi-bss.conf')
    hapd.enable()
    multi_check(apdev[0], dev, [True, True, True])
    hostapd.remove_bss(apdev[0], ifname2)
    multi_check(apdev[0], dev, [True, False, True])
    hostapd.remove_bss(apdev[0], ifname3)
    multi_check(apdev[0], dev, [True, False, False])
    hostapd.remove_bss(apdev[0], ifname1)
    multi_check(apdev[0], dev, [False, False, False])

    hapd = hostapd.add_iface(apdev[0], 'multi-bss.conf')
    hapd.enable()
    hostapd.remove_bss(apdev[0], ifname1)
    multi_check(apdev[0], dev, [False, False, False])

def invalid_ap(ap):
    logger.info("Trying to start AP " + ap['ifname'] + " with invalid configuration")
    hapd = hostapd.add_ap(ap, {}, no_enable=True)
    hapd.set("ssid", "invalid-config")
    hapd.set("channel", "12345")
    try:
        hapd.enable()
        started = True
    except Exception as e:
        started = False
    if started:
        raise Exception("ENABLE command succeeded unexpectedly")
    return hapd

@remote_compatible
def test_ap_invalid_config(dev, apdev):
    """Try to start AP with invalid configuration and fix configuration"""
    hapd = invalid_ap(apdev[0])

    logger.info("Fix configuration and start AP again")
    hapd.set("channel", "1")
    hapd.enable()
    dev[0].connect("invalid-config", key_mgmt="NONE", scan_freq="2412")

@remote_compatible
def test_ap_invalid_config2(dev, apdev):
    """Try to start AP with invalid configuration and remove interface"""
    hapd = invalid_ap(apdev[0])
    logger.info("Remove interface with failed configuration")
    hostapd.remove_bss(apdev[0])

def test_ap_remove_during_acs(dev, apdev):
    """Remove interface during ACS"""
    force_prev_ap_on_24g(apdev[0])
    params = hostapd.wpa2_params(ssid="test-acs-remove", passphrase="12345678")
    params['channel'] = '0'
    hostapd.add_ap(apdev[0], params)
    hostapd.remove_bss(apdev[0])

def test_ap_remove_during_acs2(dev, apdev):
    """Remove BSS during ACS in multi-BSS configuration"""
    force_prev_ap_on_24g(apdev[0])
    ifname = apdev[0]['ifname']
    ifname2 = ifname + "-2"
    hapd = hostapd.add_ap(apdev[0], {}, no_enable=True)
    hapd.set("ssid", "test-acs-remove")
    hapd.set("channel", "0")
    hapd.set("bss", ifname2)
    hapd.set("ssid", "test-acs-remove2")
    hapd.enable()
    hostapd.remove_bss(apdev[0])

def test_ap_remove_during_acs3(dev, apdev):
    """Remove second BSS during ACS in multi-BSS configuration"""
    force_prev_ap_on_24g(apdev[0])
    ifname = apdev[0]['ifname']
    ifname2 = ifname + "-2"
    hapd = hostapd.add_ap(apdev[0], {}, no_enable=True)
    hapd.set("ssid", "test-acs-remove")
    hapd.set("channel", "0")
    hapd.set("bss", ifname2)
    hapd.set("ssid", "test-acs-remove2")
    hapd.enable()
    hostapd.remove_bss(apdev[0], ifname2)

@remote_compatible
def test_ap_remove_during_ht_coex_scan(dev, apdev):
    """Remove interface during HT co-ex scan"""
    params = hostapd.wpa2_params(ssid="test-ht-remove", passphrase="12345678")
    params['channel'] = '1'
    params['ht_capab'] = "[HT40+]"
    ifname = apdev[0]['ifname']
    hostapd.add_ap(apdev[0], params)
    hostapd.remove_bss(apdev[0])

def test_ap_remove_during_ht_coex_scan2(dev, apdev):
    """Remove BSS during HT co-ex scan in multi-BSS configuration"""
    ifname = apdev[0]['ifname']
    ifname2 = ifname + "-2"
    hapd = hostapd.add_ap(apdev[0], {}, no_enable=True)
    hapd.set("ssid", "test-ht-remove")
    hapd.set("channel", "1")
    hapd.set("ht_capab", "[HT40+]")
    hapd.set("bss", ifname2)
    hapd.set("ssid", "test-ht-remove2")
    hapd.enable()
    hostapd.remove_bss(apdev[0])

def test_ap_remove_during_ht_coex_scan3(dev, apdev):
    """Remove second BSS during HT co-ex scan in multi-BSS configuration"""
    ifname = apdev[0]['ifname']
    ifname2 = ifname + "-2"
    hapd = hostapd.add_ap(apdev[0], {}, no_enable=True)
    hapd.set("ssid", "test-ht-remove")
    hapd.set("channel", "1")
    hapd.set("ht_capab", "[HT40+]")
    hapd.set("bss", ifname2)
    hapd.set("ssid", "test-ht-remove2")
    hapd.enable()
    hostapd.remove_bss(apdev[0], ifname2)

@remote_compatible
def test_ap_enable_disable_reenable(dev, apdev):
    """Enable, disable, re-enable AP"""
    hapd = hostapd.add_ap(apdev[0], {}, no_enable=True)
    hapd.set("ssid", "dynamic")
    hapd.enable()
    ev = hapd.wait_event(["AP-ENABLED"], timeout=30)
    if ev is None:
        raise Exception("AP startup timed out")
    dev[0].connect("dynamic", key_mgmt="NONE", scan_freq="2412")
    hapd.disable()
    ev = hapd.wait_event(["AP-DISABLED"], timeout=30)
    if ev is None:
        raise Exception("AP disabling timed out")
    dev[0].wait_disconnected(timeout=10)
    hapd.enable()
    ev = hapd.wait_event(["AP-ENABLED"], timeout=30)
    if ev is None:
        raise Exception("AP startup timed out")
    dev[1].connect("dynamic", key_mgmt="NONE", scan_freq="2412")
    dev[0].wait_connected(timeout=10)

def test_ap_double_disable(dev, apdev):
    """Double DISABLE regression test"""
    hapd = hostapd.add_bss(apdev[0], apdev[0]['ifname'], 'bss-1.conf')
    hostapd.add_bss(apdev[0], apdev[0]['ifname'] + '-2', 'bss-2.conf')
    hapd.disable()
    if "FAIL" not in hapd.request("DISABLE"):
        raise Exception("Second DISABLE accepted unexpectedly")
    hapd.enable()
    hapd.disable()
    if "FAIL" not in hapd.request("DISABLE"):
        raise Exception("Second DISABLE accepted unexpectedly")

def test_ap_bss_add_many(dev, apdev):
    """Large number of BSS add operations with hostapd"""
    try:
        _test_ap_bss_add_many(dev, apdev)
    finally:
        dev[0].request("SCAN_INTERVAL 5")
        ifname = apdev[0]['ifname']
        hapd = hostapd.HostapdGlobal(apdev[0])
        hapd.flush()
        for i in range(16):
            ifname2 = ifname + '-' + str(i)
            hapd.remove(ifname2)
        try:
            os.remove('/tmp/hwsim-bss.conf')
        except:
            pass

def _test_ap_bss_add_many(dev, apdev):
    ifname = apdev[0]['ifname']
    hostapd.add_bss(apdev[0], ifname, 'bss-1.conf')
    fname = '/tmp/hwsim-bss.conf'
    for i in range(16):
        ifname2 = ifname + '-' + str(i)
        with open(fname, 'w') as f:
            f.write("driver=nl80211\n")
            f.write("hw_mode=g\n")
            f.write("channel=1\n")
            f.write("ieee80211n=1\n")
            f.write("interface=%s\n" % ifname2)
            f.write("bssid=02:00:00:00:03:%02x\n" % (i + 1))
            f.write("ctrl_interface=/var/run/hostapd\n")
            f.write("ssid=test-%d\n" % i)
        hostapd.add_bss(apdev[0], ifname2, fname)
        os.remove(fname)

    dev[0].request("SCAN_INTERVAL 1")
    dev[0].connect("bss-1", key_mgmt="NONE", scan_freq="2412")
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected(timeout=5)
    for i in range(16):
        dev[0].connect("test-%d" % i, key_mgmt="NONE", scan_freq="2412")
        dev[0].request("DISCONNECT")
        dev[0].wait_disconnected(timeout=5)
        ifname2 = ifname + '-' + str(i)
        hostapd.remove_bss(apdev[0], ifname2)

def test_ap_bss_add_reuse_existing(dev, apdev):
    """Dynamic BSS add operation reusing existing interface"""
    ifname1 = apdev[0]['ifname']
    ifname2 = apdev[0]['ifname'] + '-2'
    hostapd.add_bss(apdev[0], ifname1, 'bss-1.conf')
    subprocess.check_call(["iw", "dev", ifname1, "interface", "add", ifname2,
                           "type", "__ap"])
    hostapd.add_bss(apdev[0], ifname2, 'bss-2.conf')
    hostapd.remove_bss(apdev[0], ifname2)
    subprocess.check_call(["iw", "dev", ifname2, "del"])

def hapd_bss_out_of_mem(hapd, phy, confname, count, func):
    with alloc_fail(hapd, count, func):
        hapd_global = hostapd.HostapdGlobal()
        res = hapd_global.ctrl.request("ADD bss_config=" + phy + ":" + confname)
        if "OK" in res:
            raise Exception("add_bss succeeded")

def test_ap_bss_add_out_of_memory(dev, apdev):
    """Running out of memory while adding a BSS"""
    hapd2 = hostapd.add_ap(apdev[1], {"ssid": "open"})

    ifname1 = apdev[0]['ifname']
    ifname2 = apdev[0]['ifname'] + '-2'

    confname1 = hostapd.cfg_file(apdev[0], "bss-1.conf")
    confname2 = hostapd.cfg_file(apdev[0], "bss-2.conf")
    hapd_bss_out_of_mem(hapd2, 'phy3', confname1, 1, 'hostapd_add_iface')
    for i in range(1, 3):
        hapd_bss_out_of_mem(hapd2, 'phy3', confname1,
                            i, 'hostapd_interface_init_bss')
    hapd_bss_out_of_mem(hapd2, 'phy3', confname1,
                        1, 'ieee802_11_build_ap_params')

    hostapd.add_bss(apdev[0], ifname1, confname1)

    hapd_bss_out_of_mem(hapd2, 'phy3', confname2,
                        1, 'hostapd_interface_init_bss')
    hapd_bss_out_of_mem(hapd2, 'phy3', confname2,
                        1, 'ieee802_11_build_ap_params')

    hostapd.add_bss(apdev[0], ifname2, confname2)
    hostapd.remove_bss(apdev[0], ifname2)
    hostapd.remove_bss(apdev[0], ifname1)

def test_ap_multi_bss(dev, apdev):
    """Multiple BSSes with hostapd"""
    ifname1 = apdev[0]['ifname']
    ifname2 = apdev[0]['ifname'] + '-2'
    hapd1 = hostapd.add_bss(apdev[0], ifname1, 'bss-1.conf')
    hapd2 = hostapd.add_bss(apdev[0], ifname2, 'bss-2.conf')
    dev[0].connect("bss-1", key_mgmt="NONE", scan_freq="2412")
    dev[1].connect("bss-2", key_mgmt="NONE", scan_freq="2412")

    hwsim_utils.test_connectivity(dev[0], hapd1)
    hwsim_utils.test_connectivity(dev[1], hapd2)

    sta0 = hapd1.get_sta(dev[0].own_addr())
    sta1 = hapd2.get_sta(dev[1].own_addr())
    if 'rx_packets' not in sta0 or int(sta0['rx_packets']) < 1:
        raise Exception("sta0 did not report receiving packets")
    if 'rx_packets' not in sta1 or int(sta1['rx_packets']) < 1:
        raise Exception("sta1 did not report receiving packets")

@remote_compatible
def test_ap_add_with_driver(dev, apdev):
    """Add hostapd interface with driver specified"""
    ifname = apdev[0]['ifname']
    try:
       hostname = apdev[0]['hostname']
    except:
       hostname = None
    hapd_global = hostapd.HostapdGlobal(apdev[0])
    hapd_global.add(ifname, driver="nl80211")
    port = hapd_global.get_ctrl_iface_port(ifname)
    hapd = hostapd.Hostapd(ifname, hostname, port)
    hapd.set_defaults()
    hapd.set("ssid", "dynamic")
    hapd.enable()
    ev = hapd.wait_event(["AP-ENABLED"], timeout=30)
    if ev is None:
        raise Exception("AP startup timed out")
    dev[0].connect("dynamic", key_mgmt="NONE", scan_freq="2412")
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()
    hapd.disable()

def test_ap_duplicate_bssid(dev, apdev):
    """Duplicate BSSID"""
    params = {"ssid": "test"}
    hapd = hostapd.add_ap(apdev[0], params, no_enable=True)
    hapd.enable()
    ifname2 = apdev[0]['ifname'] + '-2'
    ifname3 = apdev[0]['ifname'] + '-3'
    # "BSS 'wlan3-2' may not have BSSID set to the MAC address of the radio"
    try:
        hostapd.add_bss(apdev[0], ifname2, 'bss-2-dup.conf')
        raise Exception("BSS add succeeded unexpectedly")
    except Exception as e:
        if "Could not add hostapd BSS" in str(e):
            pass
        else:
            raise

    hostapd.add_bss(apdev[0], ifname3, 'bss-3.conf')

    dev[0].connect("test", key_mgmt="NONE", scan_freq="2412")
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    hapd.set("bssid", "02:00:00:00:03:02")
    hapd.disable()
    # "Duplicate BSSID 02:00:00:00:03:02 on interface 'wlan3-3' and 'wlan3'."
    if "FAIL" not in hapd.request("ENABLE"):
        raise Exception("ENABLE with duplicate BSSID succeeded unexpectedly")

def test_ap_bss_config_file(dev, apdev, params):
    """hostapd BSS config file"""
    pidfile = params['prefix'] + ".hostapd.pid"
    logfile = params['prefix'] + ".hostapd-log"
    prg = os.path.join(params['logdir'], 'alt-hostapd/hostapd/hostapd')
    if not os.path.exists(prg):
        prg = '../../hostapd/hostapd'
    phy = get_phy(apdev[0])
    confname1 = hostapd.cfg_file(apdev[0], "bss-1.conf")
    confname2 = hostapd.cfg_file(apdev[0], "bss-2.conf")
    confname3 = hostapd.cfg_file(apdev[0], "bss-3.conf")

    cmd = [prg, '-B', '-dddt', '-P', pidfile, '-f', logfile, '-S', '-T',
           '-b', phy + ':' + confname1, '-b', phy + ':' + confname2,
           '-b', phy + ':' + confname3]
    res = subprocess.check_call(cmd)
    if res != 0:
        raise Exception("Could not start hostapd: %s" % str(res))
    multi_check(apdev[0], dev, [True, True, True])
    for i in range(0, 3):
        dev[i].request("DISCONNECT")

    hapd = hostapd.Hostapd(apdev[0]['ifname'])
    hapd.ping()
    if "OK" not in hapd.request("TERMINATE"):
        raise Exception("Failed to terminate hostapd process")
    ev = hapd.wait_event(["CTRL-EVENT-TERMINATING"], timeout=15)
    if ev is None:
        raise Exception("CTRL-EVENT-TERMINATING not seen")
    for i in range(30):
        time.sleep(0.1)
        if not os.path.exists(pidfile):
            break
    if os.path.exists(pidfile):
        raise Exception("PID file exits after process termination")
