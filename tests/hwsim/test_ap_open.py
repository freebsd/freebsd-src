# Open mode AP tests
# Copyright (c) 2014, Qualcomm Atheros, Inc.
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

from remotehost import remote_compatible
import logging
logger = logging.getLogger()
import struct
import subprocess
import time
import os

import hostapd
import hwsim_utils
from tshark import run_tshark
from utils import *
from wpasupplicant import WpaSupplicant
from wlantest import WlantestCapture

@remote_compatible
def test_ap_open(dev, apdev):
    """AP with open mode (no security) configuration"""
    _test_ap_open(dev, apdev)

def _test_ap_open(dev, apdev):
    hapd = hostapd.add_ap(apdev[0], {"ssid": "open"})
    dev[0].connect("open", key_mgmt="NONE", scan_freq="2412",
                   bg_scan_period="0")
    ev = hapd.wait_event(["AP-STA-CONNECTED"], timeout=5)
    if ev is None:
        raise Exception("No connection event received from hostapd")
    hwsim_utils.test_connectivity(dev[0], hapd)

    dev[0].request("DISCONNECT")
    ev = hapd.wait_event(["AP-STA-DISCONNECTED"], timeout=5)
    if ev is None:
        raise Exception("No disconnection event received from hostapd")

def test_ap_open_packet_loss(dev, apdev):
    """AP with open mode configuration and large packet loss"""
    params = {"ssid": "open",
              "ignore_probe_probability": "0.5",
              "ignore_auth_probability": "0.5",
              "ignore_assoc_probability": "0.5",
              "ignore_reassoc_probability": "0.5"}
    hapd = hostapd.add_ap(apdev[0], params)
    for i in range(0, 3):
        dev[i].connect("open", key_mgmt="NONE", scan_freq="2412",
                       wait_connect=False)
    for i in range(0, 3):
        dev[i].wait_connected(timeout=20)

@remote_compatible
def test_ap_open_unknown_action(dev, apdev):
    """AP with open mode configuration and unknown Action frame"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": "open"})
    dev[0].connect("open", key_mgmt="NONE", scan_freq="2412")
    bssid = apdev[0]['bssid']
    cmd = "MGMT_TX {} {} freq=2412 action=765432".format(bssid, bssid)
    if "FAIL" in dev[0].request(cmd):
        raise Exception("Could not send test Action frame")
    ev = dev[0].wait_event(["MGMT-TX-STATUS"], timeout=10)
    if ev is None:
        raise Exception("Timeout on MGMT-TX-STATUS")
    if "result=SUCCESS" not in ev:
        raise Exception("AP did not ack Action frame")

def test_ap_open_invalid_wmm_action(dev, apdev):
    """AP with open mode configuration and invalid WMM Action frame"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": "open"})
    dev[0].connect("open", key_mgmt="NONE", scan_freq="2412")
    bssid = apdev[0]['bssid']
    cmd = "MGMT_TX {} {} freq=2412 action=1100".format(bssid, bssid)
    if "FAIL" in dev[0].request(cmd):
        raise Exception("Could not send test Action frame")
    ev = dev[0].wait_event(["MGMT-TX-STATUS"], timeout=10)
    if ev is None or "result=SUCCESS" not in ev:
        raise Exception("AP did not ack Action frame")

@remote_compatible
def test_ap_open_reconnect_on_inactivity_disconnect(dev, apdev):
    """Reconnect to open mode AP after inactivity related disconnection"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": "open"})
    dev[0].connect("open", key_mgmt="NONE", scan_freq="2412")
    hapd.request("DEAUTHENTICATE " + dev[0].p2p_interface_addr() + " reason=4")
    dev[0].wait_disconnected(timeout=5)
    dev[0].wait_connected(timeout=2, error="Timeout on reconnection")

@remote_compatible
def test_ap_open_assoc_timeout(dev, apdev):
    """AP timing out association"""
    ssid = "test"
    hapd = hostapd.add_ap(apdev[0], {"ssid": "open"})
    dev[0].scan(freq="2412")
    hapd.set("ext_mgmt_frame_handling", "1")
    dev[0].connect("open", key_mgmt="NONE", scan_freq="2412",
                   wait_connect=False)
    for i in range(0, 10):
        req = hapd.mgmt_rx()
        if req is None:
            raise Exception("MGMT RX wait timed out")
        if req['subtype'] == 11:
            break
        req = None
    if not req:
        raise Exception("Authentication frame not received")

    resp = {}
    resp['fc'] = req['fc']
    resp['da'] = req['sa']
    resp['sa'] = req['da']
    resp['bssid'] = req['bssid']
    resp['payload'] = struct.pack('<HHH', 0, 2, 0)
    hapd.mgmt_tx(resp)

    assoc = 0
    for i in range(0, 10):
        req = hapd.mgmt_rx()
        if req is None:
            raise Exception("MGMT RX wait timed out")
        if req['subtype'] == 0:
            assoc += 1
            if assoc == 3:
                break
    if assoc != 3:
        raise Exception("Association Request frames not received: assoc=%d" % assoc)
    hapd.set("ext_mgmt_frame_handling", "0")
    dev[0].wait_connected(timeout=15)

def test_ap_open_auth_drop_sta(dev, apdev):
    """AP dropping station after successful authentication"""
    hapd = hostapd.add_ap(apdev[0]['ifname'], {"ssid": "open"})
    dev[0].scan(freq="2412")
    hapd.set("ext_mgmt_frame_handling", "1")
    dev[0].connect("open", key_mgmt="NONE", scan_freq="2412",
                   wait_connect=False)
    for i in range(0, 10):
        req = hapd.mgmt_rx()
        if req is None:
            raise Exception("MGMT RX wait timed out")
        if req['subtype'] == 11:
            break
        req = None
    if not req:
        raise Exception("Authentication frame not received")

    # turn off before sending successful response
    hapd.set("ext_mgmt_frame_handling", "0")

    resp = {}
    resp['fc'] = req['fc']
    resp['da'] = req['sa']
    resp['sa'] = req['da']
    resp['bssid'] = req['bssid']
    resp['payload'] = struct.pack('<HHH', 0, 2, 0)
    hapd.mgmt_tx(resp)

    dev[0].wait_connected(timeout=15)

@remote_compatible
def test_ap_open_id_str(dev, apdev):
    """AP with open mode and id_str"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": "open"})
    dev[0].connect("open", key_mgmt="NONE", scan_freq="2412", id_str="foo",
                   wait_connect=False)
    ev = dev[0].wait_connected(timeout=10)
    if "id_str=foo" not in ev:
        raise Exception("CTRL-EVENT-CONNECT did not have matching id_str: " + ev)
    if dev[0].get_status_field("id_str") != "foo":
        raise Exception("id_str mismatch")

@remote_compatible
def test_ap_open_select_any(dev, apdev):
    """AP with open mode and select any network"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": "open"})
    id = dev[0].connect("unknown", key_mgmt="NONE", scan_freq="2412",
                        only_add_network=True)
    dev[0].connect("open", key_mgmt="NONE", scan_freq="2412",
                   only_add_network=True)
    dev[0].select_network(id)
    ev = dev[0].wait_event(["CTRL-EVENT-NETWORK-NOT-FOUND",
                            "CTRL-EVENT-CONNECTED"], timeout=10)
    if ev is None:
        raise Exception("No result reported")
    if "CTRL-EVENT-CONNECTED" in ev:
        raise Exception("Unexpected connection")

    dev[0].select_network("any")
    dev[0].wait_connected(timeout=10)

@remote_compatible
def test_ap_open_unexpected_assoc_event(dev, apdev):
    """AP with open mode and unexpected association event"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": "open"})
    dev[0].connect("open", key_mgmt="NONE", scan_freq="2412")
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected(timeout=15)
    dev[0].dump_monitor()
    # This association will be ignored by wpa_supplicant since the current
    # state is not to try to connect after that DISCONNECT command.
    dev[0].cmd_execute(['iw', 'dev', dev[0].ifname, 'connect', 'open', "2412",
                        apdev[0]['bssid']])
    ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED"], timeout=0.3)
    dev[0].cmd_execute(['iw', 'dev', dev[0].ifname, 'disconnect'])
    dev[0].dump_monitor()
    if ev is not None:
        raise Exception("Unexpected connection")

def test_ap_open_external_assoc(dev, apdev):
    """AP with open mode and external association"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": "open-ext-assoc"})
    try:
        dev[0].request("STA_AUTOCONNECT 0")
        id = dev[0].connect("open-ext-assoc", key_mgmt="NONE", scan_freq="2412",
                            only_add_network=True)
        dev[0].request("ENABLE_NETWORK %s no-connect" % id)
        dev[0].dump_monitor()
        # This will be accepted due to matching network
        dev[0].cmd_execute(['iw', 'dev', dev[0].ifname, 'connect',
                            'open-ext-assoc', "2412", apdev[0]['bssid']])
        ev = dev[0].wait_event(["CTRL-EVENT-DISCONNECTED",
                                "CTRL-EVENT-CONNECTED"], timeout=10)
        if ev is None:
            raise Exception("Connection timed out")
        if "CTRL-EVENT-DISCONNECTED" in ev:
            raise Exception("Unexpected disconnection event")
        dev[0].dump_monitor()
        dev[0].request("DISCONNECT")
        dev[0].wait_disconnected(timeout=5)
    finally:
        dev[0].request("STA_AUTOCONNECT 1")

@remote_compatible
def test_ap_bss_load(dev, apdev):
    """AP with open mode (no security) configuration"""
    hapd = hostapd.add_ap(apdev[0],
                          {"ssid": "open",
                           "bss_load_update_period": "10",
                           "chan_util_avg_period": "20"})
    dev[0].connect("open", key_mgmt="NONE", scan_freq="2412")
    # this does not really get much useful output with mac80211_hwsim currently,
    # but run through the channel survey update couple of times
    for i in range(0, 10):
        hwsim_utils.test_connectivity(dev[0], hapd)
        hwsim_utils.test_connectivity(dev[0], hapd)
        hwsim_utils.test_connectivity(dev[0], hapd)
        time.sleep(0.15)
    avg = hapd.get_status_field("chan_util_avg")
    if avg is None:
        raise Exception("No STATUS chan_util_avg seen")

def test_ap_bss_load_fail(dev, apdev):
    """BSS Load update failing to get survey data"""
    hapd = hostapd.add_ap(apdev[0],
                          {"ssid": "open",
                           "bss_load_update_period": "1"})
    with fail_test(hapd, 1, "wpa_driver_nl80211_get_survey"):
        wait_fail_trigger(hapd, "GET_FAIL")

def hapd_out_of_mem(hapd, apdev, count, func):
    with alloc_fail(hapd, count, func):
        started = False
        try:
            hostapd.add_ap(apdev, {"ssid": "open"})
            started = True
        except:
            pass
        if started:
            raise Exception("hostapd interface started even with memory allocation failure: %d:%s" % (count, func))

def test_ap_open_out_of_memory(dev, apdev):
    """hostapd failing to setup interface due to allocation failure"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": "open"})
    flags2 = hapd.request("DRIVER_FLAGS2").splitlines()[1:]
    hapd_out_of_mem(hapd, apdev[1], 1, "hostapd_alloc_bss_data")

    for i in range(1, 3):
        hapd_out_of_mem(hapd, apdev[1], i, "hostapd_iface_alloc")

    for i in range(1, 5):
        hapd_out_of_mem(hapd, apdev[1], i, "hostapd_config_defaults;hostapd_config_alloc")

    hapd_out_of_mem(hapd, apdev[1], 1, "hostapd_config_alloc")

    hapd_out_of_mem(hapd, apdev[1], 1, "hostapd_driver_init")

    for i in range(1, 3):
        hapd_out_of_mem(hapd, apdev[1], i, "=wpa_driver_nl80211_drv_init")

    if 'CONTROL_PORT_RX' not in flags2:
        # eloop_register_read_sock() call from i802_init()
        hapd_out_of_mem(hapd, apdev[1], 1, "eloop_sock_table_add_sock;?eloop_register_sock;?eloop_register_read_sock;=i802_init")

    # verify that a new interface can still be added when memory allocation does
    # not fail
    hostapd.add_ap(apdev[1], {"ssid": "open"})

def test_bssid_ignore_accept(dev, apdev):
    """BSSID ignore/accept list"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": "open"})
    hapd2 = hostapd.add_ap(apdev[1], {"ssid": "open"})

    dev[0].connect("open", key_mgmt="NONE", scan_freq="2412",
                   bssid_accept=apdev[1]['bssid'])
    dev[1].connect("open", key_mgmt="NONE", scan_freq="2412",
                   bssid_ignore=apdev[1]['bssid'])
    dev[2].connect("open", key_mgmt="NONE", scan_freq="2412",
                   bssid_accept="00:00:00:00:00:00/00:00:00:00:00:00",
                   bssid_ignore=apdev[1]['bssid'])
    if dev[0].get_status_field('bssid') != apdev[1]['bssid']:
        raise Exception("dev[0] connected to unexpected AP")
    if dev[1].get_status_field('bssid') != apdev[0]['bssid']:
        raise Exception("dev[1] connected to unexpected AP")
    if dev[2].get_status_field('bssid') != apdev[0]['bssid']:
        raise Exception("dev[2] connected to unexpected AP")
    dev[0].request("REMOVE_NETWORK all")
    dev[1].request("REMOVE_NETWORK all")
    dev[2].request("REMOVE_NETWORK all")

    dev[2].connect("open", key_mgmt="NONE", scan_freq="2412",
                   bssid_accept="00:00:00:00:00:00", wait_connect=False)
    dev[0].connect("open", key_mgmt="NONE", scan_freq="2412",
                   bssid_accept="11:22:33:44:55:66/ff:00:00:00:00:00 " + apdev[1]['bssid'] + " aa:bb:cc:dd:ee:ff")
    dev[1].connect("open", key_mgmt="NONE", scan_freq="2412",
                   bssid_ignore="11:22:33:44:55:66/ff:00:00:00:00:00 " + apdev[1]['bssid'] + " aa:bb:cc:dd:ee:ff")
    if dev[0].get_status_field('bssid') != apdev[1]['bssid']:
        raise Exception("dev[0] connected to unexpected AP")
    if dev[1].get_status_field('bssid') != apdev[0]['bssid']:
        raise Exception("dev[1] connected to unexpected AP")
    dev[0].request("REMOVE_NETWORK all")
    dev[1].request("REMOVE_NETWORK all")
    ev = dev[2].wait_event(["CTRL-EVENT-CONNECTED"], timeout=0.1)
    if ev is not None:
        raise Exception("Unexpected dev[2] connectin")
    dev[2].request("REMOVE_NETWORK all")

def test_ap_open_wpas_in_bridge(dev, apdev):
    """Open mode AP and wpas interface in a bridge"""
    br_ifname = 'sta-br0'
    ifname = 'wlan5'
    try:
        _test_ap_open_wpas_in_bridge(dev, apdev)
    finally:
        subprocess.call(['ip', 'link', 'set', 'dev', br_ifname, 'down'])
        subprocess.call(['brctl', 'delif', br_ifname, ifname])
        subprocess.call(['brctl', 'delbr', br_ifname])
        subprocess.call(['iw', ifname, 'set', '4addr', 'off'])

def _test_ap_open_wpas_in_bridge(dev, apdev):
    hapd = hostapd.add_ap(apdev[0], {"ssid": "open"})

    br_ifname = 'sta-br0'
    ifname = 'wlan5'
    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    # First, try a failure case of adding an interface
    try:
        wpas.interface_add(ifname, br_ifname=br_ifname)
        raise Exception("Interface addition succeeded unexpectedly")
    except Exception as e:
        if "Failed to add" in str(e):
            logger.info("Ignore expected interface_add failure due to missing bridge interface: " + str(e))
        else:
            raise

    # Next, add the bridge interface and add the interface again
    subprocess.call(['brctl', 'addbr', br_ifname])
    subprocess.call(['brctl', 'setfd', br_ifname, '0'])
    subprocess.call(['ip', 'link', 'set', 'dev', br_ifname, 'up'])
    subprocess.call(['iw', ifname, 'set', '4addr', 'on'])
    subprocess.check_call(['brctl', 'addif', br_ifname, ifname])
    wpas.interface_add(ifname, br_ifname=br_ifname)

    wpas.connect("open", key_mgmt="NONE", scan_freq="2412")

@remote_compatible
def test_ap_open_start_disabled(dev, apdev):
    """AP with open mode and beaconing disabled"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": "open",
                                     "start_disabled": "1"})
    bssid = apdev[0]['bssid']

    dev[0].flush_scan_cache()
    dev[0].scan(freq=2412, only_new=True)
    if dev[0].get_bss(bssid) is not None:
        raise Exception("AP was seen beaconing")
    if "OK" not in hapd.request("RELOAD"):
        raise Exception("RELOAD failed")
    dev[0].scan_for_bss(bssid, freq=2412)
    dev[0].connect("open", key_mgmt="NONE", scan_freq="2412")

@remote_compatible
def test_ap_open_start_disabled2(dev, apdev):
    """AP with open mode and beaconing disabled (2)"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": "open",
                                     "start_disabled": "1"})
    bssid = apdev[0]['bssid']

    dev[0].flush_scan_cache()
    dev[0].scan(freq=2412, only_new=True)
    if dev[0].get_bss(bssid) is not None:
        raise Exception("AP was seen beaconing")
    if "OK" not in hapd.request("UPDATE_BEACON"):
        raise Exception("UPDATE_BEACON failed")
    dev[0].scan_for_bss(bssid, freq=2412)
    dev[0].connect("open", key_mgmt="NONE", scan_freq="2412")
    if "OK" not in hapd.request("UPDATE_BEACON"):
        raise Exception("UPDATE_BEACON failed")
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()
    dev[0].request("RECONNECT")
    dev[0].wait_connected()

@remote_compatible
def test_ap_open_ifdown(dev, apdev):
    """AP with open mode and external ifconfig down"""
    params = {"ssid": "open",
              "ap_max_inactivity": "1"}
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = apdev[0]['bssid']

    dev[0].connect("open", key_mgmt="NONE", scan_freq="2412")
    dev[1].connect("open", key_mgmt="NONE", scan_freq="2412")
    hapd.cmd_execute(['ip', 'link', 'set', 'dev', apdev[0]['ifname'], 'down'])
    ev = hapd.wait_event(["AP-STA-DISCONNECTED"], timeout=10)
    if ev is None:
        raise Exception("Timeout on AP-STA-DISCONNECTED (1)")
    ev = hapd.wait_event(["AP-STA-DISCONNECTED"], timeout=5)
    if ev is None:
        raise Exception("Timeout on AP-STA-DISCONNECTED (2)")
    ev = hapd.wait_event(["INTERFACE-DISABLED"], timeout=5)
    if ev is None:
        raise Exception("No INTERFACE-DISABLED event")
    # The following wait tests beacon loss detection in mac80211 on dev0.
    # dev1 is used to test stopping of AP side functionality on client polling.
    dev[1].request("REMOVE_NETWORK all")
    hapd.cmd_execute(['ip', 'link', 'set', 'dev', apdev[0]['ifname'], 'up'])
    dev[0].wait_disconnected()
    dev[1].wait_disconnected()
    ev = hapd.wait_event(["INTERFACE-ENABLED"], timeout=10)
    if ev is None:
        raise Exception("No INTERFACE-ENABLED event")
    dev[0].wait_connected()
    hwsim_utils.test_connectivity(dev[0], hapd)

def test_ap_open_disconnect_in_ps(dev, apdev, params):
    """Disconnect with the client in PS to regression-test a kernel bug"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": "open"})
    dev[0].connect("open", key_mgmt="NONE", scan_freq="2412",
                   bg_scan_period="0")
    ev = hapd.wait_event(["AP-STA-CONNECTED"], timeout=5)
    if ev is None:
        raise Exception("No connection event received from hostapd")

    time.sleep(0.2)
    # enable power save mode
    hwsim_utils.set_powersave(dev[0], hwsim_utils.PS_ENABLED)
    time.sleep(0.1)
    try:
        # inject some traffic
        sa = hapd.own_addr()
        da = dev[0].own_addr()
        hapd.request('DATA_TEST_CONFIG 1')
        hapd.request('DATA_TEST_TX {} {} 0'.format(da, sa))
        hapd.request('DATA_TEST_CONFIG 0')

        # let the AP send couple of Beacon frames
        time.sleep(0.3)

        # disconnect - with traffic pending - shouldn't cause kernel warnings
        dev[0].request("DISCONNECT")
    finally:
        hwsim_utils.set_powersave(dev[0], hwsim_utils.PS_DISABLED)

    time.sleep(0.2)
    out = run_tshark(os.path.join(params['logdir'], "hwsim0.pcapng"),
                     "wlan_mgt.tim.partial_virtual_bitmap",
                     ["wlan_mgt.tim.partial_virtual_bitmap"])
    if out is not None:
        state = 0
        for l in out.splitlines():
            pvb = int(l, 16)
            if pvb > 0 and state == 0:
                state = 1
            elif pvb == 0 and state == 1:
                state = 2
        if state != 2:
            raise Exception("Didn't observe TIM bit getting set and unset (state=%d)" % state)

def test_ap_open_sta_ps(dev, apdev):
    """Station power save operation"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": "open"})
    dev[0].connect("open", key_mgmt="NONE", scan_freq="2412",
                   bg_scan_period="0")
    hapd.wait_sta()

    time.sleep(0.2)
    try:
        dev[0].cmd_execute(['iw', 'dev', dev[0].ifname,
                            'set', 'power_save', 'on'])
        run_ap_open_sta_ps(dev, hapd)
    finally:
        dev[0].cmd_execute(['iw', 'dev', dev[0].ifname,
                            'set', 'power_save', 'off'])

def run_ap_open_sta_ps(dev, hapd):
    hwsim_utils.test_connectivity(dev[0], hapd)
    # Give time to enter PS
    time.sleep(0.2)

    phyname = dev[0].get_driver_status_field("phyname")
    hw_conf = '/sys/kernel/debug/ieee80211/' + phyname + '/hw_conf'

    try:
        ok = False
        for i in range(10):
            with open(hw_conf, 'r') as f:
                val = int(f.read())
            if val & 2:
                ok = True
                break
            time.sleep(0.2)

        if not ok:
            raise Exception("STA did not enter power save")

        dev[0].dump_monitor()
        hapd.dump_monitor()
        hapd.request("DEAUTHENTICATE " + dev[0].own_addr())
        dev[0].wait_disconnected()
    except FileNotFoundError:
        raise HwsimSkip("Kernel does not support inspecting HW PS state")

def test_ap_open_ps_mc_buf(dev, apdev, params):
    """Multicast buffering with a station in power save"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": "open"})
    dev[0].connect("open", key_mgmt="NONE", scan_freq="2412",
                   bg_scan_period="0")
    hapd.wait_sta()

    buffered_mcast = 0
    try:
        dev[0].cmd_execute(['iw', 'dev', dev[0].ifname,
                            'set', 'power_save', 'on'])
        # Give time to enter PS
        time.sleep(0.3)

        for i in range(10):
            # Verify that multicast frames are released
            hwsim_utils.run_multicast_connectivity_test(hapd, dev[0])

            # Check frames were buffered until DTIM
            out = run_tshark(os.path.join(params['logdir'], "hwsim0.pcapng"),
                             "wlan.fc.type_subtype == 0x0008",
                             ["wlan.tim.bmapctl.multicast"])
            for line in out.splitlines():
                buffered_mcast = int(line)
                if buffered_mcast == 1:
                    break
            if buffered_mcast == 1:
                break
    finally:
        dev[0].cmd_execute(['iw', 'dev', dev[0].ifname,
                            'set', 'power_save', 'off'])

    if buffered_mcast != 1:
        raise Exception("AP did not buffer multicast frames")

@remote_compatible
def test_ap_open_select_network(dev, apdev):
    """Open mode connection and SELECT_NETWORK to change network"""
    hapd1 = hostapd.add_ap(apdev[0], {"ssid": "open"})
    bssid1 = apdev[0]['bssid']
    hapd2 = hostapd.add_ap(apdev[1], {"ssid": "open2"})
    bssid2 = apdev[1]['bssid']

    id1 = dev[0].connect("open", key_mgmt="NONE", scan_freq="2412",
                         only_add_network=True)
    id2 = dev[0].connect("open2", key_mgmt="NONE", scan_freq="2412")
    hwsim_utils.test_connectivity(dev[0], hapd2)

    dev[0].select_network(id1)
    dev[0].wait_connected()
    res = dev[0].request("BSSID_IGNORE")
    if bssid1 in res or bssid2 in res:
        raise Exception("Unexpected BSSID ignore list entry")
    hwsim_utils.test_connectivity(dev[0], hapd1)

    dev[0].select_network(id2)
    dev[0].wait_connected()
    hwsim_utils.test_connectivity(dev[0], hapd2)
    res = dev[0].request("BSSID_IGNORE")
    if bssid1 in res or bssid2 in res:
        raise Exception("Unexpected BSSID ignore list entry(2)")

@remote_compatible
def test_ap_open_disable_enable(dev, apdev):
    """AP with open mode getting disabled and re-enabled"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": "open"})
    dev[0].connect("open", key_mgmt="NONE", scan_freq="2412",
                   bg_scan_period="0")

    for i in range(2):
        hapd.request("DISABLE")
        dev[0].wait_disconnected()
        hapd.request("ENABLE")
        dev[0].wait_connected()
        hwsim_utils.test_connectivity(dev[0], hapd)

def sta_enable_disable(dev, bssid):
    dev.scan_for_bss(bssid, freq=2412)
    work_id = dev.request("RADIO_WORK add block-work")
    ev = dev.wait_event(["EXT-RADIO-WORK-START"])
    if ev is None:
        raise Exception("Timeout while waiting radio work to start")
    id = dev.connect("open", key_mgmt="NONE", scan_freq="2412",
                     only_add_network=True)
    dev.request("ENABLE_NETWORK %d" % id)
    if "connect@" not in dev.request("RADIO_WORK show"):
        raise Exception("connect radio work missing")
    dev.request("DISABLE_NETWORK %d" % id)
    dev.request("RADIO_WORK done " + work_id)

    ok = False
    for i in range(30):
        if "connect@" not in dev.request("RADIO_WORK show"):
            ok = True
            break
        time.sleep(0.1)
    if not ok:
        raise Exception("connect radio work not completed")
    ev = dev.wait_event(["CTRL-EVENT-CONNECTED"], timeout=0.1)
    if ev is not None:
        raise Exception("Unexpected connection")
    dev.request("DISCONNECT")

def test_ap_open_sta_enable_disable(dev, apdev):
    """AP with open mode and wpa_supplicant ENABLE/DISABLE_NETWORK"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": "open"})
    bssid = apdev[0]['bssid']

    sta_enable_disable(dev[0], bssid)

    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    wpas.interface_add("wlan5", drv_params="force_connect_cmd=1")
    sta_enable_disable(wpas, bssid)

@remote_compatible
def test_ap_open_select_twice(dev, apdev):
    """AP with open mode and select network twice"""
    id = dev[0].connect("open", key_mgmt="NONE", scan_freq="2412",
                        only_add_network=True)
    dev[0].select_network(id)
    ev = dev[0].wait_event(["CTRL-EVENT-NETWORK-NOT-FOUND"], timeout=10)
    if ev is None:
        raise Exception("No result reported")
    hapd = hostapd.add_ap(apdev[0], {"ssid": "open"})
    # Verify that the second SELECT_NETWORK starts a new scan immediately by
    # waiting less than the default scan period.
    dev[0].select_network(id)
    dev[0].wait_connected(timeout=3)

@remote_compatible
def test_ap_open_reassoc_not_found(dev, apdev):
    """AP with open mode and REASSOCIATE not finding a match"""
    id = dev[0].connect("open", key_mgmt="NONE", scan_freq="2412",
                        only_add_network=True)
    dev[0].select_network(id)
    ev = dev[0].wait_event(["CTRL-EVENT-NETWORK-NOT-FOUND"], timeout=10)
    if ev is None:
        raise Exception("No result reported")
    dev[0].request("DISCONNECT")

    time.sleep(0.1)
    dev[0].dump_monitor()

    dev[0].request("REASSOCIATE")
    ev = dev[0].wait_event(["CTRL-EVENT-NETWORK-NOT-FOUND"], timeout=10)
    if ev is None:
        raise Exception("No result reported")
    dev[0].request("DISCONNECT")

@remote_compatible
def test_ap_open_sta_statistics(dev, apdev):
    """AP with open mode and STA statistics"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": "open"})
    dev[0].connect("open", key_mgmt="NONE", scan_freq="2412")
    addr = dev[0].own_addr()

    stats1 = hapd.get_sta(addr)
    logger.info("stats1: " + str(stats1))
    time.sleep(0.4)
    stats2 = hapd.get_sta(addr)
    logger.info("stats2: " + str(stats2))
    hwsim_utils.test_connectivity(dev[0], hapd)
    stats3 = hapd.get_sta(addr)
    logger.info("stats3: " + str(stats3))

    # Cannot require specific inactive_msec changes without getting rid of all
    # unrelated traffic, so for now, just print out the results in the log for
    # manual checks.

@remote_compatible
def test_ap_open_poll_sta(dev, apdev):
    """AP with open mode and STA poll"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": "open"})
    dev[0].connect("open", key_mgmt="NONE", scan_freq="2412")
    addr = dev[0].own_addr()

    if "OK" not in hapd.request("POLL_STA " + addr):
        raise Exception("POLL_STA failed")
    ev = hapd.wait_event(["AP-STA-POLL-OK"], timeout=5)
    if ev is None:
        raise Exception("Poll response not seen")
    if addr not in ev:
        raise Exception("Unexpected poll response: " + ev)

def test_ap_open_poll_sta_no_ack(dev, apdev):
    """AP with open mode and STA poll without ACK"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": "open"})
    dev[0].connect("open", key_mgmt="NONE", scan_freq="2412")
    addr = dev[0].own_addr()

    hapd.set("ext_mgmt_frame_handling", "1")
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()
    hapd.set("ext_mgmt_frame_handling", "0")
    if "OK" not in hapd.request("POLL_STA " + addr):
        raise Exception("POLL_STA failed")
    ev = hapd.wait_event(["AP-STA-POLL-OK"], timeout=1)
    if ev is not None:
        raise Exception("Unexpected poll response reported")

def test_ap_open_pmf_default(dev, apdev):
    """AP with open mode (no security) configuration and pmf=2"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": "open"})
    dev[1].connect("open", key_mgmt="NONE", scan_freq="2412",
                   ieee80211w="2", wait_connect=False)
    dev[2].connect("open", key_mgmt="NONE", scan_freq="2412",
                   ieee80211w="1")
    try:
        dev[0].request("SET pmf 2")
        dev[0].connect("open", key_mgmt="NONE", scan_freq="2412")

        dev[0].request("DISCONNECT")
        dev[0].wait_disconnected()
    finally:
        dev[0].request("SET pmf 0")
    dev[2].request("DISCONNECT")
    dev[2].wait_disconnected()

    ev = dev[1].wait_event(["CTRL-EVENT-CONNECTED"], timeout=0.1)
    if ev is not None:
        raise Exception("Unexpected dev[1] connection")
    dev[1].request("DISCONNECT")

def test_ap_open_drv_fail(dev, apdev):
    """AP with open mode and driver operations failing"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": "open"})

    with fail_test(dev[0], 1, "wpa_driver_nl80211_authenticate"):
        dev[0].connect("open", key_mgmt="NONE", scan_freq="2412",
                       wait_connect=False)
        wait_fail_trigger(dev[0], "GET_FAIL")
        dev[0].request("REMOVE_NETWORK all")

    with fail_test(dev[0], 1, "wpa_driver_nl80211_associate"):
        dev[0].connect("open", key_mgmt="NONE", scan_freq="2412",
                       wait_connect=False)
        wait_fail_trigger(dev[0], "GET_FAIL")
        dev[0].request("REMOVE_NETWORK all")

def run_multicast_to_unicast(dev, apdev, convert):
    params = {"ssid": "open"}
    params["multicast_to_unicast"] = "1" if convert else "0"
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].scan_for_bss(hapd.own_addr(), freq=2412)
    dev[0].connect("open", key_mgmt="NONE", scan_freq="2412")
    ev = hapd.wait_event(["AP-STA-CONNECTED"], timeout=5)
    if ev is None:
        raise Exception("No connection event received from hostapd")
    hwsim_utils.test_connectivity(dev[0], hapd, multicast_to_unicast=convert)
    dev[0].request("DISCONNECT")
    ev = hapd.wait_event(["AP-STA-DISCONNECTED"], timeout=5)
    if ev is None:
        raise Exception("No disconnection event received from hostapd")

def test_ap_open_multicast_to_unicast(dev, apdev):
    """Multicast-to-unicast conversion enabled"""
    run_multicast_to_unicast(dev, apdev, True)

def test_ap_open_multicast_to_unicast_disabled(dev, apdev):
    """Multicast-to-unicast conversion disabled"""
    run_multicast_to_unicast(dev, apdev, False)

def test_ap_open_drop_duplicate(dev, apdev, params):
    """AP dropping duplicate management frames"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": "open",
                                     "interworking": "1"})
    hapd.set("ext_mgmt_frame_handling", "1")
    bssid = hapd.own_addr().replace(':', '')
    addr = "020304050607"
    auth = "b0003a01" + bssid + addr + bssid + '1000000001000000'
    if "OK" not in hapd.request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=%s" % auth):
        raise Exception("MGMT_RX_PROCESS failed")
    auth = "b0083a01" + bssid + addr + bssid + '1000000001000000'
    if "OK" not in hapd.request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=%s" % auth):
        raise Exception("MGMT_RX_PROCESS failed")

    ies = "00046f70656e010802040b160c12182432043048606c2d1a3c101bffff0000000000000000000001000000000000000000007f0a04000a020140004000013b155151525354737475767778797a7b7c7d7e7f808182dd070050f202000100"
    assoc_req = "00003a01" + bssid + addr + bssid + "2000" + "21040500" + ies
    if "OK" not in hapd.request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=%s" % assoc_req):
        raise Exception("MGMT_RX_PROCESS failed")
    assoc_req = "00083a01" + bssid + addr + bssid + "2000" + "21040500" + ies
    if "OK" not in hapd.request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=%s" % assoc_req):
        raise Exception("MGMT_RX_PROCESS failed")
    reassoc_req = "20083a01" + bssid + addr + bssid + "2000" + "21040500" + ies
    if "OK" not in hapd.request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=%s" % reassoc_req):
        raise Exception("MGMT_RX_PROCESS failed")
    if "OK" not in hapd.request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=%s" % reassoc_req):
        raise Exception("MGMT_RX_PROCESS failed")

    action = "d0003a01" + bssid + addr + bssid + "1000" + "040a006c0200000600000102000101"
    if "OK" not in hapd.request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=%s" % action):
        raise Exception("MGMT_RX_PROCESS failed")

    action = "d0083a01" + bssid + addr + bssid + "1000" + "040a006c0200000600000102000101"
    if "OK" not in hapd.request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=%s" % action):
        raise Exception("MGMT_RX_PROCESS failed")

    out = run_tshark(os.path.join(params['logdir'], "hwsim0.pcapng"),
                     "wlan.fc.type == 0", ["wlan.fc.subtype"])
    num_auth = 0
    num_assoc = 0
    num_reassoc = 0
    num_action = 0
    for subtype in out.splitlines():
        val = int(subtype)
        if val == 11:
            num_auth += 1
        elif val == 1:
            num_assoc += 1
        elif val == 3:
            num_reassoc += 1
        elif val == 13:
            num_action += 1
    if num_auth != 1:
        raise Exception("Unexpected number of Authentication frames: %d" % num_auth)
    if num_assoc != 1:
        raise Exception("Unexpected number of association frames: %d" % num_assoc)
    if num_reassoc != 1:
        raise Exception("Unexpected number of reassociation frames: %d" % num_reassoc)
    if num_action != 1:
        raise Exception("Unexpected number of Action frames: %d" % num_action)

def test_ap_open_select_network_freq(dev, apdev):
    """AP with open mode and use for SELECT_NETWORK freq parameter"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": "open"})
    id = dev[0].connect("open", key_mgmt="NONE", only_add_network=True)
    dev[0].select_network(id, freq=2412)
    start = os.times()[4]
    ev = dev[0].wait_event(["CTRL-EVENT-SCAN-STARTED"], timeout=5)
    if ev is None:
        raise Exception("Scan not started")
    ev = dev[0].wait_event(["CTRL-EVENT-SCAN-RESULTS"], timeout=15)
    if ev is None:
        raise Exception("Scan not completed")
    end = os.times()[4]
    logger.info("Scan duration: {} seconds".format(end - start))
    if end - start > 3:
        raise Exception("Scan took unexpectedly long time")
    dev[0].wait_connected()

def test_ap_open_noncountry(dev, apdev):
    """AP with open mode and noncountry entity as Country String"""
    _test_ap_open_country(dev, apdev, "XX", "0x58")

def test_ap_open_country_table_e4(dev, apdev):
    """AP with open mode and Table E-4 Country String"""
    _test_ap_open_country(dev, apdev, "DE", "0x04")

def test_ap_open_country_indoor(dev, apdev):
    """AP with open mode and indoor country code"""
    _test_ap_open_country(dev, apdev, "DE", "0x49")

def test_ap_open_country_outdoor(dev, apdev):
    """AP with open mode and outdoor country code"""
    _test_ap_open_country(dev, apdev, "DE", "0x4f")

def _test_ap_open_country(dev, apdev, country_code, country3):
    try:
        hapd = None
        hapd = run_ap_open_country(dev, apdev, country_code, country3)
    finally:
        clear_regdom(hapd, dev)

def run_ap_open_country(dev, apdev, country_code, country3):
    hapd = hostapd.add_ap(apdev[0], {"ssid": "open",
                                     "country_code": country_code,
                                     "country3": country3,
                                     "ieee80211d": "1"})
    dev[0].scan_for_bss(hapd.own_addr(), freq=2412)
    dev[0].connect("open", key_mgmt="NONE", scan_freq="2412")
    dev[0].wait_regdom(country_ie=True)
    return hapd

def test_ap_open_disable_select(dev, apdev):
    """DISABLE_NETWORK for connected AP followed by SELECT_NETWORK"""
    hapd1 = hostapd.add_ap(apdev[0], {"ssid": "open"})
    hapd2 = hostapd.add_ap(apdev[1], {"ssid": "open"})
    id = dev[0].connect("open", key_mgmt="NONE", scan_freq="2412")

    dev[0].request("DISABLE_NETWORK %d" % id)
    dev[0].wait_disconnected()
    res = dev[0].request("BSSID_IGNORE")
    if hapd1.own_addr() in res or hapd2.own_addr() in res:
        raise Exception("Unexpected BSSID ignore list entry added")
    dev[0].request("SELECT_NETWORK %d" % id)
    dev[0].wait_connected()

def test_ap_open_reassoc_same(dev, apdev):
    """AP with open mode and STA reassociating back to same AP without auth exchange"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": "open"})
    dev[0].connect("open", key_mgmt="NONE", scan_freq="2412")
    try:
        dev[0].request("SET reassoc_same_bss_optim 1")
        dev[0].request("REATTACH")
        dev[0].wait_connected()
        hwsim_utils.test_connectivity(dev[0], hapd)
    finally:
        dev[0].request("SET reassoc_same_bss_optim 0")

def test_ap_open_no_reflection(dev, apdev):
    """AP with open mode, STA sending packets to itself"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": "open"})
    dev[0].connect("open", key_mgmt="NONE", scan_freq="2412")

    ev = hapd.wait_event(["AP-STA-CONNECTED"], timeout=5)
    if ev is None:
        raise Exception("No connection event received from hostapd")
    # test normal connectivity is OK
    hwsim_utils.test_connectivity(dev[0], hapd)

    # test that we can't talk to ourselves
    addr = dev[0].own_addr()
    res = dev[0].request('DATA_TEST_CONFIG 1')
    try:
        assert 'OK' in res

        cmd = "DATA_TEST_TX {} {} {}".format(addr, addr, 0)
        dev[0].request(cmd)

        ev = dev[0].wait_event(["DATA-TEST-RX"], timeout=1)

        if ev is not None and "DATA-TEST-RX {} {}".format(addr, addr) in ev:
            raise Exception("STA can unexpectedly talk to itself")
    finally:
        dev[0].request('DATA_TEST_CONFIG 0')

def test_ap_no_auth_ack(dev, apdev):
    """AP not receiving Authentication frame ACK"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": "open",
                                     "ap_max_inactivity": "1"})
    hapd.set("ext_mgmt_frame_handling", "1")
    bssid = hapd.own_addr()
    addr = "02:01:02:03:04:05"
    frame = "b0003a01" + bssid.replace(':', '') + addr.replace(':', '') + bssid.replace(':', '') + "1000" + "000001000000"
    if "OK" not in hapd.request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=" + frame):
        raise Exception("MGMT_RX_PROCESS failed")
    ev = hapd.wait_event(["MGMT-TX-STATUS"], timeout=5)
    if ev is None:
        raise Exception("TX status for Authentication frame not reported")
    if "ok=0 buf=b0" not in ev:
        raise Exception("Unexpected TX status contents: " + ev)

    # wait for STA to be removed due to timeout
    ev = hapd.wait_event(["MGMT-TX-STATUS"], timeout=5)
    if ev is None:
        raise Exception("TX status for Deauthentication frame not reported")
    if "ok=0 buf=c0" not in ev:
        raise Exception("Unexpected TX status contents (disconnect): " + ev)

def test_ap_open_layer_2_update(dev, apdev, params):
    """AP with open mode (no security) and Layer 2 Update frame"""
    prefix = "ap_open_layer_2_update"
    ifname = apdev[0]["ifname"]
    cap = os.path.join(params['logdir'], prefix + "." + ifname + ".pcap")

    hapd = hostapd.add_ap(apdev[0], {"ssid": "open"})
    wt = WlantestCapture(ifname, cap)
    time.sleep(1)

    dev[0].connect("open", key_mgmt="NONE", scan_freq="2412")
    hapd.wait_sta()
    hwsim_utils.test_connectivity(dev[0], hapd)
    time.sleep(1)
    hwsim_utils.test_connectivity(dev[0], hapd)
    time.sleep(0.5)
    wt.close()

    # Check for Layer 2 Update frame and unexpected frames from the station
    # that did not fully complete authentication.
    res = run_tshark(cap, "basicxid.llc.xid.format == 0x81",
                     ["eth.src"], wait=False)
    real_sta_seen = False
    unexpected_sta_seen = False
    real_addr = dev[0].own_addr()
    for l in res.splitlines():
        if l == real_addr:
            real_sta_seen = True
        else:
            unexpected_sta_seen = True
    if unexpected_sta_seen:
        raise Exception("Layer 2 Update frame from unexpected STA seen")
    if not real_sta_seen:
        raise Exception("Layer 2 Update frame from real STA not seen")
