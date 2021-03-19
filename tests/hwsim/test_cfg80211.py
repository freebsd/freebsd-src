# cfg80211 test cases
# Copyright (c) 2014, Jouni Malinen <j@w1.fi>
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

from remotehost import remote_compatible
import logging
logger = logging.getLogger()
import binascii
import os
import time

import hostapd
import hwsim_utils
from tshark import run_tshark
from nl80211 import *
from wpasupplicant import WpaSupplicant
from utils import *

def nl80211_command(dev, cmd, attr):
    res = dev.request("VENDOR ffffffff {} {}".format(nl80211_cmd[cmd],
                                                     binascii.hexlify(attr).decode()))
    if "FAIL" in res:
        raise Exception("nl80211 command failed")
    return binascii.unhexlify(res)

@remote_compatible
def test_cfg80211_disassociate(dev, apdev):
    """cfg80211 disassociation command"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": "open"})
    dev[0].connect("open", key_mgmt="NONE", scan_freq="2412")
    ev = hapd.wait_event(["AP-STA-CONNECTED"], timeout=5)
    if ev is None:
        raise Exception("No connection event received from hostapd")

    ifindex = int(dev[0].get_driver_status_field("ifindex"))
    attrs = build_nl80211_attr_u32('IFINDEX', ifindex)
    attrs += build_nl80211_attr_u16('REASON_CODE', 1)
    attrs += build_nl80211_attr_mac('MAC', apdev[0]['bssid'])
    nl80211_command(dev[0], 'DISASSOCIATE', attrs)

    ev = hapd.wait_event(["AP-STA-DISCONNECTED"], timeout=5)
    if ev is None:
        raise Exception("No disconnection event received from hostapd")

def nl80211_frame(dev, ifindex, frame, freq=None, duration=None, offchannel_tx_ok=False):
    attrs = build_nl80211_attr_u32('IFINDEX', ifindex)
    if freq is not None:
        attrs += build_nl80211_attr_u32('WIPHY_FREQ', freq)
    if duration is not None:
        attrs += build_nl80211_attr_u32('DURATION', duration)
    if offchannel_tx_ok:
        attrs += build_nl80211_attr_flag('OFFCHANNEL_TX_OK')
    attrs += build_nl80211_attr('FRAME', frame)
    return parse_nl80211_attrs(nl80211_command(dev, 'FRAME', attrs))

def nl80211_frame_wait_cancel(dev, ifindex, cookie):
    attrs = build_nl80211_attr_u32('IFINDEX', ifindex)
    attrs += build_nl80211_attr('COOKIE', cookie)
    return nl80211_command(dev, 'FRAME_WAIT_CANCEL', attrs)

def nl80211_remain_on_channel(dev, ifindex, freq, duration):
    attrs = build_nl80211_attr_u32('IFINDEX', ifindex)
    attrs += build_nl80211_attr_u32('WIPHY_FREQ', freq)
    attrs += build_nl80211_attr_u32('DURATION', duration)
    return nl80211_command(dev, 'REMAIN_ON_CHANNEL', attrs)

def test_cfg80211_tx_frame(dev, apdev, params):
    """cfg80211 offchannel TX frame command"""

    dev[0].p2p_start_go(freq='2412')
    go = WpaSupplicant(dev[0].group_ifname)
    frame = binascii.unhexlify("d0000000020000000100" + go.own_addr().replace(':', '') + "02000000010000000409506f9a090001dd5e506f9a0902020025080401001f0502006414060500585804510b0906000200000000000b1000585804510b0102030405060708090a0b0d1d000200000000000108000000000000000000101100084465766963652041110500585804510bdd190050f204104a0001101012000200011049000600372a000120")
    ifindex = int(go.get_driver_status_field("ifindex"))
    res = nl80211_frame(go, ifindex, frame, freq=2422, duration=500,
                        offchannel_tx_ok=True)
    time.sleep(0.1)

    # note: Uncommenting this seems to remove the incorrect channel issue
    #nl80211_frame_wait_cancel(dev[0], ifindex, res[nl80211_attr['COOKIE']])

    # note: this Action frame ends up getting sent incorrectly on 2422 MHz
    nl80211_frame(go, ifindex, frame, freq=2412)
    time.sleep(1.5)
    # note: also the Deauthenticate frame sent by the GO going down ends up
    # being transmitted incorrectly on 2422 MHz.

    del go

    out = run_tshark(os.path.join(params['logdir'], "hwsim0.pcapng"),
                     "wlan.fc.type_subtype == 13", ["radiotap.channel.freq"])
    if out is not None:
        freq = out.splitlines()
        if len(freq) != 2:
            raise Exception("Unexpected number of Action frames (%d)" % len(freq))
        if freq[0] != "2422":
            raise Exception("First Action frame on unexpected channel: %s MHz" % freq[0])
        if freq[1] != "2412":
            raise Exception("Second Action frame on unexpected channel: %s MHz" % freq[1])

@remote_compatible
def test_cfg80211_wep_key_idx_change(dev, apdev):
    """WEP Shared Key authentication and key index change without deauth"""
    check_wep_capa(dev[0])
    hapd = hostapd.add_ap(apdev[0],
                          {"ssid": "wep-shared-key",
                           "wep_key0": '"hello12345678"',
                           "wep_key1": '"other12345678"',
                           "auth_algs": "2"})
    id = dev[0].connect("wep-shared-key", key_mgmt="NONE", auth_alg="SHARED",
                        wep_key0='"hello12345678"',
                        wep_key1='"other12345678"',
                        wep_tx_keyidx="0",
                        scan_freq="2412")
    hwsim_utils.test_connectivity(dev[0], hapd)

    dev[0].set_network(id, "wep_tx_keyidx", "1")

    # clear cfg80211 auth state to allow new auth without deauth frame
    ifindex = int(dev[0].get_driver_status_field("ifindex"))
    attrs = build_nl80211_attr_u32('IFINDEX', ifindex)
    attrs += build_nl80211_attr_u16('REASON_CODE', 1)
    attrs += build_nl80211_attr_mac('MAC', apdev[0]['bssid'])
    attrs += build_nl80211_attr_flag('LOCAL_STATE_CHANGE')
    nl80211_command(dev[0], 'DEAUTHENTICATE', attrs)
    dev[0].wait_disconnected(timeout=5, error="Local-deauth timed out")

    # the previous command results in deauth event followed by auto-reconnect
    dev[0].wait_connected(timeout=10, error="Reassociation timed out")
    hwsim_utils.test_connectivity(dev[0], hapd)

@remote_compatible
def test_cfg80211_hostapd_ext_sta_remove(dev, apdev):
    """cfg80211 DEL_STATION issued externally to hostapd"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": "open"})
    id = dev[0].connect("open", key_mgmt="NONE", scan_freq="2412")

    ifindex = int(hapd.get_driver_status_field("ifindex"))
    attrs = build_nl80211_attr_u32('IFINDEX', ifindex)
    attrs += build_nl80211_attr_u16('REASON_CODE', 1)
    attrs += build_nl80211_attr_u8('MGMT_SUBTYPE', 12)
    attrs += build_nl80211_attr_mac('MAC', dev[0].own_addr())
    nl80211_command(hapd, 'DEL_STATION', attrs)

    # Currently, hostapd ignores the NL80211_CMD_DEL_STATION event if
    # drv->device_ap_sme == 0 (which is the case with mac80211_hwsim), so no
    # further action happens here. If that event were to be used to remove the
    # STA entry from hostapd even in device_ap_sme == 0 case, this test case
    # could be extended to cover additional operations.
