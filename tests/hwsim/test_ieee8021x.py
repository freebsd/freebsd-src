# IEEE 802.1X tests
# Copyright (c) 2013-2019, Jouni Malinen <j@w1.fi>
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

from remotehost import remote_compatible
import binascii
import hmac
import logging
import os
import time

import hostapd
import hwsim_utils
from utils import *
from tshark import run_tshark

logger = logging.getLogger()

def test_ieee8021x_wep104(dev, apdev):
    """IEEE 802.1X connection using dynamic WEP104"""
    check_wep_capa(dev[0])
    skip_with_fips(dev[0])
    params = hostapd.radius_params()
    params["ssid"] = "ieee8021x-wep"
    params["ieee8021x"] = "1"
    params["wep_key_len_broadcast"] = "13"
    params["wep_key_len_unicast"] = "13"
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].connect("ieee8021x-wep", key_mgmt="IEEE8021X", eap="PSK",
                   identity="psk.user@example.com",
                   password_hex="0123456789abcdef0123456789abcdef",
                   scan_freq="2412")
    hwsim_utils.test_connectivity(dev[0], hapd)

def test_ieee8021x_wep40(dev, apdev):
    """IEEE 802.1X connection using dynamic WEP40"""
    check_wep_capa(dev[0])
    skip_with_fips(dev[0])
    params = hostapd.radius_params()
    params["ssid"] = "ieee8021x-wep"
    params["ieee8021x"] = "1"
    params["wep_key_len_broadcast"] = "5"
    params["wep_key_len_unicast"] = "5"
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].connect("ieee8021x-wep", key_mgmt="IEEE8021X", eap="PSK",
                   identity="psk.user@example.com",
                   password_hex="0123456789abcdef0123456789abcdef",
                   scan_freq="2412")
    hwsim_utils.test_connectivity(dev[0], hapd)

def test_ieee8021x_wep_index_workaround(dev, apdev):
    """IEEE 802.1X and EAPOL-Key index workaround"""
    check_wep_capa(dev[0])
    skip_with_fips(dev[0])
    params = hostapd.radius_params()
    params["ssid"] = "ieee8021x-wep"
    params["ieee8021x"] = "1"
    params["wep_key_len_broadcast"] = "5"
    params["eapol_key_index_workaround"] = "1"
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].connect("ieee8021x-wep", key_mgmt="IEEE8021X", eapol_flags="1",
                   eap="PSK",
                   identity="psk.user@example.com",
                   password_hex="0123456789abcdef0123456789abcdef",
                   scan_freq="2412")

def test_ieee8021x_open(dev, apdev):
    """IEEE 802.1X connection using open network"""
    params = hostapd.radius_params()
    params["ssid"] = "ieee8021x-open"
    params["ieee8021x"] = "1"
    hapd = hostapd.add_ap(apdev[0], params)

    id = dev[0].connect("ieee8021x-open", key_mgmt="IEEE8021X", eapol_flags="0",
                        eap="PSK", identity="psk.user@example.com",
                        password_hex="0123456789abcdef0123456789abcdef",
                        scan_freq="2412")
    hwsim_utils.test_connectivity(dev[0], hapd)

    logger.info("Test EAPOL-Logoff")
    dev[0].request("LOGOFF")
    ev = dev[0].wait_event(["CTRL-EVENT-DISCONNECTED"])
    if ev is None:
        raise Exception("Did not get disconnected")
    if "reason=23" not in ev:
        raise Exception("Unexpected disconnection reason")

    dev[0].request("LOGON")
    dev[0].connect_network(id)
    hwsim_utils.test_connectivity(dev[0], hapd)

def test_ieee8021x_static_wep40(dev, apdev):
    """IEEE 802.1X connection using static WEP40"""
    run_static_wep(dev, apdev, '"hello"')

def test_ieee8021x_static_wep104(dev, apdev):
    """IEEE 802.1X connection using static WEP104"""
    run_static_wep(dev, apdev, '"hello-there-/"')

def run_static_wep(dev, apdev, key):
    check_wep_capa(dev[0])
    params = hostapd.radius_params()
    params["ssid"] = "ieee8021x-wep"
    params["ieee8021x"] = "1"
    params["wep_key0"] = key
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].connect("ieee8021x-wep", key_mgmt="IEEE8021X", eap="PSK",
                   identity="psk.user@example.com",
                   password_hex="0123456789abcdef0123456789abcdef",
                   wep_key0=key, eapol_flags="0",
                   scan_freq="2412")
    hwsim_utils.test_connectivity(dev[0], hapd)

def test_ieee8021x_proto(dev, apdev):
    """IEEE 802.1X and EAPOL supplicant protocol testing"""
    params = hostapd.radius_params()
    params["ssid"] = "ieee8021x-open"
    params["ieee8021x"] = "1"
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = apdev[0]['bssid']

    dev[1].request("SET ext_eapol_frame_io 1")
    dev[1].connect("ieee8021x-open", key_mgmt="IEEE8021X", eapol_flags="0",
                   eap="PSK", identity="psk.user@example.com",
                   password_hex="0123456789abcdef0123456789abcdef",
                   scan_freq="2412", wait_connect=False)
    id = dev[0].connect("ieee8021x-open", key_mgmt="IEEE8021X", eapol_flags="0",
                        eap="PSK", identity="psk.user@example.com",
                        password_hex="0123456789abcdef0123456789abcdef",
                        scan_freq="2412")
    ev = dev[1].wait_event(["CTRL-EVENT-EAP-STARTED"], timeout=5)

    start = dev[0].get_mib()

    tests = ["11",
             "11223344",
             "020000050a93000501",
             "020300050a93000501",
             "0203002c0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000",
             "0203002c0100000000000000000000000000000000000000000000000000000000000000000000000000000000000000",
             "0203002c0100050000000000000000000000000000000000000000000000000000000000000000000000000000000000",
             "02aa00050a93000501"]
    for frame in tests:
        res = dev[0].request("EAPOL_RX " + bssid + " " + frame)
        if "OK" not in res:
            raise Exception("EAPOL_RX to wpa_supplicant failed")
        dev[1].request("EAPOL_RX " + bssid + " " + frame)

    stop = dev[0].get_mib()

    logger.info("MIB before test frames: " + str(start))
    logger.info("MIB after test frames: " + str(stop))

    vals = ['dot1xSuppInvalidEapolFramesRx',
            'dot1xSuppEapLengthErrorFramesRx']
    for val in vals:
        if int(stop[val]) <= int(start[val]):
            raise Exception(val + " did not increase")

@remote_compatible
def test_ieee8021x_eapol_start(dev, apdev):
    """IEEE 802.1X and EAPOL-Start retransmissions"""
    params = hostapd.radius_params()
    params["ssid"] = "ieee8021x-open"
    params["ieee8021x"] = "1"
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = apdev[0]['bssid']
    addr0 = dev[0].own_addr()

    hapd.set("ext_eapol_frame_io", "1")
    try:
        dev[0].request("SET EAPOL::startPeriod 1")
        dev[0].request("SET EAPOL::maxStart 1")
        dev[0].connect("ieee8021x-open", key_mgmt="IEEE8021X", eapol_flags="0",
                       eap="PSK", identity="psk.user@example.com",
                       password_hex="0123456789abcdef0123456789abcdef",
                       scan_freq="2412", wait_connect=False)
        held = False
        for i in range(30):
            pae = dev[0].get_status_field('Supplicant PAE state')
            if pae == "HELD":
                mib = hapd.get_sta(addr0, info="eapol")
                if mib['auth_pae_state'] != 'AUTHENTICATING':
                    raise Exception("Unexpected Auth PAE state: " + mib['auth_pae_state'])
                held = True
                break
            time.sleep(0.25)
        if not held:
            raise Exception("PAE state HELD not reached")
        dev[0].wait_disconnected()
    finally:
        dev[0].request("SET EAPOL::startPeriod 30")
        dev[0].request("SET EAPOL::maxStart 3")

def test_ieee8021x_held(dev, apdev):
    """IEEE 802.1X and HELD state"""
    params = hostapd.radius_params()
    params["ssid"] = "ieee8021x-open"
    params["ieee8021x"] = "1"
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = apdev[0]['bssid']

    hapd.set("ext_eapol_frame_io", "1")
    try:
        dev[0].request("SET EAPOL::startPeriod 1")
        dev[0].request("SET EAPOL::maxStart 0")
        dev[0].request("SET EAPOL::heldPeriod 1")
        dev[0].connect("ieee8021x-open", key_mgmt="IEEE8021X", eapol_flags="0",
                       eap="PSK", identity="psk.user@example.com",
                       password_hex="0123456789abcdef0123456789abcdef",
                       scan_freq="2412", wait_connect=False)
        held = False
        for i in range(30):
            pae = dev[0].get_status_field('Supplicant PAE state')
            if pae == "HELD":
                held = True
                break
            time.sleep(0.25)
        if not held:
            raise Exception("PAE state HELD not reached")

        hapd.set("ext_eapol_frame_io", "0")
        for i in range(30):
            pae = dev[0].get_status_field('Supplicant PAE state')
            if pae != "HELD":
                held = False
                break
            time.sleep(0.25)
        if held:
            raise Exception("PAE state HELD not left")
        ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED",
                                "CTRL-EVENT-DISCONNECTED"], timeout=10)
        if ev is None:
            raise Exception("Connection timed out")
        if "CTRL-EVENT-DISCONNECTED" in ev:
            raise Exception("Unexpected disconnection")
    finally:
        dev[0].request("SET EAPOL::startPeriod 30")
        dev[0].request("SET EAPOL::maxStart 3")
        dev[0].request("SET EAPOL::heldPeriod 60")

def test_ieee8021x_force_unauth(dev, apdev):
    """IEEE 802.1X and FORCE_UNAUTH state"""
    params = hostapd.radius_params()
    params["ssid"] = "ieee8021x-open"
    params["ieee8021x"] = "1"
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = apdev[0]['bssid']

    dev[0].connect("ieee8021x-open", key_mgmt="IEEE8021X", eapol_flags="0",
                   eap="PSK", identity="psk.user@example.com",
                   password_hex="0123456789abcdef0123456789abcdef",
                   scan_freq="2412")
    dev[0].request("SET EAPOL::portControl ForceUnauthorized")
    pae = dev[0].get_status_field('Supplicant PAE state')
    dev[0].wait_disconnected()
    dev[0].request("SET EAPOL::portControl Auto")

def send_eapol_key(dev, bssid, signkey, frame_start, frame_end):
    zero_sign = "00000000000000000000000000000000"
    frame = frame_start + zero_sign + frame_end
    hmac_obj = hmac.new(binascii.unhexlify(signkey), digestmod='MD5')
    hmac_obj.update(binascii.unhexlify(frame))
    sign = hmac_obj.digest()
    frame = frame_start + binascii.hexlify(sign).decode() + frame_end
    dev.request("EAPOL_RX " + bssid + " " + frame)

def test_ieee8021x_eapol_key(dev, apdev):
    """IEEE 802.1X connection and EAPOL-Key protocol tests"""
    check_wep_capa(dev[0])
    skip_with_fips(dev[0])
    params = hostapd.radius_params()
    params["ssid"] = "ieee8021x-wep"
    params["ieee8021x"] = "1"
    params["wep_key_len_broadcast"] = "5"
    params["wep_key_len_unicast"] = "5"
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = apdev[0]['bssid']

    dev[0].connect("ieee8021x-wep", key_mgmt="IEEE8021X", eap="VENDOR-TEST",
                   identity="vendor-test", scan_freq="2412")

    # Hardcoded MSK from VENDOR-TEST
    encrkey = "1111111111111111111111111111111111111111111111111111111111111111"
    signkey = "2222222222222222222222222222222222222222222222222222222222222222"

    # EAPOL-Key replay counter does not increase
    send_eapol_key(dev[0], bssid, signkey,
                   "02030031" + "010005" + "0000000000000000" + "056c22d109f29d4d9fb9b9ccbad33283" + "02",
                   "1c636a30a4")

    # EAPOL-Key too large Key Length field value
    send_eapol_key(dev[0], bssid, signkey,
                   "02030031" + "010021" + "ffffffffffffffff" + "056c22d109f29d4d9fb9b9ccbad33283" + "02",
                   "1c636a30a4")

    # EAPOL-Key too much key data
    send_eapol_key(dev[0], bssid, signkey,
                   "0203004d" + "010005" + "ffffffffffffffff" + "056c22d109f29d4d9fb9b9ccbad33283" + "02",
                   33*"ff")

    # EAPOL-Key too little key data
    send_eapol_key(dev[0], bssid, signkey,
                   "02030030" + "010005" + "ffffffffffffffff" + "056c22d109f29d4d9fb9b9ccbad33283" + "02",
                   "1c636a30")

    # EAPOL-Key with no key data and too long WEP key length
    send_eapol_key(dev[0], bssid, signkey,
                   "0203002c" + "010020" + "ffffffffffffffff" + "056c22d109f29d4d9fb9b9ccbad33283" + "02",
                   "")

def test_ieee8021x_reauth(dev, apdev):
    """IEEE 802.1X and EAPOL_REAUTH request"""
    params = hostapd.radius_params()
    params["ssid"] = "ieee8021x-open"
    params["ieee8021x"] = "1"
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].connect("ieee8021x-open", key_mgmt="IEEE8021X", eapol_flags="0",
                   eap="PSK", identity="psk.user@example.com",
                   password_hex="0123456789abcdef0123456789abcdef",
                   scan_freq="2412")

    hapd.request("EAPOL_REAUTH " + dev[0].own_addr())
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED"], timeout=5)
    if ev is None:
        raise Exception("EAP authentication did not start")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-SUCCESS"], timeout=5)
    if ev is None:
        raise Exception("EAP authentication did not succeed")
    time.sleep(0.1)
    hwsim_utils.test_connectivity(dev[0], hapd)

def test_ieee8021x_reauth_wep(dev, apdev, params):
    """IEEE 802.1X and EAPOL_REAUTH request with WEP"""
    check_wep_capa(dev[0])
    logdir = params['logdir']

    params = hostapd.radius_params()
    params["ssid"] = "ieee8021x-open"
    params["ieee8021x"] = "1"
    params["wep_key_len_broadcast"] = "13"
    params["wep_key_len_unicast"] = "13"
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].connect("ieee8021x-open", key_mgmt="IEEE8021X",
                   eap="PSK", identity="psk.user@example.com",
                   password_hex="0123456789abcdef0123456789abcdef",
                   scan_freq="2412")
    hwsim_utils.test_connectivity(dev[0], hapd)

    hapd.request("EAPOL_REAUTH " + dev[0].own_addr())
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED"], timeout=5)
    if ev is None:
        raise Exception("EAP authentication did not start")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-SUCCESS"], timeout=5)
    if ev is None:
        raise Exception("EAP authentication did not succeed")
    time.sleep(0.1)
    hwsim_utils.test_connectivity(dev[0], hapd)

    out = run_tshark(os.path.join(logdir, "hwsim0.pcapng"),
                     "llc.type == 0x888e", ["eapol.type", "eap.code"])
    if out is None:
        raise Exception("Could not find EAPOL frames in capture")
    num_eapol_key = 0
    num_eap_req = 0
    num_eap_resp = 0
    for line in out.splitlines():
        vals = line.split()
        if vals[0] == '3':
            num_eapol_key += 1
        if vals[0] == '0' and len(vals) == 2:
            if vals[1] == '1':
                num_eap_req += 1
            elif vals[1] == '2':
                num_eap_resp += 1
    logger.info("num_eapol_key: %d" % num_eapol_key)
    logger.info("num_eap_req: %d" % num_eap_req)
    logger.info("num_eap_resp: %d" % num_eap_resp)
    if num_eapol_key < 4:
        raise Exception("Did not see four unencrypted EAPOL-Key frames")
    if num_eap_req < 6:
        raise Exception("Did not see six unencrypted EAP-Request frames")
    if num_eap_resp < 6:
        raise Exception("Did not see six unencrypted EAP-Response frames")

def test_ieee8021x_set_conf(dev, apdev):
    """IEEE 802.1X and EAPOL_SET command"""
    params = hostapd.radius_params()
    params["ssid"] = "ieee8021x-open"
    params["ieee8021x"] = "1"
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].connect("ieee8021x-open", key_mgmt="IEEE8021X", eapol_flags="0",
                   eap="PSK", identity="psk.user@example.com",
                   password_hex="0123456789abcdef0123456789abcdef",
                   scan_freq="2412")

    addr0 = dev[0].own_addr()
    tests = ["EAPOL_SET 1",
             "EAPOL_SET %sfoo bar" % addr0,
             "EAPOL_SET %s foo" % addr0,
             "EAPOL_SET %s foo bar" % addr0,
             "EAPOL_SET %s AdminControlledDirections bar" % addr0,
             "EAPOL_SET %s AdminControlledPortControl bar" % addr0,
             "EAPOL_SET %s reAuthEnabled bar" % addr0,
             "EAPOL_SET %s KeyTransmissionEnabled bar" % addr0,
             "EAPOL_SET 11:22:33:44:55:66 AdminControlledDirections Both"]
    for t in tests:
        if "FAIL" not in hapd.request(t):
            raise Exception("Invalid EAPOL_SET command accepted: " + t)

    tests = [("AdminControlledDirections", "adminControlledDirections", "In"),
             ("AdminControlledDirections", "adminControlledDirections",
              "Both"),
             ("quietPeriod", "quietPeriod", "13"),
             ("serverTimeout", "serverTimeout", "7"),
             ("reAuthPeriod", "reAuthPeriod", "1234"),
             ("reAuthEnabled", "reAuthEnabled", "FALSE"),
             ("reAuthEnabled", "reAuthEnabled", "TRUE"),
             ("KeyTransmissionEnabled", "keyTxEnabled", "TRUE"),
             ("KeyTransmissionEnabled", "keyTxEnabled", "FALSE"),
             ("AdminControlledPortControl", "portControl", "ForceAuthorized"),
             ("AdminControlledPortControl", "portControl",
              "ForceUnauthorized"),
             ("AdminControlledPortControl", "portControl", "Auto")]
    for param, mibparam, val in tests:
        if "OK" not in hapd.request("EAPOL_SET %s %s %s" % (addr0, param, val)):
            raise Exception("Failed to set %s %s" % (param, val))
        mib = hapd.get_sta(addr0, info="eapol")
        if mib[mibparam] != val:
            raise Exception("Unexpected %s value: %s (expected %s)" % (param, mib[mibparam], val))
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-SUCCESS"], timeout=5)
    if ev is None:
        raise Exception("EAP authentication did not succeed")
    time.sleep(0.1)
    hwsim_utils.test_connectivity(dev[0], hapd)

def test_ieee8021x_auth_awhile(dev, apdev):
    """IEEE 802.1X and EAPOL Authenticator aWhile handling"""
    params = hostapd.radius_params()
    params["ssid"] = "ieee8021x-open"
    params["ieee8021x"] = "1"
    params['auth_server_port'] = "18129"
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = apdev[0]['bssid']
    addr0 = dev[0].own_addr()

    params = {}
    params['ssid'] = 'as'
    params['beacon_int'] = '2000'
    params['radius_server_clients'] = 'auth_serv/radius_clients.conf'
    params['radius_server_auth_port'] = '18129'
    params['eap_server'] = '1'
    params['eap_user_file'] = 'auth_serv/eap_user.conf'
    params['ca_cert'] = 'auth_serv/ca.pem'
    params['server_cert'] = 'auth_serv/server.pem'
    params['private_key'] = 'auth_serv/server.key'
    hapd1 = hostapd.add_ap(apdev[1], params)

    dev[0].connect("ieee8021x-open", key_mgmt="IEEE8021X", eapol_flags="0",
                   eap="PSK", identity="psk.user@example.com",
                   password_hex="0123456789abcdef0123456789abcdef",
                   scan_freq="2412")
    hapd1.disable()
    if "OK" not in hapd.request("EAPOL_SET %s serverTimeout 1" % addr0):
        raise Exception("Failed to set serverTimeout")
    hapd.request("EAPOL_REAUTH " + dev[0].own_addr())
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED"], timeout=5)

    for i in range(40):
        mib = hapd.get_sta(addr0, info="eapol")
        val = int(mib['aWhile'])
        if val > 0:
            break
        time.sleep(1)
    if val == 0:
        raise Exception("aWhile did not increase")

    hapd.dump_monitor()
    for i in range(40):
        mib = hapd.get_sta(addr0, info="eapol")
        val = int(mib['aWhile'])
        if val < 5:
            break
        time.sleep(1)
    ev = hapd.wait_event(["CTRL-EVENT-EAP-PROPOSED"], timeout=10)
    if ev is None:
        raise Exception("Authentication restart not seen")

def test_ieee8021x_open_leap(dev, apdev):
    """IEEE 802.1X connection with LEAP included in configuration"""
    params = hostapd.radius_params()
    params["ssid"] = "ieee8021x-open"
    params["ieee8021x"] = "1"
    hapd = hostapd.add_ap(apdev[0], params)

    dev[1].connect("ieee8021x-open", key_mgmt="IEEE8021X", eapol_flags="0",
                   eap="LEAP", identity="psk.user@example.com",
                   password_hex="0123456789abcdef0123456789abcdef",
                   scan_freq="2412", wait_connect=False)
    dev[0].connect("ieee8021x-open", key_mgmt="IEEE8021X", eapol_flags="0",
                   eap="PSK LEAP", identity="psk.user@example.com",
                   password_hex="0123456789abcdef0123456789abcdef",
                   scan_freq="2412")
    ev = dev[1].wait_event(["CTRL-EVENT-AUTH-REJECT"], timeout=5)
    dev[1].request("DISCONNECT")

def test_ieee8021x_and_wpa_enabled(dev, apdev):
    """IEEE 802.1X connection using dynamic WEP104 when WPA enabled"""
    check_wep_capa(dev[0])
    skip_with_fips(dev[0])
    params = hostapd.radius_params()
    params["ssid"] = "ieee8021x-wep"
    params["ieee8021x"] = "1"
    params["wep_key_len_broadcast"] = "13"
    params["wep_key_len_unicast"] = "13"
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].connect("ieee8021x-wep", key_mgmt="IEEE8021X WPA-EAP", eap="PSK",
                   identity="psk.user@example.com",
                   password_hex="0123456789abcdef0123456789abcdef",
                   scan_freq="2412")
    hwsim_utils.test_connectivity(dev[0], hapd)
