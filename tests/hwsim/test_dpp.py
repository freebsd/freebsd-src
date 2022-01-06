# Test cases for Device Provisioning Protocol (DPP)
# Copyright (c) 2017, Qualcomm Atheros, Inc.
# Copyright (c) 2018-2019, The Linux Foundation
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

import base64
import binascii
import hashlib
import logging
logger = logging.getLogger()
import os
import socket
import struct
import subprocess
import time
try:
    from socketserver import StreamRequestHandler, TCPServer
except ImportError:
    from SocketServer import StreamRequestHandler, TCPServer

import hostapd
import hwsim_utils
from hwsim import HWSimRadio
from utils import *
from wpasupplicant import WpaSupplicant
from wlantest import WlantestCapture

try:
    import OpenSSL
    openssl_imported = True
except ImportError:
    openssl_imported = False

def check_dpp_capab(dev, brainpool=False, min_ver=1):
    if "UNKNOWN COMMAND" in dev.request("DPP_BOOTSTRAP_GET_URI 0"):
        raise HwsimSkip("DPP not supported")
    if brainpool:
        tls = dev.request("GET tls_library")
        if not tls.startswith("OpenSSL") or "run=BoringSSL" in tls:
            raise HwsimSkip("Crypto library does not support Brainpool curves: " + tls)
    capa = dev.request("GET_CAPABILITY dpp")
    ver = 1
    if capa.startswith("DPP="):
        ver = int(capa[4:])
    if ver < min_ver:
        raise HwsimSkip("DPP version %d not supported" % min_ver)
    return ver

def wait_dpp_fail(dev, expected=None):
    ev = dev.wait_event(["DPP-FAIL"], timeout=5)
    if ev is None:
        raise Exception("Failure not reported")
    if expected and expected not in ev:
        raise Exception("Unexpected result: " + ev)

def test_dpp_qr_code_parsing(dev, apdev):
    """DPP QR Code parsing"""
    check_dpp_capab(dev[0])
    id = []

    tests = ["DPP:C:81/1,115/36;K:MDkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDIgADM2206avxHJaHXgLMkq/24e0rsrfMP9K1Tm8gx+ovP0I=;;",
             "DPP:C:81/1,81/2,81/3,81/4,81/5,81/6,81/7,81/8,81/9,81/10,81/11,81/12,81/13,82/14,83/1,83/2,83/3,83/4,83/5,83/6,83/7,83/8,83/9,84/5,84/6,84/7,84/8,84/9,84/10,84/11,84/12,84/13,115/36;K:MDkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDIgADM2206avxHJaHXgLMkq/24e0rsrfMP9K1Tm8gx+ovP0I=;;",
             "DPP:C:81/1,2,3,4,5,6,7,8,9,10,11,12,13,82/14,83/1,2,3,4,5,6,7,8,9,84/5,6,7,8,9,10,11,12,13,115/36;K:MDkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDIgADM2206avxHJaHXgLMkq/24e0rsrfMP9K1Tm8gx+ovP0I=;;",
             "DPP:C:81/1,2,3;K:MDkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDIgADM2206avxHJaHXgLMkq/24e0rsrfMP9K1Tm8gx+ovP0I=;;",
             "DPP:I:SN=4774LH2b4044;M:010203040506;K:MDkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDIgADURzxmttZoIRIPWGoQMV00XHWCAQIhXruVWOz0NjlkIA=;;",
             "DPP:I:;M:010203040506;K:MDkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDIgADURzxmttZoIRIPWGoQMV00XHWCAQIhXruVWOz0NjlkIA=;;"]
    for uri in tests:
        id.append(dev[0].dpp_qr_code(uri))

        uri2 = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id[-1])
        if uri != uri2:
            raise Exception("Returned URI does not match")

    tests = ["foo",
             "DPP:",
             "DPP:;;",
             "DPP:C:1/2;M:;K;;",
             "DPP:I:;M:01020304050;K:MDkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDIgADURzxmttZoIRIPWGoQMV00XHWCAQIhXruVWOz0NjlkIA=;;",
             "DPP:K:" + base64.b64encode(b"hello").decode() + ";;",
             "DPP:K:MEkwEwYHKoZIzj0CAQYIKoZIzj0DAQEDMgAEXiJuIWt1Q/CPCkuULechh37UsXPmbUANOeN5U9sOQROE4o/NEFeFEejROHYwwehF;;",
             "DPP:K:MFwwDQYJKoZIhvcNAQEBBQADSwAwSAJBANNZaZA4T/kRDjnmpI1ACOJhAuTIIEk2KFOpS6XPpGF+EVr/ao3XemkE0/nzXmGaLzLqTUCJknSdxTnVPeWfCVsCAwEAAQ==;;",
             "DPP:K:MIIBCjCB0wYHKoZIzj0CATCBxwIBATAkBgcqhkjOPQEBAhkA/////////////////////v//////////MEsEGP////////////////////7//////////AQYZCEFGeWcgOcPp+mrciQwSf643uzBRrmxAxUAMEWub8hCL2TtV5Uo04Eg6uEhltUEMQQYjagOsDCQ9ny/IOtDoYgA9P8K/YL/EBIHGSuV/8jaeGMQEe1rJM3Vc/l3oR55SBECGQD///////////////+Z3vg2FGvJsbTSKDECAQEDMgAEXiJuIWt1Q/CPCkuULechh37UsXPmbUANOeN5U9sOQROE4o/NEFeFEejROHYwwehF;;",
             "DPP:I:foo\tbar;M:010203040506;K:MDkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDIgADURzxmttZoIRIPWGoQMV00XHWCAQIhXruVWOz0NjlkIA=;;",
             "DPP:C:1;K:MDkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDIgADM2206avxHJaHXgLMkqa24e0rsrfMP9K1Tm8gx+ovP0I=;;",
             "DPP:C:81/1a;K:MDkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDIgADM2206avxHJaHXgLMkqa24e0rsrfMP9K1Tm8gx+ovP0I=;;",
             "DPP:C:1/2000,81/-1;K:MDkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDIgADM2206avxHJaHXgLMkqa24e0rsrfMP9K1Tm8gx+ovP0I=;;",
             "DPP:C:-1/1;K:MDkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDIgADM2206avxHJaHXgLMkqa24e0rsrfMP9K1Tm8gx+ovP0I=;;"]
    for t in tests:
        res = dev[0].request("DPP_QR_CODE " + t)
        if "FAIL" not in res:
            raise Exception("Accepted invalid QR Code: " + t)

    logger.info("ID: " + str(id))
    if id[0] == id[1] or id[0] == id[2] or id[1] == id[2]:
        raise Exception("Duplicate ID returned")

    if "FAIL" not in dev[0].request("DPP_BOOTSTRAP_REMOVE 12345678"):
        raise Exception("DPP_BOOTSTRAP_REMOVE accepted unexpectedly")
    if "OK" not in dev[0].request("DPP_BOOTSTRAP_REMOVE %d" % id[1]):
        raise Exception("DPP_BOOTSTRAP_REMOVE failed")

    id = dev[0].dpp_bootstrap_gen()
    uri = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id)
    logger.info("Generated URI: " + uri)

    dev[0].dpp_qr_code(uri)

    id = dev[0].dpp_bootstrap_gen(chan="81/1,115/36", mac="010203040506",
                                  info="foo")
    uri = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id)
    logger.info("Generated URI: " + uri)

    dev[0].dpp_qr_code(uri)

def test_dpp_uri_version(dev, apdev):
    """DPP URI version information"""
    check_dpp_capab(dev[0], min_ver=2)

    id0 = dev[0].dpp_bootstrap_gen()
    uri = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id0)
    logger.info("Generated URI: " + uri)

    id1 = dev[0].dpp_qr_code(uri)
    uri = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id1)
    info = dev[0].request("DPP_BOOTSTRAP_INFO %d" % id1)
    logger.info("Parsed URI info:\n" + info)
    capa = dev[0].request("GET_CAPABILITY dpp")
    ver = 1
    if capa.startswith("DPP="):
        ver = int(capa[4:])
    if "version=%d" % ver not in info.splitlines():
        raise Exception("Unexpected version information (with indication)")

    dev[0].set("dpp_version_override", "1")
    id0 = dev[0].dpp_bootstrap_gen()
    uri = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id0)
    logger.info("Generated URI: " + uri)

    id1 = dev[0].dpp_qr_code(uri)
    uri = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id1)
    info = dev[0].request("DPP_BOOTSTRAP_INFO %d" % id1)
    logger.info("Parsed URI info:\n" + info)
    if "version=0" not in info.splitlines():
        raise Exception("Unexpected version information (without indication)")

def test_dpp_qr_code_parsing_fail(dev, apdev):
    """DPP QR Code parsing local failure"""
    check_dpp_capab(dev[0])
    with alloc_fail(dev[0], 1, "dpp_parse_uri_info"):
        if "FAIL" not in dev[0].request("DPP_QR_CODE DPP:I:SN=4774LH2b4044;M:010203040506;K:MDkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDIgADURzxmttZoIRIPWGoQMV00XHWCAQIhXruVWOz0NjlkIA=;;"):
            raise Exception("DPP_QR_CODE failure not reported")

    with alloc_fail(dev[0], 1, "dpp_parse_uri_pk"):
        if "FAIL" not in dev[0].request("DPP_QR_CODE DPP:K:MDkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDIgADURzxmttZoIRIPWGoQMV00XHWCAQIhXruVWOz0NjlkIA=;;"):
            raise Exception("DPP_QR_CODE failure not reported")

    with fail_test(dev[0], 1, "dpp_parse_uri_pk"):
        if "FAIL" not in dev[0].request("DPP_QR_CODE DPP:K:MDkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDIgADURzxmttZoIRIPWGoQMV00XHWCAQIhXruVWOz0NjlkIA=;;"):
            raise Exception("DPP_QR_CODE failure not reported")

    with alloc_fail(dev[0], 1, "dpp_parse_uri"):
        if "FAIL" not in dev[0].request("DPP_QR_CODE DPP:I:SN=4774LH2b4044;M:010203040506;K:MDkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDIgADURzxmttZoIRIPWGoQMV00XHWCAQIhXruVWOz0NjlkIA=;;"):
            raise Exception("DPP_QR_CODE failure not reported")

dpp_key_p256 = "30570201010420777fc55dc51e967c10ec051b91d860b5f1e6c934e48d5daffef98d032c64b170a00a06082a8648ce3d030107a124032200020c804188c7f85beb6e91070d2b3e5e39b90ca77b4d3c5251bc1844d6ca29dcad"
dpp_key_p384 = "307402010104302f56fdd83b5345cacb630eb7c22fa5ad5daba37307c95191e2a75756d137003bd8b32dbcb00eb5650c1eb499ecfcaec0a00706052b81040022a13403320003615ec2141b5b77aebb6523f8a012755f9a34405a8398d2ceeeebca7f5ce868bf55056cba4c4ec62fad3ed26dd29e0f23"
dpp_key_p521 = "308198020101044200c8010d5357204c252551aaf4e210343111e503fd1dc615b257058997c49b6b643c975226e93be8181cca3d83a7072defd161dfbdf433c19abe1f2ad51867a05761a00706052b81040023a1460344000301cdf3608b1305fe34a1f976095dcf001182b9973354efe156291a66830292f9babd8f412ad462958663e7a75d1d0610abdfc3dd95d40669f7ab3bc001668cfb3b7c"
dpp_key_bp256 = "3058020101042057133a676fb60bf2a3e6797e19833c7b0f89dc192ab99ab5fa377ae23a157765a00b06092b2403030208010107a12403220002945d9bf7ce30c9c1ac0ff21ca62b984d5bb80ff69d2be8c9716ab39a10d2caf0"
dpp_key_bp384 = "307802010104304902df9f3033a9b7128554c0851dc7127c3573eed150671dae74c0013e9896a9b1c22b6f7d43d8a2ebb7cd474dc55039a00b06092b240303020801010ba13403320003623cb5e68787f351faababf3425161571560add2e6f9a306fcbffb507735bf955bb46dd20ba246b0d5cadce73e5bd6a6"
dpp_key_bp512 = "30819802010104405803494226eb7e50bf0e90633f37e7e35d33f5fa502165eeba721d927f9f846caf12e925701d18e123abaaaf4a7edb4fc4de21ce18bc10c4d12e8b3439f74e40a00b06092b240303020801010da144034200033b086ccd47486522d35dc16fbb2229642c2e9e87897d45abbf21f9fb52acb5a6272b31d1b227c3e53720769cc16b4cb181b26cd0d35fe463218aaedf3b6ec00a"

def test_dpp_qr_code_curves(dev, apdev):
    """DPP QR Code and supported curves"""
    check_dpp_capab(dev[0])
    tests = [("prime256v1", dpp_key_p256),
             ("secp384r1", dpp_key_p384),
             ("secp521r1", dpp_key_p521)]
    for curve, hex in tests:
        id = dev[0].dpp_bootstrap_gen(key=hex)
        info = dev[0].request("DPP_BOOTSTRAP_INFO %d" % id)
        if "FAIL" in info:
            raise Exception("Failed to get info for " + curve)
        if "curve=" + curve not in info:
            raise Exception("Curve mismatch for " + curve)

def test_dpp_qr_code_curves_brainpool(dev, apdev):
    """DPP QR Code and supported Brainpool curves"""
    check_dpp_capab(dev[0], brainpool=True)
    tests = [("brainpoolP256r1", dpp_key_bp256),
             ("brainpoolP384r1", dpp_key_bp384),
             ("brainpoolP512r1", dpp_key_bp512)]
    for curve, hex in tests:
        id = dev[0].dpp_bootstrap_gen(key=hex)
        info = dev[0].request("DPP_BOOTSTRAP_INFO %d" % id)
        if "FAIL" in info:
            raise Exception("Failed to get info for " + curve)
        if "curve=" + curve not in info:
            raise Exception("Curve mismatch for " + curve)

def test_dpp_qr_code_unsupported_curve(dev, apdev):
    """DPP QR Code and unsupported curve"""
    check_dpp_capab(dev[0])

    id = dev[0].request("DPP_BOOTSTRAP_GEN type=qrcode curve=unsupported")
    if "FAIL" not in id:
        raise Exception("Unsupported curve accepted")

    tests = ["30",
             "305f02010104187f723ed9e1b41979ec5cd02eb82696efc76b40e277661049a00a06082a8648ce3d030101a134033200043f292614dea97c43f500f069e79ae9fb48f8b07369180de5eec8fa2bc9eea5af7a46dc335f52f10cb1c0e9464201d41b"]
    for hex in tests:
        id = dev[0].request("DPP_BOOTSTRAP_GEN type=qrcode key=" + hex)
        if "FAIL" not in id:
            raise Exception("Unsupported/invalid curve accepted")

def test_dpp_qr_code_keygen_fail(dev, apdev):
    """DPP QR Code and keygen failure"""
    check_dpp_capab(dev[0])

    with alloc_fail(dev[0], 1,
                    "crypto_ec_key_get_subject_public_key;dpp_keygen"):
        if "FAIL" not in dev[0].request("DPP_BOOTSTRAP_GEN type=qrcode"):
            raise Exception("Failure not reported")

    with alloc_fail(dev[0], 1, "base64_gen_encode;dpp_keygen"):
        if "FAIL" not in dev[0].request("DPP_BOOTSTRAP_GEN type=qrcode"):
            raise Exception("Failure not reported")

def test_dpp_qr_code_curve_select(dev, apdev):
    """DPP QR Code and curve selection"""
    check_dpp_capab(dev[0], brainpool=True)
    check_dpp_capab(dev[1], brainpool=True)

    bi = []
    for key in [dpp_key_p256, dpp_key_p384, dpp_key_p521,
                dpp_key_bp256, dpp_key_bp384, dpp_key_bp512]:
        id = dev[0].dpp_bootstrap_gen(chan="81/1", mac=True, key=key)
        info = dev[0].request("DPP_BOOTSTRAP_INFO %d" % id)
        for i in info.splitlines():
            if '=' in i:
                name, val = i.split('=')
                if name == "curve":
                    curve = val
                    break
        uri = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id)
        bi.append((curve, uri))

    for curve, uri in bi:
        logger.info("Curve: " + curve)
        logger.info("URI: " + uri)

        dev[0].dpp_listen(2412)
        dev[1].dpp_auth_init(uri=uri)
        wait_auth_success(dev[0], dev[1], configurator=dev[1], enrollee=dev[0],
                          allow_enrollee_failure=True, stop_responder=True,
                          stop_initiator=True)

def test_dpp_qr_code_auth_broadcast(dev, apdev):
    """DPP QR Code and authentication exchange (broadcast)"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    logger.info("dev0 displays QR Code")
    id0 = dev[0].dpp_bootstrap_gen(chan="81/1")
    uri0 = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id0)
    logger.info("dev1 scans QR Code and initiates DPP Authentication")
    dev[0].dpp_listen(2412)
    dev[1].dpp_auth_init(uri=uri0)
    wait_auth_success(dev[0], dev[1], stop_responder=True)

def test_dpp_qr_code_auth_unicast(dev, apdev):
    """DPP QR Code and authentication exchange (unicast)"""
    run_dpp_qr_code_auth_unicast(dev, apdev, None)

def test_dpp_qr_code_auth_unicast_ap_enrollee(dev, apdev):
    """DPP QR Code and authentication exchange (AP enrollee)"""
    run_dpp_qr_code_auth_unicast(dev, apdev, None, netrole="ap")

def run_dpp_configurator_enrollee(dev, apdev, conf_curve=None):
    run_dpp_qr_code_auth_unicast(dev, apdev, None, netrole="configurator",
                                 configurator=True, conf_curve=conf_curve,
                                 conf="configurator")
    ev = dev[0].wait_event(["DPP-CONFIGURATOR-ID"], timeout=2)
    if ev is None:
        raise Exception("No Configurator instance added")

def test_dpp_configurator_enrollee(dev, apdev):
    """DPP Configurator enrolling"""
    run_dpp_configurator_enrollee(dev, apdev)

def test_dpp_configurator_enrollee_prime256v1(dev, apdev):
    """DPP Configurator enrolling (prime256v1)"""
    run_dpp_configurator_enrollee(dev, apdev, conf_curve="prime256v1")

def test_dpp_configurator_enrollee_secp384r1(dev, apdev):
    """DPP Configurator enrolling (secp384r1)"""
    run_dpp_configurator_enrollee(dev, apdev, conf_curve="secp384r1")

def test_dpp_configurator_enrollee_secp521r1(dev, apdev):
    """DPP Configurator enrolling (secp521r1)"""
    run_dpp_configurator_enrollee(dev, apdev, conf_curve="secp521r1")

def test_dpp_configurator_enrollee_brainpoolP256r1(dev, apdev):
    """DPP Configurator enrolling (brainpoolP256r1)"""
    run_dpp_configurator_enrollee(dev, apdev, conf_curve="brainpoolP256r1")

def test_dpp_configurator_enrollee_brainpoolP384r1(dev, apdev):
    """DPP Configurator enrolling (brainpoolP384r1)"""
    run_dpp_configurator_enrollee(dev, apdev, conf_curve="brainpoolP384r1")

def test_dpp_configurator_enrollee_brainpoolP512r1(dev, apdev):
    """DPP Configurator enrolling (brainpoolP512r1)"""
    run_dpp_configurator_enrollee(dev, apdev, conf_curve="brainpoolP512r1")

def test_dpp_configurator_enroll_conf(dev, apdev):
    """DPP Configurator enrolling followed by use of the new Configurator"""
    check_dpp_capab(dev[0], min_ver=2)
    try:
        dev[0].set("dpp_config_processing", "2")
        run_dpp_configurator_enroll_conf(dev, apdev)
    finally:
        dev[0].set("dpp_config_processing", "0", allow_fail=True)

def run_dpp_configurator_enroll_conf(dev, apdev):
    run_dpp_qr_code_auth_unicast(dev, apdev, None, netrole="configurator",
                                 configurator=True, conf="configurator",
                                 qr="mutual", stop_responder=False)
    ev = dev[0].wait_event(["DPP-CONFIGURATOR-ID"], timeout=2)
    if ev is None:
        raise Exception("No Configurator instance added")
    dev[1].reset()
    dev[0].dump_monitor()

    ssid = "test-network"
    passphrase = "test-passphrase"
    dev[0].set("dpp_configurator_params",
               "conf=sta-psk ssid=%s pass=%s" % (binascii.hexlify(ssid.encode()).decode(), binascii.hexlify(passphrase.encode()).decode()))
    dev[0].dpp_listen(2412, role="configurator")
    id0 = dev[0].dpp_bootstrap_gen(chan="81/1")
    uri0 = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id0)
    dev[1].dpp_auth_init(uri=uri0, role="enrollee")
    wait_auth_success(dev[0], dev[1], configurator=dev[0], enrollee=dev[1])

def test_dpp_qr_code_curve_prime256v1(dev, apdev):
    """DPP QR Code and curve prime256v1"""
    run_dpp_qr_code_auth_unicast(dev, apdev, "prime256v1")

def test_dpp_qr_code_curve_secp384r1(dev, apdev):
    """DPP QR Code and curve secp384r1"""
    run_dpp_qr_code_auth_unicast(dev, apdev, "secp384r1")

def test_dpp_qr_code_curve_secp521r1(dev, apdev):
    """DPP QR Code and curve secp521r1"""
    run_dpp_qr_code_auth_unicast(dev, apdev, "secp521r1")

def test_dpp_qr_code_curve_brainpoolP256r1(dev, apdev):
    """DPP QR Code and curve brainpoolP256r1"""
    run_dpp_qr_code_auth_unicast(dev, apdev, "brainpoolP256r1")

def test_dpp_qr_code_curve_brainpoolP384r1(dev, apdev):
    """DPP QR Code and curve brainpoolP384r1"""
    run_dpp_qr_code_auth_unicast(dev, apdev, "brainpoolP384r1")

def test_dpp_qr_code_curve_brainpoolP512r1(dev, apdev):
    """DPP QR Code and curve brainpoolP512r1"""
    run_dpp_qr_code_auth_unicast(dev, apdev, "brainpoolP512r1")

def test_dpp_qr_code_set_key(dev, apdev):
    """DPP QR Code and fixed bootstrapping key"""
    run_dpp_qr_code_auth_unicast(dev, apdev, None, key="30770201010420e5143ac74682cc6869a830e8f5301a5fa569130ac329b1d7dd6f2a7495dbcbe1a00a06082a8648ce3d030107a144034200045e13e167c33dbc7d85541e5509600aa8139bbb3e39e25898992c5d01be92039ee2850f17e71506ded0d6b25677441eae249f8e225c68dd15a6354dca54006383")

def run_dpp_qr_code_auth_unicast(dev, apdev, curve, netrole=None, key=None,
                                 require_conf_success=False, init_extra=None,
                                 require_conf_failure=False,
                                 configurator=False, conf_curve=None,
                                 conf=None, qr=None, stop_responder=True):
    brainpool = (curve and "brainpool" in curve) or \
        (conf_curve and "brainpool" in conf_curve)
    check_dpp_capab(dev[0], brainpool)
    check_dpp_capab(dev[1], brainpool)
    if configurator:
        conf_id = dev[1].dpp_configurator_add(curve=conf_curve)
    else:
        conf_id = None

    if qr == "mutual":
        logger.info("dev1 displays QR Code and dev0 scans it")
        id1 = dev[1].dpp_bootstrap_gen(chan="81/1", mac=True, curve=curve)
        uri1 = dev[1].request("DPP_BOOTSTRAP_GET_URI %d" % id1)
        id1c = dev[0].dpp_qr_code(uri1)
    else:
        id1 = None

    logger.info("dev0 displays QR Code")
    id0 = dev[0].dpp_bootstrap_gen(chan="81/1", mac=True, curve=curve, key=key)
    uri0 = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id0)

    logger.info("dev1 scans QR Code and initiates DPP Authentication")
    dev[0].dpp_listen(2412, netrole=netrole, qr=qr)
    dev[1].dpp_auth_init(uri=uri0, extra=init_extra, configurator=conf_id,
                         conf=conf, own=id1)
    wait_auth_success(dev[0], dev[1], configurator=dev[1], enrollee=dev[0],
                      allow_enrollee_failure=True,
                      allow_configurator_failure=not require_conf_success,
                      require_configurator_failure=require_conf_failure,
                      stop_responder=stop_responder)

def test_dpp_qr_code_auth_mutual(dev, apdev):
    """DPP QR Code and authentication exchange (mutual)"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    logger.info("dev0 displays QR Code")
    id0 = dev[0].dpp_bootstrap_gen(chan="81/1", mac=True)
    uri0 = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id0)

    logger.info("dev1 displays QR Code")
    id1b = dev[1].dpp_bootstrap_gen(chan="81/1", mac=True)
    uri1b = dev[1].request("DPP_BOOTSTRAP_GET_URI %d" % id1b)

    logger.info("dev0 scans QR Code")
    id0b = dev[0].dpp_qr_code(uri1b)

    logger.info("dev1 scans QR Code and initiates DPP Authentication")
    dev[0].dpp_listen(2412)
    dev[1].dpp_auth_init(uri=uri0, own=id1b)

    ev = dev[1].wait_event(["DPP-AUTH-DIRECTION"], timeout=5)
    if ev is None:
        raise Exception("DPP authentication direction not indicated (Initiator)")
    if "mutual=1" not in ev:
        raise Exception("Mutual authentication not used")

    wait_auth_success(dev[0], dev[1], stop_responder=True)

def test_dpp_qr_code_auth_mutual2(dev, apdev):
    """DPP QR Code and authentication exchange (mutual2)"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    logger.info("dev0 displays QR Code")
    id0 = dev[0].dpp_bootstrap_gen(chan="81/1", mac=True)
    uri0 = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id0)

    logger.info("dev1 displays QR Code")
    id1b = dev[1].dpp_bootstrap_gen(chan="81/1", mac=True)
    uri1b = dev[1].request("DPP_BOOTSTRAP_GET_URI %d" % id1b)

    logger.info("dev1 scans QR Code and initiates DPP Authentication")
    dev[0].dpp_listen(2412, qr="mutual")
    dev[1].dpp_auth_init(uri=uri0, own=id1b)

    ev = dev[1].wait_event(["DPP-RESPONSE-PENDING"], timeout=5)
    if ev is None:
        raise Exception("Pending response not reported")
    ev = dev[0].wait_event(["DPP-SCAN-PEER-QR-CODE"], timeout=5)
    if ev is None:
        raise Exception("QR Code scan for mutual authentication not requested")

    logger.info("dev0 scans QR Code")
    id0b = dev[0].dpp_qr_code(uri1b)

    ev = dev[1].wait_event(["DPP-AUTH-DIRECTION"], timeout=5)
    if ev is None:
        raise Exception("DPP authentication direction not indicated (Initiator)")
    if "mutual=1" not in ev:
        raise Exception("Mutual authentication not used")

    wait_auth_success(dev[0], dev[1], stop_responder=True)

def test_dpp_qr_code_auth_mutual_p_256(dev, apdev):
    """DPP QR Code and authentication exchange (mutual, autogen P-256)"""
    run_dpp_qr_code_auth_mutual(dev, apdev, "P-256")

def test_dpp_qr_code_auth_mutual_p_384(dev, apdev):
    """DPP QR Code and authentication exchange (mutual, autogen P-384)"""
    run_dpp_qr_code_auth_mutual(dev, apdev, "P-384")

def test_dpp_qr_code_auth_mutual_p_521(dev, apdev):
    """DPP QR Code and authentication exchange (mutual, autogen P-521)"""
    run_dpp_qr_code_auth_mutual(dev, apdev, "P-521")

def test_dpp_qr_code_auth_mutual_bp_256(dev, apdev):
    """DPP QR Code and authentication exchange (mutual, autogen BP-256)"""
    run_dpp_qr_code_auth_mutual(dev, apdev, "BP-256")

def test_dpp_qr_code_auth_mutual_bp_384(dev, apdev):
    """DPP QR Code and authentication exchange (mutual, autogen BP-384)"""
    run_dpp_qr_code_auth_mutual(dev, apdev, "BP-384")

def test_dpp_qr_code_auth_mutual_bp_512(dev, apdev):
    """DPP QR Code and authentication exchange (mutual, autogen BP-512)"""
    run_dpp_qr_code_auth_mutual(dev, apdev, "BP-512")

def run_dpp_qr_code_auth_mutual(dev, apdev, curve):
    check_dpp_capab(dev[0], curve and "BP-" in curve)
    check_dpp_capab(dev[1], curve and "BP-" in curve)
    logger.info("dev0 displays QR Code")
    id0 = dev[0].dpp_bootstrap_gen(chan="81/1", mac=True, curve=curve)
    uri0 = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id0)
    logger.info("dev1 scans QR Code and initiates DPP Authentication")
    dev[0].dpp_listen(2412, qr="mutual")
    dev[1].dpp_auth_init(uri=uri0)

    ev = dev[1].wait_event(["DPP-RESPONSE-PENDING"], timeout=5)
    if ev is None:
        raise Exception("Pending response not reported")
    uri = ev.split(' ')[1]

    ev = dev[0].wait_event(["DPP-SCAN-PEER-QR-CODE"], timeout=5)
    if ev is None:
        raise Exception("QR Code scan for mutual authentication not requested")

    logger.info("dev0 scans QR Code")
    dev[0].dpp_qr_code(uri)

    ev = dev[1].wait_event(["DPP-AUTH-DIRECTION"], timeout=5)
    if ev is None:
        raise Exception("DPP authentication direction not indicated (Initiator)")
    if "mutual=1" not in ev:
        raise Exception("Mutual authentication not used")

    wait_auth_success(dev[0], dev[1], stop_responder=True)

def test_dpp_auth_resp_retries(dev, apdev):
    """DPP Authentication Response retries"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    dev[0].set("dpp_resp_max_tries", "3")
    dev[0].set("dpp_resp_retry_time", "100")

    logger.info("dev0 displays QR Code")
    id0 = dev[0].dpp_bootstrap_gen(chan="81/1", mac=True)
    uri0 = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id0)
    logger.info("dev1 displays QR Code")
    id1b = dev[1].dpp_bootstrap_gen(chan="81/1", mac=True)
    uri1b = dev[1].request("DPP_BOOTSTRAP_GET_URI %d" % id1b)
    logger.info("dev1 scans QR Code and initiates DPP Authentication")
    dev[0].dpp_listen(2412, qr="mutual")
    dev[1].dpp_auth_init(uri=uri0, own=id1b)

    ev = dev[1].wait_event(["DPP-RESPONSE-PENDING"], timeout=5)
    if ev is None:
        raise Exception("Pending response not reported")
    ev = dev[0].wait_event(["DPP-SCAN-PEER-QR-CODE"], timeout=5)
    if ev is None:
        raise Exception("QR Code scan for mutual authentication not requested")

    # Stop Initiator from listening to frames to force retransmission of the
    # DPP Authentication Response frame with Status=0
    dev[1].request("DPP_STOP_LISTEN")

    dev[1].dump_monitor()
    dev[0].dump_monitor()

    logger.info("dev0 scans QR Code")
    id0b = dev[0].dpp_qr_code(uri1b)

    ev = dev[0].wait_event(["DPP-TX "], timeout=5)
    if ev is None or "type=1" not in ev:
        raise Exception("DPP Authentication Response not sent")
    ev = dev[0].wait_event(["DPP-TX-STATUS"], timeout=5)
    if ev is None:
        raise Exception("TX status for DPP Authentication Response not reported")
    if "result=no-ACK" not in ev:
        raise Exception("Unexpected TX status for Authentication Response: " + ev)

    ev = dev[0].wait_event(["DPP-TX "], timeout=15)
    if ev is None or "type=1" not in ev:
        raise Exception("DPP Authentication Response retransmission not sent")

def test_dpp_qr_code_auth_mutual_not_used(dev, apdev):
    """DPP QR Code and authentication exchange (mutual not used)"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    logger.info("dev0 displays QR Code")
    id0 = dev[0].dpp_bootstrap_gen(chan="81/1", mac=True)
    uri0 = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id0)
    logger.info("dev1 displays QR Code")
    id1b = dev[1].dpp_bootstrap_gen(chan="81/1", mac=True)
    uri1b = dev[1].request("DPP_BOOTSTRAP_GET_URI %d" % id1b)
    logger.info("dev0 does not scan QR Code")
    logger.info("dev1 scans QR Code and initiates DPP Authentication")
    dev[0].dpp_listen(2412)
    dev[1].dpp_auth_init(uri=uri0, own=id1b)

    ev = dev[1].wait_event(["DPP-AUTH-DIRECTION"], timeout=5)
    if ev is None:
        raise Exception("DPP authentication direction not indicated (Initiator)")
    if "mutual=0" not in ev:
        raise Exception("Mutual authentication not used")

    wait_auth_success(dev[0], dev[1], stop_responder=True)

def test_dpp_qr_code_auth_mutual_curve_mismatch(dev, apdev):
    """DPP QR Code and authentication exchange (mutual/mismatch)"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    logger.info("dev0 displays QR Code")
    id0 = dev[0].dpp_bootstrap_gen(chan="81/1", mac=True)
    uri0 = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id0)
    logger.info("dev1 displays QR Code")
    id1b = dev[1].dpp_bootstrap_gen(chan="81/1", mac=True, curve="secp384r1")
    uri1b = dev[1].request("DPP_BOOTSTRAP_GET_URI %d" % id1b)
    logger.info("dev0 scans QR Code")
    id0b = dev[0].dpp_qr_code(uri1b)
    logger.info("dev1 scans QR Code")
    dev[1].dpp_auth_init(uri=uri0, own=id1b, expect_fail=True)

def test_dpp_qr_code_auth_hostapd_mutual2(dev, apdev):
    """DPP QR Code and authentication exchange (hostapd mutual2)"""
    check_dpp_capab(dev[0])
    hapd = hostapd.add_ap(apdev[0], {"ssid": "unconfigured"})
    check_dpp_capab(hapd)
    logger.info("AP displays QR Code")
    id_h = hapd.dpp_bootstrap_gen(chan="81/1", mac=True)
    uri_h = hapd.request("DPP_BOOTSTRAP_GET_URI %d" % id_h)
    logger.info("dev0 displays QR Code")
    id0b = dev[0].dpp_bootstrap_gen(chan="81/1", mac=True)
    uri0 = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id0b)
    logger.info("dev0 scans QR Code and initiates DPP Authentication")
    hapd.dpp_listen(2412, qr="mutual")
    dev[0].dpp_auth_init(uri=uri_h, own=id0b)

    ev = dev[0].wait_event(["DPP-RESPONSE-PENDING"], timeout=5)
    if ev is None:
        raise Exception("Pending response not reported")
    ev = hapd.wait_event(["DPP-SCAN-PEER-QR-CODE"], timeout=5)
    if ev is None:
        raise Exception("QR Code scan for mutual authentication not requested")

    logger.info("AP scans QR Code")
    hapd.dpp_qr_code(uri0)

    wait_auth_success(hapd, dev[0], stop_responder=True)

def test_dpp_qr_code_listen_continue(dev, apdev):
    """DPP QR Code and listen operation needing continuation"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    logger.info("dev0 displays QR Code")
    id0 = dev[0].dpp_bootstrap_gen(chan="81/1", mac=True)
    uri0 = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id0)
    dev[0].dpp_listen(2412)
    logger.info("Wait for listen to expire and get restarted")
    time.sleep(5.5)
    logger.info("dev1 scans QR Code and initiates DPP Authentication")
    dev[1].dpp_auth_init(uri=uri0)
    wait_auth_success(dev[0], dev[1], stop_responder=True)

def test_dpp_qr_code_auth_initiator_enrollee(dev, apdev):
    """DPP QR Code and authentication exchange (Initiator in Enrollee role)"""
    try:
        run_dpp_qr_code_auth_initiator_enrollee(dev, apdev)
    finally:
        dev[0].set("gas_address3", "0")
        dev[1].set("gas_address3", "0")

def run_dpp_qr_code_auth_initiator_enrollee(dev, apdev):
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    dev[0].request("SET gas_address3 1")
    dev[1].request("SET gas_address3 1")
    logger.info("dev0 displays QR Code")
    id0 = dev[0].dpp_bootstrap_gen(chan="81/1", mac=True)
    uri0 = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id0)
    logger.info("dev1 scans QR Code and initiates DPP Authentication")
    dev[0].dpp_listen(2412)
    dev[1].dpp_auth_init(uri=uri0, role="enrollee")
    wait_auth_success(dev[0], dev[1], configurator=dev[0], enrollee=dev[1],
                      allow_enrollee_failure=True, stop_responder=True)

def test_dpp_qr_code_auth_initiator_either_1(dev, apdev):
    """DPP QR Code and authentication exchange (Initiator in either role)"""
    run_dpp_qr_code_auth_initiator_either(dev, apdev, None, dev[1], dev[0])

def test_dpp_qr_code_auth_initiator_either_2(dev, apdev):
    """DPP QR Code and authentication exchange (Initiator in either role)"""
    run_dpp_qr_code_auth_initiator_either(dev, apdev, "enrollee",
                                          dev[1], dev[0])

def test_dpp_qr_code_auth_initiator_either_3(dev, apdev):
    """DPP QR Code and authentication exchange (Initiator in either role)"""
    run_dpp_qr_code_auth_initiator_either(dev, apdev, "configurator",
                                          dev[0], dev[1])

def run_dpp_qr_code_auth_initiator_either(dev, apdev, resp_role,
                                          conf_dev, enrollee_dev):
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    logger.info("dev0 displays QR Code")
    id0 = dev[0].dpp_bootstrap_gen(chan="81/1", mac=True)
    uri0 = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id0)
    logger.info("dev1 scans QR Code and initiates DPP Authentication")
    dev[0].dpp_listen(2412, role=resp_role)
    dev[1].dpp_auth_init(uri=uri0, role="either")
    wait_auth_success(dev[0], dev[1], configurator=conf_dev,
                      enrollee=enrollee_dev, allow_enrollee_failure=True,
                      stop_responder=True)

def run_init_incompatible_roles(dev, role="enrollee"):
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    logger.info("dev0 displays QR Code")
    id0 = dev[0].dpp_bootstrap_gen(chan="81/1", mac=True)
    uri0 = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id0)

    logger.info("dev1 scans QR Code")
    id1 = dev[1].dpp_qr_code(uri0)

    logger.info("dev1 initiates DPP Authentication")
    dev[0].dpp_listen(2412, role=role)
    return id1

def test_dpp_qr_code_auth_incompatible_roles(dev, apdev):
    """DPP QR Code and authentication exchange (incompatible roles)"""
    id1 = run_init_incompatible_roles(dev)
    dev[1].dpp_auth_init(peer=id1, role="enrollee")
    ev = dev[1].wait_event(["DPP-NOT-COMPATIBLE"], timeout=5)
    if ev is None:
        raise Exception("DPP-NOT-COMPATIBLE event on initiator timed out")
    ev = dev[0].wait_event(["DPP-NOT-COMPATIBLE"], timeout=1)
    if ev is None:
        raise Exception("DPP-NOT-COMPATIBLE event on responder timed out")
    dev[1].dpp_auth_init(peer=id1, role="configurator")
    wait_auth_success(dev[0], dev[1], stop_responder=True)

def test_dpp_qr_code_auth_incompatible_roles2(dev, apdev):
    """DPP QR Code and authentication exchange (incompatible roles 2)"""
    id1 = run_init_incompatible_roles(dev, role="configurator")
    dev[1].dpp_auth_init(peer=id1, role="configurator")
    ev = dev[1].wait_event(["DPP-NOT-COMPATIBLE"], timeout=5)
    if ev is None:
        raise Exception("DPP-NOT-COMPATIBLE event on initiator timed out")
    ev = dev[0].wait_event(["DPP-NOT-COMPATIBLE"], timeout=1)
    if ev is None:
        raise Exception("DPP-NOT-COMPATIBLE event on responder timed out")

def test_dpp_qr_code_auth_incompatible_roles_failure(dev, apdev):
    """DPP QR Code and authentication exchange (incompatible roles failure)"""
    id1 = run_init_incompatible_roles(dev, role="configurator")
    with alloc_fail(dev[0], 1, "dpp_auth_build_resp_status"):
        dev[1].dpp_auth_init(peer=id1, role="configurator")
        ev = dev[0].wait_event(["DPP-NOT-COMPATIBLE"], timeout=1)
        if ev is None:
            raise Exception("DPP-NOT-COMPATIBLE event on responder timed out")

def test_dpp_qr_code_auth_incompatible_roles_failure2(dev, apdev):
    """DPP QR Code and authentication exchange (incompatible roles failure 2)"""
    id1 = run_init_incompatible_roles(dev, role="configurator")
    with alloc_fail(dev[1], 1, "dpp_auth_resp_rx_status"):
        dev[1].dpp_auth_init(peer=id1, role="configurator")
        wait_fail_trigger(dev[1], "GET_ALLOC_FAIL")

def test_dpp_qr_code_auth_incompatible_roles_failure3(dev, apdev):
    """DPP QR Code and authentication exchange (incompatible roles failure 3)"""
    id1 = run_init_incompatible_roles(dev, role="configurator")
    with fail_test(dev[1], 1, "dpp_auth_resp_rx_status"):
        dev[1].dpp_auth_init(peer=id1, role="configurator")
        wait_dpp_fail(dev[1], "AES-SIV decryption failed")

def test_dpp_qr_code_auth_neg_chan(dev, apdev):
    """DPP QR Code and authentication exchange with requested different channel"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    conf_id = dev[1].dpp_configurator_add()
    logger.info("dev0 displays QR Code")
    id0 = dev[0].dpp_bootstrap_gen(chan="81/1", mac=True)
    uri0 = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id0)
    logger.info("dev1 scans QR Code and initiates DPP Authentication")
    dev[0].dpp_listen(2412)
    dev[1].dpp_auth_init(uri=uri0, conf="sta-dpp", neg_freq=2462,
                         configurator=conf_id)

    ev = dev[1].wait_event(["DPP-TX "], timeout=5)
    if ev is None:
        raise Exception("DPP Authentication Request not sent")
    if "freq=2412 type=0" not in ev:
        raise Exception("Unexpected TX data for Authentication Request: " + ev)

    ev = dev[0].wait_event(["DPP-RX"], timeout=5)
    if ev is None:
        raise Exception("DPP Authentication Request not received")
    if "freq=2412 type=0" not in ev:
        raise Exception("Unexpected RX data for Authentication Request: " + ev)

    ev = dev[1].wait_event(["DPP-TX-STATUS"], timeout=5)
    if ev is None:
        raise Exception("TX status for DPP Authentication Request not reported")
    if "freq=2412 result=SUCCESS" not in ev:
        raise Exception("Unexpected TX status for Authentication Request: " + ev)

    ev = dev[0].wait_event(["DPP-TX "], timeout=5)
    if ev is None:
        raise Exception("DPP Authentication Response not sent")
    if "freq=2462 type=1" not in ev:
        raise Exception("Unexpected TX data for Authentication Response: " + ev)

    ev = dev[1].wait_event(["DPP-RX"], timeout=5)
    if ev is None:
        raise Exception("DPP Authentication Response not received")
    if "freq=2462 type=1" not in ev:
        raise Exception("Unexpected RX data for Authentication Response: " + ev)

    ev = dev[0].wait_event(["DPP-TX-STATUS"], timeout=5)
    if ev is None:
        raise Exception("TX status for DPP Authentication Response not reported")
    if "freq=2462 result=SUCCESS" not in ev:
        raise Exception("Unexpected TX status for Authentication Response: " + ev)

    ev = dev[1].wait_event(["DPP-TX "], timeout=5)
    if ev is None:
        raise Exception("DPP Authentication Confirm not sent")
    if "freq=2462 type=2" not in ev:
        raise Exception("Unexpected TX data for Authentication Confirm: " + ev)

    ev = dev[0].wait_event(["DPP-RX"], timeout=5)
    if ev is None:
        raise Exception("DPP Authentication Confirm not received")
    if "freq=2462 type=2" not in ev:
        raise Exception("Unexpected RX data for Authentication Confirm: " + ev)

    ev = dev[1].wait_event(["DPP-TX-STATUS"], timeout=5)
    if ev is None:
        raise Exception("TX status for DPP Authentication Confirm not reported")
    if "freq=2462 result=SUCCESS" not in ev:
        raise Exception("Unexpected TX status for Authentication Confirm: " + ev)

    wait_auth_success(dev[0], dev[1], configurator=dev[1], enrollee=dev[0],
                      stop_responder=True)

def test_dpp_config_legacy(dev, apdev):
    """DPP Config Object for legacy network using passphrase"""
    check_dpp_capab(dev[1])
    conf = '{"wi-fi_tech":"infra", "discovery":{"ssid":"test"},"cred":{"akm":"psk","pass":"secret passphrase"}}'
    dev[1].set("dpp_config_obj_override", conf)
    run_dpp_qr_code_auth_unicast(dev, apdev, "prime256v1",
                                 require_conf_success=True)

def test_dpp_config_legacy_psk_hex(dev, apdev):
    """DPP Config Object for legacy network using PSK"""
    check_dpp_capab(dev[1])
    conf = '{"wi-fi_tech":"infra", "discovery":{"ssid":"test"},"cred":{"akm":"psk","psk_hex":"' + 32*"12" + '"}}'
    dev[1].set("dpp_config_obj_override", conf)
    run_dpp_qr_code_auth_unicast(dev, apdev, "prime256v1",
                                 require_conf_success=True)

def test_dpp_config_fragmentation(dev, apdev):
    """DPP Config Object for legacy network requiring fragmentation"""
    check_dpp_capab(dev[1])
    conf = '{"wi-fi_tech":"infra", "discovery":{"ssid":"test"},"cred":{"akm":"psk","pass":"secret passphrase"}}' + 3000*' '
    dev[1].set("dpp_config_obj_override", conf)
    run_dpp_qr_code_auth_unicast(dev, apdev, "prime256v1",
                                 require_conf_success=True)

def test_dpp_config_legacy_gen(dev, apdev):
    """Generate DPP Config Object for legacy network"""
    run_dpp_qr_code_auth_unicast(dev, apdev, "prime256v1",
                                 init_extra="conf=sta-psk pass=%s" % binascii.hexlify(b"passphrase").decode(),
                                 require_conf_success=True)

def test_dpp_config_legacy_gen_psk(dev, apdev):
    """Generate DPP Config Object for legacy network (PSK)"""
    run_dpp_qr_code_auth_unicast(dev, apdev, "prime256v1",
                                 init_extra="conf=sta-psk psk=0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",
                                 require_conf_success=True)

def test_dpp_config_dpp_gen_prime256v1(dev, apdev):
    """Generate DPP Config Object for DPP network (P-256)"""
    run_dpp_qr_code_auth_unicast(dev, apdev, "prime256v1",
                                 init_extra="conf=sta-dpp",
                                 require_conf_success=True,
                                 configurator=True)

def test_dpp_config_dpp_gen_secp384r1(dev, apdev):
    """Generate DPP Config Object for DPP network (P-384)"""
    run_dpp_qr_code_auth_unicast(dev, apdev, "secp384r1",
                                 init_extra="conf=sta-dpp",
                                 require_conf_success=True,
                                 configurator=True)

def test_dpp_config_dpp_gen_secp521r1(dev, apdev):
    """Generate DPP Config Object for DPP network (P-521)"""
    run_dpp_qr_code_auth_unicast(dev, apdev, "secp521r1",
                                 init_extra="conf=sta-dpp",
                                 require_conf_success=True,
                                 configurator=True)

def test_dpp_config_dpp_gen_prime256v1_prime256v1(dev, apdev):
    """Generate DPP Config Object for DPP network (P-256 + P-256)"""
    run_dpp_qr_code_auth_unicast(dev, apdev, "prime256v1",
                                 init_extra="conf=sta-dpp",
                                 require_conf_success=True,
                                 configurator=True,
                                 conf_curve="prime256v1")

def test_dpp_config_dpp_gen_prime256v1_secp384r1(dev, apdev):
    """Generate DPP Config Object for DPP network (P-256 + P-384)"""
    run_dpp_qr_code_auth_unicast(dev, apdev, "prime256v1",
                                 init_extra="conf=sta-dpp",
                                 require_conf_success=True,
                                 configurator=True,
                                 conf_curve="secp384r1")

def test_dpp_config_dpp_gen_prime256v1_secp521r1(dev, apdev):
    """Generate DPP Config Object for DPP network (P-256 + P-521)"""
    run_dpp_qr_code_auth_unicast(dev, apdev, "prime256v1",
                                 init_extra="conf=sta-dpp",
                                 require_conf_success=True,
                                 configurator=True,
                                 conf_curve="secp521r1")

def test_dpp_config_dpp_gen_secp384r1_prime256v1(dev, apdev):
    """Generate DPP Config Object for DPP network (P-384 + P-256)"""
    run_dpp_qr_code_auth_unicast(dev, apdev, "secp384r1",
                                 init_extra="conf=sta-dpp",
                                 require_conf_success=True,
                                 configurator=True,
                                 conf_curve="prime256v1")

def test_dpp_config_dpp_gen_secp384r1_secp384r1(dev, apdev):
    """Generate DPP Config Object for DPP network (P-384 + P-384)"""
    run_dpp_qr_code_auth_unicast(dev, apdev, "secp384r1",
                                 init_extra="conf=sta-dpp",
                                 require_conf_success=True,
                                 configurator=True,
                                 conf_curve="secp384r1")

def test_dpp_config_dpp_gen_secp384r1_secp521r1(dev, apdev):
    """Generate DPP Config Object for DPP network (P-384 + P-521)"""
    run_dpp_qr_code_auth_unicast(dev, apdev, "secp384r1",
                                 init_extra="conf=sta-dpp",
                                 require_conf_success=True,
                                 configurator=True,
                                 conf_curve="secp521r1")

def test_dpp_config_dpp_gen_secp521r1_prime256v1(dev, apdev):
    """Generate DPP Config Object for DPP network (P-521 + P-256)"""
    run_dpp_qr_code_auth_unicast(dev, apdev, "secp521r1",
                                 init_extra="conf=sta-dpp",
                                 require_conf_success=True,
                                 configurator=True,
                                 conf_curve="prime256v1")

def test_dpp_config_dpp_gen_secp521r1_secp384r1(dev, apdev):
    """Generate DPP Config Object for DPP network (P-521 + P-384)"""
    run_dpp_qr_code_auth_unicast(dev, apdev, "secp521r1",
                                 init_extra="conf=sta-dpp",
                                 require_conf_success=True,
                                 configurator=True,
                                 conf_curve="secp384r1")

def test_dpp_config_dpp_gen_secp521r1_secp521r1(dev, apdev):
    """Generate DPP Config Object for DPP network (P-521 + P-521)"""
    run_dpp_qr_code_auth_unicast(dev, apdev, "secp521r1",
                                 init_extra="conf=sta-dpp",
                                 require_conf_success=True,
                                 configurator=True,
                                 conf_curve="secp521r1")

def test_dpp_config_dpp_gen_expiry(dev, apdev):
    """Generate DPP Config Object for DPP network with expiry value"""
    run_dpp_qr_code_auth_unicast(dev, apdev, "prime256v1",
                                 init_extra="conf=sta-dpp expiry=%d" % (time.time() + 1000),
                                 require_conf_success=True,
                                 configurator=True)

def test_dpp_config_dpp_gen_expired_key(dev, apdev):
    """Generate DPP Config Object for DPP network with expiry value"""
    run_dpp_qr_code_auth_unicast(dev, apdev, "prime256v1",
                                 init_extra="conf=sta-dpp expiry=%d" % (time.time() - 10),
                                 require_conf_failure=True,
                                 configurator=True)

def test_dpp_config_dpp_override_prime256v1(dev, apdev):
    """DPP Config Object override (P-256)"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    conf = '{"wi-fi_tech":"infra","discovery":{"ssid":"test"},"cred":{"akm":"dpp","signedConnector":"eyJ0eXAiOiJkcHBDb24iLCJraWQiOiJUbkdLaklsTlphYXRyRUFZcmJiamlCNjdyamtMX0FHVldYTzZxOWhESktVIiwiYWxnIjoiRVMyNTYifQ.eyJncm91cHMiOlt7Imdyb3VwSWQiOiIqIiwibmV0Um9sZSI6InN0YSJ9XSwibmV0QWNjZXNzS2V5Ijp7Imt0eSI6IkVDIiwiY3J2IjoiUC0yNTYiLCJ4IjoiYVRGNEpFR0lQS1NaMFh2OXpkQ01qbS10bjVYcE1zWUlWWjl3eVNBejFnSSIsInkiOiJRR2NIV0FfNnJiVTlYRFhBenRvWC1NNVEzc3VUbk1hcUVoVUx0bjdTU1h3In19._sm6YswxMf6hJLVTyYoU1uYUeY2VVkUNjrzjSiEhY42StD_RWowStEE-9CRsdCvLmsTptZ72_g40vTFwdId20A","csign":{"kty":"EC","crv":"P-256","x":"W4-Y5N1Pkos3UWb9A5qme0KUYRtY3CVUpekx_MapZ9s","y":"Et-M4NSF4NGjvh2VCh4B1sJ9eSCZ4RNzP2DBdP137VE","kid":"TnGKjIlNZaatrEAYrbbjiB67rjkL_AGVWXO6q9hDJKU"}}}'
    dev[0].set("dpp_ignore_netaccesskey_mismatch", "1")
    dev[1].set("dpp_config_obj_override", conf)
    run_dpp_qr_code_auth_unicast(dev, apdev, "prime256v1",
                                 require_conf_success=True)

def test_dpp_config_dpp_override_secp384r1(dev, apdev):
    """DPP Config Object override (P-384)"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    conf = '{"wi-fi_tech":"infra","discovery":{"ssid":"test"},"cred":{"akm":"dpp","signedConnector":"eyJ0eXAiOiJkcHBDb24iLCJraWQiOiJabi1iMndjbjRLM2pGQklkYmhGZkpVTHJTXzdESS0yMWxFQi02R3gxNjl3IiwiYWxnIjoiRVMzODQifQ.eyJncm91cHMiOlt7Imdyb3VwSWQiOiIqIiwibmV0Um9sZSI6InN0YSJ9XSwibmV0QWNjZXNzS2V5Ijp7Imt0eSI6IkVDIiwiY3J2IjoiUC0zODQiLCJ4IjoickdrSGg1UUZsOUtfWjdqYUZkVVhmbThoY1RTRjM1b25Xb1NIRXVsbVNzWW9oX1RXZGpoRjhiVGdiS0ZRN2tBViIsInkiOiJBbU1QVDA5VmFENWpGdzMwTUFKQlp2VkZXeGNlVVlKLXR5blQ0bVJ5N0xOZWxhZ0dEWHpfOExaRlpOU2FaNUdLIn19.Yn_F7m-bbOQ5PlaYQJ9-1qsuqYQ6V-rAv8nWw1COKiCYwwbt3WFBJ8DljY0dPrlg5CHJC4saXwkytpI-CpELW1yUdzYb4Lrun07d20Eo_g10ICyOl5sqQCAUElKMe_Xr","csign":{"kty":"EC","crv":"P-384","x":"dmTyXXiPV2Y8a01fujL-jo08gvzyby23XmzOtzjAiujKQZZgPJsbhfEKrZDlc6ey","y":"H5Z0av5c7bqInxYb2_OOJdNiMhVf3zlcULR0516ZZitOY4U31KhL4wl4KGV7g2XW","kid":"Zn-b2wcn4K3jFBIdbhFfJULrS_7DI-21lEB-6Gx169w"}}}'
    dev[0].set("dpp_ignore_netaccesskey_mismatch", "1")
    dev[1].set("dpp_config_obj_override", conf)
    run_dpp_qr_code_auth_unicast(dev, apdev, "secp384r1",
                                 require_conf_success=True)

def test_dpp_config_dpp_override_secp521r1(dev, apdev):
    """DPP Config Object override (P-521)"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    conf = '{"wi-fi_tech":"infra","discovery":{"ssid":"test"},"cred":{"akm":"dpp","signedConnector":"eyJ0eXAiOiJkcHBDb24iLCJraWQiOiJMZkhKY3hnV2ZKcG1uS2IwenZRT0F2VDB2b0ZKc0JjZnBmYzgxY3Y5ZXFnIiwiYWxnIjoiRVM1MTIifQ.eyJncm91cHMiOlt7Imdyb3VwSWQiOiIqIiwibmV0Um9sZSI6InN0YSJ9XSwibmV0QWNjZXNzS2V5Ijp7Imt0eSI6IkVDIiwiY3J2IjoiUC01MjEiLCJ4IjoiQVJlUFBrMFNISkRRR2NWbnlmM3lfbTlaQllHNjFJeElIbDN1NkdwRHVhMkU1WVd4TE1BSUtMMnZuUGtlSGFVRXljRmZaZlpYZ2JlNkViUUxMVkRVUm1VUSIsInkiOiJBWUtaYlNwUkFFNjJVYm9YZ2c1ZWRBVENzbEpzTlpwcm9RR1dUcW9Md04weXkzQkVoT3ZRZmZrOWhaR2lKZ295TzFobXFRRVRrS0pXb2tIYTBCQUpLSGZtIn19.ACEZLyPk13cM_OFScpLoCElQ2t1sxq5z2d_W_3_QslTQQe5SFiH_o8ycL4632YLAH4RV0gZcMKKRMtZdHgBYHjkzASDqgY-_aYN2SBmpfl8hw0YdDlUJWX3DJf-ofqNAlTbnGmhpSg69cEAhFn41Xgvx2MdwYcPVncxxESVOtWl5zNLK","csign":{"kty":"EC","crv":"P-521","x":"ADiOI_YJOAipEXHB-SpGl4KqokX8m8h3BVYCc8dgiwssZ061-nIIY3O1SIO6Re4Jjfy53RPgzDG6jitOgOGLtzZs","y":"AZKggKaQi0ExutSpJAU3-lqDV03sBQLA9C7KabfWoAn8qD6Vk4jU0WAJdt-wBBTF9o1nVuiqS2OxMVYrxN4lOz79","kid":"LfHJcxgWfJpmnKb0zvQOAvT0voFJsBcfpfc81cv9eqg"}}}'
    dev[0].set("dpp_ignore_netaccesskey_mismatch", "1")
    dev[1].set("dpp_config_obj_override", conf)
    run_dpp_qr_code_auth_unicast(dev, apdev, "secp521r1",
                                 require_conf_success=True)

def test_dpp_config_override_objects(dev, apdev):
    """Generate DPP Config Object and override objects)"""
    check_dpp_capab(dev[1])
    discovery = '{\n"ssid":"mywifi"\n}'
    groups = '[\n  {"groupId":"home","netRole":"sta"},\n  {"groupId":"cottage","netRole":"sta"}\n]'
    dev[1].set("dpp_discovery_override", discovery)
    dev[1].set("dpp_groups_override", groups)
    run_dpp_qr_code_auth_unicast(dev, apdev, "prime256v1",
                                 init_extra="conf=sta-dpp",
                                 require_conf_success=True,
                                 configurator=True)

def build_conf_obj(kty="EC", crv="P-256",
                   x="W4-Y5N1Pkos3UWb9A5qme0KUYRtY3CVUpekx_MapZ9s",
                   y="Et-M4NSF4NGjvh2VCh4B1sJ9eSCZ4RNzP2DBdP137VE",
                   kid="TnGKjIlNZaatrEAYrbbjiB67rjkL_AGVWXO6q9hDJKU",
                   prot_hdr='{"typ":"dppCon","kid":"TnGKjIlNZaatrEAYrbbjiB67rjkL_AGVWXO6q9hDJKU","alg":"ES256"}',
                   signed_connector=None,
                   no_signed_connector=False,
                   csign=True):
    conf = '{"wi-fi_tech":"infra","discovery":{"ssid":"test"},"cred":{'
    conf += '"akm":"dpp",'

    if signed_connector:
        conn = signed_connector
        conf += '"signedConnector":"%s",' % conn
    elif not no_signed_connector:
        payload = '{"groups":[{"groupId":"*","netRole":"sta"}],"netAccessKey":{"kty":"EC","crv":"P-256","x":"aTF4JEGIPKSZ0Xv9zdCMjm-tn5XpMsYIVZ9wySAz1gI","y":"QGcHWA_6rbU9XDXAztoX-M5Q3suTnMaqEhULtn7SSXw"}}'
        sign = "_sm6YswxMf6hJLVTyYoU1uYUeY2VVkUNjrzjSiEhY42StD_RWowStEE-9CRsdCvLmsTptZ72_g40vTFwdId20A"
        conn = base64.urlsafe_b64encode(prot_hdr.encode()).decode().rstrip('=') + '.'
        conn += base64.urlsafe_b64encode(payload.encode()).decode().rstrip('=') + '.'
        conn += sign
        conf += '"signedConnector":"%s",' % conn

    if csign:
        conf += '"csign":{'
        if kty:
            conf += '"kty":"%s",' % kty
        if crv:
            conf += '"crv":"%s",' % crv
        if x:
            conf += '"x":"%s",' % x
        if y:
            conf += '"y":"%s",' % y
        if kid:
            conf += '"kid":"%s"' % kid
        conf = conf.rstrip(',')
        conf += '}'
    else:
        conf = conf.rstrip(',')

    conf += '}}'

    return conf

def run_dpp_config_error(dev, apdev, conf,
                         skip_net_access_key_mismatch=True,
                         conf_failure=True):
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    if skip_net_access_key_mismatch:
        dev[0].set("dpp_ignore_netaccesskey_mismatch", "1")
    dev[1].set("dpp_config_obj_override", conf)
    run_dpp_qr_code_auth_unicast(dev, apdev, "prime256v1",
                                 require_conf_success=not conf_failure,
                                 require_conf_failure=conf_failure)

def test_dpp_config_jwk_error_no_kty(dev, apdev):
    """DPP Config Object JWK error - no kty"""
    run_dpp_config_error(dev, apdev, build_conf_obj(kty=None))

def test_dpp_config_jwk_error_unexpected_kty(dev, apdev):
    """DPP Config Object JWK error - unexpected kty"""
    run_dpp_config_error(dev, apdev, build_conf_obj(kty="unknown"))

def test_dpp_config_jwk_error_no_crv(dev, apdev):
    """DPP Config Object JWK error - no crv"""
    run_dpp_config_error(dev, apdev, build_conf_obj(crv=None))

def test_dpp_config_jwk_error_unsupported_crv(dev, apdev):
    """DPP Config Object JWK error - unsupported curve"""
    run_dpp_config_error(dev, apdev, build_conf_obj(crv="unsupported"))

def test_dpp_config_jwk_error_no_x(dev, apdev):
    """DPP Config Object JWK error - no x"""
    run_dpp_config_error(dev, apdev, build_conf_obj(x=None))

def test_dpp_config_jwk_error_invalid_x(dev, apdev):
    """DPP Config Object JWK error - invalid x"""
    run_dpp_config_error(dev, apdev, build_conf_obj(x="MTIz"))

def test_dpp_config_jwk_error_no_y(dev, apdev):
    """DPP Config Object JWK error - no y"""
    run_dpp_config_error(dev, apdev, build_conf_obj(y=None))

def test_dpp_config_jwk_error_invalid_y(dev, apdev):
    """DPP Config Object JWK error - invalid y"""
    run_dpp_config_error(dev, apdev, build_conf_obj(y="MTIz"))

def test_dpp_config_jwk_error_invalid_xy(dev, apdev):
    """DPP Config Object JWK error - invalid x,y"""
    conf = build_conf_obj(x="MDEyMzQ1Njc4OWFiY2RlZjAxMjM0NTY3ODlhYmNkZWY",
                          y="MDEyMzQ1Njc4OWFiY2RlZjAxMjM0NTY3ODlhYmNkZWY")
    run_dpp_config_error(dev, apdev, conf)

def test_dpp_config_jwk_error_no_kid(dev, apdev):
    """DPP Config Object JWK error - no kid"""
    # csign kid is optional field, so this results in success
    run_dpp_config_error(dev, apdev, build_conf_obj(kid=None),
                         conf_failure=False)

def test_dpp_config_jws_error_prot_hdr_not_an_object(dev, apdev):
    """DPP Config Object JWS error - protected header not an object"""
    run_dpp_config_error(dev, apdev, build_conf_obj(prot_hdr="1"))

def test_dpp_config_jws_error_prot_hdr_no_typ(dev, apdev):
    """DPP Config Object JWS error - protected header - no typ"""
    prot_hdr = '{"kid":"TnGKjIlNZaatrEAYrbbjiB67rjkL_AGVWXO6q9hDJKU","alg":"ES256"}'
    run_dpp_config_error(dev, apdev, build_conf_obj(prot_hdr=prot_hdr))

def test_dpp_config_jws_error_prot_hdr_unsupported_typ(dev, apdev):
    """DPP Config Object JWS error - protected header - unsupported typ"""
    prot_hdr = '{"typ":"unsupported","kid":"TnGKjIlNZaatrEAYrbbjiB67rjkL_AGVWXO6q9hDJKU","alg":"ES256"}'
    run_dpp_config_error(dev, apdev, build_conf_obj(prot_hdr=prot_hdr))

def test_dpp_config_jws_error_prot_hdr_no_alg(dev, apdev):
    """DPP Config Object JWS error - protected header - no alg"""
    prot_hdr = '{"typ":"dppCon","kid":"TnGKjIlNZaatrEAYrbbjiB67rjkL_AGVWXO6q9hDJKU"}'
    run_dpp_config_error(dev, apdev, build_conf_obj(prot_hdr=prot_hdr))

def test_dpp_config_jws_error_prot_hdr_unexpected_alg(dev, apdev):
    """DPP Config Object JWS error - protected header - unexpected alg"""
    prot_hdr = '{"typ":"dppCon","kid":"TnGKjIlNZaatrEAYrbbjiB67rjkL_AGVWXO6q9hDJKU","alg":"unexpected"}'
    run_dpp_config_error(dev, apdev, build_conf_obj(prot_hdr=prot_hdr))

def test_dpp_config_jws_error_prot_hdr_no_kid(dev, apdev):
    """DPP Config Object JWS error - protected header - no kid"""
    prot_hdr = '{"typ":"dppCon","alg":"ES256"}'
    run_dpp_config_error(dev, apdev, build_conf_obj(prot_hdr=prot_hdr))

def test_dpp_config_jws_error_prot_hdr_unexpected_kid(dev, apdev):
    """DPP Config Object JWS error - protected header - unexpected kid"""
    prot_hdr = '{"typ":"dppCon","kid":"MTIz","alg":"ES256"}'
    run_dpp_config_error(dev, apdev, build_conf_obj(prot_hdr=prot_hdr))

def test_dpp_config_signed_connector_error_no_dot_1(dev, apdev):
    """DPP Config Object signedConnector error - no dot(1)"""
    conn = "MTIz"
    run_dpp_config_error(dev, apdev, build_conf_obj(signed_connector=conn))

def test_dpp_config_signed_connector_error_no_dot_2(dev, apdev):
    """DPP Config Object signedConnector error - no dot(2)"""
    conn = "eyJ0eXAiOiJkcHBDb24iLCJraWQiOiJUbkdLaklsTlphYXRyRUFZcmJiamlCNjdyamtMX0FHVldYTzZxOWhESktVIiwiYWxnIjoiRVMyNTYifQ.MTIz"
    run_dpp_config_error(dev, apdev, build_conf_obj(signed_connector=conn))

def test_dpp_config_signed_connector_error_unexpected_signature_len(dev, apdev):
    """DPP Config Object signedConnector error - unexpected signature length"""
    conn = "eyJ0eXAiOiJkcHBDb24iLCJraWQiOiJUbkdLaklsTlphYXRyRUFZcmJiamlCNjdyamtMX0FHVldYTzZxOWhESktVIiwiYWxnIjoiRVMyNTYifQ.MTIz.MTIz"
    run_dpp_config_error(dev, apdev, build_conf_obj(signed_connector=conn))

def test_dpp_config_signed_connector_error_invalid_signature_der(dev, apdev):
    """DPP Config Object signedConnector error - invalid signature DER"""
    conn = "eyJ0eXAiOiJkcHBDb24iLCJraWQiOiJUbkdLaklsTlphYXRyRUFZcmJiamlCNjdyamtMX0FHVldYTzZxOWhESktVIiwiYWxnIjoiRVMyNTYifQ.MTIz.MTI"
    run_dpp_config_error(dev, apdev, build_conf_obj(signed_connector=conn))

def test_dpp_config_no_csign(dev, apdev):
    """DPP Config Object error - no csign"""
    run_dpp_config_error(dev, apdev, build_conf_obj(csign=False))

def test_dpp_config_no_signed_connector(dev, apdev):
    """DPP Config Object error - no signedConnector"""
    run_dpp_config_error(dev, apdev, build_conf_obj(no_signed_connector=True))

def test_dpp_config_unexpected_signed_connector_char(dev, apdev):
    """DPP Config Object error - unexpected signedConnector character"""
    run_dpp_config_error(dev, apdev, build_conf_obj(signed_connector='a\nb'))

def test_dpp_config_root_not_an_object(dev, apdev):
    """DPP Config Object error - root not an object"""
    conf = "1"
    run_dpp_config_error(dev, apdev, conf)

def test_dpp_config_no_wi_fi_tech(dev, apdev):
    """DPP Config Object error - no wi-fi_tech"""
    conf = "{}"
    run_dpp_config_error(dev, apdev, conf)

def test_dpp_config_unsupported_wi_fi_tech(dev, apdev):
    """DPP Config Object error - unsupported wi-fi_tech"""
    conf = '{"wi-fi_tech":"unsupported"}'
    run_dpp_config_error(dev, apdev, conf)

def test_dpp_config_no_discovery(dev, apdev):
    """DPP Config Object error - no discovery"""
    conf = '{"wi-fi_tech":"infra"}'
    run_dpp_config_error(dev, apdev, conf)

def test_dpp_config_no_discovery_ssid(dev, apdev):
    """DPP Config Object error - no discovery::ssid"""
    conf = '{"wi-fi_tech":"infra","discovery":{}}'
    run_dpp_config_error(dev, apdev, conf)

def test_dpp_config_too_long_discovery_ssid(dev, apdev):
    """DPP Config Object error - too long discovery::ssid"""
    conf = '{"wi-fi_tech":"infra","discovery":{"ssid":"%s"}}' % (33*'A')
    run_dpp_config_error(dev, apdev, conf)

def test_dpp_config_no_cred(dev, apdev):
    """DPP Config Object error - no cred"""
    conf = '{"wi-fi_tech":"infra","discovery":{"ssid":"test"}}'
    run_dpp_config_error(dev, apdev, conf)

def test_dpp_config_no_cred_akm(dev, apdev):
    """DPP Config Object error - no cred::akm"""
    conf = '{"wi-fi_tech":"infra","discovery":{"ssid":"test"},"cred":{}}'
    run_dpp_config_error(dev, apdev, conf)

def test_dpp_config_unsupported_cred_akm(dev, apdev):
    """DPP Config Object error - unsupported cred::akm"""
    conf = '{"wi-fi_tech":"infra","discovery":{"ssid":"test"},"cred":{"akm":"unsupported"}}'
    run_dpp_config_error(dev, apdev, conf)

def test_dpp_config_error_legacy_no_pass(dev, apdev):
    """DPP Config Object legacy error - no pass/psk"""
    conf = '{"wi-fi_tech":"infra","discovery":{"ssid":"test"},"cred":{"akm":"psk"}}'
    run_dpp_config_error(dev, apdev, conf)

def test_dpp_config_error_legacy_too_short_pass(dev, apdev):
    """DPP Config Object legacy error - too short pass/psk"""
    conf = '{"wi-fi_tech":"infra","discovery":{"ssid":"test"},"cred":{"akm":"psk","pass":"1"}}'
    run_dpp_config_error(dev, apdev, conf)

def test_dpp_config_error_legacy_too_long_pass(dev, apdev):
    """DPP Config Object legacy error - too long pass/psk"""
    conf = '{"wi-fi_tech":"infra","discovery":{"ssid":"test"},"cred":{"akm":"psk","pass":"%s"}}' % (64*'A')
    run_dpp_config_error(dev, apdev, conf)

def test_dpp_config_error_legacy_psk_with_sae(dev, apdev):
    """DPP Config Object legacy error - psk_hex with SAE"""
    conf = '{"wi-fi_tech":"infra","discovery":{"ssid":"test"},"cred":{"akm":"sae","psk_hex":"%s"}}' % (32*"12")
    run_dpp_config_error(dev, apdev, conf)

def test_dpp_config_error_legacy_no_pass_for_sae(dev, apdev):
    """DPP Config Object legacy error - no pass for SAE"""
    conf = '{"wi-fi_tech":"infra","discovery":{"ssid":"test"},"cred":{"akm":"psk+sae","psk_hex":"%s"}}' % (32*"12")
    run_dpp_config_error(dev, apdev, conf)

def test_dpp_config_error_legacy_invalid_psk(dev, apdev):
    """DPP Config Object legacy error - invalid psk_hex"""
    conf = '{"wi-fi_tech":"infra","discovery":{"ssid":"test"},"cred":{"akm":"psk","psk_hex":"%s"}}' % (32*"qa")
    run_dpp_config_error(dev, apdev, conf)

def test_dpp_config_error_legacy_too_short_psk(dev, apdev):
    """DPP Config Object legacy error - too short psk_hex"""
    conf = '{"wi-fi_tech":"infra","discovery":{"ssid":"test"},"cred":{"akm":"psk","psk_hex":"%s"}}' % (31*"12")
    run_dpp_config_error(dev, apdev, conf)

def get_der_int_32(val):
    a, b = struct.unpack('BB', val[0:2])
    if a != 0x02:
        raise Exception("Invalid DER encoding of INTEGER")
    if b > len(val) - 2:
        raise Exception("Invalid length of INTEGER (truncated)")
    val = val[2:]
    if b == 32:
        r = val[0:32]
    elif b == 33:
        if val[0] != 0:
            raise Exception("Too large INTEGER (32)")
        r = val[1:33]
    elif b < 32:
        r = (32 - b) * b'\x00' + val[0:b]
    else:
        raise Exception("Invalid length of INTEGER (32): %d" % b)
    return r, val[b:]

def ecdsa_sign(pkey, message, alg="sha256"):
    sign = OpenSSL.crypto.sign(pkey, message, alg)
    logger.debug("sign=" + binascii.hexlify(sign).decode())
    a, b = struct.unpack('BB', sign[0:2])
    if a != 0x30:
        raise Exception("Invalid DER encoding of ECDSA signature")
    if b != len(sign) - 2:
        raise Exception("Invalid length of ECDSA signature")
    sign = sign[2:]

    r, sign = get_der_int_32(sign)
    s, sign = get_der_int_32(sign)
    if len(sign) != 0:
        raise Exception("Extra data at the end of ECDSA signature")

    logger.info("r=" + binascii.hexlify(r).decode())
    logger.info("s=" + binascii.hexlify(s).decode())
    raw_sign = r + s
    return base64.urlsafe_b64encode(raw_sign).decode().rstrip('=')

p256_priv_key = """-----BEGIN EC PRIVATE KEY-----
MHcCAQEEIBVQij9ah629f1pu3tarDQGQvrzHgAkgYd1jHGiLxNajoAoGCCqGSM49
AwEHoUQDQgAEAC9d2/JirKu72F2qLuv5jEFMD1Cqu9EiyGk7cOzn/2DJ51p2mEoW
n03N6XRvTC+G7WPol9Ng97NAM2sK57+F/Q==
-----END EC PRIVATE KEY-----"""
p256_pub_key_x = binascii.unhexlify("002f5ddbf262acabbbd85daa2eebf98c414c0f50aabbd122c8693b70ece7ff60")
p256_pub_key_y = binascii.unhexlify("c9e75a76984a169f4dcde9746f4c2f86ed63e897d360f7b340336b0ae7bf85fd")

def run_dpp_config_connector(dev, apdev, expiry=None, payload=None,
                             skip_net_access_key_mismatch=True,
                             conf_failure=True):
    if not openssl_imported:
        raise HwsimSkip("OpenSSL python method not available")
    pkey = OpenSSL.crypto.load_privatekey(OpenSSL.crypto.FILETYPE_PEM,
                                          p256_priv_key)
    x = base64.urlsafe_b64encode(p256_pub_key_x).decode().rstrip('=')
    y = base64.urlsafe_b64encode(p256_pub_key_y).decode().rstrip('=')

    pubkey = b'\x04' + p256_pub_key_x + p256_pub_key_y
    kid = base64.urlsafe_b64encode(hashlib.sha256(pubkey).digest()).decode().rstrip('=')

    prot_hdr = '{"typ":"dppCon","kid":"%s","alg":"ES256"}' % kid

    if not payload:
        payload = '{"groups":[{"groupId":"*","netRole":"sta"}],"netAccessKey":{"kty":"EC","crv":"P-256","x":"aTF4JEGIPKSZ0Xv9zdCMjm-tn5XpMsYIVZ9wySAz1gI","y":"QGcHWA_6rbU9XDXAztoX-M5Q3suTnMaqEhULtn7SSXw"}'
        if expiry:
            payload += ',"expiry":"%s"' % expiry
        payload += '}'
    conn = base64.urlsafe_b64encode(prot_hdr.encode()).decode().rstrip('=') + '.'
    conn += base64.urlsafe_b64encode(payload.encode()).decode().rstrip('=')
    sign = ecdsa_sign(pkey, conn)
    conn += '.' + sign
    run_dpp_config_error(dev, apdev,
                         build_conf_obj(x=x, y=y, signed_connector=conn),
                         skip_net_access_key_mismatch=skip_net_access_key_mismatch,
                         conf_failure=conf_failure)

def test_dpp_config_connector_error_ext_sign(dev, apdev):
    """DPP Config Object connector error - external signature calculation"""
    run_dpp_config_connector(dev, apdev, conf_failure=False)

def test_dpp_config_connector_error_too_short_timestamp(dev, apdev):
    """DPP Config Object connector error - too short timestamp"""
    run_dpp_config_connector(dev, apdev, expiry="1")

def test_dpp_config_connector_error_invalid_timestamp(dev, apdev):
    """DPP Config Object connector error - invalid timestamp"""
    run_dpp_config_connector(dev, apdev, expiry=19*"1")

def test_dpp_config_connector_error_invalid_timestamp_date(dev, apdev):
    """DPP Config Object connector error - invalid timestamp date"""
    run_dpp_config_connector(dev, apdev, expiry="9999-99-99T99:99:99Z")

def test_dpp_config_connector_error_invalid_time_zone(dev, apdev):
    """DPP Config Object connector error - invalid time zone"""
    run_dpp_config_connector(dev, apdev, expiry="2018-01-01T00:00:00*")

def test_dpp_config_connector_error_invalid_time_zone_2(dev, apdev):
    """DPP Config Object connector error - invalid time zone 2"""
    run_dpp_config_connector(dev, apdev, expiry="2018-01-01T00:00:00+")

def test_dpp_config_connector_error_expired_1(dev, apdev):
    """DPP Config Object connector error - expired 1"""
    run_dpp_config_connector(dev, apdev, expiry="2018-01-01T00:00:00")

def test_dpp_config_connector_error_expired_2(dev, apdev):
    """DPP Config Object connector error - expired 2"""
    run_dpp_config_connector(dev, apdev, expiry="2018-01-01T00:00:00Z")

def test_dpp_config_connector_error_expired_3(dev, apdev):
    """DPP Config Object connector error - expired 3"""
    run_dpp_config_connector(dev, apdev, expiry="2018-01-01T00:00:00+01")

def test_dpp_config_connector_error_expired_4(dev, apdev):
    """DPP Config Object connector error - expired 4"""
    run_dpp_config_connector(dev, apdev, expiry="2018-01-01T00:00:00+01:02")

def test_dpp_config_connector_error_expired_5(dev, apdev):
    """DPP Config Object connector error - expired 5"""
    run_dpp_config_connector(dev, apdev, expiry="2018-01-01T00:00:00-01")

def test_dpp_config_connector_error_expired_6(dev, apdev):
    """DPP Config Object connector error - expired 6"""
    run_dpp_config_connector(dev, apdev, expiry="2018-01-01T00:00:00-01:02")

def test_dpp_config_connector_error_no_groups(dev, apdev):
    """DPP Config Object connector error - no groups"""
    payload = '{"netAccessKey":{"kty":"EC","crv":"P-256","x":"aTF4JEGIPKSZ0Xv9zdCMjm-tn5XpMsYIVZ9wySAz1gI","y":"QGcHWA_6rbU9XDXAztoX-M5Q3suTnMaqEhULtn7SSXw"}}'
    run_dpp_config_connector(dev, apdev, payload=payload)

def test_dpp_config_connector_error_empty_groups(dev, apdev):
    """DPP Config Object connector error - empty groups"""
    payload = '{"groups":[],"netAccessKey":{"kty":"EC","crv":"P-256","x":"aTF4JEGIPKSZ0Xv9zdCMjm-tn5XpMsYIVZ9wySAz1gI","y":"QGcHWA_6rbU9XDXAztoX-M5Q3suTnMaqEhULtn7SSXw"}}'
    run_dpp_config_connector(dev, apdev, payload=payload)

def test_dpp_config_connector_error_missing_group_id(dev, apdev):
    """DPP Config Object connector error - missing groupId"""
    payload = '{"groups":[{"netRole":"sta"}],"netAccessKey":{"kty":"EC","crv":"P-256","x":"aTF4JEGIPKSZ0Xv9zdCMjm-tn5XpMsYIVZ9wySAz1gI","y":"QGcHWA_6rbU9XDXAztoX-M5Q3suTnMaqEhULtn7SSXw"}}'
    run_dpp_config_connector(dev, apdev, payload=payload)

def test_dpp_config_connector_error_missing_net_role(dev, apdev):
    """DPP Config Object connector error - missing netRole"""
    payload = '{"groups":[{"groupId":"*"}],"netAccessKey":{"kty":"EC","crv":"P-256","x":"aTF4JEGIPKSZ0Xv9zdCMjm-tn5XpMsYIVZ9wySAz1gI","y":"QGcHWA_6rbU9XDXAztoX-M5Q3suTnMaqEhULtn7SSXw"}}'
    run_dpp_config_connector(dev, apdev, payload=payload)

def test_dpp_config_connector_error_missing_net_access_key(dev, apdev):
    """DPP Config Object connector error - missing netAccessKey"""
    payload = '{"groups":[{"groupId":"*","netRole":"sta"}]}'
    run_dpp_config_connector(dev, apdev, payload=payload)

def test_dpp_config_connector_error_net_access_key_mismatch(dev, apdev):
    """DPP Config Object connector error - netAccessKey mismatch"""
    payload = '{"groups":[{"groupId":"*","netRole":"sta"}],"netAccessKey":{"kty":"EC","crv":"P-256","x":"aTF4JEGIPKSZ0Xv9zdCMjm-tn5XpMsYIVZ9wySAz1gI","y":"QGcHWA_6rbU9XDXAztoX-M5Q3suTnMaqEhULtn7SSXw"}}'
    run_dpp_config_connector(dev, apdev, payload=payload,
                             skip_net_access_key_mismatch=False)

def test_dpp_gas_timeout(dev, apdev):
    """DPP and GAS server timeout for a query"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    logger.info("dev0 displays QR Code")
    id0 = dev[0].dpp_bootstrap_gen(chan="81/1", mac=True)
    uri0 = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id0)

    logger.info("dev1 scans QR Code and initiates DPP Authentication")
    dev[0].set("ext_mgmt_frame_handling", "1")
    dev[0].dpp_listen(2412)

    # Force GAS fragmentation
    conf = '{"wi-fi_tech":"infra", "discovery":{"ssid":"test"},"cred":{"akm":"psk","pass":"secret passphrase"}}' + 3000*' '
    dev[1].set("dpp_config_obj_override", conf)

    dev[1].dpp_auth_init(uri=uri0)

    # DPP Authentication Request
    msg = dev[0].mgmt_rx()
    if "OK" not in dev[0].request("MGMT_RX_PROCESS freq={} datarate={} ssi_signal={} frame={}".format(
        msg['freq'], msg['datarate'], msg['ssi_signal'], binascii.hexlify(msg['frame']).decode())):
        raise Exception("MGMT_RX_PROCESS failed")

    # DPP Authentication Confirmation
    msg = dev[0].mgmt_rx()
    if "OK" not in dev[0].request("MGMT_RX_PROCESS freq={} datarate={} ssi_signal={} frame={}".format(
        msg['freq'], msg['datarate'], msg['ssi_signal'], binascii.hexlify(msg['frame']).decode())):
        raise Exception("MGMT_RX_PROCESS failed")

    wait_auth_success(dev[0], dev[1])

    # DPP Configuration Response (GAS Initial Response frame)
    msg = dev[0].mgmt_rx()
    if "OK" not in dev[0].request("MGMT_RX_PROCESS freq={} datarate={} ssi_signal={} frame={}".format(
        msg['freq'], msg['datarate'], msg['ssi_signal'], binascii.hexlify(msg['frame']).decode())):
        raise Exception("MGMT_RX_PROCESS failed")

    # GAS Comeback Response frame
    msg = dev[0].mgmt_rx()
    # Do not continue to force timeout on GAS server

    ev = dev[0].wait_event(["GAS-QUERY-DONE"], timeout=10)
    if ev is None:
        raise Exception("GAS result not reported (Enrollee)")
    if "result=TIMEOUT" not in ev:
        raise Exception("Unexpected GAS result (Enrollee): " + ev)
    dev[0].set("ext_mgmt_frame_handling", "0")

    ev = dev[1].wait_event(["DPP-CONF-FAILED"], timeout=15)
    if ev is None:
        raise Exception("DPP configuration failure not reported (Configurator)")

    ev = dev[0].wait_event(["DPP-CONF-FAILED"], timeout=1)
    if ev is None:
        raise Exception("DPP configuration failure not reported (Enrollee)")

def test_dpp_akm_sha256(dev, apdev):
    """DPP AKM (SHA256)"""
    run_dpp_akm(dev, apdev, 32)

def test_dpp_akm_sha384(dev, apdev):
    """DPP AKM (SHA384)"""
    run_dpp_akm(dev, apdev, 48)

def test_dpp_akm_sha512(dev, apdev):
    """DPP AKM (SHA512)"""
    run_dpp_akm(dev, apdev, 64)

def run_dpp_akm(dev, apdev, pmk_len):
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    params = {"ssid": "dpp",
              "wpa": "2",
              "wpa_key_mgmt": "DPP",
              "rsn_pairwise": "CCMP",
              "ieee80211w": "2"}
    try:
        hapd = hostapd.add_ap(apdev[0], params)
    except:
        raise HwsimSkip("DPP not supported")

    conf = hapd.request("GET_CONFIG")
    if "key_mgmt=DPP" not in conf.splitlines():
        logger.info("GET_CONFIG:\n" + conf)
        raise Exception("GET_CONFIG did not report correct key_mgmt")

    id = dev[0].connect("dpp", key_mgmt="DPP", ieee80211w="2", scan_freq="2412",
                        dpp_pfs="2", wait_connect=False)
    ev = dev[0].wait_event(["CTRL-EVENT-NETWORK-NOT-FOUND"], timeout=2)
    if not ev:
        raise Exception("Network mismatch not reported")
    dev[0].request("DISCONNECT")
    dev[0].dump_monitor()

    bssid = hapd.own_addr()
    pmkid = 16*'11'
    akmp = 2**23
    pmk = pmk_len*'22'
    cmd = "PMKSA_ADD %d %s %s %s 30240 43200 %d 0" % (id, bssid, pmkid, pmk, akmp)
    if "OK" not in dev[0].request(cmd):
        raise Exception("PMKSA_ADD failed (wpa_supplicant)")
    dev[0].select_network(id, freq="2412")
    ev = dev[0].wait_event(["CTRL-EVENT-ASSOC-REJECT"], timeout=2)
    dev[0].request("DISCONNECT")
    dev[0].dump_monitor()
    if not ev:
        raise Exception("Association attempt was not rejected")
    if "status_code=53" not in ev:
        raise Exception("Unexpected status code: " + ev)

    addr = dev[0].own_addr()
    cmd = "PMKSA_ADD %s %s %s 0 %d" % (addr, pmkid, pmk, akmp)
    if "OK" not in hapd.request(cmd):
        raise Exception("PMKSA_ADD failed (hostapd)")

    dev[0].select_network(id, freq="2412")
    dev[0].wait_connected()
    val = dev[0].get_status_field("key_mgmt")
    if val != "DPP":
        raise Exception("Unexpected key_mgmt: " + val)

params1_csign = "3059301306072a8648ce3d020106082a8648ce3d03010703420004d02e5bd81a120762b5f0f2994777f5d40297238a6c294fd575cdf35fabec44c050a6421c401d98d659fd2ed13c961cc8287944dd3202f516977800d3ab2f39ee"
params1_ap_connector = "eyJ0eXAiOiJkcHBDb24iLCJraWQiOiJzOEFrYjg5bTV4UGhoYk5UbTVmVVo0eVBzNU5VMkdxYXNRY3hXUWhtQVFRIiwiYWxnIjoiRVMyNTYifQ.eyJncm91cHMiOlt7Imdyb3VwSWQiOiIqIiwibmV0Um9sZSI6ImFwIn1dLCJuZXRBY2Nlc3NLZXkiOnsia3R5IjoiRUMiLCJjcnYiOiJQLTI1NiIsIngiOiIwOHF4TlNYRzRWemdCV3BjVUdNSmc1czNvbElOVFJsRVQ1aERpNkRKY3ZjIiwieSI6IlVhaGFYQXpKRVpRQk1YaHRUQnlZZVlrOWtJYjk5UDA3UV9NcW9TVVZTVEkifX0.a5_nfMVr7Qe1SW0ZL3u6oQRm5NUCYUSfixDAJOUFN3XUfECBZ6E8fm8xjeSfdOytgRidTz0CTlIRjzPQo82dmQ"
params1_ap_netaccesskey = "30770201010420f6531d17f29dfab655b7c9e923478d5a345164c489aadd44a3519c3e9dcc792da00a06082a8648ce3d030107a14403420004d3cab13525c6e15ce0056a5c506309839b37a2520d4d19444f98438ba0c972f751a85a5c0cc911940131786d4c1c9879893d9086fdf4fd3b43f32aa125154932"
params1_sta_connector = "eyJ0eXAiOiJkcHBDb24iLCJraWQiOiJzOEFrYjg5bTV4UGhoYk5UbTVmVVo0eVBzNU5VMkdxYXNRY3hXUWhtQVFRIiwiYWxnIjoiRVMyNTYifQ.eyJncm91cHMiOlt7Imdyb3VwSWQiOiIqIiwibmV0Um9sZSI6InN0YSJ9XSwibmV0QWNjZXNzS2V5Ijp7Imt0eSI6IkVDIiwiY3J2IjoiUC0yNTYiLCJ4IjoiZWMzR3NqQ3lQMzVBUUZOQUJJdEltQnN4WXVyMGJZX1dES1lfSE9zUGdjNCIsInkiOiJTRS1HVllkdWVnTFhLMU1TQXZNMEx2QWdLREpTNWoyQVhCbE9PMTdUSTRBIn19.PDK9zsGlK-e1pEOmNxVeJfCS8pNeay6ckIS1TXCQsR64AR-9wFPCNVjqOxWvVKltehyMFqVAtOcv0IrjtMJFqQ"
params1_sta_netaccesskey = "30770201010420bc33380c26fd2168b69cd8242ed1df07ba89aa4813f8d4e8523de6ca3f8dd28ba00a06082a8648ce3d030107a1440342000479cdc6b230b23f7e40405340048b48981b3162eaf46d8fd60ca63f1ceb0f81ce484f8655876e7a02d72b531202f3342ef020283252e63d805c194e3b5ed32380"

def test_dpp_network_introduction(dev, apdev):
    """DPP network introduction"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])

    params = {"ssid": "dpp",
              "wpa": "2",
              "wpa_key_mgmt": "DPP",
              "ieee80211w": "2",
              "rsn_pairwise": "CCMP",
              "dpp_connector": params1_ap_connector,
              "dpp_csign": params1_csign,
              "dpp_netaccesskey": params1_ap_netaccesskey}
    try:
        hapd = hostapd.add_ap(apdev[0], params)
    except:
        raise HwsimSkip("DPP not supported")

    id = dev[0].connect("dpp", key_mgmt="DPP", scan_freq="2412",
                        ieee80211w="2",
                        dpp_csign=params1_csign,
                        dpp_connector=params1_sta_connector,
                        dpp_netaccesskey=params1_sta_netaccesskey)
    val = dev[0].get_status_field("key_mgmt")
    if val != "DPP":
        raise Exception("Unexpected key_mgmt: " + val)

def test_dpp_network_introduction_expired(dev, apdev):
    """DPP network introduction with expired netaccesskey"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])

    params = {"ssid": "dpp",
              "wpa": "2",
              "wpa_key_mgmt": "DPP",
              "ieee80211w": "2",
              "rsn_pairwise": "CCMP",
              "dpp_connector": params1_ap_connector,
              "dpp_csign": params1_csign,
              "dpp_netaccesskey": params1_ap_netaccesskey,
              "dpp_netaccesskey_expiry": "1565530889"}
    try:
        hapd = hostapd.add_ap(apdev[0], params)
    except:
        raise HwsimSkip("DPP not supported")

    dev[0].connect("dpp", key_mgmt="DPP", scan_freq="2412",
                   ieee80211w="2",
                   dpp_csign=params1_csign,
                   dpp_connector=params1_sta_connector,
                   dpp_netaccesskey=params1_sta_netaccesskey,
                   wait_connect=False)
    ev = hapd.wait_event(["DPP-RX"], timeout=10)
    if ev is None:
        raise Exception("No DPP Peer Discovery Request seen")
    if "type=5" not in ev:
        raise Exception("Unexpected DPP message received: " + ev)
    ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED"], timeout=1)
    dev[0].request("DISCONNECT")
    if ev:
        raise Exception("Connection reported")

    hapd.disable()
    hapd.set("dpp_netaccesskey_expiry", "2565530889")
    hapd.enable()
    dev[0].request("RECONNECT")
    dev[0].wait_connected()

def test_dpp_and_sae_akm(dev, apdev):
    """DPP and SAE AKMs"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    if "SAE" not in dev[1].get_capability("auth_alg"):
        raise HwsimSkip("SAE not supported")

    params = {"ssid": "dpp+sae",
              "wpa": "2",
              "wpa_key_mgmt": "DPP SAE",
              "ieee80211w": "2",
              "rsn_pairwise": "CCMP",
              "sae_password": "sae-password",
              "dpp_connector": params1_ap_connector,
              "dpp_csign": params1_csign,
              "dpp_netaccesskey": params1_ap_netaccesskey}
    try:
        hapd = hostapd.add_ap(apdev[0], params)
    except:
        raise HwsimSkip("DPP not supported")

    id = dev[0].connect("dpp+sae", key_mgmt="DPP", scan_freq="2412",
                        ieee80211w="2",
                        dpp_csign=params1_csign,
                        dpp_connector=params1_sta_connector,
                        dpp_netaccesskey=params1_sta_netaccesskey)
    val = dev[0].get_status_field("key_mgmt")
    if val != "DPP":
        raise Exception("Unexpected key_mgmt for DPP: " + val)

    dev[1].request("SET sae_groups ")
    id = dev[1].connect("dpp+sae", key_mgmt="SAE", scan_freq="2412",
                        ieee80211w="2", psk="sae-password")
    val = dev[1].get_status_field("key_mgmt")
    if val != "SAE":
        raise Exception("Unexpected key_mgmt for SAE: " + val)

def test_dpp_ap_config(dev, apdev):
    """DPP and AP configuration"""
    run_dpp_ap_config(dev, apdev)

def test_dpp_ap_config_p256_p256(dev, apdev):
    """DPP and AP configuration (P-256 + P-256)"""
    run_dpp_ap_config(dev, apdev, curve="P-256", conf_curve="P-256")

def test_dpp_ap_config_p256_p384(dev, apdev):
    """DPP and AP configuration (P-256 + P-384)"""
    run_dpp_ap_config(dev, apdev, curve="P-256", conf_curve="P-384")

def test_dpp_ap_config_p256_p521(dev, apdev):
    """DPP and AP configuration (P-256 + P-521)"""
    run_dpp_ap_config(dev, apdev, curve="P-256", conf_curve="P-521")

def test_dpp_ap_config_p384_p256(dev, apdev):
    """DPP and AP configuration (P-384 + P-256)"""
    run_dpp_ap_config(dev, apdev, curve="P-384", conf_curve="P-256")

def test_dpp_ap_config_p384_p384(dev, apdev):
    """DPP and AP configuration (P-384 + P-384)"""
    run_dpp_ap_config(dev, apdev, curve="P-384", conf_curve="P-384")

def test_dpp_ap_config_p384_p521(dev, apdev):
    """DPP and AP configuration (P-384 + P-521)"""
    run_dpp_ap_config(dev, apdev, curve="P-384", conf_curve="P-521")

def test_dpp_ap_config_p521_p256(dev, apdev):
    """DPP and AP configuration (P-521 + P-256)"""
    run_dpp_ap_config(dev, apdev, curve="P-521", conf_curve="P-256")

def test_dpp_ap_config_p521_p384(dev, apdev):
    """DPP and AP configuration (P-521 + P-384)"""
    run_dpp_ap_config(dev, apdev, curve="P-521", conf_curve="P-384")

def test_dpp_ap_config_p521_p521(dev, apdev):
    """DPP and AP configuration (P-521 + P-521)"""
    run_dpp_ap_config(dev, apdev, curve="P-521", conf_curve="P-521")

def test_dpp_ap_config_bp256_bp256(dev, apdev):
    """DPP and AP configuration (BP-256 + BP-256)"""
    run_dpp_ap_config(dev, apdev, curve="BP-256", conf_curve="BP-256")

def test_dpp_ap_config_bp384_bp384(dev, apdev):
    """DPP and AP configuration (BP-384 + BP-384)"""
    run_dpp_ap_config(dev, apdev, curve="BP-384", conf_curve="BP-384")

def test_dpp_ap_config_bp512_bp512(dev, apdev):
    """DPP and AP configuration (BP-512 + BP-512)"""
    run_dpp_ap_config(dev, apdev, curve="BP-512", conf_curve="BP-512")

def test_dpp_ap_config_p256_bp256(dev, apdev):
    """DPP and AP configuration (P-256 + BP-256)"""
    run_dpp_ap_config(dev, apdev, curve="P-256", conf_curve="BP-256")

def test_dpp_ap_config_bp256_p256(dev, apdev):
    """DPP and AP configuration (BP-256 + P-256)"""
    run_dpp_ap_config(dev, apdev, curve="BP-256", conf_curve="P-256")

def test_dpp_ap_config_p521_bp512(dev, apdev):
    """DPP and AP configuration (P-521 + BP-512)"""
    run_dpp_ap_config(dev, apdev, curve="P-521", conf_curve="BP-512")

def test_dpp_ap_config_bp512_p521(dev, apdev):
    """DPP and AP configuration (BP-512 + P-521)"""
    run_dpp_ap_config(dev, apdev, curve="BP-512", conf_curve="P-521")

def test_dpp_ap_config_reconfig_configurator(dev, apdev):
    """DPP and AP configuration with Configurator reconfiguration"""
    run_dpp_ap_config(dev, apdev, reconf_configurator=True)

def update_hapd_config(hapd):
    ev = hapd.wait_event(["DPP-CONFOBJ-SSID"], timeout=1)
    if ev is None:
        raise Exception("SSID not reported (AP)")
    ssid = ev.split(' ')[1]

    ev = hapd.wait_event(["DPP-CONNECTOR"], timeout=1)
    if ev is None:
        raise Exception("Connector not reported (AP)")
    connector = ev.split(' ')[1]

    ev = hapd.wait_event(["DPP-C-SIGN-KEY"], timeout=1)
    if ev is None:
        raise Exception("C-sign-key not reported (AP)")
    p = ev.split(' ')
    csign = p[1]

    ev = hapd.wait_event(["DPP-NET-ACCESS-KEY"], timeout=1)
    if ev is None:
        raise Exception("netAccessKey not reported (AP)")
    p = ev.split(' ')
    net_access_key = p[1]
    net_access_key_expiry = p[2] if len(p) > 2 else None

    logger.info("Update AP configuration to use key_mgmt=DPP")
    hapd.disable()
    hapd.set("ssid", ssid)
    hapd.set("utf8_ssid", "1")
    hapd.set("wpa", "2")
    hapd.set("wpa_key_mgmt", "DPP")
    hapd.set("ieee80211w", "2")
    hapd.set("rsn_pairwise", "CCMP")
    hapd.set("dpp_connector", connector)
    hapd.set("dpp_csign", csign)
    hapd.set("dpp_netaccesskey", net_access_key)
    if net_access_key_expiry:
        hapd.set("dpp_netaccesskey_expiry", net_access_key_expiry)
    hapd.enable()

def run_dpp_ap_config(dev, apdev, curve=None, conf_curve=None,
                      reconf_configurator=False):
    brainpool = (curve and "BP-" in curve) or \
        (conf_curve and "BP-" in conf_curve)
    check_dpp_capab(dev[0], brainpool)
    check_dpp_capab(dev[1], brainpool)
    hapd = hostapd.add_ap(apdev[0], {"ssid": "unconfigured"})
    check_dpp_capab(hapd)

    id_h = hapd.dpp_bootstrap_gen(chan="81/1", mac=True, curve=curve)
    uri = hapd.request("DPP_BOOTSTRAP_GET_URI %d" % id_h)

    conf_id = dev[0].dpp_configurator_add(curve=conf_curve)

    if reconf_configurator:
        csign = dev[0].request("DPP_CONFIGURATOR_GET_KEY %d" % conf_id)
        if "FAIL" in csign or len(csign) == 0:
            raise Exception("DPP_CONFIGURATOR_GET_KEY failed")

    dev[0].dpp_auth_init(uri=uri, conf="ap-dpp", configurator=conf_id)
    wait_auth_success(hapd, dev[0], configurator=dev[0], enrollee=hapd)
    update_hapd_config(hapd)

    id1 = dev[1].dpp_bootstrap_gen(chan="81/1", mac=True, curve=curve)
    uri1 = dev[1].request("DPP_BOOTSTRAP_GET_URI %d" % id1)

    if reconf_configurator:
        dev[0].dpp_configurator_remove(conf_id)
        conf_id = dev[0].dpp_configurator_add(curve=conf_curve, key=csign)

    dev[1].dpp_listen(2412)
    dev[0].dpp_auth_init(uri=uri1, conf="sta-dpp", configurator=conf_id)
    wait_auth_success(dev[1], dev[0], configurator=dev[0], enrollee=dev[1],
                      stop_responder=True)

    ev = dev[1].wait_event(["DPP-CONFOBJ-SSID"], timeout=1)
    if ev is None:
        raise Exception("SSID not reported")
    ssid = ev.split(' ')[1]

    ev = dev[1].wait_event(["DPP-CONNECTOR"], timeout=1)
    if ev is None:
        raise Exception("Connector not reported")
    connector = ev.split(' ')[1]

    ev = dev[1].wait_event(["DPP-C-SIGN-KEY"], timeout=1)
    if ev is None:
        raise Exception("C-sign-key not reported")
    p = ev.split(' ')
    csign = p[1]

    ev = dev[1].wait_event(["DPP-NET-ACCESS-KEY"], timeout=1)
    if ev is None:
        raise Exception("netAccessKey not reported")
    p = ev.split(' ')
    net_access_key = p[1]
    net_access_key_expiry = p[2] if len(p) > 2 else None

    dev[1].dump_monitor()

    id = dev[1].connect(ssid, key_mgmt="DPP", ieee80211w="2", scan_freq="2412",
                        only_add_network=True)
    dev[1].set_network_quoted(id, "dpp_connector", connector)
    dev[1].set_network(id, "dpp_csign", csign)
    dev[1].set_network(id, "dpp_netaccesskey", net_access_key)
    if net_access_key_expiry:
        dev[1].set_network(id, "dpp_netaccess_expiry", net_access_key_expiry)

    logger.info("Check data connection")
    dev[1].select_network(id, freq="2412")
    dev[1].wait_connected()

def test_dpp_auto_connect_1(dev, apdev):
    """DPP and auto connect (1)"""
    try:
        run_dpp_auto_connect(dev, apdev, 1)
    finally:
        dev[0].set("dpp_config_processing", "0", allow_fail=True)

def test_dpp_auto_connect_2(dev, apdev):
    """DPP and auto connect (2)"""
    try:
        run_dpp_auto_connect(dev, apdev, 2)
    finally:
        dev[0].set("dpp_config_processing", "0", allow_fail=True)

def test_dpp_auto_connect_2_connect_cmd(dev, apdev):
    """DPP and auto connect (2) using connect_cmd"""
    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    wpas.interface_add("wlan5", drv_params="force_connect_cmd=1")
    dev_new = [wpas, dev[1]]
    try:
        run_dpp_auto_connect(dev_new, apdev, 2)
    finally:
        wpas.set("dpp_config_processing", "0", allow_fail=True)

def test_dpp_auto_connect_2_sta_ver1(dev, apdev):
    """DPP and auto connect (2; STA using ver 1)"""
    try:
        run_dpp_auto_connect(dev, apdev, 2, sta_version=1)
    finally:
        dev[0].set("dpp_config_processing", "0", allow_fail=True)

def test_dpp_auto_connect_2_ap_ver1(dev, apdev):
    """DPP and auto connect (2; AP using ver 1)"""
    try:
        run_dpp_auto_connect(dev, apdev, 2, ap_version=1)
    finally:
        dev[0].set("dpp_config_processing", "0", allow_fail=True)

def test_dpp_auto_connect_2_ver1(dev, apdev):
    """DPP and auto connect (2; AP and STA using ver 1)"""
    try:
        run_dpp_auto_connect(dev, apdev, 2, ap_version=1, sta_version=1)
    finally:
        dev[0].set("dpp_config_processing", "0", allow_fail=True)

def test_dpp_auto_connect_2_conf_ver1(dev, apdev):
    """DPP and auto connect (2; Configurator using ver 1)"""
    try:
        run_dpp_auto_connect(dev, apdev, 2, sta1_version=1)
    finally:
        dev[0].set("dpp_config_processing", "0", allow_fail=True)

def run_dpp_auto_connect(dev, apdev, processing, ap_version=0, sta_version=0,
                         sta1_version=0, stop_after_prov=False):
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])

    csign = "30770201010420768240a3fc89d6662d9782f120527fe7fb9edc6366ab0b9c7dde96125cfd250fa00a06082a8648ce3d030107a144034200042908e1baf7bf413cc66f9e878a03e8bb1835ba94b033dbe3d6969fc8575d5eb5dfda1cb81c95cee21d0cd7d92ba30541ffa05cb6296f5dd808b0c1c2a83c0708"
    csign_pub = "3059301306072a8648ce3d020106082a8648ce3d030107034200042908e1baf7bf413cc66f9e878a03e8bb1835ba94b033dbe3d6969fc8575d5eb5dfda1cb81c95cee21d0cd7d92ba30541ffa05cb6296f5dd808b0c1c2a83c0708"
    ap_connector = "eyJ0eXAiOiJkcHBDb24iLCJraWQiOiJwYWtZbXVzd1dCdWpSYTl5OEsweDViaTVrT3VNT3dzZHRlaml2UG55ZHZzIiwiYWxnIjoiRVMyNTYifQ.eyJncm91cHMiOlt7Imdyb3VwSWQiOiIqIiwibmV0Um9sZSI6ImFwIn1dLCJuZXRBY2Nlc3NLZXkiOnsia3R5IjoiRUMiLCJjcnYiOiJQLTI1NiIsIngiOiIybU5vNXZuRkI5bEw3d1VWb1hJbGVPYzBNSEE1QXZKbnpwZXZULVVTYzVNIiwieSI6IlhzS3dqVHJlLTg5WWdpU3pKaG9CN1haeUttTU05OTl3V2ZaSVl0bi01Q3MifX0.XhjFpZgcSa7G2lHy0OCYTvaZFRo5Hyx6b7g7oYyusLC7C_73AJ4_BxEZQVYJXAtDuGvb3dXSkHEKxREP9Q6Qeg"
    ap_netaccesskey = "30770201010420ceba752db2ad5200fa7bc565b9c05c69b7eb006751b0b329b0279de1c19ca67ca00a06082a8648ce3d030107a14403420004da6368e6f9c507d94bef0515a1722578e73430703902f267ce97af4fe51273935ec2b08d3adefbcf588224b3261a01ed76722a630cf7df7059f64862d9fee42b"

    params = {"ssid": "test",
              "wpa": "2",
              "wpa_key_mgmt": "DPP",
              "ieee80211w": "2",
              "rsn_pairwise": "CCMP",
              "dpp_connector": ap_connector,
              "dpp_csign": csign_pub,
              "dpp_netaccesskey": ap_netaccesskey}
    try:
        hapd = hostapd.add_ap(apdev[0], params)
        if ap_version:
            hapd.set("dpp_version_override", str(ap_version))
    except:
        raise HwsimSkip("DPP not supported")

    if sta_version:
        dev[0].set("dpp_version_override", str(sta_version))
    if sta1_version:
        dev[1].set("dpp_version_override", str(sta1_version))
    conf_id = dev[1].dpp_configurator_add(key=csign)
    dev[0].set("dpp_config_processing", str(processing))
    id0 = dev[0].dpp_bootstrap_gen(chan="81/1", mac=True)
    uri0 = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id0)
    dev[0].dpp_listen(2412)
    dev[1].dpp_auth_init(uri=uri0, conf="sta-dpp", configurator=conf_id)
    wait_auth_success(dev[0], dev[1], configurator=dev[1], enrollee=dev[0])
    ev = dev[0].wait_event(["DPP-NETWORK-ID"], timeout=1)
    if ev is None:
        raise Exception("DPP network profile not generated")
    id = ev.split(' ')[1]
    if stop_after_prov:
        return id, hapd

    if processing == 1:
        dev[0].select_network(id, freq=2412)

    dev[0].wait_connected()
    hwsim_utils.test_connectivity(dev[0], hapd)

def test_dpp_auto_connect_legacy(dev, apdev):
    """DPP and auto connect (legacy)"""
    try:
        run_dpp_auto_connect_legacy(dev, apdev)
    finally:
        dev[0].set("dpp_config_processing", "0", allow_fail=True)

def test_dpp_auto_connect_legacy_ssid_charset(dev, apdev):
    """DPP and auto connect (legacy, ssid_charset)"""
    try:
        run_dpp_auto_connect_legacy(dev, apdev, ssid_charset=12345)
    finally:
        dev[0].set("dpp_config_processing", "0", allow_fail=True)

def test_dpp_auto_connect_legacy_sae_1(dev, apdev):
    """DPP and auto connect (legacy SAE)"""
    try:
        run_dpp_auto_connect_legacy(dev, apdev, conf='sta-sae', psk_sae=True)
    finally:
        dev[0].set("dpp_config_processing", "0", allow_fail=True)

def test_dpp_auto_connect_legacy_sae_2(dev, apdev):
    """DPP and auto connect (legacy SAE)"""
    try:
        run_dpp_auto_connect_legacy(dev, apdev, conf='sta-sae', sae_only=True)
    finally:
        dev[0].set("dpp_config_processing", "0", allow_fail=True)

def test_dpp_auto_connect_legacy_psk_sae_1(dev, apdev):
    """DPP and auto connect (legacy PSK+SAE)"""
    try:
        run_dpp_auto_connect_legacy(dev, apdev, conf='sta-psk-sae',
                                    psk_sae=True)
    finally:
        dev[0].set("dpp_config_processing", "0", allow_fail=True)

def test_dpp_auto_connect_legacy_psk_sae_2(dev, apdev):
    """DPP and auto connect (legacy PSK+SAE)"""
    try:
        run_dpp_auto_connect_legacy(dev, apdev, conf='sta-psk-sae',
                                    sae_only=True)
    finally:
        dev[0].set("dpp_config_processing", "0", allow_fail=True)

def test_dpp_auto_connect_legacy_psk_sae_3(dev, apdev):
    """DPP and auto connect (legacy PSK+SAE)"""
    try:
        run_dpp_auto_connect_legacy(dev, apdev, conf='sta-psk-sae')
    finally:
        dev[0].set("dpp_config_processing", "0", allow_fail=True)

def run_dpp_auto_connect_legacy(dev, apdev, conf='sta-psk',
                                ssid_charset=None,
                                psk_sae=False, sae_only=False):
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])

    params = hostapd.wpa2_params(ssid="dpp-legacy",
                                 passphrase="secret passphrase")
    if sae_only:
            params['wpa_key_mgmt'] = 'SAE'
            params['ieee80211w'] = '2'
    elif psk_sae:
            params['wpa_key_mgmt'] = 'WPA-PSK SAE'
            params['ieee80211w'] = '1'
            params['sae_require_mfp'] = '1'

    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].request("SET sae_groups ")
    dev[0].set("dpp_config_processing", "2")
    id0 = dev[0].dpp_bootstrap_gen(chan="81/1", mac=True)
    uri0 = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id0)

    dev[0].dpp_listen(2412)
    dev[1].dpp_auth_init(uri=uri0, conf=conf, ssid="dpp-legacy",
                         ssid_charset=ssid_charset,
                         passphrase="secret passphrase")
    wait_auth_success(dev[0], dev[1], configurator=dev[1], enrollee=dev[0])
    if ssid_charset:
        ev = dev[0].wait_event(["DPP-CONFOBJ-SSID-CHARSET"], timeout=1)
        if ev is None:
            raise Exception("ssid_charset not reported")
        charset = ev.split(' ')[1]
        if charset != str(ssid_charset):
            raise Exception("Incorrect ssid_charset reported: " + ev)
    ev = dev[0].wait_event(["DPP-NETWORK-ID"], timeout=1)
    if ev is None:
        raise Exception("DPP network profile not generated")
    id = ev.split(' ')[1]

    dev[0].wait_connected()

def test_dpp_auto_connect_legacy_pmf_required(dev, apdev):
    """DPP and auto connect (legacy, PMF required)"""
    try:
        run_dpp_auto_connect_legacy_pmf_required(dev, apdev)
    finally:
        dev[0].set("dpp_config_processing", "0", allow_fail=True)

def run_dpp_auto_connect_legacy_pmf_required(dev, apdev):
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])

    params = hostapd.wpa2_params(ssid="dpp-legacy",
                                 passphrase="secret passphrase")
    params['wpa_key_mgmt'] = "WPA-PSK-SHA256"
    params['ieee80211w'] = "2"
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].set("dpp_config_processing", "2")
    id0 = dev[0].dpp_bootstrap_gen(chan="81/1", mac=True)
    uri0 = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id0)
    dev[0].dpp_listen(2412)
    dev[1].dpp_auth_init(uri=uri0, conf="sta-psk", ssid="dpp-legacy",
                         passphrase="secret passphrase")
    wait_auth_success(dev[0], dev[1], configurator=dev[1], enrollee=dev[0])
    ev = dev[0].wait_event(["DPP-NETWORK-ID"], timeout=1)
    if ev is None:
        raise Exception("DPP network profile not generated")
    dev[0].wait_connected()

def test_dpp_qr_code_auth_responder_configurator(dev, apdev):
    """DPP QR Code and responder as the configurator"""
    run_dpp_qr_code_auth_responder_configurator(dev, apdev, "")

def test_dpp_qr_code_auth_responder_configurator_group_id(dev, apdev):
    """DPP QR Code and responder as the configurator with group_id)"""
    run_dpp_qr_code_auth_responder_configurator(dev, apdev,
                                                " group_id=test-group")

def run_dpp_qr_code_auth_responder_configurator(dev, apdev, extra):
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    conf_id = dev[0].dpp_configurator_add()
    id0 = dev[0].dpp_bootstrap_gen(chan="81/1", mac=True)
    uri0 = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id0)
    dev[0].set("dpp_configurator_params",
               " conf=sta-dpp configurator=%d%s" % (conf_id, extra))
    dev[0].dpp_listen(2412, role="configurator")
    dev[1].dpp_auth_init(uri=uri0, role="enrollee")
    wait_auth_success(dev[0], dev[1], configurator=dev[0], enrollee=dev[1],
                      stop_responder=True)

def test_dpp_qr_code_auth_enrollee_init_netrole(dev, apdev):
    """DPP QR Code and enrollee initiating with netrole specified"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    conf_id = dev[0].dpp_configurator_add()
    id0 = dev[0].dpp_bootstrap_gen(chan="81/1", mac=True)
    uri0 = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id0)
    dev[0].set("dpp_configurator_params",
               " conf=configurator configurator=%d" % conf_id)
    dev[0].dpp_listen(2412, role="configurator")
    dev[1].dpp_auth_init(uri=uri0, role="enrollee", netrole="configurator")
    wait_auth_success(dev[0], dev[1], configurator=dev[0], enrollee=dev[1],
                      stop_responder=True)
    dev[0].dump_monitor()
    dev[1].dump_monitor()

    # verify that netrole resets back to sta, if not explicitly stated
    dev[0].set("dpp_configurator_params",
               "conf=sta-dpp configurator=%d" % conf_id)
    dev[0].dpp_listen(2412, role="configurator")
    dev[1].dpp_auth_init(uri=uri0, role="enrollee")
    wait_auth_success(dev[0], dev[1], configurator=dev[0], enrollee=dev[1],
                      stop_responder=True)

def test_dpp_qr_code_hostapd_init(dev, apdev):
    """DPP QR Code and hostapd as initiator"""
    check_dpp_capab(dev[0])
    hapd = hostapd.add_ap(apdev[0], {"ssid": "unconfigured",
                                     "channel": "6"})
    check_dpp_capab(hapd)
    conf_id = dev[0].dpp_configurator_add()
    id0 = dev[0].dpp_bootstrap_gen(chan="81/6", mac=True)
    uri0 = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id0)
    dev[0].set("dpp_configurator_params",
               " conf=ap-dpp configurator=%d" % conf_id)
    dev[0].dpp_listen(2437, role="configurator")
    hapd.dpp_auth_init(uri=uri0, role="enrollee")
    wait_auth_success(dev[0], hapd, configurator=dev[0], enrollee=hapd,
                      stop_responder=True)

def test_dpp_qr_code_hostapd_init_offchannel(dev, apdev):
    """DPP QR Code and hostapd as initiator (offchannel)"""
    run_dpp_qr_code_hostapd_init_offchannel(dev, apdev, None)

def test_dpp_qr_code_hostapd_init_offchannel_neg_freq(dev, apdev):
    """DPP QR Code and hostapd as initiator (offchannel, neg_freq)"""
    run_dpp_qr_code_hostapd_init_offchannel(dev, apdev, "neg_freq=2437")

def run_dpp_qr_code_hostapd_init_offchannel(dev, apdev, extra):
    check_dpp_capab(dev[0])
    hapd = hostapd.add_ap(apdev[0], {"ssid": "unconfigured",
                                     "channel": "6"})
    check_dpp_capab(hapd)
    conf_id = dev[0].dpp_configurator_add()
    id0 = dev[0].dpp_bootstrap_gen(chan="81/1,81/11", mac=True)
    uri0 = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id0)
    dev[0].set("dpp_configurator_params",
               " conf=ap-dpp configurator=%d" % conf_id)
    dev[0].dpp_listen(2462, role="configurator")
    hapd.dpp_auth_init(uri=uri0, role="enrollee", extra=extra)
    wait_auth_success(dev[0], hapd, configurator=dev[0], enrollee=hapd,
                      stop_responder=True)

def test_dpp_qr_code_hostapd_ignore_mismatch(dev, apdev):
    """DPP QR Code and hostapd ignoring netaccessKey mismatch"""
    check_dpp_capab(dev[0])
    hapd = hostapd.add_ap(apdev[0], {"ssid": "unconfigured",
                                     "channel": "6"})
    check_dpp_capab(hapd)
    conf_id = dev[0].dpp_configurator_add()
    id0 = dev[0].dpp_bootstrap_gen(chan="81/6", mac=True)
    uri0 = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id0)
    dev[0].set("dpp_configurator_params",
               "conf=ap-dpp configurator=%d" % conf_id)
    conf = '{"wi-fi_tech":"infra","discovery":{"ssid":"test"},"cred":{"akm":"dpp","signedConnector":"eyJ0eXAiOiJkcHBDb24iLCJraWQiOiJUbkdLaklsTlphYXRyRUFZcmJiamlCNjdyamtMX0FHVldYTzZxOWhESktVIiwiYWxnIjoiRVMyNTYifQ.eyJncm91cHMiOlt7Imdyb3VwSWQiOiIqIiwibmV0Um9sZSI6InN0YSJ9XSwibmV0QWNjZXNzS2V5Ijp7Imt0eSI6IkVDIiwiY3J2IjoiUC0yNTYiLCJ4IjoiYVRGNEpFR0lQS1NaMFh2OXpkQ01qbS10bjVYcE1zWUlWWjl3eVNBejFnSSIsInkiOiJRR2NIV0FfNnJiVTlYRFhBenRvWC1NNVEzc3VUbk1hcUVoVUx0bjdTU1h3In19._sm6YswxMf6hJLVTyYoU1uYUeY2VVkUNjrzjSiEhY42StD_RWowStEE-9CRsdCvLmsTptZ72_g40vTFwdId20A","csign":{"kty":"EC","crv":"P-256","x":"W4-Y5N1Pkos3UWb9A5qme0KUYRtY3CVUpekx_MapZ9s","y":"Et-M4NSF4NGjvh2VCh4B1sJ9eSCZ4RNzP2DBdP137VE","kid":"TnGKjIlNZaatrEAYrbbjiB67rjkL_AGVWXO6q9hDJKU"}}}'
    dev[0].set("dpp_config_obj_override", conf)
    dev[0].dpp_listen(2437, role="configurator")
    hapd.set("dpp_ignore_netaccesskey_mismatch", "1")
    hapd.dpp_auth_init(uri=uri0, role="enrollee")
    wait_auth_success(dev[0], hapd, configurator=dev[0], enrollee=hapd,
                      stop_responder=True)

def test_dpp_test_vector_p_256(dev, apdev):
    """DPP P-256 test vector (mutual auth)"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])

    # Responder bootstrapping key
    priv = "54ce181a98525f217216f59b245f60e9df30ac7f6b26c939418cfc3c42d1afa0"
    id0 = dev[0].dpp_bootstrap_gen(chan="81/11", mac=True, key="30310201010420" + priv + "a00a06082a8648ce3d030107")
    uri0 = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id0)

    # Responder protocol keypair override
    priv = "f798ed2e19286f6a6efe210b1863badb99af2a14b497634dbfd2a97394fb5aa5"
    dev[0].set("dpp_protocol_key_override",
               "30310201010420" + priv + "a00a06082a8648ce3d030107")

    dev[0].set("dpp_nonce_override", "3d0cfb011ca916d796f7029ff0b43393")

    # Initiator bootstrapping key
    priv = "15b2a83c5a0a38b61f2aa8200ee4994b8afdc01c58507d10d0a38f7eedf051bb"
    id1 = dev[1].dpp_bootstrap_gen(key="30310201010420" + priv + "a00a06082a8648ce3d030107")
    uri1 = dev[1].request("DPP_BOOTSTRAP_GET_URI %d" % id1)

    # Initiator protocol keypair override
    priv = "a87de9afbb406c96e5f79a3df895ecac3ad406f95da66314c8cb3165e0c61783"
    dev[1].set("dpp_protocol_key_override",
               "30310201010420" + priv + "a00a06082a8648ce3d030107")

    dev[1].set("dpp_nonce_override", "13f4602a16daeb69712263b9c46cba31")

    dev[0].dpp_qr_code(uri1)
    dev[0].dpp_listen(2462, qr="mutual")
    dev[1].dpp_auth_init(uri=uri0, own=id1, neg_freq=2412)
    wait_auth_success(dev[0], dev[1])

def test_dpp_test_vector_p_256_b(dev, apdev):
    """DPP P-256 test vector (Responder-only auth)"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])

    # Responder bootstrapping key
    priv = "54ce181a98525f217216f59b245f60e9df30ac7f6b26c939418cfc3c42d1afa0"
    id0 = dev[0].dpp_bootstrap_gen(chan="81/11", mac=True, key="30310201010420" + priv + "a00a06082a8648ce3d030107")
    uri0 = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id0)

    # Responder protocol keypair override
    priv = "f798ed2e19286f6a6efe210b1863badb99af2a14b497634dbfd2a97394fb5aa5"
    dev[0].set("dpp_protocol_key_override",
               "30310201010420" + priv + "a00a06082a8648ce3d030107")

    dev[0].set("dpp_nonce_override", "3d0cfb011ca916d796f7029ff0b43393")

    # Initiator bootstrapping key
    priv = "15b2a83c5a0a38b61f2aa8200ee4994b8afdc01c58507d10d0a38f7eedf051bb"
    id1 = dev[1].dpp_bootstrap_gen(key="30310201010420" + priv + "a00a06082a8648ce3d030107")
    uri1 = dev[1].request("DPP_BOOTSTRAP_GET_URI %d" % id1)

    # Initiator protocol keypair override
    priv = "a87de9afbb406c96e5f79a3df895ecac3ad406f95da66314c8cb3165e0c61783"
    dev[1].set("dpp_protocol_key_override",
               "30310201010420" + priv + "a00a06082a8648ce3d030107")

    dev[1].set("dpp_nonce_override", "13f4602a16daeb69712263b9c46cba31")

    dev[0].dpp_listen(2462)
    dev[1].dpp_auth_init(uri=uri0, own=id1, neg_freq=2412)
    wait_auth_success(dev[0], dev[1])

def der_priv_key_p_521(priv):
    if len(priv) != 2 * 66:
        raise Exception("Unexpected der_priv_key_p_521 parameter: " + priv)
    der_prefix = "30500201010442"
    der_postfix = "a00706052b81040023"
    return der_prefix + priv + der_postfix

def test_dpp_test_vector_p_521(dev, apdev):
    """DPP P-521 test vector (mutual auth)"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])

    # Responder bootstrapping key
    priv = "0061e54f518cdf859735da3dd64c6f72c2f086f41a6fd52915152ea2fe0f24ddaecd8883730c9c9fd82cf7c043a41021696388cf5190b731dd83638bcd56d8b6c743"
    id0 = dev[0].dpp_bootstrap_gen(chan="81/11", mac=True,
                                   key=der_priv_key_p_521(priv))
    uri0 = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id0)

    # Responder protocol keypair override
    priv = "01d8b7b17cd1b0a33f7c66fb4220999329cdaf4f8b44b2ffadde8ab8ed8abffa9f5358c5b1caae26709ca4fb78e52a4d08f2e4f24111a36a6f440d20a0000ff51597"
    dev[0].set("dpp_protocol_key_override", der_priv_key_p_521(priv))

    dev[0].set("dpp_nonce_override",
               "d749a782012eb0a8595af30b2dfc8d0880d004ebddb55ecc5afbdef18c400e01")

    # Initiator bootstrapping key
    priv = "0060c10df14af5ef27f6e362d31bdd9eeb44be77a323ba64b08f3f03d58b92cbfe05c182a91660caa081ca344243c47b5aa088bcdf738840eb35f0218b9f26881e02"
    id1 = dev[1].dpp_bootstrap_gen(key=der_priv_key_p_521(priv))
    uri1 = dev[1].request("DPP_BOOTSTRAP_GET_URI %d" % id1)

    # Initiator protocol keypair override
    priv = "019c1c08caaeec38fb931894699b095bc3ab8c1ec7ef0622d2e3eba821477c8c6fca41774f21166ad98aebda37c067d9aa08a8a2e1b5c44c61f2bae02a61f85d9661"
    dev[1].set("dpp_protocol_key_override", der_priv_key_p_521(priv))

    dev[1].set("dpp_nonce_override",
               "de972af3847bec3ba2aedd9f5c21cfdec7bf0bc5fe8b276cbcd0267807fb15b0")

    dev[0].dpp_qr_code(uri1)
    dev[0].dpp_listen(2462, qr="mutual")
    dev[1].dpp_auth_init(uri=uri0, own=id1, neg_freq=2412)
    wait_auth_success(dev[0], dev[1])

def test_dpp_pkex(dev, apdev):
    """DPP and PKEX"""
    run_dpp_pkex(dev, apdev)

def test_dpp_pkex_v2(dev, apdev):
    """DPP and PKEXv2"""
    run_dpp_pkex(dev, apdev, v2=True)

def test_dpp_pkex_p256(dev, apdev):
    """DPP and PKEX (P-256)"""
    run_dpp_pkex(dev, apdev, "P-256")

def test_dpp_pkex_p384(dev, apdev):
    """DPP and PKEX (P-384)"""
    run_dpp_pkex(dev, apdev, "P-384")

def test_dpp_pkex_p521(dev, apdev):
    """DPP and PKEX (P-521)"""
    run_dpp_pkex(dev, apdev, "P-521")

def test_dpp_pkex_bp256(dev, apdev):
    """DPP and PKEX (BP-256)"""
    run_dpp_pkex(dev, apdev, "brainpoolP256r1")

def test_dpp_pkex_bp384(dev, apdev):
    """DPP and PKEX (BP-384)"""
    run_dpp_pkex(dev, apdev, "brainpoolP384r1")

def test_dpp_pkex_bp512(dev, apdev):
    """DPP and PKEX (BP-512)"""
    run_dpp_pkex(dev, apdev, "brainpoolP512r1")

def test_dpp_pkex_config(dev, apdev):
    """DPP and PKEX with initiator as the configurator"""
    check_dpp_capab(dev[1])
    conf_id = dev[1].dpp_configurator_add()
    run_dpp_pkex(dev, apdev,
                 init_extra="conf=sta-dpp configurator=%d" % (conf_id),
                 check_config=True)

def test_dpp_pkex_no_identifier(dev, apdev):
    """DPP and PKEX without identifier"""
    run_dpp_pkex(dev, apdev, identifier_i=None, identifier_r=None)

def test_dpp_pkex_identifier_mismatch(dev, apdev):
    """DPP and PKEX with different identifiers"""
    run_dpp_pkex(dev, apdev, identifier_i="foo", identifier_r="bar",
                 expect_no_resp=True)

def test_dpp_pkex_identifier_mismatch2(dev, apdev):
    """DPP and PKEX with initiator using identifier and the responder not"""
    run_dpp_pkex(dev, apdev, identifier_i="foo", identifier_r=None,
                 expect_no_resp=True)

def test_dpp_pkex_identifier_mismatch3(dev, apdev):
    """DPP and PKEX with responder using identifier and the initiator not"""
    run_dpp_pkex(dev, apdev, identifier_i=None, identifier_r="bar",
                 expect_no_resp=True)

def run_dpp_pkex(dev, apdev, curve=None, init_extra=None, check_config=False,
                 identifier_i="test", identifier_r="test",
                 expect_no_resp=False, v2=False):
    min_ver = 3 if v2 else 1
    check_dpp_capab(dev[0], curve and "brainpool" in curve, min_ver=min_ver)
    check_dpp_capab(dev[1], curve and "brainpool" in curve, min_ver=min_ver)
    dev[0].dpp_pkex_resp(2437, identifier=identifier_r, code="secret",
                         curve=curve)
    dev[1].dpp_pkex_init(identifier=identifier_i, code="secret", curve=curve,
                         extra=init_extra, v2=v2)

    if expect_no_resp:
        ev = dev[0].wait_event(["DPP-RX"], timeout=10)
        if ev is None:
            raise Exception("DPP PKEX frame not received")
        ev = dev[1].wait_event(["DPP-AUTH-SUCCESS"], timeout=1)
        if ev is not None:
            raise Exception("DPP authentication succeeded")
        ev = dev[0].wait_event(["DPP-AUTH-SUCCESS"], timeout=0.1)
        if ev is not None:
            raise Exception("DPP authentication succeeded")
        return

    wait_auth_success(dev[0], dev[1],
                      configurator=dev[1] if check_config else None,
                      enrollee=dev[0] if check_config else None)

def test_dpp_pkex_5ghz(dev, apdev):
    """DPP and PKEX on 5 GHz"""
    try:
        dev[0].request("SET country US")
        dev[1].request("SET country US")
        ev = dev[0].wait_event(["CTRL-EVENT-REGDOM-CHANGE"], timeout=1)
        if ev is None:
            ev = dev[0].wait_global_event(["CTRL-EVENT-REGDOM-CHANGE"],
                                          timeout=1)
        run_dpp_pkex_5ghz(dev, apdev)
    finally:
        dev[0].request("SET country 00")
        dev[1].request("SET country 00")
        subprocess.call(['iw', 'reg', 'set', '00'])
        time.sleep(0.1)

def run_dpp_pkex_5ghz(dev, apdev):
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    dev[0].dpp_pkex_resp(5745, identifier="test", code="secret")
    dev[1].dpp_pkex_init(identifier="test", code="secret")
    wait_auth_success(dev[0], dev[1], timeout=20)

def test_dpp_pkex_test_vector(dev, apdev):
    """DPP and PKEX (P-256) test vector"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])

    init_addr = "ac:64:91:f4:52:07"
    resp_addr = "6e:5e:ce:6e:f3:dd"

    identifier = "joes_key"
    code = "thisisreallysecret"

    # Initiator bootstrapping private key
    init_priv = "5941b51acfc702cdc1c347264beb2920db88eb1a0bf03a211868b1632233c269"

    # Responder bootstrapping private key
    resp_priv = "2ae8956293f49986b6d0b8169a86805d9232babb5f6813fdfe96f19d59536c60"

    # Initiator x/X keypair override
    init_x_priv = "8365c5ed93d751bef2d92b410dc6adfd95670889183fac1bd66759ad85c3187a"

    # Responder y/Y keypair override
    resp_y_priv = "d98faa24d7dd3f592665d71a95c862bfd02c4c48acb0c515a41cbc6e929675ea"

    p256_prefix = "30310201010420"
    p256_postfix = "a00a06082a8648ce3d030107"

    dev[0].set("dpp_pkex_own_mac_override", resp_addr)
    dev[0].set("dpp_pkex_peer_mac_override", init_addr)
    dev[1].set("dpp_pkex_own_mac_override", init_addr)
    dev[1].set("dpp_pkex_peer_mac_override", resp_addr)

    # Responder y/Y keypair override
    dev[0].set("dpp_pkex_ephemeral_key_override",
               p256_prefix + resp_y_priv + p256_postfix)

    # Initiator x/X keypair override
    dev[1].set("dpp_pkex_ephemeral_key_override",
               p256_prefix + init_x_priv + p256_postfix)

    dev[0].dpp_pkex_resp(2437, identifier=identifier, code=code,
                         key=p256_prefix + resp_priv + p256_postfix)
    dev[1].dpp_pkex_init(identifier=identifier, code=code,
                         key=p256_prefix + init_priv + p256_postfix)
    wait_auth_success(dev[0], dev[1])

def test_dpp_pkex_code_mismatch(dev, apdev):
    """DPP and PKEX with mismatching code"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    dev[0].dpp_pkex_resp(2437, identifier="test", code="secret")
    id1 = dev[1].dpp_pkex_init(identifier="test", code="unknown")
    wait_dpp_fail(dev[0], "possible PKEX code mismatch")
    dev[0].dump_monitor()
    dev[1].dump_monitor()
    dev[1].dpp_pkex_init(identifier="test", code="secret", use_id=id1)
    wait_auth_success(dev[0], dev[1])

def test_dpp_pkex_code_mismatch_limit(dev, apdev):
    """DPP and PKEX with mismatching code limit"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    dev[0].dpp_pkex_resp(2437, identifier="test", code="secret")

    id1 = None
    for i in range(5):
        dev[0].dump_monitor()
        dev[1].dump_monitor()
        id1 = dev[1].dpp_pkex_init(identifier="test", code="unknown",
                                   use_id=id1)
        wait_dpp_fail(dev[0], "possible PKEX code mismatch")

    ev = dev[0].wait_event(["DPP-PKEX-T-LIMIT"], timeout=1)
    if ev is None:
        raise Exception("PKEX t limit not reported")

def test_dpp_pkex_curve_mismatch(dev, apdev):
    """DPP and PKEX with mismatching curve"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    dev[0].dpp_pkex_resp(2437, identifier="test", code="secret", curve="P-256")
    dev[1].dpp_pkex_init(identifier="test", code="secret", curve="P-384")
    wait_dpp_fail(dev[0], "Mismatching PKEX curve: peer=20 own=19")
    wait_dpp_fail(dev[1], "Peer indicated mismatching PKEX group - proposed 19")

def test_dpp_pkex_curve_mismatch_failure(dev, apdev):
    """DPP and PKEX with mismatching curve (local failure)"""
    run_dpp_pkex_curve_mismatch_failure(dev, apdev, "=dpp_pkex_rx_exchange_req")

def test_dpp_pkex_curve_mismatch_failure2(dev, apdev):
    """DPP and PKEX with mismatching curve (local failure 2)"""
    run_dpp_pkex_curve_mismatch_failure(dev, apdev,
                                        "dpp_pkex_build_exchange_resp")

def run_dpp_pkex_curve_mismatch_failure(dev, apdev, func):
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    dev[0].dpp_pkex_resp(2437, identifier="test", code="secret", curve="P-256")

    with alloc_fail(dev[0], 1, func):
        dev[1].dpp_pkex_init(identifier="test", code="secret", curve="P-384")

        ev = dev[0].wait_event(["DPP-FAIL"], timeout=5)
        if ev is None:
            raise Exception("Failure not reported (dev 0)")
        if "Mismatching PKEX curve: peer=20 own=19" not in ev:
            raise Exception("Unexpected result: " + ev)
        wait_dpp_fail(dev[0], "Mismatching PKEX curve: peer=20 own=19")

def test_dpp_pkex_exchange_resp_processing_failure(dev, apdev):
    """DPP and PKEX with local failure in processing Exchange Resp"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    dev[0].dpp_pkex_resp(2437, identifier="test", code="secret")

    with fail_test(dev[1], 1, "dpp_pkex_derive_Qr;dpp_pkex_rx_exchange_resp"):
        dev[1].dpp_pkex_init(identifier="test", code="secret")
        wait_fail_trigger(dev[1], "GET_FAIL")

def test_dpp_pkex_commit_reveal_req_processing_failure(dev, apdev):
    """DPP and PKEX with local failure in processing Commit Reveal Req"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    dev[0].dpp_pkex_resp(2437, identifier="test", code="secret")

    with alloc_fail(dev[0], 1,
                    "crypto_ec_key_get_pubkey_point;dpp_pkex_rx_commit_reveal_req"):
        dev[1].dpp_pkex_init(identifier="test", code="secret")
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")

def test_dpp_pkex_config2(dev, apdev):
    """DPP and PKEX with responder as the configurator"""
    check_dpp_capab(dev[0])
    conf_id = dev[0].dpp_configurator_add()
    dev[0].set("dpp_configurator_params",
               " conf=sta-dpp configurator=%d" % conf_id)
    run_dpp_pkex2(dev, apdev)

def run_dpp_pkex2(dev, apdev, curve=None, init_extra=""):
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    dev[0].dpp_pkex_resp(2437, identifier="test", code="secret", curve=curve,
                         listen_role="configurator")
    dev[1].dpp_pkex_init(identifier="test", code="secret", role="enrollee",
                         curve=curve, extra=init_extra)
    wait_auth_success(dev[0], dev[1], configurator=dev[0], enrollee=dev[1])

def test_dpp_pkex_no_responder(dev, apdev):
    """DPP and PKEX with no responder (retry behavior)"""
    check_dpp_capab(dev[0])
    dev[0].dpp_pkex_init(identifier="test", code="secret")

    for i in range(15):
        ev = dev[0].wait_event(["DPP-TX ", "DPP-FAIL"], timeout=5)
        if ev is None:
            raise Exception("DPP PKEX failure not reported")
        if "DPP-FAIL" not in ev:
            continue
        if "No response from PKEX peer" not in ev:
            raise Exception("Unexpected failure reason: " + ev)
        break

def test_dpp_pkex_after_retry(dev, apdev):
    """DPP and PKEX completing after retry"""
    check_dpp_capab(dev[0])
    dev[0].dpp_pkex_init(identifier="test", code="secret")
    time.sleep(0.1)
    dev[1].dpp_pkex_resp(2437, identifier="test", code="secret")
    wait_auth_success(dev[1], dev[0], configurator=dev[0], enrollee=dev[1],
                      allow_enrollee_failure=True)

def test_dpp_pkex_hostapd_responder(dev, apdev):
    """DPP PKEX with hostapd as responder"""
    check_dpp_capab(dev[0])
    hapd = hostapd.add_ap(apdev[0], {"ssid": "unconfigured",
                                     "channel": "6"})
    check_dpp_capab(hapd)
    hapd.dpp_pkex_resp(2437, identifier="test", code="secret")
    conf_id = dev[0].dpp_configurator_add()
    dev[0].dpp_pkex_init(identifier="test", code="secret",
                         extra="conf=ap-dpp configurator=%d" % conf_id)
    wait_auth_success(hapd, dev[0], configurator=dev[0], enrollee=hapd,
                      stop_initiator=True)

def test_dpp_pkex_v2_hostapd_responder(dev, apdev):
    """DPP PKEXv2 with hostapd as responder"""
    check_dpp_capab(dev[0], min_ver=3)
    hapd = hostapd.add_ap(apdev[0], {"ssid": "unconfigured",
                                     "channel": "6"})
    check_dpp_capab(hapd, min_ver=3)
    hapd.dpp_pkex_resp(2437, identifier="test", code="secret")
    conf_id = dev[0].dpp_configurator_add()
    dev[0].dpp_pkex_init(identifier="test", code="secret",
                         extra="conf=ap-dpp configurator=%d" % conf_id, v2=True)
    wait_auth_success(hapd, dev[0], configurator=dev[0], enrollee=hapd,
                      stop_initiator=True)

def test_dpp_pkex_hostapd_initiator(dev, apdev):
    """DPP PKEX with hostapd as initiator"""
    check_dpp_capab(dev[0])
    hapd = hostapd.add_ap(apdev[0], {"ssid": "unconfigured",
                                     "channel": "6"})
    check_dpp_capab(hapd)
    conf_id = dev[0].dpp_configurator_add()
    dev[0].set("dpp_configurator_params",
               " conf=ap-dpp configurator=%d" % conf_id)
    dev[0].dpp_pkex_resp(2437, identifier="test", code="secret",
                         listen_role="configurator")
    hapd.dpp_pkex_init(identifier="test", code="secret", role="enrollee")
    wait_auth_success(hapd, dev[0], configurator=dev[0], enrollee=hapd,
                      stop_initiator=True)

def test_dpp_pkex_v2_hostapd_initiator(dev, apdev):
    """DPP PKEXv2 with hostapd as initiator"""
    check_dpp_capab(dev[0], min_ver=3)
    hapd = hostapd.add_ap(apdev[0], {"ssid": "unconfigured",
                                     "channel": "6"})
    check_dpp_capab(hapd, min_ver=3)
    conf_id = dev[0].dpp_configurator_add()
    dev[0].set("dpp_configurator_params",
               " conf=ap-dpp configurator=%d" % conf_id)
    dev[0].dpp_pkex_resp(2437, identifier="test", code="secret",
                         listen_role="configurator")
    hapd.dpp_pkex_init(identifier="test", code="secret", role="enrollee",
                       v2=True)
    wait_auth_success(hapd, dev[0], configurator=dev[0], enrollee=hapd,
                      stop_initiator=True)

def test_dpp_pkex_hostapd_errors(dev, apdev):
    """DPP PKEX errors with hostapd"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": "unconfigured",
                                     "channel": "6"})
    check_dpp_capab(hapd)
    id0 = hapd.dpp_bootstrap_gen(type="pkex")
    tests = ["own=%d" % id0,
             "own=%d identifier=foo" % id0,
             ""]
    for t in tests:
        if "FAIL" not in hapd.request("DPP_PKEX_ADD " + t):
            raise Exception("Invalid DPP_PKEX_ADD accepted: " + t)

    res = hapd.request("DPP_PKEX_ADD own=%d code=foo" % id0)
    if "FAIL" in res:
        raise Exception("Failed to add PKEX responder")
    if "OK" not in hapd.request("DPP_PKEX_REMOVE " + res):
        raise Exception("Failed to remove PKEX responder")
    if "FAIL" not in hapd.request("DPP_PKEX_REMOVE " + res):
        raise Exception("Unknown PKEX responder removal accepted")

    res = hapd.request("DPP_PKEX_ADD own=%d code=foo" % id0)
    if "FAIL" in res:
        raise Exception("Failed to add PKEX responder")
    if "OK" not in hapd.request("DPP_PKEX_REMOVE *"):
        raise Exception("Failed to flush PKEX responders")
    hapd.request("DPP_PKEX_REMOVE *")

def test_dpp_hostapd_configurator(dev, apdev):
    """DPP with hostapd as configurator/initiator"""
    run_dpp_hostapd_configurator(dev, apdev)

def test_dpp_hostapd_configurator_enrollee_v1(dev, apdev):
    """DPP with hostapd as configurator/initiator with v1 enrollee"""
    dev[0].set("dpp_version_override", "1")
    run_dpp_hostapd_configurator(dev, apdev)

def run_dpp_hostapd_configurator(dev, apdev):
    check_dpp_capab(dev[0])
    hapd = hostapd.add_ap(apdev[0], {"ssid": "unconfigured",
                                     "channel": "1"})
    check_dpp_capab(hapd)
    conf_id = hapd.dpp_configurator_add()
    id0 = dev[0].dpp_bootstrap_gen(chan="81/1", mac=True)
    uri0 = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id0)
    id1 = hapd.dpp_qr_code(uri0)
    res = hapd.request("DPP_BOOTSTRAP_INFO %d" % id1)
    if "FAIL" in res:
        raise Exception("DPP_BOOTSTRAP_INFO failed")
    if "type=QRCODE" not in res:
        raise Exception("DPP_BOOTSTRAP_INFO did not report correct type")
    if "mac_addr=" + dev[0].own_addr() not in res:
        raise Exception("DPP_BOOTSTRAP_INFO did not report correct mac_addr")
    dev[0].dpp_listen(2412)
    hapd.dpp_auth_init(peer=id1, configurator=conf_id, conf="sta-dpp")
    wait_auth_success(dev[0], hapd, configurator=hapd, enrollee=dev[0],
                      stop_responder=True)

def test_dpp_hostapd_configurator_responder(dev, apdev):
    """DPP with hostapd as configurator/responder"""
    check_dpp_capab(dev[0])
    hapd = hostapd.add_ap(apdev[0], {"ssid": "unconfigured",
                                     "channel": "1"})
    check_dpp_capab(hapd)
    conf_id = hapd.dpp_configurator_add()
    hapd.set("dpp_configurator_params",
             " conf=sta-dpp configurator=%d" % conf_id)
    id0 = hapd.dpp_bootstrap_gen(chan="81/1", mac=True)
    uri0 = hapd.request("DPP_BOOTSTRAP_GET_URI %d" % id0)
    dev[0].dpp_auth_init(uri=uri0, role="enrollee")
    wait_auth_success(hapd, dev[0], configurator=hapd, enrollee=dev[0],
                      stop_initiator=True)

def test_dpp_hostapd_configurator_fragmentation(dev, apdev):
    """DPP with hostapd as configurator/initiator requiring fragmentation"""
    check_dpp_capab(dev[0])
    hapd = hostapd.add_ap(apdev[0], {"ssid": "unconfigured",
                                     "channel": "1"})
    check_dpp_capab(hapd)
    conf_id = hapd.dpp_configurator_add()
    id0 = dev[0].dpp_bootstrap_gen(chan="81/1", mac=True)
    uri0 = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id0)
    id1 = hapd.dpp_qr_code(uri0)
    res = hapd.request("DPP_BOOTSTRAP_INFO %d" % id1)
    if "FAIL" in res:
        raise Exception("DPP_BOOTSTRAP_INFO failed")
    if "type=QRCODE" not in res:
        raise Exception("DPP_BOOTSTRAP_INFO did not report correct type")
    if "mac_addr=" + dev[0].own_addr() not in res:
        raise Exception("DPP_BOOTSTRAP_INFO did not report correct mac_addr")
    dev[0].dpp_listen(2412)
    conf = '{"wi-fi_tech":"infra", "discovery":{"ssid":"test"},"cred":{"akm":"psk","pass":"secret passphrase"}}' + 3000*' '
    hapd.set("dpp_config_obj_override", conf)
    hapd.dpp_auth_init(peer=id1, configurator=conf_id, conf="sta-dpp")
    wait_auth_success(dev[0], hapd, configurator=hapd, enrollee=dev[0],
                      stop_responder=True)

def test_dpp_hostapd_enrollee_fragmentation(dev, apdev):
    """DPP and hostapd as Enrollee with GAS fragmentation"""
    check_dpp_capab(dev[0])
    hapd = hostapd.add_ap(apdev[0], {"ssid": "unconfigured",
                                     "channel": "6"})
    check_dpp_capab(hapd)
    conf_id = dev[0].dpp_configurator_add()
    id0 = dev[0].dpp_bootstrap_gen(chan="81/6", mac=True)
    uri0 = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id0)
    conf = '{"wi-fi_tech":"infra", "discovery":{"ssid":"test"},"cred":{"akm":"psk","pass":"secret passphrase"}}' + 3000*' '
    dev[0].set("dpp_config_obj_override", conf)
    dev[0].set("dpp_configurator_params",
               " conf=ap-dpp configurator=%d" % conf_id)
    dev[0].dpp_listen(2437, role="configurator")
    hapd.dpp_auth_init(uri=uri0, role="enrollee")
    wait_auth_success(dev[0], hapd, configurator=dev[0], enrollee=hapd,
                      stop_responder=True)

def test_dpp_hostapd_enrollee_gas_timeout(dev, apdev):
    """DPP and hostapd as Enrollee with GAS timeout"""
    check_dpp_capab(dev[0])
    hapd = hostapd.add_ap(apdev[0], {"ssid": "unconfigured",
                                     "channel": "6"})
    check_dpp_capab(hapd)
    conf_id = dev[0].dpp_configurator_add()
    id0 = dev[0].dpp_bootstrap_gen(chan="81/6", mac=True)
    uri0 = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id0)
    conf = '{"wi-fi_tech":"infra", "discovery":{"ssid":"test"},"cred":{"akm":"psk","pass":"secret passphrase"}}' + 3000*' '
    dev[0].set("dpp_config_obj_override", conf)
    dev[0].set("dpp_configurator_params",
               "conf=ap-dpp configurator=%d" % conf_id)
    dev[0].set("ext_mgmt_frame_handling", "1")
    dev[0].dpp_listen(2437, role="configurator")
    hapd.dpp_auth_init(uri=uri0, role="enrollee")
    process_dpp_frames(dev[0])
    ev = hapd.wait_event(["GAS-QUERY-DONE"], timeout=10)
    if "result=TIMEOUT" not in ev:
        raise Exception("GAS timeout not reported")

def test_dpp_hostapd_enrollee_gas_timeout_comeback(dev, apdev):
    """DPP and hostapd as Enrollee with GAS timeout during comeback"""
    check_dpp_capab(dev[0])
    hapd = hostapd.add_ap(apdev[0], {"ssid": "unconfigured",
                                     "channel": "6"})
    check_dpp_capab(hapd)
    conf_id = dev[0].dpp_configurator_add()
    id0 = dev[0].dpp_bootstrap_gen(chan="81/6", mac=True)
    uri0 = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id0)
    conf = '{"wi-fi_tech":"infra", "discovery":{"ssid":"test"},"cred":{"akm":"psk","pass":"secret passphrase"}}' + 3000*' '
    dev[0].set("dpp_config_obj_override", conf)
    dev[0].set("dpp_configurator_params",
               "conf=ap-dpp configurator=%d" % conf_id)
    dev[0].set("ext_mgmt_frame_handling", "1")
    dev[0].dpp_listen(2437, role="configurator")
    hapd.dpp_auth_init(uri=uri0, role="enrollee")
    process_dpp_frames(dev[0], count=4)
    ev = hapd.wait_event(["GAS-QUERY-DONE"], timeout=10)
    if "result=TIMEOUT" not in ev:
        raise Exception("GAS timeout not reported")

def process_dpp_frames(dev, count=3):
    for i in range(count):
        msg = dev.mgmt_rx()
        cmd = "MGMT_RX_PROCESS freq={} datarate={} ssi_signal={} frame={}".format(msg['freq'], msg['datarate'], msg['ssi_signal'], binascii.hexlify(msg['frame']).decode())
        if "OK" not in dev.request(cmd):
            raise Exception("MGMT_RX_PROCESS failed")

def test_dpp_hostapd_enrollee_gas_errors(dev, apdev):
    """DPP and hostapd as Enrollee with GAS query local errors"""
    check_dpp_capab(dev[0])
    hapd = hostapd.add_ap(apdev[0], {"ssid": "unconfigured",
                                     "channel": "6"})
    check_dpp_capab(hapd)
    conf_id = dev[0].dpp_configurator_add()
    id0 = dev[0].dpp_bootstrap_gen(chan="81/6", mac=True)
    uri0 = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id0)
    dev[0].set("dpp_configurator_params",
               "conf=ap-dpp configurator=%d" % conf_id)
    dev[0].set("ext_mgmt_frame_handling", "1")

    # GAS without comeback
    tests = [(1, "gas_query_append;gas_query_rx_initial", 3, True),
             (1, "gas_query_rx_initial", 3, True),
             (1, "gas_query_tx_initial_req", 2, True),
             (1, "gas_query_ap_req", 2, False)]
    for count, func, frame_count, wait_ev in tests:
        dev[0].request("DPP_STOP_LISTEN")
        dev[0].dpp_listen(2437, role="configurator")
        dev[0].dump_monitor()
        hapd.dump_monitor()
        with alloc_fail(hapd, count, func):
            hapd.dpp_auth_init(uri=uri0, role="enrollee")
            process_dpp_frames(dev[0], count=frame_count)
            if wait_ev:
                ev = hapd.wait_event(["GAS-QUERY-DONE"], timeout=10)
                if not ev or "result=INTERNAL_ERROR" not in ev:
                    raise Exception("Unexpect GAS query result: " + str(ev))

    # GAS with comeback
    conf = '{"wi-fi_tech":"infra", "discovery":{"ssid":"test"},"cred":{"akm":"psk","pass":"secret passphrase"}}' + 3000*' '
    dev[0].set("dpp_config_obj_override", conf)

    tests = [(1, "gas_query_append;gas_query_rx_comeback", 4),
             (1, "wpabuf_alloc;gas_query_tx_comeback_req", 3),
             (1, "hostapd_drv_send_action;gas_query_tx_comeback_req", 3)]
    for count, func, frame_count in tests:
        dev[0].request("DPP_STOP_LISTEN")
        dev[0].dpp_listen(2437, role="configurator")
        dev[0].dump_monitor()
        hapd.dump_monitor()
        with alloc_fail(hapd, count, func):
            hapd.dpp_auth_init(uri=uri0, role="enrollee")
            process_dpp_frames(dev[0], count=frame_count)
            ev = hapd.wait_event(["GAS-QUERY-DONE"], timeout=10)
            if not ev or "result=INTERNAL_ERROR" not in ev:
                raise Exception("Unexpect GAS query result: " + str(ev))

def test_dpp_hostapd_enrollee_gas_proto(dev, apdev):
    """DPP and hostapd as Enrollee with GAS query protocol testing"""
    check_dpp_capab(dev[0])
    hapd = hostapd.add_ap(apdev[0], {"ssid": "unconfigured",
                                     "channel": "6"})
    check_dpp_capab(hapd)
    bssid = hapd.own_addr()
    conf_id = dev[0].dpp_configurator_add()
    id0 = dev[0].dpp_bootstrap_gen(chan="81/6", mac=True)
    uri0 = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id0)
    dev[0].set("dpp_configurator_params",
               "conf=ap-dpp configurator=%d" % conf_id)
    conf = '{"wi-fi_tech":"infra", "discovery":{"ssid":"test"},"cred":{"akm":"psk","pass":"secret passphrase"}}' + 3000*' '
    dev[0].set("dpp_config_obj_override", conf)
    dev[0].set("ext_mgmt_frame_handling", "1")
    dev[0].dpp_listen(2437, role="configurator")
    hapd.dpp_auth_init(uri=uri0, role="enrollee")
    process_dpp_frames(dev[0], count=3)
    msg = dev[0].mgmt_rx()
    payload = msg['payload']
    dialog_token, = struct.unpack('B', payload[2:3])
    hdr = struct.pack('<BBBHBH', 4, 13, dialog_token, 0, 0x80, 0)
    # GAS: Advertisement Protocol changed between initial and comeback response from 02:00:00:00:00:00
    adv_proto = "6c087fdd05506f9a1a02"
    action = binascii.hexlify(hdr).decode() + adv_proto + "0300" + "001001"
    cmd = "MGMT_TX %s %s freq=2437 wait_time=100 action=%s" % (bssid, bssid, action)
    dev[0].request(cmd)
    ev = hapd.wait_event(["GAS-QUERY-DONE"], timeout=10)
    if not ev or "result=PEER_ERROR" not in ev:
        raise Exception("Unexpect GAS query result: " + str(ev))
    dev[0].request("DPP_STOP_LISTEN")
    hapd.dump_monitor()
    dev[0].dump_monitor()

    dev[0].dpp_listen(2437, role="configurator")
    hapd.dpp_auth_init(uri=uri0, role="enrollee")
    process_dpp_frames(dev[0], count=3)
    msg = dev[0].mgmt_rx()
    payload = msg['payload']
    dialog_token, = struct.unpack('B', payload[2:3])
    # Another comeback delay
    hdr = struct.pack('<BBBHBH', 4, 13, dialog_token, 0, 0x80, 1)
    adv_proto = "6c087fdd05506f9a1a01"
    action = binascii.hexlify(hdr).decode() + adv_proto + "0300" + "001001"
    cmd = "MGMT_TX %s %s freq=2437 wait_time=100 action=%s" % (bssid, bssid, action)
    dev[0].request(cmd)
    msg = dev[0].mgmt_rx()
    hdr = struct.pack('<BBBHBH', 4, 13, dialog_token, 0, 0x81, 1)
    # GAS: Invalid comeback response with non-zero frag_id and comeback_delay from 02:00:00:00:00:00
    action = binascii.hexlify(hdr).decode() + adv_proto + "0300" + "001001"
    cmd = "MGMT_TX %s %s freq=2437 wait_time=100 action=%s" % (bssid, bssid, action)
    dev[0].request(cmd)
    ev = hapd.wait_event(["GAS-QUERY-DONE"], timeout=10)
    if not ev or "result=PEER_ERROR" not in ev:
        raise Exception("Unexpect GAS query result: " + str(ev))
    dev[0].request("DPP_STOP_LISTEN")
    hapd.dump_monitor()
    dev[0].dump_monitor()

    dev[0].dpp_listen(2437, role="configurator")
    hapd.dpp_auth_init(uri=uri0, role="enrollee")
    process_dpp_frames(dev[0], count=3)
    msg = dev[0].mgmt_rx()
    payload = msg['payload']
    dialog_token, = struct.unpack('B', payload[2:3])
    # Valid comeback response
    hdr = struct.pack('<BBBHBH', 4, 13, dialog_token, 0, 0x80, 0)
    action = binascii.hexlify(hdr).decode() + adv_proto + "0300" + "001001"
    cmd = "MGMT_TX %s %s freq=2437 wait_time=100 action=%s" % (bssid, bssid, action)
    dev[0].request(cmd)
    msg = dev[0].mgmt_rx()
    # GAS: Drop frame as possible retry of previous fragment
    hdr = struct.pack('<BBBHBH', 4, 13, dialog_token, 0, 0x80, 0)
    action = binascii.hexlify(hdr).decode() + adv_proto + "0300" + "001001"
    cmd = "MGMT_TX %s %s freq=2437 wait_time=100 action=%s" % (bssid, bssid, action)
    dev[0].request(cmd)
    # GAS: Unexpected frag_id in response from 02:00:00:00:00:00
    hdr = struct.pack('<BBBHBH', 4, 13, dialog_token, 0, 0x82, 0)
    action = binascii.hexlify(hdr).decode() + adv_proto + "0300" + "001001"
    cmd = "MGMT_TX %s %s freq=2437 wait_time=100 action=%s" % (bssid, bssid, action)
    dev[0].request(cmd)
    ev = hapd.wait_event(["GAS-QUERY-DONE"], timeout=10)
    if not ev or "result=PEER_ERROR" not in ev:
        raise Exception("Unexpect GAS query result: " + str(ev))
    dev[0].request("DPP_STOP_LISTEN")
    hapd.dump_monitor()
    dev[0].dump_monitor()

    dev[0].dpp_listen(2437, role="configurator")
    hapd.dpp_auth_init(uri=uri0, role="enrollee")
    process_dpp_frames(dev[0], count=3)
    msg = dev[0].mgmt_rx()
    payload = msg['payload']
    dialog_token, = struct.unpack('B', payload[2:3])
    # GAS: Unexpected initial response from 02:00:00:00:00:00 dialog token 3 when waiting for comeback response
    hdr = struct.pack('<BBBHBH', 4, 11, dialog_token, 0, 0x80, 0)
    action = binascii.hexlify(hdr).decode() + adv_proto + "0300" + "001001"
    cmd = "MGMT_TX %s %s freq=2437 wait_time=100 action=%s" % (bssid, bssid, action)
    dev[0].request(cmd)
    # GAS: Allow non-zero status for outstanding comeback response
    hdr = struct.pack('<BBBHBH', 4, 13, dialog_token, 95, 0x80, 0)
    # GAS: Ignore 1 octets of extra data after Query Response from 02:00:00:00:00:00
    action = binascii.hexlify(hdr).decode() + adv_proto + "0300" + "001001" + "ff"
    cmd = "MGMT_TX %s %s freq=2437 wait_time=100 action=%s" % (bssid, bssid, action)
    dev[0].request(cmd)
    # GAS: No pending query found for 02:00:00:00:00:00 dialog token 4
    hdr = struct.pack('<BBBHBH', 4, 13, (dialog_token + 1) % 256, 0, 0x80, 0)
    action = binascii.hexlify(hdr).decode() + adv_proto + "0300" + "001001"
    cmd = "MGMT_TX %s %s freq=2437 wait_time=100 action=%s" % (bssid, bssid, action)
    dev[0].request(cmd)
    # GAS: Truncated Query Response in response from 02:00:00:00:00:00
    hdr = struct.pack('<BBBHBH', 4, 13, dialog_token, 0, 0x81, 0)
    action = binascii.hexlify(hdr).decode() + adv_proto + "0300" + "0010"
    cmd = "MGMT_TX %s %s freq=2437 wait_time=100 action=%s" % (bssid, bssid, action)
    dev[0].request(cmd)
    # GAS: No room for GAS Response Length
    hdr = struct.pack('<BBBHBH', 4, 13, dialog_token, 0, 0x81, 0)
    action = binascii.hexlify(hdr).decode() + adv_proto + "03"
    cmd = "MGMT_TX %s %s freq=2437 wait_time=100 action=%s" % (bssid, bssid, action)
    dev[0].request(cmd)
    # GAS: Unexpected Advertisement Protocol element ID 0 in response from 02:00:00:00:00:00
    hdr = struct.pack('<BBBHBH', 4, 13, dialog_token, 0, 0x81, 0)
    adv_proto_broken = "0000"
    action = binascii.hexlify(hdr).decode() + adv_proto_broken + "0300" + "001001"
    cmd = "MGMT_TX %s %s freq=2437 wait_time=100 action=%s" % (bssid, bssid, action)
    dev[0].request(cmd)
    # GAS: No room for Advertisement Protocol element in the response from 02:00:00:00:00:00
    hdr = struct.pack('<BBBHBH', 4, 13, dialog_token, 0, 0x81, 0)
    adv_proto_broken = "00ff"
    action = binascii.hexlify(hdr).decode() + adv_proto_broken + "0300" + "001001"
    cmd = "MGMT_TX %s %s freq=2437 wait_time=100 action=%s" % (bssid, bssid, action)
    dev[0].request(cmd)
    # No room for Comeback Delay
    hdr = struct.pack('<BBBHBB', 4, 13, dialog_token, 0, 0x81, 0)
    action = binascii.hexlify(hdr).decode()
    cmd = "MGMT_TX %s %s freq=2437 wait_time=100 action=%s" % (bssid, bssid, action)
    dev[0].request(cmd)
    # No room for frag_id
    hdr = struct.pack('<BBBH', 4, 13, dialog_token, 0)
    action = binascii.hexlify(hdr).decode()
    cmd = "MGMT_TX %s %s freq=2437 wait_time=100 action=%s" % (bssid, bssid, action)
    dev[0].request(cmd)
    # GAS: Query to 02:00:00:00:00:00 dialog token 3 failed - status code 1
    hdr = struct.pack('<BBBHBH', 4, 13, dialog_token, 1, 0x81, 0)
    action = binascii.hexlify(hdr).decode() + adv_proto + "0300" + "001001"
    cmd = "MGMT_TX %s %s freq=2437 wait_time=100 action=%s" % (bssid, bssid, action)
    dev[0].request(cmd)
    ev = hapd.wait_event(["GAS-QUERY-DONE"], timeout=10)
    if not ev or "result=FAILURE" not in ev:
        raise Exception("Unexpect GAS query result: " + str(ev))
    dev[0].request("DPP_STOP_LISTEN")
    hapd.dump_monitor()
    dev[0].dump_monitor()

    dev[0].dpp_listen(2437, role="configurator")
    hapd.dpp_auth_init(uri=uri0, role="enrollee")
    process_dpp_frames(dev[0], count=2)
    msg = dev[0].mgmt_rx()
    payload = msg['payload']
    dialog_token, = struct.unpack('B', payload[2:3])
    # Unexpected comeback delay
    hdr = struct.pack('<BBBHBH', 4, 13, dialog_token, 0, 0x80, 0)
    adv_proto = "6c087fdd05506f9a1a01"
    action = binascii.hexlify(hdr).decode() + adv_proto + "0300" + "001001"
    cmd = "MGMT_TX %s %s freq=2437 wait_time=100 action=%s" % (bssid, bssid, action)
    dev[0].request(cmd)
    # GAS: Query to 02:00:00:00:00:00 dialog token 3 failed - status code 1
    hdr = struct.pack('<BBBHBH', 4, 11, dialog_token, 1, 0x80, 0)
    action = binascii.hexlify(hdr).decode() + adv_proto + "0300" + "001001"
    cmd = "MGMT_TX %s %s freq=2437 wait_time=100 action=%s" % (bssid, bssid, action)
    dev[0].request(cmd)
    ev = hapd.wait_event(["GAS-QUERY-DONE"], timeout=10)
    if not ev or "result=FAILURE" not in ev:
        raise Exception("Unexpect GAS query result: " + str(ev))
    dev[0].request("DPP_STOP_LISTEN")
    hapd.dump_monitor()
    dev[0].dump_monitor()

def test_dpp_hostapd_enrollee_gas_tx_status_errors(dev, apdev):
    """DPP and hostapd as Enrollee with GAS TX status errors"""
    check_dpp_capab(dev[0])
    hapd = hostapd.add_ap(apdev[0], {"ssid": "unconfigured",
                                     "channel": "6"})
    check_dpp_capab(hapd)
    conf_id = dev[0].dpp_configurator_add()
    id0 = dev[0].dpp_bootstrap_gen(chan="81/6", mac=True)
    uri0 = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id0)
    conf = '{"wi-fi_tech":"infra", "discovery":{"ssid":"test"},"cred":{"akm":"psk","pass":"secret passphrase"}}' + 3000*' '
    dev[0].set("dpp_config_obj_override", conf)
    dev[0].set("dpp_configurator_params",
               "conf=ap-dpp configurator=%d" % conf_id)
    dev[0].set("ext_mgmt_frame_handling", "1")
    dev[0].dpp_listen(2437, role="configurator")
    hapd.dpp_auth_init(uri=uri0, role="enrollee")
    process_dpp_frames(dev[0], count=3)

    hapd.set("ext_mgmt_frame_handling", "1")
    # GAS: TX status for unexpected destination
    frame = "d0003a01" + "222222222222"
    frame += hapd.own_addr().replace(':', '') + "ffffffffffff"
    frame += "5000" + "040a"
    hapd.request("MGMT_TX_STATUS_PROCESS stype=13 ok=1 buf=" + frame)

    # GAS: No ACK to GAS request
    frame = "d0003a01" + dev[0].own_addr().replace(':', '')
    frame += hapd.own_addr().replace(':', '') + "ffffffffffff"
    frame += "5000" + "040a"
    hapd.request("MGMT_TX_STATUS_PROCESS stype=13 ok=0 buf=" + frame)

    ev = hapd.wait_event(["GAS-QUERY-DONE"], timeout=10)
    if "result=TIMEOUT" not in ev:
        raise Exception("GAS timeout not reported")

    # GAS: Unexpected TX status: dst=02:00:00:00:00:00 ok=1 - no query in progress
    hapd.request("MGMT_TX_STATUS_PROCESS stype=13 ok=1 buf=" + frame)
    hapd.set("ext_mgmt_frame_handling", "0")

def test_dpp_hostapd_configurator_override_objects(dev, apdev):
    """DPP with hostapd as configurator and override objects"""
    check_dpp_capab(dev[0])
    hapd = hostapd.add_ap(apdev[0], {"ssid": "unconfigured",
                                     "channel": "1"})
    check_dpp_capab(hapd)
    conf_id = hapd.dpp_configurator_add()
    id0 = dev[0].dpp_bootstrap_gen(chan="81/1", mac=True)
    uri0 = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id0)
    id1 = hapd.dpp_qr_code(uri0)
    res = hapd.request("DPP_BOOTSTRAP_INFO %d" % id1)
    if "FAIL" in res:
        raise Exception("DPP_BOOTSTRAP_INFO failed")
    dev[0].dpp_listen(2412)
    discovery = '{\n"ssid":"mywifi"\n}'
    groups = '[\n  {"groupId":"home","netRole":"sta"},\n  {"groupId":"cottage","netRole":"sta"}\n]'
    hapd.set("dpp_discovery_override", discovery)
    hapd.set("dpp_groups_override", groups)
    hapd.dpp_auth_init(peer=id1, configurator=conf_id, conf="sta-dpp")
    wait_auth_success(dev[0], hapd, configurator=hapd, enrollee=dev[0],
                      stop_responder=True)

def test_dpp_own_config(dev, apdev):
    """DPP configurator signing own connector"""
    try:
        run_dpp_own_config(dev, apdev)
    finally:
        dev[0].set("dpp_config_processing", "0", allow_fail=True)

def test_dpp_own_config_group_id(dev, apdev):
    """DPP configurator signing own connector"""
    try:
        run_dpp_own_config(dev, apdev, extra=" group_id=test-group")
    finally:
        dev[0].set("dpp_config_processing", "0", allow_fail=True)

def test_dpp_own_config_curve_mismatch(dev, apdev):
    """DPP configurator signing own connector using mismatching curve"""
    try:
        run_dpp_own_config(dev, apdev, own_curve="BP-384", expect_failure=True)
    finally:
        dev[0].set("dpp_config_processing", "0", allow_fail=True)

def run_dpp_own_config(dev, apdev, own_curve=None, expect_failure=False,
                       extra=None):
    check_dpp_capab(dev[0], own_curve and "BP" in own_curve)
    hapd = hostapd.add_ap(apdev[0], {"ssid": "unconfigured"})
    check_dpp_capab(hapd)
    id_h = hapd.dpp_bootstrap_gen(chan="81/1", mac=True)
    uri = hapd.request("DPP_BOOTSTRAP_GET_URI %d" % id_h)
    conf_id = dev[0].dpp_configurator_add()
    dev[0].dpp_auth_init(uri=uri, conf="ap-dpp", configurator=conf_id,
                         extra=extra)
    wait_auth_success(hapd, dev[0], configurator=dev[0], enrollee=hapd)
    update_hapd_config(hapd)

    dev[0].set("dpp_config_processing", "1")
    cmd = "DPP_CONFIGURATOR_SIGN conf=sta-dpp configurator=%d%s" % (conf_id, extra)
    if own_curve:
        cmd += " curve=" + own_curve
    res = dev[0].request(cmd)
    if "FAIL" in res:
        raise Exception("Failed to generate own configuration")

    ev = dev[0].wait_event(["DPP-NETWORK-ID"], timeout=1)
    if ev is None:
        raise Exception("DPP network profile not generated")
    id = ev.split(' ')[1]
    dev[0].select_network(id, freq="2412")
    if expect_failure:
        ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED"], timeout=1)
        if ev is not None:
            raise Exception("Unexpected connection")
        dev[0].request("DISCONNECT")
    else:
        dev[0].wait_connected()

def test_dpp_own_config_ap(dev, apdev):
    """DPP configurator (AP) signing own connector"""
    try:
        run_dpp_own_config_ap(dev, apdev)
    finally:
        dev[0].set("dpp_config_processing", "0", allow_fail=True)

def test_dpp_own_config_ap_group_id(dev, apdev):
    """DPP configurator (AP) signing own connector (group_id)"""
    try:
        run_dpp_own_config_ap(dev, apdev, extra=" group_id=test-group")
    finally:
        dev[0].set("dpp_config_processing", "0", allow_fail=True)

def test_dpp_own_config_ap_reconf(dev, apdev):
    """DPP configurator (AP) signing own connector and configurator reconf"""
    try:
        run_dpp_own_config_ap(dev, apdev)
    finally:
        dev[0].set("dpp_config_processing", "0", allow_fail=True)

def run_dpp_own_config_ap(dev, apdev, reconf_configurator=False, extra=None):
    check_dpp_capab(dev[0])
    hapd = hostapd.add_ap(apdev[0], {"ssid": "unconfigured"})
    check_dpp_capab(hapd)
    conf_id = hapd.dpp_configurator_add()
    if reconf_configurator:
        csign = hapd.request("DPP_CONFIGURATOR_GET_KEY %d" % conf_id)
        if "FAIL" in csign or len(csign) == 0:
            raise Exception("DPP_CONFIGURATOR_GET_KEY failed")

    cmd = "DPP_CONFIGURATOR_SIGN conf=ap-dpp configurator=%d%s" % (conf_id, extra)
    res = hapd.request(cmd)
    if "FAIL" in res:
        raise Exception("Failed to generate own configuration")
    update_hapd_config(hapd)

    if reconf_configurator:
        hapd.dpp_configurator_remove(conf_id)
        conf_id = hapd.dpp_configurator_add(key=csign)

    id = dev[0].dpp_bootstrap_gen(chan="81/1", mac=True)
    uri = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id)
    dev[0].set("dpp_config_processing", "2")
    dev[0].dpp_listen(2412)
    hapd.dpp_auth_init(uri=uri, conf="sta-dpp", configurator=conf_id,
                       extra=extra)
    wait_auth_success(dev[0], hapd, configurator=hapd, enrollee=dev[0])
    dev[0].wait_connected()

def test_dpp_intro_mismatch(dev, apdev):
    """DPP network introduction mismatch cases"""
    try:
        wpas = None
        wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
        wpas.interface_add("wlan5")
        check_dpp_capab(wpas)
        run_dpp_intro_mismatch(dev, apdev, wpas)
    finally:
        dev[0].set("dpp_config_processing", "0", allow_fail=True)
        dev[2].set("dpp_config_processing", "0", allow_fail=True)
        if wpas:
            wpas.set("dpp_config_processing", "0", allow_fail=True)

def run_dpp_intro_mismatch(dev, apdev, wpas):
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    check_dpp_capab(dev[2])
    logger.info("Start AP in unconfigured state")
    hapd = hostapd.add_ap(apdev[0], {"ssid": "unconfigured"})
    check_dpp_capab(hapd)
    id_h = hapd.dpp_bootstrap_gen(chan="81/1", mac=True)
    uri = hapd.request("DPP_BOOTSTRAP_GET_URI %d" % id_h)
    logger.info("Provision AP with DPP configuration")
    conf_id = dev[1].dpp_configurator_add()
    dev[1].set("dpp_groups_override", '[{"groupId":"a","netRole":"ap"}]')
    dev[1].dpp_auth_init(uri=uri, conf="ap-dpp", configurator=conf_id)
    update_hapd_config(hapd)

    logger.info("Provision STA0 with DPP Connector that has mismatching groupId")
    dev[0].set("dpp_config_processing", "2")
    id0 = dev[0].dpp_bootstrap_gen(chan="81/1", mac=True)
    uri0 = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id0)
    dev[0].dpp_listen(2412)
    dev[1].set("dpp_groups_override", '[{"groupId":"b","netRole":"sta"}]')
    dev[1].dpp_auth_init(uri=uri0, conf="sta-dpp", configurator=conf_id)
    wait_auth_success(dev[0], dev[1], configurator=dev[1], enrollee=dev[0])

    logger.info("Provision STA2 with DPP Connector that has mismatching C-sign-key")
    dev[2].set("dpp_config_processing", "2")
    id2 = dev[2].dpp_bootstrap_gen(chan="81/1", mac=True)
    uri2 = dev[2].request("DPP_BOOTSTRAP_GET_URI %d" % id2)
    dev[2].dpp_listen(2412)
    conf_id_2 = dev[1].dpp_configurator_add()
    dev[1].set("dpp_groups_override", '')
    dev[1].dpp_auth_init(uri=uri2, conf="sta-dpp", configurator=conf_id_2)
    wait_auth_success(dev[2], dev[1], configurator=dev[1], enrollee=dev[2])

    logger.info("Provision STA5 with DPP Connector that has mismatching netAccessKey EC group")
    wpas.set("dpp_config_processing", "2")
    id5 = wpas.dpp_bootstrap_gen(chan="81/1", mac=True, curve="P-521")
    uri5 = wpas.request("DPP_BOOTSTRAP_GET_URI %d" % id5)
    wpas.dpp_listen(2412)
    dev[1].set("dpp_groups_override", '')
    dev[1].dpp_auth_init(uri=uri5, conf="sta-dpp", configurator=conf_id)
    wait_auth_success(wpas, dev[1], configurator=dev[1], enrollee=wpas)

    logger.info("Verify network introduction results")
    ev = dev[0].wait_event(["DPP-INTRO"], timeout=10)
    if ev is None:
        raise Exception("DPP network introduction result not seen on STA0")
    if "status=8" not in ev:
        raise Exception("Unexpected network introduction result on STA0: " + ev)

    ev = dev[2].wait_event(["DPP-INTRO"], timeout=5)
    if ev is None:
        raise Exception("DPP network introduction result not seen on STA2")
    if "status=8" not in ev:
        raise Exception("Unexpected network introduction result on STA2: " + ev)

    ev = wpas.wait_event(["DPP-INTRO"], timeout=10)
    if ev is None:
        raise Exception("DPP network introduction result not seen on STA5")
    if "status=7" not in ev:
        raise Exception("Unexpected network introduction result on STA5: " + ev)

def run_dpp_proto_init(dev, test_dev, test, mutual=False, unicast=True,
                       listen=True, chan="81/1", init_enrollee=False,
                       incompatible_roles=False):
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    dev[test_dev].set("dpp_test", str(test))
    if init_enrollee:
        conf_id = dev[0].dpp_configurator_add()
    else:
        conf_id = dev[1].dpp_configurator_add()
    id0 = dev[0].dpp_bootstrap_gen(chan=chan, mac=unicast)
    uri0 = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id0)

    if mutual:
        id1b = dev[1].dpp_bootstrap_gen(chan="81/1", mac=True)
        uri1b = dev[1].request("DPP_BOOTSTRAP_GET_URI %d" % id1b)

        id0b = dev[0].dpp_qr_code(uri1b)
        qr = "mutual"
    else:
        qr = None

    if init_enrollee:
        if incompatible_roles:
            role = "enrollee"
        else:
            role = "configurator"
        dev[0].set("dpp_configurator_params",
                   " conf=sta-dpp configurator=%d" % conf_id)
    elif incompatible_roles:
        role = "enrollee"
    else:
        role = None

    if listen:
        dev[0].dpp_listen(2412, qr=qr, role=role)

    role = None
    configurator = None
    conf = None
    own = None

    if init_enrollee:
        role="enrollee"
    else:
        configurator=conf_id
        conf="sta-dpp"
        if incompatible_roles:
            role="enrollee"
    if mutual:
        own = id1b
    dev[1].dpp_auth_init(uri=uri0, role=role, configurator=configurator,
                         conf=conf, own=own)
    return uri0, role, configurator, conf, own

def test_dpp_proto_after_wrapped_data_auth_req(dev, apdev):
    """DPP protocol testing - attribute after Wrapped Data in Auth Req"""
    run_dpp_proto_init(dev, 1, 1)
    ev = dev[0].wait_event(["DPP-RX"], timeout=5)
    if ev is None:
        raise Exception("DPP Authentication Request not seen")
    if "type=0" not in ev or "ignore=invalid-attributes" not in ev:
        raise Exception("Unexpected RX info: " + ev)
    ev = dev[1].wait_event(["DPP-RX"], timeout=0.1)
    if ev is not None:
        raise Exception("Unexpected DPP message seen")

def test_dpp_auth_req_stop_after_ack(dev, apdev):
    """DPP initiator stopping after ACK, but no response"""
    run_dpp_proto_init(dev, 1, 1, listen=True)
    ev = dev[1].wait_event(["DPP-AUTH-INIT-FAILED"], timeout=5)
    if ev is None:
        raise Exception("Authentication failure not reported")

def test_dpp_auth_req_retries(dev, apdev):
    """DPP initiator retries with no ACK"""
    check_dpp_capab(dev[1])
    dev[1].set("dpp_init_max_tries", "3")
    dev[1].set("dpp_init_retry_time", "1000")
    dev[1].set("dpp_resp_wait_time", "100")
    run_dpp_proto_init(dev, 1, 1, unicast=False, listen=False)

    for i in range(3):
        ev = dev[1].wait_event(["DPP-TX "], timeout=5)
        if ev is None:
            raise Exception("Auth Req not sent (%d)" % i)

    ev = dev[1].wait_event(["DPP-AUTH-INIT-FAILED"], timeout=5)
    if ev is None:
        raise Exception("Authentication failure not reported")

def test_dpp_auth_req_retries_multi_chan(dev, apdev):
    """DPP initiator retries with no ACK and multiple channels"""
    check_dpp_capab(dev[1])
    dev[1].set("dpp_init_max_tries", "3")
    dev[1].set("dpp_init_retry_time", "1000")
    dev[1].set("dpp_resp_wait_time", "100")
    run_dpp_proto_init(dev, 1, 1, unicast=False, listen=False,
                       chan="81/1,81/6,81/11")

    for i in range(3 * 3):
        ev = dev[1].wait_event(["DPP-TX "], timeout=5)
        if ev is None:
            raise Exception("Auth Req not sent (%d)" % i)

    ev = dev[1].wait_event(["DPP-AUTH-INIT-FAILED"], timeout=5)
    if ev is None:
        raise Exception("Authentication failure not reported")

def test_dpp_proto_after_wrapped_data_auth_resp(dev, apdev):
    """DPP protocol testing - attribute after Wrapped Data in Auth Resp"""
    run_dpp_proto_init(dev, 0, 2)
    ev = dev[1].wait_event(["DPP-RX"], timeout=5)
    if ev is None:
        raise Exception("DPP Authentication Response not seen")
    if "type=1" not in ev or "ignore=invalid-attributes" not in ev:
        raise Exception("Unexpected RX info: " + ev)
    ev = dev[0].wait_event(["DPP-RX"], timeout=1)
    if ev is None or "type=0" not in ev:
        raise Exception("DPP Authentication Request not seen")
    ev = dev[0].wait_event(["DPP-RX"], timeout=0.1)
    if ev is not None:
        raise Exception("Unexpected DPP message seen")

def test_dpp_proto_after_wrapped_data_auth_conf(dev, apdev):
    """DPP protocol testing - attribute after Wrapped Data in Auth Conf"""
    run_dpp_proto_init(dev, 1, 3)
    ev = dev[0].wait_event(["DPP-RX"], timeout=5)
    if ev is None or "type=0" not in ev:
        raise Exception("DPP Authentication Request not seen")
    ev = dev[0].wait_event(["DPP-RX"], timeout=5)
    if ev is None:
        raise Exception("DPP Authentication Confirm not seen")
    if "type=2" not in ev or "ignore=invalid-attributes" not in ev:
        raise Exception("Unexpected RX info: " + ev)

def test_dpp_proto_after_wrapped_data_conf_req(dev, apdev):
    """DPP protocol testing - attribute after Wrapped Data in Conf Req"""
    run_dpp_proto_init(dev, 0, 6)
    ev = dev[1].wait_event(["DPP-CONF-FAILED"], timeout=10)
    if ev is None:
        raise Exception("DPP Configuration failure not seen")

def test_dpp_proto_after_wrapped_data_conf_resp(dev, apdev):
    """DPP protocol testing - attribute after Wrapped Data in Conf Resp"""
    run_dpp_proto_init(dev, 1, 7)
    ev = dev[0].wait_event(["DPP-CONF-FAILED"], timeout=10)
    if ev is None:
        raise Exception("DPP Configuration failure not seen")

def test_dpp_proto_zero_i_capab(dev, apdev):
    """DPP protocol testing - zero I-capability in Auth Req"""
    run_dpp_proto_init(dev, 1, 8)
    wait_dpp_fail(dev[0], "Invalid role in I-capabilities 0x00")
    ev = dev[1].wait_event(["DPP-RX"], timeout=0.1)
    if ev is not None:
        raise Exception("Unexpected DPP message seen")

def test_dpp_proto_zero_r_capab(dev, apdev):
    """DPP protocol testing - zero R-capability in Auth Resp"""
    run_dpp_proto_init(dev, 0, 9)
    wait_dpp_fail(dev[1], "Unexpected role in R-capabilities 0x00")
    ev = dev[0].wait_event(["DPP-RX"], timeout=1)
    if ev is None or "type=0" not in ev:
        raise Exception("DPP Authentication Request not seen")
    ev = dev[0].wait_event(["DPP-RX"], timeout=0.1)
    if ev is not None:
        raise Exception("Unexpected DPP message seen")

def run_dpp_proto_auth_req_missing(dev, test, reason, mutual=False):
    run_dpp_proto_init(dev, 1, test, mutual=mutual)
    wait_dpp_fail(dev[0], reason)
    ev = dev[1].wait_event(["DPP-RX"], timeout=0.1)
    if ev is not None:
        raise Exception("Unexpected DPP message seen")

def test_dpp_proto_auth_req_no_r_bootstrap_key(dev, apdev):
    """DPP protocol testing - no R-bootstrap key in Auth Req"""
    run_dpp_proto_auth_req_missing(dev, 10, "Missing or invalid required Responder Bootstrapping Key Hash attribute")

def test_dpp_proto_auth_req_invalid_r_bootstrap_key(dev, apdev):
    """DPP protocol testing - invalid R-bootstrap key in Auth Req"""
    run_dpp_proto_auth_req_missing(dev, 68, "No matching own bootstrapping key found - ignore message")

def test_dpp_proto_auth_req_no_i_bootstrap_key(dev, apdev):
    """DPP protocol testing - no I-bootstrap key in Auth Req"""
    run_dpp_proto_auth_req_missing(dev, 11, "Missing or invalid required Initiator Bootstrapping Key Hash attribute")

def test_dpp_proto_auth_req_invalid_i_bootstrap_key(dev, apdev):
    """DPP protocol testing - invalid I-bootstrap key in Auth Req"""
    run_dpp_proto_init(dev, 1, 69, mutual=True)
    ev = dev[0].wait_event(["DPP-SCAN-PEER-QR-CODE"], timeout=5)
    if ev is None:
        raise Exception("DPP scan request not seen")
    ev = dev[1].wait_event(["DPP-RESPONSE-PENDING"], timeout=5)
    if ev is None:
        raise Exception("DPP response pending indivation not seen")

def test_dpp_proto_auth_req_no_i_proto_key(dev, apdev):
    """DPP protocol testing - no I-proto key in Auth Req"""
    run_dpp_proto_auth_req_missing(dev, 12, "Missing required Initiator Protocol Key attribute")

def test_dpp_proto_auth_req_invalid_i_proto_key(dev, apdev):
    """DPP protocol testing - invalid I-proto key in Auth Req"""
    run_dpp_proto_auth_req_missing(dev, 66, "Invalid Initiator Protocol Key")

def test_dpp_proto_auth_req_no_i_nonce(dev, apdev):
    """DPP protocol testing - no I-nonce in Auth Req"""
    run_dpp_proto_auth_req_missing(dev, 13, "Missing or invalid I-nonce")

def test_dpp_proto_auth_req_invalid_i_nonce(dev, apdev):
    """DPP protocol testing - invalid I-nonce in Auth Req"""
    run_dpp_proto_auth_req_missing(dev, 81, "Missing or invalid I-nonce")

def test_dpp_proto_auth_req_no_i_capab(dev, apdev):
    """DPP protocol testing - no I-capab in Auth Req"""
    run_dpp_proto_auth_req_missing(dev, 14, "Missing or invalid I-capab")

def test_dpp_proto_auth_req_no_wrapped_data(dev, apdev):
    """DPP protocol testing - no Wrapped Data in Auth Req"""
    run_dpp_proto_auth_req_missing(dev, 15, "Missing or invalid required Wrapped Data attribute")

def run_dpp_proto_auth_resp_missing(dev, test, reason,
                                    incompatible_roles=False):
    run_dpp_proto_init(dev, 0, test, mutual=True,
                       incompatible_roles=incompatible_roles)
    if reason is None:
        if incompatible_roles:
            ev = dev[0].wait_event(["DPP-NOT-COMPATIBLE"], timeout=5)
            if ev is None:
                raise Exception("DPP-NOT-COMPATIBLE not reported")
        time.sleep(0.1)
        return
    wait_dpp_fail(dev[1], reason)
    ev = dev[0].wait_event(["DPP-RX"], timeout=1)
    if ev is None or "type=0" not in ev:
        raise Exception("DPP Authentication Request not seen")
    ev = dev[0].wait_event(["DPP-RX"], timeout=0.1)
    if ev is not None:
        raise Exception("Unexpected DPP message seen")

def test_dpp_proto_auth_resp_no_status(dev, apdev):
    """DPP protocol testing - no Status in Auth Resp"""
    run_dpp_proto_auth_resp_missing(dev, 16, "Missing or invalid required DPP Status attribute")

def test_dpp_proto_auth_resp_status_no_status(dev, apdev):
    """DPP protocol testing - no Status in Auth Resp(status)"""
    run_dpp_proto_auth_resp_missing(dev, 16,
                                    "Missing or invalid required DPP Status attribute",
                                    incompatible_roles=True)

def test_dpp_proto_auth_resp_invalid_status(dev, apdev):
    """DPP protocol testing - invalid Status in Auth Resp"""
    run_dpp_proto_auth_resp_missing(dev, 74, "Responder reported failure")

def test_dpp_proto_auth_resp_no_r_bootstrap_key(dev, apdev):
    """DPP protocol testing - no R-bootstrap key in Auth Resp"""
    run_dpp_proto_auth_resp_missing(dev, 17, "Missing or invalid required Responder Bootstrapping Key Hash attribute")

def test_dpp_proto_auth_resp_status_no_r_bootstrap_key(dev, apdev):
    """DPP protocol testing - no R-bootstrap key in Auth Resp(status)"""
    run_dpp_proto_auth_resp_missing(dev, 17,
                                    "Missing or invalid required Responder Bootstrapping Key Hash attribute",
                                    incompatible_roles=True)

def test_dpp_proto_auth_resp_invalid_r_bootstrap_key(dev, apdev):
    """DPP protocol testing - invalid R-bootstrap key in Auth Resp"""
    run_dpp_proto_auth_resp_missing(dev, 70, "Unexpected Responder Bootstrapping Key Hash value")

def test_dpp_proto_auth_resp_status_invalid_r_bootstrap_key(dev, apdev):
    """DPP protocol testing - invalid R-bootstrap key in Auth Resp(status)"""
    run_dpp_proto_auth_resp_missing(dev, 70,
                                    "Unexpected Responder Bootstrapping Key Hash value",
                                    incompatible_roles=True)

def test_dpp_proto_auth_resp_no_i_bootstrap_key(dev, apdev):
    """DPP protocol testing - no I-bootstrap key in Auth Resp"""
    run_dpp_proto_auth_resp_missing(dev, 18, None)

def test_dpp_proto_auth_resp_status_no_i_bootstrap_key(dev, apdev):
    """DPP protocol testing - no I-bootstrap key in Auth Resp(status)"""
    run_dpp_proto_auth_resp_missing(dev, 18, None, incompatible_roles=True)

def test_dpp_proto_auth_resp_invalid_i_bootstrap_key(dev, apdev):
    """DPP protocol testing - invalid I-bootstrap key in Auth Resp"""
    run_dpp_proto_auth_resp_missing(dev, 71, "Initiator Bootstrapping Key Hash attribute did not match")

def test_dpp_proto_auth_resp_status_invalid_i_bootstrap_key(dev, apdev):
    """DPP protocol testing - invalid I-bootstrap key in Auth Resp(status)"""
    run_dpp_proto_auth_resp_missing(dev, 71,
                                    "Initiator Bootstrapping Key Hash attribute did not match",
                                    incompatible_roles=True)

def test_dpp_proto_auth_resp_no_r_proto_key(dev, apdev):
    """DPP protocol testing - no R-Proto Key in Auth Resp"""
    run_dpp_proto_auth_resp_missing(dev, 19, "Missing required Responder Protocol Key attribute")

def test_dpp_proto_auth_resp_invalid_r_proto_key(dev, apdev):
    """DPP protocol testing - invalid R-Proto Key in Auth Resp"""
    run_dpp_proto_auth_resp_missing(dev, 67, "Invalid Responder Protocol Key")

def test_dpp_proto_auth_resp_no_r_nonce(dev, apdev):
    """DPP protocol testing - no R-nonce in Auth Resp"""
    run_dpp_proto_auth_resp_missing(dev, 20, "Missing or invalid R-nonce")

def test_dpp_proto_auth_resp_no_i_nonce(dev, apdev):
    """DPP protocol testing - no I-nonce in Auth Resp"""
    run_dpp_proto_auth_resp_missing(dev, 21, "Missing or invalid I-nonce")

def test_dpp_proto_auth_resp_status_no_i_nonce(dev, apdev):
    """DPP protocol testing - no I-nonce in Auth Resp(status)"""
    run_dpp_proto_auth_resp_missing(dev, 21, "Missing or invalid I-nonce",
                                    incompatible_roles=True)

def test_dpp_proto_auth_resp_no_r_capab(dev, apdev):
    """DPP protocol testing - no R-capab in Auth Resp"""
    run_dpp_proto_auth_resp_missing(dev, 22, "Missing or invalid R-capabilities")

def test_dpp_proto_auth_resp_no_r_auth(dev, apdev):
    """DPP protocol testing - no R-auth in Auth Resp"""
    run_dpp_proto_auth_resp_missing(dev, 23, "Missing or invalid Secondary Wrapped Data")

def test_dpp_proto_auth_resp_no_wrapped_data(dev, apdev):
    """DPP protocol testing - no Wrapped Data in Auth Resp"""
    run_dpp_proto_auth_resp_missing(dev, 24, "Missing or invalid required Wrapped Data attribute")

def test_dpp_proto_auth_resp_i_nonce_mismatch(dev, apdev):
    """DPP protocol testing - I-nonce mismatch in Auth Resp"""
    run_dpp_proto_init(dev, 0, 30, mutual=True)
    wait_dpp_fail(dev[1], "I-nonce mismatch")
    ev = dev[0].wait_event(["DPP-RX"], timeout=1)
    if ev is None or "type=0" not in ev:
        raise Exception("DPP Authentication Request not seen")
    ev = dev[0].wait_event(["DPP-RX"], timeout=0.1)
    if ev is not None:
        raise Exception("Unexpected DPP message seen")

def test_dpp_proto_auth_resp_incompatible_r_capab(dev, apdev):
    """DPP protocol testing - Incompatible R-capab in Auth Resp"""
    run_dpp_proto_init(dev, 0, 31, mutual=True)
    wait_dpp_fail(dev[1], "Unexpected role in R-capabilities 0x02")
    wait_dpp_fail(dev[0], "Peer reported incompatible R-capab role")

def test_dpp_proto_auth_resp_r_auth_mismatch(dev, apdev):
    """DPP protocol testing - R-auth mismatch in Auth Resp"""
    run_dpp_proto_init(dev, 0, 32, mutual=True)
    wait_dpp_fail(dev[1], "Mismatching Responder Authenticating Tag")
    wait_dpp_fail(dev[0], "Peer reported authentication failure")

def test_dpp_proto_auth_resp_r_auth_mismatch_failure(dev, apdev):
    """DPP protocol testing - Auth Conf RX processing failure"""
    with alloc_fail(dev[0], 1, "dpp_auth_conf_rx_failure"):
        run_dpp_proto_init(dev, 0, 32, mutual=True)
        wait_dpp_fail(dev[0], "Authentication failed")

def test_dpp_proto_auth_resp_r_auth_mismatch_failure2(dev, apdev):
    """DPP protocol testing - Auth Conf RX processing failure 2"""
    with fail_test(dev[0], 1, "dpp_auth_conf_rx_failure"):
        run_dpp_proto_init(dev, 0, 32, mutual=True)
        wait_dpp_fail(dev[0], "AES-SIV decryption failed")

def run_dpp_proto_auth_conf_missing(dev, test, reason):
    run_dpp_proto_init(dev, 1, test, mutual=True)
    if reason is None:
        time.sleep(0.1)
        return
    wait_dpp_fail(dev[0], reason)

def test_dpp_proto_auth_conf_no_status(dev, apdev):
    """DPP protocol testing - no Status in Auth Conf"""
    run_dpp_proto_auth_conf_missing(dev, 25, "Missing or invalid required DPP Status attribute")

def test_dpp_proto_auth_conf_invalid_status(dev, apdev):
    """DPP protocol testing - invalid Status in Auth Conf"""
    run_dpp_proto_auth_conf_missing(dev, 75, "Authentication failed")

def test_dpp_proto_auth_conf_no_r_bootstrap_key(dev, apdev):
    """DPP protocol testing - no R-bootstrap key in Auth Conf"""
    run_dpp_proto_auth_conf_missing(dev, 26, "Missing or invalid required Responder Bootstrapping Key Hash attribute")

def test_dpp_proto_auth_conf_invalid_r_bootstrap_key(dev, apdev):
    """DPP protocol testing - invalid R-bootstrap key in Auth Conf"""
    run_dpp_proto_auth_conf_missing(dev, 72, "Responder Bootstrapping Key Hash mismatch")

def test_dpp_proto_auth_conf_no_i_bootstrap_key(dev, apdev):
    """DPP protocol testing - no I-bootstrap key in Auth Conf"""
    run_dpp_proto_auth_conf_missing(dev, 27, "Missing Initiator Bootstrapping Key Hash attribute")

def test_dpp_proto_auth_conf_invalid_i_bootstrap_key(dev, apdev):
    """DPP protocol testing - invalid I-bootstrap key in Auth Conf"""
    run_dpp_proto_auth_conf_missing(dev, 73, "Initiator Bootstrapping Key Hash mismatch")

def test_dpp_proto_auth_conf_no_i_auth(dev, apdev):
    """DPP protocol testing - no I-Auth in Auth Conf"""
    run_dpp_proto_auth_conf_missing(dev, 28, "Missing or invalid Initiator Authenticating Tag")

def test_dpp_proto_auth_conf_no_wrapped_data(dev, apdev):
    """DPP protocol testing - no Wrapped Data in Auth Conf"""
    run_dpp_proto_auth_conf_missing(dev, 29, "Missing or invalid required Wrapped Data attribute")

def test_dpp_proto_auth_conf_i_auth_mismatch(dev, apdev):
    """DPP protocol testing - I-auth mismatch in Auth Conf"""
    run_dpp_proto_init(dev, 1, 33, mutual=True)
    wait_dpp_fail(dev[0], "Mismatching Initiator Authenticating Tag")

def test_dpp_proto_auth_conf_replaced_by_resp(dev, apdev):
    """DPP protocol testing - Auth Conf replaced by Resp"""
    run_dpp_proto_init(dev, 1, 65, mutual=True)
    wait_dpp_fail(dev[0], "Unexpected Authentication Response")

def run_dpp_proto_conf_req_missing(dev, test, reason):
    run_dpp_proto_init(dev, 0, test)
    wait_dpp_fail(dev[1], reason)

def test_dpp_proto_conf_req_no_e_nonce(dev, apdev):
    """DPP protocol testing - no E-nonce in Conf Req"""
    run_dpp_proto_conf_req_missing(dev, 51,
                                   "Missing or invalid Enrollee Nonce attribute")

def test_dpp_proto_conf_req_invalid_e_nonce(dev, apdev):
    """DPP protocol testing - invalid E-nonce in Conf Req"""
    run_dpp_proto_conf_req_missing(dev, 83,
                                   "Missing or invalid Enrollee Nonce attribute")

def test_dpp_proto_conf_req_no_config_attr_obj(dev, apdev):
    """DPP protocol testing - no Config Attr Obj in Conf Req"""
    run_dpp_proto_conf_req_missing(dev, 52,
                                   "Missing or invalid Config Attributes attribute")

def test_dpp_proto_conf_req_invalid_config_attr_obj(dev, apdev):
    """DPP protocol testing - invalid Config Attr Obj in Conf Req"""
    run_dpp_proto_conf_req_missing(dev, 76,
                                   "Unsupported wi-fi_tech")

def test_dpp_proto_conf_req_no_wrapped_data(dev, apdev):
    """DPP protocol testing - no Wrapped Data in Conf Req"""
    run_dpp_proto_conf_req_missing(dev, 53,
                                   "Missing or invalid required Wrapped Data attribute")

def run_dpp_proto_conf_resp_missing(dev, test, reason):
    run_dpp_proto_init(dev, 1, test)
    wait_dpp_fail(dev[0], reason)

def test_dpp_proto_conf_resp_no_e_nonce(dev, apdev):
    """DPP protocol testing - no E-nonce in Conf Resp"""
    run_dpp_proto_conf_resp_missing(dev, 54,
                                    "Missing or invalid Enrollee Nonce attribute")

def test_dpp_proto_conf_resp_no_config_obj(dev, apdev):
    """DPP protocol testing - no Config Object in Conf Resp"""
    run_dpp_proto_conf_resp_missing(dev, 55,
                                    "Missing required Configuration Object attribute")

def test_dpp_proto_conf_resp_no_status(dev, apdev):
    """DPP protocol testing - no Status in Conf Resp"""
    run_dpp_proto_conf_resp_missing(dev, 56,
                                    "Missing or invalid required DPP Status attribute")

def test_dpp_proto_conf_resp_no_wrapped_data(dev, apdev):
    """DPP protocol testing - no Wrapped Data in Conf Resp"""
    run_dpp_proto_conf_resp_missing(dev, 57,
                                    "Missing or invalid required Wrapped Data attribute")

def test_dpp_proto_conf_resp_invalid_status(dev, apdev):
    """DPP protocol testing - invalid Status in Conf Resp"""
    run_dpp_proto_conf_resp_missing(dev, 58,
                                    "Configurator rejected configuration")

def test_dpp_proto_conf_resp_e_nonce_mismatch(dev, apdev):
    """DPP protocol testing - E-nonce mismatch in Conf Resp"""
    run_dpp_proto_conf_resp_missing(dev, 59,
                                    "Enrollee Nonce mismatch")

def test_dpp_proto_stop_at_auth_req(dev, apdev):
    """DPP protocol testing - stop when receiving Auth Req"""
    run_dpp_proto_init(dev, 0, 87)
    ev = dev[1].wait_event(["DPP-AUTH-INIT-FAILED"], timeout=5)
    if ev is None:
        raise Exception("Authentication init failure not reported")

def test_dpp_proto_stop_at_auth_resp(dev, apdev):
    """DPP protocol testing - stop when receiving Auth Resp"""
    uri0, role, configurator, conf, own = run_dpp_proto_init(dev, 1, 88)

    ev = dev[1].wait_event(["DPP-TX "], timeout=5)
    if ev is None:
        raise Exception("Auth Req TX not seen")

    ev = dev[0].wait_event(["DPP-TX "], timeout=5)
    if ev is None:
        raise Exception("Auth Resp TX not seen")

    ev = dev[1].wait_event(["DPP-TX "], timeout=0.1)
    if ev is not None:
        raise Exception("Unexpected Auth Conf TX")

    ev = dev[0].wait_event(["DPP-FAIL"], timeout=2)
    if ev is None or "No Auth Confirm received" not in ev:
        raise Exception("DPP-FAIL for missing Auth Confirm not reported")
    time.sleep(0.1)

    # Try again without special testing behavior to confirm Responder is able
    # to accept a new provisioning attempt.
    dev[1].set("dpp_test", "0")
    dev[1].dpp_auth_init(uri=uri0, role=role, configurator=configurator,
                         conf=conf, own=own)
    wait_auth_success(dev[0], dev[1])

def test_dpp_proto_stop_at_auth_conf(dev, apdev):
    """DPP protocol testing - stop when receiving Auth Conf"""
    run_dpp_proto_init(dev, 0, 89, init_enrollee=True)
    ev = dev[1].wait_event(["GAS-QUERY-START"], timeout=10)
    if ev is None:
        raise Exception("Enrollee did not start GAS")
    ev = dev[1].wait_event(["GAS-QUERY-DONE"], timeout=10)
    if ev is None:
        raise Exception("Enrollee did not time out GAS")
    if "result=TIMEOUT" not in ev:
        raise Exception("Unexpected GAS result: " + ev)

def test_dpp_proto_stop_at_auth_conf_tx(dev, apdev):
    """DPP protocol testing - stop when transmitting Auth Conf (Registrar)"""
    run_dpp_proto_init(dev, 1, 89, init_enrollee=True)
    wait_auth_success(dev[0], dev[1], timeout=10)
    ev = dev[1].wait_event(["GAS-QUERY-START"], timeout=0.1)
    if ev is not None:
        raise Exception("Unexpected GAS query")

    # There is currently no timeout on GAS server side, so no event to wait for
    # in this case.

def test_dpp_proto_stop_at_auth_conf_tx2(dev, apdev):
    """DPP protocol testing - stop when transmitting Auth Conf (Enrollee)"""
    run_dpp_proto_init(dev, 1, 89)
    wait_auth_success(dev[0], dev[1], timeout=10)

    ev = dev[0].wait_event(["GAS-QUERY-DONE"], timeout=5)
    if ev is None or "result=TIMEOUT" not in ev:
        raise Exception("GAS query did not time out")

def test_dpp_proto_stop_at_conf_req(dev, apdev):
    """DPP protocol testing - stop when receiving Auth Req"""
    run_dpp_proto_init(dev, 1, 90)
    ev = dev[0].wait_event(["GAS-QUERY-START"], timeout=10)
    if ev is None:
        raise Exception("Enrollee did not start GAS")
    ev = dev[0].wait_event(["GAS-QUERY-DONE"], timeout=10)
    if ev is None:
        raise Exception("Enrollee did not time out GAS")
    if "result=TIMEOUT" not in ev:
        raise Exception("Unexpected GAS result: " + ev)

def run_dpp_proto_init_pkex(dev, test_dev, test):
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    dev[test_dev].set("dpp_test", str(test))
    dev[0].dpp_pkex_resp(2437, identifier="test", code="secret")
    dev[1].dpp_pkex_init(identifier="test", code="secret")

def test_dpp_proto_after_wrapped_data_pkex_cr_req(dev, apdev):
    """DPP protocol testing - attribute after Wrapped Data in PKEX CR Req"""
    run_dpp_proto_init_pkex(dev, 1, 4)
    ev = dev[0].wait_event(["DPP-RX"], timeout=5)
    if ev is None or "type=7" not in ev:
        raise Exception("PKEX Exchange Request not seen")
    ev = dev[0].wait_event(["DPP-RX"], timeout=5)
    if ev is None or "type=9" not in ev:
        raise Exception("PKEX Commit-Reveal Request not seen")
    if "ignore=invalid-attributes" not in ev:
        raise Exception("Unexpected RX info: " + ev)

def test_dpp_proto_after_wrapped_data_pkex_cr_resp(dev, apdev):
    """DPP protocol testing - attribute after Wrapped Data in PKEX CR Resp"""
    run_dpp_proto_init_pkex(dev, 0, 5)
    ev = dev[1].wait_event(["DPP-RX"], timeout=5)
    if ev is None or "type=8" not in ev:
        raise Exception("PKEX Exchange Response not seen")
    ev = dev[1].wait_event(["DPP-RX"], timeout=5)
    if ev is None or "type=10" not in ev:
        raise Exception("PKEX Commit-Reveal Response not seen")
    if "ignore=invalid-attributes" not in ev:
        raise Exception("Unexpected RX info: " + ev)

def run_dpp_proto_pkex_req_missing(dev, test, reason):
    run_dpp_proto_init_pkex(dev, 1, test)
    wait_dpp_fail(dev[0], reason)

def run_dpp_proto_pkex_resp_missing(dev, test, reason):
    run_dpp_proto_init_pkex(dev, 0, test)
    wait_dpp_fail(dev[1], reason)

def test_dpp_proto_pkex_exchange_req_no_finite_cyclic_group(dev, apdev):
    """DPP protocol testing - no Finite Cyclic Group in PKEX Exchange Request"""
    run_dpp_proto_pkex_req_missing(dev, 34,
                                   "Missing or invalid Finite Cyclic Group attribute")

def test_dpp_proto_pkex_exchange_req_no_encrypted_key(dev, apdev):
    """DPP protocol testing - no Encrypted Key in PKEX Exchange Request"""
    run_dpp_proto_pkex_req_missing(dev, 35,
                                   "Missing Encrypted Key attribute")

def test_dpp_proto_pkex_exchange_resp_no_status(dev, apdev):
    """DPP protocol testing - no Status in PKEX Exchange Response"""
    run_dpp_proto_pkex_resp_missing(dev, 36, "No DPP Status attribute")

def test_dpp_proto_pkex_exchange_resp_no_encrypted_key(dev, apdev):
    """DPP protocol testing - no Encrypted Key in PKEX Exchange Response"""
    run_dpp_proto_pkex_resp_missing(dev, 37, "Missing Encrypted Key attribute")

def test_dpp_proto_pkex_cr_req_no_bootstrap_key(dev, apdev):
    """DPP protocol testing - no Bootstrap Key in PKEX Commit-Reveal Request"""
    run_dpp_proto_pkex_req_missing(dev, 38,
                                   "No valid peer bootstrapping key found")

def test_dpp_proto_pkex_cr_req_no_i_auth_tag(dev, apdev):
    """DPP protocol testing - no I-Auth Tag in PKEX Commit-Reveal Request"""
    run_dpp_proto_pkex_req_missing(dev, 39, "No valid u (I-Auth tag) found")

def test_dpp_proto_pkex_cr_req_no_wrapped_data(dev, apdev):
    """DPP protocol testing - no Wrapped Data in PKEX Commit-Reveal Request"""
    run_dpp_proto_pkex_req_missing(dev, 40, "Missing or invalid required Wrapped Data attribute")

def test_dpp_proto_pkex_cr_resp_no_bootstrap_key(dev, apdev):
    """DPP protocol testing - no Bootstrap Key in PKEX Commit-Reveal Response"""
    run_dpp_proto_pkex_resp_missing(dev, 41,
                                   "No valid peer bootstrapping key found")

def test_dpp_proto_pkex_cr_resp_no_r_auth_tag(dev, apdev):
    """DPP protocol testing - no R-Auth Tag in PKEX Commit-Reveal Response"""
    run_dpp_proto_pkex_resp_missing(dev, 42, "No valid v (R-Auth tag) found")

def test_dpp_proto_pkex_cr_resp_no_wrapped_data(dev, apdev):
    """DPP protocol testing - no Wrapped Data in PKEX Commit-Reveal Response"""
    run_dpp_proto_pkex_resp_missing(dev, 43, "Missing or invalid required Wrapped Data attribute")

def test_dpp_proto_pkex_exchange_req_invalid_encrypted_key(dev, apdev):
    """DPP protocol testing - invalid Encrypted Key in PKEX Exchange Request"""
    run_dpp_proto_pkex_req_missing(dev, 44,
                                   "Invalid Encrypted Key value")

def test_dpp_proto_pkex_exchange_resp_invalid_encrypted_key(dev, apdev):
    """DPP protocol testing - invalid Encrypted Key in PKEX Exchange Response"""
    run_dpp_proto_pkex_resp_missing(dev, 45,
                                    "Invalid Encrypted Key value")

def test_dpp_proto_pkex_exchange_resp_invalid_status(dev, apdev):
    """DPP protocol testing - invalid Status in PKEX Exchange Response"""
    run_dpp_proto_pkex_resp_missing(dev, 46,
                                    "PKEX failed (peer indicated failure)")

def test_dpp_proto_pkex_cr_req_invalid_bootstrap_key(dev, apdev):
    """DPP protocol testing - invalid Bootstrap Key in PKEX Commit-Reveal Request"""
    run_dpp_proto_pkex_req_missing(dev, 47,
                                   "Peer bootstrapping key is invalid")

def test_dpp_proto_pkex_cr_resp_invalid_bootstrap_key(dev, apdev):
    """DPP protocol testing - invalid Bootstrap Key in PKEX Commit-Reveal Response"""
    run_dpp_proto_pkex_resp_missing(dev, 48,
                                    "Peer bootstrapping key is invalid")

def test_dpp_proto_pkex_cr_req_i_auth_tag_mismatch(dev, apdev):
    """DPP protocol testing - I-auth tag mismatch in PKEX Commit-Reveal Request"""
    run_dpp_proto_pkex_req_missing(dev, 49, "No valid u (I-Auth tag) found")

def test_dpp_proto_pkex_cr_resp_r_auth_tag_mismatch(dev, apdev):
    """DPP protocol testing - R-auth tag mismatch in PKEX Commit-Reveal Response"""
    run_dpp_proto_pkex_resp_missing(dev, 50, "No valid v (R-Auth tag) found")

def test_dpp_proto_stop_at_pkex_exchange_resp(dev, apdev):
    """DPP protocol testing - stop when receiving PKEX Exchange Response"""
    run_dpp_proto_init_pkex(dev, 1, 84)

    ev = dev[1].wait_event(["DPP-TX "], timeout=5)
    if ev is None:
        raise Exception("PKEX Exchange Req TX not seen")

    ev = dev[0].wait_event(["DPP-TX "], timeout=5)
    if ev is None:
        raise Exception("PKEX Exchange Resp not seen")

    ev = dev[1].wait_event(["DPP-TX "], timeout=0.1)
    if ev is not None:
        raise Exception("Unexpected PKEX CR Req TX")

def test_dpp_proto_stop_at_pkex_cr_req(dev, apdev):
    """DPP protocol testing - stop when receiving PKEX CR Request"""
    run_dpp_proto_init_pkex(dev, 0, 85)

    ev = dev[1].wait_event(["DPP-TX "], timeout=5)
    if ev is None:
        raise Exception("PKEX Exchange Req TX not seen")

    ev = dev[0].wait_event(["DPP-TX "], timeout=5)
    if ev is None:
        raise Exception("PKEX Exchange Resp not seen")

    ev = dev[1].wait_event(["DPP-TX "], timeout=5)
    if ev is None:
        raise Exception("PKEX CR Req TX not seen")

    ev = dev[0].wait_event(["DPP-TX "], timeout=0.1)
    if ev is not None:
        raise Exception("Unexpected PKEX CR Resp TX")

def test_dpp_proto_stop_at_pkex_cr_resp(dev, apdev):
    """DPP protocol testing - stop when receiving PKEX CR Response"""
    run_dpp_proto_init_pkex(dev, 1, 86)

    ev = dev[1].wait_event(["DPP-TX "], timeout=5)
    if ev is None:
        raise Exception("PKEX Exchange Req TX not seen")

    ev = dev[0].wait_event(["DPP-TX "], timeout=5)
    if ev is None:
        raise Exception("PKEX Exchange Resp not seen")

    ev = dev[1].wait_event(["DPP-TX "], timeout=5)
    if ev is None:
        raise Exception("PKEX CR Req TX not seen")

    ev = dev[0].wait_event(["DPP-TX "], timeout=5)
    if ev is None:
        raise Exception("PKEX CR Resp TX not seen")

    ev = dev[1].wait_event(["DPP-TX "], timeout=0.1)
    if ev is not None:
        raise Exception("Unexpected Auth Req TX")

def test_dpp_proto_network_introduction(dev, apdev):
    """DPP protocol testing - network introduction"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])

    params = {"ssid": "dpp",
              "wpa": "2",
              "wpa_key_mgmt": "DPP",
              "ieee80211w": "2",
              "rsn_pairwise": "CCMP",
              "dpp_connector": params1_ap_connector,
              "dpp_csign": params1_csign,
              "dpp_netaccesskey": params1_ap_netaccesskey}
    try:
        hapd = hostapd.add_ap(apdev[0], params)
    except:
        raise HwsimSkip("DPP not supported")

    for test in [60, 61, 80, 82]:
        dev[0].set("dpp_test", str(test))
        dev[0].connect("dpp", key_mgmt="DPP", scan_freq="2412", ieee80211w="2",
                       dpp_csign=params1_csign,
                       dpp_connector=params1_sta_connector,
                       dpp_netaccesskey=params1_sta_netaccesskey,
                       wait_connect=False)

        ev = dev[0].wait_event(["DPP-TX "], timeout=10)
        if ev is None or "type=5" not in ev:
            raise Exception("Peer Discovery Request TX not reported")
        ev = dev[0].wait_event(["DPP-TX-STATUS"], timeout=2)
        if ev is None or "result=SUCCESS" not in ev:
            raise Exception("Peer Discovery Request TX status not reported")

        ev = hapd.wait_event(["DPP-RX"], timeout=10)
        if ev is None or "type=5" not in ev:
            raise Exception("Peer Discovery Request RX not reported")

        if test == 80:
            ev = dev[0].wait_event(["DPP-INTRO"], timeout=10)
            if ev is None:
                raise Exception("DPP-INTRO not reported for test 80")
            if "status=7" not in ev:
                raise Exception("Unexpected result in test 80: " + ev)

        dev[0].request("REMOVE_NETWORK all")
        dev[0].dump_monitor()
        hapd.dump_monitor()
    dev[0].set("dpp_test", "0")

    for test in [62, 63, 64, 77, 78, 79]:
        hapd.set("dpp_test", str(test))
        dev[0].connect("dpp", key_mgmt="DPP", scan_freq="2412", ieee80211w="2",
                       dpp_csign=params1_csign,
                       dpp_connector=params1_sta_connector,
                       dpp_netaccesskey=params1_sta_netaccesskey,
                       wait_connect=False)

        ev = dev[0].wait_event(["DPP-INTRO"], timeout=10)
        if ev is None:
            raise Exception("Peer introduction result not reported (test %d)" % test)
        if test == 77:
            if "fail=transaction_id_mismatch" not in ev:
                raise Exception("Connector validation failure not reported")
        elif test == 78:
            if "status=254" not in ev:
                raise Exception("Invalid status value not reported")
        elif test == 79:
            if "fail=peer_connector_validation_failed" not in ev:
                raise Exception("Connector validation failure not reported")
        elif "status=" in ev:
            raise Exception("Unexpected peer introduction result (test %d): " % test + ev)

        dev[0].request("REMOVE_NETWORK all")
        dev[0].dump_monitor()
        hapd.dump_monitor()
    hapd.set("dpp_test", "0")

    dev[0].connect("dpp", key_mgmt="DPP", scan_freq="2412", ieee80211w="2",
                   dpp_csign=params1_csign, dpp_connector=params1_sta_connector,
                   dpp_netaccesskey=params1_sta_netaccesskey)

def test_dpp_hostapd_auth_conf_timeout(dev, apdev):
    """DPP Authentication Confirm timeout in hostapd"""
    check_dpp_capab(dev[0])
    hapd = hostapd.add_ap(apdev[0], {"ssid": "unconfigured"})
    check_dpp_capab(hapd)
    id_h = hapd.dpp_bootstrap_gen(chan="81/1", mac=True)
    uri_h = hapd.request("DPP_BOOTSTRAP_GET_URI %d" % id_h)
    hapd.dpp_listen(2412)
    dev[0].set("dpp_test", "88")
    dev[0].dpp_auth_init(uri=uri_h)
    ev = hapd.wait_event(["DPP-FAIL"], timeout=10)
    if ev is None:
        raise Exception("DPP-FAIL not reported")
    if "No Auth Confirm received" not in ev:
        raise Exception("Unexpected failure reason: " + ev)

def test_dpp_hostapd_auth_resp_retries(dev, apdev):
    """DPP Authentication Response retries in hostapd"""
    check_dpp_capab(dev[0])
    hapd = hostapd.add_ap(apdev[0], {"ssid": "unconfigured"})
    check_dpp_capab(hapd)

    hapd.set("dpp_resp_max_tries", "3")
    hapd.set("dpp_resp_retry_time", "100")

    id_h = hapd.dpp_bootstrap_gen(chan="81/1", mac=True)
    uri_h = hapd.request("DPP_BOOTSTRAP_GET_URI %d" % id_h)
    id0b = dev[0].dpp_bootstrap_gen(chan="81/1", mac=True)
    uri0b = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id0b)
    hapd.dpp_listen(2412, qr="mutual")
    dev[0].dpp_auth_init(uri=uri_h, own=id0b)

    ev = dev[0].wait_event(["DPP-RESPONSE-PENDING"], timeout=5)
    if ev is None:
        raise Exception("Pending response not reported")
    ev = hapd.wait_event(["DPP-SCAN-PEER-QR-CODE"], timeout=5)
    if ev is None:
        raise Exception("QR Code scan for mutual authentication not requested")

    # Stop Initiator from listening to frames to force retransmission of the
    # DPP Authentication Response frame with Status=0
    dev[0].request("DPP_STOP_LISTEN")

    hapd.dump_monitor()
    dev[0].dump_monitor()

    id0b = hapd.dpp_qr_code(uri0b)

    ev = hapd.wait_event(["DPP-TX "], timeout=5)
    if ev is None or "type=1" not in ev:
        raise Exception("DPP Authentication Response not sent")
    ev = hapd.wait_event(["DPP-TX-STATUS"], timeout=5)
    if ev is None:
        raise Exception("TX status for DPP Authentication Response not reported")
    if "result=FAILED" not in ev:
        raise Exception("Unexpected TX status for Authentication Response: " + ev)

    ev = hapd.wait_event(["DPP-TX "], timeout=15)
    if ev is None or "type=1" not in ev:
        raise Exception("DPP Authentication Response retransmission not sent")

def test_dpp_qr_code_no_chan_list_unicast(dev, apdev):
    """DPP QR Code and no channel list (unicast)"""
    run_dpp_qr_code_chan_list(dev, apdev, True, 2417, None)

def test_dpp_qr_code_chan_list_unicast(dev, apdev):
    """DPP QR Code and 2.4 GHz channels (unicast)"""
    run_dpp_qr_code_chan_list(dev, apdev, True, 2417,
                              "81/1,81/2,81/3,81/4,81/5,81/6,81/7,81/8,81/9,81/10,81/11,81/12,81/13")

def test_dpp_qr_code_chan_list_unicast2(dev, apdev):
    """DPP QR Code and 2.4 GHz channels (unicast 2)"""
    run_dpp_qr_code_chan_list(dev, apdev, True, 2417,
                              "81/1,2,3,4,5,6,7,8,9,10,11,12,13")

def test_dpp_qr_code_chan_list_no_peer_unicast(dev, apdev):
    """DPP QR Code and channel list and no peer (unicast)"""
    run_dpp_qr_code_chan_list(dev, apdev, True, 2417, "81/1,81/6,81/11",
                              no_wait=True)
    ev = dev[1].wait_event(["DPP-AUTH-INIT-FAILED"], timeout=5)
    if ev is None:
        raise Exception("Initiation failure not reported")

def test_dpp_qr_code_no_chan_list_broadcast(dev, apdev):
    """DPP QR Code and no channel list (broadcast)"""
    run_dpp_qr_code_chan_list(dev, apdev, False, 2412, None)

def test_dpp_qr_code_chan_list_broadcast(dev, apdev):
    """DPP QR Code and some 2.4 GHz channels (broadcast)"""
    run_dpp_qr_code_chan_list(dev, apdev, False, 2412, "81/1,81/6,81/11",
                              timeout=10)

def run_dpp_qr_code_chan_list(dev, apdev, unicast, listen_freq, chanlist,
                              no_wait=False, timeout=5):
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    dev[1].set("dpp_init_max_tries", "3")
    dev[1].set("dpp_init_retry_time", "100")
    dev[1].set("dpp_resp_wait_time", "1000")

    logger.info("dev0 displays QR Code")
    id0 = dev[0].dpp_bootstrap_gen(chan=chanlist, mac=unicast)
    uri0 = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id0)
    logger.info("dev1 scans QR Code and initiates DPP Authentication")
    dev[0].dpp_listen(listen_freq)
    dev[1].dpp_auth_init(uri=uri0)
    if no_wait:
        return
    wait_auth_success(dev[0], dev[1], timeout=timeout, configurator=dev[1],
                      enrollee=dev[0], allow_enrollee_failure=True,
                      stop_responder=True)

def test_dpp_qr_code_chan_list_no_match(dev, apdev):
    """DPP QR Code and no matching supported channel"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    id0 = dev[0].dpp_bootstrap_gen(chan="123/123")
    uri0 = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id0)
    dev[1].dpp_auth_init(uri=uri0, expect_fail=True)

def test_dpp_pkex_alloc_fail(dev, apdev):
    """DPP/PKEX and memory allocation failures"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])

    tests = [(1, "=dpp_keygen_configurator"),
             (1, "base64_gen_encode;dpp_keygen_configurator")]
    for count, func in tests:
        with alloc_fail(dev[1], count, func):
            cmd = "DPP_CONFIGURATOR_ADD"
            res = dev[1].request(cmd)
            if "FAIL" not in res:
                raise Exception("Unexpected DPP_CONFIGURATOR_ADD success")

    conf_id = dev[1].dpp_configurator_add()

    id0 = None
    id1 = None

    # Local error cases on the Initiator
    tests = [(1, "crypto_ec_key_get_pubkey_point"),
             (1, "dpp_alloc_msg;dpp_pkex_build_exchange_req"),
             (1, "dpp_alloc_msg;dpp_pkex_build_commit_reveal_req"),
             (1, "dpp_alloc_msg;dpp_auth_build_req"),
             (1, "dpp_alloc_msg;dpp_auth_build_conf"),
             (1, "dpp_bootstrap_key_hash"),
             (1, "dpp_auth_init"),
             (1, "dpp_alloc_auth"),
             (1, "=dpp_auth_resp_rx"),
             (1, "dpp_build_conf_start"),
             (1, "dpp_build_conf_obj_dpp"),
             (2, "dpp_build_conf_obj_dpp"),
             (3, "dpp_build_conf_obj_dpp"),
             (4, "dpp_build_conf_obj_dpp"),
             (5, "dpp_build_conf_obj_dpp"),
             (6, "dpp_build_conf_obj_dpp"),
             (7, "dpp_build_conf_obj_dpp"),
             (8, "dpp_build_conf_obj_dpp"),
             (1, "dpp_conf_req_rx"),
             (2, "dpp_conf_req_rx"),
             (3, "dpp_conf_req_rx"),
             (4, "dpp_conf_req_rx"),
             (5, "dpp_conf_req_rx"),
             (6, "dpp_conf_req_rx"),
             (7, "dpp_conf_req_rx"),
             (1, "dpp_pkex_init"),
             (2, "dpp_pkex_init"),
             (3, "dpp_pkex_init"),
             (1, "dpp_pkex_derive_z"),
             (1, "=dpp_pkex_rx_commit_reveal_resp"),
             (1, "crypto_ec_key_get_pubkey_point;dpp_build_jwk"),
             (2, "crypto_ec_key_get_pubkey_point;dpp_build_jwk"),
             (1, "crypto_ec_key_get_pubkey_point;dpp_auth_init")]
    for count, func in tests:
        dev[0].request("DPP_STOP_LISTEN")
        dev[1].request("DPP_STOP_LISTEN")
        dev[0].dump_monitor()
        dev[1].dump_monitor()
        id0 = dev[0].dpp_pkex_resp(2437, identifier="test", code="secret",
                                   use_id=id0)

        with alloc_fail(dev[1], count, func):
            id1 = dev[1].dpp_pkex_init(identifier="test", code="secret",
                                       use_id=id1,
                                       extra="conf=sta-dpp configurator=%d" % conf_id,
                                       allow_fail=True)
            wait_fail_trigger(dev[1], "GET_ALLOC_FAIL", max_iter=100)
            ev = dev[0].wait_event(["GAS-QUERY-START"], timeout=0.01)
            if ev:
                dev[0].request("DPP_STOP_LISTEN")
                dev[0].wait_event(["GAS-QUERY-DONE"], timeout=3)

    # Local error cases on the Responder
    tests = [(1, "crypto_ec_key_get_pubkey_point"),
             (1, "dpp_alloc_msg;dpp_pkex_build_exchange_resp"),
             (1, "dpp_alloc_msg;dpp_pkex_build_commit_reveal_resp"),
             (1, "dpp_alloc_msg;dpp_auth_build_resp"),
             (1, "crypto_ec_key_get_pubkey_point;dpp_auth_build_resp_ok"),
             (1, "dpp_alloc_auth"),
             (1, "=dpp_auth_req_rx"),
             (1, "=dpp_auth_conf_rx"),
             (1, "json_parse;dpp_parse_jws_prot_hdr"),
             (1, "json_get_member_base64url;dpp_parse_jws_prot_hdr"),
             (1, "json_get_member_base64url;dpp_parse_jwk"),
             (2, "json_get_member_base64url;dpp_parse_jwk"),
             (1, "json_parse;dpp_parse_connector"),
             (1, "dpp_parse_jwk;dpp_parse_connector"),
             (1, "dpp_parse_jwk;dpp_parse_cred_dpp"),
             (1, "crypto_ec_key_get_pubkey_point;dpp_check_pubkey_match"),
             (1, "base64_gen_decode;dpp_process_signed_connector"),
             (1, "dpp_parse_jws_prot_hdr;dpp_process_signed_connector"),
             (2, "base64_gen_decode;dpp_process_signed_connector"),
             (3, "base64_gen_decode;dpp_process_signed_connector"),
             (4, "base64_gen_decode;dpp_process_signed_connector"),
             (1, "json_parse;dpp_parse_conf_obj"),
             (1, "dpp_conf_resp_rx"),
             (1, "=dpp_pkex_derive_z"),
             (1, "=dpp_pkex_rx_exchange_req"),
             (2, "=dpp_pkex_rx_exchange_req"),
             (3, "=dpp_pkex_rx_exchange_req"),
             (1, "=dpp_pkex_rx_commit_reveal_req"),
             (1, "crypto_ec_key_get_pubkey_point;dpp_pkex_rx_commit_reveal_req"),
             (1, "dpp_bootstrap_key_hash")]
    for count, func in tests:
        dev[0].request("DPP_STOP_LISTEN")
        dev[1].request("DPP_STOP_LISTEN")
        dev[0].dump_monitor()
        dev[1].dump_monitor()
        id0 = dev[0].dpp_pkex_resp(2437, identifier="test", code="secret",
                                   use_id=id0)

        with alloc_fail(dev[0], count, func):
            id1 = dev[1].dpp_pkex_init(identifier="test", code="secret",
                                       use_id=id1,
                                       extra="conf=sta-dpp configurator=%d" % conf_id)
            wait_fail_trigger(dev[0], "GET_ALLOC_FAIL", max_iter=100)
            ev = dev[0].wait_event(["GAS-QUERY-START"], timeout=0.01)
            if ev:
                dev[0].request("DPP_STOP_LISTEN")
                dev[0].wait_event(["GAS-QUERY-DONE"], timeout=3)

def test_dpp_pkex_test_fail(dev, apdev):
    """DPP/PKEX and local failures"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])

    tests = [(1, "dpp_keygen_configurator")]
    for count, func in tests:
        with fail_test(dev[1], count, func):
            cmd = "DPP_CONFIGURATOR_ADD"
            res = dev[1].request(cmd)
            if "FAIL" not in res:
                raise Exception("Unexpected DPP_CONFIGURATOR_ADD success")

    tests = [(1, "dpp_keygen")]
    for count, func in tests:
        with fail_test(dev[1], count, func):
            cmd = "DPP_BOOTSTRAP_GEN type=pkex"
            res = dev[1].request(cmd)
            if "FAIL" not in res:
                raise Exception("Unexpected DPP_BOOTSTRAP_GEN success")

    conf_id = dev[1].dpp_configurator_add()

    id0 = None
    id1 = None

    # Local error cases on the Initiator
    tests = [(1, "aes_siv_encrypt;dpp_auth_build_req"),
             (1, "os_get_random;dpp_auth_init"),
             (1, "dpp_derive_k1;dpp_auth_init"),
             (1, "dpp_hkdf_expand;dpp_derive_k1;dpp_auth_init"),
             (1, "dpp_gen_i_auth;dpp_auth_build_conf"),
             (1, "aes_siv_encrypt;dpp_auth_build_conf"),
             (1, "dpp_derive_k2;dpp_auth_resp_rx"),
             (1, "dpp_hkdf_expand;dpp_derive_k2;dpp_auth_resp_rx"),
             (1, "dpp_derive_bk_ke;dpp_auth_resp_rx"),
             (1, "dpp_hkdf_expand;dpp_derive_bk_ke;dpp_auth_resp_rx"),
             (1, "dpp_gen_r_auth;dpp_auth_resp_rx"),
             (1, "aes_siv_encrypt;dpp_build_conf_resp"),
             (1, "dpp_pkex_derive_Qi;dpp_pkex_build_exchange_req"),
             (1, "aes_siv_encrypt;dpp_pkex_build_commit_reveal_req"),
             (1, "hmac_sha256_vector;dpp_pkex_rx_exchange_resp"),
             (1, "aes_siv_decrypt;dpp_pkex_rx_commit_reveal_resp"),
             (1, "hmac_sha256_vector;dpp_pkex_rx_commit_reveal_resp"),
             (1, "dpp_bootstrap_key_hash")]
    for count, func in tests:
        dev[0].request("DPP_STOP_LISTEN")
        dev[1].request("DPP_STOP_LISTEN")
        dev[0].dump_monitor()
        dev[1].dump_monitor()
        id0 = dev[0].dpp_pkex_resp(2437, identifier="test", code="secret",
                                   use_id=id0)

        with fail_test(dev[1], count, func):
            id1 = dev[1].dpp_pkex_init(identifier="test", code="secret",
                                       use_id=id1,
                                       extra="conf=sta-dpp configurator=%d" % conf_id,
                                       allow_fail=True)
            wait_fail_trigger(dev[1], "GET_FAIL", max_iter=100)
            ev = dev[0].wait_event(["GAS-QUERY-START"], timeout=0.01)
            if ev:
                dev[0].request("DPP_STOP_LISTEN")
                dev[0].wait_event(["GAS-QUERY-DONE"], timeout=3)

    # Local error cases on the Responder
    tests = [(1, "aes_siv_encrypt;dpp_auth_build_resp"),
             (1, "aes_siv_encrypt;dpp_auth_build_resp;dpp_auth_build_resp_ok"),
             (1, "os_get_random;dpp_build_conf_req"),
             (1, "aes_siv_encrypt;dpp_build_conf_req"),
             (1, "os_get_random;dpp_auth_build_resp_ok"),
             (1, "dpp_derive_k2;dpp_auth_build_resp_ok"),
             (1, "dpp_derive_bk_ke;dpp_auth_build_resp_ok"),
             (1, "dpp_gen_r_auth;dpp_auth_build_resp_ok"),
             (1, "aes_siv_encrypt;dpp_auth_build_resp_ok"),
             (1, "dpp_derive_k1;dpp_auth_req_rx"),
             (1, "aes_siv_decrypt;dpp_auth_req_rx"),
             (1, "aes_siv_decrypt;dpp_auth_conf_rx"),
             (1, "dpp_gen_i_auth;dpp_auth_conf_rx"),
             (1, "dpp_check_pubkey_match"),
             (1, "aes_siv_decrypt;dpp_conf_resp_rx"),
             (1, "hmac_sha256_kdf;dpp_pkex_derive_z"),
             (1, "dpp_pkex_derive_Qi;dpp_pkex_rx_exchange_req"),
             (1, "dpp_pkex_derive_Qr;dpp_pkex_rx_exchange_req"),
             (1, "aes_siv_encrypt;dpp_pkex_build_commit_reveal_resp"),
             (1, "aes_siv_decrypt;dpp_pkex_rx_commit_reveal_req"),
             (1, "hmac_sha256_vector;dpp_pkex_rx_commit_reveal_req"),
             (2, "hmac_sha256_vector;dpp_pkex_rx_commit_reveal_req")]
    for count, func in tests:
        dev[0].request("DPP_STOP_LISTEN")
        dev[1].request("DPP_STOP_LISTEN")
        dev[0].dump_monitor()
        dev[1].dump_monitor()
        id0 = dev[0].dpp_pkex_resp(2437, identifier="test", code="secret",
                                   use_id=id0)

        with fail_test(dev[0], count, func):
            id1 = dev[1].dpp_pkex_init(identifier="test", code="secret",
                                       use_id=id1,
                                       extra="conf=sta-dpp configurator=%d" % conf_id)
            wait_fail_trigger(dev[0], "GET_FAIL", max_iter=100)
            ev = dev[0].wait_event(["GAS-QUERY-START"], timeout=0.01)
            if ev:
                dev[0].request("DPP_STOP_LISTEN")
                dev[0].wait_event(["GAS-QUERY-DONE"], timeout=3)

def test_dpp_keygen_configurator_error(dev, apdev):
    """DPP Configurator keygen error case"""
    check_dpp_capab(dev[0])
    if "FAIL" not in dev[0].request("DPP_CONFIGURATOR_ADD curve=unknown"):
        raise Exception("Unexpected success of invalid DPP_CONFIGURATOR_ADD")

def rx_process_frame(dev):
    msg = dev.mgmt_rx()
    if msg is None:
        raise Exception("No management frame RX reported")
    if "OK" not in dev.request("MGMT_RX_PROCESS freq={} datarate={} ssi_signal={} frame={}".format(
        msg['freq'], msg['datarate'], msg['ssi_signal'], binascii.hexlify(msg['frame']).decode())):
        raise Exception("MGMT_RX_PROCESS failed")
    return msg

def wait_auth_success(responder, initiator, configurator=None, enrollee=None,
                      allow_enrollee_failure=False,
                      allow_configurator_failure=False,
                      require_configurator_failure=False,
                      timeout=5, stop_responder=False, stop_initiator=False):
    res = {}
    ev = responder.wait_event(["DPP-AUTH-SUCCESS", "DPP-FAIL"], timeout=timeout)
    if ev is None or "DPP-AUTH-SUCCESS" not in ev:
        raise Exception("DPP authentication did not succeed (Responder)")
    ev = initiator.wait_event(["DPP-AUTH-SUCCESS", "DPP-FAIL"], timeout=5)
    if ev is None or "DPP-AUTH-SUCCESS" not in ev:
        raise Exception("DPP authentication did not succeed (Initiator)")
    if configurator:
        ev = configurator.wait_event(["DPP-CONF-SENT",
                                      "DPP-CONF-FAILED"], timeout=5)
        if ev is None:
            raise Exception("DPP configuration not completed (Configurator)")
        if "DPP-CONF-FAILED" in ev and not allow_configurator_failure:
            raise Exception("DPP configuration did not succeed (Configurator)")
        if "DPP-CONF-SENT" in ev and require_configurator_failure:
            raise Exception("DPP configuration succeeded (Configurator)")
        if "DPP-CONF-SENT" in ev and "wait_conn_status=1" in ev:
            res['wait_conn_status'] = True
    if enrollee:
        ev = enrollee.wait_event(["DPP-CONF-RECEIVED",
                                  "DPP-CONF-FAILED"], timeout=5)
        if ev is None:
            raise Exception("DPP configuration not completed (Enrollee)")
        if "DPP-CONF-FAILED" in ev and not allow_enrollee_failure:
            raise Exception("DPP configuration did not succeed (Enrollee)")
    if stop_responder:
        responder.request("DPP_STOP_LISTEN")
    if stop_initiator:
        initiator.request("DPP_STOP_LISTEN")
    return res

def wait_conf_completion(configurator, enrollee):
    ev = configurator.wait_event(["DPP-CONF-SENT"], timeout=5)
    if ev is None:
        raise Exception("DPP configuration not completed (Configurator)")
    ev = enrollee.wait_event(["DPP-CONF-RECEIVED", "DPP-CONF-FAILED"],
                             timeout=5)
    if ev is None:
        raise Exception("DPP configuration not completed (Enrollee)")

def start_dpp(dev):
    id0 = dev[0].dpp_bootstrap_gen(chan="81/1", mac=True)
    uri0 = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id0)

    conf = '{"wi-fi_tech":"infra", "discovery":{"ssid":"test"},"cred":{"akm":"psk","pass":"secret passphrase"}}' + 3000*' '
    dev[0].set("dpp_config_obj_override", conf)

    dev[0].set("ext_mgmt_frame_handling", "1")
    dev[0].dpp_listen(2412)
    dev[1].dpp_auth_init(uri=uri0, role="enrollee")

def test_dpp_gas_timeout_handling(dev, apdev):
    """DPP and GAS timeout handling"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    start_dpp(dev)

    # DPP Authentication Request
    rx_process_frame(dev[0])

    # DPP Authentication Confirmation
    rx_process_frame(dev[0])

    wait_auth_success(dev[0], dev[1])

    # DPP Configuration Request (GAS Initial Request frame)
    rx_process_frame(dev[0])

    # DPP Configuration Request (GAS Comeback Request frame)
    rx_process_frame(dev[0])

    # Wait for GAS timeout
    ev = dev[1].wait_event(["DPP-CONF-FAILED"], timeout=5)
    if ev is None:
        raise Exception("DPP configuration not completed (Enrollee)")

def test_dpp_gas_comeback_after_failure(dev, apdev):
    """DPP and GAS comeback after failure"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    start_dpp(dev)

    # DPP Authentication Request
    rx_process_frame(dev[0])

    # DPP Authentication Confirmation
    rx_process_frame(dev[0])

    wait_auth_success(dev[0], dev[1])

    # DPP Configuration Request (GAS Initial Request frame)
    rx_process_frame(dev[0])

    # DPP Configuration Request (GAS Comeback Request frame)
    msg = dev[0].mgmt_rx()
    frame = binascii.hexlify(msg['frame']).decode()
    with alloc_fail(dev[0], 1, "gas_build_comeback_resp;gas_server_handle_rx_comeback_req"):
        if "OK" not in dev[0].request("MGMT_RX_PROCESS freq={} datarate={} ssi_signal={} frame={}".format(msg['freq'], msg['datarate'], msg['ssi_signal'], frame)):
            raise Exception("MGMT_RX_PROCESS failed")
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
    # Try the same frame again - this is expected to fail since the response has
    # already been freed.
    if "OK" not in dev[0].request("MGMT_RX_PROCESS freq={} datarate={} ssi_signal={} frame={}".format(msg['freq'], msg['datarate'], msg['ssi_signal'], frame)):
        raise Exception("MGMT_RX_PROCESS failed")

    # DPP Configuration Request (GAS Comeback Request frame retry)
    msg = dev[0].mgmt_rx()

def test_dpp_gas(dev, apdev):
    """DPP and GAS protocol testing"""
    ver0 = check_dpp_capab(dev[0])
    ver1 = check_dpp_capab(dev[1])
    start_dpp(dev)

    # DPP Authentication Request
    rx_process_frame(dev[0])

    # DPP Authentication Confirmation
    rx_process_frame(dev[0])

    wait_auth_success(dev[0], dev[1])

    # DPP Configuration Request (GAS Initial Request frame)
    msg = dev[0].mgmt_rx()

    # Protected Dual of GAS Initial Request frame (dropped by GAS server)
    if msg == None:
        raise Exception("MGMT_RX_PROCESS failed. <Please retry>")
    frame = binascii.hexlify(msg['frame'])
    frame = frame[0:48] + b"09" + frame[50:]
    frame = frame.decode()
    if "OK" not in dev[0].request("MGMT_RX_PROCESS freq={} datarate={} ssi_signal={} frame={}".format(msg['freq'], msg['datarate'], msg['ssi_signal'], frame)):
        raise Exception("MGMT_RX_PROCESS failed")

    with alloc_fail(dev[0], 1, "gas_server_send_resp"):
        frame = binascii.hexlify(msg['frame']).decode()
        if "OK" not in dev[0].request("MGMT_RX_PROCESS freq={} datarate={} ssi_signal={} frame={}".format(msg['freq'], msg['datarate'], msg['ssi_signal'], frame)):
            raise Exception("MGMT_RX_PROCESS failed")
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")

    with alloc_fail(dev[0], 1, "gas_build_initial_resp;gas_server_send_resp"):
        frame = binascii.hexlify(msg['frame']).decode()
        if "OK" not in dev[0].request("MGMT_RX_PROCESS freq={} datarate={} ssi_signal={} frame={}".format(msg['freq'], msg['datarate'], msg['ssi_signal'], frame)):
            raise Exception("MGMT_RX_PROCESS failed")
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")

    # Add extra data after Query Request field to trigger
    # "GAS: Ignored extra data after Query Request field"
    frame = binascii.hexlify(msg['frame']).decode() + "00"
    if "OK" not in dev[0].request("MGMT_RX_PROCESS freq={} datarate={} ssi_signal={} frame={}".format(msg['freq'], msg['datarate'], msg['ssi_signal'], frame)):
        raise Exception("MGMT_RX_PROCESS failed")

    # DPP Configuration Request (GAS Comeback Request frame)
    rx_process_frame(dev[0])

    # DPP Configuration Request (GAS Comeback Request frame)
    rx_process_frame(dev[0])

    # DPP Configuration Request (GAS Comeback Request frame)
    rx_process_frame(dev[0])

    if ver0 >= 2 and ver1 >= 2:
        # DPP Configuration Result
        rx_process_frame(dev[0])

    wait_conf_completion(dev[0], dev[1])

def test_dpp_truncated_attr(dev, apdev):
    """DPP and truncated attribute"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    start_dpp(dev)

    # DPP Authentication Request
    msg = dev[0].mgmt_rx()
    frame = msg['frame']

    # DPP: Truncated message - not enough room for the attribute - dropped
    frame1 = binascii.hexlify(frame[0:36]).decode()
    if "OK" not in dev[0].request("MGMT_RX_PROCESS freq={} datarate={} ssi_signal={} frame={}".format(msg['freq'], msg['datarate'], msg['ssi_signal'], frame1)):
        raise Exception("MGMT_RX_PROCESS failed")
    ev = dev[0].wait_event(["DPP-RX"], timeout=5)
    if ev is None or "ignore=invalid-attributes" not in ev:
        raise Exception("Invalid attribute error not reported")

    # DPP: Unexpected octets (3) after the last attribute
    frame2 = binascii.hexlify(frame).decode() + "000000"
    if "OK" not in dev[0].request("MGMT_RX_PROCESS freq={} datarate={} ssi_signal={} frame={}".format(msg['freq'], msg['datarate'], msg['ssi_signal'], frame2)):
        raise Exception("MGMT_RX_PROCESS failed")
    ev = dev[0].wait_event(["DPP-RX"], timeout=5)
    if ev is None or "ignore=invalid-attributes" not in ev:
        raise Exception("Invalid attribute error not reported")

def test_dpp_bootstrap_key_autogen_issues(dev, apdev):
    """DPP bootstrap key autogen issues"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])

    logger.info("dev0 displays QR Code")
    id0 = dev[0].dpp_bootstrap_gen(chan="81/1", mac=True)
    uri0 = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id0)

    logger.info("dev1 scans QR Code")
    id1 = dev[1].dpp_qr_code(uri0)

    logger.info("dev1 initiates DPP Authentication")
    dev[0].dpp_listen(2412)
    with alloc_fail(dev[1], 1, "dpp_autogen_bootstrap_key"):
        dev[1].dpp_auth_init(peer=id1, expect_fail=True)
    with alloc_fail(dev[1], 1, "dpp_gen_uri;dpp_autogen_bootstrap_key"):
        dev[1].dpp_auth_init(peer=id1, expect_fail=True)
    with fail_test(dev[1], 1, "dpp_keygen;dpp_autogen_bootstrap_key"):
        dev[1].dpp_auth_init(peer=id1, expect_fail=True)
    dev[0].request("DPP_STOP_LISTEN")

def test_dpp_auth_resp_status_failure(dev, apdev):
    """DPP and Auth Resp(status) build failure"""
    with alloc_fail(dev[0], 1, "dpp_auth_build_resp"):
        run_dpp_proto_auth_resp_missing(dev, 99999, None,
                                        incompatible_roles=True)

def test_dpp_auth_resp_aes_siv_issue(dev, apdev):
    """DPP Auth Resp AES-SIV issue"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    logger.info("dev0 displays QR Code")
    id0 = dev[0].dpp_bootstrap_gen(chan="81/1", mac=True)
    uri0 = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id0)
    logger.info("dev1 scans QR Code and initiates DPP Authentication")
    dev[0].dpp_listen(2412)
    with fail_test(dev[1], 1, "aes_siv_decrypt;dpp_auth_resp_rx"):
        dev[1].dpp_auth_init(uri=uri0)
        wait_dpp_fail(dev[1], "AES-SIV decryption failed")
    dev[0].request("DPP_STOP_LISTEN")

def test_dpp_invalid_legacy_params(dev, apdev):
    """DPP invalid legacy parameters"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    id0 = dev[0].dpp_bootstrap_gen(chan="81/1", mac=True)
    uri0 = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id0)
    # No pass/psk
    dev[1].dpp_auth_init(uri=uri0, conf="sta-psk", ssid="dpp-legacy",
                         expect_fail=True)

def test_dpp_invalid_legacy_params2(dev, apdev):
    """DPP invalid legacy parameters 2"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    id0 = dev[0].dpp_bootstrap_gen(chan="81/1", mac=True)
    uri0 = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id0)
    dev[0].set("dpp_configurator_params",
               " conf=sta-psk ssid=%s" % (binascii.hexlify(b"dpp-legacy").decode()))
    dev[0].dpp_listen(2412, role="configurator")
    dev[1].dpp_auth_init(uri=uri0, role="enrollee")
    # No pass/psk
    ev = dev[0].wait_event(["DPP: Failed to set configurator parameters"],
                           timeout=5)
    if ev is None:
        raise Exception("DPP configuration failure not reported")

def test_dpp_legacy_params_failure(dev, apdev):
    """DPP legacy parameters local failure"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    id0 = dev[0].dpp_bootstrap_gen(chan="81/1", mac=True)
    uri0 = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id0)
    dev[0].dpp_listen(2412)
    with alloc_fail(dev[1], 1, "dpp_build_conf_obj_legacy"):
        dev[1].dpp_auth_init(uri=uri0, conf="sta-psk", passphrase="passphrase",
                             ssid="dpp-legacy")
        ev = dev[0].wait_event(["DPP-CONF-FAILED"], timeout=5)
        if ev is None:
            raise Exception("DPP configuration failure not reported")

def test_dpp_invalid_configurator_key(dev, apdev):
    """DPP invalid configurator key"""
    check_dpp_capab(dev[0])

    if "FAIL" not in dev[0].request("DPP_CONFIGURATOR_ADD key=aa"):
        raise Exception("Invalid key accepted")

    with alloc_fail(dev[0], 1, "dpp_keygen_configurator"):
        if "FAIL" not in dev[0].request("DPP_CONFIGURATOR_ADD key=" + dpp_key_p256):
            raise Exception("Error not reported")

    with alloc_fail(dev[0], 1,
                    "crypto_ec_key_get_pubkey_point;dpp_keygen_configurator"):
        if "FAIL" not in dev[0].request("DPP_CONFIGURATOR_ADD key=" + dpp_key_p256):
            raise Exception("Error not reported")

    with alloc_fail(dev[0], 1, "base64_gen_encode;dpp_keygen_configurator"):
        if "FAIL" not in dev[0].request("DPP_CONFIGURATOR_ADD key=" + dpp_key_p256):
            raise Exception("Error not reported")

    with fail_test(dev[0], 1, "dpp_keygen_configurator"):
        if "FAIL" not in dev[0].request("DPP_CONFIGURATOR_ADD key=" + dpp_key_p256):
            raise Exception("Error not reported")

def test_dpp_own_config_sign_fail(dev, apdev):
    """DPP own config signing failure"""
    check_dpp_capab(dev[0])
    conf_id = dev[0].dpp_configurator_add()
    tests = ["",
             " ",
             " conf=sta-dpp",
             " configurator=%d" % conf_id,
             " conf=sta-dpp configurator=%d curve=unsupported" % conf_id]
    for t in tests:
        if "FAIL" not in dev[0].request("DPP_CONFIGURATOR_SIGN " + t):
            raise Exception("Invalid command accepted: " + t)

def test_dpp_peer_intro_failures(dev, apdev):
    """DPP peer introduction failures"""
    try:
        run_dpp_peer_intro_failures(dev, apdev)
    finally:
        dev[0].set("dpp_config_processing", "0", allow_fail=True)

def run_dpp_peer_intro_failures(dev, apdev):
    check_dpp_capab(dev[0])
    hapd = hostapd.add_ap(apdev[0], {"ssid": "unconfigured"})
    check_dpp_capab(hapd)

    conf_id = hapd.dpp_configurator_add(key=dpp_key_p256)
    csign = hapd.request("DPP_CONFIGURATOR_GET_KEY %d" % conf_id)
    if "FAIL" in csign or len(csign) == 0:
        raise Exception("DPP_CONFIGURATOR_GET_KEY failed")

    conf_id2 = dev[0].dpp_configurator_add(key=csign)
    csign2 = dev[0].request("DPP_CONFIGURATOR_GET_KEY %d" % conf_id2)

    if csign != csign2:
        raise Exception("Unexpected difference in configurator key")

    cmd = "DPP_CONFIGURATOR_SIGN  conf=ap-dpp configurator=%d" % conf_id
    res = hapd.request(cmd)
    if "FAIL" in res:
        raise Exception("Failed to generate own configuration")
    update_hapd_config(hapd)

    dev[0].set("dpp_config_processing", "1")
    cmd = "DPP_CONFIGURATOR_SIGN  conf=sta-dpp configurator=%d" % conf_id
    res = dev[0].request(cmd)
    if "FAIL" in res:
        raise Exception("Failed to generate own configuration")
    ev = dev[0].wait_event(["DPP-NETWORK-ID"], timeout=1)
    if ev is None:
        raise Exception("DPP network profile not generated")
    id = ev.split(' ')[1]
    dev[0].select_network(id, freq=2412)
    dev[0].wait_connected()
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()
    dev[0].dump_monitor()

    tests = ["eyJ0eXAiOiJkcHBDb24iLCJraWQiOiIwTlNSNTlxRTc0alFfZTFLVGVPV1lYY1pTWnFUaDdNXzU0aHJPcFRpaFJnIiwiYWxnIjoiRVMyNTYifQ.eyJncm91cHMiOltdLCJuZXRBY2Nlc3NLZXkiOnsia3R5IjoiRUMiLCJjcnYiOiJQLTI1NiIsIngiOiJiVmFMRGlBT09OQmFjcVFVN1pYamFBVEtEMVhhbDVlUExqOUZFZUl3VkN3IiwieSI6Il95c25JR1hTYjBvNEsyMWg0anZmSkZxMHdVNnlPNWp1VUFPd3FuM0dHVHMifX0.WgzZBOJaisWBRxvtXPbVYPXU7OIZxs6sZD-cPOLmJVTIYZKdMkSOMvP5b6si_j61FIrjhm43tmGq1P6cpoxB_g",
             "eyJ0eXAiOiJkcHBDb24iLCJraWQiOiIwTlNSNTlxRTc0alFfZTFLVGVPV1lYY1pTWnFUaDdNXzU0aHJPcFRpaFJnIiwiYWxnIjoiRVMyNTYifQ.eyJncm91cHMiOlt7fV0sIm5ldEFjY2Vzc0tleSI6eyJrdHkiOiJFQyIsImNydiI6IlAtMjU2IiwieCI6IkJhY3BWSDNpNDBrZklNS0RHa1FFRzhCODBCaEk4cEFmTWpLbzM5NlFZT2ciLCJ5IjoiMjBDYjhDNjRsSjFzQzV2NXlKMnBFZXRRempxMjI4YVV2cHMxNmQ0M3EwQSJ9fQ.dG2y8VvZQJ5hfob8E5F2FAeR7Nd700qstYkxDgA2QfARaNMZ0_SfKfoG-yKXsIZNM-TvGBfACgfhagG9Oaw_Xw",
             "eyJ0eXAiOiJkcHBDb24iLCJraWQiOiIwTlNSNTlxRTc0alFfZTFLVGVPV1lYY1pTWnFUaDdNXzU0aHJPcFRpaFJnIiwiYWxnIjoiRVMyNTYifQ.eyJncm91cHMiOlt7Imdyb3VwSWQiOiIqIn1dLCJuZXRBY2Nlc3NLZXkiOnsia3R5IjoiRUMiLCJjcnYiOiJQLTI1NiIsIngiOiJkc2VmcmJWWlhad0RMWHRpLWlObDBBYkFIOXpqeFFKd0R1SUd5NzNuZGU0IiwieSI6IjZFQnExN3cwYW1fZlh1OUQ4UGxWYk9XZ2I3b19DcTUxWHlmSG8wcHJyeDQifX0.caBvdDUtXrhnS61-juVZ_2FQdprepv0yZjC04G4ERvLUpeX7cgu0Hp-A1aFDogP1PEFGpkaEdcAWRQnSSRiIKQ"]
    for t in tests:
        dev[0].set_network_quoted(id, "dpp_connector", t)
        dev[0].select_network(id, freq=2412)
        ev = dev[0].wait_event(["DPP-INTRO"], timeout=5)
        if ev is None or "status=8" not in ev:
            raise Exception("Introduction failure not reported")
        dev[0].request("DISCONNECT")
        dev[0].dump_monitor()

def test_dpp_peer_intro_local_failures(dev, apdev):
    """DPP peer introduction local failures"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])

    params = {"ssid": "dpp",
              "wpa": "2",
              "wpa_key_mgmt": "DPP",
              "ieee80211w": "2",
              "rsn_pairwise": "CCMP",
              "dpp_connector": params1_ap_connector,
              "dpp_csign": params1_csign,
              "dpp_netaccesskey": params1_ap_netaccesskey}
    try:
        hapd = hostapd.add_ap(apdev[0], params)
    except:
        raise HwsimSkip("DPP not supported")

    tests = ["dpp_derive_pmk",
             "dpp_hkdf_expand;dpp_derive_pmk",
             "dpp_derive_pmkid"]
    for func in tests:
        with fail_test(dev[0], 1, func):
            dev[0].connect("dpp", key_mgmt="DPP", scan_freq="2412",
                           ieee80211w="2",
                           dpp_csign=params1_csign,
                           dpp_connector=params1_sta_connector,
                           dpp_netaccesskey=params1_sta_netaccesskey,
                           wait_connect=False)
            ev = dev[0].wait_event(["DPP-INTRO"], timeout=10)
            if ev is None or "fail=peer_connector_validation_failed" not in ev:
                raise Exception("Introduction failure not reported")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].dump_monitor()

    tests = [(1, "base64_gen_decode;dpp_peer_intro"),
             (1, "json_parse;dpp_peer_intro"),
             (50, "json_parse;dpp_peer_intro"),
             (1, "=dpp_check_signed_connector;dpp_peer_intro"),
             (1, "dpp_parse_jwk")]
    for count, func in tests:
        with alloc_fail(dev[0], count, func):
            dev[0].connect("dpp", key_mgmt="DPP", scan_freq="2412",
                           ieee80211w="2",
                           dpp_csign=params1_csign,
                           dpp_connector=params1_sta_connector,
                           dpp_netaccesskey=params1_sta_netaccesskey,
                           wait_connect=False)
            ev = dev[0].wait_event(["DPP-INTRO"], timeout=10)
            if ev is None or "fail=peer_connector_validation_failed" not in ev:
                raise Exception("Introduction failure not reported")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].dump_monitor()

    parts = params1_ap_connector.split('.')
    for ap_connector in ['.'.join(parts[0:2]), '.'.join(parts[0:1])]:
        hapd.set("dpp_connector", ap_connector)
        dev[0].connect("dpp", key_mgmt="DPP", scan_freq="2412",
                       ieee80211w="2",
                       dpp_csign=params1_csign,
                       dpp_connector=params1_sta_connector,
                       dpp_netaccesskey=params1_sta_netaccesskey,
                       wait_connect=False)
        ev = dev[0].wait_event(["DPP-TX-STATUS"], timeout=10)
        if ev is None:
            raise Exception("No TX status reported")
        dev[0].request("REMOVE_NETWORK all")
        dev[0].dump_monitor()

    hapd.set("dpp_netaccesskey", "00")
    dev[0].connect("dpp", key_mgmt="DPP", scan_freq="2412",
                   ieee80211w="2",
                   dpp_csign=params1_csign,
                   dpp_connector=params1_sta_connector,
                   dpp_netaccesskey=params1_sta_netaccesskey,
                   wait_connect=False)
    ev = dev[0].wait_event(["DPP-TX-STATUS"], timeout=10)
    if ev is None:
        raise Exception("No TX status reported")
    dev[0].request("REMOVE_NETWORK all")
    dev[0].dump_monitor()

    hapd.set("dpp_csign", "00")
    dev[0].connect("dpp", key_mgmt="DPP", scan_freq="2412",
                   ieee80211w="2",
                   dpp_csign=params1_csign,
                   dpp_connector=params1_sta_connector,
                   dpp_netaccesskey=params1_sta_netaccesskey,
                   wait_connect=False)
    ev = dev[0].wait_event(["DPP-TX-STATUS"], timeout=10)
    if ev is None:
        raise Exception("No TX status reported")
    dev[0].request("REMOVE_NETWORK all")

def run_dpp_configurator_id_unknown(dev):
    check_dpp_capab(dev)
    conf_id = dev.dpp_configurator_add()
    if "FAIL" not in dev.request("DPP_CONFIGURATOR_GET_KEY %d" % (conf_id + 1)):
        raise Exception("DPP_CONFIGURATOR_GET_KEY with incorrect id accepted")

    cmd = "DPP_CONFIGURATOR_SIGN  conf=sta-dpp configurator=%d" % (conf_id + 1)
    if "FAIL" not in dev.request(cmd):
        raise Exception("DPP_CONFIGURATOR_SIGN with incorrect id accepted")

def test_dpp_configurator_id_unknown(dev, apdev):
    """DPP and unknown configurator id"""
    run_dpp_configurator_id_unknown(dev[0])
    hapd = hostapd.add_ap(apdev[0], {"ssid": "unconfigured"})
    run_dpp_configurator_id_unknown(hapd)

def run_dpp_bootstrap_gen_failures(dev):
    check_dpp_capab(dev)

    tests = ["type=unsupported",
             "type=qrcode chan=-1",
             "type=qrcode mac=a",
             "type=qrcode key=qq",
             "type=qrcode key=",
             "type=qrcode info=abc\tdef"]
    for t in tests:
        if "FAIL" not in dev.request("DPP_BOOTSTRAP_GEN " + t):
            raise Exception("Command accepted unexpectedly")

    id = dev.dpp_bootstrap_gen()
    uri = dev.request("DPP_BOOTSTRAP_GET_URI %d" % id)
    if not uri.startswith("DPP:"):
        raise Exception("Could not get URI")
    if "FAIL" not in dev.request("DPP_BOOTSTRAP_GET_URI 0"):
        raise Exception("Failure not reported")
    info = dev.request("DPP_BOOTSTRAP_INFO %d" % id)
    if not info.startswith("type=QRCODE"):
        raise Exception("Could not get info")
    if "FAIL" not in dev.request("DPP_BOOTSTRAP_REMOVE 0"):
        raise Exception("Failure not reported")
    if "FAIL" in dev.request("DPP_BOOTSTRAP_REMOVE *"):
        raise Exception("Failed to remove bootstrap info")
    if "FAIL" not in dev.request("DPP_BOOTSTRAP_GET_URI %d" % id):
        raise Exception("Failure not reported")
    if "FAIL" not in dev.request("DPP_BOOTSTRAP_INFO %d" % id):
        raise Exception("Failure not reported")

    func = "dpp_bootstrap_gen"
    with alloc_fail(dev, 1, "=" + func):
        if "FAIL" not in dev.request("DPP_BOOTSTRAP_GEN type=qrcode"):
            raise Exception("Command accepted unexpectedly")

    with alloc_fail(dev, 1, "dpp_gen_uri;dpp_bootstrap_gen"):
        if "FAIL" not in dev.request("DPP_BOOTSTRAP_GEN type=qrcode"):
            raise Exception("Command accepted unexpectedly")

    with alloc_fail(dev, 1, "get_param"):
        dev.request("DPP_BOOTSTRAP_GEN type=qrcode curve=foo")

def test_dpp_bootstrap_gen_failures(dev, apdev):
    """DPP_BOOTSTRAP_GEN/REMOVE/GET_URI/INFO error cases"""
    run_dpp_bootstrap_gen_failures(dev[0])
    hapd = hostapd.add_ap(apdev[0], {"ssid": "unconfigured"})
    run_dpp_bootstrap_gen_failures(hapd)

def test_dpp_listen_continue(dev, apdev):
    """DPP and continue listen state"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    id = dev[0].dpp_bootstrap_gen(chan="81/1", mac=True)
    uri = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id)
    dev[0].dpp_listen(2412)
    time.sleep(5.1)
    dev[1].dpp_auth_init(uri=uri)
    wait_auth_success(dev[0], dev[1], configurator=dev[1], enrollee=dev[0],
                      allow_enrollee_failure=True, stop_responder=True,
                      stop_initiator=True)

def test_dpp_network_addition_failure(dev, apdev):
    """DPP network addition failure"""
    try:
        run_dpp_network_addition_failure(dev, apdev)
    finally:
        dev[0].set("dpp_config_processing", "0", allow_fail=True)

def run_dpp_network_addition_failure(dev, apdev):
    check_dpp_capab(dev[0])
    conf_id = dev[0].dpp_configurator_add()
    dev[0].set("dpp_config_processing", "1")
    cmd = "DPP_CONFIGURATOR_SIGN  conf=sta-dpp configurator=%d" % conf_id
    tests = [(1, "=wpas_dpp_add_network"),
             (2, "=wpas_dpp_add_network"),
             (3, "=wpas_dpp_add_network"),
             (4, "=wpas_dpp_add_network"),
             (1, "wpa_config_add_network;wpas_dpp_add_network")]
    for count, func in tests:
        with alloc_fail(dev[0], count, func):
            res = dev[0].request(cmd)
            if "OK" in res:
                ev = dev[0].wait_event(["DPP-NET-ACCESS-KEY"], timeout=2)
                if ev is None:
                    raise Exception("Config object not processed")
            wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
        dev[0].dump_monitor()

    cmd = "DPP_CONFIGURATOR_SIGN  conf=sta-psk pass=%s configurator=%d" % (binascii.hexlify(b"passphrase").decode(), conf_id)
    tests = [(1, "wpa_config_set_quoted;wpas_dpp_add_network")]
    for count, func in tests:
        with alloc_fail(dev[0], count, func):
            res = dev[0].request(cmd)
            if "OK" in res:
                ev = dev[0].wait_event(["DPP-NET-ACCESS-KEY"], timeout=2)
                if ev is None:
                    raise Exception("Config object not processed")
            wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
        dev[0].dump_monitor()

def test_dpp_two_initiators(dev, apdev):
    """DPP and two initiators"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    check_dpp_capab(dev[2])
    id = dev[0].dpp_bootstrap_gen(chan="81/1", mac=True)
    uri = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id)
    dev[0].dpp_listen(2412)
    dev[1].dpp_auth_init(uri=uri)
    ev = dev[0].wait_event(["DPP-RX"], timeout=5)
    if ev is None:
        raise Exeption("No DPP Authentication Request seen")
    dev[2].dpp_auth_init(uri=uri)
    wait_dpp_fail(dev[0],
                  "DPP-FAIL Already in DPP authentication exchange - ignore new one")

    ev = dev[0].wait_event(["DPP-CONF-FAILED"], timeout=2)
    if ev is None:
        raise Exception("DPP configuration result not seen (Enrollee)")
    ev = dev[1].wait_event(["DPP-CONF-SENT"], timeout=2)
    if ev is None:
        raise Exception("DPP configuration result not seen (Responder)")

    dev[0].request("DPP_STOP_LISTEN")
    dev[1].request("DPP_STOP_LISTEN")
    dev[2].request("DPP_STOP_LISTEN")

def test_dpp_conf_file_update(dev, apdev, params):
    """DPP provisioning updating wpa_supplicant configuration file"""
    config = os.path.join(params['logdir'], 'dpp_conf_file_update.conf')
    with open(config, "w") as f:
        f.write("update_config=1\n")
    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    wpas.interface_add("wlan5", config=config)
    check_dpp_capab(wpas)
    wpas.set("dpp_config_processing", "1")
    run_dpp_qr_code_auth_unicast([wpas, dev[1]], apdev, None,
                                 init_extra="conf=sta-dpp",
                                 require_conf_success=True,
                                 configurator=True)
    wpas.interface_remove("wlan5")

    with open(config, "r") as f:
        res = f.read()
    for i in ["network={", "dpp_connector=", "key_mgmt=DPP", "ieee80211w=2",
              "dpp_netaccesskey=", "dpp_csign="]:
        if i not in res:
            raise Exception("Configuration file missing '%s'" % i)

    wpas.interface_add("wlan5", config=config)
    if len(wpas.list_networks()) != 1:
        raise Exception("Unexpected number of networks")

def test_dpp_duplicated_auth_resp(dev, apdev):
    """DPP and duplicated Authentication Response"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    id0 = dev[0].dpp_bootstrap_gen(chan="81/1", mac=True)
    uri0 = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id0)
    dev[0].set("ext_mgmt_frame_handling", "1")
    dev[1].set("ext_mgmt_frame_handling", "1")
    dev[0].dpp_listen(2412)
    dev[1].dpp_auth_init(uri=uri0)

    # DPP Authentication Request
    rx_process_frame(dev[0])

    # DPP Authentication Response
    msg = rx_process_frame(dev[1])
    frame = binascii.hexlify(msg['frame']).decode()
    # Duplicated frame
    if "OK" not in dev[1].request("MGMT_RX_PROCESS freq={} datarate={} ssi_signal={} frame={}".format(msg['freq'], msg['datarate'], msg['ssi_signal'], frame)):
        raise Exception("MGMT_RX_PROCESS failed")
    # Modified frame - nonzero status
    if frame[2*32:2*37] != "0010010000":
        raise Exception("Could not find Status attribute")
    frame2 = frame[0:2*32] + "0010010001" + frame[2*37:]
    if "OK" not in dev[1].request("MGMT_RX_PROCESS freq={} datarate={} ssi_signal={} frame={}".format(msg['freq'], msg['datarate'], msg['ssi_signal'], frame2)):
        raise Exception("MGMT_RX_PROCESS failed")
    frame2 = frame[0:2*32] + "00100100ff" + frame[2*37:]
    if "OK" not in dev[1].request("MGMT_RX_PROCESS freq={} datarate={} ssi_signal={} frame={}".format(msg['freq'], msg['datarate'], msg['ssi_signal'], frame2)):
        raise Exception("MGMT_RX_PROCESS failed")

    # DPP Authentication Confirmation
    rx_process_frame(dev[0])

    wait_auth_success(dev[0], dev[1])

    # DPP Configuration Request
    rx_process_frame(dev[1])

    # DPP Configuration Response
    rx_process_frame(dev[0])

    wait_conf_completion(dev[1], dev[0])

def test_dpp_duplicated_auth_conf(dev, apdev):
    """DPP and duplicated Authentication Confirmation"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    id0 = dev[0].dpp_bootstrap_gen(chan="81/1", mac=True)
    uri0 = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id0)
    dev[0].set("ext_mgmt_frame_handling", "1")
    dev[1].set("ext_mgmt_frame_handling", "1")
    dev[0].dpp_listen(2412)
    dev[1].dpp_auth_init(uri=uri0)

    # DPP Authentication Request
    rx_process_frame(dev[0])

    # DPP Authentication Response
    rx_process_frame(dev[1])

    # DPP Authentication Confirmation
    msg = rx_process_frame(dev[0])
    # Duplicated frame
    if "OK" not in dev[0].request("MGMT_RX_PROCESS freq={} datarate={} ssi_signal={} frame={}".format(msg['freq'], msg['datarate'], msg['ssi_signal'], binascii.hexlify(msg['frame']).decode())):
        raise Exception("MGMT_RX_PROCESS failed")

    wait_auth_success(dev[0], dev[1])

    # DPP Configuration Request
    rx_process_frame(dev[1])

    # DPP Configuration Response
    rx_process_frame(dev[0])

    wait_conf_completion(dev[1], dev[0])

def test_dpp_enrollee_reject_config(dev, apdev):
    """DPP and Enrollee rejecting Config Object"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    dev[0].set("dpp_test", "91")
    id0 = dev[0].dpp_bootstrap_gen(chan="81/1", mac=True)
    uri0 = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id0)
    dev[0].dpp_listen(2412)
    dev[1].dpp_auth_init(uri=uri0, conf="sta-sae", ssid="dpp-legacy",
                         passphrase="secret passphrase")
    wait_auth_success(dev[0], dev[1], configurator=dev[1], enrollee=dev[0],
                      allow_enrollee_failure=True,
                      allow_configurator_failure=True)

def test_dpp_enrollee_ap_reject_config(dev, apdev):
    """DPP and Enrollee AP rejecting Config Object"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    hapd = hostapd.add_ap(apdev[0], {"ssid": "unconfigured"})
    check_dpp_capab(hapd)
    hapd.set("dpp_test", "91")
    conf_id = dev[0].dpp_configurator_add()
    id_h = hapd.dpp_bootstrap_gen(chan="81/1", mac=True)
    uri = hapd.request("DPP_BOOTSTRAP_GET_URI %d" % id_h)
    dev[0].dpp_auth_init(uri=uri, conf="ap-dpp", configurator=conf_id)
    wait_auth_success(hapd, dev[0], configurator=dev[0], enrollee=hapd,
                      allow_enrollee_failure=True,
                      allow_configurator_failure=True)

def test_dpp_legacy_and_dpp_akm(dev, apdev):
    """DPP and provisoning DPP and legacy AKMs"""
    try:
        run_dpp_legacy_and_dpp_akm(dev, apdev)
    finally:
        dev[0].set("dpp_config_processing", "0", allow_fail=True)

def run_dpp_legacy_and_dpp_akm(dev, apdev):
    check_dpp_capab(dev[0], min_ver=2)
    check_dpp_capab(dev[1], min_ver=2)

    csign = "30770201010420768240a3fc89d6662d9782f120527fe7fb9edc6366ab0b9c7dde96125cfd250fa00a06082a8648ce3d030107a144034200042908e1baf7bf413cc66f9e878a03e8bb1835ba94b033dbe3d6969fc8575d5eb5dfda1cb81c95cee21d0cd7d92ba30541ffa05cb6296f5dd808b0c1c2a83c0708"
    csign_pub = "3059301306072a8648ce3d020106082a8648ce3d030107034200042908e1baf7bf413cc66f9e878a03e8bb1835ba94b033dbe3d6969fc8575d5eb5dfda1cb81c95cee21d0cd7d92ba30541ffa05cb6296f5dd808b0c1c2a83c0708"
    ap_connector = "eyJ0eXAiOiJkcHBDb24iLCJraWQiOiJwYWtZbXVzd1dCdWpSYTl5OEsweDViaTVrT3VNT3dzZHRlaml2UG55ZHZzIiwiYWxnIjoiRVMyNTYifQ.eyJncm91cHMiOlt7Imdyb3VwSWQiOiIqIiwibmV0Um9sZSI6ImFwIn1dLCJuZXRBY2Nlc3NLZXkiOnsia3R5IjoiRUMiLCJjcnYiOiJQLTI1NiIsIngiOiIybU5vNXZuRkI5bEw3d1VWb1hJbGVPYzBNSEE1QXZKbnpwZXZULVVTYzVNIiwieSI6IlhzS3dqVHJlLTg5WWdpU3pKaG9CN1haeUttTU05OTl3V2ZaSVl0bi01Q3MifX0.XhjFpZgcSa7G2lHy0OCYTvaZFRo5Hyx6b7g7oYyusLC7C_73AJ4_BxEZQVYJXAtDuGvb3dXSkHEKxREP9Q6Qeg"
    ap_netaccesskey = "30770201010420ceba752db2ad5200fa7bc565b9c05c69b7eb006751b0b329b0279de1c19ca67ca00a06082a8648ce3d030107a14403420004da6368e6f9c507d94bef0515a1722578e73430703902f267ce97af4fe51273935ec2b08d3adefbcf588224b3261a01ed76722a630cf7df7059f64862d9fee42b"

    ssid = "dpp-both"
    passphrase = "secret passphrase"
    params = {"ssid": ssid,
              "wpa": "2",
              "wpa_key_mgmt": "DPP WPA-PSK SAE",
              "ieee80211w": "1",
              "sae_require_mfp": '1',
              "rsn_pairwise": "CCMP",
              "wpa_passphrase": passphrase,
              "dpp_connector": ap_connector,
              "dpp_csign": csign_pub,
              "dpp_netaccesskey": ap_netaccesskey}
    try:
        hapd = hostapd.add_ap(apdev[0], params)
    except:
        raise HwsimSkip("DPP not supported")

    dev[0].request("SET sae_groups ")
    conf_id = dev[1].dpp_configurator_add(key=csign)
    dev[0].set("dpp_config_processing", "1")
    id0 = dev[0].dpp_bootstrap_gen(chan="81/1", mac=True)
    uri0 = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id0)
    dev[0].dpp_listen(2412)
    dev[1].dpp_auth_init(uri=uri0, conf="sta-psk-sae-dpp", ssid=ssid,
                         passphrase=passphrase, configurator=conf_id)
    wait_auth_success(dev[0], dev[1], configurator=dev[1], enrollee=dev[0],
                      allow_enrollee_failure=True,
                      allow_configurator_failure=True)
    ev = dev[0].wait_event(["DPP-NETWORK-ID"], timeout=1)
    if ev is None:
        raise Exception("DPP network profile not generated")
    id0 = ev.split(' ')[1]

    key_mgmt = dev[0].get_network(id0, "key_mgmt").split(' ')
    for m in ["SAE", "WPA-PSK", "DPP"]:
        if m not in key_mgmt:
            raise Exception("%s missing from key_mgmt" % m)

    dev[0].scan_for_bss(hapd.own_addr(), freq=2412)
    dev[0].select_network(id0, freq=2412)
    dev[0].wait_connected()

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()
    hapd.disable()

    params = {"ssid": ssid,
              "wpa": "2",
              "wpa_key_mgmt": "WPA-PSK SAE",
              "ieee80211w": "1",
              "sae_require_mfp": '1',
              "rsn_pairwise": "CCMP",
              "wpa_passphrase": passphrase}
    hapd2 = hostapd.add_ap(apdev[1], params)

    dev[0].request("BSS_FLUSH 0")
    dev[0].scan_for_bss(hapd2.own_addr(), freq=2412, force_scan=True,
                        only_new=True)
    dev[0].select_network(id0, freq=2412)
    dev[0].wait_connected()

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

def test_dpp_controller_relay(dev, apdev, params):
    """DPP Controller/Relay"""
    try:
        run_dpp_controller_relay(dev, apdev, params)
    finally:
        dev[0].set("dpp_config_processing", "0", allow_fail=True)
        dev[1].request("DPP_CONTROLLER_STOP")

def test_dpp_controller_relay_chirp(dev, apdev, params):
    """DPP Controller/Relay with chirping"""
    try:
        run_dpp_controller_relay(dev, apdev, params, chirp=True)
    finally:
        dev[0].set("dpp_config_processing", "0", allow_fail=True)
        dev[1].request("DPP_CONTROLLER_STOP")

def run_dpp_controller_relay(dev, apdev, params, chirp=False):
    check_dpp_capab(dev[0], min_ver=2)
    check_dpp_capab(dev[1], min_ver=2)
    prefix = "dpp_controller_relay"
    if chirp:
        prefix += "_chirp"
    cap_lo = os.path.join(params['logdir'], prefix + ".lo.pcap")

    wt = WlantestCapture('lo', cap_lo)

    # Controller
    conf_id = dev[1].dpp_configurator_add()
    dev[1].set("dpp_configurator_params",
               "conf=sta-dpp configurator=%d" % conf_id)
    id_c = dev[1].dpp_bootstrap_gen()
    uri_c = dev[1].request("DPP_BOOTSTRAP_GET_URI %d" % id_c)
    res = dev[1].request("DPP_BOOTSTRAP_INFO %d" % id_c)
    pkhash = None
    for line in res.splitlines():
        name, value = line.split('=')
        if name == "pkhash":
            pkhash = value
            break
    if not pkhash:
        raise Exception("Could not fetch public key hash from Controller")
    if "OK" not in dev[1].request("DPP_CONTROLLER_START"):
        raise Exception("Failed to start Controller")

    # Relay
    params = {"ssid": "unconfigured",
              "channel": "6",
              "dpp_controller": "ipaddr=127.0.0.1 pkhash=" + pkhash}
    if chirp:
        params["channel"] = "11"
        params["dpp_configurator_connectivity"] = "1"
    relay = hostapd.add_ap(apdev[1], params)
    check_dpp_capab(relay)

    # Enroll Relay to the network
    # TODO: Do this over TCP once direct Enrollee-over-TCP case is supported
    if chirp:
        id_h = relay.dpp_bootstrap_gen(chan="81/11", mac=True)
    else:
        id_h = relay.dpp_bootstrap_gen(chan="81/6", mac=True)
    uri_r = relay.request("DPP_BOOTSTRAP_GET_URI %d" % id_h)
    dev[1].dpp_auth_init(uri=uri_r, conf="ap-dpp", configurator=conf_id)
    wait_auth_success(relay, dev[1], configurator=dev[1], enrollee=relay)
    update_hapd_config(relay)

    # Initiate from Enrollee with broadcast DPP Authentication Request or
    # using chirping
    dev[0].set("dpp_config_processing", "2")
    if chirp:
        id1 = dev[0].dpp_bootstrap_gen()
        uri = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id1)
        idc = dev[1].dpp_qr_code(uri)
        dev[1].dpp_bootstrap_set(idc, conf="sta-dpp", configurator=conf_id)
        if "OK" not in dev[0].request("DPP_CHIRP own=%d iter=5" % id1):
            raise Exception("DPP_CHIRP failed")
        ev = relay.wait_event(["DPP-RX"], timeout=10)
        if ev is None:
            raise Exception("Presence Announcement not seen")
        if "type=13" not in ev:
            raise Exception("Unexpected DPP frame received: " + ev)
    else:
        dev[0].dpp_auth_init(uri=uri_c, role="enrollee")
    wait_auth_success(dev[1], dev[0], configurator=dev[1], enrollee=dev[0],
                      allow_enrollee_failure=True,
                      allow_configurator_failure=True)
    ev = dev[0].wait_event(["DPP-NETWORK-ID"], timeout=1)
    if ev is None:
        raise Exception("DPP network id not reported")
    network = int(ev.split(' ')[1])
    dev[0].wait_connected()
    dev[0].dump_monitor()
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()
    dev[0].dump_monitor()

    if "OK" not in dev[0].request("DPP_RECONFIG %s" % network):
        raise Exception("Failed to start reconfiguration")
    ev = dev[0].wait_event(["DPP-NETWORK-ID"], timeout=15)
    if ev is None:
        raise Exception("DPP network id not reported for reconfiguration")
    network2 = int(ev.split(' ')[1])
    if network == network2:
        raise Exception("Network ID did not change")
    dev[0].wait_connected()

    time.sleep(0.5)
    wt.close()

class MyTCPServer(TCPServer):
    def __init__(self, addr, handler):
        self.allow_reuse_address = True
        TCPServer.__init__(self, addr, handler)

class DPPControllerServer(StreamRequestHandler):
        def handle(self):
            data = self.rfile.read()
            # Do not reply

def test_dpp_relay_incomplete_connections(dev, apdev):
    """DPP Relay and incomplete connections"""
    check_dpp_capab(dev[0], min_ver=2)
    check_dpp_capab(dev[1], min_ver=2)

    id_c = dev[1].dpp_bootstrap_gen()
    uri_c = dev[1].request("DPP_BOOTSTRAP_GET_URI %d" % id_c)
    res = dev[1].request("DPP_BOOTSTRAP_INFO %d" % id_c)
    pkhash = None
    for line in res.splitlines():
        name, value = line.split('=')
        if name == "pkhash":
            pkhash = value
            break
    if not pkhash:
        raise Exception("Could not fetch public key hash from Controller")

    params = {"ssid": "unconfigured",
              "channel": "6",
              "dpp_controller": "ipaddr=127.0.0.1 pkhash=" + pkhash}
    hapd = hostapd.add_ap(apdev[0], params)
    check_dpp_capab(hapd)

    server = MyTCPServer(("127.0.0.1", 8908), DPPControllerServer)
    server.timeout = 30

    hapd.set("ext_mgmt_frame_handling", "1")
    dev[0].dpp_auth_init(uri=uri_c, role="enrollee")
    msg = hapd.mgmt_rx()
    if msg is None:
        raise Exception("MGMT RX wait timed out")
    dev[0].request("DPP_STOP_LISTEN")
    frame = msg['frame']
    for i in range(20):
        if i == 14:
            time.sleep(20)
        addr = struct.pack('6B', 0x02, 0, 0, 0, 0, i)
        tmp = frame[0:10] + addr + frame[16:]
        hapd.request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=" + binascii.hexlify(tmp).decode())
        ev = hapd.wait_event(["DPP-FAIL"], timeout=0.1)
        if ev:
            raise Exception("DPP relay failed [%d]: %s" % (i + 1, ev))

    server.server_close()

def test_dpp_tcp(dev, apdev, params):
    """DPP over TCP"""
    prefix = "dpp_tcp"
    cap_lo = os.path.join(params['logdir'], prefix + ".lo.pcap")
    try:
        run_dpp_tcp(dev[0], dev[1], cap_lo)
    finally:
        dev[1].request("DPP_CONTROLLER_STOP")

def test_dpp_tcp_port(dev, apdev, params):
    """DPP over TCP and specified port"""
    prefix = "dpp_tcp_port"
    cap_lo = os.path.join(params['logdir'], prefix + ".lo.pcap")
    try:
        run_dpp_tcp(dev[0], dev[1], cap_lo, port="23456")
    finally:
        dev[1].request("DPP_CONTROLLER_STOP")

def test_dpp_tcp_mutual(dev, apdev, params):
    """DPP over TCP (mutual)"""
    cap_lo = os.path.join(params['prefix'], ".lo.pcap")
    try:
        run_dpp_tcp(dev[0], dev[1], cap_lo, mutual=True)
    finally:
        dev[1].request("DPP_CONTROLLER_STOP")

def test_dpp_tcp_mutual_hostapd_conf(dev, apdev, params):
    """DPP over TCP (mutual, hostapd as Configurator)"""
    cap_lo = os.path.join(params['prefix'], ".lo.pcap")
    hapd = hostapd.add_ap(apdev[0], {"ssid": "unconfigured"})
    run_dpp_tcp(dev[0], hapd, cap_lo, mutual=True)

def run_dpp_tcp(dev0, dev1, cap_lo, port=None, mutual=False):
    check_dpp_capab(dev0)
    check_dpp_capab(dev1)

    wt = WlantestCapture('lo', cap_lo)
    time.sleep(1)

    # Controller
    conf_id = dev1.dpp_configurator_add()
    dev1.set("dpp_configurator_params",
             " conf=sta-dpp configurator=%d" % conf_id)
    id_c = dev1.dpp_bootstrap_gen()
    uri_c = dev1.request("DPP_BOOTSTRAP_GET_URI %d" % id_c)
    res = dev1.request("DPP_BOOTSTRAP_INFO %d" % id_c)
    pkhash = None
    for line in res.splitlines():
        name, value = line.split('=')
        if name == "pkhash":
            pkhash = value
            break
    if not pkhash:
        raise Exception("Could not fetch public key hash from Controller")
    req = "DPP_CONTROLLER_START"
    if port:
        req += " tcp_port=" + port
    if mutual:
        req += " qr=mutual"
        id0 = dev0.dpp_bootstrap_gen()
        uri0 = dev0.request("DPP_BOOTSTRAP_GET_URI %d" % id0)
        own = id0
    else:
        own = None
    if "OK" not in dev1.request(req):
        raise Exception("Failed to start Controller")

    # Initiate from Enrollee with broadcast DPP Authentication Request
    dev0.dpp_auth_init(uri=uri_c, own=own, role="enrollee",
                       tcp_addr="127.0.0.1", tcp_port=port)

    if mutual:
        ev = dev0.wait_event(["DPP-RESPONSE-PENDING"], timeout=5)
        if ev is None:
            raise Exception("Pending response not reported")
        ev = dev1.wait_event(["DPP-SCAN-PEER-QR-CODE"], timeout=5)
        if ev is None:
            raise Exception("QR Code scan for mutual authentication not requested")

        id1 = dev1.dpp_qr_code(uri0)

        ev = dev0.wait_event(["DPP-AUTH-DIRECTION"], timeout=5)
        if ev is None:
            raise Exception("DPP authentication direction not indicated (Initiator)")
        if "mutual=1" not in ev:
            raise Exception("Mutual authentication not used")

    wait_auth_success(dev1, dev0, configurator=dev1, enrollee=dev0,
                      allow_enrollee_failure=True,
                      allow_configurator_failure=True)
    time.sleep(0.5)
    wt.close()

def test_dpp_tcp_conf_init(dev, apdev, params):
    """DPP over TCP (Configurator initiates)"""
    cap_lo = os.path.join(params['prefix'], ".lo.pcap")
    try:
        run_dpp_tcp_conf_init(dev[0], dev[1], cap_lo)
    finally:
        dev[1].request("DPP_CONTROLLER_STOP")

def test_dpp_tcp_conf_init_hostapd_enrollee(dev, apdev, params):
    """DPP over TCP (Configurator initiates, hostapd as Enrollee)"""
    cap_lo = os.path.join(params['prefix'], ".lo.pcap")
    hapd = hostapd.add_ap(apdev[0], {"ssid": "unconfigured"})
    run_dpp_tcp_conf_init(dev[0], hapd, cap_lo, conf="ap-dpp")

def run_dpp_tcp_conf_init(dev0, dev1, cap_lo, port=None, conf="sta-dpp"):
    check_dpp_capab(dev0, min_ver=2)
    check_dpp_capab(dev1, min_ver=2)

    wt = WlantestCapture('lo', cap_lo)
    time.sleep(1)

    id_c = dev1.dpp_bootstrap_gen()
    uri_c = dev1.request("DPP_BOOTSTRAP_GET_URI %d" % id_c)
    res = dev1.request("DPP_BOOTSTRAP_INFO %d" % id_c)
    req = "DPP_CONTROLLER_START role=enrollee"
    if port:
        req += " tcp_port=" + port
    if "OK" not in dev1.request(req):
        raise Exception("Failed to start Controller")

    conf_id = dev0.dpp_configurator_add()
    dev0.dpp_auth_init(uri=uri_c, role="configurator", conf=conf,
                       configurator=conf_id,
                       tcp_addr="127.0.0.1", tcp_port=port)
    wait_auth_success(dev1, dev0, configurator=dev0, enrollee=dev1,
                      allow_enrollee_failure=True,
                      allow_configurator_failure=True)
    time.sleep(0.5)
    wt.close()

def test_dpp_tcp_controller_management_hostapd(dev, apdev, params):
    """DPP Controller management in hostapd"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": "unconfigured"})
    check_dpp_capab(hapd)
    conf_id = hapd.dpp_configurator_add()
    if "OK" not in hapd.request("DPP_CONTROLLER_START"):
        raise Exception("Failed to start Controller")
    if "FAIL" not in hapd.request("DPP_CONTROLLER_START"):
        raise Exception("DPP_CONTROLLER_START succeeded while already running Controller")
    hapd.request("DPP_CONTROLLER_STOP")
    hapd.dpp_configurator_remove(conf_id)
    if "FAIL" not in hapd.request("DPP_CONFIGURATOR_REMOVE %d" % conf_id):
        raise Exception("Removal of unknown Configurator accepted")

def test_dpp_tcp_controller_management_hostapd2(dev, apdev, params):
    """DPP Controller management in hostapd over interface addition/removal"""
    check_dpp_capab(dev[0], min_ver=2)
    hapd = hostapd.add_ap(apdev[0], {"ssid": "unconfigured"})
    check_dpp_capab(hapd, min_ver=2)
    hapd2 = hostapd.add_ap(apdev[1], {"ssid": "unconfigured"})
    check_dpp_capab(hapd2, min_ver=2)
    id_c = hapd.dpp_bootstrap_gen()
    uri_c = hapd.request("DPP_BOOTSTRAP_GET_URI %d" % id_c)
    if "OK" not in hapd.request("DPP_CONTROLLER_START role=enrollee"):
        raise Exception("Failed to start Controller")

    conf_id = dev[0].dpp_configurator_add()
    dev[0].dpp_auth_init(uri=uri_c, role="configurator", conf="sta-dpp",
                       configurator=conf_id, tcp_addr="127.0.0.1")
    ev = dev[0].wait_event(["DPP-AUTH-SUCCESS"], timeout=5)
    if ev is None:
        raise Exception("DPP Authentication did not succeed")
    ev = dev[0].wait_event(["DPP-CONF-SENT"], timeout=5)
    if ev is None:
        raise Exception("DPP Configuration did not succeed")

    hapd_global = hostapd.HostapdGlobal(apdev)
    hapd_global.remove(apdev[0]['ifname'])

    dev[0].dpp_auth_init(uri=uri_c, role="configurator", conf="sta-dpp",
                       configurator=conf_id, tcp_addr="127.0.0.1")
    ev = dev[0].wait_event(["DPP-AUTH-SUCCESS"], timeout=5)
    if ev is not None:
        raise Exception("Unexpected DPP Authentication success")

def test_dpp_tcp_controller_start_failure(dev, apdev, params):
    """DPP Controller startup failure"""
    check_dpp_capab(dev[0])

    try:
        if "OK" not in dev[0].request("DPP_CONTROLLER_START"):
            raise Exception("Could not start Controller")
        if "OK" in dev[0].request("DPP_CONTROLLER_START"):
            raise Exception("Second Controller start not rejected")
    finally:
        dev[0].request("DPP_CONTROLLER_STOP")

    tests = ["dpp_controller_start",
             "eloop_sock_table_add_sock;?eloop_register_sock;dpp_controller_start"]
    for func in tests:
        with alloc_fail(dev[0], 1, func):
            if "FAIL" not in dev[0].request("DPP_CONTROLLER_START"):
                raise Exception("Failure not reported during OOM")

def test_dpp_tcp_init_failure(dev, apdev, params):
    """DPP TCP init failure"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    id_c = dev[1].dpp_bootstrap_gen()
    uri_c = dev[1].request("DPP_BOOTSTRAP_GET_URI %d" % id_c)
    peer = dev[0].dpp_qr_code(uri_c)
    tests = ["dpp_tcp_init",
             "eloop_sock_table_add_sock;?eloop_register_sock;dpp_tcp_init",
             "dpp_tcp_encaps"]
    cmd = "DPP_AUTH_INIT peer=%d tcp_addr=127.0.0.1" % peer
    for func in tests:
        with alloc_fail(dev[0], 1, func):
            if "FAIL" not in dev[0].request(cmd):
                raise Exception("DPP_AUTH_INIT accepted during OOM")

def test_dpp_controller_rx_failure(dev, apdev, params):
    """DPP Controller RX failure"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    try:
        run_dpp_controller_rx_failure(dev, apdev)
    finally:
        dev[0].request("DPP_CONTROLLER_STOP")

def run_dpp_controller_rx_failure(dev, apdev):
    if "OK" not in dev[0].request("DPP_CONTROLLER_START"):
        raise Exception("Could not start Controller")
    id_c = dev[0].dpp_bootstrap_gen()
    uri_c = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id_c)
    peer = dev[1].dpp_qr_code(uri_c)
    tests = ["dpp_controller_tcp_cb",
             "eloop_sock_table_add_sock;?eloop_register_sock;dpp_controller_tcp_cb",
             "dpp_controller_rx",
             "dpp_controller_rx_auth_req",
             "wpabuf_alloc;=dpp_tcp_send_msg;dpp_controller_rx_auth_req"]
    cmd = "DPP_AUTH_INIT peer=%d tcp_addr=127.0.0.1" % peer
    for func in tests:
        with alloc_fail(dev[0], 1, func):
            if "OK" not in dev[1].request(cmd):
                raise Exception("Failed to initiate TCP connection")
            wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")

def test_dpp_controller_rx_errors(dev, apdev, params):
    """DPP Controller RX error cases"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    try:
        run_dpp_controller_rx_errors(dev, apdev)
    finally:
        dev[0].request("DPP_CONTROLLER_STOP")

def run_dpp_controller_rx_errors(dev, apdev):
    if "OK" not in dev[0].request("DPP_CONTROLLER_START"):
        raise Exception("Could not start Controller")

    addr = ("127.0.0.1", 8908)

    tests = [b"abc",
             b"abcd",
             b"\x00\x00\x00\x00",
             b"\x00\x00\x00\x01",
             b"\x00\x00\x00\x01\x09",
             b"\x00\x00\x00\x07\x09\x50\x6f\x9a\x1a\xff\xff",
             b"\x00\x00\x00\x07\x09\x50\x6f\x9a\x1a\x01\xff",
             b"\x00\x00\x00\x07\x09\x50\x6f\x9a\x1a\x01\x00",
             b"\x00\x00\x00\x08\x09\x50\x6f\x9a\x1a\x01\x00\xff",
             b"\x00\x00\x00\x01\x0a",
             b"\x00\x00\x00\x04\x0a\xff\xff\xff",
             b"\x00\x00\x00\x01\x0b",
             b"\x00\x00\x00\x08\x0b\xff\xff\xff\xff\xff\xff\xff",
             b"\x00\x00\x00\x08\x0b\xff\x00\x00\xff\xff\xff\xff",
             b"\x00\x00\x00\x08\x0b\xff\x00\x00\xff\xff\x6c\x00",
             b"\x00\x00\x00\x0a\x0b\xff\x00\x00\xff\xff\x6c\x02\xff\xff",
             b"\x00\x00\x00\x10\x0b\xff\x00\x00\xff\xff\x6c\x08\xff\xdd\x05\x50\x6f\x9a\x1a\x01",
             b"\x00\x00\x00\x12\x0b\xff\x00\x00\xff\xff\x6c\x08\xff\xdd\x05\x50\x6f\x9a\x1a\x01\x00\x00",
             b"\x00\x00\x00\x01\xff",
             b"\x00\x00\x00\x01\xff\xee"]
    #define WLAN_PA_GAS_INITIAL_REQ 10
    #define WLAN_PA_GAS_INITIAL_RESP 11

    for t in tests:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM,
                             socket.IPPROTO_TCP)
        sock.settimeout(0.1)
        sock.connect(addr)
        sock.send(t)
        sock.shutdown(1)
        try:
            sock.recv(10)
        except socket.timeout:
            pass
        sock.close()

def test_dpp_conn_status_success(dev, apdev):
    """DPP connection status - success"""
    try:
        run_dpp_conn_status(dev, apdev)
    finally:
        dev[0].set("dpp_config_processing", "0", allow_fail=True)

def test_dpp_conn_status_wrong_passphrase(dev, apdev):
    """DPP connection status - wrong passphrase"""
    try:
        run_dpp_conn_status(dev, apdev, result=2)
    finally:
        dev[0].set("dpp_config_processing", "0", allow_fail=True)

def test_dpp_conn_status_no_ap(dev, apdev):
    """DPP connection status - no AP"""
    try:
        run_dpp_conn_status(dev, apdev, result=10)
    finally:
        dev[0].set("dpp_config_processing", "0", allow_fail=True)

def test_dpp_conn_status_connector_mismatch(dev, apdev):
    """DPP connection status - invalid Connector"""
    try:
        run_dpp_conn_status(dev, apdev, result=8)
    finally:
        dev[0].set("dpp_config_processing", "0", allow_fail=True)

def test_dpp_conn_status_assoc_reject(dev, apdev):
    """DPP connection status - association rejection"""
    try:
        dev[0].request("TEST_ASSOC_IE 30020000")
        run_dpp_conn_status(dev, apdev, assoc_reject=True)
    finally:
        dev[0].set("dpp_config_processing", "0", allow_fail=True)

def run_dpp_conn_status(dev, apdev, result=0, assoc_reject=False):
    check_dpp_capab(dev[0], min_ver=2)
    check_dpp_capab(dev[1], min_ver=2)

    if result != 10:
        if result == 7 or result == 8:
            params = {"ssid": "dpp-status",
                      "wpa": "2",
                      "wpa_key_mgmt": "DPP",
                      "ieee80211w": "2",
                      "rsn_pairwise": "CCMP",
                      "dpp_connector": params1_ap_connector,
                      "dpp_csign": params1_csign,
                      "dpp_netaccesskey": params1_ap_netaccesskey}
        else:
            if result == 2:
                passphrase = "wrong passphrase"
            else:
                passphrase = "secret passphrase"
            params = hostapd.wpa2_params(ssid="dpp-status",
                                         passphrase=passphrase)
        try:
            hapd = hostapd.add_ap(apdev[0], params)
        except:
            raise HwsimSkip("DPP not supported")

    dev[0].request("SET sae_groups ")
    dev[0].set("dpp_config_processing", "2")
    id0 = dev[0].dpp_bootstrap_gen(chan="81/1", mac=True)
    uri0 = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id0)

    dev[0].dpp_listen(2412)
    if result == 7 or result == 8:
        conf = 'sta-dpp'
        passphrase = None
        configurator = dev[1].dpp_configurator_add()
    else:
        conf = 'sta-psk'
        passphrase = "secret passphrase"
        configurator = None
    dev[1].dpp_auth_init(uri=uri0, conf=conf, ssid="dpp-status",
                         passphrase=passphrase, configurator=configurator,
                         conn_status=True)
    res = wait_auth_success(dev[0], dev[1], configurator=dev[1],
                            enrollee=dev[0])
    if 'wait_conn_status' not in res:
        raise Exception("Configurator did not request connection status")

    if assoc_reject and result == 0:
        result = 2
    ev = dev[1].wait_event(["DPP-CONN-STATUS-RESULT"], timeout=20)
    if ev is None:
        raise Exception("No connection status reported")
    if "timeout" in ev:
        raise Exception("Connection status result timeout")
    if "result=%d" % result not in ev:
        raise Exception("Unexpected connection status result: " + ev)
    if "ssid=dpp-status" not in ev:
        raise Exception("SSID not reported")

    if result == 0:
        dev[0].wait_connected()
    if result == 10 and "channel_list=" not in ev:
        raise Exception("Channel list not reported for no-AP")

def test_dpp_conn_status_success_hostapd_configurator(dev, apdev):
    """DPP connection status - success with hostapd as Configurator"""
    try:
        run_dpp_conn_status_hostapd_configurator(dev, apdev)
    finally:
        dev[0].set("dpp_config_processing", "0", allow_fail=True)

def run_dpp_conn_status_hostapd_configurator(dev, apdev):
    check_dpp_capab(dev[0])
    hapd = hostapd.add_ap(apdev[0], {"ssid": "unconfigured",
                                     "channel": "1"})
    check_dpp_capab(hapd)
    conf_id = hapd.dpp_configurator_add()

    cmd = "DPP_CONFIGURATOR_SIGN conf=ap-dpp configurator=%d" % conf_id
    res = hapd.request(cmd)
    if "FAIL" in res:
        raise Exception("Failed to generate own configuration")
    update_hapd_config(hapd)

    id0 = dev[0].dpp_bootstrap_gen(chan="81/1", mac=True)
    uri0 = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id0)
    id1 = hapd.dpp_qr_code(uri0)
    res = hapd.request("DPP_BOOTSTRAP_INFO %d" % id1)
    if "FAIL" in res:
        raise Exception("DPP_BOOTSTRAP_INFO failed")
    if "type=QRCODE" not in res:
        raise Exception("DPP_BOOTSTRAP_INFO did not report correct type")
    if "mac_addr=" + dev[0].own_addr() not in res:
        raise Exception("DPP_BOOTSTRAP_INFO did not report correct mac_addr")
    dev[0].set("dpp_config_processing", "2")
    dev[0].dpp_listen(2412)
    hapd.dpp_auth_init(peer=id1, configurator=conf_id, conf="sta-dpp",
                       conn_status=True)
    res = wait_auth_success(dev[0], hapd, configurator=hapd, enrollee=dev[0])
    if 'wait_conn_status' not in res:
        raise Exception("Configurator did not request connection status")
    ev = hapd.wait_event(["DPP-CONN-STATUS-RESULT"], timeout=20)
    if ev is None:
        raise Exception("No connection status reported")
    if "result=0" not in ev:
        raise Exception("Unexpected connection status: " + ev)

def test_dpp_mud_url(dev, apdev):
    """DPP MUD URL"""
    check_dpp_capab(dev[0])
    try:
        dev[0].set("dpp_name", "Test Enrollee")
        dev[0].set("dpp_mud_url", "https://example.com/mud")
        run_dpp_qr_code_auth_unicast(dev, apdev, None)
    finally:
        dev[0].set("dpp_mud_url", "")
        dev[0].set("dpp_name", "Test")

def test_dpp_mud_url_hostapd(dev, apdev):
    """DPP MUD URL from hostapd"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    params = {"ssid": "unconfigured",
              "dpp_name": "AP Enrollee",
              "dpp_mud_url": "https://example.com/mud"}
    hapd = hostapd.add_ap(apdev[0], params)
    check_dpp_capab(hapd)

    id_h = hapd.dpp_bootstrap_gen(chan="81/1", mac=True)
    uri = hapd.request("DPP_BOOTSTRAP_GET_URI %d" % id_h)

    conf_id = dev[0].dpp_configurator_add()
    dev[0].dpp_auth_init(uri=uri, conf="ap-dpp", configurator=conf_id)
    wait_auth_success(hapd, dev[0], configurator=dev[0], enrollee=hapd)
    update_hapd_config(hapd)

def test_dpp_config_save(dev, apdev, params):
    """DPP configuration saving"""
    config = os.path.join(params['logdir'], 'dpp_config_save.conf')
    run_dpp_config_save(dev, apdev, config, "test", '"test"')

def test_dpp_config_save2(dev, apdev, params):
    """DPP configuration saving (2)"""
    config = os.path.join(params['logdir'], 'dpp_config_save2.conf')
    run_dpp_config_save(dev, apdev, config, "\\u0001*", '012a')

def test_dpp_config_save3(dev, apdev, params):
    """DPP configuration saving (3)"""
    config = os.path.join(params['logdir'], 'dpp_config_save3.conf')
    run_dpp_config_save(dev, apdev, config, "\\u0001*\\u00c2\\u00bc\\u00c3\\u009e\\u00c3\\u00bf", '012ac2bcc39ec3bf')

def run_dpp_config_save(dev, apdev, config, conf_ssid, exp_ssid):
    check_dpp_capab(dev[1])
    with open(config, "w") as f:
        f.write("update_config=1\n" +
                "dpp_config_processing=1\n")
    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    wpas.interface_add("wlan5", config=config)
    check_dpp_capab(wpas)
    conf = '{"wi-fi_tech":"infra", "discovery":{"ssid":"' + conf_ssid + '"},"cred":{"akm":"psk","pass":"secret passphrase"}}'
    dev[1].set("dpp_config_obj_override", conf)
    dpp_dev = [wpas, dev[1]]
    run_dpp_qr_code_auth_unicast(dpp_dev, apdev, "prime256v1",
                                 require_conf_success=True)
    if "OK" not in wpas.request("SAVE_CONFIG"):
        raise Exception("Failed to save configuration file")
    with open(config, "r") as f:
        data = f.read()
        logger.info("Saved configuration:\n" + data)
        if 'ssid=' + exp_ssid + '\n' not in data:
            raise Exception("SSID not saved")
        if 'psk="secret passphrase"' not in data:
            raise Exception("Passphtase not saved")

def test_dpp_nfc_uri(dev, apdev):
    """DPP bootstrapping via NFC URI record"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])

    id = dev[0].dpp_bootstrap_gen(type="nfc-uri", chan="81/1", mac=True)
    uri = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id)
    logger.info("Generated URI: " + uri)
    info = dev[0].request("DPP_BOOTSTRAP_INFO %d" % id)
    logger.info("Bootstrapping info:\n" + info)
    if "type=NFC-URI" not in info:
        raise Exception("Unexpected bootstrapping info contents")

    dev[0].dpp_listen(2412)
    conf_id = dev[1].dpp_configurator_add()
    dev[1].dpp_auth_init(nfc_uri=uri, configurator=conf_id, conf="sta-dpp")
    wait_auth_success(dev[0], dev[1], configurator=dev[1], enrollee=dev[0])

def test_dpp_nfc_uri_hostapd(dev, apdev):
    """DPP bootstrapping via NFC URI record (hostapd)"""
    check_dpp_capab(dev[0])

    hapd = hostapd.add_ap(apdev[0], {"ssid": "unconfigured"})
    check_dpp_capab(hapd)

    id = hapd.dpp_bootstrap_gen(type="nfc-uri", chan="81/1", mac=True)
    uri = hapd.request("DPP_BOOTSTRAP_GET_URI %d" % id)
    logger.info("Generated URI: " + uri)
    info = hapd.request("DPP_BOOTSTRAP_INFO %d" % id)
    logger.info("Bootstrapping info:\n" + info)
    if "type=NFC-URI" not in info:
        raise Exception("Unexpected bootstrapping info contents")

    hapd.dpp_listen(2412)
    conf_id = dev[0].dpp_configurator_add()
    dev[0].dpp_auth_init(nfc_uri=uri, configurator=conf_id, conf="ap-dpp")
    wait_auth_success(hapd, dev[0], configurator=dev[0], enrollee=hapd)

def test_dpp_nfc_uri_hostapd_tag_read(dev, apdev):
    """DPP bootstrapping via NFC URI record (hostapd reading tag)"""
    check_dpp_capab(dev[0])

    hapd = hostapd.add_ap(apdev[0], {"ssid": "unconfigured"})
    check_dpp_capab(hapd)

    id = dev[0].dpp_bootstrap_gen(type="nfc-uri", chan="81/1", mac=True)
    uri = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id)
    info = dev[0].request("DPP_BOOTSTRAP_INFO %d" % id)
    conf_id = dev[0].dpp_configurator_add()
    dev[0].set("dpp_configurator_params",
               "conf=ap-dpp configurator=%d" % conf_id)
    dev[0].dpp_listen(2412)

    hapd.dpp_auth_init(nfc_uri=uri, role="enrollee")
    wait_auth_success(dev[0], hapd, configurator=dev[0], enrollee=hapd)

def test_dpp_nfc_negotiated_handover(dev, apdev):
    """DPP bootstrapping via NFC negotiated handover"""
    run_dpp_nfc_negotiated_handover(dev)

def test_dpp_nfc_negotiated_handover_diff_curve(dev, apdev):
    """DPP bootstrapping via NFC negotiated handover (different curve)"""
    run_dpp_nfc_negotiated_handover(dev, curve0="prime256v1",
                                    curve1="secp384r1")

def test_dpp_nfc_negotiated_handover_hostapd_sel(dev, apdev):
    """DPP bootstrapping via NFC negotiated handover (hostapd as selector)"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": "unconfigured",
                                     "channel": "6"})
    check_dpp_capab(hapd)
    run_dpp_nfc_negotiated_handover([dev[0], hapd], conf="ap-dpp")

def test_dpp_nfc_negotiated_handover_hostapd_req(dev, apdev):
    """DPP bootstrapping via NFC negotiated handover (hostapd as requestor)"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": "unconfigured",
                                     "channel": "6"})
    check_dpp_capab(hapd)
    run_dpp_nfc_negotiated_handover([hapd, dev[0]])

def run_dpp_nfc_negotiated_handover(dev, curve0=None, curve1=None,
                                    conf="sta-dpp"):
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])

    id0 = dev[0].dpp_bootstrap_gen(type="nfc-uri", chan="81/6,11", mac=True,
                                   curve=curve0)
    uri0 = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id0)
    logger.info("Generated URI[0]: " + uri0)
    id1 = dev[1].dpp_bootstrap_gen(type="nfc-uri", chan="81/1,6,11", mac=True,
                                   curve=curve1)
    uri1 = dev[1].request("DPP_BOOTSTRAP_GET_URI %d" % id1)
    logger.info("Generated URI[1]: " + uri1)

    # dev[0] acting as NFC Handover Requestor
    # dev[1] acting as NFC Handover Selector
    res = dev[1].request("DPP_NFC_HANDOVER_REQ own=%d uri=%s" % (id1, uri0))
    if "FAIL" in res:
        raise Exception("Failed to process NFC Handover Request")
    info = dev[1].request("DPP_BOOTSTRAP_INFO %d" % id1)
    logger.info("Updated local bootstrapping info:\n" + info)
    freq = None
    for line in info.splitlines():
        if line.startswith("use_freq="):
            freq = int(line.split('=')[1])
    if freq is None:
        raise Exception("Selected channel not indicated")
    uri1 = dev[1].request("DPP_BOOTSTRAP_GET_URI %d" % id1)
    logger.info("Updated URI[1]: " + uri1)
    dev[1].dpp_listen(freq)
    res = dev[0].request("DPP_NFC_HANDOVER_SEL own=%d uri=%s" % (id0, uri1))
    if "FAIL" in res:
        raise Exception("Failed to process NFC Handover Select")
    peer = int(res)

    conf_id = dev[0].dpp_configurator_add()
    dev[0].dpp_auth_init(peer=peer, own=id0, configurator=conf_id,
                         conf=conf)
    wait_auth_success(dev[1], dev[0], configurator=dev[0], enrollee=dev[1])

def test_dpp_nfc_errors_hostapd(dev, apdev):
    """DPP NFC operation failures in hostapd"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])

    id0 = dev[0].dpp_bootstrap_gen(type="nfc-uri", chan="81/11", mac=True,
                                   curve="secp384r1")
    uri0 = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id0)

    hapd = hostapd.add_ap(apdev[0], {"ssid": "unconfigured",
                                     "channel": "6"})
    check_dpp_capab(hapd)

    id_h = hapd.dpp_bootstrap_gen(type="nfc-uri", chan="81/6", mac=True)
    uri_h = hapd.request("DPP_BOOTSTRAP_GET_URI %d" % id_h)

    tests = ["",
             "own=123456789",
             "own=%d" % id_h,
             "own=%d uri=%s" % (id_h, "foo")]
    for t in tests:
        if "FAIL" not in hapd.request("DPP_NFC_HANDOVER_REQ " + t):
            raise Exception("Invalid DPP_NFC_HANDOVER_REQ accepted")
        if "FAIL" not in hapd.request("DPP_NFC_HANDOVER_SEL " + t):
            raise Exception("Invalid DPP_NFC_HANDOVER_SEL accepted")

    # DPP: Peer (NFC Handover Selector) used different curve
    if "FAIL" not in hapd.request("DPP_NFC_HANDOVER_SEL own=%d uri=%s" % (id_h, uri0)):
        raise Exception("Invalid DPP_NFC_HANDOVER_SEL accepted")

    # DPP: No common channel found
    if "FAIL" not in hapd.request("DPP_NFC_HANDOVER_REQ own=%d uri=%s" % (id_h, uri0)):
        raise Exception("DPP_NFC_HANDOVER_REQ with local error accepted")

def test_dpp_with_p2p_device(dev, apdev):
    """DPP exchange when driver uses a separate P2P Device interface"""
    check_dpp_capab(dev[0])
    with HWSimRadio(use_p2p_device=True) as (radio, iface):
        wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
        wpas.interface_add(iface)
        check_dpp_capab(wpas)
        id1 = wpas.dpp_bootstrap_gen(chan="81/1", mac=True)
        uri1 = wpas.request("DPP_BOOTSTRAP_GET_URI %d" % id1)
        wpas.dpp_listen(2412)
        time.sleep(7)
        dev[0].dpp_auth_init(uri=uri1)
        wait_auth_success(wpas, dev[0], configurator=dev[0], enrollee=wpas,
                          allow_enrollee_failure=True)

@long_duration_test
def test_dpp_chirp(dev, apdev):
    """DPP chirp"""
    check_dpp_capab(dev[0])
    dev[0].flush_scan_cache()

    params = {"ssid": "dpp",
              "channel": "11"}
    hapd = hostapd.add_ap(apdev[0], params)
    check_dpp_capab(hapd)
    dpp_cc = False

    id1 = dev[0].dpp_bootstrap_gen(chan="81/1")
    if "OK" not in dev[0].request("DPP_CHIRP own=%d iter=5" % id1):
        raise Exception("DPP_CHIRP failed")
    chan1 = 0
    chan6 = 0
    chan11 = 0
    for i in range(30):
        ev = dev[0].wait_event(["DPP-CHIRP-STOPPED",
                                "DPP-TX "], timeout=60)
        if ev is None:
            raise Exception("DPP chirp stop not reported")
        if "DPP-CHIRP-STOPPED" in ev:
            break
        if "type=13" not in ev:
            continue
        freq = int(ev.split(' ')[2].split('=')[1])
        if freq == 2412:
            chan1 += 1
        elif freq == 2437:
            chan6 += 1
        elif freq == 2462:
            chan11 += 1
        if not dpp_cc:
            hapd.set("dpp_configurator_connectivity", "1")
            if "OK" not in hapd.request("UPDATE_BEACON"):
                raise Exception("UPDATE_BEACON failed")
            dpp_cc = True
    if chan1 != 5 or chan6 != 5 or chan11 != 1:
        raise Exception("Unexpected number of presence announcements sent: %d %d %d" % (chan1, chan6, chan11))
    ev = hapd.wait_event(["DPP-CHIRP-RX"], timeout=1)
    if ev is None:
        raise Exception("No chirp received on the AP")
    if "freq=2462" not in ev:
        raise Exception("Chirp reception reported on unexpected channel: " + ev)
    if "src=" + dev[0].own_addr() not in ev:
        raise Exception("Unexpected chirp source reported: " + ev)

@long_duration_test
def test_dpp_chirp_listen(dev, apdev):
    """DPP chirp with listen"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])

    id1 = dev[0].dpp_bootstrap_gen(chan="81/1", mac=True)
    uri = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id1)

    if "OK" not in dev[0].request("DPP_CHIRP own=%d iter=2 listen=2412" % id1):
        raise Exception("DPP_CHIRP failed")
    for i in range(30):
        ev = dev[0].wait_event(["DPP-CHIRP-STOPPED",
                                "DPP-TX "], timeout=60)
        if ev is None:
            raise Exception("DPP chirp stop not reported")
        if "DPP-CHIRP-STOPPED" in ev:
            break

def test_dpp_chirp_configurator(dev, apdev):
    """DPP chirp with a standalone Configurator"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])

    id1 = dev[0].dpp_bootstrap_gen(chan="81/1")
    uri = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id1)

    conf_id = dev[1].dpp_configurator_add()
    idc = dev[1].dpp_qr_code(uri)
    dev[1].dpp_bootstrap_set(idc, conf="sta-dpp", configurator=conf_id)
    dev[1].dpp_listen(2437)

    if "OK" not in dev[0].request("DPP_CHIRP own=%d iter=2" % id1):
        raise Exception("DPP_CHIRP failed")

    ev = dev[1].wait_event(["DPP-RX"], timeout=10)
    if ev is None:
        raise Exception("Presence Announcement not seen")
    if "type=13" not in ev:
        raise Exception("Unexpected DPP frame received: " + ev)

    ev = dev[1].wait_event(["DPP-TX"], timeout=10)
    if ev is None:
        raise Exception("Authentication Request TX not seen")
    if "type=0" not in ev:
        raise Exception("Unexpected DPP frame TX: " + ev)
    if "dst=" + dev[0].own_addr() not in ev:
        raise Exception("Unexpected Authentication Request destination: " + ev)

    wait_auth_success(dev[0], dev[1], dev[1], dev[0])

def test_dpp_chirp_ap_as_configurator(dev, apdev):
    """DPP chirp with an AP as a standalone Configurator"""
    check_dpp_capab(dev[0], min_ver=2)

    hapd = hostapd.add_ap(apdev[0], {"ssid": "unconfigured"})
    check_dpp_capab(hapd, min_ver=2)

    id1 = dev[0].dpp_bootstrap_gen(chan="81/1")
    uri = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id1)

    conf_id = hapd.dpp_configurator_add()
    idc = hapd.dpp_qr_code(uri)
    hapd.dpp_bootstrap_set(idc, conf="sta-dpp", configurator=conf_id)
    hapd.dpp_listen(2412)

    if "OK" not in dev[0].request("DPP_CHIRP own=%d iter=2" % id1):
        raise Exception("DPP_CHIRP failed")

    wait_auth_success(dev[0], hapd, hapd, dev[0])

def test_dpp_chirp_configurator_inits(dev, apdev):
    """DPP chirp with a standalone Configurator initiating"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])

    id1 = dev[0].dpp_bootstrap_gen(chan="81/1", mac=True)
    uri = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id1)

    conf_id = dev[1].dpp_configurator_add()
    idc = dev[1].dpp_qr_code(uri)

    if "OK" not in dev[0].request("DPP_CHIRP own=%d iter=2 listen=2412" % id1):
        raise Exception("DPP_CHIRP failed")
    for i in range(2):
        ev = dev[0].wait_event(["DPP-TX "], timeout=10)
        if ev is None or "type=13" not in ev:
            raise Exception("Presence Announcement not sent")

    dev[1].dpp_auth_init(uri=uri, conf="sta-dpp", configurator=conf_id)
    wait_auth_success(dev[0], dev[1], dev[1], dev[0])

def test_dpp_chirp_ap(dev, apdev):
    """DPP chirp by an AP"""
    check_dpp_capab(dev[0], min_ver=2)

    hapd = hostapd.add_ap(apdev[0], {"ssid": "unconfigured",
                                     "start_disabled": "1"})
    check_dpp_capab(hapd, min_ver=2)

    id_h = hapd.dpp_bootstrap_gen(chan="81/1", mac=True)
    uri = hapd.request("DPP_BOOTSTRAP_GET_URI %d" % id_h)

    conf_id = dev[0].dpp_configurator_add()
    idc = dev[0].dpp_qr_code(uri)
    dev[0].dpp_bootstrap_set(idc, conf="ap-dpp", configurator=conf_id)
    dev[0].dpp_listen(2437)
    if "OK" not in hapd.request("DPP_CHIRP own=%d iter=5" % id_h):
        raise Exception("DPP_CHIRP failed")
    wait_auth_success(hapd, dev[0], configurator=dev[0], enrollee=hapd,
                      timeout=20)
    update_hapd_config(hapd)

@long_duration_test
def test_dpp_chirp_ap_5g(dev, apdev):
    """DPP chirp by an AP on 5 GHz"""
    check_dpp_capab(dev[0], min_ver=2)

    try:
        hapd = None
        hapd2 = None

        params = {"ssid": "unconfigured",
                  "country_code": "US",
                  "hw_mode": "a",
                  "channel": "40",
                  "dpp_configurator_connectivity": "1"}
        hapd2 = hostapd.add_ap(apdev[1], params)
        check_dpp_capab(hapd2, min_ver=2)

        params = {"ssid": "unconfigured",
                  "country_code": "US",
                  "hw_mode": "a",
                  "channel": "36",
                  "start_disabled": "1"}
        hapd = hostapd.add_ap(apdev[0], params)
        check_dpp_capab(hapd, min_ver=2)

        id_h = hapd.dpp_bootstrap_gen(chan="81/1", mac=True)
        uri = hapd.request("DPP_BOOTSTRAP_GET_URI %d" % id_h)

        # First, check chirping iteration and timeout
        if "OK" not in hapd.request("DPP_CHIRP own=%d iter=2" % id_h):
            raise Exception("DPP_CHIRP failed")
        chan1 = 0
        chan6 = 0
        chan40 = 0
        chan149 = 0
        for i in range(30):
            ev = hapd.wait_event(["DPP-CHIRP-STOPPED", "DPP-TX "], timeout=60)
            if ev is None:
                raise Exception("DPP chirp stop not reported")
            if "DPP-CHIRP-STOPPED" in ev:
                break
            if "type=13" not in ev:
                continue
            freq = int(ev.split(' ')[2].split('=')[1])
            if freq == 2412:
                chan1 += 1
            elif freq == 2437:
                chan6 += 1
            elif freq == 5200:
                chan40 += 1
            elif freq == 5745:
                chan149 += 1
        if not chan1 or not chan6 or not chan40 or not chan149:
            raise Exception("Chirp not sent on all channels")

        # Then, check successful chirping
        conf_id = dev[0].dpp_configurator_add()
        idc = dev[0].dpp_qr_code(uri)
        dev[0].dpp_bootstrap_set(idc, conf="ap-dpp", configurator=conf_id)
        dev[0].dpp_listen(5200)
        if "OK" not in hapd.request("DPP_CHIRP own=%d iter=5" % id_h):
            raise Exception("DPP_CHIRP failed")
        wait_auth_success(hapd, dev[0], configurator=dev[0], enrollee=hapd,
                          timeout=20)
        update_hapd_config(hapd)
    finally:
        clear_regdom(hapd, dev)

def test_dpp_chirp_ap_errors(dev, apdev):
    """DPP chirp errors in hostapd"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": "unconfigured",
                                     "start_disabled": "1"})
    check_dpp_capab(hapd, min_ver=2)

    id_h = hapd.dpp_bootstrap_gen(chan="81/1", mac=True)
    uri = hapd.request("DPP_BOOTSTRAP_GET_URI %d" % id_h)
    tests = ["",
             "own=%d" % (id_h + 1),
             "own=%d iter=-1" % id_h,
             "own=%d listen=0" % id_h]
    for t in tests:
        if "FAIL" not in hapd.request("DPP_CHIRP " + t):
            raise Exception("Invalid DPP_CHIRP accepted: " + t)
    if "OK" not in hapd.request("DPP_CHIRP own=%d iter=5" % id_h):
        raise Exception("DPP_CHIRP failed")

    hapd.request("DPP_STOP_CHIRP")

def start_dpp_pfs_ap(apdev, pfs, sae=False):
    params = {"ssid": "dpp",
              "wpa": "2",
              "wpa_key_mgmt": "DPP",
              "dpp_pfs": str(pfs),
              "ieee80211w": "2",
              "rsn_pairwise": "CCMP",
              "dpp_connector": params1_ap_connector,
              "dpp_csign": params1_csign,
              "dpp_netaccesskey": params1_ap_netaccesskey}
    if sae:
        params["wpa_key_mgmt"] = "DPP SAE"
        params["sae_password"] = "sae-password"
    try:
        hapd = hostapd.add_ap(apdev, params)
    except:
        raise HwsimSkip("DPP not supported")
    return hapd

def run_dpp_pfs_sta(dev, pfs, fail=False, pfs_expected=None, sae=False):
    key_mgmt = "DPP SAE" if sae else "DPP"
    psk = "sae-password" if sae else None
    dev.connect("dpp", key_mgmt=key_mgmt, scan_freq="2412",
                ieee80211w="2", dpp_pfs=str(pfs),
                dpp_csign=params1_csign,
                dpp_connector=params1_sta_connector,
                dpp_netaccesskey=params1_sta_netaccesskey,
                psk=psk,
                wait_connect=not fail)
    if fail:
        for i in range(2):
            ev = dev.wait_event(["CTRL-EVENT-ASSOC-REJECT",
                                 "CTRL-EVENT-CONNECTED"], timeout=10)
            if ev is None:
                raise Exception("Connection result not reported")
            if "CTRL-EVENT-CONNECTED" in ev:
                raise Exception("Unexpected connection")
        dev.request("REMOVE_NETWORK all")
    else:
        if pfs_expected is not None:
            res = dev.get_status_field("dpp_pfs")
            pfs_used = res == "1"
            if pfs_expected != pfs_used:
                raise Exception("Unexpected PFS negotiation result")
        dev.request("REMOVE_NETWORK all")
        dev.wait_disconnected()
    dev.dump_monitor()

def test_dpp_pfs_ap_0(dev, apdev):
    """DPP PFS AP default"""
    check_dpp_capab(dev[0])
    hapd = start_dpp_pfs_ap(apdev[0], 0)
    run_dpp_pfs_sta(dev[0], 0, pfs_expected=True)
    run_dpp_pfs_sta(dev[0], 1, pfs_expected=True)
    run_dpp_pfs_sta(dev[0], 2, pfs_expected=False)

def test_dpp_pfs_ap_1(dev, apdev):
    """DPP PFS AP required"""
    check_dpp_capab(dev[0])
    hapd = start_dpp_pfs_ap(apdev[0], 1)
    run_dpp_pfs_sta(dev[0], 0, pfs_expected=True)
    run_dpp_pfs_sta(dev[0], 1, pfs_expected=True)
    run_dpp_pfs_sta(dev[0], 2, fail=True)

def test_dpp_pfs_ap_2(dev, apdev):
    """DPP PFS AP not allowed"""
    check_dpp_capab(dev[0])
    hapd = start_dpp_pfs_ap(apdev[0], 2)
    run_dpp_pfs_sta(dev[0], 0, pfs_expected=False)
    run_dpp_pfs_sta(dev[0], 1, fail=True)
    run_dpp_pfs_sta(dev[0], 2, pfs_expected=False)

def test_dpp_pfs_connect_cmd(dev, apdev):
    """DPP PFS and cfg80211 connect command"""
    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    wpas.interface_add("wlan5", drv_params="force_connect_cmd=1")
    check_dpp_capab(wpas)
    hapd = start_dpp_pfs_ap(apdev[0], 0)
    run_dpp_pfs_sta(wpas, 0, pfs_expected=True)
    run_dpp_pfs_sta(wpas, 1, pfs_expected=True)
    run_dpp_pfs_sta(wpas, 2, pfs_expected=False)

def test_dpp_pfs_connect_cmd_ap_2(dev, apdev):
    """DPP PFS and cfg80211 connect command (PFS not allowed by AP)"""
    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    wpas.interface_add("wlan5", drv_params="force_connect_cmd=1")
    check_dpp_capab(wpas)
    hapd = start_dpp_pfs_ap(apdev[0], 2)
    run_dpp_pfs_sta(wpas, 0, pfs_expected=False)
    run_dpp_pfs_sta(wpas, 1, fail=True)
    run_dpp_pfs_sta(wpas, 2, pfs_expected=False)

def test_dpp_pfs_connect_cmd_ap_2_sae(dev, apdev):
    """DPP PFS and cfg80211 connect command (PFS not allowed by AP; SAE enabled)"""
    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    wpas.interface_add("wlan5", drv_params="force_connect_cmd=1")
    check_dpp_capab(wpas)
    if "SAE" not in wpas.get_capability("auth_alg"):
        raise HwsimSkip("SAE not supported")
    hapd = start_dpp_pfs_ap(apdev[0], 2, sae=True)
    run_dpp_pfs_sta(wpas, 0, pfs_expected=False, sae=True)
    run_dpp_pfs_sta(wpas, 1, fail=True, sae=True)
    run_dpp_pfs_sta(wpas, 2, pfs_expected=False, sae=True)

def test_dpp_pfs_ap_0_sta_ver1(dev, apdev):
    """DPP PFS AP default with version 1 STA"""
    check_dpp_capab(dev[0])
    dev[0].set("dpp_version_override", "1")
    hapd = start_dpp_pfs_ap(apdev[0], 0)
    run_dpp_pfs_sta(dev[0], 0, pfs_expected=False)

def test_dpp_pfs_errors(dev, apdev):
    """DPP PFS error cases"""
    check_dpp_capab(dev[0], min_ver=2)
    hapd = start_dpp_pfs_ap(apdev[0], 1)
    tests = [(1, "dpp_pfs_init"),
             (1, "crypto_ecdh_init;dpp_pfs_init"),
             (1, "wpabuf_alloc;dpp_pfs_init")]
    for count, func in tests:
        with alloc_fail(dev[0], count, func):
            dev[0].connect("dpp", key_mgmt="DPP", scan_freq="2412",
                           ieee80211w="2", dpp_pfs="1",
                           dpp_csign=params1_csign,
                           dpp_connector=params1_sta_connector,
                           dpp_netaccesskey=params1_sta_netaccesskey)
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()
            dev[0].dump_monitor()
            hapd.dump_monitor()

def test_dpp_reconfig_connector(dev, apdev):
    """DPP reconfiguration connector"""
    try:
        run_dpp_reconfig_connector(dev, apdev)
    finally:
        dev[0].set("dpp_config_processing", "0", allow_fail=True)

def test_dpp_reconfig_connector_different_groups(dev, apdev):
    """DPP reconfiguration connector with different groups"""
    try:
        run_dpp_reconfig_connector(dev, apdev, conf_curve="secp384r1")
    finally:
        dev[0].set("dpp_config_processing", "0", allow_fail=True)

@long_duration_test
def test_dpp_reconfig_retries(dev, apdev):
    """DPP reconfiguration retries"""
    try:
        run_dpp_reconfig_connector(dev, apdev, test_retries=True)
        for i in range(4):
            ev = dev[0].wait_event(["DPP-TX "], timeout=120)
            if ev is None or "type=14" not in ev:
                raise Exception("Reconfig Announcement not sent")
        dev[0].request("DPP_STOP_LISTEN")
    finally:
        dev[0].set("dpp_config_processing", "0", allow_fail=True)

def run_dpp_reconfig_connector(dev, apdev, conf_curve=None,
                               test_retries=False):
    check_dpp_capab(dev[0], min_ver=2)
    check_dpp_capab(dev[1], min_ver=2)

    ssid = "reconfig"
    passphrase = "secret passphrase"
    passphrase2 = "another secret passphrase"
    params = hostapd.wpa2_params(ssid=ssid, passphrase=passphrase)
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].set("dpp_config_processing", "2")
    id0 = dev[0].dpp_bootstrap_gen(chan="81/1", mac=True)
    uri0 = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id0)
    dev[0].dpp_listen(2412)
    configurator = dev[1].dpp_configurator_add(curve=conf_curve)
    conf = 'sta-psk'
    dev[1].dpp_auth_init(uri=uri0, conf=conf, ssid=ssid,
                         passphrase=passphrase, configurator=configurator,
                         conn_status=True)
    res = wait_auth_success(dev[0], dev[1], configurator=dev[1],
                            enrollee=dev[0])
    if 'wait_conn_status' not in res:
        raise Exception("Configurator did not request connection status")
    ev = dev[1].wait_event(["DPP-CONN-STATUS-RESULT"], timeout=20)
    if ev is None:
        raise Exception("No connection status reported")
    dev[1].dump_monitor()

    ev = dev[0].wait_event(["DPP-CONFOBJ-SSID"], timeout=1)
    if ev is None:
        raise Exception("SSID not reported")
    res_ssid = ev.split(' ')[1]
    if res_ssid != ssid:
        raise Exception("Unexpected SSID value")

    ev = dev[0].wait_event(["DPP-CONNECTOR"], timeout=1)
    if ev is None:
        raise Exception("Connector not reported")
    connector = ev.split(' ')[1]

    ev = dev[0].wait_event(["DPP-C-SIGN-KEY"], timeout=1)
    if ev is None:
        raise Exception("C-sign-key not reported")
    p = ev.split(' ')
    csign = p[1]

    ev = dev[0].wait_event(["DPP-NET-ACCESS-KEY"], timeout=1)
    if ev is None:
        raise Exception("netAccessKey not reported")
    p = ev.split(' ')
    net_access_key = p[1]
    net_access_key_expiry = p[2] if len(p) > 2 else None

    ev = dev[0].wait_event(["DPP-NETWORK-ID"], timeout=1)
    if ev is None:
        raise Exception("DPP network profile not generated")
    id = ev.split(' ')[1]

    dev[0].wait_connected()

    n_key_mgmt = dev[0].get_network(id, "key_mgmt")
    if n_key_mgmt != "WPA-PSK FT-PSK WPA-PSK-SHA256":
        raise Exception("Unexpected key_mgmt: " + n_key_mgmt)
    n_connector = dev[0].get_network(id, "dpp_connector")
    if n_connector.strip('"') != connector:
        raise Exception("Connector mismatch: %s %s" % (n_connector, connector))
    n_csign = dev[0].get_network(id, "dpp_csign")
    if n_csign.strip('"') != csign:
        raise Exception("csign mismatch: %s %s" % (n_csign, csign))
    n_net_access_key = dev[0].get_network(id, "dpp_netaccesskey")
    if n_net_access_key.strip('"') != net_access_key:
        raise Exception("net_access_key mismatch: %s %s" % (n_net_access_key,
                                                            net_access_key))

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    hapd.disable()
    hapd.set("wpa_passphrase", passphrase2)
    hapd.enable()

    time.sleep(0.1)
    dev[0].dump_monitor()
    dev[1].dump_monitor()

    if test_retries:
        dev[1].request("DPP_STOP_LISTEN")
        if "OK" not in dev[0].request("DPP_RECONFIG %s iter=10" % id):
            raise Exception("Failed to start reconfiguration")
        return

    dev[1].set("dpp_configurator_params",
               "conf=sta-psk ssid=%s pass=%s conn_status=1" % (binascii.hexlify(ssid.encode()).decode(), binascii.hexlify(passphrase2.encode()).decode()))
    dev[1].dpp_listen(2437)

    if "OK" not in dev[0].request("DPP_RECONFIG %s" % id):
        raise Exception("Failed to start reconfiguration")
    ev = dev[0].wait_event(["DPP-TX "], timeout=10)
    if ev is None or "type=14" not in ev:
        raise Exception("Reconfig Announcement not sent")

    ev = dev[1].wait_event(["DPP-RX"], timeout=5)
    if ev is None:
        raise Exception("DPP Reconfig Announcement not received")
    if "freq=2437 type=14" not in ev:
        raise Exception("Unexpected RX data for Reconfig Announcement: " + ev)

    ev = dev[0].wait_event(["DPP-RX"], timeout=5)
    if ev is None or "freq=2437 type=15" not in ev:
        raise Exception("DPP Reconfig Authentication Request not received")

    ev = dev[1].wait_event(["DPP-RX"], timeout=5)
    if ev is None or "freq=2437 type=16" not in ev:
        raise Exception("DPP Reconfig Authentication Response not received")

    ev = dev[0].wait_event(["DPP-RX"], timeout=5)
    if ev is None or "freq=2437 type=17" not in ev:
        raise Exception("DPP Reconfig Authentication Confirm not received")

    ev = dev[0].wait_event(["GAS-QUERY-START"], timeout=5)
    if ev is None or "freq=2437" not in ev:
        raise Exception("DPP Config Request (GAS) not transmitted")

    ev = dev[1].wait_event(["DPP-CONF-REQ-RX"], timeout=5)
    if ev is None:
        raise Exception("DPP Config Request (GAS) not received")

    ev = dev[0].wait_event(["GAS-QUERY-DONE"], timeout=5)
    if ev is None or "freq=2437" not in ev:
        raise Exception("DPP Config Response (GAS) not received")

    ev = dev[1].wait_event(["DPP-RX"], timeout=5)
    if ev is None or "freq=2437 type=11" not in ev:
        raise Exception("DPP Config Result not received")

    ev = dev[1].wait_event(["DPP-CONF-SENT"], timeout=5)
    if ev is None:
        raise Exception("DPP Config Response (GAS) not transmitted")

    ev = dev[0].wait_event(["DPP-CONF-RECEIVED", "DPP-CONF-FAILED"], timeout=5)
    if ev is None:
        raise Exception("DPP config response reception result not indicated")
    if "DPP-CONF-RECEIVED" not in ev:
        raise Exception("Reconfiguration failed")

    dev[0].wait_connected()

    ev = dev[1].wait_event(["DPP-CONN-STATUS-RESULT"], timeout=20)
    if ev is None:
        raise Exception("No connection status reported")

def test_dpp_reconfig_hostapd_configurator(dev, apdev):
    """DPP reconfiguration with hostapd as configurator"""
    try:
        run_dpp_reconfig_hostapd_configurator(dev, apdev)
    finally:
        dev[0].set("dpp_config_processing", "0", allow_fail=True)

def run_dpp_reconfig_hostapd_configurator(dev, apdev):
    ssid = "reconfig-ap"
    check_dpp_capab(dev[0], min_ver=2)
    hapd = hostapd.add_ap(apdev[0], {"ssid": "unconfigured"})
    check_dpp_capab(hapd, min_ver=2)
    conf_id = hapd.dpp_configurator_add()

    cmd = "DPP_CONFIGURATOR_SIGN conf=ap-dpp configurator=%d ssid=%s" % (conf_id, binascii.hexlify(ssid.encode()).decode())
    res = hapd.request(cmd)
    if "FAIL" in res:
        raise Exception("Failed to generate own configuration")
    hapd.set("dpp_configurator_connectivity", "1")
    update_hapd_config(hapd)

    id = dev[0].dpp_bootstrap_gen(chan="81/1", mac=True)
    uri = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id)
    dev[0].set("dpp_config_processing", "2")
    dev[0].dpp_listen(2412)
    hapd.dpp_auth_init(uri=uri, conf="sta-dpp", configurator=conf_id,
                       extra="expiry=%d" % (time.time() + 10), ssid=ssid)
    wait_auth_success(dev[0], hapd, configurator=hapd, enrollee=dev[0])
    ev = dev[0].wait_event(["DPP-NETWORK-ID"], timeout=1)
    if ev is None:
        raise Exception("DPP network id not reported")
    network = int(ev.split(' ')[1])
    dev[0].wait_connected()
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()
    dev[0].dump_monitor()
    time.sleep(10)
    if "FAIL" in dev[0].request("PMKSA_FLUSH"):
        raise Exception("PMKSA_FLUSH failed")
    dev[0].request("RECONNECT")
    ev = dev[0].wait_event(["DPP-MISSING-CONNECTOR", "CTRL-EVENT-CONNECTED"],
                           timeout=15)
    if ev is None or "DPP-MISSING-CONNECTOR" not in ev:
        raise Exception("Missing Connector not reported")
    if "netAccessKey expired" not in ev:
        raise Exception("netAccessKey expiry not indicated")
    dev[0].request("DISCONNECT")
    dev[0].dump_monitor()

    hapd.set("dpp_configurator_params",
             "conf=sta-dpp configurator=%d ssid=%s" % (conf_id, binascii.hexlify(ssid.encode()).decode()))

    if "OK" not in dev[0].request("DPP_RECONFIG %s" % network):
        raise Exception("Failed to start reconfiguration")
    ev = dev[0].wait_event(["DPP-NETWORK-ID"], timeout=15)
    if ev is None:
        raise Exception("DPP network id not reported for reconfiguration")
    network2 = int(ev.split(' ')[1])
    if network == network2:
        raise Exception("Network ID did not change")
    dev[0].wait_connected()

def test_dpp_qr_code_auth_rand_mac_addr(dev, apdev):
    """DPP QR Code and authentication exchange (rand_mac_addr=1)"""
    flags = int(dev[0].get_driver_status_field('capa.flags'), 16)
    if flags & 0x0000400000000000 == 0:
        raise HwsimSkip("Driver does not support random GAS TA")

    try:
        dev[0].set("gas_rand_mac_addr", "1")
        run_dpp_qr_code_auth_unicast(dev, apdev, None)
    finally:
        dev[0].set("gas_rand_mac_addr", "0")

def dpp_sign_cert(cacert, cakey, csr_der):
    csr = OpenSSL.crypto.load_certificate_request(OpenSSL.crypto.FILETYPE_ASN1,
                                                  csr_der)
    cert = OpenSSL.crypto.X509()
    cert.set_serial_number(12345)
    cert.gmtime_adj_notBefore(-10)
    cert.gmtime_adj_notAfter(100000)
    cert.set_pubkey(csr.get_pubkey())
    dn = csr.get_subject()
    cert.set_subject(dn)
    cert.set_version(2)
    cert.add_extensions([
        OpenSSL.crypto.X509Extension(b"basicConstraints", True,
                                     b"CA:FALSE"),
        OpenSSL.crypto.X509Extension(b"subjectKeyIdentifier", False,
                                     b"hash", subject=cert),
        OpenSSL.crypto.X509Extension(b"authorityKeyIdentifier", False,
                                     b"keyid:always", issuer=cacert),
    ])
    cert.set_issuer(cacert.get_subject())
    cert.sign(cakey, "sha256")
    return cert

def test_dpp_enterprise(dev, apdev, params):
    """DPP and enterprise EAP-TLS provisioning"""
    check_dpp_capab(dev[0], min_ver=2)
    try:
        dev[0].set("dpp_config_processing", "2")
        run_dpp_enterprise(dev, apdev, params)
    finally:
        dev[0].set("dpp_config_processing", "0", allow_fail=True)

def run_dpp_enterprise(dev, apdev, params):
    if not openssl_imported:
        raise HwsimSkip("OpenSSL python method not available")
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])

    cert_file = params['prefix'] + ".cert.pem"
    pkcs7_file = params['prefix'] + ".pkcs7.der"

    params = {"ssid": "dpp-ent",
              "wpa": "2",
              "wpa_key_mgmt": "WPA-EAP",
              "rsn_pairwise": "CCMP",
              "ieee8021x": "1",
              "eap_server": "1",
              "eap_user_file": "auth_serv/eap_user.conf",
              "ca_cert": "auth_serv/ec-ca.pem",
              "server_cert": "auth_serv/ec-server.pem",
              "private_key": "auth_serv/ec-server.key"}
    hapd = hostapd.add_ap(apdev[0], params)

    with open("auth_serv/ec-ca.pem", "rb") as f:
        res = f.read()
        cacert = OpenSSL.crypto.load_certificate(OpenSSL.crypto.FILETYPE_PEM,
                                                 res)

    with open("auth_serv/ec-ca.key", "rb") as f:
        res = f.read()
        cakey = OpenSSL.crypto.load_privatekey(OpenSSL.crypto.FILETYPE_PEM, res)

    conf_id = dev[1].dpp_configurator_add()
    id0 = dev[0].dpp_bootstrap_gen(chan="81/1", mac=True)
    uri0 = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id0)
    dev[0].dpp_listen(2412)
    csrattrs = "MAsGCSqGSIb3DQEJBw=="
    id1 = dev[1].dpp_auth_init(uri=uri0, configurator=conf_id, conf="sta-dot1x",
                               csrattrs=csrattrs, ssid="dpp-ent")

    ev = dev[1].wait_event(["DPP-CSR"], timeout=10)
    if ev is None:
        raise Exception("Configurator did not receive CSR")
    id1_csr = int(ev.split(' ')[1].split('=')[1])
    if id1 != id1_csr:
        raise Exception("Peer bootstrapping ID mismatch in CSR event")
    csr = ev.split(' ')[2]
    if not csr.startswith("csr="):
        raise Exception("Could not parse CSR event: " + ev)
    csr = csr[4:]
    csr = base64.b64decode(csr.encode())
    logger.info("CSR: " + binascii.hexlify(csr).decode())

    cert = dpp_sign_cert(cacert, cakey, csr)
    with open(cert_file, 'wb') as f:
        f.write(OpenSSL.crypto.dump_certificate(OpenSSL.crypto.FILETYPE_PEM,
                                                cert))
    subprocess.check_call(['openssl', 'crl2pkcs7', '-nocrl',
                           '-certfile', cert_file,
                           '-certfile', 'auth_serv/ec-ca.pem',
                           '-outform', 'DER', '-out', pkcs7_file])

    #caCert = base64.b64encode(b"TODO").decode()
    #res = dev[1].request("DPP_CA_SET peer=%d name=caCert value=%s" % (id1, caCert))
    #if "OK" not in res:
    #    raise Exception("Failed to set caCert")

    name = "server.w1.fi"
    res = dev[1].request("DPP_CA_SET peer=%d name=trustedEapServerName value=%s" % (id1, name))
    if "OK" not in res:
        raise Exception("Failed to set trustedEapServerName")

    with open(pkcs7_file, 'rb') as f:
        pkcs7_der = f.read()
        certbag = base64.b64encode(pkcs7_der).decode()
    res = dev[1].request("DPP_CA_SET peer=%d name=certBag value=%s" % (id1, certbag))
    if "OK" not in res:
        raise Exception("Failed to set certBag")

    ev = dev[1].wait_event(["DPP-CONF-SENT", "DPP-CONF-FAILED"], timeout=5)
    if ev is None:
        raise Exception("DPP configuration not completed (Configurator)")
    if "DPP-CONF-FAILED" in ev:
        raise Exception("DPP configuration did not succeed (Configurator)")

    ev = dev[0].wait_event(["DPP-CONF-RECEIVED", "DPP-CONF-FAILED"],
                           timeout=1)
    if ev is None:
        raise Exception("DPP configuration not completed (Enrollee)")
    if "DPP-CONF-FAILED" in ev:
        raise Exception("DPP configuration did not succeed (Enrollee)")

    ev = dev[0].wait_event(["DPP-CERTBAG"], timeout=1)
    if ev is None:
        raise Exception("DPP-CERTBAG not reported")
    certbag = base64.b64decode(ev.split(' ')[1].encode())
    if certbag != pkcs7_der:
        raise Exception("DPP-CERTBAG mismatch")

    #ev = dev[0].wait_event(["DPP-CACERT"], timeout=1)
    #if ev is None:
    #    raise Exception("DPP-CACERT not reported")

    ev = dev[0].wait_event(["DPP-SERVER-NAME"], timeout=1)
    if ev is None:
        raise Exception("DPP-SERVER-NAME not reported")
    if ev.split(' ')[1] != name:
        raise Exception("DPP-SERVER-NAME mismatch: " + ev)

    ev = dev[0].wait_event(["DPP-NETWORK-ID"], timeout=1)
    if ev is None:
        raise Exception("DPP network profile not generated")
    id = ev.split(' ')[1]

    dev[0].wait_connected()

def test_dpp_enterprise_reject(dev, apdev, params):
    """DPP and enterprise EAP-TLS provisioning and CSR getting rejected"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])

    conf_id = dev[1].dpp_configurator_add()
    id0 = dev[0].dpp_bootstrap_gen(chan="81/1", mac=True)
    uri0 = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id0)
    dev[0].dpp_listen(2412)
    csrattrs = "MAsGCSqGSIb3DQEJBw=="
    id1 = dev[1].dpp_auth_init(uri=uri0, configurator=conf_id, conf="sta-dot1x",
                               csrattrs=csrattrs, ssid="dpp-ent")

    ev = dev[1].wait_event(["DPP-CSR"], timeout=10)
    if ev is None:
        raise Exception("Configurator did not receive CSR")

    res = dev[1].request("DPP_CA_SET peer=%d name=status value=5" % id1)
    if "OK" not in res:
        raise Exception("Failed to set status")

    ev = dev[1].wait_event(["DPP-CONF-SENT", "DPP-CONF-FAILED"], timeout=5)
    if ev is None:
        raise Exception("DPP configuration not completed (Configurator)")
    if "DPP-CONF-FAILED" in ev:
        raise Exception("DPP configuration did not succeed (Configurator)")

    ev = dev[0].wait_event(["DPP-CONF-RECEIVED", "DPP-CONF-FAILED"],
                           timeout=1)
    if ev is None:
        raise Exception("DPP configuration not completed (Enrollee)")
    if "DPP-CONF-FAILED" not in ev:
        raise Exception("DPP configuration did not fail (Enrollee)")

def test_dpp_enterprise_tcp(dev, apdev, params):
    """DPP over TCP for enterprise provisioning"""
    if not openssl_imported:
        raise HwsimSkip("OpenSSL python method not available")

    try:
        run_dpp_enterprise_tcp(dev, apdev, params)
    finally:
        dev[1].request("DPP_CONTROLLER_STOP")

def run_dpp_enterprise_tcp(dev, apdev, params):
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])

    cap_lo = params['prefix'] + ".lo.pcap"

    wt = WlantestCapture('lo', cap_lo)
    time.sleep(1)

    # Controller
    conf_id = dev[1].dpp_configurator_add()
    csrattrs = "MAsGCSqGSIb3DQEJBw=="
    dev[1].set("dpp_configurator_params",
               "conf=sta-dot1x configurator=%d csrattrs=%s" % (conf_id, csrattrs))
    id_c = dev[1].dpp_bootstrap_gen()
    uri_c = dev[1].request("DPP_BOOTSTRAP_GET_URI %d" % id_c)
    res = dev[1].request("DPP_BOOTSTRAP_INFO %d" % id_c)
    req = "DPP_CONTROLLER_START"
    if "OK" not in dev[1].request(req):
        raise Exception("Failed to start Controller")

    dev[0].dpp_auth_init(uri=uri_c, role="enrollee", tcp_addr="127.0.0.1")
    run_dpp_enterprise_tcp_end(params, dev, wt)

def run_dpp_enterprise_tcp_end(params, dev, wt):
    cert_file = params['prefix'] + ".cert.pem"
    pkcs7_file = params['prefix'] + ".pkcs7.der"

    with open("auth_serv/ec-ca.pem", "rb") as f:
        res = f.read()
        cacert = OpenSSL.crypto.load_certificate(OpenSSL.crypto.FILETYPE_PEM,
                                                 res)

    with open("auth_serv/ec-ca.key", "rb") as f:
        res = f.read()
        cakey = OpenSSL.crypto.load_privatekey(OpenSSL.crypto.FILETYPE_PEM, res)

    ev = dev[1].wait_event(["DPP-CSR"], timeout=10)
    if ev is None:
        raise Exception("Configurator did not receive CSR")
    id1_csr = int(ev.split(' ')[1].split('=')[1])
    csr = ev.split(' ')[2]
    if not csr.startswith("csr="):
        raise Exception("Could not parse CSR event: " + ev)
    csr = csr[4:]
    csr = base64.b64decode(csr.encode())
    logger.info("CSR: " + binascii.hexlify(csr).decode())

    cert = dpp_sign_cert(cacert, cakey, csr)
    with open(cert_file, 'wb') as f:
        f.write(OpenSSL.crypto.dump_certificate(OpenSSL.crypto.FILETYPE_PEM,
                                                cert))
    subprocess.check_call(['openssl', 'crl2pkcs7', '-nocrl',
                           '-certfile', cert_file,
                           '-certfile', 'auth_serv/ec-ca.pem',
                           '-outform', 'DER', '-out', pkcs7_file])

    with open(pkcs7_file, 'rb') as f:
        pkcs7_der = f.read()
        certbag = base64.b64encode(pkcs7_der).decode()
    res = dev[1].request("DPP_CA_SET peer=%d name=certBag value=%s" % (id1_csr, certbag))
    if "OK" not in res:
        raise Exception("Failed to set certBag")

    ev = dev[1].wait_event(["DPP-CONF-SENT", "DPP-CONF-FAILED"], timeout=5)
    if ev is None:
        raise Exception("DPP configuration not completed (Configurator)")
    if "DPP-CONF-FAILED" in ev:
        raise Exception("DPP configuration did not succeed (Configurator)")

    ev = dev[0].wait_event(["DPP-CONF-RECEIVED", "DPP-CONF-FAILED"],
                           timeout=1)
    if ev is None:
        raise Exception("DPP configuration not completed (Enrollee)")
    if "DPP-CONF-RECEIVED" not in ev:
        raise Exception("DPP configuration did not succeed (Enrollee)")

    time.sleep(0.5)
    wt.close()

def test_dpp_enterprise_tcp2(dev, apdev, params):
    """DPP over TCP for enterprise provisioning (Controller initiating)"""
    if not openssl_imported:
        raise HwsimSkip("OpenSSL python method not available")

    try:
        run_dpp_enterprise_tcp2(dev, apdev, params)
    finally:
        dev[0].request("DPP_CONTROLLER_STOP")
        dev[1].request("DPP_CONTROLLER_STOP")

def run_dpp_enterprise_tcp2(dev, apdev, params):
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])

    cap_lo = params['prefix'] + ".lo.pcap"
    cert_file = params['prefix'] + ".cert.pem"
    pkcs7_file = params['prefix'] + ".pkcs7.der"

    with open("auth_serv/ec-ca.pem", "rb") as f:
        res = f.read()
        cacert = OpenSSL.crypto.load_certificate(OpenSSL.crypto.FILETYPE_PEM,
                                                 res)

    with open("auth_serv/ec-ca.key", "rb") as f:
        res = f.read()
        cakey = OpenSSL.crypto.load_privatekey(OpenSSL.crypto.FILETYPE_PEM, res)

    wt = WlantestCapture('lo', cap_lo)
    time.sleep(1)

    # Client/Enrollee/Responder
    id_e = dev[0].dpp_bootstrap_gen()
    uri_e = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id_e)
    req = "DPP_CONTROLLER_START"
    if "OK" not in dev[0].request(req):
        raise Exception("Failed to start Client/Enrollee")

    # Controller/Configurator/Initiator
    conf_id = dev[1].dpp_configurator_add()
    csrattrs = "MAsGCSqGSIb3DQEJBw=="
    dev[1].dpp_auth_init(uri=uri_e, role="configurator", configurator=conf_id,
                         conf="sta-dot1x", csrattrs=csrattrs,
                         tcp_addr="127.0.0.1")

    run_dpp_enterprise_tcp_end(params, dev, wt)
