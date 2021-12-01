# Test cases for SAE
# Copyright (c) 2013-2020, Jouni Malinen <j@w1.fi>
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

from remotehost import remote_compatible
import binascii
import os
import time
import logging
logger = logging.getLogger()
import socket
import struct
import subprocess

import hwsim_utils
import hostapd
from wpasupplicant import WpaSupplicant
from utils import *
from test_ap_psk import find_wpas_process, read_process_memory, verify_not_present, get_key_locations

@remote_compatible
def test_sae(dev, apdev):
    """SAE with default group"""
    check_sae_capab(dev[0])
    params = hostapd.wpa2_params(ssid="test-sae",
                                 passphrase="12345678")
    params['wpa_key_mgmt'] = 'SAE'
    hapd = hostapd.add_ap(apdev[0], params)
    key_mgmt = hapd.get_config()['key_mgmt']
    if key_mgmt.split(' ')[0] != "SAE":
        raise Exception("Unexpected GET_CONFIG(key_mgmt): " + key_mgmt)

    dev[0].request("SET sae_groups ")
    id = dev[0].connect("test-sae", psk="12345678", key_mgmt="SAE",
                        scan_freq="2412")
    hapd.wait_sta()
    if dev[0].get_status_field('sae_group') != '19':
            raise Exception("Expected default SAE group not used")
    bss = dev[0].get_bss(apdev[0]['bssid'])
    if 'flags' not in bss:
        raise Exception("Could not get BSS flags from BSS table")
    if "[WPA2-SAE-CCMP]" not in bss['flags']:
        raise Exception("Unexpected BSS flags: " + bss['flags'])

    res = hapd.request("STA-FIRST")
    if "sae_group=19" not in res.splitlines():
        raise Exception("hostapd STA output did not specify SAE group")

    pmk_h = hapd.request("GET_PMK " + dev[0].own_addr())
    pmk_w = dev[0].get_pmk(id)
    if pmk_h != pmk_w:
        raise Exception("Fetched PMK does not match: hostapd %s, wpa_supplicant %s" % (pmk_h, pmk_w))
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()
    pmk_h2 = hapd.request("GET_PMK " + dev[0].own_addr())
    if pmk_h != pmk_h2:
        raise Exception("Fetched PMK from PMKSA cache does not match: %s, %s" % (pmk_h, pmk_h2))
    if "FAIL" not in hapd.request("GET_PMK foo"):
        raise Exception("Invalid GET_PMK did not return failure")
    if "FAIL" not in hapd.request("GET_PMK 02:ff:ff:ff:ff:ff"):
        raise Exception("GET_PMK for unknown STA did not return failure")

@remote_compatible
def test_sae_password_ecc(dev, apdev):
    """SAE with number of different passwords (ECC)"""
    check_sae_capab(dev[0])
    params = hostapd.wpa2_params(ssid="test-sae",
                                 passphrase="12345678")
    params['wpa_key_mgmt'] = 'SAE'
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].request("SET sae_groups 19")

    for i in range(10):
        password = "12345678-" + str(i)
        hapd.set("wpa_passphrase", password)
        dev[0].connect("test-sae", psk=password, key_mgmt="SAE",
                       scan_freq="2412")
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()

@remote_compatible
def test_sae_password_ffc(dev, apdev):
    """SAE with number of different passwords (FFC)"""
    check_sae_capab(dev[0])
    params = hostapd.wpa2_params(ssid="test-sae",
                                 passphrase="12345678")
    params['wpa_key_mgmt'] = 'SAE'
    params['sae_groups'] = '15'
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].request("SET sae_groups 15")

    for i in range(10):
        password = "12345678-" + str(i)
        hapd.set("wpa_passphrase", password)
        dev[0].connect("test-sae", psk=password, key_mgmt="SAE",
                       scan_freq="2412")
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()

@remote_compatible
def test_sae_pmksa_caching(dev, apdev):
    """SAE and PMKSA caching"""
    run_sae_pmksa_caching(dev, apdev)

@remote_compatible
def test_sae_pmksa_caching_pmkid(dev, apdev):
    """SAE and PMKSA caching (PMKID in AssocReq after SAE)"""
    try:
        dev[0].set("sae_pmkid_in_assoc", "1")
        run_sae_pmksa_caching(dev, apdev)
    finally:
        dev[0].set("sae_pmkid_in_assoc", "0")

def run_sae_pmksa_caching(dev, apdev):
    check_sae_capab(dev[0])
    params = hostapd.wpa2_params(ssid="test-sae",
                                 passphrase="12345678")
    params['wpa_key_mgmt'] = 'SAE'
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].request("SET sae_groups ")
    dev[0].connect("test-sae", psk="12345678", key_mgmt="SAE",
                   scan_freq="2412")
    ev = hapd.wait_event(["AP-STA-CONNECTED"], timeout=5)
    if ev is None:
        raise Exception("No connection event received from hostapd")
    sta0 = hapd.get_sta(dev[0].own_addr())
    if sta0['wpa'] != '2' or sta0['AKMSuiteSelector'] != '00-0f-ac-8':
        raise Exception("SAE STA(0) AKM suite selector reported incorrectly")
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()
    dev[0].request("RECONNECT")
    dev[0].wait_connected(timeout=15, error="Reconnect timed out")
    if dev[0].get_status_field('sae_group') is not None:
            raise Exception("SAE group claimed to have been used")
    sta0 = hapd.get_sta(dev[0].own_addr())
    if sta0['wpa'] != '2' or sta0['AKMSuiteSelector'] != '00-0f-ac-8':
        raise Exception("SAE STA(0) AKM suite selector reported incorrectly after PMKSA caching")

@remote_compatible
def test_sae_pmksa_caching_disabled(dev, apdev):
    """SAE and PMKSA caching disabled"""
    check_sae_capab(dev[0])
    params = hostapd.wpa2_params(ssid="test-sae",
                                 passphrase="12345678")
    params['wpa_key_mgmt'] = 'SAE'
    params['disable_pmksa_caching'] = '1'
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].request("SET sae_groups ")
    dev[0].connect("test-sae", psk="12345678", key_mgmt="SAE",
                   scan_freq="2412")
    ev = hapd.wait_event(["AP-STA-CONNECTED"], timeout=5)
    if ev is None:
        raise Exception("No connection event received from hostapd")
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()
    dev[0].request("RECONNECT")
    dev[0].wait_connected(timeout=15, error="Reconnect timed out")
    if dev[0].get_status_field('sae_group') != '19':
            raise Exception("Expected default SAE group not used")

def test_sae_groups(dev, apdev):
    """SAE with all supported groups"""
    check_sae_capab(dev[0])
    # This is the full list of supported groups, but groups 14-16 (2048-4096 bit
    # MODP) and group 21 (521-bit random ECP group) are a bit too slow on some
    # VMs and can result in hitting the mac80211 authentication timeout, so
    # allow them to fail and just report such failures in the debug log.
    sae_groups = [19, 25, 26, 20, 21, 1, 2, 5, 14, 15, 16, 22, 23, 24]
    tls = dev[0].request("GET tls_library")
    if tls.startswith("OpenSSL") and "run=OpenSSL 1." in tls:
        logger.info("Add Brainpool EC groups since OpenSSL is new enough")
        sae_groups += [27, 28, 29, 30]
    heavy_groups = [14, 15, 16]
    suitable_groups = [15, 16, 17, 18, 19, 20, 21]
    groups = [str(g) for g in sae_groups]
    params = hostapd.wpa2_params(ssid="test-sae-groups",
                                 passphrase="12345678")
    params['wpa_key_mgmt'] = 'SAE'
    params['sae_groups'] = ' '.join(groups)
    hapd = hostapd.add_ap(apdev[0], params)

    for g in groups:
        logger.info("Testing SAE group " + g)
        dev[0].request("SET sae_groups " + g)
        id = dev[0].connect("test-sae-groups", psk="12345678", key_mgmt="SAE",
                            scan_freq="2412", wait_connect=False)
        if int(g) in heavy_groups:
            ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED"], timeout=5)
            if ev is None:
                logger.info("No connection with heavy SAE group %s did not connect - likely hitting timeout in mac80211" % g)
                dev[0].remove_network(id)
                time.sleep(0.1)
                dev[0].dump_monitor()
                continue
            logger.info("Connection with heavy SAE group " + g)
        else:
            ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED"], timeout=10)
            if ev is None:
                if "BoringSSL" in tls and int(g) in [25]:
                    logger.info("Ignore connection failure with group " + g + " with BoringSSL")
                    dev[0].remove_network(id)
                    dev[0].dump_monitor()
                    continue
                if int(g) not in suitable_groups:
                    logger.info("Ignore connection failure with unsuitable group " + g)
                    dev[0].remove_network(id)
                    dev[0].dump_monitor()
                    continue
                raise Exception("Connection timed out with group " + g)
        if dev[0].get_status_field('sae_group') != g:
            raise Exception("Expected SAE group not used")
        pmksa = dev[0].get_pmksa(hapd.own_addr())
        if not pmksa:
            raise Exception("No PMKSA cache entry added")
        if pmksa['pmkid'] == '00000000000000000000000000000000':
            raise Exception("All zeros PMKID derived for group %s" % g)
        dev[0].remove_network(id)
        dev[0].wait_disconnected()
        dev[0].dump_monitor()

@remote_compatible
def test_sae_group_nego(dev, apdev):
    """SAE group negotiation"""
    check_sae_capab(dev[0])
    params = hostapd.wpa2_params(ssid="test-sae-group-nego",
                                 passphrase="12345678")
    params['wpa_key_mgmt'] = 'SAE'
    params['sae_groups'] = '19'
    hostapd.add_ap(apdev[0], params)

    dev[0].request("SET sae_groups 25 26 20 19")
    dev[0].connect("test-sae-group-nego", psk="12345678", key_mgmt="SAE",
                   scan_freq="2412")
    if dev[0].get_status_field('sae_group') != '19':
        raise Exception("Expected SAE group not used")

def test_sae_group_nego_no_match(dev, apdev):
    """SAE group negotiation (no match)"""
    check_sae_capab(dev[0])
    params = hostapd.wpa2_params(ssid="test-sae-group-nego",
                                 passphrase="12345678")
    params['wpa_key_mgmt'] = 'SAE'
    # None-existing SAE group to force all attempts to be rejected
    params['sae_groups'] = '0'
    hostapd.add_ap(apdev[0], params)

    dev[0].request("SET sae_groups ")
    dev[0].connect("test-sae-group-nego", psk="12345678", key_mgmt="SAE",
                   scan_freq="2412", wait_connect=False)
    ev = dev[0].wait_event(["CTRL-EVENT-SSID-TEMP-DISABLED"], timeout=10)
    dev[0].request("REMOVE_NETWORK all")
    if ev is None:
        raise Exception("Network profile disabling not reported")

@remote_compatible
def test_sae_anti_clogging(dev, apdev):
    """SAE anti clogging"""
    check_sae_capab(dev[0])
    check_sae_capab(dev[1])
    params = hostapd.wpa2_params(ssid="test-sae", passphrase="12345678")
    params['wpa_key_mgmt'] = 'SAE'
    params['sae_anti_clogging_threshold'] = '1'
    hostapd.add_ap(apdev[0], params)

    dev[0].request("SET sae_groups ")
    dev[1].request("SET sae_groups ")
    id = {}
    for i in range(0, 2):
        dev[i].scan(freq="2412")
        id[i] = dev[i].connect("test-sae", psk="12345678", key_mgmt="SAE",
                               scan_freq="2412", only_add_network=True)
    for i in range(0, 2):
        dev[i].select_network(id[i])
    for i in range(0, 2):
        dev[i].wait_connected(timeout=10)

def test_sae_forced_anti_clogging(dev, apdev):
    """SAE anti clogging (forced)"""
    check_sae_capab(dev[0])
    check_sae_capab(dev[1])
    params = hostapd.wpa2_params(ssid="test-sae", passphrase="12345678")
    params['wpa_key_mgmt'] = 'SAE WPA-PSK'
    params['sae_anti_clogging_threshold'] = '0'
    hostapd.add_ap(apdev[0], params)
    dev[2].connect("test-sae", psk="12345678", scan_freq="2412")
    for i in range(0, 2):
        dev[i].request("SET sae_groups ")
        dev[i].connect("test-sae", psk="12345678", key_mgmt="SAE",
                       scan_freq="2412")

def test_sae_mixed(dev, apdev):
    """Mixed SAE and non-SAE network"""
    check_sae_capab(dev[0])
    check_sae_capab(dev[1])
    params = hostapd.wpa2_params(ssid="test-sae", passphrase="12345678")
    params['wpa_key_mgmt'] = 'SAE WPA-PSK'
    params['sae_anti_clogging_threshold'] = '0'
    hapd = hostapd.add_ap(apdev[0], params)

    dev[2].connect("test-sae", psk="12345678", scan_freq="2412")
    for i in range(0, 2):
        dev[i].request("SET sae_groups ")
        dev[i].connect("test-sae", psk="12345678", key_mgmt="SAE",
                       scan_freq="2412")
    sta0 = hapd.get_sta(dev[0].own_addr())
    sta2 = hapd.get_sta(dev[2].own_addr())
    if sta0['wpa'] != '2' or sta0['AKMSuiteSelector'] != '00-0f-ac-8':
        raise Exception("SAE STA(0) AKM suite selector reported incorrectly")
    if sta2['wpa'] != '2' or sta2['AKMSuiteSelector'] != '00-0f-ac-2':
        raise Exception("PSK STA(2) AKM suite selector reported incorrectly")

def test_sae_and_psk(dev, apdev):
    """SAE and PSK enabled in network profile"""
    check_sae_capab(dev[0])
    params = hostapd.wpa2_params(ssid="test-sae", passphrase="12345678")
    params['wpa_key_mgmt'] = 'SAE'
    hostapd.add_ap(apdev[0], params)

    dev[0].request("SET sae_groups ")
    dev[0].connect("test-sae", psk="12345678", key_mgmt="SAE WPA-PSK",
                   scan_freq="2412")

def test_sae_and_psk2(dev, apdev):
    """SAE and PSK enabled in network profile (use PSK)"""
    check_sae_capab(dev[0])
    params = hostapd.wpa2_params(ssid="test-psk", passphrase="12345678")
    hostapd.add_ap(apdev[0], params)

    dev[0].request("SET sae_groups ")
    dev[0].connect("test-psk", psk="12345678", key_mgmt="SAE WPA-PSK",
                   scan_freq="2412")

def test_sae_wpa3_roam(dev, apdev):
    """SAE and WPA3-Personal transition mode roaming"""
    check_sae_capab(dev[0])

    # WPA3-Personal only AP
    params = hostapd.wpa2_params(ssid="test", passphrase="12345678")
    params['ieee80211w'] = '2'
    params['wpa_key_mgmt'] = 'SAE'
    hapd0 = hostapd.add_ap(apdev[0], params)

    # WPA2-Personal only AP
    params = hostapd.wpa2_params(ssid="test", passphrase="12345678")
    hapd1 = hostapd.add_ap(apdev[1], params)

    dev[0].set("sae_groups", "")
    dev[0].connect("test", psk="12345678", key_mgmt="SAE WPA-PSK",
                   ieee80211w="1", scan_freq="2412")
    bssid = dev[0].get_status_field('bssid')

    # Disable the current AP to force roam to the other one
    if bssid == apdev[0]['bssid']:
        hapd0.disable()
    else:
        hapd1.disable()
    dev[0].wait_connected()

    # Disable the current AP to force roam to the other (previous) one
    if bssid == apdev[0]['bssid']:
        hapd0.enable()
        hapd1.disable()
    else:
        hapd1.enable()
        hapd0.disable()
    dev[0].wait_connected()

    # Force roam to an AP in WPA3-Personal transition mode
    if bssid == apdev[0]['bssid']:
        hapd1.set("ieee80211w", "1")
        hapd1.set("sae_require_mfp", "1")
        hapd1.set("wpa_key_mgmt", "SAE WPA-PSK")
        hapd1.enable()
        hapd0.disable()
    else:
        hapd0.set("ieee80211w", "1")
        hapd0.set("sae_require_mfp", "1")
        hapd0.set("wpa_key_mgmt", "SAE WPA-PSK")
        hapd0.enable()
        hapd1.disable()
    dev[0].wait_connected()
    status = dev[0].get_status()
    if status['key_mgmt'] != "SAE":
        raise Exception("Did not use SAE with WPA3-Personal transition mode AP")
    if status['pmf'] != "1":
        raise Exception("Did not use PMF with WPA3-Personal transition mode AP")

def test_sae_mixed_mfp(dev, apdev):
    """Mixed SAE and non-SAE network and MFP required with SAE"""
    check_sae_capab(dev[0])
    check_sae_capab(dev[1])
    params = hostapd.wpa2_params(ssid="test-sae", passphrase="12345678")
    params['wpa_key_mgmt'] = 'SAE WPA-PSK'
    params["ieee80211w"] = "1"
    params['sae_require_mfp'] = '1'
    hostapd.add_ap(apdev[0], params)

    dev[0].request("SET sae_groups ")
    dev[0].connect("test-sae", psk="12345678", key_mgmt="SAE", ieee80211w="2",
                   scan_freq="2412")
    dev[0].dump_monitor()

    dev[1].request("SET sae_groups ")
    dev[1].connect("test-sae", psk="12345678", key_mgmt="SAE", ieee80211w="0",
                   scan_freq="2412", wait_connect=False)
    ev = dev[1].wait_event(["CTRL-EVENT-CONNECTED",
                            "CTRL-EVENT-ASSOC-REJECT"], timeout=10)
    if ev is None:
        raise Exception("No connection result reported")
    if "CTRL-EVENT-ASSOC-REJECT" not in ev:
        raise Exception("SAE connection without MFP was not rejected")
    if "status_code=31" not in ev:
        raise Exception("Unexpected status code in rejection: " + ev)
    dev[1].request("DISCONNECT")
    dev[1].dump_monitor()

    dev[2].connect("test-sae", psk="12345678", ieee80211w="0", scan_freq="2412")
    dev[2].dump_monitor()

def test_sae_and_psk_transition_disable(dev, apdev):
    """SAE and PSK transition disable indication"""
    check_sae_capab(dev[0])
    params = hostapd.wpa2_params(ssid="test-sae", passphrase="12345678")
    params["ieee80211w"] = "1"
    params['wpa_key_mgmt'] = 'SAE WPA-PSK'
    params['transition_disable'] = '0x01'
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].request("SET sae_groups ")
    id = dev[0].connect("test-sae", psk="12345678", key_mgmt="SAE WPA-PSK",
                        ieee80211w="1", scan_freq="2412")
    ev = dev[0].wait_event(["TRANSITION-DISABLE"], timeout=1)
    if ev is None:
        raise Exception("Transition disable not indicated")
    if ev.split(' ')[1] != "01":
        raise Exception("Unexpected transition disable bitmap: " + ev)

    val = dev[0].get_network(id, "ieee80211w")
    if val != "2":
        raise Exception("Unexpected ieee80211w value: " + val)
    val = dev[0].get_network(id, "key_mgmt")
    if val != "SAE":
        raise Exception("Unexpected key_mgmt value: " + val)
    val = dev[0].get_network(id, "group")
    if val != "CCMP":
        raise Exception("Unexpected group value: " + val)
    val = dev[0].get_network(id, "proto")
    if val != "RSN":
        raise Exception("Unexpected proto value: " + val)

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()
    dev[0].request("RECONNECT")
    dev[0].wait_connected()

def test_sae_mfp(dev, apdev):
    """SAE and MFP enabled without sae_require_mfp"""
    check_sae_capab(dev[0])
    check_sae_capab(dev[1])
    params = hostapd.wpa2_params(ssid="test-sae", passphrase="12345678")
    params['wpa_key_mgmt'] = 'SAE'
    params["ieee80211w"] = "1"
    hostapd.add_ap(apdev[0], params)

    dev[0].request("SET sae_groups ")
    dev[0].connect("test-sae", psk="12345678", key_mgmt="SAE", ieee80211w="2",
                   scan_freq="2412")

    dev[1].request("SET sae_groups ")
    dev[1].connect("test-sae", psk="12345678", key_mgmt="SAE", ieee80211w="0",
                   scan_freq="2412")

@remote_compatible
def test_sae_missing_password(dev, apdev):
    """SAE and missing password"""
    check_sae_capab(dev[0])
    params = hostapd.wpa2_params(ssid="test-sae",
                                 passphrase="12345678")
    params['wpa_key_mgmt'] = 'SAE'
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].request("SET sae_groups ")
    id = dev[0].connect("test-sae",
                        raw_psk="46b4a73b8a951ad53ebd2e0afdb9c5483257edd4c21d12b7710759da70945858",
                        key_mgmt="SAE", scan_freq="2412", wait_connect=False)
    ev = dev[0].wait_event(['CTRL-EVENT-SSID-TEMP-DISABLED'], timeout=10)
    if ev is None:
        raise Exception("Invalid network not temporarily disabled")


def test_sae_key_lifetime_in_memory(dev, apdev, params):
    """SAE and key lifetime in memory"""
    check_sae_capab(dev[0])
    password = "5ad144a7c1f5a5503baa6fa01dabc15b1843e8c01662d78d16b70b5cd23cf8b"
    p = hostapd.wpa2_params(ssid="test-sae", passphrase=password)
    p['wpa_key_mgmt'] = 'SAE'
    hapd = hostapd.add_ap(apdev[0], p)

    pid = find_wpas_process(dev[0])

    dev[0].request("SET sae_groups ")
    id = dev[0].connect("test-sae", psk=password, key_mgmt="SAE",
                        scan_freq="2412")

    # The decrypted copy of GTK is freed only after the CTRL-EVENT-CONNECTED
    # event has been delivered, so verify that wpa_supplicant has returned to
    # eloop before reading process memory.
    time.sleep(1)
    dev[0].ping()
    password = password.encode()
    buf = read_process_memory(pid, password)

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    dev[0].relog()
    sae_k = None
    sae_keyseed = None
    sae_kck = None
    pmk = None
    ptk = None
    gtk = None
    with open(os.path.join(params['logdir'], 'log0'), 'r') as f:
        for l in f.readlines():
            if "SAE: k - hexdump" in l:
                val = l.strip().split(':')[3].replace(' ', '')
                sae_k = binascii.unhexlify(val)
            if "SAE: keyseed - hexdump" in l:
                val = l.strip().split(':')[3].replace(' ', '')
                sae_keyseed = binascii.unhexlify(val)
            if "SAE: KCK - hexdump" in l:
                val = l.strip().split(':')[3].replace(' ', '')
                sae_kck = binascii.unhexlify(val)
            if "SAE: PMK - hexdump" in l:
                val = l.strip().split(':')[3].replace(' ', '')
                pmk = binascii.unhexlify(val)
            if "WPA: PTK - hexdump" in l:
                val = l.strip().split(':')[3].replace(' ', '')
                ptk = binascii.unhexlify(val)
            if "WPA: Group Key - hexdump" in l:
                val = l.strip().split(':')[3].replace(' ', '')
                gtk = binascii.unhexlify(val)
    if not sae_k or not sae_keyseed or not sae_kck or not pmk or not ptk or not gtk:
        raise Exception("Could not find keys from debug log")
    if len(gtk) != 16:
        raise Exception("Unexpected GTK length")

    kck = ptk[0:16]
    kek = ptk[16:32]
    tk = ptk[32:48]

    fname = os.path.join(params['logdir'],
                         'sae_key_lifetime_in_memory.memctx-')

    logger.info("Checking keys in memory while associated")
    get_key_locations(buf, password, "Password")
    get_key_locations(buf, pmk, "PMK")
    if password not in buf:
        raise HwsimSkip("Password not found while associated")
    if pmk not in buf:
        raise HwsimSkip("PMK not found while associated")
    if kck not in buf:
        raise Exception("KCK not found while associated")
    if kek not in buf:
        raise Exception("KEK not found while associated")
    #if tk in buf:
    #    raise Exception("TK found from memory")
    verify_not_present(buf, sae_k, fname, "SAE(k)")
    verify_not_present(buf, sae_keyseed, fname, "SAE(keyseed)")
    verify_not_present(buf, sae_kck, fname, "SAE(KCK)")

    logger.info("Checking keys in memory after disassociation")
    buf = read_process_memory(pid, password)

    # Note: Password is still present in network configuration
    # Note: PMK is in PMKSA cache

    get_key_locations(buf, password, "Password")
    get_key_locations(buf, pmk, "PMK")
    verify_not_present(buf, kck, fname, "KCK")
    verify_not_present(buf, kek, fname, "KEK")
    verify_not_present(buf, tk, fname, "TK")
    if gtk in buf:
        get_key_locations(buf, gtk, "GTK")
    verify_not_present(buf, gtk, fname, "GTK")
    verify_not_present(buf, sae_k, fname, "SAE(k)")
    verify_not_present(buf, sae_keyseed, fname, "SAE(keyseed)")
    verify_not_present(buf, sae_kck, fname, "SAE(KCK)")

    dev[0].request("PMKSA_FLUSH")
    logger.info("Checking keys in memory after PMKSA cache flush")
    buf = read_process_memory(pid, password)
    get_key_locations(buf, password, "Password")
    get_key_locations(buf, pmk, "PMK")
    verify_not_present(buf, pmk, fname, "PMK")

    dev[0].request("REMOVE_NETWORK all")

    logger.info("Checking keys in memory after network profile removal")
    buf = read_process_memory(pid, password)

    get_key_locations(buf, password, "Password")
    get_key_locations(buf, pmk, "PMK")
    verify_not_present(buf, password, fname, "password")
    verify_not_present(buf, pmk, fname, "PMK")
    verify_not_present(buf, kck, fname, "KCK")
    verify_not_present(buf, kek, fname, "KEK")
    verify_not_present(buf, tk, fname, "TK")
    verify_not_present(buf, gtk, fname, "GTK")
    verify_not_present(buf, sae_k, fname, "SAE(k)")
    verify_not_present(buf, sae_keyseed, fname, "SAE(keyseed)")
    verify_not_present(buf, sae_kck, fname, "SAE(KCK)")

@remote_compatible
def test_sae_oom_wpas(dev, apdev):
    """SAE and OOM in wpa_supplicant"""
    check_sae_capab(dev[0])
    params = hostapd.wpa2_params(ssid="test-sae",
                                 passphrase="12345678")
    params['wpa_key_mgmt'] = 'SAE'
    params['sae_groups'] = '19 25 26 20'
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].request("SET sae_groups 20")
    with alloc_fail(dev[0], 1, "sae_set_group"):
        dev[0].connect("test-sae", psk="12345678", key_mgmt="SAE",
                       scan_freq="2412")
        dev[0].request("REMOVE_NETWORK all")

    dev[0].request("SET sae_groups ")
    with alloc_fail(dev[0], 2, "sae_set_group"):
        dev[0].connect("test-sae", psk="12345678", key_mgmt="SAE",
                       scan_freq="2412")
        dev[0].request("REMOVE_NETWORK all")

    with alloc_fail(dev[0], 1, "wpabuf_alloc;sme_auth_build_sae_commit"):
        dev[0].connect("test-sae", psk="12345678", key_mgmt="SAE",
                       scan_freq="2412")
        dev[0].request("REMOVE_NETWORK all")

    with alloc_fail(dev[0], 1, "wpabuf_alloc;sme_auth_build_sae_confirm"):
        dev[0].connect("test-sae", psk="12345678", key_mgmt="SAE",
                       scan_freq="2412", wait_connect=False)
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
        dev[0].request("REMOVE_NETWORK all")

    with alloc_fail(dev[0], 1, "=sme_authenticate"):
        dev[0].connect("test-sae", psk="12345678", key_mgmt="SAE",
                       scan_freq="2412", wait_connect=False)
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
        dev[0].request("REMOVE_NETWORK all")

    with alloc_fail(dev[0], 1, "radio_add_work;sme_authenticate"):
        dev[0].connect("test-sae", psk="12345678", key_mgmt="SAE",
                       scan_freq="2412", wait_connect=False)
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
        dev[0].request("REMOVE_NETWORK all")

@remote_compatible
def test_sae_proto_ecc(dev, apdev):
    """SAE protocol testing (ECC)"""
    check_sae_capab(dev[0])
    params = hostapd.wpa2_params(ssid="test-sae",
                                 passphrase="12345678")
    params['wpa_key_mgmt'] = 'SAE'
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = apdev[0]['bssid']

    dev[0].request("SET sae_groups 19")

    tests = [("Confirm mismatch",
              "1300" + "033d3635b39666ed427fd4a3e7d37acec2810afeaf1687f746a14163ff0e6d03" + "559cb8928db4ce4e3cbd6555e837591995e5ebe503ef36b503d9ca519d63728dd3c7c676b8e8081831b6bc3a64bdf136061a7de175e17d1965bfa41983ed02f8",
              "0000800edebc3f260dc1fe7e0b20888af2b8a3316252ec37388a8504e25b73dc4240"),
             ("Commit without even full cyclic group field",
              "13",
              None),
             ("Too short commit",
              "1300" + "033d3635b39666ed427fd4a3e7d37acec2810afeaf1687f746a14163ff0e6d03" + "559cb8928db4ce4e3cbd6555e837591995e5ebe503ef36b503d9ca519d63728dd3c7c676b8e8081831b6bc3a64bdf136061a7de175e17d1965bfa41983ed02",
              None),
             ("Invalid commit scalar (0)",
              "1300" + "0000000000000000000000000000000000000000000000000000000000000000" + "559cb8928db4ce4e3cbd6555e837591995e5ebe503ef36b503d9ca519d63728dd3c7c676b8e8081831b6bc3a64bdf136061a7de175e17d1965bfa41983ed02f8",
              None),
             ("Invalid commit scalar (1)",
              "1300" + "0000000000000000000000000000000000000000000000000000000000000001" + "559cb8928db4ce4e3cbd6555e837591995e5ebe503ef36b503d9ca519d63728dd3c7c676b8e8081831b6bc3a64bdf136061a7de175e17d1965bfa41983ed02f8",
              None),
             ("Invalid commit scalar (> r)",
              "1300" + "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff" + "559cb8928db4ce4e3cbd6555e837591995e5ebe503ef36b503d9ca519d63728dd3c7c676b8e8081831b6bc3a64bdf136061a7de175e17d1965bfa41983ed02f8",
              None),
             ("Commit element not on curve",
              "1300" + "033d3635b39666ed427fd4a3e7d37acec2810afeaf1687f746a14163ff0e6d03" + "559cb8928db4ce4e3cbd6555e837591995e5ebe503ef36b503d9ca519d63728d0000000000000000000000000000000000000000000000000000000000000000",
              None),
             ("Invalid commit element (y coordinate > P)",
              "1300" + "033d3635b39666ed427fd4a3e7d37acec2810afeaf1687f746a14163ff0e6d03" + "559cb8928db4ce4e3cbd6555e837591995e5ebe503ef36b503d9ca519d63728dffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
              None),
             ("Invalid commit element (x coordinate > P)",
              "1300" + "033d3635b39666ed427fd4a3e7d37acec2810afeaf1687f746a14163ff0e6d03" + "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffd3c7c676b8e8081831b6bc3a64bdf136061a7de175e17d1965bfa41983ed02f8",
              None),
             ("Different group in commit",
              "1400" + "033d3635b39666ed427fd4a3e7d37acec2810afeaf1687f746a14163ff0e6d03" + "559cb8928db4ce4e3cbd6555e837591995e5ebe503ef36b503d9ca519d63728dd3c7c676b8e8081831b6bc3a64bdf136061a7de175e17d1965bfa41983ed02f8",
              None),
             ("Too short confirm",
              "1300" + "033d3635b39666ed427fd4a3e7d37acec2810afeaf1687f746a14163ff0e6d03" + "559cb8928db4ce4e3cbd6555e837591995e5ebe503ef36b503d9ca519d63728dd3c7c676b8e8081831b6bc3a64bdf136061a7de175e17d1965bfa41983ed02f8",
              "0000800edebc3f260dc1fe7e0b20888af2b8a3316252ec37388a8504e25b73dc42")]
    for (note, commit, confirm) in tests:
        logger.info(note)
        dev[0].scan_for_bss(bssid, freq=2412)
        hapd.set("ext_mgmt_frame_handling", "1")
        dev[0].connect("test-sae", psk="12345678", key_mgmt="SAE",
                       scan_freq="2412", wait_connect=False)

        logger.info("Commit")
        for i in range(0, 10):
            req = hapd.mgmt_rx()
            if req is None:
                raise Exception("MGMT RX wait timed out (commit)")
            if req['subtype'] == 11:
                break
            req = None
        if not req:
            raise Exception("Authentication frame (commit) not received")

        hapd.dump_monitor()
        resp = {}
        resp['fc'] = req['fc']
        resp['da'] = req['sa']
        resp['sa'] = req['da']
        resp['bssid'] = req['bssid']
        resp['payload'] = binascii.unhexlify("030001000000" + commit)
        hapd.mgmt_tx(resp)

        if confirm:
            logger.info("Confirm")
            for i in range(0, 10):
                req = hapd.mgmt_rx()
                if req is None:
                    raise Exception("MGMT RX wait timed out (confirm)")
                if req['subtype'] == 11:
                    break
                req = None
            if not req:
                raise Exception("Authentication frame (confirm) not received")

            hapd.dump_monitor()
            resp = {}
            resp['fc'] = req['fc']
            resp['da'] = req['sa']
            resp['sa'] = req['da']
            resp['bssid'] = req['bssid']
            resp['payload'] = binascii.unhexlify("030002000000" + confirm)
            hapd.mgmt_tx(resp)

        time.sleep(0.1)
        dev[0].request("REMOVE_NETWORK all")
        hapd.set("ext_mgmt_frame_handling", "0")
        hapd.dump_monitor()

@remote_compatible
def test_sae_proto_ffc(dev, apdev):
    """SAE protocol testing (FFC)"""
    check_sae_capab(dev[0])
    params = hostapd.wpa2_params(ssid="test-sae",
                                 passphrase="12345678")
    params['wpa_key_mgmt'] = 'SAE'
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = apdev[0]['bssid']

    dev[0].request("SET sae_groups 2")

    tests = [("Confirm mismatch",
              "0200" + "0c70519d874e3e4930a917cc5e17ea7a26028211159f217bab28b8d6c56691805e49f03249b2c6e22c7c9f86b30e04ccad2deedd5e5108ae07b737c00001c59cd0eb08b1dfc7f1b06a1542e2b6601a963c066e0c65940983a03917ae57a101ce84b5cbbc76ff33ebb990aac2e54aa0f0ab6ec0a58113d927683502b2cb2347d2" + "a8c00117493cdffa5dd671e934bc9cb1a69f39e25e9dd9cd9afd3aea2441a0f5491211c7ba50a753563f9ce943b043557cb71193b28e86ed9544f4289c471bf91b70af5c018cf4663e004165b0fd0bc1d8f3f78adf42eee92bcbc55246fd3ee9f107ab965dc7d4986f23eb71d616ebfe6bfe0a6c1ac5dc1718acee17c9a17486",
              "0000f3116a9731f1259622e3eb55d4b3b50ba16f8c5f5565b28e609b180c51460251"),
             ("Too short commit",
              "0200" + "0c70519d874e3e4930a917cc5e17ea7a26028211159f217bab28b8d6c56691805e49f03249b2c6e22c7c9f86b30e04ccad2deedd5e5108ae07b737c00001c59cd0eb08b1dfc7f1b06a1542e2b6601a963c066e0c65940983a03917ae57a101ce84b5cbbc76ff33ebb990aac2e54aa0f0ab6ec0a58113d927683502b2cb2347d2" + "a8c00117493cdffa5dd671e934bc9cb1a69f39e25e9dd9cd9afd3aea2441a0f5491211c7ba50a753563f9ce943b043557cb71193b28e86ed9544f4289c471bf91b70af5c018cf4663e004165b0fd0bc1d8f3f78adf42eee92bcbc55246fd3ee9f107ab965dc7d4986f23eb71d616ebfe6bfe0a6c1ac5dc1718acee17c9a174",
              None),
             ("Invalid element (0) in commit",
              "0200" + "0c70519d874e3e4930a917cc5e17ea7a26028211159f217bab28b8d6c56691805e49f03249b2c6e22c7c9f86b30e04ccad2deedd5e5108ae07b737c00001c59cd0eb08b1dfc7f1b06a1542e2b6601a963c066e0c65940983a03917ae57a101ce84b5cbbc76ff33ebb990aac2e54aa0f0ab6ec0a58113d927683502b2cb2347d2" + "0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000",
              None),
             ("Invalid element (1) in commit",
              "0200" + "0c70519d874e3e4930a917cc5e17ea7a26028211159f217bab28b8d6c56691805e49f03249b2c6e22c7c9f86b30e04ccad2deedd5e5108ae07b737c00001c59cd0eb08b1dfc7f1b06a1542e2b6601a963c066e0c65940983a03917ae57a101ce84b5cbbc76ff33ebb990aac2e54aa0f0ab6ec0a58113d927683502b2cb2347d2" + "0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001",
              None),
             ("Invalid element (> P) in commit",
              "0200" + "0c70519d874e3e4930a917cc5e17ea7a26028211159f217bab28b8d6c56691805e49f03249b2c6e22c7c9f86b30e04ccad2deedd5e5108ae07b737c00001c59cd0eb08b1dfc7f1b06a1542e2b6601a963c066e0c65940983a03917ae57a101ce84b5cbbc76ff33ebb990aac2e54aa0f0ab6ec0a58113d927683502b2cb2347d2" + "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
              None)]
    for (note, commit, confirm) in tests:
        logger.info(note)
        dev[0].scan_for_bss(bssid, freq=2412)
        hapd.set("ext_mgmt_frame_handling", "1")
        dev[0].connect("test-sae", psk="12345678", key_mgmt="SAE",
                       scan_freq="2412", wait_connect=False)

        logger.info("Commit")
        for i in range(0, 10):
            req = hapd.mgmt_rx()
            if req is None:
                raise Exception("MGMT RX wait timed out (commit)")
            if req['subtype'] == 11:
                break
            req = None
        if not req:
            raise Exception("Authentication frame (commit) not received")

        hapd.dump_monitor()
        resp = {}
        resp['fc'] = req['fc']
        resp['da'] = req['sa']
        resp['sa'] = req['da']
        resp['bssid'] = req['bssid']
        resp['payload'] = binascii.unhexlify("030001000000" + commit)
        hapd.mgmt_tx(resp)

        if confirm:
            logger.info("Confirm")
            for i in range(0, 10):
                req = hapd.mgmt_rx()
                if req is None:
                    raise Exception("MGMT RX wait timed out (confirm)")
                if req['subtype'] == 11:
                    break
                req = None
            if not req:
                raise Exception("Authentication frame (confirm) not received")

            hapd.dump_monitor()
            resp = {}
            resp['fc'] = req['fc']
            resp['da'] = req['sa']
            resp['sa'] = req['da']
            resp['bssid'] = req['bssid']
            resp['payload'] = binascii.unhexlify("030002000000" + confirm)
            hapd.mgmt_tx(resp)

        time.sleep(0.1)
        dev[0].request("REMOVE_NETWORK all")
        hapd.set("ext_mgmt_frame_handling", "0")
        hapd.dump_monitor()


def test_sae_proto_commit_delayed(dev, apdev):
    """SAE protocol testing - Commit delayed"""
    check_sae_capab(dev[0])
    params = hostapd.wpa2_params(ssid="test-sae",
                                 passphrase="12345678")
    params['wpa_key_mgmt'] = 'SAE'
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = apdev[0]['bssid']

    dev[0].request("SET sae_groups 19")

    dev[0].scan_for_bss(bssid, freq=2412)
    hapd.set("ext_mgmt_frame_handling", "1")
    dev[0].connect("test-sae", psk="12345678", key_mgmt="SAE",
                   scan_freq="2412", wait_connect=False)

    logger.info("Commit")
    for i in range(0, 10):
        req = hapd.mgmt_rx()
        if req is None:
            raise Exception("MGMT RX wait timed out (commit)")
        if req['subtype'] == 11:
            break
        req = None
    if not req:
        raise Exception("Authentication frame (commit) not received")

    hapd.dump_monitor()
    time.sleep(2.5)
    hapd.request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=" + binascii.hexlify(req['frame']).decode())

    logger.info("Commit/Confirm")
    for i in range(0, 10):
        req = hapd.mgmt_rx()
        if req is None:
            raise Exception("MGMT RX wait timed out (confirm)")
        if req['subtype'] == 11:
            trans, = struct.unpack('<H', req['payload'][2:4])
            if trans == 1:
                logger.info("Extra Commit")
                hapd.request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=" + binascii.hexlify(req['frame']).decode())
                continue
            break
        req = None
    if not req:
        raise Exception("Authentication frame (confirm) not received")

    hapd.dump_monitor()
    hapd.request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=" + binascii.hexlify(req['frame']).decode())

    logger.info("Association Request")
    for i in range(0, 10):
        req = hapd.mgmt_rx()
        if req is None:
            raise Exception("MGMT RX wait timed out (AssocReq)")
        if req['subtype'] == 0:
            break
        req = None
    if not req:
        raise Exception("Association Request frame not received")

    hapd.dump_monitor()
    hapd.request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=" + binascii.hexlify(req['frame']).decode())
    ev = hapd.wait_event(["MGMT-TX-STATUS"], timeout=5)
    if ev is None:
        raise Exception("Management frame TX status not reported (1)")
    if "stype=1 ok=1" not in ev:
        raise Exception("Unexpected management frame TX status (1): " + ev)
    cmd = "MGMT_TX_STATUS_PROCESS %s" % (" ".join(ev.split(' ')[1:4]))
    if "OK" not in hapd.request(cmd):
        raise Exception("MGMT_TX_STATUS_PROCESS failed")

    hapd.set("ext_mgmt_frame_handling", "0")

    dev[0].wait_connected()

def test_sae_proto_commit_replay(dev, apdev):
    """SAE protocol testing - Commit replay"""
    check_sae_capab(dev[0])
    params = hostapd.wpa2_params(ssid="test-sae",
                                 passphrase="12345678")
    params['wpa_key_mgmt'] = 'SAE'
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = apdev[0]['bssid']

    dev[0].request("SET sae_groups 19")

    dev[0].scan_for_bss(bssid, freq=2412)
    hapd.set("ext_mgmt_frame_handling", "1")
    dev[0].connect("test-sae", psk="12345678", key_mgmt="SAE",
                   scan_freq="2412", wait_connect=False)

    logger.info("Commit")
    for i in range(0, 10):
        req = hapd.mgmt_rx()
        if req is None:
            raise Exception("MGMT RX wait timed out (commit)")
        if req['subtype'] == 11:
            break
        req = None
    if not req:
        raise Exception("Authentication frame (commit) not received")

    hapd.dump_monitor()
    hapd.request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=" + binascii.hexlify(req['frame']).decode())
    logger.info("Replay Commit")
    hapd.request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=" + binascii.hexlify(req['frame']).decode())

    logger.info("Confirm")
    for i in range(0, 10):
        req = hapd.mgmt_rx()
        if req is None:
            raise Exception("MGMT RX wait timed out (confirm)")
        if req['subtype'] == 11:
            trans, = struct.unpack('<H', req['payload'][2:4])
            if trans == 1:
                logger.info("Extra Commit")
                hapd.request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=" + binascii.hexlify(req['frame']).decode())
                continue
            break
        req = None
    if not req:
        raise Exception("Authentication frame (confirm) not received")

    hapd.dump_monitor()
    hapd.request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=" + binascii.hexlify(req['frame']).decode())

    logger.info("Association Request")
    for i in range(0, 10):
        req = hapd.mgmt_rx()
        if req is None:
            raise Exception("MGMT RX wait timed out (AssocReq)")
        if req['subtype'] == 0:
            break
        req = None
    if not req:
        raise Exception("Association Request frame not received")

    hapd.dump_monitor()
    hapd.request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=" + binascii.hexlify(req['frame']).decode())
    for i in range(0, 10):
        ev = hapd.wait_event(["MGMT-TX-STATUS"], timeout=5)
        if ev is None:
            raise Exception("Management frame TX status not reported (1)")
        if "stype=11 ok=1" in ev:
            continue
        if "stype=12 ok=1" in ev:
            continue
        if "stype=1 ok=1" not in ev:
            raise Exception("Unexpected management frame TX status (1): " + ev)
        cmd = "MGMT_TX_STATUS_PROCESS %s" % (" ".join(ev.split(' ')[1:4]))
        if "OK" not in hapd.request(cmd):
            raise Exception("MGMT_TX_STATUS_PROCESS failed")
        break

    hapd.set("ext_mgmt_frame_handling", "0")

    dev[0].wait_connected()

def test_sae_proto_confirm_replay(dev, apdev):
    """SAE protocol testing - Confirm replay"""
    check_sae_capab(dev[0])
    params = hostapd.wpa2_params(ssid="test-sae",
                                 passphrase="12345678")
    params['wpa_key_mgmt'] = 'SAE'
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = apdev[0]['bssid']

    dev[0].request("SET sae_groups 19")

    dev[0].scan_for_bss(bssid, freq=2412)
    hapd.set("ext_mgmt_frame_handling", "1")
    dev[0].connect("test-sae", psk="12345678", key_mgmt="SAE",
                   scan_freq="2412", wait_connect=False)

    logger.info("Commit")
    for i in range(0, 10):
        req = hapd.mgmt_rx()
        if req is None:
            raise Exception("MGMT RX wait timed out (commit)")
        if req['subtype'] == 11:
            break
        req = None
    if not req:
        raise Exception("Authentication frame (commit) not received")

    hapd.dump_monitor()
    hapd.request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=" + binascii.hexlify(req['frame']).decode())

    logger.info("Confirm")
    for i in range(0, 10):
        req = hapd.mgmt_rx()
        if req is None:
            raise Exception("MGMT RX wait timed out (confirm)")
        if req['subtype'] == 11:
            break
        req = None
    if not req:
        raise Exception("Authentication frame (confirm) not received")

    hapd.dump_monitor()
    hapd.request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=" + binascii.hexlify(req['frame']).decode())

    logger.info("Replay Confirm")
    hapd.request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=" + binascii.hexlify(req['frame']).decode())

    logger.info("Association Request")
    for i in range(0, 10):
        req = hapd.mgmt_rx()
        if req is None:
            raise Exception("MGMT RX wait timed out (AssocReq)")
        if req['subtype'] == 0:
            break
        req = None
    if not req:
        raise Exception("Association Request frame not received")

    hapd.dump_monitor()
    hapd.request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=" + binascii.hexlify(req['frame']).decode())
    ev = hapd.wait_event(["MGMT-TX-STATUS"], timeout=5)
    if ev is None:
        raise Exception("Management frame TX status not reported (1)")
    if "stype=1 ok=1" not in ev:
        raise Exception("Unexpected management frame TX status (1): " + ev)
    cmd = "MGMT_TX_STATUS_PROCESS %s" % (" ".join(ev.split(' ')[1:4]))
    if "OK" not in hapd.request(cmd):
        raise Exception("MGMT_TX_STATUS_PROCESS failed")

    hapd.set("ext_mgmt_frame_handling", "0")

    dev[0].wait_connected()

def test_sae_proto_hostapd(dev, apdev):
    """SAE protocol testing with hostapd"""
    params = hostapd.wpa2_params(ssid="test-sae", passphrase="12345678")
    params['wpa_key_mgmt'] = 'SAE'
    params['sae_groups'] = "19 65535"
    hapd = hostapd.add_ap(apdev[0], params)
    hapd.set("ext_mgmt_frame_handling", "1")
    bssid = hapd.own_addr().replace(':', '')
    addr = "020000000000"
    addr2 = "020000000001"
    hdr = "b0003a01" + bssid + addr + bssid + "1000"
    hdr2 = "b0003a01" + bssid + addr2 + bssid + "1000"
    group = "1300"
    scalar = "f7df19f4a7fef1d3b895ea1de150b7c5a7a705c8ebb31a52b623e0057908bd93"
    element_x = "21931572027f2e953e2a49fab3d992944102cc95aa19515fc068b394fb25ae3c"
    element_y = "cb4eeb94d7b0b789abfdb73a67ab9d6d5efa94dd553e0e724a6289821cbce530"
    hapd.request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=" + hdr + "030001000000" + group + scalar + element_x + element_y)
    # "SAE: Not enough data for scalar"
    hapd.request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=" + hdr + "030001000000" + group + scalar[:-2])
    # "SAE: Do not allow group to be changed"
    hapd.request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=" + hdr + "030001000000" + "ffff" + scalar[:-2])
    # "SAE: Unsupported Finite Cyclic Group 65535"
    hapd.request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=" + hdr2 + "030001000000" + "ffff" + scalar[:-2])

def test_sae_proto_hostapd_ecc(dev, apdev):
    """SAE protocol testing with hostapd (ECC)"""
    params = hostapd.wpa2_params(ssid="test-sae", passphrase="foofoofoo")
    params['wpa_key_mgmt'] = 'SAE'
    params['sae_groups'] = "19"
    hapd = hostapd.add_ap(apdev[0], params)
    hapd.set("ext_mgmt_frame_handling", "1")
    bssid = hapd.own_addr().replace(':', '')
    addr = "020000000000"
    addr2 = "020000000001"
    hdr = "b0003a01" + bssid + addr + bssid + "1000"
    hdr2 = "b0003a01" + bssid + addr2 + bssid + "1000"
    group = "1300"
    scalar = "9e9a959bf2dda875a4a29ce9b2afef46f2d83060930124cd9e39ddce798cd69a"
    element_x = "dfc55fd8622b91d362f4d1fc9646474d7fba0ff7cce6ca58b8e96a931e070220"
    element_y = "dac8a4e80724f167c1349cc9e1f9dd82a7c77b29d49789b63b72b4c849301a28"
    # sae_parse_commit_element_ecc() failure to parse peer element
    # (depending on crypto library, either crypto_ec_point_from_bin() failure
    # or crypto_ec_point_is_on_curve() returning 0)
    hapd.request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=" + hdr + "030001000000" + group + scalar + element_x + element_y)
    # Unexpected continuation of the connection attempt with confirm
    hapd.request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=" + hdr + "030002000000" + "0000" + "fd7b081ff4e8676f03612a4140eedcd3c179ab3a13b93863c6f7ca451340b9ae")

def test_sae_proto_hostapd_ffc(dev, apdev):
    """SAE protocol testing with hostapd (FFC)"""
    params = hostapd.wpa2_params(ssid="test-sae", passphrase="foofoofoo")
    params['wpa_key_mgmt'] = 'SAE'
    params['sae_groups'] = "22"
    hapd = hostapd.add_ap(apdev[0], params)
    hapd.set("ext_mgmt_frame_handling", "1")
    bssid = hapd.own_addr().replace(':', '')
    addr = "020000000000"
    addr2 = "020000000001"
    hdr = "b0003a01" + bssid + addr + bssid + "1000"
    hdr2 = "b0003a01" + bssid + addr2 + bssid + "1000"
    group = "1600"
    scalar = "00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000044cc46a73c07ef479dc66ec1f5e8ccf25131fa40"
    element = "0f1d67025e12fc874cf718c35b19d1ab2db858215623f1ce661cbd1d7b1d7a09ceda7dba46866cf37044259b5cac4db15e7feb778edc8098854b93a84347c1850c02ee4d7dac46db79c477c731085d5b39f56803cda1eeac4a2fbbccb9a546379e258c00ebe93dfdd0a34cf8ce5c55cf905a89564a590b7e159fb89198e9d5cd"
    # sae_parse_commit_element_ffc() failure to parse peer element
    hapd.request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=" + hdr + "030001000000" + group + scalar + element)
    # Unexpected continuation of the connection attempt with confirm
    hapd.request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=" + hdr + "030002000000" + "0000" + "fd7b081ff4e8676f03612a4140eedcd3c179ab3a13b93863c6f7ca451340b9ae")

def sae_start_ap(apdev, sae_pwe):
    params = hostapd.wpa2_params(ssid="test-sae", passphrase="foofoofoo")
    params['wpa_key_mgmt'] = 'SAE'
    params['sae_groups'] = "19"
    params['sae_pwe'] = str(sae_pwe)
    return hostapd.add_ap(apdev, params)

def check_commit_status(hapd, use_status, expect_status):
    hapd.set("ext_mgmt_frame_handling", "1")
    bssid = hapd.own_addr().replace(':', '')
    addr = "020000000000"
    addr2 = "020000000001"
    hdr = "b0003a01" + bssid + addr + bssid + "1000"
    hdr2 = "b0003a01" + bssid + addr2 + bssid + "1000"
    group = "1300"
    scalar = "033d3635b39666ed427fd4a3e7d37acec2810afeaf1687f746a14163ff0e6d03"
    element_x = "559cb8928db4ce4e3cbd6555e837591995e5ebe503ef36b503d9ca519d63728d"
    element_y = "d3c7c676b8e8081831b6bc3a64bdf136061a7de175e17d1965bfa41983ed02f8"
    status = binascii.hexlify(struct.pack('<H', use_status)).decode()
    hapd.request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=" + hdr + "03000100" + status + group + scalar + element_x + element_y)
    ev = hapd.wait_event(["MGMT-TX-STATUS"], timeout=5)
    if ev is None:
        raise Exception("MGMT-TX-STATUS not seen")
    msg = ev.split(' ')[3].split('=')[1]
    body = msg[2 * 24:]
    status, = struct.unpack('<H', binascii.unhexlify(body[8:12]))
    if status != expect_status:
        raise Exception("Unexpected status code: %d" % status)

def test_sae_proto_hostapd_status_126(dev, apdev):
    """SAE protocol testing with hostapd (status code 126)"""
    hapd = sae_start_ap(apdev[0], 0)
    check_commit_status(hapd, 126, 1)
    check_commit_status(hapd, 0, 0)

def test_sae_proto_hostapd_status_127(dev, apdev):
    """SAE protocol testing with hostapd (status code 127)"""
    hapd = sae_start_ap(apdev[0], 2)
    check_commit_status(hapd, 127, 1)
    check_commit_status(hapd, 0, 0)

@remote_compatible
def test_sae_no_ffc_by_default(dev, apdev):
    """SAE and default groups rejecting FFC"""
    check_sae_capab(dev[0])
    params = hostapd.wpa2_params(ssid="test-sae", passphrase="12345678")
    params['wpa_key_mgmt'] = 'SAE'
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].request("SET sae_groups 15")
    dev[0].connect("test-sae", psk="12345678", key_mgmt="SAE", scan_freq="2412",
                   wait_connect=False)
    ev = dev[0].wait_event(["SME: Trying to authenticate"], timeout=3)
    if ev is None:
        raise Exception("Did not try to authenticate")
    ev = dev[0].wait_event(["SME: Trying to authenticate"], timeout=3)
    if ev is None:
        raise Exception("Did not try to authenticate (2)")
    dev[0].request("REMOVE_NETWORK all")

def sae_reflection_attack(apdev, dev, group):
    check_sae_capab(dev)
    params = hostapd.wpa2_params(ssid="test-sae",
                                 passphrase="no-knowledge-of-passphrase")
    params['wpa_key_mgmt'] = 'SAE'
    hapd = hostapd.add_ap(apdev, params)
    bssid = apdev['bssid']

    dev.scan_for_bss(bssid, freq=2412)
    hapd.set("ext_mgmt_frame_handling", "1")

    dev.request("SET sae_groups %d" % group)
    dev.connect("test-sae", psk="reflection-attack", key_mgmt="SAE",
                scan_freq="2412", wait_connect=False)

    # Commit
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
    resp['payload'] = req['payload']
    hapd.mgmt_tx(resp)

    # Confirm
    req = hapd.mgmt_rx(timeout=0.5)
    if req is not None:
        if req['subtype'] == 11:
            raise Exception("Unexpected Authentication frame seen")

@remote_compatible
def test_sae_reflection_attack_ecc(dev, apdev):
    """SAE reflection attack (ECC)"""
    sae_reflection_attack(apdev[0], dev[0], 19)

@remote_compatible
def test_sae_reflection_attack_ffc(dev, apdev):
    """SAE reflection attack (FFC)"""
    sae_reflection_attack(apdev[0], dev[0], 15)

def sae_reflection_attack_internal(apdev, dev, group):
    check_sae_capab(dev)
    params = hostapd.wpa2_params(ssid="test-sae",
                                 passphrase="no-knowledge-of-passphrase")
    params['wpa_key_mgmt'] = 'SAE'
    params['sae_reflection_attack'] = '1'
    hapd = hostapd.add_ap(apdev, params)
    bssid = apdev['bssid']

    dev.scan_for_bss(bssid, freq=2412)
    dev.request("SET sae_groups %d" % group)
    dev.connect("test-sae", psk="reflection-attack", key_mgmt="SAE",
                scan_freq="2412", wait_connect=False)
    ev = dev.wait_event(["SME: Trying to authenticate"], timeout=10)
    if ev is None:
        raise Exception("No authentication attempt seen")
    ev = dev.wait_event(["CTRL-EVENT-CONNECTED"], timeout=1)
    if ev is not None:
        raise Exception("Unexpected connection")

@remote_compatible
def test_sae_reflection_attack_ecc_internal(dev, apdev):
    """SAE reflection attack (ECC) - internal"""
    sae_reflection_attack_internal(apdev[0], dev[0], 19)

@remote_compatible
def test_sae_reflection_attack_ffc_internal(dev, apdev):
    """SAE reflection attack (FFC) - internal"""
    sae_reflection_attack_internal(apdev[0], dev[0], 15)

@remote_compatible
def test_sae_commit_override(dev, apdev):
    """SAE commit override (hostapd)"""
    check_sae_capab(dev[0])
    params = hostapd.wpa2_params(ssid="test-sae",
                                 passphrase="12345678")
    params['wpa_key_mgmt'] = 'SAE'
    params['sae_commit_override'] = '13ffbad00d215867a7c5ff37d87bb9bdb7cb116e520f71e8d7a794ca2606d537ddc6c099c40e7a25372b80a8fd443cd7dd222c8ea21b8ef372d4b3e316c26a73fd999cc79ad483eb826e7b3893ea332da68fa13224bcdeb4fb18b0584dd100a2c514'
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].request("SET sae_groups ")
    dev[0].connect("test-sae", psk="test-sae", key_mgmt="SAE",
                   scan_freq="2412", wait_connect=False)
    ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED"], timeout=1)
    if ev is not None:
        raise Exception("Unexpected connection")

@remote_compatible
def test_sae_commit_override2(dev, apdev):
    """SAE commit override (wpa_supplicant)"""
    check_sae_capab(dev[0])
    params = hostapd.wpa2_params(ssid="test-sae",
                                 passphrase="12345678")
    params['wpa_key_mgmt'] = 'SAE'
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].request("SET sae_groups ")
    dev[0].set('sae_commit_override', '13ffbad00d215867a7c5ff37d87bb9bdb7cb116e520f71e8d7a794ca2606d537ddc6c099c40e7a25372b80a8fd443cd7dd222c8ea21b8ef372d4b3e316c26a73fd999cc79ad483eb826e7b3893ea332da68fa13224bcdeb4fb18b0584dd100a2c514')
    dev[0].connect("test-sae", psk="test-sae", key_mgmt="SAE",
                   scan_freq="2412", wait_connect=False)
    ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED"], timeout=1)
    if ev is not None:
        raise Exception("Unexpected connection")

def test_sae_commit_invalid_scalar_element_ap(dev, apdev):
    """SAE commit invalid scalar/element from AP"""
    check_sae_capab(dev[0])
    params = hostapd.wpa2_params(ssid="test-sae",
                                 passphrase="12345678")
    params['wpa_key_mgmt'] = 'SAE'
    params['sae_commit_override'] = '1300' + 96*'00'
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].request("SET sae_groups ")
    dev[0].connect("test-sae", psk="test-sae", key_mgmt="SAE",
                   scan_freq="2412", wait_connect=False)
    ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED"], timeout=1)
    if ev is not None:
        raise Exception("Unexpected connection")

def test_sae_commit_invalid_element_ap(dev, apdev):
    """SAE commit invalid element from AP"""
    check_sae_capab(dev[0])
    params = hostapd.wpa2_params(ssid="test-sae",
                                 passphrase="12345678")
    params['wpa_key_mgmt'] = 'SAE'
    params['sae_commit_override'] = '1300' + 31*'00' + '02' + 64*'00'
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].request("SET sae_groups ")
    dev[0].connect("test-sae", psk="test-sae", key_mgmt="SAE",
                   scan_freq="2412", wait_connect=False)
    ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED"], timeout=1)
    if ev is not None:
        raise Exception("Unexpected connection")

def test_sae_commit_invalid_scalar_element_sta(dev, apdev):
    """SAE commit invalid scalar/element from STA"""
    check_sae_capab(dev[0])
    params = hostapd.wpa2_params(ssid="test-sae",
                                 passphrase="12345678")
    params['wpa_key_mgmt'] = 'SAE'
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].request("SET sae_groups ")
    dev[0].set('sae_commit_override', '1300' + 96*'00')
    dev[0].connect("test-sae", psk="test-sae", key_mgmt="SAE",
                   scan_freq="2412", wait_connect=False)
    ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED"], timeout=1)
    if ev is not None:
        raise Exception("Unexpected connection")

def test_sae_commit_invalid_element_sta(dev, apdev):
    """SAE commit invalid element from STA"""
    check_sae_capab(dev[0])
    params = hostapd.wpa2_params(ssid="test-sae",
                                 passphrase="12345678")
    params['wpa_key_mgmt'] = 'SAE'
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].request("SET sae_groups ")
    dev[0].set('sae_commit_override', '1300' + 31*'00' + '02' + 64*'00')
    dev[0].connect("test-sae", psk="test-sae", key_mgmt="SAE",
                   scan_freq="2412", wait_connect=False)
    ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED"], timeout=1)
    if ev is not None:
        raise Exception("Unexpected connection")

@remote_compatible
def test_sae_anti_clogging_proto(dev, apdev):
    """SAE anti clogging protocol testing"""
    check_sae_capab(dev[0])
    params = hostapd.wpa2_params(ssid="test-sae",
                                 passphrase="no-knowledge-of-passphrase")
    params['wpa_key_mgmt'] = 'SAE'
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = apdev[0]['bssid']

    dev[0].scan_for_bss(bssid, freq=2412)
    hapd.set("ext_mgmt_frame_handling", "1")

    dev[0].request("SET sae_groups ")
    dev[0].connect("test-sae", psk="anti-cloggign", key_mgmt="SAE",
                   scan_freq="2412", wait_connect=False)

    # Commit
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
    resp['payload'] = binascii.unhexlify("030001004c00" + "ffff00")
    hapd.mgmt_tx(resp)

    # Confirm (not received due to DH group being rejected)
    req = hapd.mgmt_rx(timeout=0.5)
    if req is not None:
        if req['subtype'] == 11:
            raise Exception("Unexpected Authentication frame seen")

@remote_compatible
def test_sae_no_random(dev, apdev):
    """SAE and no random numbers available"""
    check_sae_capab(dev[0])
    params = hostapd.wpa2_params(ssid="test-sae", passphrase="12345678")
    params['wpa_key_mgmt'] = 'SAE'
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].request("SET sae_groups ")
    tests = [(1, "os_get_random;sae_derive_pwe_ecc")]
    for count, func in tests:
        with fail_test(dev[0], count, func):
            dev[0].connect("test-sae", psk="12345678", key_mgmt="SAE",
                           scan_freq="2412")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

@remote_compatible
def test_sae_pwe_failure(dev, apdev):
    """SAE and pwe failure"""
    check_sae_capab(dev[0])
    params = hostapd.wpa2_params(ssid="test-sae", passphrase="12345678")
    params['wpa_key_mgmt'] = 'SAE'
    params['sae_groups'] = '19 15'
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].request("SET sae_groups 19")
    with fail_test(dev[0], 1, "hmac_sha256_vector;sae_derive_pwe_ecc"):
        dev[0].connect("test-sae", psk="12345678", key_mgmt="SAE",
                       scan_freq="2412")
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()
    with fail_test(dev[0], 1, "sae_test_pwd_seed_ecc"):
        dev[0].connect("test-sae", psk="12345678", key_mgmt="SAE",
                       scan_freq="2412")
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()

    dev[0].request("SET sae_groups 15")
    with fail_test(dev[0], 1, "hmac_sha256_vector;sae_derive_pwe_ffc"):
        dev[0].connect("test-sae", psk="12345678", key_mgmt="SAE",
                       scan_freq="2412")
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()

    dev[0].request("SET sae_groups 15")
    with fail_test(dev[0], 1, "sae_test_pwd_seed_ffc"):
        dev[0].connect("test-sae", psk="12345678", key_mgmt="SAE",
                       scan_freq="2412")
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()
    with fail_test(dev[0], 2, "sae_test_pwd_seed_ffc"):
        dev[0].connect("test-sae", psk="12345678", key_mgmt="SAE",
                       scan_freq="2412")
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()

@remote_compatible
def test_sae_bignum_failure(dev, apdev):
    """SAE and bignum failure"""
    check_sae_capab(dev[0])
    params = hostapd.wpa2_params(ssid="test-sae", passphrase="12345678")
    params['wpa_key_mgmt'] = 'SAE'
    params['sae_groups'] = '19 15 22'
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].request("SET sae_groups 19")
    tests = [(1, "crypto_bignum_init_set;dragonfly_get_rand_1_to_p_1"),
             (1, "crypto_bignum_init;dragonfly_is_quadratic_residue_blind"),
             (1, "crypto_bignum_mulmod;dragonfly_is_quadratic_residue_blind"),
             (2, "crypto_bignum_mulmod;dragonfly_is_quadratic_residue_blind"),
             (3, "crypto_bignum_mulmod;dragonfly_is_quadratic_residue_blind"),
             (1, "crypto_bignum_legendre;dragonfly_is_quadratic_residue_blind"),
             (1, "crypto_bignum_init_set;sae_test_pwd_seed_ecc"),
             (1, "crypto_ec_point_compute_y_sqr;sae_test_pwd_seed_ecc"),
             (1, "crypto_bignum_to_bin;sae_derive_pwe_ecc"),
             (1, "crypto_ec_point_init;sae_derive_pwe_ecc"),
             (1, "crypto_ec_point_solve_y_coord;sae_derive_pwe_ecc"),
             (1, "crypto_ec_point_init;sae_derive_commit_element_ecc"),
             (1, "crypto_ec_point_mul;sae_derive_commit_element_ecc"),
             (1, "crypto_ec_point_invert;sae_derive_commit_element_ecc"),
             (1, "crypto_bignum_init;=sae_derive_commit"),
             (1, "crypto_ec_point_init;sae_derive_k_ecc"),
             (1, "crypto_ec_point_mul;sae_derive_k_ecc"),
             (1, "crypto_ec_point_add;sae_derive_k_ecc"),
             (2, "crypto_ec_point_mul;sae_derive_k_ecc"),
             (1, "crypto_ec_point_to_bin;sae_derive_k_ecc"),
             (1, "crypto_bignum_legendre;dragonfly_get_random_qr_qnr"),
             (1, "sha256_prf;sae_derive_keys"),
             (1, "crypto_bignum_init;sae_derive_keys"),
             (1, "crypto_bignum_init_set;sae_parse_commit_scalar"),
             (1, "crypto_bignum_to_bin;sae_parse_commit_element_ecc"),
             (1, "crypto_ec_point_from_bin;sae_parse_commit_element_ecc")]
    for count, func in tests:
        with fail_test(dev[0], count, func):
            hapd.request("NOTE STA failure testing %d:%s" % (count, func))
            dev[0].connect("test-sae", psk="12345678", key_mgmt="SAE",
                           scan_freq="2412", wait_connect=False)
            wait_fail_trigger(dev[0], "GET_FAIL", timeout=0.1)
            dev[0].request("REMOVE_NETWORK all")
            dev[0].dump_monitor()
            hapd.dump_monitor()

    dev[0].request("SET sae_groups 15")
    tests = [(1, "crypto_bignum_init_set;sae_set_group"),
             (2, "crypto_bignum_init_set;sae_set_group"),
             (1, "crypto_bignum_init;sae_derive_commit"),
             (2, "crypto_bignum_init;sae_derive_commit"),
             (1, "crypto_bignum_init_set;sae_test_pwd_seed_ffc"),
             (1, "crypto_bignum_exptmod;sae_test_pwd_seed_ffc"),
             (1, "crypto_bignum_init;sae_derive_pwe_ffc"),
             (1, "crypto_bignum_init;sae_derive_commit_element_ffc"),
             (1, "crypto_bignum_exptmod;sae_derive_commit_element_ffc"),
             (1, "crypto_bignum_inverse;sae_derive_commit_element_ffc"),
             (1, "crypto_bignum_init;sae_derive_k_ffc"),
             (1, "crypto_bignum_exptmod;sae_derive_k_ffc"),
             (1, "crypto_bignum_mulmod;sae_derive_k_ffc"),
             (2, "crypto_bignum_exptmod;sae_derive_k_ffc"),
             (1, "crypto_bignum_to_bin;sae_derive_k_ffc"),
             (1, "crypto_bignum_init_set;sae_parse_commit_element_ffc"),
             (1, "crypto_bignum_init;sae_parse_commit_element_ffc"),
             (2, "crypto_bignum_init_set;sae_parse_commit_element_ffc"),
             (1, "crypto_bignum_exptmod;sae_parse_commit_element_ffc")]
    for count, func in tests:
        with fail_test(dev[0], count, func):
            hapd.request("NOTE STA failure testing %d:%s" % (count, func))
            dev[0].connect("test-sae", psk="12345678", key_mgmt="SAE",
                           scan_freq="2412", wait_connect=False)
            wait_fail_trigger(dev[0], "GET_FAIL", timeout=0.1)
            dev[0].request("REMOVE_NETWORK all")
            dev[0].dump_monitor()
            hapd.dump_monitor()

def test_sae_bignum_failure_unsafe_group(dev, apdev):
    """SAE and bignum failure unsafe group"""
    check_sae_capab(dev[0])
    params = hostapd.wpa2_params(ssid="test-sae", passphrase="12345678")
    params['wpa_key_mgmt'] = 'SAE'
    params['sae_groups'] = '22'
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].request("SET sae_groups 22")
    tests = [(1, "crypto_bignum_init_set;sae_test_pwd_seed_ffc"),
             (1, "crypto_bignum_sub;sae_test_pwd_seed_ffc"),
             (1, "crypto_bignum_div;sae_test_pwd_seed_ffc")]
    for count, func in tests:
        with fail_test(dev[0], count, func):
            hapd.request("NOTE STA failure testing %d:%s" % (count, func))
            dev[0].connect("test-sae", psk="12345678", key_mgmt="SAE",
                           scan_freq="2412", wait_connect=False)
            wait_fail_trigger(dev[0], "GET_FAIL")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].dump_monitor()
            hapd.dump_monitor()

def test_sae_invalid_anti_clogging_token_req(dev, apdev):
    """SAE and invalid anti-clogging token request"""
    check_sae_capab(dev[0])
    params = hostapd.wpa2_params(ssid="test-sae", passphrase="12345678")
    params['wpa_key_mgmt'] = 'SAE'
    # Beacon more frequently since Probe Request frames are practically ignored
    # in this test setup (ext_mgmt_frame_handled=1 on hostapd side) and
    # wpa_supplicant scans may end up getting ignored if no new results are
    # available due to the missing Probe Response frames.
    params['beacon_int'] = '20'
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = apdev[0]['bssid']

    dev[0].request("SET sae_groups 19")
    dev[0].scan_for_bss(bssid, freq=2412)
    hapd.set("ext_mgmt_frame_handling", "1")
    dev[0].connect("test-sae", psk="12345678", key_mgmt="SAE",
                   scan_freq="2412", wait_connect=False)
    ev = dev[0].wait_event(["SME: Trying to authenticate"])
    if ev is None:
        raise Exception("No authentication attempt seen (1)")
    dev[0].dump_monitor()

    for i in range(0, 10):
        req = hapd.mgmt_rx()
        if req is None:
            raise Exception("MGMT RX wait timed out (commit)")
        if req['subtype'] == 11:
            break
        req = None
    if not req:
        raise Exception("Authentication frame (commit) not received")

    hapd.dump_monitor()
    resp = {}
    resp['fc'] = req['fc']
    resp['da'] = req['sa']
    resp['sa'] = req['da']
    resp['bssid'] = req['bssid']
    resp['payload'] = binascii.unhexlify("030001004c0013")
    hapd.mgmt_tx(resp)
    ev = hapd.wait_event(["MGMT-TX-STATUS"], timeout=5)
    if ev is None:
        raise Exception("Management frame TX status not reported (1)")
    if "stype=11 ok=1" not in ev:
        raise Exception("Unexpected management frame TX status (1): " + ev)

    ev = dev[0].wait_event(["SME: Trying to authenticate"])
    if ev is None:
        raise Exception("No authentication attempt seen (2)")
    dev[0].dump_monitor()

    for i in range(0, 10):
        req = hapd.mgmt_rx()
        if req is None:
            raise Exception("MGMT RX wait timed out (commit) (2)")
        if req['subtype'] == 11:
            break
        req = None
    if not req:
        raise Exception("Authentication frame (commit) not received (2)")

    hapd.dump_monitor()
    resp = {}
    resp['fc'] = req['fc']
    resp['da'] = req['sa']
    resp['sa'] = req['da']
    resp['bssid'] = req['bssid']
    resp['payload'] = binascii.unhexlify("030001000100")
    hapd.mgmt_tx(resp)
    ev = hapd.wait_event(["MGMT-TX-STATUS"], timeout=5)
    if ev is None:
        raise Exception("Management frame TX status not reported (1)")
    if "stype=11 ok=1" not in ev:
        raise Exception("Unexpected management frame TX status (1): " + ev)

    ev = dev[0].wait_event(["SME: Trying to authenticate"])
    if ev is None:
        raise Exception("No authentication attempt seen (3)")
    dev[0].dump_monitor()

    dev[0].request("DISCONNECT")

def test_sae_password(dev, apdev):
    """SAE and sae_password in hostapd configuration"""
    check_sae_capab(dev[0])
    params = hostapd.wpa2_params(ssid="test-sae",
                                 passphrase="12345678")
    params['wpa_key_mgmt'] = 'SAE WPA-PSK'
    params['sae_password'] = "sae-password"
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].request("SET sae_groups ")
    dev[0].connect("test-sae", psk="sae-password", key_mgmt="SAE",
                   scan_freq="2412")
    dev[1].connect("test-sae", psk="12345678", scan_freq="2412")
    dev[2].request("SET sae_groups ")
    dev[2].connect("test-sae", sae_password="sae-password", key_mgmt="SAE",
                   scan_freq="2412")

def test_sae_password_short(dev, apdev):
    """SAE and short password"""
    check_sae_capab(dev[0])
    params = hostapd.wpa2_params(ssid="test-sae")
    params['wpa_key_mgmt'] = 'SAE'
    params['sae_password'] = "secret"
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].request("SET sae_groups ")
    dev[0].connect("test-sae", sae_password="secret", key_mgmt="SAE",
                   scan_freq="2412")

def test_sae_password_long(dev, apdev):
    """SAE and long password"""
    check_sae_capab(dev[0])
    params = hostapd.wpa2_params(ssid="test-sae")
    params['wpa_key_mgmt'] = 'SAE'
    params['sae_password'] = 100*"A"
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].request("SET sae_groups ")
    dev[0].connect("test-sae", sae_password=100*"A", key_mgmt="SAE",
                   scan_freq="2412")

def test_sae_connect_cmd(dev, apdev):
    """SAE with connect command"""
    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    wpas.interface_add("wlan5", drv_params="force_connect_cmd=1")
    check_sae_capab(wpas)
    params = hostapd.wpa2_params(ssid="test-sae", passphrase="12345678")
    params['wpa_key_mgmt'] = 'SAE'
    hapd = hostapd.add_ap(apdev[0], params)

    wpas.request("SET sae_groups ")
    wpas.connect("test-sae", psk="12345678", key_mgmt="SAE",
                 scan_freq="2412", wait_connect=False)
    # mac80211_hwsim does not support SAE offload, so accept both a successful
    # connection and association rejection.
    ev = wpas.wait_event(["CTRL-EVENT-CONNECTED", "CTRL-EVENT-ASSOC-REJECT",
                          "Association request to the driver failed"],
                         timeout=15)
    if ev is None:
        raise Exception("No connection result reported")

def run_sae_password_id(dev, apdev, groups=None):
    check_sae_capab(dev[0])
    params = hostapd.wpa2_params(ssid="test-sae")
    params['wpa_key_mgmt'] = 'SAE'
    if groups:
        params['sae_groups'] = groups
    else:
        groups = ""
    params['sae_password'] = ['secret|mac=ff:ff:ff:ff:ff:ff|id=pw id',
                              'foo|mac=02:02:02:02:02:02',
                              'another secret|mac=ff:ff:ff:ff:ff:ff|id=' + 29*'A']
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].request("SET sae_groups " + groups)
    dev[0].connect("test-sae", sae_password="secret", sae_password_id="pw id",
                   key_mgmt="SAE", scan_freq="2412")
    dev[0].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected()

    # SAE Password Identifier element with the exact same length as the
    # optional Anti-Clogging Token field
    dev[0].connect("test-sae", sae_password="another secret",
                   sae_password_id=29*'A',
                   key_mgmt="SAE", scan_freq="2412")
    dev[0].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected()

    dev[0].connect("test-sae", sae_password="secret", sae_password_id="unknown",
                   key_mgmt="SAE", scan_freq="2412", wait_connect=False)

    ev = dev[0].wait_event(["CTRL-EVENT-SAE-UNKNOWN-PASSWORD-IDENTIFIER"],
                           timeout=10)
    if ev is None:
        raise Exception("Unknown password identifier not reported")
    dev[0].request("REMOVE_NETWORK all")

def test_sae_password_id(dev, apdev):
    """SAE and password identifier"""
    run_sae_password_id(dev, apdev, "")

def test_sae_password_id_ecc(dev, apdev):
    """SAE and password identifier (ECC)"""
    run_sae_password_id(dev, apdev, "19")

def test_sae_password_id_ffc(dev, apdev):
    """SAE and password identifier (FFC)"""
    run_sae_password_id(dev, apdev, "15")

def test_sae_password_id_only(dev, apdev):
    """SAE and password identifier (exclusively)"""
    check_sae_capab(dev[0])
    params = hostapd.wpa2_params(ssid="test-sae")
    params['wpa_key_mgmt'] = 'SAE'
    params['sae_password'] = 'secret|id=pw id'
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].request("SET sae_groups ")
    dev[0].connect("test-sae", sae_password="secret", sae_password_id="pw id",
                   key_mgmt="SAE", scan_freq="2412")

def test_sae_password_id_pwe_looping(dev, apdev):
    """SAE and password identifier with forced PWE looping"""
    check_sae_capab(dev[0])
    params = hostapd.wpa2_params(ssid="test-sae")
    params['wpa_key_mgmt'] = 'SAE'
    params['sae_password'] = 'secret|id=pw id'
    params['sae_pwe'] = "3"
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].request("SET sae_groups ")
    try:
        dev[0].set("sae_pwe", "3")
        dev[0].connect("test-sae", sae_password="secret",
                       sae_password_id="pw id",
                       key_mgmt="SAE", scan_freq="2412")
    finally:
        dev[0].set("sae_pwe", "0")

def test_sae_password_id_pwe_check_ap(dev, apdev):
    """SAE and password identifier with STA using unexpected PWE derivation"""
    check_sae_capab(dev[0])
    params = hostapd.wpa2_params(ssid="test-sae")
    params['wpa_key_mgmt'] = 'SAE'
    params['sae_password'] = 'secret|id=pw id'
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].request("SET sae_groups ")
    try:
        dev[0].set("sae_pwe", "3")
        dev[0].connect("test-sae", sae_password="secret",
                       sae_password_id="pw id",
                       key_mgmt="SAE", scan_freq="2412", wait_connect=False)
        ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED",
                                "CTRL-EVENT-SSID-TEMP-DISABLED"], timeout=10)
        if ev is None or "CTRL-EVENT-SSID-TEMP-DISABLED" not in ev:
            raise Exception("Connection failure not reported")
    finally:
        dev[0].set("sae_pwe", "0")

def test_sae_password_id_pwe_check_sta(dev, apdev):
    """SAE and password identifier with AP using unexpected PWE derivation"""
    check_sae_capab(dev[0])
    params = hostapd.wpa2_params(ssid="test-sae")
    params['wpa_key_mgmt'] = 'SAE'
    params['sae_pwe'] = "3"
    params['sae_password'] = 'secret|id=pw id'
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].request("SET sae_groups ")
    dev[0].connect("test-sae", sae_password="secret",
                   sae_password_id="pw id",
                   key_mgmt="SAE", scan_freq="2412", wait_connect=False)
    ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED",
                            "CTRL-EVENT-NETWORK-NOT-FOUND"], timeout=10)
    if ev is None or "CTRL-EVENT-NETWORK-NOT-FOUND" not in ev:
        raise Exception("Connection failure not reported")

def test_sae_forced_anti_clogging_pw_id(dev, apdev):
    """SAE anti clogging (forced and Password Identifier)"""
    check_sae_capab(dev[0])
    params = hostapd.wpa2_params(ssid="test-sae")
    params['wpa_key_mgmt'] = 'SAE'
    params['sae_anti_clogging_threshold'] = '0'
    params['sae_password'] = 'secret|id=' + 29*'A'
    hostapd.add_ap(apdev[0], params)
    for i in range(0, 2):
        dev[i].request("SET sae_groups ")
        dev[i].connect("test-sae", sae_password="secret",
                       sae_password_id=29*'A', key_mgmt="SAE", scan_freq="2412")

def test_sae_reauth(dev, apdev):
    """SAE reauthentication"""
    check_sae_capab(dev[0])
    params = hostapd.wpa2_params(ssid="test-sae",
                                 passphrase="12345678")
    params['wpa_key_mgmt'] = 'SAE'
    params["ieee80211w"] = "2"
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].request("SET sae_groups ")
    id = dev[0].connect("test-sae", psk="12345678", key_mgmt="SAE",
                        ieee80211w="2", scan_freq="2412")

    hapd.set("ext_mgmt_frame_handling", "1")
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected(timeout=10)
    hapd.set("ext_mgmt_frame_handling", "0")
    dev[0].request("PMKSA_FLUSH")
    dev[0].request("REASSOCIATE")
    dev[0].wait_connected(timeout=10, error="Timeout on re-connection")

def test_sae_anti_clogging_during_attack(dev, apdev):
    """SAE anti clogging during an attack"""
    try:
        run_sae_anti_clogging_during_attack(dev, apdev)
    finally:
        stop_monitor(apdev[1]["ifname"])

def build_sae_commit(bssid, addr, group=21, token=None):
    if group == 19:
        scalar = binascii.unhexlify("7332d3ebff24804005ccd8c56141e3ed8d84f40638aa31cd2fac11d4d2e89e7b")
        element = binascii.unhexlify("954d0f4457066bff3168376a1d7174f4e66620d1792406f613055b98513a7f03a538c13dfbaf2029e2adc6aa96aa0ddcf08ac44887b02f004b7f29b9dbf4b7d9")
    elif group == 21:
        scalar = binascii.unhexlify("001eec673111b902f5c8a61c8cb4c1c4793031aeea8c8c319410903bc64bcbaea134ab01c4e016d51436f5b5426f7e2af635759a3033fb4031ea79f89a62a3e2f828")
        element = binascii.unhexlify("00580eb4b448ea600ea277d5e66e4ed37db82bb04ac90442e9c3727489f366ba4b82f0a472d02caf4cdd142e96baea5915d71374660ee23acbaca38cf3fe8c5fb94b01abbc5278121635d7c06911c5dad8f18d516e1fbe296c179b7c87a1dddfab393337d3d215ed333dd396da6d8f20f798c60d054f1093c24d9c2d98e15c030cc375f0")
        pass
    frame = binascii.unhexlify("b0003a01")
    frame += bssid + addr + bssid
    frame += binascii.unhexlify("1000")
    auth_alg = 3
    transact = 1
    status = 0
    frame += struct.pack("<HHHH", auth_alg, transact, status, group)
    if token:
        frame += token
    frame += scalar + element
    return frame

def sae_rx_commit_token_req(sock, radiotap, send_two=False):
    msg = sock.recv(1500)
    ver, pad, length, present = struct.unpack('<BBHL', msg[0:8])
    frame = msg[length:]
    if len(frame) < 4:
        return False
    fc, duration = struct.unpack('<HH', frame[0:4])
    if fc != 0xb0:
        return False
    frame = frame[4:]
    da = frame[0:6]
    if da[0] != 0xf2:
        return False
    sa = frame[6:12]
    bssid = frame[12:18]
    body = frame[20:]

    alg, seq, status, group = struct.unpack('<HHHH', body[0:8])
    if alg != 3 or seq != 1 or status != 76:
        return False
    token = body[8:]

    frame = build_sae_commit(bssid, da, token=token)
    sock.send(radiotap + frame)
    if send_two:
        sock.send(radiotap + frame)
    return True

def run_sae_anti_clogging_during_attack(dev, apdev):
    check_sae_capab(dev[0])
    params = hostapd.wpa2_params(ssid="test-sae", passphrase="12345678")
    params['wpa_key_mgmt'] = 'SAE'
    params['sae_groups'] = '21'
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].scan_for_bss(hapd.own_addr(), freq=2412)
    dev[0].request("SET sae_groups 21")
    dev[1].scan_for_bss(hapd.own_addr(), freq=2412)
    dev[1].request("SET sae_groups 21")

    sock = start_monitor(apdev[1]["ifname"])
    radiotap = radiotap_build()

    bssid = binascii.unhexlify(hapd.own_addr().replace(':', ''))
    for i in range(16):
        addr = binascii.unhexlify("f2%010x" % i)
        frame = build_sae_commit(bssid, addr)
        sock.send(radiotap + frame)
        sock.send(radiotap + frame)

    count = 0
    for i in range(150):
        if sae_rx_commit_token_req(sock, radiotap, send_two=True):
            count += 1
    logger.info("Number of token responses sent: %d" % count)
    if count < 10:
        raise Exception("Too few token responses seen: %d" % count)

    for i in range(16):
        addr = binascii.unhexlify("f201%08x" % i)
        frame = build_sae_commit(bssid, addr)
        sock.send(radiotap + frame)

    count = 0
    for i in range(150):
        if sae_rx_commit_token_req(sock, radiotap):
            count += 1
            if count == 10:
                break
    if count < 5:
        raise Exception("Too few token responses in second round: %d" % count)

    dev[0].connect("test-sae", psk="12345678", key_mgmt="SAE",
                   scan_freq="2412", wait_connect=False)
    dev[1].connect("test-sae", psk="12345678", key_mgmt="SAE",
                   scan_freq="2412", wait_connect=False)

    count = 0
    connected0 = False
    connected1 = False
    for i in range(1000):
        if sae_rx_commit_token_req(sock, radiotap):
            count += 1
            addr = binascii.unhexlify("f202%08x" % i)
            frame = build_sae_commit(bssid, addr)
            sock.send(radiotap + frame)
        while dev[0].mon.pending():
            ev = dev[0].mon.recv()
            logger.debug("EV0: " + ev)
            if "CTRL-EVENT-CONNECTED" in ev:
                connected0 = True
        while dev[1].mon.pending():
            ev = dev[1].mon.recv()
            logger.debug("EV1: " + ev)
            if "CTRL-EVENT-CONNECTED" in ev:
                connected1 = True
        if connected0 and connected1:
            break
        time.sleep(0.00000001)
    if not connected0:
        raise Exception("Real station(0) did not get connected")
    if not connected1:
        raise Exception("Real station(1) did not get connected")
    if count < 1:
        raise Exception("Too few token responses in third round: %d" % count)

def test_sae_sync(dev, apdev):
    """SAE dot11RSNASAESync"""
    check_sae_capab(dev[0])
    params = hostapd.wpa2_params(ssid="test-sae", passphrase="12345678")
    params['wpa_key_mgmt'] = 'SAE'
    params['sae_sync'] = '1'
    hostapd.add_ap(apdev[0], params)

    # TODO: More complete dot11RSNASAESync testing. For now, this is really only
    # checking that sae_sync config parameter is accepted.
    dev[0].request("SET sae_groups ")
    dev[1].request("SET sae_groups ")
    id = {}
    for i in range(0, 2):
        dev[i].scan(freq="2412")
        id[i] = dev[i].connect("test-sae", psk="12345678", key_mgmt="SAE",
                               scan_freq="2412", only_add_network=True)
    for i in range(0, 2):
        dev[i].select_network(id[i])
    for i in range(0, 2):
        dev[i].wait_connected(timeout=10)

def test_sae_confirm_immediate(dev, apdev):
    """SAE and AP sending Confirm message without waiting STA"""
    check_sae_capab(dev[0])
    params = hostapd.wpa2_params(ssid="test-sae", passphrase="12345678")
    params['wpa_key_mgmt'] = 'SAE'
    params['sae_confirm_immediate'] = '1'
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].request("SET sae_groups ")
    dev[0].connect("test-sae", psk="12345678", key_mgmt="SAE", scan_freq="2412")

def test_sae_confirm_immediate2(dev, apdev):
    """SAE and AP sending Confirm message without waiting STA (2)"""
    check_sae_capab(dev[0])
    params = hostapd.wpa2_params(ssid="test-sae", passphrase="12345678")
    params['wpa_key_mgmt'] = 'SAE'
    params['sae_confirm_immediate'] = '2'
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].request("SET sae_groups ")
    dev[0].connect("test-sae", psk="12345678", key_mgmt="SAE", scan_freq="2412")

def test_sae_pwe_group_19(dev, apdev):
    """SAE PWE derivation options with group 19"""
    run_sae_pwe_group(dev, apdev, 19)

def test_sae_pwe_group_20(dev, apdev):
    """SAE PWE derivation options with group 20"""
    run_sae_pwe_group(dev, apdev, 20)

def test_sae_pwe_group_21(dev, apdev):
    """SAE PWE derivation options with group 21"""
    run_sae_pwe_group(dev, apdev, 21)

def test_sae_pwe_group_25(dev, apdev):
    """SAE PWE derivation options with group 25"""
    run_sae_pwe_group(dev, apdev, 25)

def test_sae_pwe_group_28(dev, apdev):
    """SAE PWE derivation options with group 28"""
    run_sae_pwe_group(dev, apdev, 28)

def test_sae_pwe_group_29(dev, apdev):
    """SAE PWE derivation options with group 29"""
    run_sae_pwe_group(dev, apdev, 29)

def test_sae_pwe_group_30(dev, apdev):
    """SAE PWE derivation options with group 30"""
    run_sae_pwe_group(dev, apdev, 30)

def test_sae_pwe_group_1(dev, apdev):
    """SAE PWE derivation options with group 1"""
    run_sae_pwe_group(dev, apdev, 1)

def test_sae_pwe_group_2(dev, apdev):
    """SAE PWE derivation options with group 2"""
    run_sae_pwe_group(dev, apdev, 2)

def test_sae_pwe_group_5(dev, apdev):
    """SAE PWE derivation options with group 5"""
    run_sae_pwe_group(dev, apdev, 5)

def test_sae_pwe_group_14(dev, apdev):
    """SAE PWE derivation options with group 14"""
    run_sae_pwe_group(dev, apdev, 14)

def test_sae_pwe_group_15(dev, apdev):
    """SAE PWE derivation options with group 15"""
    run_sae_pwe_group(dev, apdev, 15)

def test_sae_pwe_group_16(dev, apdev):
    """SAE PWE derivation options with group 16"""
    run_sae_pwe_group(dev, apdev, 16)

def test_sae_pwe_group_22(dev, apdev):
    """SAE PWE derivation options with group 22"""
    run_sae_pwe_group(dev, apdev, 22)

def test_sae_pwe_group_23(dev, apdev):
    """SAE PWE derivation options with group 23"""
    run_sae_pwe_group(dev, apdev, 23)

def test_sae_pwe_group_24(dev, apdev):
    """SAE PWE derivation options with group 24"""
    run_sae_pwe_group(dev, apdev, 24)

def start_sae_pwe_ap(apdev, group, sae_pwe):
    params = hostapd.wpa2_params(ssid="sae-pwe", passphrase="12345678")
    params['wpa_key_mgmt'] = 'SAE'
    params['sae_groups'] = str(group)
    params['sae_pwe'] = str(sae_pwe)
    return hostapd.add_ap(apdev, params)

def run_sae_pwe_group(dev, apdev, group):
    check_sae_capab(dev[0])
    tls = dev[0].request("GET tls_library")
    if group in [27, 28, 29, 30]:
        if tls.startswith("OpenSSL") and "run=OpenSSL 1." in tls:
            logger.info("Add Brainpool EC groups since OpenSSL is new enough")
        else:
            raise HwsimSkip("Brainpool curve not supported")
    start_sae_pwe_ap(apdev[0], group, 2)
    try:
        check_sae_pwe_group(dev[0], group, 0)
        check_sae_pwe_group(dev[0], group, 1)
        check_sae_pwe_group(dev[0], group, 2)
    finally:
        dev[0].set("sae_groups", "")
        dev[0].set("sae_pwe", "0")

def check_sae_pwe_group(dev, group, sae_pwe):
    dev.set("sae_groups", str(group))
    dev.set("sae_pwe", str(sae_pwe))
    dev.connect("sae-pwe", psk="12345678", key_mgmt="SAE", scan_freq="2412")
    dev.request("REMOVE_NETWORK all")
    dev.wait_disconnected()
    dev.dump_monitor()

def test_sae_pwe_h2e_only_ap(dev, apdev):
    """SAE PWE derivation with H2E-only AP"""
    check_sae_capab(dev[0])
    start_sae_pwe_ap(apdev[0], 19, 1)
    try:
        check_sae_pwe_group(dev[0], 19, 1)
        check_sae_pwe_group(dev[0], 19, 2)
    finally:
        dev[0].set("sae_groups", "")
        dev[0].set("sae_pwe", "0")

    dev[0].connect("sae-pwe", psk="12345678", key_mgmt="SAE", scan_freq="2412",
                   wait_connect=False)
    ev = dev[0].wait_event(["CTRL-EVENT-NETWORK-NOT-FOUND"], timeout=10)
    if ev is None:
        raise Exception("No indication of mismatching network seen")

def test_sae_pwe_h2e_only_ap_sta_forcing_loop(dev, apdev):
    """SAE PWE derivation with H2E-only AP and STA forcing loop"""
    check_sae_capab(dev[0])
    start_sae_pwe_ap(apdev[0], 19, 1)
    dev[0].set("ignore_sae_h2e_only", "1")
    dev[0].connect("sae-pwe", psk="12345678", key_mgmt="SAE", scan_freq="2412",
                   wait_connect=False)
    ev = dev[0].wait_event(["CTRL-EVENT-SSID-TEMP-DISABLED"], timeout=10)
    dev[0].request("DISCONNECT")
    if ev is None:
        raise Exception("No indication of temporary disabled network seen")

def test_sae_pwe_loop_only_ap(dev, apdev):
    """SAE PWE derivation with loop-only AP"""
    check_sae_capab(dev[0])
    start_sae_pwe_ap(apdev[0], 19, 0)
    try:
        check_sae_pwe_group(dev[0], 19, 0)
        check_sae_pwe_group(dev[0], 19, 2)
        dev[0].set("sae_pwe", "1")
        dev[0].connect("sae-pwe", psk="12345678", key_mgmt="SAE",
                       scan_freq="2412", wait_connect=False)
        ev = dev[0].wait_event(["CTRL-EVENT-NETWORK-NOT-FOUND"], timeout=10)
        if ev is None:
            raise Exception("No indication of mismatching network seen")
    finally:
        dev[0].set("sae_groups", "")
        dev[0].set("sae_pwe", "0")

def test_sae_h2e_rejected_groups(dev, apdev):
    """SAE H2E and rejected groups indication"""
    check_sae_capab(dev[0])
    params = hostapd.wpa2_params(ssid="sae-pwe", passphrase="12345678")
    params['wpa_key_mgmt'] = 'SAE'
    params['sae_groups'] = "19"
    params['sae_pwe'] = "1"
    hapd = hostapd.add_ap(apdev[0], params)
    try:
        dev[0].set("sae_groups", "21 20 19")
        dev[0].set("sae_pwe", "1")
        dev[0].connect("sae-pwe", psk="12345678", key_mgmt="SAE",
                       scan_freq="2412")
        addr = dev[0].own_addr()
        hapd.wait_sta(addr)
        sta = hapd.get_sta(addr)
        if 'sae_rejected_groups' not in sta:
            raise Exception("No sae_rejected_groups")
        val = sta['sae_rejected_groups']
        if val != "21 20":
            raise Exception("Unexpected sae_rejected_groups value: " + val)
    finally:
        dev[0].set("sae_groups", "")
        dev[0].set("sae_pwe", "0")

def test_sae_h2e_rejected_groups_unexpected(dev, apdev):
    """SAE H2E and rejected groups indication (unexpected group)"""
    check_sae_capab(dev[0])
    params = hostapd.wpa2_params(ssid="sae-pwe", passphrase="12345678")
    params['wpa_key_mgmt'] = 'SAE'
    params['sae_groups'] = "19 20"
    params['sae_pwe'] = "1"
    hapd = hostapd.add_ap(apdev[0], params)
    try:
        dev[0].set("sae_groups", "21 19")
        dev[0].set("extra_sae_rejected_groups", "19")
        dev[0].set("sae_pwe", "1")
        dev[0].connect("sae-pwe", psk="12345678", key_mgmt="SAE",
                       scan_freq="2412", wait_connect=False)
        ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED",
                                "CTRL-EVENT-SSID-TEMP-DISABLED"], timeout=10)
        dev[0].request("DISCONNECT")
        if ev is None:
            raise Exception("No indication of temporary disabled network seen")
        if "CTRL-EVENT-CONNECTED" in ev:
            raise Exception("Unexpected connection")
    finally:
        dev[0].set("sae_groups", "")
        dev[0].set("sae_pwe", "0")

def test_sae_h2e_password_id(dev, apdev):
    """SAE H2E and password identifier"""
    check_sae_capab(dev[0])
    params = hostapd.wpa2_params(ssid="test-sae")
    params['wpa_key_mgmt'] = 'SAE'
    params['sae_pwe'] = '1'
    params['sae_password'] = 'secret|id=pw id'
    hapd = hostapd.add_ap(apdev[0], params)

    try:
        dev[0].request("SET sae_groups ")
        dev[0].set("sae_pwe", "1")
        dev[0].connect("test-sae", sae_password="secret",
                       sae_password_id="pw id",
                       key_mgmt="SAE", scan_freq="2412")
    finally:
        dev[0].set("sae_groups", "")
        dev[0].set("sae_pwe", "0")

def test_sae_pwe_in_psk_ap(dev, apdev):
    """sae_pwe parameter in PSK-only-AP"""
    params = hostapd.wpa2_params(ssid="test-psk", passphrase="12345678")
    params['sae_pwe'] = '1'
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].connect("test-psk", psk="12345678", scan_freq="2412")

def test_sae_auth_restart(dev, apdev):
    """SAE and authentication restarts with H2E/looping"""
    check_sae_capab(dev[0])
    params = hostapd.wpa2_params(ssid="test-sae")
    params['wpa_key_mgmt'] = 'SAE'
    params['sae_pwe'] = '2'
    params['sae_password'] = 'secret|id=pw id'
    hapd = hostapd.add_ap(apdev[0], params)

    try:
        dev[0].request("SET sae_groups ")
        for pwe in [1, 0, 1]:
            dev[0].set("sae_pwe", str(pwe))
            dev[0].connect("test-sae", sae_password="secret",
                           sae_password_id="pw id",
                           key_mgmt="SAE", scan_freq="2412")
            # Disconnect without hostapd removing the STA entry so that the
            # following SAE authentication instance starts with an existing
            # STA entry that has maintained some SAE state.
            hapd.set("ext_mgmt_frame_handling", "1")
            dev[0].request("REMOVE_NETWORK all")
            req = hapd.mgmt_rx()
            dev[0].wait_disconnected()
            dev[0].dump_monitor()
            hapd.set("ext_mgmt_frame_handling", "0")
    finally:
        dev[0].set("sae_groups", "")
        dev[0].set("sae_pwe", "0")

def test_sae_rsne_mismatch(dev, apdev):
    """SAE and RSNE mismatch in EAPOL-Key msg 2/4"""
    check_sae_capab(dev[0])
    dev[0].set("sae_groups", "")

    params = hostapd.wpa2_params(ssid="sae-pwe", passphrase="12345678")
    params['wpa_key_mgmt'] = 'SAE'
    hapd = hostapd.add_ap(apdev[0], params)

    # First, test with matching RSNE to confirm testing capability
    dev[0].set("rsne_override_eapol",
               "30140100000fac040100000fac040100000fac080000")
    dev[0].connect("sae-pwe", psk="12345678", key_mgmt="SAE",
                   scan_freq="2412")
    dev[0].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected()
    dev[0].dump_monitor()

    # Then, test with modified RSNE
    tests = ["30140100000fac040100000fac040100000fac080010", "0000"]
    for ie in tests:
        dev[0].set("rsne_override_eapol", ie)
        dev[0].connect("sae-pwe", psk="12345678", key_mgmt="SAE",
                       scan_freq="2412", wait_connect=False)
        ev = dev[0].wait_event(["Associated with"], timeout=10)
        if ev is None:
            raise Exception("No indication of association seen")
        ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED",
                                "CTRL-EVENT-DISCONNECTED"], timeout=5)
        dev[0].request("REMOVE_NETWORK all")
        if ev is None:
            raise Exception("No disconnection seen")
        if "CTRL-EVENT-DISCONNECTED" not in ev:
            raise Exception("Unexpected connection")
        dev[0].dump_monitor()

def test_sae_h2e_rsnxe_mismatch(dev, apdev):
    """SAE H2E and RSNXE mismatch in EAPOL-Key msg 2/4"""
    check_sae_capab(dev[0])
    params = hostapd.wpa2_params(ssid="sae-pwe", passphrase="12345678")
    params['wpa_key_mgmt'] = 'SAE'
    params['sae_pwe'] = "1"
    hapd = hostapd.add_ap(apdev[0], params)
    try:
        dev[0].set("sae_groups", "19")
        dev[0].set("sae_pwe", "1")
        for rsnxe in ["F40100", "F400", ""]:
            dev[0].set("rsnxe_override_eapol", rsnxe)
            dev[0].connect("sae-pwe", psk="12345678", key_mgmt="SAE",
                           scan_freq="2412", wait_connect=False)
            ev = dev[0].wait_event(["Associated with"], timeout=10)
            if ev is None:
                raise Exception("No indication of association seen")
            ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED",
                                    "CTRL-EVENT-DISCONNECTED"], timeout=5)
            dev[0].request("REMOVE_NETWORK all")
            if ev is None:
                raise Exception("No disconnection seen")
            if "CTRL-EVENT-DISCONNECTED" not in ev:
                raise Exception("Unexpected connection")
            dev[0].dump_monitor()
    finally:
        dev[0].set("sae_groups", "")
        dev[0].set("sae_pwe", "0")

def test_sae_h2e_rsnxe_mismatch_retries(dev, apdev):
    """SAE H2E and RSNXE mismatch in EAPOL-Key msg 2/4 retries"""
    check_sae_capab(dev[0])
    params = hostapd.wpa2_params(ssid="sae-pwe", passphrase="12345678")
    params['wpa_key_mgmt'] = 'SAE'
    params['sae_pwe'] = "1"
    hapd = hostapd.add_ap(apdev[0], params)
    try:
        dev[0].set("sae_groups", "19")
        dev[0].set("sae_pwe", "1")
        rsnxe = "F40100"
        dev[0].set("rsnxe_override_eapol", rsnxe)
        dev[0].connect("sae-pwe", psk="12345678", key_mgmt="SAE",
                       scan_freq="2412", wait_connect=False)
        ev = dev[0].wait_event(["Associated with"], timeout=10)
        if ev is None:
            raise Exception("No indication of association seen")
        ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED",
                                "CTRL-EVENT-DISCONNECTED"], timeout=5)
        if ev is None:
            raise Exception("No disconnection seen")
        if "CTRL-EVENT-DISCONNECTED" not in ev:
            raise Exception("Unexpected connection")

        ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED",
                                "CTRL-EVENT-DISCONNECTED"], timeout=10)
        if ev is None:
            raise Exception("No disconnection seen (2)")
        if "CTRL-EVENT-DISCONNECTED" not in ev:
            raise Exception("Unexpected connection (2)")

        dev[0].dump_monitor()
    finally:
        dev[0].set("sae_groups", "")
        dev[0].set("sae_pwe", "0")

def test_sae_h2e_rsnxe_mismatch_assoc(dev, apdev):
    """SAE H2E and RSNXE mismatch in EAPOL-Key msg 2/4 (assoc)"""
    check_sae_capab(dev[0])
    params = hostapd.wpa2_params(ssid="sae-pwe", passphrase="12345678")
    params['wpa_key_mgmt'] = 'SAE'
    params['sae_pwe'] = "1"
    hapd = hostapd.add_ap(apdev[0], params)
    try:
        dev[0].set("sae_groups", "19")
        dev[0].set("sae_pwe", "1")
        for rsnxe in ["F40100", "F400", ""]:
            dev[0].set("rsnxe_override_assoc", rsnxe)
            dev[0].set("rsnxe_override_eapol", "F40120")
            dev[0].connect("sae-pwe", psk="12345678", key_mgmt="SAE",
                           scan_freq="2412", wait_connect=False)
            ev = dev[0].wait_event(["Associated with"], timeout=10)
            if ev is None:
                raise Exception("No indication of association seen")
            ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED",
                                    "CTRL-EVENT-DISCONNECTED"], timeout=5)
            dev[0].request("REMOVE_NETWORK all")
            if ev is None:
                raise Exception("No disconnection seen")
            if "CTRL-EVENT-DISCONNECTED" not in ev:
                raise Exception("Unexpected connection")
            dev[0].dump_monitor()
    finally:
        dev[0].set("sae_groups", "")
        dev[0].set("sae_pwe", "0")

def test_sae_h2e_rsnxe_mismatch_ap(dev, apdev):
    """SAE H2E and RSNXE mismatch in EAPOL-Key msg 3/4"""
    run_sae_h2e_rsnxe_mismatch_ap(dev, apdev, "F40100")

def test_sae_h2e_rsnxe_mismatch_ap2(dev, apdev):
    """SAE H2E and RSNXE mismatch in EAPOL-Key msg 3/4"""
    run_sae_h2e_rsnxe_mismatch_ap(dev, apdev, "F400")

def test_sae_h2e_rsnxe_mismatch_ap3(dev, apdev):
    """SAE H2E and RSNXE mismatch in EAPOL-Key msg 3/4"""
    run_sae_h2e_rsnxe_mismatch_ap(dev, apdev, "")

def run_sae_h2e_rsnxe_mismatch_ap(dev, apdev, rsnxe):
    check_sae_capab(dev[0])
    params = hostapd.wpa2_params(ssid="sae-pwe", passphrase="12345678")
    params['wpa_key_mgmt'] = 'SAE'
    params['sae_pwe'] = "1"
    params['rsnxe_override_eapol'] = rsnxe
    hapd = hostapd.add_ap(apdev[0], params)
    try:
        dev[0].set("sae_groups", "19")
        dev[0].set("sae_pwe", "1")
        dev[0].connect("sae-pwe", psk="12345678", key_mgmt="SAE",
                       scan_freq="2412", wait_connect=False)
        ev = dev[0].wait_event(["Associated with"], timeout=10)
        if ev is None:
            raise Exception("No indication of association seen")
        ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED",
                                "CTRL-EVENT-DISCONNECTED"], timeout=5)
        dev[0].request("REMOVE_NETWORK all")
        if ev is None:
            raise Exception("No disconnection seen")
        if "CTRL-EVENT-DISCONNECTED" not in ev:
            raise Exception("Unexpected connection")
    finally:
        dev[0].set("sae_groups", "")
        dev[0].set("sae_pwe", "0")

def test_sae_forced_anti_clogging_h2e(dev, apdev):
    """SAE anti clogging (forced, H2E)"""
    check_sae_capab(dev[0])
    check_sae_capab(dev[1])
    params = hostapd.wpa2_params(ssid="test-sae", passphrase="12345678")
    params['wpa_key_mgmt'] = 'SAE WPA-PSK'
    params['sae_pwe'] = "1"
    params['sae_anti_clogging_threshold'] = '0'
    hostapd.add_ap(apdev[0], params)
    dev[2].connect("test-sae", psk="12345678", scan_freq="2412")
    try:
        for i in range(2):
            dev[i].request("SET sae_groups ")
            dev[i].set("sae_pwe", "1")
            dev[i].connect("test-sae", psk="12345678", key_mgmt="SAE",
                           scan_freq="2412")
    finally:
        for i in range(2):
            dev[i].set("sae_pwe", "0")

def test_sae_forced_anti_clogging_h2e_loop(dev, apdev):
    """SAE anti clogging (forced, H2E + loop)"""
    check_sae_capab(dev[0])
    check_sae_capab(dev[1])
    params = hostapd.wpa2_params(ssid="test-sae", passphrase="12345678")
    params['wpa_key_mgmt'] = 'SAE WPA-PSK'
    params['sae_pwe'] = "2"
    params['sae_anti_clogging_threshold'] = '0'
    hostapd.add_ap(apdev[0], params)
    dev[2].connect("test-sae", psk="12345678", scan_freq="2412")
    try:
        for i in range(2):
            dev[i].request("SET sae_groups ")
            dev[i].set("sae_pwe", "2")
            dev[i].connect("test-sae", psk="12345678", key_mgmt="SAE",
                           scan_freq="2412")
    finally:
        for i in range(2):
            dev[i].set("sae_pwe", "0")

def test_sae_okc(dev, apdev):
    """SAE and opportunistic key caching"""
    check_sae_capab(dev[0])
    params = hostapd.wpa2_params(ssid="test-sae", passphrase="12345678")
    params['wpa_key_mgmt'] = 'SAE'
    params['okc'] = '1'
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = hapd.own_addr()

    dev[0].set("sae_groups", "")
    id = dev[0].connect("test-sae", psk="12345678", key_mgmt="SAE",
                        okc=True, scan_freq="2412")
    dev[0].dump_monitor()
    hapd.wait_sta()
    if "sae_group" not in dev[0].get_status():
        raise Exception("SAE authentication not used")

    hapd2 = hostapd.add_ap(apdev[1], params)
    bssid2 = hapd2.own_addr()

    dev[0].scan_for_bss(bssid2, freq=2412)
    dev[0].roam(bssid2)
    dev[0].dump_monitor()
    hapd2.wait_sta()
    if "sae_group" in dev[0].get_status():
        raise Exception("SAE authentication used during roam to AP2")

    dev[0].roam(bssid)
    dev[0].dump_monitor()
    hapd.wait_sta()
    if "sae_group" in dev[0].get_status():
        raise Exception("SAE authentication used during roam to AP1")

def test_sae_okc_sta_only(dev, apdev):
    """SAE and opportunistic key caching only on STA"""
    check_sae_capab(dev[0])
    params = hostapd.wpa2_params(ssid="test-sae", passphrase="12345678")
    params['wpa_key_mgmt'] = 'SAE'
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = hapd.own_addr()

    dev[0].set("sae_groups", "")
    id = dev[0].connect("test-sae", psk="12345678", key_mgmt="SAE",
                        okc=True, scan_freq="2412")
    dev[0].dump_monitor()
    hapd.wait_sta()
    if "sae_group" not in dev[0].get_status():
        raise Exception("SAE authentication not used")

    hapd2 = hostapd.add_ap(apdev[1], params)
    bssid2 = hapd2.own_addr()

    dev[0].scan_for_bss(bssid2, freq=2412)
    dev[0].roam(bssid2, assoc_reject_ok=True)
    dev[0].dump_monitor()
    hapd2.wait_sta()
    if "sae_group" not in dev[0].get_status():
        raise Exception("SAE authentication not used during roam to AP2")

def test_sae_okc_pmk_lifetime(dev, apdev):
    """SAE and opportunistic key caching and PMK lifetime"""
    check_sae_capab(dev[0])
    params = hostapd.wpa2_params(ssid="test-sae", passphrase="12345678")
    params['wpa_key_mgmt'] = 'SAE'
    params['okc'] = '1'
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = hapd.own_addr()

    dev[0].set("sae_groups", "")
    dev[0].set("dot11RSNAConfigPMKLifetime", "10")
    dev[0].set("dot11RSNAConfigPMKReauthThreshold", "30")
    id = dev[0].connect("test-sae", psk="12345678", key_mgmt="SAE",
                        okc=True, scan_freq="2412")
    dev[0].dump_monitor()
    hapd.wait_sta()
    if "sae_group" not in dev[0].get_status():
        raise Exception("SAE authentication not used")

    hapd2 = hostapd.add_ap(apdev[1], params)
    bssid2 = hapd2.own_addr()

    time.sleep(5)
    dev[0].scan_for_bss(bssid2, freq=2412)
    dev[0].roam(bssid2)
    dev[0].dump_monitor()
    hapd2.wait_sta()
    if "sae_group" not in dev[0].get_status():
        raise Exception("SAE authentication not used during roam to AP2 after reauth threshold")

def test_sae_pmk_lifetime(dev, apdev):
    """SAE and PMK lifetime"""
    check_sae_capab(dev[0])
    params = hostapd.wpa2_params(ssid="test-sae", passphrase="12345678")
    params['wpa_key_mgmt'] = 'SAE'
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = hapd.own_addr()

    dev[0].set("sae_groups", "")
    dev[0].set("dot11RSNAConfigPMKLifetime", "10")
    dev[0].set("dot11RSNAConfigPMKReauthThreshold", "50")
    id = dev[0].connect("test-sae", psk="12345678", key_mgmt="SAE",
                        scan_freq="2412")
    dev[0].dump_monitor()
    hapd.wait_sta()
    if "sae_group" not in dev[0].get_status():
        raise Exception("SAE authentication not used")

    hapd2 = hostapd.add_ap(apdev[1], params)
    bssid2 = hapd2.own_addr()

    dev[0].scan_for_bss(bssid2, freq=2412)
    dev[0].roam(bssid2)
    dev[0].dump_monitor()
    hapd2.wait_sta()
    if "sae_group" not in dev[0].get_status():
        raise Exception("SAE authentication not used during roam to AP2")

    dev[0].roam(bssid)
    dev[0].dump_monitor()
    hapd.wait_sta()
    if "sae_group" in dev[0].get_status():
        raise Exception("SAE authentication used during roam to AP1")

    time.sleep(6)
    dev[0].scan_for_bss(bssid2, freq=2412)
    dev[0].roam(bssid2)
    dev[0].dump_monitor()
    hapd2.wait_sta()
    if "sae_group" not in dev[0].get_status():
        raise Exception("SAE authentication not used during roam to AP2 after reauth threshold")

    ev = dev[0].wait_event(["PMKSA-CACHE-REMOVED"], 11)
    if ev is None:
        raise Exception("PMKSA cache entry did not expire")
    if bssid2 in ev:
        raise Exception("Unexpected expiration of the current SAE PMKSA cache entry")

def test_sae_and_psk_multiple_passwords(dev, apdev, params):
    """SAE and PSK with multiple passwords/passphrases"""
    check_sae_capab(dev[0])
    check_sae_capab(dev[1])
    addr0 = dev[0].own_addr()
    addr1 = dev[1].own_addr()
    psk_file = os.path.join(params['logdir'],
                            'sae_and_psk_multiple_passwords.wpa_psk')
    with open(psk_file, 'w') as f:
        f.write(addr0 + ' passphrase0\n')
        f.write(addr1 + ' passphrase1\n')
    params = hostapd.wpa2_params(ssid="test-sae")
    params['wpa_key_mgmt'] = 'SAE WPA-PSK'
    params['sae_password'] = ['passphrase0|mac=' + addr0,
                              'passphrase1|mac=' + addr1]
    params['wpa_psk_file'] = psk_file
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].set("sae_groups", "")
    dev[0].connect("test-sae", sae_password="passphrase0",
                   key_mgmt="SAE", scan_freq="2412")
    dev[0].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected()

    dev[0].connect("test-sae", psk="passphrase0", scan_freq="2412")
    dev[0].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected()

    dev[1].set("sae_groups", "")
    dev[1].connect("test-sae", sae_password="passphrase1",
                   key_mgmt="SAE", scan_freq="2412")
    dev[1].request("REMOVE_NETWORK all")
    dev[1].wait_disconnected()

    dev[1].connect("test-sae", psk="passphrase1", scan_freq="2412")
    dev[1].request("REMOVE_NETWORK all")
    dev[1].wait_disconnected()

def test_sae_pmf_roam(dev, apdev):
    """SAE/PMF roam"""
    check_sae_capab(dev[0])
    params = hostapd.wpa2_params(ssid="test-sae", passphrase="12345678")
    params['wpa_key_mgmt'] = 'SAE'
    params['ieee80211w'] = '2'
    params['skip_prune_assoc'] = '1'
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = hapd.own_addr()

    dev[0].set("sae_groups", "")
    id = dev[0].connect("test-sae", psk="12345678", key_mgmt="SAE",
                        ieee80211w="2", scan_freq="2412")
    dev[0].dump_monitor()
    hapd.wait_sta()

    hapd2 = hostapd.add_ap(apdev[1], params)
    bssid2 = hapd2.own_addr()

    dev[0].scan_for_bss(bssid2, freq=2412)
    dev[0].roam(bssid2)
    dev[0].dump_monitor()
    hapd2.wait_sta()

    dev[0].roam(bssid)
    dev[0].dump_monitor()

def test_sae_ocv_pmk(dev, apdev):
    """SAE with OCV and fetching PMK (successful 4-way handshake)"""
    check_sae_capab(dev[0])
    params = hostapd.wpa2_params(ssid="test-sae",
                                 passphrase="12345678")
    params['wpa_key_mgmt'] = 'SAE'
    params['ieee80211w'] = '2'
    params['ocv'] = '1'
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].set("sae_groups", "")
    id = dev[0].connect("test-sae", psk="12345678", key_mgmt="SAE", ocv="1",
                        ieee80211w="2", scan_freq="2412")
    hapd.wait_sta()

    pmk_h = hapd.request("GET_PMK " + dev[0].own_addr())
    if "FAIL" in pmk_h or len(pmk_h) == 0:
        raise Exception("Failed to fetch PMK from hostapd during a successful authentication")

    pmk_w = dev[0].get_pmk(id)
    if pmk_h != pmk_w:
        raise Exception("Fetched PMK does not match: hostapd %s, wpa_supplicant %s" % (pmk_h, pmk_w))

def test_sae_ocv_pmk_failure(dev, apdev):
    """SAE with OCV and fetching PMK (failed 4-way handshake)"""
    check_sae_capab(dev[0])
    params = hostapd.wpa2_params(ssid="test-sae",
                                 passphrase="12345678")
    params['wpa_key_mgmt'] = 'SAE'
    params['ieee80211w'] = '2'
    params['ocv'] = '1'
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].set("sae_groups", "")
    dev[0].set("oci_freq_override_eapol", "2462")
    id = dev[0].connect("test-sae", psk="12345678", key_mgmt="SAE", ocv="1",
                        ieee80211w="2", scan_freq="2412", wait_connect=False)
    ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED",
                            "CTRL-EVENT-DISCONNECTED"], timeout=15)
    if ev is None:
        raise Exception("No connection result reported")
    if "CTRL-EVENT-CONNECTED" in ev:
        raise Exception("Unexpected connection")

    pmk_h = hapd.request("GET_PMK " + dev[0].own_addr())
    if "FAIL" in pmk_h or len(pmk_h) == 0:
        raise Exception("Failed to fetch PMK from hostapd during a successful authentication")

    res = dev[0].request("PMKSA_GET %d" % id)
    if not res.startswith(hapd.own_addr()):
        raise Exception("PMKSA from wpa_supplicant does not have matching BSSID")
    pmk_w = res.split(' ')[2]
    if pmk_h != pmk_w:
        raise Exception("Fetched PMK does not match: hostapd %s, wpa_supplicant %s" % (pmk_h, pmk_w))

    dev[0].request("DISCONNECT")
    time.sleep(0.1)
    pmk_h2 = hapd.request("GET_PMK " + dev[0].own_addr())
    res = dev[0].request("PMKSA_GET %d" % id)
    pmk_w2 = res.split(' ')[2]
    if pmk_h2 != pmk_h:
        raise Exception("hostapd did not report correct PMK after disconnection")
    if pmk_w2 != pmk_w:
        raise Exception("wpa_supplicant did not report correct PMK after disconnection")

def test_sae_reject(dev, apdev):
    """SAE and AP rejecting connection"""
    check_sae_capab(dev[0])
    params = hostapd.wpa2_params(ssid="test-sae",
                                 passphrase="12345678")
    params['wpa_key_mgmt'] = 'SAE'
    params['max_num_sta'] = '0'
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].set("sae_groups", "")
    id = dev[0].connect("test-sae", psk="12345678", key_mgmt="SAE",
                        scan_freq="2412", wait_connect=False)
    if not dev[0].wait_event(["CTRL-EVENT-AUTH-REJECT"], timeout=10):
        raise Exception("Authentication rejection not reported")
    dev[0].request("REMOVE_NETWORK all")
    dev[0].dump_monitor()
