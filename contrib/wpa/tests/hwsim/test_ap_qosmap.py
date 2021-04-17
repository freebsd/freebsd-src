# QoS Mapping tests
# Copyright (c) 2013-2016, Jouni Malinen <j@w1.fi>
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

from remotehost import remote_compatible
import time
import logging
logger = logging.getLogger()

import hwsim_utils
import hostapd
from utils import HwsimSkip, alloc_fail, fail_test
from wlantest import Wlantest

def check_qos_map(ap, hapd, dev, sta, dscp, tid, ap_tid=None):
    if not ap_tid:
        ap_tid = tid
    bssid = ap['bssid']
    wt = Wlantest()
    wt.clear_sta_counters(bssid, sta)
    hwsim_utils.test_connectivity(dev, hapd, dscp=dscp, config=False)
    sleep_time = 0.02 if dev.hostname is None else 0.2
    time.sleep(sleep_time)
    tx = wt.get_tx_tid(bssid, sta, tid)
    if tx == 0:
        [tx, rx] = wt.get_tid_counters(bssid, sta)
        logger.info("Expected TX DSCP " + str(dscp) + " with TID " + str(tid) + " but counters: " + str(tx))
        raise Exception("No STA->AP data frame using the expected TID")
    rx = wt.get_rx_tid(bssid, sta, ap_tid)
    if rx == 0:
        [tx, rx] = wt.get_tid_counters(bssid, sta)
        logger.info("Expected RX DSCP " + str(dscp) + " with TID " + str(ap_tid) + " but counters: " + str(rx))
        raise Exception("No AP->STA data frame using the expected TID")

@remote_compatible
def test_ap_qosmap(dev, apdev):
    """QoS mapping"""
    drv_flags = dev[0].get_driver_status_field("capa.flags")
    if int(drv_flags, 0) & 0x40000000 == 0:
        raise HwsimSkip("Driver does not support QoS Map")
    ssid = "test-qosmap"
    params = {"ssid": ssid}
    params['qos_map_set'] = '53,2,22,6,8,15,0,7,255,255,16,31,32,39,255,255,40,47,48,55'
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].connect(ssid, key_mgmt="NONE", scan_freq="2412")
    time.sleep(0.1)
    addr = dev[0].p2p_interface_addr()
    dev[0].request("DATA_TEST_CONFIG 1")
    hapd.request("DATA_TEST_CONFIG 1")
    Wlantest.setup(hapd)
    check_qos_map(apdev[0], hapd, dev[0], addr, 53, 2)
    check_qos_map(apdev[0], hapd, dev[0], addr, 22, 6)
    check_qos_map(apdev[0], hapd, dev[0], addr, 8, 0)
    check_qos_map(apdev[0], hapd, dev[0], addr, 15, 0)
    check_qos_map(apdev[0], hapd, dev[0], addr, 0, 1)
    check_qos_map(apdev[0], hapd, dev[0], addr, 7, 1)
    check_qos_map(apdev[0], hapd, dev[0], addr, 16, 3)
    check_qos_map(apdev[0], hapd, dev[0], addr, 31, 3)
    check_qos_map(apdev[0], hapd, dev[0], addr, 32, 4)
    check_qos_map(apdev[0], hapd, dev[0], addr, 39, 4)
    check_qos_map(apdev[0], hapd, dev[0], addr, 40, 6)
    check_qos_map(apdev[0], hapd, dev[0], addr, 47, 6)
    check_qos_map(apdev[0], hapd, dev[0], addr, 48, 7)
    check_qos_map(apdev[0], hapd, dev[0], addr, 55, 7)
    hapd.request("SET_QOS_MAP_SET 22,6,8,15,0,7,255,255,16,31,32,39,255,255,40,47,48,55")
    hapd.request("SEND_QOS_MAP_CONF " + dev[0].get_status_field("address"))
    check_qos_map(apdev[0], hapd, dev[0], addr, 53, 7)
    check_qos_map(apdev[0], hapd, dev[0], addr, 22, 6)
    check_qos_map(apdev[0], hapd, dev[0], addr, 48, 7)
    check_qos_map(apdev[0], hapd, dev[0], addr, 55, 7)
    check_qos_map(apdev[0], hapd, dev[0], addr, 56, 56 >> 3)
    check_qos_map(apdev[0], hapd, dev[0], addr, 63, 63 >> 3)
    dev[0].request("DATA_TEST_CONFIG 0")
    hapd.request("DATA_TEST_CONFIG 0")

@remote_compatible
def test_ap_qosmap_default(dev, apdev):
    """QoS mapping with default values"""
    ssid = "test-qosmap-default"
    params = {"ssid": ssid}
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].connect(ssid, key_mgmt="NONE", scan_freq="2412")
    addr = dev[0].p2p_interface_addr()
    dev[0].request("DATA_TEST_CONFIG 1")
    hapd.request("DATA_TEST_CONFIG 1")
    Wlantest.setup(hapd)
    for dscp in [0, 7, 8, 15, 16, 23, 24, 31, 32, 39, 40, 47, 48, 55, 56, 63]:
        check_qos_map(apdev[0], hapd, dev[0], addr, dscp, dscp >> 3)
    dev[0].request("DATA_TEST_CONFIG 0")
    hapd.request("DATA_TEST_CONFIG 0")

@remote_compatible
def test_ap_qosmap_default_acm(dev, apdev):
    """QoS mapping with default values and ACM=1 for VO/VI"""
    ssid = "test-qosmap-default"
    params = {"ssid": ssid,
              "wmm_ac_bk_aifs": "7",
              "wmm_ac_bk_cwmin": "4",
              "wmm_ac_bk_cwmax": "10",
              "wmm_ac_bk_txop_limit": "0",
              "wmm_ac_bk_acm": "0",
              "wmm_ac_be_aifs": "3",
              "wmm_ac_be_cwmin": "4",
              "wmm_ac_be_cwmax": "10",
              "wmm_ac_be_txop_limit": "0",
              "wmm_ac_be_acm": "0",
              "wmm_ac_vi_aifs": "2",
              "wmm_ac_vi_cwmin": "3",
              "wmm_ac_vi_cwmax": "4",
              "wmm_ac_vi_txop_limit": "94",
              "wmm_ac_vi_acm": "1",
              "wmm_ac_vo_aifs": "2",
              "wmm_ac_vo_cwmin": "2",
              "wmm_ac_vo_cwmax": "2",
              "wmm_ac_vo_txop_limit": "47",
              "wmm_ac_vo_acm": "1"}
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].connect(ssid, key_mgmt="NONE", scan_freq="2412")
    addr = dev[0].p2p_interface_addr()
    dev[0].request("DATA_TEST_CONFIG 1")
    hapd.request("DATA_TEST_CONFIG 1")
    Wlantest.setup(hapd)
    for dscp in [0, 7, 8, 15, 16, 23, 24, 31, 32, 39, 40, 47, 48, 55, 56, 63]:
        ap_tid = dscp >> 3
        tid = ap_tid
        # downgrade VI/VO to BE
        if tid in [4, 5, 6, 7]:
            tid = 3
        check_qos_map(apdev[0], hapd, dev[0], addr, dscp, tid, ap_tid)
    dev[0].request("DATA_TEST_CONFIG 0")
    hapd.request("DATA_TEST_CONFIG 0")

@remote_compatible
def test_ap_qosmap_invalid(dev, apdev):
    """QoS mapping ctrl_iface error handling"""
    ssid = "test-qosmap"
    params = {"ssid": ssid}
    hapd = hostapd.add_ap(apdev[0], params)
    if "FAIL" not in hapd.request("SEND_QOS_MAP_CONF 00:11:22:33:44:55"):
        raise Exception("Unexpected SEND_QOS_MAP_CONF success")
    if "FAIL" not in hapd.request("SET_QOS_MAP_SET "):
        raise Exception("Unexpected SET_QOS_MAP_SET success")
    if "FAIL" not in hapd.request("SET_QOS_MAP_SET 1,2,3"):
        raise Exception("Unexpected SET_QOS_MAP_SET success")
    if "FAIL" not in hapd.request("SET_QOS_MAP_SET 1,-2,3"):
        raise Exception("Unexpected SET_QOS_MAP_SET success")
    if "FAIL" not in hapd.request("SET_QOS_MAP_SET 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59"):
        raise Exception("Unexpected SET_QOS_MAP_SET success")
    if "FAIL" not in hapd.request("SET_QOS_MAP_SET 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21"):
        raise Exception("Unexpected SET_QOS_MAP_SET success")

    if "FAIL" in hapd.request("SET_QOS_MAP_SET 22,6,8,15,0,7,255,255,16,31,32,39,255,255,40,47,48,55"):
        raise Exception("Unexpected SET_QOS_MAP_SET failure")
    if "FAIL" not in hapd.request("SEND_QOS_MAP_CONF 00:11:22:33:44:55"):
        raise Exception("Unexpected SEND_QOS_MAP_CONF success")
    if "FAIL" not in hapd.request("SEND_QOS_MAP_CONF 00:11:22:33:44"):
        raise Exception("Unexpected SEND_QOS_MAP_CONF success")

    with fail_test(hapd, 1, "hostapd_ctrl_iface_set_qos_map_set"):
        if "FAIL" not in hapd.request("SET_QOS_MAP_SET 22,6,8,15,0,7,255,255,16,31,32,39,255,255,40,47,48,55"):
            raise Exception("SET_QOS_MAP_SET accepted during forced driver failure")

    dev[0].connect(ssid, key_mgmt="NONE", scan_freq="2412")
    with alloc_fail(hapd, 1,
                    "wpabuf_alloc;hostapd_ctrl_iface_send_qos_map_conf"):
        if "FAIL" not in hapd.request("SEND_QOS_MAP_CONF " + dev[0].own_addr()):
            raise Exception("SEND_QOS_MAP_CONF accepted during OOM")
