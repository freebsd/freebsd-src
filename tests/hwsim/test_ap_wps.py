# WPS tests
# Copyright (c) 2013-2017, Jouni Malinen <j@w1.fi>
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

from remotehost import remote_compatible
from tshark import run_tshark
import base64
import binascii
from Crypto.Cipher import AES
import hashlib
import hmac
import os
import time
import sys
import stat
import subprocess
import logging
logger = logging.getLogger()
import re
import socket
import struct
try:
    from http.client import HTTPConnection
    from urllib.request import urlopen
    from urllib.parse import urlparse, urljoin
    from urllib.error import HTTPError
    from io import StringIO
    from socketserver import StreamRequestHandler, TCPServer
except ImportError:
    from httplib import HTTPConnection
    from urllib import urlopen
    from urlparse import urlparse, urljoin
    from urllib2 import build_opener, ProxyHandler, HTTPError
    from StringIO import StringIO
    from SocketServer import StreamRequestHandler, TCPServer
import urllib
import xml.etree.ElementTree as ET

import hwsim_utils
import hostapd
from wpasupplicant import WpaSupplicant
from utils import *
from test_ap_eap import int_eap_server_params

def wps_start_ap(apdev, ssid="test-wps-conf"):
    params = {"ssid": ssid, "eap_server": "1", "wps_state": "2",
              "wpa_passphrase": "12345678", "wpa": "2",
              "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP"}
    return hostapd.add_ap(apdev, params)

@remote_compatible
def test_ap_wps_init(dev, apdev):
    """Initial AP configuration with first WPS Enrollee"""
    skip_without_tkip(dev[0])
    ssid = "test-wps"
    hapd = hostapd.add_ap(apdev[0],
                          {"ssid": ssid, "eap_server": "1", "wps_state": "1"})
    logger.info("WPS provisioning step")
    hapd.request("WPS_PBC")
    if "PBC Status: Active" not in hapd.request("WPS_GET_STATUS"):
        raise Exception("PBC status not shown correctly")

    id = dev[0].add_network()
    dev[0].set_network_quoted(id, "ssid", "home")
    dev[0].set_network_quoted(id, "psk", "12345678")
    dev[0].request("ENABLE_NETWORK %s no-connect" % id)

    id = dev[0].add_network()
    dev[0].set_network_quoted(id, "ssid", "home2")
    dev[0].set_network(id, "bssid", "00:11:22:33:44:55")
    dev[0].set_network(id, "key_mgmt", "NONE")
    dev[0].request("ENABLE_NETWORK %s no-connect" % id)

    dev[0].request("WPS_PBC")
    dev[0].wait_connected(timeout=30)
    status = dev[0].get_status()
    if status['wpa_state'] != 'COMPLETED' or status['bssid'] != apdev[0]['bssid']:
        raise Exception("Not fully connected")
    if status['ssid'] != ssid:
        raise Exception("Unexpected SSID")
    if status['pairwise_cipher'] != 'CCMP':
        raise Exception("Unexpected encryption configuration")
    if status['key_mgmt'] != 'WPA2-PSK':
        raise Exception("Unexpected key_mgmt")

    status = hapd.request("WPS_GET_STATUS")
    if "PBC Status: Disabled" not in status:
        raise Exception("PBC status not shown correctly")
    if "Last WPS result: Success" not in status:
        raise Exception("Last WPS result not shown correctly")
    if "Peer Address: " + dev[0].p2p_interface_addr() not in status:
        raise Exception("Peer address not shown correctly")
    conf = hapd.request("GET_CONFIG")
    if "wps_state=configured" not in conf:
        raise Exception("AP not in WPS configured state")
    if "wpa=2" in conf:
        if "rsn_pairwise_cipher=CCMP" not in conf:
            raise Exception("Unexpected rsn_pairwise_cipher")
        if "group_cipher=CCMP" not in conf:
            raise Exception("Unexpected group_cipher")
    else:
        if "wpa=3" not in conf:
            raise Exception("AP not in WPA+WPA2 configuration")
        if "rsn_pairwise_cipher=CCMP TKIP" not in conf:
            raise Exception("Unexpected rsn_pairwise_cipher")
        if "wpa_pairwise_cipher=CCMP TKIP" not in conf:
            raise Exception("Unexpected wpa_pairwise_cipher")
        if "group_cipher=TKIP" not in conf:
            raise Exception("Unexpected group_cipher")

    if len(dev[0].list_networks()) != 3:
        raise Exception("Unexpected number of network blocks")

def test_ap_wps_init_2ap_pbc(dev, apdev):
    """Initial two-radio AP configuration with first WPS PBC Enrollee"""
    skip_without_tkip(dev[0])
    ssid = "test-wps"
    params = {"ssid": ssid, "eap_server": "1", "wps_state": "1"}
    hapd = hostapd.add_ap(apdev[0], params)
    hostapd.add_ap(apdev[1], params)
    logger.info("WPS provisioning step")
    hapd.request("WPS_PBC")
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412", force_scan=True)
    dev[0].scan_for_bss(apdev[1]['bssid'], freq="2412")
    bss = dev[0].get_bss(apdev[0]['bssid'])
    if "[WPS-PBC]" not in bss['flags']:
        raise Exception("WPS-PBC flag missing from AP1")
    bss = dev[0].get_bss(apdev[1]['bssid'])
    if "[WPS-PBC]" not in bss['flags']:
        raise Exception("WPS-PBC flag missing from AP2")
    dev[0].dump_monitor()
    dev[0].request("SET wps_cred_processing 2")
    dev[0].request("WPS_PBC")
    ev = dev[0].wait_event(["WPS-CRED-RECEIVED"], timeout=30)
    dev[0].request("SET wps_cred_processing 0")
    if ev is None:
        raise Exception("WPS cred event not seen")
    if "100e" not in ev:
        raise Exception("WPS attributes not included in the cred event")
    dev[0].wait_connected(timeout=30)

    dev[1].scan_for_bss(apdev[0]['bssid'], freq="2412", force_scan=True)
    dev[1].scan_for_bss(apdev[1]['bssid'], freq="2412")
    bss = dev[1].get_bss(apdev[0]['bssid'])
    if "[WPS-PBC]" in bss['flags']:
        raise Exception("WPS-PBC flag not cleared from AP1")
    bss = dev[1].get_bss(apdev[1]['bssid'])
    if "[WPS-PBC]" in bss['flags']:
        raise Exception("WPS-PBC flag not cleared from AP2")

def test_ap_wps_init_2ap_pin(dev, apdev):
    """Initial two-radio AP configuration with first WPS PIN Enrollee"""
    skip_without_tkip(dev[0])
    ssid = "test-wps"
    params = {"ssid": ssid, "eap_server": "1", "wps_state": "1"}
    hapd = hostapd.add_ap(apdev[0], params)
    hostapd.add_ap(apdev[1], params)
    logger.info("WPS provisioning step")
    pin = dev[0].wps_read_pin()
    hapd.request("WPS_PIN any " + pin)
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412", force_scan=True)
    dev[0].scan_for_bss(apdev[1]['bssid'], freq="2412")
    bss = dev[0].get_bss(apdev[0]['bssid'])
    if "[WPS-AUTH]" not in bss['flags']:
        raise Exception("WPS-AUTH flag missing from AP1")
    bss = dev[0].get_bss(apdev[1]['bssid'])
    if "[WPS-AUTH]" not in bss['flags']:
        raise Exception("WPS-AUTH flag missing from AP2")
    dev[0].dump_monitor()
    dev[0].request("WPS_PIN any " + pin)
    dev[0].wait_connected(timeout=30)

    dev[1].scan_for_bss(apdev[0]['bssid'], freq="2412", force_scan=True)
    dev[1].scan_for_bss(apdev[1]['bssid'], freq="2412")
    bss = dev[1].get_bss(apdev[0]['bssid'])
    if "[WPS-AUTH]" in bss['flags']:
        raise Exception("WPS-AUTH flag not cleared from AP1")
    bss = dev[1].get_bss(apdev[1]['bssid'])
    if "[WPS-AUTH]" in bss['flags']:
        raise Exception("WPS-AUTH flag not cleared from AP2")

@remote_compatible
def test_ap_wps_init_through_wps_config(dev, apdev):
    """Initial AP configuration using wps_config command"""
    ssid = "test-wps-init-config"
    hapd = hostapd.add_ap(apdev[0],
                          {"ssid": ssid, "eap_server": "1", "wps_state": "1"})
    if "FAIL" in hapd.request("WPS_CONFIG " + binascii.hexlify(ssid.encode()).decode() + " WPA2PSK CCMP " + binascii.hexlify(b"12345678").decode()):
        raise Exception("WPS_CONFIG command failed")
    ev = hapd.wait_event(["WPS-NEW-AP-SETTINGS"], timeout=5)
    if ev is None:
        raise Exception("Timeout on WPS-NEW-AP-SETTINGS events")
    # It takes some time for the AP to update Beacon and Probe Response frames,
    # so wait here before requesting the scan to be started to avoid adding
    # extra five second wait to the test due to fetching obsolete scan results.
    hapd.ping()
    time.sleep(0.2)
    dev[0].connect(ssid, psk="12345678", scan_freq="2412", proto="WPA2",
                   pairwise="CCMP", group="CCMP")

    if "FAIL" not in hapd.request("WPS_CONFIG foo"):
        raise Exception("Invalid WPS_CONFIG accepted")

@remote_compatible
def test_ap_wps_init_through_wps_config_2(dev, apdev):
    """AP configuration using wps_config and wps_cred_processing=2"""
    ssid = "test-wps-init-config"
    hapd = hostapd.add_ap(apdev[0],
                          {"ssid": ssid, "eap_server": "1", "wps_state": "1",
                           "wps_cred_processing": "2"})
    if "FAIL" in hapd.request("WPS_CONFIG " + binascii.hexlify(ssid.encode()).decode() + " WPA2PSK CCMP " + binascii.hexlify(b"12345678").decode()):
        raise Exception("WPS_CONFIG command failed")
    ev = hapd.wait_event(["WPS-NEW-AP-SETTINGS"], timeout=5)
    if ev is None:
        raise Exception("Timeout on WPS-NEW-AP-SETTINGS events")
    if "100e" not in ev:
        raise Exception("WPS-NEW-AP-SETTINGS did not include Credential")

@remote_compatible
def test_ap_wps_invalid_wps_config_passphrase(dev, apdev):
    """AP configuration using wps_config command with invalid passphrase"""
    ssid = "test-wps-init-config"
    hapd = hostapd.add_ap(apdev[0],
                          {"ssid": ssid, "eap_server": "1", "wps_state": "1"})
    if "FAIL" not in hapd.request("WPS_CONFIG " + binascii.hexlify(ssid.encode()).decode() + " WPA2PSK CCMP " + binascii.hexlify(b"1234567").decode()):
        raise Exception("Invalid WPS_CONFIG command accepted")

def test_ap_wps_conf(dev, apdev):
    """WPS PBC provisioning with configured AP"""
    ssid = "test-wps-conf"
    hapd = hostapd.add_ap(apdev[0],
                          {"ssid": ssid, "eap_server": "1", "wps_state": "2",
                           "wpa_passphrase": "12345678", "wpa": "2",
                           "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP"})
    logger.info("WPS provisioning step")
    hapd.request("WPS_PBC")
    dev[0].set("device_name", "Device A")
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[0].dump_monitor()
    dev[0].request("WPS_PBC " + apdev[0]['bssid'])
    dev[0].wait_connected(timeout=30)
    status = dev[0].get_status()
    if status['wpa_state'] != 'COMPLETED':
        raise Exception("Not fully connected")
    if status['bssid'] != apdev[0]['bssid']:
        raise Exception("Unexpected BSSID")
    if status['ssid'] != ssid:
        raise Exception("Unexpected SSID")
    if status['pairwise_cipher'] != 'CCMP' or status['group_cipher'] != 'CCMP':
        raise Exception("Unexpected encryption configuration")
    if status['key_mgmt'] != 'WPA2-PSK':
        raise Exception("Unexpected key_mgmt")

    sta = hapd.get_sta(dev[0].p2p_interface_addr())
    if 'wpsDeviceName' not in sta or sta['wpsDeviceName'] != "Device A":
        raise Exception("Device name not available in STA command")

def test_ap_wps_conf_5ghz(dev, apdev):
    """WPS PBC provisioning with configured AP on 5 GHz band"""
    try:
        hapd = None
        ssid = "test-wps-conf"
        params = {"ssid": ssid, "eap_server": "1", "wps_state": "2",
                  "wpa_passphrase": "12345678", "wpa": "2",
                  "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP",
                  "country_code": "FI", "hw_mode": "a", "channel": "36"}
        hapd = hostapd.add_ap(apdev[0], params)
        logger.info("WPS provisioning step")
        hapd.request("WPS_PBC")
        dev[0].set("device_name", "Device A")
        dev[0].scan_for_bss(apdev[0]['bssid'], freq="5180")
        dev[0].request("WPS_PBC " + apdev[0]['bssid'])
        dev[0].wait_connected(timeout=30)

        sta = hapd.get_sta(dev[0].p2p_interface_addr())
        if 'wpsDeviceName' not in sta or sta['wpsDeviceName'] != "Device A":
            raise Exception("Device name not available in STA command")
    finally:
        dev[0].request("DISCONNECT")
        clear_regdom(hapd, dev)

def test_ap_wps_conf_chan14(dev, apdev):
    """WPS PBC provisioning with configured AP on channel 14"""
    try:
        hapd = None
        ssid = "test-wps-conf"
        params = {"ssid": ssid, "eap_server": "1", "wps_state": "2",
                  "wpa_passphrase": "12345678", "wpa": "2",
                  "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP",
                  "country_code": "JP", "hw_mode": "b", "channel": "14"}
        hapd = hostapd.add_ap(apdev[0], params)
        logger.info("WPS provisioning step")
        hapd.request("WPS_PBC")
        dev[0].set("device_name", "Device A")
        dev[0].request("WPS_PBC")
        dev[0].wait_connected(timeout=30)

        sta = hapd.get_sta(dev[0].p2p_interface_addr())
        if 'wpsDeviceName' not in sta or sta['wpsDeviceName'] != "Device A":
            raise Exception("Device name not available in STA command")
    finally:
        dev[0].request("DISCONNECT")
        clear_regdom(hapd, dev)

@remote_compatible
def test_ap_wps_twice(dev, apdev):
    """WPS provisioning with twice to change passphrase"""
    ssid = "test-wps-twice"
    params = {"ssid": ssid, "eap_server": "1", "wps_state": "2",
              "wpa_passphrase": "12345678", "wpa": "2",
              "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP"}
    hapd = hostapd.add_ap(apdev[0], params)
    logger.info("WPS provisioning step")
    hapd.request("WPS_PBC")
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[0].dump_monitor()
    dev[0].request("WPS_PBC " + apdev[0]['bssid'])
    dev[0].wait_connected(timeout=30)
    dev[0].request("DISCONNECT")

    logger.info("Restart AP with different passphrase and re-run WPS")
    hostapd.remove_bss(apdev[0])
    params['wpa_passphrase'] = 'another passphrase'
    hapd = hostapd.add_ap(apdev[0], params)
    logger.info("WPS provisioning step")
    hapd.request("WPS_PBC")
    dev[0].dump_monitor()
    dev[0].request("WPS_PBC " + apdev[0]['bssid'])
    dev[0].wait_connected(timeout=30)
    networks = dev[0].list_networks()
    if len(networks) > 1:
        raise Exception("Unexpected duplicated network block present")

@remote_compatible
def test_ap_wps_incorrect_pin(dev, apdev):
    """WPS PIN provisioning with incorrect PIN"""
    ssid = "test-wps-incorrect-pin"
    hapd = hostapd.add_ap(apdev[0],
                          {"ssid": ssid, "eap_server": "1", "wps_state": "2",
                           "wpa_passphrase": "12345678", "wpa": "2",
                           "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP"})

    logger.info("WPS provisioning attempt 1")
    hapd.request("WPS_PIN any 12345670")
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[0].dump_monitor()
    dev[0].request("WPS_PIN %s 55554444" % apdev[0]['bssid'])
    ev = dev[0].wait_event(["WPS-FAIL"], timeout=30)
    if ev is None:
        raise Exception("WPS operation timed out")
    if "config_error=18" not in ev:
        raise Exception("Incorrect config_error reported")
    if "msg=8" not in ev:
        raise Exception("PIN error detected on incorrect message")
    dev[0].wait_disconnected(timeout=10)
    dev[0].request("WPS_CANCEL")
    # if a scan was in progress, wait for it to complete before trying WPS again
    ev = dev[0].wait_event(["CTRL-EVENT-SCAN-RESULTS"], 5)

    status = hapd.request("WPS_GET_STATUS")
    if "Last WPS result: Failed" not in status:
        raise Exception("WPS failure result not shown correctly")

    logger.info("WPS provisioning attempt 2")
    hapd.request("WPS_PIN any 12345670")
    dev[0].dump_monitor()
    dev[0].request("WPS_PIN %s 12344444" % apdev[0]['bssid'])
    ev = dev[0].wait_event(["WPS-FAIL"], timeout=30)
    if ev is None:
        raise Exception("WPS operation timed out")
    if "config_error=18" not in ev:
        raise Exception("Incorrect config_error reported")
    if "msg=10" not in ev:
        raise Exception("PIN error detected on incorrect message")
    dev[0].wait_disconnected(timeout=10)

@remote_compatible
def test_ap_wps_conf_pin(dev, apdev):
    """WPS PIN provisioning with configured AP"""
    ssid = "test-wps-conf-pin"
    hapd = hostapd.add_ap(apdev[0],
                          {"ssid": ssid, "eap_server": "1", "wps_state": "2",
                           "wpa_passphrase": "12345678", "wpa": "2",
                           "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP"})
    logger.info("WPS provisioning step")
    pin = dev[0].wps_read_pin()
    hapd.request("WPS_PIN any " + pin)
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[0].dump_monitor()
    dev[0].request("WPS_PIN %s %s" % (apdev[0]['bssid'], pin))
    dev[0].wait_connected(timeout=30)
    status = dev[0].get_status()
    if status['wpa_state'] != 'COMPLETED' or status['bssid'] != apdev[0]['bssid']:
        raise Exception("Not fully connected")
    if status['ssid'] != ssid:
        raise Exception("Unexpected SSID")
    if status['pairwise_cipher'] != 'CCMP' or status['group_cipher'] != 'CCMP':
        raise Exception("Unexpected encryption configuration")
    if status['key_mgmt'] != 'WPA2-PSK':
        raise Exception("Unexpected key_mgmt")

    dev[1].scan_for_bss(apdev[0]['bssid'], freq="2412", force_scan=True)
    bss = dev[1].get_bss(apdev[0]['bssid'])
    if "[WPS-AUTH]" in bss['flags']:
        raise Exception("WPS-AUTH flag not cleared")
    logger.info("Try to connect from another station using the same PIN")
    pin = dev[1].request("WPS_PIN " + apdev[0]['bssid'])
    ev = dev[1].wait_event(["WPS-M2D", "CTRL-EVENT-CONNECTED"], timeout=30)
    if ev is None:
        raise Exception("Operation timed out")
    if "WPS-M2D" not in ev:
        raise Exception("Unexpected WPS operation started")
    hapd.request("WPS_PIN any " + pin)
    dev[1].wait_connected(timeout=30)

def test_ap_wps_conf_pin_mixed_mode(dev, apdev):
    """WPS PIN provisioning with configured AP (WPA+WPA2)"""
    skip_without_tkip(dev[0])
    ssid = "test-wps-conf-pin-mixed"
    hapd = hostapd.add_ap(apdev[0],
                          {"ssid": ssid, "eap_server": "1", "wps_state": "2",
                           "wpa_passphrase": "12345678", "wpa": "3",
                           "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP",
                           "wpa_pairwise": "TKIP"})

    logger.info("WPS provisioning step")
    pin = dev[0].wps_read_pin()
    hapd.request("WPS_PIN any " + pin)
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[0].dump_monitor()
    dev[0].request("WPS_PIN %s %s" % (apdev[0]['bssid'], pin))
    dev[0].wait_connected(timeout=30)
    status = dev[0].get_status()
    dev[0].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected()
    if status['pairwise_cipher'] != 'CCMP' or status['group_cipher'] != 'TKIP' or status['key_mgmt'] != 'WPA2-PSK':
        raise Exception("Unexpected encryption/key_mgmt configuration: pairwise=%s group=%s key_mgmt=%s" % (status['pairwise_cipher'], status['group_cipher'], status['key_mgmt']))

    logger.info("WPS provisioning step (auth_types=0x1b)")
    if "OK" not in dev[0].request("SET wps_force_auth_types 0x1b"):
        raise Exception("Failed to set wps_force_auth_types 0x1b")
    pin = dev[0].wps_read_pin()
    hapd.request("WPS_PIN any " + pin)
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[0].dump_monitor()
    dev[0].request("WPS_PIN %s %s" % (apdev[0]['bssid'], pin))
    dev[0].wait_connected(timeout=30)
    status = dev[0].get_status()
    dev[0].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected()
    if status['pairwise_cipher'] != 'CCMP' or status['group_cipher'] != 'TKIP' or status['key_mgmt'] != 'WPA2-PSK':
        raise Exception("Unexpected encryption/key_mgmt configuration: pairwise=%s group=%s key_mgmt=%s" % (status['pairwise_cipher'], status['group_cipher'], status['key_mgmt']))

    logger.info("WPS provisioning step (auth_types=0 encr_types=0)")
    if "OK" not in dev[0].request("SET wps_force_auth_types 0"):
        raise Exception("Failed to set wps_force_auth_types 0")
    if "OK" not in dev[0].request("SET wps_force_encr_types 0"):
        raise Exception("Failed to set wps_force_encr_types 0")
    pin = dev[0].wps_read_pin()
    hapd.request("WPS_PIN any " + pin)
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[0].dump_monitor()
    dev[0].request("WPS_PIN %s %s" % (apdev[0]['bssid'], pin))
    dev[0].wait_connected(timeout=30)
    status = dev[0].get_status()
    dev[0].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected()
    if status['pairwise_cipher'] != 'CCMP' or status['group_cipher'] != 'TKIP' or status['key_mgmt'] != 'WPA2-PSK':
        raise Exception("Unexpected encryption/key_mgmt configuration: pairwise=%s group=%s key_mgmt=%s" % (status['pairwise_cipher'], status['group_cipher'], status['key_mgmt']))

    dev[0].request("SET wps_force_auth_types ")
    dev[0].request("SET wps_force_encr_types ")

@remote_compatible
def test_ap_wps_conf_pin_v1(dev, apdev):
    """WPS PIN provisioning with configured WPS v1.0 AP"""
    ssid = "test-wps-conf-pin-v1"
    hapd = hostapd.add_ap(apdev[0],
                          {"ssid": ssid, "eap_server": "1", "wps_state": "2",
                           "wpa_passphrase": "12345678", "wpa": "2",
                           "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP"})
    logger.info("WPS provisioning step")
    pin = dev[0].wps_read_pin()
    hapd.request("SET wps_version_number 0x10")
    hapd.request("WPS_PIN any " + pin)
    found = False
    for i in range(0, 10):
        dev[0].scan(freq="2412")
        if "[WPS-PIN]" in dev[0].request("SCAN_RESULTS"):
            found = True
            break
    if not found:
        hapd.request("SET wps_version_number 0x20")
        raise Exception("WPS-PIN flag not seen in scan results")
    dev[0].dump_monitor()
    dev[0].request("WPS_PIN %s %s" % (apdev[0]['bssid'], pin))
    dev[0].wait_connected(timeout=30)
    hapd.request("SET wps_version_number 0x20")

@remote_compatible
def test_ap_wps_conf_pin_2sta(dev, apdev):
    """Two stations trying to use WPS PIN at the same time"""
    ssid = "test-wps-conf-pin2"
    hapd = hostapd.add_ap(apdev[0],
                          {"ssid": ssid, "eap_server": "1", "wps_state": "2",
                           "wpa_passphrase": "12345678", "wpa": "2",
                           "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP"})
    logger.info("WPS provisioning step")
    pin = "12345670"
    pin2 = "55554444"
    hapd.request("WPS_PIN " + dev[0].get_status_field("uuid") + " " + pin)
    hapd.request("WPS_PIN " + dev[1].get_status_field("uuid") + " " + pin)
    dev[0].dump_monitor()
    dev[1].dump_monitor()
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[1].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[0].request("WPS_PIN %s %s" % (apdev[0]['bssid'], pin))
    dev[1].request("WPS_PIN %s %s" % (apdev[0]['bssid'], pin))
    dev[0].wait_connected(timeout=30)
    dev[1].wait_connected(timeout=30)

@remote_compatible
def test_ap_wps_conf_pin_timeout(dev, apdev):
    """WPS PIN provisioning with configured AP timing out PIN"""
    ssid = "test-wps-conf-pin"
    hapd = hostapd.add_ap(apdev[0],
                          {"ssid": ssid, "eap_server": "1", "wps_state": "2",
                           "wpa_passphrase": "12345678", "wpa": "2",
                           "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP"})
    addr = dev[0].p2p_interface_addr()
    pin = dev[0].wps_read_pin()
    if "FAIL" not in hapd.request("WPS_PIN "):
        raise Exception("Unexpected success on invalid WPS_PIN")
    hapd.request("WPS_PIN any " + pin + " 1")
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    time.sleep(1.1)
    dev[0].request("WPS_PIN %s %s" % (apdev[0]['bssid'], pin))
    ev = hapd.wait_event(["WPS-PIN-NEEDED"], timeout=20)
    if ev is None:
        raise Exception("WPS-PIN-NEEDED event timed out")
    ev = dev[0].wait_event(["WPS-M2D"])
    if ev is None:
        raise Exception("M2D not reported")
    dev[0].request("WPS_CANCEL")

    hapd.request("WPS_PIN any " + pin + " 20 " + addr)
    dev[0].request("WPS_PIN %s %s" % (apdev[0]['bssid'], pin))
    dev[0].wait_connected(timeout=30)

def test_ap_wps_reg_connect(dev, apdev):
    """WPS registrar using AP PIN to connect"""
    ssid = "test-wps-reg-ap-pin"
    appin = "12345670"
    hostapd.add_ap(apdev[0],
                   {"ssid": ssid, "eap_server": "1", "wps_state": "2",
                    "wpa_passphrase": "12345678", "wpa": "2",
                    "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP",
                    "ap_pin": appin})
    logger.info("WPS provisioning step")
    dev[0].dump_monitor()
    dev[0].flush_scan_cache()
    dev[0].scan_for_bss(apdev[0]['bssid'], freq=2412)
    dev[0].wps_reg(apdev[0]['bssid'], appin)
    status = dev[0].get_status()
    if status['wpa_state'] != 'COMPLETED' or status['bssid'] != apdev[0]['bssid']:
        raise Exception("Not fully connected")
    if status['ssid'] != ssid:
        raise Exception("Unexpected SSID")
    if status['pairwise_cipher'] != 'CCMP' or status['group_cipher'] != 'CCMP':
        raise Exception("Unexpected encryption configuration")
    if status['key_mgmt'] != 'WPA2-PSK':
        raise Exception("Unexpected key_mgmt")

def test_ap_wps_reg_connect_zero_len_ap_pin(dev, apdev):
    """hostapd with zero length ap_pin parameter"""
    ssid = "test-wps-reg-ap-pin"
    appin = ""
    hostapd.add_ap(apdev[0],
                   {"ssid": ssid, "eap_server": "1", "wps_state": "2",
                    "wpa_passphrase": "12345678", "wpa": "2",
                    "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP",
                    "ap_pin": appin})
    logger.info("WPS provisioning step")
    dev[0].dump_monitor()
    dev[0].flush_scan_cache()
    dev[0].scan_for_bss(apdev[0]['bssid'], freq=2412)
    dev[0].wps_reg(apdev[0]['bssid'], appin, no_wait=True)
    ev = dev[0].wait_event(["WPS-FAIL"], timeout=15)
    if ev is None:
        raise Exception("No WPS-FAIL reported")
    if "msg=5 config_error=15" not in ev:
        raise Exception("Unexpected WPS-FAIL: " + ev)

def test_ap_wps_reg_connect_mixed_mode(dev, apdev):
    """WPS registrar using AP PIN to connect (WPA+WPA2)"""
    skip_without_tkip(dev[0])
    ssid = "test-wps-reg-ap-pin"
    appin = "12345670"
    hostapd.add_ap(apdev[0],
                   {"ssid": ssid, "eap_server": "1", "wps_state": "2",
                    "wpa_passphrase": "12345678", "wpa": "3",
                    "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP",
                    "wpa_pairwise": "TKIP", "ap_pin": appin})
    dev[0].flush_scan_cache()
    dev[0].scan_for_bss(apdev[0]['bssid'], freq=2412)
    dev[0].wps_reg(apdev[0]['bssid'], appin)
    status = dev[0].get_status()
    if status['wpa_state'] != 'COMPLETED' or status['bssid'] != apdev[0]['bssid']:
        raise Exception("Not fully connected")
    if status['ssid'] != ssid:
        raise Exception("Unexpected SSID")
    if status['pairwise_cipher'] != 'CCMP' or status['group_cipher'] != 'TKIP':
        raise Exception("Unexpected encryption configuration")
    if status['key_mgmt'] != 'WPA2-PSK':
        raise Exception("Unexpected key_mgmt")

def test_ap_wps_reg_override_ap_settings(dev, apdev):
    """WPS registrar and ap_settings override"""
    ap_settings = "/tmp/ap_wps_reg_override_ap_settings"
    try:
        os.remove(ap_settings)
    except:
        pass
    # Override AP Settings with values that point to another AP
    data = build_wsc_attr(ATTR_NETWORK_INDEX, b'\x01')
    data += build_wsc_attr(ATTR_SSID, b"test")
    data += build_wsc_attr(ATTR_AUTH_TYPE, b'\x00\x01')
    data += build_wsc_attr(ATTR_ENCR_TYPE, b'\x00\x01')
    data += build_wsc_attr(ATTR_NETWORK_KEY, b'')
    data += build_wsc_attr(ATTR_MAC_ADDR, binascii.unhexlify(apdev[1]['bssid'].replace(':', '')))
    with open(ap_settings, "wb") as f:
        f.write(data)
    ssid = "test-wps-reg-ap-pin"
    appin = "12345670"
    hostapd.add_ap(apdev[0],
                   {"ssid": ssid, "eap_server": "1", "wps_state": "2",
                    "wpa_passphrase": "12345678", "wpa": "2",
                    "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP",
                    "ap_pin": appin, "ap_settings": ap_settings})
    hapd2 = hostapd.add_ap(apdev[1], {"ssid": "test"})
    dev[0].flush_scan_cache()
    dev[0].scan_for_bss(apdev[0]['bssid'], freq=2412)
    dev[0].scan_for_bss(apdev[1]['bssid'], freq=2412)
    dev[0].wps_reg(apdev[0]['bssid'], appin)
    ev = hapd2.wait_event(['AP-STA-CONNECTED'], timeout=10)
    os.remove(ap_settings)
    if ev is None:
        raise Exception("No connection with the other AP")

def check_wps_reg_failure(dev, ap, appin):
    dev.request("WPS_REG " + ap['bssid'] + " " + appin)
    ev = dev.wait_event(["WPS-SUCCESS", "WPS-FAIL"], timeout=15)
    if ev is None:
        raise Exception("WPS operation timed out")
    if "WPS-SUCCESS" in ev:
        raise Exception("WPS operation succeeded unexpectedly")
    if "config_error=15" not in ev:
        raise Exception("WPS setup locked state was not reported correctly")

def test_ap_wps_random_ap_pin(dev, apdev):
    """WPS registrar using random AP PIN"""
    ssid = "test-wps-reg-random-ap-pin"
    ap_uuid = "27ea801a-9e5c-4e73-bd82-f89cbcd10d7e"
    params = {"ssid": ssid, "eap_server": "1", "wps_state": "2",
              "wpa_passphrase": "12345678", "wpa": "2",
              "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP",
              "device_name": "Wireless AP", "manufacturer": "Company",
              "model_name": "WAP", "model_number": "123",
              "serial_number": "12345", "device_type": "6-0050F204-1",
              "os_version": "01020300",
              "config_methods": "label push_button",
              "uuid": ap_uuid, "upnp_iface": "lo"}
    hapd = hostapd.add_ap(apdev[0], params)
    appin = hapd.request("WPS_AP_PIN random")
    if "FAIL" in appin:
        raise Exception("Could not generate random AP PIN")
    if appin not in hapd.request("WPS_AP_PIN get"):
        raise Exception("Could not fetch current AP PIN")
    logger.info("WPS provisioning step")
    dev[0].flush_scan_cache()
    dev[0].scan_for_bss(apdev[0]['bssid'], freq=2412)
    dev[0].wps_reg(apdev[0]['bssid'], appin)

    hapd.request("WPS_AP_PIN disable")
    logger.info("WPS provisioning step with AP PIN disabled")
    dev[1].scan_for_bss(apdev[0]['bssid'], freq=2412)
    check_wps_reg_failure(dev[1], apdev[0], appin)

    logger.info("WPS provisioning step with AP PIN reset")
    appin = "12345670"
    hapd.request("WPS_AP_PIN set " + appin)
    dev[1].wps_reg(apdev[0]['bssid'], appin)
    dev[0].request("REMOVE_NETWORK all")
    dev[1].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected(timeout=10)
    dev[1].wait_disconnected(timeout=10)

    logger.info("WPS provisioning step after AP PIN timeout")
    hapd.request("WPS_AP_PIN disable")
    appin = hapd.request("WPS_AP_PIN random 1")
    time.sleep(1.1)
    if "FAIL" not in hapd.request("WPS_AP_PIN get"):
        raise Exception("AP PIN unexpectedly still enabled")
    check_wps_reg_failure(dev[0], apdev[0], appin)

    logger.info("WPS provisioning step after AP PIN timeout(2)")
    hapd.request("WPS_AP_PIN disable")
    appin = "12345670"
    hapd.request("WPS_AP_PIN set " + appin + " 1")
    time.sleep(1.1)
    if "FAIL" not in hapd.request("WPS_AP_PIN get"):
        raise Exception("AP PIN unexpectedly still enabled")
    check_wps_reg_failure(dev[1], apdev[0], appin)

    with fail_test(hapd, 1, "os_get_random;wps_generate_pin"):
        hapd.request("WPS_AP_PIN random 1")
        hapd.request("WPS_AP_PIN disable")

    with alloc_fail(hapd, 1, "upnp_wps_set_ap_pin"):
        hapd.request("WPS_AP_PIN set 12345670")
        hapd.request("WPS_AP_PIN disable")

    if "FAIL" not in hapd.request("WPS_AP_PIN set"):
        raise Exception("Invalid WPS_AP_PIN accepted")
    if "FAIL" not in hapd.request("WPS_AP_PIN foo"):
        raise Exception("Invalid WPS_AP_PIN accepted")
    if "FAIL" not in hapd.request("WPS_AP_PIN set " + 9*'1'):
        raise Exception("Invalid WPS_AP_PIN accepted")

def test_ap_wps_reg_config(dev, apdev):
    """WPS registrar configuring an AP using AP PIN"""
    ssid = "test-wps-init-ap-pin"
    appin = "12345670"
    hostapd.add_ap(apdev[0],
                   {"ssid": ssid, "eap_server": "1", "wps_state": "2",
                    "ap_pin": appin})
    logger.info("WPS configuration step")
    dev[0].flush_scan_cache()
    dev[0].scan_for_bss(apdev[0]['bssid'], freq=2412)
    dev[0].dump_monitor()
    new_ssid = "wps-new-ssid"
    new_passphrase = "1234567890"
    dev[0].wps_reg(apdev[0]['bssid'], appin, new_ssid, "WPA2PSK", "CCMP",
                   new_passphrase)
    status = dev[0].get_status()
    if status['wpa_state'] != 'COMPLETED' or status['bssid'] != apdev[0]['bssid']:
        raise Exception("Not fully connected")
    if status['ssid'] != new_ssid:
        raise Exception("Unexpected SSID")
    if status['pairwise_cipher'] != 'CCMP' or status['group_cipher'] != 'CCMP':
        raise Exception("Unexpected encryption configuration")
    if status['key_mgmt'] != 'WPA2-PSK':
        raise Exception("Unexpected key_mgmt")

    logger.info("Re-configure back to open")
    dev[0].request("REMOVE_NETWORK all")
    dev[0].flush_scan_cache()
    dev[0].dump_monitor()
    dev[0].wps_reg(apdev[0]['bssid'], appin, "wps-open", "OPEN", "NONE", "")
    status = dev[0].get_status()
    if status['wpa_state'] != 'COMPLETED' or status['bssid'] != apdev[0]['bssid']:
        raise Exception("Not fully connected")
    if status['ssid'] != "wps-open":
        raise Exception("Unexpected SSID")
    if status['key_mgmt'] != 'NONE':
        raise Exception("Unexpected key_mgmt")

def test_ap_wps_reg_config_ext_processing(dev, apdev):
    """WPS registrar configuring an AP with external config processing"""
    ssid = "test-wps-init-ap-pin"
    appin = "12345670"
    params = {"ssid": ssid, "eap_server": "1", "wps_state": "2",
              "wps_cred_processing": "1", "ap_pin": appin}
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].flush_scan_cache()
    dev[0].scan_for_bss(apdev[0]['bssid'], freq=2412)
    new_ssid = "wps-new-ssid"
    new_passphrase = "1234567890"
    dev[0].wps_reg(apdev[0]['bssid'], appin, new_ssid, "WPA2PSK", "CCMP",
                   new_passphrase, no_wait=True)
    ev = dev[0].wait_event(["WPS-SUCCESS"], timeout=15)
    if ev is None:
        raise Exception("WPS registrar operation timed out")
    ev = hapd.wait_event(["WPS-NEW-AP-SETTINGS"], timeout=15)
    if ev is None:
        raise Exception("WPS configuration timed out")
    if "1026" not in ev:
        raise Exception("AP Settings missing from event")
    hapd.request("SET wps_cred_processing 0")
    if "FAIL" in hapd.request("WPS_CONFIG " + binascii.hexlify(new_ssid.encode()).decode() + " WPA2PSK CCMP " + binascii.hexlify(new_passphrase.encode()).decode()):
        raise Exception("WPS_CONFIG command failed")
    dev[0].wait_connected(timeout=15)

def test_ap_wps_reg_config_tkip(dev, apdev):
    """WPS registrar configuring AP to use TKIP and AP upgrading to TKIP+CCMP"""
    skip_with_fips(dev[0])
    skip_without_tkip(dev[0])
    ssid = "test-wps-init-ap"
    appin = "12345670"
    hapd = hostapd.add_ap(apdev[0],
                          {"ssid": ssid, "eap_server": "1", "wps_state": "1",
                           "ap_pin": appin})
    logger.info("WPS configuration step")
    dev[0].flush_scan_cache()
    dev[0].request("SET wps_version_number 0x10")
    dev[0].scan_for_bss(apdev[0]['bssid'], freq=2412)
    dev[0].dump_monitor()
    new_ssid = "wps-new-ssid-with-tkip"
    new_passphrase = "1234567890"
    dev[0].wps_reg(apdev[0]['bssid'], appin, new_ssid, "WPAPSK", "TKIP",
                   new_passphrase)
    logger.info("Re-connect to verify WPA2 mixed mode")
    dev[0].request("DISCONNECT")
    id = 0
    dev[0].set_network(id, "pairwise", "CCMP")
    dev[0].set_network(id, "proto", "RSN")
    dev[0].connect_network(id)
    status = dev[0].get_status()
    if status['wpa_state'] != 'COMPLETED' or status['bssid'] != apdev[0]['bssid']:
        raise Exception("Not fully connected: wpa_state={} bssid={}".format(status['wpa_state'], status['bssid']))
    if status['ssid'] != new_ssid:
        raise Exception("Unexpected SSID")
    if status['pairwise_cipher'] != 'CCMP':
        raise Exception("Unexpected encryption configuration")
    if status['group_cipher'] != 'TKIP':
        conf = hapd.request("GET_CONFIG")
        if "group_cipher=CCMP" not in conf or status['group_cipher'] != 'CCMP':
            raise Exception("Unexpected encryption configuration")
    if status['key_mgmt'] != 'WPA2-PSK':
        raise Exception("Unexpected key_mgmt")

def test_ap_wps_setup_locked(dev, apdev):
    """WPS registrar locking up AP setup on AP PIN failures"""
    ssid = "test-wps-incorrect-ap-pin"
    appin = "12345670"
    hapd = hostapd.add_ap(apdev[0],
                          {"ssid": ssid, "eap_server": "1", "wps_state": "2",
                           "wpa_passphrase": "12345678", "wpa": "2",
                           "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP",
                           "ap_pin": appin})
    new_ssid = "wps-new-ssid-test"
    new_passphrase = "1234567890"

    dev[0].flush_scan_cache()
    dev[0].scan_for_bss(apdev[0]['bssid'], freq=2412)
    ap_setup_locked = False
    for pin in ["55554444", "1234", "12345678", "00000000", "11111111"]:
        dev[0].dump_monitor()
        logger.info("Try incorrect AP PIN - attempt " + pin)
        dev[0].wps_reg(apdev[0]['bssid'], pin, new_ssid, "WPA2PSK",
                       "CCMP", new_passphrase, no_wait=True)
        ev = dev[0].wait_event(["WPS-FAIL", "CTRL-EVENT-CONNECTED"])
        if ev is None:
            raise Exception("Timeout on receiving WPS operation failure event")
        if "CTRL-EVENT-CONNECTED" in ev:
            raise Exception("Unexpected connection")
        if "config_error=15" in ev:
            logger.info("AP Setup Locked")
            ap_setup_locked = True
        elif "config_error=18" not in ev:
            raise Exception("config_error=18 not reported")
        dev[0].wait_disconnected(timeout=10)
        time.sleep(0.1)
    if not ap_setup_locked:
        raise Exception("AP setup was not locked")
    dev[0].request("WPS_CANCEL")
    dev[0].scan_for_bss(apdev[0]['bssid'], freq=2412, force_scan=True,
                        only_new=True)
    bss = dev[0].get_bss(apdev[0]['bssid'])
    if 'wps_ap_setup_locked' not in bss or bss['wps_ap_setup_locked'] != '1':
        logger.info("BSS: " + str(bss))
        raise Exception("AP Setup Locked not indicated in scan results")

    status = hapd.request("WPS_GET_STATUS")
    if "Last WPS result: Failed" not in status:
        raise Exception("WPS failure result not shown correctly")
    if "Peer Address: " + dev[0].p2p_interface_addr() not in status:
        raise Exception("Peer address not shown correctly")

    time.sleep(0.5)
    dev[0].dump_monitor()
    logger.info("WPS provisioning step")
    pin = dev[0].wps_read_pin()
    hapd.request("WPS_PIN any " + pin)
    dev[0].request("WPS_PIN %s %s" % (apdev[0]['bssid'], pin))
    ev = dev[0].wait_event(["WPS-SUCCESS"], timeout=30)
    if ev is None:
        raise Exception("WPS success was not reported")
    dev[0].wait_connected(timeout=30)

    appin = hapd.request("WPS_AP_PIN random")
    if "FAIL" in appin:
        raise Exception("Could not generate random AP PIN")
    ev = hapd.wait_event(["WPS-AP-SETUP-UNLOCKED"], timeout=10)
    if ev is None:
        raise Exception("Failed to unlock AP PIN")

def test_ap_wps_setup_locked_timeout(dev, apdev):
    """WPS re-enabling AP PIN after timeout"""
    ssid = "test-wps-incorrect-ap-pin"
    appin = "12345670"
    hapd = hostapd.add_ap(apdev[0],
                          {"ssid": ssid, "eap_server": "1", "wps_state": "2",
                           "wpa_passphrase": "12345678", "wpa": "2",
                           "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP",
                           "ap_pin": appin})
    new_ssid = "wps-new-ssid-test"
    new_passphrase = "1234567890"

    dev[0].flush_scan_cache()
    dev[0].scan_for_bss(apdev[0]['bssid'], freq=2412)
    ap_setup_locked = False
    for pin in ["55554444", "1234", "12345678", "00000000", "11111111"]:
        dev[0].dump_monitor()
        logger.info("Try incorrect AP PIN - attempt " + pin)
        dev[0].wps_reg(apdev[0]['bssid'], pin, new_ssid, "WPA2PSK",
                       "CCMP", new_passphrase, no_wait=True)
        ev = dev[0].wait_event(["WPS-FAIL", "CTRL-EVENT-CONNECTED"], timeout=15)
        if ev is None:
            raise Exception("Timeout on receiving WPS operation failure event")
        if "CTRL-EVENT-CONNECTED" in ev:
            raise Exception("Unexpected connection")
        if "config_error=15" in ev:
            logger.info("AP Setup Locked")
            ap_setup_locked = True
            break
        elif "config_error=18" not in ev:
            raise Exception("config_error=18 not reported")
        dev[0].wait_disconnected(timeout=10)
        time.sleep(0.1)
    if not ap_setup_locked:
        raise Exception("AP setup was not locked")
    ev = hapd.wait_event(["WPS-AP-SETUP-UNLOCKED"], timeout=80)
    if ev is None:
        raise Exception("AP PIN did not get unlocked on 60 second timeout")

def test_ap_wps_setup_locked_2(dev, apdev):
    """WPS AP configured for special ap_setup_locked=2 mode"""
    ssid = "test-wps-ap-pin"
    appin = "12345670"
    params = {"ssid": ssid, "eap_server": "1", "wps_state": "2",
              "wpa_passphrase": "12345678", "wpa": "2",
              "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP",
              "ap_pin": appin, "ap_setup_locked": "2"}
    hapd = hostapd.add_ap(apdev[0], params)
    new_ssid = "wps-new-ssid-test"
    new_passphrase = "1234567890"

    dev[0].flush_scan_cache()
    dev[0].scan_for_bss(apdev[0]['bssid'], freq=2412)
    dev[0].wps_reg(apdev[0]['bssid'], appin)
    dev[0].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected()

    hapd.dump_monitor()
    dev[0].dump_monitor()
    dev[0].wps_reg(apdev[0]['bssid'], appin, new_ssid, "WPA2PSK",
                   "CCMP", new_passphrase, no_wait=True)

    ev = hapd.wait_event(["WPS-FAIL"], timeout=5)
    if ev is None:
        raise Exception("hostapd did not report WPS failure")
    if "msg=12 config_error=15" not in ev:
        raise Exception("Unexpected failure reason (AP): " + ev)

    ev = dev[0].wait_event(["WPS-FAIL", "CTRL-EVENT-CONNECTED"])
    if ev is None:
        raise Exception("Timeout on receiving WPS operation failure event")
    if "CTRL-EVENT-CONNECTED" in ev:
        raise Exception("Unexpected connection")
    if "config_error=15" not in ev:
        raise Exception("Unexpected failure reason (STA): " + ev)
    dev[0].request("WPS_CANCEL")
    dev[0].wait_disconnected()

def setup_ap_wps_pbc_overlap_2ap(apdev):
    params = {"ssid": "wps1", "eap_server": "1", "wps_state": "2",
              "wpa_passphrase": "12345678", "wpa": "2",
              "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP",
              "wps_independent": "1"}
    hapd = hostapd.add_ap(apdev[0], params)
    params = {"ssid": "wps2", "eap_server": "1", "wps_state": "2",
              "wpa_passphrase": "123456789", "wpa": "2",
              "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP",
              "wps_independent": "1"}
    hapd2 = hostapd.add_ap(apdev[1], params)
    hapd.request("WPS_PBC")
    hapd2.request("WPS_PBC")
    return hapd, hapd2

@remote_compatible
def test_ap_wps_pbc_overlap_2ap(dev, apdev):
    """WPS PBC session overlap with two active APs"""
    hapd, hapd2 = setup_ap_wps_pbc_overlap_2ap(apdev)
    logger.info("WPS provisioning step")
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412", force_scan=True)
    dev[0].scan_for_bss(apdev[1]['bssid'], freq="2412")
    dev[0].request("WPS_PBC")
    ev = dev[0].wait_event(["WPS-OVERLAP-DETECTED"], timeout=15)
    hapd.request("DISABLE")
    hapd2.request("DISABLE")
    dev[0].flush_scan_cache()
    if ev is None:
        raise Exception("PBC session overlap not detected")

@remote_compatible
def test_ap_wps_pbc_overlap_2ap_specific_bssid(dev, apdev):
    """WPS PBC session overlap with two active APs (specific BSSID selected)"""
    hapd, hapd2 = setup_ap_wps_pbc_overlap_2ap(apdev)
    logger.info("WPS provisioning step")
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412", force_scan=True)
    dev[0].scan_for_bss(apdev[1]['bssid'], freq="2412")
    dev[0].request("WPS_PBC " + apdev[0]['bssid'])
    ev = dev[0].wait_event(["WPS-OVERLAP-DETECTED",
                            "CTRL-EVENT-CONNECTED"], timeout=15)
    dev[0].request("DISCONNECT")
    hapd.request("DISABLE")
    hapd2.request("DISABLE")
    dev[0].flush_scan_cache()
    if ev is None:
        raise Exception("PBC session overlap result not reported")
    if "CTRL-EVENT-CONNECTED" not in ev:
        raise Exception("Connection did not complete")

@remote_compatible
def test_ap_wps_pbc_overlap_2sta(dev, apdev):
    """WPS PBC session overlap with two active STAs"""
    ssid = "test-wps-pbc-overlap"
    hapd = hostapd.add_ap(apdev[0],
                          {"ssid": ssid, "eap_server": "1", "wps_state": "2",
                           "wpa_passphrase": "12345678", "wpa": "2",
                           "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP"})
    logger.info("WPS provisioning step")
    hapd.request("WPS_PBC")
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[0].dump_monitor()
    dev[1].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[1].dump_monitor()
    dev[0].request("WPS_PBC " + apdev[0]['bssid'])
    dev[1].request("WPS_PBC " + apdev[0]['bssid'])
    ev = dev[0].wait_event(["WPS-M2D"], timeout=15)
    if ev is None:
        raise Exception("PBC session overlap not detected (dev0)")
    if "config_error=12" not in ev:
        raise Exception("PBC session overlap not correctly reported (dev0)")
    dev[0].request("WPS_CANCEL")
    dev[0].request("DISCONNECT")
    ev = dev[1].wait_event(["WPS-M2D"], timeout=15)
    if ev is None:
        raise Exception("PBC session overlap not detected (dev1)")
    if "config_error=12" not in ev:
        raise Exception("PBC session overlap not correctly reported (dev1)")
    dev[1].request("WPS_CANCEL")
    dev[1].request("DISCONNECT")
    ev = hapd.wait_event(["WPS-OVERLAP-DETECTED"], timeout=1)
    if ev is None:
        raise Exception("PBC session overlap not detected (AP)")
    if "PBC Status: Overlap" not in hapd.request("WPS_GET_STATUS"):
        raise Exception("PBC status not shown correctly")
    hapd.request("WPS_CANCEL")
    ret = hapd.request("WPS_PBC")
    if "FAIL" not in ret:
        raise Exception("PBC mode allowed to be started while PBC overlap still active")
    hapd.request("DISABLE")
    dev[0].flush_scan_cache()
    dev[1].flush_scan_cache()

@remote_compatible
def test_ap_wps_cancel(dev, apdev):
    """WPS AP cancelling enabled config method"""
    ssid = "test-wps-ap-cancel"
    hapd = hostapd.add_ap(apdev[0],
                          {"ssid": ssid, "eap_server": "1", "wps_state": "2",
                           "wpa_passphrase": "12345678", "wpa": "2",
                           "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP"})
    bssid = apdev[0]['bssid']

    logger.info("Verify PBC enable/cancel")
    hapd.request("WPS_PBC")
    dev[0].scan(freq="2412")
    dev[0].scan(freq="2412")
    bss = dev[0].get_bss(apdev[0]['bssid'])
    if "[WPS-PBC]" not in bss['flags']:
        raise Exception("WPS-PBC flag missing")
    if "FAIL" in hapd.request("WPS_CANCEL"):
        raise Exception("WPS_CANCEL failed")
    dev[0].scan(freq="2412")
    dev[0].scan(freq="2412")
    bss = dev[0].get_bss(apdev[0]['bssid'])
    if "[WPS-PBC]" in bss['flags']:
        raise Exception("WPS-PBC flag not cleared")

    logger.info("Verify PIN enable/cancel")
    hapd.request("WPS_PIN any 12345670")
    dev[0].scan(freq="2412")
    dev[0].scan(freq="2412")
    bss = dev[0].get_bss(apdev[0]['bssid'])
    if "[WPS-AUTH]" not in bss['flags']:
        raise Exception("WPS-AUTH flag missing")
    if "FAIL" in hapd.request("WPS_CANCEL"):
        raise Exception("WPS_CANCEL failed")
    dev[0].scan(freq="2412")
    dev[0].scan(freq="2412")
    bss = dev[0].get_bss(apdev[0]['bssid'])
    if "[WPS-AUTH]" in bss['flags']:
        raise Exception("WPS-AUTH flag not cleared")

def test_ap_wps_er_add_enrollee(dev, apdev):
    """WPS ER configuring AP and adding a new enrollee using PIN"""
    try:
        _test_ap_wps_er_add_enrollee(dev, apdev)
    finally:
        dev[0].request("WPS_ER_STOP")

def _test_ap_wps_er_add_enrollee(dev, apdev):
    ssid = "wps-er-add-enrollee"
    ap_pin = "12345670"
    ap_uuid = "27ea801a-9e5c-4e73-bd82-f89cbcd10d7e"
    hostapd.add_ap(apdev[0],
                   {"ssid": ssid, "eap_server": "1", "wps_state": "1",
                    "device_name": "Wireless AP", "manufacturer": "Company",
                    "model_name": "WAP", "model_number": "123",
                    "serial_number": "12345", "device_type": "6-0050F204-1",
                    "os_version": "01020300",
                    'friendly_name': "WPS AP - <>&'\" - TEST",
                    "config_methods": "label push_button",
                    "ap_pin": ap_pin, "uuid": ap_uuid, "upnp_iface": "lo"})
    logger.info("WPS configuration step")
    new_passphrase = "1234567890"
    dev[0].dump_monitor()
    dev[0].flush_scan_cache()
    dev[0].scan_for_bss(apdev[0]['bssid'], freq=2412)
    dev[0].wps_reg(apdev[0]['bssid'], ap_pin, ssid, "WPA2PSK", "CCMP",
                   new_passphrase)
    status = dev[0].get_status()
    if status['wpa_state'] != 'COMPLETED' or status['bssid'] != apdev[0]['bssid']:
        raise Exception("Not fully connected")
    if status['ssid'] != ssid:
        raise Exception("Unexpected SSID")
    if status['pairwise_cipher'] != 'CCMP' or status['group_cipher'] != 'CCMP':
        raise Exception("Unexpected encryption configuration")
    if status['key_mgmt'] != 'WPA2-PSK':
        raise Exception("Unexpected key_mgmt")

    logger.info("Start ER")
    dev[0].request("WPS_ER_START ifname=lo")
    ev = dev[0].wait_event(["WPS-ER-AP-ADD"], timeout=15)
    if ev is None:
        raise Exception("AP discovery timed out")
    if ap_uuid not in ev:
        raise Exception("Expected AP UUID not found")
    if "|WPS AP - &lt;&gt;&amp;&apos;&quot; - TEST|Company|" not in ev:
        raise Exception("Expected friendly name not found")

    logger.info("Learn AP configuration through UPnP")
    dev[0].dump_monitor()
    dev[0].request("WPS_ER_LEARN " + ap_uuid + " " + ap_pin)
    ev = dev[0].wait_event(["WPS-ER-AP-SETTINGS"], timeout=15)
    if ev is None:
        raise Exception("AP learn timed out")
    if ap_uuid not in ev:
        raise Exception("Expected AP UUID not in settings")
    if "ssid=" + ssid not in ev:
        raise Exception("Expected SSID not in settings")
    if "key=" + new_passphrase not in ev:
        raise Exception("Expected passphrase not in settings")
    ev = dev[0].wait_event(["WPS-FAIL"], timeout=15)
    if ev is None:
        raise Exception("WPS-FAIL after AP learn timed out")
    time.sleep(0.1)

    logger.info("Add Enrollee using ER")
    pin = dev[1].wps_read_pin()
    dev[0].dump_monitor()
    dev[0].request("WPS_ER_PIN any " + pin + " " + dev[1].p2p_interface_addr())
    dev[1].scan_for_bss(apdev[0]['bssid'], freq=2412)
    dev[1].dump_monitor()
    dev[1].request("WPS_PIN %s %s" % (apdev[0]['bssid'], pin))
    ev = dev[1].wait_event(["WPS-SUCCESS"], timeout=30)
    if ev is None:
        raise Exception("Enrollee did not report success")
    dev[1].wait_connected(timeout=15)
    ev = dev[0].wait_event(["WPS-SUCCESS"], timeout=15)
    if ev is None:
        raise Exception("WPS ER did not report success")
    hwsim_utils.test_connectivity_sta(dev[0], dev[1])

    logger.info("Add a specific Enrollee using ER")
    pin = dev[2].wps_read_pin()
    addr2 = dev[2].p2p_interface_addr()
    dev[0].dump_monitor()
    dev[2].scan_for_bss(apdev[0]['bssid'], freq=2412)
    dev[2].dump_monitor()
    dev[2].request("WPS_PIN %s %s" % (apdev[0]['bssid'], pin))
    ev = dev[0].wait_event(["WPS-ER-ENROLLEE-ADD"], timeout=10)
    if ev is None:
        raise Exception("Enrollee not seen")
    if addr2 not in ev:
        raise Exception("Unexpected Enrollee MAC address")
    dev[0].request("WPS_ER_PIN " + addr2 + " " + pin + " " + addr2)
    dev[2].wait_connected(timeout=30)
    ev = dev[0].wait_event(["WPS-SUCCESS"], timeout=15)
    if ev is None:
        raise Exception("WPS ER did not report success")

    logger.info("Verify registrar selection behavior")
    dev[0].request("WPS_ER_PIN any " + pin + " " + dev[1].p2p_interface_addr())
    dev[1].request("DISCONNECT")
    dev[1].wait_disconnected(timeout=10)
    dev[1].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[1].scan(freq="2412")
    bss = dev[1].get_bss(apdev[0]['bssid'])
    if "[WPS-AUTH]" not in bss['flags']:
        # It is possible for scan to miss an update especially when running
        # tests under load with multiple VMs, so allow another attempt.
        dev[1].scan(freq="2412")
        bss = dev[1].get_bss(apdev[0]['bssid'])
        if "[WPS-AUTH]" not in bss['flags']:
            raise Exception("WPS-AUTH flag missing")

    logger.info("Stop ER")
    dev[0].dump_monitor()
    dev[0].request("WPS_ER_STOP")
    ev = dev[0].wait_event(["WPS-ER-AP-REMOVE"])
    if ev is None:
        raise Exception("WPS ER unsubscription timed out")
    # It takes some time for the UPnP UNSUBSCRIBE command to go through, so wait
    # a bit before verifying that the scan results have changed.
    time.sleep(0.2)

    for i in range(0, 10):
        dev[1].request("BSS_FLUSH 0")
        dev[1].scan(freq="2412", only_new=True)
        bss = dev[1].get_bss(apdev[0]['bssid'])
        if bss and 'flags' in bss and "[WPS-AUTH]" not in bss['flags']:
            break
        logger.debug("WPS-AUTH flag was still in place - wait a bit longer")
        time.sleep(0.1)
    if "[WPS-AUTH]" in bss['flags']:
        raise Exception("WPS-AUTH flag not removed")

def test_ap_wps_er_add_enrollee_uuid(dev, apdev):
    """WPS ER adding a new enrollee identified by UUID"""
    try:
        _test_ap_wps_er_add_enrollee_uuid(dev, apdev)
    finally:
        dev[0].request("WPS_ER_STOP")

def _test_ap_wps_er_add_enrollee_uuid(dev, apdev):
    ssid = "wps-er-add-enrollee"
    ap_pin = "12345670"
    ap_uuid = "27ea801a-9e5c-4e73-bd82-f89cbcd10d7e"
    hostapd.add_ap(apdev[0],
                   {"ssid": ssid, "eap_server": "1", "wps_state": "2",
                    "wpa_passphrase": "12345678", "wpa": "2",
                    "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP",
                    "device_name": "Wireless AP", "manufacturer": "Company",
                    "model_name": "WAP", "model_number": "123",
                    "serial_number": "12345", "device_type": "6-0050F204-1",
                    "os_version": "01020300",
                    "config_methods": "label push_button",
                    "ap_pin": ap_pin, "uuid": ap_uuid, "upnp_iface": "lo"})
    logger.info("WPS configuration step")
    dev[0].flush_scan_cache()
    dev[0].scan_for_bss(apdev[0]['bssid'], freq=2412)
    dev[0].wps_reg(apdev[0]['bssid'], ap_pin)

    logger.info("Start ER")
    dev[0].request("WPS_ER_START ifname=lo")
    ev = dev[0].wait_event(["WPS-ER-AP-ADD"], timeout=15)
    if ev is None:
        raise Exception("AP discovery timed out")
    if ap_uuid not in ev:
        raise Exception("Expected AP UUID not found")

    logger.info("Learn AP configuration through UPnP")
    dev[0].dump_monitor()
    dev[0].request("WPS_ER_LEARN " + ap_uuid + " " + ap_pin)
    ev = dev[0].wait_event(["WPS-ER-AP-SETTINGS"], timeout=15)
    if ev is None:
        raise Exception("AP learn timed out")
    if ap_uuid not in ev:
        raise Exception("Expected AP UUID not in settings")
    ev = dev[0].wait_event(["WPS-FAIL"], timeout=15)
    if ev is None:
        raise Exception("WPS-FAIL after AP learn timed out")
    time.sleep(0.1)

    logger.info("Add a specific Enrollee using ER (PBC/UUID)")
    addr1 = dev[1].p2p_interface_addr()
    dev[0].dump_monitor()
    dev[1].scan_for_bss(apdev[0]['bssid'], freq=2412)
    dev[1].dump_monitor()
    dev[1].request("WPS_PBC %s" % apdev[0]['bssid'])
    ev = dev[0].wait_event(["WPS-ER-ENROLLEE-ADD"], timeout=10)
    if ev is None:
        raise Exception("Enrollee not seen")
    if addr1 not in ev:
        raise Exception("Unexpected Enrollee MAC address")
    uuid = ev.split(' ')[1]
    dev[0].request("WPS_ER_PBC " + uuid)
    dev[1].wait_connected(timeout=30)
    ev = dev[0].wait_event(["WPS-SUCCESS"], timeout=15)
    if ev is None:
        raise Exception("WPS ER did not report success")

    logger.info("Add a specific Enrollee using ER (PIN/UUID)")
    pin = dev[2].wps_read_pin()
    addr2 = dev[2].p2p_interface_addr()
    dev[0].dump_monitor()
    dev[2].scan_for_bss(apdev[0]['bssid'], freq=2412)
    dev[2].dump_monitor()
    dev[2].request("WPS_PIN %s %s" % (apdev[0]['bssid'], pin))
    ev = dev[0].wait_event(["WPS-ER-ENROLLEE-ADD"], timeout=10)
    if ev is None:
        raise Exception("Enrollee not seen")
    if addr2 not in ev:
        raise Exception("Unexpected Enrollee MAC address")
    uuid = ev.split(' ')[1]
    dev[0].request("WPS_ER_PIN " + uuid + " " + pin)
    dev[2].wait_connected(timeout=30)
    ev = dev[0].wait_event(["WPS-SUCCESS"], timeout=15)
    if ev is None:
        raise Exception("WPS ER did not report success")

    ev = dev[0].wait_event(["WPS-ER-ENROLLEE-REMOVE"], timeout=15)
    if ev is None:
        raise Exception("No Enrollee STA entry timeout seen")

    logger.info("Stop ER")
    dev[0].dump_monitor()
    dev[0].request("WPS_ER_STOP")

def test_ap_wps_er_multi_add_enrollee(dev, apdev):
    """Multiple WPS ERs adding a new enrollee using PIN"""
    try:
        _test_ap_wps_er_multi_add_enrollee(dev, apdev)
    finally:
        for i in range(2):
            dev[i].request("WPS_ER_STOP")

def _test_ap_wps_er_multi_add_enrollee(dev, apdev):
    ssid = "wps-er-add-enrollee"
    ap_pin = "12345670"
    ap_uuid = "27ea801a-9e5c-4e73-bd82-f89cbcd10d7e"
    hostapd.add_ap(apdev[0],
                   {"ssid": ssid, "eap_server": "1", "wps_state": "2",
                    "wpa_passphrase": "12345678", "wpa": "2",
                    "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP",
                    "device_name": "Wireless AP", "manufacturer": "Company",
                    "model_name": "WAP", "model_number": "123",
                    "serial_number": "12345", "device_type": "6-0050F204-1",
                    "os_version": "01020300",
                    'friendly_name': "WPS AP",
                    "config_methods": "label push_button",
                    "ap_pin": ap_pin, "uuid": ap_uuid, "upnp_iface": "lo"})

    for i in range(2):
        dev[i].flush_scan_cache()
        dev[i].scan_for_bss(apdev[0]['bssid'], freq=2412)
        dev[i].wps_reg(apdev[0]['bssid'], ap_pin)
    for i in range(2):
        dev[i].request("WPS_ER_START ifname=lo")
    for i in range(2):
        ev = dev[i].wait_event(["WPS-ER-AP-ADD"], timeout=15)
        if ev is None:
            raise Exception("AP discovery timed out")
        dev[i].dump_monitor()
    for i in range(2):
        dev[i].request("WPS_ER_LEARN " + ap_uuid + " " + ap_pin)
    for i in range(2):
        ev = dev[i].wait_event(["WPS-ER-AP-SETTINGS"], timeout=15)
        if ev is None:
            raise Exception("AP learn timed out")
        ev = dev[i].wait_event(["WPS-FAIL"], timeout=15)
        if ev is None:
            raise Exception("WPS-FAIL after AP learn timed out")

    time.sleep(0.1)

    pin = dev[2].wps_read_pin()
    addr = dev[2].own_addr()
    dev[0].dump_monitor()
    dev[0].request("WPS_ER_PIN any " + pin + " " + addr)
    dev[1].dump_monitor()
    dev[1].request("WPS_ER_PIN any " + pin + " " + addr)

    dev[2].scan_for_bss(apdev[0]['bssid'], freq=2412)
    dev[2].dump_monitor()
    dev[2].request("WPS_PIN %s %s" % (apdev[0]['bssid'], pin))
    ev = dev[2].wait_event(["WPS-SUCCESS"], timeout=30)
    if ev is None:
        raise Exception("Enrollee did not report success")
    dev[2].wait_connected(timeout=15)

def test_ap_wps_er_add_enrollee_pbc(dev, apdev):
    """WPS ER connected to AP and adding a new enrollee using PBC"""
    try:
        _test_ap_wps_er_add_enrollee_pbc(dev, apdev)
    finally:
        dev[0].request("WPS_ER_STOP")

def _test_ap_wps_er_add_enrollee_pbc(dev, apdev):
    ssid = "wps-er-add-enrollee-pbc"
    ap_pin = "12345670"
    ap_uuid = "27ea801a-9e5c-4e73-bd82-f89cbcd10d7e"
    hostapd.add_ap(apdev[0],
                   {"ssid": ssid, "eap_server": "1", "wps_state": "2",
                    "wpa_passphrase": "12345678", "wpa": "2",
                    "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP",
                    "device_name": "Wireless AP", "manufacturer": "Company",
                    "model_name": "WAP", "model_number": "123",
                    "serial_number": "12345", "device_type": "6-0050F204-1",
                    "os_version": "01020300",
                    "config_methods": "label push_button",
                    "ap_pin": ap_pin, "uuid": ap_uuid, "upnp_iface": "lo"})
    logger.info("Learn AP configuration")
    dev[0].flush_scan_cache()
    dev[0].scan_for_bss(apdev[0]['bssid'], freq=2412)
    dev[0].dump_monitor()
    dev[0].wps_reg(apdev[0]['bssid'], ap_pin)
    status = dev[0].get_status()
    if status['wpa_state'] != 'COMPLETED' or status['bssid'] != apdev[0]['bssid']:
        raise Exception("Not fully connected")

    logger.info("Start ER")
    dev[0].request("WPS_ER_START ifname=lo")
    ev = dev[0].wait_event(["WPS-ER-AP-ADD"], timeout=15)
    if ev is None:
        raise Exception("AP discovery timed out")
    if ap_uuid not in ev:
        raise Exception("Expected AP UUID not found")

    enrollee = dev[1].p2p_interface_addr()

    if "FAIL-UNKNOWN-UUID" not in dev[0].request("WPS_ER_PBC " + enrollee):
        raise Exception("Unknown UUID not reported")

    logger.info("Add Enrollee using ER and PBC")
    dev[0].dump_monitor()
    dev[1].dump_monitor()
    dev[1].request("WPS_PBC")

    for i in range(0, 2):
        ev = dev[0].wait_event(["WPS-ER-ENROLLEE-ADD"], timeout=15)
        if ev is None:
            raise Exception("Enrollee discovery timed out")
        if enrollee in ev:
            break
        if i == 1:
            raise Exception("Expected Enrollee not found")
    if "FAIL-NO-AP-SETTINGS" not in dev[0].request("WPS_ER_PBC " + enrollee):
        raise Exception("Unknown UUID not reported")
    logger.info("Use learned network configuration on ER")
    dev[0].request("WPS_ER_SET_CONFIG " + ap_uuid + " 0")
    if "OK" not in dev[0].request("WPS_ER_PBC " + enrollee):
        raise Exception("WPS_ER_PBC failed")

    ev = dev[1].wait_event(["WPS-SUCCESS"], timeout=15)
    if ev is None:
        raise Exception("Enrollee did not report success")
    dev[1].wait_connected(timeout=15)
    ev = dev[0].wait_event(["WPS-SUCCESS"], timeout=15)
    if ev is None:
        raise Exception("WPS ER did not report success")
    hwsim_utils.test_connectivity_sta(dev[0], dev[1])

def test_ap_wps_er_pbc_overlap(dev, apdev):
    """WPS ER connected to AP and PBC session overlap"""
    try:
        _test_ap_wps_er_pbc_overlap(dev, apdev)
    finally:
        dev[0].request("WPS_ER_STOP")

def _test_ap_wps_er_pbc_overlap(dev, apdev):
    ssid = "wps-er-add-enrollee-pbc"
    ap_pin = "12345670"
    ap_uuid = "27ea801a-9e5c-4e73-bd82-f89cbcd10d7e"
    hostapd.add_ap(apdev[0],
                   {"ssid": ssid, "eap_server": "1", "wps_state": "2",
                    "wpa_passphrase": "12345678", "wpa": "2",
                    "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP",
                    "device_name": "Wireless AP", "manufacturer": "Company",
                    "model_name": "WAP", "model_number": "123",
                    "serial_number": "12345", "device_type": "6-0050F204-1",
                    "os_version": "01020300",
                    "config_methods": "label push_button",
                    "ap_pin": ap_pin, "uuid": ap_uuid, "upnp_iface": "lo"})
    dev[0].flush_scan_cache()
    dev[0].scan_for_bss(apdev[0]['bssid'], freq=2412)
    dev[0].dump_monitor()
    dev[0].wps_reg(apdev[0]['bssid'], ap_pin)

    dev[1].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[2].scan_for_bss(apdev[0]['bssid'], freq="2412")
    # avoid leaving dev 1 or 2 as the last Probe Request to the AP
    dev[0].scan_for_bss(apdev[0]['bssid'], freq=2412, force_scan=True)

    dev[0].dump_monitor()
    dev[0].request("WPS_ER_START ifname=lo")

    ev = dev[0].wait_event(["WPS-ER-AP-ADD"], timeout=15)
    if ev is None:
        raise Exception("AP discovery timed out")
    if ap_uuid not in ev:
        raise Exception("Expected AP UUID not found")

    # verify BSSID selection of the AP instead of UUID
    if "FAIL" in dev[0].request("WPS_ER_SET_CONFIG " + apdev[0]['bssid'] + " 0"):
        raise Exception("Could not select AP based on BSSID")

    dev[0].dump_monitor()
    dev[1].request("WPS_PBC " + apdev[0]['bssid'])
    dev[2].request("WPS_PBC " + apdev[0]['bssid'])
    ev = dev[1].wait_event(["CTRL-EVENT-SCAN-RESULTS"], timeout=10)
    if ev is None:
        raise Exception("PBC scan failed")
    ev = dev[2].wait_event(["CTRL-EVENT-SCAN-RESULTS"], timeout=10)
    if ev is None:
        raise Exception("PBC scan failed")
    found1 = False
    found2 = False
    addr1 = dev[1].own_addr()
    addr2 = dev[2].own_addr()
    for i in range(3):
        ev = dev[0].wait_event(["WPS-ER-ENROLLEE-ADD"], timeout=15)
        if ev is None:
            raise Exception("Enrollee discovery timed out")
        if addr1 in ev:
            found1 = True
            if found2:
                break
        if addr2 in ev:
            found2 = True
            if found1:
                break
    if dev[0].request("WPS_ER_PBC " + ap_uuid) != "FAIL-PBC-OVERLAP\n":
        raise Exception("PBC overlap not reported")
    dev[1].request("WPS_CANCEL")
    dev[2].request("WPS_CANCEL")
    if dev[0].request("WPS_ER_PBC foo") != "FAIL\n":
        raise Exception("Invalid WPS_ER_PBC accepted")

def test_ap_wps_er_v10_add_enrollee_pin(dev, apdev):
    """WPS v1.0 ER connected to AP and adding a new enrollee using PIN"""
    try:
        _test_ap_wps_er_v10_add_enrollee_pin(dev, apdev)
    finally:
        dev[0].request("WPS_ER_STOP")

def _test_ap_wps_er_v10_add_enrollee_pin(dev, apdev):
    ssid = "wps-er-add-enrollee-pbc"
    ap_pin = "12345670"
    ap_uuid = "27ea801a-9e5c-4e73-bd82-f89cbcd10d7e"
    hostapd.add_ap(apdev[0],
                   {"ssid": ssid, "eap_server": "1", "wps_state": "2",
                    "wpa_passphrase": "12345678", "wpa": "2",
                    "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP",
                    "device_name": "Wireless AP", "manufacturer": "Company",
                    "model_name": "WAP", "model_number": "123",
                    "serial_number": "12345", "device_type": "6-0050F204-1",
                    "os_version": "01020300",
                    "config_methods": "label push_button",
                    "ap_pin": ap_pin, "uuid": ap_uuid, "upnp_iface": "lo"})
    logger.info("Learn AP configuration")
    dev[0].request("SET wps_version_number 0x10")
    dev[0].flush_scan_cache()
    dev[0].scan_for_bss(apdev[0]['bssid'], freq=2412)
    dev[0].dump_monitor()
    dev[0].wps_reg(apdev[0]['bssid'], ap_pin)
    status = dev[0].get_status()
    if status['wpa_state'] != 'COMPLETED' or status['bssid'] != apdev[0]['bssid']:
        raise Exception("Not fully connected")

    logger.info("Start ER")
    dev[0].request("WPS_ER_START ifname=lo")
    ev = dev[0].wait_event(["WPS-ER-AP-ADD"], timeout=15)
    if ev is None:
        raise Exception("AP discovery timed out")
    if ap_uuid not in ev:
        raise Exception("Expected AP UUID not found")

    logger.info("Use learned network configuration on ER")
    dev[0].request("WPS_ER_SET_CONFIG " + ap_uuid + " 0")

    logger.info("Add Enrollee using ER and PIN")
    enrollee = dev[1].p2p_interface_addr()
    pin = dev[1].wps_read_pin()
    dev[0].dump_monitor()
    dev[0].request("WPS_ER_PIN any " + pin + " " + enrollee)
    dev[1].scan_for_bss(apdev[0]['bssid'], freq=2412)
    dev[1].dump_monitor()
    dev[1].request("WPS_PIN %s %s" % (apdev[0]['bssid'], pin))
    dev[1].wait_connected(timeout=30)
    ev = dev[0].wait_event(["WPS-SUCCESS"], timeout=15)
    if ev is None:
        raise Exception("WPS ER did not report success")

@remote_compatible
def test_ap_wps_er_config_ap(dev, apdev):
    """WPS ER configuring AP over UPnP"""
    try:
        _test_ap_wps_er_config_ap(dev, apdev)
    finally:
        dev[0].request("WPS_ER_STOP")

def _test_ap_wps_er_config_ap(dev, apdev):
    ssid = "wps-er-ap-config"
    ap_pin = "12345670"
    ap_uuid = "27ea801a-9e5c-4e73-bd82-f89cbcd10d7e"
    hostapd.add_ap(apdev[0],
                   {"ssid": ssid, "eap_server": "1", "wps_state": "2",
                    "wpa_passphrase": "12345678", "wpa": "2",
                    "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP",
                    "device_name": "Wireless AP", "manufacturer": "Company",
                    "model_name": "WAP", "model_number": "123",
                    "serial_number": "12345", "device_type": "6-0050F204-1",
                    "os_version": "01020300",
                    "config_methods": "label push_button",
                    "ap_pin": ap_pin, "uuid": ap_uuid, "upnp_iface": "lo"})

    logger.info("Connect ER to the AP")
    dev[0].connect(ssid, psk="12345678", scan_freq="2412")

    logger.info("WPS configuration step")
    dev[0].request("WPS_ER_START ifname=lo")
    ev = dev[0].wait_event(["WPS-ER-AP-ADD"], timeout=15)
    if ev is None:
        raise Exception("AP discovery timed out")
    if ap_uuid not in ev:
        raise Exception("Expected AP UUID not found")
    new_passphrase = "1234567890"
    dev[0].request("WPS_ER_CONFIG " + apdev[0]['bssid'] + " " + ap_pin + " " +
                   binascii.hexlify(ssid.encode()).decode() + " WPA2PSK CCMP " +
                   binascii.hexlify(new_passphrase.encode()).decode())
    ev = dev[0].wait_event(["WPS-SUCCESS"])
    if ev is None:
        raise Exception("WPS ER configuration operation timed out")
    dev[0].wait_disconnected(timeout=10)
    dev[0].connect(ssid, psk="1234567890", scan_freq="2412")

    logger.info("WPS ER restart")
    dev[0].request("WPS_ER_START")
    ev = dev[0].wait_event(["WPS-ER-AP-ADD"], timeout=15)
    if ev is None:
        raise Exception("AP discovery timed out on ER restart")
    if ap_uuid not in ev:
        raise Exception("Expected AP UUID not found on ER restart")
    if "OK" not in dev[0].request("WPS_ER_STOP"):
        raise Exception("WPS_ER_STOP failed")
    if "OK" not in dev[0].request("WPS_ER_STOP"):
        raise Exception("WPS_ER_STOP failed")

@remote_compatible
def test_ap_wps_er_cache_ap_settings(dev, apdev):
    """WPS ER caching AP settings"""
    try:
        _test_ap_wps_er_cache_ap_settings(dev, apdev)
    finally:
        dev[0].request("WPS_ER_STOP")

def _test_ap_wps_er_cache_ap_settings(dev, apdev):
    ssid = "wps-er-add-enrollee"
    ap_pin = "12345670"
    ap_uuid = "27ea801a-9e5c-4e73-bd82-f89cbcd10d7e"
    params = {"ssid": ssid, "eap_server": "1", "wps_state": "2",
              "wpa_passphrase": "12345678", "wpa": "2",
              "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP",
              "device_name": "Wireless AP", "manufacturer": "Company",
              "model_name": "WAP", "model_number": "123",
              "serial_number": "12345", "device_type": "6-0050F204-1",
              "os_version": "01020300",
              "config_methods": "label push_button",
              "ap_pin": ap_pin, "uuid": ap_uuid, "upnp_iface": "lo"}
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].flush_scan_cache()
    dev[0].scan_for_bss(apdev[0]['bssid'], freq=2412)
    dev[0].wps_reg(apdev[0]['bssid'], ap_pin)
    id = int(dev[0].list_networks()[0]['id'])
    dev[0].set_network(id, "scan_freq", "2412")

    dev[0].request("WPS_ER_START ifname=lo")
    ev = dev[0].wait_event(["WPS-ER-AP-ADD"], timeout=15)
    if ev is None:
        raise Exception("AP discovery timed out")
    if ap_uuid not in ev:
        raise Exception("Expected AP UUID not found")

    dev[0].dump_monitor()
    dev[0].request("WPS_ER_LEARN " + ap_uuid + " " + ap_pin)
    ev = dev[0].wait_event(["WPS-ER-AP-SETTINGS"], timeout=15)
    if ev is None:
        raise Exception("AP learn timed out")
    ev = dev[0].wait_event(["WPS-FAIL"], timeout=15)
    if ev is None:
        raise Exception("WPS-FAIL after AP learn timed out")
    time.sleep(0.1)

    hapd.disable()

    for i in range(2):
        ev = dev[0].wait_event(["WPS-ER-AP-REMOVE", "CTRL-EVENT-DISCONNECTED"],
                               timeout=15)
        if ev is None:
            raise Exception("AP removal or disconnection timed out")

    hapd = hostapd.add_ap(apdev[0], params)
    for i in range(2):
        ev = dev[0].wait_event(["WPS-ER-AP-ADD", "CTRL-EVENT-CONNECTED"],
                               timeout=15)
        if ev is None:
            raise Exception("AP discovery or connection timed out")

    pin = dev[1].wps_read_pin()
    dev[0].dump_monitor()
    dev[0].request("WPS_ER_PIN any " + pin + " " + dev[1].p2p_interface_addr())

    time.sleep(0.2)

    dev[1].scan_for_bss(apdev[0]['bssid'], freq=2412)
    dev[1].dump_monitor()
    dev[1].request("WPS_PIN %s %s" % (apdev[0]['bssid'], pin))
    ev = dev[1].wait_event(["WPS-SUCCESS"], timeout=30)
    if ev is None:
        raise Exception("Enrollee did not report success")
    dev[1].wait_connected(timeout=15)
    ev = dev[0].wait_event(["WPS-SUCCESS"], timeout=15)
    if ev is None:
        raise Exception("WPS ER did not report success")

    dev[0].dump_monitor()
    dev[0].request("WPS_ER_STOP")

def test_ap_wps_er_cache_ap_settings_oom(dev, apdev):
    """WPS ER caching AP settings (OOM)"""
    try:
        _test_ap_wps_er_cache_ap_settings_oom(dev, apdev)
    finally:
        dev[0].request("WPS_ER_STOP")

def _test_ap_wps_er_cache_ap_settings_oom(dev, apdev):
    ssid = "wps-er-add-enrollee"
    ap_pin = "12345670"
    ap_uuid = "27ea801a-9e5c-4e73-bd82-f89cbcd10d7e"
    params = {"ssid": ssid, "eap_server": "1", "wps_state": "2",
              "wpa_passphrase": "12345678", "wpa": "2",
              "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP",
              "device_name": "Wireless AP", "manufacturer": "Company",
              "model_name": "WAP", "model_number": "123",
              "serial_number": "12345", "device_type": "6-0050F204-1",
              "os_version": "01020300",
              "config_methods": "label push_button",
              "ap_pin": ap_pin, "uuid": ap_uuid, "upnp_iface": "lo"}
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].flush_scan_cache()
    dev[0].scan_for_bss(apdev[0]['bssid'], freq=2412)
    dev[0].wps_reg(apdev[0]['bssid'], ap_pin)
    id = int(dev[0].list_networks()[0]['id'])
    dev[0].set_network(id, "scan_freq", "2412")

    dev[0].request("WPS_ER_START ifname=lo")
    ev = dev[0].wait_event(["WPS-ER-AP-ADD"], timeout=15)
    if ev is None:
        raise Exception("AP discovery timed out")
    if ap_uuid not in ev:
        raise Exception("Expected AP UUID not found")

    dev[0].dump_monitor()
    dev[0].request("WPS_ER_LEARN " + ap_uuid + " " + ap_pin)
    ev = dev[0].wait_event(["WPS-ER-AP-SETTINGS"], timeout=15)
    if ev is None:
        raise Exception("AP learn timed out")
    ev = dev[0].wait_event(["WPS-FAIL"], timeout=15)
    if ev is None:
        raise Exception("WPS-FAIL after AP learn timed out")
    time.sleep(0.1)

    with alloc_fail(dev[0], 1, "=wps_er_ap_use_cached_settings"):
        hapd.disable()

        for i in range(2):
            ev = dev[0].wait_event(["WPS-ER-AP-REMOVE",
                                    "CTRL-EVENT-DISCONNECTED"],
                                   timeout=15)
            if ev is None:
                raise Exception("AP removal or disconnection timed out")

        hapd = hostapd.add_ap(apdev[0], params)
        for i in range(2):
            ev = dev[0].wait_event(["WPS-ER-AP-ADD", "CTRL-EVENT-CONNECTED"],
                                   timeout=15)
            if ev is None:
                raise Exception("AP discovery or connection timed out")

    dev[0].request("WPS_ER_STOP")

def test_ap_wps_er_cache_ap_settings_oom2(dev, apdev):
    """WPS ER caching AP settings (OOM 2)"""
    try:
        _test_ap_wps_er_cache_ap_settings_oom2(dev, apdev)
    finally:
        dev[0].request("WPS_ER_STOP")

def _test_ap_wps_er_cache_ap_settings_oom2(dev, apdev):
    ssid = "wps-er-add-enrollee"
    ap_pin = "12345670"
    ap_uuid = "27ea801a-9e5c-4e73-bd82-f89cbcd10d7e"
    params = {"ssid": ssid, "eap_server": "1", "wps_state": "2",
              "wpa_passphrase": "12345678", "wpa": "2",
              "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP",
              "device_name": "Wireless AP", "manufacturer": "Company",
              "model_name": "WAP", "model_number": "123",
              "serial_number": "12345", "device_type": "6-0050F204-1",
              "os_version": "01020300",
              "config_methods": "label push_button",
              "ap_pin": ap_pin, "uuid": ap_uuid, "upnp_iface": "lo"}
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].flush_scan_cache()
    dev[0].scan_for_bss(apdev[0]['bssid'], freq=2412)
    dev[0].wps_reg(apdev[0]['bssid'], ap_pin)
    id = int(dev[0].list_networks()[0]['id'])
    dev[0].set_network(id, "scan_freq", "2412")

    dev[0].request("WPS_ER_START ifname=lo")
    ev = dev[0].wait_event(["WPS-ER-AP-ADD"], timeout=15)
    if ev is None:
        raise Exception("AP discovery timed out")
    if ap_uuid not in ev:
        raise Exception("Expected AP UUID not found")

    dev[0].dump_monitor()
    dev[0].request("WPS_ER_LEARN " + ap_uuid + " " + ap_pin)
    ev = dev[0].wait_event(["WPS-ER-AP-SETTINGS"], timeout=15)
    if ev is None:
        raise Exception("AP learn timed out")
    ev = dev[0].wait_event(["WPS-FAIL"], timeout=15)
    if ev is None:
        raise Exception("WPS-FAIL after AP learn timed out")
    time.sleep(0.1)

    with alloc_fail(dev[0], 1, "=wps_er_ap_cache_settings"):
        hapd.disable()

        for i in range(2):
            ev = dev[0].wait_event(["WPS-ER-AP-REMOVE",
                                    "CTRL-EVENT-DISCONNECTED"],
                                   timeout=15)
            if ev is None:
                raise Exception("AP removal or disconnection timed out")

        hapd = hostapd.add_ap(apdev[0], params)
        for i in range(2):
            ev = dev[0].wait_event(["WPS-ER-AP-ADD", "CTRL-EVENT-CONNECTED"],
                                   timeout=15)
            if ev is None:
                raise Exception("AP discovery or connection timed out")

    dev[0].request("WPS_ER_STOP")

def test_ap_wps_er_subscribe_oom(dev, apdev):
    """WPS ER subscribe OOM"""
    try:
        _test_ap_wps_er_subscribe_oom(dev, apdev)
    finally:
        dev[0].request("WPS_ER_STOP")

def _test_ap_wps_er_subscribe_oom(dev, apdev):
    ssid = "wps-er-add-enrollee"
    ap_pin = "12345670"
    ap_uuid = "27ea801a-9e5c-4e73-bd82-f89cbcd10d7e"
    params = {"ssid": ssid, "eap_server": "1", "wps_state": "2",
              "wpa_passphrase": "12345678", "wpa": "2",
              "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP",
              "device_name": "Wireless AP", "manufacturer": "Company",
              "model_name": "WAP", "model_number": "123",
              "serial_number": "12345", "device_type": "6-0050F204-1",
              "os_version": "01020300",
              "config_methods": "label push_button",
              "ap_pin": ap_pin, "uuid": ap_uuid, "upnp_iface": "lo"}
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].flush_scan_cache()
    dev[0].scan_for_bss(apdev[0]['bssid'], freq=2412)
    dev[0].wps_reg(apdev[0]['bssid'], ap_pin)
    id = int(dev[0].list_networks()[0]['id'])
    dev[0].set_network(id, "scan_freq", "2412")

    with alloc_fail(dev[0], 1, "http_client_addr;wps_er_subscribe"):
        dev[0].request("WPS_ER_START ifname=lo")
        for i in range(50):
            res = dev[0].request("GET_ALLOC_FAIL")
            if res.startswith("0:"):
                break
            time.sleep(0.1)
        ev = dev[0].wait_event(["WPS-ER-AP-ADD"], timeout=0)
        if ev:
            raise Exception("Unexpected AP discovery during OOM")

    dev[0].request("WPS_ER_STOP")

def test_ap_wps_er_set_sel_reg_oom(dev, apdev):
    """WPS ER SetSelectedRegistrar OOM"""
    try:
        _test_ap_wps_er_set_sel_reg_oom(dev, apdev)
    finally:
        dev[0].request("WPS_ER_STOP")

def _test_ap_wps_er_set_sel_reg_oom(dev, apdev):
    ssid = "wps-er-add-enrollee"
    ap_pin = "12345670"
    ap_uuid = "27ea801a-9e5c-4e73-bd82-f89cbcd10d7e"
    params = {"ssid": ssid, "eap_server": "1", "wps_state": "2",
              "wpa_passphrase": "12345678", "wpa": "2",
              "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP",
              "device_name": "Wireless AP", "manufacturer": "Company",
              "model_name": "WAP", "model_number": "123",
              "serial_number": "12345", "device_type": "6-0050F204-1",
              "os_version": "01020300",
              "config_methods": "label push_button",
              "ap_pin": ap_pin, "uuid": ap_uuid, "upnp_iface": "lo"}
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].flush_scan_cache()
    dev[0].scan_for_bss(apdev[0]['bssid'], freq=2412)
    dev[0].wps_reg(apdev[0]['bssid'], ap_pin)

    dev[0].request("WPS_ER_START ifname=lo")
    ev = dev[0].wait_event(["WPS-ER-AP-ADD"], timeout=10)
    if ev is None:
        raise Exception("AP not discovered")

    dev[0].request("WPS_ER_LEARN " + ap_uuid + " " + ap_pin)
    ev = dev[0].wait_event(["WPS-ER-AP-SETTINGS"], timeout=15)
    if ev is None:
        raise Exception("AP learn timed out")
    ev = dev[0].wait_event(["WPS-FAIL"], timeout=15)
    if ev is None:
        raise Exception("WPS-FAIL timed out")
    time.sleep(0.1)

    for func in ["http_client_url_parse;wps_er_send_set_sel_reg",
                 "wps_er_soap_hdr;wps_er_send_set_sel_reg",
                 "http_client_addr;wps_er_send_set_sel_reg",
                 "wpabuf_alloc;wps_er_set_sel_reg"]:
        with alloc_fail(dev[0], 1, func):
            if "OK" not in dev[0].request("WPS_ER_PBC " + ap_uuid):
                raise Exception("WPS_ER_PBC failed")
            ev = dev[0].wait_event(["WPS-PBC-ACTIVE"], timeout=3)
            if ev is None:
                raise Exception("WPS-PBC-ACTIVE not seen")

    dev[0].request("WPS_ER_STOP")

@remote_compatible
def test_ap_wps_er_learn_oom(dev, apdev):
    """WPS ER learn OOM"""
    try:
        _test_ap_wps_er_learn_oom(dev, apdev)
    finally:
        dev[0].request("WPS_ER_STOP")

def _test_ap_wps_er_learn_oom(dev, apdev):
    ssid = "wps-er-add-enrollee"
    ap_pin = "12345670"
    ap_uuid = "27ea801a-9e5c-4e73-bd82-f89cbcd10d7e"
    params = {"ssid": ssid, "eap_server": "1", "wps_state": "2",
              "wpa_passphrase": "12345678", "wpa": "2",
              "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP",
              "device_name": "Wireless AP", "manufacturer": "Company",
              "model_name": "WAP", "model_number": "123",
              "serial_number": "12345", "device_type": "6-0050F204-1",
              "os_version": "01020300",
              "config_methods": "label push_button",
              "ap_pin": ap_pin, "uuid": ap_uuid, "upnp_iface": "lo"}
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].flush_scan_cache()
    dev[0].scan_for_bss(apdev[0]['bssid'], freq=2412)
    dev[0].wps_reg(apdev[0]['bssid'], ap_pin)

    dev[0].request("WPS_ER_START ifname=lo")
    ev = dev[0].wait_event(["WPS-ER-AP-ADD"], timeout=10)
    if ev is None:
        raise Exception("AP not discovered")

    for func in ["wps_er_http_put_message_cb",
                 "xml_get_base64_item;wps_er_http_put_message_cb",
                 "http_client_url_parse;wps_er_ap_put_message",
                 "wps_er_soap_hdr;wps_er_ap_put_message",
                 "http_client_addr;wps_er_ap_put_message"]:
        with alloc_fail(dev[0], 1, func):
            dev[0].request("WPS_ER_LEARN " + ap_uuid + " " + ap_pin)
            ev = dev[0].wait_event(["WPS-ER-AP-SETTINGS"], timeout=1)
            if ev is not None:
                raise Exception("AP learn succeeded during OOM")

    dev[0].request("WPS_ER_LEARN " + ap_uuid + " " + ap_pin)
    ev = dev[0].wait_event(["WPS-ER-AP-SETTINGS"], timeout=10)
    if ev is None:
        raise Exception("AP learn did not succeed")

    if "FAIL" not in dev[0].request("WPS_ER_LEARN 00000000-9e5c-4e73-bd82-f89cbcd10d7e " + ap_pin):
        raise Exception("WPS_ER_LEARN for unknown AP accepted")

    dev[0].request("WPS_ER_STOP")

def test_ap_wps_fragmentation(dev, apdev):
    """WPS with fragmentation in EAP-WSC and mixed mode WPA+WPA2"""
    skip_without_tkip(dev[0])
    ssid = "test-wps-fragmentation"
    appin = "12345670"
    hapd = hostapd.add_ap(apdev[0],
                          {"ssid": ssid, "eap_server": "1", "wps_state": "2",
                           "wpa_passphrase": "12345678", "wpa": "3",
                           "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP",
                           "wpa_pairwise": "TKIP", "ap_pin": appin,
                           "fragment_size": "50"})
    logger.info("WPS provisioning step (PBC)")
    hapd.request("WPS_PBC")
    dev[0].scan_for_bss(apdev[0]['bssid'], freq=2412)
    dev[0].dump_monitor()
    dev[0].request("SET wps_fragment_size 50")
    dev[0].request("WPS_PBC " + apdev[0]['bssid'])
    dev[0].wait_connected(timeout=30)
    status = dev[0].get_status()
    if status['wpa_state'] != 'COMPLETED':
        raise Exception("Not fully connected")
    if status['pairwise_cipher'] != 'CCMP' or status['group_cipher'] != 'TKIP':
        raise Exception("Unexpected encryption configuration")
    if status['key_mgmt'] != 'WPA2-PSK':
        raise Exception("Unexpected key_mgmt")

    logger.info("WPS provisioning step (PIN)")
    pin = dev[1].wps_read_pin()
    hapd.request("WPS_PIN any " + pin)
    dev[1].scan_for_bss(apdev[0]['bssid'], freq=2412)
    dev[1].request("SET wps_fragment_size 50")
    dev[1].request("WPS_PIN %s %s" % (apdev[0]['bssid'], pin))
    dev[1].wait_connected(timeout=30)
    status = dev[1].get_status()
    if status['wpa_state'] != 'COMPLETED':
        raise Exception("Not fully connected")
    if status['pairwise_cipher'] != 'CCMP' or status['group_cipher'] != 'TKIP':
        raise Exception("Unexpected encryption configuration")
    if status['key_mgmt'] != 'WPA2-PSK':
        raise Exception("Unexpected key_mgmt")

    logger.info("WPS connection as registrar")
    dev[2].scan_for_bss(apdev[0]['bssid'], freq=2412)
    dev[2].request("SET wps_fragment_size 50")
    dev[2].wps_reg(apdev[0]['bssid'], appin)
    status = dev[2].get_status()
    if status['wpa_state'] != 'COMPLETED':
        raise Exception("Not fully connected")
    if status['pairwise_cipher'] != 'CCMP' or status['group_cipher'] != 'TKIP':
        raise Exception("Unexpected encryption configuration")
    if status['key_mgmt'] != 'WPA2-PSK':
        raise Exception("Unexpected key_mgmt")

@remote_compatible
def test_ap_wps_new_version_sta(dev, apdev):
    """WPS compatibility with new version number on the station"""
    ssid = "test-wps-ver"
    hapd = hostapd.add_ap(apdev[0],
                          {"ssid": ssid, "eap_server": "1", "wps_state": "2",
                           "wpa_passphrase": "12345678", "wpa": "2",
                           "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP"})
    logger.info("WPS provisioning step")
    hapd.request("WPS_PBC")
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[0].dump_monitor()
    dev[0].request("SET wps_version_number 0x43")
    dev[0].request("SET wps_vendor_ext_m1 000137100100020001")
    dev[0].request("WPS_PBC " + apdev[0]['bssid'])
    dev[0].wait_connected(timeout=30)

@remote_compatible
def test_ap_wps_new_version_ap(dev, apdev):
    """WPS compatibility with new version number on the AP"""
    ssid = "test-wps-ver"
    hapd = hostapd.add_ap(apdev[0],
                          {"ssid": ssid, "eap_server": "1", "wps_state": "2",
                           "wpa_passphrase": "12345678", "wpa": "2",
                           "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP"})
    logger.info("WPS provisioning step")
    if "FAIL" in hapd.request("SET wps_version_number 0x43"):
        raise Exception("Failed to enable test functionality")
    hapd.request("WPS_PBC")
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[0].dump_monitor()
    dev[0].request("WPS_PBC " + apdev[0]['bssid'])
    dev[0].wait_connected(timeout=30)
    hapd.request("SET wps_version_number 0x20")

@remote_compatible
def test_ap_wps_check_pin(dev, apdev):
    """Verify PIN checking through control interface"""
    hapd = hostapd.add_ap(apdev[0],
                          {"ssid": "wps", "eap_server": "1", "wps_state": "2",
                           "wpa_passphrase": "12345678", "wpa": "2",
                           "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP"})
    for t in [("12345670", "12345670"),
              ("12345678", "FAIL-CHECKSUM"),
              ("12345", "FAIL"),
              ("123456789", "FAIL"),
              ("1234-5670", "12345670"),
              ("1234 5670", "12345670"),
              ("1-2.3:4 5670", "12345670")]:
        res = hapd.request("WPS_CHECK_PIN " + t[0]).rstrip('\n')
        res2 = dev[0].request("WPS_CHECK_PIN " + t[0]).rstrip('\n')
        if res != res2:
            raise Exception("Unexpected difference in WPS_CHECK_PIN responses")
        if res != t[1]:
            raise Exception("Incorrect WPS_CHECK_PIN response {} (expected {})".format(res, t[1]))

    if "FAIL" not in hapd.request("WPS_CHECK_PIN 12345"):
        raise Exception("Unexpected WPS_CHECK_PIN success")
    if "FAIL" not in hapd.request("WPS_CHECK_PIN 123456789"):
        raise Exception("Unexpected WPS_CHECK_PIN success")

    for i in range(0, 10):
        pin = dev[0].request("WPS_PIN get")
        rpin = dev[0].request("WPS_CHECK_PIN " + pin).rstrip('\n')
        if pin != rpin:
            raise Exception("Random PIN validation failed for " + pin)

def test_ap_wps_pin_get_failure(dev, apdev):
    """PIN generation failure"""
    with fail_test(dev[0], 1,
                   "os_get_random;wpa_supplicant_ctrl_iface_wps_pin"):
        if "FAIL" not in dev[0].request("WPS_PIN get"):
            raise Exception("WPS_PIN did not report failure")

def test_ap_wps_wep_config(dev, apdev):
    """WPS 2.0 AP rejecting WEP configuration"""
    ssid = "test-wps-config"
    appin = "12345670"
    hapd = hostapd.add_ap(apdev[0],
                          {"ssid": ssid, "eap_server": "1", "wps_state": "2",
                           "ap_pin": appin})
    dev[0].flush_scan_cache()
    dev[0].scan_for_bss(apdev[0]['bssid'], freq=2412)
    dev[0].wps_reg(apdev[0]['bssid'], appin, "wps-new-ssid-wep", "OPEN", "WEP",
                   "hello", no_wait=True)
    ev = hapd.wait_event(["WPS-FAIL"], timeout=15)
    if ev is None:
        raise Exception("WPS-FAIL timed out")
    if "reason=2" not in ev:
        raise Exception("Unexpected reason code in WPS-FAIL")
    status = hapd.request("WPS_GET_STATUS")
    if "Last WPS result: Failed" not in status:
        raise Exception("WPS failure result not shown correctly")
    if "Failure Reason: WEP Prohibited" not in status:
        raise Exception("Failure reason not reported correctly")
    if "Peer Address: " + dev[0].p2p_interface_addr() not in status:
        raise Exception("Peer address not shown correctly")

def test_ap_wps_wep_enroll(dev, apdev):
    """WPS 2.0 STA rejecting WEP configuration"""
    ssid = "test-wps-wep"
    params = {"ssid": ssid, "eap_server": "1", "wps_state": "2",
              "skip_cred_build": "1", "extra_cred": "wps-wep-cred"}
    hapd = hostapd.add_ap(apdev[0], params)
    hapd.request("WPS_PBC")
    dev[0].scan_for_bss(apdev[0]['bssid'], freq=2412)
    dev[0].request("WPS_PBC " + apdev[0]['bssid'])
    ev = dev[0].wait_event(["WPS-FAIL"], timeout=15)
    if ev is None:
        raise Exception("WPS-FAIL event timed out")
    if "msg=12" not in ev or "reason=2 (WEP Prohibited)" not in ev:
        raise Exception("Unexpected WPS-FAIL event: " + ev)

@remote_compatible
def test_ap_wps_ie_fragmentation(dev, apdev):
    """WPS AP using fragmented WPS IE"""
    ssid = "test-wps-ie-fragmentation"
    params = {"ssid": ssid, "eap_server": "1", "wps_state": "2",
              "wpa_passphrase": "12345678", "wpa": "2",
              "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP",
              "device_name": "1234567890abcdef1234567890abcdef",
              "manufacturer": "1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef",
              "model_name": "1234567890abcdef1234567890abcdef",
              "model_number": "1234567890abcdef1234567890abcdef",
              "serial_number": "1234567890abcdef1234567890abcdef"}
    hapd = hostapd.add_ap(apdev[0], params)
    hapd.request("WPS_PBC")
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[0].request("WPS_PBC " + apdev[0]['bssid'])
    dev[0].wait_connected(timeout=30)
    bss = dev[0].get_bss(apdev[0]['bssid'])
    if "wps_device_name" not in bss or bss['wps_device_name'] != "1234567890abcdef1234567890abcdef":
        logger.info("Device Name not received correctly")
        logger.info(bss)
        # This can fail if Probe Response frame is missed and Beacon frame was
        # used to fill in the BSS entry. This can happen, e.g., during heavy
        # load every now and then and is not really an error, so try to
        # workaround by runnign another scan.
        dev[0].scan(freq="2412", only_new=True)
        bss = dev[0].get_bss(apdev[0]['bssid'])
        if not bss or "wps_device_name" not in bss or bss['wps_device_name'] != "1234567890abcdef1234567890abcdef":
            logger.info(bss)
            raise Exception("Device Name not received correctly")
    if len(re.findall("dd..0050f204", bss['ie'])) != 2:
        raise Exception("Unexpected number of WPS IEs")

def get_psk(pskfile):
    psks = {}
    with open(pskfile, "r") as f:
        lines = f.read().splitlines()
        for l in lines:
            if l == "# WPA PSKs":
                continue
            vals = l.split(' ')
            if len(vals) != 3 or vals[0] != "wps=1":
                continue
            addr = vals[1]
            psk = vals[2]
            psks[addr] = psk
    return psks

def test_ap_wps_per_station_psk(dev, apdev):
    """WPS PBC provisioning with per-station PSK"""
    addr0 = dev[0].own_addr()
    addr1 = dev[1].own_addr()
    addr2 = dev[2].own_addr()
    ssid = "wps"
    appin = "12345670"
    pskfile = "/tmp/ap_wps_per_enrollee_psk.psk_file"
    try:
        os.remove(pskfile)
    except:
        pass

    hapd = None
    try:
        with open(pskfile, "w") as f:
            f.write("# WPA PSKs\n")

        params = {"ssid": ssid, "eap_server": "1", "wps_state": "2",
                  "wpa": "2", "wpa_key_mgmt": "WPA-PSK",
                  "rsn_pairwise": "CCMP", "ap_pin": appin,
                  "wpa_psk_file": pskfile}
        hapd = hostapd.add_ap(apdev[0], params)

        logger.info("First enrollee")
        hapd.request("WPS_PBC")
        dev[0].flush_scan_cache()
        dev[0].scan_for_bss(apdev[0]['bssid'], freq=2412)
        dev[0].request("WPS_PBC " + apdev[0]['bssid'])
        dev[0].wait_connected(timeout=30)

        logger.info("Second enrollee")
        hapd.request("WPS_PBC")
        dev[1].scan_for_bss(apdev[0]['bssid'], freq=2412)
        dev[1].request("WPS_PBC " + apdev[0]['bssid'])
        dev[1].wait_connected(timeout=30)

        logger.info("External registrar")
        dev[2].scan_for_bss(apdev[0]['bssid'], freq=2412)
        dev[2].wps_reg(apdev[0]['bssid'], appin)

        logger.info("Verifying PSK results")
        psks = get_psk(pskfile)
        if addr0 not in psks:
            raise Exception("No PSK recorded for sta0")
        if addr1 not in psks:
            raise Exception("No PSK recorded for sta1")
        if addr2 not in psks:
            raise Exception("No PSK recorded for sta2")
        if psks[addr0] == psks[addr1]:
            raise Exception("Same PSK recorded for sta0 and sta1")
        if psks[addr0] == psks[addr2]:
            raise Exception("Same PSK recorded for sta0 and sta2")
        if psks[addr1] == psks[addr2]:
            raise Exception("Same PSK recorded for sta1 and sta2")

        dev[0].request("REMOVE_NETWORK all")
        logger.info("Second external registrar")
        dev[0].scan_for_bss(apdev[0]['bssid'], freq=2412)
        dev[0].wps_reg(apdev[0]['bssid'], appin)
        psks2 = get_psk(pskfile)
        if addr0 not in psks2:
            raise Exception("No PSK recorded for sta0(reg)")
        if psks[addr0] == psks2[addr0]:
            raise Exception("Same PSK recorded for sta0(enrollee) and sta0(reg)")
    finally:
        os.remove(pskfile)
        if hapd:
            dev[0].request("DISCONNECT")
            dev[1].request("DISCONNECT")
            dev[2].request("DISCONNECT")
            hapd.disable()
            dev[0].flush_scan_cache()
            dev[1].flush_scan_cache()
            dev[2].flush_scan_cache()

def test_ap_wps_per_station_psk_preset(dev, apdev):
    """WPS PIN provisioning with per-station PSK preset"""
    addr0 = dev[0].own_addr()
    addr1 = dev[1].own_addr()
    addr2 = dev[2].own_addr()
    ssid = "wps"
    appin = "12345670"
    pskfile = "/tmp/ap_wps_per_enrollee_psk_preset.psk_file"
    try:
        os.remove(pskfile)
    except:
        pass

    hapd = None
    try:
        with open(pskfile, "w") as f:
            f.write("# WPA PSKs\n")
            f.write("wps=1 " + addr0 + " preset-passphrase-0\n")
            f.write("wps=1 " + addr2 + " preset-passphrase-2\n")

        params = {"ssid": ssid, "eap_server": "1", "wps_state": "2",
                  "wpa": "2", "wpa_key_mgmt": "WPA-PSK",
                  "rsn_pairwise": "CCMP", "ap_pin": appin,
                  "wpa_psk_file": pskfile}
        hapd = hostapd.add_ap(apdev[0], params)
        bssid = hapd.own_addr()

        logger.info("First enrollee")
        pin = dev[0].wps_read_pin()
        hapd.request("WPS_PIN any " + pin)
        dev[0].scan_for_bss(bssid, freq=2412)
        dev[0].request("WPS_PIN %s %s" % (bssid, pin))
        dev[0].wait_connected(timeout=30)

        logger.info("Second enrollee")
        pin = dev[1].wps_read_pin()
        hapd.request("WPS_PIN any " + pin)
        dev[1].scan_for_bss(bssid, freq=2412)
        dev[1].request("WPS_PIN %s %s" % (bssid, pin))
        dev[1].wait_connected(timeout=30)

        logger.info("External registrar")
        dev[2].scan_for_bss(bssid, freq=2412)
        dev[2].wps_reg(bssid, appin)

        logger.info("Verifying PSK results")
        psks = get_psk(pskfile)
        if addr0 not in psks:
            raise Exception("No PSK recorded for sta0")
        if addr1 not in psks:
            raise Exception("No PSK recorded for sta1")
        if addr2 not in psks:
            raise Exception("No PSK recorded for sta2")
        logger.info("PSK[0]: " + psks[addr0])
        logger.info("PSK[1]: " + psks[addr1])
        logger.info("PSK[2]: " + psks[addr2])
        if psks[addr0] == psks[addr1]:
            raise Exception("Same PSK recorded for sta0 and sta1")
        if psks[addr0] == psks[addr2]:
            raise Exception("Same PSK recorded for sta0 and sta2")
        if psks[addr1] == psks[addr2]:
            raise Exception("Same PSK recorded for sta1 and sta2")
        pmk0 = hapd.request("GET_PMK " + addr0)
        pmk1 = hapd.request("GET_PMK " + addr1)
        pmk2 = hapd.request("GET_PMK " + addr2)
        logger.info("PMK[0]: " + pmk0)
        logger.info("PMK[1]: " + pmk1)
        logger.info("PMK[2]: " + pmk2)
        if pmk0 != "565faec21ff04702d9d17c464e1301efd36c8a3ea46bb866b4bec7fed4384579":
            raise Exception("PSK[0] mismatch")
        if psks[addr1] != pmk1:
            raise Exception("PSK[1] mismatch")
        if psks[addr2] != pmk2:
            raise Exception("PSK[2] mismatch")

        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()
        dev[0].dump_monitor()
        logger.info("First enrollee again")
        pin = dev[0].wps_read_pin()
        hapd.request("WPS_PIN any " + pin)
        dev[0].scan_for_bss(bssid, freq=2412)
        dev[0].request("WPS_PIN %s %s" % (bssid, pin))
        dev[0].wait_connected(timeout=30)
        psks2 = get_psk(pskfile)
        if addr0 not in psks2:
            raise Exception("No PSK recorded for sta0 (2)")
        if psks[addr0] != psks2[addr0]:
            raise Exception("Different PSK recorded for sta0(enrollee) and sta0(enrollee 2)")
    finally:
        os.remove(pskfile)

def test_ap_wps_per_station_psk_failure(dev, apdev):
    """WPS PBC provisioning with per-station PSK (file not writable)"""
    addr0 = dev[0].p2p_dev_addr()
    addr1 = dev[1].p2p_dev_addr()
    addr2 = dev[2].p2p_dev_addr()
    ssid = "wps"
    appin = "12345670"
    pskfile = "/tmp/ap_wps_per_enrollee_psk.psk_file"
    try:
        os.remove(pskfile)
    except:
        pass

    hapd = None
    try:
        with open(pskfile, "w") as f:
            f.write("# WPA PSKs\n")

        params = {"ssid": ssid, "eap_server": "1", "wps_state": "2",
                  "wpa": "2", "wpa_key_mgmt": "WPA-PSK",
                  "rsn_pairwise": "CCMP", "ap_pin": appin,
                  "wpa_psk_file": pskfile}
        hapd = hostapd.add_ap(apdev[0], params)
        if "FAIL" in hapd.request("SET wpa_psk_file /tmp/does/not/exists/ap_wps_per_enrollee_psk_failure.psk_file"):
            raise Exception("Failed to set wpa_psk_file")

        logger.info("First enrollee")
        hapd.request("WPS_PBC")
        dev[0].scan_for_bss(apdev[0]['bssid'], freq=2412)
        dev[0].request("WPS_PBC " + apdev[0]['bssid'])
        dev[0].wait_connected(timeout=30)

        logger.info("Second enrollee")
        hapd.request("WPS_PBC")
        dev[1].scan_for_bss(apdev[0]['bssid'], freq=2412)
        dev[1].request("WPS_PBC " + apdev[0]['bssid'])
        dev[1].wait_connected(timeout=30)

        logger.info("External registrar")
        dev[2].scan_for_bss(apdev[0]['bssid'], freq=2412)
        dev[2].wps_reg(apdev[0]['bssid'], appin)

        logger.info("Verifying PSK results")
        psks = get_psk(pskfile)
        if len(psks) > 0:
            raise Exception("PSK recorded unexpectedly")
    finally:
        if hapd:
            for i in range(3):
                dev[i].request("DISCONNECT")
            hapd.disable()
            for i in range(3):
                dev[i].flush_scan_cache()
        os.remove(pskfile)

def test_ap_wps_pin_request_file(dev, apdev):
    """WPS PIN provisioning with configured AP"""
    ssid = "wps"
    pinfile = "/tmp/ap_wps_pin_request_file.log"
    if os.path.exists(pinfile):
        os.remove(pinfile)
    hapd = hostapd.add_ap(apdev[0],
                          {"ssid": ssid, "eap_server": "1", "wps_state": "2",
                           "wps_pin_requests": pinfile,
                           "wpa_passphrase": "12345678", "wpa": "2",
                           "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP"})
    uuid = dev[0].get_status_field("uuid")
    pin = dev[0].wps_read_pin()
    try:
        dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
        dev[0].request("WPS_PIN %s %s" % (apdev[0]['bssid'], pin))
        ev = hapd.wait_event(["WPS-PIN-NEEDED"], timeout=15)
        if ev is None:
            raise Exception("PIN needed event not shown")
        if uuid not in ev:
            raise Exception("UUID mismatch")
        dev[0].request("WPS_CANCEL")
        success = False
        with open(pinfile, "r") as f:
            lines = f.readlines()
            for l in lines:
                if uuid in l:
                    success = True
                    break
        if not success:
            raise Exception("PIN request entry not in the log file")
    finally:
        try:
            os.remove(pinfile)
        except:
            pass

def test_ap_wps_auto_setup_with_config_file(dev, apdev):
    """WPS auto-setup with configuration file"""
    skip_without_tkip(dev[0])
    conffile = "/tmp/ap_wps_auto_setup_with_config_file.conf"
    ifname = apdev[0]['ifname']
    try:
        with open(conffile, "w") as f:
            f.write("driver=nl80211\n")
            f.write("hw_mode=g\n")
            f.write("channel=1\n")
            f.write("ieee80211n=1\n")
            f.write("interface=%s\n" % ifname)
            f.write("ctrl_interface=/var/run/hostapd\n")
            f.write("ssid=wps\n")
            f.write("eap_server=1\n")
            f.write("wps_state=1\n")
        hapd = hostapd.add_bss(apdev[0], ifname, conffile)
        hapd.request("WPS_PBC")
        dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
        dev[0].request("WPS_PBC " + apdev[0]['bssid'])
        dev[0].wait_connected(timeout=30)
        with open(conffile, "r") as f:
            lines = f.read().splitlines()
            vals = dict()
            for l in lines:
                try:
                    [name, value] = l.split('=', 1)
                    vals[name] = value
                except ValueError as e:
                    if "# WPS configuration" in l:
                        pass
                    else:
                        raise Exception("Unexpected configuration line: " + l)
        if vals['ieee80211n'] != '1' or vals['wps_state'] != '2' or "WPA-PSK" not in vals['wpa_key_mgmt']:
            raise Exception("Incorrect configuration: " + str(vals))
    finally:
        try:
            os.remove(conffile)
        except:
            pass

@long_duration_test
def test_ap_wps_pbc_timeout(dev, apdev):
    """wpa_supplicant PBC walk time and WPS ER SelReg timeout"""
    ap_uuid = "27ea801a-9e5c-4e73-bd82-f89cbcd10d7e"
    hapd = add_ssdp_ap(apdev[0], ap_uuid)

    location = ssdp_get_location(ap_uuid)
    urls = upnp_get_urls(location)
    eventurl = urlparse(urls['event_sub_url'])
    ctrlurl = urlparse(urls['control_url'])

    url = urlparse(location)
    conn = HTTPConnection(url.netloc)

    class WPSERHTTPServer(StreamRequestHandler):
        def handle(self):
            data = self.rfile.readline().strip()
            logger.debug(data)
            self.wfile.write(gen_wps_event())

    server = MyTCPServer(("127.0.0.1", 12345), WPSERHTTPServer)
    server.timeout = 1

    headers = {"callback": '<http://127.0.0.1:12345/event>',
               "NT": "upnp:event",
               "timeout": "Second-1234"}
    conn.request("SUBSCRIBE", eventurl.path, "\r\n\r\n", headers)
    resp = conn.getresponse()
    if resp.status != 200:
        raise Exception("Unexpected HTTP response: %d" % resp.status)
    sid = resp.getheader("sid")
    logger.debug("Subscription SID " + sid)

    msg = '''<?xml version="1.0"?>
<s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/" s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">
<s:Body>
<u:SetSelectedRegistrar xmlns:u="urn:schemas-wifialliance-org:service:WFAWLANConfig:1">
<NewMessage>EEoAARAQQQABARASAAIAABBTAAIxSBBJAA4ANyoAASABBv///////xBIABA2LbR7pTpRkYj7
VFi5hrLk
</NewMessage>
</u:SetSelectedRegistrar>
</s:Body>
</s:Envelope>'''
    headers = {"Content-type": 'text/xml; charset="utf-8"'}
    headers["SOAPAction"] = '"urn:schemas-wifialliance-org:service:WFAWLANConfig:1#%s"' % "SetSelectedRegistrar"
    conn.request("POST", ctrlurl.path, msg, headers)
    resp = conn.getresponse()
    if resp.status != 200:
        raise Exception("Unexpected HTTP response: %d" % resp.status)

    server.handle_request()

    logger.info("Start WPS_PBC and wait for PBC walk time expiration")
    if "OK" not in dev[0].request("WPS_PBC"):
        raise Exception("WPS_PBC failed")

    start = os.times()[4]

    server.handle_request()
    dev[1].request("BSS_FLUSH 0")
    dev[1].scan_for_bss(apdev[0]['bssid'], freq="2412", force_scan=True,
                        only_new=True)
    bss = dev[1].get_bss(apdev[0]['bssid'])
    logger.debug("BSS: " + str(bss))
    if '[WPS-AUTH]' not in bss['flags']:
        raise Exception("WPS not indicated authorized")

    server.handle_request()

    wps_timeout_seen = False

    while True:
        hapd.dump_monitor()
        dev[1].dump_monitor()
        if not wps_timeout_seen:
            ev = dev[0].wait_event(["WPS-TIMEOUT"], timeout=0)
            if ev is not None:
                logger.info("PBC timeout seen")
                wps_timeout_seen = True
        else:
            dev[0].dump_monitor()
        now = os.times()[4]
        if now - start > 130:
            raise Exception("Selected registration information not removed")
        dev[1].request("BSS_FLUSH 0")
        dev[1].scan_for_bss(apdev[0]['bssid'], freq="2412", force_scan=True,
                            only_new=True)
        bss = dev[1].get_bss(apdev[0]['bssid'])
        logger.debug("BSS: " + str(bss))
        if '[WPS-AUTH]' not in bss['flags']:
            break
        server.handle_request()

    server.server_close()

    if wps_timeout_seen:
        return

    now = os.times()[4]
    if now < start + 150:
        dur = start + 150 - now
    else:
        dur = 1
    logger.info("Continue waiting for PBC timeout (%d sec)" % dur)
    ev = dev[0].wait_event(["WPS-TIMEOUT"], timeout=dur)
    if ev is None:
        raise Exception("WPS-TIMEOUT not reported")

def add_ssdp_ap(ap, ap_uuid):
    ssid = "wps-ssdp"
    ap_pin = "12345670"
    params = {"ssid": ssid, "eap_server": "1", "wps_state": "2",
              "wpa_passphrase": "12345678", "wpa": "2",
              "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP",
              "device_name": "Wireless AP", "manufacturer": "Company",
              "model_name": "WAP", "model_number": "123",
              "serial_number": "12345", "device_type": "6-0050F204-1",
              "os_version": "01020300",
              "config_methods": "label push_button",
              "ap_pin": ap_pin, "uuid": ap_uuid, "upnp_iface": "lo",
              "friendly_name": "WPS Access Point",
              "manufacturer_url": "http://www.example.com/",
              "model_description": "Wireless Access Point",
              "model_url": "http://www.example.com/model/",
              "upc": "123456789012"}
    return hostapd.add_ap(ap, params)

def ssdp_send(msg, no_recv=False):
    socket.setdefaulttimeout(1)
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 2)
    sock.bind(("127.0.0.1", 0))
    sock.sendto(msg.encode(), ("239.255.255.250", 1900))
    if no_recv:
        return None
    return sock.recv(1000).decode()

def ssdp_send_msearch(st, no_recv=False):
    msg = '\r\n'.join([
            'M-SEARCH * HTTP/1.1',
            'HOST: 239.255.255.250:1900',
            'MX: 1',
            'MAN: "ssdp:discover"',
            'ST: ' + st,
            '', ''])
    return ssdp_send(msg, no_recv=no_recv)

def test_ap_wps_ssdp_msearch(dev, apdev):
    """WPS AP and SSDP M-SEARCH messages"""
    ap_uuid = "27ea801a-9e5c-4e73-bd82-f89cbcd10d7e"
    add_ssdp_ap(apdev[0], ap_uuid)

    msg = '\r\n'.join([
            'M-SEARCH * HTTP/1.1',
            'Host: 239.255.255.250:1900',
            'Mx: 1',
            'Man: "ssdp:discover"',
            'St: urn:schemas-wifialliance-org:device:WFADevice:1',
            '', ''])
    ssdp_send(msg)

    msg = '\r\n'.join([
            'M-SEARCH * HTTP/1.1',
            'host:\t239.255.255.250:1900\t\t\t\t \t\t',
            'mx: \t1\t\t   ',
            'man: \t \t "ssdp:discover"   ',
            'st: urn:schemas-wifialliance-org:device:WFADevice:1\t\t',
            '', ''])
    ssdp_send(msg)

    ssdp_send_msearch("ssdp:all")
    ssdp_send_msearch("upnp:rootdevice")
    ssdp_send_msearch("uuid:" + ap_uuid)
    ssdp_send_msearch("urn:schemas-wifialliance-org:service:WFAWLANConfig:1")
    ssdp_send_msearch("urn:schemas-wifialliance-org:device:WFADevice:1")

    msg = '\r\n'.join([
            'M-SEARCH * HTTP/1.1',
            'HOST:\t239.255.255.250:1900',
            'MAN: "ssdp:discover"',
            'MX: 130',
            'ST: urn:schemas-wifialliance-org:device:WFADevice:1',
            '', ''])
    ssdp_send(msg, no_recv=True)

def test_ap_wps_ssdp_invalid_msearch(dev, apdev):
    """WPS AP and invalid SSDP M-SEARCH messages"""
    ap_uuid = "27ea801a-9e5c-4e73-bd82-f89cbcd10d7e"
    add_ssdp_ap(apdev[0], ap_uuid)

    socket.setdefaulttimeout(1)
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 2)
    sock.bind(("127.0.0.1", 0))

    logger.debug("Missing MX")
    msg = '\r\n'.join([
            'M-SEARCH * HTTP/1.1',
            'HOST: 239.255.255.250:1900',
            'MAN: "ssdp:discover"',
            'ST: urn:schemas-wifialliance-org:device:WFADevice:1',
            '', ''])
    sock.sendto(msg.encode(), ("239.255.255.250", 1900))

    logger.debug("Negative MX")
    msg = '\r\n'.join([
            'M-SEARCH * HTTP/1.1',
            'HOST: 239.255.255.250:1900',
            'MX: -1',
            'MAN: "ssdp:discover"',
            'ST: urn:schemas-wifialliance-org:device:WFADevice:1',
            '', ''])
    sock.sendto(msg.encode(), ("239.255.255.250", 1900))

    logger.debug("Invalid MX")
    msg = '\r\n'.join([
            'M-SEARCH * HTTP/1.1',
            'HOST: 239.255.255.250:1900',
            'MX; 1',
            'MAN: "ssdp:discover"',
            'ST: urn:schemas-wifialliance-org:device:WFADevice:1',
            '', ''])
    sock.sendto(msg.encode(), ("239.255.255.250", 1900))

    logger.debug("Missing MAN")
    msg = '\r\n'.join([
            'M-SEARCH * HTTP/1.1',
            'HOST: 239.255.255.250:1900',
            'MX: 1',
            'ST: urn:schemas-wifialliance-org:device:WFADevice:1',
            '', ''])
    sock.sendto(msg.encode(), ("239.255.255.250", 1900))

    logger.debug("Invalid MAN")
    msg = '\r\n'.join([
            'M-SEARCH * HTTP/1.1',
            'HOST: 239.255.255.250:1900',
            'MX: 1',
            'MAN: foo',
            'ST: urn:schemas-wifialliance-org:device:WFADevice:1',
            '', ''])
    sock.sendto(msg.encode(), ("239.255.255.250", 1900))
    msg = '\r\n'.join([
            'M-SEARCH * HTTP/1.1',
            'HOST: 239.255.255.250:1900',
            'MX: 1',
            'MAN; "ssdp:discover"',
            'ST: urn:schemas-wifialliance-org:device:WFADevice:1',
            '', ''])
    sock.sendto(msg.encode(), ("239.255.255.250", 1900))

    logger.debug("Missing HOST")
    msg = '\r\n'.join([
            'M-SEARCH * HTTP/1.1',
            'MAN: "ssdp:discover"',
            'MX: 1',
            'ST: urn:schemas-wifialliance-org:device:WFADevice:1',
            '', ''])
    sock.sendto(msg.encode(), ("239.255.255.250", 1900))

    logger.debug("Missing ST")
    msg = '\r\n'.join([
            'M-SEARCH * HTTP/1.1',
            'HOST: 239.255.255.250:1900',
            'MAN: "ssdp:discover"',
            'MX: 1',
            '', ''])
    sock.sendto(msg.encode(), ("239.255.255.250", 1900))

    logger.debug("Mismatching ST")
    msg = '\r\n'.join([
            'M-SEARCH * HTTP/1.1',
            'HOST: 239.255.255.250:1900',
            'MAN: "ssdp:discover"',
            'MX: 1',
            'ST: uuid:16d5f8a9-4ee4-4f5e-81f9-cc6e2f47f42d',
            '', ''])
    sock.sendto(msg.encode(), ("239.255.255.250", 1900))
    msg = '\r\n'.join([
            'M-SEARCH * HTTP/1.1',
            'HOST: 239.255.255.250:1900',
            'MAN: "ssdp:discover"',
            'MX: 1',
            'ST: foo:bar',
            '', ''])
    sock.sendto(msg.encode(), ("239.255.255.250", 1900))
    msg = '\r\n'.join([
            'M-SEARCH * HTTP/1.1',
            'HOST: 239.255.255.250:1900',
            'MAN: "ssdp:discover"',
            'MX: 1',
            'ST: foobar',
            '', ''])
    sock.sendto(msg.encode(), ("239.255.255.250", 1900))

    logger.debug("Invalid ST")
    msg = '\r\n'.join([
            'M-SEARCH * HTTP/1.1',
            'HOST: 239.255.255.250:1900',
            'MAN: "ssdp:discover"',
            'MX: 1',
            'ST; urn:schemas-wifialliance-org:device:WFADevice:1',
            '', ''])
    sock.sendto(msg.encode(), ("239.255.255.250", 1900))

    logger.debug("Invalid M-SEARCH")
    msg = '\r\n'.join([
            'M+SEARCH * HTTP/1.1',
            'HOST: 239.255.255.250:1900',
            'MAN: "ssdp:discover"',
            'MX: 1',
            'ST: urn:schemas-wifialliance-org:device:WFADevice:1',
            '', ''])
    sock.sendto(msg.encode(), ("239.255.255.250", 1900))
    msg = '\r\n'.join([
            'M-SEARCH-* HTTP/1.1',
            'HOST: 239.255.255.250:1900',
            'MAN: "ssdp:discover"',
            'MX: 1',
            'ST: urn:schemas-wifialliance-org:device:WFADevice:1',
            '', ''])
    sock.sendto(msg.encode(), ("239.255.255.250", 1900))

    logger.debug("Invalid message format")
    sock.sendto(b"NOTIFY * HTTP/1.1", ("239.255.255.250", 1900))
    msg = '\r'.join([
            'M-SEARCH * HTTP/1.1',
            'HOST: 239.255.255.250:1900',
            'MAN: "ssdp:discover"',
            'MX: 1',
            'ST: urn:schemas-wifialliance-org:device:WFADevice:1',
            '', ''])
    sock.sendto(msg.encode(), ("239.255.255.250", 1900))

    try:
        r = sock.recv(1000)
        raise Exception("Unexpected M-SEARCH response: " + r)
    except socket.timeout:
        pass

    logger.debug("Valid M-SEARCH")
    msg = '\r\n'.join([
            'M-SEARCH * HTTP/1.1',
            'HOST: 239.255.255.250:1900',
            'MAN: "ssdp:discover"',
            'MX: 1',
            'ST: urn:schemas-wifialliance-org:device:WFADevice:1',
            '', ''])
    sock.sendto(msg.encode(), ("239.255.255.250", 1900))

    try:
        r = sock.recv(1000)
        pass
    except socket.timeout:
        raise Exception("No SSDP response")

def test_ap_wps_ssdp_burst(dev, apdev):
    """WPS AP and SSDP burst"""
    ap_uuid = "27ea801a-9e5c-4e73-bd82-f89cbcd10d7e"
    add_ssdp_ap(apdev[0], ap_uuid)

    msg = '\r\n'.join([
            'M-SEARCH * HTTP/1.1',
            'HOST: 239.255.255.250:1900',
            'MAN: "ssdp:discover"',
            'MX: 1',
            'ST: urn:schemas-wifialliance-org:device:WFADevice:1',
            '', ''])
    socket.setdefaulttimeout(1)
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 2)
    sock.bind(("127.0.0.1", 0))
    for i in range(0, 25):
        sock.sendto(msg.encode(), ("239.255.255.250", 1900))
    resp = 0
    while True:
        try:
            r = sock.recv(1000).decode()
            if not r.startswith("HTTP/1.1 200 OK\r\n"):
                raise Exception("Unexpected message: " + r)
            resp += 1
        except socket.timeout:
            break
    if resp < 20:
        raise Exception("Too few SSDP responses")

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 2)
    sock.bind(("127.0.0.1", 0))
    for i in range(0, 25):
        sock.sendto(msg.encode(), ("239.255.255.250", 1900))
    while True:
        try:
            r = sock.recv(1000).decode()
            if ap_uuid in r:
                break
        except socket.timeout:
            raise Exception("No SSDP response")

def ssdp_get_location(uuid):
    res = ssdp_send_msearch("uuid:" + uuid)
    location = None
    for l in res.splitlines():
        if l.lower().startswith("location:"):
            location = l.split(':', 1)[1].strip()
            break
    if location is None:
        raise Exception("No UPnP location found")
    return location

def upnp_get_urls(location):
    if sys.version_info[0] > 2:
        conn = urlopen(location)
    else:
        conn = urlopen(location, proxies={})
    tree = ET.parse(conn)
    root = tree.getroot()
    urn = '{urn:schemas-upnp-org:device-1-0}'
    service = root.find("./" + urn + "device/" + urn + "serviceList/" + urn + "service")
    res = {}
    res['scpd_url'] = urljoin(location, service.find(urn + 'SCPDURL').text)
    res['control_url'] = urljoin(location,
                                 service.find(urn + 'controlURL').text)
    res['event_sub_url'] = urljoin(location,
                                   service.find(urn + 'eventSubURL').text)
    return res

def upnp_soap_action(conn, path, action, include_soap_action=True,
                     soap_action_override=None, newmsg=None, neweventtype=None,
                     neweventmac=None):
    soapns = 'http://schemas.xmlsoap.org/soap/envelope/'
    wpsns = 'urn:schemas-wifialliance-org:service:WFAWLANConfig:1'
    ET.register_namespace('soapenv', soapns)
    ET.register_namespace('wfa', wpsns)
    attrib = {}
    attrib['{%s}encodingStyle' % soapns] = 'http://schemas.xmlsoap.org/soap/encoding/'
    root = ET.Element("{%s}Envelope" % soapns, attrib=attrib)
    body = ET.SubElement(root, "{%s}Body" % soapns)
    act = ET.SubElement(body, "{%s}%s" % (wpsns, action))
    if newmsg:
        msg = ET.SubElement(act, "NewMessage")
        msg.text = base64.b64encode(newmsg.encode()).decode()
    if neweventtype:
        msg = ET.SubElement(act, "NewWLANEventType")
        msg.text = neweventtype
    if neweventmac:
        msg = ET.SubElement(act, "NewWLANEventMAC")
        msg.text = neweventmac

    headers = {"Content-type": 'text/xml; charset="utf-8"'}
    if include_soap_action:
        headers["SOAPAction"] = '"urn:schemas-wifialliance-org:service:WFAWLANConfig:1#%s"' % action
    elif soap_action_override:
        headers["SOAPAction"] = soap_action_override
    decl = b'<?xml version=\'1.0\' encoding=\'utf8\'?>\n'
    conn.request("POST", path, decl + ET.tostring(root), headers)
    return conn.getresponse()

def test_ap_wps_upnp(dev, apdev):
    """WPS AP and UPnP operations"""
    ap_uuid = "27ea801a-9e5c-4e73-bd82-f89cbcd10d7e"
    add_ssdp_ap(apdev[0], ap_uuid)

    location = ssdp_get_location(ap_uuid)
    urls = upnp_get_urls(location)

    if sys.version_info[0] > 2:
        conn = urlopen(urls['scpd_url'])
    else:
        conn = urlopen(urls['scpd_url'], proxies={})
    scpd = conn.read()

    if sys.version_info[0] > 2:
        try:
            conn = urlopen(urljoin(location, "unknown.html"))
            raise Exception("Unexpected HTTP response to GET unknown URL")
        except HTTPError as e:
            if e.code != 404:
                raise Exception("Unexpected HTTP response to GET unknown URL")
    else:
        conn = urlopen(urljoin(location, "unknown.html"), proxies={})
        if conn.getcode() != 404:
            raise Exception("Unexpected HTTP response to GET unknown URL")

    url = urlparse(location)
    conn = HTTPConnection(url.netloc)
    #conn.set_debuglevel(1)
    headers = {"Content-type": 'text/xml; charset="utf-8"',
               "SOAPAction": '"urn:schemas-wifialliance-org:service:WFAWLANConfig:1#GetDeviceInfo"'}
    conn.request("POST", "hello", "\r\n\r\n", headers)
    resp = conn.getresponse()
    if resp.status != 404:
        raise Exception("Unexpected HTTP response: %d" % resp.status)

    conn.request("UNKNOWN", "hello", "\r\n\r\n", headers)
    resp = conn.getresponse()
    if resp.status != 501:
        raise Exception("Unexpected HTTP response: %d" % resp.status)

    headers = {"Content-type": 'text/xml; charset="utf-8"',
               "SOAPAction": '"urn:some-unknown-action#GetDeviceInfo"'}
    ctrlurl = urlparse(urls['control_url'])
    conn.request("POST", ctrlurl.path, "\r\n\r\n", headers)
    resp = conn.getresponse()
    if resp.status != 401:
        raise Exception("Unexpected HTTP response: %d" % resp.status)

    logger.debug("GetDeviceInfo without SOAPAction header")
    resp = upnp_soap_action(conn, ctrlurl.path, "GetDeviceInfo",
                            include_soap_action=False)
    if resp.status != 401:
        raise Exception("Unexpected HTTP response: %d" % resp.status)

    logger.debug("GetDeviceInfo with invalid SOAPAction header")
    for act in ["foo",
                "urn:schemas-wifialliance-org:service:WFAWLANConfig:1#GetDeviceInfo",
                '"urn:schemas-wifialliance-org:service:WFAWLANConfig:1"',
                '"urn:schemas-wifialliance-org:service:WFAWLANConfig:123#GetDevice']:
        resp = upnp_soap_action(conn, ctrlurl.path, "GetDeviceInfo",
                                include_soap_action=False,
                                soap_action_override=act)
        if resp.status != 401:
            raise Exception("Unexpected HTTP response: %d" % resp.status)

    resp = upnp_soap_action(conn, ctrlurl.path, "GetDeviceInfo")
    if resp.status != 200:
        raise Exception("Unexpected HTTP response: %d" % resp.status)
    dev = resp.read().decode()
    if "NewDeviceInfo" not in dev:
        raise Exception("Unexpected GetDeviceInfo response")

    logger.debug("PutMessage without required parameters")
    resp = upnp_soap_action(conn, ctrlurl.path, "PutMessage")
    if resp.status != 600:
        raise Exception("Unexpected HTTP response: %d" % resp.status)

    logger.debug("PutWLANResponse without required parameters")
    resp = upnp_soap_action(conn, ctrlurl.path, "PutWLANResponse")
    if resp.status != 600:
        raise Exception("Unexpected HTTP response: %d" % resp.status)

    logger.debug("SetSelectedRegistrar from unregistered ER")
    resp = upnp_soap_action(conn, ctrlurl.path, "SetSelectedRegistrar")
    if resp.status != 501:
        raise Exception("Unexpected HTTP response: %d" % resp.status)

    logger.debug("Unknown action")
    resp = upnp_soap_action(conn, ctrlurl.path, "Unknown")
    if resp.status != 401:
        raise Exception("Unexpected HTTP response: %d" % resp.status)

def test_ap_wps_upnp_subscribe(dev, apdev):
    """WPS AP and UPnP event subscription"""
    ap_uuid = "27ea801a-9e5c-4e73-bd82-f89cbcd10d7e"
    hapd = add_ssdp_ap(apdev[0], ap_uuid)

    location = ssdp_get_location(ap_uuid)
    urls = upnp_get_urls(location)
    eventurl = urlparse(urls['event_sub_url'])

    url = urlparse(location)
    conn = HTTPConnection(url.netloc)
    #conn.set_debuglevel(1)
    headers = {"callback": '<http://127.0.0.1:12345/event>',
               "timeout": "Second-1234"}
    conn.request("SUBSCRIBE", "hello", "\r\n\r\n", headers)
    resp = conn.getresponse()
    if resp.status != 412:
        raise Exception("Unexpected HTTP response: %d" % resp.status)

    conn.request("SUBSCRIBE", eventurl.path, "\r\n\r\n", headers)
    resp = conn.getresponse()
    if resp.status != 412:
        raise Exception("Unexpected HTTP response: %d" % resp.status)

    headers = {"NT": "upnp:event",
               "timeout": "Second-1234"}
    conn.request("SUBSCRIBE", eventurl.path, "\r\n\r\n", headers)
    resp = conn.getresponse()
    if resp.status != 412:
        raise Exception("Unexpected HTTP response: %d" % resp.status)

    headers = {"callback": '<http://127.0.0.1:12345/event>',
               "NT": "upnp:foobar",
               "timeout": "Second-1234"}
    conn.request("SUBSCRIBE", eventurl.path, "\r\n\r\n", headers)
    resp = conn.getresponse()
    if resp.status != 400:
        raise Exception("Unexpected HTTP response: %d" % resp.status)

    logger.debug("Valid subscription")
    headers = {"callback": '<http://127.0.0.1:12345/event>',
               "NT": "upnp:event",
               "timeout": "Second-1234"}
    conn.request("SUBSCRIBE", eventurl.path, "\r\n\r\n", headers)
    resp = conn.getresponse()
    if resp.status != 200:
        raise Exception("Unexpected HTTP response: %d" % resp.status)
    sid = resp.getheader("sid")
    logger.debug("Subscription SID " + sid)

    logger.debug("Invalid re-subscription")
    headers = {"NT": "upnp:event",
               "sid": "123456734567854",
               "timeout": "Second-1234"}
    conn.request("SUBSCRIBE", eventurl.path, "\r\n\r\n", headers)
    resp = conn.getresponse()
    if resp.status != 400:
        raise Exception("Unexpected HTTP response: %d" % resp.status)

    logger.debug("Invalid re-subscription")
    headers = {"NT": "upnp:event",
               "sid": "uuid:123456734567854",
               "timeout": "Second-1234"}
    conn.request("SUBSCRIBE", eventurl.path, "\r\n\r\n", headers)
    resp = conn.getresponse()
    if resp.status != 400:
        raise Exception("Unexpected HTTP response: %d" % resp.status)

    logger.debug("Invalid re-subscription")
    headers = {"callback": '<http://127.0.0.1:12345/event>',
               "NT": "upnp:event",
               "sid": sid,
               "timeout": "Second-1234"}
    conn.request("SUBSCRIBE", eventurl.path, "\r\n\r\n", headers)
    resp = conn.getresponse()
    if resp.status != 400:
        raise Exception("Unexpected HTTP response: %d" % resp.status)

    logger.debug("SID mismatch in re-subscription")
    headers = {"NT": "upnp:event",
               "sid": "uuid:4c2bca79-1ff4-4e43-85d4-952a2b8a51fb",
               "timeout": "Second-1234"}
    conn.request("SUBSCRIBE", eventurl.path, "\r\n\r\n", headers)
    resp = conn.getresponse()
    if resp.status != 412:
        raise Exception("Unexpected HTTP response: %d" % resp.status)

    logger.debug("Valid re-subscription")
    headers = {"NT": "upnp:event",
               "sid": sid,
               "timeout": "Second-1234"}
    conn.request("SUBSCRIBE", eventurl.path, "\r\n\r\n", headers)
    resp = conn.getresponse()
    if resp.status != 200:
        raise Exception("Unexpected HTTP response: %d" % resp.status)
    sid2 = resp.getheader("sid")
    logger.debug("Subscription SID " + sid2)

    if sid != sid2:
        raise Exception("Unexpected SID change")

    logger.debug("Valid re-subscription")
    headers = {"NT": "upnp:event",
               "sid": "uuid: \t \t" + sid.split(':')[1],
               "timeout": "Second-1234"}
    conn.request("SUBSCRIBE", eventurl.path, "\r\n\r\n", headers)
    resp = conn.getresponse()
    if resp.status != 200:
        raise Exception("Unexpected HTTP response: %d" % resp.status)

    logger.debug("Invalid unsubscription")
    headers = {"sid": sid}
    conn.request("UNSUBSCRIBE", "/hello", "\r\n\r\n", headers)
    resp = conn.getresponse()
    if resp.status != 412:
        raise Exception("Unexpected HTTP response: %d" % resp.status)
    headers = {"foo": "bar"}
    conn.request("UNSUBSCRIBE", eventurl.path, "\r\n\r\n", headers)
    resp = conn.getresponse()
    if resp.status != 412:
        raise Exception("Unexpected HTTP response: %d" % resp.status)

    logger.debug("Valid unsubscription")
    headers = {"sid": sid}
    conn.request("UNSUBSCRIBE", eventurl.path, "\r\n\r\n", headers)
    resp = conn.getresponse()
    if resp.status != 200:
        raise Exception("Unexpected HTTP response: %d" % resp.status)

    logger.debug("Unsubscription for not existing SID")
    headers = {"sid": sid}
    conn.request("UNSUBSCRIBE", eventurl.path, "\r\n\r\n", headers)
    resp = conn.getresponse()
    if resp.status != 412:
        raise Exception("Unexpected HTTP response: %d" % resp.status)

    logger.debug("Invalid unsubscription")
    headers = {"sid": " \t \tfoo"}
    conn.request("UNSUBSCRIBE", eventurl.path, "\r\n\r\n", headers)
    resp = conn.getresponse()
    if resp.status != 400:
        raise Exception("Unexpected HTTP response: %d" % resp.status)

    logger.debug("Invalid unsubscription")
    headers = {"sid": "uuid:\t \tfoo"}
    conn.request("UNSUBSCRIBE", eventurl.path, "\r\n\r\n", headers)
    resp = conn.getresponse()
    if resp.status != 400:
        raise Exception("Unexpected HTTP response: %d" % resp.status)

    logger.debug("Invalid unsubscription")
    headers = {"NT": "upnp:event",
               "sid": sid}
    conn.request("UNSUBSCRIBE", eventurl.path, "\r\n\r\n", headers)
    resp = conn.getresponse()
    if resp.status != 400:
        raise Exception("Unexpected HTTP response: %d" % resp.status)
    headers = {"callback": '<http://127.0.0.1:12345/event>',
               "sid": sid}
    conn.request("UNSUBSCRIBE", eventurl.path, "\r\n\r\n", headers)
    resp = conn.getresponse()
    if resp.status != 400:
        raise Exception("Unexpected HTTP response: %d" % resp.status)

    logger.debug("Valid subscription with multiple callbacks")
    headers = {"callback": '<http://127.0.0.1:12345/event> <http://127.0.0.1:12345/event>\t<http://127.0.0.1:12345/event><http://127.0.0.1:12345/event><http://127.0.0.1:12345/event><http://127.0.0.1:12345/event><http://127.0.0.1:12345/event><http://127.0.0.1:12345/event><http://127.0.0.1:12345/event><http://127.0.0.1:12345/event><http://127.0.0.1:12345/event><http://127.0.0.1:12345/event><http://127.0.0.1:12345/event><http://127.0.0.1:12345/event><http://127.0.0.1:12345/event><http://127.0.0.1:12345/event><http://127.0.0.1:12345/event><http://127.0.0.1:12345/event><http://127.0.0.1:12345/event><http://127.0.0.1:12345/event><http://127.0.0.1:12345/event><http://127.0.0.1:12345/event><http://127.0.0.1:12345/event><http://127.0.0.1:12345/event>',
               "NT": "upnp:event",
               "timeout": "Second-1234"}
    conn.request("SUBSCRIBE", eventurl.path, "\r\n\r\n", headers)
    resp = conn.getresponse()
    if resp.status != 200:
        raise Exception("Unexpected HTTP response: %d" % resp.status)
    sid = resp.getheader("sid")
    logger.debug("Subscription SID " + sid)

    # Force subscription to be deleted due to errors
    dev[1].scan_for_bss(apdev[0]['bssid'], freq=2412)
    dev[2].scan_for_bss(apdev[0]['bssid'], freq=2412)
    with alloc_fail(hapd, 1, "event_build_message"):
        for i in range(10):
            dev[1].dump_monitor()
            dev[2].dump_monitor()
            dev[1].request("WPS_PIN " + apdev[0]['bssid'] + " 12345670")
            dev[2].request("WPS_PIN " + apdev[0]['bssid'] + " 12345670")
            dev[1].wait_event(["CTRL-EVENT-SCAN-RESULTS"], 5)
            dev[1].request("WPS_CANCEL")
            dev[2].wait_event(["CTRL-EVENT-SCAN-RESULTS"], 5)
            dev[2].request("WPS_CANCEL")
            if i % 4 == 1:
                time.sleep(1)
            else:
                time.sleep(0.1)
    time.sleep(0.2)

    headers = {"sid": sid}
    conn.request("UNSUBSCRIBE", eventurl.path, "", headers)
    resp = conn.getresponse()
    if resp.status != 200 and resp.status != 412:
        raise Exception("Unexpected HTTP response for UNSUBSCRIBE: %d" % resp.status)

    headers = {"callback": '<http://127.0.0.1:12345/event>',
               "NT": "upnp:event",
               "timeout": "Second-1234"}
    with alloc_fail(hapd, 1, "http_client_addr;event_send_start"):
        conn.request("SUBSCRIBE", eventurl.path, "\r\n\r\n", headers)
        resp = conn.getresponse()
        if resp.status != 200:
            raise Exception("Unexpected HTTP response for SUBSCRIBE: %d" % resp.status)
        sid = resp.getheader("sid")
        logger.debug("Subscription SID " + sid)

    headers = {"sid": sid}
    conn.request("UNSUBSCRIBE", eventurl.path, "\r\n\r\n", headers)
    resp = conn.getresponse()
    if resp.status != 200:
        raise Exception("Unexpected HTTP response for UNSUBSCRIBE: %d" % resp.status)

    headers = {"callback": '<http://127.0.0.1:12345/event>',
               "NT": "upnp:event",
               "timeout": "Second-1234"}
    conn.request("SUBSCRIBE", eventurl.path, "\r\n\r\n", headers)
    resp = conn.getresponse()
    if resp.status != 200:
        raise Exception("Unexpected HTTP response: %d" % resp.status)
    sid = resp.getheader("sid")
    logger.debug("Subscription SID " + sid)

    with alloc_fail(hapd, 1, "=wps_upnp_event_add"):
        for i in range(2):
            dev[1].dump_monitor()
            dev[2].dump_monitor()
            dev[1].request("WPS_PIN " + apdev[0]['bssid'] + " 12345670")
            dev[2].request("WPS_PIN " + apdev[0]['bssid'] + " 12345670")
            dev[1].wait_event(["CTRL-EVENT-SCAN-RESULTS"], 5)
            dev[1].request("WPS_CANCEL")
            dev[2].wait_event(["CTRL-EVENT-SCAN-RESULTS"], 5)
            dev[2].request("WPS_CANCEL")
            if i == 0:
                time.sleep(1)
            else:
                time.sleep(0.1)

    conn.request("SUBSCRIBE", eventurl.path, "\r\n\r\n", headers)
    resp = conn.getresponse()
    if resp.status != 200:
        raise Exception("Unexpected HTTP response: %d" % resp.status)

    with alloc_fail(hapd, 1, "wpabuf_dup;wps_upnp_event_add"):
        dev[1].dump_monitor()
        dev[2].dump_monitor()
        dev[1].request("WPS_PIN " + apdev[0]['bssid'] + " 12345670")
        dev[2].request("WPS_PIN " + apdev[0]['bssid'] + " 12345670")
        dev[1].wait_event(["CTRL-EVENT-SCAN-RESULTS"], 5)
        dev[1].request("WPS_CANCEL")
        dev[2].wait_event(["CTRL-EVENT-SCAN-RESULTS"], 5)
        dev[2].request("WPS_CANCEL")
        time.sleep(0.1)

    with fail_test(hapd, 1, "os_get_random;uuid_make;subscription_start"):
        conn.request("SUBSCRIBE", eventurl.path, "\r\n\r\n", headers)
        resp = conn.getresponse()
        if resp.status != 500:
            raise Exception("Unexpected HTTP response: %d" % resp.status)

    with alloc_fail(hapd, 1, "=subscription_start"):
        conn.request("SUBSCRIBE", eventurl.path, "\r\n\r\n", headers)
        resp = conn.getresponse()
        if resp.status != 500:
            raise Exception("Unexpected HTTP response: %d" % resp.status)

    headers = {"callback": '',
               "NT": "upnp:event",
               "timeout": "Second-1234"}
    conn.request("SUBSCRIBE", eventurl.path, "\r\n\r\n", headers)
    resp = conn.getresponse()
    if resp.status != 500:
        raise Exception("Unexpected HTTP response: %d" % resp.status)

    headers = {"callback": ' <',
               "NT": "upnp:event",
               "timeout": "Second-1234"}
    conn.request("SUBSCRIBE", eventurl.path, "\r\n\r\n", headers)
    resp = conn.getresponse()
    if resp.status != 500:
        raise Exception("Unexpected HTTP response: %d" % resp.status)

    headers = {"callback": '<http://127.0.0.1:12345/event>',
               "NT": "upnp:event",
               "timeout": "Second-1234"}
    with alloc_fail(hapd, 1, "wpabuf_alloc;subscription_first_event"):
        conn.request("SUBSCRIBE", eventurl.path, "\r\n\r\n", headers)
        resp = conn.getresponse()
        if resp.status != 500:
            raise Exception("Unexpected HTTP response: %d" % resp.status)

    with alloc_fail(hapd, 1, "wps_upnp_event_add;subscription_first_event"):
        conn.request("SUBSCRIBE", eventurl.path, "\r\n\r\n", headers)
        resp = conn.getresponse()
        if resp.status != 500:
            raise Exception("Unexpected HTTP response: %d" % resp.status)

    with alloc_fail(hapd, 1, "subscr_addr_add_url"):
        conn.request("SUBSCRIBE", eventurl.path, "\r\n\r\n", headers)
        resp = conn.getresponse()
        if resp.status != 500:
            raise Exception("Unexpected HTTP response: %d" % resp.status)

    with alloc_fail(hapd, 2, "subscr_addr_add_url"):
        conn.request("SUBSCRIBE", eventurl.path, "\r\n\r\n", headers)
        resp = conn.getresponse()
        if resp.status != 500:
            raise Exception("Unexpected HTTP response: %d" % resp.status)

    for i in range(6):
        headers = {"callback": '<http://127.0.0.1:%d/event>' % (12345 + i),
                   "NT": "upnp:event",
                   "timeout": "Second-1234"}
        conn.request("SUBSCRIBE", eventurl.path, "\r\n\r\n", headers)
        resp = conn.getresponse()
        if resp.status != 200:
            raise Exception("Unexpected HTTP response: %d" % resp.status)

    with alloc_fail(hapd, 1, "=upnp_wps_device_send_wlan_event"):
        dev[1].dump_monitor()
        dev[1].request("WPS_PIN " + apdev[0]['bssid'] + " 12345670")
        dev[1].wait_event(["CTRL-EVENT-SCAN-RESULTS"], 5)
        dev[1].request("WPS_CANCEL")
        time.sleep(0.1)

    with alloc_fail(hapd, 1, "wpabuf_alloc;upnp_wps_device_send_event"):
        dev[1].dump_monitor()
        dev[1].request("WPS_PIN " + apdev[0]['bssid'] + " 12345670")
        dev[1].wait_event(["CTRL-EVENT-SCAN-RESULTS"], 5)
        dev[1].request("WPS_CANCEL")
        time.sleep(0.1)

    with alloc_fail(hapd, 1,
                    "base64_gen_encode;?base64_encode;upnp_wps_device_send_wlan_event"):
        dev[1].dump_monitor()
        dev[1].request("WPS_PIN " + apdev[0]['bssid'] + " 12345670")
        dev[1].wait_event(["CTRL-EVENT-SCAN-RESULTS"], 5)
        dev[1].request("WPS_CANCEL")
        time.sleep(0.1)

    hapd.disable()
    with alloc_fail(hapd, 1, "get_netif_info"):
        if "FAIL" not in hapd.request("ENABLE"):
            raise Exception("ENABLE succeeded during OOM")

def test_ap_wps_upnp_subscribe_events(dev, apdev):
    """WPS AP and UPnP event subscription and many events"""
    ap_uuid = "27ea801a-9e5c-4e73-bd82-f89cbcd10d7e"
    hapd = add_ssdp_ap(apdev[0], ap_uuid)

    location = ssdp_get_location(ap_uuid)
    urls = upnp_get_urls(location)
    eventurl = urlparse(urls['event_sub_url'])

    class WPSERHTTPServer(StreamRequestHandler):
        def handle(self):
            data = self.rfile.readline().strip()
            logger.debug(data)
            self.wfile.write(gen_wps_event())

    server = MyTCPServer(("127.0.0.1", 12345), WPSERHTTPServer)
    server.timeout = 1

    url = urlparse(location)
    conn = HTTPConnection(url.netloc)

    headers = {"callback": '<http://127.0.0.1:12345/event>',
               "NT": "upnp:event",
               "timeout": "Second-1234"}
    conn.request("SUBSCRIBE", eventurl.path, "\r\n\r\n", headers)
    resp = conn.getresponse()
    if resp.status != 200:
        raise Exception("Unexpected HTTP response: %d" % resp.status)
    sid = resp.getheader("sid")
    logger.debug("Subscription SID " + sid)

    # Fetch the first event message
    server.handle_request()

    # Force subscription event queue to reach the maximum length by generating
    # new proxied events without the ER fetching any of the pending events.
    dev[1].scan_for_bss(apdev[0]['bssid'], freq=2412)
    dev[2].scan_for_bss(apdev[0]['bssid'], freq=2412)
    for i in range(16):
        dev[1].dump_monitor()
        dev[2].dump_monitor()
        dev[1].request("WPS_PIN " + apdev[0]['bssid'] + " 12345670")
        dev[2].request("WPS_PIN " + apdev[0]['bssid'] + " 12345670")
        dev[1].wait_event(["CTRL-EVENT-SCAN-RESULTS"], 5)
        dev[1].request("WPS_CANCEL")
        dev[2].wait_event(["CTRL-EVENT-SCAN-RESULTS"], 5)
        dev[2].request("WPS_CANCEL")
        if i % 4 == 1:
            time.sleep(1)
        else:
            time.sleep(0.1)

    hapd.request("WPS_PIN any 12345670")
    dev[1].dump_monitor()
    dev[1].request("WPS_PIN " + apdev[0]['bssid'] + " 12345670")
    ev = dev[1].wait_event(["WPS-SUCCESS"], timeout=10)
    if ev is None:
        raise Exception("WPS success not reported")

    # Close the WPS ER HTTP server without fetching all the pending events.
    # This tests hostapd code path that clears subscription and the remaining
    # event queue when the interface is deinitialized.
    server.handle_request()
    server.server_close()

    dev[1].wait_connected()

def test_ap_wps_upnp_http_proto(dev, apdev):
    """WPS AP and UPnP/HTTP protocol testing"""
    ap_uuid = "27ea801a-9e5c-4e73-bd82-f89cbcd10d7e"
    add_ssdp_ap(apdev[0], ap_uuid)

    location = ssdp_get_location(ap_uuid)

    url = urlparse(location)
    conn = HTTPConnection(url.netloc, timeout=0.2)
    #conn.set_debuglevel(1)

    conn.request("HEAD", "hello")
    resp = conn.getresponse()
    if resp.status != 501:
        raise Exception("Unexpected response to HEAD: " + str(resp.status))
    conn.close()

    for cmd in ["PUT", "DELETE", "TRACE", "CONNECT", "M-SEARCH", "M-POST"]:
        try:
            conn.request(cmd, "hello")
            resp = conn.getresponse()
        except Exception as e:
            pass
        conn.close()

    headers = {"Content-Length": 'abc'}
    conn.request("HEAD", "hello", "\r\n\r\n", headers)
    try:
        resp = conn.getresponse()
    except Exception as e:
        pass
    conn.close()

    headers = {"Content-Length": '-10'}
    conn.request("HEAD", "hello", "\r\n\r\n", headers)
    try:
        resp = conn.getresponse()
    except Exception as e:
        pass
    conn.close()

    headers = {"Content-Length": '10000000000000'}
    conn.request("HEAD", "hello", "\r\n\r\nhello", headers)
    try:
        resp = conn.getresponse()
    except Exception as e:
        pass
    conn.close()

    headers = {"Transfer-Encoding": 'abc'}
    conn.request("HEAD", "hello", "\r\n\r\n", headers)
    resp = conn.getresponse()
    if resp.status != 501:
        raise Exception("Unexpected response to HEAD: " + str(resp.status))
    conn.close()

    headers = {"Transfer-Encoding": 'chunked'}
    conn.request("HEAD", "hello", "\r\n\r\n", headers)
    resp = conn.getresponse()
    if resp.status != 501:
        raise Exception("Unexpected response to HEAD: " + str(resp.status))
    conn.close()

    # Too long a header
    conn.request("HEAD", 5000 * 'A')
    try:
        resp = conn.getresponse()
    except Exception as e:
        pass
    conn.close()

    # Long URL but within header length limits
    conn.request("HEAD", 3000 * 'A')
    resp = conn.getresponse()
    if resp.status != 501:
        raise Exception("Unexpected response to HEAD: " + str(resp.status))
    conn.close()

    headers = {"Content-Length": '20'}
    conn.request("POST", "hello", 10 * 'A' + "\r\n\r\n", headers)
    try:
        resp = conn.getresponse()
    except Exception as e:
        pass
    conn.close()

    conn.request("POST", "hello", 5000 * 'A' + "\r\n\r\n")
    resp = conn.getresponse()
    if resp.status != 404:
        raise Exception("Unexpected HTTP response: %d" % resp.status)
    conn.close()

    conn.request("POST", "hello", 60000 * 'A' + "\r\n\r\n")
    try:
        resp = conn.getresponse()
    except Exception as e:
        pass
    conn.close()

def test_ap_wps_upnp_http_proto_chunked(dev, apdev):
    """WPS AP and UPnP/HTTP protocol testing for chunked encoding"""
    ap_uuid = "27ea801a-9e5c-4e73-bd82-f89cbcd10d7e"
    add_ssdp_ap(apdev[0], ap_uuid)

    location = ssdp_get_location(ap_uuid)

    url = urlparse(location)
    conn = HTTPConnection(url.netloc)
    #conn.set_debuglevel(1)

    headers = {"Transfer-Encoding": 'chunked'}
    conn.request("POST", "hello",
                 "a\r\nabcdefghij\r\n" + "2\r\nkl\r\n" + "0\r\n\r\n",
                 headers)
    resp = conn.getresponse()
    if resp.status != 404:
        raise Exception("Unexpected HTTP response: %d" % resp.status)
    conn.close()

    conn.putrequest("POST", "hello")
    conn.putheader('Transfer-Encoding', 'chunked')
    conn.endheaders()
    conn.send(b"a\r\nabcdefghij\r\n")
    time.sleep(0.1)
    conn.send(b"2\r\nkl\r\n")
    conn.send(b"0\r\n\r\n")
    resp = conn.getresponse()
    if resp.status != 404:
        raise Exception("Unexpected HTTP response: %d" % resp.status)
    conn.close()

    conn.putrequest("POST", "hello")
    conn.putheader('Transfer-Encoding', 'chunked')
    conn.endheaders()
    completed = False
    try:
        for i in range(20000):
            conn.send(b"1\r\nZ\r\n")
        conn.send(b"0\r\n\r\n")
        resp = conn.getresponse()
        completed = True
    except Exception as e:
        pass
    conn.close()
    if completed:
        raise Exception("Too long chunked request did not result in connection reset")

    headers = {"Transfer-Encoding": 'chunked'}
    conn.request("POST", "hello", "80000000\r\na", headers)
    try:
        resp = conn.getresponse()
    except Exception as e:
        pass
    conn.close()

    conn.request("POST", "hello", "10000000\r\na", headers)
    try:
        resp = conn.getresponse()
    except Exception as e:
        pass
    conn.close()

@remote_compatible
def test_ap_wps_disabled(dev, apdev):
    """WPS operations while WPS is disabled"""
    ssid = "test-wps-disabled"
    hapd = hostapd.add_ap(apdev[0], {"ssid": ssid})
    if "FAIL" not in hapd.request("WPS_PBC"):
        raise Exception("WPS_PBC succeeded unexpectedly")
    if "FAIL" not in hapd.request("WPS_CANCEL"):
        raise Exception("WPS_CANCEL succeeded unexpectedly")

def test_ap_wps_mixed_cred(dev, apdev):
    """WPS 2.0 STA merging mixed mode WPA/WPA2 credentials"""
    skip_without_tkip(dev[0])
    ssid = "test-wps-wep"
    params = {"ssid": ssid, "eap_server": "1", "wps_state": "2",
              "skip_cred_build": "1", "extra_cred": "wps-mixed-cred"}
    hapd = hostapd.add_ap(apdev[0], params)
    hapd.request("WPS_PBC")
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[0].request("WPS_PBC " + apdev[0]['bssid'])
    ev = dev[0].wait_event(["WPS-SUCCESS"], timeout=30)
    if ev is None:
        raise Exception("WPS-SUCCESS event timed out")
    nets = dev[0].list_networks()
    if len(nets) != 1:
        raise Exception("Unexpected number of network blocks")
    id = nets[0]['id']
    proto = dev[0].get_network(id, "proto")
    if proto != "WPA RSN":
        raise Exception("Unexpected merged proto field value: " + proto)
    pairwise = dev[0].get_network(id, "pairwise")
    p = pairwise.split()
    if "CCMP" not in p or "TKIP" not in p:
        raise Exception("Unexpected merged pairwise field value: " + pairwise)

@remote_compatible
def test_ap_wps_while_connected(dev, apdev):
    """WPS PBC provisioning while connected to another AP"""
    ssid = "test-wps-conf"
    hapd = hostapd.add_ap(apdev[0],
                          {"ssid": ssid, "eap_server": "1", "wps_state": "2",
                           "wpa_passphrase": "12345678", "wpa": "2",
                           "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP"})

    hostapd.add_ap(apdev[1], {"ssid": "open"})
    dev[0].connect("open", key_mgmt="NONE", scan_freq="2412")

    logger.info("WPS provisioning step")
    hapd.request("WPS_PBC")
    dev[0].dump_monitor()
    dev[0].request("WPS_PBC " + apdev[0]['bssid'])
    dev[0].wait_connected(timeout=30)
    status = dev[0].get_status()
    if status['bssid'] != apdev[0]['bssid']:
        raise Exception("Unexpected BSSID")

@remote_compatible
def test_ap_wps_while_connected_no_autoconnect(dev, apdev):
    """WPS PBC provisioning while connected to another AP and STA_AUTOCONNECT disabled"""
    ssid = "test-wps-conf"
    hapd = hostapd.add_ap(apdev[0],
                          {"ssid": ssid, "eap_server": "1", "wps_state": "2",
                           "wpa_passphrase": "12345678", "wpa": "2",
                           "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP"})

    hostapd.add_ap(apdev[1], {"ssid": "open"})

    try:
        dev[0].request("STA_AUTOCONNECT 0")
        dev[0].connect("open", key_mgmt="NONE", scan_freq="2412")

        logger.info("WPS provisioning step")
        hapd.request("WPS_PBC")
        dev[0].dump_monitor()
        dev[0].request("WPS_PBC " + apdev[0]['bssid'])
        dev[0].wait_connected(timeout=30)
        status = dev[0].get_status()
        if status['bssid'] != apdev[0]['bssid']:
            raise Exception("Unexpected BSSID")
    finally:
        dev[0].request("STA_AUTOCONNECT 1")

@remote_compatible
def test_ap_wps_from_event(dev, apdev):
    """WPS PBC event on AP to enable PBC"""
    ssid = "test-wps-conf"
    hapd = hostapd.add_ap(apdev[0],
                          {"ssid": ssid, "eap_server": "1", "wps_state": "2",
                           "wpa_passphrase": "12345678", "wpa": "2",
                           "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP"})
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[0].dump_monitor()
    hapd.dump_monitor()
    dev[0].request("WPS_PBC " + apdev[0]['bssid'])

    ev = hapd.wait_event(['WPS-ENROLLEE-SEEN'], timeout=15)
    if ev is None:
        raise Exception("No WPS-ENROLLEE-SEEN event on AP")
    vals = ev.split(' ')
    if vals[1] != dev[0].p2p_interface_addr():
        raise Exception("Unexpected enrollee address: " + vals[1])
    if vals[5] != '4':
        raise Exception("Unexpected Device Password Id: " + vals[5])
    hapd.request("WPS_PBC")
    dev[0].wait_connected(timeout=30)

def test_ap_wps_ap_scan_2(dev, apdev):
    """AP_SCAN 2 for WPS"""
    ssid = "test-wps-conf"
    hapd = hostapd.add_ap(apdev[0],
                          {"ssid": ssid, "eap_server": "1", "wps_state": "2",
                           "wpa_passphrase": "12345678", "wpa": "2",
                           "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP"})
    hapd.request("WPS_PBC")

    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    wpas.interface_add("wlan5", drv_params="force_connect_cmd=1")
    wpas.dump_monitor()

    if "OK" not in wpas.request("AP_SCAN 2"):
        raise Exception("Failed to set AP_SCAN 2")

    wpas.flush_scan_cache()
    wpas.scan_for_bss(apdev[0]['bssid'], freq="2412")
    wpas.dump_monitor()
    wpas.request("WPS_PBC " + apdev[0]['bssid'])
    ev = wpas.wait_event(["WPS-SUCCESS"], timeout=15)
    if ev is None:
        raise Exception("WPS-SUCCESS event timed out")
    wpas.wait_connected(timeout=30)
    wpas.dump_monitor()
    wpas.request("DISCONNECT")
    wpas.wait_disconnected()
    id = wpas.list_networks()[0]['id']
    pairwise = wpas.get_network(id, "pairwise")
    if "CCMP" not in pairwise.split():
        raise Exception("Unexpected pairwise parameter value: " + pairwise)
    group = wpas.get_network(id, "group")
    if "CCMP" not in group.split():
        raise Exception("Unexpected group parameter value: " + group)
    # Need to select a single cipher for ap_scan=2 testing
    wpas.set_network(id, "pairwise", "CCMP")
    wpas.set_network(id, "group", "CCMP")
    wpas.request("BSS_FLUSH 0")
    wpas.dump_monitor()
    wpas.request("REASSOCIATE")
    wpas.wait_connected(timeout=30)
    wpas.dump_monitor()
    wpas.request("DISCONNECT")
    wpas.wait_disconnected()
    wpas.flush_scan_cache()

@remote_compatible
def test_ap_wps_eapol_workaround(dev, apdev):
    """EAPOL workaround code path for 802.1X header length mismatch"""
    ssid = "test-wps"
    hapd = hostapd.add_ap(apdev[0],
                          {"ssid": ssid, "eap_server": "1", "wps_state": "1"})
    bssid = apdev[0]['bssid']
    hapd.request("SET ext_eapol_frame_io 1")
    dev[0].request("SET ext_eapol_frame_io 1")
    hapd.request("WPS_PBC")
    dev[0].request("WPS_PBC")

    ev = hapd.wait_event(["EAPOL-TX"], timeout=15)
    if ev is None:
        raise Exception("Timeout on EAPOL-TX from hostapd")

    res = dev[0].request("EAPOL_RX " + bssid + " 020000040193000501FFFF")
    if "OK" not in res:
        raise Exception("EAPOL_RX to wpa_supplicant failed")

def test_ap_wps_iteration(dev, apdev):
    """WPS PIN and iterate through APs without selected registrar"""
    ssid = "test-wps-conf"
    hapd = hostapd.add_ap(apdev[0],
                          {"ssid": ssid, "eap_server": "1", "wps_state": "2",
                           "wpa_passphrase": "12345678", "wpa": "2",
                           "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP"})

    ssid2 = "test-wps-conf2"
    hapd2 = hostapd.add_ap(apdev[1],
                           {"ssid": ssid2, "eap_server": "1", "wps_state": "2",
                            "wpa_passphrase": "12345678", "wpa": "2",
                            "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP"})

    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[0].scan_for_bss(apdev[1]['bssid'], freq="2412")
    dev[0].dump_monitor()
    pin = dev[0].request("WPS_PIN any")

    # Wait for iteration through all WPS APs to happen before enabling any
    # Registrar.
    for i in range(2):
        ev = dev[0].wait_event(["Associated with"], timeout=30)
        if ev is None:
            raise Exception("No association seen")
        ev = dev[0].wait_event(["WPS-M2D"], timeout=10)
        if ev is None:
            raise Exception("No M2D from AP")
        dev[0].wait_disconnected()

    # Verify that each AP requested PIN
    ev = hapd.wait_event(["WPS-PIN-NEEDED"], timeout=1)
    if ev is None:
        raise Exception("No WPS-PIN-NEEDED event from AP")
    ev = hapd2.wait_event(["WPS-PIN-NEEDED"], timeout=1)
    if ev is None:
        raise Exception("No WPS-PIN-NEEDED event from AP2")

    # Provide PIN to one of the APs and verify that connection gets formed
    hapd.request("WPS_PIN any " + pin)
    dev[0].wait_connected(timeout=30)

def test_ap_wps_iteration_error(dev, apdev):
    """WPS AP iteration on no Selected Registrar and error case with an AP"""
    ssid = "test-wps-conf-pin"
    hapd = hostapd.add_ap(apdev[0],
                          {"ssid": ssid, "eap_server": "1", "wps_state": "2",
                           "wpa_passphrase": "12345678", "wpa": "2",
                           "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP",
                           "wps_independent": "1"})
    hapd.request("SET ext_eapol_frame_io 1")
    bssid = apdev[0]['bssid']
    pin = dev[0].wps_read_pin()
    dev[0].request("WPS_PIN any " + pin)

    ev = hapd.wait_event(["EAPOL-TX"], timeout=15)
    if ev is None:
        raise Exception("No EAPOL-TX (EAP-Request/Identity) from hostapd")
    dev[0].request("EAPOL_RX " + bssid + " " + ev.split(' ')[2])

    ev = hapd.wait_event(["EAPOL-TX"], timeout=15)
    if ev is None:
        raise Exception("No EAPOL-TX (EAP-WSC/Start) from hostapd")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED"], timeout=5)
    if ev is None:
        raise Exception("No CTRL-EVENT-EAP-STARTED")

    # Do not forward any more EAPOL frames to test wpa_supplicant behavior for
    # a case with an incorrectly behaving WPS AP.

    # Start the real target AP and activate registrar on it.
    hapd2 = hostapd.add_ap(apdev[1],
                          {"ssid": ssid, "eap_server": "1", "wps_state": "2",
                           "wpa_passphrase": "12345678", "wpa": "2",
                           "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP",
                           "wps_independent": "1"})
    hapd2.request("WPS_PIN any " + pin)

    dev[0].wait_disconnected(timeout=15)
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED"], timeout=15)
    if ev is None:
        raise Exception("No CTRL-EVENT-EAP-STARTED for the second AP")
    ev = dev[0].wait_event(["WPS-CRED-RECEIVED"], timeout=15)
    if ev is None:
        raise Exception("No WPS-CRED-RECEIVED for the second AP")
    dev[0].wait_connected(timeout=15)

@remote_compatible
def test_ap_wps_priority(dev, apdev):
    """WPS PIN provisioning with configured AP and wps_priority"""
    ssid = "test-wps-conf-pin"
    hapd = hostapd.add_ap(apdev[0],
                          {"ssid": ssid, "eap_server": "1", "wps_state": "2",
                           "wpa_passphrase": "12345678", "wpa": "2",
                           "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP"})
    logger.info("WPS provisioning step")
    pin = dev[0].wps_read_pin()
    hapd.request("WPS_PIN any " + pin)
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[0].dump_monitor()
    try:
        dev[0].request("SET wps_priority 6")
        dev[0].request("WPS_PIN %s %s" % (apdev[0]['bssid'], pin))
        dev[0].wait_connected(timeout=30)
        netw = dev[0].list_networks()
        prio = dev[0].get_network(netw[0]['id'], 'priority')
        if prio != '6':
            raise Exception("Unexpected network priority: " + prio)
    finally:
        dev[0].request("SET wps_priority 0")

@remote_compatible
def test_ap_wps_and_non_wps(dev, apdev):
    """WPS and non-WPS AP in single hostapd process"""
    params = {"ssid": "wps", "eap_server": "1", "wps_state": "1"}
    hapd = hostapd.add_ap(apdev[0], params)

    params = {"ssid": "no wps"}
    hapd2 = hostapd.add_ap(apdev[1], params)

    appin = hapd.request("WPS_AP_PIN random")
    if "FAIL" in appin:
        raise Exception("Could not generate random AP PIN")
    if appin not in hapd.request("WPS_AP_PIN get"):
        raise Exception("Could not fetch current AP PIN")

    if "FAIL" in hapd.request("WPS_PBC"):
        raise Exception("WPS_PBC failed")
    if "FAIL" in hapd.request("WPS_CANCEL"):
        raise Exception("WPS_CANCEL failed")

def test_ap_wps_init_oom(dev, apdev):
    """Initial AP configuration and OOM during PSK generation"""
    ssid = "test-wps"
    params = {"ssid": ssid, "eap_server": "1", "wps_state": "1"}
    hapd = hostapd.add_ap(apdev[0], params)

    with alloc_fail(hapd, 1, "base64_gen_encode;?base64_encode;wps_build_cred"):
        pin = dev[0].wps_read_pin()
        hapd.request("WPS_PIN any " + pin)
        dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
        dev[0].request("WPS_PIN %s %s" % (apdev[0]['bssid'], pin))
        dev[0].wait_disconnected()

    hapd.request("WPS_PIN any " + pin)
    dev[0].wait_connected(timeout=30)

@remote_compatible
def test_ap_wps_er_oom(dev, apdev):
    """WPS ER OOM in XML processing"""
    try:
        _test_ap_wps_er_oom(dev, apdev)
    finally:
        dev[0].request("WPS_ER_STOP")
        dev[1].request("WPS_CANCEL")
        dev[0].request("DISCONNECT")

def _test_ap_wps_er_oom(dev, apdev):
    ssid = "wps-er-ap-config"
    ap_pin = "12345670"
    ap_uuid = "27ea801a-9e5c-4e73-bd82-f89cbcd10d7e"
    hostapd.add_ap(apdev[0],
                   {"ssid": ssid, "eap_server": "1", "wps_state": "2",
                    "wpa_passphrase": "12345678", "wpa": "2",
                    "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP",
                    "device_name": "Wireless AP", "manufacturer": "Company",
                    "model_name": "WAP", "model_number": "123",
                    "serial_number": "12345", "device_type": "6-0050F204-1",
                    "os_version": "01020300",
                    "config_methods": "label push_button",
                    "ap_pin": ap_pin, "uuid": ap_uuid, "upnp_iface": "lo"})

    dev[0].connect(ssid, psk="12345678", scan_freq="2412")

    with alloc_fail(dev[0], 1,
                    "base64_gen_decode;?base64_decode;xml_get_base64_item"):
        dev[0].request("WPS_ER_START ifname=lo")
        ev = dev[0].wait_event(["WPS-ER-AP-ADD"], timeout=3)
        if ev is not None:
            raise Exception("Unexpected AP discovery")

    dev[0].request("WPS_ER_STOP")
    dev[0].request("WPS_ER_START ifname=lo")
    ev = dev[0].wait_event(["WPS-ER-AP-ADD"], timeout=10)
    if ev is None:
        raise Exception("AP discovery timed out")

    dev[1].scan_for_bss(apdev[0]['bssid'], freq=2412)
    with alloc_fail(dev[0], 1,
                    "base64_gen_decode;?base64_decode;xml_get_base64_item"):
        dev[1].request("WPS_PBC " + apdev[0]['bssid'])
        ev = dev[1].wait_event(["CTRL-EVENT-SCAN-RESULTS"], timeout=10)
        if ev is None:
            raise Exception("PBC scan failed")
        ev = dev[0].wait_event(["WPS-ER-ENROLLEE-ADD"], timeout=15)
        if ev is None:
            raise Exception("Enrollee discovery timed out")

@remote_compatible
def test_ap_wps_er_init_oom(dev, apdev):
    """WPS ER and OOM during init"""
    try:
        _test_ap_wps_er_init_oom(dev, apdev)
    finally:
        dev[0].request("WPS_ER_STOP")

def _test_ap_wps_er_init_oom(dev, apdev):
    with alloc_fail(dev[0], 1, "wps_er_init"):
        if "FAIL" not in dev[0].request("WPS_ER_START ifname=lo"):
            raise Exception("WPS_ER_START succeeded during OOM")
    with alloc_fail(dev[0], 1, "http_server_init"):
        if "FAIL" not in dev[0].request("WPS_ER_START ifname=lo"):
            raise Exception("WPS_ER_START succeeded during OOM")
    with alloc_fail(dev[0], 2, "http_server_init"):
        if "FAIL" not in dev[0].request("WPS_ER_START ifname=lo"):
            raise Exception("WPS_ER_START succeeded during OOM")
    with alloc_fail(dev[0], 1, "eloop_sock_table_add_sock;?eloop_register_sock;wps_er_ssdp_init"):
        if "FAIL" not in dev[0].request("WPS_ER_START ifname=lo"):
            raise Exception("WPS_ER_START succeeded during OOM")
    with fail_test(dev[0], 1, "os_get_random;wps_er_init"):
        if "FAIL" not in dev[0].request("WPS_ER_START ifname=lo"):
            raise Exception("WPS_ER_START succeeded during os_get_random failure")

@remote_compatible
def test_ap_wps_er_init_fail(dev, apdev):
    """WPS ER init failure"""
    if "FAIL" not in dev[0].request("WPS_ER_START ifname=does-not-exist"):
        dev[0].request("WPS_ER_STOP")
        raise Exception("WPS_ER_START with non-existing ifname succeeded")

def test_ap_wps_wpa_cli_action(dev, apdev, test_params):
    """WPS events and wpa_cli action script"""
    logdir = os.path.abspath(test_params['logdir'])
    pidfile = os.path.join(logdir, 'ap_wps_wpa_cli_action.wpa_cli.pid')
    logfile = os.path.join(logdir, 'ap_wps_wpa_cli_action.wpa_cli.res')
    actionfile = os.path.join(logdir, 'ap_wps_wpa_cli_action.wpa_cli.action.sh')

    with open(actionfile, 'w') as f:
        f.write('#!/bin/sh\n')
        f.write('echo $* >> %s\n' % logfile)
        # Kill the process and wait some time before returning to allow all the
        # pending events to be processed with some of this happening after the
        # eloop SIGALRM signal has been scheduled.
        f.write('if [ $2 = "WPS-SUCCESS" -a -r %s ]; then kill `cat %s`; sleep 1; fi\n' % (pidfile, pidfile))

    os.chmod(actionfile, stat.S_IREAD | stat.S_IWRITE | stat.S_IEXEC |
             stat.S_IRGRP | stat.S_IXGRP | stat.S_IROTH | stat.S_IXOTH)

    ssid = "test-wps-conf"
    hapd = hostapd.add_ap(apdev[0],
                          {"ssid": ssid, "eap_server": "1", "wps_state": "2",
                           "wpa_passphrase": "12345678", "wpa": "2",
                           "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP"})

    prg = os.path.join(test_params['logdir'],
                       'alt-wpa_supplicant/wpa_supplicant/wpa_cli')
    if not os.path.exists(prg):
        prg = '../../wpa_supplicant/wpa_cli'
    arg = [prg, '-P', pidfile, '-B', '-i', dev[0].ifname, '-a', actionfile]
    subprocess.call(arg)

    arg = ['ps', 'ax']
    cmd = subprocess.Popen(arg, stdout=subprocess.PIPE)
    out = cmd.communicate()[0].decode()
    cmd.wait()
    logger.debug("Processes:\n" + out)
    if "wpa_cli -P %s -B -i %s" % (pidfile, dev[0].ifname) not in out:
        raise Exception("Did not see wpa_cli running")

    hapd.request("WPS_PIN any 12345670")
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[0].dump_monitor()
    dev[0].request("WPS_PIN " + apdev[0]['bssid'] + " 12345670")
    dev[0].wait_connected(timeout=30)

    for i in range(30):
        if not os.path.exists(pidfile):
            break
        time.sleep(0.1)

    if not os.path.exists(logfile):
        raise Exception("wpa_cli action results file not found")
    with open(logfile, 'r') as f:
        res = f.read()
    if "WPS-SUCCESS" not in res:
        raise Exception("WPS-SUCCESS event not seen in action file")

    arg = ['ps', 'ax']
    cmd = subprocess.Popen(arg, stdout=subprocess.PIPE)
    out = cmd.communicate()[0].decode()
    cmd.wait()
    logger.debug("Remaining processes:\n" + out)
    if "wpa_cli -P %s -B -i %s" % (pidfile, dev[0].ifname) in out:
        raise Exception("wpa_cli still running")

    if os.path.exists(pidfile):
        raise Exception("PID file not removed")

def test_ap_wps_er_ssdp_proto(dev, apdev):
    """WPS ER SSDP protocol testing"""
    try:
        _test_ap_wps_er_ssdp_proto(dev, apdev)
    finally:
        dev[0].request("WPS_ER_STOP")

def _test_ap_wps_er_ssdp_proto(dev, apdev):
    socket.setdefaulttimeout(1)
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(("239.255.255.250", 1900))
    if "FAIL" not in dev[0].request("WPS_ER_START ifname=lo foo"):
        raise Exception("Invalid filter accepted")
    if "OK" not in dev[0].request("WPS_ER_START ifname=lo 1.2.3.4"):
        raise Exception("WPS_ER_START with filter failed")
    (msg, addr) = sock.recvfrom(1000)
    msg = msg.decode()
    logger.debug("Received SSDP message from %s: %s" % (str(addr), msg))
    if "M-SEARCH" not in msg:
        raise Exception("Not an M-SEARCH")
    sock.sendto(b"FOO", addr)
    time.sleep(0.1)
    dev[0].request("WPS_ER_STOP")

    dev[0].request("WPS_ER_START ifname=lo")
    (msg, addr) = sock.recvfrom(1000)
    msg = msg.decode()
    logger.debug("Received SSDP message from %s: %s" % (str(addr), msg))
    if "M-SEARCH" not in msg:
        raise Exception("Not an M-SEARCH")
    sock.sendto(b"FOO", addr)
    sock.sendto(b"HTTP/1.1 200 OK\r\nFOO\r\n\r\n", addr)
    sock.sendto(b"HTTP/1.1 200 OK\r\nNTS:foo\r\n\r\n", addr)
    sock.sendto(b"HTTP/1.1 200 OK\r\nNTS:ssdp:byebye\r\n\r\n", addr)
    sock.sendto(b"HTTP/1.1 200 OK\r\ncache-control:   foo=1\r\n\r\n", addr)
    sock.sendto(b"HTTP/1.1 200 OK\r\ncache-control:   max-age=1\r\n\r\n", addr)
    sock.sendto(b"HTTP/1.1 200 OK\r\nusn:\r\n\r\n", addr)
    sock.sendto(b"HTTP/1.1 200 OK\r\nusn:foo\r\n\r\n", addr)
    sock.sendto(b"HTTP/1.1 200 OK\r\nusn:   uuid:\r\n\r\n", addr)
    sock.sendto(b"HTTP/1.1 200 OK\r\nusn:   uuid:     \r\n\r\n", addr)
    sock.sendto(b"HTTP/1.1 200 OK\r\nusn:   uuid:     foo\r\n\r\n", addr)
    sock.sendto(b"HTTP/1.1 200 OK\r\nST: urn:schemas-wifialliance-org:device:WFADevice:1\r\n\r\n", addr)
    sock.sendto(b"HTTP/1.1 200 OK\r\nST: urn:schemas-wifialliance-org:device:WFADevice:1\r\nNTS:ssdp:byebye\r\n\r\n", addr)
    sock.sendto(b"HTTP/1.1 200 OK\r\nST: urn:schemas-wifialliance-org:device:WFADevice:1\r\nlocation:foo\r\n\r\n", addr)
    with alloc_fail(dev[0], 1, "wps_er_ap_add"):
        sock.sendto(b"HTTP/1.1 200 OK\r\nST: urn:schemas-wifialliance-org:device:WFADevice:1\r\nlocation:foo\r\ncache-control:max-age=1\r\n\r\n", addr)
        time.sleep(0.1)
    with alloc_fail(dev[0], 2, "wps_er_ap_add"):
        sock.sendto(b"HTTP/1.1 200 OK\r\nST: urn:schemas-wifialliance-org:device:WFADevice:1\r\nlocation:foo\r\ncache-control:max-age=1\r\n\r\n", addr)
        time.sleep(0.1)

    # Add an AP with bogus URL
    sock.sendto(b"HTTP/1.1 200 OK\r\nST: urn:schemas-wifialliance-org:device:WFADevice:1\r\nlocation:foo\r\ncache-control:max-age=1\r\n\r\n", addr)
    # Update timeout on AP without updating URL
    sock.sendto(b"HTTP/1.1 200 OK\r\nST: urn:schemas-wifialliance-org:device:WFADevice:1\r\nlocation:http://127.0.0.1:12345/foo.xml\r\ncache-control:max-age=1\r\n\r\n", addr)
    ev = dev[0].wait_event(["WPS-ER-AP-REMOVE"], timeout=5)
    if ev is None:
        raise Exception("No WPS-ER-AP-REMOVE event on max-age timeout")

    # Add an AP with a valid URL (but no server listing to it)
    sock.sendto(b"HTTP/1.1 200 OK\r\nST: urn:schemas-wifialliance-org:device:WFADevice:1\r\nlocation:http://127.0.0.1:12345/foo.xml\r\ncache-control:max-age=1\r\n\r\n", addr)
    ev = dev[0].wait_event(["WPS-ER-AP-REMOVE"], timeout=5)
    if ev is None:
        raise Exception("No WPS-ER-AP-REMOVE event on max-age timeout")

    sock.close()

wps_event_url = None

def gen_upnp_info(eventSubURL='wps_event', controlURL='wps_control',
                  udn='uuid:27ea801a-9e5c-4e73-bd82-f89cbcd10d7e'):
    payload = '''<?xml version="1.0"?>
<root xmlns="urn:schemas-upnp-org:device-1-0">
<specVersion>
<major>1</major>
<minor>0</minor>
</specVersion>
<device>
<deviceType>urn:schemas-wifialliance-org:device:WFADevice:1</deviceType>
<friendlyName>WPS Access Point</friendlyName>
<manufacturer>Company</manufacturer>
<modelName>WAP</modelName>
<modelNumber>123</modelNumber>
<serialNumber>12345</serialNumber>
'''
    if udn:
        payload += '<UDN>' + udn + '</UDN>'
    payload += '''<serviceList>
<service>
<serviceType>urn:schemas-wifialliance-org:service:WFAWLANConfig:1</serviceType>
<serviceId>urn:wifialliance-org:serviceId:WFAWLANConfig1</serviceId>
<SCPDURL>wps_scpd.xml</SCPDURL>
'''
    if controlURL:
        payload += '<controlURL>' + controlURL + '</controlURL>\n'
    if eventSubURL:
        payload += '<eventSubURL>' + eventSubURL + '</eventSubURL>\n'
    payload += '''</service>
</serviceList>
</device>
</root>
'''
    hdr = 'HTTP/1.1 200 OK\r\n' + \
          'Content-Type: text/xml; charset="utf-8"\r\n' + \
          'Server: Unspecified, UPnP/1.0, Unspecified\r\n' + \
          'Connection: close\r\n' + \
          'Content-Length: ' + str(len(payload)) + '\r\n' + \
          'Date: Sat, 15 Aug 2015 18:55:08 GMT\r\n\r\n'
    return (hdr + payload).encode()

def gen_wps_control(payload_override=None):
    payload = '''<?xml version="1.0"?>
<s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/" s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">
<s:Body>
<u:GetDeviceInfoResponse xmlns:u="urn:schemas-wifialliance-org:service:WFAWLANConfig:1">
<NewDeviceInfo>EEoAARAQIgABBBBHABAn6oAanlxOc72C+Jy80Q1+ECAABgIAAAADABAaABCJZ7DPtbU3Ust9
Z3wJF07WEDIAwH45D3i1OqB7eJGwTzqeapS71h3KyXncK2xJZ+xqScrlorNEg6LijBJzG2Ca
+FZli0iliDJd397yAx/jk4nFXco3q5ylBSvSw9dhJ5u1xBKSnTilKGlUHPhLP75PUqM3fot9
7zwtFZ4bx6x1sBA6oEe2d0aUJmLumQGCiKEIWlnxs44zego/2tAe81bDzdPBM7o5HH/FUhD+
KoGzFXp51atP+1n9Vta6AkI0Vye99JKLcC6Md9dMJltSVBgd4Xc4lRAEAAIAIxAQAAIADRAN
AAEBEAgAAgAEEEQAAQIQIQAHQ29tcGFueRAjAANXQVAQJAADMTIzEEIABTEyMzQ1EFQACAAG
AFDyBAABEBEAC1dpcmVsZXNzIEFQEDwAAQEQAgACAAAQEgACAAAQCQACAAAQLQAEgQIDABBJ
AAYANyoAASA=
</NewDeviceInfo>
</u:GetDeviceInfoResponse>
</s:Body>
</s:Envelope>
'''
    if payload_override:
        payload = payload_override
    hdr = 'HTTP/1.1 200 OK\r\n' + \
          'Content-Type: text/xml; charset="utf-8"\r\n' + \
          'Server: Unspecified, UPnP/1.0, Unspecified\r\n' + \
          'Connection: close\r\n' + \
          'Content-Length: ' + str(len(payload)) + '\r\n' + \
          'Date: Sat, 15 Aug 2015 18:55:08 GMT\r\n\r\n'
    return (hdr + payload).encode()

def gen_wps_event(sid='uuid:7eb3342a-8a5f-47fe-a585-0785bfec6d8a'):
    payload = ""
    hdr = 'HTTP/1.1 200 OK\r\n' + \
          'Content-Type: text/xml; charset="utf-8"\r\n' + \
          'Server: Unspecified, UPnP/1.0, Unspecified\r\n' + \
          'Connection: close\r\n' + \
          'Content-Length: ' + str(len(payload)) + '\r\n'
    if sid:
        hdr += 'SID: ' + sid + '\r\n'
    hdr += 'Timeout: Second-1801\r\n' + \
          'Date: Sat, 15 Aug 2015 18:55:08 GMT\r\n\r\n'
    return (hdr + payload).encode()

class WPSAPHTTPServer(StreamRequestHandler):
    def handle(self):
        data = self.rfile.readline().decode().strip()
        logger.info("HTTP server received: " + data)
        while True:
            hdr = self.rfile.readline().decode().strip()
            if len(hdr) == 0:
                break
            logger.info("HTTP header: " + hdr)
            if "CALLBACK:" in hdr:
                global wps_event_url
                wps_event_url = hdr.split(' ')[1].strip('<>')

        if "GET /foo.xml" in data:
            self.handle_upnp_info()
        elif "POST /wps_control" in data:
            self.handle_wps_control()
        elif "SUBSCRIBE /wps_event" in data:
            self.handle_wps_event()
        else:
            self.handle_others(data)

    def handle_upnp_info(self):
        self.wfile.write(gen_upnp_info())

    def handle_wps_control(self):
        self.wfile.write(gen_wps_control())

    def handle_wps_event(self):
        self.wfile.write(gen_wps_event())

    def handle_others(self, data):
        logger.info("Ignore HTTP request: " + data)

class MyTCPServer(TCPServer):
    def __init__(self, addr, handler):
        self.allow_reuse_address = True
        TCPServer.__init__(self, addr, handler)

def wps_er_start(dev, http_server, max_age=1, wait_m_search=False,
                 location_url=None):
    socket.setdefaulttimeout(1)
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(("239.255.255.250", 1900))
    dev.request("WPS_ER_START ifname=lo")
    for i in range(100):
        (msg, addr) = sock.recvfrom(1000)
        msg = msg.decode()
        logger.debug("Received SSDP message from %s: %s" % (str(addr), msg))
        if "M-SEARCH" in msg:
            break
        if not wait_m_search:
            raise Exception("Not an M-SEARCH")
        if i == 99:
            raise Exception("No M-SEARCH seen")

    # Add an AP with a valid URL and server listing to it
    server = MyTCPServer(("127.0.0.1", 12345), http_server)
    if not location_url:
        location_url = 'http://127.0.0.1:12345/foo.xml'
    sock.sendto(("HTTP/1.1 200 OK\r\nST: urn:schemas-wifialliance-org:device:WFADevice:1\r\nlocation:%s\r\ncache-control:max-age=%d\r\n\r\n" % (location_url, max_age)).encode(), addr)
    server.timeout = 1
    return server, sock

def wps_er_stop(dev, sock, server, on_alloc_fail=False):
    sock.close()
    server.server_close()

    if on_alloc_fail:
        done = False
        for i in range(50):
            res = dev.request("GET_ALLOC_FAIL")
            if res.startswith("0:"):
                done = True
                break
            time.sleep(0.1)
        if not done:
            raise Exception("No allocation failure reported")
    else:
        ev = dev.wait_event(["WPS-ER-AP-REMOVE"], timeout=5)
        if ev is None:
            raise Exception("No WPS-ER-AP-REMOVE event on max-age timeout")
    dev.request("WPS_ER_STOP")

def run_wps_er_proto_test(dev, handler, no_event_url=False, location_url=None):
    try:
        uuid = '27ea801a-9e5c-4e73-bd82-f89cbcd10d7e'
        server, sock = wps_er_start(dev, handler, location_url=location_url)
        global wps_event_url
        wps_event_url = None
        server.handle_request()
        server.handle_request()
        server.handle_request()
        server.server_close()
        if no_event_url:
            if wps_event_url:
                raise Exception("Received event URL unexpectedly")
            return
        if wps_event_url is None:
            raise Exception("Did not get event URL")
        logger.info("Event URL: " + wps_event_url)
    finally:
            dev.request("WPS_ER_STOP")

def send_wlanevent(url, uuid, data, no_response=False):
    conn = HTTPConnection(url.netloc)
    payload = '''<?xml version="1.0" encoding="utf-8"?>
<e:propertyset xmlns:e="urn:schemas-upnp-org:event-1-0">
<e:property><STAStatus>1</STAStatus></e:property>
<e:property><APStatus>1</APStatus></e:property>
<e:property><WLANEvent>'''
    payload += base64.b64encode(data).decode()
    payload += '</WLANEvent></e:property></e:propertyset>'
    headers = {"Content-type": 'text/xml; charset="utf-8"',
               "Server": "Unspecified, UPnP/1.0, Unspecified",
               "HOST": url.netloc,
               "NT": "upnp:event",
               "SID": "uuid:" + uuid,
               "SEQ": "0",
               "Content-Length": str(len(payload))}
    conn.request("NOTIFY", url.path, payload, headers)
    if no_response:
        try:
            conn.getresponse()
        except Exception as e:
            pass
        return
    resp = conn.getresponse()
    if resp.status != 200:
        raise Exception("Unexpected HTTP response: %d" % resp.status)

def test_ap_wps_er_http_proto(dev, apdev):
    """WPS ER HTTP protocol testing"""
    try:
        _test_ap_wps_er_http_proto(dev, apdev)
    finally:
        dev[0].request("WPS_ER_STOP")

def _test_ap_wps_er_http_proto(dev, apdev):
    uuid = '27ea801a-9e5c-4e73-bd82-f89cbcd10d7e'
    server, sock = wps_er_start(dev[0], WPSAPHTTPServer, max_age=15)
    global wps_event_url
    wps_event_url = None
    server.handle_request()
    server.handle_request()
    server.handle_request()
    server.server_close()
    if wps_event_url is None:
        raise Exception("Did not get event URL")
    logger.info("Event URL: " + wps_event_url)

    ev = dev[0].wait_event(["WPS-ER-AP-ADD"], timeout=10)
    if ev is None:
        raise Exception("No WPS-ER-AP-ADD event")
    if uuid not in ev:
        raise Exception("UUID mismatch")

    sock.close()

    logger.info("Valid Probe Request notification")
    url = urlparse(wps_event_url)
    conn = HTTPConnection(url.netloc)
    payload = '''<?xml version="1.0" encoding="utf-8"?>
<e:propertyset xmlns:e="urn:schemas-upnp-org:event-1-0">
<e:property><STAStatus>1</STAStatus></e:property>
<e:property><APStatus>1</APStatus></e:property>
<e:property><WLANEvent>ATAyOjAwOjAwOjAwOjAwOjAwEEoAARAQOgABAhAIAAIxSBBHABA2LbR7pTpRkYj7VFi5hrLk
EFQACAAAAAAAAAAAEDwAAQMQAgACAAAQCQACAAAQEgACAAAQIQABIBAjAAEgECQAASAQEQAI
RGV2aWNlIEEQSQAGADcqAAEg
</WLANEvent></e:property>
</e:propertyset>
'''
    headers = {"Content-type": 'text/xml; charset="utf-8"',
               "Server": "Unspecified, UPnP/1.0, Unspecified",
               "HOST": url.netloc,
               "NT": "upnp:event",
               "SID": "uuid:" + uuid,
               "SEQ": "0",
               "Content-Length": str(len(payload))}
    conn.request("NOTIFY", url.path, payload, headers)
    resp = conn.getresponse()
    if resp.status != 200:
        raise Exception("Unexpected HTTP response: %d" % resp.status)

    ev = dev[0].wait_event(["WPS-ER-ENROLLEE-ADD"], timeout=5)
    if ev is None:
        raise Exception("No WPS-ER-ENROLLEE-ADD event")
    if "362db47b-a53a-5191-88fb-5458b986b2e4" not in ev:
        raise Exception("No Enrollee UUID match")

    logger.info("Incorrect event URL AP id")
    conn = HTTPConnection(url.netloc)
    conn.request("NOTIFY", url.path + '123', payload, headers)
    resp = conn.getresponse()
    if resp.status != 404:
        raise Exception("Unexpected HTTP response: %d" % resp.status)

    logger.info("Missing AP id")
    conn = HTTPConnection(url.netloc)
    conn.request("NOTIFY", '/event/' + url.path.split('/')[2],
                 payload, headers)
    time.sleep(0.1)

    logger.info("Incorrect event URL event id")
    conn = HTTPConnection(url.netloc)
    conn.request("NOTIFY", '/event/123456789/123', payload, headers)
    time.sleep(0.1)

    logger.info("Incorrect event URL prefix")
    conn = HTTPConnection(url.netloc)
    conn.request("NOTIFY", '/foobar/123456789/123', payload, headers)
    resp = conn.getresponse()
    if resp.status != 404:
        raise Exception("Unexpected HTTP response: %d" % resp.status)

    logger.info("Unsupported request")
    conn = HTTPConnection(url.netloc)
    conn.request("FOOBAR", '/foobar/123456789/123', payload, headers)
    resp = conn.getresponse()
    if resp.status != 501:
        raise Exception("Unexpected HTTP response: %d" % resp.status)

    logger.info("Unsupported request and OOM")
    with alloc_fail(dev[0], 1, "wps_er_http_req"):
        conn = HTTPConnection(url.netloc)
        conn.request("FOOBAR", '/foobar/123456789/123', payload, headers)
        time.sleep(0.5)

    logger.info("Too short WLANEvent")
    data = b'\x00'
    send_wlanevent(url, uuid, data)

    logger.info("Invalid WLANEventMAC")
    data = b'\x00qwertyuiopasdfghjklzxcvbnm'
    send_wlanevent(url, uuid, data)

    logger.info("Unknown WLANEventType")
    data = b'\xff02:00:00:00:00:00'
    send_wlanevent(url, uuid, data)

    logger.info("Probe Request notification without any attributes")
    data = b'\x0102:00:00:00:00:00'
    send_wlanevent(url, uuid, data)

    logger.info("Probe Request notification with invalid attribute")
    data = b'\x0102:00:00:00:00:00\xff'
    send_wlanevent(url, uuid, data)

    logger.info("EAP message without any attributes")
    data = b'\x0202:00:00:00:00:00'
    send_wlanevent(url, uuid, data)

    logger.info("EAP message with invalid attribute")
    data = b'\x0202:00:00:00:00:00\xff'
    send_wlanevent(url, uuid, data)

    logger.info("EAP message from new STA and not M1")
    data = b'\x0202:ff:ff:ff:ff:ff' + b'\x10\x22\x00\x01\x05'
    send_wlanevent(url, uuid, data)

    logger.info("EAP message: M1")
    data = b'\x0202:00:00:00:00:00'
    data += b'\x10\x22\x00\x01\x04'
    data += b'\x10\x47\x00\x10' + 16 * b'\x00'
    data += b'\x10\x20\x00\x06\x02\x00\x00\x00\x00\x00'
    data += b'\x10\x1a\x00\x10' + 16 * b'\x00'
    data += b'\x10\x32\x00\xc0' + 192 * b'\x00'
    data += b'\x10\x04\x00\x02\x00\x00'
    data += b'\x10\x10\x00\x02\x00\x00'
    data += b'\x10\x0d\x00\x01\x00'
    data += b'\x10\x08\x00\x02\x00\x00'
    data += b'\x10\x44\x00\x01\x00'
    data += b'\x10\x21\x00\x00'
    data += b'\x10\x23\x00\x00'
    data += b'\x10\x24\x00\x00'
    data += b'\x10\x42\x00\x00'
    data += b'\x10\x54\x00\x08' + 8 * b'\x00'
    data += b'\x10\x11\x00\x00'
    data += b'\x10\x3c\x00\x01\x00'
    data += b'\x10\x02\x00\x02\x00\x00'
    data += b'\x10\x12\x00\x02\x00\x00'
    data += b'\x10\x09\x00\x02\x00\x00'
    data += b'\x10\x2d\x00\x04\x00\x00\x00\x00'
    m1 = data
    send_wlanevent(url, uuid, data)

    logger.info("EAP message: WSC_ACK")
    data = b'\x0202:00:00:00:00:00' + b'\x10\x22\x00\x01\x0d'
    send_wlanevent(url, uuid, data)

    logger.info("EAP message: M1")
    send_wlanevent(url, uuid, m1)

    logger.info("EAP message: WSC_NACK")
    data = b'\x0202:00:00:00:00:00' + b'\x10\x22\x00\x01\x0e'
    send_wlanevent(url, uuid, data)

    logger.info("EAP message: M1 - Too long attribute values")
    data = b'\x0202:00:00:00:00:00'
    data += b'\x10\x11\x00\x21' + 33 * b'\x00'
    data += b'\x10\x45\x00\x21' + 33 * b'\x00'
    data += b'\x10\x42\x00\x21' + 33 * b'\x00'
    data += b'\x10\x24\x00\x21' + 33 * b'\x00'
    data += b'\x10\x23\x00\x21' + 33 * b'\x00'
    data += b'\x10\x21\x00\x41' + 65 * b'\x00'
    data += b'\x10\x49\x00\x09\x00\x37\x2a\x05\x02\x00\x00\x05\x00'
    send_wlanevent(url, uuid, data)

    logger.info("EAP message: M1 missing UUID-E")
    data = b'\x0202:00:00:00:00:00'
    data += b'\x10\x22\x00\x01\x04'
    send_wlanevent(url, uuid, data)

    logger.info("EAP message: M1 missing MAC Address")
    data += b'\x10\x47\x00\x10' + 16 * b'\x00'
    send_wlanevent(url, uuid, data)

    logger.info("EAP message: M1 missing Enrollee Nonce")
    data += b'\x10\x20\x00\x06\x02\x00\x00\x00\x00\x00'
    send_wlanevent(url, uuid, data)

    logger.info("EAP message: M1 missing Public Key")
    data += b'\x10\x1a\x00\x10' + 16 * b'\x00'
    send_wlanevent(url, uuid, data)

    logger.info("EAP message: M1 missing Authentication Type flags")
    data += b'\x10\x32\x00\xc0' + 192 * b'\x00'
    send_wlanevent(url, uuid, data)

    logger.info("EAP message: M1 missing Encryption Type Flags")
    data += b'\x10\x04\x00\x02\x00\x00'
    send_wlanevent(url, uuid, data)

    logger.info("EAP message: M1 missing Connection Type flags")
    data += b'\x10\x10\x00\x02\x00\x00'
    send_wlanevent(url, uuid, data)

    logger.info("EAP message: M1 missing Config Methods")
    data += b'\x10\x0d\x00\x01\x00'
    send_wlanevent(url, uuid, data)

    logger.info("EAP message: M1 missing Wi-Fi Protected Setup State")
    data += b'\x10\x08\x00\x02\x00\x00'
    send_wlanevent(url, uuid, data)

    logger.info("EAP message: M1 missing Manufacturer")
    data += b'\x10\x44\x00\x01\x00'
    send_wlanevent(url, uuid, data)

    logger.info("EAP message: M1 missing Model Name")
    data += b'\x10\x21\x00\x00'
    send_wlanevent(url, uuid, data)

    logger.info("EAP message: M1 missing Model Number")
    data += b'\x10\x23\x00\x00'
    send_wlanevent(url, uuid, data)

    logger.info("EAP message: M1 missing Serial Number")
    data += b'\x10\x24\x00\x00'
    send_wlanevent(url, uuid, data)

    logger.info("EAP message: M1 missing Primary Device Type")
    data += b'\x10\x42\x00\x00'
    send_wlanevent(url, uuid, data)

    logger.info("EAP message: M1 missing Device Name")
    data += b'\x10\x54\x00\x08' + 8 * b'\x00'
    send_wlanevent(url, uuid, data)

    logger.info("EAP message: M1 missing RF Bands")
    data += b'\x10\x11\x00\x00'
    send_wlanevent(url, uuid, data)

    logger.info("EAP message: M1 missing Association State")
    data += b'\x10\x3c\x00\x01\x00'
    send_wlanevent(url, uuid, data)

    logger.info("EAP message: M1 missing Device Password ID")
    data += b'\x10\x02\x00\x02\x00\x00'
    send_wlanevent(url, uuid, data)

    logger.info("EAP message: M1 missing Configuration Error")
    data += b'\x10\x12\x00\x02\x00\x00'
    send_wlanevent(url, uuid, data)

    logger.info("EAP message: M1 missing OS Version")
    data += b'\x10\x09\x00\x02\x00\x00'
    send_wlanevent(url, uuid, data)

    logger.info("Check max concurrent requests")
    addr = (url.hostname, url.port)
    socks = {}
    for i in range(20):
        socks[i] = socket.socket(socket.AF_INET, socket.SOCK_STREAM,
                                 socket.IPPROTO_TCP)
        socks[i].settimeout(10)
        socks[i].connect(addr)
    for i in range(20):
        socks[i].send(b"GET / HTTP/1.1\r\n\r\n")
    count = 0
    for i in range(20):
        try:
            res = socks[i].recv(100).decode()
            if "HTTP/1" in res:
                count += 1
            else:
                logger.info("recv[%d]: len=%d" % (i, len(res)))
        except:
            pass
        socks[i].close()
    logger.info("%d concurrent HTTP GET operations returned response" % count)
    if count < 8:
        raise Exception("Too few concurrent HTTP connections accepted")

    logger.info("OOM in HTTP server")
    for func in ["http_request_init", "httpread_create",
                 "eloop_register_timeout;httpread_create",
                 "eloop_sock_table_add_sock;?eloop_register_sock;httpread_create",
                 "httpread_hdr_analyze"]:
        with alloc_fail(dev[0], 1, func):
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM,
                                 socket.IPPROTO_TCP)
            sock.connect(addr)
            sock.send(b"GET / HTTP/1.1\r\n\r\n")
            try:
                sock.recv(100)
            except:
                pass
            sock.close()

    logger.info("Invalid HTTP header")
    for req in [" GET / HTTP/1.1\r\n\r\n",
                "HTTP/1.1 200 OK\r\n\r\n",
                "HTTP/\r\n\r\n",
                "GET %%a%aa% HTTP/1.1\r\n\r\n",
                "GET / HTTP/1.1\r\n FOO\r\n\r\n",
                "NOTIFY / HTTP/1.1\r\n" + 4097*'a' + '\r\n\r\n',
                "NOTIFY / HTTP/1.1\r\n\r\n" + 8193*'a',
                "POST / HTTP/1.1\r\nTransfer-Encoding: CHUNKED\r\n\r\n foo\r\n",
                "POST / HTTP/1.1\r\nTransfer-Encoding: CHUNKED\r\n\r\n1\r\nfoo\r\n",
                "POST / HTTP/1.1\r\nTransfer-Encoding: CHUNKED\r\n\r\n0\r\n",
                "POST / HTTP/1.1\r\nTransfer-Encoding: CHUNKED\r\n\r\n0\r\naa\ra\r\n\ra"]:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM,
                             socket.IPPROTO_TCP)
        sock.settimeout(0.1)
        sock.connect(addr)
        sock.send(req.encode())
        try:
            sock.recv(100)
        except:
            pass
        sock.close()

    with alloc_fail(dev[0], 2, "httpread_read_handler"):
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM,
                             socket.IPPROTO_TCP)
        sock.connect(addr)
        sock.send(b"NOTIFY / HTTP/1.1\r\n\r\n" + 4500 * b'a')
        try:
            sock.recv(100)
        except:
            pass
        sock.close()

    conn = HTTPConnection(url.netloc)
    payload = '<foo'
    headers = {"Content-type": 'text/xml; charset="utf-8"',
               "Server": "Unspecified, UPnP/1.0, Unspecified",
               "HOST": url.netloc,
               "NT": "upnp:event",
               "SID": "uuid:" + uuid,
               "SEQ": "0",
               "Content-Length": str(len(payload))}
    conn.request("NOTIFY", url.path, payload, headers)
    resp = conn.getresponse()
    if resp.status != 200:
        raise Exception("Unexpected HTTP response: %d" % resp.status)

    conn = HTTPConnection(url.netloc)
    payload = '<WLANEvent foo></WLANEvent>'
    headers = {"Content-type": 'text/xml; charset="utf-8"',
               "Server": "Unspecified, UPnP/1.0, Unspecified",
               "HOST": url.netloc,
               "NT": "upnp:event",
               "SID": "uuid:" + uuid,
               "SEQ": "0",
               "Content-Length": str(len(payload))}
    conn.request("NOTIFY", url.path, payload, headers)
    resp = conn.getresponse()
    if resp.status != 200:
        raise Exception("Unexpected HTTP response: %d" % resp.status)

    with alloc_fail(dev[0], 1, "xml_get_first_item"):
        send_wlanevent(url, uuid, b'')

    with alloc_fail(dev[0], 1, "wpabuf_alloc_ext_data;xml_get_base64_item"):
        send_wlanevent(url, uuid, b'foo')

    for func in ["wps_init",
                 "wps_process_manufacturer",
                 "wps_process_model_name",
                 "wps_process_model_number",
                 "wps_process_serial_number",
                 "wps_process_dev_name"]:
        with alloc_fail(dev[0], 1, func):
            send_wlanevent(url, uuid, m1)

    with alloc_fail(dev[0], 1, "wps_er_http_resp_ok"):
        send_wlanevent(url, uuid, m1, no_response=True)

    with alloc_fail(dev[0], 1, "wps_er_http_resp_not_found"):
        url2 = urlparse(wps_event_url.replace('/event/', '/notfound/'))
        send_wlanevent(url2, uuid, m1, no_response=True)

    logger.info("EAP message: M1")
    data = b'\x0202:11:22:00:00:00'
    data += b'\x10\x22\x00\x01\x04'
    data += b'\x10\x47\x00\x10' + 16 * b'\x00'
    data += b'\x10\x20\x00\x06\x02\x00\x00\x00\x00\x00'
    data += b'\x10\x1a\x00\x10' + 16 * b'\x00'
    data += b'\x10\x32\x00\xc0' + 192 * b'\x00'
    data += b'\x10\x04\x00\x02\x00\x00'
    data += b'\x10\x10\x00\x02\x00\x00'
    data += b'\x10\x0d\x00\x01\x00'
    data += b'\x10\x08\x00\x02\x00\x00'
    data += b'\x10\x44\x00\x01\x00'
    data += b'\x10\x21\x00\x00'
    data += b'\x10\x23\x00\x00'
    data += b'\x10\x24\x00\x00'
    data += b'\x10\x42\x00\x00'
    data += b'\x10\x54\x00\x08' + 8 * b'\x00'
    data += b'\x10\x11\x00\x00'
    data += b'\x10\x3c\x00\x01\x00'
    data += b'\x10\x02\x00\x02\x00\x00'
    data += b'\x10\x12\x00\x02\x00\x00'
    data += b'\x10\x09\x00\x02\x00\x00'
    data += b'\x10\x2d\x00\x04\x00\x00\x00\x00'
    dev[0].dump_monitor()
    with alloc_fail(dev[0], 1, "wps_er_add_sta_data"):
        send_wlanevent(url, uuid, data)
        ev = dev[0].wait_event(["WPS-ER-ENROLLEE-ADD"], timeout=0.1)
        if ev is not None:
            raise Exception("Unexpected enrollee add event")
    send_wlanevent(url, uuid, data)
    ev = dev[0].wait_event(["WPS-ER-ENROLLEE-ADD"], timeout=2)
    if ev is None:
        raise Exception("Enrollee add event not seen")

    with alloc_fail(dev[0], 1,
                    "base64_gen_encode;?base64_encode;wps_er_soap_hdr"):
        send_wlanevent(url, uuid, data)

    with alloc_fail(dev[0], 1, "wpabuf_alloc;wps_er_soap_hdr"):
        send_wlanevent(url, uuid, data)

    with alloc_fail(dev[0], 1, "http_client_url_parse;wps_er_sta_send_msg"):
        send_wlanevent(url, uuid, data)

    with alloc_fail(dev[0], 1, "http_client_addr;wps_er_sta_send_msg"):
        send_wlanevent(url, uuid, data)

def test_ap_wps_er_http_proto_no_event_sub_url(dev, apdev):
    """WPS ER HTTP protocol testing - no eventSubURL"""
    class WPSAPHTTPServer_no_event_sub_url(WPSAPHTTPServer):
        def handle_upnp_info(self):
            self.wfile.write(gen_upnp_info(eventSubURL=None))
    run_wps_er_proto_test(dev[0], WPSAPHTTPServer_no_event_sub_url,
                          no_event_url=True)

def test_ap_wps_er_http_proto_event_sub_url_dns(dev, apdev):
    """WPS ER HTTP protocol testing - DNS name in eventSubURL"""
    class WPSAPHTTPServer_event_sub_url_dns(WPSAPHTTPServer):
        def handle_upnp_info(self):
            self.wfile.write(gen_upnp_info(eventSubURL='http://example.com/wps_event'))
    run_wps_er_proto_test(dev[0], WPSAPHTTPServer_event_sub_url_dns,
                          no_event_url=True)

def test_ap_wps_er_http_proto_subscribe_oom(dev, apdev):
    """WPS ER HTTP protocol testing - subscribe OOM"""
    try:
        _test_ap_wps_er_http_proto_subscribe_oom(dev, apdev)
    finally:
        dev[0].request("WPS_ER_STOP")

def _test_ap_wps_er_http_proto_subscribe_oom(dev, apdev):
    tests = [(1, "http_client_url_parse"),
             (1, "wpabuf_alloc;wps_er_subscribe"),
             (1, "http_client_addr"),
             (1, "eloop_sock_table_add_sock;?eloop_register_sock;http_client_addr"),
             (1, "eloop_register_timeout;http_client_addr")]
    for count, func in tests:
        with alloc_fail(dev[0], count, func):
            server, sock = wps_er_start(dev[0], WPSAPHTTPServer)
            server.handle_request()
            server.handle_request()
            wps_er_stop(dev[0], sock, server, on_alloc_fail=True)

def test_ap_wps_er_http_proto_no_sid(dev, apdev):
    """WPS ER HTTP protocol testing - no SID"""
    class WPSAPHTTPServer_no_sid(WPSAPHTTPServer):
        def handle_wps_event(self):
            self.wfile.write(gen_wps_event(sid=None))
    run_wps_er_proto_test(dev[0], WPSAPHTTPServer_no_sid)

def test_ap_wps_er_http_proto_invalid_sid_no_uuid(dev, apdev):
    """WPS ER HTTP protocol testing - invalid SID - no UUID"""
    class WPSAPHTTPServer_invalid_sid_no_uuid(WPSAPHTTPServer):
        def handle_wps_event(self):
            self.wfile.write(gen_wps_event(sid='FOO'))
    run_wps_er_proto_test(dev[0], WPSAPHTTPServer_invalid_sid_no_uuid)

def test_ap_wps_er_http_proto_invalid_sid_uuid(dev, apdev):
    """WPS ER HTTP protocol testing - invalid SID UUID"""
    class WPSAPHTTPServer_invalid_sid_uuid(WPSAPHTTPServer):
        def handle_wps_event(self):
            self.wfile.write(gen_wps_event(sid='uuid:FOO'))
    run_wps_er_proto_test(dev[0], WPSAPHTTPServer_invalid_sid_uuid)

def test_ap_wps_er_http_proto_subscribe_failing(dev, apdev):
    """WPS ER HTTP protocol testing - SUBSCRIBE failing"""
    class WPSAPHTTPServer_fail_subscribe(WPSAPHTTPServer):
        def handle_wps_event(self):
            payload = ""
            hdr = 'HTTP/1.1 404 Not Found\r\n' + \
                  'Content-Type: text/xml; charset="utf-8"\r\n' + \
                  'Server: Unspecified, UPnP/1.0, Unspecified\r\n' + \
                  'Connection: close\r\n' + \
                  'Content-Length: ' + str(len(payload)) + '\r\n' + \
                  'Timeout: Second-1801\r\n' + \
                  'Date: Sat, 15 Aug 2015 18:55:08 GMT\r\n\r\n'
            self.wfile.write((hdr + payload).encode())
    run_wps_er_proto_test(dev[0], WPSAPHTTPServer_fail_subscribe)

def test_ap_wps_er_http_proto_subscribe_invalid_response(dev, apdev):
    """WPS ER HTTP protocol testing - SUBSCRIBE and invalid response"""
    class WPSAPHTTPServer_subscribe_invalid_response(WPSAPHTTPServer):
        def handle_wps_event(self):
            payload = ""
            hdr = 'HTTP/1.1 FOO\r\n' + \
                  'Content-Type: text/xml; charset="utf-8"\r\n' + \
                  'Server: Unspecified, UPnP/1.0, Unspecified\r\n' + \
                  'Connection: close\r\n' + \
                  'Content-Length: ' + str(len(payload)) + '\r\n' + \
                  'Timeout: Second-1801\r\n' + \
                  'Date: Sat, 15 Aug 2015 18:55:08 GMT\r\n\r\n'
            self.wfile.write((hdr + payload).encode())
    run_wps_er_proto_test(dev[0], WPSAPHTTPServer_subscribe_invalid_response)

def test_ap_wps_er_http_proto_subscribe_invalid_response(dev, apdev):
    """WPS ER HTTP protocol testing - SUBSCRIBE and invalid response"""
    class WPSAPHTTPServer_invalid_m1(WPSAPHTTPServer):
        def handle_wps_control(self):
            payload = '''<?xml version="1.0"?>
<s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/" s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">
<s:Body>
<u:GetDeviceInfoResponse xmlns:u="urn:schemas-wifialliance-org:service:WFAWLANConfig:1">
<NewDeviceInfo>Rk9P</NewDeviceInfo>
</u:GetDeviceInfoResponse>
</s:Body>
</s:Envelope>
'''
            self.wfile.write(gen_wps_control(payload_override=payload))
    run_wps_er_proto_test(dev[0], WPSAPHTTPServer_invalid_m1, no_event_url=True)

def test_ap_wps_er_http_proto_upnp_info_no_device(dev, apdev):
    """WPS ER HTTP protocol testing - No device in UPnP info"""
    class WPSAPHTTPServer_no_device(WPSAPHTTPServer):
        def handle_upnp_info(self):
            payload = '''<?xml version="1.0"?>
<root xmlns="urn:schemas-upnp-org:device-1-0">
<specVersion>
<major>1</major>
<minor>0</minor>
</specVersion>
</root>
'''
            hdr = 'HTTP/1.1 200 OK\r\n' + \
                  'Content-Type: text/xml; charset="utf-8"\r\n' + \
                  'Server: Unspecified, UPnP/1.0, Unspecified\r\n' + \
                  'Connection: close\r\n' + \
                  'Content-Length: ' + str(len(payload)) + '\r\n' + \
                  'Date: Sat, 15 Aug 2015 18:55:08 GMT\r\n\r\n'
            self.wfile.write((hdr + payload).encode())
    run_wps_er_proto_test(dev[0], WPSAPHTTPServer_no_device, no_event_url=True)

def test_ap_wps_er_http_proto_upnp_info_no_device_type(dev, apdev):
    """WPS ER HTTP protocol testing - No deviceType in UPnP info"""
    class WPSAPHTTPServer_no_device(WPSAPHTTPServer):
        def handle_upnp_info(self):
            payload = '''<?xml version="1.0"?>
<root xmlns="urn:schemas-upnp-org:device-1-0">
<specVersion>
<major>1</major>
<minor>0</minor>
</specVersion>
<device>
</device>
</root>
'''
            hdr = 'HTTP/1.1 200 OK\r\n' + \
                  'Content-Type: text/xml; charset="utf-8"\r\n' + \
                  'Server: Unspecified, UPnP/1.0, Unspecified\r\n' + \
                  'Connection: close\r\n' + \
                  'Content-Length: ' + str(len(payload)) + '\r\n' + \
                  'Date: Sat, 15 Aug 2015 18:55:08 GMT\r\n\r\n'
            self.wfile.write((hdr + payload).encode())
    run_wps_er_proto_test(dev[0], WPSAPHTTPServer_no_device, no_event_url=True)

def test_ap_wps_er_http_proto_upnp_info_invalid_udn_uuid(dev, apdev):
    """WPS ER HTTP protocol testing - Invalid UDN UUID"""
    class WPSAPHTTPServer_invalid_udn_uuid(WPSAPHTTPServer):
        def handle_upnp_info(self):
            self.wfile.write(gen_upnp_info(udn='uuid:foo'))
    run_wps_er_proto_test(dev[0], WPSAPHTTPServer_invalid_udn_uuid)

def test_ap_wps_er_http_proto_no_control_url(dev, apdev):
    """WPS ER HTTP protocol testing - no controlURL"""
    class WPSAPHTTPServer_no_control_url(WPSAPHTTPServer):
        def handle_upnp_info(self):
            self.wfile.write(gen_upnp_info(controlURL=None))
    run_wps_er_proto_test(dev[0], WPSAPHTTPServer_no_control_url,
                          no_event_url=True)

def test_ap_wps_er_http_proto_control_url_dns(dev, apdev):
    """WPS ER HTTP protocol testing - DNS name in controlURL"""
    class WPSAPHTTPServer_control_url_dns(WPSAPHTTPServer):
        def handle_upnp_info(self):
            self.wfile.write(gen_upnp_info(controlURL='http://example.com/wps_control'))
    run_wps_er_proto_test(dev[0], WPSAPHTTPServer_control_url_dns,
                          no_event_url=True)

def test_ap_wps_http_timeout(dev, apdev):
    """WPS AP/ER and HTTP timeout"""
    try:
        _test_ap_wps_http_timeout(dev, apdev)
    finally:
        dev[0].request("WPS_ER_STOP")

def _test_ap_wps_http_timeout(dev, apdev):
    ap_uuid = "27ea801a-9e5c-4e73-bd82-f89cbcd10d7e"
    add_ssdp_ap(apdev[0], ap_uuid)

    location = ssdp_get_location(ap_uuid)
    url = urlparse(location)
    addr = (url.hostname, url.port)
    logger.debug("Open HTTP connection to hostapd, but do not complete request")
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM,
                         socket.IPPROTO_TCP)
    sock.connect(addr)
    sock.send(b"G")

    class DummyServer(StreamRequestHandler):
        def handle(self):
            logger.debug("DummyServer - start 31 sec wait")
            time.sleep(31)
            logger.debug("DummyServer - wait done")

    logger.debug("Start WPS ER")
    server, sock2 = wps_er_start(dev[0], DummyServer, max_age=40,
                                 wait_m_search=True)

    logger.debug("Start server to accept, but not complete, HTTP connection from WPS ER")
    # This will wait for 31 seconds..
    server.handle_request()

    logger.debug("Complete HTTP connection with hostapd (that should have already closed the connection)")
    try:
        sock.send("ET / HTTP/1.1\r\n\r\n")
        res = sock.recv(100)
        sock.close()
    except:
        pass

def test_ap_wps_er_url_parse(dev, apdev):
    """WPS ER and URL parsing special cases"""
    try:
        _test_ap_wps_er_url_parse(dev, apdev)
    finally:
        dev[0].request("WPS_ER_STOP")

def _test_ap_wps_er_url_parse(dev, apdev):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.settimeout(1)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(("239.255.255.250", 1900))
    dev[0].request("WPS_ER_START ifname=lo")
    (msg, addr) = sock.recvfrom(1000)
    msg = msg.decode()
    logger.debug("Received SSDP message from %s: %s" % (str(addr), msg))
    if "M-SEARCH" not in msg:
        raise Exception("Not an M-SEARCH")
    sock.sendto(b"HTTP/1.1 200 OK\r\nST: urn:schemas-wifialliance-org:device:WFADevice:1\r\nlocation:http://127.0.0.1\r\ncache-control:max-age=1\r\n\r\n", addr)
    ev = dev[0].wait_event(["WPS-ER-AP-REMOVE"], timeout=2)
    sock.sendto(b"HTTP/1.1 200 OK\r\nST: urn:schemas-wifialliance-org:device:WFADevice:1\r\nlocation:http://127.0.0.1/:foo\r\ncache-control:max-age=1\r\n\r\n", addr)
    ev = dev[0].wait_event(["WPS-ER-AP-REMOVE"], timeout=2)
    sock.sendto(b"HTTP/1.1 200 OK\r\nST: urn:schemas-wifialliance-org:device:WFADevice:1\r\nlocation:http://255.255.255.255:0/foo.xml\r\ncache-control:max-age=1\r\n\r\n", addr)
    ev = dev[0].wait_event(["WPS-ER-AP-REMOVE"], timeout=2)

    sock.close()

def test_ap_wps_er_link_update(dev, apdev):
    """WPS ER and link update special cases"""
    class WPSAPHTTPServer_link_update(WPSAPHTTPServer):
        def handle_upnp_info(self):
            self.wfile.write(gen_upnp_info(controlURL='/wps_control'))
    run_wps_er_proto_test(dev[0], WPSAPHTTPServer_link_update)

    class WPSAPHTTPServer_link_update2(WPSAPHTTPServer):
        def handle_others(self, data):
            if "GET / " in data:
                self.wfile.write(gen_upnp_info(controlURL='/wps_control'))
    run_wps_er_proto_test(dev[0], WPSAPHTTPServer_link_update2,
                          location_url='http://127.0.0.1:12345')

def test_ap_wps_er_http_client(dev, apdev):
    """WPS ER and HTTP client special cases"""
    with alloc_fail(dev[0], 1, "http_link_update"):
        run_wps_er_proto_test(dev[0], WPSAPHTTPServer)

    with alloc_fail(dev[0], 1, "wpabuf_alloc;http_client_url"):
        run_wps_er_proto_test(dev[0], WPSAPHTTPServer, no_event_url=True)

    with alloc_fail(dev[0], 1, "httpread_create;http_client_tx_ready"):
        run_wps_er_proto_test(dev[0], WPSAPHTTPServer, no_event_url=True)

    class WPSAPHTTPServer_req_as_resp(WPSAPHTTPServer):
        def handle_upnp_info(self):
            self.wfile.write(b"GET / HTTP/1.1\r\n\r\n")
    run_wps_er_proto_test(dev[0], WPSAPHTTPServer_req_as_resp,
                          no_event_url=True)

def test_ap_wps_init_oom(dev, apdev):
    """wps_init OOM cases"""
    ssid = "test-wps"
    appin = "12345670"
    params = {"ssid": ssid, "eap_server": "1", "wps_state": "2",
              "ap_pin": appin}
    hapd = hostapd.add_ap(apdev[0], params)
    pin = dev[0].wps_read_pin()

    with alloc_fail(hapd, 1, "wps_init"):
        hapd.request("WPS_PIN any " + pin)
        dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
        dev[0].request("WPS_PIN %s %s" % (apdev[0]['bssid'], pin))
        ev = hapd.wait_event(["CTRL-EVENT-EAP-FAILURE"], timeout=10)
        if ev is None:
            raise Exception("No EAP failure reported")
        dev[0].request("WPS_CANCEL")

    with alloc_fail(dev[0], 2, "wps_init"):
        hapd.request("WPS_PIN any " + pin)
        dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
        dev[0].request("WPS_PIN %s %s" % (apdev[0]['bssid'], pin))
        ev = hapd.wait_event(["CTRL-EVENT-EAP-FAILURE"], timeout=10)
        if ev is None:
            raise Exception("No EAP failure reported")
        dev[0].request("WPS_CANCEL")

    with alloc_fail(dev[0], 2, "wps_init"):
        hapd.request("WPS_PBC")
        dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
        dev[0].request("WPS_PBC %s" % (apdev[0]['bssid']))
        ev = hapd.wait_event(["CTRL-EVENT-EAP-FAILURE"], timeout=10)
        if ev is None:
            raise Exception("No EAP failure reported")
        dev[0].request("WPS_CANCEL")

    dev[0].dump_monitor()
    new_ssid = "wps-new-ssid"
    new_passphrase = "1234567890"
    with alloc_fail(dev[0], 3, "wps_init"):
        dev[0].wps_reg(apdev[0]['bssid'], appin, new_ssid, "WPA2PSK", "CCMP",
                       new_passphrase, no_wait=True)
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"], timeout=10)
        if ev is None:
            raise Exception("No EAP failure reported")

    dev[0].flush_scan_cache()

@remote_compatible
def test_ap_wps_invalid_assoc_req_elem(dev, apdev):
    """WPS and invalid IE in Association Request frame"""
    ssid = "test-wps"
    params = {"ssid": ssid, "eap_server": "1", "wps_state": "2"}
    hapd = hostapd.add_ap(apdev[0], params)
    pin = "12345670"
    hapd.request("WPS_PIN any " + pin)
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    try:
        dev[0].request("VENDOR_ELEM_ADD 13 dd050050f20410")
        dev[0].request("WPS_PIN %s %s" % (apdev[0]['bssid'], pin))
        for i in range(5):
            ev = hapd.wait_event(["CTRL-EVENT-EAP-PROPOSED-METHOD"], timeout=10)
            if ev and "vendor=14122" in ev:
                break
        if ev is None or "vendor=14122" not in ev:
            raise Exception("EAP-WSC not started")
        dev[0].request("WPS_CANCEL")
    finally:
        dev[0].request("VENDOR_ELEM_REMOVE 13 *")

def test_ap_wps_pbc_pin_mismatch(dev, apdev):
    """WPS PBC/PIN mismatch"""
    ssid = "test-wps"
    params = {"ssid": ssid, "eap_server": "1", "wps_state": "2"}
    hapd = hostapd.add_ap(apdev[0], params)
    hapd.request("SET wps_version_number 0x10")
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    hapd.request("WPS_PBC")
    pin = dev[0].wps_read_pin()
    dev[0].request("WPS_PIN %s %s" % (apdev[0]['bssid'], pin))
    ev = dev[0].wait_event(["CTRL-EVENT-SCAN-RESULTS"])
    if ev is None:
        raise Exception("Scan did not complete")
    dev[0].request("WPS_CANCEL")

    hapd.request("WPS_CANCEL")
    dev[0].flush_scan_cache()

@remote_compatible
def test_ap_wps_ie_invalid(dev, apdev):
    """WPS PIN attempt with AP that has invalid WSC IE"""
    ssid = "test-wps"
    params = {"ssid": ssid, "eap_server": "1", "wps_state": "2",
              "vendor_elements": "dd050050f20410"}
    hapd = hostapd.add_ap(apdev[0], params)
    params = {'ssid': "another", "vendor_elements": "dd050050f20410"}
    hostapd.add_ap(apdev[1], params)
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    pin = dev[0].wps_read_pin()
    dev[0].request("WPS_PIN %s %s" % (apdev[0]['bssid'], pin))
    ev = dev[0].wait_event(["CTRL-EVENT-SCAN-RESULTS"])
    if ev is None:
        raise Exception("Scan did not complete")
    dev[0].request("WPS_CANCEL")

@remote_compatible
def test_ap_wps_scan_prio_order(dev, apdev):
    """WPS scan priority ordering"""
    ssid = "test-wps"
    params = {"ssid": ssid, "eap_server": "1", "wps_state": "2"}
    hapd = hostapd.add_ap(apdev[0], params)
    params = {'ssid': "another", "vendor_elements": "dd050050f20410"}
    hostapd.add_ap(apdev[1], params)
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[0].scan_for_bss(apdev[1]['bssid'], freq="2412")
    pin = dev[0].wps_read_pin()
    dev[0].request("WPS_PIN %s %s" % (apdev[0]['bssid'], pin))
    ev = dev[0].wait_event(["CTRL-EVENT-SCAN-RESULTS"])
    if ev is None:
        raise Exception("Scan did not complete")
    dev[0].request("WPS_CANCEL")

def test_ap_wps_probe_req_ie_oom(dev, apdev):
    """WPS ProbeReq IE OOM"""
    ssid = "test-wps"
    params = {"ssid": ssid, "eap_server": "1", "wps_state": "2"}
    hapd = hostapd.add_ap(apdev[0], params)
    pin = dev[0].wps_read_pin()
    hapd.request("WPS_PIN any " + pin)
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    with alloc_fail(dev[0], 1, "wps_build_probe_req_ie"):
        dev[0].request("WPS_PIN %s %s" % (apdev[0]['bssid'], pin))
        ev = hapd.wait_event(["AP-STA-CONNECTED"], timeout=10)
        if ev is None:
            raise Exception("Association not seen")
    dev[0].request("WPS_CANCEL")
    dev[0].wait_disconnected()

    with alloc_fail(dev[0], 1, "wps_ie_encapsulate"):
        dev[0].request("WPS_PIN %s %s" % (apdev[0]['bssid'], pin))
        ev = hapd.wait_event(["AP-STA-CONNECTED"], timeout=10)
        if ev is None:
            raise Exception("Association not seen")
    dev[0].request("WPS_CANCEL")
    hapd.disable()
    dev[0].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected()
    time.sleep(0.2)
    dev[0].flush_scan_cache()

def test_ap_wps_assoc_req_ie_oom(dev, apdev):
    """WPS AssocReq IE OOM"""
    ssid = "test-wps"
    params = {"ssid": ssid, "eap_server": "1", "wps_state": "2"}
    hapd = hostapd.add_ap(apdev[0], params)
    pin = dev[0].wps_read_pin()
    hapd.request("WPS_PIN any " + pin)
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    with alloc_fail(dev[0], 1, "wps_build_assoc_req_ie"):
        dev[0].request("WPS_PIN %s %s" % (apdev[0]['bssid'], pin))
        ev = hapd.wait_event(["AP-STA-CONNECTED"], timeout=10)
        if ev is None:
            raise Exception("Association not seen")
    dev[0].request("WPS_CANCEL")

def test_ap_wps_assoc_resp_ie_oom(dev, apdev):
    """WPS AssocResp IE OOM"""
    ssid = "test-wps"
    params = {"ssid": ssid, "eap_server": "1", "wps_state": "2"}
    hapd = hostapd.add_ap(apdev[0], params)
    pin = dev[0].wps_read_pin()
    hapd.request("WPS_PIN any " + pin)
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    with alloc_fail(hapd, 1, "wps_build_assoc_resp_ie"):
        dev[0].request("WPS_PIN %s %s" % (apdev[0]['bssid'], pin))
        ev = hapd.wait_event(["AP-STA-CONNECTED"], timeout=10)
        if ev is None:
            raise Exception("Association not seen")
    dev[0].request("WPS_CANCEL")

@remote_compatible
def test_ap_wps_bss_info_errors(dev, apdev):
    """WPS BSS info errors"""
    params = {"ssid": "1",
              "vendor_elements": "dd0e0050f20410440001ff101100010a"}
    hostapd.add_ap(apdev[0], params)
    params = {'ssid': "2", "vendor_elements": "dd050050f20410"}
    hostapd.add_ap(apdev[1], params)
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[0].scan_for_bss(apdev[1]['bssid'], freq="2412")
    bss = dev[0].get_bss(apdev[0]['bssid'])
    logger.info("BSS: " + str(bss))
    if "wps_state" in bss:
        raise Exception("Unexpected wps_state in BSS info")
    if 'wps_device_name' not in bss:
        raise Exception("No wps_device_name in BSS info")
    if bss['wps_device_name'] != '_':
        raise Exception("Unexpected wps_device_name value")
    bss = dev[0].get_bss(apdev[1]['bssid'])
    logger.info("BSS: " + str(bss))

    with alloc_fail(dev[0], 1, "=wps_attr_text"):
        bss = dev[0].get_bss(apdev[0]['bssid'])
        logger.info("BSS(OOM): " + str(bss))

def wps_run_pbc_fail_ap(apdev, dev, hapd):
    hapd.request("WPS_PBC")
    dev.scan_for_bss(apdev['bssid'], freq="2412")
    dev.request("WPS_PBC " + apdev['bssid'])
    ev = dev.wait_event(["CTRL-EVENT-EAP-FAILURE"], timeout=10)
    if ev is None:
        raise Exception("No EAP failure reported")
    dev.request("WPS_CANCEL")
    dev.wait_disconnected()
    for i in range(5):
        try:
            dev.flush_scan_cache()
            break
        except Exception as e:
            if str(e).startswith("Failed to trigger scan"):
                # Try again
                time.sleep(1)
            else:
                raise

def wps_run_pbc_fail(apdev, dev):
    hapd = wps_start_ap(apdev)
    wps_run_pbc_fail_ap(apdev, dev, hapd)

@remote_compatible
def test_ap_wps_pk_oom(dev, apdev):
    """WPS and public key OOM"""
    with alloc_fail(dev[0], 1, "wps_build_public_key"):
        wps_run_pbc_fail(apdev[0], dev[0])

@remote_compatible
def test_ap_wps_pk_oom_ap(dev, apdev):
    """WPS and public key OOM on AP"""
    hapd = wps_start_ap(apdev[0])
    with alloc_fail(hapd, 1, "wps_build_public_key"):
        wps_run_pbc_fail_ap(apdev[0], dev[0], hapd)

@remote_compatible
def test_ap_wps_encr_oom_ap(dev, apdev):
    """WPS and encrypted settings decryption OOM on AP"""
    hapd = wps_start_ap(apdev[0])
    pin = dev[0].wps_read_pin()
    hapd.request("WPS_PIN any " + pin)
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    with alloc_fail(hapd, 1, "wps_decrypt_encr_settings"):
        dev[0].request("WPS_PIN " + apdev[0]['bssid'] + " " + pin)
        ev = hapd.wait_event(["WPS-FAIL"], timeout=10)
        if ev is None:
            raise Exception("No WPS-FAIL reported")
        dev[0].request("WPS_CANCEL")
    dev[0].wait_disconnected()

@remote_compatible
def test_ap_wps_encr_no_random_ap(dev, apdev):
    """WPS and no random data available for encryption on AP"""
    hapd = wps_start_ap(apdev[0])
    with fail_test(hapd, 1, "os_get_random;wps_build_encr_settings"):
        wps_run_pbc_fail_ap(apdev[0], dev[0], hapd)

@remote_compatible
def test_ap_wps_e_hash_no_random_sta(dev, apdev):
    """WPS and no random data available for e-hash on STA"""
    with fail_test(dev[0], 1, "os_get_random;wps_build_e_hash"):
        wps_run_pbc_fail(apdev[0], dev[0])

@remote_compatible
def test_ap_wps_m1_no_random(dev, apdev):
    """WPS and no random for M1 on STA"""
    with fail_test(dev[0], 1, "os_get_random;wps_build_m1"):
        wps_run_pbc_fail(apdev[0], dev[0])

@remote_compatible
def test_ap_wps_m1_oom(dev, apdev):
    """WPS and OOM for M1 on STA"""
    with alloc_fail(dev[0], 1, "wps_build_m1"):
        wps_run_pbc_fail(apdev[0], dev[0])

@remote_compatible
def test_ap_wps_m3_oom(dev, apdev):
    """WPS and OOM for M3 on STA"""
    with alloc_fail(dev[0], 1, "wps_build_m3"):
        wps_run_pbc_fail(apdev[0], dev[0])

@remote_compatible
def test_ap_wps_m5_oom(dev, apdev):
    """WPS and OOM for M5 on STA"""
    hapd = wps_start_ap(apdev[0])
    hapd.request("WPS_PBC")
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    for i in range(1, 3):
        with alloc_fail(dev[0], i, "wps_build_m5"):
            dev[0].request("WPS_PBC " + apdev[0]['bssid'])
            ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"], timeout=10)
            if ev is None:
                raise Exception("No EAP failure reported")
            dev[0].request("WPS_CANCEL")
            dev[0].wait_disconnected()
    dev[0].flush_scan_cache()

@remote_compatible
def test_ap_wps_m5_no_random(dev, apdev):
    """WPS and no random for M5 on STA"""
    with fail_test(dev[0], 1,
                   "os_get_random;wps_build_encr_settings;wps_build_m5"):
        wps_run_pbc_fail(apdev[0], dev[0])

@remote_compatible
def test_ap_wps_m7_oom(dev, apdev):
    """WPS and OOM for M7 on STA"""
    hapd = wps_start_ap(apdev[0])
    hapd.request("WPS_PBC")
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    for i in range(1, 3):
        with alloc_fail(dev[0], i, "wps_build_m7"):
            dev[0].request("WPS_PBC " + apdev[0]['bssid'])
            ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"], timeout=10)
            if ev is None:
                raise Exception("No EAP failure reported")
            dev[0].request("WPS_CANCEL")
            dev[0].wait_disconnected()
    dev[0].flush_scan_cache()

@remote_compatible
def test_ap_wps_m7_no_random(dev, apdev):
    """WPS and no random for M7 on STA"""
    with fail_test(dev[0], 1,
                   "os_get_random;wps_build_encr_settings;wps_build_m7"):
        wps_run_pbc_fail(apdev[0], dev[0])

@remote_compatible
def test_ap_wps_wsc_done_oom(dev, apdev):
    """WPS and OOM for WSC_Done on STA"""
    with alloc_fail(dev[0], 1, "wps_build_wsc_done"):
        wps_run_pbc_fail(apdev[0], dev[0])

def test_ap_wps_random_psk_fail(dev, apdev):
    """WPS and no random for PSK on AP"""
    ssid = "test-wps"
    pskfile = "/tmp/ap_wps_per_enrollee_psk.psk_file"
    appin = "12345670"
    try:
        os.remove(pskfile)
    except:
        pass

    try:
        with open(pskfile, "w") as f:
            f.write("# WPA PSKs\n")

        params = {"ssid": ssid, "eap_server": "1", "wps_state": "2",
                  "wpa": "2", "wpa_key_mgmt": "WPA-PSK",
                  "rsn_pairwise": "CCMP", "ap_pin": appin,
                  "wpa_psk_file": pskfile}
        hapd = hostapd.add_ap(apdev[0], params)

        dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
        with fail_test(hapd, 1, "os_get_random;wps_build_cred_network_key"):
            dev[0].request("WPS_REG " + apdev[0]['bssid'] + " " + appin)
            ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"], timeout=10)
            if ev is None:
                raise Exception("No EAP failure reported")
            dev[0].request("WPS_CANCEL")
        dev[0].wait_disconnected()

        with fail_test(hapd, 1, "os_get_random;wps_build_cred"):
            wps_run_pbc_fail_ap(apdev[0], dev[0], hapd)

        with alloc_fail(hapd, 1, "wps_build_cred"):
            wps_run_pbc_fail_ap(apdev[0], dev[0], hapd)

        with alloc_fail(hapd, 2, "wps_build_cred"):
            wps_run_pbc_fail_ap(apdev[0], dev[0], hapd)
    finally:
        os.remove(pskfile)

def wps_ext_eap_identity_req(dev, hapd, bssid):
    logger.debug("EAP-Identity/Request")
    ev = hapd.wait_event(["EAPOL-TX"], timeout=10)
    if ev is None:
        raise Exception("Timeout on EAPOL-TX from hostapd")
    res = dev.request("EAPOL_RX " + bssid + " " + ev.split(' ')[2])
    if "OK" not in res:
        raise Exception("EAPOL_RX to wpa_supplicant failed")

def wps_ext_eap_identity_resp(hapd, dev, addr):
    ev = dev.wait_event(["EAPOL-TX"], timeout=10)
    if ev is None:
        raise Exception("Timeout on EAPOL-TX from wpa_supplicant")
    res = hapd.request("EAPOL_RX " + addr + " " + ev.split(' ')[2])
    if "OK" not in res:
        raise Exception("EAPOL_RX to hostapd failed")

def wps_ext_eap_wsc(dst, src, src_addr, msg):
    logger.debug(msg)
    ev = src.wait_event(["EAPOL-TX"], timeout=10)
    if ev is None:
        raise Exception("Timeout on EAPOL-TX")
    res = dst.request("EAPOL_RX " + src_addr + " " + ev.split(' ')[2])
    if "OK" not in res:
        raise Exception("EAPOL_RX failed")

def wps_start_ext(apdev, dev, pbc=False, pin=None):
    addr = dev.own_addr()
    bssid = apdev['bssid']
    ssid = "test-wps-conf"
    params = {"ssid": ssid, "eap_server": "1", "wps_state": "2",
              "wpa_passphrase": "12345678", "wpa": "2",
              "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP"}
    hapd = hostapd.add_ap(apdev, params)

    if pbc:
        hapd.request("WPS_PBC")
    else:
        if pin is None:
            pin = dev.wps_read_pin()
        hapd.request("WPS_PIN any " + pin)
    dev.scan_for_bss(bssid, freq="2412")
    hapd.request("SET ext_eapol_frame_io 1")
    dev.request("SET ext_eapol_frame_io 1")

    if pbc:
        dev.request("WPS_PBC " + bssid)
    else:
        dev.request("WPS_PIN " + bssid + " " + pin)
    return addr, bssid, hapd

def wps_auth_corrupt(dst, src, addr):
    ev = src.wait_event(["EAPOL-TX"], timeout=10)
    if ev is None:
        raise Exception("Timeout on EAPOL-TX")
    src.request("SET ext_eapol_frame_io 0")
    dst.request("SET ext_eapol_frame_io 0")
    msg = ev.split(' ')[2]
    if msg[-24:-16] != '10050008':
        raise Exception("Could not find Authenticator attribute")
    # Corrupt Authenticator value
    msg = msg[:-1] + '%x' % ((int(msg[-1], 16) + 1) % 16)
    res = dst.request("EAPOL_RX " + addr + " " + msg)
    if "OK" not in res:
        raise Exception("EAPOL_RX failed")

def wps_fail_finish(hapd, dev, fail_str):
    ev = hapd.wait_event(["WPS-FAIL"], timeout=5)
    if ev is None:
        raise Exception("WPS-FAIL not indicated")
    if fail_str not in ev:
        raise Exception("Unexpected WPS-FAIL value: " + ev)
    dev.request("WPS_CANCEL")
    dev.wait_disconnected()

def wps_auth_corrupt_from_ap(dev, hapd, bssid, fail_str):
    wps_auth_corrupt(dev, hapd, bssid)
    wps_fail_finish(hapd, dev, fail_str)

def wps_auth_corrupt_to_ap(dev, hapd, addr, fail_str):
    wps_auth_corrupt(hapd, dev, addr)
    wps_fail_finish(hapd, dev, fail_str)

def test_ap_wps_authenticator_mismatch_m2(dev, apdev):
    """WPS and Authenticator attribute mismatch in M2"""
    addr, bssid, hapd = wps_start_ext(apdev[0], dev[0])
    wps_ext_eap_identity_req(dev[0], hapd, bssid)
    wps_ext_eap_identity_resp(hapd, dev[0], addr)
    wps_ext_eap_wsc(dev[0], hapd, bssid, "EAP-WSC/Start")
    wps_ext_eap_wsc(hapd, dev[0], addr, "M1")
    logger.debug("M2")
    wps_auth_corrupt_from_ap(dev[0], hapd, bssid, "msg=5")

def test_ap_wps_authenticator_mismatch_m3(dev, apdev):
    """WPS and Authenticator attribute mismatch in M3"""
    addr, bssid, hapd = wps_start_ext(apdev[0], dev[0])
    wps_ext_eap_identity_req(dev[0], hapd, bssid)
    wps_ext_eap_identity_resp(hapd, dev[0], addr)
    wps_ext_eap_wsc(dev[0], hapd, bssid, "EAP-WSC/Start")
    wps_ext_eap_wsc(hapd, dev[0], addr, "M1")
    wps_ext_eap_wsc(dev[0], hapd, bssid, "M2")
    logger.debug("M3")
    wps_auth_corrupt_to_ap(dev[0], hapd, addr, "msg=7")

def test_ap_wps_authenticator_mismatch_m4(dev, apdev):
    """WPS and Authenticator attribute mismatch in M4"""
    addr, bssid, hapd = wps_start_ext(apdev[0], dev[0])
    wps_ext_eap_identity_req(dev[0], hapd, bssid)
    wps_ext_eap_identity_resp(hapd, dev[0], addr)
    wps_ext_eap_wsc(dev[0], hapd, bssid, "EAP-WSC/Start")
    wps_ext_eap_wsc(hapd, dev[0], addr, "M1")
    wps_ext_eap_wsc(dev[0], hapd, bssid, "M2")
    wps_ext_eap_wsc(hapd, dev[0], addr, "M3")
    logger.debug("M4")
    wps_auth_corrupt_from_ap(dev[0], hapd, bssid, "msg=8")

def test_ap_wps_authenticator_mismatch_m5(dev, apdev):
    """WPS and Authenticator attribute mismatch in M5"""
    addr, bssid, hapd = wps_start_ext(apdev[0], dev[0])
    wps_ext_eap_identity_req(dev[0], hapd, bssid)
    wps_ext_eap_identity_resp(hapd, dev[0], addr)
    wps_ext_eap_wsc(dev[0], hapd, bssid, "EAP-WSC/Start")
    wps_ext_eap_wsc(hapd, dev[0], addr, "M1")
    wps_ext_eap_wsc(dev[0], hapd, bssid, "M2")
    wps_ext_eap_wsc(hapd, dev[0], addr, "M3")
    wps_ext_eap_wsc(dev[0], hapd, bssid, "M4")
    logger.debug("M5")
    wps_auth_corrupt_to_ap(dev[0], hapd, addr, "msg=9")

def test_ap_wps_authenticator_mismatch_m6(dev, apdev):
    """WPS and Authenticator attribute mismatch in M6"""
    addr, bssid, hapd = wps_start_ext(apdev[0], dev[0])
    wps_ext_eap_identity_req(dev[0], hapd, bssid)
    wps_ext_eap_identity_resp(hapd, dev[0], addr)
    wps_ext_eap_wsc(dev[0], hapd, bssid, "EAP-WSC/Start")
    wps_ext_eap_wsc(hapd, dev[0], addr, "M1")
    wps_ext_eap_wsc(dev[0], hapd, bssid, "M2")
    wps_ext_eap_wsc(hapd, dev[0], addr, "M3")
    wps_ext_eap_wsc(dev[0], hapd, bssid, "M4")
    wps_ext_eap_wsc(hapd, dev[0], addr, "M5")
    logger.debug("M6")
    wps_auth_corrupt_from_ap(dev[0], hapd, bssid, "msg=10")

def test_ap_wps_authenticator_mismatch_m7(dev, apdev):
    """WPS and Authenticator attribute mismatch in M7"""
    addr, bssid, hapd = wps_start_ext(apdev[0], dev[0])
    wps_ext_eap_identity_req(dev[0], hapd, bssid)
    wps_ext_eap_identity_resp(hapd, dev[0], addr)
    wps_ext_eap_wsc(dev[0], hapd, bssid, "EAP-WSC/Start")
    wps_ext_eap_wsc(hapd, dev[0], addr, "M1")
    wps_ext_eap_wsc(dev[0], hapd, bssid, "M2")
    wps_ext_eap_wsc(hapd, dev[0], addr, "M3")
    wps_ext_eap_wsc(dev[0], hapd, bssid, "M4")
    wps_ext_eap_wsc(hapd, dev[0], addr, "M5")
    wps_ext_eap_wsc(dev[0], hapd, bssid, "M6")
    logger.debug("M7")
    wps_auth_corrupt_to_ap(dev[0], hapd, addr, "msg=11")

def test_ap_wps_authenticator_mismatch_m8(dev, apdev):
    """WPS and Authenticator attribute mismatch in M8"""
    addr, bssid, hapd = wps_start_ext(apdev[0], dev[0])
    wps_ext_eap_identity_req(dev[0], hapd, bssid)
    wps_ext_eap_identity_resp(hapd, dev[0], addr)
    wps_ext_eap_wsc(dev[0], hapd, bssid, "EAP-WSC/Start")
    wps_ext_eap_wsc(hapd, dev[0], addr, "M1")
    wps_ext_eap_wsc(dev[0], hapd, bssid, "M2")
    wps_ext_eap_wsc(hapd, dev[0], addr, "M3")
    wps_ext_eap_wsc(dev[0], hapd, bssid, "M4")
    wps_ext_eap_wsc(hapd, dev[0], addr, "M5")
    wps_ext_eap_wsc(dev[0], hapd, bssid, "M6")
    wps_ext_eap_wsc(hapd, dev[0], addr, "M7")
    logger.debug("M8")
    wps_auth_corrupt_from_ap(dev[0], hapd, bssid, "msg=12")

def test_ap_wps_authenticator_missing_m2(dev, apdev):
    """WPS and Authenticator attribute missing from M2"""
    addr, bssid, hapd = wps_start_ext(apdev[0], dev[0])
    wps_ext_eap_identity_req(dev[0], hapd, bssid)
    wps_ext_eap_identity_resp(hapd, dev[0], addr)
    wps_ext_eap_wsc(dev[0], hapd, bssid, "EAP-WSC/Start")
    wps_ext_eap_wsc(hapd, dev[0], addr, "M1")
    logger.debug("M2")
    ev = hapd.wait_event(["EAPOL-TX"], timeout=10)
    if ev is None:
        raise Exception("Timeout on EAPOL-TX")
    hapd.request("SET ext_eapol_frame_io 0")
    dev[0].request("SET ext_eapol_frame_io 0")
    msg = ev.split(' ')[2]
    if msg[-24:-16] != '10050008':
        raise Exception("Could not find Authenticator attribute")
    # Remove Authenticator value
    msg = msg[:-24]
    mlen = "%04x" % (int(msg[4:8], 16) - 12)
    msg = msg[0:4] + mlen + msg[8:12] + mlen + msg[16:]
    res = dev[0].request("EAPOL_RX " + bssid + " " + msg)
    if "OK" not in res:
        raise Exception("EAPOL_RX failed")
    wps_fail_finish(hapd, dev[0], "msg=5")

def test_ap_wps_m2_dev_passwd_id_p2p(dev, apdev):
    """WPS and M2 with different Device Password ID (P2P)"""
    addr, bssid, hapd = wps_start_ext(apdev[0], dev[0])
    wps_ext_eap_identity_req(dev[0], hapd, bssid)
    wps_ext_eap_identity_resp(hapd, dev[0], addr)
    wps_ext_eap_wsc(dev[0], hapd, bssid, "EAP-WSC/Start")
    wps_ext_eap_wsc(hapd, dev[0], addr, "M1")
    logger.debug("M2")
    ev = hapd.wait_event(["EAPOL-TX"], timeout=10)
    if ev is None:
        raise Exception("Timeout on EAPOL-TX")
    hapd.request("SET ext_eapol_frame_io 0")
    dev[0].request("SET ext_eapol_frame_io 0")
    msg = ev.split(' ')[2]
    if msg[722:730] != '10120002':
        raise Exception("Could not find Device Password ID attribute")
    # Replace Device Password ID value. This will fail Authenticator check, but
    # allows the code path in wps_process_dev_pw_id() to be checked from debug
    # log.
    msg = msg[0:730] + "0005" + msg[734:]
    res = dev[0].request("EAPOL_RX " + bssid + " " + msg)
    if "OK" not in res:
        raise Exception("EAPOL_RX failed")
    wps_fail_finish(hapd, dev[0], "msg=5")

def test_ap_wps_m2_dev_passwd_id_change_pin_to_pbc(dev, apdev):
    """WPS and M2 with different Device Password ID (PIN to PBC)"""
    addr, bssid, hapd = wps_start_ext(apdev[0], dev[0])
    wps_ext_eap_identity_req(dev[0], hapd, bssid)
    wps_ext_eap_identity_resp(hapd, dev[0], addr)
    wps_ext_eap_wsc(dev[0], hapd, bssid, "EAP-WSC/Start")
    wps_ext_eap_wsc(hapd, dev[0], addr, "M1")
    logger.debug("M2")
    ev = hapd.wait_event(["EAPOL-TX"], timeout=10)
    if ev is None:
        raise Exception("Timeout on EAPOL-TX")
    hapd.request("SET ext_eapol_frame_io 0")
    dev[0].request("SET ext_eapol_frame_io 0")
    msg = ev.split(' ')[2]
    if msg[722:730] != '10120002':
        raise Exception("Could not find Device Password ID attribute")
    # Replace Device Password ID value (PIN --> PBC). This will be rejected.
    msg = msg[0:730] + "0004" + msg[734:]
    res = dev[0].request("EAPOL_RX " + bssid + " " + msg)
    if "OK" not in res:
        raise Exception("EAPOL_RX failed")
    wps_fail_finish(hapd, dev[0], "msg=5")

def test_ap_wps_m2_dev_passwd_id_change_pbc_to_pin(dev, apdev):
    """WPS and M2 with different Device Password ID (PBC to PIN)"""
    addr, bssid, hapd = wps_start_ext(apdev[0], dev[0], pbc=True)
    wps_ext_eap_identity_req(dev[0], hapd, bssid)
    wps_ext_eap_identity_resp(hapd, dev[0], addr)
    wps_ext_eap_wsc(dev[0], hapd, bssid, "EAP-WSC/Start")
    wps_ext_eap_wsc(hapd, dev[0], addr, "M1")
    logger.debug("M2")
    ev = hapd.wait_event(["EAPOL-TX"], timeout=10)
    if ev is None:
        raise Exception("Timeout on EAPOL-TX")
    hapd.request("SET ext_eapol_frame_io 0")
    dev[0].request("SET ext_eapol_frame_io 0")
    msg = ev.split(' ')[2]
    if msg[722:730] != '10120002':
        raise Exception("Could not find Device Password ID attribute")
    # Replace Device Password ID value. This will fail Authenticator check, but
    # allows the code path in wps_process_dev_pw_id() to be checked from debug
    # log.
    msg = msg[0:730] + "0000" + msg[734:]
    res = dev[0].request("EAPOL_RX " + bssid + " " + msg)
    if "OK" not in res:
        raise Exception("EAPOL_RX failed")
    wps_fail_finish(hapd, dev[0], "msg=5")
    dev[0].flush_scan_cache()

def test_ap_wps_m2_missing_dev_passwd_id(dev, apdev):
    """WPS and M2 without Device Password ID"""
    addr, bssid, hapd = wps_start_ext(apdev[0], dev[0])
    wps_ext_eap_identity_req(dev[0], hapd, bssid)
    wps_ext_eap_identity_resp(hapd, dev[0], addr)
    wps_ext_eap_wsc(dev[0], hapd, bssid, "EAP-WSC/Start")
    wps_ext_eap_wsc(hapd, dev[0], addr, "M1")
    logger.debug("M2")
    ev = hapd.wait_event(["EAPOL-TX"], timeout=10)
    if ev is None:
        raise Exception("Timeout on EAPOL-TX")
    hapd.request("SET ext_eapol_frame_io 0")
    dev[0].request("SET ext_eapol_frame_io 0")
    msg = ev.split(' ')[2]
    if msg[722:730] != '10120002':
        raise Exception("Could not find Device Password ID attribute")
    # Remove Device Password ID value. This will fail Authenticator check, but
    # allows the code path in wps_process_dev_pw_id() to be checked from debug
    # log.
    mlen = "%04x" % (int(msg[4:8], 16) - 6)
    msg = msg[0:4] + mlen + msg[8:12] + mlen + msg[16:722] + msg[734:]
    res = dev[0].request("EAPOL_RX " + bssid + " " + msg)
    if "OK" not in res:
        raise Exception("EAPOL_RX failed")
    wps_fail_finish(hapd, dev[0], "msg=5")

def test_ap_wps_m2_missing_registrar_nonce(dev, apdev):
    """WPS and M2 without Registrar Nonce"""
    addr, bssid, hapd = wps_start_ext(apdev[0], dev[0], pbc=True)
    wps_ext_eap_identity_req(dev[0], hapd, bssid)
    wps_ext_eap_identity_resp(hapd, dev[0], addr)
    wps_ext_eap_wsc(dev[0], hapd, bssid, "EAP-WSC/Start")
    wps_ext_eap_wsc(hapd, dev[0], addr, "M1")
    logger.debug("M2")
    ev = hapd.wait_event(["EAPOL-TX"], timeout=10)
    if ev is None:
        raise Exception("Timeout on EAPOL-TX")
    hapd.request("SET ext_eapol_frame_io 0")
    dev[0].request("SET ext_eapol_frame_io 0")
    msg = ev.split(' ')[2]
    if msg[96:104] != '10390010':
        raise Exception("Could not find Registrar Nonce attribute")
    # Remove Registrar Nonce. This will fail Authenticator check, but
    # allows the code path in wps_process_registrar_nonce() to be checked from
    # the debug log.
    mlen = "%04x" % (int(msg[4:8], 16) - 20)
    msg = msg[0:4] + mlen + msg[8:12] + mlen + msg[16:96] + msg[136:]
    res = dev[0].request("EAPOL_RX " + bssid + " " + msg)
    if "OK" not in res:
        raise Exception("EAPOL_RX failed")
    ev = dev[0].wait_event(["CTRL-EVENT-DISCONNECT"], timeout=5)
    if ev is None:
        raise Exception("Disconnect event not seen")
    dev[0].request("WPS_CANCEL")
    dev[0].flush_scan_cache()

def test_ap_wps_m2_missing_enrollee_nonce(dev, apdev):
    """WPS and M2 without Enrollee Nonce"""
    addr, bssid, hapd = wps_start_ext(apdev[0], dev[0], pbc=True)
    wps_ext_eap_identity_req(dev[0], hapd, bssid)
    wps_ext_eap_identity_resp(hapd, dev[0], addr)
    wps_ext_eap_wsc(dev[0], hapd, bssid, "EAP-WSC/Start")
    wps_ext_eap_wsc(hapd, dev[0], addr, "M1")
    logger.debug("M2")
    ev = hapd.wait_event(["EAPOL-TX"], timeout=10)
    if ev is None:
        raise Exception("Timeout on EAPOL-TX")
    hapd.request("SET ext_eapol_frame_io 0")
    dev[0].request("SET ext_eapol_frame_io 0")
    msg = ev.split(' ')[2]
    if msg[56:64] != '101a0010':
        raise Exception("Could not find enrollee Nonce attribute")
    # Remove Enrollee Nonce. This will fail Authenticator check, but
    # allows the code path in wps_process_enrollee_nonce() to be checked from
    # the debug log.
    mlen = "%04x" % (int(msg[4:8], 16) - 20)
    msg = msg[0:4] + mlen + msg[8:12] + mlen + msg[16:56] + msg[96:]
    res = dev[0].request("EAPOL_RX " + bssid + " " + msg)
    if "OK" not in res:
        raise Exception("EAPOL_RX failed")
    ev = dev[0].wait_event(["CTRL-EVENT-DISCONNECT"], timeout=5)
    if ev is None:
        raise Exception("Disconnect event not seen")
    dev[0].request("WPS_CANCEL")
    dev[0].flush_scan_cache()

def test_ap_wps_m2_missing_uuid_r(dev, apdev):
    """WPS and M2 without UUID-R"""
    addr, bssid, hapd = wps_start_ext(apdev[0], dev[0], pbc=True)
    wps_ext_eap_identity_req(dev[0], hapd, bssid)
    wps_ext_eap_identity_resp(hapd, dev[0], addr)
    wps_ext_eap_wsc(dev[0], hapd, bssid, "EAP-WSC/Start")
    wps_ext_eap_wsc(hapd, dev[0], addr, "M1")
    logger.debug("M2")
    ev = hapd.wait_event(["EAPOL-TX"], timeout=10)
    if ev is None:
        raise Exception("Timeout on EAPOL-TX")
    hapd.request("SET ext_eapol_frame_io 0")
    dev[0].request("SET ext_eapol_frame_io 0")
    msg = ev.split(' ')[2]
    if msg[136:144] != '10480010':
        raise Exception("Could not find enrollee Nonce attribute")
    # Remove UUID-R. This will fail Authenticator check, but allows the code
    # path in wps_process_uuid_r() to be checked from the debug log.
    mlen = "%04x" % (int(msg[4:8], 16) - 20)
    msg = msg[0:4] + mlen + msg[8:12] + mlen + msg[16:136] + msg[176:]
    res = dev[0].request("EAPOL_RX " + bssid + " " + msg)
    if "OK" not in res:
        raise Exception("EAPOL_RX failed")
    ev = dev[0].wait_event(["CTRL-EVENT-DISCONNECT"], timeout=5)
    if ev is None:
        raise Exception("Disconnect event not seen")
    dev[0].request("WPS_CANCEL")
    dev[0].flush_scan_cache()

def test_ap_wps_m2_invalid(dev, apdev):
    """WPS and M2 parsing failure"""
    addr, bssid, hapd = wps_start_ext(apdev[0], dev[0], pbc=True)
    wps_ext_eap_identity_req(dev[0], hapd, bssid)
    wps_ext_eap_identity_resp(hapd, dev[0], addr)
    wps_ext_eap_wsc(dev[0], hapd, bssid, "EAP-WSC/Start")
    wps_ext_eap_wsc(hapd, dev[0], addr, "M1")
    logger.debug("M2")
    ev = hapd.wait_event(["EAPOL-TX"], timeout=10)
    if ev is None:
        raise Exception("Timeout on EAPOL-TX")
    hapd.request("SET ext_eapol_frame_io 0")
    dev[0].request("SET ext_eapol_frame_io 0")
    msg = ev.split(' ')[2]
    if msg[136:144] != '10480010':
        raise Exception("Could not find enrollee Nonce attribute")
    # Remove UUID-R. This will fail Authenticator check, but allows the code
    # path in wps_process_uuid_r() to be checked from the debug log.
    mlen = "%04x" % (int(msg[4:8], 16) - 1)
    msg = msg[0:4] + mlen + msg[8:12] + mlen + msg[16:-2]
    res = dev[0].request("EAPOL_RX " + bssid + " " + msg)
    if "OK" not in res:
        raise Exception("EAPOL_RX failed")
    ev = dev[0].wait_event(["CTRL-EVENT-DISCONNECT"], timeout=5)
    if ev is None:
        raise Exception("Disconnect event not seen")
    dev[0].request("WPS_CANCEL")
    dev[0].flush_scan_cache()

def test_ap_wps_m2_missing_msg_type(dev, apdev):
    """WPS and M2 without Message Type"""
    addr, bssid, hapd = wps_start_ext(apdev[0], dev[0], pbc=True)
    wps_ext_eap_identity_req(dev[0], hapd, bssid)
    wps_ext_eap_identity_resp(hapd, dev[0], addr)
    wps_ext_eap_wsc(dev[0], hapd, bssid, "EAP-WSC/Start")
    wps_ext_eap_wsc(hapd, dev[0], addr, "M1")
    logger.debug("M2")
    ev = hapd.wait_event(["EAPOL-TX"], timeout=10)
    if ev is None:
        raise Exception("Timeout on EAPOL-TX")
    hapd.request("SET ext_eapol_frame_io 0")
    dev[0].request("SET ext_eapol_frame_io 0")
    msg = ev.split(' ')[2]
    if msg[46:54] != '10220001':
        raise Exception("Could not find Message Type attribute")
    # Remove Message Type. This will fail Authenticator check, but allows the
    # code path in wps_process_wsc_msg() to be checked from the debug log.
    mlen = "%04x" % (int(msg[4:8], 16) - 5)
    msg = msg[0:4] + mlen + msg[8:12] + mlen + msg[16:46] + msg[56:]
    res = dev[0].request("EAPOL_RX " + bssid + " " + msg)
    if "OK" not in res:
        raise Exception("EAPOL_RX failed")
    ev = dev[0].wait_event(["CTRL-EVENT-DISCONNECT"], timeout=5)
    if ev is None:
        raise Exception("Disconnect event not seen")
    dev[0].request("WPS_CANCEL")
    dev[0].flush_scan_cache()

def test_ap_wps_m2_unknown_msg_type(dev, apdev):
    """WPS and M2 but unknown Message Type"""
    addr, bssid, hapd = wps_start_ext(apdev[0], dev[0], pbc=True)
    wps_ext_eap_identity_req(dev[0], hapd, bssid)
    wps_ext_eap_identity_resp(hapd, dev[0], addr)
    wps_ext_eap_wsc(dev[0], hapd, bssid, "EAP-WSC/Start")
    wps_ext_eap_wsc(hapd, dev[0], addr, "M1")
    logger.debug("M2")
    ev = hapd.wait_event(["EAPOL-TX"], timeout=10)
    if ev is None:
        raise Exception("Timeout on EAPOL-TX")
    hapd.request("SET ext_eapol_frame_io 0")
    dev[0].request("SET ext_eapol_frame_io 0")
    msg = ev.split(' ')[2]
    if msg[46:54] != '10220001':
        raise Exception("Could not find Message Type attribute")
    # Replace Message Type value. This will be rejected.
    msg = msg[0:54] + "00" + msg[56:]
    res = dev[0].request("EAPOL_RX " + bssid + " " + msg)
    if "OK" not in res:
        raise Exception("EAPOL_RX failed")
    ev = dev[0].wait_event(["CTRL-EVENT-DISCONNECT"], timeout=5)
    if ev is None:
        raise Exception("Disconnect event not seen")
    dev[0].request("WPS_CANCEL")
    dev[0].flush_scan_cache()

def test_ap_wps_m2_unknown_opcode(dev, apdev):
    """WPS and M2 but unknown opcode"""
    addr, bssid, hapd = wps_start_ext(apdev[0], dev[0], pbc=True)
    wps_ext_eap_identity_req(dev[0], hapd, bssid)
    wps_ext_eap_identity_resp(hapd, dev[0], addr)
    wps_ext_eap_wsc(dev[0], hapd, bssid, "EAP-WSC/Start")
    wps_ext_eap_wsc(hapd, dev[0], addr, "M1")
    logger.debug("M2")
    ev = hapd.wait_event(["EAPOL-TX"], timeout=10)
    if ev is None:
        raise Exception("Timeout on EAPOL-TX")
    hapd.request("SET ext_eapol_frame_io 0")
    dev[0].request("SET ext_eapol_frame_io 0")
    msg = ev.split(' ')[2]
    # Replace opcode. This will be discarded in EAP-WSC processing.
    msg = msg[0:32] + "00" + msg[34:]
    res = dev[0].request("EAPOL_RX " + bssid + " " + msg)
    if "OK" not in res:
        raise Exception("EAPOL_RX failed")
    dev[0].request("WPS_CANCEL")
    dev[0].wait_disconnected()
    dev[0].flush_scan_cache()

def test_ap_wps_m2_unknown_opcode2(dev, apdev):
    """WPS and M2 but unknown opcode (WSC_Start)"""
    addr, bssid, hapd = wps_start_ext(apdev[0], dev[0], pbc=True)
    wps_ext_eap_identity_req(dev[0], hapd, bssid)
    wps_ext_eap_identity_resp(hapd, dev[0], addr)
    wps_ext_eap_wsc(dev[0], hapd, bssid, "EAP-WSC/Start")
    wps_ext_eap_wsc(hapd, dev[0], addr, "M1")
    logger.debug("M2")
    ev = hapd.wait_event(["EAPOL-TX"], timeout=10)
    if ev is None:
        raise Exception("Timeout on EAPOL-TX")
    hapd.request("SET ext_eapol_frame_io 0")
    dev[0].request("SET ext_eapol_frame_io 0")
    msg = ev.split(' ')[2]
    # Replace opcode. This will be discarded in EAP-WSC processing.
    msg = msg[0:32] + "01" + msg[34:]
    res = dev[0].request("EAPOL_RX " + bssid + " " + msg)
    if "OK" not in res:
        raise Exception("EAPOL_RX failed")
    dev[0].request("WPS_CANCEL")
    dev[0].wait_disconnected()
    dev[0].flush_scan_cache()

def test_ap_wps_m2_unknown_opcode3(dev, apdev):
    """WPS and M2 but unknown opcode (WSC_Done)"""
    addr, bssid, hapd = wps_start_ext(apdev[0], dev[0], pbc=True)
    wps_ext_eap_identity_req(dev[0], hapd, bssid)
    wps_ext_eap_identity_resp(hapd, dev[0], addr)
    wps_ext_eap_wsc(dev[0], hapd, bssid, "EAP-WSC/Start")
    wps_ext_eap_wsc(hapd, dev[0], addr, "M1")
    logger.debug("M2")
    ev = hapd.wait_event(["EAPOL-TX"], timeout=10)
    if ev is None:
        raise Exception("Timeout on EAPOL-TX")
    hapd.request("SET ext_eapol_frame_io 0")
    dev[0].request("SET ext_eapol_frame_io 0")
    msg = ev.split(' ')[2]
    # Replace opcode. This will be discarded in WPS Enrollee processing.
    msg = msg[0:32] + "05" + msg[34:]
    res = dev[0].request("EAPOL_RX " + bssid + " " + msg)
    if "OK" not in res:
        raise Exception("EAPOL_RX failed")
    dev[0].request("WPS_CANCEL")
    dev[0].wait_disconnected()
    dev[0].flush_scan_cache()

def wps_m2_but_other(dev, apdev, title, msgtype):
    addr, bssid, hapd = wps_start_ext(apdev, dev)
    wps_ext_eap_identity_req(dev, hapd, bssid)
    wps_ext_eap_identity_resp(hapd, dev, addr)
    wps_ext_eap_wsc(dev, hapd, bssid, "EAP-WSC/Start")
    wps_ext_eap_wsc(hapd, dev, addr, "M1")
    logger.debug(title)
    ev = hapd.wait_event(["EAPOL-TX"], timeout=10)
    if ev is None:
        raise Exception("Timeout on EAPOL-TX")
    hapd.request("SET ext_eapol_frame_io 0")
    dev.request("SET ext_eapol_frame_io 0")
    msg = ev.split(' ')[2]
    if msg[46:54] != '10220001':
        raise Exception("Could not find Message Type attribute")
    # Replace Message Type value. This will be rejected.
    msg = msg[0:54] + msgtype + msg[56:]
    res = dev.request("EAPOL_RX " + bssid + " " + msg)
    if "OK" not in res:
        raise Exception("EAPOL_RX failed")
    ev = dev.wait_event(["WPS-FAIL"], timeout=5)
    if ev is None:
        raise Exception("WPS-FAIL event not seen")
    dev.request("WPS_CANCEL")
    dev.wait_disconnected()

def wps_m4_but_other(dev, apdev, title, msgtype):
    addr, bssid, hapd = wps_start_ext(apdev, dev)
    wps_ext_eap_identity_req(dev, hapd, bssid)
    wps_ext_eap_identity_resp(hapd, dev, addr)
    wps_ext_eap_wsc(dev, hapd, bssid, "EAP-WSC/Start")
    wps_ext_eap_wsc(hapd, dev, addr, "M1")
    wps_ext_eap_wsc(dev, hapd, bssid, "M2")
    wps_ext_eap_wsc(hapd, dev, addr, "M3")
    logger.debug(title)
    ev = hapd.wait_event(["EAPOL-TX"], timeout=10)
    if ev is None:
        raise Exception("Timeout on EAPOL-TX")
    hapd.request("SET ext_eapol_frame_io 0")
    dev.request("SET ext_eapol_frame_io 0")
    msg = ev.split(' ')[2]
    if msg[46:54] != '10220001':
        raise Exception("Could not find Message Type attribute")
    # Replace Message Type value. This will be rejected.
    msg = msg[0:54] + msgtype + msg[56:]
    res = dev.request("EAPOL_RX " + bssid + " " + msg)
    if "OK" not in res:
        raise Exception("EAPOL_RX failed")
    ev = hapd.wait_event(["WPS-FAIL"], timeout=5)
    if ev is None:
        raise Exception("WPS-FAIL event not seen")
    dev.request("WPS_CANCEL")
    dev.wait_disconnected()

def test_ap_wps_m2_msg_type_m4(dev, apdev):
    """WPS and M2 but Message Type M4"""
    wps_m2_but_other(dev[0], apdev[0], "M2/M4", "08")

def test_ap_wps_m2_msg_type_m6(dev, apdev):
    """WPS and M2 but Message Type M6"""
    wps_m2_but_other(dev[0], apdev[0], "M2/M6", "0a")

def test_ap_wps_m2_msg_type_m8(dev, apdev):
    """WPS and M2 but Message Type M8"""
    wps_m2_but_other(dev[0], apdev[0], "M2/M8", "0c")

def test_ap_wps_m4_msg_type_m2(dev, apdev):
    """WPS and M4 but Message Type M2"""
    wps_m4_but_other(dev[0], apdev[0], "M4/M2", "05")

def test_ap_wps_m4_msg_type_m2d(dev, apdev):
    """WPS and M4 but Message Type M2D"""
    wps_m4_but_other(dev[0], apdev[0], "M4/M2D", "06")

@remote_compatible
def test_ap_wps_config_methods(dev, apdev):
    """WPS configuration method parsing"""
    ssid = "test-wps-conf"
    params = {"ssid": ssid, "eap_server": "1", "wps_state": "2",
              "wpa_passphrase": "12345678", "wpa": "2",
              "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP",
              "config_methods": "ethernet display ext_nfc_token int_nfc_token physical_display physical_push_button"}
    hapd = hostapd.add_ap(apdev[0], params)
    params = {"ssid": ssid, "eap_server": "1", "wps_state": "2",
              "wpa_passphrase": "12345678", "wpa": "2",
              "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP",
              "config_methods": "display push_button"}
    hapd2 = hostapd.add_ap(apdev[1], params)

def test_ap_wps_set_selected_registrar_proto(dev, apdev):
    """WPS UPnP SetSelectedRegistrar protocol testing"""
    ap_uuid = "27ea801a-9e5c-4e73-bd82-f89cbcd10d7e"
    hapd = add_ssdp_ap(apdev[0], ap_uuid)

    location = ssdp_get_location(ap_uuid)
    urls = upnp_get_urls(location)
    eventurl = urlparse(urls['event_sub_url'])
    ctrlurl = urlparse(urls['control_url'])
    url = urlparse(location)
    conn = HTTPConnection(url.netloc)

    class WPSERHTTPServer(StreamRequestHandler):
        def handle(self):
            data = self.rfile.readline().strip()
            logger.debug(data)
            self.wfile.write(gen_wps_event())

    server = MyTCPServer(("127.0.0.1", 12345), WPSERHTTPServer)
    server.timeout = 1

    headers = {"callback": '<http://127.0.0.1:12345/event>',
               "NT": "upnp:event",
               "timeout": "Second-1234"}
    conn.request("SUBSCRIBE", eventurl.path, "\r\n\r\n", headers)
    resp = conn.getresponse()
    if resp.status != 200:
        raise Exception("Unexpected HTTP response: %d" % resp.status)
    sid = resp.getheader("sid")
    logger.debug("Subscription SID " + sid)
    server.handle_request()

    tests = [(500, "10"),
             (200, "104a000110" + "1041000101" + "101200020000" +
              "105300023148" +
              "1049002c00372a0001200124111111111111222222222222333333333333444444444444555555555555666666666666" +
              "10480010362db47ba53a519188fb5458b986b2e4"),
             (200, "104a000110" + "1041000100" + "101200020000" +
              "105300020000"),
             (200, "104a000110" + "1041000100"),
             (200, "104a000110")]
    for status, test in tests:
        tlvs = binascii.unhexlify(test)
        newmsg = base64.b64encode(tlvs).decode()
        msg = '<?xml version="1.0"?>\n'
        msg += '<s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/" s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">'
        msg += '<s:Body>'
        msg += '<u:SetSelectedRegistrar xmlns:u="urn:schemas-wifialliance-org:service:WFAWLANConfig:1">'
        msg += '<NewMessage>'
        msg += newmsg
        msg += "</NewMessage></u:SetSelectedRegistrar></s:Body></s:Envelope>"
        headers = {"Content-type": 'text/xml; charset="utf-8"'}
        headers["SOAPAction"] = '"urn:schemas-wifialliance-org:service:WFAWLANConfig:1#%s"' % "SetSelectedRegistrar"
        conn.request("POST", ctrlurl.path, msg, headers)
        resp = conn.getresponse()
        if resp.status != status:
            raise Exception("Unexpected HTTP response: %d (expected %d)" % (resp.status, status))

def test_ap_wps_adv_oom(dev, apdev):
    """WPS AP and advertisement OOM"""
    ap_uuid = "27ea801a-9e5c-4e73-bd82-f89cbcd10d7e"
    hapd = add_ssdp_ap(apdev[0], ap_uuid)

    with alloc_fail(hapd, 1, "=msearchreply_state_machine_start"):
        ssdp_send_msearch("urn:schemas-wifialliance-org:service:WFAWLANConfig:1",
                          no_recv=True)
        time.sleep(0.2)

    with alloc_fail(hapd, 1, "eloop_register_timeout;msearchreply_state_machine_start"):
        ssdp_send_msearch("urn:schemas-wifialliance-org:service:WFAWLANConfig:1",
                          no_recv=True)
        time.sleep(0.2)

    with alloc_fail(hapd, 1,
                    "next_advertisement;advertisement_state_machine_stop"):
        hapd.disable()

    with alloc_fail(hapd, 1, "ssdp_listener_start"):
        if "FAIL" not in hapd.request("ENABLE"):
            raise Exception("ENABLE succeeded during OOM")

def test_wps_config_methods(dev):
    """WPS config method update"""
    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    wpas.interface_add("wlan5")
    if "OK" not in wpas.request("SET config_methods display label"):
        raise Exception("Failed to set config_methods")
    if wpas.request("GET config_methods").strip() != "display label":
        raise Exception("config_methods were not updated")
    if "OK" not in wpas.request("SET config_methods "):
        raise Exception("Failed to clear config_methods")
    if wpas.request("GET config_methods").strip() != "":
        raise Exception("config_methods were not cleared")

WPS_VENDOR_ID_WFA = 14122
WPS_VENDOR_TYPE = 1

# EAP-WSC Op-Code values
WSC_Start = 0x01
WSC_ACK = 0x02
WSC_NACK = 0x03
WSC_MSG = 0x04
WSC_Done = 0x05
WSC_FRAG_ACK = 0x06

ATTR_AP_CHANNEL = 0x1001
ATTR_ASSOC_STATE = 0x1002
ATTR_AUTH_TYPE = 0x1003
ATTR_AUTH_TYPE_FLAGS = 0x1004
ATTR_AUTHENTICATOR = 0x1005
ATTR_CONFIG_METHODS = 0x1008
ATTR_CONFIG_ERROR = 0x1009
ATTR_CONFIRM_URL4 = 0x100a
ATTR_CONFIRM_URL6 = 0x100b
ATTR_CONN_TYPE = 0x100c
ATTR_CONN_TYPE_FLAGS = 0x100d
ATTR_CRED = 0x100e
ATTR_ENCR_TYPE = 0x100f
ATTR_ENCR_TYPE_FLAGS = 0x1010
ATTR_DEV_NAME = 0x1011
ATTR_DEV_PASSWORD_ID = 0x1012
ATTR_E_HASH1 = 0x1014
ATTR_E_HASH2 = 0x1015
ATTR_E_SNONCE1 = 0x1016
ATTR_E_SNONCE2 = 0x1017
ATTR_ENCR_SETTINGS = 0x1018
ATTR_ENROLLEE_NONCE = 0x101a
ATTR_FEATURE_ID = 0x101b
ATTR_IDENTITY = 0x101c
ATTR_IDENTITY_PROOF = 0x101d
ATTR_KEY_WRAP_AUTH = 0x101e
ATTR_KEY_ID = 0x101f
ATTR_MAC_ADDR = 0x1020
ATTR_MANUFACTURER = 0x1021
ATTR_MSG_TYPE = 0x1022
ATTR_MODEL_NAME = 0x1023
ATTR_MODEL_NUMBER = 0x1024
ATTR_NETWORK_INDEX = 0x1026
ATTR_NETWORK_KEY = 0x1027
ATTR_NETWORK_KEY_INDEX = 0x1028
ATTR_NEW_DEVICE_NAME = 0x1029
ATTR_NEW_PASSWORD = 0x102a
ATTR_OOB_DEVICE_PASSWORD = 0x102c
ATTR_OS_VERSION = 0x102d
ATTR_POWER_LEVEL = 0x102f
ATTR_PSK_CURRENT = 0x1030
ATTR_PSK_MAX = 0x1031
ATTR_PUBLIC_KEY = 0x1032
ATTR_RADIO_ENABLE = 0x1033
ATTR_REBOOT = 0x1034
ATTR_REGISTRAR_CURRENT = 0x1035
ATTR_REGISTRAR_ESTABLISHED = 0x1036
ATTR_REGISTRAR_LIST = 0x1037
ATTR_REGISTRAR_MAX = 0x1038
ATTR_REGISTRAR_NONCE = 0x1039
ATTR_REQUEST_TYPE = 0x103a
ATTR_RESPONSE_TYPE = 0x103b
ATTR_RF_BANDS = 0x103c
ATTR_R_HASH1 = 0x103d
ATTR_R_HASH2 = 0x103e
ATTR_R_SNONCE1 = 0x103f
ATTR_R_SNONCE2 = 0x1040
ATTR_SELECTED_REGISTRAR = 0x1041
ATTR_SERIAL_NUMBER = 0x1042
ATTR_WPS_STATE = 0x1044
ATTR_SSID = 0x1045
ATTR_TOTAL_NETWORKS = 0x1046
ATTR_UUID_E = 0x1047
ATTR_UUID_R = 0x1048
ATTR_VENDOR_EXT = 0x1049
ATTR_VERSION = 0x104a
ATTR_X509_CERT_REQ = 0x104b
ATTR_X509_CERT = 0x104c
ATTR_EAP_IDENTITY = 0x104d
ATTR_MSG_COUNTER = 0x104e
ATTR_PUBKEY_HASH = 0x104f
ATTR_REKEY_KEY = 0x1050
ATTR_KEY_LIFETIME = 0x1051
ATTR_PERMITTED_CFG_METHODS = 0x1052
ATTR_SELECTED_REGISTRAR_CONFIG_METHODS = 0x1053
ATTR_PRIMARY_DEV_TYPE = 0x1054
ATTR_SECONDARY_DEV_TYPE_LIST = 0x1055
ATTR_PORTABLE_DEV = 0x1056
ATTR_AP_SETUP_LOCKED = 0x1057
ATTR_APPLICATION_EXT = 0x1058
ATTR_EAP_TYPE = 0x1059
ATTR_IV = 0x1060
ATTR_KEY_PROVIDED_AUTO = 0x1061
ATTR_802_1X_ENABLED = 0x1062
ATTR_APPSESSIONKEY = 0x1063
ATTR_WEPTRANSMITKEY = 0x1064
ATTR_REQUESTED_DEV_TYPE = 0x106a

# Message Type
WPS_Beacon = 0x01
WPS_ProbeRequest = 0x02
WPS_ProbeResponse = 0x03
WPS_M1 = 0x04
WPS_M2 = 0x05
WPS_M2D = 0x06
WPS_M3 = 0x07
WPS_M4 = 0x08
WPS_M5 = 0x09
WPS_M6 = 0x0a
WPS_M7 = 0x0b
WPS_M8 = 0x0c
WPS_WSC_ACK = 0x0d
WPS_WSC_NACK = 0x0e
WPS_WSC_DONE = 0x0f

def get_wsc_msg(dev):
    ev = dev.wait_event(["EAPOL-TX"], timeout=10)
    if ev is None:
        raise Exception("Timeout on EAPOL-TX")
    data = binascii.unhexlify(ev.split(' ')[2])
    msg = {}

    # Parse EAPOL header
    if len(data) < 4:
        raise Exception("No room for EAPOL header")
    version, type, length = struct.unpack('>BBH', data[0:4])
    msg['eapol_version'] = version
    msg['eapol_type'] = type
    msg['eapol_length'] = length
    data = data[4:]
    if length != len(data):
        raise Exception("EAPOL header length mismatch (%d != %d)" % (length, len(data)))
    if type != 0:
        raise Exception("Unexpected EAPOL header type: %d" % type)

    # Parse EAP header
    if len(data) < 4:
        raise Exception("No room for EAP header")
    code, identifier, length = struct.unpack('>BBH', data[0:4])
    msg['eap_code'] = code
    msg['eap_identifier'] = identifier
    msg['eap_length'] = length
    data = data[4:]
    if msg['eapol_length'] != msg['eap_length']:
        raise Exception("EAP header length mismatch (%d != %d)" % (msg['eapol_length'], length))

    # Parse EAP expanded header
    if len(data) < 1:
        raise Exception("No EAP type included")
    msg['eap_type'], = struct.unpack('B', data[0:1])
    data = data[1:]

    if msg['eap_type'] == 254:
        if len(data) < 3 + 4:
            raise Exception("Truncated EAP expanded header")
        msg['eap_vendor_id'], msg['eap_vendor_type'] = struct.unpack('>LL', b'\x00' + data[0:7])
        data = data[7:]
    else:
        raise Exception("Unexpected EAP type")

    if msg['eap_vendor_id'] != WPS_VENDOR_ID_WFA:
        raise Exception("Unexpected Vendor-Id")
    if msg['eap_vendor_type'] != WPS_VENDOR_TYPE:
        raise Exception("Unexpected Vendor-Type")

    # Parse EAP-WSC header
    if len(data) < 2:
        raise Exception("Truncated EAP-WSC header")
    msg['wsc_opcode'], msg['wsc_flags'] = struct.unpack('BB', data[0:2])
    data = data[2:]

    # Parse WSC attributes
    msg['raw_attrs'] = data
    attrs = {}
    while len(data) > 0:
        if len(data) < 4:
            raise Exception("Truncated attribute header")
        attr, length = struct.unpack('>HH', data[0:4])
        data = data[4:]
        if length > len(data):
            raise Exception("Truncated attribute 0x%04x" % attr)
        attrs[attr] = data[0:length]
        data = data[length:]
    msg['wsc_attrs'] = attrs

    if ATTR_MSG_TYPE in attrs:
        msg['wsc_msg_type'], = struct.unpack('B', attrs[ATTR_MSG_TYPE])

    return msg

def recv_wsc_msg(dev, opcode, msg_type):
    msg = get_wsc_msg(dev)
    if msg['wsc_opcode'] != opcode or msg['wsc_msg_type'] != msg_type:
        raise Exception("Unexpected Op-Code/MsgType")
    return msg, msg['wsc_attrs'], msg['raw_attrs']

def build_wsc_attr(attr, payload):
    _payload = payload if type(payload) == bytes else payload.encode()
    return struct.pack('>HH', attr, len(_payload)) + _payload

def build_attr_msg_type(msg_type):
    return build_wsc_attr(ATTR_MSG_TYPE, struct.pack('B', msg_type))

def build_eap_wsc(eap_code, eap_id, payload, opcode=WSC_MSG):
    length = 4 + 8 + 2 + len(payload)
    # EAPOL header
    msg = struct.pack('>BBH', 2, 0, length)
    # EAP header
    msg += struct.pack('>BBH', eap_code, eap_id, length)
    # EAP expanded header for EAP-WSC
    msg += struct.pack('B', 254)
    msg += struct.pack('>L', WPS_VENDOR_ID_WFA)[1:4]
    msg += struct.pack('>L', WPS_VENDOR_TYPE)
    # EAP-WSC header
    msg += struct.pack('BB', opcode, 0)
    # WSC attributes
    msg += payload
    return msg

def build_eap_success(eap_id):
    length = 4
    # EAPOL header
    msg = struct.pack('>BBH', 2, 0, length)
    # EAP header
    msg += struct.pack('>BBH', 3, eap_id, length)
    return msg

def build_eap_failure(eap_id):
    length = 4
    # EAPOL header
    msg = struct.pack('>BBH', 2, 0, length)
    # EAP header
    msg += struct.pack('>BBH', 4, eap_id, length)
    return msg

def send_wsc_msg(dev, src, msg):
    res = dev.request("EAPOL_RX " + src + " " + binascii.hexlify(msg).decode())
    if "OK" not in res:
        raise Exception("EAPOL_RX failed")

group_5_prime = 0xFFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD129024E088A67CC74020BBEA63B139B22514A08798E3404DDEF9519B3CD3A431B302B0A6DF25F14374FE1356D6D51C245E485B576625E7EC6F44C42E9A637ED6B0BFF5CB6F406B7EDEE386BFB5A899FA5AE9F24117C4B1FE649286651ECE45B3DC2007CB8A163BF0598DA48361C55D39A69163FA8FD24CF5F83655D23DCA3AD961C62F356208552BB9ED529077096966D670C354E4ABC9804F1746C08CA237327FFFFFFFFFFFFFFFF
group_5_generator = 2

def wsc_kdf(key, label, bits):
    result = b''
    i = 1
    while len(result) * 8 < bits:
        data = struct.pack('>L', i) + label.encode() + struct.pack('>L', bits)
        m = hmac.new(key, data, hashlib.sha256)
        result += m.digest()
        i += 1
    return result[0:bits // 8]

def wsc_keys(kdk):
    keys = wsc_kdf(kdk, "Wi-Fi Easy and Secure Key Derivation", 640)
    authkey = keys[0:32]
    keywrapkey = keys[32:48]
    emsk = keys[48:80]
    return authkey, keywrapkey, emsk

def wsc_dev_pw_half_psk(authkey, dev_pw):
    m = hmac.new(authkey, dev_pw.encode(), hashlib.sha256)
    return m.digest()[0:16]

def wsc_dev_pw_psk(authkey, dev_pw):
    dev_pw_1 = dev_pw[0:len(dev_pw) // 2]
    dev_pw_2 = dev_pw[len(dev_pw) // 2:]
    psk1 = wsc_dev_pw_half_psk(authkey, dev_pw_1)
    psk2 = wsc_dev_pw_half_psk(authkey, dev_pw_2)
    return psk1, psk2

def build_attr_authenticator(authkey, prev_msg, curr_msg):
    m = hmac.new(authkey, prev_msg + curr_msg, hashlib.sha256)
    auth = m.digest()[0:8]
    return build_wsc_attr(ATTR_AUTHENTICATOR, auth)

def build_attr_encr_settings(authkey, keywrapkey, data):
    m = hmac.new(authkey, data, hashlib.sha256)
    kwa = m.digest()[0:8]
    data += build_wsc_attr(ATTR_KEY_WRAP_AUTH, kwa)
    iv = 16*b'\x99'
    aes = AES.new(keywrapkey, AES.MODE_CBC, iv)
    pad_len = 16 - len(data) % 16
    ps = pad_len * struct.pack('B', pad_len)
    data += ps
    wrapped = aes.encrypt(data)
    return build_wsc_attr(ATTR_ENCR_SETTINGS, iv + wrapped)

def decrypt_attr_encr_settings(authkey, keywrapkey, data):
    if len(data) < 32 or len(data) % 16 != 0:
        raise Exception("Unexpected Encrypted Settings length: %d" % len(data))
    iv = data[0:16]
    encr = data[16:]
    aes = AES.new(keywrapkey, AES.MODE_CBC, iv)
    decrypted = aes.decrypt(encr)
    pad_len, = struct.unpack('B', decrypted[-1:])
    if pad_len > len(decrypted):
        raise Exception("Invalid padding in Encrypted Settings")
    for i in range(-pad_len, -1):
        if decrypted[i] != decrypted[-1]:
            raise Exception("Invalid PS value in Encrypted Settings")

    decrypted = decrypted[0:len(decrypted) - pad_len]
    if len(decrypted) < 12:
        raise Exception("Truncated Encrypted Settings plaintext")
    kwa = decrypted[-12:]
    attr, length = struct.unpack(">HH", kwa[0:4])
    if attr != ATTR_KEY_WRAP_AUTH or length != 8:
        raise Exception("Invalid KWA header")
    kwa = kwa[4:]
    decrypted = decrypted[0:len(decrypted) - 12]

    m = hmac.new(authkey, decrypted, hashlib.sha256)
    calc_kwa = m.digest()[0:8]
    if kwa != calc_kwa:
        raise Exception("KWA mismatch")

    return decrypted

def zeropad_str(val, pad_len):
    while len(val) < pad_len * 2:
        val = '0' + val
    return val

def wsc_dh_init():
    # For now, use a hardcoded private key. In theory, this is supposed to be
    # randomly selected.
    own_private = 0x123456789
    own_public = pow(group_5_generator, own_private, group_5_prime)
    pk = binascii.unhexlify(zeropad_str(format(own_public, '02x'), 192))
    return own_private, pk

def wsc_dh_kdf(peer_pk, own_private, mac_addr, e_nonce, r_nonce):
    peer_public = int(binascii.hexlify(peer_pk), 16)
    if peer_public < 2 or peer_public >= group_5_prime:
        raise Exception("Invalid peer public key")
    if pow(peer_public, (group_5_prime - 1) // 2, group_5_prime) != 1:
        raise Exception("Unexpected Legendre symbol for peer public key")

    shared_secret = pow(peer_public, own_private, group_5_prime)
    ss = zeropad_str(format(shared_secret, "02x"), 192)
    logger.debug("DH shared secret: " + ss)

    dhkey = hashlib.sha256(binascii.unhexlify(ss)).digest()
    logger.debug("DHKey: " + binascii.hexlify(dhkey).decode())

    m = hmac.new(dhkey, e_nonce + mac_addr + r_nonce, hashlib.sha256)
    kdk = m.digest()
    logger.debug("KDK: " + binascii.hexlify(kdk).decode())
    authkey, keywrapkey, emsk = wsc_keys(kdk)
    logger.debug("AuthKey: " + binascii.hexlify(authkey).decode())
    logger.debug("KeyWrapKey: " + binascii.hexlify(keywrapkey).decode())
    logger.debug("EMSK: " + binascii.hexlify(emsk).decode())
    return authkey, keywrapkey

def wsc_dev_pw_hash(authkey, dev_pw, e_pk, r_pk):
    psk1, psk2 = wsc_dev_pw_psk(authkey, dev_pw)
    logger.debug("PSK1: " + binascii.hexlify(psk1).decode())
    logger.debug("PSK2: " + binascii.hexlify(psk2).decode())

    # Note: Secret values are supposed to be random, but hardcoded values are
    # fine for testing.
    s1 = 16*b'\x77'
    m = hmac.new(authkey, s1 + psk1 + e_pk + r_pk, hashlib.sha256)
    hash1 = m.digest()
    logger.debug("Hash1: " + binascii.hexlify(hash1).decode())

    s2 = 16*b'\x88'
    m = hmac.new(authkey, s2 + psk2 + e_pk + r_pk, hashlib.sha256)
    hash2 = m.digest()
    logger.debug("Hash2: " + binascii.hexlify(hash2).decode())
    return s1, s2, hash1, hash2

def build_m1(eap_id, uuid_e, mac_addr, e_nonce, e_pk,
             manufacturer='', model_name='', config_methods='\x00\x00'):
    attrs = build_wsc_attr(ATTR_VERSION, '\x10')
    attrs += build_attr_msg_type(WPS_M1)
    attrs += build_wsc_attr(ATTR_UUID_E, uuid_e)
    attrs += build_wsc_attr(ATTR_MAC_ADDR, mac_addr)
    attrs += build_wsc_attr(ATTR_ENROLLEE_NONCE, e_nonce)
    attrs += build_wsc_attr(ATTR_PUBLIC_KEY, e_pk)
    attrs += build_wsc_attr(ATTR_AUTH_TYPE_FLAGS, '\x00\x00')
    attrs += build_wsc_attr(ATTR_ENCR_TYPE_FLAGS, '\x00\x00')
    attrs += build_wsc_attr(ATTR_CONN_TYPE_FLAGS, '\x00')
    attrs += build_wsc_attr(ATTR_CONFIG_METHODS, config_methods)
    attrs += build_wsc_attr(ATTR_WPS_STATE, '\x00')
    attrs += build_wsc_attr(ATTR_MANUFACTURER, manufacturer)
    attrs += build_wsc_attr(ATTR_MODEL_NAME, model_name)
    attrs += build_wsc_attr(ATTR_MODEL_NUMBER, '')
    attrs += build_wsc_attr(ATTR_SERIAL_NUMBER, '')
    attrs += build_wsc_attr(ATTR_PRIMARY_DEV_TYPE, 8*'\x00')
    attrs += build_wsc_attr(ATTR_DEV_NAME, '')
    attrs += build_wsc_attr(ATTR_RF_BANDS, '\x00')
    attrs += build_wsc_attr(ATTR_ASSOC_STATE, '\x00\x00')
    attrs += build_wsc_attr(ATTR_DEV_PASSWORD_ID, '\x00\x00')
    attrs += build_wsc_attr(ATTR_CONFIG_ERROR, '\x00\x00')
    attrs += build_wsc_attr(ATTR_OS_VERSION, '\x00\x00\x00\x00')
    m1 = build_eap_wsc(2, eap_id, attrs)
    return m1, attrs

def build_m2(authkey, m1, eap_id, e_nonce, r_nonce, uuid_r, r_pk,
             dev_pw_id='\x00\x00', eap_code=1):
    attrs = build_wsc_attr(ATTR_VERSION, '\x10')
    attrs += build_attr_msg_type(WPS_M2)
    if e_nonce:
        attrs += build_wsc_attr(ATTR_ENROLLEE_NONCE, e_nonce)
    if r_nonce:
        attrs += build_wsc_attr(ATTR_REGISTRAR_NONCE, r_nonce)
    attrs += build_wsc_attr(ATTR_UUID_R, uuid_r)
    if r_pk:
        attrs += build_wsc_attr(ATTR_PUBLIC_KEY, r_pk)
    attrs += build_wsc_attr(ATTR_AUTH_TYPE_FLAGS, '\x00\x00')
    attrs += build_wsc_attr(ATTR_ENCR_TYPE_FLAGS, '\x00\x00')
    attrs += build_wsc_attr(ATTR_CONN_TYPE_FLAGS, '\x00')
    attrs += build_wsc_attr(ATTR_CONFIG_METHODS, '\x00\x00')
    attrs += build_wsc_attr(ATTR_MANUFACTURER, '')
    attrs += build_wsc_attr(ATTR_MODEL_NAME, '')
    attrs += build_wsc_attr(ATTR_MODEL_NUMBER, '')
    attrs += build_wsc_attr(ATTR_SERIAL_NUMBER, '')
    attrs += build_wsc_attr(ATTR_PRIMARY_DEV_TYPE, 8*'\x00')
    attrs += build_wsc_attr(ATTR_DEV_NAME, '')
    attrs += build_wsc_attr(ATTR_RF_BANDS, '\x00')
    attrs += build_wsc_attr(ATTR_ASSOC_STATE, '\x00\x00')
    attrs += build_wsc_attr(ATTR_CONFIG_ERROR, '\x00\x00')
    attrs += build_wsc_attr(ATTR_DEV_PASSWORD_ID, dev_pw_id)
    attrs += build_wsc_attr(ATTR_OS_VERSION, '\x00\x00\x00\x00')
    attrs += build_attr_authenticator(authkey, m1, attrs)
    m2 = build_eap_wsc(eap_code, eap_id, attrs)
    return m2, attrs

def build_m2d(m1, eap_id, e_nonce, r_nonce, uuid_r, dev_pw_id=None, eap_code=1):
    attrs = build_wsc_attr(ATTR_VERSION, '\x10')
    attrs += build_attr_msg_type(WPS_M2D)
    attrs += build_wsc_attr(ATTR_ENROLLEE_NONCE, e_nonce)
    attrs += build_wsc_attr(ATTR_REGISTRAR_NONCE, r_nonce)
    attrs += build_wsc_attr(ATTR_UUID_R, uuid_r)
    attrs += build_wsc_attr(ATTR_AUTH_TYPE_FLAGS, '\x00\x00')
    attrs += build_wsc_attr(ATTR_ENCR_TYPE_FLAGS, '\x00\x00')
    attrs += build_wsc_attr(ATTR_CONN_TYPE_FLAGS, '\x00')
    attrs += build_wsc_attr(ATTR_CONFIG_METHODS, '\x00\x00')
    attrs += build_wsc_attr(ATTR_MANUFACTURER, '')
    attrs += build_wsc_attr(ATTR_MODEL_NAME, '')
    #attrs += build_wsc_attr(ATTR_MODEL_NUMBER, '')
    attrs += build_wsc_attr(ATTR_SERIAL_NUMBER, '')
    attrs += build_wsc_attr(ATTR_PRIMARY_DEV_TYPE, 8*'\x00')
    attrs += build_wsc_attr(ATTR_DEV_NAME, '')
    attrs += build_wsc_attr(ATTR_RF_BANDS, '\x00')
    attrs += build_wsc_attr(ATTR_ASSOC_STATE, '\x00\x00')
    attrs += build_wsc_attr(ATTR_CONFIG_ERROR, '\x00\x00')
    attrs += build_wsc_attr(ATTR_OS_VERSION, '\x00\x00\x00\x00')
    if dev_pw_id:
        attrs += build_wsc_attr(ATTR_DEV_PASSWORD_ID, dev_pw_id)
    m2d = build_eap_wsc(eap_code, eap_id, attrs)
    return m2d, attrs

def build_ack(eap_id, e_nonce, r_nonce, msg_type=WPS_WSC_ACK, eap_code=1):
    attrs = build_wsc_attr(ATTR_VERSION, '\x10')
    if msg_type is not None:
        attrs += build_attr_msg_type(msg_type)
    if e_nonce:
        attrs += build_wsc_attr(ATTR_ENROLLEE_NONCE, e_nonce)
    if r_nonce:
        attrs += build_wsc_attr(ATTR_REGISTRAR_NONCE, r_nonce)
    msg = build_eap_wsc(eap_code, eap_id, attrs, opcode=WSC_ACK)
    return msg, attrs

def build_nack(eap_id, e_nonce, r_nonce, config_error='\x00\x00',
               msg_type=WPS_WSC_NACK, eap_code=1):
    attrs = build_wsc_attr(ATTR_VERSION, '\x10')
    if msg_type is not None:
        attrs += build_attr_msg_type(msg_type)
    if e_nonce:
        attrs += build_wsc_attr(ATTR_ENROLLEE_NONCE, e_nonce)
    if r_nonce:
        attrs += build_wsc_attr(ATTR_REGISTRAR_NONCE, r_nonce)
    if config_error:
        attrs += build_wsc_attr(ATTR_CONFIG_ERROR, config_error)
    msg = build_eap_wsc(eap_code, eap_id, attrs, opcode=WSC_NACK)
    return msg, attrs

def test_wps_ext(dev, apdev):
    """WPS against external implementation"""
    pin = "12345670"
    addr, bssid, hapd = wps_start_ext(apdev[0], dev[0], pin=pin)
    wps_ext_eap_identity_req(dev[0], hapd, bssid)
    wps_ext_eap_identity_resp(hapd, dev[0], addr)

    logger.debug("Receive WSC/Start from AP")
    msg = get_wsc_msg(hapd)
    if msg['wsc_opcode'] != WSC_Start:
        raise Exception("Unexpected Op-Code for WSC/Start")
    wsc_start_id = msg['eap_identifier']

    mac_addr = binascii.unhexlify(dev[0].own_addr().replace(':', ''))
    uuid_e = 16*b'\x11'
    e_nonce = 16*b'\x22'
    own_private, e_pk = wsc_dh_init()

    logger.debug("Send M1 to AP")
    m1, raw_m1_attrs = build_m1(msg['eap_identifier'], uuid_e, mac_addr,
                                e_nonce, e_pk)
    send_wsc_msg(hapd, addr, m1)

    logger.debug("Receive M2 from AP")
    msg, m2_attrs, raw_m2_attrs = recv_wsc_msg(hapd, WSC_MSG, WPS_M2)

    authkey, keywrapkey = wsc_dh_kdf(m2_attrs[ATTR_PUBLIC_KEY], own_private,
                                     mac_addr, e_nonce,
                                     m2_attrs[ATTR_REGISTRAR_NONCE])
    e_s1, e_s2, e_hash1, e_hash2 = wsc_dev_pw_hash(authkey, pin, e_pk,
                                                   m2_attrs[ATTR_PUBLIC_KEY])

    logger.debug("Send M3 to AP")
    attrs = build_wsc_attr(ATTR_VERSION, '\x10')
    attrs += build_attr_msg_type(WPS_M3)
    attrs += build_wsc_attr(ATTR_REGISTRAR_NONCE,
                            m2_attrs[ATTR_REGISTRAR_NONCE])
    attrs += build_wsc_attr(ATTR_E_HASH1, e_hash1)
    attrs += build_wsc_attr(ATTR_E_HASH2, e_hash2)
    attrs += build_attr_authenticator(authkey, raw_m2_attrs, attrs)
    raw_m3_attrs = attrs
    m3 = build_eap_wsc(2, msg['eap_identifier'], attrs)
    send_wsc_msg(hapd, addr, m3)

    logger.debug("Receive M4 from AP")
    msg, m4_attrs, raw_m4_attrs = recv_wsc_msg(hapd, WSC_MSG, WPS_M4)

    logger.debug("Send M5 to AP")
    attrs = build_wsc_attr(ATTR_VERSION, '\x10')
    attrs += build_attr_msg_type(WPS_M5)
    attrs += build_wsc_attr(ATTR_REGISTRAR_NONCE,
                            m2_attrs[ATTR_REGISTRAR_NONCE])
    data = build_wsc_attr(ATTR_E_SNONCE1, e_s1)
    attrs += build_attr_encr_settings(authkey, keywrapkey, data)
    attrs += build_attr_authenticator(authkey, raw_m4_attrs, attrs)
    raw_m5_attrs = attrs
    m5 = build_eap_wsc(2, msg['eap_identifier'], attrs)
    send_wsc_msg(hapd, addr, m5)

    logger.debug("Receive M6 from AP")
    msg, m6_attrs, raw_m6_attrs = recv_wsc_msg(hapd, WSC_MSG, WPS_M6)

    logger.debug("Send M7 to AP")
    attrs = build_wsc_attr(ATTR_VERSION, '\x10')
    attrs += build_attr_msg_type(WPS_M7)
    attrs += build_wsc_attr(ATTR_REGISTRAR_NONCE,
                            m2_attrs[ATTR_REGISTRAR_NONCE])
    data = build_wsc_attr(ATTR_E_SNONCE2, e_s2)
    attrs += build_attr_encr_settings(authkey, keywrapkey, data)
    attrs += build_attr_authenticator(authkey, raw_m6_attrs, attrs)
    m7 = build_eap_wsc(2, msg['eap_identifier'], attrs)
    raw_m7_attrs = attrs
    send_wsc_msg(hapd, addr, m7)

    logger.debug("Receive M8 from AP")
    msg, m8_attrs, raw_m8_attrs = recv_wsc_msg(hapd, WSC_MSG, WPS_M8)
    m8_cred = decrypt_attr_encr_settings(authkey, keywrapkey,
                                         m8_attrs[ATTR_ENCR_SETTINGS])
    logger.debug("M8 Credential: " + binascii.hexlify(m8_cred).decode())

    logger.debug("Prepare WSC_Done")
    attrs = build_wsc_attr(ATTR_VERSION, '\x10')
    attrs += build_attr_msg_type(WPS_WSC_DONE)
    attrs += build_wsc_attr(ATTR_ENROLLEE_NONCE, e_nonce)
    attrs += build_wsc_attr(ATTR_REGISTRAR_NONCE,
                            m2_attrs[ATTR_REGISTRAR_NONCE])
    wsc_done = build_eap_wsc(2, msg['eap_identifier'], attrs, opcode=WSC_Done)
    # Do not send WSC_Done yet to allow exchangw with STA complete before the
    # AP disconnects.

    uuid_r = 16*b'\x33'
    r_nonce = 16*b'\x44'

    eap_id = wsc_start_id
    logger.debug("Send WSC/Start to STA")
    wsc_start = build_eap_wsc(1, eap_id, b'', opcode=WSC_Start)
    send_wsc_msg(dev[0], bssid, wsc_start)
    eap_id = (eap_id + 1) % 256

    logger.debug("Receive M1 from STA")
    msg, m1_attrs, raw_m1_attrs = recv_wsc_msg(dev[0], WSC_MSG, WPS_M1)

    authkey, keywrapkey = wsc_dh_kdf(m1_attrs[ATTR_PUBLIC_KEY], own_private,
                                     mac_addr, m1_attrs[ATTR_ENROLLEE_NONCE],
                                     r_nonce)
    r_s1, r_s2, r_hash1, r_hash2 = wsc_dev_pw_hash(authkey, pin,
                                                   m1_attrs[ATTR_PUBLIC_KEY],
                                                   e_pk)

    logger.debug("Send M2 to STA")
    m2, raw_m2_attrs = build_m2(authkey, raw_m1_attrs, eap_id,
                                m1_attrs[ATTR_ENROLLEE_NONCE],
                                r_nonce, uuid_r, e_pk)
    send_wsc_msg(dev[0], bssid, m2)
    eap_id = (eap_id + 1) % 256

    logger.debug("Receive M3 from STA")
    msg, m3_attrs, raw_m3_attrs = recv_wsc_msg(dev[0], WSC_MSG, WPS_M3)

    logger.debug("Send M4 to STA")
    attrs = build_wsc_attr(ATTR_VERSION, '\x10')
    attrs += build_attr_msg_type(WPS_M4)
    attrs += build_wsc_attr(ATTR_ENROLLEE_NONCE, m1_attrs[ATTR_ENROLLEE_NONCE])
    attrs += build_wsc_attr(ATTR_R_HASH1, r_hash1)
    attrs += build_wsc_attr(ATTR_R_HASH2, r_hash2)
    data = build_wsc_attr(ATTR_R_SNONCE1, r_s1)
    attrs += build_attr_encr_settings(authkey, keywrapkey, data)
    attrs += build_attr_authenticator(authkey, raw_m3_attrs, attrs)
    raw_m4_attrs = attrs
    m4 = build_eap_wsc(1, eap_id, attrs)
    send_wsc_msg(dev[0], bssid, m4)
    eap_id = (eap_id + 1) % 256

    logger.debug("Receive M5 from STA")
    msg, m5_attrs, raw_m5_attrs = recv_wsc_msg(dev[0], WSC_MSG, WPS_M5)

    logger.debug("Send M6 to STA")
    attrs = build_wsc_attr(ATTR_VERSION, '\x10')
    attrs += build_attr_msg_type(WPS_M6)
    attrs += build_wsc_attr(ATTR_ENROLLEE_NONCE,
                            m1_attrs[ATTR_ENROLLEE_NONCE])
    data = build_wsc_attr(ATTR_R_SNONCE2, r_s2)
    attrs += build_attr_encr_settings(authkey, keywrapkey, data)
    attrs += build_attr_authenticator(authkey, raw_m5_attrs, attrs)
    raw_m6_attrs = attrs
    m6 = build_eap_wsc(1, eap_id, attrs)
    send_wsc_msg(dev[0], bssid, m6)
    eap_id = (eap_id + 1) % 256

    logger.debug("Receive M7 from STA")
    msg, m7_attrs, raw_m7_attrs = recv_wsc_msg(dev[0], WSC_MSG, WPS_M7)

    logger.debug("Send M8 to STA")
    attrs = build_wsc_attr(ATTR_VERSION, '\x10')
    attrs += build_attr_msg_type(WPS_M8)
    attrs += build_wsc_attr(ATTR_ENROLLEE_NONCE,
                            m1_attrs[ATTR_ENROLLEE_NONCE])
    attrs += build_attr_encr_settings(authkey, keywrapkey, m8_cred)
    attrs += build_attr_authenticator(authkey, raw_m7_attrs, attrs)
    raw_m8_attrs = attrs
    m8 = build_eap_wsc(1, eap_id, attrs)
    send_wsc_msg(dev[0], bssid, m8)
    eap_id = (eap_id + 1) % 256

    ev = dev[0].wait_event(["WPS-CRED-RECEIVED"], timeout=5)
    if ev is None:
        raise Exception("wpa_supplicant did not report credential")

    logger.debug("Receive WSC_Done from STA")
    msg = get_wsc_msg(dev[0])
    if msg['wsc_opcode'] != WSC_Done or msg['wsc_msg_type'] != WPS_WSC_DONE:
        raise Exception("Unexpected Op-Code/MsgType for WSC_Done")

    logger.debug("Send WSC_Done to AP")
    hapd.request("SET ext_eapol_frame_io 0")
    dev[0].request("SET ext_eapol_frame_io 0")
    send_wsc_msg(hapd, addr, wsc_done)

    ev = hapd.wait_event(["WPS-REG-SUCCESS"], timeout=5)
    if ev is None:
        raise Exception("hostapd did not report WPS success")

    dev[0].wait_connected()

def wps_start_kwa(dev, apdev):
    pin = "12345670"
    addr, bssid, hapd = wps_start_ext(apdev[0], dev[0], pin=pin)
    wps_ext_eap_identity_req(dev[0], hapd, bssid)
    wps_ext_eap_identity_resp(hapd, dev[0], addr)
    wps_ext_eap_wsc(dev[0], hapd, bssid, "EAP-WSC/Start")

    mac_addr = binascii.unhexlify(dev[0].own_addr().replace(':', ''))
    uuid_r = 16*b'\x33'
    r_nonce = 16*b'\x44'
    own_private, e_pk = wsc_dh_init()

    logger.debug("Receive M1 from STA")
    msg, m1_attrs, raw_m1_attrs = recv_wsc_msg(dev[0], WSC_MSG, WPS_M1)
    eap_id = (msg['eap_identifier'] + 1) % 256

    authkey, keywrapkey = wsc_dh_kdf(m1_attrs[ATTR_PUBLIC_KEY], own_private,
                                     mac_addr, m1_attrs[ATTR_ENROLLEE_NONCE],
                                     r_nonce)
    r_s1, r_s2, r_hash1, r_hash2 = wsc_dev_pw_hash(authkey, pin,
                                                   m1_attrs[ATTR_PUBLIC_KEY],
                                                   e_pk)

    logger.debug("Send M2 to STA")
    m2, raw_m2_attrs = build_m2(authkey, raw_m1_attrs, eap_id,
                                m1_attrs[ATTR_ENROLLEE_NONCE],
                                r_nonce, uuid_r, e_pk)
    send_wsc_msg(dev[0], bssid, m2)
    eap_id = (eap_id + 1) % 256

    logger.debug("Receive M3 from STA")
    msg, m3_attrs, raw_m3_attrs = recv_wsc_msg(dev[0], WSC_MSG, WPS_M3)

    logger.debug("Send M4 to STA")
    attrs = build_wsc_attr(ATTR_VERSION, '\x10')
    attrs += build_attr_msg_type(WPS_M4)
    attrs += build_wsc_attr(ATTR_ENROLLEE_NONCE, m1_attrs[ATTR_ENROLLEE_NONCE])
    attrs += build_wsc_attr(ATTR_R_HASH1, r_hash1)
    attrs += build_wsc_attr(ATTR_R_HASH2, r_hash2)

    return r_s1, keywrapkey, authkey, raw_m3_attrs, eap_id, bssid, attrs

def wps_stop_kwa(dev, bssid, attrs, authkey, raw_m3_attrs, eap_id):
    attrs += build_attr_authenticator(authkey, raw_m3_attrs, attrs)
    m4 = build_eap_wsc(1, eap_id, attrs)
    send_wsc_msg(dev[0], bssid, m4)
    eap_id = (eap_id + 1) % 256

    logger.debug("Receive M5 from STA")
    msg = get_wsc_msg(dev[0])
    if msg['wsc_opcode'] != WSC_NACK:
        raise Exception("Unexpected message - expected WSC_Nack")

    dev[0].request("WPS_CANCEL")
    send_wsc_msg(dev[0], bssid, build_eap_failure(eap_id))
    dev[0].wait_disconnected()

def test_wps_ext_kwa_proto_no_kwa(dev, apdev):
    """WPS and KWA error: No KWA attribute"""
    r_s1, keywrapkey, authkey, raw_m3_attrs, eap_id, bssid, attrs = wps_start_kwa(dev, apdev)
    data = build_wsc_attr(ATTR_R_SNONCE1, r_s1)
    # Encrypted Settings without KWA
    iv = 16*b'\x99'
    aes = AES.new(keywrapkey, AES.MODE_CBC, iv)
    pad_len = 16 - len(data) % 16
    ps = pad_len * struct.pack('B', pad_len)
    data += ps
    wrapped = aes.encrypt(data)
    attrs += build_wsc_attr(ATTR_ENCR_SETTINGS, iv + wrapped)
    wps_stop_kwa(dev, bssid, attrs, authkey, raw_m3_attrs, eap_id)

def test_wps_ext_kwa_proto_data_after_kwa(dev, apdev):
    """WPS and KWA error: Data after KWA"""
    r_s1, keywrapkey, authkey, raw_m3_attrs, eap_id, bssid, attrs = wps_start_kwa(dev, apdev)
    data = build_wsc_attr(ATTR_R_SNONCE1, r_s1)
    # Encrypted Settings and data after KWA
    m = hmac.new(authkey, data, hashlib.sha256)
    kwa = m.digest()[0:8]
    data += build_wsc_attr(ATTR_KEY_WRAP_AUTH, kwa)
    data += build_wsc_attr(ATTR_VENDOR_EXT, "1234567890")
    iv = 16*b'\x99'
    aes = AES.new(keywrapkey, AES.MODE_CBC, iv)
    pad_len = 16 - len(data) % 16
    ps = pad_len * struct.pack('B', pad_len)
    data += ps
    wrapped = aes.encrypt(data)
    attrs += build_wsc_attr(ATTR_ENCR_SETTINGS, iv + wrapped)
    wps_stop_kwa(dev, bssid, attrs, authkey, raw_m3_attrs, eap_id)

def test_wps_ext_kwa_proto_kwa_mismatch(dev, apdev):
    """WPS and KWA error: KWA mismatch"""
    r_s1, keywrapkey, authkey, raw_m3_attrs, eap_id, bssid, attrs = wps_start_kwa(dev, apdev)
    data = build_wsc_attr(ATTR_R_SNONCE1, r_s1)
    # Encrypted Settings and KWA with incorrect value
    data += build_wsc_attr(ATTR_KEY_WRAP_AUTH, 8*'\x00')
    iv = 16*b'\x99'
    aes = AES.new(keywrapkey, AES.MODE_CBC, iv)
    pad_len = 16 - len(data) % 16
    ps = pad_len * struct.pack('B', pad_len)
    data += ps
    wrapped = aes.encrypt(data)
    attrs += build_wsc_attr(ATTR_ENCR_SETTINGS, iv + wrapped)
    wps_stop_kwa(dev, bssid, attrs, authkey, raw_m3_attrs, eap_id)

def wps_run_cred_proto(dev, apdev, m8_cred, connect=False, no_connect=False):
    pin = "12345670"
    addr, bssid, hapd = wps_start_ext(apdev[0], dev[0], pin=pin)
    wps_ext_eap_identity_req(dev[0], hapd, bssid)
    wps_ext_eap_identity_resp(hapd, dev[0], addr)
    wps_ext_eap_wsc(dev[0], hapd, bssid, "EAP-WSC/Start")

    mac_addr = binascii.unhexlify(dev[0].own_addr().replace(':', ''))
    uuid_r = 16*b'\x33'
    r_nonce = 16*b'\x44'
    own_private, e_pk = wsc_dh_init()

    logger.debug("Receive M1 from STA")
    msg, m1_attrs, raw_m1_attrs = recv_wsc_msg(dev[0], WSC_MSG, WPS_M1)
    eap_id = (msg['eap_identifier'] + 1) % 256

    authkey, keywrapkey = wsc_dh_kdf(m1_attrs[ATTR_PUBLIC_KEY], own_private,
                                     mac_addr, m1_attrs[ATTR_ENROLLEE_NONCE],
                                     r_nonce)
    r_s1, r_s2, r_hash1, r_hash2 = wsc_dev_pw_hash(authkey, pin,
                                                   m1_attrs[ATTR_PUBLIC_KEY],
                                                   e_pk)

    logger.debug("Send M2 to STA")
    m2, raw_m2_attrs = build_m2(authkey, raw_m1_attrs, eap_id,
                                m1_attrs[ATTR_ENROLLEE_NONCE],
                                r_nonce, uuid_r, e_pk)
    send_wsc_msg(dev[0], bssid, m2)
    eap_id = (eap_id + 1) % 256

    logger.debug("Receive M3 from STA")
    msg, m3_attrs, raw_m3_attrs = recv_wsc_msg(dev[0], WSC_MSG, WPS_M3)

    logger.debug("Send M4 to STA")
    attrs = build_wsc_attr(ATTR_VERSION, '\x10')
    attrs += build_attr_msg_type(WPS_M4)
    attrs += build_wsc_attr(ATTR_ENROLLEE_NONCE, m1_attrs[ATTR_ENROLLEE_NONCE])
    attrs += build_wsc_attr(ATTR_R_HASH1, r_hash1)
    attrs += build_wsc_attr(ATTR_R_HASH2, r_hash2)
    data = build_wsc_attr(ATTR_R_SNONCE1, r_s1)
    attrs += build_attr_encr_settings(authkey, keywrapkey, data)
    attrs += build_attr_authenticator(authkey, raw_m3_attrs, attrs)
    raw_m4_attrs = attrs
    m4 = build_eap_wsc(1, eap_id, attrs)
    send_wsc_msg(dev[0], bssid, m4)
    eap_id = (eap_id + 1) % 256

    logger.debug("Receive M5 from STA")
    msg, m5_attrs, raw_m5_attrs = recv_wsc_msg(dev[0], WSC_MSG, WPS_M5)

    logger.debug("Send M6 to STA")
    attrs = build_wsc_attr(ATTR_VERSION, '\x10')
    attrs += build_attr_msg_type(WPS_M6)
    attrs += build_wsc_attr(ATTR_ENROLLEE_NONCE,
                            m1_attrs[ATTR_ENROLLEE_NONCE])
    data = build_wsc_attr(ATTR_R_SNONCE2, r_s2)
    attrs += build_attr_encr_settings(authkey, keywrapkey, data)
    attrs += build_attr_authenticator(authkey, raw_m5_attrs, attrs)
    raw_m6_attrs = attrs
    m6 = build_eap_wsc(1, eap_id, attrs)
    send_wsc_msg(dev[0], bssid, m6)
    eap_id = (eap_id + 1) % 256

    logger.debug("Receive M7 from STA")
    msg, m7_attrs, raw_m7_attrs = recv_wsc_msg(dev[0], WSC_MSG, WPS_M7)

    logger.debug("Send M8 to STA")
    attrs = build_wsc_attr(ATTR_VERSION, '\x10')
    attrs += build_attr_msg_type(WPS_M8)
    attrs += build_wsc_attr(ATTR_ENROLLEE_NONCE,
                            m1_attrs[ATTR_ENROLLEE_NONCE])
    attrs += build_attr_encr_settings(authkey, keywrapkey, m8_cred)
    attrs += build_attr_authenticator(authkey, raw_m7_attrs, attrs)
    raw_m8_attrs = attrs
    m8 = build_eap_wsc(1, eap_id, attrs)
    send_wsc_msg(dev[0], bssid, m8)
    eap_id = (eap_id + 1) % 256

    if no_connect:
        logger.debug("Receive WSC_Done from STA")
        msg = get_wsc_msg(dev[0])
        if msg['wsc_opcode'] != WSC_Done or msg['wsc_msg_type'] != WPS_WSC_DONE:
            raise Exception("Unexpected Op-Code/MsgType for WSC_Done")

        hapd.request("SET ext_eapol_frame_io 0")
        dev[0].request("SET ext_eapol_frame_io 0")

        send_wsc_msg(dev[0], bssid, build_eap_failure(eap_id))

        dev[0].wait_disconnected()
        dev[0].request("REMOVE_NETWORK all")
    elif connect:
        logger.debug("Receive WSC_Done from STA")
        msg = get_wsc_msg(dev[0])
        if msg['wsc_opcode'] != WSC_Done or msg['wsc_msg_type'] != WPS_WSC_DONE:
            raise Exception("Unexpected Op-Code/MsgType for WSC_Done")

        hapd.request("SET ext_eapol_frame_io 0")
        dev[0].request("SET ext_eapol_frame_io 0")

        send_wsc_msg(dev[0], bssid, build_eap_failure(eap_id))

        dev[0].wait_connected()
    else:
        # Verify STA NACK's the credential
        msg = get_wsc_msg(dev[0])
        if msg['wsc_opcode'] != WSC_NACK:
            raise Exception("Unexpected message - expected WSC_Nack")
        dev[0].request("WPS_CANCEL")
        send_wsc_msg(dev[0], bssid, build_eap_failure(eap_id))
        dev[0].wait_disconnected()

def build_cred(nw_idx='\x01', ssid='test-wps-conf', auth_type='\x00\x20',
               encr_type='\x00\x08', nw_key="12345678",
               mac_addr='\x00\x00\x00\x00\x00\x00'):
    attrs = b''
    if nw_idx is not None:
        attrs += build_wsc_attr(ATTR_NETWORK_INDEX, nw_idx)
    if ssid is not None:
        attrs += build_wsc_attr(ATTR_SSID, ssid)
    if auth_type is not None:
        attrs += build_wsc_attr(ATTR_AUTH_TYPE, auth_type)
    if encr_type is not None:
        attrs += build_wsc_attr(ATTR_ENCR_TYPE, encr_type)
    if nw_key is not None:
        attrs += build_wsc_attr(ATTR_NETWORK_KEY, nw_key)
    if mac_addr is not None:
        attrs += build_wsc_attr(ATTR_MAC_ADDR, mac_addr)
    return build_wsc_attr(ATTR_CRED, attrs)

def test_wps_ext_cred_proto_success(dev, apdev):
    """WPS and Credential: success"""
    mac_addr = binascii.unhexlify(dev[0].own_addr().replace(':', ''))
    m8_cred = build_cred(mac_addr=mac_addr)
    wps_run_cred_proto(dev, apdev, m8_cred, connect=True)

def test_wps_ext_cred_proto_mac_addr_mismatch(dev, apdev):
    """WPS and Credential: MAC Address mismatch"""
    m8_cred = build_cred()
    wps_run_cred_proto(dev, apdev, m8_cred, connect=True)

def test_wps_ext_cred_proto_zero_padding(dev, apdev):
    """WPS and Credential: zeropadded attributes"""
    mac_addr = binascii.unhexlify(dev[0].own_addr().replace(':', ''))
    m8_cred = build_cred(mac_addr=mac_addr, ssid='test-wps-conf\x00',
                         nw_key="12345678\x00")
    wps_run_cred_proto(dev, apdev, m8_cred, connect=True)

def test_wps_ext_cred_proto_ssid_missing(dev, apdev):
    """WPS and Credential: SSID missing"""
    mac_addr = binascii.unhexlify(dev[0].own_addr().replace(':', ''))
    m8_cred = build_cred(mac_addr=mac_addr, ssid=None)
    wps_run_cred_proto(dev, apdev, m8_cred)

def test_wps_ext_cred_proto_ssid_zero_len(dev, apdev):
    """WPS and Credential: Zero-length SSID"""
    mac_addr = binascii.unhexlify(dev[0].own_addr().replace(':', ''))
    m8_cred = build_cred(mac_addr=mac_addr, ssid="")
    wps_run_cred_proto(dev, apdev, m8_cred, no_connect=True)

def test_wps_ext_cred_proto_auth_type_missing(dev, apdev):
    """WPS and Credential: Auth Type missing"""
    mac_addr = binascii.unhexlify(dev[0].own_addr().replace(':', ''))
    m8_cred = build_cred(mac_addr=mac_addr, auth_type=None)
    wps_run_cred_proto(dev, apdev, m8_cred)

def test_wps_ext_cred_proto_encr_type_missing(dev, apdev):
    """WPS and Credential: Encr Type missing"""
    mac_addr = binascii.unhexlify(dev[0].own_addr().replace(':', ''))
    m8_cred = build_cred(mac_addr=mac_addr, encr_type=None)
    wps_run_cred_proto(dev, apdev, m8_cred)

def test_wps_ext_cred_proto_network_key_missing(dev, apdev):
    """WPS and Credential: Network Key missing"""
    mac_addr = binascii.unhexlify(dev[0].own_addr().replace(':', ''))
    m8_cred = build_cred(mac_addr=mac_addr, nw_key=None)
    wps_run_cred_proto(dev, apdev, m8_cred)

def test_wps_ext_cred_proto_network_key_missing_open(dev, apdev):
    """WPS and Credential: Network Key missing (open)"""
    mac_addr = binascii.unhexlify(dev[0].own_addr().replace(':', ''))
    m8_cred = build_cred(mac_addr=mac_addr, auth_type='\x00\x01',
                         encr_type='\x00\x01', nw_key=None, ssid="foo")
    wps_run_cred_proto(dev, apdev, m8_cred, no_connect=True)

def test_wps_ext_cred_proto_mac_addr_missing(dev, apdev):
    """WPS and Credential: MAC Address missing"""
    m8_cred = build_cred(mac_addr=None)
    wps_run_cred_proto(dev, apdev, m8_cred)

def test_wps_ext_cred_proto_invalid_encr_type(dev, apdev):
    """WPS and Credential: Invalid Encr Type"""
    mac_addr = binascii.unhexlify(dev[0].own_addr().replace(':', ''))
    m8_cred = build_cred(mac_addr=mac_addr, encr_type='\x00\x00')
    wps_run_cred_proto(dev, apdev, m8_cred)

def test_wps_ext_cred_proto_missing_cred(dev, apdev):
    """WPS and Credential: Missing Credential"""
    mac_addr = binascii.unhexlify(dev[0].own_addr().replace(':', ''))
    m8_cred = b''
    wps_run_cred_proto(dev, apdev, m8_cred)

def test_wps_ext_proto_m2_no_public_key(dev, apdev):
    """WPS and no Public Key in M2"""
    pin = "12345670"
    addr, bssid, hapd = wps_start_ext(apdev[0], dev[0], pin=pin)
    wps_ext_eap_identity_req(dev[0], hapd, bssid)
    wps_ext_eap_identity_resp(hapd, dev[0], addr)
    wps_ext_eap_wsc(dev[0], hapd, bssid, "EAP-WSC/Start")

    mac_addr = binascii.unhexlify(dev[0].own_addr().replace(':', ''))
    uuid_r = 16*b'\x33'
    r_nonce = 16*b'\x44'
    own_private, e_pk = wsc_dh_init()

    logger.debug("Receive M1 from STA")
    msg, m1_attrs, raw_m1_attrs = recv_wsc_msg(dev[0], WSC_MSG, WPS_M1)
    eap_id = (msg['eap_identifier'] + 1) % 256

    authkey, keywrapkey = wsc_dh_kdf(m1_attrs[ATTR_PUBLIC_KEY], own_private,
                                     mac_addr, m1_attrs[ATTR_ENROLLEE_NONCE],
                                     r_nonce)
    r_s1, r_s2, r_hash1, r_hash2 = wsc_dev_pw_hash(authkey, pin,
                                                   m1_attrs[ATTR_PUBLIC_KEY],
                                                   e_pk)

    logger.debug("Send M2 to STA")
    m2, raw_m2_attrs = build_m2(authkey, raw_m1_attrs, eap_id,
                                m1_attrs[ATTR_ENROLLEE_NONCE],
                                r_nonce, uuid_r, None)
    send_wsc_msg(dev[0], bssid, m2)
    eap_id = (eap_id + 1) % 256

    # Verify STA NACK's the credential
    msg = get_wsc_msg(dev[0])
    if msg['wsc_opcode'] != WSC_NACK:
        raise Exception("Unexpected message - expected WSC_Nack")
    dev[0].request("WPS_CANCEL")
    send_wsc_msg(dev[0], bssid, build_eap_failure(eap_id))
    dev[0].wait_disconnected()

def test_wps_ext_proto_m2_invalid_public_key(dev, apdev):
    """WPS and invalid Public Key in M2"""
    pin = "12345670"
    addr, bssid, hapd = wps_start_ext(apdev[0], dev[0], pin=pin)
    wps_ext_eap_identity_req(dev[0], hapd, bssid)
    wps_ext_eap_identity_resp(hapd, dev[0], addr)
    wps_ext_eap_wsc(dev[0], hapd, bssid, "EAP-WSC/Start")

    mac_addr = binascii.unhexlify(dev[0].own_addr().replace(':', ''))
    uuid_r = 16*b'\x33'
    r_nonce = 16*b'\x44'
    own_private, e_pk = wsc_dh_init()

    logger.debug("Receive M1 from STA")
    msg, m1_attrs, raw_m1_attrs = recv_wsc_msg(dev[0], WSC_MSG, WPS_M1)
    eap_id = (msg['eap_identifier'] + 1) % 256

    authkey, keywrapkey = wsc_dh_kdf(m1_attrs[ATTR_PUBLIC_KEY], own_private,
                                     mac_addr, m1_attrs[ATTR_ENROLLEE_NONCE],
                                     r_nonce)
    r_s1, r_s2, r_hash1, r_hash2 = wsc_dev_pw_hash(authkey, pin,
                                                   m1_attrs[ATTR_PUBLIC_KEY],
                                                   e_pk)

    logger.debug("Send M2 to STA")
    m2, raw_m2_attrs = build_m2(authkey, raw_m1_attrs, eap_id,
                                m1_attrs[ATTR_ENROLLEE_NONCE],
                                r_nonce, uuid_r, 192*b'\xff')
    send_wsc_msg(dev[0], bssid, m2)
    eap_id = (eap_id + 1) % 256

    # Verify STA NACK's the credential
    msg = get_wsc_msg(dev[0])
    if msg['wsc_opcode'] != WSC_NACK:
        raise Exception("Unexpected message - expected WSC_Nack")
    dev[0].request("WPS_CANCEL")
    send_wsc_msg(dev[0], bssid, build_eap_failure(eap_id))
    dev[0].wait_disconnected()

def test_wps_ext_proto_m2_public_key_oom(dev, apdev):
    """WPS and Public Key OOM in M2"""
    pin = "12345670"
    addr, bssid, hapd = wps_start_ext(apdev[0], dev[0], pin=pin)
    wps_ext_eap_identity_req(dev[0], hapd, bssid)
    wps_ext_eap_identity_resp(hapd, dev[0], addr)
    wps_ext_eap_wsc(dev[0], hapd, bssid, "EAP-WSC/Start")

    mac_addr = binascii.unhexlify(dev[0].own_addr().replace(':', ''))
    uuid_r = 16*b'\x33'
    r_nonce = 16*b'\x44'
    own_private, e_pk = wsc_dh_init()

    logger.debug("Receive M1 from STA")
    msg, m1_attrs, raw_m1_attrs = recv_wsc_msg(dev[0], WSC_MSG, WPS_M1)
    eap_id = (msg['eap_identifier'] + 1) % 256

    authkey, keywrapkey = wsc_dh_kdf(m1_attrs[ATTR_PUBLIC_KEY], own_private,
                                     mac_addr, m1_attrs[ATTR_ENROLLEE_NONCE],
                                     r_nonce)
    r_s1, r_s2, r_hash1, r_hash2 = wsc_dev_pw_hash(authkey, pin,
                                                   m1_attrs[ATTR_PUBLIC_KEY],
                                                   e_pk)

    logger.debug("Send M2 to STA")
    m2, raw_m2_attrs = build_m2(authkey, raw_m1_attrs, eap_id,
                                m1_attrs[ATTR_ENROLLEE_NONCE],
                                r_nonce, uuid_r, e_pk)
    with alloc_fail(dev[0], 1, "wpabuf_alloc_copy;wps_process_pubkey"):
        send_wsc_msg(dev[0], bssid, m2)
        eap_id = (eap_id + 1) % 256

        # Verify STA NACK's the credential
        msg = get_wsc_msg(dev[0])
        if msg['wsc_opcode'] != WSC_NACK:
            raise Exception("Unexpected message - expected WSC_Nack")
        dev[0].request("WPS_CANCEL")
        send_wsc_msg(dev[0], bssid, build_eap_failure(eap_id))
        dev[0].wait_disconnected()

def test_wps_ext_proto_nack_m3(dev, apdev):
    """WPS and NACK M3"""
    pin = "12345670"
    addr, bssid, hapd = wps_start_ext(apdev[0], dev[0], pin=pin)
    wps_ext_eap_identity_req(dev[0], hapd, bssid)
    wps_ext_eap_identity_resp(hapd, dev[0], addr)
    wps_ext_eap_wsc(dev[0], hapd, bssid, "EAP-WSC/Start")

    mac_addr = binascii.unhexlify(dev[0].own_addr().replace(':', ''))
    uuid_r = 16*b'\x33'
    r_nonce = 16*b'\x44'
    own_private, e_pk = wsc_dh_init()

    logger.debug("Receive M1 from STA")
    msg, m1_attrs, raw_m1_attrs = recv_wsc_msg(dev[0], WSC_MSG, WPS_M1)
    eap_id = (msg['eap_identifier'] + 1) % 256

    authkey, keywrapkey = wsc_dh_kdf(m1_attrs[ATTR_PUBLIC_KEY], own_private,
                                     mac_addr, m1_attrs[ATTR_ENROLLEE_NONCE],
                                     r_nonce)
    r_s1, r_s2, r_hash1, r_hash2 = wsc_dev_pw_hash(authkey, pin,
                                                   m1_attrs[ATTR_PUBLIC_KEY],
                                                   e_pk)

    logger.debug("Send M2 to STA")
    m2, raw_m2_attrs = build_m2(authkey, raw_m1_attrs, eap_id,
                                m1_attrs[ATTR_ENROLLEE_NONCE],
                                r_nonce, uuid_r, e_pk)
    send_wsc_msg(dev[0], bssid, m2)
    eap_id = (eap_id + 1) % 256

    logger.debug("Receive M3 from STA")
    msg, m3_attrs, raw_m3_attrs = recv_wsc_msg(dev[0], WSC_MSG, WPS_M3)

    logger.debug("Send NACK to STA")
    msg, attrs = build_nack(eap_id, m1_attrs[ATTR_ENROLLEE_NONCE],
                            r_nonce, config_error='\x01\x23')
    send_wsc_msg(dev[0], bssid, msg)
    ev = dev[0].wait_event(["WPS-FAIL"], timeout=5)
    if ev is None:
        raise Exception("Failure not reported")
    if "msg=7 config_error=291" not in ev:
        raise Exception("Unexpected failure reason: " + ev)

def test_wps_ext_proto_nack_m5(dev, apdev):
    """WPS and NACK M5"""
    pin = "12345670"
    addr, bssid, hapd = wps_start_ext(apdev[0], dev[0], pin=pin)
    wps_ext_eap_identity_req(dev[0], hapd, bssid)
    wps_ext_eap_identity_resp(hapd, dev[0], addr)
    wps_ext_eap_wsc(dev[0], hapd, bssid, "EAP-WSC/Start")

    mac_addr = binascii.unhexlify(dev[0].own_addr().replace(':', ''))
    uuid_r = 16*b'\x33'
    r_nonce = 16*b'\x44'
    own_private, e_pk = wsc_dh_init()

    logger.debug("Receive M1 from STA")
    msg, m1_attrs, raw_m1_attrs = recv_wsc_msg(dev[0], WSC_MSG, WPS_M1)
    eap_id = (msg['eap_identifier'] + 1) % 256

    authkey, keywrapkey = wsc_dh_kdf(m1_attrs[ATTR_PUBLIC_KEY], own_private,
                                     mac_addr, m1_attrs[ATTR_ENROLLEE_NONCE],
                                     r_nonce)
    r_s1, r_s2, r_hash1, r_hash2 = wsc_dev_pw_hash(authkey, pin,
                                                   m1_attrs[ATTR_PUBLIC_KEY],
                                                   e_pk)

    logger.debug("Send M2 to STA")
    m2, raw_m2_attrs = build_m2(authkey, raw_m1_attrs, eap_id,
                                m1_attrs[ATTR_ENROLLEE_NONCE],
                                r_nonce, uuid_r, e_pk)
    send_wsc_msg(dev[0], bssid, m2)
    eap_id = (eap_id + 1) % 256

    logger.debug("Receive M3 from STA")
    msg, m3_attrs, raw_m3_attrs = recv_wsc_msg(dev[0], WSC_MSG, WPS_M3)

    logger.debug("Send M4 to STA")
    attrs = build_wsc_attr(ATTR_VERSION, '\x10')
    attrs += build_attr_msg_type(WPS_M4)
    attrs += build_wsc_attr(ATTR_ENROLLEE_NONCE, m1_attrs[ATTR_ENROLLEE_NONCE])
    attrs += build_wsc_attr(ATTR_R_HASH1, r_hash1)
    attrs += build_wsc_attr(ATTR_R_HASH2, r_hash2)
    data = build_wsc_attr(ATTR_R_SNONCE1, r_s1)
    attrs += build_attr_encr_settings(authkey, keywrapkey, data)
    attrs += build_attr_authenticator(authkey, raw_m3_attrs, attrs)
    raw_m4_attrs = attrs
    m4 = build_eap_wsc(1, eap_id, attrs)
    send_wsc_msg(dev[0], bssid, m4)
    eap_id = (eap_id + 1) % 256

    logger.debug("Receive M5 from STA")
    msg, m5_attrs, raw_m5_attrs = recv_wsc_msg(dev[0], WSC_MSG, WPS_M5)

    logger.debug("Send NACK to STA")
    msg, attrs = build_nack(eap_id, m1_attrs[ATTR_ENROLLEE_NONCE],
                            r_nonce, config_error='\x01\x24')
    send_wsc_msg(dev[0], bssid, msg)
    ev = dev[0].wait_event(["WPS-FAIL"], timeout=5)
    if ev is None:
        raise Exception("Failure not reported")
    if "msg=9 config_error=292" not in ev:
        raise Exception("Unexpected failure reason: " + ev)

def wps_nack_m3(dev, apdev):
    pin = "00000000"
    addr, bssid, hapd = wps_start_ext(apdev[0], dev[0], pbc=True)
    wps_ext_eap_identity_req(dev[0], hapd, bssid)
    wps_ext_eap_identity_resp(hapd, dev[0], addr)
    wps_ext_eap_wsc(dev[0], hapd, bssid, "EAP-WSC/Start")

    mac_addr = binascii.unhexlify(dev[0].own_addr().replace(':', ''))
    uuid_r = 16*b'\x33'
    r_nonce = 16*b'\x44'
    own_private, e_pk = wsc_dh_init()

    logger.debug("Receive M1 from STA")
    msg, m1_attrs, raw_m1_attrs = recv_wsc_msg(dev[0], WSC_MSG, WPS_M1)
    eap_id = (msg['eap_identifier'] + 1) % 256

    authkey, keywrapkey = wsc_dh_kdf(m1_attrs[ATTR_PUBLIC_KEY], own_private,
                                     mac_addr, m1_attrs[ATTR_ENROLLEE_NONCE],
                                     r_nonce)
    r_s1, r_s2, r_hash1, r_hash2 = wsc_dev_pw_hash(authkey, pin,
                                                   m1_attrs[ATTR_PUBLIC_KEY],
                                                   e_pk)

    logger.debug("Send M2 to STA")
    m2, raw_m2_attrs = build_m2(authkey, raw_m1_attrs, eap_id,
                                m1_attrs[ATTR_ENROLLEE_NONCE],
                                r_nonce, uuid_r, e_pk, dev_pw_id='\x00\x04')
    send_wsc_msg(dev[0], bssid, m2)
    eap_id = (eap_id + 1) % 256

    logger.debug("Receive M3 from STA")
    msg, m3_attrs, raw_m3_attrs = recv_wsc_msg(dev[0], WSC_MSG, WPS_M3)
    return eap_id, m1_attrs[ATTR_ENROLLEE_NONCE], r_nonce, bssid

def test_wps_ext_proto_nack_m3_no_config_error(dev, apdev):
    """WPS and NACK M3 missing Config Error"""
    eap_id, e_nonce, r_nonce, bssid = wps_nack_m3(dev, apdev)
    logger.debug("Send NACK to STA")
    msg, attrs = build_nack(eap_id, e_nonce, r_nonce, config_error=None)
    send_wsc_msg(dev[0], bssid, msg)
    dev[0].request("WPS_CANCEL")
    dev[0].wait_disconnected()
    dev[0].flush_scan_cache()

def test_wps_ext_proto_nack_m3_no_e_nonce(dev, apdev):
    """WPS and NACK M3 missing E-Nonce"""
    eap_id, e_nonce, r_nonce, bssid = wps_nack_m3(dev, apdev)
    logger.debug("Send NACK to STA")
    msg, attrs = build_nack(eap_id, None, r_nonce)
    send_wsc_msg(dev[0], bssid, msg)
    dev[0].request("WPS_CANCEL")
    dev[0].wait_disconnected()
    dev[0].flush_scan_cache()

def test_wps_ext_proto_nack_m3_e_nonce_mismatch(dev, apdev):
    """WPS and NACK M3 E-Nonce mismatch"""
    eap_id, e_nonce, r_nonce, bssid = wps_nack_m3(dev, apdev)
    logger.debug("Send NACK to STA")
    msg, attrs = build_nack(eap_id, 16*'\x00', r_nonce)
    send_wsc_msg(dev[0], bssid, msg)
    dev[0].request("WPS_CANCEL")
    dev[0].wait_disconnected()
    dev[0].flush_scan_cache()

def test_wps_ext_proto_nack_m3_no_r_nonce(dev, apdev):
    """WPS and NACK M3 missing R-Nonce"""
    eap_id, e_nonce, r_nonce, bssid = wps_nack_m3(dev, apdev)
    logger.debug("Send NACK to STA")
    msg, attrs = build_nack(eap_id, e_nonce, None)
    send_wsc_msg(dev[0], bssid, msg)
    dev[0].request("WPS_CANCEL")
    dev[0].wait_disconnected()
    dev[0].flush_scan_cache()

def test_wps_ext_proto_nack_m3_r_nonce_mismatch(dev, apdev):
    """WPS and NACK M3 R-Nonce mismatch"""
    eap_id, e_nonce, r_nonce, bssid = wps_nack_m3(dev, apdev)
    logger.debug("Send NACK to STA")
    msg, attrs = build_nack(eap_id, e_nonce, 16*'\x00')
    send_wsc_msg(dev[0], bssid, msg)
    dev[0].request("WPS_CANCEL")
    dev[0].wait_disconnected()
    dev[0].flush_scan_cache()

def test_wps_ext_proto_nack_m3_no_msg_type(dev, apdev):
    """WPS and NACK M3 no Message Type"""
    eap_id, e_nonce, r_nonce, bssid = wps_nack_m3(dev, apdev)
    logger.debug("Send NACK to STA")
    msg, attrs = build_nack(eap_id, e_nonce, r_nonce, msg_type=None)
    send_wsc_msg(dev[0], bssid, msg)
    dev[0].request("WPS_CANCEL")
    dev[0].wait_disconnected()
    dev[0].flush_scan_cache()

def test_wps_ext_proto_nack_m3_invalid_msg_type(dev, apdev):
    """WPS and NACK M3 invalid Message Type"""
    eap_id, e_nonce, r_nonce, bssid = wps_nack_m3(dev, apdev)
    logger.debug("Send NACK to STA")
    msg, attrs = build_nack(eap_id, e_nonce, r_nonce, msg_type=123)
    send_wsc_msg(dev[0], bssid, msg)
    dev[0].request("WPS_CANCEL")
    dev[0].wait_disconnected()
    dev[0].flush_scan_cache()

def test_wps_ext_proto_nack_m3_invalid_attr(dev, apdev):
    """WPS and NACK M3 invalid attribute"""
    eap_id, e_nonce, r_nonce, bssid = wps_nack_m3(dev, apdev)
    logger.debug("Send NACK to STA")
    attrs = b'\x10\x10\x00'
    msg = build_eap_wsc(1, eap_id, attrs, opcode=WSC_NACK)
    send_wsc_msg(dev[0], bssid, msg)
    dev[0].request("WPS_CANCEL")
    dev[0].wait_disconnected()
    dev[0].flush_scan_cache()

def test_wps_ext_proto_ack_m3_no_e_nonce(dev, apdev):
    """WPS and ACK M3 missing E-Nonce"""
    eap_id, e_nonce, r_nonce, bssid = wps_nack_m3(dev, apdev)
    logger.debug("Send NACK to STA")
    msg, attrs = build_ack(eap_id, None, r_nonce)
    send_wsc_msg(dev[0], bssid, msg)
    dev[0].request("WPS_CANCEL")
    dev[0].wait_disconnected()
    dev[0].flush_scan_cache()

def test_wps_ext_proto_ack_m3_e_nonce_mismatch(dev, apdev):
    """WPS and ACK M3 E-Nonce mismatch"""
    eap_id, e_nonce, r_nonce, bssid = wps_nack_m3(dev, apdev)
    logger.debug("Send NACK to STA")
    msg, attrs = build_ack(eap_id, 16*'\x00', r_nonce)
    send_wsc_msg(dev[0], bssid, msg)
    dev[0].request("WPS_CANCEL")
    dev[0].wait_disconnected()
    dev[0].flush_scan_cache()

def test_wps_ext_proto_ack_m3_no_r_nonce(dev, apdev):
    """WPS and ACK M3 missing R-Nonce"""
    eap_id, e_nonce, r_nonce, bssid = wps_nack_m3(dev, apdev)
    logger.debug("Send NACK to STA")
    msg, attrs = build_ack(eap_id, e_nonce, None)
    send_wsc_msg(dev[0], bssid, msg)
    dev[0].request("WPS_CANCEL")
    dev[0].wait_disconnected()
    dev[0].flush_scan_cache()

def test_wps_ext_proto_ack_m3_r_nonce_mismatch(dev, apdev):
    """WPS and ACK M3 R-Nonce mismatch"""
    eap_id, e_nonce, r_nonce, bssid = wps_nack_m3(dev, apdev)
    logger.debug("Send NACK to STA")
    msg, attrs = build_ack(eap_id, e_nonce, 16*'\x00')
    send_wsc_msg(dev[0], bssid, msg)
    dev[0].request("WPS_CANCEL")
    dev[0].wait_disconnected()
    dev[0].flush_scan_cache()

def test_wps_ext_proto_ack_m3_no_msg_type(dev, apdev):
    """WPS and ACK M3 no Message Type"""
    eap_id, e_nonce, r_nonce, bssid = wps_nack_m3(dev, apdev)
    logger.debug("Send NACK to STA")
    msg, attrs = build_ack(eap_id, e_nonce, r_nonce, msg_type=None)
    send_wsc_msg(dev[0], bssid, msg)
    dev[0].request("WPS_CANCEL")
    dev[0].wait_disconnected()
    dev[0].flush_scan_cache()

def test_wps_ext_proto_ack_m3_invalid_msg_type(dev, apdev):
    """WPS and ACK M3 invalid Message Type"""
    eap_id, e_nonce, r_nonce, bssid = wps_nack_m3(dev, apdev)
    logger.debug("Send NACK to STA")
    msg, attrs = build_ack(eap_id, e_nonce, r_nonce, msg_type=123)
    send_wsc_msg(dev[0], bssid, msg)
    dev[0].request("WPS_CANCEL")
    dev[0].wait_disconnected()
    dev[0].flush_scan_cache()

def test_wps_ext_proto_ack_m3_invalid_attr(dev, apdev):
    """WPS and ACK M3 invalid attribute"""
    eap_id, e_nonce, r_nonce, bssid = wps_nack_m3(dev, apdev)
    logger.debug("Send ACK to STA")
    attrs = b'\x10\x10\x00'
    msg = build_eap_wsc(1, eap_id, attrs, opcode=WSC_ACK)
    send_wsc_msg(dev[0], bssid, msg)
    dev[0].request("WPS_CANCEL")
    dev[0].wait_disconnected()
    dev[0].flush_scan_cache()

def test_wps_ext_proto_ack_m3(dev, apdev):
    """WPS and ACK M3"""
    eap_id, e_nonce, r_nonce, bssid = wps_nack_m3(dev, apdev)
    logger.debug("Send ACK to STA")
    msg, attrs = build_ack(eap_id, e_nonce, r_nonce)
    send_wsc_msg(dev[0], bssid, msg)
    dev[0].request("WPS_CANCEL")
    dev[0].wait_disconnected()
    dev[0].flush_scan_cache()

def wps_to_m3_helper(dev, apdev):
    pin = "12345670"
    addr, bssid, hapd = wps_start_ext(apdev[0], dev[0], pin=pin)
    wps_ext_eap_identity_req(dev[0], hapd, bssid)
    wps_ext_eap_identity_resp(hapd, dev[0], addr)
    wps_ext_eap_wsc(dev[0], hapd, bssid, "EAP-WSC/Start")

    mac_addr = binascii.unhexlify(dev[0].own_addr().replace(':', ''))
    uuid_r = 16*b'\x33'
    r_nonce = 16*b'\x44'
    own_private, e_pk = wsc_dh_init()

    logger.debug("Receive M1 from STA")
    msg, m1_attrs, raw_m1_attrs = recv_wsc_msg(dev[0], WSC_MSG, WPS_M1)
    eap_id = (msg['eap_identifier'] + 1) % 256

    authkey, keywrapkey = wsc_dh_kdf(m1_attrs[ATTR_PUBLIC_KEY], own_private,
                                     mac_addr, m1_attrs[ATTR_ENROLLEE_NONCE],
                                     r_nonce)
    r_s1, r_s2, r_hash1, r_hash2 = wsc_dev_pw_hash(authkey, pin,
                                                   m1_attrs[ATTR_PUBLIC_KEY],
                                                   e_pk)

    logger.debug("Send M2 to STA")
    m2, raw_m2_attrs = build_m2(authkey, raw_m1_attrs, eap_id,
                                m1_attrs[ATTR_ENROLLEE_NONCE],
                                r_nonce, uuid_r, e_pk)
    send_wsc_msg(dev[0], bssid, m2)
    eap_id = (eap_id + 1) % 256

    logger.debug("Receive M3 from STA")
    msg, m3_attrs, raw_m3_attrs = recv_wsc_msg(dev[0], WSC_MSG, WPS_M3)
    return eap_id, m1_attrs, r_nonce, bssid, r_hash1, r_hash2, r_s1, r_s2, raw_m3_attrs, authkey, keywrapkey

def wps_to_m3(dev, apdev):
    eap_id, m1_attrs, r_nonce, bssid, r_hash1, r_hash2, r_s1, r_s2, raw_m3_attrs, authkey, keywrapkey = wps_to_m3_helper(dev, apdev)
    return eap_id, m1_attrs[ATTR_ENROLLEE_NONCE], r_nonce, bssid, r_hash1, r_hash2, r_s1, raw_m3_attrs, authkey, keywrapkey

def wps_to_m5(dev, apdev):
    eap_id, m1_attrs, r_nonce, bssid, r_hash1, r_hash2, r_s1, r_s2, raw_m3_attrs, authkey, keywrapkey = wps_to_m3_helper(dev, apdev)

    logger.debug("Send M4 to STA")
    attrs = build_wsc_attr(ATTR_VERSION, '\x10')
    attrs += build_attr_msg_type(WPS_M4)
    attrs += build_wsc_attr(ATTR_ENROLLEE_NONCE, m1_attrs[ATTR_ENROLLEE_NONCE])
    attrs += build_wsc_attr(ATTR_R_HASH1, r_hash1)
    attrs += build_wsc_attr(ATTR_R_HASH2, r_hash2)
    data = build_wsc_attr(ATTR_R_SNONCE1, r_s1)
    attrs += build_attr_encr_settings(authkey, keywrapkey, data)
    attrs += build_attr_authenticator(authkey, raw_m3_attrs, attrs)
    raw_m4_attrs = attrs
    m4 = build_eap_wsc(1, eap_id, attrs)
    send_wsc_msg(dev[0], bssid, m4)
    eap_id = (eap_id + 1) % 256

    logger.debug("Receive M5 from STA")
    msg, m5_attrs, raw_m5_attrs = recv_wsc_msg(dev[0], WSC_MSG, WPS_M5)

    return eap_id, m1_attrs[ATTR_ENROLLEE_NONCE], r_nonce, bssid, r_hash1, r_hash2, r_s2, raw_m5_attrs, authkey, keywrapkey

def test_wps_ext_proto_m4_missing_r_hash1(dev, apdev):
    """WPS and no R-Hash1 in M4"""
    eap_id, e_nonce, r_nonce, bssid, r_hash1, r_hash2, r_s1, m3, authkey, keywrapkey = wps_to_m3(dev, apdev)

    logger.debug("Send M4 to STA")
    attrs = build_wsc_attr(ATTR_VERSION, '\x10')
    attrs += build_attr_msg_type(WPS_M4)
    attrs += build_wsc_attr(ATTR_ENROLLEE_NONCE, e_nonce)
    #attrs += build_wsc_attr(ATTR_R_HASH1, r_hash1)
    attrs += build_wsc_attr(ATTR_R_HASH2, r_hash2)
    data = build_wsc_attr(ATTR_R_SNONCE1, r_s1)
    attrs += build_attr_encr_settings(authkey, keywrapkey, data)
    attrs += build_attr_authenticator(authkey, m3, attrs)
    m4 = build_eap_wsc(1, eap_id, attrs)
    send_wsc_msg(dev[0], bssid, m4)
    eap_id = (eap_id + 1) % 256

    logger.debug("Receive M5 (NACK) from STA")
    msg = get_wsc_msg(dev[0])
    if msg['wsc_opcode'] != WSC_NACK:
        raise Exception("Unexpected message - expected WSC_Nack")

    dev[0].request("WPS_CANCEL")
    send_wsc_msg(dev[0], bssid, build_eap_failure(eap_id))
    dev[0].wait_disconnected()

def test_wps_ext_proto_m4_missing_r_hash2(dev, apdev):
    """WPS and no R-Hash2 in M4"""
    eap_id, e_nonce, r_nonce, bssid, r_hash1, r_hash2, r_s1, m3, authkey, keywrapkey = wps_to_m3(dev, apdev)

    logger.debug("Send M4 to STA")
    attrs = build_wsc_attr(ATTR_VERSION, '\x10')
    attrs += build_attr_msg_type(WPS_M4)
    attrs += build_wsc_attr(ATTR_ENROLLEE_NONCE, e_nonce)
    attrs += build_wsc_attr(ATTR_R_HASH1, r_hash1)
    #attrs += build_wsc_attr(ATTR_R_HASH2, r_hash2)
    data = build_wsc_attr(ATTR_R_SNONCE1, r_s1)
    attrs += build_attr_encr_settings(authkey, keywrapkey, data)
    attrs += build_attr_authenticator(authkey, m3, attrs)
    m4 = build_eap_wsc(1, eap_id, attrs)
    send_wsc_msg(dev[0], bssid, m4)
    eap_id = (eap_id + 1) % 256

    logger.debug("Receive M5 (NACK) from STA")
    msg = get_wsc_msg(dev[0])
    if msg['wsc_opcode'] != WSC_NACK:
        raise Exception("Unexpected message - expected WSC_Nack")

    dev[0].request("WPS_CANCEL")
    send_wsc_msg(dev[0], bssid, build_eap_failure(eap_id))
    dev[0].wait_disconnected()

def test_wps_ext_proto_m4_missing_r_snonce1(dev, apdev):
    """WPS and no R-SNonce1 in M4"""
    eap_id, e_nonce, r_nonce, bssid, r_hash1, r_hash2, r_s1, m3, authkey, keywrapkey = wps_to_m3(dev, apdev)

    logger.debug("Send M4 to STA")
    attrs = build_wsc_attr(ATTR_VERSION, '\x10')
    attrs += build_attr_msg_type(WPS_M4)
    attrs += build_wsc_attr(ATTR_ENROLLEE_NONCE, e_nonce)
    attrs += build_wsc_attr(ATTR_R_HASH1, r_hash1)
    attrs += build_wsc_attr(ATTR_R_HASH2, r_hash2)
    #data = build_wsc_attr(ATTR_R_SNONCE1, r_s1)
    data = b''
    attrs += build_attr_encr_settings(authkey, keywrapkey, data)
    attrs += build_attr_authenticator(authkey, m3, attrs)
    m4 = build_eap_wsc(1, eap_id, attrs)
    send_wsc_msg(dev[0], bssid, m4)
    eap_id = (eap_id + 1) % 256

    logger.debug("Receive M5 (NACK) from STA")
    msg = get_wsc_msg(dev[0])
    if msg['wsc_opcode'] != WSC_NACK:
        raise Exception("Unexpected message - expected WSC_Nack")

    dev[0].request("WPS_CANCEL")
    send_wsc_msg(dev[0], bssid, build_eap_failure(eap_id))
    dev[0].wait_disconnected()

def test_wps_ext_proto_m4_invalid_pad_string(dev, apdev):
    """WPS and invalid pad string in M4"""
    eap_id, e_nonce, r_nonce, bssid, r_hash1, r_hash2, r_s1, m3, authkey, keywrapkey = wps_to_m3(dev, apdev)

    logger.debug("Send M4 to STA")
    attrs = build_wsc_attr(ATTR_VERSION, '\x10')
    attrs += build_attr_msg_type(WPS_M4)
    attrs += build_wsc_attr(ATTR_ENROLLEE_NONCE, e_nonce)
    attrs += build_wsc_attr(ATTR_R_HASH1, r_hash1)
    attrs += build_wsc_attr(ATTR_R_HASH2, r_hash2)
    data = build_wsc_attr(ATTR_R_SNONCE1, r_s1)

    m = hmac.new(authkey, data, hashlib.sha256)
    kwa = m.digest()[0:8]
    data += build_wsc_attr(ATTR_KEY_WRAP_AUTH, kwa)
    iv = 16*b'\x99'
    aes = AES.new(keywrapkey, AES.MODE_CBC, iv)
    pad_len = 16 - len(data) % 16
    ps = (pad_len - 1) * struct.pack('B', pad_len) + struct.pack('B', pad_len - 1)
    data += ps
    wrapped = aes.encrypt(data)
    attrs += build_wsc_attr(ATTR_ENCR_SETTINGS, iv + wrapped)

    attrs += build_attr_authenticator(authkey, m3, attrs)
    m4 = build_eap_wsc(1, eap_id, attrs)
    send_wsc_msg(dev[0], bssid, m4)
    eap_id = (eap_id + 1) % 256

    logger.debug("Receive M5 (NACK) from STA")
    msg = get_wsc_msg(dev[0])
    if msg['wsc_opcode'] != WSC_NACK:
        raise Exception("Unexpected message - expected WSC_Nack")

    dev[0].request("WPS_CANCEL")
    send_wsc_msg(dev[0], bssid, build_eap_failure(eap_id))
    dev[0].wait_disconnected()

def test_wps_ext_proto_m4_invalid_pad_value(dev, apdev):
    """WPS and invalid pad value in M4"""
    eap_id, e_nonce, r_nonce, bssid, r_hash1, r_hash2, r_s1, m3, authkey, keywrapkey = wps_to_m3(dev, apdev)

    logger.debug("Send M4 to STA")
    attrs = build_wsc_attr(ATTR_VERSION, '\x10')
    attrs += build_attr_msg_type(WPS_M4)
    attrs += build_wsc_attr(ATTR_ENROLLEE_NONCE, e_nonce)
    attrs += build_wsc_attr(ATTR_R_HASH1, r_hash1)
    attrs += build_wsc_attr(ATTR_R_HASH2, r_hash2)
    data = build_wsc_attr(ATTR_R_SNONCE1, r_s1)

    m = hmac.new(authkey, data, hashlib.sha256)
    kwa = m.digest()[0:8]
    data += build_wsc_attr(ATTR_KEY_WRAP_AUTH, kwa)
    iv = 16*b'\x99'
    aes = AES.new(keywrapkey, AES.MODE_CBC, iv)
    pad_len = 16 - len(data) % 16
    ps = (pad_len - 1) * struct.pack('B', pad_len) + struct.pack('B', 255)
    data += ps
    wrapped = aes.encrypt(data)
    attrs += build_wsc_attr(ATTR_ENCR_SETTINGS, iv + wrapped)

    attrs += build_attr_authenticator(authkey, m3, attrs)
    m4 = build_eap_wsc(1, eap_id, attrs)
    send_wsc_msg(dev[0], bssid, m4)
    eap_id = (eap_id + 1) % 256

    logger.debug("Receive M5 (NACK) from STA")
    msg = get_wsc_msg(dev[0])
    if msg['wsc_opcode'] != WSC_NACK:
        raise Exception("Unexpected message - expected WSC_Nack")

    dev[0].request("WPS_CANCEL")
    send_wsc_msg(dev[0], bssid, build_eap_failure(eap_id))
    dev[0].wait_disconnected()

def test_wps_ext_proto_m4_no_encr_settings(dev, apdev):
    """WPS and no Encr Settings in M4"""
    eap_id, e_nonce, r_nonce, bssid, r_hash1, r_hash2, r_s1, m3, authkey, keywrapkey = wps_to_m3(dev, apdev)

    logger.debug("Send M4 to STA")
    attrs = build_wsc_attr(ATTR_VERSION, '\x10')
    attrs += build_attr_msg_type(WPS_M4)
    attrs += build_wsc_attr(ATTR_ENROLLEE_NONCE, e_nonce)
    attrs += build_wsc_attr(ATTR_R_HASH1, r_hash1)
    attrs += build_wsc_attr(ATTR_R_HASH2, r_hash2)
    attrs += build_attr_authenticator(authkey, m3, attrs)
    m4 = build_eap_wsc(1, eap_id, attrs)
    send_wsc_msg(dev[0], bssid, m4)
    eap_id = (eap_id + 1) % 256

    logger.debug("Receive M5 (NACK) from STA")
    msg = get_wsc_msg(dev[0])
    if msg['wsc_opcode'] != WSC_NACK:
        raise Exception("Unexpected message - expected WSC_Nack")

    dev[0].request("WPS_CANCEL")
    send_wsc_msg(dev[0], bssid, build_eap_failure(eap_id))
    dev[0].wait_disconnected()

def test_wps_ext_proto_m6_missing_r_snonce2(dev, apdev):
    """WPS and no R-SNonce2 in M6"""
    eap_id, e_nonce, r_nonce, bssid, r_hash1, r_hash2, r_s2, m5, authkey, keywrapkey = wps_to_m5(dev, apdev)

    logger.debug("Send M6 to STA")
    attrs = build_wsc_attr(ATTR_VERSION, '\x10')
    attrs += build_attr_msg_type(WPS_M6)
    attrs += build_wsc_attr(ATTR_ENROLLEE_NONCE, e_nonce)
    #data = build_wsc_attr(ATTR_R_SNONCE2, r_s2)
    data = b''
    attrs += build_attr_encr_settings(authkey, keywrapkey, data)
    attrs += build_attr_authenticator(authkey, m5, attrs)
    m6 = build_eap_wsc(1, eap_id, attrs)
    send_wsc_msg(dev[0], bssid, m6)
    eap_id = (eap_id + 1) % 256

    logger.debug("Receive M7 (NACK) from STA")
    msg = get_wsc_msg(dev[0])
    if msg['wsc_opcode'] != WSC_NACK:
        raise Exception("Unexpected message - expected WSC_Nack")

    dev[0].request("WPS_CANCEL")
    send_wsc_msg(dev[0], bssid, build_eap_failure(eap_id))
    dev[0].wait_disconnected()

def test_wps_ext_proto_m6_no_encr_settings(dev, apdev):
    """WPS and no Encr Settings in M6"""
    eap_id, e_nonce, r_nonce, bssid, r_hash1, r_hash2, r_s2, m5, authkey, keywrapkey = wps_to_m5(dev, apdev)

    logger.debug("Send M6 to STA")
    attrs = build_wsc_attr(ATTR_VERSION, '\x10')
    attrs += build_attr_msg_type(WPS_M6)
    attrs += build_wsc_attr(ATTR_ENROLLEE_NONCE, e_nonce)
    data = build_wsc_attr(ATTR_R_SNONCE2, r_s2)
    #attrs += build_attr_encr_settings(authkey, keywrapkey, data)
    attrs += build_attr_authenticator(authkey, m5, attrs)
    m6 = build_eap_wsc(1, eap_id, attrs)
    send_wsc_msg(dev[0], bssid, m6)
    eap_id = (eap_id + 1) % 256

    logger.debug("Receive M7 (NACK) from STA")
    msg = get_wsc_msg(dev[0])
    if msg['wsc_opcode'] != WSC_NACK:
        raise Exception("Unexpected message - expected WSC_Nack")

    dev[0].request("WPS_CANCEL")
    send_wsc_msg(dev[0], bssid, build_eap_failure(eap_id))
    dev[0].wait_disconnected()

def test_wps_ext_proto_m8_no_encr_settings(dev, apdev):
    """WPS and no Encr Settings in M6"""
    eap_id, e_nonce, r_nonce, bssid, r_hash1, r_hash2, r_s2, m5, authkey, keywrapkey = wps_to_m5(dev, apdev)

    logger.debug("Send M6 to STA")
    attrs = build_wsc_attr(ATTR_VERSION, '\x10')
    attrs += build_attr_msg_type(WPS_M6)
    attrs += build_wsc_attr(ATTR_ENROLLEE_NONCE, e_nonce)
    data = build_wsc_attr(ATTR_R_SNONCE2, r_s2)
    attrs += build_attr_encr_settings(authkey, keywrapkey, data)
    attrs += build_attr_authenticator(authkey, m5, attrs)
    raw_m6_attrs = attrs
    m6 = build_eap_wsc(1, eap_id, attrs)
    send_wsc_msg(dev[0], bssid, m6)
    eap_id = (eap_id + 1) % 256

    logger.debug("Receive M7 from STA")
    msg, m7_attrs, raw_m7_attrs = recv_wsc_msg(dev[0], WSC_MSG, WPS_M7)

    logger.debug("Send M8 to STA")
    attrs = build_wsc_attr(ATTR_VERSION, '\x10')
    attrs += build_attr_msg_type(WPS_M8)
    attrs += build_wsc_attr(ATTR_ENROLLEE_NONCE, e_nonce)
    #attrs += build_attr_encr_settings(authkey, keywrapkey, m8_cred)
    attrs += build_attr_authenticator(authkey, raw_m7_attrs, attrs)
    raw_m8_attrs = attrs
    m8 = build_eap_wsc(1, eap_id, attrs)
    send_wsc_msg(dev[0], bssid, m8)

    logger.debug("Receive WSC_Done (NACK) from STA")
    msg = get_wsc_msg(dev[0])
    if msg['wsc_opcode'] != WSC_NACK:
        raise Exception("Unexpected message - expected WSC_Nack")

    dev[0].request("WPS_CANCEL")
    send_wsc_msg(dev[0], bssid, build_eap_failure(eap_id))
    dev[0].wait_disconnected()

def wps_start_ext_reg(apdev, dev):
    addr = dev.own_addr()
    bssid = apdev['bssid']
    ssid = "test-wps-conf"
    appin = "12345670"
    params = {"ssid": ssid, "eap_server": "1", "wps_state": "2",
              "wpa_passphrase": "12345678", "wpa": "2",
              "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP",
              "ap_pin": appin}
    hapd = hostapd.add_ap(apdev, params)

    dev.scan_for_bss(bssid, freq="2412")
    hapd.request("SET ext_eapol_frame_io 1")
    dev.request("SET ext_eapol_frame_io 1")

    dev.request("WPS_REG " + bssid + " " + appin)

    return addr, bssid, hapd

def wps_run_ap_settings_proto(dev, apdev, ap_settings, success):
    addr, bssid, hapd = wps_start_ext_reg(apdev[0], dev[0])
    wps_ext_eap_identity_req(dev[0], hapd, bssid)
    wps_ext_eap_identity_resp(hapd, dev[0], addr)

    logger.debug("Receive M1 from AP")
    msg, m1_attrs, raw_m1_attrs = recv_wsc_msg(hapd, WSC_MSG, WPS_M1)
    mac_addr = m1_attrs[ATTR_MAC_ADDR]
    e_nonce = m1_attrs[ATTR_ENROLLEE_NONCE]
    e_pk = m1_attrs[ATTR_PUBLIC_KEY]

    appin = '12345670'
    uuid_r = 16*b'\x33'
    r_nonce = 16*b'\x44'
    own_private, r_pk = wsc_dh_init()
    authkey, keywrapkey = wsc_dh_kdf(e_pk, own_private, mac_addr, e_nonce,
                                     r_nonce)
    r_s1, r_s2, r_hash1, r_hash2 = wsc_dev_pw_hash(authkey, appin, e_pk, r_pk)

    logger.debug("Send M2 to AP")
    m2, raw_m2_attrs = build_m2(authkey, raw_m1_attrs, msg['eap_identifier'],
                                e_nonce, r_nonce, uuid_r, r_pk, eap_code=2)
    send_wsc_msg(hapd, addr, m2)

    logger.debug("Receive M3 from AP")
    msg, m3_attrs, raw_m3_attrs = recv_wsc_msg(hapd, WSC_MSG, WPS_M3)

    logger.debug("Send M4 to AP")
    attrs = build_wsc_attr(ATTR_VERSION, '\x10')
    attrs += build_attr_msg_type(WPS_M4)
    attrs += build_wsc_attr(ATTR_ENROLLEE_NONCE, e_nonce)
    attrs += build_wsc_attr(ATTR_R_HASH1, r_hash1)
    attrs += build_wsc_attr(ATTR_R_HASH2, r_hash2)
    data = build_wsc_attr(ATTR_R_SNONCE1, r_s1)
    attrs += build_attr_encr_settings(authkey, keywrapkey, data)
    attrs += build_attr_authenticator(authkey, raw_m3_attrs, attrs)
    raw_m4_attrs = attrs
    m4 = build_eap_wsc(2, msg['eap_identifier'], attrs)
    send_wsc_msg(hapd, addr, m4)

    logger.debug("Receive M5 from AP")
    msg, m5_attrs, raw_m5_attrs = recv_wsc_msg(hapd, WSC_MSG, WPS_M5)

    logger.debug("Send M6 to STA")
    attrs = build_wsc_attr(ATTR_VERSION, '\x10')
    attrs += build_attr_msg_type(WPS_M6)
    attrs += build_wsc_attr(ATTR_ENROLLEE_NONCE, e_nonce)
    data = build_wsc_attr(ATTR_R_SNONCE2, r_s2)
    attrs += build_attr_encr_settings(authkey, keywrapkey, data)
    attrs += build_attr_authenticator(authkey, raw_m5_attrs, attrs)
    raw_m6_attrs = attrs
    m6 = build_eap_wsc(2, msg['eap_identifier'], attrs)
    send_wsc_msg(hapd, addr, m6)

    logger.debug("Receive M7 from AP")
    msg, m7_attrs, raw_m7_attrs = recv_wsc_msg(hapd, WSC_MSG, WPS_M7)

    logger.debug("Send M8 to STA")
    attrs = build_wsc_attr(ATTR_VERSION, '\x10')
    attrs += build_attr_msg_type(WPS_M8)
    attrs += build_wsc_attr(ATTR_ENROLLEE_NONCE, e_nonce)
    if ap_settings:
        attrs += build_attr_encr_settings(authkey, keywrapkey, ap_settings)
    attrs += build_attr_authenticator(authkey, raw_m7_attrs, attrs)
    raw_m8_attrs = attrs
    m8 = build_eap_wsc(2, msg['eap_identifier'], attrs)
    send_wsc_msg(hapd, addr, m8)

    if success:
        ev = hapd.wait_event(["WPS-NEW-AP-SETTINGS"], timeout=5)
        if ev is None:
            raise Exception("New AP settings not reported")
        logger.debug("Receive WSC_Done from AP")
        msg = get_wsc_msg(hapd)
        if msg['wsc_opcode'] != WSC_Done:
            raise Exception("Unexpected message - expected WSC_Done")

        logger.debug("Send WSC_ACK to AP")
        ack, attrs = build_ack(msg['eap_identifier'], e_nonce, r_nonce,
                               eap_code=2)
        send_wsc_msg(hapd, addr, ack)
        dev[0].wait_disconnected()
    else:
        ev = hapd.wait_event(["WPS-FAIL"], timeout=5)
        if ev is None:
            raise Exception("WPS failure not reported")
        logger.debug("Receive WSC_NACK from AP")
        msg = get_wsc_msg(hapd)
        if msg['wsc_opcode'] != WSC_NACK:
            raise Exception("Unexpected message - expected WSC_NACK")

        logger.debug("Send WSC_NACK to AP")
        nack, attrs = build_nack(msg['eap_identifier'], e_nonce, r_nonce,
                                 eap_code=2)
        send_wsc_msg(hapd, addr, nack)
        dev[0].wait_disconnected()

def test_wps_ext_ap_settings_success(dev, apdev):
    """WPS and AP Settings: success"""
    ap_settings = build_wsc_attr(ATTR_NETWORK_INDEX, '\x01')
    ap_settings += build_wsc_attr(ATTR_SSID, "test")
    ap_settings += build_wsc_attr(ATTR_AUTH_TYPE, '\x00\x01')
    ap_settings += build_wsc_attr(ATTR_ENCR_TYPE, '\x00\x01')
    ap_settings += build_wsc_attr(ATTR_NETWORK_KEY, '')
    ap_settings += build_wsc_attr(ATTR_MAC_ADDR, binascii.unhexlify(apdev[0]['bssid'].replace(':', '')))
    wps_run_ap_settings_proto(dev, apdev, ap_settings, True)

@remote_compatible
def test_wps_ext_ap_settings_missing(dev, apdev):
    """WPS and AP Settings: missing"""
    wps_run_ap_settings_proto(dev, apdev, None, False)

@remote_compatible
def test_wps_ext_ap_settings_mac_addr_mismatch(dev, apdev):
    """WPS and AP Settings: MAC Address mismatch"""
    ap_settings = build_wsc_attr(ATTR_NETWORK_INDEX, '\x01')
    ap_settings += build_wsc_attr(ATTR_SSID, "test")
    ap_settings += build_wsc_attr(ATTR_AUTH_TYPE, '\x00\x01')
    ap_settings += build_wsc_attr(ATTR_ENCR_TYPE, '\x00\x01')
    ap_settings += build_wsc_attr(ATTR_NETWORK_KEY, '')
    ap_settings += build_wsc_attr(ATTR_MAC_ADDR, '\x00\x00\x00\x00\x00\x00')
    wps_run_ap_settings_proto(dev, apdev, ap_settings, True)

@remote_compatible
def test_wps_ext_ap_settings_mac_addr_missing(dev, apdev):
    """WPS and AP Settings: missing MAC Address"""
    ap_settings = build_wsc_attr(ATTR_NETWORK_INDEX, '\x01')
    ap_settings += build_wsc_attr(ATTR_SSID, "test")
    ap_settings += build_wsc_attr(ATTR_AUTH_TYPE, '\x00\x01')
    ap_settings += build_wsc_attr(ATTR_ENCR_TYPE, '\x00\x01')
    ap_settings += build_wsc_attr(ATTR_NETWORK_KEY, '')
    wps_run_ap_settings_proto(dev, apdev, ap_settings, False)

@remote_compatible
def test_wps_ext_ap_settings_reject_encr_type(dev, apdev):
    """WPS and AP Settings: reject Encr Type"""
    ap_settings = build_wsc_attr(ATTR_NETWORK_INDEX, '\x01')
    ap_settings += build_wsc_attr(ATTR_SSID, "test")
    ap_settings += build_wsc_attr(ATTR_AUTH_TYPE, '\x00\x01')
    ap_settings += build_wsc_attr(ATTR_ENCR_TYPE, '\x00\x00')
    ap_settings += build_wsc_attr(ATTR_NETWORK_KEY, '')
    ap_settings += build_wsc_attr(ATTR_MAC_ADDR, binascii.unhexlify(apdev[0]['bssid'].replace(':', '')))
    wps_run_ap_settings_proto(dev, apdev, ap_settings, False)

@remote_compatible
def test_wps_ext_ap_settings_m2d(dev, apdev):
    """WPS and AP Settings: M2D"""
    addr, bssid, hapd = wps_start_ext_reg(apdev[0], dev[0])
    wps_ext_eap_identity_req(dev[0], hapd, bssid)
    wps_ext_eap_identity_resp(hapd, dev[0], addr)

    logger.debug("Receive M1 from AP")
    msg, m1_attrs, raw_m1_attrs = recv_wsc_msg(hapd, WSC_MSG, WPS_M1)
    e_nonce = m1_attrs[ATTR_ENROLLEE_NONCE]

    r_nonce = 16*'\x44'
    uuid_r = 16*'\x33'

    logger.debug("Send M2D to AP")
    m2d, raw_m2d_attrs = build_m2d(raw_m1_attrs, msg['eap_identifier'],
                                   e_nonce, r_nonce, uuid_r,
                                   dev_pw_id='\x00\x00', eap_code=2)
    send_wsc_msg(hapd, addr, m2d)

    ev = hapd.wait_event(["WPS-M2D"], timeout=5)
    if ev is None:
        raise Exception("M2D not reported")

    wps_wait_ap_nack(hapd, dev[0], e_nonce, r_nonce)

def wps_wait_ap_nack(hapd, dev, e_nonce, r_nonce):
    logger.debug("Receive WSC_NACK from AP")
    msg = get_wsc_msg(hapd)
    if msg['wsc_opcode'] != WSC_NACK:
        raise Exception("Unexpected message - expected WSC_NACK")

    logger.debug("Send WSC_NACK to AP")
    nack, attrs = build_nack(msg['eap_identifier'], e_nonce, r_nonce,
                             eap_code=2)
    send_wsc_msg(hapd, dev.own_addr(), nack)
    dev.wait_disconnected()

@remote_compatible
def test_wps_ext_m3_missing_e_hash1(dev, apdev):
    """WPS proto: M3 missing E-Hash1"""
    pin = "12345670"
    addr, bssid, hapd = wps_start_ext(apdev[0], dev[0], pin=pin)
    wps_ext_eap_identity_req(dev[0], hapd, bssid)
    wps_ext_eap_identity_resp(hapd, dev[0], addr)

    logger.debug("Receive WSC/Start from AP")
    msg = get_wsc_msg(hapd)
    if msg['wsc_opcode'] != WSC_Start:
        raise Exception("Unexpected Op-Code for WSC/Start")

    mac_addr = binascii.unhexlify(dev[0].own_addr().replace(':', ''))
    uuid_e = 16*b'\x11'
    e_nonce = 16*b'\x22'
    own_private, e_pk = wsc_dh_init()

    logger.debug("Send M1 to AP")
    m1, raw_m1_attrs = build_m1(msg['eap_identifier'], uuid_e, mac_addr,
                                e_nonce, e_pk)
    send_wsc_msg(hapd, addr, m1)

    logger.debug("Receive M2 from AP")
    msg, m2_attrs, raw_m2_attrs = recv_wsc_msg(hapd, WSC_MSG, WPS_M2)
    r_nonce = m2_attrs[ATTR_REGISTRAR_NONCE]
    r_pk = m2_attrs[ATTR_PUBLIC_KEY]

    authkey, keywrapkey = wsc_dh_kdf(r_pk, own_private, mac_addr, e_nonce,
                                     r_nonce)
    e_s1, e_s2, e_hash1, e_hash2 = wsc_dev_pw_hash(authkey, pin, e_pk, r_pk)

    logger.debug("Send M3 to AP")
    attrs = build_wsc_attr(ATTR_VERSION, '\x10')
    attrs += build_attr_msg_type(WPS_M3)
    attrs += build_wsc_attr(ATTR_REGISTRAR_NONCE, r_nonce)
    #attrs += build_wsc_attr(ATTR_E_HASH1, e_hash1)
    attrs += build_wsc_attr(ATTR_E_HASH2, e_hash2)
    attrs += build_attr_authenticator(authkey, raw_m2_attrs, attrs)
    raw_m3_attrs = attrs
    m3 = build_eap_wsc(2, msg['eap_identifier'], attrs)
    send_wsc_msg(hapd, addr, m3)

    wps_wait_ap_nack(hapd, dev[0], e_nonce, r_nonce)

@remote_compatible
def test_wps_ext_m3_missing_e_hash2(dev, apdev):
    """WPS proto: M3 missing E-Hash2"""
    pin = "12345670"
    addr, bssid, hapd = wps_start_ext(apdev[0], dev[0], pin=pin)
    wps_ext_eap_identity_req(dev[0], hapd, bssid)
    wps_ext_eap_identity_resp(hapd, dev[0], addr)

    logger.debug("Receive WSC/Start from AP")
    msg = get_wsc_msg(hapd)
    if msg['wsc_opcode'] != WSC_Start:
        raise Exception("Unexpected Op-Code for WSC/Start")

    mac_addr = binascii.unhexlify(dev[0].own_addr().replace(':', ''))
    uuid_e = 16*b'\x11'
    e_nonce = 16*b'\x22'
    own_private, e_pk = wsc_dh_init()

    logger.debug("Send M1 to AP")
    m1, raw_m1_attrs = build_m1(msg['eap_identifier'], uuid_e, mac_addr,
                                e_nonce, e_pk)
    send_wsc_msg(hapd, addr, m1)

    logger.debug("Receive M2 from AP")
    msg, m2_attrs, raw_m2_attrs = recv_wsc_msg(hapd, WSC_MSG, WPS_M2)
    r_nonce = m2_attrs[ATTR_REGISTRAR_NONCE]
    r_pk = m2_attrs[ATTR_PUBLIC_KEY]

    authkey, keywrapkey = wsc_dh_kdf(r_pk, own_private, mac_addr, e_nonce,
                                     r_nonce)
    e_s1, e_s2, e_hash1, e_hash2 = wsc_dev_pw_hash(authkey, pin, e_pk, r_pk)

    logger.debug("Send M3 to AP")
    attrs = build_wsc_attr(ATTR_VERSION, '\x10')
    attrs += build_attr_msg_type(WPS_M3)
    attrs += build_wsc_attr(ATTR_REGISTRAR_NONCE, r_nonce)
    attrs += build_wsc_attr(ATTR_E_HASH1, e_hash1)
    #attrs += build_wsc_attr(ATTR_E_HASH2, e_hash2)
    attrs += build_attr_authenticator(authkey, raw_m2_attrs, attrs)
    raw_m3_attrs = attrs
    m3 = build_eap_wsc(2, msg['eap_identifier'], attrs)
    send_wsc_msg(hapd, addr, m3)

    wps_wait_ap_nack(hapd, dev[0], e_nonce, r_nonce)

@remote_compatible
def test_wps_ext_m5_missing_e_snonce1(dev, apdev):
    """WPS proto: M5 missing E-SNonce1"""
    pin = "12345670"
    addr, bssid, hapd = wps_start_ext(apdev[0], dev[0], pin=pin)
    wps_ext_eap_identity_req(dev[0], hapd, bssid)
    wps_ext_eap_identity_resp(hapd, dev[0], addr)

    logger.debug("Receive WSC/Start from AP")
    msg = get_wsc_msg(hapd)
    if msg['wsc_opcode'] != WSC_Start:
        raise Exception("Unexpected Op-Code for WSC/Start")

    mac_addr = binascii.unhexlify(dev[0].own_addr().replace(':', ''))
    uuid_e = 16*b'\x11'
    e_nonce = 16*b'\x22'
    own_private, e_pk = wsc_dh_init()

    logger.debug("Send M1 to AP")
    m1, raw_m1_attrs = build_m1(msg['eap_identifier'], uuid_e, mac_addr,
                                e_nonce, e_pk)
    send_wsc_msg(hapd, addr, m1)

    logger.debug("Receive M2 from AP")
    msg, m2_attrs, raw_m2_attrs = recv_wsc_msg(hapd, WSC_MSG, WPS_M2)
    r_nonce = m2_attrs[ATTR_REGISTRAR_NONCE]
    r_pk = m2_attrs[ATTR_PUBLIC_KEY]

    authkey, keywrapkey = wsc_dh_kdf(r_pk, own_private, mac_addr, e_nonce,
                                     r_nonce)
    e_s1, e_s2, e_hash1, e_hash2 = wsc_dev_pw_hash(authkey, pin, e_pk, r_pk)

    logger.debug("Send M3 to AP")
    attrs = build_wsc_attr(ATTR_VERSION, '\x10')
    attrs += build_attr_msg_type(WPS_M3)
    attrs += build_wsc_attr(ATTR_REGISTRAR_NONCE, r_nonce)
    attrs += build_wsc_attr(ATTR_E_HASH1, e_hash1)
    attrs += build_wsc_attr(ATTR_E_HASH2, e_hash2)
    attrs += build_attr_authenticator(authkey, raw_m2_attrs, attrs)
    raw_m3_attrs = attrs
    m3 = build_eap_wsc(2, msg['eap_identifier'], attrs)
    send_wsc_msg(hapd, addr, m3)

    logger.debug("Receive M4 from AP")
    msg, m4_attrs, raw_m4_attrs = recv_wsc_msg(hapd, WSC_MSG, WPS_M4)

    logger.debug("Send M5 to AP")
    attrs = build_wsc_attr(ATTR_VERSION, '\x10')
    attrs += build_attr_msg_type(WPS_M5)
    attrs += build_wsc_attr(ATTR_REGISTRAR_NONCE, r_nonce)
    #data = build_wsc_attr(ATTR_E_SNONCE1, e_s1)
    data = b''
    attrs += build_attr_encr_settings(authkey, keywrapkey, data)
    attrs += build_attr_authenticator(authkey, raw_m4_attrs, attrs)
    raw_m5_attrs = attrs
    m5 = build_eap_wsc(2, msg['eap_identifier'], attrs)
    send_wsc_msg(hapd, addr, m5)

    wps_wait_ap_nack(hapd, dev[0], e_nonce, r_nonce)

@remote_compatible
def test_wps_ext_m5_e_snonce1_mismatch(dev, apdev):
    """WPS proto: M5 E-SNonce1 mismatch"""
    pin = "12345670"
    addr, bssid, hapd = wps_start_ext(apdev[0], dev[0], pin=pin)
    wps_ext_eap_identity_req(dev[0], hapd, bssid)
    wps_ext_eap_identity_resp(hapd, dev[0], addr)

    logger.debug("Receive WSC/Start from AP")
    msg = get_wsc_msg(hapd)
    if msg['wsc_opcode'] != WSC_Start:
        raise Exception("Unexpected Op-Code for WSC/Start")

    mac_addr = binascii.unhexlify(dev[0].own_addr().replace(':', ''))
    uuid_e = 16*b'\x11'
    e_nonce = 16*b'\x22'
    own_private, e_pk = wsc_dh_init()

    logger.debug("Send M1 to AP")
    m1, raw_m1_attrs = build_m1(msg['eap_identifier'], uuid_e, mac_addr,
                                e_nonce, e_pk)
    send_wsc_msg(hapd, addr, m1)

    logger.debug("Receive M2 from AP")
    msg, m2_attrs, raw_m2_attrs = recv_wsc_msg(hapd, WSC_MSG, WPS_M2)
    r_nonce = m2_attrs[ATTR_REGISTRAR_NONCE]
    r_pk = m2_attrs[ATTR_PUBLIC_KEY]

    authkey, keywrapkey = wsc_dh_kdf(r_pk, own_private, mac_addr, e_nonce,
                                     r_nonce)
    e_s1, e_s2, e_hash1, e_hash2 = wsc_dev_pw_hash(authkey, pin, e_pk, r_pk)

    logger.debug("Send M3 to AP")
    attrs = build_wsc_attr(ATTR_VERSION, '\x10')
    attrs += build_attr_msg_type(WPS_M3)
    attrs += build_wsc_attr(ATTR_REGISTRAR_NONCE, r_nonce)
    attrs += build_wsc_attr(ATTR_E_HASH1, e_hash1)
    attrs += build_wsc_attr(ATTR_E_HASH2, e_hash2)
    attrs += build_attr_authenticator(authkey, raw_m2_attrs, attrs)
    raw_m3_attrs = attrs
    m3 = build_eap_wsc(2, msg['eap_identifier'], attrs)
    send_wsc_msg(hapd, addr, m3)

    logger.debug("Receive M4 from AP")
    msg, m4_attrs, raw_m4_attrs = recv_wsc_msg(hapd, WSC_MSG, WPS_M4)

    logger.debug("Send M5 to AP")
    attrs = build_wsc_attr(ATTR_VERSION, '\x10')
    attrs += build_attr_msg_type(WPS_M5)
    attrs += build_wsc_attr(ATTR_REGISTRAR_NONCE, r_nonce)
    data = build_wsc_attr(ATTR_E_SNONCE1, 16*'\x00')
    attrs += build_attr_encr_settings(authkey, keywrapkey, data)
    attrs += build_attr_authenticator(authkey, raw_m4_attrs, attrs)
    raw_m5_attrs = attrs
    m5 = build_eap_wsc(2, msg['eap_identifier'], attrs)
    send_wsc_msg(hapd, addr, m5)

    wps_wait_ap_nack(hapd, dev[0], e_nonce, r_nonce)

def test_wps_ext_m7_missing_e_snonce2(dev, apdev):
    """WPS proto: M7 missing E-SNonce2"""
    pin = "12345670"
    addr, bssid, hapd = wps_start_ext(apdev[0], dev[0], pin=pin)
    wps_ext_eap_identity_req(dev[0], hapd, bssid)
    wps_ext_eap_identity_resp(hapd, dev[0], addr)

    logger.debug("Receive WSC/Start from AP")
    msg = get_wsc_msg(hapd)
    if msg['wsc_opcode'] != WSC_Start:
        raise Exception("Unexpected Op-Code for WSC/Start")

    mac_addr = binascii.unhexlify(dev[0].own_addr().replace(':', ''))
    uuid_e = 16*b'\x11'
    e_nonce = 16*b'\x22'
    own_private, e_pk = wsc_dh_init()

    logger.debug("Send M1 to AP")
    m1, raw_m1_attrs = build_m1(msg['eap_identifier'], uuid_e, mac_addr,
                                e_nonce, e_pk)
    send_wsc_msg(hapd, addr, m1)

    logger.debug("Receive M2 from AP")
    msg, m2_attrs, raw_m2_attrs = recv_wsc_msg(hapd, WSC_MSG, WPS_M2)
    r_nonce = m2_attrs[ATTR_REGISTRAR_NONCE]
    r_pk = m2_attrs[ATTR_PUBLIC_KEY]

    authkey, keywrapkey = wsc_dh_kdf(r_pk, own_private, mac_addr, e_nonce,
                                     r_nonce)
    e_s1, e_s2, e_hash1, e_hash2 = wsc_dev_pw_hash(authkey, pin, e_pk, r_pk)

    logger.debug("Send M3 to AP")
    attrs = build_wsc_attr(ATTR_VERSION, '\x10')
    attrs += build_attr_msg_type(WPS_M3)
    attrs += build_wsc_attr(ATTR_REGISTRAR_NONCE, r_nonce)
    attrs += build_wsc_attr(ATTR_E_HASH1, e_hash1)
    attrs += build_wsc_attr(ATTR_E_HASH2, e_hash2)
    attrs += build_attr_authenticator(authkey, raw_m2_attrs, attrs)
    raw_m3_attrs = attrs
    m3 = build_eap_wsc(2, msg['eap_identifier'], attrs)
    send_wsc_msg(hapd, addr, m3)

    logger.debug("Receive M4 from AP")
    msg, m4_attrs, raw_m4_attrs = recv_wsc_msg(hapd, WSC_MSG, WPS_M4)

    logger.debug("Send M5 to AP")
    attrs = build_wsc_attr(ATTR_VERSION, '\x10')
    attrs += build_attr_msg_type(WPS_M5)
    attrs += build_wsc_attr(ATTR_REGISTRAR_NONCE, r_nonce)
    data = build_wsc_attr(ATTR_E_SNONCE1, e_s1)
    attrs += build_attr_encr_settings(authkey, keywrapkey, data)
    attrs += build_attr_authenticator(authkey, raw_m4_attrs, attrs)
    raw_m5_attrs = attrs
    m5 = build_eap_wsc(2, msg['eap_identifier'], attrs)
    send_wsc_msg(hapd, addr, m5)

    logger.debug("Receive M6 from AP")
    msg, m6_attrs, raw_m6_attrs = recv_wsc_msg(hapd, WSC_MSG, WPS_M6)

    logger.debug("Send M7 to AP")
    attrs = build_wsc_attr(ATTR_VERSION, '\x10')
    attrs += build_attr_msg_type(WPS_M7)
    attrs += build_wsc_attr(ATTR_REGISTRAR_NONCE, r_nonce)
    #data = build_wsc_attr(ATTR_E_SNONCE2, e_s2)
    data = b''
    attrs += build_attr_encr_settings(authkey, keywrapkey, data)
    attrs += build_attr_authenticator(authkey, raw_m6_attrs, attrs)
    m7 = build_eap_wsc(2, msg['eap_identifier'], attrs)
    raw_m7_attrs = attrs
    send_wsc_msg(hapd, addr, m7)

    wps_wait_ap_nack(hapd, dev[0], e_nonce, r_nonce)

@remote_compatible
def test_wps_ext_m7_e_snonce2_mismatch(dev, apdev):
    """WPS proto: M7 E-SNonce2 mismatch"""
    pin = "12345670"
    addr, bssid, hapd = wps_start_ext(apdev[0], dev[0], pin=pin)
    wps_ext_eap_identity_req(dev[0], hapd, bssid)
    wps_ext_eap_identity_resp(hapd, dev[0], addr)

    logger.debug("Receive WSC/Start from AP")
    msg = get_wsc_msg(hapd)
    if msg['wsc_opcode'] != WSC_Start:
        raise Exception("Unexpected Op-Code for WSC/Start")

    mac_addr = binascii.unhexlify(dev[0].own_addr().replace(':', ''))
    uuid_e = 16*b'\x11'
    e_nonce = 16*b'\x22'
    own_private, e_pk = wsc_dh_init()

    logger.debug("Send M1 to AP")
    m1, raw_m1_attrs = build_m1(msg['eap_identifier'], uuid_e, mac_addr,
                                e_nonce, e_pk)
    send_wsc_msg(hapd, addr, m1)

    logger.debug("Receive M2 from AP")
    msg, m2_attrs, raw_m2_attrs = recv_wsc_msg(hapd, WSC_MSG, WPS_M2)
    r_nonce = m2_attrs[ATTR_REGISTRAR_NONCE]
    r_pk = m2_attrs[ATTR_PUBLIC_KEY]

    authkey, keywrapkey = wsc_dh_kdf(r_pk, own_private, mac_addr, e_nonce,
                                     r_nonce)
    e_s1, e_s2, e_hash1, e_hash2 = wsc_dev_pw_hash(authkey, pin, e_pk, r_pk)

    logger.debug("Send M3 to AP")
    attrs = build_wsc_attr(ATTR_VERSION, '\x10')
    attrs += build_attr_msg_type(WPS_M3)
    attrs += build_wsc_attr(ATTR_REGISTRAR_NONCE, r_nonce)
    attrs += build_wsc_attr(ATTR_E_HASH1, e_hash1)
    attrs += build_wsc_attr(ATTR_E_HASH2, e_hash2)
    attrs += build_attr_authenticator(authkey, raw_m2_attrs, attrs)
    raw_m3_attrs = attrs
    m3 = build_eap_wsc(2, msg['eap_identifier'], attrs)
    send_wsc_msg(hapd, addr, m3)

    logger.debug("Receive M4 from AP")
    msg, m4_attrs, raw_m4_attrs = recv_wsc_msg(hapd, WSC_MSG, WPS_M4)

    logger.debug("Send M5 to AP")
    attrs = build_wsc_attr(ATTR_VERSION, '\x10')
    attrs += build_attr_msg_type(WPS_M5)
    attrs += build_wsc_attr(ATTR_REGISTRAR_NONCE, r_nonce)
    data = build_wsc_attr(ATTR_E_SNONCE1, e_s1)
    attrs += build_attr_encr_settings(authkey, keywrapkey, data)
    attrs += build_attr_authenticator(authkey, raw_m4_attrs, attrs)
    raw_m5_attrs = attrs
    m5 = build_eap_wsc(2, msg['eap_identifier'], attrs)
    send_wsc_msg(hapd, addr, m5)

    logger.debug("Receive M6 from AP")
    msg, m6_attrs, raw_m6_attrs = recv_wsc_msg(hapd, WSC_MSG, WPS_M6)

    logger.debug("Send M7 to AP")
    attrs = build_wsc_attr(ATTR_VERSION, '\x10')
    attrs += build_attr_msg_type(WPS_M7)
    attrs += build_wsc_attr(ATTR_REGISTRAR_NONCE, r_nonce)
    data = build_wsc_attr(ATTR_E_SNONCE2, 16*'\x00')
    attrs += build_attr_encr_settings(authkey, keywrapkey, data)
    attrs += build_attr_authenticator(authkey, raw_m6_attrs, attrs)
    m7 = build_eap_wsc(2, msg['eap_identifier'], attrs)
    raw_m7_attrs = attrs
    send_wsc_msg(hapd, addr, m7)

    wps_wait_ap_nack(hapd, dev[0], e_nonce, r_nonce)

@remote_compatible
def test_wps_ext_m1_pubkey_oom(dev, apdev):
    """WPS proto: M1 PubKey OOM"""
    pin = "12345670"
    addr, bssid, hapd = wps_start_ext(apdev[0], dev[0], pin=pin)
    wps_ext_eap_identity_req(dev[0], hapd, bssid)
    wps_ext_eap_identity_resp(hapd, dev[0], addr)

    logger.debug("Receive WSC/Start from AP")
    msg = get_wsc_msg(hapd)
    if msg['wsc_opcode'] != WSC_Start:
        raise Exception("Unexpected Op-Code for WSC/Start")

    mac_addr = binascii.unhexlify(dev[0].own_addr().replace(':', ''))
    uuid_e = 16*'\x11'
    e_nonce = 16*'\x22'
    own_private, e_pk = wsc_dh_init()

    logger.debug("Send M1 to AP")
    with alloc_fail(hapd, 1, "wpabuf_alloc_copy;wps_process_pubkey"):
        m1, raw_m1_attrs = build_m1(msg['eap_identifier'], uuid_e, mac_addr,
                                    e_nonce, e_pk)
        send_wsc_msg(hapd, addr, m1)
        wps_wait_eap_failure(hapd, dev[0])

def wps_wait_eap_failure(hapd, dev):
    ev = hapd.wait_event(["CTRL-EVENT-EAP-FAILURE"], timeout=5)
    if ev is None:
        raise Exception("EAP-Failure not reported")
    dev.wait_disconnected()

@remote_compatible
def test_wps_ext_m3_m1(dev, apdev):
    """WPS proto: M3 replaced with M1"""
    pin = "12345670"
    addr, bssid, hapd = wps_start_ext(apdev[0], dev[0], pin=pin)
    wps_ext_eap_identity_req(dev[0], hapd, bssid)
    wps_ext_eap_identity_resp(hapd, dev[0], addr)

    logger.debug("Receive WSC/Start from AP")
    msg = get_wsc_msg(hapd)
    if msg['wsc_opcode'] != WSC_Start:
        raise Exception("Unexpected Op-Code for WSC/Start")

    mac_addr = binascii.unhexlify(dev[0].own_addr().replace(':', ''))
    uuid_e = 16*b'\x11'
    e_nonce = 16*b'\x22'
    own_private, e_pk = wsc_dh_init()

    logger.debug("Send M1 to AP")
    m1, raw_m1_attrs = build_m1(msg['eap_identifier'], uuid_e, mac_addr,
                                e_nonce, e_pk)
    send_wsc_msg(hapd, addr, m1)

    logger.debug("Receive M2 from AP")
    msg, m2_attrs, raw_m2_attrs = recv_wsc_msg(hapd, WSC_MSG, WPS_M2)
    r_nonce = m2_attrs[ATTR_REGISTRAR_NONCE]
    r_pk = m2_attrs[ATTR_PUBLIC_KEY]

    authkey, keywrapkey = wsc_dh_kdf(r_pk, own_private, mac_addr, e_nonce,
                                     r_nonce)
    e_s1, e_s2, e_hash1, e_hash2 = wsc_dev_pw_hash(authkey, pin, e_pk, r_pk)

    logger.debug("Send M3(M1) to AP")
    attrs = build_wsc_attr(ATTR_VERSION, '\x10')
    attrs += build_attr_msg_type(WPS_M1)
    attrs += build_wsc_attr(ATTR_REGISTRAR_NONCE, r_nonce)
    attrs += build_wsc_attr(ATTR_E_HASH1, e_hash1)
    attrs += build_wsc_attr(ATTR_E_HASH2, e_hash2)
    attrs += build_attr_authenticator(authkey, raw_m2_attrs, attrs)
    raw_m3_attrs = attrs
    m3 = build_eap_wsc(2, msg['eap_identifier'], attrs)
    send_wsc_msg(hapd, addr, m3)

    wps_wait_eap_failure(hapd, dev[0])

@remote_compatible
def test_wps_ext_m5_m3(dev, apdev):
    """WPS proto: M5 replaced with M3"""
    pin = "12345670"
    addr, bssid, hapd = wps_start_ext(apdev[0], dev[0], pin=pin)
    wps_ext_eap_identity_req(dev[0], hapd, bssid)
    wps_ext_eap_identity_resp(hapd, dev[0], addr)

    logger.debug("Receive WSC/Start from AP")
    msg = get_wsc_msg(hapd)
    if msg['wsc_opcode'] != WSC_Start:
        raise Exception("Unexpected Op-Code for WSC/Start")

    mac_addr = binascii.unhexlify(dev[0].own_addr().replace(':', ''))
    uuid_e = 16*b'\x11'
    e_nonce = 16*b'\x22'
    own_private, e_pk = wsc_dh_init()

    logger.debug("Send M1 to AP")
    m1, raw_m1_attrs = build_m1(msg['eap_identifier'], uuid_e, mac_addr,
                                e_nonce, e_pk)
    send_wsc_msg(hapd, addr, m1)

    logger.debug("Receive M2 from AP")
    msg, m2_attrs, raw_m2_attrs = recv_wsc_msg(hapd, WSC_MSG, WPS_M2)
    r_nonce = m2_attrs[ATTR_REGISTRAR_NONCE]
    r_pk = m2_attrs[ATTR_PUBLIC_KEY]

    authkey, keywrapkey = wsc_dh_kdf(r_pk, own_private, mac_addr, e_nonce,
                                     r_nonce)
    e_s1, e_s2, e_hash1, e_hash2 = wsc_dev_pw_hash(authkey, pin, e_pk, r_pk)

    logger.debug("Send M3 to AP")
    attrs = build_wsc_attr(ATTR_VERSION, '\x10')
    attrs += build_attr_msg_type(WPS_M3)
    attrs += build_wsc_attr(ATTR_REGISTRAR_NONCE, r_nonce)
    attrs += build_wsc_attr(ATTR_E_HASH1, e_hash1)
    attrs += build_wsc_attr(ATTR_E_HASH2, e_hash2)
    attrs += build_attr_authenticator(authkey, raw_m2_attrs, attrs)
    raw_m3_attrs = attrs
    m3 = build_eap_wsc(2, msg['eap_identifier'], attrs)
    send_wsc_msg(hapd, addr, m3)

    logger.debug("Receive M4 from AP")
    msg, m4_attrs, raw_m4_attrs = recv_wsc_msg(hapd, WSC_MSG, WPS_M4)

    logger.debug("Send M5(M3) to AP")
    attrs = build_wsc_attr(ATTR_VERSION, '\x10')
    attrs += build_attr_msg_type(WPS_M3)
    attrs += build_wsc_attr(ATTR_REGISTRAR_NONCE, r_nonce)
    data = build_wsc_attr(ATTR_E_SNONCE1, e_s1)
    attrs += build_attr_encr_settings(authkey, keywrapkey, data)
    attrs += build_attr_authenticator(authkey, raw_m4_attrs, attrs)
    raw_m5_attrs = attrs
    m5 = build_eap_wsc(2, msg['eap_identifier'], attrs)
    send_wsc_msg(hapd, addr, m5)

    wps_wait_ap_nack(hapd, dev[0], e_nonce, r_nonce)

@remote_compatible
def test_wps_ext_m3_m2(dev, apdev):
    """WPS proto: M3 replaced with M2"""
    pin = "12345670"
    addr, bssid, hapd = wps_start_ext(apdev[0], dev[0], pin=pin)
    wps_ext_eap_identity_req(dev[0], hapd, bssid)
    wps_ext_eap_identity_resp(hapd, dev[0], addr)

    logger.debug("Receive WSC/Start from AP")
    msg = get_wsc_msg(hapd)
    if msg['wsc_opcode'] != WSC_Start:
        raise Exception("Unexpected Op-Code for WSC/Start")

    mac_addr = binascii.unhexlify(dev[0].own_addr().replace(':', ''))
    uuid_e = 16*b'\x11'
    e_nonce = 16*b'\x22'
    own_private, e_pk = wsc_dh_init()

    logger.debug("Send M1 to AP")
    m1, raw_m1_attrs = build_m1(msg['eap_identifier'], uuid_e, mac_addr,
                                e_nonce, e_pk)
    send_wsc_msg(hapd, addr, m1)

    logger.debug("Receive M2 from AP")
    msg, m2_attrs, raw_m2_attrs = recv_wsc_msg(hapd, WSC_MSG, WPS_M2)
    r_nonce = m2_attrs[ATTR_REGISTRAR_NONCE]
    r_pk = m2_attrs[ATTR_PUBLIC_KEY]

    authkey, keywrapkey = wsc_dh_kdf(r_pk, own_private, mac_addr, e_nonce,
                                     r_nonce)
    e_s1, e_s2, e_hash1, e_hash2 = wsc_dev_pw_hash(authkey, pin, e_pk, r_pk)

    logger.debug("Send M3(M2) to AP")
    attrs = build_wsc_attr(ATTR_VERSION, '\x10')
    attrs += build_attr_msg_type(WPS_M2)
    attrs += build_wsc_attr(ATTR_REGISTRAR_NONCE, r_nonce)
    attrs += build_attr_authenticator(authkey, raw_m2_attrs, attrs)
    raw_m3_attrs = attrs
    m3 = build_eap_wsc(2, msg['eap_identifier'], attrs)
    send_wsc_msg(hapd, addr, m3)

    wps_wait_eap_failure(hapd, dev[0])

@remote_compatible
def test_wps_ext_m3_m5(dev, apdev):
    """WPS proto: M3 replaced with M5"""
    pin = "12345670"
    addr, bssid, hapd = wps_start_ext(apdev[0], dev[0], pin=pin)
    wps_ext_eap_identity_req(dev[0], hapd, bssid)
    wps_ext_eap_identity_resp(hapd, dev[0], addr)

    logger.debug("Receive WSC/Start from AP")
    msg = get_wsc_msg(hapd)
    if msg['wsc_opcode'] != WSC_Start:
        raise Exception("Unexpected Op-Code for WSC/Start")

    mac_addr = binascii.unhexlify(dev[0].own_addr().replace(':', ''))
    uuid_e = 16*b'\x11'
    e_nonce = 16*b'\x22'
    own_private, e_pk = wsc_dh_init()

    logger.debug("Send M1 to AP")
    m1, raw_m1_attrs = build_m1(msg['eap_identifier'], uuid_e, mac_addr,
                                e_nonce, e_pk)
    send_wsc_msg(hapd, addr, m1)

    logger.debug("Receive M2 from AP")
    msg, m2_attrs, raw_m2_attrs = recv_wsc_msg(hapd, WSC_MSG, WPS_M2)
    r_nonce = m2_attrs[ATTR_REGISTRAR_NONCE]
    r_pk = m2_attrs[ATTR_PUBLIC_KEY]

    authkey, keywrapkey = wsc_dh_kdf(r_pk, own_private, mac_addr, e_nonce,
                                     r_nonce)
    e_s1, e_s2, e_hash1, e_hash2 = wsc_dev_pw_hash(authkey, pin, e_pk, r_pk)

    logger.debug("Send M3(M5) to AP")
    attrs = build_wsc_attr(ATTR_VERSION, '\x10')
    attrs += build_attr_msg_type(WPS_M5)
    attrs += build_wsc_attr(ATTR_REGISTRAR_NONCE, r_nonce)
    attrs += build_wsc_attr(ATTR_E_HASH1, e_hash1)
    attrs += build_wsc_attr(ATTR_E_HASH2, e_hash2)
    attrs += build_attr_authenticator(authkey, raw_m2_attrs, attrs)
    raw_m3_attrs = attrs
    m3 = build_eap_wsc(2, msg['eap_identifier'], attrs)
    send_wsc_msg(hapd, addr, m3)

    wps_wait_ap_nack(hapd, dev[0], e_nonce, r_nonce)

@remote_compatible
def test_wps_ext_m3_m7(dev, apdev):
    """WPS proto: M3 replaced with M7"""
    pin = "12345670"
    addr, bssid, hapd = wps_start_ext(apdev[0], dev[0], pin=pin)
    wps_ext_eap_identity_req(dev[0], hapd, bssid)
    wps_ext_eap_identity_resp(hapd, dev[0], addr)

    logger.debug("Receive WSC/Start from AP")
    msg = get_wsc_msg(hapd)
    if msg['wsc_opcode'] != WSC_Start:
        raise Exception("Unexpected Op-Code for WSC/Start")

    mac_addr = binascii.unhexlify(dev[0].own_addr().replace(':', ''))
    uuid_e = 16*b'\x11'
    e_nonce = 16*b'\x22'
    own_private, e_pk = wsc_dh_init()

    logger.debug("Send M1 to AP")
    m1, raw_m1_attrs = build_m1(msg['eap_identifier'], uuid_e, mac_addr,
                                e_nonce, e_pk)
    send_wsc_msg(hapd, addr, m1)

    logger.debug("Receive M2 from AP")
    msg, m2_attrs, raw_m2_attrs = recv_wsc_msg(hapd, WSC_MSG, WPS_M2)
    r_nonce = m2_attrs[ATTR_REGISTRAR_NONCE]
    r_pk = m2_attrs[ATTR_PUBLIC_KEY]

    authkey, keywrapkey = wsc_dh_kdf(r_pk, own_private, mac_addr, e_nonce,
                                     r_nonce)
    e_s1, e_s2, e_hash1, e_hash2 = wsc_dev_pw_hash(authkey, pin, e_pk, r_pk)

    logger.debug("Send M3(M7) to AP")
    attrs = build_wsc_attr(ATTR_VERSION, '\x10')
    attrs += build_attr_msg_type(WPS_M7)
    attrs += build_wsc_attr(ATTR_REGISTRAR_NONCE, r_nonce)
    attrs += build_wsc_attr(ATTR_E_HASH1, e_hash1)
    attrs += build_wsc_attr(ATTR_E_HASH2, e_hash2)
    attrs += build_attr_authenticator(authkey, raw_m2_attrs, attrs)
    raw_m3_attrs = attrs
    m3 = build_eap_wsc(2, msg['eap_identifier'], attrs)
    send_wsc_msg(hapd, addr, m3)

    wps_wait_ap_nack(hapd, dev[0], e_nonce, r_nonce)

@remote_compatible
def test_wps_ext_m3_done(dev, apdev):
    """WPS proto: M3 replaced with WSC_Done"""
    pin = "12345670"
    addr, bssid, hapd = wps_start_ext(apdev[0], dev[0], pin=pin)
    wps_ext_eap_identity_req(dev[0], hapd, bssid)
    wps_ext_eap_identity_resp(hapd, dev[0], addr)

    logger.debug("Receive WSC/Start from AP")
    msg = get_wsc_msg(hapd)
    if msg['wsc_opcode'] != WSC_Start:
        raise Exception("Unexpected Op-Code for WSC/Start")

    mac_addr = binascii.unhexlify(dev[0].own_addr().replace(':', ''))
    uuid_e = 16*b'\x11'
    e_nonce = 16*b'\x22'
    own_private, e_pk = wsc_dh_init()

    logger.debug("Send M1 to AP")
    m1, raw_m1_attrs = build_m1(msg['eap_identifier'], uuid_e, mac_addr,
                                e_nonce, e_pk)
    send_wsc_msg(hapd, addr, m1)

    logger.debug("Receive M2 from AP")
    msg, m2_attrs, raw_m2_attrs = recv_wsc_msg(hapd, WSC_MSG, WPS_M2)
    r_nonce = m2_attrs[ATTR_REGISTRAR_NONCE]
    r_pk = m2_attrs[ATTR_PUBLIC_KEY]

    authkey, keywrapkey = wsc_dh_kdf(r_pk, own_private, mac_addr, e_nonce,
                                     r_nonce)
    e_s1, e_s2, e_hash1, e_hash2 = wsc_dev_pw_hash(authkey, pin, e_pk, r_pk)

    logger.debug("Send M3(WSC_Done) to AP")
    attrs = build_wsc_attr(ATTR_VERSION, '\x10')
    attrs += build_attr_msg_type(WPS_WSC_DONE)
    attrs += build_attr_authenticator(authkey, raw_m2_attrs, attrs)
    raw_m3_attrs = attrs
    m3 = build_eap_wsc(2, msg['eap_identifier'], attrs, opcode=WSC_Done)
    send_wsc_msg(hapd, addr, m3)

    wps_wait_eap_failure(hapd, dev[0])

@remote_compatible
def test_wps_ext_m2_nack_invalid(dev, apdev):
    """WPS proto: M2 followed by invalid NACK"""
    pin = "12345670"
    addr, bssid, hapd = wps_start_ext(apdev[0], dev[0], pin=pin)
    wps_ext_eap_identity_req(dev[0], hapd, bssid)
    wps_ext_eap_identity_resp(hapd, dev[0], addr)

    logger.debug("Receive WSC/Start from AP")
    msg = get_wsc_msg(hapd)
    if msg['wsc_opcode'] != WSC_Start:
        raise Exception("Unexpected Op-Code for WSC/Start")

    mac_addr = binascii.unhexlify(dev[0].own_addr().replace(':', ''))
    uuid_e = 16*b'\x11'
    e_nonce = 16*b'\x22'
    own_private, e_pk = wsc_dh_init()

    logger.debug("Send M1 to AP")
    m1, raw_m1_attrs = build_m1(msg['eap_identifier'], uuid_e, mac_addr,
                                e_nonce, e_pk)
    send_wsc_msg(hapd, addr, m1)

    logger.debug("Receive M2 from AP")
    msg, m2_attrs, raw_m2_attrs = recv_wsc_msg(hapd, WSC_MSG, WPS_M2)
    r_nonce = m2_attrs[ATTR_REGISTRAR_NONCE]
    r_pk = m2_attrs[ATTR_PUBLIC_KEY]

    authkey, keywrapkey = wsc_dh_kdf(r_pk, own_private, mac_addr, e_nonce,
                                     r_nonce)
    e_s1, e_s2, e_hash1, e_hash2 = wsc_dev_pw_hash(authkey, pin, e_pk, r_pk)

    logger.debug("Send WSC_NACK to AP")
    attrs = b'\x10\x00\x00'
    nack = build_eap_wsc(2, msg['eap_identifier'], attrs, opcode=WSC_NACK)
    send_wsc_msg(hapd, addr, nack)

    wps_wait_eap_failure(hapd, dev[0])

@remote_compatible
def test_wps_ext_m2_nack_no_msg_type(dev, apdev):
    """WPS proto: M2 followed by NACK without Msg Type"""
    pin = "12345670"
    addr, bssid, hapd = wps_start_ext(apdev[0], dev[0], pin=pin)
    wps_ext_eap_identity_req(dev[0], hapd, bssid)
    wps_ext_eap_identity_resp(hapd, dev[0], addr)

    logger.debug("Receive WSC/Start from AP")
    msg = get_wsc_msg(hapd)
    if msg['wsc_opcode'] != WSC_Start:
        raise Exception("Unexpected Op-Code for WSC/Start")

    mac_addr = binascii.unhexlify(dev[0].own_addr().replace(':', ''))
    uuid_e = 16*b'\x11'
    e_nonce = 16*b'\x22'
    own_private, e_pk = wsc_dh_init()

    logger.debug("Send M1 to AP")
    m1, raw_m1_attrs = build_m1(msg['eap_identifier'], uuid_e, mac_addr,
                                e_nonce, e_pk)
    send_wsc_msg(hapd, addr, m1)

    logger.debug("Receive M2 from AP")
    msg, m2_attrs, raw_m2_attrs = recv_wsc_msg(hapd, WSC_MSG, WPS_M2)
    r_nonce = m2_attrs[ATTR_REGISTRAR_NONCE]
    r_pk = m2_attrs[ATTR_PUBLIC_KEY]

    authkey, keywrapkey = wsc_dh_kdf(r_pk, own_private, mac_addr, e_nonce,
                                     r_nonce)
    e_s1, e_s2, e_hash1, e_hash2 = wsc_dev_pw_hash(authkey, pin, e_pk, r_pk)

    logger.debug("Send WSC_NACK to AP")
    nack, attrs = build_nack(msg['eap_identifier'], e_nonce, r_nonce,
                             msg_type=None, eap_code=2)
    send_wsc_msg(hapd, addr, nack)

    wps_wait_eap_failure(hapd, dev[0])

@remote_compatible
def test_wps_ext_m2_nack_invalid_msg_type(dev, apdev):
    """WPS proto: M2 followed by NACK with invalid Msg Type"""
    pin = "12345670"
    addr, bssid, hapd = wps_start_ext(apdev[0], dev[0], pin=pin)
    wps_ext_eap_identity_req(dev[0], hapd, bssid)
    wps_ext_eap_identity_resp(hapd, dev[0], addr)

    logger.debug("Receive WSC/Start from AP")
    msg = get_wsc_msg(hapd)
    if msg['wsc_opcode'] != WSC_Start:
        raise Exception("Unexpected Op-Code for WSC/Start")

    mac_addr = binascii.unhexlify(dev[0].own_addr().replace(':', ''))
    uuid_e = 16*b'\x11'
    e_nonce = 16*b'\x22'
    own_private, e_pk = wsc_dh_init()

    logger.debug("Send M1 to AP")
    m1, raw_m1_attrs = build_m1(msg['eap_identifier'], uuid_e, mac_addr,
                                e_nonce, e_pk)
    send_wsc_msg(hapd, addr, m1)

    logger.debug("Receive M2 from AP")
    msg, m2_attrs, raw_m2_attrs = recv_wsc_msg(hapd, WSC_MSG, WPS_M2)
    r_nonce = m2_attrs[ATTR_REGISTRAR_NONCE]
    r_pk = m2_attrs[ATTR_PUBLIC_KEY]

    authkey, keywrapkey = wsc_dh_kdf(r_pk, own_private, mac_addr, e_nonce,
                                     r_nonce)
    e_s1, e_s2, e_hash1, e_hash2 = wsc_dev_pw_hash(authkey, pin, e_pk, r_pk)

    logger.debug("Send WSC_NACK to AP")
    nack, attrs = build_nack(msg['eap_identifier'], e_nonce, r_nonce,
                             msg_type=WPS_WSC_ACK, eap_code=2)
    send_wsc_msg(hapd, addr, nack)

    wps_wait_eap_failure(hapd, dev[0])

@remote_compatible
def test_wps_ext_m2_nack_e_nonce_mismatch(dev, apdev):
    """WPS proto: M2 followed by NACK with e-nonce mismatch"""
    pin = "12345670"
    addr, bssid, hapd = wps_start_ext(apdev[0], dev[0], pin=pin)
    wps_ext_eap_identity_req(dev[0], hapd, bssid)
    wps_ext_eap_identity_resp(hapd, dev[0], addr)

    logger.debug("Receive WSC/Start from AP")
    msg = get_wsc_msg(hapd)
    if msg['wsc_opcode'] != WSC_Start:
        raise Exception("Unexpected Op-Code for WSC/Start")

    mac_addr = binascii.unhexlify(dev[0].own_addr().replace(':', ''))
    uuid_e = 16*b'\x11'
    e_nonce = 16*b'\x22'
    own_private, e_pk = wsc_dh_init()

    logger.debug("Send M1 to AP")
    m1, raw_m1_attrs = build_m1(msg['eap_identifier'], uuid_e, mac_addr,
                                e_nonce, e_pk)
    send_wsc_msg(hapd, addr, m1)

    logger.debug("Receive M2 from AP")
    msg, m2_attrs, raw_m2_attrs = recv_wsc_msg(hapd, WSC_MSG, WPS_M2)
    r_nonce = m2_attrs[ATTR_REGISTRAR_NONCE]
    r_pk = m2_attrs[ATTR_PUBLIC_KEY]

    authkey, keywrapkey = wsc_dh_kdf(r_pk, own_private, mac_addr, e_nonce,
                                     r_nonce)
    e_s1, e_s2, e_hash1, e_hash2 = wsc_dev_pw_hash(authkey, pin, e_pk, r_pk)

    logger.debug("Send WSC_NACK to AP")
    nack, attrs = build_nack(msg['eap_identifier'], 16*b'\x00', r_nonce,
                             eap_code=2)
    send_wsc_msg(hapd, addr, nack)

    wps_wait_eap_failure(hapd, dev[0])

@remote_compatible
def test_wps_ext_m2_nack_no_config_error(dev, apdev):
    """WPS proto: M2 followed by NACK without Config Error"""
    pin = "12345670"
    addr, bssid, hapd = wps_start_ext(apdev[0], dev[0], pin=pin)
    wps_ext_eap_identity_req(dev[0], hapd, bssid)
    wps_ext_eap_identity_resp(hapd, dev[0], addr)

    logger.debug("Receive WSC/Start from AP")
    msg = get_wsc_msg(hapd)
    if msg['wsc_opcode'] != WSC_Start:
        raise Exception("Unexpected Op-Code for WSC/Start")

    mac_addr = binascii.unhexlify(dev[0].own_addr().replace(':', ''))
    uuid_e = 16*b'\x11'
    e_nonce = 16*b'\x22'
    own_private, e_pk = wsc_dh_init()

    logger.debug("Send M1 to AP")
    m1, raw_m1_attrs = build_m1(msg['eap_identifier'], uuid_e, mac_addr,
                                e_nonce, e_pk)
    send_wsc_msg(hapd, addr, m1)

    logger.debug("Receive M2 from AP")
    msg, m2_attrs, raw_m2_attrs = recv_wsc_msg(hapd, WSC_MSG, WPS_M2)
    r_nonce = m2_attrs[ATTR_REGISTRAR_NONCE]
    r_pk = m2_attrs[ATTR_PUBLIC_KEY]

    authkey, keywrapkey = wsc_dh_kdf(r_pk, own_private, mac_addr, e_nonce,
                                     r_nonce)
    e_s1, e_s2, e_hash1, e_hash2 = wsc_dev_pw_hash(authkey, pin, e_pk, r_pk)

    logger.debug("Send WSC_NACK to AP")
    nack, attrs = build_nack(msg['eap_identifier'], e_nonce, r_nonce,
                             config_error=None, eap_code=2)
    send_wsc_msg(hapd, addr, nack)

    wps_wait_eap_failure(hapd, dev[0])

@remote_compatible
def test_wps_ext_m2_ack_invalid(dev, apdev):
    """WPS proto: M2 followed by invalid ACK"""
    pin = "12345670"
    addr, bssid, hapd = wps_start_ext(apdev[0], dev[0], pin=pin)
    wps_ext_eap_identity_req(dev[0], hapd, bssid)
    wps_ext_eap_identity_resp(hapd, dev[0], addr)

    logger.debug("Receive WSC/Start from AP")
    msg = get_wsc_msg(hapd)
    if msg['wsc_opcode'] != WSC_Start:
        raise Exception("Unexpected Op-Code for WSC/Start")

    mac_addr = binascii.unhexlify(dev[0].own_addr().replace(':', ''))
    uuid_e = 16*b'\x11'
    e_nonce = 16*b'\x22'
    own_private, e_pk = wsc_dh_init()

    logger.debug("Send M1 to AP")
    m1, raw_m1_attrs = build_m1(msg['eap_identifier'], uuid_e, mac_addr,
                                e_nonce, e_pk)
    send_wsc_msg(hapd, addr, m1)

    logger.debug("Receive M2 from AP")
    msg, m2_attrs, raw_m2_attrs = recv_wsc_msg(hapd, WSC_MSG, WPS_M2)
    r_nonce = m2_attrs[ATTR_REGISTRAR_NONCE]
    r_pk = m2_attrs[ATTR_PUBLIC_KEY]

    authkey, keywrapkey = wsc_dh_kdf(r_pk, own_private, mac_addr, e_nonce,
                                     r_nonce)
    e_s1, e_s2, e_hash1, e_hash2 = wsc_dev_pw_hash(authkey, pin, e_pk, r_pk)

    logger.debug("Send WSC_ACK to AP")
    attrs = b'\x10\x00\x00'
    ack = build_eap_wsc(2, msg['eap_identifier'], attrs, opcode=WSC_ACK)
    send_wsc_msg(hapd, addr, ack)

    wps_wait_eap_failure(hapd, dev[0])

@remote_compatible
def test_wps_ext_m2_ack(dev, apdev):
    """WPS proto: M2 followed by ACK"""
    pin = "12345670"
    addr, bssid, hapd = wps_start_ext(apdev[0], dev[0], pin=pin)
    wps_ext_eap_identity_req(dev[0], hapd, bssid)
    wps_ext_eap_identity_resp(hapd, dev[0], addr)

    logger.debug("Receive WSC/Start from AP")
    msg = get_wsc_msg(hapd)
    if msg['wsc_opcode'] != WSC_Start:
        raise Exception("Unexpected Op-Code for WSC/Start")

    mac_addr = binascii.unhexlify(dev[0].own_addr().replace(':', ''))
    uuid_e = 16*b'\x11'
    e_nonce = 16*b'\x22'
    own_private, e_pk = wsc_dh_init()

    logger.debug("Send M1 to AP")
    m1, raw_m1_attrs = build_m1(msg['eap_identifier'], uuid_e, mac_addr,
                                e_nonce, e_pk)
    send_wsc_msg(hapd, addr, m1)

    logger.debug("Receive M2 from AP")
    msg, m2_attrs, raw_m2_attrs = recv_wsc_msg(hapd, WSC_MSG, WPS_M2)
    r_nonce = m2_attrs[ATTR_REGISTRAR_NONCE]
    r_pk = m2_attrs[ATTR_PUBLIC_KEY]

    authkey, keywrapkey = wsc_dh_kdf(r_pk, own_private, mac_addr, e_nonce,
                                     r_nonce)
    e_s1, e_s2, e_hash1, e_hash2 = wsc_dev_pw_hash(authkey, pin, e_pk, r_pk)

    logger.debug("Send WSC_ACK to AP")
    ack, attrs = build_ack(msg['eap_identifier'], e_nonce, r_nonce, eap_code=2)
    send_wsc_msg(hapd, addr, ack)

    wps_wait_eap_failure(hapd, dev[0])

@remote_compatible
def test_wps_ext_m2_ack_no_msg_type(dev, apdev):
    """WPS proto: M2 followed by ACK missing Msg Type"""
    pin = "12345670"
    addr, bssid, hapd = wps_start_ext(apdev[0], dev[0], pin=pin)
    wps_ext_eap_identity_req(dev[0], hapd, bssid)
    wps_ext_eap_identity_resp(hapd, dev[0], addr)

    logger.debug("Receive WSC/Start from AP")
    msg = get_wsc_msg(hapd)
    if msg['wsc_opcode'] != WSC_Start:
        raise Exception("Unexpected Op-Code for WSC/Start")

    mac_addr = binascii.unhexlify(dev[0].own_addr().replace(':', ''))
    uuid_e = 16*b'\x11'
    e_nonce = 16*b'\x22'
    own_private, e_pk = wsc_dh_init()

    logger.debug("Send M1 to AP")
    m1, raw_m1_attrs = build_m1(msg['eap_identifier'], uuid_e, mac_addr,
                                e_nonce, e_pk)
    send_wsc_msg(hapd, addr, m1)

    logger.debug("Receive M2 from AP")
    msg, m2_attrs, raw_m2_attrs = recv_wsc_msg(hapd, WSC_MSG, WPS_M2)
    r_nonce = m2_attrs[ATTR_REGISTRAR_NONCE]
    r_pk = m2_attrs[ATTR_PUBLIC_KEY]

    authkey, keywrapkey = wsc_dh_kdf(r_pk, own_private, mac_addr, e_nonce,
                                     r_nonce)
    e_s1, e_s2, e_hash1, e_hash2 = wsc_dev_pw_hash(authkey, pin, e_pk, r_pk)

    logger.debug("Send WSC_ACK to AP")
    ack, attrs = build_ack(msg['eap_identifier'], e_nonce, r_nonce,
                           msg_type=None, eap_code=2)
    send_wsc_msg(hapd, addr, ack)

    wps_wait_eap_failure(hapd, dev[0])

@remote_compatible
def test_wps_ext_m2_ack_invalid_msg_type(dev, apdev):
    """WPS proto: M2 followed by ACK with invalid Msg Type"""
    pin = "12345670"
    addr, bssid, hapd = wps_start_ext(apdev[0], dev[0], pin=pin)
    wps_ext_eap_identity_req(dev[0], hapd, bssid)
    wps_ext_eap_identity_resp(hapd, dev[0], addr)

    logger.debug("Receive WSC/Start from AP")
    msg = get_wsc_msg(hapd)
    if msg['wsc_opcode'] != WSC_Start:
        raise Exception("Unexpected Op-Code for WSC/Start")

    mac_addr = binascii.unhexlify(dev[0].own_addr().replace(':', ''))
    uuid_e = 16*b'\x11'
    e_nonce = 16*b'\x22'
    own_private, e_pk = wsc_dh_init()

    logger.debug("Send M1 to AP")
    m1, raw_m1_attrs = build_m1(msg['eap_identifier'], uuid_e, mac_addr,
                                e_nonce, e_pk)
    send_wsc_msg(hapd, addr, m1)

    logger.debug("Receive M2 from AP")
    msg, m2_attrs, raw_m2_attrs = recv_wsc_msg(hapd, WSC_MSG, WPS_M2)
    r_nonce = m2_attrs[ATTR_REGISTRAR_NONCE]
    r_pk = m2_attrs[ATTR_PUBLIC_KEY]

    authkey, keywrapkey = wsc_dh_kdf(r_pk, own_private, mac_addr, e_nonce,
                                     r_nonce)
    e_s1, e_s2, e_hash1, e_hash2 = wsc_dev_pw_hash(authkey, pin, e_pk, r_pk)

    logger.debug("Send WSC_ACK to AP")
    ack, attrs = build_ack(msg['eap_identifier'], e_nonce, r_nonce,
                          msg_type=WPS_WSC_NACK, eap_code=2)
    send_wsc_msg(hapd, addr, ack)

    wps_wait_eap_failure(hapd, dev[0])

@remote_compatible
def test_wps_ext_m2_ack_e_nonce_mismatch(dev, apdev):
    """WPS proto: M2 followed by ACK with e-nonce mismatch"""
    pin = "12345670"
    addr, bssid, hapd = wps_start_ext(apdev[0], dev[0], pin=pin)
    wps_ext_eap_identity_req(dev[0], hapd, bssid)
    wps_ext_eap_identity_resp(hapd, dev[0], addr)

    logger.debug("Receive WSC/Start from AP")
    msg = get_wsc_msg(hapd)
    if msg['wsc_opcode'] != WSC_Start:
        raise Exception("Unexpected Op-Code for WSC/Start")

    mac_addr = binascii.unhexlify(dev[0].own_addr().replace(':', ''))
    uuid_e = 16*b'\x11'
    e_nonce = 16*b'\x22'
    own_private, e_pk = wsc_dh_init()

    logger.debug("Send M1 to AP")
    m1, raw_m1_attrs = build_m1(msg['eap_identifier'], uuid_e, mac_addr,
                                e_nonce, e_pk)
    send_wsc_msg(hapd, addr, m1)

    logger.debug("Receive M2 from AP")
    msg, m2_attrs, raw_m2_attrs = recv_wsc_msg(hapd, WSC_MSG, WPS_M2)
    r_nonce = m2_attrs[ATTR_REGISTRAR_NONCE]
    r_pk = m2_attrs[ATTR_PUBLIC_KEY]

    authkey, keywrapkey = wsc_dh_kdf(r_pk, own_private, mac_addr, e_nonce,
                                     r_nonce)
    e_s1, e_s2, e_hash1, e_hash2 = wsc_dev_pw_hash(authkey, pin, e_pk, r_pk)

    logger.debug("Send WSC_ACK to AP")
    ack, attrs = build_ack(msg['eap_identifier'], 16*b'\x00', r_nonce,
                           eap_code=2)
    send_wsc_msg(hapd, addr, ack)

    wps_wait_eap_failure(hapd, dev[0])

@remote_compatible
def test_wps_ext_m1_invalid(dev, apdev):
    """WPS proto: M1 failing parsing"""
    pin = "12345670"
    addr, bssid, hapd = wps_start_ext(apdev[0], dev[0], pin=pin)
    wps_ext_eap_identity_req(dev[0], hapd, bssid)
    wps_ext_eap_identity_resp(hapd, dev[0], addr)

    logger.debug("Receive WSC/Start from AP")
    msg = get_wsc_msg(hapd)
    if msg['wsc_opcode'] != WSC_Start:
        raise Exception("Unexpected Op-Code for WSC/Start")

    logger.debug("Send M1 to AP")
    attrs = b'\x10\x00\x00'
    m1 = build_eap_wsc(2, msg['eap_identifier'], attrs)
    send_wsc_msg(hapd, addr, m1)

    wps_wait_eap_failure(hapd, dev[0])

def test_wps_ext_m1_missing_msg_type(dev, apdev):
    """WPS proto: M1 missing Msg Type"""
    pin = "12345670"
    addr, bssid, hapd = wps_start_ext(apdev[0], dev[0], pin=pin)
    wps_ext_eap_identity_req(dev[0], hapd, bssid)
    wps_ext_eap_identity_resp(hapd, dev[0], addr)

    logger.debug("Receive WSC/Start from AP")
    msg = get_wsc_msg(hapd)
    if msg['wsc_opcode'] != WSC_Start:
        raise Exception("Unexpected Op-Code for WSC/Start")

    logger.debug("Send M1 to AP")
    attrs = build_wsc_attr(ATTR_VERSION, '\x10')
    m1 = build_eap_wsc(2, msg['eap_identifier'], attrs)
    send_wsc_msg(hapd, addr, m1)

    wps_wait_ap_nack(hapd, dev[0], 16*b'\x00', 16*b'\x00')

def wps_ext_wsc_done(dev, apdev):
    pin = "12345670"
    addr, bssid, hapd = wps_start_ext(apdev[0], dev[0], pin=pin)
    wps_ext_eap_identity_req(dev[0], hapd, bssid)
    wps_ext_eap_identity_resp(hapd, dev[0], addr)

    logger.debug("Receive WSC/Start from AP")
    msg = get_wsc_msg(hapd)
    if msg['wsc_opcode'] != WSC_Start:
        raise Exception("Unexpected Op-Code for WSC/Start")

    mac_addr = binascii.unhexlify(dev[0].own_addr().replace(':', ''))
    uuid_e = 16*b'\x11'
    e_nonce = 16*b'\x22'
    own_private, e_pk = wsc_dh_init()

    logger.debug("Send M1 to AP")
    m1, raw_m1_attrs = build_m1(msg['eap_identifier'], uuid_e, mac_addr,
                                e_nonce, e_pk)
    send_wsc_msg(hapd, addr, m1)

    logger.debug("Receive M2 from AP")
    msg, m2_attrs, raw_m2_attrs = recv_wsc_msg(hapd, WSC_MSG, WPS_M2)
    r_nonce = m2_attrs[ATTR_REGISTRAR_NONCE]
    r_pk = m2_attrs[ATTR_PUBLIC_KEY]

    authkey, keywrapkey = wsc_dh_kdf(r_pk, own_private, mac_addr, e_nonce,
                                     r_nonce)
    e_s1, e_s2, e_hash1, e_hash2 = wsc_dev_pw_hash(authkey, pin, e_pk, r_pk)

    logger.debug("Send M3 to AP")
    attrs = build_wsc_attr(ATTR_VERSION, '\x10')
    attrs += build_attr_msg_type(WPS_M3)
    attrs += build_wsc_attr(ATTR_REGISTRAR_NONCE, r_nonce)
    attrs += build_wsc_attr(ATTR_E_HASH1, e_hash1)
    attrs += build_wsc_attr(ATTR_E_HASH2, e_hash2)
    attrs += build_attr_authenticator(authkey, raw_m2_attrs, attrs)
    raw_m3_attrs = attrs
    m3 = build_eap_wsc(2, msg['eap_identifier'], attrs)
    send_wsc_msg(hapd, addr, m3)

    logger.debug("Receive M4 from AP")
    msg, m4_attrs, raw_m4_attrs = recv_wsc_msg(hapd, WSC_MSG, WPS_M4)

    logger.debug("Send M5 to AP")
    attrs = build_wsc_attr(ATTR_VERSION, '\x10')
    attrs += build_attr_msg_type(WPS_M5)
    attrs += build_wsc_attr(ATTR_REGISTRAR_NONCE, r_nonce)
    data = build_wsc_attr(ATTR_E_SNONCE1, e_s1)
    attrs += build_attr_encr_settings(authkey, keywrapkey, data)
    attrs += build_attr_authenticator(authkey, raw_m4_attrs, attrs)
    raw_m5_attrs = attrs
    m5 = build_eap_wsc(2, msg['eap_identifier'], attrs)
    send_wsc_msg(hapd, addr, m5)

    logger.debug("Receive M6 from AP")
    msg, m6_attrs, raw_m6_attrs = recv_wsc_msg(hapd, WSC_MSG, WPS_M6)

    logger.debug("Send M7 to AP")
    attrs = build_wsc_attr(ATTR_VERSION, '\x10')
    attrs += build_attr_msg_type(WPS_M7)
    attrs += build_wsc_attr(ATTR_REGISTRAR_NONCE, r_nonce)
    data = build_wsc_attr(ATTR_E_SNONCE2, e_s2)
    attrs += build_attr_encr_settings(authkey, keywrapkey, data)
    attrs += build_attr_authenticator(authkey, raw_m6_attrs, attrs)
    m7 = build_eap_wsc(2, msg['eap_identifier'], attrs)
    raw_m7_attrs = attrs
    send_wsc_msg(hapd, addr, m7)

    logger.debug("Receive M8 from AP")
    msg, m8_attrs, raw_m8_attrs = recv_wsc_msg(hapd, WSC_MSG, WPS_M8)
    return hapd, msg, e_nonce, r_nonce

@remote_compatible
def test_wps_ext_wsc_done_invalid(dev, apdev):
    """WPS proto: invalid WSC_Done"""
    hapd, msg, e_nonce, r_nonce = wps_ext_wsc_done(dev, apdev)

    logger.debug("Send WSC_Done to AP")
    attrs = b'\x10\x00\x00'
    wsc_done = build_eap_wsc(2, msg['eap_identifier'], attrs, opcode=WSC_Done)
    send_wsc_msg(hapd, dev[0].own_addr(), wsc_done)

    wps_wait_eap_failure(hapd, dev[0])

@remote_compatible
def test_wps_ext_wsc_done_no_msg_type(dev, apdev):
    """WPS proto: invalid WSC_Done"""
    hapd, msg, e_nonce, r_nonce = wps_ext_wsc_done(dev, apdev)

    logger.debug("Send WSC_Done to AP")
    attrs = build_wsc_attr(ATTR_VERSION, '\x10')
    #attrs += build_attr_msg_type(WPS_WSC_DONE)
    attrs += build_wsc_attr(ATTR_ENROLLEE_NONCE, e_nonce)
    attrs += build_wsc_attr(ATTR_REGISTRAR_NONCE, r_nonce)
    wsc_done = build_eap_wsc(2, msg['eap_identifier'], attrs, opcode=WSC_Done)
    send_wsc_msg(hapd, dev[0].own_addr(), wsc_done)

    wps_wait_eap_failure(hapd, dev[0])

@remote_compatible
def test_wps_ext_wsc_done_wrong_msg_type(dev, apdev):
    """WPS proto: WSC_Done with wrong Msg Type"""
    hapd, msg, e_nonce, r_nonce = wps_ext_wsc_done(dev, apdev)

    logger.debug("Send WSC_Done to AP")
    attrs = build_wsc_attr(ATTR_VERSION, '\x10')
    attrs += build_attr_msg_type(WPS_WSC_ACK)
    attrs += build_wsc_attr(ATTR_ENROLLEE_NONCE, e_nonce)
    attrs += build_wsc_attr(ATTR_REGISTRAR_NONCE, r_nonce)
    wsc_done = build_eap_wsc(2, msg['eap_identifier'], attrs, opcode=WSC_Done)
    send_wsc_msg(hapd, dev[0].own_addr(), wsc_done)

    wps_wait_eap_failure(hapd, dev[0])

@remote_compatible
def test_wps_ext_wsc_done_no_e_nonce(dev, apdev):
    """WPS proto: WSC_Done without e_nonce"""
    hapd, msg, e_nonce, r_nonce = wps_ext_wsc_done(dev, apdev)

    logger.debug("Send WSC_Done to AP")
    attrs = build_wsc_attr(ATTR_VERSION, '\x10')
    attrs += build_attr_msg_type(WPS_WSC_DONE)
    #attrs += build_wsc_attr(ATTR_ENROLLEE_NONCE, e_nonce)
    attrs += build_wsc_attr(ATTR_REGISTRAR_NONCE, r_nonce)
    wsc_done = build_eap_wsc(2, msg['eap_identifier'], attrs, opcode=WSC_Done)
    send_wsc_msg(hapd, dev[0].own_addr(), wsc_done)

    wps_wait_eap_failure(hapd, dev[0])

def test_wps_ext_wsc_done_no_r_nonce(dev, apdev):
    """WPS proto: WSC_Done without r_nonce"""
    hapd, msg, e_nonce, r_nonce = wps_ext_wsc_done(dev, apdev)

    logger.debug("Send WSC_Done to AP")
    attrs = build_wsc_attr(ATTR_VERSION, '\x10')
    attrs += build_attr_msg_type(WPS_WSC_DONE)
    attrs += build_wsc_attr(ATTR_ENROLLEE_NONCE, e_nonce)
    #attrs += build_wsc_attr(ATTR_REGISTRAR_NONCE, r_nonce)
    wsc_done = build_eap_wsc(2, msg['eap_identifier'], attrs, opcode=WSC_Done)
    send_wsc_msg(hapd, dev[0].own_addr(), wsc_done)

    wps_wait_eap_failure(hapd, dev[0])

@remote_compatible
def test_wps_ext_m7_no_encr_settings(dev, apdev):
    """WPS proto: M7 without Encr Settings"""
    pin = "12345670"
    addr, bssid, hapd = wps_start_ext(apdev[0], dev[0], pin=pin)
    wps_ext_eap_identity_req(dev[0], hapd, bssid)
    wps_ext_eap_identity_resp(hapd, dev[0], addr)

    logger.debug("Receive WSC/Start from AP")
    msg = get_wsc_msg(hapd)
    if msg['wsc_opcode'] != WSC_Start:
        raise Exception("Unexpected Op-Code for WSC/Start")

    mac_addr = binascii.unhexlify(dev[0].own_addr().replace(':', ''))
    uuid_e = 16*b'\x11'
    e_nonce = 16*b'\x22'
    own_private, e_pk = wsc_dh_init()

    logger.debug("Send M1 to AP")
    m1, raw_m1_attrs = build_m1(msg['eap_identifier'], uuid_e, mac_addr,
                                e_nonce, e_pk)
    send_wsc_msg(hapd, addr, m1)

    logger.debug("Receive M2 from AP")
    msg, m2_attrs, raw_m2_attrs = recv_wsc_msg(hapd, WSC_MSG, WPS_M2)
    r_nonce = m2_attrs[ATTR_REGISTRAR_NONCE]
    r_pk = m2_attrs[ATTR_PUBLIC_KEY]

    authkey, keywrapkey = wsc_dh_kdf(r_pk, own_private, mac_addr, e_nonce,
                                     r_nonce)
    e_s1, e_s2, e_hash1, e_hash2 = wsc_dev_pw_hash(authkey, pin, e_pk, r_pk)

    logger.debug("Send M3 to AP")
    attrs = build_wsc_attr(ATTR_VERSION, '\x10')
    attrs += build_attr_msg_type(WPS_M3)
    attrs += build_wsc_attr(ATTR_REGISTRAR_NONCE, r_nonce)
    attrs += build_wsc_attr(ATTR_E_HASH1, e_hash1)
    attrs += build_wsc_attr(ATTR_E_HASH2, e_hash2)
    attrs += build_attr_authenticator(authkey, raw_m2_attrs, attrs)
    raw_m3_attrs = attrs
    m3 = build_eap_wsc(2, msg['eap_identifier'], attrs)
    send_wsc_msg(hapd, addr, m3)

    logger.debug("Receive M4 from AP")
    msg, m4_attrs, raw_m4_attrs = recv_wsc_msg(hapd, WSC_MSG, WPS_M4)

    logger.debug("Send M5 to AP")
    attrs = build_wsc_attr(ATTR_VERSION, '\x10')
    attrs += build_attr_msg_type(WPS_M5)
    attrs += build_wsc_attr(ATTR_REGISTRAR_NONCE, r_nonce)
    data = build_wsc_attr(ATTR_E_SNONCE1, e_s1)
    attrs += build_attr_encr_settings(authkey, keywrapkey, data)
    attrs += build_attr_authenticator(authkey, raw_m4_attrs, attrs)
    raw_m5_attrs = attrs
    m5 = build_eap_wsc(2, msg['eap_identifier'], attrs)
    send_wsc_msg(hapd, addr, m5)

    logger.debug("Receive M6 from AP")
    msg, m6_attrs, raw_m6_attrs = recv_wsc_msg(hapd, WSC_MSG, WPS_M6)

    logger.debug("Send M7 to AP")
    attrs = build_wsc_attr(ATTR_VERSION, '\x10')
    attrs += build_attr_msg_type(WPS_M7)
    attrs += build_wsc_attr(ATTR_REGISTRAR_NONCE, r_nonce)
    #data = build_wsc_attr(ATTR_E_SNONCE2, e_s2)
    #attrs += build_attr_encr_settings(authkey, keywrapkey, data)
    attrs += build_attr_authenticator(authkey, raw_m6_attrs, attrs)
    m7 = build_eap_wsc(2, msg['eap_identifier'], attrs)
    raw_m7_attrs = attrs
    send_wsc_msg(hapd, addr, m7)

    wps_wait_ap_nack(hapd, dev[0], e_nonce, r_nonce)

@remote_compatible
def test_wps_ext_m1_workaround(dev, apdev):
    """WPS proto: M1 Manufacturer/Model workaround"""
    pin = "12345670"
    addr, bssid, hapd = wps_start_ext(apdev[0], dev[0], pin=pin)
    wps_ext_eap_identity_req(dev[0], hapd, bssid)
    wps_ext_eap_identity_resp(hapd, dev[0], addr)

    logger.debug("Receive WSC/Start from AP")
    msg = get_wsc_msg(hapd)
    if msg['wsc_opcode'] != WSC_Start:
        raise Exception("Unexpected Op-Code for WSC/Start")

    mac_addr = binascii.unhexlify(dev[0].own_addr().replace(':', ''))
    uuid_e = 16*b'\x11'
    e_nonce = 16*b'\x22'
    own_private, e_pk = wsc_dh_init()

    logger.debug("Send M1 to AP")
    m1, raw_m1_attrs = build_m1(msg['eap_identifier'], uuid_e, mac_addr,
                                e_nonce, e_pk, manufacturer='Apple TEST',
                                model_name='AirPort', config_methods=b'\xff\xff')
    send_wsc_msg(hapd, addr, m1)

    logger.debug("Receive M2 from AP")
    msg, m2_attrs, raw_m2_attrs = recv_wsc_msg(hapd, WSC_MSG, WPS_M2)

@remote_compatible
def test_ap_wps_disable_enable(dev, apdev):
    """WPS and DISABLE/ENABLE AP"""
    hapd = wps_start_ap(apdev[0])
    hapd.disable()
    hapd.enable()
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")

def test_ap_wps_upnp_web_oom(dev, apdev, params):
    """hostapd WPS UPnP web OOM"""
    ap_uuid = "27ea801a-9e5c-4e73-bd82-f89cbcd10d7e"
    hapd = add_ssdp_ap(apdev[0], ap_uuid)

    location = ssdp_get_location(ap_uuid)
    url = urlparse(location)
    urls = upnp_get_urls(location)
    eventurl = urlparse(urls['event_sub_url'])
    ctrlurl = urlparse(urls['control_url'])

    conn = HTTPConnection(url.netloc)
    with alloc_fail(hapd, 1, "web_connection_parse_get"):
        conn.request("GET", "/wps_device.xml")
        try:
            resp = conn.getresponse()
        except:
            pass

    conn = HTTPConnection(url.netloc)
    conn.request("GET", "/unknown")
    resp = conn.getresponse()
    if resp.status != 404:
        raise Exception("Unexpected HTTP result for unknown URL: %d" + resp.status)

    with alloc_fail(hapd, 1, "web_connection_parse_get"):
        conn.request("GET", "/unknown")
        try:
            resp = conn.getresponse()
            print(resp.status)
        except:
            pass

    conn = HTTPConnection(url.netloc)
    conn.request("GET", "/wps_device.xml")
    resp = conn.getresponse()
    if resp.status != 200:
        raise Exception("GET /wps_device.xml failed")

    conn = HTTPConnection(url.netloc)
    resp = upnp_soap_action(conn, ctrlurl.path, "GetDeviceInfo")
    if resp.status != 200:
        raise Exception("GetDeviceInfo failed")

    with alloc_fail(hapd, 1, "web_process_get_device_info"):
        conn = HTTPConnection(url.netloc)
        resp = upnp_soap_action(conn, ctrlurl.path, "GetDeviceInfo")
        if resp.status != 500:
            raise Exception("Internal error not reported from GetDeviceInfo OOM")

    with alloc_fail(hapd, 1, "wps_build_m1;web_process_get_device_info"):
        conn = HTTPConnection(url.netloc)
        resp = upnp_soap_action(conn, ctrlurl.path, "GetDeviceInfo")
        if resp.status != 500:
            raise Exception("Internal error not reported from GetDeviceInfo OOM")

    with alloc_fail(hapd, 1, "wpabuf_alloc;web_connection_send_reply"):
        conn = HTTPConnection(url.netloc)
        try:
            resp = upnp_soap_action(conn, ctrlurl.path, "GetDeviceInfo")
        except:
            pass

    conn = HTTPConnection(url.netloc)
    resp = upnp_soap_action(conn, ctrlurl.path, "GetDeviceInfo")
    if resp.status != 200:
        raise Exception("GetDeviceInfo failed")

    # No NewWLANEventType in PutWLANResponse NewMessage
    conn = HTTPConnection(url.netloc)
    resp = upnp_soap_action(conn, ctrlurl.path, "PutWLANResponse", newmsg="foo")
    if resp.status != 600:
        raise Exception("Unexpected HTTP response: %d" % resp.status)

    # No NewWLANEventMAC in PutWLANResponse NewMessage
    conn = HTTPConnection(url.netloc)
    resp = upnp_soap_action(conn, ctrlurl.path, "PutWLANResponse",
                            newmsg="foo", neweventtype="1")
    if resp.status != 600:
        raise Exception("Unexpected HTTP response: %d" % resp.status)

    # Invalid NewWLANEventMAC in PutWLANResponse NewMessage
    conn = HTTPConnection(url.netloc)
    resp = upnp_soap_action(conn, ctrlurl.path, "PutWLANResponse",
                            newmsg="foo", neweventtype="1",
                            neweventmac="foo")
    if resp.status != 600:
        raise Exception("Unexpected HTTP response: %d" % resp.status)

    # Workaround for NewWLANEventMAC in PutWLANResponse NewMessage
    # Ignored unexpected PutWLANResponse WLANEventType 1
    conn = HTTPConnection(url.netloc)
    resp = upnp_soap_action(conn, ctrlurl.path, "PutWLANResponse",
                            newmsg="foo", neweventtype="1",
                            neweventmac="00.11.22.33.44.55")
    if resp.status != 500:
        raise Exception("Unexpected HTTP response: %d" % resp.status)

    # PutWLANResponse NewMessage with invalid EAP message
    conn = HTTPConnection(url.netloc)
    resp = upnp_soap_action(conn, ctrlurl.path, "PutWLANResponse",
                            newmsg="foo", neweventtype="2",
                            neweventmac="00:11:22:33:44:55")
    if resp.status != 200:
        raise Exception("Unexpected HTTP response: %d" % resp.status)

    with alloc_fail(hapd, 1, "web_connection_parse_subscribe"):
        conn = HTTPConnection(url.netloc)
        headers = {"callback": '<http://127.0.0.1:12345/event>',
                   "NT": "upnp:event",
                   "timeout": "Second-1234"}
        conn.request("SUBSCRIBE", eventurl.path, "\r\n\r\n", headers)
        try:
            resp = conn.getresponse()
        except:
            pass

    with alloc_fail(hapd, 1, "dup_binstr;web_connection_parse_subscribe"):
        conn = HTTPConnection(url.netloc)
        headers = {"callback": '<http://127.0.0.1:12345/event>',
                   "NT": "upnp:event",
                   "timeout": "Second-1234"}
        conn.request("SUBSCRIBE", eventurl.path, "\r\n\r\n", headers)
        resp = conn.getresponse()
        if resp.status != 500:
            raise Exception("Unexpected HTTP response: %d" % resp.status)

    with alloc_fail(hapd, 1, "wpabuf_alloc;web_connection_parse_unsubscribe"):
        conn = HTTPConnection(url.netloc)
        headers = {"callback": '<http://127.0.0.1:12345/event>',
                   "NT": "upnp:event",
                   "timeout": "Second-1234"}
        conn.request("UNSUBSCRIBE", eventurl.path, "\r\n\r\n", headers)
        try:
            resp = conn.getresponse()
        except:
            pass

    with alloc_fail(hapd, 1, "web_connection_unimplemented"):
        conn = HTTPConnection(url.netloc)
        conn.request("HEAD", "/wps_device.xml")
        try:
            resp = conn.getresponse()
        except:
            pass

def test_ap_wps_frag_ack_oom(dev, apdev):
    """WPS and fragment ack OOM"""
    dev[0].request("SET wps_fragment_size 50")
    hapd = wps_start_ap(apdev[0])
    with alloc_fail(hapd, 1, "eap_wsc_build_frag_ack"):
        wps_run_pbc_fail_ap(apdev[0], dev[0], hapd)

def wait_scan_stopped(dev):
    dev.request("ABORT_SCAN")
    for i in range(50):
        res = dev.get_driver_status_field("scan_state")
        if "SCAN_STARTED" not in res and "SCAN_REQUESTED" not in res:
            break
        logger.debug("Waiting for scan to complete")
        time.sleep(0.1)

@remote_compatible
def test_ap_wps_eap_wsc_errors(dev, apdev):
    """WPS and EAP-WSC error cases"""
    ssid = "test-wps-conf-pin"
    appin = "12345670"
    params = {"ssid": ssid, "eap_server": "1", "wps_state": "2",
              "wpa_passphrase": "12345678", "wpa": "2",
              "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP",
              "fragment_size": "300", "ap_pin": appin}
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = apdev[0]['bssid']

    pin = dev[0].wps_read_pin()
    hapd.request("WPS_PIN any " + pin)
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[0].dump_monitor()

    dev[0].wps_reg(bssid, appin + " new_ssid=a", "new ssid", "WPA2PSK", "CCMP",
                   "new passphrase", no_wait=True)
    ev = dev[0].wait_event(["WPS-FAIL"], timeout=10)
    if ev is None:
        raise Exception("WPS-FAIL not reported")
    dev[0].request("WPS_CANCEL")
    dev[0].wait_disconnected()
    wait_scan_stopped(dev[0])
    dev[0].dump_monitor()

    dev[0].wps_reg(bssid, appin, "new ssid", "FOO", "CCMP",
                   "new passphrase", no_wait=True)
    ev = dev[0].wait_event(["WPS-FAIL"], timeout=10)
    if ev is None:
        raise Exception("WPS-FAIL not reported")
    dev[0].request("WPS_CANCEL")
    dev[0].wait_disconnected()
    wait_scan_stopped(dev[0])
    dev[0].dump_monitor()

    dev[0].wps_reg(bssid, appin, "new ssid", "WPA2PSK", "FOO",
                   "new passphrase", no_wait=True)
    ev = dev[0].wait_event(["WPS-FAIL"], timeout=10)
    if ev is None:
        raise Exception("WPS-FAIL not reported")
    dev[0].request("WPS_CANCEL")
    dev[0].wait_disconnected()
    wait_scan_stopped(dev[0])
    dev[0].dump_monitor()

    dev[0].wps_reg(bssid, appin + "new_key=a", "new ssid", "WPA2PSK", "CCMP",
                   "new passphrase", no_wait=True)
    ev = dev[0].wait_event(["WPS-FAIL"], timeout=10)
    if ev is None:
        raise Exception("WPS-FAIL not reported")
    dev[0].request("WPS_CANCEL")
    dev[0].wait_disconnected()
    wait_scan_stopped(dev[0])
    dev[0].dump_monitor()

    tests = ["eap_wsc_init",
             "eap_msg_alloc;eap_wsc_build_msg",
             "wpabuf_alloc;eap_wsc_process_fragment"]
    for func in tests:
        with alloc_fail(dev[0], 1, func):
            dev[0].request("WPS_PIN %s %s" % (bssid, pin))
            wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
            dev[0].request("WPS_CANCEL")
            dev[0].wait_disconnected()
            wait_scan_stopped(dev[0])
            dev[0].dump_monitor()

    tests = [(1, "wps_decrypt_encr_settings"),
             (2, "hmac_sha256;wps_derive_psk")]
    for count, func in tests:
        hapd.request("WPS_PIN any " + pin)
        with fail_test(dev[0], count, func):
            dev[0].request("WPS_PIN %s %s" % (bssid, pin))
            wait_fail_trigger(dev[0], "GET_FAIL")
            dev[0].request("WPS_CANCEL")
            dev[0].wait_disconnected()
            wait_scan_stopped(dev[0])
            dev[0].dump_monitor()

    with alloc_fail(dev[0], 1, "eap_msg_alloc;eap_sm_build_expanded_nak"):
        dev[0].wps_reg(bssid, appin + " new_ssid=a", "new ssid", "WPA2PSK",
                       "CCMP", "new passphrase", no_wait=True)
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
        dev[0].request("WPS_CANCEL")
        dev[0].wait_disconnected()
        wait_scan_stopped(dev[0])
        dev[0].dump_monitor()

def test_ap_wps_eap_wsc(dev, apdev):
    """WPS and EAP-WSC in network profile"""
    params = int_eap_server_params()
    params["wps_state"] = "2"
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = apdev[0]['bssid']

    logger.info("Unexpected identity")
    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", scan_freq="2412",
                   eap="WSC", identity="WFA-SimpleConfig-Enrollee-unexpected",
                   wait_connect=False)
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"], timeout=5)
    if ev is None:
        raise Exception("No EAP-Failure seen")
    dev[0].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected()

    logger.info("No phase1 parameter")
    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", scan_freq="2412",
                   eap="WSC", identity="WFA-SimpleConfig-Enrollee-1-0",
                   wait_connect=False)
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-PROPOSED-METHOD"], timeout=5)
    if ev is None:
        raise Exception("Timeout on EAP method start")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"], timeout=5)
    if ev is None:
        raise Exception("No EAP-Failure seen")
    dev[0].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected()

    logger.info("No PIN/PBC in phase1")
    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", scan_freq="2412",
                   eap="WSC", identity="WFA-SimpleConfig-Enrollee-1-0",
                   phase1="foo", wait_connect=False)
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-PROPOSED-METHOD"], timeout=5)
    if ev is None:
        raise Exception("Timeout on EAP method start")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"], timeout=5)
    if ev is None:
        raise Exception("No EAP-Failure seen")
    dev[0].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected()

    logger.info("Invalid pkhash in phase1")
    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", scan_freq="2412",
                   eap="WSC", identity="WFA-SimpleConfig-Enrollee-1-0",
                   phase1="foo pkhash=q pbc=1", wait_connect=False)
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-PROPOSED-METHOD"], timeout=5)
    if ev is None:
        raise Exception("Timeout on EAP method start")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"], timeout=5)
    if ev is None:
        raise Exception("No EAP-Failure seen")
    dev[0].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected()

    logger.info("Zero fragment_size")
    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", scan_freq="2412",
                   eap="WSC", identity="WFA-SimpleConfig-Enrollee-1-0",
                   fragment_size="0", phase1="pin=12345670", wait_connect=False)
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-PROPOSED-METHOD"], timeout=5)
    if ev is None:
        raise Exception("Timeout on EAP method start")
    ev = dev[0].wait_event(["WPS-M2D"], timeout=5)
    if ev is None:
        raise Exception("No M2D seen")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"], timeout=5)
    if ev is None:
        raise Exception("No EAP-Failure seen")
    dev[0].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected()

    logger.info("Missing new_auth")
    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", scan_freq="2412",
                   eap="WSC", identity="WFA-SimpleConfig-Enrollee-1-0",
                   phase1="pin=12345670 new_ssid=aa", wait_connect=False)
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-PROPOSED-METHOD"], timeout=5)
    if ev is None:
        raise Exception("Timeout on EAP method start")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"], timeout=5)
    if ev is None:
        raise Exception("No EAP-Failure seen")
    dev[0].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected()

    logger.info("Missing new_encr")
    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", scan_freq="2412",
                   eap="WSC", identity="WFA-SimpleConfig-Enrollee-1-0",
                   phase1="pin=12345670 new_auth=WPA2PSK new_ssid=aa", wait_connect=False)
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-PROPOSED-METHOD"], timeout=5)
    if ev is None:
        raise Exception("Timeout on EAP method start")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"], timeout=5)
    if ev is None:
        raise Exception("No EAP-Failure seen")
    dev[0].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected()

    logger.info("Missing new_key")
    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", scan_freq="2412",
                   eap="WSC", identity="WFA-SimpleConfig-Enrollee-1-0",
                   phase1="pin=12345670 new_auth=WPA2PSK new_ssid=aa new_encr=CCMP",
                   wait_connect=False)
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-PROPOSED-METHOD"], timeout=5)
    if ev is None:
        raise Exception("Timeout on EAP method start")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"], timeout=5)
    if ev is None:
        raise Exception("No EAP-Failure seen")
    dev[0].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected()

def test_ap_wps_and_bss_limit(dev, apdev):
    """WPS and wpa_supplicant BSS entry limit"""
    try:
        _test_ap_wps_and_bss_limit(dev, apdev)
    finally:
        dev[0].request("SET bss_max_count 200")
        pass

def _test_ap_wps_and_bss_limit(dev, apdev):
    params = {"ssid": "test-wps", "eap_server": "1", "wps_state": "2",
              "wpa_passphrase": "12345678", "wpa": "2",
              "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP"}
    hapd = hostapd.add_ap(apdev[0], params)

    params = {"ssid": "test-wps-2", "eap_server": "1", "wps_state": "2",
              "wpa_passphrase": "1234567890", "wpa": "2",
              "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP"}
    hapd2 = hostapd.add_ap(apdev[1], params)

    id = dev[1].add_network()
    dev[1].set_network(id, "mode", "2")
    dev[1].set_network_quoted(id, "ssid", "wpas-ap-no-wps")
    dev[1].set_network_quoted(id, "psk", "12345678")
    dev[1].set_network(id, "frequency", "2462")
    dev[1].set_network(id, "scan_freq", "2462")
    dev[1].set_network(id, "wps_disabled", "1")
    dev[1].select_network(id)

    id = dev[2].add_network()
    dev[2].set_network(id, "mode", "2")
    dev[2].set_network_quoted(id, "ssid", "wpas-ap")
    dev[2].set_network_quoted(id, "psk", "12345678")
    dev[2].set_network(id, "frequency", "2437")
    dev[2].set_network(id, "scan_freq", "2437")
    dev[2].select_network(id)

    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    wpas.interface_add("wlan5")
    id = wpas.add_network()
    wpas.set_network(id, "mode", "2")
    wpas.set_network_quoted(id, "ssid", "wpas-ap")
    wpas.set_network_quoted(id, "psk", "12345678")
    wpas.set_network(id, "frequency", "2437")
    wpas.set_network(id, "scan_freq", "2437")
    wpas.select_network(id)

    dev[1].wait_connected()
    dev[2].wait_connected()
    wpas.wait_connected()
    wpas.request("WPS_PIN any 12345670")

    hapd.request("WPS_PBC")
    hapd2.request("WPS_PBC")

    dev[0].request("SET bss_max_count 1")

    id = dev[0].add_network()
    dev[0].set_network_quoted(id, "ssid", "testing")

    id = dev[0].add_network()
    dev[0].set_network_quoted(id, "ssid", "testing")
    dev[0].set_network(id, "key_mgmt", "WPS")

    dev[0].request("WPS_PBC")
    ev = dev[0].wait_event(["CTRL-EVENT-SCAN-RESULTS"], timeout=10)
    dev[0].request("WPS_CANCEL")

    id = dev[0].add_network()
    dev[0].set_network_quoted(id, "ssid", "testing")
    dev[0].set_network(id, "key_mgmt", "WPS")

    dev[0].scan(freq="2412")

def test_ap_wps_pbc_2ap(dev, apdev):
    """WPS PBC with two APs advertising same SSID"""
    params = {"ssid": "wps", "eap_server": "1", "wps_state": "2",
              "wpa_passphrase": "12345678", "wpa": "2",
              "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP",
              "wps_independent": "1"}
    hapd = hostapd.add_ap(apdev[0], params)
    params = {"ssid": "wps", "eap_server": "1", "wps_state": "2",
              "wpa_passphrase": "123456789", "wpa": "2",
              "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP",
              "wps_independent": "1"}
    hapd2 = hostapd.add_ap(apdev[1], params)
    hapd.request("WPS_PBC")

    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    wpas.interface_add("wlan5", drv_params="force_connect_cmd=1")
    wpas.dump_monitor()
    wpas.flush_scan_cache()

    wpas.scan_for_bss(apdev[0]['bssid'], freq="2412", force_scan=True)
    wpas.scan_for_bss(apdev[1]['bssid'], freq="2412")
    wpas.request("WPS_PBC")
    wpas.wait_connected()
    wpas.request("DISCONNECT")
    hapd.request("DISABLE")
    hapd2.request("DISABLE")
    wpas.flush_scan_cache()

def test_ap_wps_er_enrollee_to_conf_ap(dev, apdev):
    """WPS ER enrolling a new device to a configured AP"""
    try:
        _test_ap_wps_er_enrollee_to_conf_ap(dev, apdev)
    finally:
        dev[0].request("WPS_ER_STOP")

def _test_ap_wps_er_enrollee_to_conf_ap(dev, apdev):
    ssid = "wps-er-enrollee-to-conf-ap"
    ap_pin = "12345670"
    ap_uuid = "27ea801a-9e5c-4e73-bd82-f89cbcd10d7e"
    params = {"ssid": ssid, "eap_server": "1", "wps_state": "2",
              "wpa_passphrase": "12345678", "wpa": "2",
              "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP",
              "device_name": "Wireless AP", "manufacturer": "Company",
              "model_name": "WAP", "model_number": "123",
              "serial_number": "12345", "device_type": "6-0050F204-1",
              "os_version": "01020300",
              "config_methods": "label push_button",
              "ap_pin": ap_pin, "uuid": ap_uuid, "upnp_iface": "lo"}
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = hapd.own_addr()

    id = dev[0].connect(ssid, psk="12345678", scan_freq="2412")
    dev[0].dump_monitor()

    dev[0].request("WPS_ER_START ifname=lo")
    ev = dev[0].wait_event(["WPS-ER-AP-ADD"], timeout=15)
    if ev is None:
        raise Exception("AP discovery timed out")
    if ap_uuid not in ev:
        raise Exception("Expected AP UUID not found")

    pin = dev[2].wps_read_pin()
    addr2 = dev[2].own_addr()
    dev[0].dump_monitor()
    dev[2].scan_for_bss(bssid, freq=2412)
    dev[2].dump_monitor()
    dev[2].request("WPS_PIN %s %s" % (bssid, pin))

    for i in range(3):
        ev = dev[0].wait_event(["WPS-ER-ENROLLEE-ADD"], timeout=10)
        if ev is None:
            raise Exception("Enrollee not seen")
        if addr2 in ev:
            break
    if addr2 not in ev:
        raise Exception("Unexpected Enrollee MAC address")
    dev[0].dump_monitor()

    dev[0].request("WPS_ER_SET_CONFIG " + ap_uuid + " " + str(id))
    dev[0].request("WPS_ER_PIN " + addr2 + " " + pin + " " + addr2)
    dev[2].wait_connected(timeout=30)
    ev = dev[0].wait_event(["WPS-SUCCESS"], timeout=15)
    if ev is None:
        raise Exception("WPS ER did not report success")

def test_ap_wps_er_enrollee_to_conf_ap2(dev, apdev):
    """WPS ER enrolling a new device to a configured AP (2)"""
    try:
        _test_ap_wps_er_enrollee_to_conf_ap2(dev, apdev)
    finally:
        dev[0].request("WPS_ER_STOP")

def _test_ap_wps_er_enrollee_to_conf_ap2(dev, apdev):
    ssid = "wps-er-enrollee-to-conf-ap"
    ap_pin = "12345670"
    ap_uuid = "27ea801a-9e5c-4e73-bd82-f89cbcd10d7e"
    params = {"ssid": ssid, "eap_server": "1", "wps_state": "2",
              "wpa_passphrase": "12345678", "wpa": "2",
              "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP",
              "device_name": "Wireless AP", "manufacturer": "Company",
              "model_name": "WAP", "model_number": "123",
              "serial_number": "12345", "device_type": "6-0050F204-1",
              "os_version": "01020300",
              "config_methods": "label push_button",
              "ap_pin": ap_pin, "uuid": ap_uuid, "upnp_iface": "lo"}
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = hapd.own_addr()

    id = dev[0].connect(ssid, psk="12345678", scan_freq="2412")
    dev[0].dump_monitor()

    dev[0].request("WPS_ER_START ifname=lo")
    ev = dev[0].wait_event(["WPS-ER-AP-ADD"], timeout=15)
    if ev is None:
        raise Exception("AP discovery timed out")
    if ap_uuid not in ev:
        raise Exception("Expected AP UUID not found")

    dev[0].request("WPS_ER_LEARN " + ap_uuid + " " + ap_pin)
    ev = dev[0].wait_event(["WPS-ER-AP-SETTINGS"], timeout=15)
    if ev is None:
        raise Exception("AP learn timed out")
    if ap_uuid not in ev:
        raise Exception("Expected AP UUID not in settings")
    ev = dev[0].wait_event(["WPS-FAIL"], timeout=15)
    if ev is None:
        raise Exception("WPS-FAIL after AP learn timed out")
    time.sleep(0.1)

    pin = dev[1].wps_read_pin()
    addr1 = dev[1].own_addr()
    dev[0].dump_monitor()
    dev[0].request("WPS_ER_PIN any " + pin)
    time.sleep(0.1)
    dev[1].scan_for_bss(bssid, freq=2412)
    dev[1].request("WPS_PIN any %s" % pin)
    ev = dev[1].wait_event(["WPS-SUCCESS"], timeout=30)
    if ev is None:
        raise Exception("Enrollee did not report success")
    dev[1].wait_connected(timeout=15)
    ev = dev[0].wait_event(["WPS-SUCCESS"], timeout=15)
    if ev is None:
        raise Exception("WPS ER did not report success")

def test_ap_wps_ignore_broadcast_ssid(dev, apdev):
    """WPS AP trying to ignore broadcast SSID"""
    ssid = "test-wps"
    hapd = hostapd.add_ap(apdev[0],
                          {"ssid": ssid, "eap_server": "1", "wps_state": "1",
                           "ignore_broadcast_ssid": "1"})
    if "FAIL" not in hapd.request("WPS_PBC"):
        raise Exception("WPS unexpectedly enabled")

def test_ap_wps_wep(dev, apdev):
    """WPS AP trying to enable WEP"""
    check_wep_capa(dev[0])
    ssid = "test-wps"
    hapd = hostapd.add_ap(apdev[0],
                          {"ssid": ssid, "eap_server": "1", "wps_state": "1",
                           "ieee80211n": "0", "wep_key0": '"hello"'})
    if "FAIL" not in hapd.request("WPS_PBC"):
        raise Exception("WPS unexpectedly enabled")

def test_ap_wps_tkip(dev, apdev):
    """WPS AP trying to enable TKIP"""
    ssid = "test-wps"
    hapd = hostapd.add_ap(apdev[0],
                          {"ssid": ssid, "eap_server": "1", "wps_state": "1",
                           "ieee80211n": "0", "wpa": '1',
                           "wpa_key_mgmt": "WPA-PSK",
                           "wpa_passphrase": "12345678"})
    if "FAIL" not in hapd.request("WPS_PBC"):
        raise Exception("WPS unexpectedly enabled")

def test_ap_wps_conf_dummy_cred(dev, apdev):
    """WPS PIN provisioning with configured AP using dummy cred"""
    ssid = "test-wps-conf"
    hapd = hostapd.add_ap(apdev[0],
                          {"ssid": ssid, "eap_server": "1", "wps_state": "2",
                           "wpa_passphrase": "12345678", "wpa": "2",
                           "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP"})
    hapd.request("WPS_PIN any 12345670")
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[0].dump_monitor()
    try:
        hapd.set("wps_testing_dummy_cred", "1")
        dev[0].request("WPS_PIN " + apdev[0]['bssid'] + " 12345670")
        for i in range(1, 3):
            ev = dev[0].wait_event(["WPS-CRED-RECEIVED"], timeout=15)
            if ev is None:
                raise Exception("WPS credential %d not received" % i)
        dev[0].wait_connected(timeout=30)
    finally:
        hapd.set("wps_testing_dummy_cred", "0")

def test_ap_wps_rf_bands(dev, apdev):
    """WPS and wps_rf_bands configuration"""
    ssid = "test-wps-conf"
    params = {"ssid": ssid, "eap_server": "1", "wps_state": "2",
              "wpa_passphrase": "12345678", "wpa": "2",
              "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP",
              "wps_rf_bands": "ag"}

    hapd = hostapd.add_ap(apdev[0], params)
    bssid = hapd.own_addr()
    hapd.request("WPS_PBC")
    dev[0].scan_for_bss(bssid, freq="2412")
    dev[0].dump_monitor()
    dev[0].request("WPS_PBC " + bssid)
    dev[0].wait_connected(timeout=30)
    bss = dev[0].get_bss(bssid)
    logger.info("BSS: " + str(bss))
    if "103c000103" not in bss['ie']:
        raise Exception("RF Bands attribute with expected values not found")
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()
    hapd.set("wps_rf_bands", "ad")
    hapd.set("wps_rf_bands", "a")
    hapd.set("wps_rf_bands", "g")
    hapd.set("wps_rf_bands", "b")
    hapd.set("wps_rf_bands", "ga")
    hapd.disable()
    dev[0].dump_monitor()
    dev[0].flush_scan_cache()

def test_ap_wps_pbc_in_m1(dev, apdev):
    """WPS and pbc_in_m1"""
    ssid = "test-wps-conf"
    params = {"ssid": ssid, "eap_server": "1", "wps_state": "2",
              "wpa_passphrase": "12345678", "wpa": "2",
              "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP",
              "config_methods": "virtual_push_button virtual_display",
              "pbc_in_m1": "1"}

    hapd = hostapd.add_ap(apdev[0], params)
    bssid = hapd.own_addr()
    hapd.request("WPS_PBC")
    dev[0].scan_for_bss(bssid, freq="2412")
    dev[0].dump_monitor()
    dev[0].request("WPS_PBC " + bssid)
    dev[0].wait_connected(timeout=30)
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()
    hapd.disable()
    dev[0].dump_monitor()
    dev[0].flush_scan_cache()

def test_ap_wps_pbc_mac_addr_change(dev, apdev, params):
    """WPS M1 with MAC address change"""
    skip_without_tkip(dev[0])
    ssid = "test-wps-mac-addr-change"
    hapd = hostapd.add_ap(apdev[0],
                          {"ssid": ssid, "eap_server": "1", "wps_state": "1"})
    hapd.request("WPS_PBC")
    if "PBC Status: Active" not in hapd.request("WPS_GET_STATUS"):
        raise Exception("PBC status not shown correctly")
    dev[0].flush_scan_cache()

    test_addr = '02:11:22:33:44:55'
    addr = dev[0].get_status_field("address")
    if addr == test_addr:
        raise Exception("Unexpected initial MAC address")

    try:
        subprocess.call(['ip', 'link', 'set', 'dev', dev[0].ifname, 'down'])
        subprocess.call(['ip', 'link', 'set', 'dev', dev[0].ifname, 'address',
                         test_addr])
        subprocess.call(['ip', 'link', 'set', 'dev', dev[0].ifname, 'up'])
        addr1 = dev[0].get_status_field("address")
        if addr1 != test_addr:
            raise Exception("Failed to change MAC address")

        dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412", force_scan=True)
        dev[0].request("WPS_PBC " + apdev[0]['bssid'])
        dev[0].wait_connected(timeout=30)
        status = dev[0].get_status()
        if status['wpa_state'] != 'COMPLETED' or \
           status['bssid'] != apdev[0]['bssid']:
            raise Exception("Not fully connected")

        out = run_tshark(os.path.join(params['logdir'], "hwsim0.pcapng"),
                         "wps.message_type == 0x04",
                         display=["wps.mac_address"])
        res = out.splitlines()

        if len(res) < 1:
            raise Exception("No M1 message with MAC address found")
        if res[0] != addr1:
            raise Exception("Wrong M1 MAC address")
        dev[0].request("DISCONNECT")
        dev[0].wait_disconnected()
        hapd.disable()
        dev[0].dump_monitor()
        dev[0].flush_scan_cache()
    finally:
        # Restore MAC address
        subprocess.call(['ip', 'link', 'set', 'dev', dev[0].ifname, 'down'])
        subprocess.call(['ip', 'link', 'set', 'dev', dev[0].ifname, 'address',
                         addr])
        subprocess.call(['ip', 'link', 'set', 'dev', dev[0].ifname, 'up'])

def test_ap_wps_pin_start_failure(dev, apdev):
    """WPS_PIN start failure"""
    with alloc_fail(dev[0], 1, "wpas_wps_start_dev_pw"):
        if "FAIL" not in dev[0].request("WPS_PIN any 12345670"):
            raise Exception("WPS_PIN not rejected during OOM")
    with alloc_fail(dev[0], 1, "wpas_wps_start_dev_pw"):
        if "FAIL" not in dev[0].request("WPS_PIN any"):
            raise Exception("WPS_PIN not rejected during OOM")

def test_ap_wps_ap_pin_failure(dev, apdev):
    """WPS_AP_PIN failure"""
    id = dev[0].add_network()
    dev[0].set_network(id, "mode", "2")
    dev[0].set_network_quoted(id, "ssid", "wpas-ap-wps")
    dev[0].set_network_quoted(id, "psk", "1234567890")
    dev[0].set_network(id, "frequency", "2412")
    dev[0].set_network(id, "scan_freq", "2412")
    dev[0].select_network(id)
    dev[0].wait_connected()

    with fail_test(dev[0], 1,
                   "os_get_random;wpa_supplicant_ctrl_iface_wps_ap_pin"):
        if "FAIL" not in dev[0].request("WPS_AP_PIN random"):
            raise Exception("WPS_AP_PIN random accepted")
    with alloc_fail(dev[0], 1, "wpas_wps_ap_pin_set"):
        if "FAIL" not in dev[0].request("WPS_AP_PIN set 12345670"):
            raise Exception("WPS_AP_PIN set accepted")

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

def test_ap_wps_random_uuid(dev, apdev, params):
    """WPS and random UUID on Enrollee"""
    ssid = "test-wps-conf"
    hapd = hostapd.add_ap(apdev[0],
                          {"ssid": ssid, "eap_server": "1", "wps_state": "2",
                           "wpa_passphrase": "12345678", "wpa": "2",
                           "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP"})

    config = os.path.join(params['logdir'], 'ap_wps_random_uuid.conf')
    with open(config, "w") as f:
        f.write("auto_uuid=1\n")

    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')

    uuid = []
    for i in range(3):
        wpas.interface_add("wlan5", config=config)

        wpas.scan_for_bss(apdev[0]['bssid'], freq="2412")
        wpas.dump_monitor()
        wpas.request("WPS_PBC " + apdev[0]['bssid'])

        ev = hapd.wait_event(["WPS-ENROLLEE-SEEN"], timeout=10)
        if ev is None:
            raise Exception("Enrollee not seen")
        uuid.append(ev.split(' ')[2])
        wpas.request("WPS_CANCEL")
        wpas.dump_monitor()

        wpas.interface_remove("wlan5")

        hapd.dump_monitor()

    logger.info("Seen UUIDs: " + str(uuid))
    if uuid[0] == uuid[1] or uuid[0] == uuid[2] or uuid[1] == uuid[2]:
        raise Exception("Same UUID used multiple times")

def test_ap_wps_conf_pin_gcmp_128(dev, apdev):
    """WPS PIN provisioning with configured AP using GCMP-128"""
    run_ap_wps_conf_pin_cipher(dev, apdev, "GCMP")

def test_ap_wps_conf_pin_gcmp_256(dev, apdev):
    """WPS PIN provisioning with configured AP using GCMP-256"""
    run_ap_wps_conf_pin_cipher(dev, apdev, "GCMP-256")

def test_ap_wps_conf_pin_ccmp_256(dev, apdev):
    """WPS PIN provisioning with configured AP using CCMP-256"""
    run_ap_wps_conf_pin_cipher(dev, apdev, "CCMP-256")

def run_ap_wps_conf_pin_cipher(dev, apdev, cipher):
    if cipher not in dev[0].get_capability("pairwise"):
        raise HwsimSkip("Cipher %s not supported" % cipher)
    ssid = "test-wps-conf-pin"
    hapd = hostapd.add_ap(apdev[0],
                          {"ssid": ssid, "eap_server": "1", "wps_state": "2",
                           "wpa_passphrase": "12345678", "wpa": "2",
                           "wpa_key_mgmt": "WPA-PSK",
                           "rsn_pairwise": cipher})
    logger.info("WPS provisioning step")
    pin = dev[0].wps_read_pin()
    hapd.request("WPS_PIN any " + pin)
    dev[0].flush_scan_cache()
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[0].request("WPS_PIN %s %s" % (apdev[0]['bssid'], pin))
    dev[0].wait_connected(timeout=15)

def test_ap_wps_and_sae(dev, apdev):
    """Initial AP configuration with first WPS Enrollee and adding SAE"""
    skip_without_tkip(dev[0])
    skip_without_tkip(dev[1])
    try:
        run_ap_wps_and_sae(dev, apdev)
    finally:
        dev[0].set("wps_cred_add_sae", "0")

def run_ap_wps_and_sae(dev, apdev):
    check_sae_capab(dev[0])
    ssid = "test-wps-sae"
    hapd = hostapd.add_ap(apdev[0],
                          {"ssid": ssid, "eap_server": "1", "wps_state": "1",
                           "wps_cred_add_sae": "1"})
    logger.info("WPS provisioning step")
    pin = dev[0].wps_read_pin()
    hapd.request("WPS_PIN any " + pin)

    dev[0].set("wps_cred_add_sae", "1")
    dev[0].request("SET sae_groups ")
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412", force_scan=True)
    dev[0].request("WPS_PIN " + apdev[0]['bssid'] + " " + pin)
    dev[0].wait_connected(timeout=30)
    status = dev[0].get_status()
    if status['key_mgmt'] != "SAE":
        raise Exception("SAE not used")
    if 'pmf' not in status or status['pmf'] != "1":
        raise Exception("PMF not enabled")

    pin = dev[1].wps_read_pin()
    hapd.request("WPS_PIN any " + pin)
    dev[1].scan_for_bss(apdev[0]['bssid'], freq="2412", force_scan=True)
    dev[1].request("WPS_PIN " + apdev[0]['bssid'] + " " + pin)
    dev[1].wait_connected(timeout=30)
    status = dev[1].get_status()
    if status['key_mgmt'] != "WPA2-PSK":
        raise Exception("WPA2-PSK not used")
    if 'pmf' in status:
        raise Exception("PMF enabled")

def test_ap_wps_conf_and_sae(dev, apdev):
    """WPS PBC provisioning with configured AP using PSK+SAE"""
    try:
        run_ap_wps_conf_and_sae(dev, apdev)
    finally:
        dev[0].set("wps_cred_add_sae", "0")

def run_ap_wps_conf_and_sae(dev, apdev):
    check_sae_capab(dev[0])
    ssid = "test-wps-conf-sae"
    hapd = hostapd.add_ap(apdev[0],
                          {"ssid": ssid, "eap_server": "1", "wps_state": "2",
                           "wpa_passphrase": "12345678", "wpa": "2",
                           "ieee80211w": "1", "sae_require_mfp": "1",
                           "wpa_key_mgmt": "WPA-PSK SAE",
                           "rsn_pairwise": "CCMP"})

    dev[0].set("wps_cred_add_sae", "1")
    dev[0].request("SET sae_groups ")
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    pin = dev[0].wps_read_pin()
    hapd.request("WPS_PIN any " + pin)
    dev[0].request("WPS_PIN " + apdev[0]['bssid'] + " " + pin)
    dev[0].wait_connected(timeout=30)
    status = dev[0].get_status()
    if status['key_mgmt'] != "SAE":
        raise Exception("SAE not used")
    if 'pmf' not in status or status['pmf'] != "1":
        raise Exception("PMF not enabled")

    dev[1].connect(ssid, psk="12345678", scan_freq="2412", proto="WPA2",
                   key_mgmt="WPA-PSK", ieee80211w="0")

def test_ap_wps_reg_config_and_sae(dev, apdev):
    """WPS registrar configuring an AP using AP PIN and using PSK+SAE"""
    try:
        run_ap_wps_reg_config_and_sae(dev, apdev)
    finally:
        dev[0].set("wps_cred_add_sae", "0")

def run_ap_wps_reg_config_and_sae(dev, apdev):
    check_sae_capab(dev[0])
    ssid = "test-wps-init-ap-pin-sae"
    appin = "12345670"
    hostapd.add_ap(apdev[0],
                   {"ssid": ssid, "eap_server": "1", "wps_state": "2",
                    "ap_pin": appin, "wps_cred_add_sae": "1"})
    logger.info("WPS configuration step")
    dev[0].flush_scan_cache()
    dev[0].set("wps_cred_add_sae", "1")
    dev[0].request("SET sae_groups ")
    dev[0].scan_for_bss(apdev[0]['bssid'], freq=2412)
    dev[0].dump_monitor()
    new_ssid = "wps-new-ssid"
    new_passphrase = "1234567890"
    dev[0].wps_reg(apdev[0]['bssid'], appin, new_ssid, "WPA2PSK", "CCMP",
                   new_passphrase)
    status = dev[0].get_status()
    if status['key_mgmt'] != "SAE":
        raise Exception("SAE not used")
    if 'pmf' not in status or status['pmf'] != "1":
        raise Exception("PMF not enabled")

    dev[1].connect(new_ssid, psk=new_passphrase, scan_freq="2412", proto="WPA2",
                   key_mgmt="WPA-PSK", ieee80211w="0")

def test_ap_wps_appl_ext(dev, apdev):
    """WPS Application Extension attribute"""
    ssid = "test-wps-conf"
    params = {"ssid": ssid, "eap_server": "1", "wps_state": "2",
              "wps_application_ext": 16*"11" + 5*"ee",
              "wpa_passphrase": "12345678", "wpa": "2",
              "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP"}
    hapd = hostapd.add_ap(apdev[0], params)
    pin = dev[0].wps_read_pin()
    hapd.request("WPS_PIN any " + pin)
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[0].request("WPS_PIN %s %s" % (apdev[0]['bssid'], pin))
    dev[0].wait_connected(timeout=30)

@long_duration_test
def test_ap_wps_pbc_ap_timeout(dev, apdev):
    """WPS PBC timeout on AP"""
    run_ap_wps_ap_timeout(dev, apdev, "WPS_PBC")

@long_duration_test
def test_ap_wps_pin_ap_timeout(dev, apdev):
    """WPS PIN timeout on AP"""
    run_ap_wps_ap_timeout(dev, apdev, "WPS_PIN any 12345670 10")

def run_ap_wps_ap_timeout(dev, apdev, cmd):
    ssid = "test-wps-conf"
    hapd = hostapd.add_ap(apdev[0],
                          {"ssid": ssid, "eap_server": "1", "wps_state": "2",
                           "wpa_passphrase": "12345678", "wpa": "2",
                           "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP"})
    bssid = hapd.own_addr()
    hapd.request(cmd)
    time.sleep(1)
    dev[0].scan_for_bss(bssid, freq="2412")
    bss = dev[0].get_bss(bssid)
    logger.info("BSS during active Registrar: " + str(bss))
    if not bss['ie'].endswith("0106ffffffffffff"):
        raise Exception("Authorized MAC not included")
    ev = hapd.wait_event(["WPS-TIMEOUT"], timeout=130)
    if ev is None and "PBC" in cmd:
        raise Exception("WPS-TIMEOUT not reported")
    if "PBC" in cmd and \
       "PBC Status: Timed-out" not in hapd.request("WPS_GET_STATUS"):
        raise Exception("PBC status not shown correctly")

    time.sleep(5)
    dev[0].flush_scan_cache()
    dev[0].scan_for_bss(bssid, freq="2412", force_scan=True)
    bss = dev[0].get_bss(bssid)
    logger.info("BSS after timeout: " + str(bss))
    if bss['ie'].endswith("0106ffffffffffff"):
        raise Exception("Authorized MAC not removed")
