#!/usr/bin/python
#
# Test cases for AP VLAN
# Copyright (c) 2013-2016, Jouni Malinen <j@w1.fi>
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

from remotehost import remote_compatible
import time
import subprocess
import logging
logger = logging.getLogger(__name__)

try:
    import netifaces
    netifaces_imported = True
except ImportError:
    netifaces_imported = False

import hwsim_utils
import hostapd
from utils import iface_is_in_bridge, HwsimSkip, alloc_fail
import os
from tshark import run_tshark

def test_ap_vlan_open(dev, apdev):
    """AP VLAN with open network"""
    filename = hostapd.acl_file(dev, apdev, 'hostapd.accept')
    hostapd.send_file(apdev[0], filename, filename)
    params = {"ssid": "test-vlan-open",
              "dynamic_vlan": "1",
              "accept_mac_file": filename}
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].connect("test-vlan-open", key_mgmt="NONE", scan_freq="2412")
    dev[1].connect("test-vlan-open", key_mgmt="NONE", scan_freq="2412")
    dev[2].connect("test-vlan-open", key_mgmt="NONE", scan_freq="2412")
    hwsim_utils.test_connectivity_iface(dev[0], hapd, "brvlan1")
    hwsim_utils.test_connectivity_iface(dev[1], hapd, "brvlan2")
    hwsim_utils.test_connectivity(dev[2], hapd)
    if filename.startswith('/tmp/'):
        os.unlink(filename)

def test_ap_vlan_file_open(dev, apdev):
    """AP VLAN with open network and vlan_file mapping"""
    filename = hostapd.acl_file(dev, apdev, 'hostapd.accept')
    hostapd.send_file(apdev[0], filename, filename)
    params = {"ssid": "test-vlan-open",
              "dynamic_vlan": "1",
              "vlan_file": "hostapd.vlan",
              "accept_mac_file": filename}
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].connect("test-vlan-open", key_mgmt="NONE", scan_freq="2412")
    dev[1].connect("test-vlan-open", key_mgmt="NONE", scan_freq="2412")
    dev[2].connect("test-vlan-open", key_mgmt="NONE", scan_freq="2412")
    hwsim_utils.test_connectivity_iface(dev[0], hapd, "brvlan1")
    hwsim_utils.test_connectivity_iface(dev[1], hapd, "brvlan2")
    hwsim_utils.test_connectivity(dev[2], hapd)
    if filename.startswith('/tmp/'):
        os.unlink(filename)

def test_ap_vlan_file_open2(dev, apdev):
    """AP VLAN with open network and vlan_file mapping (2)"""
    filename = hostapd.acl_file(dev, apdev, 'hostapd.accept2')
    hostapd.send_file(apdev[0], filename, filename)
    params = {"ssid": "test-vlan-open",
              "dynamic_vlan": "1",
              "vlan_file": "hostapd.vlan2",
              "accept_mac_file": filename}
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].connect("test-vlan-open", key_mgmt="NONE", scan_freq="2412")
    dev[1].connect("test-vlan-open", key_mgmt="NONE", scan_freq="2412")
    dev[2].connect("test-vlan-open", key_mgmt="NONE", scan_freq="2412")
    hwsim_utils.test_connectivity_iface(dev[0], hapd, "brvlan1")
    hwsim_utils.test_connectivity_iface(dev[1], hapd, "brvlan2")
    hwsim_utils.test_connectivity_iface(dev[2], hapd, "hwsimbr3")
    if filename.startswith('/tmp/'):
        os.unlink(filename)

def test_ap_vlan_file_parsing(dev, apdev, params):
    """hostapd vlan_file/mac_file parsing"""
    tmp = os.path.join(params['logdir'], 'ap_vlan_file_parsing.tmp')
    params = {"ssid": "test-vlan-open", "dynamic_vlan": "1"}
    hapd = hostapd.add_ap(apdev[0], params)

    tests = ["#\n\n0\t11\n",
             "1  netdev  br\n1",
             "* ",
             "1 netdev12345678901234567890"]
    for t in tests:
        with open(tmp, "w") as f:
            f.write(t)
        if "FAIL" not in hapd.request("SET vlan_file " + tmp):
            raise Exception("Invalid vlan_file accepted")

    with open(tmp, "w") as f:
        f.write("1\tvlan\n")
    with alloc_fail(hapd, 1, "=hostapd_config_read_vlan_file"):
        if "FAIL" not in hapd.request("SET vlan_file " + tmp):
            raise Exception("vlan_file accepted during OOM")

    tests = ["#\n\n0\tvlan\n",
             "4095\tvlan\n",
             "vlan\n",
             "1\t1234567890abcdef1234567890\n",
             "1\n"]
    for t in tests:
        with open(tmp, "w") as f:
            f.write(t)
        if "FAIL" not in hapd.request("SET accept_mac_file " + tmp):
            raise Exception("Invalid accept_mac_file accepted")

    with open(tmp, "w") as f:
        f.write("00:11:22:33:44:55\n")
    with alloc_fail(hapd, 1, "hostapd_config_read_maclist"):
        if "FAIL" not in hapd.request("SET accept_mac_file " + tmp):
            raise Exception("accept_mac_file accepted during OOM")

def test_ap_vlan_wpa2(dev, apdev):
    """AP VLAN with WPA2-PSK"""
    filename = hostapd.acl_file(dev, apdev, 'hostapd.accept')
    hostapd.send_file(apdev[0], filename, filename)
    params = hostapd.wpa2_params(ssid="test-vlan",
                                 passphrase="12345678")
    params['dynamic_vlan'] = "1"
    params['accept_mac_file'] = filename
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].connect("test-vlan", psk="12345678", scan_freq="2412")
    dev[1].connect("test-vlan", psk="12345678", scan_freq="2412")
    dev[2].connect("test-vlan", psk="12345678", scan_freq="2412")
    hapd.wait_sta()
    hapd.wait_sta()
    hapd.wait_sta()
    hwsim_utils.test_connectivity_iface(dev[0], hapd, "brvlan1")
    hwsim_utils.test_connectivity_iface(dev[1], hapd, "brvlan2")
    hwsim_utils.test_connectivity(dev[2], hapd)
    if filename.startswith('/tmp/'):
        os.unlink(filename)

def test_ap_vlan_wpa2_radius(dev, apdev):
    """AP VLAN with WPA2-Enterprise and RADIUS attributes"""
    params = hostapd.wpa2_eap_params(ssid="test-vlan")
    params['dynamic_vlan'] = "1"
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].connect("test-vlan", key_mgmt="WPA-EAP", eap="PAX",
                   identity="vlan1",
                   password_hex="0123456789abcdef0123456789abcdef",
                   scan_freq="2412")
    dev[1].connect("test-vlan", key_mgmt="WPA-EAP", eap="PAX",
                   identity="vlan2",
                   password_hex="0123456789abcdef0123456789abcdef",
                   scan_freq="2412")
    dev[2].connect("test-vlan", key_mgmt="WPA-EAP", eap="PAX",
                   identity="pax.user@example.com",
                   password_hex="0123456789abcdef0123456789abcdef",
                   scan_freq="2412")
    hapd.wait_sta()
    hapd.wait_sta()
    hapd.wait_sta()
    hwsim_utils.test_connectivity_iface(dev[0], hapd, "brvlan1")
    hwsim_utils.test_connectivity_iface(dev[1], hapd, "brvlan2")
    hwsim_utils.test_connectivity(dev[2], hapd)

def test_ap_vlan_wpa2_radius_2(dev, apdev):
    """AP VLAN with WPA2-Enterprise and RADIUS EGRESS_VLANID attributes"""
    params = hostapd.wpa2_eap_params(ssid="test-vlan")
    params['dynamic_vlan'] = "1"
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].connect("test-vlan", key_mgmt="WPA-EAP", eap="PAX",
                   identity="vlan1b",
                   password_hex="0123456789abcdef0123456789abcdef",
                   scan_freq="2412")

    hapd.wait_sta()
    hwsim_utils.test_connectivity_iface(dev[0], hapd, "brvlan1")

def test_ap_vlan_wpa2_radius_local(dev, apdev):
    """AP VLAN with WPA2-Enterprise and local file setting VLAN IDs"""
    filename = hostapd.acl_file(dev, apdev, 'hostapd.accept')
    hostapd.send_file(apdev[0], filename, filename)
    params = hostapd.wpa2_eap_params(ssid="test-vlan")
    params['dynamic_vlan'] = "0"
    params['vlan_file'] = "hostapd.vlan"
    params['vlan_bridge'] = "test_br_vlan"
    params['accept_mac_file'] = filename
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].connect("test-vlan", key_mgmt="WPA-EAP", eap="PAX",
                   identity="pax.user@example.com",
                   password_hex="0123456789abcdef0123456789abcdef",
                   scan_freq="2412")
    dev[1].connect("test-vlan", key_mgmt="WPA-EAP", eap="PAX",
                   identity="pax.user@example.com",
                   password_hex="0123456789abcdef0123456789abcdef",
                   scan_freq="2412")
    dev[2].connect("test-vlan", key_mgmt="WPA-EAP", eap="PAX",
                   identity="pax.user@example.com",
                   password_hex="0123456789abcdef0123456789abcdef",
                   scan_freq="2412")
    hapd.wait_sta()
    hapd.wait_sta()
    hapd.wait_sta()
    hwsim_utils.test_connectivity_iface(dev[0], hapd, "test_br_vlan1")
    hwsim_utils.test_connectivity_iface(dev[1], hapd, "test_br_vlan2")
    hwsim_utils.test_connectivity(dev[2], hapd)
    if filename.startswith('/tmp/'):
        os.unlink(filename)

def test_ap_vlan_wpa2_radius_id_change(dev, apdev):
    """AP VLAN with WPA2-Enterprise and RADIUS attributes changing VLANID"""
    generic_ap_vlan_wpa2_radius_id_change(dev, apdev, False)

def test_ap_vlan_tagged_wpa2_radius_id_change(dev, apdev):
    """AP tagged VLAN with WPA2-Enterprise and RADIUS attributes changing VLANID"""
    ifname1 = 'wlan0.1'
    ifname2 = 'wlan0.2'
    try:
        # Create tagged interface for wpa_supplicant
        subprocess.call(['ip', 'link', 'add', 'link', dev[0].ifname,
                         'name', ifname1, 'type', 'vlan', 'id', '1'])
        subprocess.call(['ifconfig', ifname1, 'up'])

        subprocess.call(['ip', 'link', 'add', 'link', dev[0].ifname,
                         'name', ifname2, 'type', 'vlan', 'id', '2'])
        subprocess.call(['ifconfig', ifname2, 'up'])

        generic_ap_vlan_wpa2_radius_id_change(dev, apdev, True)
    finally:
        subprocess.call(['ifconfig', ifname1, 'down'])
        subprocess.call(['ifconfig', ifname2, 'down'])
        subprocess.call(['ip', 'link', 'del', ifname1])
        subprocess.call(['ip', 'link', 'del', ifname2])

def generic_ap_vlan_wpa2_radius_id_change(dev, apdev, tagged):
    as_params = {"ssid": "as",
                 "beacon_int": "2000",
                 "radius_server_clients": "auth_serv/radius_clients.conf",
                 "radius_server_auth_port": '18128',
                 "eap_server": "1",
                 "eap_user_file": "auth_serv/eap_user.conf",
                 "ca_cert": "auth_serv/ca.pem",
                 "server_cert": "auth_serv/server.pem",
                 "private_key": "auth_serv/server.key"}
    authserv = hostapd.add_ap(apdev[1], as_params)

    params = hostapd.wpa2_eap_params(ssid="test-vlan")
    params['dynamic_vlan'] = "1"
    params['auth_server_port'] = "18128"
    hapd = hostapd.add_ap(apdev[0], params)

    identity = "vlan1tagged" if tagged else "vlan1"

    dev[0].connect("test-vlan", key_mgmt="WPA-EAP", eap="PAX",
                   identity=identity,
                   password_hex="0123456789abcdef0123456789abcdef",
                   scan_freq="2412")
    hapd.wait_sta()
    if tagged:
        hwsim_utils.run_connectivity_test(dev[0], hapd, 0, ifname1="wlan0.1",
                                          ifname2="brvlan1")
    else:
        hwsim_utils.test_connectivity_iface(dev[0], hapd, "brvlan1")

    logger.info("VLAN-ID -> 2")

    authserv.disable()
    authserv.set('eap_user_file', "auth_serv/eap_user_vlan.conf")
    authserv.enable()

    dev[0].dump_monitor()
    dev[0].request("REAUTHENTICATE")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-SUCCESS"], timeout=15)
    if ev is None:
        raise Exception("EAP reauthentication timed out")
    ev = dev[0].wait_event(["WPA: Key negotiation completed"], timeout=5)
    if ev is None:
        raise Exception("4-way handshake after reauthentication timed out")
    state = dev[0].get_status_field('wpa_state')
    if state != "COMPLETED":
        raise Exception("Unexpected state after reauth: " + state)
    sta = hapd.get_sta(dev[0].own_addr())
    if 'vlan_id' not in sta:
        raise Exception("No VLAN ID in STA info")
    if (not tagged) and (sta['vlan_id'] != '2'):
        raise Exception("Unexpected VLAN ID: " + sta['vlan_id'])
    if tagged:
        hwsim_utils.run_connectivity_test(dev[0], hapd, 0, ifname1="wlan0.2",
                                          ifname2="brvlan2")
    else:
        hwsim_utils.test_connectivity_iface(dev[0], hapd, "brvlan2")

    logger.info("VLAN-ID -> 1")
    time.sleep(1)

    authserv.disable()
    authserv.set('eap_user_file', "auth_serv/eap_user.conf")
    authserv.enable()

    dev[0].dump_monitor()
    dev[0].request("REAUTHENTICATE")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-SUCCESS"], timeout=15)
    if ev is None:
        raise Exception("EAP reauthentication timed out")
    ev = dev[0].wait_event(["WPA: Key negotiation completed"], timeout=5)
    if ev is None:
        raise Exception("4-way handshake after reauthentication timed out")
    state = dev[0].get_status_field('wpa_state')
    if state != "COMPLETED":
        raise Exception("Unexpected state after reauth: " + state)
    sta = hapd.get_sta(dev[0].own_addr())
    if 'vlan_id' not in sta:
        raise Exception("No VLAN ID in STA info")
    if (not tagged) and (sta['vlan_id'] != '1'):
        raise Exception("Unexpected VLAN ID: " + sta['vlan_id'])
    time.sleep(0.2)
    try:
        if tagged:
            hwsim_utils.run_connectivity_test(dev[0], hapd, 0,
                                              ifname1="wlan0.1",
                                              ifname2="brvlan1")
        else:
            hwsim_utils.test_connectivity_iface(dev[0], hapd, "brvlan1")
    except Exception as e:
        # It is possible for new bridge setup to not be ready immediately, so
        # try again to avoid reporting issues related to that.
        logger.info("First VLAN-ID 1 data test failed - try again")
        if tagged:
            hwsim_utils.run_connectivity_test(dev[0], hapd, 0,
                                              ifname1="wlan0.1",
                                              ifname2="brvlan1")
        else:
            hwsim_utils.test_connectivity_iface(dev[0], hapd, "brvlan1")

def test_ap_vlan_wpa2_radius_required(dev, apdev):
    """AP VLAN with WPA2-Enterprise and RADIUS attributes required"""
    params = hostapd.wpa2_eap_params(ssid="test-vlan")
    params['dynamic_vlan'] = "2"
    hostapd.add_ap(apdev[0], params)

    dev[0].connect("test-vlan", key_mgmt="WPA-EAP", eap="PAX",
                   identity="vlan1",
                   password_hex="0123456789abcdef0123456789abcdef",
                   scan_freq="2412")
    dev[2].connect("test-vlan", key_mgmt="WPA-EAP", eap="PAX",
                   identity="pax.user@example.com",
                   password_hex="0123456789abcdef0123456789abcdef",
                   scan_freq="2412", wait_connect=False)
    ev = dev[2].wait_event(["CTRL-EVENT-CONNECTED",
                            "CTRL-EVENT-DISCONNECTED"], timeout=20)
    if ev is None:
        raise Exception("Timeout on connection attempt")
    if "CTRL-EVENT-CONNECTED" in ev:
        raise Exception("Unexpected success without tunnel parameters")

def test_ap_vlan_tagged(dev, apdev):
    """AP VLAN with tagged interface"""
    filename = hostapd.acl_file(dev, apdev, 'hostapd.accept')
    hostapd.send_file(apdev[0], filename, filename)
    params = {"ssid": "test-vlan-open",
              "dynamic_vlan": "1",
              "vlan_tagged_interface": "lo",
              "accept_mac_file": filename}
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].connect("test-vlan-open", key_mgmt="NONE", scan_freq="2412")
    dev[1].connect("test-vlan-open", key_mgmt="NONE", scan_freq="2412")
    dev[2].connect("test-vlan-open", key_mgmt="NONE", scan_freq="2412")
    hwsim_utils.test_connectivity_iface(dev[0], hapd, "brlo.1")
    hwsim_utils.test_connectivity_iface(dev[1], hapd, "brlo.2")
    hwsim_utils.test_connectivity(dev[2], hapd)
    if filename.startswith('/tmp/'):
        os.unlink(filename)

def ap_vlan_iface_cleanup_multibss_cleanup():
    subprocess.call(['ifconfig', 'stub0', 'down'],
                    stderr=open('/dev/null', 'w'))
    ifnames = ['wlan3.1', 'wlan3.2', 'wlan3-2.1', 'wlan3-2.2', 'stub0.2',
               'stub0.1', 'stub0', 'brvlan1', 'brvlan2']
    for ifname in ifnames:
        subprocess.call(['ip', 'link', 'del', ifname],
                        stderr=open('/dev/null', 'w'))

def ap_vlan_iface_test_and_prepare_environ():
    ifaces = netifaces.interfaces()
    if "stub0" in ifaces:
        raise Exception("stub0 already exists before")
    ifaces = netifaces.interfaces()
    if "stub0.1" in ifaces:
        raise Exception("stub0.1 already exists before")

    subprocess.call(['ip', 'link', 'add', 'stub0', 'type', 'dummy'])
    subprocess.call(['ifconfig', 'stub0', 'up'])

    ifaces = netifaces.interfaces()
    if "stub0" not in ifaces:
        raise HwsimSkip("failed to add stub0 - missing kernel config DUMMY ?")

    subprocess.call(['ip', 'link', 'add', 'link', 'stub0', 'name', 'stub0.1',
                     'type', 'vlan', 'id', '1'])

    ifaces = netifaces.interfaces()
    if "stub0.1" not in ifaces:
        raise HwsimSkip("failed to add stub0.1 - missing kernel config VLAN_8021Q ?")

    subprocess.call(['ip', 'link', 'del', 'stub0.1'])

    ifaces = netifaces.interfaces()
    if "stub0.1" in ifaces:
        raise Exception("stub0.1 was not removed before testing")

def test_ap_vlan_iface_cleanup_multibss(dev, apdev):
    """AP VLAN operation in multi-BSS multi-VLAN case"""
    ap_vlan_iface_cleanup_multibss(dev, apdev, 'multi-bss-iface.conf')

def ap_vlan_iface_cleanup_multibss(dev, apdev, cfgfile):
    # AP VLAN with WPA2-Enterprise and RADIUS attributes changing VLANID
    # check that multiple bss do not interfere with each other with respect
    # to deletion of bridge and tagged interface.

    if not netifaces_imported:
        raise HwsimSkip("python module netifaces not available")

    try:
        ap_vlan_iface_cleanup_multibss_cleanup()
        ap_vlan_iface_test_and_prepare_environ()

        as_params = {"ssid": "as",
                     "beacon_int": "2000",
                     "radius_server_clients": "auth_serv/radius_clients.conf",
                     "radius_server_auth_port": '18128',
                     "eap_server": "1",
                     "eap_user_file": "auth_serv/eap_user.conf",
                     "ca_cert": "auth_serv/ca.pem",
                     "server_cert": "auth_serv/server.pem",
                     "private_key": "auth_serv/server.key",
                     "vlan_naming": "1"}
        authserv = hostapd.add_ap(apdev[1], as_params)

        # start the actual test
        hapd = hostapd.add_iface(apdev[0], cfgfile)
        hapd1 = hostapd.Hostapd("wlan3-2", 1)
        hapd1.enable()

        ifaces = netifaces.interfaces()
        if "brvlan1" in ifaces:
            raise Exception("bridge brvlan1 already exists before")
        if "brvlan2" in ifaces:
            raise Exception("bridge brvlan2 already exists before")

        dev[0].connect("bss-1", key_mgmt="WPA-EAP", eap="PAX",
                       identity="vlan1",
                       password_hex="0123456789abcdef0123456789abcdef",
                       scan_freq="2412")
        hapd.wait_sta()

        ifaces = netifaces.interfaces()
        if "brvlan1" not in ifaces:
            raise Exception("bridge brvlan1 was not created")

        hwsim_utils.test_connectivity_iface(dev[0], hapd, "brvlan1")
        if not iface_is_in_bridge("brvlan1", "stub0.1"):
            raise Exception("stub0.1 not in brvlan1")

        dev[1].connect("bss-2", key_mgmt="WPA-EAP", eap="PAX",
                       identity="vlan1",
                       password_hex="0123456789abcdef0123456789abcdef",
                       scan_freq="2412")

        hapd1.wait_sta()
        hwsim_utils.test_connectivity_iface(dev[1], hapd1, "brvlan1")
        if not iface_is_in_bridge("brvlan1", "stub0.1"):
            raise Exception("stub0.1 not in brvlan1")

        authserv.disable()
        authserv.set('eap_user_file', "auth_serv/eap_user_vlan.conf")
        authserv.enable()

        logger.info("wlan0 -> VLAN 2")

        dev[0].dump_monitor()
        dev[0].request("REAUTHENTICATE")
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-SUCCESS"], timeout=15)
        if ev is None:
            raise Exception("EAP reauthentication timed out")
        ev = dev[0].wait_event(["WPA: Key negotiation completed"], timeout=5)
        if ev is None:
            raise Exception("4-way handshake after reauthentication timed out")
        state = dev[0].get_status_field('wpa_state')
        if state != "COMPLETED":
            raise Exception("Unexpected state after reauth: " + state)

        ifaces = netifaces.interfaces()
        if "brvlan1" not in ifaces:
            raise Exception("bridge brvlan1 has been removed too early")

        hwsim_utils.test_connectivity_iface(dev[0], hapd, "brvlan2",
                                            max_tries=5)

        if not iface_is_in_bridge("brvlan2", "stub0.2"):
            raise Exception("stub0.2 not in brvlan2")

        logger.info("test wlan1 == VLAN 1")
        hwsim_utils.test_connectivity_iface(dev[1], hapd1, "brvlan1")
        if not iface_is_in_bridge("brvlan1", "stub0.1"):
            raise Exception("stub0.1 not in brvlan1")

        logger.info("wlan1 -> VLAN 2")

        dev[1].dump_monitor()
        dev[1].request("REAUTHENTICATE")
        ev = dev[1].wait_event(["CTRL-EVENT-EAP-SUCCESS"], timeout=15)
        if ev is None:
            raise Exception("EAP reauthentication timed out")
        ev = dev[1].wait_event(["WPA: Key negotiation completed"], timeout=5)
        if ev is None:
            raise Exception("4-way handshake after reauthentication timed out")
        state = dev[1].get_status_field('wpa_state')
        if state != "COMPLETED":
            raise Exception("Unexpected state after reauth: " + state)

        # it can take some time for data connectivity to be updated
        hwsim_utils.test_connectivity_iface(dev[1], hapd1, "brvlan2",
                                            max_tries=5)
        logger.info("test wlan0 == VLAN 2")
        hwsim_utils.test_connectivity_iface(dev[0], hapd, "brvlan2")

        if not iface_is_in_bridge("brvlan2", "stub0.2"):
            raise Exception("stub0.2 not in brvlan2")

        ifaces = netifaces.interfaces()
        if "brvlan1" in ifaces:
            raise Exception("bridge brvlan1 has not been cleaned up")

        # disconnect dev0 first to test a corner case
        dev[0].request("DISCONNECT")
        dev[0].wait_disconnected()
        dev[1].request("DISCONNECT")
        dev[1].wait_disconnected()

        # station removal needs some time
        for i in range(15):
            time.sleep(1)
            ifaces = netifaces.interfaces()
            if "brvlan2" not in ifaces:
                break

        ifaces = netifaces.interfaces()
        if "brvlan2" in ifaces:
            raise Exception("bridge brvlan2 has not been cleaned up")

        hapd.request("DISABLE")
    finally:
        ap_vlan_iface_cleanup_multibss_cleanup()

def test_ap_vlan_iface_cleanup_multibss_per_sta_vif(dev, apdev):
    """AP VLAN operation in multi-BSS multi-VLAN case with per-sta-vif set"""

    # AP VLAN with WPA2-Enterprise and RADIUS attributes changing VLANID
    # check that multiple bss do not interfere with each other with respect
    # to deletion of bridge and tagged interface. per_sta_vif is enabled.
    ap_vlan_iface_cleanup_multibss(dev, apdev,
                                   'multi-bss-iface-per_sta_vif.conf')

def test_ap_vlan_without_station(dev, apdev, p):
    """AP VLAN with WPA2-PSK and no station"""
    try:
        filename = hostapd.acl_file(dev, apdev, 'hostapd.accept')
        hostapd.send_file(apdev[0], filename, filename)
        subprocess.call(['brctl', 'addbr', 'brvlan1'])
        subprocess.call(['brctl', 'setfd', 'brvlan1', '0'])
        subprocess.call(['ifconfig', 'brvlan1', 'up'])
        # use a passphrase wlantest does not know, so it cannot
        # inject decrypted frames into pcap
        params = hostapd.wpa2_params(ssid="test-vlan",
                                     passphrase="12345678x")
        params['dynamic_vlan'] = "1"
        params['vlan_file'] = 'hostapd.wlan3.vlan'
        params['accept_mac_file'] = filename
        hapd = hostapd.add_ap(apdev[0], params)

        # inject some traffic
        sa = hapd.own_addr()
        da = "ff:ff:ff:ff:ff:00"
        hapd.request('DATA_TEST_CONFIG 1 ifname=brvlan1')
        hapd.request('DATA_TEST_TX {} {} 0'.format(da, sa))
        hapd.request('DATA_TEST_CONFIG 0')
        time.sleep(.1)

        dev[0].connect("test-vlan", psk="12345678x", scan_freq="2412")

        # inject some traffic
        sa = hapd.own_addr()
        da = "ff:ff:ff:ff:ff:01"
        hapd.request('DATA_TEST_CONFIG 1 ifname=brvlan1')
        hapd.request('DATA_TEST_TX {} {} 0'.format(da, sa))
        hapd.request('DATA_TEST_CONFIG 0')

        # let the AP send couple of Beacon frames
        time.sleep(1)
        out = run_tshark(os.path.join(p['logdir'], "hwsim0.pcapng"),
                         "wlan.da == ff:ff:ff:ff:ff:00",
                         ["wlan.fc.protected"])

        if out is not None:
            lines = out.splitlines()
            if len(lines) < 1:
                # Newer kernel versions filter out frames when there are no
                # authorized stations on an AP/AP_VLAN interface, so do not
                # trigger an error here.
                logger.info("first frame not observed")
            state = 1
            for l in lines:
                is_protected = int(l, 16)
                if is_protected != 1:
                    state = 0
            if state != 1:
                raise Exception("Broadcast packets were not encrypted when no station was connected")
        else:
            raise Exception("first frame not observed")

        out = run_tshark(os.path.join(p['logdir'], "hwsim0.pcapng"),
                         "wlan.da == ff:ff:ff:ff:ff:01",
                         ["wlan.fc.protected"])

        if out is not None:
            lines = out.splitlines()
            if len(lines) < 1:
                raise Exception("second frame not observed")
            state = 1
            for l in lines:
                is_protected = int(l, 16)
                if is_protected != 1:
                    state = 0
            if state != 1:
                raise Exception("Broadcast packets were not encrypted when station was connected")
        else:
            raise Exception("second frame not observed")

        dev[0].request("DISCONNECT")
        dev[0].wait_disconnected()
        if filename.startswith('/tmp/'):
            os.unlink(filename)

    finally:
        subprocess.call(['ip', 'link', 'set', 'dev', 'brvlan1', 'down'])
        subprocess.call(['ip', 'link', 'set', 'dev', 'wlan3.1', 'down'],
                        stderr=open('/dev/null', 'w'))
        subprocess.call(['brctl', 'delif', 'brvlan1', 'wlan3.1'],
                        stderr=open('/dev/null', 'w'))
        subprocess.call(['brctl', 'delbr', 'brvlan1'])

@remote_compatible
def test_ap_open_per_sta_vif(dev, apdev):
    """AP VLAN with open network"""
    params = {"ssid": "test-vlan-open",
              "per_sta_vif": "1"}
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].connect("test-vlan-open", key_mgmt="NONE", scan_freq="2412")
    hwsim_utils.test_connectivity_iface(dev[0], hapd,
                                        apdev[0]['ifname'] + ".4096")

@remote_compatible
def test_ap_vlan_open_per_sta_vif(dev, apdev):
    """AP VLAN (dynamic) with open network"""
    params = {"ssid": "test-vlan-open",
              "per_sta_vif": "1",
              "dynamic_vlan": "1"}
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].connect("test-vlan-open", key_mgmt="NONE", scan_freq="2412")
    hwsim_utils.test_connectivity_iface(dev[0], hapd,
                                        apdev[0]['ifname'] + ".4096")

def test_ap_vlan_wpa2_radius_tagged(dev, apdev):
    """AP VLAN with WPA2-Enterprise and RADIUS EGRESS_VLANID attributes"""
    ifname = 'wlan0.1'
    try:
        params = hostapd.wpa2_eap_params(ssid="test-vlan")
        params['dynamic_vlan'] = "1"
        params["vlan_naming"] = "1"
        hapd = hostapd.add_ap(apdev[0], params)

        dev[0].connect("test-vlan", key_mgmt="WPA-EAP", eap="PAX",
                       identity="vlan1tagged",
                       password_hex="0123456789abcdef0123456789abcdef",
                       scan_freq="2412")

        # Create tagged interface for wpa_supplicant
        subprocess.call(['ip', 'link', 'add', 'link', dev[0].ifname,
                         'name', ifname, 'type', 'vlan', 'id', '1'])
        subprocess.call(['ifconfig', ifname, 'up'])

        hwsim_utils.run_connectivity_test(dev[0], hapd, 0, ifname1=ifname,
                                          ifname2="brvlan1")
    finally:
        subprocess.call(['ifconfig', ifname, 'down'])
        subprocess.call(['ip', 'link', 'del', ifname])

def test_ap_vlan_wpa2_radius_mixed(dev, apdev):
    """AP VLAN with WPA2-Enterprise and tagged+untagged VLANs"""
    ifname = 'wlan0.1'
    try:
        params = hostapd.wpa2_eap_params(ssid="test-vlan")
        params['dynamic_vlan'] = "1"
        params["vlan_naming"] = "1"
        hapd = hostapd.add_ap(apdev[0], params)

        dev[0].connect("test-vlan", key_mgmt="WPA-EAP", eap="PAX",
                       identity="vlan12mixed",
                       password_hex="0123456789abcdef0123456789abcdef",
                       scan_freq="2412")

        # Add tagged VLAN interface to wpa_supplicant interface for testing
        subprocess.call(['ip', 'link', 'add', 'link', dev[0].ifname,
                         'name', ifname, 'type', 'vlan', 'id', '1'])
        subprocess.call(['ifconfig', ifname, 'up'])

        logger.info("Test connectivity in untagged VLAN 2")
        hwsim_utils.run_connectivity_test(dev[0], hapd, 0,
                                          ifname1=dev[0].ifname,
                                          ifname2="brvlan2")
        logger.info("Test connectivity in tagged VLAN 1")
        hwsim_utils.run_connectivity_test(dev[0], hapd, 0, ifname1=ifname,
                                          ifname2="brvlan1")
    finally:
        subprocess.call(['ifconfig', ifname, 'down'])
        subprocess.call(['ip', 'link', 'del', ifname])

def test_ap_vlan_reconnect(dev, apdev):
    """AP VLAN with WPA2-PSK connect, disconnect, connect"""
    filename = hostapd.acl_file(dev, apdev, 'hostapd.accept')
    hostapd.send_file(apdev[0], filename, filename)
    params = hostapd.wpa2_params(ssid="test-vlan",
                                 passphrase="12345678")
    params['dynamic_vlan'] = "1"
    params['accept_mac_file'] = filename
    hapd = hostapd.add_ap(apdev[0], params)

    logger.info("connect sta")
    dev[0].connect("test-vlan", psk="12345678", scan_freq="2412")
    hapd.wait_sta()
    hwsim_utils.test_connectivity_iface(dev[0], hapd, "brvlan1")
    logger.info("disconnect sta")
    dev[0].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected(timeout=10)
    time.sleep(1)
    logger.info("reconnect sta")
    dev[0].connect("test-vlan", psk="12345678", scan_freq="2412")
    hapd.wait_sta()
    hwsim_utils.test_connectivity_iface(dev[0], hapd, "brvlan1")
    if filename.startswith('/tmp/'):
        os.unlink(filename)

def test_ap_vlan_psk(dev, apdev, params):
    """AP VLAN based on PSK/passphrase"""
    psk_file = os.path.join(params['logdir'], 'ap_vlan_psk.wpa_psk')
    with open(psk_file, 'w') as f:
        f.write('vlanid=1 00:00:00:00:00:00 passphrase-for-vlan-1\n')
        f.write('vlanid=2 00:00:00:00:00:00 passphrase-for-vlan-2\n')
        f.write('vlanid=3 00:00:00:00:00:00 passphrase-for-vlan-3\n')

    ssid = 'test-vlan-rsn'
    params = hostapd.wpa2_params(ssid=ssid)
    params['dynamic_vlan'] = "1"
    params['wpa_psk_file'] = psk_file
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].connect(ssid, psk="passphrase-for-vlan-1", scan_freq="2412")
    dev[1].connect(ssid, psk="passphrase-for-vlan-2", scan_freq="2412")
    dev[2].connect(ssid, psk="passphrase-for-vlan-3", scan_freq="2412")
    hapd.wait_sta()
    hapd.wait_sta()
    hapd.wait_sta()
    hwsim_utils.test_connectivity_iface(dev[0], hapd, "brvlan1")
    hwsim_utils.test_connectivity_iface(dev[1], hapd, "brvlan2")
    hwsim_utils.test_connectivity_iface(dev[2], hapd, "brvlan3")

def test_ap_vlan_sae(dev, apdev, params):
    """AP VLAN based on SAE Password Identifier"""
    for i in range(3):
        if "SAE" not in dev[i].get_capability("auth_alg"):
            raise HwsimSkip("SAE not supported")
    params = hostapd.wpa2_params(ssid="test-sae-vlan")
    params['wpa_key_mgmt'] = 'SAE'
    params['sae_password'] = ['pw1|vlanid=1|id=id1',
                              'pw2|mac=ff:ff:ff:ff:ff:ff|vlanid=2|id=id2',
                              'pw3|vlanid=3|id=id3']
    params['dynamic_vlan'] = "1"
    hapd = hostapd.add_ap(apdev[0], params)

    for i in range(3):
        dev[i].request("SET sae_groups ")
        dev[i].connect("test-sae-vlan", sae_password="pw%d" % (i + 1),
                       sae_password_id="id%d" % (i + 1),
                       key_mgmt="SAE", scan_freq="2412")
        hapd.wait_sta()

    hwsim_utils.test_connectivity_iface(dev[0], hapd, "brvlan1")
    hwsim_utils.test_connectivity_iface(dev[1], hapd, "brvlan2")
    hwsim_utils.test_connectivity_iface(dev[2], hapd, "brvlan3")
