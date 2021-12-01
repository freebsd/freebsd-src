# P2P autonomous GO test cases
# Copyright (c) 2013-2015, Jouni Malinen <j@w1.fi>
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

from remotehost import remote_compatible
import time
import subprocess
import logging
logger = logging.getLogger()

import hostapd
import hwsim_utils
import utils
from utils import HwsimSkip
from wlantest import Wlantest
from wpasupplicant import WpaSupplicant
from p2p_utils import *
from test_p2p_messages import mgmt_tx, parse_p2p_public_action

def test_autogo(dev):
    """P2P autonomous GO and client joining group"""
    addr0 = dev[0].p2p_dev_addr()
    addr2 = dev[2].p2p_dev_addr()
    res = autogo(dev[0])
    if "p2p-wlan" in res['ifname']:
        raise Exception("Unexpected group interface name on GO")
    res = connect_cli(dev[0], dev[1])
    if "p2p-wlan" in res['ifname']:
        raise Exception("Unexpected group interface name on client")
    bss = dev[1].get_bss("p2p_dev_addr=" + addr0, res['ifname'])
    if not bss or bss['bssid'] != dev[0].p2p_interface_addr():
        raise Exception("Unexpected BSSID in the BSS entry for the GO")
    id = bss['id']
    bss = dev[1].get_bss("ID-" + id, res['ifname'])
    if not bss or bss['id'] != id:
        raise Exception("Could not find BSS entry based on id")
    res = dev[1].group_request("BSS RANGE=" + id + "- MASK=0x1")
    if "id=" + id not in res:
        raise Exception("Could not find BSS entry based on id range")

    res = dev[1].request("SCAN_RESULTS")
    if "[P2P]" not in res:
        raise Exception("P2P flag missing from scan results: " + res)

    # Presence request to increase testing coverage
    if "FAIL" not in dev[1].group_request("P2P_PRESENCE_REQ 30000"):
        raise Exception("Invald P2P_PRESENCE_REQ accepted")
    if "FAIL" not in dev[1].group_request("P2P_PRESENCE_REQ 30000 102400 30001"):
        raise Exception("Invald P2P_PRESENCE_REQ accepted")
    if "FAIL" in dev[1].group_request("P2P_PRESENCE_REQ 30000 102400"):
        raise Exception("Could not send presence request")
    ev = dev[1].wait_group_event(["P2P-PRESENCE-RESPONSE"], 10)
    if ev is None:
        raise Exception("Timeout while waiting for Presence Response")
    if "FAIL" in dev[1].group_request("P2P_PRESENCE_REQ 30000 102400 20000 102400"):
        raise Exception("Could not send presence request")
    ev = dev[1].wait_group_event(["P2P-PRESENCE-RESPONSE"])
    if ev is None:
        raise Exception("Timeout while waiting for Presence Response")
    if "FAIL" in dev[1].group_request("P2P_PRESENCE_REQ"):
        raise Exception("Could not send presence request")
    ev = dev[1].wait_group_event(["P2P-PRESENCE-RESPONSE"])
    if ev is None:
        raise Exception("Timeout while waiting for Presence Response")

    if not dev[2].discover_peer(addr0):
        raise Exception("Could not discover GO")
    dev[0].dump_monitor()
    dev[2].global_request("P2P_PROV_DISC " + addr0 + " display join")
    ev = dev[0].wait_global_event(["P2P-PROV-DISC-SHOW-PIN"], timeout=10)
    if ev is None:
        raise Exception("GO did not report P2P-PROV-DISC-SHOW-PIN")
    if "p2p_dev_addr=" + addr2 not in ev:
        raise Exception("Unexpected P2P Device Address in event: " + ev)
    if "group=" + dev[0].group_ifname not in ev:
        raise Exception("Unexpected group interface in event: " + ev)
    ev = dev[2].wait_global_event(["P2P-PROV-DISC-ENTER-PIN"], timeout=10)
    if ev is None:
        raise Exception("P2P-PROV-DISC-ENTER-PIN not reported")

    dev[0].remove_group()
    dev[1].wait_go_ending_session()

def test_autogo2(dev):
    """P2P autonomous GO with a separate group interface and client joining group"""
    dev[0].global_request("SET p2p_no_group_iface 0")
    res = autogo(dev[0], freq=2437)
    if "p2p-wlan" not in res['ifname']:
        raise Exception("Unexpected group interface name on GO")
    if res['ifname'] not in utils.get_ifnames():
        raise Exception("Could not find group interface netdev")
    connect_cli(dev[0], dev[1], social=True, freq=2437)
    dev[0].remove_group()
    dev[1].wait_go_ending_session()
    if res['ifname'] in utils.get_ifnames():
        raise Exception("Group interface netdev was not removed")

def test_autogo3(dev):
    """P2P autonomous GO and client with a separate group interface joining group"""
    dev[1].global_request("SET p2p_no_group_iface 0")
    autogo(dev[0], freq=2462)
    res = connect_cli(dev[0], dev[1], social=True, freq=2462)
    if "p2p-wlan" not in res['ifname']:
        raise Exception("Unexpected group interface name on client")
    if res['ifname'] not in utils.get_ifnames():
        raise Exception("Could not find group interface netdev")
    dev[0].remove_group()
    dev[1].wait_go_ending_session()
    dev[1].ping()
    if res['ifname'] in utils.get_ifnames():
        raise Exception("Group interface netdev was not removed")

def test_autogo4(dev):
    """P2P autonomous GO and client joining group (both with a separate group interface)"""
    dev[0].global_request("SET p2p_no_group_iface 0")
    dev[1].global_request("SET p2p_no_group_iface 0")
    res1 = autogo(dev[0], freq=2412)
    res2 = connect_cli(dev[0], dev[1], social=True, freq=2412)
    if "p2p-wlan" not in res1['ifname']:
        raise Exception("Unexpected group interface name on GO")
    if "p2p-wlan" not in res2['ifname']:
        raise Exception("Unexpected group interface name on client")
    ifnames = utils.get_ifnames()
    if res1['ifname'] not in ifnames:
        raise Exception("Could not find GO group interface netdev")
    if res2['ifname'] not in ifnames:
        raise Exception("Could not find client group interface netdev")
    dev[0].remove_group()
    dev[1].wait_go_ending_session()
    dev[1].ping()
    ifnames = utils.get_ifnames()
    if res1['ifname'] in ifnames:
        raise Exception("GO group interface netdev was not removed")
    if res2['ifname'] in ifnames:
        raise Exception("Client group interface netdev was not removed")

def test_autogo_m2d(dev):
    """P2P autonomous GO and clients not authorized"""
    autogo(dev[0], freq=2412)
    go_addr = dev[0].p2p_dev_addr()

    dev[1].global_request("SET p2p_no_group_iface 0")
    if not dev[1].discover_peer(go_addr, social=True):
        raise Exception("GO " + go_addr + " not found")
    dev[1].dump_monitor()

    if not dev[2].discover_peer(go_addr, social=True):
        raise Exception("GO " + go_addr + " not found")
    dev[2].dump_monitor()

    logger.info("Trying to join the group when GO has not authorized the client")
    pin = dev[1].wps_read_pin()
    cmd = "P2P_CONNECT " + go_addr + " " + pin + " join"
    if "OK" not in dev[1].global_request(cmd):
        raise Exception("P2P_CONNECT join failed")

    pin = dev[2].wps_read_pin()
    cmd = "P2P_CONNECT " + go_addr + " " + pin + " join"
    if "OK" not in dev[2].global_request(cmd):
        raise Exception("P2P_CONNECT join failed")

    ev = dev[1].wait_global_event(["WPS-M2D"], timeout=16)
    if ev is None:
        raise Exception("No global M2D event")
    ifaces = dev[1].request("INTERFACES").splitlines()
    iface = ifaces[0] if "p2p-wlan" in ifaces[0] else ifaces[1]
    wpas = WpaSupplicant(ifname=iface)
    ev = wpas.wait_event(["WPS-M2D"], timeout=10)
    if ev is None:
        raise Exception("No M2D event on group interface")

    ev = dev[2].wait_global_event(["WPS-M2D"], timeout=10)
    if ev is None:
        raise Exception("No global M2D event (2)")
    ev = dev[2].wait_event(["WPS-M2D"], timeout=10)
    if ev is None:
        raise Exception("No M2D event on group interface (2)")

@remote_compatible
def test_autogo_fail(dev):
    """P2P autonomous GO and incorrect PIN"""
    autogo(dev[0], freq=2412)
    go_addr = dev[0].p2p_dev_addr()
    dev[0].p2p_go_authorize_client("00000000")

    dev[1].global_request("SET p2p_no_group_iface 0")
    if not dev[1].discover_peer(go_addr, social=True):
        raise Exception("GO " + go_addr + " not found")
    dev[1].dump_monitor()

    logger.info("Trying to join the group when GO has not authorized the client")
    pin = dev[1].wps_read_pin()
    cmd = "P2P_CONNECT " + go_addr + " " + pin + " join"
    if "OK" not in dev[1].global_request(cmd):
        raise Exception("P2P_CONNECT join failed")

    ev = dev[1].wait_global_event(["WPS-FAIL"], timeout=10)
    if ev is None:
        raise Exception("No global WPS-FAIL event")

def test_autogo_2cli(dev):
    """P2P autonomous GO and two clients joining group"""
    autogo(dev[0], freq=2412)
    connect_cli(dev[0], dev[1], social=True, freq=2412)
    connect_cli(dev[0], dev[2], social=True, freq=2412)
    hwsim_utils.test_connectivity_p2p(dev[1], dev[2])
    dev[0].global_request("P2P_REMOVE_CLIENT " + dev[1].p2p_dev_addr())
    dev[1].wait_go_ending_session()
    dev[0].global_request("P2P_REMOVE_CLIENT iface=" + dev[2].p2p_interface_addr())
    dev[2].wait_go_ending_session()
    if "FAIL" not in dev[0].global_request("P2P_REMOVE_CLIENT foo"):
        raise Exception("Invalid P2P_REMOVE_CLIENT command accepted")
    dev[0].remove_group()

def test_autogo_pbc(dev):
    """P2P autonomous GO and PBC"""
    dev[1].global_request("SET p2p_no_group_iface 0")
    autogo(dev[0], freq=2412)
    if "FAIL" not in dev[0].group_request("WPS_PBC p2p_dev_addr=00:11:22:33:44"):
        raise Exception("Invalid WPS_PBC succeeded")
    if "OK" not in dev[0].group_request("WPS_PBC p2p_dev_addr=" + dev[1].p2p_dev_addr()):
        raise Exception("WPS_PBC failed")
    dev[2].p2p_connect_group(dev[0].p2p_dev_addr(), "pbc", timeout=0,
                             social=True)
    ev = dev[2].wait_global_event(["WPS-M2D"], timeout=15)
    if ev is None:
        raise Exception("WPS-M2D not reported")
    if "config_error=12" not in ev:
        raise Exception("Unexpected config_error: " + ev)
    dev[1].p2p_connect_group(dev[0].p2p_dev_addr(), "pbc", timeout=15,
                             social=True)

def test_autogo_pbc_session_overlap(dev, apdev):
    """P2P autonomous GO and PBC session overlap"""
    params = {"ssid": "wps", "eap_server": "1", "wps_state": "1"}
    hapd = hostapd.add_ap(apdev[0], params)
    hapd.request("WPS_PBC")
    bssid = hapd.own_addr()
    time.sleep(0.1)

    dev[0].scan_for_bss(bssid, freq=2412)
    dev[1].scan_for_bss(bssid, freq=2412)

    dev[1].global_request("SET p2p_no_group_iface 0")
    autogo(dev[0], freq=2412)
    if "OK" not in dev[0].group_request("WPS_PBC p2p_dev_addr=" + dev[1].p2p_dev_addr()):
        raise Exception("WPS_PBC failed")
    dev[1].p2p_connect_group(dev[0].p2p_dev_addr(), "pbc", timeout=15,
                             social=True)
    hapd.disable()
    remove_group(dev[0], dev[1])
    dev[0].flush_scan_cache()
    dev[1].flush_scan_cache()

def test_autogo_tdls(dev):
    """P2P autonomous GO and two clients using TDLS"""
    go = dev[0]
    logger.info("Start autonomous GO with fixed parameters " + go.ifname)
    id = go.add_network()
    go.set_network_quoted(id, "ssid", "DIRECT-tdls")
    go.set_network_quoted(id, "psk", "12345678")
    go.set_network(id, "mode", "3")
    go.set_network(id, "disabled", "2")
    res = go.p2p_start_go(persistent=id, freq="2462")
    logger.debug("res: " + str(res))
    Wlantest.setup(go, True)
    wt = Wlantest()
    wt.flush()
    wt.add_passphrase("12345678")
    connect_cli(go, dev[1], social=True, freq=2462)
    connect_cli(go, dev[2], social=True, freq=2462)
    hwsim_utils.test_connectivity_p2p(dev[1], dev[2])
    bssid = dev[0].p2p_interface_addr()
    addr1 = dev[1].p2p_interface_addr()
    addr2 = dev[2].p2p_interface_addr()
    dev[1].tdls_setup(addr2)
    time.sleep(1)
    hwsim_utils.test_connectivity_p2p(dev[1], dev[2])
    conf = wt.get_tdls_counter("setup_conf_ok", bssid, addr1, addr2)
    if conf == 0:
        raise Exception("No TDLS Setup Confirm (success) seen")
    dl = wt.get_tdls_counter("valid_direct_link", bssid, addr1, addr2)
    if dl == 0:
        raise Exception("No valid frames through direct link")
    wt.tdls_clear(bssid, addr1, addr2)
    dev[1].tdls_teardown(addr2)
    time.sleep(1)
    teardown = wt.get_tdls_counter("teardown", bssid, addr1, addr2)
    if teardown == 0:
        raise Exception("No TDLS Setup Teardown seen")
    wt.tdls_clear(bssid, addr1, addr2)
    hwsim_utils.test_connectivity_p2p(dev[1], dev[2])
    ap_path = wt.get_tdls_counter("valid_ap_path", bssid, addr1, addr2)
    if ap_path == 0:
        raise Exception("No valid frames via AP path")
    direct_link = wt.get_tdls_counter("valid_direct_link", bssid, addr1, addr2)
    if direct_link > 0:
        raise Exception("Unexpected frames through direct link")
    idirect_link = wt.get_tdls_counter("invalid_direct_link", bssid, addr1,
                                       addr2)
    if idirect_link > 0:
        raise Exception("Unexpected frames through direct link (invalid)")
    dev[2].remove_group()
    dev[1].remove_group()
    dev[0].remove_group()

def test_autogo_legacy(dev):
    """P2P autonomous GO and legacy clients"""
    res = autogo(dev[0], freq=2462)
    if dev[0].get_group_status_field("passphrase", extra="WPS") != res['passphrase']:
        raise Exception("passphrase mismatch")
    if dev[0].group_request("P2P_GET_PASSPHRASE") != res['passphrase']:
        raise Exception("passphrase mismatch(2)")

    logger.info("Connect P2P client")
    connect_cli(dev[0], dev[1], social=True, freq=2462)

    if "FAIL" not in dev[1].request("P2P_GET_PASSPHRASE"):
        raise Exception("P2P_GET_PASSPHRASE succeeded on P2P Client")

    logger.info("Connect legacy WPS client")
    pin = dev[2].wps_read_pin()
    dev[0].p2p_go_authorize_client(pin)
    dev[2].request("P2P_SET disabled 1")
    dev[2].dump_monitor()
    dev[2].request("WPS_PIN any " + pin)
    dev[2].wait_connected(timeout=30)
    status = dev[2].get_status()
    if status['wpa_state'] != 'COMPLETED':
        raise Exception("Not fully connected")
    hwsim_utils.test_connectivity_p2p_sta(dev[1], dev[2])
    dev[2].request("DISCONNECT")

    logger.info("Connect legacy non-WPS client")
    dev[2].request("FLUSH")
    dev[2].request("P2P_SET disabled 1")
    dev[2].connect(ssid=res['ssid'], psk=res['passphrase'], proto='RSN',
                   key_mgmt='WPA-PSK', pairwise='CCMP', group='CCMP',
                   scan_freq=res['freq'])
    hwsim_utils.test_connectivity_p2p_sta(dev[1], dev[2])
    dev[2].request("DISCONNECT")

    dev[0].remove_group()
    dev[1].wait_go_ending_session()

def test_autogo_chan_switch(dev):
    """P2P autonomous GO switching channels"""
    run_autogo_chan_switch(dev)

def run_autogo_chan_switch(dev):
    autogo(dev[0], freq=2417)
    connect_cli(dev[0], dev[1], freq=2417)
    res = dev[0].group_request("CHAN_SWITCH 5 2422")
    if "FAIL" in res:
        # for now, skip test since mac80211_hwsim support is not yet widely
        # deployed
        raise HwsimSkip("Assume mac80211_hwsim did not support channel switching")
    ev = dev[0].wait_group_event(["AP-CSA-FINISHED"], timeout=10)
    if ev is None:
        raise Exception("CSA finished event timed out")
    if "freq=2422" not in ev:
        raise Exception("Unexpected cahnnel in CSA finished event")
    dev[0].dump_monitor()
    dev[1].dump_monitor()
    time.sleep(0.1)
    hwsim_utils.test_connectivity_p2p(dev[0], dev[1])

    dev[0].remove_group()
    dev[1].wait_go_ending_session()

def test_autogo_chan_switch_group_iface(dev):
    """P2P autonomous GO switching channels (separate group interface)"""
    dev[0].global_request("SET p2p_no_group_iface 0")
    run_autogo_chan_switch(dev)

@remote_compatible
def test_autogo_extra_cred(dev):
    """P2P autonomous GO sending two WPS credentials"""
    if "FAIL" in dev[0].request("SET wps_testing_stub_cred 1"):
        raise Exception("Failed to enable test mode")
    autogo(dev[0], freq=2412)
    connect_cli(dev[0], dev[1], social=True, freq=2412)
    dev[0].remove_group()
    dev[1].wait_go_ending_session()

def test_autogo_ifdown(dev):
    """P2P autonomous GO and external ifdown"""
    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    wpas.interface_add("wlan5")
    res = autogo(wpas)
    wpas.dump_monitor()
    wpas.interface_remove("wlan5")
    wpas.interface_add("wlan5")
    res = autogo(wpas)
    wpas.dump_monitor()
    subprocess.call(['ifconfig', res['ifname'], 'down'])
    ev = wpas.wait_global_event(["P2P-GROUP-REMOVED"], timeout=10)
    if ev is None:
        raise Exception("Group removal not reported")
    if res['ifname'] not in ev:
        raise Exception("Unexpected group removal event: " + ev)

@remote_compatible
def test_autogo_start_during_scan(dev):
    """P2P autonomous GO started during ongoing manual scan"""
    try:
        # use autoscan to set scan_req = MANUAL_SCAN_REQ
        if "OK" not in dev[0].request("AUTOSCAN periodic:1"):
            raise Exception("Failed to set autoscan")
        autogo(dev[0], freq=2462)
        connect_cli(dev[0], dev[1], social=True, freq=2462)
        dev[0].remove_group()
        dev[1].wait_go_ending_session()
    finally:
        dev[0].request("AUTOSCAN ")

def test_autogo_passphrase_len(dev):
    """P2P autonomous GO and longer passphrase"""
    try:
        if "OK" not in dev[0].request("SET p2p_passphrase_len 13"):
            raise Exception("Failed to set passphrase length")
        res = autogo(dev[0], freq=2412)
        if len(res['passphrase']) != 13:
            raise Exception("Unexpected passphrase length")
        if dev[0].get_group_status_field("passphrase", extra="WPS") != res['passphrase']:
            raise Exception("passphrase mismatch")

        logger.info("Connect P2P client")
        connect_cli(dev[0], dev[1], social=True, freq=2412)

        logger.info("Connect legacy WPS client")
        pin = dev[2].wps_read_pin()
        dev[0].p2p_go_authorize_client(pin)
        dev[2].request("P2P_SET disabled 1")
        dev[2].dump_monitor()
        dev[2].request("WPS_PIN any " + pin)
        dev[2].wait_connected(timeout=30)
        status = dev[2].get_status()
        if status['wpa_state'] != 'COMPLETED':
            raise Exception("Not fully connected")
        dev[2].request("DISCONNECT")

        logger.info("Connect legacy non-WPS client")
        dev[2].request("FLUSH")
        dev[2].request("P2P_SET disabled 1")
        dev[2].connect(ssid=res['ssid'], psk=res['passphrase'], proto='RSN',
                       key_mgmt='WPA-PSK', pairwise='CCMP', group='CCMP',
                       scan_freq=res['freq'])
        hwsim_utils.test_connectivity_p2p_sta(dev[1], dev[2])
        dev[2].request("DISCONNECT")

        dev[0].remove_group()
        dev[1].wait_go_ending_session()
    finally:
        dev[0].request("SET p2p_passphrase_len 8")

@remote_compatible
def test_autogo_bridge(dev):
    """P2P autonomous GO in a bridge"""
    try:
        # use autoscan to set scan_req = MANUAL_SCAN_REQ
        if "OK" not in dev[0].request("AUTOSCAN periodic:1"):
            raise Exception("Failed to set autoscan")
        autogo(dev[0])
        ifname = dev[0].get_group_ifname()
        dev[0].cmd_execute(['brctl', 'addbr', 'p2p-br0'])
        dev[0].cmd_execute(['brctl', 'setfd', 'p2p-br0', '0'])
        dev[0].cmd_execute(['brctl', 'addif', 'p2p-br0', ifname])
        dev[0].cmd_execute(['ip', 'link', 'set', 'dev', 'p2p-br0', 'up'])
        time.sleep(0.1)
        dev[0].cmd_execute(['brctl', 'delif', 'p2p-br0', ifname])
        time.sleep(0.1)
        dev[0].cmd_execute(['ip', 'link', 'set', 'dev', 'p2p-br0', 'down'])
        time.sleep(0.1)
        dev[0].cmd_execute(['brctl', 'delbr', 'p2p-br0'])
        ev = dev[0].wait_global_event(["P2P-GROUP-REMOVED"], timeout=1)
        if ev is not None:
            raise Exception("P2P group removed unexpectedly")
        if dev[0].get_group_status_field('wpa_state') != "COMPLETED":
            raise Exception("Unexpected wpa_state")
        dev[0].remove_group()
    finally:
        dev[0].request("AUTOSCAN ")
        dev[0].cmd_execute(['brctl', 'delif', 'p2p-br0', ifname,
                            '2>', '/dev/null'], shell=True)
        dev[0].cmd_execute(['ip', 'link', 'set', 'dev', 'p2p-br0', 'down',
                            '2>', '/dev/null'], shell=True)
        dev[0].cmd_execute(['brctl', 'delbr', 'p2p-br0', '2>', '/dev/null'],
                           shell=True)

@remote_compatible
def test_presence_req_on_group_interface(dev):
    """P2P_PRESENCE_REQ on group interface"""
    dev[1].global_request("SET p2p_no_group_iface 0")
    res = autogo(dev[0], freq=2437)
    res = connect_cli(dev[0], dev[1], social=True, freq=2437)
    if "FAIL" in dev[1].group_request("P2P_PRESENCE_REQ 30000 102400"):
        raise Exception("Could not send presence request")
    ev = dev[1].wait_group_event(["P2P-PRESENCE-RESPONSE"])
    if ev is None:
        raise Exception("Timeout while waiting for Presence Response")
    dev[0].remove_group()
    dev[1].wait_go_ending_session()

def test_autogo_join_auto_go_not_found(dev):
    """P2P_CONNECT-auto not finding GO"""
    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    wpas.interface_add("wlan5")
    wpas.request("P2P_SET listen_channel 1")
    wpas.global_request("SET p2p_no_group_iface 0")
    autogo(wpas, freq=2412)
    addr = wpas.p2p_dev_addr()
    bssid = wpas.p2p_interface_addr()
    wpas.dump_monitor()

    dev[1].global_request("SET p2p_no_group_iface 0")
    dev[1].scan_for_bss(bssid, freq=2412)
    # This makes the GO not show up in the scan iteration following the
    # P2P_CONNECT command by stopping beaconing and handling Probe Request
    # frames externally (but not really replying to them). P2P listen mode is
    # needed to keep the GO listening on the operating channel for the PD
    # exchange.
    if "OK" not in wpas.group_request("STOP_AP"):
        raise Exception("STOP_AP failed")
    wpas.dump_monitor()
    wpas.group_request("SET ext_mgmt_frame_handling 1")
    wpas.p2p_listen()
    wpas.dump_monitor()
    time.sleep(0.02)
    dev[1].global_request("P2P_CONNECT " + addr + " pbc auto")

    ev = dev[1].wait_global_event(["P2P-FALLBACK-TO-GO-NEG-ENABLED"], 15)
    wpas.dump_monitor()
    if ev is None:
        raise Exception("Could not trigger old-scan-only case")
        return

    ev = dev[1].wait_global_event(["P2P-FALLBACK-TO-GO-NEG"], 15)
    wpas.remove_group()
    if ev is None:
        raise Exception("Fallback to GO Negotiation not seen")
    if "reason=GO-not-found" not in ev:
        raise Exception("Unexpected reason for fallback: " + ev)
    wpas.dump_monitor()

def test_autogo_join_auto(dev):
    """P2P_CONNECT-auto joining a group"""
    autogo(dev[0])
    addr = dev[0].p2p_dev_addr()
    if "OK" not in dev[1].global_request("P2P_CONNECT " + addr + " pbc auto"):
        raise Exception("P2P_CONNECT failed")

    ev = dev[0].wait_global_event(["P2P-PROV-DISC-PBC-REQ"], timeout=15)
    if ev is None:
        raise Exception("Timeout on P2P-PROV-DISC-PBC-REQ")
    if "group=" + dev[0].group_ifname not in ev:
        raise Exception("Unexpected PD event contents: " + ev)
    dev[0].group_request("WPS_PBC")

    ev = dev[1].wait_global_event(["P2P-GROUP-STARTED"], timeout=15)
    if ev is None:
        raise Exception("Joining the group timed out")
    dev[1].group_form_result(ev)

    dev[0].remove_group()
    dev[1].wait_go_ending_session()
    dev[1].flush_scan_cache()

@remote_compatible
def test_autogo_join_auto_go_neg(dev):
    """P2P_CONNECT-auto fallback to GO Neg"""
    dev[1].flush_scan_cache()
    dev[0].p2p_listen()
    addr = dev[0].p2p_dev_addr()
    if not dev[1].discover_peer(addr, social=True):
        raise Exception("Peer not found")
    dev[1].p2p_stop_find()
    if "OK" not in dev[1].global_request("P2P_CONNECT " + addr + " pbc auto"):
        raise Exception("P2P_CONNECT failed")

    ev = dev[0].wait_global_event(["P2P-GO-NEG-REQUEST"], timeout=15)
    if ev is None:
        raise Exception("Timeout on P2P-GO-NEG-REQUEST")
    peer = ev.split(' ')[1]
    dev[0].p2p_go_neg_init(peer, None, "pbc", timeout=15, go_intent=15)

    ev = dev[1].wait_global_event(["P2P-FALLBACK-TO-GO-NEG"], timeout=1)
    if ev is None:
        raise Exception("No P2P-FALLBACK-TO-GO-NEG event seen")
    if "P2P-FALLBACK-TO-GO-NEG-ENABLED" in ev:
        ev = dev[1].wait_global_event(["P2P-FALLBACK-TO-GO-NEG"], timeout=1)
        if ev is None:
            raise Exception("No P2P-FALLBACK-TO-GO-NEG event seen")
    if "reason=peer-not-running-GO" not in ev:
        raise Exception("Unexpected reason: " + ev)

    ev = dev[1].wait_global_event(["P2P-GROUP-STARTED"], timeout=15)
    if ev is None:
        raise Exception("Joining the group timed out")
    dev[1].group_form_result(ev)

    dev[0].remove_group()
    dev[1].wait_go_ending_session()
    dev[1].flush_scan_cache()

@remote_compatible
def test_autogo_join_auto_go_neg_after_seeing_go(dev):
    """P2P_CONNECT-auto fallback to GO Neg after seeing GO"""
    autogo(dev[0], freq=2412)
    addr = dev[0].p2p_dev_addr()
    bssid = dev[0].p2p_interface_addr()
    dev[1].scan_for_bss(bssid, freq=2412)
    dev[0].remove_group()
    dev[0].p2p_listen()

    if "OK" not in dev[1].global_request("P2P_CONNECT " + addr + " pbc auto"):
        raise Exception("P2P_CONNECT failed")

    ev = dev[1].wait_global_event(["P2P-FALLBACK-TO-GO-NEG-ENABLED"],
                                  timeout=15)
    if ev is None:
        raise Exception("No P2P-FALLBACK-TO-GO-NEG-ENABLED event seen")

    ev = dev[0].wait_global_event(["P2P-GO-NEG-REQUEST"], timeout=15)
    if ev is None:
        raise Exception("Timeout on P2P-GO-NEG-REQUEST")
    peer = ev.split(' ')[1]
    dev[0].p2p_go_neg_init(peer, None, "pbc", timeout=15, go_intent=15)

    ev = dev[1].wait_global_event(["P2P-FALLBACK-TO-GO-NEG"], timeout=1)
    if ev is None:
        raise Exception("No P2P-FALLBACK-TO-GO-NEG event seen")
    if "reason=no-ACK-to-PD-Req" not in ev and "reason=PD-failed" not in ev:
        raise Exception("Unexpected reason: " + ev)

    ev = dev[1].wait_global_event(["P2P-GROUP-STARTED"], timeout=15)
    if ev is None:
        raise Exception("Joining the group timed out")
    dev[1].group_form_result(ev)

    dev[0].remove_group()
    dev[1].wait_go_ending_session()
    dev[1].flush_scan_cache()

def test_go_search_non_social(dev):
    """P2P_FIND with freq parameter to scan a single channel"""
    addr0 = dev[0].p2p_dev_addr()
    autogo(dev[0], freq=2422)
    dev[1].p2p_find(freq=2422)
    ev = dev[1].wait_global_event(["P2P-DEVICE-FOUND"], timeout=3.5)
    if ev is None:
        dev[1].p2p_stop_find()
        dev[1].p2p_find(freq=2422)
        ev = dev[1].wait_global_event(["P2P-DEVICE-FOUND"], timeout=3.5)
        if ev is None:
            raise Exception("Did not find GO quickly enough")
    dev[2].p2p_listen()
    ev = dev[1].wait_global_event(["P2P-DEVICE-FOUND"], timeout=5)
    if ev is None:
        raise Exception("Did not find peer")
    dev[2].p2p_stop_find()
    dev[1].p2p_stop_find()
    dev[0].remove_group()

def test_go_search_non_social2(dev):
    """P2P_FIND with freq parameter to scan a single channel (2)"""
    addr0 = dev[0].p2p_dev_addr()
    dev[1].p2p_find(freq=2422)
    # Wait for the first p2p_find scan round to complete before starting GO
    time.sleep(1)
    autogo(dev[0], freq=2422)
    # Verify that p2p_find is still scanning the specified frequency
    ev = dev[1].wait_global_event(["P2P-DEVICE-FOUND"], timeout=5)
    if ev is None:
        dev[1].p2p_stop_find()
        raise Exception("Did not find GO quickly enough")
    # Verify that p2p_find is scanning the social channels
    dev[2].p2p_listen()
    ev = dev[1].wait_global_event(["P2P-DEVICE-FOUND"], timeout=5)
    if ev is None:
        raise Exception("Did not find peer")
    dev[2].p2p_stop_find()
    dev[1].p2p_stop_find()
    dev[0].remove_group()
    dev[1].dump_monitor()

    # Verify that social channel as the specific channel works
    dev[1].p2p_find(freq=2412)
    time.sleep(0.5)
    dev[2].p2p_listen()
    ev = dev[1].wait_global_event(["P2P-DEVICE-FOUND"], timeout=5)
    if ev is None:
        raise Exception("Did not find peer (2)")

def test_autogo_many(dev):
    """P2P autonomous GO with large number of GO instances"""
    dev[0].global_request("SET p2p_no_group_iface 0")
    for i in range(100):
        if "OK" not in dev[0].global_request("P2P_GROUP_ADD freq=2412"):
            logger.info("Was able to add %d groups" % i)
            if i < 5:
                raise Exception("P2P_GROUP_ADD failed")
            stop_ev = dev[0].wait_global_event(["P2P-GROUP-REMOVE"], timeout=1)
            if stop_ev is not None:
                raise Exception("Unexpected P2P-GROUP-REMOVE event")
            break
        ev = dev[0].wait_global_event(["P2P-GROUP-STARTED"], timeout=5)
        if ev is None:
            raise Exception("GO start up timed out")
        dev[0].group_form_result(ev)

    for i in dev[0].global_request("INTERFACES").splitlines():
        dev[0].request("P2P_GROUP_REMOVE " + i)
        dev[0].dump_monitor()
    dev[0].request("P2P_GROUP_REMOVE *")

def test_autogo_many_clients(dev):
    """P2P autonomous GO and many clients (P2P IE fragmentation)"""
    try:
        _test_autogo_many_clients(dev)
    finally:
        dev[0].global_request("SET device_name Device A")
        dev[1].global_request("SET device_name Device B")
        dev[2].global_request("SET device_name Device C")

def _test_autogo_many_clients(dev):
    # These long device names will push the P2P IE contents beyond the limit
    # that requires fragmentation.
    name0 = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    name1 = "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB"
    name2 = "CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC"
    name3 = "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD"
    dev[0].global_request("SET device_name " + name0)
    dev[1].global_request("SET device_name " + name1)
    dev[2].global_request("SET device_name " + name2)

    addr0 = dev[0].p2p_dev_addr()
    res = autogo(dev[0], freq=2412)
    bssid = dev[0].p2p_interface_addr()

    connect_cli(dev[0], dev[1], social=True, freq=2412)
    dev[0].dump_monitor()
    connect_cli(dev[0], dev[2], social=True, freq=2412)
    dev[0].dump_monitor()

    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    wpas.interface_add("wlan5")
    wpas.global_request("SET device_name " + name3)
    wpas.global_request("SET sec_device_type 1-11111111-1")
    wpas.global_request("SET sec_device_type 2-22222222-2")
    wpas.global_request("SET sec_device_type 3-33333333-3")
    wpas.global_request("SET sec_device_type 4-44444444-4")
    wpas.global_request("SET sec_device_type 5-55555555-5")
    connect_cli(dev[0], wpas, social=True, freq=2412)
    dev[0].dump_monitor()

    dev[1].dump_monitor()
    dev[1].p2p_find(freq=2412)
    ev1 = dev[1].wait_global_event(["P2P-DEVICE-FOUND"], timeout=10)
    if ev1 is None:
        raise Exception("Could not find peer (1)")
    ev2 = dev[1].wait_global_event(["P2P-DEVICE-FOUND"], timeout=10)
    if ev2 is None:
        raise Exception("Could not find peer (2)")
    ev3 = dev[1].wait_global_event(["P2P-DEVICE-FOUND"], timeout=10)
    if ev3 is None:
        raise Exception("Could not find peer (3)")
    dev[1].p2p_stop_find()

    for i in [name0, name2, name3]:
        if i not in ev1 and i not in ev2 and i not in ev3:
            raise Exception('name "%s" not found' % i)

def rx_pd_req(dev):
    msg = dev.mgmt_rx()
    if msg is None:
        raise Exception("MGMT-RX timeout")
    p2p = parse_p2p_public_action(msg['payload'])
    if p2p is None:
        raise Exception("Not a P2P Public Action frame " + str(dialog_token))
    if p2p['subtype'] != P2P_PROV_DISC_REQ:
        raise Exception("Unexpected subtype %d" % p2p['subtype'])
    p2p['freq'] = msg['freq']
    return p2p

@remote_compatible
def test_autogo_scan(dev):
    """P2P autonomous GO and no P2P IE in Probe Response scan results"""
    addr0 = dev[0].p2p_dev_addr()
    addr1 = dev[1].p2p_dev_addr()
    dev[0].p2p_start_go(freq=2412, persistent=True)
    bssid = dev[0].p2p_interface_addr()

    dev[1].discover_peer(addr0)
    dev[1].p2p_stop_find()
    ev = dev[1].wait_global_event(["P2P-FIND-STOPPED"], timeout=2)
    time.sleep(0.1)
    dev[1].flush_scan_cache()

    pin = dev[1].wps_read_pin()
    dev[0].group_request("WPS_PIN any " + pin)

    try:
        dev[1].request("SET p2p_disabled 1")
        dev[1].request("SCAN freq=2412")
        ev = dev[1].wait_event(["CTRL-EVENT-SCAN-RESULTS"])
        if ev is None:
            raise Exception("Active scan did not complete")
    finally:
        dev[1].request("SET p2p_disabled 0")

    for i in range(2):
        dev[1].request("SCAN freq=2412 passive=1")
        ev = dev[1].wait_event(["CTRL-EVENT-SCAN-RESULTS"])
        if ev is None:
            raise Exception("Scan did not complete")

    # Disable management frame processing for a moment to skip Probe Response
    # frame with P2P IE.
    dev[0].group_request("SET ext_mgmt_frame_handling 1")

    dev[1].global_request("P2P_CONNECT " + bssid + " " + pin + " freq=2412 join")

    # Skip the first Probe Request frame
    ev = dev[0].wait_group_event(["MGMT-RX"], timeout=10)
    if ev is None:
        raise Exception("No Probe Request frame seen")
    if not ev.split(' ')[4].startswith("40"):
        raise Exception("Not a Probe Request frame")

    # If a P2P Device is not used, the PD Request will be received on the group
    # interface (which is actually wlan0, since a separate interface is not
    # used), which was set to external management frame handling, so need to
    # reply to it manually.
    res = dev[0].get_driver_status()
    if not (int(res['capa.flags'], 0) & 0x20000000):
        # Reply to PD Request while still filtering Probe Request frames
        msg = rx_pd_req(dev[0])
        mgmt_tx(dev[0], "MGMT_TX {} {} freq={} wait_time=10 no_cck=1 action={}".format(addr1, addr0, 2412, "0409506f9a0908%02xdd0a0050f204100800020008" % msg['dialog_token']))

    # Skip Probe Request frames until something else is received
    for i in range(10):
        ev = dev[0].wait_group_event(["MGMT-RX"], timeout=10)
        if ev is None:
            raise Exception("No frame seen")
        if not ev.split(' ')[4].startswith("40"):
            break

    # Allow wpa_supplicant to process authentication and association
    dev[0].group_request("SET ext_mgmt_frame_handling 0")

    # Joining the group should succeed and indicate persistent group based on
    # Beacon frame P2P IE.
    ev = dev[1].wait_global_event(["P2P-GROUP-STARTED"], timeout=10)
    if ev is None:
        raise Exception("Failed to join group")
    if "[PERSISTENT]" not in ev:
        raise Exception("Did not recognize group as persistent")
    dev[0].remove_group()
    dev[1].wait_go_ending_session()

@remote_compatible
def test_autogo_join_before_found(dev):
    """P2P client joining a group before having found GO Device Address"""
    dev[0].global_request("SET p2p_no_group_iface 0")
    res = autogo(dev[0], freq=2412)
    if "p2p-wlan" not in res['ifname']:
        raise Exception("Unexpected group interface name on GO")
    status = dev[0].get_group_status()
    bssid = status['bssid']

    pin = dev[1].wps_read_pin()
    dev[0].p2p_go_authorize_client(pin)
    cmd = "P2P_CONNECT " + bssid + " " + pin + " join freq=2412"
    if "OK" not in dev[1].global_request(cmd):
        raise Exception("P2P_CONNECT join failed")
    ev = dev[1].wait_global_event(["P2P-GROUP-STARTED"], timeout=15)
    if ev is None:
        raise Exception("Joining the group timed out")
    dev[0].remove_group()
    dev[1].wait_go_ending_session()

def test_autogo_noa(dev):
    """P2P autonomous GO and NoA"""
    res = autogo(dev[0])
    dev[0].group_request("P2P_SET noa 1,5,20")
    dev[0].group_request("P2P_SET noa 255,10,50")

    # Connect and disconnect legacy STA to check NoA special cases
    try:
        dev[1].request("SET p2p_disabled 1")
        dev[1].connect(ssid=res['ssid'], psk=res['passphrase'], proto='RSN',
                       key_mgmt='WPA-PSK', pairwise='CCMP', group='CCMP',
                       scan_freq=res['freq'])
        dev[0].group_request("P2P_SET noa 255,15,55")
        dev[1].request("DISCONNECT")
        dev[1].wait_disconnected()
    finally:
        dev[1].request("SET p2p_disabled 0")

    dev[0].group_request("P2P_SET noa 0,0,0")

def test_autogo_interworking(dev):
    """P2P autonomous GO and Interworking"""
    try:
        run_autogo_interworking(dev)
    finally:
        dev[0].set("go_interworking", "0")

def run_autogo_interworking(dev):
    dev[0].global_request("SET go_interworking 1")
    dev[0].global_request("SET go_access_network_type 1")
    dev[0].global_request("SET go_internet 1")
    dev[0].global_request("SET go_venue_group 2")
    dev[0].global_request("SET go_venue_type 3")
    res = autogo(dev[0])
    bssid = dev[0].p2p_interface_addr()
    dev[1].scan_for_bss(bssid, freq=res['freq'])
    bss = dev[1].get_bss(bssid)
    dev[0].remove_group()
    if '6b03110203' not in bss['ie']:
        raise Exception("Interworking element not seen")

def test_autogo_remove_iface(dev):
    """P2P autonomous GO and interface being removed"""
    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    wpas.interface_add("wlan5")
    wpas.global_request("SET p2p_no_group_iface 1")
    wpas.set("p2p_group_idle", "1")
    autogo(wpas)
    wpas.global_request("P2P_SET disallow_freq 5000")
    time.sleep(0.1)
    wpas.global_request("INTERFACE_REMOVE " + wpas.ifname)
    time.sleep(1)
