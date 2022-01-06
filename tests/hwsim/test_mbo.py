# MBO tests
# Copyright (c) 2016, Intel Deutschland GmbH
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

from remotehost import remote_compatible
import logging
logger = logging.getLogger()

import hostapd
import os
import time

import hostapd
from tshark import run_tshark
from utils import *

def set_reg(country_code, apdev0=None, apdev1=None, dev0=None):
    if apdev0:
        hostapd.cmd_execute(apdev0, ['iw', 'reg', 'set', country_code])
    if apdev1:
        hostapd.cmd_execute(apdev1, ['iw', 'reg', 'set', country_code])
    if dev0:
        dev0.cmd_execute(['iw', 'reg', 'set', country_code])

def run_mbo_supp_oper_classes(dev, apdev, hapd, hapd2, country, freq_list=None,
                              disable_ht=False, disable_vht=False):
    """MBO and supported operating classes"""
    addr = dev[0].own_addr()

    res2 = None
    res5 = None

    dev[0].flush_scan_cache()
    dev[0].dump_monitor()

    logger.info("Country: " + country)
    dev[0].note("Setting country code " + country)
    set_reg(country, apdev[0], apdev[1], dev[0])
    for j in range(5):
        ev = dev[0].wait_event(["CTRL-EVENT-REGDOM-CHANGE"], timeout=5)
        if ev is None:
            raise Exception("No regdom change event")
        if "alpha2=" + country in ev:
            break
    dev[0].dump_monitor()
    dev[1].dump_monitor()
    dev[2].dump_monitor()
    _disable_ht = "1" if disable_ht else "0"
    _disable_vht = "1" if disable_vht else "0"
    if hapd:
        hapd.set("country_code", country)
        hapd.enable()
        dev[0].scan_for_bss(hapd.own_addr(), 5180, force_scan=True)
        dev[0].connect("test-wnm-mbo", key_mgmt="NONE", scan_freq="5180",
                       freq_list=freq_list, disable_ht=_disable_ht,
                       disable_vht=_disable_vht)
        sta = hapd.get_sta(addr)
        res5 = sta['supp_op_classes'][2:]
        dev[0].wait_regdom(country_ie=True)
        time.sleep(0.1)
        hapd.disable()
        time.sleep(0.1)
        dev[0].request("REMOVE_NETWORK all")
        dev[0].request("ABORT_SCAN")
        dev[0].wait_disconnected()
        dev[0].dump_monitor()

    hapd2.set("country_code", country)
    hapd2.enable()
    dev[0].scan_for_bss(hapd2.own_addr(), 2412, force_scan=True)
    dev[0].connect("test-wnm-mbo-2", key_mgmt="NONE", scan_freq="2412",
                   freq_list=freq_list, disable_ht=_disable_ht,
                   disable_vht=_disable_vht)
    sta = hapd2.get_sta(addr)
    res2 = sta['supp_op_classes'][2:]
    dev[0].wait_regdom(country_ie=True)
    time.sleep(0.1)
    hapd2.disable()
    time.sleep(0.1)
    dev[0].request("REMOVE_NETWORK all")
    dev[0].request("ABORT_SCAN")
    dev[0].wait_disconnected()
    dev[0].dump_monitor()

    return res2, res5

def run_mbo_supp_oper_class(dev, apdev, country, expected, inc5,
                            freq_list=None, disable_ht=False,
                            disable_vht=False):
    if inc5:
        params = {'ssid': "test-wnm-mbo",
                  'mbo': '1',
                  "country_code": "US",
                  'ieee80211d': '1',
                  "ieee80211n": "1",
                  "hw_mode": "a",
                  "channel": "36"}
        hapd = hostapd.add_ap(apdev[0], params, no_enable=True)
    else:
        hapd = None

    params = {'ssid': "test-wnm-mbo-2",
              'mbo': '1',
              "country_code": "US",
              'ieee80211d': '1',
              "ieee80211n": "1",
              "hw_mode": "g",
              "channel": "1"}
    hapd2 = hostapd.add_ap(apdev[1], params, no_enable=True)

    try:
        dev[0].request("STA_AUTOCONNECT 0")
        res2, res5 = run_mbo_supp_oper_classes(dev, apdev, hapd, hapd2, country,
                                               freq_list=freq_list,
                                               disable_ht=disable_ht,
                                               disable_vht=disable_vht)
    finally:
        dev[0].dump_monitor()
        dev[0].request("STA_AUTOCONNECT 1")
        wait_regdom_changes(dev[0])
        country1 = dev[0].get_driver_status_field("country")
        logger.info("Country code at the end (1): " + country1)
        set_reg("00", apdev[0], apdev[1], dev[0])
        country2 = dev[0].get_driver_status_field("country")
        logger.info("Country code at the end (2): " + country2)
        for i in range(5):
            ev = dev[0].wait_event(["CTRL-EVENT-REGDOM-CHANGE"], timeout=1)
            if ev is None or "init=USER type=WORLD" in ev:
                break
        wait_regdom_changes(dev[0])
        country3 = dev[0].get_driver_status_field("country")
        logger.info("Country code at the end (3): " + country3)
        if country3 != "00":
            clear_country(dev)

    # For now, allow operating class 129 to be missing since not all
    # installed regdb files include the 160 MHz channels.
    expected2 = expected.replace('808182', '8082')
    # For now, allow operating classes 121-123 to be missing since not all
    # installed regdb files include the related US DFS channels.
    expected2 = expected2.replace('78797a7b7c', '787c')
    expected3 = expected
    # For now, allow operating classes 124-127 to be missing for Finland
    # since they were added only recently in regdb.
    if country == "FI":
        expected3 = expected3.replace("7b7c7d7e7f80", "7b80")
    if res2 != expected and res2 != expected2 and res2 != expected3:
        raise Exception("Unexpected supp_op_class string (country=%s, 2.4 GHz): %s (expected: %s)" % (country, res2, expected))
    if inc5 and res5 != expected and res5 != expected2 and res5 != expected3:
        raise Exception("Unexpected supp_op_class string (country=%s, 5 GHz): %s (expected: %s)" % (country, res5, expected))

def test_mbo_supp_oper_classes_za(dev, apdev):
    """MBO and supported operating classes (ZA)"""
    run_mbo_supp_oper_class(dev, apdev, "ZA",
                            "515354737475767778797a7b808182", True)

def test_mbo_supp_oper_classes_fi(dev, apdev):
    """MBO and supported operating classes (FI)"""
    run_mbo_supp_oper_class(dev, apdev, "FI",
                            "515354737475767778797a7b7c7d7e7f808182", True)

def test_mbo_supp_oper_classes_us(dev, apdev):
    """MBO and supported operating classes (US)"""
    run_mbo_supp_oper_class(dev, apdev, "US",
                            "515354737475767778797a7b7c7d7e7f808182", True)

def test_mbo_supp_oper_classes_jp(dev, apdev):
    """MBO and supported operating classes (JP)"""
    run_mbo_supp_oper_class(dev, apdev, "JP",
                            "51525354737475767778797a7b808182", True)

def test_mbo_supp_oper_classes_bd(dev, apdev):
    """MBO and supported operating classes (BD)"""
    run_mbo_supp_oper_class(dev, apdev, "BD",
                            "5153547c7d7e7f80", False)

def test_mbo_supp_oper_classes_sy(dev, apdev):
    """MBO and supported operating classes (SY)"""
    run_mbo_supp_oper_class(dev, apdev, "SY",
                            "515354", False)

def test_mbo_supp_oper_classes_us_freq_list(dev, apdev):
    """MBO and supported operating classes (US) - freq_list"""
    run_mbo_supp_oper_class(dev, apdev, "US", "515354", False,
                            freq_list="2412 2437 2462")

def test_mbo_supp_oper_classes_us_disable_ht(dev, apdev):
    """MBO and supported operating classes (US) - disable_ht"""
    run_mbo_supp_oper_class(dev, apdev, "US", "517376797c7d", False,
                            disable_ht=True)

def test_mbo_supp_oper_classes_us_disable_vht(dev, apdev):
    """MBO and supported operating classes (US) - disable_vht"""
    run_mbo_supp_oper_class(dev, apdev, "US",
                            "515354737475767778797a7b7c7d7e7f", False,
                            disable_vht=True)

def test_mbo_assoc_disallow(dev, apdev, params):
    """MBO and association disallowed"""
    hapd1 = hostapd.add_ap(apdev[0], {"ssid": "MBO", "mbo": "1"})
    hapd2 = hostapd.add_ap(apdev[1], {"ssid": "MBO", "mbo": "1"})

    logger.debug("Set mbo_assoc_disallow with invalid value")
    if "FAIL" not in hapd1.request("SET mbo_assoc_disallow 6"):
        raise Exception("Set mbo_assoc_disallow for AP1 succeeded unexpectedly with value 6")

    logger.debug("Disallow associations to AP1 and allow association to AP2")
    if "OK" not in hapd1.request("SET mbo_assoc_disallow 1"):
        raise Exception("Failed to set mbo_assoc_disallow for AP1")
    if "OK" not in hapd2.request("SET mbo_assoc_disallow 0"):
        raise Exception("Failed to set mbo_assoc_disallow for AP2")

    dev[0].connect("MBO", key_mgmt="NONE", scan_freq="2412")

    out = run_tshark(os.path.join(params['logdir'], "hwsim0.pcapng"),
                     "wlan.fc.type == 0 && wlan.fc.type_subtype == 0x00",
                     wait=False)
    if "Destination address: " + hapd1.own_addr() in out:
        raise Exception("Association request sent to disallowed AP")

    timestamp = run_tshark(os.path.join(params['logdir'], "hwsim0.pcapng"),
                           "wlan.fc.type_subtype == 0x00",
                           display=['frame.time'], wait=False)

    logger.debug("Allow associations to AP1 and disallow associations to AP2")
    if "OK" not in hapd1.request("SET mbo_assoc_disallow 0"):
        raise Exception("Failed to set mbo_assoc_disallow for AP1")
    if "OK" not in hapd2.request("SET mbo_assoc_disallow 1"):
        raise Exception("Failed to set mbo_assoc_disallow for AP2")

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    # Force new scan, so the assoc_disallowed indication is updated */
    dev[0].request("FLUSH")

    dev[0].connect("MBO", key_mgmt="NONE", scan_freq="2412")

    filter = 'wlan.fc.type == 0 && wlan.fc.type_subtype == 0x00 && frame.time > "' + timestamp.rstrip() + '"'
    out = run_tshark(os.path.join(params['logdir'], "hwsim0.pcapng"),
                     filter, wait=False)
    if "Destination address: " + hapd2.own_addr() in out:
        raise Exception("Association request sent to disallowed AP 2")

def test_mbo_assoc_disallow_ignore(dev, apdev):
    """MBO and ignoring disallowed association"""
    try:
        _test_mbo_assoc_disallow_ignore(dev, apdev)
    finally:
        dev[0].request("SCAN_INTERVAL 5")

def _test_mbo_assoc_disallow_ignore(dev, apdev):
    hapd1 = hostapd.add_ap(apdev[0], {"ssid": "MBO", "mbo": "1"})
    if "OK" not in hapd1.request("SET mbo_assoc_disallow 1"):
        raise Exception("Failed to set mbo_assoc_disallow for AP1")

    if "OK" not in dev[0].request("SCAN_INTERVAL 1"):
        raise Exception("Failed to set scan interval")
    dev[0].connect("MBO", key_mgmt="NONE", scan_freq="2412", wait_connect=False)
    ev = dev[0].wait_event(["CTRL-EVENT-NETWORK-NOT-FOUND"], timeout=10)
    if ev is None:
        raise Exception("CTRL-EVENT-NETWORK-NOT-FOUND not seen")

    if "OK" not in dev[0].request("SET ignore_assoc_disallow 1"):
        raise Exception("Failed to set ignore_assoc_disallow")
    ev = dev[0].wait_event(["CTRL-EVENT-ASSOC-REJECT"], timeout=10)
    if ev is None:
        raise Exception("CTRL-EVENT-ASSOC-REJECT not seen")
    if "status_code=17" not in ev:
        raise Exception("Unexpected association reject reason: " + ev)

    if "OK" not in hapd1.request("SET mbo_assoc_disallow 0"):
        raise Exception("Failed to set mbo_assoc_disallow for AP1")
    dev[0].wait_connected()

@remote_compatible
def test_mbo_cell_capa_update(dev, apdev):
    """MBO cellular data capability update"""
    ssid = "test-wnm-mbo"
    params = {'ssid': ssid, 'mbo': '1'}
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = apdev[0]['bssid']
    if "OK" not in dev[0].request("SET mbo_cell_capa 1"):
        raise Exception("Failed to set STA as cellular data capable")

    dev[0].connect(ssid, key_mgmt="NONE", scan_freq="2412")

    addr = dev[0].own_addr()
    sta = hapd.get_sta(addr)
    if 'mbo_cell_capa' not in sta or sta['mbo_cell_capa'] != '1':
        raise Exception("mbo_cell_capa missing after association")

    if "OK" not in dev[0].request("SET mbo_cell_capa 3"):
        raise Exception("Failed to set STA as cellular data not-capable")
    # Duplicate update for additional code coverage
    if "OK" not in dev[0].request("SET mbo_cell_capa 3"):
        raise Exception("Failed to set STA as cellular data not-capable")

    time.sleep(0.2)
    sta = hapd.get_sta(addr)
    if 'mbo_cell_capa' not in sta:
        raise Exception("mbo_cell_capa missing after update")
    if sta['mbo_cell_capa'] != '3':
        raise Exception("mbo_cell_capa not updated properly")

@remote_compatible
def test_mbo_cell_capa_update_pmf(dev, apdev):
    """MBO cellular data capability update with PMF required"""
    ssid = "test-wnm-mbo"
    passphrase = "12345678"
    params = hostapd.wpa2_params(ssid=ssid, passphrase=passphrase)
    params["wpa_key_mgmt"] = "WPA-PSK-SHA256"
    params["ieee80211w"] = "2"
    params['mbo'] = '1'
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = apdev[0]['bssid']
    if "OK" not in dev[0].request("SET mbo_cell_capa 1"):
        raise Exception("Failed to set STA as cellular data capable")

    dev[0].connect(ssid, psk=passphrase, key_mgmt="WPA-PSK-SHA256",
                   proto="WPA2", ieee80211w="2", scan_freq="2412")
    hapd.wait_sta()

    addr = dev[0].own_addr()
    sta = hapd.get_sta(addr)
    if 'mbo_cell_capa' not in sta or sta['mbo_cell_capa'] != '1':
        raise Exception("mbo_cell_capa missing after association")

    if "OK" not in dev[0].request("SET mbo_cell_capa 3"):
        raise Exception("Failed to set STA as cellular data not-capable")

    time.sleep(0.2)
    sta = hapd.get_sta(addr)
    if 'mbo_cell_capa' not in sta:
        raise Exception("mbo_cell_capa missing after update")
    if sta['mbo_cell_capa'] != '3':
        raise Exception("mbo_cell_capa not updated properly")

def test_mbo_wnm_token_wrap(dev, apdev):
    """MBO WNM token wrap around"""
    ssid = "test-wnm-mbo"
    params = {'ssid': ssid, 'mbo': '1'}
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = apdev[0]['bssid']

    dev[0].connect(ssid, key_mgmt="NONE", scan_freq="2412")

    # Trigger transmission of 256 WNM-Notification frames to wrap around the
    # 8-bit mbo_wnm_token counter.
    for i in range(128):
        if "OK" not in dev[0].request("SET mbo_cell_capa 1"):
            raise Exception("Failed to set STA as cellular data capable")
        if "OK" not in dev[0].request("SET mbo_cell_capa 3"):
            raise Exception("Failed to set STA as cellular data not-capable")

@remote_compatible
def test_mbo_non_pref_chan(dev, apdev):
    """MBO non-preferred channel list"""
    ssid = "test-wnm-mbo"
    params = {'ssid': ssid, 'mbo': '1'}
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = apdev[0]['bssid']
    if "FAIL" not in dev[0].request("SET non_pref_chan 81:7:200:99"):
        raise Exception("Invalid non_pref_chan value accepted")
    if "FAIL" not in dev[0].request("SET non_pref_chan 81:15:200:3"):
        raise Exception("Invalid non_pref_chan value accepted")
    if "FAIL" not in dev[0].request("SET non_pref_chan 81:7:200:3 81:7:201:3"):
        raise Exception("Invalid non_pref_chan value accepted")
    if "OK" not in dev[0].request("SET non_pref_chan 81:7:200:3"):
        raise Exception("Failed to set non-preferred channel list")
    if "OK" not in dev[0].request("SET non_pref_chan 81:7:200:1 81:9:100:2"):
        raise Exception("Failed to set non-preferred channel list")

    dev[0].connect(ssid, key_mgmt="NONE", scan_freq="2412")

    addr = dev[0].own_addr()
    sta = hapd.get_sta(addr)
    logger.debug("STA: " + str(sta))
    if 'non_pref_chan[0]' not in sta:
        raise Exception("Missing non_pref_chan[0] value (assoc)")
    if sta['non_pref_chan[0]'] != '81:200:1:7':
        raise Exception("Unexpected non_pref_chan[0] value (assoc)")
    if 'non_pref_chan[1]' not in sta:
        raise Exception("Missing non_pref_chan[1] value (assoc)")
    if sta['non_pref_chan[1]'] != '81:100:2:9':
        raise Exception("Unexpected non_pref_chan[1] value (assoc)")
    if 'non_pref_chan[2]' in sta:
        raise Exception("Unexpected non_pref_chan[2] value (assoc)")

    if "OK" not in dev[0].request("SET non_pref_chan 81:9:100:2"):
        raise Exception("Failed to update non-preferred channel list")
    time.sleep(0.1)
    sta = hapd.get_sta(addr)
    logger.debug("STA: " + str(sta))
    if 'non_pref_chan[0]' not in sta:
        raise Exception("Missing non_pref_chan[0] value (update 1)")
    if sta['non_pref_chan[0]'] != '81:100:2:9':
        raise Exception("Unexpected non_pref_chan[0] value (update 1)")
    if 'non_pref_chan[1]' in sta:
        raise Exception("Unexpected non_pref_chan[1] value (update 1)")

    if "OK" not in dev[0].request("SET non_pref_chan 81:9:100:2 81:10:100:2 81:8:100:2 81:7:100:1 81:5:100:1"):
        raise Exception("Failed to update non-preferred channel list")
    time.sleep(0.1)
    sta = hapd.get_sta(addr)
    logger.debug("STA: " + str(sta))
    if 'non_pref_chan[0]' not in sta:
        raise Exception("Missing non_pref_chan[0] value (update 2)")
    if sta['non_pref_chan[0]'] != '81:100:1:7,5':
        raise Exception("Unexpected non_pref_chan[0] value (update 2)")
    if 'non_pref_chan[1]' not in sta:
        raise Exception("Missing non_pref_chan[1] value (update 2)")
    if sta['non_pref_chan[1]'] != '81:100:2:9,10,8':
        raise Exception("Unexpected non_pref_chan[1] value (update 2)")
    if 'non_pref_chan[2]' in sta:
        raise Exception("Unexpected non_pref_chan[2] value (update 2)")

    if "OK" not in dev[0].request("SET non_pref_chan 81:5:90:2 82:14:91:2"):
        raise Exception("Failed to update non-preferred channel list")
    time.sleep(0.1)
    sta = hapd.get_sta(addr)
    logger.debug("STA: " + str(sta))
    if 'non_pref_chan[0]' not in sta:
        raise Exception("Missing non_pref_chan[0] value (update 3)")
    if sta['non_pref_chan[0]'] != '81:90:2:5':
        raise Exception("Unexpected non_pref_chan[0] value (update 3)")
    if 'non_pref_chan[1]' not in sta:
        raise Exception("Missing non_pref_chan[1] value (update 3)")
    if sta['non_pref_chan[1]'] != '82:91:2:14':
        raise Exception("Unexpected non_pref_chan[1] value (update 3)")
    if 'non_pref_chan[2]' in sta:
        raise Exception("Unexpected non_pref_chan[2] value (update 3)")

    if "OK" not in dev[0].request("SET non_pref_chan "):
        raise Exception("Failed to update non-preferred channel list")
    time.sleep(0.1)
    sta = hapd.get_sta(addr)
    logger.debug("STA: " + str(sta))
    if 'non_pref_chan[0]' in sta:
        raise Exception("Unexpected non_pref_chan[0] value (update 4)")

@remote_compatible
def test_mbo_sta_supp_op_classes(dev, apdev):
    """MBO STA supported operating classes"""
    ssid = "test-wnm-mbo"
    params = {'ssid': ssid, 'mbo': '1'}
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].connect(ssid, key_mgmt="NONE", scan_freq="2412")

    addr = dev[0].own_addr()
    sta = hapd.get_sta(addr)
    logger.debug("STA: " + str(sta))
    if 'supp_op_classes' not in sta:
        raise Exception("No supp_op_classes")
    supp = bytearray(binascii.unhexlify(sta['supp_op_classes']))
    if supp[0] != 81:
        raise Exception("Unexpected current operating class %d" % supp[0])
    if 115 not in supp:
        raise Exception("Operating class 115 missing")

def test_mbo_failures(dev, apdev):
    """MBO failure cases"""
    ssid = "test-wnm-mbo"
    params = {'ssid': ssid, 'mbo': '1'}
    hapd = hostapd.add_ap(apdev[0], params)

    with alloc_fail(dev[0], 1, "wpas_mbo_ie"):
        dev[0].connect(ssid, key_mgmt="NONE", scan_freq="2412")

    with alloc_fail(dev[0], 1, "wpas_mbo_send_wnm_notification"):
        if "OK" not in dev[0].request("SET mbo_cell_capa 1"):
            raise Exception("Failed to set STA as cellular data capable")
    with fail_test(dev[0], 1, "wpas_mbo_send_wnm_notification"):
        if "OK" not in dev[0].request("SET mbo_cell_capa 3"):
            raise Exception("Failed to set STA as cellular data not-capable")
    with alloc_fail(dev[0], 1, "wpas_mbo_update_non_pref_chan"):
        if "FAIL" not in dev[0].request("SET non_pref_chan 81:7:200:3"):
            raise Exception("non_pref_chan value accepted during OOM")
    with alloc_fail(dev[0], 2, "wpas_mbo_update_non_pref_chan"):
        if "FAIL" not in dev[0].request("SET non_pref_chan 81:7:200:3"):
            raise Exception("non_pref_chan value accepted during OOM")

def test_mbo_wnm_bss_tm_ie_parsing(dev, apdev):
    """MBO BSS transition request MBO IE parsing"""
    ssid = "test-wnm-mbo"
    params = hostapd.wpa2_params(ssid=ssid, passphrase="12345678")
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = apdev[0]['bssid']
    addr = dev[0].own_addr()
    dev[0].connect(ssid, psk="12345678", key_mgmt="WPA-PSK",
                   proto="WPA2", ieee80211w="0", scan_freq="2412")

    dev[0].request("SET ext_mgmt_frame_handling 1")
    hdr = "d0003a01" + addr.replace(':', '') + bssid.replace(':', '') + bssid.replace(':', '') + "3000"
    btm_hdr = "0a070100030001"

    tests = [("Truncated attribute in MBO IE", "dd06506f9a160101"),
             ("Unexpected cell data capa attribute length in MBO IE",
              "dd09506f9a160501030500"),
             ("Unexpected transition reason attribute length in MBO IE",
              "dd06506f9a160600"),
             ("Unexpected assoc retry delay attribute length in MBO IE",
              "dd0c506f9a160100080200000800"),
             ("Unknown attribute id 255 in MBO IE",
              "dd06506f9a16ff00")]

    for test, mbo_ie in tests:
        logger.info(test)
        dev[0].request("NOTE " + test)
        frame = hdr + btm_hdr + mbo_ie
        if "OK" not in dev[0].request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=" + frame):
            raise Exception("MGMT_RX_PROCESS failed")

    logger.info("Unexpected association retry delay")
    dev[0].request("NOTE Unexpected association retry delay")
    btm_hdr = "0a070108030001112233445566778899aabbcc"
    mbo_ie = "dd08506f9a1608020000"
    frame = hdr + btm_hdr + mbo_ie
    if "OK" not in dev[0].request("MGMT_RX_PROCESS freq=2412 datarate=0 ssi_signal=-30 frame=" + frame):
        raise Exception("MGMT_RX_PROCESS failed")

    dev[0].request("SET ext_mgmt_frame_handling 0")

def test_mbo_without_pmf(dev, apdev):
    """MBO and WPA2 without PMF"""
    ssid = "test-wnm-mbo"
    params = {'ssid': ssid, 'mbo': '1', "wpa": '2',
              "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP",
              "wpa_passphrase": "12345678"}
    try:
        # "MBO: PMF needs to be enabled whenever using WPA2 with MBO"
        hostapd.add_ap(apdev[0], params)
        raise Exception("AP setup succeeded unexpectedly")
    except Exception as e:
        if "Failed to enable hostapd" in str(e):
            pass
        else:
            raise

def test_mbo_without_pmf_workaround(dev, apdev):
    """MBO and WPA2 without PMF on misbehaving AP"""
    ssid = "test-wnm-mbo"
    params0 = {'ssid': ssid, "wpa": '2',
               "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP",
               "wpa_passphrase": "12345678",
               "vendor_elements": "dd07506f9a16010100"}
    params1 = {'ssid': ssid, "mbo": '1', "wpa": '2',
               "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP",
               "wpa_passphrase": "12345678", "ieee80211w": "1"}
    hapd0 = hostapd.add_ap(apdev[0], params0)
    dev[0].connect(ssid, psk="12345678", key_mgmt="WPA-PSK",
                   proto="WPA2", ieee80211w="1", scan_freq="2412")
    hapd0.wait_sta()
    sta = hapd0.get_sta(dev[0].own_addr())
    ext_capab = bytearray(binascii.unhexlify(sta['ext_capab']))
    if ext_capab[2] & 0x08:
        raise Exception("STA did not disable BSS Transition capability")
    hapd1 = hostapd.add_ap(apdev[1], params1)
    dev[0].scan_for_bss(hapd1.own_addr(), 2412, force_scan=True)
    dev[0].roam(hapd1.own_addr())
    hapd1.wait_sta()
    sta = hapd1.get_sta(dev[0].own_addr())
    ext_capab = bytearray(binascii.unhexlify(sta['ext_capab']))
    if not ext_capab[2] & 0x08:
        raise Exception("STA disabled BSS Transition capability")
    dev[0].roam(hapd0.own_addr())
    hapd0.wait_sta()
    sta = hapd0.get_sta(dev[0].own_addr())
    ext_capab = bytearray(binascii.unhexlify(sta['ext_capab']))
    if ext_capab[2] & 0x08:
        raise Exception("STA did not disable BSS Transition capability")

def check_mbo_anqp(dev, bssid, cell_data_conn_pref):
    if "OK" not in dev.request("ANQP_GET " + bssid + " 272,mbo:2"):
        raise Exception("ANQP_GET command failed")

    ev = dev.wait_event(["GAS-QUERY-START"], timeout=5)
    if ev is None:
        raise Exception("GAS query start timed out")

    ev = dev.wait_event(["GAS-QUERY-DONE"], timeout=10)
    if ev is None:
        raise Exception("GAS query timed out")

    if cell_data_conn_pref is not None:
        ev = dev.wait_event(["RX-MBO-ANQP"], timeout=1)
        if ev is None or "cell_conn_pref" not in ev:
            raise Exception("Did not receive MBO Cellular Data Connection Preference")
        if cell_data_conn_pref != int(ev.split('=')[1]):
            raise Exception("Unexpected cell_conn_pref value: " + ev)

    dev.dump_monitor()

def test_mbo_anqp(dev, apdev):
    """MBO ANQP"""
    params = {'ssid': "test-wnm-mbo",
              'mbo': '1',
              'interworking': '1',
              'mbo_cell_data_conn_pref': '1'}
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = hapd.own_addr()

    dev[0].scan_for_bss(bssid, freq="2412", force_scan=True)
    check_mbo_anqp(dev[0], bssid, 1)

    hapd.set('mbo_cell_data_conn_pref', '255')
    check_mbo_anqp(dev[0], bssid, 255)

    hapd.set('mbo_cell_data_conn_pref', '-1')
    check_mbo_anqp(dev[0], bssid, None)
