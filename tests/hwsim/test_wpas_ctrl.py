# wpa_supplicant control interface
# Copyright (c) 2014, Qualcomm Atheros, Inc.
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

from remotehost import remote_compatible
import logging
logger = logging.getLogger()
import os
import socket
import subprocess
import time
import binascii

import hostapd
import hwsim_utils
from hwsim import HWSimRadio
from wpasupplicant import WpaSupplicant
from utils import *
from test_wpas_ap import wait_ap_ready

@remote_compatible
def test_wpas_ctrl_network(dev):
    """wpa_supplicant ctrl_iface network set/get"""
    skip_without_tkip(dev[0])
    id = dev[0].add_network()

    if "FAIL" not in dev[0].request("SET_NETWORK " + str(id)):
        raise Exception("Unexpected success for invalid SET_NETWORK")
    if "FAIL" not in dev[0].request("SET_NETWORK " + str(id) + " name"):
        raise Exception("Unexpected success for invalid SET_NETWORK")
    if "FAIL" not in dev[0].request("SET_NETWORK " + str(id + 1) + " proto OPEN"):
        raise Exception("Unexpected success for invalid network id")
    if "FAIL" not in dev[0].request("GET_NETWORK " + str(id)):
        raise Exception("Unexpected success for invalid GET_NETWORK")
    if "FAIL" not in dev[0].request("GET_NETWORK " + str(id + 1) + " proto"):
        raise Exception("Unexpected success for invalid network id")

    if "OK" not in dev[0].request("SET_NETWORK " + str(id) + " proto  \t WPA2 "):
        raise Exception("Unexpected failure for SET_NETWORK proto")
    res = dev[0].request("GET_NETWORK " + str(id) + " proto")
    if res != "RSN":
        raise Exception("Unexpected SET_NETWORK/GET_NETWORK conversion for proto: " + res)

    if "OK" not in dev[0].request("SET_NETWORK " + str(id) + " key_mgmt  \t WPA-PSK "):
        raise Exception("Unexpected success for SET_NETWORK key_mgmt")
    res = dev[0].request("GET_NETWORK " + str(id) + " key_mgmt")
    if res != "WPA-PSK":
        raise Exception("Unexpected SET_NETWORK/GET_NETWORK conversion for key_mgmt: " + res)

    if "OK" not in dev[0].request("SET_NETWORK " + str(id) + " auth_alg  \t OPEN "):
        raise Exception("Unexpected failure for SET_NETWORK auth_alg")
    res = dev[0].request("GET_NETWORK " + str(id) + " auth_alg")
    if res != "OPEN":
        raise Exception("Unexpected SET_NETWORK/GET_NETWORK conversion for auth_alg: " + res)

    if "OK" not in dev[0].request("SET_NETWORK " + str(id) + " eap  \t TLS "):
        raise Exception("Unexpected failure for SET_NETWORK eap")
    res = dev[0].request("GET_NETWORK " + str(id) + " eap")
    if res != "TLS":
        raise Exception("Unexpected SET_NETWORK/GET_NETWORK conversion for eap: " + res)

    tests = ("bssid foo", "key_mgmt foo", "key_mgmt ", "group NONE")
    for t in tests:
        if "FAIL" not in dev[0].request("SET_NETWORK " + str(id) + " " + t):
            raise Exception("Unexpected success for invalid SET_NETWORK: " + t)

    tests = [("key_mgmt", "WPA-PSK WPA-EAP IEEE8021X NONE WPA-NONE FT-PSK FT-EAP WPA-PSK-SHA256 WPA-EAP-SHA256"),
             ("pairwise", "CCMP-256 GCMP-256 CCMP GCMP TKIP"),
             ("group", "CCMP-256 GCMP-256 CCMP GCMP TKIP"),
             ("auth_alg", "OPEN SHARED LEAP"),
             ("scan_freq", "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15"),
             ("freq_list", "2412 2417"),
             ("scan_ssid", "1"),
             ("bssid", "00:11:22:33:44:55"),
             ("proto", "WPA RSN OSEN"),
             ("eap", "TLS"),
             ("go_p2p_dev_addr", "22:33:44:55:66:aa"),
             ("p2p_client_list", "22:33:44:55:66:bb 02:11:22:33:44:55")]
    if "SAE" not in dev[0].get_capability("auth_alg"):
        tests.append(("key_mgmt", "WPS OSEN"))
    else:
        tests.append(("key_mgmt", "WPS SAE FT-SAE OSEN"))

    dev[0].set_network_quoted(id, "ssid", "test")
    for field, value in tests:
        dev[0].set_network(id, field, value)
        res = dev[0].get_network(id, field)
        if res != value:
            raise Exception("Unexpected response for '" + field + "': '" + res + "'")

    try:
        value = "WPA-EAP-SUITE-B WPA-EAP-SUITE-B-192"
        dev[0].set_network(id, "key_mgmt", value)
        res = dev[0].get_network(id, "key_mgmt")
        if res != value:
            raise Exception("Unexpected response for key_mgmt")
    except Exception as e:
        if str(e).startswith("Unexpected"):
            raise
        else:
            pass

    q_tests = (("identity", "hello"),
               ("anonymous_identity", "foo@nowhere.com"))
    for field, value in q_tests:
        dev[0].set_network_quoted(id, field, value)
        res = dev[0].get_network(id, field)
        if res != '"' + value + '"':
            raise Exception("Unexpected quoted response for '" + field + "': '" + res + "'")

    get_tests = (("foo", None), ("ssid", '"test"'))
    for field, value in get_tests:
        res = dev[0].get_network(id, field)
        if res != value:
            raise Exception("Unexpected response for '" + field + "': '" + res + "'")

    if dev[0].get_network(id, "password"):
        raise Exception("Unexpected response for 'password'")
    dev[0].set_network_quoted(id, "password", "foo")
    if dev[0].get_network(id, "password") != '*':
        raise Exception("Unexpected response for 'password' (expected *)")
    dev[0].set_network(id, "password", "hash:12345678901234567890123456789012")
    if dev[0].get_network(id, "password") != '*':
        raise Exception("Unexpected response for 'password' (expected *)")
    dev[0].set_network(id, "password", "NULL")
    if dev[0].get_network(id, "password"):
        raise Exception("Unexpected response for 'password'")
    if "FAIL" not in dev[0].request("SET_NETWORK " + str(id) + " password hash:12"):
        raise Exception("Unexpected success for invalid password hash")
    if "FAIL" not in dev[0].request("SET_NETWORK " + str(id) + " password hash:123456789012345678x0123456789012"):
        raise Exception("Unexpected success for invalid password hash")

    dev[0].set_network(id, "identity", "414243")
    if dev[0].get_network(id, "identity") != '"ABC"':
        raise Exception("Unexpected identity hex->text response")

    dev[0].set_network(id, "identity", 'P"abc\ndef"')
    if dev[0].get_network(id, "identity") != "6162630a646566":
        raise Exception("Unexpected identity printf->hex response")

    if "FAIL" not in dev[0].request("SET_NETWORK " + str(id) + ' identity P"foo'):
        raise Exception("Unexpected success for invalid identity string")

    if "FAIL" not in dev[0].request("SET_NETWORK " + str(id) + ' identity 12x3'):
        raise Exception("Unexpected success for invalid identity string")

    if "WEP40" in dev[0].get_capability("group"):
        for i in range(0, 4):
            if "FAIL" in dev[0].request("SET_NETWORK " + str(id) + ' wep_key' + str(i) + ' aabbccddee'):
                raise Exception("Unexpected wep_key set failure")
            if dev[0].get_network(id, "wep_key" + str(i)) != '*':
                raise Exception("Unexpected wep_key get failure")

    if "FAIL" in dev[0].request("SET_NETWORK " + str(id) + ' psk_list P2P-00:11:22:33:44:55-0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef'):
        raise Exception("Unexpected failure for psk_list string")

    if "FAIL" not in dev[0].request("SET_NETWORK " + str(id) + ' psk_list 00:11:x2:33:44:55-0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef'):
        raise Exception("Unexpected success for invalid psk_list string")

    if "FAIL" not in dev[0].request("SET_NETWORK " + str(id) + ' psk_list P2P-00:11:x2:33:44:55-0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef'):
        raise Exception("Unexpected success for invalid psk_list string")

    if "FAIL" not in dev[0].request("SET_NETWORK " + str(id) + ' psk_list P2P-00:11:22:33:44:55+0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef'):
        raise Exception("Unexpected success for invalid psk_list string")

    if "FAIL" not in dev[0].request("SET_NETWORK " + str(id) + ' psk_list P2P-00:11:22:33:44:55-0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcde'):
        raise Exception("Unexpected success for invalid psk_list string")

    if "FAIL" not in dev[0].request("SET_NETWORK " + str(id) + ' psk_list P2P-00:11:22:33:44:55-0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdex'):
        raise Exception("Unexpected success for invalid psk_list string")

    if dev[0].get_network(id, "psk_list"):
        raise Exception("Unexpected psk_list get response")

    if dev[0].list_networks()[0]['ssid'] != "test":
        raise Exception("Unexpected ssid in LIST_NETWORKS")
    dev[0].set_network(id, "ssid", "NULL")
    if dev[0].list_networks()[0]['ssid'] != "":
        raise Exception("Unexpected ssid in LIST_NETWORKS after clearing it")

    if "FAIL" not in dev[0].request('SET_NETWORK ' + str(id) + ' ssid "0123456789abcdef0123456789abcdef0"'):
        raise Exception("Too long SSID accepted")
    if "FAIL" not in dev[0].request('SET_NETWORK ' + str(id) + ' scan_ssid qwerty'):
        raise Exception("Invalid integer accepted")
    if "FAIL" not in dev[0].request('SET_NETWORK ' + str(id) + ' scan_ssid 2'):
        raise Exception("Too large integer accepted")
    if "FAIL" not in dev[0].request('SET_NETWORK ' + str(id) + ' psk 12345678'):
        raise Exception("Invalid PSK accepted")
    if "FAIL" not in dev[0].request('SET_NETWORK ' + str(id) + ' psk "1234567"'):
        raise Exception("Too short PSK accepted")
    if "FAIL" not in dev[0].request('SET_NETWORK ' + str(id) + ' psk "1234567890123456789012345678901234567890123456789012345678901234"'):
        raise Exception("Too long PSK accepted")
    dev[0].set_network_quoted(id, "psk", "123456768")
    dev[0].set_network_quoted(id, "psk", "123456789012345678901234567890123456789012345678901234567890123")
    if dev[0].get_network(id, "psk") != '*':
        raise Exception("Unexpected psk read result")

    if "FAIL" not in dev[0].request('SET_NETWORK ' + str(id) + ' eap UNKNOWN'):
        raise Exception("Unknown EAP method accepted")

    if "FAIL" not in dev[0].request('SET_NETWORK ' + str(id) + ' password "foo'):
        raise Exception("Invalid password accepted")

    if "FAIL" not in dev[0].request('SET_NETWORK ' + str(id) + ' wep_key0 "foo'):
        raise Exception("Invalid WEP key accepted")
    if "FAIL" not in dev[0].request('SET_NETWORK ' + str(id) + ' wep_key0 "12345678901234567"'):
        raise Exception("Too long WEP key accepted")
    if "WEP40" in dev[0].get_capability("group"):
        # too short WEP key is ignored
        dev[0].set_network_quoted(id, "wep_key0", "1234")
        dev[0].set_network_quoted(id, "wep_key1", "12345")
        dev[0].set_network_quoted(id, "wep_key2", "1234567890123")
        dev[0].set_network_quoted(id, "wep_key3", "1234567890123456")

    dev[0].set_network(id, "go_p2p_dev_addr", "any")
    if dev[0].get_network(id, "go_p2p_dev_addr") is not None:
        raise Exception("Unexpected go_p2p_dev_addr value")
    if "FAIL" not in dev[0].request('SET_NETWORK ' + str(id) + ' go_p2p_dev_addr 00:11:22:33:44'):
        raise Exception("Invalid go_p2p_dev_addr accepted")
    if "FAIL" not in dev[0].request('SET_NETWORK ' + str(id) + ' p2p_client_list 00:11:22:33:44'):
        raise Exception("Invalid p2p_client_list accepted")
    if "FAIL" in dev[0].request('SET_NETWORK ' + str(id) + ' p2p_client_list 00:11:22:33:44:55 00:1'):
        raise Exception("p2p_client_list truncation workaround failed")
    if dev[0].get_network(id, "p2p_client_list") != "00:11:22:33:44:55":
        raise Exception("p2p_client_list truncation workaround did not work")

    if "FAIL" not in dev[0].request('SET_NETWORK ' + str(id) + ' auth_alg '):
        raise Exception("Empty auth_alg accepted")
    if "FAIL" not in dev[0].request('SET_NETWORK ' + str(id) + ' auth_alg FOO'):
        raise Exception("Invalid auth_alg accepted")

    if "FAIL" not in dev[0].request('SET_NETWORK ' + str(id) + ' proto '):
        raise Exception("Empty proto accepted")
    if "FAIL" not in dev[0].request('SET_NETWORK ' + str(id) + ' proto FOO'):
        raise Exception("Invalid proto accepted")

    if "FAIL" not in dev[0].request('SET_NETWORK ' + str(id) + ' pairwise '):
        raise Exception("Empty pairwise accepted")
    if "FAIL" not in dev[0].request('SET_NETWORK ' + str(id) + ' pairwise FOO'):
        raise Exception("Invalid pairwise accepted")
    if "FAIL" not in dev[0].request('SET_NETWORK ' + str(id) + ' pairwise WEP40'):
        raise Exception("Invalid pairwise accepted")

    if "OK" not in dev[0].request('BSSID ' + str(id) + ' 00:11:22:33:44:55'):
        raise Exception("Unexpected BSSID failure")
    if dev[0].request("GET_NETWORK 0 bssid") != '00:11:22:33:44:55':
        raise Exception("BSSID command did not set network bssid")
    if "OK" not in dev[0].request('BSSID ' + str(id) + ' 00:00:00:00:00:00'):
        raise Exception("Unexpected BSSID failure")
    if "FAIL" not in dev[0].request("GET_NETWORK 0 bssid"):
        raise Exception("bssid claimed configured after clearing")
    if "FAIL" not in dev[0].request('BSSID 123 00:11:22:33:44:55'):
        raise Exception("Unexpected BSSID success")
    if "FAIL" not in dev[0].request('BSSID ' + str(id) + ' 00:11:22:33:44'):
        raise Exception("Unexpected BSSID success")
    if "FAIL" not in dev[0].request('BSSID ' + str(id)):
        raise Exception("Unexpected BSSID success")

    tests = ["02:11:22:33:44:55",
             "02:11:22:33:44:55 02:ae:be:ce:53:77",
             "02:11:22:33:44:55/ff:00:ff:00:ff:00",
             "02:11:22:33:44:55/ff:00:ff:00:ff:00 f2:99:88:77:66:55",
             "f2:99:88:77:66:55 02:11:22:33:44:55/ff:00:ff:00:ff:00",
             "f2:99:88:77:66:55 02:11:22:33:44:55/ff:00:ff:00:ff:00 12:34:56:78:90:ab",
             "02:11:22:33:44:55/ff:ff:ff:00:00:00 02:ae:be:ce:53:77/00:00:00:00:00:ff"]
    for val in tests:
        dev[0].set_network(id, "bssid_ignore", val)
        res = dev[0].get_network(id, "bssid_ignore")
        if res != val:
            raise Exception("Unexpected bssid_ignore value: %s != %s" % (res, val))
        dev[0].set_network(id, "bssid_accept", val)
        res = dev[0].get_network(id, "bssid_accept")
        if res != val:
            raise Exception("Unexpected bssid_accept value: %s != %s" % (res, val))

    tests = ["foo",
             "00:11:22:33:44:5",
             "00:11:22:33:44:55q",
             "00:11:22:33:44:55/",
             "00:11:22:33:44:55/66:77:88:99:aa:b"]
    for val in tests:
        if "FAIL" not in dev[0].request("SET_NETWORK %d bssid_ignore %s" % (id, val)):
            raise Exception("Invalid bssid_ignore value accepted")

@remote_compatible
def test_wpas_ctrl_network_oom(dev):
    """wpa_supplicant ctrl_iface network OOM in string parsing"""
    id = dev[0].add_network()

    tests = [('"foo"', 1, 'dup_binstr;wpa_config_set'),
             ('P"foo"', 1, 'dup_binstr;wpa_config_set'),
             ('P"foo"', 2, 'wpa_config_set'),
             ('112233', 1, 'wpa_config_set')]
    for val, count, func in tests:
        with alloc_fail(dev[0], count, func):
            if "FAIL" not in dev[0].request("SET_NETWORK " + str(id) + ' ssid ' + val):
                raise Exception("Unexpected success for SET_NETWORK during OOM")

@remote_compatible
def test_wpas_ctrl_many_networks(dev, apdev):
    """wpa_supplicant ctrl_iface LIST_NETWORKS with huge number of networks"""
    for i in range(1000):
        id = dev[0].add_network()
    res = dev[0].request("LIST_NETWORKS")
    if str(id) in res:
        raise Exception("Last added network was unexpectedly included")
    res = dev[0].request("LIST_NETWORKS LAST_ID=%d" % (id - 2))
    if str(id) not in res:
        raise Exception("Last added network was not present when using LAST_ID")
    # This command can take a very long time under valgrind testing on a low
    # power CPU, so increase the command timeout significantly to avoid issues
    # with the test case failing and following reset operation timing out.
    dev[0].request("REMOVE_NETWORK all", timeout=60)

@remote_compatible
def test_wpas_ctrl_dup_network(dev, apdev):
    """wpa_supplicant ctrl_iface DUP_NETWORK"""
    ssid = "target"
    passphrase = 'qwertyuiop'
    params = hostapd.wpa2_params(ssid=ssid, passphrase=passphrase)
    hostapd.add_ap(apdev[0], params)

    src = dev[0].connect("another", psk=passphrase, scan_freq="2412",
                         only_add_network=True)
    id = dev[0].add_network()
    dev[0].set_network_quoted(id, "ssid", ssid)
    for f in ["key_mgmt", "psk", "scan_freq"]:
        res = dev[0].request("DUP_NETWORK {} {} {}".format(src, id, f))
        if "OK" not in res:
            raise Exception("DUP_NETWORK failed")
    dev[0].connect_network(id)

    if "FAIL" not in dev[0].request("DUP_NETWORK "):
        raise Exception("Unexpected DUP_NETWORK success")
    if "FAIL" not in dev[0].request("DUP_NETWORK %d " % id):
        raise Exception("Unexpected DUP_NETWORK success")
    if "FAIL" not in dev[0].request("DUP_NETWORK %d %d" % (id, id)):
        raise Exception("Unexpected DUP_NETWORK success")
    if "FAIL" not in dev[0].request("DUP_NETWORK 123456 1234567 "):
        raise Exception("Unexpected DUP_NETWORK success")
    if "FAIL" not in dev[0].request("DUP_NETWORK %d 123456 " % id):
        raise Exception("Unexpected DUP_NETWORK success")
    if "FAIL" not in dev[0].request("DUP_NETWORK %d %d foo" % (id, id)):
        raise Exception("Unexpected DUP_NETWORK success")
    dev[0].request("DISCONNECT")
    if "OK" not in dev[0].request("DUP_NETWORK %d %d ssid" % (id, id)):
        raise Exception("Unexpected DUP_NETWORK failure")

@remote_compatible
def test_wpas_ctrl_dup_network_global(dev, apdev):
    """wpa_supplicant ctrl_iface DUP_NETWORK (global)"""
    ssid = "target"
    passphrase = 'qwertyuiop'
    params = hostapd.wpa2_params(ssid=ssid, passphrase=passphrase)
    hostapd.add_ap(apdev[0], params)

    src = dev[0].connect("another", psk=passphrase, scan_freq="2412",
                         only_add_network=True)
    id = dev[0].add_network()
    dev[0].set_network_quoted(id, "ssid", ssid)
    for f in ["key_mgmt", "psk", "scan_freq"]:
        res = dev[0].global_request("DUP_NETWORK {} {} {} {} {}".format(dev[0].ifname, dev[0].ifname, src, id, f))
        if "OK" not in res:
            raise Exception("DUP_NETWORK failed")
    dev[0].connect_network(id)

    if "FAIL" not in dev[0].global_request("DUP_NETWORK "):
        raise Exception("Unexpected DUP_NETWORK success")
    if "FAIL" not in dev[0].global_request("DUP_NETWORK %s" % dev[0].ifname):
        raise Exception("Unexpected DUP_NETWORK success")
    if "FAIL" not in dev[0].global_request("DUP_NETWORK %s %s" % (dev[0].ifname, dev[0].ifname)):
        raise Exception("Unexpected DUP_NETWORK success")
    if "FAIL" not in dev[0].global_request("DUP_NETWORK %s %s %d" % (dev[0].ifname, dev[0].ifname, id)):
        raise Exception("Unexpected DUP_NETWORK success")
    if "FAIL" not in dev[0].global_request("DUP_NETWORK %s %s %d %d" % (dev[0].ifname, dev[0].ifname, id, id)):
        raise Exception("Unexpected DUP_NETWORK success")
    dev[0].request("DISCONNECT")
    if "OK" not in dev[0].global_request("DUP_NETWORK %s %s %d %d ssid" % (dev[0].ifname, dev[0].ifname, id, id)):
        raise Exception("Unexpected DUP_NETWORK failure")

def add_cred(dev):
    id = dev.add_cred()
    ev = dev.wait_event(["CRED-ADDED"])
    if ev is None:
        raise Exception("Missing CRED-ADDED event")
    if " " + str(id) not in ev:
        raise Exception("CRED-ADDED event without matching id")
    return id

def set_cred(dev, id, field, value):
    dev.set_cred(id, field, value)
    ev = dev.wait_event(["CRED-MODIFIED"])
    if ev is None:
        raise Exception("Missing CRED-MODIFIED event")
    if " " + str(id) + " " not in ev:
        raise Exception("CRED-MODIFIED event without matching id")
    if field not in ev:
        raise Exception("CRED-MODIFIED event without matching field")

def set_cred_quoted(dev, id, field, value):
    dev.set_cred_quoted(id, field, value)
    ev = dev.wait_event(["CRED-MODIFIED"])
    if ev is None:
        raise Exception("Missing CRED-MODIFIED event")
    if " " + str(id) + " " not in ev:
        raise Exception("CRED-MODIFIED event without matching id")
    if field not in ev:
        raise Exception("CRED-MODIFIED event without matching field")

def remove_cred(dev, id):
    dev.remove_cred(id)
    ev = dev.wait_event(["CRED-REMOVED"])
    if ev is None:
        raise Exception("Missing CRED-REMOVED event")
    if " " + str(id) not in ev:
        raise Exception("CRED-REMOVED event without matching id")

@remote_compatible
def test_wpas_ctrl_cred(dev):
    """wpa_supplicant ctrl_iface cred set"""
    id1 = add_cred(dev[0])
    if "FAIL" not in dev[0].request("SET_CRED " + str(id1 + 1) + " temporary 1"):
        raise Exception("SET_CRED succeeded unexpectedly on unknown cred id")
    if "FAIL" not in dev[0].request("SET_CRED " + str(id1)):
        raise Exception("Invalid SET_CRED succeeded unexpectedly")
    if "FAIL" not in dev[0].request("SET_CRED " + str(id1) + " temporary"):
        raise Exception("Invalid SET_CRED succeeded unexpectedly")
    if "FAIL" not in dev[0].request("GET_CRED " + str(id1 + 1) + " temporary"):
        raise Exception("GET_CRED succeeded unexpectedly on unknown cred id")
    if "FAIL" not in dev[0].request("GET_CRED " + str(id1)):
        raise Exception("Invalid GET_CRED succeeded unexpectedly")
    if "FAIL" not in dev[0].request("GET_CRED " + str(id1) + " foo"):
        raise Exception("Invalid GET_CRED succeeded unexpectedly")
    id = add_cred(dev[0])
    id2 = add_cred(dev[0])
    set_cred(dev[0], id, "temporary", "1")
    set_cred(dev[0], id, "priority", "1")
    set_cred(dev[0], id, "pcsc", "1")
    set_cred(dev[0], id, "sim_num", "0")
    set_cred_quoted(dev[0], id, "private_key_passwd", "test")
    set_cred_quoted(dev[0], id, "domain_suffix_match", "test")
    set_cred_quoted(dev[0], id, "phase1", "test")
    set_cred_quoted(dev[0], id, "phase2", "test")

    if "FAIL" not in dev[0].request("SET_CRED " + str(id) + " eap FOO"):
        raise Exception("Unexpected success on unknown EAP method")

    if "FAIL" not in dev[0].request("SET_CRED " + str(id) + " username 12xa"):
        raise Exception("Unexpected success on invalid string")

    for i in ("11", "1122", "112233445566778899aabbccddeeff00"):
        if "FAIL" not in dev[0].request("SET_CRED " + str(id) + " roaming_consortium " + i):
            raise Exception("Unexpected success on invalid roaming_consortium")

    dev[0].set_cred(id, "excluded_ssid", "00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff")
    if "FAIL" not in dev[0].request("SET_CRED " + str(id) + " excluded_ssid 00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff00"):
        raise Exception("Unexpected success on invalid excluded_ssid")

    if "FAIL" not in dev[0].request("SET_CRED " + str(id) + " foo 4142"):
        raise Exception("Unexpected success on unknown field")

    tests = ["sp_priority 256",
             'roaming_partner "example.org"',
             'roaming_partner "' + 200*'a' + '.example.org,"',
             'roaming_partner "example.org,1"',
             'roaming_partner "example.org,1,2"',
             'roaming_partner "example.org,1,2,ABC"']
    for t in tests:
        if "FAIL" not in dev[0].request("SET_CRED " + str(id) + " " + t):
            raise Exception("Unexpected success on invalid SET_CRED value: " + t)

    id3 = add_cred(dev[0])
    id4 = add_cred(dev[0])
    if len(dev[0].request("LIST_CREDS").splitlines()) != 6:
        raise Exception("Unexpected LIST_CREDS result(1)")

    remove_cred(dev[0], id1)
    remove_cred(dev[0], id3)
    remove_cred(dev[0], id4)
    remove_cred(dev[0], id2)
    remove_cred(dev[0], id)
    if "FAIL" not in dev[0].request("REMOVE_CRED 1"):
        raise Exception("Unexpected success on invalid remove cred")
    if len(dev[0].request("LIST_CREDS").splitlines()) != 1:
        raise Exception("Unexpected LIST_CREDS result(2)")

    id = add_cred(dev[0])
    values = [("temporary", "1", False),
              ("temporary", "0", False),
              ("pcsc", "1", False),
              ("realm", "example.com", True),
              ("username", "user@example.com", True),
              ("password", "foo", True, "*"),
              ("ca_cert", "ca.pem", True),
              ("client_cert", "user.pem", True),
              ("private_key", "key.pem", True),
              ("private_key_passwd", "foo", True, "*"),
              ("imsi", "310026-000000000", True),
              ("milenage", "foo", True, "*"),
              ("domain_suffix_match", "example.com", True),
              ("domain", "example.com", True),
              ("domain", "example.org", True, "example.com\nexample.org"),
              ("roaming_consortium", "0123456789", False),
              ("required_roaming_consortium", "456789", False),
              ("eap", "TTLS", False),
              ("phase1", "foo=bar1", True),
              ("phase2", "foo=bar2", True),
              ("excluded_ssid", "test", True),
              ("excluded_ssid", "foo", True, "test\nfoo"),
              ("roaming_partner", "example.com,0,4,*", True),
              ("roaming_partner", "example.org,1,2,US", True,
               "example.com,0,4,*\nexample.org,1,2,US"),
              ("update_identifier", "4", False),
              ("provisioning_sp", "sp.example.com", True),
              ("sp_priority", "7", False),
              ("min_dl_bandwidth_home", "100", False),
              ("min_ul_bandwidth_home", "101", False),
              ("min_dl_bandwidth_roaming", "102", False),
              ("min_ul_bandwidth_roaming", "103", False),
              ("max_bss_load", "57", False),
              ("req_conn_capab", "6:22,80,443", False),
              ("req_conn_capab", "17:500", False, "6:22,80,443\n17:500"),
              ("req_conn_capab", "50", False, "6:22,80,443\n17:500\n50"),
              ("ocsp", "1", False)]
    for v in values:
        if v[2]:
            set_cred_quoted(dev[0], id, v[0], v[1])
        else:
            set_cred(dev[0], id, v[0], v[1])
        val = dev[0].get_cred(id, v[0])
        if len(v) == 4:
            expect = v[3]
        else:
            expect = v[1]
        if val != expect:
            raise Exception("Unexpected GET_CRED value for {}: {} != {}".format(v[0], val, expect))
    creds = dev[0].request("LIST_CREDS").splitlines()
    if len(creds) != 2:
        raise Exception("Unexpected LIST_CREDS result(3)")
    if creds[1] != "0\texample.com\tuser@example.com\texample.com\t310026-000000000":
        raise Exception("Unexpected LIST_CREDS value")
    remove_cred(dev[0], id)
    if len(dev[0].request("LIST_CREDS").splitlines()) != 1:
        raise Exception("Unexpected LIST_CREDS result(4)")

    id = add_cred(dev[0])
    set_cred_quoted(dev[0], id, "domain", "foo.example.com")
    id = add_cred(dev[0])
    set_cred_quoted(dev[0], id, "domain", "bar.example.com")
    id = add_cred(dev[0])
    set_cred_quoted(dev[0], id, "domain", "foo.example.com")
    if "OK" not in dev[0].request("REMOVE_CRED sp_fqdn=foo.example.com"):
        raise Exception("REMOVE_CRED failed")
    creds = dev[0].request("LIST_CREDS")
    if "foo.example.com" in creds:
        raise Exception("REMOVE_CRED sp_fqdn did not remove cred")
    if "bar.example.com" not in creds:
        raise Exception("REMOVE_CRED sp_fqdn removed incorrect cred")
    dev[0].request("REMOVE_CRED all")

    id = add_cred(dev[0])
    set_cred_quoted(dev[0], id, "domain", "foo.example.com")
    set_cred_quoted(dev[0], id, "provisioning_sp", "sp.foo.example.com")
    id = add_cred(dev[0])
    set_cred_quoted(dev[0], id, "domain", "bar.example.com")
    set_cred_quoted(dev[0], id, "provisioning_sp", "sp.bar.example.com")
    id = add_cred(dev[0])
    set_cred_quoted(dev[0], id, "domain", "foo.example.com")
    set_cred_quoted(dev[0], id, "provisioning_sp", "sp.foo.example.com")
    if "OK" not in dev[0].request("REMOVE_CRED provisioning_sp=sp.foo.example.com"):
        raise Exception("REMOVE_CRED failed")
    creds = dev[0].request("LIST_CREDS")
    if "foo.example.com" in creds:
        raise Exception("REMOVE_CRED provisioning_sp did not remove cred")
    if "bar.example.com" not in creds:
        raise Exception("REMOVE_CRED provisioning_sp removed incorrect cred")
    dev[0].request("REMOVE_CRED all")

    # Test large number of creds and LIST_CREDS truncation
    dev[0].dump_monitor()
    for i in range(0, 100):
        id = add_cred(dev[0])
        set_cred_quoted(dev[0], id, "realm", "relatively.long.realm.test%d.example.com" % i)
        dev[0].dump_monitor()
    creds = dev[0].request("LIST_CREDS")
    for i in range(0, 100):
        dev[0].remove_cred(i)
        dev[0].dump_monitor()
    if len(creds) < 3900 or len(creds) > 4100:
        raise Exception("Unexpected LIST_CREDS length: %d" % len(creds))
    if "test10.example.com" not in creds:
        raise Exception("Missing credential")
    if len(creds.splitlines()) > 95:
        raise Exception("Too many LIST_CREDS entries in the buffer")

def test_wpas_ctrl_pno(dev):
    """wpa_supplicant ctrl_iface pno"""
    if "FAIL" not in dev[0].request("SET pno 1"):
        raise Exception("Unexpected success in enabling PNO without enabled network blocks")
    id = dev[0].add_network()
    dev[0].set_network_quoted(id, "ssid", "test")
    dev[0].set_network(id, "key_mgmt", "NONE")
    dev[0].request("ENABLE_NETWORK " + str(id) + " no-connect")
    #mac80211_hwsim does not yet support PNO, so this fails
    if "FAIL" not in dev[0].request("SET pno 1"):
        raise Exception("Unexpected success in enabling PNO")
    if "FAIL" not in dev[0].request("SET pno 1 freq=2000-3000,5180"):
        raise Exception("Unexpected success in enabling PNO")
    if "FAIL" not in dev[0].request("SET pno 1 freq=0-6000"):
        raise Exception("Unexpected success in enabling PNO")
    if "FAIL" in dev[0].request("SET pno 0"):
        raise Exception("Unexpected failure in disabling PNO")

@remote_compatible
def test_wpas_ctrl_get(dev):
    """wpa_supplicant ctrl_iface get"""
    if "FAIL" in dev[0].request("GET version"):
        raise Exception("Unexpected get failure for version")
    if "FAIL" in dev[0].request("GET wifi_display"):
        raise Exception("Unexpected get failure for wifi_display")
    if "FAIL" not in dev[0].request("GET foo"):
        raise Exception("Unexpected success on get command")

    dev[0].set("wifi_display", "0")
    if dev[0].request("GET wifi_display") != '0':
        raise Exception("Unexpected wifi_display value")
    dev[0].set("wifi_display", "1")
    if dev[0].request("GET wifi_display") != '1':
        raise Exception("Unexpected wifi_display value")
    dev[0].request("P2P_SET disabled 1")
    if dev[0].request("GET wifi_display") != '0':
        raise Exception("Unexpected wifi_display value (P2P disabled)")
    dev[0].request("P2P_SET disabled 0")
    if dev[0].request("GET wifi_display") != '1':
        raise Exception("Unexpected wifi_display value (P2P re-enabled)")
    dev[0].set("wifi_display", "0")
    if dev[0].request("GET wifi_display") != '0':
        raise Exception("Unexpected wifi_display value")

@remote_compatible
def test_wpas_ctrl_preauth(dev):
    """wpa_supplicant ctrl_iface preauth"""
    if "FAIL" not in dev[0].request("PREAUTH "):
        raise Exception("Unexpected success on invalid PREAUTH")
    if "FAIL" in dev[0].request("PREAUTH 00:11:22:33:44:55"):
        raise Exception("Unexpected failure on PREAUTH")

@remote_compatible
def test_wpas_ctrl_tdls_discover(dev):
    """wpa_supplicant ctrl_iface tdls_discover"""
    if "FAIL" not in dev[0].request("TDLS_DISCOVER "):
        raise Exception("Unexpected success on invalid TDLS_DISCOVER")
    if "FAIL" not in dev[0].request("TDLS_DISCOVER 00:11:22:33:44:55"):
        raise Exception("Unexpected success on TDLS_DISCOVER")

@remote_compatible
def test_wpas_ctrl_tdls_chan_switch(dev):
    """wpa_supplicant ctrl_iface tdls_chan_switch error cases"""
    for args in ['', '00:11:22:33:44:55']:
        if "FAIL" not in dev[0].request("TDLS_CANCEL_CHAN_SWITCH " + args):
            raise Exception("Unexpected success on invalid TDLS_CANCEL_CHAN_SWITCH: " + args)

    for args in ['', 'foo ', '00:11:22:33:44:55 ', '00:11:22:33:44:55 q',
                 '00:11:22:33:44:55 81', '00:11:22:33:44:55 81 1234',
                 '00:11:22:33:44:55 81 1234 center_freq1=234 center_freq2=345 bandwidth=456 sec_channel_offset=567 ht vht']:
        if "FAIL" not in dev[0].request("TDLS_CHAN_SWITCH " + args):
            raise Exception("Unexpected success on invalid TDLS_CHAN_SWITCH: " + args)

@remote_compatible
def test_wpas_ctrl_addr(dev):
    """wpa_supplicant ctrl_iface invalid address"""
    if "FAIL" not in dev[0].request("TDLS_SETUP "):
        raise Exception("Unexpected success on invalid TDLS_SETUP")
    if "FAIL" not in dev[0].request("TDLS_TEARDOWN "):
        raise Exception("Unexpected success on invalid TDLS_TEARDOWN")
    if "FAIL" not in dev[0].request("FT_DS "):
        raise Exception("Unexpected success on invalid FT_DS")
    if "FAIL" not in dev[0].request("WPS_PBC 00:11:22:33:44"):
        raise Exception("Unexpected success on invalid WPS_PBC")
    if "FAIL" not in dev[0].request("WPS_PIN 00:11:22:33:44"):
        raise Exception("Unexpected success on invalid WPS_PIN")
    if "FAIL" not in dev[0].request("WPS_NFC 00:11:22:33:44"):
        raise Exception("Unexpected success on invalid WPS_NFC")
    if "FAIL" not in dev[0].request("WPS_REG 00:11:22:33:44 12345670"):
        raise Exception("Unexpected success on invalid WPS_REG")
    if "FAIL" not in dev[0].request("IBSS_RSN 00:11:22:33:44"):
        raise Exception("Unexpected success on invalid IBSS_RSN")
    if "FAIL" not in dev[0].request("BSSID_IGNORE 00:11:22:33:44"):
        raise Exception("Unexpected success on invalid BSSID_IGNORE")

@remote_compatible
def test_wpas_ctrl_wps_errors(dev):
    """wpa_supplicant ctrl_iface WPS error cases"""
    if "FAIL" not in dev[0].request("WPS_REG 00:11:22:33:44:55"):
        raise Exception("Unexpected success on invalid WPS_REG")
    if "FAIL" not in dev[0].request("WPS_REG 00:11:22:33:44:55 12345670 2233"):
        raise Exception("Unexpected success on invalid WPS_REG")
    if "FAIL" not in dev[0].request("WPS_REG 00:11:22:33:44:55 12345670 2233 OPEN"):
        raise Exception("Unexpected success on invalid WPS_REG")
    if "FAIL" not in dev[0].request("WPS_REG 00:11:22:33:44:55 12345670 2233 OPEN NONE"):
        raise Exception("Unexpected success on invalid WPS_REG")

    if "FAIL" not in dev[0].request("WPS_AP_PIN random"):
        raise Exception("Unexpected success on WPS_AP_PIN in non-AP mode")

    if "FAIL" not in dev[0].request("WPS_ER_PIN any"):
        raise Exception("Unexpected success on invalid WPS_ER_PIN")

    if "FAIL" not in dev[0].request("WPS_ER_LEARN 00:11:22:33:44:55"):
        raise Exception("Unexpected success on invalid WPS_ER_LEARN")

    if "FAIL" not in dev[0].request("WPS_ER_SET_CONFIG 00:11:22:33:44:55"):
        raise Exception("Unexpected success on invalid WPS_ER_SET_CONFIG")

    if "FAIL" not in dev[0].request("WPS_ER_CONFIG 00:11:22:33:44:55"):
        raise Exception("Unexpected success on invalid WPS_ER_CONFIG")
    if "FAIL" not in dev[0].request("WPS_ER_CONFIG 00:11:22:33:44:55 12345670"):
        raise Exception("Unexpected success on invalid WPS_ER_CONFIG")
    if "FAIL" not in dev[0].request("WPS_ER_CONFIG 00:11:22:33:44:55 12345670 2233"):
        raise Exception("Unexpected success on invalid WPS_ER_CONFIG")
    if "FAIL" not in dev[0].request("WPS_ER_CONFIG 00:11:22:33:44:55 12345670 2233 OPEN"):
        raise Exception("Unexpected success on invalid WPS_ER_CONFIG")
    if "FAIL" not in dev[0].request("WPS_ER_CONFIG 00:11:22:33:44:55 12345670 2233 OPEN NONE"):
        raise Exception("Unexpected success on invalid WPS_ER_CONFIG")

    if "FAIL" not in dev[0].request("WPS_ER_NFC_CONFIG_TOKEN WPS"):
        raise Exception("Unexpected success on invalid WPS_ER_NFC_CONFIG_TOKEN")
    if "FAIL" not in dev[0].request("WPS_ER_NFC_CONFIG_TOKEN FOO 00:11:22:33:44:55"):
        raise Exception("Unexpected success on invalid WPS_ER_NFC_CONFIG_TOKEN")
    if "FAIL" not in dev[0].request("WPS_ER_NFC_CONFIG_TOKEN NDEF 00:11:22:33:44:55"):
        raise Exception("Unexpected success on invalid WPS_ER_NFC_CONFIG_TOKEN")

    if "FAIL" not in dev[0].request("WPS_NFC_CONFIG_TOKEN FOO"):
        raise Exception("Unexpected success on invalid WPS_NFC_CONFIG_TOKEN")
    if "FAIL" not in dev[0].request("WPS_NFC_CONFIG_TOKEN WPS FOO"):
        raise Exception("Unexpected success on invalid WPS_NFC_CONFIG_TOKEN")
    if "FAIL" not in dev[0].request("WPS_NFC_TOKEN FOO"):
        raise Exception("Unexpected success on invalid WPS_NFC_TOKEN")

@remote_compatible
def test_wpas_ctrl_config_parser(dev):
    """wpa_supplicant ctrl_iface SET config parser"""
    if "FAIL" not in dev[0].request("SET pbc_in_m1 qwerty"):
        raise Exception("Non-number accepted as integer")
    if "FAIL" not in dev[0].request("SET eapol_version 0"):
        raise Exception("Out-of-range value accepted")
    if "FAIL" not in dev[0].request("SET eapol_version 10"):
        raise Exception("Out-of-range value accepted")

    if "FAIL" not in dev[0].request("SET serial_number 0123456789abcdef0123456789abcdef0"):
        raise Exception("Too long string accepted")

@remote_compatible
def test_wpas_ctrl_mib(dev):
    """wpa_supplicant ctrl_iface MIB"""
    mib = dev[0].get_mib()
    if "dot11RSNAOptionImplemented" not in mib:
        raise Exception("Missing MIB entry")
    if mib["dot11RSNAOptionImplemented"] != "TRUE":
        raise Exception("Unexpected dot11RSNAOptionImplemented value")

def test_wpas_ctrl_set_wps_params(dev):
    """wpa_supplicant ctrl_iface SET config_methods"""
    try:
        _test_wpas_ctrl_set_wps_params(dev)
    finally:
        dev[2].request("SET config_methods ")

def _test_wpas_ctrl_set_wps_params(dev):
    ts = ["config_methods label virtual_display virtual_push_button keypad",
          "device_type 1-0050F204-1",
          "os_version 01020300",
          "uuid 12345678-9abc-def0-1234-56789abcdef0"]
    for t in ts:
        if "OK" not in dev[2].request("SET " + t):
            raise Exception("SET failed for: " + t)

    ts = ["uuid 12345678+9abc-def0-1234-56789abcdef0",
          "uuid 12345678-qabc-def0-1234-56789abcdef0",
          "uuid 12345678-9abc+def0-1234-56789abcdef0",
          "uuid 12345678-9abc-qef0-1234-56789abcdef0",
          "uuid 12345678-9abc-def0+1234-56789abcdef0",
          "uuid 12345678-9abc-def0-q234-56789abcdef0",
          "uuid 12345678-9abc-def0-1234+56789abcdef0",
          "uuid 12345678-9abc-def0-1234-q6789abcdef0",
          "uuid qwerty"]
    for t in ts:
        if "FAIL" not in dev[2].request("SET " + t):
            raise Exception("SET succeeded for: " + t)

def test_wpas_ctrl_level(dev):
    """wpa_supplicant ctrl_iface LEVEL"""
    try:
        if "FAIL" not in dev[2].request("LEVEL 3"):
            raise Exception("Unexpected LEVEL success")
        if "OK" not in dev[2].mon.request("LEVEL 2"):
            raise Exception("Unexpected LEVEL failure")
        dev[2].request("SCAN freq=2412")
        ev = dev[2].wait_event(["State:"], timeout=5)
        if ev is None:
            raise Exception("No debug message received")
        dev[2].wait_event(["CTRL-EVENT-SCAN-RESULTS"], timeout=5)
    finally:
        dev[2].mon.request("LEVEL 3")

@remote_compatible
def test_wpas_ctrl_bssid_filter(dev, apdev):
    """wpa_supplicant bssid_filter"""
    try:
        if "OK" not in dev[2].request("SET bssid_filter " + apdev[0]['bssid']):
            raise Exception("Failed to set bssid_filter")
        params = {"ssid": "test"}
        hostapd.add_ap(apdev[0], params)
        hostapd.add_ap(apdev[1], params)
        dev[2].scan_for_bss(apdev[0]['bssid'], freq="2412")
        dev[2].scan(freq="2412")
        bss = dev[2].get_bss(apdev[0]['bssid'])
        if bss is None or len(bss) == 0:
            raise Exception("Missing BSS data")
        bss = dev[2].get_bss(apdev[1]['bssid'])
        if bss and len(bss) != 0:
            raise Exception("Unexpected BSS data")
        dev[2].request("SET bssid_filter " + apdev[0]['bssid'] + " " + \
                       apdev[1]['bssid'])
        dev[2].scan(freq="2412")
        bss = dev[2].get_bss(apdev[0]['bssid'])
        if bss is None or len(bss) == 0:
            raise Exception("Missing BSS data")
        bss = dev[2].get_bss(apdev[1]['bssid'])
        if bss is None or len(bss) == 0:
            raise Exception("Missing BSS data(2)")
        res = dev[2].request("SCAN_RESULTS").splitlines()
        if "test" not in res[1] or "test" not in res[2]:
            raise Exception("SSID missing from SCAN_RESULTS")
        if apdev[0]['bssid'] not in res[1] and apdev[1]['bssid'] not in res[1]:
            raise Exception("BSS1 missing from SCAN_RESULTS")
        if apdev[0]['bssid'] not in res[2] and apdev[1]['bssid'] not in res[2]:
            raise Exception("BSS1 missing from SCAN_RESULTS")

        if "FAIL" not in dev[2].request("SET bssid_filter 00:11:22:33:44:55 00:11:22:33:44"):
            raise Exception("Unexpected success for invalid SET bssid_filter")
    finally:
        dev[2].request("SET bssid_filter ")

@remote_compatible
def test_wpas_ctrl_disallow_aps(dev, apdev):
    """wpa_supplicant ctrl_iface disallow_aps"""
    params = {"ssid": "test"}
    hostapd.add_ap(apdev[0], params)

    if "FAIL" not in dev[0].request("SET disallow_aps bssid "):
        raise Exception("Unexpected success on invalid disallow_aps")
    if "FAIL" not in dev[0].request("SET disallow_aps bssid 00:11:22:33:44"):
        raise Exception("Unexpected success on invalid disallow_aps")
    if "FAIL" not in dev[0].request("SET disallow_aps ssid 0"):
        raise Exception("Unexpected success on invalid disallow_aps")
    if "FAIL" not in dev[0].request("SET disallow_aps ssid 4q"):
        raise Exception("Unexpected success on invalid disallow_aps")
    if "FAIL" not in dev[0].request("SET disallow_aps bssid 00:11:22:33:44:55 ssid 112233 ssid 123"):
        raise Exception("Unexpected success on invalid disallow_aps")
    if "FAIL" not in dev[0].request("SET disallow_aps ssid 000102030405060708090a0b0c0d0e0f000102030405060708090a0b0c0d0e0f00"):
        raise Exception("Unexpected success on invalid disallow_aps")
    if "FAIL" not in dev[0].request("SET disallow_aps foo 112233445566"):
        raise Exception("Unexpected success on invalid disallow_aps")

    dev[0].connect("test", key_mgmt="NONE", scan_freq="2412")
    hostapd.add_ap(apdev[1], params)
    dev[0].scan_for_bss(apdev[1]['bssid'], freq="2412")
    dev[0].dump_monitor()
    if "OK" not in dev[0].request("SET disallow_aps bssid 00:11:22:33:44:55 bssid 00:22:33:44:55:66"):
        raise Exception("Failed to set disallow_aps")
    if "OK" not in dev[0].request("SET disallow_aps bssid " + apdev[0]['bssid']):
        raise Exception("Failed to set disallow_aps")
    ev = dev[0].wait_connected(timeout=30, error="Reassociation timed out")
    if apdev[1]['bssid'] not in ev:
        raise Exception("Unexpected BSSID")

    dev[0].dump_monitor()
    if "OK" not in dev[0].request("SET disallow_aps ssid " + binascii.hexlify(b"test").decode()):
        raise Exception("Failed to set disallow_aps")
    dev[0].wait_disconnected(timeout=5, error="Disconnection not seen")
    ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED"], timeout=1)
    if ev is not None:
        raise Exception("Unexpected reassociation")

    dev[0].request("DISCONNECT")
    dev[0].p2p_start_go(freq=2412)
    if "OK" not in dev[0].request("SET disallow_aps "):
        raise Exception("Failed to set disallow_aps")

@remote_compatible
def test_wpas_ctrl_blob(dev):
    """wpa_supplicant ctrl_iface SET blob"""
    if "FAIL" not in dev[0].request("SET blob foo"):
        raise Exception("Unexpected SET success")
    if "FAIL" not in dev[0].request("SET blob foo 0"):
        raise Exception("Unexpected SET success")
    if "FAIL" not in dev[0].request("SET blob foo 0q"):
        raise Exception("Unexpected SET success")
    if "OK" not in dev[0].request("SET blob foo 00"):
        raise Exception("Unexpected SET failure")
    if "OK" not in dev[0].request("SET blob foo 0011"):
        raise Exception("Unexpected SET failure")

@remote_compatible
def test_wpas_ctrl_set_uapsd(dev):
    """wpa_supplicant ctrl_iface SET uapsd"""
    if "FAIL" not in dev[0].request("SET uapsd foo"):
        raise Exception("Unexpected SET success")
    if "FAIL" not in dev[0].request("SET uapsd 0,0,0"):
        raise Exception("Unexpected SET success")
    if "FAIL" not in dev[0].request("SET uapsd 0,0"):
        raise Exception("Unexpected SET success")
    if "FAIL" not in dev[0].request("SET uapsd 0"):
        raise Exception("Unexpected SET success")
    if "OK" not in dev[0].request("SET uapsd 1,1,1,1;1"):
        raise Exception("Unexpected SET failure")
    if "OK" not in dev[0].request("SET uapsd 0,0,0,0;0"):
        raise Exception("Unexpected SET failure")
    if "OK" not in dev[0].request("SET uapsd disable"):
        raise Exception("Unexpected SET failure")

def test_wpas_ctrl_set(dev):
    """wpa_supplicant ctrl_iface SET"""
    vals = ["foo",
            "ampdu 0",
            "radio_disable 0",
            "ps 10",
            "dot11RSNAConfigPMKLifetime 0",
            "dot11RSNAConfigPMKReauthThreshold 101",
            "dot11RSNAConfigSATimeout 0",
            "wps_version_number -1",
            "wps_version_number 256",
            "fst_group_id ",
            "fst_llt 0"]
    for val in vals:
        if "FAIL" not in dev[0].request("SET " + val):
            raise Exception("Unexpected SET success for " + val)

    vals = ["ps 1"]
    for val in vals:
        dev[0].request("SET " + val)

    vals = ["EAPOL::heldPeriod 60",
            "EAPOL::authPeriod 30",
            "EAPOL::startPeriod 30",
            "EAPOL::maxStart 3",
            "dot11RSNAConfigSATimeout 60",
            "ps -1",
            "ps 0",
            "no_keep_alive 0",
            "tdls_disabled 1",
            "tdls_disabled 0"]
    for val in vals:
        if "OK" not in dev[0].request("SET " + val):
            raise Exception("Unexpected SET failure for " + val)

    # This fails if wpa_supplicant is built with loadable EAP peer method
    # support due to missing file and succeeds if no support for loadable
    # methods is included, so don't check the return value for now.
    dev[0].request("SET load_dynamic_eap /tmp/hwsim-eap-not-found.so")

@remote_compatible
def test_wpas_ctrl_get_capability(dev):
    """wpa_supplicant ctrl_iface GET_CAPABILITY"""
    if "FAIL" not in dev[0].request("GET_CAPABILITY 1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"):
        raise Exception("Unexpected success on invalid GET_CAPABILITY")
    if "FAIL" not in dev[0].request("GET_CAPABILITY eap foo"):
        raise Exception("Unexpected success on invalid GET_CAPABILITY")
    if "AP" not in dev[0].request("GET_CAPABILITY modes strict"):
        raise Exception("Unexpected GET_CAPABILITY response")
    res = dev[0].get_capability("eap")
    if "TTLS" not in res:
        raise Exception("Unexpected GET_CAPABILITY eap response: " + str(res))

    res = dev[0].get_capability("pairwise")
    if "CCMP" not in res:
        raise Exception("Unexpected GET_CAPABILITY pairwise response: " + str(res))

    res = dev[0].get_capability("group")
    if "CCMP" not in res:
        raise Exception("Unexpected GET_CAPABILITY group response: " + str(res))

    res = dev[0].get_capability("key_mgmt")
    if "WPA-PSK" not in res or "WPA-EAP" not in res:
        raise Exception("Unexpected GET_CAPABILITY key_mgmt response: " + str(res))

    res = dev[0].get_capability("key_mgmt iftype=STATION")
    if "WPA-PSK" not in res or "WPA-EAP" not in res:
        raise Exception("Unexpected GET_CAPABILITY key_mgmt iftype=STATION response: " + str(res))

    iftypes = [ "STATION", "AP_VLAN", "AP", "P2P_GO", "P2P_CLIENT",
                "P2P_DEVICE", "MESH", "IBSS", "NAN", "UNKNOWN" ]
    for i in iftypes:
        res = dev[0].get_capability("key_mgmt iftype=" + i)
        logger.info("GET_CAPABILITY key_mgmt iftype=%s: %s" % (i, res))

    res = dev[0].get_capability("proto")
    if "WPA" not in res or "RSN" not in res:
        raise Exception("Unexpected GET_CAPABILITY proto response: " + str(res))

    res = dev[0].get_capability("auth_alg")
    if "OPEN" not in res or "SHARED" not in res:
        raise Exception("Unexpected GET_CAPABILITY auth_alg response: " + str(res))

    res = dev[0].get_capability("modes")
    if "IBSS" not in res or "AP" not in res:
        raise Exception("Unexpected GET_CAPABILITY modes response: " + str(res))

    res = dev[0].get_capability("channels")
    if "8" not in res or "36" not in res:
        raise Exception("Unexpected GET_CAPABILITY channels response: " + str(res))

    res = dev[0].get_capability("freq")
    if "2457" not in res or "5180" not in res:
        raise Exception("Unexpected GET_CAPABILITY freq response: " + str(res))

    res = dev[0].get_capability("tdls")
    if "EXTERNAL" not in res[0]:
        raise Exception("Unexpected GET_CAPABILITY tdls response: " + str(res))

    res = dev[0].get_capability("erp")
    if res is None or "ERP" not in res[0]:
        raise Exception("Unexpected GET_CAPABILITY erp response: " + str(res))

    if dev[0].get_capability("foo") is not None:
        raise Exception("Unexpected GET_CAPABILITY foo response: " + str(res))

@remote_compatible
def test_wpas_ctrl_nfc_report_handover(dev):
    """wpa_supplicant ctrl_iface NFC_REPORT_HANDOVER"""
    vals = ["FOO",
            "ROLE freq=12345",
            "ROLE TYPE",
            "ROLE TYPE REQ",
            "ROLE TYPE REQ SEL",
            "ROLE TYPE 0Q SEL",
            "ROLE TYPE 00 SEL",
            "ROLE TYPE 00 0Q",
            "ROLE TYPE 00 00"]
    for v in vals:
        if "FAIL" not in dev[0].request("NFC_REPORT_HANDOVER " + v):
            raise Exception("Unexpected NFC_REPORT_HANDOVER success for " + v)

@remote_compatible
def test_wpas_ctrl_nfc_tag_read(dev):
    """wpa_supplicant ctrl_iface WPS_NFC_TAG_READ"""
    vals = ["FOO", "0Q", "00", "000000", "10000001", "10000000", "00000000",
            "100e0000", "100e0001ff", "100e000411110000", "100e0004100e0001"]
    for v in vals:
        if "FAIL" not in dev[0].request("WPS_NFC_TAG_READ " + v):
            raise Exception("Unexpected WPS_NFC_TAG_READ success for " + v)

@remote_compatible
def test_wpas_ctrl_nfc_get_handover(dev):
    """wpa_supplicant ctrl_iface NFC_GET_HANDOVER"""
    vals = ["FOO", "FOO BAR", "WPS WPS", "WPS WPS-CR", "WPS FOO", "NDEF P2P"]
    for v in vals:
        if "FAIL" not in dev[0].request("NFC_GET_HANDOVER_REQ " + v):
            raise Exception("Unexpected NFC_GET_HANDOVER_REQ success for " + v)

    vals = ["NDEF WPS", "NDEF P2P-CR", "WPS P2P-CR"]
    for v in vals:
        if "FAIL" in dev[0].request("NFC_GET_HANDOVER_REQ " + v):
            raise Exception("Unexpected NFC_GET_HANDOVER_REQ failure for " + v)

    vals = ["FOO", "FOO BAR", "WPS WPS", "WPS WPS-CR", "WPS FOO", "NDEF P2P",
            "NDEF WPS", "NDEF WPS uuid"]
    for v in vals:
        if "FAIL" not in dev[0].request("NFC_GET_HANDOVER_SEL " + v):
            raise Exception("Unexpected NFC_GET_HANDOVER_SEL success for " + v)

    vals = ["NDEF P2P-CR", "WPS P2P-CR", "NDEF P2P-CR-TAG",
            "WPS P2P-CR-TAG"]
    for v in vals:
        if "FAIL" in dev[0].request("NFC_GET_HANDOVER_SEL " + v):
            raise Exception("Unexpected NFC_GET_HANDOVER_SEL failure for " + v)

def get_bssid_ignore_list(dev):
    return dev.request("BSSID_IGNORE").splitlines()

@remote_compatible
def test_wpas_ctrl_bssid_ignore(dev):
    """wpa_supplicant ctrl_iface BSSID_IGNORE"""
    if "OK" not in dev[0].request("BSSID_IGNORE clear"):
        raise Exception("BSSID_IGNORE clear failed")
    b = get_bssid_ignore_list(dev[0])
    if len(b) != 0:
        raise Exception("Unexpected BSSID ignore list contents: " + str(b))
    if "OK" not in dev[0].request("BSSID_IGNORE 00:11:22:33:44:55"):
        raise Exception("BSSID_IGNORE add failed")
    b = get_bssid_ignore_list(dev[0])
    if "00:11:22:33:44:55" not in b:
        raise Exception("Unexpected BSSID ignore list contents: " + str(b))
    if "OK" not in dev[0].request("BSSID_IGNORE 00:11:22:33:44:56"):
        raise Exception("BSSID_IGNORE add failed")
    b = get_bssid_ignore_list(dev[0])
    if "00:11:22:33:44:55" not in b or "00:11:22:33:44:56" not in b:
        raise Exception("Unexpected BSSID ignore list contents: " + str(b))
    if "OK" not in dev[0].request("BSSID_IGNORE 00:11:22:33:44:56"):
        raise Exception("BSSID_IGNORE add failed")
    b = get_bssid_ignore_list(dev[0])
    if "00:11:22:33:44:55" not in b or "00:11:22:33:44:56" not in b or len(b) != 2:
        raise Exception("Unexpected BSSID ignore list contents: " + str(b))

    if "OK" not in dev[0].request("BSSID_IGNORE clear"):
        raise Exception("BSSID_IGNORE clear failed")
    if dev[0].request("BSSID_IGNORE") != "":
        raise Exception("Unexpected BSSID ignore list contents")

@remote_compatible
def test_wpas_ctrl_bssid_ignore_oom(dev):
    """wpa_supplicant ctrl_iface BSSID_IGNORE and out-of-memory"""
    with alloc_fail(dev[0], 1, "wpa_bssid_ignore_add"):
        if "FAIL" not in dev[0].request("BSSID_IGNORE aa:bb:cc:dd:ee:ff"):
            raise Exception("Unexpected success with allocation failure")

def test_wpas_ctrl_log_level(dev):
    """wpa_supplicant ctrl_iface LOG_LEVEL"""
    level = dev[2].request("LOG_LEVEL")
    if "Current level: MSGDUMP" not in level:
        raise Exception("Unexpected debug level(1): " + level)
    if "Timestamp: 1" not in level:
        raise Exception("Unexpected timestamp(1): " + level)

    if "OK" not in dev[2].request("LOG_LEVEL  MSGDUMP  0"):
        raise Exception("LOG_LEVEL failed")
    level = dev[2].request("LOG_LEVEL")
    if "Current level: MSGDUMP" not in level:
        raise Exception("Unexpected debug level(2): " + level)
    if "Timestamp: 0" not in level:
        raise Exception("Unexpected timestamp(2): " + level)

    if "OK" not in dev[2].request("LOG_LEVEL  MSGDUMP  1"):
        raise Exception("LOG_LEVEL failed")
    level = dev[2].request("LOG_LEVEL")
    if "Current level: MSGDUMP" not in level:
        raise Exception("Unexpected debug level(3): " + level)
    if "Timestamp: 1" not in level:
        raise Exception("Unexpected timestamp(3): " + level)

    if "FAIL" not in dev[2].request("LOG_LEVEL FOO"):
        raise Exception("Invalid LOG_LEVEL accepted")

    for lev in ["EXCESSIVE", "MSGDUMP", "DEBUG", "INFO", "WARNING", "ERROR"]:
        if "OK" not in dev[2].request("LOG_LEVEL " + lev):
            raise Exception("LOG_LEVEL failed for " + lev)
        level = dev[2].request("LOG_LEVEL")
        if "Current level: " + lev not in level:
            raise Exception("Unexpected debug level: " + level)

    if "OK" not in dev[2].request("LOG_LEVEL  MSGDUMP  1"):
        raise Exception("LOG_LEVEL failed")
    level = dev[2].request("LOG_LEVEL")
    if "Current level: MSGDUMP" not in level:
        raise Exception("Unexpected debug level(3): " + level)
    if "Timestamp: 1" not in level:
        raise Exception("Unexpected timestamp(3): " + level)

@remote_compatible
def test_wpas_ctrl_enable_disable_network(dev, apdev):
    """wpa_supplicant ctrl_iface ENABLE/DISABLE_NETWORK"""
    params = {"ssid": "test"}
    hostapd.add_ap(apdev[0], params)

    id = dev[0].connect("test", key_mgmt="NONE", scan_freq="2412",
                        only_add_network=True)
    if "OK" not in dev[0].request("DISABLE_NETWORK " + str(id)):
        raise Exception("Failed to disable network")
    if "OK" not in dev[0].request("ENABLE_NETWORK " + str(id) + " no-connect"):
        raise Exception("Failed to enable network")
    if "OK" not in dev[0].request("DISABLE_NETWORK all"):
        raise Exception("Failed to disable networks")
    if "OK" not in dev[0].request("ENABLE_NETWORK " + str(id)):
        raise Exception("Failed to enable network")
    dev[0].wait_connected(timeout=10)
    if "OK" not in dev[0].request("DISABLE_NETWORK " + str(id)):
        raise Exception("Failed to disable network")
    dev[0].wait_disconnected(timeout=10)
    time.sleep(0.1)

    if "OK" not in dev[0].request("ENABLE_NETWORK all"):
        raise Exception("Failed to enable network")
    dev[0].wait_connected(timeout=10)
    if "OK" not in dev[0].request("DISABLE_NETWORK all"):
        raise Exception("Failed to disable network")
    dev[0].wait_disconnected(timeout=10)

def test_wpas_ctrl_country(dev, apdev):
    """wpa_supplicant SET/GET country code"""
    try:
        # work around issues with possible pending regdom event from the end of
        # the previous test case
        time.sleep(0.2)
        dev[0].dump_monitor()

        if "OK" not in dev[0].request("SET country FI"):
            raise Exception("Failed to set country code")
        if dev[0].request("GET country") != "FI":
            raise Exception("Country code set failed")
        ev = dev[0].wait_global_event(["CTRL-EVENT-REGDOM-CHANGE"], 10)
        if ev is None:
            raise Exception("regdom change event not seen")
        if "init=USER type=COUNTRY alpha2=FI" not in ev:
            raise Exception("Unexpected event contents: " + ev)
        dev[0].request("SET country 00")
        if dev[0].request("GET country") != "00":
            raise Exception("Country code set failed")
        ev = dev[0].wait_global_event(["CTRL-EVENT-REGDOM-CHANGE"], 10)
        if ev is None:
            raise Exception("regdom change event not seen")
        # init=CORE was previously used due to invalid db.txt data for 00. For
        # now, allow both it and the new init=USER after fixed db.txt.
        if "init=CORE type=WORLD" not in ev and "init=USER type=WORLD" not in ev:
            raise Exception("Unexpected event contents: " + ev)
    finally:
        subprocess.call(['iw', 'reg', 'set', '00'])

def test_wpas_ctrl_suspend_resume(dev):
    """wpa_supplicant SUSPEND/RESUME"""
    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    wpas.interface_add("wlan5")
    if "OK" not in wpas.global_request("SUSPEND"):
        raise Exception("SUSPEND failed")
    time.sleep(1)
    if "OK" not in wpas.global_request("RESUME"):
        raise Exception("RESUME failed")
    if "OK" not in wpas.request("SUSPEND"):
        raise Exception("Per-interface SUSPEND failed")
    if "OK" not in wpas.request("RESUME"):
        raise Exception("Per-interface RESUME failed")
    ev = wpas.wait_event(["CTRL-EVENT-SCAN-RESULTS"], timeout=10)
    if ev is None:
        raise Exception("Scan not completed")

def test_wpas_ctrl_global(dev):
    """wpa_supplicant global control interface"""
    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    wpas.interface_add("wlan5")

    if "PONG" not in wpas.global_request("PING"):
        raise Exception("PING failed")
    if "wlan5" not in wpas.global_request("INTERFACES"):
        raise Exception("Interface not found")
    if "UNKNOWN COMMAND" not in wpas.global_request("FOO"):
        raise Exception("Unexpected response to unknown command")
    if "PONG" not in wpas.global_request("IFNAME=wlan5 PING"):
        raise Exception("Per-interface PING failed")
    if "FAIL-NO-IFNAME-MATCH" not in wpas.global_request("IFNAME=notfound PING"):
        raise Exception("Unknown interface not reported correctly")
    if "FAIL" not in wpas.global_request("SAVE_CONFIG"):
        raise Exception("SAVE_CONFIG succeeded unexpectedly")
    if "OK" not in wpas.global_request("SET wifi_display 0"):
        raise Exception("SET failed")
    if "wifi_display=0" not in wpas.global_request("STATUS"):
        raise Exception("wifi_display not disabled")
    if "OK" not in wpas.global_request("SET wifi_display 1"):
        raise Exception("SET failed")
    if "wifi_display=1" not in wpas.global_request("STATUS"):
        raise Exception("wifi_display not enabled")
    if "FAIL" not in wpas.global_request("SET foo 1"):
        raise Exception("SET succeeded unexpectedly")

    if "p2p_state=IDLE" not in wpas.global_request("STATUS"):
        raise Exception("P2P was disabled")
    wpas.global_request("P2P_SET disabled 1")
    if "p2p_state=DISABLED" not in wpas.global_request("STATUS"):
        raise Exception("P2P was not disabled")
    wpas.global_request("P2P_SET disabled 0")
    if "p2p_state=IDLE" not in wpas.global_request("STATUS"):
        raise Exception("P2P was not enabled")

    # driver_nl80211.c does not support interface list, so do not fail because
    # of that
    logger.debug(wpas.global_request("INTERFACE_LIST"))

    if "FAIL" not in wpas.global_request("INTERFACE_ADD "):
        raise Exception("INTERFACE_ADD succeeded unexpectedly")
    if "FAIL" not in wpas.global_request("INTERFACE_ADD FOO"):
        raise Exception("INTERFACE_ADD succeeded unexpectedly")
    if "FAIL" not in wpas.global_request("INTERFACE_ADD FOO	conf"):
        raise Exception("INTERFACE_ADD succeeded unexpectedly")
    if "FAIL" not in wpas.global_request("INTERFACE_ADD FOO	conf	driver"):
        raise Exception("INTERFACE_ADD succeeded unexpectedly")
    if "FAIL" not in wpas.global_request("INTERFACE_ADD FOO	conf	driver	ctrliface"):
        raise Exception("INTERFACE_ADD succeeded unexpectedly")
    if "FAIL" not in wpas.global_request("INTERFACE_ADD FOO	conf	driver	ctrliface	driverparam"):
        raise Exception("INTERFACE_ADD succeeded unexpectedly")
    if "FAIL" not in wpas.global_request("INTERFACE_ADD FOO	conf	driver	ctrliface	driverparam	bridge"):
        raise Exception("INTERFACE_ADD succeeded unexpectedly")
    if "FAIL" not in wpas.global_request("INTERFACE_ADD FOO	conf	driver	ctrliface	driverparam	bridge	foo"):
        raise Exception("INTERFACE_ADD succeeded unexpectedly")
    if "FAIL" not in wpas.global_request("INTERFACE_ADD FOO					"):
        raise Exception("INTERFACE_ADD succeeded unexpectedly")
    if "FAIL" not in wpas.global_request("INTERFACE_ADD FOO	conf	driver	ctrliface	driverparam	bridge	create	abcd"):
        raise Exception("INTERFACE_ADD succeeded unexpectedly")

@remote_compatible
def test_wpas_ctrl_roam(dev, apdev):
    """wpa_supplicant ctrl_iface ROAM error cases"""
    if "FAIL" not in dev[0].request("ROAM 00:11:22:33:44"):
        raise Exception("Unexpected success")
    if "FAIL" not in dev[0].request("ROAM 00:11:22:33:44:55"):
        raise Exception("Unexpected success")
    params = {"ssid": "test"}
    hostapd.add_ap(apdev[0], params)
    id = dev[0].connect("test", key_mgmt="NONE", scan_freq="2412")
    if "FAIL" not in dev[0].request("ROAM 00:11:22:33:44:55"):
        raise Exception("Unexpected success")

@remote_compatible
def test_wpas_ctrl_ipaddr(dev, apdev):
    """wpa_supplicant IP address in STATUS"""
    try:
        dev[0].cmd_execute(['ip', 'addr', 'add', '10.174.65.207/32', 'dev',
                            dev[0].ifname])
        ipaddr = dev[0].get_status_field('ip_address')
        if ipaddr != '10.174.65.207':
            raise Exception("IP address not in STATUS output")
    finally:
        dev[0].cmd_execute(['ip', 'addr', 'del', '10.174.65.207/32', 'dev',
                            dev[0].ifname])

@remote_compatible
def test_wpas_ctrl_rsp(dev, apdev):
    """wpa_supplicant ctrl_iface CTRL-RSP-"""
    if "FAIL" not in dev[0].request("CTRL-RSP-"):
        raise Exception("Request succeeded unexpectedly")
    if "FAIL" not in dev[0].request("CTRL-RSP-foo-"):
        raise Exception("Request succeeded unexpectedly")
    if "FAIL" not in dev[0].request("CTRL-RSP-foo-1234567"):
        raise Exception("Request succeeded unexpectedly")
    if "FAIL" not in dev[0].request("CTRL-RSP-foo-1234567:"):
        raise Exception("Request succeeded unexpectedly")
    id = dev[0].add_network()
    if "FAIL" not in dev[0].request("CTRL-RSP-foo-%d:" % id):
        raise Exception("Request succeeded unexpectedly")
    for req in ["IDENTITY", "PASSWORD", "NEW_PASSWORD", "PIN", "OTP",
                "PASSPHRASE", "SIM"]:
        if "OK" not in dev[0].request("CTRL-RSP-%s-%d:" % (req, id)):
            raise Exception("Request failed unexpectedly")
        if "OK" not in dev[0].request("CTRL-RSP-%s-%d:" % (req, id)):
            raise Exception("Request failed unexpectedly")

def test_wpas_ctrl_vendor_test(dev, apdev):
    """wpas_supplicant and VENDOR test command"""
    OUI_QCA = 0x001374
    QCA_NL80211_VENDOR_SUBCMD_TEST = 1
    QCA_WLAN_VENDOR_ATTR_TEST = 8
    attr = struct.pack("@HHI", 4 + 4, QCA_WLAN_VENDOR_ATTR_TEST, 123)
    cmd = "VENDOR %x %d %s" % (OUI_QCA, QCA_NL80211_VENDOR_SUBCMD_TEST, binascii.hexlify(attr).decode())

    res = dev[0].request(cmd)
    if "FAIL" in res:
        raise Exception("VENDOR command failed")
    val, = struct.unpack("@I", binascii.unhexlify(res))
    if val != 125:
        raise Exception("Incorrect response value")

    res = dev[0].request(cmd + " nested=1")
    if "FAIL" in res:
        raise Exception("VENDOR command failed")
    val, = struct.unpack("@I", binascii.unhexlify(res))
    if val != 125:
        raise Exception("Incorrect response value")

    res = dev[0].request(cmd + " nested=0")
    if "FAIL" not in res:
        raise Exception("VENDOR command with invalid (not nested) data accepted")

@remote_compatible
def test_wpas_ctrl_vendor(dev, apdev):
    """wpa_supplicant ctrl_iface VENDOR"""
    cmds = ["foo",
            "1",
            "1 foo",
            "1 2foo",
            "1 2 qq"]
    for cmd in cmds:
        if "FAIL" not in dev[0].request("VENDOR " + cmd):
            raise Exception("Invalid VENDOR command accepted: " + cmd)

@remote_compatible
def test_wpas_ctrl_mgmt_tx(dev, apdev):
    """wpa_supplicant ctrl_iface MGMT_TX"""
    cmds = ["foo",
            "00:11:22:33:44:55 foo",
            "00:11:22:33:44:55 11:22:33:44:55:66",
            "00:11:22:33:44:55 11:22:33:44:55:66 freq=0 no_cck=0 wait_time=0 action=123",
            "00:11:22:33:44:55 11:22:33:44:55:66 action=12qq"]
    for cmd in cmds:
        if "FAIL" not in dev[0].request("MGMT_TX " + cmd):
            raise Exception("Invalid MGMT_TX command accepted: " + cmd)

    if "OK" not in dev[0].request("MGMT_TX_DONE"):
        raise Exception("MGMT_TX_DONE failed")

@remote_compatible
def test_wpas_ctrl_driver_event(dev, apdev):
    """wpa_supplicant ctrl_iface DRIVER_EVENT"""
    if "FAIL" not in dev[0].request("DRIVER_EVENT foo"):
        raise Exception("Invalid DRIVER_EVENT accepted")

@remote_compatible
def test_wpas_ctrl_eapol_rx(dev, apdev):
    """wpa_supplicant ctrl_iface EAPOL_RX"""
    cmds = ["foo",
            "00:11:22:33:44:55 123",
            "00:11:22:33:44:55 12qq"]
    for cmd in cmds:
        if "FAIL" not in dev[0].request("EAPOL_RX " + cmd):
            raise Exception("Invalid EAPOL_RX command accepted: " + cmd)

@remote_compatible
def test_wpas_ctrl_data_test(dev, apdev):
    """wpa_supplicant ctrl_iface DATA_TEST"""
    dev[0].request("DATA_TEST_CONFIG 0")
    if "FAIL" not in dev[0].request("DATA_TEST_TX 00:11:22:33:44:55 00:11:22:33:44:55 0"):
        raise Exception("DATA_TEST_TX accepted when not in test mode")

    try:
        if "OK" not in dev[0].request("DATA_TEST_CONFIG 1"):
            raise Exception("DATA_TEST_CONFIG failed")
        if "OK" not in dev[0].request("DATA_TEST_CONFIG 1"):
            raise Exception("DATA_TEST_CONFIG failed")
        cmds = ["foo",
                "00:11:22:33:44:55 foo",
                "00:11:22:33:44:55 00:11:22:33:44:55 -1",
                "00:11:22:33:44:55 00:11:22:33:44:55 256"]
        for cmd in cmds:
            if "FAIL" not in dev[0].request("DATA_TEST_TX " + cmd):
                raise Exception("Invalid DATA_TEST_TX command accepted: " + cmd)
        if "OK" not in dev[0].request("DATA_TEST_TX 00:11:22:33:44:55 00:11:22:33:44:55 0"):
            raise Exception("DATA_TEST_TX failed")
    finally:
        dev[0].request("DATA_TEST_CONFIG 0")

    cmds = ["",
            "00",
            "00112233445566778899aabbccdde",
            "00112233445566778899aabbccdq"]
    for cmd in cmds:
        if "FAIL" not in dev[0].request("DATA_TEST_FRAME " + cmd):
            raise Exception("Invalid DATA_TEST_FRAME command accepted: " + cmd)

    if "OK" not in dev[0].request("DATA_TEST_FRAME 00112233445566778899aabbccddee"):
        raise Exception("DATA_TEST_FRAME failed")

@remote_compatible
def test_wpas_ctrl_vendor_elem(dev, apdev):
    """wpa_supplicant ctrl_iface VENDOR_ELEM"""
    if "OK" not in dev[0].request("VENDOR_ELEM_ADD 1 "):
        raise Exception("VENDOR_ELEM_ADD failed")
    cmds = ["-1 ",
            "255 ",
            "1",
            "1 123",
            "1 12qq34"]
    for cmd in cmds:
        if "FAIL" not in dev[0].request("VENDOR_ELEM_ADD " + cmd):
            raise Exception("Invalid VENDOR_ELEM_ADD command accepted: " + cmd)

    cmds = ["-1 ",
            "255 "]
    for cmd in cmds:
        if "FAIL" not in dev[0].request("VENDOR_ELEM_GET " + cmd):
            raise Exception("Invalid VENDOR_ELEM_GET command accepted: " + cmd)

    dev[0].request("VENDOR_ELEM_REMOVE 1 *")
    cmds = ["-1 ",
            "255 ",
            "1",
            "1",
            "1 123",
            "1 12qq34",
            "1 12",
            "1 0000"]
    for cmd in cmds:
        if "FAIL" not in dev[0].request("VENDOR_ELEM_REMOVE " + cmd):
            raise Exception("Invalid VENDOR_ELEM_REMOVE command accepted: " + cmd)

    dev[0].request("VENDOR_ELEM_ADD 1 000100")
    if "OK" not in dev[0].request("VENDOR_ELEM_REMOVE 1 "):
        raise Exception("VENDOR_ELEM_REMOVE failed")
    cmds = ["-1 ",
            "255 ",
            "1",
            "1 123",
            "1 12qq34",
            "1 12",
            "1 0000"]
    for cmd in cmds:
        if "FAIL" not in dev[0].request("VENDOR_ELEM_REMOVE " + cmd):
            raise Exception("Invalid VENDOR_ELEM_REMOVE command accepted: " + cmd)
    if "OK" not in dev[0].request("VENDOR_ELEM_REMOVE 1 000100"):
        raise Exception("VENDOR_ELEM_REMOVE failed")

def test_wpas_ctrl_misc(dev, apdev):
    """wpa_supplicant ctrl_iface and miscellaneous commands"""
    if "OK" not in dev[0].request("RELOG"):
        raise Exception("RELOG failed")
    if dev[0].request("IFNAME") != dev[0].ifname:
        raise Exception("IFNAME returned unexpected response")
    if "FAIL" not in dev[0].request("REATTACH"):
        raise Exception("REATTACH accepted while disabled")
    if "OK" not in dev[2].request("RECONFIGURE"):
        raise Exception("RECONFIGURE failed")
    if "FAIL" in dev[0].request("INTERFACE_LIST"):
        raise Exception("INTERFACE_LIST failed")
    if "UNKNOWN COMMAND" not in dev[0].request("FOO"):
        raise Exception("Unknown command accepted")

    if "FAIL" not in dev[0].global_request("INTERFACE_REMOVE foo"):
        raise Exception("Invalid INTERFACE_REMOVE accepted")
    if "FAIL" not in dev[0].global_request("SET foo"):
        raise Exception("Invalid global SET accepted")

@remote_compatible
def test_wpas_ctrl_dump(dev, apdev):
    """wpa_supplicant ctrl_iface and DUMP/GET global parameters"""
    vals = dev[0].get_config()
    logger.info("Config values from DUMP: " + str(vals))
    for field in vals:
        res = dev[0].request("GET " + field)
        if res == 'FAIL\n':
            res = "null"
        if res != vals[field]:
            print("'{}' != '{}'".format(res, vals[field]))
            raise Exception("Mismatch in config field " + field)
    if "beacon_int" not in vals:
        raise Exception("Missing config field")

def test_wpas_ctrl_interface_add(dev, apdev):
    """wpa_supplicant INTERFACE_ADD/REMOVE with vif creation/removal"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": "open"})
    dev[0].connect("open", key_mgmt="NONE", scan_freq="2412")
    hwsim_utils.test_connectivity(dev[0], hapd)

    ifname = "test-" + dev[0].ifname
    dev[0].interface_add(ifname, create=True)
    wpas = WpaSupplicant(ifname=ifname)
    wpas.connect("open", key_mgmt="NONE", scan_freq="2412")
    hwsim_utils.test_connectivity(wpas, hapd)
    hwsim_utils.test_connectivity(dev[0], hapd)
    dev[0].global_request("INTERFACE_REMOVE " + ifname)
    hwsim_utils.test_connectivity(dev[0], hapd)

def test_wpas_ctrl_interface_add_sta(dev, apdev):
    """wpa_supplicant INTERFACE_ADD/REMOVE with STA vif creation/removal"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": "open"})
    ifname = "test-" + dev[0].ifname
    dev[0].interface_add(ifname, create=True, if_type='sta')
    wpas = WpaSupplicant(ifname=ifname)
    wpas.connect("open", key_mgmt="NONE", scan_freq="2412")
    wpas.request("DISCONNECT")
    wpas.wait_disconnected()
    dev[0].global_request("INTERFACE_REMOVE " + ifname)

def test_wpas_ctrl_interface_add_ap(dev, apdev):
    """wpa_supplicant INTERFACE_ADD/REMOVE AP interface"""
    with HWSimRadio() as (radio, iface):
        wpas0 = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
        wpas0.interface_add(iface)

        ifname = "test-wpas-ap"
        wpas0.interface_add(ifname, create=True, if_type='ap')
        wpas = WpaSupplicant(ifname=ifname)

        id = wpas.add_network()
        wpas.set_network(id, "mode", "2")
        wpas.set_network_quoted(id, "ssid", "wpas-ap-open")
        wpas.set_network(id, "key_mgmt", "NONE")
        wpas.set_network(id, "frequency", "2412")
        wpas.set_network(id, "scan_freq", "2412")
        wpas.select_network(id)
        wait_ap_ready(wpas)

        dev[1].connect("wpas-ap-open", key_mgmt="NONE", scan_freq="2412")
        dev[2].connect("wpas-ap-open", key_mgmt="NONE", scan_freq="2412")

        hwsim_utils.test_connectivity(wpas, dev[1])
        hwsim_utils.test_connectivity(dev[1], dev[2])

        dev[1].request("DISCONNECT")
        dev[2].request("DISCONNECT")
        dev[1].wait_disconnected()
        dev[2].wait_disconnected()
        wpas0.global_request("INTERFACE_REMOVE " + ifname)

def test_wpas_ctrl_interface_add_many(dev, apdev):
    """wpa_supplicant INTERFACE_ADD/REMOVE with vif creation/removal (many)"""
    try:
        _test_wpas_ctrl_interface_add_many(dev, apdev)
    finally:
        for i in range(10):
            ifname = "test%d-" % i + dev[0].ifname
            dev[0].global_request("INTERFACE_REMOVE " + ifname)

def _test_wpas_ctrl_interface_add_many(dev, apdev):
    hapd = hostapd.add_ap(apdev[0], {"ssid": "open"})
    dev[0].connect("open", key_mgmt="NONE", scan_freq="2412")
    hwsim_utils.test_connectivity(dev[0], hapd)
    dev[0].dump_monitor()

    l = []
    for i in range(10):
        ifname = "test%d-" % i + dev[0].ifname
        dev[0].interface_add(ifname, create=True)
        wpas = WpaSupplicant(ifname=ifname)
        wpas.connect("open", key_mgmt="NONE", scan_freq="2412")
        wpas.dump_monitor()
        l.append(wpas)
        dev[0].dump_monitor()
    for wpas in l:
        wpas.dump_monitor()
        hwsim_utils.test_connectivity(wpas, hapd)
        wpas.dump_monitor()
    dev[0].dump_monitor()

def test_wpas_ctrl_interface_add2(dev, apdev):
    """wpa_supplicant INTERFACE_ADD/REMOVE with vif without creation/removal"""
    ifname = "test-ext-" + dev[0].ifname
    try:
        _test_wpas_ctrl_interface_add2(dev, apdev, ifname)
    finally:
        subprocess.call(['iw', 'dev', ifname, 'del'])

def _test_wpas_ctrl_interface_add2(dev, apdev, ifname):
    hapd = hostapd.add_ap(apdev[0], {"ssid": "open"})
    dev[0].connect("open", key_mgmt="NONE", scan_freq="2412")
    hwsim_utils.test_connectivity(dev[0], hapd)

    subprocess.call(['iw', 'dev', dev[0].ifname, 'interface', 'add', ifname,
                     'type', 'station'])
    subprocess.call(['ip', 'link', 'set', 'dev', ifname, 'address',
                     '02:01:00:00:02:01'])
    dev[0].interface_add(ifname, set_ifname=False, all_params=True)
    wpas = WpaSupplicant(ifname=ifname)
    wpas.connect("open", key_mgmt="NONE", scan_freq="2412")
    hwsim_utils.test_connectivity(wpas, hapd)
    hwsim_utils.test_connectivity(dev[0], hapd)
    del wpas
    dev[0].global_request("INTERFACE_REMOVE " + ifname)
    hwsim_utils.test_connectivity(dev[0], hapd)

def test_wpas_ctrl_wait(dev, apdev, test_params):
    """wpa_supplicant control interface wait for client"""
    logfile = os.path.join(test_params['logdir'], 'wpas_ctrl_wait.log-wpas')
    pidfile = os.path.join(test_params['logdir'], 'wpas_ctrl_wait.pid-wpas')
    conffile = os.path.join(test_params['logdir'], 'wpas_ctrl_wait.conf')
    with open(conffile, 'w') as f:
        f.write("ctrl_interface=DIR=/var/run/wpa_supplicant\n")

    prg = os.path.join(test_params['logdir'],
                       'alt-wpa_supplicant/wpa_supplicant/wpa_supplicant')
    if not os.path.exists(prg):
        prg = '../../wpa_supplicant/wpa_supplicant'
    arg = [prg]
    cmd = subprocess.Popen(arg, stdout=subprocess.PIPE)
    out = cmd.communicate()[0].decode()
    cmd.wait()
    tracing = "Linux tracing" in out

    with HWSimRadio() as (radio, iface):
        arg = [prg, '-BdddW', '-P', pidfile, '-f', logfile,
               '-Dnl80211', '-c', conffile, '-i', iface]
        if tracing:
            arg += ['-T']
        logger.info("Start wpa_supplicant: " + str(arg))
        subprocess.call(arg)
        wpas = WpaSupplicant(ifname=iface)
        if "PONG" not in wpas.request("PING"):
            raise Exception("Could not PING wpa_supplicant")
        if not os.path.exists(pidfile):
            raise Exception("PID file not created")
        if "OK" not in wpas.request("TERMINATE"):
            raise Exception("Could not TERMINATE")
        ev = wpas.wait_event(["CTRL-EVENT-TERMINATING"], timeout=2)
        if ev is None:
            raise Exception("No termination event received")
        for i in range(20):
            if not os.path.exists(pidfile):
                break
            time.sleep(0.1)
        if os.path.exists(pidfile):
            raise Exception("PID file not removed")

@remote_compatible
def test_wpas_ctrl_oom(dev):
    """Various wpa_supplicant ctrl_iface OOM cases"""
    try:
        _test_wpas_ctrl_oom(dev)
    finally:
        dev[0].request("VENDOR_ELEM_REMOVE 1 *")
        dev[0].request("VENDOR_ELEM_REMOVE 2 *")
        dev[0].request("SET bssid_filter ")

def _test_wpas_ctrl_oom(dev):
    dev[0].request('VENDOR_ELEM_ADD 2 000100')
    tests = [('DRIVER_EVENT AVOID_FREQUENCIES 2412', 'FAIL',
              1, 'freq_range_list_parse'),
             ('P2P_SET disallow_freq 2412', 'FAIL',
              1, 'freq_range_list_parse'),
             ('SCAN freq=2412', 'FAIL',
              1, 'freq_range_list_parse'),
             ('INTERWORKING_SELECT freq=2412', 'FAIL',
              1, 'freq_range_list_parse'),
             ('SCAN ssid 112233', 'FAIL',
              1, 'wpas_ctrl_scan'),
             ('MGMT_TX 00:00:00:00:00:00 00:00:00:00:00:00 action=00', 'FAIL',
              1, 'wpas_ctrl_iface_mgmt_tx'),
             ('EAPOL_RX 00:00:00:00:00:00 00', 'FAIL',
              1, 'wpas_ctrl_iface_eapol_rx'),
             ('DATA_TEST_FRAME 00112233445566778899aabbccddee', 'FAIL',
              1, 'wpas_ctrl_iface_data_test_frame'),
             ('DATA_TEST_FRAME 00112233445566778899aabbccddee', 'FAIL',
              1, 'l2_packet_init;wpas_ctrl_iface_data_test_frame'),
             ('VENDOR_ELEM_ADD 1 000100', 'FAIL',
              1, 'wpas_ctrl_vendor_elem_add'),
             ('VENDOR_ELEM_ADD 2 000100', 'FAIL',
              2, 'wpas_ctrl_vendor_elem_add'),
             ('VENDOR_ELEM_REMOVE 2 000100', 'FAIL',
              1, 'wpas_ctrl_vendor_elem_remove'),
             ('SET bssid_filter 00:11:22:33:44:55', 'FAIL',
              1, 'set_bssid_filter'),
             ('SET disallow_aps bssid 00:11:22:33:44:55', 'FAIL',
              1, 'set_disallow_aps'),
             ('SET disallow_aps ssid 11', 'FAIL',
              1, 'set_disallow_aps'),
             ('SET blob foo 0011', 'FAIL',
              1, 'wpas_ctrl_set_blob'),
             ('SET blob foo 0011', 'FAIL',
              2, 'wpas_ctrl_set_blob'),
             ('SET blob foo 0011', 'FAIL',
              3, 'wpas_ctrl_set_blob'),
             ('WPS_NFC_TAG_READ 00', 'FAIL',
              1, 'wpa_supplicant_ctrl_iface_wps_nfc_tag_read'),
             ('WPS_NFC_TOKEN NDEF', 'FAIL',
              1, 'wpa_supplicant_ctrl_iface_wps_nfc_token'),
             ('WPS_NFC_TOKEN NDEF', 'FAIL',
              2, 'wpa_supplicant_ctrl_iface_wps_nfc_token'),
             ('WPS_NFC_TOKEN NDEF', 'FAIL',
              3, 'wpa_supplicant_ctrl_iface_wps_nfc_token'),
             ('WPS_NFC_TOKEN NDEF', 'FAIL',
              4, 'wpa_supplicant_ctrl_iface_wps_nfc_token'),
             ('NFC_REPORT_HANDOVER ROLE TYPE 00 00', 'FAIL',
              1, 'wpas_ctrl_nfc_report_handover'),
             ('NFC_REPORT_HANDOVER ROLE TYPE 00 00', 'FAIL',
              2, 'wpas_ctrl_nfc_report_handover'),
             ('NFC_GET_HANDOVER_REQ NDEF WPS-CR', 'FAIL',
              1, 'wps_build_nfc_handover_req'),
             ('NFC_GET_HANDOVER_REQ NDEF WPS-CR', 'FAIL',
              1, 'ndef_build_record'),
             ('NFC_GET_HANDOVER_REQ NDEF P2P-CR', None,
              1, 'wpas_p2p_nfc_handover'),
             ('NFC_GET_HANDOVER_REQ NDEF P2P-CR', None,
              1, 'wps_build_nfc_handover_req_p2p'),
             ('NFC_GET_HANDOVER_REQ NDEF P2P-CR', 'FAIL',
              1, 'ndef_build_record'),
             ('NFC_GET_HANDOVER_SEL NDEF P2P-CR-TAG', None,
              1, 'wpas_ctrl_nfc_get_handover_sel_p2p'),
             ('NFC_GET_HANDOVER_SEL NDEF P2P-CR', None,
              1, 'wpas_ctrl_nfc_get_handover_sel_p2p'),
             ('P2P_ASP_PROVISION_RESP 00:11:22:33:44:55 id=1', 'FAIL',
              1, 'p2p_parse_asp_provision_cmd'),
             ('P2P_SERV_DISC_REQ 00:11:22:33:44:55 02000001', 'FAIL',
              1, 'p2p_ctrl_serv_disc_req'),
             ('P2P_SERV_DISC_RESP 2412 00:11:22:33:44:55 1 00', 'FAIL',
              1, 'p2p_ctrl_serv_disc_resp'),
             ('P2P_SERVICE_ADD bonjour 0b5f6166706f766572746370c00c000c01 074578616d706c65c027',
              'FAIL',
              1, 'p2p_ctrl_service_add_bonjour'),
             ('P2P_SERVICE_ADD bonjour 0b5f6166706f766572746370c00c000c01 074578616d706c65c027',
              'FAIL',
              2, 'p2p_ctrl_service_add_bonjour'),
             ('P2P_SERVICE_ADD bonjour 0b5f6166706f766572746370c00c000c01 074578616d706c65c027',
              'FAIL',
              3, 'p2p_ctrl_service_add_bonjour'),
             ('P2P_SERVICE_DEL bonjour 0b5f6166706f766572746370c00c000c01',
              'FAIL',
              1, 'p2p_ctrl_service_del_bonjour'),
             ('GAS_REQUEST 00:11:22:33:44:55 00', 'FAIL',
              1, 'gas_request'),
             ('GAS_REQUEST 00:11:22:33:44:55 00 11', 'FAIL',
              2, 'gas_request'),
             ('HS20_GET_NAI_HOME_REALM_LIST 00:11:22:33:44:55 realm=example.com',
             'FAIL',
             1, 'hs20_nai_home_realm_list'),
             ('HS20_GET_NAI_HOME_REALM_LIST 00:11:22:33:44:55 00',
             'FAIL',
             1, 'hs20_get_nai_home_realm_list'),
             ('WNM_SLEEP enter tfs_req=11', 'FAIL',
              1, 'wpas_ctrl_iface_wnm_sleep'),
             ('WNM_SLEEP enter tfs_req=11', 'FAIL',
              2, 'wpas_ctrl_iface_wnm_sleep'),
             ('WNM_SLEEP enter tfs_req=11', 'FAIL',
              3, 'wpas_ctrl_iface_wnm_sleep'),
             ('WNM_SLEEP enter tfs_req=11', 'FAIL',
              4, 'wpas_ctrl_iface_wnm_sleep'),
             ('WNM_SLEEP enter tfs_req=11', 'FAIL',
              5, 'wpas_ctrl_iface_wnm_sleep'),
             ('WNM_SLEEP enter', 'FAIL',
              3, 'wpas_ctrl_iface_wnm_sleep'),
             ('VENDOR 1 1 00', 'FAIL',
              1, 'wpa_supplicant_vendor_cmd'),
             ('VENDOR 1 1 00', 'FAIL',
              2, 'wpa_supplicant_vendor_cmd'),
             ('RADIO_WORK add test', 'FAIL',
              1, 'wpas_ctrl_radio_work_add'),
             ('RADIO_WORK add test', 'FAIL',
              2, 'wpas_ctrl_radio_work_add'),
             ('AUTOSCAN periodic:1', 'FAIL',
              1, 'wpa_supplicant_ctrl_iface_autoscan'),
             ('PING', None,
              1, 'wpa_supplicant_ctrl_iface_process')]
    tls = dev[0].request("GET tls_library")
    if not tls.startswith("internal"):
        tests.append(('NFC_GET_HANDOVER_SEL NDEF P2P-CR-TAG', 'FAIL',
                      4, 'wpas_ctrl_nfc_get_handover_sel_p2p'))
    for cmd, exp, count, func in tests:
        with alloc_fail(dev[0], count, func):
            res = dev[0].request(cmd)
            if exp and exp not in res:
                raise Exception("Unexpected success for '%s' during OOM (%d:%s)" % (cmd, count, func))

    tests = [('FOO', None,
              1, 'wpa_supplicant_global_ctrl_iface_process'),
             ('IFNAME=notfound PING', 'FAIL\n',
              1, 'wpas_global_ctrl_iface_ifname')]
    for cmd, exp, count, func in tests:
        with alloc_fail(dev[0], count, func):
            res = dev[0].global_request(cmd)
            if exp and exp not in res:
                raise Exception("Unexpected success for '%s' during OOM" % cmd)

@remote_compatible
def test_wpas_ctrl_error(dev):
    """Various wpa_supplicant ctrl_iface error cases"""
    tests = [('WPS_NFC_TOKEN NDEF', 'FAIL',
              1, 'wpa_supplicant_ctrl_iface_wps_nfc_token'),
             ('WPS_NFC_TOKEN NDEF', 'FAIL',
              2, 'wpa_supplicant_ctrl_iface_wps_nfc_token'),
             ('NFC_GET_HANDOVER_REQ NDEF P2P-CR', None,
              1, 'wpas_p2p_nfc_handover'),
             ('NFC_GET_HANDOVER_REQ NDEF P2P-CR', None,
              1, 'wps_build_nfc_handover_req_p2p')]
    for cmd, exp, count, func in tests:
        with fail_test(dev[0], count, func):
            res = dev[0].request(cmd)
            if exp and exp not in res:
                raise Exception("Unexpected success for '%s' during failure testing (%d:%s)" % (cmd, count, func))

def test_wpas_ctrl_socket_full(dev, apdev, test_params):
    """wpa_supplicant control socket and full send buffer"""
    if not dev[0].ping():
        raise Exception("Could not ping wpa_supplicant at the beginning of the test")
    dev[0].get_status()

    counter = 0

    s = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
    local = "/tmp/wpa_ctrl_test_%d-%d" % (os.getpid(), counter)
    counter += 1
    s.bind(local)
    s.connect("/var/run/wpa_supplicant/wlan0")
    for i in range(20):
        logger.debug("Command %d" % i)
        try:
            s.send(b"MIB")
        except Exception as e:
            logger.info("Could not send command %d: %s" % (i, str(e)))
            break
        # Close without receiving response
        time.sleep(0.01)

    if not dev[0].ping():
        raise Exception("Could not ping wpa_supplicant in the middle of the test")
    dev[0].get_status()

    s2 = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
    local2 = "/tmp/wpa_ctrl_test_%d-%d" % (os.getpid(), counter)
    counter += 1
    s2.bind(local2)
    s2.connect("/var/run/wpa_supplicant/wlan0")
    for i in range(10):
        logger.debug("Command %d [2]" % i)
        try:
            s2.send(b"MIB")
        except Exception as e:
            logger.info("Could not send command %d [2]: %s" % (i, str(e)))
            break
        # Close without receiving response
        time.sleep(0.01)

    s.close()
    os.unlink(local)

    for i in range(10):
        logger.debug("Command %d [3]" % i)
        try:
            s2.send(b"MIB")
        except Exception as e:
            logger.info("Could not send command %d [3]: %s" % (i, str(e)))
            break
        # Close without receiving response
        time.sleep(0.01)

    s2.close()
    os.unlink(local2)

    if not dev[0].ping():
        raise Exception("Could not ping wpa_supplicant in the middle of the test [2]")
    dev[0].get_status()

    s = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
    local = "/tmp/wpa_ctrl_test_%d-%d" % (os.getpid(), counter)
    counter += 1
    s.bind(local)
    s.connect("/var/run/wpa_supplicant/wlan0")
    s.send(b"ATTACH")
    res = s.recv(100).decode()
    if "OK" not in res:
        raise Exception("Could not attach a test socket")

    for i in range(5):
        dev[0].scan(freq=2412)

    s.close()
    os.unlink(local)

    for i in range(5):
        dev[0].scan(freq=2412)

    if not dev[0].ping():
        raise Exception("Could not ping wpa_supplicant at the end of the test")
    dev[0].get_status()

def test_wpas_ctrl_event_burst(dev, apdev):
    """wpa_supplicant control socket and event burst"""
    if "OK" not in dev[0].request("EVENT_TEST 1000"):
        raise Exception("Could not request event messages")

    total_i = 0
    total_g = 0
    for i in range(100):
        (i, g) = dev[0].dump_monitor()
        total_i += i
        total_g += g
        logger.info("Received i=%d g=%d" % (i, g))
        if total_i >= 1000 and total_g >= 1000:
            break
        time.sleep(0.05)

    if total_i < 1000:
        raise Exception("Some per-interface events not seen: %d" % total_i)
    if total_g < 1000:
        raise Exception("Some global events not seen: %d" % total_g)

    if not dev[0].ping():
        raise Exception("Could not ping wpa_supplicant at the end of the test")

@remote_compatible
def test_wpas_ctrl_sched_scan_plans(dev, apdev):
    """wpa_supplicant sched_scan_plans parsing"""
    dev[0].request("SET sched_scan_plans foo")
    dev[0].request("SET sched_scan_plans 10:100 20:200 30")
    dev[0].request("SET sched_scan_plans 4294967295:0")
    dev[0].request("SET sched_scan_plans 1 1")
    dev[0].request("SET sched_scan_plans  ")
    try:
        with alloc_fail(dev[0], 1, "wpas_sched_scan_plans_set"):
            dev[0].request("SET sched_scan_plans 10:100")
    finally:
        dev[0].request("SET sched_scan_plans ")

def test_wpas_ctrl_signal_monitor(dev, apdev):
    """wpa_supplicant SIGNAL_MONITOR command"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": "open"})
    dev[0].connect("open", key_mgmt="NONE", scan_freq="2412")
    dev[1].connect("open", key_mgmt="NONE", scan_freq="2412",
                   bgscan="simple:1:-45:2")
    dev[2].connect("open", key_mgmt="NONE", scan_freq="2412")

    tests = [" THRESHOLD=-45", " THRESHOLD=-44 HYSTERESIS=5", ""]
    try:
        if "FAIL" in dev[2].request("SIGNAL_MONITOR THRESHOLD=-1 HYSTERESIS=5"):
            raise Exception("SIGNAL_MONITOR command failed")
        for t in tests:
            if "OK" not in dev[0].request("SIGNAL_MONITOR" + t):
                raise Exception("SIGNAL_MONITOR command failed: " + t)
        if "FAIL" not in dev[1].request("SIGNAL_MONITOR THRESHOLD=-44 HYSTERESIS=5"):
            raise Exception("SIGNAL_MONITOR command accepted while using bgscan")
        ev = dev[2].wait_event(["CTRL-EVENT-SIGNAL-CHANGE"], timeout=10)
        if ev is None:
            raise Exception("No signal change event seen")
        if "above=0" not in ev:
            raise Exception("Unexpected signal change event contents: " + ev)
    finally:
        dev[0].request("SIGNAL_MONITOR")
        dev[1].request("SIGNAL_MONITOR")
        dev[2].request("SIGNAL_MONITOR")

    dev[0].request("REMOVE_NETWORK all")
    dev[1].request("REMOVE_NETWORK all")
    dev[1].wait_disconnected()

def test_wpas_ctrl_p2p_listen_offload(dev, apdev):
    """wpa_supplicant P2P_LO_START and P2P_LO_STOP commands"""
    dev[0].request("P2P_LO_STOP")
    dev[0].request("P2P_LO_START ")
    dev[0].request("P2P_LO_START 2412")
    dev[0].request("P2P_LO_START 2412 100 200 3")
    dev[0].request("P2P_LO_STOP")

def test_wpas_ctrl_driver_flags(dev, apdev):
    """DRIVER_FLAGS command"""
    params = hostapd.wpa2_params(ssid="test", passphrase="12345678")
    hapd = hostapd.add_ap(apdev[0], params)
    hapd_flags = hapd.request("DRIVER_FLAGS")
    wpas_flags = dev[0].request("DRIVER_FLAGS")
    if "FAIL" in hapd_flags:
        raise Exception("DRIVER_FLAGS failed")
    if hapd_flags != wpas_flags:
        raise Exception("Unexpected difference in hostapd vs. wpa_supplicant DRIVER_FLAGS output")
    logger.info("DRIVER_FLAGS: " + hapd_flags)
    flags = hapd_flags.split('\n')
    if 'AP' not in flags:
        raise Exception("AP flag missing from DRIVER_FLAGS")

def test_wpas_ctrl_bss_current(dev, apdev):
    """wpa_supplicant BSS CURRENT command"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": "open"})
    bssid = hapd.own_addr()
    res = dev[0].request("BSS CURRENT")
    if res != '':
        raise Exception("Unexpected BSS CURRENT response in disconnected state")
    dev[0].connect("open", key_mgmt="NONE", scan_freq="2412")
    res = dev[0].request("BSS CURRENT")
    if bssid not in res:
        raise Exception("Unexpected BSS CURRENT response in connected state")

def test_wpas_ctrl_set_lci_errors(dev):
    """wpa_supplicant SET lci error cases"""
    if "FAIL" not in dev[0].request("SET lci q"):
        raise Exception("Invalid LCI value accepted")

    with fail_test(dev[0], 1, "os_get_reltime;wpas_ctrl_iface_set_lci"):
        if "FAIL" not in dev[0].request("SET lci 00"):
            raise Exception("SET lci accepted with failing os_get_reltime")

def test_wpas_ctrl_set_radio_disabled(dev):
    """wpa_supplicant SET radio_disabled"""
    # This is not currently supported with nl80211, but execute the commands
    # without checking the result for some additional code coverage.
    dev[0].request("SET radio_disabled 1")
    dev[0].request("SET radio_disabled 0")

def test_wpas_ctrl_set_tdls_trigger_control(dev):
    """wpa_supplicant SET tdls_trigger_control"""
    # This is not supported with upstream nl80211, but execute the commands
    # without checking the result for some additional code coverage.
    dev[0].request("SET tdls_trigger_control 1")
    dev[0].request("SET tdls_trigger_control 0")

def test_wpas_ctrl_set_sched_scan_relative_rssi(dev):
    """wpa_supplicant SET relative RSSI"""
    tests = ["relative_rssi -1",
             "relative_rssi 101",
             "relative_band_adjust 2G",
             "relative_band_adjust 2G:-101",
             "relative_band_adjust 2G:101",
             "relative_band_adjust 3G:1"]
    for t in tests:
        if "FAIL" not in dev[0].request("SET " + t):
            raise Exception("No failure reported for SET " + t)

    tests = ["relative_rssi 0",
             "relative_rssi 10",
             "relative_rssi disable",
             "relative_band_adjust 2G:-1",
             "relative_band_adjust 2G:0",
             "relative_band_adjust 2G:1",
             "relative_band_adjust 5G:-1",
             "relative_band_adjust 5G:1",
             "relative_band_adjust 5G:0"]
    for t in tests:
        if "OK" not in dev[0].request("SET " + t):
            raise Exception("Failed to SET " + t)

def test_wpas_ctrl_get_pref_freq_list_override(dev):
    """wpa_supplicant get_pref_freq_list_override"""
    if dev[0].request("GET_PREF_FREQ_LIST ").strip() != "FAIL":
        raise Exception("Invalid GET_PREF_FREQ_LIST accepted")

    dev[0].set("get_pref_freq_list_override", "foo")
    res = dev[0].request("GET_PREF_FREQ_LIST STATION").strip()
    if res != "FAIL":
        raise Exception("Unexpected GET_PREF_FREQ_LIST response: " + res)

    dev[0].set("get_pref_freq_list_override", "1234:1,2,3 0")
    res = dev[0].request("GET_PREF_FREQ_LIST STATION").strip()
    if res != "FAIL":
        raise Exception("Unexpected GET_PREF_FREQ_LIST response: " + res)

    dev[0].set("get_pref_freq_list_override", "1234:1,2,3 0:")
    res = dev[0].request("GET_PREF_FREQ_LIST STATION").strip()
    if res != "0":
        raise Exception("Unexpected GET_PREF_FREQ_LIST response: " + res)

    dev[0].set("get_pref_freq_list_override", "0:1,2")
    res = dev[0].request("GET_PREF_FREQ_LIST STATION").strip()
    if res != "1,2":
        raise Exception("Unexpected GET_PREF_FREQ_LIST response: " + res)

    dev[0].set("get_pref_freq_list_override", "1:3,4 0:1,2 2:5,6")
    res = dev[0].request("GET_PREF_FREQ_LIST STATION").strip()
    if res != "1,2":
        raise Exception("Unexpected GET_PREF_FREQ_LIST response: " + res)

    dev[0].set("get_pref_freq_list_override", "1:3,4 0:1 2:5,6")
    res = dev[0].request("GET_PREF_FREQ_LIST STATION").strip()
    if res != "1":
        raise Exception("Unexpected GET_PREF_FREQ_LIST response: " + res)

    dev[0].set("get_pref_freq_list_override", "0:1000,1001 2:1002,1003 3:1004,1005 4:1006,1007 8:1010,1011 9:1008,1009")
    res = dev[0].request("GET_PREF_FREQ_LIST STATION").strip()
    if res != "1000,1001":
        raise Exception("Unexpected GET_PREF_FREQ_LIST response: " + res)
    res = dev[0].request("GET_PREF_FREQ_LIST AP").strip()
    if res != "1002,1003":
        raise Exception("Unexpected GET_PREF_FREQ_LIST response: " + res)
    res = dev[0].request("GET_PREF_FREQ_LIST P2P_GO").strip()
    if res != "1004,1005":
        raise Exception("Unexpected GET_PREF_FREQ_LIST response: " + res)
    res = dev[0].request("GET_PREF_FREQ_LIST P2P_CLIENT").strip()
    if res != "1006,1007":
        raise Exception("Unexpected GET_PREF_FREQ_LIST response: " + res)
    res = dev[0].request("GET_PREF_FREQ_LIST IBSS").strip()
    if res != "1008,1009":
        raise Exception("Unexpected GET_PREF_FREQ_LIST response: " + res)
    res = dev[0].request("GET_PREF_FREQ_LIST TDLS").strip()
    if res != "1010,1011":
        raise Exception("Unexpected GET_PREF_FREQ_LIST response: " + res)

    dev[0].set("get_pref_freq_list_override", "")
    res = dev[0].request("GET_PREF_FREQ_LIST STATION").strip()
    logger.info("STATION (without override): " + res)
