# hostapd configuration tests
# Copyright (c) 2014-2016, Jouni Malinen <j@w1.fi>
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

import os
import signal
import time
import logging
logger = logging.getLogger(__name__)
import subprocess

from remotehost import remote_compatible
import hostapd
from utils import alloc_fail, fail_test

@remote_compatible
def test_ap_config_errors(dev, apdev):
    """Various hostapd configuration errors"""

    # IEEE 802.11d without country code
    params = {"ssid": "foo", "ieee80211d": "1"}
    hapd = hostapd.add_ap(apdev[0], params, no_enable=True)
    if "FAIL" not in hapd.request("ENABLE"):
        raise Exception("Unexpected ENABLE success (ieee80211d without country_code)")
    hostapd.remove_bss(apdev[0])

    # IEEE 802.11h without IEEE 802.11d
    params = {"ssid": "foo", "ieee80211h": "1"}
    hapd = hostapd.add_ap(apdev[0], params, no_enable=True)
    if "FAIL" not in hapd.request("ENABLE"):
        raise Exception("Unexpected ENABLE success (ieee80211h without ieee80211d")
    hostapd.remove_bss(apdev[0])

    # Power Constraint without IEEE 802.11d
    params = {"ssid": "foo", "local_pwr_constraint": "1"}
    hapd = hostapd.add_ap(apdev[0], params, no_enable=True)
    if "FAIL" not in hapd.request("ENABLE"):
        raise Exception("Unexpected ENABLE success (local_pwr_constraint without ieee80211d)")
    hostapd.remove_bss(apdev[0])

    # Spectrum management without Power Constraint
    params = {"ssid": "foo", "spectrum_mgmt_required": "1"}
    hapd = hostapd.add_ap(apdev[0], params, no_enable=True)
    if "FAIL" not in hapd.request("ENABLE"):
        raise Exception("Unexpected ENABLE success (spectrum_mgmt_required without local_pwr_constraint)")
    hostapd.remove_bss(apdev[0])

    # IEEE 802.1X without authentication server
    params = {"ssid": "foo", "ieee8021x": "1"}
    hapd = hostapd.add_ap(apdev[0], params, no_enable=True)
    if "FAIL" not in hapd.request("ENABLE"):
        raise Exception("Unexpected ENABLE success (ieee8021x)")
    hostapd.remove_bss(apdev[0])

    # RADIUS-PSK without macaddr_acl=2
    params = hostapd.wpa2_params(ssid="foo", passphrase="12345678")
    params["wpa_psk_radius"] = "1"
    hapd = hostapd.add_ap(apdev[0], params, no_enable=True)
    if "FAIL" not in hapd.request("ENABLE"):
        raise Exception("Unexpected ENABLE success (wpa_psk_radius)")
    hostapd.remove_bss(apdev[0])

    # FT without NAS-Identifier
    params = {"wpa": "2",
              "wpa_key_mgmt": "FT-PSK",
              "rsn_pairwise": "CCMP",
              "wpa_passphrase": "12345678"}
    hapd = hostapd.add_ap(apdev[0], params, no_enable=True)
    if "FAIL" not in hapd.request("ENABLE"):
        raise Exception("Unexpected ENABLE success (FT without nas_identifier)")
    hostapd.remove_bss(apdev[0])

    # Hotspot 2.0 without WPA2/CCMP
    params = hostapd.wpa2_params(ssid="foo")
    params['wpa_key_mgmt'] = "WPA-EAP"
    params['ieee8021x'] = "1"
    params['auth_server_addr'] = "127.0.0.1"
    params['auth_server_port'] = "1812"
    params['auth_server_shared_secret'] = "radius"
    params['interworking'] = "1"
    params['hs20'] = "1"
    params['wpa'] = "1"
    hapd = hostapd.add_ap(apdev[0], params, no_enable=True)
    if "FAIL" not in hapd.request("ENABLE"):
        raise Exception("Unexpected ENABLE success (HS 2.0 without WPA2/CCMP)")
    hostapd.remove_bss(apdev[0])

def test_ap_config_reload(dev, apdev, params):
    """hostapd configuration reload"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": "foo"})
    hapd.set("ssid", "foobar")
    with open(os.path.join(params['logdir'], 'hostapd-test.pid'), "r") as f:
        pid = int(f.read())
    os.kill(pid, signal.SIGHUP)
    time.sleep(0.1)
    dev[0].connect("foobar", key_mgmt="NONE", scan_freq="2412")
    hapd.set("ssid", "foo")
    os.kill(pid, signal.SIGHUP)
    dev[0].wait_disconnected()
    dev[0].request("DISCONNECT")

def test_ap_config_reload_file(dev, apdev, params):
    """hostapd configuration reload from file"""
    hapd = hostapd.add_iface(apdev[0], "bss-1.conf")
    hapd.enable()
    hapd.set("ssid", "foobar")
    with open(os.path.join(params['logdir'], 'hostapd-test.pid'), "r") as f:
        pid = int(f.read())
    os.kill(pid, signal.SIGHUP)
    time.sleep(0.1)
    dev[0].connect("foobar", key_mgmt="NONE", scan_freq="2412")
    hapd.set("ssid", "foo")
    os.kill(pid, signal.SIGHUP)
    dev[0].wait_disconnected()
    dev[0].request("DISCONNECT")

def test_ap_config_reload_file_while_disabled(dev, apdev, params):
    """hostapd configuration reload from file when disabled"""
    hapd = hostapd.add_iface(apdev[0], "bss-1.conf")
    hapd.enable()
    ev = hapd.wait_event(["AP-ENABLED"], timeout=3)
    if ev is None:
        raise Exception("AP-ENABLED event not reported")
    hapd.set("ssid", "foobar")
    with open(os.path.join(params['logdir'], 'hostapd-test.pid'), "r") as f:
        pid = int(f.read())
    hapd.disable()
    ev = hapd.wait_event(["AP-DISABLED"], timeout=3)
    if ev is None:
        raise Exception("AP-DISABLED event not reported")
    hapd.dump_monitor()
    os.kill(pid, signal.SIGHUP)
    time.sleep(0.1)
    hapd.enable()
    dev[0].connect("foobar", key_mgmt="NONE", scan_freq="2412")

def write_hostapd_config(conffile, ifname, ssid, ht=True, bss2=False):
    with open(conffile, "w") as f:
        f.write("driver=nl80211\n")
        f.write("hw_mode=g\n")
        f.write("channel=1\n")
        if ht:
            f.write("ieee80211n=1\n")
        f.write("interface=" + ifname + "\n")
        f.write("ssid=" + ssid + "\n")
        if bss2:
            f.write("bss=" + ifname + "_2\n")
            f.write("ssid=" + ssid + "-2\n")

def test_ap_config_reload_on_sighup(dev, apdev, params):
    """hostapd configuration reload modification from file on SIGHUP"""
    run_ap_config_reload_on_sighup(dev, apdev, params)

def test_ap_config_reload_on_sighup_no_ht(dev, apdev, params):
    """hostapd configuration reload modification from file on SIGHUP (no HT)"""
    run_ap_config_reload_on_sighup(dev, apdev, params, ht=False)

def run_ap_config_reload_on_sighup(dev, apdev, params, ht=True):
    name = "ap_config_reload_on_sighup"
    if not ht:
        name += "_no_ht"
    pidfile = params['prefix'] + ".hostapd.pid"
    logfile = params['prefix'] + ".hostapd.log"
    conffile = params['prefix'] + ".hostapd.conf"
    prg = os.path.join(params['logdir'], 'alt-hostapd/hostapd/hostapd')
    if not os.path.exists(prg):
        prg = '../../hostapd/hostapd'
    write_hostapd_config(conffile, apdev[0]['ifname'], "test-1", ht=ht)
    cmd = [prg, '-B', '-dddt', '-P', pidfile, '-f', logfile, conffile]
    res = subprocess.check_call(cmd)
    if res != 0:
        raise Exception("Could not start hostapd: %s" % str(res))

    dev[0].connect("test-1", key_mgmt="NONE", scan_freq="2412")
    dev[0].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected()

    write_hostapd_config(conffile, apdev[0]['ifname'], "test-2", ht=ht)
    with open(pidfile, "r") as f:
        pid = int(f.read())
    os.kill(pid, signal.SIGHUP)

    time.sleep(0.1)
    dev[0].flush_scan_cache()

    dev[0].connect("test-2", key_mgmt="NONE", scan_freq="2412")
    bss = dev[0].get_bss(apdev[0]['bssid'])
    dev[0].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected()

    os.kill(pid, signal.SIGTERM)
    removed = False
    for i in range(20):
        time.sleep(0.1)
        if not os.path.exists(pidfile):
            removed = True
            break
    if not removed:
        raise Exception("hostapd PID file not removed on SIGTERM")

    if ht and "dd180050f202" not in bss['ie']:
            raise Exception("Missing WMM IE after reload")
    if not ht and "dd180050f202" in bss['ie']:
            raise Exception("Unexpected WMM IE after reload")

def test_ap_config_reload_on_sighup_bss_changes(dev, apdev, params):
    """hostapd configuration reload modification from file on SIGHUP with bss remove/add"""
    pidfile = params['prefix'] + ".hostapd.pid"
    logfile = params['prefix'] + ".hostapd-log"
    conffile = params['prefix'] + ".hostapd.conf"
    prg = os.path.join(params['logdir'], 'alt-hostapd/hostapd/hostapd')
    if not os.path.exists(prg):
        prg = '../../hostapd/hostapd'
    write_hostapd_config(conffile, apdev[0]['ifname'], "test", bss2=True)
    cmd = [prg, '-B', '-dddt', '-P', pidfile, '-f', logfile, conffile]
    res = subprocess.check_call(cmd)
    if res != 0:
        raise Exception("Could not start hostapd: %s" % str(res))

    dev[0].connect("test", key_mgmt="NONE", scan_freq="2412",
                   wait_connect=False)
    dev[1].connect("test-2", key_mgmt="NONE", scan_freq="2412")
    dev[0].wait_connected()
    dev[0].request("REMOVE_NETWORK all")
    dev[1].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected()
    dev[1].wait_disconnected()
    dev[0].dump_monitor()
    dev[1].dump_monitor()

    write_hostapd_config(conffile, apdev[0]['ifname'], "test-a", bss2=False)
    with open(pidfile, "r") as f:
        pid = int(f.read())
    os.kill(pid, signal.SIGHUP)

    time.sleep(0.5)
    dev[0].flush_scan_cache()

    dev[0].connect("test-a", key_mgmt="NONE", scan_freq="2412")
    dev[0].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected()
    dev[0].dump_monitor()

    write_hostapd_config(conffile, apdev[0]['ifname'], "test-b", bss2=True)
    os.kill(pid, signal.SIGHUP)

    time.sleep(0.5)
    dev[0].flush_scan_cache()
    dev[1].flush_scan_cache()

    dev[0].connect("test-b", key_mgmt="NONE", scan_freq="2412",
                   wait_connect=False)
    dev[1].connect("test-b-2", key_mgmt="NONE", scan_freq="2412")
    dev[0].wait_connected()
    dev[0].request("REMOVE_NETWORK all")
    dev[1].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected()
    dev[1].wait_disconnected()
    dev[0].dump_monitor()
    dev[1].dump_monitor()

    os.kill(pid, signal.SIGTERM)

def test_ap_config_reload_before_enable(dev, apdev, params):
    """hostapd configuration reload before enable"""
    hapd = hostapd.add_iface(apdev[0], "bss-1.conf")
    with open(os.path.join(params['logdir'], 'hostapd-test.pid'), "r") as f:
        pid = int(f.read())
    os.kill(pid, signal.SIGHUP)
    hapd.ping()

def test_ap_config_sigusr1(dev, apdev, params):
    """hostapd SIGUSR1"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": "foobar"})
    with open(os.path.join(params['logdir'], 'hostapd-test.pid'), "r") as f:
        pid = int(f.read())
    os.kill(pid, signal.SIGUSR1)
    dev[0].connect("foobar", key_mgmt="NONE", scan_freq="2412")
    os.kill(pid, signal.SIGUSR1)

def test_ap_config_invalid_value(dev, apdev, params):
    """Ignoring invalid hostapd configuration parameter updates"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": "test"}, no_enable=True)
    not_exist = "/tmp/hostapd-test/does-not-exist"
    tests = [("driver", "foobar"),
             ("ssid2", "Q"),
             ("macaddr_acl", "255"),
             ("accept_mac_file", not_exist),
             ("deny_mac_file", not_exist),
             ("eapol_version", "255"),
             ("eap_user_file", not_exist),
             ("wep_key_len_broadcast", "-1"),
             ("wep_key_len_unicast", "-1"),
             ("wep_rekey_period", "-1"),
             ("eap_rekey_period", "-1"),
             ("radius_client_addr", "foo"),
             ("acs_chan_bias", "-1:0.8"),
             ("acs_chan_bias", "1"),
             ("acs_chan_bias", "1:p"),
             ("acs_chan_bias", "1:-0.8"),
             ("acs_chan_bias", "1:0.8p"),
             ("dtim_period", "0"),
             ("bss_load_update_period", "-1"),
             ("send_probe_response", "255"),
             ("beacon_rate", "ht:-1"),
             ("beacon_rate", "ht:32"),
             ("beacon_rate", "vht:-1"),
             ("beacon_rate", "vht:10"),
             ("beacon_rate", "9"),
             ("beacon_rate", "10001"),
             ("vlan_file", not_exist),
             ("bss", ""),
             ("bssid", "foo"),
             ("extra_cred", not_exist),
             ("anqp_elem", "265"),
             ("anqp_elem", "265"),
             ("anqp_elem", "265:1"),
             ("anqp_elem", "265:1q"),
             ("fst_priority", ""),
             ("fils_cache_id", "q"),
             ("venue_url", "foo"),
             ("venue_url", "1:" + 255*"a"),
             ("sae_password", "secret|mac=qq"),
             ("dpp_controller", "ipaddr=1"),
             ("dpp_controller", "ipaddr=127.0.0.1 pkhash=q"),
             ("dpp_controller", "ipaddr=127.0.0.1 pkhash=" + 32*"qq"),
             ("dpp_controller", "pkhash=" + 32*"aa"),
             ("check_cert_subject", ""),
             ("eap_teap_auth", "-1"),
             ("eap_teap_auth", "100"),
             ("group_cipher", "foo"),
             ("group_cipher", "NONE"),
             ("chan_util_avg_period", "-1"),
             ("multi_ap_backhaul_ssid", ""),
             ("multi_ap_backhaul_ssid", '""'),
             ("multi_ap_backhaul_ssid", "1"),
             ("multi_ap_backhaul_ssid", '"' + 33*"A" + '"'),
             ("multi_ap_backhaul_wpa_passphrase", ""),
             ("multi_ap_backhaul_wpa_passphrase", 64*"q"),
             ("multi_ap_backhaul_wpa_psk", "q"),
             ("multi_ap_backhaul_wpa_psk", 63*"aa"),
             ("hs20_release", "0"),
             ("hs20_release", "255"),
             ("dhcp_server", "::::::"),
             ("dpp_netaccesskey", "q"),
             ("dpp_csign", "q"),
             ("owe_transition_bssid", "q"),
             ("owe_transition_ssid", ""),
             ("owe_transition_ssid", '""'),
             ("owe_transition_ssid", '"' + 33*"a" + '"'),
             ("multi_ap", "-1"),
             ("multi_ap", "255"),
             ("unknown-item", "foo")]
    for field, val in tests:
        if "FAIL" not in hapd.request("SET %s %s" % (field, val)):
            raise Exception("Invalid %s accepted" % field)
    hapd.enable()
    dev[0].connect("test", key_mgmt="NONE", scan_freq="2412")

def test_ap_config_eap_user_file_parsing(dev, apdev, params):
    """hostapd eap_user_file parsing"""
    tmp = params['prefix'] + '.tmp'
    hapd = hostapd.add_ap(apdev[0], {"ssid": "foobar"})

    for i in range(2):
        if "OK" not in hapd.request("SET eap_user_file auth_serv/eap_user.conf"):
            raise Exception("eap_user_file rejected")

    tests = ["#\n\n*\tTLS\nradius_accept_attr=:",
             "foo\n",
             "\"foo\n",
             "\"foo\"\n",
             "\"foo\" FOOBAR\n",
             "\"foo\" " + 10*"TLS," + "TLS \"\n",
             "\"foo\" TLS \nfoo\n",
             "\"foo\" PEAP hash:foo\n",
             "\"foo\" PEAP hash:8846f7eaee8fb117ad06bdd830b7586q\n",
             "\"foo\" PEAP 01020\n",
             "\"foo\" PEAP 010q\n"
             '"pwd" PWD ssha1:\n',
             '"pwd" PWD ssha1:' + 20*'00' + '\n',
             '"pwd" PWD ssha256:\n',
             '"pwd" PWD ssha512:\n',
             '"pwd" PWD ssha1:' + 20*'00' + 'qq\n',
             '"pwd" PWD ssha1:' + 19*'00' + 'qq00\n',
             "\"foo\" TLS\nradius_accept_attr=123:x:012\n",
             "\"foo\" TLS\nradius_accept_attr=123:x:012q\n",
             "\"foo\" TLS\nradius_accept_attr=123:Q:01\n",
             "\"foo\" TLS\nradius_accept_attr=123\nfoo\n"]
    for t in tests:
        with open(tmp, "w") as f:
            f.write(t)
        if "FAIL" not in hapd.request("SET eap_user_file " + tmp):
            raise Exception("Invalid eap_user_file accepted")

    tests = [("\"foo\" TLS\n", 2, "hostapd_config_read_eap_user"),
             ("\"foo\" PEAP \"foo\"\n", 3, "hostapd_config_read_eap_user"),
             ("\"foo\" PEAP hash:8846f7eaee8fb117ad06bdd830b75861\n", 3,
              "hostapd_config_read_eap_user"),
             ("\"foo\" PEAP 0102\n", 3, "hostapd_config_read_eap_user"),
             ("\"foo\" TLS\nradius_accept_attr=123\n", 1,
              "=hostapd_parse_radius_attr"),
             ("\"foo\" TLS\nradius_accept_attr=123\n", 1,
              "wpabuf_alloc;hostapd_parse_radius_attr"),
             ("\"foo\" TLS\nradius_accept_attr=123:s:foo\n", 2,
              "hostapd_parse_radius_attr"),
             ("\"foo\" TLS\nradius_accept_attr=123:x:0102\n", 2,
              "hostapd_parse_radius_attr"),
             ("\"foo\" TLS\nradius_accept_attr=123:d:1\n", 2,
              "hostapd_parse_radius_attr"),
             ('"pwd" PWD ssha1:046239e0660a59015231082a071c803e9f5848ae42eaccb4c08c97ae397bc879c4b071b9088ee715\n', 1, "hostapd_config_eap_user_salted"),
             ('"pwd" PWD ssha1:046239e0660a59015231082a071c803e9f5848ae42eaccb4c08c97ae397bc879c4b071b9088ee715\n', 2, "hostapd_config_eap_user_salted"),
             ("* TLS\n", 1, "hostapd_config_read_eap_user")]
    for t, count, func in tests:
        with alloc_fail(hapd, count, func):
            with open(tmp, "w") as f:
                f.write(t)
            if "FAIL" not in hapd.request("SET eap_user_file " + tmp):
                raise Exception("eap_user_file accepted during OOM")

def test_ap_config_set_oom(dev, apdev):
    """hostapd configuration parsing OOM"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": "foobar"})

    tests = [(1, "hostapd_parse_das_client",
              "SET radius_das_client 192.168.1.123 pw"),
             (1, "hostapd_parse_chanlist", "SET chanlist 1 6 11-13"),
             (1, "hostapd_config_bss", "SET bss foo"),
             (2, "hostapd_config_bss", "SET bss foo"),
             (3, "hostapd_config_bss", "SET bss foo"),
             (1, "add_r0kh",
              "SET r0kh 02:01:02:03:04:05 r0kh-1.example.com 000102030405060708090a0b0c0d0e0f"),
             (1, "add_r1kh",
              "SET r1kh 02:01:02:03:04:05 02:11:22:33:44:55 000102030405060708090a0b0c0d0e0f"),
             (1, "parse_roaming_consortium", "SET roaming_consortium 021122"),
             (1, "parse_lang_string", "SET venue_name eng:Example venue"),
             (1, "parse_3gpp_cell_net",
              "SET anqp_3gpp_cell_net 244,91;310,026;234,56"),
             (1, "parse_nai_realm", "SET nai_realm 0,example.com;example.net"),
             (2, "parse_nai_realm", "SET nai_realm 0,example.com;example.net"),
             (1, "parse_anqp_elem", "SET anqp_elem 265:0000"),
             (2, "parse_anqp_elem", "SET anqp_elem 266:000000"),
             (1, "parse_venue_url", "SET venue_url 1:http://example.com/"),
             (1, "hs20_parse_operator_icon", "SET operator_icon icon"),
             (2, "hs20_parse_operator_icon", "SET operator_icon icon"),
             (1, "hs20_parse_conn_capab", "SET hs20_conn_capab 1:0:2"),
             (1, "hs20_parse_wan_metrics",
              "SET hs20_wan_metrics 01:8000:1000:80:240:3000"),
             (1, "hs20_parse_icon",
              "SET hs20_icon 32:32:eng:image/png:icon32:/tmp/icon32.png"),
             (1, "hs20_parse_osu_server_uri",
              "SET osu_server_uri https://example.com/osu/"),
             (1, "hostapd_config_parse_acs_chan_bias",
              "SET acs_chan_bias 1:0.8 6:0.8 11:0.8"),
             (2, "hostapd_config_parse_acs_chan_bias",
              "SET acs_chan_bias 1:0.8 6:0.8 11:0.8"),
             (1, "parse_wpabuf_hex", "SET vendor_elements 01020304"),
             (1, "parse_fils_realm", "SET fils_realm example.com"),
             (1, "parse_sae_password", "SET sae_password secret"),
             (2, "parse_sae_password", "SET sae_password secret"),
             (2, "parse_sae_password", "SET sae_password secret|id=pw"),
             (3, "parse_sae_password", "SET sae_password secret|id=pw"),
             (1, "hostapd_dpp_controller_parse", "SET dpp_controller ipaddr=127.0.0.1 pkhash=" + 32*"11"),
             (1, "hostapd_config_fill", "SET check_cert_subject foo"),
             (1, "hostapd_config_fill", "SET multi_ap_backhaul_wpa_psk " + 64*"00"),
             (1, "hostapd_parse_intlist;hostapd_config_fill",
              "SET owe_groups 19"),
             (1, "hostapd_config_fill",
              "SET pac_opaque_encr_key 000102030405060708090a0b0c0d0e0f"),
             (1, "hostapd_config_fill", "SET eap_message hello"),
             (1, "hostapd_config_fill",
              "SET wpa_psk 0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"),
             (1, "hostapd_config_fill", "SET time_zone EST5"),
             (1, "hostapd_config_fill",
              "SET network_auth_type 02http://www.example.com/redirect/"),
             (1, "hostapd_config_fill", "SET domain_name example.com"),
             (1, "hostapd_config_fill", "SET hs20_operating_class 5173"),
             (1, "hostapd_config_fill", "SET own_ie_override 11223344"),
             (1, "hostapd_parse_intlist", "SET sae_groups 19 25"),
             (1, "hostapd_parse_intlist", "SET basic_rates 10 20 55 110"),
             (1, "hostapd_parse_intlist", "SET supported_rates 10 20 55 110")]
    if "WEP40" in dev[0].get_capability("group"):
        tests += [(1, "hostapd_config_read_wep", "SET wep_key0 \"hello\""),
                  (1, "hostapd_config_read_wep", "SET wep_key0 0102030405")]
    for count, func, cmd in tests:
        with alloc_fail(hapd, count, func):
            if "FAIL" not in hapd.request(cmd):
                raise Exception("Command accepted during OOM: " + cmd)

    hapd.set("hs20_icon", "32:32:eng:image/png:icon32:/tmp/icon32.png")
    hapd.set("hs20_conn_capab", "1:0:2")
    hapd.set("nai_realm", "0,example.com;example.net")
    hapd.set("venue_name", "eng:Example venue")
    hapd.set("roaming_consortium", "021122")
    hapd.set("osu_server_uri", "https://example.com/osu/")
    hapd.set("vendor_elements", "01020304")
    hapd.set("vendor_elements", "01020304")
    hapd.set("vendor_elements", "")
    hapd.set("lci", "11223344")
    hapd.set("civic", "11223344")
    hapd.set("lci", "")
    hapd.set("civic", "")

    tests = [(1, "hs20_parse_icon",
              "SET hs20_icon 32:32:eng:image/png:icon32:/tmp/icon32.png"),
             (1, "parse_roaming_consortium", "SET roaming_consortium 021122"),
             (2, "parse_nai_realm", "SET nai_realm 0,example.com;example.net"),
             (1, "parse_lang_string", "SET venue_name eng:Example venue"),
             (1, "hs20_parse_osu_server_uri",
              "SET osu_server_uri https://example.com/osu/"),
             (1, "hs20_parse_osu_nai", "SET osu_nai anonymous@example.com"),
             (1, "hs20_parse_osu_nai2", "SET osu_nai2 anonymous@example.com"),
             (1, "hostapd_parse_intlist", "SET osu_method_list 1 0"),
             (1, "hs20_parse_osu_icon", "SET osu_icon icon32"),
             (2, "hs20_parse_osu_icon", "SET osu_icon icon32"),
             (2, "hs20_parse_osu_icon", "SET osu_icon icon32"),
             (1, "hs20_parse_conn_capab", "SET hs20_conn_capab 1:0:2")]
    for count, func, cmd in tests:
        with alloc_fail(hapd, count, func):
            if "FAIL" not in hapd.request(cmd):
                raise Exception("Command accepted during OOM (2): " + cmd)

    tests = [(1, "parse_fils_realm", "SET fils_realm example.com")]
    for count, func, cmd in tests:
        with fail_test(hapd, count, func):
            if "FAIL" not in hapd.request(cmd):
                raise Exception("Command accepted during FAIL_TEST: " + cmd)

def test_ap_config_set_errors(dev, apdev):
    """hostapd configuration parsing errors"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": "foobar"})
    if "WEP40" in dev[0].get_capability("group"):
        hapd.set("wep_key0", '"hello"')
        hapd.set("wep_key1", '"hello"')
        hapd.set("wep_key0", '')
        hapd.set("wep_key0", '"hello"')
        if "FAIL" not in hapd.request("SET wep_key1 \"hello\""):
            raise Exception("SET wep_key1 allowed to override existing key")
        hapd.set("wep_key1", '')
        hapd.set("wep_key1", '"hello"')

    hapd.set("auth_server_addr", "127.0.0.1")
    hapd.set("acct_server_addr", "127.0.0.1")

    hapd.set("fst_group_id", "hello")
    if "FAIL" not in hapd.request("SET fst_group_id hello2"):
        raise Exception("Duplicate fst_group_id accepted")

    tests = ["SET eap_reauth_period -1",
             "SET fst_llt ",
             "SET auth_server_addr_replace foo",
             "SET acct_server_addr_replace foo"]
    for t in tests:
        if "FAIL" not in hapd.request(t):
            raise Exception("Invalid command accepted: " + t)

    # Deprecated entries
    hapd.set("tx_queue_after_beacon_aifs", '2')
    hapd.set("tx_queue_beacon_aifs", '2')
    hapd.set("tx_queue_data9_aifs", '2')
    hapd.set("debug", '1')
    hapd.set("dump_file", '/tmp/hostapd-test-dump')
    hapd.set("eap_authenticator", '0')
    hapd.set("radio_measurements", '0')
    hapd.set("radio_measurements", '1')
    hapd.set("peerkey", "0")

    # Various extra coverage (not really errors)
    hapd.set("logger_syslog_level", '1')
    hapd.set("logger_syslog", '0')
    hapd.set("ctrl_interface_group", '4')
    hapd.set("tls_flags", "[ALLOW-SIGN-RSA-MD5][DISABLE-TIME-CHECKS][DISABLE-TLSv1.0]")

    for i in range(50000):
        if "OK" not in hapd.request("SET hs20_conn_capab 17:5060:0"):
            logger.info("hs20_conn_capab limit at %d" % i)
            break
    if i < 1000 or i >= 49999:
        raise Exception("hs20_conn_capab limit not seen")
