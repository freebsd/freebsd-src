# Test cases for X.509 certificate checking
# Copyright (c) 2019, The Linux Foundation
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

import os
try:
    import OpenSSL
    openssl_imported = True
except ImportError:
    openssl_imported = False

from utils import HwsimSkip
import hostapd
from test_ap_eap import check_domain_suffix_match, check_altsubject_match_support, check_domain_match

def check_cert_check_support():
    if not openssl_imported:
        raise HwsimSkip("OpenSSL python method not available")

def start_hapd(apdev, server_cert="auth_serv/server.pem"):
    params = {"ssid": "cert-check", "wpa": "2", "wpa_key_mgmt": "WPA-EAP",
              "rsn_pairwise": "CCMP", "ieee8021x": "1",
              "eap_server": "1", "eap_user_file": "auth_serv/eap_user.conf",
              "ca_cert": "auth_serv/ca.pem",
              "server_cert": server_cert,
              "private_key": "auth_serv/server.key",
              "dh_file": "auth_serv/dh.conf"}
    hapd = hostapd.add_ap(apdev, params)
    return hapd

def load_certs():
    with open("auth_serv/ca.pem", "rb") as f:
        res = f.read()
        cacert = OpenSSL.crypto.load_certificate(OpenSSL.crypto.FILETYPE_PEM,
                                                 res)

    with open("auth_serv/ca-key.pem", "rb") as f:
        res = f.read()
        cakey = OpenSSL.crypto.load_privatekey(OpenSSL.crypto.FILETYPE_PEM, res)

    with open("auth_serv/server.pem", "rb") as f:
        res = f.read()
        servercert = OpenSSL.crypto.load_certificate(OpenSSL.crypto.FILETYPE_PEM, res)

    return cacert, cakey, servercert

def start_cert(servercert, cacert, cn='server.w1.fi', v3=True):
    cert = OpenSSL.crypto.X509()
    cert.set_serial_number(12345)
    cert.gmtime_adj_notBefore(-10)
    cert.gmtime_adj_notAfter(1000)
    cert.set_pubkey(servercert.get_pubkey())
    dn = cert.get_subject()
    dn.CN = cn
    cert.set_subject(dn)
    if v3:
        cert.set_version(2)
        cert.add_extensions([
            OpenSSL.crypto.X509Extension(b"basicConstraints", True,
                                         b"CA:FALSE"),
            OpenSSL.crypto.X509Extension(b"subjectKeyIdentifier", False,
                                         b"hash", subject=cert),
            OpenSSL.crypto.X509Extension(b"authorityKeyIdentifier", False,
                                         b"keyid:always", issuer=cacert),
        ])
    return cert

def sign_cert(cert, cert_file, cakey, cacert):
    cert.set_issuer(cacert.get_subject())
    cert.sign(cakey, "sha256")
    with open(cert_file, 'wb') as f:
        f.write(OpenSSL.crypto.dump_certificate(OpenSSL.crypto.FILETYPE_PEM,
                                                cert))

def check_connect(dev, fail=False, wait_error=None, **kwargs):
    dev.connect("cert-check", key_mgmt="WPA-EAP", eap="TTLS",
                identity="pap user", anonymous_identity="ttls",
                password="password",
                ca_cert="auth_serv/ca.pem", phase2="auth=PAP",
                scan_freq="2412", wait_connect=False, **kwargs)
    ev = dev.wait_event(["CTRL-EVENT-EAP-STARTED"], timeout=10)
    if ev is None:
        raise Exception("EAP not started")
    if fail:
        if wait_error:
            ev = dev.wait_event([wait_error], timeout=5)
            if ev is None:
                raise Exception("Specific error not reported")
        ev = dev.wait_event(["CTRL-EVENT-EAP-FAILURE"], timeout=5)
        if ev is None:
            raise Exception("EAP failure not reported")
    else:
        dev.wait_connected()
    dev.request("REMOVE_NETWORK all")
    dev.request("ABORT_SCAN")
    dev.wait_disconnected()
    dev.dump_monitor()

def test_cert_check_basic(dev, apdev, params):
    """Basic test with generated X.509 server certificate"""
    check_cert_check_support()
    cert_file = os.path.join(params['logdir'], "cert_check_basic.pem")
    cacert, cakey, servercert = load_certs()

    cert = start_cert(servercert, cacert, v3=False)
    sign_cert(cert, cert_file, cakey, cacert)
    hapd = start_hapd(apdev[0], server_cert=cert_file)
    check_connect(dev[0])

def test_cert_check_v3(dev, apdev, params):
    """Basic test with generated X.509v3 server certificate"""
    check_cert_check_support()
    cert_file = os.path.join(params['logdir'], "cert_check_v3.pem")
    cacert, cakey, servercert = load_certs()

    cert = start_cert(servercert, cacert)
    sign_cert(cert, cert_file, cakey, cacert)
    hapd = start_hapd(apdev[0], server_cert=cert_file)
    check_connect(dev[0])

def test_cert_check_dnsname(dev, apdev, params):
    """Certificate check with multiple dNSName values"""
    check_cert_check_support()
    check_domain_suffix_match(dev[0])
    check_domain_match(dev[0])
    cert_file = os.path.join(params['logdir'], "cert_check_dnsname.pem")
    cacert, cakey, servercert = load_certs()

    cert = start_cert(servercert, cacert, cn="server")
    dns = ["DNS:one.example.com", "DNS:two.example.com",
           "DNS:three.example.com"]
    cert.add_extensions([OpenSSL.crypto.X509Extension(b"subjectAltName", False,
                                                      ",".join(dns).encode())])
    sign_cert(cert, cert_file, cakey, cacert)
    hapd = start_hapd(apdev[0], server_cert=cert_file)
    check_connect(dev[0])

    tests = ["two.example.com",
             "one.example.com",
             "tWo.Example.com",
             "three.example.com",
             "no.match.example.com;two.example.com;no.match.example.org",
             "no.match.example.com;example.com;no.match.example.org",
             "no.match.example.com;no.match.example.org;example.com",
             "example.com",
             "com"]
    for match in tests:
        check_connect(dev[0], domain_suffix_match=match)

    tests = ["four.example.com",
             "foo.one.example.com",
             "no.match.example.org;no.match.example.com",
             "xample.com"]
    for match in tests:
        check_connect(dev[0], fail=True,
                      wait_error="CTRL-EVENT-EAP-TLS-CERT-ERROR",
                      domain_suffix_match=match)

    tests = ["one.example.com",
             "two.example.com",
             "three.example.com",
             "no.match.example.com;two.example.com;no.match.example.org",
             "tWo.Example.Com"]
    for match in tests:
        check_connect(dev[0], domain_match=match)

    tests = ["four.example.com",
             "foo.one.example.com",
             "example.com",
             "xample.com",
             "no.match.example.org;no.match.example.com",
             "ne.example.com"]
    for match in tests:
        check_connect(dev[0], fail=True,
                      wait_error="CTRL-EVENT-EAP-TLS-CERT-ERROR",
                      domain_match=match)

def test_cert_check_dnsname_wildcard(dev, apdev, params):
    """Certificate check with multiple dNSName wildcard values"""
    check_cert_check_support()
    check_domain_suffix_match(dev[0])
    check_domain_match(dev[0])
    cert_file = os.path.join(params['logdir'], "cert_check_dnsname.pem")
    cacert, cakey, servercert = load_certs()

    cert = start_cert(servercert, cacert, cn="server")
    dns = ["DNS:*.one.example.com", "DNS:two.example.com",
           "DNS:*.three.example.com"]
    cert.add_extensions([OpenSSL.crypto.X509Extension(b"subjectAltName", False,
                                                      ",".join(dns).encode())])
    sign_cert(cert, cert_file, cakey, cacert)
    hapd = start_hapd(apdev[0], server_cert=cert_file)
    check_connect(dev[0])

    tests = ["two.example.com",
             "one.example.com",
             "tWo.Example.com",
             "three.example.com",
             "no.match.example.com;two.example.com;no.match.example.org",
             "no.match.example.com;example.com;no.match.example.org",
             "no.match.example.com;no.match.example.org;example.com",
             "example.com",
             "com"]
    for match in tests:
        check_connect(dev[0], domain_suffix_match=match)

    tests = ["four.example.com",
             "foo.one.example.com",
             "no.match.example.org;no.match.example.com",
             "xample.com"]
    for match in tests:
        check_connect(dev[0], fail=True,
                      wait_error="CTRL-EVENT-EAP-TLS-CERT-ERROR",
                      domain_suffix_match=match)

    tests = ["*.one.example.com",
             "two.example.com",
             "*.three.example.com",
             "no.match.example.com;two.example.com;no.match.example.org",
             "tWo.Example.Com"]
    for match in tests:
        check_connect(dev[0], domain_match=match)

    tests = ["four.example.com",
             "foo.one.example.com",
             "example.com",
             "xample.com",
             "no.match.example.org;no.match.example.com",
             "one.example.com"]
    for match in tests:
        check_connect(dev[0], fail=True,
                      wait_error="CTRL-EVENT-EAP-TLS-CERT-ERROR",
                      domain_match=match)

def test_cert_check_dnsname_alt(dev, apdev, params):
    """Certificate check with multiple dNSName values using altsubject_match"""
    check_cert_check_support()
    check_altsubject_match_support(dev[0])
    cert_file = os.path.join(params['logdir'], "cert_check_dnsname_alt.pem")
    cacert, cakey, servercert = load_certs()

    cert = start_cert(servercert, cacert, cn="server")
    dns = ["DNS:*.one.example.com", "DNS:two.example.com",
           "DNS:*.three.example.com"]
    cert.add_extensions([OpenSSL.crypto.X509Extension(b"subjectAltName", False,
                                                      ",".join(dns).encode())])
    sign_cert(cert, cert_file, cakey, cacert)
    hapd = start_hapd(apdev[0], server_cert=cert_file)

    tests = ["DNS:*.one.example.com",
             "DNS:two.example.com",
             "DNS:*.three.example.com",
             "DNS:*.three.example.com;DNS:two.example.com;DNS:*.one.example.com",
             "DNS:foo.example.org;DNS:two.example.com;DNS:bar.example.org"]
    for alt in tests:
        check_connect(dev[0], altsubject_match=alt)

    tests = ["DNS:one.example.com",
             "DNS:four.example.com;DNS:five.example.com"]
    for alt in tests:
        check_connect(dev[0], fail=True,
                      wait_error="CTRL-EVENT-EAP-TLS-CERT-ERROR",
                      altsubject_match=alt)

def test_cert_check_dnsname_cn(dev, apdev, params):
    """Certificate check with dNSName in CN"""
    check_cert_check_support()
    check_domain_suffix_match(dev[0])
    check_domain_match(dev[0])
    cert_file = os.path.join(params['logdir'], "cert_check_dnsname_cn.pem")
    cacert, cakey, servercert = load_certs()

    cert = start_cert(servercert, cacert, cn="server.example.com")
    sign_cert(cert, cert_file, cakey, cacert)
    hapd = start_hapd(apdev[0], server_cert=cert_file)
    check_connect(dev[0])

    tests = ["server.example.com",
             "example.com",
             "eXample.Com",
             "no.match.example.com;example.com;no.match.example.org",
             "no.match.example.com;server.example.com;no.match.example.org",
             "com"]
    for match in tests:
        check_connect(dev[0], domain_suffix_match=match)

    tests = ["aaa.example.com",
             "foo.server.example.com",
             "no.match.example.org;no.match.example.com",
             "xample.com"]
    for match in tests:
        check_connect(dev[0], fail=True,
                      wait_error="CTRL-EVENT-EAP-TLS-CERT-ERROR",
                      domain_suffix_match=match)

    tests = ["server.example.com",
             "no.match.example.com;server.example.com;no.match.example.org",
             "sErver.Example.Com"]
    for match in tests:
        check_connect(dev[0], domain_match=match)

    tests = ["aaa.example.com",
             "foo.server.example.com",
             "example.com",
             "no.match.example.org;no.match.example.com",
             "xample.com"]
    for match in tests:
        check_connect(dev[0], fail=True,
                      wait_error="CTRL-EVENT-EAP-TLS-CERT-ERROR",
                      domain_match=match)
