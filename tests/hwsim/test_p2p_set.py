# P2P_SET test cases
# Copyright (c) 2014, Jouni Malinen <j@w1.fi>
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

from remotehost import remote_compatible

def test_p2p_set(dev):
    """P2P_SET commands"""
    for cmd in ["",
                "foo bar",
                "noa 1",
                "noa 1,2",
                "noa 1,2,3",
                "noa -1,0,0",
                "noa 256,0,0",
                "noa 0,-1,0",
                "noa 0,0,-1",
                "noa 0,0,1",
                "noa 255,10,20",
                "ps 2",
                "oppps 1",
                "ctwindow 1",
                "conc_pref foo",
                "peer_filter foo",
                "client_apsd 0",
                "client_apsd 0,0",
                "client_apsd 0,0,0",
                "disc_int 1",
                "disc_int 1 2",
                "disc_int 2 1 10",
                "disc_int -1 0 10",
                "disc_int 0 -1 10",
                "ssid_postfix 123456789012345678901234"]:
        if "FAIL" not in dev[0].request("P2P_SET " + cmd):
            raise Exception("Invalid P2P_SET accepted: " + cmd)
    dev[0].request("P2P_SET ps 1")
    if "OK" not in dev[0].request("P2P_SET ps 0"):
        raise Exception("P2P_SET ps 0 failed unexpectedly")

def test_p2p_set_discoverability(dev):
    """P2P_SET discoverability"""
    addr0 = dev[0].p2p_dev_addr()
    addr1 = dev[1].p2p_dev_addr()

    dev[0].p2p_start_go(freq="2412")
    if "OK" not in dev[1].request("P2P_SET discoverability 0"):
        raise Exception("P2P_SET discoverability 0 failed")
    pin = dev[1].wps_read_pin()
    dev[0].p2p_go_authorize_client(pin)
    dev[1].p2p_connect_group(addr0, pin, timeout=20, social=True, freq="2412")

    if not dev[2].discover_peer(addr1, timeout=10):
        if not dev[2].discover_peer(addr1, timeout=10):
            if not dev[2].discover_peer(addr1, timeout=10):
                raise Exception("Could not discover group client")

    peer = dev[2].get_peer(addr1)
    if int(peer['dev_capab'], 16) & 0x02 != 0:
        raise Exception("Discoverability dev_capab reported: " + peer['dev_capab'])
    dev[2].p2p_stop_find()

    if "OK" not in dev[1].request("P2P_SET discoverability 1"):
        raise Exception("P2P_SET discoverability 1 failed")
    dev[1].dump_monitor()
    dev[1].group_request("REASSOCIATE")
    ev = dev[1].wait_group_event(["CTRL-EVENT-CONNECTED"], timeout=20)
    if ev is None:
        raise Exception("Connection timed out")

    dev[2].request("P2P_FLUSH")
    if not dev[2].discover_peer(addr1, timeout=10):
        if not dev[2].discover_peer(addr1, timeout=10):
            if not dev[2].discover_peer(addr1, timeout=10):
                raise Exception("Could not discover group client")

    peer = dev[2].get_peer(addr1)
    if int(peer['dev_capab'], 16) & 0x02 != 0x02:
        raise Exception("Discoverability dev_capab reported: " + peer['dev_capab'])
    dev[2].p2p_stop_find()

def test_p2p_set_managed(dev):
    """P2P_SET managed"""
    addr0 = dev[0].p2p_dev_addr()

    if "OK" not in dev[0].request("P2P_SET managed 1"):
        raise Exception("P2P_SET managed 1 failed")

    dev[0].p2p_listen()
    if not dev[1].discover_peer(addr0):
        raise Exception("Could not discover peer")
    peer = dev[1].get_peer(addr0)
    if int(peer['dev_capab'], 16) & 0x08 != 0x08:
        raise Exception("Managed dev_capab not reported: " + peer['dev_capab'])
    dev[1].p2p_stop_find()

    if "OK" not in dev[0].request("P2P_SET managed 0"):
        raise Exception("P2P_SET managed 0 failed")

    if not dev[2].discover_peer(addr0):
        raise Exception("Could not discover peer")
    peer = dev[2].get_peer(addr0)
    if int(peer['dev_capab'], 16) & 0x08 != 0:
        raise Exception("Managed dev_capab reported: " + peer['dev_capab'])
    dev[2].p2p_stop_find()
    dev[0].p2p_stop_find()

@remote_compatible
def test_p2p_set_ssid_postfix(dev):
    """P2P_SET ssid_postfix"""
    addr0 = dev[0].p2p_dev_addr()
    addr1 = dev[1].p2p_dev_addr()
    postfix = "12345678901234567890123"

    try:
        if "OK" not in dev[0].request("P2P_SET ssid_postfix " + postfix):
            raise Exception("P2P_SET ssid_postfix failed")
        dev[0].p2p_start_go(freq="2412")
        pin = dev[1].wps_read_pin()
        dev[0].p2p_go_authorize_client(pin)
        dev[1].p2p_connect_group(addr0, pin, timeout=20, social=True, freq="2412")
        if postfix not in dev[1].get_group_status_field("ssid"):
            raise Exception("SSID postfix missing from status")
        if postfix not in dev[1].group_request("SCAN_RESULTS"):
            raise Exception("SSID postfix missing from scan results")
    finally:
        dev[0].request("P2P_SET ssid_postfix ")
