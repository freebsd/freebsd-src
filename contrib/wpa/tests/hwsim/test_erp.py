# EAP Re-authentication Protocol (ERP) tests
# Copyright (c) 2014-2019, Jouni Malinen <j@w1.fi>
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

import binascii
import logging
logger = logging.getLogger()
import os
import time

import hostapd
from utils import *
from test_ap_eap import int_eap_server_params
from test_ap_psk import find_wpas_process, read_process_memory, verify_not_present, get_key_locations

def test_erp_initiate_reauth_start(dev, apdev):
    """Authenticator sending EAP-Initiate/Re-auth-Start, but ERP disabled on peer"""
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    params['erp_send_reauth_start'] = '1'
    params['erp_domain'] = 'example.com'
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].request("ERP_FLUSH")
    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP",
                   eap="PAX", identity="pax.user@example.com",
                   password_hex="0123456789abcdef0123456789abcdef",
                   scan_freq="2412")

def test_erp_enabled_on_server(dev, apdev):
    """ERP enabled on internal EAP server, but disabled on peer"""
    params = int_eap_server_params()
    params['erp_send_reauth_start'] = '1'
    params['erp_domain'] = 'example.com'
    params['eap_server_erp'] = '1'
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].request("ERP_FLUSH")
    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP",
                   eap="PAX", identity="pax.user@example.com",
                   password_hex="0123456789abcdef0123456789abcdef",
                   scan_freq="2412")

def test_erp(dev, apdev):
    """ERP enabled on server and peer"""
    check_erp_capa(dev[0])
    params = int_eap_server_params()
    params['erp_send_reauth_start'] = '1'
    params['erp_domain'] = 'example.com'
    params['eap_server_erp'] = '1'
    params['disable_pmksa_caching'] = '1'
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].request("ERP_FLUSH")
    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP",
                   eap="PSK", identity="psk.user@example.com",
                   password_hex="0123456789abcdef0123456789abcdef",
                   erp="1", scan_freq="2412")
    for i in range(3):
        dev[0].request("DISCONNECT")
        dev[0].wait_disconnected(timeout=15)
        dev[0].request("RECONNECT")
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-SUCCESS"], timeout=15)
        if ev is None:
            raise Exception("EAP success timed out")
        if "EAP re-authentication completed successfully" not in ev:
            raise Exception("Did not use ERP")
        dev[0].wait_connected(timeout=15, error="Reconnection timed out")

def test_erp_server_no_match(dev, apdev):
    """ERP enabled on server and peer, but server has no key match"""
    check_erp_capa(dev[0])
    params = int_eap_server_params()
    params['erp_send_reauth_start'] = '1'
    params['erp_domain'] = 'example.com'
    params['eap_server_erp'] = '1'
    params['disable_pmksa_caching'] = '1'
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].request("ERP_FLUSH")
    id = dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP",
                        eap="PSK", identity="psk.user@example.com",
                        password_hex="0123456789abcdef0123456789abcdef",
                        erp="1", scan_freq="2412")
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected(timeout=15)
    hapd.request("ERP_FLUSH")
    dev[0].request("RECONNECT")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-SUCCESS",
                            "CTRL-EVENT-EAP-FAILURE"], timeout=15)
    if ev is None:
        raise Exception("EAP result timed out")
    if "CTRL-EVENT-EAP-SUCCESS" in ev:
        raise Exception("Unexpected EAP success")
    dev[0].request("DISCONNECT")
    dev[0].select_network(id)
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-SUCCESS"], timeout=15)
    if ev is None:
        raise Exception("EAP success timed out")
    if "EAP re-authentication completed successfully" in ev:
        raise Exception("Unexpected use of ERP")
    dev[0].wait_connected(timeout=15, error="Reconnection timed out")

def start_erp_as(erp_domain="example.com", msk_dump=None, tls13=False,
                 eap_user_file="auth_serv/eap_user.conf"):
    params = {"driver": "none",
              "interface": "as-erp",
              "radius_server_clients": "auth_serv/radius_clients.conf",
              "radius_server_auth_port": '18128',
              "eap_server": "1",
              "eap_user_file": eap_user_file,
              "ca_cert": "auth_serv/ca.pem",
              "server_cert": "auth_serv/server.pem",
              "private_key": "auth_serv/server.key",
              "eap_sim_db": "unix:/tmp/hlr_auc_gw.sock",
              "dh_file": "auth_serv/dh.conf",
              "pac_opaque_encr_key": "000102030405060708090a0b0c0d0e0f",
              "eap_fast_a_id": "101112131415161718191a1b1c1d1e1f",
              "eap_fast_a_id_info": "test server",
              "eap_server_erp": "1",
              "erp_domain": erp_domain}
    if msk_dump:
        params["dump_msk_file"] = msk_dump
    if tls13:
        params["tls_flags"] = "[ENABLE-TLSv1.3]"
    apdev = {'ifname': 'as-erp'}
    return hostapd.add_ap(apdev, params, driver="none")

def test_erp_radius(dev, apdev):
    """ERP enabled on RADIUS server and peer"""
    check_erp_capa(dev[0])
    start_erp_as()
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    params['auth_server_port'] = "18128"
    params['erp_send_reauth_start'] = '1'
    params['erp_domain'] = 'example.com'
    params['disable_pmksa_caching'] = '1'
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].request("ERP_FLUSH")
    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP",
                   eap="PSK", identity="psk.user@example.com",
                   password_hex="0123456789abcdef0123456789abcdef",
                   erp="1", scan_freq="2412")
    for i in range(3):
        dev[0].request("DISCONNECT")
        dev[0].wait_disconnected(timeout=15)
        dev[0].request("RECONNECT")
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-SUCCESS"], timeout=15)
        if ev is None:
            raise Exception("EAP success timed out")
        if "EAP re-authentication completed successfully" not in ev:
            raise Exception("Did not use ERP")
        dev[0].wait_connected(timeout=15, error="Reconnection timed out")

def test_erp_radius_no_wildcard_user(dev, apdev, params):
    """ERP enabled on RADIUS server and peer and no wildcard user"""
    check_erp_capa(dev[0])
    user_file = os.path.join(params['logdir'],
                             'erp_radius_no_wildcard_user.eap_users')
    with open(user_file, 'w') as f:
        f.write('"user@example.com" PSK 0123456789abcdef0123456789abcdef\n')
    start_erp_as(eap_user_file=user_file)
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    params['auth_server_port'] = "18128"
    params['erp_send_reauth_start'] = '1'
    params['erp_domain'] = 'example.com'
    params['disable_pmksa_caching'] = '1'
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].request("ERP_FLUSH")
    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP",
                   eap="PSK", identity="user@example.com",
                   password_hex="0123456789abcdef0123456789abcdef",
                   erp="1", scan_freq="2412")
    for i in range(3):
        dev[0].request("DISCONNECT")
        dev[0].wait_disconnected(timeout=15)
        dev[0].request("RECONNECT")
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-SUCCESS"], timeout=15)
        if ev is None:
            raise Exception("EAP success timed out")
        if "EAP re-authentication completed successfully" not in ev:
            raise Exception("Did not use ERP")
        dev[0].wait_connected(timeout=15, error="Reconnection timed out")

def test_erp_radius_ext(dev, apdev):
    """ERP enabled on a separate RADIUS server and peer"""
    as_hapd = hostapd.Hostapd("as")
    try:
        as_hapd.disable()
        as_hapd.set("eap_server_erp", "1")
        as_hapd.set("erp_domain", "erp.example.com")
        as_hapd.enable()
        run_erp_radius_ext(dev, apdev)
    finally:
        as_hapd.disable()
        as_hapd.set("eap_server_erp", "0")
        as_hapd.set("erp_domain", "")
        as_hapd.enable()

def run_erp_radius_ext(dev, apdev):
    check_erp_capa(dev[0])
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    params['erp_send_reauth_start'] = '1'
    params['erp_domain'] = 'erp.example.com'
    params['disable_pmksa_caching'] = '1'
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].request("ERP_FLUSH")
    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP",
                   eap="PSK", identity="psk@erp.example.com",
                   password_hex="0123456789abcdef0123456789abcdef",
                   erp="1", scan_freq="2412")
    for i in range(3):
        dev[0].request("DISCONNECT")
        dev[0].wait_disconnected(timeout=15)
        dev[0].request("RECONNECT")
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-SUCCESS"], timeout=15)
        if ev is None:
            raise Exception("EAP success timed out")
        if "EAP re-authentication completed successfully" not in ev:
            raise Exception("Did not use ERP")
        dev[0].wait_connected(timeout=15, error="Reconnection timed out")

def erp_test(dev, hapd, reauth=False, **kwargs):
    res = dev.get_capability("eap")
    if kwargs['eap'] not in res:
        logger.info("Skip ERP test with %s due to missing support" % kwargs['eap'])
        return
    hapd.dump_monitor()
    dev.dump_monitor()
    dev.request("ERP_FLUSH")
    id = dev.connect("test-wpa2-eap", key_mgmt="WPA-EAP", erp="1",
                     scan_freq="2412", **kwargs)
    dev.request("DISCONNECT")
    dev.wait_disconnected(timeout=15)
    dev.dump_monitor()
    hapd.dump_monitor()

    if reauth:
        dev.request("ERP_FLUSH")
        dev.request("RECONNECT")
        ev = dev.wait_event(["CTRL-EVENT-EAP-SUCCESS"], timeout=15)
        if ev is None:
            raise Exception("EAP success timed out")
        if "EAP re-authentication completed successfully" in ev:
            raise Exception("Used ERP unexpectedly")
        dev.wait_connected(timeout=15, error="Reconnection timed out")
        dev.request("DISCONNECT")
        dev.wait_disconnected(timeout=15)
        dev.dump_monitor()
        hapd.dump_monitor()

    dev.request("RECONNECT")
    ev = dev.wait_event(["CTRL-EVENT-EAP-SUCCESS"], timeout=15)
    if ev is None:
        raise Exception("EAP success timed out")
    if "EAP re-authentication completed successfully" not in ev:
        raise Exception("Did not use ERP")
    dev.wait_connected(timeout=15, error="Reconnection timed out")
    ev = hapd.wait_event(["AP-STA-CONNECTED"], timeout=5)
    if ev is None:
        raise Exception("No connection event received from hostapd")
    dev.request("DISCONNECT")

def test_erp_radius_eap_methods(dev, apdev):
    """ERP enabled on RADIUS server and peer"""
    check_erp_capa(dev[0])
    eap_methods = dev[0].get_capability("eap")
    start_erp_as()
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    params['auth_server_port'] = "18128"
    params['erp_send_reauth_start'] = '1'
    params['erp_domain'] = 'example.com'
    params['disable_pmksa_caching'] = '1'
    hapd = hostapd.add_ap(apdev[0], params)

    erp_test(dev[0], hapd, eap="AKA", identity="0232010000000000@example.com",
             password="90dca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb82581:000000000123")
    erp_test(dev[0], hapd, reauth=True,
             eap="AKA", identity="0232010000000000@example.com",
             password="90dca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb82581:000000000123")
    erp_test(dev[0], hapd, eap="AKA'", identity="6555444333222111@example.com",
             password="5122250214c33e723a5dd523fc145fc0:981d464c7c52eb6e5036234984ad0bcf:000000000123")
    erp_test(dev[0], hapd, reauth=True,
             eap="AKA'", identity="6555444333222111@example.com",
             password="5122250214c33e723a5dd523fc145fc0:981d464c7c52eb6e5036234984ad0bcf:000000000123")
    erp_test(dev[0], hapd, eap="EKE", identity="erp-eke@example.com",
             password="hello")
    if "FAST" in eap_methods:
        erp_test(dev[0], hapd, eap="FAST", identity="erp-fast@example.com",
                 password="password", ca_cert="auth_serv/ca.pem",
                 phase2="auth=GTC",
                 phase1="fast_provisioning=2",
                 pac_file="blob://fast_pac_auth_erp")
    erp_test(dev[0], hapd, eap="GPSK", identity="erp-gpsk@example.com",
             password="abcdefghijklmnop0123456789abcdef")
    erp_test(dev[0], hapd, eap="IKEV2", identity="erp-ikev2@example.com",
             password="password")
    erp_test(dev[0], hapd, eap="PAX", identity="erp-pax@example.com",
             password_hex="0123456789abcdef0123456789abcdef")
    if "MSCHAPV2" in eap_methods:
        erp_test(dev[0], hapd, eap="PEAP", identity="erp-peap@example.com",
                 password="password", ca_cert="auth_serv/ca.pem",
                 phase2="auth=MSCHAPV2")
        erp_test(dev[0], hapd, eap="TEAP", identity="erp-teap@example.com",
                 password="password", ca_cert="auth_serv/ca.pem",
                 phase2="auth=MSCHAPV2", pac_file="blob://teap_pac")
    erp_test(dev[0], hapd, eap="PSK", identity="erp-psk@example.com",
             password_hex="0123456789abcdef0123456789abcdef")
    if "PWD" in eap_methods:
        erp_test(dev[0], hapd, eap="PWD", identity="erp-pwd@example.com",
                 password="secret password")
    erp_test(dev[0], hapd, eap="SAKE", identity="erp-sake@example.com",
             password_hex="0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef")
    erp_test(dev[0], hapd, eap="SIM", identity="1232010000000000@example.com",
             password="90dca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb82581")
    erp_test(dev[0], hapd, reauth=True,
             eap="SIM", identity="1232010000000000@example.com",
             password="90dca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb82581")
    erp_test(dev[0], hapd, eap="TLS", identity="erp-tls@example.com",
             ca_cert="auth_serv/ca.pem", client_cert="auth_serv/user.pem",
             private_key="auth_serv/user.key")
    erp_test(dev[0], hapd, eap="TTLS", identity="erp-ttls@example.com",
             password="password", ca_cert="auth_serv/ca.pem", phase2="auth=PAP")

def test_erp_radius_eap_tls_v13(dev, apdev):
    """ERP enabled on RADIUS server and peer using EAP-TLS v1.3"""
    check_erp_capa(dev[0])
    tls = dev[0].request("GET tls_library")
    if "run=OpenSSL 1.1.1" not in tls:
        raise HwsimSkip("No TLS v1.3 support in TLS library")

    eap_methods = dev[0].get_capability("eap")
    start_erp_as(tls13=True)
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    params['auth_server_port'] = "18128"
    params['erp_send_reauth_start'] = '1'
    params['erp_domain'] = 'example.com'
    params['disable_pmksa_caching'] = '1'
    hapd = hostapd.add_ap(apdev[0], params)

    erp_test(dev[0], hapd, eap="TLS", identity="erp-tls@example.com",
             ca_cert="auth_serv/ca.pem", client_cert="auth_serv/user.pem",
             private_key="auth_serv/user.key",
             phase1="tls_disable_tlsv1_0=1 tls_disable_tlsv1_1=1 tls_disable_tlsv1_2=1 tls_disable_tlsv1_3=0")

def test_erp_key_lifetime_in_memory(dev, apdev, params):
    """ERP and key lifetime in memory"""
    check_erp_capa(dev[0])
    p = int_eap_server_params()
    p['erp_send_reauth_start'] = '1'
    p['erp_domain'] = 'example.com'
    p['eap_server_erp'] = '1'
    p['disable_pmksa_caching'] = '1'
    hapd = hostapd.add_ap(apdev[0], p)
    password = "63d2d21ac3c09ed567ee004a34490f1d16e7fa5835edf17ddba70a63f1a90a25"

    pid = find_wpas_process(dev[0])

    dev[0].request("ERP_FLUSH")
    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TTLS",
                   identity="pap-secret@example.com", password=password,
                   ca_cert="auth_serv/ca.pem", phase2="auth=PAP",
                   erp="1", scan_freq="2412")

    # The decrypted copy of GTK is freed only after the CTRL-EVENT-CONNECTED
    # event has been delivered, so verify that wpa_supplicant has returned to
    # eloop before reading process memory.
    time.sleep(1)
    dev[0].ping()
    password = password.encode()
    buf = read_process_memory(pid, password)

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected(timeout=15)

    dev[0].relog()
    msk = None
    emsk = None
    rRK = None
    rIK = None
    pmk = None
    ptk = None
    gtk = None
    with open(os.path.join(params['logdir'], 'log0'), 'r') as f:
        for l in f.readlines():
            if "EAP-TTLS: Derived key - hexdump" in l:
                val = l.strip().split(':')[3].replace(' ', '')
                msk = binascii.unhexlify(val)
            if "EAP-TTLS: Derived EMSK - hexdump" in l:
                val = l.strip().split(':')[3].replace(' ', '')
                emsk = binascii.unhexlify(val)
            if "EAP: ERP rRK - hexdump" in l:
                val = l.strip().split(':')[3].replace(' ', '')
                rRK = binascii.unhexlify(val)
            if "EAP: ERP rIK - hexdump" in l:
                val = l.strip().split(':')[3].replace(' ', '')
                rIK = binascii.unhexlify(val)
            if "WPA: PMK - hexdump" in l:
                val = l.strip().split(':')[3].replace(' ', '')
                pmk = binascii.unhexlify(val)
            if "WPA: PTK - hexdump" in l:
                val = l.strip().split(':')[3].replace(' ', '')
                ptk = binascii.unhexlify(val)
            if "WPA: Group Key - hexdump" in l:
                val = l.strip().split(':')[3].replace(' ', '')
                gtk = binascii.unhexlify(val)
    if not msk or not emsk or not rIK or not rRK or not pmk or not ptk or not gtk:
        raise Exception("Could not find keys from debug log")
    if len(gtk) != 16:
        raise Exception("Unexpected GTK length")

    kck = ptk[0:16]
    kek = ptk[16:32]
    tk = ptk[32:48]

    fname = os.path.join(params['logdir'],
                         'erp_key_lifetime_in_memory.memctx-')

    logger.info("Checking keys in memory while associated")
    get_key_locations(buf, password, "Password")
    get_key_locations(buf, pmk, "PMK")
    get_key_locations(buf, msk, "MSK")
    get_key_locations(buf, emsk, "EMSK")
    get_key_locations(buf, rRK, "rRK")
    get_key_locations(buf, rIK, "rIK")
    if password not in buf:
        raise HwsimSkip("Password not found while associated")
    if pmk not in buf:
        raise HwsimSkip("PMK not found while associated")
    if kck not in buf:
        raise Exception("KCK not found while associated")
    if kek not in buf:
        raise Exception("KEK not found while associated")
    #if tk in buf:
    #    raise Exception("TK found from memory")

    logger.info("Checking keys in memory after disassociation")
    buf = read_process_memory(pid, password)

    # Note: Password is still present in network configuration
    # Note: PMK is in EAP fast re-auth data

    get_key_locations(buf, password, "Password")
    get_key_locations(buf, pmk, "PMK")
    get_key_locations(buf, msk, "MSK")
    get_key_locations(buf, emsk, "EMSK")
    get_key_locations(buf, rRK, "rRK")
    get_key_locations(buf, rIK, "rIK")
    verify_not_present(buf, kck, fname, "KCK")
    verify_not_present(buf, kek, fname, "KEK")
    verify_not_present(buf, tk, fname, "TK")
    if gtk in buf:
        get_key_locations(buf, gtk, "GTK")
    verify_not_present(buf, gtk, fname, "GTK")

    dev[0].request("RECONNECT")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-SUCCESS"], timeout=15)
    if ev is None:
        raise Exception("EAP success timed out")
    if "EAP re-authentication completed successfully" not in ev:
        raise Exception("Did not use ERP")
    dev[0].wait_connected(timeout=15, error="Reconnection timed out")

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected(timeout=15)

    dev[0].relog()
    pmk = None
    ptk = None
    gtk = None
    with open(os.path.join(params['logdir'], 'log0'), 'r') as f:
        for l in f.readlines():
            if "WPA: PMK - hexdump" in l:
                val = l.strip().split(':')[3].replace(' ', '')
                pmk = binascii.unhexlify(val)
            if "WPA: PTK - hexdump" in l:
                val = l.strip().split(':')[3].replace(' ', '')
                ptk = binascii.unhexlify(val)
            if "WPA: GTK in EAPOL-Key - hexdump" in l:
                val = l.strip().split(':')[3].replace(' ', '')
                gtk = binascii.unhexlify(val)
    if not pmk or not ptk or not gtk:
        raise Exception("Could not find keys from debug log")

    kck = ptk[0:16]
    kek = ptk[16:32]
    tk = ptk[32:48]

    logger.info("Checking keys in memory after ERP and disassociation")
    buf = read_process_memory(pid, password)

    # Note: Password is still present in network configuration

    get_key_locations(buf, password, "Password")
    get_key_locations(buf, pmk, "PMK")
    get_key_locations(buf, msk, "MSK")
    get_key_locations(buf, emsk, "EMSK")
    get_key_locations(buf, rRK, "rRK")
    get_key_locations(buf, rIK, "rIK")
    verify_not_present(buf, kck, fname, "KCK")
    verify_not_present(buf, kek, fname, "KEK")
    verify_not_present(buf, tk, fname, "TK")
    verify_not_present(buf, gtk, fname, "GTK")

    dev[0].request("REMOVE_NETWORK all")

    logger.info("Checking keys in memory after network profile removal")
    buf = read_process_memory(pid, password)

    # Note: rRK and rIK are still in memory

    get_key_locations(buf, password, "Password")
    get_key_locations(buf, pmk, "PMK")
    get_key_locations(buf, msk, "MSK")
    get_key_locations(buf, emsk, "EMSK")
    get_key_locations(buf, rRK, "rRK")
    get_key_locations(buf, rIK, "rIK")
    verify_not_present(buf, password, fname, "password")
    verify_not_present(buf, pmk, fname, "PMK")
    verify_not_present(buf, kck, fname, "KCK")
    verify_not_present(buf, kek, fname, "KEK")
    verify_not_present(buf, tk, fname, "TK")
    verify_not_present(buf, gtk, fname, "GTK")
    verify_not_present(buf, msk, fname, "MSK")
    verify_not_present(buf, emsk, fname, "EMSK")

    dev[0].request("ERP_FLUSH")
    logger.info("Checking keys in memory after ERP_FLUSH")
    buf = read_process_memory(pid, password)
    get_key_locations(buf, rRK, "rRK")
    get_key_locations(buf, rIK, "rIK")
    verify_not_present(buf, rRK, fname, "rRK")
    verify_not_present(buf, rIK, fname, "rIK")

def test_erp_anonymous_identity(dev, apdev):
    """ERP and anonymous identity"""
    check_erp_capa(dev[0])
    params = int_eap_server_params()
    params['erp_send_reauth_start'] = '1'
    params['erp_domain'] = 'example.com'
    params['eap_server_erp'] = '1'
    params['disable_pmksa_caching'] = '1'
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].request("ERP_FLUSH")
    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TTLS",
                   identity="erp-ttls",
                   anonymous_identity="anonymous@example.com",
                   password="password",
                   ca_cert="auth_serv/ca.pem", phase2="auth=PAP",
                   erp="1", scan_freq="2412")
    for i in range(3):
        dev[0].request("DISCONNECT")
        dev[0].wait_disconnected(timeout=15)
        dev[0].request("RECONNECT")
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-SUCCESS"], timeout=15)
        if ev is None:
            raise Exception("EAP success timed out")
        if "EAP re-authentication completed successfully" not in ev:
            raise Exception("Did not use ERP")
        dev[0].wait_connected(timeout=15, error="Reconnection timed out")

def test_erp_home_realm_oom(dev, apdev):
    """ERP and home realm OOM"""
    check_erp_capa(dev[0])
    params = int_eap_server_params()
    params['erp_send_reauth_start'] = '1'
    params['erp_domain'] = 'example.com'
    params['eap_server_erp'] = '1'
    params['disable_pmksa_caching'] = '1'
    hapd = hostapd.add_ap(apdev[0], params)

    for count in range(1, 3):
        with alloc_fail(dev[0], count, "eap_get_realm"):
            dev[0].request("ERP_FLUSH")
            dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TTLS",
                           identity="erp-ttls@example.com",
                           anonymous_identity="anonymous@example.com",
                           password="password",
                           ca_cert="auth_serv/ca.pem", phase2="auth=PAP",
                           erp="1", scan_freq="2412", wait_connect=False)
            dev[0].wait_connected(timeout=10)
            wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

    for count in range(1, 3):
        with alloc_fail(dev[0], count, "eap_get_realm"):
            dev[0].request("ERP_FLUSH")
            dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TTLS",
                           identity="erp-ttls",
                           anonymous_identity="anonymous@example.com",
                           password="password",
                           ca_cert="auth_serv/ca.pem", phase2="auth=PAP",
                           erp="1", scan_freq="2412", wait_connect=False)
            dev[0].wait_connected(timeout=10)
            wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

    for count in range(1, 3):
        dev[0].request("ERP_FLUSH")
        dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TTLS",
                       identity="erp-ttls@example.com",
                       anonymous_identity="anonymous@example.com",
                       password="password",
                       ca_cert="auth_serv/ca.pem", phase2="auth=PAP",
                       erp="1", scan_freq="2412", wait_connect=False)
        dev[0].wait_connected(timeout=10)
        if count > 1:
            continue
        with alloc_fail(dev[0], count, "eap_get_realm"):
            dev[0].request("DISCONNECT")
            dev[0].wait_disconnected(timeout=15)
            dev[0].request("RECONNECT")
            wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

def test_erp_local_errors(dev, apdev):
    """ERP and local error cases"""
    check_erp_capa(dev[0])
    params = int_eap_server_params()
    params['erp_send_reauth_start'] = '1'
    params['erp_domain'] = 'example.com'
    params['eap_server_erp'] = '1'
    params['disable_pmksa_caching'] = '1'
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].request("ERP_FLUSH")
    with alloc_fail(dev[0], 1, "eap_peer_erp_init"):
        dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TTLS",
                       identity="erp-ttls@example.com",
                       anonymous_identity="anonymous@example.com",
                       password="password",
                       ca_cert="auth_serv/ca.pem", phase2="auth=PAP",
                       erp="1", scan_freq="2412")
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()

    for count in range(1, 6):
        dev[0].request("ERP_FLUSH")
        with fail_test(dev[0], count, "hmac_sha256_kdf;eap_peer_erp_init"):
            dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TTLS",
                           identity="erp-ttls@example.com",
                           anonymous_identity="anonymous@example.com",
                           password="password",
                           ca_cert="auth_serv/ca.pem", phase2="auth=PAP",
                           erp="1", scan_freq="2412")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

    dev[0].request("ERP_FLUSH")
    with alloc_fail(dev[0], 1, "eap_msg_alloc;eap_peer_erp_reauth_start"):
        dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TTLS",
                       identity="erp-ttls@example.com",
                       anonymous_identity="anonymous@example.com",
                       password="password",
                       ca_cert="auth_serv/ca.pem", phase2="auth=PAP",
                       erp="1", scan_freq="2412")
        dev[0].request("DISCONNECT")
        dev[0].wait_disconnected(timeout=15)
        dev[0].request("RECONNECT")
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()

    dev[0].request("ERP_FLUSH")
    with fail_test(dev[0], 1, "hmac_sha256;eap_peer_erp_reauth_start"):
        dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TTLS",
                       identity="erp-ttls@example.com",
                       anonymous_identity="anonymous@example.com",
                       password="password",
                       ca_cert="auth_serv/ca.pem", phase2="auth=PAP",
                       erp="1", scan_freq="2412")
        dev[0].request("DISCONNECT")
        dev[0].wait_disconnected(timeout=15)
        dev[0].request("RECONNECT")
        wait_fail_trigger(dev[0], "GET_FAIL")
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()

    dev[0].request("ERP_FLUSH")
    with fail_test(dev[0], 1, "hmac_sha256;eap_peer_finish"):
        dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TTLS",
                       identity="erp-ttls@example.com",
                       anonymous_identity="anonymous@example.com",
                       password="password",
                       ca_cert="auth_serv/ca.pem", phase2="auth=PAP",
                       erp="1", scan_freq="2412")
        dev[0].request("DISCONNECT")
        dev[0].wait_disconnected(timeout=15)
        dev[0].request("RECONNECT")
        wait_fail_trigger(dev[0], "GET_FAIL")
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()

    dev[0].request("ERP_FLUSH")
    with alloc_fail(dev[0], 1, "eap_peer_erp_init"):
        dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TTLS",
                       identity="erp-ttls@example.com",
                       anonymous_identity="anonymous@example.com",
                       password="password",
                       ca_cert="auth_serv/ca.pem", phase2="auth=PAP",
                       erp="1", scan_freq="2412")
        dev[0].request("DISCONNECT")
        dev[0].wait_disconnected(timeout=15)

    dev[0].request("ERP_FLUSH")
    with alloc_fail(dev[0], 1, "eap_peer_finish"):
        dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TTLS",
                       identity="erp-ttls@example.com",
                       anonymous_identity="anonymous@example.com",
                       password="password",
                       ca_cert="auth_serv/ca.pem", phase2="auth=PAP",
                       erp="1", scan_freq="2412")
        dev[0].request("DISCONNECT")
        dev[0].wait_disconnected(timeout=15)
        dev[0].request("RECONNECT")
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()

    dev[0].request("ERP_FLUSH")
    with fail_test(dev[0], 1, "hmac_sha256_kdf;eap_peer_finish"):
        dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TTLS",
                       identity="erp-ttls@example.com",
                       anonymous_identity="anonymous@example.com",
                       password="password",
                       ca_cert="auth_serv/ca.pem", phase2="auth=PAP",
                       erp="1", scan_freq="2412")
        dev[0].request("DISCONNECT")
        dev[0].wait_disconnected(timeout=15)
        dev[0].request("RECONNECT")
        wait_fail_trigger(dev[0], "GET_FAIL")
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()
