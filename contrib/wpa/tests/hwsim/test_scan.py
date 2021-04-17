# Scanning tests
# Copyright (c) 2013-2016, Jouni Malinen <j@w1.fi>
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

from remotehost import remote_compatible
import binascii
import time
import logging
logger = logging.getLogger()
import os
import struct
import subprocess

import hostapd
from wpasupplicant import WpaSupplicant
from utils import *
from tshark import run_tshark
from test_ap_csa import switch_channel, wait_channel_switch

def check_scan(dev, params, other_started=False, test_busy=False):
    if not other_started:
        dev.dump_monitor()
    id = dev.request("SCAN " + params)
    if "FAIL" in id:
        raise Exception("Failed to start scan")
    id = int(id)

    if test_busy:
        if "FAIL-BUSY" not in dev.request("SCAN"):
            raise Exception("SCAN command while already scanning not rejected")

    if other_started:
        ev = dev.wait_event(["CTRL-EVENT-SCAN-STARTED"])
        if ev is None:
            raise Exception("Other scan did not start")
        if "id=" + str(id) in ev:
            raise Exception("Own scan id unexpectedly included in start event")

        ev = dev.wait_event(["CTRL-EVENT-SCAN-RESULTS"])
        if ev is None:
            raise Exception("Other scan did not complete")
        if "id=" + str(id) in ev:
            raise Exception("Own scan id unexpectedly included in completed event")

    ev = dev.wait_event(["CTRL-EVENT-SCAN-STARTED"])
    if ev is None:
        raise Exception("Scan did not start")
    if "id=" + str(id) not in ev:
        raise Exception("Scan id not included in start event")
    if test_busy:
        if "FAIL-BUSY" not in dev.request("SCAN"):
            raise Exception("SCAN command while already scanning not rejected")

    ev = dev.wait_event(["CTRL-EVENT-SCAN-RESULTS"])
    if ev is None:
        raise Exception("Scan did not complete")
    if "id=" + str(id) not in ev:
        raise Exception("Scan id not included in completed event")

def check_scan_retry(dev, params, bssid):
    for i in range(0, 5):
        check_scan(dev, "freq=2412-2462,5180 use_id=1")
        if int(dev.get_bss(bssid)['age']) <= 1:
            return
    raise Exception("Unexpectedly old BSS entry")

@remote_compatible
def test_scan(dev, apdev):
    """Control interface behavior on scan parameters"""
    hostapd.add_ap(apdev[0], {"ssid": "test-scan"})
    bssid = apdev[0]['bssid']

    logger.info("Full scan")
    check_scan(dev[0], "use_id=1", test_busy=True)

    logger.info("Limited channel scan")
    check_scan_retry(dev[0], "freq=2412-2462,5180 use_id=1", bssid)

    # wait long enough to allow next scans to be verified not to find the AP
    time.sleep(2)

    logger.info("Passive single-channel scan")
    check_scan(dev[0], "freq=2457 passive=1 use_id=1")
    logger.info("Active single-channel scan")
    check_scan(dev[0], "freq=2452 passive=0 use_id=1")
    if int(dev[0].get_bss(bssid)['age']) < 2:
        raise Exception("Unexpectedly updated BSS entry")

    logger.info("Active single-channel scan on AP's operating channel")
    check_scan_retry(dev[0], "freq=2412 passive=0 use_id=1", bssid)

@remote_compatible
def test_scan_tsf(dev, apdev):
    """Scan and TSF updates from Beacon/Probe Response frames"""
    hostapd.add_ap(apdev[0], {"ssid": "test-scan",
                              'beacon_int': "100"})
    bssid = apdev[0]['bssid']

    tsf = []
    for passive in [1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1]:
        check_scan(dev[0], "freq=2412 passive=%d use_id=1" % passive)
        bss = dev[0].get_bss(bssid)
        if bss:
            tsf.append(int(bss['tsf']))
            logger.info("TSF: " + bss['tsf'])
    if tsf[-3] <= tsf[-4]:
        # For now, only write this in the log without failing the test case
        # since mac80211_hwsim does not yet update the Timestamp field in
        # Probe Response frames.
        logger.info("Probe Response did not update TSF")
        #raise Exception("Probe Response did not update TSF")
    if tsf[-1] <= tsf[-3]:
        raise Exception("Beacon did not update TSF")
    if 0 in tsf:
        raise Exception("0 TSF reported")

@remote_compatible
def test_scan_only(dev, apdev):
    """Control interface behavior on scan parameters with type=only"""
    hostapd.add_ap(apdev[0], {"ssid": "test-scan"})
    bssid = apdev[0]['bssid']

    logger.info("Full scan")
    check_scan(dev[0], "type=only use_id=1")

    logger.info("Limited channel scan")
    check_scan_retry(dev[0], "type=only freq=2412-2462,5180 use_id=1", bssid)

    # wait long enough to allow next scans to be verified not to find the AP
    time.sleep(2)

    logger.info("Passive single-channel scan")
    check_scan(dev[0], "type=only freq=2457 passive=1 use_id=1")
    logger.info("Active single-channel scan")
    check_scan(dev[0], "type=only freq=2452 passive=0 use_id=1")
    if int(dev[0].get_bss(bssid)['age']) < 2:
        raise Exception("Unexpectedly updated BSS entry")

    logger.info("Active single-channel scan on AP's operating channel")
    check_scan_retry(dev[0], "type=only freq=2412 passive=0 use_id=1", bssid)

@remote_compatible
def test_scan_external_trigger(dev, apdev):
    """Avoid operations during externally triggered scan"""
    hostapd.add_ap(apdev[0], {"ssid": "test-scan"})
    bssid = apdev[0]['bssid']
    dev[0].cmd_execute(['iw', dev[0].ifname, 'scan', 'trigger'])
    check_scan(dev[0], "use_id=1", other_started=True)

def test_scan_bss_expiration_count(dev, apdev):
    """BSS entry expiration based on scan results without match"""
    if "FAIL" not in dev[0].request("BSS_EXPIRE_COUNT 0"):
        raise Exception("Invalid BSS_EXPIRE_COUNT accepted")
    if "OK" not in dev[0].request("BSS_EXPIRE_COUNT 2"):
        raise Exception("BSS_EXPIRE_COUNT failed")
    hapd = hostapd.add_ap(apdev[0], {"ssid": "test-scan"})
    bssid = apdev[0]['bssid']
    dev[0].scan(freq="2412", only_new=True)
    if bssid not in dev[0].request("SCAN_RESULTS"):
        raise Exception("BSS not found in initial scan")
    hapd.request("DISABLE")
    # Try to give enough time for hostapd to have stopped mac80211 from
    # beaconing before checking a new scan. This is needed with UML time travel
    # testing.
    hapd.ping()
    time.sleep(0.2)
    dev[0].scan(freq="2412", only_new=True)
    if bssid not in dev[0].request("SCAN_RESULTS"):
        raise Exception("BSS not found in first scan without match")
    dev[0].scan(freq="2412", only_new=True)
    if bssid in dev[0].request("SCAN_RESULTS"):
        raise Exception("BSS found after two scans without match")

@remote_compatible
def test_scan_bss_expiration_age(dev, apdev):
    """BSS entry expiration based on age"""
    try:
        if "FAIL" not in dev[0].request("BSS_EXPIRE_AGE COUNT 9"):
            raise Exception("Invalid BSS_EXPIRE_AGE accepted")
        if "OK" not in dev[0].request("BSS_EXPIRE_AGE 10"):
            raise Exception("BSS_EXPIRE_AGE failed")
        hapd = hostapd.add_ap(apdev[0], {"ssid": "test-scan"})
        bssid = apdev[0]['bssid']
        # Allow couple more retries to avoid reporting errors during heavy load
        for i in range(5):
            dev[0].scan(freq="2412")
            if bssid in dev[0].request("SCAN_RESULTS"):
                break
        if bssid not in dev[0].request("SCAN_RESULTS"):
            raise Exception("BSS not found in initial scan")
        hapd.request("DISABLE")
        logger.info("Waiting for BSS entry to expire")
        time.sleep(7)
        if bssid not in dev[0].request("SCAN_RESULTS"):
            raise Exception("BSS expired too quickly")
        ev = dev[0].wait_event(["CTRL-EVENT-BSS-REMOVED"], timeout=15)
        if ev is None:
            raise Exception("BSS entry expiration timed out")
        if bssid in dev[0].request("SCAN_RESULTS"):
            raise Exception("BSS not removed after expiration time")
    finally:
        dev[0].request("BSS_EXPIRE_AGE 180")

@remote_compatible
def test_scan_filter(dev, apdev):
    """Filter scan results based on SSID"""
    try:
        if "OK" not in dev[0].request("SET filter_ssids 1"):
            raise Exception("SET failed")
        id = dev[0].connect("test-scan", key_mgmt="NONE", only_add_network=True)
        hostapd.add_ap(apdev[0], {"ssid": "test-scan"})
        bssid = apdev[0]['bssid']
        hostapd.add_ap(apdev[1], {"ssid": "test-scan2"})
        bssid2 = apdev[1]['bssid']
        dev[0].scan(freq="2412", only_new=True)
        if bssid not in dev[0].request("SCAN_RESULTS"):
            raise Exception("BSS not found in scan results")
        if bssid2 in dev[0].request("SCAN_RESULTS"):
            raise Exception("Unexpected BSS found in scan results")
        dev[0].set_network_quoted(id, "ssid", "")
        dev[0].scan(freq="2412")
        id2 = dev[0].connect("test", key_mgmt="NONE", only_add_network=True)
        dev[0].scan(freq="2412")
    finally:
        dev[0].request("SET filter_ssids 0")

@remote_compatible
def test_scan_int(dev, apdev):
    """scan interval configuration"""
    try:
        if "FAIL" not in dev[0].request("SCAN_INTERVAL -1"):
            raise Exception("Accepted invalid scan interval")
        if "OK" not in dev[0].request("SCAN_INTERVAL 1"):
            raise Exception("Failed to set scan interval")
        dev[0].connect("not-used", key_mgmt="NONE", scan_freq="2412",
                       wait_connect=False)
        times = {}
        for i in range(0, 3):
            logger.info("Waiting for scan to start")
            start = os.times()[4]
            ev = dev[0].wait_event(["CTRL-EVENT-SCAN-STARTED"], timeout=5)
            if ev is None:
                raise Exception("did not start a scan")
            stop = os.times()[4]
            times[i] = stop - start
            logger.info("Waiting for scan to complete")
            ev = dev[0].wait_event(["CTRL-EVENT-SCAN-RESULTS"], 10)
            if ev is None:
                raise Exception("did not complete a scan")
        logger.info("times=" + str(times))
        if times[0] > 1 or times[1] < 0.5 or times[1] > 1.5 or times[2] < 0.5 or times[2] > 1.5:
            raise Exception("Unexpected scan timing: " + str(times))
    finally:
        dev[0].request("SCAN_INTERVAL 5")

def test_scan_bss_operations(dev, apdev):
    """Control interface behavior on BSS parameters"""
    hostapd.add_ap(apdev[0], {"ssid": "test-scan"})
    bssid = apdev[0]['bssid']
    hostapd.add_ap(apdev[1], {"ssid": "test2-scan"})
    bssid2 = apdev[1]['bssid']

    dev[0].scan(freq="2412")
    dev[0].scan(freq="2412")
    dev[0].scan(freq="2412")

    id1 = dev[0].request("BSS FIRST MASK=0x1").splitlines()[0].split('=')[1]
    id2 = dev[0].request("BSS LAST MASK=0x1").splitlines()[0].split('=')[1]

    res = dev[0].request("BSS RANGE=ALL MASK=0x20001")
    if "id=" + id1 not in res:
        raise Exception("Missing BSS " + id1)
    if "id=" + id2 not in res:
        raise Exception("Missing BSS " + id2)
    if "====" not in res:
        raise Exception("Missing delim")
    if "####" not in res:
        raise Exception("Missing end")

    res = dev[0].request("BSS RANGE=ALL MASK=0")
    if "id=" + id1 not in res:
        raise Exception("Missing BSS " + id1)
    if "id=" + id2 not in res:
        raise Exception("Missing BSS " + id2)
    if "====" in res:
        raise Exception("Unexpected delim")
    if "####" in res:
        raise Exception("Unexpected end delim")

    res = dev[0].request("BSS RANGE=ALL MASK=0x1").splitlines()
    if len(res) != 2:
        raise Exception("Unexpected result: " + str(res))
    res = dev[0].request("BSS FIRST MASK=0x1")
    if "id=" + id1 not in res:
        raise Exception("Unexpected result: " + res)
    res = dev[0].request("BSS LAST MASK=0x1")
    if "id=" + id2 not in res:
        raise Exception("Unexpected result: " + res)
    res = dev[0].request("BSS ID-" + id1 + " MASK=0x1")
    if "id=" + id1 not in res:
        raise Exception("Unexpected result: " + res)
    res = dev[0].request("BSS NEXT-" + id1 + " MASK=0x1")
    if "id=" + id2 not in res:
        raise Exception("Unexpected result: " + res)
    res = dev[0].request("BSS NEXT-" + id2 + " MASK=0x1")
    if "id=" in res:
        raise Exception("Unexpected result: " + res)

    if len(dev[0].request("BSS RANGE=" + id2 + " MASK=0x1").splitlines()) != 0:
        raise Exception("Unexpected RANGE=1 result")
    if len(dev[0].request("BSS RANGE=" + id1 + "- MASK=0x1").splitlines()) != 2:
        raise Exception("Unexpected RANGE=0- result")
    if len(dev[0].request("BSS RANGE=-" + id2 + " MASK=0x1").splitlines()) != 2:
        raise Exception("Unexpected RANGE=-1 result")
    if len(dev[0].request("BSS RANGE=" + id1 + "-" + id2 + " MASK=0x1").splitlines()) != 2:
        raise Exception("Unexpected RANGE=0-1 result")
    if len(dev[0].request("BSS RANGE=" + id2 + "-" + id2 + " MASK=0x1").splitlines()) != 1:
        raise Exception("Unexpected RANGE=1-1 result")
    if len(dev[0].request("BSS RANGE=" + str(int(id2) + 1) + "-" + str(int(id2) + 10) + " MASK=0x1").splitlines()) != 0:
        raise Exception("Unexpected RANGE=2-10 result")
    if len(dev[0].request("BSS RANGE=0-" + str(int(id2) + 10) + " MASK=0x1").splitlines()) != 2:
        raise Exception("Unexpected RANGE=0-10 result")
    if len(dev[0].request("BSS RANGE=" + id1 + "-" + id1 + " MASK=0x1").splitlines()) != 1:
        raise Exception("Unexpected RANGE=0-0 result")

    res = dev[0].request("BSS p2p_dev_addr=FOO")
    if "FAIL" in res or "id=" in res:
        raise Exception("Unexpected result: " + res)
    res = dev[0].request("BSS p2p_dev_addr=00:11:22:33:44:55")
    if "FAIL" in res or "id=" in res:
        raise Exception("Unexpected result: " + res)

    dev[0].request("BSS_FLUSH 1000")
    res = dev[0].request("BSS RANGE=ALL MASK=0x1").splitlines()
    if len(res) != 2:
        raise Exception("Unexpected result after BSS_FLUSH 1000")
    dev[0].request("BSS_FLUSH 0")
    res = dev[0].request("BSS RANGE=ALL MASK=0x1").splitlines()
    if len(res) != 0:
        raise Exception("Unexpected result after BSS_FLUSH 0")

@remote_compatible
def test_scan_and_interface_disabled(dev, apdev):
    """Scan operation when interface gets disabled"""
    try:
        dev[0].request("SCAN")
        ev = dev[0].wait_event(["CTRL-EVENT-SCAN-STARTED"])
        if ev is None:
            raise Exception("Scan did not start")
        dev[0].request("DRIVER_EVENT INTERFACE_DISABLED")
        ev = dev[0].wait_event(["CTRL-EVENT-SCAN-RESULTS"], timeout=7)
        if ev is not None:
            raise Exception("Scan completed unexpectedly")

        # verify that scan is rejected
        if "FAIL" not in dev[0].request("SCAN"):
            raise Exception("New scan request was accepted unexpectedly")

        dev[0].request("DRIVER_EVENT INTERFACE_ENABLED")
        dev[0].scan(freq="2412")
    finally:
        dev[0].request("DRIVER_EVENT INTERFACE_ENABLED")

@remote_compatible
def test_scan_for_auth(dev, apdev):
    """cfg80211 workaround with scan-for-auth"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": "open"})
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    # Block sme-connect radio work with an external radio work item, so that
    # SELECT_NETWORK can decide to use fast associate without a new scan while
    # cfg80211 still has the matching BSS entry, but the actual connection is
    # not yet started.
    id = dev[0].request("RADIO_WORK add block-work")
    ev = dev[0].wait_event(["EXT-RADIO-WORK-START"])
    if ev is None:
        raise Exception("Timeout while waiting radio work to start")
    dev[0].connect("open", key_mgmt="NONE", scan_freq="2412",
                   wait_connect=False)
    dev[0].dump_monitor()
    # Clear cfg80211 BSS table.
    res, data = dev[0].cmd_execute(['iw', dev[0].ifname, 'scan', 'trigger',
                                    'freq', '2457', 'flush'])
    if res != 0:
        raise HwsimSkip("iw scan trigger flush not supported")
    ev = dev[0].wait_event(["CTRL-EVENT-SCAN-RESULTS"], 5)
    if ev is None:
        raise Exception("External flush scan timed out")
    # Release blocking radio work to allow connection to go through with the
    # cfg80211 BSS entry missing.
    dev[0].request("RADIO_WORK done " + id)

    dev[0].wait_connected(timeout=15)

@remote_compatible
def test_scan_for_auth_fail(dev, apdev):
    """cfg80211 workaround with scan-for-auth failing"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": "open"})
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    # Block sme-connect radio work with an external radio work item, so that
    # SELECT_NETWORK can decide to use fast associate without a new scan while
    # cfg80211 still has the matching BSS entry, but the actual connection is
    # not yet started.
    id = dev[0].request("RADIO_WORK add block-work")
    ev = dev[0].wait_event(["EXT-RADIO-WORK-START"])
    if ev is None:
        raise Exception("Timeout while waiting radio work to start")
    dev[0].connect("open", key_mgmt="NONE", scan_freq="2412",
                   wait_connect=False)
    dev[0].dump_monitor()
    hapd.disable()
    # Clear cfg80211 BSS table.
    res, data = dev[0].cmd_execute(['iw', dev[0].ifname, 'scan', 'trigger',
                                    'freq', '2457', 'flush'])
    if res != 0:
        raise HwsimSkip("iw scan trigger flush not supported")
    ev = dev[0].wait_event(["CTRL-EVENT-SCAN-RESULTS"], 5)
    if ev is None:
        raise Exception("External flush scan timed out")
    # Release blocking radio work to allow connection to go through with the
    # cfg80211 BSS entry missing.
    dev[0].request("RADIO_WORK done " + id)

    ev = dev[0].wait_event(["CTRL-EVENT-SCAN-RESULTS",
                            "CTRL-EVENT-CONNECTED"], 15)
    if ev is None:
        raise Exception("Scan event missing")
    if "CTRL-EVENT-CONNECTED" in ev:
        raise Exception("Unexpected connection")
    dev[0].request("DISCONNECT")

@remote_compatible
def test_scan_for_auth_wep(dev, apdev):
    """cfg80211 scan-for-auth workaround with WEP keys"""
    check_wep_capa(dev[0])
    dev[0].flush_scan_cache()
    hapd = hostapd.add_ap(apdev[0],
                          {"ssid": "wep", "wep_key0": '"abcde"',
                           "auth_algs": "2"})
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    # Block sme-connect radio work with an external radio work item, so that
    # SELECT_NETWORK can decide to use fast associate without a new scan while
    # cfg80211 still has the matching BSS entry, but the actual connection is
    # not yet started.
    id = dev[0].request("RADIO_WORK add block-work")
    ev = dev[0].wait_event(["EXT-RADIO-WORK-START"])
    if ev is None:
        raise Exception("Timeout while waiting radio work to start")
    dev[0].connect("wep", key_mgmt="NONE", wep_key0='"abcde"',
                   auth_alg="SHARED", scan_freq="2412", wait_connect=False)
    dev[0].dump_monitor()
    # Clear cfg80211 BSS table.
    res, data = dev[0].cmd_execute(['iw', dev[0].ifname, 'scan', 'trigger',
                                    'freq', '2457', 'flush'])
    if res != 0:
        raise HwsimSkip("iw scan trigger flush not supported")
    ev = dev[0].wait_event(["CTRL-EVENT-SCAN-RESULTS"], 5)
    if ev is None:
        raise Exception("External flush scan timed out")
    # Release blocking radio work to allow connection to go through with the
    # cfg80211 BSS entry missing.
    dev[0].request("RADIO_WORK done " + id)

    dev[0].wait_connected(timeout=15)

@remote_compatible
def test_scan_hidden(dev, apdev):
    """Control interface behavior on scan parameters"""
    dev[0].flush_scan_cache()
    ssid = "test-scan"
    wrong_ssid = "wrong"
    hapd = hostapd.add_ap(apdev[0], {"ssid": ssid,
                                     "ignore_broadcast_ssid": "1"})
    bssid = apdev[0]['bssid']

    check_scan(dev[0], "freq=2412 use_id=1")
    try:
        payload = struct.pack('BB', 0, len(wrong_ssid)) + wrong_ssid.encode()
        ssid_list = struct.pack('BB', 84, len(payload)) + payload
        cmd = "VENDOR_ELEM_ADD 14 " + binascii.hexlify(ssid_list).decode()
        if "OK" not in dev[0].request(cmd):
            raise Exception("VENDOR_ELEM_ADD failed")
        check_scan(dev[0], "freq=2412 use_id=1")

        payload = struct.pack('<L', binascii.crc32(wrong_ssid.encode()))
        ssid_list = struct.pack('BBB', 255, 1 + len(payload), 58) + payload
        cmd = "VENDOR_ELEM_ADD 14 " + binascii.hexlify(ssid_list).decode()
        if "OK" not in dev[0].request(cmd):
            raise Exception("VENDOR_ELEM_ADD failed")
        check_scan(dev[0], "freq=2412 use_id=1")
    finally:
        dev[0].request("VENDOR_ELEM_REMOVE 14 *")
    if "test-scan" in dev[0].request("SCAN_RESULTS"):
        raise Exception("BSS unexpectedly found in initial scan")

    id1 = dev[0].connect("foo", key_mgmt="NONE", scan_ssid="1",
                         only_add_network=True)
    id2 = dev[0].connect("test-scan", key_mgmt="NONE", scan_ssid="1",
                         only_add_network=True)
    id3 = dev[0].connect("bar", key_mgmt="NONE", only_add_network=True)

    check_scan(dev[0], "freq=2412 use_id=1")
    if "test-scan" in dev[0].request("SCAN_RESULTS"):
        raise Exception("BSS unexpectedly found in scan")

    # Allow multiple attempts to be more robust under heavy CPU load that can
    # result in Probe Response frames getting sent only after the station has
    # already stopped waiting for the response on the channel.
    found = False
    for i in range(10):
        check_scan(dev[0], "scan_id=%d,%d,%d freq=2412 use_id=1" % (id1, id2, id3))
        if "test-scan" in dev[0].request("SCAN_RESULTS"):
            found = True
            break
    if not found:
        raise Exception("BSS not found in scan")

    if "FAIL" not in dev[0].request("SCAN scan_id=1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17"):
        raise Exception("Too many scan_id values accepted")

    # Duplicate SSID removal
    check_scan(dev[0], "scan_id=%d,%d,%d freq=2412 use_id=1" % (id1, id1, id2))

    dev[0].request("REMOVE_NETWORK all")
    hapd.disable()
    dev[0].flush_scan_cache(freq=2432)
    dev[0].flush_scan_cache()

def test_scan_and_bss_entry_removed(dev, apdev):
    """Last scan result and connect work processing on BSS entry update"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": "open",
                                     "eap_server": "1",
                                     "wps_state": "2"})
    bssid = apdev[0]['bssid']

    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    wpas.interface_add("wlan5", drv_params="force_connect_cmd=1")

    # Add a BSS entry
    dev[0].scan_for_bss(bssid, freq="2412")
    wpas.scan_for_bss(bssid, freq="2412")

    # Start a connect radio work with a blocking entry preventing this from
    # proceeding; this stores a pointer to the selected BSS entry.
    id = dev[0].request("RADIO_WORK add block-work")
    w_id = wpas.request("RADIO_WORK add block-work")
    dev[0].wait_event(["EXT-RADIO-WORK-START"], timeout=1)
    wpas.wait_event(["EXT-RADIO-WORK-START"], timeout=1)
    nid = dev[0].connect("open", key_mgmt="NONE", scan_freq="2412",
                         wait_connect=False)
    w_nid = wpas.connect("open", key_mgmt="NONE", scan_freq="2412",
                         wait_connect=False)
    time.sleep(0.1)

    # Remove the BSS entry
    dev[0].request("BSS_FLUSH 0")
    wpas.request("BSS_FLUSH 0")

    # Allow the connect radio work to continue. The bss entry stored in the
    # pending connect work is now stale. This will result in the connection
    # attempt failing since the BSS entry does not exist.
    dev[0].request("RADIO_WORK done " + id)
    wpas.request("RADIO_WORK done " + w_id)

    ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED"], timeout=1)
    if ev is not None:
        raise Exception("Unexpected connection")
    dev[0].remove_network(nid)
    ev = wpas.wait_event(["CTRL-EVENT-CONNECTED"], timeout=1)
    if ev is not None:
        raise Exception("Unexpected connection")
    wpas.remove_network(w_nid)
    time.sleep(0.5)
    dev[0].request("BSS_FLUSH 0")
    wpas.request("BSS_FLUSH 0")

    # Add a BSS entry
    dev[0].scan_for_bss(bssid, freq="2412")
    wpas.scan_for_bss(bssid, freq="2412")

    # Start a connect radio work with a blocking entry preventing this from
    # proceeding; this stores a pointer to the selected BSS entry.
    id = dev[0].request("RADIO_WORK add block-work")
    w_id = wpas.request("RADIO_WORK add block-work")
    dev[0].wait_event(["EXT-RADIO-WORK-START"], timeout=1)
    wpas.wait_event(["EXT-RADIO-WORK-START"], timeout=1)

    # Schedule a connection based on the current BSS entry.
    dev[0].connect("open", key_mgmt="NONE", scan_freq="2412",
                   wait_connect=False)
    wpas.connect("open", key_mgmt="NONE", scan_freq="2412",
                 wait_connect=False)

    # Update scan results with results that have longer set of IEs so that new
    # memory needs to be allocated for the BSS entry.
    hapd.request("WPS_PBC")
    time.sleep(0.1)
    subprocess.call(['iw', dev[0].ifname, 'scan', 'trigger', 'freq', '2412'])
    subprocess.call(['iw', wpas.ifname, 'scan', 'trigger', 'freq', '2412'])
    time.sleep(0.1)

    # Allow the connect radio work to continue. The bss entry stored in the
    # pending connect work becomes stale during the scan and it must have been
    # updated for the connection to work.
    dev[0].request("RADIO_WORK done " + id)
    wpas.request("RADIO_WORK done " + w_id)

    dev[0].wait_connected(timeout=15, error="No connection (sme-connect)")
    wpas.wait_connected(timeout=15, error="No connection (connect)")
    dev[0].request("DISCONNECT")
    wpas.request("DISCONNECT")
    dev[0].flush_scan_cache()
    wpas.flush_scan_cache()

@remote_compatible
def test_scan_reqs_with_non_scan_radio_work(dev, apdev):
    """SCAN commands while non-scan radio_work is in progress"""
    id = dev[0].request("RADIO_WORK add test-work-a")
    ev = dev[0].wait_event(["EXT-RADIO-WORK-START"])
    if ev is None:
        raise Exception("Timeout while waiting radio work to start")

    if "OK" not in dev[0].request("SCAN"):
        raise Exception("SCAN failed")
    if "FAIL-BUSY" not in dev[0].request("SCAN"):
        raise Exception("SCAN accepted while one is already pending")
    if "FAIL-BUSY" not in dev[0].request("SCAN"):
        raise Exception("SCAN accepted while one is already pending")

    res = dev[0].request("RADIO_WORK show").splitlines()
    count = 0
    for l in res:
        if "scan" in l:
            count += 1
    if count != 1:
        logger.info(res)
        raise Exception("Unexpected number of scan radio work items")

    dev[0].dump_monitor()
    dev[0].request("RADIO_WORK done " + id)
    ev = dev[0].wait_event(["CTRL-EVENT-SCAN-STARTED"], timeout=5)
    if ev is None:
        raise Exception("Scan did not start")
    if "FAIL-BUSY" not in dev[0].request("SCAN"):
        raise Exception("SCAN accepted while one is already in progress")

    ev = dev[0].wait_event(["CTRL-EVENT-SCAN-RESULTS"], timeout=10)
    if ev is None:
        raise Exception("Scan did not complete")
    ev = dev[0].wait_event(["CTRL-EVENT-SCAN-STARTED"], timeout=0.2)
    if ev is not None:
        raise Exception("Unexpected scan started")

def test_scan_setband(dev, apdev):
    """Band selection for scan operations"""
    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    wpas.interface_add("wlan5")
    devs = [ dev[0], dev[1], dev[2], wpas ]

    try:
        hapd = None
        hapd2 = None
        params = {"ssid": "test-setband",
                  "hw_mode": "a",
                  "channel": "36",
                  "country_code": "US"}
        hapd = hostapd.add_ap(apdev[0], params)
        bssid = apdev[0]['bssid']

        params = {"ssid": "test-setband",
                  "hw_mode": "g",
                  "channel": "1"}
        hapd2 = hostapd.add_ap(apdev[1], params)
        bssid2 = apdev[1]['bssid']

        if "FAIL" not in dev[0].request("SET setband FOO"):
            raise Exception("Invalid set setband accepted")
        if "OK" not in dev[0].request("SET setband AUTO"):
            raise Exception("Failed to set setband")
        if "OK" not in dev[1].request("SET setband 5G"):
            raise Exception("Failed to set setband")
        if "OK" not in dev[2].request("SET setband 2G"):
            raise Exception("Failed to set setband")
        if "OK" not in wpas.request("SET setband 2G,5G"):
            raise Exception("Failed to set setband")

        # Allow a retry to avoid reporting errors during heavy load
        for j in range(5):
            for d in devs:
                d.request("SCAN only_new=1")

            for d in devs:
                ev = d.wait_event(["CTRL-EVENT-SCAN-RESULTS"], 15)
                if ev is None:
                    raise Exception("Scan timed out")

            res0 = dev[0].request("SCAN_RESULTS")
            res1 = dev[1].request("SCAN_RESULTS")
            res2 = dev[2].request("SCAN_RESULTS")
            res3 = wpas.request("SCAN_RESULTS")
            if bssid in res0 and bssid2 in res0 and \
               bssid in res1 and bssid2 in res2 and \
               bssid in res3 and bssid2 in res3:
                break

        res = dev[0].request("SCAN_RESULTS")
        if bssid not in res or bssid2 not in res:
            raise Exception("Missing scan result(0)")

        res = dev[1].request("SCAN_RESULTS")
        if bssid not in res:
            raise Exception("Missing scan result(1)")
        if bssid2 in res:
            raise Exception("Unexpected scan result(1)")

        res = dev[2].request("SCAN_RESULTS")
        if bssid2 not in res:
            raise Exception("Missing scan result(2)")
        if bssid in res:
            raise Exception("Unexpected scan result(2)")

        res = wpas.request("SCAN_RESULTS")
        if bssid not in res or bssid2 not in res:
            raise Exception("Missing scan result(3)")
    finally:
        if hapd:
            hapd.request("DISABLE")
        if hapd2:
            hapd2.request("DISABLE")
        subprocess.call(['iw', 'reg', 'set', '00'])
        for d in devs:
            d.request("SET setband AUTO")
            d.flush_scan_cache()

@remote_compatible
def test_scan_hidden_many(dev, apdev):
    """scan_ssid=1 with large number of profile with hidden SSID"""
    try:
        _test_scan_hidden_many(dev, apdev)
    finally:
        dev[0].flush_scan_cache(freq=2432)
        dev[0].flush_scan_cache()
        dev[0].request("SCAN_INTERVAL 5")

def _test_scan_hidden_many(dev, apdev):
    hapd = hostapd.add_ap(apdev[0], {"ssid": "test-scan-ssid",
                                     "ignore_broadcast_ssid": "1"})
    bssid = apdev[0]['bssid']

    dev[0].request("SCAN_INTERVAL 1")

    for i in range(5):
        id = dev[0].add_network()
        dev[0].set_network_quoted(id, "ssid", "foo")
        dev[0].set_network(id, "key_mgmt", "NONE")
        dev[0].set_network(id, "disabled", "0")
        dev[0].set_network(id, "scan_freq", "2412")
        dev[0].set_network(id, "scan_ssid", "1")

    dev[0].set_network_quoted(id, "ssid", "test-scan-ssid")
    dev[0].set_network(id, "key_mgmt", "NONE")
    dev[0].set_network(id, "disabled", "0")
    dev[0].set_network(id, "scan_freq", "2412")
    dev[0].set_network(id, "scan_ssid", "1")

    for i in range(5):
        id = dev[0].add_network()
        dev[0].set_network_quoted(id, "ssid", "foo")
        dev[0].set_network(id, "key_mgmt", "NONE")
        dev[0].set_network(id, "disabled", "0")
        dev[0].set_network(id, "scan_freq", "2412")
        dev[0].set_network(id, "scan_ssid", "1")

    dev[0].request("REASSOCIATE")
    dev[0].wait_connected(timeout=30)
    dev[0].request("REMOVE_NETWORK all")
    hapd.disable()

def test_scan_random_mac(dev, apdev, params):
    """Random MAC address in scans"""
    try:
        _test_scan_random_mac(dev, apdev, params)
    finally:
        dev[0].request("MAC_RAND_SCAN all enable=0")

def _test_scan_random_mac(dev, apdev, params):
    hostapd.add_ap(apdev[0], {"ssid": "test-scan"})
    bssid = apdev[0]['bssid']

    tests = ["",
             "addr=foo",
             "mask=foo",
             "enable=1",
             "all enable=1 mask=00:11:22:33:44:55",
             "all enable=1 addr=00:11:22:33:44:55",
             "all enable=1 addr=01:11:22:33:44:55 mask=ff:ff:ff:ff:ff:ff",
             "all enable=1 addr=00:11:22:33:44:55 mask=fe:ff:ff:ff:ff:ff",
             "enable=2 scan sched pno all",
             "pno enable=1",
             "all enable=2",
             "foo"]
    for args in tests:
        if "FAIL" not in dev[0].request("MAC_RAND_SCAN " + args):
            raise Exception("Invalid MAC_RAND_SCAN accepted: " + args)

    if dev[0].get_driver_status_field('capa.mac_addr_rand_scan_supported') != '1':
        raise HwsimSkip("Driver does not support random MAC address for scanning")

    tests = ["all enable=1",
             "all enable=1 addr=f2:11:22:33:44:55 mask=ff:ff:ff:ff:ff:ff",
             "all enable=1 addr=f2:11:33:00:00:00 mask=ff:ff:ff:00:00:00"]
    for args in tests:
        dev[0].request("MAC_RAND_SCAN " + args)
        dev[0].scan_for_bss(bssid, freq=2412, force_scan=True)

    out = run_tshark(os.path.join(params['logdir'], "hwsim0.pcapng"),
                     "wlan.fc.type_subtype == 4", ["wlan.ta"])
    if out is not None:
        addr = out.splitlines()
        logger.info("Probe Request frames seen from: " + str(addr))
        if dev[0].own_addr() in addr:
            raise Exception("Real address used to transmit Probe Request frame")
        if "f2:11:22:33:44:55" not in addr:
            raise Exception("Fully configured random address not seen")
        found = False
        for a in addr:
            if a.startswith('f2:11:33'):
                found = True
                break
        if not found:
            raise Exception("Fixed OUI random address not seen")

def test_scan_random_mac_connected(dev, apdev, params):
    """Random MAC address in scans while connected"""
    try:
        _test_scan_random_mac_connected(dev, apdev, params)
    finally:
        dev[0].request("MAC_RAND_SCAN all enable=0")

def _test_scan_random_mac_connected(dev, apdev, params):
    hostapd.add_ap(apdev[0], {"ssid": "test-scan"})
    bssid = apdev[0]['bssid']
    if dev[0].get_driver_status_field('capa.mac_addr_rand_scan_supported') != '1':
        raise HwsimSkip("Driver does not support random MAC address for scanning")

    dev[0].connect("test-scan", key_mgmt="NONE", scan_freq="2412")

    hostapd.add_ap(apdev[1], {"ssid": "test-scan-2", "channel": "11"})
    bssid1 = apdev[1]['bssid']

    # Verify that scanning can be completed while connected even if that means
    # disabling use of random MAC address.
    dev[0].request("MAC_RAND_SCAN all enable=1")
    dev[0].scan_for_bss(bssid1, freq=2462, force_scan=True)

@remote_compatible
def test_scan_trigger_failure(dev, apdev):
    """Scan trigger to the driver failing"""
    if dev[0].get_status_field('wpa_state') == "SCANNING":
        raise Exception("wpa_state was already SCANNING")

    hostapd.add_ap(apdev[0], {"ssid": "test-scan"})
    bssid = apdev[0]['bssid']

    if "OK" not in dev[0].request("SET test_failure 1"):
        raise Exception("Failed to set test_failure")

    if "OK" not in dev[0].request("SCAN"):
        raise Exception("SCAN command failed")
    ev = dev[0].wait_event(["CTRL-EVENT-SCAN-FAILED"], timeout=10)
    if ev is None:
        raise Exception("Did not receive CTRL-EVENT-SCAN-FAILED event")
    if "retry=1" in ev:
        raise Exception("Unexpected scan retry indicated")
    if dev[0].get_status_field('wpa_state') == "SCANNING":
        raise Exception("wpa_state SCANNING not cleared")

    id = dev[0].connect("test-scan", key_mgmt="NONE", scan_freq="2412",
                        only_add_network=True)
    dev[0].select_network(id)
    ev = dev[0].wait_event(["CTRL-EVENT-SCAN-FAILED"], timeout=10)
    if ev is None:
        raise Exception("Did not receive CTRL-EVENT-SCAN-FAILED event")
    if "retry=1" not in ev:
        raise Exception("No scan retry indicated for connection")
    if dev[0].get_status_field('wpa_state') == "SCANNING":
        raise Exception("wpa_state SCANNING not cleared")
    dev[0].request("SET test_failure 0")
    dev[0].wait_connected()

    dev[0].request("SET test_failure 1")
    if "OK" not in dev[0].request("SCAN"):
        raise Exception("SCAN command failed")
    ev = dev[0].wait_event(["CTRL-EVENT-SCAN-FAILED"], timeout=10)
    if ev is None:
        raise Exception("Did not receive CTRL-EVENT-SCAN-FAILED event")
    if "retry=1" in ev:
        raise Exception("Unexpected scan retry indicated")
    if dev[0].get_status_field('wpa_state') != "COMPLETED":
        raise Exception("wpa_state COMPLETED not restored")
    dev[0].request("SET test_failure 0")

@remote_compatible
def test_scan_specify_ssid(dev, apdev):
    """Control interface behavior on scan SSID parameter"""
    dev[0].flush_scan_cache()
    hapd = hostapd.add_ap(apdev[0], {"ssid": "test-hidden",
                                     "ignore_broadcast_ssid": "1"})
    bssid = apdev[0]['bssid']
    check_scan(dev[0], "freq=2412 use_id=1 ssid 414243")
    bss = dev[0].get_bss(bssid)
    if bss is not None and bss['ssid'] == 'test-hidden':
        raise Exception("BSS entry for hidden AP present unexpectedly")
    # Allow couple more retries to avoid reporting errors during heavy load
    for i in range(5):
        check_scan(dev[0], "freq=2412 ssid 414243 ssid 746573742d68696464656e ssid 616263313233 use_id=1")
        bss = dev[0].get_bss(bssid)
        if bss and 'test-hidden' in dev[0].request("SCAN_RESULTS"):
            break
    if bss is None:
        raise Exception("BSS entry for hidden AP not found")
    if 'test-hidden' not in dev[0].request("SCAN_RESULTS"):
        raise Exception("Expected SSID not included in the scan results")

    hapd.disable()
    dev[0].flush_scan_cache(freq=2432)
    dev[0].flush_scan_cache()

    if "FAIL" not in dev[0].request("SCAN ssid foo"):
        raise Exception("Invalid SCAN command accepted")

@remote_compatible
def test_scan_ap_scan_2_ap_mode(dev, apdev):
    """AP_SCAN 2 AP mode and scan()"""
    try:
        _test_scan_ap_scan_2_ap_mode(dev, apdev)
    finally:
        dev[0].request("AP_SCAN 1")

def _test_scan_ap_scan_2_ap_mode(dev, apdev):
    if "OK" not in dev[0].request("AP_SCAN 2"):
        raise Exception("Failed to set AP_SCAN 2")

    id = dev[0].add_network()
    dev[0].set_network(id, "mode", "2")
    dev[0].set_network_quoted(id, "ssid", "wpas-ap-open")
    dev[0].set_network(id, "key_mgmt", "NONE")
    dev[0].set_network(id, "frequency", "2412")
    dev[0].set_network(id, "scan_freq", "2412")
    dev[0].set_network(id, "disabled", "0")
    dev[0].select_network(id)
    ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED"], timeout=5)
    if ev is None:
        raise Exception("AP failed to start")

    with fail_test(dev[0], 1, "wpa_driver_nl80211_scan"):
        if "OK" not in dev[0].request("SCAN freq=2412"):
            raise Exception("SCAN command failed unexpectedly")
        ev = dev[0].wait_event(["CTRL-EVENT-SCAN-FAILED",
                                "AP-DISABLED"], timeout=5)
        if ev is None:
            raise Exception("CTRL-EVENT-SCAN-FAILED not seen")
        if "AP-DISABLED" in ev:
            raise Exception("Unexpected AP-DISABLED event")
        if "retry=1" in ev:
            # Wait for the retry to scan happen
            ev = dev[0].wait_event(["CTRL-EVENT-SCAN-FAILED",
                                    "AP-DISABLED"], timeout=5)
            if ev is None:
                raise Exception("CTRL-EVENT-SCAN-FAILED not seen - retry")
            if "AP-DISABLED" in ev:
                raise Exception("Unexpected AP-DISABLED event - retry")

    dev[1].connect("wpas-ap-open", key_mgmt="NONE", scan_freq="2412")
    dev[1].request("DISCONNECT")
    dev[1].wait_disconnected()
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

def test_scan_bss_expiration_on_ssid_change(dev, apdev):
    """BSS entry expiration when AP changes SSID"""
    dev[0].flush_scan_cache()
    hapd = hostapd.add_ap(apdev[0], {"ssid": "test-scan"})
    bssid = apdev[0]['bssid']
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")

    hapd.request("DISABLE")
    hapd = hostapd.add_ap(apdev[0], {"ssid": "open"})
    if "OK" not in dev[0].request("BSS_EXPIRE_COUNT 3"):
        raise Exception("BSS_EXPIRE_COUNT failed")
    dev[0].scan(freq="2412")
    dev[0].scan(freq="2412")
    if "OK" not in dev[0].request("BSS_EXPIRE_COUNT 2"):
        raise Exception("BSS_EXPIRE_COUNT failed")
    res = dev[0].request("SCAN_RESULTS")
    if "test-scan" not in res:
        raise Exception("The first SSID not in scan results")
    if "open" not in res:
        raise Exception("The second SSID not in scan results")
    dev[0].connect("open", key_mgmt="NONE")

    dev[0].request("BSS_FLUSH 0")
    res = dev[0].request("SCAN_RESULTS")
    if "test-scan" in res:
        raise Exception("The BSS entry with the old SSID was not removed")
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

def test_scan_dfs(dev, apdev, params):
    """Scan on DFS channels"""
    try:
        _test_scan_dfs(dev, apdev, params)
    finally:
        clear_regdom_dev(dev)

def _test_scan_dfs(dev, apdev, params):
    subprocess.call(['iw', 'reg', 'set', 'US'])
    for i in range(2):
        for j in range(5):
            ev = dev[i].wait_event(["CTRL-EVENT-REGDOM-CHANGE"], timeout=5)
            if ev is None:
                raise Exception("No regdom change event")
            if "alpha2=US" in ev:
                break
        dev[i].dump_monitor()

    if "OK" not in dev[0].request("SCAN"):
        raise Exception("SCAN command failed")
    ev = dev[0].wait_event(["CTRL-EVENT-SCAN-RESULTS"])
    if ev is None:
        raise Exception("Scan did not complete")

    if "OK" not in dev[0].request("SCAN freq=2412,5180,5260,5500,5600,5745"):
        raise Exception("SCAN command failed")
    ev = dev[0].wait_event(["CTRL-EVENT-SCAN-RESULTS"])
    if ev is None:
        raise Exception("Scan did not complete")

    out = run_tshark(os.path.join(params['logdir'], "hwsim0.pcapng"),
                     "wlan.fc.type_subtype == 4", ["radiotap.channel.freq"])
    if out is not None:
        freq = out.splitlines()
        freq = [int(f) for f in freq]
        freq = list(set(freq))
        freq.sort()
        logger.info("Active scan seen on channels: " + str(freq))
        for f in freq:
            if (f >= 5260 and f <= 5320) or (f >= 5500 and f <= 5700):
                raise Exception("Active scan on DFS channel: %d" % f)
            if f in [2467, 2472]:
                raise Exception("Active scan on US-disallowed channel: %d" % f)

@remote_compatible
def test_scan_abort(dev, apdev):
    """Aborting a full scan"""
    dev[0].request("SCAN")
    ev = dev[0].wait_event(["CTRL-EVENT-SCAN-STARTED"])
    if ev is None:
        raise Exception("Scan did not start")
    if "OK" not in dev[0].request("ABORT_SCAN"):
        raise Exception("ABORT_SCAN command failed")
    ev = dev[0].wait_event(["CTRL-EVENT-SCAN-RESULTS"], timeout=2)
    if ev is None:
        raise Exception("Scan did not terminate")

@remote_compatible
def test_scan_abort_on_connect(dev, apdev):
    """Aborting a full scan on connection request"""
    hostapd.add_ap(apdev[0], {"ssid": "test-scan"})
    bssid = apdev[0]['bssid']

    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[0].dump_monitor()
    dev[0].request("SCAN")
    ev = dev[0].wait_event(["CTRL-EVENT-SCAN-STARTED"])
    if ev is None:
        raise Exception("Scan did not start")
    dev[0].connect("test-scan", key_mgmt="NONE")

@remote_compatible
def test_scan_ext(dev, apdev):
    """Custom IE in Probe Request frame"""
    hostapd.add_ap(apdev[0], {"ssid": "test-scan"})
    bssid = apdev[0]['bssid']

    try:
        if "OK" not in dev[0].request("VENDOR_ELEM_ADD 14 dd050011223300"):
            raise Exception("VENDOR_ELEM_ADD failed")
        check_scan(dev[0], "freq=2412 use_id=1")
    finally:
        dev[0].request("VENDOR_ELEM_REMOVE 14 *")

def test_scan_fail(dev, apdev):
    """Scan failures"""
    with fail_test(dev[0], 1, "wpa_driver_nl80211_scan"):
        dev[0].request("DISCONNECT")
        if "OK" not in dev[0].request("SCAN freq=2412"):
            raise Exception("SCAN failed")
        ev = dev[0].wait_event(["CTRL-EVENT-SCAN-FAILED"], timeout=5)
        if ev is None:
            raise Exception("Did not see scan failure event")
    dev[0].dump_monitor()

    for i in range(1, 5):
        with alloc_fail(dev[0], i,
                        "wpa_scan_clone_params;wpa_supplicant_trigger_scan"):
            if "OK" not in dev[0].request("SCAN ssid 112233 freq=2412"):
                raise Exception("SCAN failed")
            ev = dev[0].wait_event(["CTRL-EVENT-SCAN-FAILED"], timeout=5)
            if ev is None:
                raise Exception("Did not see scan failure event")
        dev[0].dump_monitor()

    with alloc_fail(dev[0], 1, "radio_add_work;wpa_supplicant_trigger_scan"):
        if "OK" not in dev[0].request("SCAN freq=2412"):
            raise Exception("SCAN failed")
        ev = dev[0].wait_event(["CTRL-EVENT-SCAN-FAILED"], timeout=5)
        if ev is None:
            raise Exception("Did not see scan failure event")
    dev[0].dump_monitor()

    try:
        if "OK" not in dev[0].request("SET filter_ssids 1"):
            raise Exception("SET failed")
        id = dev[0].connect("test-scan", key_mgmt="NONE", only_add_network=True)
        with alloc_fail(dev[0], 1, "wpa_supplicant_build_filter_ssids"):
            # While the filter list cannot be created due to memory allocation
            # failure, this scan is expected to be completed without SSID
            # filtering.
            if "OK" not in dev[0].request("SCAN freq=2412"):
                raise Exception("SCAN failed")
            ev = dev[0].wait_event(["CTRL-EVENT-SCAN-RESULTS"])
            if ev is None:
                raise Exception("Scan did not complete")
        dev[0].remove_network(id)
    finally:
        dev[0].request("SET filter_ssids 0")
    dev[0].dump_monitor()

    with alloc_fail(dev[0], 1, "nl80211_get_scan_results"):
        if "OK" not in dev[0].request("SCAN freq=2412"):
            raise Exception("SCAN failed")
        ev = dev[0].wait_event(["CTRL-EVENT-SCAN-STARTED"], timeout=5)
        if ev is None:
            raise Exception("Did not see scan started event")
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
    dev[0].dump_monitor()

    try:
        if "OK" not in dev[0].request("SET setband 2G"):
            raise Exception("SET setband failed")
        with alloc_fail(dev[0], 1, "=wpa_add_scan_freqs_list"):
            # While the frequency list cannot be created due to memory
            # allocation failure, this scan is expected to be completed without
            # frequency filtering.
            if "OK" not in dev[0].request("SCAN"):
                raise Exception("SCAN failed")
            wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
            dev[0].request("ABORT_SCAN")
            ev = dev[0].wait_event(["CTRL-EVENT-SCAN-RESULTS"])
            if ev is None:
                raise Exception("Scan did not complete")
    finally:
        dev[0].request("SET setband AUTO")
    dev[0].dump_monitor()

    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    wpas.interface_add("wlan5")
    wpas.request("SET preassoc_mac_addr 1")
    with fail_test(wpas, 1, "nl80211_set_mac_addr;wpas_trigger_scan_cb"):
        if "OK" not in wpas.request("SCAN freq=2412"):
            raise Exception("SCAN failed")
        ev = wpas.wait_event(["CTRL-EVENT-SCAN-FAILED"], timeout=5)
        if ev is None:
            raise Exception("Did not see scan failure event")
    wpas.request("SET preassoc_mac_addr 0")
    wpas.dump_monitor()

    hapd = hostapd.add_ap(apdev[0], {"ssid": "open"})
    with alloc_fail(dev[0], 1, "wpa_bss_add"):
        dev[0].flush_scan_cache()
        dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")

def test_scan_fail_type_only(dev, apdev):
    """Scan failures for TYPE=ONLY"""
    with fail_test(dev[0], 1, "wpa_driver_nl80211_scan"):
        dev[0].request("SCAN TYPE=ONLY freq=2417")
        ev = dev[0].wait_event(["CTRL-EVENT-SCAN-FAILED"], timeout=5)
        if ev is None:
            raise Exception("Scan trigger failure not reported")
    # Verify that scan_only_handler() does not get left set as the
    # wpa_s->scan_res_handler in failure case.
    hapd = hostapd.add_ap(apdev[0], {"ssid": "open"})
    dev[0].connect("open", key_mgmt="NONE", scan_freq="2412")

@remote_compatible
def test_scan_freq_list(dev, apdev):
    """Scan with SET freq_list and scan_cur_freq"""
    try:
        if "OK" not in dev[0].request("SET freq_list 2412 2417"):
            raise Exception("SET freq_list failed")
        check_scan(dev[0], "use_id=1")
    finally:
        dev[0].request("SET freq_list ")

    hapd = hostapd.add_ap(apdev[0], {"ssid": "test-scan"})
    dev[0].connect("test-scan", key_mgmt="NONE", scan_freq="2412")
    try:
        if "OK" not in dev[0].request("SET scan_cur_freq 1"):
            raise Exception("SET scan_cur_freq failed")
        check_scan(dev[0], "use_id=1")
    finally:
        dev[0].request("SET scan_cur_freq 0")
    dev[0].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected()

def test_scan_bss_limit(dev, apdev):
    """Scan and wpa_supplicant BSS entry limit"""
    try:
        _test_scan_bss_limit(dev, apdev)
    finally:
        dev[0].request("SET bss_max_count 200")
        pass

def _test_scan_bss_limit(dev, apdev):
    dev[0].flush_scan_cache()
    # Trigger 'Increasing the MAX BSS count to 2 because all BSSes are in use.
    # We should normally not get here!' message by limiting the maximum BSS
    # count to one so that the second AP would not fit in the BSS list and the
    # first AP cannot be removed from the list since it is still in use.
    dev[0].request("SET bss_max_count 1")
    hapd = hostapd.add_ap(apdev[0], {"ssid": "test-scan"})
    dev[0].connect("test-scan", key_mgmt="NONE", scan_freq="2412")
    hapd2 = hostapd.add_ap(apdev[1], {"ssid": "test-scan-2",
                                      "channel": "6"})
    dev[0].scan_for_bss(apdev[1]['bssid'], freq=2437, force_scan=True)

def run_scan(dev, bssid, exp_freq):
    for i in range(5):
        dev.request("SCAN freq=2412,2437,2462")
        ev = dev.wait_event(["CTRL-EVENT-SCAN-RESULTS"])
        if ev is None:
            raise Exception("Scan did not complete")
        bss = dev.get_bss(bssid)
        freq = int(bss['freq']) if bss else 0
        if freq == exp_freq:
            break
    if freq != exp_freq:
        raise Exception("BSS entry shows incorrect frequency: %d != %d" % (freq, exp_freq))

def test_scan_chan_switch(dev, apdev):
    """Scanning and AP changing channels"""

    # This test verifies that wpa_supplicant updates its local BSS table based
    # on the correct cfg80211 scan entry in cases where the cfg80211 BSS table
    # has multiple (one for each frequency) BSS entries for the same BSS.

    csa_supported(dev[0])
    hapd = hostapd.add_ap(apdev[0], {"ssid": "test-scan", "channel": "1"})
    csa_supported(hapd)
    bssid = hapd.own_addr()

    logger.info("AP channel switch while not connected")
    run_scan(dev[0], bssid, 2412)
    dev[0].dump_monitor()
    switch_channel(hapd, 1, 2437)
    run_scan(dev[0], bssid, 2437)
    dev[0].dump_monitor()
    switch_channel(hapd, 1, 2462)
    run_scan(dev[0], bssid, 2462)
    dev[0].dump_monitor()

    logger.info("AP channel switch while connected")
    dev[0].connect("test-scan", key_mgmt="NONE", scan_freq="2412 2437 2462")
    run_scan(dev[0], bssid, 2462)
    dev[0].dump_monitor()
    switch_channel(hapd, 2, 2437)
    wait_channel_switch(dev[0], 2437)
    dev[0].dump_monitor()
    run_scan(dev[0], bssid, 2437)
    dev[0].dump_monitor()
    switch_channel(hapd, 2, 2412)
    wait_channel_switch(dev[0], 2412)
    dev[0].dump_monitor()
    run_scan(dev[0], bssid, 2412)
    dev[0].dump_monitor()

@reset_ignore_old_scan_res
def test_scan_new_only(dev, apdev):
    """Scan and only_new=1 multiple times"""
    dev[0].flush_scan_cache()
    hapd = hostapd.add_ap(apdev[0], {"ssid": "test-scan"})
    dev[0].set("ignore_old_scan_res", "1")
    # Get the BSS added to cfg80211 BSS list
    bssid = hapd.own_addr()
    dev[0].scan_for_bss(bssid, freq=2412)
    bss = dev[0].get_bss(bssid)
    idx1 = bss['update_idx']
    dev[0].scan_for_bss(bssid, freq=2412, force_scan=True)
    dev[0].scan_for_bss(bssid, freq=2412, force_scan=True)
    bss = dev[0].get_bss(bssid)
    idx2 = bss['update_idx']
    if int(idx2) <= int(idx1):
        raise Exception("Scan result update_idx did not increase")
    # Disable AP to ensure there are no new scan results after this.
    hapd.disable()

    # Try to scan multiple times to verify that old scan results do not get
    # accepted as new.
    for i in range(10):
        dev[0].scan(freq=2412)
        bss = dev[0].get_bss(bssid)
        if bss:
            idx = bss['update_idx']
            if int(idx) > int(idx2):
                raise Exception("Unexpected update_idx increase")

def test_scan_flush(dev, apdev):
    """Ongoing scan and FLUSH"""
    dev[0].flush_scan_cache()
    hapd = hostapd.add_ap(apdev[0], {"ssid": "test-scan"})
    dev[0].dump_monitor()
    dev[0].request("SCAN TYPE=ONLY freq=2412-2472 passive=1")
    ev = dev[0].wait_event(["CTRL-EVENT-SCAN-STARTED"], timeout=10)
    if ev is None:
        raise Exception("Scan did not start")
    time.sleep(0.1)
    dev[0].request("FLUSH")
    ev = dev[0].wait_event(["CTRL-EVENT-SCAN-RESULTS",
                            "CTRL-EVENT-SCAN-FAILED",
                            "CTRL-EVENT-BSS-ADDED"], timeout=10)
    if ev is None:
        raise Exception("Scan did not complete")
    if "CTRL-EVENT-BSS-ADDED" in ev:
        raise Exception("Unexpected BSS entry addition after FLUSH")

def test_scan_ies(dev, apdev):
    """Scan and both Beacon and Probe Response frame IEs"""
    dev[0].flush_scan_cache()
    hapd = hostapd.add_ap(apdev[0], {"ssid": "test-scan",
                                     "beacon_int": "20"})
    bssid = hapd.own_addr()
    dev[0].dump_monitor()

    for i in range(10):
        dev[0].request("SCAN TYPE=ONLY freq=2412 passive=1")
        ev = dev[0].wait_event(["CTRL-EVENT-SCAN-RESULTS"], timeout=15)
        if ev is None:
            raise Exception("Scan did not complete")
        if dev[0].get_bss(bssid):
            break

    for i in range(10):
        dev[0].scan_for_bss(bssid, freq=2412, force_scan=True)
        bss = dev[0].get_bss(bssid)
        if 'beacon_ie' in bss:
            if bss['ie'] != bss['beacon_ie']:
                break

    if not bss or 'beacon_ie' not in bss:
        raise Exception("beacon_ie not present")
    ie = parse_ie(bss['ie'])
    logger.info("ie: " + str(list(ie.keys())))
    beacon_ie = parse_ie(bss['beacon_ie'])
    logger.info("beacon_ie: " + str(list(ie.keys())))
    if bss['ie'] == bss['beacon_ie']:
        raise Exception("Both ie and beacon_ie show same data")

def test_scan_parsing(dev, apdev):
    """Scan result parsing"""
    if "OK" not in dev[0].request("DRIVER_EVENT SCAN_RES START"):
        raise Exception("DRIVER_EVENT SCAN_RES START failed")

    if "FAIL" not in dev[0].request("DRIVER_EVENT SCAN_RES foo "):
        raise Exception("Invalid DRIVER_EVENT SCAN_RES accepted")

    tests = ["",
             "flags=ffffffff",
             "bssid=02:03:04:05:06:07",
             "freq=1234",
             "beacon_int=102",
             "caps=1234",
             "qual=10",
             "noise=10",
             "level=10",
             "tsf=1122334455667788",
             "age=123",
             "est_throughput=100",
             "snr=10",
             "parent_tsf=1122334455667788",
             "tsf_bssid=02:03:04:05:06:07",
             "ie=00",
             "beacon_ie=00",
             # Too long SSID
             "bssid=02:ff:00:00:00:01 ie=0033" + 33*'FF',
             # All parameters
             "flags=ffffffff bssid=02:ff:00:00:00:02 freq=1234 beacon_int=102 caps=1234 qual=10 noise=10 level=10 tsf=1122334455667788 age=123456 est_throughput=100 snr=10 parent_tsf=1122334455667788 tsf_bssid=02:03:04:05:06:07 ie=000474657374 beacon_ie=000474657374",
             # Beacon IEs truncated
             "bssid=02:ff:00:00:00:03 ie=0000 beacon_ie=0003ffff",
             # Probe Response IEs truncated
             "bssid=02:ff:00:00:00:04 ie=00000101 beacon_ie=0000",
             # DMG (invalid caps)
             "bssid=02:ff:00:00:00:05 freq=58320 ie=0003646d67",
             # DMG (IBSS)
             "bssid=02:ff:00:00:00:06 freq=60480 caps=0001 ie=0003646d67",
             # DMG (PBSS)
             "bssid=02:ff:00:00:00:07 freq=62640 caps=0002 ie=0003646d67",
             # DMG (AP)
             "bssid=02:ff:00:00:00:08 freq=64800 caps=0003 ie=0003646d67",
             # Test BSS for updates
             "bssid=02:ff:00:00:00:09 freq=2412 caps=0011 level=1 ie=0003757064010182",
             # Minimal BSS data
             "bssid=02:ff:00:00:00:00 ie=0000"]
    for t in tests:
        if "OK" not in dev[0].request("DRIVER_EVENT SCAN_RES BSS " + t):
            raise Exception("DRIVER_EVENT SCAN_RES BSS failed")

    if "OK" not in dev[0].request("DRIVER_EVENT SCAN_RES END"):
        raise Exception("DRIVER_EVENT SCAN_RES END failed")

    res = dev[0].request("SCAN_RESULTS")
    logger.info("SCAN_RESULTS:\n" + res)

    bss = []
    res = dev[0].request("BSS FIRST")
    if "FAIL" in res:
        raise Exception("BSS FIRST failed")
    while "\nbssid=" in res:
        logger.info("BSS output:\n" + res)
        bssid = None
        id = None
        for val in res.splitlines():
            if val.startswith("id="):
                id = val.split('=')[1]
            if val.startswith("bssid="):
                bssid = val.split('=')[1]
        if bssid is None or id is None:
            raise Exception("Missing id or bssid line")
        bss.append(bssid)
        res = dev[0].request("BSS NEXT-" + id)

    logger.info("Discovered BSSs: " + str(bss))
    invalid_bss = ["02:03:04:05:06:07", "02:ff:00:00:00:01"]
    valid_bss = ["02:ff:00:00:00:00", "02:ff:00:00:00:02",
                 "02:ff:00:00:00:03", "02:ff:00:00:00:04",
                 "02:ff:00:00:00:05", "02:ff:00:00:00:06",
                 "02:ff:00:00:00:07", "02:ff:00:00:00:08",
                 "02:ff:00:00:00:09"]
    for bssid in invalid_bss:
        if bssid in bss:
            raise Exception("Invalid BSS included: " + bssid)
    for bssid in valid_bss:
        if bssid not in bss:
            raise Exception("Valid BSS missing: " + bssid)

    logger.info("Update BSS parameters")
    if "OK" not in dev[0].request("DRIVER_EVENT SCAN_RES START"):
        raise Exception("DRIVER_EVENT SCAN_RES START failed")
    if "OK" not in dev[0].request("DRIVER_EVENT SCAN_RES BSS bssid=02:ff:00:00:00:09 freq=2412 caps=0002 level=2 ie=000375706401028204"):
        raise Exception("DRIVER_EVENT SCAN_RES BSS failed")
    if "OK" not in dev[0].request("DRIVER_EVENT SCAN_RES END"):
        raise Exception("DRIVER_EVENT SCAN_RES END failed")
    res = dev[0].request("BSS 02:ff:00:00:00:09")
    logger.info("Updated BSS:\n" + res)

def get_probe_req_ies(hapd):
    for i in range(10):
        msg = hapd.mgmt_rx()
        if msg is None:
            break
        if msg['subtype'] != 4:
            continue
        return parse_ie(binascii.hexlify(msg['payload']).decode())

    raise Exception("Probe Request not seen")

def test_scan_specific_bssid(dev, apdev):
    """Scan for a specific BSSID"""
    dev[0].flush_scan_cache()
    hapd = hostapd.add_ap(apdev[0], {"ssid": "test-scan",
                                     "beacon_int": "1000"})
    bssid = hapd.own_addr()

    time.sleep(0.1)
    dev[0].request("SCAN TYPE=ONLY freq=2412 bssid=02:ff:ff:ff:ff:ff")
    ev = dev[0].wait_event(["CTRL-EVENT-SCAN-RESULTS"], timeout=10)
    if ev is None:
        raise Exception("Scan did not complete")
    bss1 = dev[0].get_bss(bssid)

    for i in range(10):
        dev[0].request("SCAN TYPE=ONLY freq=2412 bssid=" + bssid)
        ev = dev[0].wait_event(["CTRL-EVENT-SCAN-RESULTS"], timeout=10)
        if ev is None:
            raise Exception("Scan did not complete")
        bss2 = dev[0].get_bss(bssid)
        if bss2:
            break

    if not bss2:
        raise Exception("Did not find BSS")
    if bss1 and 'beacon_ie' in bss1 and 'ie' in bss1 and bss1['beacon_ie'] != bss1['ie']:
        raise Exception("First scan for unknown BSSID returned unexpected response")
    if bss2 and 'beacon_ie' in bss2 and 'ie' in bss2 and bss2['beacon_ie'] == bss2['ie']:
        raise Exception("Second scan did find Probe Response frame")

    hapd.dump_monitor()
    hapd.set("ext_mgmt_frame_handling", "1")

    # With specific SSID in the Probe Request frame
    dev[0].request("SCAN TYPE=ONLY freq=2412 bssid=" + bssid)
    ev = dev[0].wait_event(["CTRL-EVENT-SCAN-RESULTS"], timeout=10)
    if ev is None:
        raise Exception("Scan did not complete")
    ie = get_probe_req_ies(hapd)
    if ie[0] != b"test-scan":
        raise Exception("Specific SSID not seen in Probe Request frame")

    hapd.dump_monitor()

    # Without specific SSID in the Probe Request frame
    dev[0].request("SCAN TYPE=ONLY freq=2412 wildcard_ssid=1 bssid=" + bssid)
    ev = dev[0].wait_event(["CTRL-EVENT-SCAN-RESULTS"], timeout=10)
    if ev is None:
        raise Exception("Scan did not complete")
    ie = get_probe_req_ies(hapd)
    if len(ie[0]) != 0:
        raise Exception("Wildcard SSID not seen in Probe Request frame")

def test_scan_probe_req_events(dev, apdev):
    """Probe Request frame RX events from hostapd"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": "open"})
    hapd2 = hostapd.Hostapd(apdev[0]['ifname'])
    if "OK" not in hapd2.mon.request("ATTACH probe_rx_events=1"):
        raise Exception("Failed to register for events")

    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412", force_scan=True)

    ev = hapd2.wait_event(["RX-PROBE-REQUEST"], timeout=5)
    if ev is None:
        raise Exception("RX-PROBE-REQUEST not reported")
    if "sa=" + dev[0].own_addr() not in ev:
        raise Exception("Unexpected event parameters: " + ev)

    ev = hapd.wait_event(["RX-PROBE-REQUEST"], timeout=0.1)
    if ev is not None:
        raise Exception("Unexpected RX-PROBE-REQUEST")

    if "OK" not in hapd2.mon.request("ATTACH probe_rx_events=0"):
        raise Exception("Failed to update event registration")

    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412", force_scan=True)
    ev = hapd2.wait_event(["RX-PROBE-REQUEST"], timeout=0.5)
    if ev is not None:
        raise Exception("Unexpected RX-PROBE-REQUEST")

    tests = ["probe_rx_events", "probe_rx_events=-1", "probe_rx_events=2"]
    for val in tests:
        if "FAIL" not in hapd2.mon.request("ATTACH " + val):
            raise Exception("Invalid ATTACH command accepted")

def elem_capab(capab):
    # Nontransmitted BSSID Capability element (83 = 0x53)
    return struct.pack('<BBH', 83, 2, capab)

def elem_ssid(ssid):
    # SSID element
    return struct.pack('BB', 0, len(ssid)) + ssid.encode()

def elem_bssid_index(index):
    # Multiple BSSID-index element (85 = 0x55)
    return struct.pack('BBB', 85, 1, index)

def elem_multibssid(profiles, max_bssid_indic):
    # TODO: add support for fragmenting over multiple Multiple BSSID elements
    if 1 + len(profiles) > 255:
        raise Exception("Too long Multiple BSSID element")
    elem = struct.pack('BBB', 71, 1 + len(profiles), max_bssid_indic) + profiles
    return binascii.hexlify(elem).decode()

def run_scans(dev, check):
    for i in range(2):
        dev.request("SCAN TYPE=ONLY freq=2412")
        ev = dev.wait_event(["CTRL-EVENT-SCAN-RESULTS"], timeout=10)
        if ev is None:
            raise Exception("Scan did not complete")

    # TODO: Check IEs
    for (bssid, ssid, capab) in check:
        bss = dev.get_bss(bssid)
        if bss is None:
            raise Exception("AP " + bssid + " missing from scan results")
        logger.info("AP " + bssid + ": " + str(bss))
        if bss['ssid'] != ssid:
            raise Exception("Unexpected AP " + bssid + " SSID")
        if int(bss['capabilities'], 16) != capab:
            raise Exception("Unexpected AP " + bssid + " capabilities")

def check_multibss_sta_capa(dev):
    res = dev.get_capability("multibss")
    if res is None or 'MULTIBSS-STA' not in res:
        raise HwsimSkip("Multi-BSS STA functionality not supported")

def test_scan_multi_bssid(dev, apdev):
    """Scan and Multiple BSSID element"""
    check_multibss_sta_capa(dev[0])
    dev[0].flush_scan_cache()

    params = {"ssid": "test-scan"}
    # Max BSSID Indicator 0 (max 1 BSSID) and no subelements
    params['vendor_elements'] = elem_multibssid(b'', 0)
    hostapd.add_ap(apdev[0], params)

    params = {"ssid": "test-scan"}
    elems = elem_capab(0x0401) + elem_ssid("1") + elem_bssid_index(1)
    profile1 = struct.pack('BB', 0, len(elems)) + elems
    params['vendor_elements'] = elem_multibssid(profile1, 1)
    hostapd.add_ap(apdev[1], params)

    bssid0 = apdev[0]['bssid']
    bssid1 = apdev[1]['bssid']
    check = [(bssid0, 'test-scan', 0x401),
             (bssid1, 'test-scan', 0x401),
             (bssid1[0:16] + '1', '1', 0x401)]
    run_scans(dev[0], check)

def test_scan_multi_bssid_2(dev, apdev):
    """Scan and Multiple BSSID element (2)"""
    check_multibss_sta_capa(dev[0])
    dev[0].flush_scan_cache()

    params = {"ssid": "transmitted"}

    # Duplicated entry for the transmitted BSS (not a normal use case)
    elems = elem_capab(1) + elem_ssid("transmitted") + elem_bssid_index(0)
    profile1 = struct.pack('BB', 0, len(elems)) + elems

    elems = elem_capab(1) + elem_ssid("nontransmitted") + elem_bssid_index(1)
    profile2 = struct.pack('BB', 0, len(elems)) + elems

    elems = elem_capab(1) + elem_ssid("nontransmitted_2") + elem_bssid_index(2)
    profile3 = struct.pack('BB', 0, len(elems)) + elems

    profiles = profile1 + profile2 + profile3
    params['vendor_elements'] = elem_multibssid(profiles, 4)
    hostapd.add_ap(apdev[0], params)

    bssid = apdev[0]['bssid']
    check = [(bssid, 'transmitted', 0x401),
             (bssid[0:16] + '1', 'nontransmitted', 0x1),
             (bssid[0:16] + '2', 'nontransmitted_2', 0x1)]
    run_scans(dev[0], check)

def test_scan_multi_bssid_3(dev, apdev):
    """Scan and Multiple BSSID element (3)"""
    check_multibss_sta_capa(dev[0])
    dev[0].flush_scan_cache()

    params = {"ssid": "transmitted"}

    # Duplicated nontransmitted BSS (not a normal use case)
    elems = elem_capab(1) + elem_ssid("nontransmitted") + elem_bssid_index(1)
    profile1 = struct.pack('BB', 0, len(elems)) + elems

    elems = elem_capab(1) + elem_ssid("nontransmitted") + elem_bssid_index(1)
    profile2 = struct.pack('BB', 0, len(elems)) + elems

    profiles = profile1 + profile2
    params['vendor_elements'] = elem_multibssid(profiles, 2)
    hostapd.add_ap(apdev[0], params)

    bssid = apdev[0]['bssid']
    check = [(bssid, 'transmitted', 0x401),
             (bssid[0:16] + '1', 'nontransmitted', 0x1)]
    run_scans(dev[0], check)

def test_scan_multi_bssid_4(dev, apdev):
    """Scan and Multiple BSSID element (3)"""
    check_multibss_sta_capa(dev[0])
    dev[0].flush_scan_cache()

    # Transmitted BSSID is not the first one in the block
    bssid = apdev[0]['bssid']
    hapd = None
    try:
        params = {"ssid": "transmitted",
                  "bssid": bssid[0:16] + '1'}

        elems = elem_capab(1) + elem_ssid("1") + elem_bssid_index(1)
        profile1 = struct.pack('BB', 0, len(elems)) + elems

        elems = elem_capab(1) + elem_ssid("2") + elem_bssid_index(2)
        profile2 = struct.pack('BB', 0, len(elems)) + elems

        elems = elem_capab(1) + elem_ssid("3") + elem_bssid_index(3)
        profile3 = struct.pack('BB', 0, len(elems)) + elems

        profiles = profile1 + profile2 + profile3
        params['vendor_elements'] = elem_multibssid(profiles, 2)
        hapd = hostapd.add_ap(apdev[0], params)

        check = [(bssid[0:16] + '1', 'transmitted', 0x401),
                 (bssid[0:16] + '2', '1', 0x1),
                 (bssid[0:16] + '3', '2', 0x1),
                 (bssid[0:16] + '0', '3', 0x1)]
        run_scans(dev[0], check)
    finally:
        if hapd:
            hapd.disable()
            hapd.set('bssid', bssid)
            hapd.enable()

def test_scan_multi_bssid_check_ie(dev, apdev):
    """Scan and check if nontransmitting BSS inherits IE from transmitting BSS"""
    check_multibss_sta_capa(dev[0])
    dev[0].flush_scan_cache()

    params = {"ssid": "transmitted"}

    # Duplicated entry for the transmitted BSS (not a normal use case)
    elems = elem_capab(1) + elem_ssid("transmitted") + elem_bssid_index(0)
    profile1 = struct.pack('BB', 0, len(elems)) + elems

    elems = elem_capab(1) + elem_ssid("nontransmitted") + elem_bssid_index(1)
    profile2 = struct.pack('BB', 0, len(elems)) + elems

    profiles = profile1 + profile2
    params['vendor_elements'] = elem_multibssid(profiles, 2)
    hostapd.add_ap(apdev[0], params)

    bssid = apdev[0]['bssid']

    for i in range(10):
        dev[0].request("SCAN TYPE=ONLY freq=2412 passive=1")
        ev = dev[0].wait_event(["CTRL-EVENT-SCAN-RESULTS"], timeout=15)
        if ev is None:
            raise Exception("Scan did not complete")
        if dev[0].get_bss(bssid):
            break

    for i in range(10):
        dev[0].scan_for_bss(bssid, freq=2412, force_scan=True)
        bss = dev[0].get_bss(bssid)
        if 'beacon_ie' in bss:
            break

    trans_bss = dev[0].get_bss(bssid)
    if trans_bss is None:
        raise Exception("AP " + bssid + " missing from scan results")

    if not trans_bss or 'beacon_ie' not in trans_bss:
        raise Exception("beacon_ie not present in trans_bss")

    beacon_ie = parse_ie(trans_bss['beacon_ie'])
    logger.info("trans_bss beacon_ie: " + str(list(beacon_ie.keys())))

    bssid = bssid[0:16] + '1'
    nontrans_bss1 = dev[0].get_bss(bssid)
    if nontrans_bss1 is None:
        raise Exception("AP " + bssid + " missing from scan results")

    if not trans_bss or 'beacon_ie' not in nontrans_bss1:
        raise Exception("beacon_ie not present in nontrans_bss1")

    nontx_beacon_ie = parse_ie(nontrans_bss1['beacon_ie'])
    logger.info("nontrans_bss1 beacon_ie: " + str(list(nontx_beacon_ie.keys())))

    if 71 in list(beacon_ie.keys()):
        ie_list = list(beacon_ie.keys())
        ie_list.remove(71)
        nontx_ie_list = list(nontx_beacon_ie.keys())
        try:
            nontx_ie_list.remove(85)
        except ValueError:
            pass
        if sorted(ie_list) != sorted(nontx_ie_list):
            raise Exception("check IE failed")

def elem_fms1():
    # this FMS IE has 1 FMS counter
    fms_counters = struct.pack('B', 0x39)
    fms_ids = struct.pack('B', 0x01)
    return struct.pack('BBB', 86, 3, 1) + fms_counters + fms_ids

def elem_fms2():
    # this FMS IE has 2 FMS counters
    fms_counters = struct.pack('BB', 0x29, 0x32)
    fms_ids = struct.pack('BB', 0x01, 0x02)
    return struct.pack('BBB', 86, 5, 2) + fms_counters + fms_ids

def test_scan_multi_bssid_fms(dev, apdev):
    """Non-transmitting BSS has different FMS IE from transmitting BSS"""
    check_multibss_sta_capa(dev[0])
    dev[0].flush_scan_cache()

    params = {"ssid": "transmitted"}

    # construct transmitting BSS Beacon with FMS IE
    elems = elem_capab(1) + elem_ssid("transmitted") + elem_bssid_index(0) + elem_fms1()
    profile1 = struct.pack('BB', 0, len(elems)) + elems

    elems = elem_capab(1) + elem_ssid("nontransmitted") + elem_bssid_index(1) + elem_fms2()
    profile2 = struct.pack('BB', 0, len(elems)) + elems

    profiles = profile1 + profile2
    params['vendor_elements'] = elem_multibssid(profiles, 2) + binascii.hexlify(elem_fms1()).decode()
    hostapd.add_ap(apdev[0], params)

    bssid = apdev[0]['bssid']

    for i in range(10):
        dev[0].request("SCAN TYPE=ONLY freq=2412 passive=1")
        ev = dev[0].wait_event(["CTRL-EVENT-SCAN-RESULTS"], timeout=15)
        if ev is None:
            raise Exception("Scan did not complete")
        if dev[0].get_bss(bssid):
            break

    for i in range(10):
        dev[0].scan_for_bss(bssid, freq=2412, force_scan=True)
        bss = dev[0].get_bss(bssid)
        if 'beacon_ie' in bss:
            break

    trans_bss = dev[0].get_bss(bssid)
    if trans_bss is None:
        raise Exception("AP " + bssid + " missing from scan results")

    if not trans_bss or 'beacon_ie' not in trans_bss:
        raise Exception("beacon_ie not present in trans_bss")

    beacon_ie = parse_ie(trans_bss['beacon_ie'])
    trans_bss_fms = beacon_ie[86]
    logger.info("trans_bss fms ie: " + binascii.hexlify(trans_bss_fms).decode())

    bssid = bssid[0:16] + '1'
    nontrans_bss1 = dev[0].get_bss(bssid)
    if nontrans_bss1 is None:
        raise Exception("AP " + bssid + " missing from scan results")

    if not nontrans_bss1 or 'beacon_ie' not in nontrans_bss1:
        raise Exception("beacon_ie not present in nontrans_bss1")

    nontrans_beacon_ie = parse_ie(nontrans_bss1['beacon_ie'])
    nontrans_bss_fms = nontrans_beacon_ie[86]
    logger.info("nontrans_bss fms ie: " + binascii.hexlify(nontrans_bss_fms).decode())

    if binascii.hexlify(trans_bss_fms) == binascii.hexlify(nontrans_bss_fms):
        raise Exception("Nontrans BSS has the same FMS IE as trans BSS")

def test_scan_multiple_mbssid_ie(dev, apdev):
    """Transmitting BSS has 2 MBSSID IE"""
    check_multibss_sta_capa(dev[0])
    dev[0].flush_scan_cache()

    bssid = apdev[0]['bssid']
    logger.info("bssid: " + bssid)
    hapd = None

    # construct 2 MBSSID IEs, each MBSSID IE contains 1 profile
    params = {"ssid": "transmitted",
              "bssid": bssid}

    elems = elem_capab(1) + elem_ssid("1") + elem_bssid_index(1)
    profile1 = struct.pack('BB', 0, len(elems)) + elems

    elems = elem_capab(2) + elem_ssid("2") + elem_bssid_index(2)
    profile2 = struct.pack('BB', 0, len(elems)) + elems

    params['vendor_elements'] = elem_multibssid(profile1, 2) + elem_multibssid(profile2, 2)
    hapd = hostapd.add_ap(apdev[0], params)

    check = [(bssid, 'transmitted', 0x401),
             (bssid[0:16] + '1', '1', 0x1),
             (bssid[0:16] + '2', '2', 0x2)]
    run_scans(dev[0], check)

def test_scan_mbssid_hidden_ssid(dev, apdev):
    """Non-transmitting BSS has hidden SSID"""
    check_multibss_sta_capa(dev[0])
    dev[0].flush_scan_cache()

    bssid = apdev[0]['bssid']
    logger.info("bssid: " + bssid)
    hapd = None

    # construct 2 MBSSID IEs, each MBSSID IE contains 1 profile
    params = {"ssid": "transmitted",
              "bssid": bssid}

    elems = elem_capab(1) + elem_ssid("") + elem_bssid_index(1)
    profile1 = struct.pack('BB', 0, len(elems)) + elems

    elems = elem_capab(2) + elem_ssid("2") + elem_bssid_index(2)
    profile2 = struct.pack('BB', 0, len(elems)) + elems

    profiles = profile1 + profile2
    params['vendor_elements'] = elem_multibssid(profiles, 2)
    hapd = hostapd.add_ap(apdev[0], params)

    check = [(bssid, 'transmitted', 0x401),
             (bssid[0:16] + '1', '', 0x1),
             (bssid[0:16] + '2', '2', 0x2)]
    run_scans(dev[0], check)

def test_connect_mbssid_open_1(dev, apdev):
    """Connect to transmitting and nontransmitting BSS in open mode"""
    check_multibss_sta_capa(dev[0])
    dev[0].flush_scan_cache()

    bssid = apdev[0]['bssid']
    params = {"ssid": "transmitted"}

    elems = elem_capab(1) + elem_ssid("nontransmitted") + elem_bssid_index(1)
    profile1 = struct.pack('BB', 0, len(elems)) + elems

    elems = elem_capab(1) + elem_ssid("nontransmitted_2") + elem_bssid_index(2)
    profile2 = struct.pack('BB', 0, len(elems)) + elems

    profiles = profile1 + profile2
    params['vendor_elements'] = elem_multibssid(profiles, 4)
    hostapd.add_ap(apdev[0], params)

    dev[0].connect("transmitted", key_mgmt="NONE", scan_freq="2412")
    dev[0].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected()
    dev[0].dump_monitor()

    dev[0].connect("nontransmitted", key_mgmt="NONE", scan_freq="2412",
                   wait_connect=False)
    ev = dev[0].wait_event(["SME: Trying to authenticate"], timeout=10)
    if ev is None:
        raise Exception("Connection attempt to nontransmitted BSS not started")
    if "02:00:00:00:03:01 (SSID='nontransmitted'" not in ev:
        raise Exception("Unexpected authentication target")
    # hostapd does not yet support Multiple-BSSID, so only verify that STA is
    # able to start connection attempt.
    dev[0].request("REMOVE_NETWORK all")
    dev[0].dump_monitor()

    dev[0].connect("nontransmitted_2", key_mgmt="NONE", scan_freq="2412",
                   wait_connect=False)
    ev = dev[0].wait_event(["SME: Trying to authenticate"], timeout=10)
    if ev is None:
        raise Exception("Connection attempt to nontransmitted BSS not started")
    if "02:00:00:00:03:02 (SSID='nontransmitted_2'" not in ev:
        raise Exception("Unexpected authentication target")
    # hostapd does not yet support Multiple-BSSID, so only verify that STA is
    # able to start connection attempt.
    dev[0].request("REMOVE_NETWORK all")
    dev[0].dump_monitor()

def test_scan_only_one(dev, apdev):
    """Test that scanning with a single active AP only returns that one"""
    dev[0].flush_scan_cache()
    hostapd.add_ap(apdev[0], {"ssid": "test-scan"})
    bssid = apdev[0]['bssid']

    check_scan(dev[0], "use_id=1", test_busy=True)
    dev[0].scan_for_bss(bssid, freq="2412")

    status, stdout = hostapd.cmd_execute(dev[0], ['iw', dev[0].ifname, 'scan', 'dump'])
    if status != 0:
        raise Exception("iw scan dump failed with code %d" % status)
    lines = stdout.split('\n')
    entries = len(list(filter(lambda x: x.startswith('BSS '), lines)))
    if entries != 1:
        raise Exception("expected to find 1 BSS entry, got %d" % entries)

def test_scan_ssid_list(dev, apdev):
    """Scan using SSID List element"""
    dev[0].flush_scan_cache()
    ssid = "test-ssid-list"
    hapd = hostapd.add_ap(apdev[0], {"ssid": ssid,
                                     "ignore_broadcast_ssid": "1"})
    bssid = apdev[0]['bssid']
    found = False
    try:
        payload = struct.pack('BB', 0, len(ssid)) + ssid.encode()
        ssid_list = struct.pack('BB', 84, len(payload)) + payload
        cmd = "VENDOR_ELEM_ADD 14 " + binascii.hexlify(ssid_list).decode()
        if "OK" not in dev[0].request(cmd):
            raise Exception("VENDOR_ELEM_ADD failed")
        for i in range(10):
            check_scan(dev[0], "freq=2412 use_id=1")
            if ssid in dev[0].request("SCAN_RESULTS"):
                found = True
                break
    finally:
        dev[0].request("VENDOR_ELEM_REMOVE 14 *")
        hapd.disable()
        dev[0].flush_scan_cache(freq=2432)
        dev[0].flush_scan_cache()

    if not found:
        raise Exception("AP not found in scan results")

def test_scan_short_ssid_list(dev, apdev):
    """Scan using Short SSID List element"""
    dev[0].flush_scan_cache()
    ssid = "test-short-ssid-list"
    hapd = hostapd.add_ap(apdev[0], {"ssid": ssid,
                                     "ignore_broadcast_ssid": "1"})
    bssid = apdev[0]['bssid']
    found = False
    try:
        payload = struct.pack('<L', binascii.crc32(ssid.encode()))
        ssid_list = struct.pack('BBB', 255, 1 + len(payload), 58) + payload
        cmd = "VENDOR_ELEM_ADD 14 " + binascii.hexlify(ssid_list).decode()
        if "OK" not in dev[0].request(cmd):
            raise Exception("VENDOR_ELEM_ADD failed")
        for i in range(10):
            check_scan(dev[0], "freq=2412 use_id=1")
            if ssid in dev[0].request("SCAN_RESULTS"):
                found = True
                break
    finally:
        dev[0].request("VENDOR_ELEM_REMOVE 14 *")
        hapd.disable()
        dev[0].flush_scan_cache(freq=2432)
        dev[0].flush_scan_cache()

    if not found:
        raise Exception("AP not found in scan results")
