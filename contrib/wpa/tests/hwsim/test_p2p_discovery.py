# P2P device discovery test cases
# Copyright (c) 2013, Jouni Malinen <j@w1.fi>
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

from remotehost import remote_compatible
import logging
logger = logging.getLogger()
import binascii
import os
import struct
import time

import hostapd
import hwsim_utils
from wpasupplicant import WpaSupplicant
from p2p_utils import *
from hwsim import HWSimRadio
from tshark import run_tshark
from test_gas import start_ap
from test_cfg80211 import nl80211_remain_on_channel
from test_p2p_channel import set_country

@remote_compatible
def test_discovery(dev):
    """P2P device discovery and provision discovery"""
    addr0 = dev[0].p2p_dev_addr()
    addr1 = dev[1].p2p_dev_addr()
    logger.info("Start device discovery")
    dev[0].p2p_find(social=True, delay=1)
    if not dev[1].discover_peer(addr0):
        raise Exception("Device discovery timed out")
    if not dev[0].discover_peer(addr1):
        raise Exception("Device discovery timed out")

    logger.info("Test provision discovery for display")
    dev[0].global_request("P2P_PROV_DISC " + addr1 + " display")
    ev1 = dev[1].wait_global_event(["P2P-PROV-DISC-SHOW-PIN"], timeout=15)
    if ev1 is None:
        raise Exception("Provision discovery timed out (display/dev1)")
    if addr0 not in ev1:
        raise Exception("Dev0 not in provision discovery event")
    ev0 = dev[0].wait_global_event(["P2P-PROV-DISC-ENTER-PIN",
                                    "P2P-PROV-DISC-FAILURE"], timeout=15)
    if ev0 is None:
        raise Exception("Provision discovery timed out (display/dev0)")
    if "P2P-PROV-DISC-FAILURE" in ev0:
        raise Exception("Provision discovery failed (display/dev0)")
    if addr1 not in ev0:
        raise Exception("Dev1 not in provision discovery event")

    logger.info("Test provision discovery for keypad")
    dev[0].global_request("P2P_PROV_DISC " + addr1 + " keypad")
    ev1 = dev[1].wait_global_event(["P2P-PROV-DISC-ENTER-PIN"], timeout=15)
    if ev1 is None:
        raise Exception("Provision discovery timed out (keypad/dev1)")
    if addr0 not in ev1:
        raise Exception("Dev0 not in provision discovery event")
    ev0 = dev[0].wait_global_event(["P2P-PROV-DISC-SHOW-PIN",
                                    "P2P-PROV-DISC-FAILURE"],
                                   timeout=15)
    if ev0 is None:
        raise Exception("Provision discovery timed out (keypad/dev0)")
    if "P2P-PROV-DISC-FAILURE" in ev0:
        raise Exception("Provision discovery failed (keypad/dev0)")
    if addr1 not in ev0:
        raise Exception("Dev1 not in provision discovery event")

    logger.info("Test provision discovery for push button")
    dev[0].global_request("P2P_PROV_DISC " + addr1 + " pbc")
    ev1 = dev[1].wait_global_event(["P2P-PROV-DISC-PBC-REQ"], timeout=15)
    if ev1 is None:
        raise Exception("Provision discovery timed out (pbc/dev1)")
    if addr0 not in ev1:
        raise Exception("Dev0 not in provision discovery event")
    ev0 = dev[0].wait_global_event(["P2P-PROV-DISC-PBC-RESP",
                                    "P2P-PROV-DISC-FAILURE"],
                                   timeout=15)
    if ev0 is None:
        raise Exception("Provision discovery timed out (pbc/dev0)")
    if "P2P-PROV-DISC-FAILURE" in ev0:
        raise Exception("Provision discovery failed (pbc/dev0)")
    if addr1 not in ev0:
        raise Exception("Dev1 not in provision discovery event")

    dev[0].p2p_stop_find()
    dev[1].p2p_stop_find()

    if "FAIL" not in dev[0].p2p_find(dev_id="foo"):
        raise Exception("P2P_FIND with invalid dev_id accepted")
    if "FAIL" not in dev[0].p2p_find(dev_type="foo"):
        raise Exception("P2P_FIND with invalid dev_type accepted")
    if "FAIL" not in dev[0].p2p_find(dev_type="1-foo-2"):
        raise Exception("P2P_FIND with invalid dev_type accepted")
    if "FAIL" not in dev[0].p2p_find(dev_type="1-11223344"):
        raise Exception("P2P_FIND with invalid dev_type accepted")

    if "FAIL" not in dev[0].global_request("P2P_PROV_DISC foo pbc"):
        raise Exception("Invalid P2P_PROV_DISC accepted")
    if "FAIL" not in dev[0].global_request("P2P_PROV_DISC 00:11:22:33:44:55"):
        raise Exception("Invalid P2P_PROV_DISC accepted")
    if "FAIL" not in dev[0].global_request("P2P_PROV_DISC 00:11:22:33:44:55 pbc join"):
        raise Exception("Invalid P2P_PROV_DISC accepted")
    if "FAIL" not in dev[0].global_request("P2P_PROV_DISC 00:11:22:33:44:55 foo"):
        raise Exception("Invalid P2P_PROV_DISC accepted")

@remote_compatible
def test_discovery_pd_retries(dev):
    """P2P device discovery and provision discovery retries"""
    addr0 = dev[0].p2p_dev_addr()
    addr1 = dev[1].p2p_dev_addr()
    dev[1].p2p_listen()
    if not dev[0].discover_peer(addr1):
        raise Exception("Device discovery timed out")
    dev[1].p2p_stop_find()
    dev[0].p2p_stop_find()
    dev[0].global_request("P2P_PROV_DISC " + addr1 + " display")
    ev = dev[0].wait_global_event(["P2P-PROV-DISC-FAILURE"], timeout=60)
    if ev is None:
        raise Exception("No PD failure reported")

def test_discovery_group_client(dev):
    """P2P device discovery for a client in a group"""
    logger.info("Start autonomous GO " + dev[0].ifname)
    res = dev[0].p2p_start_go(freq="2422")
    logger.debug("res: " + str(res))
    logger.info("Connect a client to the GO")
    pin = dev[1].wps_read_pin()
    dev[0].p2p_go_authorize_client(pin)
    dev[1].p2p_connect_group(dev[0].p2p_dev_addr(), pin, freq=int(res['freq']),
                             timeout=60)
    logger.info("Client connected")
    hwsim_utils.test_connectivity_p2p(dev[0], dev[1])
    logger.info("Try to discover a P2P client in a group")
    if not dev[2].discover_peer(dev[1].p2p_dev_addr(), social=False, timeout=10):
        stop_p2p_find_and_wait(dev[2])
        if not dev[2].discover_peer(dev[1].p2p_dev_addr(), social=False, timeout=10):
            stop_p2p_find_and_wait(dev[2])
            if not dev[2].discover_peer(dev[1].p2p_dev_addr(), social=False, timeout=10):
                raise Exception("Could not discover group client")

    # This is not really perfect, but something to get a bit more testing
    # coverage.. For proper discoverability mechanism validation, the P2P
    # client would need to go to sleep to avoid acknowledging the GO Negotiation
    # Request frame. Offchannel Listen mode operation on the P2P Client with
    # mac80211_hwsim is apparently not enough to avoid the acknowledgement on
    # the operating channel, so need to disconnect from the group which removes
    # the GO-to-P2P Client part of the discoverability exchange in practice.

    pin = dev[2].wps_read_pin()
    # make group client non-responsive on operating channel
    dev[1].dump_monitor()
    dev[1].group_request("DISCONNECT")
    ev = dev[1].wait_group_event(["CTRL-EVENT-DISCONNECTED"], timeout=10)
    if ev is None:
        raise Exception("Timeout on waiting disconnection")
    dev[2].request("P2P_CONNECT {} {} display".format(dev[1].p2p_dev_addr(),
                                                      pin))
    ev = dev[1].wait_global_event(["P2P-GO-NEG-REQUEST"], timeout=2)
    if ev:
        raise Exception("Unexpected frame RX on P2P client")
    # make group client available on operating channe
    dev[1].group_request("REASSOCIATE")
    ev = dev[1].wait_global_event(["CTRL-EVENT-CONNECTED",
                                   "P2P-GO-NEG-REQUEST"], timeout=10)
    if ev is None:
        raise Exception("Timeout on reconnection to group")
    if "P2P-GO-NEG-REQUEST" not in ev:
        ev = dev[1].wait_global_event(["P2P-GO-NEG-REQUEST"], timeout=10)
        if ev is None:
            raise Exception("Timeout on waiting for GO Negotiation Request")

def stop_p2p_find_and_wait(dev):
    dev.request("P2P_STOP_FIND")
    for i in range(10):
        res = dev.get_driver_status_field("scan_state")
        if "SCAN_STARTED" not in res and "SCAN_REQUESTED" not in res:
            break
        logger.debug("Waiting for final P2P_FIND scan to complete")
        time.sleep(0.02)

def test_discovery_ctrl_char_in_devname(dev):
    """P2P device discovery and control character in Device Name"""
    try:
        _test_discovery_ctrl_char_in_devname(dev)
    finally:
        dev[1].global_request("SET device_name Device B")

def _test_discovery_ctrl_char_in_devname(dev):
    dev[1].global_request("SET device_name Device\tB")
    addr0 = dev[0].p2p_dev_addr()
    addr1 = dev[1].p2p_dev_addr()
    res = dev[0].p2p_start_go(freq=2422)
    bssid = dev[0].p2p_interface_addr()
    pin = dev[1].wps_read_pin()
    dev[0].p2p_go_authorize_client(pin)
    dev[1].scan_for_bss(bssid, freq=2422)
    dev[1].p2p_connect_group(addr0, pin, timeout=60, freq=2422)
    if not dev[2].discover_peer(addr1, social=False, freq=2422, timeout=5):
        stop_p2p_find_and_wait(dev[2])
        if not dev[2].discover_peer(addr1, social=False, freq=2422, timeout=5):
            stop_p2p_find_and_wait(dev[2])
            if not dev[2].discover_peer(addr1, social=False, freq=2422,
                                        timeout=5):
                raise Exception("Could not discover group client")
    devname = dev[2].get_peer(addr1)['device_name']
    dev[2].p2p_stop_find()
    if devname != "Device_B":
        raise Exception("Unexpected device_name from group client: " + devname)

    terminate_group(dev[0], dev[1])
    dev[2].request("P2P_FLUSH")

    dev[1].p2p_listen()
    if not dev[2].discover_peer(addr1, social=True, timeout=10):
        raise Exception("Could not discover peer")
    devname = dev[2].get_peer(addr1)['device_name']
    dev[2].p2p_stop_find()
    if devname != "Device_B":
        raise Exception("Unexpected device_name from peer: " + devname)

@remote_compatible
def test_discovery_dev_type(dev):
    """P2P device discovery with Device Type filter"""
    dev[1].request("SET sec_device_type 1-0050F204-2")
    dev[1].p2p_listen()
    dev[0].p2p_find(social=True, dev_type="5-0050F204-1")
    ev = dev[0].wait_global_event(['P2P-DEVICE-FOUND'], timeout=1)
    if ev:
        raise Exception("Unexpected P2P device found")
    dev[0].p2p_find(social=True, dev_type="1-0050F204-2")
    ev = dev[0].wait_global_event(['P2P-DEVICE-FOUND'], timeout=2)
    if ev is None:
        raise Exception("P2P device not found")
    peer = dev[0].get_peer(dev[1].p2p_dev_addr())
    if "1-0050F204-2" not in peer['sec_dev_type']:
        raise Exception("sec_device_type not reported properly")

def test_discovery_dev_type_go(dev):
    """P2P device discovery with Device Type filter on GO"""
    addr1 = dev[1].p2p_dev_addr()
    dev[1].request("SET sec_device_type 1-0050F204-2")
    res = dev[0].p2p_start_go(freq="2412")
    pin = dev[1].wps_read_pin()
    dev[0].p2p_go_authorize_client(pin)
    dev[1].p2p_connect_group(dev[0].p2p_dev_addr(), pin, timeout=60)

    dev[2].p2p_find(social=True, dev_type="5-0050F204-1")
    ev = dev[2].wait_global_event(['P2P-DEVICE-FOUND'], timeout=1)
    if ev:
        raise Exception("Unexpected P2P device found")
    dev[2].p2p_find(social=True, dev_type="1-0050F204-2")
    ev = dev[2].wait_global_event(['P2P-DEVICE-FOUND ' + addr1], timeout=2)
    if ev is None:
        raise Exception("P2P device not found")

def test_discovery_dev_id(dev):
    """P2P device discovery with Device ID filter"""
    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    wpas.interface_add("wlan5")
    wpas.request("P2P_LISTEN 1")
    status = wpas.global_request("STATUS")
    if "p2p_state=LISTEN_ONLY" not in status:
        raise Exception("Unexpected status: " + status)
    addr1 = dev[1].p2p_dev_addr()
    dev[1].p2p_listen()
    dev[0].p2p_find(social=True, dev_id="02:03:04:05:06:07")
    ev = dev[0].wait_global_event(['P2P-DEVICE-FOUND'], timeout=1)
    if ev:
        raise Exception("Unexpected P2P device found")
    dev[0].p2p_find(social=True, dev_id=addr1)
    ev = dev[0].wait_global_event(['P2P-DEVICE-FOUND'], timeout=5)
    if ev is None:
        raise Exception("P2P device not found")
    if addr1 not in ev:
        raise Exception("Unexpected P2P peer found")
    status = wpas.global_request("STATUS")
    for i in range(0, 2):
        if "p2p_state=IDLE" in status:
            break
        time.sleep(0.5)
        status = wpas.global_request("STATUS")
    if "p2p_state=IDLE" not in status:
        raise Exception("Unexpected status: " + status)

def test_discovery_dev_id_go(dev):
    """P2P device discovery with Device ID filter on GO"""
    addr1 = dev[1].p2p_dev_addr()
    res = dev[0].p2p_start_go(freq="2412")
    pin = dev[1].wps_read_pin()
    dev[0].p2p_go_authorize_client(pin)
    dev[1].p2p_connect_group(dev[0].p2p_dev_addr(), pin, timeout=60)

    dev[2].p2p_find(social=True, dev_id="02:03:04:05:06:07")
    ev = dev[2].wait_global_event(['P2P-DEVICE-FOUND'], timeout=1)
    if ev:
        raise Exception("Unexpected P2P device found")
    dev[2].p2p_find(social=True, dev_id=addr1)
    ev = dev[2].wait_global_event(['P2P-DEVICE-FOUND ' + addr1], timeout=2)
    if ev is None:
        raise Exception("P2P device not found")

def test_discovery_social_plus_one(dev):
    """P2P device discovery with social-plus-one"""
    logger.info("Start autonomous GO " + dev[0].ifname)
    dev[1].p2p_find(social=True)
    dev[0].p2p_find(progressive=True)
    logger.info("Wait for initial progressive find phases")
    dev[0].wait_event(["CTRL-EVENT-SCAN-STARTED"])
    dev[0].wait_event(["CTRL-EVENT-SCAN-STARTED"])
    go = dev[2].p2p_dev_addr()
    dev[2].p2p_start_go(freq="2422")
    logger.info("Verify whether the GO on non-social channel can be found")
    ev = dev[0].wait_global_event(["P2P-DEVICE-FOUND"], timeout=15)
    if ev is None:
        raise Exception("Peer not found")
    if go not in ev:
        ev = dev[0].wait_global_event(["P2P-DEVICE-FOUND"], timeout=15)
        if ev is None:
            raise Exception("Peer not found")
    ev = dev[1].wait_global_event(["P2P-DEVICE-FOUND"], timeout=15)
    if ev is None:
        raise Exception("Peer not found")
    dev[0].p2p_stop_find()
    dev[1].p2p_stop_find()
    if not dev[0].peer_known(go):
        raise Exception("GO not found in progressive scan")
    if dev[1].peer_known(go):
        raise Exception("GO found in social-only scan")

def _test_discovery_and_interface_disabled(dev, delay=1):
    try:
        if "OK" not in dev[0].p2p_find():
            raise Exception("Failed to start P2P find")
        ev = dev[0].wait_event(["CTRL-EVENT-SCAN-STARTED"])
        if ev is None:
            raise Exception("Scan did not start")
        dev[0].request("DRIVER_EVENT INTERFACE_DISABLED")
        time.sleep(delay)

        # verify that P2P_FIND is rejected
        if "FAIL" not in dev[0].p2p_find():
            raise Exception("New P2P_FIND request was accepted unexpectedly")

        dev[0].request("DRIVER_EVENT INTERFACE_ENABLED")
        time.sleep(3)
        dev[0].scan(freq="2412")
        if "OK" not in dev[0].p2p_find():
            raise Exception("Failed to start P2P find")
        dev[0].dump_monitor()
        dev[1].p2p_listen()
        ev = dev[0].wait_global_event(["P2P-DEVICE-FOUND"], timeout=15)
        if ev is None:
            raise Exception("Peer not found")
    finally:
        dev[0].request("DRIVER_EVENT INTERFACE_ENABLED")

def test_discovery_and_interface_disabled(dev):
    """P2P device discovery with interface getting disabled"""
    _test_discovery_and_interface_disabled(dev, delay=1)
    _test_discovery_and_interface_disabled(dev, delay=5)

def test_discovery_auto(dev):
    """P2P device discovery and provision discovery with auto GO/dev selection"""
    dev[0].flush_scan_cache()
    addr0 = dev[0].p2p_dev_addr()
    addr1 = dev[1].p2p_dev_addr()
    addr2 = dev[2].p2p_dev_addr()
    dev[2].p2p_start_go(freq="2412")
    logger.info("Start device discovery")
    dev[0].p2p_listen()
    if not dev[1].discover_peer(addr0):
        raise Exception("Device discovery timed out")
    dev[1].p2p_listen()
    if not dev[0].discover_peer(addr1):
        raise Exception("Device discovery timed out")
    if not dev[0].discover_peer(addr2):
        raise Exception("Device discovery timed out")

    logger.info("Test provision discovery for display (device)")
    dev[0].global_request("P2P_PROV_DISC " + addr1 + " display auto")
    ev1 = dev[1].wait_global_event(["P2P-PROV-DISC-SHOW-PIN"], timeout=15)
    if ev1 is None:
        raise Exception("Provision discovery timed out (display/dev1)")
    if addr0 not in ev1:
        raise Exception("Dev0 not in provision discovery event")
    if " group=" in ev1:
        raise Exception("Unexpected group parameter from non-GO")
    ev0 = dev[0].wait_global_event(["P2P-PROV-DISC-ENTER-PIN",
                                    "P2P-PROV-DISC-FAILURE"], timeout=15)
    if ev0 is None:
        raise Exception("Provision discovery timed out (display/dev0)")
    if "P2P-PROV-DISC-FAILURE" in ev0:
        raise Exception("Provision discovery failed (display/dev0)")
    if addr1 not in ev0:
        raise Exception("Dev1 not in provision discovery event")
    if "peer_go=0" not in ev0:
        raise Exception("peer_go incorrect in PD response from non-GO")

    logger.info("Test provision discovery for display (GO)")
    dev[0].global_request("P2P_PROV_DISC " + addr2 + " display auto")
    ev2 = dev[2].wait_global_event(["P2P-PROV-DISC-SHOW-PIN"], timeout=15)
    if ev2 is None:
        raise Exception("Provision discovery timed out (display/dev2)")
    if addr0 not in ev2:
        raise Exception("Dev0 not in provision discovery event")
    if " group=" not in ev2:
        raise Exception("Group parameter missing from GO")
    ev0 = dev[0].wait_global_event(["P2P-PROV-DISC-ENTER-PIN",
                                    "P2P-PROV-DISC-FAILURE"], timeout=15)
    if ev0 is None:
        raise Exception("Provision discovery timed out (display/dev0)")
    if "P2P-PROV-DISC-FAILURE" in ev0:
        raise Exception("Provision discovery failed (display/dev0)")
    if addr2 not in ev0:
        raise Exception("Dev1 not in provision discovery event")
    if "peer_go=1" not in ev0:
        raise Exception("peer_go incorrect in PD response from GO")

def test_discovery_stop(dev):
    """P2P device discovery and p2p_stop_find"""
    addr0 = dev[0].p2p_dev_addr()
    addr1 = dev[1].p2p_dev_addr()
    dev[1].p2p_listen()
    dev[2].p2p_listen()

    dev[0].p2p_find(social=False)
    ev = dev[0].wait_event(["CTRL-EVENT-SCAN-STARTED"], timeout=0.5)
    if ev is None:
        logger.info("No CTRL-EVENT-SCAN-STARTED event")
    dev[0].p2p_stop_find()
    ev = dev[0].wait_global_event(["P2P-FIND-STOPPED"], timeout=1)
    if ev is None:
        raise Exception("P2P_STOP not reported")
    ev = dev[0].wait_global_event(["P2P-DEVICE-FOUND"], timeout=5)
    if ev is not None:
        raise Exception("Peer found unexpectedly: " + ev)

    dev[0].p2p_find(social=False)
    ev = dev[0].wait_event(["CTRL-EVENT-SCAN-STARTED"], timeout=0.5)
    if ev is None:
        logger.info("No CTRL-EVENT-SCAN-STARTED event")
    dev[0].global_request("P2P_FLUSH")
    ev = dev[0].wait_global_event(["P2P-FIND-STOPPED"], timeout=1)
    if ev is None:
        raise Exception("P2P_STOP not reported")
    ev = dev[0].wait_global_event(["P2P-DEVICE-FOUND"], timeout=5)
    if ev is not None:
        raise Exception("Peer found unexpectedly: " + ev)

def test_discovery_restart(dev):
    """P2P device discovery and p2p_find restart"""
    autogo(dev[1], freq=2457)
    dev[0].p2p_find(social=True)
    dev[0].p2p_stop_find()
    dev[0].p2p_find(social=False)
    ev = dev[0].wait_global_event(["P2P-DEVICE-FOUND"], timeout=7)
    if ev is None:
        dev[0].p2p_find(social=False)
        ev = dev[0].wait_global_event(["P2P-DEVICE-FOUND"], timeout=7)
        if ev is None:
            raise Exception("Peer not found")

def test_discovery_restart_progressive(dev):
    """P2P device discovery and p2p_find type=progressive restart"""
    try:
        set_country("US", dev[1])
        autogo(dev[1], freq=5805)
        dev[0].p2p_find(social=True)
        dev[0].p2p_stop_find()
        dev[0].p2p_find(progressive=True)
        ev = dev[0].wait_global_event(["P2P-DEVICE-FOUND"], timeout=20)
        dev[1].remove_group()
        if ev is None:
            raise Exception("Peer not found")
    finally:
        set_country("00")
        dev[1].flush_scan_cache()

def test_p2p_peer_command(dev):
    """P2P_PEER command"""
    addr0 = dev[0].p2p_dev_addr()
    addr1 = dev[1].p2p_dev_addr()
    addr2 = dev[2].p2p_dev_addr()
    dev[1].p2p_listen()
    dev[2].p2p_listen()
    if not dev[0].discover_peer(addr1):
        raise Exception("Device discovery timed out")
    if not dev[0].discover_peer(addr2):
        raise Exception("Device discovery timed out")
    dev[0].p2p_stop_find()
    dev[1].p2p_stop_find()
    dev[2].p2p_stop_find()

    res0 = dev[0].request("P2P_PEER FIRST")
    peer = res0.splitlines()[0]
    if peer not in [addr1, addr2]:
        raise Exception("Unexpected P2P_PEER FIRST address")
    res1 = dev[0].request("P2P_PEER NEXT-" + peer)
    peer2 = res1.splitlines()[0]
    if peer2 not in [addr1, addr2] or peer == peer2:
        raise Exception("Unexpected P2P_PEER NEXT address")

    if "FAIL" not in dev[0].request("P2P_PEER NEXT-foo"):
        raise Exception("Invalid P2P_PEER command accepted")
    if "FAIL" not in dev[0].request("P2P_PEER foo"):
        raise Exception("Invalid P2P_PEER command accepted")
    if "FAIL" not in dev[0].request("P2P_PEER 00:11:22:33:44:55"):
        raise Exception("P2P_PEER command for unknown peer accepted")

def test_p2p_listen_and_offchannel_tx(dev):
    """P2P_LISTEN behavior with offchannel TX"""
    addr0 = dev[0].p2p_dev_addr()
    addr1 = dev[1].p2p_dev_addr()
    addr2 = dev[2].p2p_dev_addr()

    dev[1].p2p_listen()
    if not dev[0].discover_peer(addr1):
        raise Exception("Device discovery timed out")

    dev[0].p2p_listen()
    dev[0].global_request("P2P_PROV_DISC " + addr1 + " display")
    ev = dev[0].wait_global_event(["P2P-PROV-DISC-ENTER-PIN"], timeout=15)
    if ev is None:
        raise Exception("No PD result reported")
    dev[1].p2p_stop_find()

    if not dev[2].discover_peer(addr0):
        raise Exception("Device discovery timed out after PD exchange")
    dev[2].p2p_stop_find()
    dev[0].p2p_stop_find()

@remote_compatible
def test_p2p_listen_and_scan(dev):
    """P2P_LISTEN and scan"""
    dev[0].p2p_listen()
    if "OK" not in dev[0].request("SCAN freq=2412"):
        raise Exception("Failed to request a scan")
    ev = dev[0].wait_event(["CTRL-EVENT-SCAN-RESULTS"], 3)
    if ev is not None:
        raise Exception("Unexpected scan results")
    dev[0].p2p_stop_find()
    ev = dev[0].wait_event(["CTRL-EVENT-SCAN-RESULTS"], 15)
    if ev is None:
        raise Exception("Scan timed out")

def test_p2p_config_methods(dev):
    """P2P and WPS config method update"""
    addr0 = dev[0].p2p_dev_addr()
    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    wpas.interface_add("wlan5")
    addr1 = wpas.p2p_dev_addr()

    if "OK" not in wpas.request("SET config_methods keypad virtual_push_button"):
        raise Exception("Failed to set config_methods")

    wpas.p2p_listen()
    if not dev[0].discover_peer(addr1):
        raise Exception("Device discovery timed out")
    dev[0].p2p_stop_find()
    peer = dev[0].get_peer(addr1)
    if peer['config_methods'] != '0x180':
        raise Exception("Unexpected peer config methods(1): " + peer['config_methods'])
    dev[0].global_request("P2P_FLUSH")

    if "OK" not in wpas.request("SET config_methods virtual_display"):
        raise Exception("Failed to set config_methods")

    if not dev[0].discover_peer(addr1):
        raise Exception("Device discovery timed out")
    dev[0].p2p_stop_find()
    peer = dev[0].get_peer(addr1)
    if peer['config_methods'] != '0x8':
        raise Exception("Unexpected peer config methods(2): " + peer['config_methods'])

    wpas.p2p_stop_find()

@remote_compatible
def test_discovery_after_gas(dev, apdev):
    """P2P device discovery after GAS/ANQP exchange"""
    hapd = start_ap(apdev[0])
    hapd.set("gas_frag_limit", "50")
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412", force_scan=True)
    dev[0].request("FETCH_ANQP")
    ev = dev[0].wait_event(["ANQP-QUERY-DONE"], timeout=10)
    if ev is None:
        raise Exception("No ANQP-QUERY-DONE event")
    dev[0].dump_monitor()

    start = os.times()[4]
    dev[0].p2p_listen()
    dev[1].p2p_find(social=True)
    ev = dev[1].wait_global_event(["P2P-DEVICE-FOUND"], timeout=5)
    if ev is None:
        raise Exception("Peer not discovered")
    end = os.times()[4]
    dev[0].dump_monitor()
    dev[0].p2p_stop_find()
    dev[1].p2p_stop_find()
    logger.info("Device discovery after fragmented GAS took %f seconds" % (end - start))
    if end - start > 1.3:
        raise Exception("Device discovery took unexpectedly long time")

@remote_compatible
def test_discovery_listen_find(dev):
    """P2P_LISTEN immediately followed by P2P_FIND"""
    # Request an external remain-on-channel operation to delay start of the ROC
    # for the following p2p_listen() enough to get p2p_find() processed before
    # the ROC started event shows up. This is done to test a code path where the
    # p2p_find() needs to clear the wait for the pending listen operation
    # (p2p->pending_listen_freq).
    ifindex = int(dev[0].get_driver_status_field("ifindex"))
    nl80211_remain_on_channel(dev[0], ifindex, 2417, 200)

    addr0 = dev[0].p2p_dev_addr()
    dev[0].p2p_listen()
    dev[0].p2p_find(social=True)
    time.sleep(0.4)
    dev[1].p2p_listen()
    ev = dev[0].wait_global_event(["P2P-DEVICE-FOUND"], timeout=1.2)
    if not dev[1].discover_peer(addr0):
        raise Exception("Device discovery timed out")
    if ev is None:
        raise Exception("Did not find peer quickly enough after stopped P2P_LISTEN")

def test_discovery_long_listen(dev):
    """Long P2P_LISTEN and offchannel TX"""
    addr0 = dev[0].p2p_dev_addr()
    dev[0].p2p_listen()

    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    wpas.interface_add("wlan5")
    addr = wpas.p2p_dev_addr()
    if not wpas.discover_peer(addr0):
        raise Exception("Device discovery timed out")
    peer = wpas.get_peer(addr0)
    chan = '1' if peer['listen_freq'] == '2462' else '11'

    wpas.request("P2P_SET listen_channel " + chan)
    wpas.request("P2P_LISTEN 10")
    if not dev[0].discover_peer(addr):
        raise Exception("Device discovery timed out (2)")

    time.sleep(0.1)
    wpas.global_request("P2P_PROV_DISC " + addr0 + " display")
    ev = dev[0].wait_global_event(["P2P-PROV-DISC-SHOW-PIN"], timeout=15)
    if ev is None:
        raise Exception("Provision discovery timed out")
    dev[0].p2p_stop_find()

    # Verify that the long listen period is still continuing after off-channel
    # TX of Provision Discovery frames.
    if not dev[1].discover_peer(addr):
        raise Exception("Device discovery timed out (3)")

    dev[1].p2p_stop_find()
    wpas.p2p_stop_find()

def test_discovery_long_listen2(dev):
    """Long P2P_LISTEN longer than remain-on-channel time"""
    with HWSimRadio(use_p2p_device=True) as (radio, iface):
        wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
        wpas.interface_add(iface)
        addr = wpas.p2p_dev_addr()
        wpas.request("P2P_LISTEN 15")

        # Wait for remain maximum remain-on-channel time to pass
        time.sleep(7)

        if not dev[0].discover_peer(addr):
            raise Exception("Device discovery timed out")
        dev[0].p2p_stop_find()
        wpas.p2p_stop_find()

def pd_test(dev, addr):
    if not dev.discover_peer(addr, freq=2412):
        raise Exception("Device discovery timed out")
    dev.global_request("P2P_PROV_DISC " + addr + " display")
    ev0 = dev.wait_global_event(["P2P-PROV-DISC-ENTER-PIN"], timeout=15)
    if ev0 is None:
        raise Exception("Provision discovery timed out (display)")
    dev.p2p_stop_find()

def run_discovery_while_go(wpas, dev, params):
    wpas.request("P2P_SET listen_channel 1")
    wpas.p2p_start_go(freq="2412")
    addr = wpas.p2p_dev_addr()
    pin = dev[0].wps_read_pin()
    wpas.p2p_go_authorize_client(pin)
    dev[1].p2p_connect_group(addr, pin, freq=2412, timeout=30)

    pd_test(dev[0], addr)
    wpas.p2p_listen()
    pd_test(dev[2], addr)

    wpas.p2p_stop_find()
    terminate_group(wpas, dev[1])

    out = run_tshark(os.path.join(params['logdir'], "hwsim0.pcapng"),
                     "wifi_p2p.public_action.subtype == 8", ["wlan.da"])
    da = out.splitlines()
    logger.info("PD Response DAs: " + str(da))
    if len(da) != 3:
        raise Exception("Unexpected DA count for PD Response")

def test_discovery_while_go(dev, apdev, params):
    """P2P provision discovery from GO"""
    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    wpas.interface_add("wlan5")
    run_discovery_while_go(wpas, dev, params)

def test_discovery_while_go_p2p_dev(dev, apdev, params):
    """P2P provision discovery from GO (using P2P Device interface)"""
    with HWSimRadio(use_p2p_device=True) as (radio, iface):
        wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
        wpas.interface_add(iface)
        run_discovery_while_go(wpas, dev, params)

def run_discovery_while_cli(wpas, dev, params):
    wpas.request("P2P_SET listen_channel 1")
    dev[1].p2p_start_go(freq="2412")
    addr = wpas.p2p_dev_addr()
    pin = wpas.wps_read_pin()
    dev[1].p2p_go_authorize_client(pin)
    wpas.p2p_connect_group(dev[1].p2p_dev_addr(), pin, freq=2412, timeout=30)

    pd_test(dev[0], addr)
    wpas.p2p_listen()
    pd_test(dev[2], addr)

    wpas.p2p_stop_find()
    terminate_group(dev[1], wpas)

    out = run_tshark(os.path.join(params['logdir'], "hwsim0.pcapng"),
                     "wifi_p2p.public_action.subtype == 8", ["wlan.da"])
    da = out.splitlines()
    logger.info("PD Response DAs: " + str(da))
    if len(da) != 3:
        raise Exception("Unexpected DA count for PD Response")

def test_discovery_while_cli(dev, apdev, params):
    """P2P provision discovery from CLI"""
    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    wpas.interface_add("wlan5")
    run_discovery_while_cli(wpas, dev, params)

def test_discovery_while_cli_p2p_dev(dev, apdev, params):
    """P2P provision discovery from CLI (using P2P Device interface)"""
    with HWSimRadio(use_p2p_device=True) as (radio, iface):
        wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
        wpas.interface_add(iface)
        run_discovery_while_cli(wpas, dev, params)

def test_discovery_device_name_change(dev):
    """P2P device discovery and peer changing device name"""
    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    wpas.interface_add("wlan5")
    wpas.set("device_name", "test-a")
    wpas.p2p_listen()
    dev[0].p2p_find(social=True)
    ev = dev[0].wait_global_event(["P2P-DEVICE-FOUND"], timeout=15)
    if ev is None:
        raise Exception("Peer not found")
    if "new=1" not in ev:
        raise Exception("Incorrect new event: " + ev)
    if "name='test-a'" not in ev:
        raise Exception("Unexpected device name(1): " + ev)

    # Verify that new P2P-DEVICE-FOUND event is indicated when the peer changes
    # its device name.
    wpas.set("device_name", "test-b")
    ev = dev[0].wait_global_event(["P2P-DEVICE-FOUND"], timeout=15)
    if ev is None:
        raise Exception("Peer update not seen")
    if "new=0" not in ev:
        raise Exception("Incorrect update event: " + ev)
    if "name='test-b'" not in ev:
        raise Exception("Unexpected device name(2): " + ev)
    wpas.p2p_stop_find()
    dev[0].p2p_stop_find()

def test_p2p_group_cli_invalid(dev, apdev):
    """P2P device discovery with invalid group client info"""
    attr = struct.pack('<BHBB', 2, 2, 0x25, 0x09)

    attr += struct.pack('<BH', 3, 6) + "\x02\x02\x02\x02\x02\x00".encode()

    cli = bytes()
    cli += "\x02\x02\x02\x02\x02\x03".encode()
    cli += "\x02\x02\x02\x02\x02\x04".encode()
    cli += struct.pack('>BH', 0, 0x3148)
    dev_type = "\x00\x00\x00\x00\x00\x00\x00\x01".encode()
    cli += dev_type
    num_sec = 25
    cli += struct.pack('B', num_sec)
    cli += num_sec * dev_type
    name = "TEST".encode()
    cli += struct.pack('>HH', 0x1011, len(name)) + name
    desc = struct.pack('B', len(cli)) + cli
    attr += struct.pack('<BH', 14, len(desc)) + desc

    p2p_ie = struct.pack('>BBL', 0xdd, 4 + len(attr), 0x506f9a09) + attr
    ie = binascii.hexlify(p2p_ie).decode()

    params = {"ssid": "DIRECT-test",
              "eap_server": "1",
              "wps_state": "2",
              "wpa_passphrase": "12345678",
              "wpa": "2",
              "wpa_key_mgmt": "WPA-PSK",
              "rsn_pairwise": "CCMP",
              "vendor_elements": ie}
    hapd = hostapd.add_ap(apdev[0], params)

    for i in range(2):
        dev[i].p2p_find(social=True)
        ev = dev[i].wait_global_event(["P2P-DEVICE-FOUND"], timeout=5)
        if not ev:
            raise Exception("P2P device not found")

def test_discovery_max_peers(dev):
    """P2P device discovery and maximum peer limit exceeded"""
    dev[0].p2p_listen()
    dev[0].request("SET ext_mgmt_frame_handling 1")
    probereq1 = "40000000ffffffffffff"
    probereq2 = "ffffffffffff000000074449524543542d01080c1218243048606c0301012d1afe131bffff000000000000000000000100000000000000000000ff16230178c812400000bfce0000000000000000fafffaffdd730050f204104a000110103a00010110080002314810470010572cf82fc95756539b16b5cfb298abf1105400080000000000000000103c0001031002000200001009000200001012000200001021000120102300012010240001201011000844657669636520421049000900372a000120030101dd11506f9a0902020025000605005858045106"

    # Fill the P2P peer table with max+1 entries based on Probe Request frames
    # to verify correct behavior on# removing the oldest entry when running out
    # of room.
    for i in range(101):
        addr = "0202020202%02x" % i
        probereq = probereq1 + addr + probereq2
        if "OK" not in dev[0].request("MGMT_RX_PROCESS freq=2412 datarate=60 ssi_signal=-30 frame=" + probereq):
            raise Exception("MGMT_RX_PROCESS failed")

    res = dev[0].global_request("P2P_PEER FIRST")
    addr = res.splitlines()[0]
    peers = [addr]
    limit = 200
    while limit > 0:
        res = dev[0].global_request("P2P_PEER NEXT-" + addr)
        addr = res.splitlines()[0]
        if addr == "FAIL":
            break
        peers.append(addr)
        limit -= 1
    logger.info("Peers: " + str(peers))

    if len(peers) != 100:
        raise Exception("Unexpected number of peer entries")
    oldest = "02:02:02:02:02:00"
    if oldest in peers:
        raise Exception("Oldest entry is still present")
    for i in range(101):
        addr = "02:02:02:02:02:%02x" % i
        if addr == oldest:
            continue
        if addr not in peers:
            raise Exception("Peer missing from table: " + addr)

    # Provision Discovery Request from the oldest peer (SA) using internally
    # different P2P Device Address as a regression test for incorrect processing
    # for this corner case.
    dst = dev[0].own_addr().replace(':', '')
    src = peers[99].replace(':', '')
    devaddr = "0202020202ff"
    pdreq = "d0004000" + dst + src + dst + "d0000409506f9a090701dd29506f9a0902020025000d1d00" + devaddr + "1108000000000000000000101100084465766963652041dd0a0050f204100800020008"
    if "OK" not in dev[0].request("MGMT_RX_PROCESS freq=2412 datarate=60 ssi_signal=-30 frame=" + pdreq):
        raise Exception("MGMT_RX_PROCESS failed")
