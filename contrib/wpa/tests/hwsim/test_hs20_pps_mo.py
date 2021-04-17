# Hotspot 2.0 PPS MO tests
# Copyright (c) 2018, The Linux Foundation
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

import logging
logger = logging.getLogger()
import os.path
import subprocess

import hostapd
from utils import HwsimSkip
from test_ap_hs20 import hs20_ap_params, interworking_select, interworking_connect, check_sp_type
from test_ap_eap import check_eap_capa, check_domain_suffix_match

def check_hs20_osu_client():
    if not os.path.exists("../../hs20/client/hs20-osu-client"):
        raise HwsimSkip("No hs20-osu-client available")

def set_pps(pps_mo):
    res = subprocess.check_output(["../../hs20/client/hs20-osu-client",
                                   "set_pps", pps_mo]).decode()
    logger.info("set_pps result: " + res)

def test_hs20_pps_mo_1(dev, apdev):
    """Hotspot 2.0 PPS MO with username/password credential"""
    check_hs20_osu_client()
    check_eap_capa(dev[0], "MSCHAPV2")
    check_domain_suffix_match(dev[0])
    bssid = apdev[0]['bssid']
    params = hs20_ap_params()
    params['hessid'] = bssid
    params['nai_realm'] = ["0,w1.fi,13[5:6],21[2:4][5:7]",
                           "0,another.example.com"]
    params['domain_name'] = "w1.fi"
    hapd = hostapd.add_ap(apdev[0], params)

    dev[0].hs20_enable()
    set_pps("pps-mo-1.xml")
    interworking_select(dev[0], bssid, "home", freq="2412")
    interworking_connect(dev[0], bssid, "TTLS")
    check_sp_type(dev[0], "home")
