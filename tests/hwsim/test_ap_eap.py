# -*- coding: utf-8 -*-
# WPA2-Enterprise tests
# Copyright (c) 2013-2019, Jouni Malinen <j@w1.fi>
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

import base64
import binascii
import time
import subprocess
import logging
logger = logging.getLogger()
import os
import signal
import socket
try:
    import SocketServer
except ImportError:
    import socketserver as SocketServer
import struct
import tempfile

import hwsim_utils
from hwsim import HWSimRadio
import hostapd
from utils import *
from wpasupplicant import WpaSupplicant
from test_ap_psk import check_mib, find_wpas_process, read_process_memory, verify_not_present, get_key_locations, set_test_assoc_ie

try:
    import OpenSSL
    openssl_imported = True
except ImportError:
    openssl_imported = False

def check_hlr_auc_gw_support():
    if not os.path.exists("/tmp/hlr_auc_gw.sock"):
        raise HwsimSkip("No hlr_auc_gw available")

def check_eap_capa(dev, method):
    res = dev.get_capability("eap")
    if method not in res:
        raise HwsimSkip("EAP method %s not supported in the build" % method)

def check_subject_match_support(dev):
    tls = dev.request("GET tls_library")
    if not tls.startswith("OpenSSL") and not tls.startswith("wolfSSL"):
        raise HwsimSkip("subject_match not supported with this TLS library: " + tls)

def check_check_cert_subject_support(dev):
    tls = dev.request("GET tls_library")
    if not tls.startswith("OpenSSL"):
        raise HwsimSkip("check_cert_subject not supported with this TLS library: " + tls)

def check_altsubject_match_support(dev):
    tls = dev.request("GET tls_library")
    if not tls.startswith("OpenSSL") and not tls.startswith("wolfSSL"):
        raise HwsimSkip("altsubject_match not supported with this TLS library: " + tls)

def check_domain_match(dev):
    tls = dev.request("GET tls_library")
    if tls.startswith("internal"):
        raise HwsimSkip("domain_match not supported with this TLS library: " + tls)

def check_domain_suffix_match(dev):
    tls = dev.request("GET tls_library")
    if tls.startswith("internal"):
        raise HwsimSkip("domain_suffix_match not supported with this TLS library: " + tls)

def check_domain_match_full(dev):
    tls = dev.request("GET tls_library")
    if not tls.startswith("OpenSSL") and not tls.startswith("wolfSSL"):
        raise HwsimSkip("domain_suffix_match requires full match with this TLS library: " + tls)

def check_cert_probe_support(dev):
    tls = dev.request("GET tls_library")
    if not tls.startswith("OpenSSL") and not tls.startswith("internal"):
        raise HwsimSkip("Certificate probing not supported with this TLS library: " + tls)

def check_ext_cert_check_support(dev):
    tls = dev.request("GET tls_library")
    if not tls.startswith("OpenSSL"):
        raise HwsimSkip("ext_cert_check not supported with this TLS library: " + tls)

def check_ocsp_support(dev):
    tls = dev.request("GET tls_library")
    #if tls.startswith("internal"):
    #    raise HwsimSkip("OCSP not supported with this TLS library: " + tls)
    #if "BoringSSL" in tls:
    #    raise HwsimSkip("OCSP not supported with this TLS library: " + tls)
    if tls.startswith("wolfSSL"):
        raise HwsimSkip("OCSP not supported with this TLS library: " + tls)

def check_pkcs5_v15_support(dev):
    tls = dev.request("GET tls_library")
    if "BoringSSL" in tls or "GnuTLS" in tls:
        raise HwsimSkip("PKCS#5 v1.5 not supported with this TLS library: " + tls)

def check_ocsp_multi_support(dev):
    tls = dev.request("GET tls_library")
    if not tls.startswith("internal"):
        raise HwsimSkip("OCSP-multi not supported with this TLS library: " + tls)
    as_hapd = hostapd.Hostapd("as")
    res = as_hapd.request("GET tls_library")
    del as_hapd
    if not res.startswith("internal"):
        raise HwsimSkip("Authentication server does not support ocsp_multi")

def check_pkcs12_support(dev):
    tls = dev.request("GET tls_library")
    #if tls.startswith("internal"):
    #    raise HwsimSkip("PKCS#12 not supported with this TLS library: " + tls)
    if tls.startswith("wolfSSL"):
        raise HwsimSkip("PKCS#12 not supported with this TLS library: " + tls)

def check_dh_dsa_support(dev):
    tls = dev.request("GET tls_library")
    if tls.startswith("internal"):
        raise HwsimSkip("DH DSA not supported with this TLS library: " + tls)

def check_ec_support(dev):
    tls = dev.request("GET tls_library")
    if tls.startswith("internal"):
        raise HwsimSkip("EC not supported with this TLS library: " + tls)

def read_pem(fname, decode=True):
    with open(fname, "r") as f:
        lines = f.readlines()
        copy = False
        cert = ""
        for l in lines:
            if "-----END" in l:
                if not decode:
                    cert = cert + l
                break
            if copy:
                cert = cert + l
            if "-----BEGIN" in l:
                copy = True
                if not decode:
                    cert = cert + l
    if decode:
        return base64.b64decode(cert)
    return cert.encode()

def eap_connect(dev, hapd, method, identity,
                sha256=False, expect_failure=False, local_error_report=False,
                maybe_local_error=False, report_failure=False,
                expect_cert_error=None, **kwargs):
    id = dev.connect("test-wpa2-eap", key_mgmt="WPA-EAP WPA-EAP-SHA256",
                     eap=method, identity=identity,
                     wait_connect=False, scan_freq="2412", ieee80211w="1",
                     **kwargs)
    eap_check_auth(dev, method, True, sha256=sha256,
                   expect_failure=expect_failure,
                   local_error_report=local_error_report,
                   maybe_local_error=maybe_local_error,
                   report_failure=report_failure,
                   expect_cert_error=expect_cert_error)
    if expect_failure:
        return id
    if hapd:
        ev = hapd.wait_event(["AP-STA-CONNECTED"], timeout=5)
        if ev is None:
            raise Exception("No connection event received from hostapd")
    return id

def eap_check_auth(dev, method, initial, rsn=True, sha256=False,
                   expect_failure=False, local_error_report=False,
                   maybe_local_error=False, report_failure=False,
                   expect_cert_error=None):
    ev = dev.wait_event(["CTRL-EVENT-EAP-STARTED"], timeout=16)
    if ev is None:
        raise Exception("Association and EAP start timed out")
    ev = dev.wait_event(["CTRL-EVENT-EAP-METHOD",
                         "CTRL-EVENT-EAP-FAILURE"], timeout=10)
    if ev is None:
        raise Exception("EAP method selection timed out")
    if "CTRL-EVENT-EAP-FAILURE" in ev:
        if maybe_local_error:
            return
        raise Exception("Could not select EAP method")
    if method not in ev:
        raise Exception("Unexpected EAP method")
    if expect_cert_error is not None:
        ev = dev.wait_event(["CTRL-EVENT-EAP-TLS-CERT-ERROR",
                             "CTRL-EVENT-EAP-FAILURE",
                             "CTRL-EVENT-EAP-SUCCESS"], timeout=5)
        if ev is None or "reason=%d " % expect_cert_error not in ev:
            raise Exception("Expected certificate error not reported")
    if expect_failure:
        ev = dev.wait_event(["CTRL-EVENT-EAP-FAILURE",
                             "CTRL-EVENT-EAP-SUCCESS"], timeout=5)
        if ev is None:
            raise Exception("EAP failure timed out")
        if "CTRL-EVENT-EAP-SUCCESS" in ev:
            raise Exception("Unexpected EAP success")
        ev = dev.wait_disconnected(timeout=10)
        if maybe_local_error and "locally_generated=1" in ev:
            return
        if not local_error_report:
            if "reason=23" not in ev:
                raise Exception("Proper reason code for disconnection not reported")
        return
    if report_failure:
        ev = dev.wait_event(["CTRL-EVENT-EAP-SUCCESS",
                             "CTRL-EVENT-EAP-FAILURE"], timeout=10)
        if ev is None:
            raise Exception("EAP success timed out")
        if "CTRL-EVENT-EAP-SUCCESS" not in ev:
            raise Exception("EAP failed")
    else:
        ev = dev.wait_event(["CTRL-EVENT-EAP-SUCCESS"], timeout=10)
        if ev is None:
            raise Exception("EAP success timed out")

    if initial:
        ev = dev.wait_event(["CTRL-EVENT-CONNECTED"], timeout=10)
    else:
        ev = dev.wait_event(["WPA: Key negotiation completed"], timeout=10)
    if ev is None:
        raise Exception("Association with the AP timed out")
    status = dev.get_status()
    if status["wpa_state"] != "COMPLETED":
        raise Exception("Connection not completed")

    if status["suppPortStatus"] != "Authorized":
        raise Exception("Port not authorized")
    if "selectedMethod" not in status:
        logger.info("Status: " + str(status))
        raise Exception("No selectedMethod in status")
    if method not in status["selectedMethod"]:
        raise Exception("Incorrect EAP method status")
    if sha256:
        e = "WPA2-EAP-SHA256"
    elif rsn:
        e = "WPA2/IEEE 802.1X/EAP"
    else:
        e = "WPA/IEEE 802.1X/EAP"
    if status["key_mgmt"] != e:
        raise Exception("Unexpected key_mgmt status: " + status["key_mgmt"])
    return status

def eap_reauth(dev, method, rsn=True, sha256=False, expect_failure=False):
    dev.request("REAUTHENTICATE")
    return eap_check_auth(dev, method, False, rsn=rsn, sha256=sha256,
                          expect_failure=expect_failure)

def test_ap_wpa2_eap_sim(dev, apdev):
    """WPA2-Enterprise connection using EAP-SIM"""
    check_hlr_auc_gw_support()
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "SIM", "1232010000000000",
                password="90dca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb82581")
    hwsim_utils.test_connectivity(dev[0], hapd)
    eap_reauth(dev[0], "SIM")

    eap_connect(dev[1], hapd, "SIM", "1232010000000001",
                password="90dca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb82581")
    eap_connect(dev[2], hapd, "SIM", "1232010000000002",
                password="90dca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb82581",
                expect_failure=True)

    logger.info("Negative test with incorrect key")
    dev[0].request("REMOVE_NETWORK all")
    eap_connect(dev[0], hapd, "SIM", "1232010000000000",
                password="ffdca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb82581",
                expect_failure=True)

    logger.info("Invalid GSM-Milenage key")
    dev[0].request("REMOVE_NETWORK all")
    eap_connect(dev[0], hapd, "SIM", "1232010000000000",
                password="ffdca4eda45b53cf0f12d7c9c3bc6a",
                expect_failure=True)

    logger.info("Invalid GSM-Milenage key(2)")
    dev[0].request("REMOVE_NETWORK all")
    eap_connect(dev[0], hapd, "SIM", "1232010000000000",
                password="ffdca4eda45b53cf0f12d7c9c3bc6a8q:cb9cccc4b9258e6dca4760379fb82581",
                expect_failure=True)

    logger.info("Invalid GSM-Milenage key(3)")
    dev[0].request("REMOVE_NETWORK all")
    eap_connect(dev[0], hapd, "SIM", "1232010000000000",
                password="ffdca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb8258q",
                expect_failure=True)

    logger.info("Invalid GSM-Milenage key(4)")
    dev[0].request("REMOVE_NETWORK all")
    eap_connect(dev[0], hapd, "SIM", "1232010000000000",
                password="ffdca4eda45b53cf0f12d7c9c3bc6a89qcb9cccc4b9258e6dca4760379fb82581",
                expect_failure=True)

    logger.info("Missing key configuration")
    dev[0].request("REMOVE_NETWORK all")
    eap_connect(dev[0], hapd, "SIM", "1232010000000000",
                expect_failure=True)

def test_ap_wpa2_eap_sim_sql(dev, apdev, params):
    """WPA2-Enterprise connection using EAP-SIM (SQL)"""
    check_hlr_auc_gw_support()
    try:
        import sqlite3
    except ImportError:
        raise HwsimSkip("No sqlite3 module available")
    con = sqlite3.connect(os.path.join(params['logdir'], "hostapd.db"))
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    params['auth_server_port'] = "1814"
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "SIM", "1232010000000000",
                password="90dca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb82581")

    logger.info("SIM fast re-authentication")
    eap_reauth(dev[0], "SIM")

    logger.info("SIM full auth with pseudonym")
    with con:
        cur = con.cursor()
        cur.execute("DELETE FROM reauth WHERE permanent='1232010000000000'")
    eap_reauth(dev[0], "SIM")

    logger.info("SIM full auth with permanent identity")
    with con:
        cur = con.cursor()
        cur.execute("DELETE FROM reauth WHERE permanent='1232010000000000'")
        cur.execute("DELETE FROM pseudonyms WHERE permanent='1232010000000000'")
    eap_reauth(dev[0], "SIM")

    logger.info("SIM reauth with mismatching MK")
    with con:
        cur = con.cursor()
        cur.execute("UPDATE reauth SET mk='0000000000000000000000000000000000000000' WHERE permanent='1232010000000000'")
    eap_reauth(dev[0], "SIM", expect_failure=True)
    dev[0].request("REMOVE_NETWORK all")

    eap_connect(dev[0], hapd, "SIM", "1232010000000000",
                password="90dca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb82581")
    with con:
        cur = con.cursor()
        cur.execute("UPDATE reauth SET counter='10' WHERE permanent='1232010000000000'")
    eap_reauth(dev[0], "SIM")
    with con:
        cur = con.cursor()
        cur.execute("UPDATE reauth SET counter='10' WHERE permanent='1232010000000000'")
    logger.info("SIM reauth with mismatching counter")
    eap_reauth(dev[0], "SIM")
    dev[0].request("REMOVE_NETWORK all")

    eap_connect(dev[0], hapd, "SIM", "1232010000000000",
                password="90dca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb82581")
    with con:
        cur = con.cursor()
        cur.execute("UPDATE reauth SET counter='1001' WHERE permanent='1232010000000000'")
    logger.info("SIM reauth with max reauth count reached")
    eap_reauth(dev[0], "SIM")

def test_ap_wpa2_eap_sim_config(dev, apdev):
    """EAP-SIM configuration options"""
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="SIM",
                   identity="1232010000000000",
                   password="90dca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb82581",
                   phase1="sim_min_num_chal=1",
                   wait_connect=False, scan_freq="2412")
    ev = dev[0].wait_event(["EAP: Failed to initialize EAP method: vendor 0 method 18 (SIM)"], timeout=10)
    if ev is None:
        raise Exception("No EAP error message seen")
    dev[0].request("REMOVE_NETWORK all")

    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="SIM",
                   identity="1232010000000000",
                   password="90dca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb82581",
                   phase1="sim_min_num_chal=4",
                   wait_connect=False, scan_freq="2412")
    ev = dev[0].wait_event(["EAP: Failed to initialize EAP method: vendor 0 method 18 (SIM)"], timeout=10)
    if ev is None:
        raise Exception("No EAP error message seen (2)")
    dev[0].request("REMOVE_NETWORK all")

    eap_connect(dev[0], hapd, "SIM", "1232010000000000",
                password="90dca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb82581",
                phase1="sim_min_num_chal=2")
    eap_connect(dev[1], hapd, "SIM", "1232010000000000",
                password="90dca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb82581",
                anonymous_identity="345678")

def test_ap_wpa2_eap_sim_id_0(dev, apdev):
    """WPA2-Enterprise connection using EAP-SIM (no pseudonym or reauth)"""
    run_ap_wpa2_eap_sim_id(dev, apdev, 0)

def test_ap_wpa2_eap_sim_id_1(dev, apdev):
    """WPA2-Enterprise connection using EAP-SIM (pseudonym, no reauth)"""
    run_ap_wpa2_eap_sim_id(dev, apdev, 1)

def test_ap_wpa2_eap_sim_id_2(dev, apdev):
    """WPA2-Enterprise connection using EAP-SIM (no pseudonym, reauth)"""
    run_ap_wpa2_eap_sim_id(dev, apdev, 2)

def test_ap_wpa2_eap_sim_id_3(dev, apdev):
    """WPA2-Enterprise connection using EAP-SIM (pseudonym and reauth)"""
    run_ap_wpa2_eap_sim_id(dev, apdev, 3)

def run_ap_wpa2_eap_sim_id(dev, apdev, eap_sim_id):
    check_hlr_auc_gw_support()
    params = int_eap_server_params()
    params['eap_sim_id'] = str(eap_sim_id)
    params['eap_sim_db'] = 'unix:/tmp/hlr_auc_gw.sock'
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "SIM", "1232010000000000",
                password="90dca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb82581")
    eap_reauth(dev[0], "SIM")

def test_ap_wpa2_eap_sim_ext(dev, apdev):
    """WPA2-Enterprise connection using EAP-SIM and external GSM auth"""
    try:
        _test_ap_wpa2_eap_sim_ext(dev, apdev)
    finally:
        dev[0].request("SET external_sim 0")

def _test_ap_wpa2_eap_sim_ext(dev, apdev):
    check_hlr_auc_gw_support()
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hostapd.add_ap(apdev[0], params)
    dev[0].request("SET external_sim 1")
    id = dev[0].connect("test-wpa2-eap", eap="SIM", key_mgmt="WPA-EAP",
                        identity="1232010000000000",
                        wait_connect=False, scan_freq="2412")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-METHOD"], timeout=15)
    if ev is None:
        raise Exception("Network connected timed out")

    ev = dev[0].wait_event(["CTRL-REQ-SIM"], timeout=15)
    if ev is None:
        raise Exception("Wait for external SIM processing request timed out")
    p = ev.split(':', 2)
    if p[1] != "GSM-AUTH":
        raise Exception("Unexpected CTRL-REQ-SIM type")
    rid = p[0].split('-')[3]

    # IK:CK:RES
    resp = "00112233445566778899aabbccddeeff:00112233445566778899aabbccddeeff:0011223344"
    # This will fail during processing, but the ctrl_iface command succeeds
    dev[0].request("CTRL-RSP-SIM-" + rid + ":UMTS-AUTH:" + resp)
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"], timeout=15)
    if ev is None:
        raise Exception("EAP failure not reported")
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()
    time.sleep(0.1)

    dev[0].select_network(id, freq="2412")
    ev = dev[0].wait_event(["CTRL-REQ-SIM"], timeout=15)
    if ev is None:
        raise Exception("Wait for external SIM processing request timed out")
    p = ev.split(':', 2)
    if p[1] != "GSM-AUTH":
        raise Exception("Unexpected CTRL-REQ-SIM type")
    rid = p[0].split('-')[3]
    # This will fail during GSM auth validation
    if "OK" not in dev[0].request("CTRL-RSP-SIM-" + rid + ":GSM-AUTH:q"):
        raise Exception("CTRL-RSP-SIM failed")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"], timeout=15)
    if ev is None:
        raise Exception("EAP failure not reported")
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()
    time.sleep(0.1)

    dev[0].select_network(id, freq="2412")
    ev = dev[0].wait_event(["CTRL-REQ-SIM"], timeout=15)
    if ev is None:
        raise Exception("Wait for external SIM processing request timed out")
    p = ev.split(':', 2)
    if p[1] != "GSM-AUTH":
        raise Exception("Unexpected CTRL-REQ-SIM type")
    rid = p[0].split('-')[3]
    # This will fail during GSM auth validation
    if "OK" not in dev[0].request("CTRL-RSP-SIM-" + rid + ":GSM-AUTH:34"):
        raise Exception("CTRL-RSP-SIM failed")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"], timeout=15)
    if ev is None:
        raise Exception("EAP failure not reported")
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()
    time.sleep(0.1)

    dev[0].select_network(id, freq="2412")
    ev = dev[0].wait_event(["CTRL-REQ-SIM"], timeout=15)
    if ev is None:
        raise Exception("Wait for external SIM processing request timed out")
    p = ev.split(':', 2)
    if p[1] != "GSM-AUTH":
        raise Exception("Unexpected CTRL-REQ-SIM type")
    rid = p[0].split('-')[3]
    # This will fail during GSM auth validation
    if "OK" not in dev[0].request("CTRL-RSP-SIM-" + rid + ":GSM-AUTH:0011223344556677"):
        raise Exception("CTRL-RSP-SIM failed")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"], timeout=15)
    if ev is None:
        raise Exception("EAP failure not reported")
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()
    time.sleep(0.1)

    dev[0].select_network(id, freq="2412")
    ev = dev[0].wait_event(["CTRL-REQ-SIM"], timeout=15)
    if ev is None:
        raise Exception("Wait for external SIM processing request timed out")
    p = ev.split(':', 2)
    if p[1] != "GSM-AUTH":
        raise Exception("Unexpected CTRL-REQ-SIM type")
    rid = p[0].split('-')[3]
    # This will fail during GSM auth validation
    if "OK" not in dev[0].request("CTRL-RSP-SIM-" + rid + ":GSM-AUTH:0011223344556677:q"):
        raise Exception("CTRL-RSP-SIM failed")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"], timeout=15)
    if ev is None:
        raise Exception("EAP failure not reported")
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()
    time.sleep(0.1)

    dev[0].select_network(id, freq="2412")
    ev = dev[0].wait_event(["CTRL-REQ-SIM"], timeout=15)
    if ev is None:
        raise Exception("Wait for external SIM processing request timed out")
    p = ev.split(':', 2)
    if p[1] != "GSM-AUTH":
        raise Exception("Unexpected CTRL-REQ-SIM type")
    rid = p[0].split('-')[3]
    # This will fail during GSM auth validation
    if "OK" not in dev[0].request("CTRL-RSP-SIM-" + rid + ":GSM-AUTH:0011223344556677:00112233"):
        raise Exception("CTRL-RSP-SIM failed")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"], timeout=15)
    if ev is None:
        raise Exception("EAP failure not reported")
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()
    time.sleep(0.1)

    dev[0].select_network(id, freq="2412")
    ev = dev[0].wait_event(["CTRL-REQ-SIM"], timeout=15)
    if ev is None:
        raise Exception("Wait for external SIM processing request timed out")
    p = ev.split(':', 2)
    if p[1] != "GSM-AUTH":
        raise Exception("Unexpected CTRL-REQ-SIM type")
    rid = p[0].split('-')[3]
    # This will fail during GSM auth validation
    if "OK" not in dev[0].request("CTRL-RSP-SIM-" + rid + ":GSM-AUTH:0011223344556677:00112233:q"):
        raise Exception("CTRL-RSP-SIM failed")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"], timeout=15)
    if ev is None:
        raise Exception("EAP failure not reported")

def test_ap_wpa2_eap_sim_ext_replace_sim(dev, apdev):
    """EAP-SIM with external GSM auth and replacing SIM without clearing pseudonym id"""
    try:
        _test_ap_wpa2_eap_sim_ext_replace_sim(dev, apdev)
    finally:
        dev[0].request("SET external_sim 0")

def _test_ap_wpa2_eap_sim_ext_replace_sim(dev, apdev):
    check_hlr_auc_gw_support()
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hostapd.add_ap(apdev[0], params)
    dev[0].request("SET external_sim 1")
    id = dev[0].connect("test-wpa2-eap", eap="SIM", key_mgmt="WPA-EAP",
                        identity="1232010000000000",
                        wait_connect=False, scan_freq="2412")

    ev = dev[0].wait_event(["CTRL-REQ-SIM"], timeout=15)
    if ev is None:
        raise Exception("Wait for external SIM processing request timed out")
    p = ev.split(':', 2)
    if p[1] != "GSM-AUTH":
        raise Exception("Unexpected CTRL-REQ-SIM type")
    rid = p[0].split('-')[3]
    rand = p[2].split(' ')[0]

    res = subprocess.check_output(["../../hostapd/hlr_auc_gw",
                                   "-m",
                                   "auth_serv/hlr_auc_gw.milenage_db",
                                   "GSM-AUTH-REQ 232010000000000 " + rand]).decode()
    if "GSM-AUTH-RESP" not in res:
        raise Exception("Unexpected hlr_auc_gw response")
    resp = res.split(' ')[2].rstrip()

    dev[0].request("CTRL-RSP-SIM-" + rid + ":GSM-AUTH:" + resp)
    dev[0].wait_connected(timeout=15)
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    # Replace SIM, but forget to drop the previous pseudonym identity
    dev[0].set_network_quoted(id, "identity", "1232010000000009")
    dev[0].select_network(id, freq="2412")

    ev = dev[0].wait_event(["CTRL-REQ-SIM"], timeout=15)
    if ev is None:
        raise Exception("Wait for external SIM processing request timed out")
    p = ev.split(':', 2)
    if p[1] != "GSM-AUTH":
        raise Exception("Unexpected CTRL-REQ-SIM type")
    rid = p[0].split('-')[3]
    rand = p[2].split(' ')[0]

    res = subprocess.check_output(["../../hostapd/hlr_auc_gw",
                                   "-m",
                                   "auth_serv/hlr_auc_gw.milenage_db",
                                   "GSM-AUTH-REQ 232010000000009 " + rand]).decode()
    if "GSM-AUTH-RESP" not in res:
        raise Exception("Unexpected hlr_auc_gw response")
    resp = res.split(' ')[2].rstrip()

    dev[0].request("CTRL-RSP-SIM-" + rid + ":GSM-AUTH:" + resp)
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"], timeout=15)
    if ev is None:
        raise Exception("EAP-Failure not reported")
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

def test_ap_wpa2_eap_sim_ext_replace_sim2(dev, apdev):
    """EAP-SIM with external GSM auth and replacing SIM and clearing pseudonym identity"""
    try:
        _test_ap_wpa2_eap_sim_ext_replace_sim2(dev, apdev)
    finally:
        dev[0].request("SET external_sim 0")

def _test_ap_wpa2_eap_sim_ext_replace_sim2(dev, apdev):
    check_hlr_auc_gw_support()
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hostapd.add_ap(apdev[0], params)
    dev[0].request("SET external_sim 1")
    id = dev[0].connect("test-wpa2-eap", eap="SIM", key_mgmt="WPA-EAP",
                        identity="1232010000000000",
                        wait_connect=False, scan_freq="2412")

    ev = dev[0].wait_event(["CTRL-REQ-SIM"], timeout=15)
    if ev is None:
        raise Exception("Wait for external SIM processing request timed out")
    p = ev.split(':', 2)
    if p[1] != "GSM-AUTH":
        raise Exception("Unexpected CTRL-REQ-SIM type")
    rid = p[0].split('-')[3]
    rand = p[2].split(' ')[0]

    res = subprocess.check_output(["../../hostapd/hlr_auc_gw",
                                   "-m",
                                   "auth_serv/hlr_auc_gw.milenage_db",
                                   "GSM-AUTH-REQ 232010000000000 " + rand]).decode()
    if "GSM-AUTH-RESP" not in res:
        raise Exception("Unexpected hlr_auc_gw response")
    resp = res.split(' ')[2].rstrip()

    dev[0].request("CTRL-RSP-SIM-" + rid + ":GSM-AUTH:" + resp)
    dev[0].wait_connected(timeout=15)
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    # Replace SIM and drop the previous pseudonym identity
    dev[0].set_network_quoted(id, "identity", "1232010000000009")
    dev[0].set_network(id, "anonymous_identity", "NULL")
    dev[0].select_network(id, freq="2412")

    ev = dev[0].wait_event(["CTRL-REQ-SIM"], timeout=15)
    if ev is None:
        raise Exception("Wait for external SIM processing request timed out")
    p = ev.split(':', 2)
    if p[1] != "GSM-AUTH":
        raise Exception("Unexpected CTRL-REQ-SIM type")
    rid = p[0].split('-')[3]
    rand = p[2].split(' ')[0]

    res = subprocess.check_output(["../../hostapd/hlr_auc_gw",
                                   "-m",
                                   "auth_serv/hlr_auc_gw.milenage_db",
                                   "GSM-AUTH-REQ 232010000000009 " + rand]).decode()
    if "GSM-AUTH-RESP" not in res:
        raise Exception("Unexpected hlr_auc_gw response")
    resp = res.split(' ')[2].rstrip()

    dev[0].request("CTRL-RSP-SIM-" + rid + ":GSM-AUTH:" + resp)
    dev[0].wait_connected()
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

def test_ap_wpa2_eap_sim_ext_replace_sim3(dev, apdev):
    """EAP-SIM with external GSM auth, replacing SIM, and no identity in config"""
    try:
        _test_ap_wpa2_eap_sim_ext_replace_sim3(dev, apdev)
    finally:
        dev[0].request("SET external_sim 0")

def _test_ap_wpa2_eap_sim_ext_replace_sim3(dev, apdev):
    check_hlr_auc_gw_support()
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hostapd.add_ap(apdev[0], params)
    dev[0].request("SET external_sim 1")
    id = dev[0].connect("test-wpa2-eap", eap="SIM", key_mgmt="WPA-EAP",
                        wait_connect=False, scan_freq="2412")

    ev = dev[0].wait_event(["CTRL-REQ-IDENTITY"])
    if ev is None:
        raise Exception("Request for identity timed out")
    rid = ev.split(':')[0].split('-')[-1]
    dev[0].request("CTRL-RSP-IDENTITY-" + rid + ":1232010000000000")

    ev = dev[0].wait_event(["CTRL-REQ-SIM"], timeout=15)
    if ev is None:
        raise Exception("Wait for external SIM processing request timed out")
    p = ev.split(':', 2)
    if p[1] != "GSM-AUTH":
        raise Exception("Unexpected CTRL-REQ-SIM type")
    rid = p[0].split('-')[3]
    rand = p[2].split(' ')[0]

    res = subprocess.check_output(["../../hostapd/hlr_auc_gw",
                                   "-m",
                                   "auth_serv/hlr_auc_gw.milenage_db",
                                   "GSM-AUTH-REQ 232010000000000 " + rand]).decode()
    if "GSM-AUTH-RESP" not in res:
        raise Exception("Unexpected hlr_auc_gw response")
    resp = res.split(' ')[2].rstrip()

    dev[0].request("CTRL-RSP-SIM-" + rid + ":GSM-AUTH:" + resp)
    dev[0].wait_connected(timeout=15)
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    # Replace SIM and drop the previous permanent and pseudonym identities
    dev[0].set_network(id, "identity", "NULL")
    dev[0].set_network(id, "anonymous_identity", "NULL")
    dev[0].select_network(id, freq="2412")

    ev = dev[0].wait_event(["CTRL-REQ-IDENTITY"])
    if ev is None:
        raise Exception("Request for identity timed out")
    rid = ev.split(':')[0].split('-')[-1]
    dev[0].request("CTRL-RSP-IDENTITY-" + rid + ":1232010000000009")

    ev = dev[0].wait_event(["CTRL-REQ-SIM"], timeout=15)
    if ev is None:
        raise Exception("Wait for external SIM processing request timed out")
    p = ev.split(':', 2)
    if p[1] != "GSM-AUTH":
        raise Exception("Unexpected CTRL-REQ-SIM type")
    rid = p[0].split('-')[3]
    rand = p[2].split(' ')[0]

    res = subprocess.check_output(["../../hostapd/hlr_auc_gw",
                                   "-m",
                                   "auth_serv/hlr_auc_gw.milenage_db",
                                   "GSM-AUTH-REQ 232010000000009 " + rand]).decode()
    if "GSM-AUTH-RESP" not in res:
        raise Exception("Unexpected hlr_auc_gw response")
    resp = res.split(' ')[2].rstrip()

    dev[0].request("CTRL-RSP-SIM-" + rid + ":GSM-AUTH:" + resp)
    dev[0].wait_connected()
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

def test_ap_wpa2_eap_sim_ext_auth_fail(dev, apdev):
    """EAP-SIM with external GSM auth and auth failing"""
    try:
        _test_ap_wpa2_eap_sim_ext_auth_fail(dev, apdev)
    finally:
        dev[0].request("SET external_sim 0")

def _test_ap_wpa2_eap_sim_ext_auth_fail(dev, apdev):
    check_hlr_auc_gw_support()
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hostapd.add_ap(apdev[0], params)
    dev[0].request("SET external_sim 1")
    id = dev[0].connect("test-wpa2-eap", eap="SIM", key_mgmt="WPA-EAP",
                        identity="1232010000000000",
                        wait_connect=False, scan_freq="2412")

    ev = dev[0].wait_event(["CTRL-REQ-SIM"], timeout=15)
    if ev is None:
        raise Exception("Wait for external SIM processing request timed out")
    p = ev.split(':', 2)
    rid = p[0].split('-')[3]
    dev[0].request("CTRL-RSP-SIM-" + rid + ":GSM-FAIL")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"], timeout=5)
    if ev is None:
        raise Exception("EAP failure not reported")
    dev[0].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected()

def test_ap_wpa2_eap_sim_change_bssid(dev, apdev):
    """EAP-SIM and external GSM auth to check fast reauth with bssid change"""
    try:
        _test_ap_wpa2_eap_sim_change_bssid(dev, apdev)
    finally:
        dev[0].request("SET external_sim 0")

def _test_ap_wpa2_eap_sim_change_bssid(dev, apdev):
    check_hlr_auc_gw_support()
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].request("SET external_sim 1")
    id = dev[0].connect("test-wpa2-eap", eap="SIM", key_mgmt="WPA-EAP",
                        identity="1232010000000000",
                        wait_connect=False, scan_freq="2412")

    ev = dev[0].wait_event(["CTRL-REQ-SIM"], timeout=15)
    if ev is None:
        raise Exception("Wait for external SIM processing request timed out")
    p = ev.split(':', 2)
    if p[1] != "GSM-AUTH":
        raise Exception("Unexpected CTRL-REQ-SIM type")
    rid = p[0].split('-')[3]
    rand = p[2].split(' ')[0]

    res = subprocess.check_output(["../../hostapd/hlr_auc_gw",
                                   "-m",
                                   "auth_serv/hlr_auc_gw.milenage_db",
                                   "GSM-AUTH-REQ 232010000000000 " + rand]).decode()
    if "GSM-AUTH-RESP" not in res:
        raise Exception("Unexpected hlr_auc_gw response")
    resp = res.split(' ')[2].rstrip()

    dev[0].request("CTRL-RSP-SIM-" + rid + ":GSM-AUTH:" + resp)
    dev[0].wait_connected(timeout=15)
    hapd.wait_sta()

    # Verify that EAP-SIM Reauthentication can be used after a profile change
    # that does not affect EAP parameters.
    dev[0].set_network(id, "bssid", "any")
    eap_reauth(dev[0], "SIM")

def test_ap_wpa2_eap_sim_no_change_set(dev, apdev):
    """EAP-SIM and external GSM auth to check fast reauth with no-change SET_NETWORK"""
    try:
        _test_ap_wpa2_eap_sim_no_change_set(dev, apdev)
    finally:
        dev[0].request("SET external_sim 0")

def _test_ap_wpa2_eap_sim_no_change_set(dev, apdev):
    check_hlr_auc_gw_support()
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].request("SET external_sim 1")
    id = dev[0].connect("test-wpa2-eap", eap="SIM", key_mgmt="WPA-EAP",
                        identity="1232010000000000",
                        wait_connect=False, scan_freq="2412")

    ev = dev[0].wait_event(["CTRL-REQ-SIM"], timeout=15)
    if ev is None:
        raise Exception("Wait for external SIM processing request timed out")
    p = ev.split(':', 2)
    if p[1] != "GSM-AUTH":
        raise Exception("Unexpected CTRL-REQ-SIM type")
    rid = p[0].split('-')[3]
    rand = p[2].split(' ')[0]

    res = subprocess.check_output(["../../hostapd/hlr_auc_gw",
                                   "-m",
                                   "auth_serv/hlr_auc_gw.milenage_db",
                                   "GSM-AUTH-REQ 232010000000000 " + rand]).decode()
    if "GSM-AUTH-RESP" not in res:
        raise Exception("Unexpected hlr_auc_gw response")
    resp = res.split(' ')[2].rstrip()

    dev[0].request("CTRL-RSP-SIM-" + rid + ":GSM-AUTH:" + resp)
    dev[0].wait_connected(timeout=15)
    hapd.wait_sta()

    # Verify that EAP-SIM Reauthentication can be used after network profile
    # SET_NETWORK commands that do not actually change previously set
    # parameter values.
    dev[0].set_network(id, "key_mgmt", "WPA-EAP")
    dev[0].set_network(id, "eap", "SIM")
    dev[0].set_network_quoted(id, "identity", "1232010000000000")
    dev[0].set_network_quoted(id, "ssid", "test-wpa2-eap")
    eap_reauth(dev[0], "SIM")

def test_ap_wpa2_eap_sim_ext_anonymous(dev, apdev):
    """EAP-SIM with external GSM auth and anonymous identity"""
    check_hlr_auc_gw_support()
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hostapd.add_ap(apdev[0], params)
    try:
        run_ap_wpa2_eap_sim_ext_anonymous(dev, "anonymous@example.org")
        run_ap_wpa2_eap_sim_ext_anonymous(dev, "@example.org")
        run_ap_wpa2_eap_sim_ext_anonymous(dev, "example.org!anonymous@otherexample.org")
    finally:
        dev[0].request("SET external_sim 0")

def test_ap_wpa2_eap_sim_ext_anonymous_no_pseudonym(dev, apdev):
    """EAP-SIM with external GSM auth and anonymous identity without pseudonym update"""
    check_hlr_auc_gw_support()
    params = int_eap_server_params()
    params['eap_sim_id'] = '0'
    params['eap_sim_db'] = 'unix:/tmp/hlr_auc_gw.sock'
    hostapd.add_ap(apdev[0], params)
    try:
        run_ap_wpa2_eap_sim_ext_anonymous(dev, "anonymous@example.org",
                                          anon_id_change=False)
        run_ap_wpa2_eap_sim_ext_anonymous(dev, "@example.org",
                                          anon_id_change=False)
    finally:
        dev[0].request("SET external_sim 0")

def run_ap_wpa2_eap_sim_ext_anonymous(dev, anon, anon_id_change=True):
    dev[0].request("SET external_sim 1")
    id = dev[0].connect("test-wpa2-eap", eap="SIM", key_mgmt="WPA-EAP",
                        identity="1232010000000000",
                        anonymous_identity=anon,
                        wait_connect=False, scan_freq="2412")

    ev = dev[0].wait_event(["CTRL-REQ-SIM"], timeout=15)
    if ev is None:
        raise Exception("Wait for external SIM processing request timed out")
    p = ev.split(':', 2)
    if p[1] != "GSM-AUTH":
        raise Exception("Unexpected CTRL-REQ-SIM type")
    rid = p[0].split('-')[3]
    rand = p[2].split(' ')[0]

    res = subprocess.check_output(["../../hostapd/hlr_auc_gw",
                                   "-m",
                                   "auth_serv/hlr_auc_gw.milenage_db",
                                   "GSM-AUTH-REQ 232010000000000 " + rand]).decode()
    if "GSM-AUTH-RESP" not in res:
        raise Exception("Unexpected hlr_auc_gw response")
    resp = res.split(' ')[2].rstrip()

    dev[0].request("CTRL-RSP-SIM-" + rid + ":GSM-AUTH:" + resp)
    dev[0].wait_connected(timeout=5)
    anon_id = dev[0].get_network(id, "anonymous_identity").strip('"')
    if anon_id_change and anon == anon_id:
        raise Exception("anonymous_identity did not change")
    if not anon_id_change and anon != anon_id:
        raise Exception("anonymous_identity changed")
    dev[0].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected()
    dev[0].dump_monitor()

def test_ap_wpa2_eap_sim_oom(dev, apdev):
    """EAP-SIM and OOM"""
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hostapd.add_ap(apdev[0], params)
    tests = [(1, "milenage_f2345"),
             (2, "milenage_f2345"),
             (3, "milenage_f2345"),
             (4, "milenage_f2345"),
             (5, "milenage_f2345"),
             (6, "milenage_f2345"),
             (7, "milenage_f2345"),
             (8, "milenage_f2345"),
             (9, "milenage_f2345"),
             (10, "milenage_f2345"),
             (11, "milenage_f2345"),
             (12, "milenage_f2345")]
    for count, func in tests:
        with fail_test(dev[0], count, func):
            dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="SIM",
                           identity="1232010000000000",
                           password="90dca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb82581",
                           wait_connect=False, scan_freq="2412")
            ev = dev[0].wait_event(["CTRL-EVENT-EAP-METHOD"], timeout=5)
            if ev is None:
                raise Exception("EAP method not selected")
            dev[0].wait_disconnected()
            dev[0].request("REMOVE_NETWORK all")

def test_ap_wpa2_eap_aka(dev, apdev):
    """WPA2-Enterprise connection using EAP-AKA"""
    check_hlr_auc_gw_support()
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "AKA", "0232010000000000",
                password="90dca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb82581:000000000123")
    hwsim_utils.test_connectivity(dev[0], hapd)
    eap_reauth(dev[0], "AKA")

    logger.info("Negative test with incorrect key")
    dev[0].request("REMOVE_NETWORK all")
    eap_connect(dev[0], hapd, "AKA", "0232010000000000",
                password="ffdca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb82581:000000000123",
                expect_failure=True)

    logger.info("Invalid Milenage key")
    dev[0].request("REMOVE_NETWORK all")
    eap_connect(dev[0], hapd, "AKA", "0232010000000000",
                password="ffdca4eda45b53cf0f12d7c9c3bc6a",
                expect_failure=True)

    logger.info("Invalid Milenage key(2)")
    eap_connect(dev[0], hapd, "AKA", "0232010000000000",
                password="ffdca4eda45b53cf0f12d7c9c3bc6a8q:cb9cccc4b9258e6dca4760379fb82581:000000000123",
                expect_failure=True)

    logger.info("Invalid Milenage key(3)")
    eap_connect(dev[0], hapd, "AKA", "0232010000000000",
                password="ffdca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb8258q:000000000123",
                expect_failure=True)

    logger.info("Invalid Milenage key(4)")
    eap_connect(dev[0], hapd, "AKA", "0232010000000000",
                password="ffdca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb82581:00000000012q",
                expect_failure=True)

    logger.info("Invalid Milenage key(5)")
    dev[0].request("REMOVE_NETWORK all")
    eap_connect(dev[0], hapd, "AKA", "0232010000000000",
                password="ffdca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb82581q000000000123",
                expect_failure=True)

    logger.info("Invalid Milenage key(6)")
    dev[0].request("REMOVE_NETWORK all")
    eap_connect(dev[0], hapd, "AKA", "0232010000000000",
                password="ffdca4eda45b53cf0f12d7c9c3bc6a89qcb9cccc4b9258e6dca4760379fb82581q000000000123",
                expect_failure=True)

    logger.info("Missing key configuration")
    dev[0].request("REMOVE_NETWORK all")
    eap_connect(dev[0], hapd, "AKA", "0232010000000000",
                expect_failure=True)

def test_ap_wpa2_eap_aka_sql(dev, apdev, params):
    """WPA2-Enterprise connection using EAP-AKA (SQL)"""
    check_hlr_auc_gw_support()
    try:
        import sqlite3
    except ImportError:
        raise HwsimSkip("No sqlite3 module available")
    con = sqlite3.connect(os.path.join(params['logdir'], "hostapd.db"))
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    params['auth_server_port'] = "1814"
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "AKA", "0232010000000000",
                password="90dca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb82581:000000000123")

    logger.info("AKA fast re-authentication")
    eap_reauth(dev[0], "AKA")

    logger.info("AKA full auth with pseudonym")
    with con:
        cur = con.cursor()
        cur.execute("DELETE FROM reauth WHERE permanent='0232010000000000'")
    eap_reauth(dev[0], "AKA")

    logger.info("AKA full auth with permanent identity")
    with con:
        cur = con.cursor()
        cur.execute("DELETE FROM reauth WHERE permanent='0232010000000000'")
        cur.execute("DELETE FROM pseudonyms WHERE permanent='0232010000000000'")
    eap_reauth(dev[0], "AKA")

    logger.info("AKA reauth with mismatching MK")
    with con:
        cur = con.cursor()
        cur.execute("UPDATE reauth SET mk='0000000000000000000000000000000000000000' WHERE permanent='0232010000000000'")
    eap_reauth(dev[0], "AKA", expect_failure=True)
    dev[0].request("REMOVE_NETWORK all")

    eap_connect(dev[0], hapd, "AKA", "0232010000000000",
                password="90dca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb82581:000000000123")
    with con:
        cur = con.cursor()
        cur.execute("UPDATE reauth SET counter='10' WHERE permanent='0232010000000000'")
    eap_reauth(dev[0], "AKA")
    with con:
        cur = con.cursor()
        cur.execute("UPDATE reauth SET counter='10' WHERE permanent='0232010000000000'")
    logger.info("AKA reauth with mismatching counter")
    eap_reauth(dev[0], "AKA")
    dev[0].request("REMOVE_NETWORK all")

    eap_connect(dev[0], hapd, "AKA", "0232010000000000",
                password="90dca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb82581:000000000123")
    with con:
        cur = con.cursor()
        cur.execute("UPDATE reauth SET counter='1001' WHERE permanent='0232010000000000'")
    logger.info("AKA reauth with max reauth count reached")
    eap_reauth(dev[0], "AKA")

def test_ap_wpa2_eap_aka_config(dev, apdev):
    """EAP-AKA configuration options"""
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "AKA", "0232010000000000",
                password="90dca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb82581:000000000123",
                anonymous_identity="2345678")

def test_ap_wpa2_eap_aka_ext(dev, apdev):
    """WPA2-Enterprise connection using EAP-AKA and external UMTS auth"""
    try:
        _test_ap_wpa2_eap_aka_ext(dev, apdev)
    finally:
        dev[0].request("SET external_sim 0")

def _test_ap_wpa2_eap_aka_ext(dev, apdev):
    check_hlr_auc_gw_support()
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hostapd.add_ap(apdev[0], params)
    dev[0].request("SET external_sim 1")
    id = dev[0].connect("test-wpa2-eap", eap="AKA", key_mgmt="WPA-EAP",
                        identity="0232010000000000",
                        password="90dca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb82581:000000000123",
                        wait_connect=False, scan_freq="2412")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-METHOD"], timeout=15)
    if ev is None:
        raise Exception("Network connected timed out")

    ev = dev[0].wait_event(["CTRL-REQ-SIM"], timeout=15)
    if ev is None:
        raise Exception("Wait for external SIM processing request timed out")
    p = ev.split(':', 2)
    if p[1] != "UMTS-AUTH":
        raise Exception("Unexpected CTRL-REQ-SIM type")
    rid = p[0].split('-')[3]

    # IK:CK:RES
    resp = "00112233445566778899aabbccddeeff:00112233445566778899aabbccddeeff:0011223344"
    # This will fail during processing, but the ctrl_iface command succeeds
    dev[0].request("CTRL-RSP-SIM-" + rid + ":GSM-AUTH:" + resp)
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"], timeout=15)
    if ev is None:
        raise Exception("EAP failure not reported")
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()
    time.sleep(0.1)
    dev[0].dump_monitor()

    dev[0].select_network(id, freq="2412")
    ev = dev[0].wait_event(["CTRL-REQ-SIM"], timeout=15)
    if ev is None:
        raise Exception("Wait for external SIM processing request timed out")
    p = ev.split(':', 2)
    if p[1] != "UMTS-AUTH":
        raise Exception("Unexpected CTRL-REQ-SIM type")
    rid = p[0].split('-')[3]
    # This will fail during UMTS auth validation
    if "OK" not in dev[0].request("CTRL-RSP-SIM-" + rid + ":UMTS-AUTS:112233445566778899aabbccddee"):
        raise Exception("CTRL-RSP-SIM failed")
    ev = dev[0].wait_event(["CTRL-REQ-SIM"], timeout=15)
    if ev is None:
        raise Exception("Wait for external SIM processing request timed out")
    p = ev.split(':', 2)
    if p[1] != "UMTS-AUTH":
        raise Exception("Unexpected CTRL-REQ-SIM type")
    rid = p[0].split('-')[3]
    # This will fail during UMTS auth validation
    if "OK" not in dev[0].request("CTRL-RSP-SIM-" + rid + ":UMTS-AUTS:12"):
        raise Exception("CTRL-RSP-SIM failed")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"], timeout=15)
    if ev is None:
        raise Exception("EAP failure not reported")
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()
    time.sleep(0.1)
    dev[0].dump_monitor()

    tests = [":UMTS-AUTH:00112233445566778899aabbccddeeff:00112233445566778899aabbccddeeff:0011223344",
             ":UMTS-AUTH:34",
             ":UMTS-AUTH:00112233445566778899aabbccddeeff.00112233445566778899aabbccddeeff:0011223344",
             ":UMTS-AUTH:00112233445566778899aabbccddeeff:00112233445566778899aabbccddee:0011223344",
             ":UMTS-AUTH:00112233445566778899aabbccddeeff:00112233445566778899aabbccddeeff.0011223344",
             ":UMTS-AUTH:00112233445566778899aabbccddeeff:00112233445566778899aabbccddeeff:00112233445566778899aabbccddeeff0011223344",
             ":UMTS-AUTH:00112233445566778899aabbccddeeff:00112233445566778899aabbccddeeff:001122334q"]
    for t in tests:
        dev[0].select_network(id, freq="2412")
        ev = dev[0].wait_event(["CTRL-REQ-SIM"], timeout=15)
        if ev is None:
            raise Exception("Wait for external SIM processing request timed out")
        p = ev.split(':', 2)
        if p[1] != "UMTS-AUTH":
            raise Exception("Unexpected CTRL-REQ-SIM type")
        rid = p[0].split('-')[3]
        # This will fail during UMTS auth validation
        if "OK" not in dev[0].request("CTRL-RSP-SIM-" + rid + t):
            raise Exception("CTRL-RSP-SIM failed")
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"], timeout=15)
        if ev is None:
            raise Exception("EAP failure not reported")
        dev[0].request("DISCONNECT")
        dev[0].wait_disconnected()
        time.sleep(0.1)
        dev[0].dump_monitor()

def test_ap_wpa2_eap_aka_ext_auth_fail(dev, apdev):
    """EAP-AKA with external UMTS auth and auth failing"""
    try:
        _test_ap_wpa2_eap_aka_ext_auth_fail(dev, apdev)
    finally:
        dev[0].request("SET external_sim 0")

def _test_ap_wpa2_eap_aka_ext_auth_fail(dev, apdev):
    check_hlr_auc_gw_support()
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hostapd.add_ap(apdev[0], params)
    dev[0].request("SET external_sim 1")
    id = dev[0].connect("test-wpa2-eap", eap="AKA", key_mgmt="WPA-EAP",
                        identity="0232010000000000",
                        wait_connect=False, scan_freq="2412")

    ev = dev[0].wait_event(["CTRL-REQ-SIM"], timeout=15)
    if ev is None:
        raise Exception("Wait for external SIM processing request timed out")
    p = ev.split(':', 2)
    rid = p[0].split('-')[3]
    dev[0].request("CTRL-RSP-SIM-" + rid + ":UMTS-FAIL")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"], timeout=5)
    if ev is None:
        raise Exception("EAP failure not reported")
    dev[0].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected()

def test_ap_wpa2_eap_aka_prime(dev, apdev):
    """WPA2-Enterprise connection using EAP-AKA'"""
    check_hlr_auc_gw_support()
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "AKA'", "6555444333222111",
                password="5122250214c33e723a5dd523fc145fc0:981d464c7c52eb6e5036234984ad0bcf:000000000123")
    hwsim_utils.test_connectivity(dev[0], hapd)
    eap_reauth(dev[0], "AKA'")

    logger.info("EAP-AKA' bidding protection when EAP-AKA enabled as well")
    dev[1].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="AKA' AKA",
                   identity="6555444333222111@both",
                   password="5122250214c33e723a5dd523fc145fc0:981d464c7c52eb6e5036234984ad0bcf:000000000123",
                   wait_connect=False, scan_freq="2412")
    dev[1].wait_connected(timeout=15)

    logger.info("Negative test with incorrect key")
    dev[0].request("REMOVE_NETWORK all")
    eap_connect(dev[0], hapd, "AKA'", "6555444333222111",
                password="ff22250214c33e723a5dd523fc145fc0:981d464c7c52eb6e5036234984ad0bcf:000000000123",
                expect_failure=True)

def test_ap_wpa2_eap_aka_prime_sql(dev, apdev, params):
    """WPA2-Enterprise connection using EAP-AKA' (SQL)"""
    check_hlr_auc_gw_support()
    try:
        import sqlite3
    except ImportError:
        raise HwsimSkip("No sqlite3 module available")
    con = sqlite3.connect(os.path.join(params['logdir'], "hostapd.db"))
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    params['auth_server_port'] = "1814"
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "AKA'", "6555444333222111",
                password="5122250214c33e723a5dd523fc145fc0:981d464c7c52eb6e5036234984ad0bcf:000000000123")

    logger.info("AKA' fast re-authentication")
    eap_reauth(dev[0], "AKA'")

    logger.info("AKA' full auth with pseudonym")
    with con:
        cur = con.cursor()
        cur.execute("DELETE FROM reauth WHERE permanent='6555444333222111'")
    eap_reauth(dev[0], "AKA'")

    logger.info("AKA' full auth with permanent identity")
    with con:
        cur = con.cursor()
        cur.execute("DELETE FROM reauth WHERE permanent='6555444333222111'")
        cur.execute("DELETE FROM pseudonyms WHERE permanent='6555444333222111'")
    eap_reauth(dev[0], "AKA'")

    logger.info("AKA' reauth with mismatching k_aut")
    with con:
        cur = con.cursor()
        cur.execute("UPDATE reauth SET k_aut='0000000000000000000000000000000000000000000000000000000000000000' WHERE permanent='6555444333222111'")
    eap_reauth(dev[0], "AKA'", expect_failure=True)
    dev[0].request("REMOVE_NETWORK all")

    eap_connect(dev[0], hapd, "AKA'", "6555444333222111",
                password="5122250214c33e723a5dd523fc145fc0:981d464c7c52eb6e5036234984ad0bcf:000000000123")
    with con:
        cur = con.cursor()
        cur.execute("UPDATE reauth SET counter='10' WHERE permanent='6555444333222111'")
    eap_reauth(dev[0], "AKA'")
    with con:
        cur = con.cursor()
        cur.execute("UPDATE reauth SET counter='10' WHERE permanent='6555444333222111'")
    logger.info("AKA' reauth with mismatching counter")
    eap_reauth(dev[0], "AKA'")
    dev[0].request("REMOVE_NETWORK all")

    eap_connect(dev[0], hapd, "AKA'", "6555444333222111",
                password="5122250214c33e723a5dd523fc145fc0:981d464c7c52eb6e5036234984ad0bcf:000000000123")
    with con:
        cur = con.cursor()
        cur.execute("UPDATE reauth SET counter='1001' WHERE permanent='6555444333222111'")
    logger.info("AKA' reauth with max reauth count reached")
    eap_reauth(dev[0], "AKA'")

def test_ap_wpa2_eap_aka_prime_ext_auth_fail(dev, apdev):
    """EAP-AKA' with external UMTS auth and auth failing"""
    try:
        _test_ap_wpa2_eap_aka_prime_ext_auth_fail(dev, apdev)
    finally:
        dev[0].request("SET external_sim 0")

def _test_ap_wpa2_eap_aka_prime_ext_auth_fail(dev, apdev):
    check_hlr_auc_gw_support()
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hostapd.add_ap(apdev[0], params)
    dev[0].request("SET external_sim 1")
    id = dev[0].connect("test-wpa2-eap", eap="AKA'", key_mgmt="WPA-EAP",
                        identity="6555444333222111",
                        wait_connect=False, scan_freq="2412")

    ev = dev[0].wait_event(["CTRL-REQ-SIM"], timeout=15)
    if ev is None:
        raise Exception("Wait for external SIM processing request timed out")
    p = ev.split(':', 2)
    rid = p[0].split('-')[3]
    dev[0].request("CTRL-RSP-SIM-" + rid + ":UMTS-FAIL")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"], timeout=5)
    if ev is None:
        raise Exception("EAP failure not reported")
    dev[0].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected()

def test_ap_wpa2_eap_aka_prime_ext(dev, apdev):
    """EAP-AKA' with external UMTS auth to hit Synchronization-Failure"""
    try:
        _test_ap_wpa2_eap_aka_prime_ext(dev, apdev)
    finally:
        dev[0].request("SET external_sim 0")

def _test_ap_wpa2_eap_aka_prime_ext(dev, apdev):
    check_hlr_auc_gw_support()
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hostapd.add_ap(apdev[0], params)
    dev[0].request("SET external_sim 1")
    id = dev[0].connect("test-wpa2-eap", eap="AKA'", key_mgmt="WPA-EAP",
                        identity="6555444333222111",
                        password="90dca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb82581:000000000123",
                        wait_connect=False, scan_freq="2412")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-METHOD"], timeout=15)
    if ev is None:
        raise Exception("Network connected timed out")

    ev = dev[0].wait_event(["CTRL-REQ-SIM"], timeout=15)
    if ev is None:
        raise Exception("Wait for external SIM processing request timed out")
    p = ev.split(':', 2)
    if p[1] != "UMTS-AUTH":
        raise Exception("Unexpected CTRL-REQ-SIM type")
    rid = p[0].split('-')[3]
    # This will fail during UMTS auth validation
    if "OK" not in dev[0].request("CTRL-RSP-SIM-" + rid + ":UMTS-AUTS:112233445566778899aabbccddee"):
        raise Exception("CTRL-RSP-SIM failed")
    ev = dev[0].wait_event(["CTRL-REQ-SIM"], timeout=15)
    if ev is None:
        raise Exception("Wait for external SIM processing request timed out")

def test_ap_wpa2_eap_ttls_pap(dev, apdev):
    """WPA2-Enterprise connection using EAP-TTLS/PAP"""
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    key_mgmt = hapd.get_config()['key_mgmt']
    if key_mgmt.split(' ')[0] != "WPA-EAP":
        raise Exception("Unexpected GET_CONFIG(key_mgmt): " + key_mgmt)
    eap_connect(dev[0], hapd, "TTLS", "pap user",
                anonymous_identity="ttls", password="password",
                ca_cert="auth_serv/ca.pem", phase2="auth=PAP")
    hwsim_utils.test_connectivity(dev[0], hapd)
    eap_reauth(dev[0], "TTLS")
    check_mib(dev[0], [("dot11RSNAAuthenticationSuiteRequested", "00-0f-ac-1"),
                       ("dot11RSNAAuthenticationSuiteSelected", "00-0f-ac-1")])

def test_ap_wpa2_eap_ttls_pap_subject_match(dev, apdev):
    """WPA2-Enterprise connection using EAP-TTLS/PAP and (alt)subject_match"""
    check_subject_match_support(dev[0])
    check_altsubject_match_support(dev[0])
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "TTLS", "pap user",
                anonymous_identity="ttls", password="password",
                ca_cert="auth_serv/ca.pem", phase2="auth=PAP",
                subject_match="/C=FI/O=w1.fi/CN=server.w1.fi",
                altsubject_match="EMAIL:noone@example.com;DNS:server.w1.fi;URI:http://example.com/")
    eap_reauth(dev[0], "TTLS")

def test_ap_wpa2_eap_ttls_pap_check_cert_subject(dev, apdev):
    """EAP-TTLS/PAP and check_cert_subject"""
    check_check_cert_subject_support(dev[0])
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    tests = ["C=FI/O=w1.fi/CN=server.w1.fi",
             "C=FI/O=w1.fi",
             "C=FI/CN=server.w1.fi",
             "O=w1.fi/CN=server.w1.fi",
             "C=FI",
             "O=w1.fi",
             "O=w1.*",
             "CN=server.w1.fi",
             "*"]
    for test in tests:
        eap_connect(dev[0], hapd, "TTLS", "pap user",
                    anonymous_identity="ttls", password="password",
                    ca_cert="auth_serv/ca.pem", phase2="auth=PAP",
                    check_cert_subject=test)
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()
        dev[0].dump_monitor()

def test_ap_wpa2_eap_ttls_pap_check_cert_subject_neg(dev, apdev):
    """EAP-TTLS/PAP and check_cert_subject (negative)"""
    check_check_cert_subject_support(dev[0])
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    tests = ["C=US",
             "C",
             "C=FI1*",
             "O=w1.f",
             "O=w1.fi1",
             "O=w1.fi/O=foo",
             "O=foo/O=w1.fi",
             "O=w1.fi/O=w1.fi"]
    for test in tests:
        eap_connect(dev[0], hapd, "TTLS", "pap user",
                    anonymous_identity="ttls", password="password",
                    ca_cert="auth_serv/ca.pem", phase2="auth=PAP",
                    expect_failure=True, expect_cert_error=12,
                    check_cert_subject=test)
        dev[0].request("REMOVE_NETWORK all")
        dev[0].dump_monitor()

def test_ap_wpa2_eap_ttls_pap_incorrect_password(dev, apdev):
    """WPA2-Enterprise connection using EAP-TTLS/PAP - incorrect password"""
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "TTLS", "pap user",
                anonymous_identity="ttls", password="wrong",
                ca_cert="auth_serv/ca.pem", phase2="auth=PAP",
                expect_failure=True)
    eap_connect(dev[1], hapd, "TTLS", "user",
                anonymous_identity="ttls", password="password",
                ca_cert="auth_serv/ca.pem", phase2="auth=PAP",
                expect_failure=True)

def test_ap_wpa2_eap_ttls_chap(dev, apdev):
    """WPA2-Enterprise connection using EAP-TTLS/CHAP"""
    skip_with_fips(dev[0])
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "TTLS", "chap user",
                anonymous_identity="ttls", password="password",
                ca_cert="auth_serv/ca.der", phase2="auth=CHAP")
    hwsim_utils.test_connectivity(dev[0], hapd)
    eap_reauth(dev[0], "TTLS")

def test_ap_wpa2_eap_ttls_chap_altsubject_match(dev, apdev):
    """WPA2-Enterprise connection using EAP-TTLS/CHAP"""
    skip_with_fips(dev[0])
    check_altsubject_match_support(dev[0])
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "TTLS", "chap user",
                anonymous_identity="ttls", password="password",
                ca_cert="auth_serv/ca.der", phase2="auth=CHAP",
                altsubject_match="EMAIL:noone@example.com;URI:http://example.com/;DNS:server.w1.fi")
    eap_reauth(dev[0], "TTLS")

def test_ap_wpa2_eap_ttls_chap_incorrect_password(dev, apdev):
    """WPA2-Enterprise connection using EAP-TTLS/CHAP - incorrect password"""
    skip_with_fips(dev[0])
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "TTLS", "chap user",
                anonymous_identity="ttls", password="wrong",
                ca_cert="auth_serv/ca.pem", phase2="auth=CHAP",
                expect_failure=True)
    eap_connect(dev[1], hapd, "TTLS", "user",
                anonymous_identity="ttls", password="password",
                ca_cert="auth_serv/ca.pem", phase2="auth=CHAP",
                expect_failure=True)

def test_ap_wpa2_eap_ttls_mschap(dev, apdev):
    """WPA2-Enterprise connection using EAP-TTLS/MSCHAP"""
    skip_with_fips(dev[0])
    check_domain_suffix_match(dev[0])
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "TTLS", "mschap user",
                anonymous_identity="ttls", password="password",
                ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAP",
                domain_suffix_match="server.w1.fi")
    hwsim_utils.test_connectivity(dev[0], hapd)
    eap_reauth(dev[0], "TTLS")
    dev[0].request("REMOVE_NETWORK all")
    eap_connect(dev[0], hapd, "TTLS", "mschap user",
                anonymous_identity="ttls", password="password",
                ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAP",
                fragment_size="200")
    dev[0].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected()
    eap_connect(dev[0], hapd, "TTLS", "mschap user",
                anonymous_identity="ttls",
                password_hex="hash:8846f7eaee8fb117ad06bdd830b7586c",
                ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAP")

def test_ap_wpa2_eap_ttls_mschap_incorrect_password(dev, apdev):
    """WPA2-Enterprise connection using EAP-TTLS/MSCHAP - incorrect password"""
    skip_with_fips(dev[0])
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "TTLS", "mschap user",
                anonymous_identity="ttls", password="wrong",
                ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAP",
                expect_failure=True)
    eap_connect(dev[1], hapd, "TTLS", "user",
                anonymous_identity="ttls", password="password",
                ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAP",
                expect_failure=True)
    eap_connect(dev[2], hapd, "TTLS", "no such user",
                anonymous_identity="ttls", password="password",
                ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAP",
                expect_failure=True)

def test_ap_wpa2_eap_ttls_mschapv2(dev, apdev):
    """WPA2-Enterprise connection using EAP-TTLS/MSCHAPv2"""
    check_domain_suffix_match(dev[0])
    check_eap_capa(dev[0], "MSCHAPV2")
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "TTLS", "DOMAIN\mschapv2 user",
                anonymous_identity="ttls", password="password",
                ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2",
                domain_suffix_match="server.w1.fi")
    hwsim_utils.test_connectivity(dev[0], hapd)
    sta1 = hapd.get_sta(dev[0].p2p_interface_addr())
    eapol1 = hapd.get_sta(dev[0].p2p_interface_addr(), info="eapol")
    eap_reauth(dev[0], "TTLS")
    sta2 = hapd.get_sta(dev[0].p2p_interface_addr())
    eapol2 = hapd.get_sta(dev[0].p2p_interface_addr(), info="eapol")
    if int(sta2['dot1xAuthEapolFramesRx']) <= int(sta1['dot1xAuthEapolFramesRx']):
        raise Exception("dot1xAuthEapolFramesRx did not increase")
    if int(eapol2['authAuthEapStartsWhileAuthenticated']) < 1:
        raise Exception("authAuthEapStartsWhileAuthenticated did not increase")
    if int(eapol2['backendAuthSuccesses']) <= int(eapol1['backendAuthSuccesses']):
        raise Exception("backendAuthSuccesses did not increase")

    logger.info("Password as hash value")
    dev[0].request("REMOVE_NETWORK all")
    eap_connect(dev[0], hapd, "TTLS", "DOMAIN\mschapv2 user",
                anonymous_identity="ttls",
                password_hex="hash:8846f7eaee8fb117ad06bdd830b7586c",
                ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2")

def test_ap_wpa2_eap_ttls_invalid_phase2(dev, apdev):
    """EAP-TTLS with invalid phase2 parameter values"""
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hostapd.add_ap(apdev[0], params)
    tests = ["auth=MSCHAPv2", "auth=MSCHAPV2 autheap=MD5",
             "autheap=MD5 auth=MSCHAPV2", "auth=PAP auth=CHAP",
             "autheap=MD5 autheap=FOO autheap=MSCHAPV2"]
    for t in tests:
        dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TTLS",
                       identity="DOMAIN\mschapv2 user",
                       anonymous_identity="ttls", password="password",
                       ca_cert="auth_serv/ca.pem", phase2=t,
                       wait_connect=False, scan_freq="2412")
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-PROPOSED-METHOD"], timeout=10)
        if ev is None or "method=21" not in ev:
            raise Exception("EAP-TTLS not started")
        ev = dev[0].wait_event(["EAP: Failed to initialize EAP method",
                                "CTRL-EVENT-CONNECTED"], timeout=5)
        if ev is None or "CTRL-EVENT-CONNECTED" in ev:
            raise Exception("No EAP-TTLS failure reported for phase2=" + t)
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()
        dev[0].dump_monitor()

def test_ap_wpa2_eap_ttls_mschapv2_suffix_match(dev, apdev):
    """WPA2-Enterprise connection using EAP-TTLS/MSCHAPv2"""
    check_domain_match_full(dev[0])
    skip_with_fips(dev[0])
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "TTLS", "DOMAIN\mschapv2 user",
                anonymous_identity="ttls", password="password",
                ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2",
                domain_suffix_match="w1.fi")
    hwsim_utils.test_connectivity(dev[0], hapd)
    eap_reauth(dev[0], "TTLS")

def test_ap_wpa2_eap_ttls_mschapv2_domain_match(dev, apdev):
    """WPA2-Enterprise connection using EAP-TTLS/MSCHAPv2 (domain_match)"""
    check_domain_match(dev[0])
    skip_with_fips(dev[0])
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "TTLS", "DOMAIN\mschapv2 user",
                anonymous_identity="ttls", password="password",
                ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2",
                domain_match="Server.w1.fi")
    hwsim_utils.test_connectivity(dev[0], hapd)
    eap_reauth(dev[0], "TTLS")

def test_ap_wpa2_eap_ttls_mschapv2_incorrect_password(dev, apdev):
    """WPA2-Enterprise connection using EAP-TTLS/MSCHAPv2 - incorrect password"""
    skip_with_fips(dev[0])
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "TTLS", "DOMAIN\mschapv2 user",
                anonymous_identity="ttls", password="password1",
                ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2",
                expect_failure=True)
    eap_connect(dev[1], hapd, "TTLS", "user",
                anonymous_identity="ttls", password="password",
                ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2",
                expect_failure=True)

def test_ap_wpa2_eap_ttls_mschapv2_utf8(dev, apdev):
    """WPA2-Enterprise connection using EAP-TTLS/MSCHAPv2 and UTF-8 password"""
    skip_with_fips(dev[0])
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "TTLS", "utf8-user-hash",
                anonymous_identity="ttls", password="secret-åäö-€-password",
                ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2")
    eap_connect(dev[1], hapd, "TTLS", "utf8-user",
                anonymous_identity="ttls",
                password_hex="hash:bd5844fad2489992da7fe8c5a01559cf",
                ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2")
    for p in ["80", "41c041e04141e041", 257*"41"]:
        dev[2].connect("test-wpa2-eap", key_mgmt="WPA-EAP",
                       eap="TTLS", identity="utf8-user-hash",
                       anonymous_identity="ttls", password_hex=p,
                       ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2",
                       wait_connect=False, scan_freq="2412")
        ev = dev[2].wait_event(["CTRL-EVENT-EAP-FAILURE"], timeout=1)
        if ev is None:
            raise Exception("No failure reported")
        dev[2].request("REMOVE_NETWORK all")
        dev[2].wait_disconnected()

def test_ap_wpa2_eap_ttls_eap_gtc(dev, apdev):
    """WPA2-Enterprise connection using EAP-TTLS/EAP-GTC"""
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "TTLS", "user",
                anonymous_identity="ttls", password="password",
                ca_cert="auth_serv/ca.pem", phase2="autheap=GTC")
    hwsim_utils.test_connectivity(dev[0], hapd)
    eap_reauth(dev[0], "TTLS")

def test_ap_wpa2_eap_ttls_eap_gtc_incorrect_password(dev, apdev):
    """WPA2-Enterprise connection using EAP-TTLS/EAP-GTC - incorrect password"""
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "TTLS", "user",
                anonymous_identity="ttls", password="wrong",
                ca_cert="auth_serv/ca.pem", phase2="autheap=GTC",
                expect_failure=True)

def test_ap_wpa2_eap_ttls_eap_gtc_no_password(dev, apdev):
    """WPA2-Enterprise connection using EAP-TTLS/EAP-GTC - no password"""
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "TTLS", "user-no-passwd",
                anonymous_identity="ttls", password="password",
                ca_cert="auth_serv/ca.pem", phase2="autheap=GTC",
                expect_failure=True)

def test_ap_wpa2_eap_ttls_eap_gtc_server_oom(dev, apdev):
    """WPA2-Enterprise connection using EAP-TTLS/EAP-GTC - server OOM"""
    params = int_eap_server_params()
    hapd = hostapd.add_ap(apdev[0], params)
    with alloc_fail(hapd, 1, "eap_gtc_init"):
        eap_connect(dev[0], hapd, "TTLS", "user",
                    anonymous_identity="ttls", password="password",
                    ca_cert="auth_serv/ca.pem", phase2="autheap=GTC",
                    expect_failure=True)
        dev[0].request("REMOVE_NETWORK all")

    with alloc_fail(hapd, 1, "eap_gtc_buildReq"):
        dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP WPA-EAP-SHA256",
                       eap="TTLS", identity="user",
                       anonymous_identity="ttls", password="password",
                       ca_cert="auth_serv/ca.pem", phase2="autheap=GTC",
                       wait_connect=False, scan_freq="2412")
        # This would eventually time out, but we can stop after having reached
        # the allocation failure.
        for i in range(20):
            time.sleep(0.1)
            if hapd.request("GET_ALLOC_FAIL").startswith('0'):
                break

def test_ap_wpa2_eap_ttls_eap_gtc_oom(dev, apdev):
    """WPA2-Enterprise connection using EAP-TTLS/EAP-GTC (OOM)"""
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)

    tests = ["eap_gtc_init",
             "eap_msg_alloc;eap_gtc_process"]
    for func in tests:
        with alloc_fail(dev[0], 1, func):
            dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP",
                           scan_freq="2412",
                           eap="TTLS", identity="user",
                           anonymous_identity="ttls", password="password",
                           ca_cert="auth_serv/ca.pem", phase2="autheap=GTC",
                           wait_connect=False)
            wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

def test_ap_wpa2_eap_ttls_eap_md5(dev, apdev):
    """WPA2-Enterprise connection using EAP-TTLS/EAP-MD5"""
    check_eap_capa(dev[0], "MD5")
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "TTLS", "user",
                anonymous_identity="ttls", password="password",
                ca_cert="auth_serv/ca.pem", phase2="autheap=MD5")
    hwsim_utils.test_connectivity(dev[0], hapd)
    eap_reauth(dev[0], "TTLS")

def test_ap_wpa2_eap_ttls_eap_md5_incorrect_password(dev, apdev):
    """WPA2-Enterprise connection using EAP-TTLS/EAP-MD5 - incorrect password"""
    check_eap_capa(dev[0], "MD5")
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "TTLS", "user",
                anonymous_identity="ttls", password="wrong",
                ca_cert="auth_serv/ca.pem", phase2="autheap=MD5",
                expect_failure=True)

def test_ap_wpa2_eap_ttls_eap_md5_no_password(dev, apdev):
    """WPA2-Enterprise connection using EAP-TTLS/EAP-MD5 - no password"""
    check_eap_capa(dev[0], "MD5")
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "TTLS", "user-no-passwd",
                anonymous_identity="ttls", password="password",
                ca_cert="auth_serv/ca.pem", phase2="autheap=MD5",
                expect_failure=True)

def test_ap_wpa2_eap_ttls_eap_md5_server_oom(dev, apdev):
    """WPA2-Enterprise connection using EAP-TTLS/EAP-MD5 - server OOM"""
    check_eap_capa(dev[0], "MD5")
    params = int_eap_server_params()
    hapd = hostapd.add_ap(apdev[0], params)
    with alloc_fail(hapd, 1, "eap_md5_init"):
        eap_connect(dev[0], hapd, "TTLS", "user",
                    anonymous_identity="ttls", password="password",
                    ca_cert="auth_serv/ca.pem", phase2="autheap=MD5",
                    expect_failure=True)
        dev[0].request("REMOVE_NETWORK all")

    with alloc_fail(hapd, 1, "eap_md5_buildReq"):
        dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP WPA-EAP-SHA256",
                       eap="TTLS", identity="user",
                       anonymous_identity="ttls", password="password",
                       ca_cert="auth_serv/ca.pem", phase2="autheap=MD5",
                       wait_connect=False, scan_freq="2412")
        # This would eventually time out, but we can stop after having reached
        # the allocation failure.
        for i in range(20):
            time.sleep(0.1)
            if hapd.request("GET_ALLOC_FAIL").startswith('0'):
                break

def test_ap_wpa2_eap_ttls_eap_mschapv2(dev, apdev):
    """WPA2-Enterprise connection using EAP-TTLS/EAP-MSCHAPv2"""
    check_eap_capa(dev[0], "MSCHAPV2")
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "TTLS", "user",
                anonymous_identity="ttls", password="password",
                ca_cert="auth_serv/ca.pem", phase2="autheap=MSCHAPV2")
    hwsim_utils.test_connectivity(dev[0], hapd)
    eap_reauth(dev[0], "TTLS")

    logger.info("Negative test with incorrect password")
    dev[0].request("REMOVE_NETWORK all")
    eap_connect(dev[0], hapd, "TTLS", "user",
                anonymous_identity="ttls", password="password1",
                ca_cert="auth_serv/ca.pem", phase2="autheap=MSCHAPV2",
                expect_failure=True)

def test_ap_wpa2_eap_ttls_eap_mschapv2_no_password(dev, apdev):
    """WPA2-Enterprise connection using EAP-TTLS/EAP-MSCHAPv2 - no password"""
    check_eap_capa(dev[0], "MSCHAPV2")
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "TTLS", "user-no-passwd",
                anonymous_identity="ttls", password="password",
                ca_cert="auth_serv/ca.pem", phase2="autheap=MSCHAPV2",
                expect_failure=True)

def test_ap_wpa2_eap_ttls_eap_mschapv2_server_oom(dev, apdev):
    """WPA2-Enterprise connection using EAP-TTLS/EAP-MSCHAPv2 - server OOM"""
    check_eap_capa(dev[0], "MSCHAPV2")
    params = int_eap_server_params()
    hapd = hostapd.add_ap(apdev[0], params)
    with alloc_fail(hapd, 1, "eap_mschapv2_init"):
        eap_connect(dev[0], hapd, "TTLS", "user",
                    anonymous_identity="ttls", password="password",
                    ca_cert="auth_serv/ca.pem", phase2="autheap=MSCHAPV2",
                    expect_failure=True)
        dev[0].request("REMOVE_NETWORK all")

    with alloc_fail(hapd, 1, "eap_mschapv2_build_challenge"):
        dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP WPA-EAP-SHA256",
                       eap="TTLS", identity="user",
                       anonymous_identity="ttls", password="password",
                       ca_cert="auth_serv/ca.pem", phase2="autheap=MSCHAPV2",
                       wait_connect=False, scan_freq="2412")
        # This would eventually time out, but we can stop after having reached
        # the allocation failure.
        for i in range(20):
            time.sleep(0.1)
            if hapd.request("GET_ALLOC_FAIL").startswith('0'):
                break
        dev[0].request("REMOVE_NETWORK all")

    with alloc_fail(hapd, 1, "eap_mschapv2_build_success_req"):
        dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP WPA-EAP-SHA256",
                       eap="TTLS", identity="user",
                       anonymous_identity="ttls", password="password",
                       ca_cert="auth_serv/ca.pem", phase2="autheap=MSCHAPV2",
                       wait_connect=False, scan_freq="2412")
        # This would eventually time out, but we can stop after having reached
        # the allocation failure.
        for i in range(20):
            time.sleep(0.1)
            if hapd.request("GET_ALLOC_FAIL").startswith('0'):
                break
        dev[0].request("REMOVE_NETWORK all")

    with alloc_fail(hapd, 1, "eap_mschapv2_build_failure_req"):
        dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP WPA-EAP-SHA256",
                       eap="TTLS", identity="user",
                       anonymous_identity="ttls", password="wrong",
                       ca_cert="auth_serv/ca.pem", phase2="autheap=MSCHAPV2",
                       wait_connect=False, scan_freq="2412")
        # This would eventually time out, but we can stop after having reached
        # the allocation failure.
        for i in range(20):
            time.sleep(0.1)
            if hapd.request("GET_ALLOC_FAIL").startswith('0'):
                break
        dev[0].request("REMOVE_NETWORK all")

def test_ap_wpa2_eap_ttls_eap_sim(dev, apdev):
    """WPA2-Enterprise connection using EAP-TTLS/EAP-SIM"""
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "TTLS", "1232010000000000",
                anonymous_identity="1232010000000000@ttls",
                password="90dca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb82581",
                ca_cert="auth_serv/ca.pem", phase2="autheap=SIM")
    eap_reauth(dev[0], "TTLS")

def run_ext_sim_auth(hapd, dev):
    ev = dev.wait_event(["CTRL-REQ-SIM"], timeout=15)
    if ev is None:
        raise Exception("Wait for external SIM processing request timed out")
    p = ev.split(':', 2)
    if p[1] != "GSM-AUTH":
        raise Exception("Unexpected CTRL-REQ-SIM type")
    rid = p[0].split('-')[3]
    rand = p[2].split(' ')[0]

    res = subprocess.check_output(["../../hostapd/hlr_auc_gw",
                                   "-m",
                                   "auth_serv/hlr_auc_gw.milenage_db",
                                   "GSM-AUTH-REQ 232010000000000 " + rand]).decode()
    if "GSM-AUTH-RESP" not in res:
        raise Exception("Unexpected hlr_auc_gw response")
    resp = res.split(' ')[2].rstrip()

    dev.request("CTRL-RSP-SIM-" + rid + ":GSM-AUTH:" + resp)
    dev.wait_connected(timeout=15)
    hapd.wait_sta()

    dev.dump_monitor()
    dev.request("REAUTHENTICATE")
    ev = dev.wait_event(["CTRL-EVENT-EAP-SUCCESS"], timeout=5)
    if ev is None:
        raise Exception("EAP reauthentication did not succeed")
    ev = dev.wait_event(["WPA: Key negotiation completed"], timeout=5)
    if ev is None:
        raise Exception("Key negotiation did not complete")
    dev.dump_monitor()

def test_ap_wpa2_eap_ttls_eap_sim_ext(dev, apdev):
    """WPA2-Enterprise connection using EAP-TTLS/EAP-SIM and external GSM auth"""
    check_hlr_auc_gw_support()
    try:
        run_ap_wpa2_eap_ttls_eap_sim_ext(dev, apdev)
    finally:
        dev[0].request("SET external_sim 0")

def run_ap_wpa2_eap_ttls_eap_sim_ext(dev, apdev):
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].request("SET external_sim 1")
    dev[0].connect("test-wpa2-eap", eap="TTLS", key_mgmt="WPA-EAP",
                   identity="1232010000000000",
                   anonymous_identity="1232010000000000@ttls",
                   password="90dca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb82581",
                   ca_cert="auth_serv/ca.pem", phase2="autheap=SIM",
                   wait_connect=False, scan_freq="2412")
    run_ext_sim_auth(hapd, dev[0])

def test_ap_wpa2_eap_ttls_eap_vendor(dev, apdev):
    """WPA2-Enterprise connection using EAP-TTLS/EAP-vendor"""
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "TTLS", "vendor-test-2",
                anonymous_identity="ttls",
                ca_cert="auth_serv/ca.pem", phase2="autheap=VENDOR-TEST")

def test_ap_wpa2_eap_peap_eap_sim(dev, apdev):
    """WPA2-Enterprise connection using EAP-PEAP/EAP-SIM"""
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "PEAP", "1232010000000000",
                anonymous_identity="1232010000000000@peap",
                password="90dca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb82581",
                ca_cert="auth_serv/ca.pem", phase2="auth=SIM")
    eap_reauth(dev[0], "PEAP")

def test_ap_wpa2_eap_peap_eap_sim_ext(dev, apdev):
    """WPA2-Enterprise connection using EAP-PEAP/EAP-SIM and external GSM auth"""
    check_hlr_auc_gw_support()
    try:
        run_ap_wpa2_eap_peap_eap_sim_ext(dev, apdev)
    finally:
        dev[0].request("SET external_sim 0")

def run_ap_wpa2_eap_peap_eap_sim_ext(dev, apdev):
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].request("SET external_sim 1")
    dev[0].connect("test-wpa2-eap", eap="PEAP", key_mgmt="WPA-EAP",
                   identity="1232010000000000",
                   anonymous_identity="1232010000000000@peap",
                   password="90dca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb82581",
                   ca_cert="auth_serv/ca.pem", phase2="auth=SIM",
                   wait_connect=False, scan_freq="2412")
    run_ext_sim_auth(hapd, dev[0])

def test_ap_wpa2_eap_fast_eap_sim(dev, apdev):
    """WPA2-Enterprise connection using EAP-FAST/EAP-SIM"""
    check_eap_capa(dev[0], "FAST")
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "FAST", "1232010000000000",
                anonymous_identity="1232010000000000@fast",
                password="90dca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb82581",
                phase1="fast_provisioning=2",
                pac_file="blob://fast_pac_auth_sim",
                ca_cert="auth_serv/ca.pem", phase2="auth=SIM")
    eap_reauth(dev[0], "FAST")

def test_ap_wpa2_eap_fast_eap_sim_ext(dev, apdev):
    """WPA2-Enterprise connection using EAP-FAST/EAP-SIM and external GSM auth"""
    check_hlr_auc_gw_support()
    try:
        run_ap_wpa2_eap_fast_eap_sim_ext(dev, apdev)
    finally:
        dev[0].request("SET external_sim 0")

def run_ap_wpa2_eap_fast_eap_sim_ext(dev, apdev):
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].request("SET external_sim 1")
    dev[0].connect("test-wpa2-eap", eap="PEAP", key_mgmt="WPA-EAP",
                   identity="1232010000000000",
                   anonymous_identity="1232010000000000@peap",
                   password="90dca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb82581",
                   phase1="fast_provisioning=2",
                   pac_file="blob://fast_pac_auth_sim",
                   ca_cert="auth_serv/ca.pem", phase2="auth=SIM",
                   wait_connect=False, scan_freq="2412")
    run_ext_sim_auth(hapd, dev[0])

def test_ap_wpa2_eap_ttls_eap_aka(dev, apdev):
    """WPA2-Enterprise connection using EAP-TTLS/EAP-AKA"""
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "TTLS", "0232010000000000",
                anonymous_identity="0232010000000000@ttls",
                password="90dca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb82581:000000000123",
                ca_cert="auth_serv/ca.pem", phase2="autheap=AKA")
    eap_reauth(dev[0], "TTLS")

def test_ap_wpa2_eap_peap_eap_aka(dev, apdev):
    """WPA2-Enterprise connection using EAP-PEAP/EAP-AKA"""
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "PEAP", "0232010000000000",
                anonymous_identity="0232010000000000@peap",
                password="90dca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb82581:000000000123",
                ca_cert="auth_serv/ca.pem", phase2="auth=AKA")
    eap_reauth(dev[0], "PEAP")

def test_ap_wpa2_eap_fast_eap_aka(dev, apdev):
    """WPA2-Enterprise connection using EAP-FAST/EAP-AKA"""
    check_eap_capa(dev[0], "FAST")
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "FAST", "0232010000000000",
                anonymous_identity="0232010000000000@fast",
                password="90dca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb82581:000000000123",
                phase1="fast_provisioning=2",
                pac_file="blob://fast_pac_auth_aka",
                ca_cert="auth_serv/ca.pem", phase2="auth=AKA")
    eap_reauth(dev[0], "FAST")

def test_ap_wpa2_eap_peap_eap_mschapv2(dev, apdev):
    """WPA2-Enterprise connection using EAP-PEAP/EAP-MSCHAPv2"""
    check_eap_capa(dev[0], "MSCHAPV2")
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "PEAP", "user",
                anonymous_identity="peap", password="password",
                ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2")
    hwsim_utils.test_connectivity(dev[0], hapd)
    eap_reauth(dev[0], "PEAP")
    dev[0].request("REMOVE_NETWORK all")
    eap_connect(dev[0], hapd, "PEAP", "user",
                anonymous_identity="peap", password="password",
                ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2",
                fragment_size="200")

    logger.info("Password as hash value")
    dev[0].request("REMOVE_NETWORK all")
    eap_connect(dev[0], hapd, "PEAP", "user",
                anonymous_identity="peap",
                password_hex="hash:8846f7eaee8fb117ad06bdd830b7586c",
                ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2")

    logger.info("Negative test with incorrect password")
    dev[0].request("REMOVE_NETWORK all")
    eap_connect(dev[0], hapd, "PEAP", "user",
                anonymous_identity="peap", password="password1",
                ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2",
                expect_failure=True)

def test_ap_wpa2_eap_peap_eap_mschapv2_domain(dev, apdev):
    """WPA2-Enterprise connection using EAP-PEAP/EAP-MSCHAPv2 with domain"""
    check_eap_capa(dev[0], "MSCHAPV2")
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "PEAP", r"DOMAIN\user3",
                anonymous_identity="peap", password="password",
                ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2")
    hwsim_utils.test_connectivity(dev[0], hapd)
    eap_reauth(dev[0], "PEAP")

def test_ap_wpa2_eap_peap_eap_mschapv2_incorrect_password(dev, apdev):
    """WPA2-Enterprise connection using EAP-PEAP/EAP-MSCHAPv2 - incorrect password"""
    check_eap_capa(dev[0], "MSCHAPV2")
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "PEAP", "user",
                anonymous_identity="peap", password="wrong",
                ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2",
                expect_failure=True)

def test_ap_wpa2_eap_peap_crypto_binding(dev, apdev):
    """WPA2-Enterprise connection using EAP-PEAPv0/EAP-MSCHAPv2 and crypto binding"""
    check_eap_capa(dev[0], "MSCHAPV2")
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "PEAP", "user", password="password",
                ca_cert="auth_serv/ca.pem",
                phase1="peapver=0 crypto_binding=2",
                phase2="auth=MSCHAPV2")
    hwsim_utils.test_connectivity(dev[0], hapd)
    eap_reauth(dev[0], "PEAP")

    eap_connect(dev[1], hapd, "PEAP", "user", password="password",
                ca_cert="auth_serv/ca.pem",
                phase1="peapver=0 crypto_binding=1",
                phase2="auth=MSCHAPV2")
    eap_connect(dev[2], hapd, "PEAP", "user", password="password",
                ca_cert="auth_serv/ca.pem",
                phase1="peapver=0 crypto_binding=0",
                phase2="auth=MSCHAPV2")

def test_ap_wpa2_eap_peap_crypto_binding_server_oom(dev, apdev):
    """WPA2-Enterprise connection using EAP-PEAPv0/EAP-MSCHAPv2 and crypto binding with server OOM"""
    check_eap_capa(dev[0], "MSCHAPV2")
    params = int_eap_server_params()
    hapd = hostapd.add_ap(apdev[0], params)
    with alloc_fail(hapd, 1, "eap_mschapv2_getKey"):
        eap_connect(dev[0], hapd, "PEAP", "user", password="password",
                    ca_cert="auth_serv/ca.pem",
                    phase1="peapver=0 crypto_binding=2",
                    phase2="auth=MSCHAPV2",
                    expect_failure=True, local_error_report=True)

def test_ap_wpa2_eap_peap_params(dev, apdev):
    """WPA2-Enterprise connection using EAP-PEAPv0/EAP-MSCHAPv2 and various parameters"""
    check_eap_capa(dev[0], "MSCHAPV2")
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "PEAP", "user",
                anonymous_identity="peap", password="password",
                ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2",
                phase1="peapver=0 peaplabel=1",
                expect_failure=True)
    dev[0].request("REMOVE_NETWORK all")
    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="PEAP",
                   identity="user",
                   anonymous_identity="peap", password="password",
                   ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2",
                   phase1="peap_outer_success=0",
                   wait_connect=False, scan_freq="2412")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-SUCCESS"], timeout=15)
    if ev is None:
        raise Exception("No EAP success seen")
    # This won't succeed to connect with peap_outer_success=0, so stop here.
    dev[0].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected()
    eap_connect(dev[1], hapd, "PEAP", "user", password="password",
                ca_cert="auth_serv/ca.pem",
                phase1="peap_outer_success=1",
                phase2="auth=MSCHAPV2")
    eap_connect(dev[2], hapd, "PEAP", "user", password="password",
                ca_cert="auth_serv/ca.pem",
                phase1="peap_outer_success=2",
                phase2="auth=MSCHAPV2")
    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="PEAP",
                   identity="user",
                   anonymous_identity="peap", password="password",
                   ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2",
                   phase1="peapver=1 peaplabel=1",
                   wait_connect=False, scan_freq="2412")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-SUCCESS"], timeout=15)
    if ev is None:
        raise Exception("No EAP success seen")
    ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED",
                            "CTRL-EVENT-DISCONNECTED"], timeout=1)
    if ev and "CTRL-EVENT-CONNECTED" in ev:
        raise Exception("Unexpected connection")
    dev[0].request("REMOVE_NETWORK all")
    dev[0].disconnect_and_stop_scan()

    tests = [("peap-ver0", ""),
             ("peap-ver1", ""),
             ("peap-ver0", "peapver=0"),
             ("peap-ver1", "peapver=1")]
    for anon, phase1 in tests:
        dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="PEAP",
                       identity="user", anonymous_identity=anon,
                       password="password", phase1=phase1,
                       ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2",
                       scan_freq="2412")
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()

    tests = [("peap-ver0", "peapver=1"),
             ("peap-ver1", "peapver=0")]
    for anon, phase1 in tests:
        dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="PEAP",
                       identity="user", anonymous_identity=anon,
                       password="password", phase1=phase1,
                       ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2",
                       wait_connect=False, scan_freq="2412")
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"], timeout=15)
        if ev is None:
            raise Exception("No EAP-Failure seen")
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()

    eap_connect(dev[0], hapd, "PEAP", "user", password="password",
                ca_cert="auth_serv/ca.pem",
                phase1="tls_allow_md5=1 tls_disable_session_ticket=1 tls_disable_tlsv1_0=0 tls_disable_tlsv1_1=0 tls_disable_tlsv1_2=0 tls_ext_cert_check=0",
                phase2="auth=MSCHAPV2")

def test_ap_wpa2_eap_peap_eap_gtc(dev, apdev, params):
    """WPA2-Enterprise connection using EAP-PEAP/EAP-GTC"""
    p = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], p)
    eap_connect(dev[0], hapd, "PEAP", "user", phase1="peapver=1",
                anonymous_identity="peap", password="password",
                ca_cert="auth_serv/ca.pem", phase2="auth=GTC")

def test_ap_wpa2_eap_peap_eap_tls(dev, apdev):
    """WPA2-Enterprise connection using EAP-PEAP/EAP-TLS"""
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "PEAP", "cert user",
                ca_cert="auth_serv/ca.pem", phase2="auth=TLS",
                ca_cert2="auth_serv/ca.pem",
                client_cert2="auth_serv/user.pem",
                private_key2="auth_serv/user.key")
    eap_reauth(dev[0], "PEAP")

def test_ap_wpa2_eap_peap_eap_vendor(dev, apdev):
    """WPA2-Enterprise connection using EAP-PEAP/EAP-vendor"""
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "PEAP", "vendor-test-2",
                ca_cert="auth_serv/ca.pem", phase2="auth=VENDOR-TEST")

def test_ap_wpa2_eap_tls(dev, apdev):
    """WPA2-Enterprise connection using EAP-TLS"""
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "TLS", "tls user", ca_cert="auth_serv/ca.pem",
                client_cert="auth_serv/user.pem",
                private_key="auth_serv/user.key")
    eap_reauth(dev[0], "TLS")

def test_eap_tls_pkcs8_pkcs5_v2_des3(dev, apdev):
    """WPA2-Enterprise connection using EAP-TLS and PKCS #8, PKCS #5 v2 DES3 key"""
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "TLS", "tls user", ca_cert="auth_serv/ca.pem",
                client_cert="auth_serv/user.pem",
                private_key="auth_serv/user.key.pkcs8",
                private_key_passwd="whatever")

def test_eap_tls_pkcs8_pkcs5_v15(dev, apdev):
    """WPA2-Enterprise connection using EAP-TLS and PKCS #8, PKCS #5 v1.5 key"""
    check_pkcs5_v15_support(dev[0])
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "TLS", "tls user", ca_cert="auth_serv/ca.pem",
                client_cert="auth_serv/user.pem",
                private_key="auth_serv/user.key.pkcs8.pkcs5v15",
                private_key_passwd="whatever")

def test_ap_wpa2_eap_tls_blob(dev, apdev):
    """WPA2-Enterprise connection using EAP-TLS and config blobs"""
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    cert = read_pem("auth_serv/ca.pem")
    if "OK" not in dev[0].request("SET blob cacert " +  binascii.hexlify(cert).decode()):
        raise Exception("Could not set cacert blob")
    cert = read_pem("auth_serv/user.pem")
    if "OK" not in dev[0].request("SET blob usercert " + binascii.hexlify(cert).decode()):
        raise Exception("Could not set usercert blob")
    key = read_pem("auth_serv/user.rsa-key")
    if "OK" not in dev[0].request("SET blob userkey " + binascii.hexlify(key).decode()):
        raise Exception("Could not set cacert blob")
    eap_connect(dev[0], hapd, "TLS", "tls user", ca_cert="blob://cacert",
                client_cert="blob://usercert",
                private_key="blob://userkey")

def test_ap_wpa2_eap_tls_blob_pem(dev, apdev):
    """WPA2-Enterprise connection using EAP-TLS and config blobs (PEM)"""
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    cert = read_pem("auth_serv/ca.pem", decode=False)
    if "OK" not in dev[0].request("SET blob cacert " +  binascii.hexlify(cert).decode()):
        raise Exception("Could not set cacert blob")
    cert = read_pem("auth_serv/user.pem", decode=False)
    if "OK" not in dev[0].request("SET blob usercert " + binascii.hexlify(cert).decode()):
        raise Exception("Could not set usercert blob")
    key = read_pem("auth_serv/user.key.pkcs8", decode=False)
    if "OK" not in dev[0].request("SET blob userkey " + binascii.hexlify(key).decode()):
        raise Exception("Could not set cacert blob")
    eap_connect(dev[0], hapd, "TLS", "tls user", ca_cert="blob://cacert",
                client_cert="blob://usercert",
                private_key="blob://userkey",
                private_key_passwd="whatever")

def test_ap_wpa2_eap_tls_blob_missing(dev, apdev):
    """EAP-TLS and config blob missing"""
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hostapd.add_ap(apdev[0], params)
    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TLS",
                   identity="tls user",
                   ca_cert="blob://testing-blob-does-not-exist",
                   client_cert="blob://testing-blob-does-not-exist",
                   private_key="blob://testing-blob-does-not-exist",
                   wait_connect=False, scan_freq="2412")
    ev = dev[0].wait_event(["EAP: Failed to initialize EAP method"], timeout=10)
    if ev is None:
        raise Exception("EAP failure not reported")
    dev[0].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected()

def test_ap_wpa2_eap_tls_with_tls_len(dev, apdev):
    """EAP-TLS and TLS Message Length in unfragmented packets"""
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "TLS", "tls user", ca_cert="auth_serv/ca.pem",
                phase1="include_tls_length=1",
                client_cert="auth_serv/user.pem",
                private_key="auth_serv/user.key")

def test_ap_wpa2_eap_tls_pkcs12(dev, apdev):
    """WPA2-Enterprise connection using EAP-TLS and PKCS#12"""
    check_pkcs12_support(dev[0])
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "TLS", "tls user", ca_cert="auth_serv/ca.pem",
                private_key="auth_serv/user.pkcs12",
                private_key_passwd="whatever")
    dev[0].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected()

    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TLS",
                   identity="tls user",
                   ca_cert="auth_serv/ca.pem",
                   private_key="auth_serv/user.pkcs12",
                   wait_connect=False, scan_freq="2412")
    ev = dev[0].wait_event(["CTRL-REQ-PASSPHRASE"])
    if ev is None:
        raise Exception("Request for private key passphrase timed out")
    id = ev.split(':')[0].split('-')[-1]
    dev[0].request("CTRL-RSP-PASSPHRASE-" + id + ":whatever")
    dev[0].wait_connected(timeout=10)
    dev[0].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected()

    # Run this twice to verify certificate chain handling with OpenSSL. Use two
    # different files to cover both cases of the extra certificate being the
    # one that signed the client certificate and it being unrelated to the
    # client certificate.
    for pkcs12 in "auth_serv/user2.pkcs12", "auth_serv/user3.pkcs12":
        for i in range(2):
            eap_connect(dev[0], hapd, "TLS", "tls user",
                        ca_cert="auth_serv/ca.pem",
                        private_key=pkcs12,
                        private_key_passwd="whatever")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

def test_ap_wpa2_eap_tls_pkcs12_blob(dev, apdev):
    """WPA2-Enterprise connection using EAP-TLS and PKCS#12 from configuration blob"""
    cert = read_pem("auth_serv/ca.pem")
    cacert = binascii.hexlify(cert).decode()
    run_ap_wpa2_eap_tls_pkcs12_blob(dev, apdev, cacert)

def test_ap_wpa2_eap_tls_pkcs12_blob_pem(dev, apdev):
    """WPA2-Enterprise connection using EAP-TLS and PKCS#12 from configuration blob and PEM ca_cert blob"""
    with open("auth_serv/ca.pem", "r") as f:
        lines = f.readlines()
        copy = False
        cert = ""
        for l in lines:
            if "-----BEGIN" in l:
                copy = True
            if copy:
                cert += l
            if "-----END" in l:
                copy = False
                break
    cacert = binascii.hexlify(cert.encode()).decode()
    run_ap_wpa2_eap_tls_pkcs12_blob(dev, apdev, cacert)

def run_ap_wpa2_eap_tls_pkcs12_blob(dev, apdev, cacert):
    check_pkcs12_support(dev[0])
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    if "OK" not in dev[0].request("SET blob cacert " + cacert):
        raise Exception("Could not set cacert blob")
    with open("auth_serv/user.pkcs12", "rb") as f:
        if "OK" not in dev[0].request("SET blob pkcs12 " + binascii.hexlify(f.read()).decode()):
            raise Exception("Could not set pkcs12 blob")
    eap_connect(dev[0], hapd, "TLS", "tls user", ca_cert="blob://cacert",
                private_key="blob://pkcs12",
                private_key_passwd="whatever")

def test_ap_wpa2_eap_tls_neg_incorrect_trust_root(dev, apdev):
    """WPA2-Enterprise negative test - incorrect trust root"""
    check_eap_capa(dev[0], "MSCHAPV2")
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hostapd.add_ap(apdev[0], params)
    cert = read_pem("auth_serv/ca-incorrect.pem")
    if "OK" not in dev[0].request("SET blob cacert " + binascii.hexlify(cert).decode()):
        raise Exception("Could not set cacert blob")
    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TTLS",
                   identity="DOMAIN\mschapv2 user", anonymous_identity="ttls",
                   password="password", phase2="auth=MSCHAPV2",
                   ca_cert="blob://cacert",
                   wait_connect=False, scan_freq="2412")
    dev[1].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TTLS",
                   identity="DOMAIN\mschapv2 user", anonymous_identity="ttls",
                   password="password", phase2="auth=MSCHAPV2",
                   ca_cert="auth_serv/ca-incorrect.pem",
                   wait_connect=False, scan_freq="2412")

    for dev in (dev[0], dev[1]):
        ev = dev.wait_event(["CTRL-EVENT-EAP-STARTED"], timeout=16)
        if ev is None:
            raise Exception("Association and EAP start timed out")

        ev = dev.wait_event(["CTRL-EVENT-EAP-METHOD"], timeout=10)
        if ev is None:
            raise Exception("EAP method selection timed out")
        if "TTLS" not in ev:
            raise Exception("Unexpected EAP method")

        ev = dev.wait_event(["CTRL-EVENT-EAP-TLS-CERT-ERROR",
                             "CTRL-EVENT-EAP-SUCCESS",
                             "CTRL-EVENT-EAP-FAILURE",
                             "CTRL-EVENT-CONNECTED",
                             "CTRL-EVENT-DISCONNECTED"], timeout=10)
        if ev is None:
            raise Exception("EAP result timed out")
        if "CTRL-EVENT-EAP-TLS-CERT-ERROR" not in ev:
            raise Exception("TLS certificate error not reported")

        ev = dev.wait_event(["CTRL-EVENT-EAP-SUCCESS",
                             "CTRL-EVENT-EAP-FAILURE",
                             "CTRL-EVENT-CONNECTED",
                             "CTRL-EVENT-DISCONNECTED"], timeout=10)
        if ev is None:
            raise Exception("EAP result(2) timed out")
        if "CTRL-EVENT-EAP-FAILURE" not in ev:
            raise Exception("EAP failure not reported")

        ev = dev.wait_event(["CTRL-EVENT-CONNECTED",
                             "CTRL-EVENT-DISCONNECTED"], timeout=10)
        if ev is None:
            raise Exception("EAP result(3) timed out")
        if "CTRL-EVENT-DISCONNECTED" not in ev:
            raise Exception("Disconnection not reported")

        ev = dev.wait_event(["CTRL-EVENT-SSID-TEMP-DISABLED"], timeout=10)
        if ev is None:
            raise Exception("Network block disabling not reported")

def test_ap_wpa2_eap_tls_diff_ca_trust(dev, apdev):
    """WPA2-Enterprise connection using EAP-TTLS/PAP and different CA trust"""
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TTLS",
                   identity="pap user", anonymous_identity="ttls",
                   password="password", phase2="auth=PAP",
                   ca_cert="auth_serv/ca.pem",
                   wait_connect=True, scan_freq="2412")
    id = dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TTLS",
                        identity="pap user", anonymous_identity="ttls",
                        password="password", phase2="auth=PAP",
                        ca_cert="auth_serv/ca-incorrect.pem",
                        only_add_network=True, scan_freq="2412")

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()
    dev[0].dump_monitor()
    dev[0].select_network(id, freq="2412")

    ev = dev[0].wait_event(["CTRL-EVENT-EAP-PROPOSED-METHOD vendor=0 method=21"], timeout=15)
    if ev is None:
        raise Exception("EAP-TTLS not re-started")

    ev = dev[0].wait_disconnected(timeout=15)
    if "reason=23" not in ev:
        raise Exception("Proper reason code for disconnection not reported")

def test_ap_wpa2_eap_tls_diff_ca_trust2(dev, apdev):
    """WPA2-Enterprise connection using EAP-TTLS/PAP and different CA trust"""
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TTLS",
                   identity="pap user", anonymous_identity="ttls",
                   password="password", phase2="auth=PAP",
                   wait_connect=True, scan_freq="2412")
    id = dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TTLS",
                        identity="pap user", anonymous_identity="ttls",
                        password="password", phase2="auth=PAP",
                        ca_cert="auth_serv/ca-incorrect.pem",
                        only_add_network=True, scan_freq="2412")

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()
    dev[0].dump_monitor()
    dev[0].select_network(id, freq="2412")

    ev = dev[0].wait_event(["CTRL-EVENT-EAP-PROPOSED-METHOD vendor=0 method=21"], timeout=15)
    if ev is None:
        raise Exception("EAP-TTLS not re-started")

    ev = dev[0].wait_disconnected(timeout=15)
    if "reason=23" not in ev:
        raise Exception("Proper reason code for disconnection not reported")

def test_ap_wpa2_eap_tls_diff_ca_trust3(dev, apdev):
    """WPA2-Enterprise connection using EAP-TTLS/PAP and different CA trust"""
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    id = dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TTLS",
                        identity="pap user", anonymous_identity="ttls",
                        password="password", phase2="auth=PAP",
                        ca_cert="auth_serv/ca.pem",
                        wait_connect=True, scan_freq="2412")
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()
    dev[0].dump_monitor()
    dev[0].set_network_quoted(id, "ca_cert", "auth_serv/ca-incorrect.pem")
    dev[0].select_network(id, freq="2412")

    ev = dev[0].wait_event(["CTRL-EVENT-EAP-PROPOSED-METHOD vendor=0 method=21"], timeout=15)
    if ev is None:
        raise Exception("EAP-TTLS not re-started")

    ev = dev[0].wait_disconnected(timeout=15)
    if "reason=23" not in ev:
        raise Exception("Proper reason code for disconnection not reported")

def test_ap_wpa2_eap_tls_neg_suffix_match(dev, apdev):
    """WPA2-Enterprise negative test - domain suffix mismatch"""
    check_domain_suffix_match(dev[0])
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hostapd.add_ap(apdev[0], params)
    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TTLS",
                   identity="DOMAIN\mschapv2 user", anonymous_identity="ttls",
                   password="password", phase2="auth=MSCHAPV2",
                   ca_cert="auth_serv/ca.pem",
                   domain_suffix_match="incorrect.example.com",
                   wait_connect=False, scan_freq="2412")

    ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED"], timeout=16)
    if ev is None:
        raise Exception("Association and EAP start timed out")

    ev = dev[0].wait_event(["CTRL-EVENT-EAP-METHOD"], timeout=10)
    if ev is None:
        raise Exception("EAP method selection timed out")
    if "TTLS" not in ev:
        raise Exception("Unexpected EAP method")

    ev = dev[0].wait_event(["CTRL-EVENT-EAP-TLS-CERT-ERROR",
                            "CTRL-EVENT-EAP-SUCCESS",
                            "CTRL-EVENT-EAP-FAILURE",
                            "CTRL-EVENT-CONNECTED",
                            "CTRL-EVENT-DISCONNECTED"], timeout=10)
    if ev is None:
        raise Exception("EAP result timed out")
    if "CTRL-EVENT-EAP-TLS-CERT-ERROR" not in ev:
        raise Exception("TLS certificate error not reported")
    if "Domain suffix mismatch" not in ev:
        raise Exception("Domain suffix mismatch not reported")

    ev = dev[0].wait_event(["CTRL-EVENT-EAP-SUCCESS",
                            "CTRL-EVENT-EAP-FAILURE",
                            "CTRL-EVENT-CONNECTED",
                            "CTRL-EVENT-DISCONNECTED"], timeout=10)
    if ev is None:
        raise Exception("EAP result(2) timed out")
    if "CTRL-EVENT-EAP-FAILURE" not in ev:
        raise Exception("EAP failure not reported")

    ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED",
                            "CTRL-EVENT-DISCONNECTED"], timeout=10)
    if ev is None:
        raise Exception("EAP result(3) timed out")
    if "CTRL-EVENT-DISCONNECTED" not in ev:
        raise Exception("Disconnection not reported")

    ev = dev[0].wait_event(["CTRL-EVENT-SSID-TEMP-DISABLED"], timeout=10)
    if ev is None:
        raise Exception("Network block disabling not reported")

def test_ap_wpa2_eap_tls_neg_domain_match(dev, apdev):
    """WPA2-Enterprise negative test - domain mismatch"""
    check_domain_match(dev[0])
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hostapd.add_ap(apdev[0], params)
    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TTLS",
                   identity="DOMAIN\mschapv2 user", anonymous_identity="ttls",
                   password="password", phase2="auth=MSCHAPV2",
                   ca_cert="auth_serv/ca.pem",
                   domain_match="w1.fi",
                   wait_connect=False, scan_freq="2412")

    ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED"], timeout=16)
    if ev is None:
        raise Exception("Association and EAP start timed out")

    ev = dev[0].wait_event(["CTRL-EVENT-EAP-METHOD"], timeout=10)
    if ev is None:
        raise Exception("EAP method selection timed out")
    if "TTLS" not in ev:
        raise Exception("Unexpected EAP method")

    ev = dev[0].wait_event(["CTRL-EVENT-EAP-TLS-CERT-ERROR",
                            "CTRL-EVENT-EAP-SUCCESS",
                            "CTRL-EVENT-EAP-FAILURE",
                            "CTRL-EVENT-CONNECTED",
                            "CTRL-EVENT-DISCONNECTED"], timeout=10)
    if ev is None:
        raise Exception("EAP result timed out")
    if "CTRL-EVENT-EAP-TLS-CERT-ERROR" not in ev:
        raise Exception("TLS certificate error not reported")
    if "Domain mismatch" not in ev:
        raise Exception("Domain mismatch not reported")

    ev = dev[0].wait_event(["CTRL-EVENT-EAP-SUCCESS",
                            "CTRL-EVENT-EAP-FAILURE",
                            "CTRL-EVENT-CONNECTED",
                            "CTRL-EVENT-DISCONNECTED"], timeout=10)
    if ev is None:
        raise Exception("EAP result(2) timed out")
    if "CTRL-EVENT-EAP-FAILURE" not in ev:
        raise Exception("EAP failure not reported")

    ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED",
                            "CTRL-EVENT-DISCONNECTED"], timeout=10)
    if ev is None:
        raise Exception("EAP result(3) timed out")
    if "CTRL-EVENT-DISCONNECTED" not in ev:
        raise Exception("Disconnection not reported")

    ev = dev[0].wait_event(["CTRL-EVENT-SSID-TEMP-DISABLED"], timeout=10)
    if ev is None:
        raise Exception("Network block disabling not reported")

def test_ap_wpa2_eap_tls_neg_subject_match(dev, apdev):
    """WPA2-Enterprise negative test - subject mismatch"""
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hostapd.add_ap(apdev[0], params)
    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TTLS",
                   identity="DOMAIN\mschapv2 user", anonymous_identity="ttls",
                   password="password", phase2="auth=MSCHAPV2",
                   ca_cert="auth_serv/ca.pem",
                   subject_match="/C=FI/O=w1.fi/CN=example.com",
                   wait_connect=False, scan_freq="2412")

    ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED"], timeout=16)
    if ev is None:
        raise Exception("Association and EAP start timed out")

    ev = dev[0].wait_event(["CTRL-EVENT-EAP-METHOD",
                            "EAP: Failed to initialize EAP method"], timeout=10)
    if ev is None:
        raise Exception("EAP method selection timed out")
    if "EAP: Failed to initialize EAP method" in ev:
        tls = dev[0].request("GET tls_library")
        if tls.startswith("OpenSSL"):
            raise Exception("Failed to select EAP method")
        logger.info("subject_match not supported - connection failed, so test succeeded")
        return
    if "TTLS" not in ev:
        raise Exception("Unexpected EAP method")

    ev = dev[0].wait_event(["CTRL-EVENT-EAP-TLS-CERT-ERROR",
                            "CTRL-EVENT-EAP-SUCCESS",
                            "CTRL-EVENT-EAP-FAILURE",
                            "CTRL-EVENT-CONNECTED",
                            "CTRL-EVENT-DISCONNECTED"], timeout=10)
    if ev is None:
        raise Exception("EAP result timed out")
    if "CTRL-EVENT-EAP-TLS-CERT-ERROR" not in ev:
        raise Exception("TLS certificate error not reported")
    if "Subject mismatch" not in ev:
        raise Exception("Subject mismatch not reported")

    ev = dev[0].wait_event(["CTRL-EVENT-EAP-SUCCESS",
                            "CTRL-EVENT-EAP-FAILURE",
                            "CTRL-EVENT-CONNECTED",
                            "CTRL-EVENT-DISCONNECTED"], timeout=10)
    if ev is None:
        raise Exception("EAP result(2) timed out")
    if "CTRL-EVENT-EAP-FAILURE" not in ev:
        raise Exception("EAP failure not reported")

    ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED",
                            "CTRL-EVENT-DISCONNECTED"], timeout=10)
    if ev is None:
        raise Exception("EAP result(3) timed out")
    if "CTRL-EVENT-DISCONNECTED" not in ev:
        raise Exception("Disconnection not reported")

    ev = dev[0].wait_event(["CTRL-EVENT-SSID-TEMP-DISABLED"], timeout=10)
    if ev is None:
        raise Exception("Network block disabling not reported")

def test_ap_wpa2_eap_tls_neg_altsubject_match(dev, apdev):
    """WPA2-Enterprise negative test - altsubject mismatch"""
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hostapd.add_ap(apdev[0], params)

    tests = ["incorrect.example.com",
             "DNS:incorrect.example.com",
             "DNS:w1.fi",
             "DNS:erver.w1.fi"]
    for match in tests:
        _test_ap_wpa2_eap_tls_neg_altsubject_match(dev, apdev, match)

def _test_ap_wpa2_eap_tls_neg_altsubject_match(dev, apdev, match):
    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TTLS",
                   identity="DOMAIN\mschapv2 user", anonymous_identity="ttls",
                   password="password", phase2="auth=MSCHAPV2",
                   ca_cert="auth_serv/ca.pem",
                   altsubject_match=match,
                   wait_connect=False, scan_freq="2412")

    ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED"], timeout=16)
    if ev is None:
        raise Exception("Association and EAP start timed out")

    ev = dev[0].wait_event(["CTRL-EVENT-EAP-METHOD",
                            "EAP: Failed to initialize EAP method"], timeout=10)
    if ev is None:
        raise Exception("EAP method selection timed out")
    if "EAP: Failed to initialize EAP method" in ev:
        tls = dev[0].request("GET tls_library")
        if tls.startswith("OpenSSL"):
            raise Exception("Failed to select EAP method")
        logger.info("altsubject_match not supported - connection failed, so test succeeded")
        return
    if "TTLS" not in ev:
        raise Exception("Unexpected EAP method")

    ev = dev[0].wait_event(["CTRL-EVENT-EAP-TLS-CERT-ERROR",
                            "CTRL-EVENT-EAP-SUCCESS",
                            "CTRL-EVENT-EAP-FAILURE",
                            "CTRL-EVENT-CONNECTED",
                            "CTRL-EVENT-DISCONNECTED"], timeout=10)
    if ev is None:
        raise Exception("EAP result timed out")
    if "CTRL-EVENT-EAP-TLS-CERT-ERROR" not in ev:
        raise Exception("TLS certificate error not reported")
    if "AltSubject mismatch" not in ev:
        raise Exception("altsubject mismatch not reported")

    ev = dev[0].wait_event(["CTRL-EVENT-EAP-SUCCESS",
                            "CTRL-EVENT-EAP-FAILURE",
                            "CTRL-EVENT-CONNECTED",
                            "CTRL-EVENT-DISCONNECTED"], timeout=10)
    if ev is None:
        raise Exception("EAP result(2) timed out")
    if "CTRL-EVENT-EAP-FAILURE" not in ev:
        raise Exception("EAP failure not reported")

    ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED",
                            "CTRL-EVENT-DISCONNECTED"], timeout=10)
    if ev is None:
        raise Exception("EAP result(3) timed out")
    if "CTRL-EVENT-DISCONNECTED" not in ev:
        raise Exception("Disconnection not reported")

    ev = dev[0].wait_event(["CTRL-EVENT-SSID-TEMP-DISABLED"], timeout=10)
    if ev is None:
        raise Exception("Network block disabling not reported")

    dev[0].request("REMOVE_NETWORK all")

def test_ap_wpa2_eap_unauth_tls(dev, apdev):
    """WPA2-Enterprise connection using UNAUTH-TLS"""
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "UNAUTH-TLS", "unauth-tls",
                ca_cert="auth_serv/ca.pem")
    eap_reauth(dev[0], "UNAUTH-TLS")

def test_ap_wpa2_eap_ttls_server_cert_hash(dev, apdev):
    """WPA2-Enterprise connection using EAP-TTLS and server certificate hash"""
    check_cert_probe_support(dev[0])
    skip_with_fips(dev[0])
    srv_cert_hash = "5891bd91eaf977684e70d4376d1514621d18f09ab2020bea1ad293d59a6e8944"
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TTLS",
                   identity="probe", ca_cert="probe://",
                   wait_connect=False, scan_freq="2412")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED"], timeout=16)
    if ev is None:
        raise Exception("Association and EAP start timed out")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-PEER-CERT depth=0"], timeout=10)
    if ev is None:
        raise Exception("No peer server certificate event seen")
    if "hash=" + srv_cert_hash not in ev:
        raise Exception("Expected server certificate hash not reported")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-TLS-CERT-ERROR"], timeout=10)
    if ev is None:
        raise Exception("EAP result timed out")
    if "Server certificate chain probe" not in ev:
        raise Exception("Server certificate probe not reported")
    dev[0].wait_disconnected(timeout=10)
    dev[0].request("REMOVE_NETWORK all")

    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TTLS",
                   identity="DOMAIN\mschapv2 user", anonymous_identity="ttls",
                   password="password", phase2="auth=MSCHAPV2",
                   ca_cert="hash://server/sha256/5a1bc1296205e6fdbe3979728efe3920798885c1c4590b5f90f43222d239ca6a",
                   wait_connect=False, scan_freq="2412")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED"], timeout=16)
    if ev is None:
        raise Exception("Association and EAP start timed out")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-TLS-CERT-ERROR"], timeout=10)
    if ev is None:
        raise Exception("EAP result timed out")
    if "Server certificate mismatch" not in ev:
        raise Exception("Server certificate mismatch not reported")
    dev[0].wait_disconnected(timeout=10)
    dev[0].request("REMOVE_NETWORK all")

    eap_connect(dev[0], hapd, "TTLS", "DOMAIN\mschapv2 user",
                anonymous_identity="ttls", password="password",
                ca_cert="hash://server/sha256/" + srv_cert_hash,
                phase2="auth=MSCHAPV2")

def test_ap_wpa2_eap_ttls_server_cert_hash_invalid(dev, apdev):
    """WPA2-Enterprise connection using EAP-TTLS and server certificate hash (invalid config)"""
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hostapd.add_ap(apdev[0], params)
    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TTLS",
                   identity="DOMAIN\mschapv2 user", anonymous_identity="ttls",
                   password="password", phase2="auth=MSCHAPV2",
                   ca_cert="hash://server/md5/5a1bc1296205e6fdbe3979728efe3920798885c1c4590b5f90f43222d239ca6a",
                   wait_connect=False, scan_freq="2412")
    dev[1].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TTLS",
                   identity="DOMAIN\mschapv2 user", anonymous_identity="ttls",
                   password="password", phase2="auth=MSCHAPV2",
                   ca_cert="hash://server/sha256/5a1bc1296205e6fdbe3979728efe3920798885c1c4590b5f90f43222d239ca",
                   wait_connect=False, scan_freq="2412")
    dev[2].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TTLS",
                   identity="DOMAIN\mschapv2 user", anonymous_identity="ttls",
                   password="password", phase2="auth=MSCHAPV2",
                   ca_cert="hash://server/sha256/5a1bc1296205e6fdbe3979728efe3920798885c1c4590b5f90f43222d239ca6Q",
                   wait_connect=False, scan_freq="2412")
    for i in range(0, 3):
        ev = dev[i].wait_event(["CTRL-EVENT-EAP-STARTED"], timeout=16)
        if ev is None:
            raise Exception("Association and EAP start timed out")
        ev = dev[i].wait_event(["EAP: Failed to initialize EAP method: vendor 0 method 21 (TTLS)"], timeout=5)
        if ev is None:
            raise Exception("Did not report EAP method initialization failure")

def test_ap_wpa2_eap_pwd(dev, apdev):
    """WPA2-Enterprise connection using EAP-pwd"""
    check_eap_capa(dev[0], "PWD")
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "PWD", "pwd user", password="secret password")
    eap_reauth(dev[0], "PWD")
    dev[0].request("REMOVE_NETWORK all")

    eap_connect(dev[1], hapd, "PWD",
                "pwd.user@test123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890.example.com",
                password="secret password",
                fragment_size="90")

    logger.info("Negative test with incorrect password")
    eap_connect(dev[2], hapd, "PWD", "pwd user", password="secret-password",
                expect_failure=True, local_error_report=True)

    eap_connect(dev[0], hapd, "PWD",
                "pwd.user@test123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890.example.com",
                password="secret password",
                fragment_size="31")

def test_ap_wpa2_eap_pwd_nthash(dev, apdev):
    """WPA2-Enterprise connection using EAP-pwd and NTHash"""
    check_eap_capa(dev[0], "PWD")
    skip_with_fips(dev[0])
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "PWD", "pwd-hash", password="secret password")
    eap_connect(dev[1], hapd, "PWD", "pwd-hash",
                password_hex="hash:e3718ece8ab74792cbbfffd316d2d19a")
    eap_connect(dev[2], hapd, "PWD", "pwd user",
                password_hex="hash:e3718ece8ab74792cbbfffd316d2d19a",
                expect_failure=True, local_error_report=True)

def test_ap_wpa2_eap_pwd_salt_sha1(dev, apdev):
    """WPA2-Enterprise connection using EAP-pwd and salted password SHA-1"""
    check_eap_capa(dev[0], "PWD")
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "PWD", "pwd-hash-sha1",
                password="secret password")

def test_ap_wpa2_eap_pwd_salt_sha256(dev, apdev):
    """WPA2-Enterprise connection using EAP-pwd and salted password SHA256"""
    check_eap_capa(dev[0], "PWD")
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "PWD", "pwd-hash-sha256",
                password="secret password")

def test_ap_wpa2_eap_pwd_salt_sha512(dev, apdev):
    """WPA2-Enterprise connection using EAP-pwd and salted password SHA512"""
    check_eap_capa(dev[0], "PWD")
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "PWD", "pwd-hash-sha512",
                password="secret password")

def test_ap_wpa2_eap_pwd_groups(dev, apdev):
    """WPA2-Enterprise connection using various EAP-pwd groups"""
    check_eap_capa(dev[0], "PWD")
    tls = dev[0].request("GET tls_library")
    params = {"ssid": "test-wpa2-eap", "wpa": "2", "wpa_key_mgmt": "WPA-EAP",
              "rsn_pairwise": "CCMP", "ieee8021x": "1",
              "eap_server": "1", "eap_user_file": "auth_serv/eap_user.conf"}
    groups = [19, 20, 21]
    for i in groups:
        logger.info("Group %d" % i)
        params['pwd_group'] = str(i)
        hapd = hostapd.add_ap(apdev[0], params)
        eap_connect(dev[0], hapd, "PWD", "pwd user",
                    password="secret password",
                    phase1="eap_pwd_groups=0-65535")
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()
        dev[0].dump_monitor()
        hapd.disable()

def test_ap_wpa2_eap_pwd_invalid_group(dev, apdev):
    """WPA2-Enterprise connection using invalid EAP-pwd group"""
    check_eap_capa(dev[0], "PWD")
    params = {"ssid": "test-wpa2-eap", "wpa": "2", "wpa_key_mgmt": "WPA-EAP",
              "rsn_pairwise": "CCMP", "ieee8021x": "1",
              "eap_server": "1", "eap_user_file": "auth_serv/eap_user.conf"}
    for i in [0, 25, 26, 27]:
        logger.info("Group %d" % i)
        params['pwd_group'] = str(i)
        hapd = hostapd.add_ap(apdev[0], params)
        dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="PWD",
                       identity="pwd user", password="secret password",
                       phase1="eap_pwd_groups=0-65535",
                       scan_freq="2412", wait_connect=False)
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"])
        if ev is None:
            raise Exception("Timeout on EAP failure report (group %d)" % i)
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()
        dev[0].dump_monitor()
        hapd.disable()

def test_ap_wpa2_eap_pwd_disabled_group(dev, apdev):
    """WPA2-Enterprise connection using disabled EAP-pwd group"""
    check_eap_capa(dev[0], "PWD")
    params = {"ssid": "test-wpa2-eap", "wpa": "2", "wpa_key_mgmt": "WPA-EAP",
              "rsn_pairwise": "CCMP", "ieee8021x": "1",
              "eap_server": "1", "eap_user_file": "auth_serv/eap_user.conf"}
    for i in [19, 21]:
        logger.info("Group %d" % i)
        params['pwd_group'] = str(i)
        hapd = hostapd.add_ap(apdev[0], params)
        dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="PWD",
                       identity="pwd user", password="secret password",
                       phase1="eap_pwd_groups=20",
                       scan_freq="2412", wait_connect=False)
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"])
        if ev is None:
            raise Exception("Timeout on EAP failure report (group %d)" % i)
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()
        dev[0].dump_monitor()
        hapd.disable()

    params['pwd_group'] = "20"
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="PWD",
                   identity="pwd user", password="secret password",
                   phase1="eap_pwd_groups=20",
                   scan_freq="2412")

def test_ap_wpa2_eap_pwd_as_frag(dev, apdev):
    """WPA2-Enterprise connection using EAP-pwd with server fragmentation"""
    check_eap_capa(dev[0], "PWD")
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    params = {"ssid": "test-wpa2-eap", "wpa": "2", "wpa_key_mgmt": "WPA-EAP",
              "rsn_pairwise": "CCMP", "ieee8021x": "1",
              "eap_server": "1", "eap_user_file": "auth_serv/eap_user.conf",
              "pwd_group": "19", "fragment_size": "40"}
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "PWD", "pwd user", password="secret password")

def test_ap_wpa2_eap_gpsk(dev, apdev):
    """WPA2-Enterprise connection using EAP-GPSK"""
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    id = eap_connect(dev[0], hapd, "GPSK", "gpsk user",
                     password="abcdefghijklmnop0123456789abcdef")
    eap_reauth(dev[0], "GPSK")

    logger.info("Test forced algorithm selection")
    for phase1 in ["cipher=1", "cipher=2"]:
        dev[0].set_network_quoted(id, "phase1", phase1)
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-SUCCESS"], timeout=10)
        if ev is None:
            raise Exception("EAP success timed out")
        dev[0].wait_connected(timeout=10)

    logger.info("Test failed algorithm negotiation")
    dev[0].set_network_quoted(id, "phase1", "cipher=9")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"], timeout=10)
    if ev is None:
        raise Exception("EAP failure timed out")

    logger.info("Negative test with incorrect password")
    dev[0].request("REMOVE_NETWORK all")
    eap_connect(dev[0], hapd, "GPSK", "gpsk user",
                password="ffcdefghijklmnop0123456789abcdef",
                expect_failure=True)

def test_ap_wpa2_eap_sake(dev, apdev):
    """WPA2-Enterprise connection using EAP-SAKE"""
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "SAKE", "sake user",
                password_hex="0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef")
    eap_reauth(dev[0], "SAKE")

    logger.info("Negative test with incorrect password")
    dev[0].request("REMOVE_NETWORK all")
    eap_connect(dev[0], hapd, "SAKE", "sake user",
                password_hex="ff23456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",
                expect_failure=True)

def test_ap_wpa2_eap_eke(dev, apdev):
    """WPA2-Enterprise connection using EAP-EKE"""
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    id = eap_connect(dev[0], hapd, "EKE", "eke user", password="hello")
    eap_reauth(dev[0], "EKE")

    logger.info("Test forced algorithm selection")
    for phase1 in ["dhgroup=5 encr=1 prf=2 mac=2",
                   "dhgroup=4 encr=1 prf=2 mac=2",
                   "dhgroup=3 encr=1 prf=2 mac=2",
                   "dhgroup=3 encr=1 prf=1 mac=1"]:
        dev[0].set_network_quoted(id, "phase1", phase1)
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-SUCCESS"], timeout=10)
        if ev is None:
            raise Exception("EAP success timed out")
        dev[0].wait_connected(timeout=10)
    dev[0].dump_monitor()

    logger.info("Test failed algorithm negotiation")
    dev[0].set_network_quoted(id, "phase1", "dhgroup=9 encr=9 prf=9 mac=9")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"], timeout=10)
    if ev is None:
        raise Exception("EAP failure timed out")
    dev[0].dump_monitor()

    logger.info("Test unsupported algorithm proposals")
    dev[0].request("REMOVE_NETWORK all")
    dev[0].dump_monitor()
    eap_connect(dev[0], hapd, "EKE", "eke user", password="hello",
                phase1="dhgroup=2 encr=1 prf=1 mac=1", expect_failure=True)
    dev[0].request("REMOVE_NETWORK all")
    dev[0].dump_monitor()
    eap_connect(dev[0], hapd, "EKE", "eke user", password="hello",
                phase1="dhgroup=1 encr=1 prf=1 mac=1", expect_failure=True)

    logger.info("Negative test with incorrect password")
    dev[0].request("REMOVE_NETWORK all")
    eap_connect(dev[0], hapd, "EKE", "eke user", password="hello1",
                expect_failure=True)

@long_duration_test
def test_ap_wpa2_eap_eke_many(dev, apdev):
    """WPA2-Enterprise connection using EAP-EKE (many connections)"""
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hostapd.add_ap(apdev[0], params)
    success = 0
    fail = 0
    for i in range(100):
        for j in range(3):
            dev[j].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="EKE",
                           identity="eke user", password="hello",
                           phase1="dhgroup=3 encr=1 prf=1 mac=1",
                           scan_freq="2412", wait_connect=False)
        for j in range(3):
            ev = dev[j].wait_event(["CTRL-EVENT-CONNECTED",
                                    "CTRL-EVENT-DISCONNECTED"], timeout=15)
            if ev is None:
                raise Exception("No connected/disconnected event")
            if "CTRL-EVENT-DISCONNECTED" in ev:
                fail += 1
                # The RADIUS server limits on active sessions can be hit when
                # going through this test case, so try to give some more time
                # for the server to remove sessions.
                logger.info("Failed to connect i=%d j=%d" % (i, j))
                dev[j].request("REMOVE_NETWORK all")
                time.sleep(1)
            else:
                success += 1
                dev[j].request("REMOVE_NETWORK all")
                dev[j].wait_disconnected()
            dev[j].dump_monitor()
    logger.info("Total success=%d failure=%d" % (success, fail))

def test_ap_wpa2_eap_eke_serverid_nai(dev, apdev):
    """WPA2-Enterprise connection using EAP-EKE with serverid NAI"""
    params = int_eap_server_params()
    params['server_id'] = 'example.server@w1.fi'
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "EKE", "eke user", password="hello")

def test_ap_wpa2_eap_eke_server_oom(dev, apdev):
    """WPA2-Enterprise connection using EAP-EKE with server OOM"""
    params = int_eap_server_params()
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].scan_for_bss(apdev[0]['bssid'], freq=2412)

    for count, func in [(1, "eap_eke_build_commit"),
                        (2, "eap_eke_build_commit"),
                        (3, "eap_eke_build_commit"),
                        (1, "eap_eke_build_confirm"),
                        (2, "eap_eke_build_confirm"),
                        (1, "eap_eke_process_commit"),
                        (2, "eap_eke_process_commit"),
                        (1, "eap_eke_process_confirm"),
                        (1, "eap_eke_process_identity"),
                        (2, "eap_eke_process_identity"),
                        (3, "eap_eke_process_identity"),
                        (4, "eap_eke_process_identity")]:
        with alloc_fail(hapd, count, func):
            eap_connect(dev[0], hapd, "EKE", "eke user", password="hello",
                        expect_failure=True)
            dev[0].request("REMOVE_NETWORK all")

    for count, func, pw in [(1, "eap_eke_init", "hello"),
                            (1, "eap_eke_get_session_id", "hello"),
                            (1, "eap_eke_getKey", "hello"),
                            (1, "eap_eke_build_msg", "hello"),
                            (1, "eap_eke_build_failure", "wrong"),
                            (1, "eap_eke_build_identity", "hello"),
                            (2, "eap_eke_build_identity", "hello")]:
        with alloc_fail(hapd, count, func):
            dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP WPA-EAP-SHA256",
                           eap="EKE", identity="eke user", password=pw,
                           wait_connect=False, scan_freq="2412")
            # This would eventually time out, but we can stop after having
            # reached the allocation failure.
            for i in range(20):
                time.sleep(0.1)
                if hapd.request("GET_ALLOC_FAIL").startswith('0'):
                    break
            dev[0].request("REMOVE_NETWORK all")

    for count in range(1, 1000):
        try:
            with alloc_fail(hapd, count, "eap_server_sm_step"):
                dev[0].connect("test-wpa2-eap",
                               key_mgmt="WPA-EAP WPA-EAP-SHA256",
                               eap="EKE", identity="eke user", password=pw,
                               wait_connect=False, scan_freq="2412")
                # This would eventually time out, but we can stop after having
                # reached the allocation failure.
                for i in range(10):
                    time.sleep(0.1)
                    if hapd.request("GET_ALLOC_FAIL").startswith('0'):
                        break
                dev[0].request("REMOVE_NETWORK all")
        except Exception as e:
            if str(e) == "Allocation failure did not trigger":
                if count < 30:
                    raise Exception("Too few allocation failures")
                logger.info("%d allocation failures tested" % (count - 1))
                break
            raise e

def test_ap_wpa2_eap_ikev2(dev, apdev):
    """WPA2-Enterprise connection using EAP-IKEv2"""
    check_eap_capa(dev[0], "IKEV2")
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "IKEV2", "ikev2 user",
                password="ike password")
    eap_reauth(dev[0], "IKEV2")
    dev[0].request("REMOVE_NETWORK all")
    eap_connect(dev[0], hapd, "IKEV2", "ikev2 user",
                password="ike password", fragment_size="50")

    logger.info("Negative test with incorrect password")
    dev[0].request("REMOVE_NETWORK all")
    eap_connect(dev[0], hapd, "IKEV2", "ikev2 user",
                password="ike-password", expect_failure=True)
    dev[0].request("REMOVE_NETWORK all")

    eap_connect(dev[0], hapd, "IKEV2", "ikev2 user",
                password="ike password", fragment_size="0")
    dev[0].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected()

def test_ap_wpa2_eap_ikev2_as_frag(dev, apdev):
    """WPA2-Enterprise connection using EAP-IKEv2 with server fragmentation"""
    check_eap_capa(dev[0], "IKEV2")
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    params = {"ssid": "test-wpa2-eap", "wpa": "2", "wpa_key_mgmt": "WPA-EAP",
              "rsn_pairwise": "CCMP", "ieee8021x": "1",
              "eap_server": "1", "eap_user_file": "auth_serv/eap_user.conf",
              "fragment_size": "50"}
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "IKEV2", "ikev2 user",
                password="ike password")
    eap_reauth(dev[0], "IKEV2")

def test_ap_wpa2_eap_ikev2_oom(dev, apdev):
    """WPA2-Enterprise connection using EAP-IKEv2 and OOM"""
    check_eap_capa(dev[0], "IKEV2")
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hostapd.add_ap(apdev[0], params)

    tests = [(1, "dh_init"),
             (2, "dh_init"),
             (1, "dh_derive_shared")]
    for count, func in tests:
        with alloc_fail(dev[0], count, func):
            dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="IKEV2",
                           identity="ikev2 user", password="ike password",
                           wait_connect=False, scan_freq="2412")
            ev = dev[0].wait_event(["CTRL-EVENT-EAP-METHOD"], timeout=5)
            if ev is None:
                raise Exception("EAP method not selected")
            for i in range(10):
                if "0:" in dev[0].request("GET_ALLOC_FAIL"):
                    break
                time.sleep(0.02)
            dev[0].request("REMOVE_NETWORK all")

    tls = dev[0].request("GET tls_library")
    if not tls.startswith("wolfSSL"):
        tests = [(1, "os_get_random;dh_init")]
    else:
        tests = [(1, "crypto_dh_init;dh_init")]
    for count, func in tests:
        with fail_test(dev[0], count, func):
            dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="IKEV2",
                           identity="ikev2 user", password="ike password",
                           wait_connect=False, scan_freq="2412")
            ev = dev[0].wait_event(["CTRL-EVENT-EAP-METHOD"], timeout=5)
            if ev is None:
                raise Exception("EAP method not selected")
            for i in range(10):
                if "0:" in dev[0].request("GET_FAIL"):
                    break
                time.sleep(0.02)
            dev[0].request("REMOVE_NETWORK all")

def test_ap_wpa2_eap_pax(dev, apdev):
    """WPA2-Enterprise connection using EAP-PAX"""
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "PAX", "pax.user@example.com",
                password_hex="0123456789abcdef0123456789abcdef")
    eap_reauth(dev[0], "PAX")

    logger.info("Negative test with incorrect password")
    dev[0].request("REMOVE_NETWORK all")
    eap_connect(dev[0], hapd, "PAX", "pax.user@example.com",
                password_hex="ff23456789abcdef0123456789abcdef",
                expect_failure=True)

def test_ap_wpa2_eap_psk(dev, apdev):
    """WPA2-Enterprise connection using EAP-PSK"""
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    params["wpa_key_mgmt"] = "WPA-EAP-SHA256"
    params["ieee80211w"] = "2"
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "PSK", "psk.user@example.com",
                password_hex="0123456789abcdef0123456789abcdef", sha256=True)
    eap_reauth(dev[0], "PSK", sha256=True)
    check_mib(dev[0], [("dot11RSNAAuthenticationSuiteRequested", "00-0f-ac-5"),
                       ("dot11RSNAAuthenticationSuiteSelected", "00-0f-ac-5")])

    bss = dev[0].get_bss(apdev[0]['bssid'])
    if 'flags' not in bss:
        raise Exception("Could not get BSS flags from BSS table")
    if "[WPA2-EAP-SHA256-CCMP]" not in bss['flags']:
        raise Exception("Unexpected BSS flags: " + bss['flags'])

    logger.info("Negative test with incorrect password")
    dev[0].request("REMOVE_NETWORK all")
    eap_connect(dev[0], hapd, "PSK", "psk.user@example.com",
                password_hex="ff23456789abcdef0123456789abcdef", sha256=True,
                expect_failure=True)

def test_ap_wpa2_eap_psk_oom(dev, apdev):
    """WPA2-Enterprise connection using EAP-PSK and OOM"""
    skip_with_fips(dev[0])
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hostapd.add_ap(apdev[0], params)
    tests = [(1, "=aes_128_eax_encrypt"),
             (1, "=aes_128_eax_decrypt")]
    for count, func in tests:
        with alloc_fail(dev[0], count, func):
            dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="PSK",
                           identity="psk.user@example.com",
                           password_hex="0123456789abcdef0123456789abcdef",
                           wait_connect=False, scan_freq="2412")
            ev = dev[0].wait_event(["CTRL-EVENT-EAP-METHOD"], timeout=5)
            if ev is None:
                raise Exception("EAP method not selected")
            wait_fail_trigger(dev[0], "GET_ALLOC_FAIL",
                              note="Failure not triggered: %d:%s" % (count, func))
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

    tests = [(1, "aes_ctr_encrypt;aes_128_eax_encrypt"),
             (1, "omac1_aes_128;aes_128_eax_encrypt"),
             (2, "omac1_aes_128;aes_128_eax_encrypt"),
             (3, "omac1_aes_128;aes_128_eax_encrypt"),
             (1, "omac1_aes_vector"),
             (1, "omac1_aes_128;aes_128_eax_decrypt"),
             (2, "omac1_aes_128;aes_128_eax_decrypt"),
             (3, "omac1_aes_128;aes_128_eax_decrypt"),
             (1, "aes_ctr_encrypt;aes_128_eax_decrypt")]
    for count, func in tests:
        with fail_test(dev[0], count, func):
            dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="PSK",
                           identity="psk.user@example.com",
                           password_hex="0123456789abcdef0123456789abcdef",
                           wait_connect=False, scan_freq="2412")
            ev = dev[0].wait_event(["CTRL-EVENT-EAP-METHOD"], timeout=5)
            if ev is None:
                raise Exception("EAP method not selected")
            wait_fail_trigger(dev[0], "GET_FAIL",
                              note="Failure not triggered: %d:%s" % (count, func))
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

    with fail_test(dev[0], 1, "aes_128_encrypt_block"):
            dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="PSK",
                           identity="psk.user@example.com",
                           password_hex="0123456789abcdef0123456789abcdef",
                           wait_connect=False, scan_freq="2412")
            ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"], timeout=10)
            if ev is None:
                raise Exception("EAP method failure not reported")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

def test_ap_wpa_eap_peap_eap_mschapv2(dev, apdev):
    """WPA-Enterprise connection using EAP-PEAP/EAP-MSCHAPv2"""
    skip_without_tkip(dev[0])
    check_eap_capa(dev[0], "MSCHAPV2")
    params = hostapd.wpa_eap_params(ssid="test-wpa-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].connect("test-wpa-eap", key_mgmt="WPA-EAP", eap="PEAP",
                   identity="user", password="password", phase2="auth=MSCHAPV2",
                   ca_cert="auth_serv/ca.pem", wait_connect=False,
                   scan_freq="2412")
    eap_check_auth(dev[0], "PEAP", True, rsn=False)
    hapd.wait_sta()
    hwsim_utils.test_connectivity(dev[0], hapd)
    eap_reauth(dev[0], "PEAP", rsn=False)
    check_mib(dev[0], [("dot11RSNAAuthenticationSuiteRequested", "00-50-f2-1"),
                       ("dot11RSNAAuthenticationSuiteSelected", "00-50-f2-1")])
    status = dev[0].get_status(extra="VERBOSE")
    if 'portControl' not in status:
        raise Exception("portControl missing from STATUS-VERBOSE")
    if status['portControl'] != 'Auto':
        raise Exception("Unexpected portControl value: " + status['portControl'])
    if 'eap_session_id' not in status:
        raise Exception("eap_session_id missing from STATUS-VERBOSE")
    if not status['eap_session_id'].startswith("19"):
        raise Exception("Unexpected eap_session_id value: " + status['eap_session_id'])

def test_ap_wpa2_eap_interactive(dev, apdev):
    """WPA2-Enterprise connection using interactive identity/password entry"""
    check_eap_capa(dev[0], "MSCHAPV2")
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)

    tests = [("Connection with dynamic TTLS/MSCHAPv2 password entry",
              "TTLS", "ttls", "DOMAIN\mschapv2 user", "auth=MSCHAPV2",
              None, "password"),
             ("Connection with dynamic TTLS/MSCHAPv2 identity and password entry",
              "TTLS", "ttls", None, "auth=MSCHAPV2",
              "DOMAIN\mschapv2 user", "password"),
             ("Connection with dynamic TTLS/EAP-MSCHAPv2 password entry",
              "TTLS", "ttls", "user", "autheap=MSCHAPV2", None, "password"),
             ("Connection with dynamic TTLS/EAP-MD5 password entry",
              "TTLS", "ttls", "user", "autheap=MD5", None, "password"),
             ("Connection with dynamic PEAP/EAP-MSCHAPv2 password entry",
              "PEAP", None, "user", "auth=MSCHAPV2", None, "password"),
             ("Connection with dynamic PEAP/EAP-GTC password entry",
              "PEAP", None, "user", "auth=GTC", None, "password")]
    for [desc, eap, anon, identity, phase2, req_id, req_pw] in tests:
        logger.info(desc)
        dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap=eap,
                       anonymous_identity=anon, identity=identity,
                       ca_cert="auth_serv/ca.pem", phase2=phase2,
                       wait_connect=False, scan_freq="2412")
        if req_id:
            ev = dev[0].wait_event(["CTRL-REQ-IDENTITY"])
            if ev is None:
                raise Exception("Request for identity timed out")
            id = ev.split(':')[0].split('-')[-1]
            dev[0].request("CTRL-RSP-IDENTITY-" + id + ":" + req_id)
        ev = dev[0].wait_event(["CTRL-REQ-PASSWORD", "CTRL-REQ-OTP"])
        if ev is None:
            raise Exception("Request for password timed out")
        id = ev.split(':')[0].split('-')[-1]
        type = "OTP" if "CTRL-REQ-OTP" in ev else "PASSWORD"
        dev[0].request("CTRL-RSP-" + type + "-" + id + ":" + req_pw)
        dev[0].wait_connected(timeout=10)
        dev[0].request("REMOVE_NETWORK all")

def test_ap_wpa2_eap_ext_enable_network_while_connected(dev, apdev):
    """WPA2-Enterprise interactive identity entry and ENABLE_NETWORK"""
    check_eap_capa(dev[0], "MSCHAPV2")
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)

    id_other = dev[0].connect("other", key_mgmt="NONE", scan_freq="2412",
                              only_add_network=True)

    req_id = "DOMAIN\mschapv2 user"
    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TTLS",
                   anonymous_identity="ttls", identity=None,
                   password="password",
                   ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2",
                   wait_connect=False, scan_freq="2412")
    ev = dev[0].wait_event(["CTRL-REQ-IDENTITY"])
    if ev is None:
        raise Exception("Request for identity timed out")
    id = ev.split(':')[0].split('-')[-1]
    dev[0].request("CTRL-RSP-IDENTITY-" + id + ":" + req_id)
    dev[0].wait_connected(timeout=10)

    if "OK" not in dev[0].request("ENABLE_NETWORK " + str(id_other)):
        raise Exception("Failed to enable network")
    ev = dev[0].wait_event(["SME: Trying to authenticate"], timeout=1)
    if ev is not None:
        raise Exception("Unexpected reconnection attempt on ENABLE_NETWORK")
    dev[0].request("REMOVE_NETWORK all")

def test_ap_wpa2_eap_vendor_test(dev, apdev):
    """WPA2-Enterprise connection using EAP vendor test"""
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "VENDOR-TEST", "vendor-test")
    eap_reauth(dev[0], "VENDOR-TEST")
    eap_connect(dev[1], hapd, "VENDOR-TEST", "vendor-test",
                password="pending")

def test_ap_wpa2_eap_vendor_test_oom(dev, apdev):
    """WPA2-Enterprise connection using EAP vendor test (OOM)"""
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hostapd.add_ap(apdev[0], params)

    tests = ["eap_vendor_test_init",
             "eap_msg_alloc;eap_vendor_test_process",
             "eap_vendor_test_getKey"]
    for func in tests:
        with alloc_fail(dev[0], 1, func):
            dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP",
                           scan_freq="2412",
                           eap="VENDOR-TEST", identity="vendor-test",
                           wait_connect=False)
            wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

def test_ap_wpa2_eap_fast_mschapv2_unauth_prov(dev, apdev):
    """WPA2-Enterprise connection using EAP-FAST/MSCHAPv2 and unauthenticated provisioning"""
    check_eap_capa(dev[0], "FAST")
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "FAST", "user",
                anonymous_identity="FAST", password="password",
                ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2",
                phase1="fast_provisioning=1", pac_file="blob://fast_pac")
    hwsim_utils.test_connectivity(dev[0], hapd)
    res = eap_reauth(dev[0], "FAST")
    if res['tls_session_reused'] != '1':
        raise Exception("EAP-FAST could not use PAC session ticket")

def test_ap_wpa2_eap_fast_pac_file(dev, apdev, params):
    """WPA2-Enterprise connection using EAP-FAST/MSCHAPv2 and PAC file"""
    check_eap_capa(dev[0], "FAST")
    pac_file = os.path.join(params['logdir'], "fast.pac")
    pac_file2 = os.path.join(params['logdir'], "fast-bin.pac")
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)

    try:
        eap_connect(dev[0], hapd, "FAST", "user",
                    anonymous_identity="FAST", password="password",
                    ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2",
                    phase1="fast_provisioning=1", pac_file=pac_file)
        with open(pac_file, "r") as f:
            data = f.read()
            if "wpa_supplicant EAP-FAST PAC file - version 1" not in data:
                raise Exception("PAC file header missing")
            if "PAC-Key=" not in data:
                raise Exception("PAC-Key missing from PAC file")
        dev[0].request("REMOVE_NETWORK all")
        eap_connect(dev[0], hapd, "FAST", "user",
                    anonymous_identity="FAST", password="password",
                    ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2",
                    pac_file=pac_file)

        eap_connect(dev[1], hapd, "FAST", "user",
                    anonymous_identity="FAST", password="password",
                    ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2",
                    phase1="fast_provisioning=1 fast_pac_format=binary",
                    pac_file=pac_file2)
        dev[1].request("REMOVE_NETWORK all")
        eap_connect(dev[1], hapd, "FAST", "user",
                    anonymous_identity="FAST", password="password",
                    ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2",
                    phase1="fast_pac_format=binary",
                    pac_file=pac_file2)
    finally:
        try:
            os.remove(pac_file)
        except:
            pass
        try:
            os.remove(pac_file2)
        except:
            pass

def test_ap_wpa2_eap_fast_binary_pac(dev, apdev):
    """WPA2-Enterprise connection using EAP-FAST and binary PAC format"""
    check_eap_capa(dev[0], "FAST")
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "FAST", "user",
                anonymous_identity="FAST", password="password",
                ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2",
                phase1="fast_provisioning=1 fast_max_pac_list_len=1 fast_pac_format=binary",
                pac_file="blob://fast_pac_bin")
    res = eap_reauth(dev[0], "FAST")
    if res['tls_session_reused'] != '1':
        raise Exception("EAP-FAST could not use PAC session ticket")

    # Verify fast_max_pac_list_len=0 special case
    dev[0].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected()
    eap_connect(dev[0], hapd, "FAST", "user",
                anonymous_identity="FAST", password="password",
                ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2",
                phase1="fast_provisioning=1 fast_max_pac_list_len=0 fast_pac_format=binary",
                pac_file="blob://fast_pac_bin")

def test_ap_wpa2_eap_fast_missing_pac_config(dev, apdev):
    """WPA2-Enterprise connection using EAP-FAST and missing PAC config"""
    check_eap_capa(dev[0], "FAST")
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hostapd.add_ap(apdev[0], params)

    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="FAST",
                   identity="user", anonymous_identity="FAST",
                   password="password",
                   ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2",
                   pac_file="blob://fast_pac_not_in_use",
                   wait_connect=False, scan_freq="2412")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"])
    if ev is None:
        raise Exception("Timeout on EAP failure report")
    dev[0].request("REMOVE_NETWORK all")

    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="FAST",
                   identity="user", anonymous_identity="FAST",
                   password="password",
                   ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2",
                   wait_connect=False, scan_freq="2412")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"])
    if ev is None:
        raise Exception("Timeout on EAP failure report")

def test_ap_wpa2_eap_fast_binary_pac_errors(dev, apdev):
    """EAP-FAST and binary PAC errors"""
    check_eap_capa(dev[0], "FAST")
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)

    tests = [(1, "=eap_fast_save_pac_bin"),
             (1, "eap_fast_write_pac"),
             (2, "eap_fast_write_pac"),]
    for count, func in tests:
        if "OK" not in dev[0].request("SET blob fast_pac_bin_errors "):
            raise Exception("Could not set blob")

        with alloc_fail(dev[0], count, func):
            eap_connect(dev[0], hapd, "FAST", "user",
                        anonymous_identity="FAST", password="password",
                        ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2",
                        phase1="fast_provisioning=1 fast_pac_format=binary",
                        pac_file="blob://fast_pac_bin_errors")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

    tests = ["00", "000000000000", "6ae4920c0001",
             "6ae4920c000000",
             "6ae4920c0000" + "0000" + 32*"00" + "ffff" + "0000",
             "6ae4920c0000" + "0000" + 32*"00" + "0001" + "0000",
             "6ae4920c0000" + "0000" + 32*"00" + "0000" + "0001",
             "6ae4920c0000" + "0000" + 32*"00" + "0000" + "0008" + "00040000" + "0007000100"]
    for t in tests:
        if "OK" not in dev[0].request("SET blob fast_pac_bin_errors " + t):
            raise Exception("Could not set blob")

        dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="FAST",
                       identity="user", anonymous_identity="FAST",
                       password="password",
                       ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2",
                       phase1="fast_provisioning=1 fast_pac_format=binary",
                       pac_file="blob://fast_pac_bin_errors",
                       scan_freq="2412", wait_connect=False)
        ev = dev[0].wait_event(["EAP: Failed to initialize EAP method"],
                               timeout=5)
        if ev is None:
            raise Exception("Failure not reported")
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()

    pac = "6ae4920c0000" + "0000" + 32*"00" + "0000" + "0000"
    tests = [(1, "eap_fast_load_pac_bin"),
             (2, "eap_fast_load_pac_bin"),
             (3, "eap_fast_load_pac_bin")]
    for count, func in tests:
        if "OK" not in dev[0].request("SET blob fast_pac_bin_errors " + pac):
            raise Exception("Could not set blob")

        with alloc_fail(dev[0], count, func):
            dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="FAST",
                           identity="user", anonymous_identity="FAST",
                           password="password",
                           ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2",
                           phase1="fast_provisioning=1 fast_pac_format=binary",
                           pac_file="blob://fast_pac_bin_errors",
                           scan_freq="2412", wait_connect=False)
            ev = dev[0].wait_event(["EAP: Failed to initialize EAP method"],
                                   timeout=5)
            if ev is None:
                raise Exception("Failure not reported")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

    pac = "6ae4920c0000" + "0000" + 32*"00" + "0000" + "0005" + "0011223344"
    if "OK" not in dev[0].request("SET blob fast_pac_bin_errors " + pac):
        raise Exception("Could not set blob")

    eap_connect(dev[0], hapd, "FAST", "user",
                anonymous_identity="FAST", password="password",
                ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2",
                phase1="fast_provisioning=1 fast_pac_format=binary",
                pac_file="blob://fast_pac_bin_errors")
    dev[0].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected()

    pac = "6ae4920c0000" + "0000" + 32*"00" + "0000" + "0009" + "00040000" + "0007000100"
    tests = [(1, "eap_fast_pac_get_a_id"),
             (2, "eap_fast_pac_get_a_id")]
    for count, func in tests:
        if "OK" not in dev[0].request("SET blob fast_pac_bin_errors " + pac):
            raise Exception("Could not set blob")
        with alloc_fail(dev[0], count, func):
            eap_connect(dev[0], hapd, "FAST", "user",
                        anonymous_identity="FAST", password="password",
                        ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2",
                        phase1="fast_provisioning=1 fast_pac_format=binary",
                        pac_file="blob://fast_pac_bin_errors")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

def test_ap_wpa2_eap_fast_text_pac_errors(dev, apdev):
    """EAP-FAST and text PAC errors"""
    check_eap_capa(dev[0], "FAST")
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hostapd.add_ap(apdev[0], params)

    tests = [(1, "eap_fast_parse_hex;eap_fast_parse_pac_key"),
             (1, "eap_fast_parse_hex;eap_fast_parse_pac_opaque"),
             (1, "eap_fast_parse_hex;eap_fast_parse_a_id"),
             (1, "eap_fast_parse_start"),
             (1, "eap_fast_save_pac")]
    for count, func in tests:
        dev[0].request("FLUSH")
        if "OK" not in dev[0].request("SET blob fast_pac_text_errors "):
            raise Exception("Could not set blob")

        with alloc_fail(dev[0], count, func):
            dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="FAST",
                           identity="user", anonymous_identity="FAST",
                           password="password",
                           ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2",
                           phase1="fast_provisioning=1",
                           pac_file="blob://fast_pac_text_errors",
                           scan_freq="2412", wait_connect=False)
            wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

    pac = "wpa_supplicant EAP-FAST PAC file - version 1\n"
    pac += "START\n"
    pac += "PAC-Type\n"
    pac += "END\n"
    if "OK" not in dev[0].request("SET blob fast_pac_text_errors " + binascii.hexlify(pac.encode()).decode()):
        raise Exception("Could not set blob")

    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="FAST",
                   identity="user", anonymous_identity="FAST",
                   password="password",
                   ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2",
                   phase1="fast_provisioning=1",
                   pac_file="blob://fast_pac_text_errors",
                   scan_freq="2412", wait_connect=False)
    ev = dev[0].wait_event(["EAP: Failed to initialize EAP method"], timeout=5)
    if ev is None:
        raise Exception("Failure not reported")
    dev[0].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected()

    dev[0].request("FLUSH")
    if "OK" not in dev[0].request("SET blob fast_pac_text_errors "):
        raise Exception("Could not set blob")

    with alloc_fail(dev[0], 1, "eap_fast_add_pac_data"):
        for i in range(3):
            params = int_eap_server_params()
            params['ssid'] = "test-wpa2-eap-2"
            params['pac_opaque_encr_key'] = "000102030405060708090a0b0c0dff%02x" % i
            params['eap_fast_a_id'] = "101112131415161718191a1b1c1dff%02x" % i
            params['eap_fast_a_id_info'] = "test server %d" % i

            hapd2 = hostapd.add_ap(apdev[1], params)

            dev[0].connect("test-wpa2-eap-2", key_mgmt="WPA-EAP", eap="FAST",
                           identity="user", anonymous_identity="FAST",
                           password="password",
                           ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2",
                           phase1="fast_provisioning=1",
                           pac_file="blob://fast_pac_text_errors",
                           scan_freq="2412", wait_connect=False)
            dev[0].wait_connected()
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

            hapd2.disable()

def test_ap_wpa2_eap_fast_pac_truncate(dev, apdev):
    """EAP-FAST and PAC list truncation"""
    check_eap_capa(dev[0], "FAST")
    if "OK" not in dev[0].request("SET blob fast_pac_truncate "):
        raise Exception("Could not set blob")
    for i in range(5):
        params = int_eap_server_params()
        params['pac_opaque_encr_key'] = "000102030405060708090a0b0c0dff%02x" % i
        params['eap_fast_a_id'] = "101112131415161718191a1b1c1dff%02x" % i
        params['eap_fast_a_id_info'] = "test server %d" % i
        hapd = hostapd.add_ap(apdev[0], params)

        dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="FAST",
                       identity="user", anonymous_identity="FAST",
                       password="password",
                       ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2",
                       phase1="fast_provisioning=1 fast_max_pac_list_len=2",
                       pac_file="blob://fast_pac_truncate",
                       scan_freq="2412", wait_connect=False)
        dev[0].wait_connected()
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()

        hapd.disable()

def test_ap_wpa2_eap_fast_pac_refresh(dev, apdev):
    """EAP-FAST and PAC refresh"""
    check_eap_capa(dev[0], "FAST")
    if "OK" not in dev[0].request("SET blob fast_pac_refresh "):
        raise Exception("Could not set blob")
    for i in range(2):
        params = int_eap_server_params()
        params['pac_opaque_encr_key'] = "000102030405060708090a0b0c0dff%02x" % i
        params['eap_fast_a_id'] = "101112131415161718191a1b1c1dff%02x" % i
        params['eap_fast_a_id_info'] = "test server %d" % i
        params['pac_key_refresh_time'] = "1"
        params['pac_key_lifetime'] = "10"
        hapd = hostapd.add_ap(apdev[0], params)

        dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="FAST",
                       identity="user", anonymous_identity="FAST",
                       password="password",
                       ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2",
                       phase1="fast_provisioning=1",
                       pac_file="blob://fast_pac_refresh",
                       scan_freq="2412", wait_connect=False)
        dev[0].wait_connected()
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()

        hapd.disable()

    for i in range(2):
        params = int_eap_server_params()
        params['pac_opaque_encr_key'] = "000102030405060708090a0b0c0dff%02x" % i
        params['eap_fast_a_id'] = "101112131415161718191a1b1c1dff%02x" % i
        params['eap_fast_a_id_info'] = "test server %d" % i
        params['pac_key_refresh_time'] = "10"
        params['pac_key_lifetime'] = "10"
        hapd = hostapd.add_ap(apdev[0], params)

        dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="FAST",
                       identity="user", anonymous_identity="FAST",
                       password="password",
                       ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2",
                       phase1="fast_provisioning=1",
                       pac_file="blob://fast_pac_refresh",
                       scan_freq="2412", wait_connect=False)
        dev[0].wait_connected()
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()

        hapd.disable()

def test_ap_wpa2_eap_fast_pac_lifetime(dev, apdev):
    """EAP-FAST and PAC lifetime"""
    check_eap_capa(dev[0], "FAST")
    if "OK" not in dev[0].request("SET blob fast_pac_refresh "):
        raise Exception("Could not set blob")

    i = 0
    params = int_eap_server_params()
    params['pac_opaque_encr_key'] = "000102030405060708090a0b0c0dff%02x" % i
    params['eap_fast_a_id'] = "101112131415161718191a1b1c1dff%02x" % i
    params['eap_fast_a_id_info'] = "test server %d" % i
    params['pac_key_refresh_time'] = "0"
    params['pac_key_lifetime'] = "2"
    hapd = hostapd.add_ap(apdev[0], params)

    id = dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="FAST",
                        identity="user", anonymous_identity="FAST",
                        password="password",
                        ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2",
                        phase1="fast_provisioning=2",
                        pac_file="blob://fast_pac_refresh",
                        scan_freq="2412", wait_connect=False)
    dev[0].wait_connected()
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    time.sleep(3)
    dev[0].request("PMKSA_FLUSH")
    dev[0].request("RECONNECT")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"], timeout=10)
    if ev is None:
        raise Exception("No EAP-Failure seen after expired PAC")
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    dev[0].select_network(id)
    dev[0].wait_connected()
    dev[0].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected()

def test_ap_wpa2_eap_fast_gtc_auth_prov(dev, apdev):
    """WPA2-Enterprise connection using EAP-FAST/GTC and authenticated provisioning"""
    check_eap_capa(dev[0], "FAST")
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "FAST", "user",
                anonymous_identity="FAST", password="password",
                ca_cert="auth_serv/ca.pem", phase2="auth=GTC",
                phase1="fast_provisioning=2", pac_file="blob://fast_pac_auth")
    hwsim_utils.test_connectivity(dev[0], hapd)
    res = eap_reauth(dev[0], "FAST")
    if res['tls_session_reused'] != '1':
        raise Exception("EAP-FAST could not use PAC session ticket")

def test_ap_wpa2_eap_fast_gtc_identity_change(dev, apdev):
    """WPA2-Enterprise connection using EAP-FAST/GTC and identity changing"""
    check_eap_capa(dev[0], "FAST")
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    id = eap_connect(dev[0], hapd, "FAST", "user",
                     anonymous_identity="FAST", password="password",
                     ca_cert="auth_serv/ca.pem", phase2="auth=GTC",
                     phase1="fast_provisioning=2",
                     pac_file="blob://fast_pac_auth")
    dev[0].set_network_quoted(id, "identity", "user2")
    dev[0].wait_disconnected()
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-METHOD"], timeout=15)
    if ev is None:
        raise Exception("EAP-FAST not started")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"], timeout=5)
    if ev is None:
        raise Exception("EAP failure not reported")
    dev[0].wait_disconnected()

def test_ap_wpa2_eap_fast_prf_oom(dev, apdev):
    """WPA2-Enterprise connection using EAP-FAST and OOM in PRF"""
    check_eap_capa(dev[0], "FAST")
    tls = dev[0].request("GET tls_library")
    if tls.startswith("OpenSSL"):
        func = "tls_connection_get_eap_fast_key"
        count = 2
    elif tls.startswith("internal"):
        func = "tls_connection_prf"
        count = 1
    else:
        raise HwsimSkip("Unsupported TLS library")
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    with alloc_fail(dev[0], count, func):
        dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="FAST",
                       identity="user", anonymous_identity="FAST",
                       password="password", ca_cert="auth_serv/ca.pem",
                       phase2="auth=GTC",
                       phase1="fast_provisioning=2",
                       pac_file="blob://fast_pac_auth",
                       wait_connect=False, scan_freq="2412")
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"], timeout=15)
        if ev is None:
            raise Exception("EAP failure not reported")
    dev[0].request("DISCONNECT")

def test_ap_wpa2_eap_fast_server_oom(dev, apdev):
    """EAP-FAST/MSCHAPv2 and server OOM"""
    check_eap_capa(dev[0], "FAST")

    params = int_eap_server_params()
    params['dh_file'] = 'auth_serv/dh.conf'
    params['pac_opaque_encr_key'] = '000102030405060708090a0b0c0d0e0f'
    params['eap_fast_a_id'] = '1011'
    params['eap_fast_a_id_info'] = 'another test server'
    hapd = hostapd.add_ap(apdev[0], params)

    with alloc_fail(hapd, 1, "tls_session_ticket_ext_cb"):
        id = eap_connect(dev[0], hapd, "FAST", "user",
                         anonymous_identity="FAST", password="password",
                         ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2",
                         phase1="fast_provisioning=1",
                         pac_file="blob://fast_pac",
                         expect_failure=True)
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"], timeout=10)
        if ev is None:
            raise Exception("No EAP failure reported")
        dev[0].wait_disconnected()
        dev[0].request("DISCONNECT")

    dev[0].select_network(id, freq="2412")

def test_ap_wpa2_eap_fast_cipher_suites(dev, apdev):
    """EAP-FAST and different TLS cipher suites"""
    check_eap_capa(dev[0], "FAST")
    tls = dev[0].request("GET tls_library")
    if not tls.startswith("OpenSSL") and not tls.startswith("wolfSSL"):
        raise HwsimSkip("TLS library is not OpenSSL or wolfSSL: " + tls)

    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].request("SET blob fast_pac_ciphers ")
    eap_connect(dev[0], hapd, "FAST", "user",
                anonymous_identity="FAST", password="password",
                ca_cert="auth_serv/ca.pem", phase2="auth=GTC",
                phase1="fast_provisioning=2",
                pac_file="blob://fast_pac_ciphers")
    res = dev[0].get_status_field('EAP TLS cipher')
    dev[0].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected()
    if res != "DHE-RSA-AES256-SHA":
        raise Exception("Unexpected cipher suite for provisioning: " + res)

    tests = ["DHE-RSA-AES128-SHA",
             "RC4-SHA",
             "AES128-SHA",
             "AES256-SHA",
             "DHE-RSA-AES256-SHA"]
    for cipher in tests:
        dev[0].dump_monitor()
        logger.info("Testing " + cipher)
        try:
            eap_connect(dev[0], hapd, "FAST", "user",
                        openssl_ciphers=cipher,
                        anonymous_identity="FAST", password="password",
                        ca_cert="auth_serv/ca.pem", phase2="auth=GTC",
                        pac_file="blob://fast_pac_ciphers",
                        report_failure=True)
        except Exception as e:
            if cipher == "RC4-SHA" and \
               ("Could not select EAP method" in str(e) or \
                "EAP failed" in str(e)):
                if "run=OpenSSL 1.1" in tls:
                    logger.info("Allow failure due to missing TLS library support")
                    dev[0].request("REMOVE_NETWORK all")
                    dev[0].wait_disconnected()
                    continue
            raise
        res = dev[0].get_status_field('EAP TLS cipher')
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()
        if res != cipher:
            raise Exception("Unexpected TLS cipher info (configured %s): %s" % (cipher, res))

def test_ap_wpa2_eap_fast_prov(dev, apdev):
    """EAP-FAST and provisioning options"""
    check_eap_capa(dev[0], "FAST")
    if "OK" not in dev[0].request("SET blob fast_pac_prov "):
        raise Exception("Could not set blob")

    i = 100
    params = int_eap_server_params()
    params['disable_pmksa_caching'] = '1'
    params['pac_opaque_encr_key'] = "000102030405060708090a0b0c0dff%02x" % i
    params['eap_fast_a_id'] = "101112131415161718191a1b1c1dff%02x" % i
    params['eap_fast_a_id_info'] = "test server %d" % i
    params['eap_fast_prov'] = "0"
    hapd = hostapd.add_ap(apdev[0], params)

    logger.info("Provisioning attempt while server has provisioning disabled")
    id = dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="FAST",
                        identity="user", anonymous_identity="FAST",
                        password="password",
                        ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2",
                        phase1="fast_provisioning=2",
                        pac_file="blob://fast_pac_prov",
                        scan_freq="2412", wait_connect=False)
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-STATUS status='completion'"],
                           timeout=15)
    if ev is None:
        raise Exception("EAP result not reported")
    if "parameter='failure'" not in ev:
        raise Exception("Unexpected EAP result: " + ev)
    dev[0].wait_disconnected()
    dev[0].request("DISCONNECT")
    dev[0].dump_monitor()

    hapd.disable()
    logger.info("Authenticated provisioning")
    hapd.set("eap_fast_prov", "2")
    hapd.enable()

    dev[0].select_network(id, freq="2412")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-STATUS status='completion'"],
                           timeout=15)
    if ev is None:
        raise Exception("EAP result not reported")
    if "parameter='success'" not in ev:
        raise Exception("Unexpected EAP result: " + ev)
    dev[0].wait_connected()
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()
    dev[0].dump_monitor()

    hapd.disable()
    logger.info("Provisioning disabled - using previously provisioned PAC")
    hapd.set("eap_fast_prov", "0")
    hapd.enable()

    dev[0].select_network(id, freq="2412")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-STATUS status='completion'"],
                           timeout=15)
    if ev is None:
        raise Exception("EAP result not reported")
    if "parameter='success'" not in ev:
        raise Exception("Unexpected EAP result: " + ev)
    dev[0].wait_connected()
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()
    dev[0].dump_monitor()

    logger.info("Drop PAC and verify connection failure")
    if "OK" not in dev[0].request("SET blob fast_pac_prov "):
        raise Exception("Could not set blob")

    dev[0].select_network(id, freq="2412")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-STATUS status='completion'"],
                           timeout=15)
    if ev is None:
        raise Exception("EAP result not reported")
    if "parameter='failure'" not in ev:
        raise Exception("Unexpected EAP result: " + ev)
    dev[0].wait_disconnected()
    dev[0].request("DISCONNECT")
    dev[0].dump_monitor()

    hapd.disable()
    logger.info("Anonymous provisioning")
    hapd.set("eap_fast_prov", "1")
    hapd.enable()
    dev[0].set_network_quoted(id, "phase1", "fast_provisioning=1")
    dev[0].select_network(id, freq="2412")
    # Anonymous provisioning results in EAP-Failure first
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-STATUS status='completion'"],
                           timeout=15)
    if ev is None:
        raise Exception("EAP result not reported")
    if "parameter='failure'" not in ev:
        raise Exception("Unexpected EAP result: " + ev)
    dev[0].wait_disconnected()
    # And then the actual data connection
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-STATUS status='completion'"],
                           timeout=15)
    if ev is None:
        raise Exception("EAP result not reported")
    if "parameter='success'" not in ev:
        raise Exception("Unexpected EAP result: " + ev)
    dev[0].wait_connected()
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()
    dev[0].dump_monitor()

    hapd.disable()
    logger.info("Provisioning disabled - using previously provisioned PAC")
    hapd.set("eap_fast_prov", "0")
    hapd.enable()

    dev[0].select_network(id, freq="2412")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-STATUS status='completion'"],
                           timeout=15)
    if ev is None:
        raise Exception("EAP result not reported")
    if "parameter='success'" not in ev:
        raise Exception("Unexpected EAP result: " + ev)
    dev[0].wait_connected()
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()
    dev[0].dump_monitor()

def test_ap_wpa2_eap_fast_eap_vendor(dev, apdev):
    """WPA2-Enterprise connection using EAP-FAST/EAP-vendor"""
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "FAST", "vendor-test-2",
                anonymous_identity="FAST",
                phase1="fast_provisioning=2", pac_file="blob://fast_pac",
                ca_cert="auth_serv/ca.pem", phase2="auth=VENDOR-TEST")

def test_ap_wpa2_eap_tls_ocsp(dev, apdev):
    """WPA2-Enterprise connection using EAP-TLS and verifying OCSP"""
    check_ocsp_support(dev[0])
    check_pkcs12_support(dev[0])
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "TLS", "tls user", ca_cert="auth_serv/ca.pem",
                private_key="auth_serv/user.pkcs12",
                private_key_passwd="whatever", ocsp=2)

def test_ap_wpa2_eap_tls_ocsp_multi(dev, apdev):
    """WPA2-Enterprise connection using EAP-TLS and verifying OCSP-multi"""
    check_ocsp_multi_support(dev[0])
    check_pkcs12_support(dev[0])

    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "TLS", "tls user", ca_cert="auth_serv/ca.pem",
                private_key="auth_serv/user.pkcs12",
                private_key_passwd="whatever", ocsp=2)

def int_eap_server_params():
    params = {"ssid": "test-wpa2-eap", "wpa": "2", "wpa_key_mgmt": "WPA-EAP",
              "rsn_pairwise": "CCMP", "ieee8021x": "1",
              "eap_server": "1", "eap_user_file": "auth_serv/eap_user.conf",
              "ca_cert": "auth_serv/ca.pem",
              "server_cert": "auth_serv/server.pem",
              "private_key": "auth_serv/server.key",
              "dh_file": "auth_serv/dh.conf"}
    return params

def run_openssl(arg):
    logger.info(' '.join(arg))
    cmd = subprocess.Popen(arg, stdout=subprocess.PIPE,
                           stderr=subprocess.PIPE)
    res = cmd.stdout.read().decode() + "\n" + cmd.stderr.read().decode()
    cmd.stdout.close()
    cmd.stderr.close()
    cmd.wait()
    if cmd.returncode != 0:
        raise Exception("bad return code from openssl\n\n" + res)
    logger.info("openssl result:\n" + res)

def ocsp_cache_key_id(outfile):
    if os.path.exists(outfile):
        return
    arg = ["openssl", "ocsp", "-index", "auth_serv/index.txt",
           '-rsigner', 'auth_serv/ocsp-responder.pem',
           '-rkey', 'auth_serv/ocsp-responder.key',
           '-resp_key_id',
           '-CA', 'auth_serv/ca.pem',
           '-issuer', 'auth_serv/ca.pem',
           '-verify_other', 'auth_serv/ca.pem',
           '-trust_other',
           '-ndays', '7',
           '-reqin', 'auth_serv/ocsp-req.der',
           '-respout', outfile]
    run_openssl(arg)

def test_ap_wpa2_eap_tls_ocsp_key_id(dev, apdev, params):
    """EAP-TLS and OCSP certificate signed OCSP response using key ID"""
    check_ocsp_support(dev[0])
    check_pkcs12_support(dev[0])
    ocsp = os.path.join(params['logdir'], "ocsp-server-cache-key-id.der")
    ocsp_cache_key_id(ocsp)
    if not os.path.exists(ocsp):
        raise HwsimSkip("No OCSP response available")
    params = int_eap_server_params()
    params["ocsp_stapling_response"] = ocsp
    hostapd.add_ap(apdev[0], params)
    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TLS",
                   identity="tls user", ca_cert="auth_serv/ca.pem",
                   private_key="auth_serv/user.pkcs12",
                   private_key_passwd="whatever", ocsp=2,
                   scan_freq="2412")

def ocsp_req(outfile):
    if os.path.exists(outfile):
        return
    arg = ["openssl", "ocsp",
           "-reqout", outfile,
           '-issuer', 'auth_serv/ca.pem',
           '-sha256',
           '-serial', '0xD8D3E3A6CBE3CD69',
           '-no_nonce']
    run_openssl(arg)
    if not os.path.exists(outfile):
        raise HwsimSkip("Failed to generate OCSP request")

def ocsp_resp_ca_signed(reqfile, outfile, status):
    ocsp_req(reqfile)
    if os.path.exists(outfile):
        return
    arg = ["openssl", "ocsp",
           "-index", "auth_serv/index%s.txt" % status,
           "-rsigner", "auth_serv/ca.pem",
           "-rkey", "auth_serv/ca-key.pem",
           "-CA", "auth_serv/ca.pem",
           "-ndays", "7",
           "-reqin", reqfile,
           "-resp_no_certs",
           "-respout", outfile]
    run_openssl(arg)
    if not os.path.exists(outfile):
        raise HwsimSkip("No OCSP response available")

def ocsp_resp_server_signed(reqfile, outfile):
    ocsp_req(reqfile)
    if os.path.exists(outfile):
        return
    arg = ["openssl", "ocsp",
           "-index", "auth_serv/index.txt",
           "-rsigner", "auth_serv/server.pem",
           "-rkey", "auth_serv/server.key",
           "-CA", "auth_serv/ca.pem",
           "-ndays", "7",
           "-reqin", reqfile,
           "-respout", outfile]
    run_openssl(arg)
    if not os.path.exists(outfile):
        raise HwsimSkip("No OCSP response available")

def test_ap_wpa2_eap_tls_ocsp_ca_signed_good(dev, apdev, params):
    """EAP-TLS and CA signed OCSP response (good)"""
    check_ocsp_support(dev[0])
    check_pkcs12_support(dev[0])
    req = os.path.join(params['logdir'], "ocsp-req.der")
    ocsp = os.path.join(params['logdir'], "ocsp-resp-ca-signed.der")
    ocsp_resp_ca_signed(req, ocsp, "")
    params = int_eap_server_params()
    params["ocsp_stapling_response"] = ocsp
    hostapd.add_ap(apdev[0], params)
    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TLS",
                   identity="tls user", ca_cert="auth_serv/ca.pem",
                   private_key="auth_serv/user.pkcs12",
                   private_key_passwd="whatever", ocsp=2,
                   scan_freq="2412")

def test_ap_wpa2_eap_tls_ocsp_ca_signed_revoked(dev, apdev, params):
    """EAP-TLS and CA signed OCSP response (revoked)"""
    check_ocsp_support(dev[0])
    check_pkcs12_support(dev[0])
    req = os.path.join(params['logdir'], "ocsp-req.der")
    ocsp = os.path.join(params['logdir'], "ocsp-resp-ca-signed-revoked.der")
    ocsp_resp_ca_signed(req, ocsp, "-revoked")
    params = int_eap_server_params()
    params["ocsp_stapling_response"] = ocsp
    hostapd.add_ap(apdev[0], params)
    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TLS",
                   identity="tls user", ca_cert="auth_serv/ca.pem",
                   private_key="auth_serv/user.pkcs12",
                   private_key_passwd="whatever", ocsp=2,
                   wait_connect=False, scan_freq="2412")
    count = 0
    while True:
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-STATUS"])
        if ev is None:
            raise Exception("Timeout on EAP status")
        if 'bad certificate status response' in ev:
            break
        if 'certificate revoked' in ev:
            break
        count = count + 1
        if count > 10:
            raise Exception("Unexpected number of EAP status messages")

    ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"])
    if ev is None:
        raise Exception("Timeout on EAP failure report")

def test_ap_wpa2_eap_tls_ocsp_ca_signed_unknown(dev, apdev, params):
    """EAP-TLS and CA signed OCSP response (unknown)"""
    check_ocsp_support(dev[0])
    check_pkcs12_support(dev[0])
    req = os.path.join(params['logdir'], "ocsp-req.der")
    ocsp = os.path.join(params['logdir'], "ocsp-resp-ca-signed-unknown.der")
    ocsp_resp_ca_signed(req, ocsp, "-unknown")
    params = int_eap_server_params()
    params["ocsp_stapling_response"] = ocsp
    hostapd.add_ap(apdev[0], params)
    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TLS",
                   identity="tls user", ca_cert="auth_serv/ca.pem",
                   private_key="auth_serv/user.pkcs12",
                   private_key_passwd="whatever", ocsp=2,
                   wait_connect=False, scan_freq="2412")
    count = 0
    while True:
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-STATUS"])
        if ev is None:
            raise Exception("Timeout on EAP status")
        if 'bad certificate status response' in ev:
            break
        count = count + 1
        if count > 10:
            raise Exception("Unexpected number of EAP status messages")

    ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"])
    if ev is None:
        raise Exception("Timeout on EAP failure report")

def test_ap_wpa2_eap_tls_ocsp_server_signed(dev, apdev, params):
    """EAP-TLS and server signed OCSP response"""
    check_ocsp_support(dev[0])
    check_pkcs12_support(dev[0])
    req = os.path.join(params['logdir'], "ocsp-req.der")
    ocsp = os.path.join(params['logdir'], "ocsp-resp-server-signed.der")
    ocsp_resp_server_signed(req, ocsp)
    params = int_eap_server_params()
    params["ocsp_stapling_response"] = ocsp
    hostapd.add_ap(apdev[0], params)
    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TLS",
                   identity="tls user", ca_cert="auth_serv/ca.pem",
                   private_key="auth_serv/user.pkcs12",
                   private_key_passwd="whatever", ocsp=2,
                   wait_connect=False, scan_freq="2412")
    count = 0
    while True:
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-STATUS"])
        if ev is None:
            raise Exception("Timeout on EAP status")
        if 'bad certificate status response' in ev:
            break
        count = count + 1
        if count > 10:
            raise Exception("Unexpected number of EAP status messages")

    ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"])
    if ev is None:
        raise Exception("Timeout on EAP failure report")

def test_ap_wpa2_eap_tls_ocsp_invalid_data(dev, apdev):
    """WPA2-Enterprise connection using EAP-TLS and invalid OCSP data"""
    check_ocsp_support(dev[0])
    check_pkcs12_support(dev[0])
    params = int_eap_server_params()
    params["ocsp_stapling_response"] = "auth_serv/ocsp-req.der"
    hostapd.add_ap(apdev[0], params)
    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TLS",
                   identity="tls user", ca_cert="auth_serv/ca.pem",
                   private_key="auth_serv/user.pkcs12",
                   private_key_passwd="whatever", ocsp=2,
                   wait_connect=False, scan_freq="2412")
    count = 0
    while True:
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-STATUS"])
        if ev is None:
            raise Exception("Timeout on EAP status")
        if 'bad certificate status response' in ev:
            break
        count = count + 1
        if count > 10:
            raise Exception("Unexpected number of EAP status messages")

    ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"])
    if ev is None:
        raise Exception("Timeout on EAP failure report")

def test_ap_wpa2_eap_tls_ocsp_invalid(dev, apdev):
    """WPA2-Enterprise connection using EAP-TLS and invalid OCSP response"""
    check_ocsp_support(dev[0])
    check_pkcs12_support(dev[0])
    params = int_eap_server_params()
    params["ocsp_stapling_response"] = "auth_serv/ocsp-server-cache.der-invalid"
    hostapd.add_ap(apdev[0], params)
    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TLS",
                   identity="tls user", ca_cert="auth_serv/ca.pem",
                   private_key="auth_serv/user.pkcs12",
                   private_key_passwd="whatever", ocsp=2,
                   wait_connect=False, scan_freq="2412")
    count = 0
    while True:
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-STATUS"])
        if ev is None:
            raise Exception("Timeout on EAP status")
        if 'bad certificate status response' in ev:
            break
        count = count + 1
        if count > 10:
            raise Exception("Unexpected number of EAP status messages")

    ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"])
    if ev is None:
        raise Exception("Timeout on EAP failure report")

def test_ap_wpa2_eap_tls_ocsp_unknown_sign(dev, apdev):
    """WPA2-Enterprise connection using EAP-TLS and unknown OCSP signer"""
    check_ocsp_support(dev[0])
    check_pkcs12_support(dev[0])
    params = int_eap_server_params()
    params["ocsp_stapling_response"] = "auth_serv/ocsp-server-cache.der-unknown-sign"
    hostapd.add_ap(apdev[0], params)
    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TLS",
                   identity="tls user", ca_cert="auth_serv/ca.pem",
                   private_key="auth_serv/user.pkcs12",
                   private_key_passwd="whatever", ocsp=2,
                   wait_connect=False, scan_freq="2412")
    count = 0
    while True:
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-STATUS"])
        if ev is None:
            raise Exception("Timeout on EAP status")
        if 'bad certificate status response' in ev:
            break
        count = count + 1
        if count > 10:
            raise Exception("Unexpected number of EAP status messages")

    ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"])
    if ev is None:
        raise Exception("Timeout on EAP failure report")

def ocsp_resp_status(outfile, status):
    if os.path.exists(outfile):
        return
    arg = ["openssl", "ocsp", "-index", "auth_serv/index-%s.txt" % status,
           '-rsigner', 'auth_serv/ocsp-responder.pem',
           '-rkey', 'auth_serv/ocsp-responder.key',
           '-CA', 'auth_serv/ca.pem',
           '-issuer', 'auth_serv/ca.pem',
           '-verify_other', 'auth_serv/ca.pem',
           '-trust_other',
           '-ndays', '7',
           '-reqin', 'auth_serv/ocsp-req.der',
           '-respout', outfile]
    run_openssl(arg)

def test_ap_wpa2_eap_ttls_ocsp_revoked(dev, apdev, params):
    """WPA2-Enterprise connection using EAP-TTLS and OCSP status revoked"""
    check_ocsp_support(dev[0])
    ocsp = os.path.join(params['logdir'], "ocsp-server-cache-revoked.der")
    ocsp_resp_status(ocsp, "revoked")
    if not os.path.exists(ocsp):
        raise HwsimSkip("No OCSP response available")
    params = int_eap_server_params()
    params["ocsp_stapling_response"] = ocsp
    hostapd.add_ap(apdev[0], params)
    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TTLS",
                   identity="pap user", ca_cert="auth_serv/ca.pem",
                   anonymous_identity="ttls", password="password",
                   phase2="auth=PAP", ocsp=2,
                   wait_connect=False, scan_freq="2412")
    count = 0
    while True:
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-STATUS"])
        if ev is None:
            raise Exception("Timeout on EAP status")
        if 'bad certificate status response' in ev:
            break
        if 'certificate revoked' in ev:
            break
        count = count + 1
        if count > 10:
            raise Exception("Unexpected number of EAP status messages")

    ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"])
    if ev is None:
        raise Exception("Timeout on EAP failure report")

def test_ap_wpa2_eap_ttls_ocsp_unknown(dev, apdev, params):
    """WPA2-Enterprise connection using EAP-TTLS and OCSP status unknown"""
    check_ocsp_support(dev[0])
    ocsp = os.path.join(params['logdir'], "ocsp-server-cache-unknown.der")
    ocsp_resp_status(ocsp, "unknown")
    if not os.path.exists(ocsp):
        raise HwsimSkip("No OCSP response available")
    params = int_eap_server_params()
    params["ocsp_stapling_response"] = ocsp
    hostapd.add_ap(apdev[0], params)
    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TTLS",
                   identity="pap user", ca_cert="auth_serv/ca.pem",
                   anonymous_identity="ttls", password="password",
                   phase2="auth=PAP", ocsp=2,
                   wait_connect=False, scan_freq="2412")
    count = 0
    while True:
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-STATUS"])
        if ev is None:
            raise Exception("Timeout on EAP status")
        if 'bad certificate status response' in ev:
            break
        count = count + 1
        if count > 10:
            raise Exception("Unexpected number of EAP status messages")

    ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"])
    if ev is None:
        raise Exception("Timeout on EAP failure report")

def test_ap_wpa2_eap_ttls_optional_ocsp_unknown(dev, apdev, params):
    """WPA2-Enterprise connection using EAP-TTLS and OCSP status unknown"""
    check_ocsp_support(dev[0])
    ocsp = os.path.join(params['logdir'], "ocsp-server-cache-unknown.der")
    ocsp_resp_status(ocsp, "unknown")
    if not os.path.exists(ocsp):
        raise HwsimSkip("No OCSP response available")
    params = int_eap_server_params()
    params["ocsp_stapling_response"] = ocsp
    hostapd.add_ap(apdev[0], params)
    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TTLS",
                   identity="pap user", ca_cert="auth_serv/ca.pem",
                   anonymous_identity="ttls", password="password",
                   phase2="auth=PAP", ocsp=1, scan_freq="2412")

def test_ap_wpa2_eap_tls_intermediate_ca(dev, apdev, params):
    """EAP-TLS with intermediate server/user CA"""
    params = int_eap_server_params()
    params["ca_cert"] = "auth_serv/iCA-server/ca-and-root.pem"
    params["server_cert"] = "auth_serv/iCA-server/server.pem"
    params["private_key"] = "auth_serv/iCA-server/server.key"
    hostapd.add_ap(apdev[0], params)
    tls = dev[0].request("GET tls_library")
    if "GnuTLS" in tls or "wolfSSL" in tls:
        ca_cert = "auth_serv/iCA-user/ca-and-root.pem"
        client_cert = "auth_serv/iCA-user/user_and_ica.pem"
    else:
        ca_cert = "auth_serv/iCA-user/ca-and-root.pem"
        client_cert = "auth_serv/iCA-user/user.pem"
    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TLS",
                   identity="tls user",
                   ca_cert=ca_cert,
                   client_cert=client_cert,
                   private_key="auth_serv/iCA-user/user.key",
                   scan_freq="2412")

def root_ocsp(cert):
    ca = "auth_serv/ca.pem"

    fd2, fn2 = tempfile.mkstemp()
    os.close(fd2)

    arg = ["openssl", "ocsp", "-reqout", fn2, "-issuer", ca, "-sha256",
           "-cert", cert, "-no_nonce", "-text"]
    run_openssl(arg)

    fd, fn = tempfile.mkstemp()
    os.close(fd)
    arg = ["openssl", "ocsp", "-index", "auth_serv/rootCA/index.txt",
           "-rsigner", ca, "-rkey", "auth_serv/ca-key.pem",
           "-CA", ca, "-issuer", ca, "-verify_other", ca, "-trust_other",
           "-ndays", "7", "-reqin", fn2, "-resp_no_certs", "-respout", fn,
           "-text"]
    run_openssl(arg)
    os.unlink(fn2)
    return fn

def ica_ocsp(cert, md="-sha256"):
    prefix = "auth_serv/iCA-server/"
    ca = prefix + "cacert.pem"
    cert = prefix + cert

    fd2, fn2 = tempfile.mkstemp()
    os.close(fd2)

    arg = ["openssl", "ocsp", "-reqout", fn2, "-issuer", ca, md,
           "-cert", cert, "-no_nonce", "-text"]
    run_openssl(arg)

    fd, fn = tempfile.mkstemp()
    os.close(fd)
    arg = ["openssl", "ocsp", "-index", prefix + "index.txt",
           "-rsigner", ca, "-rkey", prefix + "private/cakey.pem",
           "-CA", ca, "-issuer", ca, "-verify_other", ca, "-trust_other",
           "-ndays", "7", "-reqin", fn2, "-resp_no_certs", "-respout", fn,
           "-text"]
    run_openssl(arg)
    os.unlink(fn2)
    return fn

def test_ap_wpa2_eap_tls_intermediate_ca_ocsp(dev, apdev, params):
    """EAP-TLS with intermediate server/user CA and OCSP on server certificate"""
    run_ap_wpa2_eap_tls_intermediate_ca_ocsp(dev, apdev, params, "-sha256")

def test_ap_wpa2_eap_tls_intermediate_ca_ocsp_sha1(dev, apdev, params):
    """EAP-TLS with intermediate server/user CA and OCSP on server certificate )SHA1)"""
    run_ap_wpa2_eap_tls_intermediate_ca_ocsp(dev, apdev, params, "-sha1")

def run_ap_wpa2_eap_tls_intermediate_ca_ocsp(dev, apdev, params, md):
    params = int_eap_server_params()
    params["ca_cert"] = "auth_serv/iCA-server/ca-and-root.pem"
    params["server_cert"] = "auth_serv/iCA-server/server.pem"
    params["private_key"] = "auth_serv/iCA-server/server.key"
    fn = ica_ocsp("server.pem", md)
    params["ocsp_stapling_response"] = fn
    try:
        hostapd.add_ap(apdev[0], params)
        tls = dev[0].request("GET tls_library")
        if "GnuTLS" in tls or "wolfSSL" in tls:
            ca_cert = "auth_serv/iCA-user/ca-and-root.pem"
            client_cert = "auth_serv/iCA-user/user_and_ica.pem"
        else:
            ca_cert = "auth_serv/iCA-user/ca-and-root.pem"
            client_cert = "auth_serv/iCA-user/user.pem"
        dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TLS",
                       identity="tls user",
                       ca_cert=ca_cert,
                       client_cert=client_cert,
                       private_key="auth_serv/iCA-user/user.key",
                       scan_freq="2412", ocsp=2)
    finally:
        os.unlink(fn)

def test_ap_wpa2_eap_tls_intermediate_ca_ocsp_revoked(dev, apdev, params):
    """EAP-TLS with intermediate server/user CA and OCSP on revoked server certificate"""
    run_ap_wpa2_eap_tls_intermediate_ca_ocsp_revoked(dev, apdev, params,
                                                     "-sha256")

def test_ap_wpa2_eap_tls_intermediate_ca_ocsp_revoked_sha1(dev, apdev, params):
    """EAP-TLS with intermediate server/user CA and OCSP on revoked server certificate (SHA1)"""
    run_ap_wpa2_eap_tls_intermediate_ca_ocsp_revoked(dev, apdev, params,
                                                     "-sha1")

def run_ap_wpa2_eap_tls_intermediate_ca_ocsp_revoked(dev, apdev, params, md):
    check_ocsp_support(dev[0])
    params = int_eap_server_params()
    params["ca_cert"] = "auth_serv/iCA-server/ca-and-root.pem"
    params["server_cert"] = "auth_serv/iCA-server/server-revoked.pem"
    params["private_key"] = "auth_serv/iCA-server/server-revoked.key"
    fn = ica_ocsp("server-revoked.pem", md)
    params["ocsp_stapling_response"] = fn
    try:
        hostapd.add_ap(apdev[0], params)
        tls = dev[0].request("GET tls_library")
        if "GnuTLS" in tls or "wolfSSL" in tls:
            ca_cert = "auth_serv/iCA-user/ca-and-root.pem"
            client_cert = "auth_serv/iCA-user/user_and_ica.pem"
        else:
            ca_cert = "auth_serv/iCA-user/ca-and-root.pem"
            client_cert = "auth_serv/iCA-user/user.pem"
        dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TLS",
                       identity="tls user",
                       ca_cert=ca_cert,
                       client_cert=client_cert,
                       private_key="auth_serv/iCA-user/user.key",
                       scan_freq="2412", ocsp=1, wait_connect=False)
        count = 0
        while True:
            ev = dev[0].wait_event(["CTRL-EVENT-EAP-STATUS",
                                    "CTRL-EVENT-EAP-SUCCESS"])
            if ev is None:
                raise Exception("Timeout on EAP status")
            if "CTRL-EVENT-EAP-SUCCESS" in ev:
                raise Exception("Unexpected EAP-Success")
            if 'bad certificate status response' in ev:
                break
            if 'certificate revoked' in ev:
                break
            count = count + 1
            if count > 10:
                raise Exception("Unexpected number of EAP status messages")

        ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"])
        if ev is None:
            raise Exception("Timeout on EAP failure report")
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()
    finally:
        os.unlink(fn)

def test_ap_wpa2_eap_tls_intermediate_ca_ocsp_multi_missing_resp(dev, apdev, params):
    """EAP-TLS with intermediate server/user CA and OCSP multi missing response"""
    check_ocsp_support(dev[0])
    check_ocsp_multi_support(dev[0])

    params = int_eap_server_params()
    params["ca_cert"] = "auth_serv/iCA-server/ca-and-root.pem"
    params["server_cert"] = "auth_serv/iCA-server/server.pem"
    params["private_key"] = "auth_serv/iCA-server/server.key"
    fn = ica_ocsp("server.pem")
    params["ocsp_stapling_response"] = fn
    try:
        hostapd.add_ap(apdev[0], params)
        tls = dev[0].request("GET tls_library")
        if "GnuTLS" in tls or "wolfSSL" in tls:
            ca_cert = "auth_serv/iCA-user/ca-and-root.pem"
            client_cert = "auth_serv/iCA-user/user_and_ica.pem"
        else:
            ca_cert = "auth_serv/iCA-user/ca-and-root.pem"
            client_cert = "auth_serv/iCA-user/user.pem"
        dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TLS",
                       identity="tls user",
                       ca_cert=ca_cert,
                       client_cert=client_cert,
                       private_key="auth_serv/iCA-user/user.key",
                       scan_freq="2412", ocsp=3, wait_connect=False)
        count = 0
        while True:
            ev = dev[0].wait_event(["CTRL-EVENT-EAP-STATUS",
                                    "CTRL-EVENT-EAP-SUCCESS"])
            if ev is None:
                raise Exception("Timeout on EAP status")
            if "CTRL-EVENT-EAP-SUCCESS" in ev:
                raise Exception("Unexpected EAP-Success")
            if 'bad certificate status response' in ev:
                break
            if 'certificate revoked' in ev:
                break
            count = count + 1
            if count > 10:
                raise Exception("Unexpected number of EAP status messages")

        ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"])
        if ev is None:
            raise Exception("Timeout on EAP failure report")
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()
    finally:
        os.unlink(fn)

def test_ap_wpa2_eap_tls_intermediate_ca_ocsp_multi(dev, apdev, params):
    """EAP-TLS with intermediate server/user CA and OCSP multi OK"""
    check_ocsp_support(dev[0])
    check_ocsp_multi_support(dev[0])

    params = int_eap_server_params()
    params["ca_cert"] = "auth_serv/iCA-server/ca-and-root.pem"
    params["server_cert"] = "auth_serv/iCA-server/server.pem"
    params["private_key"] = "auth_serv/iCA-server/server.key"
    fn = ica_ocsp("server.pem")
    fn2 = root_ocsp("auth_serv/iCA-server/cacert.pem")
    params["ocsp_stapling_response"] = fn

    with open(fn, "rb") as f:
        resp_server = f.read()
    with open(fn2, "rb") as f:
        resp_ica = f.read()

    fd3, fn3 = tempfile.mkstemp()
    try:
        f = os.fdopen(fd3, 'wb')
        f.write(struct.pack(">L", len(resp_server))[1:4])
        f.write(resp_server)
        f.write(struct.pack(">L", len(resp_ica))[1:4])
        f.write(resp_ica)
        f.close()

        params["ocsp_stapling_response_multi"] = fn3

        hostapd.add_ap(apdev[0], params)
        tls = dev[0].request("GET tls_library")
        if "GnuTLS" in tls or "wolfSSL" in tls:
            ca_cert = "auth_serv/iCA-user/ca-and-root.pem"
            client_cert = "auth_serv/iCA-user/user_and_ica.pem"
        else:
            ca_cert = "auth_serv/iCA-user/ca-and-root.pem"
            client_cert = "auth_serv/iCA-user/user.pem"
        dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TLS",
                       identity="tls user",
                       ca_cert=ca_cert,
                       client_cert=client_cert,
                       private_key="auth_serv/iCA-user/user.key",
                       scan_freq="2412", ocsp=3)
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()
    finally:
        os.unlink(fn)
        os.unlink(fn2)
        os.unlink(fn3)

def test_ap_wpa2_eap_tls_ocsp_multi_revoked(dev, apdev, params):
    """EAP-TLS and CA signed OCSP multi response (revoked)"""
    check_ocsp_support(dev[0])
    check_ocsp_multi_support(dev[0])
    check_pkcs12_support(dev[0])

    req = os.path.join(params['logdir'], "ocsp-req.der")
    ocsp_revoked = os.path.join(params['logdir'],
                                "ocsp-resp-ca-signed-revoked.der")
    ocsp_unknown = os.path.join(params['logdir'],
                                "ocsp-resp-ca-signed-unknown.der")
    ocsp_resp_ca_signed(req, ocsp_revoked, "-revoked")
    ocsp_resp_ca_signed(req, ocsp_unknown, "-unknown")

    with open(ocsp_revoked, "rb") as f:
        resp_revoked = f.read()
    with open(ocsp_unknown, "rb") as f:
        resp_unknown = f.read()

    fd, fn = tempfile.mkstemp()
    try:
        # This is not really a valid order of the OCSPResponse items in the
        # list, but this works for now to verify parsing and processing of
        # multiple responses.
        f = os.fdopen(fd, 'wb')
        f.write(struct.pack(">L", len(resp_unknown))[1:4])
        f.write(resp_unknown)
        f.write(struct.pack(">L", len(resp_revoked))[1:4])
        f.write(resp_revoked)
        f.write(struct.pack(">L", 0)[1:4])
        f.write(struct.pack(">L", len(resp_unknown))[1:4])
        f.write(resp_unknown)
        f.close()

        params = int_eap_server_params()
        params["ocsp_stapling_response_multi"] = fn
        hostapd.add_ap(apdev[0], params)
        dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TLS",
                       identity="tls user", ca_cert="auth_serv/ca.pem",
                       private_key="auth_serv/user.pkcs12",
                       private_key_passwd="whatever", ocsp=1,
                       wait_connect=False, scan_freq="2412")
        count = 0
        while True:
            ev = dev[0].wait_event(["CTRL-EVENT-EAP-STATUS",
                                    "CTRL-EVENT-EAP-SUCCESS"])
            if ev is None:
                raise Exception("Timeout on EAP status")
            if "CTRL-EVENT-EAP-SUCCESS" in ev:
                raise Exception("Unexpected EAP-Success")
            if 'bad certificate status response' in ev:
                break
            if 'certificate revoked' in ev:
                break
            count = count + 1
            if count > 10:
                raise Exception("Unexpected number of EAP status messages")
    finally:
        os.unlink(fn)

def test_ap_wpa2_eap_tls_domain_suffix_match_cn_full(dev, apdev):
    """WPA2-Enterprise using EAP-TLS and domain suffix match (CN)"""
    check_domain_match_full(dev[0])
    check_pkcs12_support(dev[0])
    params = int_eap_server_params()
    params["server_cert"] = "auth_serv/server-no-dnsname.pem"
    params["private_key"] = "auth_serv/server-no-dnsname.key"
    hostapd.add_ap(apdev[0], params)
    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TLS",
                   identity="tls user", ca_cert="auth_serv/ca.pem",
                   private_key="auth_serv/user.pkcs12",
                   private_key_passwd="whatever",
                   domain_suffix_match="server3.w1.fi",
                   scan_freq="2412")

def test_ap_wpa2_eap_tls_domain_match_cn(dev, apdev):
    """WPA2-Enterprise using EAP-TLS and domainmatch (CN)"""
    check_domain_match(dev[0])
    check_pkcs12_support(dev[0])
    params = int_eap_server_params()
    params["server_cert"] = "auth_serv/server-no-dnsname.pem"
    params["private_key"] = "auth_serv/server-no-dnsname.key"
    hostapd.add_ap(apdev[0], params)
    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TLS",
                   identity="tls user", ca_cert="auth_serv/ca.pem",
                   private_key="auth_serv/user.pkcs12",
                   private_key_passwd="whatever",
                   domain_match="server3.w1.fi",
                   scan_freq="2412")

def test_ap_wpa2_eap_tls_domain_suffix_match_cn(dev, apdev):
    """WPA2-Enterprise using EAP-TLS and domain suffix match (CN)"""
    check_domain_match_full(dev[0])
    check_pkcs12_support(dev[0])
    params = int_eap_server_params()
    params["server_cert"] = "auth_serv/server-no-dnsname.pem"
    params["private_key"] = "auth_serv/server-no-dnsname.key"
    hostapd.add_ap(apdev[0], params)
    dev[1].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TLS",
                   identity="tls user", ca_cert="auth_serv/ca.pem",
                   private_key="auth_serv/user.pkcs12",
                   private_key_passwd="whatever",
                   domain_suffix_match="w1.fi",
                   scan_freq="2412")

def test_ap_wpa2_eap_tls_domain_suffix_mismatch_cn(dev, apdev):
    """WPA2-Enterprise using EAP-TLS and domain suffix mismatch (CN)"""
    check_domain_suffix_match(dev[0])
    check_pkcs12_support(dev[0])
    params = int_eap_server_params()
    params["server_cert"] = "auth_serv/server-no-dnsname.pem"
    params["private_key"] = "auth_serv/server-no-dnsname.key"
    hostapd.add_ap(apdev[0], params)
    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TLS",
                   identity="tls user", ca_cert="auth_serv/ca.pem",
                   private_key="auth_serv/user.pkcs12",
                   private_key_passwd="whatever",
                   domain_suffix_match="example.com",
                   wait_connect=False,
                   scan_freq="2412")
    dev[1].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TLS",
                   identity="tls user", ca_cert="auth_serv/ca.pem",
                   private_key="auth_serv/user.pkcs12",
                   private_key_passwd="whatever",
                   domain_suffix_match="erver3.w1.fi",
                   wait_connect=False,
                   scan_freq="2412")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"])
    if ev is None:
        raise Exception("Timeout on EAP failure report")
    ev = dev[1].wait_event(["CTRL-EVENT-EAP-FAILURE"])
    if ev is None:
        raise Exception("Timeout on EAP failure report (2)")

def test_ap_wpa2_eap_tls_domain_mismatch_cn(dev, apdev):
    """WPA2-Enterprise using EAP-TLS and domain mismatch (CN)"""
    check_domain_match(dev[0])
    check_pkcs12_support(dev[0])
    params = int_eap_server_params()
    params["server_cert"] = "auth_serv/server-no-dnsname.pem"
    params["private_key"] = "auth_serv/server-no-dnsname.key"
    hostapd.add_ap(apdev[0], params)
    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TLS",
                   identity="tls user", ca_cert="auth_serv/ca.pem",
                   private_key="auth_serv/user.pkcs12",
                   private_key_passwd="whatever",
                   domain_match="example.com",
                   wait_connect=False,
                   scan_freq="2412")
    dev[1].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TLS",
                   identity="tls user", ca_cert="auth_serv/ca.pem",
                   private_key="auth_serv/user.pkcs12",
                   private_key_passwd="whatever",
                   domain_match="w1.fi",
                   wait_connect=False,
                   scan_freq="2412")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"])
    if ev is None:
        raise Exception("Timeout on EAP failure report")
    ev = dev[1].wait_event(["CTRL-EVENT-EAP-FAILURE"])
    if ev is None:
        raise Exception("Timeout on EAP failure report (2)")

def test_ap_wpa2_eap_ttls_expired_cert(dev, apdev):
    """WPA2-Enterprise using EAP-TTLS and expired certificate"""
    skip_with_fips(dev[0])
    params = int_eap_server_params()
    params["server_cert"] = "auth_serv/server-expired.pem"
    params["private_key"] = "auth_serv/server-expired.key"
    hostapd.add_ap(apdev[0], params)
    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TTLS",
                   identity="mschap user", password="password",
                   ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAP",
                   wait_connect=False,
                   scan_freq="2412")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-TLS-CERT-ERROR"])
    if ev is None:
        raise Exception("Timeout on EAP certificate error report")
    if "reason=4" not in ev or "certificate has expired" not in ev:
        raise Exception("Unexpected failure reason: " + ev)
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"])
    if ev is None:
        raise Exception("Timeout on EAP failure report")

def test_ap_wpa2_eap_ttls_ignore_expired_cert(dev, apdev):
    """WPA2-Enterprise using EAP-TTLS and ignore certificate expiration"""
    skip_with_fips(dev[0])
    params = int_eap_server_params()
    params["server_cert"] = "auth_serv/server-expired.pem"
    params["private_key"] = "auth_serv/server-expired.key"
    hostapd.add_ap(apdev[0], params)
    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TTLS",
                   identity="mschap user", password="password",
                   ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAP",
                   phase1="tls_disable_time_checks=1",
                   scan_freq="2412")

def test_ap_wpa2_eap_ttls_long_duration(dev, apdev):
    """WPA2-Enterprise using EAP-TTLS and long certificate duration"""
    skip_with_fips(dev[0])
    params = int_eap_server_params()
    params["server_cert"] = "auth_serv/server-long-duration.pem"
    params["private_key"] = "auth_serv/server-long-duration.key"
    hostapd.add_ap(apdev[0], params)
    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TTLS",
                   identity="mschap user", password="password",
                   ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAP",
                   scan_freq="2412")

def test_ap_wpa2_eap_ttls_server_cert_eku_client(dev, apdev):
    """WPA2-Enterprise using EAP-TTLS and server cert with client EKU"""
    skip_with_fips(dev[0])
    params = int_eap_server_params()
    params["server_cert"] = "auth_serv/server-eku-client.pem"
    params["private_key"] = "auth_serv/server-eku-client.key"
    hostapd.add_ap(apdev[0], params)
    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TTLS",
                   identity="mschap user", password="password",
                   ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAP",
                   wait_connect=False,
                   scan_freq="2412")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"])
    if ev is None:
        raise Exception("Timeout on EAP failure report")

def test_ap_wpa2_eap_ttls_server_cert_eku_client_server(dev, apdev):
    """WPA2-Enterprise using EAP-TTLS and server cert with client and server EKU"""
    skip_with_fips(dev[0])
    params = int_eap_server_params()
    params["server_cert"] = "auth_serv/server-eku-client-server.pem"
    params["private_key"] = "auth_serv/server-eku-client-server.key"
    hostapd.add_ap(apdev[0], params)
    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TTLS",
                   identity="mschap user", password="password",
                   ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAP",
                   scan_freq="2412")

def test_ap_wpa2_eap_ttls_server_pkcs12(dev, apdev):
    """WPA2-Enterprise using EAP-TTLS and server PKCS#12 file"""
    skip_with_fips(dev[0])
    params = int_eap_server_params()
    del params["server_cert"]
    params["private_key"] = "auth_serv/server.pkcs12"
    hostapd.add_ap(apdev[0], params)
    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TTLS",
                   identity="mschap user", password="password",
                   ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAP",
                   scan_freq="2412")

def test_ap_wpa2_eap_ttls_server_pkcs12_extra(dev, apdev):
    """EAP-TTLS and server PKCS#12 file with extra certs"""
    skip_with_fips(dev[0])
    params = int_eap_server_params()
    del params["server_cert"]
    params["private_key"] = "auth_serv/server-extra.pkcs12"
    params["private_key_passwd"] = "whatever"
    hostapd.add_ap(apdev[0], params)
    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TTLS",
                   identity="mschap user", password="password",
                   ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAP",
                   scan_freq="2412")

def test_ap_wpa2_eap_ttls_dh_params(dev, apdev):
    """WPA2-Enterprise connection using EAP-TTLS/CHAP and setting DH params"""
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "TTLS", "pap user",
                anonymous_identity="ttls", password="password",
                ca_cert="auth_serv/ca.der", phase2="auth=PAP",
                dh_file="auth_serv/dh.conf")

def test_ap_wpa2_eap_ttls_dh_params_dsa(dev, apdev):
    """WPA2-Enterprise connection using EAP-TTLS and setting DH params (DSA)"""
    check_dh_dsa_support(dev[0])
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "TTLS", "pap user",
                anonymous_identity="ttls", password="password",
                ca_cert="auth_serv/ca.der", phase2="auth=PAP",
                dh_file="auth_serv/dsaparam.pem")

def test_ap_wpa2_eap_ttls_dh_params_not_found(dev, apdev):
    """EAP-TTLS and DH params file not found"""
    skip_with_fips(dev[0])
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hostapd.add_ap(apdev[0], params)
    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TTLS",
                   identity="mschap user", password="password",
                   ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAP",
                   dh_file="auth_serv/dh-no-such-file.conf",
                   scan_freq="2412", wait_connect=False)
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"])
    if ev is None:
        raise Exception("EAP failure timed out")
    dev[0].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected()

def test_ap_wpa2_eap_ttls_dh_params_invalid(dev, apdev):
    """EAP-TTLS and invalid DH params file"""
    skip_with_fips(dev[0])
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hostapd.add_ap(apdev[0], params)
    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TTLS",
                   identity="mschap user", password="password",
                   ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAP",
                   dh_file="auth_serv/ca.pem",
                   scan_freq="2412", wait_connect=False)
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"])
    if ev is None:
        raise Exception("EAP failure timed out")
    dev[0].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected()

def test_ap_wpa2_eap_ttls_dh_params_blob(dev, apdev):
    """WPA2-Enterprise connection using EAP-TTLS/CHAP and setting DH params from blob"""
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    dh = read_pem("auth_serv/dh2.conf")
    if "OK" not in dev[0].request("SET blob dhparams " + binascii.hexlify(dh).decode()):
        raise Exception("Could not set dhparams blob")
    eap_connect(dev[0], hapd, "TTLS", "pap user",
                anonymous_identity="ttls", password="password",
                ca_cert="auth_serv/ca.der", phase2="auth=PAP",
                dh_file="blob://dhparams")

def test_ap_wpa2_eap_ttls_dh_params_server(dev, apdev):
    """WPA2-Enterprise using EAP-TTLS and alternative server dhparams"""
    params = int_eap_server_params()
    params["dh_file"] = "auth_serv/dh2.conf"
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "TTLS", "pap user",
                anonymous_identity="ttls", password="password",
                ca_cert="auth_serv/ca.der", phase2="auth=PAP")

def test_ap_wpa2_eap_ttls_dh_params_dsa_server(dev, apdev):
    """WPA2-Enterprise using EAP-TTLS and alternative server dhparams (DSA)"""
    params = int_eap_server_params()
    params["dh_file"] = "auth_serv/dsaparam.pem"
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "TTLS", "pap user",
                anonymous_identity="ttls", password="password",
                ca_cert="auth_serv/ca.der", phase2="auth=PAP")

def test_ap_wpa2_eap_ttls_dh_params_not_found(dev, apdev):
    """EAP-TLS server and dhparams file not found"""
    params = int_eap_server_params()
    params["dh_file"] = "auth_serv/dh-no-such-file.conf"
    hapd = hostapd.add_ap(apdev[0], params, no_enable=True)
    if "FAIL" not in hapd.request("ENABLE"):
        raise Exception("Invalid configuration accepted")

def test_ap_wpa2_eap_ttls_dh_params_invalid(dev, apdev):
    """EAP-TLS server and invalid dhparams file"""
    params = int_eap_server_params()
    params["dh_file"] = "auth_serv/ca.pem"
    hapd = hostapd.add_ap(apdev[0], params, no_enable=True)
    if "FAIL" not in hapd.request("ENABLE"):
        raise Exception("Invalid configuration accepted")

def test_ap_wpa2_eap_reauth(dev, apdev):
    """WPA2-Enterprise and Authenticator forcing reauthentication"""
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    params['eap_reauth_period'] = '2'
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "PAX", "pax.user@example.com",
                password_hex="0123456789abcdef0123456789abcdef")
    logger.info("Wait for reauthentication")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED"], timeout=10)
    if ev is None:
        raise Exception("Timeout on reauthentication")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-SUCCESS"], timeout=10)
    if ev is None:
        raise Exception("Timeout on reauthentication")
    for i in range(0, 20):
        state = dev[0].get_status_field("wpa_state")
        if state == "COMPLETED":
            break
        time.sleep(0.1)
    if state != "COMPLETED":
        raise Exception("Reauthentication did not complete")

def test_ap_wpa2_eap_reauth_ptk_rekey_blocked_ap(dev, apdev):
    """WPA2-Enterprise and Authenticator forcing reauthentication with PTK rekey blocked on AP"""
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    params['eap_reauth_period'] = '2'
    params['wpa_deny_ptk0_rekey'] = '2'
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "PAX", "pax.user@example.com",
                password_hex="0123456789abcdef0123456789abcdef")
    logger.info("Wait for disconnect due to reauth")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED",
                            "CTRL-EVENT-DISCONNECTED"], timeout=10)
    if ev is None:
        raise Exception("Timeout on reauthentication")
    if "CTRL-EVENT-EAP-STARTED" in ev:
        raise Exception("Reauthentication without disconnect")

    ev = dev[0].wait_event(["WPA: Key negotiation completed"], timeout=1)
    if ev is None:
        raise Exception("Timeout on reconnect")

def test_ap_wpa2_eap_reauth_ptk_rekey_blocked_sta(dev, apdev):
    """WPA2-Enterprise and Authenticator forcing reauthentication with PTK rekey blocked on station"""
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    params['eap_reauth_period'] = '2'
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "PAX", "pax.user@example.com",
                password_hex="0123456789abcdef0123456789abcdef",
                wpa_deny_ptk0_rekey="2")
    logger.info("Wait for disconnect due to reauth")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED",
                            "CTRL-EVENT-DISCONNECTED"], timeout=10)
    if ev is None:
        raise Exception("Timeout on reauthentication")
    if "CTRL-EVENT-EAP-STARTED" in ev:
        raise Exception("Reauthentication without disconnect")

    ev = dev[0].wait_event(["WPA: Key negotiation completed"], timeout=1)
    if ev is None:
        raise Exception("Timeout on reconnect")

def test_ap_wpa2_eap_request_identity_message(dev, apdev):
    """Optional displayable message in EAP Request-Identity"""
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    params['eap_message'] = 'hello\\0networkid=netw,nasid=foo,portid=0,NAIRealms=example.com'
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "PAX", "pax.user@example.com",
                password_hex="0123456789abcdef0123456789abcdef")

def test_ap_wpa2_eap_sim_aka_result_ind(dev, apdev):
    """WPA2-Enterprise using EAP-SIM/AKA and protected result indication"""
    check_hlr_auc_gw_support()
    params = int_eap_server_params()
    params['eap_sim_db'] = "unix:/tmp/hlr_auc_gw.sock"
    params['eap_sim_aka_result_ind'] = "1"
    hapd = hostapd.add_ap(apdev[0], params)

    eap_connect(dev[0], hapd, "SIM", "1232010000000000",
                password="90dca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb82581",
                phase1="result_ind=1")
    eap_reauth(dev[0], "SIM")
    eap_connect(dev[1], hapd, "SIM", "1232010000000000",
                password="90dca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb82581")

    dev[0].request("REMOVE_NETWORK all")
    dev[1].request("REMOVE_NETWORK all")

    eap_connect(dev[0], hapd, "AKA", "0232010000000000",
                password="90dca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb82581:000000000123",
                phase1="result_ind=1")
    eap_reauth(dev[0], "AKA")
    eap_connect(dev[1], hapd, "AKA", "0232010000000000",
                password="90dca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb82581:000000000123")

    dev[0].request("REMOVE_NETWORK all")
    dev[1].request("REMOVE_NETWORK all")

    eap_connect(dev[0], hapd, "AKA'", "6555444333222111",
                password="5122250214c33e723a5dd523fc145fc0:981d464c7c52eb6e5036234984ad0bcf:000000000123",
                phase1="result_ind=1")
    eap_reauth(dev[0], "AKA'")
    eap_connect(dev[1], hapd, "AKA'", "6555444333222111",
                password="5122250214c33e723a5dd523fc145fc0:981d464c7c52eb6e5036234984ad0bcf:000000000123")

def test_ap_wpa2_eap_sim_zero_db_timeout(dev, apdev):
    """WPA2-Enterprise using EAP-SIM with zero database timeout"""
    check_hlr_auc_gw_support()
    params = int_eap_server_params()
    params['eap_sim_db'] = "unix:/tmp/hlr_auc_gw.sock"
    params['eap_sim_db_timeout'] = "0"
    params['disable_pmksa_caching'] = '1'
    hapd = hostapd.add_ap(apdev[0], params)

    # Run multiple iterations to make it more likely to hit the case where the
    # DB request times out and response is lost.
    for i in range(20):
        dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="SIM",
                       identity="1232010000000000",
                       password="90dca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb82581",
                       wait_connect=False, scan_freq="2412")
        ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED",
                                "CTRL-EVENT-DISCONNECTED"],
                               timeout=15)
        if ev is None:
            raise Exception("No connection result")
        dev[0].request("REMOVE_NETWORK all")
        if "CTRL-EVENT-DISCONNECTED" in ev:
            break
        dev[0].wait_disconnected()
        hapd.ping()

def test_ap_wpa2_eap_too_many_roundtrips(dev, apdev):
    """WPA2-Enterprise connection resulting in too many EAP roundtrips"""
    skip_with_fips(dev[0])
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hostapd.add_ap(apdev[0], params)
    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP WPA-EAP-SHA256",
                   eap="TTLS", identity="mschap user",
                   wait_connect=False, scan_freq="2412", ieee80211w="1",
                   anonymous_identity="ttls", password="password",
                   ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAP",
                   fragment_size="4")
    ev = dev[0].wait_event(["EAP: more than",
                            "CTRL-EVENT-EAP-SUCCESS"], timeout=20)
    if ev is None or "EAP: more than" not in ev:
        raise Exception("EAP roundtrip limit not reached")

def test_ap_wpa2_eap_too_many_roundtrips_server(dev, apdev):
    """WPA2-Enterprise connection resulting in too many EAP roundtrips (server)"""
    run_ap_wpa2_eap_too_many_roundtrips_server(dev, apdev, 10, 10)

def test_ap_wpa2_eap_too_many_roundtrips_server2(dev, apdev):
    """WPA2-Enterprise connection resulting in too many EAP roundtrips (server)"""
    run_ap_wpa2_eap_too_many_roundtrips_server(dev, apdev, 10, 1)

def run_ap_wpa2_eap_too_many_roundtrips_server(dev, apdev, max_rounds,
                                               max_rounds_short):
    skip_with_fips(dev[0])
    params = int_eap_server_params()
    params["max_auth_rounds"] = str(max_rounds)
    params["max_auth_rounds_short"] = str(max_rounds_short)
    hostapd.add_ap(apdev[0], params)
    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP WPA-EAP-SHA256",
                   eap="TTLS", identity="mschap user",
                   wait_connect=False, scan_freq="2412", ieee80211w="1",
                   anonymous_identity="ttls", password="password",
                   ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAP",
                   fragment_size="4")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE",
                            "CTRL-EVENT-EAP-SUCCESS"], timeout=10)
    dev[0].request("DISCONNECT")
    if ev is None or "SUCCESS" in ev:
        raise Exception("EAP roundtrip limit not reported")

def test_ap_wpa2_eap_expanded_nak(dev, apdev):
    """WPA2-Enterprise connection with EAP resulting in expanded NAK"""
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hostapd.add_ap(apdev[0], params)
    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP WPA-EAP-SHA256",
                   eap="PSK", identity="vendor-test",
                   password_hex="ff23456789abcdef0123456789abcdef",
                   wait_connect=False)

    found = False
    for i in range(0, 5):
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-STATUS"], timeout=16)
        if ev is None:
            raise Exception("Association and EAP start timed out")
        if "refuse proposed method" in ev:
            found = True
            break
    if not found:
        raise Exception("Unexpected EAP status: " + ev)

    ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"])
    if ev is None:
        raise Exception("EAP failure timed out")

def test_ap_wpa2_eap_sql(dev, apdev, params):
    """WPA2-Enterprise connection using SQLite for user DB"""
    skip_with_fips(dev[0])
    try:
        import sqlite3
    except ImportError:
        raise HwsimSkip("No sqlite3 module available")
    dbfile = os.path.join(params['logdir'], "eap-user.db")
    try:
        os.remove(dbfile)
    except:
        pass
    con = sqlite3.connect(dbfile)
    with con:
        cur = con.cursor()
        cur.execute("CREATE TABLE users(identity TEXT PRIMARY KEY, methods TEXT, password TEXT, remediation TEXT, phase2 INTEGER)")
        cur.execute("CREATE TABLE wildcards(identity TEXT PRIMARY KEY, methods TEXT)")
        cur.execute("INSERT INTO users(identity,methods,password,phase2) VALUES ('user-pap','TTLS-PAP','password',1)")
        cur.execute("INSERT INTO users(identity,methods,password,phase2) VALUES ('user-chap','TTLS-CHAP','password',1)")
        cur.execute("INSERT INTO users(identity,methods,password,phase2) VALUES ('user-mschap','TTLS-MSCHAP','password',1)")
        cur.execute("INSERT INTO users(identity,methods,password,phase2) VALUES ('user-mschapv2','TTLS-MSCHAPV2','password',1)")
        cur.execute("INSERT INTO wildcards(identity,methods) VALUES ('','TTLS,TLS')")
        cur.execute("CREATE TABLE authlog(timestamp TEXT, session TEXT, nas_ip TEXT, username TEXT, note TEXT)")

    try:
        params = int_eap_server_params()
        params["eap_user_file"] = "sqlite:" + dbfile
        hapd = hostapd.add_ap(apdev[0], params)
        eap_connect(dev[0], hapd, "TTLS", "user-mschapv2",
                    anonymous_identity="ttls", password="password",
                    ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2")
        dev[0].request("REMOVE_NETWORK all")
        eap_connect(dev[1], hapd, "TTLS", "user-mschap",
                    anonymous_identity="ttls", password="password",
                    ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAP")
        dev[1].request("REMOVE_NETWORK all")
        eap_connect(dev[0], hapd, "TTLS", "user-chap",
                    anonymous_identity="ttls", password="password",
                    ca_cert="auth_serv/ca.pem", phase2="auth=CHAP")
        eap_connect(dev[1], hapd, "TTLS", "user-pap",
                    anonymous_identity="ttls", password="password",
                    ca_cert="auth_serv/ca.pem", phase2="auth=PAP")
        dev[0].request("REMOVE_NETWORK all")
        dev[1].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()
        dev[1].wait_disconnected()
        hapd.disable()
        hapd.enable()
        eap_connect(dev[0], hapd, "TTLS", "user-mschapv2",
                    anonymous_identity="ttls", password="password",
                    ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2")
    finally:
        os.remove(dbfile)

def test_ap_wpa2_eap_non_ascii_identity(dev, apdev):
    """WPA2-Enterprise connection attempt using non-ASCII identity"""
    params = int_eap_server_params()
    hostapd.add_ap(apdev[0], params)
    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TTLS",
                   identity="\x80", password="password", wait_connect=False)
    dev[1].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TTLS",
                   identity="a\x80", password="password", wait_connect=False)
    for i in range(0, 2):
        ev = dev[i].wait_event(["CTRL-EVENT-EAP-STARTED"], timeout=16)
        if ev is None:
            raise Exception("Association and EAP start timed out")
        ev = dev[i].wait_event(["CTRL-EVENT-EAP-METHOD"], timeout=10)
        if ev is None:
            raise Exception("EAP method selection timed out")

def test_ap_wpa2_eap_non_ascii_identity2(dev, apdev):
    """WPA2-Enterprise connection attempt using non-ASCII identity"""
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hostapd.add_ap(apdev[0], params)
    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TTLS",
                   identity="\x80", password="password", wait_connect=False)
    dev[1].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TTLS",
                   identity="a\x80", password="password", wait_connect=False)
    for i in range(0, 2):
        ev = dev[i].wait_event(["CTRL-EVENT-EAP-STARTED"], timeout=16)
        if ev is None:
            raise Exception("Association and EAP start timed out")
        ev = dev[i].wait_event(["CTRL-EVENT-EAP-METHOD"], timeout=10)
        if ev is None:
            raise Exception("EAP method selection timed out")

def test_openssl_cipher_suite_config_wpas(dev, apdev):
    """OpenSSL cipher suite configuration on wpa_supplicant"""
    tls = dev[0].request("GET tls_library")
    if not tls.startswith("OpenSSL"):
        raise HwsimSkip("TLS library is not OpenSSL: " + tls)
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "TTLS", "pap user",
                anonymous_identity="ttls", password="password",
                openssl_ciphers="AES128",
                ca_cert="auth_serv/ca.pem", phase2="auth=PAP")
    eap_connect(dev[1], hapd, "TTLS", "pap user",
                anonymous_identity="ttls", password="password",
                openssl_ciphers="EXPORT",
                ca_cert="auth_serv/ca.pem", phase2="auth=PAP",
                expect_failure=True, maybe_local_error=True)
    dev[2].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TTLS",
                   identity="pap user", anonymous_identity="ttls",
                   password="password",
                   openssl_ciphers="FOO",
                   ca_cert="auth_serv/ca.pem", phase2="auth=PAP",
                   wait_connect=False)
    ev = dev[2].wait_event(["CTRL-EVENT-EAP-FAILURE"], timeout=10)
    if ev is None:
        raise Exception("EAP failure after invalid openssl_ciphers not reported")
    dev[2].request("DISCONNECT")

def test_openssl_cipher_suite_config_hapd(dev, apdev):
    """OpenSSL cipher suite configuration on hostapd"""
    tls = dev[0].request("GET tls_library")
    if not tls.startswith("OpenSSL"):
        raise HwsimSkip("wpa_supplicant TLS library is not OpenSSL: " + tls)
    params = int_eap_server_params()
    params['openssl_ciphers'] = "AES256"
    hapd = hostapd.add_ap(apdev[0], params)
    tls = hapd.request("GET tls_library")
    if not tls.startswith("OpenSSL"):
        raise HwsimSkip("hostapd TLS library is not OpenSSL: " + tls)
    eap_connect(dev[0], hapd, "TTLS", "pap user",
                anonymous_identity="ttls", password="password",
                ca_cert="auth_serv/ca.pem", phase2="auth=PAP")
    eap_connect(dev[1], hapd, "TTLS", "pap user",
                anonymous_identity="ttls", password="password",
                openssl_ciphers="AES128",
                ca_cert="auth_serv/ca.pem", phase2="auth=PAP",
                expect_failure=True)
    eap_connect(dev[2], hapd, "TTLS", "pap user",
                anonymous_identity="ttls", password="password",
                openssl_ciphers="HIGH:!ADH",
                ca_cert="auth_serv/ca.pem", phase2="auth=PAP")

    params['openssl_ciphers'] = "FOO"
    hapd2 = hostapd.add_ap(apdev[1], params, no_enable=True)
    if "FAIL" not in hapd2.request("ENABLE"):
        if "run=OpenSSL 1.1.1" in tls:
            logger.info("Ignore acceptance of an invalid openssl_ciphers value with OpenSSL 1.1.1")
        else:
            raise Exception("Invalid openssl_ciphers value accepted")

def test_wpa2_eap_ttls_pap_key_lifetime_in_memory(dev, apdev, params):
    """Key lifetime in memory with WPA2-Enterprise using EAP-TTLS/PAP"""
    p = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], p)
    password = "63d2d21ac3c09ed567ee004a34490f1d16e7fa5835edf17ddba70a63f1a90a25"
    id = eap_connect(dev[0], hapd, "TTLS", "pap-secret",
                     anonymous_identity="ttls", password=password,
                     ca_cert="auth_serv/ca.pem", phase2="auth=PAP")
    run_eap_key_lifetime_in_memory(dev, params, id, password)

def test_wpa2_eap_peap_gtc_key_lifetime_in_memory(dev, apdev, params):
    """Key lifetime in memory with WPA2-Enterprise using PEAP/GTC"""
    p = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], p)
    password = "63d2d21ac3c09ed567ee004a34490f1d16e7fa5835edf17ddba70a63f1a90a25"
    id = eap_connect(dev[0], hapd, "PEAP", "user-secret",
                     anonymous_identity="peap", password=password,
                     ca_cert="auth_serv/ca.pem", phase2="auth=GTC")
    run_eap_key_lifetime_in_memory(dev, params, id, password)

def run_eap_key_lifetime_in_memory(dev, params, id, password):
    pid = find_wpas_process(dev[0])

    # The decrypted copy of GTK is freed only after the CTRL-EVENT-CONNECTED
    # event has been delivered, so verify that wpa_supplicant has returned to
    # eloop before reading process memory.
    time.sleep(1)
    dev[0].ping()
    password = password.encode()
    buf = read_process_memory(pid, password)

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    dev[0].relog()
    msk = None
    emsk = None
    pmk = None
    ptk = None
    gtk = None
    with open(os.path.join(params['logdir'], 'log0'), 'r') as f:
        for l in f.readlines():
            if "EAP-TTLS: Derived key - hexdump" in l or \
               "EAP-PEAP: Derived key - hexdump" in l:
                val = l.strip().split(':')[3].replace(' ', '')
                msk = binascii.unhexlify(val)
            if "EAP-TTLS: Derived EMSK - hexdump" in l or \
               "EAP-PEAP: Derived EMSK - hexdump" in l:
                val = l.strip().split(':')[3].replace(' ', '')
                emsk = binascii.unhexlify(val)
            if "WPA: PMK - hexdump" in l:
                val = l.strip().split(':')[3].replace(' ', '')
                pmk = binascii.unhexlify(val)
            if "WPA: PTK - hexdump" in l:
                val = l.strip().split(':')[3].replace(' ', '')
                ptk = binascii.unhexlify(val)
            if "WPA: Group Key - hexdump" in l:
                val = l.strip().split(':')[3].replace(' ', '')
                gtk = binascii.unhexlify(val)
    if not msk or not emsk or not pmk or not ptk or not gtk:
        raise Exception("Could not find keys from debug log")
    if len(gtk) != 16:
        raise Exception("Unexpected GTK length")

    kck = ptk[0:16]
    kek = ptk[16:32]
    tk = ptk[32:48]

    fname = os.path.join(params['logdir'],
                         'wpa2_eap_ttls_pap_key_lifetime_in_memory.memctx-')

    logger.info("Checking keys in memory while associated")
    get_key_locations(buf, password, "Password")
    get_key_locations(buf, pmk, "PMK")
    get_key_locations(buf, msk, "MSK")
    get_key_locations(buf, emsk, "EMSK")
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
    # Note: PMK is in PMKSA cache and EAP fast re-auth data

    get_key_locations(buf, password, "Password")
    get_key_locations(buf, pmk, "PMK")
    get_key_locations(buf, msk, "MSK")
    get_key_locations(buf, emsk, "EMSK")
    verify_not_present(buf, kck, fname, "KCK")
    verify_not_present(buf, kek, fname, "KEK")
    verify_not_present(buf, tk, fname, "TK")
    if gtk in buf:
        get_key_locations(buf, gtk, "GTK")
    verify_not_present(buf, gtk, fname, "GTK")

    dev[0].request("PMKSA_FLUSH")
    dev[0].set_network_quoted(id, "identity", "foo")
    logger.info("Checking keys in memory after PMKSA cache and EAP fast reauth flush")
    buf = read_process_memory(pid, password)
    get_key_locations(buf, password, "Password")
    get_key_locations(buf, pmk, "PMK")
    get_key_locations(buf, msk, "MSK")
    get_key_locations(buf, emsk, "EMSK")
    verify_not_present(buf, pmk, fname, "PMK")

    dev[0].request("REMOVE_NETWORK all")

    logger.info("Checking keys in memory after network profile removal")
    buf = read_process_memory(pid, password)

    get_key_locations(buf, password, "Password")
    get_key_locations(buf, pmk, "PMK")
    get_key_locations(buf, msk, "MSK")
    get_key_locations(buf, emsk, "EMSK")
    verify_not_present(buf, password, fname, "password")
    verify_not_present(buf, pmk, fname, "PMK")
    verify_not_present(buf, kck, fname, "KCK")
    verify_not_present(buf, kek, fname, "KEK")
    verify_not_present(buf, tk, fname, "TK")
    verify_not_present(buf, gtk, fname, "GTK")
    verify_not_present(buf, msk, fname, "MSK")
    verify_not_present(buf, emsk, fname, "EMSK")

def test_ap_wpa2_eap_unexpected_wep_eapol_key(dev, apdev):
    """WPA2-Enterprise connection and unexpected WEP EAPOL-Key"""
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = apdev[0]['bssid']
    eap_connect(dev[0], hapd, "TTLS", "pap user",
                anonymous_identity="ttls", password="password",
                ca_cert="auth_serv/ca.pem", phase2="auth=PAP")

    # Send unexpected WEP EAPOL-Key; this gets dropped
    res = dev[0].request("EAPOL_RX " + bssid + " 0203002c0100000000000000000000000000000000000000000000000000000000000000000000000000000000000000")
    if "OK" not in res:
        raise Exception("EAPOL_RX to wpa_supplicant failed")

def test_ap_wpa2_eap_in_bridge(dev, apdev):
    """WPA2-EAP and wpas interface in a bridge"""
    br_ifname = 'sta-br0'
    ifname = 'wlan5'
    try:
        _test_ap_wpa2_eap_in_bridge(dev, apdev)
    finally:
        subprocess.call(['ip', 'link', 'set', 'dev', br_ifname, 'down'])
        subprocess.call(['brctl', 'delif', br_ifname, ifname])
        subprocess.call(['brctl', 'delbr', br_ifname])
        subprocess.call(['iw', ifname, 'set', '4addr', 'off'])

def _test_ap_wpa2_eap_in_bridge(dev, apdev):
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)

    br_ifname = 'sta-br0'
    ifname = 'wlan5'
    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    subprocess.call(['brctl', 'addbr', br_ifname])
    subprocess.call(['brctl', 'setfd', br_ifname, '0'])
    subprocess.call(['ip', 'link', 'set', 'dev', br_ifname, 'up'])
    subprocess.call(['iw', ifname, 'set', '4addr', 'on'])
    subprocess.check_call(['brctl', 'addif', br_ifname, ifname])
    wpas.interface_add(ifname, br_ifname=br_ifname)
    wpas.dump_monitor()

    id = eap_connect(wpas, hapd, "PAX", "pax.user@example.com",
                     password_hex="0123456789abcdef0123456789abcdef")
    wpas.dump_monitor()
    eap_reauth(wpas, "PAX")
    wpas.dump_monitor()
    # Try again as a regression test for packet socket workaround
    eap_reauth(wpas, "PAX")
    wpas.dump_monitor()
    wpas.request("DISCONNECT")
    wpas.wait_disconnected()
    wpas.dump_monitor()
    wpas.request("RECONNECT")
    wpas.wait_connected()
    wpas.dump_monitor()

def test_ap_wpa2_eap_session_ticket(dev, apdev):
    """WPA2-Enterprise connection using EAP-TTLS and TLS session ticket enabled"""
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    key_mgmt = hapd.get_config()['key_mgmt']
    if key_mgmt.split(' ')[0] != "WPA-EAP":
        raise Exception("Unexpected GET_CONFIG(key_mgmt): " + key_mgmt)
    eap_connect(dev[0], hapd, "TTLS", "pap user",
                anonymous_identity="ttls", password="password",
                ca_cert="auth_serv/ca.pem",
                phase1="tls_disable_session_ticket=0", phase2="auth=PAP")
    eap_reauth(dev[0], "TTLS")

def test_ap_wpa2_eap_no_workaround(dev, apdev):
    """WPA2-Enterprise connection using EAP-TTLS and eap_workaround=0"""
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    key_mgmt = hapd.get_config()['key_mgmt']
    if key_mgmt.split(' ')[0] != "WPA-EAP":
        raise Exception("Unexpected GET_CONFIG(key_mgmt): " + key_mgmt)
    eap_connect(dev[0], hapd, "TTLS", "pap user",
                anonymous_identity="ttls", password="password",
                ca_cert="auth_serv/ca.pem", eap_workaround='0',
                phase2="auth=PAP")
    eap_reauth(dev[0], "TTLS")

def test_ap_wpa2_eap_tls_check_crl(dev, apdev):
    """EAP-TLS and server checking CRL"""
    params = int_eap_server_params()
    params['check_crl'] = '1'
    hapd = hostapd.add_ap(apdev[0], params)

    # check_crl=1 and no CRL available --> reject connection
    eap_connect(dev[0], hapd, "TLS", "tls user", ca_cert="auth_serv/ca.pem",
                client_cert="auth_serv/user.pem",
                private_key="auth_serv/user.key", expect_failure=True)
    dev[0].request("REMOVE_NETWORK all")

    hapd.disable()
    hapd.set("ca_cert", "auth_serv/ca-and-crl.pem")
    hapd.enable()

    # check_crl=1 and valid CRL --> accept
    eap_connect(dev[0], hapd, "TLS", "tls user", ca_cert="auth_serv/ca.pem",
                client_cert="auth_serv/user.pem",
                private_key="auth_serv/user.key")
    dev[0].request("REMOVE_NETWORK all")

    hapd.disable()
    hapd.set("check_crl", "2")
    hapd.enable()

    # check_crl=2 and valid CRL --> accept
    eap_connect(dev[0], hapd, "TLS", "tls user", ca_cert="auth_serv/ca.pem",
                client_cert="auth_serv/user.pem",
                private_key="auth_serv/user.key")
    dev[0].request("REMOVE_NETWORK all")

def test_ap_wpa2_eap_tls_check_crl_not_strict(dev, apdev):
    """EAP-TLS and server checking CRL with check_crl_strict=0"""
    params = int_eap_server_params()
    params['check_crl'] = '1'
    params['ca_cert'] = "auth_serv/ca-and-crl-expired.pem"
    hapd = hostapd.add_ap(apdev[0], params)

    # check_crl_strict=1 and expired CRL --> reject connection
    eap_connect(dev[0], hapd, "TLS", "tls user", ca_cert="auth_serv/ca.pem",
                client_cert="auth_serv/user.pem",
                private_key="auth_serv/user.key", expect_failure=True)
    dev[0].request("REMOVE_NETWORK all")

    hapd.disable()
    hapd.set("check_crl_strict", "0")
    hapd.enable()

    # check_crl_strict=0 --> accept
    eap_connect(dev[0], hapd, "TLS", "tls user", ca_cert="auth_serv/ca.pem",
                client_cert="auth_serv/user.pem",
                private_key="auth_serv/user.key")
    dev[0].request("REMOVE_NETWORK all")

def test_ap_wpa2_eap_tls_crl_reload(dev, apdev, params):
    """EAP-TLS and server reloading CRL from ca_cert"""
    ca_cert = os.path.join(params['logdir'],
                           "ap_wpa2_eap_tls_crl_reload.ca_cert")
    with open('auth_serv/ca.pem', 'r') as f:
        only_cert = f.read()
    with open('auth_serv/ca-and-crl.pem', 'r') as f:
        cert_and_crl = f.read()
    with open(ca_cert, 'w') as f:
        f.write(only_cert)
    params = int_eap_server_params()
    params['ca_cert'] = ca_cert
    params['check_crl'] = '1'
    params['crl_reload_interval'] = '1'
    hapd = hostapd.add_ap(apdev[0], params)

    # check_crl=1 and no CRL available --> reject connection
    eap_connect(dev[0], hapd, "TLS", "tls user", ca_cert="auth_serv/ca.pem",
                client_cert="auth_serv/user.pem",
                private_key="auth_serv/user.key", expect_failure=True)
    dev[0].request("REMOVE_NETWORK all")
    dev[0].dump_monitor()

    with open(ca_cert, 'w') as f:
        f.write(cert_and_crl)
    time.sleep(1)

    # check_crl=1 and valid CRL --> accept
    eap_connect(dev[0], hapd, "TLS", "tls user", ca_cert="auth_serv/ca.pem",
                client_cert="auth_serv/user.pem",
                private_key="auth_serv/user.key")
    dev[0].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected()

def test_ap_wpa2_eap_tls_check_cert_subject(dev, apdev):
    """EAP-TLS and server checking client subject name"""
    params = int_eap_server_params()
    params['check_cert_subject'] = 'C=FI/O=w1.fi/CN=Test User'
    hapd = hostapd.add_ap(apdev[0], params)
    check_check_cert_subject_support(hapd)

    eap_connect(dev[0], hapd, "TLS", "tls user", ca_cert="auth_serv/ca.pem",
                client_cert="auth_serv/user.pem",
                private_key="auth_serv/user.key")

def test_ap_wpa2_eap_tls_check_cert_subject_neg(dev, apdev):
    """EAP-TLS and server checking client subject name (negative)"""
    params = int_eap_server_params()
    params['check_cert_subject'] = 'C=FI/O=example'
    hapd = hostapd.add_ap(apdev[0], params)
    check_check_cert_subject_support(hapd)

    eap_connect(dev[0], hapd, "TLS", "tls user", ca_cert="auth_serv/ca.pem",
                client_cert="auth_serv/user.pem",
                private_key="auth_serv/user.key", expect_failure=True)

def test_ap_wpa2_eap_tls_oom(dev, apdev):
    """EAP-TLS and OOM"""
    check_subject_match_support(dev[0])
    check_altsubject_match_support(dev[0])
    check_domain_match(dev[0])
    check_domain_match_full(dev[0])

    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hostapd.add_ap(apdev[0], params)

    tests = [(1, "tls_connection_set_subject_match"),
             (2, "tls_connection_set_subject_match"),
             (3, "tls_connection_set_subject_match"),
             (4, "tls_connection_set_subject_match")]
    for count, func in tests:
        with alloc_fail(dev[0], count, func):
            dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TLS",
                           identity="tls user", ca_cert="auth_serv/ca.pem",
                           client_cert="auth_serv/user.pem",
                           private_key="auth_serv/user.key",
                           subject_match="/C=FI/O=w1.fi/CN=server.w1.fi",
                           altsubject_match="EMAIL:noone@example.com;DNS:server.w1.fi;URI:http://example.com/",
                           domain_suffix_match="server.w1.fi",
                           domain_match="server.w1.fi",
                           wait_connect=False, scan_freq="2412")
            # TLS parameter configuration error results in CTRL-REQ-PASSPHRASE
            ev = dev[0].wait_event(["CTRL-REQ-PASSPHRASE"], timeout=5)
            if ev is None:
                raise Exception("No passphrase request")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

def test_ap_wpa2_eap_tls_macacl(dev, apdev):
    """WPA2-Enterprise connection using MAC ACL"""
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    params["macaddr_acl"] = "2"
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[1], hapd, "TLS", "tls user", ca_cert="auth_serv/ca.pem",
                client_cert="auth_serv/user.pem",
                private_key="auth_serv/user.key")

def test_ap_wpa2_eap_oom(dev, apdev):
    """EAP server and OOM"""
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].scan_for_bss(apdev[0]['bssid'], freq=2412)

    with alloc_fail(hapd, 1, "eapol_auth_alloc"):
        # The first attempt fails, but STA will send EAPOL-Start to retry and
        # that succeeds.
        dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TLS",
                       identity="tls user", ca_cert="auth_serv/ca.pem",
                       client_cert="auth_serv/user.pem",
                       private_key="auth_serv/user.key",
                       scan_freq="2412")

def check_tls_ver(dev, hapd, phase1, expected):
    eap_connect(dev, hapd, "TLS", "tls user", ca_cert="auth_serv/ca.pem",
                client_cert="auth_serv/user.pem",
                private_key="auth_serv/user.key",
                phase1=phase1)
    ver = dev.get_status_field("eap_tls_version")
    if ver != expected:
        raise Exception("Unexpected TLS version (expected %s): %s" % (expected, ver))
    dev.request("REMOVE_NETWORK all")
    dev.wait_disconnected()
    dev.dump_monitor()

def test_ap_wpa2_eap_tls_versions(dev, apdev):
    """EAP-TLS and TLS version configuration"""
    params = {"ssid": "test-wpa2-eap",
              "wpa": "2",
              "wpa_key_mgmt": "WPA-EAP",
              "rsn_pairwise": "CCMP",
              "ieee8021x": "1",
              "eap_server": "1",
              "tls_flags": "[ENABLE-TLSv1.0][ENABLE-TLSv1.1][ENABLE-TLSv1.2][ENABLE-TLSv1.3]",
              "eap_user_file": "auth_serv/eap_user.conf",
              "ca_cert": "auth_serv/ca.pem",
              "server_cert": "auth_serv/server.pem",
              "private_key": "auth_serv/server.key"}
    hapd = hostapd.add_ap(apdev[0], params)

    tls = dev[0].request("GET tls_library")
    if tls.startswith("OpenSSL"):
        if "build=OpenSSL 1.0.1" not in tls and "run=OpenSSL 1.0.1" not in tls:
            check_tls_ver(dev[0], hapd,
                          "tls_disable_tlsv1_0=1 tls_disable_tlsv1_1=1",
                          "TLSv1.2")
    if tls.startswith("wolfSSL"):
        if ("build=3.10.0" in tls and "run=3.10.0" in tls) or \
           ("build=3.13.0" in tls and "run=3.13.0" in tls):
            check_tls_ver(dev[0], hapd,
                          "tls_disable_tlsv1_0=1 tls_disable_tlsv1_1=1",
                          "TLSv1.2")
    elif tls.startswith("internal"):
        check_tls_ver(dev[0], hapd,
                      "tls_disable_tlsv1_0=1 tls_disable_tlsv1_1=1", "TLSv1.2")
    check_tls_ver(dev[1], hapd,
                  "tls_disable_tlsv1_0=1 tls_disable_tlsv1_1=0 tls_disable_tlsv1_2=1", "TLSv1.1")
    check_tls_ver(dev[2], hapd,
                  "tls_disable_tlsv1_0=0 tls_disable_tlsv1_1=1 tls_disable_tlsv1_2=1", "TLSv1")
    if "run=OpenSSL 1.1.1" in tls:
        check_tls_ver(dev[0], hapd,
                      "tls_disable_tlsv1_0=1 tls_disable_tlsv1_1=1 tls_disable_tlsv1_2=1 tls_disable_tlsv1_3=0", "TLSv1.3")

def test_ap_wpa2_eap_tls_versions_server(dev, apdev):
    """EAP-TLS and TLS version configuration on server side"""
    params = {"ssid": "test-wpa2-eap",
              "wpa": "2",
              "wpa_key_mgmt": "WPA-EAP",
              "rsn_pairwise": "CCMP",
              "ieee8021x": "1",
              "eap_server": "1",
              "eap_user_file": "auth_serv/eap_user.conf",
              "ca_cert": "auth_serv/ca.pem",
              "server_cert": "auth_serv/server.pem",
              "private_key": "auth_serv/server.key"}
    hapd = hostapd.add_ap(apdev[0], params)

    tests = [("TLSv1", "[ENABLE-TLSv1.0][DISABLE-TLSv1.1][DISABLE-TLSv1.2][DISABLE-TLSv1.3]"),
             ("TLSv1.1", "[ENABLE-TLSv1.0][ENABLE-TLSv1.1][DISABLE-TLSv1.2][DISABLE-TLSv1.3]"),
             ("TLSv1.2", "[ENABLE-TLSv1.0][ENABLE-TLSv1.1][ENABLE-TLSv1.2][DISABLE-TLSv1.3]")]
    for exp, flags in tests:
        hapd.disable()
        hapd.set("tls_flags", flags)
        hapd.enable()
        check_tls_ver(dev[0], hapd, "tls_disable_tlsv1_0=0 tls_disable_tlsv1_1=0 tls_disable_tlsv1_2=0 tls_disable_tlsv1_3=0", exp)

def test_ap_wpa2_eap_tls_13(dev, apdev):
    """EAP-TLS and TLS 1.3"""
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)

    tls = dev[0].request("GET tls_library")
    if "run=OpenSSL 1.1.1" not in tls:
        raise HwsimSkip("TLS v1.3 not supported")
    id = eap_connect(dev[0], hapd, "TLS", "tls user",
                     ca_cert="auth_serv/ca.pem",
                     client_cert="auth_serv/user.pem",
                     private_key="auth_serv/user.key",
                     phase1="tls_disable_tlsv1_0=1 tls_disable_tlsv1_1=1 tls_disable_tlsv1_2=1 tls_disable_tlsv1_3=0")
    ver = dev[0].get_status_field("eap_tls_version")
    if ver != "TLSv1.3":
        raise Exception("Unexpected TLS version")

    eap_reauth(dev[0], "TLS")
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()
    dev[0].request("PMKSA_FLUSH")
    dev[0].request("RECONNECT")
    dev[0].wait_connected()

def test_ap_wpa2_eap_ttls_13(dev, apdev):
    """EAP-TTLS and TLS 1.3"""
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)

    tls = dev[0].request("GET tls_library")
    if "run=OpenSSL 1.1.1" not in tls:
        raise HwsimSkip("TLS v1.3 not supported")
    id = eap_connect(dev[0], hapd, "TTLS", "pap user",
                     anonymous_identity="ttls", password="password",
                     ca_cert="auth_serv/ca.pem",
                     phase1="tls_disable_tlsv1_0=1 tls_disable_tlsv1_1=1 tls_disable_tlsv1_2=1 tls_disable_tlsv1_3=0",
                     phase2="auth=PAP")
    ver = dev[0].get_status_field("eap_tls_version")
    if ver != "TLSv1.3":
        raise Exception("Unexpected TLS version")

    eap_reauth(dev[0], "TTLS")
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()
    dev[0].request("PMKSA_FLUSH")
    dev[0].request("RECONNECT")
    dev[0].wait_connected()

def test_ap_wpa2_eap_peap_13(dev, apdev):
    """PEAP and TLS 1.3"""
    check_eap_capa(dev[0], "MSCHAPV2")
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)

    tls = dev[0].request("GET tls_library")
    if "run=OpenSSL 1.1.1" not in tls:
        raise HwsimSkip("TLS v1.3 not supported")
    id = eap_connect(dev[0], hapd, "PEAP", "user",
                     anonymous_identity="peap", password="password",
                     ca_cert="auth_serv/ca.pem",
                     phase1="tls_disable_tlsv1_0=1 tls_disable_tlsv1_1=1 tls_disable_tlsv1_2=1 tls_disable_tlsv1_3=0",
                     phase2="auth=MSCHAPV2")
    ver = dev[0].get_status_field("eap_tls_version")
    if ver != "TLSv1.3":
        raise Exception("Unexpected TLS version")

    eap_reauth(dev[0], "PEAP")
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()
    dev[0].request("PMKSA_FLUSH")
    dev[0].request("RECONNECT")
    dev[0].wait_connected()

def test_ap_wpa2_eap_tls_13_ec(dev, apdev):
    """EAP-TLS and TLS 1.3 (EC certificates)"""
    params = {"ssid": "test-wpa2-eap",
              "wpa": "2",
              "wpa_key_mgmt": "WPA-EAP",
              "rsn_pairwise": "CCMP",
              "ieee8021x": "1",
              "eap_server": "1",
              "eap_user_file": "auth_serv/eap_user.conf",
              "ca_cert": "auth_serv/ec-ca.pem",
              "server_cert": "auth_serv/ec-server.pem",
              "private_key": "auth_serv/ec-server.key",
              "tls_flags": "[ENABLE-TLSv1.3]"}
    hapd = hostapd.add_ap(apdev[0], params)
    tls = hapd.request("GET tls_library")
    if "run=OpenSSL 1.1.1" not in tls:
        raise HwsimSkip("TLS v1.3 not supported")

    tls = dev[0].request("GET tls_library")
    if "run=OpenSSL 1.1.1" not in tls:
        raise HwsimSkip("TLS v1.3 not supported")
    id = eap_connect(dev[0], hapd, "TLS", "tls user",
                     ca_cert="auth_serv/ec-ca.pem",
                     client_cert="auth_serv/ec-user.pem",
                     private_key="auth_serv/ec-user.key",
                     phase1="tls_disable_tlsv1_0=1 tls_disable_tlsv1_1=1 tls_disable_tlsv1_2=1 tls_disable_tlsv1_3=0")
    ver = dev[0].get_status_field("eap_tls_version")
    if ver != "TLSv1.3":
        raise Exception("Unexpected TLS version")

def test_ap_wpa2_eap_tls_rsa_and_ec(dev, apdev, params):
    """EAP-TLS and both RSA and EC sertificates certificates"""
    check_ec_support(dev[0])
    ca = os.path.join(params['logdir'], "ap_wpa2_eap_tls_rsa_and_ec.ca.pem")
    with open(ca, "w") as f:
        with open("auth_serv/ca.pem", "r") as f2:
            f.write(f2.read())
        with open("auth_serv/ec-ca.pem", "r") as f2:
            f.write(f2.read())
    params = {"ssid": "test-wpa2-eap",
              "wpa": "2",
              "wpa_key_mgmt": "WPA-EAP",
              "rsn_pairwise": "CCMP",
              "ieee8021x": "1",
              "eap_server": "1",
              "eap_user_file": "auth_serv/eap_user.conf",
              "ca_cert": ca,
              "server_cert": "auth_serv/server.pem",
              "private_key": "auth_serv/server.key",
              "server_cert2": "auth_serv/ec-server.pem",
              "private_key2": "auth_serv/ec-server.key"}
    hapd = hostapd.add_ap(apdev[0], params)

    eap_connect(dev[0], hapd, "TLS", "tls user",
                ca_cert="auth_serv/ec-ca.pem",
                client_cert="auth_serv/ec-user.pem",
                private_key="auth_serv/ec-user.key")
    dev[0].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected()

    # TODO: Make wpa_supplicant automatically filter out cipher suites that
    # would require ECDH/ECDSA keys when those are not configured in the
    # selected client certificate. And for no-client-cert case, deprioritize
    # those cipher suites based on configured ca_cert value so that the most
    # likely to work cipher suites are selected by the server. Only do these
    # when an explicit openssl_ciphers parameter is not set.
    eap_connect(dev[1], hapd, "TLS", "tls user",
                openssl_ciphers="DEFAULT:-aECDH:-aECDSA",
                ca_cert="auth_serv/ca.pem",
                client_cert="auth_serv/user.pem",
                private_key="auth_serv/user.key")
    dev[1].request("REMOVE_NETWORK all")
    dev[1].wait_disconnected()

def test_ap_wpa2_eap_tls_ec_and_rsa(dev, apdev, params):
    """EAP-TLS and both EC and RSA sertificates certificates"""
    check_ec_support(dev[0])
    ca = os.path.join(params['logdir'], "ap_wpa2_eap_tls_ec_and_rsa.ca.pem")
    with open(ca, "w") as f:
        with open("auth_serv/ca.pem", "r") as f2:
            f.write(f2.read())
        with open("auth_serv/ec-ca.pem", "r") as f2:
            f.write(f2.read())
    params = {"ssid": "test-wpa2-eap",
              "wpa": "2",
              "wpa_key_mgmt": "WPA-EAP",
              "rsn_pairwise": "CCMP",
              "ieee8021x": "1",
              "eap_server": "1",
              "eap_user_file": "auth_serv/eap_user.conf",
              "ca_cert": ca,
              "private_key2": "auth_serv/server-extra.pkcs12",
              "private_key_passwd2": "whatever",
              "server_cert": "auth_serv/ec-server.pem",
              "private_key": "auth_serv/ec-server.key"}
    hapd = hostapd.add_ap(apdev[0], params)

    eap_connect(dev[0], hapd, "TLS", "tls user",
                ca_cert="auth_serv/ec-ca.pem",
                client_cert="auth_serv/ec-user.pem",
                private_key="auth_serv/ec-user.key")
    dev[0].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected()

    # TODO: Make wpa_supplicant automatically filter out cipher suites that
    # would require ECDH/ECDSA keys when those are not configured in the
    # selected client certificate. And for no-client-cert case, deprioritize
    # those cipher suites based on configured ca_cert value so that the most
    # likely to work cipher suites are selected by the server. Only do these
    # when an explicit openssl_ciphers parameter is not set.
    eap_connect(dev[1], hapd, "TLS", "tls user",
                openssl_ciphers="DEFAULT:-aECDH:-aECDSA",
                ca_cert="auth_serv/ca.pem",
                client_cert="auth_serv/user.pem",
                private_key="auth_serv/user.key")
    dev[1].request("REMOVE_NETWORK all")
    dev[1].wait_disconnected()

def test_rsn_ie_proto_eap_sta(dev, apdev):
    """RSN element protocol testing for EAP cases on STA side"""
    bssid = apdev[0]['bssid']
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    # This is the RSN element used normally by hostapd
    params['own_ie_override'] = '30140100000fac040100000fac040100000fac010c00'
    hapd = hostapd.add_ap(apdev[0], params)
    id = dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="GPSK",
                        identity="gpsk user",
                        password="abcdefghijklmnop0123456789abcdef",
                        scan_freq="2412")

    tests = [('No RSN Capabilities field',
              '30120100000fac040100000fac040100000fac01'),
             ('No AKM Suite fields',
              '300c0100000fac040100000fac04'),
             ('No Pairwise Cipher Suite fields',
              '30060100000fac04'),
             ('No Group Data Cipher Suite field',
              '30020100')]
    for txt, ie in tests:
        dev[0].request("DISCONNECT")
        dev[0].wait_disconnected()
        logger.info(txt)
        hapd.disable()
        hapd.set('own_ie_override', ie)
        hapd.enable()
        dev[0].request("BSS_FLUSH 0")
        dev[0].scan_for_bss(bssid, 2412, force_scan=True, only_new=True)
        dev[0].select_network(id, freq=2412)
        dev[0].wait_connected()

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()
    dev[0].flush_scan_cache()

def check_tls_session_resumption_capa(dev, hapd):
    tls = hapd.request("GET tls_library")
    if not tls.startswith("OpenSSL"):
        raise HwsimSkip("hostapd TLS library is not OpenSSL or wolfSSL: " + tls)

    tls = dev.request("GET tls_library")
    if not tls.startswith("OpenSSL"):
        raise HwsimSkip("Session resumption not supported with this TLS library: " + tls)

def test_eap_ttls_pap_session_resumption(dev, apdev):
    """EAP-TTLS/PAP session resumption"""
    params = int_eap_server_params()
    params['tls_session_lifetime'] = '60'
    hapd = hostapd.add_ap(apdev[0], params)
    check_tls_session_resumption_capa(dev[0], hapd)
    eap_connect(dev[0], hapd, "TTLS", "pap user",
                anonymous_identity="ttls", password="password",
                ca_cert="auth_serv/ca.pem", eap_workaround='0',
                phase2="auth=PAP")
    if dev[0].get_status_field("tls_session_reused") != '0':
        raise Exception("Unexpected session resumption on the first connection")

    dev[0].request("REAUTHENTICATE")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-SUCCESS"], timeout=10)
    if ev is None:
        raise Exception("EAP success timed out")
    ev = dev[0].wait_event(["WPA: Key negotiation completed"], timeout=10)
    if ev is None:
        raise Exception("Key handshake with the AP timed out")
    if dev[0].get_status_field("tls_session_reused") != '1':
        raise Exception("Session resumption not used on the second connection")
    hwsim_utils.test_connectivity(dev[0], hapd)

def test_eap_ttls_chap_session_resumption(dev, apdev):
    """EAP-TTLS/CHAP session resumption"""
    params = int_eap_server_params()
    params['tls_session_lifetime'] = '60'
    hapd = hostapd.add_ap(apdev[0], params)
    check_tls_session_resumption_capa(dev[0], hapd)
    eap_connect(dev[0], hapd, "TTLS", "chap user",
                anonymous_identity="ttls", password="password",
                ca_cert="auth_serv/ca.der", phase2="auth=CHAP")
    if dev[0].get_status_field("tls_session_reused") != '0':
        raise Exception("Unexpected session resumption on the first connection")

    dev[0].request("REAUTHENTICATE")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-SUCCESS"], timeout=10)
    if ev is None:
        raise Exception("EAP success timed out")
    ev = dev[0].wait_event(["WPA: Key negotiation completed"], timeout=10)
    if ev is None:
        raise Exception("Key handshake with the AP timed out")
    if dev[0].get_status_field("tls_session_reused") != '1':
        raise Exception("Session resumption not used on the second connection")

def test_eap_ttls_mschap_session_resumption(dev, apdev):
    """EAP-TTLS/MSCHAP session resumption"""
    check_domain_suffix_match(dev[0])
    params = int_eap_server_params()
    params['tls_session_lifetime'] = '60'
    hapd = hostapd.add_ap(apdev[0], params)
    check_tls_session_resumption_capa(dev[0], hapd)
    eap_connect(dev[0], hapd, "TTLS", "mschap user",
                anonymous_identity="ttls", password="password",
                ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAP",
                domain_suffix_match="server.w1.fi")
    if dev[0].get_status_field("tls_session_reused") != '0':
        raise Exception("Unexpected session resumption on the first connection")

    dev[0].request("REAUTHENTICATE")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-SUCCESS"], timeout=10)
    if ev is None:
        raise Exception("EAP success timed out")
    ev = dev[0].wait_event(["WPA: Key negotiation completed"], timeout=10)
    if ev is None:
        raise Exception("Key handshake with the AP timed out")
    if dev[0].get_status_field("tls_session_reused") != '1':
        raise Exception("Session resumption not used on the second connection")

def test_eap_ttls_mschapv2_session_resumption(dev, apdev):
    """EAP-TTLS/MSCHAPv2 session resumption"""
    check_domain_suffix_match(dev[0])
    check_eap_capa(dev[0], "MSCHAPV2")
    params = int_eap_server_params()
    params['tls_session_lifetime'] = '60'
    hapd = hostapd.add_ap(apdev[0], params)
    check_tls_session_resumption_capa(dev[0], hapd)
    eap_connect(dev[0], hapd, "TTLS", "DOMAIN\mschapv2 user",
                anonymous_identity="ttls", password="password",
                ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2",
                domain_suffix_match="server.w1.fi")
    if dev[0].get_status_field("tls_session_reused") != '0':
        raise Exception("Unexpected session resumption on the first connection")

    dev[0].request("REAUTHENTICATE")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-SUCCESS"], timeout=10)
    if ev is None:
        raise Exception("EAP success timed out")
    ev = dev[0].wait_event(["WPA: Key negotiation completed"], timeout=10)
    if ev is None:
        raise Exception("Key handshake with the AP timed out")
    if dev[0].get_status_field("tls_session_reused") != '1':
        raise Exception("Session resumption not used on the second connection")

def test_eap_ttls_eap_gtc_session_resumption(dev, apdev):
    """EAP-TTLS/EAP-GTC session resumption"""
    params = int_eap_server_params()
    params['tls_session_lifetime'] = '60'
    hapd = hostapd.add_ap(apdev[0], params)
    check_tls_session_resumption_capa(dev[0], hapd)
    eap_connect(dev[0], hapd, "TTLS", "user",
                anonymous_identity="ttls", password="password",
                ca_cert="auth_serv/ca.pem", phase2="autheap=GTC")
    if dev[0].get_status_field("tls_session_reused") != '0':
        raise Exception("Unexpected session resumption on the first connection")

    dev[0].request("REAUTHENTICATE")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-SUCCESS"], timeout=10)
    if ev is None:
        raise Exception("EAP success timed out")
    ev = dev[0].wait_event(["WPA: Key negotiation completed"], timeout=10)
    if ev is None:
        raise Exception("Key handshake with the AP timed out")
    if dev[0].get_status_field("tls_session_reused") != '1':
        raise Exception("Session resumption not used on the second connection")

def test_eap_ttls_no_session_resumption(dev, apdev):
    """EAP-TTLS session resumption disabled on server"""
    params = int_eap_server_params()
    params['tls_session_lifetime'] = '0'
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "TTLS", "pap user",
                anonymous_identity="ttls", password="password",
                ca_cert="auth_serv/ca.pem", eap_workaround='0',
                phase2="auth=PAP")
    if dev[0].get_status_field("tls_session_reused") != '0':
        raise Exception("Unexpected session resumption on the first connection")

    dev[0].request("REAUTHENTICATE")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-SUCCESS"], timeout=10)
    if ev is None:
        raise Exception("EAP success timed out")
    ev = dev[0].wait_event(["WPA: Key negotiation completed"], timeout=10)
    if ev is None:
        raise Exception("Key handshake with the AP timed out")
    if dev[0].get_status_field("tls_session_reused") != '0':
        raise Exception("Unexpected session resumption on the second connection")

def test_eap_peap_session_resumption(dev, apdev):
    """EAP-PEAP session resumption"""
    check_eap_capa(dev[0], "MSCHAPV2")
    params = int_eap_server_params()
    params['tls_session_lifetime'] = '60'
    hapd = hostapd.add_ap(apdev[0], params)
    check_tls_session_resumption_capa(dev[0], hapd)
    eap_connect(dev[0], hapd, "PEAP", "user",
                anonymous_identity="peap", password="password",
                ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2")
    if dev[0].get_status_field("tls_session_reused") != '0':
        raise Exception("Unexpected session resumption on the first connection")

    dev[0].request("REAUTHENTICATE")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-SUCCESS"], timeout=10)
    if ev is None:
        raise Exception("EAP success timed out")
    ev = dev[0].wait_event(["WPA: Key negotiation completed"], timeout=10)
    if ev is None:
        raise Exception("Key handshake with the AP timed out")
    if dev[0].get_status_field("tls_session_reused") != '1':
        raise Exception("Session resumption not used on the second connection")

def test_eap_peap_session_resumption_crypto_binding(dev, apdev):
    """EAP-PEAP session resumption with crypto binding"""
    params = int_eap_server_params()
    params['tls_session_lifetime'] = '60'
    hapd = hostapd.add_ap(apdev[0], params)
    check_tls_session_resumption_capa(dev[0], hapd)
    eap_connect(dev[0], hapd, "PEAP", "user",
                anonymous_identity="peap", password="password",
                phase1="peapver=0 crypto_binding=2",
                ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2")
    if dev[0].get_status_field("tls_session_reused") != '0':
        raise Exception("Unexpected session resumption on the first connection")

    dev[0].request("REAUTHENTICATE")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-SUCCESS"], timeout=10)
    if ev is None:
        raise Exception("EAP success timed out")
    ev = dev[0].wait_event(["WPA: Key negotiation completed"], timeout=10)
    if ev is None:
        raise Exception("Key handshake with the AP timed out")
    if dev[0].get_status_field("tls_session_reused") != '1':
        raise Exception("Session resumption not used on the second connection")

def test_eap_peap_no_session_resumption(dev, apdev):
    """EAP-PEAP session resumption disabled on server"""
    params = int_eap_server_params()
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "PEAP", "user",
                anonymous_identity="peap", password="password",
                ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2")
    if dev[0].get_status_field("tls_session_reused") != '0':
        raise Exception("Unexpected session resumption on the first connection")

    dev[0].request("REAUTHENTICATE")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-SUCCESS"], timeout=10)
    if ev is None:
        raise Exception("EAP success timed out")
    ev = dev[0].wait_event(["WPA: Key negotiation completed"], timeout=10)
    if ev is None:
        raise Exception("Key handshake with the AP timed out")
    if dev[0].get_status_field("tls_session_reused") != '0':
        raise Exception("Unexpected session resumption on the second connection")

def test_eap_tls_session_resumption(dev, apdev):
    """EAP-TLS session resumption"""
    params = int_eap_server_params()
    params['tls_session_lifetime'] = '60'
    hapd = hostapd.add_ap(apdev[0], params)
    check_tls_session_resumption_capa(dev[0], hapd)
    eap_connect(dev[0], hapd, "TLS", "tls user", ca_cert="auth_serv/ca.pem",
                client_cert="auth_serv/user.pem",
                private_key="auth_serv/user.key")
    if dev[0].get_status_field("tls_session_reused") != '0':
        raise Exception("Unexpected session resumption on the first connection")

    dev[0].request("REAUTHENTICATE")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-SUCCESS"], timeout=10)
    if ev is None:
        raise Exception("EAP success timed out")
    ev = dev[0].wait_event(["WPA: Key negotiation completed"], timeout=10)
    if ev is None:
        raise Exception("Key handshake with the AP timed out")
    if dev[0].get_status_field("tls_session_reused") != '1':
        raise Exception("Session resumption not used on the second connection")

    dev[0].request("REAUTHENTICATE")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-SUCCESS"], timeout=10)
    if ev is None:
        raise Exception("EAP success timed out")
    ev = dev[0].wait_event(["WPA: Key negotiation completed"], timeout=10)
    if ev is None:
        raise Exception("Key handshake with the AP timed out")
    if dev[0].get_status_field("tls_session_reused") != '1':
        raise Exception("Session resumption not used on the third connection")

def test_eap_tls_session_resumption_expiration(dev, apdev):
    """EAP-TLS session resumption"""
    params = int_eap_server_params()
    params['tls_session_lifetime'] = '1'
    hapd = hostapd.add_ap(apdev[0], params)
    check_tls_session_resumption_capa(dev[0], hapd)
    eap_connect(dev[0], hapd, "TLS", "tls user", ca_cert="auth_serv/ca.pem",
                client_cert="auth_serv/user.pem",
                private_key="auth_serv/user.key")
    if dev[0].get_status_field("tls_session_reused") != '0':
        raise Exception("Unexpected session resumption on the first connection")

    # Allow multiple attempts since OpenSSL may not expire the cached entry
    # immediately.
    for i in range(10):
        time.sleep(1.2)

        dev[0].request("REAUTHENTICATE")
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-SUCCESS"], timeout=10)
        if ev is None:
            raise Exception("EAP success timed out")
        ev = dev[0].wait_event(["WPA: Key negotiation completed"], timeout=10)
        if ev is None:
            raise Exception("Key handshake with the AP timed out")
        if dev[0].get_status_field("tls_session_reused") == '0':
            break
    if dev[0].get_status_field("tls_session_reused") != '0':
        raise Exception("Session resumption used after lifetime expiration")

def test_eap_tls_no_session_resumption(dev, apdev):
    """EAP-TLS session resumption disabled on server"""
    params = int_eap_server_params()
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "TLS", "tls user", ca_cert="auth_serv/ca.pem",
                client_cert="auth_serv/user.pem",
                private_key="auth_serv/user.key")
    if dev[0].get_status_field("tls_session_reused") != '0':
        raise Exception("Unexpected session resumption on the first connection")

    dev[0].request("REAUTHENTICATE")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-SUCCESS"], timeout=10)
    if ev is None:
        raise Exception("EAP success timed out")
    ev = dev[0].wait_event(["WPA: Key negotiation completed"], timeout=10)
    if ev is None:
        raise Exception("Key handshake with the AP timed out")
    if dev[0].get_status_field("tls_session_reused") != '0':
        raise Exception("Unexpected session resumption on the second connection")

def test_eap_tls_session_resumption_radius(dev, apdev):
    """EAP-TLS session resumption (RADIUS)"""
    params = {"ssid": "as", "beacon_int": "2000",
              "radius_server_clients": "auth_serv/radius_clients.conf",
              "radius_server_auth_port": '18128',
              "eap_server": "1",
              "eap_user_file": "auth_serv/eap_user.conf",
              "ca_cert": "auth_serv/ca.pem",
              "server_cert": "auth_serv/server.pem",
              "private_key": "auth_serv/server.key",
              "tls_session_lifetime": "60"}
    authsrv = hostapd.add_ap(apdev[1], params)
    check_tls_session_resumption_capa(dev[0], authsrv)

    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    params['auth_server_port'] = "18128"
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "TLS", "tls user", ca_cert="auth_serv/ca.pem",
                client_cert="auth_serv/user.pem",
                private_key="auth_serv/user.key")
    if dev[0].get_status_field("tls_session_reused") != '0':
        raise Exception("Unexpected session resumption on the first connection")

    dev[0].request("REAUTHENTICATE")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-SUCCESS"], timeout=10)
    if ev is None:
        raise Exception("EAP success timed out")
    ev = dev[0].wait_event(["WPA: Key negotiation completed"], timeout=10)
    if ev is None:
        raise Exception("Key handshake with the AP timed out")
    if dev[0].get_status_field("tls_session_reused") != '1':
        raise Exception("Session resumption not used on the second connection")

def test_eap_tls_no_session_resumption_radius(dev, apdev):
    """EAP-TLS session resumption disabled (RADIUS)"""
    params = {"ssid": "as", "beacon_int": "2000",
              "radius_server_clients": "auth_serv/radius_clients.conf",
              "radius_server_auth_port": '18128',
              "eap_server": "1",
              "eap_user_file": "auth_serv/eap_user.conf",
              "ca_cert": "auth_serv/ca.pem",
              "server_cert": "auth_serv/server.pem",
              "private_key": "auth_serv/server.key",
              "tls_session_lifetime": "0"}
    hostapd.add_ap(apdev[1], params)

    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    params['auth_server_port'] = "18128"
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "TLS", "tls user", ca_cert="auth_serv/ca.pem",
                client_cert="auth_serv/user.pem",
                private_key="auth_serv/user.key")
    if dev[0].get_status_field("tls_session_reused") != '0':
        raise Exception("Unexpected session resumption on the first connection")

    dev[0].request("REAUTHENTICATE")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-SUCCESS"], timeout=10)
    if ev is None:
        raise Exception("EAP success timed out")
    ev = dev[0].wait_event(["WPA: Key negotiation completed"], timeout=10)
    if ev is None:
        raise Exception("Key handshake with the AP timed out")
    if dev[0].get_status_field("tls_session_reused") != '0':
        raise Exception("Unexpected session resumption on the second connection")

def test_eap_mschapv2_errors(dev, apdev):
    """EAP-MSCHAPv2 error cases"""
    check_eap_capa(dev[0], "MSCHAPV2")
    check_eap_capa(dev[0], "FAST")

    params = hostapd.wpa2_eap_params(ssid="test-wpa-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].connect("test-wpa-eap", key_mgmt="WPA-EAP", eap="MSCHAPV2",
                   identity="phase1-user", password="password",
                   scan_freq="2412")
    dev[0].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected()

    tests = [(1, "hash_nt_password_hash;mschapv2_derive_response"),
             (1, "nt_password_hash;mschapv2_derive_response"),
             (1, "nt_password_hash;=mschapv2_derive_response"),
             (1, "generate_nt_response;mschapv2_derive_response"),
             (1, "generate_authenticator_response;mschapv2_derive_response"),
             (1, "nt_password_hash;=mschapv2_derive_response"),
             (1, "get_master_key;mschapv2_derive_response"),
             (1, "os_get_random;eap_mschapv2_challenge_reply")]
    for count, func in tests:
        with fail_test(dev[0], count, func):
            dev[0].connect("test-wpa-eap", key_mgmt="WPA-EAP", eap="MSCHAPV2",
                           identity="phase1-user", password="password",
                           wait_connect=False, scan_freq="2412")
            wait_fail_trigger(dev[0], "GET_FAIL")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

    tests = [(1, "hash_nt_password_hash;mschapv2_derive_response"),
             (1, "hash_nt_password_hash;=mschapv2_derive_response"),
             (1, "generate_nt_response_pwhash;mschapv2_derive_response"),
             (1, "generate_authenticator_response_pwhash;mschapv2_derive_response")]
    for count, func in tests:
        with fail_test(dev[0], count, func):
            dev[0].connect("test-wpa-eap", key_mgmt="WPA-EAP", eap="MSCHAPV2",
                           identity="phase1-user",
                           password_hex="hash:8846f7eaee8fb117ad06bdd830b7586c",
                           wait_connect=False, scan_freq="2412")
            wait_fail_trigger(dev[0], "GET_FAIL")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

    tests = [(1, "eap_mschapv2_init"),
             (1, "eap_msg_alloc;eap_mschapv2_challenge_reply"),
             (1, "eap_msg_alloc;eap_mschapv2_success"),
             (1, "eap_mschapv2_getKey")]
    for count, func in tests:
        with alloc_fail(dev[0], count, func):
            dev[0].connect("test-wpa-eap", key_mgmt="WPA-EAP", eap="MSCHAPV2",
                           identity="phase1-user", password="password",
                           wait_connect=False, scan_freq="2412")
            wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

    tests = [(1, "eap_msg_alloc;eap_mschapv2_failure")]
    for count, func in tests:
        with alloc_fail(dev[0], count, func):
            dev[0].connect("test-wpa-eap", key_mgmt="WPA-EAP", eap="MSCHAPV2",
                           identity="phase1-user", password="wrong password",
                           wait_connect=False, scan_freq="2412")
            wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

    tests = [(2, "eap_mschapv2_init"),
             (3, "eap_mschapv2_init")]
    for count, func in tests:
        with alloc_fail(dev[0], count, func):
            dev[0].connect("test-wpa-eap", key_mgmt="WPA-EAP", eap="FAST",
                           anonymous_identity="FAST", identity="user",
                           password="password",
                           ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2",
                           phase1="fast_provisioning=1",
                           pac_file="blob://fast_pac",
                           wait_connect=False, scan_freq="2412")
            wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

def test_eap_gpsk_errors(dev, apdev):
    """EAP-GPSK error cases"""
    params = hostapd.wpa2_eap_params(ssid="test-wpa-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].connect("test-wpa-eap", key_mgmt="WPA-EAP", eap="GPSK",
                   identity="gpsk user",
                   password="abcdefghijklmnop0123456789abcdef",
                   scan_freq="2412")
    dev[0].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected()

    tests = [(1, "os_get_random;eap_gpsk_send_gpsk_2", None),
             (1, "eap_gpsk_derive_session_id;eap_gpsk_send_gpsk_2",
              "cipher=1"),
             (1, "eap_gpsk_derive_session_id;eap_gpsk_send_gpsk_2",
              "cipher=2"),
             (1, "eap_gpsk_derive_keys_helper", None),
             (2, "eap_gpsk_derive_keys_helper", None),
             (3, "eap_gpsk_derive_keys_helper", None),
             (1, "eap_gpsk_compute_mic_aes;eap_gpsk_compute_mic;eap_gpsk_send_gpsk_2",
              "cipher=1"),
             (1, "hmac_sha256;eap_gpsk_compute_mic;eap_gpsk_send_gpsk_2",
              "cipher=2"),
             (1, "eap_gpsk_compute_mic;eap_gpsk_validate_gpsk_3_mic", None),
             (1, "eap_gpsk_compute_mic;eap_gpsk_send_gpsk_4", None),
             (1, "eap_gpsk_derive_mid_helper", None)]
    for count, func, phase1 in tests:
        with fail_test(dev[0], count, func):
            dev[0].connect("test-wpa-eap", key_mgmt="WPA-EAP", eap="GPSK",
                           identity="gpsk user",
                           password="abcdefghijklmnop0123456789abcdef",
                           phase1=phase1,
                           wait_connect=False, scan_freq="2412")
            wait_fail_trigger(dev[0], "GET_FAIL")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

    tests = [(1, "eap_gpsk_init"),
             (2, "eap_gpsk_init"),
             (3, "eap_gpsk_init"),
             (1, "eap_gpsk_process_id_server"),
             (1, "eap_msg_alloc;eap_gpsk_send_gpsk_2"),
             (1, "eap_gpsk_derive_session_id;eap_gpsk_send_gpsk_2"),
             (1, "eap_gpsk_derive_mid_helper;eap_gpsk_derive_session_id;eap_gpsk_send_gpsk_2"),
             (1, "eap_gpsk_derive_keys"),
             (1, "eap_gpsk_derive_keys_helper"),
             (1, "eap_msg_alloc;eap_gpsk_send_gpsk_4"),
             (1, "eap_gpsk_getKey"),
             (1, "eap_gpsk_get_emsk"),
             (1, "eap_gpsk_get_session_id")]
    for count, func in tests:
        with alloc_fail(dev[0], count, func):
            dev[0].request("ERP_FLUSH")
            dev[0].connect("test-wpa-eap", key_mgmt="WPA-EAP", eap="GPSK",
                           identity="gpsk user@domain", erp="1",
                           password="abcdefghijklmnop0123456789abcdef",
                           wait_connect=False, scan_freq="2412")
            wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

def test_ap_wpa2_eap_sim_db(dev, apdev, params):
    """EAP-SIM DB error cases"""
    sockpath = '/tmp/hlr_auc_gw.sock-test'
    try:
        os.remove(sockpath)
    except:
        pass
    hparams = int_eap_server_params()
    hparams['eap_sim_db'] = 'unix:' + sockpath
    hapd = hostapd.add_ap(apdev[0], hparams)

    # Initial test with hlr_auc_gw socket not available
    id = dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP WPA-EAP-SHA256",
                        eap="SIM", identity="1232010000000000",
                        password="90dca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb82581",
                        scan_freq="2412", wait_connect=False)
    ev = dev[0].wait_event(["EAP-ERROR-CODE"], timeout=10)
    if ev is None:
        raise Exception("EAP method specific error code not reported")
    if int(ev.split()[1]) != 16384:
        raise Exception("Unexpected EAP method specific error code: " + ev)
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"], timeout=10)
    if ev is None:
        raise Exception("EAP-Failure not reported")
    dev[0].wait_disconnected()
    dev[0].request("DISCONNECT")

    # Test with invalid responses and response timeout

    class test_handler(SocketServer.DatagramRequestHandler):
        def handle(self):
            data = self.request[0].decode().strip()
            socket = self.request[1]
            logger.debug("Received hlr_auc_gw request: " + data)
            # EAP-SIM DB: Failed to parse response string
            socket.sendto(b"FOO", self.client_address)
            # EAP-SIM DB: Failed to parse response string
            socket.sendto(b"FOO 1", self.client_address)
            # EAP-SIM DB: Unknown external response
            socket.sendto(b"FOO 1 2", self.client_address)
            logger.info("No proper response - wait for pending eap_sim_db request timeout")

    server = SocketServer.UnixDatagramServer(sockpath, test_handler)
    server.timeout = 1

    dev[0].select_network(id)
    server.handle_request()
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"], timeout=10)
    if ev is None:
        raise Exception("EAP-Failure not reported")
    dev[0].wait_disconnected()
    dev[0].request("DISCONNECT")

    # Test with a valid response

    class test_handler2(SocketServer.DatagramRequestHandler):
        def handle(self):
            data = self.request[0].decode().strip()
            socket = self.request[1]
            logger.debug("Received hlr_auc_gw request: " + data)
            fname = os.path.join(params['logdir'],
                                 'hlr_auc_gw.milenage_db')
            cmd = subprocess.Popen(['../../hostapd/hlr_auc_gw',
                                    '-m', fname, data],
                                   stdout=subprocess.PIPE)
            res = cmd.stdout.read().decode().strip()
            cmd.stdout.close()
            logger.debug("hlr_auc_gw response: " + res)
            socket.sendto(res.encode(), self.client_address)

    server.RequestHandlerClass = test_handler2

    dev[0].select_network(id)
    server.handle_request()
    dev[0].wait_connected()
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

def test_ap_wpa2_eap_sim_db_sqlite(dev, apdev, params):
    """EAP-SIM DB error cases (SQLite)"""
    sockpath = '/tmp/hlr_auc_gw.sock-test'
    try:
        os.remove(sockpath)
    except:
        pass
    hparams = int_eap_server_params()
    hparams['eap_sim_db'] = 'unix:' + sockpath
    hapd = hostapd.add_ap(apdev[0], hparams)

    fname = params['prefix'] + ".milenage_db.sqlite"
    cmd = subprocess.Popen(['../../hostapd/hlr_auc_gw',
                            '-D', fname, "FOO"],
                           stdout=subprocess.PIPE)
    res = cmd.stdout.read().decode().strip()
    cmd.stdout.close()
    logger.debug("hlr_auc_gw response: " + res)

    try:
        import sqlite3
    except ImportError:
        raise HwsimSkip("No sqlite3 module available")
    con = sqlite3.connect(fname)
    with con:
        cur = con.cursor()
        try:
            cur.execute("INSERT INTO milenage(imsi,ki,opc,amf,sqn) VALUES ('232010000000000', '90dca4eda45b53cf0f12d7c9c3bc6a89', 'cb9cccc4b9258e6dca4760379fb82581', '61df', '000000000000')")
        except sqlite3.IntegrityError as e:
            pass

    class test_handler3(SocketServer.DatagramRequestHandler):
        def handle(self):
            data = self.request[0].decode().strip()
            socket = self.request[1]
            logger.debug("Received hlr_auc_gw request: " + data)
            cmd = subprocess.Popen(['../../hostapd/hlr_auc_gw',
                                    '-D', fname, data],
                                   stdout=subprocess.PIPE)
            res = cmd.stdout.read().decode().strip()
            cmd.stdout.close()
            logger.debug("hlr_auc_gw response: " + res)
            socket.sendto(res.encode(), self.client_address)

    server = SocketServer.UnixDatagramServer(sockpath, test_handler3)
    server.timeout = 1

    id = dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP WPA-EAP-SHA256",
                        eap="SIM", identity="1232010000000000",
                        password="90dca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb82581",
                        scan_freq="2412", wait_connect=False)
    server.handle_request()
    dev[0].wait_connected()
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

def test_eap_tls_sha512(dev, apdev, params):
    """EAP-TLS with SHA512 signature"""
    params = int_eap_server_params()
    params["ca_cert"] = "auth_serv/sha512-ca.pem"
    params["server_cert"] = "auth_serv/sha512-server.pem"
    params["private_key"] = "auth_serv/sha512-server.key"
    hostapd.add_ap(apdev[0], params)

    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TLS",
                   identity="tls user sha512",
                   ca_cert="auth_serv/sha512-ca.pem",
                   client_cert="auth_serv/sha512-user.pem",
                   private_key="auth_serv/sha512-user.key",
                   scan_freq="2412")
    dev[1].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TLS",
                   identity="tls user sha512",
                   ca_cert="auth_serv/sha512-ca.pem",
                   client_cert="auth_serv/sha384-user.pem",
                   private_key="auth_serv/sha384-user.key",
                   scan_freq="2412")

def test_eap_tls_sha384(dev, apdev, params):
    """EAP-TLS with SHA384 signature"""
    params = int_eap_server_params()
    params["ca_cert"] = "auth_serv/sha512-ca.pem"
    params["server_cert"] = "auth_serv/sha384-server.pem"
    params["private_key"] = "auth_serv/sha384-server.key"
    hostapd.add_ap(apdev[0], params)

    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TLS",
                   identity="tls user sha512",
                   ca_cert="auth_serv/sha512-ca.pem",
                   client_cert="auth_serv/sha512-user.pem",
                   private_key="auth_serv/sha512-user.key",
                   scan_freq="2412")
    dev[1].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TLS",
                   identity="tls user sha512",
                   ca_cert="auth_serv/sha512-ca.pem",
                   client_cert="auth_serv/sha384-user.pem",
                   private_key="auth_serv/sha384-user.key",
                   scan_freq="2412")

def test_ap_wpa2_eap_assoc_rsn(dev, apdev):
    """WPA2-Enterprise AP and association request RSN IE differences"""
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hostapd.add_ap(apdev[0], params)

    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap-11w")
    params["ieee80211w"] = "2"
    hostapd.add_ap(apdev[1], params)

    # Success cases with optional RSN IE fields removed one by one
    tests = [("Normal wpa_supplicant assoc req RSN IE",
              "30140100000fac040100000fac040100000fac010000"),
             ("Extra PMKIDCount field in RSN IE",
              "30160100000fac040100000fac040100000fac0100000000"),
             ("Extra Group Management Cipher Suite in RSN IE",
              "301a0100000fac040100000fac040100000fac0100000000000fac06"),
             ("Extra undefined extension field in RSN IE",
              "301c0100000fac040100000fac040100000fac0100000000000fac061122"),
             ("RSN IE without RSN Capabilities",
              "30120100000fac040100000fac040100000fac01"),
             ("RSN IE without AKM", "300c0100000fac040100000fac04"),
             ("RSN IE without pairwise", "30060100000fac04"),
             ("RSN IE without group", "30020100")]
    for title, ie in tests:
        logger.info(title)
        set_test_assoc_ie(dev[0], ie)
        dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="GPSK",
                       identity="gpsk user",
                       password="abcdefghijklmnop0123456789abcdef",
                       scan_freq="2412")
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()

    tests = [("Normal wpa_supplicant assoc req RSN IE",
              "30140100000fac040100000fac040100000fac01cc00"),
             ("Group management cipher included in assoc req RSN IE",
              "301a0100000fac040100000fac040100000fac01cc000000000fac06")]
    for title, ie in tests:
        logger.info(title)
        set_test_assoc_ie(dev[0], ie)
        dev[0].connect("test-wpa2-eap-11w", key_mgmt="WPA-EAP", ieee80211w="1",
                       eap="GPSK", identity="gpsk user",
                       password="abcdefghijklmnop0123456789abcdef",
                       scan_freq="2412")
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()

    tests = [("Invalid group cipher", "30060100000fac02", [40, 41]),
             ("Invalid pairwise cipher", "300c0100000fac040100000fac02", 42)]
    for title, ie, status in tests:
        logger.info(title)
        set_test_assoc_ie(dev[0], ie)
        dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="GPSK",
                       identity="gpsk user",
                       password="abcdefghijklmnop0123456789abcdef",
                       scan_freq="2412", wait_connect=False)
        ev = dev[0].wait_event(["CTRL-EVENT-ASSOC-REJECT"])
        if ev is None:
            raise Exception("Association rejection not reported")
        ok = False
        if isinstance(status, list):
            for i in status:
                ok = "status_code=" + str(i) in ev
                if ok:
                    break
        else:
            ok = "status_code=" + str(status) in ev
        if not ok:
            raise Exception("Unexpected status code: " + ev)
        dev[0].request("REMOVE_NETWORK all")
        dev[0].dump_monitor()

    tests = [("Management frame protection not enabled",
              "30140100000fac040100000fac040100000fac010000", 31),
             ("Unsupported management group cipher",
              "301a0100000fac040100000fac040100000fac01cc000000000fac0b", 46)]
    for title, ie, status in tests:
        logger.info(title)
        set_test_assoc_ie(dev[0], ie)
        dev[0].connect("test-wpa2-eap-11w", key_mgmt="WPA-EAP", ieee80211w="1",
                       eap="GPSK", identity="gpsk user",
                       password="abcdefghijklmnop0123456789abcdef",
                       scan_freq="2412", wait_connect=False)
        ev = dev[0].wait_event(["CTRL-EVENT-ASSOC-REJECT"])
        if ev is None:
            raise Exception("Association rejection not reported")
        if "status_code=" + str(status) not in ev:
            raise Exception("Unexpected status code: " + ev)
        dev[0].request("REMOVE_NETWORK all")
        dev[0].dump_monitor()

def test_eap_tls_ext_cert_check(dev, apdev):
    """EAP-TLS and external server certification validation"""
    # With internal server certificate chain validation
    id = dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TLS",
                        identity="tls user",
                        ca_cert="auth_serv/ca.pem",
                        client_cert="auth_serv/user.pem",
                        private_key="auth_serv/user.key",
                        phase1="tls_ext_cert_check=1", scan_freq="2412",
                        only_add_network=True)
    run_ext_cert_check(dev, apdev, id)

def test_eap_ttls_ext_cert_check(dev, apdev):
    """EAP-TTLS and external server certification validation"""
    # Without internal server certificate chain validation
    id = dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TTLS",
                        identity="pap user", anonymous_identity="ttls",
                        password="password", phase2="auth=PAP",
                        phase1="tls_ext_cert_check=1", scan_freq="2412",
                        only_add_network=True)
    run_ext_cert_check(dev, apdev, id)

def test_eap_peap_ext_cert_check(dev, apdev):
    """EAP-PEAP and external server certification validation"""
    # With internal server certificate chain validation
    id = dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="PEAP",
                        identity="user", anonymous_identity="peap",
                        ca_cert="auth_serv/ca.pem",
                        password="password", phase2="auth=MSCHAPV2",
                        phase1="tls_ext_cert_check=1", scan_freq="2412",
                        only_add_network=True)
    run_ext_cert_check(dev, apdev, id)

def test_eap_fast_ext_cert_check(dev, apdev):
    """EAP-FAST and external server certification validation"""
    check_eap_capa(dev[0], "FAST")
    # With internal server certificate chain validation
    dev[0].request("SET blob fast_pac_auth_ext ")
    id = dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="FAST",
                        identity="user", anonymous_identity="FAST",
                        ca_cert="auth_serv/ca.pem",
                        password="password", phase2="auth=GTC",
                        phase1="tls_ext_cert_check=1 fast_provisioning=2",
                        pac_file="blob://fast_pac_auth_ext",
                        scan_freq="2412",
                        only_add_network=True)
    run_ext_cert_check(dev, apdev, id)

def run_ext_cert_check(dev, apdev, net_id):
    check_ext_cert_check_support(dev[0])
    if not openssl_imported:
        raise HwsimSkip("OpenSSL python method not available")

    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].select_network(net_id)
    certs = {}
    while True:
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-PEER-CERT",
                                "CTRL-REQ-EXT_CERT_CHECK",
                                "CTRL-EVENT-EAP-SUCCESS"], timeout=10)
        if ev is None:
            raise Exception("No peer server certificate event seen")
        if "CTRL-EVENT-EAP-PEER-CERT" in ev:
            depth = None
            cert = None
            vals = ev.split(' ')
            for v in vals:
                if v.startswith("depth="):
                    depth = int(v.split('=')[1])
                elif v.startswith("cert="):
                    cert = v.split('=')[1]
            if depth is not None and cert:
                certs[depth] = binascii.unhexlify(cert)
        elif "CTRL-EVENT-EAP-SUCCESS" in ev:
            raise Exception("Unexpected EAP-Success")
        elif "CTRL-REQ-EXT_CERT_CHECK" in ev:
            id = ev.split(':')[0].split('-')[-1]
            break
    if 0 not in certs:
        raise Exception("Server certificate not received")
    if 1 not in certs:
        raise Exception("Server certificate issuer not received")

    cert = OpenSSL.crypto.load_certificate(OpenSSL.crypto.FILETYPE_ASN1,
                                           certs[0])
    cn = cert.get_subject().commonName
    logger.info("Server certificate CN=" + cn)

    issuer = OpenSSL.crypto.load_certificate(OpenSSL.crypto.FILETYPE_ASN1,
                                             certs[1])
    icn = issuer.get_subject().commonName
    logger.info("Issuer certificate CN=" + icn)

    if cn != "server.w1.fi":
        raise Exception("Unexpected server certificate CN: " + cn)
    if icn != "Root CA":
        raise Exception("Unexpected server certificate issuer CN: " + icn)

    ev = dev[0].wait_event(["CTRL-EVENT-EAP-SUCCESS"], timeout=0.1)
    if ev:
        raise Exception("Unexpected EAP-Success before external check result indication")

    dev[0].request("CTRL-RSP-EXT_CERT_CHECK-" + id + ":good")
    dev[0].wait_connected()

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()
    if "FAIL" in dev[0].request("PMKSA_FLUSH"):
        raise Exception("PMKSA_FLUSH failed")
    dev[0].request("SET blob fast_pac_auth_ext ")
    dev[0].request("RECONNECT")

    ev = dev[0].wait_event(["CTRL-REQ-EXT_CERT_CHECK"], timeout=10)
    if ev is None:
        raise Exception("No peer server certificate event seen (2)")
    id = ev.split(':')[0].split('-')[-1]
    dev[0].request("CTRL-RSP-EXT_CERT_CHECK-" + id + ":bad")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"], timeout=5)
    if ev is None:
        raise Exception("EAP-Failure not reported")
    dev[0].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected()

def test_eap_tls_errors(dev, apdev):
    """EAP-TLS error cases"""
    params = int_eap_server_params()
    params['fragment_size'] = '100'
    hostapd.add_ap(apdev[0], params)
    with alloc_fail(dev[0], 1,
                    "eap_peer_tls_reassemble_fragment"):
        dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TLS",
                       identity="tls user", ca_cert="auth_serv/ca.pem",
                       client_cert="auth_serv/user.pem",
                       private_key="auth_serv/user.key",
                       wait_connect=False, scan_freq="2412")
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()

    with alloc_fail(dev[0], 1, "eap_tls_init"):
        dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TLS",
                       identity="tls user", ca_cert="auth_serv/ca.pem",
                       client_cert="auth_serv/user.pem",
                       private_key="auth_serv/user.key",
                       wait_connect=False, scan_freq="2412")
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()

    with alloc_fail(dev[0], 1, "eap_peer_tls_ssl_init"):
        dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TLS",
                       identity="tls user", ca_cert="auth_serv/ca.pem",
                       client_cert="auth_serv/user.pem",
                       private_key="auth_serv/user.key",
                       engine="1",
                       wait_connect=False, scan_freq="2412")
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
        ev = dev[0].wait_event(["CTRL-REQ-PIN"], timeout=5)
        if ev is None:
            raise Exception("No CTRL-REQ-PIN seen")
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()

    tests = ["eap_peer_tls_derive_key;eap_tls_success",
             "eap_peer_tls_derive_session_id;eap_tls_success",
             "eap_tls_getKey",
             "eap_tls_get_emsk",
             "eap_tls_get_session_id"]
    for func in tests:
        with alloc_fail(dev[0], 1, func):
            dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TLS",
                           identity="tls user@domain",
                           ca_cert="auth_serv/ca.pem",
                           client_cert="auth_serv/user.pem",
                           private_key="auth_serv/user.key",
                           erp="1",
                           wait_connect=False, scan_freq="2412")
            wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

    with alloc_fail(dev[0], 1, "eap_unauth_tls_init"):
        dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="UNAUTH-TLS",
                       identity="unauth-tls", ca_cert="auth_serv/ca.pem",
                       wait_connect=False, scan_freq="2412")
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()

    with alloc_fail(dev[0], 1, "eap_peer_tls_ssl_init;eap_unauth_tls_init"):
        dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="UNAUTH-TLS",
                       identity="unauth-tls", ca_cert="auth_serv/ca.pem",
                       wait_connect=False, scan_freq="2412")
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()

    with alloc_fail(dev[0], 1, "eap_wfa_unauth_tls_init"):
        dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP",
                       eap="WFA-UNAUTH-TLS",
                       identity="osen@example.com", ca_cert="auth_serv/ca.pem",
                       wait_connect=False, scan_freq="2412")
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()

    with alloc_fail(dev[0], 1, "eap_peer_tls_ssl_init;eap_wfa_unauth_tls_init"):
        dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP",
                       eap="WFA-UNAUTH-TLS",
                       identity="osen@example.com", ca_cert="auth_serv/ca.pem",
                       wait_connect=False, scan_freq="2412")
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()

def test_ap_wpa2_eap_status(dev, apdev):
    """EAP state machine status information"""
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hostapd.add_ap(apdev[0], params)
    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="PEAP",
                   identity="cert user",
                   ca_cert="auth_serv/ca.pem", phase2="auth=TLS",
                   ca_cert2="auth_serv/ca.pem",
                   client_cert2="auth_serv/user.pem",
                   private_key2="auth_serv/user.key",
                   scan_freq="2412", wait_connect=False)
    success = False
    states = []
    method_states = []
    decisions = []
    req_methods = []
    selected_methods = []
    connected = False
    for i in range(100000):
        if not connected and i % 10 == 9:
            ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED"], timeout=0.0001)
            if ev:
                connected = True
        s = dev[0].get_status(extra="VERBOSE")
        if 'EAP state' in s:
            state = s['EAP state']
            if state:
                if state not in states:
                    states.append(state)
                if state == "SUCCESS":
                    success = True
                    break
        if 'methodState' in s:
            val = s['methodState']
            if val not in method_states:
                method_states.append(val)
        if 'decision' in s:
            val = s['decision']
            if val not in decisions:
                decisions.append(val)
        if 'reqMethod' in s:
            val = s['reqMethod']
            if val not in req_methods:
                req_methods.append(val)
        if 'selectedMethod' in s:
            val = s['selectedMethod']
            if val not in selected_methods:
                selected_methods.append(val)
    logger.info("Iterations: %d" % i)
    logger.info("EAP states: " + str(states))
    logger.info("methodStates: " + str(method_states))
    logger.info("decisions: " + str(decisions))
    logger.info("reqMethods: " + str(req_methods))
    logger.info("selectedMethods: " + str(selected_methods))
    if not success:
        raise Exception("EAP did not succeed")
    if not connected:
        dev[0].wait_connected()
    dev[0].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected()

def test_ap_wpa2_eap_gpsk_ptk_rekey_ap(dev, apdev):
    """WPA2-Enterprise with EAP-GPSK and PTK rekey enforced by AP"""
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    params['wpa_ptk_rekey'] = '2'
    hapd = hostapd.add_ap(apdev[0], params)
    id = eap_connect(dev[0], hapd, "GPSK", "gpsk user",
                     password="abcdefghijklmnop0123456789abcdef")
    ev = dev[0].wait_event(["WPA: Key negotiation completed"])
    if ev is None:
        raise Exception("PTK rekey timed out")
    time.sleep(0.1)
    hwsim_utils.test_connectivity(dev[0], hapd)

def test_ap_wpa2_eap_wildcard_ssid(dev, apdev):
    """WPA2-Enterprise connection using EAP-GPSK and wildcard SSID"""
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].connect(bssid=apdev[0]['bssid'], key_mgmt="WPA-EAP", eap="GPSK",
                   identity="gpsk user",
                   password="abcdefghijklmnop0123456789abcdef",
                   scan_freq="2412")

def test_ap_wpa2_eap_psk_mac_addr_change(dev, apdev):
    """WPA2-Enterprise connection using EAP-PSK after MAC address change"""
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)

    cmd = subprocess.Popen(['ps', '-eo', 'pid,command'], stdout=subprocess.PIPE)
    res = cmd.stdout.read().decode()
    cmd.stdout.close()
    pid = 0
    for p in res.splitlines():
        if "wpa_supplicant" not in p:
            continue
        if dev[0].ifname not in p:
            continue
        pid = int(p.strip().split(' ')[0])
    if pid == 0:
        logger.info("Could not find wpa_supplicant PID")
    else:
        logger.info("wpa_supplicant PID %d" % pid)

    addr = dev[0].get_status_field("address")
    subprocess.call(['ip', 'link', 'set', 'dev', dev[0].ifname, 'down'])
    subprocess.call(['ip', 'link', 'set', 'dev', dev[0].ifname, 'address',
                     '02:11:22:33:44:55'])
    subprocess.call(['ip', 'link', 'set', 'dev', dev[0].ifname, 'up'])
    addr1 = dev[0].get_status_field("address")
    if addr1 != '02:11:22:33:44:55':
        raise Exception("Failed to change MAC address")

    # Scan using the externally set MAC address, stop the wpa_supplicant
    # process to avoid it from processing the ifdown event before the interface
    # is already UP, change the MAC address back, allow the wpa_supplicant
    # process to continue. This will result in the ifdown + ifup sequence of
    # RTM_NEWLINK events to be processed while the interface is already UP.
    try:
        dev[0].scan_for_bss(apdev[0]['bssid'], freq=2412)
        os.kill(pid, signal.SIGSTOP)
        time.sleep(0.1)
    finally:
        subprocess.call(['ip', 'link', 'set', 'dev', dev[0].ifname, 'down'])
        subprocess.call(['ip', 'link', 'set', 'dev', dev[0].ifname, 'address',
                         addr])
        subprocess.call(['ip', 'link', 'set', 'dev', dev[0].ifname, 'up'])
        time.sleep(0.1)
        os.kill(pid, signal.SIGCONT)

    eap_connect(dev[0], hapd, "PSK", "psk.user@example.com",
                password_hex="0123456789abcdef0123456789abcdef")

    addr2 = dev[0].get_status_field("address")
    if addr != addr2:
        raise Exception("Failed to restore MAC address")

def test_ap_wpa2_eap_server_get_id(dev, apdev):
    """Internal EAP server and dot1xAuthSessionUserName"""
    params = int_eap_server_params()
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "TLS", "tls user", ca_cert="auth_serv/ca.pem",
                client_cert="auth_serv/user.pem",
                private_key="auth_serv/user.key")
    sta = hapd.get_sta(dev[0].own_addr())
    if 'dot1xAuthSessionUserName' not in sta:
        raise Exception("No dot1xAuthSessionUserName included")
    user = sta['dot1xAuthSessionUserName']
    if user != "tls user":
        raise Exception("Unexpected dot1xAuthSessionUserName value: " + user)

def test_ap_wpa2_radius_server_get_id(dev, apdev):
    """External RADIUS server and dot1xAuthSessionUserName"""
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(apdev[0], params)
    eap_connect(dev[0], hapd, "TTLS", "test-user",
                anonymous_identity="ttls", password="password",
                ca_cert="auth_serv/ca.pem", phase2="auth=PAP")
    sta = hapd.get_sta(dev[0].own_addr())
    if 'dot1xAuthSessionUserName' not in sta:
        raise Exception("No dot1xAuthSessionUserName included")
    user = sta['dot1xAuthSessionUserName']
    if user != "real-user":
        raise Exception("Unexpected dot1xAuthSessionUserName value: " + user)

def test_openssl_systemwide_policy(dev, apdev, test_params):
    """OpenSSL systemwide policy and overrides"""
    prefix = "openssl_systemwide_policy"
    pidfile = os.path.join(test_params['logdir'], prefix + '.pid-wpas')
    try:
        with HWSimRadio() as (radio, iface):
            run_openssl_systemwide_policy(iface, apdev, test_params)
    finally:
        if os.path.exists(pidfile):
            with open(pidfile, 'r') as f:
                pid = int(f.read().strip())
                os.kill(pid, signal.SIGTERM)

def write_openssl_cnf(cnf, MinProtocol=None, CipherString=None):
    with open(cnf, "w") as f:
        f.write("""openssl_conf = default_conf
[default_conf]
ssl_conf = ssl_sect
[ssl_sect]
system_default = system_default_sect
[system_default_sect]
""")
        if MinProtocol:
            f.write("MinProtocol = %s\n" % MinProtocol)
        if CipherString:
            f.write("CipherString = %s\n" % CipherString)

def run_openssl_systemwide_policy(iface, apdev, test_params):
    prefix = "openssl_systemwide_policy"
    logfile = os.path.join(test_params['logdir'], prefix + '.log-wpas')
    pidfile = os.path.join(test_params['logdir'], prefix + '.pid-wpas')
    conffile = os.path.join(test_params['logdir'], prefix + '.conf')
    openssl_cnf = os.path.join(test_params['logdir'], prefix + '.openssl.cnf')

    write_openssl_cnf(openssl_cnf, "TLSv1.2", "DEFAULT@SECLEVEL=2")

    with open(conffile, 'w') as f:
        f.write("ctrl_interface=DIR=/var/run/wpa_supplicant\n")

    params = int_eap_server_params()
    params['tls_flags'] = "[DISABLE-TLSv1.1][DISABLE-TLSv1.2][DISABLE-TLSv1.3]"

    hapd = hostapd.add_ap(apdev[0], params)

    prg = os.path.join(test_params['logdir'],
                       'alt-wpa_supplicant/wpa_supplicant/wpa_supplicant')
    if not os.path.exists(prg):
        prg = '../../wpa_supplicant/wpa_supplicant'
    arg = [prg, '-BddtK', '-P', pidfile, '-f', logfile,
           '-Dnl80211', '-c', conffile, '-i', iface]
    logger.info("Start wpa_supplicant: " + str(arg))
    subprocess.call(arg, env={'OPENSSL_CONF': openssl_cnf})
    wpas = WpaSupplicant(ifname=iface)
    try:
        finish_openssl_systemwide_policy(wpas)
    finally:
        wpas.close_monitor()
        wpas.request("TERMINATE")

def finish_openssl_systemwide_policy(wpas):
    if "PONG" not in wpas.request("PING"):
        raise Exception("Could not PING wpa_supplicant")
    tls = wpas.request("GET tls_library")
    if not tls.startswith("OpenSSL"):
        raise HwsimSkip("Not using OpenSSL")

    # Use default configuration without any TLS version overrides. This should
    # end up using OpenSSL systemwide policy and result in failure to find a
    # compatible protocol version.
    ca_file = os.path.join(os.getcwd(), "auth_serv/ca.pem")
    id = wpas.connect("test-wpa2-eap", key_mgmt="WPA-EAP", eap="TTLS",
                      identity="pap user", anonymous_identity="ttls",
                      password="password", phase2="auth=PAP",
                      ca_cert=ca_file,
                      scan_freq="2412", wait_connect=False)
    ev = wpas.wait_event(["CTRL-EVENT-EAP-STARTED"], timeout=10)
    if ev is None:
        raise Exception("EAP not started")
    ev = wpas.wait_event(["CTRL-EVENT-EAP-STATUS status='local TLS alert'"],
                         timeout=1)
    if ev is None:
        raise HwsimSkip("OpenSSL systemwide policy not supported")
    wpas.request("DISCONNECT")
    wpas.wait_disconnected()
    wpas.dump_monitor()

    # Explicitly allow TLSv1.0 to be used to override OpenSSL systemwide policy
    wpas.set_network_quoted(id, "openssl_ciphers", "DEFAULT@SECLEVEL=1")
    wpas.set_network_quoted(id, "phase1", "tls_disable_tlsv1_0=0")
    wpas.select_network(id, freq="2412")
    wpas.wait_connected()

def test_ap_wpa2_eap_tls_tod(dev, apdev):
    """EAP-TLS server certificate validation and TOD-STRICT"""
    check_tls_tod(dev[0])
    params = int_eap_server_params()
    params["server_cert"] = "auth_serv/server-certpol.pem"
    params["private_key"] = "auth_serv/server-certpol.key"
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP",
                   eap="TLS", identity="tls user",
                   wait_connect=False, scan_freq="2412",
                   ca_cert="auth_serv/ca.pem",
                   client_cert="auth_serv/user.pem",
                   private_key="auth_serv/user.key")
    tod0 = None
    tod1 = None
    while tod0 is None or tod1 is None:
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-PEER-CERT"], timeout=10)
        if ev is None:
            raise Exception("Peer certificate not reported")
        if "depth=1 " in ev and "hash=" in ev:
            tod1 = " tod=1" in ev
        if "depth=0 " in ev and "hash=" in ev:
            tod0 = " tod=1" in ev
    dev[0].wait_connected()
    if not tod0:
        raise Exception("TOD-STRICT policy not reported for server certificate")
    if tod1:
        raise Exception("TOD-STRICT policy unexpectedly reported for CA certificate")

def test_ap_wpa2_eap_tls_tod_tofu(dev, apdev):
    """EAP-TLS server certificate validation and TOD-TOFU"""
    check_tls_tod(dev[0])
    params = int_eap_server_params()
    params["server_cert"] = "auth_serv/server-certpol2.pem"
    params["private_key"] = "auth_serv/server-certpol2.key"
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP",
                   eap="TLS", identity="tls user",
                   wait_connect=False, scan_freq="2412",
                   ca_cert="auth_serv/ca.pem",
                   client_cert="auth_serv/user.pem",
                   private_key="auth_serv/user.key")
    tod0 = None
    tod1 = None
    while tod0 is None or tod1 is None:
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-PEER-CERT"], timeout=10)
        if ev is None:
            raise Exception("Peer certificate not reported")
        if "depth=1 " in ev and "hash=" in ev:
            tod1 = " tod=2" in ev
        if "depth=0 " in ev and "hash=" in ev:
            tod0 = " tod=2" in ev
    dev[0].wait_connected()
    if not tod0:
        raise Exception("TOD-TOFU policy not reported for server certificate")
    if tod1:
        raise Exception("TOD-TOFU policy unexpectedly reported for CA certificate")

def test_ap_wpa2_eap_sake_no_control_port(dev, apdev):
    """WPA2-Enterprise connection using EAP-SAKE without nl80211 control port"""
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    params['driver_params'] = "control_port=0"
    hapd = hostapd.add_ap(apdev[0], params)
    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    wpas.interface_add("wlan5", drv_params="control_port=0")
    eap_connect(wpas, hapd, "SAKE", "sake user",
                password_hex="0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef")
    eap_reauth(wpas, "SAKE")

    logger.info("Negative test with incorrect password")
    wpas.request("REMOVE_NETWORK all")
    eap_connect(wpas, hapd, "SAKE", "sake user",
                password_hex="ff23456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",
                expect_failure=True)

def test_ap_wpa3_eap_transition_disable(dev, apdev):
    """WPA3-Enterprise transition disable indication"""
    skip_without_tkip(dev[0])
    params = hostapd.wpa2_eap_params(ssid="test-wpa3-eap")
    params["ieee80211w"] = "1"
    params['transition_disable'] = '0x04'
    hapd = hostapd.add_ap(apdev[0], params)
    id = dev[0].connect("test-wpa3-eap", key_mgmt="WPA-EAP", ieee80211w="1",
                        proto="WPA WPA2", pairwise="CCMP", group="TKIP CCMP",
                        eap="GPSK", identity="gpsk user",
                        password="abcdefghijklmnop0123456789abcdef",
                        scan_freq="2412")
    ev = dev[0].wait_event(["TRANSITION-DISABLE"], timeout=1)
    if ev is None:
        raise Exception("Transition disable not indicated")
    if ev.split(' ')[1] != "04":
        raise Exception("Unexpected transition disable bitmap: " + ev)

    val = dev[0].get_network(id, "ieee80211w")
    if val != "2":
        raise Exception("Unexpected ieee80211w value: " + val)
    val = dev[0].get_network(id, "key_mgmt")
    if val != "WPA-EAP":
        raise Exception("Unexpected key_mgmt value: " + val)
    val = dev[0].get_network(id, "group")
    if val != "CCMP":
        raise Exception("Unexpected group value: " + val)
    val = dev[0].get_network(id, "proto")
    if val != "RSN":
        raise Exception("Unexpected proto value: " + val)

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()
    dev[0].request("RECONNECT")
    dev[0].wait_connected()
