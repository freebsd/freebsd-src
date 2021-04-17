# External password storage
# Copyright (c) 2014, Jouni Malinen <j@w1.fi>
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

from remotehost import remote_compatible
import logging
logger = logging.getLogger()
import os
import tempfile

import hostapd
from utils import skip_with_fips
from wpasupplicant import WpaSupplicant
from test_ap_hs20 import hs20_ap_params
from test_ap_hs20 import interworking_select
from test_ap_hs20 import interworking_connect

@remote_compatible
def test_ext_password_psk(dev, apdev):
    """External password storage for PSK"""
    params = hostapd.wpa2_params(ssid="ext-pw-psk", passphrase="12345678")
    hostapd.add_ap(apdev[0], params)
    dev[0].request("SET ext_password_backend test:psk1=12345678")
    dev[0].connect("ext-pw-psk", raw_psk="ext:psk1", scan_freq="2412")

def test_ext_password_psk_not_found(dev, apdev):
    """External password storage for PSK and PSK not found"""
    params = hostapd.wpa2_params(ssid="ext-pw-psk", passphrase="12345678")
    hostapd.add_ap(apdev[0], params)
    dev[0].request("SET ext_password_backend test:psk1=12345678")
    dev[0].connect("ext-pw-psk", raw_psk="ext:psk2", scan_freq="2412",
                   wait_connect=False)
    dev[1].request("SET ext_password_backend test:psk1=1234567")
    dev[1].connect("ext-pw-psk", raw_psk="ext:psk1", scan_freq="2412",
                   wait_connect=False)
    dev[2].request("SET ext_password_backend test:psk1=1234567890123456789012345678901234567890123456789012345678901234567890")
    dev[2].connect("ext-pw-psk", raw_psk="ext:psk1", scan_freq="2412",
                   wait_connect=False)
    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    wpas.interface_add("wlan5")
    wpas.request("SET ext_password_backend test:psk1=123456789012345678901234567890123456789012345678901234567890123q")
    wpas.connect("ext-pw-psk", raw_psk="ext:psk1", scan_freq="2412",
                 wait_connect=False)

    ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED"], timeout=1)
    if ev is not None:
        raise Exception("Unexpected association")
    ev = dev[1].wait_event(["CTRL-EVENT-CONNECTED"], timeout=0.1)
    if ev is not None:
        raise Exception("Unexpected association")
    ev = dev[2].wait_event(["CTRL-EVENT-CONNECTED"], timeout=0.1)
    if ev is not None:
        raise Exception("Unexpected association")
    ev = wpas.wait_event(["CTRL-EVENT-CONNECTED"], timeout=0.1)
    if ev is not None:
        raise Exception("Unexpected association")

def test_ext_password_eap(dev, apdev):
    """External password storage for EAP password"""
    params = hostapd.wpa2_eap_params(ssid="ext-pw-eap")
    hostapd.add_ap(apdev[0], params)
    dev[0].request("SET ext_password_backend test:pw0=hello|pw1=password|pw2=secret")
    dev[0].connect("ext-pw-eap", key_mgmt="WPA-EAP", eap="PEAP",
                   identity="user", password_hex="ext:pw1",
                   ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2",
                   scan_freq="2412")

def test_ext_password_interworking(dev, apdev):
    """External password storage for Interworking network selection"""
    skip_with_fips(dev[0])
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable()
    dev[0].request("SET ext_password_backend test:pw1=password")
    id = dev[0].add_cred_values({'realm': "example.com",
                                 'username': "hs20-test"})
    dev[0].set_cred(id, "password", "ext:pw1")
    interworking_select(dev[0], bssid, freq="2412")
    interworking_connect(dev[0], bssid, "TTLS")

def test_ext_password_file_psk(dev, apdev):
    """External password (file) storage for PSK"""
    params = hostapd.wpa2_params(ssid="ext-pw-psk", passphrase="12345678")
    hostapd.add_ap(apdev[0], params)
    fd, fn = tempfile.mkstemp()
    with open(fn, "w") as f:
        f.write("psk1=12345678\n")
    os.close(fd)
    dev[0].request("SET ext_password_backend file:%s" % fn)
    dev[0].connect("ext-pw-psk", raw_psk="ext:psk1", scan_freq="2412")
    for i in range(2):
        dev[0].request("REMOVE_NETWORK all")
        if i == 0:
            dev[0].wait_disconnected()
            dev[0].connect("ext-pw-psk", raw_psk="ext:psk2", scan_freq="2412",
                           wait_connect=False)
        else:
            dev[0].connect("ext-pw-psk", raw_psk="ext:psk1", scan_freq="2412",
                           wait_connect=False)
        ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED",
                                "EXT PW: No PSK found from external storage"],
                               timeout=10)
        if i == 0:
            os.unlink(fn)
        if ev is None:
            raise Exception("No connection result reported")
        if "CTRL-EVENT-CONNECTED" in ev:
            raise Exception("Unexpected connection")
