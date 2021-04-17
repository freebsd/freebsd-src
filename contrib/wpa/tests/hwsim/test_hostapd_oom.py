# hostapd and out-of-memory error paths
# Copyright (c) 2015, Jouni Malinen
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

from remotehost import remote_compatible
import logging
logger = logging.getLogger()
import time

import hostapd
from utils import *

def hostapd_oom_loop(apdev, params, start_func="main"):
    hapd = hostapd.add_ap(apdev[0], {"ssid": "ctrl"})

    count = 0
    for i in range(1, 1000):
        if "OK" not in hapd.request("TEST_ALLOC_FAIL %d:%s" % (i, start_func)):
            raise HwsimSkip("TEST_ALLOC_FAIL not supported")
        try:
            hostapd.add_ap(apdev[1], params, timeout=2.5)
            logger.info("Iteration %d - success" % i)
            hostapd.remove_bss(apdev[1])

            state = hapd.request('GET_ALLOC_FAIL')
            logger.info("GET_ALLOC_FAIL: " + state)
            hapd.request("TEST_ALLOC_FAIL 0:")
            if i < 3:
                raise Exception("AP setup succeeded during out-of-memory")
            if state.startswith('0:'):
                count = 0
            else:
                count += 1
                if count == 5:
                    break
        except Exception as e:
            logger.info("Iteration %d - %s" % (i, str(e)))

@remote_compatible
def test_hostapd_oom_open(dev, apdev):
    """hostapd failing to setup open mode due to OOM"""
    params = {"ssid": "open"}
    hostapd_oom_loop(apdev, params)

def test_hostapd_oom_wpa2_psk(dev, apdev):
    """hostapd failing to setup WPA2-PSK mode due to OOM"""
    params = hostapd.wpa2_params(ssid="test", passphrase="12345678")
    params['wpa_psk_file'] = 'hostapd.wpa_psk'
    hostapd_oom_loop(apdev, params)

    tests = ["hostapd_config_read_wpa_psk", "hostapd_derive_psk"]
    for t in tests:
        hapd = hostapd.add_ap(apdev[0], {"ssid": "ctrl"})
        hapd.request("TEST_ALLOC_FAIL 1:%s" % t)
        try:
            hostapd.add_ap(apdev[1], params, timeout=2.5)
            raise Exception("Unexpected add_ap() success during OOM")
        except Exception as e:
            if "Failed to enable hostapd" in str(e):
                pass
            else:
                raise
        state = hapd.request('GET_ALLOC_FAIL')
        if state != "0:%s" % t:
            raise Exception("OOM not triggered")

@remote_compatible
def test_hostapd_oom_wpa2_eap(dev, apdev):
    """hostapd failing to setup WPA2-EAP mode due to OOM"""
    params = hostapd.wpa2_eap_params(ssid="test")
    params['acct_server_addr'] = "127.0.0.1"
    params['acct_server_port'] = "1813"
    params['acct_server_shared_secret'] = "radius"
    hostapd_oom_loop(apdev, params)

@remote_compatible
def test_hostapd_oom_wpa2_eap_radius(dev, apdev):
    """hostapd failing to setup WPA2-EAP mode due to OOM in RADIUS"""
    params = hostapd.wpa2_eap_params(ssid="test")
    params['acct_server_addr'] = "127.0.0.1"
    params['acct_server_port'] = "1813"
    params['acct_server_shared_secret'] = "radius"
    hostapd_oom_loop(apdev, params, start_func="accounting_init")

def test_hostapd_oom_wpa2_psk_connect(dev, apdev):
    """hostapd failing during WPA2-PSK mode connection due to OOM"""
    params = hostapd.wpa2_params(ssid="test-wpa2-psk", passphrase="12345678")
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].request("SCAN_INTERVAL 1")
    count = 0
    for i in range(1, 1000):
        logger.info("Iteration %d" % i)
        if "OK" not in hapd.request("TEST_ALLOC_FAIL %d:main" % i):
            raise HwsimSkip("TEST_ALLOC_FAIL not supported")
        id = dev[0].connect("test-wpa2-psk", psk="12345678",
                            scan_freq="2412", wait_connect=False)
        ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED",
                                "CTRL-EVENT-SSID-TEMP-DISABLED"], timeout=5)
        if ev is None:
            logger.info("Timeout while waiting for connection in iteration %d" % i)
            dev[0].request("REMOVE_NETWORK all")
            time.sleep(0.1)
        else:
            if "CTRL-EVENT-SSID-TEMP-DISABLED" in ev:
                logger.info("Re-select to avoid long wait for temp disavle")
                dev[0].select_network(id)
                dev[0].wait_connected()
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()
        for i in range(3):
            dev[i].dump_monitor()
        hapd.dump_monitor()

        state = hapd.request('GET_ALLOC_FAIL')
        logger.info("GET_ALLOC_FAIL: " + state)
        hapd.request("TEST_ALLOC_FAIL 0:")
        if state.startswith('0:'):
            count = 0
        else:
            count += 1
            if count == 5:
                break
    dev[0].request("SCAN_INTERVAL 5")

@long_duration_test
def test_hostapd_oom_wpa2_eap_connect(dev, apdev):
    """hostapd failing during WPA2-EAP mode connection due to OOM"""
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    params['acct_server_addr'] = "127.0.0.1"
    params['acct_server_port'] = "1813"
    params['acct_server_shared_secret'] = "radius"
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].request("SCAN_INTERVAL 1")
    count = 0
    for i in range(1, 1000):
        logger.info("Iteration %d" % i)
        if "OK" not in hapd.request("TEST_ALLOC_FAIL %d:main" % i):
            raise HwsimSkip("TEST_ALLOC_FAIL not supported")
        id = dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP",
                            eap="GPSK", identity="gpsk user",
                            password="abcdefghijklmnop0123456789abcdef",
                            scan_freq="2412", wait_connect=False)
        ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED",
                                "CTRL-EVENT-SSID-TEMP-DISABLED"], timeout=5)
        if ev is None:
            logger.info("Timeout while waiting for connection in iteration %d" % i)
            dev[0].request("REMOVE_NETWORK all")
            time.sleep(0.1)
        else:
            if "CTRL-EVENT-SSID-TEMP-DISABLED" in ev:
                logger.info("Re-select to avoid long wait for temp disavle")
                dev[0].select_network(id)
                dev[0].wait_connected()
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()
        for i in range(3):
            dev[i].dump_monitor()
        hapd.dump_monitor()

        state = hapd.request('GET_ALLOC_FAIL')
        logger.info("GET_ALLOC_FAIL: " + state)
        hapd.request("TEST_ALLOC_FAIL 0:")
        if state.startswith('0:'):
            count = 0
        else:
            count += 1
            if count == 5:
                break
    dev[0].request("SCAN_INTERVAL 5")
