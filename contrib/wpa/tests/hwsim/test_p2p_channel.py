# P2P channel selection test cases
# Copyright (c) 2014, Jouni Malinen <j@w1.fi>
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

from remotehost import remote_compatible
import logging
logger = logging.getLogger()
import os
import subprocess
import time

import hostapd
import hwsim_utils
from tshark import run_tshark
from wpasupplicant import WpaSupplicant
from hwsim import HWSimRadio
from p2p_utils import *
from utils import *

def set_country(country, dev=None):
    subprocess.call(['iw', 'reg', 'set', country])
    time.sleep(0.1)
    if dev:
        for i in range(10):
            ev = dev.wait_global_event(["CTRL-EVENT-REGDOM-CHANGE"], timeout=15)
            if ev is None:
                raise Exception("No regdom change event seen")
            if "type=COUNTRY alpha2=" + country in ev:
                return
        raise Exception("No matching regdom event seen for set_country(%s)" % country)

def test_p2p_channel_5ghz(dev):
    """P2P group formation with 5 GHz preference"""
    try:
        set_country("US", dev[0])
        [i_res, r_res] = go_neg_pin_authorized(i_dev=dev[0], i_intent=15,
                                               r_dev=dev[1], r_intent=0,
                                               test_data=False)
        check_grpform_results(i_res, r_res)
        freq = int(i_res['freq'])
        if freq < 5000:
            raise Exception("Unexpected channel %d MHz - did not follow 5 GHz preference" % freq)
        remove_group(dev[0], dev[1])
    finally:
        set_country("00")
        dev[1].flush_scan_cache()

def test_p2p_channel_5ghz_no_vht(dev):
    """P2P group formation with 5 GHz preference when VHT channels are disallowed"""
    try:
        set_country("US", dev[0])
        dev[0].global_request("P2P_SET disallow_freq 5180-5240")
        [i_res, r_res] = go_neg_pin_authorized(i_dev=dev[0], i_intent=15,
                                               r_dev=dev[1], r_intent=0,
                                               test_data=False)
        check_grpform_results(i_res, r_res)
        freq = int(i_res['freq'])
        if freq < 5000:
            raise Exception("Unexpected channel %d MHz - did not follow 5 GHz preference" % freq)
        remove_group(dev[0], dev[1])
    finally:
        set_country("00")
        dev[0].global_request("P2P_SET disallow_freq ")
        dev[1].flush_scan_cache()

def test_p2p_channel_random_social(dev):
    """P2P group formation with 5 GHz preference but all 5 GHz channels disabled"""
    try:
        set_country("US", dev[0])
        dev[0].global_request("SET p2p_oper_channel 11")
        dev[0].global_request("P2P_SET disallow_freq 5000-6000,2462")
        [i_res, r_res] = go_neg_pin_authorized(i_dev=dev[0], i_intent=15,
                                               r_dev=dev[1], r_intent=0,
                                               test_data=False)
        check_grpform_results(i_res, r_res)
        freq = int(i_res['freq'])
        if freq not in [2412, 2437, 2462]:
            raise Exception("Unexpected channel %d MHz - did not pick random social channel" % freq)
        remove_group(dev[0], dev[1])
    finally:
        set_country("00")
        dev[0].global_request("P2P_SET disallow_freq ")
        dev[1].flush_scan_cache()

def test_p2p_channel_random(dev):
    """P2P group formation with 5 GHz preference but all 5 GHz channels and all social channels disabled"""
    try:
        set_country("US", dev[0])
        dev[0].global_request("SET p2p_oper_channel 11")
        dev[0].global_request("P2P_SET disallow_freq 5000-6000,2412,2437,2462")
        [i_res, r_res] = go_neg_pin_authorized(i_dev=dev[0], i_intent=15,
                                               r_dev=dev[1], r_intent=0,
                                               test_data=False)
        check_grpform_results(i_res, r_res)
        freq = int(i_res['freq'])
        if freq > 2500 or freq in [2412, 2437, 2462]:
            raise Exception("Unexpected channel %d MHz" % freq)
        remove_group(dev[0], dev[1])
    finally:
        set_country("00")
        dev[0].global_request("P2P_SET disallow_freq ")
        dev[1].flush_scan_cache()

def test_p2p_channel_random_social_with_op_class_change(dev, apdev, params):
    """P2P group formation using random social channel with oper class change needed"""
    try:
        set_country("US", dev[0])
        logger.info("Start group on 5 GHz")
        [i_res, r_res] = go_neg_pin_authorized(i_dev=dev[0], i_intent=15,
                                               r_dev=dev[1], r_intent=0,
                                               test_data=False)
        check_grpform_results(i_res, r_res)
        freq = int(i_res['freq'])
        if freq < 5000:
            raise Exception("Unexpected channel %d MHz - did not pick 5 GHz preference" % freq)
        remove_group(dev[0], dev[1])

        logger.info("Disable 5 GHz and try to re-start group based on 5 GHz preference")
        dev[0].global_request("SET p2p_oper_reg_class 115")
        dev[0].global_request("SET p2p_oper_channel 36")
        dev[0].global_request("P2P_SET disallow_freq 5000-6000")
        [i_res, r_res] = go_neg_pin_authorized(i_dev=dev[0], i_intent=15,
                                               r_dev=dev[1], r_intent=0,
                                               test_data=False)
        check_grpform_results(i_res, r_res)
        freq = int(i_res['freq'])
        if freq not in [2412, 2437, 2462]:
            raise Exception("Unexpected channel %d MHz - did not pick random social channel" % freq)
        remove_group(dev[0], dev[1])

        out = run_tshark(os.path.join(params['logdir'], "hwsim0.pcapng"),
                         "wifi_p2p.public_action.subtype == 0")
        if out is not None:
            last = None
            for l in out.splitlines():
                if "Operating Channel:" not in l:
                    continue
                last = l
            if last is None:
                raise Exception("Could not find GO Negotiation Request")
            if "Operating Class 81" not in last:
                raise Exception("Unexpected operating class: " + last.strip())
    finally:
        set_country("00")
        dev[0].global_request("P2P_SET disallow_freq ")
        dev[0].global_request("SET p2p_oper_reg_class 0")
        dev[0].global_request("SET p2p_oper_channel 0")
        dev[1].flush_scan_cache()

def test_p2p_channel_avoid(dev):
    """P2P and avoid frequencies driver event"""
    try:
        set_country("US", dev[0])
        if "OK" not in dev[0].request("DRIVER_EVENT AVOID_FREQUENCIES 5000-6000,2412,2437,2462"):
            raise Exception("Could not simulate driver event")
        ev = dev[0].wait_event(["CTRL-EVENT-AVOID-FREQ"], timeout=10)
        if ev is None:
            raise Exception("No CTRL-EVENT-AVOID-FREQ event")
        [i_res, r_res] = go_neg_pin_authorized(i_dev=dev[0], i_intent=15,
                                               r_dev=dev[1], r_intent=0,
                                               test_data=False)
        check_grpform_results(i_res, r_res)
        freq = int(i_res['freq'])
        if freq > 2500 or freq in [2412, 2437, 2462]:
            raise Exception("Unexpected channel %d MHz" % freq)

        if "OK" not in dev[0].request("DRIVER_EVENT AVOID_FREQUENCIES"):
            raise Exception("Could not simulate driver event(2)")
        ev = dev[0].wait_event(["CTRL-EVENT-AVOID-FREQ"], timeout=10)
        if ev is None:
            raise Exception("No CTRL-EVENT-AVOID-FREQ event")
        ev = dev[0].wait_group_event(["P2P-REMOVE-AND-REFORM-GROUP",
                                      "AP-CSA-FINISHED"], timeout=1)
        if ev is not None:
            raise Exception("Unexpected + " + ev + " event")

        if "OK" not in dev[0].request("DRIVER_EVENT AVOID_FREQUENCIES " + str(freq)):
            raise Exception("Could not simulate driver event(3)")
        ev = dev[0].wait_event(["CTRL-EVENT-AVOID-FREQ"], timeout=10)
        if ev is None:
            raise Exception("No CTRL-EVENT-AVOID-FREQ event")
        ev = dev[0].wait_group_event(["P2P-REMOVE-AND-REFORM-GROUP",
                                      "AP-CSA-FINISHED"],
                                     timeout=10)
        if ev is None:
            raise Exception("No P2P-REMOVE-AND-REFORM-GROUP or AP-CSA-FINISHED event")
    finally:
        set_country("00")
        dev[0].request("DRIVER_EVENT AVOID_FREQUENCIES")
        dev[1].flush_scan_cache()

def test_p2p_channel_avoid2(dev):
    """P2P and avoid frequencies driver event on 5 GHz"""
    try:
        set_country("US", dev[0])
        [i_res, r_res] = go_neg_pin_authorized(i_dev=dev[0], i_intent=15,
                                               r_dev=dev[1], r_intent=0,
                                               test_data=False,
                                               i_max_oper_chwidth=80,
                                               i_ht40=True, i_vht=True)
        check_grpform_results(i_res, r_res)
        freq = int(i_res['freq'])
        if freq < 5000:
            raise Exception("Unexpected channel %d MHz" % freq)

        if "OK" not in dev[0].request("DRIVER_EVENT AVOID_FREQUENCIES " + str(freq)):
            raise Exception("Could not simulate driver event(2)")
        ev = dev[0].wait_event(["CTRL-EVENT-AVOID-FREQ"], timeout=10)
        if ev is None:
            raise Exception("No CTRL-EVENT-AVOID-FREQ event")
        ev = dev[0].wait_group_event(["CTRL-EVENT-CHANNEL-SWITCH"], timeout=10)
        if ev is None:
            raise Exception("No channel switch event seen")
        if "ch_width=80 MHz" not in ev:
            raise Exception("Could not move to a VHT80 channel")
        ev = dev[0].wait_group_event(["AP-CSA-FINISHED"], timeout=1)
        if ev is None:
            raise Exception("No AP-CSA-FINISHED event seen")
    finally:
        set_country("00")
        dev[0].request("DRIVER_EVENT AVOID_FREQUENCIES")
        dev[1].flush_scan_cache()

def test_p2p_channel_avoid3(dev):
    """P2P and avoid frequencies driver event on 5 GHz"""
    try:
        dev[0].global_request("SET p2p_pref_chan 128:44")
        set_country("CN", dev[0])
        form(dev[0], dev[1])
        set_country("CN", dev[0])
        [i_res, r_res] = invite_from_go(dev[0], dev[1], terminate=False,
                                        extra="ht40 vht")
        freq = int(i_res['freq'])
        if freq < 5000:
            raise Exception("Unexpected channel %d MHz" % freq)

        if "OK" not in dev[0].request("DRIVER_EVENT AVOID_FREQUENCIES 5180-5320,5500-5640"):
            raise Exception("Could not simulate driver event(2)")
        ev = dev[0].wait_event(["CTRL-EVENT-AVOID-FREQ"], timeout=10)
        if ev is None:
            raise Exception("No CTRL-EVENT-AVOID-FREQ event")
        ev = dev[0].wait_group_event(["CTRL-EVENT-CHANNEL-SWITCH"], timeout=10)
        if ev is None:
            raise Exception("No channel switch event seen")
        if "ch_width=80 MHz" not in ev:
            raise Exception("Could not move to a VHT80 channel")
        ev = dev[0].wait_group_event(["AP-CSA-FINISHED"], timeout=1)
        if ev is None:
            raise Exception("No AP-CSA-FINISHED event seen")
    finally:
        set_country("00")
        dev[0].request("DRIVER_EVENT AVOID_FREQUENCIES")
        dev[0].global_request("SET p2p_pref_chan ")
        dev[1].flush_scan_cache()

@remote_compatible
def test_autogo_following_bss(dev, apdev):
    """P2P autonomous GO operate on the same channel as station interface"""
    if dev[0].get_mcc() > 1:
        logger.info("test mode: MCC")

    dev[0].global_request("SET p2p_no_group_iface 0")

    channels = {3: "2422", 5: "2432", 9: "2452"}
    for key in channels:
        hapd = hostapd.add_ap(apdev[0], {"ssid": 'ap-test',
                                         "channel": str(key)})
        dev[0].connect("ap-test", key_mgmt="NONE",
                       scan_freq=str(channels[key]))
        res_go = autogo(dev[0])
        if res_go['freq'] != channels[key]:
            raise Exception("Group operation channel is not the same as on connected station interface")
        hwsim_utils.test_connectivity(dev[0], hapd)
        dev[0].remove_group(res_go['ifname'])

@remote_compatible
def test_go_neg_with_bss_connected(dev, apdev):
    """P2P channel selection: GO negotiation when station interface is connected"""

    dev[0].flush_scan_cache()
    dev[1].flush_scan_cache()
    dev[0].global_request("SET p2p_no_group_iface 0")

    hapd = hostapd.add_ap(apdev[0],
                          {"ssid": 'bss-2.4ghz', "channel": '5'})
    dev[0].connect("bss-2.4ghz", key_mgmt="NONE", scan_freq="2432")
    #dev[0] as GO
    [i_res, r_res] = go_neg_pbc(i_dev=dev[0], i_intent=10, r_dev=dev[1],
                                r_intent=1)
    check_grpform_results(i_res, r_res)
    if i_res['role'] != "GO":
       raise Exception("GO not selected according to go_intent")
    if i_res['freq'] != "2432":
       raise Exception("Group formed on a different frequency than BSS")
    hwsim_utils.test_connectivity(dev[0], hapd)
    dev[0].remove_group(i_res['ifname'])
    dev[1].wait_go_ending_session()

    if dev[0].get_mcc() > 1:
        logger.info("Skip as-client case due to MCC being enabled")
        return

    #dev[0] as client
    [i_res2, r_res2] = go_neg_pbc(i_dev=dev[0], i_intent=1, r_dev=dev[1],
                                  r_intent=10)
    check_grpform_results(i_res2, r_res2)
    if i_res2['role'] != "client":
       raise Exception("GO not selected according to go_intent")
    if i_res2['freq'] != "2432":
       raise Exception("Group formed on a different frequency than BSS")
    hwsim_utils.test_connectivity(dev[0], hapd)
    dev[1].remove_group(r_res2['ifname'])
    dev[0].wait_go_ending_session()
    dev[0].request("DISCONNECT")
    hapd.disable()
    dev[0].flush_scan_cache()
    dev[1].flush_scan_cache()

def test_autogo_with_bss_on_disallowed_chan(dev, apdev):
    """P2P channel selection: Autonomous GO with BSS on a disallowed channel"""

    with HWSimRadio(n_channels=2) as (radio, iface):
        wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
        wpas.interface_add(iface)

        wpas.global_request("SET p2p_no_group_iface 0")

        if wpas.get_mcc() < 2:
           raise Exception("New radio does not support MCC")

        try:
            hapd = hostapd.add_ap(apdev[0], {"ssid": 'bss-2.4ghz',
                                             "channel": '1'})
            wpas.global_request("P2P_SET disallow_freq 2412")
            wpas.connect("bss-2.4ghz", key_mgmt="NONE", scan_freq="2412")
            res = autogo(wpas)
            if res['freq'] == "2412":
               raise Exception("GO set on a disallowed channel")
            hwsim_utils.test_connectivity(wpas, hapd)
        finally:
            wpas.global_request("P2P_SET disallow_freq ")

def test_go_neg_with_bss_on_disallowed_chan(dev, apdev):
    """P2P channel selection: GO negotiation with station interface on a disallowed channel"""

    with HWSimRadio(n_channels=2) as (radio, iface):
        wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
        wpas.interface_add(iface)

        wpas.global_request("SET p2p_no_group_iface 0")

        if wpas.get_mcc() < 2:
           raise Exception("New radio does not support MCC")

        try:
            hapd = hostapd.add_ap(apdev[0],
                                  {"ssid": 'bss-2.4ghz', "channel": '1'})
            # make sure PBC overlap from old test cases is not maintained
            dev[1].flush_scan_cache()
            wpas.connect("bss-2.4ghz", key_mgmt="NONE", scan_freq="2412")
            wpas.global_request("P2P_SET disallow_freq 2412")

            #wpas as GO
            [i_res, r_res] = go_neg_pbc(i_dev=wpas, i_intent=10, r_dev=dev[1],
                                        r_intent=1)
            check_grpform_results(i_res, r_res)
            if i_res['role'] != "GO":
               raise Exception("GO not selected according to go_intent")
            if i_res['freq'] == "2412":
               raise Exception("Group formed on a disallowed channel")
            hwsim_utils.test_connectivity(wpas, hapd)
            wpas.remove_group(i_res['ifname'])
            dev[1].wait_go_ending_session()
            dev[1].flush_scan_cache()

            wpas.dump_monitor()
            dev[1].dump_monitor()

            #wpas as client
            [i_res2, r_res2] = go_neg_pbc(i_dev=wpas, i_intent=1, r_dev=dev[1],
                                          r_intent=10)
            check_grpform_results(i_res2, r_res2)
            if i_res2['role'] != "client":
               raise Exception("GO not selected according to go_intent")
            if i_res2['freq'] == "2412":
               raise Exception("Group formed on a disallowed channel")
            hwsim_utils.test_connectivity(wpas, hapd)
            dev[1].remove_group(r_res2['ifname'])
            wpas.wait_go_ending_session()
            ev = dev[1].wait_global_event(["P2P-GROUP-REMOVED"], timeout=5)
            if ev is None:
                raise Exception("Group removal not indicated")
            wpas.request("DISCONNECT")
            hapd.disable()
        finally:
            wpas.global_request("P2P_SET disallow_freq ")

def test_autogo_force_diff_channel(dev, apdev):
    """P2P autonomous GO and station interface operate on different channels"""
    with HWSimRadio(n_channels=2) as (radio, iface):
        wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
        wpas.interface_add(iface)

        if wpas.get_mcc() < 2:
           raise Exception("New radio does not support MCC")

        wpas.global_request("SET p2p_no_group_iface 0")

        hapd = hostapd.add_ap(apdev[0],
                              {"ssid": 'ap-test', "channel": '1'})
        wpas.connect("ap-test", key_mgmt="NONE", scan_freq="2412")
        wpas.dump_monitor()
        channels = {2: 2417, 5: 2432, 9: 2452}
        for key in channels:
            res_go = autogo(wpas, channels[key])
            wpas.dump_monitor()
            hwsim_utils.test_connectivity(wpas, hapd)
            if int(res_go['freq']) == 2412:
                raise Exception("Group operation channel is: 2412 excepted: " + res_go['freq'])
            wpas.remove_group(res_go['ifname'])
            wpas.dump_monitor()

def test_go_neg_forced_freq_diff_than_bss_freq(dev, apdev):
    """P2P channel selection: GO negotiation with forced freq different than station interface"""
    with HWSimRadio(n_channels=2) as (radio, iface):
        wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
        wpas.interface_add(iface)

        if wpas.get_mcc() < 2:
           raise Exception("New radio does not support MCC")

        # Clear possible PBC session overlap from previous test case
        dev[1].flush_scan_cache()

        wpas.global_request("SET p2p_no_group_iface 0")

        hapd = hostapd.add_ap(apdev[0],
                              {"country_code": 'US',
                               "ssid": 'bss-5ghz', "hw_mode": 'a',
                               "channel": '40'})
        wpas.connect("bss-5ghz", key_mgmt="NONE", scan_freq="5200")

        # GO and peer force the same freq, different than BSS freq,
        # wpas to become GO
        [i_res, r_res] = go_neg_pbc(i_dev=dev[1], i_intent=1, i_freq=5180,
                                    r_dev=wpas, r_intent=14, r_freq=5180)
        check_grpform_results(i_res, r_res)
        if i_res['freq'] != "5180":
           raise Exception("P2P group formed on unexpected frequency: " + i_res['freq'])
        if r_res['role'] != "GO":
           raise Exception("GO not selected according to go_intent")
        hwsim_utils.test_connectivity(wpas, hapd)
        wpas.remove_group(r_res['ifname'])
        dev[1].wait_go_ending_session()
        dev[1].flush_scan_cache()

        # GO and peer force the same freq, different than BSS freq, wpas to
        # become client
        [i_res2, r_res2] = go_neg_pbc(i_dev=dev[1], i_intent=14, i_freq=2422,
                                      r_dev=wpas, r_intent=1, r_freq=2422)
        check_grpform_results(i_res2, r_res2)
        if i_res2['freq'] != "2422":
           raise Exception("P2P group formed on unexpected frequency: " + i_res2['freq'])
        if r_res2['role'] != "client":
           raise Exception("GO not selected according to go_intent")
        hwsim_utils.test_connectivity(wpas, hapd)

        hapd.request("DISABLE")
        wpas.request("DISCONNECT")
        wpas.request("ABORT_SCAN")
        wpas.wait_disconnected()
        subprocess.call(['iw', 'reg', 'set', '00'])
        wpas.flush_scan_cache()

@remote_compatible
def test_go_pref_chan_bss_on_diff_chan(dev, apdev):
    """P2P channel selection: Station on different channel than GO configured pref channel"""

    dev[0].global_request("SET p2p_no_group_iface 0")

    try:
        hapd = hostapd.add_ap(apdev[0], {"ssid": 'bss-2.4ghz',
                                         "channel": '1'})
        dev[0].global_request("SET p2p_pref_chan 81:2")
        dev[0].connect("bss-2.4ghz", key_mgmt="NONE", scan_freq="2412")
        res = autogo(dev[0])
        if res['freq'] != "2412":
           raise Exception("GO channel did not follow BSS")
        hwsim_utils.test_connectivity(dev[0], hapd)
    finally:
        dev[0].global_request("SET p2p_pref_chan ")

def test_go_pref_chan_bss_on_disallowed_chan(dev, apdev):
    """P2P channel selection: Station interface on different channel than GO configured pref channel, and station channel is disallowed"""
    with HWSimRadio(n_channels=2) as (radio, iface):
        wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
        wpas.interface_add(iface)

        if wpas.get_mcc() < 2:
           raise Exception("New radio does not support MCC")

        wpas.global_request("SET p2p_no_group_iface 0")

        try:
            hapd = hostapd.add_ap(apdev[0], {"ssid": 'bss-2.4ghz',
                                             "channel": '1'})
            wpas.global_request("P2P_SET disallow_freq 2412")
            wpas.global_request("SET p2p_pref_chan 81:2")
            wpas.connect("bss-2.4ghz", key_mgmt="NONE", scan_freq="2412")
            res2 = autogo(wpas)
            if res2['freq'] != "2417":
               raise Exception("GO channel did not follow pref_chan configuration")
            hwsim_utils.test_connectivity(wpas, hapd)
        finally:
            wpas.global_request("P2P_SET disallow_freq ")
            wpas.global_request("SET p2p_pref_chan ")

@remote_compatible
def test_no_go_freq(dev, apdev):
    """P2P channel selection: no GO freq"""
    try:
       dev[0].global_request("SET p2p_no_go_freq 2412")
       # dev[0] as client, channel 1 is ok
       [i_res, r_res] = go_neg_pbc(i_dev=dev[0], i_intent=1,
                                   r_dev=dev[1], r_intent=14, r_freq=2412)
       check_grpform_results(i_res, r_res)
       if i_res['freq'] != "2412":
          raise Exception("P2P group not formed on forced freq")

       dev[1].remove_group(r_res['ifname'])
       dev[0].wait_go_ending_session()
       dev[0].flush_scan_cache()

       fail = False
       # dev[0] as GO, channel 1 is not allowed
       try:
          dev[0].global_request("SET p2p_no_go_freq 2412")
          [i_res2, r_res2] = go_neg_pbc(i_dev=dev[0], i_intent=14,
                                        r_dev=dev[1], r_intent=1, r_freq=2412)
          check_grpform_results(i_res2, r_res2)
          fail = True
       except:
           pass
       if fail:
           raise Exception("GO set on a disallowed freq")
    finally:
       dev[0].global_request("SET p2p_no_go_freq ")

@remote_compatible
def test_go_neg_peers_force_diff_freq(dev, apdev):
    """P2P channel selection when peers for different frequency"""
    try:
       [i_res2, r_res2] = go_neg_pbc(i_dev=dev[0], i_intent=14, i_freq=5180,
                                     r_dev=dev[1], r_intent=0, r_freq=5200)
    except Exception as e:
        return
    raise Exception("Unexpected group formation success")

@remote_compatible
def test_autogo_random_channel(dev, apdev):
    """P2P channel selection: GO instantiated on random channel 1, 6, 11"""
    freqs = []
    go_freqs = ["2412", "2437", "2462"]
    for i in range(0, 20):
        result = autogo(dev[0])
        if result['freq'] not in go_freqs:
           raise Exception("Unexpected frequency selected: " + result['freq'])
        if result['freq'] not in freqs:
            freqs.append(result['freq'])
        if len(freqs) == 3:
            break
        dev[0].remove_group(result['ifname'])
    if i == 20:
       raise Exception("GO created 20 times and not all social channels were selected. freqs not selected: " + str(list(set(go_freqs) - set(freqs))))

@remote_compatible
def test_p2p_autogo_pref_chan_disallowed(dev, apdev):
    """P2P channel selection: GO preferred channels are disallowed"""
    try:
       dev[0].global_request("SET p2p_pref_chan 81:1,81:3,81:6,81:9,81:11")
       dev[0].global_request("P2P_SET disallow_freq 2412,2422,2437,2452,2462")
       for i in range(0, 5):
           res = autogo(dev[0])
           if res['freq'] in ["2412", "2422", "2437", "2452", "2462"]:
               raise Exception("GO channel is disallowed")
           dev[0].remove_group(res['ifname'])
    finally:
       dev[0].global_request("P2P_SET disallow_freq ")
       dev[0].global_request("SET p2p_pref_chan ")

def test_p2p_autogo_pref_chan_not_in_regulatory(dev, apdev):
    """P2P channel selection: GO preferred channel not allowed in the regulatory rules"""
    try:
        set_country("US", dev[0])
        dev[0].global_request("SET p2p_pref_chan 124:149")
        res = autogo(dev[0], persistent=True)
        if res['freq'] != "5745":
            raise Exception("Unexpected channel selected: " + res['freq'])
        dev[0].remove_group(res['ifname'])

        netw = dev[0].list_networks(p2p=True)
        if len(netw) != 1:
            raise Exception("Unexpected number of network blocks: " + str(netw))
        id = netw[0]['id']

        set_country("JP", dev[0])
        res = autogo(dev[0], persistent=id)
        if res['freq'] == "5745":
            raise Exception("Unexpected channel selected(2): " + res['freq'])
        dev[0].remove_group(res['ifname'])
    finally:
        dev[0].global_request("SET p2p_pref_chan ")
        clear_regdom_dev(dev)

def run_autogo(dev, param):
    if "OK" not in dev.global_request("P2P_GROUP_ADD " + param):
        raise Exception("P2P_GROUP_ADD failed: " + param)
    ev = dev.wait_global_event(["P2P-GROUP-STARTED"], timeout=10)
    if ev is None:
        raise Exception("GO start up timed out")
    res = dev.group_form_result(ev)
    dev.remove_group()
    return res

def _test_autogo_ht_vht(dev):
    res = run_autogo(dev[0], "ht40")

    res = run_autogo(dev[0], "vht")

    res = run_autogo(dev[0], "freq=2")
    freq = int(res['freq'])
    if freq < 2412 or freq > 2462:
        raise Exception("Unexpected freq=2 channel: " + str(freq))

    res = run_autogo(dev[0], "freq=5")
    freq = int(res['freq'])
    if freq < 5000 or freq >= 6000:
        raise Exception("Unexpected freq=5 channel: " + str(freq))

    res = run_autogo(dev[0], "freq=5 ht40 vht")
    logger.info(str(res))
    freq = int(res['freq'])
    if freq < 5000 or freq >= 6000:
        raise Exception("Unexpected freq=5 ht40 vht channel: " + str(freq))

def test_autogo_ht_vht(dev):
    """P2P autonomous GO with HT/VHT parameters"""
    try:
        set_country("US", dev[0])
        _test_autogo_ht_vht(dev)
    finally:
        clear_regdom_dev(dev)

def test_p2p_listen_chan_optimize(dev, apdev):
    """P2P listen channel optimization"""
    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    wpas.interface_add("wlan5")
    addr5 = wpas.p2p_dev_addr()
    try:
        if "OK" not in wpas.global_request("SET p2p_optimize_listen_chan 1"):
            raise Exception("Failed to set p2p_optimize_listen_chan")
        wpas.p2p_listen()
        if not dev[0].discover_peer(addr5):
            raise Exception("Could not discover peer")
        peer = dev[0].get_peer(addr5)
        lfreq = peer['listen_freq']
        wpas.p2p_stop_find()
        dev[0].p2p_stop_find()

        channel = "1" if lfreq != '2412' else "6"
        freq = "2412" if lfreq != '2412' else "2437"
        params = {"ssid": "test-open", "channel": channel}
        hapd = hostapd.add_ap(apdev[0], params)

        id = wpas.connect("test-open", key_mgmt="NONE", scan_freq=freq)
        wpas.p2p_listen()

        if "OK" not in dev[0].global_request("P2P_FLUSH"):
            raise Exception("P2P_FLUSH failed")
        if not dev[0].discover_peer(addr5):
            raise Exception("Could not discover peer")
        peer = dev[0].get_peer(addr5)
        lfreq2 = peer['listen_freq']
        if lfreq == lfreq2:
            raise Exception("Listen channel did not change")
        if lfreq2 != freq:
            raise Exception("Listen channel not on AP's operating channel")
        wpas.p2p_stop_find()
        dev[0].p2p_stop_find()

        wpas.request("DISCONNECT")
        wpas.wait_disconnected()

        # for larger coverage, cover case of current channel matching
        wpas.select_network(id)
        wpas.wait_connected()
        wpas.request("DISCONNECT")
        wpas.wait_disconnected()

        lchannel = "1" if channel != "1" else "6"
        lfreq3 = "2412" if channel != "1" else "2437"
        if "OK" not in wpas.global_request("P2P_SET listen_channel " + lchannel):
            raise Exception("Failed to set listen channel")

        wpas.select_network(id)
        wpas.wait_connected()
        wpas.p2p_listen()

        if "OK" not in dev[0].global_request("P2P_FLUSH"):
            raise Exception("P2P_FLUSH failed")
        if not dev[0].discover_peer(addr5):
            raise Exception("Could not discover peer")
        peer = dev[0].get_peer(addr5)
        lfreq4 = peer['listen_freq']
        if lfreq4 != lfreq3:
            raise Exception("Unexpected Listen channel after configuration")
        wpas.p2p_stop_find()
        dev[0].p2p_stop_find()
    finally:
        wpas.global_request("SET p2p_optimize_listen_chan 0")

def test_p2p_channel_5ghz_only(dev):
    """P2P GO start with only 5 GHz band allowed"""
    try:
        set_country("US", dev[0])
        dev[0].global_request("P2P_SET disallow_freq 2400-2500")
        res = autogo(dev[0])
        freq = int(res['freq'])
        if freq < 5000:
            raise Exception("Unexpected channel %d MHz" % freq)
        dev[0].remove_group()
    finally:
        dev[0].global_request("P2P_SET disallow_freq ")
        clear_regdom_dev(dev)

def test_p2p_channel_5ghz_165_169_us(dev):
    """P2P GO and 5 GHz channels 165 (allowed) and 169 (disallowed) in US"""
    try:
        set_country("US", dev[0])
        res = dev[0].p2p_start_go(freq=5825)
        if res['freq'] != "5825":
            raise Exception("Unexpected frequency: " + res['freq'])
        dev[0].remove_group()

        res = dev[0].global_request("P2P_GROUP_ADD freq=5845")
        if "FAIL" not in res:
            raise Exception("GO on channel 169 allowed unexpectedly")
    finally:
        clear_regdom_dev(dev)

def wait_go_down_up(dev):
    ev = dev.wait_group_event(["AP-DISABLED"], timeout=5)
    if ev is None:
        raise Exception("AP-DISABLED not seen after P2P-REMOVE-AND-REFORM-GROUP")
    ev = dev.wait_group_event(["AP-ENABLED"], timeout=5)
    if ev is None:
        raise Exception("AP-ENABLED not seen after P2P-REMOVE-AND-REFORM-GROUP")

def test_p2p_go_move_reg_change(dev, apdev):
    """P2P GO move due to regulatory change"""
    try:
        set_country("US")
        dev[0].global_request("P2P_SET disallow_freq 2400-5000,5700-6000")
        res = autogo(dev[0])
        freq1 = int(res['freq'])
        if freq1 < 5000:
            raise Exception("Unexpected channel %d MHz" % freq1)
        dev[0].dump_monitor()

        dev[0].global_request("P2P_SET disallow_freq ")

        # GO move is not allowed while waiting for initial client connection
        connect_cli(dev[0], dev[1], freq=freq1)
        dev[1].remove_group()
        ev = dev[1].wait_global_event(["P2P-GROUP-REMOVED"], timeout=5)
        if ev is None:
            raise Exception("P2P-GROUP-REMOVED not reported on client")
        dev[1].dump_monitor()
        dev[0].dump_monitor()

        freq = dev[0].get_group_status_field('freq')
        if int(freq) < 5000:
            raise Exception("Unexpected freq after initial client: " + freq)
        dev[0].dump_monitor()

        dev[0].request("NOTE Setting country=BD")
        set_country("BD")
        dev[0].request("NOTE Waiting for GO channel change")
        ev = dev[0].wait_group_event(["P2P-REMOVE-AND-REFORM-GROUP",
                                      "AP-CSA-FINISHED"],
                                     timeout=10)
        if ev is None:
            raise Exception("P2P-REMOVE-AND-REFORM-GROUP or AP-CSA-FINISHED not seen")
        if "P2P-REMOVE-AND-REFORM-GROUP" in ev:
            wait_go_down_up(dev[0])

        freq2 = dev[0].get_group_status_field('freq')
        if freq1 == freq2:
            raise Exception("Unexpected freq after group reform=" + freq2)

        dev[0].remove_group()
    finally:
        dev[0].global_request("P2P_SET disallow_freq ")
        set_country("00")

def test_p2p_go_move_active(dev, apdev):
    """P2P GO stays in freq although SCM is possible"""
    with HWSimRadio(n_channels=2) as (radio, iface):
        wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
        wpas.interface_add(iface)

        if wpas.get_mcc() < 2:
            raise Exception("New radio does not support MCC")

        ndev = [wpas, dev[1]]
        _test_p2p_go_move_active(ndev, apdev)

def _test_p2p_go_move_active(dev, apdev):
    dev[0].global_request("SET p2p_no_group_iface 0")
    try:
        dev[0].global_request("P2P_SET disallow_freq 2430-6000")
        hapd = hostapd.add_ap(apdev[0], {"ssid": 'ap-test',
                                         "channel": '11'})
        dev[0].connect("ap-test", key_mgmt="NONE",
                       scan_freq="2462")

        res = autogo(dev[0])
        freq = int(res['freq'])
        if freq > 2430:
            raise Exception("Unexpected channel %d MHz" % freq)

        # GO move is not allowed while waiting for initial client connection
        connect_cli(dev[0], dev[1], freq=freq)
        dev[1].remove_group()

        freq = dev[0].get_group_status_field('freq')
        if int(freq) > 2430:
            raise Exception("Unexpected freq after initial client: " + freq)

        dev[0].dump_monitor()
        dev[0].global_request("P2P_SET disallow_freq ")

        ev = dev[0].wait_group_event(["P2P-REMOVE-AND-REFORM-GROUP",
                                      "AP-CSA-FINISHED"],
                                     timeout=10)
        if ev is not None:
            raise Exception("Unexpected P2P-REMOVE-AND-REFORM-GROUP or AP-CSA-FINISHED seen")

        dev[0].remove_group()
    finally:
        dev[0].global_request("P2P_SET disallow_freq ")

def test_p2p_go_move_scm(dev, apdev):
    """P2P GO move due to SCM operation preference"""
    with HWSimRadio(n_channels=2) as (radio, iface):
        wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
        wpas.interface_add(iface)

        if wpas.get_mcc() < 2:
            raise Exception("New radio does not support MCC")

        ndev = [wpas, dev[1]]
        _test_p2p_go_move_scm(ndev, apdev)

def _test_p2p_go_move_scm(dev, apdev):
    dev[0].global_request("SET p2p_no_group_iface 0")
    try:
        dev[0].global_request("P2P_SET disallow_freq 2430-6000")
        hapd = hostapd.add_ap(apdev[0], {"ssid": 'ap-test',
                                         "channel": '11'})
        dev[0].connect("ap-test", key_mgmt="NONE",
                       scan_freq="2462")

        dev[0].global_request("SET p2p_go_freq_change_policy 0")
        res = autogo(dev[0])
        freq = int(res['freq'])
        if freq > 2430:
            raise Exception("Unexpected channel %d MHz" % freq)

        # GO move is not allowed while waiting for initial client connection
        connect_cli(dev[0], dev[1], freq=freq)
        dev[1].remove_group()

        freq = dev[0].get_group_status_field('freq')
        if int(freq) > 2430:
            raise Exception("Unexpected freq after initial client: " + freq)

        dev[0].dump_monitor()
        dev[0].global_request("P2P_SET disallow_freq ")

        ev = dev[0].wait_group_event(["P2P-REMOVE-AND-REFORM-GROUP",
                                      "AP-CSA-FINISHED"], timeout=3)
        if ev is None:
            raise Exception("P2P-REMOVE-AND-REFORM-GROUP or AP-CSA-FINISHED not seen")
        if "P2P-REMOVE-AND-REFORM-GROUP" in ev:
            wait_go_down_up(dev[0])

        freq = dev[0].get_group_status_field('freq')
        if freq != '2462':
            raise Exception("Unexpected freq after group reform=" + freq)

        dev[0].remove_group()
    finally:
        dev[0].global_request("P2P_SET disallow_freq ")
        dev[0].global_request("SET p2p_go_freq_change_policy 2")

def test_p2p_go_move_scm_peer_supports(dev, apdev):
    """P2P GO move due to SCM operation preference (peer supports)"""
    with HWSimRadio(n_channels=2) as (radio, iface):
        wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
        wpas.interface_add(iface)

        if wpas.get_mcc() < 2:
            raise Exception("New radio does not support MCC")

        ndev = [wpas, dev[1]]
        _test_p2p_go_move_scm_peer_supports(ndev, apdev)

def _test_p2p_go_move_scm_peer_supports(dev, apdev):
    try:
        dev[0].global_request("SET p2p_go_freq_change_policy 1")
        set_country("US", dev[0])

        dev[0].global_request("SET p2p_no_group_iface 0")
        [i_res, r_res] = go_neg_pin_authorized(i_dev=dev[0], i_intent=15,
                                               r_dev=dev[1], r_intent=0,
                                               test_data=False)
        check_grpform_results(i_res, r_res)
        freq = int(i_res['freq'])
        if freq < 5000:
            raise Exception("Unexpected channel %d MHz - did not follow 5 GHz preference" % freq)

        hapd = hostapd.add_ap(apdev[0], {"ssid": 'ap-test',
                                         "channel": '11'})
        logger.info('Connecting client to to an AP on channel 11')
        dev[0].connect("ap-test", key_mgmt="NONE",
                       scan_freq="2462")

        ev = dev[0].wait_group_event(["P2P-REMOVE-AND-REFORM-GROUP",
                                      "AP-CSA-FINISHED"], timeout=3)
        if ev is None:
            raise Exception("P2P-REMOVE-AND-REFORM-GROUP or AP-CSA-FINISHED not seen")
        if "P2P-REMOVE-AND-REFORM-GROUP" in ev:
            wait_go_down_up(dev[0])

        freq = dev[0].get_group_status_field('freq')
        if freq != '2462':
            raise Exception("Unexpected freq after group reform=" + freq)

        dev[0].remove_group()
    finally:
        dev[0].global_request("SET p2p_go_freq_change_policy 2")
        disable_hapd(hapd)
        clear_regdom_dev(dev, 1)

def test_p2p_go_move_scm_peer_does_not_support(dev, apdev):
    """No P2P GO move due to SCM operation (peer does not supports)"""
    with HWSimRadio(n_channels=2) as (radio, iface):
        wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
        wpas.interface_add(iface)

        if wpas.get_mcc() < 2:
            raise Exception("New radio does not support MCC")

        ndev = [wpas, dev[1]]
        _test_p2p_go_move_scm_peer_does_not_support(ndev, apdev)

def _test_p2p_go_move_scm_peer_does_not_support(dev, apdev):
    try:
        dev[0].global_request("SET p2p_go_freq_change_policy 1")
        set_country("US", dev[0])

        dev[0].global_request("SET p2p_no_group_iface 0")
        if "OK" not in dev[1].request("DRIVER_EVENT AVOID_FREQUENCIES 2400-2500"):
            raise Exception("Could not simulate driver event")
        [i_res, r_res] = go_neg_pin_authorized(i_dev=dev[0], i_intent=15,
                                               r_dev=dev[1], r_intent=0,
                                               test_data=False)
        check_grpform_results(i_res, r_res)
        freq = int(i_res['freq'])
        if freq < 5000:
            raise Exception("Unexpected channel %d MHz - did not follow 5 GHz preference" % freq)

        hapd = hostapd.add_ap(apdev[0], {"ssid": 'ap-test',
                                         "channel": '11'})
        logger.info('Connecting client to to an AP on channel 11')
        dev[0].connect("ap-test", key_mgmt="NONE",
                       scan_freq="2462")

        ev = dev[0].wait_group_event(["P2P-REMOVE-AND-REFORM-GROUP",
                                      "AP-CSA-FINISHED"],
                                     timeout=10)
        if ev is not None:
            raise Exception("Unexpected P2P-REMOVE-AND-REFORM-GROUP or AP-CSA-FINISHED seen")

        dev[0].remove_group()
    finally:
        dev[0].global_request("SET p2p_go_freq_change_policy 2")
        dev[1].request("DRIVER_EVENT AVOID_FREQUENCIES")
        disable_hapd(hapd)
        clear_regdom_dev(dev, 2)

def test_p2p_go_move_scm_multi(dev, apdev):
    """P2P GO move due to SCM operation preference multiple times"""
    with HWSimRadio(n_channels=2) as (radio, iface):
        wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
        wpas.interface_add(iface)

        if wpas.get_mcc() < 2:
            raise Exception("New radio does not support MCC")

        ndev = [wpas, dev[1]]
        _test_p2p_go_move_scm_multi(ndev, apdev)

def _test_p2p_go_move_scm_multi(dev, apdev):
    dev[0].request("SET p2p_no_group_iface 0")
    try:
        dev[0].global_request("P2P_SET disallow_freq 2430-6000")
        hapd = hostapd.add_ap(apdev[0], {"ssid": 'ap-test-1',
                                         "channel": '11'})
        dev[0].connect("ap-test-1", key_mgmt="NONE",
                       scan_freq="2462")

        dev[0].global_request("SET p2p_go_freq_change_policy 0")
        res = autogo(dev[0])
        freq = int(res['freq'])
        if freq > 2430:
            raise Exception("Unexpected channel %d MHz" % freq)

        # GO move is not allowed while waiting for initial client connection
        connect_cli(dev[0], dev[1], freq=freq)
        dev[1].remove_group()

        freq = dev[0].get_group_status_field('freq')
        if int(freq) > 2430:
            raise Exception("Unexpected freq after initial client: " + freq)

        dev[0].dump_monitor()
        dev[0].global_request("P2P_SET disallow_freq ")

        ev = dev[0].wait_group_event(["P2P-REMOVE-AND-REFORM-GROUP",
                                      "AP-CSA-FINISHED"], timeout=3)
        if ev is None:
            raise Exception("P2P-REMOVE-AND-REFORM-GROUP or AP-CSA-FINISHED not seen")
        if "P2P-REMOVE-AND-REFORM-GROUP" in ev:
            wait_go_down_up(dev[0])

        freq = dev[0].get_group_status_field('freq')
        if freq != '2462':
            raise Exception("Unexpected freq after group reform=" + freq)

        hapd = hostapd.add_ap(apdev[0], {"ssid": 'ap-test-2',
                                         "channel": '6'})
        dev[0].connect("ap-test-2", key_mgmt="NONE",
                       scan_freq="2437")

        ev = dev[0].wait_group_event(["P2P-REMOVE-AND-REFORM-GROUP",
                                      "AP-CSA-FINISHED"], timeout=5)
        if ev is None:
            raise Exception("(2) P2P-REMOVE-AND-REFORM-GROUP or AP-CSA-FINISHED not seen")
        if "P2P-REMOVE-AND-REFORM-GROUP" in ev:
            wait_go_down_up(dev[0])

        freq = dev[0].get_group_status_field('freq')
        if freq != '2437':
            raise Exception("(2) Unexpected freq after group reform=" + freq)

        dev[0].remove_group()
    finally:
        dev[0].global_request("P2P_SET disallow_freq ")
        dev[0].global_request("SET p2p_go_freq_change_policy 2")

def test_p2p_delay_go_csa(dev, apdev, params):
    """P2P GO CSA delayed when inviting a P2P Device to an active P2P Group"""
    with HWSimRadio(n_channels=2) as (radio, iface):
        wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
        wpas.interface_add(iface)

        wpas.global_request("SET p2p_no_group_iface 0")

        if wpas.get_mcc() < 2:
           raise Exception("New radio does not support MCC")

        addr0 = wpas.p2p_dev_addr()
        addr1 = dev[1].p2p_dev_addr()

        try:
            dev[1].p2p_listen()
            if not wpas.discover_peer(addr1, social=True):
                raise Exception("Peer " + addr1 + " not found")
            wpas.p2p_stop_find()

            hapd = hostapd.add_ap(apdev[0], {"ssid": 'bss-2.4ghz',
                                             "channel": '1'})

            wpas.connect("bss-2.4ghz", key_mgmt="NONE", scan_freq="2412")

            wpas.global_request("SET p2p_go_freq_change_policy 0")
            wpas.dump_monitor()

            logger.info("Start GO on channel 6")
            res = autogo(wpas, freq=2437)
            if res['freq'] != "2437":
               raise Exception("GO set on a freq=%s instead of 2437" % res['freq'])

            # Start find on dev[1] to run scans with dev[2] in parallel
            dev[1].p2p_find(social=True)

            # Use another client device to stop the initial client connection
            # timeout on the GO
            if not dev[2].discover_peer(addr0, social=True):
                raise Exception("Peer2 did not find the GO")
            dev[2].p2p_stop_find()
            pin = dev[2].wps_read_pin()
            wpas.p2p_go_authorize_client(pin)
            dev[2].global_request("P2P_CONNECT " + addr0 + " " + pin + " join freq=2437")
            ev = dev[2].wait_global_event(["P2P-GROUP-STARTED"], timeout=10)
            if ev is None:
                raise Exception("Peer2 did not get connected")

            if not dev[1].discover_peer(addr0, social=True):
                raise Exception("Peer did not find the GO")

            pin = dev[1].wps_read_pin()
            dev[1].global_request("P2P_CONNECT " + addr0 + " " + pin + " join auth")
            dev[1].p2p_listen()

            # Force P2P GO channel switch on successful invitation signaling
            wpas.group_request("SET p2p_go_csa_on_inv 1")

            logger.info("Starting invitation")
            wpas.p2p_go_authorize_client(pin)
            wpas.global_request("P2P_INVITE group=" + wpas.group_ifname + " peer=" + addr1)
            ev = dev[1].wait_global_event(["P2P-INVITATION-RECEIVED",
                                           "P2P-GROUP-STARTED"], timeout=10)

            if ev is None:
                raise Exception("Timeout on invitation on peer")
            if "P2P-INVITATION-RECEIVED" in ev:
                raise Exception("Unexpected request to accept pre-authorized invitation")

            # A P2P GO move is not expected at this stage, as during the
            # invitation signaling, the P2P GO includes only its current
            # operating channel in the channel list, and as the invitation
            # response can only include channels that were also in the
            # invitation request channel list, the group common channels
            # includes only the current P2P GO operating channel.
            ev = wpas.wait_group_event(["P2P-REMOVE-AND-REFORM-GROUP",
                                        "AP-CSA-FINISHED"], timeout=1)
            if ev is not None:
                raise Exception("Unexpected + " + ev + " event")

        finally:
            wpas.global_request("SET p2p_go_freq_change_policy 2")

def test_p2p_channel_vht80(dev):
    """P2P group formation with VHT 80 MHz"""
    try:
        set_country("FI", dev[0])
        [i_res, r_res] = go_neg_pin_authorized(i_dev=dev[0], i_intent=15,
                                               i_freq=5180,
                                               i_max_oper_chwidth=80,
                                               i_ht40=True, i_vht=True,
                                               r_dev=dev[1], r_intent=0,
                                               test_data=False)
        check_grpform_results(i_res, r_res)
        freq = int(i_res['freq'])
        if freq < 5000:
            raise Exception("Unexpected channel %d MHz - did not follow 5 GHz preference" % freq)
        sig = dev[1].group_request("SIGNAL_POLL").splitlines()
        if "FREQUENCY=5180" not in sig:
            raise Exception("Unexpected SIGNAL_POLL value(1): " + str(sig))
        if "WIDTH=80 MHz" not in sig:
            raise Exception("Unexpected SIGNAL_POLL value(2): " + str(sig))
        remove_group(dev[0], dev[1])
    finally:
        set_country("00")
        dev[1].flush_scan_cache()

def test_p2p_channel_vht80p80(dev):
    """P2P group formation and VHT 80+80 MHz channel"""
    try:
        set_country("US", dev[0])
        [i_res, r_res] = go_neg_pin_authorized(i_dev=dev[0], i_intent=15,
                                               i_freq=5180,
                                               i_freq2=5775,
                                               i_max_oper_chwidth=160,
                                               i_ht40=True, i_vht=True,
                                               r_dev=dev[1], r_intent=0,
                                               test_data=False)
        check_grpform_results(i_res, r_res)
        freq = int(i_res['freq'])
        if freq < 5000:
            raise Exception("Unexpected channel %d MHz - did not follow 5 GHz preference" % freq)
        sig = dev[1].group_request("SIGNAL_POLL").splitlines()
        if "FREQUENCY=5180" not in sig:
            raise Exception("Unexpected SIGNAL_POLL value(1): " + str(sig))
        if "WIDTH=80+80 MHz" not in sig:
            raise Exception("Unexpected SIGNAL_POLL value(2): " + str(sig))
        if "CENTER_FRQ1=5210" not in sig:
            raise Exception("Unexpected SIGNAL_POLL value(3): " + str(sig))
        if "CENTER_FRQ2=5775" not in sig:
            raise Exception("Unexpected SIGNAL_POLL value(4): " + str(sig))
        remove_group(dev[0], dev[1])
    finally:
        set_country("00")
        dev[1].flush_scan_cache()

def test_p2p_channel_vht80p80_autogo(dev):
    """P2P autonomous GO and VHT 80+80 MHz channel"""
    addr0 = dev[0].p2p_dev_addr()

    try:
        set_country("US", dev[0])
        if "OK" not in dev[0].global_request("P2P_GROUP_ADD vht freq=5180 freq2=5775"):
            raise Exception("Could not start GO")
        ev = dev[0].wait_global_event(["P2P-GROUP-STARTED"], timeout=5)
        if ev is None:
            raise Exception("GO start up timed out")
        dev[0].group_form_result(ev)

        pin = dev[1].wps_read_pin()
        dev[0].p2p_go_authorize_client(pin)

        dev[1].global_request("P2P_CONNECT " + addr0 + " " + pin + " join freq=5180")
        ev = dev[1].wait_global_event(["P2P-GROUP-STARTED"], timeout=10)
        if ev is None:
            raise Exception("Peer did not get connected")

        dev[1].group_form_result(ev)
        sig = dev[1].group_request("SIGNAL_POLL").splitlines()
        if "FREQUENCY=5180" not in sig:
            raise Exception("Unexpected SIGNAL_POLL value(1): " + str(sig))
        if "WIDTH=80+80 MHz" not in sig:
            raise Exception("Unexpected SIGNAL_POLL value(2): " + str(sig))
        if "CENTER_FRQ1=5210" not in sig:
            raise Exception("Unexpected SIGNAL_POLL value(3): " + str(sig))
        if "CENTER_FRQ2=5775" not in sig:
            raise Exception("Unexpected SIGNAL_POLL value(4): " + str(sig))
        remove_group(dev[0], dev[1])
    finally:
        set_country("00")
        dev[1].flush_scan_cache()

def test_p2p_channel_vht80_autogo(dev):
    """P2P autonomous GO and VHT 80 MHz channel"""
    addr0 = dev[0].p2p_dev_addr()

    try:
        set_country("US", dev[0])
        if "OK" not in dev[0].global_request("P2P_GROUP_ADD vht freq=5180 max_oper_chwidth=80"):
            raise Exception("Could not start GO")
        ev = dev[0].wait_global_event(["P2P-GROUP-STARTED"], timeout=5)
        if ev is None:
            raise Exception("GO start up timed out")
        dev[0].group_form_result(ev)

        pin = dev[1].wps_read_pin()
        dev[0].p2p_go_authorize_client(pin)

        dev[1].global_request("P2P_CONNECT " + addr0 + " " + pin + " join freq=5180")
        ev = dev[1].wait_global_event(["P2P-GROUP-STARTED"], timeout=10)
        if ev is None:
            raise Exception("Peer did not get connected")

        dev[1].group_form_result(ev)
        sig = dev[1].group_request("SIGNAL_POLL").splitlines()
        if "FREQUENCY=5180" not in sig:
            raise Exception("Unexpected SIGNAL_POLL value(1): " + str(sig))
        if "WIDTH=80 MHz" not in sig:
            raise Exception("Unexpected SIGNAL_POLL value(2): " + str(sig))
        remove_group(dev[0], dev[1])
    finally:
        set_country("00")
        dev[1].flush_scan_cache()

def test_p2p_channel_vht80p80_persistent(dev):
    """P2P persistent group re-invocation and VHT 80+80 MHz channel"""
    addr0 = dev[0].p2p_dev_addr()
    form(dev[0], dev[1])

    try:
        set_country("US", dev[0])
        invite(dev[0], dev[1], extra="vht freq=5745 freq2=5210")
        [go_res, cli_res] = check_result(dev[0], dev[1])

        sig = dev[1].group_request("SIGNAL_POLL").splitlines()
        if "FREQUENCY=5745" not in sig:
            raise Exception("Unexpected SIGNAL_POLL value(1): " + str(sig))
        if "WIDTH=80+80 MHz" not in sig:
            raise Exception("Unexpected SIGNAL_POLL value(2): " + str(sig))
        if "CENTER_FRQ1=5775" not in sig:
            raise Exception("Unexpected SIGNAL_POLL value(3): " + str(sig))
        if "CENTER_FRQ2=5210" not in sig:
            raise Exception("Unexpected SIGNAL_POLL value(4): " + str(sig))
        remove_group(dev[0], dev[1])
    finally:
        set_country("00")
        dev[1].flush_scan_cache()

def test_p2p_channel_drv_pref_go_neg(dev):
    """P2P GO Negotiation with GO device channel preference"""
    dev[0].global_request("SET get_pref_freq_list_override 3:2417 4:2422")
    [i_res, r_res] = go_neg_pin_authorized(i_dev=dev[0], i_intent=15,
                                           r_dev=dev[1], r_intent=0,
                                           test_data=False)
    check_grpform_results(i_res, r_res)
    freq = int(i_res['freq'])
    if freq != 2417:
        raise Exception("Unexpected channel selected: %d" % freq)
    remove_group(dev[0], dev[1])

def test_p2p_channel_drv_pref_go_neg2(dev):
    """P2P GO Negotiation with P2P client device channel preference"""
    dev[0].global_request("SET get_pref_freq_list_override 3:2417,2422")
    dev[1].global_request("SET get_pref_freq_list_override 4:2422")
    [i_res, r_res] = go_neg_pin_authorized(i_dev=dev[0], i_intent=15,
                                           r_dev=dev[1], r_intent=0,
                                           test_data=False)
    check_grpform_results(i_res, r_res)
    freq = int(i_res['freq'])
    if freq != 2422:
        raise Exception("Unexpected channel selected: %d" % freq)
    remove_group(dev[0], dev[1])

def test_p2p_channel_drv_pref_go_neg3(dev):
    """P2P GO Negotiation with GO device channel preference"""
    dev[1].global_request("SET get_pref_freq_list_override 3:2417,2427 4:2422")
    [i_res, r_res] = go_neg_pin_authorized(i_dev=dev[0], i_intent=0,
                                           r_dev=dev[1], r_intent=15,
                                           test_data=False)
    check_grpform_results(i_res, r_res)
    freq = int(i_res['freq'])
    if freq != 2417:
        raise Exception("Unexpected channel selected: %d" % freq)
    remove_group(dev[0], dev[1])

def test_p2p_channel_drv_pref_go_neg4(dev):
    """P2P GO Negotiation with P2P client device channel preference"""
    dev[0].global_request("SET get_pref_freq_list_override 3:2417,2422,5180")
    dev[1].global_request("P2P_SET override_pref_op_chan 115:36")
    [i_res, r_res] = go_neg_pin_authorized(i_dev=dev[0], i_intent=15,
                                           r_dev=dev[1], r_intent=0,
                                           test_data=False)
    check_grpform_results(i_res, r_res)
    freq = int(i_res['freq'])
    if freq != 2417:
        raise Exception("Unexpected channel selected: %d" % freq)
    remove_group(dev[0], dev[1])

def test_p2p_channel_drv_pref_go_neg5(dev):
    """P2P GO Negotiation with P2P client device channel preference"""
    dev[0].global_request("SET get_pref_freq_list_override 3:2417")
    dev[1].global_request("SET get_pref_freq_list_override 4:2422")
    dev[1].global_request("P2P_SET override_pref_op_chan 115:36")
    [i_res, r_res] = go_neg_pin_authorized(i_dev=dev[0], i_intent=15,
                                           r_dev=dev[1], r_intent=0,
                                           test_data=False)
    check_grpform_results(i_res, r_res)
    freq = int(i_res['freq'])
    if freq != 2417:
        raise Exception("Unexpected channel selected: %d" % freq)
    remove_group(dev[0], dev[1])

def test_p2p_channel_drv_pref_autogo(dev):
    """P2P autonomous GO with driver channel preference"""
    dev[0].global_request("SET get_pref_freq_list_override 3:2417,2422,5180")
    res_go = autogo(dev[0])
    if res_go['freq'] != "2417":
        raise Exception("Unexpected operating frequency: " + res_go['freq'])

def test_p2p_channel_disable_6ghz(dev):
    """P2P with 6 GHz disabled"""
    try:
        dev[0].global_request("SET p2p_6ghz_disable 1")
        dev[1].p2p_listen()
        dev[0].discover_peer(dev[1].p2p_dev_addr(), social=False)

        autogo(dev[1])
        connect_cli(dev[1], dev[0])
    finally:
        dev[0].global_request("SET p2p_6ghz_disable 0")
