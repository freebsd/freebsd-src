# WPA2-Personal OCV tests
# Copyright (c) 2018, Mathy Vanhoef
#
# This software may be distributed under the terms of the BSD license.
# See README for more details

from remotehost import remote_compatible
import binascii, struct
import logging, time
logger = logging.getLogger()

import hostapd
from wpasupplicant import WpaSupplicant
import hwsim_utils
from utils import *
from test_erp import start_erp_as
from test_ap_ft import ft_params1, ft_params2
from test_ap_psk import parse_eapol, build_eapol, pmk_to_ptk, eapol_key_mic, recv_eapol, send_eapol, reply_eapol, build_eapol_key_3_4, aes_wrap, pad_key_data

#TODO: Refuse setting up AP with OCV but without MFP support
#TODO: Refuse to connect to AP that advertises OCV but not MFP

def make_ocikde(op_class, channel, seg1_idx):
    WLAN_EID_VENDOR_SPECIFIC = 221
    RSN_KEY_DATA_OCI = b"\x00\x0f\xac\x0d"

    data = RSN_KEY_DATA_OCI + struct.pack("<BBB", op_class, channel, seg1_idx)
    ocikde = struct.pack("<BB", WLAN_EID_VENDOR_SPECIFIC, len(data)) + data

    return ocikde

def ocv_setup_ap(apdev, params):
    ssid = "test-wpa2-ocv"
    passphrase = "qwertyuiop"
    params.update(hostapd.wpa2_params(ssid=ssid, passphrase=passphrase))
    try:
        hapd = hostapd.add_ap(apdev, params)
    except Exception as e:
        if "Failed to set hostapd parameter ocv" in str(e):
            raise HwsimSkip("OCV not supported")
        raise
    return hapd, ssid, passphrase

def build_eapol_key_1_2(kck, key_data, replay_counter=3, key_info=0x1382,
                        extra_len=0, descr_type=2, key_len=16):
    msg = {}
    msg['version'] = 2
    msg['type'] = 3
    msg['length'] = 95 + len(key_data) + extra_len

    msg['descr_type'] = descr_type
    msg['rsn_key_info'] = key_info
    msg['rsn_key_len'] = key_len
    msg['rsn_replay_counter'] = struct.pack('>Q', replay_counter)
    msg['rsn_key_nonce'] = binascii.unhexlify('0000000000000000000000000000000000000000000000000000000000000000')
    msg['rsn_key_iv'] = binascii.unhexlify('00000000000000000000000000000000')
    msg['rsn_key_rsc'] = binascii.unhexlify('0000000000000000')
    msg['rsn_key_id'] = binascii.unhexlify('0000000000000000')
    msg['rsn_key_data_len'] = len(key_data)
    msg['rsn_key_data'] = key_data
    eapol_key_mic(kck, msg)
    return msg

def build_eapol_key_2_2(kck, key_data, replay_counter=3, key_info=0x0302,
                        extra_len=0, descr_type=2, key_len=16):
    return build_eapol_key_1_2(kck, key_data, replay_counter, key_info,
                               extra_len, descr_type, key_len)

@remote_compatible
def test_wpa2_ocv(dev, apdev):
    """OCV on 2.4 GHz"""
    params = {"channel": "1",
              "ieee80211w": "2",
              "ocv": "1"}
    hapd, ssid, passphrase = ocv_setup_ap(apdev[0], params)
    for ocv in range(2):
        dev[0].connect(ssid, psk=passphrase, scan_freq="2412", ocv=str(ocv),
                       ieee80211w="1")
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()

@remote_compatible
def test_wpa2_ocv_5ghz(dev, apdev):
    """OCV on 5 GHz"""
    try:
        run_wpa2_ocv_5ghz(dev, apdev)
    finally:
        set_world_reg(apdev[0], apdev[1], dev[0])
        dev[0].flush_scan_cache()

def run_wpa2_ocv_5ghz(dev, apdev):
    params = {"hw_mode": "a",
              "channel": "40",
              "ieee80211w": "2",
              "country_code": "US",
              "ocv": "1"}
    hapd, ssid, passphrase = ocv_setup_ap(apdev[0], params)
    for ocv in range(2):
        dev[0].connect(ssid, psk=passphrase, scan_freq="5200", ocv=str(ocv),
                       ieee80211w="1")
        dev[0].wait_regdom(country_ie=True)
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()

@remote_compatible
def test_wpa2_ocv_ht20(dev, apdev):
    """OCV with HT20 channel"""
    params = {"channel": "6",
              "ieee80211n": "1",
              "ieee80211w": "1",
              "ocv": "1"}
    hapd, ssid, passphrase = ocv_setup_ap(apdev[0], params)
    for ocv in range(2):
        dev[0].connect(ssid, psk=passphrase, scan_freq="2437", ocv=str(ocv),
                       ieee80211w="1", disable_ht="1")
        dev[1].connect(ssid, psk=passphrase, scan_freq="2437", ocv=str(ocv),
                       ieee80211w="1")
        dev[0].request("REMOVE_NETWORK all")
        dev[1].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()
        dev[1].wait_disconnected()

@remote_compatible
def test_wpa2_ocv_ht40(dev, apdev):
    """OCV with HT40 channel"""
    try:
        run_wpa2_ocv_ht40(dev, apdev)
    finally:
        set_world_reg(apdev[0], apdev[1], dev[0])
        dev[0].flush_scan_cache()
        dev[1].flush_scan_cache()

def run_wpa2_ocv_ht40(dev, apdev):
    for channel, capab, freq, mode in [("6", "[HT40-]", "2437", "g"),
                                       ("6", "[HT40+]", "2437", "g"),
                                       ("40", "[HT40-]", "5200", "a"),
                                       ("36", "[HT40+]", "5180", "a")]:
        params = {"hw_mode": mode,
                  "channel": channel,
                  "country_code": "US",
                  "ieee80211n": "1",
                  "ht_capab": capab,
                  "ieee80211w": "1",
                  "ocv": "1"}
        hapd, ssid, passphrase = ocv_setup_ap(apdev[0], params)
        dev[0].flush_scan_cache()
        dev[1].flush_scan_cache()
        for ocv in range(2):
            dev[0].connect(ssid, psk=passphrase, scan_freq=freq, ocv=str(ocv),
                           ieee80211w="1", disable_ht="1")
            dev[1].connect(ssid, psk=passphrase, scan_freq=freq, ocv=str(ocv),
                           ieee80211w="1")
            dev[0].wait_regdom(country_ie=True)
            dev[0].request("REMOVE_NETWORK all")
            dev[1].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()
            dev[1].wait_disconnected()
        hapd.disable()

@remote_compatible
def test_wpa2_ocv_vht40(dev, apdev):
    """OCV with VHT40 channel"""
    try:
        run_wpa2_ocv_vht40(dev, apdev)
    finally:
        set_world_reg(apdev[0], apdev[1], dev[0])
        dev[0].flush_scan_cache()
        dev[1].flush_scan_cache()
        dev[2].flush_scan_cache()

def run_wpa2_ocv_vht40(dev, apdev):
    for channel, capab, freq in [("40", "[HT40-]", "5200"),
                                 ("36", "[HT40+]", "5180")]:
        params = {"hw_mode": "a",
                  "channel": channel,
                  "country_code": "US",
                  "ht_capab": capab,
                  "ieee80211n": "1",
                  "ieee80211ac": "1",
                  "vht_oper_chwidth": "0",
                  "vht_oper_centr_freq_seg0_idx": "38",
                  "ieee80211w": "1",
                  "ocv": "1"}
        hapd, ssid, passphrase = ocv_setup_ap(apdev[0], params)
        dev[0].flush_scan_cache()
        dev[1].flush_scan_cache()
        dev[2].flush_scan_cache()
        for ocv in range(2):
            dev[0].connect(ssid, psk=passphrase, scan_freq=freq, ocv=str(ocv),
                           ieee80211w="1", disable_ht="1")
            dev[1].connect(ssid, psk=passphrase, scan_freq=freq, ocv=str(ocv),
                           ieee80211w="1", disable_vht="1")
            dev[2].connect(ssid, psk=passphrase, scan_freq=freq, ocv=str(ocv),
                           ieee80211w="1")
            dev[0].wait_regdom(country_ie=True)
            dev[0].request("REMOVE_NETWORK all")
            dev[1].request("REMOVE_NETWORK all")
            dev[2].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()
            dev[1].wait_disconnected()
            dev[2].wait_disconnected()
        hapd.disable()

@remote_compatible
def test_wpa2_ocv_vht80(dev, apdev):
    """OCV with VHT80 channel"""
    try:
        run_wpa2_ocv_vht80(dev, apdev)
    finally:
        set_world_reg(apdev[0], apdev[1], dev[0])
        dev[0].flush_scan_cache()
        dev[1].flush_scan_cache()
        dev[2].flush_scan_cache()

def run_wpa2_ocv_vht80(dev, apdev):
    for channel, capab, freq in [("40", "[HT40-]", "5200"),
                                 ("36", "[HT40+]", "5180")]:
        params = {"hw_mode": "a",
                  "channel": channel,
                  "country_code": "US",
                  "ht_capab": capab,
                  "ieee80211n": "1",
                  "ieee80211ac": "1",
                  "vht_oper_chwidth": "1",
                  "vht_oper_centr_freq_seg0_idx": "42",
                  "ieee80211w": "1",
                  "ocv": "1"}
        hapd, ssid, passphrase = ocv_setup_ap(apdev[0], params)
        for ocv in range(2):
            dev[0].connect(ssid, psk=passphrase, scan_freq=freq, ocv=str(ocv),
                           ieee80211w="1", disable_ht="1")
            dev[1].connect(ssid, psk=passphrase, scan_freq=freq, ocv=str(ocv),
                           ieee80211w="1", disable_vht="1")
            dev[2].connect(ssid, psk=passphrase, scan_freq=freq, ocv=str(ocv),
                           ieee80211w="1")
            dev[0].wait_regdom(country_ie=True)
            dev[0].request("REMOVE_NETWORK all")
            dev[1].request("REMOVE_NETWORK all")
            dev[2].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()
            dev[1].wait_disconnected()
            dev[2].wait_disconnected()
        hapd.disable()

@remote_compatible
def test_wpa2_ocv_vht160(dev, apdev):
    """OCV with VHT160 channel"""
    try:
        run_wpa2_ocv_vht160(dev, apdev)
    finally:
        set_world_reg(apdev[0], apdev[1], dev[0])
        dev[0].flush_scan_cache()
        dev[1].flush_scan_cache()
        dev[2].flush_scan_cache()

def run_wpa2_ocv_vht160(dev, apdev):
    for channel, capab, freq in [("100", "[HT40+]", "5500"),
                                 ("104", "[HT40-]", "5520")]:
        params = {"hw_mode": "a",
                  "channel": channel,
                  "country_code": "ZA",
                  "ht_capab": capab,
                  "vht_capab": "[VHT160]",
                  "ieee80211n": "1",
                  "ieee80211ac": "1",
                  "vht_oper_chwidth": "2",
                  "vht_oper_centr_freq_seg0_idx": "114",
                  "ieee80211w": "1",
                  "ocv": "1"}
        hapd, ssid, passphrase = ocv_setup_ap(apdev[0], params)
        for ocv in range(2):
            dev[0].connect(ssid, psk=passphrase, scan_freq=freq, ocv=str(ocv),
                           ieee80211w="1", disable_ht="1")
            dev[1].connect(ssid, psk=passphrase, scan_freq=freq, ocv=str(ocv),
                           ieee80211w="1", disable_vht="1")
            dev[2].connect(ssid, psk=passphrase, scan_freq=freq, ocv=str(ocv),
                           ieee80211w="1")
            dev[0].wait_regdom(country_ie=True)
            dev[0].request("REMOVE_NETWORK all")
            dev[1].request("REMOVE_NETWORK all")
            dev[2].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()
            dev[1].wait_disconnected()
            dev[2].wait_disconnected()
        hapd.disable()

@remote_compatible
def test_wpa2_ocv_vht80plus80(dev, apdev):
    """OCV with VHT80+80 channel"""
    try:
        run_wpa2_ocv_vht80plus80(dev, apdev)
    finally:
        set_world_reg(apdev[0], apdev[1], dev[0])
        dev[0].wait_event(["CTRL-EVENT-REGDOM-CHANGE"], timeout=0.5)
        dev[0].flush_scan_cache()
        dev[1].flush_scan_cache()
        dev[2].flush_scan_cache()

def run_wpa2_ocv_vht80plus80(dev, apdev):
    for channel, capab, freq in [("36", "[HT40+]", "5180"),
                                 ("40", "[HT40-]", "5200")]:
        params = {"hw_mode": "a",
                  "channel": channel,
                  "country_code": "US",
                  "ht_capab": capab,
                  "vht_capab": "[VHT160-80PLUS80]",
                  "ieee80211n": "1",
                  "ieee80211ac": "1",
                  "vht_oper_chwidth": "3",
                  "vht_oper_centr_freq_seg0_idx": "42",
                  "vht_oper_centr_freq_seg1_idx": "155",
                  "ieee80211w": "1",
                  "ieee80211d": "1",
                  "ieee80211h": "1",
                  "ocv": "1"}
        hapd, ssid, passphrase = ocv_setup_ap(apdev[0], params)
        for ocv in range(2):
            dev[0].connect(ssid, psk=passphrase, scan_freq=freq, ocv=str(ocv),
                           ieee80211w="1", disable_ht="1")
            dev[1].connect(ssid, psk=passphrase, scan_freq=freq, ocv=str(ocv),
                           ieee80211w="1", disable_vht="1")
            dev[2].connect(ssid, psk=passphrase, scan_freq=freq, ocv=str(ocv),
                           ieee80211w="1")
            dev[0].wait_regdom(country_ie=True)
            dev[0].request("REMOVE_NETWORK all")
            dev[1].request("REMOVE_NETWORK all")
            dev[2].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()
            dev[1].wait_disconnected()
            dev[2].wait_disconnected()
        for i in range(3):
            dev[i].connect(ssid, psk=passphrase, scan_freq=freq, ocv=str(ocv),
                           ieee80211w="1")
            if i == 0:
                dev[i].wait_regdom(country_ie=True)
        hapd.disable()
        for i in range(3):
            dev[i].request("DISCONNECT")
        for i in range(3):
            dev[i].disconnect_and_stop_scan()

class APConnection:
    def init_params(self):
        # Static parameters
        self.ssid = "test-wpa2-ocv"
        self.passphrase = "qwertyuiop"
        self.psk = "c2c6c255af836bed1b3f2f1ded98e052f5ad618bb554e2836757b55854a0eab7"

        # Dynamic parameters
        self.hapd = None
        self.addr = None
        self.rsne = None
        self.kck = None
        self.kek = None
        self.msg = None
        self.bssid = None
        self.anonce = None
        self.snonce = None

    def __init__(self, apdev, dev, params):
        self.init_params()

        # By default, OCV is enabled for both the client and AP. The following
        # parameters can be used to disable OCV for the client or AP.
        ap_ocv = params.pop("ap_ocv", "1")
        sta_ocv = params.pop("sta_ocv", "1")

        freq = params.pop("freq")
        params.update(hostapd.wpa2_params(ssid=self.ssid,
                                          passphrase=self.passphrase))
        params["wpa_pairwise_update_count"] = "10"
        params["ocv"] = ap_ocv
        try:
            self.hapd = hostapd.add_ap(apdev, params)
        except Exception as e:
            if "Failed to set hostapd parameter ocv" in str(e):
                raise HwsimSkip("OCV not supported")
            raise
        self.hapd.request("SET ext_eapol_frame_io 1")
        dev.request("SET ext_eapol_frame_io 1")

        self.bssid = apdev['bssid']
        pmk = binascii.unhexlify("c2c6c255af836bed1b3f2f1ded98e052f5ad618bb554e2836757b55854a0eab7")

        if sta_ocv != "0":
            self.rsne = binascii.unhexlify("301a0100000fac040100000fac040100000fac0280400000000fac06")
        else:
            self.rsne = binascii.unhexlify("301a0100000fac040100000fac040100000fac0280000000000fac06")
        self.snonce = binascii.unhexlify('1111111111111111111111111111111111111111111111111111111111111111')

        dev.connect(self.ssid, raw_psk=self.psk, scan_freq=freq, ocv=sta_ocv,
                    ieee80211w="1", wait_connect=False)
        if "country_code" in params:
            dev.wait_regdom(country_ie=True)
        self.addr = dev.p2p_interface_addr()

        # Wait for EAPOL-Key msg 1/4 from hostapd to determine when associated
        self.msg = recv_eapol(self.hapd)
        self.anonce = self.msg['rsn_key_nonce']
        (ptk, self.kck, self.kek) = pmk_to_ptk(pmk, self.addr, self.bssid,
                                               self.snonce, self.anonce)

    # hapd, addr, rsne, kck, msg, anonce, snonce
    def test_bad_oci(self, logmsg, op_class, channel, seg1_idx):
        logger.debug("Bad OCI element: " + logmsg)
        if op_class is None:
            ocikde = b''
        else:
            ocikde = make_ocikde(op_class, channel, seg1_idx)

        reply_eapol("2/4", self.hapd, self.addr, self.msg, 0x010a, self.snonce,
                    self.rsne + ocikde, self.kck)
        self.msg = recv_eapol(self.hapd)
        if self.anonce != self.msg['rsn_key_nonce'] or self.msg["rsn_key_info"] != 138:
            raise Exception("Didn't receive retransmitted 1/4")

    def confirm_valid_oci(self, op_class, channel, seg1_idx):
        logger.debug("Valid OCI element to complete handshake")
        ocikde = make_ocikde(op_class, channel, seg1_idx)

        reply_eapol("2/4", self.hapd, self.addr, self.msg, 0x010a, self.snonce,
                    self.rsne + ocikde, self.kck)
        self.msg = recv_eapol(self.hapd)
        if self.anonce != self.msg['rsn_key_nonce'] or self.msg["rsn_key_info"] != 5066:
            raise Exception("Didn't receive 3/4 in response to valid 2/4")

        reply_eapol("4/4", self.hapd, self.addr, self.msg, 0x030a, None, None,
                    self.kck)
        self.hapd.wait_sta(timeout=15)

@remote_compatible
def test_wpa2_ocv_ap_mismatch(dev, apdev):
    """OCV AP mismatch"""
    params = {"channel": "1",
              "ieee80211w": "1",
              "freq": "2412"}
    conn = APConnection(apdev[0], dev[0], params)
    conn.test_bad_oci("element missing", None, 0, 0)
    conn.test_bad_oci("wrong channel number", 81, 6, 0)
    conn.test_bad_oci("invalid channel number", 81, 0, 0)
    conn.test_bad_oci("wrong operating class", 80, 0, 0)
    conn.test_bad_oci("invalid operating class", 0, 0, 0)
    conn.confirm_valid_oci(81, 1, 0)

@remote_compatible
def test_wpa2_ocv_ap_ht_mismatch(dev, apdev):
    """OCV AP mismatch (HT)"""
    params = {"channel": "6",
              "ht_capab": "[HT40-]",
              "ieee80211w": "1",
              "freq": "2437"}
    conn = APConnection(apdev[0], dev[0], params)
    conn.test_bad_oci("wrong primary channel", 84, 5, 0)
    conn.test_bad_oci("lower bandwidth than negotiated", 81, 6, 0)
    conn.test_bad_oci("bad upper/lower channel", 83, 6, 0)
    conn.confirm_valid_oci(84, 6, 0)

@remote_compatible
def test_wpa2_ocv_ap_vht80_mismatch(dev, apdev):
    """OCV AP mismatch (VHT80)"""
    try:
        run_wpa2_ocv_ap_vht80_mismatch(dev, apdev)
    finally:
        set_world_reg(apdev[0], apdev[1], dev[0])
        dev[0].flush_scan_cache()

def run_wpa2_ocv_ap_vht80_mismatch(dev, apdev):
    params = {"hw_mode": "a",
              "channel": "36",
              "country_code": "US",
              "ht_capab": "[HT40+]",
              "ieee80211w": "1",
              "ieee80211n": "1",
              "ieee80211ac": "1",
              "vht_oper_chwidth": "1",
              "freq": "5180",
              "vht_oper_centr_freq_seg0_idx": "42"}
    conn = APConnection(apdev[0], dev[0], params)
    conn.test_bad_oci("wrong primary channel", 128, 38, 0)
    conn.test_bad_oci("wrong primary channel", 128, 32, 0)
    conn.test_bad_oci("smaller bandwidth than negotiated", 116, 36, 0)
    conn.test_bad_oci("smaller bandwidth than negotiated", 115, 36, 0)
    conn.confirm_valid_oci(128, 36, 0)

    dev[0].dump_monitor()
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

@remote_compatible
def test_wpa2_ocv_ap_vht160_mismatch(dev, apdev):
    """OCV AP mismatch (VHT160)"""
    try:
        run_wpa2_ocv_ap_vht160_mismatch(dev, apdev)
    finally:
        set_world_reg(apdev[0], apdev[1], dev[0])
        dev[0].wait_event(["CTRL-EVENT-REGDOM-CHANGE"], timeout=0.5)
        dev[0].flush_scan_cache()

def run_wpa2_ocv_ap_vht160_mismatch(dev, apdev):
    params = {"hw_mode": "a",
              "channel": "100",
              "country_code": "ZA",
              "ht_capab": "[HT40+]",
              "ieee80211w": "1",
              "ieee80211n": "1",
              "ieee80211ac": "1",
              "vht_oper_chwidth": "2",
              "freq": "5500",
              "vht_oper_centr_freq_seg0_idx": "114",
              "ieee80211d": "1",
              "ieee80211h": "1"}
    conn = APConnection(apdev[0], dev[0], params)
    conn.test_bad_oci("wrong primary channel", 129, 36, 0)
    conn.test_bad_oci("wrong primary channel", 129, 114, 0)
    conn.test_bad_oci("smaller bandwidth (20 Mhz) than negotiated", 121, 100, 0)
    conn.test_bad_oci("smaller bandwidth (40 Mhz) than negotiated", 122, 100, 0)
    conn.test_bad_oci("smaller bandwidth (80 Mhz) than negotiated", 128, 100, 0)
    conn.test_bad_oci("using 80+80 channel instead of 160", 130, 100, 155)
    conn.confirm_valid_oci(129, 100, 0)

    dev[0].dump_monitor()
    if conn.hapd:
        conn.hapd.request("DISABLE")
    dev[0].disconnect_and_stop_scan()

@remote_compatible
def test_wpa2_ocv_ap_vht80plus80_mismatch(dev, apdev):
    """OCV AP mismatch (VHT80+80)"""
    try:
        run_wpa2_ocv_ap_vht80plus80_mismatch(dev, apdev)
    finally:
        set_world_reg(apdev[0], apdev[1], dev[0])
        dev[0].wait_event(["CTRL-EVENT-REGDOM-CHANGE"], timeout=0.5)
        dev[0].flush_scan_cache()

def run_wpa2_ocv_ap_vht80plus80_mismatch(dev, apdev):
    params = {"hw_mode": "a",
              "channel": "36",
              "country_code": "US",
              "ht_capab": "[HT40+]",
              "ieee80211w": "1",
              "ieee80211n": "1",
              "ieee80211ac": "1",
              "vht_oper_chwidth": "3",
              "freq": "5180",
              "vht_oper_centr_freq_seg0_idx": "42",
              "ieee80211d": "1",
              "vht_oper_centr_freq_seg1_idx": "155",
              "ieee80211h": "1"}
    conn = APConnection(apdev[0], dev[0], params)
    conn.test_bad_oci("using 80 MHz operating class", 128, 36, 155)
    conn.test_bad_oci("wrong frequency segment 1", 130, 36, 138)
    conn.confirm_valid_oci(130, 36, 155)

    dev[0].dump_monitor()
    if conn.hapd:
        conn.hapd.request("DISABLE")
    dev[0].disconnect_and_stop_scan()

@remote_compatible
def test_wpa2_ocv_ap_unexpected1(dev, apdev):
    """OCV and unexpected OCI KDE from station"""
    params = {"channel": "1",
              "ieee80211w": "1",
              "ap_ocv": "0",
              "sta_ocv": "1",
              "freq": "2412"}
    conn = APConnection(apdev[0], dev[0], params)
    logger.debug("Client will send OCI KDE even if it was not negotiated")
    conn.confirm_valid_oci(81, 1, 0)

@remote_compatible
def test_wpa2_ocv_ap_unexpected2(dev, apdev):
    """OCV and unexpected OCI KDE from station"""
    params = {"channel": "1",
              "ieee80211w": "1",
              "ap_ocv": "1",
              "sta_ocv": "0",
              "freq": "2412"}
    conn = APConnection(apdev[0], dev[0], params)
    logger.debug("Client will send OCI KDE even if it was not negotiated")
    conn.confirm_valid_oci(81, 1, 0)

@remote_compatible
def test_wpa2_ocv_ap_retransmit_msg3(dev, apdev):
    """Verify that manually retransmitted msg 3/4 contain a correct OCI"""
    bssid = apdev[0]['bssid']
    ssid = "test-wpa2-ocv"
    passphrase = "qwertyuiop"
    psk = "c2c6c255af836bed1b3f2f1ded98e052f5ad618bb554e2836757b55854a0eab7"
    params = hostapd.wpa2_params(ssid=ssid)
    params["wpa_psk"] = psk
    params["ieee80211w"] = "1"
    params["ocv"] = "1"
    params['wpa_disable_eapol_key_retries'] = "1"
    try:
        hapd = hostapd.add_ap(apdev[0], params)
    except Exception as e:
        if "Failed to set hostapd parameter ocv" in str(e):
            raise HwsimSkip("OCV not supported")
        raise
    hapd.request("SET ext_eapol_frame_io 1")
    dev[0].request("SET ext_eapol_frame_io 1")
    dev[0].connect(ssid, psk=passphrase, scan_freq="2412", wait_connect=False,
                   ocv="1", ieee80211w="1")
    addr = dev[0].own_addr()

    # EAPOL-Key msg 1/4
    ev = hapd.wait_event(["EAPOL-TX"], timeout=15)
    if ev is None:
        raise Exception("Timeout on EAPOL-TX from hostapd")
    res = dev[0].request("EAPOL_RX " + bssid + " " + ev.split(' ')[2])
    if "OK" not in res:
        raise Exception("EAPOL_RX to wpa_supplicant failed")

    # EAPOL-Key msg 2/4
    ev = dev[0].wait_event(["EAPOL-TX"], timeout=15)
    if ev is None:
        raise Exception("Timeout on EAPOL-TX from wpa_supplicant")
    res = hapd.request("EAPOL_RX " + addr + " " + ev.split(' ')[2])
    if "OK" not in res:
        raise Exception("EAPOL_RX to hostapd failed")

    # EAPOL-Key msg 3/4
    ev = hapd.wait_event(["EAPOL-TX"], timeout=15)
    if ev is None:
        raise Exception("Timeout on EAPOL-TX from hostapd")
    logger.info("Drop the first EAPOL-Key msg 3/4")

    # Use normal EAPOL TX/RX to handle retries.
    hapd.request("SET ext_eapol_frame_io 0")
    dev[0].request("SET ext_eapol_frame_io 0")

    # Manually retransmit EAPOL-Key msg 3/4
    if "OK" not in hapd.request("RESEND_M3 " + addr):
        raise Exception("RESEND_M3 failed")

    dev[0].wait_connected()
    hwsim_utils.test_connectivity(dev[0], hapd)

def test_wpa2_ocv_ap_group_hs(dev, apdev):
    """OCV group handshake (AP)"""
    params = {"channel": "1",
              "ieee80211w": "1",
              "freq": "2412",
              "wpa_strict_rekey": "1"}
    conn = APConnection(apdev[0], dev[0], params)
    conn.confirm_valid_oci(81, 1, 0)

    conn.hapd.request("SET ext_eapol_frame_io 0")
    dev[1].connect(conn.ssid, psk=conn.passphrase, scan_freq="2412", ocv="1",
                   ieee80211w="1")
    conn.hapd.wait_sta()
    conn.hapd.request("SET ext_eapol_frame_io 1")

    # Trigger a group key handshake
    dev[1].request("DISCONNECT")
    dev[0].dump_monitor()

    # Wait for EAPOL-Key msg 1/2
    conn.msg = recv_eapol(conn.hapd)
    if conn.msg["rsn_key_info"] != 4994:
        raise Exception("Didn't receive 1/2 of group key handshake")

    # Send a EAPOL-Key msg 2/2 with a bad OCI
    logger.info("Bad OCI element")
    ocikde = make_ocikde(1, 1, 1)
    msg = build_eapol_key_2_2(conn.kck, ocikde, replay_counter=3)
    conn.hapd.dump_monitor()
    send_eapol(conn.hapd, conn.addr, build_eapol(msg))

    # Wait for retransmitted EAPOL-Key msg 1/2
    conn.msg = recv_eapol(conn.hapd)
    if conn.msg["rsn_key_info"] != 4994:
        raise Exception("Didn't receive 1/2 of group key handshake")

    # Send a EAPOL-Key msg 2/2 with a good OCI
    logger.info("Good OCI element")
    ocikde = make_ocikde(81, 1, 0)
    msg = build_eapol_key_2_2(conn.kck, ocikde, replay_counter=4)
    conn.hapd.dump_monitor()
    send_eapol(conn.hapd, conn.addr, build_eapol(msg))

    # Verify that group key handshake has completed
    ev = conn.hapd.wait_event(["EAPOL-TX"], timeout=1)
    if ev is not None:
        eapol = binascii.unhexlify(ev.split(' ')[2])
        msg = parse_eapol(eapol)
        if msg["rsn_key_info"] == 4994:
            raise Exception("AP didn't accept 2/2 of group key handshake")

class STAConnection:
    def init_params(self):
        # Static parameters
        self.ssid = "test-wpa2-ocv"
        self.passphrase = "qwertyuiop"
        self.psk = "c2c6c255af836bed1b3f2f1ded98e052f5ad618bb554e2836757b55854a0eab7"

        # Dynamic parameters
        self.hapd = None
        self.dev = None
        self.addr = None
        self.rsne = None
        self.kck = None
        self.kek = None
        self.msg = None
        self.bssid = None
        self.anonce = None
        self.snonce = None
        self.gtkie = None
        self.counter = None

    def __init__(self, apdev, dev, params, sta_params=None):
        self.init_params()
        self.dev = dev
        self.bssid = apdev['bssid']

        freq = params.pop("freq")
        if sta_params is None:
            sta_params = dict()
        if "ocv" not in sta_params:
            sta_params["ocv"] = "1"
        if "ieee80211w" not in sta_params:
            sta_params["ieee80211w"] = "1"

        params.update(hostapd.wpa2_params(ssid=self.ssid,
                                          passphrase=self.passphrase))
        params['wpa_pairwise_update_count'] = "10"

        try:
            self.hapd = hostapd.add_ap(apdev, params)
        except Exception as e:
            if "Failed to set hostapd parameter ocv" in str(e):
                raise HwsimSkip("OCV not supported")
            raise
        self.hapd.request("SET ext_eapol_frame_io 1")
        self.dev.request("SET ext_eapol_frame_io 1")
        pmk = binascii.unhexlify("c2c6c255af836bed1b3f2f1ded98e052f5ad618bb554e2836757b55854a0eab7")

        self.gtkie = binascii.unhexlify("dd16000fac010100dc11188831bf4aa4a8678d2b41498618")
        if sta_params["ocv"] != "0":
            self.rsne = binascii.unhexlify("30140100000fac040100000fac040100000fac028c40")
        else:
            self.rsne = binascii.unhexlify("30140100000fac040100000fac040100000fac028c00")

        self.dev.connect(self.ssid, raw_psk=self.psk, scan_freq=freq,
                         wait_connect=False, **sta_params)
        if "country_code" in params:
            self.dev.wait_regdom(country_ie=True)
        self.addr = dev.p2p_interface_addr()

        # Forward msg 1/4 from AP to STA
        self.msg = recv_eapol(self.hapd)
        self.anonce = self.msg['rsn_key_nonce']
        send_eapol(self.dev, self.bssid, build_eapol(self.msg))

        # Capture msg 2/4 from the STA so we can derive the session keys
        self.msg = recv_eapol(dev)
        self.snonce = self.msg['rsn_key_nonce']
        (ptk, self.kck, self.kek) = pmk_to_ptk(pmk, self.addr, self.bssid,
                                               self.snonce, self.anonce)

        self.counter = struct.unpack('>Q',
                                     self.msg['rsn_replay_counter'])[0] + 1

    def test_bad_oci(self, logmsg, op_class, channel, seg1_idx, errmsg):
        logger.info("Bad OCI element: " + logmsg)
        if op_class is None:
            ocikde = b''
        else:
            ocikde = make_ocikde(op_class, channel, seg1_idx)

        plain = self.rsne + self.gtkie + ocikde
        wrapped = aes_wrap(self.kek, pad_key_data(plain))
        msg = build_eapol_key_3_4(self.anonce, self.kck, wrapped,
                                  replay_counter=self.counter)

        self.dev.dump_monitor()
        send_eapol(self.dev, self.bssid, build_eapol(msg))
        self.counter += 1

        ev = self.dev.wait_event([errmsg], timeout=5)
        if ev is None:
            raise Exception("Bad OCI not reported")

    def confirm_valid_oci(self, op_class, channel, seg1_idx):
        logger.debug("Valid OCI element to complete handshake")
        ocikde = make_ocikde(op_class, channel, seg1_idx)

        plain = self.rsne + self.gtkie + ocikde
        wrapped = aes_wrap(self.kek, pad_key_data(plain))
        msg = build_eapol_key_3_4(self.anonce, self.kck, wrapped,
                                  replay_counter=self.counter)

        self.dev.dump_monitor()
        send_eapol(self.dev, self.bssid, build_eapol(msg))
        self.counter += 1

        self.dev.wait_connected(timeout=1)

@remote_compatible
def test_wpa2_ocv_mismatch_client(dev, apdev):
    """OCV client mismatch"""
    params = {"channel": "1",
              "ieee80211w": "1",
              "ocv": "1",
              "freq": "2412"}
    conn = STAConnection(apdev[0], dev[0], params)
    conn.test_bad_oci("element missing", None, 0, 0,
                      "did not receive mandatory OCI")
    conn.test_bad_oci("wrong channel number", 81, 6, 0,
                      "primary channel mismatch")
    conn.test_bad_oci("invalid channel number", 81, 0, 0,
                      "unable to interpret received OCI")
    conn.test_bad_oci("wrong operating class", 80, 0, 0,
                      "unable to interpret received OCI")
    conn.test_bad_oci("invalid operating class", 0, 0, 0,
                      "unable to interpret received OCI")
    conn.confirm_valid_oci(81, 1, 0)

@remote_compatible
def test_wpa2_ocv_vht160_mismatch_client(dev, apdev):
    """OCV client mismatch (VHT160)"""
    try:
        run_wpa2_ocv_vht160_mismatch_client(dev, apdev)
    finally:
        set_world_reg(apdev[0], apdev[1], dev[0])
        dev[0].wait_event(["CTRL-EVENT-REGDOM-CHANGE"], timeout=0.5)
        dev[0].flush_scan_cache()

def run_wpa2_ocv_vht160_mismatch_client(dev, apdev):
    params = {"hw_mode": "a",
              "channel": "100",
              "country_code": "ZA",
              "ht_capab": "[HT40+]",
              "ieee80211w": "1",
              "ieee80211n": "1",
              "ieee80211ac": "1",
              "vht_oper_chwidth": "2",
              "ocv": "1",
              "vht_oper_centr_freq_seg0_idx": "114",
              "freq": "5500",
              "ieee80211d": "1",
              "ieee80211h": "1"}
    sta_params = {"disable_vht": "1"}
    conn = STAConnection(apdev[0], dev[0], params, sta_params)
    conn.test_bad_oci("smaller bandwidth (20 Mhz) than negotiated",
                      121, 100, 0, "channel bandwidth mismatch")
    conn.test_bad_oci("wrong frequency, bandwith, and secondary channel",
                      123, 104, 0, "primary channel mismatch")
    conn.test_bad_oci("wrong upper/lower behaviour",
                      129, 104, 0, "primary channel mismatch")
    conn.confirm_valid_oci(122, 100, 0)

    dev[0].dump_monitor()
    if conn.hapd:
        conn.hapd.request("DISABLE")
    dev[0].disconnect_and_stop_scan()

def test_wpa2_ocv_sta_group_hs(dev, apdev):
    """OCV group handshake (STA)"""
    params = {"channel": "1",
              "ieee80211w": "1",
              "ocv": "1",
              "freq": "2412",
              "wpa_strict_rekey": "1"}
    conn = STAConnection(apdev[0], dev[0], params.copy())
    conn.confirm_valid_oci(81, 1, 0)

    # Send a EAPOL-Key msg 1/2 with a bad OCI
    logger.info("Bad OCI element")
    plain = conn.gtkie + make_ocikde(1, 1, 1)
    wrapped = aes_wrap(conn.kek, pad_key_data(plain))
    msg = build_eapol_key_1_2(conn.kck, wrapped, replay_counter=3)
    send_eapol(dev[0], conn.bssid, build_eapol(msg))

    # We shouldn't get a EAPOL-Key message back
    ev = dev[0].wait_event(["EAPOL-TX"], timeout=1)
    if ev is not None:
        raise Exception("Received response to invalid EAPOL-Key 1/2")

    # Reset AP to try with valid OCI
    conn.hapd.disable()
    conn = STAConnection(apdev[0], dev[0], params.copy())
    conn.confirm_valid_oci(81, 1, 0)

    # Send a EAPOL-Key msg 1/2 with a good OCI
    logger.info("Good OCI element")
    plain = conn.gtkie + make_ocikde(81, 1, 0)
    wrapped = aes_wrap(conn.kek, pad_key_data(plain))
    msg = build_eapol_key_1_2(conn.kck, wrapped, replay_counter=4)
    send_eapol(dev[0], conn.bssid, build_eapol(msg))

    # Wait for EAPOL-Key msg 2/2
    conn.msg = recv_eapol(dev[0])
    if conn.msg["rsn_key_info"] != 0x0302:
        raise Exception("Didn't receive 2/2 of group key handshake")

def test_wpa2_ocv_auto_enable_pmf(dev, apdev):
    """OCV on 2.4 GHz with PMF getting enabled automatically"""
    params = {"channel": "1",
              "ocv": "1"}
    hapd, ssid, passphrase = ocv_setup_ap(apdev[0], params)
    for ocv in range(2):
        dev[0].connect(ssid, psk=passphrase, scan_freq="2412", ocv=str(ocv),
                       ieee80211w="2")
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()

def test_wpa2_ocv_sta_override_eapol(dev, apdev):
    """OCV on 2.4 GHz and STA override EAPOL-Key msg 2/4"""
    params = {"channel": "1",
              "ieee80211w": "2",
              "ocv": "1"}
    hapd, ssid, passphrase = ocv_setup_ap(apdev[0], params)
    dev[0].set("oci_freq_override_eapol", "2462")
    dev[0].connect(ssid, psk=passphrase, scan_freq="2412", ocv="1",
                   ieee80211w="2", wait_connect=False)
    ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED",
                            "CTRL-EVENT-DISCONNECTED"], timeout=15)
    dev[0].request("DISCONNECT")
    if ev is None:
        raise Exception("No connection result reported")
    if "CTRL-EVENT-CONNECTED" in ev:
        raise Exception("Unexpected connection")
    if "reason=15" not in ev:
        raise Exception("Unexpected disconnection reason: " + ev)

    check_ocv_failure(hapd, "EAPOL-Key msg 2/4", "eapol-key-m2",
                      dev[0].own_addr())

def test_wpa2_ocv_sta_override_sa_query_req(dev, apdev):
    """OCV on 2.4 GHz and STA override SA Query Request"""
    params = {"channel": "1",
              "ieee80211w": "2",
              "ocv": "1"}
    hapd, ssid, passphrase = ocv_setup_ap(apdev[0], params)
    dev[0].connect(ssid, psk=passphrase, scan_freq="2412", ocv="1",
                   ieee80211w="2")
    hapd.wait_sta()
    dev[0].set("oci_freq_override_saquery_req", "2462")
    if "OK" not in dev[0].request("UNPROT_DEAUTH"):
        raise Exception("Triggering SA Query from the STA failed")
    ev = dev[0].wait_event(["CTRL-EVENT-DISCONNECTED"], timeout=3)
    if ev is None:
        raise Exception("Disconnection after failed SA Query not reported")
    dev[0].set("oci_freq_override_saquery_req", "0")
    dev[0].wait_connected()
    if "OK" not in dev[0].request("UNPROT_DEAUTH"):
        raise Exception("Triggering SA Query from the STA failed")
    check_ocv_failure(hapd, "SA Query Request", "saqueryreq",
                      dev[0].own_addr())
    ev = dev[0].wait_event(["CTRL-EVENT-DISCONNECTED"], timeout=3)
    if ev is not None:
        raise Exception("SA Query from the STA failed")

def test_wpa2_ocv_sta_override_sa_query_resp(dev, apdev):
    """OCV on 2.4 GHz and STA override SA Query Response"""
    params = {"channel": "1",
              "ieee80211w": "2",
              "ocv": "1"}
    hapd, ssid, passphrase = ocv_setup_ap(apdev[0], params)
    dev[0].connect(ssid, psk=passphrase, scan_freq="2412", ocv="1",
                   ieee80211w="2")
    dev[0].set("oci_freq_override_saquery_resp", "2462")
    hapd.wait_sta()
    if "OK" not in hapd.request("SA_QUERY " + dev[0].own_addr()):
        raise Exception("SA_QUERY failed")
    check_ocv_failure(hapd, "SA Query Response", "saqueryresp",
                      dev[0].own_addr())

def check_ocv_failure(dev, frame_txt, frame, addr):
    ev = dev.wait_event(["OCV-FAILURE"], timeout=3)
    if ev is None:
        raise Exception("OCV failure for %s not reported" % frame_txt)
    if "addr=" + addr not in ev:
        raise Exception("Unexpected OCV failure addr: " + ev)
    if "frame=" + frame not in ev:
        raise Exception("Unexpected OCV failure frame: " + ev)
    if "error=primary channel mismatch" not in ev:
        raise Exception("Unexpected OCV failure error: " + ev)

def test_wpa2_ocv_ap_override_eapol_m3(dev, apdev):
    """OCV on 2.4 GHz and AP override EAPOL-Key msg 3/4"""
    params = {"channel": "1",
              "ieee80211w": "2",
              "ocv": "1",
              "oci_freq_override_eapol_m3": "2462"}
    hapd, ssid, passphrase = ocv_setup_ap(apdev[0], params)
    bssid = hapd.own_addr()
    dev[0].connect(ssid, psk=passphrase, scan_freq="2412", ocv="1",
                   ieee80211w="2", wait_connect=False)

    check_ocv_failure(dev[0], "EAPOL-Key msg 3/4", "eapol-key-m3", bssid)

    ev = dev[0].wait_disconnected()
    if "reason=15" not in ev:
        raise Exception("Unexpected disconnection reason: " + ev)

def test_wpa2_ocv_ap_override_eapol_g1(dev, apdev):
    """OCV on 2.4 GHz and AP override EAPOL-Key group msg 1/2"""
    params = {"channel": "1",
              "ieee80211w": "2",
              "ocv": "1",
              "oci_freq_override_eapol_g1": "2462"}
    hapd, ssid, passphrase = ocv_setup_ap(apdev[0], params)
    bssid = hapd.own_addr()
    dev[0].connect(ssid, psk=passphrase, scan_freq="2412", ocv="1",
                   ieee80211w="2")

    if "OK" not in hapd.request("REKEY_GTK"):
        raise Exception("REKEY_GTK failed")
    check_ocv_failure(dev[0], "EAPOL-Key group msg 1/2", "eapol-key-g1", bssid)

def test_wpa2_ocv_ap_override_saquery_req(dev, apdev):
    """OCV on 2.4 GHz and AP override SA Query Request"""
    params = {"channel": "1",
              "ieee80211w": "2",
              "ocv": "1",
              "oci_freq_override_saquery_req": "2462"}
    hapd, ssid, passphrase = ocv_setup_ap(apdev[0], params)
    bssid = hapd.own_addr()
    dev[0].connect(ssid, psk=passphrase, scan_freq="2412", ocv="1",
                   ieee80211w="2")

    if "OK" not in hapd.request("SA_QUERY " + dev[0].own_addr()):
        raise Exception("SA_QUERY failed")
    check_ocv_failure(dev[0], "SA Query Request", "saqueryreq", bssid)

def test_wpa2_ocv_ap_override_saquery_resp(dev, apdev):
    """OCV on 2.4 GHz and AP override SA Query Response"""
    params = {"channel": "1",
              "ieee80211w": "2",
              "ocv": "1",
              "oci_freq_override_saquery_resp": "2462"}
    hapd, ssid, passphrase = ocv_setup_ap(apdev[0], params)
    bssid = hapd.own_addr()
    dev[0].connect(ssid, psk=passphrase, scan_freq="2412", ocv="1",
                   ieee80211w="2")

    if "OK" not in dev[0].request("UNPROT_DEAUTH"):
        raise Exception("Triggering SA Query from the STA failed")
    check_ocv_failure(dev[0], "SA Query Response", "saqueryresp", bssid)

def test_wpa2_ocv_ap_override_fils_assoc(dev, apdev, params):
    """OCV on 2.4 GHz and AP override FILS association"""
    check_fils_capa(dev[0])
    check_erp_capa(dev[0])

    start_erp_as(msk_dump=os.path.join(params['logdir'], "msk.lst"))

    bssid = apdev[0]['bssid']
    ssid = "test-wpa2-ocv"
    params = hostapd.wpa2_eap_params(ssid=ssid)
    params['wpa_key_mgmt'] = "FILS-SHA256"
    params['auth_server_port'] = "18128"
    params['erp_send_reauth_start'] = '1'
    params['erp_domain'] = 'example.com'
    params['fils_realm'] = 'example.com'
    params['wpa_group_rekey'] = '1'
    params["ieee80211w"] = "2"
    params["ocv"] = "1"
    params["oci_freq_override_fils_assoc"] = "2462"
    try:
        hapd = hostapd.add_ap(apdev[0], params)
    except Exception as e:
        if "Failed to set hostapd parameter ocv" in str(e):
            raise HwsimSkip("OCV not supported")
        raise
    bssid = hapd.own_addr()
    dev[0].request("ERP_FLUSH")
    id = dev[0].connect(ssid, key_mgmt="FILS-SHA256",
                        eap="PSK", identity="psk.user@example.com",
                        password_hex="0123456789abcdef0123456789abcdef",
                        erp="1", scan_freq="2412", ocv="1", ieee80211w="2")

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    dev[0].dump_monitor()
    dev[0].select_network(id, freq=2412)

    check_ocv_failure(dev[0], "FILS Association Response", "fils-assoc", bssid)
    dev[0].request("DISCONNECT")

def test_wpa2_ocv_ap_override_ft_assoc(dev, apdev):
    """OCV on 2.4 GHz and AP override FT reassociation"""
    ssid = "test-wpa2-ocv"
    passphrase = "qwertyuiop"
    params = ft_params1(ssid=ssid, passphrase=passphrase)
    params["ieee80211w"] = "2"
    params["ocv"] = "1"
    params["oci_freq_override_fils_assoc"] = "2462"
    try:
        hapd0 = hostapd.add_ap(apdev[0], params)
    except Exception as e:
        if "Failed to set hostapd parameter ocv" in str(e):
            raise HwsimSkip("OCV not supported")
        raise
    params = ft_params2(ssid=ssid, passphrase=passphrase)
    params["ieee80211w"] = "2"
    params["ocv"] = "1"
    params["oci_freq_override_ft_assoc"] = "2462"
    hapd1 = hostapd.add_ap(apdev[1], params)

    dev[0].connect(ssid, key_mgmt="FT-PSK", psk=passphrase,
                   scan_freq="2412", ocv="1", ieee80211w="2")

    bssid = dev[0].get_status_field("bssid")
    bssid0 = hapd0.own_addr()
    bssid1 = hapd1.own_addr()
    target = bssid0 if bssid == bssid1 else bssid1

    dev[0].scan_for_bss(target, freq="2412")
    if "OK" not in dev[0].request("ROAM " + target):
        raise Exception("ROAM failed")

    check_ocv_failure(dev[0], "FT Reassociation Response", "ft-assoc", target)
    dev[0].request("DISCONNECT")

@remote_compatible
def test_wpa2_ocv_no_pmf(dev, apdev):
    """OCV on 2.4 GHz and no PMF on STA"""
    params = {"channel": "1",
              "ieee80211w": "1",
              "ocv": "1"}
    hapd, ssid, passphrase = ocv_setup_ap(apdev[0], params)
    ie = "301a0100000fac040100000fac040100000fac0200400000000fac06"
    if "OK" not in dev[0].request("TEST_ASSOC_IE " + ie):
        raise Exception("Could not set TEST_ASSOC_IE")
    dev[0].connect(ssid, psk=passphrase, scan_freq="2412", ocv="0",
                   ieee80211w="0", wait_connect=False)
    ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED", "CTRL-EVENT-ASSOC-REJECT"],
                           timeout=10)
    dev[0].request("DISCONNECT")
    if ev is None:
        raise Exception("No connection result seen")
    if "CTRL-EVENT-CONNECTED" in ev:
        raise Exception("Unexpected connection")
    if "status_code=31" not in ev:
        raise Exception("Unexpected status code: " + ev)

@remote_compatible
def test_wpa2_ocv_no_pmf_workaround(dev, apdev):
    """OCV on 2.4 GHz and no PMF on STA with workaround"""
    params = {"channel": "1",
              "ieee80211w": "1",
              "ocv": "2"}
    hapd, ssid, passphrase = ocv_setup_ap(apdev[0], params)
    ie = "301a0100000fac040100000fac040100000fac0200400000000fac06"
    if "OK" not in dev[0].request("TEST_ASSOC_IE " + ie):
        raise Exception("Could not set TEST_ASSOC_IE")
    dev[0].connect(ssid, psk=passphrase, scan_freq="2412", ocv="0",
                   ieee80211w="0")

@remote_compatible
def test_wpa2_ocv_no_oci(dev, apdev):
    """OCV on 2.4 GHz and no OCI from STA"""
    params = {"channel": "1",
              "ieee80211w": "1",
              "ocv": "1"}
    hapd, ssid, passphrase = ocv_setup_ap(apdev[0], params)
    ie = "301a0100000fac040100000fac040100000fac0280400000000fac06"
    if "OK" not in dev[0].request("TEST_ASSOC_IE " + ie):
        raise Exception("Could not set TEST_ASSOC_IE")
    dev[0].connect(ssid, psk=passphrase, scan_freq="2412", ocv="0",
                   ieee80211w="1", wait_connect=False)
    ev = hapd.wait_event(["OCV-FAILURE"], timeout=10)
    if ev is None:
        raise Exception("No OCV failure reported")
    if "frame=eapol-key-m2 error=did not receive mandatory OCI" not in ev:
        raise Exception("Unexpected error: " + ev)
    ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED",
                            "WPA: 4-Way Handshake failed"], timeout=10)
    dev[0].request("DISCONNECT")
    if "CTRL-EVENT-CONNECTED" in ev:
        raise Exception("Unexpected connection")
    if ev is None:
        raise Exception("4-way handshake failure not reported")

@remote_compatible
def test_wpa2_ocv_no_oci_workaround(dev, apdev):
    """OCV on 2.4 GHz and no OCI from STA with workaround"""
    params = {"channel": "1",
              "ieee80211w": "1",
              "ocv": "2"}
    hapd, ssid, passphrase = ocv_setup_ap(apdev[0], params)
    ie = "301a0100000fac040100000fac040100000fac0280400000000fac06"
    if "OK" not in dev[0].request("TEST_ASSOC_IE " + ie):
        raise Exception("Could not set TEST_ASSOC_IE")
    dev[0].connect(ssid, psk=passphrase, scan_freq="2412", ocv="0",
                   ieee80211w="1")

def test_wpa2_ocv_without_pmf(dev, apdev):
    """OCV without PMF"""
    params = {"channel": "6",
              "ieee80211n": "1",
              "ieee80211w": "1",
              "ocv": "1"}
    hapd, ssid, passphrase = ocv_setup_ap(apdev[0], params)
    hapd.disable()
    hapd.set("ieee80211w", "0")
    if "FAIL" not in hapd.request("ENABLE"):
        raise Exception("OCV without PMF accepted")
