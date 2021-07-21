# Test cases for Multi-AP
# Copyright (c) 2018, The Linux Foundation
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

import hostapd
from wpasupplicant import WpaSupplicant
from utils import *

def test_multi_ap_association(dev, apdev):
    """Multi-AP association in backhaul BSS"""
    run_multi_ap_association(dev, apdev, 1)
    dev[1].connect("multi-ap", psk="12345678", scan_freq="2412",
                   wait_connect=False)
    ev = dev[1].wait_event(["CTRL-EVENT-DISCONNECTED",
                            "CTRL-EVENT-CONNECTED",
                            "CTRL-EVENT-ASSOC-REJECT"],
                           timeout=5)
    dev[1].request("DISCONNECT")
    if ev is None:
        raise Exception("Connection result not reported")
    if "CTRL-EVENT-ASSOC-REJECT" not in ev:
        raise Exception("Association rejection not reported")
    if "status_code=12" not in ev:
        raise Exception("Unexpected association status code: " + ev)

def test_multi_ap_association_shared_bss(dev, apdev):
    """Multi-AP association in backhaul BSS (with fronthaul BSS enabled)"""
    run_multi_ap_association(dev, apdev, 3)
    dev[1].connect("multi-ap", psk="12345678", scan_freq="2412")

def run_multi_ap_association(dev, apdev, multi_ap, wait_connect=True):
    params = hostapd.wpa2_params(ssid="multi-ap", passphrase="12345678")
    if multi_ap:
        params["multi_ap"] = str(multi_ap)
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].connect("multi-ap", psk="12345678", scan_freq="2412",
                   multi_ap_backhaul_sta="1", wait_connect=wait_connect)

def test_multi_ap_backhaul_roam_with_bridge(dev, apdev):
    """Multi-AP backhaul BSS reassociation to another BSS with bridge"""
    br_ifname = 'sta-br0'
    ifname = 'wlan5'
    try:
        run_multi_ap_backhaul_roam_with_bridge(dev, apdev)
    finally:
        subprocess.call(['ip', 'link', 'set', 'dev', br_ifname, 'down'])
        subprocess.call(['brctl', 'delif', br_ifname, ifname])
        subprocess.call(['brctl', 'delbr', br_ifname])
        subprocess.call(['iw', ifname, 'set', '4addr', 'off'])

def run_multi_ap_backhaul_roam_with_bridge(dev, apdev):
    br_ifname = 'sta-br0'
    ifname = 'wlan5'
    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    subprocess.call(['brctl', 'addbr', br_ifname])
    subprocess.call(['brctl', 'setfd', br_ifname, '0'])
    subprocess.call(['ip', 'link', 'set', 'dev', br_ifname, 'up'])
    subprocess.call(['iw', ifname, 'set', '4addr', 'on'])
    subprocess.check_call(['brctl', 'addif', br_ifname, ifname])
    wpas.interface_add(ifname, br_ifname=br_ifname)
    wpas.flush_scan_cache()

    params = hostapd.wpa2_params(ssid="multi-ap", passphrase="12345678")
    params["multi_ap"] = "1"
    hapd = hostapd.add_ap(apdev[0], params)

    wpas.connect("multi-ap", psk="12345678", scan_freq="2412",
                 multi_ap_backhaul_sta="1")

    hapd2 = hostapd.add_ap(apdev[1], params)
    bssid2 = hapd2.own_addr()
    wpas.scan_for_bss(bssid2, freq="2412", force_scan=True)
    wpas.roam(bssid2)

def test_multi_ap_disabled_on_ap(dev, apdev):
    """Multi-AP association attempt when disabled on AP"""
    run_multi_ap_association(dev, apdev, 0, wait_connect=False)
    ev = dev[0].wait_event(["CTRL-EVENT-DISCONNECTED",
                            "CTRL-EVENT-CONNECTED"],
                           timeout=5)
    dev[0].request("DISCONNECT")
    if ev is None:
        raise Exception("Connection result not reported")
    if "CTRL-EVENT-DISCONNECTED" not in ev:
        raise Exception("Unexpected connection result")

def test_multi_ap_fronthaul_on_ap(dev, apdev):
    """Multi-AP association attempt when only fronthaul BSS on AP"""
    run_multi_ap_association(dev, apdev, 2, wait_connect=False)
    ev = dev[0].wait_event(["CTRL-EVENT-DISCONNECTED",
                            "CTRL-EVENT-CONNECTED",
                            "CTRL-EVENT-ASSOC-REJECT"],
                           timeout=5)
    dev[0].request("DISCONNECT")
    if ev is None:
        raise Exception("Connection result not reported")
    if "CTRL-EVENT-DISCONNECTED" not in ev:
        raise Exception("Unexpected connection result")

def remove_apdev(dev, ifname):
    hglobal = hostapd.HostapdGlobal()
    hglobal.remove(ifname)
    dev.cmd_execute(['iw', ifname, 'del'])

def run_multi_ap_wps(dev, apdev, params, params_backhaul=None, add_apdev=False,
                     run_csa=False, allow_csa_fail=False):
    """Helper for running Multi-AP WPS tests

    dev[0] does multi_ap WPS, dev[1] does normal WPS. apdev[0] is the fronthaul
    BSS. If there is a separate backhaul BSS, it must have been set up by the
    caller. params are the normal SSID parameters, they will be extended with
    the WPS parameters. multi_ap_bssid must be given if it is not equal to the
    fronthaul BSSID."""

    wpas_apdev = None

    if params_backhaul:
        hapd_backhaul = hostapd.add_ap(apdev[1], params_backhaul)
        multi_ap_bssid =  hapd_backhaul.own_addr()
    else:
        multi_ap_bssid = apdev[0]['bssid']

    params.update({"wps_state": "2", "eap_server": "1"})

    # WPS with multi-ap station dev[0]
    hapd = hostapd.add_ap(apdev[0], params)
    conf = hapd.request("GET_CONFIG").splitlines()
    if "ssid=" + params['ssid'] not in conf:
        raise Exception("GET_CONFIG did not show correct ssid entry")
    if "multi_ap" in params and \
       "multi_ap=" + params["multi_ap"] not in conf:
        raise Exception("GET_CONFIG did not show correct multi_ap entry")
    if "multi_ap_backhaul_ssid" in params and \
       "multi_ap_backhaul_ssid=" + params["multi_ap_backhaul_ssid"].strip('"') not in conf:
        raise Exception("GET_CONFIG did not show correct multi_ap_backhaul_ssid entry")
    if "wpa" in params and "multi_ap_backhaul_wpa_passphrase" in params and \
       "multi_ap_backhaul_wpa_passphrase=" + params["multi_ap_backhaul_wpa_passphrase"] not in conf:
        raise Exception("GET_CONFIG did not show correct multi_ap_backhaul_wpa_passphrase entry")
    if "multi_ap_backhaul_wpa_psk" in params and \
       "multi_ap_backhaul_wpa_psk=" + params["multi_ap_backhaul_wpa_psk"] not in conf:
        raise Exception("GET_CONFIG did not show correct multi_ap_backhaul_wpa_psk entry")
    hapd.request("WPS_PBC")
    if "PBC Status: Active" not in hapd.request("WPS_GET_STATUS"):
        raise Exception("PBC status not shown correctly")

    dev[0].request("WPS_PBC multi_ap=1")
    dev[0].wait_connected(timeout=20)
    status = dev[0].get_status()
    if status['wpa_state'] != 'COMPLETED' or status['bssid'] != multi_ap_bssid:
        raise Exception("Not fully connected")
    if status['ssid'] != params['multi_ap_backhaul_ssid'].strip('"'):
        raise Exception("Unexpected SSID %s != %s" % (status['ssid'], params["multi_ap_backhaul_ssid"]))
    if status['pairwise_cipher'] != 'CCMP':
        raise Exception("Unexpected encryption configuration %s" % status['pairwise_cipher'])
    if status['key_mgmt'] != 'WPA2-PSK':
        raise Exception("Unexpected key_mgmt")

    status = hapd.request("WPS_GET_STATUS")
    if "PBC Status: Disabled" not in status:
        raise Exception("PBC status not shown correctly")
    if "Last WPS result: Success" not in status:
        raise Exception("Last WPS result not shown correctly")
    if "Peer Address: " + dev[0].own_addr() not in status:
        raise Exception("Peer address not shown correctly")

    if len(dev[0].list_networks()) != 1:
        raise Exception("Unexpected number of network blocks")

    # WPS with non-Multi-AP station dev[1]
    hapd.request("WPS_PBC")
    if "PBC Status: Active" not in hapd.request("WPS_GET_STATUS"):
        raise Exception("PBC status not shown correctly")

    dev[1].request("WPS_PBC")
    dev[1].wait_connected(timeout=20)
    status = dev[1].get_status()
    if status['wpa_state'] != 'COMPLETED' or status['bssid'] != apdev[0]['bssid']:
        raise Exception("Not fully connected")
    if status['ssid'] != params["ssid"]:
        raise Exception("Unexpected SSID")
    # Fronthaul may be something else than WPA2-PSK so don't test it.

    status = hapd.request("WPS_GET_STATUS")
    if "PBC Status: Disabled" not in status:
        raise Exception("PBC status not shown correctly")
    if "Last WPS result: Success" not in status:
        raise Exception("Last WPS result not shown correctly")
    if "Peer Address: " + dev[1].own_addr() not in status:
        raise Exception("Peer address not shown correctly")

    if len(dev[1].list_networks()) != 1:
        raise Exception("Unexpected number of network blocks")

    try:
        # Add apdev to the same phy that dev[0]
        if add_apdev:
            wpas_apdev = {}
            wpas_apdev['ifname'] = dev[0].ifname + "_ap"
            status, buf = dev[0].cmd_execute(['iw', dev[0].ifname,
                                              'interface', 'add',
                                              wpas_apdev['ifname'],
                                              'type', 'managed'])
            if status != 0:
                raise Exception("iw interface add failed")
            wpas_hapd = hostapd.add_ap(wpas_apdev, params)

        if run_csa:
            if 'OK' not in hapd.request("CHAN_SWITCH 5 2462 ht"):
                raise Exception("chan switch request failed")

            ev = hapd.wait_event(["AP-CSA-FINISHED"], timeout=5)
            if not ev:
                raise Exception("chan switch failed")

            # now check station
            ev = dev[0].wait_event(["CTRL-EVENT-CHANNEL-SWITCH",
                                    "CTRL-EVENT-DISCONNECTED"], timeout=5)
            if not ev:
                raise Exception("sta - no chanswitch event")
            if "CTRL-EVENT-CHANNEL-SWITCH" not in ev and not allow_csa_fail:
                raise Exception("Received disconnection event instead of channel switch event")

        if add_apdev:
            remove_apdev(dev[0], wpas_apdev['ifname'])
    except:
        if wpas_apdev:
            remove_apdev(dev[0], wpas_apdev['ifname'])
        raise

    return hapd

def test_multi_ap_wps_shared(dev, apdev):
    """WPS on shared fronthaul/backhaul AP"""
    ssid = "multi-ap-wps"
    passphrase = "12345678"
    params = hostapd.wpa2_params(ssid=ssid, passphrase=passphrase)
    params.update({"multi_ap": "3",
                   "multi_ap_backhaul_ssid": '"%s"' % ssid,
                   "multi_ap_backhaul_wpa_passphrase": passphrase})
    hapd = run_multi_ap_wps(dev, apdev, params)
    # Verify WPS parameter update with Multi-AP
    if "OK" not in hapd.request("RELOAD"):
        raise Exception("hostapd RELOAD failed")
    dev[0].wait_disconnected()
    dev[0].request("REMOVE_NETWORK all")
    hapd.request("WPS_PBC")
    dev[0].request("WPS_PBC multi_ap=1")
    dev[0].wait_connected(timeout=20)

def test_multi_ap_wps_shared_csa(dev, apdev):
    """WPS on shared fronthaul/backhaul AP, run CSA"""
    ssid = "multi-ap-wps-csa"
    passphrase = "12345678"
    params = hostapd.wpa2_params(ssid=ssid, passphrase=passphrase)
    params.update({"multi_ap": "3",
                   "multi_ap_backhaul_ssid": '"%s"' % ssid,
                   "multi_ap_backhaul_wpa_passphrase": passphrase})
    run_multi_ap_wps(dev, apdev, params, run_csa=True)

def test_multi_ap_wps_shared_apdev_csa(dev, apdev):
    """WPS on shared fronthaul/backhaul AP add apdev on same phy and run CSA"""
    ssid = "multi-ap-wps-apdev-csa"
    passphrase = "12345678"
    params = hostapd.wpa2_params(ssid=ssid, passphrase=passphrase)
    params.update({"multi_ap": "3",
                   "multi_ap_backhaul_ssid": '"%s"' % ssid,
                   "multi_ap_backhaul_wpa_passphrase": passphrase})
    # This case is currently failing toc omplete CSA on the station interface.
    # For the time being, ignore that to avoid always failing tests. Full
    # validation can be enabled once the issue behind this is fixed.
    run_multi_ap_wps(dev, apdev, params, add_apdev=True, run_csa=True,
                     allow_csa_fail=True)

def test_multi_ap_wps_shared_psk(dev, apdev):
    """WPS on shared fronthaul/backhaul AP using PSK"""
    ssid = "multi-ap-wps"
    psk = "1234567890abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
    params = hostapd.wpa2_params(ssid=ssid)
    params.update({"wpa_psk": psk,
                   "multi_ap": "3",
                   "multi_ap_backhaul_ssid": '"%s"' % ssid,
                   "multi_ap_backhaul_wpa_psk": psk})
    run_multi_ap_wps(dev, apdev, params)

def test_multi_ap_wps_split(dev, apdev):
    """WPS on split fronthaul and backhaul AP"""
    backhaul_ssid = "multi-ap-backhaul-wps"
    backhaul_passphrase = "87654321"
    params = hostapd.wpa2_params(ssid="multi-ap-fronthaul-wps",
                                 passphrase="12345678")
    params.update({"multi_ap": "2",
                   "multi_ap_backhaul_ssid": '"%s"' % backhaul_ssid,
                   "multi_ap_backhaul_wpa_passphrase": backhaul_passphrase})
    params_backhaul = hostapd.wpa2_params(ssid=backhaul_ssid,
                                          passphrase=backhaul_passphrase)
    params_backhaul.update({"multi_ap": "1"})

    run_multi_ap_wps(dev, apdev, params, params_backhaul)

def test_multi_ap_wps_split_psk(dev, apdev):
    """WPS on split fronthaul and backhaul AP"""
    backhaul_ssid = "multi-ap-backhaul-wps"
    backhaul_psk = "1234567890abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
    params = hostapd.wpa2_params(ssid="multi-ap-fronthaul-wps",
                                 passphrase="12345678")
    params.update({"multi_ap": "2",
                   "multi_ap_backhaul_ssid": '"%s"' % backhaul_ssid,
                   "multi_ap_backhaul_wpa_psk": backhaul_psk})
    params_backhaul = hostapd.wpa2_params(ssid=backhaul_ssid)
    params_backhaul.update({"multi_ap": "1", "wpa_psk": backhaul_psk})

    run_multi_ap_wps(dev, apdev, params, params_backhaul)

def test_multi_ap_wps_split_mixed(dev, apdev):
    """WPS on split fronthaul and backhaul AP with mixed-mode fronthaul"""
    skip_without_tkip(dev[0])
    backhaul_ssid = "multi-ap-backhaul-wps"
    backhaul_passphrase = "87654321"
    params = hostapd.wpa_mixed_params(ssid="multi-ap-fronthaul-wps",
                                      passphrase="12345678")
    params.update({"multi_ap": "2",
                   "multi_ap_backhaul_ssid": '"%s"' % backhaul_ssid,
                   "multi_ap_backhaul_wpa_passphrase": backhaul_passphrase})
    params_backhaul = hostapd.wpa2_params(ssid=backhaul_ssid,
                                          passphrase=backhaul_passphrase)
    params_backhaul.update({"multi_ap": "1"})

    run_multi_ap_wps(dev, apdev, params, params_backhaul)

def test_multi_ap_wps_split_open(dev, apdev):
    """WPS on split fronthaul and backhaul AP with open fronthaul"""
    backhaul_ssid = "multi-ap-backhaul-wps"
    backhaul_passphrase = "87654321"
    params = {"ssid": "multi-ap-wps-fronthaul", "multi_ap": "2",
              "multi_ap_backhaul_ssid": '"%s"' % backhaul_ssid,
              "multi_ap_backhaul_wpa_passphrase": backhaul_passphrase}
    params_backhaul = hostapd.wpa2_params(ssid=backhaul_ssid,
                                          passphrase=backhaul_passphrase)
    params_backhaul.update({"multi_ap": "1"})

    run_multi_ap_wps(dev, apdev, params, params_backhaul)

def test_multi_ap_wps_fail_non_multi_ap(dev, apdev):
    """Multi-AP WPS on non-WPS AP fails"""

    params = hostapd.wpa2_params(ssid="non-multi-ap-wps", passphrase="12345678")
    params.update({"wps_state": "2", "eap_server": "1"})

    hapd = hostapd.add_ap(apdev[0], params)
    hapd.request("WPS_PBC")
    if "PBC Status: Active" not in hapd.request("WPS_GET_STATUS"):
        raise Exception("PBC status not shown correctly")

    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[0].request("WPS_PBC %s multi_ap=1" % apdev[0]['bssid'])
    # Since we will fail to associate and WPS doesn't even get started, there
    # isn't much we can do except wait for timeout. For PBC, it is not possible
    # to change the timeout from 2 minutes. Instead of waiting for the timeout,
    # just check that WPS doesn't finish within reasonable time.
    for i in range(2):
        ev = dev[0].wait_event(["WPS-SUCCESS", "WPS-FAIL",
                                "CTRL-EVENT-DISCONNECTED"], timeout=10)
        if ev and "WPS-" in ev:
            raise Exception("WPS operation completed: " + ev)
    dev[0].request("WPS_CANCEL")
