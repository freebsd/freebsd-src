# Python class for controlling wpa_supplicant
# Copyright (c) 2013-2019, Jouni Malinen <j@w1.fi>
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

import os
import time
import logging
import binascii
import re
import struct
import wpaspy
import remotehost
import subprocess

logger = logging.getLogger()
wpas_ctrl = '/var/run/wpa_supplicant'

class WpaSupplicant:
    def __init__(self, ifname=None, global_iface=None, hostname=None,
                 port=9877, global_port=9878, monitor=True):
        self.monitor = monitor
        self.hostname = hostname
        self.group_ifname = None
        self.global_mon = None
        self.global_ctrl = None
        self.gctrl_mon = None
        self.ctrl = None
        self.mon = None
        self.ifname = None
        self.host = remotehost.Host(hostname, ifname)
        self._group_dbg = None
        if ifname:
            self.set_ifname(ifname, hostname, port)
            res = self.get_driver_status()
            if 'capa.flags' in res and int(res['capa.flags'], 0) & 0x20000000:
                self.p2p_dev_ifname = 'p2p-dev-' + self.ifname
            else:
                self.p2p_dev_ifname = ifname

        self.global_iface = global_iface
        if global_iface:
            if hostname != None:
                self.global_ctrl = wpaspy.Ctrl(hostname, global_port)
                if self.monitor:
                    self.global_mon = wpaspy.Ctrl(hostname, global_port)
                self.global_dbg = hostname + "/" + str(global_port) + "/"
            else:
                self.global_ctrl = wpaspy.Ctrl(global_iface)
                if self.monitor:
                    self.global_mon = wpaspy.Ctrl(global_iface)
                self.global_dbg = ""
            if self.monitor:
                self.global_mon.attach()

    def __del__(self):
        self.close_monitor()
        self.close_control()

    def close_control_ctrl(self):
        if self.ctrl:
            del self.ctrl
            self.ctrl = None

    def close_control_global(self):
        if self.global_ctrl:
            del self.global_ctrl
            self.global_ctrl = None

    def close_control(self):
        self.close_control_ctrl()
        self.close_control_global()

    def close_monitor_mon(self):
        if not self.mon:
            return
        try:
            while self.mon.pending():
                ev = self.mon.recv()
                logger.debug(self.dbg + ": " + ev)
        except:
            pass
        try:
            self.mon.detach()
        except ConnectionRefusedError:
            pass
        except Exception as e:
            if str(e) == "DETACH failed":
                pass
            else:
                raise
        del self.mon
        self.mon = None

    def close_monitor_global(self):
        if not self.global_mon:
            return
        try:
            while self.global_mon.pending():
                ev = self.global_mon.recv()
                logger.debug(self.global_dbg + ": " + ev)
        except:
            pass
        try:
            self.global_mon.detach()
        except ConnectionRefusedError:
            pass
        except Exception as e:
            if str(e) == "DETACH failed":
                pass
            else:
                raise
        del self.global_mon
        self.global_mon = None

    def close_monitor_group(self):
        if not self.gctrl_mon:
            return
        try:
            while self.gctrl_mon.pending():
                ev = self.gctrl_mon.recv()
                logger.debug(self.dbg + ": " + ev)
        except:
            pass
        try:
            self.gctrl_mon.detach()
        except:
            pass
        del self.gctrl_mon
        self.gctrl_mon = None

    def close_monitor(self):
        self.close_monitor_mon()
        self.close_monitor_global()
        self.close_monitor_group()

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

    def terminate(self):
        if self.global_mon:
            self.close_monitor_global()
            self.global_ctrl.terminate()
            self.global_ctrl = None

    def close_ctrl(self):
        self.close_monitor_global()
        self.close_control_global()
        self.remove_ifname()

    def set_ifname(self, ifname, hostname=None, port=9877):
        self.remove_ifname()
        self.ifname = ifname
        if hostname != None:
            self.ctrl = wpaspy.Ctrl(hostname, port)
            if self.monitor:
                self.mon = wpaspy.Ctrl(hostname, port)
            self.host = remotehost.Host(hostname, ifname)
            self.dbg = hostname + "/" + ifname
        else:
            self.ctrl = wpaspy.Ctrl(os.path.join(wpas_ctrl, ifname))
            if self.monitor:
                self.mon = wpaspy.Ctrl(os.path.join(wpas_ctrl, ifname))
            self.dbg = ifname
        if self.monitor:
            self.mon.attach()

    def remove_ifname(self):
        self.close_monitor_mon()
        self.close_control_ctrl()
        self.ifname = None

    def get_ctrl_iface_port(self, ifname):
        if self.hostname is None:
            return None

        res = self.global_request("INTERFACES ctrl")
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

    def interface_add(self, ifname, config="", driver="nl80211",
                      drv_params=None, br_ifname=None, create=False,
                      set_ifname=True, all_params=False, if_type=None):
        status, groups = self.host.execute(["id"])
        if status != 0:
            group = "admin"
        group = "admin" if "(admin)" in groups else "adm"
        cmd = "INTERFACE_ADD " + ifname + "\t" + config + "\t" + driver + "\tDIR=/var/run/wpa_supplicant GROUP=" + group
        if drv_params:
            cmd = cmd + '\t' + drv_params
        if br_ifname:
            if not drv_params:
                cmd += '\t'
            cmd += '\t' + br_ifname
        if create:
            if not br_ifname:
                cmd += '\t'
                if not drv_params:
                    cmd += '\t'
            cmd += '\tcreate'
            if if_type:
                cmd += '\t' + if_type
        if all_params and not create:
            if not br_ifname:
                cmd += '\t'
                if not drv_params:
                    cmd += '\t'
            cmd += '\t'
        if "FAIL" in self.global_request(cmd):
            raise Exception("Failed to add a dynamic wpa_supplicant interface")
        if not create and set_ifname:
            port = self.get_ctrl_iface_port(ifname)
            self.set_ifname(ifname, self.hostname, port)
            res = self.get_driver_status()
            if 'capa.flags' in res and int(res['capa.flags'], 0) & 0x20000000:
                self.p2p_dev_ifname = 'p2p-dev-' + self.ifname
            else:
                self.p2p_dev_ifname = ifname

    def interface_remove(self, ifname):
        self.remove_ifname()
        self.global_request("INTERFACE_REMOVE " + ifname)

    def request(self, cmd, timeout=10):
        logger.debug(self.dbg + ": CTRL: " + cmd)
        return self.ctrl.request(cmd, timeout=timeout)

    def global_request(self, cmd):
        if self.global_iface is None:
            return self.request(cmd)
        else:
            ifname = self.ifname or self.global_iface
            logger.debug(self.global_dbg + ifname + ": CTRL(global): " + cmd)
            return self.global_ctrl.request(cmd)

    @property
    def group_dbg(self):
        if self._group_dbg is not None:
            return self._group_dbg
        if self.group_ifname is None:
            raise Exception("Cannot have group_dbg without group_ifname")
        if self.hostname is None:
            self._group_dbg = self.group_ifname
        else:
            self._group_dbg = self.hostname + "/" + self.group_ifname
        return self._group_dbg

    def group_request(self, cmd):
        if self.group_ifname and self.group_ifname != self.ifname:
            if self.hostname is None:
                gctrl = wpaspy.Ctrl(os.path.join(wpas_ctrl, self.group_ifname))
            else:
                port = self.get_ctrl_iface_port(self.group_ifname)
                gctrl = wpaspy.Ctrl(self.hostname, port)
            logger.debug(self.group_dbg + ": CTRL(group): " + cmd)
            return gctrl.request(cmd)
        return self.request(cmd)

    def ping(self):
        return "PONG" in self.request("PING")

    def global_ping(self):
        return "PONG" in self.global_request("PING")

    def reset(self):
        self.dump_monitor()
        res = self.request("FLUSH")
        if "OK" not in res:
            logger.info("FLUSH to " + self.ifname + " failed: " + res)
        self.global_request("REMOVE_NETWORK all")
        self.global_request("SET p2p_no_group_iface 1")
        self.global_request("P2P_FLUSH")
        self.close_monitor_group()
        self.group_ifname = None
        self.dump_monitor()

        iter = 0
        while iter < 60:
            state1 = self.get_driver_status_field("scan_state")
            p2pdev = "p2p-dev-" + self.ifname
            state2 = self.get_driver_status_field("scan_state", ifname=p2pdev)
            states = str(state1) + " " + str(state2)
            if "SCAN_STARTED" in states or "SCAN_REQUESTED" in states:
                logger.info(self.ifname + ": Waiting for scan operation to complete before continuing")
                time.sleep(1)
            else:
                break
            iter = iter + 1
        if iter == 60:
            logger.error(self.ifname + ": Driver scan state did not clear")
            print("Trying to clear cfg80211/mac80211 scan state")
            status, buf = self.host.execute(["ifconfig", self.ifname, "down"])
            if status != 0:
                logger.info("ifconfig failed: " + buf)
                logger.info(status)
            status, buf = self.host.execute(["ifconfig", self.ifname, "up"])
            if status != 0:
                logger.info("ifconfig failed: " + buf)
                logger.info(status)
        if iter > 0:
            # The ongoing scan could have discovered BSSes or P2P peers
            logger.info("Run FLUSH again since scan was in progress")
            self.request("FLUSH")
            self.dump_monitor()

        if not self.ping():
            logger.info("No PING response from " + self.ifname + " after reset")

    def set(self, field, value, allow_fail=False):
        if "OK" not in self.request("SET " + field + " " + value):
            if allow_fail:
                return
            raise Exception("Failed to set wpa_supplicant parameter " + field)

    def add_network(self):
        id = self.request("ADD_NETWORK")
        if "FAIL" in id:
            raise Exception("ADD_NETWORK failed")
        return int(id)

    def remove_network(self, id):
        id = self.request("REMOVE_NETWORK " + str(id))
        if "FAIL" in id:
            raise Exception("REMOVE_NETWORK failed")
        return None

    def get_network(self, id, field):
        res = self.request("GET_NETWORK " + str(id) + " " + field)
        if res == "FAIL\n":
            return None
        return res

    def set_network(self, id, field, value):
        res = self.request("SET_NETWORK " + str(id) + " " + field + " " + value)
        if "FAIL" in res:
            raise Exception("SET_NETWORK failed")
        return None

    def set_network_quoted(self, id, field, value):
        res = self.request("SET_NETWORK " + str(id) + " " + field + ' "' + value + '"')
        if "FAIL" in res:
            raise Exception("SET_NETWORK failed")
        return None

    def p2pdev_request(self, cmd):
        return self.global_request("IFNAME=" + self.p2p_dev_ifname + " " + cmd)

    def p2pdev_add_network(self):
        id = self.p2pdev_request("ADD_NETWORK")
        if "FAIL" in id:
            raise Exception("p2pdev ADD_NETWORK failed")
        return int(id)

    def p2pdev_set_network(self, id, field, value):
        res = self.p2pdev_request("SET_NETWORK " + str(id) + " " + field + " " + value)
        if "FAIL" in res:
            raise Exception("p2pdev SET_NETWORK failed")
        return None

    def p2pdev_set_network_quoted(self, id, field, value):
        res = self.p2pdev_request("SET_NETWORK " + str(id) + " " + field + ' "' + value + '"')
        if "FAIL" in res:
            raise Exception("p2pdev SET_NETWORK failed")
        return None

    def list_networks(self, p2p=False):
        if p2p:
            res = self.global_request("LIST_NETWORKS")
        else:
            res = self.request("LIST_NETWORKS")
        lines = res.splitlines()
        networks = []
        for l in lines:
            if "network id" in l:
                continue
            [id, ssid, bssid, flags] = l.split('\t')
            network = {}
            network['id'] = id
            network['ssid'] = ssid
            network['bssid'] = bssid
            network['flags'] = flags
            networks.append(network)
        return networks

    def hs20_enable(self, auto_interworking=False):
        self.request("SET interworking 1")
        self.request("SET hs20 1")
        if auto_interworking:
            self.request("SET auto_interworking 1")
        else:
            self.request("SET auto_interworking 0")

    def interworking_add_network(self, bssid):
        id = self.request("INTERWORKING_ADD_NETWORK " + bssid)
        if "FAIL" in id or "OK" in id:
            raise Exception("INTERWORKING_ADD_NETWORK failed")
        return int(id)

    def add_cred(self):
        id = self.request("ADD_CRED")
        if "FAIL" in id:
            raise Exception("ADD_CRED failed")
        return int(id)

    def remove_cred(self, id):
        id = self.request("REMOVE_CRED " + str(id))
        if "FAIL" in id:
            raise Exception("REMOVE_CRED failed")
        return None

    def set_cred(self, id, field, value):
        res = self.request("SET_CRED " + str(id) + " " + field + " " + value)
        if "FAIL" in res:
            raise Exception("SET_CRED failed")
        return None

    def set_cred_quoted(self, id, field, value):
        res = self.request("SET_CRED " + str(id) + " " + field + ' "' + value + '"')
        if "FAIL" in res:
            raise Exception("SET_CRED failed")
        return None

    def get_cred(self, id, field):
        return self.request("GET_CRED " + str(id) + " " + field)

    def add_cred_values(self, params):
        id = self.add_cred()

        quoted = ["realm", "username", "password", "domain", "imsi",
                  "excluded_ssid", "milenage", "ca_cert", "client_cert",
                  "private_key", "domain_suffix_match", "provisioning_sp",
                  "roaming_partner", "phase1", "phase2", "private_key_passwd",
                  "roaming_consortiums"]
        for field in quoted:
            if field in params:
                self.set_cred_quoted(id, field, params[field])

        not_quoted = ["eap", "roaming_consortium", "priority",
                      "required_roaming_consortium", "sp_priority",
                      "max_bss_load", "update_identifier", "req_conn_capab",
                      "min_dl_bandwidth_home", "min_ul_bandwidth_home",
                      "min_dl_bandwidth_roaming", "min_ul_bandwidth_roaming"]
        for field in not_quoted:
            if field in params:
                self.set_cred(id, field, params[field])

        return id

    def select_network(self, id, freq=None):
        if freq:
            extra = " freq=" + str(freq)
        else:
            extra = ""
        id = self.request("SELECT_NETWORK " + str(id) + extra)
        if "FAIL" in id:
            raise Exception("SELECT_NETWORK failed")
        return None

    def mesh_group_add(self, id):
        id = self.request("MESH_GROUP_ADD " + str(id))
        if "FAIL" in id:
            raise Exception("MESH_GROUP_ADD failed")
        return None

    def mesh_group_remove(self):
        id = self.request("MESH_GROUP_REMOVE " + str(self.ifname))
        if "FAIL" in id:
            raise Exception("MESH_GROUP_REMOVE failed")
        return None

    def connect_network(self, id, timeout=None):
        if timeout is None:
            timeout = 10 if self.hostname is None else 60
        self.dump_monitor()
        self.select_network(id)
        self.wait_connected(timeout=timeout)
        self.dump_monitor()

    def get_status(self, extra=None):
        if extra:
            extra = "-" + extra
        else:
            extra = ""
        res = self.request("STATUS" + extra)
        lines = res.splitlines()
        vals = dict()
        for l in lines:
            try:
                [name, value] = l.split('=', 1)
                vals[name] = value
            except ValueError as e:
                logger.info(self.ifname + ": Ignore unexpected STATUS line: " + l)
        return vals

    def get_status_field(self, field, extra=None):
        vals = self.get_status(extra)
        if field in vals:
            return vals[field]
        return None

    def get_group_status(self, extra=None):
        if extra:
            extra = "-" + extra
        else:
            extra = ""
        res = self.group_request("STATUS" + extra)
        lines = res.splitlines()
        vals = dict()
        for l in lines:
            try:
                [name, value] = l.split('=', 1)
            except ValueError:
                logger.info(self.ifname + ": Ignore unexpected status line: " + l)
                continue
            vals[name] = value
        return vals

    def get_group_status_field(self, field, extra=None):
        vals = self.get_group_status(extra)
        if field in vals:
            return vals[field]
        return None

    def get_driver_status(self, ifname=None):
        if ifname is None:
            res = self.request("STATUS-DRIVER")
        else:
            res = self.global_request("IFNAME=%s STATUS-DRIVER" % ifname)
            if res.startswith("FAIL"):
                return dict()
        lines = res.splitlines()
        vals = dict()
        for l in lines:
            try:
                [name, value] = l.split('=', 1)
            except ValueError:
                logger.info(self.ifname + ": Ignore unexpected status-driver line: " + l)
                continue
            vals[name] = value
        return vals

    def get_driver_status_field(self, field, ifname=None):
        vals = self.get_driver_status(ifname)
        if field in vals:
            return vals[field]
        return None

    def get_mcc(self):
        mcc = int(self.get_driver_status_field('capa.num_multichan_concurrent'))
        return 1 if mcc < 2 else mcc

    def get_mib(self):
        res = self.request("MIB")
        lines = res.splitlines()
        vals = dict()
        for l in lines:
            try:
                [name, value] = l.split('=', 1)
                vals[name] = value
            except ValueError as e:
                logger.info(self.ifname + ": Ignore unexpected MIB line: " + l)
        return vals

    def p2p_dev_addr(self):
        return self.get_status_field("p2p_device_address")

    def p2p_interface_addr(self):
        return self.get_group_status_field("address")

    def own_addr(self):
        try:
            res = self.p2p_interface_addr()
        except:
            res = self.p2p_dev_addr()
        return res

    def get_addr(self, group=False):
        dev_addr = self.own_addr()
        if not group:
            addr = self.get_status_field('address')
            if addr:
                dev_addr = addr

        return dev_addr

    def p2p_listen(self):
        return self.global_request("P2P_LISTEN")

    def p2p_ext_listen(self, period, interval):
        return self.global_request("P2P_EXT_LISTEN %d %d" % (period, interval))

    def p2p_cancel_ext_listen(self):
        return self.global_request("P2P_EXT_LISTEN")

    def p2p_find(self, social=False, progressive=False, dev_id=None,
                 dev_type=None, delay=None, freq=None):
        cmd = "P2P_FIND"
        if social:
            cmd = cmd + " type=social"
        elif progressive:
            cmd = cmd + " type=progressive"
        if dev_id:
            cmd = cmd + " dev_id=" + dev_id
        if dev_type:
            cmd = cmd + " dev_type=" + dev_type
        if delay:
            cmd = cmd + " delay=" + str(delay)
        if freq:
            cmd = cmd + " freq=" + str(freq)
        return self.global_request(cmd)

    def p2p_stop_find(self):
        return self.global_request("P2P_STOP_FIND")

    def wps_read_pin(self):
        self.pin = self.request("WPS_PIN get").rstrip("\n")
        if "FAIL" in self.pin:
            raise Exception("Could not generate PIN")
        return self.pin

    def peer_known(self, peer, full=True):
        res = self.global_request("P2P_PEER " + peer)
        if peer.lower() not in res.lower():
            return False
        if not full:
            return True
        return "[PROBE_REQ_ONLY]" not in res

    def discover_peer(self, peer, full=True, timeout=15, social=True,
                      force_find=False, freq=None):
        logger.info(self.ifname + ": Trying to discover peer " + peer)
        if not force_find and self.peer_known(peer, full):
            return True
        self.p2p_find(social, freq=freq)
        count = 0
        while count < timeout * 4:
            time.sleep(0.25)
            count = count + 1
            if self.peer_known(peer, full):
                return True
        return False

    def get_peer(self, peer):
        res = self.global_request("P2P_PEER " + peer)
        if peer.lower() not in res.lower():
            raise Exception("Peer information not available")
        lines = res.splitlines()
        vals = dict()
        for l in lines:
            if '=' in l:
                [name, value] = l.split('=', 1)
                vals[name] = value
        return vals

    def group_form_result(self, ev, expect_failure=False, go_neg_res=None):
        if expect_failure:
            if "P2P-GROUP-STARTED" in ev:
                raise Exception("Group formation succeeded when expecting failure")
            exp = r'<.>(P2P-GO-NEG-FAILURE) status=([0-9]*)'
            s = re.split(exp, ev)
            if len(s) < 3:
                return None
            res = {}
            res['result'] = 'go-neg-failed'
            res['status'] = int(s[2])
            return res

        if "P2P-GROUP-STARTED" not in ev:
            raise Exception("No P2P-GROUP-STARTED event seen")

        exp = r'<.>(P2P-GROUP-STARTED) ([^ ]*) ([^ ]*) ssid="(.*)" freq=([0-9]*) ((?:psk=.*)|(?:passphrase=".*")) go_dev_addr=([0-9a-f:]*) ip_addr=([0-9.]*) ip_mask=([0-9.]*) go_ip_addr=([0-9.]*)'
        s = re.split(exp, ev)
        if len(s) < 11:
            exp = r'<.>(P2P-GROUP-STARTED) ([^ ]*) ([^ ]*) ssid="(.*)" freq=([0-9]*) ((?:psk=.*)|(?:passphrase=".*")) go_dev_addr=([0-9a-f:]*)'
            s = re.split(exp, ev)
            if len(s) < 8:
                raise Exception("Could not parse P2P-GROUP-STARTED")
        res = {}
        res['result'] = 'success'
        res['ifname'] = s[2]
        self.group_ifname = s[2]
        try:
            if self.hostname is None:
                self.gctrl_mon = wpaspy.Ctrl(os.path.join(wpas_ctrl,
                                                          self.group_ifname))
            else:
                port = self.get_ctrl_iface_port(self.group_ifname)
                self.gctrl_mon = wpaspy.Ctrl(self.hostname, port)
            if self.monitor:
                self.gctrl_mon.attach()
        except:
            logger.debug("Could not open monitor socket for group interface")
            self.gctrl_mon = None
        res['role'] = s[3]
        res['ssid'] = s[4]
        res['freq'] = s[5]
        if "[PERSISTENT]" in ev:
            res['persistent'] = True
        else:
            res['persistent'] = False
        p = re.match(r'psk=([0-9a-f]*)', s[6])
        if p:
            res['psk'] = p.group(1)
        p = re.match(r'passphrase="(.*)"', s[6])
        if p:
            res['passphrase'] = p.group(1)
        res['go_dev_addr'] = s[7]

        if len(s) > 8 and len(s[8]) > 0 and "[PERSISTENT]" not in s[8]:
            res['ip_addr'] = s[8]
        if len(s) > 9:
            res['ip_mask'] = s[9]
        if len(s) > 10:
            res['go_ip_addr'] = s[10]

        if go_neg_res:
            exp = r'<.>(P2P-GO-NEG-SUCCESS) role=(GO|client) freq=([0-9]*)'
            s = re.split(exp, go_neg_res)
            if len(s) < 4:
                raise Exception("Could not parse P2P-GO-NEG-SUCCESS")
            res['go_neg_role'] = s[2]
            res['go_neg_freq'] = s[3]

        return res

    def p2p_go_neg_auth(self, peer, pin, method, go_intent=None,
                        persistent=False, freq=None, freq2=None,
                        max_oper_chwidth=None, ht40=False, vht=False):
        if not self.discover_peer(peer):
            raise Exception("Peer " + peer + " not found")
        self.dump_monitor()
        if pin:
            cmd = "P2P_CONNECT " + peer + " " + pin + " " + method + " auth"
        else:
            cmd = "P2P_CONNECT " + peer + " " + method + " auth"
        if go_intent:
            cmd = cmd + ' go_intent=' + str(go_intent)
        if freq:
            cmd = cmd + ' freq=' + str(freq)
        if freq2:
            cmd = cmd + ' freq2=' + str(freq2)
        if max_oper_chwidth:
            cmd = cmd + ' max_oper_chwidth=' + str(max_oper_chwidth)
        if ht40:
            cmd = cmd + ' ht40'
        if vht:
            cmd = cmd + ' vht'
        if persistent:
            cmd = cmd + " persistent"
        if "OK" in self.global_request(cmd):
            return None
        raise Exception("P2P_CONNECT (auth) failed")

    def p2p_go_neg_auth_result(self, timeout=None, expect_failure=False):
        if timeout is None:
            timeout = 1 if expect_failure else 5
        go_neg_res = None
        ev = self.wait_global_event(["P2P-GO-NEG-SUCCESS",
                                     "P2P-GO-NEG-FAILURE"], timeout)
        if ev is None:
            if expect_failure:
                return None
            raise Exception("Group formation timed out")
        if "P2P-GO-NEG-SUCCESS" in ev:
            go_neg_res = ev
            ev = self.wait_global_event(["P2P-GROUP-STARTED"], timeout)
            if ev is None:
                if expect_failure:
                    return None
                raise Exception("Group formation timed out")
        self.dump_monitor()
        return self.group_form_result(ev, expect_failure, go_neg_res)

    def p2p_go_neg_init(self, peer, pin, method, timeout=0, go_intent=None,
                        expect_failure=False, persistent=False,
                        persistent_id=None, freq=None, provdisc=False,
                        wait_group=True, freq2=None, max_oper_chwidth=None,
                        ht40=False, vht=False):
        if not self.discover_peer(peer):
            raise Exception("Peer " + peer + " not found")
        self.dump_monitor()
        if pin:
            cmd = "P2P_CONNECT " + peer + " " + pin + " " + method
        else:
            cmd = "P2P_CONNECT " + peer + " " + method
        if go_intent is not None:
            cmd = cmd + ' go_intent=' + str(go_intent)
        if freq:
            cmd = cmd + ' freq=' + str(freq)
        if freq2:
            cmd = cmd + ' freq2=' + str(freq2)
        if max_oper_chwidth:
            cmd = cmd + ' max_oper_chwidth=' + str(max_oper_chwidth)
        if ht40:
            cmd = cmd + ' ht40'
        if vht:
            cmd = cmd + ' vht'
        if persistent:
            cmd = cmd + " persistent"
        elif persistent_id:
            cmd = cmd + " persistent=" + persistent_id
        if provdisc:
            cmd = cmd + " provdisc"
        if "OK" in self.global_request(cmd):
            if timeout == 0:
                return None
            go_neg_res = None
            ev = self.wait_global_event(["P2P-GO-NEG-SUCCESS",
                                         "P2P-GO-NEG-FAILURE"], timeout)
            if ev is None:
                if expect_failure:
                    return None
                raise Exception("Group formation timed out")
            if "P2P-GO-NEG-SUCCESS" in ev:
                if not wait_group:
                    return ev
                go_neg_res = ev
                ev = self.wait_global_event(["P2P-GROUP-STARTED"], timeout)
                if ev is None:
                    if expect_failure:
                        return None
                    raise Exception("Group formation timed out")
            self.dump_monitor()
            return self.group_form_result(ev, expect_failure, go_neg_res)
        raise Exception("P2P_CONNECT failed")

    def _wait_event(self, mon, pfx, events, timeout):
        if not isinstance(events, list):
            raise Exception("WpaSupplicant._wait_event() called with incorrect events argument type")
        start = os.times()[4]
        while True:
            while mon.pending():
                ev = mon.recv()
                logger.debug(self.dbg + pfx + ev)
                for event in events:
                    if event in ev:
                        return ev
            now = os.times()[4]
            remaining = start + timeout - now
            if remaining <= 0:
                break
            if not mon.pending(timeout=remaining):
                break
        return None

    def wait_event(self, events, timeout=10):
        return self._wait_event(self.mon, ": ", events, timeout)

    def wait_global_event(self, events, timeout):
        if self.global_iface is None:
            return self.wait_event(events, timeout)
        return self._wait_event(self.global_mon, "(global): ",
                                events, timeout)

    def wait_group_event(self, events, timeout=10):
        if not isinstance(events, list):
            raise Exception("WpaSupplicant.wait_group_event() called with incorrect events argument type")
        if self.group_ifname and self.group_ifname != self.ifname:
            if self.gctrl_mon is None:
                return None
            start = os.times()[4]
            while True:
                while self.gctrl_mon.pending():
                    ev = self.gctrl_mon.recv()
                    logger.debug(self.group_dbg + "(group): " + ev)
                    for event in events:
                        if event in ev:
                            return ev
                now = os.times()[4]
                remaining = start + timeout - now
                if remaining <= 0:
                    break
                if not self.gctrl_mon.pending(timeout=remaining):
                    break
            return None

        return self.wait_event(events, timeout)

    def wait_go_ending_session(self):
        self.close_monitor_group()
        timeout = 3 if self.hostname is None else 10
        ev = self.wait_global_event(["P2P-GROUP-REMOVED"], timeout=timeout)
        if ev is None:
            raise Exception("Group removal event timed out")
        if "reason=GO_ENDING_SESSION" not in ev:
            raise Exception("Unexpected group removal reason")

    def dump_monitor(self, mon=True, global_mon=True):
        count_iface = 0
        count_global = 0
        while mon and self.monitor and self.mon.pending():
            ev = self.mon.recv()
            logger.debug(self.dbg + ": " + ev)
            count_iface += 1
        while global_mon and self.monitor and self.global_mon and self.global_mon.pending():
            ev = self.global_mon.recv()
            logger.debug(self.global_dbg + self.ifname + "(global): " + ev)
            count_global += 1
        return (count_iface, count_global)

    def remove_group(self, ifname=None):
        self.close_monitor_group()
        if ifname is None:
            ifname = self.group_ifname if self.group_ifname else self.ifname
        if "OK" not in self.global_request("P2P_GROUP_REMOVE " + ifname):
            raise Exception("Group could not be removed")
        self.group_ifname = None

    def p2p_start_go(self, persistent=None, freq=None, no_event_clear=False):
        self.dump_monitor()
        cmd = "P2P_GROUP_ADD"
        if persistent is None:
            pass
        elif persistent is True:
            cmd = cmd + " persistent"
        else:
            cmd = cmd + " persistent=" + str(persistent)
        if freq:
            cmd = cmd + " freq=" + str(freq)
        if "OK" in self.global_request(cmd):
            ev = self.wait_global_event(["P2P-GROUP-STARTED"], timeout=5)
            if ev is None:
                raise Exception("GO start up timed out")
            if not no_event_clear:
                self.dump_monitor()
            return self.group_form_result(ev)
        raise Exception("P2P_GROUP_ADD failed")

    def p2p_go_authorize_client(self, pin):
        cmd = "WPS_PIN any " + pin
        if "FAIL" in self.group_request(cmd):
            raise Exception("Failed to authorize client connection on GO")
        return None

    def p2p_go_authorize_client_pbc(self):
        cmd = "WPS_PBC"
        if "FAIL" in self.group_request(cmd):
            raise Exception("Failed to authorize client connection on GO")
        return None

    def p2p_connect_group(self, go_addr, pin, timeout=0, social=False,
                          freq=None):
        self.dump_monitor()
        if not self.discover_peer(go_addr, social=social, freq=freq):
            if social or not self.discover_peer(go_addr, social=social):
                raise Exception("GO " + go_addr + " not found")
        self.p2p_stop_find()
        self.dump_monitor()
        cmd = "P2P_CONNECT " + go_addr + " " + pin + " join"
        if freq:
            cmd += " freq=" + str(freq)
        if "OK" in self.global_request(cmd):
            if timeout == 0:
                self.dump_monitor()
                return None
            ev = self.wait_global_event(["P2P-GROUP-STARTED",
                                         "P2P-GROUP-FORMATION-FAILURE"],
                                        timeout)
            if ev is None:
                raise Exception("Joining the group timed out")
            if "P2P-GROUP-STARTED" not in ev:
                raise Exception("Failed to join the group")
            self.dump_monitor()
            return self.group_form_result(ev)
        raise Exception("P2P_CONNECT(join) failed")

    def tdls_setup(self, peer):
        cmd = "TDLS_SETUP " + peer
        if "FAIL" in self.group_request(cmd):
            raise Exception("Failed to request TDLS setup")
        return None

    def tdls_teardown(self, peer):
        cmd = "TDLS_TEARDOWN " + peer
        if "FAIL" in self.group_request(cmd):
            raise Exception("Failed to request TDLS teardown")
        return None

    def tdls_link_status(self, peer):
        cmd = "TDLS_LINK_STATUS " + peer
        ret = self.group_request(cmd)
        if "FAIL" in ret:
            raise Exception("Failed to request TDLS link status")
        return ret

    def tspecs(self):
        """Return (tsid, up) tuples representing current tspecs"""
        res = self.request("WMM_AC_STATUS")
        tspecs = re.findall(r"TSID=(\d+) UP=(\d+)", res)
        tspecs = [tuple(map(int, tspec)) for tspec in tspecs]

        logger.debug("tspecs: " + str(tspecs))
        return tspecs

    def add_ts(self, tsid, up, direction="downlink", expect_failure=False,
               extra=None):
        params = {
            "sba": 9000,
            "nominal_msdu_size": 1500,
            "min_phy_rate": 6000000,
            "mean_data_rate": 1500,
        }
        cmd = "WMM_AC_ADDTS %s tsid=%d up=%d" % (direction, tsid, up)
        for (key, value) in params.items():
            cmd += " %s=%d" % (key, value)
        if extra:
            cmd += " " + extra

        if self.request(cmd).strip() != "OK":
            raise Exception("ADDTS failed (tsid=%d up=%d)" % (tsid, up))

        if expect_failure:
            ev = self.wait_event(["TSPEC-REQ-FAILED"], timeout=2)
            if ev is None:
                raise Exception("ADDTS failed (time out while waiting failure)")
            if "tsid=%d" % (tsid) not in ev:
                raise Exception("ADDTS failed (invalid tsid in TSPEC-REQ-FAILED")
            return

        ev = self.wait_event(["TSPEC-ADDED"], timeout=1)
        if ev is None:
            raise Exception("ADDTS failed (time out)")
        if "tsid=%d" % (tsid) not in ev:
            raise Exception("ADDTS failed (invalid tsid in TSPEC-ADDED)")

        if (tsid, up) not in self.tspecs():
            raise Exception("ADDTS failed (tsid not in tspec list)")

    def del_ts(self, tsid):
        if self.request("WMM_AC_DELTS %d" % (tsid)).strip() != "OK":
            raise Exception("DELTS failed")

        ev = self.wait_event(["TSPEC-REMOVED"], timeout=1)
        if ev is None:
            raise Exception("DELTS failed (time out)")
        if "tsid=%d" % (tsid) not in ev:
            raise Exception("DELTS failed (invalid tsid in TSPEC-REMOVED)")

        tspecs = [(t, u) for (t, u) in self.tspecs() if t == tsid]
        if tspecs:
            raise Exception("DELTS failed (still in tspec list)")

    def connect(self, ssid=None, ssid2=None, **kwargs):
        logger.info("Connect STA " + self.ifname + " to AP")
        id = self.add_network()
        if ssid:
            self.set_network_quoted(id, "ssid", ssid)
        elif ssid2:
            self.set_network(id, "ssid", ssid2)

        quoted = ["psk", "identity", "anonymous_identity", "password",
                  "machine_identity", "machine_password",
                  "ca_cert", "client_cert", "private_key",
                  "private_key_passwd", "ca_cert2", "client_cert2",
                  "private_key2", "phase1", "phase2", "domain_suffix_match",
                  "altsubject_match", "subject_match", "pac_file", "dh_file",
                  "bgscan", "ht_mcs", "id_str", "openssl_ciphers",
                  "domain_match", "dpp_connector", "sae_password",
                  "sae_password_id", "check_cert_subject",
                  "machine_ca_cert", "machine_client_cert",
                  "machine_private_key", "machine_phase2"]
        for field in quoted:
            if field in kwargs and kwargs[field]:
                self.set_network_quoted(id, field, kwargs[field])

        not_quoted = ["proto", "key_mgmt", "ieee80211w", "pairwise",
                      "group", "wep_key0", "wep_key1", "wep_key2", "wep_key3",
                      "wep_tx_keyidx", "scan_freq", "freq_list", "eap",
                      "eapol_flags", "fragment_size", "scan_ssid", "auth_alg",
                      "wpa_ptk_rekey", "disable_ht", "disable_vht", "bssid",
                      "disable_he",
                      "disable_max_amsdu", "ampdu_factor", "ampdu_density",
                      "disable_ht40", "disable_sgi", "disable_ldpc",
                      "ht40_intolerant", "update_identifier", "mac_addr",
                      "erp", "bg_scan_period", "bssid_ignore",
                      "bssid_accept", "mem_only_psk", "eap_workaround",
                      "engine", "fils_dh_group", "bssid_hint",
                      "dpp_csign", "dpp_csign_expiry",
                      "dpp_netaccesskey", "dpp_netaccesskey_expiry", "dpp_pfs",
                      "group_mgmt", "owe_group", "owe_only",
                      "owe_ptk_workaround",
                      "transition_disable", "sae_pk",
                      "roaming_consortium_selection", "ocv",
                      "multi_ap_backhaul_sta", "rx_stbc", "tx_stbc",
                      "ft_eap_pmksa_caching", "beacon_prot",
                      "wpa_deny_ptk0_rekey"]
        for field in not_quoted:
            if field in kwargs and kwargs[field]:
                self.set_network(id, field, kwargs[field])

        known_args = {"raw_psk", "password_hex", "peerkey", "okc", "ocsp",
                      "only_add_network", "wait_connect"}
        unknown = set(kwargs.keys())
        unknown -= set(quoted)
        unknown -= set(not_quoted)
        unknown -= known_args
        if unknown:
            raise Exception("Unknown WpaSupplicant::connect() arguments: " + str(unknown))

        if "raw_psk" in kwargs and kwargs['raw_psk']:
            self.set_network(id, "psk", kwargs['raw_psk'])
        if "password_hex" in kwargs and kwargs['password_hex']:
            self.set_network(id, "password", kwargs['password_hex'])
        if "peerkey" in kwargs and kwargs['peerkey']:
            self.set_network(id, "peerkey", "1")
        if "okc" in kwargs and kwargs['okc']:
            self.set_network(id, "proactive_key_caching", "1")
        if "ocsp" in kwargs and kwargs['ocsp']:
            self.set_network(id, "ocsp", str(kwargs['ocsp']))
        if "only_add_network" in kwargs and kwargs['only_add_network']:
            return id
        if "wait_connect" not in kwargs or kwargs['wait_connect']:
            if "eap" in kwargs:
                self.connect_network(id, timeout=20)
            else:
                self.connect_network(id)
        else:
            self.dump_monitor()
            self.select_network(id)
        return id

    def scan(self, type=None, freq=None, no_wait=False, only_new=False,
             passive=False):
        if not no_wait:
            self.dump_monitor()
        if type:
            cmd = "SCAN TYPE=" + type
        else:
            cmd = "SCAN"
        if freq:
            cmd = cmd + " freq=" + str(freq)
        if only_new:
            cmd += " only_new=1"
        if passive:
            cmd += " passive=1"
        if not no_wait:
            self.dump_monitor()
        res = self.request(cmd)
        if "OK" not in res:
            raise Exception("Failed to trigger scan: " + str(res))
        if no_wait:
            return
        ev = self.wait_event(["CTRL-EVENT-SCAN-RESULTS",
                              "CTRL-EVENT-SCAN-FAILED"], 15)
        if ev is None:
            raise Exception("Scan timed out")
        if "CTRL-EVENT-SCAN-FAILED" in ev:
            raise Exception("Scan failed: " + ev)

    def scan_for_bss(self, bssid, freq=None, force_scan=False, only_new=False,
                     passive=False):
        if not force_scan and self.get_bss(bssid) is not None:
            return
        for i in range(0, 10):
            self.scan(freq=freq, type="ONLY", only_new=only_new,
                      passive=passive)
            if self.get_bss(bssid) is not None:
                return
        raise Exception("Could not find BSS " + bssid + " in scan")

    def flush_scan_cache(self, freq=2417):
        self.request("BSS_FLUSH 0")
        self.scan(freq=freq, only_new=True)
        res = self.request("SCAN_RESULTS")
        if len(res.splitlines()) > 1:
            logger.debug("Scan results remaining after first attempt to flush the results:\n" + res)
            self.request("BSS_FLUSH 0")
            self.scan(freq=2422, only_new=True)
            res = self.request("SCAN_RESULTS")
            if len(res.splitlines()) > 1:
                logger.info("flush_scan_cache: Could not clear all BSS entries. These remain:\n" + res)

    def disconnect_and_stop_scan(self):
        self.request("DISCONNECT")
        res = self.request("ABORT_SCAN")
        for i in range(2 if "OK" in res else 1):
                self.wait_event(["CTRL-EVENT-DISCONNECTED",
                                 "CTRL-EVENT-SCAN-RESULTS"], timeout=0.5)
        self.dump_monitor()

    def roam(self, bssid, fail_test=False, assoc_reject_ok=False,
             check_bssid=True):
        self.dump_monitor()
        if "OK" not in self.request("ROAM " + bssid):
            raise Exception("ROAM failed")
        if fail_test:
            if assoc_reject_ok:
                ev = self.wait_event(["CTRL-EVENT-CONNECTED",
                                      "CTRL-EVENT-DISCONNECTED",
                                      "CTRL-EVENT-ASSOC-REJECT"], timeout=1)
            else:
                ev = self.wait_event(["CTRL-EVENT-CONNECTED",
                                      "CTRL-EVENT-DISCONNECTED"], timeout=1)
            if ev and "CTRL-EVENT-DISCONNECTED" in ev:
                self.dump_monitor()
                return
            if ev is not None and "CTRL-EVENT-ASSOC-REJECT" not in ev:
                raise Exception("Unexpected connection")
            self.dump_monitor()
            return
        if assoc_reject_ok:
            ev = self.wait_event(["CTRL-EVENT-CONNECTED",
                                  "CTRL-EVENT-DISCONNECTED"], timeout=10)
        else:
            ev = self.wait_event(["CTRL-EVENT-CONNECTED",
                                      "CTRL-EVENT-DISCONNECTED",
                                  "CTRL-EVENT-ASSOC-REJECT"], timeout=10)
        if ev is None:
            raise Exception("Roaming with the AP timed out")
        if "CTRL-EVENT-ASSOC-REJECT" in ev:
            raise Exception("Roaming association rejected")
        if "CTRL-EVENT-DISCONNECTED" in ev:
            raise Exception("Unexpected disconnection when waiting for roam to complete")
        self.dump_monitor()
        if check_bssid and self.get_status_field('bssid') != bssid:
            raise Exception("Did not roam to correct BSSID")

    def roam_over_ds(self, bssid, fail_test=False):
        self.dump_monitor()
        if "OK" not in self.request("FT_DS " + bssid):
            raise Exception("FT_DS failed")
        if fail_test:
            ev = self.wait_event(["CTRL-EVENT-CONNECTED"], timeout=1)
            if ev is not None:
                raise Exception("Unexpected connection")
            self.dump_monitor()
            return
        ev = self.wait_event(["CTRL-EVENT-CONNECTED",
                              "CTRL-EVENT-ASSOC-REJECT"], timeout=10)
        if ev is None:
            raise Exception("Roaming with the AP timed out")
        if "CTRL-EVENT-ASSOC-REJECT" in ev:
            raise Exception("Roaming association rejected")
        self.dump_monitor()

    def wps_reg(self, bssid, pin, new_ssid=None, key_mgmt=None, cipher=None,
                new_passphrase=None, no_wait=False):
        self.dump_monitor()
        if new_ssid:
            self.request("WPS_REG " + bssid + " " + pin + " " +
                         binascii.hexlify(new_ssid.encode()).decode() + " " +
                         key_mgmt + " " + cipher + " " +
                         binascii.hexlify(new_passphrase.encode()).decode())
            if no_wait:
                return
            ev = self.wait_event(["WPS-SUCCESS"], timeout=15)
        else:
            self.request("WPS_REG " + bssid + " " + pin)
            if no_wait:
                return
            ev = self.wait_event(["WPS-CRED-RECEIVED"], timeout=15)
            if ev is None:
                raise Exception("WPS cred timed out")
            ev = self.wait_event(["WPS-FAIL"], timeout=15)
        if ev is None:
            raise Exception("WPS timed out")
        self.wait_connected(timeout=15)

    def relog(self):
        self.global_request("RELOG")

    def wait_completed(self, timeout=10):
        for i in range(0, timeout * 2):
            if self.get_status_field("wpa_state") == "COMPLETED":
                return
            time.sleep(0.5)
        raise Exception("Timeout while waiting for COMPLETED state")

    def get_capability(self, field):
        res = self.request("GET_CAPABILITY " + field)
        if "FAIL" in res:
            return None
        return res.split(' ')

    def get_bss(self, bssid, ifname=None):
        if not ifname or ifname == self.ifname:
            res = self.request("BSS " + bssid)
        elif ifname == self.group_ifname:
            res = self.group_request("BSS " + bssid)
        else:
            return None

        if "FAIL" in res:
            return None
        lines = res.splitlines()
        vals = dict()
        for l in lines:
            [name, value] = l.split('=', 1)
            vals[name] = value
        if len(vals) == 0:
            return None
        return vals

    def get_pmksa(self, bssid):
        res = self.request("PMKSA")
        lines = res.splitlines()
        for l in lines:
            if bssid not in l:
                continue
            vals = dict()
            try:
                [index, aa, pmkid, expiration, opportunistic] = l.split(' ')
                cache_id = None
            except ValueError:
                [index, aa, pmkid, expiration, opportunistic, cache_id] = l.split(' ')
            vals['index'] = index
            vals['pmkid'] = pmkid
            vals['expiration'] = expiration
            vals['opportunistic'] = opportunistic
            if cache_id != None:
                vals['cache_id'] = cache_id
            return vals
        return None

    def get_pmk(self, network_id):
        bssid = self.get_status_field('bssid')
        res = self.request("PMKSA_GET %d" % network_id)
        for val in res.splitlines():
            if val.startswith(bssid):
                return val.split(' ')[2]
        return None

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
            if first:
                vals['addr'] = l
                first = False
            else:
                [name, value] = l.split('=', 1)
                vals[name] = value
        return vals

    def mgmt_rx(self, timeout=5):
        ev = self.wait_event(["MGMT-RX"], timeout=timeout)
        if ev is None:
            return None
        msg = {}
        items = ev.split(' ')
        field, val = items[1].split('=')
        if field != "freq":
            raise Exception("Unexpected MGMT-RX event format: " + ev)
        msg['freq'] = val

        field, val = items[2].split('=')
        if field != "datarate":
            raise Exception("Unexpected MGMT-RX event format: " + ev)
        msg['datarate'] = val

        field, val = items[3].split('=')
        if field != "ssi_signal":
            raise Exception("Unexpected MGMT-RX event format: " + ev)
        msg['ssi_signal'] = val

        frame = binascii.unhexlify(items[4])
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

    def wait_connected(self, timeout=10, error="Connection timed out"):
        ev = self.wait_event(["CTRL-EVENT-CONNECTED"], timeout=timeout)
        if ev is None:
            raise Exception(error)
        return ev

    def wait_disconnected(self, timeout=None, error="Disconnection timed out"):
        if timeout is None:
            timeout = 10 if self.hostname is None else 30
        ev = self.wait_event(["CTRL-EVENT-DISCONNECTED"], timeout=timeout)
        if ev is None:
            raise Exception(error)
        return ev

    def get_group_ifname(self):
        return self.group_ifname if self.group_ifname else self.ifname

    def get_config(self):
        res = self.request("DUMP")
        if res.startswith("FAIL"):
            raise Exception("DUMP failed")
        lines = res.splitlines()
        vals = dict()
        for l in lines:
            [name, value] = l.split('=', 1)
            vals[name] = value
        return vals

    def asp_provision(self, peer, adv_id, adv_mac, session_id, session_mac,
                      method="1000", info="", status=None, cpt=None, role=None):
        if status is None:
            cmd = "P2P_ASP_PROVISION"
            params = "info='%s' method=%s" % (info, method)
        else:
            cmd = "P2P_ASP_PROVISION_RESP"
            params = "status=%d" % status

        if role is not None:
            params += " role=" + role
        if cpt is not None:
            params += " cpt=" + cpt

        if "OK" not in self.global_request("%s %s adv_id=%s adv_mac=%s session=%d session_mac=%s %s" %
                                           (cmd, peer, adv_id, adv_mac, session_id, session_mac, params)):
            raise Exception("%s request failed" % cmd)

    def note(self, txt):
        self.request("NOTE " + txt)

    def save_config(self):
        if "OK" not in self.request("SAVE_CONFIG"):
            raise Exception("Failed to save configuration file")

    def wait_regdom(self, country_ie=False):
        for i in range(5):
            ev = self.wait_event(["CTRL-EVENT-REGDOM-CHANGE"], timeout=1)
            if ev is None:
                break
            if country_ie:
                if "init=COUNTRY_IE" in ev:
                    break
            else:
                break

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
                      tcp_addr=None, tcp_port=None, conn_status=False,
                      ssid_charset=None, nfc_uri=None, netrole=None,
                      csrattrs=None):
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
        if ssid_charset:
            cmd += " ssid_charset=%d" % ssid_charset
        if passphrase:
            cmd += " pass=" + binascii.hexlify(passphrase.encode()).decode()
        if tcp_addr:
            cmd += " tcp_addr=" + tcp_addr
        if tcp_port:
            cmd += " tcp_port=" + tcp_port
        if conn_status:
            cmd += " conn_status=1"
        if netrole:
            cmd += " netrole=" + netrole
        if csrattrs:
            cmd += " csrattrs=" + csrattrs
        res = self.request(cmd)
        if expect_fail:
            if "FAIL" not in res:
                raise Exception("DPP authentication started unexpectedly")
            return
        if "OK" not in res:
            raise Exception("Failed to initiate DPP Authentication")
        return int(peer)

    def dpp_pkex_init(self, identifier, code, role=None, key=None, curve=None,
                      extra=None, use_id=None, allow_fail=False, v2=False):
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
        if allow_fail:
            return id1
        if "FAIL" in res:
            raise Exception("Failed to set PKEX data (initiator)")
        return id1

    def dpp_pkex_resp(self, freq, identifier, code, key=None, curve=None,
                      listen_role=None, use_id=None):
        if use_id is None:
            id0 = self.dpp_bootstrap_gen(type="pkex", key=key, curve=curve)
        else:
            id0 = use_id
        cmd = "own=%d " % id0
        if identifier:
            cmd += "identifier=%s " % identifier
        cmd += "code=%s" % code
        res = self.request("DPP_PKEX_ADD " + cmd)
        if "FAIL" in res:
            raise Exception("Failed to set PKEX data (responder)")
        self.dpp_listen(freq, role=listen_role)
        return id0

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
