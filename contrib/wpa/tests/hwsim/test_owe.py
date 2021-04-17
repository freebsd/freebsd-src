# Test cases for Opportunistic Wireless Encryption (OWE)
# Copyright (c) 2017, Jouni Malinen <j@w1.fi>
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

import binascii
import logging
logger = logging.getLogger()
import time
import os
import struct

import hostapd
from wpasupplicant import WpaSupplicant
import hwsim_utils
from tshark import run_tshark
from utils import HwsimSkip, fail_test, alloc_fail, wait_fail_trigger
from test_ap_acs import wait_acs

def test_owe(dev, apdev):
    """Opportunistic Wireless Encryption"""
    if "OWE" not in dev[0].get_capability("key_mgmt"):
        raise HwsimSkip("OWE not supported")
    params = {"ssid": "owe",
              "wpa": "2",
              "ieee80211w": "2",
              "wpa_key_mgmt": "OWE",
              "rsn_pairwise": "CCMP"}
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = hapd.own_addr()
    conf = hapd.request("GET_CONFIG")
    if "key_mgmt=OWE" not in conf.splitlines():
        logger.info("GET_CONFIG:\n" + conf)
        raise Exception("GET_CONFIG did not report correct key_mgmt")

    dev[0].scan_for_bss(bssid, freq="2412")
    bss = dev[0].get_bss(bssid)
    if "[WPA2-OWE-CCMP]" not in bss['flags']:
        raise Exception("OWE AKM not recognized: " + bss['flags'])

    id = dev[0].connect("owe", key_mgmt="OWE", ieee80211w="2", scan_freq="2412")
    hapd.wait_sta()
    pmk_h = hapd.request("GET_PMK " + dev[0].own_addr())
    pmk_w = dev[0].get_pmk(id)
    if pmk_h != pmk_w:
        raise Exception("Fetched PMK does not match: hostapd %s, wpa_supplicant %s" % (pmk_h, pmk_w))
    hwsim_utils.test_connectivity(dev[0], hapd)
    val = dev[0].get_status_field("key_mgmt")
    if val != "OWE":
        raise Exception("Unexpected key_mgmt: " + val)

def test_owe_groups(dev, apdev):
    """Opportunistic Wireless Encryption - DH groups"""
    if "OWE" not in dev[0].get_capability("key_mgmt"):
        raise HwsimSkip("OWE not supported")
    params = {"ssid": "owe",
              "wpa": "2",
              "wpa_key_mgmt": "OWE",
              "rsn_pairwise": "CCMP"}
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = hapd.own_addr()

    dev[0].scan_for_bss(bssid, freq="2412")
    for group in [19, 20, 21]:
        dev[0].connect("owe", key_mgmt="OWE", owe_group=str(group))
        hapd.wait_sta()
        hwsim_utils.test_connectivity(dev[0], hapd)
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()
        dev[0].dump_monitor()
        hapd.dump_monitor()

def test_owe_pmksa_caching(dev, apdev):
    """Opportunistic Wireless Encryption and PMKSA caching"""
    try:
        run_owe_pmksa_caching(dev, apdev)
    finally:
        dev[0].set("reassoc_same_bss_optim", "0")

def test_owe_pmksa_caching_connect_cmd(dev, apdev):
    """Opportunistic Wireless Encryption and PMKSA caching using cfg80211 connect command"""
    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    wpas.interface_add("wlan5", drv_params="force_connect_cmd=1")
    try:
        run_owe_pmksa_caching([wpas], apdev)
    finally:
        wpas.set("reassoc_same_bss_optim", "0")

def run_owe_pmksa_caching(dev, apdev):
    if "OWE" not in dev[0].get_capability("key_mgmt"):
        raise HwsimSkip("OWE not supported")
    params = {"ssid": "owe",
              "wpa": "2",
              "wpa_key_mgmt": "OWE",
              "rsn_pairwise": "CCMP"}
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = hapd.own_addr()

    dev[0].set("reassoc_same_bss_optim", "1")
    dev[0].scan_for_bss(bssid, freq="2412")
    id = dev[0].connect("owe", key_mgmt="OWE")
    hapd.wait_sta()
    hwsim_utils.test_connectivity(dev[0], hapd)
    pmksa = dev[0].get_pmksa(bssid)
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()
    dev[0].dump_monitor()

    dev[0].select_network(id, 2412)
    dev[0].wait_connected()
    hapd.wait_sta()
    hwsim_utils.test_connectivity(dev[0], hapd)
    pmksa2 = dev[0].get_pmksa(bssid)
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()
    dev[0].dump_monitor()

    if "OK" not in hapd.request("PMKSA_FLUSH"):
        raise Exception("PMKSA_FLUSH failed")

    dev[0].select_network(id, 2412)
    dev[0].wait_connected()
    hapd.wait_sta()
    hwsim_utils.test_connectivity(dev[0], hapd)
    pmksa3 = dev[0].get_pmksa(bssid)

    if pmksa is None or pmksa2 is None or pmksa3 is None:
        raise Exception("PMKSA entry missing")
    if pmksa['pmkid'] != pmksa2['pmkid']:
        raise Exception("Unexpected PMKID change when using PMKSA caching")
    if pmksa['pmkid'] == pmksa3['pmkid']:
        raise Exception("PMKID did not change after PMKSA cache flush")

    dev[0].request("REASSOCIATE")
    dev[0].wait_connected()
    pmksa4 = dev[0].get_pmksa(bssid)
    if pmksa3['pmkid'] != pmksa4['pmkid']:
        raise Exception("Unexpected PMKID change when using PMKSA caching [2]")

def test_owe_and_psk(dev, apdev):
    """Opportunistic Wireless Encryption and WPA2-PSK enabled"""
    if "OWE" not in dev[0].get_capability("key_mgmt"):
        raise HwsimSkip("OWE not supported")
    params = {"ssid": "owe+psk",
              "wpa": "2",
              "wpa_key_mgmt": "OWE WPA-PSK",
              "rsn_pairwise": "CCMP",
              "wpa_passphrase": "12345678"}
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = hapd.own_addr()

    dev[0].scan_for_bss(bssid, freq="2412")
    dev[0].connect("owe+psk", psk="12345678")
    hapd.wait_sta()
    hwsim_utils.test_connectivity(dev[0], hapd)

    dev[1].scan_for_bss(bssid, freq="2412")
    dev[1].connect("owe+psk", key_mgmt="OWE")
    hapd.wait_sta()
    hwsim_utils.test_connectivity(dev[1], hapd)

def test_owe_transition_mode(dev, apdev):
    """Opportunistic Wireless Encryption transition mode"""
    run_owe_transition_mode(dev, apdev)

def test_owe_transition_mode_connect_cmd(dev, apdev):
    """Opportunistic Wireless Encryption transition mode using cfg80211 connect command"""
    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    wpas.interface_add("wlan5", drv_params="force_connect_cmd=1")
    run_owe_transition_mode([wpas], apdev)

def test_owe_transition_mode_mismatch1(dev, apdev):
    """Opportunistic Wireless Encryption transition mode (mismatch 1)"""
    run_owe_transition_mode(dev, apdev, adv_bssid0="02:11:22:33:44:55")

def test_owe_transition_mode_mismatch2(dev, apdev):
    """Opportunistic Wireless Encryption transition mode (mismatch 2)"""
    run_owe_transition_mode(dev, apdev, adv_bssid1="02:11:22:33:44:66")

def test_owe_transition_mode_mismatch3(dev, apdev):
    """Opportunistic Wireless Encryption transition mode (mismatch 3)"""
    run_owe_transition_mode(dev, apdev, adv_bssid0="02:11:22:33:44:55",
                            adv_bssid1="02:11:22:33:44:66")

def run_owe_transition_mode(dev, apdev, adv_bssid0=None, adv_bssid1=None):
    if "OWE" not in dev[0].get_capability("key_mgmt"):
        raise HwsimSkip("OWE not supported")
    dev[0].flush_scan_cache()
    adv_bssid = adv_bssid0 if adv_bssid0 else apdev[1]['bssid']
    params = {"ssid": "owe-random",
              "wpa": "2",
              "wpa_key_mgmt": "OWE",
              "rsn_pairwise": "CCMP",
              "ieee80211w": "2",
              "owe_transition_bssid": adv_bssid,
              "owe_transition_ssid": '"owe-test"',
              "ignore_broadcast_ssid": "1"}
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = hapd.own_addr()

    adv_bssid = adv_bssid1 if adv_bssid1 else apdev[0]['bssid']
    params = {"ssid": "owe-test",
              "owe_transition_bssid": adv_bssid,
              "owe_transition_ssid": '"owe-random"'}
    hapd2 = hostapd.add_ap(apdev[1], params)
    bssid2 = hapd2.own_addr()

    dev[0].scan_for_bss(bssid, freq="2412")
    dev[0].scan_for_bss(bssid2, freq="2412")

    bss = dev[0].get_bss(bssid)
    if "[WPA2-OWE-CCMP]" not in bss['flags']:
        raise Exception("OWE AKM not recognized: " + bss['flags'])
    if "[OWE-TRANS]" not in bss['flags']:
        raise Exception("OWE transition not recognized: " + bss['flags'])

    bss = dev[0].get_bss(bssid2)
    if "[OWE-TRANS-OPEN]" not in bss['flags']:
        raise Exception("OWE transition (open) not recognized: " + bss['flags'])

    id = dev[0].connect("owe-test", key_mgmt="OWE", ieee80211w="2",
                        scan_freq="2412")
    hapd.wait_sta()
    hwsim_utils.test_connectivity(dev[0], hapd)
    val = dev[0].get_status_field("key_mgmt")
    if val != "OWE":
        raise Exception("Unexpected key_mgmt: " + val)

    logger.info("Move to OWE only mode (disable transition mode)")

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()
    dev[0].dump_monitor()

    hapd2.disable()
    hapd.disable()
    dev[0].flush_scan_cache()
    hapd.set("owe_transition_bssid", "00:00:00:00:00:00")
    hapd.set("ignore_broadcast_ssid", '0')
    hapd.set("ssid", 'owe-test')
    hapd.enable()

    dev[0].scan_for_bss(bssid, freq="2412")
    dev[0].select_network(id, 2412)
    dev[0].wait_connected()
    hapd.wait_sta()
    hwsim_utils.test_connectivity(dev[0], hapd)

def test_owe_transition_mode_ifname(dev, apdev):
    """Opportunistic Wireless Encryption transition mode (ifname)"""
    if "OWE" not in dev[0].get_capability("key_mgmt"):
        raise HwsimSkip("OWE not supported")
    dev[0].flush_scan_cache()
    params = {"ssid": "owe-random",
              "wpa": "2",
              "wpa_key_mgmt": "OWE",
              "rsn_pairwise": "CCMP",
              "ieee80211w": "2",
              "owe_transition_ifname": apdev[1]['ifname'],
              "ignore_broadcast_ssid": "1"}
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = hapd.own_addr()

    params = {"ssid": "owe-test",
              "owe_transition_ifname": apdev[0]['ifname']}
    hapd2 = hostapd.add_ap(apdev[1], params)
    bssid2 = hapd2.own_addr()

    dev[0].scan_for_bss(bssid, freq="2412")
    dev[0].scan_for_bss(bssid2, freq="2412")

    id = dev[0].connect("owe-test", key_mgmt="OWE", ieee80211w="2",
                        scan_freq="2412")
    val = dev[0].get_status_field("key_mgmt")
    if val != "OWE":
        raise Exception("Unexpected key_mgmt: " + val)

def test_owe_transition_mode_ifname_acs(dev, apdev):
    """Opportunistic Wireless Encryption transition mode (ifname, ACS)"""
    run_owe_transition_mode_ifname_acs(dev, apdev, wait_first=False)

def test_owe_transition_mode_ifname_acs2(dev, apdev):
    """Opportunistic Wireless Encryption transition mode (ifname, ACS)"""
    run_owe_transition_mode_ifname_acs(dev, apdev, wait_first=True)

def run_owe_transition_mode_ifname_acs(dev, apdev, wait_first):
    if "OWE" not in dev[0].get_capability("key_mgmt"):
        raise HwsimSkip("OWE not supported")
    dev[0].flush_scan_cache()
    params = {"ssid": "owe-random",
              "channel": "0",
              "wpa": "2",
              "wpa_key_mgmt": "OWE",
              "rsn_pairwise": "CCMP",
              "ieee80211w": "2",
              "owe_transition_ifname": apdev[1]['ifname'],
              "ignore_broadcast_ssid": "1"}
    hapd = hostapd.add_ap(apdev[0], params, wait_enabled=False)
    bssid = hapd.own_addr()

    if wait_first:
        wait_acs(hapd)

    params = {"ssid": "owe-test",
              "channel": "0",
              "owe_transition_ifname": apdev[0]['ifname']}
    hapd2 = hostapd.add_ap(apdev[1], params, wait_enabled=False)
    bssid2 = hapd2.own_addr()

    wait_acs(hapd2)
    if not wait_first:
        state = hapd.get_status_field("state")
        if state == "ACS-STARTED":
            time.sleep(5)
            state = hapd.get_status_field("state")
        if state != "ENABLED":
            raise Exception("AP1 startup did not succeed")

    freq = hapd.get_status_field("freq")
    freq2 = hapd2.get_status_field("freq")

    dev[0].scan_for_bss(bssid, freq=freq)
    dev[0].scan_for_bss(bssid2, freq=freq2)

    id = dev[0].connect("owe-test", key_mgmt="OWE", ieee80211w="2",
                        scan_freq="%s %s" % (freq, freq2))
    val = dev[0].get_status_field("key_mgmt")
    if val != "OWE":
        raise Exception("Unexpected key_mgmt: " + val)

def test_owe_transition_mode_open_only_ap(dev, apdev):
    """Opportunistic Wireless Encryption transition mode connect to open-only AP"""
    if "OWE" not in dev[0].get_capability("key_mgmt"):
        raise HwsimSkip("OWE not supported")
    dev[0].flush_scan_cache()
    params = {"ssid": "owe-test-open"}
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = hapd.own_addr()

    dev[0].scan_for_bss(bssid, freq="2412")

    bss = dev[0].get_bss(bssid)

    id = dev[0].connect("owe-test-open", key_mgmt="OWE", ieee80211w="2",
                        scan_freq="2412")
    hwsim_utils.test_connectivity(dev[0], hapd)
    val = dev[0].get_status_field("key_mgmt")
    if val != "NONE":
        raise Exception("Unexpected key_mgmt: " + val)

def test_owe_only_sta(dev, apdev):
    """Opportunistic Wireless Encryption transition mode disabled on STA"""
    if "OWE" not in dev[0].get_capability("key_mgmt"):
        raise HwsimSkip("OWE not supported")
    dev[0].flush_scan_cache()
    params = {"ssid": "owe-test-open"}
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = hapd.own_addr()

    dev[0].scan_for_bss(bssid, freq="2412")
    id = dev[0].connect("owe-test-open", key_mgmt="OWE", ieee80211w="2",
                        scan_freq="2412", owe_only="1", wait_connect=False)
    ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED",
                            "CTRL-EVENT-NETWORK-NOT-FOUND"], timeout=10)
    if not ev:
        raise Exception("Unknown result for the connection attempt")
    if "CTRL-EVENT-CONNECTED" in ev:
        raise Exception("Unexpected connection to open network")
    dev[0].request("DISCONNECT")
    dev[0].dump_monitor()

    params = {"ssid": "owe-test-open",
              "wpa": "2",
              "ieee80211w": "2",
              "wpa_key_mgmt": "OWE",
              "rsn_pairwise": "CCMP"}
    hapd2 = hostapd.add_ap(apdev[1], params)
    dev[0].request("RECONNECT")
    dev[0].wait_connected()

def test_owe_transition_mode_open_multiple_scans(dev, apdev):
    """Opportunistic Wireless Encryption transition mode and need for multiple scans"""
    if "OWE" not in dev[0].get_capability("key_mgmt"):
        raise HwsimSkip("OWE not supported")
    dev[0].flush_scan_cache()
    params = {"ssid": "owe-test",
              "owe_transition_bssid": apdev[0]['bssid'],
              "owe_transition_ssid": '"owe-random"'}
    hapd2 = hostapd.add_ap(apdev[1], params)
    bssid2 = hapd2.own_addr()

    dev[0].scan_for_bss(bssid2, freq="2412")

    dev[0].dump_monitor()
    id = dev[0].connect("owe-test", key_mgmt="OWE", ieee80211w="2",
                        scan_freq="2412", wait_connect=False)
    ev = dev[0].wait_event(["CTRL-EVENT-SCAN-RESULTS"], timeout=1)

    params = {"ssid": "owe-random",
              "wpa": "2",
              "wpa_key_mgmt": "OWE",
              "rsn_pairwise": "CCMP",
              "ieee80211w": "2",
              "owe_transition_bssid": apdev[1]['bssid'],
              "owe_transition_ssid": '"owe-test"',
              "ignore_broadcast_ssid": "1"}
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = hapd.own_addr()

    dev[0].wait_connected()

    val = dev[0].get_status_field("key_mgmt")
    if val != "OWE":
        raise Exception("Unexpected key_mgmt: " + val)

def test_owe_transition_mode_multi_bss(dev, apdev):
    """Opportunistic Wireless Encryption transition mode (multi BSS)"""
    try:
        run_owe_transition_mode_multi_bss(dev, apdev)
    finally:
        dev[0].request("SCAN_INTERVAL 5")

def run_owe_transition_mode_multi_bss(dev, apdev):
    if "OWE" not in dev[0].get_capability("key_mgmt"):
        raise HwsimSkip("OWE not supported")
    ifname1 = apdev[0]['ifname']
    ifname2 = apdev[0]['ifname'] + '-2'
    hapd1 = hostapd.add_bss(apdev[0], ifname1, 'owe-bss-1.conf')
    hapd2 = hostapd.add_bss(apdev[0], ifname2, 'owe-bss-2.conf')
    hapd2.bssidx = 1

    bssid = hapd1.own_addr()
    bssid2 = hapd2.own_addr()

    # Beaconing with the OWE Transition Mode element can start only once both
    # BSSs are enabled, so the very first Beacon frame may go out without this
    # element. Wait a bit to avoid getting incomplete scan results.
    time.sleep(0.1)

    dev[0].request("SCAN_INTERVAL 1")
    dev[0].scan_for_bss(bssid2, freq="2412")
    dev[0].scan_for_bss(bssid, freq="2412")
    dev[0].connect("transition-mode-open", key_mgmt="OWE")
    val = dev[0].get_status_field("bssid")
    if val != bssid2:
        raise Exception("Unexpected bssid: " + val)
    val = dev[0].get_status_field("key_mgmt")
    if val != "OWE":
        raise Exception("Unexpected key_mgmt: " + val)
    hwsim_utils.test_connectivity(dev[0], hapd2)

def test_owe_transition_mode_rsne_mismatch(dev, apdev):
    """Opportunistic Wireless Encryption transition mode and RSNE mismatch"""
    if "OWE" not in dev[0].get_capability("key_mgmt"):
        raise HwsimSkip("OWE not supported")
    dev[0].flush_scan_cache()
    params = {"ssid": "owe-random",
              "wpa": "2",
              "wpa_key_mgmt": "OWE",
              "rsn_pairwise": "CCMP",
              "ieee80211w": "2",
              "rsne_override_eapol": "30140100000fac040100000fac040100000fac020c00",
              "owe_transition_bssid": apdev[1]['bssid'],
              "owe_transition_ssid": '"owe-test"',
              "ignore_broadcast_ssid": "1"}
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = hapd.own_addr()

    params = {"ssid": "owe-test",
              "owe_transition_bssid": apdev[0]['bssid'],
              "owe_transition_ssid": '"owe-random"'}
    hapd2 = hostapd.add_ap(apdev[1], params)
    bssid2 = hapd2.own_addr()

    dev[0].scan_for_bss(bssid, freq="2412")
    dev[0].scan_for_bss(bssid2, freq="2412")

    id = dev[0].connect("owe-test", key_mgmt="OWE", ieee80211w="2",
                        scan_freq="2412", wait_connect=False)
    ev = dev[0].wait_event(["PMKSA-CACHE-ADDED"], timeout=5)
    if ev is None:
        raise Exception("OWE PMKSA not created")
    ev = dev[0].wait_event(["WPA: IE in 3/4 msg does not match with IE in Beacon/ProbeResp"],
                           timeout=5)
    if ev is None:
        raise Exception("RSNE mismatch not reported")
    ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED",
                            "CTRL-EVENT-DISCONNECTED"], timeout=5)
    dev[0].request("REMOVE_NETWORK all")
    if ev is None:
        raise Exception("No disconnection seen")
    if "CTRL-EVENT-DISCONNECTED" not in ev:
        raise Exception("Unexpected connection")
    if "reason=17 locally_generated=1" not in ev:
        raise Exception("Unexpected disconnection reason: " + ev)

def test_owe_unsupported_group(dev, apdev):
    """Opportunistic Wireless Encryption and unsupported group"""
    try:
        run_owe_unsupported_group(dev, apdev)
    finally:
        dev[0].request("VENDOR_ELEM_REMOVE 13 *")

def test_owe_unsupported_group_connect_cmd(dev, apdev):
    """Opportunistic Wireless Encryption and unsupported group using cfg80211 connect command"""
    try:
        wpas = None
        wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
        wpas.interface_add("wlan5", drv_params="force_connect_cmd=1")
        run_owe_unsupported_group([wpas], apdev)
    finally:
        if wpas:
            wpas.request("VENDOR_ELEM_REMOVE 13 *")

def run_owe_unsupported_group(dev, apdev):
    if "OWE" not in dev[0].get_capability("key_mgmt"):
        raise HwsimSkip("OWE not supported")
    # Override OWE Dh Parameters element with a payload that uses invalid group
    # 0 (and actual group 19 data) to make the AP reject this with the specific
    # status code 77.
    dev[0].request("VENDOR_ELEM_ADD 13 ff23200000783590fb7440e03d5b3b33911f86affdcc6b4411b707846ac4ff08ddc8831ccd")

    params = {"ssid": "owe",
              "wpa": "2",
              "wpa_key_mgmt": "OWE",
              "rsn_pairwise": "CCMP"}
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = hapd.own_addr()

    dev[0].scan_for_bss(bssid, freq="2412")
    dev[0].connect("owe", key_mgmt="OWE", wait_connect=False)
    ev = dev[0].wait_event(["CTRL-EVENT-ASSOC-REJECT"], timeout=10)
    dev[0].request("DISCONNECT")
    if ev is None:
        raise Exception("Association not rejected")
    if "status_code=77" not in ev:
        raise Exception("Unexpected rejection reason: " + ev)

def test_owe_limited_group_set(dev, apdev):
    """Opportunistic Wireless Encryption and limited group set"""
    if "OWE" not in dev[0].get_capability("key_mgmt"):
        raise HwsimSkip("OWE not supported")
    params = {"ssid": "owe",
              "wpa": "2",
              "wpa_key_mgmt": "OWE",
              "rsn_pairwise": "CCMP",
              "owe_groups": "20 21"}
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = hapd.own_addr()

    dev[0].scan_for_bss(bssid, freq="2412")
    dev[0].connect("owe", key_mgmt="OWE", owe_group="19", wait_connect=False)
    ev = dev[0].wait_event(["CTRL-EVENT-ASSOC-REJECT"], timeout=10)
    dev[0].request("DISCONNECT")
    if ev is None:
        raise Exception("Association not rejected")
    if "status_code=77" not in ev:
        raise Exception("Unexpected rejection reason: " + ev)
    dev[0].dump_monitor()

    for group in [20, 21]:
        dev[0].connect("owe", key_mgmt="OWE", owe_group=str(group))
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()
        dev[0].dump_monitor()

def test_owe_limited_group_set_pmf(dev, apdev, params):
    """Opportunistic Wireless Encryption and limited group set (PMF)"""
    if "OWE" not in dev[0].get_capability("key_mgmt"):
        raise HwsimSkip("OWE not supported")
    pcapng = os.path.join(params['logdir'], "hwsim0.pcapng")

    params = {"ssid": "owe",
              "wpa": "2",
              "ieee80211w": "2",
              "wpa_key_mgmt": "OWE",
              "rsn_pairwise": "CCMP",
              "owe_groups": "21"}
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = hapd.own_addr()

    dev[0].scan_for_bss(bssid, freq="2412")
    dev[0].connect("owe", key_mgmt="OWE", owe_group="19", ieee80211w="2",
                   scan_freq="2412", wait_connect=False)
    ev = dev[0].wait_event(["CTRL-EVENT-ASSOC-REJECT"], timeout=10)
    dev[0].request("DISCONNECT")
    if ev is None:
        raise Exception("Association not rejected")
    if "status_code=77" not in ev:
        raise Exception("Unexpected rejection reason: " + ev)
    dev[0].dump_monitor()

    dev[0].connect("owe", key_mgmt="OWE", owe_group="20", ieee80211w="2",
                   scan_freq="2412", wait_connect=False)
    ev = dev[0].wait_event(["CTRL-EVENT-ASSOC-REJECT"], timeout=10)
    dev[0].request("DISCONNECT")
    if ev is None:
        raise Exception("Association not rejected (2)")
    if "status_code=77" not in ev:
        raise Exception("Unexpected rejection reason (2): " + ev)
    dev[0].dump_monitor()

    dev[0].connect("owe", key_mgmt="OWE", owe_group="21", ieee80211w="2",
                   scan_freq="2412")
    dev[0].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected()
    dev[0].dump_monitor()

    out = run_tshark(pcapng,
                     "wlan.fc.type_subtype == 1",
                     display=['wlan_mgt.fixed.status_code'])
    status = out.splitlines()
    logger.info("Association Response frame status codes: " + str(status))
    if len(status) != 3:
        raise Exception("Unexpected number of Association Response frames")
    if (int(status[0], base=0) != 77 or int(status[1], base=0) != 77 or
        int(status[2], base=0) != 0):
        raise Exception("Unexpected Association Response frame status code")

def test_owe_group_negotiation(dev, apdev):
    """Opportunistic Wireless Encryption and group negotiation"""
    run_owe_group_negotiation(dev[0], apdev)

def test_owe_group_negotiation_connect_cmd(dev, apdev):
    """Opportunistic Wireless Encryption and group negotiation (connect command)"""
    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    wpas.interface_add("wlan5", drv_params="force_connect_cmd=1")
    run_owe_group_negotiation(wpas, apdev)

def run_owe_group_negotiation(dev, apdev):
    if "OWE" not in dev.get_capability("key_mgmt"):
        raise HwsimSkip("OWE not supported")
    params = {"ssid": "owe",
              "wpa": "2",
              "wpa_key_mgmt": "OWE",
              "rsn_pairwise": "CCMP",
              "owe_groups": "21"}
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = hapd.own_addr()

    dev.scan_for_bss(bssid, freq="2412")
    dev.connect("owe", key_mgmt="OWE")

def test_owe_assoc_reject(dev, apdev):
    """Opportunistic Wireless Encryption association rejection handling"""
    if "OWE" not in dev[0].get_capability("key_mgmt"):
        raise HwsimSkip("OWE not supported")
    params = {"ssid": "owe",
              "require_ht": "1",
              "wpa": "2",
              "ieee80211w": "2",
              "wpa_key_mgmt": "OWE",
              "rsn_pairwise": "CCMP",
              "owe_groups": "19"}
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = hapd.own_addr()

    # First, reject two associations with HT-required (i.e., not OWE related)
    dev[0].scan_for_bss(bssid, freq="2412")
    dev[0].connect("owe", key_mgmt="OWE", ieee80211w="2",
                   disable_ht="1", scan_freq="2412", wait_connect=False)
    for i in range(0, 2):
        ev = dev[0].wait_event(["CTRL-EVENT-ASSOC-REJECT"], timeout=10)
        if ev is None:
            raise Exception("Association rejection not reported")

    # Then, verify that STA tries OWE with the default group (19) on the next
    # attempt instead of having moved to testing another group.
    hapd.set("require_ht", "0")
    for i in range(0, 2):
        ev = dev[0].wait_event(["CTRL-EVENT-ASSOC-REJECT",
                                "CTRL-EVENT-CONNECTED"], timeout=10)
        if ev is None:
            raise Exception("Association result not reported")
        if "CTRL-EVENT-CONNECTED" in ev:
            break
        if "status_code=77" in ev:
            raise Exception("Unexpected unsupport group rejection")
    if "CTRL-EVENT-CONNECTED" not in ev:
        raise Exception("Did not connect successfully")

def test_owe_local_errors(dev, apdev):
    """Opportunistic Wireless Encryption - local errors on supplicant"""
    if "OWE" not in dev[0].get_capability("key_mgmt"):
        raise HwsimSkip("OWE not supported")
    params = {"ssid": "owe",
              "wpa": "2",
              "ieee80211w": "2",
              "wpa_key_mgmt": "OWE",
              "rsn_pairwise": "CCMP"}
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = hapd.own_addr()

    dev[0].scan_for_bss(bssid, freq="2412")

    tests = [(1, "crypto_ecdh_init;owe_build_assoc_req"),
             (1, "crypto_ecdh_get_pubkey;owe_build_assoc_req"),
             (1, "wpabuf_alloc;owe_build_assoc_req")]
    for count, func in tests:
        with alloc_fail(dev[0], count, func):
            dev[0].connect("owe", key_mgmt="OWE", owe_group="20",
                           ieee80211w="2",
                           scan_freq="2412", wait_connect=False)
            wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].dump_monitor()

    tests = [(1, "crypto_ecdh_set_peerkey;owe_process_assoc_resp"),
             (1, "crypto_ecdh_get_pubkey;owe_process_assoc_resp"),
             (1, "wpabuf_alloc;=owe_process_assoc_resp")]
    for count, func in tests:
        with alloc_fail(dev[0], count, func):
            dev[0].connect("owe", key_mgmt="OWE", owe_group="20",
                           ieee80211w="2",
                           scan_freq="2412", wait_connect=False)
            dev[0].wait_disconnected()
            dev[0].request("REMOVE_NETWORK all")
            dev[0].dump_monitor()

    tests = [(1, "hmac_sha256;owe_process_assoc_resp", 19),
             (1, "hmac_sha256_kdf;owe_process_assoc_resp", 19),
             (1, "hmac_sha384;owe_process_assoc_resp", 20),
             (1, "hmac_sha384_kdf;owe_process_assoc_resp", 20),
             (1, "hmac_sha512;owe_process_assoc_resp", 21),
             (1, "hmac_sha512_kdf;owe_process_assoc_resp", 21)]
    for count, func, group in tests:
        with fail_test(dev[0], count, func):
            dev[0].connect("owe", key_mgmt="OWE", owe_group=str(group),
                           ieee80211w="2",
                           scan_freq="2412", wait_connect=False)
            dev[0].wait_disconnected()
            dev[0].request("REMOVE_NETWORK all")
            dev[0].dump_monitor()

    dev[0].connect("owe", key_mgmt="OWE", owe_group="18",
                   ieee80211w="2",
                   scan_freq="2412", wait_connect=False)
    ev = dev[0].wait_event(["SME: Trying to authenticate"], timeout=5)
    if ev is None:
        raise Exception("No authentication attempt")
    time.sleep(0.5)
    dev[0].request("REMOVE_NETWORK all")
    dev[0].dump_monitor()

def hapd_auth(hapd):
    for i in range(0, 10):
        req = hapd.mgmt_rx()
        if req is None:
            raise Exception("MGMT RX wait timed out")
        if req['subtype'] == 11:
            break
        req = None
    if not req:
        raise Exception("Authentication frame not received")

    resp = {}
    resp['fc'] = req['fc']
    resp['da'] = req['sa']
    resp['sa'] = req['da']
    resp['bssid'] = req['bssid']
    resp['payload'] = struct.pack('<HHH', 0, 2, 0)
    hapd.mgmt_tx(resp)

def hapd_assoc(hapd, extra):
    for i in range(0, 10):
        req = hapd.mgmt_rx()
        if req is None:
            raise Exception("MGMT RX wait timed out")
        if req['subtype'] == 0:
            break
        req = None
    if not req:
        raise Exception("Association Request frame not received")

    resp = {}
    resp['fc'] = 0x0010
    resp['da'] = req['sa']
    resp['sa'] = req['da']
    resp['bssid'] = req['bssid']
    payload = struct.pack('<HHH', 0x0411, 0, 0xc001)
    payload += binascii.unhexlify("010882848b960c121824")
    resp['payload'] = payload + extra
    hapd.mgmt_tx(resp)

def test_owe_invalid_assoc_resp(dev, apdev):
    """Opportunistic Wireless Encryption - invalid Association Response frame"""
    if "OWE" not in dev[0].get_capability("key_mgmt"):
        raise HwsimSkip("OWE not supported")
    params = {"ssid": "owe",
              "wpa": "2",
              "ieee80211w": "2",
              "wpa_key_mgmt": "OWE",
              "rsn_pairwise": "CCMP"}
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = hapd.own_addr()

    dev[0].scan_for_bss(bssid, freq="2412")

    hapd.set("ext_mgmt_frame_handling", "1")
    # OWE: No Diffie-Hellman Parameter element found in Association Response frame
    tests = [b'']
    # No room for group --> no DH Params
    tests += [binascii.unhexlify('ff0120')]
    # OWE: Unexpected Diffie-Hellman group in response: 18
    tests += [binascii.unhexlify('ff03201200')]
    # OWE: Invalid peer DH public key
    tests += [binascii.unhexlify('ff23201300' + 31*'00' + '01')]
    # OWE: Invalid peer DH public key
    tests += [binascii.unhexlify('ff24201300' + 33*'ee')]
    for extra in tests:
        dev[0].connect("owe", key_mgmt="OWE", owe_group="19", ieee80211w="2",
                       scan_freq="2412", wait_connect=False)
        hapd_auth(hapd)
        hapd_assoc(hapd, extra)
        dev[0].wait_disconnected()
        dev[0].request("REMOVE_NETWORK all")
        dev[0].dump_monitor()

    # OWE: Empty public key (this ends up getting padded to a valid point)
    dev[0].connect("owe", key_mgmt="OWE", owe_group="19", ieee80211w="2",
                   scan_freq="2412", wait_connect=False)
    hapd_auth(hapd)
    hapd_assoc(hapd, binascii.unhexlify('ff03201300'))
    ev = dev[0].wait_event(["CTRL-EVENT-DISCONNECTED", "PMKSA-CACHE-ADDED"],
                           timeout=5)
    if ev is None:
        raise Exception("No result reported for empty public key")
    dev[0].request("REMOVE_NETWORK all")
    dev[0].dump_monitor()

def start_owe(dev, apdev, workaround=0):
    if "OWE" not in dev[0].get_capability("key_mgmt"):
        raise HwsimSkip("OWE not supported")
    params = {"ssid": "owe",
              "wpa": "2",
              "ieee80211w": "2",
              "wpa_key_mgmt": "OWE",
              "owe_ptk_workaround": str(workaround),
              "rsn_pairwise": "CCMP"}
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].scan_for_bss(hapd.own_addr(), freq="2412")
    return hapd

def owe_check_ok(dev, hapd, owe_group, owe_ptk_workaround):
    dev.connect("owe", key_mgmt="OWE", ieee80211w="2",
                owe_group=owe_group, owe_ptk_workaround=owe_ptk_workaround,
                scan_freq="2412")
    hapd.wait_sta()
    dev.request("REMOVE_NETWORK all")
    dev.wait_disconnected()
    dev.dump_monitor()

def test_owe_ptk_workaround_ap(dev, apdev):
    """Opportunistic Wireless Encryption - AP using PTK workaround"""
    hapd = start_owe(dev, apdev, workaround=1)
    for group, workaround in [(19, 0), (20, 0), (21, 0),
                              (19, 1), (20, 1), (21, 1)]:
        owe_check_ok(dev[0], hapd, str(group), str(workaround))

def test_owe_ptk_hash(dev, apdev):
    """Opportunistic Wireless Encryption - PTK derivation hash alg"""
    hapd = start_owe(dev, apdev)
    for group, workaround in [(19, 0), (20, 0), (21, 0), (19, 1)]:
        owe_check_ok(dev[0], hapd, str(group), str(workaround))

    for group in [20, 21]:
        dev[0].connect("owe", key_mgmt="OWE", ieee80211w="2",
                       owe_group=str(group), owe_ptk_workaround="1",
                       scan_freq="2412", wait_connect=False)
        ev = dev[0].wait_event(["PMKSA-CACHE-ADDED"], timeout=10)
        if ev is None:
            raise Exception("Could not complete OWE association")
        ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED",
                                "CTRL-EVENT-DISCONNECTED"], timeout=5)
        if ev is None:
            raise Exception("Unknown connection result")
        if "CTRL-EVENT-CONNECTED" in ev:
            raise Exception("Unexpected connection")
        dev[0].request("REMOVE_NETWORK all")
        ev = dev[0].wait_event(["PMKSA-CACHE-REMOVED"], timeout=5)
        if ev is None:
            raise Exception("No PMKSA cache removal event seen")
        dev[0].dump_monitor()

def test_owe_transition_mode_disable(dev, apdev):
    """Opportunistic Wireless Encryption transition mode disable"""
    if "OWE" not in dev[0].get_capability("key_mgmt"):
        raise HwsimSkip("OWE not supported")
    dev[0].flush_scan_cache()
    params = {"ssid": "owe-random",
              "wpa": "2",
              "wpa_key_mgmt": "OWE",
              "rsn_pairwise": "CCMP",
              "ieee80211w": "2",
              "transition_disable": '0x08',
              "owe_transition_bssid": apdev[1]['bssid'],
              "owe_transition_ssid": '"owe-test"',
              "ignore_broadcast_ssid": "1"}
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = hapd.own_addr()

    params = {"ssid": "owe-test",
              "owe_transition_bssid": apdev[0]['bssid'],
              "owe_transition_ssid": '"owe-random"'}
    hapd2 = hostapd.add_ap(apdev[1], params)
    bssid2 = hapd2.own_addr()

    dev[0].scan_for_bss(bssid, freq="2412")
    dev[0].scan_for_bss(bssid2, freq="2412")

    id = dev[0].connect("owe-test", key_mgmt="OWE", ieee80211w="2",
                        scan_freq="2412")

    ev = dev[0].wait_event(["TRANSITION-DISABLE"], timeout=1)
    if ev is None:
        raise Exception("Transition disable not indicated")
    if ev.split(' ')[1] != "08":
        raise Exception("Unexpected transition disable bitmap: " + ev)

    val = dev[0].get_network(id, "owe_only")
    if val != "1":
        raise Exception("Unexpected owe_only value: " + val)

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()
    dev[0].request("RECONNECT")
    dev[0].wait_connected()
