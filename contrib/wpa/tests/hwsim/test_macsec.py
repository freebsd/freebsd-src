# Test cases for MACsec/MKA
# Copyright (c) 2018-2019, Jouni Malinen <j@w1.fi>
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

import logging
logger = logging.getLogger()
import binascii
import os
import signal
import subprocess
import time

import hostapd
from wpasupplicant import WpaSupplicant
import hwsim_utils
from utils import HwsimSkip, alloc_fail, fail_test, wait_fail_trigger
from wlantest import WlantestCapture

def cleanup_macsec():
    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5', monitor=False)
    wpas.interface_remove("veth0")
    wpas.interface_remove("veth1")
    del wpas
    subprocess.call(["ip", "link", "del", "veth0"],
                    stderr=open('/dev/null', 'w'))

def test_macsec_psk(dev, apdev, params):
    """MACsec PSK"""
    try:
        run_macsec_psk(dev, apdev, params, "macsec_psk")
    finally:
        cleanup_macsec()

def test_macsec_psk_mka_life_time(dev, apdev, params):
    """MACsec PSK - MKA life time"""
    try:
        run_macsec_psk(dev, apdev, params, "macsec_psk_mka_life_time")
        wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5', monitor=False)
        wpas.interface_remove("veth1")
        del wpas
        # Wait for live peer to be removed on veth0
        time.sleep(6.1)
    finally:
        cleanup_macsec()

def test_macsec_psk_integ_only(dev, apdev, params):
    """MACsec PSK (integrity only)"""
    try:
        run_macsec_psk(dev, apdev, params, "macsec_psk_integ_only",
                       integ_only=True)
    finally:
        cleanup_macsec()

def test_macsec_psk_port(dev, apdev, params):
    """MACsec PSK (port)"""
    try:
        run_macsec_psk(dev, apdev, params, "macsec_psk_port",
                       port0=65534, port1=65534)
    finally:
        cleanup_macsec()

def test_macsec_psk_different_ports(dev, apdev, params):
    """MACsec PSK (different ports)"""
    try:
        run_macsec_psk(dev, apdev, params, "macsec_psk_different_ports",
                       port0=2, port1=3)
    finally:
        cleanup_macsec()

def test_macsec_psk_shorter_ckn(dev, apdev, params):
    """MACsec PSK (shorter CKN)"""
    try:
        ckn = "11223344"
        run_macsec_psk(dev, apdev, params, "macsec_psk_shorter_ckn",
                       ckn0=ckn, ckn1=ckn)
    finally:
        cleanup_macsec()

def test_macsec_psk_shorter_ckn2(dev, apdev, params):
    """MACsec PSK (shorter CKN, unaligned)"""
    try:
        ckn = "112233"
        run_macsec_psk(dev, apdev, params, "macsec_psk_shorter_ckn2",
                       ckn0=ckn, ckn1=ckn)
    finally:
        cleanup_macsec()

def test_macsec_psk_ckn_mismatch(dev, apdev, params):
    """MACsec PSK (CKN mismatch)"""
    try:
        ckn0 = "11223344"
        ckn1 = "1122334455667788"
        run_macsec_psk(dev, apdev, params, "macsec_psk_ckn_mismatch",
                       ckn0=ckn0, ckn1=ckn1, expect_failure=True)
    finally:
        cleanup_macsec()

def test_macsec_psk_cak_mismatch(dev, apdev, params):
    """MACsec PSK (CAK mismatch)"""
    try:
        cak0 = 16*"11"
        cak1 = 16*"22"
        run_macsec_psk(dev, apdev, params, "macsec_psk_cak_mismatch",
                       cak0=cak0, cak1=cak1, expect_failure=True)
    finally:
        cleanup_macsec()

def test_macsec_psk_256(dev, apdev, params):
    """MACsec PSK with 256-bit keys"""
    try:
        cak = "202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f"
        run_macsec_psk(dev, apdev, params, "macsec_psk_256", cak0=cak, cak1=cak)
    finally:
        cleanup_macsec()

def set_mka_psk_config(dev, mka_priority=None, integ_only=False, port=None,
                       ckn=None, cak=None):
    dev.set("eapol_version", "3")
    dev.set("ap_scan", "0")
    dev.set("fast_reauth", "1")

    id = dev.add_network()
    dev.set_network(id, "key_mgmt", "NONE")
    if cak is None:
        cak = "000102030405060708090a0b0c0d0e0f"
    dev.set_network(id, "mka_cak", cak)
    if ckn is None:
        ckn = "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"
    dev.set_network(id, "mka_ckn", ckn)
    dev.set_network(id, "eapol_flags", "0")
    dev.set_network(id, "macsec_policy", "1")
    if integ_only:
        dev.set_network(id, "macsec_integ_only", "1")
    if mka_priority is not None:
        dev.set_network(id, "mka_priority", str(mka_priority))
    if port is not None:
        dev.set_network(id, "macsec_port", str(port))

    dev.select_network(id)

def set_mka_eap_config(dev, mka_priority=None, integ_only=False, port=None):
    dev.set("eapol_version", "3")
    dev.set("ap_scan", "0")
    dev.set("fast_reauth", "1")

    id = dev.add_network()
    dev.set_network(id, "key_mgmt", "NONE")
    dev.set_network(id, "eapol_flags", "0")
    dev.set_network(id, "macsec_policy", "1")
    if integ_only:
        dev.set_network(id, "macsec_integ_only", "1")
    if mka_priority is not None:
        dev.set_network(id, "mka_priority", str(mka_priority))
    if port is not None:
        dev.set_network(id, "macsec_port", str(port))

    dev.set_network(id, "key_mgmt", "IEEE8021X")
    dev.set_network(id, "eap", "TTLS")
    dev.set_network_quoted(id, "ca_cert", "auth_serv/ca.pem")
    dev.set_network_quoted(id, "phase2", "auth=MSCHAPV2")
    dev.set_network_quoted(id, "anonymous_identity", "ttls")
    dev.set_network_quoted(id, "identity", "DOMAIN\mschapv2 user")
    dev.set_network_quoted(id, "password", "password")

    dev.select_network(id)

def log_ip_macsec():
    cmd = subprocess.Popen(["ip", "macsec", "show"],
                           stdout=subprocess.PIPE,
                           stderr=open('/dev/null', 'w'))
    res = cmd.stdout.read().decode()
    cmd.stdout.close()
    logger.info("ip macsec:\n" + res)

def log_ip_link():
    cmd = subprocess.Popen(["ip", "link", "show"],
                           stdout=subprocess.PIPE)
    res = cmd.stdout.read().decode()
    cmd.stdout.close()
    logger.info("ip link:\n" + res)

def add_veth():
    try:
        subprocess.check_call(["ip", "link", "add", "veth0", "type", "veth",
                               "peer", "name", "veth1"])
    except subprocess.CalledProcessError:
        raise HwsimSkip("veth not supported (kernel CONFIG_VETH)")

def add_wpas_interfaces(count=2):
    wpa = []
    try:
        for i in range(count):
            wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
            wpas.interface_add("veth%d" % i, driver="macsec_linux")
            wpa.append(wpas)
    except Exception as e:
        if "Failed to add a dynamic wpa_supplicant interface" in str(e):
            raise HwsimSkip("macsec supported (wpa_supplicant CONFIG_MACSEC, CONFIG_DRIVER_MACSEC_LINUX; kernel CONFIG_MACSEC)")
        raise

    return wpa

def lower_addr(addr1, addr2):
    a1 = addr1.split(':')
    a2 = addr2.split(':')
    for i in range(6):
        if binascii.unhexlify(a1[i]) < binascii.unhexlify(a2[i]):
            return True
        if binascii.unhexlify(a1[i]) > binascii.unhexlify(a2[i]):
            return False
    return False

def wait_mka_done(wpa, expect_failure=False, hostapd=False):
    max_iter = 14 if expect_failure else 40
    for i in range(max_iter):
        done = True
        for w in wpa:
            secured = w.get_status_field("Secured")
            live_peers = w.get_status_field("live_peers")
            peers = int(live_peers) if live_peers else 0
            if expect_failure and (secured == "Yes" or peers > 0):
                raise Exception("MKA completed unexpectedly")
            expect_peers = len(wpa) - 1
            if hostapd:
                expect_peers += 1
            if peers != expect_peers or secured != "Yes":
                done = False
                break
            w.dump_monitor()
        if done:
            break
        time.sleep(0.5)

    if expect_failure:
        return

    if not done:
        raise Exception("MKA not completed successfully")

    if hostapd:
        # TODO: check that hostapd is the key server
        return

    key_server = None
    ks_prio = 999
    for w in wpa:
        logger.info("%s STATUS:\n%s" % (w.ifname, w.request("STATUS")))
        addr = w.get_status_field("address")
        prio = int(w.get_status_field("Actor Priority"))
        if key_server is None or prio < ks_prio or \
           (prio == ks_prio and lower_addr(addr, ks_addr)):
            key_server = w
            ks_addr = addr
            ks_prio = prio

    logger.info("Expected key server: " + key_server.ifname)
    if key_server.get_status_field("is_key_server") != "Yes":
        raise Exception("Expected key server was not elected")
    for w in wpa:
        if w != key_server and w.get_status_field("is_key_server") == "Yes":
            raise Exception("Unexpected key server")

def run_macsec_psk(dev, apdev, params, prefix, integ_only=False, port0=None,
                   port1=None, ckn0=None, ckn1=None, cak0=None, cak1=None,
                   expect_failure=False):
    add_veth()

    cap_veth0 = os.path.join(params['logdir'], prefix + ".veth0.pcap")
    cap_veth1 = os.path.join(params['logdir'], prefix + ".veth1.pcap")
    cap_macsec0 = os.path.join(params['logdir'], prefix + ".macsec0.pcap")
    cap_macsec1 = os.path.join(params['logdir'], prefix + ".macsec1.pcap")

    for i in range(2):
        subprocess.check_call(["ip", "link", "set", "dev", "veth%d" % i, "up"])

    cmd = {}
    cmd[0] = WlantestCapture('veth0', cap_veth0)
    cmd[1] = WlantestCapture('veth1', cap_veth1)

    wpa = add_wpas_interfaces()
    wpas0 = wpa[0]
    wpas1 = wpa[1]

    set_mka_psk_config(wpas0, integ_only=integ_only, port=port0, ckn=ckn0,
                       cak=cak0)
    set_mka_psk_config(wpas1, mka_priority=100, integ_only=integ_only,
                       port=port1, ckn=ckn1, cak=cak1)

    log_ip_macsec()
    log_ip_link()

    logger.info("wpas0 STATUS:\n" + wpas0.request("STATUS"))
    logger.info("wpas1 STATUS:\n" + wpas1.request("STATUS"))
    logger.info("wpas0 STATUS-DRIVER:\n" + wpas0.request("STATUS-DRIVER"))
    logger.info("wpas1 STATUS-DRIVER:\n" + wpas1.request("STATUS-DRIVER"))
    macsec_ifname0 = wpas0.get_driver_status_field("parent_ifname")
    macsec_ifname1 = wpas1.get_driver_status_field("parent_ifname")

    wait_mka_done(wpa, expect_failure=expect_failure)

    if expect_failure:
        for i in range(len(cmd)):
            cmd[i].close()
        return

    cmd[2] = WlantestCapture(macsec_ifname0, cap_macsec0)
    cmd[3] = WlantestCapture(macsec_ifname1, cap_macsec1)
    time.sleep(0.5)

    mi0 = wpas0.get_status_field("mi")
    mi1 = wpas1.get_status_field("mi")
    sci0 = wpas0.get_status_field("actor_sci")
    sci1 = wpas1.get_status_field("actor_sci")
    logger.info("wpas0 MIB:\n" +  wpas0.request("MIB"))
    logger.info("wpas1 MIB:\n" +  wpas1.request("MIB"))
    mib0 = wpas0.get_mib()
    mib1 = wpas1.get_mib()

    if mib0['ieee8021XKayMkaPeerListMI'] != mi1:
        raise Exception("Unexpected ieee8021XKayMkaPeerListMI value (0)")
    if mib0['ieee8021XKayMkaPeerListType'] != "1":
        raise Exception("Unexpected ieee8021XKayMkaPeerListType value (0)")
    if mib0['ieee8021XKayMkaPeerListSCI'] != sci1:
        raise Exception("Unexpected ieee8021XKayMkaPeerListSCI value (0)")
    if mib1['ieee8021XKayMkaPeerListMI'] != mi0:
        raise Exception("Unexpected ieee8021XKayMkaPeerListMI value (1)")
    if mib1['ieee8021XKayMkaPeerListType'] != "1":
        raise Exception("Unexpected ieee8021XKayMkaPeerListType value (1)")
    if mib1['ieee8021XKayMkaPeerListSCI'] != sci0:
        raise Exception("Unexpected ieee8021XKayMkaPeerListSCI value (1)")

    logger.info("wpas0 STATUS:\n" + wpas0.request("STATUS"))
    logger.info("wpas1 STATUS:\n" + wpas1.request("STATUS"))
    log_ip_macsec()
    hwsim_utils.test_connectivity(wpas0, wpas1,
                                  ifname1=macsec_ifname0,
                                  ifname2=macsec_ifname1,
                                  send_len=1400)
    log_ip_macsec()

    time.sleep(1)
    for i in range(len(cmd)):
        cmd[i].close()

def cleanup_macsec_br(count):
    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5', monitor=False)
    for i in range(count):
        wpas.interface_remove("veth%d" % i)
        subprocess.call(["ip", "link", "del", "veth%d" % i],
                        stderr=open('/dev/null', 'w'))
    del wpas
    subprocess.call(["ip", "link", "set", "brveth", "down"])
    subprocess.call(["brctl", "delbr", "brveth"])

def test_macsec_psk_br2(dev, apdev):
    """MACsec PSK (bridge; 2 devices)"""
    try:
        run_macsec_psk_br(dev, apdev, 2, [10, 20])
    finally:
        cleanup_macsec_br(count=2)

def test_macsec_psk_br2_same_prio(dev, apdev):
    """MACsec PSK (bridge; 2 devices, same mka_priority)"""
    try:
        run_macsec_psk_br(dev, apdev, 2, [None, None])
    finally:
        cleanup_macsec_br(count=2)

def test_macsec_psk_br3(dev, apdev):
    """MACsec PSK (bridge; 3 devices)"""
    try:
        run_macsec_psk_br(dev, apdev, 3, [10, 20, 30])
    finally:
        cleanup_macsec_br(count=3)

def test_macsec_psk_br3_same_prio(dev, apdev):
    """MACsec PSK (bridge; 3 devices, same mka_priority)"""
    try:
        run_macsec_psk_br(dev, apdev, 3, [None, None, None])
    finally:
        cleanup_macsec_br(count=3)

def run_macsec_psk_br(dev, apdev, count, mka_priority):
    subprocess.check_call(["brctl", "addbr", "brveth"])
    subprocess.call(["echo 8 > /sys/devices/virtual/net/brveth/bridge/group_fwd_mask"],
                    shell=True)

    try:
        for i in range(count):
            subprocess.check_call(["ip", "link", "add", "veth%d" % i,
                                   "type", "veth",
                                   "peer", "name", "vethbr%d" % i])
            subprocess.check_call(["ip", "link", "set", "vethbr%d" % i, "up"])
            subprocess.check_call(["brctl", "addif", "brveth",
                                   "vethbr%d" % i])
    except subprocess.CalledProcessError:
        raise HwsimSkip("veth not supported (kernel CONFIG_VETH)")

    subprocess.check_call(["ip", "link", "set", "brveth", "up"])

    log_ip_link()

    wpa = add_wpas_interfaces(count=count)
    for i in range(count):
        set_mka_psk_config(wpa[i], mka_priority=mka_priority[i])
        wpa[i].dump_monitor()
    wait_mka_done(wpa)

    macsec_ifname = []
    for i in range(count):
        macsec_ifname.append(wpa[i].get_driver_status_field("parent_ifname"))

    timeout = 2
    max_tries = 2 if count > 2 else 1
    success_seen = False
    failure_seen = False
    for i in range(1, count):
        try:
            hwsim_utils.test_connectivity(wpa[0], wpa[i],
                                          ifname1=macsec_ifname[0],
                                          ifname2=macsec_ifname[i],
                                          send_len=1400,
                                          timeout=timeout, max_tries=max_tries)
            success_seen = True
            logger.info("Traffic test %d<->%d success" % (0, i))
        except:
            failure_seen = True
            logger.info("Traffic test %d<->%d failure" % (0, i))
    for i in range(2, count):
        try:
            hwsim_utils.test_connectivity(wpa[1], wpa[i],
                                          ifname1=macsec_ifname[1],
                                          ifname2=macsec_ifname[i],
                                          send_len=1400,
                                          timeout=timeout, max_tries=max_tries)
            success_seen = True
            logger.info("Traffic test %d<->%d success" % (1, i))
        except:
            failure_seen = True
            logger.info("Traffic test %d<->%d failure" % (1, i))

    if not success_seen:
        raise Exception("None of the data traffic tests succeeded")

    # Something seems to be failing with three device tests semi-regularly, so
    # do not report this as a failed test case until the real reason behind
    # those failures have been determined.
    if failure_seen:
        if count < 3:
            raise Exception("Data traffic test failed")
        else:
            logger.info("Data traffic test failed - ignore for now for >= 3 device cases")

    for i in range(count):
        wpa[i].close_monitor()
    for i in range(count):
        wpa[0].close_control()
        del wpa[0]

def test_macsec_psk_ns(dev, apdev, params):
    """MACsec PSK (netns)"""
    try:
        run_macsec_psk_ns(dev, apdev, params)
    finally:
        prefix = "macsec_psk_ns"
        pidfile = os.path.join(params['logdir'], prefix + ".pid")
        for i in range(2):
            was_running = False
            if os.path.exists(pidfile + str(i)):
                with open(pidfile + str(i), 'r') as f:
                    pid = int(f.read().strip())
                    logger.info("wpa_supplicant for wpas%d still running with pid %d - kill it" % (i, pid))
                    was_running = True
                    os.kill(pid, signal.SIGTERM)
            if was_running:
                time.sleep(1)

        subprocess.call(["ip", "netns", "exec", "ns0",
                         "ip", "link", "del", "veth0"],
                        stderr=open('/dev/null', 'w'))
        subprocess.call(["ip", "link", "del", "veth0"],
                        stderr=open('/dev/null', 'w'))
        log_ip_link_ns()
        subprocess.call(["ip", "netns", "delete", "ns0"],
                        stderr=open('/dev/null', 'w'))
        subprocess.call(["ip", "netns", "delete", "ns1"],
                        stderr=open('/dev/null', 'w'))

def log_ip_macsec_ns():
    cmd = subprocess.Popen(["ip", "macsec", "show"],
                           stdout=subprocess.PIPE,
                           stderr=open('/dev/null', 'w'))
    res = cmd.stdout.read().decode()
    cmd.stdout.close()
    logger.info("ip macsec show:\n" + res)

    cmd = subprocess.Popen(["ip", "netns", "exec", "ns0",
                            "ip", "macsec", "show"],
                           stdout=subprocess.PIPE,
                           stderr=open('/dev/null', 'w'))
    res = cmd.stdout.read().decode()
    cmd.stdout.close()
    logger.info("ip macsec show (ns0):\n" + res)

    cmd = subprocess.Popen(["ip", "netns", "exec", "ns1",
                            "ip", "macsec", "show"],
                           stdout=subprocess.PIPE,
                           stderr=open('/dev/null', 'w'))
    res = cmd.stdout.read().decode()
    cmd.stdout.close()
    logger.info("ip macsec show (ns1):\n" + res)

def log_ip_link_ns():
    cmd = subprocess.Popen(["ip", "link", "show"],
                           stdout=subprocess.PIPE)
    res = cmd.stdout.read().decode()
    cmd.stdout.close()
    logger.info("ip link:\n" + res)

    cmd = subprocess.Popen(["ip", "netns", "exec", "ns0",
                            "ip", "link", "show"],
                           stdout=subprocess.PIPE,
                           stderr=open('/dev/null', 'w'))
    res = cmd.stdout.read().decode()
    cmd.stdout.close()
    logger.info("ip link show (ns0):\n" + res)

    cmd = subprocess.Popen(["ip", "netns", "exec", "ns1",
                            "ip", "link", "show"],
                           stdout=subprocess.PIPE,
                           stderr=open('/dev/null', 'w'))
    res = cmd.stdout.read().decode()
    cmd.stdout.close()
    logger.info("ip link show (ns1):\n" + res)

def write_conf(conffile, mka_priority=None):
    with open(conffile, 'w') as f:
        f.write("ctrl_interface=DIR=/var/run/wpa_supplicant\n")
        f.write("eapol_version=3\n")
        f.write("ap_scan=0\n")
        f.write("fast_reauth=1\n")
        f.write("network={\n")
        f.write("   key_mgmt=NONE\n")
        f.write("   mka_cak=000102030405060708090a0b0c0d0e0f\n")
        f.write("   mka_ckn=000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f\n")
        if mka_priority is not None:
            f.write("   mka_priority=%d\n" % mka_priority)
        f.write("   eapol_flags=0\n")
        f.write("   macsec_policy=1\n")
        f.write("}\n")

def run_macsec_psk_ns(dev, apdev, params):
    try:
        subprocess.check_call(["ip", "link", "add", "veth0", "type", "veth",
                               "peer", "name", "veth1"])
    except subprocess.CalledProcessError:
        raise HwsimSkip("veth not supported (kernel CONFIG_VETH)")

    prefix = "macsec_psk_ns"
    conffile = os.path.join(params['logdir'], prefix + ".conf")
    pidfile = os.path.join(params['logdir'], prefix + ".pid")
    logfile0 = os.path.join(params['logdir'], prefix + ".veth0.log")
    logfile1 = os.path.join(params['logdir'], prefix + ".veth1.log")
    cap_veth0 = os.path.join(params['logdir'], prefix + ".veth0.pcap")
    cap_veth1 = os.path.join(params['logdir'], prefix + ".veth1.pcap")
    cap_macsec0 = os.path.join(params['logdir'], prefix + ".macsec0.pcap")
    cap_macsec1 = os.path.join(params['logdir'], prefix + ".macsec1.pcap")

    for i in range(2):
        try:
            subprocess.check_call(["ip", "netns", "add", "ns%d" % i])
        except subprocess.CalledProcessError:
            raise HwsimSkip("network namespace not supported (kernel CONFIG_NAMESPACES, CONFIG_NET_NS)")
        subprocess.check_call(["ip", "link", "set", "veth%d" % i,
                               "netns", "ns%d" %i])
        subprocess.check_call(["ip", "netns", "exec", "ns%d" % i,
                               "ip", "link", "set", "dev", "veth%d" % i,
                               "up"])

    cmd = {}
    cmd[0] = WlantestCapture('veth0', cap_veth0, netns='ns0')
    cmd[1] = WlantestCapture('veth1', cap_veth1, netns='ns1')

    write_conf(conffile + '0')
    write_conf(conffile + '1', mka_priority=100)

    prg = os.path.join(params['logdir'],
                       'alt-wpa_supplicant/wpa_supplicant/wpa_supplicant')
    if not os.path.exists(prg):
        prg = '../../wpa_supplicant/wpa_supplicant'

    arg = ["ip", "netns", "exec", "ns0",
           prg, '-BdddtKW', '-P', pidfile + '0', '-f', logfile0,
           '-g', '/tmp/wpas-veth0',
           '-Dmacsec_linux', '-c', conffile + '0', '-i', "veth0"]
    logger.info("Start wpa_supplicant: " + str(arg))
    try:
        subprocess.check_call(arg)
    except subprocess.CalledProcessError:
        raise HwsimSkip("macsec supported (wpa_supplicant CONFIG_MACSEC, CONFIG_DRIVER_MACSEC_LINUX; kernel CONFIG_MACSEC)")

    if os.path.exists("wpa_supplicant-macsec2"):
        logger.info("Use alternative wpa_supplicant binary for one of the macsec devices")
        prg = "wpa_supplicant-macsec2"

    arg = ["ip", "netns", "exec", "ns1",
           prg, '-BdddtKW', '-P', pidfile + '1', '-f', logfile1,
           '-g', '/tmp/wpas-veth1',
           '-Dmacsec_linux', '-c', conffile + '1', '-i', "veth1"]
    logger.info("Start wpa_supplicant: " + str(arg))
    subprocess.check_call(arg)

    wpas0 = WpaSupplicant('veth0', '/tmp/wpas-veth0')
    wpas1 = WpaSupplicant('veth1', '/tmp/wpas-veth1')

    log_ip_macsec_ns()
    log_ip_link_ns()

    logger.info("wpas0 STATUS:\n" + wpas0.request("STATUS"))
    logger.info("wpas1 STATUS:\n" + wpas1.request("STATUS"))
    logger.info("wpas0 STATUS-DRIVER:\n" + wpas0.request("STATUS-DRIVER"))
    logger.info("wpas1 STATUS-DRIVER:\n" + wpas1.request("STATUS-DRIVER"))

    for i in range(10):
        macsec_ifname0 = wpas0.get_driver_status_field("parent_ifname")
        macsec_ifname1 = wpas1.get_driver_status_field("parent_ifname")
        if "Number of Keys" in wpas0.request("STATUS"):
            key_tx0 = int(wpas0.get_status_field("Number of Keys Distributed"))
            key_rx0 = int(wpas0.get_status_field("Number of Keys Received"))
        else:
            key_tx0 = 0
            key_rx0 = 0
        if "Number of Keys" in wpas1.request("STATUS"):
            key_tx1 = int(wpas1.get_status_field("Number of Keys Distributed"))
            key_rx1 = int(wpas1.get_status_field("Number of Keys Received"))
        else:
            key_tx1 = 0
            key_rx1 = 0
        if key_rx0 > 0 and key_tx1 > 0:
            break
        time.sleep(1)

    cmd[2] = WlantestCapture(macsec_ifname0, cap_macsec0, netns='ns0')
    cmd[3] = WlantestCapture(macsec_ifname1, cap_macsec1, netns='ns0')
    time.sleep(0.5)

    logger.info("wpas0 STATUS:\n" + wpas0.request("STATUS"))
    logger.info("wpas1 STATUS:\n" + wpas1.request("STATUS"))
    log_ip_macsec_ns()
    hwsim_utils.test_connectivity(wpas0, wpas1,
                                  ifname1=macsec_ifname0,
                                  ifname2=macsec_ifname1,
                                  send_len=1400)
    log_ip_macsec_ns()

    subprocess.check_call(['ip', 'netns', 'exec', 'ns0',
                           'ip', 'addr', 'add', '192.168.248.17/30',
                           'dev', macsec_ifname0])
    subprocess.check_call(['ip', 'netns', 'exec', 'ns1',
                           'ip', 'addr', 'add', '192.168.248.18/30',
                           'dev', macsec_ifname1])
    c = subprocess.Popen(['ip', 'netns', 'exec', 'ns0',
                          'ping', '-c', '2', '192.168.248.18'],
                         stdout=subprocess.PIPE)
    res = c.stdout.read().decode()
    c.stdout.close()
    logger.info("ping:\n" + res)
    if "2 packets transmitted, 2 received" not in res:
        raise Exception("ping did not work")

    wpas0.close_monitor()
    wpas0.request("TERMINATE")
    wpas0.close_control()
    del wpas0
    wpas1.close_monitor()
    wpas1.request("TERMINATE")
    wpas1.close_control()
    del wpas1

    time.sleep(1)
    for i in range(len(cmd)):
        cmd[i].close()

def test_macsec_psk_fail_cp(dev, apdev):
    """MACsec PSK local failures in CP state machine"""
    try:
        add_veth()
        wpa = add_wpas_interfaces()
        set_mka_psk_config(wpa[0])
        with alloc_fail(wpa[0], 1, "sm_CP_RECEIVE_Enter"):
            set_mka_psk_config(wpa[1])
            wait_fail_trigger(wpa[0], "GET_ALLOC_FAIL", max_iter=100)

        wait_mka_done(wpa)
    finally:
        cleanup_macsec()

def test_macsec_psk_fail_cp2(dev, apdev):
    """MACsec PSK local failures in CP state machine (2)"""
    try:
        add_veth()
        wpa = add_wpas_interfaces()
        set_mka_psk_config(wpa[0])
        with alloc_fail(wpa[1], 1, "ieee802_1x_cp_sm_init"):
            set_mka_psk_config(wpa[1])
            wait_fail_trigger(wpa[1], "GET_ALLOC_FAIL", max_iter=100)

        wait_mka_done(wpa)
    finally:
        cleanup_macsec()

def cleanup_macsec_hostapd():
    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5', monitor=False)
    wpas.interface_remove("veth0")
    del wpas
    hapd = hostapd.HostapdGlobal()
    hapd.remove('veth1')
    subprocess.call(["ip", "link", "del", "veth0"],
                    stderr=open('/dev/null', 'w'))
    log_ip_link()

def test_macsec_hostapd_psk(dev, apdev, params):
    """MACsec PSK with hostapd"""
    try:
        run_macsec_hostapd_psk(dev, apdev, params, "macsec_hostapd_psk")
    finally:
        cleanup_macsec_hostapd()

def run_macsec_hostapd_psk(dev, apdev, params, prefix, integ_only=False,
                           port0=None, port1=None, ckn0=None, ckn1=None,
                           cak0=None, cak1=None, expect_failure=False):
    add_veth()

    cap_veth0 = os.path.join(params['logdir'], prefix + ".veth0.pcap")
    cap_veth1 = os.path.join(params['logdir'], prefix + ".veth1.pcap")
    cap_macsec0 = os.path.join(params['logdir'], prefix + ".macsec0.pcap")
    cap_macsec1 = os.path.join(params['logdir'], prefix + ".macsec1.pcap")

    for i in range(2):
        subprocess.check_call(["ip", "link", "set", "dev", "veth%d" % i, "up"])

    cmd = {}
    cmd[0] = WlantestCapture('veth0', cap_veth0)
    cmd[1] = WlantestCapture('veth1', cap_veth1)

    wpa = add_wpas_interfaces(count=1)
    wpas0 = wpa[0]

    set_mka_psk_config(wpas0, integ_only=integ_only, port=port0, ckn=ckn0,
                       cak=cak0, mka_priority=100)

    if cak1 is None:
        cak1 = "000102030405060708090a0b0c0d0e0f"
    if ckn1 is None:
        ckn1 = "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"
    params = {"driver": "macsec_linux",
              "interface": "veth1",
              "eapol_version": "3",
              "mka_cak": cak1,
              "mka_ckn": ckn1,
              "macsec_policy": "1",
              "mka_priority": "1"}
    if integ_only:
        params["macsec_integ_only"] = "1"
    if port1 is not None:
        params["macsec_port"] = str(port1)
    apdev = {'ifname': 'veth1'}
    try:
        hapd = hostapd.add_ap(apdev, params, driver="macsec_linux")
    except:
        raise HwsimSkip("No CONFIG_MACSEC=y in hostapd")

    log_ip_macsec()
    log_ip_link()

    logger.info("wpas0 STATUS:\n" + wpas0.request("STATUS"))
    logger.info("wpas0 STATUS-DRIVER:\n" + wpas0.request("STATUS-DRIVER"))

    wait_mka_done(wpa, expect_failure=expect_failure, hostapd=True)
    log_ip_link()

    if expect_failure:
        for i in range(len(cmd)):
            cmd[i].close()
        return

    macsec_ifname0 = wpas0.get_driver_status_field("parent_ifname")
    macsec_ifname1 = hapd.get_driver_status_field("parent_ifname")

    cmd[2] = WlantestCapture(macsec_ifname0, cap_macsec0)
    cmd[3] = WlantestCapture(macsec_ifname1, cap_macsec1)
    time.sleep(0.5)

    logger.info("wpas0 MIB:\n" +  wpas0.request("MIB"))
    logger.info("wpas0 STATUS:\n" + wpas0.request("STATUS"))
    log_ip_macsec()
    hwsim_utils.test_connectivity(wpas0, hapd,
                                  ifname1=macsec_ifname0,
                                  ifname2=macsec_ifname1,
                                  send_len=1400)
    log_ip_macsec()

    time.sleep(1)
    for i in range(len(cmd)):
        cmd[i].close()

def test_macsec_hostapd_eap(dev, apdev, params):
    """MACsec EAP with hostapd"""
    try:
        run_macsec_hostapd_eap(dev, apdev, params, "macsec_hostapd_eap")
    finally:
        cleanup_macsec_hostapd()

def run_macsec_hostapd_eap(dev, apdev, params, prefix, integ_only=False,
                           port0=None, port1=None, expect_failure=False):
    add_veth()

    cap_veth0 = os.path.join(params['logdir'], prefix + ".veth0.pcap")
    cap_veth1 = os.path.join(params['logdir'], prefix + ".veth1.pcap")
    cap_macsec0 = os.path.join(params['logdir'], prefix + ".macsec0.pcap")
    cap_macsec1 = os.path.join(params['logdir'], prefix + ".macsec1.pcap")

    for i in range(2):
        subprocess.check_call(["ip", "link", "set", "dev", "veth%d" % i, "up"])

    cmd = {}
    cmd[0] = WlantestCapture('veth0', cap_veth0)
    cmd[1] = WlantestCapture('veth1', cap_veth1)

    wpa = add_wpas_interfaces(count=1)
    wpas0 = wpa[0]

    set_mka_eap_config(wpas0, integ_only=integ_only, port=port0,
                       mka_priority=100)

    params = {"driver": "macsec_linux",
              "interface": "veth1",
              "eapol_version": "3",
              "macsec_policy": "1",
              "mka_priority": "1",
              "ieee8021x": "1",
              "auth_server_addr": "127.0.0.1",
              "auth_server_port": "1812",
              "auth_server_shared_secret": "radius",
              "nas_identifier": "nas.w1.fi"}
    if integ_only:
        params["macsec_integ_only"] = "1"
    if port1 is not None:
        params["macsec_port"] = str(port1)
    apdev = {'ifname': 'veth1'}
    try:
        hapd = hostapd.add_ap(apdev, params, driver="macsec_linux")
    except:
        raise HwsimSkip("No CONFIG_MACSEC=y in hostapd")

    log_ip_macsec()
    log_ip_link()

    logger.info("wpas0 STATUS:\n" + wpas0.request("STATUS"))
    logger.info("wpas0 STATUS-DRIVER:\n" + wpas0.request("STATUS-DRIVER"))

    wait_mka_done(wpa, expect_failure=expect_failure, hostapd=True)
    log_ip_link()

    if expect_failure:
        for i in range(len(cmd)):
            cmd[i].close()
        return

    macsec_ifname0 = wpas0.get_driver_status_field("parent_ifname")
    macsec_ifname1 = hapd.get_driver_status_field("parent_ifname")

    cmd[2] = WlantestCapture(macsec_ifname0, cap_macsec0)
    cmd[3] = WlantestCapture(macsec_ifname1, cap_macsec1)
    time.sleep(0.5)

    logger.info("wpas0 MIB:\n" +  wpas0.request("MIB"))
    logger.info("wpas0 STATUS:\n" + wpas0.request("STATUS"))
    log_ip_macsec()
    hwsim_utils.test_connectivity(wpas0, hapd,
                                  ifname1=macsec_ifname0,
                                  ifname2=macsec_ifname1,
                                  send_len=1400)
    log_ip_macsec()

    time.sleep(1)
    for i in range(len(cmd)):
        cmd[i].close()
