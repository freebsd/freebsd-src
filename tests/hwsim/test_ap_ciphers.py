# Cipher suite tests
# Copyright (c) 2013-2015, Jouni Malinen <j@w1.fi>
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

from remotehost import remote_compatible
import time
import logging
logger = logging.getLogger()
import os
import subprocess

import hwsim_utils
import hostapd
from utils import *
from wlantest import Wlantest
from wpasupplicant import WpaSupplicant

KT_PTK, KT_GTK, KT_IGTK, KT_BIGTK = range(4)

def check_cipher(dev, ap, cipher, group_cipher=None):
    if cipher not in dev.get_capability("pairwise"):
        raise HwsimSkip("Cipher %s not supported" % cipher)
    if group_cipher and group_cipher not in dev.get_capability("group"):
        raise HwsimSkip("Cipher %s not supported" % group_cipher)
    params = {"ssid": "test-wpa2-psk",
              "wpa_passphrase": "12345678",
              "wpa": "2",
              "wpa_key_mgmt": "WPA-PSK",
              "rsn_pairwise": cipher}
    if group_cipher:
        params["group_cipher"] = group_cipher
    else:
        group_cipher = cipher
    hapd = hostapd.add_ap(ap, params)
    dev.connect("test-wpa2-psk", psk="12345678",
                pairwise=cipher, group=group_cipher, scan_freq="2412")
    hapd.wait_sta()
    hwsim_utils.test_connectivity(dev, hapd)

def check_group_mgmt_cipher(dev, ap, cipher, sta_req_cipher=None):
    if cipher not in dev.get_capability("group_mgmt"):
        raise HwsimSkip("Cipher %s not supported" % cipher)
    params = {"ssid": "test-wpa2-psk-pmf",
              "wpa_passphrase": "12345678",
              "wpa": "2",
              "ieee80211w": "2",
              "wpa_key_mgmt": "WPA-PSK-SHA256",
              "rsn_pairwise": "CCMP",
              "group_mgmt_cipher": cipher}
    hapd = hostapd.add_ap(ap, params)

    Wlantest.setup(hapd)
    wt = Wlantest()
    wt.flush()
    wt.add_passphrase("12345678")

    dev.connect("test-wpa2-psk-pmf", psk="12345678", ieee80211w="2",
                key_mgmt="WPA-PSK-SHA256", group_mgmt=sta_req_cipher,
                pairwise="CCMP", group="CCMP", scan_freq="2412")
    hapd.wait_sta()
    hwsim_utils.test_connectivity(dev, hapd)
    hapd.request("DEAUTHENTICATE ff:ff:ff:ff:ff:ff")
    dev.wait_disconnected()
    if wt.get_bss_counter('valid_bip_mmie', ap['bssid']) < 1:
        raise Exception("No valid BIP MMIE seen")
    if wt.get_bss_counter('bip_deauth', ap['bssid']) < 1:
        raise Exception("No valid BIP deauth seen")

    if cipher == "AES-128-CMAC":
        group_mgmt = "BIP"
    else:
        group_mgmt = cipher
    res = wt.info_bss('group_mgmt', ap['bssid']).strip()
    if res != group_mgmt:
        raise Exception("Unexpected group mgmt cipher: " + res)

@remote_compatible
def test_ap_cipher_tkip(dev, apdev):
    """WPA2-PSK/TKIP connection"""
    skip_with_fips(dev[0])
    skip_without_tkip(dev[0])
    check_cipher(dev[0], apdev[0], "TKIP")

@remote_compatible
def test_ap_cipher_tkip_countermeasures_ap(dev, apdev):
    """WPA-PSK/TKIP countermeasures (detected by AP)"""
    skip_with_fips(dev[0])
    skip_without_tkip(dev[0])
    testfile = "/sys/kernel/debug/ieee80211/%s/netdev:%s/tkip_mic_test" % (dev[0].get_driver_status_field("phyname"), dev[0].ifname)
    if dev[0].cmd_execute(["ls", testfile])[0] != 0:
        raise HwsimSkip("tkip_mic_test not supported in mac80211")

    params = {"ssid": "tkip-countermeasures",
              "wpa_passphrase": "12345678",
              "wpa": "1",
              "wpa_key_mgmt": "WPA-PSK",
              "wpa_pairwise": "TKIP"}
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].connect("tkip-countermeasures", psk="12345678",
                   pairwise="TKIP", group="TKIP", scan_freq="2412")

    dev[0].dump_monitor()
    dev[0].cmd_execute(["echo", "-n", apdev[0]['bssid'], ">", testfile],
                       shell=True)
    ev = dev[0].wait_event(["CTRL-EVENT-DISCONNECTED"], timeout=1)
    if ev is not None:
        raise Exception("Unexpected disconnection on first Michael MIC failure")

    dev[0].cmd_execute(["echo", "-n", "ff:ff:ff:ff:ff:ff", ">", testfile],
                       shell=True)
    ev = dev[0].wait_disconnected(timeout=10,
                                  error="No disconnection after two Michael MIC failures")
    if "reason=14" not in ev:
        raise Exception("Unexpected disconnection reason: " + ev)
    ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED"], timeout=1)
    if ev is not None:
        raise Exception("Unexpected connection during TKIP countermeasures")

def test_ap_cipher_tkip_countermeasures_ap_mixed_mode(dev, apdev):
    """WPA+WPA2-PSK/TKIP countermeasures (detected by mixed mode AP)"""
    skip_with_fips(dev[0])
    skip_without_tkip(dev[0])
    testfile = "/sys/kernel/debug/ieee80211/%s/netdev:%s/tkip_mic_test" % (dev[0].get_driver_status_field("phyname"), dev[0].ifname)
    if dev[0].cmd_execute(["ls", testfile])[0] != 0:
        raise HwsimSkip("tkip_mic_test not supported in mac80211")

    params = {"ssid": "tkip-countermeasures",
              "wpa_passphrase": "12345678",
              "wpa": "3",
              "wpa_key_mgmt": "WPA-PSK",
              "wpa_pairwise": "TKIP",
              "rsn_pairwise": "CCMP"}
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].connect("tkip-countermeasures", psk="12345678",
                   pairwise="TKIP", group="TKIP", scan_freq="2412")
    dev[1].connect("tkip-countermeasures", psk="12345678",
                   pairwise="CCMP", scan_freq="2412")

    dev[0].dump_monitor()
    dev[0].cmd_execute(["echo", "-n", apdev[0]['bssid'], ">", testfile],
                       shell=True)
    ev = dev[0].wait_event(["CTRL-EVENT-DISCONNECTED"], timeout=1)
    if ev is not None:
        raise Exception("Unexpected disconnection on first Michael MIC failure")

    dev[0].cmd_execute(["echo", "-n", "ff:ff:ff:ff:ff:ff", ">", testfile],
                       shell=True)

    ev = dev[0].wait_disconnected(timeout=10,
                                  error="No disconnection after two Michael MIC failures")
    if "reason=14" not in ev:
        raise Exception("Unexpected disconnection reason: " + ev)

    ev = dev[1].wait_disconnected(timeout=10,
                                  error="No disconnection after two Michael MIC failures (2)")
    if "reason=14" not in ev:
        raise Exception("Unexpected disconnection reason (2): " + ev)

    ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED"], timeout=1)
    if ev is not None:
        raise Exception("Unexpected connection during TKIP countermeasures (1)")
    ev = dev[1].wait_event(["CTRL-EVENT-CONNECTED"], timeout=1)
    if ev is not None:
        raise Exception("Unexpected connection during TKIP countermeasures (2)")

@remote_compatible
def test_ap_cipher_tkip_countermeasures_sta(dev, apdev):
    """WPA-PSK/TKIP countermeasures (detected by STA)"""
    skip_with_fips(dev[0])
    skip_without_tkip(dev[0])
    params = {"ssid": "tkip-countermeasures",
              "wpa_passphrase": "12345678",
              "wpa": "1",
              "wpa_key_mgmt": "WPA-PSK",
              "wpa_pairwise": "TKIP"}
    hapd = hostapd.add_ap(apdev[0], params)

    testfile = "/sys/kernel/debug/ieee80211/%s/netdev:%s/tkip_mic_test" % (hapd.get_driver_status_field("phyname"), apdev[0]['ifname'])
    if hapd.cmd_execute(["ls", testfile])[0] != 0:
        raise HwsimSkip("tkip_mic_test not supported in mac80211")

    dev[0].connect("tkip-countermeasures", psk="12345678",
                   pairwise="TKIP", group="TKIP", scan_freq="2412")

    dev[0].dump_monitor()
    hapd.cmd_execute(["echo", "-n", dev[0].own_addr(), ">", testfile],
                     shell=True)
    ev = dev[0].wait_event(["CTRL-EVENT-DISCONNECTED"], timeout=1)
    if ev is not None:
        raise Exception("Unexpected disconnection on first Michael MIC failure")

    hapd.cmd_execute(["echo", "-n", "ff:ff:ff:ff:ff:ff", ">", testfile],
                     shell=True)
    ev = dev[0].wait_disconnected(timeout=10,
                                  error="No disconnection after two Michael MIC failures")
    if "reason=14 locally_generated=1" not in ev:
        raise Exception("Unexpected disconnection reason: " + ev)
    ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED"], timeout=1)
    if ev is not None:
        raise Exception("Unexpected connection during TKIP countermeasures")

@long_duration_test
def test_ap_cipher_tkip_countermeasures_sta2(dev, apdev):
    """WPA-PSK/TKIP countermeasures (detected by two STAs)"""
    skip_with_fips(dev[0])
    skip_without_tkip(dev[0])
    params = {"ssid": "tkip-countermeasures",
              "wpa_passphrase": "12345678",
              "wpa": "1",
              "wpa_key_mgmt": "WPA-PSK",
              "wpa_pairwise": "TKIP"}
    hapd = hostapd.add_ap(apdev[0], params)

    testfile = "/sys/kernel/debug/ieee80211/%s/netdev:%s/tkip_mic_test" % (hapd.get_driver_status_field("phyname"), apdev[0]['ifname'])
    if hapd.cmd_execute(["ls", testfile])[0] != 0:
        raise HwsimSkip("tkip_mic_test not supported in mac80211")

    dev[0].connect("tkip-countermeasures", psk="12345678",
                   pairwise="TKIP", group="TKIP", scan_freq="2412")
    dev[0].dump_monitor()
    id = dev[1].connect("tkip-countermeasures", psk="12345678",
                        pairwise="TKIP", group="TKIP", scan_freq="2412")
    dev[1].dump_monitor()

    hapd.cmd_execute(["echo", "-n", "ff:ff:ff:ff:ff:ff", ">", testfile],
                     shell=True)
    ev = dev[0].wait_disconnected(timeout=10,
                                  error="No disconnection after two Michael MIC failure")
    if "reason=14" not in ev:
        raise Exception("Unexpected disconnection reason: " + ev)
    ev = dev[1].wait_disconnected(timeout=5,
                                  error="No disconnection after two Michael MIC failure")
    if "reason=14" not in ev:
        raise Exception("Unexpected disconnection reason: " + ev)
    ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED"], timeout=1)
    if ev is not None:
        raise Exception("Unexpected connection during TKIP countermeasures")
    ev = dev[1].wait_event(["CTRL-EVENT-CONNECTED"], timeout=1)
    if ev is not None:
        raise Exception("Unexpected connection during TKIP countermeasures")

    dev[0].request("REMOVE_NETWORK all")
    logger.info("Waiting for TKIP countermeasures to end")
    connected = False
    start = os.times()[4]
    while True:
        now = os.times()[4]
        if start + 70 < now:
            break
        dev[0].connect("tkip-countermeasures", psk="12345678",
                       pairwise="TKIP", group="TKIP", scan_freq="2412",
                       wait_connect=False)
        ev = dev[0].wait_event(["CTRL-EVENT-AUTH-REJECT",
                                "CTRL-EVENT-CONNECTED"], timeout=10)
        if ev is None:
            raise Exception("No connection result")
        if "CTRL-EVENT-CONNECTED" in ev:
            connected = True
            break
        if "status_code=1" not in ev:
            raise Exception("Unexpected connection failure reason during TKIP countermeasures: " + ev)
        dev[0].request("REMOVE_NETWORK all")
        time.sleep(1)
        dev[0].dump_monitor()
        dev[1].dump_monitor()
    if not connected:
        raise Exception("No connection after TKIP countermeasures terminated")

    ev = dev[1].wait_event(["CTRL-EVENT-CONNECTED"], timeout=1)
    if ev is None:
        dev[1].request("DISCONNECT")
        dev[1].select_network(id)
        dev[1].wait_connected()

@remote_compatible
def test_ap_cipher_ccmp(dev, apdev):
    """WPA2-PSK/CCMP connection"""
    check_cipher(dev[0], apdev[0], "CCMP")

def test_ap_cipher_gcmp(dev, apdev):
    """WPA2-PSK/GCMP connection"""
    check_cipher(dev[0], apdev[0], "GCMP")

def test_ap_cipher_ccmp_256(dev, apdev):
    """WPA2-PSK/CCMP-256 connection"""
    check_cipher(dev[0], apdev[0], "CCMP-256")

def test_ap_cipher_gcmp_256(dev, apdev):
    """WPA2-PSK/GCMP-256 connection"""
    check_cipher(dev[0], apdev[0], "GCMP-256")

def test_ap_cipher_gcmp_256_group_gcmp_256(dev, apdev):
    """WPA2-PSK/GCMP-256 connection with group cipher override GCMP-256"""
    check_cipher(dev[0], apdev[0], "GCMP-256", "GCMP-256")

def test_ap_cipher_gcmp_256_group_gcmp(dev, apdev):
    """WPA2-PSK/GCMP-256 connection with group cipher override GCMP"""
    check_cipher(dev[0], apdev[0], "GCMP-256", "GCMP")

def test_ap_cipher_gcmp_256_group_ccmp_256(dev, apdev):
    """WPA2-PSK/GCMP-256 connection with group cipher override CCMP-256"""
    check_cipher(dev[0], apdev[0], "GCMP-256", "CCMP-256")

def test_ap_cipher_gcmp_256_group_ccmp(dev, apdev):
    """WPA2-PSK/GCMP-256 connection with group cipher override CCMP"""
    check_cipher(dev[0], apdev[0], "GCMP-256", "CCMP")

def test_ap_cipher_gcmp_ccmp(dev, apdev, params):
    """WPA2-PSK/GCMP/CCMP ciphers"""
    config = os.path.join(params['logdir'], 'ap_cipher_gcmp_ccmp.conf')

    for cipher in ["CCMP", "GCMP", "CCMP-256", "GCMP-256"]:
        if cipher not in dev[0].get_capability("pairwise"):
            raise HwsimSkip("Cipher %s not supported" % cipher)
        if cipher not in dev[0].get_capability("group"):
            raise HwsimSkip("Group cipher %s not supported" % cipher)

    params = {"ssid": "test-wpa2-psk",
              "wpa_passphrase": "12345678",
              "wpa": "2",
              "wpa_key_mgmt": "WPA-PSK",
              "rsn_pairwise": "CCMP GCMP CCMP-256 GCMP-256"}
    hapd = hostapd.add_ap(apdev[0], params)


    for cipher in ["CCMP", "GCMP", "CCMP-256", "GCMP-256"]:
        dev[0].connect("test-wpa2-psk", psk="12345678",
                       pairwise=cipher, group="CCMP", scan_freq="2412")
        if dev[0].get_status_field("group_cipher") != "CCMP":
            raise Exception("Unexpected group_cipher")
        if dev[0].get_status_field("pairwise_cipher") != cipher:
            raise Exception("Unexpected pairwise_cipher")
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()

    dev[0].connect("test-wpa2-psk", psk="12345678",
                   pairwise="CCMP CCMP-256 GCMP GCMP-256",
                   group="CCMP CCMP-256 GCMP GCMP-256", scan_freq="2412")
    if dev[0].get_status_field("group_cipher") != "CCMP":
        raise Exception("Unexpected group_cipher")
    res = dev[0].get_status_field("pairwise_cipher")
    if res != "CCMP-256" and res != "GCMP-256":
        raise Exception("Unexpected pairwise_cipher")

    try:
        with open(config, "w") as f:
            f.write("network={\n" +
                    "\tssid=\"test-wpa2-psk\"\n" +
                    "\tkey_mgmt=WPA-PSK\n" +
                    "\tpsk=\"12345678\"\n" +
                    "\tpairwise=GCMP\n" +
                    "\tgroup=CCMP\n" +
                    "\tscan_freq=2412\n" +
                    "}\n")

        wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
        wpas.interface_add("wlan5", config=config)
        wpas.wait_connected()
        if wpas.get_status_field("group_cipher") != "CCMP":
            raise Exception("Unexpected group_cipher")
        if wpas.get_status_field("pairwise_cipher") != "GCMP":
            raise Exception("Unexpected pairwise_cipher")
    finally:
        os.remove(config)

@remote_compatible
def test_ap_cipher_mixed_wpa_wpa2(dev, apdev):
    """WPA2-PSK/CCMP/ and WPA-PSK/TKIP mixed configuration"""
    skip_with_fips(dev[0])
    skip_without_tkip(dev[0])
    ssid = "test-wpa-wpa2-psk"
    passphrase = "12345678"
    params = {"ssid": ssid,
              "wpa_passphrase": passphrase,
              "wpa": "3",
              "wpa_key_mgmt": "WPA-PSK",
              "rsn_pairwise": "CCMP",
              "wpa_pairwise": "TKIP"}
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].flush_scan_cache()
    dev[0].connect(ssid, psk=passphrase, proto="WPA2",
                   pairwise="CCMP", group="TKIP", scan_freq="2412")
    status = dev[0].get_status()
    if status['key_mgmt'] != 'WPA2-PSK':
        raise Exception("Incorrect key_mgmt reported")
    if status['pairwise_cipher'] != 'CCMP':
        raise Exception("Incorrect pairwise_cipher reported")
    if status['group_cipher'] != 'TKIP':
        raise Exception("Incorrect group_cipher reported")
    bss = dev[0].get_bss(apdev[0]['bssid'])
    if bss['ssid'] != ssid:
        raise Exception("Unexpected SSID in the BSS entry")
    if "[WPA-PSK-TKIP]" not in bss['flags']:
        raise Exception("Missing BSS flag WPA-PSK-TKIP")
    if "[WPA2-PSK-CCMP]" not in bss['flags']:
        raise Exception("Missing BSS flag WPA2-PSK-CCMP")
    hapd.wait_sta()
    hwsim_utils.test_connectivity(dev[0], hapd)

    dev[1].connect(ssid, psk=passphrase, proto="WPA",
                   pairwise="TKIP", group="TKIP", scan_freq="2412")
    status = dev[1].get_status()
    if status['key_mgmt'] != 'WPA-PSK':
        raise Exception("Incorrect key_mgmt reported")
    if status['pairwise_cipher'] != 'TKIP':
        raise Exception("Incorrect pairwise_cipher reported")
    if status['group_cipher'] != 'TKIP':
        raise Exception("Incorrect group_cipher reported")
    hapd.wait_sta()
    hwsim_utils.test_connectivity(dev[1], hapd)
    hwsim_utils.test_connectivity(dev[0], dev[1])

@remote_compatible
def test_ap_cipher_wpa_sae(dev, apdev):
    """WPA-PSK/TKIP and SAE mixed AP - WPA IE and RSNXE coexistence"""
    skip_with_fips(dev[0])
    skip_without_tkip(dev[0])
    check_sae_capab(dev[0])
    ssid = "test-wpa-sae"
    passphrase = "12345678"
    params = {"ssid": ssid,
              "wpa_passphrase": passphrase,
              "wpa": "3",
              "wpa_key_mgmt": "WPA-PSK SAE",
              "rsn_pairwise": "CCMP",
              "wpa_pairwise": "TKIP",
              "sae_pwe": "1"}
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].flush_scan_cache()

    dev[0].connect(ssid, psk=passphrase, proto="WPA",
                   pairwise="TKIP", group="TKIP", scan_freq="2412")
    status = dev[0].get_status()
    if status['key_mgmt'] != 'WPA-PSK':
        raise Exception("Incorrect key_mgmt reported")
    if status['pairwise_cipher'] != 'TKIP':
        raise Exception("Incorrect pairwise_cipher reported")
    if status['group_cipher'] != 'TKIP':
        raise Exception("Incorrect group_cipher reported")
    hapd.wait_sta()
    hwsim_utils.test_connectivity(dev[0], hapd)

@remote_compatible
def test_ap_cipher_bip(dev, apdev):
    """WPA2-PSK with BIP"""
    check_group_mgmt_cipher(dev[0], apdev[0], "AES-128-CMAC")

def test_ap_cipher_bip_req(dev, apdev):
    """WPA2-PSK with BIP required"""
    check_group_mgmt_cipher(dev[0], apdev[0], "AES-128-CMAC", "AES-128-CMAC")

def test_ap_cipher_bip_req2(dev, apdev):
    """WPA2-PSK with BIP required (2)"""
    check_group_mgmt_cipher(dev[0], apdev[0], "AES-128-CMAC",
                            "AES-128-CMAC BIP-GMAC-128 BIP-GMAC-256 BIP-CMAC-256")

def test_ap_cipher_bip_gmac_128(dev, apdev):
    """WPA2-PSK with BIP-GMAC-128"""
    check_group_mgmt_cipher(dev[0], apdev[0], "BIP-GMAC-128")

def test_ap_cipher_bip_gmac_128_req(dev, apdev):
    """WPA2-PSK with BIP-GMAC-128 required"""
    check_group_mgmt_cipher(dev[0], apdev[0], "BIP-GMAC-128", "BIP-GMAC-128")

def test_ap_cipher_bip_gmac_256(dev, apdev):
    """WPA2-PSK with BIP-GMAC-256"""
    check_group_mgmt_cipher(dev[0], apdev[0], "BIP-GMAC-256")

def test_ap_cipher_bip_gmac_256_req(dev, apdev):
    """WPA2-PSK with BIP-GMAC-256 required"""
    check_group_mgmt_cipher(dev[0], apdev[0], "BIP-GMAC-256", "BIP-GMAC-256")

def test_ap_cipher_bip_cmac_256(dev, apdev):
    """WPA2-PSK with BIP-CMAC-256"""
    check_group_mgmt_cipher(dev[0], apdev[0], "BIP-CMAC-256")

def test_ap_cipher_bip_cmac_256_req(dev, apdev):
    """WPA2-PSK with BIP-CMAC-256 required"""
    check_group_mgmt_cipher(dev[0], apdev[0], "BIP-CMAC-256", "BIP-CMAC-256")

def test_ap_cipher_bip_req_mismatch(dev, apdev):
    """WPA2-PSK with BIP cipher mismatch"""
    group_mgmt = dev[0].get_capability("group_mgmt")
    for cipher in ["AES-128-CMAC", "BIP-GMAC-256"]:
        if cipher not in group_mgmt:
            raise HwsimSkip("Cipher %s not supported" % cipher)

    params = {"ssid": "test-wpa2-psk-pmf",
              "wpa_passphrase": "12345678",
              "wpa": "2",
              "ieee80211w": "2",
              "wpa_key_mgmt": "WPA-PSK-SHA256",
              "rsn_pairwise": "CCMP",
              "group_mgmt_cipher": "AES-128-CMAC"}
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].scan_for_bss(hapd.own_addr(), 2412)
    id = dev[0].connect("test-wpa2-psk-pmf", psk="12345678", ieee80211w="2",
                        key_mgmt="WPA-PSK-SHA256", group_mgmt="BIP-GMAC-256",
                        pairwise="CCMP", group="CCMP", scan_freq="2412",
                        wait_connect=False)
    ev = dev[0].wait_event(["CTRL-EVENT-NETWORK-NOT-FOUND",
                            "CTRL-EVENT-CONNECTED"], timeout=10)
    if ev is None:
        raise Exception("Network selection result not indicated")
    if "CTRL-EVENT-CONNECTED" in ev:
        raise Exception("Unexpected connection")

    dev[0].request("DISCONNECT")
    dev[0].set_network(id, "group_mgmt", "AES-128-CMAC")
    dev[0].select_network(id)
    dev[0].wait_connected()

def get_rx_spec(phy, keytype=KT_PTK):
    keys = "/sys/kernel/debug/ieee80211/%s/keys" % (phy)
    try:
        for key in os.listdir(keys):
            keydir = keys + "/" + key
            with open(keydir + '/keyidx') as f:
                keyid = int(f.read())
            if keytype in (KT_PTK, KT_GTK) and keyid not in (0, 1, 2, 3):
                continue
            if keytype == KT_IGTK and keyid not in (4, 5):
                continue
            if keytype == KT_BIGTK and keyid not in (6, 7):
                continue
            files = os.listdir(keydir)
            if keytype == KT_PTK and "station" not in files:
                continue
            if keytype != KT_PTK and "station" in files:
                continue
            with open(keydir + "/rx_spec") as f:
                return f.read()
    except OSError as e:
        raise HwsimSkip("debugfs not supported in mac80211")
    return None

def get_tk_replay_counter(phy, keytype=KT_PTK):
    keys = "/sys/kernel/debug/ieee80211/%s/keys" % (phy)
    try:
        for key in os.listdir(keys):
            keydir = keys + "/" + key
            with open(keydir + '/keyidx') as f:
                keyid = int(f.read())
            if keytype in (KT_PTK, KT_GTK) and keyid not in (0, 1, 2, 3):
                continue
            if keytype == KT_IGTK and keyid not in (4, 5):
                continue
            if keytype == KT_BIGTK and keyid not in (6, 7):
                continue
            files = os.listdir(keydir)
            if keytype == KT_PTK and "station" not in files:
                continue
            if keytype != KT_PTK and "station" in files:
                continue
            with open(keydir + "/replays") as f:
                return int(f.read())
    except OSError as e:
        raise HwsimSkip("debugfs not supported in mac80211")
    return None

def test_ap_cipher_replay_protection_ap_ccmp(dev, apdev):
    """CCMP replay protection on AP"""
    run_ap_cipher_replay_protection_ap(dev, apdev, "CCMP")

def test_ap_cipher_replay_protection_ap_tkip(dev, apdev):
    """TKIP replay protection on AP"""
    skip_without_tkip(dev[0])
    run_ap_cipher_replay_protection_ap(dev, apdev, "TKIP")

def test_ap_cipher_replay_protection_ap_gcmp(dev, apdev):
    """GCMP replay protection on AP"""
    if "GCMP" not in dev[0].get_capability("pairwise"):
        raise HwsimSkip("GCMP not supported")
    run_ap_cipher_replay_protection_ap(dev, apdev, "GCMP")

def run_ap_cipher_replay_protection_ap(dev, apdev, cipher):
    params = {"ssid": "test-wpa2-psk",
              "wpa_passphrase": "12345678",
              "wpa": "2",
              "wpa_key_mgmt": "WPA-PSK",
              "rsn_pairwise": cipher}
    hapd = hostapd.add_ap(apdev[0], params)
    phy = hapd.get_driver_status_field("phyname")

    Wlantest.setup(hapd)
    wt = Wlantest()
    wt.flush()
    wt.add_passphrase("12345678")

    dev[0].connect("test-wpa2-psk", psk="12345678",
                   pairwise=cipher, group=cipher, scan_freq="2412")
    hapd.wait_sta()

    if cipher != "TKIP":
        replays = get_tk_replay_counter(phy)
        if replays != 0:
            raise Exception("Unexpected replay reported (1)")

    for i in range(5):
        hwsim_utils.test_connectivity(dev[0], hapd)

    if cipher != "TKIP":
        replays = get_tk_replay_counter(phy)
        if replays != 0:
            raise Exception("Unexpected replay reported (2)")

    if "OK" not in dev[0].request("RESET_PN"):
        raise Exception("RESET_PN failed")
    time.sleep(0.1)
    hwsim_utils.test_connectivity(dev[0], hapd, timeout=1,
                                  success_expected=False)

    if cipher != "TKIP":
        replays = get_tk_replay_counter(phy)
        if replays < 1:
            raise Exception("Replays not reported")

def test_ap_cipher_replay_protection_sta_ccmp(dev, apdev):
    """CCMP replay protection on STA (TK)"""
    run_ap_cipher_replay_protection_sta(dev, apdev, "CCMP")

def test_ap_cipher_replay_protection_sta_tkip(dev, apdev):
    """TKIP replay protection on STA (TK)"""
    skip_without_tkip(dev[0])
    run_ap_cipher_replay_protection_sta(dev, apdev, "TKIP")

def test_ap_cipher_replay_protection_sta_gcmp(dev, apdev):
    """GCMP replay protection on STA (TK)"""
    if "GCMP" not in dev[0].get_capability("pairwise"):
        raise HwsimSkip("GCMP not supported")
    run_ap_cipher_replay_protection_sta(dev, apdev, "GCMP")

def test_ap_cipher_replay_protection_sta_gtk_ccmp(dev, apdev):
    """CCMP replay protection on STA (GTK)"""
    run_ap_cipher_replay_protection_sta(dev, apdev, "CCMP", keytype=KT_GTK)

def test_ap_cipher_replay_protection_sta_gtk_tkip(dev, apdev):
    """TKIP replay protection on STA (GTK)"""
    skip_without_tkip(dev[0])
    run_ap_cipher_replay_protection_sta(dev, apdev, "TKIP", keytype=KT_GTK)

def test_ap_cipher_replay_protection_sta_gtk_gcmp(dev, apdev):
    """GCMP replay protection on STA (GTK)"""
    if "GCMP" not in dev[0].get_capability("pairwise"):
        raise HwsimSkip("GCMP not supported")
    run_ap_cipher_replay_protection_sta(dev, apdev, "GCMP", keytype=KT_GTK)

def test_ap_cipher_replay_protection_sta_igtk(dev, apdev):
    """CCMP replay protection on STA (IGTK)"""
    run_ap_cipher_replay_protection_sta(dev, apdev, "CCMP", keytype=KT_IGTK)

def test_ap_cipher_replay_protection_sta_bigtk(dev, apdev):
    """CCMP replay protection on STA (BIGTK)"""
    run_ap_cipher_replay_protection_sta(dev, apdev, "CCMP", keytype=KT_BIGTK)

def run_ap_cipher_replay_protection_sta(dev, apdev, cipher, keytype=KT_PTK):
    params = {"ssid": "test-wpa2-psk",
              "wpa_passphrase": "12345678",
              "wpa": "2",
              "wpa_key_mgmt": "WPA-PSK",
              "rsn_pairwise": cipher}
    if keytype == KT_IGTK or keytype == KT_BIGTK:
        params['ieee80211w'] = '2'
    if keytype == KT_BIGTK:
        params['beacon_prot'] = '1'
    hapd = hostapd.add_ap(apdev[0], params)

    Wlantest.setup(hapd)
    wt = Wlantest()
    wt.flush()
    wt.add_passphrase("12345678")

    phy = dev[0].get_driver_status_field("phyname")
    dev[0].connect("test-wpa2-psk", psk="12345678", ieee80211w='1',
                   beacon_prot='1',
                   pairwise=cipher, group=cipher, scan_freq="2412")
    hapd.wait_sta()

    if keytype == KT_BIGTK:
        time.sleep(1)

    if cipher != "TKIP":
        replays = get_tk_replay_counter(phy, keytype)
        if replays != 0:
            raise Exception("Unexpected replay reported (1)")

    for i in range(5):
        hwsim_utils.test_connectivity(dev[0], hapd)

    if cipher != "TKIP":
        replays = get_tk_replay_counter(phy, keytype)
        if replays != 0:
            raise Exception("Unexpected replay reported (2)")

    if keytype == KT_IGTK:
        hapd.request("DEAUTHENTICATE ff:ff:ff:ff:ff:ff test=1")
        ev = dev[0].wait_event(["CTRL-EVENT-DISCONNECTED"], timeout=1)
        if ev:
            dev[0].wait_connected()

    addr = "ff:ff:ff:ff:ff:ff" if keytype != KT_PTK else dev[0].own_addr()
    cmd = "RESET_PN " + addr
    if keytype == KT_IGTK:
        cmd += " IGTK"
    if keytype == KT_BIGTK:
        cmd += " BIGTK"
    if "OK" not in hapd.request(cmd):
        raise Exception("RESET_PN failed")
    time.sleep(0.1)
    if keytype == KT_IGTK:
        hapd.request("DEAUTHENTICATE ff:ff:ff:ff:ff:ff test=1")
        ev = dev[0].wait_event(["CTRL-EVENT-DISCONNECTED"], timeout=1)
    elif keytype == KT_BIGTK:
        time.sleep(1)
    else:
        hwsim_utils.test_connectivity(dev[0], hapd, timeout=1,
                                      success_expected=False)

    if cipher != "TKIP":
        replays = get_tk_replay_counter(phy, keytype)
        if replays < 1:
            raise Exception("Replays not reported")

@disable_ipv6
def test_ap_wpa2_delayed_m3_retransmission(dev, apdev):
    """Delayed M3 retransmission"""
    params = hostapd.wpa2_params(ssid="test-wpa2-psk", passphrase="12345678")
    hapd = hostapd.add_ap(apdev[0], params)

    Wlantest.setup(hapd)
    wt = Wlantest()
    wt.flush()
    wt.add_passphrase("12345678")

    phy = dev[0].get_driver_status_field("phyname")
    dev[0].connect("test-wpa2-psk", psk="12345678", scan_freq="2412")
    hapd.wait_sta()

    for i in range(5):
        hwsim_utils.test_connectivity(dev[0], hapd)

    time.sleep(0.1)
    before_tk = get_rx_spec(phy, keytype=KT_PTK).splitlines()
    before_gtk = get_rx_spec(phy, keytype=KT_GTK).splitlines()
    addr = dev[0].own_addr()
    if "OK" not in hapd.request("RESEND_M3 " + addr):
        raise Exception("RESEND_M3 failed")
    time.sleep(0.1)
    after_tk = get_rx_spec(phy, keytype=KT_PTK).splitlines()
    after_gtk = get_rx_spec(phy, keytype=KT_GTK).splitlines()

    if "OK" not in hapd.request("RESET_PN " + addr):
        raise Exception("RESET_PN failed")
    time.sleep(0.1)
    hwsim_utils.test_connectivity(dev[0], hapd, timeout=1,
                                  success_expected=False)
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    for i in range(len(before_tk)):
        b = int(before_tk[i], 16)
        a = int(after_tk[i], 16)
        if a < b:
            raise Exception("TK RX counter decreased: idx=%d before=%d after=%d" % (i, b, a))

    for i in range(len(before_gtk)):
        b = int(before_gtk[i], 16)
        a = int(after_gtk[i], 16)
        if a < b:
            raise Exception("GTK RX counter decreased: idx=%d before=%d after=%d" % (i, b, a))

@disable_ipv6
def test_ap_wpa2_delayed_m1_m3_retransmission(dev, apdev):
    """Delayed M1+M3 retransmission"""
    run_ap_wpa2_delayed_m1_m3_retransmission(dev, apdev, False)

@disable_ipv6
def test_ap_wpa2_delayed_m1_m3_retransmission2(dev, apdev):
    """Delayed M1+M3 retransmission (change M1 ANonce)"""
    run_ap_wpa2_delayed_m1_m3_retransmission(dev, apdev, True)

def run_ap_wpa2_delayed_m1_m3_retransmission(dev, apdev,
                                             change_m1_anonce=False):
    params = hostapd.wpa2_params(ssid="test-wpa2-psk", passphrase="12345678")
    hapd = hostapd.add_ap(apdev[0], params)

    Wlantest.setup(hapd)
    wt = Wlantest()
    wt.flush()
    wt.add_passphrase("12345678")

    phy = dev[0].get_driver_status_field("phyname")
    dev[0].connect("test-wpa2-psk", psk="12345678", scan_freq="2412")
    hapd.wait_sta()

    for i in range(5):
        hwsim_utils.test_connectivity(dev[0], hapd)

    time.sleep(0.1)
    before_tk = get_rx_spec(phy, keytype=KT_PTK).splitlines()
    before_gtk = get_rx_spec(phy, keytype=KT_GTK).splitlines()
    addr = dev[0].own_addr()
    if change_m1_anonce:
        if "OK" not in hapd.request("RESEND_M1 " + addr + " change-anonce"):
            raise Exception("RESEND_M1 failed")
    if "OK" not in hapd.request("RESEND_M1 " + addr):
        raise Exception("RESEND_M1 failed")
    if "OK" not in hapd.request("RESEND_M3 " + addr):
        raise Exception("RESEND_M3 failed")
    time.sleep(0.1)
    after_tk = get_rx_spec(phy, keytype=KT_PTK).splitlines()
    after_gtk = get_rx_spec(phy, keytype=KT_GTK).splitlines()

    if "OK" not in hapd.request("RESET_PN " + addr):
        raise Exception("RESET_PN failed")
    time.sleep(0.1)
    hwsim_utils.test_connectivity(dev[0], hapd, timeout=1,
                                  success_expected=False)
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    for i in range(len(before_tk)):
        b = int(before_tk[i], 16)
        a = int(after_tk[i], 16)
        if a < b:
            raise Exception("TK RX counter decreased: idx=%d before=%d after=%d" % (i, b, a))

    for i in range(len(before_gtk)):
        b = int(before_gtk[i], 16)
        a = int(after_gtk[i], 16)
        if a < b:
            raise Exception("GTK RX counter decreased: idx=%d before=%d after=%d" % (i, b, a))

@disable_ipv6
def test_ap_wpa2_delayed_group_m1_retransmission(dev, apdev):
    """Delayed group M1 retransmission"""
    params = hostapd.wpa2_params(ssid="test-wpa2-psk", passphrase="12345678")
    hapd = hostapd.add_ap(apdev[0], params)

    Wlantest.setup(hapd)
    wt = Wlantest()
    wt.flush()
    wt.add_passphrase("12345678")

    phy = dev[0].get_driver_status_field("phyname")
    dev[0].connect("test-wpa2-psk", psk="12345678", scan_freq="2412")
    hapd.wait_sta()

    for i in range(5):
        hwsim_utils.test_connectivity(dev[0], hapd)

    time.sleep(0.1)
    before = get_rx_spec(phy, keytype=KT_GTK).splitlines()
    addr = dev[0].own_addr()
    if "OK" not in hapd.request("RESEND_GROUP_M1 " + addr):
        raise Exception("RESEND_GROUP_M1 failed")
    time.sleep(0.1)
    after = get_rx_spec(phy, keytype=KT_GTK).splitlines()

    if "OK" not in hapd.request("RESET_PN ff:ff:ff:ff:ff:ff"):
        raise Exception("RESET_PN failed")
    time.sleep(0.1)
    hwsim_utils.test_connectivity(dev[0], hapd, timeout=1,
                                  success_expected=False)
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    for i in range(len(before)):
        b = int(before[i], 16)
        a = int(after[i], 16)
        if a < b:
            raise Exception("RX counter decreased: idx=%d before=%d after=%d" % (i, b, a))

@disable_ipv6
def test_ap_wpa2_delayed_group_m1_retransmission_igtk(dev, apdev):
    """Delayed group M1 retransmission (check IGTK protection)"""
    params = hostapd.wpa2_params(ssid="test-wpa2-psk", passphrase="12345678",
                                 ieee80211w="2")
    hapd = hostapd.add_ap(apdev[0], params)

    Wlantest.setup(hapd)
    wt = Wlantest()
    wt.flush()
    wt.add_passphrase("12345678")

    phy = dev[0].get_driver_status_field("phyname")
    dev[0].connect("test-wpa2-psk", psk="12345678", scan_freq="2412",
                   ieee80211w="1")
    hapd.wait_sta()

    hwsim_utils.test_connectivity(dev[0], hapd, timeout=1)

    # deauth once to see that works OK
    addr = dev[0].own_addr()
    hapd.request("DEAUTHENTICATE ff:ff:ff:ff:ff:ff")
    dev[0].wait_disconnected(timeout=10)

    # now to check the protection
    dev[0].request("RECONNECT")
    dev[0].wait_connected()
    hapd.wait_sta()

    hwsim_utils.test_connectivity(dev[0], hapd, timeout=1)

    if "OK" not in hapd.request("RESEND_GROUP_M1 " + addr):
        raise Exception("RESEND_GROUP_M1 failed")
    if "OK" not in hapd.request("RESET_PN ff:ff:ff:ff:ff:ff IGTK"):
        raise Exception("RESET_PN failed")

    time.sleep(0.1)
    hapd.request("DEAUTHENTICATE ff:ff:ff:ff:ff:ff test=1")

    ev = dev[0].wait_event(["CTRL-EVENT-DISCONNECTED"], timeout=0.1)
    if ev is not None:
        raise Exception("Unexpected disconnection")

    hwsim_utils.test_connectivity(dev[0], hapd, timeout=1)

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

def test_ap_wpa2_delayed_m1_m3_zero_tk(dev, apdev):
    """Delayed M1+M3 retransmission and zero TK"""
    params = hostapd.wpa2_params(ssid="test-wpa2-psk", passphrase="12345678")
    hapd = hostapd.add_ap(apdev[0], params)

    Wlantest.setup(hapd)
    wt = Wlantest()
    wt.flush()
    wt.add_passphrase("12345678")

    dev[0].connect("test-wpa2-psk", psk="12345678", scan_freq="2412")
    hapd.wait_sta()

    hwsim_utils.test_connectivity(dev[0], hapd)
    addr = dev[0].own_addr()
    if "OK" not in hapd.request("RESEND_M1 " + addr + " change-anonce"):
        raise Exception("RESEND_M1 failed")
    if "OK" not in hapd.request("RESEND_M1 " + addr):
        raise Exception("RESEND_M1 failed")
    if "OK" not in hapd.request("RESEND_M3 " + addr):
        raise Exception("RESEND_M3 failed")

    KEY_FLAG_RX = 0x04
    KEY_FLAG_TX = 0x08
    KEY_FLAG_PAIRWISE = 0x20
    KEY_FLAG_RX_TX = KEY_FLAG_RX | KEY_FLAG_TX
    KEY_FLAG_PAIRWISE_RX_TX = KEY_FLAG_PAIRWISE | KEY_FLAG_RX_TX
    if "OK" not in hapd.request("SET_KEY 3 %s %d %d %s %s %d" % (addr, 0, 1, 6*"00", 16*"00", KEY_FLAG_PAIRWISE_RX_TX)):
        raise Exception("SET_KEY failed")
    time.sleep(0.1)
    hwsim_utils.test_connectivity(dev[0], hapd, timeout=1, broadcast=False,
                                  success_expected=False)
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

def test_ap_wpa2_plaintext_m1_m3(dev, apdev):
    """Plaintext M1/M3 during PTK rekey"""
    params = hostapd.wpa2_params(ssid="test-wpa2-psk", passphrase="12345678")
    hapd = hostapd.add_ap(apdev[0], params)

    Wlantest.setup(hapd)
    wt = Wlantest()
    wt.flush()
    wt.add_passphrase("12345678")

    dev[0].connect("test-wpa2-psk", psk="12345678", scan_freq="2412")

    time.sleep(0.1)
    addr = dev[0].own_addr()
    if "OK" not in hapd.request("RESEND_M1 " + addr + " plaintext"):
        raise Exception("RESEND_M1 failed")
    time.sleep(0.1)
    if "OK" not in hapd.request("RESEND_M3 " + addr + " plaintext"):
        raise Exception("RESEND_M3 failed")
    time.sleep(0.1)

def test_ap_wpa2_plaintext_m1_m3_pmf(dev, apdev):
    """Plaintext M1/M3 during PTK rekey (PMF)"""
    params = hostapd.wpa2_params(ssid="test-wpa2-psk", passphrase="12345678")
    params["ieee80211w"] = "2"
    hapd = hostapd.add_ap(apdev[0], params)

    Wlantest.setup(hapd)
    wt = Wlantest()
    wt.flush()
    wt.add_passphrase("12345678")

    dev[0].connect("test-wpa2-psk", psk="12345678", ieee80211w="2",
                   scan_freq="2412")

    time.sleep(0.1)
    addr = dev[0].own_addr()
    if "OK" not in hapd.request("RESEND_M1 " + addr + " plaintext"):
        raise Exception("RESEND_M1 failed")
    time.sleep(0.1)
    if "OK" not in hapd.request("RESEND_M3 " + addr + " plaintext"):
        raise Exception("RESEND_M3 failed")
    time.sleep(0.1)

def test_ap_wpa2_plaintext_m3(dev, apdev):
    """Plaintext M3 during PTK rekey"""
    params = hostapd.wpa2_params(ssid="test-wpa2-psk", passphrase="12345678")
    hapd = hostapd.add_ap(apdev[0], params)

    Wlantest.setup(hapd)
    wt = Wlantest()
    wt.flush()
    wt.add_passphrase("12345678")

    dev[0].connect("test-wpa2-psk", psk="12345678", scan_freq="2412")

    time.sleep(0.1)
    addr = dev[0].own_addr()
    if "OK" not in hapd.request("RESEND_M1 " + addr):
        raise Exception("RESEND_M1 failed")
    time.sleep(0.1)
    if "OK" not in hapd.request("RESEND_M3 " + addr + " plaintext"):
        raise Exception("RESEND_M3 failed")
    time.sleep(0.1)

def test_ap_wpa2_plaintext_group_m1(dev, apdev):
    """Plaintext group M1"""
    params = hostapd.wpa2_params(ssid="test-wpa2-psk", passphrase="12345678")
    hapd = hostapd.add_ap(apdev[0], params)

    Wlantest.setup(hapd)
    wt = Wlantest()
    wt.flush()
    wt.add_passphrase("12345678")

    dev[0].connect("test-wpa2-psk", psk="12345678", scan_freq="2412")

    time.sleep(0.1)
    addr = dev[0].own_addr()
    if "OK" not in hapd.request("RESEND_GROUP_M1 " + addr + " plaintext"):
        raise Exception("RESEND_GROUP_M1 failed")
    time.sleep(0.2)
    if "OK" not in hapd.request("RESEND_GROUP_M1 " + addr):
        raise Exception("RESEND_GROUP_M1 failed")
    time.sleep(0.1)

def test_ap_wpa2_plaintext_group_m1_pmf(dev, apdev):
    """Plaintext group M1 (PMF)"""
    params = hostapd.wpa2_params(ssid="test-wpa2-psk", passphrase="12345678")
    params["ieee80211w"] = "2"
    hapd = hostapd.add_ap(apdev[0], params)

    Wlantest.setup(hapd)
    wt = Wlantest()
    wt.flush()
    wt.add_passphrase("12345678")

    dev[0].connect("test-wpa2-psk", psk="12345678", ieee80211w="2",
                   scan_freq="2412")

    time.sleep(0.1)
    addr = dev[0].own_addr()
    if "OK" not in hapd.request("RESEND_GROUP_M1 " + addr + " plaintext"):
        raise Exception("RESEND_GROUP_M1 failed")
    time.sleep(0.2)
    if "OK" not in hapd.request("RESEND_GROUP_M1 " + addr):
        raise Exception("RESEND_GROUP_M1 failed")
    time.sleep(0.1)

def test_ap_wpa2_test_command_failures(dev, apdev):
    """EAPOL/key config test command failures"""
    params = hostapd.wpa2_params(ssid="test-wpa2-psk", passphrase="12345678")
    hapd = hostapd.add_ap(apdev[0], params)
    tests = ["RESEND_M1 foo",
             "RESEND_M1 22:22:22:22:22:22",
             "RESEND_M3 foo",
             "RESEND_M3 22:22:22:22:22:22",
             "RESEND_GROUP_M1 foo",
             "RESEND_GROUP_M1 22:22:22:22:22:22",
             "SET_KEY foo",
             "SET_KEY 3 foo",
             "SET_KEY 3 22:22:22:22:22:22",
             "SET_KEY 3 22:22:22:22:22:22 1",
             "SET_KEY 3 22:22:22:22:22:22 1 1",
             "SET_KEY 3 22:22:22:22:22:22 1 1 q",
             "SET_KEY 3 22:22:22:22:22:22 1 1 112233445566",
             "SET_KEY 3 22:22:22:22:22:22 1 1 112233445566 1",
             "SET_KEY 3 22:22:22:22:22:22 1 1 112233445566 12",
             "SET_KEY 3 22:22:22:22:22:22 1 1 112233445566 12 1",
             "SET_KEY 3 22:22:22:22:22:22 1 1 112233445566 12 1 ",
             "RESET_PN ff:ff:ff:ff:ff:ff BIGTK",
             "RESET_PN ff:ff:ff:ff:ff:ff IGTK",
             "RESET_PN 22:22:22:22:22:22",
             "RESET_PN foo"]
    for t in tests:
        if "FAIL" not in hapd.request(t):
            raise Exception("Invalid command accepted: " + t)

def test_ap_wpa2_gtk_initial_rsc_tkip(dev, apdev):
    """Initial group cipher RSC (TKIP)"""
    skip_without_tkip(dev[0])
    run_ap_wpa2_gtk_initial_rsc(dev, apdev, "TKIP")

def test_ap_wpa2_gtk_initial_rsc_ccmp(dev, apdev):
    """Initial group cipher RSC (CCMP)"""
    run_ap_wpa2_gtk_initial_rsc(dev, apdev, "CCMP")

def test_ap_wpa2_gtk_initial_rsc_ccmp_256(dev, apdev):
    """Initial group cipher RSC (CCMP-256)"""
    run_ap_wpa2_gtk_initial_rsc(dev, apdev, "CCMP-256")

def test_ap_wpa2_gtk_initial_rsc_gcmp(dev, apdev):
    """Initial group cipher RSC (GCMP)"""
    run_ap_wpa2_gtk_initial_rsc(dev, apdev, "GCMP")

def test_ap_wpa2_gtk_initial_rsc_gcmp_256(dev, apdev):
    """Initial group cipher RSC (GCMP-256)"""
    run_ap_wpa2_gtk_initial_rsc(dev, apdev, "GCMP-256")

def run_ap_wpa2_gtk_initial_rsc(dev, apdev, cipher):
    if cipher not in dev[0].get_capability("pairwise") or \
       cipher not in dev[0].get_capability("group"):
        raise HwsimSkip("Cipher %s not supported" % cipher)

    params = hostapd.wpa2_params(ssid="test-wpa2-psk", passphrase="12345678")
    params["rsn_pairwise"] = cipher
    params["group_cipher"] = cipher
    params["gtk_rsc_override"] = "341200000000"
    hapd = hostapd.add_ap(apdev[0], params)

    Wlantest.setup(hapd)
    wt = Wlantest()
    wt.flush()
    wt.add_passphrase("12345678")

    dev[0].connect("test-wpa2-psk", psk="12345678", proto="WPA2",
                   pairwise=cipher, group=cipher, scan_freq="2412")
    hapd.wait_sta()
    # Verify that unicast traffic works, but broadcast traffic does not.
    hwsim_utils.test_connectivity(dev[0], hapd, broadcast=False)
    hwsim_utils.test_connectivity(dev[0], hapd, success_expected=False)
    hwsim_utils.test_connectivity(dev[0], hapd, success_expected=False)

def test_ap_wpa2_igtk_initial_rsc_aes_128_cmac(dev, apdev):
    """Initial management group cipher RSC (AES-128-CMAC)"""
    run_ap_wpa2_igtk_initial_rsc(dev, apdev, "AES-128-CMAC")

def test_ap_wpa2_igtk_initial_rsc_bip_gmac_128(dev, apdev):
    """Initial management group cipher RSC (BIP-GMAC-128)"""
    run_ap_wpa2_igtk_initial_rsc(dev, apdev, "BIP-GMAC-128")

def test_ap_wpa2_igtk_initial_rsc_bip_gmac_256(dev, apdev):
    """Initial management group cipher RSC (BIP-GMAC-256)"""
    run_ap_wpa2_igtk_initial_rsc(dev, apdev, "BIP-GMAC-256")

def test_ap_wpa2_igtk_initial_rsc_bip_cmac_256(dev, apdev):
    """Initial management group cipher RSC (BIP-CMAC-256)"""
    run_ap_wpa2_igtk_initial_rsc(dev, apdev, "BIP-CMAC-256")

def run_ap_wpa2_igtk_initial_rsc(dev, apdev, cipher):
    if cipher not in dev[0].get_capability("group_mgmt"):
        raise HwsimSkip("Cipher %s not supported" % cipher)

    params = hostapd.wpa2_params(ssid="test-wpa2-psk", passphrase="12345678")
    params["ieee80211w"] = "2"
    params["rsn_pairwise"] = "CCMP"
    params["group_cipher"] = "CCMP"
    params["group_mgmt_cipher"] = cipher
    params["igtk_rsc_override"] = "341200000000"
    hapd = hostapd.add_ap(apdev[0], params)

    Wlantest.setup(hapd)
    wt = Wlantest()
    wt.flush()
    wt.add_passphrase("12345678")

    dev[0].connect("test-wpa2-psk", psk="12345678", proto="WPA2",
                   ieee80211w="2", pairwise="CCMP", group="CCMP",
                   group_mgmt=cipher,
                   scan_freq="2412")
    hapd.wait_sta()
    # Verify that broadcast robust management frames are dropped.
    dev[0].note("Sending broadcast Deauthentication and Disassociation frames with too small IPN")
    hapd.request("DEAUTHENTICATE ff:ff:ff:ff:ff:ff test=1")
    hapd.request("DISASSOCIATE ff:ff:ff:ff:ff:ff test=1")
    hapd.request("DEAUTHENTICATE ff:ff:ff:ff:ff:ff test=1")
    hapd.request("DISASSOCIATE ff:ff:ff:ff:ff:ff test=1")
    dev[0].note("Done sending broadcast Deauthentication and Disassociation frames with too small IPN")
    ev = dev[0].wait_event(["CTRL-EVENT-DISCONNECTED"], timeout=1)
    if ev is not None:
        raise Exception("Unexpected disconnection")

    # Verify thar unicast robust management frames go through.
    hapd.request("DEAUTHENTICATE " + dev[0].own_addr() + " reason=123 test=1")
    ev = dev[0].wait_event(["CTRL-EVENT-DISCONNECTED"], timeout=1)
    if ev is None:
        raise Exception("Disconnection not reported")
    if "reason=123" not in ev:
        raise Exception("Unexpected disconnection reason: " + ev)
