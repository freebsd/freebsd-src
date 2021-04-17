# Deprecated WEXT driver interface in wpa_supplicant
# Copyright (c) 2013-2015, Jouni Malinen <j@w1.fi>
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

import logging
logger = logging.getLogger()
import os

import hostapd
import hwsim_utils
from wpasupplicant import WpaSupplicant
from utils import *
from test_rfkill import get_rfkill

def get_wext_interface():
    if not os.path.exists("/proc/net/wireless"):
        raise HwsimSkip("WEXT support not included in the kernel")

    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    try:
        wpas.interface_add("wlan5", driver="wext")
    except Exception as e:
        wpas.close_ctrl()
        raise HwsimSkip("WEXT driver support not included in wpa_supplicant")
    return wpas

def test_wext_open(dev, apdev):
    """WEXT driver interface with open network"""
    wpas = get_wext_interface()

    params = {"ssid": "wext-open"}
    hapd = hostapd.add_ap(apdev[0], params)

    wpas.connect("wext-open", key_mgmt="NONE")
    hwsim_utils.test_connectivity(wpas, hapd)

def test_wext_wpa2_psk(dev, apdev):
    """WEXT driver interface with WPA2-PSK"""
    wpas = get_wext_interface()

    params = hostapd.wpa2_params(ssid="wext-wpa2-psk", passphrase="12345678")
    hapd = hostapd.add_ap(apdev[0], params)

    wpas.connect("wext-wpa2-psk", psk="12345678")
    hwsim_utils.test_connectivity(wpas, hapd)
    if "RSSI=" not in wpas.request("SIGNAL_POLL"):
        raise Exception("Missing RSSI from SIGNAL_POLL")

    wpas.dump_monitor()
    hapd.request("DEAUTHENTICATE " + wpas.p2p_interface_addr())
    wpas.wait_disconnected(timeout=15)

def test_wext_wpa_psk(dev, apdev):
    """WEXT driver interface with WPA-PSK"""
    skip_with_fips(dev[0])
    skip_without_tkip(dev[0])
    wpas = get_wext_interface()

    params = hostapd.wpa_params(ssid="wext-wpa-psk", passphrase="12345678")
    hapd = hostapd.add_ap(apdev[0], params)
    testfile = "/sys/kernel/debug/ieee80211/%s/netdev:%s/tkip_mic_test" % (hapd.get_driver_status_field("phyname"), apdev[0]['ifname'])
    if not os.path.exists(testfile):
        wpas.close_ctrl()
        raise HwsimSkip("tkip_mic_test not supported in mac80211")

    wpas.connect("wext-wpa-psk", psk="12345678")
    hwsim_utils.test_connectivity(wpas, hapd)

    with open(testfile, "w") as f:
        f.write(wpas.p2p_interface_addr())
    ev = wpas.wait_event(["CTRL-EVENT-DISCONNECTED"], timeout=1)
    if ev is not None:
        raise Exception("Unexpected disconnection on first Michael MIC failure")

    with open(testfile, "w") as f:
        f.write("ff:ff:ff:ff:ff:ff")
    ev = wpas.wait_disconnected(timeout=10,
                                error="No disconnection after two Michael MIC failures")
    if "reason=14 locally_generated=1" not in ev:
        raise Exception("Unexpected disconnection reason: " + ev)

def test_wext_pmksa_cache(dev, apdev):
    """PMKSA caching with WEXT"""
    wpas = get_wext_interface()

    params = hostapd.wpa2_eap_params(ssid="test-pmksa-cache")
    hostapd.add_ap(apdev[0], params)
    bssid = apdev[0]['bssid']
    wpas.connect("test-pmksa-cache", proto="RSN", key_mgmt="WPA-EAP",
                   eap="GPSK", identity="gpsk user",
                   password="abcdefghijklmnop0123456789abcdef",
                   scan_freq="2412")
    pmksa = wpas.get_pmksa(bssid)
    if pmksa is None:
        raise Exception("No PMKSA cache entry created")
    if pmksa['opportunistic'] != '0':
        raise Exception("Unexpected opportunistic PMKSA cache entry")

    hostapd.add_ap(apdev[1], params)
    bssid2 = apdev[1]['bssid']

    wpas.dump_monitor()
    logger.info("Roam to AP2")
    # It can take some time for the second AP to become ready to reply to Probe
    # Request frames especially under heavy CPU load, so allow couple of rounds
    # of scanning to avoid reporting errors incorrectly just because of scans
    # not having seen the target AP.
    for i in range(3):
        wpas.scan()
        if wpas.get_bss(bssid2) is not None:
            break
        logger.info("Scan again to find target AP")
    wpas.request("ROAM " + bssid2)
    ev = wpas.wait_event(["CTRL-EVENT-EAP-SUCCESS"], timeout=10)
    if ev is None:
        raise Exception("EAP success timed out")
    wpas.wait_connected(timeout=10, error="Roaming timed out")
    pmksa2 = wpas.get_pmksa(bssid2)
    if pmksa2 is None:
        raise Exception("No PMKSA cache entry found")
    if pmksa2['opportunistic'] != '0':
        raise Exception("Unexpected opportunistic PMKSA cache entry")

    wpas.dump_monitor()
    logger.info("Roam back to AP1")
    wpas.scan()
    wpas.request("ROAM " + bssid)
    ev = wpas.wait_event(["CTRL-EVENT-EAP-STARTED",
                          "CTRL-EVENT-CONNECTED"], timeout=15)
    if ev is None:
        raise Exception("Roaming with the AP timed out")
    if "CTRL-EVENT-EAP-STARTED" in ev:
        raise Exception("Unexpected EAP exchange")
    pmksa1b = wpas.get_pmksa(bssid)
    if pmksa1b is None:
        raise Exception("No PMKSA cache entry found")
    if pmksa['pmkid'] != pmksa1b['pmkid']:
        raise Exception("Unexpected PMKID change for AP1")

    wpas.dump_monitor()
    if "FAIL" in wpas.request("PMKSA_FLUSH"):
        raise Exception("PMKSA_FLUSH failed")
    if wpas.get_pmksa(bssid) is not None or wpas.get_pmksa(bssid2) is not None:
        raise Exception("PMKSA_FLUSH did not remove PMKSA entries")
    wpas.wait_disconnected(timeout=5)
    wpas.wait_connected(timeout=15, error="Reconnection timed out")

def test_wext_wep_open_auth(dev, apdev):
    """WEP Open System authentication"""
    wpas = get_wext_interface()
    check_wep_capa(wpas)

    hapd = hostapd.add_ap(apdev[0],
                          {"ssid": "wep-open",
                           "wep_key0": '"hello"'})
    wpas.connect("wep-open", key_mgmt="NONE", wep_key0='"hello"',
                 scan_freq="2412")
    hwsim_utils.test_connectivity(wpas, hapd)
    if "[WEP]" not in wpas.request("SCAN_RESULTS"):
        raise Exception("WEP flag not indicated in scan results")

def test_wext_wep_shared_key_auth(dev, apdev):
    """WEP Shared Key authentication"""
    wpas = get_wext_interface()
    check_wep_capa(wpas)

    hapd = hostapd.add_ap(apdev[0],
                          {"ssid": "wep-shared-key",
                           "wep_key0": '"hello12345678"',
                           "auth_algs": "2"})
    wpas.connect("wep-shared-key", key_mgmt="NONE", auth_alg="SHARED",
                 wep_key0='"hello12345678"', scan_freq="2412")
    hwsim_utils.test_connectivity(wpas, hapd)
    wpas.request("REMOVE_NETWORK all")
    wpas.wait_disconnected(timeout=5)
    wpas.connect("wep-shared-key", key_mgmt="NONE", auth_alg="OPEN SHARED",
                 wep_key0='"hello12345678"', scan_freq="2412")

def test_wext_pmf(dev, apdev):
    """WEXT driver interface with WPA2-PSK and PMF"""
    wpas = get_wext_interface()

    params = hostapd.wpa2_params(ssid="wext-wpa2-psk", passphrase="12345678")
    params["wpa_key_mgmt"] = "WPA-PSK-SHA256"
    params["ieee80211w"] = "2"
    hapd = hostapd.add_ap(apdev[0], params)

    wpas.connect("wext-wpa2-psk", psk="12345678", ieee80211w="1",
                 key_mgmt="WPA-PSK WPA-PSK-SHA256", proto="WPA2",
                 scan_freq="2412")
    hwsim_utils.test_connectivity(wpas, hapd)

    addr = wpas.p2p_interface_addr()
    hapd.request("DEAUTHENTICATE " + addr)
    wpas.wait_disconnected(timeout=5)

def test_wext_scan_hidden(dev, apdev):
    """WEXT with hidden SSID"""
    wpas = get_wext_interface()

    hapd = hostapd.add_ap(apdev[0], {"ssid": "test-scan",
                                     "ignore_broadcast_ssid": "1"})
    hapd2 = hostapd.add_ap(apdev[1], {"ssid": "test-scan2",
                                      "ignore_broadcast_ssid": "1"})

    id1 = wpas.connect("test-scan", key_mgmt="NONE", scan_ssid="1",
                       only_add_network=True)

    wpas.request("SCAN scan_id=%d" % id1)

    ev = wpas.wait_event(["CTRL-EVENT-SCAN-RESULTS"], timeout=15)
    if ev is None:
        raise Exception("Scan did not complete")

    if "test-scan" not in wpas.request("SCAN_RESULTS"):
        raise Exception("Did not find hidden SSID in scan")

    id = wpas.connect("test-scan2", key_mgmt="NONE", scan_ssid="1",
                      only_add_network=True)
    wpas.connect_network(id, timeout=30)
    wpas.request("DISCONNECT")
    hapd2.disable()
    hapd.disable()
    wpas.interface_remove("wlan5")
    wpas.interface_add("wlan5")
    wpas.flush_scan_cache(freq=2412)
    wpas.flush_scan_cache()

def test_wext_rfkill(dev, apdev):
    """WEXT and rfkill block/unblock"""
    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    wpas.interface_add("wlan5")
    rfk = get_rfkill(wpas)
    wpas.interface_remove("wlan5")

    wpas = get_wext_interface()

    hapd = hostapd.add_ap(apdev[0], {"ssid": "open"})
    wpas.connect("open", key_mgmt="NONE", scan_freq="2412")
    try:
        logger.info("rfkill block")
        rfk.block()
        wpas.wait_disconnected(timeout=10,
                               error="Missing disconnection event on rfkill block")

        logger.info("rfkill unblock")
        rfk.unblock()
        wpas.wait_connected(timeout=20,
                            error="Missing connection event on rfkill unblock")
        hwsim_utils.test_connectivity(wpas, hapd)
    finally:
        rfk.unblock()
