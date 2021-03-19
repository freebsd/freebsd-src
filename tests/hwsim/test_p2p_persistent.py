# P2P persistent group test cases
# Copyright (c) 2013-2014, Jouni Malinen <j@w1.fi>
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

from remotehost import remote_compatible
import logging
logger = logging.getLogger()
import re
import time

import hwsim_utils
from p2p_utils import *

@remote_compatible
def test_persistent_group(dev):
    """P2P persistent group formation and re-invocation"""
    form(dev[0], dev[1])
    invite_from_cli(dev[0], dev[1])
    invite_from_go(dev[0], dev[1])

    logger.info("Remove group on the client and try to invite from GO")
    id = None
    for n in dev[0].list_networks(p2p=True):
        if "[P2P-PERSISTENT]" in n['flags']:
            id = n['id']
            break
    if id is None:
        raise Exception("Could not find persistent group entry")
    clients = dev[0].global_request("GET_NETWORK " + id + " p2p_client_list").rstrip()
    if dev[1].p2p_dev_addr() not in clients:
        raise Exception("Peer missing from client list")
    if "FAIL" not in dev[1].request("SELECT_NETWORK " + str(id)):
        raise Exception("SELECT_NETWORK succeeded unexpectedly")
    if "FAIL" not in dev[1].request("SELECT_NETWORK 1234567"):
        raise Exception("SELECT_NETWORK succeeded unexpectedly(2)")
    if "FAIL" not in dev[1].request("ENABLE_NETWORK " + str(id)):
        raise Exception("ENABLE_NETWORK succeeded unexpectedly")
    if "FAIL" not in dev[1].request("ENABLE_NETWORK 1234567"):
        raise Exception("ENABLE_NETWORK succeeded unexpectedly(2)")
    if "FAIL" not in dev[1].request("DISABLE_NETWORK " + str(id)):
        raise Exception("DISABLE_NETWORK succeeded unexpectedly")
    if "FAIL" not in dev[1].request("DISABLE_NETWORK 1234567"):
        raise Exception("DISABLE_NETWORK succeeded unexpectedly(2)")
    if "FAIL" not in dev[1].request("REMOVE_NETWORK 1234567"):
        raise Exception("REMOVE_NETWORK succeeded unexpectedly")
    dev[1].global_request("REMOVE_NETWORK all")
    if len(dev[1].list_networks(p2p=True)) > 0:
        raise Exception("Unexpected network block remaining")
    invite(dev[0], dev[1])
    ev = dev[0].wait_global_event(["P2P-INVITATION-RESULT"], timeout=10)
    if ev is None:
        raise Exception("No invitation result seen")
    if "status=8" not in ev:
        raise Exception("Unexpected invitation result: " + ev)
    clients = dev[0].request("GET_NETWORK " + id + " p2p_client_list").rstrip()
    if dev[1].p2p_dev_addr() in clients:
        raise Exception("Peer was still in client list")

@remote_compatible
def test_persistent_group2(dev):
    """P2P persistent group formation with reverse roles"""
    form(dev[0], dev[1], reverse_init=True)
    invite_from_cli(dev[0], dev[1])
    invite_from_go(dev[0], dev[1])

@remote_compatible
def test_persistent_group3(dev):
    """P2P persistent group formation and re-invocation with empty BSS table"""
    form(dev[0], dev[1])
    dev[1].request("BSS_FLUSH 0")
    invite_from_cli(dev[0], dev[1])
    dev[1].request("BSS_FLUSH 0")
    invite_from_go(dev[0], dev[1])

def test_persistent_group_per_sta_psk(dev):
    """P2P persistent group formation and re-invocation using per-client PSK"""
    addr0 = dev[0].p2p_dev_addr()
    addr1 = dev[1].p2p_dev_addr()
    addr2 = dev[2].p2p_dev_addr()
    dev[0].global_request("P2P_SET per_sta_psk 1")
    logger.info("Form a persistent group")
    [i_res, r_res] = go_neg_pin_authorized_persistent(i_dev=dev[0], i_intent=15,
                                                      r_dev=dev[1], r_intent=0)
    if not i_res['persistent'] or not r_res['persistent']:
        raise Exception("Formed group was not persistent")

    logger.info("Join another client to the group")
    pin = dev[2].wps_read_pin()
    dev[0].p2p_go_authorize_client(pin)
    social = int(i_res['freq']) in [2412, 2437, 2462]
    c_res = dev[2].p2p_connect_group(addr0, pin, timeout=60, social=social,
                                     freq=i_res['freq'])
    if not c_res['persistent']:
        raise Exception("Joining client did not recognize persistent group")
    if r_res['psk'] == c_res['psk']:
        raise Exception("Same PSK assigned for both clients")
    hwsim_utils.test_connectivity_p2p(dev[1], dev[2])

    logger.info("Remove persistent group and re-start it manually")
    dev[0].remove_group()
    dev[1].wait_go_ending_session()
    dev[2].wait_go_ending_session()
    dev[0].dump_monitor()
    dev[1].dump_monitor()
    dev[2].dump_monitor()

    for i in range(0, 3):
        networks = dev[i].list_networks(p2p=True)
        if len(networks) != 1:
            raise Exception("Unexpected number of networks")
        if "[P2P-PERSISTENT]" not in networks[0]['flags']:
            raise Exception("Not the persistent group data")
        if i > 0:
            # speed up testing by avoiding use of the old BSS entry since the
            # GO may have changed channels
            dev[i].request("BSS_FLUSH 0")
            dev[i].scan(freq="2412", only_new=True)
        if "OK" not in dev[i].global_request("P2P_GROUP_ADD persistent=" + networks[0]['id'] + " freq=2412"):
            raise Exception("Could not re-start persistent group")
        ev = dev[i].wait_global_event(["P2P-GROUP-STARTED"], timeout=30)
        if ev is None:
            raise Exception("Timeout on group restart")
        dev[i].group_form_result(ev)

    logger.info("Leave persistent group and rejoin it")
    dev[2].remove_group()
    ev = dev[2].wait_global_event(["P2P-GROUP-REMOVED"], timeout=3)
    if ev is None:
        raise Exception("Group removal event timed out")
    if not dev[2].discover_peer(addr0, social=True):
        raise Exception("Peer " + addr0 + " not found")
    dev[2].dump_monitor()
    peer = dev[2].get_peer(addr0)
    dev[2].global_request("P2P_GROUP_ADD persistent=" + peer['persistent'] + " freq=2412")
    ev = dev[2].wait_global_event(["P2P-GROUP-STARTED"], timeout=30)
    if ev is None:
        raise Exception("Timeout on group restart (on client)")
    cli_res = dev[2].group_form_result(ev)
    if not cli_res['persistent']:
        raise Exception("Persistent group not restarted as persistent (cli)")
    hwsim_utils.test_connectivity_p2p(dev[1], dev[2])

    logger.info("Remove one of the clients from the group without removing persistent group information for the client")
    dev[0].global_request("P2P_REMOVE_CLIENT iface=" + dev[2].p2p_interface_addr())
    dev[2].wait_go_ending_session()

    logger.info("Try to reconnect after having been removed from group (but persistent group info still present)")
    if not dev[2].discover_peer(addr0, social=True):
        raise Exception("Peer " + peer + " not found")
    dev[2].dump_monitor()
    peer = dev[2].get_peer(addr0)
    dev[2].global_request("P2P_GROUP_ADD persistent=" + peer['persistent'] + " freq=2412")
    ev = dev[2].wait_global_event(["P2P-GROUP-STARTED",
                                   "WPA: 4-Way Handshake failed"], timeout=30)
    if ev is None:
        raise Exception("Timeout on group restart (on client)")
    if "P2P-GROUP-STARTED" not in ev:
        raise Exception("Connection failed")

    logger.info("Remove one of the clients from the group")
    dev[0].global_request("P2P_REMOVE_CLIENT " + addr2)
    dev[2].wait_go_ending_session()

    logger.info("Try to reconnect after having been removed from group")
    if not dev[2].discover_peer(addr0, social=True):
        raise Exception("Peer " + peer + " not found")
    dev[2].dump_monitor()
    peer = dev[2].get_peer(addr0)
    dev[2].global_request("P2P_GROUP_ADD persistent=" + peer['persistent'] + " freq=2412")
    ev = dev[2].wait_global_event(["P2P-GROUP-STARTED",
                                   "WPA: 4-Way Handshake failed"], timeout=30)
    if ev is None:
        raise Exception("Timeout on group restart (on client)")
    if "P2P-GROUP-STARTED" in ev:
        raise Exception("Client managed to connect after being removed")

    logger.info("Remove the remaining client from the group")
    dev[0].global_request("P2P_REMOVE_CLIENT " + addr1)
    dev[1].wait_go_ending_session()

    logger.info("Terminate persistent group")
    dev[0].remove_group()
    dev[0].dump_monitor()

    logger.info("Try to re-invoke persistent group from client")
    dev[0].global_request("SET persistent_reconnect 1")
    dev[0].p2p_listen()
    if not dev[1].discover_peer(addr0, social=True):
        raise Exception("Peer " + peer + " not found")
    dev[1].dump_monitor()
    peer = dev[1].get_peer(addr0)
    dev[1].global_request("P2P_INVITE persistent=" + peer['persistent'] + " peer=" + addr0)
    ev = dev[0].wait_global_event(["P2P-GROUP-STARTED"], timeout=30)
    dev[0].group_form_result(ev)
    ev = dev[1].wait_global_event(["P2P-GROUP-STARTED",
                                   "WPA: 4-Way Handshake failed"], timeout=30)
    if ev is None:
        raise Exception("Timeout on group restart (on client)")
    if "P2P-GROUP-STARTED" in ev:
        raise Exception("Client managed to re-invoke after being removed")
    dev[0].dump_monitor()

    logger.info("Terminate persistent group")
    dev[0].remove_group()
    dev[0].dump_monitor()

def test_persistent_group_invite_removed_client(dev):
    """P2P persistent group client removal and re-invitation"""
    addr0 = dev[0].p2p_dev_addr()
    addr1 = dev[1].p2p_dev_addr()
    dev[0].request("P2P_SET per_sta_psk 1")
    logger.info("Form a persistent group")
    [i_res, r_res] = go_neg_pin_authorized_persistent(i_dev=dev[0], i_intent=15,
                                                      r_dev=dev[1], r_intent=0)
    if not i_res['persistent'] or not r_res['persistent']:
        raise Exception("Formed group was not persistent")

    logger.info("Remove client from the group")
    dev[0].global_request("P2P_REMOVE_CLIENT " + addr1)
    dev[1].wait_go_ending_session()

    logger.info("Re-invite the removed client to join the group")
    dev[1].p2p_listen()
    if not dev[0].discover_peer(addr1, social=True):
        raise Exception("Peer " + peer + " not found")
    dev[0].global_request("P2P_INVITE group=" + dev[0].group_ifname + " peer=" + addr1)
    ev = dev[1].wait_global_event(["P2P-INVITATION-RECEIVED"], timeout=10)
    if ev is None:
        raise Exception("Timeout on invitation")
    if "sa=" + addr0 + " persistent=" not in ev:
        raise Exception("Unexpected invitation event")
    [event, addr, persistent] = ev.split(' ', 2)
    dev[1].global_request("P2P_GROUP_ADD " + persistent)
    ev = dev[1].wait_global_event(["P2P-PERSISTENT-PSK-FAIL"], timeout=30)
    if ev is None:
        raise Exception("Did not receive PSK failure report")
    [tmp, id] = ev.split('=', 1)
    ev = dev[1].wait_global_event(["P2P-GROUP-REMOVED"], timeout=10)
    if ev is None:
        raise Exception("Group removal event timed out")
    if "reason=PSK_FAILURE" not in ev:
        raise Exception("Unexpected group removal reason")
    dev[1].global_request("REMOVE_NETWORK " + id)

    logger.info("Re-invite after client removed persistent group info")
    dev[1].p2p_listen()
    if not dev[0].discover_peer(addr1, social=True):
        raise Exception("Peer " + peer + " not found")
    dev[0].global_request("P2P_INVITE group=" + dev[0].group_ifname + " peer=" + addr1)
    ev = dev[1].wait_global_event(["P2P-INVITATION-RECEIVED"], timeout=10)
    if ev is None:
        raise Exception("Timeout on invitation")
    if " persistent=" in ev:
        raise Exception("Unexpected invitation event")
    pin = dev[1].wps_read_pin()
    dev[0].p2p_go_authorize_client(pin)
    c_res = dev[1].p2p_connect_group(addr0, pin, timeout=60, social=True,
                                     freq=i_res['freq'])
    if not c_res['persistent']:
        raise Exception("Joining client did not recognize persistent group")
    if r_res['psk'] == c_res['psk']:
        raise Exception("Same PSK assigned on both times")
    hwsim_utils.test_connectivity_p2p(dev[0], dev[1])

    terminate_group(dev[0], dev[1])

@remote_compatible
def test_persistent_group_channel(dev):
    """P2P persistent group re-invocation with channel selection"""
    form(dev[0], dev[1], test_data=False)

    logger.info("Re-invoke persistent group from client with forced channel")
    invite(dev[1], dev[0], "freq=2427")
    [go_res, cli_res] = check_result(dev[0], dev[1])
    if go_res['freq'] != "2427":
        raise Exception("Persistent group client forced channel not followed")
    terminate_group(dev[0], dev[1])

    logger.info("Re-invoke persistent group from GO with forced channel")
    invite(dev[0], dev[1], "freq=2432")
    [go_res, cli_res] = check_result(dev[0], dev[1])
    if go_res['freq'] != "2432":
        raise Exception("Persistent group GO channel preference not followed")
    terminate_group(dev[0], dev[1])

    logger.info("Re-invoke persistent group from client with channel preference")
    invite(dev[1], dev[0], "pref=2417")
    [go_res, cli_res] = check_result(dev[0], dev[1])
    if go_res['freq'] != "2417":
        raise Exception("Persistent group client channel preference not followed")
    terminate_group(dev[0], dev[1])

@remote_compatible
def test_persistent_group_and_role_change(dev):
    """P2P persistent group, auto GO in another role, and re-invocation"""
    form(dev[0], dev[1])

    logger.info("Start and stop autonomous GO on previous P2P client device")
    dev[1].p2p_start_go()
    dev[1].remove_group()
    dev[1].dump_monitor()

    logger.info("Re-invoke the persistent group")
    invite_from_go(dev[0], dev[1])

def test_persistent_go_client_list(dev):
    """P2P GO and list of clients in persistent group"""
    addr0 = dev[0].p2p_dev_addr()
    addr1 = dev[1].p2p_dev_addr()
    addr2 = dev[2].p2p_dev_addr()

    res = dev[0].p2p_start_go(persistent=True)
    id = None
    for n in dev[0].list_networks(p2p=True):
        if "[P2P-PERSISTENT]" in n['flags']:
            id = n['id']
            break
    if id is None:
        raise Exception("Could not find persistent group entry")

    connect_cli(dev[0], dev[1], social=True, freq=res['freq'])
    clients = dev[0].global_request("GET_NETWORK " + id + " p2p_client_list").rstrip()
    if clients != addr1:
        raise Exception("Unexpected p2p_client_list entry(2): " + clients)
    connect_cli(dev[0], dev[2], social=True, freq=res['freq'])
    clients = dev[0].global_request("GET_NETWORK " + id + " p2p_client_list").rstrip()
    if clients != addr2 + " " + addr1:
        raise Exception("Unexpected p2p_client_list entry(3): " + clients)

    peer = dev[1].get_peer(res['go_dev_addr'])
    dev[1].remove_group()
    dev[1].global_request("P2P_GROUP_ADD persistent=" + peer['persistent'])
    ev = dev[1].wait_global_event(["P2P-GROUP-STARTED"], timeout=30)
    if ev is None:
        raise Exception("Timeout on group restart (on client)")
    dev[1].group_form_result(ev)
    clients = dev[0].global_request("GET_NETWORK " + id + " p2p_client_list").rstrip()
    if clients != addr1 + " " + addr2:
        raise Exception("Unexpected p2p_client_list entry(4): " + clients)

    dev[2].remove_group()
    dev[1].remove_group()
    dev[0].remove_group()

    clients = dev[0].global_request("GET_NETWORK " + id + " p2p_client_list").rstrip()
    if clients != addr1 + " " + addr2:
        raise Exception("Unexpected p2p_client_list entry(5): " + clients)

    dev[1].p2p_listen()
    dev[2].p2p_listen()
    dev[0].request("P2P_FLUSH")
    dev[0].discover_peer(addr1, social=True)
    peer = dev[0].get_peer(addr1)
    if 'persistent' not in peer or peer['persistent'] != id:
        raise Exception("Persistent group client not recognized(1)")

    dev[0].discover_peer(addr2, social=True)
    peer = dev[0].get_peer(addr2)
    if 'persistent' not in peer or peer['persistent'] != id:
        raise Exception("Persistent group client not recognized(2)")

@remote_compatible
def test_persistent_group_in_grpform(dev):
    """P2P persistent group parameters re-used in group formation"""
    addr0 = dev[0].p2p_dev_addr()
    addr1 = dev[1].p2p_dev_addr()
    form(dev[0], dev[1])
    dev[1].p2p_listen()
    if not dev[0].discover_peer(addr1, social=True):
        raise Exception("Could not discover peer")
    peer = dev[0].get_peer(addr1)
    if "persistent" not in peer:
        raise Exception("Could not map peer to a persistent group")

    pin = dev[1].wps_read_pin()
    dev[1].p2p_go_neg_auth(addr0, pin, "display", go_intent=0)
    i_res = dev[0].p2p_go_neg_init(addr1, pin, "enter", timeout=20,
                                   go_intent=15,
                                   persistent_id=peer['persistent'])
    r_res = dev[1].p2p_go_neg_auth_result()
    logger.debug("i_res: " + str(i_res))
    logger.debug("r_res: " + str(r_res))

@remote_compatible
def test_persistent_group_without_persistent_reconnect(dev):
    """P2P persistent group re-invocation without persistent reconnect"""
    form(dev[0], dev[1])
    dev[0].dump_monitor()
    dev[1].dump_monitor()

    logger.info("Re-invoke persistent group from client")
    invite(dev[1], dev[0], persistent_reconnect=False)

    ev = dev[0].wait_global_event(["P2P-INVITATION-RECEIVED"], timeout=15)
    if ev is None:
        raise Exception("No invitation request reported")
    if "persistent=" not in ev:
        raise Exception("Invalid invitation type reported: " + ev)

    ev2 = dev[1].wait_global_event(["P2P-INVITATION-RESULT"], timeout=15)
    if ev2 is None:
        raise Exception("No invitation response reported")
    if "status=1" not in ev2:
        raise Exception("Unexpected status: " + ev2)
    dev[1].p2p_listen()

    exp = r'<.>(P2P-INVITATION-RECEIVED) sa=([0-9a-f:]*) persistent=([0-9]*) freq=([0-9]*)'
    s = re.split(exp, ev)
    if len(s) < 5:
        raise Exception("Could not parse invitation event")
    sa = s[2]
    id = s[3]
    freq = s[4]
    logger.info("Invalid P2P_INVITE test coverage")
    if "FAIL" not in dev[0].global_request("P2P_INVITE persistent=" + id + " peer=" + sa + " freq=0"):
        raise Exception("Invalid P2P_INVITE accepted")
    if "FAIL" not in dev[0].global_request("P2P_INVITE persistent=" + id + " peer=" + sa + " pref=0"):
        raise Exception("Invalid P2P_INVITE accepted")
    logger.info("Re-initiate invitation based on upper layer acceptance")
    if "OK" not in dev[0].global_request("P2P_INVITE persistent=" + id + " peer=" + sa + " freq=" + freq):
        raise Exception("Invitation command failed")
    [go_res, cli_res] = check_result(dev[0], dev[1])
    if go_res['freq'] != freq:
        raise Exception("Unexpected channel on GO: {} MHz, expected {} MHz".format(go_res['freq'], freq))
    if cli_res['freq'] != freq:
        raise Exception("Unexpected channel on CLI: {} MHz, expected {} MHz".format(cli_res['freq'], freq))
    terminate_group(dev[0], dev[1])
    dev[0].dump_monitor()
    dev[1].dump_monitor()

    logger.info("Re-invoke persistent group from GO")
    invite(dev[0], dev[1], persistent_reconnect=False)

    ev = dev[1].wait_global_event(["P2P-INVITATION-RECEIVED"], timeout=15)
    if ev is None:
        raise Exception("No invitation request reported")
    if "persistent=" not in ev:
        raise Exception("Invalid invitation type reported: " + ev)

    ev2 = dev[0].wait_global_event(["P2P-INVITATION-RESULT"], timeout=15)
    if ev2 is None:
        raise Exception("No invitation response reported")
    if "status=1" not in ev2:
        raise Exception("Unexpected status: " + ev2)
    dev[0].p2p_listen()

    exp = r'<.>(P2P-INVITATION-RECEIVED) sa=([0-9a-f:]*) persistent=([0-9]*)'
    s = re.split(exp, ev)
    if len(s) < 4:
        raise Exception("Could not parse invitation event")
    sa = s[2]
    id = s[3]
    logger.info("Re-initiate invitation based on upper layer acceptance")
    if "OK" not in dev[1].global_request("P2P_INVITE persistent=" + id + " peer=" + sa + " freq=" + freq):
        raise Exception("Invitation command failed")
    [go_res, cli_res] = check_result(dev[0], dev[1])
    terminate_group(dev[0], dev[1])

@remote_compatible
def test_persistent_group_already_running(dev):
    """P2P persistent group formation and invitation while GO already running"""
    form(dev[0], dev[1])
    peer = dev[1].get_peer(dev[0].p2p_dev_addr())
    listen_freq = peer['listen_freq']
    dev[0].dump_monitor()
    dev[1].dump_monitor()
    networks = dev[0].list_networks(p2p=True)
    if len(networks) != 1:
        raise Exception("Unexpected number of networks")
    if "[P2P-PERSISTENT]" not in networks[0]['flags']:
        raise Exception("Not the persistent group data")
    if "OK" not in dev[0].global_request("P2P_GROUP_ADD persistent=" + networks[0]['id'] + " freq=" + listen_freq):
        raise Exception("Could not state GO")
    invite_from_cli(dev[0], dev[1])

@remote_compatible
def test_persistent_group_add_cli_chan(dev):
    """P2P persistent group formation and re-invocation with p2p_add_cli_chan=1"""
    try:
        dev[0].request("SET p2p_add_cli_chan 1")
        dev[1].request("SET p2p_add_cli_chan 1")
        form(dev[0], dev[1])
        dev[1].request("BSS_FLUSH 0")
        dev[1].scan(freq="2412", only_new=True)
        dev[1].scan(freq="2437", only_new=True)
        dev[1].scan(freq="2462", only_new=True)
        dev[1].request("BSS_FLUSH 0")
        invite_from_cli(dev[0], dev[1])
        invite_from_go(dev[0], dev[1])
    finally:
        dev[0].request("SET p2p_add_cli_chan 0")
        dev[1].request("SET p2p_add_cli_chan 0")

@remote_compatible
def test_persistent_invalid_group_add(dev):
    """Invalid P2P_GROUP_ADD command"""
    id = dev[0].add_network()
    if "FAIL" not in dev[0].global_request("P2P_GROUP_ADD persistent=12345"):
        raise Exception("Invalid P2P_GROUP_ADD accepted")
    if "FAIL" not in dev[0].global_request("P2P_GROUP_ADD persistent=%d" % id):
        raise Exception("Invalid P2P_GROUP_ADD accepted")
    if "FAIL" not in dev[0].global_request("P2P_GROUP_ADD foo"):
        raise Exception("Invalid P2P_GROUP_ADD accepted")

def test_persistent_group_missed_inv_resp(dev):
    """P2P persistent group re-invocation with invitation response getting lost"""
    form(dev[0], dev[1])
    addr = dev[1].p2p_dev_addr()
    dev[1].global_request("SET persistent_reconnect 1")
    dev[1].p2p_listen()
    if not dev[0].discover_peer(addr, social=True):
        raise Exception("Peer " + addr + " not found")
    dev[0].dump_monitor()
    peer = dev[0].get_peer(addr)
    # Drop the first Invitation Response frame
    if "FAIL" in dev[0].request("SET ext_mgmt_frame_handling 1"):
        raise Exception("Failed to enable external management frame handling")
    cmd = "P2P_INVITE persistent=" + peer['persistent'] + " peer=" + addr
    dev[0].global_request(cmd)
    rx_msg = dev[0].mgmt_rx()
    if rx_msg is None:
        raise Exception("MGMT-RX timeout (no Invitation Response)")
    time.sleep(2)
    # Allow following Invitation Response frame to go through
    if "FAIL" in dev[0].request("SET ext_mgmt_frame_handling 0"):
        raise Exception("Failed to disable external management frame handling")
    time.sleep(1)
    # Force the P2P Client side to be on its Listen channel for retry
    dev[1].p2p_listen()
    ev = dev[0].wait_global_event(["P2P-INVITATION-RESULT"], timeout=15)
    if ev is None:
        raise Exception("Invitation result timed out")
    # Allow P2P Client side to continue connection-to-GO attempts
    dev[1].p2p_stop_find()

    # Verify that group re-invocation goes through
    ev = dev[1].wait_global_event(["P2P-GROUP-STARTED",
                                   "P2P-GROUP-FORMATION-FAILURE"],
                                  timeout=20)
    if ev is None:
        raise Exception("Group start event timed out")
    if "P2P-GROUP-STARTED" not in ev:
        raise Exception("Group re-invocation failed")
    dev[0].group_form_result(ev)

    ev = dev[0].wait_global_event(["P2P-GROUP-STARTED"], timeout=5)
    if ev is None:
        raise Exception("Group start event timed out on GO")
    dev[0].group_form_result(ev)

    terminate_group(dev[0], dev[1])

@remote_compatible
def test_persistent_group_profile_add(dev):
    """Create a P2P persistent group with ADD_NETWORK"""
    passphrase = "passphrase here"
    id = dev[0].p2pdev_add_network()
    dev[0].p2pdev_set_network_quoted(id, "ssid", "DIRECT-ab")
    dev[0].p2pdev_set_network_quoted(id, "psk", passphrase)
    dev[0].p2pdev_set_network(id, "mode", "3")
    dev[0].p2pdev_set_network(id, "disabled", "2")
    dev[0].p2p_start_go(persistent=id, freq=2412)

    pin = dev[1].wps_read_pin()
    dev[0].p2p_go_authorize_client(pin)
    res = dev[1].p2p_connect_group(dev[0].p2p_dev_addr(), pin, timeout=60,
                                   social=True, freq=2412)
    if res['result'] != 'success':
        raise Exception("Joining the group did not succeed")

    dev[0].remove_group()
    dev[1].wait_go_ending_session()

@remote_compatible
def test_persistent_group_cancel_on_cli(dev):
    """P2P persistent group formation, re-invocation, and cancel"""
    dev[0].global_request("SET p2p_no_group_iface 0")
    dev[1].global_request("SET p2p_no_group_iface 0")
    form(dev[0], dev[1])

    invite_from_go(dev[0], dev[1], terminate=False)
    if "FAIL" not in dev[1].global_request("P2P_CANCEL"):
        raise Exception("P2P_CANCEL succeeded unexpectedly on CLI")
    if "FAIL" not in dev[0].global_request("P2P_CANCEL"):
        raise Exception("P2P_CANCEL succeeded unexpectedly on GO")
    terminate_group(dev[0], dev[1])

    invite_from_cli(dev[0], dev[1], terminate=False)
    if "FAIL" not in dev[1].global_request("P2P_CANCEL"):
        raise Exception("P2P_CANCEL succeeded unexpectedly on CLI")
    if "FAIL" not in dev[0].global_request("P2P_CANCEL"):
        raise Exception("P2P_CANCEL succeeded unexpectedly on GO")
    terminate_group(dev[0], dev[1])

@remote_compatible
def test_persistent_group_cancel_on_cli2(dev):
    """P2P persistent group formation, re-invocation, and cancel (2)"""
    form(dev[0], dev[1])
    invite_from_go(dev[0], dev[1], terminate=False)
    if "FAIL" not in dev[1].global_request("P2P_CANCEL"):
        raise Exception("P2P_CANCEL succeeded unexpectedly on CLI")
    if "FAIL" not in dev[0].global_request("P2P_CANCEL"):
        raise Exception("P2P_CANCEL succeeded unexpectedly on GO")
    terminate_group(dev[0], dev[1])

    invite_from_cli(dev[0], dev[1], terminate=False)
    if "FAIL" not in dev[1].global_request("P2P_CANCEL"):
        raise Exception("P2P_CANCEL succeeded unexpectedly on CLI")
    if "FAIL" not in dev[0].global_request("P2P_CANCEL"):
        raise Exception("P2P_CANCEL succeeded unexpectedly on GO")
    terminate_group(dev[0], dev[1])

@remote_compatible
def test_persistent_group_peer_dropped(dev):
    """P2P persistent group formation and re-invocation with peer having dropped group"""
    form(dev[0], dev[1], reverse_init=True)
    invite_from_cli(dev[0], dev[1])

    logger.info("Remove group on the GO and try to invite from the client")
    dev[0].global_request("REMOVE_NETWORK all")
    invite(dev[1], dev[0])
    ev = dev[1].wait_global_event(["P2P-INVITATION-RESULT"], timeout=10)
    if ev is None:
        raise Exception("No invitation result seen")
    if "status=8" not in ev:
        raise Exception("Unexpected invitation result: " + ev)
    networks = dev[1].list_networks(p2p=True)
    if len(networks) > 0:
        raise Exception("Unexpected network block on client")

    logger.info("Verify that a new group can be formed")
    form(dev[0], dev[1], reverse_init=True)

@remote_compatible
def test_persistent_group_peer_dropped2(dev):
    """P2P persistent group formation and re-invocation with peer having dropped group (2)"""
    form(dev[0], dev[1])
    invite_from_go(dev[0], dev[1])

    logger.info("Remove group on the client and try to invite from the GO")
    dev[1].global_request("REMOVE_NETWORK all")
    invite(dev[0], dev[1])
    ev = dev[0].wait_global_event(["P2P-INVITATION-RESULT"], timeout=10)
    if ev is None:
        raise Exception("No invitation result seen")
    if "status=8" not in ev:
        raise Exception("Unexpected invitation result: " + ev)
    networks = dev[1].list_networks(p2p=True)
    if len(networks) > 0:
        raise Exception("Unexpected network block on client")

    logger.info("Verify that a new group can be formed")
    form(dev[0], dev[1])

def test_persistent_group_peer_dropped3(dev):
    """P2P persistent group formation and re-invocation with peer having dropped group (3)"""
    form(dev[0], dev[1], reverse_init=True)
    invite_from_cli(dev[0], dev[1])

    logger.info("Remove group on the GO and try to invite from the client")
    dev[0].global_request("REMOVE_NETWORK all")
    invite(dev[1], dev[0], use_listen=False)
    ev = dev[1].wait_global_event(["P2P-INVITATION-RESULT"], timeout=10)
    if ev is None:
        raise Exception("No invitation result seen")
    if "status=8" not in ev:
        raise Exception("Unexpected invitation result: " + ev)
    networks = dev[1].list_networks(p2p=True)
    if len(networks) > 0:
        raise Exception("Unexpected network block on client")

    time.sleep(0.2)
    logger.info("Verify that a new group can be formed")
    form(dev[0], dev[1], reverse_init=True, r_listen=False)
