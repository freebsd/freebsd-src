# Test cases for sigma_dut
# Copyright (c) 2017, Qualcomm Atheros, Inc.
# Copyright (c) 2018-2019, The Linux Foundation
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

import binascii
import errno
import fcntl
import hashlib
import logging
logger = logging.getLogger()
import os
import socket
import struct
import subprocess
import threading
import time

import hostapd
from utils import *
from hwsim import HWSimRadio
import hwsim_utils
from wlantest import Wlantest
from tshark import run_tshark
from test_dpp import check_dpp_capab, update_hapd_config, wait_auth_success
from test_suite_b import check_suite_b_192_capa, suite_b_as_params, suite_b_192_rsa_ap_params
from test_ap_eap import check_eap_capa, int_eap_server_params, check_domain_match, check_domain_suffix_match
from test_ap_hs20 import hs20_ap_params
from test_ap_pmf import check_mac80211_bigtk
from test_ocv import check_ocv_failure

def check_sigma_dut():
    if not os.path.exists("./sigma_dut"):
        raise HwsimSkip("sigma_dut not available")

def to_hex(s):
    return binascii.hexlify(s.encode()).decode()

def from_hex(s):
    return binascii.unhexlify(s).decode()

def sigma_log_output(cmd):
    try:
        out = cmd.stdout.read()
        if out:
            logger.debug("sigma_dut stdout: " + str(out.decode()))
    except IOError as e:
        if e.errno != errno.EAGAIN:
            raise
    try:
        out = cmd.stderr.read()
        if out:
            logger.debug("sigma_dut stderr: " + str(out.decode()))
    except IOError as e:
        if e.errno != errno.EAGAIN:
            raise

sigma_prog = None

def sigma_dut_cmd(cmd, port=9000, timeout=2):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM,
                         socket.IPPROTO_TCP)
    sock.settimeout(timeout)
    addr = ('127.0.0.1', port)
    sock.connect(addr)
    sock.send(cmd.encode() + b"\r\n")
    try:
        res = sock.recv(1000).decode()
        running = False
        done = False
        for line in res.splitlines():
            if line.startswith("status,RUNNING"):
                running = True
            elif line.startswith("status,INVALID"):
                done = True
            elif line.startswith("status,ERROR"):
                done = True
            elif line.startswith("status,COMPLETE"):
                done = True
        if running and not done:
            # Read the actual response
            res = sock.recv(1000).decode()
    except:
        res = ''
        pass
    sock.close()
    res = res.rstrip()
    logger.debug("sigma_dut: '%s' --> '%s'" % (cmd, res))
    global sigma_prog
    if sigma_prog:
        sigma_log_output(sigma_prog)
    return res

def sigma_dut_cmd_check(cmd, port=9000, timeout=2):
    res = sigma_dut_cmd(cmd, port=port, timeout=timeout)
    if "COMPLETE" not in res:
        raise Exception("sigma_dut command failed: " + cmd)
    return res

def start_sigma_dut(ifname, hostapd_logdir=None, cert_path=None,
                    bridge=None, sae_h2e=False, owe_ptk_workaround=False):
    check_sigma_dut()
    cmd = ['./sigma_dut',
           '-d',
           '-M', ifname,
           '-S', ifname,
           '-F', '../../hostapd/hostapd',
           '-G',
           '-w', '/var/run/wpa_supplicant/',
           '-j', ifname]
    if hostapd_logdir:
        cmd += ['-H', hostapd_logdir]
    if cert_path:
        cmd += ['-C', cert_path]
    if bridge:
        cmd += ['-b', bridge]
    if sae_h2e:
        cmd += ['-2']
    if owe_ptk_workaround:
        cmd += ['-3']
    sigma = subprocess.Popen(cmd, stdout=subprocess.PIPE,
                             stderr=subprocess.PIPE)
    for stream in [sigma.stdout, sigma.stderr]:
        fd = stream.fileno()
        fl = fcntl.fcntl(fd, fcntl.F_GETFL)
        fcntl.fcntl(fd, fcntl.F_SETFL, fl | os.O_NONBLOCK)

    global sigma_prog
    sigma_prog = sigma
    res = None
    for i in range(20):
        try:
            res = sigma_dut_cmd("HELLO")
            break
        except:
            time.sleep(0.05)
    if res is None or "errorCode,Unknown command" not in res:
        raise Exception("Failed to start sigma_dut")
    return {'cmd': sigma, 'ifname': ifname}

def stop_sigma_dut(sigma):
    global sigma_prog
    sigma_prog = None
    cmd = sigma['cmd']
    sigma_log_output(cmd)
    logger.debug("Terminating sigma_dut process")
    cmd.terminate()
    cmd.wait()
    out, err = cmd.communicate()
    logger.debug("sigma_dut stdout: " + str(out.decode()))
    logger.debug("sigma_dut stderr: " + str(err.decode()))
    subprocess.call(["ip", "addr", "del", "dev", sigma['ifname'],
                     "127.0.0.11/24"],
                    stderr=open('/dev/null', 'w'))

def sigma_dut_wait_connected(ifname):
    for i in range(50):
        res = sigma_dut_cmd("sta_is_connected,interface," + ifname)
        if "connected,1" in res:
            break
        time.sleep(0.2)
        if i == 49:
            raise Exception("Connection did not complete")

def test_sigma_dut_basic(dev, apdev):
    """sigma_dut basic functionality"""
    sigma = start_sigma_dut(dev[0].ifname)

    tests = [("ca_get_version", "status,COMPLETE,version,1.0"),
             ("device_get_info", "status,COMPLETE,vendor"),
             ("device_list_interfaces,interfaceType,foo", "status,ERROR"),
             ("device_list_interfaces,interfaceType,802.11",
              "status,COMPLETE,interfaceType,802.11,interfaceID," + dev[0].ifname)]
    try:
        res = sigma_dut_cmd("UNKNOWN")
        if "status,INVALID,errorCode,Unknown command" not in res:
            raise Exception("Unexpected sigma_dut response to unknown command")

        for cmd, response in tests:
            res = sigma_dut_cmd(cmd)
            if response not in res:
                raise Exception("Unexpected %s response: %s" % (cmd, res))
    finally:
        stop_sigma_dut(sigma)

@reset_ignore_old_scan_res
def test_sigma_dut_open(dev, apdev):
    """sigma_dut controlled open network association"""
    ifname = dev[0].ifname
    sigma = start_sigma_dut(ifname)

    try:
        hapd = hostapd.add_ap(apdev[0], {"ssid": "open"})

        sigma_dut_cmd_check("sta_set_ip_config,interface,%s,dhcp,0,ip,127.0.0.11,mask,255.255.255.0" % ifname)
        sigma_dut_cmd_check("sta_set_encryption,interface,%s,ssid,%s,encpType,none" % (ifname, "open"))
        sigma_dut_cmd_check("sta_associate,interface,%s,ssid,%s" % (ifname, "open"),
                            timeout=10)
        sigma_dut_wait_connected(ifname)
        sigma_dut_cmd_check("sta_get_ip_config,interface," + ifname)
        sigma_dut_cmd_check("sta_disconnect,interface," + ifname)
        sigma_dut_cmd_check("sta_reset_default,interface," + ifname)
    finally:
        stop_sigma_dut(sigma)

@reset_ignore_old_scan_res
def test_sigma_dut_psk_pmf(dev, apdev):
    """sigma_dut controlled PSK+PMF association"""
    ifname = dev[0].ifname
    sigma = start_sigma_dut(ifname)

    try:
        ssid = "test-pmf-required"
        params = hostapd.wpa2_params(ssid=ssid, passphrase="12345678")
        params["wpa_key_mgmt"] = "WPA-PSK-SHA256"
        params["ieee80211w"] = "2"
        hapd = hostapd.add_ap(apdev[0], params)

        sigma_dut_cmd_check("sta_reset_default,interface,%s,prog,PMF" % ifname)
        sigma_dut_cmd_check("sta_set_ip_config,interface,%s,dhcp,0,ip,127.0.0.11,mask,255.255.255.0" % ifname)
        sigma_dut_cmd_check("sta_set_psk,interface,%s,ssid,%s,passphrase,%s,encpType,aes-ccmp,keymgmttype,wpa2,PMF,Required" % (ifname, "test-pmf-required", "12345678"))
        sigma_dut_cmd_check("sta_associate,interface,%s,ssid,%s,channel,1" % (ifname, "test-pmf-required"),
                            timeout=10)
        sigma_dut_wait_connected(ifname)
        sigma_dut_cmd_check("sta_get_ip_config,interface," + ifname)
        sigma_dut_cmd_check("sta_disconnect,interface," + ifname)
        sigma_dut_cmd_check("sta_reset_default,interface," + ifname)
    finally:
        stop_sigma_dut(sigma)

@reset_ignore_old_scan_res
def test_sigma_dut_psk_pmf_bip_cmac_128(dev, apdev):
    """sigma_dut controlled PSK+PMF association with BIP-CMAC-128"""
    run_sigma_dut_psk_pmf_cipher(dev, apdev, "BIP-CMAC-128", "AES-128-CMAC")

@reset_ignore_old_scan_res
def test_sigma_dut_psk_pmf_bip_cmac_256(dev, apdev):
    """sigma_dut controlled PSK+PMF association with BIP-CMAC-256"""
    run_sigma_dut_psk_pmf_cipher(dev, apdev, "BIP-CMAC-256", "BIP-CMAC-256")

@reset_ignore_old_scan_res
def test_sigma_dut_psk_pmf_bip_gmac_128(dev, apdev):
    """sigma_dut controlled PSK+PMF association with BIP-GMAC-128"""
    run_sigma_dut_psk_pmf_cipher(dev, apdev, "BIP-GMAC-128", "BIP-GMAC-128")

@reset_ignore_old_scan_res
def test_sigma_dut_psk_pmf_bip_gmac_256(dev, apdev):
    """sigma_dut controlled PSK+PMF association with BIP-GMAC-256"""
    run_sigma_dut_psk_pmf_cipher(dev, apdev, "BIP-GMAC-256", "BIP-GMAC-256")

@reset_ignore_old_scan_res
def test_sigma_dut_psk_pmf_bip_gmac_256_mismatch(dev, apdev):
    """sigma_dut controlled PSK+PMF association with BIP-GMAC-256 mismatch"""
    run_sigma_dut_psk_pmf_cipher(dev, apdev, "BIP-GMAC-256", "AES-128-CMAC",
                                 failure=True)

def run_sigma_dut_psk_pmf_cipher(dev, apdev, sigma_cipher, hostapd_cipher,
                                 failure=False):
    ifname = dev[0].ifname
    sigma = start_sigma_dut(ifname)

    try:
        ssid = "test-pmf-required"
        params = hostapd.wpa2_params(ssid=ssid, passphrase="12345678")
        params["wpa_key_mgmt"] = "WPA-PSK-SHA256"
        params["ieee80211w"] = "2"
        params["group_mgmt_cipher"] = hostapd_cipher
        hapd = hostapd.add_ap(apdev[0], params)

        sigma_dut_cmd_check("sta_reset_default,interface,%s,prog,PMF" % ifname)
        sigma_dut_cmd_check("sta_set_ip_config,interface,%s,dhcp,0,ip,127.0.0.11,mask,255.255.255.0" % ifname)
        sigma_dut_cmd_check("sta_set_psk,interface,%s,ssid,%s,passphrase,%s,encpType,aes-ccmp,keymgmttype,wpa2,PMF,Required,GroupMgntCipher,%s" % (ifname, "test-pmf-required", "12345678", sigma_cipher))
        sigma_dut_cmd_check("sta_associate,interface,%s,ssid,%s,channel,1" % (ifname, "test-pmf-required"),
                            timeout=2 if failure else 10)
        if failure:
            ev = dev[0].wait_event(["CTRL-EVENT-NETWORK-NOT-FOUND",
                                    "CTRL-EVENT-CONNECTED"], timeout=10)
            if ev is None:
                raise Exception("Network selection result not indicated")
            if "CTRL-EVENT-CONNECTED" in ev:
                raise Exception("Unexpected connection")
            res = sigma_dut_cmd("sta_is_connected,interface," + ifname)
            if "connected,1" in res:
                raise Exception("Connection reported")
        else:
            sigma_dut_wait_connected(ifname)
            sigma_dut_cmd_check("sta_get_ip_config,interface," + ifname)

        sigma_dut_cmd_check("sta_disconnect,interface," + ifname)
        sigma_dut_cmd_check("sta_reset_default,interface," + ifname)
    finally:
        stop_sigma_dut(sigma)

def test_sigma_dut_sae(dev, apdev):
    """sigma_dut controlled SAE association"""
    if "SAE" not in dev[0].get_capability("auth_alg"):
        raise HwsimSkip("SAE not supported")

    ifname = dev[0].ifname
    sigma = start_sigma_dut(ifname)

    try:
        ssid = "test-sae"
        params = hostapd.wpa2_params(ssid=ssid, passphrase="12345678")
        params['wpa_key_mgmt'] = 'SAE'
        params["ieee80211w"] = "2"
        params['sae_groups'] = '19 20 21'
        hapd = hostapd.add_ap(apdev[0], params)

        sigma_dut_cmd_check("sta_reset_default,interface,%s" % ifname)
        sigma_dut_cmd_check("sta_set_ip_config,interface,%s,dhcp,0,ip,127.0.0.11,mask,255.255.255.0" % ifname)
        sigma_dut_cmd_check("sta_set_security,interface,%s,ssid,%s,passphrase,%s,type,SAE,encpType,aes-ccmp,keymgmttype,wpa2" % (ifname, "test-sae", "12345678"))
        sigma_dut_cmd_check("sta_associate,interface,%s,ssid,%s,channel,1" % (ifname, "test-sae"),
                            timeout=10)
        sigma_dut_wait_connected(ifname)
        sigma_dut_cmd_check("sta_get_ip_config,interface," + ifname)
        if dev[0].get_status_field('sae_group') != '19':
            raise Exception("Expected default SAE group not used")
        res = sigma_dut_cmd_check("sta_get_parameter,interface,%s,Parameter,PMK" % ifname)
        logger.info("Reported PMK: " + res)
        if ",PMK," not in res:
            raise Exception("PMK not reported");
        if hapd.request("GET_PMK " + dev[0].own_addr()) != res.split(',')[3]:
            raise Exception("Mismatch in reported PMK")
        sigma_dut_cmd_check("sta_disconnect,interface," + ifname)

        sigma_dut_cmd_check("sta_reset_default,interface," + ifname)

        sigma_dut_cmd_check("sta_set_ip_config,interface,%s,dhcp,0,ip,127.0.0.11,mask,255.255.255.0" % ifname)
        sigma_dut_cmd_check("sta_set_security,interface,%s,ssid,%s,passphrase,%s,type,SAE,encpType,aes-ccmp,keymgmttype,wpa2,ECGroupID,20" % (ifname, "test-sae", "12345678"))
        sigma_dut_cmd_check("sta_associate,interface,%s,ssid,%s,channel,1" % (ifname, "test-sae"),
                            timeout=10)
        sigma_dut_wait_connected(ifname)
        sigma_dut_cmd_check("sta_get_ip_config,interface," + ifname)
        if dev[0].get_status_field('sae_group') != '20':
            raise Exception("Expected SAE group not used")
        sigma_dut_cmd_check("sta_disconnect,interface," + ifname)
        sigma_dut_cmd_check("sta_reset_default,interface," + ifname)
    finally:
        stop_sigma_dut(sigma)

def test_sigma_dut_sae_groups(dev, apdev):
    """sigma_dut controlled SAE association with group negotiation"""
    if "SAE" not in dev[0].get_capability("auth_alg"):
        raise HwsimSkip("SAE not supported")

    ifname = dev[0].ifname
    sigma = start_sigma_dut(ifname)

    try:
        ssid = "test-sae"
        params = hostapd.wpa2_params(ssid=ssid, passphrase="12345678")
        params['wpa_key_mgmt'] = 'SAE'
        params["ieee80211w"] = "2"
        params['sae_groups'] = '19'
        hapd = hostapd.add_ap(apdev[0], params)

        sigma_dut_cmd_check("sta_reset_default,interface,%s" % ifname)
        sigma_dut_cmd_check("sta_set_ip_config,interface,%s,dhcp,0,ip,127.0.0.11,mask,255.255.255.0" % ifname)
        sigma_dut_cmd_check("sta_set_security,interface,%s,ssid,%s,passphrase,%s,type,SAE,encpType,aes-ccmp,keymgmttype,wpa2,ECGroupID,21 20 19" % (ifname, "test-sae", "12345678"))
        sigma_dut_cmd_check("sta_associate,interface,%s,ssid,%s,channel,1" % (ifname, "test-sae"),
                            timeout=10)
        sigma_dut_wait_connected(ifname)
        sigma_dut_cmd_check("sta_get_ip_config,interface," + ifname)
        if dev[0].get_status_field('sae_group') != '19':
            raise Exception("Expected default SAE group not used")
        sigma_dut_cmd_check("sta_disconnect,interface," + ifname)

        sigma_dut_cmd_check("sta_reset_default,interface," + ifname)
    finally:
        stop_sigma_dut(sigma)

def test_sigma_dut_sae_pmkid_include(dev, apdev):
    """sigma_dut controlled SAE association with PMKID"""
    if "SAE" not in dev[0].get_capability("auth_alg"):
        raise HwsimSkip("SAE not supported")

    ifname = dev[0].ifname
    sigma = start_sigma_dut(ifname)

    try:
        ssid = "test-sae"
        params = hostapd.wpa2_params(ssid=ssid, passphrase="12345678")
        params['wpa_key_mgmt'] = 'SAE'
        params["ieee80211w"] = "2"
        params["sae_confirm_immediate"] = "1"
        hapd = hostapd.add_ap(apdev[0], params)

        sigma_dut_cmd_check("sta_reset_default,interface,%s" % ifname)
        sigma_dut_cmd_check("sta_set_ip_config,interface,%s,dhcp,0,ip,127.0.0.11,mask,255.255.255.0" % ifname)
        sigma_dut_cmd_check("sta_set_security,interface,%s,ssid,%s,passphrase,%s,type,SAE,encpType,aes-ccmp,keymgmttype,wpa2,PMKID_Include,enable" % (ifname, "test-sae", "12345678"))
        sigma_dut_cmd_check("sta_associate,interface,%s,ssid,%s,channel,1" % (ifname, "test-sae"),
                            timeout=10)
        sigma_dut_wait_connected(ifname)
        sigma_dut_cmd_check("sta_reset_default,interface," + ifname)
    finally:
        stop_sigma_dut(sigma)

def test_sigma_dut_sae_password(dev, apdev):
    """sigma_dut controlled SAE association and long password"""
    if "SAE" not in dev[0].get_capability("auth_alg"):
        raise HwsimSkip("SAE not supported")

    ifname = dev[0].ifname
    sigma = start_sigma_dut(ifname)

    try:
        ssid = "test-sae"
        params = hostapd.wpa2_params(ssid=ssid)
        params['sae_password'] = 100*'B'
        params['wpa_key_mgmt'] = 'SAE'
        params["ieee80211w"] = "2"
        hapd = hostapd.add_ap(apdev[0], params)

        sigma_dut_cmd_check("sta_reset_default,interface,%s" % ifname)
        sigma_dut_cmd_check("sta_set_ip_config,interface,%s,dhcp,0,ip,127.0.0.11,mask,255.255.255.0" % ifname)
        sigma_dut_cmd_check("sta_set_security,interface,%s,ssid,%s,passphrase,%s,type,SAE,encpType,aes-ccmp,keymgmttype,wpa2" % (ifname, "test-sae", 100*'B'))
        sigma_dut_cmd_check("sta_associate,interface,%s,ssid,%s,channel,1" % (ifname, "test-sae"),
                            timeout=10)
        sigma_dut_wait_connected(ifname)
        sigma_dut_cmd_check("sta_get_ip_config,interface," + ifname)
        sigma_dut_cmd_check("sta_disconnect,interface," + ifname)
        sigma_dut_cmd_check("sta_reset_default,interface," + ifname)
    finally:
        stop_sigma_dut(sigma)

def test_sigma_dut_sae_pw_id(dev, apdev):
    """sigma_dut controlled SAE association with Password Identifier"""
    if "SAE" not in dev[0].get_capability("auth_alg"):
        raise HwsimSkip("SAE not supported")

    ifname = dev[0].ifname
    sigma = start_sigma_dut(ifname)

    try:
        ssid = "test-sae"
        params = hostapd.wpa2_params(ssid=ssid)
        params['wpa_key_mgmt'] = 'SAE'
        params["ieee80211w"] = "2"
        params['sae_password'] = 'secret|id=pw id'
        params['sae_groups'] = '19'
        hapd = hostapd.add_ap(apdev[0], params)

        sigma_dut_cmd_check("sta_reset_default,interface,%s" % ifname)
        sigma_dut_cmd_check("sta_set_ip_config,interface,%s,dhcp,0,ip,127.0.0.11,mask,255.255.255.0" % ifname)
        sigma_dut_cmd_check("sta_set_security,interface,%s,ssid,%s,passphrase,%s,type,SAE,encpType,aes-ccmp,AKMSuiteType,8;9,PasswordID,pw id" % (ifname, "test-sae", "secret"))
        sigma_dut_cmd_check("sta_associate,interface,%s,ssid,%s,channel,1" % (ifname, "test-sae"),
                            timeout=10)
        sigma_dut_wait_connected(ifname)
        sigma_dut_cmd_check("sta_disconnect,interface," + ifname)
        sigma_dut_cmd_check("sta_reset_default,interface," + ifname)
    finally:
        stop_sigma_dut(sigma)

def test_sigma_dut_sae_pw_id_pwe_loop(dev, apdev):
    """sigma_dut controlled SAE association with Password Identifier and forced PWE looping"""
    if "SAE" not in dev[0].get_capability("auth_alg"):
        raise HwsimSkip("SAE not supported")

    ifname = dev[0].ifname
    sigma = start_sigma_dut(ifname)

    try:
        ssid = "test-sae"
        params = hostapd.wpa2_params(ssid=ssid)
        params['wpa_key_mgmt'] = 'SAE'
        params["ieee80211w"] = "2"
        params['sae_password'] = 'secret|id=pw id'
        params['sae_groups'] = '19'
        hapd = hostapd.add_ap(apdev[0], params)

        sigma_dut_cmd_check("sta_reset_default,interface,%s" % ifname)
        sigma_dut_cmd_check("sta_set_ip_config,interface,%s,dhcp,0,ip,127.0.0.11,mask,255.255.255.0" % ifname)
        sigma_dut_cmd_check("sta_set_security,interface,%s,ssid,%s,passphrase,%s,type,SAE,encpType,aes-ccmp,AKMSuiteType,8;9,PasswordID,pw id,sae_pwe,looping" % (ifname, "test-sae", "secret"))
        sigma_dut_cmd_check("sta_associate,interface,%s,ssid,%s,channel,1" % (ifname, "test-sae"),
                            timeout=10)
        for i in range(3):
            ev = dev[0].wait_event(["SME: Trying to authenticate",
                                    "CTRL-EVENT-CONNECTED"], timeout=10)
            if ev is None:
                raise Exception("Network selection result not indicated")
            if "CTRL-EVENT-CONNECTED" in ev:
                raise Exception("Unexpected connection")
        res = sigma_dut_cmd("sta_is_connected,interface," + ifname)
        if "connected,1" in res:
            raise Exception("Connection reported")
        sigma_dut_cmd_check("sta_reset_default,interface," + ifname)
    finally:
        stop_sigma_dut(sigma)
        dev[0].set("sae_pwe", "0")

def test_sigma_dut_sae_pw_id_ft(dev, apdev):
    """sigma_dut controlled SAE association with Password Identifier and FT"""
    run_sigma_dut_sae_pw_id_ft(dev, apdev)

def test_sigma_dut_sae_pw_id_ft_over_ds(dev, apdev):
    """sigma_dut controlled SAE association with Password Identifier and FT-over-DS"""
    run_sigma_dut_sae_pw_id_ft(dev, apdev, over_ds=True)

def run_sigma_dut_sae_pw_id_ft(dev, apdev, over_ds=False):
    if "SAE" not in dev[0].get_capability("auth_alg"):
        raise HwsimSkip("SAE not supported")

    ifname = dev[0].ifname
    sigma = start_sigma_dut(ifname)

    try:
        ssid = "test-sae"
        params = hostapd.wpa2_params(ssid=ssid)
        params['wpa_key_mgmt'] = 'SAE FT-SAE'
        params["ieee80211w"] = "2"
        params['sae_password'] = ['pw1|id=id1', 'pw2|id=id2', 'pw3', 'pw4|id=id4']
        params['mobility_domain'] = 'aabb'
        params['ft_over_ds'] = '1' if over_ds else '0'
        bssid = apdev[0]['bssid'].replace(':', '')
        params['nas_identifier'] = bssid + '.nas.example.com'
        params['r1_key_holder'] = bssid
        params['pmk_r1_push'] = '0'
        params['r0kh'] = 'ff:ff:ff:ff:ff:ff * 00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff'
        params['r1kh'] = '00:00:00:00:00:00 00:00:00:00:00:00 00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff'
        hapd = hostapd.add_ap(apdev[0], params)

        sigma_dut_cmd_check("sta_reset_default,interface,%s" % ifname)
        if over_ds:
            sigma_dut_cmd_check("sta_preset_testparameters,interface,%s,FT_DS,Enable" % ifname)
        sigma_dut_cmd_check("sta_set_ip_config,interface,%s,dhcp,0,ip,127.0.0.11,mask,255.255.255.0" % ifname)
        sigma_dut_cmd_check("sta_set_security,interface,%s,ssid,%s,passphrase,%s,type,SAE,encpType,aes-ccmp,AKMSuiteType,8;9,PasswordID,id2" % (ifname, "test-sae", "pw2"))
        sigma_dut_cmd_check("sta_associate,interface,%s,ssid,%s,channel,1" % (ifname, "test-sae"),
                            timeout=10)
        sigma_dut_wait_connected(ifname)

        bssid = apdev[1]['bssid'].replace(':', '')
        params['nas_identifier'] = bssid + '.nas.example.com'
        params['r1_key_holder'] = bssid
        hapd2 = hostapd.add_ap(apdev[1], params)
        bssid = hapd2.own_addr()
        sigma_dut_cmd_check("sta_reassoc,interface,%s,Channel,1,bssid,%s" % (ifname, bssid))
        dev[0].wait_connected()

        sigma_dut_cmd_check("sta_disconnect,interface," + ifname)
        sigma_dut_cmd_check("sta_reset_default,interface," + ifname)
    finally:
        stop_sigma_dut(sigma)

@reset_ignore_old_scan_res
def test_sigma_dut_sta_override_rsne(dev, apdev):
    """sigma_dut and RSNE override on STA"""
    ifname = dev[0].ifname
    sigma = start_sigma_dut(ifname)

    try:
        ssid = "test-psk"
        params = hostapd.wpa2_params(ssid=ssid, passphrase="12345678")
        hapd = hostapd.add_ap(apdev[0], params)

        sigma_dut_cmd_check("sta_set_ip_config,interface,%s,dhcp,0,ip,127.0.0.11,mask,255.255.255.0" % ifname)

        tests = ["30120100000fac040100000fac040100000fac02",
                 "30140100000fac040100000fac040100000fac02ffff"]
        for test in tests:
            sigma_dut_cmd_check("sta_set_security,interface,%s,ssid,%s,type,PSK,passphrase,%s,EncpType,aes-ccmp,KeyMgmtType,wpa2" % (ifname, "test-psk", "12345678"))
            sigma_dut_cmd_check("dev_configure_ie,interface,%s,IE_Name,RSNE,Contents,%s" % (ifname, test))
            sigma_dut_cmd_check("sta_associate,interface,%s,ssid,%s,channel,1" % (ifname, "test-psk"),
                                timeout=10)
            sigma_dut_wait_connected(ifname)
            sigma_dut_cmd_check("sta_disconnect,interface," + ifname)
            dev[0].dump_monitor()

        sigma_dut_cmd_check("sta_set_security,interface,%s,ssid,%s,type,PSK,passphrase,%s,EncpType,aes-ccmp,KeyMgmtType,wpa2" % (ifname, "test-psk", "12345678"))
        sigma_dut_cmd_check("dev_configure_ie,interface,%s,IE_Name,RSNE,Contents,300101" % ifname)
        sigma_dut_cmd_check("sta_associate,interface,%s,ssid,%s,channel,1" % (ifname, "test-psk"),
                            timeout=10)

        ev = dev[0].wait_event(["CTRL-EVENT-ASSOC-REJECT"])
        if ev is None:
            raise Exception("Association rejection not reported")
        if "status_code=40" not in ev:
            raise Exception("Unexpected status code: " + ev)

        sigma_dut_cmd_check("sta_reset_default,interface," + ifname)
    finally:
        stop_sigma_dut(sigma)

def test_sigma_dut_ap_psk(dev, apdev):
    """sigma_dut controlled AP"""
    with HWSimRadio() as (radio, iface):
        sigma = start_sigma_dut(iface)
        try:
            sigma_dut_cmd_check("ap_reset_default")
            sigma_dut_cmd_check("ap_set_wireless,NAME,AP,CHANNEL,1,SSID,test-psk,MODE,11ng")
            sigma_dut_cmd_check("ap_set_security,NAME,AP,KEYMGNT,WPA2-PSK,PSK,12345678")
            sigma_dut_cmd_check("ap_config_commit,NAME,AP")

            dev[0].connect("test-psk", psk="12345678", scan_freq="2412")

            sigma_dut_cmd_check("ap_reset_default")
        finally:
            stop_sigma_dut(sigma)

def test_sigma_dut_ap_pskhex(dev, apdev, params):
    """sigma_dut controlled AP and PSKHEX"""
    logdir = os.path.join(params['logdir'],
                          "sigma_dut_ap_pskhex.sigma-hostapd")
    with HWSimRadio() as (radio, iface):
        sigma = start_sigma_dut(iface, hostapd_logdir=logdir)
        try:
            psk = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
            sigma_dut_cmd_check("ap_reset_default")
            sigma_dut_cmd_check("ap_set_wireless,NAME,AP,CHANNEL,1,SSID,test-psk,MODE,11ng")
            sigma_dut_cmd_check("ap_set_security,NAME,AP,KEYMGNT,WPA2-PSK,PSKHEX," + psk)
            sigma_dut_cmd_check("ap_config_commit,NAME,AP")

            dev[0].connect("test-psk", raw_psk=psk, scan_freq="2412")

            sigma_dut_cmd_check("ap_reset_default")
        finally:
            stop_sigma_dut(sigma)

def test_sigma_dut_ap_psk_sha256(dev, apdev, params):
    """sigma_dut controlled AP PSK SHA256"""
    logdir = os.path.join(params['logdir'],
                          "sigma_dut_ap_psk_sha256.sigma-hostapd")
    with HWSimRadio() as (radio, iface):
        sigma = start_sigma_dut(iface, hostapd_logdir=logdir)
        try:
            sigma_dut_cmd_check("ap_reset_default")
            sigma_dut_cmd_check("ap_set_wireless,NAME,AP,CHANNEL,1,SSID,test-psk,MODE,11ng")
            sigma_dut_cmd_check("ap_set_security,NAME,AP,KEYMGNT,WPA2-PSK-256,PSK,12345678")
            sigma_dut_cmd_check("ap_config_commit,NAME,AP")

            dev[0].connect("test-psk", key_mgmt="WPA-PSK-SHA256",
                           psk="12345678", scan_freq="2412")

            sigma_dut_cmd_check("ap_reset_default")
        finally:
            stop_sigma_dut(sigma)

def test_sigma_dut_ap_psk_deauth(dev, apdev, params):
    """sigma_dut controlled AP and deauth commands"""
    logdir = os.path.join(params['logdir'],
                          "sigma_dut_ap_psk_deauth.sigma-hostapd")
    with HWSimRadio() as (radio, iface):
        sigma = start_sigma_dut(iface, hostapd_logdir=logdir)
        try:
            sigma_dut_cmd_check("ap_reset_default")
            sigma_dut_cmd_check("ap_set_wireless,NAME,AP,CHANNEL,1,SSID,test-psk,MODE,11ng")
            sigma_dut_cmd_check("ap_set_security,NAME,AP,KEYMGNT,WPA2-PSK,PSK,12345678,PMF,Required")
            sigma_dut_cmd_check("ap_config_commit,NAME,AP")

            dev[0].connect("test-psk", key_mgmt="WPA-PSK-SHA256",
                           psk="12345678", ieee80211w="2", scan_freq="2412")
            addr = dev[0].own_addr()
            dev[0].dump_monitor()

            sigma_dut_cmd_check("ap_deauth_sta,NAME,AP,sta_mac_address," + addr)
            ev = dev[0].wait_disconnected()
            dev[0].dump_monitor()
            if "locally_generated=1" in ev:
                raise Exception("Unexpected disconnection reason")
            dev[0].wait_connected()
            dev[0].dump_monitor()

            sigma_dut_cmd_check("ap_deauth_sta,NAME,AP,sta_mac_address," + addr + ",disconnect,silent")
            ev = dev[0].wait_event(["CTRL-EVENT-DISCONNECTED"], timeout=5)
            if ev and "locally_generated=1" not in ev:
                raise Exception("Unexpected disconnection")

            sigma_dut_cmd_check("ap_reset_default")
        finally:
            stop_sigma_dut(sigma)

def test_sigma_dut_eap_ttls(dev, apdev, params):
    """sigma_dut controlled STA and EAP-TTLS parameters"""
    check_domain_match(dev[0])
    logdir = params['logdir']

    with open("auth_serv/ca.pem", "r") as f:
        with open(os.path.join(logdir, "sigma_dut_eap_ttls.ca.pem"), "w") as f2:
            f2.write(f.read())

    src = "auth_serv/server.pem"
    dst = os.path.join(logdir, "sigma_dut_eap_ttls.server.der")
    hashdst = os.path.join(logdir, "sigma_dut_eap_ttls.server.pem.sha256")
    subprocess.check_call(["openssl", "x509", "-in", src, "-out", dst,
                           "-outform", "DER"],
                          stderr=open('/dev/null', 'w'))
    with open(dst, "rb") as f:
        der = f.read()
    hash = hashlib.sha256(der).digest()
    with open(hashdst, "w") as f:
        f.write(binascii.hexlify(hash).decode())

    dst = os.path.join(logdir, "sigma_dut_eap_ttls.incorrect.pem.sha256")
    with open(dst, "w") as f:
        f.write(32*"00")

    ssid = "test-wpa2-eap"
    params = hostapd.wpa2_eap_params(ssid=ssid)
    hapd = hostapd.add_ap(apdev[0], params)

    ifname = dev[0].ifname
    sigma = start_sigma_dut(ifname, cert_path=logdir)

    cmd = "sta_set_security,type,eapttls,interface,%s,ssid,%s,keymgmttype,wpa2,encType,AES-CCMP,PairwiseCipher,AES-CCMP-128,trustedRootCA,sigma_dut_eap_ttls.ca.pem,username,DOMAIN\mschapv2 user,password,password" % (ifname, ssid)

    try:
        tests = ["",
                 ",Domain,server.w1.fi",
                 ",DomainSuffix,w1.fi",
                 ",DomainSuffix,server.w1.fi",
                 ",ServerCert,sigma_dut_eap_ttls.server.pem"]
        for extra in tests:
            sigma_dut_cmd_check("sta_reset_default,interface,%s,prog,WPA3" % ifname)
            sigma_dut_cmd_check("sta_set_ip_config,interface,%s,dhcp,0,ip,127.0.0.11,mask,255.255.255.0" % ifname)
            sigma_dut_cmd_check(cmd + extra)
            sigma_dut_cmd_check("sta_associate,interface,%s,ssid,%s,channel,1" % (ifname, ssid),
                                timeout=10)
            sigma_dut_wait_connected(ifname)
            sigma_dut_cmd_check("sta_get_ip_config,interface," + ifname)
            sigma_dut_cmd_check("sta_disconnect,interface," + ifname)
            sigma_dut_cmd_check("sta_reset_default,interface," + ifname)
            dev[0].dump_monitor()

        tests = [",Domain,w1.fi",
                 ",DomainSuffix,example.com",
                 ",ServerCert,sigma_dut_eap_ttls.incorrect.pem"]
        for extra in tests:
            sigma_dut_cmd_check("sta_reset_default,interface,%s,prog,WPA3" % ifname)
            sigma_dut_cmd_check("sta_set_ip_config,interface,%s,dhcp,0,ip,127.0.0.11,mask,255.255.255.0" % ifname)
            sigma_dut_cmd_check(cmd + extra)
            sigma_dut_cmd_check("sta_associate,interface,%s,ssid,%s,channel,1" % (ifname, ssid),
                                timeout=10)
            ev = dev[0].wait_event(["CTRL-EVENT-EAP-TLS-CERT-ERROR"], timeout=10)
            if ev is None:
                raise Exception("Server certificate error not reported")
            res = sigma_dut_cmd("sta_is_connected,interface," + ifname)
            if "connected,1" in res:
                raise Exception("Unexpected connection reported")
            sigma_dut_cmd_check("sta_disconnect,interface," + ifname)
            sigma_dut_cmd_check("sta_reset_default,interface," + ifname)
            dev[0].dump_monitor()
    finally:
        stop_sigma_dut(sigma)

def test_sigma_dut_suite_b(dev, apdev, params):
    """sigma_dut controlled STA Suite B"""
    check_suite_b_192_capa(dev)
    logdir = params['logdir']

    with open("auth_serv/ec2-ca.pem", "r") as f:
        with open(os.path.join(logdir, "suite_b_ca.pem"), "w") as f2:
            f2.write(f.read())

    with open("auth_serv/ec2-user.pem", "r") as f:
        with open("auth_serv/ec2-user.key", "r") as f2:
            with open(os.path.join(logdir, "suite_b.pem"), "w") as f3:
                f3.write(f.read())
                f3.write(f2.read())

    dev[0].flush_scan_cache()
    params = suite_b_as_params()
    params['ca_cert'] = 'auth_serv/ec2-ca.pem'
    params['server_cert'] = 'auth_serv/ec2-server.pem'
    params['private_key'] = 'auth_serv/ec2-server.key'
    params['openssl_ciphers'] = 'SUITEB192'
    hostapd.add_ap(apdev[1], params)

    params = {"ssid": "test-suite-b",
              "wpa": "2",
              "wpa_key_mgmt": "WPA-EAP-SUITE-B-192",
              "rsn_pairwise": "GCMP-256",
              "group_mgmt_cipher": "BIP-GMAC-256",
              "ieee80211w": "2",
              "ieee8021x": "1",
              'auth_server_addr': "127.0.0.1",
              'auth_server_port': "18129",
              'auth_server_shared_secret': "radius",
              'nas_identifier': "nas.w1.fi"}
    hapd = hostapd.add_ap(apdev[0], params)

    ifname = dev[0].ifname
    sigma = start_sigma_dut(ifname, cert_path=logdir)

    try:
        sigma_dut_cmd_check("sta_reset_default,interface,%s,prog,PMF" % ifname)
        sigma_dut_cmd_check("sta_set_ip_config,interface,%s,dhcp,0,ip,127.0.0.11,mask,255.255.255.0" % ifname)
        sigma_dut_cmd_check("sta_set_security,type,eaptls,interface,%s,ssid,%s,PairwiseCipher,AES-GCMP-256,GroupCipher,AES-GCMP-256,GroupMgntCipher,BIP-GMAC-256,keymgmttype,SuiteB,clientCertificate,suite_b.pem,trustedRootCA,suite_b_ca.pem,CertType,ECC" % (ifname, "test-suite-b"))
        sigma_dut_cmd_check("sta_associate,interface,%s,ssid,%s,channel,1" % (ifname, "test-suite-b"),
                            timeout=10)
        sigma_dut_wait_connected(ifname)
        sigma_dut_cmd_check("sta_get_ip_config,interface," + ifname)
        sigma_dut_cmd_check("sta_disconnect,interface," + ifname)
        sigma_dut_cmd_check("sta_reset_default,interface," + ifname)
    finally:
        stop_sigma_dut(sigma)

def test_sigma_dut_suite_b_rsa(dev, apdev, params):
    """sigma_dut controlled STA Suite B (RSA)"""
    check_suite_b_192_capa(dev)
    logdir = params['logdir']

    with open("auth_serv/rsa3072-ca.pem", "r") as f:
        with open(os.path.join(logdir, "suite_b_ca_rsa.pem"), "w") as f2:
            f2.write(f.read())

    with open("auth_serv/rsa3072-user.pem", "r") as f:
        with open("auth_serv/rsa3072-user.key", "r") as f2:
            with open(os.path.join(logdir, "suite_b_rsa.pem"), "w") as f3:
                f3.write(f.read())
                f3.write(f2.read())

    dev[0].flush_scan_cache()
    params = suite_b_192_rsa_ap_params()
    hapd = hostapd.add_ap(apdev[0], params)

    ifname = dev[0].ifname
    sigma = start_sigma_dut(ifname, cert_path=logdir)

    cmd = "sta_set_security,type,eaptls,interface,%s,ssid,%s,PairwiseCipher,AES-GCMP-256,GroupCipher,AES-GCMP-256,GroupMgntCipher,BIP-GMAC-256,keymgmttype,SuiteB,clientCertificate,suite_b_rsa.pem,trustedRootCA,suite_b_ca_rsa.pem,CertType,RSA" % (ifname, "test-suite-b")

    try:
        tests = ["",
                 ",TLSCipher,TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384",
                 ",TLSCipher,TLS_DHE_RSA_WITH_AES_256_GCM_SHA384"]
        for extra in tests:
            sigma_dut_cmd_check("sta_reset_default,interface,%s,prog,PMF" % ifname)
            sigma_dut_cmd_check("sta_set_ip_config,interface,%s,dhcp,0,ip,127.0.0.11,mask,255.255.255.0" % ifname)
            sigma_dut_cmd_check(cmd + extra)
            sigma_dut_cmd_check("sta_associate,interface,%s,ssid,%s,channel,1" % (ifname, "test-suite-b"),
                                timeout=10)
            sigma_dut_wait_connected(ifname)
            sigma_dut_cmd_check("sta_get_ip_config,interface," + ifname)
            sigma_dut_cmd_check("sta_disconnect,interface," + ifname)
            sigma_dut_cmd_check("sta_reset_default,interface," + ifname)
    finally:
        stop_sigma_dut(sigma)

def test_sigma_dut_ap_suite_b(dev, apdev, params):
    """sigma_dut controlled AP Suite B"""
    check_suite_b_192_capa(dev)
    logdir = os.path.join(params['logdir'],
                          "sigma_dut_ap_suite_b.sigma-hostapd")
    params = suite_b_as_params()
    params['ca_cert'] = 'auth_serv/ec2-ca.pem'
    params['server_cert'] = 'auth_serv/ec2-server.pem'
    params['private_key'] = 'auth_serv/ec2-server.key'
    params['openssl_ciphers'] = 'SUITEB192'
    hostapd.add_ap(apdev[1], params)
    with HWSimRadio() as (radio, iface):
        sigma = start_sigma_dut(iface, hostapd_logdir=logdir)
        try:
            sigma_dut_cmd_check("ap_reset_default")
            sigma_dut_cmd_check("ap_set_wireless,NAME,AP,CHANNEL,1,SSID,test-suite-b,MODE,11ng")
            sigma_dut_cmd_check("ap_set_radius,NAME,AP,IPADDR,127.0.0.1,PORT,18129,PASSWORD,radius")
            sigma_dut_cmd_check("ap_set_security,NAME,AP,KEYMGNT,SuiteB")
            sigma_dut_cmd_check("ap_config_commit,NAME,AP")

            dev[0].connect("test-suite-b", key_mgmt="WPA-EAP-SUITE-B-192",
                           ieee80211w="2",
                           openssl_ciphers="SUITEB192",
                           eap="TLS", identity="tls user",
                           ca_cert="auth_serv/ec2-ca.pem",
                           client_cert="auth_serv/ec2-user.pem",
                           private_key="auth_serv/ec2-user.key",
                           pairwise="GCMP-256", group="GCMP-256",
                           scan_freq="2412")

            sigma_dut_cmd_check("ap_reset_default")
        finally:
            stop_sigma_dut(sigma)

def test_sigma_dut_ap_cipher_gcmp_128(dev, apdev, params):
    """sigma_dut controlled AP with GCMP-128/BIP-GMAC-128 cipher"""
    run_sigma_dut_ap_cipher(dev, apdev, params, "AES-GCMP-128", "BIP-GMAC-128",
                            "GCMP")

def test_sigma_dut_ap_cipher_gcmp_256(dev, apdev, params):
    """sigma_dut controlled AP with GCMP-256/BIP-GMAC-256 cipher"""
    run_sigma_dut_ap_cipher(dev, apdev, params, "AES-GCMP-256", "BIP-GMAC-256",
                            "GCMP-256")

def test_sigma_dut_ap_cipher_ccmp_128(dev, apdev, params):
    """sigma_dut controlled AP with CCMP-128/BIP-CMAC-128 cipher"""
    run_sigma_dut_ap_cipher(dev, apdev, params, "AES-CCMP-128", "BIP-CMAC-128",
                            "CCMP")

def test_sigma_dut_ap_cipher_ccmp_256(dev, apdev, params):
    """sigma_dut controlled AP with CCMP-256/BIP-CMAC-256 cipher"""
    run_sigma_dut_ap_cipher(dev, apdev, params, "AES-CCMP-256", "BIP-CMAC-256",
                            "CCMP-256")

def test_sigma_dut_ap_cipher_ccmp_gcmp_1(dev, apdev, params):
    """sigma_dut controlled AP with CCMP-128+GCMP-256 ciphers (1)"""
    run_sigma_dut_ap_cipher(dev, apdev, params, "AES-CCMP-128 AES-GCMP-256",
                            "BIP-GMAC-256", "CCMP")

def test_sigma_dut_ap_cipher_ccmp_gcmp_2(dev, apdev, params):
    """sigma_dut controlled AP with CCMP-128+GCMP-256 ciphers (2)"""
    run_sigma_dut_ap_cipher(dev, apdev, params, "AES-CCMP-128 AES-GCMP-256",
                            "BIP-GMAC-256", "GCMP-256", "CCMP")

def test_sigma_dut_ap_cipher_gcmp_256_group_ccmp(dev, apdev, params):
    """sigma_dut controlled AP with GCMP-256/CCMP/BIP-GMAC-256 cipher"""
    run_sigma_dut_ap_cipher(dev, apdev, params, "AES-GCMP-256", "BIP-GMAC-256",
                            "GCMP-256", "CCMP", "AES-CCMP-128")

def run_sigma_dut_ap_cipher(dev, apdev, params, ap_pairwise, ap_group_mgmt,
                            sta_cipher, sta_cipher_group=None, ap_group=None):
    check_suite_b_192_capa(dev)
    logdir = os.path.join(params['logdir'],
                          "sigma_dut_ap_cipher.sigma-hostapd")
    params = suite_b_as_params()
    params['ca_cert'] = 'auth_serv/ec2-ca.pem'
    params['server_cert'] = 'auth_serv/ec2-server.pem'
    params['private_key'] = 'auth_serv/ec2-server.key'
    params['openssl_ciphers'] = 'SUITEB192'
    hostapd.add_ap(apdev[1], params)
    with HWSimRadio() as (radio, iface):
        sigma = start_sigma_dut(iface, hostapd_logdir=logdir)
        try:
            sigma_dut_cmd_check("ap_reset_default")
            sigma_dut_cmd_check("ap_set_wireless,NAME,AP,CHANNEL,1,SSID,test-suite-b,MODE,11ng")
            sigma_dut_cmd_check("ap_set_radius,NAME,AP,IPADDR,127.0.0.1,PORT,18129,PASSWORD,radius")
            cmd = "ap_set_security,NAME,AP,KEYMGNT,SuiteB,PMF,Required,PairwiseCipher,%s,GroupMgntCipher,%s" % (ap_pairwise, ap_group_mgmt)
            if ap_group:
                cmd += ",GroupCipher,%s" % ap_group
            sigma_dut_cmd_check(cmd)
            sigma_dut_cmd_check("ap_config_commit,NAME,AP")

            if sta_cipher_group is None:
                sta_cipher_group = sta_cipher
            dev[0].connect("test-suite-b", key_mgmt="WPA-EAP-SUITE-B-192",
                           ieee80211w="2",
                           openssl_ciphers="SUITEB192",
                           eap="TLS", identity="tls user",
                           ca_cert="auth_serv/ec2-ca.pem",
                           client_cert="auth_serv/ec2-user.pem",
                           private_key="auth_serv/ec2-user.key",
                           pairwise=sta_cipher, group=sta_cipher_group,
                           scan_freq="2412")

            sigma_dut_cmd_check("ap_reset_default")
        finally:
            stop_sigma_dut(sigma)

def test_sigma_dut_ap_override_rsne(dev, apdev):
    """sigma_dut controlled AP overriding RSNE"""
    with HWSimRadio() as (radio, iface):
        sigma = start_sigma_dut(iface)
        try:
            sigma_dut_cmd_check("ap_reset_default")
            sigma_dut_cmd_check("ap_set_wireless,NAME,AP,CHANNEL,1,SSID,test-psk,MODE,11ng")
            sigma_dut_cmd_check("ap_set_security,NAME,AP,KEYMGNT,WPA2-PSK,PSK,12345678")
            sigma_dut_cmd_check("dev_configure_ie,NAME,AP,interface,%s,IE_Name,RSNE,Contents,30180100000fac040200ffffffff000fac040100000fac020c00" % iface)
            sigma_dut_cmd_check("ap_config_commit,NAME,AP")

            dev[0].connect("test-psk", psk="12345678", scan_freq="2412")

            sigma_dut_cmd_check("ap_reset_default")
        finally:
            stop_sigma_dut(sigma)

def test_sigma_dut_ap_sae(dev, apdev, params):
    """sigma_dut controlled AP with SAE"""
    logdir = os.path.join(params['logdir'],
                          "sigma_dut_ap_sae.sigma-hostapd")
    if "SAE" not in dev[0].get_capability("auth_alg"):
        raise HwsimSkip("SAE not supported")
    with HWSimRadio() as (radio, iface):
        sigma = start_sigma_dut(iface, hostapd_logdir=logdir)
        try:
            sigma_dut_cmd_check("ap_reset_default")
            sigma_dut_cmd_check("ap_set_wireless,NAME,AP,CHANNEL,1,SSID,test-sae,MODE,11ng")
            sigma_dut_cmd_check("ap_set_security,NAME,AP,KEYMGNT,WPA2-SAE,PSK,12345678")
            sigma_dut_cmd_check("ap_config_commit,NAME,AP")

            dev[0].request("SET sae_groups ")
            id = dev[0].connect("test-sae", key_mgmt="SAE", psk="12345678",
                                ieee80211w="2", scan_freq="2412")
            if dev[0].get_status_field('sae_group') != '19':
                raise Exception("Expected default SAE group not used")

            res = sigma_dut_cmd_check("ap_get_parameter,name,AP,STA_MAC_Address,%s,Parameter,PMK" % dev[0].own_addr())
            logger.info("Reported PMK: " + res)
            if ",PMK," not in res:
                raise Exception("PMK not reported");
            if dev[0].get_pmk(id) != res.split(',')[3]:
                raise Exception("Mismatch in reported PMK")

            sigma_dut_cmd_check("ap_reset_default")
        finally:
            stop_sigma_dut(sigma)

def test_sigma_dut_ap_sae_confirm_immediate(dev, apdev, params):
    """sigma_dut controlled AP with SAE Confirm immediate"""
    logdir = os.path.join(params['logdir'],
                          "sigma_dut_ap_sae_confirm_immediate.sigma-hostapd")
    if "SAE" not in dev[0].get_capability("auth_alg"):
        raise HwsimSkip("SAE not supported")
    with HWSimRadio() as (radio, iface):
        sigma = start_sigma_dut(iface, hostapd_logdir=logdir)
        try:
            sigma_dut_cmd_check("ap_reset_default")
            sigma_dut_cmd_check("ap_set_wireless,NAME,AP,CHANNEL,1,SSID,test-sae,MODE,11ng")
            sigma_dut_cmd_check("ap_set_security,NAME,AP,KEYMGNT,WPA2-SAE,PSK,12345678,SAE_Confirm_Immediate,enable")
            sigma_dut_cmd_check("ap_config_commit,NAME,AP")

            dev[0].request("SET sae_groups ")
            dev[0].connect("test-sae", key_mgmt="SAE", psk="12345678",
                           ieee80211w="2", scan_freq="2412")
            if dev[0].get_status_field('sae_group') != '19':
                raise Exception("Expected default SAE group not used")

            sigma_dut_cmd_check("ap_reset_default")
        finally:
            stop_sigma_dut(sigma)

def test_sigma_dut_ap_sae_password(dev, apdev, params):
    """sigma_dut controlled AP with SAE and long password"""
    logdir = os.path.join(params['logdir'],
                          "sigma_dut_ap_sae_password.sigma-hostapd")
    if "SAE" not in dev[0].get_capability("auth_alg"):
        raise HwsimSkip("SAE not supported")
    with HWSimRadio() as (radio, iface):
        sigma = start_sigma_dut(iface, hostapd_logdir=logdir)
        try:
            sigma_dut_cmd_check("ap_reset_default")
            sigma_dut_cmd_check("ap_set_wireless,NAME,AP,CHANNEL,1,SSID,test-sae,MODE,11ng")
            sigma_dut_cmd_check("ap_set_security,NAME,AP,KEYMGNT,WPA2-SAE,PSK," + 100*'C')
            sigma_dut_cmd_check("ap_config_commit,NAME,AP")

            dev[0].request("SET sae_groups ")
            dev[0].connect("test-sae", key_mgmt="SAE", sae_password=100*'C',
                           ieee80211w="2", scan_freq="2412")
            if dev[0].get_status_field('sae_group') != '19':
                raise Exception("Expected default SAE group not used")

            sigma_dut_cmd_check("ap_reset_default")
        finally:
            stop_sigma_dut(sigma)

def test_sigma_dut_ap_sae_pw_id(dev, apdev, params):
    """sigma_dut controlled AP with SAE Password Identifier"""
    logdir = os.path.join(params['logdir'],
                          "sigma_dut_ap_sae_pw_id.sigma-hostapd")
    conffile = os.path.join(params['logdir'],
                            "sigma_dut_ap_sae_pw_id.sigma-conf")
    if "SAE" not in dev[0].get_capability("auth_alg"):
        raise HwsimSkip("SAE not supported")
    with HWSimRadio() as (radio, iface):
        sigma = start_sigma_dut(iface, hostapd_logdir=logdir)
        try:
            sigma_dut_cmd_check("ap_reset_default")
            sigma_dut_cmd_check("ap_set_wireless,NAME,AP,CHANNEL,1,SSID,test-sae,MODE,11ng")
            sigma_dut_cmd_check("ap_set_security,NAME,AP,AKMSuiteType,8,SAEPasswords,pw1:id1;pw2:id2;pw3;pw4:id4,PMF,Required")
            sigma_dut_cmd_check("ap_config_commit,NAME,AP")

            with open("/tmp/sigma_dut-ap.conf", "rb") as f:
                with open(conffile, "wb") as f2:
                    f2.write(f.read())

            dev[0].request("SET sae_groups ")
            tests = [("pw1", "id1"),
                     ("pw2", "id2"),
                     ("pw3", None),
                     ("pw4", "id4")]
            for pw, pw_id in tests:
                dev[0].connect("test-sae", key_mgmt="SAE", sae_password=pw,
                               sae_password_id=pw_id,
                               ieee80211w="2", scan_freq="2412")
                dev[0].request("REMOVE_NETWORK all")
                dev[0].wait_disconnected()

            sigma_dut_cmd_check("ap_reset_default")
        finally:
            stop_sigma_dut(sigma)

def test_sigma_dut_ap_sae_pw_id_pwe_loop(dev, apdev, params):
    """sigma_dut controlled AP with SAE Password Identifier and forced PWE looping"""
    logdir = os.path.join(params['logdir'],
                          "sigma_dut_ap_sae_pw_id_pwe_loop.sigma-hostapd")
    conffile = os.path.join(params['logdir'],
                            "sigma_dut_ap_sae_pw_id_pwe_loop.sigma-conf")
    if "SAE" not in dev[0].get_capability("auth_alg"):
        raise HwsimSkip("SAE not supported")
    with HWSimRadio() as (radio, iface):
        sigma = start_sigma_dut(iface, hostapd_logdir=logdir)
        try:
            sigma_dut_cmd_check("ap_reset_default")
            sigma_dut_cmd_check("ap_set_wireless,NAME,AP,CHANNEL,1,SSID,test-sae,MODE,11ng")
            sigma_dut_cmd_check("ap_set_security,NAME,AP,AKMSuiteType,8,SAEPasswords,12345678:pwid,PMF,Required,sae_pwe,looping")
            sigma_dut_cmd_check("ap_config_commit,NAME,AP")

            with open("/tmp/sigma_dut-ap.conf", "rb") as f:
                with open(conffile, "wb") as f2:
                    f2.write(f.read())

            dev[0].set("sae_groups", "")
            dev[0].connect("test-sae", key_mgmt="SAE", sae_password="12345678",
                           sae_password_id="pwid",
                           ieee80211w="2", scan_freq="2412", wait_connect=False)
            ev = dev[0].wait_event(["CTRL-EVENT-NETWORK-NOT-FOUND",
                                    "CTRL-EVENT-CONNECTED"], timeout=10)
            if ev is None:
                raise Exception("Network selection result not indicated")
            if "CTRL-EVENT-CONNECTED" in ev:
                raise Exception("Unexpected connection")
            dev[0].request("REMOVE_NETWORK all")

            sigma_dut_cmd_check("ap_reset_default")
        finally:
            stop_sigma_dut(sigma)

def test_sigma_dut_ap_sae_pw_id_ft(dev, apdev, params):
    """sigma_dut controlled AP with SAE Password Identifier and FT"""
    logdir = os.path.join(params['logdir'],
                          "sigma_dut_ap_sae_pw_id_ft.sigma-hostapd")
    conffile = os.path.join(params['logdir'],
                            "sigma_dut_ap_sae_pw_id_ft.sigma-conf")
    if "SAE" not in dev[0].get_capability("auth_alg"):
        raise HwsimSkip("SAE not supported")
    with HWSimRadio() as (radio, iface):
        sigma = start_sigma_dut(iface, hostapd_logdir=logdir)
        try:
            sigma_dut_cmd_check("ap_reset_default")
            sigma_dut_cmd_check("ap_set_wireless,NAME,AP,CHANNEL,1,SSID,test-sae,MODE,11ng,DOMAIN,aabb")
            sigma_dut_cmd_check("ap_set_security,NAME,AP,AKMSuiteType,8;9,SAEPasswords,pw1:id1;pw2:id2;pw3;pw4:id4,PMF,Required")
            sigma_dut_cmd_check("ap_config_commit,NAME,AP")

            with open("/tmp/sigma_dut-ap.conf", "rb") as f:
                with open(conffile, "wb") as f2:
                    f2.write(f.read())

            dev[0].request("SET sae_groups ")
            tests = [("pw1", "id1", "SAE"),
                     ("pw2", "id2", "FT-SAE"),
                     ("pw3", None, "FT-SAE"),
                     ("pw4", "id4", "SAE")]
            for pw, pw_id, key_mgmt in tests:
                dev[0].connect("test-sae", key_mgmt=key_mgmt, sae_password=pw,
                               sae_password_id=pw_id,
                               ieee80211w="2", scan_freq="2412")
                dev[0].request("REMOVE_NETWORK all")
                dev[0].wait_disconnected()

            sigma_dut_cmd_check("ap_reset_default")
        finally:
            stop_sigma_dut(sigma)

def test_sigma_dut_ap_sae_group(dev, apdev, params):
    """sigma_dut controlled AP with SAE and specific group"""
    logdir = os.path.join(params['logdir'],
                          "sigma_dut_ap_sae_group.sigma-hostapd")
    if "SAE" not in dev[0].get_capability("auth_alg"):
        raise HwsimSkip("SAE not supported")
    with HWSimRadio() as (radio, iface):
        sigma = start_sigma_dut(iface, hostapd_logdir=logdir)
        try:
            sigma_dut_cmd_check("ap_reset_default")
            sigma_dut_cmd_check("ap_set_wireless,NAME,AP,CHANNEL,1,SSID,test-sae,MODE,11ng")
            sigma_dut_cmd_check("ap_set_security,NAME,AP,KEYMGNT,WPA2-SAE,PSK,12345678,ECGroupID,20")
            sigma_dut_cmd_check("ap_config_commit,NAME,AP")

            dev[0].request("SET sae_groups ")
            dev[0].connect("test-sae", key_mgmt="SAE", psk="12345678",
                           ieee80211w="2", scan_freq="2412")
            if dev[0].get_status_field('sae_group') != '20':
                raise Exception("Expected SAE group not used")

            sigma_dut_cmd_check("ap_reset_default")
        finally:
            stop_sigma_dut(sigma)

def test_sigma_dut_ap_psk_sae(dev, apdev, params):
    """sigma_dut controlled AP with PSK+SAE"""
    if "SAE" not in dev[0].get_capability("auth_alg"):
        raise HwsimSkip("SAE not supported")
    logdir = os.path.join(params['logdir'],
                          "sigma_dut_ap_psk_sae.sigma-hostapd")
    with HWSimRadio() as (radio, iface):
        sigma = start_sigma_dut(iface, hostapd_logdir=logdir)
        try:
            sigma_dut_cmd_check("ap_reset_default")
            sigma_dut_cmd_check("ap_set_wireless,NAME,AP,CHANNEL,1,SSID,test-sae,MODE,11ng")
            sigma_dut_cmd_check("ap_set_security,NAME,AP,KEYMGNT,WPA2-PSK-SAE,PSK,12345678")
            sigma_dut_cmd_check("ap_config_commit,NAME,AP")

            dev[2].request("SET sae_groups ")
            dev[2].connect("test-sae", key_mgmt="SAE", psk="12345678",
                           scan_freq="2412", ieee80211w="0", wait_connect=False)
            dev[0].request("SET sae_groups ")
            dev[0].connect("test-sae", key_mgmt="SAE", psk="12345678",
                           scan_freq="2412", ieee80211w="2")
            dev[1].connect("test-sae", psk="12345678", scan_freq="2412")

            ev = dev[2].wait_event(["CTRL-EVENT-CONNECTED"], timeout=0.1)
            dev[2].request("DISCONNECT")
            if ev is not None:
                raise Exception("Unexpected connection without PMF")

            sigma_dut_cmd_check("ap_reset_default")
        finally:
            stop_sigma_dut(sigma)

def test_sigma_dut_ap_psk_sae_ft(dev, apdev, params):
    """sigma_dut controlled AP with PSK, SAE, FT"""
    logdir = os.path.join(params['logdir'],
                          "sigma_dut_ap_psk_sae_ft.sigma-hostapd")
    conffile = os.path.join(params['logdir'],
                            "sigma_dut_ap_psk_sae_ft.sigma-conf")
    if "SAE" not in dev[0].get_capability("auth_alg"):
        raise HwsimSkip("SAE not supported")
    with HWSimRadio() as (radio, iface):
        sigma = start_sigma_dut(iface, hostapd_logdir=logdir)
        try:
            sigma_dut_cmd_check("ap_reset_default,NAME,AP,Program,WPA3")
            sigma_dut_cmd_check("ap_set_wireless,NAME,AP,CHANNEL,1,SSID,test-sae-psk,MODE,11ng,DOMAIN,aabb")
            sigma_dut_cmd_check("ap_set_security,NAME,AP,AKMSuiteType,2;4;6;8;9,PSK,12345678,PairwiseCipher,AES-CCMP-128,GroupCipher,AES-CCMP-128")
            sigma_dut_cmd_check("ap_set_wireless,NAME,AP,DOMAIN,0101,FT_OA,Enable")
            sigma_dut_cmd_check("ap_set_wireless,NAME,AP,FT_BSS_LIST," + apdev[1]['bssid'])
            sigma_dut_cmd_check("ap_config_commit,NAME,AP")

            with open("/tmp/sigma_dut-ap.conf", "rb") as f:
                with open(conffile, "wb") as f2:
                    f2.write(f.read())

            dev[0].request("SET sae_groups ")
            dev[0].connect("test-sae-psk", key_mgmt="SAE FT-SAE",
                           sae_password="12345678", scan_freq="2412")
            dev[1].connect("test-sae-psk", key_mgmt="WPA-PSK FT-PSK",
                           psk="12345678", scan_freq="2412")
            dev[2].connect("test-sae-psk", key_mgmt="WPA-PSK",
                           psk="12345678", scan_freq="2412")

            sigma_dut_cmd_check("ap_reset_default")
        finally:
            stop_sigma_dut(sigma)

@reset_ignore_old_scan_res
def test_sigma_dut_owe(dev, apdev):
    """sigma_dut controlled OWE station"""
    if "OWE" not in dev[0].get_capability("key_mgmt"):
        raise HwsimSkip("OWE not supported")

    ifname = dev[0].ifname
    sigma = start_sigma_dut(ifname)

    try:
        params = {"ssid": "owe",
                  "wpa": "2",
                  "wpa_key_mgmt": "OWE",
                  "ieee80211w": "2",
                  "rsn_pairwise": "CCMP"}
        hapd = hostapd.add_ap(apdev[0], params)
        bssid = hapd.own_addr()

        sigma_dut_cmd_check("sta_reset_default,interface,%s,prog,WPA3" % ifname)
        sigma_dut_cmd_check("sta_set_ip_config,interface,%s,dhcp,0,ip,127.0.0.11,mask,255.255.255.0" % ifname)
        sigma_dut_cmd_check("sta_set_security,interface,%s,ssid,owe,Type,OWE" % ifname)
        sigma_dut_cmd_check("sta_associate,interface,%s,ssid,owe,channel,1" % ifname,
                            timeout=10)
        sigma_dut_wait_connected(ifname)
        sigma_dut_cmd_check("sta_get_ip_config,interface," + ifname)
        res = sigma_dut_cmd_check("sta_get_parameter,interface,%s,Parameter,PMK" % ifname)
        logger.info("Reported PMK: " + res)
        if ",PMK," not in res:
            raise Exception("PMK not reported");
        if hapd.request("GET_PMK " + dev[0].own_addr()) != res.split(',')[3]:
            raise Exception("Mismatch in reported PMK")

        dev[0].dump_monitor()
        sigma_dut_cmd("sta_reassoc,interface,%s,Channel,1,bssid,%s" % (ifname, bssid))
        dev[0].wait_connected()
        sigma_dut_cmd_check("sta_disconnect,interface," + ifname)
        dev[0].wait_disconnected()
        dev[0].dump_monitor()

        sigma_dut_cmd_check("sta_reset_default,interface,%s,prog,WPA3" % ifname)
        sigma_dut_cmd_check("sta_set_ip_config,interface,%s,dhcp,0,ip,127.0.0.11,mask,255.255.255.0" % ifname)
        sigma_dut_cmd_check("sta_set_security,interface,%s,ssid,owe,Type,OWE,ECGroupID,20" % ifname)
        sigma_dut_cmd_check("sta_associate,interface,%s,ssid,owe,channel,1" % ifname,
                            timeout=10)
        sigma_dut_wait_connected(ifname)
        sigma_dut_cmd_check("sta_get_ip_config,interface," + ifname)
        sigma_dut_cmd_check("sta_disconnect,interface," + ifname)
        dev[0].wait_disconnected()
        dev[0].dump_monitor()

        sigma_dut_cmd_check("sta_reset_default,interface,%s,prog,WPA3" % ifname)
        sigma_dut_cmd_check("sta_set_ip_config,interface,%s,dhcp,0,ip,127.0.0.11,mask,255.255.255.0" % ifname)
        sigma_dut_cmd_check("sta_set_security,interface,%s,ssid,owe,Type,OWE,ECGroupID,0" % ifname)
        sigma_dut_cmd_check("sta_associate,interface,%s,ssid,owe,channel,1" % ifname,
                            timeout=10)
        ev = dev[0].wait_event(["CTRL-EVENT-ASSOC-REJECT"], timeout=10)
        sigma_dut_cmd_check("sta_disconnect,interface," + ifname)
        if ev is None:
            raise Exception("Association not rejected")
        if "status_code=77" not in ev:
            raise Exception("Unexpected rejection reason: " + ev)

        sigma_dut_cmd_check("sta_reset_default,interface," + ifname)
    finally:
        stop_sigma_dut(sigma)

@reset_ignore_old_scan_res
def test_sigma_dut_owe_ptk_workaround(dev, apdev):
    """sigma_dut controlled OWE station with PTK workaround"""
    if "OWE" not in dev[0].get_capability("key_mgmt"):
        raise HwsimSkip("OWE not supported")

    params = {"ssid": "owe",
              "wpa": "2",
              "wpa_key_mgmt": "OWE",
              "owe_ptk_workaround": "1",
              "owe_groups": "20",
              "ieee80211w": "2",
              "rsn_pairwise": "CCMP"}
    hapd = hostapd.add_ap(apdev[0], params)

    ifname = dev[0].ifname
    sigma = start_sigma_dut(ifname, owe_ptk_workaround=True)

    try:
        sigma_dut_cmd_check("sta_reset_default,interface,%s,prog,WPA3" % ifname)
        sigma_dut_cmd_check("sta_set_ip_config,interface,%s,dhcp,0,ip,127.0.0.11,mask,255.255.255.0" % ifname)
        sigma_dut_cmd_check("sta_set_security,interface,%s,ssid,owe,Type,OWE,ECGroupID,20" % ifname)
        sigma_dut_cmd_check("sta_associate,interface,%s,ssid,owe,channel,1" % ifname,
                            timeout=10)
        sigma_dut_wait_connected(ifname)
        sigma_dut_cmd_check("sta_reset_default,interface," + ifname)
    finally:
        stop_sigma_dut(sigma)

def test_sigma_dut_ap_owe(dev, apdev, params):
    """sigma_dut controlled AP with OWE"""
    logdir = os.path.join(params['logdir'],
                          "sigma_dut_ap_owe.sigma-hostapd")
    if "OWE" not in dev[0].get_capability("key_mgmt"):
        raise HwsimSkip("OWE not supported")
    with HWSimRadio() as (radio, iface):
        sigma = start_sigma_dut(iface, hostapd_logdir=logdir)
        try:
            sigma_dut_cmd_check("ap_reset_default,NAME,AP,Program,WPA3")
            sigma_dut_cmd_check("ap_set_wireless,NAME,AP,CHANNEL,1,SSID,owe,MODE,11ng")
            sigma_dut_cmd_check("ap_set_security,NAME,AP,KEYMGNT,OWE")
            sigma_dut_cmd_check("ap_config_commit,NAME,AP")

            id = dev[0].connect("owe", key_mgmt="OWE", ieee80211w="2",
                                scan_freq="2412")

            res = sigma_dut_cmd_check("ap_get_parameter,name,AP,STA_MAC_Address,%s,Parameter,PMK" % dev[0].own_addr())
            logger.info("Reported PMK: " + res)
            if ",PMK," not in res:
                raise Exception("PMK not reported");
            if dev[0].get_pmk(id) != res.split(',')[3]:
                raise Exception("Mismatch in reported PMK")

            sigma_dut_cmd_check("ap_reset_default")
        finally:
            stop_sigma_dut(sigma)

def test_sigma_dut_ap_owe_ecgroupid(dev, apdev):
    """sigma_dut controlled AP with OWE and ECGroupID"""
    if "OWE" not in dev[0].get_capability("key_mgmt"):
        raise HwsimSkip("OWE not supported")
    with HWSimRadio() as (radio, iface):
        sigma = start_sigma_dut(iface)
        try:
            sigma_dut_cmd_check("ap_reset_default,NAME,AP,Program,WPA3")
            sigma_dut_cmd_check("ap_set_wireless,NAME,AP,CHANNEL,1,SSID,owe,MODE,11ng")
            sigma_dut_cmd_check("ap_set_security,NAME,AP,KEYMGNT,OWE,ECGroupID,20 21,PMF,Required")
            sigma_dut_cmd_check("ap_config_commit,NAME,AP")

            dev[0].connect("owe", key_mgmt="OWE", ieee80211w="2",
                           owe_group="20", scan_freq="2412")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

            dev[0].connect("owe", key_mgmt="OWE", ieee80211w="2",
                           owe_group="21", scan_freq="2412")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

            dev[0].connect("owe", key_mgmt="OWE", ieee80211w="2",
                           owe_group="19", scan_freq="2412", wait_connect=False)
            ev = dev[0].wait_event(["CTRL-EVENT-ASSOC-REJECT"], timeout=10)
            dev[0].request("DISCONNECT")
            if ev is None:
                raise Exception("Association not rejected")
            if "status_code=77" not in ev:
                raise Exception("Unexpected rejection reason: " + ev)
            dev[0].dump_monitor()

            sigma_dut_cmd_check("ap_reset_default")
        finally:
            stop_sigma_dut(sigma)

def test_sigma_dut_ap_owe_ptk_workaround(dev, apdev):
    """sigma_dut controlled AP with OWE PTK workaround"""
    if "OWE" not in dev[0].get_capability("key_mgmt"):
        raise HwsimSkip("OWE not supported")
    with HWSimRadio() as (radio, iface):
        sigma = start_sigma_dut(iface, owe_ptk_workaround=True)
        try:
            sigma_dut_cmd_check("ap_reset_default,NAME,AP,Program,WPA3")
            sigma_dut_cmd_check("ap_set_wireless,NAME,AP,CHANNEL,1,SSID,owe,MODE,11ng")
            sigma_dut_cmd_check("ap_set_security,NAME,AP,KEYMGNT,OWE,ECGroupID,20,PMF,Required")
            sigma_dut_cmd_check("ap_config_commit,NAME,AP")

            dev[0].connect("owe", key_mgmt="OWE", ieee80211w="2",
                           owe_group="20", owe_ptk_workaround="1",
                           scan_freq="2412")
            sigma_dut_cmd_check("ap_reset_default")
        finally:
            stop_sigma_dut(sigma)

def test_sigma_dut_ap_owe_transition_mode(dev, apdev, params):
    """sigma_dut controlled AP with OWE and transition mode"""
    if "OWE" not in dev[0].get_capability("key_mgmt"):
        raise HwsimSkip("OWE not supported")
    logdir = os.path.join(params['logdir'],
                          "sigma_dut_ap_owe_transition_mode.sigma-hostapd")
    with HWSimRadio() as (radio, iface):
        sigma = start_sigma_dut(iface, hostapd_logdir=logdir)
        try:
            sigma_dut_cmd_check("ap_reset_default,NAME,AP,Program,WPA3")
            sigma_dut_cmd_check("ap_set_wireless,NAME,AP,WLAN_TAG,1,CHANNEL,1,SSID,owe,MODE,11ng")
            sigma_dut_cmd_check("ap_set_security,NAME,AP,WLAN_TAG,1,KEYMGNT,OWE")
            sigma_dut_cmd_check("ap_set_wireless,NAME,AP,WLAN_TAG,2,CHANNEL,1,SSID,owe,MODE,11ng")
            sigma_dut_cmd_check("ap_set_security,NAME,AP,WLAN_TAG,2,KEYMGNT,NONE")
            sigma_dut_cmd_check("ap_config_commit,NAME,AP")

            res1 = sigma_dut_cmd_check("ap_get_mac_address,NAME,AP,WLAN_TAG,1,Interface,24G")
            res2 = sigma_dut_cmd_check("ap_get_mac_address,NAME,AP,WLAN_TAG,2,Interface,24G")

            dev[0].connect("owe", key_mgmt="OWE", ieee80211w="2",
                           scan_freq="2412")
            dev[1].connect("owe", key_mgmt="NONE", scan_freq="2412")
            if dev[0].get_status_field('bssid') not in res1:
                raise Exception("Unexpected ap_get_mac_address WLAN_TAG,1: " + res1)
            if dev[1].get_status_field('bssid') not in res2:
                raise Exception("Unexpected ap_get_mac_address WLAN_TAG,2: " + res2)

            sigma_dut_cmd_check("ap_reset_default")
        finally:
            stop_sigma_dut(sigma)

def test_sigma_dut_ap_owe_transition_mode_2(dev, apdev, params):
    """sigma_dut controlled AP with OWE and transition mode (2)"""
    if "OWE" not in dev[0].get_capability("key_mgmt"):
        raise HwsimSkip("OWE not supported")
    logdir = os.path.join(params['logdir'],
                          "sigma_dut_ap_owe_transition_mode_2.sigma-hostapd")
    with HWSimRadio() as (radio, iface):
        sigma = start_sigma_dut(iface, hostapd_logdir=logdir)
        try:
            sigma_dut_cmd_check("ap_reset_default,NAME,AP,Program,WPA3")
            sigma_dut_cmd_check("ap_set_wireless,NAME,AP,WLAN_TAG,1,CHANNEL,1,SSID,owe,MODE,11ng")
            sigma_dut_cmd_check("ap_set_security,NAME,AP,WLAN_TAG,1,KEYMGNT,NONE")
            sigma_dut_cmd_check("ap_set_wireless,NAME,AP,WLAN_TAG,2,CHANNEL,1,MODE,11ng")
            sigma_dut_cmd_check("ap_set_security,NAME,AP,WLAN_TAG,2,KEYMGNT,OWE")
            sigma_dut_cmd_check("ap_config_commit,NAME,AP")

            res1 = sigma_dut_cmd_check("ap_get_mac_address,NAME,AP,WLAN_TAG,1,Interface,24G")
            res2 = sigma_dut_cmd_check("ap_get_mac_address,NAME,AP,WLAN_TAG,2,Interface,24G")

            dev[0].connect("owe", key_mgmt="OWE", ieee80211w="2",
                           scan_freq="2412")
            dev[1].connect("owe", key_mgmt="NONE", scan_freq="2412")
            if dev[0].get_status_field('bssid') not in res2:
                raise Exception("Unexpected ap_get_mac_address WLAN_TAG,2: " + res1)
            if dev[1].get_status_field('bssid') not in res1:
                raise Exception("Unexpected ap_get_mac_address WLAN_TAG,1: " + res2)

            sigma_dut_cmd_check("ap_reset_default")
        finally:
            stop_sigma_dut(sigma)

def dpp_init_enrollee(dev, id1, enrollee_role):
    logger.info("Starting DPP initiator/enrollee in a thread")
    time.sleep(1)
    cmd = "DPP_AUTH_INIT peer=%d role=enrollee" % id1
    if enrollee_role == "Configurator":
        cmd += " netrole=configurator"
    if "OK" not in dev.request(cmd):
        raise Exception("Failed to initiate DPP Authentication")
    ev = dev.wait_event(["DPP-CONF-RECEIVED"], timeout=5)
    if ev is None:
        raise Exception("DPP configuration not completed (Enrollee)")
    logger.info("DPP initiator/enrollee done")

def test_sigma_dut_dpp_qr_resp_1(dev, apdev):
    """sigma_dut DPP/QR responder (conf index 1)"""
    run_sigma_dut_dpp_qr_resp(dev, apdev, 1)

def test_sigma_dut_dpp_qr_resp_2(dev, apdev):
    """sigma_dut DPP/QR responder (conf index 2)"""
    run_sigma_dut_dpp_qr_resp(dev, apdev, 2)

def test_sigma_dut_dpp_qr_resp_3(dev, apdev):
    """sigma_dut DPP/QR responder (conf index 3)"""
    run_sigma_dut_dpp_qr_resp(dev, apdev, 3)

def test_sigma_dut_dpp_qr_resp_4(dev, apdev):
    """sigma_dut DPP/QR responder (conf index 4)"""
    run_sigma_dut_dpp_qr_resp(dev, apdev, 4)

def test_sigma_dut_dpp_qr_resp_5(dev, apdev):
    """sigma_dut DPP/QR responder (conf index 5)"""
    run_sigma_dut_dpp_qr_resp(dev, apdev, 5)

def test_sigma_dut_dpp_qr_resp_6(dev, apdev):
    """sigma_dut DPP/QR responder (conf index 6)"""
    run_sigma_dut_dpp_qr_resp(dev, apdev, 6)

def test_sigma_dut_dpp_qr_resp_7(dev, apdev):
    """sigma_dut DPP/QR responder (conf index 7)"""
    run_sigma_dut_dpp_qr_resp(dev, apdev, 7)

def test_sigma_dut_dpp_qr_resp_8(dev, apdev):
    """sigma_dut DPP/QR responder (conf index 8)"""
    run_sigma_dut_dpp_qr_resp(dev, apdev, 8)

def test_sigma_dut_dpp_qr_resp_9(dev, apdev):
    """sigma_dut DPP/QR responder (conf index 9)"""
    run_sigma_dut_dpp_qr_resp(dev, apdev, 9)

def test_sigma_dut_dpp_qr_resp_10(dev, apdev):
    """sigma_dut DPP/QR responder (conf index 10)"""
    run_sigma_dut_dpp_qr_resp(dev, apdev, 10)

def test_sigma_dut_dpp_qr_resp_11(dev, apdev, params):
    """sigma_dut DPP/QR responder (conf index 11)"""
    if not os.path.exists("./dpp-ca.py"):
        raise HwsimSkip("dpp-ca.py not available")
    logdir = params['logdir']
    with open("auth_serv/ec-ca.pem", "rb") as f:
        res = f.read()
    with open(os.path.join(logdir, "dpp-ca.pem"), "wb") as f:
        f.write(res)
    with open("auth_serv/ec-ca.key", "rb") as f:
        res = f.read()
    with open(os.path.join(logdir, "dpp-ca.key"), "wb") as f:
        f.write(res)
    with open(os.path.join(logdir, "dpp-ca-csrattrs"), "wb") as f:
        f.write(b'MAsGCSqGSIb3DQEJBw==')
    run_sigma_dut_dpp_qr_resp(dev, apdev, 11, cert_path=logdir)

def test_sigma_dut_dpp_qr_resp_chan_list(dev, apdev):
    """sigma_dut DPP/QR responder (channel list override)"""
    run_sigma_dut_dpp_qr_resp(dev, apdev, 1, chan_list='81/2 81/6 81/1',
                              listen_chan=2)

def test_sigma_dut_dpp_qr_resp_status_query(dev, apdev):
    """sigma_dut DPP/QR responder status query"""
    check_dpp_capab(dev[1])
    params = hostapd.wpa2_params(ssid="DPPNET01",
                                 passphrase="ThisIsDppPassphrase")
    hapd = hostapd.add_ap(apdev[0], params)

    try:
        dev[1].set("dpp_config_processing", "2")
        run_sigma_dut_dpp_qr_resp(dev, apdev, 3, status_query=True)
    finally:
        dev[1].set("dpp_config_processing", "0", allow_fail=True)

def test_sigma_dut_dpp_qr_resp_configurator(dev, apdev):
    """sigma_dut DPP/QR responder (configurator provisioning)"""
    run_sigma_dut_dpp_qr_resp(dev, apdev, -1, enrollee_role="Configurator")

def run_sigma_dut_dpp_qr_resp(dev, apdev, conf_idx, chan_list=None,
                              listen_chan=None, status_query=False,
                              enrollee_role="STA", cert_path=None):
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    sigma = start_sigma_dut(dev[0].ifname, cert_path=cert_path)
    try:
        cmd = "dev_exec_action,program,DPP,DPPActionType,GetLocalBootstrap,DPPCryptoIdentifier,P-256,DPPBS,QR"
        if chan_list:
            cmd += ",DPPChannelList," + chan_list
        res = sigma_dut_cmd(cmd)
        if "status,COMPLETE" not in res:
            raise Exception("dev_exec_action did not succeed: " + res)
        hex = res.split(',')[3]
        uri = from_hex(hex)
        logger.info("URI from sigma_dut: " + uri)

        id1 = dev[1].dpp_qr_code(uri)

        t = threading.Thread(target=dpp_init_enrollee, args=(dev[1], id1,
                                                             enrollee_role))
        t.start()
        cmd = "dev_exec_action,program,DPP,DPPActionType,AutomaticDPP,DPPAuthRole,Responder,DPPAuthDirection,Single,DPPProvisioningRole,Configurator,DPPConfEnrolleeRole,%s,DPPSigningKeyECC,P-256,DPPBS,QR,DPPTimeout,6" % enrollee_role
        if conf_idx is not None:
            cmd += ",DPPConfIndex,%d" % conf_idx
        if listen_chan:
            cmd += ",DPPListenChannel," + str(listen_chan)
        if status_query:
            cmd += ",DPPStatusQuery,Yes"
        res = sigma_dut_cmd(cmd, timeout=10)
        t.join()
        if "BootstrapResult,OK,AuthResult,OK,ConfResult,OK" not in res:
            raise Exception("Unexpected result: " + res)
        if status_query and "StatusResult,0" not in res:
            raise Exception("Status query did not succeed: " + res)
    finally:
        stop_sigma_dut(sigma)

csign = "30770201010420768240a3fc89d6662d9782f120527fe7fb9edc6366ab0b9c7dde96125cfd250fa00a06082a8648ce3d030107a144034200042908e1baf7bf413cc66f9e878a03e8bb1835ba94b033dbe3d6969fc8575d5eb5dfda1cb81c95cee21d0cd7d92ba30541ffa05cb6296f5dd808b0c1c2a83c0708"
csign_pub = "3059301306072a8648ce3d020106082a8648ce3d030107034200042908e1baf7bf413cc66f9e878a03e8bb1835ba94b033dbe3d6969fc8575d5eb5dfda1cb81c95cee21d0cd7d92ba30541ffa05cb6296f5dd808b0c1c2a83c0708"
ap_connector = "eyJ0eXAiOiJkcHBDb24iLCJraWQiOiJwYWtZbXVzd1dCdWpSYTl5OEsweDViaTVrT3VNT3dzZHRlaml2UG55ZHZzIiwiYWxnIjoiRVMyNTYifQ.eyJncm91cHMiOlt7Imdyb3VwSWQiOiIqIiwibmV0Um9sZSI6ImFwIn1dLCJuZXRBY2Nlc3NLZXkiOnsia3R5IjoiRUMiLCJjcnYiOiJQLTI1NiIsIngiOiIybU5vNXZuRkI5bEw3d1VWb1hJbGVPYzBNSEE1QXZKbnpwZXZULVVTYzVNIiwieSI6IlhzS3dqVHJlLTg5WWdpU3pKaG9CN1haeUttTU05OTl3V2ZaSVl0bi01Q3MifX0.XhjFpZgcSa7G2lHy0OCYTvaZFRo5Hyx6b7g7oYyusLC7C_73AJ4_BxEZQVYJXAtDuGvb3dXSkHEKxREP9Q6Qeg"
ap_netaccesskey = "30770201010420ceba752db2ad5200fa7bc565b9c05c69b7eb006751b0b329b0279de1c19ca67ca00a06082a8648ce3d030107a14403420004da6368e6f9c507d94bef0515a1722578e73430703902f267ce97af4fe51273935ec2b08d3adefbcf588224b3261a01ed76722a630cf7df7059f64862d9fee42b"

def start_dpp_ap(apdev):
    params = {"ssid": "DPPNET01",
              "wpa": "2",
              "ieee80211w": "2",
              "wpa_key_mgmt": "DPP",
              "rsn_pairwise": "CCMP",
              "dpp_connector": ap_connector,
              "dpp_csign": csign_pub,
              "dpp_netaccesskey": ap_netaccesskey}
    try:
        hapd = hostapd.add_ap(apdev, params)
    except:
        raise HwsimSkip("DPP not supported")
    return hapd

def test_sigma_dut_dpp_qr_init_enrollee(dev, apdev):
    """sigma_dut DPP/QR initiator as Enrollee"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    hapd = start_dpp_ap(apdev[0])
    sigma = start_sigma_dut(dev[0].ifname)
    try:
        dev[0].set("dpp_config_processing", "2")

        cmd = "DPP_CONFIGURATOR_ADD key=" + csign
        res = dev[1].request(cmd)
        if "FAIL" in res:
            raise Exception("Failed to add configurator")
        conf_id = int(res)

        id0 = dev[1].dpp_bootstrap_gen(chan="81/6", mac=True)
        uri0 = dev[1].request("DPP_BOOTSTRAP_GET_URI %d" % id0)

        dev[1].set("dpp_configurator_params",
                   " conf=sta-dpp ssid=%s configurator=%d" % (to_hex("DPPNET01"), conf_id))
        cmd = "DPP_LISTEN 2437 role=configurator"
        if "OK" not in dev[1].request(cmd):
            raise Exception("Failed to start listen operation")

        res = sigma_dut_cmd("dev_exec_action,program,DPP,DPPActionType,SetPeerBootstrap,DPPBootstrappingdata,%s,DPPBS,QR" % to_hex(uri0))
        if "status,COMPLETE" not in res:
            raise Exception("dev_exec_action did not succeed: " + res)

        res = sigma_dut_cmd("dev_exec_action,program,DPP,DPPActionType,AutomaticDPP,DPPAuthRole,Initiator,DPPAuthDirection,Single,DPPProvisioningRole,Enrollee,DPPBS,QR,DPPTimeout,6,DPPWaitForConnect,Yes", timeout=10)
        if "BootstrapResult,OK,AuthResult,OK,ConfResult,OK,NetworkIntroResult,OK,NetworkConnectResult,OK" not in res:
            raise Exception("Unexpected result: " + res)
    finally:
        dev[0].set("dpp_config_processing", "0")
        stop_sigma_dut(sigma)

def test_sigma_dut_dpp_qr_init_enrollee_configurator(dev, apdev):
    """sigma_dut DPP/QR initiator as Enrollee (to become Configurator)"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])

    sigma = start_sigma_dut(dev[0].ifname)
    try:
        cmd = "DPP_CONFIGURATOR_ADD"
        res = dev[1].request(cmd)
        if "FAIL" in res:
            raise Exception("Failed to add configurator")
        conf_id = int(res)

        id0 = dev[1].dpp_bootstrap_gen(chan="81/6", mac=True)
        uri0 = dev[1].request("DPP_BOOTSTRAP_GET_URI %d" % id0)

        dev[1].set("dpp_configurator_params",
                   " conf=configurator ssid=%s configurator=%d" % (to_hex("DPPNET01"), conf_id))
        cmd = "DPP_LISTEN 2437 role=configurator"
        if "OK" not in dev[1].request(cmd):
            raise Exception("Failed to start listen operation")

        res = sigma_dut_cmd("dev_exec_action,program,DPP,DPPActionType,SetPeerBootstrap,DPPBootstrappingdata,%s,DPPBS,QR" % to_hex(uri0))
        if "status,COMPLETE" not in res:
            raise Exception("dev_exec_action did not succeed: " + res)

        res = sigma_dut_cmd("dev_exec_action,program,DPP,DPPActionType,AutomaticDPP,DPPAuthRole,Initiator,DPPAuthDirection,Single,DPPProvisioningRole,Enrollee,DPPNetworkRole,Configurator,DPPBS,QR,DPPTimeout,6", timeout=10)
        if "BootstrapResult,OK,AuthResult,OK,ConfResult,OK" not in res:
            raise Exception("Unexpected result: " + res)
    finally:
        dev[0].set("dpp_config_processing", "0")
        stop_sigma_dut(sigma)

def test_sigma_dut_dpp_qr_mutual_init_enrollee(dev, apdev):
    """sigma_dut DPP/QR (mutual) initiator as Enrollee"""
    run_sigma_dut_dpp_qr_mutual_init_enrollee_check(dev, apdev)

def test_sigma_dut_dpp_qr_mutual_init_enrollee_check(dev, apdev):
    """sigma_dut DPP/QR (mutual) initiator as Enrollee (extra check)"""
    run_sigma_dut_dpp_qr_mutual_init_enrollee_check(dev, apdev,
                                                    extra="DPPAuthDirection,Mutual,")

def test_sigma_dut_dpp_qr_mutual_init_enrollee_mud_url(dev, apdev):
    """sigma_dut DPP/QR (mutual) initiator as Enrollee (MUD URL)"""
    run_sigma_dut_dpp_qr_mutual_init_enrollee_check(dev, apdev,
                                                    mud_url="https://example.com/mud")

def run_sigma_dut_dpp_qr_mutual_init_enrollee_check(dev, apdev, extra='',
                                                    mud_url=None):
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    hapd = start_dpp_ap(apdev[0])
    ifname = dev[0].ifname
    sigma = start_sigma_dut(ifname)
    try:
        sigma_dut_cmd_check("sta_reset_default,interface,%s,prog,DPP" % ifname)

        cmd = "DPP_CONFIGURATOR_ADD key=" + csign
        res = dev[1].request(cmd)
        if "FAIL" in res:
            raise Exception("Failed to add configurator")
        conf_id = int(res)

        id0 = dev[1].dpp_bootstrap_gen(chan="81/6", mac=True)
        uri0 = dev[1].request("DPP_BOOTSTRAP_GET_URI %d" % id0)

        dev[1].set("dpp_configurator_params",
                   " conf=sta-dpp ssid=%s configurator=%d" % (to_hex("DPPNET01"), conf_id))
        cmd = "DPP_LISTEN 2437 role=configurator qr=mutual"
        if "OK" not in dev[1].request(cmd):
            raise Exception("Failed to start listen operation")

        res = sigma_dut_cmd("dev_exec_action,program,DPP,DPPActionType,GetLocalBootstrap,DPPCryptoIdentifier,P-256,DPPBS,QR")
        if "status,COMPLETE" not in res:
            raise Exception("dev_exec_action did not succeed: " + res)
        hex = res.split(',')[3]
        uri = from_hex(hex)
        logger.info("URI from sigma_dut: " + uri)

        id1 = dev[1].dpp_qr_code(uri)

        res = sigma_dut_cmd("dev_exec_action,program,DPP,DPPActionType,SetPeerBootstrap,DPPBootstrappingdata,%s,DPPBS,QR" % to_hex(uri0))
        if "status,COMPLETE" not in res:
            raise Exception("dev_exec_action did not succeed: " + res)

        cmd = "dev_exec_action,program,DPP,DPPActionType,AutomaticDPP,DPPAuthRole,Initiator,%sDPPProvisioningRole,Enrollee,DPPBS,QR,DPPTimeout,6,DPPWaitForConnect,Yes" % extra
        if mud_url:
            cmd += ",MUDURL," + mud_url
        res = sigma_dut_cmd_check(cmd, timeout=10)
        sigma_dut_cmd_check("sta_reset_default,interface,%s,prog,DPP" % ifname)
        if "BootstrapResult,OK,AuthResult,OK,ConfResult,OK,NetworkIntroResult,OK,NetworkConnectResult,OK" not in res:
            raise Exception("Unexpected result: " + res)

        if mud_url:
            ev = dev[1].wait_event(["DPP-MUD-URL"], timeout=1)
            if ev is None:
                raise Exception("DPP MUD URL not reported")
            if ev.split(' ')[1] != mud_url:
                raise Exception("Unexpected MUD URL value: " + ev)
    finally:
        dev[0].set("dpp_config_processing", "0")
        stop_sigma_dut(sigma)

def dpp_init_conf_mutual(dev, id1, conf_id, own_id=None):
    time.sleep(1)
    logger.info("Starting DPP initiator/configurator in a thread")
    cmd = "DPP_AUTH_INIT peer=%d conf=sta-dpp ssid=%s configurator=%d" % (id1, to_hex("DPPNET01"), conf_id)
    if own_id is not None:
        cmd += " own=%d" % own_id
    if "OK" not in dev.request(cmd):
        raise Exception("Failed to initiate DPP Authentication")
    ev = dev.wait_event(["DPP-CONF-SENT"], timeout=10)
    if ev is None:
        raise Exception("DPP configuration not completed (Configurator)")
    logger.info("DPP initiator/configurator done")

def test_sigma_dut_dpp_qr_mutual_resp_enrollee(dev, apdev):
    """sigma_dut DPP/QR (mutual) responder as Enrollee"""
    run_sigma_dut_dpp_qr_mutual_resp_enrollee(dev, apdev)

def test_sigma_dut_dpp_qr_mutual_resp_enrollee_pending(dev, apdev):
    """sigma_dut DPP/QR (mutual) responder as Enrollee (response pending)"""
    run_sigma_dut_dpp_qr_mutual_resp_enrollee(dev, apdev, ',DPPDelayQRResponse,1')

def run_sigma_dut_dpp_qr_mutual_resp_enrollee(dev, apdev, extra=None):
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    hapd = start_dpp_ap(apdev[0])
    sigma = start_sigma_dut(dev[0].ifname)
    try:
        dev[0].set("dpp_config_processing", "2")

        cmd = "DPP_CONFIGURATOR_ADD key=" + csign
        res = dev[1].request(cmd)
        if "FAIL" in res:
            raise Exception("Failed to add configurator")
        conf_id = int(res)

        id0 = dev[1].dpp_bootstrap_gen(chan="81/6", mac=True)
        uri0 = dev[1].request("DPP_BOOTSTRAP_GET_URI %d" % id0)

        res = sigma_dut_cmd("dev_exec_action,program,DPP,DPPActionType,GetLocalBootstrap,DPPCryptoIdentifier,P-256,DPPBS,QR")
        if "status,COMPLETE" not in res:
            raise Exception("dev_exec_action did not succeed: " + res)
        hex = res.split(',')[3]
        uri = from_hex(hex)
        logger.info("URI from sigma_dut: " + uri)

        id1 = dev[1].dpp_qr_code(uri)

        res = sigma_dut_cmd("dev_exec_action,program,DPP,DPPActionType,SetPeerBootstrap,DPPBootstrappingdata,%s,DPPBS,QR" % to_hex(uri0))
        if "status,COMPLETE" not in res:
            raise Exception("dev_exec_action did not succeed: " + res)

        t = threading.Thread(target=dpp_init_conf_mutual,
                             args=(dev[1], id1, conf_id, id0))
        t.start()

        cmd = "dev_exec_action,program,DPP,DPPActionType,AutomaticDPP,DPPAuthRole,Responder,DPPAuthDirection,Mutual,DPPProvisioningRole,Enrollee,DPPBS,QR,DPPTimeout,20,DPPWaitForConnect,Yes"
        if extra:
            cmd += extra
        res = sigma_dut_cmd(cmd, timeout=25)
        t.join()
        if "BootstrapResult,OK,AuthResult,OK,ConfResult,OK,NetworkIntroResult,OK,NetworkConnectResult,OK" not in res:
            raise Exception("Unexpected result: " + res)
    finally:
        dev[0].set("dpp_config_processing", "0")
        stop_sigma_dut(sigma)

def dpp_resp_conf_mutual(dev, conf_id, uri):
    logger.info("Starting DPP responder/configurator in a thread")
    dev.set("dpp_configurator_params",
            " conf=sta-dpp ssid=%s configurator=%d" % (to_hex("DPPNET01"),
                                                       conf_id))
    cmd = "DPP_LISTEN 2437 role=configurator qr=mutual"
    if "OK" not in dev.request(cmd):
        raise Exception("Failed to initiate DPP listen")
    if uri:
        ev = dev.wait_event(["DPP-SCAN-PEER-QR-CODE"], timeout=10)
        if ev is None:
            raise Exception("QR Code scan for mutual authentication not requested")
        dev.dpp_qr_code(uri)
    ev = dev.wait_event(["DPP-CONF-SENT"], timeout=10)
    if ev is None:
        raise Exception("DPP configuration not completed (Configurator)")
    logger.info("DPP responder/configurator done")

def test_sigma_dut_dpp_qr_mutual_init_enrollee(dev, apdev):
    """sigma_dut DPP/QR (mutual) initiator as Enrollee"""
    run_sigma_dut_dpp_qr_mutual_init_enrollee(dev, apdev, False)

def test_sigma_dut_dpp_qr_mutual_init_enrollee_pending(dev, apdev):
    """sigma_dut DPP/QR (mutual) initiator as Enrollee (response pending)"""
    run_sigma_dut_dpp_qr_mutual_init_enrollee(dev, apdev, True)

def run_sigma_dut_dpp_qr_mutual_init_enrollee(dev, apdev, resp_pending):
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    hapd = start_dpp_ap(apdev[0])
    sigma = start_sigma_dut(dev[0].ifname)
    try:
        dev[0].set("dpp_config_processing", "2")

        cmd = "DPP_CONFIGURATOR_ADD key=" + csign
        res = dev[1].request(cmd)
        if "FAIL" in res:
            raise Exception("Failed to add configurator")
        conf_id = int(res)

        id0 = dev[1].dpp_bootstrap_gen(chan="81/6", mac=True)
        uri0 = dev[1].request("DPP_BOOTSTRAP_GET_URI %d" % id0)

        res = sigma_dut_cmd("dev_exec_action,program,DPP,DPPActionType,GetLocalBootstrap,DPPCryptoIdentifier,P-256,DPPBS,QR")
        if "status,COMPLETE" not in res:
            raise Exception("dev_exec_action did not succeed: " + res)
        hex = res.split(',')[3]
        uri = from_hex(hex)
        logger.info("URI from sigma_dut: " + uri)

        if not resp_pending:
            dev[1].dpp_qr_code(uri)
            uri = None

        res = sigma_dut_cmd("dev_exec_action,program,DPP,DPPActionType,SetPeerBootstrap,DPPBootstrappingdata,%s,DPPBS,QR" % to_hex(uri0))
        if "status,COMPLETE" not in res:
            raise Exception("dev_exec_action did not succeed: " + res)

        t = threading.Thread(target=dpp_resp_conf_mutual,
                             args=(dev[1], conf_id, uri))
        t.start()

        time.sleep(1)
        cmd = "dev_exec_action,program,DPP,DPPActionType,AutomaticDPP,DPPAuthRole,Initiator,DPPProvisioningRole,Enrollee,DPPBS,QR,DPPTimeout,10,DPPWaitForConnect,Yes"
        res = sigma_dut_cmd(cmd, timeout=15)
        t.join()
        if "BootstrapResult,OK,AuthResult,OK,ConfResult,OK,NetworkIntroResult,OK,NetworkConnectResult,OK" not in res:
            raise Exception("Unexpected result: " + res)
    finally:
        dev[0].set("dpp_config_processing", "0")
        stop_sigma_dut(sigma)

def test_sigma_dut_dpp_qr_init_enrollee_psk(dev, apdev):
    """sigma_dut DPP/QR initiator as Enrollee (PSK)"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])

    params = hostapd.wpa2_params(ssid="DPPNET01",
                                 passphrase="ThisIsDppPassphrase")
    hapd = hostapd.add_ap(apdev[0], params)

    sigma = start_sigma_dut(dev[0].ifname)
    try:
        dev[0].set("dpp_config_processing", "2")

        cmd = "DPP_CONFIGURATOR_ADD"
        res = dev[1].request(cmd)
        if "FAIL" in res:
            raise Exception("Failed to add configurator")
        conf_id = int(res)

        id0 = dev[1].dpp_bootstrap_gen(chan="81/6", mac=True)
        uri0 = dev[1].request("DPP_BOOTSTRAP_GET_URI %d" % id0)

        dev[1].set("dpp_configurator_params",
                   " conf=sta-psk ssid=%s pass=%s configurator=%d" % (to_hex("DPPNET01"), to_hex("ThisIsDppPassphrase"), conf_id))
        cmd = "DPP_LISTEN 2437 role=configurator"
        if "OK" not in dev[1].request(cmd):
            raise Exception("Failed to start listen operation")

        res = sigma_dut_cmd("dev_exec_action,program,DPP,DPPActionType,SetPeerBootstrap,DPPBootstrappingdata,%s,DPPBS,QR" % to_hex(uri0))
        if "status,COMPLETE" not in res:
            raise Exception("dev_exec_action did not succeed: " + res)

        res = sigma_dut_cmd("dev_exec_action,program,DPP,DPPActionType,AutomaticDPP,DPPAuthRole,Initiator,DPPAuthDirection,Single,DPPProvisioningRole,Enrollee,DPPBS,QR,DPPTimeout,6,DPPWaitForConnect,Yes", timeout=10)
        if "BootstrapResult,OK,AuthResult,OK,ConfResult,OK,NetworkConnectResult,OK" not in res:
            raise Exception("Unexpected result: " + res)
    finally:
        dev[0].set("dpp_config_processing", "0")
        stop_sigma_dut(sigma)

def test_sigma_dut_dpp_qr_init_enrollee_sae(dev, apdev):
    """sigma_dut DPP/QR initiator as Enrollee (SAE)"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    if "SAE" not in dev[0].get_capability("auth_alg"):
        raise HwsimSkip("SAE not supported")

    params = hostapd.wpa2_params(ssid="DPPNET01",
                                 passphrase="ThisIsDppPassphrase")
    params['wpa_key_mgmt'] = 'SAE'
    params["ieee80211w"] = "2"
    hapd = hostapd.add_ap(apdev[0], params)

    sigma = start_sigma_dut(dev[0].ifname)
    try:
        dev[0].set("dpp_config_processing", "2")
        dev[0].set("sae_groups", "")

        cmd = "DPP_CONFIGURATOR_ADD"
        res = dev[1].request(cmd)
        if "FAIL" in res:
            raise Exception("Failed to add configurator")
        conf_id = int(res)

        id0 = dev[1].dpp_bootstrap_gen(chan="81/6", mac=True)
        uri0 = dev[1].request("DPP_BOOTSTRAP_GET_URI %d" % id0)

        dev[1].set("dpp_configurator_params",
                   " conf=sta-sae ssid=%s pass=%s configurator=%d" % (to_hex("DPPNET01"), to_hex("ThisIsDppPassphrase"), conf_id))
        cmd = "DPP_LISTEN 2437 role=configurator"
        if "OK" not in dev[1].request(cmd):
            raise Exception("Failed to start listen operation")

        res = sigma_dut_cmd("dev_exec_action,program,DPP,DPPActionType,SetPeerBootstrap,DPPBootstrappingdata,%s,DPPBS,QR" % to_hex(uri0))
        if "status,COMPLETE" not in res:
            raise Exception("dev_exec_action did not succeed: " + res)

        res = sigma_dut_cmd("dev_exec_action,program,DPP,DPPActionType,AutomaticDPP,DPPAuthRole,Initiator,DPPAuthDirection,Single,DPPProvisioningRole,Enrollee,DPPBS,QR,DPPTimeout,6,DPPWaitForConnect,Yes", timeout=10)
        if "BootstrapResult,OK,AuthResult,OK,ConfResult,OK,NetworkConnectResult,OK" not in res:
            raise Exception("Unexpected result: " + res)
    finally:
        dev[0].set("dpp_config_processing", "0")
        stop_sigma_dut(sigma)

def test_sigma_dut_dpp_qr_init_configurator_1(dev, apdev):
    """sigma_dut DPP/QR initiator as Configurator (conf index 1)"""
    run_sigma_dut_dpp_qr_init_configurator(dev, apdev, 1)

def test_sigma_dut_dpp_qr_init_configurator_2(dev, apdev):
    """sigma_dut DPP/QR initiator as Configurator (conf index 2)"""
    run_sigma_dut_dpp_qr_init_configurator(dev, apdev, 2)

def test_sigma_dut_dpp_qr_init_configurator_3(dev, apdev):
    """sigma_dut DPP/QR initiator as Configurator (conf index 3)"""
    run_sigma_dut_dpp_qr_init_configurator(dev, apdev, 3)

def test_sigma_dut_dpp_qr_init_configurator_4(dev, apdev):
    """sigma_dut DPP/QR initiator as Configurator (conf index 4)"""
    run_sigma_dut_dpp_qr_init_configurator(dev, apdev, 4)

def test_sigma_dut_dpp_qr_init_configurator_5(dev, apdev):
    """sigma_dut DPP/QR initiator as Configurator (conf index 5)"""
    run_sigma_dut_dpp_qr_init_configurator(dev, apdev, 5)

def test_sigma_dut_dpp_qr_init_configurator_6(dev, apdev):
    """sigma_dut DPP/QR initiator as Configurator (conf index 6)"""
    run_sigma_dut_dpp_qr_init_configurator(dev, apdev, 6)

def test_sigma_dut_dpp_qr_init_configurator_7(dev, apdev):
    """sigma_dut DPP/QR initiator as Configurator (conf index 7)"""
    run_sigma_dut_dpp_qr_init_configurator(dev, apdev, 7)

def test_sigma_dut_dpp_qr_init_configurator_both(dev, apdev):
    """sigma_dut DPP/QR initiator as Configurator or Enrollee (conf index 1)"""
    run_sigma_dut_dpp_qr_init_configurator(dev, apdev, 1, "Both")

def test_sigma_dut_dpp_qr_init_configurator_neg_freq(dev, apdev):
    """sigma_dut DPP/QR initiator as Configurator (neg_freq)"""
    run_sigma_dut_dpp_qr_init_configurator(dev, apdev, 1, extra='DPPSubsequentChannel,81/11')

def test_sigma_dut_dpp_qr_init_configurator_mud_url(dev, apdev):
    """sigma_dut DPP/QR initiator as Configurator (MUD URL)"""
    run_sigma_dut_dpp_qr_init_configurator(dev, apdev, 1,
                                           mud_url="https://example.com/mud")

def run_sigma_dut_dpp_qr_init_configurator(dev, apdev, conf_idx,
                                           prov_role="Configurator",
                                           extra=None, mud_url=None):
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    sigma = start_sigma_dut(dev[0].ifname)
    try:
        id0 = dev[1].dpp_bootstrap_gen(chan="81/6", mac=True)
        uri0 = dev[1].request("DPP_BOOTSTRAP_GET_URI %d" % id0)

        if mud_url:
            dev[1].set("dpp_mud_url", mud_url)
        cmd = "DPP_LISTEN 2437 role=enrollee"
        if "OK" not in dev[1].request(cmd):
            raise Exception("Failed to start listen operation")

        res = sigma_dut_cmd("dev_exec_action,program,DPP,DPPActionType,SetPeerBootstrap,DPPBootstrappingdata,%s,DPPBS,QR" % to_hex(uri0))
        if "status,COMPLETE" not in res:
            raise Exception("dev_exec_action did not succeed: " + res)

        cmd = "dev_exec_action,program,DPP,DPPActionType,AutomaticDPP,DPPAuthRole,Initiator,DPPAuthDirection,Single,DPPProvisioningRole,%s,DPPConfIndex,%d,DPPSigningKeyECC,P-256,DPPConfEnrolleeRole,STA,DPPBS,QR,DPPTimeout,6" % (prov_role, conf_idx)
        if extra:
            cmd += "," + extra
        res = sigma_dut_cmd(cmd)
        if "BootstrapResult,OK,AuthResult,OK,ConfResult,OK" not in res:
            raise Exception("Unexpected result: " + res)
        if mud_url and ",MUDURL," + mud_url not in res:
            raise Exception("Unexpected result (missing MUD URL): " + res)
    finally:
        dev[1].set("dpp_mud_url", "")
        stop_sigma_dut(sigma)

def test_sigma_dut_dpp_incompatible_roles_init(dev, apdev):
    """sigma_dut DPP roles incompatible (Initiator)"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    sigma = start_sigma_dut(dev[0].ifname)
    try:
        res = sigma_dut_cmd("dev_exec_action,program,DPP,DPPActionType,GetLocalBootstrap,DPPCryptoIdentifier,P-256,DPPBS,QR")
        if "status,COMPLETE" not in res:
            raise Exception("dev_exec_action did not succeed: " + res)
        hex = res.split(',')[3]
        uri = from_hex(hex)
        logger.info("URI from sigma_dut: " + uri)

        id1 = dev[1].dpp_qr_code(uri)

        id0 = dev[1].dpp_bootstrap_gen(chan="81/6", mac=True)
        uri0 = dev[1].request("DPP_BOOTSTRAP_GET_URI %d" % id0)

        cmd = "DPP_LISTEN 2437 role=enrollee"
        if "OK" not in dev[1].request(cmd):
            raise Exception("Failed to start listen operation")

        res = sigma_dut_cmd("dev_exec_action,program,DPP,DPPActionType,SetPeerBootstrap,DPPBootstrappingdata,%s,DPPBS,QR" % to_hex(uri0))
        if "status,COMPLETE" not in res:
            raise Exception("dev_exec_action did not succeed: " + res)

        cmd = "dev_exec_action,program,DPP,DPPActionType,AutomaticDPP,DPPAuthRole,Initiator,DPPAuthDirection,Mutual,DPPProvisioningRole,Enrollee,DPPBS,QR,DPPTimeout,6"
        res = sigma_dut_cmd(cmd)
        if "BootstrapResult,OK,AuthResult,ROLES_NOT_COMPATIBLE" not in res:
            raise Exception("Unexpected result: " + res)
    finally:
        stop_sigma_dut(sigma)

def dpp_init_enrollee_mutual(dev, id1, own_id):
    logger.info("Starting DPP initiator/enrollee in a thread")
    time.sleep(1)
    cmd = "DPP_AUTH_INIT peer=%d own=%d role=enrollee" % (id1, own_id)
    if "OK" not in dev.request(cmd):
        raise Exception("Failed to initiate DPP Authentication")
    ev = dev.wait_event(["DPP-CONF-RECEIVED",
                         "DPP-NOT-COMPATIBLE"], timeout=5)
    if ev is None:
        raise Exception("DPP configuration not completed (Enrollee)")
    logger.info("DPP initiator/enrollee done")

def test_sigma_dut_dpp_incompatible_roles_resp(dev, apdev):
    """sigma_dut DPP roles incompatible (Responder)"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    sigma = start_sigma_dut(dev[0].ifname)
    try:
        cmd = "dev_exec_action,program,DPP,DPPActionType,GetLocalBootstrap,DPPCryptoIdentifier,P-256,DPPBS,QR"
        res = sigma_dut_cmd(cmd)
        if "status,COMPLETE" not in res:
            raise Exception("dev_exec_action did not succeed: " + res)
        hex = res.split(',')[3]
        uri = from_hex(hex)
        logger.info("URI from sigma_dut: " + uri)

        id1 = dev[1].dpp_qr_code(uri)

        id0 = dev[1].dpp_bootstrap_gen(chan="81/6", mac=True)
        uri0 = dev[1].request("DPP_BOOTSTRAP_GET_URI %d" % id0)

        res = sigma_dut_cmd("dev_exec_action,program,DPP,DPPActionType,SetPeerBootstrap,DPPBootstrappingdata,%s,DPPBS,QR" % to_hex(uri0))
        if "status,COMPLETE" not in res:
            raise Exception("dev_exec_action did not succeed: " + res)

        t = threading.Thread(target=dpp_init_enrollee_mutual, args=(dev[1], id1, id0))
        t.start()
        cmd = "dev_exec_action,program,DPP,DPPActionType,AutomaticDPP,DPPAuthRole,Responder,DPPAuthDirection,Mutual,DPPProvisioningRole,Enrollee,DPPBS,QR,DPPTimeout,6"
        res = sigma_dut_cmd(cmd, timeout=10)
        t.join()
        if "BootstrapResult,OK,AuthResult,ROLES_NOT_COMPATIBLE" not in res:
            raise Exception("Unexpected result: " + res)
    finally:
        stop_sigma_dut(sigma)

def test_sigma_dut_dpp_qr_enrollee_chirp(dev, apdev):
    """sigma_dut DPP/QR as chirping Enrollee"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    hapd = start_dpp_ap(apdev[0])
    ifname = dev[0].ifname
    sigma = start_sigma_dut(ifname)
    try:
        sigma_dut_cmd_check("sta_reset_default,interface,%s,prog,DPP" % ifname)
        cmd = "dev_exec_action,program,DPP,DPPActionType,GetLocalBootstrap,DPPCryptoIdentifier,P-256,DPPBS,QR"
        res = sigma_dut_cmd_check(cmd)
        if "status,COMPLETE" not in res:
            raise Exception("dev_exec_action did not succeed: " + res)
        hex = res.split(',')[3]
        uri = from_hex(hex)
        logger.info("URI from sigma_dut: " + uri)

        conf_id = dev[1].dpp_configurator_add(key=csign)
        idc = dev[1].dpp_qr_code(uri)
        dev[1].dpp_bootstrap_set(idc, conf="sta-dpp", configurator=conf_id,
                                 ssid="DPPNET01")
        dev[1].dpp_listen(2437)

        res = sigma_dut_cmd_check("dev_exec_action,program,DPP,DPPActionType,AutomaticDPP,DPPAuthRole,Responder,DPPAuthDirection,Single,DPPProvisioningRole,Enrollee,DPPBS,QR,DPPTimeout,16,DPPWaitForConnect,Yes,DPPChirp,Enable", timeout=20)
        sigma_dut_cmd_check("sta_reset_default,interface,%s,prog,DPP" % ifname)
        if "BootstrapResult,OK,AuthResult,OK,ConfResult,OK,NetworkIntroResult,OK,NetworkConnectResult,OK" not in res:
            raise Exception("Unexpected result: " + res)
    finally:
        dev[0].set("dpp_config_processing", "0")
        stop_sigma_dut(sigma)

def dpp_enrollee_chirp(dev, id1):
    logger.info("Starting chirping Enrollee in a thread")
    time.sleep(0.1)
    cmd = "DPP_CHIRP own=%d" % id1
    if "OK" not in dev.request(cmd):
        raise Exception("Failed to initiate DPP chirping")
    ev = dev.wait_event(["DPP-CONF-RECEIVED"], timeout=15)
    if ev is None:
        raise Exception("DPP configuration not completed (Enrollee)")
    logger.info("DPP enrollee done")

def test_sigma_dut_dpp_qr_configurator_chirp(dev, apdev):
    """sigma_dut DPP/QR as Configurator waiting for chirp"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    ifname = dev[0].ifname
    sigma = start_sigma_dut(ifname)
    try:
        sigma_dut_cmd_check("sta_reset_default,interface,%s,prog,DPP" % ifname)

        id1 = dev[1].dpp_bootstrap_gen(chan="81/1")
        uri = dev[1].request("DPP_BOOTSTRAP_GET_URI %d" % id1)

        res = sigma_dut_cmd_check("dev_exec_action,program,DPP,DPPActionType,SetPeerBootstrap,DPPBootstrappingdata,%s,DPPBS,QR" % to_hex(uri))
        if "status,COMPLETE" not in res:
            raise Exception("dev_exec_action did not succeed: " + res)

        t = threading.Thread(target=dpp_enrollee_chirp, args=(dev[1], id1))
        t.start()
        res = sigma_dut_cmd("dev_exec_action,program,DPP,DPPActionType,AutomaticDPP,DPPAuthRole,Initiator,DPPAuthDirection,Single,DPPProvisioningRole,Configurator,DPPConfIndex,1,DPPConfEnrolleeRole,STA,DPPBS,QR,DPPTimeout,16,DPPChirp,Enable,DPPChirpChannel,6", timeout=20)
        t.join()
        sigma_dut_cmd_check("sta_reset_default,interface,%s,prog,DPP" % ifname)
        if "BootstrapResult,OK,AuthResult,OK,ConfResult,OK" not in res:
            raise Exception("Unexpected result: " + res)
    finally:
        dev[0].set("dpp_config_processing", "0")
        stop_sigma_dut(sigma)

def test_sigma_dut_ap_dpp_qr_enrollee_chirp(dev, apdev, params):
    """sigma_dut DPP/QR AP as chirping Enrollee"""
    check_dpp_capab(dev[0], min_ver=2)
    check_dpp_capab(dev[1])
    logdir = params['prefix'] + ".sigma-hostapd"
    with HWSimRadio() as (radio, iface):
        sigma = start_sigma_dut(iface, hostapd_logdir=logdir)
        try:
            sigma_dut_cmd_check("ap_reset_default,program,DPP")
            cmd = "dev_exec_action,program,DPP,DPPActionType,GetLocalBootstrap,DPPCryptoIdentifier,P-256,DPPBS,QR"
            res = sigma_dut_cmd_check(cmd)
            if "status,COMPLETE" not in res:
                raise Exception("dev_exec_action did not succeed: " + res)
            hex = res.split(',')[3]
            uri = from_hex(hex)
            logger.info("URI from sigma_dut: " + uri)

            conf_id = dev[0].dpp_configurator_add(key=csign)
            idc = dev[0].dpp_qr_code(uri)
            dev[0].dpp_bootstrap_set(idc, conf="ap-dpp", configurator=conf_id,
                                 ssid="DPPNET01")
            dev[0].dpp_listen(2437)

            res = sigma_dut_cmd_check("dev_exec_action,program,DPP,DPPActionType,AutomaticDPP,DPPAuthRole,Responder,DPPAuthDirection,Single,DPPProvisioningRole,Enrollee,DPPBS,QR,DPPTimeout,16,DPPChirp,Enable", timeout=20)
            if "BootstrapResult,OK,AuthResult,OK,ConfResult,OK" not in res:
                raise Exception("Unexpected result: " + res)

            dev[1].set("dpp_config_processing", "2")
            id = dev[1].dpp_bootstrap_gen(chan="81/6", mac=True)
            uri = dev[1].request("DPP_BOOTSTRAP_GET_URI %d" % id)
            dev[1].dpp_listen(2437)
            dev[0].dpp_auth_init(uri=uri, conf="sta-dpp", ssid="DPPNET01",
                                 configurator=conf_id)
            dev[1].wait_connected(timeout=20)

            sigma_dut_cmd_check("ap_reset_default,program,DPP")
        finally:
            dev[1].set("dpp_config_processing", "0", allow_fail=True)
            stop_sigma_dut(sigma)

def test_sigma_dut_dpp_pkex_init_configurator(dev, apdev):
    """sigma_dut DPP/PKEX initiator as Configurator"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    sigma = start_sigma_dut(dev[0].ifname)
    try:
        id1 = dev[1].dpp_bootstrap_gen(type="pkex")
        cmd = "DPP_PKEX_ADD own=%d identifier=test code=secret" % (id1)
        res = dev[1].request(cmd)
        if "FAIL" in res:
            raise Exception("Failed to set PKEX data (responder)")
        cmd = "DPP_LISTEN 2437 role=enrollee"
        if "OK" not in dev[1].request(cmd):
            raise Exception("Failed to start listen operation")

        res = sigma_dut_cmd("dev_exec_action,program,DPP,DPPActionType,AutomaticDPP,DPPAuthRole,Initiator,DPPProvisioningRole,Configurator,DPPConfIndex,1,DPPSigningKeyECC,P-256,DPPConfEnrolleeRole,STA,DPPBS,PKEX,DPPPKEXCodeIdentifier,test,DPPPKEXCode,secret,DPPTimeout,6")
        if "BootstrapResult,OK,AuthResult,OK,ConfResult,OK" not in res:
            raise Exception("Unexpected result: " + res)
    finally:
        stop_sigma_dut(sigma)

def dpp_init_conf(dev, id1, conf, conf_id, extra):
    logger.info("Starting DPP initiator/configurator in a thread")
    cmd = "DPP_AUTH_INIT peer=%d conf=%s %s configurator=%d" % (id1, conf, extra, conf_id)
    if "OK" not in dev.request(cmd):
        raise Exception("Failed to initiate DPP Authentication")
    ev = dev.wait_event(["DPP-CONF-SENT"], timeout=5)
    if ev is None:
        raise Exception("DPP configuration not completed (Configurator)")
    logger.info("DPP initiator/configurator done")

def test_sigma_dut_ap_dpp_qr(dev, apdev, params):
    """sigma_dut controlled AP (DPP)"""
    run_sigma_dut_ap_dpp_qr(dev, apdev, params, "ap-dpp", "sta-dpp")

def test_sigma_dut_ap_dpp_qr_legacy(dev, apdev, params):
    """sigma_dut controlled AP (legacy)"""
    run_sigma_dut_ap_dpp_qr(dev, apdev, params, "ap-psk", "sta-psk",
                            extra="pass=%s" % to_hex("qwertyuiop"))

def test_sigma_dut_ap_dpp_qr_legacy_psk(dev, apdev, params):
    """sigma_dut controlled AP (legacy)"""
    run_sigma_dut_ap_dpp_qr(dev, apdev, params, "ap-psk", "sta-psk",
                            extra="psk=%s" % (32*"12"))

def run_sigma_dut_ap_dpp_qr(dev, apdev, params, ap_conf, sta_conf, extra=""):
    check_dpp_capab(dev[0])
    logdir = os.path.join(params['logdir'], "sigma_dut_ap_dpp_qr.sigma-hostapd")
    with HWSimRadio() as (radio, iface):
        sigma = start_sigma_dut(iface, hostapd_logdir=logdir)
        try:
            sigma_dut_cmd_check("ap_reset_default,program,DPP")
            res = sigma_dut_cmd("dev_exec_action,program,DPP,DPPActionType,GetLocalBootstrap,DPPCryptoIdentifier,P-256,DPPBS,QR")
            if "status,COMPLETE" not in res:
                raise Exception("dev_exec_action did not succeed: " + res)
            hex = res.split(',')[3]
            uri = from_hex(hex)
            logger.info("URI from sigma_dut: " + uri)

            cmd = "DPP_CONFIGURATOR_ADD"
            res = dev[0].request(cmd)
            if "FAIL" in res:
                raise Exception("Failed to add configurator")
            conf_id = int(res)

            id1 = dev[0].dpp_qr_code(uri)

            t = threading.Thread(target=dpp_init_conf,
                                 args=(dev[0], id1, ap_conf, conf_id, extra))
            t.start()
            res = sigma_dut_cmd("dev_exec_action,program,DPP,DPPActionType,AutomaticDPP,DPPAuthRole,Responder,DPPAuthDirection,Single,DPPProvisioningRole,Enrollee,DPPBS,QR,DPPTimeout,6")
            t.join()
            if "ConfResult,OK" not in res:
                raise Exception("Unexpected result: " + res)

            id1 = dev[1].dpp_bootstrap_gen(chan="81/1", mac=True)
            uri1 = dev[1].request("DPP_BOOTSTRAP_GET_URI %d" % id1)

            id0b = dev[0].dpp_qr_code(uri1)

            dev[1].set("dpp_config_processing", "2")
            cmd = "DPP_LISTEN 2412"
            if "OK" not in dev[1].request(cmd):
                raise Exception("Failed to start listen operation")
            cmd = "DPP_AUTH_INIT peer=%d conf=%s %s configurator=%d" % (id0b, sta_conf, extra, conf_id)
            if "OK" not in dev[0].request(cmd):
                raise Exception("Failed to initiate DPP Authentication")
            dev[1].wait_connected(timeout=20)

            sigma_dut_cmd_check("ap_reset_default")
        finally:
            dev[1].set("dpp_config_processing", "0")
            stop_sigma_dut(sigma)

def test_sigma_dut_ap_dpp_offchannel(dev, apdev, params):
    """sigma_dut controlled AP doing DPP on offchannel"""
    check_dpp_capab(dev[0])
    logdir = params['prefix'] + ".sigma-hostapd"
    with HWSimRadio() as (radio, iface):
        sigma = start_sigma_dut(iface, hostapd_logdir=logdir)
        try:
            sigma_dut_cmd_check("ap_reset_default,program,DPP")
            sigma_dut_cmd_check("ap_preset_testparameters,Program,DPP,Oper_Chn,3")
            res = sigma_dut_cmd_check("dev_exec_action,program,DPP,DPPActionType,GetLocalBootstrap,DPPCryptoIdentifier,P-256,DPPBS,QR")
            hex = res.split(',')[3]
            uri = from_hex(hex)
            logger.info("URI from sigma_dut: " + uri)
            if "C:81/3;" not in uri:
                raise Exception("Unexpected channel in AP's URI: " + uri)

            cmd = "DPP_CONFIGURATOR_ADD"
            res = dev[0].request(cmd)
            if "FAIL" in res:
                raise Exception("Failed to add configurator")
            conf_id = int(res)

            id0 = dev[0].dpp_bootstrap_gen(chan="81/7", mac=True)
            uri0 = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id0)
            dev[0].set("dpp_configurator_params",
                       "conf=ap-dpp ssid=%s configurator=%d" % (to_hex("DPPNET01"), conf_id))
            dev[0].dpp_listen(2442)

            res = sigma_dut_cmd("dev_exec_action,program,DPP,DPPActionType,SetPeerBootstrap,DPPBootstrappingdata,%s,DPPBS,QR" % to_hex(uri0))
            if "status,COMPLETE" not in res:
                raise Exception("dev_exec_action did not succeed: " + res)

            res = sigma_dut_cmd("dev_exec_action,program,DPP,DPPActionType,AutomaticDPP,DPPAuthRole,Initiator,DPPAuthDirection,Single,DPPProvisioningRole,Enrollee,DPPBS,QR,DPPTimeout,6")
            if "ConfResult,OK" not in res:
                raise Exception("Unexpected result: " + res)

            id1 = dev[1].dpp_bootstrap_gen(chan="81/1", mac=True)
            uri1 = dev[1].request("DPP_BOOTSTRAP_GET_URI %d" % id1)

            id0b = dev[0].dpp_qr_code(uri1)

            dev[1].set("dpp_config_processing", "2")
            cmd = "DPP_LISTEN 2412"
            if "OK" not in dev[1].request(cmd):
                raise Exception("Failed to start listen operation")
            cmd = "DPP_AUTH_INIT peer=%d conf=sta-dpp ssid=%s configurator=%d" % (id0b, to_hex("DPPNET01"), conf_id)
            if "OK" not in dev[0].request(cmd):
                raise Exception("Failed to initiate DPP Authentication")
            dev[1].wait_connected(timeout=20)

            sigma_dut_cmd_check("ap_reset_default")
        finally:
            dev[1].set("dpp_config_processing", "0")
            stop_sigma_dut(sigma)

def test_sigma_dut_ap_dpp_pkex_responder(dev, apdev, params):
    """sigma_dut controlled AP as DPP PKEX responder"""
    check_dpp_capab(dev[0])
    logdir = os.path.join(params['logdir'],
                          "sigma_dut_ap_dpp_pkex_responder.sigma-hostapd")
    with HWSimRadio() as (radio, iface):
        sigma = start_sigma_dut(iface, hostapd_logdir=logdir)
        try:
            run_sigma_dut_ap_dpp_pkex_responder(dev, apdev)
        finally:
            stop_sigma_dut(sigma)

def dpp_init_conf_pkex(dev, conf_id, check_config=True):
    logger.info("Starting DPP PKEX initiator/configurator in a thread")
    time.sleep(1.5)
    id = dev.dpp_bootstrap_gen(type="pkex")
    cmd = "DPP_PKEX_ADD own=%d init=1 conf=ap-dpp configurator=%d code=password" % (id, conf_id)
    res = dev.request(cmd)
    if "FAIL" in res:
        raise Exception("Failed to initiate DPP PKEX")
    if not check_config:
        return
    ev = dev.wait_event(["DPP-CONF-SENT"], timeout=5)
    if ev is None:
        raise Exception("DPP configuration not completed (Configurator)")
    logger.info("DPP initiator/configurator done")

def run_sigma_dut_ap_dpp_pkex_responder(dev, apdev):
    sigma_dut_cmd_check("ap_reset_default,program,DPP")

    cmd = "DPP_CONFIGURATOR_ADD"
    res = dev[0].request(cmd)
    if "FAIL" in res:
        raise Exception("Failed to add configurator")
    conf_id = int(res)

    t = threading.Thread(target=dpp_init_conf_pkex, args=(dev[0], conf_id))
    t.start()
    res = sigma_dut_cmd("dev_exec_action,program,DPP,DPPActionType,AutomaticDPP,DPPAuthRole,Responder,DPPAuthDirection,Mutual,DPPProvisioningRole,Enrollee,DPPBS,PKEX,DPPPKEXCode,password,DPPTimeout,6,DPPWaitForConnect,No", timeout=10)
    t.join()
    if "BootstrapResult,OK,AuthResult,OK,ConfResult,OK" not in res:
        raise Exception("Unexpected result: " + res)

    sigma_dut_cmd_check("ap_reset_default")

def test_sigma_dut_dpp_pkex_responder_proto(dev, apdev):
    """sigma_dut controlled STA as DPP PKEX responder and error case"""
    check_dpp_capab(dev[0])
    sigma = start_sigma_dut(dev[0].ifname)
    try:
        run_sigma_dut_dpp_pkex_responder_proto(dev, apdev)
    finally:
        stop_sigma_dut(sigma)

def run_sigma_dut_dpp_pkex_responder_proto(dev, apdev):
    cmd = "DPP_CONFIGURATOR_ADD"
    res = dev[1].request(cmd)
    if "FAIL" in res:
        raise Exception("Failed to add configurator")
    conf_id = int(res)

    dev[1].set("dpp_test", "44")

    t = threading.Thread(target=dpp_init_conf_pkex, args=(dev[1], conf_id,
                                                          False))
    t.start()
    res = sigma_dut_cmd("dev_exec_action,program,DPP,DPPActionType,AutomaticDPP,DPPAuthRole,Responder,DPPProvisioningRole,Enrollee,DPPBS,PKEX,DPPPKEXCode,password,DPPTimeout,6", timeout=10)
    t.join()
    if "BootstrapResult,Timeout" not in res:
        raise Exception("Unexpected result: " + res)

def dpp_proto_init(dev, id1):
    time.sleep(1)
    logger.info("Starting DPP initiator/configurator in a thread")
    cmd = "DPP_CONFIGURATOR_ADD"
    res = dev.request(cmd)
    if "FAIL" in res:
        raise Exception("Failed to add configurator")
    conf_id = int(res)

    cmd = "DPP_AUTH_INIT peer=%d conf=sta-dpp configurator=%d" % (id1, conf_id)
    if "OK" not in dev.request(cmd):
        raise Exception("Failed to initiate DPP Authentication")

def test_sigma_dut_dpp_proto_initiator(dev, apdev):
    """sigma_dut DPP protocol testing - Initiator"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    tests = [("InvalidValue", "AuthenticationRequest", "WrappedData",
              "BootstrapResult,OK,AuthResult,Errorsent",
              None),
             ("InvalidValue", "AuthenticationConfirm", "WrappedData",
              "BootstrapResult,OK,AuthResult,Errorsent",
              None),
             ("MissingAttribute", "AuthenticationRequest", "InitCapabilities",
              "BootstrapResult,OK,AuthResult,Errorsent",
              "Missing or invalid I-capabilities"),
             ("InvalidValue", "AuthenticationConfirm", "InitAuthTag",
              "BootstrapResult,OK,AuthResult,Errorsent",
              "Mismatching Initiator Authenticating Tag"),
             ("MissingAttribute", "ConfigurationResponse", "EnrolleeNonce",
              "BootstrapResult,OK,AuthResult,OK,ConfResult,Errorsent",
              "Missing or invalid Enrollee Nonce attribute")]
    for step, frame, attr, result, fail in tests:
        dev[0].request("FLUSH")
        dev[1].request("FLUSH")
        sigma = start_sigma_dut(dev[0].ifname)
        try:
            run_sigma_dut_dpp_proto_initiator(dev, step, frame, attr, result,
                                              fail)
        finally:
            stop_sigma_dut(sigma)

def run_sigma_dut_dpp_proto_initiator(dev, step, frame, attr, result, fail):
    id0 = dev[1].dpp_bootstrap_gen(chan="81/6", mac=True)
    uri0 = dev[1].request("DPP_BOOTSTRAP_GET_URI %d" % id0)

    cmd = "DPP_LISTEN 2437 role=enrollee"
    if "OK" not in dev[1].request(cmd):
        raise Exception("Failed to start listen operation")

    res = sigma_dut_cmd("dev_exec_action,program,DPP,DPPActionType,SetPeerBootstrap,DPPBootstrappingdata,%s,DPPBS,QR" % to_hex(uri0))
    if "status,COMPLETE" not in res:
        raise Exception("dev_exec_action did not succeed: " + res)

    res = sigma_dut_cmd("dev_exec_action,program,DPP,DPPActionType,AutomaticDPP,DPPAuthRole,Initiator,DPPAuthDirection,Single,DPPProvisioningRole,Configurator,DPPConfIndex,1,DPPSigningKeyECC,P-256,DPPConfEnrolleeRole,STA,DPPBS,QR,DPPTimeout,6,DPPStep,%s,DPPFrameType,%s,DPPIEAttribute,%s" % (step, frame, attr),
                        timeout=10)
    if result not in res:
        raise Exception("Unexpected result: " + res)
    if fail:
        ev = dev[1].wait_event(["DPP-FAIL"], timeout=5)
        if ev is None or fail not in ev:
            raise Exception("Failure not reported correctly: " + str(ev))

    dev[1].request("DPP_STOP_LISTEN")
    dev[0].dump_monitor()
    dev[1].dump_monitor()

def test_sigma_dut_dpp_proto_responder(dev, apdev):
    """sigma_dut DPP protocol testing - Responder"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    tests = [("MissingAttribute", "AuthenticationResponse", "DPPStatus",
              "BootstrapResult,OK,AuthResult,Errorsent",
              "Missing or invalid required DPP Status attribute"),
             ("MissingAttribute", "ConfigurationRequest", "EnrolleeNonce",
              "BootstrapResult,OK,AuthResult,OK,ConfResult,Errorsent",
              "Missing or invalid Enrollee Nonce attribute")]
    for step, frame, attr, result, fail in tests:
        dev[0].request("FLUSH")
        dev[1].request("FLUSH")
        sigma = start_sigma_dut(dev[0].ifname)
        try:
            run_sigma_dut_dpp_proto_responder(dev, step, frame, attr, result,
                                              fail)
        finally:
            stop_sigma_dut(sigma)

def run_sigma_dut_dpp_proto_responder(dev, step, frame, attr, result, fail):
    res = sigma_dut_cmd("dev_exec_action,program,DPP,DPPActionType,GetLocalBootstrap,DPPCryptoIdentifier,P-256,DPPBS,QR")
    if "status,COMPLETE" not in res:
        raise Exception("dev_exec_action did not succeed: " + res)
    hex = res.split(',')[3]
    uri = from_hex(hex)
    logger.info("URI from sigma_dut: " + uri)

    id1 = dev[1].dpp_qr_code(uri)

    t = threading.Thread(target=dpp_proto_init, args=(dev[1], id1))
    t.start()
    res = sigma_dut_cmd("dev_exec_action,program,DPP,DPPActionType,AutomaticDPP,DPPAuthRole,Responder,DPPAuthDirection,Single,DPPProvisioningRole,Enrollee,DPPConfIndex,1,DPPSigningKeyECC,P-256,DPPConfEnrolleeRole,STA,DPPBS,QR,DPPTimeout,6,DPPStep,%s,DPPFrameType,%s,DPPIEAttribute,%s" % (step, frame, attr), timeout=10)
    t.join()
    if result not in res:
        raise Exception("Unexpected result: " + res)
    if fail:
        ev = dev[1].wait_event(["DPP-FAIL"], timeout=5)
        if ev is None or fail not in ev:
            raise Exception("Failure not reported correctly:" + str(ev))

    dev[1].request("DPP_STOP_LISTEN")
    dev[0].dump_monitor()
    dev[1].dump_monitor()

def test_sigma_dut_dpp_proto_stop_at_initiator(dev, apdev):
    """sigma_dut DPP protocol testing - Stop at RX on Initiator"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    tests = [("AuthenticationResponse",
              "BootstrapResult,OK,AuthResult,Errorsent",
              None),
             ("ConfigurationRequest",
              "BootstrapResult,OK,AuthResult,OK,ConfResult,Errorsent",
              None)]
    for frame, result, fail in tests:
        dev[0].request("FLUSH")
        dev[1].request("FLUSH")
        sigma = start_sigma_dut(dev[0].ifname)
        try:
            run_sigma_dut_dpp_proto_stop_at_initiator(dev, frame, result, fail)
        finally:
            stop_sigma_dut(sigma)

def run_sigma_dut_dpp_proto_stop_at_initiator(dev, frame, result, fail):
    id0 = dev[1].dpp_bootstrap_gen(chan="81/6", mac=True)
    uri0 = dev[1].request("DPP_BOOTSTRAP_GET_URI %d" % id0)

    cmd = "DPP_LISTEN 2437 role=enrollee"
    if "OK" not in dev[1].request(cmd):
        raise Exception("Failed to start listen operation")

    res = sigma_dut_cmd("dev_exec_action,program,DPP,DPPActionType,SetPeerBootstrap,DPPBootstrappingdata,%s,DPPBS,QR" % to_hex(uri0))
    if "status,COMPLETE" not in res:
        raise Exception("dev_exec_action did not succeed: " + res)

    res = sigma_dut_cmd("dev_exec_action,program,DPP,DPPActionType,AutomaticDPP,DPPAuthRole,Initiator,DPPAuthDirection,Single,DPPProvisioningRole,Configurator,DPPConfIndex,1,DPPSigningKeyECC,P-256,DPPConfEnrolleeRole,STA,DPPBS,QR,DPPTimeout,6,DPPStep,Timeout,DPPFrameType,%s" % (frame))
    if result not in res:
        raise Exception("Unexpected result: " + res)
    if fail:
        ev = dev[1].wait_event(["DPP-FAIL"], timeout=5)
        if ev is None or fail not in ev:
            raise Exception("Failure not reported correctly: " + str(ev))

    dev[1].request("DPP_STOP_LISTEN")
    dev[0].dump_monitor()
    dev[1].dump_monitor()

def test_sigma_dut_dpp_proto_stop_at_initiator_enrollee(dev, apdev):
    """sigma_dut DPP protocol testing - Stop at TX on Initiator/Enrollee"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    tests = [("AuthenticationConfirm",
              "BootstrapResult,OK,AuthResult,Errorsent,LastFrameReceived,AuthenticationResponse",
              None)]
    for frame, result, fail in tests:
        dev[0].request("FLUSH")
        dev[1].request("FLUSH")
        sigma = start_sigma_dut(dev[0].ifname)
        try:
            run_sigma_dut_dpp_proto_stop_at_initiator_enrollee(dev, frame,
                                                               result, fail)
        finally:
            stop_sigma_dut(sigma)

def run_sigma_dut_dpp_proto_stop_at_initiator_enrollee(dev, frame, result,
                                                       fail):
    id0 = dev[1].dpp_bootstrap_gen(chan="81/6", mac=True)
    uri0 = dev[1].request("DPP_BOOTSTRAP_GET_URI %d" % id0)

    cmd = "DPP_LISTEN 2437 role=configurator"
    if "OK" not in dev[1].request(cmd):
        raise Exception("Failed to start listen operation")

    res = sigma_dut_cmd("dev_exec_action,program,DPP,DPPActionType,SetPeerBootstrap,DPPBootstrappingdata,%s,DPPBS,QR" % to_hex(uri0))
    if "status,COMPLETE" not in res:
        raise Exception("dev_exec_action did not succeed: " + res)

    res = sigma_dut_cmd("dev_exec_action,program,DPP,DPPActionType,AutomaticDPP,DPPAuthRole,Initiator,DPPAuthDirection,Single,DPPProvisioningRole,Enrollee,DPPBS,QR,DPPTimeout,6,DPPStep,Timeout,DPPFrameType,%s" % (frame), timeout=10)
    if result not in res:
        raise Exception("Unexpected result: " + res)
    if fail:
        ev = dev[1].wait_event(["DPP-FAIL"], timeout=5)
        if ev is None or fail not in ev:
            raise Exception("Failure not reported correctly: " + str(ev))

    dev[1].request("DPP_STOP_LISTEN")
    dev[0].dump_monitor()
    dev[1].dump_monitor()

def test_sigma_dut_dpp_proto_stop_at_responder(dev, apdev):
    """sigma_dut DPP protocol testing - Stop at RX on Responder"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    tests = [("AuthenticationRequest",
              "BootstrapResult,OK,AuthResult,Errorsent",
              None),
             ("AuthenticationConfirm",
              "BootstrapResult,OK,AuthResult,Errorsent",
              None)]
    for frame, result, fail in tests:
        dev[0].request("FLUSH")
        dev[1].request("FLUSH")
        sigma = start_sigma_dut(dev[0].ifname)
        try:
            run_sigma_dut_dpp_proto_stop_at_responder(dev, frame, result, fail)
        finally:
            stop_sigma_dut(sigma)

def run_sigma_dut_dpp_proto_stop_at_responder(dev, frame, result, fail):
    res = sigma_dut_cmd("dev_exec_action,program,DPP,DPPActionType,GetLocalBootstrap,DPPCryptoIdentifier,P-256,DPPBS,QR")
    if "status,COMPLETE" not in res:
        raise Exception("dev_exec_action did not succeed: " + res)
    hex = res.split(',')[3]
    uri = from_hex(hex)
    logger.info("URI from sigma_dut: " + uri)

    id1 = dev[1].dpp_qr_code(uri)

    t = threading.Thread(target=dpp_proto_init, args=(dev[1], id1))
    t.start()
    res = sigma_dut_cmd("dev_exec_action,program,DPP,DPPActionType,AutomaticDPP,DPPAuthRole,Responder,DPPAuthDirection,Single,DPPProvisioningRole,Enrollee,DPPConfIndex,1,DPPSigningKeyECC,P-256,DPPConfEnrolleeRole,STA,DPPBS,QR,DPPTimeout,6,DPPStep,Timeout,DPPFrameType,%s" % (frame), timeout=10)
    t.join()
    if result not in res:
        raise Exception("Unexpected result: " + res)
    if fail:
        ev = dev[1].wait_event(["DPP-FAIL"], timeout=5)
        if ev is None or fail not in ev:
            raise Exception("Failure not reported correctly:" + str(ev))

    dev[1].request("DPP_STOP_LISTEN")
    dev[0].dump_monitor()
    dev[1].dump_monitor()

def dpp_proto_init_pkex(dev):
    time.sleep(1)
    logger.info("Starting DPP PKEX initiator/configurator in a thread")
    cmd = "DPP_CONFIGURATOR_ADD"
    res = dev.request(cmd)
    if "FAIL" in res:
        raise Exception("Failed to add configurator")
    conf_id = int(res)

    id = dev.dpp_bootstrap_gen(type="pkex")

    cmd = "DPP_PKEX_ADD own=%d init=1 conf=sta-dpp configurator=%d code=secret" % (id, conf_id)
    if "FAIL" in dev.request(cmd):
        raise Exception("Failed to initiate DPP PKEX")

def test_sigma_dut_dpp_proto_initiator_pkex(dev, apdev):
    """sigma_dut DPP protocol testing - Initiator (PKEX)"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    tests = [("InvalidValue", "PKEXCRRequest", "WrappedData",
              "BootstrapResult,Errorsent",
              None),
             ("MissingAttribute", "PKEXExchangeRequest", "FiniteCyclicGroup",
              "BootstrapResult,Errorsent",
              "Missing or invalid Finite Cyclic Group attribute"),
             ("MissingAttribute", "PKEXCRRequest", "BSKey",
              "BootstrapResult,Errorsent",
              "No valid peer bootstrapping key found")]
    for step, frame, attr, result, fail in tests:
        dev[0].request("FLUSH")
        dev[1].request("FLUSH")
        sigma = start_sigma_dut(dev[0].ifname)
        try:
            run_sigma_dut_dpp_proto_initiator_pkex(dev, step, frame, attr,
                                                   result, fail)
        finally:
            stop_sigma_dut(sigma)

def run_sigma_dut_dpp_proto_initiator_pkex(dev, step, frame, attr, result, fail):
    id1 = dev[1].dpp_bootstrap_gen(type="pkex")

    cmd = "DPP_PKEX_ADD own=%d code=secret" % (id1)
    res = dev[1].request(cmd)
    if "FAIL" in res:
        raise Exception("Failed to set PKEX data (responder)")

    cmd = "DPP_LISTEN 2437 role=enrollee"
    if "OK" not in dev[1].request(cmd):
        raise Exception("Failed to start listen operation")

    res = sigma_dut_cmd("dev_exec_action,program,DPP,DPPActionType,AutomaticDPP,DPPAuthRole,Initiator,DPPAuthDirection,Single,DPPProvisioningRole,Configurator,DPPConfIndex,1,DPPSigningKeyECC,P-256,DPPConfEnrolleeRole,STA,DPPBS,PKEX,DPPPKEXCode,secret,DPPTimeout,6,DPPStep,%s,DPPFrameType,%s,DPPIEAttribute,%s" % (step, frame, attr))
    if result not in res:
        raise Exception("Unexpected result: " + res)
    if fail:
        ev = dev[1].wait_event(["DPP-FAIL"], timeout=5)
        if ev is None or fail not in ev:
            raise Exception("Failure not reported correctly: " + str(ev))

    dev[1].request("DPP_STOP_LISTEN")
    dev[0].dump_monitor()
    dev[1].dump_monitor()

def test_sigma_dut_dpp_proto_responder_pkex(dev, apdev):
    """sigma_dut DPP protocol testing - Responder (PKEX)"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    tests = [("InvalidValue", "PKEXCRResponse", "WrappedData",
              "BootstrapResult,Errorsent",
              None),
             ("MissingAttribute", "PKEXExchangeResponse", "DPPStatus",
              "BootstrapResult,Errorsent",
              "No DPP Status attribute"),
             ("MissingAttribute", "PKEXCRResponse", "BSKey",
              "BootstrapResult,Errorsent",
              "No valid peer bootstrapping key found")]
    for step, frame, attr, result, fail in tests:
        dev[0].request("FLUSH")
        dev[1].request("FLUSH")
        sigma = start_sigma_dut(dev[0].ifname)
        try:
            run_sigma_dut_dpp_proto_responder_pkex(dev, step, frame, attr,
                                                   result, fail)
        finally:
            stop_sigma_dut(sigma)

def run_sigma_dut_dpp_proto_responder_pkex(dev, step, frame, attr, result, fail):
    t = threading.Thread(target=dpp_proto_init_pkex, args=(dev[1],))
    t.start()
    res = sigma_dut_cmd("dev_exec_action,program,DPP,DPPActionType,AutomaticDPP,DPPAuthRole,Responder,DPPAuthDirection,Single,DPPProvisioningRole,Enrollee,DPPConfIndex,1,DPPSigningKeyECC,P-256,DPPConfEnrolleeRole,STA,DPPBS,PKEX,DPPPKEXCode,secret,DPPTimeout,6,DPPStep,%s,DPPFrameType,%s,DPPIEAttribute,%s" % (step, frame, attr), timeout=10)
    t.join()
    if result not in res:
        raise Exception("Unexpected result: " + res)
    if fail:
        ev = dev[1].wait_event(["DPP-FAIL"], timeout=5)
        if ev is None or fail not in ev:
            raise Exception("Failure not reported correctly:" + str(ev))

    dev[1].request("DPP_STOP_LISTEN")
    dev[0].dump_monitor()
    dev[1].dump_monitor()

def init_sigma_dut_dpp_proto_peer_disc_req(dev, apdev):
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    hapd = start_dpp_ap(apdev[0])
    dev[0].set("dpp_config_processing", "2")

    cmd = "DPP_CONFIGURATOR_ADD key=" + csign
    res = dev[1].request(cmd)
    if "FAIL" in res:
        raise Exception("Failed to add configurator")
    conf_id = int(res)

    id0 = dev[1].dpp_bootstrap_gen(chan="81/6", mac=True)
    uri0 = dev[1].request("DPP_BOOTSTRAP_GET_URI %d" % id0)

    dev[1].set("dpp_configurator_params",
               " conf=sta-dpp ssid=%s configurator=%d" % (to_hex("DPPNET01"),
                                                          conf_id))
    cmd = "DPP_LISTEN 2437 role=configurator"
    if "OK" not in dev[1].request(cmd):
        raise Exception("Failed to start listen operation")

    res = sigma_dut_cmd("dev_exec_action,program,DPP,DPPActionType,SetPeerBootstrap,DPPBootstrappingdata,%s,DPPBS,QR" % to_hex(uri0))
    if "status,COMPLETE" not in res:
        raise Exception("dev_exec_action did not succeed: " + res)

def test_sigma_dut_dpp_proto_peer_disc_req(dev, apdev):
    """sigma_dut DPP protocol testing - Peer Discovery Request"""
    sigma = start_sigma_dut(dev[0].ifname)
    try:
        init_sigma_dut_dpp_proto_peer_disc_req(dev, apdev)

        res = sigma_dut_cmd("dev_exec_action,program,DPP,DPPActionType,AutomaticDPP,DPPAuthRole,Initiator,DPPAuthDirection,Single,DPPProvisioningRole,Enrollee,DPPBS,QR,DPPTimeout,6,DPPWaitForConnect,Yes,DPPStep,MissingAttribute,DPPFrameType,PeerDiscoveryRequest,DPPIEAttribute,TransactionID", timeout=10)
        if "BootstrapResult,OK,AuthResult,OK,ConfResult,OK,NetworkIntroResult,Errorsent" not in res:
            raise Exception("Unexpected result: " + res)
    finally:
        dev[0].set("dpp_config_processing", "0", allow_fail=True)
        stop_sigma_dut(sigma)

def test_sigma_dut_dpp_self_config(dev, apdev):
    """sigma_dut DPP Configurator enrolling an AP and using self-configuration"""
    check_dpp_capab(dev[0])

    hapd = hostapd.add_ap(apdev[0], {"ssid": "unconfigured"})
    check_dpp_capab(hapd)

    sigma = start_sigma_dut(dev[0].ifname)
    try:
        dev[0].set("dpp_config_processing", "2")
        id = hapd.dpp_bootstrap_gen(chan="81/1", mac=True)
        uri = hapd.request("DPP_BOOTSTRAP_GET_URI %d" % id)

        res = sigma_dut_cmd("dev_exec_action,program,DPP,DPPActionType,SetPeerBootstrap,DPPBootstrappingdata,%s,DPPBS,QR" % to_hex(uri))
        if "status,COMPLETE" not in res:
            raise Exception("dev_exec_action did not succeed: " + res)

        res = sigma_dut_cmd("dev_exec_action,program,DPP,DPPActionType,AutomaticDPP,DPPAuthRole,Initiator,DPPAuthDirection,Single,DPPProvisioningRole,Configurator,DPPConfIndex,1,DPPSigningKeyECC,P-256,DPPConfEnrolleeRole,AP,DPPBS,QR,DPPTimeout,6")
        if "BootstrapResult,OK,AuthResult,OK,ConfResult,OK" not in res:
            raise Exception("Unexpected result: " + res)
        update_hapd_config(hapd)

        cmd = "dev_exec_action,program,DPP,DPPActionType,AutomaticDPP,DPPCryptoIdentifier,P-256,DPPBS,QR,DPPAuthRole,Initiator,DPPProvisioningRole,Configurator,DPPAuthDirection,Single,DPPConfIndex,1,DPPTimeout,6,DPPWaitForConnect,Yes,DPPSelfConfigure,Yes"
        res = sigma_dut_cmd(cmd, timeout=10)
        if "BootstrapResult,OK,AuthResult,OK,ConfResult,OK,NetworkIntroResult,OK,NetworkConnectResult,OK" not in res:
            raise Exception("Unexpected result: " + res)
    finally:
        stop_sigma_dut(sigma)
        dev[0].set("dpp_config_processing", "0")

def test_sigma_dut_ap_dpp_self_config(dev, apdev, params):
    """sigma_dut DPP AP Configurator using self-configuration"""
    logdir = os.path.join(params['logdir'],
                          "sigma_dut_ap_dpp_self_config.sigma-hostapd")
    with HWSimRadio() as (radio, iface):
        sigma = start_sigma_dut(iface, hostapd_logdir=logdir)
        try:
            run_sigma_dut_ap_dpp_self_config(dev, apdev)
        finally:
            stop_sigma_dut(sigma)
            dev[0].set("dpp_config_processing", "0", allow_fail=True)

def run_sigma_dut_ap_dpp_self_config(dev, apdev):
    check_dpp_capab(dev[0])

    sigma_dut_cmd_check("ap_reset_default,program,DPP")

    res = sigma_dut_cmd("dev_exec_action,program,DPP,DPPActionType,AutomaticDPP,DPPAuthRole,Initiator,DPPAuthDirection,Single,DPPProvisioningRole,Configurator,DPPConfEnrolleeRole,AP,DPPBS,QR,DPPConfIndex,1,DPPSelfConfigure,Yes,DPPTimeout,6", timeout=10)
    if "BootstrapResult,OK,AuthResult,OK,ConfResult,OK" not in res:
            raise Exception("Unexpected result: " + res)

    dev[0].set("dpp_config_processing", "2")

    id = dev[0].dpp_bootstrap_gen(chan="81/11", mac=True)
    uri = dev[0].request("DPP_BOOTSTRAP_GET_URI %d" % id)
    cmd = "DPP_LISTEN 2462 role=enrollee"
    if "OK" not in dev[0].request(cmd):
        raise Exception("Failed to start listen operation")

    res = sigma_dut_cmd("dev_exec_action,program,DPP,DPPActionType,SetPeerBootstrap,DPPBootstrappingdata,%s,DPPBS,QR" % to_hex(uri))
    if "status,COMPLETE" not in res:
        raise Exception("dev_exec_action did not succeed: " + res)
    cmd = "dev_exec_action,program,DPP,DPPActionType,AutomaticDPP,DPPAuthRole,Initiator,DPPAuthDirection,Single,DPPProvisioningRole,Configurator,DPPConfIndex,1,DPPSigningKeyECC,P-256,DPPConfEnrolleeRole,STA,DPPBS,QR,DPPTimeout,6"
    res = sigma_dut_cmd(cmd)
    if "BootstrapResult,OK,AuthResult,OK,ConfResult,OK" not in res:
        raise Exception("Unexpected result: " + res)
    dev[0].wait_connected(timeout=20)
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()
    sigma_dut_cmd_check("ap_reset_default")


def test_sigma_dut_ap_dpp_relay(dev, apdev, params):
    """sigma_dut DPP AP as Relay to Controller"""
    logdir = os.path.join(params['logdir'],
                          "sigma_dut_ap_dpp_relay.sigma-hostapd")
    with HWSimRadio() as (radio, iface):
        sigma = start_sigma_dut(iface, hostapd_logdir=logdir)
        try:
            run_sigma_dut_ap_dpp_relay(dev, apdev)
        finally:
            stop_sigma_dut(sigma)
            dev[1].request("DPP_CONTROLLER_STOP")

def run_sigma_dut_ap_dpp_relay(dev, apdev):
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])

    # Controller
    conf_id = dev[1].dpp_configurator_add()
    dev[1].set("dpp_configurator_params",
               " conf=sta-dpp configurator=%d" % conf_id)
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

    sigma_dut_cmd_check("ap_reset_default,program,DPP")
    sigma_dut_cmd_check("ap_preset_testparameters,program,DPP,DPPConfiguratorAddress,127.0.0.1,DPPConfiguratorPKHash," + pkhash)
    res = sigma_dut_cmd_check("dev_exec_action,program,DPP,DPPActionType,GetLocalBootstrap,DPPCryptoIdentifier,P-256,DPPBS,QR")

    dev[0].dpp_auth_init(uri=uri_c, role="enrollee")
    wait_auth_success(dev[1], dev[0], configurator=dev[1], enrollee=dev[0])

    sigma_dut_cmd_check("ap_reset_default")

def dpp_init_tcp_enrollee(dev, id1):
    logger.info("Starting DPP initiator/enrollee (TCP) in a thread")
    time.sleep(1)
    cmd = "DPP_AUTH_INIT peer=%d role=enrollee tcp_addr=127.0.0.1" % id1
    if "OK" not in dev.request(cmd):
        raise Exception("Failed to initiate DPP Authentication")
    ev = dev.wait_event(["DPP-CONF-RECEIVED"], timeout=5)
    if ev is None:
        raise Exception("DPP configuration not completed (Enrollee)")
    logger.info("DPP initiator/enrollee done")

def test_sigma_dut_dpp_tcp_conf_resp(dev, apdev):
    """sigma_dut DPP TCP Configurator (Controller) as responder"""
    run_sigma_dut_dpp_tcp_conf_resp(dev)

def run_sigma_dut_dpp_tcp_conf_resp(dev, status_query=False):
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    sigma = start_sigma_dut(dev[0].ifname)
    try:
        cmd = "dev_exec_action,program,DPP,DPPActionType,GetLocalBootstrap,DPPCryptoIdentifier,P-256,DPPBS,QR"
        res = sigma_dut_cmd(cmd)
        if "status,COMPLETE" not in res:
            raise Exception("dev_exec_action did not succeed: " + res)
        hex = res.split(',')[3]
        uri = from_hex(hex)
        logger.info("URI from sigma_dut: " + uri)

        id1 = dev[1].dpp_qr_code(uri)

        t = threading.Thread(target=dpp_init_tcp_enrollee, args=(dev[1], id1))
        t.start()
        cmd = "dev_exec_action,program,DPP,DPPActionType,AutomaticDPP,DPPAuthRole,Responder,DPPConfIndex,1,DPPAuthDirection,Single,DPPProvisioningRole,Configurator,DPPConfEnrolleeRole,STA,DPPSigningKeyECC,P-256,DPPBS,QR,DPPOverTCP,yes,DPPTimeout,6"
        if status_query:
            cmd += ",DPPStatusQuery,Yes"
        res = sigma_dut_cmd(cmd, timeout=10)
        t.join()
        if "BootstrapResult,OK,AuthResult,OK,ConfResult,OK" not in res:
            raise Exception("Unexpected result: " + res)
        if status_query and "StatusResult,0" not in res:
            raise Exception("Status query did not succeed: " + res)
    finally:
        stop_sigma_dut(sigma)

def dpp_init_tcp_configurator(dev, id1, conf_id):
    logger.info("Starting DPP initiator/configurator (TCP) in a thread")
    time.sleep(1)
    cmd = "DPP_AUTH_INIT peer=%d role=configurator conf=sta-dpp configurator=%d tcp_addr=127.0.0.1" % (id1, conf_id)
    if "OK" not in dev.request(cmd):
        raise Exception("Failed to initiate DPP Authentication")
    ev = dev.wait_event(["DPP-CONF-SENT"], timeout=5)
    if ev is None:
        raise Exception("DPP configuration not completed (Configurator)")
    logger.info("DPP initiator/configurator done")

def test_sigma_dut_dpp_tcp_enrollee_resp(dev, apdev):
    """sigma_dut DPP TCP Enrollee (Controller) as responder"""
    run_sigma_dut_dpp_tcp_enrollee_resp(dev)

def run_sigma_dut_dpp_tcp_enrollee_resp(dev, status_query=False):
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    sigma = start_sigma_dut(dev[0].ifname)
    try:
        cmd = "dev_exec_action,program,DPP,DPPActionType,GetLocalBootstrap,DPPCryptoIdentifier,P-256,DPPBS,QR"
        res = sigma_dut_cmd(cmd)
        if "status,COMPLETE" not in res:
            raise Exception("dev_exec_action did not succeed: " + res)
        hex = res.split(',')[3]
        uri = from_hex(hex)
        logger.info("URI from sigma_dut: " + uri)

        cmd = "DPP_CONFIGURATOR_ADD"
        res = dev[1].request(cmd)
        if "FAIL" in res:
            raise Exception("Failed to add configurator")
        conf_id = int(res)

        id1 = dev[1].dpp_qr_code(uri)

        t = threading.Thread(target=dpp_init_tcp_configurator, args=(dev[1], id1, conf_id))
        t.start()
        cmd = "dev_exec_action,program,DPP,DPPActionType,AutomaticDPP,DPPAuthRole,Responder,DPPAuthDirection,Single,DPPProvisioningRole,Enrollee,DPPSigningKeyECC,P-256,DPPBS,QR,DPPOverTCP,yes,DPPTimeout,6"
        if status_query:
            cmd += ",DPPStatusQuery,Yes"
        res = sigma_dut_cmd(cmd, timeout=10)
        t.join()
        if "BootstrapResult,OK,AuthResult,OK,ConfResult,OK" not in res:
            raise Exception("Unexpected result: " + res)
        if status_query and "StatusResult,0" not in res:
            raise Exception("Status query did not succeed: " + res)
    finally:
        stop_sigma_dut(sigma)

def test_sigma_dut_dpp_tcp_enrollee_init(dev, apdev):
    """sigma_dut DPP TCP Enrollee as initiator"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    sigma = start_sigma_dut(dev[0].ifname)
    try:
        # Controller
        conf_id = dev[1].dpp_configurator_add()
        dev[1].set("dpp_configurator_params",
                   " conf=sta-dpp configurator=%d" % conf_id)
        id_c = dev[1].dpp_bootstrap_gen()
        uri_c = dev[1].request("DPP_BOOTSTRAP_GET_URI %d" % id_c)
        if "OK" not in dev[1].request("DPP_CONTROLLER_START"):
            raise Exception("Failed to start Controller")

        res = sigma_dut_cmd("dev_exec_action,program,DPP,DPPActionType,SetPeerBootstrap,DPPBootstrappingdata,%s,DPPBS,QR" % to_hex(uri_c))
        if "status,COMPLETE" not in res:
            raise Exception("dev_exec_action did not succeed: " + res)

        cmd = "dev_exec_action,program,DPP,DPPActionType,AutomaticDPP,DPPAuthRole,Initiator,DPPAuthDirection,Single,DPPProvisioningRole,Enrollee,DPPConfEnrolleeRole,STA,DPPBS,QR,DPPOverTCP,127.0.0.1,DPPTimeout,6"
        res = sigma_dut_cmd(cmd, timeout=10)
        if "BootstrapResult,OK,AuthResult,OK,ConfResult,OK" not in res:
            raise Exception("Unexpected result: " + res)
    finally:
        stop_sigma_dut(sigma)
        dev[1].request("DPP_CONTROLLER_STOP")

def test_sigma_dut_ap_dpp_tcp_enrollee_init(dev, apdev, params):
    """sigma_dut DPP AP as TCP Enrollee/initiator"""
    logdir = params['prefix'] + ".sigma-hostapd"
    with HWSimRadio() as (radio, iface):
        sigma = start_sigma_dut(iface, hostapd_logdir=logdir)
        try:
            run_sigma_dut_ap_dpp_tcp_enrollee_init(dev, apdev)
        finally:
            stop_sigma_dut(sigma)
            dev[1].request("DPP_CONTROLLER_STOP")

def run_sigma_dut_ap_dpp_tcp_enrollee_init(dev, apdev):
    check_dpp_capab(dev[1])
    # Controller
    conf_id = dev[1].dpp_configurator_add()
    dev[1].set("dpp_configurator_params",
               "conf=ap-dpp configurator=%d" % conf_id)
    id_c = dev[1].dpp_bootstrap_gen()
    uri_c = dev[1].request("DPP_BOOTSTRAP_GET_URI %d" % id_c)
    if "OK" not in dev[1].request("DPP_CONTROLLER_START"):
        raise Exception("Failed to start Controller")

    sigma_dut_cmd_check("ap_reset_default,program,DPP")
    sigma_dut_cmd_check("ap_preset_testparameters,Program,DPP,NAME,AP,oper_chn,6")
    sigma_dut_cmd_check("dev_exec_action,program,DPP,DPPActionType,SetPeerBootstrap,DPPBootstrappingdata,%s,DPPBS,QR" % to_hex(uri_c))

    cmd = "dev_exec_action,program,DPP,DPPActionType,AutomaticDPP,DPPAuthRole,Initiator,DPPProvisioningRole,Enrollee,DPPBS,QR,DPPOverTCP,127.0.0.1,DPPTimeout,6"
    res = sigma_dut_cmd(cmd, timeout=10)
    if "BootstrapResult,OK,AuthResult,OK,ConfResult,OK" not in res:
        raise Exception("Unexpected result: " + res)
    sigma_dut_cmd_check("ap_reset_default")

def test_sigma_dut_dpp_tcp_enrollee_init_mutual(dev, apdev):
    """sigma_dut DPP TCP Enrollee as initiator with mutual authentication"""
    check_dpp_capab(dev[0], min_ver=2)
    check_dpp_capab(dev[1], min_ver=2)
    sigma = start_sigma_dut(dev[0].ifname)
    try:
        # Controller
        conf_id = dev[1].dpp_configurator_add()
        dev[1].set("dpp_configurator_params",
                   "conf=sta-dpp configurator=%d" % conf_id)
        id_c = dev[1].dpp_bootstrap_gen()
        uri_c = dev[1].request("DPP_BOOTSTRAP_GET_URI %d" % id_c)
        if "OK" not in dev[1].request("DPP_CONTROLLER_START"):
            raise Exception("Failed to start Controller")

        res = sigma_dut_cmd("dev_exec_action,program,DPP,DPPActionType,SetPeerBootstrap,DPPBootstrappingdata,%s,DPPBS,QR" % to_hex(uri_c))
        if "status,COMPLETE" not in res:
            raise Exception("dev_exec_action did not succeed: " + res)

        cmd = "dev_exec_action,program,DPP,DPPActionType,GetLocalBootstrap,DPPCryptoIdentifier,P-256,DPPBS,QR"
        res = sigma_dut_cmd_check(cmd)
        hex = res.split(',')[3]
        uri = from_hex(hex)
        logger.info("URI from sigma_dut: " + uri)
        id1 = dev[1].dpp_qr_code(uri)

        cmd = "dev_exec_action,program,DPP,DPPActionType,AutomaticDPP,DPPAuthRole,Initiator,DPPAuthDirection,Mutual,DPPProvisioningRole,Enrollee,DPPBS,QR,DPPOverTCP,127.0.0.1,DPPTimeout,6"
        res = sigma_dut_cmd(cmd, timeout=10)
        if "BootstrapResult,OK,AuthResult,OK,ConfResult,OK" not in res:
            raise Exception("Unexpected result: " + res)
    finally:
        stop_sigma_dut(sigma)
        dev[1].request("DPP_CONTROLLER_STOP")

def test_sigma_dut_dpp_tcp_configurator_init_mutual(dev, apdev):
    """sigma_dut DPP TCP Configurator as initiator with mutual authentication"""
    check_dpp_capab(dev[0], min_ver=2)
    check_dpp_capab(dev[1], min_ver=2)
    sigma = start_sigma_dut(dev[0].ifname)
    try:
        id_c = dev[1].dpp_bootstrap_gen()
        uri_c = dev[1].request("DPP_BOOTSTRAP_GET_URI %d" % id_c)
        if "OK" not in dev[1].request("DPP_CONTROLLER_START role=enrollee"):
            raise Exception("Failed to start Controller")

        res = sigma_dut_cmd("dev_exec_action,program,DPP,DPPActionType,SetPeerBootstrap,DPPBootstrappingdata,%s,DPPBS,QR" % to_hex(uri_c))
        if "status,COMPLETE" not in res:
            raise Exception("dev_exec_action did not succeed: " + res)

        cmd = "dev_exec_action,program,DPP,DPPActionType,GetLocalBootstrap,DPPCryptoIdentifier,P-256,DPPBS,QR"
        res = sigma_dut_cmd_check(cmd)
        hex = res.split(',')[3]
        uri = from_hex(hex)
        logger.info("URI from sigma_dut: " + uri)
        id1 = dev[1].dpp_qr_code(uri)

        cmd = "dev_exec_action,program,DPP,DPPActionType,AutomaticDPP,DPPAuthRole,Initiator,DPPAuthDirection,Mutual,DPPProvisioningRole,Configurator,DPPConfIndex,1,DPPConfEnrolleeRole,STA,DPPBS,QR,DPPOverTCP,127.0.0.1,DPPTimeout,6"
        res = sigma_dut_cmd(cmd, timeout=10)
        if "BootstrapResult,OK,AuthResult,OK,ConfResult,OK" not in res:
            raise Exception("Unexpected result: " + res)
    finally:
        stop_sigma_dut(sigma)
        dev[1].request("DPP_CONTROLLER_STOP")

def test_sigma_dut_dpp_nfc_handover_requestor_enrollee(dev, apdev):
    """sigma_dut DPP/NFC handover requestor as Enrollee"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    hapd = start_dpp_ap(apdev[0])
    sigma = start_sigma_dut(dev[0].ifname)
    try:
        dev[0].set("dpp_config_processing", "2")

        cmd = "DPP_CONFIGURATOR_ADD key=" + csign
        res = dev[1].request(cmd)
        if "FAIL" in res:
            raise Exception("Failed to add configurator")
        conf_id = int(res)
        dev[1].set("dpp_configurator_params",
                   " conf=sta-dpp ssid=%s configurator=%d" % (to_hex("DPPNET01"), conf_id))

        id_own = dev[1].dpp_bootstrap_gen(type="nfc-uri", chan="81/1,6,11",
                                          mac=True)
        uri_own = dev[1].request("DPP_BOOTSTRAP_GET_URI %d" % id_own)

        res = sigma_dut_cmd_check("dev_exec_action,program,DPP,DPPActionType,GetLocalBootstrap,DPPBS,NFC")
        hex = res.split(',')[3]
        uri_peer = from_hex(hex)
        logger.info("URI from sigma_dut: " + uri_peer)

        sigma_dut_cmd_check("dev_exec_action,program,DPP,DPPActionType,SetPeerBootstrap,DPPBootstrappingdata,%s,DPPBS,NFC" % to_hex(uri_own))

        res = dev[1].request("DPP_NFC_HANDOVER_REQ own=%d uri=%s" % (id_own,
                                                                     uri_peer))
        if "FAIL" in res:
            raise Exception("Failed to process NFC Handover Request")
        info = dev[1].request("DPP_BOOTSTRAP_INFO %d" % id_own)
        logger.info("Updated local bootstrapping info:\n" + info)
        freq = None
        for line in info.splitlines():
            if line.startswith("use_freq="):
                freq = int(line.split('=')[1])
        if freq is None:
            raise Exception("Selected channel not indicated")
        uri1 = dev[1].request("DPP_BOOTSTRAP_GET_URI %d" % id_own)
        logger.info("Updated URI[1]: " + uri1)
        dev[1].dpp_listen(freq, role="configurator")

        res = sigma_dut_cmd("dev_exec_action,program,DPP,DPPActionType,AutomaticDPP,DPPAuthRole,Initiator,DPPProvisioningRole,Enrollee,DPPBS,NFC,DPPNFCHandover,Negotiated_Requestor,DPPTimeout,6,DPPWaitForConnect,Yes", timeout=10)
        if "BootstrapResult,OK,AuthResult,OK,ConfResult,OK,NetworkIntroResult,OK,NetworkConnectResult,OK" not in res:
            raise Exception("Unexpected result: " + res)
    finally:
        dev[0].set("dpp_config_processing", "0")
        stop_sigma_dut(sigma)

def test_sigma_dut_dpp_nfc_handover_selector_enrollee(dev, apdev):
    """sigma_dut DPP/NFC handover selector as Enrollee"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    hapd = start_dpp_ap(apdev[0])
    sigma = start_sigma_dut(dev[0].ifname)
    try:
        dev[0].set("dpp_config_processing", "2")

        cmd = "DPP_CONFIGURATOR_ADD key=" + csign
        res = dev[1].request(cmd)
        if "FAIL" in res:
            raise Exception("Failed to add configurator")
        conf_id = int(res)
        dev[1].set("dpp_configurator_params",
                   " conf=sta-dpp ssid=%s configurator=%d" % (to_hex("DPPNET01"), conf_id))

        id_own = dev[1].dpp_bootstrap_gen(type="nfc-uri", chan="81/1,6,11",
                                          mac=True)
        uri_own = dev[1].request("DPP_BOOTSTRAP_GET_URI %d" % id_own)

        res = sigma_dut_cmd_check("dev_exec_action,program,DPP,DPPActionType,GetLocalBootstrap,DPPBS,NFC")
        hex = res.split(',')[3]
        uri_peer = from_hex(hex)
        logger.info("URI from sigma_dut: " + uri_peer)

        sigma_dut_cmd_check("dev_exec_action,program,DPP,DPPActionType,SetPeerBootstrap,DPPBootstrappingdata,%s,DPPBS,NFC" % to_hex(uri_own))

        res = dev[1].request("DPP_NFC_HANDOVER_SEL own=%d uri=%s" % (id_own,
                                                                     uri_peer))
        if "FAIL" in res:
            raise Exception("Failed to process NFC Handover Select")
        peer = int(res)
        dev[1].dpp_auth_init(peer=peer, own=id_own, configurator=conf_id,
                             conf="sta-dpp", ssid="DPPNET01")

        res = sigma_dut_cmd("dev_exec_action,program,DPP,DPPActionType,AutomaticDPP,DPPAuthRole,Initiator,DPPProvisioningRole,Enrollee,DPPBS,NFC,DPPNFCHandover,Negotiated_Selector,DPPTimeout,6,DPPWaitForConnect,Yes", timeout=10)
        if "BootstrapResult,OK,AuthResult,OK,ConfResult,OK,NetworkIntroResult,OK,NetworkConnectResult,OK" not in res:
            raise Exception("Unexpected result: " + res)
    finally:
        dev[0].set("dpp_config_processing", "0")
        stop_sigma_dut(sigma)

def test_sigma_dut_dpp_nfc_static_read_enrollee(dev, apdev):
    """sigma_dut DPP/NFC read tag as Enrollee"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    hapd = start_dpp_ap(apdev[0])
    sigma = start_sigma_dut(dev[0].ifname)
    try:
        dev[0].set("dpp_config_processing", "2")

        cmd = "DPP_CONFIGURATOR_ADD key=" + csign
        res = dev[1].request(cmd)
        if "FAIL" in res:
            raise Exception("Failed to add configurator")
        conf_id = int(res)
        dev[1].set("dpp_configurator_params",
                   " conf=sta-dpp ssid=%s configurator=%d" % (to_hex("DPPNET01"), conf_id))

        id_own = dev[1].dpp_bootstrap_gen(type="nfc-uri", chan="81/6", mac=True)
        uri_own = dev[1].request("DPP_BOOTSTRAP_GET_URI %d" % id_own)

        sigma_dut_cmd_check("dev_exec_action,program,DPP,DPPActionType,SetPeerBootstrap,DPPBootstrappingdata,%s,DPPBS,NFC" % to_hex(uri_own))
        dev[1].dpp_listen(2437, role="configurator")

        res = sigma_dut_cmd("dev_exec_action,program,DPP,DPPActionType,AutomaticDPP,DPPAuthRole,Initiator,DPPProvisioningRole,Enrollee,DPPBS,NFC,DPPNFCHandover,Static,DPPTimeout,6,DPPWaitForConnect,Yes", timeout=10)
        if "BootstrapResult,OK,AuthResult,OK,ConfResult,OK,NetworkIntroResult,OK,NetworkConnectResult,OK" not in res:
            raise Exception("Unexpected result: " + res)
    finally:
        dev[0].set("dpp_config_processing", "0")
        stop_sigma_dut(sigma)

def test_sigma_dut_dpp_nfc_static_write_enrollee(dev, apdev):
    """sigma_dut DPP/NFC write tag as Enrollee"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    hapd = start_dpp_ap(apdev[0])
    sigma = start_sigma_dut(dev[0].ifname)
    try:
        dev[0].set("dpp_config_processing", "2")

        cmd = "DPP_CONFIGURATOR_ADD key=" + csign
        res = dev[1].request(cmd)
        if "FAIL" in res:
            raise Exception("Failed to add configurator")
        conf_id = int(res)
        dev[1].set("dpp_configurator_params",
                   " conf=sta-dpp ssid=%s configurator=%d" % (to_hex("DPPNET01"), conf_id))

        res = sigma_dut_cmd_check("dev_exec_action,program,DPP,DPPActionType,GetLocalBootstrap,DPPBS,NFC")
        hex = res.split(',')[3]
        uri_peer = from_hex(hex)
        logger.info("URI from sigma_dut: " + uri_peer)

        dev[1].dpp_auth_init(nfc_uri=uri_peer, configurator=conf_id,
                             conf="sta-dpp", ssid="DPPNET01")

        res = sigma_dut_cmd("dev_exec_action,program,DPP,DPPActionType,AutomaticDPP,DPPAuthRole,Responder,DPPProvisioningRole,Enrollee,DPPBS,NFC,DPPNFCHandover,Static,DPPTimeout,6,DPPWaitForConnect,Yes", timeout=10)
        if "BootstrapResult,OK,AuthResult,OK,ConfResult,OK,NetworkIntroResult,OK,NetworkConnectResult,OK" not in res:
            raise Exception("Unexpected result: " + res)
    finally:
        dev[0].set("dpp_config_processing", "0")
        stop_sigma_dut(sigma)

def test_sigma_dut_dpp_reconfig_enrollee(dev, apdev):
    """sigma_dut DPP reconfiguration (Enrollee)"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    hapd = start_dpp_ap(apdev[0])
    sigma = start_sigma_dut(dev[0].ifname)
    try:
        cmd = "DPP_CONFIGURATOR_ADD key=" + csign
        res = dev[1].request(cmd)
        if "FAIL" in res:
            raise Exception("Failed to add configurator")
        conf_id = int(res)

        id0 = dev[1].dpp_bootstrap_gen(chan="81/6", mac=True)
        uri0 = dev[1].request("DPP_BOOTSTRAP_GET_URI %d" % id0)

        dev[1].set("dpp_configurator_params",
                   " conf=sta-dpp ssid=%s configurator=%d" % (to_hex("DPPNET01"), conf_id))
        cmd = "DPP_LISTEN 2437 role=configurator"
        if "OK" not in dev[1].request(cmd):
            raise Exception("Failed to start listen operation")

        ifname = dev[0].ifname
        sigma_dut_cmd_check("sta_reset_default,interface,%s,prog,DPP" % ifname)

        res = sigma_dut_cmd("dev_exec_action,program,DPP,DPPActionType,SetPeerBootstrap,DPPBootstrappingdata,%s,DPPBS,QR" % to_hex(uri0))
        if "status,COMPLETE" not in res:
            raise Exception("dev_exec_action did not succeed: " + res)

        res = sigma_dut_cmd("dev_exec_action,program,DPP,DPPActionType,AutomaticDPP,DPPAuthRole,Initiator,DPPAuthDirection,Single,DPPProvisioningRole,Enrollee,DPPBS,QR,DPPTimeout,6,DPPWaitForConnect,Yes", timeout=10)
        if "BootstrapResult,OK,AuthResult,OK,ConfResult,OK,NetworkIntroResult,OK,NetworkConnectResult,OK" not in res:
            raise Exception("Unexpected result: " + res)

        hapd.disable()
        dev[0].dump_monitor()

        ssid = "reconfig"
        passphrase = "secret passphrase"
        params = hostapd.wpa2_params(ssid=ssid, passphrase=passphrase)
        hapd = hostapd.add_ap(apdev[0], params)

        dev[1].set("dpp_configurator_params",
                   "conf=sta-psk ssid=%s pass=%s conn_status=1" % (binascii.hexlify(ssid.encode()).decode(), binascii.hexlify(passphrase.encode()).decode()))
        cmd = "DPP_LISTEN 2437 role=configurator"
        if "OK" not in dev[1].request(cmd):
            raise Exception("Failed to start listen operation")
        dev[1].dump_monitor()

        res = sigma_dut_cmd("dev_exec_action,program,DPP,DPPActionType,DPPReconfigure,DPPTimeout,6,DPPWaitForConnect,Yes", timeout=10)
        if "status,COMPLETE,ReconfigAuthResult,OK,ConfResult,OK,NetworkConnectResult,OK" not in res:
            raise Exception("Unexpected reconfiguration result: " + res)

        ev = dev[1].wait_event(["DPP-CONF-SENT"], timeout=15)
        if ev is None:
            raise Exception("DPP Config Response (reconfig) not transmitted")

        dev[0].wait_connected(timeout=20)
        ev = dev[1].wait_event(["DPP-CONN-STATUS-RESULT"], timeout=20)
        if ev is None:
            raise Exception("No connection status reported")
        if "result=0" not in ev:
            raise Exception("Connection status did not report success: " + ev)

        time.sleep(1)
        cmd = "DPP_LISTEN 2437 role=configurator"
        if "OK" not in dev[1].request(cmd):
            raise Exception("Failed to start listen operation")
        dev[0].dump_monitor()
        dev[1].dump_monitor()

        res = sigma_dut_cmd("dev_exec_action,program,DPP,DPPActionType,DPPReconfigure,DPPTimeout,6,DPPWaitForConnect,Yes", timeout=30)
        if "status,COMPLETE,ReconfigAuthResult,OK,ConfResult,OK,NetworkConnectResult,OK" not in res:
            raise Exception("Unexpected reconfiguration [2] result: " + res)

        ev = dev[1].wait_event(["DPP-CONF-SENT"], timeout=5)
        if ev is None:
            raise Exception("DPP Config Response (reconfig) not transmitted [2]")

        dev[0].wait_connected(timeout=20)
    finally:
        dev[0].set("dpp_config_processing", "0")
        stop_sigma_dut(sigma)

def test_sigma_dut_dpp_reconfig_configurator(dev, apdev):
    """sigma_dut DPP reconfiguration (Configurator)"""
    check_dpp_capab(dev[0])
    check_dpp_capab(dev[1])
    sigma = start_sigma_dut(dev[0].ifname)
    try:
        dev[1].set("dpp_config_processing", "1")
        id0 = dev[1].dpp_bootstrap_gen(chan="81/6", mac=True)
        uri0 = dev[1].request("DPP_BOOTSTRAP_GET_URI %d" % id0)
        cmd = "DPP_LISTEN 2437"
        if "OK" not in dev[1].request(cmd):
            raise Exception("Failed to start listen operation")

        ifname = dev[0].ifname
        sigma_dut_cmd_check("sta_reset_default,interface,%s,prog,DPP" % ifname)

        res = sigma_dut_cmd("dev_exec_action,program,DPP,DPPActionType,SetPeerBootstrap,DPPBootstrappingdata,%s,DPPBS,QR" % to_hex(uri0))
        if "status,COMPLETE" not in res:
            raise Exception("dev_exec_action did not succeed: " + res)

        res = sigma_dut_cmd("dev_exec_action,program,DPP,DPPActionType,AutomaticDPP,DPPAuthRole,Initiator,DPPAuthDirection,Single,DPPProvisioningRole,Configurator,DPPConfEnrolleeRole,STA,DPPSigningKeyECC,P-256,DPPConfIndex,1,DPPBS,QR,DPPTimeout,6", timeout=10)
        if "BootstrapResult,OK,AuthResult,OK,ConfResult,OK" not in res:
            raise Exception("Unexpected result: " + res)

        dev[0].dump_monitor()

        ev = dev[1].wait_event(["DPP-NETWORK-ID"], timeout=1)
        if ev is None:
            raise Exception("No network profile created")
        id = int(ev.split(' ')[1])

        ev = dev[1].wait_event(["DPP-TX-STATUS"], timeout=5)
        if ev is None:
            raise Exception("Configuration Result not sent")
        dev[1].dump_monitor()
        cmd = "DPP_RECONFIG %d" % id
        if "OK" not in dev[1].request(cmd):
            raise Exception("Failed to start reconfiguration")

        res = sigma_dut_cmd("dev_exec_action,program,DPP,DPPActionType,DPPReconfigure,DPPProvisioningRole,Configurator,DPPConfEnrolleeRole,STA,DPPSigningKeyECC,P-256,DPPConfIndex,2,DPPListenChannel,6,DPPTimeout,6", timeout=10)
        if "status,COMPLETE,ReconfigAuthResult,OK,ConfResult,OK" not in res:
            raise Exception("Unexpected reconfiguration result: " + res)

        ev = dev[1].wait_event(["DPP-CONF-RECEIVED"], timeout=15)
        if ev is None:
            raise Exception("DPP Config Response (reconfig) not received")
    finally:
        dev[0].set("dpp_config_processing", "0")
        dev[1].set("dpp_config_processing", "0")
        stop_sigma_dut(sigma)

@reset_ignore_old_scan_res
def test_sigma_dut_preconfigured_profile(dev, apdev):
    """sigma_dut controlled connection using preconfigured profile"""
    ifname = dev[0].ifname
    sigma = start_sigma_dut(ifname)

    try:
        params = hostapd.wpa2_params(ssid="test-psk", passphrase="12345678")
        hapd = hostapd.add_ap(apdev[0], params)
        dev[0].connect("test-psk", psk="12345678", scan_freq="2412",
                       only_add_network=True)

        sigma_dut_cmd_check("sta_set_ip_config,interface,%s,dhcp,0,ip,127.0.0.11,mask,255.255.255.0" % ifname)
        sigma_dut_cmd_check("sta_associate,interface,%s,ssid,%s" % (ifname, "test-psk"),
                            timeout=10)
        sigma_dut_wait_connected(ifname)
        sigma_dut_cmd_check("sta_get_ip_config,interface," + ifname)
        sigma_dut_cmd_check("sta_disconnect,interface," + ifname)
        sigma_dut_cmd_check("sta_reset_default,interface," + ifname)
    finally:
        stop_sigma_dut(sigma)

@reset_ignore_old_scan_res
def test_sigma_dut_wps_pbc(dev, apdev):
    """sigma_dut and WPS PBC Enrollee"""
    ssid = "test-wps-conf"
    hapd = hostapd.add_ap(apdev[0],
                          {"ssid": "wps", "eap_server": "1", "wps_state": "2",
                           "wpa_passphrase": "12345678", "wpa": "2",
                           "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP"})
    hapd.request("WPS_PBC")

    ifname = dev[0].ifname
    sigma = start_sigma_dut(ifname)

    try:
        cmd = "start_wps_registration,interface,%s" % ifname
        cmd += ",WpsRole,Enrollee"
        cmd += ",WpsConfigMethod,PBC"
        sigma_dut_cmd_check(cmd, timeout=15)

        sigma_dut_cmd_check("sta_disconnect,interface," + ifname)
        hapd.disable()
        sigma_dut_cmd_check("sta_reset_default,interface," + ifname)
    finally:
        stop_sigma_dut(sigma)
    dev[0].flush_scan_cache()

def test_sigma_dut_sta_scan_bss(dev, apdev):
    """sigma_dut sta_scan_bss"""
    hapd = hostapd.add_ap(apdev[0], {"ssid": "test"})
    sigma = start_sigma_dut(dev[0].ifname)
    try:
        cmd = "sta_scan_bss,Interface,%s,BSSID,%s" % (dev[0].ifname, \
                                                      hapd.own_addr())
        res = sigma_dut_cmd(cmd, timeout=10)
        if "ssid,test,bsschannel,1" not in res:
            raise Exception("Unexpected result: " + res)
    finally:
        stop_sigma_dut(sigma)

def test_sigma_dut_sta_scan_ssid_bssid(dev, apdev):
    """sigma_dut sta_scan GetParameter,SSID_BSSID"""
    hostapd.add_ap(apdev[0], {"ssid": "abcdef"})
    hostapd.add_ap(apdev[1], {"ssid": "qwerty"})
    sigma = start_sigma_dut(dev[0].ifname)
    try:
        cmd = "sta_scan,Interface,%s,GetParameter,SSID_BSSID" % dev[0].ifname
        res = sigma_dut_cmd(cmd, timeout=10)
        if "abcdef" not in res or "qwerty" not in res:
            raise Exception("Unexpected result: " + res)
    finally:
        stop_sigma_dut(sigma)

def test_sigma_dut_sta_scan_short_ssid(dev, apdev):
    """sigma_dut sta_scan ShortSSID"""
    dev[0].flush_scan_cache()
    ssid = "test-short-ssid-list"
    hapd = hostapd.add_ap(apdev[0], {"ssid": ssid,
                                     "ignore_broadcast_ssid": "1"})
    bssid = apdev[0]['bssid']
    payload = struct.pack('>L', binascii.crc32(ssid.encode()))
    val = binascii.hexlify(payload).decode()
    sigma = start_sigma_dut(dev[0].ifname)
    found = False
    try:
        cmd = "sta_scan,Interface,%s,ChnlFreq,2412,ShortSSID,%s" % (dev[0].ifname, val)
        for i in range(10):
            sigma_dut_cmd_check(cmd, timeout=5)
            ev = dev[0].wait_event(["CTRL-EVENT-SCAN-RESULTS"])
            if ev is None:
                raise Exception("Scan did not complete")
            if bssid in dev[0].request("SCAN_RESULTS"):
                found = True
                break
    finally:
        stop_sigma_dut(sigma)
        dev[0].request("VENDOR_ELEM_REMOVE 14 *")

    if not found:
        raise Exception("AP not found in scan results")

def test_sigma_dut_sta_scan_wait_completion(dev, apdev):
    """sigma_dut sta_scan WaitCompletion,1"""
    sigma = start_sigma_dut(dev[0].ifname)
    try:
        cmd = "sta_scan,Interface,%s,ChnlFreq,2412,WaitCompletion,1" % dev[0].ifname
        res = sigma_dut_cmd(cmd, timeout=10)
    finally:
        stop_sigma_dut(sigma)

def test_sigma_dut_ap_osen(dev, apdev, params):
    """sigma_dut controlled AP with OSEN"""
    logdir = os.path.join(params['logdir'],
                          "sigma_dut_ap_osen.sigma-hostapd")
    with HWSimRadio() as (radio, iface):
        sigma = start_sigma_dut(iface, hostapd_logdir=logdir)
        try:
            sigma_dut_cmd_check("ap_reset_default")
            sigma_dut_cmd_check("ap_set_wireless,NAME,AP,CHANNEL,1,SSID,test-hs20,MODE,11ng")
            sigma_dut_cmd_check("ap_set_radius,NAME,AP,IPADDR,127.0.0.1,PORT,1812,PASSWORD,radius")
            sigma_dut_cmd_check("ap_set_security,NAME,AP,KEYMGNT,OSEN,PMF,Optional")
            sigma_dut_cmd_check("ap_config_commit,NAME,AP")

            # RSN-OSEN (for OSU)
            dev[0].connect("test-hs20", proto="OSEN", key_mgmt="OSEN",
                           pairwise="CCMP", group="GTK_NOT_USED",
                           eap="WFA-UNAUTH-TLS", identity="osen@example.com",
                           ca_cert="auth_serv/ca.pem", scan_freq="2412")

            sigma_dut_cmd_check("ap_reset_default")
        finally:
            stop_sigma_dut(sigma)

def test_sigma_dut_ap_eap_osen(dev, apdev, params):
    """sigma_dut controlled AP with EAP+OSEN"""
    logdir = os.path.join(params['logdir'],
                          "sigma_dut_ap_eap_osen.sigma-hostapd")
    with HWSimRadio() as (radio, iface):
        sigma = start_sigma_dut(iface, bridge="ap-br0", hostapd_logdir=logdir)
        try:
            sigma_dut_cmd_check("ap_reset_default")
            sigma_dut_cmd_check("ap_set_wireless,NAME,AP,CHANNEL,1,SSID,test-hs20,MODE,11ng")
            sigma_dut_cmd_check("ap_set_radius,NAME,AP,IPADDR,127.0.0.1,PORT,1812,PASSWORD,radius")
            sigma_dut_cmd_check("ap_set_security,NAME,AP,KEYMGNT,WPA2-ENT-OSEN,PMF,Optional")
            sigma_dut_cmd_check("ap_config_commit,NAME,AP")

            subprocess.call(['brctl', 'setfd', 'ap-br0', '0'])
            subprocess.call(['ip', 'link', 'set', 'dev', 'ap-br0', 'up'])

            # RSN-OSEN (for OSU)
            dev[0].connect("test-hs20", proto="OSEN", key_mgmt="OSEN",
                           pairwise="CCMP",
                           eap="WFA-UNAUTH-TLS", identity="osen@example.com",
                           ca_cert="auth_serv/ca.pem", ieee80211w='2',
                           scan_freq="2412")
            # RSN-EAP (for data connection)
            dev[1].connect("test-hs20", key_mgmt="WPA-EAP", eap="TTLS",
                           identity="hs20-test", password="password",
                           ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2",
                           ieee80211w='2', scan_freq="2412")

            hwsim_utils.test_connectivity(dev[0], dev[1], broadcast=False,
                                          success_expected=False, timeout=1)

            sigma_dut_cmd_check("ap_reset_default")
        finally:
            stop_sigma_dut(sigma)
            subprocess.call(['ip', 'link', 'set', 'dev', 'ap-br0', 'down'],
                            stderr=open('/dev/null', 'w'))
            subprocess.call(['brctl', 'delbr', 'ap-br0'],
                            stderr=open('/dev/null', 'w'))

def test_sigma_dut_ap_eap(dev, apdev, params):
    """sigma_dut controlled AP WPA2-Enterprise"""
    logdir = os.path.join(params['logdir'], "sigma_dut_ap_eap.sigma-hostapd")
    with HWSimRadio() as (radio, iface):
        sigma = start_sigma_dut(iface, hostapd_logdir=logdir)
        try:
            sigma_dut_cmd_check("ap_reset_default")
            sigma_dut_cmd_check("ap_set_wireless,NAME,AP,CHANNEL,1,SSID,test-eap,MODE,11ng")
            sigma_dut_cmd_check("ap_set_radius,NAME,AP,IPADDR,127.0.0.1,PORT,1812,PASSWORD,radius")
            sigma_dut_cmd_check("ap_set_security,NAME,AP,KEYMGNT,WPA2-ENT")
            sigma_dut_cmd_check("ap_config_commit,NAME,AP")

            dev[0].connect("test-eap", key_mgmt="WPA-EAP", eap="GPSK",
                           identity="gpsk user",
                           password="abcdefghijklmnop0123456789abcdef",
                           scan_freq="2412")

            sigma_dut_cmd_check("ap_reset_default")
        finally:
            stop_sigma_dut(sigma)

def test_sigma_dut_ap_eap_sha256(dev, apdev, params):
    """sigma_dut controlled AP WPA2-Enterprise SHA256"""
    logdir = os.path.join(params['logdir'],
                          "sigma_dut_ap_eap_sha256.sigma-hostapd")
    with HWSimRadio() as (radio, iface):
        sigma = start_sigma_dut(iface, hostapd_logdir=logdir)
        try:
            sigma_dut_cmd_check("ap_reset_default")
            sigma_dut_cmd_check("ap_set_wireless,NAME,AP,CHANNEL,1,SSID,test-eap,MODE,11ng")
            sigma_dut_cmd_check("ap_set_radius,NAME,AP,IPADDR,127.0.0.1,PORT,1812,PASSWORD,radius")
            sigma_dut_cmd_check("ap_set_security,NAME,AP,KEYMGNT,WPA2-ENT-256")
            sigma_dut_cmd_check("ap_config_commit,NAME,AP")

            dev[0].connect("test-eap", key_mgmt="WPA-EAP-SHA256", eap="GPSK",
                           identity="gpsk user",
                           password="abcdefghijklmnop0123456789abcdef",
                           scan_freq="2412")

            sigma_dut_cmd_check("ap_reset_default")
        finally:
            stop_sigma_dut(sigma)

def test_sigma_dut_ap_ft_eap(dev, apdev, params):
    """sigma_dut controlled AP FT-EAP"""
    logdir = os.path.join(params['logdir'], "sigma_dut_ap_ft_eap.sigma-hostapd")
    with HWSimRadio() as (radio, iface):
        sigma = start_sigma_dut(iface, hostapd_logdir=logdir)
        try:
            sigma_dut_cmd_check("ap_reset_default")
            sigma_dut_cmd_check("ap_set_wireless,NAME,AP,CHANNEL,1,SSID,test-ft-eap,MODE,11ng,DOMAIN,0101,FT_OA,Enable")
            sigma_dut_cmd_check("ap_set_radius,NAME,AP,IPADDR,127.0.0.1,PORT,1812,PASSWORD,radius")
            sigma_dut_cmd_check("ap_set_security,NAME,AP,KEYMGNT,FT-EAP")
            sigma_dut_cmd_check("ap_config_commit,NAME,AP")

            dev[0].connect("test-ft-eap", key_mgmt="FT-EAP", eap="GPSK",
                           identity="gpsk user",
                           password="abcdefghijklmnop0123456789abcdef",
                           scan_freq="2412")

            sigma_dut_cmd_check("ap_reset_default")
        finally:
            stop_sigma_dut(sigma)

def test_sigma_dut_ap_ft_psk(dev, apdev, params):
    """sigma_dut controlled AP FT-PSK"""
    logdir = os.path.join(params['logdir'], "sigma_dut_ap_ft_psk.sigma-hostapd")
    with HWSimRadio() as (radio, iface):
        sigma = start_sigma_dut(iface, hostapd_logdir=logdir)
        try:
            sigma_dut_cmd_check("ap_reset_default")
            sigma_dut_cmd_check("ap_set_wireless,NAME,AP,CHANNEL,1,SSID,test-ft-psk,MODE,11ng,DOMAIN,0101,FT_OA,Enable")
            sigma_dut_cmd_check("ap_set_security,NAME,AP,KEYMGNT,FT-PSK,PSK,12345678")
            sigma_dut_cmd_check("ap_config_commit,NAME,AP")

            dev[0].connect("test-ft-psk", key_mgmt="FT-PSK", psk="12345678",
                           scan_freq="2412")

            sigma_dut_cmd_check("ap_reset_default")
        finally:
            stop_sigma_dut(sigma)

def test_sigma_dut_ap_ft_over_ds_psk(dev, apdev, params):
    """sigma_dut controlled AP FT-PSK (over-DS)"""
    logdir = os.path.join(params['logdir'],
                          "sigma_dut_ap_ft_over_ds_psk.sigma-hostapd")
    conffile = os.path.join(params['logdir'],
                            "sigma_dut_ap_ft_over_ds_psk.sigma-conf")
    with HWSimRadio() as (radio, iface):
        sigma = start_sigma_dut(iface, hostapd_logdir=logdir)
        try:
            sigma_dut_cmd_check("ap_reset_default")
            sigma_dut_cmd_check("ap_set_wireless,NAME,AP,CHANNEL,1,SSID,test-ft-psk,MODE,11ng,DOMAIN,0101,FT_DS,Enable")
            sigma_dut_cmd_check("ap_set_security,NAME,AP,KEYMGNT,FT-PSK,PSK,12345678")
            sigma_dut_cmd_check("ap_config_commit,NAME,AP")

            with open("/tmp/sigma_dut-ap.conf", "rb") as f:
                with open(conffile, "wb") as f2:
                    f2.write(f.read())

            dev[0].connect("test-ft-psk", key_mgmt="FT-PSK", psk="12345678",
                           scan_freq="2412")

            sigma_dut_cmd_check("ap_reset_default")
        finally:
            stop_sigma_dut(sigma)

def test_sigma_dut_ap_ent_ft_eap(dev, apdev, params):
    """sigma_dut controlled AP WPA-EAP and FT-EAP"""
    logdir = os.path.join(params['logdir'],
                          "sigma_dut_ap_ent_ft_eap.sigma-hostapd")
    with HWSimRadio() as (radio, iface):
        sigma = start_sigma_dut(iface, hostapd_logdir=logdir)
        try:
            sigma_dut_cmd_check("ap_reset_default")
            sigma_dut_cmd_check("ap_set_wireless,NAME,AP,CHANNEL,1,SSID,test-ent-ft-eap,MODE,11ng,DOMAIN,0101,FT_OA,Enable")
            sigma_dut_cmd_check("ap_set_radius,NAME,AP,IPADDR,127.0.0.1,PORT,1812,PASSWORD,radius")
            sigma_dut_cmd_check("ap_set_security,NAME,AP,KEYMGNT,WPA2-ENT-FT-EAP")
            sigma_dut_cmd_check("ap_config_commit,NAME,AP")

            dev[0].connect("test-ent-ft-eap", key_mgmt="FT-EAP", eap="GPSK",
                           identity="gpsk user",
                           password="abcdefghijklmnop0123456789abcdef",
                           scan_freq="2412")
            dev[1].connect("test-ent-ft-eap", key_mgmt="WPA-EAP", eap="GPSK",
                           identity="gpsk user",
                           password="abcdefghijklmnop0123456789abcdef",
                           scan_freq="2412")

            sigma_dut_cmd_check("ap_reset_default")
        finally:
            stop_sigma_dut(sigma)

@reset_ignore_old_scan_res
def test_sigma_dut_venue_url(dev, apdev):
    """sigma_dut controlled Venue URL fetch"""
    ifname = dev[0].ifname
    sigma = start_sigma_dut(ifname)

    try:
        ssid = "venue"
        params = hostapd.wpa2_params(ssid=ssid, passphrase="12345678")
        params["wpa_key_mgmt"] = "WPA-PSK-SHA256"
        params["ieee80211w"] = "2"

        venue_group = 1
        venue_type = 13
        venue_info = struct.pack('BB', venue_group, venue_type)
        lang1 = "eng"
        name1 = "Example venue"
        lang2 = "fin"
        name2 = "Esimerkkipaikka"
        venue1 = struct.pack('B', len(lang1 + name1)) + lang1.encode() + name1.encode()
        venue2 = struct.pack('B', len(lang2 + name2)) + lang2.encode() + name2.encode()
        venue_name = binascii.hexlify(venue_info + venue1 + venue2)

        url1 = "http://example.com/venue"
        url2 = "https://example.org/venue-info/"
        params["venue_group"] = str(venue_group)
        params["venue_type"] = str(venue_type)
        params["venue_name"] = [lang1 + ":" + name1, lang2 + ":" + name2]
        params["venue_url"] = ["1:" + url1, "2:" + url2]

        hapd = hostapd.add_ap(apdev[0], params)

        sigma_dut_cmd_check("sta_reset_default,interface,%s,prog,PMF" % ifname)
        sigma_dut_cmd_check("sta_set_ip_config,interface,%s,dhcp,0,ip,127.0.0.11,mask,255.255.255.0" % ifname)
        sigma_dut_cmd_check("sta_set_psk,interface,%s,ssid,%s,passphrase,%s,encpType,aes-ccmp,keymgmttype,wpa2,PMF,Required" % (ifname, "venue", "12345678"))
        sigma_dut_cmd_check("sta_associate,interface,%s,ssid,%s,channel,1" % (ifname, "venue"),
                            timeout=10)
        sigma_dut_wait_connected(ifname)
        sigma_dut_cmd_check("sta_get_ip_config,interface," + ifname)
        sigma_dut_cmd_check("sta_hs2_venue_info,interface," + ifname + ",Display,Yes")
        sigma_dut_cmd_check("sta_disconnect,interface," + ifname)
        sigma_dut_cmd_check("sta_reset_default,interface," + ifname)
    finally:
        stop_sigma_dut(sigma)

def test_sigma_dut_hs20_assoc_24(dev, apdev):
    """sigma_dut controlled Hotspot 2.0 connection (2.4 GHz)"""
    run_sigma_dut_hs20_assoc(dev, apdev, True)

def test_sigma_dut_hs20_assoc_5(dev, apdev):
    """sigma_dut controlled Hotspot 2.0 connection (5 GHz)"""
    run_sigma_dut_hs20_assoc(dev, apdev, False)

def run_sigma_dut_hs20_assoc(dev, apdev, band24):
    hapd0 = None
    hapd1 = None
    try:
        bssid0 = apdev[0]['bssid']
        params = hs20_ap_params()
        params['hessid'] = bssid0
        hapd0 = hostapd.add_ap(apdev[0], params)

        bssid1 = apdev[1]['bssid']
        params = hs20_ap_params()
        params['hessid'] = bssid0
        params["hw_mode"] = "a"
        params["channel"] = "36"
        params["country_code"] = "US"
        hapd1 = hostapd.add_ap(apdev[1], params)

        band = "2.4" if band24 else "5"
        exp_bssid = bssid0 if band24 else bssid1
        run_sigma_dut_hs20_assoc_2(dev, apdev, band, exp_bssid)
    finally:
        dev[0].request("DISCONNECT")
        if hapd0:
            hapd0.request("DISABLE")
        if hapd1:
            hapd1.request("DISABLE")
        subprocess.call(['iw', 'reg', 'set', '00'])
        dev[0].flush_scan_cache()

def run_sigma_dut_hs20_assoc_2(dev, apdev, band, expect_bssid):
    check_eap_capa(dev[0], "MSCHAPV2")
    dev[0].flush_scan_cache()

    ifname = dev[0].ifname
    sigma = start_sigma_dut(ifname)

    try:
        sigma_dut_cmd_check("sta_reset_default,interface,%s,prog,HS2-R3" % ifname)
        sigma_dut_cmd_check("sta_set_ip_config,interface,%s,dhcp,0,ip,127.0.0.11,mask,255.255.255.0" % ifname)
        sigma_dut_cmd_check("sta_add_credential,interface,%s,type,uname_pwd,realm,example.com,username,hs20-test,password,password" % ifname)
        res = sigma_dut_cmd_check("sta_hs2_associate,interface,%s,band,%s" % (ifname, band),
                                  timeout=15)
        sigma_dut_wait_connected(ifname)
        sigma_dut_cmd_check("sta_get_ip_config,interface," + ifname)
        sigma_dut_cmd_check("sta_disconnect,interface," + ifname)
        sigma_dut_cmd_check("sta_reset_default,interface," + ifname)
    finally:
        stop_sigma_dut(sigma)

    if "BSSID," + expect_bssid not in res:
        raise Exception("Unexpected BSSID: " + res)

def test_sigma_dut_ap_hs20(dev, apdev, params):
    """sigma_dut controlled AP with Hotspot 2.0 parameters"""
    logdir = os.path.join(params['logdir'],
                          "sigma_dut_ap_hs20.sigma-hostapd")
    conffile = os.path.join(params['logdir'],
                            "sigma_dut_ap_hs20.sigma-conf")
    with HWSimRadio() as (radio, iface):
        sigma = start_sigma_dut(iface, hostapd_logdir=logdir)
        try:
            sigma_dut_cmd_check("ap_reset_default,NAME,AP,program,HS2-R3")
            sigma_dut_cmd_check("ap_set_wireless,NAME,AP,WLAN_TAG,1,CHANNEL,1,SSID,test-hs20,MODE,11ng")
            sigma_dut_cmd_check("ap_set_radius,NAME,AP,WLAN_TAG,1,IPADDR,127.0.0.1,PORT,1812,PASSWORD,radius")
            sigma_dut_cmd_check("ap_set_security,NAME,AP,WLAN_TAG,1,KEYMGNT,WPA2-ENT")
            sigma_dut_cmd_check("ap_set_hs2,NAME,AP,WLAN_TAG,1,HESSID,02:12:34:56:78:9a,NAI_REALM_LIST,1,OPER_NAME,1")
            sigma_dut_cmd_check("ap_set_hs2,NAME,AP,WLAN_TAG,1,OSU_SERVER_URI,https://example.com/ https://example.org/,OSU_SSID,test-osu,OSU_METHOD,SOAP SOAP,OSU_PROVIDER_LIST,10,OSU_PROVIDER_NAI_LIST,4")
            sigma_dut_cmd_check("ap_set_hs2,NAME,AP,WLAN_TAG,1,NET_AUTH_TYPE,2")
            sigma_dut_cmd_check("ap_set_hs2,NAME,AP,WLAN_TAG,1,VENUE_NAME,1")
            sigma_dut_cmd_check("ap_set_hs2,NAME,AP,WLAN_TAG,1,DOMAIN_LIST,example.com")
            sigma_dut_cmd_check("ap_set_hs2,NAME,AP,WLAN_TAG,1,OPERATOR_ICON_METADATA,1")
            sigma_dut_cmd_check("ap_set_wireless,NAME,AP,WLAN_TAG,2,CHANNEL,1,SSID,test-osu,MODE,11ng")
            sigma_dut_cmd_check("ap_set_security,NAME,AP,WLAN_TAG,2,KEYMGNT,NONE")
            sigma_dut_cmd_check("ap_set_hs2,NAME,AP,WLAN_TAG,2,OSU,1")
            sigma_dut_cmd_check("ap_config_commit,NAME,AP")

            with open("/tmp/sigma_dut-ap.conf", "rb") as f:
                with open(conffile, "wb") as f2:
                    f2.write(f.read())

            sigma_dut_cmd_check("ap_reset_default")
        finally:
            stop_sigma_dut(sigma)

def test_sigma_dut_eap_ttls_uosc(dev, apdev, params):
    """sigma_dut controlled STA and EAP-TTLS with UOSC"""
    logdir = params['logdir']

    with open("auth_serv/ca.pem", "r") as f:
        with open(os.path.join(logdir, "sigma_dut_eap_ttls_uosc.ca.pem"),
                  "w") as f2:
            f2.write(f.read())

    src = "auth_serv/server.pem"
    dst = os.path.join(logdir, "sigma_dut_eap_ttls_uosc.server.der")
    hashdst = os.path.join(logdir, "sigma_dut_eap_ttls_uosc.server.pem.sha256")
    subprocess.check_call(["openssl", "x509", "-in", src, "-out", dst,
                           "-outform", "DER"],
                          stderr=open('/dev/null', 'w'))
    with open(dst, "rb") as f:
        der = f.read()
    hash = hashlib.sha256(der).digest()
    with open(hashdst, "w") as f:
        f.write(binascii.hexlify(hash).decode())

    dst = os.path.join(logdir, "sigma_dut_eap_ttls_uosc.incorrect.pem.sha256")
    with open(dst, "w") as f:
        f.write(32*"00")

    ssid = "test-wpa2-eap"
    params = hostapd.wpa2_eap_params(ssid=ssid)
    hapd = hostapd.add_ap(apdev[0], params)

    ifname = dev[0].ifname
    sigma = start_sigma_dut(ifname, cert_path=logdir)

    try:
        cmd = "sta_set_security,type,eapttls,interface,%s,ssid,%s,keymgmttype,wpa2,encType,AES-CCMP,PairwiseCipher,AES-CCMP-128,username,DOMAIN\mschapv2 user,password,password,ServerCert,sigma_dut_eap_ttls_uosc.incorrect.pem" % (ifname, ssid)

        sigma_dut_cmd_check("sta_reset_default,interface,%s,prog,WPA3" % ifname)
        sigma_dut_cmd_check("sta_set_ip_config,interface,%s,dhcp,0,ip,127.0.0.11,mask,255.255.255.0" % ifname)
        sigma_dut_cmd_check(cmd)
        sigma_dut_cmd_check("sta_associate,interface,%s,ssid,%s,channel,1" % (ifname, ssid),
                            timeout=10)
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-TLS-CERT-ERROR"], timeout=10)
        if ev is None:
            raise Exception("Server certificate error not reported")

        res = sigma_dut_cmd_check("dev_exec_action,program,WPA3,interface,%s,ServerCertTrust,Accept" % ifname)
        if "ServerCertTrustResult,Accepted" not in res:
            raise Exception("Server certificate trust was not accepted")
        sigma_dut_wait_connected(ifname)
        sigma_dut_cmd_check("sta_disconnect,interface," + ifname)
        sigma_dut_cmd_check("sta_reset_default,interface," + ifname)
        dev[0].dump_monitor()
    finally:
        stop_sigma_dut(sigma)

def test_sigma_dut_eap_ttls_uosc_tod(dev, apdev, params):
    """sigma_dut controlled STA and EAP-TTLS with UOSC/TOD-STRICT"""
    run_sigma_dut_eap_ttls_uosc_tod(dev, apdev, params, False)

def test_sigma_dut_eap_ttls_uosc_tod_tofu(dev, apdev, params):
    """sigma_dut controlled STA and EAP-TTLS with UOSC/TOD-TOFU"""
    run_sigma_dut_eap_ttls_uosc_tod(dev, apdev, params, True)

def run_sigma_dut_eap_ttls_uosc_tod(dev, apdev, params, tofu):
    check_tls_tod(dev[0])
    logdir = params['logdir']

    name = "sigma_dut_eap_ttls_uosc_tod"
    if tofu:
        name += "_tofu"
    with open("auth_serv/ca.pem", "r") as f:
        with open(os.path.join(logdir, name + ".ca.pem"), "w") as f2:
            f2.write(f.read())

    if tofu:
        src = "auth_serv/server-certpol2.pem"
    else:
        src = "auth_serv/server-certpol.pem"
    dst = os.path.join(logdir, name + ".server.der")
    hashdst = os.path.join(logdir, name + ".server.pem.sha256")
    subprocess.check_call(["openssl", "x509", "-in", src, "-out", dst,
                           "-outform", "DER"],
                          stderr=open('/dev/null', 'w'))
    with open(dst, "rb") as f:
        der = f.read()
    hash = hashlib.sha256(der).digest()
    with open(hashdst, "w") as f:
        f.write(binascii.hexlify(hash).decode())

    ssid = "test-wpa2-eap"
    params = int_eap_server_params()
    params["ssid"] = ssid
    if tofu:
        params["server_cert"] = "auth_serv/server-certpol2.pem"
        params["private_key"] = "auth_serv/server-certpol2.key"
    else:
        params["server_cert"] = "auth_serv/server-certpol.pem"
        params["private_key"] = "auth_serv/server-certpol.key"
    hapd = hostapd.add_ap(apdev[0], params)

    ifname = dev[0].ifname
    sigma = start_sigma_dut(ifname, cert_path=logdir)

    try:
        cmd = ("sta_set_security,type,eapttls,interface,%s,ssid,%s,keymgmttype,wpa2,encType,AES-CCMP,PairwiseCipher,AES-CCMP-128,trustedRootCA," + name + ".ca.pem,username,DOMAIN\mschapv2 user,password,password,ServerCert," + name + ".server.pem") % (ifname, ssid)
        sigma_dut_cmd_check("sta_reset_default,interface,%s,prog,WPA3" % ifname)
        sigma_dut_cmd_check("sta_set_ip_config,interface,%s,dhcp,0,ip,127.0.0.11,mask,255.255.255.0" % ifname)
        sigma_dut_cmd_check(cmd)
        sigma_dut_cmd_check("sta_associate,interface,%s,ssid,%s,channel,1" % (ifname, ssid),
                            timeout=10)
        sigma_dut_wait_connected(ifname)
        sigma_dut_cmd_check("sta_get_ip_config,interface," + ifname)
        sigma_dut_cmd_check("sta_disconnect,interface," + ifname + ",maintain_profile,1")
        dev[0].wait_disconnected()
        dev[0].dump_monitor()

        hapd.disable()
        params = hostapd.wpa2_eap_params(ssid=ssid)
        hapd = hostapd.add_ap(apdev[0], params)

        sigma_dut_cmd_check("sta_associate,interface,%s,ssid,%s,channel,1" % (ifname, ssid),
                            timeout=10)
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-TLS-CERT-ERROR"], timeout=10)
        if ev is None:
            raise Exception("Server certificate error not reported")

        res = sigma_dut_cmd_check("dev_exec_action,program,WPA3,interface,%s,ServerCertTrust,Accept" % ifname)
        if "ServerCertTrustResult,Accepted" in res:
            raise Exception("Server certificate trust override was accepted unexpectedly")
        sigma_dut_cmd_check("sta_reset_default,interface," + ifname)
        dev[0].dump_monitor()
    finally:
        stop_sigma_dut(sigma)

def test_sigma_dut_eap_ttls_uosc_initial_tod_strict(dev, apdev, params):
    """sigma_dut controlled STA and EAP-TTLS with initial UOSC/TOD-STRICT"""
    run_sigma_dut_eap_ttls_uosc_initial_tod(dev, apdev, params, False)

def test_sigma_dut_eap_ttls_uosc_initial_tod_tofu(dev, apdev, params):
    """sigma_dut controlled STA and EAP-TTLS with initial UOSC/TOD-TOFU"""
    run_sigma_dut_eap_ttls_uosc_initial_tod(dev, apdev, params, True)

def run_sigma_dut_eap_ttls_uosc_initial_tod(dev, apdev, params, tofu):
    check_tls_tod(dev[0])
    logdir = params['logdir']
    name = params['name']
    with open("auth_serv/rsa3072-ca.pem", "r") as f:
        with open(params['prefix'] + ".ca.pem", "w") as f2:
            f2.write(f.read())

    if tofu:
        src = "auth_serv/server-certpol2.pem"
    else:
        src = "auth_serv/server-certpol.pem"
    dst = params['prefix'] + ".server.der"
    hashdst = params['prefix'] + ".server.pem.sha256"
    subprocess.check_call(["openssl", "x509", "-in", src, "-out", dst,
                           "-outform", "DER"],
                          stderr=open('/dev/null', 'w'))
    with open(dst, "rb") as f:
        der = f.read()
    hash = hashlib.sha256(der).digest()
    with open(hashdst, "w") as f:
        f.write(binascii.hexlify(hash).decode())

    ssid = "test-wpa2-eap"
    params = int_eap_server_params()
    params["ssid"] = ssid
    if tofu:
        params["server_cert"] = "auth_serv/server-certpol2.pem"
        params["private_key"] = "auth_serv/server-certpol2.key"
    else:
        params["server_cert"] = "auth_serv/server-certpol.pem"
        params["private_key"] = "auth_serv/server-certpol.key"
    hapd = hostapd.add_ap(apdev[0], params)

    ifname = dev[0].ifname
    sigma = start_sigma_dut(ifname, cert_path=logdir)

    try:
        cmd = ("sta_set_security,type,eapttls,interface,%s,ssid,%s,keymgmttype,wpa2,encType,AES-CCMP,PairwiseCipher,AES-CCMP-128,trustedRootCA," + name + ".ca.pem,username,DOMAIN\mschapv2 user,password,password") % (ifname, ssid)
        sigma_dut_cmd_check("sta_reset_default,interface,%s,prog,WPA3" % ifname)
        sigma_dut_cmd_check("sta_set_ip_config,interface,%s,dhcp,0,ip,127.0.0.11,mask,255.255.255.0" % ifname)
        sigma_dut_cmd_check(cmd)
        sigma_dut_cmd_check("sta_associate,interface,%s,ssid,%s,channel,1" % (ifname, ssid),
                            timeout=10)
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-TLS-CERT-ERROR"], timeout=15)
        if ev is None:
            raise Exception("Server certificate validation failure not reported")

        res = sigma_dut_cmd_check("dev_exec_action,program,WPA3,interface,%s,ServerCertTrust,Accept" % ifname)
        if not tofu and "ServerCertTrustResult,Accepted" in res:
            raise Exception("Server certificate trust override was accepted unexpectedly")
        if tofu and "ServerCertTrustResult,Accepted" not in res:
            raise Exception("Server certificate trust override was not accepted")
        sigma_dut_cmd_check("sta_reset_default,interface," + ifname)
        dev[0].dump_monitor()
    finally:
        stop_sigma_dut(sigma)

def test_sigma_dut_eap_ttls_uosc_ca_mistrust(dev, apdev, params):
    """sigma_dut controlled STA and EAP-TTLS with UOSC when CA is not trusted"""
    check_domain_suffix_match(dev[0])
    logdir = params['logdir']

    with open("auth_serv/ca.pem", "r") as f:
        with open(os.path.join(logdir,
                               "sigma_dut_eap_ttls_uosc_ca_mistrust.ca.pem"),
                  "w") as f2:
            f2.write(f.read())

    ssid = "test-wpa2-eap"
    params = int_eap_server_params()
    params["ssid"] = ssid
    params["ca_cert"] = "auth_serv/rsa3072-ca.pem"
    params["server_cert"] = "auth_serv/rsa3072-server.pem"
    params["private_key"] = "auth_serv/rsa3072-server.key"
    hapd = hostapd.add_ap(apdev[0], params)

    ifname = dev[0].ifname
    sigma = start_sigma_dut(ifname, cert_path=logdir)

    try:
        cmd = "sta_set_security,type,eapttls,interface,%s,ssid,%s,keymgmttype,wpa2,encType,AES-CCMP,PairwiseCipher,AES-CCMP-128,trustedRootCA,sigma_dut_eap_ttls_uosc_ca_mistrust.ca.pem,username,DOMAIN\mschapv2 user,password,password,domainSuffix,w1.fi" % (ifname, ssid)
        sigma_dut_cmd_check("sta_reset_default,interface,%s,prog,WPA3" % ifname)
        sigma_dut_cmd_check("sta_set_ip_config,interface,%s,dhcp,0,ip,127.0.0.11,mask,255.255.255.0" % ifname)
        sigma_dut_cmd_check(cmd)
        sigma_dut_cmd_check("sta_associate,interface,%s,ssid,%s,channel,1" % (ifname, ssid),
                            timeout=10)
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-TLS-CERT-ERROR"], timeout=10)
        if ev is None:
            raise Exception("Server certificate error not reported")

        res = sigma_dut_cmd_check("dev_exec_action,program,WPA3,interface,%s,ServerCertTrust,Accept" % ifname)
        if "ServerCertTrustResult,Accepted" not in res:
            raise Exception("Server certificate trust was not accepted")
        sigma_dut_wait_connected(ifname)
        sigma_dut_cmd_check("sta_disconnect,interface," + ifname)
        sigma_dut_cmd_check("sta_reset_default,interface," + ifname)
        dev[0].dump_monitor()
    finally:
        stop_sigma_dut(sigma)

def start_sae_pwe_ap(apdev, sae_pwe):
    ssid = "test-sae"
    params = hostapd.wpa2_params(ssid=ssid, passphrase="12345678")
    params['wpa_key_mgmt'] = 'SAE'
    params["ieee80211w"] = "2"
    params['sae_groups'] = '19'
    params['sae_pwe'] = str(sae_pwe)
    return hostapd.add_ap(apdev, params)

def connect_sae_pwe_sta(dev, ifname, extra=None):
    dev.dump_monitor()
    sigma_dut_cmd_check("sta_reset_default,interface,%s" % ifname)
    sigma_dut_cmd_check("sta_set_ip_config,interface,%s,dhcp,0,ip,127.0.0.11,mask,255.255.255.0" % ifname)
    cmd = "sta_set_security,interface,%s,ssid,%s,passphrase,%s,type,SAE,encpType,aes-ccmp,keymgmttype,wpa2" % (ifname, "test-sae", "12345678")
    if extra:
        cmd += "," + extra
    sigma_dut_cmd_check(cmd)
    sigma_dut_cmd_check("sta_associate,interface,%s,ssid,%s,channel,1" % (ifname, "test-sae"),
                        timeout=10)
    sigma_dut_wait_connected(ifname)
    sigma_dut_cmd_check("sta_disconnect,interface," + ifname)
    dev.wait_disconnected()
    sigma_dut_cmd_check("sta_reset_default,interface," + ifname)
    dev.dump_monitor()

def no_connect_sae_pwe_sta(dev, ifname, extra=None):
    dev.dump_monitor()
    sigma_dut_cmd_check("sta_reset_default,interface,%s" % ifname)
    sigma_dut_cmd_check("sta_set_ip_config,interface,%s,dhcp,0,ip,127.0.0.11,mask,255.255.255.0" % ifname)
    cmd = "sta_set_security,interface,%s,ssid,%s,passphrase,%s,type,SAE,encpType,aes-ccmp,keymgmttype,wpa2" % (ifname, "test-sae", "12345678")
    if extra:
        cmd += "," + extra
    sigma_dut_cmd_check(cmd)
    sigma_dut_cmd_check("sta_associate,interface,%s,ssid,%s,channel,1" % (ifname, "test-sae"),
                        timeout=10)
    ev = dev.wait_event(["CTRL-EVENT-CONNECTED",
                         "CTRL-EVENT-NETWORK-NOT-FOUND"], timeout=10)
    if ev is None or "CTRL-EVENT-CONNECTED" in ev:
        raise Exception("Unexpected connection result")
    sigma_dut_cmd_check("sta_reset_default,interface," + ifname)
    dev.dump_monitor()

def test_sigma_dut_sae_h2e(dev, apdev):
    """sigma_dut controlled SAE H2E association (AP using loop+H2E)"""
    if "SAE" not in dev[0].get_capability("auth_alg"):
        raise HwsimSkip("SAE not supported")

    start_sae_pwe_ap(apdev[0], 2)

    ifname = dev[0].ifname
    sigma = start_sigma_dut(ifname, sae_h2e=True)
    try:
        connect_sae_pwe_sta(dev[0], ifname)
        connect_sae_pwe_sta(dev[0], ifname, extra="sae_pwe,h2e")
        connect_sae_pwe_sta(dev[0], ifname, extra="sae_pwe,loop")
        res = sigma_dut_cmd("sta_set_security,interface,%s,ssid,%s,passphrase,%s,type,SAE,encpType,aes-ccmp,keymgmttype,wpa2,sae_pwe,unknown" % (ifname, "test-sae", "12345678"))
        if res != "status,ERROR,errorCode,Unsupported sae_pwe value":
            raise Exception("Unexpected error result: " + res)
    finally:
        stop_sigma_dut(sigma)
        dev[0].set("sae_pwe", "0")

def test_sigma_dut_sae_h2e_ap_loop(dev, apdev):
    """sigma_dut controlled SAE H2E association (AP using loop-only)"""
    if "SAE" not in dev[0].get_capability("auth_alg"):
        raise HwsimSkip("SAE not supported")

    start_sae_pwe_ap(apdev[0], 0)

    ifname = dev[0].ifname
    sigma = start_sigma_dut(ifname, sae_h2e=True)
    try:
        connect_sae_pwe_sta(dev[0], ifname)
        connect_sae_pwe_sta(dev[0], ifname, extra="sae_pwe,loop")
        no_connect_sae_pwe_sta(dev[0], ifname, extra="sae_pwe,h2e")
    finally:
        stop_sigma_dut(sigma)
        dev[0].set("sae_pwe", "0")

def test_sigma_dut_sae_h2e_ap_h2e(dev, apdev):
    """sigma_dut controlled SAE H2E association (AP using H2E-only)"""
    if "SAE" not in dev[0].get_capability("auth_alg"):
        raise HwsimSkip("SAE not supported")

    start_sae_pwe_ap(apdev[0], 1)

    ifname = dev[0].ifname
    sigma = start_sigma_dut(ifname, sae_h2e=True)
    try:
        connect_sae_pwe_sta(dev[0], ifname)
        no_connect_sae_pwe_sta(dev[0], ifname, extra="sae_pwe,loop")
        connect_sae_pwe_sta(dev[0], ifname, extra="sae_pwe,h2e")
    finally:
        stop_sigma_dut(sigma)
        dev[0].set("sae_pwe", "0")

def test_sigma_dut_ap_sae_h2e(dev, apdev, params):
    """sigma_dut controlled AP with SAE H2E"""
    logdir = os.path.join(params['logdir'],
                          "sigma_dut_ap_sae_h2e.sigma-hostapd")
    if "SAE" not in dev[0].get_capability("auth_alg"):
        raise HwsimSkip("SAE not supported")
    with HWSimRadio() as (radio, iface):
        sigma = start_sigma_dut(iface, sae_h2e=True, hostapd_logdir=logdir)
        try:
            sigma_dut_cmd_check("ap_reset_default")
            sigma_dut_cmd_check("ap_set_wireless,NAME,AP,CHANNEL,1,SSID,test-sae,MODE,11ng")
            sigma_dut_cmd_check("ap_set_security,NAME,AP,KEYMGNT,WPA2-SAE,PSK,12345678")
            sigma_dut_cmd_check("ap_config_commit,NAME,AP")

            for sae_pwe in [0, 1, 2]:
                dev[0].request("SET sae_groups ")
                dev[0].set("sae_pwe", str(sae_pwe))
                dev[0].connect("test-sae", key_mgmt="SAE", psk="12345678",
                               ieee80211w="2", scan_freq="2412")
                dev[0].request("REMOVE_NETWORK all")
                dev[0].wait_disconnected()
                dev[0].dump_monitor()

            sigma_dut_cmd_check("ap_reset_default")
        finally:
            stop_sigma_dut(sigma)
            dev[0].set("sae_pwe", "0")

def test_sigma_dut_ap_sae_h2e_only(dev, apdev, params):
    """sigma_dut controlled AP with SAE H2E-only"""
    logdir = os.path.join(params['logdir'],
                          "sigma_dut_ap_sae_h2e.sigma-hostapd")
    if "SAE" not in dev[0].get_capability("auth_alg"):
        raise HwsimSkip("SAE not supported")
    with HWSimRadio() as (radio, iface):
        sigma = start_sigma_dut(iface, sae_h2e=True, hostapd_logdir=logdir)
        try:
            sigma_dut_cmd_check("ap_reset_default")
            sigma_dut_cmd_check("ap_set_wireless,NAME,AP,CHANNEL,1,SSID,test-sae,MODE,11ng")
            sigma_dut_cmd_check("ap_set_security,NAME,AP,KEYMGNT,WPA2-SAE,PSK,12345678,sae_pwe,h2e")
            sigma_dut_cmd_check("ap_config_commit,NAME,AP")

            dev[0].request("SET sae_groups ")
            dev[0].set("sae_pwe", "1")
            dev[0].connect("test-sae", key_mgmt="SAE", psk="12345678",
                           ieee80211w="2", scan_freq="2412")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()
            dev[0].dump_monitor()

            dev[0].set("sae_pwe", "0")
            dev[0].connect("test-sae", key_mgmt="SAE", psk="12345678",
                           ieee80211w="2", scan_freq="2412", wait_connect=False)
            ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED",
                                    "CTRL-EVENT-NETWORK-NOT-FOUND"], timeout=10)
            dev[0].request("DISCONNECT")
            if ev is None or "CTRL-EVENT-CONNECTED" in ev:
                raise Exception("Unexpected connection result")

            sigma_dut_cmd_check("ap_reset_default")
        finally:
            stop_sigma_dut(sigma)
            dev[0].set("sae_pwe", "0")

def test_sigma_dut_ap_sae_loop_only(dev, apdev, params):
    """sigma_dut controlled AP with SAE looping-only"""
    logdir = os.path.join(params['logdir'],
                          "sigma_dut_ap_sae_h2e.sigma-hostapd")
    if "SAE" not in dev[0].get_capability("auth_alg"):
        raise HwsimSkip("SAE not supported")
    with HWSimRadio() as (radio, iface):
        sigma = start_sigma_dut(iface, sae_h2e=True, hostapd_logdir=logdir)
        try:
            sigma_dut_cmd_check("ap_reset_default")
            sigma_dut_cmd_check("ap_set_wireless,NAME,AP,CHANNEL,1,SSID,test-sae,MODE,11ng")
            sigma_dut_cmd_check("ap_set_security,NAME,AP,KEYMGNT,WPA2-SAE,PSK,12345678,sae_pwe,loop")
            sigma_dut_cmd_check("ap_config_commit,NAME,AP")

            dev[0].request("SET sae_groups ")
            dev[0].set("sae_pwe", "0")
            dev[0].connect("test-sae", key_mgmt="SAE", psk="12345678",
                           ieee80211w="2", scan_freq="2412")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()
            dev[0].dump_monitor()

            dev[0].set("sae_pwe", "1")
            dev[0].connect("test-sae", key_mgmt="SAE", psk="12345678",
                           ieee80211w="2", scan_freq="2412", wait_connect=False)
            ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED",
                                    "CTRL-EVENT-NETWORK-NOT-FOUND"], timeout=10)
            dev[0].request("DISCONNECT")
            if ev is None or "CTRL-EVENT-CONNECTED" in ev:
                raise Exception("Unexpected connection result")

            sigma_dut_cmd_check("ap_reset_default")
        finally:
            stop_sigma_dut(sigma)
            dev[0].set("sae_pwe", "0")

def test_sigma_dut_sae_h2e_loop_forcing(dev, apdev):
    """sigma_dut controlled SAE H2E misbehavior with looping forced"""
    if "SAE" not in dev[0].get_capability("auth_alg"):
        raise HwsimSkip("SAE not supported")

    ssid = "test-sae"
    params = hostapd.wpa2_params(ssid=ssid, passphrase="12345678")
    params['wpa_key_mgmt'] = 'SAE'
    params["ieee80211w"] = "2"
    params['sae_pwe'] = '1'
    hapd = hostapd.add_ap(apdev[0], params)

    ifname = dev[0].ifname
    sigma = start_sigma_dut(ifname)
    try:
        sigma_dut_cmd_check("sta_reset_default,interface,%s" % ifname)
        sigma_dut_cmd_check("sta_set_ip_config,interface,%s,dhcp,0,ip,127.0.0.11,mask,255.255.255.0" % ifname)
        sigma_dut_cmd_check("sta_set_security,interface,%s,ssid,%s,passphrase,%s,type,SAE,encpType,aes-ccmp,keymgmttype,wpa2,IgnoreH2E_RSNXE_BSSMemSel,1" % (ifname, "test-sae", "12345678"))
        sigma_dut_cmd_check("sta_associate,interface,%s,ssid,%s,channel,1" % (ifname, "test-sae"),
                            timeout=10)
        ev = dev[0].wait_event(["SME: Trying to authenticate with"], timeout=10)
        if ev is None:
            raise Exception("No authentication attempt reported")
        ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED"], timeout=0.5)
        if ev is not None:
            raise Exception("Unexpected connection reported")
    finally:
        stop_sigma_dut(sigma)

def test_sigma_dut_sae_h2e_enabled_group_rejected(dev, apdev):
    """sigma_dut controlled SAE H2E misbehavior with rejected groups"""
    if "SAE" not in dev[0].get_capability("auth_alg"):
        raise HwsimSkip("SAE not supported")

    ssid = "test-sae"
    params = hostapd.wpa2_params(ssid=ssid, passphrase="12345678")
    params['wpa_key_mgmt'] = 'SAE'
    params["ieee80211w"] = "2"
    params['sae_groups'] = "19 20"
    params['sae_pwe'] = '1'
    hapd = hostapd.add_ap(apdev[0], params)

    ifname = dev[0].ifname
    sigma = start_sigma_dut(ifname, sae_h2e=True)
    try:
        sigma_dut_cmd_check("sta_reset_default,interface,%s" % ifname)
        sigma_dut_cmd_check("sta_set_ip_config,interface,%s,dhcp,0,ip,127.0.0.11,mask,255.255.255.0" % ifname)
        sigma_dut_cmd_check("sta_set_security,interface,%s,ssid,%s,passphrase,%s,type,SAE,encpType,aes-ccmp,keymgmttype,wpa2,ECGroupID_RGE,19 123" % (ifname, "test-sae", "12345678"))
        sigma_dut_cmd_check("sta_associate,interface,%s,ssid,%s,channel,1" % (ifname, "test-sae"),
                            timeout=10)
        ev = dev[0].wait_event(["SME: Trying to authenticate with"], timeout=10)
        if ev is None:
            raise Exception("No authentication attempt reported")
        ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED"], timeout=0.5)
        if ev is not None:
            raise Exception("Unexpected connection reported")
    finally:
        stop_sigma_dut(sigma)
        dev[0].set("sae_pwe", "0")

def test_sigma_dut_sae_h2e_rsnxe_mismatch(dev, apdev):
    """sigma_dut controlled SAE H2E misbehavior with RSNXE"""
    if "SAE" not in dev[0].get_capability("auth_alg"):
        raise HwsimSkip("SAE not supported")

    ssid = "test-sae"
    params = hostapd.wpa2_params(ssid=ssid, passphrase="12345678")
    params['wpa_key_mgmt'] = 'SAE'
    params["ieee80211w"] = "2"
    params['sae_groups'] = "19"
    params['sae_pwe'] = '1'
    hapd = hostapd.add_ap(apdev[0], params)

    ifname = dev[0].ifname
    sigma = start_sigma_dut(ifname, sae_h2e=True)
    try:
        sigma_dut_cmd_check("sta_reset_default,interface,%s" % ifname)
        sigma_dut_cmd_check("sta_set_ip_config,interface,%s,dhcp,0,ip,127.0.0.11,mask,255.255.255.0" % ifname)
        sigma_dut_cmd_check("sta_set_security,interface,%s,ssid,%s,passphrase,%s,type,SAE,encpType,aes-ccmp,keymgmttype,wpa2,RSNXE_Content,EapolM2:F40100" % (ifname, "test-sae", "12345678"))
        sigma_dut_cmd_check("sta_associate,interface,%s,ssid,%s,channel,1" % (ifname, "test-sae"),
                            timeout=10)
        ev = dev[0].wait_event(["SME: Trying to authenticate with"], timeout=10)
        if ev is None:
            raise Exception("No authentication attempt reported")
        ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED"], timeout=0.5)
        if ev is not None:
            raise Exception("Unexpected connection reported")
    finally:
        stop_sigma_dut(sigma)
        dev[0].set("sae_pwe", "0")

def test_sigma_dut_ap_sae_h2e_rsnxe_mismatch(dev, apdev, params):
    """sigma_dut controlled SAE H2E AP misbehavior with RSNXE"""
    logdir = os.path.join(params['logdir'],
                          "sigma_dut_ap_sae_h2e_rsnxe_mismatch.sigma-hostapd")
    if "SAE" not in dev[0].get_capability("auth_alg"):
        raise HwsimSkip("SAE not supported")
    with HWSimRadio() as (radio, iface):
        sigma = start_sigma_dut(iface, sae_h2e=True, hostapd_logdir=logdir)
        try:
            sigma_dut_cmd_check("ap_reset_default")
            sigma_dut_cmd_check("ap_set_wireless,NAME,AP,CHANNEL,1,SSID,test-sae,MODE,11ng")
            sigma_dut_cmd_check("ap_set_security,NAME,AP,KEYMGNT,WPA2-SAE,PSK,12345678,sae_pwe,h2e,RSNXE_Content,EapolM3:F40100")
            sigma_dut_cmd_check("ap_config_commit,NAME,AP")

            dev[0].request("SET sae_groups ")
            dev[0].set("sae_pwe", "1")
            dev[0].connect("test-sae", key_mgmt="SAE", psk="12345678",
                           ieee80211w="2", scan_freq="2412", wait_connect=False)
            ev = dev[0].wait_event(["Associated with"], timeout=10)
            if ev is None:
                raise Exception("No indication of association seen")
            ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED",
                                    "CTRL-EVENT-DISCONNECTED"], timeout=10)
            dev[0].request("DISCONNECT")
            if ev is None:
                raise Exception("No disconnection seen")
            if "CTRL-EVENT-DISCONNECTED" not in ev:
                raise Exception("Unexpected connection")

            sigma_dut_cmd_check("ap_reset_default")
        finally:
            stop_sigma_dut(sigma)
            dev[0].set("sae_pwe", "0")

def test_sigma_dut_ap_sae_h2e_group_rejection(dev, apdev, params):
    """sigma_dut controlled AP with SAE H2E-only and group rejection"""
    logdir = os.path.join(params['logdir'],
                          "sigma_dut_ap_sae_h2e_group_rejection.sigma-hostapd")
    if "SAE" not in dev[0].get_capability("auth_alg"):
        raise HwsimSkip("SAE not supported")
    with HWSimRadio() as (radio, iface):
        sigma = start_sigma_dut(iface, sae_h2e=True, hostapd_logdir=logdir)
        try:
            sigma_dut_cmd_check("ap_reset_default")
            sigma_dut_cmd_check("ap_set_wireless,NAME,AP,CHANNEL,1,SSID,test-sae,MODE,11ng")
            sigma_dut_cmd_check("ap_set_security,NAME,AP,KEYMGNT,WPA2-SAE,PSK,12345678,sae_pwe,h2e")
            sigma_dut_cmd_check("ap_config_commit,NAME,AP")

            dev[0].request("SET sae_groups 21 20 19")
            dev[0].set("sae_pwe", "1")
            dev[0].connect("test-sae", key_mgmt="SAE", psk="12345678",
                           ieee80211w="2", scan_freq="2412")
            addr = dev[0].own_addr()
            res = sigma_dut_cmd_check("dev_exec_action,program,WPA3,Dest_MAC,%s,Rejected_DH_Groups,1" % addr)
            if "DHGroupVerResult,21 20" not in res:
                raise Exception("Unexpected dev_exec_action response: " + res)

            sigma_dut_cmd_check("ap_reset_default")
        finally:
            stop_sigma_dut(sigma)
            dev[0].set("sae_pwe", "0")

def test_sigma_dut_ap_sae_h2e_anti_clogging(dev, apdev, params):
    """sigma_dut controlled AP with SAE H2E and anti-clogging token"""
    logdir = os.path.join(params['logdir'],
                          "sigma_dut_ap_sae_h2e_anti_clogging.sigma-hostapd")
    if "SAE" not in dev[0].get_capability("auth_alg"):
        raise HwsimSkip("SAE not supported")
    with HWSimRadio() as (radio, iface):
        sigma = start_sigma_dut(iface, sae_h2e=True, hostapd_logdir=logdir)
        try:
            sigma_dut_cmd_check("ap_reset_default")
            sigma_dut_cmd_check("ap_set_wireless,NAME,AP,CHANNEL,1,SSID,test-sae,MODE,11ng")
            sigma_dut_cmd_check("ap_set_security,NAME,AP,KEYMGNT,SAE,PSK,12345678,AntiCloggingThreshold,0")
            sigma_dut_cmd_check("ap_config_commit,NAME,AP")

            dev[0].set("sae_groups", "")
            dev[0].set("sae_pwe", "2")
            dev[0].connect("test-sae", key_mgmt="SAE", psk="12345678",
                           ieee80211w="2", scan_freq="2412")

            sigma_dut_cmd_check("ap_reset_default")
        finally:
            stop_sigma_dut(sigma)
            dev[0].set("sae_pwe", "0")

def test_sigma_dut_ap_5ghz(dev, apdev, params):
    """sigma_dut controlled AP on 5 GHz"""
    run_sigma_dut_ap_channel(dev, apdev, params, 36, '11na', 5180,
                             check_signal="WIDTH=20 MHz")

def test_sigma_dut_ap_ht40plus(dev, apdev, params):
    """sigma_dut controlled AP and HT40+"""
    run_sigma_dut_ap_channel(dev, apdev, params, 36, '11na', 5180,
                             extra="width,40", check_signal="WIDTH=40 MHz")

def test_sigma_dut_ap_ht40minus(dev, apdev, params):
    """sigma_dut controlled AP and HT40-"""
    run_sigma_dut_ap_channel(dev, apdev, params, 40, '11na', 5200,
                             extra="width,40", check_signal="WIDTH=40 MHz")

def test_sigma_dut_ap_vht40(dev, apdev, params):
    """sigma_dut controlled AP and VHT40"""
    run_sigma_dut_ap_channel(dev, apdev, params, 36, '11ac', 5180,
                             extra="width,40", check_signal="WIDTH=40 MHz",
                             program="VHT")

def test_sigma_dut_ap_vht80(dev, apdev, params):
    """sigma_dut controlled AP and VHT80"""
    run_sigma_dut_ap_channel(dev, apdev, params, 36, '11ac', 5180,
                             extra="width,80", check_signal="WIDTH=80 MHz",
                             program="VHT")

def run_sigma_dut_ap_channel(dev, apdev, params, channel, mode, scan_freq,
                             extra=None, check_signal=None, program=None):
    logdir = params['prefix'] + ".sigma-hostapd"
    with HWSimRadio() as (radio, iface):
        sigma = start_sigma_dut(iface, hostapd_logdir=logdir)
        try:
            subprocess.call(['iw', 'reg', 'set', 'US'])
            cmd = "ap_reset_default"
            if program:
                cmd += ",program," + program
            sigma_dut_cmd_check(cmd)
            cmd = "ap_set_wireless,NAME,AP,CHANNEL,%d,SSID,test-psk,MODE,%s" % (channel, mode)
            if extra:
                cmd += "," + extra
            sigma_dut_cmd_check(cmd)
            sigma_dut_cmd_check("ap_set_security,NAME,AP,KEYMGNT,WPA2-PSK,PSK,12345678")
            sigma_dut_cmd_check("ap_config_commit,NAME,AP")

            with open("/tmp/sigma_dut-ap.conf", "rb") as f:
                with open(params['prefix'] + ".sigma-conf", "wb") as f2:
                    f2.write(f.read())

            dev[0].connect("test-psk", psk="12345678", scan_freq=str(scan_freq))
            sig = dev[0].request("SIGNAL_POLL")
            logger.info("SIGNAL_POLL:\n" + sig.strip())
            dev[0].request("DISCONNECT")
            dev[0].wait_disconnected()

            sigma_dut_cmd_check("ap_reset_default")

            if check_signal and check_signal not in sig:
                raise Exception("Unexpected SIGNAL_POLL data")
        finally:
            stop_sigma_dut(sigma)
            subprocess.call(['iw', 'reg', 'set', '00'])
            dev[0].flush_scan_cache()

@reset_ignore_old_scan_res
def test_sigma_dut_beacon_prot(dev, apdev):
    """sigma_dut controlled STA and beacon protection"""
    ssid = "test-pmf-required"
    params = hostapd.wpa2_params(ssid=ssid, passphrase="12345678")
    params["wpa_key_mgmt"] = "WPA-PSK-SHA256"
    params["ieee80211w"] = "2"
    params["beacon_prot"] = "1"
    try:
        hapd = hostapd.add_ap(apdev[0], params)
    except Exception as e:
        if "Failed to enable hostapd interface" in str(e):
            raise HwsimSkip("Beacon protection not supported")
        raise

    ifname = dev[0].ifname
    sigma = start_sigma_dut(ifname)

    try:
        sigma_dut_cmd_check("sta_reset_default,interface,%s,prog,PMF" % ifname)
        sigma_dut_cmd_check("sta_set_ip_config,interface,%s,dhcp,0,ip,127.0.0.11,mask,255.255.255.0" % ifname)
        sigma_dut_cmd_check("sta_set_security,interface,%s,ssid,%s,type,PSK,passphrase,%s,encpType,aes-ccmp,keymgmttype,wpa2,PMF,Required,BeaconProtection,1" % (ifname, "test-pmf-required", "12345678"))
        sigma_dut_cmd_check("sta_associate,interface,%s,ssid,%s,channel,1" % (ifname, "test-pmf-required"),
                            timeout=10)
        sigma_dut_wait_connected(ifname)

        time.sleep(1)
        check_mac80211_bigtk(dev[0], hapd)

        sigma_dut_cmd_check("sta_reset_default,interface," + ifname)
    finally:
        stop_sigma_dut(sigma)

def test_sigma_dut_ap_beacon_prot(dev, apdev, params):
    """sigma_dut controlled AP and beacon protection"""
    logdir = params['prefix'] + ".sigma-hostapd"

    Wlantest.setup(None)
    wt = Wlantest()
    wt.flush()
    wt.add_passphrase("12345678")

    with HWSimRadio() as (radio, iface):
        sigma = start_sigma_dut(iface, hostapd_logdir=logdir)
        try:
            sigma_dut_cmd_check("ap_reset_default")
            sigma_dut_cmd_check("ap_set_wireless,NAME,AP,CHANNEL,1,SSID,test-psk,MODE,11ng")
            sigma_dut_cmd_check("ap_set_security,NAME,AP,KEYMGNT,WPA2-PSK,PSK,12345678,PMF,Required,BeaconProtection,1")
            sigma_dut_cmd_check("ap_config_commit,NAME,AP")
            bssid = sigma_dut_cmd_check("ap_get_mac_address,NAME,AP")
            bssid = bssid.split(',')[3]

            dev[0].connect("test-psk", key_mgmt="WPA-PSK-SHA256",
                           psk="12345678", scan_freq="2412",
                           ieee80211w="2", beacon_prot="1")
            time.sleep(1)

            sigma_dut_cmd_check("ap_reset_default")
        finally:
            stop_sigma_dut(sigma)

    valid_bip = wt.get_bss_counter('valid_bip_mmie', bssid)
    invalid_bip = wt.get_bss_counter('invalid_bip_mmie', bssid)
    missing_bip = wt.get_bss_counter('missing_bip_mmie', bssid)
    logger.info("wlantest BIP counters: valid=%d invalid=%d missing=%d" % (valid_bip, invalid_bip, missing_bip))
    if valid_bip < 0 or invalid_bip > 0 or missing_bip > 0:
        raise Exception("Unexpected wlantest BIP counters: valid=%d invalid=%d missing=%d" % (valid_bip, invalid_bip, missing_bip))

def test_sigma_dut_ap_transition_disable(dev, apdev, params):
    """sigma_dut controlled AP and transition disabled indication"""
    check_sae_capab(dev[0])
    logdir = params['prefix'] + ".sigma-hostapd"

    with HWSimRadio() as (radio, iface):
        sigma = start_sigma_dut(iface, hostapd_logdir=logdir)
        try:
            sigma_dut_cmd_check("ap_reset_default")
            sigma_dut_cmd_check("ap_set_wireless,NAME,AP,CHANNEL,1,SSID,test-sae,MODE,11ng")
            sigma_dut_cmd_check("ap_set_security,NAME,AP,KEYMGNT,WPA2-SAE,PSK,12345678,PMF,Required,Transition_Disable,1,Transition_Disable_Index,0")
            sigma_dut_cmd_check("ap_config_commit,NAME,AP")

            dev[0].set("sae_groups", "")
            dev[0].connect("test-sae", key_mgmt="SAE", psk="12345678",
                           ieee80211w="2", scan_freq="2412")
            ev = dev[0].wait_event(["TRANSITION-DISABLE"], timeout=1)
            if ev is None:
                raise Exception("Transition disable not indicated")
            if ev.split(' ')[1] != "01":
                raise Exception("Unexpected transition disable bitmap: " + ev)

            sigma_dut_cmd_check("ap_reset_default")
        finally:
            stop_sigma_dut(sigma)

def test_sigma_dut_ap_transition_disable_change(dev, apdev, params):
    """sigma_dut controlled AP and transition disabled indication change"""
    check_sae_capab(dev[0])
    logdir = params['prefix'] + ".sigma-hostapd"

    with HWSimRadio() as (radio, iface):
        sigma = start_sigma_dut(iface, hostapd_logdir=logdir)
        try:
            sigma_dut_cmd_check("ap_reset_default")
            sigma_dut_cmd_check("ap_set_wireless,NAME,AP,CHANNEL,1,SSID,test-sae,MODE,11ng")
            sigma_dut_cmd_check("ap_set_security,NAME,AP,KEYMGNT,WPA2-SAE,PSK,12345678,PMF,Required")
            sigma_dut_cmd_check("ap_config_commit,NAME,AP")
            dev[0].set("sae_groups", "")
            dev[0].connect("test-sae", key_mgmt="SAE", psk="12345678",
                           ieee80211w="2", scan_freq="2412")
            ev = dev[0].wait_event(["TRANSITION-DISABLE"], timeout=1)
            if ev is not None:
                raise Exception("Unexpected transition disable indication")
            dev[0].request("DISCONNECT")
            dev[0].wait_disconnected()
            dev[0].dump_monitor()

            sigma_dut_cmd_check("ap_set_rfeature,NAME,AP,Transition_Disable,1,Transition_Disable_Index,0")
            dev[0].request("RECONNECT")
            dev[0].wait_connected()
            ev = dev[0].wait_event(["TRANSITION-DISABLE"], timeout=1)
            if ev is None:
                raise Exception("Transition disable not indicated")
            if ev.split(' ')[1] != "01":
                raise Exception("Unexpected transition disable bitmap: " + ev)

            sigma_dut_cmd_check("ap_reset_default")
        finally:
            stop_sigma_dut(sigma)

def test_sigma_dut_ft_rsnxe_used_mismatch(dev, apdev):
    """sigma_dut controlled FT protocol with RSNXE Used mismatch"""
    if "SAE" not in dev[0].get_capability("auth_alg"):
        raise HwsimSkip("SAE not supported")

    ifname = dev[0].ifname
    sigma = start_sigma_dut(ifname)

    try:
        ssid = "test-sae"
        params = hostapd.wpa2_params(ssid=ssid)
        params['wpa_key_mgmt'] = 'SAE FT-SAE'
        params["ieee80211w"] = "2"
        params['sae_password'] = "hello"
        params['sae_pwe'] = "2"
        params['mobility_domain'] = 'aabb'
        bssid = apdev[0]['bssid'].replace(':', '')
        params['nas_identifier'] = bssid + '.nas.example.com'
        params['r1_key_holder'] = bssid
        params['pmk_r1_push'] = '0'
        params['r0kh'] = 'ff:ff:ff:ff:ff:ff * 00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff'
        params['r1kh'] = '00:00:00:00:00:00 00:00:00:00:00:00 00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff'
        hapd = hostapd.add_ap(apdev[0], params)
        bssid = hapd.own_addr()

        sigma_dut_cmd_check("sta_reset_default,interface,%s" % ifname)
        sigma_dut_cmd_check("sta_set_ip_config,interface,%s,dhcp,0,ip,127.0.0.11,mask,255.255.255.0" % ifname)
        sigma_dut_cmd_check("sta_set_security,interface,%s,ssid,%s,passphrase,%s,type,SAE,encpType,aes-ccmp,AKMSuiteType,8;9" % (ifname, "test-sae", "hello"))
        sigma_dut_cmd_check("sta_associate,interface,%s,ssid,%s,channel,1" % (ifname, "test-sae"),
                            timeout=10)
        sigma_dut_wait_connected(ifname)
        dev[0].dump_monitor()

        bssid2 = apdev[1]['bssid'].replace(':', '')
        params['nas_identifier'] = bssid2 + '.nas.example.com'
        params['r1_key_holder'] = bssid2
        hapd2 = hostapd.add_ap(apdev[1], params)
        bssid2 = hapd2.own_addr()

        sigma_dut_cmd_check("sta_reassoc,interface,%s,Channel,1,bssid,%s" % (ifname, bssid2))
        count = 0
        for i in range(5):
            ev = dev[0].wait_event(["Trying to associate",
                                    "CTRL-EVENT-CONNECTED"], timeout=10)
            if ev is None:
                raise Exception("Connection timed out")
            if "CTRL-EVENT-CONNECTED" in ev:
                break
            count += 1
        dev[0].dump_monitor()
        if count != 1:
            raise Exception("Unexpected number of association attempts for the first FT protocol exchange (expecting success)")

        sigma_dut_cmd_check("sta_set_rfeature,interface,%s,prog,WPA3,ReassocReq_RSNXE_Used,1" % ifname)
        sigma_dut_cmd_check("sta_reassoc,interface,%s,Channel,1,bssid,%s" % (ifname, bssid))
        count = 0
        for i in range(5):
            ev = dev[0].wait_event(["Trying to associate",
                                    "CTRL-EVENT-CONNECTED"], timeout=10)
            if ev is None:
                raise Exception("Connection timed out")
            if "CTRL-EVENT-CONNECTED" in ev:
                break
            count += 1
        dev[0].dump_monitor()
        if count != 2:
            raise Exception("Unexpected number of association attempts for the second FT protocol exchange (expecting failure)")

        sigma_dut_cmd_check("sta_disconnect,interface," + ifname)
        sigma_dut_cmd_check("sta_reset_default,interface," + ifname)
    finally:
        stop_sigma_dut(sigma)

def test_sigma_dut_ap_ft_rsnxe_used_mismatch(dev, apdev, params):
    """sigma_dut controlled AP with FT and RSNXE Used mismatch"""
    logdir = params['prefix'] + ".sigma-hostapd"
    conffile = params['prefix'] + ".sigma-conf"
    if "SAE" not in dev[0].get_capability("auth_alg"):
        raise HwsimSkip("SAE not supported")
    with HWSimRadio() as (radio, iface):
        sigma = start_sigma_dut(iface, hostapd_logdir=logdir)
        try:
            sigma_dut_cmd_check("ap_reset_default")
            sigma_dut_cmd_check("ap_set_wireless,NAME,AP,CHANNEL,1,SSID,test-sae,MODE,11ng,DOMAIN,aabb")
            sigma_dut_cmd_check("ap_set_security,NAME,AP,AKMSuiteType,8;9,SAEPasswords,hello,PMF,Required")
            sigma_dut_cmd_check("ap_config_commit,NAME,AP")

            with open("/tmp/sigma_dut-ap.conf", "rb") as f:
                with open(conffile, "wb") as f2:
                    f2.write(f.read())

            dev[0].connect("test-sae", key_mgmt="FT-SAE", sae_password="hello",
                           ieee80211w="2", scan_freq="2412")

            sigma_dut_cmd_check("ap_set_rfeature,NAME,AP,type,WPA3,ReassocResp_RSNXE_Used,1")
            # This would need to be followed by FT protocol roaming test, but
            # that is not currently convenient to implement, so for now, this
            # test is based on manual inspection of hostapd getting configured
            # properly.

            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

            sigma_dut_cmd_check("ap_reset_default")
        finally:
            stop_sigma_dut(sigma)

def test_sigma_dut_ocv(dev, apdev):
    """sigma_dut controlled STA using OCV"""
    if "SAE" not in dev[0].get_capability("auth_alg"):
        raise HwsimSkip("SAE not supported")

    ifname = dev[0].ifname
    sigma = start_sigma_dut(ifname)

    try:
        ssid = "test-sae"
        params = hostapd.wpa2_params(ssid=ssid, passphrase="12345678")
        params['wpa_key_mgmt'] = 'SAE'
        params["ieee80211w"] = "2"
        params['sae_groups'] = '19'
        params['ocv'] = '1'
        hapd = hostapd.add_ap(apdev[0], params)

        sigma_dut_cmd_check("sta_reset_default,interface,%s,prog,WPA3" % ifname)
        sigma_dut_cmd_check("sta_set_ip_config,interface,%s,dhcp,0,ip,127.0.0.11,mask,255.255.255.0" % ifname)
        sigma_dut_cmd_check("sta_set_wireless,interface,%s,program,WPA3,ocvc,1" % ifname)
        sigma_dut_cmd_check("sta_set_security,interface,%s,ssid,%s,passphrase,%s,type,SAE,encpType,aes-ccmp,keymgmttype,wpa2" % (ifname, "test-sae", "12345678"))
        sigma_dut_cmd_check("sta_associate,interface,%s,ssid,%s,channel,1" % (ifname, "test-sae"),
                            timeout=10)
        sigma_dut_wait_connected(ifname)

        sigma_dut_cmd_check("sta_reset_default,interface,%s,prog,WPA3" % ifname)
        sigma_dut_cmd_check("sta_set_ip_config,interface,%s,dhcp,0,ip,127.0.0.11,mask,255.255.255.0" % ifname)
        sigma_dut_cmd_check("sta_set_wireless,interface,%s,program,WPA3,ocvc,1" % ifname)
        sigma_dut_cmd_check("sta_set_rfeature,interface,%s,prog,WPA3,OCIFrameType,eapolM2,OCIChannel,11" % ifname)
        sigma_dut_cmd_check("sta_set_security,interface,%s,ssid,%s,passphrase,%s,type,SAE,encpType,aes-ccmp,keymgmttype,wpa2" % (ifname, "test-sae", "12345678"))
        sigma_dut_cmd_check("sta_associate,interface,%s,ssid,%s,channel,1" % (ifname, "test-sae"))
        ev = hapd.wait_event(["OCV-FAILURE"], timeout=1)
        if ev is None:
            raise Exception("OCV failure for EAPOL-Key msg 2/4 not reported")
        if "addr=" + dev[0].own_addr() not in ev:
            raise Exception("Unexpected OCV failure addr: " + ev)
        if "frame=eapol-key-m2" not in ev:
            raise Exception("Unexpected OCV failure frame: " + ev)
        if "error=primary channel mismatch" not in ev:
            raise Exception("Unexpected OCV failure error: " + ev)

        sigma_dut_cmd_check("sta_reset_default,interface," + ifname)
    finally:
        stop_sigma_dut(sigma)

def test_sigma_dut_ap_ocv(dev, apdev, params):
    """sigma_dut controlled AP using OCV"""
    logdir = params['prefix'] + ".sigma-hostapd"
    conffile = params['prefix'] + ".sigma-conf"
    if "SAE" not in dev[0].get_capability("auth_alg"):
        raise HwsimSkip("SAE not supported")
    with HWSimRadio() as (radio, iface):
        sigma = start_sigma_dut(iface, hostapd_logdir=logdir)
        try:
            sigma_dut_cmd_check("ap_reset_default")
            sigma_dut_cmd_check("ap_set_wireless,NAME,AP,CHANNEL,1,SSID,test-sae,MODE,11ng")
            sigma_dut_cmd_check("ap_set_wireless,NAME,AP,ocvc,1")
            sigma_dut_cmd_check("ap_set_security,NAME,AP,KEYMGNT,WPA2-SAE,PSK,12345678")
            sigma_dut_cmd_check("ap_config_commit,NAME,AP")
            bssid = sigma_dut_cmd_check("ap_get_mac_address,NAME,AP")
            bssid = bssid.split(',')[3]

            with open("/tmp/sigma_dut-ap.conf", "rb") as f:
                with open(conffile, "wb") as f2:
                    f2.write(f.read())

            dev[0].set("sae_groups", "")
            dev[0].connect("test-sae", key_mgmt="SAE", psk="12345678",
                           ieee80211w="2", ocv="1", scan_freq="2412")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()
            dev[0].dump_monitor()

            sigma_dut_cmd_check("ap_set_rfeature,NAME,AP,type,WPA3,OCIFrameType,eapolM3,OCIChannel,3")
            dev[0].connect("test-sae", key_mgmt="SAE", psk="12345678",
                           ieee80211w="2", ocv="1", scan_freq="2412",
                           wait_connect=False)
            check_ocv_failure(dev[0], "EAPOL-Key msg 3/4", "eapol-key-m3",
                              bssid)
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()
            dev[0].dump_monitor()

            sigma_dut_cmd_check("ap_reset_default")
        finally:
            stop_sigma_dut(sigma)

def test_sigma_dut_gtk_rekey(dev, apdev):
    """sigma_dut controlled STA requesting GTK rekeying"""
    if "SAE" not in dev[0].get_capability("auth_alg"):
        raise HwsimSkip("SAE not supported")

    ifname = dev[0].ifname
    sigma = start_sigma_dut(ifname)

    try:
        ssid = "test-sae"
        params = hostapd.wpa2_params(ssid=ssid, passphrase="12345678")
        params['wpa_key_mgmt'] = 'SAE'
        params["ieee80211w"] = "2"
        params['sae_groups'] = '19'
        hapd = hostapd.add_ap(apdev[0], params)

        sigma_dut_cmd_check("sta_reset_default,interface,%s,prog,WPA3" % ifname)
        sigma_dut_cmd_check("sta_set_ip_config,interface,%s,dhcp,0,ip,127.0.0.11,mask,255.255.255.0" % ifname)
        sigma_dut_cmd_check("sta_set_wireless,interface,%s,program,WPA3,ocvc,1" % ifname)
        sigma_dut_cmd_check("sta_set_security,interface,%s,ssid,%s,passphrase,%s,type,SAE,encpType,aes-ccmp,keymgmttype,wpa2" % (ifname, "test-sae", "12345678"))
        sigma_dut_cmd_check("sta_associate,interface,%s,ssid,%s,channel,1" % (ifname, "test-sae"),
                            timeout=10)
        sigma_dut_wait_connected(ifname)

        dev[0].dump_monitor()
        sigma_dut_cmd_check("dev_exec_action,interface,%s,program,WPA3,KeyRotation,1" % ifname)
        ev = dev[0].wait_event(["WPA: Group rekeying completed"], timeout=5)
        if ev is None:
            raise Exception("GTK rekeying not seen")

        sigma_dut_cmd_check("sta_reset_default,interface," + ifname)
    finally:
        stop_sigma_dut(sigma)

def test_sigma_dut_ap_gtk_rekey(dev, apdev, params):
    """sigma_dut controlled AP and requested GTK rekeying"""
    logdir = params['prefix'] + ".sigma-hostapd"
    if "SAE" not in dev[0].get_capability("auth_alg"):
        raise HwsimSkip("SAE not supported")
    with HWSimRadio() as (radio, iface):
        sigma = start_sigma_dut(iface, hostapd_logdir=logdir)
        try:
            sigma_dut_cmd_check("ap_reset_default")
            sigma_dut_cmd_check("ap_set_wireless,NAME,AP,CHANNEL,1,SSID,test-sae,MODE,11ng")
            sigma_dut_cmd_check("ap_set_security,NAME,AP,KEYMGNT,WPA2-SAE,PSK,12345678")
            sigma_dut_cmd_check("ap_config_commit,NAME,AP")

            dev[0].set("sae_groups", "")
            dev[0].connect("test-sae", key_mgmt="SAE", psk="12345678",
                           ieee80211w="2", scan_freq="2412")
            dev[0].dump_monitor()

            sigma_dut_cmd_check("dev_exec_action,name,AP,interface,%s,program,WPA3,KeyRotation,1" % iface)

            ev = dev[0].wait_event(["WPA: Group rekeying completed"], timeout=5)
            if ev is None:
                raise Exception("GTK rekeying not seen")

            sigma_dut_cmd_check("ap_reset_default")
        finally:
            stop_sigma_dut(sigma)

def test_sigma_dut_sae_pk(dev, apdev):
    """sigma_dut controlled STA using SAE-PK"""
    if "SAE" not in dev[0].get_capability("auth_alg"):
        raise HwsimSkip("SAE not supported")

    ifname = dev[0].ifname
    sigma = start_sigma_dut(ifname)

    ssid = "SAE-PK test"
    pw = "hbbi-f4xq-b45g"
    m = "d2e5fa27d1be8897f987f2d480d2af6b"
    pk = "MHcCAQEEIAJIGlfnteonDb7rQyP/SGQjwzrZAnfrXIm4280VWajYoAoGCCqGSM49AwEHoUQDQgAEeRkstKQV+FSAMqBayqFknn2nAQsdsh/MhdX6tiHOTAFin/sUMFRMyspPtIu7YvlKdsexhI0jPVhaYZn1jKWhZg=="

    try:
        params = hostapd.wpa2_params(ssid=ssid, passphrase="12345678")
        params['wpa_key_mgmt'] = 'SAE'
        params["ieee80211w"] = "2"
        params['sae_groups'] = '19'
        params['sae_password'] = ['%s|pk=%s:%s' % (pw, m, pk)]
        hapd = hostapd.add_ap(apdev[0], params)

        sigma_dut_cmd_check("sta_reset_default,interface,%s,prog,WPA3" % ifname)
        sigma_dut_cmd_check("sta_set_ip_config,interface,%s,dhcp,0,ip,127.0.0.11,mask,255.255.255.0" % ifname)
        sigma_dut_cmd_check("sta_set_wireless,interface,%s,program,WPA3" % ifname)
        sigma_dut_cmd_check("sta_set_security,interface,%s,ssid,%s,passphrase,%s,type,SAE,encpType,aes-ccmp,keymgmttype,wpa2,sae_pk,1" % (ifname, ssid, pw))
        sigma_dut_cmd_check("sta_associate,interface,%s,ssid,%s,channel,1" % (ifname, ssid),
                            timeout=10)
        sigma_dut_wait_connected(ifname)
        dev[0].dump_monitor()

        sigma_dut_cmd_check("sta_reset_default,interface," + ifname)
    finally:
        stop_sigma_dut(sigma)

def run_sigma_dut_ap_sae_pk(conffile, dev, ssid, pw, keypair, m, failure,
                            status=None, omit=False, immediate=False, sig=None):
    sigma_dut_cmd_check("ap_reset_default")
    sigma_dut_cmd_check("ap_set_wireless,NAME,AP,CHANNEL,1,SSID,%s,MODE,11ng" % ssid)
    cmd = "ap_set_security,NAME,AP,AKMSuiteType,8,PairwiseCipher,AES-CCMP-128,GroupCipher,AES-CCMP-128,GroupMgntCipher,BIP-CMAC-128,PMF,Required,PSK,%s,sae_pk,1,Transition_Disable,1,Transition_Disable_Index,0,SAE_PK_KeyPair,%s,SAE_PK_Modifier,%s" % (pw, keypair, m)
    if status is not None:
        cmd += ",SAE_Commit_StatusCode,%d" % status
    if omit:
        cmd += ",SAE_PK_Omit,1"
    if immediate:
        cmd += ",SAE_Confirm_Immediate,1"
    if sig:
        cmd += ",SAE_PK_KeyPairSigOverride," + sig
    sigma_dut_cmd_check(cmd)
    sigma_dut_cmd_check("ap_config_commit,NAME,AP")
    bssid = sigma_dut_cmd_check("ap_get_mac_address,NAME,AP")
    bssid = bssid.split(',')[3]

    with open("/tmp/sigma_dut-ap.conf", "rb") as f:
        with open(conffile, "ab") as f2:
            f2.write(f.read())
            f2.write('\n'.encode())

    dev.set("sae_groups", "")
    dev.connect(ssid, key_mgmt="SAE", sae_password=pw, ieee80211w="2",
                scan_freq="2412", wait_connect=False)

    ev = dev.wait_event(["CTRL-EVENT-CONNECTED",
                         "CTRL-EVENT-SSID-TEMP-DISABLED"], timeout=15)
    if ev is None:
        raise Exception("No connection result reported")

    bss = dev.get_bss(bssid)
    if 'flags' not in bss:
        raise Exception("Could not get BSS flags from BSS table")
    if "[SAE-H2E]" not in bss['flags'] or "[SAE-PK]" not in bss['flags']:
        raise Exception("Unexpected BSS flags: " + bss['flags'])

    if failure:
        if "CTRL-EVENT-CONNECTED" in ev:
            raise Exception("Unexpected connection")
        dev.request("REMOVE_NETWORK all")
    else:
        if "CTRL-EVENT-CONNECTED" not in ev:
            raise Exception("Connection failed")
        dev.request("REMOVE_NETWORK all")
        dev.wait_disconnected()
    dev.dump_monitor()

    sigma_dut_cmd_check("ap_reset_default")

def test_sigma_dut_ap_sae_pk(dev, apdev, params):
    """sigma_dut controlled AP using SAE-PK"""
    logdir = params['prefix'] + ".sigma-hostapd"
    conffile = params['prefix'] + ".sigma-conf"
    if "SAE" not in dev[0].get_capability("auth_alg"):
        raise HwsimSkip("SAE not supported")
    tests = [("SAEPK-4.7.1.1", "ya3o-zvm2-r4so", "saepk1.pem",
              "faa1ef5094bdb4cb2836332ca2c09839", False),
             ("SAEPK-4.7.1.2", "xcc2-qwru-yg23", "saepk1.pem",
              "b1b30107eb74de2f25afd079bb4196c1", False),
             ("SAEPK-4.7.1.3", "skqz-6scq-zcqv", "saepk1.pem",
              "4c0ff61465e0f298510254ff54916c71", False),
             ("SAEPK-4.7.1.4", "r6em-rya4-tqfa", "saepkP384.pem",
              "fb811655209e9edf347a675ddd3e9c82", False),
             ("SAEPK-4.7.1.5", "6kjo-umvi-7x3w", "saepkP521.pem",
              "cccb76bc0f113ab754826ba9538d66f5", False),
             ("SAEPK-5.7.1.1", "sw4h-re63-wgqg", "saepk1.pem",
              "0d126f302d85ac809a6a4229dbbe3c75", False),
             ("SAEPK-5.7.1.2", "wewq-r4kg-4ioz-xb2p", "saepk1.pem",
              "d6b1d8924b1a462677e67b3bbfe73977", False),
             ("SAEPK-5.7.1.3", "vb3v-5skk-5eft-v4hu-w2c5", "saepk1.pem",
              "41f8cfceb96ebc5c8af9677d22749fad", False),
             ("SAEPK-5.7.1.4", "2qsw-6tgy-xnwa-s7lo-75tq-qggr", "saepk1.pem",
              "089e8d4a3a79ec637c54dd7bd61972f2", False),
             ("SAE-PK test", "hbbi-f4xq-b45g", "saepkP256.pem",
              "d2e5fa27d1be8897f987f2d480d2af6b", False),
             ("SAE-PK test", "hbbi-f4xq-b457-jje4", "saepkP256.pem",
              "d2e5fa27d1be8897f987f2d480d2af6b", False),
             ("SAE-PK test", "hbbi-f4xq-b457-jjew-muei", "saepkP256.pem",
              "d2e5fa27d1be8897f987f2d480d2af6b", False),
             ("SAE-PK test", "hbbi-f4xq-b457-jjew-muey-fod3", "saepkP256.pem",
              "d2e5fa27d1be8897f987f2d480d2af6b", False),
             ("SAEPK-5.7.1.1", "sw4h-re63-wgqg", "saepk1.pem",
              "0d126f302d85ac809a6a4229dbbe3c75", False),
             ("SAEPK-5.7.1.10", "tkor-7nb3-r7tv", "saepkP384.pem",
              "af1a3df913fc0103f65f105ed1472277", False),
             ("SAEPK-5.7.1.11", "yjl3-vfvu-w6r3", "saepkP521.pem",
              "24dadf9d253c4169c9647a21cb54fc57", False),
             ("SAEPK-5.7.2.1", "rntm-tkrp-xgke", "saepk1.pem",
              "cd38ccce3baff627d09bee7b9530d6ce", False),
             ("SAEPK-5.7.2.2", "7lt7-7dqt-6abk", "saepk1.pem",
              "a22fc8489932597c9e83de62dec02b21", False),
             ("SAEPK-5.7.2.3", "sw4h-re63-wgqg", "saepk2.pem",
              "1f4a4c7d290d97e0b6ab0cbbbfa0726d", True),
             ("SAEPK-5.7.2.4", "rmj3-ya7b-42k4", "saepk1.pem",
              "5f65e2bc37f8494de7a605ff615c8b6a", False),
             ("SAEPK-5.7.2.4", "rmj3-ya7b-42k4", "saepk2.pem",
              "5f65e2bc37f8494de7a605ff615c8b6a", True),
             ("SAEPK-5.7.3", "4322-ufus-4bhm", "saepk1.pem",
              "21ede99abc46679646693cafe4677d4e", False)]

    with HWSimRadio() as (radio, iface):
        sigma = start_sigma_dut(iface, hostapd_logdir=logdir)
        try:
            for ssid, pw, keypair, m, failure in tests:
                run_sigma_dut_ap_sae_pk(conffile, dev[0], ssid, pw, keypair, m,
                                        failure)
        finally:
            stop_sigma_dut(sigma)

def test_sigma_dut_ap_sae_pk_misbehavior(dev, apdev, params):
    """sigma_dut controlled AP using SAE-PK misbehavior"""
    logdir = params['prefix'] + ".sigma-hostapd"
    conffile = params['prefix'] + ".sigma-conf"
    if "SAE" not in dev[0].get_capability("auth_alg"):
        raise HwsimSkip("SAE not supported")
    ssid = "SAEPK-4.7.1.1"
    pw = "rmj3-ya7b-42k4"
    keypair = "saepk1.pem"
    m = "faa1ef5094bdb4cb2836332ca2c09839"

    with HWSimRadio() as (radio, iface):
        sigma = start_sigma_dut(iface, hostapd_logdir=logdir)
        try:
            run_sigma_dut_ap_sae_pk(conffile, dev[0], ssid, pw, keypair, m,
                                    True, status=126)
            run_sigma_dut_ap_sae_pk(conffile, dev[0], ssid, pw, keypair, m,
                                    True, omit=True)
            run_sigma_dut_ap_sae_pk(conffile, dev[0], ssid, pw, keypair, m,
                                    True, status=126, omit=True, immediate=True)
            run_sigma_dut_ap_sae_pk(conffile, dev[0], ssid, pw, keypair, m,
                                    True, sig="saepk2.pem")
        finally:
            stop_sigma_dut(sigma)

def run_sigma_dut_ap_sae_pk_mixed(conffile, dev, ssid, pw, keypair, m, failure):
    sigma_dut_cmd_check("ap_reset_default")
    sigma_dut_cmd_check("ap_set_wireless,NAME,AP,CHANNEL,1,SSID,%s,MODE,11ng" % ssid)
    cmd = "ap_set_security,NAME,AP,AKMSuiteType,2;8,PairwiseCipher,AES-CCMP-128,GroupCipher,AES-CCMP-128,GroupMgntCipher,BIP-CMAC-128,PMF,Required,PSK,%s,sae_pk,0,Transition_Disable,0" % (pw)
    sigma_dut_cmd_check(cmd)
    sigma_dut_cmd_check("ap_config_commit,NAME,AP")
    bssid = sigma_dut_cmd_check("ap_get_mac_address,NAME,AP")
    bssid = bssid.split(',')[3]

    with open("/tmp/sigma_dut-ap.conf", "rb") as f:
        with open(conffile, "ab") as f2:
            f2.write(f.read())
            f2.write('\n'.encode())

    sigma_dut_cmd_check("ap_set_rfeature,NAME,AP,type,WPA3,Transition_Disable,1,Transition_Disable_Index,0")

    dev[0].set("sae_groups", "")
    dev[0].connect(ssid, key_mgmt="SAE", sae_password=pw, ieee80211w="2",
                   scan_freq="2412")
    dev[1].connect(ssid, key_mgmt="WPA-PSK", psk=pw, ieee80211w="2",
                   scan_freq="2412")

    sigma_dut_cmd_check("ap_reset_default")

def test_sigma_dut_ap_sae_pk_mixed(dev, apdev, params):
    """sigma_dut controlled AP using SAE-PK(disabled) and PSK"""
    logdir = params['prefix'] + ".sigma-hostapd"
    conffile = params['prefix'] + ".sigma-conf"
    if "SAE" not in dev[0].get_capability("auth_alg"):
        raise HwsimSkip("SAE not supported")
    ssid = "SAEPK-5.7.3"
    pw = "4322-ufus-4bhm"
    keypair = "saepk1.pem"
    m = "21ede99abc46679646693cafe4677d4e"

    with HWSimRadio() as (radio, iface):
        sigma = start_sigma_dut(iface, hostapd_logdir=logdir)
        try:
            run_sigma_dut_ap_sae_pk_mixed(conffile, dev, ssid, pw, keypair,
                                          m, False)
        finally:
            stop_sigma_dut(sigma)

def test_sigma_dut_client_privacy(dev, apdev, params):
    """sigma_dut client privacy"""
    logdir = params['logdir']

    ssid = "test"
    params = hostapd.wpa2_params(ssid=ssid, passphrase="12345678")
    hapd = hostapd.add_ap(apdev[0], params)

    ifname = dev[0].ifname
    addr = dev[0].own_addr()
    sigma = start_sigma_dut(ifname)
    try:
        sigma_dut_cmd_check("sta_reset_default,interface,%s,prog,WPA3" % ifname)
        sigma_dut_cmd_check("sta_set_wireless,interface,%s,program,WPA3,ClientPrivacy,1" % ifname)
        cmd = "sta_scan,Interface,%s,ChnlFreq,2412,WaitCompletion,1" % dev[0].ifname
        sigma_dut_cmd_check(cmd, timeout=10)
        time.sleep(2)
        sigma_dut_cmd_check("sta_set_ip_config,interface,%s,dhcp,0,ip,127.0.0.11,mask,255.255.255.0" % ifname)
        sigma_dut_cmd_check("sta_set_psk,interface,%s,ssid,%s,passphrase,%s,encpType,aes-ccmp,keymgmttype,wpa2" % (ifname, ssid, "12345678"))
        sigma_dut_cmd_check("sta_associate,interface,%s,ssid,%s,channel,1" % (ifname, ssid),
                            timeout=10)
        sigma_dut_wait_connected(ifname)
        sigma_dut_cmd_check("sta_get_ip_config,interface," + ifname)
        sigma_dut_cmd_check("sta_disconnect,interface," + ifname)
        sigma_dut_cmd_check("sta_reset_default,interface," + ifname)
    finally:
        stop_sigma_dut(sigma)
        dev[0].set("mac_addr", "0", allow_fail=True)
        dev[0].set("rand_addr_lifetime", "60", allow_fail=True)
        dev[0].request("MAC_RAND_SCAN enable=0 all")
        dev[0].set("preassoc_mac_addr", "0", allow_fail=True)
        dev[0].set("gas_rand_mac_addr", "0", allow_fail=True)
        dev[0].set("gas_rand_addr_lifetime", "60", allow_fail=True)

    out = run_tshark(os.path.join(logdir, "hwsim0.pcapng"),
                     "wlan.addr == " + addr,
                     display=["wlan.ta"])
    res = out.splitlines()
    if len(res) > 0:
        raise Exception("Permanent address used unexpectedly")

def test_sigma_dut_wpa3_inject_frame(dev, apdev):
    """sigma_dut and WPA3 frame inject"""
    if "SAE" not in dev[0].get_capability("auth_alg"):
        raise HwsimSkip("SAE not supported")

    ifname = dev[0].ifname
    sigma = start_sigma_dut(ifname)

    try:
        ssid = "test-sae"
        params = hostapd.wpa2_params(ssid=ssid, passphrase="12345678")
        params['wpa_key_mgmt'] = 'SAE'
        params["ieee80211w"] = "2"
        params["ocv"] = "1"
        params['sae_groups'] = '19 20 21'
        hapd = hostapd.add_ap(apdev[0], params)

        sigma_dut_cmd_check("sta_reset_default,interface,%s,prog,WPA3" % ifname)
        sigma_dut_cmd_check("sta_set_ip_config,interface,%s,dhcp,0,ip,127.0.0.11,mask,255.255.255.0" % ifname)
        sigma_dut_cmd_check("sta_set_wireless,interface,%s,program,WPA3,ocvc,1" % ifname)
        sigma_dut_cmd_check("sta_set_security,interface,%s,ssid,%s,passphrase,%s,type,SAE,encpType,aes-ccmp,keymgmttype,wpa2" % (ifname, "test-sae", "12345678"))
        sigma_dut_cmd_check("sta_associate,interface,%s,ssid,%s,channel,1" % (ifname, "test-sae"),
                            timeout=10)
        sigma_dut_wait_connected(ifname)
        sigma_dut_cmd("dev_send_frame,interface,%s,program,WPA3,framename,SAQueryReq,OCIChannel,2" % ifname)
        sigma_dut_cmd("dev_send_frame,interface,%s,program,WPA3,framename,SAQueryReq,OCIChannel,1" % ifname)
        sigma_dut_cmd("dev_send_frame,interface,%s,program,WPA3,framename,ReassocReq" % ifname)
        hwsim_utils.test_connectivity(dev[0], hapd)
        sigma_dut_cmd_check("sta_reset_default,interface," + ifname)
    finally:
        stop_sigma_dut(sigma)
