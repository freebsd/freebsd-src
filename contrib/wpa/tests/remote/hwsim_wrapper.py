# Hwsim wrapper
# Copyright (c) 2016, Tieto Corporation
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

import remotehost
from wpasupplicant import WpaSupplicant
import hostapd
import config
import rutils
import monitor
import traceback
import wlantest

import logging
logger = logging.getLogger()

def run_hwsim_test(devices, setup_params, refs, duts, monitors, hwsim_test):
    try:
        ref_hosts = []
        dut_hosts = []
        dev = []
        apdev = []

        # get hosts
        for ref in refs:
            ref_host = rutils.get_host(devices, ref)
            ref_hosts.append(ref_host)
        for dut in duts:
            dut_host = rutils.get_host(devices, dut)
            dut_hosts.append(dut_host)

        # setup log dir
        local_log_dir = setup_params['local_log_dir']

        # setup hw before test
        rutils.setup_hw(ref_hosts, setup_params)
        rutils.setup_hw(dut_hosts, setup_params)

        # run monitors if requested/possible
        for ref_host in ref_hosts:
            monitor.add(ref_host, monitors)
            monitor.run(ref_host, setup_params)
        for dut_host in dut_hosts:
            monitor.add(dut_host, monitors)
            monitor.run(dut_host, setup_params)

        monitor_hosts = monitor.create(devices, setup_params, refs, duts,
                                       monitors)
        mon = None
        if len(monitor_hosts) > 0:
            mon = monitor_hosts[0]
            wlantest.Wlantest.reset_remote_wlantest()
            wlantest.Wlantest.register_remote_wlantest(mon, setup_params,
                                                       monitor)

        # run hostapd/wpa_supplicant
        for ref_host in ref_hosts:
            rutils.run_wpasupplicant(ref_host, setup_params)
            wpas = WpaSupplicant(hostname=ref_host.host, global_iface="udp",
                                 global_port=ref_host.port)
            wpas.interface_add(ref_host.ifname)
            dev.append(wpas)
        for dut_host in dut_hosts:
            rutils.run_hostapd(dut_host, setup_params)
            dut_host.dev['bssid'] = rutils.get_mac_addr(dut_host)
            apdev.append(dut_host.dev)

        if hwsim_test.__code__.co_argcount == 1:
            hwsim_test(dev)
        elif hwsim_test.__code__.co_argcount == 2:
            hwsim_test(dev, apdev)
        else:
            params = {}
            params['long'] = 1
            params['logdir'] = local_log_dir
            hwsim_test(dev, apdev, params)

       # hostapd/wpa_supplicant cleanup
        for wpas in dev:
            wpas.interface_remove(wpas.host.ifname)
            wpas.terminate()
        dev = []

        # remove monitors
        for ref_host in ref_hosts:
            monitor.remove(ref_host)
        for dut_host in dut_hosts:
            monitor.remove(dut_host)

        for ref_host in ref_hosts:
            rutils.kill_wpasupplicant(ref_host, setup_params)
            ref_host.get_logs(local_log_dir)
        for dut_host in dut_hosts:
            rutils.kill_hostapd(dut_host, setup_params)
            dut_host.get_logs(local_log_dir)
        if mon is not None:
            wlantest.Wlantest.reset_remote_wlantest()
            mon.get_logs(local_log_dir)

        return ""
    except:
        logger.info(traceback.format_exc())
        for wpas in dev:
            try:
                wpas.interface_remove(wpas.host.ifname)
                wpas.terminate()
            except:
                pass

        for ref_host in ref_hosts:
            monitor.remove(ref_host)
        for dut_host in dut_hosts:
            monitor.remove(dut_host)

        for ref_host in ref_hosts:
            rutils.kill_wpasupplicant(ref_host, setup_params)
            ref_host.get_logs(local_log_dir)
        for dut_host in dut_hosts:
            rutils.kill_hostapd(dut_host, setup_params)
            dut_host.get_logs(local_log_dir)
        if mon is not None:
            wlantest.Wlantest.reset_remote_wlantest()
            mon.get_logs(local_log_dir)
        raise
