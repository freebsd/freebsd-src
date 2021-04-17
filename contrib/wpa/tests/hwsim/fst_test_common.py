# FST tests related definitions
# Copyright (c) 2015, Qualcomm Atheros, Inc.
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

import subprocess
import time
import logging

import hostapd

logger = logging.getLogger()

fst_test_def_group = 'fstg0'
fst_test_def_freq_g = '2412' # Channel 1
fst_test_def_freq_a = '5180' # Channel 36
fst_test_def_chan_g = '1'
fst_test_def_chan_a = '36'
fst_test_def_prio_low = '100'
fst_test_def_prio_high = '110'
fst_test_def_llt = '100'
fst_test_def_reg_domain = '00'

class HapdRegCtrl:
    def __init__(self):
        self.refcnt = 0
        self.ifname = None
        self.changed = False

    def __del__(self):
        if self.refcnt != 0 and self.changed == True:
            self.restore_reg_domain()

    def start(self):
        if self.ifname != None:
             hapd = hostapd.Hostapd(self.ifname)
             self.changed = self.wait_hapd_reg_change(hapd)

    def stop(self):
        if self.changed == True:
            self.restore_reg_domain()
            self.changed = False

    def add_ap(self, ifname, chan):
        if self.changed == False and self.channel_may_require_reg_change(chan):
             self.ifname = ifname

    @staticmethod
    def channel_may_require_reg_change(chan):
        if int(chan) > 14:
            return True
        return False

    @staticmethod
    def wait_hapd_reg_change(hapd):
        state = hapd.get_status_field("state")
        if state != "COUNTRY_UPDATE":
            state = hapd.get_status_field("state")
            if state != "ENABLED":
                raise Exception("Unexpected interface state - expected COUNTRY_UPDATE")
            else:
                logger.debug("fst hostapd: regulatory domain already set")
                return True

        logger.debug("fst hostapd: waiting for regulatory domain to be set...")

        ev = hapd.wait_event(["AP-ENABLED"], timeout=10)
        if not ev:
            raise Exception("AP setup timed out")

        logger.debug("fst hostapd: regulatory domain set")

        state = hapd.get_status_field("state")
        if state != "ENABLED":
            raise Exception("Unexpected interface state - expected ENABLED")

        logger.debug("fst hostapd: regulatory domain ready")
        return True

    @staticmethod
    def restore_reg_domain():
        logger.debug("fst hostapd: waiting for regulatory domain to be restored...")

        res = subprocess.call(['iw', 'reg', 'set', fst_test_def_reg_domain])
        if res != 0:
            raise Exception("Cannot restore regulatory domain")

        logger.debug("fst hostapd: regulatory domain ready")

def fst_clear_regdom():
    cmd = subprocess.Popen(["iw", "reg", "get"], stdout=subprocess.PIPE)
    res = cmd.stdout.read().decode()
    cmd.stdout.close()
    if "country 00:" not in res:
        subprocess.call(['iw', 'reg', 'set', '00'])
        time.sleep(0.1)
