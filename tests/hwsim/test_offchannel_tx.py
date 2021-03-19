# cfg80211 offchannel TX using remain-on-channel
# Copyright (c) 2014, Jouni Malinen <j@w1.fi>
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

import logging
logger = logging.getLogger()

import hostapd
from wpasupplicant import WpaSupplicant
from test_gas import start_ap
from test_gas import anqp_get
from p2p_utils import *

def test_offchannel_tx_roc_gas(dev, apdev):
    """GAS using cfg80211 remain-on-channel for offchannel TX"""
    start_ap(apdev[0])
    bssid = apdev[0]['bssid']

    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    wpas.interface_add("wlan5", drv_params="no_offchannel_tx=1")
    wpas.flush_scan_cache()
    wpas.scan_for_bss(bssid, freq=2412)
    anqp_get(wpas, bssid, 263)
    ev = wpas.wait_event(["GAS-QUERY-DONE"], timeout=10)
    if ev is None:
        raise Exception("GAS query timed out")
    if "result=SUCCESS" not in ev:
        raise Exception("Unexpected GAS query result")

def test_offchannel_tx_roc_grpform(dev, apdev):
    """P2P group formation using cfg80211 remain-on-channel for offchannel TX"""
    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    wpas.interface_add("wlan5", drv_params="no_offchannel_tx=1")

    [i_res, r_res] = go_neg_pin_authorized(i_dev=dev[0], i_freq=2412,
                                           r_dev=wpas, r_freq=2412)
    check_grpform_results(i_res, r_res)
    remove_group(dev[0], wpas)

def test_offchannel_tx_roc_grpform2(dev, apdev):
    """P2P group formation(2) using cfg80211 remain-on-channel for offchannel TX"""
    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    wpas.interface_add("wlan5", drv_params="no_offchannel_tx=1")

    [i_res, r_res] = go_neg_pin_authorized(i_dev=wpas, i_freq=2412,
                                           r_dev=dev[0], r_freq=2412)
    check_grpform_results(i_res, r_res)
    remove_group(dev[0], wpas)
