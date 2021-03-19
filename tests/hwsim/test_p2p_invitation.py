# P2P invitation test cases
# Copyright (c) 2013, Jouni Malinen <j@w1.fi>
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

from remotehost import remote_compatible
import logging
logger = logging.getLogger()

import hwsim_utils

@remote_compatible
def test_p2p_go_invite(dev):
    """P2P GO inviting a client to join"""
    addr0 = dev[0].p2p_dev_addr()
    addr1 = dev[1].p2p_dev_addr()

    logger.info("Generate BSS table entry for old group")
    # this adds more coverage to testing by forcing the GO to be found with an
    # older entry in the BSS table and with that entry having a different
    # operating channel.
    dev[0].p2p_start_go(freq=2422)
    dev[1].scan()
    dev[0].remove_group()

    logger.info("Discover peer")
    dev[1].p2p_listen()
    if not dev[0].discover_peer(addr1, social=True):
        raise Exception("Peer " + addr1 + " not found")

    logger.info("Start GO on non-social channel")
    res = dev[0].p2p_start_go(freq=2417)
    logger.debug("res: " + str(res))

    logger.info("Invite peer to join the group")
    dev[0].global_request("P2P_INVITE group=" + dev[0].group_ifname + " peer=" + addr1)
    ev = dev[1].wait_global_event(["P2P-INVITATION-RECEIVED"], timeout=10)
    if ev is None:
        raise Exception("Timeout on invitation on peer")
    ev = dev[0].wait_global_event(["P2P-INVITATION-RESULT"], timeout=10)
    if ev is None:
        raise Exception("Timeout on invitation on GO")
    if "status=1" not in ev:
        raise Exception("Unexpected invitation result")

    logger.info("Join the group")
    pin = dev[1].wps_read_pin()
    dev[0].p2p_go_authorize_client(pin)
    dev[1].p2p_connect_group(addr0, pin, timeout=60)
    logger.info("Client connected")
    hwsim_utils.test_connectivity_p2p(dev[0], dev[1])

    logger.info("Terminate group")
    dev[0].remove_group()
    dev[1].wait_go_ending_session()

@remote_compatible
def test_p2p_go_invite_auth(dev):
    """P2P GO inviting a client to join (authorized invitation)"""
    addr0 = dev[0].p2p_dev_addr()
    addr1 = dev[1].p2p_dev_addr()

    logger.info("Generate BSS table entry for old group")
    # this adds more coverage to testing by forcing the GO to be found with an
    # older entry in the BSS table and with that entry having a different
    # operating channel.
    dev[0].p2p_start_go(freq=2432)
    dev[1].scan()
    dev[0].remove_group()
    dev[0].dump_monitor()
    dev[1].dump_monitor()

    logger.info("Discover peer")
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

    logger.info("Start GO on non-social channel")
    res = dev[0].p2p_start_go(freq=2427)
    logger.debug("res: " + str(res))

    logger.info("Invite peer to join the group")
    dev[0].p2p_go_authorize_client(pin)
    dev[0].global_request("P2P_INVITE group=" + dev[0].group_ifname + " peer=" + addr1)
    ev = dev[1].wait_global_event(["P2P-INVITATION-RECEIVED",
                                   "P2P-GROUP-STARTED"], timeout=20)
    if ev is None:
        raise Exception("Timeout on invitation on peer")
    if "P2P-INVITATION-RECEIVED" in ev:
        raise Exception("Unexpected request to accept pre-authorized invitaton")
    dev[1].group_form_result(ev)
    dev[0].dump_monitor()

    logger.info("Client connected")
    hwsim_utils.test_connectivity_p2p(dev[0], dev[1])

    logger.info("Terminate group")
    dev[0].remove_group()
    dev[1].wait_go_ending_session()

@remote_compatible
def test_p2p_go_invite_unknown(dev):
    """P2P GO inviting a client that has not discovered the GO"""
    try:
        addr0 = dev[0].p2p_dev_addr()
        addr1 = dev[1].p2p_dev_addr()

        dev[1].p2p_listen()
        if not dev[0].discover_peer(addr1, social=True):
            raise Exception("Peer " + addr1 + " not found")
        dev[1].global_request("P2P_FLUSH")
        dev[1].p2p_listen()

        dev[0].p2p_start_go(freq=2412)

        logger.info("Invite peer to join the group")
        # Prevent peer entry from being added for testing coverage
        if "OK" not in dev[1].global_request("P2P_SET peer_filter 00:11:22:33:44:55"):
            raise Exception("Failed to set peer_filter")
        dev[0].p2p_go_authorize_client("12345670")
        dev[0].global_request("P2P_INVITE group=" + dev[0].group_ifname + " peer=" + addr1)
        ev = dev[1].wait_global_event(["P2P-INVITATION-RECEIVED"], timeout=15)
        if ev is None:
            raise Exception("Invitation Request not received")
        ev = dev[0].wait_global_event(["P2P-INVITATION-RESULT"], timeout=15)
        if ev is None:
            raise Exception("Invitation Response not received")
        if "status=1" not in ev:
            raise Exception("Unexpected invitation result: " + ev)
    finally:
        dev[1].global_request("P2P_SET peer_filter 00:00:00:00:00:00")

def test_p2p_cli_invite(dev):
    """P2P Client inviting a device to join"""
    addr0 = dev[0].p2p_dev_addr()
    addr1 = dev[1].p2p_dev_addr()
    addr2 = dev[2].p2p_dev_addr()

    dev[0].p2p_start_go(freq=2412)
    pin = dev[1].wps_read_pin()
    dev[0].p2p_go_authorize_client(pin)
    dev[1].p2p_connect_group(addr0, pin, timeout=60)

    dev[2].p2p_listen()
    if not dev[1].discover_peer(addr2, social=True):
        raise Exception("Peer " + addr2 + " not found")

    if "OK" not in dev[1].global_request("P2P_INVITE group=" + dev[1].group_ifname + " peer=" + addr2):
        raise Exception("Unexpected failure of P2P_INVITE to known peer")
    ev = dev[2].wait_global_event(["P2P-INVITATION-RECEIVED"], timeout=10)
    if ev is None:
        raise Exception("Timeout on invitation invited peer")
    if "sa=" + addr1 not in ev:
        raise Exception("Incorrect source address")
    if "go_dev_addr=" + addr0 not in ev:
        raise Exception("Incorrect GO address")
    ev = dev[1].wait_global_event(["P2P-INVITATION-RESULT"], timeout=10)
    if ev is None:
        raise Exception("Timeout on invitation on inviting client")
    if "status=1" not in ev:
        raise Exception("Unexpected invitation result")

    pin = dev[2].wps_read_pin()
    dev[0].p2p_go_authorize_client(pin)
    dev[2].p2p_connect_group(addr0, pin, timeout=60)

    if "FAIL" not in dev[1].global_request("P2P_INVITE group=" + dev[1].group_ifname + " peer=00:11:22:33:44:55"):
        raise Exception("Unexpected success of P2P_INVITE to unknown peer")

    dev[0].remove_group()
    dev[1].wait_go_ending_session()
    dev[2].wait_go_ending_session()

@remote_compatible
def test_p2p_invite_invalid(dev):
    """Invalid parameters to P2P_INVITE"""
    id = dev[0].add_network()
    for cmd in ["foo=bar",
                "persistent=123 peer=foo",
                "persistent=123",
                "persistent=%d" % id,
                "group=foo",
                "group=foo peer=foo",
                "group=foo peer=00:11:22:33:44:55 go_dev_addr=foo"]:
        if "FAIL" not in dev[0].request("P2P_INVITE " + cmd):
            raise Exception("Invalid P2P_INVITE accepted: " + cmd)
