# Test a few kernel bugs and functionality
# Copyright (c) 2016, Intel Deutschland GmbH
#
# Author: Johannes Berg <johannes.berg@intel.com>
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

import hostapd
import binascii
import os
import struct
from test_wnm import expect_ack
from tshark import run_tshark

def _test_kernel_bss_leak(dev, apdev, deauth):
    ssid = "test-bss-leak"
    passphrase = 'qwertyuiop'
    params = hostapd.wpa2_params(ssid=ssid, passphrase=passphrase)
    hapd = hostapd.add_ap(apdev[0], params)
    hapd.set("ext_mgmt_frame_handling", "1")
    dev[0].connect(ssid, psk=passphrase, scan_freq="2412", wait_connect=False)
    while True:
        pkt = hapd.mgmt_rx()
        if not pkt:
            raise Exception("MGMT RX wait timed out for auth frame")
        if pkt['fc'] & 0xc:
            continue
        if pkt['subtype'] == 0: # assoc request
            if deauth:
                # return a deauth immediately
                hapd.mgmt_tx({
                    'fc': 0xc0,
                    'sa': pkt['da'],
                    'da': pkt['sa'],
                    'bssid': pkt['bssid'],
                    'payload': b'\x01\x00',
                })
            break
        else:
            hapd.request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=%s" % (
                         binascii.hexlify(pkt['frame']).decode(), ))
    hapd.set("ext_mgmt_frame_handling", "0")

    hapd.request("STOP_AP")

    dev[0].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected()

    dev[0].flush_scan_cache(freq=5180)
    res = dev[0].request("SCAN_RESULTS")
    if len(res.splitlines()) > 1:
        raise Exception("BSS entry should no longer be around")

def test_kernel_bss_leak_deauth(dev, apdev):
    """cfg80211/mac80211 BSS leak on deauthentication"""
    return _test_kernel_bss_leak(dev, apdev, deauth=True)

def test_kernel_bss_leak_timeout(dev, apdev):
    """cfg80211/mac80211 BSS leak on timeout"""
    return _test_kernel_bss_leak(dev, apdev, deauth=False)

MGMT_SUBTYPE_ACTION = 13

def expect_no_ack(hapd):
    ev = hapd.wait_event(["MGMT-TX-STATUS"], timeout=5)
    if ev is None:
        raise Exception("Missing TX status")
    if "ok=0" not in ev:
        raise Exception("Action frame unexpectedly acknowledged")

def test_kernel_unknown_action_frame_rejection_sta(dev, apdev, params):
    """mac80211 and unknown Action frame rejection in STA mode"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": "unknown-action"})
    dev[0].connect("unknown-action", key_mgmt="NONE", scan_freq="2412")
    bssid = hapd.own_addr()
    addr = dev[0].own_addr()

    hapd.set("ext_mgmt_frame_handling", "1")

    # Unicast Action frame with unknown category (response expected)
    msg = {}
    msg['fc'] = MGMT_SUBTYPE_ACTION << 4
    msg['da'] = addr
    msg['sa'] = bssid
    msg['bssid'] = bssid
    msg['payload'] = struct.pack("<BB", 0x70, 0)
    hapd.mgmt_tx(msg)
    expect_ack(hapd)

    # Note: mac80211 does not allow group-addressed Action frames in unknown
    # categories to be transmitted in AP mode, so for now, these steps are
    # commented out.

    # Multicast Action frame with unknown category (no response expected)
    #msg['da'] = "01:ff:ff:ff:ff:ff"
    #msg['payload'] = struct.pack("<BB", 0x71, 1)
    #hapd.mgmt_tx(msg)
    #expect_no_ack(hapd)

    # Broadcast Action frame with unknown category (no response expected)
    #msg['da'] = "ff:ff:ff:ff:ff:ff"
    #msg['payload'] = struct.pack("<BB", 0x72, 2)
    #hapd.mgmt_tx(msg)
    #expect_no_ack(hapd)

    # Unicast Action frame with error indication category (no response expected)
    msg['da'] = addr
    msg['payload'] = struct.pack("<BB", 0xf3, 3)
    hapd.mgmt_tx(msg)
    expect_ack(hapd)

    # Unicast Action frame with unknown category (response expected)
    msg['da'] = addr
    msg['payload'] = struct.pack("<BB", 0x74, 4)
    hapd.mgmt_tx(msg)
    expect_ack(hapd)

    out = run_tshark(os.path.join(params['logdir'], "hwsim0.pcapng"),
                     "wlan.sa == %s && wlan.fc.type_subtype == 0x0d" % addr,
                     display=["wlan_mgt.fixed.category_code"])
    res = out.splitlines()
    categ = [int(x) for x in res]

    if 0xf2 in categ or 0xf3 in categ:
        raise Exception("Unexpected Action frame rejection: " + str(categ))
    if 0xf0 not in categ or 0xf4 not in categ:
        raise Exception("Action frame rejection missing: " + str(categ))
