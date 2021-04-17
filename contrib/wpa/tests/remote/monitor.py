# Monitor support
# Copyright (c) 2016, Tieto Corporation
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

import time
from remotehost import Host
import config
import rutils
import re
import traceback
import logging
logger = logging.getLogger()
import hostapd

# standalone monitor with multi iface support
def create(devices, setup_params, refs, duts, monitors):
    mons = []
    mhosts = []
    hosts = duts + refs

    # choose only standalone monitors
    for monitor in monitors:
        if monitor not in hosts and monitor != "all":
            mons.append(monitor)

    for mon in mons:
        word = mon.split(":")
        dev = config.get_device(devices, word[0])
        if dev is None:
            continue

        host = Host(host=dev['hostname'],
                    ifname=dev['ifname'],
                    port=dev['port'],
                    name=dev['name'])

        for iface_param in word[1:]:
            params = iface_param.split(",")
            if len(params) > 3:
                monitor_param = { "freq" : rutils.c2f(params[0]),
                                  "bw" : params[1],
                                  "center_freq1" : rutils.c2f(params[2]),
                                  "center_freq2" : rutils.c2f(params[3]) }
                host.monitor_params.append(monitor_param)

        try:
            host.execute(["iw", "reg", "set", setup_params['country']])
            rutils.setup_hw_host(host, setup_params, True)
        except:
            pass
        mhosts.append(host)

    return mhosts

def destroy(devices, hosts):
    for host in hosts:
        stop(host)
        for monitor in host.monitors:
            host.execute(["ifconfig", monitor, "down"])
        host.monitor_params = []

def setup(host, monitor_params=None):
    if host is None:
        return

    if monitor_params == None:
        monitor_params = host.monitor_params

    ifaces = re.split('; | |, ', host.ifname)
    count = 0
    for param in monitor_params:
        try:
            iface = ifaces[count]
        except:
            logger.debug(traceback.format_exc())
            break
        host.execute(["ifconfig", iface, " down"])
        host.execute(["rfkill", "unblock", "wifi"])
        host.execute(["iw", iface, "set type monitor"])
        host.execute(["ifconfig", iface, "up"])
        status, buf = host.execute(["iw", iface, "set", "freq", param['freq'],
                                    param['bw'], param['center_freq1'],
                                    param['center_freq2']])
        if status != 0:
            logger.debug("Could not setup monitor interface: " + buf)
            continue
        host.monitors.append(iface)
        count = count + 1

def run(host, setup_params):
    monitor_res = []
    log_monitor = ""
    if host is None:
        return None
    if len(host.monitors) == 0:
        return None
    try:
        log_dir = setup_params['log_dir']
        tc_name = setup_params['tc_name']
    except:
        return None

    tshark = "tshark"
    for monitor in host.monitors:
        host.execute(["ifconfig", monitor, "up"])
        tshark = tshark + " -i " + monitor
        log_monitor = log_monitor + "_" + monitor

    log = log_dir + tc_name + "_" + host.name + log_monitor + ".pcap"
    host.add_log(log)
    thread = host.thread_run([tshark, "-w", log], monitor_res)
    host.thread = thread


def stop(host):
    if host is None:
        return
    if len(host.monitors) == 0:
        return
    if host.thread is None:
        return

    host.thread_stop(host.thread)
    host.thread = None

# Add monitor to existing interface
def add(host, monitors):
    if host is None:
        return

    for monitor in monitors:
        if monitor != "all" and monitor != host.name:
            continue
        mon = "mon_" + host.ifname
        status, buf = host.execute(["iw", host.ifname, "interface", "add", mon,
                                    "type", "monitor"])
        if status == 0:
            host.monitors.append(mon)
            host.execute(["ifconfig", mon, "up"])
        else:
            logger.debug("Could not add monitor for " + host.name)

def remove(host):
    stop(host)
    for monitor in host.monitors:
        host.execute(["iw", monitor, "del"])
        host.monitors.remove(monitor)


# get monitor params from hostapd/wpa_supplicant
def get_monitor_params(wpa, is_p2p=False):
    if is_p2p:
        get_status_field_f = wpa.get_group_status_field
    else:
        get_status_field_f = wpa.get_status_field
    freq = get_status_field_f("freq")
    bw = "20"
    center_freq1 = ""
    center_freq2 = ""

    vht_oper_chwidth = get_status_field_f("vht_oper_chwidth")
    secondary_channel = get_status_field_f("secondary_channel")
    vht_oper_centr_freq_seg0_idx = get_status_field_f("vht_oper_centr_freq_seg0_idx")
    vht_oper_centr_freq_seg1_idx = get_status_field_f("vht_oper_centr_freq_seg1_idx")
    if vht_oper_chwidth == "0" or vht_oper_chwidth is None:
        if secondary_channel == "1":
            bw = "40"
            center_freq1 = str(int(freq) + 10)
        elif secondary_channel == "-1":
            center_freq1 = str(int(freq) - 10)
        else:
            pass
    elif vht_oper_chwidth == "1":
        bw = "80"
        center_freq1 = str(int(vht_oper_centr_freq_seg0_idx) * 5 + 5000)
    elif vht_oper_chwidth == "2":
        bw = "160"
        center_freq1 = str(int(vht_oper_centr_freq_seg0_idx) * 5 + 5000)
    elif vht_oper_chwidth == "3":
        bw = "80+80"
        center_freq1 = str(int(vht_oper_centr_freq_seg0_idx) * 5 + 5000)
        center_freq2 = str(int(vht_oper_centr_freq_seg1_idx) * 5 + 5000)
    else:
        pass

    monitor_params = {"freq" : freq,
                      "bw" : bw,
                      "center_freq1" : center_freq1,
                      "center_freq2" : center_freq2}

    return monitor_params
