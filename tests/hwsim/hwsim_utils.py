# hwsim testing utilities
# Copyright (c) 2013-2014, Jouni Malinen <j@w1.fi>
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

import os
import time
import logging
logger = logging.getLogger()

from wpasupplicant import WpaSupplicant

def config_data_test(dev1, dev2, dev1group, dev2group, ifname1, ifname2):
    cmd = "DATA_TEST_CONFIG 1"
    if ifname1:
        cmd = cmd + " ifname=" + ifname1
    if dev1group:
        res = dev1.group_request(cmd)
    else:
        res = dev1.request(cmd)
    if "OK" not in res:
        raise Exception("Failed to enable data test functionality")

    cmd = "DATA_TEST_CONFIG 1"
    if ifname2:
        cmd = cmd + " ifname=" + ifname2
    if dev2group:
        res = dev2.group_request(cmd)
    else:
        res = dev2.request(cmd)
    if "OK" not in res:
        raise Exception("Failed to enable data test functionality")

def run_multicast_connectivity_test(dev1, dev2, tos=None,
                                    dev1group=False, dev2group=False,
                                    ifname1=None, ifname2=None,
                                    config=True, timeout=5,
                                    send_len=None, multicast_to_unicast=False,
                                    broadcast_retry_c=1):
    addr1 = dev1.get_addr(dev1group)
    addr2 = dev2.get_addr(dev2group)

    if config:
        config_data_test(dev1, dev2, dev1group, dev2group, ifname1, ifname2)

    cmd = "DATA_TEST_TX ff:ff:ff:ff:ff:ff {} {}".format(addr1, tos)
    if send_len is not None:
        cmd += " len=" + str(send_len)
    for i in range(broadcast_retry_c):
        try:
            if dev1group:
                dev1.group_request(cmd)
            else:
                dev1.request(cmd)
            if dev2group:
                ev = dev2.wait_group_event(["DATA-TEST-RX"],
                                           timeout=timeout)
            else:
                ev = dev2.wait_event(["DATA-TEST-RX"], timeout=timeout)
            if ev is None:
                raise Exception("dev1->dev2 broadcast data delivery failed")
            if multicast_to_unicast:
                if "DATA-TEST-RX ff:ff:ff:ff:ff:ff {}".format(addr1) in ev:
                    raise Exception("Unexpected dev1->dev2 broadcast data result: multicast to unicast conversion missing")
                if "DATA-TEST-RX {} {}".format(addr2, addr1) not in ev:
                    raise Exception("Unexpected dev1->dev2 broadcast data result (multicast to unicast enabled)")
            else:
                if "DATA-TEST-RX ff:ff:ff:ff:ff:ff {}".format(addr1) not in ev:
                    raise Exception("Unexpected dev1->dev2 broadcast data result")
            if send_len is not None:
                if " len=" + str(send_len) not in ev:
                    raise Exception("Unexpected dev1->dev2 broadcast data length")
            else:
                if " len=" in ev:
                    raise Exception("Unexpected dev1->dev2 broadcast data length")
            break
        except Exception as e:
            if i == broadcast_retry_c - 1:
                raise

def run_connectivity_test(dev1, dev2, tos, dev1group=False, dev2group=False,
                          ifname1=None, ifname2=None, config=True, timeout=5,
                          multicast_to_unicast=False, broadcast=True,
                          send_len=None):
    addr1 = dev1.get_addr(dev1group)
    addr2 = dev2.get_addr(dev2group)

    dev1.dump_monitor()
    dev2.dump_monitor()

    if dev1.hostname is None and dev2.hostname is None:
        broadcast_retry_c = 1
    else:
        broadcast_retry_c = 10

    try:
        if config:
            config_data_test(dev1, dev2, dev1group, dev2group, ifname1, ifname2)

        cmd = "DATA_TEST_TX {} {} {}".format(addr2, addr1, tos)
        if send_len is not None:
            cmd += " len=" + str(send_len)
        if dev1group:
            dev1.group_request(cmd)
        else:
            dev1.request(cmd)
        if dev2group:
            ev = dev2.wait_group_event(["DATA-TEST-RX"], timeout=timeout)
        else:
            ev = dev2.wait_event(["DATA-TEST-RX"], timeout=timeout)
        if ev is None:
            raise Exception("dev1->dev2 unicast data delivery failed")
        if "DATA-TEST-RX {} {}".format(addr2, addr1) not in ev:
            raise Exception("Unexpected dev1->dev2 unicast data result")
        if send_len is not None:
            if " len=" + str(send_len) not in ev:
                raise Exception("Unexpected dev1->dev2 unicast data length")
        else:
            if " len=" in ev:
                raise Exception("Unexpected dev1->dev2 unicast data length")

        if broadcast:
            run_multicast_connectivity_test(dev1, dev2, tos,
                                            dev1group, dev2group,
                                            ifname1, ifname2, False, timeout,
                                            send_len, False, broadcast_retry_c)

        cmd = "DATA_TEST_TX {} {} {}".format(addr1, addr2, tos)
        if send_len is not None:
            cmd += " len=" + str(send_len)
        if dev2group:
            dev2.group_request(cmd)
        else:
            dev2.request(cmd)
        if dev1group:
            ev = dev1.wait_group_event(["DATA-TEST-RX"], timeout=timeout)
        else:
            ev = dev1.wait_event(["DATA-TEST-RX"], timeout=timeout)
        if ev is None:
            raise Exception("dev2->dev1 unicast data delivery failed")
        if "DATA-TEST-RX {} {}".format(addr1, addr2) not in ev:
            raise Exception("Unexpected dev2->dev1 unicast data result")
        if send_len is not None:
            if " len=" + str(send_len) not in ev:
                raise Exception("Unexpected dev2->dev1 unicast data length")
        else:
            if " len=" in ev:
                raise Exception("Unexpected dev2->dev1 unicast data length")

        if broadcast:
            run_multicast_connectivity_test(dev2, dev1, tos,
                                            dev2group, dev1group,
                                            ifname2, ifname1, False, timeout,
                                            send_len, multicast_to_unicast,
                                            broadcast_retry_c)

    finally:
        if config:
            if dev1group:
                dev1.group_request("DATA_TEST_CONFIG 0")
            else:
                dev1.request("DATA_TEST_CONFIG 0")
            if dev2group:
                dev2.group_request("DATA_TEST_CONFIG 0")
            else:
                dev2.request("DATA_TEST_CONFIG 0")

def test_connectivity(dev1, dev2, dscp=None, tos=None, max_tries=1,
                      dev1group=False, dev2group=False,
                      ifname1=None, ifname2=None, config=True, timeout=5,
                      multicast_to_unicast=False, success_expected=True,
                      broadcast=True, send_len=None):
    if dscp:
        tos = dscp << 2
    if not tos:
        tos = 0

    success = False
    last_err = None
    for i in range(0, max_tries):
        try:
            run_connectivity_test(dev1, dev2, tos, dev1group, dev2group,
                                  ifname1, ifname2, config=config,
                                  timeout=timeout,
                                  multicast_to_unicast=multicast_to_unicast,
                                  broadcast=broadcast, send_len=send_len)
            success = True
            break
        except Exception as e:
            last_err = e
            if i + 1 < max_tries:
                time.sleep(1)
    if success_expected and not success:
        raise Exception(last_err)
    if not success_expected and success:
        raise Exception("Unexpected connectivity detected")

def test_connectivity_iface(dev1, dev2, ifname, dscp=None, tos=None,
                            max_tries=1, timeout=5):
    test_connectivity(dev1, dev2, dscp, tos, ifname2=ifname,
                      max_tries=max_tries, timeout=timeout)

def test_connectivity_p2p(dev1, dev2, dscp=None, tos=None):
    test_connectivity(dev1, dev2, dscp, tos, dev1group=True, dev2group=True)

def test_connectivity_p2p_sta(dev1, dev2, dscp=None, tos=None):
    test_connectivity(dev1, dev2, dscp, tos, dev1group=True, dev2group=False)

def test_connectivity_sta(dev1, dev2, dscp=None, tos=None):
    test_connectivity(dev1, dev2, dscp, tos)

(PS_DISABLED, PS_ENABLED, PS_AUTO_POLL, PS_MANUAL_POLL) = list(range(4))

def set_powersave(dev, val):
    phy = dev.get_driver_status_field("phyname")
    fname = '/sys/kernel/debug/ieee80211/%s/hwsim/ps' % phy
    data = '%d' % val
    (res, data) = dev.cmd_execute(["echo", data, ">", fname], shell=True)
    if res != 0:
        raise Exception("Failed to set power save for device")

def set_group_map(dev, val):
    phy = dev.get_driver_status_field("phyname")
    fname = '/sys/kernel/debug/ieee80211/%s/hwsim/group' % phy
    data = '%d' % val
    (res, data) = dev.cmd_execute(["echo", data, ">", fname], shell=True)
    if res != 0:
        raise Exception("Failed to set group map for %s" % phy)

def set_rx_rssi(dev, val):
    """
    Configure signal strength when receiving transmitted frames.
    mac80211_hwsim driver sets rssi to: TX power - 50
    According to that set tx_power in order to get the desired RSSI.
    Valid RSSI range: -50 to -30.
    """
    tx_power = (val + 50) * 100
    ifname = dev.get_driver_status_field("ifname")
    (res, data) = dev.cmd_execute(['iw', ifname, 'set', 'txpower',
                                   'fixed', str(tx_power)])
    if res != 0:
        raise Exception("Failed to set RSSI to %d" % val)

def reset_rx_rssi(dev):
    set_rx_rssi(dev, -30)
