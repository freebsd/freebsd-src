# Hotspot 2.0 filtering tests
# Copyright (c) 2015, Intel Deutschland GmbH
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

import hostapd
import hwsim_utils
import socket
import subprocess
import binascii
from utils import HwsimSkip, require_under_vm
import os
import time
from test_ap_hs20 import build_arp, build_na, hs20_ap_params
from test_ap_hs20 import interworking_select, interworking_connect
import struct
import logging
logger = logging.getLogger()

class IPAssign(object):
    def __init__(self, iface, addr, ipv6=False):
        self._iface = iface
        self._addr = addr
        self._cmd = ['ip']
        if ipv6:
            self._cmd.append('-6')
        self._cmd.append('addr')
        self._ipv6 = ipv6
    def __enter__(self):
        subprocess.call(self._cmd + ['add', self._addr, 'dev', self._iface])
        if self._ipv6:
            # wait for DAD to finish
            while True:
                o = subprocess.check_output(self._cmd + ['show', 'tentative', 'dev', self._iface]).decode()
                if self._addr not in o:
                    break
                time.sleep(0.1)
    def __exit__(self, type, value, traceback):
        subprocess.call(self._cmd + ['del', self._addr, 'dev', self._iface])

def hs20_filters_connect(dev, apdev, disable_dgaf=False, proxy_arp=False):
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid

    # Do not disable dgaf, to test that the station drops unicast IP packets
    # encrypted with GTK.
    params['disable_dgaf'] = '0'
    params['proxy_arp'] = '1'
    params['ap_isolate'] = '1'
    params['bridge'] = 'ap-br0'

    try:
        hapd = hostapd.add_ap(apdev[0], params)
    except:
        # For now, do not report failures due to missing kernel support.
        raise HwsimSkip("Could not start hostapd - assume proxyarp not supported in the kernel")

    subprocess.call(['brctl', 'setfd', 'ap-br0', '0'])
    subprocess.call(['ip', 'link', 'set', 'dev', 'ap-br0', 'up'])

    dev[0].hs20_enable()

    id = dev[0].add_cred_values({'realm': "example.com",
                                 'username': "hs20-test",
                                 'password': "password",
                                 'ca_cert': "auth_serv/ca.pem",
                                 'domain': "example.com",
                                 'update_identifier': "1234"})
    interworking_select(dev[0], bssid, "home", freq="2412")
    interworking_connect(dev[0], bssid, "TTLS")

    time.sleep(0.1)

    return dev[0], hapd

def _test_ip4_gtk_drop(devs, apdevs, params, dst):
    require_under_vm()
    procfile = '/proc/sys/net/ipv4/conf/%s/drop_unicast_in_l2_multicast' % devs[0].ifname
    if not os.path.exists(procfile):
        raise HwsimSkip("kernel doesn't have capability")

    [dev, hapd] = hs20_filters_connect(devs, apdevs)
    with IPAssign(dev.ifname, '10.0.0.1/24'):
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.bind(("10.0.0.1", 12345))
        s.settimeout(0.1)

        pkt = dst
        pkt += hapd.own_addr().replace(':', '')
        pkt += '0800'
        pkt += '45000020786840004011ae600a0000040a000001'
        pkt += '30393039000c0000'
        pkt += '61736466' # "asdf"
        if "OK" not in hapd.request('DATA_TEST_FRAME ' + pkt):
            raise Exception("DATA_TEST_FRAME failed")
        try:
            logger.info(s.recvfrom(1024))
            logger.info("procfile=" + procfile + " val=" + open(procfile, 'r').read().rstrip())
            raise Exception("erroneously received frame!")
        except socket.timeout:
            # this is the expected behaviour
            pass

def test_ip4_gtk_drop_bcast(devs, apdevs, params):
    """Hotspot 2.0 frame filtering - IPv4 GTK drop broadcast"""
    _test_ip4_gtk_drop(devs, apdevs, params, dst='ffffffffffff')

def test_ip4_gtk_drop_mcast(devs, apdevs, params):
    """Hotspot 2.0 frame filtering - IPv4 GTK drop multicast"""
    _test_ip4_gtk_drop(devs, apdevs, params, dst='ff0000000000')

def _test_ip6_gtk_drop(devs, apdevs, params, dst):
    require_under_vm()
    dev = devs[0]
    procfile = '/proc/sys/net/ipv6/conf/%s/drop_unicast_in_l2_multicast' % devs[0].ifname
    if not os.path.exists(procfile):
        raise HwsimSkip("kernel doesn't have capability")

    [dev, hapd] = hs20_filters_connect(devs, apdevs)

    with IPAssign(dev.ifname, 'fdaa::1/48', ipv6=True):
        s = socket.socket(socket.AF_INET6, socket.SOCK_DGRAM)
        s.bind(("fdaa::1", 12345))
        s.settimeout(0.1)

        pkt = dst
        pkt += hapd.own_addr().replace(':', '')
        pkt += '86dd'
        pkt += '60000000000c1140fdaa0000000000000000000000000002fdaa0000000000000000000000000001'
        pkt += '30393039000cde31'
        pkt += '61736466' # "asdf"
        if "OK" not in hapd.request('DATA_TEST_FRAME ' + pkt):
            raise Exception("DATA_TEST_FRAME failed")
        try:
            logger.info(s.recvfrom(1024))
            logger.info("procfile=" + procfile + " val=" + open(procfile, 'r').read().rstrip())
            raise Exception("erroneously received frame!")
        except socket.timeout:
            # this is the expected behaviour
            pass

def test_ip6_gtk_drop_bcast(devs, apdevs, params):
    """Hotspot 2.0 frame filtering - IPv6 GTK drop broadcast"""
    _test_ip6_gtk_drop(devs, apdevs, params, dst='ffffffffffff')

def test_ip6_gtk_drop_mcast(devs, apdevs, params):
    """Hotspot 2.0 frame filtering - IPv6 GTK drop multicast"""
    _test_ip6_gtk_drop(devs, apdevs, params, dst='ff0000000000')

def test_ip4_drop_gratuitous_arp(devs, apdevs, params):
    """Hotspot 2.0 frame filtering - IPv4 drop gratuitous ARP"""
    require_under_vm()
    procfile = '/proc/sys/net/ipv4/conf/%s/drop_gratuitous_arp' % devs[0].ifname
    if not os.path.exists(procfile):
        raise HwsimSkip("kernel doesn't have capability")

    [dev, hapd] = hs20_filters_connect(devs, apdevs)

    with IPAssign(dev.ifname, '10.0.0.2/24'):
        # add an entry that can be updated by gratuitous ARP
        subprocess.call(['ip', 'neigh', 'add', '10.0.0.1', 'lladdr', '02:00:00:00:00:ff', 'nud', 'reachable', 'dev', dev.ifname])
        # wait for lock-time
        time.sleep(1)
        try:
            ap_addr = hapd.own_addr()
            cl_addr = dev.own_addr()
            pkt = build_arp(cl_addr, ap_addr, 2, ap_addr, '10.0.0.1', ap_addr, '10.0.0.1')
            pkt = binascii.hexlify(pkt).decode()

            if "OK" not in hapd.request('DATA_TEST_FRAME ' + pkt):
                raise Exception("DATA_TEST_FRAME failed")

            if hapd.own_addr() in subprocess.check_output(['ip', 'neigh', 'show']).decode():
                raise Exception("gratuitous ARP frame updated erroneously")
        finally:
            subprocess.call(['ip', 'neigh', 'del', '10.0.0.1', 'dev', dev.ifname])

def test_ip6_drop_unsolicited_na(devs, apdevs, params):
    """Hotspot 2.0 frame filtering - IPv6 drop unsolicited NA"""
    require_under_vm()
    procfile = '/proc/sys/net/ipv6/conf/%s/drop_unsolicited_na' % devs[0].ifname
    if not os.path.exists(procfile):
        raise HwsimSkip("kernel doesn't have capability")

    [dev, hapd] = hs20_filters_connect(devs, apdevs)

    with IPAssign(dev.ifname, 'fdaa::1/48', ipv6=True):
        # add an entry that can be updated by unsolicited NA
        subprocess.call(['ip', '-6', 'neigh', 'add', 'fdaa::2', 'lladdr', '02:00:00:00:00:ff', 'nud', 'reachable', 'dev', dev.ifname])
        try:
            ap_addr = hapd.own_addr()
            cl_addr = dev.own_addr()
            pkt = build_na(ap_addr, 'fdaa::2', 'ff02::1', 'fdaa::2', flags=0x20,
                           opt=binascii.unhexlify('0201' + ap_addr.replace(':', '')))
            pkt = binascii.hexlify(pkt).decode()

            if "OK" not in hapd.request('DATA_TEST_FRAME ' + pkt):
                raise Exception("DATA_TEST_FRAME failed")

            if hapd.own_addr() in subprocess.check_output(['ip', 'neigh', 'show']).decode():
                raise Exception("unsolicited NA frame updated erroneously")
        finally:
            subprocess.call(['ip', '-6', 'neigh', 'del', 'fdaa::2', 'dev', dev.ifname])
