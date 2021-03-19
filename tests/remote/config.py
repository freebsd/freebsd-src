# Environment configuration
# Copyright (c) 2016, Tieto Corporation
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

#
# Currently static definition, in the future this could be a config file,
# or even common database with host management.
#

import logging
logger = logging.getLogger()

#
# You can put your settings in cfg.py file with setup_params, devices
# definitions in the format as below. In other case HWSIM cfg will be used.
#
setup_params = {"setup_hw" : "./tests/setup_hw.sh",
                "hostapd" : "./tests/hostapd-rt",
                "wpa_supplicant" : "./tests/wpa_supplicant-rt",
                "iperf" : "iperf",
                "wlantest" : "./tests/wlantest",
                "wlantest_cli" : "./tests/wlantest_cli",
                "country" : "US",
                "log_dir" : "/tmp/",
                "ipv4_test_net" : "192.168.12.0",
                "trace_start" : "./tests/trace_start.sh",
                "trace_stop" : "./tests/trace_stop.sh",
                "perf_start" : "./tests/perf_start.sh",
                "perf_stop" : "./tests/perf_stop.sh"}

#
#devices = [{"hostname": "192.168.254.58", "ifname" : "wlan0", "port": "9877", "name" : "t2-ath9k", "flags" : "AP_HT40 STA_HT40"},
#           {"hostname": "192.168.254.58", "ifname" : "wlan1", "port": "9877", "name" : "t2-ath10k", "flags" : "AP_VHT80"},
#           {"hostname": "192.168.254.58", "ifname" : "wlan3", "port": "9877", "name" : "t2-intel7260", "flags" : "STA_VHT80"},
#           {"hostname": "192.168.254.55", "ifname" : "wlan0, wlan1, wlan2", "port": "", "name" : "t3-monitor"},
#           {"hostname": "192.168.254.50", "ifname" : "wlan0", "port": "9877", "name" : "t1-ath9k"},
#           {"hostname": "192.168.254.50", "ifname" : "wlan1", "port": "9877", "name" : "t1-ath10k"}]

#
# HWSIM - ifaces available after modprobe mac80211_hwsim
#
devices = [{"hostname": "localhost", "ifname": "wlan0", "port": "9868", "name": "hwsim0", "flags": "AP_VHT80 STA_VHT80"},
           {"hostname": "localhost", "ifname": "wlan1", "port": "9878", "name": "hwsim1", "flags": "AP_VHT80 STA_VHT80"},
           {"hostname": "localhost", "ifname": "wlan2", "port": "9888", "name": "hwsim2", "flags": "AP_VHT80 STA_VHT80"},
           {"hostname": "localhost", "ifname": "wlan3", "port": "9898", "name": "hwsim3", "flags": "AP_VHT80 STA_VHT80"},
           {"hostname": "localhost", "ifname": "wlan4", "port": "9908", "name": "hwsim4", "flags": "AP_VHT80 STA_VHT80"}]


def get_setup_params(filename="cfg.py"):
    try:
       mod = __import__(filename.split(".")[0])
       return mod.setup_params
    except:
       logger.debug("__import__(" + filename + ") failed, using static settings")
       pass
    return setup_params

def get_devices(filename="cfg.py"):
    try:
       mod = __import__(filename.split(".")[0])
       return mod.devices
    except:
       logger.debug("__import__(" + filename + ") failed, using static settings")
       pass
    return devices

def get_device(devices, name=None, flags=None, lock=False):
    if name is None and flags is None:
        raise Exception("Failed to get device")
    word = name.split(":")
    name = word[0]
    for device in devices:
        if device['name'] == name:
            return device
    for device in devices:
        try:
            device_flags = device['flags']
            if device_flags.find(flags) != -1:
                return device
        except:
            pass
    raise Exception("Failed to get device " + name)

def put_device(devices, name):
    pass
