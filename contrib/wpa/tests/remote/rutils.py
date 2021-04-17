# Utils
# Copyright (c) 2016, Tieto Corporation
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

import re
import time
from remotehost import Host
import hostapd
import config

class TestSkip(Exception):
    def __init__(self, reason):
        self.reason = reason
    def __str__(self):
        return self.reason

# get host based on name
def get_host(devices, dev_name):
    dev = config.get_device(devices, dev_name)
    host = Host(host=dev['hostname'],
                ifname=dev['ifname'],
                port=dev['port'],
                name=dev['name'])
    host.dev = dev
    return host

# Run setup_hw - hardware specific
def setup_hw_host_iface(host, iface, setup_params, force_restart=False):
    try:
        setup_hw = setup_params['setup_hw']
        restart = ""
        try:
            if setup_params['restart_device'] == True:
                restart = "-R"
        except:
            pass

        if force_restart:
            restart = "-R"

        host.execute([setup_hw, "-I", iface, restart])
    except:
        pass

def setup_hw_host(host, setup_params, force_restart=False):
    ifaces = re.split('; | |, ', host.ifname)
    for iface in ifaces:
        setup_hw_host_iface(host, iface, setup_params, force_restart)

def setup_hw(hosts, setup_params, force_restart=False):
    for host in hosts:
        setup_hw_host(host, setup_params, force_restart)

# get traces - hw specific
def trace_start(hosts, setup_params):
    for host in hosts:
        trace_start_stop(host, setup_params, start=True)

def trace_stop(hosts, setup_params):
    for host in hosts:
        trace_start_stop(host, setup_params, start=False)

def trace_start_stop(host, setup_params, start):
    if setup_params['trace'] == False:
        return
    try:
        start_trace = setup_params['trace_start']
        stop_trace = setup_params['trace_stop']
        if start:
            cmd = start_trace
        else:
            cmd = stop_trace
        trace_dir = setup_params['log_dir'] + host.ifname + "/remote_traces"
        host.add_log(trace_dir + "/*")
        host.execute([cmd, "-I", host.ifname, "-D", trace_dir])
    except:
        pass

# get perf
def perf_start(hosts, setup_params):
    for host in hosts:
        perf_start_stop(host, setup_params, start=True)

def perf_stop(hosts, setup_params):
    for host in hosts:
        perf_start_stop(host, setup_params, start=False)

def perf_start_stop(host, setup_params, start):
    if setup_params['perf'] == False:
        return
    try:
        perf_start = setup_params['perf_start']
        perf_stop = setup_params['perf_stop']
        if start:
            cmd = perf_start
        else:
            cmd = perf_stop
        perf_dir = setup_params['log_dir'] + host.ifname + "/remote_perf"
        host.add_log(perf_dir + "/*")
        host.execute([cmd, "-I", host.ifname, "-D", perf_dir])
    except:
        pass

# hostapd/wpa_supplicant helpers
def run_hostapd(host, setup_params):
    log_file = None
    try:
        tc_name = setup_params['tc_name']
        log_dir = setup_params['log_dir']
        log_file = log_dir + tc_name + "_hostapd_" + host.name + "_" + host.ifname + ".log"
        host.execute(["rm", log_file])
        log = " -f " + log_file
    except:
        log = ""

    if log_file:
        host.add_log(log_file)
    pidfile = setup_params['log_dir'] + "hostapd_" + host.ifname + "_" + setup_params['tc_name'] + ".pid"
    status, buf = host.execute([setup_params['hostapd'], "-B", "-ddt", "-g", "udp:" + host.port, "-P", pidfile, log])
    if status != 0:
        raise Exception("Could not run hostapd: " + buf)

def run_wpasupplicant(host, setup_params):
    log_file = None
    try:
        tc_name = setup_params['tc_name']
        log_dir = setup_params['log_dir']
        log_file = log_dir + tc_name + "_wpa_supplicant_" + host.name + "_" + host.ifname + ".log"
        host.execute(["rm", log_file])
        log = " -f " + log_file
    except:
        log = ""

    if log_file:
        host.add_log(log_file)
    pidfile = setup_params['log_dir'] + "wpa_supplicant_" + host.ifname + "_" + setup_params['tc_name'] + ".pid"
    status, buf = host.execute([setup_params['wpa_supplicant'], "-B", "-ddt", "-g", "udp:" + host.port, "-P", pidfile, log])
    if status != 0:
        raise Exception("Could not run wpa_supplicant: " + buf)

def kill_wpasupplicant(host, setup_params):
    pidfile = setup_params['log_dir'] + "wpa_supplicant_" + host.ifname + "_" + setup_params['tc_name'] + ".pid"
    host.execute(["kill `cat " + pidfile + "`"])

def kill_hostapd(host, setup_params):
    pidfile = setup_params['log_dir'] + "hostapd_" + host.ifname + "_" + setup_params['tc_name'] + ".pid"
    host.execute(["kill `cat " + pidfile + "`"])

def get_ap_params(channel="1", bw="HT20", country="US", security="open", ht_capab=None, vht_capab=None):
    ssid = "test_" + channel + "_" + security + "_" + bw

    if bw == "b_only":
        params = hostapd.b_only_params(channel, ssid, country)
    elif bw == "g_only":
        params = hostapd.g_only_params(channel, ssid, country)
    elif bw == "g_only_wmm":
        params = hostapd.g_only_params(channel, ssid, country)
        params['wmm_enabled'] = "1"
    elif bw == "a_only":
        params = hostapd.a_only_params(channel, ssid, country)
    elif bw == "a_only_wmm":
        params = hostapd.a_only_params(channel, ssid, country)
        params['wmm_enabled'] = "1"
    elif bw == "HT20":
        params = hostapd.ht20_params(channel, ssid, country)
        if ht_capab:
            try:
                params['ht_capab'] = params['ht_capab'] + ht_capab
            except:
                params['ht_capab'] = ht_capab
    elif bw == "HT40+":
        params = hostapd.ht40_plus_params(channel, ssid, country)
        if ht_capab:
            params['ht_capab'] = params['ht_capab'] + ht_capab
    elif bw == "HT40-":
        params = hostapd.ht40_minus_params(channel, ssid, country)
        if ht_capab:
            params['ht_capab'] = params['ht_capab'] + ht_capab
    elif bw == "VHT80":
        params = hostapd.ht40_plus_params(channel, ssid, country)
        if ht_capab:
            params['ht_capab'] = params['ht_capab'] + ht_capab
        if vht_capab:
            try:
                params['vht_capab'] = params['vht_capab'] + vht_capab
            except:
                params['vht_capab'] = vht_capab
        params['ieee80211ac'] = "1"
        params['vht_oper_chwidth'] = "1"
        params['vht_oper_centr_freq_seg0_idx'] = str(int(channel) + 6)
    else:
        params = {}

    # now setup security params
    if security == "tkip":
        sec_params = hostapd.wpa_params(passphrase="testtest")
    elif security == "ccmp":
        sec_params = hostapd.wpa2_params(passphrase="testtest")
    elif security == "mixed":
        sec_params = hostapd.wpa_mixed_params(passphrase="testtest")
    elif security == "wep":
        sec_params = {"wep_key0" : "123456789a",
                      "wep_default_key" : "0",
                      "auth_algs" : "1"}
    elif security == "wep_shared":
        sec_params = {"wep_key0" : "123456789a",
                      "wep_default_key" : "0",
                      "auth_algs" : "2"}
    else:
        sec_params = {}

    params.update(sec_params)

    return params

# ip helpers
def get_ipv4(client, ifname=None):
    if ifname is None:
        ifname = client.ifname
    status, buf = client.execute(["ifconfig", ifname])
    lines = buf.splitlines()

    for line in lines:
        res = line.find("inet addr:")
        if res != -1:
            break

    if res != -1:
        words = line.split()
        addr = words[1].split(":")
        return addr[1]

    return "unknown"

def get_ipv6(client, ifname=None):
    res = -1
    if ifname is None:
        ifname = client.ifname
    status, buf = client.execute(["ifconfig", ifname])
    lines = buf.splitlines()

    for line in lines:
        res = line.find("Scope:Link")
        if res == -1:
            res = line.find("<link>")
        if res != -1:
            break

    if res != -1:
        words = line.split()
        if words[0] == "inet6" and words[1] == "addr:":
            addr_mask = words[2]
            addr = addr_mask.split("/")
            return addr[0]
        if words[0] == "inet6":
            return words[1]

    return "unknown"

def get_ip(client, addr_type="ipv6", iface=None):
    if addr_type == "ipv6":
        return get_ipv6(client, iface)
    elif addr_type == "ipv4":
        return get_ipv4(client, iface)
    else:
        return "unknown addr_type: " + addr_type

def get_ipv4_addr(setup_params, number):
    try:
        ipv4_base = setup_params['ipv4_test_net']
    except:
        ipv4_base = "172.16.12.0"

    parts = ipv4_base.split('.')
    ipv4 = parts[0] + "." + parts[1] + "." + parts[2] + "." + str(number)

    return ipv4

def get_mac_addr(host, iface=None):
    if iface == None:
        iface = host.ifname
    status, buf = host.execute(["ifconfig", iface])
    if status != 0:
        raise Exception("ifconfig " + iface)
    words = buf.split()
    found = 0
    for word in words:
        if found == 1:
            return word
        if word == "HWaddr" or word == "ether":
            found = 1
    raise Exception("Could not find HWaddr")

# connectivity/ping helpers
def get_ping_packet_loss(ping_res):
    loss_line = ""
    lines = ping_res.splitlines()
    for line in lines:
        if line.find("packet loss") != -1:
            loss_line = line
            break;

    if loss_line == "":
        return "100%"

    sections = loss_line.split(",")

    for section in sections:
        if section.find("packet loss") != -1:
            words = section.split()
            return words[0]

    return "100%"

def ac_to_ping_ac(qos):
    if qos == "be":
        qos_param = "0x00"
    elif qos == "bk":
        qos_param = "0x20"
    elif qos == "vi":
        qos_param = "0xA0"
    elif qos == "vo":
        qos_param = "0xE0"
    else:
        qos_param = "0x00"
    return qos_param

def ping_run(host, ip, result, ifname=None, addr_type="ipv4", deadline="5", qos=None):
    if ifname is None:
       ifname = host.ifname
    if addr_type == "ipv6":
        ping = ["ping6"]
    else:
        ping = ["ping"]

    ping = ping + ["-w", deadline, "-I", ifname]
    if qos:
        ping = ping + ["-Q", ac_to_ping_ac(qos)]
    ping = ping + [ip]

    flush_arp_cache(host)

    thread = host.thread_run(ping, result)
    return thread

def ping_wait(host, thread, timeout=None):
    host.thread_wait(thread, timeout)
    if thread.is_alive():
        raise Exception("ping thread still alive")

def flush_arp_cache(host):
    host.execute(["ip", "-s", "-s", "neigh", "flush", "all"])

def check_connectivity(a, b, addr_type="ipv4", deadline="5", qos=None):
    addr_a = get_ip(a, addr_type)
    addr_b = get_ip(b, addr_type)

    if addr_type == "ipv4":
        ping = ["ping"]
    else:
        ping = ["ping6"]

    ping_a_b = ping + ["-w", deadline, "-I", a.ifname]
    ping_b_a = ping + ["-w", deadline, "-I", b.ifname]
    if qos:
        ping_a_b = ping_a_b + ["-Q", ac_to_ping_ac(qos)]
        ping_b_a = ping_b_a + ["-Q", ac_to_ping_ac(qos)]
    ping_a_b = ping_a_b + [addr_b]
    ping_b_a = ping_b_a + [addr_a]

    # Clear arp cache
    flush_arp_cache(a)
    flush_arp_cache(b)

    status, buf = a.execute(ping_a_b)
    if status == 2 and ping == "ping6":
        # tentative possible for a while, try again
        time.sleep(3)
        status, buf = a.execute(ping_a_b)
    if status != 0:
        raise Exception("ping " + a.name + "/" + a.ifname + " >> " + b.name + "/" + b.ifname)

    a_b = get_ping_packet_loss(buf)

    # Clear arp cache
    flush_arp_cache(a)
    flush_arp_cache(b)

    status, buf = b.execute(ping_b_a)
    if status != 0:
        raise Exception("ping " + b.name + "/" + b.ifname + " >> " + a.name + "/" + a.ifname)

    b_a = get_ping_packet_loss(buf)

    if int(a_b[:-1]) > 40:
        raise Exception("Too high packet lost: " + a_b)

    if int(b_a[:-1]) > 40:
        raise Exception("Too high packet lost: " + b_a)

    return a_b, b_a


# iperf helpers
def get_iperf_speed(iperf_res, pattern="Mbits/sec"):
    lines = iperf_res.splitlines()
    sum_line = ""
    last_line = ""
    count = 0
    res = -1

    # first find last SUM line
    for line in lines:
        res = line.find("[SUM]")
        if res != -1:
            sum_line = line

    # next check SUM status
    if sum_line != "":
        words = sum_line.split()
        for word in words:
            res = word.find(pattern)
            if res != -1:
                return words[count - 1] + " " + pattern
            count = count + 1

    # no SUM - one thread - find last line
    for line in lines:
        res = line.find(pattern)
        if res != -1:
            last_line = line

    if last_line == "":
        return "0 " + pattern

    count = 0
    words = last_line.split()
    for word in words:
        res = word.find(pattern)
        if res != -1:
            return words[count - 1] + " " + pattern
            break;
        count = count + 1
    return "0 " + pattern

def ac_to_iperf_ac(qos):
    if qos == "be":
        qos_param = "0x00"
    elif qos == "bk":
        qos_param = "0x20"
    elif qos == "vi":
        qos_param = "0xA0"
    elif qos == "vo":
        qos_param = "0xE0"
    else:
        qos_param = "0x00"
    return qos_param

def iperf_run(server, client, server_ip, client_res, server_res,
              l4="udp", bw="30M", test_time="30", parallel="5",
              qos="be", param=" -i 5 ", ifname=None, l3="ipv4",
              port="5001", iperf="iperf"):
    if ifname == None:
        ifname = client.ifname

    if iperf == "iperf":
        iperf_server = [iperf]
    elif iperf == "iperf3":
        iperf_server = [iperf, "-1"]

    if l3 == "ipv4":
        iperf_client = [iperf, "-c", server_ip, "-p", port]
        iperf_server = iperf_server + ["-p", port]
    elif l3 == "ipv6":
        iperf_client = [iperf, "-V", "-c", server_ip  + "%" + ifname, "-p", port]
        iperf_server = iperf_server + ["-V", "-p", port]
    else:
        return -1, -1

    iperf_server = iperf_server + ["-s", "-f", "m", param]
    iperf_client = iperf_client + ["-f", "m", "-t", test_time]

    if parallel != "1":
        iperf_client = iperf_client + ["-P", parallel]

    if l4 == "udp":
        if iperf != "iperf3":
            iperf_server = iperf_server + ["-u"]
        iperf_client = iperf_client + ["-u", "-b", bw]

    if qos:
        iperf_client = iperf_client + ["-Q", ac_to_iperf_ac(qos)]

    flush_arp_cache(server)
    flush_arp_cache(client)

    server_thread = server.thread_run(iperf_server, server_res)
    time.sleep(1)
    client_thread = client.thread_run(iperf_client, client_res)

    return server_thread, client_thread

def iperf_wait(server, client, server_thread, client_thread, timeout=None, iperf="iperf"):
    client.thread_wait(client_thread, timeout)
    if client_thread.is_alive():
        raise Exception("iperf client thread still alive")

    server.thread_wait(server_thread, 5)
    if server_thread.is_alive():
        server.execute(["killall", "-s", "INT", iperf])
        time.sleep(1)

    server.thread_wait(server_thread, 5)
    if server_thread.is_alive():
        raise Exception("iperf server thread still alive")

    return

def run_tp_test(server, client, l3="ipv4", iperf="iperf", l4="tcp", test_time="10", parallel="5",
                qos="be", bw="30M", ifname=None, port="5001"):
    client_res = []
    server_res = []

    server_ip = get_ip(server, l3)
    time.sleep(1)
    server_thread, client_thread = iperf_run(server, client, server_ip, client_res, server_res,
                                             l3=l3, iperf=iperf, l4=l4, test_time=test_time,
                                             parallel=parallel, qos=qos, bw=bw, ifname=ifname,
                                             port=port)
    iperf_wait(server, client, server_thread, client_thread, iperf=iperf, timeout=int(test_time) + 10)

    if client_res[0] != 0:
        raise Exception(iperf + " client: " + client_res[1])
    if server_res[0] != 0:
        raise Exception(iperf + " server: " + server_res[1])
    if client_res[1] is None:
        raise Exception(iperf + " client result issue")
    if server_res[1] is None:
        raise Exception(iperf + " server result issue")

    if iperf == "iperf":
          result = server_res[1]
    if iperf == "iperf3":
          result = client_res[1]

    speed = get_iperf_speed(result)
    return speed

def get_iperf_bw(bw, parallel, spacial_streams=2):
    if bw == "b_only":
        max_tp = 11
    elif bw == "g_only" or bw == "g_only_wmm" or bw == "a_only" or bw == "a_only_wmm":
        max_tp = 54
    elif bw == "HT20":
        max_tp = 72 * spacial_streams
    elif bw == "HT40+" or bw == "HT40-":
        max_tp = 150 * spacial_streams
    elif bw == "VHT80":
        max_tp = 433 * spacial_streams
    else:
        max_tp = 150

    max_tp = 1.2 * max_tp

    return str(int(max_tp/int(parallel))) + "M"
