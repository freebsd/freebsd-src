# AP mode using the older monitor interface design
# Copyright (c) 2013, Jouni Malinen <j@w1.fi>
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

import binascii
from remotehost import remote_compatible
import logging
logger = logging.getLogger()
import time

import hwsim_utils
import hostapd
from wpasupplicant import WpaSupplicant
from utils import radiotap_build, start_monitor, stop_monitor

def test_monitor_iface_open(dev, apdev):
    """Open connection using cfg80211 monitor interface on AP"""
    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    wpas.interface_add("wlan5", drv_params="use_monitor=1")
    id = wpas.add_network()
    wpas.set_network(id, "mode", "2")
    wpas.set_network_quoted(id, "ssid", "monitor-iface")
    wpas.set_network(id, "key_mgmt", "NONE")
    wpas.set_network(id, "frequency", "2412")
    wpas.connect_network(id)

    dev[0].connect("monitor-iface", key_mgmt="NONE", scan_freq="2412")

def test_monitor_iface_wpa2_psk(dev, apdev):
    """WPA2-PSK connection using cfg80211 monitor interface on AP"""
    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    wpas.interface_add("wlan5", drv_params="use_monitor=1")
    id = wpas.add_network()
    wpas.set_network(id, "mode", "2")
    wpas.set_network_quoted(id, "ssid", "monitor-iface-wpa2")
    wpas.set_network(id, "proto", "WPA2")
    wpas.set_network(id, "key_mgmt", "WPA-PSK")
    wpas.set_network_quoted(id, "psk", "12345678")
    wpas.set_network(id, "pairwise", "CCMP")
    wpas.set_network(id, "group", "CCMP")
    wpas.set_network(id, "frequency", "2412")
    wpas.connect_network(id)

    dev[0].connect("monitor-iface-wpa2", psk="12345678", scan_freq="2412")

def test_monitor_iface_multi_bss(dev, apdev):
    """AP mode mmonitor interface with hostapd multi-BSS setup"""
    params = {"ssid": "monitor-iface", "driver_params": "use_monitor=1"}
    hapd = hostapd.add_ap(apdev[0], params)
    hostapd.add_bss(apdev[0], apdev[0]['ifname'] + '-2', 'bss-2.conf')
    dev[0].connect("monitor-iface", key_mgmt="NONE", scan_freq="2412")
    dev[1].connect("bss-2", key_mgmt="NONE", scan_freq="2412")

@remote_compatible
def test_monitor_iface_unknown_sta(dev, apdev):
    """AP mode monitor interface and Data frame from unknown STA"""
    ssid = "monitor-iface-pmf"
    passphrase = "12345678"
    params = hostapd.wpa2_params(ssid=ssid, passphrase=passphrase)
    params["wpa_key_mgmt"] = "WPA-PSK-SHA256"
    params["ieee80211w"] = "2"
    params['driver_params'] = "use_monitor=1"
    hapd = hostapd.add_ap(apdev[0], params)

    bssid = apdev[0]['bssid']
    addr = dev[0].p2p_interface_addr()
    dev[0].connect(ssid, psk=passphrase, ieee80211w="2",
                   key_mgmt="WPA-PSK-SHA256", proto="WPA2",
                   scan_freq="2412")
    dev[0].request("DROP_SA")
    # This protected Deauth will be ignored by the STA
    hapd.request("DEAUTHENTICATE " + addr)
    # But the unprotected Deauth from TX frame-from-unassoc-STA will now be
    # processed
    try:
        sock = start_monitor(apdev[1]["ifname"])
        radiotap = radiotap_build()

        bssid = hapd.own_addr().replace(':', '')
        addr = dev[0].own_addr().replace(':', '')

        # Inject Data frame from STA to AP since we not have SA in place
        # anymore for normal data TX
        frame = binascii.unhexlify("48010000" + bssid + addr + bssid + "0000")
        sock.send(radiotap + frame)
    finally:
        stop_monitor(apdev[1]["ifname"])

    ev = dev[0].wait_event(["CTRL-EVENT-DISCONNECTED"], timeout=5)
    if ev is None:
        raise Exception("No disconnection")
    dev[0].request("DISCONNECT")
