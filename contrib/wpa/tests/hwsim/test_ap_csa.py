# AP CSA tests
# Copyright (c) 2013, Luciano Coelho <luciano.coelho@intel.com>
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

from remotehost import remote_compatible
import time
import logging
logger = logging.getLogger()

import hwsim_utils
import hostapd
from utils import *

def connect(dev, apdev, scan_freq="2412", **kwargs):
    params = {"ssid": "ap-csa",
              "channel": "1"}
    params.update(kwargs)
    ap = hostapd.add_ap(apdev[0], params)
    dev.connect("ap-csa", key_mgmt="NONE", scan_freq=scan_freq)
    return ap

def switch_channel(ap, count, freq):
    ap.request("CHAN_SWITCH " + str(count) + " " + str(freq))

    ev = ap.wait_event(["CTRL-EVENT-STARTED-CHANNEL-SWITCH"], timeout=10)
    if ev is None:
        raise Exception("Channel switch start event not seen")
    if "freq=" + str(freq) not in ev:
        raise Exception("Unexpected channel in CS started event")

    ev = ap.wait_event(["CTRL-EVENT-CHANNEL-SWITCH"], timeout=10)
    if ev is None:
        raise Exception("Channel switch completed event not seen")
    if "freq=" + str(freq) not in ev:
        raise Exception("Unexpected channel in CS completed event")

    ev = ap.wait_event(["AP-CSA-FINISHED"], timeout=10)
    if ev is None:
        raise Exception("CSA finished event timed out")
    if "freq=" + str(freq) not in ev:
        raise Exception("Unexpected channel in CSA finished event")

def wait_channel_switch(dev, freq):
    ev = dev.wait_event(["CTRL-EVENT-STARTED-CHANNEL-SWITCH"], timeout=5)
    if ev is None:
        raise Exception("Channel switch start not reported")
    if "freq=%d" % freq not in ev:
        raise Exception("Unexpected frequency in channel switch started: " + ev)

    ev = dev.wait_event(["CTRL-EVENT-CHANNEL-SWITCH"], timeout=5)
    if ev is None:
        raise Exception("Channel switch not reported")
    if "freq=%d" % freq not in ev:
        raise Exception("Unexpected frequency: " + ev)

@remote_compatible
def test_ap_csa_1_switch(dev, apdev):
    """AP Channel Switch, one switch"""
    csa_supported(dev[0])
    freq = int(dev[0].get_driver_status_field("freq"))
    if freq != 0:
        raise Exception("Unexpected driver freq=%d in beginning" % freq)
    ap = connect(dev[0], apdev)
    freq = int(dev[0].get_driver_status_field("freq"))
    if freq != 2412:
        raise Exception("Unexpected driver freq=%d after association" % freq)

    hwsim_utils.test_connectivity(dev[0], ap)
    switch_channel(ap, 10, 2462)
    wait_channel_switch(dev[0], 2462)
    hwsim_utils.test_connectivity(dev[0], ap)
    freq = int(dev[0].get_driver_status_field("freq"))
    if freq != 2462:
        raise Exception("Unexpected driver freq=%d after channel switch" % freq)
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()
    freq = int(dev[0].get_driver_status_field("freq"))
    if freq != 0:
        raise Exception("Unexpected driver freq=%d after disconnection" % freq)

@remote_compatible
def test_ap_csa_2_switches(dev, apdev):
    """AP Channel Switch, two switches"""
    csa_supported(dev[0])
    ap = connect(dev[0], apdev)

    hwsim_utils.test_connectivity(dev[0], ap)
    switch_channel(ap, 10, 2462)
    wait_channel_switch(dev[0], 2462)
    hwsim_utils.test_connectivity(dev[0], ap)
    switch_channel(ap, 10, 2412)
    wait_channel_switch(dev[0], 2412)
    hwsim_utils.test_connectivity(dev[0], ap)

@remote_compatible
def test_ap_csa_1_switch_count_0(dev, apdev):
    """AP Channel Switch, one switch with count 0"""
    csa_supported(dev[0])
    ap = connect(dev[0], apdev)

    hwsim_utils.test_connectivity(dev[0], ap)
    switch_channel(ap, 0, 2462)
    # this does not result in CSA currently, so do not bother checking
    # connectivity

@remote_compatible
def test_ap_csa_2_switches_count_0(dev, apdev):
    """AP Channel Switch, two switches with count 0"""
    csa_supported(dev[0])
    ap = connect(dev[0], apdev)

    hwsim_utils.test_connectivity(dev[0], ap)
    switch_channel(ap, 0, 2462)
    # this does not result in CSA currently, so do not bother checking
    # connectivity
    switch_channel(ap, 0, 2412)
    # this does not result in CSA currently, so do not bother checking
    # connectivity

@remote_compatible
def test_ap_csa_1_switch_count_1(dev, apdev):
    """AP Channel Switch, one switch with count 1"""
    csa_supported(dev[0])
    ap = connect(dev[0], apdev)

    hwsim_utils.test_connectivity(dev[0], ap)
    switch_channel(ap, 1, 2462)
    # this does not result in CSA currently, so do not bother checking
    # connectivity

@remote_compatible
def test_ap_csa_2_switches_count_1(dev, apdev):
    """AP Channel Switch, two switches with count 1"""
    csa_supported(dev[0])
    ap = connect(dev[0], apdev)

    hwsim_utils.test_connectivity(dev[0], ap)
    switch_channel(ap, 1, 2462)
    # this does not result in CSA currently, so do not bother checking
    # connectivity
    switch_channel(ap, 1, 2412)
    # this does not result in CSA currently, so do not bother checking
    # connectivity

@remote_compatible
def test_ap_csa_1_switch_count_2(dev, apdev):
    """AP Channel Switch, one switch with count 2"""
    csa_supported(dev[0])
    ap = connect(dev[0], apdev)

    hwsim_utils.test_connectivity(dev[0], ap)
    switch_channel(ap, 2, 2462)
    wait_channel_switch(dev[0], 2462)
    hwsim_utils.test_connectivity(dev[0], ap)

@remote_compatible
def test_ap_csa_ecsa_only(dev, apdev):
    """AP Channel Switch, one switch with only ECSA IE"""
    csa_supported(dev[0])
    ap = connect(dev[0], apdev, ecsa_ie_only="1")

    hwsim_utils.test_connectivity(dev[0], ap)
    switch_channel(ap, 10, 2462)
    wait_channel_switch(dev[0], 2462)
    hwsim_utils.test_connectivity(dev[0], ap)

@remote_compatible
def test_ap_csa_invalid(dev, apdev):
    """AP Channel Switch - invalid channel"""
    csa_supported(dev[0])
    ap = connect(dev[0], apdev)

    vals = [2461, 4900, 4901, 5181, 5746, 5699, 5895, 5899]
    for val in vals:
        if "FAIL" not in ap.request("CHAN_SWITCH 1 %d" % val):
            raise Exception("Invalid channel accepted: %d" % val)

def test_ap_csa_disable(dev, apdev):
    """AP Channel Switch and DISABLE command before completion"""
    csa_supported(dev[0])
    ap = connect(dev[0], apdev, scan_freq="2412 2462")
    if "OK" not in ap.request("CHAN_SWITCH 10 2462"):
        raise Exception("CHAN_SWITCH failed")
    ap.disable()
    ap.enable()
    dev[0].wait_disconnected()
    dev[0].wait_connected()
