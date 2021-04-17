# IBSS test cases
# Copyright (c) 2013-2015, Jouni Malinen <j@w1.fi>
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

from remotehost import remote_compatible
import logging
logger = logging.getLogger()
import time
import re
import subprocess

import hwsim_utils
from utils import *

def connect_ibss_cmd(dev, id, freq=2412):
    dev.dump_monitor()
    dev.select_network(id, freq=str(freq))

def wait_ibss_connection(dev):
    logger.info(dev.ifname + " waiting for IBSS start/join to complete")
    ev = dev.wait_connected(timeout=20,
                            error="Connection to the IBSS timed out")
    exp = r'<.>(CTRL-EVENT-CONNECTED) - Connection to ([0-9a-f:]*) completed.*'
    s = re.split(exp, ev)
    if len(s) < 3:
        return None
    return s[2]

def wait_4way_handshake(dev1, dev2):
    logger.info(dev1.ifname + " waiting for 4-way handshake completion with " + dev2.ifname + " " + dev2.p2p_interface_addr())
    ev = dev1.wait_event(["IBSS-RSN-COMPLETED " + dev2.p2p_interface_addr()],
                         timeout=20)
    if ev is None:
        raise Exception("4-way handshake in IBSS timed out")

def wait_4way_handshake2(dev1, dev2, dev3):
    logger.info(dev1.ifname + " waiting for 4-way handshake completion with " + dev2.ifname + " " + dev2.p2p_interface_addr() + " and " + dev3.p2p_interface_addr())
    ev = dev1.wait_event(["IBSS-RSN-COMPLETED " + dev2.p2p_interface_addr(),
                          "IBSS-RSN-COMPLETED " + dev3.p2p_interface_addr()],
                         timeout=20)
    if ev is None:
        raise Exception("4-way handshake in IBSS timed out")
    ev = dev1.wait_event(["IBSS-RSN-COMPLETED " + dev2.p2p_interface_addr(),
                          "IBSS-RSN-COMPLETED " + dev3.p2p_interface_addr()],
                         timeout=20)
    if ev is None:
        raise Exception("4-way handshake in IBSS timed out")

def add_ibss(dev, ssid, psk=None, proto=None, key_mgmt=None, pairwise=None,
             group=None, beacon_int=None, bssid=None, scan_freq=None,
             wep_key0=None, freq=2412, chwidth=0, group_rekey=0):
    id = dev.add_network()
    dev.set_network(id, "mode", "1")
    dev.set_network(id, "frequency", str(freq))
    if chwidth > 0:
        dev.set_network(id, "max_oper_chwidth", str(chwidth))
    if scan_freq:
        dev.set_network(id, "scan_freq", str(scan_freq))
    dev.set_network_quoted(id, "ssid", ssid)
    if psk:
        dev.set_network_quoted(id, "psk", psk)
    if proto:
        dev.set_network(id, "proto", proto)
    if key_mgmt:
        dev.set_network(id, "key_mgmt", key_mgmt)
    if pairwise:
        dev.set_network(id, "pairwise", pairwise)
    if group:
        dev.set_network(id, "group", group)
    if beacon_int:
        dev.set_network(id, "beacon_int", beacon_int)
    if bssid:
        dev.set_network(id, "bssid", bssid)
    if wep_key0:
        dev.set_network(id, "wep_key0", wep_key0)
    if group_rekey:
        dev.set_network(id, "group_rekey", str(group_rekey))
    dev.request("ENABLE_NETWORK " + str(id) + " no-connect")
    return id

def add_ibss_rsn(dev, ssid, group_rekey=0, scan_freq=None):
    return add_ibss(dev, ssid, "12345678", "RSN", "WPA-PSK", "CCMP", "CCMP",
                    group_rekey=group_rekey, scan_freq=scan_freq)

def add_ibss_rsn_tkip(dev, ssid):
    return add_ibss(dev, ssid, "12345678", "RSN", "WPA-PSK", "TKIP", "TKIP")

def add_ibss_wpa_none(dev, ssid):
    return add_ibss(dev, ssid, "12345678", "WPA", "WPA-NONE", "TKIP", "TKIP")

def add_ibss_wpa_none_ccmp(dev, ssid):
    return add_ibss(dev, ssid, "12345678", "WPA", "WPA-NONE", "CCMP", "CCMP")

def test_ibss_rsn(dev):
    """IBSS RSN"""
    ssid = "ibss-rsn"

    logger.info("Start IBSS on the first STA")
    id = add_ibss_rsn(dev[0], ssid)
    # FIX: For now, this disables HT to avoid a strange issue with mac80211
    # frame reordering during the final test_connectivity() call. Once that is
    # figured out, these disable_ht=1 calls should be removed from the test
    # case.
    dev[0].set_network(id, "disable_ht", "1")
    connect_ibss_cmd(dev[0], id)
    bssid0 = wait_ibss_connection(dev[0])

    logger.info("Join two STAs to the IBSS")

    id = add_ibss_rsn(dev[1], ssid)
    dev[1].set_network(id, "disable_ht", "1")
    connect_ibss_cmd(dev[1], id)
    bssid1 = wait_ibss_connection(dev[1])
    if bssid0 != bssid1:
        logger.info("STA0 BSSID " + bssid0 + " differs from STA1 BSSID " + bssid1)
        # try to merge with a scan
        dev[1].scan()
    wait_4way_handshake(dev[0], dev[1])
    wait_4way_handshake(dev[1], dev[0])

    id = add_ibss_rsn(dev[2], ssid)
    connect_ibss_cmd(dev[2], id)
    bssid2 = wait_ibss_connection(dev[2])
    if bssid0 != bssid2:
        logger.info("STA0 BSSID " + bssid0 + " differs from STA2 BSSID " + bssid2)
        # try to merge with a scan
        dev[2].scan()
    wait_4way_handshake(dev[0], dev[2])
    wait_4way_handshake2(dev[2], dev[0], dev[1])

    # Allow some time for all peers to complete key setup
    time.sleep(3)
    hwsim_utils.test_connectivity(dev[0], dev[1])
    hwsim_utils.test_connectivity(dev[0], dev[2])
    hwsim_utils.test_connectivity(dev[1], dev[2])

    dev[1].request("REMOVE_NETWORK all")
    time.sleep(1)
    id = add_ibss_rsn(dev[1], ssid)
    dev[1].set_network(id, "disable_ht", "1")
    connect_ibss_cmd(dev[1], id)
    bssid1 = wait_ibss_connection(dev[1])
    if bssid0 != bssid1:
        logger.info("STA0 BSSID " + bssid0 + " differs from STA1 BSSID " + bssid1)
        # try to merge with a scan
        dev[1].scan()
    wait_4way_handshake(dev[0], dev[1])
    wait_4way_handshake(dev[1], dev[0])
    time.sleep(3)
    hwsim_utils.test_connectivity(dev[0], dev[1])

    if "OK" not in dev[0].request("IBSS_RSN " + dev[1].p2p_interface_addr()):
        raise Exception("IBSS_RSN command failed")

    key_mgmt = dev[0].get_status_field("key_mgmt")
    if key_mgmt != "WPA2-PSK":
        raise Exception("Unexpected STATUS key_mgmt: " + key_mgmt)

def test_ibss_rsn_group_rekey(dev):
    """IBSS RSN group rekeying"""
    ssid = "ibss-rsn"

    logger.info("Start IBSS on the first STA")
    id = add_ibss_rsn(dev[0], ssid, group_rekey=4, scan_freq=2412)
    connect_ibss_cmd(dev[0], id)
    bssid0 = wait_ibss_connection(dev[0])
    dev[0].dump_monitor()

    logger.info("Join two STAs to the IBSS")

    dev[1].scan_for_bss(bssid0, freq=2412)
    id = add_ibss_rsn(dev[1], ssid, scan_freq=2412)
    connect_ibss_cmd(dev[1], id)
    bssid1 = wait_ibss_connection(dev[1])
    if bssid0 != bssid1:
        raise Exception("STA0 BSSID " + bssid0 + " differs from STA1 BSSID " + bssid1)
    wait_4way_handshake(dev[0], dev[1])
    wait_4way_handshake(dev[1], dev[0])
    dev[0].dump_monitor()
    dev[1].dump_monitor()

    hwsim_utils.test_connectivity(dev[0], dev[1])
    ev = dev[1].wait_event(["WPA: Group rekeying completed"], timeout=10)
    if ev is None:
        raise Exception("No group rekeying reported")
    hwsim_utils.test_connectivity(dev[0], dev[1])

def test_ibss_wpa_none(dev):
    """IBSS WPA-None"""
    skip_without_tkip(dev[0])
    skip_without_tkip(dev[1])
    skip_without_tkip(dev[2])
    ssid = "ibss-wpa-none"

    logger.info("Start IBSS on the first STA")
    id = add_ibss_wpa_none(dev[0], ssid)
    connect_ibss_cmd(dev[0], id)
    bssid0 = wait_ibss_connection(dev[0])

    # This is a bit ugly, but no one really cares about WPA-None, so there may
    # not be enough justification to clean this up.. For now, wpa_supplicant
    # will show two connection events with mac80211_hwsim where the first one
    # comes with all zeros address.
    if bssid0 == "00:00:00:00:00:00":
        logger.info("Waiting for real BSSID on the first STA")
        bssid0 = wait_ibss_connection(dev[0])

    logger.info("Join two STAs to the IBSS")

    id = add_ibss_wpa_none(dev[1], ssid)
    connect_ibss_cmd(dev[1], id)
    id = add_ibss_wpa_none(dev[2], ssid)
    connect_ibss_cmd(dev[2], id)

    bssid1 = wait_ibss_connection(dev[1])
    if bssid0 != bssid1:
        logger.info("STA0 BSSID " + bssid0 + " differs from STA1 BSSID " + bssid1)
        bssid1 = wait_ibss_connection(dev[1])

    bssid2 = wait_ibss_connection(dev[2])
    if bssid0 != bssid2:
        logger.info("STA0 BSSID " + bssid0 + " differs from STA2 BSSID " + bssid2)
        bssid2 = wait_ibss_connection(dev[2])

    logger.info("bssid0=%s bssid1=%s bssid2=%s" % (bssid0, bssid1, bssid2))

    bss = dev[0].get_bss(bssid0)
    if not bss:
        bss = dev[1].get_bss(bssid1)
        if not bss:
            raise Exception("Could not find BSS entry for IBSS")
    if 'flags' not in bss:
        raise Exception("Could not get BSS flags from BSS table")
    if "[WPA-None-TKIP]" not in bss['flags']:
        raise Exception("Unexpected BSS flags: " + bss['flags'])

    # Allow some time for all peers to complete key setup
    time.sleep(1)

    # This is supposed to work, but looks like WPA-None does not work with
    # mac80211 currently..
    try:
        hwsim_utils.test_connectivity(dev[0], dev[1])
    except Exception as e:
        logger.info("Ignoring known connectivity failure: " + str(e))
    try:
        hwsim_utils.test_connectivity(dev[0], dev[2])
    except Exception as e:
        logger.info("Ignoring known connectivity failure: " + str(e))
    try:
        hwsim_utils.test_connectivity(dev[1], dev[2])
    except Exception as e:
        logger.info("Ignoring known connectivity failure: " + str(e))

    key_mgmt = dev[0].get_status_field("key_mgmt")
    if key_mgmt != "WPA-NONE":
        raise Exception("Unexpected STATUS key_mgmt: " + key_mgmt)

def test_ibss_wpa_none_ccmp(dev):
    """IBSS WPA-None/CCMP"""
    skip_without_tkip(dev[0])
    skip_without_tkip(dev[1])
    ssid = "ibss-wpa-none"

    logger.info("Start IBSS on the first STA")
    id = add_ibss_wpa_none(dev[0], ssid)
    connect_ibss_cmd(dev[0], id)
    bssid0 = wait_ibss_connection(dev[0])

    # This is a bit ugly, but no one really cares about WPA-None, so there may
    # not be enough justification to clean this up.. For now, wpa_supplicant
    # will show two connection events with mac80211_hwsim where the first one
    # comes with all zeros address.
    if bssid0 == "00:00:00:00:00:00":
        logger.info("Waiting for real BSSID on the first STA")
        bssid0 = wait_ibss_connection(dev[0])


    logger.info("Join a STA to the IBSS")
    id = add_ibss_wpa_none(dev[1], ssid)
    connect_ibss_cmd(dev[1], id)

    bssid1 = wait_ibss_connection(dev[1])
    if bssid0 != bssid1:
        logger.info("STA0 BSSID " + bssid0 + " differs from STA1 BSSID " + bssid1)
        bssid1 = wait_ibss_connection(dev[1])

    logger.info("bssid0=%s bssid1=%s" % (bssid0, bssid1))

    # Allow some time for all peers to complete key setup
    time.sleep(1)

    # This is supposed to work, but looks like WPA-None does not work with
    # mac80211 currently..
    try:
        hwsim_utils.test_connectivity(dev[0], dev[1])
    except Exception as e:
        logger.info("Ignoring known connectivity failure: " + str(e))

def test_ibss_open(dev):
    """IBSS open (no security)"""
    ssid = "ibss"
    id = add_ibss(dev[0], ssid, key_mgmt="NONE", beacon_int="150")
    connect_ibss_cmd(dev[0], id)
    bssid0 = wait_ibss_connection(dev[0])

    id = add_ibss(dev[1], ssid, key_mgmt="NONE", beacon_int="200")
    connect_ibss_cmd(dev[1], id)
    bssid1 = wait_ibss_connection(dev[1])
    if bssid0 != bssid1:
        logger.info("STA0 BSSID " + bssid0 + " differs from STA1 BSSID " + bssid1)

    res = dev[0].request("SCAN_RESULTS")
    if "[IBSS]" not in res:
        res = dev[1].request("SCAN_RESULTS")
        if "[IBSS]" not in res:
            raise Exception("IBSS flag missing from scan results: " + res)
    bss = dev[0].get_bss(bssid0)
    if not bss:
        bss = dev[1].get_bss(bssid1)
        if not bss:
            raise Exception("Could not find BSS entry for IBSS")
    if 'flags' not in bss:
        raise Exception("Could not get BSS flags from BSS table")
    if "[IBSS]" not in bss['flags']:
        raise Exception("Unexpected BSS flags: " + bss['flags'])

    freq0 = dev[0].get_status_field("freq")
    freq1 = dev[1].get_status_field("freq")
    if freq0 != "2412" or freq1 != "2412":
        raise Exception("IBSS operating frequency not reported correctly (%s %s)" % (freq0, freq1))

    key_mgmt = dev[0].get_status_field("key_mgmt")
    if key_mgmt != "NONE":
        raise Exception("Unexpected STATUS key_mgmt: " + key_mgmt)

def test_ibss_open_fixed_bssid(dev):
    """IBSS open (no security) and fixed BSSID"""
    ssid = "ibss"
    bssid = "02:11:22:33:44:55"
    try:
        dev[0].request("AP_SCAN 2")
        add_ibss(dev[0], ssid, key_mgmt="NONE", bssid=bssid, beacon_int="150")
        dev[0].request("REASSOCIATE")

        dev[1].request("AP_SCAN 2")
        add_ibss(dev[1], ssid, key_mgmt="NONE", bssid=bssid, beacon_int="200")
        dev[1].request("REASSOCIATE")

        bssid0 = wait_ibss_connection(dev[0])
        bssid1 = wait_ibss_connection(dev[1])
        if bssid0 != bssid:
            raise Exception("STA0 BSSID " + bssid0 + " differs from fixed BSSID " + bssid)
        if bssid1 != bssid:
            raise Exception("STA0 BSSID " + bssid0 + " differs from fixed BSSID " + bssid)
    finally:
        dev[0].request("AP_SCAN 1")
        dev[1].request("AP_SCAN 1")

def test_ibss_open_retry(dev):
    """IBSS open (no security) with cfg80211 retry workaround"""
    subprocess.check_call(['iw', 'dev', dev[0].ifname, 'set', 'type', 'adhoc'])
    subprocess.check_call(['iw', 'dev', dev[0].ifname, 'ibss', 'join',
                           'ibss-test', '2412', 'HT20', 'fixed-freq',
                           '02:22:33:44:55:66'])
    ssid = "ibss"
    try:
        dev[0].request("AP_SCAN 2")
        id = add_ibss(dev[0], ssid, key_mgmt="NONE", beacon_int="150",
                      bssid="02:33:44:55:66:77", scan_freq=2412)
        #connect_ibss_cmd(dev[0], id)
        dev[0].request("REASSOCIATE")
        bssid0 = wait_ibss_connection(dev[0])

        subprocess.check_call(['iw', 'dev', dev[0].ifname, 'ibss', 'leave'])
        time.sleep(1)
        dev[0].request("DISCONNECT")
    finally:
        dev[0].request("AP_SCAN 1")

def test_ibss_rsn_tkip(dev):
    """IBSS RSN with TKIP as the cipher"""
    skip_without_tkip(dev[0])
    skip_without_tkip(dev[1])
    ssid = "ibss-rsn-tkip"

    id = add_ibss_rsn_tkip(dev[0], ssid)
    connect_ibss_cmd(dev[0], id)
    bssid0 = wait_ibss_connection(dev[0])

    id = add_ibss_rsn_tkip(dev[1], ssid)
    connect_ibss_cmd(dev[1], id)
    bssid1 = wait_ibss_connection(dev[1])
    if bssid0 != bssid1:
        logger.info("STA0 BSSID " + bssid0 + " differs from STA1 BSSID " + bssid1)
        # try to merge with a scan
        dev[1].scan()
    wait_4way_handshake(dev[0], dev[1])
    wait_4way_handshake(dev[1], dev[0])

def test_ibss_wep(dev):
    """IBSS with WEP"""
    check_wep_capa(dev[0])
    check_wep_capa(dev[1])

    ssid = "ibss-wep"

    id = add_ibss(dev[0], ssid, key_mgmt="NONE", wep_key0='"hello"')
    connect_ibss_cmd(dev[0], id)
    bssid0 = wait_ibss_connection(dev[0])

    id = add_ibss(dev[1], ssid, key_mgmt="NONE", wep_key0='"hello"')
    connect_ibss_cmd(dev[1], id)
    bssid1 = wait_ibss_connection(dev[1])

@remote_compatible
def test_ibss_rsn_error_case(dev):
    """IBSS RSN regression test for IBSS_RSN prior IBSS setup"""
    if "FAIL" not in dev[0].request("IBSS_RSN 02:03:04:05:06:07"):
        raise Exception("Unexpected IBSS_RSN result")

def test_ibss_5ghz(dev):
    """IBSS on 5 GHz band"""
    try:
        _test_ibss_5ghz(dev)
    finally:
        subprocess.call(['iw', 'reg', 'set', '00'])
        dev[0].flush_scan_cache()
        dev[1].flush_scan_cache()

def _test_ibss_5ghz(dev):
    subprocess.call(['iw', 'reg', 'set', 'US'])
    for i in range(2):
        for j in range(5):
            ev = dev[i].wait_event(["CTRL-EVENT-REGDOM-CHANGE"], timeout=5)
            if ev is None:
                raise Exception("No regdom change event")
            if "alpha2=US" in ev:
                break
        dev[i].dump_monitor()

    ssid = "ibss"
    id = add_ibss(dev[0], ssid, key_mgmt="NONE", beacon_int="150", freq=5180)
    connect_ibss_cmd(dev[0], id, freq=5180)
    bssid0 = wait_ibss_connection(dev[0])

    dev[1].scan_for_bss(bssid0, freq=5180)
    id = add_ibss(dev[1], ssid, key_mgmt="NONE", beacon_int="200", freq=5180)
    connect_ibss_cmd(dev[1], id, freq=5180)
    bssid1 = wait_ibss_connection(dev[1])
    if bssid0 != bssid1:
        logger.info("STA0 BSSID " + bssid0 + " differs from STA1 BSSID " + bssid1)

    dev[0].request("DISCONNECT")
    dev[1].request("DISCONNECT")
    dev[0].dump_monitor()
    dev[1].dump_monitor()

def test_ibss_vht_80p80(dev):
    """IBSS on VHT 80+80 MHz channel"""
    try:
        _test_ibss_vht_80p80(dev)
    finally:
        subprocess.call(['iw', 'reg', 'set', '00'])
        dev[0].flush_scan_cache()
        dev[1].flush_scan_cache()

def _test_ibss_vht_80p80(dev):
    subprocess.call(['iw', 'reg', 'set', 'US'])
    for i in range(2):
        for j in range(5):
            ev = dev[i].wait_event(["CTRL-EVENT-REGDOM-CHANGE"], timeout=5)
            if ev is None:
                raise Exception("No regdom change event")
            if "alpha2=US" in ev:
                break
        dev[i].dump_monitor()

    ssid = "ibss"
    id = add_ibss(dev[0], ssid, key_mgmt="NONE", freq=5180, chwidth=3)
    connect_ibss_cmd(dev[0], id, freq=5180)
    bssid0 = wait_ibss_connection(dev[0])
    sig = dev[0].request("SIGNAL_POLL").splitlines()
    if "FREQUENCY=5180" not in sig:
        raise Exception("Unexpected SIGNAL_POLL value(1): " + str(sig))
    if "WIDTH=80+80 MHz" not in sig:
        raise Exception("Unexpected SIGNAL_POLL value(2): " + str(sig))
    if "CENTER_FRQ1=5210" not in sig:
        raise Exception("Unexpected SIGNAL_POLL value(3): " + str(sig))
    if "CENTER_FRQ2=5775" not in sig:
        raise Exception("Unexpected SIGNAL_POLL value(4): " + str(sig))

    dev[1].scan_for_bss(bssid0, freq=5180)
    id = add_ibss(dev[1], ssid, key_mgmt="NONE", freq=5180, chwidth=3)
    connect_ibss_cmd(dev[1], id, freq=5180)
    bssid1 = wait_ibss_connection(dev[1])
    if bssid0 != bssid1:
        logger.info("STA0 BSSID " + bssid0 + " differs from STA1 BSSID " + bssid1)

    sig = dev[1].request("SIGNAL_POLL").splitlines()
    if "FREQUENCY=5180" not in sig:
        raise Exception("Unexpected SIGNAL_POLL value(1b): " + str(sig))
    logger.info("STA1 SIGNAL_POLL: " + str(sig))
    # For now, don't report errors on joining STA failing to get 80+80 MHZ
    # since mac80211 missed functionality for that to work.

    dev[0].request("DISCONNECT")
    dev[1].request("DISCONNECT")
    dev[0].dump_monitor()
    dev[1].dump_monitor()

def test_ibss_rsn_oom(dev):
    """IBSS RSN OOM during wpa_init"""
    with alloc_fail(dev[0], 1, "wpa_init"):
        ssid = "ibss-rsn"
        id = add_ibss_rsn(dev[0], ssid, scan_freq=2412)
        connect_ibss_cmd(dev[0], id)
        bssid0 = wait_ibss_connection(dev[0])
    dev[0].request("REMOVE_NETWORK all")
    dev[0].dump_monitor()

    with alloc_fail(dev[0], 1, "=ibss_rsn_init"):
        ssid = "ibss-rsn"
        id = add_ibss_rsn(dev[0], ssid, scan_freq=2412)
        connect_ibss_cmd(dev[0], id)
        bssid0 = wait_ibss_connection(dev[0])
    dev[0].request("REMOVE_NETWORK all")
    dev[0].dump_monitor()

def send_eapol_rx(dev, dst):
    if "OK" not in dev.request("EAPOL_RX %s 0203005f02008a001000000000000000013a54fb19d8a785f5986bdc2ba800553550bc9513e6603eb50809154588c22b110000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000" % dst):
        raise Exception("EAPOL_RX for %s failed" % dst)

def test_ibss_rsn_eapol_trigger(dev):
    """IBSS RSN and EAPOL trigger for a new peer"""
    ssid = "ibss-rsn"

    id = add_ibss_rsn(dev[0], ssid, scan_freq=2412)
    connect_ibss_cmd(dev[0], id)
    bssid0 = wait_ibss_connection(dev[0])

    send_eapol_rx(dev[0], "02:ff:00:00:00:01")
    send_eapol_rx(dev[0], "02:ff:00:00:00:01")

    dst = "02:ff:00:00:00:01"
    logger.info("Too short EAPOL frame")
    if "OK" not in dev[0].request("EAPOL_RX %s 0203005e02008a001000000000000000013a54fb19d8a785f5986bdc2ba800553550bc9513e6603eb50809154588c22b1100000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000" % dst):
        raise Exception("EAPOL_RX for %s failed" % dst)
    logger.info("RSN: EAPOL frame (type 255) discarded, not a Key frame")
    if "OK" not in dev[0].request("EAPOL_RX %s 02ff005f02008a001000000000000000013a54fb19d8a785f5986bdc2ba800553550bc9513e6603eb50809154588c22b110000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000" % dst):
        raise Exception("EAPOL_RX for %s failed" % dst)
    logger.info("RSN: EAPOL frame payload size 96 invalid (frame size 99)")
    if "OK" not in dev[0].request("EAPOL_RX %s 0203006002008a001000000000000000013a54fb19d8a785f5986bdc2ba800553550bc9513e6603eb50809154588c22b110000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000" % dst):
        raise Exception("EAPOL_RX for %s failed" % dst)
    logger.info("RSN: EAPOL-Key type (255) unknown, discarded")
    if "OK" not in dev[0].request("EAPOL_RX %s 0203005fff008a001000000000000000013a54fb19d8a785f5986bdc2ba800553550bc9513e6603eb50809154588c22b110000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000" % dst):
        raise Exception("EAPOL_RX for %s failed" % dst)

    with alloc_fail(dev[0], 1, "ibss_rsn_rx_eapol"):
        send_eapol_rx(dev[0], "02:ff:00:00:00:02")
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")

    with alloc_fail(dev[0], 1, "wpa_auth_sta_init;ibss_rsn_auth_init"):
        send_eapol_rx(dev[0], "02:ff:00:00:00:03")
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")

    with alloc_fail(dev[0], 1, "=ibss_rsn_peer_init"):
        send_eapol_rx(dev[0], "02:ff:00:00:00:04")
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")

    with alloc_fail(dev[0], 1, "ibss_rsn_process_rx_eapol"):
        send_eapol_rx(dev[0], "02:ff:00:00:00:05")
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")

    with alloc_fail(dev[0], 1,
                    "wpa_sm_set_assoc_wpa_ie_default;ibss_rsn_supp_init"):
        send_eapol_rx(dev[0], "02:ff:00:00:00:06")
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")

    with alloc_fail(dev[0], 1, "wpa_sm_init;ibss_rsn_supp_init"):
        send_eapol_rx(dev[0], "02:ff:00:00:00:07")
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")

    with alloc_fail(dev[0], 1, "=ibss_rsn_supp_init"):
        send_eapol_rx(dev[0], "02:ff:00:00:00:08")
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")

    with alloc_fail(dev[0], 1, "supp_alloc_eapol"):
        send_eapol_rx(dev[0], "02:ff:00:00:00:09")
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")

    with alloc_fail(dev[0], 1, "wpa_validate_wpa_ie;ibss_rsn_auth_init"):
        send_eapol_rx(dev[0], "02:ff:00:00:00:0a")
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")

    logger.info("RSN: Timeout on waiting Authentication frame response")
    if "OK" not in dev[0].request("IBSS_RSN 02:ff:00:00:00:0b"):
        raise Exception("Unexpected IBSS_RSN result")
    time.sleep(1.1)
