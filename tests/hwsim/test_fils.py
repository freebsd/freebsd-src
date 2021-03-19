# Test cases for FILS
# Copyright (c) 2015-2017, Qualcomm Atheros, Inc.
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

import binascii
import hashlib
import logging
logger = logging.getLogger()
import os
import socket
import struct
import time

import hostapd
from tshark import run_tshark
from wpasupplicant import WpaSupplicant
import hwsim_utils
from utils import *
from test_erp import start_erp_as
from test_ap_hs20 import ip_checksum

def test_fils_sk_full_auth(dev, apdev, params):
    """FILS SK full authentication"""
    check_fils_capa(dev[0])
    check_erp_capa(dev[0])

    start_erp_as(msk_dump=os.path.join(params['logdir'], "msk.lst"))

    bssid = apdev[0]['bssid']
    params = hostapd.wpa2_eap_params(ssid="fils")
    params['wpa_key_mgmt'] = "FILS-SHA256"
    params['auth_server_port'] = "18128"
    params['erp_send_reauth_start'] = '1'
    params['erp_domain'] = 'example.com'
    params['fils_realm'] = 'example.com'
    params['wpa_group_rekey'] = '1'
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    dev[0].scan_for_bss(bssid, freq=2412)
    bss = dev[0].get_bss(bssid)
    logger.debug("BSS: " + str(bss))
    if "[FILS]" not in bss['flags']:
        raise Exception("[FILS] flag not indicated")
    if "[WPA2-FILS-SHA256-CCMP]" not in bss['flags']:
        raise Exception("[WPA2-FILS-SHA256-CCMP] flag not indicated")

    res = dev[0].request("SCAN_RESULTS")
    logger.debug("SCAN_RESULTS: " + res)
    if "[FILS]" not in res:
        raise Exception("[FILS] flag not indicated")
    if "[WPA2-FILS-SHA256-CCMP]" not in res:
        raise Exception("[WPA2-FILS-SHA256-CCMP] flag not indicated")

    dev[0].request("ERP_FLUSH")
    dev[0].connect("fils", key_mgmt="FILS-SHA256",
                   eap="PSK", identity="psk.user@example.com",
                   password_hex="0123456789abcdef0123456789abcdef",
                   erp="1", scan_freq="2412")
    hwsim_utils.test_connectivity(dev[0], hapd)

    ev = dev[0].wait_event(["WPA: Group rekeying completed"], timeout=2)
    if ev is None:
        raise Exception("GTK rekey timed out")
    hwsim_utils.test_connectivity(dev[0], hapd)

    conf = hapd.get_config()
    if conf['key_mgmt'] != 'FILS-SHA256':
        raise Exception("Unexpected config key_mgmt: " + conf['key_mgmt'])

def test_fils_sk_sha384_full_auth(dev, apdev, params):
    """FILS SK full authentication (SHA384)"""
    check_fils_capa(dev[0])
    check_erp_capa(dev[0])

    start_erp_as(msk_dump=os.path.join(params['logdir'], "msk.lst"))

    bssid = apdev[0]['bssid']
    params = hostapd.wpa2_eap_params(ssid="fils")
    params['wpa_key_mgmt'] = "FILS-SHA384"
    params['auth_server_port'] = "18128"
    params['erp_send_reauth_start'] = '1'
    params['erp_domain'] = 'example.com'
    params['fils_realm'] = 'example.com'
    params['wpa_group_rekey'] = '1'
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    dev[0].scan_for_bss(bssid, freq=2412)
    bss = dev[0].get_bss(bssid)
    logger.debug("BSS: " + str(bss))
    if "[FILS]" not in bss['flags']:
        raise Exception("[FILS] flag not indicated")
    if "[WPA2-FILS-SHA384-CCMP]" not in bss['flags']:
        raise Exception("[WPA2-FILS-SHA384-CCMP] flag not indicated")

    res = dev[0].request("SCAN_RESULTS")
    logger.debug("SCAN_RESULTS: " + res)
    if "[FILS]" not in res:
        raise Exception("[FILS] flag not indicated")
    if "[WPA2-FILS-SHA384-CCMP]" not in res:
        raise Exception("[WPA2-FILS-SHA384-CCMP] flag not indicated")

    dev[0].request("ERP_FLUSH")
    dev[0].connect("fils", key_mgmt="FILS-SHA384",
                   eap="PSK", identity="psk.user@example.com",
                   password_hex="0123456789abcdef0123456789abcdef",
                   erp="1", scan_freq="2412")
    hwsim_utils.test_connectivity(dev[0], hapd)

    ev = dev[0].wait_event(["WPA: Group rekeying completed"], timeout=2)
    if ev is None:
        raise Exception("GTK rekey timed out")
    hwsim_utils.test_connectivity(dev[0], hapd)

    conf = hapd.get_config()
    if conf['key_mgmt'] != 'FILS-SHA384':
        raise Exception("Unexpected config key_mgmt: " + conf['key_mgmt'])

def test_fils_sk_pmksa_caching(dev, apdev, params):
    """FILS SK and PMKSA caching"""
    check_fils_capa(dev[0])
    check_erp_capa(dev[0])

    start_erp_as(msk_dump=os.path.join(params['logdir'], "msk.lst"))

    bssid = apdev[0]['bssid']
    params = hostapd.wpa2_eap_params(ssid="fils")
    params['wpa_key_mgmt'] = "FILS-SHA256"
    params['auth_server_port'] = "18128"
    params['erp_domain'] = 'example.com'
    params['fils_realm'] = 'example.com'
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    dev[0].scan_for_bss(bssid, freq=2412)
    dev[0].request("ERP_FLUSH")
    id = dev[0].connect("fils", key_mgmt="FILS-SHA256",
                        eap="PSK", identity="psk.user@example.com",
                        password_hex="0123456789abcdef0123456789abcdef",
                        erp="1", scan_freq="2412")
    pmksa = dev[0].get_pmksa(bssid)
    if pmksa is None:
        raise Exception("No PMKSA cache entry created")

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    dev[0].dump_monitor()
    dev[0].select_network(id, freq=2412)
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED",
                            "CTRL-EVENT-CONNECTED"], timeout=10)
    if ev is None:
        raise Exception("Connection using PMKSA caching timed out")
    if "CTRL-EVENT-EAP-STARTED" in ev:
        raise Exception("Unexpected EAP exchange")
    hwsim_utils.test_connectivity(dev[0], hapd)
    pmksa2 = dev[0].get_pmksa(bssid)
    if pmksa2 is None:
        raise Exception("No PMKSA cache entry found")
    if pmksa['pmkid'] != pmksa2['pmkid']:
        raise Exception("Unexpected PMKID change")

    # Verify EAPOL reauthentication after FILS authentication
    hapd.request("EAPOL_REAUTH " + dev[0].own_addr())
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED"], timeout=5)
    if ev is None:
        raise Exception("EAP authentication did not start")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-SUCCESS"], timeout=5)
    if ev is None:
        raise Exception("EAP authentication did not succeed")
    time.sleep(0.1)
    hwsim_utils.test_connectivity(dev[0], hapd)

def test_fils_sk_pmksa_caching_ocv(dev, apdev, params):
    """FILS SK and PMKSA caching with OCV"""
    check_fils_capa(dev[0])
    check_erp_capa(dev[0])

    start_erp_as(msk_dump=os.path.join(params['logdir'], "msk.lst"))

    bssid = apdev[0]['bssid']
    params = hostapd.wpa2_eap_params(ssid="fils")
    params['wpa_key_mgmt'] = "FILS-SHA256"
    params['auth_server_port'] = "18128"
    params['erp_domain'] = 'example.com'
    params['fils_realm'] = 'example.com'
    params['ieee80211w'] = '1'
    params['ocv'] = '1'
    try:
        hapd = hostapd.add_ap(apdev[0]['ifname'], params)
    except Exception as e:
        if "Failed to set hostapd parameter ocv" in str(e):
            raise HwsimSkip("OCV not supported")
        raise

    dev[0].scan_for_bss(bssid, freq=2412)
    dev[0].request("ERP_FLUSH")
    id = dev[0].connect("fils", key_mgmt="FILS-SHA256",
                        eap="PSK", identity="psk.user@example.com",
                        password_hex="0123456789abcdef0123456789abcdef",
                        erp="1", scan_freq="2412", ieee80211w="1", ocv="1")
    pmksa = dev[0].get_pmksa(bssid)
    if pmksa is None:
        raise Exception("No PMKSA cache entry created")

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    dev[0].dump_monitor()
    dev[0].select_network(id, freq=2412)
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED",
                            "CTRL-EVENT-CONNECTED"], timeout=10)
    if ev is None:
        raise Exception("Connection using PMKSA caching timed out")
    if "CTRL-EVENT-EAP-STARTED" in ev:
        raise Exception("Unexpected EAP exchange")
    hwsim_utils.test_connectivity(dev[0], hapd)
    pmksa2 = dev[0].get_pmksa(bssid)
    if pmksa2 is None:
        raise Exception("No PMKSA cache entry found")
    if pmksa['pmkid'] != pmksa2['pmkid']:
        raise Exception("Unexpected PMKID change")

    # Verify EAPOL reauthentication after FILS authentication
    hapd.request("EAPOL_REAUTH " + dev[0].own_addr())
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED"], timeout=5)
    if ev is None:
        raise Exception("EAP authentication did not start")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-SUCCESS"], timeout=5)
    if ev is None:
        raise Exception("EAP authentication did not succeed")
    time.sleep(0.1)
    hwsim_utils.test_connectivity(dev[0], hapd)

def test_fils_sk_pmksa_caching_and_cache_id(dev, apdev):
    """FILS SK and PMKSA caching with Cache Identifier"""
    check_fils_capa(dev[0])
    check_erp_capa(dev[0])

    bssid = apdev[0]['bssid']
    params = hostapd.wpa2_eap_params(ssid="fils")
    params['wpa_key_mgmt'] = "FILS-SHA256"
    params['auth_server_port'] = "18128"
    params['erp_domain'] = 'example.com'
    params['fils_realm'] = 'example.com'
    params['fils_cache_id'] = "abcd"
    params["radius_server_clients"] = "auth_serv/radius_clients.conf"
    params["radius_server_auth_port"] = '18128'
    params["eap_server"] = "1"
    params["eap_user_file"] = "auth_serv/eap_user.conf"
    params["ca_cert"] = "auth_serv/ca.pem"
    params["server_cert"] = "auth_serv/server.pem"
    params["private_key"] = "auth_serv/server.key"
    params["eap_sim_db"] = "unix:/tmp/hlr_auc_gw.sock"
    params["dh_file"] = "auth_serv/dh.conf"
    params["pac_opaque_encr_key"] = "000102030405060708090a0b0c0d0e0f"
    params["eap_fast_a_id"] = "101112131415161718191a1b1c1d1e1f"
    params["eap_fast_a_id_info"] = "test server"
    params["eap_server_erp"] = "1"
    params["erp_domain"] = "example.com"
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    dev[0].scan_for_bss(bssid, freq=2412)
    dev[0].request("ERP_FLUSH")
    id = dev[0].connect("fils", key_mgmt="FILS-SHA256",
                        eap="PSK", identity="psk.user@example.com",
                        password_hex="0123456789abcdef0123456789abcdef",
                        erp="1", scan_freq="2412")
    res = dev[0].request("PMKSA")
    if "FILS Cache Identifier" not in res:
        raise Exception("PMKSA list does not include FILS Cache Identifier")
    pmksa = dev[0].get_pmksa(bssid)
    if pmksa is None:
        raise Exception("No PMKSA cache entry created")
    if "cache_id" not in pmksa:
        raise Exception("No FILS Cache Identifier listed")
    if pmksa["cache_id"] != "abcd":
        raise Exception("The configured FILS Cache Identifier not seen in PMKSA")

    bssid2 = apdev[1]['bssid']
    params = hostapd.wpa2_eap_params(ssid="fils")
    params['wpa_key_mgmt'] = "FILS-SHA256"
    params['auth_server_port'] = "18128"
    params['erp_domain'] = 'example.com'
    params['fils_realm'] = 'example.com'
    params['fils_cache_id'] = "abcd"
    hapd2 = hostapd.add_ap(apdev[1]['ifname'], params)

    dev[0].scan_for_bss(bssid2, freq=2412)

    dev[0].dump_monitor()
    if "OK" not in dev[0].request("ROAM " + bssid2):
        raise Exception("ROAM failed")

    ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED",
                            "CTRL-EVENT-CONNECTED"], timeout=10)
    if ev is None:
        raise Exception("Connection using PMKSA caching timed out")
    if "CTRL-EVENT-EAP-STARTED" in ev:
        raise Exception("Unexpected EAP exchange")
    if bssid2 not in ev:
        raise Exception("Failed to connect to the second AP")

    hwsim_utils.test_connectivity(dev[0], hapd2)
    pmksa2 = dev[0].get_pmksa(bssid2)
    if pmksa2:
        raise Exception("Unexpected extra PMKSA cache added")
    pmksa2 = dev[0].get_pmksa(bssid)
    if not pmksa2:
        raise Exception("Original PMKSA cache entry removed")
    if pmksa['pmkid'] != pmksa2['pmkid']:
        raise Exception("Unexpected PMKID change")

def test_fils_sk_pmksa_caching_ctrl_ext(dev, apdev, params):
    """FILS SK and PMKSA caching with Cache Identifier and external management"""
    check_fils_capa(dev[0])
    check_erp_capa(dev[0])

    hapd_as = start_erp_as(msk_dump=os.path.join(params['logdir'], "msk.lst"))

    bssid = apdev[0]['bssid']
    params = hostapd.wpa2_eap_params(ssid="fils")
    params['wpa_key_mgmt'] = "FILS-SHA384"
    params['auth_server_port'] = "18128"
    params['erp_send_reauth_start'] = '1'
    params['erp_domain'] = 'example.com'
    params['fils_realm'] = 'example.com'
    params['fils_cache_id'] = "ffee"
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    dev[0].scan_for_bss(bssid, freq=2412)
    dev[0].request("ERP_FLUSH")
    id = dev[0].connect("fils", key_mgmt="FILS-SHA384",
                        eap="PSK", identity="psk.user@example.com",
                        password_hex="0123456789abcdef0123456789abcdef",
                        erp="1", scan_freq="2412")

    res1 = dev[0].request("PMKSA_GET %d" % id)
    logger.info("PMKSA_GET: " + res1)
    if "UNKNOWN COMMAND" in res1:
        raise HwsimSkip("PMKSA_GET not supported in the build")
    if bssid not in res1:
        raise Exception("PMKSA cache entry missing")
    if "ffee" not in res1:
        raise Exception("FILS Cache Identifier not seen in PMKSA cache entry")

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()
    hapd_as.disable()

    dev[0].scan_for_bss(bssid, freq=2412)
    dev[0].request("PMKSA_FLUSH")
    dev[0].request("ERP_FLUSH")
    for entry in res1.splitlines():
        if "OK" not in dev[0].request("PMKSA_ADD %d %s" % (id, entry)):
            raise Exception("Failed to add PMKSA entry")

    bssid2 = apdev[1]['bssid']
    params = hostapd.wpa2_eap_params(ssid="fils")
    params['wpa_key_mgmt'] = "FILS-SHA384"
    params['auth_server_port'] = "18128"
    params['erp_send_reauth_start'] = '1'
    params['erp_domain'] = 'example.com'
    params['fils_realm'] = 'example.com'
    params['fils_cache_id'] = "ffee"
    hapd2 = hostapd.add_ap(apdev[1]['ifname'], params)

    dev[0].scan_for_bss(bssid2, freq=2412)
    dev[0].set_network(id, "bssid", bssid2)
    dev[0].select_network(id, freq=2412)
    ev = dev[0].wait_connected()
    if bssid2 not in ev:
        raise Exception("Unexpected BSS selected")

def test_fils_sk_erp(dev, apdev, params):
    """FILS SK using ERP"""
    run_fils_sk_erp(dev, apdev, "FILS-SHA256", params)

def test_fils_sk_erp_sha384(dev, apdev, params):
    """FILS SK using ERP and SHA384"""
    run_fils_sk_erp(dev, apdev, "FILS-SHA384", params)

def run_fils_sk_erp(dev, apdev, key_mgmt, params):
    check_fils_capa(dev[0])
    check_erp_capa(dev[0])

    start_erp_as(msk_dump=os.path.join(params['logdir'], "msk.lst"))

    bssid = apdev[0]['bssid']
    params = hostapd.wpa2_eap_params(ssid="fils")
    params['wpa_key_mgmt'] = key_mgmt
    params['auth_server_port'] = "18128"
    params['erp_domain'] = 'example.com'
    params['fils_realm'] = 'example.com'
    params['disable_pmksa_caching'] = '1'
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    dev[0].scan_for_bss(bssid, freq=2412)
    dev[0].request("ERP_FLUSH")
    id = dev[0].connect("fils", key_mgmt=key_mgmt,
                        eap="PSK", identity="psk.user@example.com",
                        password_hex="0123456789abcdef0123456789abcdef",
                        erp="1", scan_freq="2412")

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    dev[0].dump_monitor()
    dev[0].select_network(id, freq=2412)
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED",
                            "EVENT-ASSOC-REJECT",
                            "CTRL-EVENT-CONNECTED"], timeout=10)
    if ev is None:
        raise Exception("Connection using FILS/ERP timed out")
    if "CTRL-EVENT-EAP-STARTED" in ev:
        raise Exception("Unexpected EAP exchange")
    if "EVENT-ASSOC-REJECT" in ev:
        raise Exception("Association failed")
    hwsim_utils.test_connectivity(dev[0], hapd)

def test_fils_sk_erp_followed_by_pmksa_caching(dev, apdev, params):
    """FILS SK ERP following by PMKSA caching"""
    check_fils_capa(dev[0])
    check_erp_capa(dev[0])

    start_erp_as(msk_dump=os.path.join(params['logdir'], "msk.lst"))

    bssid = apdev[0]['bssid']
    params = hostapd.wpa2_eap_params(ssid="fils")
    params['wpa_key_mgmt'] = "FILS-SHA256"
    params['auth_server_port'] = "18128"
    params['erp_domain'] = 'example.com'
    params['fils_realm'] = 'example.com'
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    dev[0].scan_for_bss(bssid, freq=2412)
    dev[0].request("ERP_FLUSH")
    id = dev[0].connect("fils", key_mgmt="FILS-SHA256",
                        eap="PSK", identity="psk.user@example.com",
                        password_hex="0123456789abcdef0123456789abcdef",
                        erp="1", scan_freq="2412")

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    # Force the second connection to use ERP by deleting the PMKSA entry.
    dev[0].request("PMKSA_FLUSH")

    dev[0].dump_monitor()
    dev[0].select_network(id, freq=2412)
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED",
                            "EVENT-ASSOC-REJECT",
                            "CTRL-EVENT-CONNECTED"], timeout=10)
    if ev is None:
        raise Exception("Connection using FILS/ERP timed out")
    if "CTRL-EVENT-EAP-STARTED" in ev:
        raise Exception("Unexpected EAP exchange")
    if "EVENT-ASSOC-REJECT" in ev:
        raise Exception("Association failed")
    hwsim_utils.test_connectivity(dev[0], hapd)

    pmksa = dev[0].get_pmksa(bssid)
    if pmksa is None:
        raise Exception("No PMKSA cache entry created")

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    # The third connection is expected to use PMKSA caching for FILS
    # authentication.
    dev[0].dump_monitor()
    dev[0].select_network(id, freq=2412)
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED",
                            "EVENT-ASSOC-REJECT",
                            "CTRL-EVENT-CONNECTED"], timeout=10)
    if ev is None:
        raise Exception("Connection using PMKSA caching timed out")
    if "CTRL-EVENT-EAP-STARTED" in ev:
        raise Exception("Unexpected EAP exchange")
    if "EVENT-ASSOC-REJECT" in ev:
        raise Exception("Association failed")
    hwsim_utils.test_connectivity(dev[0], hapd)

    pmksa2 = dev[0].get_pmksa(bssid)
    if pmksa2 is None:
        raise Exception("No PMKSA cache entry found")
    if pmksa['pmkid'] != pmksa2['pmkid']:
        raise Exception("Unexpected PMKID change")

def test_fils_sk_erp_another_ssid(dev, apdev, params):
    """FILS SK using ERP and roam to another SSID"""
    check_fils_capa(dev[0])
    check_erp_capa(dev[0])

    start_erp_as(msk_dump=os.path.join(params['logdir'], "msk.lst"))

    bssid = apdev[0]['bssid']
    params = hostapd.wpa2_eap_params(ssid="fils")
    params['wpa_key_mgmt'] = "FILS-SHA256"
    params['auth_server_port'] = "18128"
    params['erp_domain'] = 'example.com'
    params['fils_realm'] = 'example.com'
    params['disable_pmksa_caching'] = '1'
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    dev[0].scan_for_bss(bssid, freq=2412)
    dev[0].request("ERP_FLUSH")
    id = dev[0].connect("fils", key_mgmt="FILS-SHA256",
                        eap="PSK", identity="psk.user@example.com",
                        password_hex="0123456789abcdef0123456789abcdef",
                        erp="1", scan_freq="2412")

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()
    hapd.disable()
    dev[0].flush_scan_cache()
    if "FAIL" in dev[0].request("PMKSA_FLUSH"):
        raise Exception("PMKSA_FLUSH failed")

    params = hostapd.wpa2_eap_params(ssid="fils2")
    params['wpa_key_mgmt'] = "FILS-SHA256"
    params['auth_server_port'] = "18128"
    params['erp_domain'] = 'example.com'
    params['fils_realm'] = 'example.com'
    params['disable_pmksa_caching'] = '1'
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    dev[0].scan_for_bss(bssid, freq=2412)
    dev[0].dump_monitor()
    id = dev[0].connect("fils2", key_mgmt="FILS-SHA256",
                        eap="PSK", identity="psk.user@example.com",
                        password_hex="0123456789abcdef0123456789abcdef",
                        erp="1", scan_freq="2412", wait_connect=False)

    ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED",
                            "EVENT-ASSOC-REJECT",
                            "CTRL-EVENT-CONNECTED"], timeout=10)
    if ev is None:
        raise Exception("Connection using FILS/ERP timed out")
    if "CTRL-EVENT-EAP-STARTED" in ev:
        raise Exception("Unexpected EAP exchange")
    if "EVENT-ASSOC-REJECT" in ev:
        raise Exception("Association failed")
    hwsim_utils.test_connectivity(dev[0], hapd)

def test_fils_sk_multiple_realms(dev, apdev, params):
    """FILS SK and multiple realms"""
    check_fils_capa(dev[0])
    check_erp_capa(dev[0])

    start_erp_as(msk_dump=os.path.join(params['logdir'], "msk.lst"))

    bssid = apdev[0]['bssid']
    params = hostapd.wpa2_eap_params(ssid="fils")
    params['wpa_key_mgmt'] = "FILS-SHA256"
    params['auth_server_port'] = "18128"
    params['erp_domain'] = 'example.com'
    fils_realms = ['r1.example.org', 'r2.EXAMPLE.org', 'r3.example.org',
                   'r4.example.org', 'r5.example.org', 'r6.example.org',
                   'r7.example.org', 'r8.example.org',
                   'example.com',
                   'r9.example.org', 'r10.example.org', 'r11.example.org',
                   'r12.example.org', 'r13.example.org', 'r14.example.org',
                   'r15.example.org', 'r16.example.org']
    params['fils_realm'] = fils_realms
    params['fils_cache_id'] = "1234"
    params['hessid'] = bssid
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    dev[0].scan_for_bss(bssid, freq=2412)

    if "OK" not in dev[0].request("ANQP_GET " + bssid + " 275"):
        raise Exception("ANQP_GET command failed")
    ev = dev[0].wait_event(["GAS-QUERY-DONE"], timeout=10)
    if ev is None:
        raise Exception("GAS query timed out")
    bss = dev[0].get_bss(bssid)

    if 'fils_info' not in bss:
        raise Exception("FILS Indication element information missing")
    if bss['fils_info'] != '02b8':
        raise Exception("Unexpected FILS Information: " + bss['fils_info'])

    if 'fils_cache_id' not in bss:
        raise Exception("FILS Cache Identifier missing")
    if bss['fils_cache_id'] != '1234':
        raise Exception("Unexpected FILS Cache Identifier: " + bss['fils_cache_id'])

    if 'fils_realms' not in bss:
        raise Exception("FILS Realm Identifiers missing")
    expected = ''
    count = 0
    for realm in fils_realms:
        hash = hashlib.sha256(realm.lower().encode()).digest()
        expected += binascii.hexlify(hash[0:2]).decode()
        count += 1
        if count == 7:
            break
    if bss['fils_realms'] != expected:
        raise Exception("Unexpected FILS Realm Identifiers: " + bss['fils_realms'])

    if 'anqp_fils_realm_info' not in bss:
        raise Exception("FILS Realm Information ANQP-element not seen")
    info = bss['anqp_fils_realm_info']
    expected = ''
    for realm in fils_realms:
        hash = hashlib.sha256(realm.lower().encode()).digest()
        expected += binascii.hexlify(hash[0:2]).decode()
    if info != expected:
        raise Exception("Unexpected FILS Realm Info ANQP-element: " + info)

    dev[0].request("ERP_FLUSH")
    id = dev[0].connect("fils", key_mgmt="FILS-SHA256",
                        eap="PSK", identity="psk.user@example.com",
                        password_hex="0123456789abcdef0123456789abcdef",
                        erp="1", scan_freq="2412")

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    dev[0].dump_monitor()
    dev[0].select_network(id, freq=2412)
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED",
                            "EVENT-ASSOC-REJECT",
                            "CTRL-EVENT-CONNECTED"], timeout=10)
    if ev is None:
        raise Exception("Connection using FILS/ERP timed out")
    if "CTRL-EVENT-EAP-STARTED" in ev:
        raise Exception("Unexpected EAP exchange")
    if "EVENT-ASSOC-REJECT" in ev:
        raise Exception("Association failed")
    hwsim_utils.test_connectivity(dev[0], hapd)

# DHCP message op codes
BOOTREQUEST = 1
BOOTREPLY = 2

OPT_PAD = 0
OPT_DHCP_MESSAGE_TYPE = 53
OPT_RAPID_COMMIT = 80
OPT_END = 255

DHCPDISCOVER = 1
DHCPOFFER = 2
DHCPREQUEST = 3
DHCPDECLINE = 4
DHCPACK = 5
DHCPNAK = 6
DHCPRELEASE = 7
DHCPINFORM = 8

def build_dhcp(req, dhcp_msg, chaddr, giaddr="0.0.0.0",
               ip_src="0.0.0.0", ip_dst="255.255.255.255",
               rapid_commit=True, override_op=None, magic_override=None,
               opt_end=True, extra_op=None):
    proto = b'\x08\x00' # IPv4
    _ip_src = socket.inet_pton(socket.AF_INET, ip_src)
    _ip_dst = socket.inet_pton(socket.AF_INET, ip_dst)

    _ciaddr = b'\x00\x00\x00\x00'
    _yiaddr = b'\x00\x00\x00\x00'
    _siaddr = b'\x00\x00\x00\x00'
    _giaddr = socket.inet_pton(socket.AF_INET, giaddr)
    _chaddr = binascii.unhexlify(chaddr.replace(':', '')) + 10 * b'\x00'
    htype = 1 # Hardware address type; 1 = Ethernet
    hlen = 6 # Hardware address length
    hops = 0
    xid = 123456
    secs = 0
    flags = 0
    if req:
        op = BOOTREQUEST
        src_port = 68
        dst_port = 67
    else:
        op = BOOTREPLY
        src_port = 67
        dst_port = 68
    if override_op is not None:
        op = override_op
    payload = struct.pack('>BBBBLHH', op, htype, hlen, hops, xid, secs, flags)
    sname = 64*b'\x00'
    file = 128*b'\x00'
    payload += _ciaddr + _yiaddr + _siaddr + _giaddr + _chaddr + sname + file
    # magic - DHCP
    if magic_override is not None:
        payload += magic_override
    else:
        payload += b'\x63\x82\x53\x63'
    # Option: DHCP Message Type
    if dhcp_msg is not None:
        payload += struct.pack('BBB', OPT_DHCP_MESSAGE_TYPE, 1, dhcp_msg)
    if rapid_commit:
        # Option: Rapid Commit
        payload += struct.pack('BB', OPT_RAPID_COMMIT, 0)
    if extra_op:
        payload += extra_op
    # End Option
    if opt_end:
        payload += struct.pack('B', OPT_END)

    udp = struct.pack('>HHHH', src_port, dst_port,
                      8 + len(payload), 0) + payload

    tot_len = 20 + len(udp)
    start = struct.pack('>BBHHBBBB', 0x45, 0, tot_len, 0, 0, 0, 128, 17)
    ipv4 = start + b'\x00\x00' + _ip_src + _ip_dst
    csum = ip_checksum(ipv4)
    ipv4 = start + csum + _ip_src + _ip_dst

    return proto + ipv4 + udp

def fils_hlp_config(fils_hlp_wait_time=10000):
    params = hostapd.wpa2_eap_params(ssid="fils")
    params['wpa_key_mgmt'] = "FILS-SHA256"
    params['auth_server_port'] = "18128"
    params['erp_domain'] = 'example.com'
    params['fils_realm'] = 'example.com'
    params['disable_pmksa_caching'] = '1'
    params['own_ip_addr'] = '127.0.0.3'
    params['dhcp_server'] = '127.0.0.2'
    params['fils_hlp_wait_time'] = str(fils_hlp_wait_time)
    return params

def test_fils_sk_hlp(dev, apdev, params):
    """FILS SK HLP (rapid commit server)"""
    run_fils_sk_hlp(dev, apdev, True, params)

def test_fils_sk_hlp_no_rapid_commit(dev, apdev, params):
    """FILS SK HLP (no rapid commit server)"""
    run_fils_sk_hlp(dev, apdev, False, params)

def run_fils_sk_hlp(dev, apdev, rapid_commit_server, params):
    check_fils_capa(dev[0])
    check_erp_capa(dev[0])

    start_erp_as(msk_dump=os.path.join(params['logdir'], "msk.lst"))

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.settimeout(5)
    sock.bind(("127.0.0.2", 67))

    bssid = apdev[0]['bssid']
    params = fils_hlp_config()
    params['fils_hlp_wait_time'] = '10000'
    if not rapid_commit_server:
        params['dhcp_rapid_commit_proxy'] = '1'
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    dev[0].scan_for_bss(bssid, freq=2412)
    dev[0].request("ERP_FLUSH")
    if "OK" not in dev[0].request("FILS_HLP_REQ_FLUSH"):
        raise Exception("Failed to flush pending FILS HLP requests")
    tests = ["",
             "q",
             "ff:ff:ff:ff:ff:ff",
             "ff:ff:ff:ff:ff:ff q"]
    for t in tests:
        if "FAIL" not in dev[0].request("FILS_HLP_REQ_ADD " + t):
            raise Exception("Invalid FILS_HLP_REQ_ADD accepted: " + t)
    dhcpdisc = build_dhcp(req=True, dhcp_msg=DHCPDISCOVER,
                          chaddr=dev[0].own_addr())
    tests = ["ff:ff:ff:ff:ff:ff aabb",
             "ff:ff:ff:ff:ff:ff " + 255*'cc',
             hapd.own_addr() + " ddee010203040506070809",
             "ff:ff:ff:ff:ff:ff " + binascii.hexlify(dhcpdisc).decode()]
    for t in tests:
        if "OK" not in dev[0].request("FILS_HLP_REQ_ADD " + t):
            raise Exception("FILS_HLP_REQ_ADD failed: " + t)
    id = dev[0].connect("fils", key_mgmt="FILS-SHA256",
                        eap="PSK", identity="psk.user@example.com",
                        password_hex="0123456789abcdef0123456789abcdef",
                        erp="1", scan_freq="2412")

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    dev[0].dump_monitor()
    dev[0].select_network(id, freq=2412)

    (msg, addr) = sock.recvfrom(1000)
    logger.debug("Received DHCP message from %s" % str(addr))
    if rapid_commit_server:
        # TODO: Proper rapid commit response
        dhcpdisc = build_dhcp(req=False, dhcp_msg=DHCPACK,
                              chaddr=dev[0].own_addr(), giaddr="127.0.0.3")
        sock.sendto(dhcpdisc[2+20+8:], addr)
    else:
        dhcpdisc = build_dhcp(req=False, dhcp_msg=DHCPOFFER, rapid_commit=False,
                              chaddr=dev[0].own_addr(), giaddr="127.0.0.3")
        sock.sendto(dhcpdisc[2+20+8:], addr)
        (msg, addr) = sock.recvfrom(1000)
        logger.debug("Received DHCP message from %s" % str(addr))
        dhcpdisc = build_dhcp(req=False, dhcp_msg=DHCPACK, rapid_commit=False,
                              chaddr=dev[0].own_addr(), giaddr="127.0.0.3")
        sock.sendto(dhcpdisc[2+20+8:], addr)
    ev = dev[0].wait_event(["FILS-HLP-RX"], timeout=10)
    if ev is None:
        raise Exception("FILS HLP response not reported")
    vals = ev.split(' ')
    frame = binascii.unhexlify(vals[3].split('=')[1])
    proto, = struct.unpack('>H', frame[0:2])
    if proto != 0x0800:
        raise Exception("Unexpected ethertype in HLP response: %d" % proto)
    frame = frame[2:]
    ip = frame[0:20]
    if ip_checksum(ip) != b'\x00\x00':
        raise Exception("IP header checksum mismatch in HLP response")
    frame = frame[20:]
    udp = frame[0:8]
    frame = frame[8:]
    sport, dport, ulen, ucheck = struct.unpack('>HHHH', udp)
    if sport != 67 or dport != 68:
        raise Exception("Unexpected UDP port in HLP response")
    dhcp = frame[0:28]
    frame = frame[28:]
    op, htype, hlen, hops, xid, secs, flags, ciaddr, yiaddr, siaddr, giaddr = struct.unpack('>4BL2H4L', dhcp)
    chaddr = frame[0:16]
    frame = frame[16:]
    sname = frame[0:64]
    frame = frame[64:]
    file = frame[0:128]
    frame = frame[128:]
    options = frame
    if options[0:4] != b'\x63\x82\x53\x63':
        raise Exception("No DHCP magic seen in HLP response")
    options = options[4:]
    # TODO: fully parse and validate DHCPACK options
    if struct.pack('BBB', OPT_DHCP_MESSAGE_TYPE, 1, DHCPACK) not in options:
        raise Exception("DHCPACK not in HLP response")

    dev[0].wait_connected()

    dev[0].request("FILS_HLP_REQ_FLUSH")

def test_fils_sk_hlp_timeout(dev, apdev, params):
    """FILS SK HLP (rapid commit server timeout)"""
    check_fils_capa(dev[0])
    check_erp_capa(dev[0])

    start_erp_as(msk_dump=os.path.join(params['logdir'], "msk.lst"))

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.settimeout(5)
    sock.bind(("127.0.0.2", 67))

    bssid = apdev[0]['bssid']
    params = fils_hlp_config(fils_hlp_wait_time=30)
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    dev[0].scan_for_bss(bssid, freq=2412)
    dev[0].request("ERP_FLUSH")
    if "OK" not in dev[0].request("FILS_HLP_REQ_FLUSH"):
        raise Exception("Failed to flush pending FILS HLP requests")
    dhcpdisc = build_dhcp(req=True, dhcp_msg=DHCPDISCOVER,
                          chaddr=dev[0].own_addr())
    if "OK" not in dev[0].request("FILS_HLP_REQ_ADD " + "ff:ff:ff:ff:ff:ff " + binascii.hexlify(dhcpdisc).decode()):
        raise Exception("FILS_HLP_REQ_ADD failed")
    id = dev[0].connect("fils", key_mgmt="FILS-SHA256",
                        eap="PSK", identity="psk.user@example.com",
                        password_hex="0123456789abcdef0123456789abcdef",
                        erp="1", scan_freq="2412")

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    dev[0].dump_monitor()
    dev[0].select_network(id, freq=2412)

    (msg, addr) = sock.recvfrom(1000)
    logger.debug("Received DHCP message from %s" % str(addr))
    # Wait for HLP wait timeout to hit
    # FILS: HLP response timeout - continue with association response
    dev[0].wait_connected()

    dev[0].request("FILS_HLP_REQ_FLUSH")

def test_fils_sk_hlp_oom(dev, apdev, params):
    """FILS SK HLP and hostapd OOM"""
    check_fils_capa(dev[0])
    check_erp_capa(dev[0])

    start_erp_as(msk_dump=os.path.join(params['logdir'], "msk.lst"))

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.settimeout(5)
    sock.bind(("127.0.0.2", 67))

    bssid = apdev[0]['bssid']
    params = fils_hlp_config(fils_hlp_wait_time=500)
    params['dhcp_rapid_commit_proxy'] = '1'
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    dev[0].scan_for_bss(bssid, freq=2412)
    dev[0].request("ERP_FLUSH")
    if "OK" not in dev[0].request("FILS_HLP_REQ_FLUSH"):
        raise Exception("Failed to flush pending FILS HLP requests")
    dhcpdisc = build_dhcp(req=True, dhcp_msg=DHCPDISCOVER,
                          chaddr=dev[0].own_addr())
    if "OK" not in dev[0].request("FILS_HLP_REQ_ADD " + "ff:ff:ff:ff:ff:ff " + binascii.hexlify(dhcpdisc).decode()):
        raise Exception("FILS_HLP_REQ_ADD failed")
    id = dev[0].connect("fils", key_mgmt="FILS-SHA256",
                        eap="PSK", identity="psk.user@example.com",
                        password_hex="0123456789abcdef0123456789abcdef",
                        erp="1", scan_freq="2412")

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    dev[0].dump_monitor()
    with alloc_fail(hapd, 1, "fils_process_hlp"):
        dev[0].select_network(id, freq=2412)
        dev[0].wait_connected()
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    dev[0].dump_monitor()
    with alloc_fail(hapd, 1, "fils_process_hlp_dhcp"):
        dev[0].select_network(id, freq=2412)
        dev[0].wait_connected()
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    dev[0].dump_monitor()
    with alloc_fail(hapd, 1, "wpabuf_alloc;fils_process_hlp_dhcp"):
        dev[0].select_network(id, freq=2412)
        dev[0].wait_connected()
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    dev[0].dump_monitor()
    with alloc_fail(hapd, 1, "wpabuf_alloc;fils_dhcp_handler"):
        dev[0].select_network(id, freq=2412)
        (msg, addr) = sock.recvfrom(1000)
        logger.debug("Received DHCP message from %s" % str(addr))
        dhcpdisc = build_dhcp(req=False, dhcp_msg=DHCPACK,
                              chaddr=dev[0].own_addr(), giaddr="127.0.0.3")
        sock.sendto(dhcpdisc[2+20+8:], addr)
        dev[0].wait_connected()
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    dev[0].dump_monitor()
    with alloc_fail(hapd, 1, "wpabuf_resize;fils_dhcp_handler"):
        dev[0].select_network(id, freq=2412)
        (msg, addr) = sock.recvfrom(1000)
        logger.debug("Received DHCP message from %s" % str(addr))
        dhcpdisc = build_dhcp(req=False, dhcp_msg=DHCPACK,
                              chaddr=dev[0].own_addr(), giaddr="127.0.0.3")
        sock.sendto(dhcpdisc[2+20+8:], addr)
        dev[0].wait_connected()
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    dev[0].dump_monitor()
    dev[0].select_network(id, freq=2412)
    (msg, addr) = sock.recvfrom(1000)
    logger.debug("Received DHCP message from %s" % str(addr))
    dhcpoffer = build_dhcp(req=False, dhcp_msg=DHCPOFFER, rapid_commit=False,
                           chaddr=dev[0].own_addr(), giaddr="127.0.0.3")
    with alloc_fail(hapd, 1, "wpabuf_resize;fils_dhcp_request"):
        sock.sendto(dhcpoffer[2+20+8:], addr)
        dev[0].wait_connected()
        dev[0].request("DISCONNECT")
        dev[0].wait_disconnected()

    dev[0].request("FILS_HLP_REQ_FLUSH")

def test_fils_sk_hlp_req_parsing(dev, apdev, params):
    """FILS SK HLP request parsing"""
    check_fils_capa(dev[0])
    check_erp_capa(dev[0])

    start_erp_as(msk_dump=os.path.join(params['logdir'], "msk.lst"))

    bssid = apdev[0]['bssid']
    params = fils_hlp_config(fils_hlp_wait_time=30)
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    dev[0].scan_for_bss(bssid, freq=2412)
    dev[0].request("ERP_FLUSH")
    if "OK" not in dev[0].request("FILS_HLP_REQ_FLUSH"):
        raise Exception("Failed to flush pending FILS HLP requests")

    tot_len = 20 + 1
    start = struct.pack('>BBHHBBBB', 0x45, 0, tot_len, 0, 0, 0, 128, 17)
    _ip_src = b'\x00\x00\x00\x00'
    _ip_dst = b'\x00\x00\x00\x00'
    ipv4 = start + b'\x00\x00' + _ip_src + _ip_dst
    csum = ip_checksum(ipv4)
    ipv4_overflow = start + csum + _ip_src + _ip_dst

    tot_len = 20
    start = struct.pack('>BBHHBBBB', 0x45, 0, tot_len, 0, 0, 0, 128, 123)
    ipv4 = start + b'\x00\x00' + _ip_src + _ip_dst
    csum = ip_checksum(ipv4)
    ipv4_unknown_proto = start + csum + _ip_src + _ip_dst

    tot_len = 20
    start = struct.pack('>BBHHBBBB', 0x45, 0, tot_len, 0, 0, 0, 128, 17)
    ipv4 = start + b'\x00\x00' + _ip_src + _ip_dst
    csum = ip_checksum(ipv4)
    ipv4_missing_udp_hdr = start + csum + _ip_src + _ip_dst

    src_port = 68
    dst_port = 67
    udp = struct.pack('>HHHH', src_port, dst_port, 8 + 1, 0)
    tot_len = 20 + len(udp)
    start = struct.pack('>BBHHBBBB', 0x45, 0, tot_len, 0, 0, 0, 128, 17)
    ipv4 = start + b'\x00\x00' + _ip_src + _ip_dst
    csum = ip_checksum(ipv4)
    udp_overflow = start + csum + _ip_src + _ip_dst + udp

    udp = struct.pack('>HHHH', src_port, dst_port, 7, 0)
    tot_len = 20 + len(udp)
    start = struct.pack('>BBHHBBBB', 0x45, 0, tot_len, 0, 0, 0, 128, 17)
    ipv4 = start + b'\x00\x00' + _ip_src + _ip_dst
    csum = ip_checksum(ipv4)
    udp_underflow = start + csum + _ip_src + _ip_dst + udp

    src_port = 123
    dst_port = 456
    udp = struct.pack('>HHHH', src_port, dst_port, 8, 0)
    tot_len = 20 + len(udp)
    start = struct.pack('>BBHHBBBB', 0x45, 0, tot_len, 0, 0, 0, 128, 17)
    ipv4 = start + b'\x00\x00' + _ip_src + _ip_dst
    csum = ip_checksum(ipv4)
    udp_unknown_port = start + csum + _ip_src + _ip_dst + udp

    src_port = 68
    dst_port = 67
    udp = struct.pack('>HHHH', src_port, dst_port, 8, 0)
    tot_len = 20 + len(udp)
    start = struct.pack('>BBHHBBBB', 0x45, 0, tot_len, 0, 0, 0, 128, 17)
    ipv4 = start + b'\x00\x00' + _ip_src + _ip_dst
    csum = ip_checksum(ipv4)
    dhcp_missing_data = start + csum + _ip_src + _ip_dst + udp

    dhcp_not_req = build_dhcp(req=True, dhcp_msg=DHCPDISCOVER,
                              chaddr=dev[0].own_addr(), override_op=BOOTREPLY)
    dhcp_no_magic = build_dhcp(req=True, dhcp_msg=None,
                               chaddr=dev[0].own_addr(), magic_override=b'',
                               rapid_commit=False, opt_end=False)
    dhcp_unknown_magic = build_dhcp(req=True, dhcp_msg=DHCPDISCOVER,
                                    chaddr=dev[0].own_addr(),
                                    magic_override=b'\x00\x00\x00\x00')
    dhcp_opts = build_dhcp(req=True, dhcp_msg=DHCPNAK,
                           chaddr=dev[0].own_addr(),
                           extra_op=b'\x00\x11', opt_end=False)
    dhcp_opts2 = build_dhcp(req=True, dhcp_msg=DHCPNAK,
                            chaddr=dev[0].own_addr(),
                            extra_op=b'\x11\x01', opt_end=False)
    dhcp_valid = build_dhcp(req=True, dhcp_msg=DHCPDISCOVER,
                            chaddr=dev[0].own_addr())

    tests = ["ff",
             "0800",
             "0800" + 20*"00",
             "0800" + binascii.hexlify(ipv4_overflow).decode(),
             "0800" + binascii.hexlify(ipv4_unknown_proto).decode(),
             "0800" + binascii.hexlify(ipv4_missing_udp_hdr).decode(),
             "0800" + binascii.hexlify(udp_overflow).decode(),
             "0800" + binascii.hexlify(udp_underflow).decode(),
             "0800" + binascii.hexlify(udp_unknown_port).decode(),
             "0800" + binascii.hexlify(dhcp_missing_data).decode(),
             binascii.hexlify(dhcp_not_req).decode(),
             binascii.hexlify(dhcp_no_magic).decode(),
             binascii.hexlify(dhcp_unknown_magic).decode()]
    for t in tests:
        if "OK" not in dev[0].request("FILS_HLP_REQ_ADD ff:ff:ff:ff:ff:ff " + t):
            raise Exception("FILS_HLP_REQ_ADD failed: " + t)
    id = dev[0].connect("fils", key_mgmt="FILS-SHA256",
                        eap="PSK", identity="psk.user@example.com",
                        password_hex="0123456789abcdef0123456789abcdef",
                        erp="1", scan_freq="2412")

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    dev[0].dump_monitor()
    dev[0].select_network(id, freq=2412)
    dev[0].wait_connected()
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    dev[0].request("FILS_HLP_REQ_FLUSH")
    tests = [binascii.hexlify(dhcp_opts).decode(),
             binascii.hexlify(dhcp_opts2).decode()]
    for t in tests:
        if "OK" not in dev[0].request("FILS_HLP_REQ_ADD ff:ff:ff:ff:ff:ff " + t):
            raise Exception("FILS_HLP_REQ_ADD failed: " + t)

    dev[0].dump_monitor()
    dev[0].select_network(id, freq=2412)
    dev[0].wait_connected()
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    dev[0].request("FILS_HLP_REQ_FLUSH")
    if "OK" not in dev[0].request("FILS_HLP_REQ_ADD ff:ff:ff:ff:ff:ff " + binascii.hexlify(dhcp_valid).decode()):
        raise Exception("FILS_HLP_REQ_ADD failed")
    hapd.set("own_ip_addr", "0.0.0.0")
    dev[0].select_network(id, freq=2412)
    dev[0].wait_connected()
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    hapd.set("dhcp_server", "0.0.0.0")
    dev[0].select_network(id, freq=2412)
    dev[0].wait_connected()
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    # FILS: Failed to bind DHCP socket: Address already in use
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.settimeout(5)
    sock.bind(("127.0.0.2", 67))
    hapd.set("own_ip_addr", "127.0.0.2")
    hapd.set("dhcp_server", "127.0.0.2")
    dev[0].select_network(id, freq=2412)
    dev[0].wait_connected()
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    # FILS: DHCP sendto failed: Invalid argument
    hapd.set("own_ip_addr", "127.0.0.3")
    hapd.set("dhcp_server", "127.0.0.2")
    hapd.set("dhcp_relay_port", "0")
    hapd.set("dhcp_server_port", "0")
    dev[0].select_network(id, freq=2412)
    dev[0].wait_connected()
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    dev[0].request("FILS_HLP_REQ_FLUSH")

def test_fils_sk_hlp_dhcp_parsing(dev, apdev, params):
    """FILS SK HLP and DHCP response parsing"""
    check_fils_capa(dev[0])
    check_erp_capa(dev[0])

    start_erp_as(msk_dump=os.path.join(params['logdir'], "msk.lst"))

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.settimeout(5)
    sock.bind(("127.0.0.2", 67))

    bssid = apdev[0]['bssid']
    params = fils_hlp_config(fils_hlp_wait_time=30)
    params['dhcp_rapid_commit_proxy'] = '1'
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    dev[0].scan_for_bss(bssid, freq=2412)
    dev[0].request("ERP_FLUSH")
    if "OK" not in dev[0].request("FILS_HLP_REQ_FLUSH"):
        raise Exception("Failed to flush pending FILS HLP requests")
    dhcpdisc = build_dhcp(req=True, dhcp_msg=DHCPDISCOVER,
                          chaddr=dev[0].own_addr())
    if "OK" not in dev[0].request("FILS_HLP_REQ_ADD " + "ff:ff:ff:ff:ff:ff " + binascii.hexlify(dhcpdisc).decode()):
        raise Exception("FILS_HLP_REQ_ADD failed")
    id = dev[0].connect("fils", key_mgmt="FILS-SHA256",
                        eap="PSK", identity="psk.user@example.com",
                        password_hex="0123456789abcdef0123456789abcdef",
                        erp="1", scan_freq="2412")

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    dev[0].dump_monitor()
    with alloc_fail(hapd, 1, "fils_process_hlp"):
        dev[0].select_network(id, freq=2412)
        dev[0].wait_connected()
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    dev[0].dump_monitor()
    dev[0].select_network(id, freq=2412)
    (msg, addr) = sock.recvfrom(1000)
    logger.debug("Received DHCP message from %s" % str(addr))
    dhcpdisc = build_dhcp(req=False, dhcp_msg=DHCPACK,
                          chaddr=dev[0].own_addr(), giaddr="127.0.0.3")
    #sock.sendto(dhcpdisc[2+20+8:], addr)
    chaddr = binascii.unhexlify(dev[0].own_addr().replace(':', '')) + 10*b'\x00'
    tests = [b"\x00",
             b"\x02" + 500 * b"\x00",
             b"\x02\x00\x00\x00" + 20*b"\x00" + b"\x7f\x00\x00\x03" + 500*b"\x00",
             b"\x02\x00\x00\x00" + 20*b"\x00" + b"\x7f\x00\x00\x03" + 16*b"\x00" + 64*b"\x00" + 128*b"\x00" + b"\x63\x82\x53\x63",
             b"\x02\x00\x00\x00" + 20*b"\x00" + b"\x7f\x00\x00\x03" + 16*b"\x00" + 64*b"\x00" + 128*b"\x00" + b"\x63\x82\x53\x63" + b"\x00\x11",
             b"\x02\x00\x00\x00" + 20*b"\x00" + b"\x7f\x00\x00\x03" + 16*b"\x00" + 64*b"\x00" + 128*b"\x00" + b"\x63\x82\x53\x63" + b"\x11\x01",
             b"\x02\x00\x00\x00" + 20*b"\x00" + b"\x7f\x00\x00\x03" + chaddr + 64*b"\x00" + 128*b"\x00" + b"\x63\x82\x53\x63" + b"\x35\x00\xff",
             b"\x02\x00\x00\x00" + 20*b"\x00" + b"\x7f\x00\x00\x03" + chaddr + 64*b"\x00" + 128*b"\x00" + b"\x63\x82\x53\x63" + b"\x35\x01\x00\xff",
             1501 * b"\x00"]
    for t in tests:
        sock.sendto(t, addr)
    dev[0].wait_connected()
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    # FILS: DHCP sendto failed: Invalid argument for second DHCP TX in proxy
    dev[0].dump_monitor()
    dev[0].select_network(id, freq=2412)
    (msg, addr) = sock.recvfrom(1000)
    logger.debug("Received DHCP message from %s" % str(addr))
    hapd.set("dhcp_server_port", "0")
    dhcpoffer = build_dhcp(req=False, dhcp_msg=DHCPOFFER, rapid_commit=False,
                           chaddr=dev[0].own_addr(), giaddr="127.0.0.3")
    sock.sendto(dhcpoffer[2+20+8:], addr)
    dev[0].wait_connected()
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()
    hapd.set("dhcp_server_port", "67")

    # Options in DHCPOFFER
    dev[0].dump_monitor()
    dev[0].select_network(id, freq=2412)
    (msg, addr) = sock.recvfrom(1000)
    logger.debug("Received DHCP message from %s" % str(addr))
    dhcpoffer = build_dhcp(req=False, dhcp_msg=DHCPOFFER, rapid_commit=False,
                           chaddr=dev[0].own_addr(), giaddr="127.0.0.3",
                           extra_op=b"\x00\x11", opt_end=False)
    sock.sendto(dhcpoffer[2+20+8:], addr)
    (msg, addr) = sock.recvfrom(1000)
    logger.debug("Received DHCP message from %s" % str(addr))
    dev[0].wait_connected()
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    # Options in DHCPOFFER (2)
    dev[0].dump_monitor()
    dev[0].select_network(id, freq=2412)
    (msg, addr) = sock.recvfrom(1000)
    logger.debug("Received DHCP message from %s" % str(addr))
    dhcpoffer = build_dhcp(req=False, dhcp_msg=DHCPOFFER, rapid_commit=False,
                           chaddr=dev[0].own_addr(), giaddr="127.0.0.3",
                           extra_op=b"\x11\x01", opt_end=False)
    sock.sendto(dhcpoffer[2+20+8:], addr)
    (msg, addr) = sock.recvfrom(1000)
    logger.debug("Received DHCP message from %s" % str(addr))
    dev[0].wait_connected()
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    # Server ID in DHCPOFFER
    dev[0].dump_monitor()
    dev[0].select_network(id, freq=2412)
    (msg, addr) = sock.recvfrom(1000)
    logger.debug("Received DHCP message from %s" % str(addr))
    dhcpoffer = build_dhcp(req=False, dhcp_msg=DHCPOFFER, rapid_commit=False,
                           chaddr=dev[0].own_addr(), giaddr="127.0.0.3",
                           extra_op=b"\x36\x01\x30")
    sock.sendto(dhcpoffer[2+20+8:], addr)
    (msg, addr) = sock.recvfrom(1000)
    logger.debug("Received DHCP message from %s" % str(addr))
    dev[0].wait_connected()
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    # FILS: Could not update DHCPDISCOVER
    dev[0].request("FILS_HLP_REQ_FLUSH")
    dhcpdisc = build_dhcp(req=True, dhcp_msg=DHCPDISCOVER,
                          chaddr=dev[0].own_addr(),
                          extra_op=b"\x00\x11", opt_end=False)
    if "OK" not in dev[0].request("FILS_HLP_REQ_ADD " + "ff:ff:ff:ff:ff:ff " + binascii.hexlify(dhcpdisc).decode()):
        raise Exception("FILS_HLP_REQ_ADD failed")
    dev[0].dump_monitor()
    dev[0].select_network(id, freq=2412)
    (msg, addr) = sock.recvfrom(1000)
    logger.debug("Received DHCP message from %s" % str(addr))
    dhcpoffer = build_dhcp(req=False, dhcp_msg=DHCPOFFER, rapid_commit=False,
                           chaddr=dev[0].own_addr(), giaddr="127.0.0.3",
                           extra_op=b"\x36\x01\x30")
    sock.sendto(dhcpoffer[2+20+8:], addr)
    dev[0].wait_connected()
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    # FILS: Could not update DHCPDISCOVER (2)
    dev[0].request("FILS_HLP_REQ_FLUSH")
    dhcpdisc = build_dhcp(req=True, dhcp_msg=DHCPDISCOVER,
                          chaddr=dev[0].own_addr(),
                          extra_op=b"\x11\x01", opt_end=False)
    if "OK" not in dev[0].request("FILS_HLP_REQ_ADD " + "ff:ff:ff:ff:ff:ff " + binascii.hexlify(dhcpdisc).decode()):
        raise Exception("FILS_HLP_REQ_ADD failed")
    dev[0].dump_monitor()
    dev[0].select_network(id, freq=2412)
    (msg, addr) = sock.recvfrom(1000)
    logger.debug("Received DHCP message from %s" % str(addr))
    dhcpoffer = build_dhcp(req=False, dhcp_msg=DHCPOFFER, rapid_commit=False,
                           chaddr=dev[0].own_addr(), giaddr="127.0.0.3",
                           extra_op=b"\x36\x01\x30")
    sock.sendto(dhcpoffer[2+20+8:], addr)
    dev[0].wait_connected()
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    dev[0].request("FILS_HLP_REQ_FLUSH")

def test_fils_sk_erp_and_reauth(dev, apdev, params):
    """FILS SK using ERP and AP going away"""
    check_fils_capa(dev[0])
    check_erp_capa(dev[0])

    start_erp_as(msk_dump=os.path.join(params['logdir'], "msk.lst"))

    bssid = apdev[0]['bssid']
    params = hostapd.wpa2_eap_params(ssid="fils")
    params['wpa_key_mgmt'] = "FILS-SHA256"
    params['auth_server_port'] = "18128"
    params['erp_domain'] = 'example.com'
    params['fils_realm'] = 'example.com'
    params['disable_pmksa_caching'] = '1'
    params['broadcast_deauth'] = '0'
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    dev[0].scan_for_bss(bssid, freq=2412)
    dev[0].request("ERP_FLUSH")
    id = dev[0].connect("fils", key_mgmt="FILS-SHA256",
                        eap="PSK", identity="psk.user@example.com",
                        password_hex="0123456789abcdef0123456789abcdef",
                        erp="1", scan_freq="2412")

    hapd.disable()
    dev[0].wait_disconnected()
    dev[0].dump_monitor()
    hapd.enable()

    ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED",
                            "EVENT-ASSOC-REJECT",
                            "CTRL-EVENT-CONNECTED"], timeout=10)
    if ev is None:
        raise Exception("Reconnection using FILS/ERP timed out")
    if "CTRL-EVENT-EAP-STARTED" in ev:
        raise Exception("Unexpected EAP exchange")
    if "EVENT-ASSOC-REJECT" in ev:
        raise Exception("Association failed")

def test_fils_sk_erp_sim(dev, apdev, params):
    """FILS SK using ERP with SIM"""
    check_fils_capa(dev[0])
    check_erp_capa(dev[0])

    realm = 'wlan.mnc001.mcc232.3gppnetwork.org'
    start_erp_as(erp_domain=realm,
                 msk_dump=os.path.join(params['logdir'], "msk.lst"))

    bssid = apdev[0]['bssid']
    params = hostapd.wpa2_eap_params(ssid="fils")
    params['wpa_key_mgmt'] = "FILS-SHA256"
    params['auth_server_port'] = "18128"
    params['fils_realm'] = realm
    params['disable_pmksa_caching'] = '1'
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    dev[0].scan_for_bss(bssid, freq=2412)
    dev[0].request("ERP_FLUSH")
    id = dev[0].connect("fils", key_mgmt="FILS-SHA256",
                        eap="SIM", identity="1232010000000000@" + realm,
                        password="90dca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb82581",
                        erp="1", scan_freq="2412")

    hapd.disable()
    dev[0].wait_disconnected()
    dev[0].dump_monitor()
    hapd.enable()

    ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED",
                            "EVENT-ASSOC-REJECT",
                            "CTRL-EVENT-CONNECTED"], timeout=10)
    if ev is None:
        raise Exception("Reconnection using FILS/ERP timed out")
    if "CTRL-EVENT-EAP-STARTED" in ev:
        raise Exception("Unexpected EAP exchange")
    if "EVENT-ASSOC-REJECT" in ev:
        raise Exception("Association failed")

def test_fils_sk_pfs_19(dev, apdev, params):
    """FILS SK with PFS (DH group 19)"""
    run_fils_sk_pfs(dev, apdev, "19", params)

def test_fils_sk_pfs_20(dev, apdev, params):
    """FILS SK with PFS (DH group 20)"""
    run_fils_sk_pfs(dev, apdev, "20", params)

def test_fils_sk_pfs_21(dev, apdev, params):
    """FILS SK with PFS (DH group 21)"""
    run_fils_sk_pfs(dev, apdev, "21", params)

def test_fils_sk_pfs_25(dev, apdev, params):
    """FILS SK with PFS (DH group 25)"""
    run_fils_sk_pfs(dev, apdev, "25", params)

def test_fils_sk_pfs_26(dev, apdev, params):
    """FILS SK with PFS (DH group 26)"""
    run_fils_sk_pfs(dev, apdev, "26", params)

def test_fils_sk_pfs_27(dev, apdev, params):
    """FILS SK with PFS (DH group 27)"""
    run_fils_sk_pfs(dev, apdev, "27", params)

def test_fils_sk_pfs_28(dev, apdev, params):
    """FILS SK with PFS (DH group 28)"""
    run_fils_sk_pfs(dev, apdev, "28", params)

def test_fils_sk_pfs_29(dev, apdev, params):
    """FILS SK with PFS (DH group 29)"""
    run_fils_sk_pfs(dev, apdev, "29", params)

def test_fils_sk_pfs_30(dev, apdev, params):
    """FILS SK with PFS (DH group 30)"""
    run_fils_sk_pfs(dev, apdev, "30", params)

def run_fils_sk_pfs(dev, apdev, group, params):
    check_fils_sk_pfs_capa(dev[0])
    check_erp_capa(dev[0])

    tls = dev[0].request("GET tls_library")
    if int(group) in [25]:
        if not (tls.startswith("OpenSSL") and ("build=OpenSSL 1.0.2" in tls or "build=OpenSSL 1.1" in tls) and ("run=OpenSSL 1.0.2" in tls or "run=OpenSSL 1.1" in tls)):
            raise HwsimSkip("EC group not supported")
    if int(group) in [27, 28, 29, 30]:
        if not (tls.startswith("OpenSSL") and ("build=OpenSSL 1.0.2" in tls or "build=OpenSSL 1.1" in tls) and ("run=OpenSSL 1.0.2" in tls or "run=OpenSSL 1.1" in tls)):
            raise HwsimSkip("Brainpool EC group not supported")

    start_erp_as(msk_dump=os.path.join(params['logdir'], "msk.lst"))

    bssid = apdev[0]['bssid']
    params = hostapd.wpa2_eap_params(ssid="fils")
    params['wpa_key_mgmt'] = "FILS-SHA256"
    params['auth_server_port'] = "18128"
    params['erp_domain'] = 'example.com'
    params['fils_realm'] = 'example.com'
    params['disable_pmksa_caching'] = '1'
    params['fils_dh_group'] = group
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    dev[0].scan_for_bss(bssid, freq=2412)
    dev[0].request("ERP_FLUSH")
    id = dev[0].connect("fils", key_mgmt="FILS-SHA256",
                        eap="PSK", identity="psk.user@example.com",
                        password_hex="0123456789abcdef0123456789abcdef",
                        erp="1", fils_dh_group=group, scan_freq="2412")

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    dev[0].dump_monitor()
    dev[0].select_network(id, freq=2412)
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED",
                            "EVENT-ASSOC-REJECT",
                            "CTRL-EVENT-CONNECTED"], timeout=10)
    if ev is None:
        raise Exception("Connection using FILS/ERP timed out")
    if "CTRL-EVENT-EAP-STARTED" in ev:
        raise Exception("Unexpected EAP exchange")
    if "EVENT-ASSOC-REJECT" in ev:
        raise Exception("Association failed")
    hwsim_utils.test_connectivity(dev[0], hapd)

def test_fils_sk_pfs_group_mismatch(dev, apdev, params):
    """FILS SK PFS DH group mismatch"""
    check_fils_sk_pfs_capa(dev[0])
    check_erp_capa(dev[0])

    start_erp_as(msk_dump=os.path.join(params['logdir'], "msk.lst"))

    bssid = apdev[0]['bssid']
    params = hostapd.wpa2_eap_params(ssid="fils")
    params['wpa_key_mgmt'] = "FILS-SHA256"
    params['auth_server_port'] = "18128"
    params['erp_domain'] = 'example.com'
    params['fils_realm'] = 'example.com'
    params['disable_pmksa_caching'] = '1'
    params['fils_dh_group'] = "20"
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    dev[0].scan_for_bss(bssid, freq=2412)
    dev[0].request("ERP_FLUSH")
    id = dev[0].connect("fils", key_mgmt="FILS-SHA256",
                        eap="PSK", identity="psk.user@example.com",
                        password_hex="0123456789abcdef0123456789abcdef",
                        erp="1", fils_dh_group="19", scan_freq="2412")

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    dev[0].dump_monitor()
    dev[0].select_network(id, freq=2412)
    ev = dev[0].wait_event(["CTRL-EVENT-AUTH-REJECT"], timeout=10)
    dev[0].request("DISCONNECT")
    if ev is None:
        raise Exception("Authentication rejection not seen")
    if "auth_type=5 auth_transaction=2 status_code=77" not in ev:
        raise Exception("Unexpected auth reject value: " + ev)

def test_fils_sk_pfs_pmksa_caching(dev, apdev, params):
    """FILS SK with PFS and PMKSA caching"""
    check_fils_sk_pfs_capa(dev[0])
    check_erp_capa(dev[0])

    start_erp_as(msk_dump=os.path.join(params['logdir'], "msk.lst"))

    bssid = apdev[0]['bssid']
    params = hostapd.wpa2_eap_params(ssid="fils")
    params['wpa_key_mgmt'] = "FILS-SHA256"
    params['auth_server_port'] = "18128"
    params['erp_domain'] = 'example.com'
    params['fils_realm'] = 'example.com'
    params['fils_dh_group'] = "19"
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    dev[0].scan_for_bss(bssid, freq=2412)
    dev[0].request("ERP_FLUSH")
    id = dev[0].connect("fils", key_mgmt="FILS-SHA256",
                        eap="PSK", identity="psk.user@example.com",
                        password_hex="0123456789abcdef0123456789abcdef",
                        erp="1", fils_dh_group="19", scan_freq="2412")
    pmksa = dev[0].get_pmksa(bssid)
    if pmksa is None:
        raise Exception("No PMKSA cache entry created")

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    # FILS authentication with PMKSA caching and PFS
    dev[0].dump_monitor()
    dev[0].select_network(id, freq=2412)
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED",
                            "CTRL-EVENT-CONNECTED"], timeout=10)
    if ev is None:
        raise Exception("Connection using PMKSA caching timed out")
    if "CTRL-EVENT-EAP-STARTED" in ev:
        raise Exception("Unexpected EAP exchange")
    hwsim_utils.test_connectivity(dev[0], hapd)
    pmksa2 = dev[0].get_pmksa(bssid)
    if pmksa2 is None:
        raise Exception("No PMKSA cache entry found")
    if pmksa['pmkid'] != pmksa2['pmkid']:
        raise Exception("Unexpected PMKID change")

    # Verify EAPOL reauthentication after FILS authentication
    hapd.request("EAPOL_REAUTH " + dev[0].own_addr())
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED"], timeout=5)
    if ev is None:
        raise Exception("EAP authentication did not start")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-SUCCESS"], timeout=5)
    if ev is None:
        raise Exception("EAP authentication did not succeed")
    time.sleep(0.1)
    hwsim_utils.test_connectivity(dev[0], hapd)

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    # FILS authentication with ERP and PFS
    dev[0].request("PMKSA_FLUSH")
    dev[0].dump_monitor()
    dev[0].select_network(id, freq=2412)
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED",
                            "CTRL-EVENT-EAP-SUCCESS",
                            "CTRL-EVENT-CONNECTED"], timeout=10)
    if ev is None:
        raise Exception("Connection using ERP and PFS timed out")
    if "CTRL-EVENT-EAP-STARTED" in ev:
        raise Exception("Unexpected EAP exchange")
    if "CTRL-EVENT-EAP-SUCCESS" not in ev:
        raise Exception("ERP success not reported")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED",
                            "SME: Trying to authenticate",
                            "CTRL-EVENT-CONNECTED"], timeout=10)
    if ev is None:
        raise Exception("Connection using ERP and PFS timed out")
    if "CTRL-EVENT-EAP-STARTED" in ev:
        raise Exception("Unexpected EAP exchange")
    if "SME: Trying to authenticate" in ev:
        raise Exception("Unexpected extra authentication round with ERP and PFS")
    hwsim_utils.test_connectivity(dev[0], hapd)
    pmksa3 = dev[0].get_pmksa(bssid)
    if pmksa3 is None:
        raise Exception("No PMKSA cache entry found")
    if pmksa2['pmkid'] == pmksa3['pmkid']:
        raise Exception("PMKID did not change")

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    # FILS authentication with PMKSA caching and PFS
    dev[0].dump_monitor()
    dev[0].select_network(id, freq=2412)
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED",
                            "CTRL-EVENT-CONNECTED"], timeout=10)
    if ev is None:
        raise Exception("Connection using PMKSA caching timed out")
    if "CTRL-EVENT-EAP-STARTED" in ev:
        raise Exception("Unexpected EAP exchange")
    hwsim_utils.test_connectivity(dev[0], hapd)
    pmksa4 = dev[0].get_pmksa(bssid)
    if pmksa4 is None:
        raise Exception("No PMKSA cache entry found")
    if pmksa3['pmkid'] != pmksa4['pmkid']:
        raise Exception("Unexpected PMKID change (2)")

def test_fils_sk_auth_mismatch(dev, apdev, params):
    """FILS SK authentication type mismatch (PFS not supported)"""
    check_fils_sk_pfs_capa(dev[0])
    check_erp_capa(dev[0])

    start_erp_as(msk_dump=os.path.join(params['logdir'], "msk.lst"))

    bssid = apdev[0]['bssid']
    params = hostapd.wpa2_eap_params(ssid="fils")
    params['wpa_key_mgmt'] = "FILS-SHA256"
    params['auth_server_port'] = "18128"
    params['erp_domain'] = 'example.com'
    params['fils_realm'] = 'example.com'
    params['disable_pmksa_caching'] = '1'
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    dev[0].scan_for_bss(bssid, freq=2412)
    dev[0].request("ERP_FLUSH")
    id = dev[0].connect("fils", key_mgmt="FILS-SHA256",
                        eap="PSK", identity="psk.user@example.com",
                        password_hex="0123456789abcdef0123456789abcdef",
                        erp="1", fils_dh_group="19", scan_freq="2412")

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    hapd.dump_monitor()
    dev[0].dump_monitor()
    dev[0].select_network(id, freq=2412)
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED",
                            "EVENT-ASSOC-REJECT",
                            "CTRL-EVENT-CONNECTED"], timeout=10)
    if ev is None:
        raise Exception("Connection using FILS/ERP timed out")
    if "CTRL-EVENT-EAP-STARTED" not in ev:
        raise Exception("No EAP exchange seen")
    dev[0].wait_connected()
    hapd.wait_sta()
    hwsim_utils.test_connectivity(dev[0], hapd)

def setup_fils_rekey(dev, apdev, params, wpa_ptk_rekey=0, wpa_group_rekey=0,
                     pmksa_caching=True, ext_key_id=False):
    check_fils_capa(dev[0])
    check_erp_capa(dev[0])

    start_erp_as(msk_dump=os.path.join(params['logdir'], "msk.lst"))

    bssid = apdev[0]['bssid']
    params = hostapd.wpa2_eap_params(ssid="fils")
    params['wpa_key_mgmt'] = "FILS-SHA256"
    params['auth_server_port'] = "18128"
    params['erp_domain'] = 'example.com'
    params['fils_realm'] = 'example.com'
    if wpa_ptk_rekey:
        params['wpa_ptk_rekey'] = str(wpa_ptk_rekey)
    if wpa_group_rekey:
        params['wpa_group_rekey'] = str(wpa_group_rekey)
    if not pmksa_caching:
            params['disable_pmksa_caching'] = '1'
    if ext_key_id:
        params['extended_key_id'] = '1'
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    dev[0].scan_for_bss(bssid, freq=2412)
    dev[0].request("ERP_FLUSH")
    id = dev[0].connect("fils", key_mgmt="FILS-SHA256",
                        eap="PSK", identity="psk.user@example.com",
                        password_hex="0123456789abcdef0123456789abcdef",
                        erp="1", scan_freq="2412")

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    dev[0].dump_monitor()
    dev[0].select_network(id, freq=2412)
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED",
                            "CTRL-EVENT-CONNECTED"], timeout=10)
    if ev is None:
        raise Exception("Connection using ERP or PMKSA caching timed out")
    if "CTRL-EVENT-EAP-STARTED" in ev:
        raise Exception("Unexpected EAP exchange")
    dev[0].dump_monitor()

    hwsim_utils.test_connectivity(dev[0], hapd)
    return hapd

def test_fils_auth_gtk_rekey(dev, apdev, params):
    """GTK rekeying after FILS authentication"""
    hapd = setup_fils_rekey(dev, apdev, params, wpa_group_rekey=1)
    ev = dev[0].wait_event(["WPA: Group rekeying completed"], timeout=2)
    if ev is None:
        raise Exception("GTK rekey timed out")
    hwsim_utils.test_connectivity(dev[0], hapd)

    ev = dev[0].wait_event(["CTRL-EVENT-DISCONNECTED"], timeout=5)
    if ev is not None:
        raise Exception("Rekeying failed - disconnected")
    hwsim_utils.test_connectivity(dev[0], hapd)

def test_fils_auth_ptk_rekey_ap(dev, apdev, params):
    """PTK rekeying after FILS authentication triggered by AP"""
    hapd = setup_fils_rekey(dev, apdev, params, wpa_ptk_rekey=2)
    ev = dev[0].wait_event(["WPA: Key negotiation completed"], timeout=3)
    if ev is None:
        raise Exception("PTK rekey timed out")
    hwsim_utils.test_connectivity(dev[0], hapd)

    ev = dev[0].wait_event(["CTRL-EVENT-DISCONNECTED"], timeout=1)
    if ev is not None:
        raise Exception("Rekeying failed - disconnected")
    hwsim_utils.test_connectivity(dev[0], hapd)

def test_fils_auth_ptk_rekey_ap_erp(dev, apdev, params):
    """PTK rekeying after FILS authentication triggered by AP (ERP)"""
    hapd = setup_fils_rekey(dev, apdev, params, wpa_ptk_rekey=2,
                            pmksa_caching=False)
    ev = dev[0].wait_event(["WPA: Key negotiation completed"], timeout=3)
    if ev is None:
        raise Exception("PTK rekey timed out")
    hwsim_utils.test_connectivity(dev[0], hapd)

    ev = dev[0].wait_event(["CTRL-EVENT-DISCONNECTED"], timeout=1)
    if ev is not None:
        raise Exception("Rekeying failed - disconnected")
    hwsim_utils.test_connectivity(dev[0], hapd)

def test_fils_and_ft(dev, apdev, params):
    """FILS SK using ERP and FT initial mobility domain association"""
    check_fils_capa(dev[0])
    check_erp_capa(dev[0])

    er = start_erp_as(msk_dump=os.path.join(params['logdir'], "msk.lst"))

    bssid = apdev[0]['bssid']
    params = hostapd.wpa2_eap_params(ssid="fils")
    params['wpa_key_mgmt'] = "FILS-SHA256"
    params['auth_server_port'] = "18128"
    params['erp_domain'] = 'example.com'
    params['fils_realm'] = 'example.com'
    params['disable_pmksa_caching'] = '1'
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    dev[0].scan_for_bss(bssid, freq=2412)
    dev[0].request("ERP_FLUSH")
    id = dev[0].connect("fils", key_mgmt="FILS-SHA256",
                        eap="PSK", identity="psk.user@example.com",
                        password_hex="0123456789abcdef0123456789abcdef",
                        erp="1", scan_freq="2412")

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()
    hapd.disable()
    dev[0].flush_scan_cache()
    if "FAIL" in dev[0].request("PMKSA_FLUSH"):
        raise Exception("PMKSA_FLUSH failed")

    params = hostapd.wpa2_eap_params(ssid="fils-ft")
    params['wpa_key_mgmt'] = "FILS-SHA256 FT-FILS-SHA256 FT-EAP"
    params['auth_server_port'] = "18128"
    params['erp_domain'] = 'example.com'
    params['fils_realm'] = 'example.com'
    params['disable_pmksa_caching'] = '1'
    params["mobility_domain"] = "a1b2"
    params["r0_key_lifetime"] = "10000"
    params["pmk_r1_push"] = "1"
    params["reassociation_deadline"] = "1000"
    params['nas_identifier'] = "nas1.w1.fi"
    params['r1_key_holder'] = "000102030405"
    params['r0kh'] = ["02:00:00:00:04:00 nas2.w1.fi 300102030405060708090a0b0c0d0e0f"]
    params['r1kh'] = "02:00:00:00:04:00 00:01:02:03:04:06 200102030405060708090a0b0c0d0e0f"
    params['ieee80211w'] = "1"
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    dev[0].scan_for_bss(bssid, freq=2412)
    dev[0].dump_monitor()
    id = dev[0].connect("fils-ft", key_mgmt="FILS-SHA256 FT-FILS-SHA256 FT-EAP",
                        ieee80211w="1",
                        eap="PSK", identity="psk.user@example.com",
                        password_hex="0123456789abcdef0123456789abcdef",
                        erp="1", scan_freq="2412", wait_connect=False)

    ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED",
                            "CTRL-EVENT-AUTH-REJECT",
                            "EVENT-ASSOC-REJECT",
                            "CTRL-EVENT-CONNECTED"], timeout=10)
    if ev is None:
        raise Exception("Connection using FILS/ERP timed out")
    if "CTRL-EVENT-EAP-STARTED" in ev:
        raise Exception("Unexpected EAP exchange")
    if "CTRL-EVENT-AUTH-REJECT" in ev:
        raise Exception("Authentication failed")
    if "EVENT-ASSOC-REJECT" in ev:
        raise Exception("Association failed")
    hwsim_utils.test_connectivity(dev[0], hapd)

    er.disable()

    # FIX: FT-FILS-SHA256 does not currently work for FT protocol due to not
    # fully defined FT Reassociation Request/Response frame MIC use in FTE.
    # FT-EAP can be used to work around that in this test case to confirm the
    # FT key hierarchy was properly formed in the previous step.
    #params['wpa_key_mgmt'] = "FILS-SHA256 FT-FILS-SHA256"
    params['wpa_key_mgmt'] = "FT-EAP"
    params['nas_identifier'] = "nas2.w1.fi"
    params['r1_key_holder'] = "000102030406"
    params['r0kh'] = ["02:00:00:00:03:00 nas1.w1.fi 200102030405060708090a0b0c0d0e0f"]
    params['r1kh'] = "02:00:00:00:03:00 00:01:02:03:04:05 300102030405060708090a0b0c0d0e0f"
    hapd2 = hostapd.add_ap(apdev[1]['ifname'], params)

    dev[0].scan_for_bss(apdev[1]['bssid'], freq="2412", force_scan=True)
    # FIX: Cannot use FT-over-DS without the FTE MIC issue addressed
    #dev[0].roam_over_ds(apdev[1]['bssid'])
    dev[0].roam(apdev[1]['bssid'])

def test_fils_and_ft_over_air(dev, apdev, params):
    """FILS SK using ERP and FT-over-air (SHA256)"""
    run_fils_and_ft_over_air(dev, apdev, params, "FT-FILS-SHA256")

def test_fils_and_ft_over_air_sha384(dev, apdev, params):
    """FILS SK using ERP and FT-over-air (SHA384)"""
    run_fils_and_ft_over_air(dev, apdev, params, "FT-FILS-SHA384")

def run_fils_and_ft_over_air(dev, apdev, params, key_mgmt):
    hapd, hapd2 = run_fils_and_ft_setup(dev, apdev, params, key_mgmt)
    conf = hapd.request("GET_CONFIG")
    if "key_mgmt=" + key_mgmt not in conf.splitlines():
        logger.info("GET_CONFIG:\n" + conf)
        raise Exception("GET_CONFIG did not report correct key_mgmt")

    logger.info("FT protocol using FT key hierarchy established during FILS authentication")
    dev[0].scan_for_bss(apdev[1]['bssid'], freq="2412", force_scan=True)
    hapd.request("NOTE FT protocol to AP2 using FT keys established during FILS FILS authentication")
    dev[0].roam(apdev[1]['bssid'])
    hwsim_utils.test_connectivity(dev[0], hapd2)

    logger.info("FT protocol using the previously established FT key hierarchy from FILS authentication")
    hapd.request("NOTE FT protocol back to AP1 using FT keys established during FILS FILS authentication")
    dev[0].roam(apdev[0]['bssid'])
    hwsim_utils.test_connectivity(dev[0], hapd)

    hapd.request("NOTE FT protocol back to AP2 using FT keys established during FILS FILS authentication")
    dev[0].roam(apdev[1]['bssid'])
    hwsim_utils.test_connectivity(dev[0], hapd2)

    hapd.request("NOTE FT protocol back to AP1 using FT keys established during FILS FILS authentication (2)")
    dev[0].roam(apdev[0]['bssid'])
    hwsim_utils.test_connectivity(dev[0], hapd)

def test_fils_and_ft_over_ds(dev, apdev, params):
    """FILS SK using ERP and FT-over-DS (SHA256)"""
    run_fils_and_ft_over_ds(dev, apdev, params, "FT-FILS-SHA256")

def test_fils_and_ft_over_ds_sha384(dev, apdev, params):
    """FILS SK using ERP and FT-over-DS (SHA384)"""
    run_fils_and_ft_over_ds(dev, apdev, params, "FT-FILS-SHA384")

def run_fils_and_ft_over_ds(dev, apdev, params, key_mgmt):
    hapd, hapd2 = run_fils_and_ft_setup(dev, apdev, params, key_mgmt)

    logger.info("FT protocol using FT key hierarchy established during FILS authentication")
    dev[0].scan_for_bss(apdev[1]['bssid'], freq="2412", force_scan=True)
    hapd.request("NOTE FT protocol to AP2 using FT keys established during FILS FILS authentication")
    dev[0].roam_over_ds(apdev[1]['bssid'])

    logger.info("FT protocol using the previously established FT key hierarchy from FILS authentication")
    hapd.request("NOTE FT protocol back to AP1 using FT keys established during FILS FILS authentication")
    dev[0].roam_over_ds(apdev[0]['bssid'])

    hapd.request("NOTE FT protocol back to AP2 using FT keys established during FILS FILS authentication")
    dev[0].roam_over_ds(apdev[1]['bssid'])

    hapd.request("NOTE FT protocol back to AP1 using FT keys established during FILS FILS authentication (2)")
    dev[0].roam_over_ds(apdev[0]['bssid'])

def run_fils_and_ft_setup(dev, apdev, params, key_mgmt):
    check_fils_capa(dev[0])
    check_erp_capa(dev[0])

    er = start_erp_as(msk_dump=os.path.join(params['logdir'], "msk.lst"))

    logger.info("Set up ERP key hierarchy without FILS/FT authentication")
    bssid = apdev[0]['bssid']
    params = hostapd.wpa2_eap_params(ssid="fils")
    params['wpa_key_mgmt'] = key_mgmt
    params['auth_server_port'] = "18128"
    params['erp_domain'] = 'example.com'
    params['fils_realm'] = 'example.com'
    params['disable_pmksa_caching'] = '1'
    params['ieee80211w'] = "2"
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    dev[0].scan_for_bss(bssid, freq=2412)
    dev[0].request("ERP_FLUSH")
    hapd.request("NOTE Initial association to establish ERP keys")
    id = dev[0].connect("fils", key_mgmt=key_mgmt, ieee80211w="2",
                        eap="PSK", identity="psk.user@example.com",
                        password_hex="0123456789abcdef0123456789abcdef",
                        erp="1", scan_freq="2412")
    hapd.wait_sta()
    hwsim_utils.test_connectivity(dev[0], hapd)

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()
    hapd.disable()
    dev[0].flush_scan_cache()
    if "FAIL" in dev[0].request("PMKSA_FLUSH"):
        raise Exception("PMKSA_FLUSH failed")

    logger.info("Initial mobility domain association using FILS authentication")
    params = hostapd.wpa2_eap_params(ssid="fils-ft")
    params['wpa_key_mgmt'] = key_mgmt
    params['auth_server_port'] = "18128"
    params['erp_domain'] = 'example.com'
    params['fils_realm'] = 'example.com'
    params['disable_pmksa_caching'] = '1'
    params["mobility_domain"] = "a1b2"
    params["r0_key_lifetime"] = "10000"
    params["pmk_r1_push"] = "1"
    params["reassociation_deadline"] = "1000"
    params['nas_identifier'] = "nas1.w1.fi"
    params['r1_key_holder'] = "000102030405"
    params['r0kh'] = ["02:00:00:00:03:00 nas1.w1.fi 100102030405060708090a0b0c0d0e0f100102030405060708090a0b0c0d0e0f",
                      "02:00:00:00:04:00 nas2.w1.fi 300102030405060708090a0b0c0d0e0f"]
    params['r1kh'] = "02:00:00:00:04:00 00:01:02:03:04:06 200102030405060708090a0b0c0d0e0f"
    params['ieee80211w'] = "2"
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    dev[0].scan_for_bss(bssid, freq=2412)
    dev[0].dump_monitor()
    hapd.request("NOTE Initial FT mobility domain association using FILS authentication")
    dev[0].set_network_quoted(id, "ssid", "fils-ft")
    dev[0].select_network(id, freq=2412)

    ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED",
                            "CTRL-EVENT-AUTH-REJECT",
                            "EVENT-ASSOC-REJECT",
                            "CTRL-EVENT-CONNECTED"], timeout=10)
    if ev is None:
        raise Exception("Connection using FILS/ERP timed out")
    if "CTRL-EVENT-EAP-STARTED" in ev:
        raise Exception("Unexpected EAP exchange")
    if "CTRL-EVENT-AUTH-REJECT" in ev:
        raise Exception("Authentication failed")
    if "EVENT-ASSOC-REJECT" in ev:
        raise Exception("Association failed")
    hapd.wait_sta()
    hwsim_utils.test_connectivity(dev[0], hapd)

    er.disable()

    params['wpa_key_mgmt'] = key_mgmt
    params['nas_identifier'] = "nas2.w1.fi"
    params['r1_key_holder'] = "000102030406"
    params['r0kh'] = ["02:00:00:00:03:00 nas1.w1.fi 200102030405060708090a0b0c0d0e0f",
                      "02:00:00:00:04:00 nas2.w1.fi 000102030405060708090a0b0c0d0e0f000102030405060708090a0b0c0d0e0f"]
    params['r1kh'] = "02:00:00:00:03:00 00:01:02:03:04:05 300102030405060708090a0b0c0d0e0f"
    hapd2 = hostapd.add_ap(apdev[1]['ifname'], params)

    return hapd, hapd2

def test_fils_assoc_replay(dev, apdev, params):
    """FILS AP and replayed Association Request frame"""
    capfile = os.path.join(params['logdir'], "hwsim0.pcapng")
    check_fils_capa(dev[0])
    check_erp_capa(dev[0])

    start_erp_as()

    bssid = apdev[0]['bssid']
    params = hostapd.wpa2_eap_params(ssid="fils")
    params['wpa_key_mgmt'] = "FILS-SHA256"
    params['auth_server_port'] = "18128"
    params['erp_domain'] = 'example.com'
    params['fils_realm'] = 'example.com'
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    dev[0].scan_for_bss(bssid, freq=2412)
    dev[0].request("ERP_FLUSH")
    id = dev[0].connect("fils", key_mgmt="FILS-SHA256",
                        eap="PSK", identity="psk.user@example.com",
                        password_hex="0123456789abcdef0123456789abcdef",
                        erp="1", scan_freq="2412")

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    hapd.set("ext_mgmt_frame_handling", "1")
    dev[0].dump_monitor()
    dev[0].select_network(id, freq=2412)

    assocreq = None
    count = 0
    while count < 100:
        req = hapd.mgmt_rx()
        count += 1
        hapd.dump_monitor()
        hapd.request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=" + binascii.hexlify(req['frame']).decode())
        if req['subtype'] == 0:
            assocreq = req
            ev = hapd.wait_event(["MGMT-TX-STATUS"], timeout=5)
            if ev is None:
                raise Exception("No TX status seen")
            cmd = "MGMT_TX_STATUS_PROCESS %s" % (" ".join(ev.split(' ')[1:4]))
            if "OK" not in hapd.request(cmd):
                raise Exception("MGMT_TX_STATUS_PROCESS failed")
            break
    hapd.set("ext_mgmt_frame_handling", "0")
    if assocreq is None:
        raise Exception("No Association Request frame seen")
    dev[0].wait_connected()
    dev[0].dump_monitor()
    hapd.dump_monitor()

    hwsim_utils.test_connectivity(dev[0], hapd)

    logger.info("Replay the last Association Request frame")
    hapd.dump_monitor()
    hapd.set("ext_mgmt_frame_handling", "1")
    hapd.request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=" + binascii.hexlify(req['frame']).decode())
    ev = hapd.wait_event(["MGMT-TX-STATUS"], timeout=5)
    if ev is None:
        raise Exception("No TX status seen")
    cmd = "MGMT_TX_STATUS_PROCESS %s" % (" ".join(ev.split(' ')[1:4]))
    if "OK" not in hapd.request(cmd):
        raise Exception("MGMT_TX_STATUS_PROCESS failed")
    hapd.set("ext_mgmt_frame_handling", "0")

    try:
        hwsim_utils.test_connectivity(dev[0], hapd)
        ok = True
    except:
        ok = False

    ap = hapd.own_addr()
    sta = dev[0].own_addr()
    filt = "wlan.fc.type == 2 && " + \
           "wlan.da == " + sta + " && " + \
           "wlan.sa == " + ap + " && wlan.ccmp.extiv"
    fields = ["wlan.ccmp.extiv"]
    res = run_tshark(capfile, filt, fields)
    vals = res.splitlines()
    logger.info("CCMP PN: " + str(vals))
    if len(vals) < 2:
        raise Exception("Could not find all CCMP protected frames from capture")
    if len(set(vals)) < len(vals):
        raise Exception("Duplicate CCMP PN used")

    if not ok:
        raise Exception("The second hwsim connectivity test failed")

def test_fils_sk_erp_server_flush(dev, apdev, params):
    """FILS SK ERP and ERP flush on server, but not on peer"""
    check_fils_capa(dev[0])
    check_erp_capa(dev[0])

    hapd_as = start_erp_as(msk_dump=os.path.join(params['logdir'], "msk.lst"))

    bssid = apdev[0]['bssid']
    params = hostapd.wpa2_eap_params(ssid="fils")
    params['wpa_key_mgmt'] = "FILS-SHA256"
    params['auth_server_port'] = "18128"
    params['erp_domain'] = 'example.com'
    params['fils_realm'] = 'example.com'
    params['disable_pmksa_caching'] = '1'
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    dev[0].scan_for_bss(bssid, freq=2412)
    dev[0].request("ERP_FLUSH")
    id = dev[0].connect("fils", key_mgmt="FILS-SHA256",
                        eap="PSK", identity="psk.user@example.com",
                        password_hex="0123456789abcdef0123456789abcdef",
                        erp="1", scan_freq="2412")

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    dev[0].dump_monitor()
    dev[0].select_network(id, freq=2412)
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED",
                            "EVENT-ASSOC-REJECT",
                            "CTRL-EVENT-CONNECTED"], timeout=10)
    if ev is None:
        raise Exception("Connection using FILS/ERP timed out")
    if "CTRL-EVENT-EAP-STARTED" in ev:
        raise Exception("Unexpected EAP exchange")
    if "EVENT-ASSOC-REJECT" in ev:
        raise Exception("Association failed")

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    hapd_as.request("ERP_FLUSH")
    dev[0].dump_monitor()
    dev[0].select_network(id, freq=2412)
    ev = dev[0].wait_event(["CTRL-EVENT-AUTH-REJECT"], timeout=10)
    if ev is None:
        raise Exception("No authentication rejection seen after ERP flush on server")

    ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED",
                            "CTRL-EVENT-AUTH-REJECT",
                            "EVENT-ASSOC-REJECT",
                            "CTRL-EVENT-CONNECTED"], timeout=10)
    if ev is None:
        raise Exception("Connection attempt using FILS/ERP timed out")
    if "CTRL-EVENT-AUTH-REJECT" in ev:
        raise Exception("Failed to recover from ERP flush on server")
    if "EVENT-ASSOC-REJECT" in ev:
        raise Exception("Association failed")
    if "CTRL-EVENT-EAP-STARTED" not in ev:
        raise Exception("New EAP exchange not seen")
    dev[0].wait_connected(error="Connection timeout after ERP flush")

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()
    dev[0].dump_monitor()
    dev[0].select_network(id, freq=2412)
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED",
                            "CTRL-EVENT-AUTH-REJECT",
                            "EVENT-ASSOC-REJECT",
                            "CTRL-EVENT-CONNECTED"], timeout=10)
    if ev is None:
        raise Exception("Connection attempt using FILS with new ERP keys timed out")
    if "CTRL-EVENT-AUTH-REJECT" in ev:
        raise Exception("Authentication failed with new ERP keys")
    if "EVENT-ASSOC-REJECT" in ev:
        raise Exception("Association failed with new ERP keys")
    if "CTRL-EVENT-EAP-STARTED" in ev:
        raise Exception("Unexpected EAP exchange")

def test_fils_sk_erp_radius_ext(dev, apdev, params):
    """FILS SK using ERP and external RADIUS server"""
    as_hapd = hostapd.Hostapd("as")
    try:
        as_hapd.disable()
        as_hapd.set("eap_server_erp", "1")
        as_hapd.set("erp_domain", "erp.example.com")
        as_hapd.enable()
        run_fils_sk_erp_radius_ext(dev, apdev, params)
    finally:
        as_hapd.disable()
        as_hapd.set("eap_server_erp", "0")
        as_hapd.set("erp_domain", "")
        as_hapd.enable()

def run_fils_sk_erp_radius_ext(dev, apdev, params):
    check_fils_capa(dev[0])
    check_erp_capa(dev[0])

    bssid = apdev[0]['bssid']
    params = hostapd.wpa2_eap_params(ssid="fils")
    params['wpa_key_mgmt'] = "FILS-SHA256"
    params['erp_domain'] = 'erp.example.com'
    params['fils_realm'] = 'erp.example.com'
    params['disable_pmksa_caching'] = '1'
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    dev[0].scan_for_bss(bssid, freq=2412)
    dev[0].request("ERP_FLUSH")
    id = dev[0].connect("fils", key_mgmt="FILS-SHA256",
                        eap="PWD", identity="pwd@erp.example.com",
                        password="secret password",
                        erp="1", scan_freq="2412")

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    dev[0].dump_monitor()
    dev[0].select_network(id, freq=2412)
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED",
                            "EVENT-ASSOC-REJECT",
                            "CTRL-EVENT-CONNECTED"], timeout=10)
    if ev is None:
        raise Exception("Connection using FILS/ERP timed out")
    if "CTRL-EVENT-EAP-STARTED" in ev:
        raise Exception("Unexpected EAP exchange")
    if "EVENT-ASSOC-REJECT" in ev:
        raise Exception("Association failed")
    hwsim_utils.test_connectivity(dev[0], hapd)

def test_fils_sk_erp_radius_roam(dev, apdev):
    """FILS SK/ERP and roaming with different AKM"""
    as_hapd = hostapd.Hostapd("as")
    try:
        as_hapd.disable()
        as_hapd.set("eap_server_erp", "1")
        as_hapd.set("erp_domain", "example.com")
        as_hapd.enable()
        run_fils_sk_erp_radius_roam(dev, apdev)
    finally:
        as_hapd.disable()
        as_hapd.set("eap_server_erp", "0")
        as_hapd.set("erp_domain", "")
        as_hapd.enable()

def run_fils_sk_erp_radius_roam(dev, apdev):
    check_fils_capa(dev[0])
    check_erp_capa(dev[0])

    bssid = apdev[0]['bssid']
    params = hostapd.wpa2_eap_params(ssid="fils")
    params['wpa_key_mgmt'] = "FILS-SHA256"
    params['erp_domain'] = 'example.com'
    params['fils_realm'] = 'example.com'
    params['disable_pmksa_caching'] = '1'
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    dev[0].scan_for_bss(bssid, freq=2412)
    dev[0].request("ERP_FLUSH")
    id = dev[0].connect("fils", key_mgmt="FILS-SHA256 FILS-SHA384",
                        eap="PWD", identity="erp-pwd@example.com",
                        password="secret password",
                        erp="1", scan_freq="2412")

    bssid2 = apdev[1]['bssid']
    params = hostapd.wpa2_eap_params(ssid="fils")
    params['wpa_key_mgmt'] = "FILS-SHA384"
    params['erp_domain'] = 'example.com'
    params['fils_realm'] = 'example.com'
    params['disable_pmksa_caching'] = '1'
    hapd2 = hostapd.add_ap(apdev[1]['ifname'], params)

    dev[0].scan_for_bss(bssid2, freq=2412)

    dev[0].dump_monitor()
    if "OK" not in dev[0].request("ROAM " + bssid2):
        raise Exception("ROAM failed")

    ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED",
                            "CTRL-EVENT-CONNECTED"], timeout=10)
    if ev is None:
        raise Exception("Connection using PMKSA caching timed out")
    if "CTRL-EVENT-EAP-STARTED" in ev:
        raise Exception("Unexpected EAP exchange")
    if bssid2 not in ev:
        raise Exception("Failed to connect to the second AP")

    hapd2.wait_sta()
    hwsim_utils.test_connectivity(dev[0], hapd2)

def test_fils_sk_erp_roam_diff_akm(dev, apdev, params):
    """FILS SK using ERP and SHA256/SHA384 change in roam"""
    check_fils_capa(dev[0])
    check_erp_capa(dev[0])

    start_erp_as()

    bssid = apdev[0]['bssid']
    params = hostapd.wpa2_eap_params(ssid="fils")
    params['wpa_key_mgmt'] = "FILS-SHA256"
    params['auth_server_port'] = "18128"
    params['erp_domain'] = 'example.com'
    params['fils_realm'] = 'example.com'
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    dev[0].scan_for_bss(bssid, freq=2412)
    dev[0].request("ERP_FLUSH")
    id = dev[0].connect("fils", key_mgmt="FILS-SHA256 FILS-SHA384",
                        eap="PSK", identity="psk.user@example.com",
                        password_hex="0123456789abcdef0123456789abcdef",
                        erp="1", scan_freq="2412")
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()
    dev[0].request("RECONNECT")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED",
                            "CTRL-EVENT-CONNECTED"], timeout=10)
    if ev is None:
        raise Exception("Connection using FILS timed out")
    if "CTRL-EVENT-EAP-STARTED" in ev:
        raise Exception("Unexpected EAP exchange")

    bssid2 = apdev[1]['bssid']
    params = hostapd.wpa2_eap_params(ssid="fils")
    params['wpa_key_mgmt'] = "FILS-SHA256 FILS-SHA384"
    params['auth_server_port'] = "18128"
    params['erp_domain'] = 'example.com'
    params['fils_realm'] = 'example.com'
    hapd2 = hostapd.add_ap(apdev[1]['ifname'], params)

    dev[0].scan_for_bss(bssid2, freq=2412)

    dev[0].dump_monitor()
    if "OK" not in dev[0].request("ROAM " + bssid2):
        raise Exception("ROAM failed")

    ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED",
                            "CTRL-EVENT-CONNECTED"], timeout=10)
    if ev is None:
        raise Exception("Roaming using FILS timed out")
    if "CTRL-EVENT-EAP-STARTED" in ev:
        raise Exception("Unexpected EAP exchange")
    if bssid2 not in ev:
        raise Exception("Failed to connect to the second AP")

    hwsim_utils.test_connectivity(dev[0], hapd2)

def test_fils_auth_ptk_rekey_ap_ext_key_id(dev, apdev, params):
    """PTK rekeying after FILS authentication triggered by AP (Ext Key ID)"""
    check_ext_key_id_capa(dev[0])
    try:
        dev[0].set("extended_key_id", "1")
        hapd = setup_fils_rekey(dev, apdev, params, wpa_ptk_rekey=2,
                                ext_key_id=True)
        check_ext_key_id_capa(hapd)
        idx = int(dev[0].request("GET last_tk_key_idx"))
        if idx != 0:
            raise Exception("Unexpected Key ID before TK rekey: %d" % idx)
        ev = dev[0].wait_event(["WPA: Key negotiation completed"], timeout=3)
        if ev is None:
            raise Exception("PTK rekey timed out")
        idx = int(dev[0].request("GET last_tk_key_idx"))
        if idx != 1:
            raise Exception("Unexpected Key ID after TK rekey: %d" % idx)
        hwsim_utils.test_connectivity(dev[0], hapd)

        ev = dev[0].wait_event(["CTRL-EVENT-DISCONNECTED"], timeout=1)
        if ev is not None:
            raise Exception("Rekeying failed - disconnected")
        hwsim_utils.test_connectivity(dev[0], hapd)
    finally:
        dev[0].set("extended_key_id", "0")

def test_fils_discovery_frame(dev, apdev, params):
    """FILS Discovery frame generation"""
    check_fils_capa(dev[0])
    check_erp_capa(dev[0])

    start_erp_as(msk_dump=os.path.join(params['logdir'], "msk.lst"))

    bssid = apdev[0]['bssid']
    params = hostapd.wpa2_eap_params(ssid="fils")
    params['wpa_key_mgmt'] = "FILS-SHA256"
    params['auth_server_port'] = "18128"
    params['erp_send_reauth_start'] = '1'
    params['erp_domain'] = 'example.com'
    params['fils_realm'] = 'example.com'
    params['wpa_group_rekey'] = '1'
    params['fils_discovery_min_interval'] = '20'
    params['fils_discovery_max_interval'] = '20'
    hapd = hostapd.add_ap(apdev[0]['ifname'], params, no_enable=True)

    if "OK" not in hapd.request("ENABLE"):
        raise HwsimSkip("FILS Discovery frame transmission not supported")

    ev = hapd.wait_event(["AP-ENABLED", "AP-DISABLED"], timeout=5)
    if ev is None:
        raise Exception("AP startup timed out")
    if "AP-ENABLED" not in ev:
        raise Exception("AP startup failed")

    dev[0].request("ERP_FLUSH")
    dev[0].connect("fils", key_mgmt="FILS-SHA256",
                   eap="PSK", identity="psk.user@example.com",
                   password_hex="0123456789abcdef0123456789abcdef",
                   erp="1", scan_freq="2412")
