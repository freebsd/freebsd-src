# hostapd authentication server tests
# Copyright (c) 2017, Jouni Malinen
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

import hostapd
from utils import alloc_fail, fail_test, wait_fail_trigger

def authsrv_params():
    params = {"ssid": "as", "beacon_int": "2000",
              "radius_server_clients": "auth_serv/radius_clients.conf",
              "radius_server_auth_port": '18128',
              "eap_server": "1",
              "eap_user_file": "auth_serv/eap_user.conf",
              "eap_sim_db": "unix:/tmp/hlr_auc_gw.sock",
              "ca_cert": "auth_serv/ca.pem",
              "server_cert": "auth_serv/server.pem",
              "private_key": "auth_serv/server.key",
              "eap_message": "hello"}
    return params

def test_authsrv_oom(dev, apdev):
    """Authentication server OOM"""
    params = authsrv_params()
    authsrv = hostapd.add_ap(apdev[1], params)

    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    params['auth_server_port'] = "18128"
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].scan_for_bss(hapd.own_addr(), 2412)
    with alloc_fail(authsrv, 1, "hostapd_radius_get_eap_user"):
        dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP",
                       eap="TTLS", identity="user",
                       anonymous_identity="ttls", password="password",
                       ca_cert="auth_serv/ca.pem", phase2="autheap=GTC",
                       wait_connect=False, scan_freq="2412")
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"], timeout=10)
        if ev is None:
            raise Exception("EAP failure not reported")
    dev[0].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected()
    dev[0].dump_monitor()

    with alloc_fail(authsrv, 1, "srv_log"):
        dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP",
                       eap="TTLS", identity="user",
                       anonymous_identity="ttls", password="password",
                       ca_cert="auth_serv/ca.pem", phase2="autheap=GTC",
                       scan_freq="2412")
    dev[0].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected()
    dev[0].dump_monitor()

    with alloc_fail(authsrv, 1, "radius_server_new_session"):
        dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP",
                       eap="TTLS", identity="user",
                       anonymous_identity="ttls", password="password",
                       ca_cert="auth_serv/ca.pem", phase2="autheap=GTC",
                       wait_connect=False, scan_freq="2412")
        dev[0].wait_disconnected()
    dev[0].request("REMOVE_NETWORK all")
    dev[0].dump_monitor()

    for count in range(1, 3):
        with alloc_fail(authsrv, count, "=radius_server_get_new_session"):
            dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP",
                           eap="TTLS", identity="user",
                           anonymous_identity="ttls", password="password",
                           ca_cert="auth_serv/ca.pem", phase2="autheap=GTC",
                           wait_connect=False, scan_freq="2412")
            dev[0].wait_disconnected()
        dev[0].request("REMOVE_NETWORK all")
        dev[0].dump_monitor()

    with alloc_fail(authsrv, 1, "eap_server_sm_init"):
        dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP",
                       eap="TTLS", identity="user",
                       anonymous_identity="ttls", password="password",
                       ca_cert="auth_serv/ca.pem", phase2="autheap=GTC",
                       wait_connect=False, scan_freq="2412")
        dev[0].wait_disconnected()
    dev[0].request("REMOVE_NETWORK all")
    dev[0].dump_monitor()

    tests = ["radius_server_encapsulate_eap",
             "radius_server_receive_auth"]
    for t in tests:
        with alloc_fail(authsrv, 1, t):
            dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP",
                           eap="TTLS", identity="user",
                           anonymous_identity="ttls", password="password",
                           ca_cert="auth_serv/ca.pem", phase2="autheap=GTC",
                           wait_connect=False, scan_freq="2412")
            wait_fail_trigger(authsrv, "GET_ALLOC_FAIL")
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()
        dev[0].dump_monitor()

    tests = ["radius_msg_add_attr;radius_server_encapsulate_eap",
             "radius_msg_add_eap;radius_server_encapsulate_eap",
             "radius_msg_finish_srv;radius_server_encapsulate_eap"]
    for t in tests:
        with fail_test(authsrv, 1, t):
            dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP",
                           eap="TTLS", identity="user",
                           anonymous_identity="ttls", password="password",
                           ca_cert="auth_serv/ca.pem", phase2="autheap=GTC",
                           wait_connect=False, scan_freq="2412")
            wait_fail_trigger(authsrv, "GET_FAIL")
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()
        dev[0].dump_monitor()

    with alloc_fail(authsrv, 1, "radius_server_get_new_session"):
        with fail_test(authsrv, 1, "radius_msg_add_eap;radius_server_reject"):
            dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP",
                           eap="TTLS", identity="user",
                           anonymous_identity="ttls", password="password",
                           ca_cert="auth_serv/ca.pem", phase2="autheap=GTC",
                           wait_connect=False, scan_freq="2412")
            wait_fail_trigger(authsrv, "GET_FAIL")
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()
        dev[0].dump_monitor()

    with alloc_fail(authsrv, 1, "radius_server_get_new_session"):
        with fail_test(authsrv, 1,
                       "radius_msg_finish_srv;radius_server_reject"):
            dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP",
                           eap="TTLS", identity="user",
                           anonymous_identity="ttls", password="password",
                           ca_cert="auth_serv/ca.pem", phase2="autheap=GTC",
                           wait_connect=False, scan_freq="2412")
            wait_fail_trigger(authsrv, "GET_FAIL")
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()
        dev[0].dump_monitor()

    authsrv.disable()
    with alloc_fail(authsrv, 1, "radius_server_init;hostapd_setup_radius_srv"):
        if "FAIL" not in authsrv.request("ENABLE"):
            raise Exception("ENABLE succeeded during OOM")
    with alloc_fail(authsrv, 2, "radius_server_init;hostapd_setup_radius_srv"):
        if "FAIL" not in authsrv.request("ENABLE"):
            raise Exception("ENABLE succeeded during OOM")

    for count in range(1, 4):
        with alloc_fail(authsrv, count,
                        "radius_server_read_clients;radius_server_init;hostapd_setup_radius_srv"):
            if "FAIL" not in authsrv.request("ENABLE"):
                raise Exception("ENABLE succeeded during OOM")

    with alloc_fail(authsrv, 1, "eloop_sock_table_add_sock;radius_server_init;hostapd_setup_radius_srv"):
        if "FAIL" not in authsrv.request("ENABLE"):
            raise Exception("ENABLE succeeded during OOM")

    with alloc_fail(authsrv, 1, "tls_init;authsrv_init"):
        if "FAIL" not in authsrv.request("ENABLE"):
            raise Exception("ENABLE succeeded during OOM")

    for count in range(1, 3):
        with alloc_fail(authsrv, count, "eap_sim_db_init;authsrv_init"):
            if "FAIL" not in authsrv.request("ENABLE"):
                raise Exception("ENABLE succeeded during OOM")

def test_authsrv_errors_1(dev, apdev):
    """Authentication server errors (1)"""
    params = authsrv_params()
    params["eap_user_file"] = "sqlite:auth_serv/does-not-exist/does-not-exist"
    authsrv = hostapd.add_ap(apdev[1], params, no_enable=True)
    if "FAIL" not in authsrv.request("ENABLE"):
        raise Exception("ENABLE succeeded with invalid SQLite EAP user file")

def test_authsrv_errors_2(dev, apdev):
    """Authentication server errors (2)"""
    params = authsrv_params()
    params["radius_server_clients"] = "auth_serv/does-not-exist"
    authsrv = hostapd.add_ap(apdev[1], params, no_enable=True)
    if "FAIL" not in authsrv.request("ENABLE"):
        raise Exception("ENABLE succeeded with invalid RADIUS client file")

def test_authsrv_errors_3(dev, apdev):
    """Authentication server errors (3)"""
    params = authsrv_params()
    params["eap_sim_db"] = "unix:/tmp/hlr_auc_gw.sock db=auth_serv/does-not-exist/does-not-exist"
    authsrv = hostapd.add_ap(apdev[1], params, no_enable=True)
    if "FAIL" not in authsrv.request("ENABLE"):
        raise Exception("ENABLE succeeded with invalid RADIUS client file")

def test_authsrv_testing_options(dev, apdev):
    """Authentication server and testing options"""
    params = authsrv_params()
    authsrv = hostapd.add_ap(apdev[1], params)

    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    params['auth_server_port'] = "18128"
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].scan_for_bss(hapd.own_addr(), 2412)
    # The first two would be fine to run with any server build; the rest are
    # actually supposed to fail, but they don't fail when using a server build
    # that does not support the TLS protocol tests.
    tests = ["foo@test-unknown",
             "foo@test-tls-unknown",
             "foo@test-tls-1",
             "foo@test-tls-2",
             "foo@test-tls-3",
             "foo@test-tls-4",
             "foo@test-tls-5",
             "foo@test-tls-6",
             "foo@test-tls-7",
             "foo@test-tls-8"]
    for t in tests:
        dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP",
                       eap="TTLS", identity="user",
                       anonymous_identity=t,
                       password="password",
                       ca_cert="auth_serv/ca.pem", phase2="autheap=GTC",
                       scan_freq="2412")
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()

def test_authsrv_unknown_user(dev, apdev):
    """Authentication server and unknown user"""
    params = authsrv_params()
    params["eap_user_file"] = "auth_serv/eap_user_vlan.conf"
    authsrv = hostapd.add_ap(apdev[1], params)

    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    params['auth_server_port'] = "18128"
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP",
                   eap="TTLS", identity="user",
                   anonymous_identity="ttls", password="password",
                   ca_cert="auth_serv/ca.pem", phase2="autheap=GTC",
                   wait_connect=False, scan_freq="2412")
    dev[0].wait_disconnected()
    dev[0].request("REMOVE_NETWORK all")

def test_authsrv_unknown_client(dev, apdev):
    """Authentication server and unknown user"""
    params = authsrv_params()
    params["radius_server_clients"] = "auth_serv/radius_clients_none.conf"
    authsrv = hostapd.add_ap(apdev[1], params)

    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    params['auth_server_port'] = "18128"
    hapd = hostapd.add_ap(apdev[0], params)

    # RADIUS SRV: Unknown client 127.0.0.1 - packet ignored
    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP",
                   eap="TTLS", identity="user",
                   anonymous_identity="ttls", password="password",
                   ca_cert="auth_serv/ca.pem", phase2="autheap=GTC",
                   wait_connect=False, scan_freq="2412")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED"], timeout=10)
    if ev is None:
        raise Exception("EAP not started")
    dev[0].request("REMOVE_NETWORK all")
