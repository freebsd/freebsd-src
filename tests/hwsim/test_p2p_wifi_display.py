# Wi-Fi Display test cases
# Copyright (c) 2013, Jouni Malinen <j@w1.fi>
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

from remotehost import remote_compatible
import logging
logger = logging.getLogger()

import hwsim_utils
from p2p_utils import *

def test_wifi_display(dev):
    """Wi-Fi Display extensions to P2P"""
    wfd_devinfo = "00411c440028"
    dev[0].request("SET wifi_display 1")
    dev[0].request("WFD_SUBELEM_SET 0 0006" + wfd_devinfo)
    if wfd_devinfo not in dev[0].request("WFD_SUBELEM_GET 0"):
        raise Exception("Could not fetch back configured subelement")

    # Associated BSSID
    dev[0].request("WFD_SUBELEM_SET 1 0006020304050607")
    # Coupled Sink
    dev[0].request("WFD_SUBELEM_SET 6 000700000000000000")
    # Session Info
    dev[0].request("WFD_SUBELEM_SET 9 0000")
    # WFD Extended Capability
    dev[0].request("WFD_SUBELEM_SET 7 00020000")
    # WFD Content Protection
    prot = "0001" + "00"
    dev[0].request("WFD_SUBELEM_SET 5 " + prot)
    # WFD Video Formats
    video = "0015" + "010203040506070809101112131415161718192021"
    dev[0].request("WFD_SUBELEM_SET 3 " + video)
    # WFD 3D Video Formats
    video_3d = "0011" + "0102030405060708091011121314151617"
    dev[0].request("WFD_SUBELEM_SET 4 " + video_3d)
    # WFD Audio Formats
    audio = "000f" + "010203040506070809101112131415"
    dev[0].request("WFD_SUBELEM_SET 2 " + audio)

    elems = dev[0].request("WFD_SUBELEM_GET all")
    if wfd_devinfo not in elems:
        raise Exception("Could not fetch back configured subelements")

    wfd_devinfo2 = "00001c440028"
    dev[1].request("SET wifi_display 1")
    dev[1].request("WFD_SUBELEM_SET 0 0006" + wfd_devinfo2)
    if wfd_devinfo2 not in dev[1].request("WFD_SUBELEM_GET 0"):
        raise Exception("Could not fetch back configured subelement")

    dev[0].p2p_listen()
    if "FAIL" in dev[1].global_request("P2P_SERV_DISC_REQ " + dev[0].p2p_dev_addr() + " wifi-display [source][pri-sink] 2,3,4,5"):
        raise Exception("Setting SD request failed")
    dev[1].p2p_find(social=True)
    ev = dev[0].wait_global_event(["P2P-SERV-DISC-REQ"], timeout=10)
    if ev is None:
        raise Exception("Device discovery request not reported")
    ev = dev[1].wait_global_event(["P2P-DEVICE-FOUND"], timeout=5)
    if ev is None:
        raise Exception("Device discovery timed out")
    if "wfd_dev_info=0x" + wfd_devinfo not in ev:
        raise Exception("Wi-Fi Display Info not in P2P-DEVICE-FOUND event")
    if "new=1" not in ev:
        raise Exception("new=1 flag missing from P2P-DEVICE-FOUND event")
    ev = dev[1].wait_global_event(["P2P-SERV-DISC-RESP"], timeout=5)
    if ev is None:
        raise Exception("Service discovery timed out")
    if prot not in ev:
        raise Exception("WFD Content Protection missing from WSD response")
    if video not in ev:
        raise Exception("WFD Video Formats missing from WSD response")
    if video_3d not in ev:
        raise Exception("WFD 3D Video Formats missing from WSD response")
    if audio not in ev:
        raise Exception("WFD Audio Formats missing from WSD response")

    dev[1].dump_monitor()
    dev[0].request("WFD_SUBELEM_SET 0 0006" + wfd_devinfo2)
    ev = dev[1].wait_global_event(["P2P-DEVICE-FOUND"], timeout=15)
    if ev is None:
        raise Exception("Peer info update timed out")
    if "new=0" not in ev:
        raise Exception("new=0 flag missing from P2P-DEVICE-FOUND event")
    if "wfd_dev_info=0x" + wfd_devinfo2 not in ev:
        raise Exception("Wi-Fi Display Info not in P2P-DEVICE-FOUND event")
    dev[1].dump_monitor()
    dev[0].request("WFD_SUBELEM_SET 0 0006" + wfd_devinfo)
    ev = dev[1].wait_global_event(["P2P-DEVICE-FOUND"], timeout=15)
    if ev is None:
        raise Exception("Peer info update timed out")
    if "new=0" not in ev:
        raise Exception("new=0 flag missing from P2P-DEVICE-FOUND event")
    if "wfd_dev_info=0x" + wfd_devinfo not in ev:
        raise Exception("Wi-Fi Display Info not in P2P-DEVICE-FOUND event")

    pin = dev[0].wps_read_pin()
    dev[0].p2p_go_neg_auth(dev[1].p2p_dev_addr(), pin, 'display')
    res1 = dev[1].p2p_go_neg_init(dev[0].p2p_dev_addr(), pin, 'enter',
                                  timeout=20, go_intent=15, freq=2437)
    res2 = dev[0].p2p_go_neg_auth_result()

    bss = dev[0].get_bss("p2p_dev_addr=" + dev[1].p2p_dev_addr())
    if bss['bssid'] != dev[1].p2p_interface_addr():
        raise Exception("Unexpected BSSID in the BSS entry for the GO")
    if wfd_devinfo2 not in bss['wfd_subelems']:
        raise Exception("Could not see wfd_subelems in GO's BSS entry")
    peer = dev[0].get_peer(dev[1].p2p_dev_addr())
    if wfd_devinfo2 not in peer['wfd_subelems']:
        raise Exception("Could not see wfd_subelems in GO's peer entry")
    peer = dev[1].get_peer(dev[0].p2p_dev_addr())
    if wfd_devinfo not in peer['wfd_subelems']:
        raise Exception("Could not see wfd_subelems in client's peer entry")

    wfd_devinfo3 = "00001c440028"
    dev[2].request("SET wifi_display 1")
    dev[2].request("WFD_SUBELEM_SET 0 0006" + wfd_devinfo3)
    dev[2].p2p_find(social=True)
    ev = dev[2].wait_global_event(["P2P-DEVICE-FOUND"], timeout=5)
    if ev is None:
        raise Exception("Device discovery timed out")
    if dev[1].p2p_dev_addr() not in ev:
        ev = dev[2].wait_global_event(["P2P-DEVICE-FOUND"], timeout=5)
        if ev is None:
            raise Exception("Device discovery timed out")
        if dev[1].p2p_dev_addr() not in ev:
            raise Exception("Could not discover GO")
    if "wfd_dev_info=0x" + wfd_devinfo2 not in ev:
        raise Exception("Wi-Fi Display Info not in P2P-DEVICE-FOUND event")
    bss = dev[2].get_bss("p2p_dev_addr=" + dev[1].p2p_dev_addr())
    if bss['bssid'] != dev[1].p2p_interface_addr():
        raise Exception("Unexpected BSSID in the BSS entry for the GO")
    if wfd_devinfo2 not in bss['wfd_subelems']:
        raise Exception("Could not see wfd_subelems in GO's BSS entry")
    peer = dev[2].get_peer(dev[1].p2p_dev_addr())
    if wfd_devinfo2 not in peer['wfd_subelems']:
        raise Exception("Could not see wfd_subelems in GO's peer entry")
    dev[2].p2p_stop_find()

    if dev[0].request("WFD_SUBELEM_GET 2") != audio:
        raise Exception("Unexpected WFD_SUBELEM_GET 2 value")
    if dev[0].request("WFD_SUBELEM_GET 3") != video:
        raise Exception("Unexpected WFD_SUBELEM_GET 3 value")
    if dev[0].request("WFD_SUBELEM_GET 4") != video_3d:
        raise Exception("Unexpected WFD_SUBELEM_GET 42 value")
    if dev[0].request("WFD_SUBELEM_GET 5") != prot:
        raise Exception("Unexpected WFD_SUBELEM_GET 5 value")
    if "FAIL" not in dev[0].request("WFD_SUBELEM_SET "):
        raise Exception("Unexpected WFD_SUBELEM_SET success")
    if "FAIL" not in dev[0].request("WFD_SUBELEM_SET 6"):
        raise Exception("Unexpected WFD_SUBELEM_SET success")
    if "OK" not in dev[0].request("WFD_SUBELEM_SET 6 "):
        raise Exception("Unexpected WFD_SUBELEM_SET failure")
    if "FAIL" not in dev[0].request("WFD_SUBELEM_SET 6 0"):
        raise Exception("Unexpected WFD_SUBELEM_SET success")
    if "FAIL" not in dev[0].request("WFD_SUBELEM_SET 6 0q"):
        raise Exception("Unexpected WFD_SUBELEM_SET success")
    if dev[0].request("WFD_SUBELEM_GET 6") != "":
        raise Exception("Unexpected WFD_SUBELEM_GET 6 response")
    if dev[0].request("WFD_SUBELEM_GET 8") != "":
        raise Exception("Unexpected WFD_SUBELEM_GET 8 response")

    if dev[0].global_request("WFD_SUBELEM_GET 2") != audio:
        raise Exception("Unexpected WFD_SUBELEM_GET 2 value from global interface")
    if "OK" not in dev[0].global_request("WFD_SUBELEM_SET 1 0006020304050608"):
        raise Exception("WFD_SUBELEM_SET failed on global interface")
    if dev[0].request("WFD_SUBELEM_GET 1") != "0006020304050608":
        raise Exception("Unexpected WFD_SUBELEM_GET 1 value (per-interface)")

    elems = dev[0].request("WFD_SUBELEM_GET all")
    if "OK" not in dev[0].request("WFD_SUBELEM_SET all " + elems):
        raise Exception("WFD_SUBELEM_SET all failed")
    if dev[0].request("WFD_SUBELEM_GET all") != elems:
        raise Exception("Mismatch in WFS_SUBELEM_SET/GET all")
    test = "00000600411c440028"
    if "OK" not in dev[0].request("WFD_SUBELEM_SET all " + test):
        raise Exception("WFD_SUBELEM_SET all failed")
    if dev[0].request("WFD_SUBELEM_GET all") != test:
        raise Exception("Mismatch in WFS_SUBELEM_SET/GET all")

    if "FAIL" not in dev[0].request("WFD_SUBELEM_SET all qwerty"):
        raise Exception("Invalid WFD_SUBELEM_SET all succeeded")
    if "FAIL" not in dev[0].request("WFD_SUBELEM_SET all 11"):
        raise Exception("Invalid WFD_SUBELEM_SET all succeeded")
    dev[0].request("WFD_SUBELEM_SET all 112233445566")
    dev[0].request("WFD_SUBELEM_SET all ff0000fe0000fd00")

    if "FAIL" not in dev[0].request("WFD_SUBELEM_SET 300 112233"):
        raise Exception("Invalid WFD_SUBELEM_SET 300 succeeded")
    if "FAIL" not in dev[0].request("WFD_SUBELEM_SET -1 112233"):
        raise Exception("Invalid WFD_SUBELEM_SET -1 succeeded")
    if "FAIL" not in dev[0].request("WFD_SUBELEM_GET 300"):
        raise Exception("Invalid WFD_SUBELEM_GET 300 succeeded")
    if "FAIL" not in dev[0].request("WFD_SUBELEM_GET -1"):
        raise Exception("Invalid WFD_SUBELEM_GET -1 succeeded")

    dev[0].request("SET wifi_display 0")
    dev[1].request("SET wifi_display 0")
    dev[2].request("SET wifi_display 0")

def test_wifi_display_r2(dev):
    """Wi-Fi Display extensions to P2P with R2 subelems"""
    wfd_devinfo = "00411c440028"
    dev[0].request("SET wifi_display 1")
    dev[0].request("WFD_SUBELEM_SET 0 0006" + wfd_devinfo)

    # Associated BSSID
    dev[0].request("WFD_SUBELEM_SET 1 0006020304050607")
    # Coupled Sink
    dev[0].request("WFD_SUBELEM_SET 6 000700000000000000")
    # Session Info
    dev[0].request("WFD_SUBELEM_SET 9 0000")
    # WFD Extended Capability
    dev[0].request("WFD_SUBELEM_SET 7 00020000")
    # WFD Content Protection
    prot = "0001" + "00"
    dev[0].request("WFD_SUBELEM_SET 5 " + prot)
    # WFD Video Formats
    video = "0015" + "010203040506070809101112131415161718192021"
    dev[0].request("WFD_SUBELEM_SET 3 " + video)
    # WFD 3D Video Formats
    video_3d = "0011" + "0102030405060708091011121314151617"
    dev[0].request("WFD_SUBELEM_SET 4 " + video_3d)
    # WFD Audio Formats
    audio = "000f" + "010203040506070809101112131415"
    dev[0].request("WFD_SUBELEM_SET 2 " + audio)
    # MAC Info
    mac_info = "0006" + "112233445566"
    dev[0].request("WFD_SUBELEM_SET 10 " + mac_info)
    # R2 Device Info
    r2_dev_info = "0006" + "aabbccddeeff"
    dev[0].request("WFD_SUBELEM_SET 11 " + r2_dev_info)

    elems = dev[0].request("WFD_SUBELEM_GET all")
    if wfd_devinfo not in elems:
        raise Exception("Could not fetch back configured subelements")

    wfd_devinfo2 = "00001c440028"
    dev[1].request("SET wifi_display 1")
    dev[1].request("WFD_SUBELEM_SET 0 0006" + wfd_devinfo2)
    if wfd_devinfo2 not in dev[1].request("WFD_SUBELEM_GET 0"):
        raise Exception("Could not fetch back configured subelement")

    dev[0].p2p_listen()
    dev[1].p2p_find(social=True)
    ev = dev[1].wait_global_event(["P2P-DEVICE-FOUND"], timeout=5)
    if ev is None:
        raise Exception("Device discovery timed out")
    if "wfd_dev_info=0x" + wfd_devinfo not in ev:
        raise Exception("Wi-Fi Display Info not in P2P-DEVICE-FOUND event")
    if "new=1" not in ev:
        raise Exception("new=1 flag missing from P2P-DEVICE-FOUND event")

    pin = dev[0].wps_read_pin()
    dev[0].p2p_go_neg_auth(dev[1].p2p_dev_addr(), pin, 'display')
    res1 = dev[1].p2p_go_neg_init(dev[0].p2p_dev_addr(), pin, 'enter',
                                  timeout=20, go_intent=15, freq=2437)
    res2 = dev[0].p2p_go_neg_auth_result()

    bss = dev[0].get_bss("p2p_dev_addr=" + dev[1].p2p_dev_addr())
    if bss['bssid'] != dev[1].p2p_interface_addr():
        raise Exception("Unexpected BSSID in the BSS entry for the GO")
    if wfd_devinfo2 not in bss['wfd_subelems']:
        raise Exception("Could not see wfd_subelems in GO's BSS entry")
    peer = dev[0].get_peer(dev[1].p2p_dev_addr())
    if wfd_devinfo2 not in peer['wfd_subelems']:
        raise Exception("Could not see wfd_subelems in GO's peer entry")
    peer = dev[1].get_peer(dev[0].p2p_dev_addr())
    if wfd_devinfo not in peer['wfd_subelems']:
        raise Exception("Could not see wfd_subelems in client's peer entry")
    if r2_dev_info not in peer['wfd_subelems']:
        raise Exception("Could not see r2_dev_info in client's peer entry")

    elems = dev[0].request("WFD_SUBELEM_GET all")
    if "OK" not in dev[0].request("WFD_SUBELEM_SET all " + elems):
        raise Exception("WFD_SUBELEM_SET all failed")
    if dev[0].request("WFD_SUBELEM_GET all") != elems:
        raise Exception("Mismatch in WFS_SUBELEM_SET/GET all")
    test = "00000600411c440028"
    if "OK" not in dev[0].request("WFD_SUBELEM_SET all " + test):
        raise Exception("WFD_SUBELEM_SET all failed")
    if dev[0].request("WFD_SUBELEM_GET all") != test:
        raise Exception("Mismatch in WFS_SUBELEM_SET/GET all")

    dev[0].request("SET wifi_display 0")
    dev[1].request("SET wifi_display 0")
    dev[2].request("SET wifi_display 0")

def enable_wifi_display(dev):
    dev.request("SET wifi_display 1")
    dev.request("WFD_SUBELEM_SET 0 000600411c440028")

def test_wifi_display_go_invite(dev):
    """P2P GO with Wi-Fi Display inviting a client to join"""
    addr0 = dev[0].p2p_dev_addr()
    addr1 = dev[1].p2p_dev_addr()

    try:
        enable_wifi_display(dev[0])
        enable_wifi_display(dev[1])
        enable_wifi_display(dev[2])

        dev[1].p2p_listen()
        if not dev[0].discover_peer(addr1, social=True):
            raise Exception("Peer " + addr1 + " not found")
        dev[0].p2p_listen()
        if not dev[1].discover_peer(addr0, social=True):
            raise Exception("Peer " + addr0 + " not found")
        dev[1].p2p_listen()

        logger.info("Authorize invitation")
        pin = dev[1].wps_read_pin()
        dev[1].global_request("P2P_CONNECT " + addr0 + " " + pin + " join auth")

        dev[0].p2p_start_go(freq=2412)

        # Add test client to the group
        connect_cli(dev[0], dev[2], social=True, freq=2412)

        logger.info("Invite peer to join the group")
        dev[0].p2p_go_authorize_client(pin)
        dev[0].global_request("P2P_INVITE group=" + dev[0].group_ifname + " peer=" + addr1)
        ev = dev[1].wait_global_event(["P2P-INVITATION-RECEIVED",
                                       "P2P-GROUP-STARTED"], timeout=20)
        if ev is None:
            raise Exception("Timeout on invitation on peer")
        if "P2P-INVITATION-RECEIVED" in ev:
            raise Exception("Unexpected request to accept pre-authorized invitation")

        dev[0].remove_group()
        dev[1].wait_go_ending_session()
        dev[2].wait_go_ending_session()

    finally:
        dev[0].request("SET wifi_display 0")
        dev[1].request("SET wifi_display 0")
        dev[2].request("SET wifi_display 0")

def test_wifi_display_persistent_group(dev):
    """P2P persistent group formation and re-invocation with Wi-Fi Display enabled"""
    try:
        enable_wifi_display(dev[0])
        enable_wifi_display(dev[1])
        enable_wifi_display(dev[2])

        form(dev[0], dev[1])
        peer = dev[1].get_peer(dev[0].p2p_dev_addr())
        listen_freq = peer['listen_freq']
        invite_from_cli(dev[0], dev[1])
        invite_from_go(dev[0], dev[1])

        dev[0].dump_monitor()
        dev[1].dump_monitor()
        networks = dev[0].list_networks(p2p=True)
        if len(networks) != 1:
            raise Exception("Unexpected number of networks")
        if "[P2P-PERSISTENT]" not in networks[0]['flags']:
            raise Exception("Not the persistent group data")
        if "OK" not in dev[0].global_request("P2P_GROUP_ADD persistent=" + networks[0]['id'] + " freq=" + listen_freq):
            raise Exception("Could not start GO")
        ev = dev[0].wait_global_event(["P2P-GROUP-STARTED"], timeout=2)
        if ev is None:
            raise Exception("GO start up timed out")
        dev[0].group_form_result(ev)

        connect_cli(dev[0], dev[2], social=True, freq=listen_freq)
        dev[0].dump_monitor()
        dev[1].dump_monitor()
        invite(dev[1], dev[0])
        ev = dev[1].wait_global_event(["P2P-GROUP-STARTED"], timeout=30)
        if ev is None:
            raise Exception("Timeout on group re-invocation (on client)")
        dev[1].group_form_result(ev)

        ev = dev[0].wait_global_event(["P2P-GROUP-STARTED"], timeout=0.1)
        if ev is not None:
            raise Exception("Unexpected P2P-GROUP-START on GO")
        hwsim_utils.test_connectivity_p2p(dev[0], dev[1])

    finally:
        dev[0].request("SET wifi_display 0")
        dev[1].request("SET wifi_display 0")
        dev[2].request("SET wifi_display 0")

@remote_compatible
def test_wifi_display_invalid_subelem(dev):
    """Wi-Fi Display and invalid subelement parsing"""
    addr1 = dev[1].p2p_dev_addr()

    try:
        enable_wifi_display(dev[0])
        enable_wifi_display(dev[1])
        dev[1].request("WFD_SUBELEM_SET 0 ffff00411c440028")

        dev[1].p2p_listen()
        dev[0].p2p_find(social=True)
        ev = dev[0].wait_global_event(["P2P-DEVICE-FOUND"], timeout=10)
        if ev is None:
            raise Exception("Device discovery timed out")
        if "wfd_dev_info=" in ev:
            raise Exception("Invalid WFD subelement was shown")

    finally:
        dev[0].request("SET wifi_display 0")
        dev[1].request("SET wifi_display 0")

def test_wifi_display_parsing(dev):
    """Wi-Fi Display extensions to P2P and special parsing cases"""
    try:
        _test_wifi_display_parsing(dev)
    finally:
        dev[1].request("VENDOR_ELEM_REMOVE 11 *")
        dev[0].request("SET wifi_display 0")

def _test_wifi_display_parsing(dev):
    wfd_devinfo = "00411c440028"
    dev[0].request("SET wifi_display 1")
    dev[0].request("WFD_SUBELEM_SET 0 0006" + wfd_devinfo)
    dev[0].p2p_start_go(freq=2412)

    # P2P Client with invalid WFD IE
    if "OK" not in dev[1].request("VENDOR_ELEM_ADD 11 dd10506f9a0a000000010000060000ffffff"):
        raise Exception("VENDOR_ELEM_ADD failed")

    pin = dev[1].wps_read_pin()
    dev[0].p2p_go_authorize_client(pin)
    dev[1].p2p_connect_group(dev[0].p2p_dev_addr(), pin, timeout=60,
                             social=True, freq=2412)
    bssid = dev[0].get_group_status_field('bssid')
    dev[2].scan_for_bss(bssid, freq=2412, force_scan=True)
    bss = dev[2].get_bss(bssid)
    if bss['wfd_subelems'] != "000006" + wfd_devinfo:
        raise Exception("Unexpected WFD elements in scan results: " + bss['wfd_subelems'])

    # P2P Client without WFD IE
    pin = dev[2].wps_read_pin()
    dev[0].p2p_go_authorize_client(pin)
    dev[2].p2p_connect_group(dev[0].p2p_dev_addr(), pin, timeout=60,
                             social=True, freq=2412)
    dev[2].remove_group()

    dev[0].remove_group()
    dev[1].wait_go_ending_session()

def test_wifi_display_disable(dev):
    """Peer disabling Wi-Fi Display advertisement"""
    try:
        enable_wifi_display(dev[1])
        dev[1].p2p_listen()
        dev[0].p2p_find(social=True)
        ev = dev[0].wait_global_event(["P2P-DEVICE-FOUND"], timeout=15)
        if ev is None:
            raise Exception("Peer not found")
        if "wfd_dev_info" not in ev:
            raise Exception("Missing wfd_dev_info")

        dev[1].request("SET wifi_display 0")

        ev = dev[0].wait_global_event(["P2P-DEVICE-FOUND"], timeout=10)
        if ev is None:
            raise Exception("Peer update not indicated")
        if "new=0" not in ev:
            raise Exception("Incorrect update event: " + ev)
        if "wfd_dev_info" in ev:
            raise Exception("Unexpected wfd_dev_info")

        ev = dev[0].wait_global_event(["P2P-DEVICE-FOUND"], timeout=0.75)
        if ev is not None:
            raise Exception("Unexpected peer found event: " + ev)
        dev[0].p2p_stop_find()
        dev[1].p2p_stop_find()

    finally:
        dev[1].request("SET wifi_display 0")
