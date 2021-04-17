# Roaming tests
# Copyright (c) 2013-2021, Jouni Malinen <j@w1.fi>
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

from remotehost import remote_compatible
import time
import logging
logger = logging.getLogger()

import hwsim_utils
import hostapd
from wpasupplicant import WpaSupplicant

@remote_compatible
def test_ap_roam_open(dev, apdev):
    """Roam between two open APs"""
    hapd0 = hostapd.add_ap(apdev[0], {"ssid": "test-open"})
    dev[0].connect("test-open", key_mgmt="NONE")
    hwsim_utils.test_connectivity(dev[0], hapd0)
    hapd1 = hostapd.add_ap(apdev[1], {"ssid": "test-open"})
    dev[0].scan(type="ONLY")
    dev[0].roam(apdev[1]['bssid'])
    hwsim_utils.test_connectivity(dev[0], hapd1)
    dev[0].roam(apdev[0]['bssid'])
    hwsim_utils.test_connectivity(dev[0], hapd0)

def test_ap_ignore_bssid_all(dev, apdev, params):
    """Ensure we clear the ignore BSSID list if all visible APs reject"""
    hapd0 = hostapd.add_ap(apdev[0], {"ssid": "test-open", "max_num_sta": "0"})
    hapd1 = hostapd.add_ap(apdev[1], {"ssid": "test-open", "max_num_sta": "0"})
    bss0 = hapd0.own_addr()
    bss1 = hapd1.own_addr()

    dev[0].connect("test-open", key_mgmt="NONE", scan_freq="2412",
                   wait_connect=False, bssid=bss0)
    if not dev[0].wait_event(["CTRL-EVENT-AUTH-REJECT"], timeout=10):
        raise Exception("AP 0 didn't reject us")
    dev[0].request("REMOVE_NETWORK all")
    dev[0].dump_monitor()
    dev[0].connect("test-open", key_mgmt="NONE", scan_freq="2412",
                   wait_connect=False, bssid=bss1)
    if not dev[0].wait_event(["CTRL-EVENT-AUTH-REJECT"], timeout=10):
        raise Exception("AP 1 didn't reject us")
    ignore_list = get_bssid_ignore_list(dev[0])
    logger.info("ignore list: " + str(ignore_list))
    dev[0].request("REMOVE_NETWORK all")
    dev[0].dump_monitor()

    hapd0.set("max_num_sta", "1")
    # All visible APs were ignored; we should clear the ignore list and find
    # the AP that now accepts us.
    dev[0].scan_for_bss(bss0, freq=2412)
    dev[0].connect("test-open", key_mgmt="NONE", scan_freq="2412", bssid=bss0)

@remote_compatible
def test_ap_roam_open_failed(dev, apdev):
    """Roam failure due to rejected authentication"""
    hapd0 = hostapd.add_ap(apdev[0], {"ssid": "test-open"})
    dev[0].connect("test-open", key_mgmt="NONE", scan_freq="2412")
    hwsim_utils.test_connectivity(dev[0], hapd0)
    params = {"ssid": "test-open", "max_num_sta": "0"}
    hapd1 = hostapd.add_ap(apdev[1], params)
    bssid = hapd1.own_addr()

    dev[0].scan_for_bss(bssid, freq=2412)
    dev[0].dump_monitor()
    if "OK" not in dev[0].request("ROAM " + bssid):
        raise Exception("ROAM failed")

    ev = dev[0].wait_event(["CTRL-EVENT-AUTH-REJECT"], 1)
    if not ev:
        raise Exception("CTRL-EVENT-AUTH-REJECT was not seen")

    dev[0].wait_connected(timeout=5)
    hwsim_utils.test_connectivity(dev[0], hapd0)

def test_ap_roam_open_failed_ssid_mismatch(dev, apdev):
    """Roam failure due to SSID mismatch"""
    hapd0 = hostapd.add_ap(apdev[0], {"ssid": "test-open"})
    bssid0 = hapd0.own_addr()
    hapd1 = hostapd.add_ap(apdev[1], {"ssid": "test-open2"})
    bssid1 = hapd1.own_addr()
    dev[0].flush_scan_cache()
    dev[0].scan_for_bss(bssid0, freq=2412)
    dev[0].scan_for_bss(bssid1, freq=2412)
    dev[0].connect("test-open", key_mgmt="NONE", scan_freq="2412")
    hapd0.wait_sta()
    bssid = dev[0].get_status_field("bssid")
    if bssid != bssid0:
        raise Exception("Unexpected BSSID reported after initial connection: " + bssid)
    if "FAIL" not in dev[0].request("ROAM " + bssid1):
        raise Exception("ROAM succeed unexpectedly")
    bssid = dev[0].get_status_field("bssid")
    if bssid != bssid0:
        raise Exception("Unexpected BSSID reported after failed roam attempt: " + bssid)
    hwsim_utils.test_connectivity(dev[0], hapd0)

@remote_compatible
def test_ap_roam_wpa2_psk(dev, apdev):
    """Roam between two WPA2-PSK APs"""
    params = hostapd.wpa2_params(ssid="test-wpa2-psk", passphrase="12345678")
    hapd0 = hostapd.add_ap(apdev[0], params)
    dev[0].connect("test-wpa2-psk", psk="12345678")
    hapd0.wait_sta()
    hwsim_utils.test_connectivity(dev[0], hapd0)
    hapd1 = hostapd.add_ap(apdev[1], params)
    dev[0].scan(type="ONLY")
    dev[0].roam(apdev[1]['bssid'])
    hapd1.wait_sta()
    hwsim_utils.test_connectivity(dev[0], hapd1)
    dev[0].roam(apdev[0]['bssid'])
    hapd0.wait_sta()
    hwsim_utils.test_connectivity(dev[0], hapd0)

def test_ap_roam_wpa2_psk_pmf_mismatch(dev, apdev):
    """Roam between two WPA2-PSK APs - PMF mismatch"""
    params = hostapd.wpa2_params(ssid="test-wpa2-psk", passphrase="12345678")
    params['ieee80211w'] = '1'
    hapd0 = hostapd.add_ap(apdev[0], params)
    bssid0 = hapd0.own_addr()
    params['ieee80211w'] = '0'
    hapd1 = hostapd.add_ap(apdev[1], params)
    bssid1 = hapd1.own_addr()
    dev[0].flush_scan_cache()
    dev[0].scan_for_bss(bssid0, freq=2412)
    dev[0].scan_for_bss(bssid1, freq=2412)
    dev[0].connect("test-wpa2-psk", psk="12345678", ieee80211w='2')
    hapd0.wait_sta()
    bssid = dev[0].get_status_field("bssid")
    if bssid != bssid0:
        raise Exception("Unexpected BSSID reported after initial connection: " + bssid)
    if "OK" not in dev[0].request("ROAM " + apdev[1]['bssid']):
        raise Exception("ROAM failed")
    ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED"], timeout=0.5)
    if ev is not None:
        raise Exception("Unexpected connection reported")
    bssid = dev[0].get_status_field("bssid")
    if bssid != bssid0:
        raise Exception("Unexpected BSSID reported after failed roam attempt: " + bssid)
    hwsim_utils.test_connectivity(dev[0], hapd0)

def get_bssid_ignore_list(dev):
    return dev.request("BSSID_IGNORE").splitlines()

def test_ap_reconnect_auth_timeout(dev, apdev, params):
    """Reconnect to 2nd AP and authentication times out"""
    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    wpas.interface_add("wlan5",
                       drv_params="force_connect_cmd=1,force_bss_selection=1")

    params = hostapd.wpa2_params(ssid="test-wpa2-psk", passphrase="12345678")
    hapd0 = hostapd.add_ap(apdev[0], params)
    bssid0 = hapd0.own_addr()

    wpas.scan_for_bss(bssid0, freq=2412)
    id = wpas.connect("test-wpa2-psk", psk="12345678", scan_freq="2412")
    hapd0.wait_sta()
    hwsim_utils.test_connectivity(wpas, hapd0)

    hapd1 = hostapd.add_ap(apdev[1], params)
    bssid1 = hapd1.own_addr()

    wpas.request("BSSID_IGNORE " + bssid0)

    wpas.scan_for_bss(bssid1, freq=2412)
    wpas.request("DISCONNECT")
    if "OK" not in wpas.request("SET ignore_auth_resp 1"):
        raise Exception("SET ignore_auth_resp failed")
    if "OK" not in wpas.request("ENABLE_NETWORK " + str(id)):
        raise Exception("ENABLE_NETWORK failed")
    if "OK" not in wpas.request("SELECT_NETWORK " + str(id)):
        raise Exception("SELECT_NETWORK failed")

    logger.info("Wait ~10s for auth timeout...")
    time.sleep(10)
    ev = wpas.wait_event(["CTRL-EVENT-SCAN-STARTED"], 12)
    if not ev:
        raise Exception("CTRL-EVENT-SCAN-STARTED not seen")

    b = get_bssid_ignore_list(wpas)
    if '00:00:00:00:00:00' in b:
        raise Exception("Unexpected ignore list contents: " + str(b))
    if bssid1 not in b:
        raise Exception("Unexpected ignore list contents: " + str(b))

def test_ap_roam_with_reassoc_auth_timeout(dev, apdev, params):
    """Roam using reassoc between two APs and authentication times out"""
    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    wpas.interface_add("wlan5",
                       drv_params="force_connect_cmd=1,force_bss_selection=1")

    params = hostapd.wpa2_params(ssid="test-wpa2-psk", passphrase="12345678")
    hapd0 = hostapd.add_ap(apdev[0], params)
    bssid0 = hapd0.own_addr()

    id = wpas.connect("test-wpa2-psk", psk="12345678", scan_freq="2412")
    hapd0.wait_sta()
    hwsim_utils.test_connectivity(wpas, hapd0)

    hapd1 = hostapd.add_ap(apdev[1], params)
    bssid1 = hapd1.own_addr()
    wpas.scan_for_bss(bssid1, freq=2412)

    if "OK" not in wpas.request("SET_NETWORK " + str(id) + " bssid " + bssid1):
        raise Exception("SET_NETWORK failed")
    if "OK" not in wpas.request("SET ignore_auth_resp 1"):
        raise Exception("SET ignore_auth_resp failed")
    if "OK" not in wpas.request("REASSOCIATE"):
        raise Exception("REASSOCIATE failed")

    logger.info("Wait ~10s for auth timeout...")
    time.sleep(10)
    ev = wpas.wait_event(["CTRL-EVENT-SCAN-STARTED"], 12)
    if not ev:
        raise Exception("CTRL-EVENT-SCAN-STARTED not seen")

    b = get_bssid_ignore_list(wpas)
    if bssid0 in b:
        raise Exception("Unexpected ignore list contents: " + str(b))

def test_ap_roam_wpa2_psk_failed(dev, apdev, params):
    """Roam failure with WPA2-PSK AP due to wrong passphrase"""
    params = hostapd.wpa2_params(ssid="test-wpa2-psk", passphrase="12345678")
    hapd0 = hostapd.add_ap(apdev[0], params)
    id = dev[0].connect("test-wpa2-psk", psk="12345678", scan_freq="2412")
    hapd0.wait_sta()
    hwsim_utils.test_connectivity(dev[0], hapd0)
    params['wpa_passphrase'] = "22345678"
    hapd1 = hostapd.add_ap(apdev[1], params)
    bssid = hapd1.own_addr()
    dev[0].scan_for_bss(bssid, freq=2412)

    dev[0].dump_monitor()
    if "OK" not in dev[0].request("ROAM " + bssid):
        raise Exception("ROAM failed")

    ev = dev[0].wait_event(["CTRL-EVENT-SSID-TEMP-DISABLED",
                            "CTRL-EVENT-CONNECTED"], 5)
    if "CTRL-EVENT-CONNECTED" in ev:
        raise Exception("Got unexpected CTRL-EVENT-CONNECTED")
    if "CTRL-EVENT-SSID-TEMP-DISABLED" not in ev:
        raise Exception("CTRL-EVENT-SSID-TEMP-DISABLED not seen")

    if "OK" not in dev[0].request("SELECT_NETWORK id=" + str(id)):
        raise Exception("SELECT_NETWORK failed")

    ev = dev[0].wait_event(["CTRL-EVENT-SSID-REENABLED"], 3)
    if not ev:
        raise Exception("CTRL-EVENT-SSID-REENABLED not seen")

    dev[0].wait_connected(timeout=5)
    hapd0.wait_sta()
    hwsim_utils.test_connectivity(dev[0], hapd0)

@remote_compatible
def test_ap_reassociation_to_same_bss(dev, apdev):
    """Reassociate to the same BSS"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": "test-open"})
    dev[0].connect("test-open", key_mgmt="NONE")
    hapd.wait_sta()

    dev[0].request("REASSOCIATE")
    dev[0].wait_connected(timeout=10, error="Reassociation timed out")
    hapd.wait_sta()
    hwsim_utils.test_connectivity(dev[0], hapd)

    dev[0].request("REATTACH")
    dev[0].wait_connected(timeout=10, error="Reattach timed out")
    hapd.wait_sta()
    hwsim_utils.test_connectivity(dev[0], hapd)

    # Wait for previous scan results to expire to trigger new scan
    time.sleep(5)
    dev[0].request("REATTACH")
    dev[0].wait_connected(timeout=10, error="Reattach timed out")
    hapd.wait_sta()
    hwsim_utils.test_connectivity(dev[0], hapd)

@remote_compatible
def test_ap_roam_set_bssid(dev, apdev):
    """Roam control"""
    hostapd.add_ap(apdev[0], {"ssid": "test-open"})
    hostapd.add_ap(apdev[1], {"ssid": "test-open"})
    id = dev[0].connect("test-open", key_mgmt="NONE", bssid=apdev[1]['bssid'],
                        scan_freq="2412")
    if dev[0].get_status_field('bssid') != apdev[1]['bssid']:
        raise Exception("Unexpected BSS")
    # for now, these are just verifying that the code path to indicate
    # within-ESS roaming changes can be executed; the actual results of those
    # operations are not currently verified (that would require a test driver
    # that does BSS selection)
    dev[0].set_network(id, "bssid", "")
    dev[0].set_network(id, "bssid", apdev[0]['bssid'])
    dev[0].set_network(id, "bssid", apdev[1]['bssid'])

@remote_compatible
def test_ap_roam_wpa2_psk_race(dev, apdev):
    """Roam between two WPA2-PSK APs and try to hit a disconnection race"""
    params = hostapd.wpa2_params(ssid="test-wpa2-psk", passphrase="12345678")
    hapd0 = hostapd.add_ap(apdev[0], params)
    dev[0].connect("test-wpa2-psk", psk="12345678", scan_freq="2412")
    hapd0.wait_sta()
    hwsim_utils.test_connectivity(dev[0], hapd0)

    params['channel'] = '2'
    hapd1 = hostapd.add_ap(apdev[1], params)
    dev[0].scan_for_bss(apdev[1]['bssid'], freq=2417)
    dev[0].roam(apdev[1]['bssid'])
    hapd1.wait_sta()
    hwsim_utils.test_connectivity(dev[0], hapd1)
    dev[0].roam(apdev[0]['bssid'])
    hapd0.wait_sta()
    hwsim_utils.test_connectivity(dev[0], hapd0)
    # Wait at least two seconds to trigger the previous issue with the
    # disconnection callback.
    for i in range(3):
        time.sleep(0.8)
        hwsim_utils.test_connectivity(dev[0], hapd0)

def test_ap_roam_signal_level_override(dev, apdev):
    """Roam between two APs based on driver signal level override"""
    hapd0 = hostapd.add_ap(apdev[0], {"ssid": "test-open"})
    bssid0 = apdev[0]['bssid']
    hapd1 = hostapd.add_ap(apdev[1], {"ssid": "test-open"})
    bssid1 = apdev[1]['bssid']
    dev[0].scan_for_bss(bssid0, freq=2412)
    dev[0].scan_for_bss(bssid1, freq=2412)

    dev[0].connect("test-open", key_mgmt="NONE")
    bssid = dev[0].get_status_field('bssid')
    if bssid == bssid0:
        dst = bssid1
        src = bssid0
    else:
        dst = bssid0
        src = bssid1

    dev[0].scan(freq=2412)
    ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED"], 0.5)
    if ev is not None:
        raise Exception("Unexpected roam")

    orig_res = dev[0].request("SIGNAL_POLL")
    dev[0].set("driver_signal_override", src + " -1 -2 -3 -4 -5")
    res = dev[0].request("SIGNAL_POLL").splitlines()
    if "RSSI=-1" not in res or \
       "AVG_RSSI=-2" not in res or \
       "AVG_BEACON_RSSI=-3" not in res or \
       "NOISE=-4" not in res:
        raise Exception("SIGNAL_POLL override did not work: " + str(res))

    dev[0].set("driver_signal_override", src)
    new_res = dev[0].request("SIGNAL_POLL")
    if orig_res != new_res:
        raise Exception("SIGNAL_POLL restore did not work: " + new_res)

    tests = [("-30 -30 -30 -95 -30", "-30 -30 -30 -95 -30"),
             ("-30 -30 -30 -95 -30", "-20 -20 -20 -95 -20"),
             ("-90 -90 -90 -95 -90", "-89 -89 -89 -95 -89"),
             ("-90 -90 -90 -95 -95", "-89 -89 -89 -95 -89")]
    for src_override, dst_override in tests:
        dev[0].set("driver_signal_override", src + " " + src_override)
        dev[0].set("driver_signal_override", dst + " " + dst_override)
        dev[0].scan(freq=2412)
        ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED"], 0.1)
        if ev is not None:
            raise Exception("Unexpected roam")
        dev[0].dump_monitor()

    dev[0].set("driver_signal_override", src + " -90 -90 -90 -95 -90")
    dev[0].set("driver_signal_override", dst + " -80 -80 -80 -95 -80")
    dev[0].scan(freq=2412)
    dev[0].wait_connected()
    if dst != dev[0].get_status_field('bssid'):
        raise Exception("Unexpected AP after roam")
    dev[0].dump_monitor()

def test_ap_roam_during_scan(dev, apdev):
    """Roam command during a scan operation"""
    hapd0 = hostapd.add_ap(apdev[0], {"ssid": "test-open"})
    dev[0].scan_for_bss(hapd0.own_addr(), freq=2412)
    dev[0].connect("test-open", key_mgmt="NONE")
    hapd1 = hostapd.add_ap(apdev[1], {"ssid": "test-open"})
    dev[0].scan_for_bss(hapd1.own_addr(), freq=2412)
    if "OK" not in dev[0].request("SCAN"):
        raise Exception("Failed to start scan")
    if "OK" not in dev[0].request("ROAM " + hapd1.own_addr()):
        raise Exception("Failed to issue ROAM")
    ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED"], timeout=10)
    if ev is None:
        raise Exception("Connection not reported after ROAM")
    if hapd1.own_addr() not in ev:
        raise Exception("Connected to unexpected AP")
