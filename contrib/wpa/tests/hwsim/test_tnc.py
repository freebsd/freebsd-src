# -*- coding: utf-8 -*-
# TNC tests
# Copyright (c) 2014-2015, Jouni Malinen <j@w1.fi>
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

import os.path

import hostapd
from utils import HwsimSkip, alloc_fail, fail_test, wait_fail_trigger
from test_ap_eap import int_eap_server_params, check_eap_capa

def test_tnc_peap_soh(dev, apdev):
    """TNC PEAP-SoH"""
    params = int_eap_server_params()
    params["tnc"] = "1"
    hostapd.add_ap(apdev[0], params)

    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP",
                   eap="PEAP", identity="user", password="password",
                   ca_cert="auth_serv/ca.pem",
                   phase1="peapver=0 tnc=soh cryptobinding=0",
                   phase2="auth=MSCHAPV2",
                   scan_freq="2412", wait_connect=False)
    dev[0].wait_connected(timeout=10)

    dev[1].connect("test-wpa2-eap", key_mgmt="WPA-EAP",
                   eap="PEAP", identity="user", password="password",
                   ca_cert="auth_serv/ca.pem",
                   phase1="peapver=0 tnc=soh1 cryptobinding=1",
                   phase2="auth=MSCHAPV2",
                   scan_freq="2412", wait_connect=False)
    dev[1].wait_connected(timeout=10)

    dev[2].connect("test-wpa2-eap", key_mgmt="WPA-EAP",
                   eap="PEAP", identity="user", password="password",
                   ca_cert="auth_serv/ca.pem",
                   phase1="peapver=0 tnc=soh2 cryptobinding=2",
                   phase2="auth=MSCHAPV2",
                   scan_freq="2412", wait_connect=False)
    dev[2].wait_connected(timeout=10)

def test_tnc_peap_soh_errors(dev, apdev):
    """TNC PEAP-SoH local error cases"""
    params = int_eap_server_params()
    params["tnc"] = "1"
    hostapd.add_ap(apdev[0], params)

    tests = [(1, "tncc_build_soh"),
             (1, "eap_msg_alloc;=eap_peap_phase2_request")]
    for count, func in tests:
        with alloc_fail(dev[0], count, func):
            dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP",
                           eap="PEAP", identity="user", password="password",
                           ca_cert="auth_serv/ca.pem",
                           phase1="peapver=0 tnc=soh cryptobinding=0",
                           phase2="auth=MSCHAPV2",
                           scan_freq="2412", wait_connect=False)
            wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

    with fail_test(dev[0], 1, "os_get_random;tncc_build_soh"):
        dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP",
                       eap="PEAP", identity="user", password="password",
                       ca_cert="auth_serv/ca.pem",
                       phase1="peapver=0 tnc=soh cryptobinding=0",
                       phase2="auth=MSCHAPV2",
                       scan_freq="2412", wait_connect=False)
        wait_fail_trigger(dev[0], "GET_FAIL")
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()

def test_tnc_ttls(dev, apdev):
    """TNC TTLS"""
    check_eap_capa(dev[0], "MSCHAPV2")
    params = int_eap_server_params()
    params["tnc"] = "1"
    hostapd.add_ap(apdev[0], params)

    if not os.path.exists("tnc/libhostap_imc.so"):
        raise HwsimSkip("No IMC installed")

    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP",
                   eap="TTLS", identity="DOMAIN\mschapv2 user",
                   anonymous_identity="ttls", password="password",
                   phase2="auth=MSCHAPV2",
                   ca_cert="auth_serv/ca.pem",
                   scan_freq="2412", wait_connect=False)
    dev[0].wait_connected(timeout=10)

def test_tnc_ttls_fragmentation(dev, apdev):
    """TNC TTLS with fragmentation"""
    check_eap_capa(dev[0], "MSCHAPV2")
    params = int_eap_server_params()
    params["tnc"] = "1"
    params["fragment_size"] = "150"
    hostapd.add_ap(apdev[0], params)

    if not os.path.exists("tnc/libhostap_imc.so"):
        raise HwsimSkip("No IMC installed")

    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP",
                   eap="TTLS", identity="DOMAIN\mschapv2 user",
                   anonymous_identity="ttls", password="password",
                   phase2="auth=MSCHAPV2",
                   ca_cert="auth_serv/ca.pem",
                   fragment_size="150",
                   scan_freq="2412", wait_connect=False)
    dev[0].wait_connected(timeout=10)

def test_tnc_ttls_errors(dev, apdev):
    """TNC TTLS local error cases"""
    if not os.path.exists("tnc/libhostap_imc.so"):
        raise HwsimSkip("No IMC installed")
    check_eap_capa(dev[0], "MSCHAPV2")

    params = int_eap_server_params()
    params["tnc"] = "1"
    params["fragment_size"] = "150"
    hostapd.add_ap(apdev[0], params)

    tests = [(1, "eap_ttls_process_phase2_eap;eap_ttls_process_tnc_start",
              "DOMAIN\mschapv2 user", "auth=MSCHAPV2"),
             (1, "eap_ttls_process_phase2_eap;eap_ttls_process_tnc_start",
              "mschap user", "auth=MSCHAP"),
             (1, "=eap_tnc_init", "chap user", "auth=CHAP"),
             (1, "tncc_init;eap_tnc_init", "pap user", "auth=PAP"),
             (1, "eap_msg_alloc;eap_tnc_build_frag_ack",
              "pap user", "auth=PAP"),
             (1, "eap_msg_alloc;eap_tnc_build_msg",
              "pap user", "auth=PAP"),
             (1, "wpabuf_alloc;=eap_tnc_process_fragment",
              "pap user", "auth=PAP"),
             (1, "eap_msg_alloc;=eap_tnc_process", "pap user", "auth=PAP"),
             (1, "wpabuf_alloc;=eap_tnc_process", "pap user", "auth=PAP"),
             (1, "dup_binstr;tncc_process_if_tnccs", "pap user", "auth=PAP"),
             (1, "tncc_get_base64;tncc_process_if_tnccs",
              "pap user", "auth=PAP"),
             (1, "tncc_if_tnccs_start", "pap user", "auth=PAP"),
             (1, "tncc_if_tnccs_end", "pap user", "auth=PAP"),
             (1, "tncc_parse_imc", "pap user", "auth=PAP"),
             (2, "tncc_parse_imc", "pap user", "auth=PAP"),
             (3, "tncc_parse_imc", "pap user", "auth=PAP"),
             (1, "os_readfile;tncc_read_config", "pap user", "auth=PAP"),
             (1, "tncc_init", "pap user", "auth=PAP"),
             (1, "TNC_TNCC_ReportMessageTypes", "pap user", "auth=PAP"),
             (1, "base64_gen_encode;?base64_encode;TNC_TNCC_SendMessage",
              "pap user", "auth=PAP"),
             (1, "=TNC_TNCC_SendMessage", "pap user", "auth=PAP"),
             (1, "tncc_get_base64;tncc_process_if_tnccs",
              "pap user", "auth=PAP")]
    for count, func, identity, phase2 in tests:
        with alloc_fail(dev[0], count, func):
            dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP",
                           scan_freq="2412",
                           eap="TTLS", anonymous_identity="ttls",
                           identity=identity, password="password",
                           ca_cert="auth_serv/ca.pem", phase2=phase2,
                           fragment_size="150", wait_connect=False)
            ev = dev[0].wait_event(["CTRL-EVENT-EAP-PROPOSED-METHOD"],
                                   timeout=15)
            if ev is None:
                raise Exception("Timeout on EAP start")
            wait_fail_trigger(dev[0], "GET_ALLOC_FAIL",
                              note="Allocation failure not triggered for: %d:%s" % (count, func))
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()
            dev[0].dump_monitor()

def test_tnc_fast(dev, apdev):
    """TNC FAST"""
    check_eap_capa(dev[0], "FAST")
    params = int_eap_server_params()
    params["tnc"] = "1"
    params["pac_opaque_encr_key"] = "000102030405060708090a0b0c0d0e00"
    params["eap_fast_a_id"] = "101112131415161718191a1b1c1d1e00"
    params["eap_fast_a_id_info"] = "test server2"

    hostapd.add_ap(apdev[0], params)

    if not os.path.exists("tnc/libhostap_imc.so"):
        raise HwsimSkip("No IMC installed")

    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP",
                   eap="FAST", identity="user",
                   anonymous_identity="FAST", password="password",
                   phase2="auth=GTC",
                   phase1="fast_provisioning=2",
                   pac_file="blob://fast_pac_auth_tnc",
                   ca_cert="auth_serv/ca.pem",
                   scan_freq="2412", wait_connect=False)
    dev[0].wait_connected(timeout=10)
