# Test cases for SAE-PK
# Copyright (c) 2020, The Linux Foundation
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

import hostapd
from utils import *

SAE_PK_SSID = "SAE-PK test"

SAE_PK_SEC3_PW = "r6cr-6ksa-56og"
SAE_PK_SEC3_M = "089ec11475d55f0d38403f5117a6d64d"
SAE_PK_19_PK = "MHcCAQEEIAJIGlfnteonDb7rQyP/SGQjwzrZAnfrXIm4280VWajYoAoGCCqGSM49AwEHoUQDQgAEeRkstKQV+FSAMqBayqFknn2nAQsdsh/MhdX6tiHOTAFin/sUMFRMyspPtIu7YvlKdsexhI0jPVhaYZn1jKWhZg=="

SAE_PK_20_PW = "4zsy-uspe-xbfr-3ifo"
SAE_PK_20_M = "206902f9f09b62e3fafcd487c65f5c64"
SAE_PK_20_PK = "MIGkAgEBBDA4wpA6w/fK0g3a2V6QmcoxNoFCVuQPyzWvKYimJkgXsVsXt2ERXQ7dGOVXeycM5DqgBwYFK4EEACKhZANiAARTdszGBNe2PGCnc8Wvs+IDvdVEf4PPBrty0meRZf6UTbGouquTHpy6KKTq5sxrulYzsQFimg4op0UJBGxAzqo0EtTgMlLiBvY0I3Nl3N69MhWo8nvnmguvGGN32AAPXpQ="

SAE_PK_21_PW = "vluk-umpa-3mbw-zrhe-s2n2"
SAE_PK_21_M = "1c63c1b17e9a999f0693b4341a970a63"
SAE_PK_21_PK = "MIHcAgEBBEIBnFBjU0ywxo1dLTYcg2aZdMfNY7JHt4GTADRTgJ7RRo9qzRIlfmK7p+BP1c8YM8ia8v7YDTut00rDOfzkdmLOi0WgBwYFK4EEACOhgYkDgYYABAD6n3DHI+qaj/lElhe2sUSKqAe4sweckMlr9bhdmwp8Wsx5lKR/Tt7WPexeqFrA47nChw5WMWy6qJanCKNFvGYG0ADUWnxesYczGtCdUYJQgs3X5tHSapMssz6tP8QL0X9adTI/H3tFYhiVIdor03eZDUVnej78/F31CcHcjGBEyItVfw=="

def run_sae_pk(apdev, dev, ssid, pw, m, pk, ap_groups=None,
               confirm_immediate=False):
    params = hostapd.wpa2_params(ssid=ssid)
    params['wpa_key_mgmt'] = 'SAE'
    params['sae_password'] = ['%s|pk=%s:%s' % (pw, m, pk)]
    if ap_groups:
        params['sae_groups'] = ap_groups
    if confirm_immediate:
        params['sae_confirm_immediate'] = '1'
    hapd = hostapd.add_ap(apdev, params)
    bssid = hapd.own_addr()

    dev.connect(ssid, sae_password=pw, key_mgmt="SAE", scan_freq="2412")
    bss = dev.get_bss(bssid)
    if 'flags' not in bss:
        raise Exception("Could not get BSS flags from BSS table")
    if "[SAE-H2E]" not in bss['flags'] or "[SAE-PK]" not in bss['flags']:
        raise Exception("Unexpected BSS flags: " + bss['flags'])
    status = dev.get_status()
    if "sae_h2e" not in status or "sae_pk" not in status or \
       status["sae_h2e"] != "1" or status["sae_pk"] != "1":
        raise Exception("SAE-PK or H2E not indicated in STATUS")
    dev.request("REMOVE_NETWORK *")
    dev.wait_disconnected()
    hapd.disable()

def test_sae_pk(dev, apdev):
    """SAE-PK"""
    check_sae_pk_capab(dev[0])
    dev[0].flush_scan_cache()
    dev[0].set("sae_groups", "")

    passwords = [SAE_PK_SEC3_PW,
                 "r6cr-6ksa-56oo-5557",
                 "r6cr-6ksa-56oo-555p-wi44",
                 "r6cr-6ksa-56oo-555p-wi4b-vghb",
                 "r6cr-6ksa-56oo-555p-wi4b-vghv-vwro",
                 "r6cr-6ksa-56oo-555p-wi4b-vghv-vwrp-taqj",
                 "r6cr-6ksa-56oo-555p-wi4b-vghv-vwrp-taq5-4zfq",
                 "r6cr-6ksa-56oo-555p-wi4b-vghv-vwrp-taq5-4zfa-ye3x",
                 "r6cr-6ksa-56oo-555p-wi4b-vghv-vwrp-taq5-4zfa-ye35-4rne",
                 "r6cr-6ksa-56oo-555p-wi4b-vghv-vwrp-taq5-4zfa-ye35-4rny-5yqz"]
    for p in passwords:
        run_sae_pk(apdev[0], dev[0], SAE_PK_SSID, p, SAE_PK_SEC3_M,
                   SAE_PK_19_PK)

def test_sae_pk_group_negotiation(dev, apdev):
    """SAE-PK"""
    check_sae_pk_capab(dev[0])
    dev[0].flush_scan_cache()
    dev[0].set("sae_groups", "20 19")

    try:
        run_sae_pk(apdev[0], dev[0], SAE_PK_SSID, SAE_PK_SEC3_PW,
                   SAE_PK_SEC3_M, SAE_PK_19_PK, ap_groups="19 20")
    finally:
        dev[0].set("sae_groups", "")

def test_sae_pk_sec_3(dev, apdev):
    """SAE-PK with Sec 3"""
    check_sae_pk_capab(dev[0])
    dev[0].flush_scan_cache()
    dev[0].set("sae_groups", "")

    run_sae_pk(apdev[0], dev[0], SAE_PK_SSID, SAE_PK_SEC3_PW, SAE_PK_SEC3_M,
               SAE_PK_19_PK)

def test_sae_pk_sec_5(dev, apdev):
    """SAE-PK with Sec 5"""
    check_sae_pk_capab(dev[0])
    dev[0].flush_scan_cache()
    dev[0].set("sae_groups", "")

    pw = "hbbi-f4xq-b457-jjew-muei"
    m = "d2e5fa27d1be8897f987f2d480d2af6b"
    run_sae_pk(apdev[0], dev[0], SAE_PK_SSID, pw, m, SAE_PK_19_PK)

def test_sae_pk_group_20(dev, apdev):
    """SAE-PK with group 20"""
    check_sae_pk_capab(dev[0])
    dev[0].flush_scan_cache()
    dev[0].set("sae_groups", "20")

    try:
        run_sae_pk(apdev[0], dev[0], SAE_PK_SSID, SAE_PK_20_PW,
                   SAE_PK_20_M, SAE_PK_20_PK, ap_groups="20")
    finally:
        dev[0].set("sae_groups", "")

def test_sae_pk_group_21(dev, apdev):
    """SAE-PK with group 21"""
    check_sae_pk_capab(dev[0])
    dev[0].flush_scan_cache()
    dev[0].set("sae_groups", "21")

    try:
        run_sae_pk(apdev[0], dev[0], SAE_PK_SSID, SAE_PK_21_PW,
                   SAE_PK_21_M, SAE_PK_21_PK, ap_groups="21")
    finally:
        dev[0].set("sae_groups", "")

def test_sae_pk_group_20_sae_group_19(dev, apdev):
    """SAE-PK with group 20 with SAE group 19"""
    check_sae_pk_capab(dev[0])
    dev[0].flush_scan_cache()
    dev[0].set("sae_groups", "19")
    try:
        run_sae_pk(apdev[0], dev[0], SAE_PK_SSID, SAE_PK_20_PW,
                   SAE_PK_20_M, SAE_PK_20_PK, ap_groups="19")
    finally:
        dev[0].set("sae_groups", "")

def test_sae_pk_group_20_sae_group_21(dev, apdev):
    """SAE-PK with group 20 with SAE group 21"""
    check_sae_pk_capab(dev[0])
    dev[0].flush_scan_cache()
    dev[0].set("sae_groups", "21")
    try:
        run_sae_pk(apdev[0], dev[0], SAE_PK_SSID, SAE_PK_20_PW,
                   SAE_PK_20_M, SAE_PK_20_PK, ap_groups="21")
    finally:
        dev[0].set("sae_groups", "")

def test_sae_pk_group_19_sae_group_20(dev, apdev):
    """SAE-PK with group 19 with SAE group 20"""
    check_sae_pk_capab(dev[0])
    dev[0].flush_scan_cache()
    dev[0].set("sae_groups", "20")
    try:
        run_sae_pk(apdev[0], dev[0], SAE_PK_SSID, SAE_PK_SEC3_PW,
                   SAE_PK_SEC3_M, SAE_PK_19_PK, ap_groups="20")
    finally:
        dev[0].set("sae_groups", "")

def test_sae_pk_password_without_pk(dev, apdev):
    """SAE-PK password but not SAE-PK on the AP"""
    check_sae_pk_capab(dev[0])
    dev[0].set("sae_groups", "")

    params = hostapd.wpa2_params(ssid=SAE_PK_SSID)
    params['wpa_key_mgmt'] = 'SAE'
    params['sae_password'] = SAE_PK_SEC3_PW
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].connect(SAE_PK_SSID, sae_password=SAE_PK_SEC3_PW,
                   key_mgmt="SAE", scan_freq="2412")
    if dev[0].get_status_field("sae_pk") != "0":
        raise Exception("Unexpected sae_pk STATUS value")

def test_sae_pk_only(dev, apdev):
    """SAE-PK only"""
    check_sae_pk_capab(dev[0])
    dev[0].set("sae_groups", "")

    params = hostapd.wpa2_params(ssid=SAE_PK_SSID)
    params['wpa_key_mgmt'] = 'SAE'
    params['sae_password'] = SAE_PK_SEC3_PW
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].connect(SAE_PK_SSID, sae_password=SAE_PK_SEC3_PW,
                   key_mgmt="SAE", sae_pk="1",
                   scan_freq="2412", wait_connect=False)
    ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED",
                            "CTRL-EVENT-NETWORK-NOT-FOUND"], timeout=10)
    if ev is None:
        raise Exception("No result for the connection attempt")
    if "CTRL-EVENT-CONNECTED" in ev:
        raise Exception("Unexpected connection without SAE-PK")
    dev[0].request("DISCONNECT")
    dev[0].dump_monitor()

    params = hostapd.wpa2_params(ssid=SAE_PK_SSID)
    params['wpa_key_mgmt'] = 'SAE'
    params['sae_password'] = ['%s|pk=%s:%s' % (SAE_PK_SEC3_PW, SAE_PK_SEC3_M,
                                               SAE_PK_19_PK)]
    hapd2 = hostapd.add_ap(apdev[1], params)
    bssid2 = hapd2.own_addr()

    dev[0].scan_for_bss(bssid2, freq=2412, force_scan=True)
    dev[0].request("RECONNECT")
    ev = dev[0].wait_connected()
    if bssid2 not in ev:
        raise Exception("Unexpected connection BSSID")
    if dev[0].get_status_field("sae_pk") != "1":
        raise Exception("SAE-PK was not used")

def test_sae_pk_modes(dev, apdev):
    """SAE-PK modes"""
    check_sae_pk_capab(dev[0])
    dev[0].set("sae_groups", "")

    params = hostapd.wpa2_params(ssid=SAE_PK_SSID)
    params['wpa_key_mgmt'] = 'SAE'
    params["ieee80211w"] = "2"
    params['sae_password'] = ['%s|pk=%s:%s' % (SAE_PK_SEC3_PW, SAE_PK_SEC3_M,
                                               SAE_PK_19_PK)]
    hapd = hostapd.add_ap(apdev[0], params)

    tests = [(2, 0), (1, 1), (0, 1)]
    for sae_pk, expected in tests:
        dev[0].connect(SAE_PK_SSID, sae_password=SAE_PK_SEC3_PW,
                       key_mgmt="SAE", sae_pk=str(sae_pk), ieee80211w="2",
                       scan_freq="2412")
        val = dev[0].get_status_field("sae_pk")
        if val != str(expected):
            raise Exception("Unexpected sae_pk=%d result %s" % (sae_pk, val))
        dev[0].request("REMOVE_NETWORK *")
        dev[0].wait_disconnected()
        dev[0].dump_monitor()

def test_sae_pk_not_on_ap(dev, apdev):
    """SAE-PK password, but no PK on AP"""
    check_sae_pk_capab(dev[0])
    dev[0].set("sae_groups", "")

    params = hostapd.wpa2_params(ssid=SAE_PK_SSID)
    params['wpa_key_mgmt'] = 'SAE'
    params['sae_password'] = SAE_PK_SEC3_PW
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].connect(SAE_PK_SSID, sae_password=SAE_PK_SEC3_PW,
                   key_mgmt="SAE", scan_freq="2412")
    if dev[0].get_status_field("sae_pk") == "1":
        raise Exception("SAE-PK was claimed to be used")

def test_sae_pk_transition_disable(dev, apdev):
    """SAE-PK transition disable indication"""
    check_sae_pk_capab(dev[0])
    dev[0].set("sae_groups", "")

    params = hostapd.wpa2_params(ssid=SAE_PK_SSID)
    params['wpa_key_mgmt'] = 'SAE'
    params['sae_password'] = ['%s|pk=%s:%s' % (SAE_PK_SEC3_PW, SAE_PK_SEC3_M,
                                               SAE_PK_19_PK)]
    params['transition_disable'] = '0x02'
    hapd = hostapd.add_ap(apdev[0], params)

    id = dev[0].connect(SAE_PK_SSID, sae_password=SAE_PK_SEC3_PW,
                        key_mgmt="SAE", scan_freq="2412")
    ev = dev[0].wait_event(["TRANSITION-DISABLE"], timeout=1)
    if ev is None:
        raise Exception("Transition disable not indicated")
    if ev.split(' ')[1] != "02":
        raise Exception("Unexpected transition disable bitmap: " + ev)

    val = dev[0].get_network(id, "sae_pk")
    if val != "1":
        raise Exception("Unexpected sae_pk value: " + str(val))

def test_sae_pk_mixed(dev, apdev):
    """SAE-PK mixed deployment"""
    run_sae_pk_mixed(dev, apdev)

def test_sae_pk_mixed_immediate_confirm(dev, apdev):
    """SAE-PK mixed deployment with immediate confirm on AP"""
    run_sae_pk_mixed(dev, apdev, confirm_immediate=True)

def run_sae_pk_mixed(dev, apdev, confirm_immediate=False):
    check_sae_pk_capab(dev[0])
    dev[0].set("sae_groups", "")

    params = hostapd.wpa2_params(ssid=SAE_PK_SSID)
    params['wpa_key_mgmt'] = 'SAE'
    params['sae_password'] = SAE_PK_SEC3_PW
    if confirm_immediate:
        params['sae_confirm_immediate'] = '1'
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = hapd.own_addr()

    params = hostapd.wpa2_params(ssid=SAE_PK_SSID)
    params['wpa_key_mgmt'] = 'SAE'
    params['sae_password'] = ['%s|pk=%s:%s' % (SAE_PK_SEC3_PW, SAE_PK_SEC3_M,
                                               SAE_PK_19_PK)]
    # Disable HT from the SAE-PK BSS to make the station prefer the other BSS
    # by default.
    params['ieee80211n'] = '0'
    hapd2 = hostapd.add_ap(apdev[1], params)
    bssid2 = hapd2.own_addr()

    dev[0].scan_for_bss(bssid, freq=2412)
    dev[0].scan_for_bss(bssid2, freq=2412)

    dev[0].connect(SAE_PK_SSID, sae_password=SAE_PK_SEC3_PW,
                   key_mgmt="SAE", scan_freq="2412")

    if dev[0].get_status_field("sae_pk") != "1":
        raise Exception("SAE-PK was not used")
    if dev[0].get_status_field("bssid") != bssid2:
        raise Exception("Unexpected BSSID selected")

def check_sae_pk_sta_connect_failure(dev):
    dev.connect(SAE_PK_SSID, sae_password=SAE_PK_SEC3_PW,
                key_mgmt="SAE", scan_freq="2412", wait_connect=False)
    ev = dev.wait_event(["CTRL-EVENT-CONNECTED",
                         "CTRL-EVENT-SSID-TEMP-DISABLED"], timeout=10)
    if ev is None:
        raise Exception("No result for the connection attempt")
    if "CTRL-EVENT-CONNECTED" in ev:
        raise Exception("Unexpected connection")

def test_sae_pk_missing_ie(dev, apdev):
    """SAE-PK and missing SAE-PK IE in confirm"""
    check_sae_pk_capab(dev[0])
    dev[0].set("sae_groups", "")

    params = hostapd.wpa2_params(ssid=SAE_PK_SSID)
    params['wpa_key_mgmt'] = 'SAE'
    params['sae_password'] = ['%s|pk=%s:%s' % (SAE_PK_SEC3_PW, SAE_PK_SEC3_M,
                                               SAE_PK_19_PK)]
    params['sae_pk_omit'] = '1'
    hapd = hostapd.add_ap(apdev[0], params)
    check_sae_pk_sta_connect_failure(dev[0])

def test_sae_pk_unexpected_status(dev, apdev):
    """SAE-PK and unexpected status code in commit"""
    check_sae_pk_capab(dev[0])
    dev[0].set("sae_groups", "")

    params = hostapd.wpa2_params(ssid=SAE_PK_SSID)
    params['wpa_key_mgmt'] = 'SAE'
    params['sae_password'] = ['%s|pk=%s:%s' % (SAE_PK_SEC3_PW, SAE_PK_SEC3_M,
                                               SAE_PK_19_PK)]
    params['sae_commit_status'] = '126'
    hapd = hostapd.add_ap(apdev[0], params)
    check_sae_pk_sta_connect_failure(dev[0])

def test_sae_pk_invalid_signature(dev, apdev):
    """SAE-PK and invalid signature"""
    check_sae_pk_capab(dev[0])
    dev[0].set("sae_groups", "")

    other = "MHcCAQEEILw+nTjFzRyhVea0G6KbwZu18oWrfhzppxj+MceUO3YLoAoGCCqGSM49AwEHoUQDQgAELdou6LuTDNiMVlMB65KsWhQFbPXR9url0EA6luWzUfAuGoDXYJUBTVz6Nv3mz6oQcDrSiDmz/LejndJ0YHGgfQ=="
    params = hostapd.wpa2_params(ssid=SAE_PK_SSID)
    params['wpa_key_mgmt'] = 'SAE'
    params['sae_password'] = ['%s|pk=%s:%s:%s' % (SAE_PK_SEC3_PW, SAE_PK_SEC3_M,
                                                  SAE_PK_19_PK, other)]
    hapd = hostapd.add_ap(apdev[0], params)
    check_sae_pk_sta_connect_failure(dev[0])

def test_sae_pk_invalid_fingerprint(dev, apdev):
    """SAE-PK and invalid fingerprint"""
    check_sae_pk_capab(dev[0])
    dev[0].set("sae_groups", "")

    other = "431ff8322f93b9dc50ded9f3d14ace21"
    params = hostapd.wpa2_params(ssid=SAE_PK_SSID)
    params['wpa_key_mgmt'] = 'SAE'
    params['sae_password'] = ['%s|pk=%s:%s' % (SAE_PK_SEC3_PW, other,
                                               SAE_PK_19_PK)]
    hapd = hostapd.add_ap(apdev[0], params)
    check_sae_pk_sta_connect_failure(dev[0])

def test_sae_pk_confirm_immediate(dev, apdev):
    """SAE-PK with immediate confirm on AP"""
    check_sae_pk_capab(dev[0])
    dev[0].flush_scan_cache()
    dev[0].set("sae_groups", "")

    run_sae_pk(apdev[0], dev[0], SAE_PK_SSID, SAE_PK_SEC3_PW,
               SAE_PK_SEC3_M, SAE_PK_19_PK, confirm_immediate=True)

def test_sae_pk_and_psk(dev, apdev):
    """SAE-PK and PSK"""
    check_sae_pk_capab(dev[0])
    dev[0].flush_scan_cache()
    dev[0].set("sae_groups", "")
    dev[2].set("sae_groups", "")

    params = hostapd.wpa2_params(ssid=SAE_PK_SSID)
    params['wpa_key_mgmt'] = 'SAE WPA-PSK'
    params['sae_password'] = ['%s|pk=%s:%s' % (SAE_PK_SEC3_PW,
                                               SAE_PK_SEC3_M,
                                               SAE_PK_19_PK)]
    params['wpa_passphrase'] = SAE_PK_SEC3_PW
    hapd = hostapd.add_ap(apdev[0], params)
    bssid = hapd.own_addr()

    dev[0].connect(SAE_PK_SSID, sae_password=SAE_PK_SEC3_PW, key_mgmt="SAE",
                   scan_freq="2412")
    dev[1].connect(SAE_PK_SSID, psk=SAE_PK_SEC3_PW, key_mgmt="WPA-PSK",
                   scan_freq="2412")
    dev[2].connect(SAE_PK_SSID, sae_password=SAE_PK_SEC3_PW, key_mgmt="SAE",
                   sae_pk="2", scan_freq="2412")
    dev[0].request("REMOVE_NETWORK *")
    dev[0].wait_disconnected()
    dev[0].dump_monitor()
    dev[0].connect(SAE_PK_SSID, psk=SAE_PK_SEC3_PW, key_mgmt="SAE",
                   scan_freq="2412")
    status = dev[0].get_status()
    if "sae_h2e" not in status or "sae_pk" not in status or \
       status["sae_h2e"] != "1" or status["sae_pk"] != "1":
        raise Exception("SAE-PK or H2E not indicated in STATUS")

def test_sae_pk_and_psk_invalid_password(dev, apdev):
    """SAE-PK and PSK using invalid password combination"""
    check_sae_pk_capab(dev[0])
    dev[0].flush_scan_cache()

    params = hostapd.wpa2_params(ssid=SAE_PK_SSID)
    params['wpa_key_mgmt'] = 'SAE WPA-PSK'
    params['sae_password'] = ['%s|pk=%s:%s' % (SAE_PK_SEC3_PW,
                                               SAE_PK_SEC3_M,
                                               SAE_PK_19_PK)]
    params['wpa_passphrase'] = SAE_PK_20_PW
    hapd = hostapd.add_ap(apdev[0], params, no_enable=True)
    res = hapd.request("ENABLE")
    if "FAIL" not in res:
        raise Exception("Invalid configuration accepted")

def test_sae_pk_invalid_pw(dev, apdev):
    """SAE-PK with invalid password on AP"""
    check_sae_pk_capab(dev[0])
    dev[0].set("sae_groups", "")

    params = hostapd.wpa2_params(ssid=SAE_PK_SSID)
    params['wpa_key_mgmt'] = 'SAE'
    params["ieee80211w"] = "2"
    params["sae_pk_password_check_skip"] = "1"
    invalid_pw = "r6cr+6ksa+56og"
    params['sae_password'] = ['%s|pk=%s:%s' % (invalid_pw, SAE_PK_SEC3_M,
                                               SAE_PK_19_PK)]
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].connect(SAE_PK_SSID, sae_password=invalid_pw,
                   key_mgmt="SAE", ieee80211w="2", scan_freq="2412")
    dev[0].request("REMOVE_NETWORK *")
    dev[0].wait_disconnected()
    dev[0].dump_monitor()

    dev[0].connect(SAE_PK_SSID, sae_password=SAE_PK_SEC3_PW,
                   key_mgmt="SAE", ieee80211w="2", scan_freq="2412",
                   wait_connect=False)
    ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED",
                            "CTRL-EVENT-SSID-TEMP-DISABLED"], timeout=10)
    if ev is None:
        raise Exception("No result for the connection attempt")
    if "CTRL-EVENT-CONNECTED" in ev:
        raise Exception("Unexpected connection with invalid SAE-PK password")
    dev[0].request("DISCONNECT")
