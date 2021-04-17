# Protected management frames tests
# Copyright (c) 2013, Jouni Malinen <j@w1.fi>
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

from remotehost import remote_compatible
import binascii
import os
import time
import logging
logger = logging.getLogger()

import hwsim_utils
import hostapd
from utils import *
from wlantest import Wlantest
from wpasupplicant import WpaSupplicant

@remote_compatible
def test_ap_pmf_required(dev, apdev):
    """WPA2-PSK AP with PMF required"""
    ssid = "test-pmf-required"
    params = hostapd.wpa2_params(ssid=ssid, passphrase="12345678")
    params["wpa_key_mgmt"] = "WPA-PSK-SHA256"
    params["ieee80211w"] = "2"
    hapd = hostapd.add_ap(apdev[0], params)
    Wlantest.setup(hapd)
    wt = Wlantest()
    wt.flush()
    wt.add_passphrase("12345678")
    key_mgmt = hapd.get_config()['key_mgmt']
    if key_mgmt.split(' ')[0] != "WPA-PSK-SHA256":
        raise Exception("Unexpected GET_CONFIG(key_mgmt): " + key_mgmt)
    dev[0].connect(ssid, psk="12345678", ieee80211w="1",
                   key_mgmt="WPA-PSK WPA-PSK-SHA256", proto="WPA2",
                   scan_freq="2412")
    if "[WPA2-PSK-SHA256-CCMP]" not in dev[0].request("SCAN_RESULTS"):
        raise Exception("Scan results missing RSN element info")
    hwsim_utils.test_connectivity(dev[0], hapd)
    dev[1].connect(ssid, psk="12345678", ieee80211w="2",
                   key_mgmt="WPA-PSK WPA-PSK-SHA256", proto="WPA2",
                   scan_freq="2412")
    hwsim_utils.test_connectivity(dev[1], hapd)
    if "OK" not in hapd.request("SA_QUERY " + dev[0].own_addr()):
        raise Exception("SA_QUERY failed")
    if "OK" not in hapd.request("SA_QUERY " + dev[1].own_addr()):
        raise Exception("SA_QUERY failed")
    if "FAIL" not in hapd.request("SA_QUERY foo"):
        raise Exception("Invalid SA_QUERY accepted")
    wt.require_ap_pmf_mandatory(apdev[0]['bssid'])
    wt.require_sta_pmf(apdev[0]['bssid'], dev[0].p2p_interface_addr())
    wt.require_sta_pmf_mandatory(apdev[0]['bssid'], dev[1].p2p_interface_addr())
    time.sleep(0.1)
    if wt.get_sta_counter("valid_saqueryresp_tx", apdev[0]['bssid'],
                          dev[0].p2p_interface_addr()) < 1:
        raise Exception("STA did not reply to SA Query")
    if wt.get_sta_counter("valid_saqueryresp_tx", apdev[0]['bssid'],
                          dev[1].p2p_interface_addr()) < 1:
        raise Exception("STA did not reply to SA Query")

def start_ocv_ap(apdev):
    ssid = "test-pmf-required"
    params = hostapd.wpa2_params(ssid=ssid, passphrase="12345678")
    params["wpa_key_mgmt"] = "WPA-PSK-SHA256"
    params["ieee80211w"] = "2"
    params["ocv"] = "1"
    try:
        hapd = hostapd.add_ap(apdev, params)
    except Exception as e:
        if "Failed to set hostapd parameter ocv" in str(e):
            raise HwsimSkip("OCV not supported")
        raise

    Wlantest.setup(hapd)
    wt = Wlantest()
    wt.flush()
    wt.add_passphrase("12345678")

    return hapd, ssid, wt

@remote_compatible
def test_ocv_sa_query(dev, apdev):
    """Test SA Query with OCV"""
    hapd, ssid, wt = start_ocv_ap(apdev[0])
    dev[0].connect(ssid, psk="12345678", ieee80211w="1", ocv="1",
                   key_mgmt="WPA-PSK WPA-PSK-SHA256", proto="WPA2",
                   scan_freq="2412")

    # Test that client can handle SA Query with OCI element
    if "OK" not in hapd.request("SA_QUERY " + dev[0].own_addr()):
        raise Exception("SA_QUERY failed")
    ev = hapd.wait_event(["OCV-FAILURE"], timeout=0.1)
    if ev:
        raise Exception("Unexpected OCV failure reported")
    if wt.get_sta_counter("valid_saqueryresp_tx", apdev[0]['bssid'],
                          dev[0].own_addr()) < 1:
        raise Exception("STA did not reply to SA Query")

    # Test that AP can handle SA Query with OCI element
    if "OK" not in dev[0].request("UNPROT_DEAUTH"):
        raise Exception("Triggering SA Query from the STA failed")
    ev = dev[0].wait_event(["CTRL-EVENT-DISCONNECTED"], timeout=3)
    if ev is not None:
        raise Exception("SA Query from the STA failed")

@remote_compatible
def test_ocv_sa_query_csa(dev, apdev):
    """Test SA Query with OCV after channel switch"""
    hapd, ssid, wt = start_ocv_ap(apdev[0])
    dev[0].connect(ssid, psk="12345678", ieee80211w="1", ocv="1",
                   key_mgmt="WPA-PSK WPA-PSK-SHA256", proto="WPA2",
                   scan_freq="2412")

    hapd.request("CHAN_SWITCH 5 2437")
    time.sleep(1)
    if wt.get_sta_counter("valid_saqueryreq_tx", apdev[0]['bssid'],
                          dev[0].own_addr()) < 1:
        raise Exception("STA did not start SA Query after channel switch")

    ev = dev[0].wait_event(["CTRL-EVENT-DISCONNECTED"], timeout=16)
    if ev is not None:
        raise Exception("Unexpected disconnection")

def test_ocv_sa_query_csa_no_resp(dev, apdev):
    """Test SA Query with OCV after channel switch getting no response"""
    hapd, ssid, wt = start_ocv_ap(apdev[0])
    dev[0].connect(ssid, psk="12345678", ieee80211w="1", ocv="1",
                   key_mgmt="WPA-PSK WPA-PSK-SHA256", proto="WPA2",
                   scan_freq="2412")

    hapd.set("ext_mgmt_frame_handling", "1")
    hapd.request("CHAN_SWITCH 5 2437")
    ev = dev[0].wait_event(["CTRL-EVENT-DISCONNECTED"], timeout=5)
    if ev is None:
        raise Exception("Disconnection after CSA not reported")
    if "locally_generated=1" not in ev:
        raise Exception("Unexpectedly disconnected by AP: " + ev)

def test_ocv_sa_query_csa_missing(dev, apdev):
    """Test SA Query with OCV missing after channel switch"""
    hapd, ssid, wt = start_ocv_ap(apdev[0])
    dev[0].connect(ssid, psk="12345678", ieee80211w="1", ocv="1",
                   key_mgmt="WPA-PSK WPA-PSK-SHA256", proto="WPA2",
                   scan_freq="2412")

    hapd.set("ext_mgmt_frame_handling", "1")
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()
    ev = hapd.wait_event(['MGMT-RX'], timeout=5)
    if ev is None:
        raise Exception("Deauthentication frame RX not reported")
    hapd.set("ext_mgmt_frame_handling", "0")
    hapd.request("CHAN_SWITCH 5 2437")
    ev = hapd.wait_event(["AP-STA-DISCONNECTED"], timeout=20)
    if ev is None:
        raise Exception("No disconnection event received from hostapd")

@remote_compatible
def test_ap_pmf_optional(dev, apdev):
    """WPA2-PSK AP with PMF optional"""
    ssid = "test-pmf-optional"
    params = hostapd.wpa2_params(ssid=ssid, passphrase="12345678")
    params["wpa_key_mgmt"] = "WPA-PSK"
    params["ieee80211w"] = "1"
    hapd = hostapd.add_ap(apdev[0], params)
    Wlantest.setup(hapd)
    wt = Wlantest()
    wt.flush()
    wt.add_passphrase("12345678")
    dev[0].connect(ssid, psk="12345678", ieee80211w="1",
                   key_mgmt="WPA-PSK WPA-PSK-SHA256", proto="WPA2",
                   scan_freq="2412")
    hwsim_utils.test_connectivity(dev[0], hapd)
    dev[1].connect(ssid, psk="12345678", ieee80211w="2",
                   key_mgmt="WPA-PSK WPA-PSK-SHA256", proto="WPA2",
                   scan_freq="2412")
    hwsim_utils.test_connectivity(dev[1], hapd)
    wt.require_ap_pmf_optional(apdev[0]['bssid'])
    wt.require_sta_pmf(apdev[0]['bssid'], dev[0].p2p_interface_addr())
    wt.require_sta_pmf_mandatory(apdev[0]['bssid'], dev[1].p2p_interface_addr())

@remote_compatible
def test_ap_pmf_optional_2akm(dev, apdev):
    """WPA2-PSK AP with PMF optional (2 AKMs)"""
    ssid = "test-pmf-optional-2akm"
    params = hostapd.wpa2_params(ssid=ssid, passphrase="12345678")
    params["wpa_key_mgmt"] = "WPA-PSK WPA-PSK-SHA256"
    params["ieee80211w"] = "1"
    hapd = hostapd.add_ap(apdev[0], params)
    Wlantest.setup(hapd)
    wt = Wlantest()
    wt.flush()
    wt.add_passphrase("12345678")
    dev[0].connect(ssid, psk="12345678", ieee80211w="1",
                   key_mgmt="WPA-PSK WPA-PSK-SHA256", proto="WPA2",
                   scan_freq="2412")
    hwsim_utils.test_connectivity(dev[0], hapd)
    dev[1].connect(ssid, psk="12345678", ieee80211w="2",
                   key_mgmt="WPA-PSK WPA-PSK-SHA256", proto="WPA2",
                   scan_freq="2412")
    hwsim_utils.test_connectivity(dev[1], hapd)
    wt.require_ap_pmf_optional(apdev[0]['bssid'])
    wt.require_sta_pmf(apdev[0]['bssid'], dev[0].p2p_interface_addr())
    wt.require_sta_key_mgmt(apdev[0]['bssid'], dev[0].p2p_interface_addr(),
                            "PSK-SHA256")
    wt.require_sta_pmf_mandatory(apdev[0]['bssid'], dev[1].p2p_interface_addr())
    wt.require_sta_key_mgmt(apdev[0]['bssid'], dev[1].p2p_interface_addr(),
                            "PSK-SHA256")

@remote_compatible
def test_ap_pmf_negative(dev, apdev):
    """WPA2-PSK AP without PMF (negative test)"""
    ssid = "test-pmf-negative"
    params = hostapd.wpa2_params(ssid=ssid, passphrase="12345678")
    hapd = hostapd.add_ap(apdev[0], params)
    Wlantest.setup(hapd)
    wt = Wlantest()
    wt.flush()
    wt.add_passphrase("12345678")
    dev[0].connect(ssid, psk="12345678", ieee80211w="1",
                   key_mgmt="WPA-PSK WPA-PSK-SHA256", proto="WPA2",
                   scan_freq="2412")
    hwsim_utils.test_connectivity(dev[0], hapd)
    try:
        dev[1].connect(ssid, psk="12345678", ieee80211w="2",
                       key_mgmt="WPA-PSK WPA-PSK-SHA256", proto="WPA2",
                       scan_freq="2412")
        hwsim_utils.test_connectivity(dev[1], hapd)
        raise Exception("PMF required STA connected to no PMF AP")
    except Exception as e:
        logger.debug("Ignore expected exception: " + str(e))
    wt.require_ap_no_pmf(apdev[0]['bssid'])

@remote_compatible
def test_ap_pmf_assoc_comeback(dev, apdev):
    """WPA2-PSK AP with PMF association comeback"""
    ssid = "assoc-comeback"
    params = hostapd.wpa2_params(ssid=ssid, passphrase="12345678")
    params["wpa_key_mgmt"] = "WPA-PSK-SHA256"
    params["ieee80211w"] = "2"
    hapd = hostapd.add_ap(apdev[0], params)
    Wlantest.setup(hapd)
    wt = Wlantest()
    wt.flush()
    wt.add_passphrase("12345678")
    dev[0].connect(ssid, psk="12345678", ieee80211w="1",
                   key_mgmt="WPA-PSK WPA-PSK-SHA256", proto="WPA2",
                   scan_freq="2412")
    hapd.set("ext_mgmt_frame_handling", "1")
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected(timeout=10)
    hapd.set("ext_mgmt_frame_handling", "0")
    dev[0].request("REASSOCIATE")
    dev[0].wait_connected(timeout=10, error="Timeout on re-connection")
    if wt.get_sta_counter("assocresp_comeback", apdev[0]['bssid'],
                          dev[0].p2p_interface_addr()) < 1:
        raise Exception("AP did not use association comeback request")

@remote_compatible
def test_ap_pmf_assoc_comeback2(dev, apdev):
    """WPA2-PSK AP with PMF association comeback (using DROP_SA)"""
    ssid = "assoc-comeback"
    params = hostapd.wpa2_params(ssid=ssid, passphrase="12345678")
    params["wpa_key_mgmt"] = "WPA-PSK"
    params["ieee80211w"] = "1"
    hapd = hostapd.add_ap(apdev[0], params)
    Wlantest.setup(hapd)
    wt = Wlantest()
    wt.flush()
    wt.add_passphrase("12345678")
    dev[0].connect(ssid, psk="12345678", ieee80211w="2",
                   key_mgmt="WPA-PSK", proto="WPA2", scan_freq="2412")
    if "OK" not in dev[0].request("DROP_SA"):
        raise Exception("DROP_SA failed")
    dev[0].request("REASSOCIATE")
    dev[0].wait_connected(timeout=10, error="Timeout on re-connection")
    if wt.get_sta_counter("reassocresp_comeback", apdev[0]['bssid'],
                          dev[0].p2p_interface_addr()) < 1:
        raise Exception("AP did not use reassociation comeback request")

@remote_compatible
def test_ap_pmf_assoc_comeback_wps(dev, apdev):
    """WPA2-PSK AP with PMF association comeback (WPS)"""
    ssid = "assoc-comeback"
    appin = "12345670"
    params = hostapd.wpa2_params(ssid=ssid, passphrase="12345678")
    params["wpa_key_mgmt"] = "WPA-PSK-SHA256"
    params["ieee80211w"] = "2"
    params["eap_server"] = "1"
    params["wps_state"] = "2"
    params["ap_pin"] = appin
    hapd = hostapd.add_ap(apdev[0], params)
    Wlantest.setup(hapd)
    wt = Wlantest()
    wt.flush()
    wt.add_passphrase("12345678")
    dev[0].connect(ssid, psk="12345678", ieee80211w="1",
                   key_mgmt="WPA-PSK WPA-PSK-SHA256", proto="WPA2",
                   scan_freq="2412")
    hapd.set("ext_mgmt_frame_handling", "1")
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected(timeout=10)
    hapd.set("ext_mgmt_frame_handling", "0")
    dev[0].wps_reg(apdev[0]['bssid'], appin)
    if wt.get_sta_counter("assocresp_comeback", apdev[0]['bssid'],
                          dev[0].p2p_interface_addr()) < 1:
        raise Exception("AP did not use association comeback request")

def test_ap_pmf_ap_dropping_sa(dev, apdev):
    """WPA2-PSK PMF AP dropping SA"""
    ssid = "pmf"
    params = hostapd.wpa2_params(ssid=ssid, passphrase="12345678")
    params["wpa_key_mgmt"] = "WPA-PSK-SHA256"
    params["ieee80211w"] = "2"
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = hapd.own_addr()
    Wlantest.setup(hapd)
    wt = Wlantest()
    wt.flush()
    wt.add_passphrase("12345678")
    dev[0].connect(ssid, psk="12345678", ieee80211w="2",
                   key_mgmt="WPA-PSK-SHA256", proto="WPA2", scan_freq="2412")
    addr0 = dev[0].own_addr()
    dev[0].dump_monitor()
    hapd.wait_sta()
    # Drop SA and association at the AP locally without notifying the STA. This
    # results in the STA getting unprotected Deauthentication frames when trying
    # to transmit the next Class 3 frame.
    if "OK" not in hapd.request("DEAUTHENTICATE " + addr0 + " tx=0"):
        raise Exception("DEAUTHENTICATE command failed")
    ev = dev[0].wait_event(["CTRL-EVENT-DISCONNECTED"], timeout=1)
    if ev is not None:
        raise Exception("Unexpected disconnection event after DEAUTHENTICATE tx=0: " + ev)
    dev[0].request("DATA_TEST_CONFIG 1")
    dev[0].request("DATA_TEST_TX " + bssid + " " + addr0)
    ev = dev[0].wait_event(["CTRL-EVENT-DISCONNECTED"], timeout=5)
    dev[0].request("DATA_TEST_CONFIG 0")
    if ev is None or "locally_generated=1" not in ev:
        raise Exception("Locally generated disconnection not reported")

def test_ap_pmf_valid_broadcast_deauth(dev, apdev):
    """WPA2-PSK PMF AP sending valid broadcast deauth without dropping SA"""
    run_ap_pmf_valid(dev, apdev, False, True)

def test_ap_pmf_valid_broadcast_disassoc(dev, apdev):
    """WPA2-PSK PMF AP sending valid broadcast disassoc without dropping SA"""
    run_ap_pmf_valid(dev, apdev, True, True)

def test_ap_pmf_valid_unicast_deauth(dev, apdev):
    """WPA2-PSK PMF AP sending valid unicast deauth without dropping SA"""
    run_ap_pmf_valid(dev, apdev, False, False)

def test_ap_pmf_valid_unicast_disassoc(dev, apdev):
    """WPA2-PSK PMF AP sending valid unicast disassoc without dropping SA"""
    run_ap_pmf_valid(dev, apdev, True, False)

def run_ap_pmf_valid(dev, apdev, disassociate, broadcast):
    ssid = "pmf"
    params = hostapd.wpa2_params(ssid=ssid, passphrase="12345678")
    params["wpa_key_mgmt"] = "WPA-PSK-SHA256"
    params["ieee80211w"] = "2"
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = hapd.own_addr()
    Wlantest.setup(hapd)
    wt = Wlantest()
    wt.flush()
    wt.add_passphrase("12345678")
    dev[0].connect(ssid, psk="12345678", ieee80211w="2",
                   key_mgmt="WPA-PSK-SHA256", proto="WPA2", scan_freq="2412")
    addr0 = dev[0].own_addr()
    dev[0].dump_monitor()
    hapd.wait_sta()
    cmd = "DISASSOCIATE " if disassociate else "DEAUTHENTICATE "
    cmd += "ff:ff:ff:ff:ff:ff" if broadcast else addr0
    cmd += " test=1"
    if "OK" not in hapd.request(cmd):
        raise Exception("hostapd command failed")
    sta = hapd.get_sta(addr0)
    if not sta:
        raise Exception("STA entry lost")
    ev = dev[0].wait_event(["CTRL-EVENT-DISCONNECTED"], timeout=5)
    if ev is None:
        raise Exception("Disconnection not reported")
    if "locally_generated=1" in ev:
        raise Exception("Unexpected locally generated disconnection")

    # Wait for SA Query procedure to fail and association comeback to succeed
    dev[0].wait_connected()

def start_wpas_ap(ssid):
    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    wpas.interface_add("wlan5", drv_params="use_monitor=1")
    id = wpas.add_network()
    wpas.set_network(id, "mode", "2")
    wpas.set_network_quoted(id, "ssid", ssid)
    wpas.set_network(id, "proto", "WPA2")
    wpas.set_network(id, "key_mgmt", "WPA-PSK-SHA256")
    wpas.set_network(id, "ieee80211w", "2")
    wpas.set_network_quoted(id, "psk", "12345678")
    wpas.set_network(id, "pairwise", "CCMP")
    wpas.set_network(id, "group", "CCMP")
    wpas.set_network(id, "frequency", "2412")
    wpas.set_network(id, "scan_freq", "2412")
    wpas.connect_network(id)
    wpas.dump_monitor()
    return wpas

def test_ap_pmf_sta_sa_query(dev, apdev):
    """WPA2-PSK AP with station using SA Query"""
    ssid = "assoc-comeback"
    addr = dev[0].own_addr()

    wpas = start_wpas_ap(ssid)
    bssid = wpas.own_addr()

    Wlantest.setup(wpas)
    wt = Wlantest()
    wt.flush()
    wt.add_passphrase("12345678")

    dev[0].connect(ssid, psk="12345678", ieee80211w="1",
                   key_mgmt="WPA-PSK WPA-PSK-SHA256", proto="WPA2",
                   scan_freq="2412")
    wpas.dump_monitor()
    wpas.request("DEAUTHENTICATE " + addr + " test=0")
    wpas.dump_monitor()
    wpas.request("DISASSOCIATE " + addr + " test=0")
    wpas.dump_monitor()
    ev = dev[0].wait_event(["CTRL-EVENT-DISCONNECTED"], timeout=1)
    if ev is not None:
        raise Exception("Unexpected disconnection")

    wpas.request("DEAUTHENTICATE " + addr + " reason=6 test=0")
    wpas.dump_monitor()
    wpas.request("DISASSOCIATE " + addr + " reason=7 test=0")
    wpas.dump_monitor()
    ev = dev[0].wait_event(["CTRL-EVENT-DISCONNECTED"], timeout=1)
    if ev is not None:
        raise Exception("Unexpected disconnection")
    if wt.get_sta_counter("valid_saqueryreq_tx", bssid, addr) < 1:
        raise Exception("STA did not send SA Query")
    if wt.get_sta_counter("valid_saqueryresp_rx", bssid, addr) < 1:
        raise Exception("AP did not reply to SA Query")
    wpas.dump_monitor()

def test_ap_pmf_sta_sa_query_no_response(dev, apdev):
    """WPA2-PSK AP with station using SA Query and getting no response"""
    ssid = "assoc-comeback"
    addr = dev[0].own_addr()

    wpas = start_wpas_ap(ssid)
    bssid = wpas.own_addr()

    dev[0].connect(ssid, psk="12345678", ieee80211w="1",
                   key_mgmt="WPA-PSK WPA-PSK-SHA256", proto="WPA2",
                   scan_freq="2412")
    wpas.dump_monitor()
    wpas.request("DEAUTHENTICATE " + addr + " test=0")
    wpas.dump_monitor()
    wpas.request("DISASSOCIATE " + addr + " test=0")
    wpas.dump_monitor()
    ev = dev[0].wait_event(["CTRL-EVENT-DISCONNECTED"], timeout=1)
    if ev is not None:
        raise Exception("Unexpected disconnection")

    wpas.request("SET ext_mgmt_frame_handling 1")
    wpas.request("DEAUTHENTICATE " + addr + " reason=6 test=0")
    wpas.dump_monitor()
    wpas.request("DISASSOCIATE " + addr + " reason=7 test=0")
    wpas.dump_monitor()
    dev[0].wait_disconnected()
    wpas.dump_monitor()
    wpas.request("SET ext_mgmt_frame_handling 0")
    dev[0].wait_connected()
    wpas.dump_monitor()

def test_ap_pmf_sta_unprot_deauth_burst(dev, apdev):
    """WPA2-PSK AP with station receiving burst of unprotected Deauthentication frames"""
    ssid = "deauth-attack"
    addr = dev[0].own_addr()

    wpas = start_wpas_ap(ssid)
    bssid = wpas.own_addr()

    Wlantest.setup(wpas)
    wt = Wlantest()
    wt.flush()
    wt.add_passphrase("12345678")

    dev[0].connect(ssid, psk="12345678", ieee80211w="1",
                   key_mgmt="WPA-PSK WPA-PSK-SHA256", proto="WPA2",
                   scan_freq="2412")

    for i in range(0, 10):
        wpas.request("DEAUTHENTICATE " + addr + " reason=6 test=0")
        wpas.request("DISASSOCIATE " + addr + " reason=7 test=0")
    ev = dev[0].wait_event(["CTRL-EVENT-DISCONNECTED"], timeout=1)
    if ev is not None:
        raise Exception("Unexpected disconnection")
    num_req = wt.get_sta_counter("valid_saqueryreq_tx", bssid, addr)
    num_resp = wt.get_sta_counter("valid_saqueryresp_rx", bssid, addr)
    if num_req < 1:
        raise Exception("STA did not send SA Query")
    if num_resp < 1:
        raise Exception("AP did not reply to SA Query")
    if num_req > 1:
        raise Exception("STA initiated too many SA Query procedures (%d)" % num_req)

    time.sleep(10)
    for i in range(0, 5):
        wpas.request("DEAUTHENTICATE " + addr + " reason=6 test=0")
        wpas.request("DISASSOCIATE " + addr + " reason=7 test=0")
    ev = dev[0].wait_event(["CTRL-EVENT-DISCONNECTED"], timeout=1)
    if ev is not None:
        raise Exception("Unexpected disconnection")
    num_req = wt.get_sta_counter("valid_saqueryreq_tx", bssid, addr)
    num_resp = wt.get_sta_counter("valid_saqueryresp_rx", bssid, addr)
    if num_req != 2 or num_resp != 2:
        raise Exception("Unexpected number of SA Query procedures (req=%d resp=%d)" % (num_req, num_resp))

def test_ap_pmf_sta_sa_query_oom(dev, apdev):
    """WPA2-PSK AP with station using SA Query (OOM)"""
    ssid = "assoc-comeback"
    addr = dev[0].own_addr()
    wpas = start_wpas_ap(ssid)
    dev[0].connect(ssid, psk="12345678", ieee80211w="1",
                   key_mgmt="WPA-PSK WPA-PSK-SHA256", proto="WPA2",
                   scan_freq="2412")
    with alloc_fail(dev[0], 1, "=sme_sa_query_timer"):
        wpas.request("DEAUTHENTICATE " + addr + " reason=6 test=0")
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
    dev[0].request("DISCONNECT")
    wpas.request("DISCONNECT")
    dev[0].wait_disconnected()

def test_ap_pmf_sta_sa_query_local_failure(dev, apdev):
    """WPA2-PSK AP with station using SA Query (local failure)"""
    ssid = "assoc-comeback"
    addr = dev[0].own_addr()
    wpas = start_wpas_ap(ssid)
    dev[0].connect(ssid, psk="12345678", ieee80211w="1",
                   key_mgmt="WPA-PSK WPA-PSK-SHA256", proto="WPA2",
                   scan_freq="2412")
    with fail_test(dev[0], 1, "os_get_random;sme_sa_query_timer"):
        wpas.request("DEAUTHENTICATE " + addr + " reason=6 test=0")
        wait_fail_trigger(dev[0], "GET_FAIL")
    dev[0].request("DISCONNECT")
    wpas.request("DISCONNECT")
    dev[0].wait_disconnected()

def test_ap_pmf_sta_sa_query_hostapd(dev, apdev):
    """WPA2-PSK AP with station using SA Query (hostapd)"""
    ssid = "assoc-comeback"
    passphrase = "12345678"
    addr = dev[0].own_addr()

    params = hostapd.wpa2_params(ssid=ssid, passphrase=passphrase,
                                 wpa_key_mgmt="WPA-PSK-SHA256",
                                 ieee80211w="2")
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = hapd.own_addr()

    Wlantest.setup(hapd)
    wt = Wlantest()
    wt.flush()
    wt.add_passphrase("12345678")

    dev[0].connect(ssid, psk=passphrase, ieee80211w="2",
                   key_mgmt="WPA-PSK-SHA256", proto="WPA2",
                   scan_freq="2412")
    hapd.wait_sta()
    if "OK" not in hapd.request("DEAUTHENTICATE " + addr + " test=0") or \
       "OK" not in hapd.request("DISASSOCIATE " + addr + " test=0"):
        raise Exception("Failed to send unprotected disconnection messages")
    ev = dev[0].wait_event(["CTRL-EVENT-DISCONNECTED"], timeout=1)
    if ev is not None:
        raise Exception("Unexpected disconnection")

    if "OK" not in hapd.request("DEAUTHENTICATE " + addr + " reason=6 test=0") or \
       "OK" not in hapd.request("DISASSOCIATE " + addr + " reason=7 test=0"):
        raise Exception("Failed to send unprotected disconnection messages (2)")
    ev = dev[0].wait_event(["CTRL-EVENT-DISCONNECTED"], timeout=1)
    if ev is not None:
        raise Exception("Unexpected disconnection")
    if wt.get_sta_counter("valid_saqueryreq_tx", bssid, addr) < 1:
        raise Exception("STA did not send SA Query")
    if wt.get_sta_counter("valid_saqueryresp_rx", bssid, addr) < 1:
        raise Exception("AP did not reply to SA Query")

def test_ap_pmf_sta_sa_query_no_response_hostapd(dev, apdev):
    """WPA2-PSK AP with station using SA Query and getting no response (hostapd)"""
    ssid = "assoc-comeback"
    passphrase = "12345678"
    addr = dev[0].own_addr()

    params = hostapd.wpa2_params(ssid=ssid, passphrase=passphrase,
                                 wpa_key_mgmt="WPA-PSK-SHA256",
                                 ieee80211w="2")
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = hapd.own_addr()

    Wlantest.setup(hapd)
    wt = Wlantest()
    wt.flush()
    wt.add_passphrase("12345678")

    dev[0].connect(ssid, psk=passphrase, ieee80211w="2",
                   key_mgmt="WPA-PSK-SHA256", proto="WPA2",
                   scan_freq="2412")
    hapd.wait_sta()
    hapd.set("ext_mgmt_frame_handling", "1")
    if "OK" not in hapd.request("DEAUTHENTICATE " + addr + " reason=6 test=0") or \
       "OK" not in hapd.request("DISASSOCIATE " + addr + " reason=7 test=0"):
        raise Exception("Failed to send unprotected disconnection messages")
    dev[0].wait_disconnected()
    hapd.set("ext_mgmt_frame_handling", "0")
    if wt.get_sta_counter("valid_saqueryreq_tx", bssid, addr) < 1:
        raise Exception("STA did not send SA Query")
    if wt.get_sta_counter("valid_saqueryresp_rx", bssid, addr) > 0:
        raise Exception("AP replied to SA Query")
    dev[0].wait_connected()

def test_ap_pmf_sta_unprot_deauth_burst_hostapd(dev, apdev):
    """WPA2-PSK AP with station receiving burst of unprotected Deauthentication frames (hostapd)"""
    ssid = "deauth-attack"
    passphrase = "12345678"
    addr = dev[0].own_addr()

    params = hostapd.wpa2_params(ssid=ssid, passphrase=passphrase,
                                 wpa_key_mgmt="WPA-PSK-SHA256",
                                 ieee80211w="2")
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = hapd.own_addr()

    Wlantest.setup(hapd)
    wt = Wlantest()
    wt.flush()
    wt.add_passphrase("12345678")

    dev[0].connect(ssid, psk=passphrase, ieee80211w="2",
                   key_mgmt="WPA-PSK-SHA256", proto="WPA2",
                   scan_freq="2412")
    hapd.wait_sta()
    for i in range(10):
        if "OK" not in hapd.request("DEAUTHENTICATE " + addr + " reason=6 test=0") or \
           "OK" not in hapd.request("DISASSOCIATE " + addr + " reason=7 test=0"):
            raise Exception("Failed to send unprotected disconnection messages")
    ev = dev[0].wait_event(["CTRL-EVENT-DISCONNECTED"], timeout=1)
    if ev is not None:
        raise Exception("Unexpected disconnection")
    num_req = wt.get_sta_counter("valid_saqueryreq_tx", bssid, addr)
    num_resp = wt.get_sta_counter("valid_saqueryresp_rx", bssid, addr)
    if num_req < 1:
        raise Exception("STA did not send SA Query")
    if num_resp < 1:
        raise Exception("AP did not reply to SA Query")
    if num_req > 1:
        raise Exception("STA initiated too many SA Query procedures (%d)" % num_req)

    time.sleep(10)
    for i in range(5):
        if "OK" not in hapd.request("DEAUTHENTICATE " + addr + " reason=6 test=0") or \
           "OK" not in hapd.request("DISASSOCIATE " + addr + " reason=7 test=0"):
            raise Exception("Failed to send unprotected disconnection messages")
    ev = dev[0].wait_event(["CTRL-EVENT-DISCONNECTED"], timeout=1)
    if ev is not None:
        raise Exception("Unexpected disconnection")
    num_req = wt.get_sta_counter("valid_saqueryreq_tx", bssid, addr)
    num_resp = wt.get_sta_counter("valid_saqueryresp_rx", bssid, addr)
    if num_req != 2 or num_resp != 2:
        raise Exception("Unexpected number of SA Query procedures (req=%d resp=%d)" % (num_req, num_resp))

def test_ap_pmf_required_eap(dev, apdev):
    """WPA2-EAP AP with PMF required"""
    ssid = "test-pmf-required-eap"
    params = hostapd.wpa2_eap_params(ssid=ssid)
    params["wpa_key_mgmt"] = "WPA-EAP-SHA256"
    params["ieee80211w"] = "2"
    hapd = hostapd.add_ap(apdev[0], params)
    key_mgmt = hapd.get_config()['key_mgmt']
    if key_mgmt.split(' ')[0] != "WPA-EAP-SHA256":
        raise Exception("Unexpected GET_CONFIG(key_mgmt): " + key_mgmt)
    dev[0].connect("test-pmf-required-eap", key_mgmt="WPA-EAP-SHA256",
                   ieee80211w="2", eap="PSK", identity="psk.user@example.com",
                   password_hex="0123456789abcdef0123456789abcdef",
                   scan_freq="2412")
    dev[1].connect("test-pmf-required-eap", key_mgmt="WPA-EAP WPA-EAP-SHA256",
                   ieee80211w="1", eap="PSK", identity="psk.user@example.com",
                   password_hex="0123456789abcdef0123456789abcdef",
                   scan_freq="2412")

def test_ap_pmf_optional_eap(dev, apdev):
    """WPA2EAP AP with PMF optional"""
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    params["ieee80211w"] = "1"
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TTLS",
                   identity="pap user", anonymous_identity="ttls",
                   password="password",
                   ca_cert="auth_serv/ca.pem", phase2="auth=PAP",
                   ieee80211w="1", scan_freq="2412")
    dev[1].connect("test-wpa2-eap", key_mgmt="WPA-EAP WPA-EAP-SHA256",
                   eap="TTLS", identity="pap user", anonymous_identity="ttls",
                   password="password",
                   ca_cert="auth_serv/ca.pem", phase2="auth=PAP",
                   ieee80211w="2", scan_freq="2412")

@remote_compatible
def test_ap_pmf_required_sha1(dev, apdev):
    """WPA2-PSK AP with PMF required with SHA1 AKM"""
    ssid = "test-pmf-required-sha1"
    params = hostapd.wpa2_params(ssid=ssid, passphrase="12345678")
    params["wpa_key_mgmt"] = "WPA-PSK"
    params["ieee80211w"] = "2"
    hapd = hostapd.add_ap(apdev[0], params)
    Wlantest.setup(hapd)
    wt = Wlantest()
    wt.flush()
    wt.add_passphrase("12345678")
    key_mgmt = hapd.get_config()['key_mgmt']
    if key_mgmt.split(' ')[0] != "WPA-PSK":
        raise Exception("Unexpected GET_CONFIG(key_mgmt): " + key_mgmt)
    dev[0].connect(ssid, psk="12345678", ieee80211w="2",
                   key_mgmt="WPA-PSK", proto="WPA2", scan_freq="2412")
    if "[WPA2-PSK-CCMP]" not in dev[0].request("SCAN_RESULTS"):
        raise Exception("Scan results missing RSN element info")
    hwsim_utils.test_connectivity(dev[0], hapd)

@remote_compatible
def test_ap_pmf_toggle(dev, apdev):
    """WPA2-PSK AP with PMF optional and changing PMF on reassociation"""
    try:
        _test_ap_pmf_toggle(dev, apdev)
    finally:
        dev[0].request("SET reassoc_same_bss_optim 0")

def _test_ap_pmf_toggle(dev, apdev):
    ssid = "test-pmf-optional"
    params = hostapd.wpa2_params(ssid=ssid, passphrase="12345678")
    params["wpa_key_mgmt"] = "WPA-PSK"
    params["ieee80211w"] = "1"
    params["assoc_sa_query_max_timeout"] = "1"
    params["assoc_sa_query_retry_timeout"] = "1"
    hapd = hostapd.add_ap(apdev[0], params)
    Wlantest.setup(hapd)
    wt = Wlantest()
    wt.flush()
    wt.add_passphrase("12345678")
    bssid = apdev[0]['bssid']
    addr = dev[0].own_addr()
    dev[0].request("SET reassoc_same_bss_optim 1")
    id = dev[0].connect(ssid, psk="12345678", ieee80211w="1",
                        key_mgmt="WPA-PSK WPA-PSK-SHA256", proto="WPA2",
                        scan_freq="2412")
    wt.require_ap_pmf_optional(bssid)
    wt.require_sta_pmf(bssid, addr)
    sta = hapd.get_sta(addr)
    if '[MFP]' not in sta['flags']:
        raise Exception("MFP flag not present for STA")

    dev[0].set_network(id, "ieee80211w", "0")
    dev[0].request("REASSOCIATE")
    dev[0].wait_connected()
    wt.require_sta_no_pmf(bssid, addr)
    sta = hapd.get_sta(addr)
    if '[MFP]' in sta['flags']:
        raise Exception("MFP flag unexpectedly present for STA")
    err, data = hapd.cmd_execute(['iw', 'dev', apdev[0]['ifname'], 'station',
                                  'get', addr])
    if "yes" in [l for l in data.splitlines() if "MFP" in l][0]:
        raise Exception("Kernel STA entry had MFP enabled")

    dev[0].set_network(id, "ieee80211w", "1")
    dev[0].request("REASSOCIATE")
    dev[0].wait_connected()
    wt.require_sta_pmf(bssid, addr)
    sta = hapd.get_sta(addr)
    if '[MFP]' not in sta['flags']:
        raise Exception("MFP flag not present for STA")
    err, data = hapd.cmd_execute(['iw', 'dev', apdev[0]['ifname'], 'station',
                                  'get', addr])
    if "yes" not in [l for l in data.splitlines() if "MFP" in l][0]:
        raise Exception("Kernel STA entry did not have MFP enabled")

@remote_compatible
def test_ap_pmf_required_sta_no_pmf(dev, apdev):
    """WPA2-PSK AP with PMF required and PMF disabled on STA"""
    ssid = "test-pmf-required"
    params = hostapd.wpa2_params(ssid=ssid, passphrase="12345678")
    params["wpa_key_mgmt"] = "WPA-PSK-SHA256"
    params["ieee80211w"] = "2"
    hapd = hostapd.add_ap(apdev[0], params)

    # Disable PMF on the station and try to connect
    dev[0].connect(ssid, psk="12345678", ieee80211w="0",
                   key_mgmt="WPA-PSK WPA-PSK-SHA256", proto="WPA2",
                   scan_freq="2412", wait_connect=False)
    ev = dev[0].wait_event(["CTRL-EVENT-NETWORK-NOT-FOUND",
                            "CTRL-EVENT-ASSOC-REJECT"], timeout=2)
    if ev is None:
        raise Exception("No connection result")
    if "CTRL-EVENT-ASSOC-REJECT" in ev:
        raise Exception("Tried to connect to PMF required AP without PMF enabled")
    dev[0].request("REMOVE_NETWORK all")

def test_ap_pmf_inject_auth(dev, apdev):
    """WPA2-PSK AP with PMF and Authentication frame injection"""
    ssid = "test-pmf"
    params = hostapd.wpa2_params(ssid=ssid, passphrase="12345678")
    params["wpa_key_mgmt"] = "WPA-PSK-SHA256"
    params["ieee80211w"] = "2"
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].connect(ssid, psk="12345678", ieee80211w="2",
                   key_mgmt="WPA-PSK-SHA256", proto="WPA2",
                   scan_freq="2412")
    hapd.wait_sta()
    hwsim_utils.test_connectivity(dev[0], hapd)

    bssid = hapd.own_addr().replace(':', '')
    addr = dev[0].own_addr().replace(':', '')

    # Inject an unprotected Authentication frame claiming to be from the
    # associated STA, from another STA, from the AP's own address, from all
    # zeros and all ones addresses, and from a multicast address.
    hapd.request("SET ext_mgmt_frame_handling 1")
    failed = False
    addresses = [ addr, "021122334455", bssid, 6*"00", 6*"ff", 6*"01" ]
    for a in addresses:
        auth = "b0003a01" + bssid + a + bssid + '1000000001000000'
        res = hapd.request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=%s" % auth)
        if "OK" not in res:
            failed = True
    hapd.request("SET ext_mgmt_frame_handling 0")
    if failed:
        raise Exception("MGMT_RX_PROCESS failed")
    time.sleep(0.1)

    ev = dev[0].wait_event(["CTRL-EVENT-DISCONNECTED"], timeout=0.1)
    if ev:
        raise Exception("Unexpected disconnection reported on the STA")

    # Verify that original association is still functional.
    hwsim_utils.test_connectivity(dev[0], hapd)

    # Inject an unprotected Association Request frame (with and without RSNE)
    # claiming to be from the set of test addresses.
    hapd.request("SET ext_mgmt_frame_handling 1")
    for a in addresses:
        assoc = "00003a01" + bssid + a + bssid + '2000' + '31040500' + '0008746573742d706d66' + '010802040b160c121824' + '301a0100000fac040100000fac040100000fac06c0000000000fac06'
        res = hapd.request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=%s" % assoc)
        if "OK" not in res:
            failed = True

        assoc = "00003a01" + bssid + a + bssid + '2000' + '31040500' + '0008746573742d706d66' + '010802040b160c121824' + '3000'
        res = hapd.request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=%s" % assoc)
        if "OK" not in res:
            failed = True

        assoc = "00003a01" + bssid + a + bssid + '2000' + '31040500' + '0008746573742d706d66' + '010802040b160c121824'
        res = hapd.request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=%s" % assoc)
        if "OK" not in res:
            failed = True
    hapd.request("SET ext_mgmt_frame_handling 0")
    if failed:
        raise Exception("MGMT_RX_PROCESS failed")
    time.sleep(5)

    ev = dev[0].wait_event(["CTRL-EVENT-DISCONNECTED"], timeout=0.1)
    if ev:
        raise Exception("Unexpected disconnection reported on the STA")

    # Verify that original association is still functional.
    hwsim_utils.test_connectivity(dev[0], hapd)

def test_ap_pmf_inject_data(dev, apdev):
    """WPA2-PSK AP with PMF and Data frame injection"""
    try:
        run_ap_pmf_inject_data(dev, apdev)
    finally:
        stop_monitor(apdev[1]["ifname"])

def run_ap_pmf_inject_data(dev, apdev):
    ssid = "test-pmf"
    params = hostapd.wpa2_params(ssid=ssid, passphrase="12345678")
    params["wpa_key_mgmt"] = "WPA-PSK-SHA256"
    params["ieee80211w"] = "2"
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].connect(ssid, psk="12345678", ieee80211w="2",
                   key_mgmt="WPA-PSK-SHA256", proto="WPA2",
                   scan_freq="2412")
    hapd.wait_sta()
    hwsim_utils.test_connectivity(dev[0], hapd)

    sock = start_monitor(apdev[1]["ifname"])
    radiotap = radiotap_build()

    bssid = hapd.own_addr().replace(':', '')
    addr = dev[0].own_addr().replace(':', '')

    # Inject Data frame with A2=broadcast, A2=multicast, A2=BSSID, A2=STA, and
    # A2=unknown unicast
    addresses = [ 6*"ff", 6*"01", bssid, addr, "020102030405" ]
    for a in addresses:
        frame = binascii.unhexlify("48010000" + bssid + a + bssid + "0000")
        sock.send(radiotap + frame)

    time.sleep(0.1)
    ev = dev[0].wait_event(["CTRL-EVENT-DISCONNECTED"], timeout=0.1)
    if ev:
        raise Exception("Unexpected disconnection reported on the STA")
    hwsim_utils.test_connectivity(dev[0], hapd)

def test_ap_pmf_tkip_reject(dev, apdev):
    """Mixed mode BSS and MFP-enabled AP rejecting TKIP"""
    skip_without_tkip(dev[0])
    params = hostapd.wpa2_params(ssid="test-pmf", passphrase="12345678")
    params['wpa'] = '3'
    params["ieee80211w"] = "1"
    params["wpa_pairwise"] = "TKIP CCMP"
    params["rsn_pairwise"] = "TKIP CCMP"
    hostapd.add_ap(apdev[0], params)

    dev[0].connect("test-pmf", psk="12345678", pairwise="CCMP", ieee80211w="2",
                   scan_freq="2412")
    dev[0].dump_monitor()

    dev[1].connect("test-pmf", psk="12345678", proto="WPA", pairwise="TKIP",
                   ieee80211w="0", scan_freq="2412")
    dev[1].dump_monitor()

    dev[2].connect("test-pmf", psk="12345678", pairwise="TKIP",
                   ieee80211w="2", scan_freq="2412", wait_connect=False)
    ev = dev[2].wait_event(["CTRL-EVENT-CONNECTED",
                            "CTRL-EVENT-ASSOC-REJECT"], timeout=10)
    if ev is None:
        raise Exception("No connection result reported")
    if "CTRL-EVENT-ASSOC-REJECT" not in ev:
        raise Exception("MFP + TKIP connection was not rejected")
    if "status_code=31" not in ev:
        raise Exception("Unexpected status code in rejection: " + ev)
    dev[2].request("DISCONNECT")
    dev[2].dump_monitor()

def test_ap_pmf_sa_query_timeout(dev, apdev):
    """SA Query timeout"""
    ssid = "test-pmf-required"
    params = hostapd.wpa2_params(ssid=ssid, passphrase="12345678")
    params["wpa_key_mgmt"] = "WPA-PSK-SHA256"
    params["ieee80211w"] = "2"
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].connect(ssid, psk="12345678", ieee80211w="2",
                   key_mgmt="WPA-PSK-SHA256", proto="WPA2",
                   scan_freq="2412")

    hapd.set("ext_mgmt_frame_handling", "1")
    if "OK" not in dev[0].request("UNPROT_DEAUTH"):
        raise Exception("Triggering SA Query from the STA failed")
    ev = dev[0].wait_event(["CTRL-EVENT-DISCONNECTED"], timeout=2)
    if ev is None:
        raise Exception("No disconnection on SA Query timeout seen")
    hapd.set("ext_mgmt_frame_handling", "0")
    dev[0].wait_connected()
    dev[0].dump_monitor()

    hapd.set("ext_mgmt_frame_handling", "1")
    if "OK" not in dev[0].request("UNPROT_DEAUTH"):
        raise Exception("Triggering SA Query from the STA failed")
    ev = hapd.mgmt_rx()
    hapd.set("ext_mgmt_frame_handling", "0")
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()
    dev[0].request("RECONNECT")
    dev[0].wait_connected()
    hapd.set("ext_mgmt_frame_handling", "1")
    ev = dev[0].wait_event(["CTRL-EVENT-DISCONNECTED"], timeout=1.5)
    if ev is not None:
        raise Exception("Unexpected disconnection after reconnection seen")

def mac80211_read_key(keydir):
    vals = {}
    for name in os.listdir(keydir):
        try:
            with open(os.path.join(keydir, name)) as f:
                vals[name] = f.read().strip()
        except OSError as e:
            pass
    return vals

def check_mac80211_bigtk(dev, hapd):
    sta_key = None
    ap_key = None

    phy = dev.get_driver_status_field("phyname")
    keys = "/sys/kernel/debug/ieee80211/%s/keys" % phy
    try:
        for key in os.listdir(keys):
            keydir = os.path.join(keys, key)
            vals = mac80211_read_key(keydir)
            keyidx = int(vals['keyidx'])
            if keyidx == 6 or keyidx == 7:
                sta_key = vals;
                break
    except OSError as e:
        raise HwsimSkip("debugfs not supported in mac80211 (STA)")

    phy = hapd.get_driver_status_field("phyname")
    keys = "/sys/kernel/debug/ieee80211/%s/keys" % phy
    try:
        for key in os.listdir(keys):
            keydir = os.path.join(keys, key)
            vals = mac80211_read_key(keydir)
            keyidx = int(vals['keyidx'])
            if keyidx == 6 or keyidx == 7:
                ap_key = vals;
                break
    except OSError as e:
        raise HwsimSkip("debugfs not supported in mac80211 (AP)")

    if not sta_key:
        raise Exception("Could not find STA key information from debugfs")
    logger.info("STA key: " + str(sta_key))

    if not ap_key:
        raise Exception("Could not find AP key information from debugfs")
    logger.info("AP key: " + str(ap_key))

    if sta_key['key'] != ap_key['key']:
        raise Exception("AP and STA BIGTK mismatch")

    if sta_key['keyidx'] != ap_key['keyidx']:
        raise Exception("AP and STA BIGTK keyidx mismatch")

    if sta_key['algorithm'] != ap_key['algorithm']:
        raise Exception("AP and STA BIGTK algorithm mismatch")

    replays = int(sta_key['replays'])
    icverrors = int(sta_key['icverrors'])
    if replays > 0 or icverrors > 0:
        raise Exception("STA reported errors: replays=%d icverrors=%d" % replays, icverrors)

    rx_spec = int(sta_key['rx_spec'], base=16)
    if rx_spec < 3:
        raise Exception("STA did not update BIGTK receive counter sufficiently")

    tx_spec = int(ap_key['tx_spec'], base=16)
    if tx_spec < 3:
        raise Exception("AP did not update BIGTK BIPN sufficiently")

def test_ap_pmf_beacon_protection_bip(dev, apdev):
    """WPA2-PSK Beacon protection (BIP)"""
    run_ap_pmf_beacon_protection(dev, apdev, "AES-128-CMAC")

def test_ap_pmf_beacon_protection_bip_cmac_256(dev, apdev):
    """WPA2-PSK Beacon protection (BIP-CMAC-256)"""
    run_ap_pmf_beacon_protection(dev, apdev, "BIP-CMAC-256")

def test_ap_pmf_beacon_protection_bip_gmac_128(dev, apdev):
    """WPA2-PSK Beacon protection (BIP-GMAC-128)"""
    run_ap_pmf_beacon_protection(dev, apdev, "BIP-GMAC-128")

def test_ap_pmf_beacon_protection_bip_gmac_256(dev, apdev):
    """WPA2-PSK Beacon protection (BIP-GMAC-256)"""
    run_ap_pmf_beacon_protection(dev, apdev, "BIP-GMAC-256")

def run_ap_pmf_beacon_protection(dev, apdev, cipher):
    ssid = "test-beacon-prot"
    params = hostapd.wpa2_params(ssid=ssid, passphrase="12345678")
    params["wpa_key_mgmt"] = "WPA-PSK-SHA256"
    params["ieee80211w"] = "2"
    params["beacon_prot"] = "1"
    params["group_mgmt_cipher"] = cipher
    try:
        hapd = hostapd.add_ap(apdev[0], params)
    except Exception as e:
        if "Failed to enable hostapd interface" in str(e):
            raise HwsimSkip("Beacon protection not supported")
        raise

    bssid = hapd.own_addr()

    Wlantest.setup(hapd)
    wt = Wlantest()
    wt.flush()
    wt.add_passphrase("12345678")

    # STA with Beacon protection enabled
    dev[0].connect(ssid, psk="12345678", ieee80211w="2", beacon_prot="1",
                   key_mgmt="WPA-PSK-SHA256", proto="WPA2", scan_freq="2412")

    # STA with Beacon protection disabled
    dev[1].connect(ssid, psk="12345678", ieee80211w="2",
                   key_mgmt="WPA-PSK-SHA256", proto="WPA2", scan_freq="2412")

    time.sleep(1)
    check_mac80211_bigtk(dev[0], hapd)

    valid_bip = wt.get_bss_counter('valid_bip_mmie', bssid)
    invalid_bip = wt.get_bss_counter('invalid_bip_mmie', bssid)
    missing_bip = wt.get_bss_counter('missing_bip_mmie', bssid)
    logger.info("wlantest BIP counters: valid=%d invalid=%d missing=%d" % (valid_bip, invalid_bip, missing_bip))
    if valid_bip < 0 or invalid_bip > 0 or missing_bip > 0:
        raise Exception("Unexpected wlantest BIP counters: valid=%d invalid=%d missing=%d" % (valid_bip, invalid_bip, missing_bip))

def test_ap_pmf_beacon_protection_mismatch(dev, apdev):
    """WPA2-PSK Beacon protection MIC mismatch"""
    run_ap_pmf_beacon_protection_mismatch(dev, apdev, False)

def test_ap_pmf_beacon_protection_missing(dev, apdev):
    """WPA2-PSK Beacon protection MME missing"""
    run_ap_pmf_beacon_protection_mismatch(dev, apdev, True)

def run_ap_pmf_beacon_protection_mismatch(dev, apdev, clear):
    ssid = "test-beacon-prot"
    params = hostapd.wpa2_params(ssid=ssid, passphrase="12345678")
    params["wpa_key_mgmt"] = "WPA-PSK-SHA256"
    params["ieee80211w"] = "2"
    params["beacon_prot"] = "1"
    params["group_mgmt_cipher"] = "AES-128-CMAC"
    try:
        hapd = hostapd.add_ap(apdev[0], params)
    except Exception as e:
        if "Failed to enable hostapd interface" in str(e):
            raise HwsimSkip("Beacon protection not supported")
        raise

    bssid = hapd.own_addr()

    Wlantest.setup(hapd)
    wt = Wlantest()
    wt.flush()
    wt.add_passphrase("12345678")

    dev[0].connect(ssid, psk="12345678", ieee80211w="2", beacon_prot="1",
                   key_mgmt="WPA-PSK-SHA256", proto="WPA2", scan_freq="2412")

    WPA_ALG_NONE = 0
    WPA_ALG_IGTK = 4
    KEY_FLAG_DEFAULT = 0x02
    KEY_FLAG_TX = 0x08
    KEY_FLAG_GROUP = 0x10
    KEY_FLAG_GROUP_TX_DEFAULT = KEY_FLAG_GROUP | KEY_FLAG_TX | KEY_FLAG_DEFAULT

    addr = "ff:ff:ff:ff:ff:ff"

    if clear:
        res = hapd.request("SET_KEY %d %s %d %d %s %s %d" % (WPA_ALG_NONE, addr, 6, 1, 6*"00", "", KEY_FLAG_GROUP))
    else:
        res = hapd.request("SET_KEY %d %s %d %d %s %s %d" % (WPA_ALG_IGTK, addr, 6, 1, 6*"00", 16*"00", KEY_FLAG_GROUP_TX_DEFAULT))
    if "OK" not in res:
        raise Exception("SET_KEY failed")

    ev = dev[0].wait_event(["CTRL-EVENT-UNPROT-BEACON"], timeout=5)
    if ev is None:
        raise Exception("Unprotected Beacon frame not reported")

    ev = dev[0].wait_event(["CTRL-EVENT-BEACON-LOSS"], timeout=5)
    if ev is None:
        raise Exception("Beacon loss not reported")

    ev = hapd.wait_event(["CTRL-EVENT-UNPROT-BEACON"], timeout=5)
    if ev is None:
        raise Exception("WNM-Notification Request frame not reported")

def test_ap_pmf_sta_global_require(dev, apdev):
    """WPA2-PSK AP with PMF optional and wpa_supplicant pmf=2"""
    ssid = "test-pmf-optional"
    params = hostapd.wpa2_params(ssid=ssid, passphrase="12345678")
    params["wpa_key_mgmt"] = "WPA-PSK"
    params["ieee80211w"] = "1"
    hapd = hostapd.add_ap(apdev[0], params)
    try:
        dev[0].set("pmf", "2")
        dev[0].connect(ssid, psk="12345678",
                       key_mgmt="WPA-PSK WPA-PSK-SHA256", proto="WPA2",
                       scan_freq="2412")
        pmf = dev[0].get_status_field("pmf")
        if pmf != "1":
            raise Exception("Unexpected PMF state: " + str(pmf))
    finally:
        dev[0].set("pmf", "0")

def test_ap_pmf_sta_global_require2(dev, apdev):
    """WPA2-PSK AP with PMF optional and wpa_supplicant pmf=2 (2)"""
    ssid = "test-pmf-optional"
    params = hostapd.wpa2_params(ssid=ssid, passphrase="12345678")
    params["wpa_key_mgmt"] = "WPA-PSK"
    params["ieee80211w"] = "0"
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = hapd.own_addr()
    try:
        dev[0].scan_for_bss(bssid, freq=2412)
        dev[0].set("pmf", "2")
        dev[0].connect(ssid, psk="12345678",
                       key_mgmt="WPA-PSK WPA-PSK-SHA256", proto="WPA2",
                       scan_freq="2412", wait_connect=False)
        ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED",
                                "CTRL-EVENT-NETWORK-NOT-FOUND"], timeout=10)
        if ev is None:
            raise Exception("Connection result not reported")
        if "CTRL-EVENT-CONNECTED" in ev:
            raise Exception("Unexpected connection")
    finally:
        dev[0].set("pmf", "0")
