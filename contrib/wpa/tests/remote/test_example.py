# Example test case
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

import logging
logger = logging.getLogger()

def test_example(devices, setup_params, refs, duts, monitors):
    """TC example - simple connect and ping test"""
    try:
        sta = None
        ap = None
        hapd = None
        wpas = None
        mon = None

        # get hosts based on name
        sta = rutils.get_host(devices, duts[0])
        ap = rutils.get_host(devices, refs[0])

        # setup log dir
        local_log_dir = setup_params['local_log_dir']

        # setup hw before test
        rutils.setup_hw([sta, ap], setup_params)

        # run traces if requested
        rutils.trace_start([sta], setup_params)

        # run perf if requested
        rutils.perf_start([sta], setup_params)

        # run hostapd/wpa_supplicant
        rutils.run_wpasupplicant(sta, setup_params)
        rutils.run_hostapd(ap, setup_params)

        # get ap_params
        ap_params = rutils.get_ap_params(channel="1", bw="HT20", country="US",
                                         security="open")

        # Add monitors if requested
        monitor_hosts = monitor.create(devices, setup_params, refs, duts,
                                       monitors)
        if len(monitor_hosts) > 0:
            mon = monitor_hosts[0]
        monitor.add(sta, monitors)
        monitor.add(ap, monitors)

        # connect to hostapd/wpa_supplicant UDP CTRL iface
        hapd = hostapd.add_ap(ap.dev, ap_params)
        freq = hapd.get_status_field("freq")
        wpas = WpaSupplicant(hostname=sta.host, global_iface="udp",
                             global_port=sta.port)
        wpas.interface_add(sta.ifname)

        # setup standalone monitor based on hapd; could be multi interface
        # monitor
        monitor_param = monitor.get_monitor_params(hapd)
        monitor.setup(mon, [monitor_param])

        # run monitors
        monitor.run(sta, setup_params)
        monitor.run(ap, setup_params)
        monitor.run(mon, setup_params)

        # connect wpa_supplicant to hostapd
        wpas.connect(ap_params['ssid'], key_mgmt="NONE", scan_freq=freq)

        # run ping test
        ap_sta, sta_ap = rutils.check_connectivity(ap, sta, "ipv6")

        # remove/destroy monitors
        monitor.remove(sta)
        monitor.remove(ap)
        monitor.destroy(devices, monitor_hosts)

        # hostapd/wpa_supplicant cleanup
        wpas.interface_remove(sta.ifname)
        wpas.terminate()

        hapd.close_ctrl()
        hostapd.remove_bss(ap.dev)
        hostapd.terminate(ap.dev)

        # stop perf
        rutils.perf_stop([sta], setup_params)

        # stop traces
        rutils.trace_stop([sta], setup_params)

        # get wpa_supplicant/hostapd/tshark logs
        sta.get_logs(local_log_dir)
        ap.get_logs(local_log_dir)
        if mon:
            mon.get_logs(local_log_dir)

        return "packet_loss: " + ap_sta + ", " + sta_ap
    except:
        rutils.perf_stop([sta], setup_params)
        rutils.trace_stop([sta], setup_params)
        if wpas:
            try:
                wpas.interface_remove(sta.ifname)
                wpas.terminate()
            except:
                pass
        if hapd:
            try:
                hapd.close_ctrl()
                hostapd.remove_bss(ap.dev)
                hostapd.terminate(ap.dev)
            except:
                pass
        if mon:
            monitor.destroy(devices, monitor_hosts)
            mon.get_logs(local_log_dir)

        if sta:
            monitor.remove(sta)
            dmesg = setup_params['log_dir'] + setup_params['tc_name'] + "_" + sta.name + "_" + sta.ifname + ".dmesg"
            sta.execute(["dmesg", "-c", ">", dmesg])
            sta.add_log(dmesg)
            sta.get_logs(local_log_dir)
            sta.execute(["ifconfig", sta.ifname, "down"])
        if ap:
            monitor.remove(ap)
            dmesg = setup_params['log_dir'] + setup_params['tc_name'] + "_" + ap.name + "_" + ap.ifname + ".dmesg"
            ap.execute(["dmesg", "-c", ">", dmesg])
            ap.add_log(dmesg)
            ap.get_logs(local_log_dir)
            ap.execute(["ifconfig", ap.ifname, " down"])
        raise
