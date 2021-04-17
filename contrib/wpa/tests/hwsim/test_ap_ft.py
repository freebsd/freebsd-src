# Fast BSS Transition tests
# Copyright (c) 2013-2019, Jouni Malinen <j@w1.fi>
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

from remotehost import remote_compatible
import binascii
import os
import time
import logging
logger = logging.getLogger()
import signal
import struct
import subprocess

import hwsim_utils
from hwsim import HWSimRadio
import hostapd
from tshark import run_tshark
from utils import *
from wlantest import Wlantest
from test_ap_psk import check_mib, find_wpas_process, read_process_memory, verify_not_present, get_key_locations
from test_rrm import check_beacon_req
from test_suite_b import check_suite_b_192_capa

def ft_base_rsn():
    params = {"wpa": "2",
              "wpa_key_mgmt": "FT-PSK",
              "rsn_pairwise": "CCMP"}
    return params

def ft_base_mixed():
    params = {"wpa": "3",
              "wpa_key_mgmt": "WPA-PSK FT-PSK",
              "wpa_pairwise": "TKIP",
              "rsn_pairwise": "CCMP"}
    return params

def ft_params(rsn=True, ssid=None, passphrase=None):
    if rsn:
        params = ft_base_rsn()
    else:
        params = ft_base_mixed()
    if ssid:
        params["ssid"] = ssid
    if passphrase:
        params["wpa_passphrase"] = passphrase

    params["mobility_domain"] = "a1b2"
    params["r0_key_lifetime"] = "10000"
    params["pmk_r1_push"] = "1"
    params["reassociation_deadline"] = "1000"
    return params

def ft_params1a(rsn=True, ssid=None, passphrase=None):
    params = ft_params(rsn, ssid, passphrase)
    params['nas_identifier'] = "nas1.w1.fi"
    params['r1_key_holder'] = "000102030405"
    return params

def ft_params1(rsn=True, ssid=None, passphrase=None, discovery=False):
    params = ft_params1a(rsn, ssid, passphrase)
    if discovery:
        params['r0kh'] = "ff:ff:ff:ff:ff:ff * 100102030405060708090a0b0c0d0e0f100102030405060708090a0b0c0d0e0f"
        params['r1kh'] = "00:00:00:00:00:00 00:00:00:00:00:00 100102030405060708090a0b0c0d0e0f100102030405060708090a0b0c0d0e0f"
    else:
        params['r0kh'] = ["02:00:00:00:03:00 nas1.w1.fi 100102030405060708090a0b0c0d0e0f100102030405060708090a0b0c0d0e0f",
                          "02:00:00:00:04:00 nas2.w1.fi 300102030405060708090a0b0c0d0e0f300102030405060708090a0b0c0d0e0f"]
        params['r1kh'] = "02:00:00:00:04:00 00:01:02:03:04:06 200102030405060708090a0b0c0d0e0f200102030405060708090a0b0c0d0e0f"
    return params

def ft_params1_old_key(rsn=True, ssid=None, passphrase=None):
    params = ft_params1a(rsn, ssid, passphrase)
    params['r0kh'] = ["02:00:00:00:03:00 nas1.w1.fi 100102030405060708090a0b0c0d0e0f",
                      "02:00:00:00:04:00 nas2.w1.fi 300102030405060708090a0b0c0d0e0f"]
    params['r1kh'] = "02:00:00:00:04:00 00:01:02:03:04:06 200102030405060708090a0b0c0d0e0f"
    return params

def ft_params2a(rsn=True, ssid=None, passphrase=None):
    params = ft_params(rsn, ssid, passphrase)
    params['nas_identifier'] = "nas2.w1.fi"
    params['r1_key_holder'] = "000102030406"
    return params

def ft_params2(rsn=True, ssid=None, passphrase=None, discovery=False):
    params = ft_params2a(rsn, ssid, passphrase)
    if discovery:
        params['r0kh'] = "ff:ff:ff:ff:ff:ff * 100102030405060708090a0b0c0d0e0f100102030405060708090a0b0c0d0e0f"
        params['r1kh'] = "00:00:00:00:00:00 00:00:00:00:00:00 100102030405060708090a0b0c0d0e0f100102030405060708090a0b0c0d0e0f"
    else:
        params['r0kh'] = ["02:00:00:00:03:00 nas1.w1.fi 200102030405060708090a0b0c0d0e0f200102030405060708090a0b0c0d0e0f",
                          "02:00:00:00:04:00 nas2.w1.fi 000102030405060708090a0b0c0d0e0f000102030405060708090a0b0c0d0e0f"]
        params['r1kh'] = "02:00:00:00:03:00 00:01:02:03:04:05 300102030405060708090a0b0c0d0e0f300102030405060708090a0b0c0d0e0f"
    return params

def ft_params2_old_key(rsn=True, ssid=None, passphrase=None):
    params = ft_params2a(rsn, ssid, passphrase)
    params['r0kh'] = ["02:00:00:00:03:00 nas1.w1.fi 200102030405060708090a0b0c0d0e0f",
                      "02:00:00:00:04:00 nas2.w1.fi 000102030405060708090a0b0c0d0e0f"]
    params['r1kh'] = "02:00:00:00:03:00 00:01:02:03:04:05 300102030405060708090a0b0c0d0e0f"
    return params

def ft_params1_r0kh_mismatch(rsn=True, ssid=None, passphrase=None):
    params = ft_params(rsn, ssid, passphrase)
    params['nas_identifier'] = "nas1.w1.fi"
    params['r1_key_holder'] = "000102030405"
    params['r0kh'] = ["02:00:00:00:03:00 nas1.w1.fi 100102030405060708090a0b0c0d0e0f100102030405060708090a0b0c0d0e0f",
                      "12:00:00:00:04:00 nas2.w1.fi 300102030405060708090a0b0c0d0e0f300102030405060708090a0b0c0d0e0f"]
    params['r1kh'] = "12:00:00:00:04:00 10:01:02:03:04:06 200102030405060708090a0b0c0d0e0f200102030405060708090a0b0c0d0e0f"
    return params

def ft_params2_incorrect_rrb_key(rsn=True, ssid=None, passphrase=None):
    params = ft_params(rsn, ssid, passphrase)
    params['nas_identifier'] = "nas2.w1.fi"
    params['r1_key_holder'] = "000102030406"
    params['r0kh'] = ["02:00:00:00:03:00 nas1.w1.fi 200102030405060708090a0b0c0d0ef1200102030405060708090a0b0c0d0ef1",
                      "02:00:00:00:04:00 nas2.w1.fi 000102030405060708090a0b0c0d0ef2000102030405060708090a0b0c0d0ef2"]
    params['r1kh'] = "02:00:00:00:03:00 00:01:02:03:04:05 300102030405060708090a0b0c0d0ef3300102030405060708090a0b0c0d0ef3"
    return params

def ft_params2_r0kh_mismatch(rsn=True, ssid=None, passphrase=None):
    params = ft_params(rsn, ssid, passphrase)
    params['nas_identifier'] = "nas2.w1.fi"
    params['r1_key_holder'] = "000102030406"
    params['r0kh'] = ["12:00:00:00:03:00 nas1.w1.fi 200102030405060708090a0b0c0d0e0f200102030405060708090a0b0c0d0e0f",
                      "02:00:00:00:04:00 nas2.w1.fi 000102030405060708090a0b0c0d0e0f000102030405060708090a0b0c0d0e0f"]
    params['r1kh'] = "12:00:00:00:03:00 10:01:02:03:04:05 300102030405060708090a0b0c0d0e0f300102030405060708090a0b0c0d0e0f"
    return params

def run_roams(dev, apdev, hapd0, hapd1, ssid, passphrase, over_ds=False,
              sae=False, eap=False, fail_test=False, roams=1,
              pairwise_cipher="CCMP", group_cipher="CCMP", ptk_rekey="0",
              test_connectivity=True, eap_identity="gpsk user", conndev=False,
              force_initial_conn_to_first_ap=False, sha384=False,
              group_mgmt=None, ocv=None, sae_password=None,
              sae_password_id=None, sae_and_psk=False, pmksa_caching=False,
              roam_with_reassoc=False, also_non_ft=False, only_one_way=False,
              wait_before_roam=0, return_after_initial=False, ieee80211w="1",
              sae_transition=False, beacon_prot=False):
    logger.info("Connect to first AP")

    copts = {}
    copts["proto"] = "WPA2"
    copts["ieee80211w"] = ieee80211w
    copts["scan_freq"] = "2412"
    copts["pairwise"] = pairwise_cipher
    copts["group"] = group_cipher
    copts["wpa_ptk_rekey"] = ptk_rekey
    if group_mgmt:
        copts["group_mgmt"] = group_mgmt
    if ocv:
        copts["ocv"] = ocv
    if beacon_prot:
        copts["beacon_prot"] = "1"
    if eap:
        if pmksa_caching:
            copts["ft_eap_pmksa_caching"] = "1"
        if also_non_ft:
            copts["key_mgmt"] = "WPA-EAP-SUITE-B-192 FT-EAP-SHA384" if sha384 else "WPA-EAP FT-EAP"
        else:
            copts["key_mgmt"] = "FT-EAP-SHA384" if sha384 else "FT-EAP"
        copts["eap"] = "GPSK"
        copts["identity"] = eap_identity
        copts["password"] = "abcdefghijklmnop0123456789abcdef"
    else:
        if sae_transition:
            copts["key_mgmt"] = "FT-SAE FT-PSK"
        elif sae:
            copts["key_mgmt"] = "SAE FT-SAE" if sae_and_psk else "FT-SAE"
        else:
            copts["key_mgmt"] = "FT-PSK"
        if passphrase:
            copts["psk"] = passphrase
        if sae_password:
            copts["sae_password"] = sae_password
        if sae_password_id:
            copts["sae_password_id"] = sae_password_id
    if force_initial_conn_to_first_ap:
        copts["bssid"] = apdev[0]['bssid']
    netw = dev.connect(ssid, **copts)
    if pmksa_caching:
        if dev.get_status_field('bssid') == apdev[0]['bssid']:
            hapd0.wait_sta()
        else:
            hapd1.wait_sta()
        dev.request("DISCONNECT")
        dev.wait_disconnected()
        dev.request("RECONNECT")
        ev = dev.wait_event(["CTRL-EVENT-CONNECTED",
                             "CTRL-EVENT-DISCONNECTED",
                             "CTRL-EVENT-EAP-STARTED"],
                            timeout=15)
        if ev is None:
            raise Exception("Reconnect timed out")
        if "CTRL-EVENT-DISCONNECTED" in ev:
            raise Exception("Unexpected disconnection after RECONNECT")
        if "CTRL-EVENT-EAP-STARTED" in ev:
            raise Exception("Unexpected EAP start after RECONNECT")

    if dev.get_status_field('bssid') == apdev[0]['bssid']:
        ap1 = apdev[0]
        ap2 = apdev[1]
        hapd1ap = hapd0
        hapd2ap = hapd1
    else:
        ap1 = apdev[1]
        ap2 = apdev[0]
        hapd1ap = hapd1
        hapd2ap = hapd0
    if test_connectivity:
        hapd1ap.wait_sta()
        if conndev:
            hwsim_utils.test_connectivity_iface(dev, hapd1ap, conndev)
        else:
            hwsim_utils.test_connectivity(dev, hapd1ap)

    if return_after_initial:
        return ap2['bssid']

    if wait_before_roam:
        time.sleep(wait_before_roam)
    dev.scan_for_bss(ap2['bssid'], freq="2412")

    for i in range(0, roams):
        dev.dump_monitor()
        hapd1ap.dump_monitor()
        hapd2ap.dump_monitor()

        # Roaming artificially fast can make data test fail because the key is
        # set later.
        time.sleep(0.01)
        logger.info("Roam to the second AP")
        if roam_with_reassoc:
            dev.set_network(netw, "bssid", ap2['bssid'])
            dev.request("REASSOCIATE")
            dev.wait_connected()
        elif over_ds:
            dev.roam_over_ds(ap2['bssid'], fail_test=fail_test)
        else:
            dev.roam(ap2['bssid'], fail_test=fail_test)
        if fail_test:
            return
        if dev.get_status_field('bssid') != ap2['bssid']:
            raise Exception("Did not connect to correct AP")
        if (i == 0 or i == roams - 1) and test_connectivity:
            hapd2ap.wait_sta()
            dev.dump_monitor()
            hapd1ap.dump_monitor()
            hapd2ap.dump_monitor()
            if conndev:
                hwsim_utils.test_connectivity_iface(dev, hapd2ap, conndev)
            else:
                hwsim_utils.test_connectivity(dev, hapd2ap)

        dev.dump_monitor()
        hapd1ap.dump_monitor()
        hapd2ap.dump_monitor()

        if only_one_way:
            return
        # Roaming artificially fast can make data test fail because the key is
        # set later.
        time.sleep(0.01)
        logger.info("Roam back to the first AP")
        if roam_with_reassoc:
            dev.set_network(netw, "bssid", ap1['bssid'])
            dev.request("REASSOCIATE")
            dev.wait_connected()
        elif over_ds:
            dev.roam_over_ds(ap1['bssid'])
        else:
            dev.roam(ap1['bssid'])
        if dev.get_status_field('bssid') != ap1['bssid']:
            raise Exception("Did not connect to correct AP")
        if (i == 0 or i == roams - 1) and test_connectivity:
            hapd1ap.wait_sta()
            dev.dump_monitor()
            hapd1ap.dump_monitor()
            hapd2ap.dump_monitor()
            if conndev:
                hwsim_utils.test_connectivity_iface(dev, hapd1ap, conndev)
            else:
                hwsim_utils.test_connectivity(dev, hapd1ap)

def test_ap_ft(dev, apdev):
    """WPA2-PSK-FT AP"""
    ssid = "test-ft"
    passphrase = "12345678"

    params = ft_params1(ssid=ssid, passphrase=passphrase)
    hapd0 = hostapd.add_ap(apdev[0], params)
    params = ft_params2(ssid=ssid, passphrase=passphrase)
    hapd1 = hostapd.add_ap(apdev[1], params)

    run_roams(dev[0], apdev, hapd0, hapd1, ssid, passphrase)
    if "[WPA2-FT/PSK-CCMP]" not in dev[0].request("SCAN_RESULTS"):
        raise Exception("Scan results missing RSN element info")

def test_ap_ft_old_key(dev, apdev):
    """WPA2-PSK-FT AP (old key)"""
    ssid = "test-ft"
    passphrase = "12345678"

    params = ft_params1_old_key(ssid=ssid, passphrase=passphrase)
    hapd0 = hostapd.add_ap(apdev[0], params)
    params = ft_params2_old_key(ssid=ssid, passphrase=passphrase)
    hapd1 = hostapd.add_ap(apdev[1], params)

    run_roams(dev[0], apdev, hapd0, hapd1, ssid, passphrase)

def test_ap_ft_multi_akm(dev, apdev):
    """WPA2-PSK-FT AP with non-FT AKMs enabled"""
    ssid = "test-ft"
    passphrase = "12345678"

    params = ft_params1(ssid=ssid, passphrase=passphrase)
    params["wpa_key_mgmt"] = "FT-PSK WPA-PSK WPA-PSK-SHA256"
    hapd0 = hostapd.add_ap(apdev[0], params)
    params = ft_params2(ssid=ssid, passphrase=passphrase)
    params["wpa_key_mgmt"] = "FT-PSK WPA-PSK WPA-PSK-SHA256"
    hapd1 = hostapd.add_ap(apdev[1], params)

    Wlantest.setup(hapd0)
    wt = Wlantest()
    wt.flush()
    wt.add_passphrase(passphrase)

    run_roams(dev[0], apdev, hapd0, hapd1, ssid, passphrase)
    if "[WPA2-PSK+FT/PSK+PSK-SHA256-CCMP]" not in dev[0].request("SCAN_RESULTS"):
        raise Exception("Scan results missing RSN element info")
    dev[1].connect(ssid, psk=passphrase, scan_freq="2412")
    dev[2].connect(ssid, psk=passphrase, key_mgmt="WPA-PSK-SHA256",
                   scan_freq="2412")

def test_ap_ft_local_key_gen(dev, apdev):
    """WPA2-PSK-FT AP with local key generation (without pull/push)"""
    ssid = "test-ft"
    passphrase = "12345678"

    params = ft_params1a(ssid=ssid, passphrase=passphrase)
    params['ft_psk_generate_local'] = "1"
    del params['pmk_r1_push']
    hapd0 = hostapd.add_ap(apdev[0], params)
    params = ft_params2a(ssid=ssid, passphrase=passphrase)
    params['ft_psk_generate_local'] = "1"
    del params['pmk_r1_push']
    hapd1 = hostapd.add_ap(apdev[1], params)

    run_roams(dev[0], apdev, hapd0, hapd1, ssid, passphrase)
    if "[WPA2-FT/PSK-CCMP]" not in dev[0].request("SCAN_RESULTS"):
        raise Exception("Scan results missing RSN element info")

def test_ap_ft_vlan(dev, apdev):
    """WPA2-PSK-FT AP with VLAN"""
    ssid = "test-ft"
    passphrase = "12345678"
    filename = hostapd.acl_file(dev, apdev, 'hostapd.accept')
    hostapd.send_file(apdev[0], filename, filename)
    hostapd.send_file(apdev[1], filename, filename)

    params = ft_params1(ssid=ssid, passphrase=passphrase)
    params['dynamic_vlan'] = "1"
    params['accept_mac_file'] = filename
    hapd0 = hostapd.add_ap(apdev[0]['ifname'], params)

    params = ft_params2(ssid=ssid, passphrase=passphrase)
    params['dynamic_vlan'] = "1"
    params['accept_mac_file'] = filename
    hapd1 = hostapd.add_ap(apdev[1]['ifname'], params)

    run_roams(dev[0], apdev, hapd0, hapd1, ssid, passphrase, conndev="brvlan1")
    if "[WPA2-FT/PSK-CCMP]" not in dev[0].request("SCAN_RESULTS"):
        raise Exception("Scan results missing RSN element info")
    if filename.startswith('/tmp/'):
        os.unlink(filename)

def test_ap_ft_vlan_disconnected(dev, apdev):
    """WPA2-PSK-FT AP with VLAN and local key generation"""
    ssid = "test-ft"
    passphrase = "12345678"
    filename = hostapd.acl_file(dev, apdev, 'hostapd.accept')
    hostapd.send_file(apdev[0], filename, filename)
    hostapd.send_file(apdev[1], filename, filename)

    params = ft_params1a(ssid=ssid, passphrase=passphrase)
    params['dynamic_vlan'] = "1"
    params['accept_mac_file'] = filename
    params['ft_psk_generate_local'] = "1"
    hapd0 = hostapd.add_ap(apdev[0]['ifname'], params)

    params = ft_params2a(ssid=ssid, passphrase=passphrase)
    params['dynamic_vlan'] = "1"
    params['accept_mac_file'] = filename
    params['ft_psk_generate_local'] = "1"
    hapd1 = hostapd.add_ap(apdev[1]['ifname'], params)

    run_roams(dev[0], apdev, hapd0, hapd1, ssid, passphrase, conndev="brvlan1")
    if "[WPA2-FT/PSK-CCMP]" not in dev[0].request("SCAN_RESULTS"):
        raise Exception("Scan results missing RSN element info")
    if filename.startswith('/tmp/'):
        os.unlink(filename)

def test_ap_ft_vlan_2(dev, apdev):
    """WPA2-PSK-FT AP with VLAN and dest-AP does not have VLAN info locally"""
    ssid = "test-ft"
    passphrase = "12345678"
    filename = hostapd.acl_file(dev, apdev, 'hostapd.accept')
    hostapd.send_file(apdev[0], filename, filename)

    params = ft_params1(ssid=ssid, passphrase=passphrase)
    params['dynamic_vlan'] = "1"
    params['accept_mac_file'] = filename
    hapd0 = hostapd.add_ap(apdev[0]['ifname'], params)

    params = ft_params2(ssid=ssid, passphrase=passphrase)
    params['dynamic_vlan'] = "1"
    hapd1 = hostapd.add_ap(apdev[1]['ifname'], params)

    run_roams(dev[0], apdev, hapd0, hapd1, ssid, passphrase, conndev="brvlan1",
              force_initial_conn_to_first_ap=True)
    if "[WPA2-FT/PSK-CCMP]" not in dev[0].request("SCAN_RESULTS"):
        raise Exception("Scan results missing RSN element info")
    if filename.startswith('/tmp/'):
        os.unlink(filename)

def test_ap_ft_many(dev, apdev):
    """WPA2-PSK-FT AP multiple times"""
    ssid = "test-ft"
    passphrase = "12345678"

    params = ft_params1(ssid=ssid, passphrase=passphrase)
    hapd0 = hostapd.add_ap(apdev[0], params)
    params = ft_params2(ssid=ssid, passphrase=passphrase)
    hapd1 = hostapd.add_ap(apdev[1], params)

    run_roams(dev[0], apdev, hapd0, hapd1, ssid, passphrase, roams=50)

def test_ap_ft_many_vlan(dev, apdev):
    """WPA2-PSK-FT AP with VLAN multiple times"""
    ssid = "test-ft"
    passphrase = "12345678"
    filename = hostapd.acl_file(dev, apdev, 'hostapd.accept')
    hostapd.send_file(apdev[0], filename, filename)
    hostapd.send_file(apdev[1], filename, filename)

    params = ft_params1(ssid=ssid, passphrase=passphrase)
    params['dynamic_vlan'] = "1"
    params['accept_mac_file'] = filename
    hapd0 = hostapd.add_ap(apdev[0]['ifname'], params)

    params = ft_params2(ssid=ssid, passphrase=passphrase)
    params['dynamic_vlan'] = "1"
    params['accept_mac_file'] = filename
    hapd1 = hostapd.add_ap(apdev[1]['ifname'], params)

    run_roams(dev[0], apdev, hapd0, hapd1, ssid, passphrase, roams=50,
              conndev="brvlan1")
    if filename.startswith('/tmp/'):
        os.unlink(filename)

def test_ap_ft_mixed(dev, apdev):
    """WPA2-PSK-FT mixed-mode AP"""
    skip_without_tkip(dev[0])
    ssid = "test-ft-mixed"
    passphrase = "12345678"

    params = ft_params1(rsn=False, ssid=ssid, passphrase=passphrase)
    hapd = hostapd.add_ap(apdev[0], params)
    key_mgmt = hapd.get_config()['key_mgmt']
    vals = key_mgmt.split(' ')
    if vals[0] != "WPA-PSK" or vals[1] != "FT-PSK":
        raise Exception("Unexpected GET_CONFIG(key_mgmt): " + key_mgmt)
    params = ft_params2(rsn=False, ssid=ssid, passphrase=passphrase)
    hapd1 = hostapd.add_ap(apdev[1], params)

    run_roams(dev[0], apdev, hapd, hapd1, ssid, passphrase,
              group_cipher="TKIP CCMP")

def test_ap_ft_pmf(dev, apdev):
    """WPA2-PSK-FT AP with PMF"""
    run_ap_ft_pmf(dev, apdev, "1")

def test_ap_ft_pmf_over_ds(dev, apdev):
    """WPA2-PSK-FT AP with PMF (over DS)"""
    run_ap_ft_pmf(dev, apdev, "1", over_ds=True)

def test_ap_ft_pmf_required(dev, apdev):
    """WPA2-PSK-FT AP with PMF required on STA"""
    run_ap_ft_pmf(dev, apdev, "2")

def test_ap_ft_pmf_required_over_ds(dev, apdev):
    """WPA2-PSK-FT AP with PMF required on STA (over DS)"""
    run_ap_ft_pmf(dev, apdev, "2", over_ds=True)

def test_ap_ft_pmf_beacon_prot(dev, apdev):
    """WPA2-PSK-FT AP with PMF and beacon protection"""
    run_ap_ft_pmf(dev, apdev, "1", beacon_prot=True)

def run_ap_ft_pmf(dev, apdev, ieee80211w, over_ds=False, beacon_prot=False):
    ssid = "test-ft"
    passphrase = "12345678"

    params = ft_params1(ssid=ssid, passphrase=passphrase)
    params["ieee80211w"] = "2"
    if beacon_prot:
        params["beacon_prot"] = "1"
    hapd0 = hostapd.add_ap(apdev[0], params)
    params = ft_params2(ssid=ssid, passphrase=passphrase)
    params["ieee80211w"] = "2"
    if beacon_prot:
        params["beacon_prot"] = "1"
    hapd1 = hostapd.add_ap(apdev[1], params)

    Wlantest.setup(hapd0)
    wt = Wlantest()
    wt.flush()
    wt.add_passphrase(passphrase)

    run_roams(dev[0], apdev, hapd0, hapd1, ssid, passphrase,
              ieee80211w=ieee80211w, over_ds=over_ds, beacon_prot=beacon_prot)

def test_ap_ft_pmf_required_mismatch(dev, apdev):
    """WPA2-PSK-FT AP with PMF required on STA but AP2 not enabling PMF"""
    run_ap_ft_pmf_required_mismatch(dev, apdev)

def test_ap_ft_pmf_required_mismatch_over_ds(dev, apdev):
    """WPA2-PSK-FT AP with PMF required on STA but AP2 not enabling PMF (over DS)"""
    run_ap_ft_pmf_required_mismatch(dev, apdev, over_ds=True)

def run_ap_ft_pmf_required_mismatch(dev, apdev, over_ds=False):
    ssid = "test-ft"
    passphrase = "12345678"

    params = ft_params1(ssid=ssid, passphrase=passphrase)
    params["ieee80211w"] = "2"
    hapd0 = hostapd.add_ap(apdev[0], params)
    params = ft_params2(ssid=ssid, passphrase=passphrase)
    params["ieee80211w"] = "0"
    hapd1 = hostapd.add_ap(apdev[1], params)

    run_roams(dev[0], apdev, hapd0, hapd1, ssid, passphrase, ieee80211w="2",
              force_initial_conn_to_first_ap=True, fail_test=True,
              over_ds=over_ds)

def test_ap_ft_pmf_bip_cmac_128(dev, apdev):
    """WPA2-PSK-FT AP with PMF/BIP-CMAC-128"""
    run_ap_ft_pmf_bip(dev, apdev, "AES-128-CMAC")

def test_ap_ft_pmf_bip_gmac_128(dev, apdev):
    """WPA2-PSK-FT AP with PMF/BIP-GMAC-128"""
    run_ap_ft_pmf_bip(dev, apdev, "BIP-GMAC-128")

def test_ap_ft_pmf_bip_gmac_256(dev, apdev):
    """WPA2-PSK-FT AP with PMF/BIP-GMAC-256"""
    run_ap_ft_pmf_bip(dev, apdev, "BIP-GMAC-256")

def test_ap_ft_pmf_bip_cmac_256(dev, apdev):
    """WPA2-PSK-FT AP with PMF/BIP-CMAC-256"""
    run_ap_ft_pmf_bip(dev, apdev, "BIP-CMAC-256")

def run_ap_ft_pmf_bip(dev, apdev, cipher):
    if cipher not in dev[0].get_capability("group_mgmt"):
        raise HwsimSkip("Cipher %s not supported" % cipher)

    ssid = "test-ft"
    passphrase = "12345678"

    params = ft_params1(ssid=ssid, passphrase=passphrase)
    params["ieee80211w"] = "2"
    params["group_mgmt_cipher"] = cipher
    hapd0 = hostapd.add_ap(apdev[0], params)
    params = ft_params2(ssid=ssid, passphrase=passphrase)
    params["ieee80211w"] = "2"
    params["group_mgmt_cipher"] = cipher
    hapd1 = hostapd.add_ap(apdev[1], params)

    run_roams(dev[0], apdev, hapd0, hapd1, ssid, passphrase,
              group_mgmt=cipher)

def test_ap_ft_ocv(dev, apdev):
    """WPA2-PSK-FT AP with OCV"""
    ssid = "test-ft"
    passphrase = "12345678"

    params = ft_params1(ssid=ssid, passphrase=passphrase)
    params["ieee80211w"] = "2"
    params["ocv"] = "1"
    try:
        hapd0 = hostapd.add_ap(apdev[0], params)
    except Exception as e:
        if "Failed to set hostapd parameter ocv" in str(e):
            raise HwsimSkip("OCV not supported")
        raise
    params = ft_params2(ssid=ssid, passphrase=passphrase)
    params["ieee80211w"] = "2"
    params["ocv"] = "1"
    hapd1 = hostapd.add_ap(apdev[1], params)

    run_roams(dev[0], apdev, hapd0, hapd1, ssid, passphrase, ocv="1")

def test_ap_ft_over_ds(dev, apdev):
    """WPA2-PSK-FT AP over DS"""
    ssid = "test-ft"
    passphrase = "12345678"

    params = ft_params1(ssid=ssid, passphrase=passphrase)
    hapd0 = hostapd.add_ap(apdev[0], params)
    params = ft_params2(ssid=ssid, passphrase=passphrase)
    hapd1 = hostapd.add_ap(apdev[1], params)

    run_roams(dev[0], apdev, hapd0, hapd1, ssid, passphrase, over_ds=True)
    check_mib(dev[0], [("dot11RSNAAuthenticationSuiteRequested", "00-0f-ac-4"),
                       ("dot11RSNAAuthenticationSuiteSelected", "00-0f-ac-4")])

def cleanup_ap_ft_separate_hostapd():
    subprocess.call(["brctl", "delif", "br0ft", "veth0"],
                    stderr=open('/dev/null', 'w'))
    subprocess.call(["brctl", "delif", "br1ft", "veth1"],
                    stderr=open('/dev/null', 'w'))
    subprocess.call(["ip", "link", "del", "veth0"],
                    stderr=open('/dev/null', 'w'))
    subprocess.call(["ip", "link", "del", "veth1"],
                    stderr=open('/dev/null', 'w'))
    for ifname in ['br0ft', 'br1ft', 'br-ft']:
        subprocess.call(['ip', 'link', 'set', 'dev', ifname, 'down'],
                        stderr=open('/dev/null', 'w'))
        subprocess.call(['brctl', 'delbr', ifname],
                        stderr=open('/dev/null', 'w'))

def test_ap_ft_separate_hostapd(dev, apdev, params):
    """WPA2-PSK-FT AP and separate hostapd process"""
    try:
        run_ap_ft_separate_hostapd(dev, apdev, params, False)
    finally:
        cleanup_ap_ft_separate_hostapd()

def test_ap_ft_over_ds_separate_hostapd(dev, apdev, params):
    """WPA2-PSK-FT AP over DS and separate hostapd process"""
    try:
        run_ap_ft_separate_hostapd(dev, apdev, params, True)
    finally:
        cleanup_ap_ft_separate_hostapd()

def run_ap_ft_separate_hostapd(dev, apdev, params, over_ds):
    ssid = "test-ft"
    passphrase = "12345678"
    logdir = params['logdir']
    pidfile = os.path.join(logdir, 'ap_ft_over_ds_separate_hostapd.pid')
    logfile = os.path.join(logdir, 'ap_ft_over_ds_separate_hostapd.hapd')
    global_ctrl = '/var/run/hostapd-ft'
    br_ifname = 'br-ft'

    try:
        subprocess.check_call(['brctl', 'addbr', br_ifname])
        subprocess.check_call(['brctl', 'setfd', br_ifname, '0'])
        subprocess.check_call(['ip', 'link', 'set', 'dev', br_ifname, 'up'])

        subprocess.check_call(["ip", "link", "add", "veth0", "type", "veth",
                               "peer", "name", "veth0br"])
        subprocess.check_call(["ip", "link", "add", "veth1", "type", "veth",
                               "peer", "name", "veth1br"])
        subprocess.check_call(['ip', 'link', 'set', 'dev', 'veth0br', 'up'])
        subprocess.check_call(['ip', 'link', 'set', 'dev', 'veth1br', 'up'])
        subprocess.check_call(['brctl', 'addif', br_ifname, 'veth0br'])
        subprocess.check_call(['brctl', 'addif', br_ifname, 'veth1br'])

        subprocess.check_call(['brctl', 'addbr', 'br0ft'])
        subprocess.check_call(['brctl', 'setfd', 'br0ft', '0'])
        subprocess.check_call(['ip', 'link', 'set', 'dev', 'br0ft', 'up'])
        subprocess.check_call(['ip', 'link', 'set', 'dev', 'veth0', 'up'])
        subprocess.check_call(['brctl', 'addif', 'br0ft', 'veth0'])
        subprocess.check_call(['brctl', 'addbr', 'br1ft'])
        subprocess.check_call(['brctl', 'setfd', 'br1ft', '0'])
        subprocess.check_call(['ip', 'link', 'set', 'dev', 'br1ft', 'up'])
        subprocess.check_call(['ip', 'link', 'set', 'dev', 'veth1', 'up'])
        subprocess.check_call(['brctl', 'addif', 'br1ft', 'veth1'])
    except subprocess.CalledProcessError:
        raise HwsimSkip("Bridge or veth not supported (kernel CONFIG_VETH)")

    with HWSimRadio() as (radio, iface):
        prg = os.path.join(logdir, 'alt-hostapd/hostapd/hostapd')
        if not os.path.exists(prg):
            prg = '../../hostapd/hostapd'
        cmd = [prg, '-B', '-ddKt',
               '-P', pidfile, '-f', logfile, '-g', global_ctrl]
        subprocess.check_call(cmd)

        hglobal = hostapd.HostapdGlobal(global_ctrl_override=global_ctrl)
        apdev_ft = {'ifname': iface}
        apdev2 = [apdev_ft, apdev[1]]

        params = ft_params1(ssid=ssid, passphrase=passphrase)
        params["r0kh"] = "ff:ff:ff:ff:ff:ff * 00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff"
        params["r1kh"] = "00:00:00:00:00:00 00:00:00:00:00:00 00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff"
        params['bridge'] = 'br0ft'
        hapd0 = hostapd.add_ap(apdev2[0], params,
                               global_ctrl_override=global_ctrl)
        apdev2[0]['bssid'] = hapd0.own_addr()
        params = ft_params2(ssid=ssid, passphrase=passphrase)
        params["r0kh"] = "ff:ff:ff:ff:ff:ff * 00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff"
        params["r1kh"] = "00:00:00:00:00:00 00:00:00:00:00:00 00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff"
        params['bridge'] = 'br1ft'
        hapd1 = hostapd.add_ap(apdev2[1], params)

        run_roams(dev[0], apdev2, hapd0, hapd1, ssid, passphrase,
                  over_ds=over_ds, test_connectivity=False, roams=2)

        hglobal.terminate()

    if os.path.exists(pidfile):
        with open(pidfile, 'r') as f:
            pid = int(f.read())
            f.close()
        os.kill(pid, signal.SIGTERM)

def test_ap_ft_over_ds_ocv(dev, apdev):
    """WPA2-PSK-FT AP over DS"""
    ssid = "test-ft"
    passphrase = "12345678"

    params = ft_params1(ssid=ssid, passphrase=passphrase)
    params["ieee80211w"] = "2"
    params["ocv"] = "1"
    try:
        hapd0 = hostapd.add_ap(apdev[0], params)
    except Exception as e:
        if "Failed to set hostapd parameter ocv" in str(e):
            raise HwsimSkip("OCV not supported")
        raise
    params = ft_params2(ssid=ssid, passphrase=passphrase)
    params["ieee80211w"] = "2"
    params["ocv"] = "1"
    hapd1 = hostapd.add_ap(apdev[1], params)

    run_roams(dev[0], apdev, hapd0, hapd1, ssid, passphrase, over_ds=True,
              ocv="1")

def test_ap_ft_over_ds_disabled(dev, apdev):
    """WPA2-PSK-FT AP over DS disabled"""
    ssid = "test-ft"
    passphrase = "12345678"

    params = ft_params1(ssid=ssid, passphrase=passphrase)
    params['ft_over_ds'] = '0'
    hapd0 = hostapd.add_ap(apdev[0], params)
    params = ft_params2(ssid=ssid, passphrase=passphrase)
    params['ft_over_ds'] = '0'
    hapd1 = hostapd.add_ap(apdev[1], params)

    run_roams(dev[0], apdev, hapd0, hapd1, ssid, passphrase, over_ds=True,
              fail_test=True)

def test_ap_ft_vlan_over_ds(dev, apdev):
    """WPA2-PSK-FT AP over DS with VLAN"""
    ssid = "test-ft"
    passphrase = "12345678"
    filename = hostapd.acl_file(dev, apdev, 'hostapd.accept')
    hostapd.send_file(apdev[0], filename, filename)
    hostapd.send_file(apdev[1], filename, filename)

    params = ft_params1(ssid=ssid, passphrase=passphrase)
    params['dynamic_vlan'] = "1"
    params['accept_mac_file'] = filename
    hapd0 = hostapd.add_ap(apdev[0]['ifname'], params)
    params = ft_params2(ssid=ssid, passphrase=passphrase)
    params['dynamic_vlan'] = "1"
    params['accept_mac_file'] = filename
    hapd1 = hostapd.add_ap(apdev[1]['ifname'], params)

    run_roams(dev[0], apdev, hapd0, hapd1, ssid, passphrase, over_ds=True,
              conndev="brvlan1")
    check_mib(dev[0], [("dot11RSNAAuthenticationSuiteRequested", "00-0f-ac-4"),
                       ("dot11RSNAAuthenticationSuiteSelected", "00-0f-ac-4")])
    if filename.startswith('/tmp/'):
        os.unlink(filename)

def test_ap_ft_over_ds_many(dev, apdev):
    """WPA2-PSK-FT AP over DS multiple times"""
    ssid = "test-ft"
    passphrase = "12345678"

    params = ft_params1(ssid=ssid, passphrase=passphrase)
    hapd0 = hostapd.add_ap(apdev[0], params)
    params = ft_params2(ssid=ssid, passphrase=passphrase)
    hapd1 = hostapd.add_ap(apdev[1], params)

    run_roams(dev[0], apdev, hapd0, hapd1, ssid, passphrase, over_ds=True,
              roams=50)

def test_ap_ft_vlan_over_ds_many(dev, apdev):
    """WPA2-PSK-FT AP over DS with VLAN multiple times"""
    ssid = "test-ft"
    passphrase = "12345678"
    filename = hostapd.acl_file(dev, apdev, 'hostapd.accept')
    hostapd.send_file(apdev[0], filename, filename)
    hostapd.send_file(apdev[1], filename, filename)

    params = ft_params1(ssid=ssid, passphrase=passphrase)
    params['dynamic_vlan'] = "1"
    params['accept_mac_file'] = filename
    hapd0 = hostapd.add_ap(apdev[0]['ifname'], params)
    params = ft_params2(ssid=ssid, passphrase=passphrase)
    params['dynamic_vlan'] = "1"
    params['accept_mac_file'] = filename
    hapd1 = hostapd.add_ap(apdev[1]['ifname'], params)

    run_roams(dev[0], apdev, hapd0, hapd1, ssid, passphrase, over_ds=True,
              roams=50, conndev="brvlan1")
    if filename.startswith('/tmp/'):
        os.unlink(filename)

@remote_compatible
def test_ap_ft_over_ds_unknown_target(dev, apdev):
    """WPA2-PSK-FT AP"""
    ssid = "test-ft"
    passphrase = "12345678"

    params = ft_params1(ssid=ssid, passphrase=passphrase)
    hapd0 = hostapd.add_ap(apdev[0], params)

    dev[0].connect(ssid, psk=passphrase, key_mgmt="FT-PSK", proto="WPA2",
                   scan_freq="2412")
    dev[0].roam_over_ds("02:11:22:33:44:55", fail_test=True)

@remote_compatible
def test_ap_ft_over_ds_unexpected(dev, apdev):
    """WPA2-PSK-FT AP over DS and unexpected response"""
    ssid = "test-ft"
    passphrase = "12345678"

    params = ft_params1(ssid=ssid, passphrase=passphrase)
    hapd0 = hostapd.add_ap(apdev[0], params)
    params = ft_params2(ssid=ssid, passphrase=passphrase)
    hapd1 = hostapd.add_ap(apdev[1], params)

    dev[0].connect(ssid, psk=passphrase, key_mgmt="FT-PSK", proto="WPA2",
                   scan_freq="2412")
    if dev[0].get_status_field('bssid') == apdev[0]['bssid']:
        ap1 = apdev[0]
        ap2 = apdev[1]
        hapd1ap = hapd0
        hapd2ap = hapd1
    else:
        ap1 = apdev[1]
        ap2 = apdev[0]
        hapd1ap = hapd1
        hapd2ap = hapd0

    addr = dev[0].own_addr()
    hapd1ap.set("ext_mgmt_frame_handling", "1")
    logger.info("Foreign STA address")
    msg = {}
    msg['fc'] = 13 << 4
    msg['da'] = addr
    msg['sa'] = ap1['bssid']
    msg['bssid'] = ap1['bssid']
    msg['payload'] = binascii.unhexlify("06021122334455660102030405060000")
    hapd1ap.mgmt_tx(msg)

    logger.info("No over-the-DS in progress")
    msg['payload'] = binascii.unhexlify("0602" + addr.replace(':', '') + "0102030405060000")
    hapd1ap.mgmt_tx(msg)

    logger.info("Non-zero status code")
    msg['payload'] = binascii.unhexlify("0602" + addr.replace(':', '') + "0102030405060100")
    hapd1ap.mgmt_tx(msg)

    hapd1ap.dump_monitor()

    dev[0].scan_for_bss(ap2['bssid'], freq="2412")
    if "OK" not in dev[0].request("FT_DS " + ap2['bssid']):
            raise Exception("FT_DS failed")

    req = hapd1ap.mgmt_rx()

    logger.info("Foreign Target AP")
    msg['payload'] = binascii.unhexlify("0602" + addr.replace(':', '') + "0102030405060000")
    hapd1ap.mgmt_tx(msg)

    addrs = addr.replace(':', '') + ap2['bssid'].replace(':', '')

    logger.info("No IEs")
    msg['payload'] = binascii.unhexlify("0602" + addrs + "0000")
    hapd1ap.mgmt_tx(msg)

    logger.info("Invalid IEs (trigger parsing failure)")
    msg['payload'] = binascii.unhexlify("0602" + addrs + "00003700")
    hapd1ap.mgmt_tx(msg)

    logger.info("Too short MDIE")
    msg['payload'] = binascii.unhexlify("0602" + addrs + "000036021122")
    hapd1ap.mgmt_tx(msg)

    logger.info("Mobility domain mismatch")
    msg['payload'] = binascii.unhexlify("0602" + addrs + "00003603112201")
    hapd1ap.mgmt_tx(msg)

    logger.info("No FTIE")
    msg['payload'] = binascii.unhexlify("0602" + addrs + "00003603a1b201")
    hapd1ap.mgmt_tx(msg)

    logger.info("FTIE SNonce mismatch")
    msg['payload'] = binascii.unhexlify("0602" + addrs + "00003603a1b201375e0000" + "00000000000000000000000000000000" + "0000000000000000000000000000000000000000000000000000000000000000" + "1000000000000000000000000000000000000000000000000000000000000001" + "030a6e6173322e77312e6669")
    hapd1ap.mgmt_tx(msg)

    logger.info("No R0KH-ID subelem in FTIE")
    snonce = binascii.hexlify(req['payload'][111:111+32]).decode()
    msg['payload'] = binascii.unhexlify("0602" + addrs + "00003603a1b20137520000" + "00000000000000000000000000000000" + "0000000000000000000000000000000000000000000000000000000000000000" + snonce)
    hapd1ap.mgmt_tx(msg)

    logger.info("No R0KH-ID subelem mismatch in FTIE")
    snonce = binascii.hexlify(req['payload'][111:111+32]).decode()
    msg['payload'] = binascii.unhexlify("0602" + addrs + "00003603a1b201375e0000" + "00000000000000000000000000000000" + "0000000000000000000000000000000000000000000000000000000000000000" + snonce + "030a11223344556677889900")
    hapd1ap.mgmt_tx(msg)

    logger.info("No R1KH-ID subelem in FTIE")
    r0khid = binascii.hexlify(req['payload'][145:145+10]).decode()
    msg['payload'] = binascii.unhexlify("0602" + addrs + "00003603a1b201375e0000" + "00000000000000000000000000000000" + "0000000000000000000000000000000000000000000000000000000000000000" + snonce + "030a" + r0khid)
    hapd1ap.mgmt_tx(msg)

    logger.info("No RSNE")
    r0khid = binascii.hexlify(req['payload'][145:145+10]).decode()
    msg['payload'] = binascii.unhexlify("0602" + addrs + "00003603a1b20137660000" + "00000000000000000000000000000000" + "0000000000000000000000000000000000000000000000000000000000000000" + snonce + "030a" + r0khid + "0106000102030405")
    hapd1ap.mgmt_tx(msg)

def test_ap_ft_pmf_over_ds(dev, apdev):
    """WPA2-PSK-FT AP over DS with PMF"""
    run_ap_ft_pmf_bip_over_ds(dev, apdev, None)

def test_ap_ft_pmf_bip_cmac_128_over_ds(dev, apdev):
    """WPA2-PSK-FT AP over DS with PMF/BIP-CMAC-128"""
    run_ap_ft_pmf_bip_over_ds(dev, apdev, "AES-128-CMAC")

def test_ap_ft_pmf_bip_gmac_128_over_ds(dev, apdev):
    """WPA2-PSK-FT AP over DS with PMF/BIP-GMAC-128"""
    run_ap_ft_pmf_bip_over_ds(dev, apdev, "BIP-GMAC-128")

def test_ap_ft_pmf_bip_gmac_256_over_ds(dev, apdev):
    """WPA2-PSK-FT AP over DS with PMF/BIP-GMAC-256"""
    run_ap_ft_pmf_bip_over_ds(dev, apdev, "BIP-GMAC-256")

def test_ap_ft_pmf_bip_cmac_256_over_ds(dev, apdev):
    """WPA2-PSK-FT AP over DS with PMF/BIP-CMAC-256"""
    run_ap_ft_pmf_bip_over_ds(dev, apdev, "BIP-CMAC-256")

def run_ap_ft_pmf_bip_over_ds(dev, apdev, cipher):
    if cipher and cipher not in dev[0].get_capability("group_mgmt"):
        raise HwsimSkip("Cipher %s not supported" % cipher)

    ssid = "test-ft"
    passphrase = "12345678"

    params = ft_params1(ssid=ssid, passphrase=passphrase)
    params["ieee80211w"] = "2"
    if cipher:
        params["group_mgmt_cipher"] = cipher
    hapd0 = hostapd.add_ap(apdev[0], params)
    params = ft_params2(ssid=ssid, passphrase=passphrase)
    params["ieee80211w"] = "2"
    if cipher:
        params["group_mgmt_cipher"] = cipher
    hapd1 = hostapd.add_ap(apdev[1], params)

    Wlantest.setup(hapd0)
    wt = Wlantest()
    wt.flush()
    wt.add_passphrase(passphrase)

    run_roams(dev[0], apdev, hapd0, hapd1, ssid, passphrase, over_ds=True,
              group_mgmt=cipher)

def test_ap_ft_over_ds_pull(dev, apdev):
    """WPA2-PSK-FT AP over DS (pull PMK)"""
    ssid = "test-ft"
    passphrase = "12345678"

    params = ft_params1(ssid=ssid, passphrase=passphrase)
    params["pmk_r1_push"] = "0"
    hapd0 = hostapd.add_ap(apdev[0], params)
    params = ft_params2(ssid=ssid, passphrase=passphrase)
    params["pmk_r1_push"] = "0"
    hapd1 = hostapd.add_ap(apdev[1], params)

    run_roams(dev[0], apdev, hapd0, hapd1, ssid, passphrase, over_ds=True)

def test_ap_ft_over_ds_pull_old_key(dev, apdev):
    """WPA2-PSK-FT AP over DS (pull PMK; old key)"""
    ssid = "test-ft"
    passphrase = "12345678"

    params = ft_params1_old_key(ssid=ssid, passphrase=passphrase)
    params["pmk_r1_push"] = "0"
    hapd0 = hostapd.add_ap(apdev[0], params)
    params = ft_params2_old_key(ssid=ssid, passphrase=passphrase)
    params["pmk_r1_push"] = "0"
    hapd1 = hostapd.add_ap(apdev[1], params)

    run_roams(dev[0], apdev, hapd0, hapd1, ssid, passphrase, over_ds=True)

def test_ap_ft_over_ds_pull_vlan(dev, apdev):
    """WPA2-PSK-FT AP over DS (pull PMK) with VLAN"""
    ssid = "test-ft"
    passphrase = "12345678"
    filename = hostapd.acl_file(dev, apdev, 'hostapd.accept')
    hostapd.send_file(apdev[0], filename, filename)
    hostapd.send_file(apdev[1], filename, filename)

    params = ft_params1(ssid=ssid, passphrase=passphrase)
    params["pmk_r1_push"] = "0"
    params['dynamic_vlan'] = "1"
    params['accept_mac_file'] = filename
    hapd0 = hostapd.add_ap(apdev[0]['ifname'], params)
    params = ft_params2(ssid=ssid, passphrase=passphrase)
    params["pmk_r1_push"] = "0"
    params['dynamic_vlan'] = "1"
    params['accept_mac_file'] = filename
    hapd1 = hostapd.add_ap(apdev[1]['ifname'], params)

    run_roams(dev[0], apdev, hapd0, hapd1, ssid, passphrase, over_ds=True,
              conndev="brvlan1")
    if filename.startswith('/tmp/'):
        os.unlink(filename)

def start_ft_sae(dev, apdev, wpa_ptk_rekey=None, sae_pwe=None,
                 rsne_override=None, rsnxe_override=None,
                 no_beacon_rsnxe2=False, ext_key_id=False,
                 skip_prune_assoc=False, ft_rsnxe_used=False,
                 sae_transition=False):
    if "SAE" not in dev.get_capability("auth_alg"):
        raise HwsimSkip("SAE not supported")
    ssid = "test-ft"
    passphrase = "12345678"

    params = ft_params1(ssid=ssid, passphrase=passphrase)
    params['wpa_key_mgmt'] = "FT-SAE"
    if wpa_ptk_rekey:
        params['wpa_ptk_rekey'] = str(wpa_ptk_rekey)
    if sae_pwe is not None:
        params['sae_pwe'] = sae_pwe
    if rsne_override:
        params['rsne_override_ft'] = rsne_override
    if rsnxe_override:
        params['rsnxe_override_ft'] = rsnxe_override
    if ext_key_id:
        params['extended_key_id'] = '1'
    if skip_prune_assoc:
        params['skip_prune_assoc'] = '1'
    if ft_rsnxe_used:
        params['ft_rsnxe_used'] = '1'
    hapd0 = hostapd.add_ap(apdev[0], params)
    params = ft_params2(ssid=ssid, passphrase=passphrase)
    if not sae_transition:
        params['wpa_key_mgmt'] = "FT-SAE"
    if wpa_ptk_rekey:
        params['wpa_ptk_rekey'] = str(wpa_ptk_rekey)
    if sae_pwe is not None:
        params['sae_pwe'] = sae_pwe
    if rsne_override:
        params['rsne_override_ft'] = rsne_override
    if rsnxe_override:
        params['rsnxe_override_ft'] = rsnxe_override
    if no_beacon_rsnxe2:
        params['no_beacon_rsnxe'] = "1"
    if ext_key_id:
        params['extended_key_id'] = '1'
    if skip_prune_assoc:
        params['skip_prune_assoc'] = '1'
    if ft_rsnxe_used:
        params['ft_rsnxe_used'] = '1'
    hapd1 = hostapd.add_ap(apdev[1], params)
    key_mgmt = hapd1.get_config()['key_mgmt']
    if key_mgmt.split(' ')[0] != "FT-SAE" and not sae_transition:
        raise Exception("Unexpected GET_CONFIG(key_mgmt): " + key_mgmt)

    dev.request("SET sae_groups ")
    return hapd0, hapd1

def test_ap_ft_sae(dev, apdev):
    """WPA2-PSK-FT-SAE AP"""
    hapd0, hapd1 = start_ft_sae(dev[0], apdev)
    run_roams(dev[0], apdev, hapd0, hapd1, "test-ft", "12345678", sae=True)

def test_ap_ft_sae_transition(dev, apdev):
    """WPA2-PSK-FT-SAE/PSK AP"""
    hapd0, hapd1 = start_ft_sae(dev[0], apdev, sae_transition=True)
    run_roams(dev[0], apdev, hapd0, hapd1, "test-ft", "12345678",
              sae_transition=True)

def test_ap_ft_sae_h2e(dev, apdev):
    """WPA2-PSK-FT-SAE AP (H2E)"""
    try:
        dev[0].set("sae_pwe", "2")
        hapd0, hapd1 = start_ft_sae(dev[0], apdev, sae_pwe="2")
        run_roams(dev[0], apdev, hapd0, hapd1, "test-ft", "12345678", sae=True)
    finally:
        dev[0].set("sae_pwe", "0")

def test_ap_ft_sae_h2e_and_loop(dev, apdev):
    """WPA2-PSK-FT-SAE AP (AP H2E, STA loop)"""
    dev[0].set("sae_pwe", "0")
    hapd0, hapd1 = start_ft_sae(dev[0], apdev, sae_pwe="2")
    run_roams(dev[0], apdev, hapd0, hapd1, "test-ft", "12345678", sae=True)

def test_ap_ft_sae_h2e_and_loop2(dev, apdev):
    """WPA2-PSK-FT-SAE AP (AP loop, STA H2E)"""
    try:
        dev[0].set("sae_pwe", "2")
        hapd0, hapd1 = start_ft_sae(dev[0], apdev, sae_pwe="0")
        run_roams(dev[0], apdev, hapd0, hapd1, "test-ft", "12345678", sae=True)
    finally:
        dev[0].set("sae_pwe", "0")

def test_ap_ft_sae_h2e_downgrade_attack(dev, apdev):
    """WPA2-PSK-FT-SAE AP (H2E downgrade attack)"""
    try:
        dev[0].set("sae_pwe", "2")
        hapd0, hapd1 = start_ft_sae(dev[0], apdev, sae_pwe="2",
                                    no_beacon_rsnxe2=True)
        run_roams(dev[0], apdev, hapd0, hapd1, "test-ft", "12345678", sae=True,
                  force_initial_conn_to_first_ap=True,
                  return_after_initial=True)
        dev[0].scan_for_bss(hapd1.own_addr(), freq="2412")
        if "OK" not in dev[0].request("ROAM " + hapd1.own_addr()):
            raise Exception("ROAM command failed")
        # The target AP is expected to discard Reassociation Response frame due
        # to RSNXE Used mismatch. This will result in roaming timeout and
        # returning back to the old AP.
        ev = dev[0].wait_event(["CTRL-EVENT-ASSOC-REJECT",
                                "CTRL-EVENT-CONNECTED"], timeout=10)
        if ev and "CTRL-EVENT-ASSOC-REJECT" in ev:
            pass
        elif ev and hapd1.own_addr() in ev:
            raise Exception("Roaming succeeded unexpectedly")
    finally:
        dev[0].set("sae_pwe", "0")

def test_ap_ft_sae_ptk_rekey0(dev, apdev):
    """WPA2-PSK-FT-SAE AP and PTK rekey triggered by station"""
    hapd0, hapd1 = start_ft_sae(dev[0], apdev)
    run_roams(dev[0], apdev, hapd0, hapd1, "test-ft", "12345678", sae=True,
              ptk_rekey="1", roams=0)
    check_ptk_rekey(dev[0], hapd0, hapd1)

def test_ap_ft_sae_ptk_rekey1(dev, apdev):
    """WPA2-PSK-FT-SAE AP and PTK rekey triggered by station"""
    hapd0, hapd1 = start_ft_sae(dev[0], apdev)
    run_roams(dev[0], apdev, hapd0, hapd1, "test-ft", "12345678", sae=True,
              ptk_rekey="1", only_one_way=True)
    check_ptk_rekey(dev[0], hapd0, hapd1)

def test_ap_ft_sae_ptk_rekey_ap(dev, apdev):
    """WPA2-PSK-FT-SAE AP and PTK rekey triggered by AP"""
    hapd0, hapd1 = start_ft_sae(dev[0], apdev, wpa_ptk_rekey=2)
    run_roams(dev[0], apdev, hapd0, hapd1, "test-ft", "12345678", sae=True,
              only_one_way=True)
    check_ptk_rekey(dev[0], hapd0, hapd1)

def test_ap_ft_sae_ptk_rekey_ap_ext_key_id(dev, apdev):
    """WPA2-PSK-FT-SAE AP and PTK rekey triggered by AP (Ext Key ID)"""
    check_ext_key_id_capa(dev[0])
    try:
        dev[0].set("extended_key_id", "1")
        hapd0, hapd1 = start_ft_sae(dev[0], apdev, wpa_ptk_rekey=2,
                                    ext_key_id=True)
        check_ext_key_id_capa(hapd0)
        check_ext_key_id_capa(hapd1)
        run_roams(dev[0], apdev, hapd0, hapd1, "test-ft", "12345678", sae=True,
                  only_one_way=True)
        check_ptk_rekey(dev[0], hapd0, hapd1)
        idx = int(dev[0].request("GET last_tk_key_idx"))
        if idx != 1:
            raise Exception("Unexpected Key ID after TK rekey: %d" % idx)
    finally:
        dev[0].set("extended_key_id", "0")

def test_ap_ft_sae_over_ds(dev, apdev):
    """WPA2-PSK-FT-SAE AP over DS"""
    hapd0, hapd1 = start_ft_sae(dev[0], apdev)
    run_roams(dev[0], apdev, hapd0, hapd1, "test-ft", "12345678", sae=True,
              over_ds=True)

def test_ap_ft_sae_over_ds_ptk_rekey0(dev, apdev):
    """WPA2-PSK-FT-SAE AP over DS and PTK rekey triggered by station"""
    hapd0, hapd1 = start_ft_sae(dev[0], apdev)
    run_roams(dev[0], apdev, hapd0, hapd1, "test-ft", "12345678", sae=True,
              over_ds=True, ptk_rekey="1", roams=0)
    check_ptk_rekey(dev[0], hapd0, hapd1)

def test_ap_ft_sae_over_ds_ptk_rekey1(dev, apdev):
    """WPA2-PSK-FT-SAE AP over DS and PTK rekey triggered by station"""
    hapd0, hapd1 = start_ft_sae(dev[0], apdev)
    run_roams(dev[0], apdev, hapd0, hapd1, "test-ft", "12345678", sae=True,
              over_ds=True, ptk_rekey="1", only_one_way=True)
    check_ptk_rekey(dev[0], hapd0, hapd1)

def test_ap_ft_sae_over_ds_ptk_rekey_ap(dev, apdev):
    """WPA2-PSK-FT-SAE AP over DS and PTK rekey triggered by AP"""
    hapd0, hapd1 = start_ft_sae(dev[0], apdev, wpa_ptk_rekey=2)
    run_roams(dev[0], apdev, hapd0, hapd1, "test-ft", "12345678", sae=True,
              over_ds=True, only_one_way=True)
    check_ptk_rekey(dev[0], hapd0, hapd1)

def test_ap_ft_sae_h2e_rsne_override(dev, apdev):
    """WPA2-PSK-FT-SAE AP (H2E) and RSNE override (same value)"""
    try:
        dev[0].set("sae_pwe", "2")
        hapd0, hapd1 = start_ft_sae(dev[0], apdev, sae_pwe="2",
                                    rsne_override="30260100000fac040100000fac040100000fac090c000100" + 16*"ff")
        run_roams(dev[0], apdev, hapd0, hapd1, "test-ft", "12345678", sae=True)
    finally:
        dev[0].set("sae_pwe", "0")

def test_ap_ft_sae_h2e_rsnxe_override(dev, apdev):
    """WPA2-PSK-FT-SAE AP (H2E) and RSNXE override (same value)"""
    try:
        dev[0].set("sae_pwe", "2")
        hapd0, hapd1 = start_ft_sae(dev[0], apdev, sae_pwe="2",
                                    rsnxe_override="F40120")
        run_roams(dev[0], apdev, hapd0, hapd1, "test-ft", "12345678", sae=True)
    finally:
        dev[0].set("sae_pwe", "0")

def test_ap_ft_sae_h2e_rsne_mismatch(dev, apdev):
    """WPA2-PSK-FT-SAE AP (H2E) and RSNE mismatch"""
    try:
        dev[0].set("sae_pwe", "2")
        hapd0, hapd1 = start_ft_sae(dev[0], apdev, sae_pwe="2",
                                    rsne_override="30260100000fac040100000fac040100000fac090c010100" + 16*"ff")
        run_roams(dev[0], apdev, hapd0, hapd1, "test-ft", "12345678", sae=True,
                  fail_test=True)
    finally:
        dev[0].set("sae_pwe", "0")

def test_ap_ft_sae_h2e_rsne_mismatch_pmkr1name(dev, apdev):
    """WPA2-PSK-FT-SAE AP (H2E) and RSNE mismatch in PMKR1Name"""
    try:
        dev[0].set("sae_pwe", "2")
        hapd0, hapd1 = start_ft_sae(dev[0], apdev, sae_pwe="2",
                                    rsne_override="30260100000fac040100000fac040100000fac090c000100" + 16*"00")
        run_roams(dev[0], apdev, hapd0, hapd1, "test-ft", "12345678", sae=True,
                  fail_test=True)
    finally:
        dev[0].set("sae_pwe", "0")

def test_ap_ft_sae_h2e_rsnxe_mismatch(dev, apdev):
    """WPA2-PSK-FT-SAE AP (H2E) and RSNXE mismatch"""
    try:
        dev[0].set("sae_pwe", "2")
        hapd0, hapd1 = start_ft_sae(dev[0], apdev, sae_pwe="2",
                                    rsnxe_override="F40160")
        run_roams(dev[0], apdev, hapd0, hapd1, "test-ft", "12345678", sae=True,
                  fail_test=True)
    finally:
        dev[0].set("sae_pwe", "0")

def test_ap_ft_sae_rsnxe_used_mismatch(dev, apdev):
    """FT-SAE AP and unexpected RSNXE Used in ReassocReq"""
    try:
        hapd0, hapd1 = start_ft_sae(dev[0], apdev, sae_pwe="2")
        dev[0].set("sae_pwe", "0")
        dev[0].set("ft_rsnxe_used", "1")
        next = run_roams(dev[0], apdev, hapd0, hapd1, "test-ft", "12345678",
                         sae=True, return_after_initial=True)
        if "OK" not in dev[0].request("ROAM " + next):
            raise Exception("ROAM command failed")
        # The target AP is expected to discard Reassociation Request frame due
        # to RSNXE Used mismatch. This will result in roaming timeout and
        # returning back to the old AP.
        ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED"], timeout=5)
        if ev and next in ev:
            raise Exception("Roaming succeeded unexpectedly")
    finally:
        dev[0].set("sae_pwe", "0")

def test_ap_ft_sae_rsnxe_used_mismatch2(dev, apdev):
    """FT-SAE AP and unexpected RSNXE Used in ReassocResp"""
    try:
        hapd0, hapd1 = start_ft_sae(dev[0], apdev, sae_pwe="0",
                                    ft_rsnxe_used=True)
        dev[0].set("sae_pwe", "2")
        next = run_roams(dev[0], apdev, hapd0, hapd1, "test-ft", "12345678",
                         sae=True, return_after_initial=True)
        if "OK" not in dev[0].request("ROAM " + next):
            raise Exception("ROAM command failed")
        # The STA is expected to discard Reassociation Response frame due to
        # RSNXE Used mismatch. This will result in returning back to the old AP.
        ev = dev[0].wait_disconnected()
        if next not in ev:
            raise Exception("Unexpected disconnection BSSID: " + ev)
        if "reason=13 locally_generated=1" not in ev:
            raise Exception("Unexpected disconnection reason: " + ev)
        ev = dev[0].wait_connected()
        if next in ev:
            raise Exception("Roaming succeeded unexpectedly")

        hapd0.set("ft_rsnxe_used", "0")
        hapd1.set("ft_rsnxe_used", "0")
        dev[0].roam(next);
    finally:
        dev[0].set("sae_pwe", "0")

def test_ap_ft_sae_pw_id(dev, apdev):
    """FT-SAE with Password Identifier"""
    if "SAE" not in dev[0].get_capability("auth_alg"):
        raise HwsimSkip("SAE not supported")
    ssid = "test-ft"

    params = ft_params1(ssid=ssid)
    params["ieee80211w"] = "2"
    params['wpa_key_mgmt'] = "FT-SAE"
    params['sae_password'] = 'secret|id=pwid'
    hapd0 = hostapd.add_ap(apdev[0], params)
    params = ft_params2(ssid=ssid)
    params["ieee80211w"] = "2"
    params['wpa_key_mgmt'] = "FT-SAE"
    params['sae_password'] = 'secret|id=pwid'
    hapd = hostapd.add_ap(apdev[1], params)

    dev[0].request("SET sae_groups ")
    run_roams(dev[0], apdev, hapd0, hapd, ssid, passphrase=None, sae=True,
              sae_password="secret", sae_password_id="pwid")

def test_ap_ft_sae_with_both_akms(dev, apdev):
    """SAE + FT-SAE configuration"""
    if "SAE" not in dev[0].get_capability("auth_alg"):
        raise HwsimSkip("SAE not supported")
    ssid = "test-ft"
    passphrase = "12345678"

    params = ft_params1(ssid=ssid, passphrase=passphrase)
    params['wpa_key_mgmt'] = "FT-SAE SAE"
    hapd0 = hostapd.add_ap(apdev[0], params)
    params = ft_params2(ssid=ssid, passphrase=passphrase)
    params['wpa_key_mgmt'] = "FT-SAE SAE"
    hapd = hostapd.add_ap(apdev[1], params)
    key_mgmt = hapd.get_config()['key_mgmt']
    if key_mgmt.split(' ')[0] != "FT-SAE":
        raise Exception("Unexpected GET_CONFIG(key_mgmt): " + key_mgmt)

    dev[0].request("SET sae_groups ")
    run_roams(dev[0], apdev, hapd0, hapd, ssid, passphrase, sae=True,
              sae_and_psk=True)

def test_ap_ft_sae_pmksa_caching(dev, apdev):
    """WPA2-FT-SAE AP and PMKSA caching for initial mobility domain association"""
    if "SAE" not in dev[0].get_capability("auth_alg"):
        raise HwsimSkip("SAE not supported")
    ssid = "test-ft"
    passphrase = "12345678"

    params = ft_params1(ssid=ssid, passphrase=passphrase)
    params['wpa_key_mgmt'] = "FT-SAE"
    hapd0 = hostapd.add_ap(apdev[0], params)
    params = ft_params2(ssid=ssid, passphrase=passphrase)
    params['wpa_key_mgmt'] = "FT-SAE"
    hapd = hostapd.add_ap(apdev[1], params)
    key_mgmt = hapd.get_config()['key_mgmt']
    if key_mgmt.split(' ')[0] != "FT-SAE":
        raise Exception("Unexpected GET_CONFIG(key_mgmt): " + key_mgmt)

    dev[0].request("SET sae_groups ")
    run_roams(dev[0], apdev, hapd0, hapd, ssid, passphrase, sae=True,
              pmksa_caching=True)

def test_ap_ft_sae_pmksa_caching_pwe(dev, apdev):
    """WPA2-FT-SAE AP and PMKSA caching for initial mobility domain association (STA PWE both)"""
    if "SAE" not in dev[0].get_capability("auth_alg"):
        raise HwsimSkip("SAE not supported")
    ssid = "test-ft"
    passphrase = "12345678"

    params = ft_params1(ssid=ssid, passphrase=passphrase)
    params['wpa_key_mgmt'] = "FT-SAE"
    hapd0 = hostapd.add_ap(apdev[0], params)
    params = ft_params2(ssid=ssid, passphrase=passphrase)
    params['wpa_key_mgmt'] = "FT-SAE"
    hapd = hostapd.add_ap(apdev[1], params)
    key_mgmt = hapd.get_config()['key_mgmt']
    if key_mgmt.split(' ')[0] != "FT-SAE":
        raise Exception("Unexpected GET_CONFIG(key_mgmt): " + key_mgmt)

    try:
        dev[0].request("SET sae_groups ")
        dev[0].set("sae_pwe", "2")
        run_roams(dev[0], apdev, hapd0, hapd, ssid, passphrase, sae=True,
                  pmksa_caching=True)
    finally:
        dev[0].set("sae_groups", "")
        dev[0].set("sae_pwe", "0")

def test_ap_ft_sae_pmksa_caching_h2e(dev, apdev):
    """WPA2-FT-SAE AP and PMKSA caching for initial mobility domain association (H2E)"""
    if "SAE" not in dev[0].get_capability("auth_alg"):
        raise HwsimSkip("SAE not supported")
    ssid = "test-ft"
    passphrase = "12345678"

    params = ft_params1(ssid=ssid, passphrase=passphrase)
    params['wpa_key_mgmt'] = "FT-SAE"
    params['sae_pwe'] = "1"
    hapd0 = hostapd.add_ap(apdev[0], params)
    params = ft_params2(ssid=ssid, passphrase=passphrase)
    params['wpa_key_mgmt'] = "FT-SAE"
    params['sae_pwe'] = "1"
    hapd = hostapd.add_ap(apdev[1], params)
    key_mgmt = hapd.get_config()['key_mgmt']
    if key_mgmt.split(' ')[0] != "FT-SAE":
        raise Exception("Unexpected GET_CONFIG(key_mgmt): " + key_mgmt)

    try:
        dev[0].request("SET sae_groups ")
        dev[0].set("sae_pwe", "1")
        run_roams(dev[0], apdev, hapd0, hapd, ssid, passphrase, sae=True,
                  pmksa_caching=True)
    finally:
        dev[0].set("sae_groups", "")
        dev[0].set("sae_pwe", "0")

def generic_ap_ft_eap(dev, apdev, vlan=False, cui=False, over_ds=False,
                      discovery=False, roams=1, wpa_ptk_rekey=0,
                      only_one_way=False):
    ssid = "test-ft"
    passphrase = "12345678"
    if vlan:
        identity = "gpsk-vlan1"
        conndev = "brvlan1"
    elif cui:
        identity = "gpsk-cui"
        conndev = False
    else:
        identity = "gpsk user"
        conndev = False

    radius = hostapd.radius_params()
    params = ft_params1(ssid=ssid, passphrase=passphrase, discovery=discovery)
    params['wpa_key_mgmt'] = "FT-EAP"
    params["ieee8021x"] = "1"
    if vlan:
        params["dynamic_vlan"] = "1"
    params = dict(list(radius.items()) + list(params.items()))
    hapd = hostapd.add_ap(apdev[0], params)
    key_mgmt = hapd.get_config()['key_mgmt']
    if key_mgmt.split(' ')[0] != "FT-EAP":
        raise Exception("Unexpected GET_CONFIG(key_mgmt): " + key_mgmt)
    params = ft_params2(ssid=ssid, passphrase=passphrase, discovery=discovery)
    params['wpa_key_mgmt'] = "FT-EAP"
    params["ieee8021x"] = "1"
    if vlan:
        params["dynamic_vlan"] = "1"
    if wpa_ptk_rekey:
        params["wpa_ptk_rekey"] = str(wpa_ptk_rekey)
    params = dict(list(radius.items()) + list(params.items()))
    hapd1 = hostapd.add_ap(apdev[1], params)

    run_roams(dev[0], apdev, hapd, hapd1, ssid, passphrase, eap=True,
              over_ds=over_ds, roams=roams, eap_identity=identity,
              conndev=conndev, only_one_way=only_one_way)
    if "[WPA2-FT/EAP-CCMP]" not in dev[0].request("SCAN_RESULTS"):
        raise Exception("Scan results missing RSN element info")
    check_mib(dev[0], [("dot11RSNAAuthenticationSuiteRequested", "00-0f-ac-3"),
                       ("dot11RSNAAuthenticationSuiteSelected", "00-0f-ac-3")])
    if only_one_way:
        return

    # Verify EAPOL reauthentication after FT protocol
    if dev[0].get_status_field('bssid') == apdev[0]['bssid']:
        ap = hapd
    else:
        ap = hapd1
    ap.request("EAPOL_REAUTH " + dev[0].own_addr())
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED"], timeout=5)
    if ev is None:
        raise Exception("EAP authentication did not start")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-SUCCESS"], timeout=5)
    if ev is None:
        raise Exception("EAP authentication did not succeed")
    time.sleep(0.1)
    if conndev:
        hwsim_utils.test_connectivity_iface(dev[0], ap, conndev)
    else:
        hwsim_utils.test_connectivity(dev[0], ap)

def test_ap_ft_eap(dev, apdev):
    """WPA2-EAP-FT AP"""
    generic_ap_ft_eap(dev, apdev)

def test_ap_ft_eap_cui(dev, apdev):
    """WPA2-EAP-FT AP with CUI"""
    generic_ap_ft_eap(dev, apdev, vlan=False, cui=True)

def test_ap_ft_eap_vlan(dev, apdev):
    """WPA2-EAP-FT AP with VLAN"""
    generic_ap_ft_eap(dev, apdev, vlan=True)

def test_ap_ft_eap_vlan_multi(dev, apdev):
    """WPA2-EAP-FT AP with VLAN"""
    generic_ap_ft_eap(dev, apdev, vlan=True, roams=50)

def test_ap_ft_eap_over_ds(dev, apdev):
    """WPA2-EAP-FT AP using over-the-DS"""
    generic_ap_ft_eap(dev, apdev, over_ds=True)

def test_ap_ft_eap_dis(dev, apdev):
    """WPA2-EAP-FT AP with AP discovery"""
    generic_ap_ft_eap(dev, apdev, discovery=True)

def test_ap_ft_eap_dis_over_ds(dev, apdev):
    """WPA2-EAP-FT AP with AP discovery and over-the-DS"""
    generic_ap_ft_eap(dev, apdev, over_ds=True, discovery=True)

def test_ap_ft_eap_vlan(dev, apdev):
    """WPA2-EAP-FT AP with VLAN"""
    generic_ap_ft_eap(dev, apdev, vlan=True)

def test_ap_ft_eap_vlan_multi(dev, apdev):
    """WPA2-EAP-FT AP with VLAN"""
    generic_ap_ft_eap(dev, apdev, vlan=True, roams=50)

def test_ap_ft_eap_vlan_over_ds(dev, apdev):
    """WPA2-EAP-FT AP with VLAN + over_ds"""
    generic_ap_ft_eap(dev, apdev, vlan=True, over_ds=True)

def test_ap_ft_eap_vlan_over_ds_multi(dev, apdev):
    """WPA2-EAP-FT AP with VLAN + over_ds"""
    generic_ap_ft_eap(dev, apdev, vlan=True, over_ds=True, roams=50)

def generic_ap_ft_eap_pull(dev, apdev, vlan=False):
    """WPA2-EAP-FT AP (pull PMK)"""
    ssid = "test-ft"
    passphrase = "12345678"
    if vlan:
        identity = "gpsk-vlan1"
        conndev = "brvlan1"
    else:
        identity = "gpsk user"
        conndev = False

    radius = hostapd.radius_params()
    params = ft_params1(ssid=ssid, passphrase=passphrase)
    params['wpa_key_mgmt'] = "FT-EAP"
    params["ieee8021x"] = "1"
    params["pmk_r1_push"] = "0"
    if vlan:
        params["dynamic_vlan"] = "1"
    params = dict(list(radius.items()) + list(params.items()))
    hapd = hostapd.add_ap(apdev[0], params)
    key_mgmt = hapd.get_config()['key_mgmt']
    if key_mgmt.split(' ')[0] != "FT-EAP":
        raise Exception("Unexpected GET_CONFIG(key_mgmt): " + key_mgmt)
    params = ft_params2(ssid=ssid, passphrase=passphrase)
    params['wpa_key_mgmt'] = "FT-EAP"
    params["ieee8021x"] = "1"
    params["pmk_r1_push"] = "0"
    if vlan:
        params["dynamic_vlan"] = "1"
    params = dict(list(radius.items()) + list(params.items()))
    hapd1 = hostapd.add_ap(apdev[1], params)

    run_roams(dev[0], apdev, hapd, hapd1, ssid, passphrase, eap=True,
              eap_identity=identity, conndev=conndev)

def test_ap_ft_eap_pull(dev, apdev):
    """WPA2-EAP-FT AP (pull PMK)"""
    generic_ap_ft_eap_pull(dev, apdev)

def test_ap_ft_eap_pull_vlan(dev, apdev):
    """WPA2-EAP-FT AP (pull PMK) - with VLAN"""
    generic_ap_ft_eap_pull(dev, apdev, vlan=True)

def test_ap_ft_eap_pull_wildcard(dev, apdev):
    """WPA2-EAP-FT AP (pull PMK) - wildcard R0KH/R1KH"""
    ssid = "test-ft"
    passphrase = "12345678"

    radius = hostapd.radius_params()
    params = ft_params1(ssid=ssid, passphrase=passphrase, discovery=True)
    params['wpa_key_mgmt'] = "WPA-EAP FT-EAP"
    params["ieee8021x"] = "1"
    params["pmk_r1_push"] = "0"
    params["r0kh"] = "ff:ff:ff:ff:ff:ff * 00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff"
    params["r1kh"] = "00:00:00:00:00:00 00:00:00:00:00:00 00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff"
    params["ft_psk_generate_local"] = "1"
    params["eap_server"] = "0"
    params["rkh_pos_timeout"] = "100"
    params["rkh_neg_timeout"] = "50"
    params["rkh_pull_timeout"] = "1234"
    params["rkh_pull_retries"] = "10"
    params = dict(list(radius.items()) + list(params.items()))
    hapd = hostapd.add_ap(apdev[0], params)
    params = ft_params2(ssid=ssid, passphrase=passphrase, discovery=True)
    params['wpa_key_mgmt'] = "WPA-EAP FT-EAP"
    params["ieee8021x"] = "1"
    params["pmk_r1_push"] = "0"
    params["r0kh"] = "ff:ff:ff:ff:ff:ff * 00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff"
    params["r1kh"] = "00:00:00:00:00:00 00:00:00:00:00:00 00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff"
    params["ft_psk_generate_local"] = "1"
    params["eap_server"] = "0"
    params = dict(list(radius.items()) + list(params.items()))
    hapd1 = hostapd.add_ap(apdev[1], params)

    run_roams(dev[0], apdev, hapd, hapd1, ssid, passphrase, eap=True)

def test_ap_ft_eap_pull_wildcard_multi_bss(dev, apdev, params):
    """WPA2-EAP-FT AP (pull PMK) - wildcard R0KH/R1KH with multiple BSSs"""
    bssconf = os.path.join(params['logdir'],
                           'ap_ft_eap_pull_wildcard_multi_bss.bss.conf')
    ssid = "test-ft"
    passphrase = "12345678"
    radius = hostapd.radius_params()

    params = ft_params1(ssid=ssid, passphrase=passphrase, discovery=True)
    params['wpa_key_mgmt'] = "WPA-EAP FT-EAP"
    params["ieee8021x"] = "1"
    params["pmk_r1_push"] = "0"
    params["r0kh"] = "ff:ff:ff:ff:ff:ff * 00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff"
    params["r1kh"] = "00:00:00:00:00:00 00:00:00:00:00:00 00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff"
    params["eap_server"] = "0"
    params = dict(list(radius.items()) + list(params.items()))
    hapd = hostapd.add_ap(apdev[0], params)
    ifname2 = apdev[0]['ifname'] + "-2"
    bssid2 = "02:00:00:00:03:01"
    params['nas_identifier'] = "nas1b.w1.fi"
    params['r1_key_holder'] = "000102030415"
    with open(bssconf, 'w') as f:
        f.write("driver=nl80211\n")
        f.write("hw_mode=g\n")
        f.write("channel=1\n")
        f.write("ieee80211n=1\n")
        f.write("interface=%s\n" % ifname2)
        f.write("bssid=%s\n" % bssid2)
        f.write("ctrl_interface=/var/run/hostapd\n")

        fields = ["ssid", "wpa_passphrase", "nas_identifier", "wpa_key_mgmt",
                  "wpa", "rsn_pairwise", "auth_server_addr"]
        for name in fields:
            f.write("%s=%s\n" % (name, params[name]))
        for name, val in params.items():
            if name in fields:
                continue
            f.write("%s=%s\n" % (name, val))
    hapd2 = hostapd.add_bss(apdev[0], ifname2, bssconf)

    params = ft_params2(ssid=ssid, passphrase=passphrase, discovery=True)
    params['wpa_key_mgmt'] = "WPA-EAP FT-EAP"
    params["ieee8021x"] = "1"
    params["pmk_r1_push"] = "0"
    params["r0kh"] = "ff:ff:ff:ff:ff:ff * 00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff"
    params["r1kh"] = "00:00:00:00:00:00 00:00:00:00:00:00 00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff"
    params["eap_server"] = "0"
    params = dict(list(radius.items()) + list(params.items()))
    hapd1 = hostapd.add_ap(apdev[1], params)

    # The first iteration of the roaming test will use wildcard R0KH discovery
    # and RRB sequence number synchronization while the second iteration shows
    # the clean RRB exchange where those extra steps are not needed.
    for i in range(2):
        hapd.note("Test iteration %d" % i)
        dev[0].note("Test iteration %d" % i)

        id = dev[0].connect(ssid, key_mgmt="FT-EAP", eap="GPSK",
                            identity="gpsk user",
                            password="abcdefghijklmnop0123456789abcdef",
                            bssid=bssid2,
                            scan_freq="2412")
        res = dev[0].get_status_field("bssid")
        if res != bssid2:
            raise Exception("Unexpected BSSID after initial connection: " + res)

        dev[0].scan_for_bss(apdev[1]['bssid'], freq="2412")
        dev[0].set_network(id, "bssid", "00:00:00:00:00:00")
        dev[0].roam(apdev[1]['bssid'])
        res = dev[0].get_status_field("bssid")
        if res != apdev[1]['bssid']:
            raise Exception("Unexpected BSSID after first roam: " + res)

        dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
        dev[0].roam(apdev[0]['bssid'])
        res = dev[0].get_status_field("bssid")
        if res != apdev[0]['bssid']:
            raise Exception("Unexpected BSSID after second roam: " + res)

        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()
        dev[0].dump_monitor()
        hapd.dump_monitor()
        hapd2.dump_monitor()

@remote_compatible
def test_ap_ft_mismatching_rrb_key_push(dev, apdev):
    """WPA2-PSK-FT AP over DS with mismatching RRB key (push)"""
    ssid = "test-ft"
    passphrase = "12345678"

    params = ft_params1(ssid=ssid, passphrase=passphrase)
    params["ieee80211w"] = "2"
    hapd0 = hostapd.add_ap(apdev[0], params)
    params = ft_params2_incorrect_rrb_key(ssid=ssid, passphrase=passphrase)
    params["ieee80211w"] = "2"
    hapd1 = hostapd.add_ap(apdev[1], params)

    run_roams(dev[0], apdev, hapd0, hapd1, ssid, passphrase, over_ds=True,
              fail_test=True)

@remote_compatible
def test_ap_ft_mismatching_rrb_key_pull(dev, apdev):
    """WPA2-PSK-FT AP over DS with mismatching RRB key (pull)"""
    ssid = "test-ft"
    passphrase = "12345678"

    params = ft_params1(ssid=ssid, passphrase=passphrase)
    params["pmk_r1_push"] = "0"
    hapd0 = hostapd.add_ap(apdev[0], params)
    params = ft_params2_incorrect_rrb_key(ssid=ssid, passphrase=passphrase)
    params["pmk_r1_push"] = "0"
    hapd1 = hostapd.add_ap(apdev[1], params)

    run_roams(dev[0], apdev, hapd0, hapd1, ssid, passphrase, over_ds=True,
              fail_test=True)

@remote_compatible
def test_ap_ft_mismatching_r0kh_id_pull(dev, apdev):
    """WPA2-PSK-FT AP over DS with mismatching R0KH-ID (pull)"""
    ssid = "test-ft"
    passphrase = "12345678"

    params = ft_params1(ssid=ssid, passphrase=passphrase)
    params["pmk_r1_push"] = "0"
    params["nas_identifier"] = "nas0.w1.fi"
    hostapd.add_ap(apdev[0], params)
    dev[0].connect(ssid, psk=passphrase, key_mgmt="FT-PSK", proto="WPA2",
                   scan_freq="2412")

    params = ft_params2(ssid=ssid, passphrase=passphrase)
    params["pmk_r1_push"] = "0"
    hostapd.add_ap(apdev[1], params)

    dev[0].scan_for_bss(apdev[1]['bssid'], freq="2412")
    dev[0].roam_over_ds(apdev[1]['bssid'], fail_test=True)

@remote_compatible
def test_ap_ft_mismatching_rrb_r0kh_push(dev, apdev):
    """WPA2-PSK-FT AP over DS with mismatching R0KH key (push)"""
    ssid = "test-ft"
    passphrase = "12345678"

    params = ft_params1(ssid=ssid, passphrase=passphrase)
    params["ieee80211w"] = "2"
    hapd0 = hostapd.add_ap(apdev[0], params)
    params = ft_params2_r0kh_mismatch(ssid=ssid, passphrase=passphrase)
    params["ieee80211w"] = "2"
    hapd1 = hostapd.add_ap(apdev[1], params)

    run_roams(dev[0], apdev, hapd0, hapd1, ssid, passphrase, over_ds=True,
              fail_test=True)

@remote_compatible
def test_ap_ft_mismatching_rrb_r0kh_pull(dev, apdev):
    """WPA2-PSK-FT AP over DS with mismatching R0KH key (pull)"""
    ssid = "test-ft"
    passphrase = "12345678"

    params = ft_params1_r0kh_mismatch(ssid=ssid, passphrase=passphrase)
    params["pmk_r1_push"] = "0"
    hapd0 = hostapd.add_ap(apdev[0], params)
    params = ft_params2(ssid=ssid, passphrase=passphrase)
    params["pmk_r1_push"] = "0"
    hapd1 = hostapd.add_ap(apdev[1], params)

    run_roams(dev[0], apdev, hapd0, hapd1, ssid, passphrase, over_ds=True,
              fail_test=True)

def test_ap_ft_mismatching_rrb_key_push_eap(dev, apdev):
    """WPA2-EAP-FT AP over DS with mismatching RRB key (push)"""
    ssid = "test-ft"
    passphrase = "12345678"

    radius = hostapd.radius_params()
    params = ft_params1(ssid=ssid, passphrase=passphrase)
    params["ieee80211w"] = "2"
    params['wpa_key_mgmt'] = "FT-EAP"
    params["ieee8021x"] = "1"
    params = dict(list(radius.items()) + list(params.items()))
    hapd0 = hostapd.add_ap(apdev[0], params)
    params = ft_params2_incorrect_rrb_key(ssid=ssid, passphrase=passphrase)
    params["ieee80211w"] = "2"
    params['wpa_key_mgmt'] = "FT-EAP"
    params["ieee8021x"] = "1"
    params = dict(list(radius.items()) + list(params.items()))
    hapd1 = hostapd.add_ap(apdev[1], params)

    run_roams(dev[0], apdev, hapd0, hapd1, ssid, passphrase, over_ds=True,
              fail_test=True, eap=True)

def test_ap_ft_mismatching_rrb_key_pull_eap(dev, apdev):
    """WPA2-EAP-FT AP over DS with mismatching RRB key (pull)"""
    ssid = "test-ft"
    passphrase = "12345678"

    radius = hostapd.radius_params()
    params = ft_params1(ssid=ssid, passphrase=passphrase)
    params["pmk_r1_push"] = "0"
    params['wpa_key_mgmt'] = "FT-EAP"
    params["ieee8021x"] = "1"
    params = dict(list(radius.items()) + list(params.items()))
    hapd0 = hostapd.add_ap(apdev[0], params)
    params = ft_params2_incorrect_rrb_key(ssid=ssid, passphrase=passphrase)
    params["pmk_r1_push"] = "0"
    params['wpa_key_mgmt'] = "FT-EAP"
    params["ieee8021x"] = "1"
    params = dict(list(radius.items()) + list(params.items()))
    hapd1 = hostapd.add_ap(apdev[1], params)

    run_roams(dev[0], apdev, hapd0, hapd1, ssid, passphrase, over_ds=True,
              fail_test=True, eap=True)

def test_ap_ft_mismatching_r0kh_id_pull_eap(dev, apdev):
    """WPA2-EAP-FT AP over DS with mismatching R0KH-ID (pull)"""
    ssid = "test-ft"
    passphrase = "12345678"

    radius = hostapd.radius_params()
    params = ft_params1(ssid=ssid, passphrase=passphrase)
    params["pmk_r1_push"] = "0"
    params["nas_identifier"] = "nas0.w1.fi"
    params['wpa_key_mgmt'] = "FT-EAP"
    params["ieee8021x"] = "1"
    params = dict(list(radius.items()) + list(params.items()))
    hostapd.add_ap(apdev[0], params)
    dev[0].connect(ssid, key_mgmt="FT-EAP", proto="WPA2", ieee80211w="1",
                   eap="GPSK", identity="gpsk user",
                   password="abcdefghijklmnop0123456789abcdef",
                   scan_freq="2412")

    params = ft_params2(ssid=ssid, passphrase=passphrase)
    params["pmk_r1_push"] = "0"
    params['wpa_key_mgmt'] = "FT-EAP"
    params["ieee8021x"] = "1"
    params = dict(list(radius.items()) + list(params.items()))
    hostapd.add_ap(apdev[1], params)

    dev[0].scan_for_bss(apdev[1]['bssid'], freq="2412")
    dev[0].roam_over_ds(apdev[1]['bssid'], fail_test=True)

def test_ap_ft_mismatching_rrb_r0kh_push_eap(dev, apdev):
    """WPA2-EAP-FT AP over DS with mismatching R0KH key (push)"""
    ssid = "test-ft"
    passphrase = "12345678"

    radius = hostapd.radius_params()
    params = ft_params1(ssid=ssid, passphrase=passphrase)
    params["ieee80211w"] = "2"
    params['wpa_key_mgmt'] = "FT-EAP"
    params["ieee8021x"] = "1"
    params = dict(list(radius.items()) + list(params.items()))
    hapd0 = hostapd.add_ap(apdev[0], params)
    params = ft_params2_r0kh_mismatch(ssid=ssid, passphrase=passphrase)
    params["ieee80211w"] = "2"
    params['wpa_key_mgmt'] = "FT-EAP"
    params["ieee8021x"] = "1"
    params = dict(list(radius.items()) + list(params.items()))
    hapd1 = hostapd.add_ap(apdev[1], params)

    run_roams(dev[0], apdev, hapd0, hapd1, ssid, passphrase, over_ds=True,
              fail_test=True, eap=True)

def test_ap_ft_mismatching_rrb_r0kh_pull_eap(dev, apdev):
    """WPA2-EAP-FT AP over DS with mismatching R0KH key (pull)"""
    ssid = "test-ft"
    passphrase = "12345678"

    radius = hostapd.radius_params()
    params = ft_params1_r0kh_mismatch(ssid=ssid, passphrase=passphrase)
    params["pmk_r1_push"] = "0"
    params['wpa_key_mgmt'] = "FT-EAP"
    params["ieee8021x"] = "1"
    params = dict(list(radius.items()) + list(params.items()))
    hapd0 = hostapd.add_ap(apdev[0], params)
    params = ft_params2(ssid=ssid, passphrase=passphrase)
    params["pmk_r1_push"] = "0"
    params['wpa_key_mgmt'] = "FT-EAP"
    params["ieee8021x"] = "1"
    params = dict(list(radius.items()) + list(params.items()))
    hapd1 = hostapd.add_ap(apdev[1], params)

    run_roams(dev[0], apdev, hapd0, hapd1, ssid, passphrase, over_ds=True,
              fail_test=True, eap=True)

def test_ap_ft_gtk_rekey(dev, apdev):
    """WPA2-PSK-FT AP and GTK rekey"""
    ssid = "test-ft"
    passphrase = "12345678"

    params = ft_params1(ssid=ssid, passphrase=passphrase)
    params['wpa_group_rekey'] = '1'
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].connect(ssid, psk=passphrase, key_mgmt="FT-PSK", proto="WPA2",
                   ieee80211w="1", scan_freq="2412")

    ev = dev[0].wait_event(["WPA: Group rekeying completed"], timeout=2)
    if ev is None:
        raise Exception("GTK rekey timed out after initial association")
    hwsim_utils.test_connectivity(dev[0], hapd)

    params = ft_params2(ssid=ssid, passphrase=passphrase)
    params['wpa_group_rekey'] = '1'
    hapd1 = hostapd.add_ap(apdev[1], params)

    dev[0].scan_for_bss(apdev[1]['bssid'], freq="2412")
    dev[0].roam(apdev[1]['bssid'])
    if dev[0].get_status_field('bssid') != apdev[1]['bssid']:
        raise Exception("Did not connect to correct AP")
    hwsim_utils.test_connectivity(dev[0], hapd1)

    ev = dev[0].wait_event(["WPA: Group rekeying completed"], timeout=2)
    if ev is None:
        raise Exception("GTK rekey timed out after FT protocol")
    hwsim_utils.test_connectivity(dev[0], hapd1)

def test_ft_psk_key_lifetime_in_memory(dev, apdev, params):
    """WPA2-PSK-FT and key lifetime in memory"""
    ssid = "test-ft"
    passphrase = "04c2726b4b8d5f1b4db9c07aa4d9e9d8f765cb5d25ec817e6cc4fcdd5255db0"
    psk = '93c90846ff67af9037ed83fb72b63dbeddaa81d47f926c20909b5886f1d9358d'
    pmk = binascii.unhexlify(psk)
    p = ft_params1(ssid=ssid, passphrase=passphrase)
    hapd0 = hostapd.add_ap(apdev[0], p)
    p = ft_params2(ssid=ssid, passphrase=passphrase)
    hapd1 = hostapd.add_ap(apdev[1], p)

    pid = find_wpas_process(dev[0])

    dev[0].connect(ssid, psk=passphrase, key_mgmt="FT-PSK", proto="WPA2",
                   scan_freq="2412")
    # The decrypted copy of GTK is freed only after the CTRL-EVENT-CONNECTED
    # event has been delivered, so verify that wpa_supplicant has returned to
    # eloop before reading process memory.
    time.sleep(1)
    dev[0].ping()

    buf = read_process_memory(pid, pmk)

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    dev[0].relog()
    pmkr0 = None
    pmkr1 = None
    ptk = None
    gtk = None
    with open(os.path.join(params['logdir'], 'log0'), 'r') as f:
        for l in f.readlines():
            if "FT: PMK-R0 - hexdump" in l:
                val = l.strip().split(':')[3].replace(' ', '')
                pmkr0 = binascii.unhexlify(val)
            if "FT: PMK-R1 - hexdump" in l:
                val = l.strip().split(':')[3].replace(' ', '')
                pmkr1 = binascii.unhexlify(val)
            if "FT: KCK - hexdump" in l:
                val = l.strip().split(':')[3].replace(' ', '')
                kck = binascii.unhexlify(val)
            if "FT: KEK - hexdump" in l:
                val = l.strip().split(':')[3].replace(' ', '')
                kek = binascii.unhexlify(val)
            if "FT: TK - hexdump" in l:
                val = l.strip().split(':')[3].replace(' ', '')
                tk = binascii.unhexlify(val)
            if "WPA: Group Key - hexdump" in l:
                val = l.strip().split(':')[3].replace(' ', '')
                gtk = binascii.unhexlify(val)
    if not pmkr0 or not pmkr1 or not kck or not kek or not tk or not gtk:
        raise Exception("Could not find keys from debug log")
    if len(gtk) != 16:
        raise Exception("Unexpected GTK length")

    logger.info("Checking keys in memory while associated")
    get_key_locations(buf, pmk, "PMK")
    get_key_locations(buf, pmkr0, "PMK-R0")
    get_key_locations(buf, pmkr1, "PMK-R1")
    if pmk not in buf:
        raise HwsimSkip("PMK not found while associated")
    if pmkr0 not in buf:
        raise HwsimSkip("PMK-R0 not found while associated")
    if pmkr1 not in buf:
        raise HwsimSkip("PMK-R1 not found while associated")
    if kck not in buf:
        raise Exception("KCK not found while associated")
    if kek not in buf:
        raise Exception("KEK not found while associated")
    #if tk in buf:
    #    raise Exception("TK found from memory")

    logger.info("Checking keys in memory after disassociation")
    buf = read_process_memory(pid, pmk)
    get_key_locations(buf, pmk, "PMK")
    get_key_locations(buf, pmkr0, "PMK-R0")
    get_key_locations(buf, pmkr1, "PMK-R1")

    # Note: PMK/PSK is still present in network configuration

    fname = os.path.join(params['logdir'],
                         'ft_psk_key_lifetime_in_memory.memctx-')
    verify_not_present(buf, pmkr0, fname, "PMK-R0")
    verify_not_present(buf, pmkr1, fname, "PMK-R1")
    verify_not_present(buf, kck, fname, "KCK")
    verify_not_present(buf, kek, fname, "KEK")
    verify_not_present(buf, tk, fname, "TK")
    if gtk in buf:
        get_key_locations(buf, gtk, "GTK")
    verify_not_present(buf, gtk, fname, "GTK")

    dev[0].request("REMOVE_NETWORK all")

    logger.info("Checking keys in memory after network profile removal")
    buf = read_process_memory(pid, pmk)
    get_key_locations(buf, pmk, "PMK")
    get_key_locations(buf, pmkr0, "PMK-R0")
    get_key_locations(buf, pmkr1, "PMK-R1")

    verify_not_present(buf, pmk, fname, "PMK")
    verify_not_present(buf, pmkr0, fname, "PMK-R0")
    verify_not_present(buf, pmkr1, fname, "PMK-R1")
    verify_not_present(buf, kck, fname, "KCK")
    verify_not_present(buf, kek, fname, "KEK")
    verify_not_present(buf, tk, fname, "TK")
    verify_not_present(buf, gtk, fname, "GTK")

@remote_compatible
def test_ap_ft_invalid_resp(dev, apdev):
    """WPA2-PSK-FT AP and invalid response IEs"""
    ssid = "test-ft"
    passphrase = "12345678"

    params = ft_params1(ssid=ssid, passphrase=passphrase)
    hapd0 = hostapd.add_ap(apdev[0], params)
    dev[0].connect(ssid, psk=passphrase, key_mgmt="FT-PSK", proto="WPA2",
                   scan_freq="2412")

    params = ft_params2(ssid=ssid, passphrase=passphrase)
    hapd1 = hostapd.add_ap(apdev[1], params)

    tests = [
        # Various IEs for test coverage. The last one is FTIE with invalid
        # R1KH-ID subelement.
        "020002000000" + "3800" + "38051122334455" + "3754000102030405060708090001020304050607080900010203040506070809000102030405060708090001020304050607080900010203040506070809000102030405060708090001020304050607080900010100",
        # FTIE with invalid R0KH-ID subelement (len=0).
        "020002000000" + "3754000102030405060708090001020304050607080900010203040506070809000102030405060708090001020304050607080900010203040506070809000102030405060708090001020304050607080900010300",
        # FTIE with invalid R0KH-ID subelement (len=49).
        "020002000000" + "378500010203040506070809000102030405060708090001020304050607080900010203040506070809000102030405060708090001020304050607080900010203040506070809000102030405060708090001033101020304050607080910111213141516171819202122232425262728293031323334353637383940414243444546474849",
        # Invalid RSNE.
        "020002000000" + "3000",
        # Required IEs missing from protected IE count.
        "020002000000" + "3603a1b201" + "375200010203040506070809000102030405060708090001020304050607080900010203040506070809000102030405060708090001020304050607080900010203040506070809000102030405060708090001" + "3900",
        # RIC missing from protected IE count.
        "020002000000" + "3603a1b201" + "375200020203040506070809000102030405060708090001020304050607080900010203040506070809000102030405060708090001020304050607080900010203040506070809000102030405060708090001" + "3900",
        # Protected IE missing.
        "020002000000" + "3603a1b201" + "375200ff0203040506070809000102030405060708090001020304050607080900010203040506070809000102030405060708090001020304050607080900010203040506070809000102030405060708090001" + "3900" + "0000"]
    for t in tests:
        dev[0].scan_for_bss(apdev[1]['bssid'], freq="2412")
        hapd1.set("ext_mgmt_frame_handling", "1")
        hapd1.dump_monitor()
        if "OK" not in dev[0].request("ROAM " + apdev[1]['bssid']):
            raise Exception("ROAM failed")
        auth = None
        for i in range(20):
            msg = hapd1.mgmt_rx()
            if msg['subtype'] == 11:
                auth = msg
                break
        if not auth:
            raise Exception("Authentication frame not seen")

        resp = {}
        resp['fc'] = auth['fc']
        resp['da'] = auth['sa']
        resp['sa'] = auth['da']
        resp['bssid'] = auth['bssid']
        resp['payload'] = binascii.unhexlify(t)
        hapd1.mgmt_tx(resp)
        hapd1.set("ext_mgmt_frame_handling", "0")
        dev[0].wait_disconnected()

        dev[0].request("RECONNECT")
        dev[0].wait_connected()

def test_ap_ft_gcmp_256(dev, apdev):
    """WPA2-PSK-FT AP with GCMP-256 cipher"""
    if "GCMP-256" not in dev[0].get_capability("pairwise"):
        raise HwsimSkip("Cipher GCMP-256 not supported")
    ssid = "test-ft"
    passphrase = "12345678"

    params = ft_params1(ssid=ssid, passphrase=passphrase)
    params['rsn_pairwise'] = "GCMP-256"
    hapd0 = hostapd.add_ap(apdev[0], params)
    params = ft_params2(ssid=ssid, passphrase=passphrase)
    params['rsn_pairwise'] = "GCMP-256"
    hapd1 = hostapd.add_ap(apdev[1], params)

    run_roams(dev[0], apdev, hapd0, hapd1, ssid, passphrase,
              pairwise_cipher="GCMP-256", group_cipher="GCMP-256")

def setup_ap_ft_oom(dev, apdev):
    skip_with_fips(dev[0])
    ssid = "test-ft"
    passphrase = "12345678"

    params = ft_params1(ssid=ssid, passphrase=passphrase)
    hapd0 = hostapd.add_ap(apdev[0], params)
    params = ft_params2(ssid=ssid, passphrase=passphrase)
    hapd1 = hostapd.add_ap(apdev[1], params)

    dev[0].connect(ssid, psk=passphrase, key_mgmt="FT-PSK", proto="WPA2",
                   scan_freq="2412")
    if dev[0].get_status_field('bssid') == apdev[0]['bssid']:
        dst = apdev[1]['bssid']
    else:
        dst = apdev[0]['bssid']

    dev[0].scan_for_bss(dst, freq="2412")

    return dst

def test_ap_ft_oom(dev, apdev):
    """WPA2-PSK-FT and OOM"""
    dst = setup_ap_ft_oom(dev, apdev)
    with alloc_fail(dev[0], 1, "wpa_ft_gen_req_ies"):
        dev[0].roam(dst, check_bssid=False, fail_test=True)

def test_ap_ft_oom2(dev, apdev):
    """WPA2-PSK-FT and OOM (2)"""
    dst = setup_ap_ft_oom(dev, apdev)
    with fail_test(dev[0], 1, "wpa_ft_mic"):
        dev[0].roam(dst, fail_test=True, assoc_reject_ok=True)

def test_ap_ft_oom3(dev, apdev):
    """WPA2-PSK-FT and OOM (3)"""
    dst = setup_ap_ft_oom(dev, apdev)
    with fail_test(dev[0], 1, "os_get_random;wpa_ft_prepare_auth_request"):
        dev[0].roam(dst)

def test_ap_ft_oom4(dev, apdev):
    """WPA2-PSK-FT and OOM (4)"""
    ssid = "test-ft"
    passphrase = "12345678"
    dst = setup_ap_ft_oom(dev, apdev)
    dev[0].request("REMOVE_NETWORK all")
    with alloc_fail(dev[0], 1, "=sme_update_ft_ies"):
        dev[0].connect(ssid, psk=passphrase, key_mgmt="FT-PSK", proto="WPA2",
                       scan_freq="2412")

def test_ap_ft_ap_oom(dev, apdev):
    """WPA2-PSK-FT and AP OOM"""
    ssid = "test-ft"
    passphrase = "12345678"

    params = ft_params1(ssid=ssid, passphrase=passphrase)
    hapd0 = hostapd.add_ap(apdev[0], params)
    bssid0 = hapd0.own_addr()

    dev[0].scan_for_bss(bssid0, freq="2412")
    with alloc_fail(hapd0, 1, "wpa_ft_store_pmk_r0"):
        dev[0].connect(ssid, psk=passphrase, key_mgmt="FT-PSK", proto="WPA2",
                       scan_freq="2412")

    params = ft_params2(ssid=ssid, passphrase=passphrase)
    hapd1 = hostapd.add_ap(apdev[1], params)
    bssid1 = hapd1.own_addr()
    dev[0].scan_for_bss(bssid1, freq="2412")
    # This roam will fail due to missing PMK-R0 (OOM prevented storing it)
    dev[0].roam(bssid1, check_bssid=False)

def test_ap_ft_ap_oom2(dev, apdev):
    """WPA2-PSK-FT and AP OOM 2"""
    ssid = "test-ft"
    passphrase = "12345678"

    params = ft_params1(ssid=ssid, passphrase=passphrase)
    hapd0 = hostapd.add_ap(apdev[0], params)
    bssid0 = hapd0.own_addr()

    dev[0].scan_for_bss(bssid0, freq="2412")
    with alloc_fail(hapd0, 1, "wpa_ft_store_pmk_r1"):
        dev[0].connect(ssid, psk=passphrase, key_mgmt="FT-PSK", proto="WPA2",
                       scan_freq="2412")

    params = ft_params2(ssid=ssid, passphrase=passphrase)
    hapd1 = hostapd.add_ap(apdev[1], params)
    bssid1 = hapd1.own_addr()
    dev[0].scan_for_bss(bssid1, freq="2412")
    dev[0].roam(bssid1)
    if dev[0].get_status_field('bssid') != bssid1:
        raise Exception("Did not roam to AP1")
    # This roam will fail due to missing PMK-R1 (OOM prevented storing it)
    dev[0].roam(bssid0)

def test_ap_ft_ap_oom3(dev, apdev):
    """WPA2-PSK-FT and AP OOM 3"""
    ssid = "test-ft"
    passphrase = "12345678"

    params = ft_params1(ssid=ssid, passphrase=passphrase)
    hapd0 = hostapd.add_ap(apdev[0], params)
    bssid0 = hapd0.own_addr()

    dev[0].scan_for_bss(bssid0, freq="2412")
    dev[0].connect(ssid, psk=passphrase, key_mgmt="FT-PSK", proto="WPA2",
                   scan_freq="2412")

    params = ft_params2(ssid=ssid, passphrase=passphrase)
    hapd1 = hostapd.add_ap(apdev[1], params)
    bssid1 = hapd1.own_addr()
    dev[0].scan_for_bss(bssid1, freq="2412")
    with alloc_fail(hapd1, 1, "wpa_ft_pull_pmk_r1"):
        # This will fail due to not being able to send out PMK-R1 pull request
        dev[0].roam(bssid1, check_bssid=False)

    with fail_test(hapd1, 2, "os_get_random;wpa_ft_pull_pmk_r1"):
        # This will fail due to not being able to send out PMK-R1 pull request
        dev[0].roam(bssid1, check_bssid=False)

    with fail_test(hapd1, 2, "aes_siv_encrypt;wpa_ft_pull_pmk_r1"):
        # This will fail due to not being able to send out PMK-R1 pull request
        dev[0].roam(bssid1, check_bssid=False)

def test_ap_ft_ap_oom3b(dev, apdev):
    """WPA2-PSK-FT and AP OOM 3b"""
    ssid = "test-ft"
    passphrase = "12345678"

    params = ft_params1(ssid=ssid, passphrase=passphrase)
    hapd0 = hostapd.add_ap(apdev[0], params)
    bssid0 = hapd0.own_addr()

    dev[0].scan_for_bss(bssid0, freq="2412")
    dev[0].connect(ssid, psk=passphrase, key_mgmt="FT-PSK", proto="WPA2",
                   scan_freq="2412")

    params = ft_params2(ssid=ssid, passphrase=passphrase)
    hapd1 = hostapd.add_ap(apdev[1], params)
    bssid1 = hapd1.own_addr()
    dev[0].scan_for_bss(bssid1, freq="2412")
    with fail_test(hapd1, 1, "os_get_random;wpa_ft_pull_pmk_r1"):
        # This will fail due to not being able to send out PMK-R1 pull request
        dev[0].roam(bssid1)

def test_ap_ft_ap_oom4(dev, apdev):
    """WPA2-PSK-FT and AP OOM 4"""
    ssid = "test-ft"
    passphrase = "12345678"

    params = ft_params1(ssid=ssid, passphrase=passphrase)
    hapd0 = hostapd.add_ap(apdev[0], params)
    bssid0 = hapd0.own_addr()

    dev[0].scan_for_bss(bssid0, freq="2412")
    dev[0].connect(ssid, psk=passphrase, key_mgmt="FT-PSK", proto="WPA2",
                   scan_freq="2412")

    params = ft_params2(ssid=ssid, passphrase=passphrase)
    hapd1 = hostapd.add_ap(apdev[1], params)
    bssid1 = hapd1.own_addr()
    dev[0].scan_for_bss(bssid1, freq="2412")
    with alloc_fail(hapd1, 1, "wpa_ft_gtk_subelem"):
        dev[0].roam(bssid1)
        if dev[0].get_status_field('bssid') != bssid1:
            raise Exception("Did not roam to AP1")

    with fail_test(hapd0, 1, "i802_get_seqnum;wpa_ft_gtk_subelem"):
        dev[0].roam(bssid0)
        if dev[0].get_status_field('bssid') != bssid0:
            raise Exception("Did not roam to AP0")

    with fail_test(hapd0, 1, "aes_wrap;wpa_ft_gtk_subelem"):
        dev[0].roam(bssid1)
        if dev[0].get_status_field('bssid') != bssid1:
            raise Exception("Did not roam to AP1")

def test_ap_ft_ap_oom5(dev, apdev):
    """WPA2-PSK-FT and AP OOM 5"""
    ssid = "test-ft"
    passphrase = "12345678"

    params = ft_params1(ssid=ssid, passphrase=passphrase)
    hapd0 = hostapd.add_ap(apdev[0], params)
    bssid0 = hapd0.own_addr()

    dev[0].scan_for_bss(bssid0, freq="2412")
    dev[0].connect(ssid, psk=passphrase, key_mgmt="FT-PSK", proto="WPA2",
                   scan_freq="2412")

    params = ft_params2(ssid=ssid, passphrase=passphrase)
    hapd1 = hostapd.add_ap(apdev[1], params)
    bssid1 = hapd1.own_addr()
    dev[0].scan_for_bss(bssid1, freq="2412")
    with alloc_fail(hapd1, 1, "=wpa_ft_process_auth_req"):
        # This will fail to roam
        dev[0].roam(bssid1, check_bssid=False)

    with fail_test(hapd1, 1, "os_get_random;wpa_ft_process_auth_req"):
        # This will fail to roam
        dev[0].roam(bssid1, check_bssid=False)

    with fail_test(hapd1, 1, "sha256_prf_bits;wpa_pmk_r1_to_ptk;wpa_ft_process_auth_req"):
        # This will fail to roam
        dev[0].roam(bssid1, check_bssid=False)

    with fail_test(hapd1, 3, "wpa_pmk_r1_to_ptk;wpa_ft_process_auth_req"):
        # This will fail to roam
        dev[0].roam(bssid1, check_bssid=False)

    with fail_test(hapd1, 1, "wpa_derive_pmk_r1_name;wpa_ft_process_auth_req"):
        # This will fail to roam
        dev[0].roam(bssid1, check_bssid=False)

def test_ap_ft_ap_oom6(dev, apdev):
    """WPA2-PSK-FT and AP OOM 6"""
    ssid = "test-ft"
    passphrase = "12345678"

    params = ft_params1(ssid=ssid, passphrase=passphrase)
    hapd0 = hostapd.add_ap(apdev[0], params)
    bssid0 = hapd0.own_addr()

    dev[0].scan_for_bss(bssid0, freq="2412")
    with fail_test(hapd0, 1, "wpa_derive_pmk_r0;wpa_auth_derive_ptk_ft"):
        dev[0].connect(ssid, psk=passphrase, key_mgmt="FT-PSK", proto="WPA2",
                       scan_freq="2412")
    dev[0].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected()
    with fail_test(hapd0, 1, "wpa_derive_pmk_r1;wpa_auth_derive_ptk_ft"):
        dev[0].connect(ssid, psk=passphrase, key_mgmt="FT-PSK", proto="WPA2",
                       scan_freq="2412")
    dev[0].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected()
    with fail_test(hapd0, 1, "wpa_pmk_r1_to_ptk;wpa_auth_derive_ptk_ft"):
        dev[0].connect(ssid, psk=passphrase, key_mgmt="FT-PSK", proto="WPA2",
                       scan_freq="2412")

def test_ap_ft_ap_oom7a(dev, apdev):
    """WPA2-PSK-FT and AP OOM 7a"""
    ssid = "test-ft"
    passphrase = "12345678"

    params = ft_params1(ssid=ssid, passphrase=passphrase)
    params["ieee80211w"] = "2"
    hapd0 = hostapd.add_ap(apdev[0], params)
    bssid0 = hapd0.own_addr()

    dev[0].scan_for_bss(bssid0, freq="2412")
    dev[0].connect(ssid, psk=passphrase, key_mgmt="FT-PSK", proto="WPA2",
                   ieee80211w="2", scan_freq="2412")

    params = ft_params2(ssid=ssid, passphrase=passphrase)
    params["ieee80211w"] = "2"
    hapd1 = hostapd.add_ap(apdev[1], params)
    bssid1 = hapd1.own_addr()
    dev[0].scan_for_bss(bssid1, freq="2412")
    with alloc_fail(hapd1, 1, "wpa_ft_igtk_subelem"):
        # This will fail to roam
        dev[0].roam(bssid1)

def test_ap_ft_ap_oom7b(dev, apdev):
    """WPA2-PSK-FT and AP OOM 7b"""
    ssid = "test-ft"
    passphrase = "12345678"

    params = ft_params1(ssid=ssid, passphrase=passphrase)
    params["ieee80211w"] = "2"
    hapd0 = hostapd.add_ap(apdev[0], params)
    bssid0 = hapd0.own_addr()

    dev[0].scan_for_bss(bssid0, freq="2412")
    dev[0].connect(ssid, psk=passphrase, key_mgmt="FT-PSK", proto="WPA2",
                   ieee80211w="2", scan_freq="2412")

    params = ft_params2(ssid=ssid, passphrase=passphrase)
    params["ieee80211w"] = "2"
    hapd1 = hostapd.add_ap(apdev[1], params)
    bssid1 = hapd1.own_addr()
    dev[0].scan_for_bss(bssid1, freq="2412")
    with fail_test(hapd1, 1, "aes_wrap;wpa_ft_igtk_subelem"):
        # This will fail to roam
        dev[0].roam(bssid1)

def test_ap_ft_ap_oom7c(dev, apdev):
    """WPA2-PSK-FT and AP OOM 7c"""
    ssid = "test-ft"
    passphrase = "12345678"

    params = ft_params1(ssid=ssid, passphrase=passphrase)
    params["ieee80211w"] = "2"
    hapd0 = hostapd.add_ap(apdev[0], params)
    bssid0 = hapd0.own_addr()

    dev[0].scan_for_bss(bssid0, freq="2412")
    dev[0].connect(ssid, psk=passphrase, key_mgmt="FT-PSK", proto="WPA2",
                   ieee80211w="2", scan_freq="2412")

    params = ft_params2(ssid=ssid, passphrase=passphrase)
    params["ieee80211w"] = "2"
    hapd1 = hostapd.add_ap(apdev[1], params)
    bssid1 = hapd1.own_addr()
    dev[0].scan_for_bss(bssid1, freq="2412")
    with alloc_fail(hapd1, 1, "=wpa_sm_write_assoc_resp_ies"):
        # This will fail to roam
        dev[0].roam(bssid1)

def test_ap_ft_ap_oom7d(dev, apdev):
    """WPA2-PSK-FT and AP OOM 7d"""
    ssid = "test-ft"
    passphrase = "12345678"

    params = ft_params1(ssid=ssid, passphrase=passphrase)
    params["ieee80211w"] = "2"
    hapd0 = hostapd.add_ap(apdev[0], params)
    bssid0 = hapd0.own_addr()

    dev[0].scan_for_bss(bssid0, freq="2412")
    dev[0].connect(ssid, psk=passphrase, key_mgmt="FT-PSK", proto="WPA2",
                   ieee80211w="2", scan_freq="2412")

    params = ft_params2(ssid=ssid, passphrase=passphrase)
    params["ieee80211w"] = "2"
    hapd1 = hostapd.add_ap(apdev[1], params)
    bssid1 = hapd1.own_addr()
    dev[0].scan_for_bss(bssid1, freq="2412")
    with fail_test(hapd1, 1, "wpa_ft_mic;wpa_sm_write_assoc_resp_ies"):
        # This will fail to roam
        dev[0].roam(bssid1)

def test_ap_ft_ap_oom8(dev, apdev):
    """WPA2-PSK-FT and AP OOM 8"""
    ssid = "test-ft"
    passphrase = "12345678"

    params = ft_params1(ssid=ssid, passphrase=passphrase)
    params['ft_psk_generate_local'] = "1"
    hapd0 = hostapd.add_ap(apdev[0], params)
    bssid0 = hapd0.own_addr()

    dev[0].scan_for_bss(bssid0, freq="2412")
    dev[0].connect(ssid, psk=passphrase, key_mgmt="FT-PSK", proto="WPA2",
                   scan_freq="2412")

    params = ft_params2(ssid=ssid, passphrase=passphrase)
    params['ft_psk_generate_local'] = "1"
    hapd1 = hostapd.add_ap(apdev[1], params)
    bssid1 = hapd1.own_addr()
    dev[0].scan_for_bss(bssid1, freq="2412")
    with fail_test(hapd1, 1, "wpa_derive_pmk_r0;wpa_ft_psk_pmk_r1"):
        # This will fail to roam
        dev[0].roam(bssid1, check_bssid=False)
    with fail_test(hapd1, 1, "wpa_derive_pmk_r1;wpa_ft_psk_pmk_r1"):
        # This will fail to roam
        dev[0].roam(bssid1, check_bssid=False)

def test_ap_ft_ap_oom9(dev, apdev):
    """WPA2-PSK-FT and AP OOM 9"""
    ssid = "test-ft"
    passphrase = "12345678"

    params = ft_params1(ssid=ssid, passphrase=passphrase)
    hapd0 = hostapd.add_ap(apdev[0], params)
    bssid0 = hapd0.own_addr()

    dev[0].scan_for_bss(bssid0, freq="2412")
    dev[0].connect(ssid, psk=passphrase, key_mgmt="FT-PSK", proto="WPA2",
                   scan_freq="2412")

    params = ft_params2(ssid=ssid, passphrase=passphrase)
    hapd1 = hostapd.add_ap(apdev[1], params)
    bssid1 = hapd1.own_addr()
    dev[0].scan_for_bss(bssid1, freq="2412")

    with alloc_fail(hapd0, 1, "wpa_ft_action_rx"):
        # This will fail to roam
        if "OK" not in dev[0].request("FT_DS " + bssid1):
            raise Exception("FT_DS failed")
        wait_fail_trigger(hapd0, "GET_ALLOC_FAIL")

    with alloc_fail(hapd1, 1, "wpa_ft_rrb_rx_request"):
        # This will fail to roam
        if "OK" not in dev[0].request("FT_DS " + bssid1):
            raise Exception("FT_DS failed")
        wait_fail_trigger(hapd1, "GET_ALLOC_FAIL")

    with alloc_fail(hapd1, 1, "wpa_ft_send_rrb_auth_resp"):
        # This will fail to roam
        if "OK" not in dev[0].request("FT_DS " + bssid1):
            raise Exception("FT_DS failed")
        wait_fail_trigger(hapd1, "GET_ALLOC_FAIL")

def test_ap_ft_ap_oom10(dev, apdev):
    """WPA2-PSK-FT and AP OOM 10"""
    ssid = "test-ft"
    passphrase = "12345678"

    params = ft_params1(ssid=ssid, passphrase=passphrase)
    hapd0 = hostapd.add_ap(apdev[0], params)
    bssid0 = hapd0.own_addr()

    dev[0].scan_for_bss(bssid0, freq="2412")
    dev[0].connect(ssid, psk=passphrase, key_mgmt="FT-PSK", proto="WPA2",
                   scan_freq="2412")

    params = ft_params2(ssid=ssid, passphrase=passphrase)
    hapd1 = hostapd.add_ap(apdev[1], params)
    bssid1 = hapd1.own_addr()
    dev[0].scan_for_bss(bssid1, freq="2412")

    with fail_test(hapd0, 1, "aes_siv_decrypt;wpa_ft_rrb_rx_pull"):
        # This will fail to roam
        if "OK" not in dev[0].request("FT_DS " + bssid1):
            raise Exception("FT_DS failed")
        wait_fail_trigger(hapd0, "GET_FAIL")

    with fail_test(hapd0, 1, "wpa_derive_pmk_r1;wpa_ft_rrb_rx_pull"):
        # This will fail to roam
        if "OK" not in dev[0].request("FT_DS " + bssid1):
            raise Exception("FT_DS failed")
        wait_fail_trigger(hapd0, "GET_FAIL")

    with fail_test(hapd0, 1, "aes_siv_encrypt;wpa_ft_rrb_rx_pull"):
        # This will fail to roam
        if "OK" not in dev[0].request("FT_DS " + bssid1):
            raise Exception("FT_DS failed")
        wait_fail_trigger(hapd0, "GET_FAIL")

    with fail_test(hapd1, 1, "aes_siv_decrypt;wpa_ft_rrb_rx_resp"):
        # This will fail to roam
        if "OK" not in dev[0].request("FT_DS " + bssid1):
            raise Exception("FT_DS failed")
        wait_fail_trigger(hapd1, "GET_FAIL")

def test_ap_ft_ap_oom11(dev, apdev):
    """WPA2-PSK-FT and AP OOM 11"""
    ssid = "test-ft"
    passphrase = "12345678"

    params = ft_params1(ssid=ssid, passphrase=passphrase)
    hapd0 = hostapd.add_ap(apdev[0], params)
    bssid0 = hapd0.own_addr()

    dev[0].scan_for_bss(bssid0, freq="2412")
    with fail_test(hapd0, 1, "wpa_derive_pmk_r1;wpa_ft_generate_pmk_r1"):
        dev[0].connect(ssid, psk=passphrase, key_mgmt="FT-PSK", proto="WPA2",
                       scan_freq="2412")
        wait_fail_trigger(hapd0, "GET_FAIL")

    dev[1].scan_for_bss(bssid0, freq="2412")
    with fail_test(hapd0, 1, "aes_siv_encrypt;wpa_ft_generate_pmk_r1"):
        dev[1].connect(ssid, psk=passphrase, key_mgmt="FT-PSK", proto="WPA2",
                       scan_freq="2412")
        wait_fail_trigger(hapd0, "GET_FAIL")

def test_ap_ft_over_ds_proto_ap(dev, apdev):
    """WPA2-PSK-FT AP over DS protocol testing for AP processing"""
    ssid = "test-ft"
    passphrase = "12345678"

    params = ft_params1(ssid=ssid, passphrase=passphrase)
    hapd0 = hostapd.add_ap(apdev[0], params)
    bssid0 = hapd0.own_addr()
    _bssid0 = bssid0.replace(':', '')
    dev[0].connect(ssid, psk=passphrase, key_mgmt="FT-PSK", proto="WPA2",
                   scan_freq="2412")
    addr = dev[0].own_addr()
    _addr = addr.replace(':', '')

    params = ft_params2(ssid=ssid, passphrase=passphrase)
    hapd1 = hostapd.add_ap(apdev[1], params)
    bssid1 = hapd1.own_addr()
    _bssid1 = bssid1.replace(':', '')

    hapd0.set("ext_mgmt_frame_handling", "1")
    hdr = "d0003a01" + _bssid0 + _addr + _bssid0 + "1000"
    valid = "0601" + _addr + _bssid1
    tests = ["0601",
             "0601" + _addr,
             "0601" + _addr + _bssid0,
             "0601" + _addr + "ffffffffffff",
             "0601" + _bssid0 + _bssid0,
             valid,
             valid + "01",
             valid + "3700",
             valid + "3600",
             valid + "3603ffffff",
             valid + "3603a1b2ff",
             valid + "3603a1b2ff" + "3700",
             valid + "3603a1b2ff" + "37520000" + 16*"00" + 32*"00" + 32*"00",
             valid + "3603a1b2ff" + "37520001" + 16*"00" + 32*"00" + 32*"00",
             valid + "3603a1b2ff" + "37550000" + 16*"00" + 32*"00" + 32*"00" + "0301aa",
             valid + "3603a1b2ff" + "37550000" + 16*"00" + 32*"00" + 32*"00" + "0301aa" + "3000",
             valid + "3603a1b2ff" + "37550000" + 16*"00" + 32*"00" + 32*"00" + "0301aa" + "30260100000fac040100000fac040100000facff00000100a225368fe0983b5828a37a0acb37f253",
             valid + "3603a1b2ff" + "37550000" + 16*"00" + 32*"00" + 32*"00" + "0301aa" + "30260100000fac040100000fac030100000fac0400000100a225368fe0983b5828a37a0acb37f253",
             valid + "3603a1b2ff" + "37550000" + 16*"00" + 32*"00" + 32*"00" + "0301aa" + "30260100000fac040100000fac040100000fac0400000100a225368fe0983b5828a37a0acb37f253",
             valid + "0001"]
    for t in tests:
        hapd0.dump_monitor()
        if "OK" not in hapd0.request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=" + hdr + t):
            raise Exception("MGMT_RX_PROCESS failed")

    hapd0.set("ext_mgmt_frame_handling", "0")

def test_ap_ft_over_ds_proto(dev, apdev):
    """WPA2-PSK-FT AP over DS protocol testing"""
    ssid = "test-ft"
    passphrase = "12345678"

    params = ft_params1(ssid=ssid, passphrase=passphrase)
    hapd0 = hostapd.add_ap(apdev[0], params)
    dev[0].connect(ssid, psk=passphrase, key_mgmt="FT-PSK", proto="WPA2",
                   scan_freq="2412")

    # FT Action Response while no FT-over-DS in progress
    msg = {}
    msg['fc'] = 13 << 4
    msg['da'] = dev[0].own_addr()
    msg['sa'] = apdev[0]['bssid']
    msg['bssid'] = apdev[0]['bssid']
    msg['payload'] = binascii.unhexlify("06020200000000000200000004000000")
    hapd0.mgmt_tx(msg)

    params = ft_params2(ssid=ssid, passphrase=passphrase)
    hapd1 = hostapd.add_ap(apdev[1], params)
    dev[0].scan_for_bss(apdev[1]['bssid'], freq="2412")
    hapd0.set("ext_mgmt_frame_handling", "1")
    hapd0.dump_monitor()
    dev[0].request("FT_DS " + apdev[1]['bssid'])
    for i in range(0, 10):
        req = hapd0.mgmt_rx()
        if req is None:
            raise Exception("MGMT RX wait timed out")
        if req['subtype'] == 13:
            break
        req = None
    if not req:
        raise Exception("FT Action frame not received")

    # FT Action Response for unexpected Target AP
    msg['payload'] = binascii.unhexlify("0602020000000000" + "f20000000400" + "0000")
    hapd0.mgmt_tx(msg)

    # FT Action Response without MDIE
    msg['payload'] = binascii.unhexlify("0602020000000000" + "020000000400" + "0000")
    hapd0.mgmt_tx(msg)

    # FT Action Response without FTIE
    msg['payload'] = binascii.unhexlify("0602020000000000" + "020000000400" + "0000" + "3603a1b201")
    hapd0.mgmt_tx(msg)

    # FT Action Response with FTIE SNonce mismatch
    msg['payload'] = binascii.unhexlify("0602020000000000" + "020000000400" + "0000" + "3603a1b201" + "3766000000000000000000000000000000000000c4e67ac1999bebd00ff4ae4d5dcaf87896bb060b469f7c78d49623fb395c3455ffffff6b693fe6f8d8c5dfac0a22344750775bd09437f98b238c9f87b97f790c0106000102030406030a6e6173312e77312e6669")
    hapd0.mgmt_tx(msg)

@remote_compatible
def test_ap_ft_rrb(dev, apdev):
    """WPA2-PSK-FT RRB protocol testing"""
    ssid = "test-ft"
    passphrase = "12345678"

    params = ft_params1(ssid=ssid, passphrase=passphrase)
    hapd0 = hostapd.add_ap(apdev[0], params)

    dev[0].connect(ssid, psk=passphrase, key_mgmt="FT-PSK", proto="WPA2",
                   scan_freq="2412")

    _dst_ll = binascii.unhexlify(apdev[0]['bssid'].replace(':', ''))
    _src_ll = binascii.unhexlify(dev[0].own_addr().replace(':', ''))
    proto = b'\x89\x0d'
    ehdr = _dst_ll + _src_ll + proto

    # Too short RRB frame
    pkt = ehdr + b'\x01'
    if "OK" not in dev[0].request("DATA_TEST_FRAME " + binascii.hexlify(pkt).decode()):
        raise Exception("DATA_TEST_FRAME failed")

    # RRB discarded frame wikth unrecognized type
    pkt = ehdr + b'\x02' + b'\x02' + b'\x01\x00' + _src_ll
    if "OK" not in dev[0].request("DATA_TEST_FRAME " + binascii.hexlify(pkt).decode()):
        raise Exception("DATA_TEST_FRAME failed")

    # RRB frame too short for action frame
    pkt = ehdr + b'\x01' + b'\x02' + b'\x01\x00' + _src_ll
    if "OK" not in dev[0].request("DATA_TEST_FRAME " + binascii.hexlify(pkt).decode()):
        raise Exception("DATA_TEST_FRAME failed")

    # Too short RRB frame (not enough room for Action Frame body)
    pkt = ehdr + b'\x01' + b'\x02' + b'\x00\x00' + _src_ll
    if "OK" not in dev[0].request("DATA_TEST_FRAME " + binascii.hexlify(pkt).decode()):
        raise Exception("DATA_TEST_FRAME failed")

    # Unexpected Action frame category
    pkt = ehdr + b'\x01' + b'\x02' + b'\x0e\x00' + _src_ll + b'\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00'
    if "OK" not in dev[0].request("DATA_TEST_FRAME " + binascii.hexlify(pkt).decode()):
        raise Exception("DATA_TEST_FRAME failed")

    # Unexpected Action in RRB Request
    pkt = ehdr + b'\x01' + b'\x00' + b'\x0e\x00' + _src_ll + b'\x06\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00'
    if "OK" not in dev[0].request("DATA_TEST_FRAME " + binascii.hexlify(pkt).decode()):
        raise Exception("DATA_TEST_FRAME failed")

    # Target AP address in RRB Request does not match with own address
    pkt = ehdr + b'\x01' + b'\x00' + b'\x0e\x00' + _src_ll + b'\x06\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00'
    if "OK" not in dev[0].request("DATA_TEST_FRAME " + binascii.hexlify(pkt).decode()):
        raise Exception("DATA_TEST_FRAME failed")

    # Not enough room for status code in RRB Response
    pkt = ehdr + b'\x01' + b'\x01' + b'\x0e\x00' + _src_ll + b'\x06\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00'
    if "OK" not in dev[0].request("DATA_TEST_FRAME " + binascii.hexlify(pkt).decode()):
        raise Exception("DATA_TEST_FRAME failed")

    # RRB discarded frame with unknown packet_type
    pkt = ehdr + b'\x01' + b'\x02' + b'\x0e\x00' + _src_ll + b'\x06\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00'
    if "OK" not in dev[0].request("DATA_TEST_FRAME " + binascii.hexlify(pkt).decode()):
        raise Exception("DATA_TEST_FRAME failed")

    # RRB Response with non-zero status code; no STA match
    pkt = ehdr + b'\x01' + b'\x01' + b'\x10\x00' + _src_ll + b'\x06\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00' + b'\xff\xff'
    if "OK" not in dev[0].request("DATA_TEST_FRAME " + binascii.hexlify(pkt).decode()):
        raise Exception("DATA_TEST_FRAME failed")

    # RRB Response with zero status code and extra data; STA match
    pkt = ehdr + b'\x01' + b'\x01' + b'\x11\x00' + _src_ll + b'\x06\x01' + _src_ll + b'\x00\x00\x00\x00\x00\x00' + b'\x00\x00' + b'\x00'
    if "OK" not in dev[0].request("DATA_TEST_FRAME " + binascii.hexlify(pkt).decode()):
        raise Exception("DATA_TEST_FRAME failed")

    # Too short PMK-R1 pull
    pkt = ehdr + b'\x01' + b'\xc8' + b'\x0e\x00' + _src_ll + b'\x06\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00'
    if "OK" not in dev[0].request("DATA_TEST_FRAME " + binascii.hexlify(pkt).decode()):
        raise Exception("DATA_TEST_FRAME failed")

    # Too short PMK-R1 resp
    pkt = ehdr + b'\x01' + b'\xc9' + b'\x0e\x00' + _src_ll + b'\x06\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00'
    if "OK" not in dev[0].request("DATA_TEST_FRAME " + binascii.hexlify(pkt).decode()):
        raise Exception("DATA_TEST_FRAME failed")

    # Too short PMK-R1 push
    pkt = ehdr + b'\x01' + b'\xca' + b'\x0e\x00' + _src_ll + b'\x06\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00'
    if "OK" not in dev[0].request("DATA_TEST_FRAME " + binascii.hexlify(pkt).decode()):
        raise Exception("DATA_TEST_FRAME failed")

    # No matching R0KH address found for PMK-R0 pull response
    pkt = ehdr + b'\x01' + b'\xc9' + b'\x5a\x00' + _src_ll + b'\x06\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00' + 76 * b'\00'
    if "OK" not in dev[0].request("DATA_TEST_FRAME " + binascii.hexlify(pkt).decode()):
        raise Exception("DATA_TEST_FRAME failed")

@remote_compatible
def test_rsn_ie_proto_ft_psk_sta(dev, apdev):
    """RSN element protocol testing for FT-PSK + PMF cases on STA side"""
    bssid = apdev[0]['bssid']
    ssid = "test-ft"
    passphrase = "12345678"

    params = ft_params1(ssid=ssid, passphrase=passphrase)
    params["ieee80211w"] = "1"
    # This is the RSN element used normally by hostapd
    params['own_ie_override'] = '30140100000fac040100000fac040100000fac048c00' + '3603a1b201'
    hapd = hostapd.add_ap(apdev[0], params)
    id = dev[0].connect(ssid, psk=passphrase, key_mgmt="FT-PSK", proto="WPA2",
                        ieee80211w="1", scan_freq="2412",
                        pairwise="CCMP", group="CCMP")

    tests = [('PMKIDCount field included',
              '30160100000fac040100000fac040100000fac048c000000' + '3603a1b201'),
             ('Extra IE before RSNE',
              'dd0400000000' + '30140100000fac040100000fac040100000fac048c00' + '3603a1b201'),
             ('PMKIDCount and Group Management Cipher suite fields included',
              '301a0100000fac040100000fac040100000fac048c000000000fac06' + '3603a1b201'),
             ('Extra octet after defined fields (future extensibility)',
              '301b0100000fac040100000fac040100000fac048c000000000fac0600' + '3603a1b201'),
             ('No RSN Capabilities field (PMF disabled in practice)',
              '30120100000fac040100000fac040100000fac04' + '3603a1b201')]
    for txt, ie in tests:
        dev[0].request("DISCONNECT")
        dev[0].wait_disconnected()
        logger.info(txt)
        hapd.disable()
        hapd.set('own_ie_override', ie)
        hapd.enable()
        dev[0].request("BSS_FLUSH 0")
        dev[0].scan_for_bss(bssid, 2412, force_scan=True, only_new=True)
        dev[0].select_network(id, freq=2412)
        dev[0].wait_connected()

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    logger.info('Invalid RSNE causing internal hostapd error')
    hapd.disable()
    hapd.set('own_ie_override', '30130100000fac040100000fac040100000fac048c' + '3603a1b201')
    hapd.enable()
    dev[0].request("BSS_FLUSH 0")
    dev[0].scan_for_bss(bssid, 2412, force_scan=True, only_new=True)
    dev[0].select_network(id, freq=2412)
    # hostapd fails to generate EAPOL-Key msg 3/4, so this connection cannot
    # complete.
    ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED"], timeout=1)
    if ev is not None:
        raise Exception("Unexpected connection")
    dev[0].request("DISCONNECT")

def start_ft(apdev, wpa_ptk_rekey=None):
    ssid = "test-ft"
    passphrase = "12345678"

    params = ft_params1(ssid=ssid, passphrase=passphrase)
    if wpa_ptk_rekey:
        params['wpa_ptk_rekey'] = str(wpa_ptk_rekey)
    hapd0 = hostapd.add_ap(apdev[0], params)
    params = ft_params2(ssid=ssid, passphrase=passphrase)
    if wpa_ptk_rekey:
        params['wpa_ptk_rekey'] = str(wpa_ptk_rekey)
    hapd1 = hostapd.add_ap(apdev[1], params)

    return hapd0, hapd1

def check_ptk_rekey(dev, hapd0=None, hapd1=None):
    ev = dev.wait_event(["CTRL-EVENT-DISCONNECTED",
                         "WPA: Key negotiation completed"], timeout=5)
    if ev is None:
        raise Exception("No event received after roam")
    if "CTRL-EVENT-DISCONNECTED" in ev:
        raise Exception("Unexpected disconnection after roam")

    if not hapd0 or not hapd1:
        return
    if dev.get_status_field('bssid') == hapd0.own_addr():
        hapd = hapd0
    else:
        hapd = hapd1
    time.sleep(0.1)
    hwsim_utils.test_connectivity(dev, hapd)

def test_ap_ft_ptk_rekey(dev, apdev):
    """WPA2-PSK-FT PTK rekeying triggered by station after roam"""
    hapd0, hapd1 = start_ft(apdev)
    run_roams(dev[0], apdev, hapd0, hapd1, "test-ft", "12345678", ptk_rekey="1")
    check_ptk_rekey(dev[0], hapd0, hapd1)

def test_ap_ft_ptk_rekey2(dev, apdev):
    """WPA2-PSK-FT PTK rekeying triggered by station after one roam"""
    hapd0, hapd1 = start_ft(apdev)
    run_roams(dev[0], apdev, hapd0, hapd1, "test-ft", "12345678", ptk_rekey="1",
              only_one_way=True)
    check_ptk_rekey(dev[0], hapd0, hapd1)

def test_ap_ft_ptk_rekey_ap(dev, apdev):
    """WPA2-PSK-FT PTK rekeying triggered by AP after roam"""
    hapd0, hapd1 = start_ft(apdev, wpa_ptk_rekey=2)
    run_roams(dev[0], apdev, hapd0, hapd1, "test-ft", "12345678")
    check_ptk_rekey(dev[0], hapd0, hapd1)

def test_ap_ft_ptk_rekey_ap2(dev, apdev):
    """WPA2-PSK-FT PTK rekeying triggered by AP after one roam"""
    hapd0, hapd1 = start_ft(apdev, wpa_ptk_rekey=2)
    run_roams(dev[0], apdev, hapd0, hapd1, "test-ft", "12345678",
              only_one_way=True)
    check_ptk_rekey(dev[0], hapd0, hapd1)

def test_ap_ft_eap_ptk_rekey_ap(dev, apdev):
    """WPA2-EAP-FT PTK rekeying triggered by AP"""
    generic_ap_ft_eap(dev, apdev, only_one_way=True, wpa_ptk_rekey=2)
    check_ptk_rekey(dev[0])

def test_ap_ft_internal_rrb_check(dev, apdev):
    """RRB internal delivery only to WPA enabled BSS"""
    ssid = "test-ft"
    passphrase = "12345678"

    radius = hostapd.radius_params()
    params = ft_params1(ssid=ssid, passphrase=passphrase)
    params['wpa_key_mgmt'] = "FT-EAP"
    params["ieee8021x"] = "1"
    params = dict(list(radius.items()) + list(params.items()))
    hapd = hostapd.add_ap(apdev[0], params)
    key_mgmt = hapd.get_config()['key_mgmt']
    if key_mgmt.split(' ')[0] != "FT-EAP":
        raise Exception("Unexpected GET_CONFIG(key_mgmt): " + key_mgmt)

    hapd1 = hostapd.add_ap(apdev[1], {"ssid": ssid})

    # Connect to WPA enabled AP
    dev[0].connect(ssid, key_mgmt="FT-EAP", proto="WPA2", ieee80211w="1",
                   eap="GPSK", identity="gpsk user",
                   password="abcdefghijklmnop0123456789abcdef",
                   scan_freq="2412")

    # Try over_ds roaming to non-WPA-enabled AP.
    # If hostapd does not check hapd->wpa_auth internally, it will crash now.
    dev[0].roam_over_ds(apdev[1]['bssid'], fail_test=True)

def test_ap_ft_extra_ie(dev, apdev):
    """WPA2-PSK-FT AP with WPA2-PSK enabled and unexpected MDE"""
    ssid = "test-ft"
    passphrase = "12345678"

    params = ft_params1(ssid=ssid, passphrase=passphrase)
    params["wpa_key_mgmt"] = "WPA-PSK FT-PSK"
    hapd0 = hostapd.add_ap(apdev[0], params)
    dev[1].connect(ssid, psk=passphrase, key_mgmt="FT-PSK", proto="WPA2",
                   scan_freq="2412")
    dev[2].connect(ssid, psk=passphrase, key_mgmt="WPA-PSK", proto="WPA2",
                   scan_freq="2412")
    try:
        # Add Mobility Domain element to test AP validation code.
        dev[0].request("VENDOR_ELEM_ADD 13 3603a1b201")
        dev[0].connect(ssid, psk=passphrase, key_mgmt="WPA-PSK", proto="WPA2",
                       scan_freq="2412", wait_connect=False)
        ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED",
                                "CTRL-EVENT-ASSOC-REJECT"], timeout=10)
        if ev is None:
            raise Exception("No connection result")
        if "CTRL-EVENT-CONNECTED" in ev:
            raise Exception("Non-FT association accepted with MDE")
        if "status_code=43" not in ev:
            raise Exception("Unexpected status code: " + ev)
        dev[0].request("DISCONNECT")
    finally:
        dev[0].request("VENDOR_ELEM_REMOVE 13 *")

def test_ap_ft_ric(dev, apdev):
    """WPA2-PSK-FT AP and RIC"""
    ssid = "test-ft"
    passphrase = "12345678"

    params = ft_params1(ssid=ssid, passphrase=passphrase)
    hapd0 = hostapd.add_ap(apdev[0], params)
    params = ft_params2(ssid=ssid, passphrase=passphrase)
    hapd1 = hostapd.add_ap(apdev[1], params)

    dev[0].set("ric_ies", "")
    dev[0].set("ric_ies", '""')
    if "FAIL" not in dev[0].request("SET ric_ies q"):
        raise Exception("Invalid ric_ies value accepted")

    tests = ["3900",
             "3900ff04eeeeeeee",
             "390400000000",
             "390400000000" + "390400000000",
             "390400000000" + "dd050050f20202",
             "390400000000" + "dd3d0050f2020201" + 55*"00",
             "390400000000" + "dd3d0050f2020201aa300010270000000000000000000000000000000000000000000000000000ffffff7f00000000000000000000000040420f00ffff0000",
             "390401010000" + "dd3d0050f2020201aa3000dc050000000000000000000000000000000000000000000000000000dc050000000000000000000000000000808d5b0028230000"]
    for t in tests:
        dev[0].set("ric_ies", t)
        run_roams(dev[0], apdev, hapd0, hapd1, ssid, passphrase,
                  test_connectivity=False)
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()
        dev[0].dump_monitor()

def ie_hex(ies, id):
    return binascii.hexlify(struct.pack('BB', id, len(ies[id])) + ies[id]).decode()

def test_ap_ft_reassoc_proto(dev, apdev):
    """WPA2-PSK-FT AP Reassociation Request frame parsing"""
    ssid = "test-ft"
    passphrase = "12345678"

    params = ft_params1(ssid=ssid, passphrase=passphrase)
    hapd0 = hostapd.add_ap(apdev[0], params)
    params = ft_params2(ssid=ssid, passphrase=passphrase)
    hapd1 = hostapd.add_ap(apdev[1], params)

    dev[0].connect(ssid, psk=passphrase, key_mgmt="FT-PSK", proto="WPA2",
                   ieee80211w="1", scan_freq="2412")
    if dev[0].get_status_field('bssid') == hapd0.own_addr():
        hapd1ap = hapd0
        hapd2ap = hapd1
    else:
        hapd1ap = hapd1
        hapd2ap = hapd0

    dev[0].scan_for_bss(hapd2ap.own_addr(), freq="2412")
    hapd2ap.set("ext_mgmt_frame_handling", "1")
    dev[0].request("ROAM " + hapd2ap.own_addr())

    while True:
        req = hapd2ap.mgmt_rx()
        hapd2ap.request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=" + binascii.hexlify(req['frame']).decode())
        if req['subtype'] == 11:
            break

    while True:
        req = hapd2ap.mgmt_rx()
        if req['subtype'] == 2:
            break
        hapd2ap.request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=" + binascii.hexlify(req['frame']).decode())

    # IEEE 802.11 header + fixed fields before IEs
    hdr = binascii.hexlify(req['frame'][0:34]).decode()
    ies = parse_ie(binascii.hexlify(req['frame'][34:]))
    # First elements: SSID, Supported Rates, Extended Supported Rates
    ies1 = ie_hex(ies, 0) + ie_hex(ies, 1) + ie_hex(ies, 50)

    rsne = ie_hex(ies, 48)
    mde = ie_hex(ies, 54)
    fte = ie_hex(ies, 55)
    tests = []
    # RSN: Trying to use FT, but MDIE not included
    tests += [rsne]
    # RSN: Attempted to use unknown MDIE
    tests += [rsne + "3603000000"]
    # Invalid RSN pairwise cipher
    tests += ["30260100000fac040100000fac030100000fac040000010029208a42cd25c85aa571567dce10dae3"]
    # FT: No PMKID in RSNIE
    tests += ["30160100000fac040100000fac040100000fac0400000000" + ie_hex(ies, 54)]
    # FT: Invalid FTIE
    tests += [rsne + mde]
    # FT: RIC IE(s) in the frame, but not included in protected IE count
    # FT: Failed to parse FT IEs
    tests += [rsne + mde + fte + "3900"]
    # FT: SNonce mismatch in FTIE
    tests += [rsne + mde + "37520000" + 16*"00" + 32*"00" + 32*"00"]
    # FT: ANonce mismatch in FTIE
    tests += [rsne + mde + fte[0:40] + 32*"00" + fte[104:]]
    # FT: No R0KH-ID subelem in FTIE
    tests += [rsne + mde + "3752" + fte[4:168]]
    # FT: R0KH-ID in FTIE did not match with the current R0KH-ID
    tests += [rsne + mde + "3755" + fte[4:168] + "0301ff"]
    # FT: No R1KH-ID subelem in FTIE
    tests += [rsne + mde + "375e" + fte[4:168] + "030a" + binascii.hexlify(b"nas1.w1.fi").decode()]
    # FT: Unknown R1KH-ID used in ReassocReq
    tests += [rsne + mde + "3766" + fte[4:168] + "030a" + binascii.hexlify(b"nas1.w1.fi").decode() + "0106000000000000"]
    # FT: PMKID in Reassoc Req did not match with the PMKR1Name derived from auth request
    tests += [rsne[:-32] + 16*"00" + mde + fte]
    # Invalid MIC in FTIE
    tests += [rsne + mde + fte[0:8] + 16*"00" + fte[40:]]
    for t in tests:
        hapd2ap.request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=" + hdr + ies1 + t)

def test_ap_ft_reassoc_local_fail(dev, apdev):
    """WPA2-PSK-FT AP Reassociation Request frame and local failure"""
    ssid = "test-ft"
    passphrase = "12345678"

    params = ft_params1(ssid=ssid, passphrase=passphrase)
    hapd0 = hostapd.add_ap(apdev[0], params)
    params = ft_params2(ssid=ssid, passphrase=passphrase)
    hapd1 = hostapd.add_ap(apdev[1], params)

    dev[0].connect(ssid, psk=passphrase, key_mgmt="FT-PSK", proto="WPA2",
                   ieee80211w="1", scan_freq="2412")
    if dev[0].get_status_field('bssid') == hapd0.own_addr():
        hapd1ap = hapd0
        hapd2ap = hapd1
    else:
        hapd1ap = hapd1
        hapd2ap = hapd0

    dev[0].scan_for_bss(hapd2ap.own_addr(), freq="2412")
    # FT: Failed to calculate MIC
    with fail_test(hapd2ap, 1, "wpa_ft_validate_reassoc"):
        dev[0].request("ROAM " + hapd2ap.own_addr())
        ev = dev[0].wait_event(["CTRL-EVENT-ASSOC-REJECT"], timeout=10)
        dev[0].request("DISCONNECT")
        if ev is None:
            raise Exception("Association reject not seen")

def test_ap_ft_reassoc_replay(dev, apdev, params):
    """WPA2-PSK-FT AP and replayed Reassociation Request frame"""
    capfile = os.path.join(params['logdir'], "hwsim0.pcapng")
    ssid = "test-ft"
    passphrase = "12345678"

    params = ft_params1(ssid=ssid, passphrase=passphrase)
    hapd0 = hostapd.add_ap(apdev[0], params)
    params = ft_params2(ssid=ssid, passphrase=passphrase)
    hapd1 = hostapd.add_ap(apdev[1], params)

    dev[0].connect(ssid, psk=passphrase, key_mgmt="FT-PSK", proto="WPA2",
                   scan_freq="2412")
    if dev[0].get_status_field('bssid') == hapd0.own_addr():
        hapd1ap = hapd0
        hapd2ap = hapd1
    else:
        hapd1ap = hapd1
        hapd2ap = hapd0

    dev[0].scan_for_bss(hapd2ap.own_addr(), freq="2412")
    hapd2ap.set("ext_mgmt_frame_handling", "1")
    dev[0].dump_monitor()
    if "OK" not in dev[0].request("ROAM " + hapd2ap.own_addr()):
        raise Exception("ROAM failed")

    reassocreq = None
    count = 0
    while count < 100:
        req = hapd2ap.mgmt_rx()
        count += 1
        hapd2ap.dump_monitor()
        hapd2ap.request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=" + binascii.hexlify(req['frame']).decode())
        if req['subtype'] == 2:
            reassocreq = req
            ev = hapd2ap.wait_event(["MGMT-TX-STATUS"], timeout=5)
            if ev is None:
                raise Exception("No TX status seen")
            cmd = "MGMT_TX_STATUS_PROCESS %s" % (" ".join(ev.split(' ')[1:4]))
            if "OK" not in hapd2ap.request(cmd):
                raise Exception("MGMT_TX_STATUS_PROCESS failed")
            break
    hapd2ap.set("ext_mgmt_frame_handling", "0")
    if reassocreq is None:
        raise Exception("No Reassociation Request frame seen")
    dev[0].wait_connected()
    dev[0].dump_monitor()
    hapd2ap.dump_monitor()

    hwsim_utils.test_connectivity(dev[0], hapd2ap)

    logger.info("Replay the last Reassociation Request frame")
    hapd2ap.dump_monitor()
    hapd2ap.set("ext_mgmt_frame_handling", "1")
    hapd2ap.request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=" + binascii.hexlify(req['frame']).decode())
    ev = hapd2ap.wait_event(["MGMT-TX-STATUS"], timeout=5)
    if ev is None:
        raise Exception("No TX status seen")
    cmd = "MGMT_TX_STATUS_PROCESS %s" % (" ".join(ev.split(' ')[1:4]))
    if "OK" not in hapd2ap.request(cmd):
        raise Exception("MGMT_TX_STATUS_PROCESS failed")
    hapd2ap.set("ext_mgmt_frame_handling", "0")

    try:
        hwsim_utils.test_connectivity(dev[0], hapd2ap)
        ok = True
    except:
        ok = False

    ap = hapd2ap.own_addr()
    sta = dev[0].own_addr()
    filt = "wlan.fc.type == 2 && " + \
           "wlan.da == " + sta + " && " + \
           "wlan.sa == " + ap + " && " + \
           "wlan.fc.protected == 1"
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

def test_ap_ft_psk_file(dev, apdev):
    """WPA2-PSK-FT AP with PSK from a file"""
    ssid = "test-ft"
    passphrase = "12345678"

    params = ft_params1a(ssid=ssid, passphrase=passphrase)
    params['wpa_psk_file'] = 'hostapd.wpa_psk'
    hapd = hostapd.add_ap(apdev[0], params)

    dev[1].connect(ssid, psk="very secret",
                   key_mgmt="FT-PSK", proto="WPA2", ieee80211w="1",
                   scan_freq="2412", wait_connect=False)
    dev[0].connect(ssid, psk=passphrase, key_mgmt="FT-PSK", proto="WPA2",
                   ieee80211w="1", scan_freq="2412")
    dev[0].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected()
    dev[0].connect(ssid, psk="very secret", key_mgmt="FT-PSK", proto="WPA2",
                   ieee80211w="1", scan_freq="2412")
    dev[0].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected()
    dev[0].connect(ssid, psk="secret passphrase",
                   key_mgmt="FT-PSK", proto="WPA2", ieee80211w="1",
                   scan_freq="2412")
    dev[2].connect(ssid, psk="another passphrase for all STAs",
                   key_mgmt="FT-PSK", proto="WPA2", ieee80211w="1",
                   scan_freq="2412")
    ev = dev[1].wait_event(["WPA: 4-Way Handshake failed"], timeout=10)
    if ev is None:
        raise Exception("Timed out while waiting for failure report")
    dev[1].request("REMOVE_NETWORK all")

def test_ap_ft_eap_ap_config_change(dev, apdev):
    """WPA2-EAP-FT AP changing from 802.1X-only to FT-only"""
    ssid = "test-ft"
    passphrase = "12345678"
    bssid = apdev[0]['bssid']

    radius = hostapd.radius_params()
    params = ft_params1(ssid=ssid, passphrase=passphrase, discovery=True)
    params['wpa_key_mgmt'] = "WPA-EAP"
    params["ieee8021x"] = "1"
    params["pmk_r1_push"] = "0"
    params["r0kh"] = "ff:ff:ff:ff:ff:ff * 00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff"
    params["r1kh"] = "00:00:00:00:00:00 00:00:00:00:00:00 00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff"
    params["eap_server"] = "0"
    params = dict(list(radius.items()) + list(params.items()))
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].connect(ssid, key_mgmt="FT-EAP WPA-EAP", proto="WPA2",
                   eap="GPSK", identity="gpsk user",
                   password="abcdefghijklmnop0123456789abcdef",
                   scan_freq="2412")
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()
    dev[0].dump_monitor()

    hapd.disable()
    hapd.set('wpa_key_mgmt', "FT-EAP")
    hapd.enable()

    dev[0].request("BSS_FLUSH 0")
    dev[0].scan_for_bss(bssid, 2412, force_scan=True, only_new=True)

    dev[0].request("RECONNECT")
    dev[0].wait_connected()

def test_ap_ft_eap_sha384(dev, apdev):
    """WPA2-EAP-FT with SHA384"""
    ssid = "test-ft"
    passphrase = "12345678"

    radius = hostapd.radius_params()
    params = ft_params1(ssid=ssid, passphrase=passphrase)
    params["ieee80211w"] = "2"
    params['wpa_key_mgmt'] = "FT-EAP-SHA384"
    params["ieee8021x"] = "1"
    params = dict(list(radius.items()) + list(params.items()))
    hapd0 = hostapd.add_ap(apdev[0], params)
    conf = hapd0.request("GET_CONFIG")
    if "key_mgmt=FT-EAP-SHA384" not in conf.splitlines():
        logger.info("GET_CONFIG:\n" + conf)
        raise Exception("GET_CONFIG did not report correct key_mgmt")
    params = ft_params2(ssid=ssid, passphrase=passphrase)
    params["ieee80211w"] = "2"
    params['wpa_key_mgmt'] = "FT-EAP-SHA384"
    params["ieee8021x"] = "1"
    params = dict(list(radius.items()) + list(params.items()))
    hapd1 = hostapd.add_ap(apdev[1], params)

    run_roams(dev[0], apdev, hapd0, hapd1, ssid, passphrase, eap=True,
              sha384=True)

def test_ap_ft_eap_sha384_reassoc(dev, apdev):
    """WPA2-EAP-FT with SHA384 using REASSOCIATE"""
    check_suite_b_192_capa(dev)
    ssid = "test-ft"
    passphrase = "12345678"

    radius = hostapd.radius_params()
    params = ft_params1(ssid=ssid, passphrase=passphrase)
    params["ieee80211w"] = "2"
    params['wpa_key_mgmt'] = "WPA-EAP-SUITE-B-192 FT-EAP-SHA384"
    params["ieee8021x"] = "1"
    params = dict(list(radius.items()) + list(params.items()))
    hapd0 = hostapd.add_ap(apdev[0], params)
    params = ft_params2(ssid=ssid, passphrase=passphrase)
    params["ieee80211w"] = "2"
    params['wpa_key_mgmt'] = "WPA-EAP-SUITE-B-192 FT-EAP-SHA384"
    params["ieee8021x"] = "1"
    params = dict(list(radius.items()) + list(params.items()))
    hapd1 = hostapd.add_ap(apdev[1], params)

    run_roams(dev[0], apdev, hapd0, hapd1, ssid, passphrase, eap=True,
              sha384=True, also_non_ft=True, roam_with_reassoc=True)

def test_ap_ft_eap_sha384_over_ds(dev, apdev):
    """WPA2-EAP-FT with SHA384 over DS"""
    ssid = "test-ft"
    passphrase = "12345678"

    radius = hostapd.radius_params()
    params = ft_params1(ssid=ssid, passphrase=passphrase)
    params["ieee80211w"] = "2"
    params['wpa_key_mgmt'] = "FT-EAP-SHA384"
    params["ieee8021x"] = "1"
    params = dict(list(radius.items()) + list(params.items()))
    hapd0 = hostapd.add_ap(apdev[0], params)
    params = ft_params2(ssid=ssid, passphrase=passphrase)
    params["ieee80211w"] = "2"
    params['wpa_key_mgmt'] = "FT-EAP-SHA384"
    params["ieee8021x"] = "1"
    params = dict(list(radius.items()) + list(params.items()))
    hapd1 = hostapd.add_ap(apdev[1], params)

    run_roams(dev[0], apdev, hapd0, hapd1, ssid, passphrase, over_ds=True,
              eap=True, sha384=True)

def test_ap_ft_roam_rrm(dev, apdev):
    """WPA2-PSK-FT AP and radio measurement request"""
    ssid = "test-ft"
    passphrase = "12345678"

    params = ft_params1(ssid=ssid, passphrase=passphrase)
    params["rrm_beacon_report"] = "1"
    hapd0 = hostapd.add_ap(apdev[0], params)
    bssid0 = hapd0.own_addr()

    addr = dev[0].own_addr()
    dev[0].connect(ssid, psk=passphrase, key_mgmt="FT-PSK", proto="WPA2",
                   scan_freq="2412")
    check_beacon_req(hapd0, addr, 1)

    params = ft_params2(ssid=ssid, passphrase=passphrase)
    params["rrm_beacon_report"] = "1"
    hapd1 = hostapd.add_ap(apdev[1], params)
    bssid1 = hapd1.own_addr()

    dev[0].scan_for_bss(bssid1, freq=2412)
    dev[0].roam(bssid1)
    check_beacon_req(hapd1, addr, 2)

    dev[0].scan_for_bss(bssid0, freq=2412)
    dev[0].roam(bssid0)
    check_beacon_req(hapd0, addr, 3)

def test_ap_ft_pmksa_caching(dev, apdev):
    """FT-EAP and PMKSA caching for initial mobility domain association"""
    ssid = "test-ft"
    identity = "gpsk user"

    radius = hostapd.radius_params()
    params = ft_params1(ssid=ssid)
    params['wpa_key_mgmt'] = "FT-EAP"
    params["ieee8021x"] = "1"
    params["mobility_domain"] = "c3d4"
    params = dict(list(radius.items()) + list(params.items()))
    hapd = hostapd.add_ap(apdev[0], params)

    params = ft_params2(ssid=ssid)
    params['wpa_key_mgmt'] = "FT-EAP"
    params["ieee8021x"] = "1"
    params["mobility_domain"] = "c3d4"
    params = dict(list(radius.items()) + list(params.items()))
    hapd1 = hostapd.add_ap(apdev[1], params)

    run_roams(dev[0], apdev, hapd, hapd1, ssid, None, eap=True,
              eap_identity=identity, pmksa_caching=True)

def test_ap_ft_pmksa_caching_sha384(dev, apdev):
    """FT-EAP-SHA384 and PMKSA caching for initial mobility domain association"""
    ssid = "test-ft"
    identity = "gpsk user"

    radius = hostapd.radius_params()
    params = ft_params1(ssid=ssid)
    params['wpa_key_mgmt'] = "FT-EAP-SHA384"
    params["ieee8021x"] = "1"
    params["mobility_domain"] = "c3d4"
    params = dict(list(radius.items()) + list(params.items()))
    hapd = hostapd.add_ap(apdev[0], params)

    params = ft_params2(ssid=ssid)
    params['wpa_key_mgmt'] = "FT-EAP-SHA384"
    params["ieee8021x"] = "1"
    params["mobility_domain"] = "c3d4"
    params = dict(list(radius.items()) + list(params.items()))
    hapd1 = hostapd.add_ap(apdev[1], params)

    run_roams(dev[0], apdev, hapd, hapd1, ssid, None, eap=True,
              eap_identity=identity, pmksa_caching=True, sha384=True)

def test_ap_ft_r1_key_expiration(dev, apdev):
    """WPA2-PSK-FT and PMK-R1 expiration"""
    ssid = "test-ft"
    passphrase = "12345678"

    params = ft_params1(ssid=ssid, passphrase=passphrase)
    params['r1_max_key_lifetime'] = "2"
    hapd0 = hostapd.add_ap(apdev[0], params)
    params = ft_params2(ssid=ssid, passphrase=passphrase)
    params['r1_max_key_lifetime'] = "2"
    hapd1 = hostapd.add_ap(apdev[1], params)

    # This succeeds, but results in having to run another PMK-R1 pull before the
    # second AP can complete FT protocol.
    run_roams(dev[0], apdev, hapd0, hapd1, ssid, passphrase, wait_before_roam=4)

def test_ap_ft_r0_key_expiration(dev, apdev):
    """WPA2-PSK-FT and PMK-R0 expiration"""
    ssid = "test-ft"
    passphrase = "12345678"

    params = ft_params1(ssid=ssid, passphrase=passphrase)
    params.pop('r0_key_lifetime', None)
    params['ft_r0_key_lifetime'] = "2"
    hapd0 = hostapd.add_ap(apdev[0], params)
    params = ft_params2(ssid=ssid, passphrase=passphrase)
    params.pop('r0_key_lifetime', None)
    params['ft_r0_key_lifetime'] = "2"
    hapd1 = hostapd.add_ap(apdev[1], params)

    bssid2 = run_roams(dev[0], apdev, hapd0, hapd1, ssid, passphrase,
                       return_after_initial=True)
    time.sleep(4)
    dev[0].scan_for_bss(bssid2, freq="2412")
    if "OK" not in dev[0].request("ROAM " + bssid2):
        raise Exception("ROAM failed")
    ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED",
                            "CTRL-EVENT-AUTH-REJECT",
                            "CTRL-EVENT-ASSOC-REJECT"], timeout=5)
    dev[0].request("DISCONNECT")
    if ev is None or "CTRL-EVENT-AUTH-REJECT" not in ev:
        raise Exception("FT protocol failure not reported")
    if "status_code=53" not in ev:
        raise Exception("Unexpected status in FT protocol failure: " + ev)

    # Generate a new PMK-R0
    dev[0].dump_monitor()
    dev[0].request("RECONNECT")
    dev[0].wait_connected()

def test_ap_ft_no_full_ap_client_state(dev, apdev):
    """WPA2-PSK-FT AP with full_ap_client_state=0"""
    run_ap_ft_skip_prune_assoc(dev, apdev, False, False)

def test_ap_ft_skip_prune_assoc(dev, apdev):
    """WPA2-PSK-FT AP with skip_prune_assoc"""
    run_ap_ft_skip_prune_assoc(dev, apdev, True, True)

def test_ap_ft_skip_prune_assoc2(dev, apdev):
    """WPA2-PSK-FT AP with skip_prune_assoc (disable full_ap_client_state)"""
    run_ap_ft_skip_prune_assoc(dev, apdev, True, False, test_connectivity=False)

def test_ap_ft_skip_prune_assoc_pmf(dev, apdev):
    """WPA2-PSK-FT/PMF AP with skip_prune_assoc"""
    run_ap_ft_skip_prune_assoc(dev, apdev, True, True, pmf=True)

def test_ap_ft_skip_prune_assoc_pmf_over_ds(dev, apdev):
    """WPA2-PSK-FT/PMF AP with skip_prune_assoc (over DS)"""
    run_ap_ft_skip_prune_assoc(dev, apdev, True, True, pmf=True, over_ds=True)

def run_ap_ft_skip_prune_assoc(dev, apdev, skip_prune_assoc,
                               full_ap_client_state, test_connectivity=True,
                               pmf=False, over_ds=False):
    ssid = "test-ft"
    passphrase = "12345678"

    params = ft_params1(ssid=ssid, passphrase=passphrase)
    if skip_prune_assoc:
        params['skip_prune_assoc'] = '1'
    if not full_ap_client_state:
        params['driver_params'] = "full_ap_client_state=0"
    if pmf:
        params["ieee80211w"] = "2"
    hapd0 = hostapd.add_ap(apdev[0], params)
    params = ft_params2(ssid=ssid, passphrase=passphrase)
    if skip_prune_assoc:
        params['skip_prune_assoc'] = '1'
    if not full_ap_client_state:
        params['driver_params'] = "full_ap_client_state=0"
    if pmf:
        params["ieee80211w"] = "2"
    hapd1 = hostapd.add_ap(apdev[1], params)

    run_roams(dev[0], apdev, hapd0, hapd1, ssid, passphrase,
              ieee80211w="2" if pmf else "0",
              over_ds=over_ds, test_connectivity=test_connectivity)

def test_ap_ft_sae_skip_prune_assoc(dev, apdev):
    """WPA2-PSK-FT-SAE AP with skip_prune_assoc"""
    hapd0, hapd1 = start_ft_sae(dev[0], apdev, skip_prune_assoc=True)
    run_roams(dev[0], apdev, hapd0, hapd1, "test-ft", "12345678", sae=True)
