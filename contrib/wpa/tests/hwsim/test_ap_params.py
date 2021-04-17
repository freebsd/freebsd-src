# Test various AP mode parameters
# Copyright (c) 2014, Qualcomm Atheros, Inc.
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

from remotehost import remote_compatible
import logging
logger = logging.getLogger()
import os
import struct
import subprocess
import time

import hwsim_utils
import hostapd
from tshark import run_tshark
from utils import *

@remote_compatible
def test_ap_fragmentation_rts_set_high(dev, apdev):
    """WPA2-PSK AP with fragmentation and RTS thresholds larger than frame length"""
    ssid = "test-wpa2-psk"
    passphrase = 'qwertyuiop'
    params = hostapd.wpa2_params(ssid=ssid, passphrase=passphrase)
    params['rts_threshold'] = "1000"
    params['fragm_threshold'] = "2000"
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].connect(ssid, psk=passphrase, scan_freq="2412")
    hwsim_utils.test_connectivity(dev[0], hapd)
    dev[0].request("DISCONNECT")
    hapd.disable()
    hapd.set('fragm_threshold', '-1')
    hapd.set('rts_threshold', '-1')
    hapd.enable()

@remote_compatible
def test_ap_fragmentation_open(dev, apdev):
    """Open AP with fragmentation threshold"""
    ssid = "fragmentation"
    params = {}
    params['ssid'] = ssid
    params['fragm_threshold'] = "1000"
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].connect(ssid, key_mgmt="NONE", scan_freq="2412")
    hwsim_utils.test_connectivity(dev[0], hapd)
    dev[0].request("DISCONNECT")
    hapd.disable()
    hapd.set('fragm_threshold', '-1')
    hapd.enable()

@remote_compatible
def test_ap_fragmentation_wpa2(dev, apdev):
    """WPA2-PSK AP with fragmentation threshold"""
    ssid = "test-wpa2-psk"
    passphrase = 'qwertyuiop'
    params = hostapd.wpa2_params(ssid=ssid, passphrase=passphrase)
    params['fragm_threshold'] = "1000"
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].connect(ssid, psk=passphrase, scan_freq="2412")
    hwsim_utils.test_connectivity(dev[0], hapd)
    dev[0].request("DISCONNECT")
    hapd.disable()
    hapd.set('fragm_threshold', '-1')
    hapd.enable()

def test_ap_vendor_elements(dev, apdev):
    """WPA2-PSK AP with vendor elements added"""
    bssid = apdev[0]['bssid']
    ssid = "test-wpa2-psk"
    passphrase = 'qwertyuiop'
    params = hostapd.wpa2_params(ssid=ssid, passphrase=passphrase)
    params['vendor_elements'] = "dd0411223301"
    params['assocresp_elements'] = "dd0411223302"
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].connect(ssid, psk=passphrase, scan_freq="2412")
    bss = dev[0].get_bss(bssid)
    if "dd0411223301" not in bss['ie']:
        raise Exception("Vendor element not shown in scan results")

    hapd.set('vendor_elements', 'dd051122330203dd0400137400dd04001374ff')
    if "OK" not in hapd.request("UPDATE_BEACON"):
        raise Exception("UPDATE_BEACON failed")
    dev[1].scan_for_bss(apdev[0]['bssid'], freq="2412")
    bss = dev[1].get_bss(bssid)
    if "dd0411223301" in bss['ie']:
        raise Exception("Old vendor element still in scan results")
    if "dd051122330203" not in bss['ie']:
        raise Exception("New vendor element not shown in scan results")

def test_ap_element_parse(dev, apdev):
    """Information element parsing - extra coverage"""
    bssid = apdev[0]['bssid']
    ssid = "test-wpa2-psk"
    params = {'ssid': ssid,
              'vendor_elements': "380501020304059e009e009e009e009e009e00"}
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    bss = dev[0].get_bss(bssid)
    if "38050102030405" not in bss['ie']:
        raise Exception("Timeout element not shown in scan results")

@remote_compatible
def test_ap_element_parse_oom(dev, apdev):
    """Information element parsing OOM"""
    bssid = apdev[0]['bssid']
    ssid = "test-wpa2-psk"
    params = {'ssid': ssid,
              'vendor_elements': "dd0d506f9a0a00000600411c440028"}
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    with alloc_fail(dev[0], 1, "wpabuf_alloc;ieee802_11_vendor_ie_concat"):
        bss = dev[0].get_bss(bssid)
        logger.info(str(bss))

def test_ap_country(dev, apdev):
    """WPA2-PSK AP setting country code and using 5 GHz band"""
    try:
        hapd = None
        bssid = apdev[0]['bssid']
        ssid = "test-wpa2-psk"
        passphrase = 'qwertyuiop'
        params = hostapd.wpa2_params(ssid=ssid, passphrase=passphrase)
        params['country_code'] = 'FI'
        params['ieee80211d'] = '1'
        params['hw_mode'] = 'a'
        params['channel'] = '36'
        hapd = hostapd.add_ap(apdev[0], params)
        dev[0].connect(ssid, psk=passphrase, scan_freq="5180")
        hwsim_utils.test_connectivity(dev[0], hapd)
    finally:
        if hapd:
            hapd.request("DISABLE")
        dev[0].disconnect_and_stop_scan()
        hostapd.cmd_execute(apdev[0], ['iw', 'reg', 'set', '00'])
        dev[0].wait_event(["CTRL-EVENT-REGDOM-CHANGE"], timeout=0.5)
        dev[0].flush_scan_cache()

def test_ap_acl_accept(dev, apdev):
    """MAC ACL accept list"""
    ssid = "acl"
    params = {}
    filename = hostapd.acl_file(dev, apdev, 'hostapd.macaddr')
    hostapd.send_file(apdev[0], filename, filename)
    params['ssid'] = ssid
    params['accept_mac_file'] = filename
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[0].connect(ssid, key_mgmt="NONE", scan_freq="2412")
    dev[1].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[1].connect(ssid, key_mgmt="NONE", scan_freq="2412")
    dev[0].request("REMOVE_NETWORK all")
    dev[1].request("REMOVE_NETWORK all")
    hapd.request("SET macaddr_acl 1")
    dev[1].dump_monitor()
    dev[1].connect(ssid, key_mgmt="NONE", scan_freq="2412", wait_connect=False)
    dev[0].connect(ssid, key_mgmt="NONE", scan_freq="2412")
    ev = dev[1].wait_event(["CTRL-EVENT-CONNECTED"], timeout=1)
    if ev is not None:
        raise Exception("Unexpected association")
    if filename.startswith('/tmp/'):
        os.unlink(filename)

def test_ap_acl_deny(dev, apdev):
    """MAC ACL deny list"""
    ssid = "acl"
    params = {}
    filename = hostapd.acl_file(dev, apdev, 'hostapd.macaddr')
    hostapd.send_file(apdev[0], filename, filename)
    params['ssid'] = ssid
    params['deny_mac_file'] = filename
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412", passive=True)
    dev[0].connect(ssid, key_mgmt="NONE", scan_freq="2412", wait_connect=False)
    dev[1].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[1].connect(ssid, key_mgmt="NONE", scan_freq="2412")
    ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED"], timeout=1)
    if ev is not None:
        raise Exception("Unexpected association")
    if filename.startswith('/tmp/'):
        os.unlink(filename)

def test_ap_acl_mgmt(dev, apdev):
    """MAC ACL accept/deny management"""
    ssid = "acl"
    params = {}
    filename = hostapd.acl_file(dev, apdev, 'hostapd.macaddr')
    hostapd.send_file(apdev[0], filename, filename)
    params['ssid'] = ssid
    params['deny_mac_file'] = filename
    hapd = hostapd.add_ap(apdev[0], params)

    accept = hapd.request("ACCEPT_ACL SHOW").splitlines()
    logger.info("accept: " + str(accept))
    deny = hapd.request("DENY_ACL SHOW").splitlines()
    logger.info("deny: " + str(deny))
    if len(accept) != 0:
        raise Exception("Unexpected number of accept entries")
    if len(deny) != 3:
        raise Exception("Unexpected number of deny entries")
    if "01:01:01:01:01:01 VLAN_ID=0" not in deny:
        raise Exception("Missing deny entry")

    if "OK" not in hapd.request("ACCEPT_ACL DEL_MAC 22:33:44:55:66:77"):
        raise Exception("DEL_MAC with empty list failed")
    if "FAIL" not in hapd.request("ACCEPT_ACL ADD_MAC 22:33:44:55:66"):
        raise Exception("ADD_MAC with invalid MAC address accepted")
    hapd.request("ACCEPT_ACL ADD_MAC 22:33:44:55:66:77")
    if "FAIL" not in hapd.request("ACCEPT_ACL DEL_MAC 22:33:44:55:66"):
        raise Exception("DEL_MAC with invalid MAC address accepted")
    hapd.request("DENY_ACL ADD_MAC 22:33:44:55:66:88 VLAN_ID=2")

    accept = hapd.request("ACCEPT_ACL SHOW").splitlines()
    logger.info("accept: " + str(accept))
    deny = hapd.request("DENY_ACL SHOW").splitlines()
    logger.info("deny: " + str(deny))
    if len(accept) != 1:
        raise Exception("Unexpected number of accept entries (2)")
    if len(deny) != 4:
        raise Exception("Unexpected number of deny entries (2)")
    if "01:01:01:01:01:01 VLAN_ID=0" not in deny:
        raise Exception("Missing deny entry (2)")
    if "22:33:44:55:66:88 VLAN_ID=2" not in deny:
        raise Exception("Missing deny entry (2)")
    if "22:33:44:55:66:77 VLAN_ID=0" not in accept:
        raise Exception("Missing accept entry (2)")

    hapd.request("ACCEPT_ACL DEL_MAC 22:33:44:55:66:77")
    hapd.request("DENY_ACL DEL_MAC 22:33:44:55:66:88")

    accept = hapd.request("ACCEPT_ACL SHOW").splitlines()
    logger.info("accept: " + str(accept))
    deny = hapd.request("DENY_ACL SHOW").splitlines()
    logger.info("deny: " + str(deny))
    if len(accept) != 0:
        raise Exception("Unexpected number of accept entries (3)")
    if len(deny) != 3:
        raise Exception("Unexpected number of deny entries (3)")
    if "01:01:01:01:01:01 VLAN_ID=0" not in deny:
        raise Exception("Missing deny entry (3)")

    hapd.request("ACCEPT_ACL CLEAR")
    hapd.request("DENY_ACL CLEAR")

    accept = hapd.request("ACCEPT_ACL SHOW").splitlines()
    logger.info("accept: " + str(accept))
    deny = hapd.request("DENY_ACL SHOW").splitlines()
    logger.info("deny: " + str(deny))
    if len(accept) != 0:
        raise Exception("Unexpected number of accept entries (4)")
    if len(deny) != 0:
        raise Exception("Unexpected number of deny entries (4)")

    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[0].connect(ssid, key_mgmt="NONE", scan_freq="2412")
    dev[0].dump_monitor()
    hapd.request("DENY_ACL ADD_MAC " + dev[0].own_addr())
    dev[0].wait_disconnected()
    dev[0].request("DISCONNECT")
    if filename.startswith('/tmp/'):
        os.unlink(filename)

def test_ap_acl_accept_changes(dev, apdev):
    """MAC ACL accept list changes"""
    ssid = "acl"
    params = {}
    params['ssid'] = ssid
    params['macaddr_acl'] = "1"
    hapd = hostapd.add_ap(apdev[0], params)
    hapd.request("ACCEPT_ACL ADD_MAC " + dev[0].own_addr())
    hapd.request("ACCEPT_ACL ADD_MAC " + dev[1].own_addr())
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[0].connect(ssid, key_mgmt="NONE", scan_freq="2412")
    dev[1].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[1].connect(ssid, key_mgmt="NONE", scan_freq="2412")
    hapd.request("ACCEPT_ACL DEL_MAC " + dev[0].own_addr())
    dev[0].wait_disconnected()
    dev[0].request("DISCONNECT")
    ev = dev[1].wait_event(["CTRL-EVENT-DISCONNECTED"], timeout=0.1)
    if ev is not None:
        raise Exception("Unexpected disconnection")
    hapd.request("ACCEPT_ACL CLEAR")
    dev[1].wait_disconnected()
    dev[1].request("DISCONNECT")

@remote_compatible
def test_ap_wds_sta(dev, apdev):
    """WPA2-PSK AP with STA using 4addr mode"""
    ssid = "test-wpa2-psk"
    passphrase = 'qwertyuiop'
    params = hostapd.wpa2_params(ssid=ssid, passphrase=passphrase)
    params['wds_sta'] = "1"
    params['wds_bridge'] = "wds-br0"
    hapd = hostapd.add_ap(apdev[0], params)

    try:
        dev[0].cmd_execute(['brctl', 'addbr', 'wds-br0'])
        dev[0].cmd_execute(['brctl', 'setfd', 'wds-br0', '0'])
        dev[0].cmd_execute(['ip', 'link', 'set', 'dev', 'wds-br0', 'up'])
        dev[0].cmd_execute(['iw', dev[0].ifname, 'set', '4addr', 'on'])
        dev[0].connect(ssid, psk=passphrase, scan_freq="2412")
        ev = hapd.wait_event(["WDS-STA-INTERFACE-ADDED"], timeout=10)
        if ev is None:
            raise Exception("No WDS-STA-INTERFACE-ADDED event seen")
        if "sta_addr=" + dev[0].own_addr() not in ev:
            raise Exception("No sta_addr match in " + ev)
        if "ifname=" + hapd.ifname + ".sta" not in ev:
            raise Exception("No ifname match in " + ev)
        sta = hapd.get_sta(dev[0].own_addr())
        if "wds_sta_ifname" not in sta:
            raise Exception("Missing wds_sta_ifname in STA data")
        if "ifname=" + sta['wds_sta_ifname'] not in ev:
            raise Exception("wds_sta_ifname %s not in event: %s" %
                            (sta['wds_sta_ifname'], ev))
        hwsim_utils.test_connectivity_iface(dev[0], hapd, "wds-br0",
                                            max_tries=15)
        dev[0].request("REATTACH")
        dev[0].wait_connected()
        hwsim_utils.test_connectivity_iface(dev[0], hapd, "wds-br0",
                                            max_tries=15)
        dev[0].request("SET reassoc_same_bss_optim 1")
        dev[0].request("REATTACH")
        dev[0].wait_connected()
        hwsim_utils.test_connectivity_iface(dev[0], hapd, "wds-br0",
                                            max_tries=5, timeout=1)
    finally:
        dev[0].request("SET reassoc_same_bss_optim 0")
        dev[0].cmd_execute(['iw', dev[0].ifname, 'set', '4addr', 'off'])
        dev[0].cmd_execute(['ip', 'link', 'set', 'dev', 'wds-br0', 'down'])
        dev[0].cmd_execute(['brctl', 'delbr', 'wds-br0'])

def test_ap_wds_sta_eap(dev, apdev):
    """WPA2-EAP AP with STA using 4addr mode"""
    ssid = "test-wpa2-eap"
    params = hostapd.wpa2_eap_params(ssid=ssid)
    params['wds_sta'] = "1"
    params['wds_bridge'] = "wds-br0"
    hapd = hostapd.add_ap(apdev[0], params)

    try:
        dev[0].cmd_execute(['brctl', 'addbr', 'wds-br0'])
        dev[0].cmd_execute(['brctl', 'setfd', 'wds-br0', '0'])
        dev[0].cmd_execute(['ip', 'link', 'set', 'dev', 'wds-br0', 'up'])
        dev[0].cmd_execute(['iw', dev[0].ifname, 'set', '4addr', 'on'])
        dev[0].connect(ssid, key_mgmt="WPA-EAP", eap="GPSK",
                       identity="gpsk user",
                       password="abcdefghijklmnop0123456789abcdef",
                       scan_freq="2412")
        ev = hapd.wait_event(["WDS-STA-INTERFACE-ADDED"], timeout=10)
        if ev is None:
            raise Exception("No WDS-STA-INTERFACE-ADDED event seen")
        if "sta_addr=" + dev[0].own_addr() not in ev:
            raise Exception("No sta_addr match in " + ev)
        if "ifname=" + hapd.ifname + ".sta" not in ev:
            raise Exception("No ifname match in " + ev)
        sta = hapd.get_sta(dev[0].own_addr())
        if "wds_sta_ifname" not in sta:
            raise Exception("Missing wds_sta_ifname in STA data")
        if "ifname=" + sta['wds_sta_ifname'] not in ev:
            raise Exception("wds_sta_ifname %s not in event: %s" %
                            (sta['wds_sta_ifname'], ev))
        hwsim_utils.test_connectivity_iface(dev[0], hapd, "wds-br0",
                                            max_tries=15)
    finally:
        dev[0].cmd_execute(['iw', dev[0].ifname, 'set', '4addr', 'off'])
        dev[0].cmd_execute(['ip', 'link', 'set', 'dev', 'wds-br0', 'down'])
        dev[0].cmd_execute(['brctl', 'delbr', 'wds-br0'])

def test_ap_wds_sta_open(dev, apdev):
    """Open AP with STA using 4addr mode"""
    ssid = "test-wds-open"
    params = {}
    params['ssid'] = ssid
    params['wds_sta'] = "1"
    params['wds_bridge'] = "wds-br0"
    hapd = hostapd.add_ap(apdev[0], params)

    try:
        dev[0].cmd_execute(['brctl', 'addbr', 'wds-br0'])
        dev[0].cmd_execute(['brctl', 'setfd', 'wds-br0', '0'])
        dev[0].cmd_execute(['ip', 'link', 'set', 'dev', 'wds-br0', 'up'])
        dev[0].cmd_execute(['iw', dev[0].ifname, 'set', '4addr', 'on'])
        dev[0].connect(ssid, key_mgmt="NONE", scan_freq="2412")
        hwsim_utils.test_connectivity_iface(dev[0], hapd, "wds-br0",
                                            max_tries=15)
        dev[0].request("REATTACH")
        dev[0].wait_connected()
        hwsim_utils.test_connectivity_iface(dev[0], hapd, "wds-br0",
                                            max_tries=15)
        dev[0].request("SET reassoc_same_bss_optim 1")
        dev[0].request("REATTACH")
        dev[0].wait_connected()
        hwsim_utils.test_connectivity_iface(dev[0], hapd, "wds-br0",
                                            max_tries=5, timeout=1)
    finally:
        dev[0].request("SET reassoc_same_bss_optim 0")
        dev[0].cmd_execute(['iw', dev[0].ifname, 'set', '4addr', 'off'])
        dev[0].cmd_execute(['ip', 'link', 'set', 'dev', 'wds-br0', 'down'])
        dev[0].cmd_execute(['brctl', 'delbr', 'wds-br0'])

def test_ap_wds_sta_wep(dev, apdev):
    """WEP AP with STA using 4addr mode"""
    check_wep_capa(dev[0])
    ssid = "test-wds-wep"
    params = {}
    params['ssid'] = ssid
    params["ieee80211n"] = "0"
    params['wep_key0'] = '"hello"'
    params['wds_sta'] = "1"
    params['wds_bridge'] = "wds-br0"
    hapd = hostapd.add_ap(apdev[0], params)

    try:
        dev[0].cmd_execute(['brctl', 'addbr', 'wds-br0'])
        dev[0].cmd_execute(['brctl', 'setfd', 'wds-br0', '0'])
        dev[0].cmd_execute(['ip', 'link', 'set', 'dev', 'wds-br0', 'up'])
        dev[0].cmd_execute(['iw', dev[0].ifname, 'set', '4addr', 'on'])
        dev[0].connect(ssid, key_mgmt="NONE", wep_key0='"hello"',
                       scan_freq="2412")
        hwsim_utils.test_connectivity_iface(dev[0], hapd, "wds-br0",
                                            max_tries=15)
        dev[0].request("REATTACH")
        dev[0].wait_connected()
        hwsim_utils.test_connectivity_iface(dev[0], hapd, "wds-br0",
                                            max_tries=15)
        dev[0].request("SET reassoc_same_bss_optim 1")
        dev[0].request("REATTACH")
        dev[0].wait_connected()
        hwsim_utils.test_connectivity_iface(dev[0], hapd, "wds-br0",
                                            max_tries=5, timeout=1)
    finally:
        dev[0].request("SET reassoc_same_bss_optim 0")
        dev[0].cmd_execute(['iw', dev[0].ifname, 'set', '4addr', 'off'])
        dev[0].cmd_execute(['ip', 'link', 'set', 'dev', 'wds-br0', 'down'])
        dev[0].cmd_execute(['brctl', 'delbr', 'wds-br0'])

@remote_compatible
def test_ap_inactivity_poll(dev, apdev):
    """AP using inactivity poll"""
    ssid = "test-wpa2-psk"
    passphrase = 'qwertyuiop'
    params = hostapd.wpa2_params(ssid=ssid, passphrase=passphrase)
    params['ap_max_inactivity'] = "1"
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].connect(ssid, psk=passphrase, scan_freq="2412")
    hapd.set("ext_mgmt_frame_handling", "1")
    dev[0].request("DISCONNECT")
    ev = hapd.wait_event(["MGMT-RX"], timeout=5)
    if ev is None:
        raise Exception("MGMT RX wait timed out for Deauth")
    hapd.set("ext_mgmt_frame_handling", "0")
    ev = hapd.wait_event(["AP-STA-DISCONNECTED"], timeout=30)
    if ev is None:
        raise Exception("STA disconnection on inactivity was not reported")

@remote_compatible
def test_ap_inactivity_disconnect(dev, apdev):
    """AP using inactivity disconnect"""
    ssid = "test-wpa2-psk"
    passphrase = 'qwertyuiop'
    params = hostapd.wpa2_params(ssid=ssid, passphrase=passphrase)
    params['ap_max_inactivity'] = "1"
    params['skip_inactivity_poll'] = "1"
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].connect(ssid, psk=passphrase, scan_freq="2412")
    hapd.set("ext_mgmt_frame_handling", "1")
    dev[0].request("DISCONNECT")
    ev = hapd.wait_event(["MGMT-RX"], timeout=5)
    if ev is None:
        raise Exception("MGMT RX wait timed out for Deauth")
    hapd.set("ext_mgmt_frame_handling", "0")
    ev = hapd.wait_event(["AP-STA-DISCONNECTED"], timeout=30)
    if ev is None:
        raise Exception("STA disconnection on inactivity was not reported")

@remote_compatible
def test_ap_basic_rates(dev, apdev):
    """Open AP with lots of basic rates"""
    ssid = "basic rates"
    params = {}
    params['ssid'] = ssid
    params['basic_rates'] = "10 20 55 110 60 90 120 180 240 360 480 540"
    hostapd.add_ap(apdev[0], params)
    dev[0].connect(ssid, key_mgmt="NONE", scan_freq="2412")

@remote_compatible
def test_ap_short_preamble(dev, apdev):
    """Open AP with short preamble"""
    ssid = "short preamble"
    params = {}
    params['ssid'] = ssid
    params['preamble'] = "1"
    hostapd.add_ap(apdev[0], params)
    dev[0].connect(ssid, key_mgmt="NONE", scan_freq="2412")

def test_ap_spectrum_management_required(dev, apdev):
    """Open AP with spectrum management required"""
    ssid = "spectrum mgmt"
    params = {}
    params['ssid'] = ssid
    params["country_code"] = "JP"
    params["hw_mode"] = "a"
    params["channel"] = "36"
    params["ieee80211d"] = "1"
    params["local_pwr_constraint"] = "3"
    params['spectrum_mgmt_required'] = "1"
    try:
        hapd = None
        hapd = hostapd.add_ap(apdev[0], params)
        dev[0].connect(ssid, key_mgmt="NONE", scan_freq="5180")
        dev[0].wait_regdom(country_ie=True)
    finally:
        if hapd:
            hapd.request("DISABLE")
        dev[0].disconnect_and_stop_scan()
        hostapd.cmd_execute(apdev[0], ['iw', 'reg', 'set', '00'])
        dev[0].wait_event(["CTRL-EVENT-REGDOM-CHANGE"], timeout=0.5)
        dev[0].flush_scan_cache()

@remote_compatible
def test_ap_max_listen_interval(dev, apdev):
    """Open AP with maximum listen interval limit"""
    ssid = "listen"
    params = {}
    params['ssid'] = ssid
    params['max_listen_interval'] = "1"
    hostapd.add_ap(apdev[0], params)
    dev[0].connect(ssid, key_mgmt="NONE", scan_freq="2412", wait_connect=False)
    ev = dev[0].wait_event(["CTRL-EVENT-ASSOC-REJECT"])
    if ev is None:
        raise Exception("Association rejection not reported")
    if "status_code=51" not in ev:
        raise Exception("Unexpected ASSOC-REJECT reason")

@remote_compatible
def test_ap_max_num_sta(dev, apdev):
    """Open AP with maximum STA count"""
    ssid = "max"
    params = {}
    params['ssid'] = ssid
    params['max_num_sta'] = "1"
    hostapd.add_ap(apdev[0], params)
    dev[1].connect(ssid, key_mgmt="NONE", scan_freq="2412")
    dev[0].connect(ssid, key_mgmt="NONE", scan_freq="2412", wait_connect=False)
    ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED"], timeout=1)
    if ev is not None:
        raise Exception("Unexpected association")

def test_ap_max_num_sta_no_probe_resp(dev, apdev, params):
    """Maximum STA count and limit on Probe Response frames"""
    logdir = params['logdir']
    dev[0].flush_scan_cache()
    ssid = "max"
    params = {}
    params['ssid'] = ssid
    params['beacon_int'] = "2000"
    params['max_num_sta'] = "1"
    params['no_probe_resp_if_max_sta'] = "1"
    hostapd.add_ap(apdev[0], params)
    dev[1].connect(ssid, key_mgmt="NONE", scan_freq="2412")
    dev[0].scan(freq=2412, type="ONLY")
    dev[0].scan(freq=2412, type="ONLY")
    seen = dev[0].get_bss(apdev[0]['bssid']) != None
    dev[1].scan(freq=2412, type="ONLY")
    if seen:
        out = run_tshark(os.path.join(logdir, "hwsim0.pcapng"),
                         "wlan.fc.type_subtype == 5", ["wlan.da"])
        if out:
            if dev[0].own_addr() not in out:
                # Discovery happened through Beacon frame reception. That's not
                # an error case.
                seen = False
            if dev[1].own_addr() not in out:
                raise Exception("No Probe Response frames to dev[1] seen")
        if seen:
            raise Exception("AP found unexpectedly")

@remote_compatible
def test_ap_tx_queue_params(dev, apdev):
    """Open AP with TX queue params set"""
    ssid = "tx"
    params = {}
    params['ssid'] = ssid
    params['tx_queue_data2_aifs'] = "4"
    params['tx_queue_data2_cwmin'] = "7"
    params['tx_queue_data2_cwmax'] = "1023"
    params['tx_queue_data2_burst'] = "4.2"
    params['tx_queue_data1_aifs'] = "4"
    params['tx_queue_data1_cwmin'] = "7"
    params['tx_queue_data1_cwmax'] = "1023"
    params['tx_queue_data1_burst'] = "2"
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].connect(ssid, key_mgmt="NONE", scan_freq="2412")
    hwsim_utils.test_connectivity(dev[0], hapd)

def test_ap_tx_queue_params_invalid(dev, apdev):
    """Invalid TX queue params set (cwmin/cwmax)"""
    ssid = "tx"
    params = {}
    params['ssid'] = ssid
    params['tx_queue_data2_aifs'] = "4"
    params['tx_queue_data2_cwmin'] = "7"
    params['tx_queue_data2_cwmax'] = "1023"
    params['tx_queue_data2_burst'] = "4.2"
    params['wmm_ac_bk_cwmin'] = "4"
    params['wmm_ac_bk_cwmax'] = "10"
    params['wmm_ac_bk_aifs'] = "7"
    params['wmm_ac_bk_txop_limit'] = "0"
    params['wmm_ac_bk_acm'] = "0"

    hapd = hostapd.add_ap(apdev[0], params)

    # Valid WMM change
    hapd.set("wmm_ac_be_cwmin", "3")

    # "Invalid TX queue cwMin/cwMax values. cwMin(7) greater than cwMax(3)"
    if "FAIL" not in hapd.request('SET tx_queue_data2_cwmax 3'):
        raise Exception("TX cwMax < cwMin accepted")
    # "Invalid WMM AC cwMin/cwMax values. cwMin(4) greater than cwMax(3)"
    if "FAIL" not in hapd.request('SET wmm_ac_bk_cwmax 3'):
        raise Exception("AC cwMax < cwMin accepted")

    hapd.request("SET tx_queue_data2_cwmax 1023")
    hapd.set("wmm_ac_bk_cwmax", "10")
    # Invalid IEs to cause WMM parameter update failing
    hapd.set("vendor_elements", "dd04112233")
    hapd.set("wmm_ac_be_cwmin", "3")
    # Valid IEs to cause WMM parameter update succeeding
    hapd.set("vendor_elements", "dd0411223344")
    hapd.set("wmm_ac_be_cwmin", "3")

    dev[0].connect(ssid, key_mgmt="NONE", scan_freq="2412")

def test_ap_beacon_rate_legacy(dev, apdev):
    """Open AP with Beacon frame TX rate 5.5 Mbps"""
    hapd = hostapd.add_ap(apdev[0], {'ssid': 'beacon-rate'})
    res = hapd.get_driver_status_field('capa.flags')
    if (int(res, 0) & 0x0000080000000000) == 0:
        raise HwsimSkip("Setting Beacon frame TX rate not supported")
    hapd.disable()
    hapd.set('beacon_rate', '55')
    hapd.enable()
    dev[0].connect('beacon-rate', key_mgmt="NONE", scan_freq="2412")
    time.sleep(0.5)

def test_ap_beacon_rate_legacy2(dev, apdev):
    """Open AP with Beacon frame TX rate 12 Mbps in VHT BSS"""
    hapd = hostapd.add_ap(apdev[0], {'ssid': 'beacon-rate'})
    res = hapd.get_driver_status_field('capa.flags')
    if (int(res, 0) & 0x0000080000000000) == 0:
        raise HwsimSkip("Setting Beacon frame TX rate not supported")
    hapd.disable()
    hapd.set('beacon_rate', '120')
    hapd.set("country_code", "DE")
    hapd.set("hw_mode", "a")
    hapd.set("channel", "36")
    hapd.set("ieee80211n", "1")
    hapd.set("ieee80211ac", "1")
    hapd.set("ht_capab", "[HT40+]")
    hapd.set("vht_capab", "")
    hapd.set("vht_oper_chwidth", "0")
    hapd.set("vht_oper_centr_freq_seg0_idx", "0")
    try:
        hapd.enable()
        dev[0].scan_for_bss(hapd.own_addr(), freq="5180")
        dev[0].connect('beacon-rate', key_mgmt="NONE", scan_freq="5180")
        time.sleep(0.5)
    finally:
        dev[0].request("DISCONNECT")
        hapd.request("DISABLE")
        subprocess.call(['iw', 'reg', 'set', '00'])
        dev[0].flush_scan_cache()

def test_ap_beacon_rate_ht(dev, apdev):
    """Open AP with Beacon frame TX rate HT-MCS 0"""
    hapd = hostapd.add_ap(apdev[0], {'ssid': 'beacon-rate'})
    res = hapd.get_driver_status_field('capa.flags')
    if (int(res, 0) & 0x0000100000000000) == 0:
        raise HwsimSkip("Setting Beacon frame TX rate not supported")
    hapd.disable()
    hapd.set('beacon_rate', 'ht:0')
    hapd.enable()
    dev[0].connect('beacon-rate', key_mgmt="NONE", scan_freq="2412")
    time.sleep(0.5)

def test_ap_beacon_rate_ht2(dev, apdev):
    """Open AP with Beacon frame TX rate HT-MCS 1 in VHT BSS"""
    hapd = hostapd.add_ap(apdev[0], {'ssid': 'beacon-rate'})
    res = hapd.get_driver_status_field('capa.flags')
    if (int(res, 0) & 0x0000100000000000) == 0:
        raise HwsimSkip("Setting Beacon frame TX rate not supported")
    hapd.disable()
    hapd.set('beacon_rate', 'ht:1')
    hapd.set("country_code", "DE")
    hapd.set("hw_mode", "a")
    hapd.set("channel", "36")
    hapd.set("ieee80211n", "1")
    hapd.set("ieee80211ac", "1")
    hapd.set("ht_capab", "[HT40+]")
    hapd.set("vht_capab", "")
    hapd.set("vht_oper_chwidth", "0")
    hapd.set("vht_oper_centr_freq_seg0_idx", "0")
    try:
        hapd.enable()
        dev[0].scan_for_bss(hapd.own_addr(), freq="5180")
        dev[0].connect('beacon-rate', key_mgmt="NONE", scan_freq="5180")
        time.sleep(0.5)
    finally:
        dev[0].request("DISCONNECT")
        hapd.request("DISABLE")
        subprocess.call(['iw', 'reg', 'set', '00'])
        dev[0].flush_scan_cache()

def test_ap_beacon_rate_vht(dev, apdev):
    """Open AP with Beacon frame TX rate VHT-MCS 0"""
    hapd = hostapd.add_ap(apdev[0], {'ssid': 'beacon-rate'})
    res = hapd.get_driver_status_field('capa.flags')
    if (int(res, 0) & 0x0000200000000000) == 0:
        raise HwsimSkip("Setting Beacon frame TX rate not supported")
    hapd.disable()
    hapd.set('beacon_rate', 'vht:0')
    hapd.set("country_code", "DE")
    hapd.set("hw_mode", "a")
    hapd.set("channel", "36")
    hapd.set("ieee80211n", "1")
    hapd.set("ieee80211ac", "1")
    hapd.set("ht_capab", "[HT40+]")
    hapd.set("vht_capab", "")
    hapd.set("vht_oper_chwidth", "0")
    hapd.set("vht_oper_centr_freq_seg0_idx", "0")
    try:
        hapd.enable()
        dev[0].scan_for_bss(hapd.own_addr(), freq="5180")
        dev[0].connect('beacon-rate', key_mgmt="NONE", scan_freq="5180")
        time.sleep(0.5)
    finally:
        dev[0].request("DISCONNECT")
        hapd.request("DISABLE")
        subprocess.call(['iw', 'reg', 'set', '00'])
        dev[0].flush_scan_cache()

def test_ap_wep_to_wpa(dev, apdev):
    """WEP to WPA2-PSK configuration change in hostapd"""
    check_wep_capa(dev[0])
    hapd = hostapd.add_ap(apdev[0],
                          {"ssid": "wep-to-wpa",
                           "wep_key0": '"hello"'})
    dev[0].flush_scan_cache()
    dev[0].connect("wep-to-wpa", key_mgmt="NONE", wep_key0='"hello"',
                   scan_freq="2412")
    hwsim_utils.test_connectivity(dev[0], hapd)
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    hapd.disable()
    hapd.set("wep_key0", "")
    hapd.set("wpa_passphrase", "12345678")
    hapd.set("wpa", "2")
    hapd.set("wpa_key_mgmt", "WPA-PSK")
    hapd.set("rsn_pairwise", "CCMP")
    hapd.enable()

    dev[0].connect("wep-to-wpa", psk="12345678", scan_freq="2412")
    hwsim_utils.test_connectivity(dev[0], hapd)

def test_ap_missing_psk(dev, apdev):
    """WPA2-PSK AP and no PSK configured"""
    ssid = "test-wpa2-psk"
    params = hostapd.wpa2_params(ssid=ssid)
    try:
        # "WPA-PSK enabled, but PSK or passphrase is not configured."
        hostapd.add_ap(apdev[0], params)
        raise Exception("AP setup succeeded unexpectedly")
    except Exception as e:
        if "Failed to enable hostapd" in str(e):
            pass
        else:
            raise

def test_ap_eapol_version(dev, apdev):
    """hostapd eapol_version configuration"""
    passphrase = "asdfghjkl"
    params = hostapd.wpa2_params(ssid="test1", passphrase=passphrase)
    hapd = hostapd.add_ap(apdev[0], params)
    params = hostapd.wpa2_params(ssid="test2", passphrase=passphrase)
    params['eapol_version'] = '1'
    hapd2 = hostapd.add_ap(apdev[1], params)

    hapd.request("SET ext_eapol_frame_io 1")
    dev[0].connect("test1", psk=passphrase, scan_freq="2412",
                   wait_connect=False)
    ev1 = hapd.wait_event(["EAPOL-TX"], timeout=15)
    if ev1 is None:
        raise Exception("Timeout on EAPOL-TX from hostapd")
    hapd.request("SET ext_eapol_frame_io 0")

    hapd2.request("SET ext_eapol_frame_io 1")
    dev[1].connect("test2", psk=passphrase, scan_freq="2412",
                   wait_connect=False)
    ev2 = hapd2.wait_event(["EAPOL-TX"], timeout=15)
    if ev2 is None:
        raise Exception("Timeout on EAPOL-TX from hostapd")
    hapd2.request("SET ext_eapol_frame_io 0")

    dev[0].wait_connected()
    dev[1].wait_connected()

    ver1 = ev1.split(' ')[2][0:2]
    ver2 = ev2.split(' ')[2][0:2]
    if ver1 != "02":
        raise Exception("Unexpected default eapol_version: " + ver1)
    if ver2 != "01":
        raise Exception("eapol_version did not match configuration: " + ver2)

def test_ap_dtim_period(dev, apdev):
    """DTIM period configuration"""
    ssid = "dtim-period"
    params = {'ssid': ssid, 'dtim_period': "10"}
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = hapd.own_addr()
    dev[0].connect(ssid, key_mgmt="NONE", scan_freq="2412")
    for i in range(10):
        dev[0].scan(freq="2412")
        bss = dev[0].get_bss(bssid)
        if 'beacon_ie' in bss:
            break
        time.sleep(0.2)
    if 'beacon_ie' not in bss:
        raise Exception("Did not find Beacon IEs")

    ie = parse_ie(bss['beacon_ie'])
    if 5 not in ie:
        raise Exception("TIM element missing")
    count, period = struct.unpack('BB', ie[5][0:2])
    logger.info("DTIM count %d  DTIM period %d" % (count, period))
    if period != 10:
        raise Exception("Unexpected DTIM period: %d" % period)
    if count >= period:
        raise Exception("Unexpected DTIM count: %d" % count)

def test_ap_no_probe_resp(dev, apdev):
    """AP with Probe Response frame sending from hostapd disabled"""
    ssid = "no-probe-resp"
    params = {'ssid': ssid, 'send_probe_response': "0"}
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = hapd.own_addr()
    dev[0].scan_for_bss(bssid, freq="2412", passive=True)
    dev[0].scan_for_bss(bssid, freq="2412", force_scan=True)
    dev[0].connect(ssid, key_mgmt="NONE", scan_freq="2412")
    bss = dev[0].get_bss(bssid)
    if 'ie' in bss and 'beacon_ie' in bss and \
       len(bss['ie']) != len(bss['beacon_ie']):
        raise Exception("Probe Response frames seen")

def test_ap_long_preamble(dev, apdev):
    """AP with long preamble"""
    ssid = "long-preamble"
    params = {'ssid': ssid, 'preamble': "0",
              'hw_mode': 'b', 'ieee80211n': '0',
              'supported_rates': '10', 'basic_rates': '10'}
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = hapd.own_addr()
    dev[0].scan_for_bss(bssid, freq="2412")
    dev[0].connect(ssid, key_mgmt="NONE", scan_freq="2412")
    hwsim_utils.test_connectivity(dev[0], hapd)

def test_ap_wmm_uapsd(dev, apdev):
    """AP with U-APSD advertisement"""
    ssid = "uapsd"
    params = {'ssid': ssid, 'uapsd_advertisement_enabled': "1"}
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = hapd.own_addr()
    dev[0].scan_for_bss(bssid, freq="2412")
    dev[0].connect(ssid, key_mgmt="NONE", scan_freq="2412")
    hwsim_utils.test_connectivity(dev[0], hapd)

def test_ap_wowlan_triggers(dev, apdev):
    """AP with wowlan_triggers"""
    ssid = "wowlan"
    params = {'ssid': ssid, 'wowlan_triggers': "any"}
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = hapd.own_addr()
    dev[0].scan_for_bss(bssid, freq="2412")
    dev[0].connect(ssid, key_mgmt="NONE", scan_freq="2412")
    hwsim_utils.test_connectivity(dev[0], hapd)

def test_ap_notify_mgmt_frames(dev, apdev):
    """hostapd notify_mgmt_frames configuration enabled"""
    ssid = "mgmt_frames"
    params = {'ssid': ssid, 'notify_mgmt_frames': "1"}
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = hapd.own_addr()
    dev[0].scan_for_bss(bssid, freq="2412")
    dev[0].connect(ssid, key_mgmt="NONE", scan_freq="2412")
    ev = hapd.wait_event(["AP-MGMT-FRAME-RECEIVED"], timeout=5)
    if ev is None:
        raise Exception("AP-MGMT-FRAME-RECEIVED wait timed out")
    if "buf=b0" not in ev:
        raise Exception("Expected auth request in AP-MGMT-FRAME-RECEIVED")

def test_ap_notify_mgmt_frames_disabled(dev, apdev):
    """hostapd notify_mgmt_frames configuration disabled"""
    ssid = "mgmt_frames"
    params = {'ssid': ssid, 'notify_mgmt_frames': "0"}
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = hapd.own_addr()
    dev[0].scan_for_bss(bssid, freq="2412")
    dev[0].connect(ssid, key_mgmt="NONE", scan_freq="2412")
    ev = hapd.wait_event(["AP-MGMT-FRAME-RECEIVED"], timeout=0.1)
    if ev is not None:
        raise Exception("Unexpected AP-MGMT-FRAME-RECEIVED")

def test_ap_airtime_policy_static(dev, apdev):
    """Airtime policy - static"""
    ssid = "test-wpa2-psk"
    passphrase = 'qwertyuiop'
    params = hostapd.wpa2_params(ssid=ssid, passphrase=passphrase)
    params['airtime_mode'] = "1"
    params['airtime_update_interval'] = "200"
    params['airtime_sta_weight'] = dev[0].own_addr() + " 512"
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].connect(ssid, psk=passphrase, scan_freq="2412")
    dev[1].connect(ssid, psk=passphrase, scan_freq="2412")
    time.sleep(1)

def test_ap_airtime_policy_per_bss_dynamic(dev, apdev):
    """Airtime policy - per-BSS dynamic"""
    ssid = "test-wpa2-psk"
    passphrase = 'qwertyuiop'
    params = hostapd.wpa2_params(ssid=ssid, passphrase=passphrase)
    params['airtime_mode'] = "2"
    params['airtime_update_interval'] = "200"
    params['airtime_bss_weight'] = "2"
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].connect(ssid, psk=passphrase, scan_freq="2412")
    dev[1].connect(ssid, psk=passphrase, scan_freq="2412")
    time.sleep(1)

def test_ap_airtime_policy_per_bss_limit(dev, apdev):
    """Airtime policy - per-BSS limit"""
    ssid = "test-wpa2-psk"
    passphrase = 'qwertyuiop'
    params = hostapd.wpa2_params(ssid=ssid, passphrase=passphrase)
    params['airtime_mode'] = "3"
    params['airtime_update_interval'] = "200"
    params['airtime_bss_weight'] = "2"
    params['airtime_bss_limit'] = "1"
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].connect(ssid, psk=passphrase, scan_freq="2412")
    dev[1].connect(ssid, psk=passphrase, scan_freq="2412")
    time.sleep(1)
    hapd.set("force_backlog_bytes", "1")
    time.sleep(1)

def test_ap_airtime_policy_per_bss_limit_invalid(dev, apdev):
    """Airtime policy - per-BSS limit (invalid)"""
    ssid = "test-wpa2-psk"
    passphrase = 'qwertyuiop'
    params = hostapd.wpa2_params(ssid=ssid, passphrase=passphrase)
    params['airtime_mode'] = "3"
    params['airtime_update_interval'] = "0"
    params['airtime_bss_weight'] = "2"
    params['airtime_bss_limit'] = "1"
    hapd = hostapd.add_ap(apdev[0], params, no_enable=True)
    if "FAIL" not in hapd.request("ENABLE"):
        raise Exception("Invalid airtime policy configuration accepted")
    hapd.set("airtime_update_interval", "200")
    hapd.enable()
    hapd.set("airtime_update_interval", "0")
    dev[0].connect(ssid, psk=passphrase, scan_freq="2412")
    dev[1].connect(ssid, psk=passphrase, scan_freq="2412")
    time.sleep(1)
