# cfg80211 P2P Device
# Copyright (c) 2013-2015, Jouni Malinen <j@w1.fi>
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

import logging
logger = logging.getLogger()
import time

from wpasupplicant import WpaSupplicant
from p2p_utils import *
from test_nfc_p2p import set_ip_addr_info, check_ip_addr, grpform_events
from hwsim import HWSimRadio
from utils import HwsimSkip
import hostapd
import hwsim_utils

def test_p2p_device_grpform(dev, apdev):
    """P2P group formation with driver using cfg80211 P2P Device"""
    with HWSimRadio(use_p2p_device=True) as (radio, iface):
        wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
        wpas.interface_add(iface)
        [i_res, r_res] = go_neg_pin_authorized(i_dev=dev[0], i_intent=15,
                                               r_dev=wpas, r_intent=0)
        check_grpform_results(i_res, r_res)
        wpas.dump_monitor()
        remove_group(dev[0], wpas)
        wpas.dump_monitor()
        if not r_res['ifname'].startswith('p2p-' + iface):
            raise Exception("Unexpected group ifname: " + r_res['ifname'])

        res = wpas.global_request("IFNAME=p2p-dev-" + iface + " STATUS-DRIVER")
        lines = res.splitlines()
        found = False
        for l in lines:
            try:
                [name, value] = l.split('=', 1)
                if name == "wdev_id":
                    found = True
                    break
            except ValueError:
                pass
        if not found:
            raise Exception("wdev_id not found")

def test_p2p_device_grpform2(dev, apdev):
    """P2P group formation with driver using cfg80211 P2P Device (reverse)"""
    with HWSimRadio(use_p2p_device=True) as (radio, iface):
        wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
        wpas.interface_add(iface)
        [i_res, r_res] = go_neg_pin_authorized(i_dev=wpas, i_intent=15,
                                               r_dev=dev[0], r_intent=0)
        check_grpform_results(i_res, r_res)
        wpas.dump_monitor()
        remove_group(wpas, dev[0])
        wpas.dump_monitor()
        if not i_res['ifname'].startswith('p2p-' + iface):
            raise Exception("Unexpected group ifname: " + i_res['ifname'])

def test_p2p_device_grpform_no_group_iface(dev, apdev):
    """P2P group formation with driver using cfg80211 P2P Device but no separate group interface"""
    with HWSimRadio(use_p2p_device=True) as (radio, iface):
        wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
        wpas.interface_add(iface)
        wpas.global_request("SET p2p_no_group_iface 1")
        [i_res, r_res] = go_neg_pin_authorized(i_dev=dev[0], i_intent=15,
                                               r_dev=wpas, r_intent=0)
        check_grpform_results(i_res, r_res)
        wpas.dump_monitor()
        remove_group(dev[0], wpas)
        wpas.dump_monitor()
        if r_res['ifname'] != iface:
            raise Exception("Unexpected group ifname: " + r_res['ifname'])

def test_p2p_device_grpform_no_group_iface2(dev, apdev):
    """P2P group formation with driver using cfg80211 P2P Device but no separate group interface (reverse)"""
    with HWSimRadio(use_p2p_device=True) as (radio, iface):
        wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
        wpas.interface_add(iface)
        wpas.global_request("SET p2p_no_group_iface 1")
        [i_res, r_res] = go_neg_pin_authorized(i_dev=wpas, i_intent=15,
                                               r_dev=dev[0], r_intent=0)
        check_grpform_results(i_res, r_res)
        wpas.dump_monitor()
        remove_group(dev[0], wpas)
        wpas.dump_monitor()
        if i_res['ifname'] != iface:
            raise Exception("Unexpected group ifname: " + i_res['ifname'])

def test_p2p_device_group_remove(dev, apdev):
    """P2P group removal via the P2P ctrl interface with driver using cfg80211 P2P Device"""
    with HWSimRadio(use_p2p_device=True) as (radio, iface):
        wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
        wpas.interface_add(iface)
        [i_res, r_res] = go_neg_pin_authorized(i_dev=dev[0], i_intent=15,
                                               r_dev=wpas, r_intent=0)
        check_grpform_results(i_res, r_res)
        # Issue the remove request on the interface which will be removed
        p2p_iface_wpas = WpaSupplicant(ifname=r_res['ifname'])
        res = p2p_iface_wpas.request("P2P_GROUP_REMOVE *")
        if "OK" not in res:
            raise Exception("Failed to remove P2P group")
        ev = wpas.wait_global_event(["P2P-GROUP-REMOVED"], timeout=10)
        if ev is None:
            raise Exception("Group removal event not received")
        if not wpas.global_ping():
            raise Exception("Could not ping global ctrl_iface after group removal")

def test_p2p_device_concurrent_scan(dev, apdev):
    """Concurrent P2P and station mode scans with driver using cfg80211 P2P Device"""
    with HWSimRadio(use_p2p_device=True) as (radio, iface):
        wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
        wpas.interface_add(iface)
        wpas.p2p_find()
        time.sleep(0.1)
        wpas.request("SCAN")
        ev = wpas.wait_event(["CTRL-EVENT-SCAN-STARTED"], timeout=15)
        if ev is None:
            raise Exception("Station mode scan did not start")

def test_p2p_device_nfc_invite(dev, apdev):
    """P2P NFC invitation with driver using cfg80211 P2P Device"""
    run_p2p_device_nfc_invite(dev, apdev, 0)

def test_p2p_device_nfc_invite_no_group_iface(dev, apdev):
    """P2P NFC invitation with driver using cfg80211 P2P Device (no separate group interface)"""
    run_p2p_device_nfc_invite(dev, apdev, 1)

def run_p2p_device_nfc_invite(dev, apdev, no_group_iface):
    with HWSimRadio(use_p2p_device=True) as (radio, iface):
        wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
        wpas.interface_add(iface)
        wpas.global_request("SET p2p_no_group_iface %d" % no_group_iface)

        set_ip_addr_info(dev[0])
        logger.info("Start autonomous GO")
        dev[0].p2p_start_go()

        logger.info("Write NFC Tag on the P2P Client")
        res = wpas.global_request("P2P_LISTEN")
        if "FAIL" in res:
            raise Exception("Failed to start Listen mode")
        wpas.dump_monitor()
        pw = wpas.global_request("WPS_NFC_TOKEN NDEF").rstrip()
        if "FAIL" in pw:
            raise Exception("Failed to generate password token")
        res = wpas.global_request("P2P_SET nfc_tag 1").rstrip()
        if "FAIL" in res:
            raise Exception("Failed to enable NFC Tag for P2P static handover")
        sel = wpas.global_request("NFC_GET_HANDOVER_SEL NDEF P2P-CR-TAG").rstrip()
        if "FAIL" in sel:
            raise Exception("Failed to generate NFC connection handover select")
        wpas.dump_monitor()

        logger.info("Read NFC Tag on the GO to trigger invitation")
        res = dev[0].global_request("WPS_NFC_TAG_READ " + sel)
        if "FAIL" in res:
            raise Exception("Failed to provide NFC tag contents to wpa_supplicant")

        ev = wpas.wait_global_event(grpform_events, timeout=20)
        if ev is None:
            raise Exception("Joining the group timed out")
        res = wpas.group_form_result(ev)
        wpas.dump_monitor()
        hwsim_utils.test_connectivity_p2p(dev[0], wpas)
        check_ip_addr(res)
        wpas.dump_monitor()

def test_p2p_device_misuses(dev, apdev):
    """cfg80211 P2P Device misuses"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": "open"})
    with HWSimRadio(use_p2p_device=True) as (radio, iface):
        wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
        wpas.interface_add(iface)

        # Add a normal network profile to the P2P Device management only
        # interface to verify that it does not get used.
        id = int(wpas.global_request('IFNAME=p2p-dev-%s ADD_NETWORK' % iface).strip())
        wpas.global_request('IFNAME=p2p-dev-%s SET_NETWORK %d ssid "open"' % (iface, id))
        wpas.global_request('IFNAME=p2p-dev-%s SET_NETWORK %d key_mgmt NONE' % (iface, id))
        wpas.global_request('IFNAME=p2p-dev-%s ENABLE_NETWORK %d' % (iface, id))

        # Scan requests get ignored on p2p-dev
        wpas.global_request('IFNAME=p2p-dev-%s SCAN' % iface)

        dev[0].p2p_start_go(freq=2412)
        addr = dev[0].p2p_interface_addr()
        wpas.scan_for_bss(addr, freq=2412)
        wpas.connect("open", key_mgmt="NONE", scan_freq="2412")
        hwsim_utils.test_connectivity(wpas, hapd)

        pin = wpas.wps_read_pin()
        dev[0].p2p_go_authorize_client(pin)
        res = wpas.p2p_connect_group(dev[0].p2p_dev_addr(), pin, timeout=60,
                                     social=True, freq=2412)
        hwsim_utils.test_connectivity_p2p(dev[0], wpas)

        # Optimize scan-after-disconnect
        wpas.group_request("SET_NETWORK 0 scan_freq 2412")

        dev[0].group_request("DISASSOCIATE " + wpas.p2p_interface_addr())
        ev = wpas.wait_group_event(["CTRL-EVENT-DISCONNECT"])
        if ev is None:
            raise Exception("Did not see disconnect event on P2P group interface")
        dev[0].remove_group()

        ev = wpas.wait_group_event(["CTRL-EVENT-SCAN-STARTED"], timeout=5)
        if ev is None:
            raise Exception("Scan not started")
        ev = wpas.wait_group_event(["CTRL-EVENT-SCAN-RESULTS"], timeout=15)
        if ev is None:
            raise Exception("Scan not completed")
        time.sleep(1)
        hwsim_utils.test_connectivity(wpas, hapd)

        ev = hapd.wait_event(["AP-STA-DISCONNECTED"], timeout=0.1)
        if ev is not None:
            raise Exception("Unexpected disconnection event received from hostapd")
        ev = wpas.wait_event(["CTRL-EVENT-DISCONNECTED"], timeout=0.1)
        if ev is not None:
            raise Exception("Unexpected disconnection event received from wpa_supplicant")

        wpas.request("DISCONNECT")
        wpas.wait_disconnected()

def test_p2p_device_incorrect_command_interface(dev, apdev):
    """cfg80211 P2P Device and P2P_* command on incorrect interface"""
    with HWSimRadio(use_p2p_device=True) as (radio, iface):
        wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
        wpas.interface_add(iface)

        dev[0].p2p_listen()
        wpas.request('P2P_FIND type=social')
        ev = wpas.wait_global_event(["P2P-DEVICE-FOUND"], timeout=10)
        if ev is None:
            raise Exception("Peer not found")
        ev = wpas.wait_event(["P2P-DEVICE-FOUND"], timeout=0.1)
        if ev is not None:
            raise Exception("Unexpected P2P-DEVICE-FOUND event on station interface")
        wpas.dump_monitor()

        pin = wpas.wps_read_pin()
        dev[0].p2p_go_neg_auth(wpas.p2p_dev_addr(), pin, "enter", go_intent=14,
                               freq=2412)
        wpas.request('P2P_STOP_FIND')
        wpas.dump_monitor()
        if "OK" not in wpas.request('P2P_CONNECT ' + dev[0].p2p_dev_addr() + ' ' + pin + ' display go_intent=1'):
            raise Exception("P2P_CONNECT failed")

        ev = wpas.wait_global_event(["P2P-GROUP-STARTED"], timeout=15)
        if ev is None:
            raise Exception("Group formation timed out")
        wpas.group_form_result(ev)
        wpas.dump_monitor()

        ev = dev[0].wait_global_event(["P2P-GROUP-STARTED"], timeout=15)
        if ev is None:
            raise Exception("Group formation timed out(2)")
        dev[0].group_form_result(ev)

        dev[0].remove_group()
        wpas.wait_go_ending_session()
        wpas.dump_monitor()

def test_p2p_device_incorrect_command_interface2(dev, apdev):
    """cfg80211 P2P Device and P2P_GROUP_ADD command on incorrect interface"""
    with HWSimRadio(use_p2p_device=True) as (radio, iface):
        wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
        wpas.interface_add(iface)

        if "OK" not in wpas.request('P2P_GROUP_ADD'):
            raise Exception("P2P_GROUP_ADD failed")
        ev = wpas.wait_global_event(["P2P-GROUP-STARTED"], timeout=15)
        if ev is None:
            raise Exception("Group formation timed out")
        res = wpas.group_form_result(ev)
        wpas.dump_monitor()
        logger.info("Group results: " + str(res))
        wpas.remove_group()
        if not res['ifname'].startswith('p2p-' + iface + '-'):
            raise Exception("Unexpected group ifname: " + res['ifname'])
        wpas.dump_monitor()

def test_p2p_device_grpform_timeout_client(dev, apdev):
    """P2P group formation timeout on client with cfg80211 P2P Device"""
    with HWSimRadio(use_p2p_device=True) as (radio, iface):
        wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
        wpas.interface_add(iface)
        addr0 = dev[0].p2p_dev_addr()
        addr5 = wpas.p2p_dev_addr()
        wpas.p2p_listen()
        dev[0].discover_peer(addr5)
        dev[0].p2p_listen()
        wpas.discover_peer(addr0)
        wpas.p2p_ext_listen(100, 150)
        dev[0].global_request("P2P_CONNECT " + addr5 + " 12345670 enter go_intent=15 auth")
        wpas.global_request("P2P_CONNECT " + addr0 + " 12345670 display go_intent=0")
        ev = dev[0].wait_global_event(["P2P-GO-NEG-SUCCESS"], timeout=5)
        if ev is None:
            raise Exception("GO Negotiation did not succeed")
        ev = dev[0].wait_global_event(["WPS-SUCCESS"], timeout=10)
        if ev is None:
            raise Exception("WPS did not succeed (GO)")
        if "OK" not in dev[0].global_request("P2P_CANCEL"):
            wpas.global_request("P2P_CANCEL")
            del wpas
            raise HwsimSkip("Did not manage to cancel group formation")
        dev[0].dump_monitor()
        ev = wpas.wait_global_event(["WPS-SUCCESS"], timeout=10)
        if ev is None:
            raise Exception("WPS did not succeed (Client)")
        dev[0].dump_monitor()
        ev = wpas.wait_global_event(["P2P-GROUP-FORMATION-FAILURE"], timeout=20)
        if ev is None:
            raise Exception("Group formation timeout not seen on client")
        ev = wpas.wait_global_event(["P2P-GROUP-REMOVED"], timeout=5)
        if ev is None:
            raise Exception("Group removal not seen on client")
        wpas.p2p_cancel_ext_listen()
        time.sleep(0.1)
        ifaces = wpas.global_request("INTERFACES")
        logger.info("Remaining interfaces: " + ifaces)
        del wpas
        if "p2p-" + iface + "-" in ifaces:
            raise Exception("Group interface still present after failure")

def test_p2p_device_grpform_timeout_go(dev, apdev):
    """P2P group formation timeout on GO with cfg80211 P2P Device"""
    with HWSimRadio(use_p2p_device=True) as (radio, iface):
        wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
        wpas.interface_add(iface)
        addr0 = dev[0].p2p_dev_addr()
        addr5 = wpas.p2p_dev_addr()
        wpas.p2p_listen()
        dev[0].discover_peer(addr5)
        dev[0].p2p_listen()
        wpas.discover_peer(addr0)
        wpas.p2p_ext_listen(100, 150)
        dev[0].global_request("P2P_CONNECT " + addr5 + " 12345670 enter go_intent=0 auth")
        wpas.global_request("P2P_CONNECT " + addr0 + " 12345670 display go_intent=15")
        ev = dev[0].wait_global_event(["P2P-GO-NEG-SUCCESS"], timeout=5)
        if ev is None:
            raise Exception("GO Negotiation did not succeed")
        ev = dev[0].wait_global_event(["WPS-SUCCESS"], timeout=10)
        if ev is None:
            raise Exception("WPS did not succeed (Client)")
        if "OK" not in dev[0].global_request("P2P_CANCEL"):
            if "OK" not in dev[0].global_request("P2P_GROUP_REMOVE *"):
                wpas.global_request("P2P_CANCEL")
                del wpas
                raise HwsimSkip("Did not manage to cancel group formation")
        dev[0].dump_monitor()
        ev = wpas.wait_global_event(["WPS-SUCCESS"], timeout=10)
        if ev is None:
            raise Exception("WPS did not succeed (GO)")
        dev[0].dump_monitor()
        ev = wpas.wait_global_event(["P2P-GROUP-FORMATION-FAILURE"], timeout=20)
        if ev is None:
            raise Exception("Group formation timeout not seen on GO")
        ev = wpas.wait_global_event(["P2P-GROUP-REMOVED"], timeout=5)
        if ev is None:
            raise Exception("Group removal not seen on GO")
        wpas.p2p_cancel_ext_listen()
        time.sleep(0.1)
        ifaces = wpas.global_request("INTERFACES")
        logger.info("Remaining interfaces: " + ifaces)
        del wpas
        if "p2p-" + iface + "-" in ifaces:
            raise Exception("Group interface still present after failure")

def test_p2p_device_autogo(dev, apdev):
    """P2P autogo using cfg80211 P2P Device"""
    with HWSimRadio(use_p2p_device=True) as (radio, iface):
        wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
        wpas.interface_add(iface)

        res = wpas.p2p_start_go()
        if not res['ifname'].startswith('p2p-' + iface):
            raise Exception("Unexpected group ifname: " + res['ifname'])
        bssid = wpas.get_group_status_field('bssid')

        dev[0].scan_for_bss(bssid, res['freq'])
        connect_cli(wpas, dev[0], freq=res['freq'])
        terminate_group(wpas, dev[0])

def test_p2p_device_autogo_no_group_iface(dev, apdev):
    """P2P autogo using cfg80211 P2P Device (no separate group interface)"""
    with HWSimRadio(use_p2p_device=True) as (radio, iface):
        wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
        wpas.interface_add(iface)
        wpas.global_request("SET p2p_no_group_iface 1")

        res = wpas.p2p_start_go()
        if res['ifname'] != iface:
            raise Exception("Unexpected group ifname: " + res['ifname'])
        bssid = wpas.get_group_status_field('bssid')

        dev[0].scan_for_bss(bssid, res['freq'])
        connect_cli(wpas, dev[0], freq=res['freq'])
        terminate_group(wpas, dev[0])

def test_p2p_device_join(dev, apdev):
    """P2P join-group using cfg80211 P2P Device"""
    with HWSimRadio(use_p2p_device=True) as (radio, iface):
        wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
        wpas.interface_add(iface)

        res = dev[0].p2p_start_go()
        bssid = dev[0].get_group_status_field('bssid')

        wpas.scan_for_bss(bssid, res['freq'])
        res2 = connect_cli(dev[0], wpas, freq=res['freq'])
        if not res2['ifname'].startswith('p2p-' + iface):
            raise Exception("Unexpected group ifname: " + res2['ifname'])

        terminate_group(dev[0], wpas)

def test_p2p_device_join_no_group_iface(dev, apdev):
    """P2P join-group using cfg80211 P2P Device (no separate group interface)"""
    with HWSimRadio(use_p2p_device=True) as (radio, iface):
        wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
        wpas.interface_add(iface)
        wpas.global_request("SET p2p_no_group_iface 1")

        res = dev[0].p2p_start_go()
        bssid = dev[0].get_group_status_field('bssid')

        wpas.scan_for_bss(bssid, res['freq'])
        res2 = connect_cli(dev[0], wpas, freq=res['freq'])
        if res2['ifname'] != iface:
            raise Exception("Unexpected group ifname: " + res2['ifname'])

        terminate_group(dev[0], wpas)

def test_p2p_device_join_no_group_iface_cancel(dev, apdev):
    """P2P cancel join-group using cfg80211 P2P Device (no separate group interface)"""
    with HWSimRadio(use_p2p_device=True) as (radio, iface):
        wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
        wpas.interface_add(iface)
        wpas.global_request("SET p2p_no_group_iface 1")

        res = dev[0].p2p_start_go()
        bssid = dev[0].get_group_status_field('bssid')

        wpas.scan_for_bss(bssid, res['freq'])
        pin = wpas.wps_read_pin()
        dev[0].p2p_go_authorize_client(pin)
        cmd = "P2P_CONNECT %s %s join freq=%s" % (dev[0].p2p_dev_addr(), pin,
                                                  res['freq'])
        if "OK" not in wpas.request(cmd):
            raise Exception("P2P_CONNECT(join) failed")
        ev = wpas.wait_event(["CTRL-EVENT-SCAN-STARTED"], timeout=1)
        if "OK" not in wpas.request("P2P_CANCEL"):
            raise Exception("P2P_CANCEL failed")

        dev[0].remove_group()

def test_p2p_device_persistent_group(dev):
    """P2P persistent group formation and re-invocation with cfg80211 P2P Device"""
    with HWSimRadio(use_p2p_device=True) as (radio, iface):
        wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
        wpas.interface_add(iface)
        wpas.global_request("SET p2p_no_group_iface 0")

        form(dev[0], wpas)
        invite_from_cli(dev[0], wpas)
        invite_from_go(dev[0], wpas)

def test_p2p_device_persistent_group_no_group_iface(dev):
    """P2P persistent group formation and re-invocation with cfg80211 P2P Device (no separate group interface)"""
    with HWSimRadio(use_p2p_device=True) as (radio, iface):
        wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
        wpas.interface_add(iface)
        wpas.global_request("SET p2p_no_group_iface 1")

        form(dev[0], wpas)
        invite_from_cli(dev[0], wpas)
        invite_from_go(dev[0], wpas)

def test_p2p_device_persistent_group2(dev):
    """P2P persistent group formation and re-invocation (reverse) with cfg80211 P2P Device"""
    with HWSimRadio(use_p2p_device=True) as (radio, iface):
        wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
        wpas.interface_add(iface)
        wpas.global_request("SET p2p_no_group_iface 0")

        form(wpas, dev[0])
        invite_from_cli(wpas, dev[0])
        invite_from_go(wpas, dev[0])

def test_p2p_device_persistent_group2_no_group_iface(dev):
    """P2P persistent group formation and re-invocation (reverse) with cfg80211 P2P Device (no separate group interface)"""
    with HWSimRadio(use_p2p_device=True) as (radio, iface):
        wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
        wpas.interface_add(iface)
        wpas.global_request("SET p2p_no_group_iface 1")

        form(wpas, dev[0])
        invite_from_cli(wpas, dev[0])
        invite_from_go(wpas, dev[0])

def p2p_device_group_conf(dev1, dev2):
    dev1.global_request("SET p2p_group_idle 12")
    dev1.global_request("SET p2p_go_freq_change_policy 2")
    dev1.global_request("SET p2p_go_ctwindow 7")

    [i_res, r_res] = go_neg_pin_authorized(i_dev=dev1, i_intent=15,
                                           r_dev=dev2, r_intent=0)
    check_grpform_results(i_res, r_res)

    if (dev1.group_request("GET p2p_group_idle") != "12" or
        dev1.group_request("GET p2p_go_freq_change_policy") != "2" or
        dev1.group_request("GET p2p_go_ctwindow") != "7"):
        raise Exception("Unexpected configuration value")

    remove_group(dev1, dev2)
    dev1.global_request("P2P_FLUSH")
    dev2.global_request("P2P_FLUSH")

def test_p2p_device_conf(dev, apdev):
    """P2P configuration with cfg80211 P2P Device"""
    with HWSimRadio(use_p2p_device=True) as (radio, iface):
        wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
        wpas.interface_add(iface)
        wpas.global_request("SET p2p_no_group_iface 1")
        p2p_device_group_conf(wpas, dev[0])
        wpas.global_request("SET p2p_no_group_iface 0")
        p2p_device_group_conf(wpas, dev[0])

def test_p2p_device_autogo_chan_switch(dev):
    """P2P autonomous GO switching channels with cfg80211 P2P Device"""
    with HWSimRadio(use_p2p_device=True) as (radio, iface):
        wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
        wpas.interface_add(iface)
        wpas.global_request("SET p2p_no_group_iface 1")
        autogo(wpas, freq=2417)
        connect_cli(wpas, dev[1])
        res = wpas.group_request("CHAN_SWITCH 5 2422")
        if "FAIL" in res:
            # for now, skip test since mac80211_hwsim support is not yet widely
            # deployed
            raise HwsimSkip("Assume mac80211_hwsim did not support channel switching")
        ev = wpas.wait_group_event(["AP-CSA-FINISHED"], timeout=10)
        if ev is None:
            raise Exception("CSA finished event timed out")
        if "freq=2422" not in ev:
            raise Exception("Unexpected cahnnel in CSA finished event")
        wpas.dump_monitor()
        dev[1].dump_monitor()
        time.sleep(0.1)
        hwsim_utils.test_connectivity_p2p(wpas, dev[1])
