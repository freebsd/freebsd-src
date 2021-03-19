# OCE tests
# Copyright (c) 2016, Intel Deutschland GmbH
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

import logging
logger = logging.getLogger()

import hostapd
from wpasupplicant import WpaSupplicant

from hwsim_utils import set_rx_rssi, reset_rx_rssi
import time
import os
from datetime import datetime
from utils import HwsimSkip

def check_set_tx_power(dev, apdev):
    hapd = hostapd.add_ap(apdev[0], {'ssid': 'check_tx_power'})
    set_rx_rssi(hapd, -50)

    dev[0].scan(freq=2412)
    ev = dev[0].wait_event(["CTRL-EVENT-SCAN-RESULTS"], 2)

    res = dev[0].request("SCAN_RESULTS")
    if '-50' not in res:
        raise HwsimSkip('set_rx_rssi not supported')

    reset_rx_rssi(hapd)

    dev[0].scan(freq=2412)
    ev = dev[0].wait_event(["CTRL-EVENT-SCAN-RESULTS"], 2)

    res = dev[0].request("SCAN_RESULTS")
    if '-30' not in res:
        raise HwsimSkip('set_rx_rssi not supported')

def run_rssi_based_assoc_rej_timeout(dev, apdev, params):
    rssi_retry_to = 5

    ap_params = {'ssid': "test-RSSI-ar-to",
                 'rssi_reject_assoc_rssi': '-45',
                 'rssi_reject_assoc_timeout': str(rssi_retry_to)}

    logger.info("Set APs RSSI rejection threshold to -45 dBm, retry timeout: " +
                str(rssi_retry_to))
    hapd = hostapd.add_ap(apdev[0], ap_params)

    logger.info("Set STAs TX RSSI to -50")
    set_rx_rssi(dev[0], -50)

    logger.info("STA is trying to connect")
    dev[0].connect("test-RSSI-ar-to", key_mgmt="NONE", scan_freq="2412",
                   wait_connect=False)

    ev = dev[0].wait_event(['CTRL-EVENT-ASSOC-REJECT'], 2)
    if ev is None:
        raise Exception("Association not rejected")
    if 'status_code=34' not in ev:
        raise Exception("STA assoc request was not rejected with status code 34: " + ev)
    t_rej = datetime.now()

    # Set the scan interval to make dev[0] look for connections
    if 'OK' not in dev[0].request("SCAN_INTERVAL 1"):
        raise Exception("Failed to set scan interval")

    logger.info("Validate that STA did not connect or sent assoc request within retry timeout")
    ev = dev[0].wait_event(['CTRL-EVENT-CONNECTED', 'CTRL-EVENT-ASSOC-REJECT'],
                           rssi_retry_to + 2)
    t_ev = datetime.now()

    if ((t_ev - t_rej).total_seconds() < rssi_retry_to):
        raise Exception("STA sent assoc request within retry timeout")

    if 'CTRL-EVENT-CONNECTED' in ev:
        raise Exception("STA connected with low RSSI")

    if not ev:
        raise Exception("STA didn't send association request after retry timeout!")

def test_rssi_based_assoc_rej_timeout(dev, apdev, params):
    """RSSI-based association rejection: no assoc request during retry timeout"""
    check_set_tx_power(dev, apdev)
    try:
        run_rssi_based_assoc_rej_timeout(dev, apdev, params)
    finally:
        reset_rx_rssi(dev[0])
        dev[0].request("SCAN_INTERVAL 5")

def run_rssi_based_assoc_rej_good_rssi(dev, apdev):
    ap_params = {'ssid': "test-RSSI-ar-to",
                 'rssi_reject_assoc_rssi': '-45',
                 'rssi_reject_assoc_timeout': '60'}

    logger.info("Set APs RSSI rejection threshold to -45 dBm")
    hapd = hostapd.add_ap(apdev[0], ap_params)

    logger.info("Set STAs TX RSSI to -45")
    set_rx_rssi(dev[0], -45)

    logger.info("STA is trying to connect")
    dev[0].connect("test-RSSI-ar-to", key_mgmt="NONE", scan_freq="2412")

def test_rssi_based_assoc_rej_good_rssi(dev, apdev):
    """RSSI-based association rejection: STA with RSSI above the threshold connects"""
    check_set_tx_power(dev, apdev)
    try:
        run_rssi_based_assoc_rej_good_rssi(dev, apdev)
    finally:
        reset_rx_rssi(dev[0])

def run_rssi_based_assoc_rssi_change(dev, hapd):
    logger.info("Set STAs and APs TX RSSI to -50")
    set_rx_rssi(dev[0], -50)
    set_rx_rssi(hapd, -50)

    # Set the scan interval to make dev[0] look for connections
    if 'OK' not in dev[0].request("SCAN_INTERVAL 1"):
        raise Exception("Failed to set scan interval")

    logger.info("STA is trying to connect")
    dev[0].connect("test-RSSI-ar-to", key_mgmt="NONE", scan_freq="2412",
                   wait_connect=False)

    try:
        dev[0].wait_completed(2)
    except:
        logger.info("STA didn't connect after 2 seconds.")
    else:
        raise Exception("STA connected with low RSSI")

    logger.info("Set STAs and APs TX RSSI to -40dBm, validate that STA connects")
    set_rx_rssi(dev[0], -40)
    set_rx_rssi(hapd, -40)

    dev[0].wait_completed(2)

def test_rssi_based_assoc_rssi_change(dev, apdev):
    """RSSI-based association rejection: connect after improving RSSI"""
    check_set_tx_power(dev, apdev)
    try:
        ap_params = {'ssid': "test-RSSI-ar-to",
                     'rssi_reject_assoc_rssi': '-45',
                     'rssi_reject_assoc_timeout': '60'}

        logger.info("Set APs RSSI rejection threshold to -45 dBm, retry timeout: 60")
        hapd = hostapd.add_ap(apdev[0], ap_params)

        run_rssi_based_assoc_rssi_change(dev, hapd)
    finally:
        reset_rx_rssi(dev[0])
        reset_rx_rssi(hapd)
        dev[0].request("SCAN_INTERVAL 5")

def test_oce_ap(dev, apdev):
    """OCE AP"""
    ssid = "test-oce"
    passphrase = 'qwertyuiop'
    params = hostapd.wpa2_params(ssid=ssid, passphrase=passphrase)
    params['ieee80211w'] = "1"
    params['mbo'] = "1"
    params['oce'] = "4"
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].connect(ssid, psk=passphrase, ieee80211w="1", scan_freq="2412")

def test_oce_ap_open(dev, apdev):
    """OCE AP (open)"""
    ssid = "test-oce"
    params = {"ssid": ssid}
    params['mbo'] = "1"
    params['oce'] = "4"
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].connect(ssid, key_mgmt="NONE", scan_freq="2412")

def test_oce_ap_open_connect_cmd(dev, apdev):
    """OCE AP (open, connect command)"""
    ssid = "test-oce"
    params = {"ssid": ssid}
    params['mbo'] = "1"
    params['oce'] = "4"
    hapd = hostapd.add_ap(apdev[0], params)
    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    wpas.interface_add("wlan5", drv_params="force_connect_cmd=1")
    wpas.connect(ssid, key_mgmt="NONE", scan_freq="2412")
