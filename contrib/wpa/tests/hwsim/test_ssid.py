# -*- coding: utf-8 -*-
# SSID contents and encoding tests
# Copyright (c) 2013-2014, Jouni Malinen <j@w1.fi>
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

from remotehost import remote_compatible
import logging
logger = logging.getLogger()

import hostapd

@remote_compatible
def test_ssid_hex_encoded(dev, apdev):
    """SSID configuration using hex encoded version"""
    hostapd.add_ap(apdev[0], {"ssid2": '68656c6c6f'})
    dev[0].connect("hello", key_mgmt="NONE", scan_freq="2412")
    dev[1].connect(ssid2="68656c6c6f", key_mgmt="NONE", scan_freq="2412")

def test_ssid_printf_encoded(dev, apdev):
    """SSID configuration using printf encoded version"""
    hostapd.add_ap(apdev[0], {"ssid2": 'P"\\0hello\\nthere"'})
    dev[0].connect(ssid2="0068656c6c6f0a7468657265", key_mgmt="NONE",
                   scan_freq="2412")
    dev[1].connect(ssid2='P"\\x00hello\\nthere"', key_mgmt="NONE",
                   scan_freq="2412")
    ssid = dev[0].get_status_field("ssid")
    bss = dev[1].get_bss(apdev[0]['bssid'])
    if ssid != bss['ssid']:
        raise Exception("Unexpected difference in SSID")
    dev[2].connect(ssid2='P"' + ssid + '"', key_mgmt="NONE", scan_freq="2412")

@remote_compatible
def test_ssid_1_octet(dev, apdev):
    """SSID with one octet"""
    hostapd.add_ap(apdev[0], {"ssid": '1'})
    dev[0].connect("1", key_mgmt="NONE", scan_freq="2412")

@remote_compatible
def test_ssid_32_octets(dev, apdev):
    """SSID with 32 octets"""
    hostapd.add_ap(apdev[0],
                   {"ssid": '1234567890abcdef1234567890ABCDEF'})
    dev[0].connect("1234567890abcdef1234567890ABCDEF", key_mgmt="NONE",
                   scan_freq="2412")

def test_ssid_32_octets_nul_term(dev, apdev):
    """SSID with 32 octets with nul at the end"""
    ssid = 'P"1234567890abcdef1234567890ABCDE\\x00"'
    hostapd.add_ap(apdev[0],
                   {"ssid2": ssid})
    dev[0].connect(ssid2=ssid, key_mgmt="NONE", scan_freq="2412")

@remote_compatible
def test_ssid_utf8(dev, apdev):
    """SSID with UTF8 encoding"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": 'testi-åäöÅÄÖ-testi',
                                     "utf8_ssid": "1"})
    dev[0].connect("testi-åäöÅÄÖ-testi", key_mgmt="NONE", scan_freq="2412")
    dev[1].connect(ssid2="74657374692dc3a5c3a4c3b6c385c384c3962d7465737469",
                   key_mgmt="NONE", scan_freq="2412")
    # verify ctrl_iface for coverage
    addrs = [dev[0].p2p_interface_addr(), dev[1].p2p_interface_addr()]
    sta = hapd.get_sta(None)
    if sta['addr'] not in addrs:
        raise Exception("Unexpected STA address")
    sta2 = hapd.get_sta(sta['addr'], next=True)
    if sta2['addr'] not in addrs:
        raise Exception("Unexpected STA2 address")
    sta3 = hapd.get_sta(sta2['addr'], next=True)
    if len(sta3) != 0:
        raise Exception("Unexpected STA iteration result (did not stop)")

    if "[UTF-8]" not in dev[0].get_bss(hapd.own_addr())['flags']:
        raise Exception("[UTF-8] flag not included in BSS")
    if "[UTF-8]" not in dev[0].request("SCAN_RESULTS"):
        raise Exception("[UTF-8] flag not included in SCAN_RESULTS")

def clear_scan_cache2(hapd, dev):
    # clear BSS table to avoid issues in following test cases
    dev[0].request("REMOVE_NETWORK all")
    dev[1].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected()
    hapd.disable()
    dev[0].flush_scan_cache()
    dev[1].flush_scan_cache()

@remote_compatible
def test_ssid_hidden(dev, apdev):
    """Hidden SSID"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": 'secret',
                                     "ignore_broadcast_ssid": "1"})
    dev[1].connect("secret", key_mgmt="NONE", scan_freq="2412",
                   wait_connect=False)
    dev[0].connect("secret", key_mgmt="NONE", scan_freq="2412", scan_ssid="1")
    ev = dev[1].wait_event(["CTRL-EVENT-CONNECTED"], timeout=1)
    if ev is not None:
        raise Exception("Unexpected connection")
    clear_scan_cache2(hapd, dev)

@remote_compatible
def test_ssid_hidden2(dev, apdev):
    """Hidden SSID using zero octets as payload"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": 'secret2',
                                     "ignore_broadcast_ssid": "2"})
    dev[1].connect("secret2", key_mgmt="NONE", scan_freq="2412",
                   wait_connect=False)
    dev[0].connect("secret2", key_mgmt="NONE", scan_freq="2412", scan_ssid="1")
    ev = dev[1].wait_event(["CTRL-EVENT-CONNECTED"], timeout=1)
    if ev is not None:
        raise Exception("Unexpected connection")
    clear_scan_cache2(hapd, dev)

@remote_compatible
def test_ssid_hidden_wpa2(dev, apdev):
    """Hidden SSID with WPA2-PSK"""
    params = hostapd.wpa2_params(ssid="secret", passphrase="12345678")
    params["ignore_broadcast_ssid"] = "1"
    hapd = hostapd.add_ap(apdev[0], params)
    dev[1].connect("secret", psk="12345678", scan_freq="2412",
                   wait_connect=False)
    dev[0].connect("secret", psk="12345678", scan_freq="2412", scan_ssid="1")
    ev = dev[1].wait_event(["CTRL-EVENT-CONNECTED"], timeout=1)
    if ev is not None:
        raise Exception("Unexpected connection")
    clear_scan_cache2(hapd, dev)
