# Python class for controlling hostapd
# Copyright (c) 2013-2019, Jouni Malinen <j@w1.fi>
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

import os
import re
import time
import logging
import binascii
import struct
import tempfile
import wpaspy
import remotehost
import utils
import subprocess

logger = logging.getLogger()
hapd_ctrl = '/var/run/hostapd'
hapd_global = '/var/run/hostapd-global'

def mac2tuple(mac):
    return struct.unpack('6B', binascii.unhexlify(mac.replace(':', '')))

class HostapdGlobal:
    def __init__(self, apdev=None, global_ctrl_override=None):
        try:
            hostname = apdev['hostname']
            port = apdev['port']
        except:
            hostname = None
            port = 8878
        self.host = remotehost.Host(hostname)
        self.hostname = hostname
        self.port = port
        if hostname is None:
            global_ctrl = hapd_global
            if global_ctrl_override:
                global_ctrl = global_ctrl_override
            self.ctrl = wpaspy.Ctrl(global_ctrl)
            self.mon = wpaspy.Ctrl(global_ctrl)
            self.dbg = ""
        else:
            self.ctrl = wpaspy.Ctrl(hostname, port)
            self.mon = wpaspy.Ctrl(hostname, port)
            self.dbg = hostname + "/" + str(port)
        self.mon.attach()

    def cmd_execute(self, cmd_array, shell=False):
        if self.hostname is None:
            if shell:
                cmd = ' '.join(cmd_array)
            else:
                cmd = cmd_array
            proc = subprocess.Popen(cmd, stderr=subprocess.STDOUT,
                                    stdout=subprocess.PIPE, shell=shell)
            out = proc.communicate()[0]
            ret = proc.returncode
            return ret, out.decode()
        else:
            return self.host.execute(cmd_array)

    def request(self, cmd, timeout=10):
        logger.debug(self.dbg + ": CTRL(global): " + cmd)
        return self.ctrl.request(cmd, timeout)

    def wait_event(self, events, timeout):
        start = os.times()[4]
        while True:
            while self.mon.pending():
                ev = self.mon.recv()
                logger.debug(self.dbg + "(global): " + ev)
                for event in events:
                    if event in ev:
                        return ev
            now = os.times()[4]
            remaining = start + timeout - now
            if remaining <= 0:
                break
            if not self.mon.pending(timeout=remaining):
                break
        return None

    def add(self, ifname, driver=None):
        cmd = "ADD " + ifname + " " + hapd_ctrl
        if driver:
            cmd += " " + driver
        res = self.request(cmd)
        if "OK" not in res:
            raise Exception("Could not add hostapd interface " + ifname)

    def add_iface(self, ifname, confname):
        res = self.request("ADD " + ifname + " config=" + confname)
        if "OK" not in res:
            raise Exception("Could not add hostapd interface")

    def add_bss(self, phy, confname, ignore_error=False):
        res = self.request("ADD bss_config=" + phy + ":" + confname)
        if "OK" not in res:
            if not ignore_error:
                raise Exception("Could not add hostapd BSS")

    def remove(self, ifname):
        self.request("REMOVE " + ifname, timeout=30)

    def relog(self):
        self.request("RELOG")

    def flush(self):
        self.request("FLUSH")

    def get_ctrl_iface_port(self, ifname):
        if self.hostname is None:
            return None

        res = self.request("INTERFACES ctrl")
        lines = res.splitlines()
        found = False
        for line in lines:
            words = line.split()
            if words[0] == ifname:
                found = True
                break
        if not found:
            raise Exception("Could not find UDP port for " + ifname)
        res = line.find("ctrl_iface=udp:")
        if res == -1:
            raise Exception("Wrong ctrl_interface format")
        words = line.split(":")
        return int(words[1])

    def terminate(self):
        self.mon.detach()
        self.mon.close()
        self.mon = None
        self.ctrl.terminate()
        self.ctrl = None

    def send_file(self, src, dst):
        self.host.send_file(src, dst)

class Hostapd:
    def __init__(self, ifname, bssidx=0, hostname=None, port=8877):
        self.hostname = hostname
        self.host = remotehost.Host(hostname, ifname)
        self.ifname = ifname
        if hostname is None:
            self.ctrl = wpaspy.Ctrl(os.path.join(hapd_ctrl, ifname))
            self.mon = wpaspy.Ctrl(os.path.join(hapd_ctrl, ifname))
            self.dbg = ifname
        else:
            self.ctrl = wpaspy.Ctrl(hostname, port)
            self.mon = wpaspy.Ctrl(hostname, port)
            self.dbg = hostname + "/" + ifname
        self.mon.attach()
        self.bssid = None
        self.bssidx = bssidx

    def cmd_execute(self, cmd_array, shell=False):
        if self.hostname is None:
            if shell:
                cmd = ' '.join(cmd_array)
            else:
                cmd = cmd_array
            proc = subprocess.Popen(cmd, stderr=subprocess.STDOUT,
                                    stdout=subprocess.PIPE, shell=shell)
            out = proc.communicate()[0]
            ret = proc.returncode
            return ret, out.decode()
        else:
            return self.host.execute(cmd_array)

    def close_ctrl(self):
        if self.mon is not None:
            self.mon.detach()
            self.mon.close()
            self.mon = None
            self.ctrl.close()
            self.ctrl = None

    def own_addr(self):
        if self.bssid is None:
            self.bssid = self.get_status_field('bssid[%d]' % self.bssidx)
        return self.bssid

    def get_addr(self, group=False):
        return self.own_addr()

    def request(self, cmd):
        logger.debug(self.dbg + ": CTRL: " + cmd)
        return self.ctrl.request(cmd)

    def ping(self):
        return "PONG" in self.request("PING")

    def set(self, field, value):
        if "OK" not in self.request("SET " + field + " " + value):
            if "TKIP" in value and (field == "wpa_pairwise" or \
                                    field == "rsn_pairwise"):
                raise utils.HwsimSkip("Cipher TKIP not supported")
            raise Exception("Failed to set hostapd parameter " + field)

    def set_defaults(self):
        self.set("driver", "nl80211")
        self.set("hw_mode", "g")
        self.set("channel", "1")
        self.set("ieee80211n", "1")
        self.set("logger_stdout", "-1")
        self.set("logger_stdout_level", "0")

    def set_open(self, ssid):
        self.set_defaults()
        self.set("ssid", ssid)

    def set_wpa2_psk(self, ssid, passphrase):
        self.set_defaults()
        self.set("ssid", ssid)
        self.set("wpa_passphrase", passphrase)
        self.set("wpa", "2")
        self.set("wpa_key_mgmt", "WPA-PSK")
        self.set("rsn_pairwise", "CCMP")

    def set_wpa_psk(self, ssid, passphrase):
        self.set_defaults()
        self.set("ssid", ssid)
        self.set("wpa_passphrase", passphrase)
        self.set("wpa", "1")
        self.set("wpa_key_mgmt", "WPA-PSK")
        self.set("wpa_pairwise", "TKIP")

    def set_wpa_psk_mixed(self, ssid, passphrase):
        self.set_defaults()
        self.set("ssid", ssid)
        self.set("wpa_passphrase", passphrase)
        self.set("wpa", "3")
        self.set("wpa_key_mgmt", "WPA-PSK")
        self.set("wpa_pairwise", "TKIP")
        self.set("rsn_pairwise", "CCMP")

    def set_wep(self, ssid, key):
        self.set_defaults()
        self.set("ssid", ssid)
        self.set("wep_key0", key)

    def enable(self):
        if "OK" not in self.request("ENABLE"):
            raise Exception("Failed to enable hostapd interface " + self.ifname)

    def disable(self):
        if "OK" not in self.request("DISABLE"):
            raise Exception("Failed to disable hostapd interface " + self.ifname)

    def dump_monitor(self):
        while self.mon.pending():
            ev = self.mon.recv()
            logger.debug(self.dbg + ": " + ev)

    def wait_event(self, events, timeout):
        if not isinstance(events, list):
            raise Exception("Hostapd.wait_event() called with incorrect events argument type")
        start = os.times()[4]
        while True:
            while self.mon.pending():
                ev = self.mon.recv()
                logger.debug(self.dbg + ": " + ev)
                for event in events:
                    if event in ev:
                        return ev
            now = os.times()[4]
            remaining = start + timeout - now
            if remaining <= 0:
                break
            if not self.mon.pending(timeout=remaining):
                break
        return None

    def wait_sta(self, addr=None, timeout=2):
        ev = self.wait_event(["AP-STA-CONNECT"], timeout=timeout)
        if ev is None:
            raise Exception("AP did not report STA connection")
        if addr and addr not in ev:
            raise Exception("Unexpected STA address in connection event: " + ev)

    def wait_ptkinitdone(self, addr, timeout=2):
        while timeout > 0:
            sta = self.get_sta(addr)
            if 'hostapdWPAPTKState' not in sta:
                raise Exception("GET_STA did not return hostapdWPAPTKState")
            state = sta['hostapdWPAPTKState']
            if state == "11":
                return
            time.sleep(0.1)
            timeout -= 0.1
        raise Exception("Timeout while waiting for PTKINITDONE")

    def get_status(self):
        res = self.request("STATUS")
        lines = res.splitlines()
        vals = dict()
        for l in lines:
            [name, value] = l.split('=', 1)
            vals[name] = value
        return vals

    def get_status_field(self, field):
        vals = self.get_status()
        if field in vals:
            return vals[field]
        return None

    def get_driver_status(self):
        res = self.request("STATUS-DRIVER")
        lines = res.splitlines()
        vals = dict()
        for l in lines:
            [name, value] = l.split('=', 1)
            vals[name] = value
        return vals

    def get_driver_status_field(self, field):
        vals = self.get_driver_status()
        if field in vals:
            return vals[field]
        return None

    def get_config(self):
        res = self.request("GET_CONFIG")
        lines = res.splitlines()
        vals = dict()
        for l in lines:
            [name, value] = l.split('=', 1)
            vals[name] = value
        return vals

    def mgmt_rx(self, timeout=5):
        ev = self.wait_event(["MGMT-RX"], timeout=timeout)
        if ev is None:
            return None
        msg = {}
        frame = binascii.unhexlify(ev.split(' ')[1])
        msg['frame'] = frame

        hdr = struct.unpack('<HH6B6B6BH', frame[0:24])
        msg['fc'] = hdr[0]
        msg['subtype'] = (hdr[0] >> 4) & 0xf
        hdr = hdr[1:]
        msg['duration'] = hdr[0]
        hdr = hdr[1:]
        msg['da'] = "%02x:%02x:%02x:%02x:%02x:%02x" % hdr[0:6]
        hdr = hdr[6:]
        msg['sa'] = "%02x:%02x:%02x:%02x:%02x:%02x" % hdr[0:6]
        hdr = hdr[6:]
        msg['bssid'] = "%02x:%02x:%02x:%02x:%02x:%02x" % hdr[0:6]
        hdr = hdr[6:]
        msg['seq_ctrl'] = hdr[0]
        msg['payload'] = frame[24:]

        return msg

    def mgmt_tx(self, msg):
        t = (msg['fc'], 0) + mac2tuple(msg['da']) + mac2tuple(msg['sa']) + mac2tuple(msg['bssid']) + (0,)
        hdr = struct.pack('<HH6B6B6BH', *t)
        res = self.request("MGMT_TX " + binascii.hexlify(hdr + msg['payload']).decode())
        if "OK" not in res:
            raise Exception("MGMT_TX command to hostapd failed")

    def get_sta(self, addr, info=None, next=False):
        cmd = "STA-NEXT " if next else "STA "
        if addr is None:
            res = self.request("STA-FIRST")
        elif info:
            res = self.request(cmd + addr + " " + info)
        else:
            res = self.request(cmd + addr)
        lines = res.splitlines()
        vals = dict()
        first = True
        for l in lines:
            if first and '=' not in l:
                vals['addr'] = l
                first = False
            else:
                [name, value] = l.split('=', 1)
                vals[name] = value
        return vals

    def get_mib(self, param=None):
        if param:
            res = self.request("MIB " + param)
        else:
            res = self.request("MIB")
        lines = res.splitlines()
        vals = dict()
        for l in lines:
            name_val = l.split('=', 1)
            if len(name_val) > 1:
                vals[name_val[0]] = name_val[1]
        return vals

    def get_pmksa(self, addr):
        res = self.request("PMKSA")
        lines = res.splitlines()
        for l in lines:
            if addr not in l:
                continue
            vals = dict()
            [index, aa, pmkid, expiration, opportunistic] = l.split(' ')
            vals['index'] = index
            vals['pmkid'] = pmkid
            vals['expiration'] = expiration
            vals['opportunistic'] = opportunistic
            return vals
        return None

    def dpp_qr_code(self, uri):
        res = self.request("DPP_QR_CODE " + uri)
        if "FAIL" in res:
            raise Exception("Failed to parse QR Code URI")
        return int(res)

    def dpp_nfc_uri(self, uri):
        res = self.request("DPP_NFC_URI " + uri)
        if "FAIL" in res:
            raise Exception("Failed to parse NFC URI")
        return int(res)

    def dpp_bootstrap_gen(self, type="qrcode", chan=None, mac=None, info=None,
                          curve=None, key=None):
        cmd = "DPP_BOOTSTRAP_GEN type=" + type
        if chan:
            cmd += " chan=" + chan
        if mac:
            if mac is True:
                mac = self.own_addr()
            cmd += " mac=" + mac.replace(':', '')
        if info:
            cmd += " info=" + info
        if curve:
            cmd += " curve=" + curve
        if key:
            cmd += " key=" + key
        res = self.request(cmd)
        if "FAIL" in res:
            raise Exception("Failed to generate bootstrapping info")
        return int(res)

    def dpp_bootstrap_set(self, id, conf=None, configurator=None, ssid=None,
                          extra=None):
        cmd = "DPP_BOOTSTRAP_SET %d" % id
        if ssid:
            cmd += " ssid=" + binascii.hexlify(ssid.encode()).decode()
        if extra:
            cmd += " " + extra
        if conf:
            cmd += " conf=" + conf
        if configurator is not None:
            cmd += " configurator=%d" % configurator
        if "OK" not in self.request(cmd):
            raise Exception("Failed to set bootstrapping parameters")

    def dpp_listen(self, freq, netrole=None, qr=None, role=None):
        cmd = "DPP_LISTEN " + str(freq)
        if netrole:
            cmd += " netrole=" + netrole
        if qr:
            cmd += " qr=" + qr
        if role:
            cmd += " role=" + role
        if "OK" not in self.request(cmd):
            raise Exception("Failed to start listen operation")

    def dpp_auth_init(self, peer=None, uri=None, conf=None, configurator=None,
                      extra=None, own=None, role=None, neg_freq=None,
                      ssid=None, passphrase=None, expect_fail=False,
                      conn_status=False, nfc_uri=None):
        cmd = "DPP_AUTH_INIT"
        if peer is None:
            if nfc_uri:
                peer = self.dpp_nfc_uri(nfc_uri)
            else:
                peer = self.dpp_qr_code(uri)
        cmd += " peer=%d" % peer
        if own is not None:
            cmd += " own=%d" % own
        if role:
            cmd += " role=" + role
        if extra:
            cmd += " " + extra
        if conf:
            cmd += " conf=" + conf
        if configurator is not None:
            cmd += " configurator=%d" % configurator
        if neg_freq:
            cmd += " neg_freq=%d" % neg_freq
        if ssid:
            cmd += " ssid=" + binascii.hexlify(ssid.encode()).decode()
        if passphrase:
            cmd += " pass=" + binascii.hexlify(passphrase.encode()).decode()
        if conn_status:
            cmd += " conn_status=1"
        res = self.request(cmd)
        if expect_fail:
            if "FAIL" not in res:
                raise Exception("DPP authentication started unexpectedly")
            return
        if "OK" not in res:
            raise Exception("Failed to initiate DPP Authentication")

    def dpp_pkex_init(self, identifier, code, role=None, key=None, curve=None,
                      extra=None, use_id=None, v2=False):
        if use_id is None:
            id1 = self.dpp_bootstrap_gen(type="pkex", key=key, curve=curve)
        else:
            id1 = use_id
        cmd = "own=%d " % id1
        if identifier:
            cmd += "identifier=%s " % identifier
        if v2:
            cmd += "init=2 "
        else:
            cmd += "init=1 "
        if role:
            cmd += "role=%s " % role
        if extra:
            cmd += extra + " "
        cmd += "code=%s" % code
        res = self.request("DPP_PKEX_ADD " + cmd)
        if "FAIL" in res:
            raise Exception("Failed to set PKEX data (initiator)")
        return id1

    def dpp_pkex_resp(self, freq, identifier, code, key=None, curve=None,
                      listen_role=None):
        id0 = self.dpp_bootstrap_gen(type="pkex", key=key, curve=curve)
        cmd = "own=%d " % id0
        if identifier:
            cmd += "identifier=%s " % identifier
        cmd += "code=%s" % code
        res = self.request("DPP_PKEX_ADD " + cmd)
        if "FAIL" in res:
            raise Exception("Failed to set PKEX data (responder)")
        self.dpp_listen(freq, role=listen_role)

    def dpp_configurator_add(self, curve=None, key=None):
        cmd = "DPP_CONFIGURATOR_ADD"
        if curve:
            cmd += " curve=" + curve
        if key:
            cmd += " key=" + key
        res = self.request(cmd)
        if "FAIL" in res:
            raise Exception("Failed to add configurator")
        return int(res)

    def dpp_configurator_remove(self, conf_id):
        res = self.request("DPP_CONFIGURATOR_REMOVE %d" % conf_id)
        if "OK" not in res:
            raise Exception("DPP_CONFIGURATOR_REMOVE failed")

    def note(self, txt):
        self.request("NOTE " + txt)

    def send_file(self, src, dst):
        self.host.send_file(src, dst)

    def get_ptksa(self, bssid, cipher):
        res = self.request("PTKSA_CACHE_LIST")
        lines = res.splitlines()
        for l in lines:
            if bssid not in l or cipher not in l:
                continue
            vals = dict()
            [index, addr, cipher, expiration, tk, kdk] = l.split(' ', 5)
            vals['index'] = index
            vals['addr'] = addr
            vals['cipher'] = cipher
            vals['expiration'] = expiration
            vals['tk'] = tk
            vals['kdk'] = kdk
            return vals
        return None

def add_ap(apdev, params, wait_enabled=True, no_enable=False, timeout=30,
           global_ctrl_override=None, driver=False):
        if isinstance(apdev, dict):
            ifname = apdev['ifname']
            try:
                hostname = apdev['hostname']
                port = apdev['port']
                logger.info("Starting AP " + hostname + "/" + port + " " + ifname)
            except:
                logger.info("Starting AP " + ifname)
                hostname = None
                port = 8878
        else:
            ifname = apdev
            logger.info("Starting AP " + ifname + " (old add_ap argument type)")
            hostname = None
            port = 8878
        hapd_global = HostapdGlobal(apdev,
                                    global_ctrl_override=global_ctrl_override)
        hapd_global.remove(ifname)
        hapd_global.add(ifname, driver=driver)
        port = hapd_global.get_ctrl_iface_port(ifname)
        hapd = Hostapd(ifname, hostname=hostname, port=port)
        if not hapd.ping():
            raise Exception("Could not ping hostapd")
        hapd.set_defaults()
        fields = ["ssid", "wpa_passphrase", "nas_identifier", "wpa_key_mgmt",
                  "wpa", "wpa_deny_ptk0_rekey",
                  "wpa_pairwise", "rsn_pairwise", "auth_server_addr",
                  "acct_server_addr", "osu_server_uri"]
        for field in fields:
            if field in params:
                hapd.set(field, params[field])
        for f, v in list(params.items()):
            if f in fields:
                continue
            if isinstance(v, list):
                for val in v:
                    hapd.set(f, val)
            else:
                hapd.set(f, v)
        if no_enable:
            return hapd
        hapd.enable()
        if wait_enabled:
            ev = hapd.wait_event(["AP-ENABLED", "AP-DISABLED"], timeout=timeout)
            if ev is None:
                raise Exception("AP startup timed out")
            if "AP-ENABLED" not in ev:
                raise Exception("AP startup failed")
        return hapd

def add_bss(apdev, ifname, confname, ignore_error=False):
    phy = utils.get_phy(apdev)
    try:
        hostname = apdev['hostname']
        port = apdev['port']
        logger.info("Starting BSS " + hostname + "/" + port + " phy=" + phy + " ifname=" + ifname)
    except:
        logger.info("Starting BSS phy=" + phy + " ifname=" + ifname)
        hostname = None
        port = 8878
    hapd_global = HostapdGlobal(apdev)
    confname = cfg_file(apdev, confname, ifname)
    hapd_global.send_file(confname, confname)
    hapd_global.add_bss(phy, confname, ignore_error)
    port = hapd_global.get_ctrl_iface_port(ifname)
    hapd = Hostapd(ifname, hostname=hostname, port=port)
    if not hapd.ping():
        raise Exception("Could not ping hostapd")
    return hapd

def add_iface(apdev, confname):
    ifname = apdev['ifname']
    try:
        hostname = apdev['hostname']
        port = apdev['port']
        logger.info("Starting interface " + hostname + "/" + port + " " + ifname)
    except:
        logger.info("Starting interface " + ifname)
        hostname = None
        port = 8878
    hapd_global = HostapdGlobal(apdev)
    confname = cfg_file(apdev, confname, ifname)
    hapd_global.send_file(confname, confname)
    hapd_global.add_iface(ifname, confname)
    port = hapd_global.get_ctrl_iface_port(ifname)
    hapd = Hostapd(ifname, hostname=hostname, port=port)
    if not hapd.ping():
        raise Exception("Could not ping hostapd")
    return hapd

def remove_bss(apdev, ifname=None):
    if ifname == None:
        ifname = apdev['ifname']
    try:
        hostname = apdev['hostname']
        port = apdev['port']
        logger.info("Removing BSS " + hostname + "/" + port + " " + ifname)
    except:
        logger.info("Removing BSS " + ifname)
    hapd_global = HostapdGlobal(apdev)
    hapd_global.remove(ifname)

def terminate(apdev):
    try:
        hostname = apdev['hostname']
        port = apdev['port']
        logger.info("Terminating hostapd " + hostname + "/" + port)
    except:
        logger.info("Terminating hostapd")
    hapd_global = HostapdGlobal(apdev)
    hapd_global.terminate()

def wpa2_params(ssid=None, passphrase=None, wpa_key_mgmt="WPA-PSK",
                ieee80211w=None):
    params = {"wpa": "2",
              "wpa_key_mgmt": wpa_key_mgmt,
              "rsn_pairwise": "CCMP"}
    if ssid:
        params["ssid"] = ssid
    if passphrase:
        params["wpa_passphrase"] = passphrase
    if ieee80211w is not None:
        params["ieee80211w"] = ieee80211w
    return params

def wpa_params(ssid=None, passphrase=None):
    params = {"wpa": "1",
              "wpa_key_mgmt": "WPA-PSK",
              "wpa_pairwise": "TKIP"}
    if ssid:
        params["ssid"] = ssid
    if passphrase:
        params["wpa_passphrase"] = passphrase
    return params

def wpa_mixed_params(ssid=None, passphrase=None):
    params = {"wpa": "3",
              "wpa_key_mgmt": "WPA-PSK",
              "wpa_pairwise": "TKIP",
              "rsn_pairwise": "CCMP"}
    if ssid:
        params["ssid"] = ssid
    if passphrase:
        params["wpa_passphrase"] = passphrase
    return params

def radius_params():
    params = {"auth_server_addr": "127.0.0.1",
              "auth_server_port": "1812",
              "auth_server_shared_secret": "radius",
              "nas_identifier": "nas.w1.fi"}
    return params

def wpa_eap_params(ssid=None):
    params = radius_params()
    params["wpa"] = "1"
    params["wpa_key_mgmt"] = "WPA-EAP"
    params["wpa_pairwise"] = "TKIP"
    params["ieee8021x"] = "1"
    if ssid:
        params["ssid"] = ssid
    return params

def wpa2_eap_params(ssid=None):
    params = radius_params()
    params["wpa"] = "2"
    params["wpa_key_mgmt"] = "WPA-EAP"
    params["rsn_pairwise"] = "CCMP"
    params["ieee8021x"] = "1"
    if ssid:
        params["ssid"] = ssid
    return params

def b_only_params(channel="1", ssid=None, country=None):
    params = {"hw_mode": "b",
              "channel": channel}
    if ssid:
        params["ssid"] = ssid
    if country:
        params["country_code"] = country
    return params

def g_only_params(channel="1", ssid=None, country=None):
    params = {"hw_mode": "g",
              "channel": channel}
    if ssid:
        params["ssid"] = ssid
    if country:
        params["country_code"] = country
    return params

def a_only_params(channel="36", ssid=None, country=None):
    params = {"hw_mode": "a",
              "channel": channel}
    if ssid:
        params["ssid"] = ssid
    if country:
        params["country_code"] = country
    return params

def ht20_params(channel="1", ssid=None, country=None):
    params = {"ieee80211n": "1",
              "channel": channel,
              "hw_mode": "g"}
    if int(channel) > 14:
        params["hw_mode"] = "a"
    if ssid:
        params["ssid"] = ssid
    if country:
        params["country_code"] = country
    return params

def ht40_plus_params(channel="1", ssid=None, country=None):
    params = ht20_params(channel, ssid, country)
    params['ht_capab'] = "[HT40+]"
    return params

def ht40_minus_params(channel="1", ssid=None, country=None):
    params = ht20_params(channel, ssid, country)
    params['ht_capab'] = "[HT40-]"
    return params

def cmd_execute(apdev, cmd, shell=False):
    hapd_global = HostapdGlobal(apdev)
    return hapd_global.cmd_execute(cmd, shell=shell)

def send_file(apdev, src, dst):
    hapd_global = HostapdGlobal(apdev)
    return hapd_global.send_file(src, dst)

def acl_file(dev, apdev, conf):
    fd, filename = tempfile.mkstemp(dir='/tmp', prefix=conf + '-')
    f = os.fdopen(fd, 'w')

    if conf == 'hostapd.macaddr':
        mac0 = dev[0].get_status_field("address")
        f.write(mac0 + '\n')
        f.write("02:00:00:00:00:12\n")
        f.write("02:00:00:00:00:34\n")
        f.write("-02:00:00:00:00:12\n")
        f.write("-02:00:00:00:00:34\n")
        f.write("01:01:01:01:01:01\n")
        f.write("03:01:01:01:01:03\n")
    elif conf == 'hostapd.accept':
        mac0 = dev[0].get_status_field("address")
        mac1 = dev[1].get_status_field("address")
        f.write(mac0 + "    1\n")
        f.write(mac1 + "    2\n")
    elif conf == 'hostapd.accept2':
        mac0 = dev[0].get_status_field("address")
        mac1 = dev[1].get_status_field("address")
        mac2 = dev[2].get_status_field("address")
        f.write(mac0 + "    1\n")
        f.write(mac1 + "    2\n")
        f.write(mac2 + "    3\n")
    else:
        f.close()
        os.unlink(filename)
        return conf

    return filename

def bssid_inc(apdev, inc=1):
    parts = apdev['bssid'].split(':')
    parts[5] = '%02x' % (int(parts[5], 16) + int(inc))
    bssid = '%s:%s:%s:%s:%s:%s' % (parts[0], parts[1], parts[2],
                                   parts[3], parts[4], parts[5])
    return bssid

def cfg_file(apdev, conf, ifname=None):
    match = re.search(r'^bss-.+', conf)
    if match:
        # put cfg file in /tmp directory
        fd, fname = tempfile.mkstemp(dir='/tmp', prefix=conf + '-')
        f = os.fdopen(fd, 'w')
        idx = ''.join(filter(str.isdigit, conf.split('-')[-1]))
        if ifname is None:
            ifname = apdev['ifname']
            if idx != '1':
                ifname = ifname + '-' + idx

        f.write("driver=nl80211\n")
        f.write("ctrl_interface=/var/run/hostapd\n")
        f.write("hw_mode=g\n")
        f.write("channel=1\n")
        f.write("ieee80211n=1\n")
        if conf.startswith('bss-ht40-'):
            f.write("ht_capab=[HT40+]\n")
        f.write("interface=%s\n" % ifname)

        f.write("ssid=bss-%s\n" % idx)
        if conf == 'bss-2-dup.conf':
            bssid = apdev['bssid']
        else:
            bssid = bssid_inc(apdev, int(idx) - 1)
        f.write("bssid=%s\n" % bssid)

        return fname

    return conf
