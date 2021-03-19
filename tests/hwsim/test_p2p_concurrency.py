# P2P concurrency test cases
# Copyright (c) 2013-2015, Jouni Malinen <j@w1.fi>
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

from remotehost import remote_compatible
import logging
logger = logging.getLogger()
import subprocess
import time

import hwsim_utils
import hostapd
from p2p_utils import *
from utils import *

@remote_compatible
def test_concurrent_autogo(dev, apdev):
    """Concurrent P2P autonomous GO"""
    logger.info("Connect to an infrastructure AP")
    dev[0].request("P2P_SET cross_connect 0")
    hapd = hostapd.add_ap(apdev[0], {"ssid": "test-open"})
    dev[0].connect("test-open", key_mgmt="NONE", scan_freq="2412")
    hwsim_utils.test_connectivity(dev[0], hapd)

    logger.info("Start a P2P group while associated to an AP")
    dev[0].global_request("SET p2p_no_group_iface 0")
    dev[0].p2p_start_go()
    pin = dev[1].wps_read_pin()
    dev[0].p2p_go_authorize_client(pin)
    dev[1].p2p_connect_group(dev[0].p2p_dev_addr(), pin, timeout=60,
                             social=True)
    hwsim_utils.test_connectivity_p2p(dev[0], dev[1])
    dev[0].remove_group()
    dev[1].wait_go_ending_session()

    logger.info("Confirm AP connection after P2P group removal")
    hwsim_utils.test_connectivity(dev[0], hapd)

def test_concurrent_autogo_5ghz_ht40(dev, apdev):
    """Concurrent P2P autonomous GO on 5 GHz and HT40 co-ex"""
    clear_scan_cache(apdev[1])
    try:
        hapd = None
        hapd2 = None
        params = {"ssid": "ht40",
                  "hw_mode": "a",
                  "channel": "153",
                  "country_code": "US",
                  "ht_capab": "[HT40-]"}
        hapd2 = hostapd.add_ap(apdev[1], params)

        params = {"ssid": "test-open-5",
                  "hw_mode": "a",
                  "channel": "149",
                  "country_code": "US"}
        hapd = hostapd.add_ap(apdev[0], params)

        dev[0].request("P2P_SET cross_connect 0")
        dev[0].scan_for_bss(apdev[0]['bssid'], freq=5745)
        dev[0].scan_for_bss(apdev[1]['bssid'], freq=5765)
        dev[0].connect("test-open-5", key_mgmt="NONE", scan_freq="5745")

        dev[0].global_request("SET p2p_no_group_iface 0")
        if "OK" not in dev[0].global_request("P2P_GROUP_ADD ht40"):
            raise Exception("P2P_GROUP_ADD failed")
        ev = dev[0].wait_global_event(["P2P-GROUP-STARTED"], timeout=5)
        if ev is None:
            raise Exception("GO start up timed out")
        dev[0].group_form_result(ev)

        pin = dev[1].wps_read_pin()
        dev[0].p2p_go_authorize_client(pin)
        dev[1].p2p_find(freq=5745)
        addr0 = dev[0].p2p_dev_addr()
        count = 0
        while count < 10:
            time.sleep(0.25)
            count += 1
            if dev[1].peer_known(addr0):
                break
        dev[1].p2p_connect_group(addr0, pin, timeout=60)

        dev[0].remove_group()
        dev[1].wait_go_ending_session()
    finally:
        dev[0].request("REMOVE_NETWORK all")
        if hapd:
            hapd.request("DISABLE")
        if hapd2:
            hapd2.request("DISABLE")
        subprocess.call(['iw', 'reg', 'set', '00'])
        dev[0].flush_scan_cache()
        dev[1].flush_scan_cache()

def test_concurrent_autogo_crossconnect(dev, apdev):
    """Concurrent P2P autonomous GO"""
    dev[0].global_request("P2P_SET cross_connect 1")
    hapd = hostapd.add_ap(apdev[0], {"ssid": "test-open"})
    dev[0].connect("test-open", key_mgmt="NONE", scan_freq="2412")

    dev[0].global_request("SET p2p_no_group_iface 0")
    dev[0].p2p_start_go(no_event_clear=True)
    ev = dev[0].wait_global_event(["P2P-CROSS-CONNECT-ENABLE"], timeout=10)
    if ev is None:
        raise Exception("Timeout on cross connection enabled event")
    if dev[0].group_ifname + " " + dev[0].ifname not in ev:
        raise Exception("Unexpected interfaces: " + ev)
    dev[0].dump_monitor()

    dev[0].global_request("P2P_SET cross_connect 0")
    ev = dev[0].wait_global_event(["P2P-CROSS-CONNECT-DISABLE"], timeout=10)
    if ev is None:
        raise Exception("Timeout on cross connection disabled event")
    if dev[0].group_ifname + " " + dev[0].ifname not in ev:
        raise Exception("Unexpected interfaces: " + ev)
    dev[0].remove_group()

    dev[0].global_request("P2P_SET cross_connect 1")
    dev[0].p2p_start_go(no_event_clear=True)
    ev = dev[0].wait_global_event(["P2P-CROSS-CONNECT-ENABLE"], timeout=10)
    if ev is None:
        raise Exception("Timeout on cross connection enabled event")
    if dev[0].group_ifname + " " + dev[0].ifname not in ev:
        raise Exception("Unexpected interfaces: " + ev)
    dev[0].dump_monitor()
    dev[0].remove_group()
    ev = dev[0].wait_global_event(["P2P-CROSS-CONNECT-DISABLE"], timeout=10)
    if ev is None:
        raise Exception("Timeout on cross connection disabled event")
    dev[0].global_request("P2P_SET cross_connect 0")

@remote_compatible
def test_concurrent_p2pcli(dev, apdev):
    """Concurrent P2P client join"""
    logger.info("Connect to an infrastructure AP")
    hapd = hostapd.add_ap(apdev[0], {"ssid": "test-open"})
    dev[0].connect("test-open", key_mgmt="NONE", scan_freq="2412")
    hwsim_utils.test_connectivity(dev[0], hapd)

    logger.info("Join a P2P group while associated to an AP")
    dev[0].global_request("SET p2p_no_group_iface 0")
    dev[1].p2p_start_go(freq=2412)
    pin = dev[0].wps_read_pin()
    dev[1].p2p_go_authorize_client(pin)
    dev[0].p2p_connect_group(dev[1].p2p_dev_addr(), pin, timeout=60,
                             social=True)
    hwsim_utils.test_connectivity_p2p(dev[0], dev[1])
    dev[1].remove_group()
    dev[0].wait_go_ending_session()

    logger.info("Confirm AP connection after P2P group removal")
    hwsim_utils.test_connectivity(dev[0], hapd)

@remote_compatible
def test_concurrent_grpform_go(dev, apdev):
    """Concurrent P2P group formation to become GO"""
    logger.info("Connect to an infrastructure AP")
    hapd = hostapd.add_ap(apdev[0], {"ssid": "test-open"})
    dev[0].connect("test-open", key_mgmt="NONE", scan_freq="2412")
    hwsim_utils.test_connectivity(dev[0], hapd)

    logger.info("Form a P2P group while associated to an AP")
    dev[0].global_request("SET p2p_no_group_iface 0")

    [i_res, r_res] = go_neg_pin_authorized(i_dev=dev[0], i_intent=15,
                                           r_dev=dev[1], r_intent=0)
    check_grpform_results(i_res, r_res)
    remove_group(dev[0], dev[1])

    logger.info("Confirm AP connection after P2P group removal")
    hwsim_utils.test_connectivity(dev[0], hapd)

@remote_compatible
def test_concurrent_grpform_cli(dev, apdev):
    """Concurrent P2P group formation to become P2P Client"""
    logger.info("Connect to an infrastructure AP")
    hapd = hostapd.add_ap(apdev[0], {"ssid": "test-open"})
    dev[0].connect("test-open", key_mgmt="NONE", scan_freq="2412")
    hwsim_utils.test_connectivity(dev[0], hapd)

    logger.info("Form a P2P group while associated to an AP")
    dev[0].global_request("SET p2p_no_group_iface 0")

    [i_res, r_res] = go_neg_pin_authorized(i_dev=dev[0], i_intent=0,
                                           r_dev=dev[1], r_intent=15)
    check_grpform_results(i_res, r_res)
    remove_group(dev[0], dev[1])

    logger.info("Confirm AP connection after P2P group removal")
    hwsim_utils.test_connectivity(dev[0], hapd)

@remote_compatible
def test_concurrent_grpform_while_connecting(dev, apdev):
    """Concurrent P2P group formation while connecting to an AP"""
    logger.info("Start connection to an infrastructure AP")
    hapd = hostapd.add_ap(apdev[0], {"ssid": "test-open"})
    dev[0].connect("test-open", key_mgmt="NONE", wait_connect=False)

    logger.info("Form a P2P group while connecting to an AP")
    dev[0].global_request("SET p2p_no_group_iface 0")

    [i_res, r_res] = go_neg_pin_authorized(i_dev=dev[0], i_freq=2412,
                                           r_dev=dev[1], r_freq=2412)
    check_grpform_results(i_res, r_res)
    remove_group(dev[0], dev[1])

    logger.info("Confirm AP connection after P2P group removal")
    hwsim_utils.test_connectivity(dev[0], hapd)

@remote_compatible
def test_concurrent_grpform_while_connecting2(dev, apdev):
    """Concurrent P2P group formation while connecting to an AP (2)"""
    logger.info("Start connection to an infrastructure AP")
    hapd = hostapd.add_ap(apdev[0], {"ssid": "test-open"})
    dev[0].connect("test-open", key_mgmt="NONE", wait_connect=False)
    dev[1].flush_scan_cache()

    logger.info("Form a P2P group while connecting to an AP")
    dev[0].global_request("SET p2p_no_group_iface 0")

    [i_res, r_res] = go_neg_pbc(i_dev=dev[0], i_intent=15, i_freq=2412,
                                r_dev=dev[1], r_intent=0, r_freq=2412)
    check_grpform_results(i_res, r_res)
    remove_group(dev[0], dev[1])

    logger.info("Confirm AP connection after P2P group removal")
    dev[0].wait_completed()
    hwsim_utils.test_connectivity(dev[0], hapd)

@remote_compatible
def test_concurrent_grpform_while_connecting3(dev, apdev):
    """Concurrent P2P group formation while connecting to an AP (3)"""
    logger.info("Start connection to an infrastructure AP")
    hapd = hostapd.add_ap(apdev[0], {"ssid": "test-open"})
    dev[0].connect("test-open", key_mgmt="NONE", wait_connect=False)

    logger.info("Form a P2P group while connecting to an AP")
    dev[0].global_request("SET p2p_no_group_iface 0")

    [i_res, r_res] = go_neg_pbc(i_dev=dev[1], i_intent=15, i_freq=2412,
                                r_dev=dev[0], r_intent=0, r_freq=2412)
    check_grpform_results(i_res, r_res)
    remove_group(dev[0], dev[1])

    logger.info("Confirm AP connection after P2P group removal")
    dev[0].wait_completed()
    hwsim_utils.test_connectivity(dev[0], hapd)

@remote_compatible
def test_concurrent_persistent_group(dev, apdev):
    """Concurrent P2P persistent group"""
    logger.info("Connect to an infrastructure AP")
    hostapd.add_ap(apdev[0], {"ssid": "test-open", "channel": "2"})
    dev[0].global_request("SET p2p_no_group_iface 0")
    dev[0].connect("test-open", key_mgmt="NONE", scan_freq="2417")

    logger.info("Run persistent group test while associated to an AP")
    form(dev[0], dev[1])
    [go_res, cli_res] = invite_from_cli(dev[0], dev[1])
    if go_res['freq'] != '2417':
        raise Exception("Unexpected channel selected: " + go_res['freq'])
    [go_res, cli_res] = invite_from_go(dev[0], dev[1])
    if go_res['freq'] != '2417':
        raise Exception("Unexpected channel selected: " + go_res['freq'])

def test_concurrent_invitation_channel_mismatch(dev, apdev):
    """P2P persistent group invitation and channel mismatch"""
    if dev[0].get_mcc() > 1:
        raise HwsimSkip("Skip due to MCC being enabled")

    form(dev[0], dev[1])
    dev[0].dump_monitor()
    dev[1].dump_monitor()

    logger.info("Connect to an infrastructure AP")
    hostapd.add_ap(apdev[0], {"ssid": "test-open", "channel": "2"})
    dev[0].global_request("SET p2p_no_group_iface 0")
    dev[0].connect("test-open", key_mgmt="NONE", scan_freq="2417")
    invite(dev[1], dev[0], extra="freq=2412")
    ev = dev[1].wait_global_event(["P2P-INVITATION-RESULT"], timeout=15)
    if ev is None:
        raise Exception("P2P invitation result not received")
    if "status=7" not in ev:
        raise Exception("Unexpected P2P invitation result: " + ev)
