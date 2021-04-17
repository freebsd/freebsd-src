# rfkill tests
# Copyright (c) 2014, Jouni Malinen <j@w1.fi>
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

import logging
logger = logging.getLogger()
import time

import hostapd
from hostapd import HostapdGlobal
import hwsim_utils
from wpasupplicant import WpaSupplicant
from rfkill import RFKill
from utils import HwsimSkip
from hwsim import HWSimRadio

def get_rfkill(dev):
    phy = dev.get_driver_status_field("phyname")
    try:
        for r, s, h in RFKill.list():
            if r.name == phy:
                return r
    except Exception as e:
        raise HwsimSkip("No rfkill available: " + str(e))
    raise HwsimSkip("No rfkill match found for the interface")

def test_rfkill_open(dev, apdev):
    """rfkill block/unblock during open mode connection"""
    rfk = get_rfkill(dev[0])

    hapd = hostapd.add_ap(apdev[0], {"ssid": "open"})
    dev[0].connect("open", key_mgmt="NONE", scan_freq="2412")
    try:
        logger.info("rfkill block")
        rfk.block()
        dev[0].wait_disconnected(timeout=10,
                                 error="Missing disconnection event on rfkill block")

        if "FAIL" not in dev[0].request("REASSOCIATE"):
            raise Exception("REASSOCIATE accepted while disabled")
        if "FAIL" not in dev[0].request("REATTACH"):
            raise Exception("REATTACH accepted while disabled")
        if "FAIL" not in dev[0].request("RECONNECT"):
            raise Exception("RECONNECT accepted while disabled")
        if "FAIL" not in dev[0].request("FETCH_OSU"):
            raise Exception("FETCH_OSU accepted while disabled")

        logger.info("rfkill unblock")
        rfk.unblock()
        dev[0].wait_connected(timeout=10,
                              error="Missing connection event on rfkill unblock")
        hwsim_utils.test_connectivity(dev[0], hapd)
    finally:
        rfk.unblock()

def test_rfkill_wpa2_psk(dev, apdev):
    """rfkill block/unblock during WPA2-PSK connection"""
    rfk = get_rfkill(dev[0])

    ssid = "test-wpa2-psk"
    passphrase = 'qwertyuiop'
    params = hostapd.wpa2_params(ssid=ssid, passphrase=passphrase)
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].connect(ssid, psk=passphrase, scan_freq="2412")
    hapd.wait_sta()
    try:
        logger.info("rfkill block")
        rfk.block()
        dev[0].wait_disconnected(timeout=10,
                                 error="Missing disconnection event on rfkill block")

        logger.info("rfkill unblock")
        rfk.unblock()
        dev[0].wait_connected(timeout=10,
                              error="Missing connection event on rfkill unblock")
        hapd.wait_sta()
        hwsim_utils.test_connectivity(dev[0], hapd)
    finally:
        rfk.unblock()

def test_rfkill_autogo(dev, apdev):
    """rfkill block/unblock for autonomous P2P GO"""
    rfk0 = get_rfkill(dev[0])
    rfk1 = get_rfkill(dev[1])

    dev[0].p2p_start_go()
    dev[1].request("SET p2p_no_group_iface 0")
    dev[1].p2p_start_go()

    try:
        logger.info("rfkill block 0")
        rfk0.block()
        ev = dev[0].wait_global_event(["P2P-GROUP-REMOVED"], timeout=10)
        if ev is None:
            raise Exception("Group removal not reported")
        if "reason=UNAVAILABLE" not in ev:
            raise Exception("Unexpected group removal reason: " + ev)
        if "FAIL" not in dev[0].request("P2P_LISTEN 1"):
            raise Exception("P2P_LISTEN accepted unexpectedly")
        if "FAIL" not in dev[0].request("P2P_LISTEN"):
            raise Exception("P2P_LISTEN accepted unexpectedly")

        logger.info("rfkill block 1")
        rfk1.block()
        ev = dev[1].wait_global_event(["P2P-GROUP-REMOVED"], timeout=10)
        if ev is None:
            raise Exception("Group removal not reported")
        if "reason=UNAVAILABLE" not in ev:
            raise Exception("Unexpected group removal reason: " + ev)

        logger.info("rfkill unblock 0")
        rfk0.unblock()
        logger.info("rfkill unblock 1")
        rfk1.unblock()
        time.sleep(1)
    finally:
        rfk0.unblock()
        rfk1.unblock()

def _test_rfkill_p2p_discovery(dev0, dev1):
    """rfkill block/unblock P2P Discovery"""
    rfk0 = get_rfkill(dev0)
    rfk1 = get_rfkill(dev1)

    try:
        addr0 = dev0.p2p_dev_addr()

        logger.info("rfkill block 0")
        rfk0.block()
        logger.info("rfkill block 1")
        rfk1.block()

        for i in range(10):
            time.sleep(0.1)
            if dev0.get_status_field("wpa_state") == "INTERFACE_DISABLED" and dev1.get_status_field("wpa_state") == "INTERFACE_DISABLED":
                break

        if "OK" in dev0.p2p_listen():
            raise Exception("P2P Listen success although in rfkill")

        if "OK" in dev1.p2p_find():
            raise Exception("P2P Find success although in rfkill")

        dev0.dump_monitor()
        dev1.dump_monitor()

        logger.info("rfkill unblock 0")
        rfk0.unblock()
        logger.info("rfkill unblock 1")
        rfk1.unblock()

        for i in range(10):
            time.sleep(0.1)
            if dev0.get_status_field("wpa_state") != "INTERFACE_DISABLED" and dev1.get_status_field("wpa_state") != "INTERFACE_DISABLED":
                break

        if "OK" not in dev0.p2p_listen():
            raise Exception("P2P Listen failed after unblocking rfkill")

        if not dev1.discover_peer(addr0, social=True):
            raise Exception("Failed to discover peer after unblocking rfkill")

    finally:
        rfk0.unblock()
        rfk1.unblock()
        dev0.p2p_stop_find()
        dev1.p2p_stop_find()
        dev0.dump_monitor()
        dev1.dump_monitor()

def test_rfkill_p2p_discovery(dev, apdev):
    """rfkill block/unblock P2P Discovery"""
    _test_rfkill_p2p_discovery(dev[0], dev[1])

def test_rfkill_p2p_discovery_p2p_dev(dev, apdev):
    """rfkill block/unblock P2P Discovery with P2P Device"""
    with HWSimRadio(use_p2p_device=True) as (radio, iface):
        wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
        wpas.interface_add(iface)
        _test_rfkill_p2p_discovery(dev[0], wpas)
        _test_rfkill_p2p_discovery(wpas, dev[1])

def test_rfkill_hostapd(dev, apdev):
    """rfkill block/unblock during and prior to hostapd operations"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": "open"})

    rfk = get_rfkill(hapd)

    try:
        rfk.block()
        ev = hapd.wait_event(["INTERFACE-DISABLED"], timeout=5)
        if ev is None:
            raise Exception("INTERFACE-DISABLED event not seen")
        rfk.unblock()
        ev = hapd.wait_event(["INTERFACE-ENABLED"], timeout=5)
        if ev is None:
            raise Exception("INTERFACE-ENABLED event not seen")
        # hostapd does not current re-enable beaconing automatically
        hapd.disable()
        hapd.enable()
        dev[0].connect("open", key_mgmt="NONE", scan_freq="2412")
        rfk.block()
        ev = hapd.wait_event(["INTERFACE-DISABLED"], timeout=5)
        if ev is None:
            raise Exception("INTERFACE-DISABLED event not seen")
        dev[0].wait_disconnected(timeout=10)
        dev[0].request("DISCONNECT")
        hapd.disable()

        hglobal = HostapdGlobal(apdev[0])
        hglobal.flush()
        hglobal.remove(apdev[0]['ifname'])

        hapd = hostapd.add_ap(apdev[0], {"ssid": "open2"},
                              no_enable=True)
        if "FAIL" not in hapd.request("ENABLE"):
            raise Exception("ENABLE succeeded unexpectedly (rfkill)")
    finally:
        rfk.unblock()

def test_rfkill_wpas(dev, apdev):
    """rfkill block prior to wpa_supplicant start"""
    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    wpas.interface_add("wlan5")
    rfk = get_rfkill(wpas)
    wpas.interface_remove("wlan5")
    try:
        rfk.block()
        wpas.interface_add("wlan5")
        time.sleep(0.5)
        state = wpas.get_status_field("wpa_state")
        if state != "INTERFACE_DISABLED":
            raise Exception("Unexpected state with rfkill blocked: " + state)
        rfk.unblock()
        time.sleep(0.5)
        state = wpas.get_status_field("wpa_state")
        if state == "INTERFACE_DISABLED":
            raise Exception("Unexpected state with rfkill unblocked: " + state)
    finally:
        rfk.unblock()
