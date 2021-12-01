# FST configuration tests
# Copyright (c) 2015, Qualcomm Atheros, Inc.
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

import logging
logger = logging.getLogger()
import subprocess
import time
import os
import signal
import hostapd
import wpasupplicant
import utils

import fst_test_common

class FstLauncherConfig:
    """FstLauncherConfig class represents configuration to be used for
    FST config tests related hostapd/wpa_supplicant instances"""
    def __init__(self, iface, fst_group, fst_pri, fst_llt=None):
        self.iface = iface
        self.fst_group = fst_group
        self.fst_pri = fst_pri
        self.fst_llt = fst_llt # None llt means no llt parameter will be set

    def ifname(self):
        return self.iface

    def is_ap(self):
        """Returns True if the configuration is for AP, otherwise - False"""
        raise Exception("Virtual is_ap() called!")

    def to_file(self, pathname):
        """Creates configuration file to be used by FST config tests related
        hostapd/wpa_supplicant instances"""
        raise Exception("Virtual to_file() called!")

class FstLauncherConfigAP(FstLauncherConfig):
    """FstLauncherConfigAP class represents configuration to be used for
    FST config tests related hostapd instance"""
    def __init__(self, iface, ssid, mode, chan, fst_group, fst_pri,
                 fst_llt=None):
        self.ssid = ssid
        self.mode = mode
        self.chan = chan
        FstLauncherConfig.__init__(self, iface, fst_group, fst_pri, fst_llt)

    def is_ap(self):
        return True

    def get_channel(self):
        return self.chan

    def to_file(self, pathname):
        """Creates configuration file to be used by FST config tests related
        hostapd instance"""
        with open(pathname, "w") as f:
            f.write("country_code=US\n"
                    "interface=%s\n"
                    "ctrl_interface=/var/run/hostapd\n"
                    "ssid=%s\n"
                    "channel=%s\n"
                    "hw_mode=%s\n"
                    "ieee80211n=1\n" % (self.iface, self.ssid, self.chan,
                                        self.mode))
            if len(self.fst_group) != 0:
                f.write("fst_group_id=%s\n"
                        "fst_priority=%s\n" % (self.fst_group, self.fst_pri))
                if self.fst_llt is not None:
                    f.write("fst_llt=%s\n" % self.fst_llt)
        with open(pathname, "r") as f:
            logger.debug("wrote hostapd config file %s:\n%s" % (pathname,
                                                                f.read()))

class FstLauncherConfigSTA(FstLauncherConfig):
    """FstLauncherConfig class represents configuration to be used for
    FST config tests related wpa_supplicant instance"""
    def __init__(self, iface, fst_group, fst_pri, fst_llt=None):
        FstLauncherConfig.__init__(self, iface, fst_group, fst_pri, fst_llt)

    def is_ap(self):
        return False

    def to_file(self, pathname):
        """Creates configuration file to be used by FST config tests related
        wpa_supplicant instance"""
        with open(pathname, "w") as f:
            f.write("ctrl_interface=DIR=/var/run/wpa_supplicant\n"
                "p2p_no_group_iface=1\n")
            if len(self.fst_group) != 0:
                f.write("fst_group_id=%s\n"
                    "fst_priority=%s\n" % (self.fst_group, self.fst_pri))
                if self.fst_llt is not None:
                    f.write("fst_llt=%s\n" % self.fst_llt)
        with open(pathname, "r") as f:
            logger.debug("wrote wpa_supplicant config file %s:\n%s" % (pathname, f.read()))

class FstLauncher:
    """FstLauncher class is responsible for launching and cleaning up of FST
    config tests related hostapd/wpa_supplicant instances"""
    def __init__(self, logpath):
        self.logger = logging.getLogger()
        self.fst_logpath = logpath
        self.cfgs_to_run = []
        self.hapd_fst_global = '/var/run/hostapd-fst-global'
        self.wsup_fst_global = '/tmp/fststa'
        self.nof_aps = 0
        self.nof_stas = 0
        self.reg_ctrl = fst_test_common.HapdRegCtrl()
        self.test_is_supported()

    def __enter__(self):
        return self

    def __exit__(self, type, value, traceback):
        self.cleanup()

    @staticmethod
    def test_is_supported():
        h = hostapd.HostapdGlobal()
        resp = h.request("FST-MANAGER TEST_REQUEST IS_SUPPORTED")
        if not resp.startswith("OK"):
            raise utils.HwsimSkip("FST not supported")
        w = wpasupplicant.WpaSupplicant(global_iface='/tmp/wpas-wlan5')
        resp = w.global_request("FST-MANAGER TEST_REQUEST IS_SUPPORTED")
        if not resp.startswith("OK"):
            raise utils.HwsimSkip("FST not supported")

    def get_cfg_pathname(self, cfg):
        """Returns pathname of ifname based configuration file"""
        return self.fst_logpath +'/'+ cfg.ifname() + '.conf'

    def add_cfg(self, cfg):
        """Adds configuration to be used for launching hostapd/wpa_supplicant
        instances"""
        if cfg not in self.cfgs_to_run:
            self.cfgs_to_run.append(cfg)
        if cfg.is_ap() == True:
            self.nof_aps += 1
        else:
            self.nof_stas += 1

    def remove_cfg(self, cfg):
        """Removes configuration previously added with add_cfg"""
        if cfg in self.cfgs_to_run:
            self.cfgs_to_run.remove(cfg)
        if cfg.is_ap() == True:
            self.nof_aps -= 1
        else:
            self.nof_stas -= 1
        config_file = self.get_cfg_pathname(cfg)
        if os.path.exists(config_file):
            os.remove(config_file)

    def run_hostapd(self):
        """Lauches hostapd with interfaces configured according to
        FstLauncherConfigAP configurations added"""
        if self.nof_aps == 0:
            raise Exception("No FST APs to start")
        pidfile = self.fst_logpath + '/' + 'myhostapd.pid'
        mylogfile = self.fst_logpath + '/' + 'fst-hostapd'
        prg = os.path.join(self.fst_logpath,
                           'alt-hostapd/hostapd/hostapd')
        if not os.path.exists(prg):
            prg = '../../hostapd/hostapd'
        cmd = [prg, '-B', '-dddt',
               '-P', pidfile, '-f', mylogfile, '-g', self.hapd_fst_global]
        for i in range(0, len(self.cfgs_to_run)):
            cfg = self.cfgs_to_run[i]
            if cfg.is_ap() == True:
                cfgfile = self.get_cfg_pathname(cfg)
                cfg.to_file(cfgfile)
                cmd.append(cfgfile)
                self.reg_ctrl.add_ap(cfg.ifname(), cfg.get_channel())
        self.logger.debug("Starting fst hostapd: " + ' '.join(cmd))
        res = subprocess.call(cmd)
        self.logger.debug("fst hostapd start result: %d" % res)
        if res == 0:
            self.reg_ctrl.start()
        return res

    def run_wpa_supplicant(self):
        """Lauches wpa_supplicant with interfaces configured according to
        FstLauncherConfigSTA configurations added"""
        if self.nof_stas == 0:
            raise Exception("No FST STAs to start")
        pidfile = self.fst_logpath + '/' + 'mywpa_supplicant.pid'
        mylogfile = self.fst_logpath + '/' + 'fst-wpa_supplicant'
        prg = os.path.join(self.fst_logpath,
                           'alt-wpa_supplicant/wpa_supplicant/wpa_supplicant')
        if not os.path.exists(prg):
            prg = '../../wpa_supplicant/wpa_supplicant'
        cmd = [prg, '-B', '-dddt',
               '-P' + pidfile, '-f', mylogfile, '-g', self.wsup_fst_global]
        sta_no = 0
        for i in range(0, len(self.cfgs_to_run)):
            cfg = self.cfgs_to_run[i]
            if cfg.is_ap() == False:
                cfgfile = self.get_cfg_pathname(cfg)
                cfg.to_file(cfgfile)
                cmd.append('-c' + cfgfile)
                cmd.append('-i' + cfg.ifname())
                cmd.append('-Dnl80211')
                if sta_no != self.nof_stas -1:
                    cmd.append('-N')    # Next station configuration
                sta_no += 1
        self.logger.debug("Starting fst supplicant: " + ' '.join(cmd))
        res = subprocess.call(cmd)
        self.logger.debug("fst supplicant start result: %d" % res)
        return res

    def cleanup(self):
        """Terminates hostapd/wpa_supplicant processes previously launched with
        run_hostapd/run_wpa_supplicant"""
        pidfile = self.fst_logpath + '/' + 'myhostapd.pid'
        self.kill_pid(pidfile, self.nof_aps > 0)
        pidfile = self.fst_logpath + '/' + 'mywpa_supplicant.pid'
        self.kill_pid(pidfile, self.nof_stas > 0)
        self.reg_ctrl.stop()
        while len(self.cfgs_to_run) != 0:
            cfg = self.cfgs_to_run[0]
            self.remove_cfg(cfg)
        fst_test_common.fst_clear_regdom()

    def kill_pid(self, pidfile, try_again=False):
        """Kills process by PID file"""
        if not os.path.exists(pidfile):
            if not try_again:
                return
            # It might take some time for the process to write the PID file,
            # so wait a bit longer before giving up.
            self.logger.info("kill_pid: pidfile %s does not exist - try again after a second" % pidfile)
            time.sleep(1)
            if not os.path.exists(pidfile):
                self.logger.info("kill_pid: pidfile %s does not exist - could not kill the process" % pidfile)
                return
        pid = -1
        try:
            for i in range(3):
                pf = open(pidfile, 'r')
                pidtxt = pf.read().strip()
                self.logger.debug("kill_pid: %s: '%s'" % (pidfile, pidtxt))
                pf.close()
                try:
                    pid = int(pidtxt)
                    break
                except Exception as e:
                    self.logger.debug("kill_pid: No valid PID found: %s" % str(e))
                    time.sleep(1)
            self.logger.debug("kill_pid %s --> pid %d" % (pidfile, pid))
            os.kill(pid, signal.SIGTERM)
            for i in range(10):
                try:
                    # Poll the pid (Is the process still existing?)
                    os.kill(pid, 0)
                except OSError:
                    # No, already done
                    break
                # Wait and check again
                time.sleep(1)
        except Exception as e:
            self.logger.debug("Didn't stop the pid=%d. Was it stopped already? (%s)" % (pid, str(e)))


def parse_ies(iehex, el=-1):
    """Parses the information elements hex string 'iehex' in format
    "0a0b0c0d0e0f". If no 'el' defined just checks the IE string for integrity.
    If 'el' is defined returns the list of hex values of the specific IE (or
    empty list if the element is not in the string."""
    iel = [iehex[i:i + 2] for i in range(0, len(iehex), 2)]
    for i in range(0, len(iel)):
         iel[i] = int(iel[i], 16)
    # Validity check
    i = 0
    res = []
    while i < len(iel):
        logger.debug("IE found: %x" % iel[i])
        if el != -1 and el == iel[i]:
            res = iel[i + 2:i + 2 + iel[i + 1]]
        i += 2 + iel[i + 1]
    if i != len(iel):
        logger.error("Bad IE string: " + iehex)
        res = []
    return res

def scan_and_get_bss(dev, frq):
    """Issues a scan on given device on given frequency, returns the bss info
    dictionary ('ssid','ie','flags', etc.) or None. Note, the function
    implies there is only one AP on the given channel. If not a case,
    the function must be changed to call dev.get_bss() till the AP with the
    [b]ssid that we need is found"""
    dev.scan(freq=frq)
    return dev.get_bss('0')


# AP configuration tests

def run_test_ap_configuration(apdev, test_params,
                              fst_group=fst_test_common.fst_test_def_group,
                              fst_pri=fst_test_common.fst_test_def_prio_high,
                              fst_llt=fst_test_common.fst_test_def_llt):
    """Runs FST hostapd where the 1st AP configuration is fixed, the 2nd fst
    configuration is provided by the parameters. Returns the result of the run:
    0 - no errors discovered, an error otherwise. The function is used for
    simplek "bad configuration" tests."""
    logdir = test_params['logdir']
    with FstLauncher(logdir) as fst_launcher:
        ap1 = FstLauncherConfigAP(apdev[0]['ifname'], 'fst_goodconf', 'a',
                                  fst_test_common.fst_test_def_chan_a,
                                  fst_test_common.fst_test_def_group,
                                  fst_test_common.fst_test_def_prio_low,
                                  fst_test_common.fst_test_def_llt)
        ap2 = FstLauncherConfigAP(apdev[1]['ifname'], 'fst_badconf', 'b',
                                  fst_test_common.fst_test_def_chan_g, fst_group,
                                  fst_pri, fst_llt)
        fst_launcher.add_cfg(ap1)
        fst_launcher.add_cfg(ap2)
        res = fst_launcher.run_hostapd()
        return res

def run_test_sta_configuration(test_params,
                               fst_group=fst_test_common.fst_test_def_group,
                               fst_pri=fst_test_common.fst_test_def_prio_high,
                               fst_llt=fst_test_common.fst_test_def_llt):
    """Runs FST wpa_supplicant where the 1st STA configuration is fixed, the
    2nd fst configuration is provided by the parameters. Returns the result of
    the run: 0 - no errors discovered, an error otherwise. The function is used
    for simple "bad configuration" tests."""
    logdir = test_params['logdir']
    with FstLauncher(logdir) as fst_launcher:
        sta1 = FstLauncherConfigSTA('wlan5',
                                    fst_test_common.fst_test_def_group,
                                    fst_test_common.fst_test_def_prio_low,
                                    fst_test_common.fst_test_def_llt)
        sta2 = FstLauncherConfigSTA('wlan6', fst_group, fst_pri, fst_llt)
        fst_launcher.add_cfg(sta1)
        fst_launcher.add_cfg(sta2)
        res = fst_launcher.run_wpa_supplicant()
        return res

def test_fst_ap_config_llt_neg(dev, apdev, test_params):
    """FST AP configuration negative LLT"""
    res = run_test_ap_configuration(apdev, test_params, fst_llt='-1')
    if res == 0:
        raise Exception("hostapd started with a negative llt")

def test_fst_ap_config_llt_zero(dev, apdev, test_params):
    """FST AP configuration zero LLT"""
    res = run_test_ap_configuration(apdev, test_params, fst_llt='0')
    if res == 0:
        raise Exception("hostapd started with a zero llt")

def test_fst_ap_config_llt_too_big(dev, apdev, test_params):
    """FST AP configuration LLT is too big"""
    res = run_test_ap_configuration(apdev, test_params,
                                    fst_llt='4294967296') #0x100000000
    if res == 0:
        raise Exception("hostapd started with llt that is too big")

def test_fst_ap_config_llt_nan(dev, apdev, test_params):
    """FST AP configuration LLT is not a number"""
    res = run_test_ap_configuration(apdev, test_params, fst_llt='nan')
    if res == 0:
        raise Exception("hostapd started with llt not a number")

def test_fst_ap_config_pri_neg(dev, apdev, test_params):
    """FST AP configuration Priority negative"""
    res = run_test_ap_configuration(apdev, test_params, fst_pri='-1')
    if res == 0:
        raise Exception("hostapd started with a negative fst priority")

def test_fst_ap_config_pri_zero(dev, apdev, test_params):
    """FST AP configuration Priority zero"""
    res = run_test_ap_configuration(apdev, test_params, fst_pri='0')
    if res == 0:
        raise Exception("hostapd started with a zero fst priority")

def test_fst_ap_config_pri_large(dev, apdev, test_params):
    """FST AP configuration Priority too large"""
    res = run_test_ap_configuration(apdev, test_params, fst_pri='256')
    if res == 0:
        raise Exception("hostapd started with too large fst priority")

def test_fst_ap_config_pri_nan(dev, apdev, test_params):
    """FST AP configuration Priority not a number"""
    res = run_test_ap_configuration(apdev, test_params, fst_pri='nan')
    if res == 0:
        raise Exception("hostapd started with fst priority not a number")

def test_fst_ap_config_group_len(dev, apdev, test_params):
    """FST AP configuration Group max length"""
    res = run_test_ap_configuration(apdev, test_params,
                                    fst_group='fstg5678abcd34567')
    if res == 0:
        raise Exception("hostapd started with fst_group length too big")

def test_fst_ap_config_good(dev, apdev, test_params):
    """FST AP configuration good parameters"""
    res = run_test_ap_configuration(apdev, test_params)
    if res != 0:
        raise Exception("hostapd didn't start with valid config parameters")

def test_fst_ap_config_default(dev, apdev, test_params):
    """FST AP configuration default parameters"""
    res = run_test_ap_configuration(apdev, test_params, fst_llt=None)
    if res != 0:
        raise Exception("hostapd didn't start with valid config parameters")


# STA configuration tests

def test_fst_sta_config_llt_neg(dev, apdev, test_params):
    """FST STA configuration negative LLT"""
    res = run_test_sta_configuration(test_params, fst_llt='-1')
    if res == 0:
        raise Exception("wpa_supplicant started with a negative llt")

def test_fst_sta_config_llt_zero(dev, apdev, test_params):
    """FST STA configuration zero LLT"""
    res = run_test_sta_configuration(test_params, fst_llt='0')
    if res == 0:
        raise Exception("wpa_supplicant started with a zero llt")

def test_fst_sta_config_llt_large(dev, apdev, test_params):
    """FST STA configuration LLT is too large"""
    res = run_test_sta_configuration(test_params,
                                     fst_llt='4294967296') #0x100000000
    if res == 0:
        raise Exception("wpa_supplicant started with llt that is too large")

def test_fst_sta_config_llt_nan(dev, apdev, test_params):
    """FST STA configuration LLT is not a number"""
    res = run_test_sta_configuration(test_params, fst_llt='nan')
    if res == 0:
        raise Exception("wpa_supplicant started with llt not a number")

def test_fst_sta_config_pri_neg(dev, apdev, test_params):
    """FST STA configuration Priority negative"""
    res = run_test_sta_configuration(test_params, fst_pri='-1')
    if res == 0:
        raise Exception("wpa_supplicant started with a negative fst priority")

def test_fst_sta_config_pri_zero(dev, apdev, test_params):
    """FST STA configuration Priority zero"""
    res = run_test_sta_configuration(test_params, fst_pri='0')
    if res == 0:
        raise Exception("wpa_supplicant started with a zero fst priority")

def test_fst_sta_config_pri_big(dev, apdev, test_params):
    """FST STA configuration Priority too large"""
    res = run_test_sta_configuration(test_params, fst_pri='256')
    if res == 0:
        raise Exception("wpa_supplicant started with too large fst priority")

def test_fst_sta_config_pri_nan(dev, apdev, test_params):
    """FST STA configuration Priority not a number"""
    res = run_test_sta_configuration(test_params, fst_pri='nan')
    if res == 0:
        raise Exception("wpa_supplicant started with fst priority not a number")

def test_fst_sta_config_group_len(dev, apdev, test_params):
    """FST STA configuration Group max length"""
    res = run_test_sta_configuration(test_params,
                                     fst_group='fstg5678abcd34567')
    if res == 0:
        raise Exception("wpa_supplicant started with fst_group length too big")

def test_fst_sta_config_good(dev, apdev, test_params):
    """FST STA configuration good parameters"""
    res = run_test_sta_configuration(test_params)
    if res != 0:
        raise Exception("wpa_supplicant didn't start with valid config parameters")

def test_fst_sta_config_default(dev, apdev, test_params):
    """FST STA configuration default parameters"""
    res = run_test_sta_configuration(test_params, fst_llt=None)
    if res != 0:
        raise Exception("wpa_supplicant didn't start with valid config parameters")

def test_fst_scan_mb(dev, apdev, test_params):
    """FST scan valid MB IE presence with normal start"""
    logdir = test_params['logdir']

    # Test valid MB IE in scan results
    with FstLauncher(logdir) as fst_launcher:
        ap1 = FstLauncherConfigAP(apdev[0]['ifname'], 'fst_11a', 'a',
                                  fst_test_common.fst_test_def_chan_a,
                                  fst_test_common.fst_test_def_group,
                                  fst_test_common.fst_test_def_prio_high)
        ap2 = FstLauncherConfigAP(apdev[1]['ifname'], 'fst_11g', 'b',
                                  fst_test_common.fst_test_def_chan_g,
                                  fst_test_common.fst_test_def_group,
                                  fst_test_common.fst_test_def_prio_low)
        fst_launcher.add_cfg(ap1)
        fst_launcher.add_cfg(ap2)
        res = fst_launcher.run_hostapd()
        if res != 0:
            raise Exception("hostapd didn't start properly")

        mbie1 = []
        flags1 = ''
        mbie2 = []
        flags2 = ''
        # Scan 1st AP
        vals1 = scan_and_get_bss(dev[0], fst_test_common.fst_test_def_freq_a)
        if vals1 != None:
            if 'ie' in vals1:
                mbie1 = parse_ies(vals1['ie'], 0x9e)
            if 'flags' in vals1:
                flags1 = vals1['flags']
        # Scan 2nd AP
        vals2 = scan_and_get_bss(dev[2], fst_test_common.fst_test_def_freq_g)
        if vals2 != None:
            if 'ie' in vals2:
                mbie2 = parse_ies(vals2['ie'], 0x9e)
            if 'flags' in vals2:
                flags2 = vals2['flags']

    if len(mbie1) == 0:
        raise Exception("No MB IE created by 1st AP")
    if len(mbie2) == 0:
        raise Exception("No MB IE created by 2nd AP")

def test_fst_scan_nomb(dev, apdev, test_params):
    """FST scan no MB IE presence with 1 AP start"""
    logdir = test_params['logdir']

    # Test valid MB IE in scan results
    with FstLauncher(logdir) as fst_launcher:
        ap1 = FstLauncherConfigAP(apdev[0]['ifname'], 'fst_11a', 'a',
                                  fst_test_common.fst_test_def_chan_a,
                                  fst_test_common.fst_test_def_group,
                                  fst_test_common.fst_test_def_prio_high)
        fst_launcher.add_cfg(ap1)
        res = fst_launcher.run_hostapd()
        if res != 0:
            raise Exception("Hostapd didn't start properly")

        time.sleep(2)
        mbie1 = []
        flags1 = ''
        vals1 = scan_and_get_bss(dev[0], fst_test_common.fst_test_def_freq_a)
        if vals1 != None:
            if 'ie' in vals1:
                mbie1 = parse_ies(vals1['ie'], 0x9e)
            if 'flags' in vals1:
                flags1 = vals1['flags']

    if len(mbie1) != 0:
        raise Exception("MB IE exists with 1 AP")
