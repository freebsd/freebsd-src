# Python class for controlling wlantest
# Copyright (c) 2013-2019, Jouni Malinen <j@w1.fi>
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

import re
import os
import posixpath
import time
import subprocess
import logging
import wpaspy

logger = logging.getLogger()

class Wlantest:
    remote_host = None
    setup_params = None
    exe_thread = None
    exe_res = []
    monitor_mod = None
    setup_done = False

    @classmethod
    def stop_remote_wlantest(cls):
        if cls.exe_thread is None:
            # Local flow - no need for remote operations
            return

        cls.remote_host.execute(["killall", "-9", "wlantest"])
        cls.remote_host.thread_wait(cls.exe_thread, 5)
        cls.exe_thread = None
        cls.exe_res = []

    @classmethod
    def reset_remote_wlantest(cls):
        cls.stop_remote_wlantest()
        cls.remote_host = None
        cls.setup_params = None
        cls.exe_thread = None
        cls.exe_res = []
        cls.monitor_mod = None
        cls.setup_done = False

    @classmethod
    def start_remote_wlantest(cls):
        if cls.remote_host is None:
            # Local flow - no need for remote operations
            return
        if cls.exe_thread is not None:
            raise Exception("Cannot start wlantest twice")

        log_dir = cls.setup_params['log_dir']
        ifaces = re.split('; | |, ', cls.remote_host.ifname)
        ifname = ifaces[0]
        exe = cls.setup_params["wlantest"]
        tc_name = cls.setup_params["tc_name"]
        base_log_name = tc_name + "_wlantest_" + \
                        cls.remote_host.name + "_" + ifname
        log_file = posixpath.join(log_dir, base_log_name + ".log")
        pcap_file = posixpath.join(log_dir, base_log_name + ".pcapng")
        cmd = "{} -i {} -n {} -c -dtN -L {}".format(exe, ifname,
                                                    pcap_file, log_file)
        cls.remote_host.add_log(log_file)
        cls.remote_host.add_log(pcap_file)
        cls.exe_thread = cls.remote_host.thread_run(cmd.split(), cls.exe_res)
        # Give wlantest a chance to start working
        time.sleep(1)

    @classmethod
    def register_remote_wlantest(cls, host, setup_params, monitor_mod):
        if cls.remote_host is not None:
            raise Exception("Cannot register remote wlantest twice")
        cls.remote_host = host
        cls.setup_params = setup_params
        cls.monitor_mod = monitor_mod
        status, buf = host.execute(["which", setup_params['wlantest']])
        if status != 0:
            raise Exception(host.name + " - wlantest: " + buf)
        status, buf = host.execute(["which", setup_params['wlantest_cli']])
        if status != 0:
            raise Exception(host.name + " - wlantest_cli: " + buf)

    @classmethod
    def chan_from_wpa(cls, wpa, is_p2p=False):
        if cls.monitor_mod is None:
            return
        m = cls.monitor_mod
        return m.setup(cls.remote_host, [m.get_monitor_params(wpa, is_p2p)])

    @classmethod
    def setup(cls, wpa, is_p2p=False):
        if wpa:
            cls.chan_from_wpa(wpa, is_p2p)
        cls.start_remote_wlantest()
        cls.setup_done = True

    def __init__(self):
        if not self.setup_done:
            raise Exception("Cannot create Wlantest instance before setup()")
        if os.path.isfile('../../wlantest/wlantest_cli'):
            self.wlantest_cli = '../../wlantest/wlantest_cli'
        else:
            self.wlantest_cli = 'wlantest_cli'

    def cli_cmd(self, params):
        if self.remote_host is not None:
            exe = self.setup_params["wlantest_cli"]
            ret = self.remote_host.execute([exe] + params)
            if ret[0] != 0:
                raise Exception("wlantest_cli failed")
            return ret[1]
        else:
            return subprocess.check_output([self.wlantest_cli] + params).decode()

    def flush(self):
        res = self.cli_cmd(["flush"])
        if "FAIL" in res:
            raise Exception("wlantest_cli flush failed")

    def relog(self):
        res = self.cli_cmd(["relog"])
        if "FAIL" in res:
            raise Exception("wlantest_cli relog failed")

    def add_passphrase(self, passphrase):
        res = self.cli_cmd(["add_passphrase", passphrase])
        if "FAIL" in res:
            raise Exception("wlantest_cli add_passphrase failed")

    def add_wepkey(self, key):
        res = self.cli_cmd(["add_wepkey", key])
        if "FAIL" in res:
            raise Exception("wlantest_cli add_key failed")

    def info_bss(self, field, bssid):
        res = self.cli_cmd(["info_bss", field, bssid])
        if "FAIL" in res:
            raise Exception("Could not get BSS info from wlantest for " + bssid)
        return res

    def get_bss_counter(self, field, bssid):
        try:
            res = self.cli_cmd(["get_bss_counter", field, bssid])
        except Exception as e:
            return 0
        if "FAIL" in res:
            return 0
        return int(res)

    def clear_bss_counters(self, bssid):
        self.cli_cmd(["clear_bss_counters", bssid])

    def info_sta(self, field, bssid, addr):
        res = self.cli_cmd(["info_sta", field, bssid, addr])
        if "FAIL" in res:
            raise Exception("Could not get STA info from wlantest for " + addr)
        return res

    def get_sta_counter(self, field, bssid, addr):
        res = self.cli_cmd(["get_sta_counter", field, bssid, addr])
        if "FAIL" in res:
            raise Exception("wlantest_cli command failed")
        return int(res)

    def clear_sta_counters(self, bssid, addr):
        res = self.cli_cmd(["clear_sta_counters", bssid, addr])
        if "FAIL" in res:
            raise Exception("wlantest_cli command failed")

    def tdls_clear(self, bssid, addr1, addr2):
        self.cli_cmd(["clear_tdls_counters", bssid, addr1, addr2])

    def get_tdls_counter(self, field, bssid, addr1, addr2):
        res = self.cli_cmd(["get_tdls_counter", field, bssid, addr1, addr2])
        if "FAIL" in res:
            raise Exception("wlantest_cli command failed")
        return int(res)

    def require_ap_pmf_mandatory(self, bssid):
        res = self.info_bss("rsn_capab", bssid)
        if "MFPR" not in res:
            raise Exception("AP did not require PMF")
        if "MFPC" not in res:
            raise Exception("AP did not enable PMF")
        res = self.info_bss("key_mgmt", bssid)
        if "PSK-SHA256" not in res:
            raise Exception("AP did not enable SHA256-based AKM for PMF")

    def require_ap_pmf_optional(self, bssid):
        res = self.info_bss("rsn_capab", bssid)
        if "MFPR" in res:
            raise Exception("AP required PMF")
        if "MFPC" not in res:
            raise Exception("AP did not enable PMF")

    def require_ap_no_pmf(self, bssid):
        res = self.info_bss("rsn_capab", bssid)
        if "MFPR" in res:
            raise Exception("AP required PMF")
        if "MFPC" in res:
            raise Exception("AP enabled PMF")

    def require_sta_pmf_mandatory(self, bssid, addr):
        res = self.info_sta("rsn_capab", bssid, addr)
        if "MFPR" not in res:
            raise Exception("STA did not require PMF")
        if "MFPC" not in res:
            raise Exception("STA did not enable PMF")

    def require_sta_pmf(self, bssid, addr):
        res = self.info_sta("rsn_capab", bssid, addr)
        if "MFPC" not in res:
            raise Exception("STA did not enable PMF")

    def require_sta_no_pmf(self, bssid, addr):
        res = self.info_sta("rsn_capab", bssid, addr)
        if "MFPC" in res:
            raise Exception("STA enabled PMF")

    def require_sta_key_mgmt(self, bssid, addr, key_mgmt):
        res = self.info_sta("key_mgmt", bssid, addr)
        if key_mgmt not in res:
            raise Exception("Unexpected STA key_mgmt")

    def get_tx_tid(self, bssid, addr, tid):
        res = self.cli_cmd(["get_tx_tid", bssid, addr, str(tid)])
        if "FAIL" in res:
            raise Exception("wlantest_cli command failed")
        return int(res)

    def get_rx_tid(self, bssid, addr, tid):
        res = self.cli_cmd(["get_rx_tid", bssid, addr, str(tid)])
        if "FAIL" in res:
            raise Exception("wlantest_cli command failed")
        return int(res)

    def get_tid_counters(self, bssid, addr):
        tx = {}
        rx = {}
        for tid in range(0, 17):
            tx[tid] = self.get_tx_tid(bssid, addr, tid)
            rx[tid] = self.get_rx_tid(bssid, addr, tid)
        return [tx, rx]

class WlantestCapture:
    def __init__(self, ifname, output, netns=None):
        self.cmd = None
        self.ifname = ifname
        if os.path.isfile('../../wlantest/wlantest'):
            bin = '../../wlantest/wlantest'
        else:
            bin = 'wlantest'
        logger.debug("wlantest[%s] starting" % ifname)
        args = [bin, '-e', '-i', ifname, '-w', output]
        if netns:
            args = ['ip', 'netns', 'exec', netns] + args
        self.cmd = subprocess.Popen(args,
                                    stdout=subprocess.PIPE,
                                    stderr=subprocess.PIPE)

    def __del__(self):
        if self.cmd:
            self.close()

    def close(self):
        logger.debug("wlantest[%s] stopping" % self.ifname)
        self.cmd.terminate()
        res = self.cmd.communicate()
        if len(res[0]) > 0:
            logger.debug("wlantest[%s] stdout: %s" % (self.ifname,
                                                      res[0].decode().strip()))
        if len(res[1]) > 0:
            logger.debug("wlantest[%s] stderr: %s" % (self.ifname,
                                                      res[1].decode().strip()))
        self.cmd = None
