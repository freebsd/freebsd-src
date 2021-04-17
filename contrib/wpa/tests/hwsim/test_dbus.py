# wpa_supplicant D-Bus interface tests
# Copyright (c) 2014-2015, Jouni Malinen <j@w1.fi>
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

import binascii
import logging
logger = logging.getLogger()
import subprocess
import time
import shutil
import struct
import sys

try:
    if sys.version_info[0] > 2:
        from gi.repository import GObject as gobject
    else:
        import gobject
    import dbus
    dbus_imported = True
except ImportError:
    dbus_imported = False

import hostapd
from wpasupplicant import WpaSupplicant
from utils import *
from p2p_utils import *
from test_ap_tdls import connect_2sta_open
from test_ap_eap import check_altsubject_match_support
from test_nfc_p2p import set_ip_addr_info
from test_wpas_mesh import check_mesh_support, add_open_mesh_network

WPAS_DBUS_SERVICE = "fi.w1.wpa_supplicant1"
WPAS_DBUS_PATH = "/fi/w1/wpa_supplicant1"
WPAS_DBUS_IFACE = "fi.w1.wpa_supplicant1.Interface"
WPAS_DBUS_IFACE_WPS = WPAS_DBUS_IFACE + ".WPS"
WPAS_DBUS_NETWORK = "fi.w1.wpa_supplicant1.Network"
WPAS_DBUS_BSS = "fi.w1.wpa_supplicant1.BSS"
WPAS_DBUS_IFACE_P2PDEVICE = WPAS_DBUS_IFACE + ".P2PDevice"
WPAS_DBUS_P2P_PEER = "fi.w1.wpa_supplicant1.Peer"
WPAS_DBUS_GROUP = "fi.w1.wpa_supplicant1.Group"
WPAS_DBUS_PERSISTENT_GROUP = "fi.w1.wpa_supplicant1.PersistentGroup"
WPAS_DBUS_IFACE_MESH = WPAS_DBUS_IFACE + ".Mesh"

def prepare_dbus(dev):
    if not dbus_imported:
        logger.info("No dbus module available")
        raise HwsimSkip("No dbus module available")
    try:
        from dbus.mainloop.glib import DBusGMainLoop
        dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
        bus = dbus.SystemBus()
        wpas_obj = bus.get_object(WPAS_DBUS_SERVICE, WPAS_DBUS_PATH)
        wpas = dbus.Interface(wpas_obj, WPAS_DBUS_SERVICE)
        path = wpas.GetInterface(dev.ifname)
        if_obj = bus.get_object(WPAS_DBUS_SERVICE, path)
        return (bus, wpas_obj, path, if_obj)
    except Exception as e:
        raise HwsimSkip("Could not connect to D-Bus: %s" % e)

class TestDbus(object):
    def __init__(self, bus):
        self.loop = gobject.MainLoop()
        self.signals = []
        self.bus = bus

    def __exit__(self, type, value, traceback):
        for s in self.signals:
            s.remove()

    def add_signal(self, handler, interface, name, byte_arrays=False):
        s = self.bus.add_signal_receiver(handler, dbus_interface=interface,
                                         signal_name=name,
                                         byte_arrays=byte_arrays)
        self.signals.append(s)

    def timeout(self, *args):
        logger.debug("timeout")
        self.loop.quit()
        return False

class alloc_fail_dbus(object):
    def __init__(self, dev, count, funcs, operation="Operation",
                 expected="NoMemory"):
        self._dev = dev
        self._count = count
        self._funcs = funcs
        self._operation = operation
        self._expected = expected
    def __enter__(self):
        cmd = "TEST_ALLOC_FAIL %d:%s" % (self._count, self._funcs)
        if "OK" not in self._dev.request(cmd):
            raise HwsimSkip("TEST_ALLOC_FAIL not supported")
    def __exit__(self, type, value, traceback):
        if type is None:
            raise Exception("%s succeeded during out-of-memory" % self._operation)
        if type == dbus.exceptions.DBusException and self._expected in str(value):
            return True
        if self._dev.request("GET_ALLOC_FAIL") != "0:%s" % self._funcs:
            raise Exception("%s did not trigger allocation failure" % self._operation)
        return False

def start_ap(ap, ssid="test-wps",
             ap_uuid="27ea801a-9e5c-4e73-bd82-f89cbcd10d7e"):
    params = {"ssid": ssid, "eap_server": "1", "wps_state": "2",
              "wpa_passphrase": "12345678", "wpa": "2",
              "wpa_key_mgmt": "WPA-PSK", "rsn_pairwise": "CCMP",
              "ap_pin": "12345670", "uuid": ap_uuid}
    return hostapd.add_ap(ap, params)

def test_dbus_getall(dev, apdev):
    """D-Bus GetAll"""
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])

    props = wpas_obj.GetAll(WPAS_DBUS_SERVICE,
                            dbus_interface=dbus.PROPERTIES_IFACE)
    logger.debug("GetAll(fi.w1.wpa.supplicant1, /fi/w1/wpa_supplicant1) ==> " + str(props))

    props = if_obj.GetAll(WPAS_DBUS_IFACE,
                          dbus_interface=dbus.PROPERTIES_IFACE)
    logger.debug("GetAll(%s, %s): %s" % (WPAS_DBUS_IFACE, path, str(props)))

    props = if_obj.GetAll(WPAS_DBUS_IFACE_WPS,
                          dbus_interface=dbus.PROPERTIES_IFACE)
    logger.debug("GetAll(%s, %s): %s" % (WPAS_DBUS_IFACE_WPS, path, str(props)))

    res = if_obj.Get(WPAS_DBUS_IFACE, 'BSSs',
                     dbus_interface=dbus.PROPERTIES_IFACE)
    if len(res) != 0:
        raise Exception("Unexpected BSSs entry: " + str(res))

    res = if_obj.Get(WPAS_DBUS_IFACE, 'Networks',
                     dbus_interface=dbus.PROPERTIES_IFACE)
    if len(res) != 0:
        raise Exception("Unexpected Networks entry: " + str(res))

    hapd = hostapd.add_ap(apdev[0], {"ssid": "open"})
    bssid = apdev[0]['bssid']
    dev[0].scan_for_bss(bssid, freq=2412)
    id = dev[0].add_network()
    dev[0].set_network(id, "disabled", "0")
    dev[0].set_network_quoted(id, "ssid", "test")

    res = if_obj.Get(WPAS_DBUS_IFACE, 'BSSs',
                     dbus_interface=dbus.PROPERTIES_IFACE)
    if len(res) != 1:
        raise Exception("Missing BSSs entry: " + str(res))
    bss_obj = bus.get_object(WPAS_DBUS_SERVICE, res[0])
    props = bss_obj.GetAll(WPAS_DBUS_BSS, dbus_interface=dbus.PROPERTIES_IFACE)
    logger.debug("GetAll(%s, %s): %s" % (WPAS_DBUS_BSS, res[0], str(props)))
    bssid_str = ''
    for item in props['BSSID']:
        if len(bssid_str) > 0:
            bssid_str += ':'
        bssid_str += '%02x' % item
    if bssid_str != bssid:
        raise Exception("Unexpected BSSID in BSSs entry")

    res = if_obj.Get(WPAS_DBUS_IFACE, 'Networks',
                     dbus_interface=dbus.PROPERTIES_IFACE)
    if len(res) != 1:
        raise Exception("Missing Networks entry: " + str(res))
    net_obj = bus.get_object(WPAS_DBUS_SERVICE, res[0])
    props = net_obj.GetAll(WPAS_DBUS_NETWORK,
                           dbus_interface=dbus.PROPERTIES_IFACE)
    logger.debug("GetAll(%s, %s): %s" % (WPAS_DBUS_NETWORK, res[0], str(props)))
    ssid = props['Properties']['ssid']
    if ssid != '"test"':
        raise Exception("Unexpected SSID in network entry")

def test_dbus_getall_oom(dev, apdev):
    """D-Bus GetAll wpa_config_get_all() OOM"""
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])

    id = dev[0].add_network()
    dev[0].set_network(id, "disabled", "0")
    dev[0].set_network_quoted(id, "ssid", "test")

    res = if_obj.Get(WPAS_DBUS_IFACE, 'Networks',
                     dbus_interface=dbus.PROPERTIES_IFACE)
    if len(res) != 1:
        raise Exception("Missing Networks entry: " + str(res))
    net_obj = bus.get_object(WPAS_DBUS_SERVICE, res[0])
    for i in range(1, 50):
        with alloc_fail(dev[0], i, "wpa_config_get_all"):
            try:
                props = net_obj.GetAll(WPAS_DBUS_NETWORK,
                                       dbus_interface=dbus.PROPERTIES_IFACE)
            except dbus.exceptions.DBusException as e:
                pass

def dbus_get(dbus, wpas_obj, prop, expect=None, byte_arrays=False):
    val = wpas_obj.Get(WPAS_DBUS_SERVICE, prop,
                       dbus_interface=dbus.PROPERTIES_IFACE,
                       byte_arrays=byte_arrays)
    if expect is not None and val != expect:
        raise Exception("Unexpected %s: %s (expected: %s)" %
                        (prop, str(val), str(expect)))
    return val

def dbus_set(dbus, wpas_obj, prop, val):
    wpas_obj.Set(WPAS_DBUS_SERVICE, prop, val,
                 dbus_interface=dbus.PROPERTIES_IFACE)

def test_dbus_properties(dev, apdev):
    """D-Bus Get/Set fi.w1.wpa_supplicant1 properties"""
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])

    dbus_get(dbus, wpas_obj, "DebugLevel", expect="msgdump")
    dbus_set(dbus, wpas_obj, "DebugLevel", "debug")
    dbus_get(dbus, wpas_obj, "DebugLevel", expect="debug")
    for (val, err) in [(3, "Error.Failed: wrong property type"),
                       ("foo", "Error.Failed: wrong debug level value")]:
        try:
            dbus_set(dbus, wpas_obj, "DebugLevel", val)
            raise Exception("Invalid DebugLevel value accepted: " + str(val))
        except dbus.exceptions.DBusException as e:
            if err not in str(e):
                raise Exception("Unexpected error message: " + str(e))
    dbus_set(dbus, wpas_obj, "DebugLevel", "msgdump")
    dbus_get(dbus, wpas_obj, "DebugLevel", expect="msgdump")

    dbus_get(dbus, wpas_obj, "DebugTimestamp", expect=True)
    dbus_set(dbus, wpas_obj, "DebugTimestamp", False)
    dbus_get(dbus, wpas_obj, "DebugTimestamp", expect=False)
    try:
        dbus_set(dbus, wpas_obj, "DebugTimestamp", "foo")
        raise Exception("Invalid DebugTimestamp value accepted")
    except dbus.exceptions.DBusException as e:
        if "Error.Failed: wrong property type" not in str(e):
            raise Exception("Unexpected error message: " + str(e))
    dbus_set(dbus, wpas_obj, "DebugTimestamp", True)
    dbus_get(dbus, wpas_obj, "DebugTimestamp", expect=True)

    dbus_get(dbus, wpas_obj, "DebugShowKeys", expect=True)
    dbus_set(dbus, wpas_obj, "DebugShowKeys", False)
    dbus_get(dbus, wpas_obj, "DebugShowKeys", expect=False)
    try:
        dbus_set(dbus, wpas_obj, "DebugShowKeys", "foo")
        raise Exception("Invalid DebugShowKeys value accepted")
    except dbus.exceptions.DBusException as e:
        if "Error.Failed: wrong property type" not in str(e):
            raise Exception("Unexpected error message: " + str(e))
    dbus_set(dbus, wpas_obj, "DebugShowKeys", True)
    dbus_get(dbus, wpas_obj, "DebugShowKeys", expect=True)

    res = dbus_get(dbus, wpas_obj, "Interfaces")
    if len(res) != 1:
        raise Exception("Unexpected Interfaces value: " + str(res))

    res = dbus_get(dbus, wpas_obj, "EapMethods")
    if len(res) < 5 or "TTLS" not in res:
        raise Exception("Unexpected EapMethods value: " + str(res))

    res = dbus_get(dbus, wpas_obj, "Capabilities")
    if len(res) < 2 or "p2p" not in res:
        raise Exception("Unexpected Capabilities value: " + str(res))

    dbus_get(dbus, wpas_obj, "WFDIEs", byte_arrays=True)
    val = binascii.unhexlify("010006020304050608")
    dbus_set(dbus, wpas_obj, "WFDIEs", dbus.ByteArray(val))
    res = dbus_get(dbus, wpas_obj, "WFDIEs", byte_arrays=True)
    if val != res:
        raise Exception("WFDIEs value changed")
    try:
        dbus_set(dbus, wpas_obj, "WFDIEs", dbus.ByteArray(b'\x00'))
        raise Exception("Invalid WFDIEs value accepted")
    except dbus.exceptions.DBusException as e:
        if "InvalidArgs" not in str(e):
            raise Exception("Unexpected error message: " + str(e))
    dbus_set(dbus, wpas_obj, "WFDIEs", dbus.ByteArray(b''))
    dbus_set(dbus, wpas_obj, "WFDIEs", dbus.ByteArray(val))
    dbus_set(dbus, wpas_obj, "WFDIEs", dbus.ByteArray(b''))
    res = dbus_get(dbus, wpas_obj, "WFDIEs", byte_arrays=True)
    if len(res) != 0:
        raise Exception("WFDIEs not cleared properly")

    res = dbus_get(dbus, wpas_obj, "EapMethods")
    try:
        dbus_set(dbus, wpas_obj, "EapMethods", res)
        raise Exception("Invalid Set accepted")
    except dbus.exceptions.DBusException as e:
        if "InvalidArgs: Property is read-only" not in str(e):
            raise Exception("Unexpected error message: " + str(e))

    try:
        wpas_obj.SetFoo(WPAS_DBUS_SERVICE, "DebugShowKeys", True,
                        dbus_interface=dbus.PROPERTIES_IFACE)
        raise Exception("Unknown method accepted")
    except dbus.exceptions.DBusException as e:
        if "UnknownMethod" not in str(e):
            raise Exception("Unexpected error message: " + str(e))

    try:
        wpas_obj.Get("foo", "DebugShowKeys",
                     dbus_interface=dbus.PROPERTIES_IFACE)
        raise Exception("Invalid Get accepted")
    except dbus.exceptions.DBusException as e:
        if "InvalidArgs: No such property" not in str(e):
            raise Exception("Unexpected error message: " + str(e))

    test_obj = bus.get_object(WPAS_DBUS_SERVICE, WPAS_DBUS_PATH,
                              introspect=False)
    try:
        test_obj.Get(123, "DebugShowKeys",
                     dbus_interface=dbus.PROPERTIES_IFACE)
        raise Exception("Invalid Get accepted")
    except dbus.exceptions.DBusException as e:
        if "InvalidArgs: Invalid arguments" not in str(e):
            raise Exception("Unexpected error message: " + str(e))
    try:
        test_obj.Get(WPAS_DBUS_SERVICE, 123,
                     dbus_interface=dbus.PROPERTIES_IFACE)
        raise Exception("Invalid Get accepted")
    except dbus.exceptions.DBusException as e:
        if "InvalidArgs: Invalid arguments" not in str(e):
            raise Exception("Unexpected error message: " + str(e))

    try:
        wpas_obj.Set(WPAS_DBUS_SERVICE, "WFDIEs",
                     dbus.ByteArray(b'', variant_level=2),
                     dbus_interface=dbus.PROPERTIES_IFACE)
        raise Exception("Invalid Set accepted")
    except dbus.exceptions.DBusException as e:
        if "InvalidArgs: invalid message format" not in str(e):
            raise Exception("Unexpected error message: " + str(e))

def test_dbus_set_global_properties(dev, apdev):
    """D-Bus Get/Set fi.w1.wpa_supplicant1 interface global properties"""
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])

    dev[0].set("model_name", "")
    props = [('Okc', '0', '1'), ('ModelName', '', 'blahblahblah')]

    for p in props:
        res = if_obj.Get(WPAS_DBUS_IFACE, p[0],
                         dbus_interface=dbus.PROPERTIES_IFACE)
        if res != p[1]:
            raise Exception("Unexpected " + p[0] + " value: " + str(res))

        if_obj.Set(WPAS_DBUS_IFACE, p[0], p[2],
                   dbus_interface=dbus.PROPERTIES_IFACE)

        res = if_obj.Get(WPAS_DBUS_IFACE, p[0],
                         dbus_interface=dbus.PROPERTIES_IFACE)
        if res != p[2]:
            raise Exception("Unexpected " + p[0] + " value after set: " + str(res))
    dev[0].set("model_name", "")

def test_dbus_invalid_method(dev, apdev):
    """D-Bus invalid method"""
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    wps = dbus.Interface(if_obj, WPAS_DBUS_IFACE_WPS)

    try:
        wps.Foo()
        raise Exception("Unknown method accepted")
    except dbus.exceptions.DBusException as e:
        if "UnknownMethod" not in str(e):
            raise Exception("Unexpected error message: " + str(e))

    test_obj = bus.get_object(WPAS_DBUS_SERVICE, path, introspect=False)
    test_wps = dbus.Interface(test_obj, WPAS_DBUS_IFACE_WPS)
    try:
        test_wps.Start(123)
        raise Exception("WPS.Start with incorrect signature accepted")
    except dbus.exceptions.DBusException as e:
        if "InvalidArgs: Invalid arg" not in str(e):
            raise Exception("Unexpected error message: " + str(e))

def test_dbus_get_set_wps(dev, apdev):
    """D-Bus Get/Set for WPS properties"""
    try:
        _test_dbus_get_set_wps(dev, apdev)
    finally:
        dev[0].request("SET wps_cred_processing 0")
        dev[0].request("SET config_methods display keypad virtual_display nfc_interface p2ps")
        dev[0].set("device_name", "Device A")
        dev[0].set("manufacturer", "")
        dev[0].set("model_name", "")
        dev[0].set("model_number", "")
        dev[0].set("serial_number", "")
        dev[0].set("device_type", "0-00000000-0")

def _test_dbus_get_set_wps(dev, apdev):
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])

    if_obj.Get(WPAS_DBUS_IFACE_WPS, "ConfigMethods",
               dbus_interface=dbus.PROPERTIES_IFACE)

    val = "display keypad virtual_display nfc_interface"
    dev[0].request("SET config_methods " + val)

    config = if_obj.Get(WPAS_DBUS_IFACE_WPS, "ConfigMethods",
                        dbus_interface=dbus.PROPERTIES_IFACE)
    if config != val:
        raise Exception("Unexpected Get(ConfigMethods) result: " + config)

    val2 = "push_button display"
    if_obj.Set(WPAS_DBUS_IFACE_WPS, "ConfigMethods", val2,
               dbus_interface=dbus.PROPERTIES_IFACE)
    config = if_obj.Get(WPAS_DBUS_IFACE_WPS, "ConfigMethods",
                        dbus_interface=dbus.PROPERTIES_IFACE)
    if config != val2:
        raise Exception("Unexpected Get(ConfigMethods) result after Set: " + config)

    dev[0].request("SET config_methods " + val)

    for i in range(3):
        dev[0].request("SET wps_cred_processing " + str(i))
        val = if_obj.Get(WPAS_DBUS_IFACE_WPS, "ProcessCredentials",
                         dbus_interface=dbus.PROPERTIES_IFACE)
        expected_val = False if i == 1 else True
        if val != expected_val:
            raise Exception("Unexpected Get(ProcessCredentials) result({}): {}".format(i, val))

    tests = [("device_name", "DeviceName"),
             ("manufacturer", "Manufacturer"),
             ("model_name", "ModelName"),
             ("model_number", "ModelNumber"),
             ("serial_number", "SerialNumber")]

    for f1, f2 in tests:
        val2 = "test-value-test"
        dev[0].set(f1, val2)
        val = if_obj.Get(WPAS_DBUS_IFACE_WPS, f2,
                         dbus_interface=dbus.PROPERTIES_IFACE)
        if val != val2:
            raise Exception("Get(%s) returned unexpected value" % f2)
        val2 = "TEST-value"
        if_obj.Set(WPAS_DBUS_IFACE_WPS, f2, val2,
                   dbus_interface=dbus.PROPERTIES_IFACE)
        val = if_obj.Get(WPAS_DBUS_IFACE_WPS, f2,
                         dbus_interface=dbus.PROPERTIES_IFACE)
        if val != val2:
            raise Exception("Get(%s) returned unexpected value after Set" % f2)

    dev[0].set("device_type", "5-0050F204-1")
    val = if_obj.Get(WPAS_DBUS_IFACE_WPS, "DeviceType",
                     dbus_interface=dbus.PROPERTIES_IFACE)
    if val[0] != 0x00 or val[1] != 0x05 != val[2] != 0x00 or val[3] != 0x50 or val[4] != 0xf2 or val[5] != 0x04 or val[6] != 0x00 or val[7] != 0x01:
        raise Exception("DeviceType mismatch")
    if_obj.Set(WPAS_DBUS_IFACE_WPS, "DeviceType", val,
               dbus_interface=dbus.PROPERTIES_IFACE)
    val = if_obj.Get(WPAS_DBUS_IFACE_WPS, "DeviceType",
                     dbus_interface=dbus.PROPERTIES_IFACE)
    if val[0] != 0x00 or val[1] != 0x05 != val[2] != 0x00 or val[3] != 0x50 or val[4] != 0xf2 or val[5] != 0x04 or val[6] != 0x00 or val[7] != 0x01:
        raise Exception("DeviceType mismatch after Set")

    val2 = b'\x01\x02\x03\x04\x05\x06\x07\x08'
    if_obj.Set(WPAS_DBUS_IFACE_WPS, "DeviceType", dbus.ByteArray(val2),
               dbus_interface=dbus.PROPERTIES_IFACE)
    val = if_obj.Get(WPAS_DBUS_IFACE_WPS, "DeviceType",
                     dbus_interface=dbus.PROPERTIES_IFACE,
                     byte_arrays=True)
    if val != val2:
        raise Exception("DeviceType mismatch after Set (2)")

    class TestDbusGetSet(TestDbus):
        def __init__(self, bus):
            TestDbus.__init__(self, bus)
            self.signal_received = False
            self.signal_received_deprecated = False
            self.sets_done = False

        def __enter__(self):
            gobject.timeout_add(1, self.run_sets)
            gobject.timeout_add(1000, self.timeout)
            self.add_signal(self.propertiesChanged, WPAS_DBUS_IFACE_WPS,
                            "PropertiesChanged")
            self.add_signal(self.propertiesChanged2, dbus.PROPERTIES_IFACE,
                            "PropertiesChanged")
            self.loop.run()
            return self

        def propertiesChanged(self, properties):
            logger.debug("PropertiesChanged: " + str(properties))
            if "ProcessCredentials" in properties:
                self.signal_received_deprecated = True
                if self.sets_done and self.signal_received:
                    self.loop.quit()

        def propertiesChanged2(self, interface_name, changed_properties,
                               invalidated_properties):
            logger.debug("propertiesChanged2: interface_name=%s changed_properties=%s invalidated_properties=%s" % (interface_name, str(changed_properties), str(invalidated_properties)))
            if interface_name != WPAS_DBUS_IFACE_WPS:
                return
            if "ProcessCredentials" in changed_properties:
                self.signal_received = True
                if self.sets_done and self.signal_received_deprecated:
                    self.loop.quit()

        def run_sets(self, *args):
            logger.debug("run_sets")
            if_obj.Set(WPAS_DBUS_IFACE_WPS, "ProcessCredentials",
                       dbus.Boolean(1),
                       dbus_interface=dbus.PROPERTIES_IFACE)
            if if_obj.Get(WPAS_DBUS_IFACE_WPS, "ProcessCredentials",
                          dbus_interface=dbus.PROPERTIES_IFACE) != True:
                raise Exception("Unexpected Get(ProcessCredentials) result after Set")
            if_obj.Set(WPAS_DBUS_IFACE_WPS, "ProcessCredentials",
                       dbus.Boolean(0),
                       dbus_interface=dbus.PROPERTIES_IFACE)
            if if_obj.Get(WPAS_DBUS_IFACE_WPS, "ProcessCredentials",
                          dbus_interface=dbus.PROPERTIES_IFACE) != False:
                raise Exception("Unexpected Get(ProcessCredentials) result after Set")

            self.dbus_sets_done = True
            return False

        def success(self):
            return self.signal_received and self.signal_received_deprecated

    with TestDbusGetSet(bus) as t:
        if not t.success():
            raise Exception("No signal received for ProcessCredentials change")

def test_dbus_wps_invalid(dev, apdev):
    """D-Bus invaldi WPS operation"""
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    wps = dbus.Interface(if_obj, WPAS_DBUS_IFACE_WPS)

    failures = [{'Role': 'foo', 'Type': 'pbc'},
                {'Role': 123, 'Type': 'pbc'},
                {'Type': 'pbc'},
                {'Role': 'enrollee'},
                {'Role': 'registrar'},
                {'Role': 'enrollee', 'Type': 123},
                {'Role': 'enrollee', 'Type': 'foo'},
                {'Role': 'enrollee', 'Type': 'pbc',
                 'Bssid': '02:33:44:55:66:77'},
                {'Role': 'enrollee', 'Type': 'pin', 'Pin': 123},
                {'Role': 'enrollee', 'Type': 'pbc',
                 'Bssid': dbus.ByteArray(b'12345')},
                {'Role': 'enrollee', 'Type': 'pbc',
                 'P2PDeviceAddress': 12345},
                {'Role': 'enrollee', 'Type': 'pbc',
                 'P2PDeviceAddress': dbus.ByteArray(b'12345')},
                {'Role': 'enrollee', 'Type': 'pbc', 'Foo': 'bar'}]
    for args in failures:
        try:
            wps.Start(args)
            raise Exception("Invalid WPS.Start() arguments accepted: " + str(args))
        except dbus.exceptions.DBusException as e:
            if not str(e).startswith("fi.w1.wpa_supplicant1.InvalidArgs"):
                raise Exception("Unexpected error message: " + str(e))

def test_dbus_wps_oom(dev, apdev):
    """D-Bus WPS operation (OOM)"""
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    wps = dbus.Interface(if_obj, WPAS_DBUS_IFACE_WPS)

    with alloc_fail_dbus(dev[0], 1, "=wpas_dbus_getter_state", "Get"):
        if_obj.Get(WPAS_DBUS_IFACE, "State",
                   dbus_interface=dbus.PROPERTIES_IFACE)

    hapd = hostapd.add_ap(apdev[0], {"ssid": "open"})
    bssid = apdev[0]['bssid']
    dev[0].scan_for_bss(bssid, freq=2412)

    time.sleep(0.05)
    for i in range(1, 3):
        with alloc_fail_dbus(dev[0], i, "=wpas_dbus_getter_bsss", "Get"):
            if_obj.Get(WPAS_DBUS_IFACE, "BSSs",
                       dbus_interface=dbus.PROPERTIES_IFACE)

    res = if_obj.Get(WPAS_DBUS_IFACE, 'BSSs',
                     dbus_interface=dbus.PROPERTIES_IFACE)
    bss_obj = bus.get_object(WPAS_DBUS_SERVICE, res[0])
    with alloc_fail_dbus(dev[0], 1, "=wpas_dbus_getter_bss_rates", "Get"):
        bss_obj.Get(WPAS_DBUS_BSS, "Rates",
                    dbus_interface=dbus.PROPERTIES_IFACE)
    with alloc_fail(dev[0], 1,
                    "wpa_bss_get_bit_rates;wpas_dbus_getter_bss_rates"):
        try:
            bss_obj.Get(WPAS_DBUS_BSS, "Rates",
                        dbus_interface=dbus.PROPERTIES_IFACE)
        except dbus.exceptions.DBusException as e:
            pass

    id = dev[0].add_network()
    dev[0].set_network(id, "disabled", "0")
    dev[0].set_network_quoted(id, "ssid", "test")

    for i in range(1, 3):
        with alloc_fail_dbus(dev[0], i, "=wpas_dbus_getter_networks", "Get"):
            if_obj.Get(WPAS_DBUS_IFACE, "Networks",
                       dbus_interface=dbus.PROPERTIES_IFACE)

    with alloc_fail_dbus(dev[0], 1, "wpas_dbus_getter_interfaces", "Get"):
        dbus_get(dbus, wpas_obj, "Interfaces")

    for i in range(1, 6):
        with alloc_fail_dbus(dev[0], i, "=eap_get_names_as_string_array;wpas_dbus_getter_eap_methods", "Get"):
            dbus_get(dbus, wpas_obj, "EapMethods")

    with alloc_fail_dbus(dev[0], 1, "wpas_dbus_setter_config_methods", "Set",
                         expected="Error.Failed: Failed to set property"):
        val2 = "push_button display"
        if_obj.Set(WPAS_DBUS_IFACE_WPS, "ConfigMethods", val2,
                   dbus_interface=dbus.PROPERTIES_IFACE)

    with alloc_fail_dbus(dev[0], 1, "=wpa_config_add_network;wpas_dbus_handler_wps_start",
                         "WPS.Start",
                         expected="UnknownError: WPS start failed"):
        wps.Start({'Role': 'enrollee', 'Type': 'pin', 'Pin': '12345670'})

def test_dbus_wps_pbc(dev, apdev):
    """D-Bus WPS/PBC operation and signals"""
    try:
        _test_dbus_wps_pbc(dev, apdev)
    finally:
        dev[0].request("SET wps_cred_processing 0")

def _test_dbus_wps_pbc(dev, apdev):
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    wps = dbus.Interface(if_obj, WPAS_DBUS_IFACE_WPS)

    hapd = start_ap(apdev[0])
    hapd.request("WPS_PBC")
    bssid = apdev[0]['bssid']
    dev[0].scan_for_bss(bssid, freq="2412")
    dev[0].request("SET wps_cred_processing 2")

    res = if_obj.Get(WPAS_DBUS_IFACE, 'BSSs',
                     dbus_interface=dbus.PROPERTIES_IFACE)
    if len(res) != 1:
        raise Exception("Missing BSSs entry: " + str(res))
    bss_obj = bus.get_object(WPAS_DBUS_SERVICE, res[0])
    props = bss_obj.GetAll(WPAS_DBUS_BSS, dbus_interface=dbus.PROPERTIES_IFACE)
    logger.debug("GetAll(%s, %s): %s" % (WPAS_DBUS_BSS, res[0], str(props)))
    if 'WPS' not in props:
        raise Exception("No WPS information in the BSS entry")
    if 'Type' not in props['WPS']:
        raise Exception("No Type field in the WPS dictionary")
    if props['WPS']['Type'] != 'pbc':
        raise Exception("Unexpected WPS Type: " + props['WPS']['Type'])

    class TestDbusWps(TestDbus):
        def __init__(self, bus, wps):
            TestDbus.__init__(self, bus)
            self.success_seen = False
            self.credentials_received = False
            self.wps = wps

        def __enter__(self):
            gobject.timeout_add(1, self.start_pbc)
            gobject.timeout_add(15000, self.timeout)
            self.add_signal(self.wpsEvent, WPAS_DBUS_IFACE_WPS, "Event")
            self.add_signal(self.credentials, WPAS_DBUS_IFACE_WPS,
                            "Credentials")
            self.loop.run()
            return self

        def wpsEvent(self, name, args):
            logger.debug("wpsEvent: %s args='%s'" % (name, str(args)))
            if name == "success":
                self.success_seen = True
                if self.credentials_received:
                    self.loop.quit()

        def credentials(self, args):
            logger.debug("credentials: " + str(args))
            self.credentials_received = True
            if self.success_seen:
                self.loop.quit()

        def start_pbc(self, *args):
            logger.debug("start_pbc")
            self.wps.Start({'Role': 'enrollee', 'Type': 'pbc'})
            return False

        def success(self):
            return self.success_seen and self.credentials_received

    with TestDbusWps(bus, wps) as t:
        if not t.success():
            raise Exception("Failure in D-Bus operations")

    dev[0].wait_connected(timeout=10)
    dev[0].request("DISCONNECT")
    hapd.disable()
    dev[0].flush_scan_cache()

def test_dbus_wps_pbc_overlap(dev, apdev):
    """D-Bus WPS/PBC operation and signal for PBC overlap"""
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    wps = dbus.Interface(if_obj, WPAS_DBUS_IFACE_WPS)

    hapd = start_ap(apdev[0])
    hapd2 = start_ap(apdev[1], ssid="test-wps2",
                     ap_uuid="27ea801a-9e5c-4e73-bd82-f89cbcd10d7f")
    hapd.request("WPS_PBC")
    hapd2.request("WPS_PBC")
    bssid = apdev[0]['bssid']
    dev[0].scan_for_bss(bssid, freq="2412")
    bssid2 = apdev[1]['bssid']
    dev[0].scan_for_bss(bssid2, freq="2412")

    class TestDbusWps(TestDbus):
        def __init__(self, bus, wps):
            TestDbus.__init__(self, bus)
            self.overlap_seen = False
            self.wps = wps

        def __enter__(self):
            gobject.timeout_add(1, self.start_pbc)
            gobject.timeout_add(15000, self.timeout)
            self.add_signal(self.wpsEvent, WPAS_DBUS_IFACE_WPS, "Event")
            self.loop.run()
            return self

        def wpsEvent(self, name, args):
            logger.debug("wpsEvent: %s args='%s'" % (name, str(args)))
            if name == "pbc-overlap":
                self.overlap_seen = True
                self.loop.quit()

        def start_pbc(self, *args):
            logger.debug("start_pbc")
            self.wps.Start({'Role': 'enrollee', 'Type': 'pbc'})
            return False

        def success(self):
            return self.overlap_seen

    with TestDbusWps(bus, wps) as t:
        if not t.success():
            raise Exception("Failure in D-Bus operations")

    dev[0].request("WPS_CANCEL")
    dev[0].request("DISCONNECT")
    hapd.disable()
    dev[0].flush_scan_cache()

def test_dbus_wps_pin(dev, apdev):
    """D-Bus WPS/PIN operation and signals"""
    try:
        _test_dbus_wps_pin(dev, apdev)
    finally:
        dev[0].request("SET wps_cred_processing 0")

def _test_dbus_wps_pin(dev, apdev):
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    wps = dbus.Interface(if_obj, WPAS_DBUS_IFACE_WPS)

    hapd = start_ap(apdev[0])
    hapd.request("WPS_PIN any 12345670")
    bssid = apdev[0]['bssid']
    dev[0].scan_for_bss(bssid, freq="2412")
    dev[0].request("SET wps_cred_processing 2")

    class TestDbusWps(TestDbus):
        def __init__(self, bus):
            TestDbus.__init__(self, bus)
            self.success_seen = False
            self.credentials_received = False

        def __enter__(self):
            gobject.timeout_add(1, self.start_pin)
            gobject.timeout_add(15000, self.timeout)
            self.add_signal(self.wpsEvent, WPAS_DBUS_IFACE_WPS, "Event")
            self.add_signal(self.credentials, WPAS_DBUS_IFACE_WPS,
                            "Credentials")
            self.loop.run()
            return self

        def wpsEvent(self, name, args):
            logger.debug("wpsEvent: %s args='%s'" % (name, str(args)))
            if name == "success":
                self.success_seen = True
                if self.credentials_received:
                    self.loop.quit()

        def credentials(self, args):
            logger.debug("credentials: " + str(args))
            self.credentials_received = True
            if self.success_seen:
                self.loop.quit()

        def start_pin(self, *args):
            logger.debug("start_pin")
            bssid_ay = dbus.ByteArray(binascii.unhexlify(bssid.replace(':', '').encode()))
            wps.Start({'Role': 'enrollee', 'Type': 'pin', 'Pin': '12345670',
                       'Bssid': bssid_ay})
            return False

        def success(self):
            return self.success_seen and self.credentials_received

    with TestDbusWps(bus) as t:
        if not t.success():
            raise Exception("Failure in D-Bus operations")

    dev[0].wait_connected(timeout=10)

def test_dbus_wps_pin2(dev, apdev):
    """D-Bus WPS/PIN operation and signals (PIN from wpa_supplicant)"""
    try:
        _test_dbus_wps_pin2(dev, apdev)
    finally:
        dev[0].request("SET wps_cred_processing 0")

def _test_dbus_wps_pin2(dev, apdev):
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    wps = dbus.Interface(if_obj, WPAS_DBUS_IFACE_WPS)

    hapd = start_ap(apdev[0])
    bssid = apdev[0]['bssid']
    dev[0].scan_for_bss(bssid, freq="2412")
    dev[0].request("SET wps_cred_processing 2")

    class TestDbusWps(TestDbus):
        def __init__(self, bus):
            TestDbus.__init__(self, bus)
            self.success_seen = False
            self.failed = False

        def __enter__(self):
            gobject.timeout_add(1, self.start_pin)
            gobject.timeout_add(15000, self.timeout)
            self.add_signal(self.wpsEvent, WPAS_DBUS_IFACE_WPS, "Event")
            self.add_signal(self.credentials, WPAS_DBUS_IFACE_WPS,
                            "Credentials")
            self.loop.run()
            return self

        def wpsEvent(self, name, args):
            logger.debug("wpsEvent: %s args='%s'" % (name, str(args)))
            if name == "success":
                self.success_seen = True
                if self.credentials_received:
                    self.loop.quit()

        def credentials(self, args):
            logger.debug("credentials: " + str(args))
            self.credentials_received = True
            if self.success_seen:
                self.loop.quit()

        def start_pin(self, *args):
            logger.debug("start_pin")
            bssid_ay = dbus.ByteArray(binascii.unhexlify(bssid.replace(':', '').encode()))
            res = wps.Start({'Role': 'enrollee', 'Type': 'pin',
                             'Bssid': bssid_ay})
            pin = res['Pin']
            h = hostapd.Hostapd(apdev[0]['ifname'])
            h.request("WPS_PIN any " + pin)
            return False

        def success(self):
            return self.success_seen and self.credentials_received

    with TestDbusWps(bus) as t:
        if not t.success():
            raise Exception("Failure in D-Bus operations")

    dev[0].wait_connected(timeout=10)

def test_dbus_wps_pin_m2d(dev, apdev):
    """D-Bus WPS/PIN operation and signals with M2D"""
    try:
        _test_dbus_wps_pin_m2d(dev, apdev)
    finally:
        dev[0].request("SET wps_cred_processing 0")

def _test_dbus_wps_pin_m2d(dev, apdev):
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    wps = dbus.Interface(if_obj, WPAS_DBUS_IFACE_WPS)

    hapd = start_ap(apdev[0])
    bssid = apdev[0]['bssid']
    dev[0].scan_for_bss(bssid, freq="2412")
    dev[0].request("SET wps_cred_processing 2")

    class TestDbusWps(TestDbus):
        def __init__(self, bus):
            TestDbus.__init__(self, bus)
            self.success_seen = False
            self.credentials_received = False

        def __enter__(self):
            gobject.timeout_add(1, self.start_pin)
            gobject.timeout_add(15000, self.timeout)
            self.add_signal(self.wpsEvent, WPAS_DBUS_IFACE_WPS, "Event")
            self.add_signal(self.credentials, WPAS_DBUS_IFACE_WPS,
                            "Credentials")
            self.loop.run()
            return self

        def wpsEvent(self, name, args):
            logger.debug("wpsEvent: %s args='%s'" % (name, str(args)))
            if name == "success":
                self.success_seen = True
                if self.credentials_received:
                    self.loop.quit()
            elif name == "m2d":
                h = hostapd.Hostapd(apdev[0]['ifname'])
                h.request("WPS_PIN any 12345670")

        def credentials(self, args):
            logger.debug("credentials: " + str(args))
            self.credentials_received = True
            if self.success_seen:
                self.loop.quit()

        def start_pin(self, *args):
            logger.debug("start_pin")
            bssid_ay = dbus.ByteArray(binascii.unhexlify(bssid.replace(':', '').encode()))
            wps.Start({'Role': 'enrollee', 'Type': 'pin', 'Pin': '12345670',
                       'Bssid': bssid_ay})
            return False

        def success(self):
            return self.success_seen and self.credentials_received

    with TestDbusWps(bus) as t:
        if not t.success():
            raise Exception("Failure in D-Bus operations")

    dev[0].wait_connected(timeout=10)

def test_dbus_wps_reg(dev, apdev):
    """D-Bus WPS/Registrar operation and signals"""
    try:
        _test_dbus_wps_reg(dev, apdev)
    finally:
        dev[0].request("SET wps_cred_processing 0")

def _test_dbus_wps_reg(dev, apdev):
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    wps = dbus.Interface(if_obj, WPAS_DBUS_IFACE_WPS)

    hapd = start_ap(apdev[0])
    hapd.request("WPS_PIN any 12345670")
    bssid = apdev[0]['bssid']
    dev[0].scan_for_bss(bssid, freq="2412")
    dev[0].request("SET wps_cred_processing 2")

    class TestDbusWps(TestDbus):
        def __init__(self, bus):
            TestDbus.__init__(self, bus)
            self.credentials_received = False

        def __enter__(self):
            gobject.timeout_add(100, self.start_reg)
            gobject.timeout_add(15000, self.timeout)
            self.add_signal(self.wpsEvent, WPAS_DBUS_IFACE_WPS, "Event")
            self.add_signal(self.credentials, WPAS_DBUS_IFACE_WPS,
                            "Credentials")
            self.loop.run()
            return self

        def wpsEvent(self, name, args):
            logger.debug("wpsEvent: %s args='%s'" % (name, str(args)))

        def credentials(self, args):
            logger.debug("credentials: " + str(args))
            self.credentials_received = True
            self.loop.quit()

        def start_reg(self, *args):
            logger.debug("start_reg")
            bssid_ay = dbus.ByteArray(binascii.unhexlify(bssid.replace(':', '').encode()))
            wps.Start({'Role': 'registrar', 'Type': 'pin',
                       'Pin': '12345670', 'Bssid': bssid_ay})
            return False

        def success(self):
            return self.credentials_received

    with TestDbusWps(bus) as t:
        if not t.success():
            raise Exception("Failure in D-Bus operations")

    dev[0].wait_connected(timeout=10)

def test_dbus_wps_cancel(dev, apdev):
    """D-Bus WPS Cancel operation"""
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    wps = dbus.Interface(if_obj, WPAS_DBUS_IFACE_WPS)

    hapd = start_ap(apdev[0])
    bssid = apdev[0]['bssid']

    wps.Cancel()
    dev[0].scan_for_bss(bssid, freq="2412")
    bssid_ay = dbus.ByteArray(binascii.unhexlify(bssid.replace(':', '').encode()))
    wps.Start({'Role': 'enrollee', 'Type': 'pin', 'Pin': '12345670',
               'Bssid': bssid_ay})
    wps.Cancel()
    dev[0].wait_event(["CTRL-EVENT-SCAN-RESULTS"], 1)

def test_dbus_scan_invalid(dev, apdev):
    """D-Bus invalid scan method"""
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    iface = dbus.Interface(if_obj, WPAS_DBUS_IFACE)

    tests = [({}, "InvalidArgs"),
             ({'Type': 123}, "InvalidArgs"),
             ({'Type': 'foo'}, "InvalidArgs"),
             ({'Type': 'active', 'Foo': 'bar'}, "InvalidArgs"),
             ({'Type': 'active', 'SSIDs': 'foo'}, "InvalidArgs"),
             ({'Type': 'active', 'SSIDs': ['foo']}, "InvalidArgs"),
             ({'Type': 'active',
               'SSIDs': [dbus.ByteArray(b"1"), dbus.ByteArray(b"2"),
                         dbus.ByteArray(b"3"), dbus.ByteArray(b"4"),
                         dbus.ByteArray(b"5"), dbus.ByteArray(b"6"),
                         dbus.ByteArray(b"7"), dbus.ByteArray(b"8"),
                         dbus.ByteArray(b"9"), dbus.ByteArray(b"10"),
                         dbus.ByteArray(b"11"), dbus.ByteArray(b"12"),
                         dbus.ByteArray(b"13"), dbus.ByteArray(b"14"),
                         dbus.ByteArray(b"15"), dbus.ByteArray(b"16"),
                         dbus.ByteArray(b"17")]},
              "InvalidArgs"),
             ({'Type': 'active',
               'SSIDs': [dbus.ByteArray(b"1234567890abcdef1234567890abcdef1")]},
              "InvalidArgs"),
             ({'Type': 'active', 'IEs': 'foo'}, "InvalidArgs"),
             ({'Type': 'active', 'IEs': ['foo']}, "InvalidArgs"),
             ({'Type': 'active', 'Channels': 2412}, "InvalidArgs"),
             ({'Type': 'active', 'Channels': [2412]}, "InvalidArgs"),
             ({'Type': 'active',
               'Channels': [(dbus.Int32(2412), dbus.UInt32(20))]},
              "InvalidArgs"),
             ({'Type': 'active',
               'Channels': [(dbus.UInt32(2412), dbus.Int32(20))]},
              "InvalidArgs"),
             ({'Type': 'active', 'AllowRoam': "yes"}, "InvalidArgs"),
             ({'Type': 'passive', 'IEs': [dbus.ByteArray(b"\xdd\x00")]},
              "InvalidArgs"),
             ({'Type': 'passive', 'SSIDs': [dbus.ByteArray(b"foo")]},
              "InvalidArgs")]
    for (t, err) in tests:
        try:
            iface.Scan(t)
            raise Exception("Invalid Scan() arguments accepted: " + str(t))
        except dbus.exceptions.DBusException as e:
            if err not in str(e):
                raise Exception("Unexpected error message for invalid Scan(%s): %s" % (str(t), str(e)))

def test_dbus_scan_oom(dev, apdev):
    """D-Bus scan method and OOM"""
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    iface = dbus.Interface(if_obj, WPAS_DBUS_IFACE)

    with alloc_fail_dbus(dev[0], 1,
                         "wpa_scan_clone_params;wpas_dbus_handler_scan",
                         "Scan", expected="ScanError: Scan request rejected"):
        iface.Scan({'Type': 'passive',
                    'Channels': [(dbus.UInt32(2412), dbus.UInt32(20))]})

    with alloc_fail_dbus(dev[0], 1,
                         "=wpas_dbus_get_scan_channels;wpas_dbus_handler_scan",
                         "Scan"):
        iface.Scan({'Type': 'passive',
                    'Channels': [(dbus.UInt32(2412), dbus.UInt32(20))]})

    with alloc_fail_dbus(dev[0], 1,
                         "=wpas_dbus_get_scan_ies;wpas_dbus_handler_scan",
                         "Scan"):
        iface.Scan({'Type': 'active',
                    'IEs': [dbus.ByteArray(b"\xdd\x00")],
                    'Channels': [(dbus.UInt32(2412), dbus.UInt32(20))]})

    with alloc_fail_dbus(dev[0], 1,
                         "=wpas_dbus_get_scan_ssids;wpas_dbus_handler_scan",
                         "Scan"):
        iface.Scan({'Type': 'active',
                    'SSIDs': [dbus.ByteArray(b"open"),
                              dbus.ByteArray()],
                    'Channels': [(dbus.UInt32(2412), dbus.UInt32(20))]})

def test_dbus_scan(dev, apdev):
    """D-Bus scan and related signals"""
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    iface = dbus.Interface(if_obj, WPAS_DBUS_IFACE)

    hapd = hostapd.add_ap(apdev[0], {"ssid": "open"})

    class TestDbusScan(TestDbus):
        def __init__(self, bus):
            TestDbus.__init__(self, bus)
            self.scan_completed = 0
            self.bss_added = False
            self.fail_reason = None

        def __enter__(self):
            gobject.timeout_add(1, self.run_scan)
            gobject.timeout_add(15000, self.timeout)
            self.add_signal(self.scanDone, WPAS_DBUS_IFACE, "ScanDone")
            self.add_signal(self.bssAdded, WPAS_DBUS_IFACE, "BSSAdded")
            self.add_signal(self.bssRemoved, WPAS_DBUS_IFACE, "BSSRemoved")
            self.loop.run()
            return self

        def scanDone(self, success):
            logger.debug("scanDone: success=%s" % success)
            self.scan_completed += 1
            if self.scan_completed == 1:
                iface.Scan({'Type': 'passive',
                            'AllowRoam': True,
                            'Channels': [(dbus.UInt32(2412), dbus.UInt32(20))]})
            elif self.scan_completed == 2:
                iface.Scan({'Type': 'passive',
                            'AllowRoam': False})
            elif self.bss_added and self.scan_completed == 3:
                self.loop.quit()

        def bssAdded(self, bss, properties):
            logger.debug("bssAdded: %s" % bss)
            logger.debug(str(properties))
            if 'WPS' in properties:
                if 'Type' in properties['WPS']:
                    self.fail_reason = "Unexpected WPS dictionary entry in non-WPS BSS"
                    self.loop.quit()
            self.bss_added = True
            if self.scan_completed == 3:
                self.loop.quit()

        def bssRemoved(self, bss):
            logger.debug("bssRemoved: %s" % bss)

        def run_scan(self, *args):
            logger.debug("run_scan")
            iface.Scan({'Type': 'active',
                        'SSIDs': [dbus.ByteArray(b"open"),
                                  dbus.ByteArray()],
                        'IEs': [dbus.ByteArray(b"\xdd\x00"),
                                dbus.ByteArray()],
                        'AllowRoam': False,
                        'Channels': [(dbus.UInt32(2412), dbus.UInt32(20))]})
            return False

        def success(self):
            return self.scan_completed == 3 and self.bss_added

    with TestDbusScan(bus) as t:
        if t.fail_reason:
            raise Exception(t.fail_reason)
        if not t.success():
            raise Exception("Expected signals not seen")

    res = if_obj.Get(WPAS_DBUS_IFACE, "BSSs",
                     dbus_interface=dbus.PROPERTIES_IFACE)
    if len(res) < 1:
        raise Exception("Scan result not in BSSs property")
    iface.FlushBSS(0)
    res = if_obj.Get(WPAS_DBUS_IFACE, "BSSs",
                     dbus_interface=dbus.PROPERTIES_IFACE)
    if len(res) != 0:
        raise Exception("FlushBSS() did not remove scan results from BSSs property")
    iface.FlushBSS(1)

def test_dbus_scan_rand(dev, apdev):
    """D-Bus MACAddressRandomizationMask property Get/Set"""
    try:
        run_dbus_scan_rand(dev, apdev)
    finally:
        dev[0].request("MAC_RAND_SCAN all enable=0")

def run_dbus_scan_rand(dev, apdev):
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    iface = dbus.Interface(if_obj, WPAS_DBUS_IFACE)

    res = if_obj.Get(WPAS_DBUS_IFACE, "MACAddressRandomizationMask",
                     dbus_interface=dbus.PROPERTIES_IFACE)
    if len(res) != 0:
        logger.info(str(res))
        raise Exception("Unexpected initial MACAddressRandomizationMask value")

    try:
        if_obj.Set(WPAS_DBUS_IFACE, "MACAddressRandomizationMask", "foo",
                   dbus_interface=dbus.PROPERTIES_IFACE)
        raise Exception("Invalid Set accepted")
    except dbus.exceptions.DBusException as e:
        if "InvalidArgs: invalid message format" not in str(e):
            raise Exception("Unexpected error message: " + str(e))

    try:
        if_obj.Set(WPAS_DBUS_IFACE, "MACAddressRandomizationMask",
                   {"foo": "bar"},
                   dbus_interface=dbus.PROPERTIES_IFACE)
        raise Exception("Invalid Set accepted")
    except dbus.exceptions.DBusException as e:
        if "wpas_dbus_setter_mac_address_randomization_mask: mask was not a byte array" not in str(e):
            raise Exception("Unexpected error message: " + str(e))

    try:
        if_obj.Set(WPAS_DBUS_IFACE, "MACAddressRandomizationMask",
                   {"foo": dbus.ByteArray(b'123456')},
                   dbus_interface=dbus.PROPERTIES_IFACE)
        raise Exception("Invalid Set accepted")
    except dbus.exceptions.DBusException as e:
        if 'wpas_dbus_setter_mac_address_randomization_mask: bad scan type "foo"' not in str(e):
            raise Exception("Unexpected error message: " + str(e))

    try:
        if_obj.Set(WPAS_DBUS_IFACE, "MACAddressRandomizationMask",
                   {"scan": dbus.ByteArray(b'12345')},
                   dbus_interface=dbus.PROPERTIES_IFACE)
        raise Exception("Invalid Set accepted")
    except dbus.exceptions.DBusException as e:
        if 'wpas_dbus_setter_mac_address_randomization_mask: malformed MAC mask given' not in str(e):
            raise Exception("Unexpected error message: " + str(e))

    if_obj.Set(WPAS_DBUS_IFACE, "MACAddressRandomizationMask",
               {"scan": dbus.ByteArray(b'123456')},
               dbus_interface=dbus.PROPERTIES_IFACE)
    res = if_obj.Get(WPAS_DBUS_IFACE, "MACAddressRandomizationMask",
                     dbus_interface=dbus.PROPERTIES_IFACE)
    if len(res) != 1:
        logger.info(str(res))
        raise Exception("Unexpected MACAddressRandomizationMask value")

    try:
        if_obj.Set(WPAS_DBUS_IFACE, "MACAddressRandomizationMask",
                   {"scan": dbus.ByteArray(b'123456'),
                    "sched_scan": dbus.ByteArray(b'987654')},
                   dbus_interface=dbus.PROPERTIES_IFACE)
    except dbus.exceptions.DBusException as e:
        # sched_scan is unlikely to be supported
        pass

    if_obj.Set(WPAS_DBUS_IFACE, "MACAddressRandomizationMask",
               dbus.Dictionary({}, signature='sv'),
               dbus_interface=dbus.PROPERTIES_IFACE)
    res = if_obj.Get(WPAS_DBUS_IFACE, "MACAddressRandomizationMask",
                     dbus_interface=dbus.PROPERTIES_IFACE)
    if len(res) != 0:
        logger.info(str(res))
        raise Exception("Unexpected MACAddressRandomizationMask value")

def test_dbus_scan_busy(dev, apdev):
    """D-Bus scan trigger rejection when busy with previous scan"""
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    iface = dbus.Interface(if_obj, WPAS_DBUS_IFACE)

    if "OK" not in dev[0].request("SCAN freq=2412-2462"):
        raise Exception("Failed to start scan")
    ev = dev[0].wait_event(["CTRL-EVENT-SCAN-STARTED"], 15)
    if ev is None:
        raise Exception("Scan start timed out")

    try:
        iface.Scan({'Type': 'active', 'AllowRoam': False})
        raise Exception("Scan() accepted when busy")
    except dbus.exceptions.DBusException as e:
        if "ScanError: Scan request reject" not in str(e):
            raise Exception("Unexpected error message: " + str(e))

    ev = dev[0].wait_event(["CTRL-EVENT-SCAN-RESULTS"], 15)
    if ev is None:
        raise Exception("Scan timed out")

def test_dbus_scan_abort(dev, apdev):
    """D-Bus scan trigger and abort"""
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    iface = dbus.Interface(if_obj, WPAS_DBUS_IFACE)

    iface.Scan({'Type': 'active', 'AllowRoam': False})
    ev = dev[0].wait_event(["CTRL-EVENT-SCAN-STARTED"], 15)
    if ev is None:
        raise Exception("Scan start timed out")

    iface.AbortScan()
    ev = dev[0].wait_event(["CTRL-EVENT-SCAN-RESULTS"], 15)
    if ev is None:
        raise Exception("Scan abort result timed out")
    dev[0].dump_monitor()
    iface.Scan({'Type': 'active', 'AllowRoam': False})
    iface.AbortScan()

    ev = dev[0].wait_event(["CTRL-EVENT-SCAN-RESULTS"], 15)
    if ev is None:
        raise Exception("Scan timed out")

def test_dbus_connect(dev, apdev):
    """D-Bus AddNetwork and connect"""
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    iface = dbus.Interface(if_obj, WPAS_DBUS_IFACE)

    ssid = "test-wpa2-psk"
    passphrase = 'qwertyuiop'
    params = hostapd.wpa2_params(ssid=ssid, passphrase=passphrase)
    hapd = hostapd.add_ap(apdev[0], params)

    class TestDbusConnect(TestDbus):
        def __init__(self, bus):
            TestDbus.__init__(self, bus)
            self.network_added = False
            self.network_selected = False
            self.network_removed = False
            self.state = 0

        def __enter__(self):
            gobject.timeout_add(1, self.run_connect)
            gobject.timeout_add(15000, self.timeout)
            self.add_signal(self.networkAdded, WPAS_DBUS_IFACE, "NetworkAdded")
            self.add_signal(self.networkRemoved, WPAS_DBUS_IFACE,
                            "NetworkRemoved")
            self.add_signal(self.networkSelected, WPAS_DBUS_IFACE,
                            "NetworkSelected")
            self.add_signal(self.propertiesChanged, WPAS_DBUS_IFACE,
                            "PropertiesChanged")
            self.loop.run()
            return self

        def networkAdded(self, network, properties):
            logger.debug("networkAdded: %s" % str(network))
            logger.debug(str(properties))
            self.network_added = True

        def networkRemoved(self, network):
            logger.debug("networkRemoved: %s" % str(network))
            self.network_removed = True

        def networkSelected(self, network):
            logger.debug("networkSelected: %s" % str(network))
            self.network_selected = True

        def propertiesChanged(self, properties):
            logger.debug("propertiesChanged: %s" % str(properties))
            if 'State' in properties and properties['State'] == "completed":
                if self.state == 0:
                    self.state = 1
                    iface.Disconnect()
                elif self.state == 2:
                    self.state = 3
                    iface.Disconnect()
                elif self.state == 4:
                    self.state = 5
                    iface.Reattach()
                elif self.state == 5:
                    self.state = 6
                    iface.Disconnect()
                elif self.state == 7:
                    self.state = 8
                    res = iface.SignalPoll()
                    logger.debug("SignalPoll: " + str(res))
                    if 'frequency' not in res or res['frequency'] != 2412:
                        self.state = -1
                        logger.info("Unexpected SignalPoll result")
                    iface.RemoveNetwork(self.netw)
            if 'State' in properties and properties['State'] == "disconnected":
                if self.state == 1:
                    self.state = 2
                    iface.SelectNetwork(self.netw)
                elif self.state == 3:
                    self.state = 4
                    iface.Reassociate()
                elif self.state == 6:
                    self.state = 7
                    iface.Reconnect()
                elif self.state == 8:
                    self.state = 9
                    self.loop.quit()

        def run_connect(self, *args):
            logger.debug("run_connect")
            args = dbus.Dictionary({'ssid': ssid,
                                    'key_mgmt': 'WPA-PSK',
                                    'psk': passphrase,
                                    'scan_freq': 2412},
                                   signature='sv')
            self.netw = iface.AddNetwork(args)
            iface.SelectNetwork(self.netw)
            return False

        def success(self):
            if not self.network_added or \
               not self.network_removed or \
               not self.network_selected:
                return False
            return self.state == 9

    with TestDbusConnect(bus) as t:
        if not t.success():
            raise Exception("Expected signals not seen")

def test_dbus_remove_connected(dev, apdev):
    """D-Bus RemoveAllNetworks while connected"""
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    iface = dbus.Interface(if_obj, WPAS_DBUS_IFACE)

    ssid = "test-open"
    hapd = hostapd.add_ap(apdev[0], {"ssid": ssid})

    class TestDbusConnect(TestDbus):
        def __init__(self, bus):
            TestDbus.__init__(self, bus)
            self.network_added = False
            self.network_selected = False
            self.network_removed = False
            self.state = 0

        def __enter__(self):
            gobject.timeout_add(1, self.run_connect)
            gobject.timeout_add(15000, self.timeout)
            self.add_signal(self.networkAdded, WPAS_DBUS_IFACE, "NetworkAdded")
            self.add_signal(self.networkRemoved, WPAS_DBUS_IFACE,
                            "NetworkRemoved")
            self.add_signal(self.networkSelected, WPAS_DBUS_IFACE,
                            "NetworkSelected")
            self.add_signal(self.propertiesChanged, WPAS_DBUS_IFACE,
                            "PropertiesChanged")
            self.loop.run()
            return self

        def networkAdded(self, network, properties):
            logger.debug("networkAdded: %s" % str(network))
            logger.debug(str(properties))
            self.network_added = True

        def networkRemoved(self, network):
            logger.debug("networkRemoved: %s" % str(network))
            self.network_removed = True

        def networkSelected(self, network):
            logger.debug("networkSelected: %s" % str(network))
            self.network_selected = True

        def propertiesChanged(self, properties):
            logger.debug("propertiesChanged: %s" % str(properties))
            if 'State' in properties and properties['State'] == "completed":
                if self.state == 0:
                    self.state = 1
                    iface.Disconnect()
                elif self.state == 2:
                    self.state = 3
                    iface.Disconnect()
                elif self.state == 4:
                    self.state = 5
                    iface.Reattach()
                elif self.state == 5:
                    self.state = 6
                    iface.Disconnect()
                elif self.state == 7:
                    self.state = 8
                    res = iface.SignalPoll()
                    logger.debug("SignalPoll: " + str(res))
                    if 'frequency' not in res or res['frequency'] != 2412:
                        self.state = -1
                        logger.info("Unexpected SignalPoll result")
                    iface.RemoveAllNetworks()
            if 'State' in properties and properties['State'] == "disconnected":
                if self.state == 1:
                    self.state = 2
                    iface.SelectNetwork(self.netw)
                elif self.state == 3:
                    self.state = 4
                    iface.Reassociate()
                elif self.state == 6:
                    self.state = 7
                    iface.Reconnect()
                elif self.state == 8:
                    self.state = 9
                    self.loop.quit()

        def run_connect(self, *args):
            logger.debug("run_connect")
            args = dbus.Dictionary({'ssid': ssid,
                                    'key_mgmt': 'NONE',
                                    'scan_freq': 2412},
                                   signature='sv')
            self.netw = iface.AddNetwork(args)
            iface.SelectNetwork(self.netw)
            return False

        def success(self):
            if not self.network_added or \
               not self.network_removed or \
               not self.network_selected:
                return False
            return self.state == 9

    with TestDbusConnect(bus) as t:
        if not t.success():
            raise Exception("Expected signals not seen")

def test_dbus_connect_psk_mem(dev, apdev):
    """D-Bus AddNetwork and connect with memory-only PSK"""
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    iface = dbus.Interface(if_obj, WPAS_DBUS_IFACE)

    ssid = "test-wpa2-psk"
    passphrase = 'qwertyuiop'
    params = hostapd.wpa2_params(ssid=ssid, passphrase=passphrase)
    hapd = hostapd.add_ap(apdev[0], params)

    class TestDbusConnect(TestDbus):
        def __init__(self, bus):
            TestDbus.__init__(self, bus)
            self.connected = False

        def __enter__(self):
            gobject.timeout_add(1, self.run_connect)
            gobject.timeout_add(15000, self.timeout)
            self.add_signal(self.propertiesChanged, WPAS_DBUS_IFACE,
                            "PropertiesChanged")
            self.add_signal(self.networkRequest, WPAS_DBUS_IFACE,
                            "NetworkRequest")
            self.loop.run()
            return self

        def propertiesChanged(self, properties):
            logger.debug("propertiesChanged: %s" % str(properties))
            if 'State' in properties and properties['State'] == "completed":
                self.connected = True
                self.loop.quit()

        def networkRequest(self, path, field, txt):
            logger.debug("networkRequest: %s %s %s" % (path, field, txt))
            if field == "PSK_PASSPHRASE":
                iface.NetworkReply(path, field, '"' + passphrase + '"')

        def run_connect(self, *args):
            logger.debug("run_connect")
            args = dbus.Dictionary({'ssid': ssid,
                                    'key_mgmt': 'WPA-PSK',
                                    'mem_only_psk': 1,
                                    'scan_freq': 2412},
                                   signature='sv')
            self.netw = iface.AddNetwork(args)
            iface.SelectNetwork(self.netw)
            return False

        def success(self):
            return self.connected

    with TestDbusConnect(bus) as t:
        if not t.success():
            raise Exception("Expected signals not seen")

def test_dbus_connect_oom(dev, apdev):
    """D-Bus AddNetwork and connect when out-of-memory"""
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    iface = dbus.Interface(if_obj, WPAS_DBUS_IFACE)

    if "OK" not in dev[0].request("TEST_ALLOC_FAIL 0:"):
        raise HwsimSkip("TEST_ALLOC_FAIL not supported in the build")

    ssid = "test-wpa2-psk"
    passphrase = 'qwertyuiop'
    params = hostapd.wpa2_params(ssid=ssid, passphrase=passphrase)
    hapd = hostapd.add_ap(apdev[0], params)

    class TestDbusConnect(TestDbus):
        def __init__(self, bus):
            TestDbus.__init__(self, bus)
            self.network_added = False
            self.network_selected = False
            self.network_removed = False
            self.state = 0

        def __enter__(self):
            gobject.timeout_add(1, self.run_connect)
            gobject.timeout_add(1500, self.timeout)
            self.add_signal(self.networkAdded, WPAS_DBUS_IFACE, "NetworkAdded")
            self.add_signal(self.networkRemoved, WPAS_DBUS_IFACE,
                            "NetworkRemoved")
            self.add_signal(self.networkSelected, WPAS_DBUS_IFACE,
                            "NetworkSelected")
            self.add_signal(self.propertiesChanged, WPAS_DBUS_IFACE,
                            "PropertiesChanged")
            self.loop.run()
            return self

        def networkAdded(self, network, properties):
            logger.debug("networkAdded: %s" % str(network))
            logger.debug(str(properties))
            self.network_added = True

        def networkRemoved(self, network):
            logger.debug("networkRemoved: %s" % str(network))
            self.network_removed = True

        def networkSelected(self, network):
            logger.debug("networkSelected: %s" % str(network))
            self.network_selected = True

        def propertiesChanged(self, properties):
            logger.debug("propertiesChanged: %s" % str(properties))
            if 'State' in properties and properties['State'] == "completed":
                if self.state == 0:
                    self.state = 1
                    iface.Disconnect()
                elif self.state == 2:
                    self.state = 3
                    iface.Disconnect()
                elif self.state == 4:
                    self.state = 5
                    iface.Reattach()
                elif self.state == 5:
                    self.state = 6
                    res = iface.SignalPoll()
                    logger.debug("SignalPoll: " + str(res))
                    if 'frequency' not in res or res['frequency'] != 2412:
                        self.state = -1
                        logger.info("Unexpected SignalPoll result")
                    iface.RemoveNetwork(self.netw)
            if 'State' in properties and properties['State'] == "disconnected":
                if self.state == 1:
                    self.state = 2
                    iface.SelectNetwork(self.netw)
                elif self.state == 3:
                    self.state = 4
                    iface.Reassociate()
                elif self.state == 6:
                    self.state = 7
                    self.loop.quit()

        def run_connect(self, *args):
            logger.debug("run_connect")
            args = dbus.Dictionary({'ssid': ssid,
                                    'key_mgmt': 'WPA-PSK',
                                    'psk': passphrase,
                                    'scan_freq': 2412},
                                   signature='sv')
            try:
                self.netw = iface.AddNetwork(args)
            except Exception as e:
                logger.info("Exception on AddNetwork: " + str(e))
                self.loop.quit()
                return False
            try:
                iface.SelectNetwork(self.netw)
            except Exception as e:
                logger.info("Exception on SelectNetwork: " + str(e))
                self.loop.quit()

            return False

        def success(self):
            if not self.network_added or \
               not self.network_removed or \
               not self.network_selected:
                return False
            return self.state == 7

    count = 0
    for i in range(1, 1000):
        for j in range(3):
            dev[j].dump_monitor()
        dev[0].request("TEST_ALLOC_FAIL %d:main" % i)
        try:
            with TestDbusConnect(bus) as t:
                if not t.success():
                    logger.info("Iteration %d - Expected signals not seen" % i)
                else:
                    logger.info("Iteration %d - success" % i)

            state = dev[0].request('GET_ALLOC_FAIL')
            logger.info("GET_ALLOC_FAIL: " + state)
            dev[0].dump_monitor()
            dev[0].request("TEST_ALLOC_FAIL 0:")
            if i < 3:
                raise Exception("Connection succeeded during out-of-memory")
            if not state.startswith('0:'):
                count += 1
                if count == 5:
                    break
        except:
            pass

    # Force regulatory update to re-fetch hw capabilities for the following
    # test cases.
    try:
        dev[0].dump_monitor()
        subprocess.call(['iw', 'reg', 'set', 'US'])
        ev = dev[0].wait_event(["CTRL-EVENT-REGDOM-CHANGE"], timeout=1)
    finally:
        dev[0].dump_monitor()
        subprocess.call(['iw', 'reg', 'set', '00'])
        ev = dev[0].wait_event(["CTRL-EVENT-REGDOM-CHANGE"], timeout=1)

def test_dbus_while_not_connected(dev, apdev):
    """D-Bus invalid operations while not connected"""
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    iface = dbus.Interface(if_obj, WPAS_DBUS_IFACE)

    try:
        iface.Disconnect()
        raise Exception("Disconnect() accepted when not connected")
    except dbus.exceptions.DBusException as e:
        if "NotConnected" not in str(e):
            raise Exception("Unexpected error message for invalid Disconnect: " + str(e))

    try:
        iface.Reattach()
        raise Exception("Reattach() accepted when not connected")
    except dbus.exceptions.DBusException as e:
        if "NotConnected" not in str(e):
            raise Exception("Unexpected error message for invalid Reattach: " + str(e))

def test_dbus_connect_eap(dev, apdev):
    """D-Bus AddNetwork and connect to EAP network"""
    check_altsubject_match_support(dev[0])
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    iface = dbus.Interface(if_obj, WPAS_DBUS_IFACE)

    ssid = "ieee8021x-open"
    params = hostapd.radius_params()
    params["ssid"] = ssid
    params["ieee8021x"] = "1"
    hapd = hostapd.add_ap(apdev[0], params)

    class TestDbusConnect(TestDbus):
        def __init__(self, bus):
            TestDbus.__init__(self, bus)
            self.certification_received = False
            self.eap_status = False
            self.state = 0

        def __enter__(self):
            gobject.timeout_add(1, self.run_connect)
            gobject.timeout_add(15000, self.timeout)
            self.add_signal(self.propertiesChanged, WPAS_DBUS_IFACE,
                            "PropertiesChanged")
            self.add_signal(self.certification, WPAS_DBUS_IFACE,
                            "Certification", byte_arrays=True)
            self.add_signal(self.networkRequest, WPAS_DBUS_IFACE,
                            "NetworkRequest")
            self.add_signal(self.eap, WPAS_DBUS_IFACE, "EAP")
            self.loop.run()
            return self

        def propertiesChanged(self, properties):
            logger.debug("propertiesChanged: %s" % str(properties))
            if 'State' in properties and properties['State'] == "completed":
                if self.state == 0:
                    self.state = 1
                    iface.EAPLogoff()
                    logger.info("Set dNSName constraint")
                    net_obj = bus.get_object(WPAS_DBUS_SERVICE, self.netw)
                    args = dbus.Dictionary({'altsubject_match':
                                            self.server_dnsname},
                                           signature='sv')
                    net_obj.Set(WPAS_DBUS_NETWORK, "Properties", args,
                                dbus_interface=dbus.PROPERTIES_IFACE)
                elif self.state == 2:
                    self.state = 3
                    iface.Disconnect()
                    logger.info("Set non-matching dNSName constraint")
                    net_obj = bus.get_object(WPAS_DBUS_SERVICE, self.netw)
                    args = dbus.Dictionary({'altsubject_match':
                                            self.server_dnsname + "FOO"},
                                           signature='sv')
                    net_obj.Set(WPAS_DBUS_NETWORK, "Properties", args,
                                dbus_interface=dbus.PROPERTIES_IFACE)
            if 'State' in properties and properties['State'] == "disconnected":
                if self.state == 1:
                    self.state = 2
                    iface.EAPLogon()
                    iface.SelectNetwork(self.netw)
                if self.state == 3:
                    self.state = 4
                    iface.SelectNetwork(self.netw)

        def certification(self, args):
            logger.debug("certification: %s" % str(args))
            self.certification_received = True
            if args['depth'] == 0:
                # The test server certificate is supposed to have dNSName
                if len(args['altsubject']) < 1:
                    raise Exception("Missing dNSName")
                dnsname = args['altsubject'][0]
                if not dnsname.startswith("DNS:"):
                    raise Exception("Expected dNSName not found: " + dnsname)
                logger.info("altsubject: " + dnsname)
                self.server_dnsname = dnsname

        def eap(self, status, parameter):
            logger.debug("EAP: status=%s parameter=%s" % (status, parameter))
            if status == 'completion' and parameter == 'success':
                self.eap_status = True
            if self.state == 4 and status == 'remote certificate verification' and parameter == 'AltSubject mismatch':
                self.state = 5
                self.loop.quit()

        def networkRequest(self, path, field, txt):
            logger.debug("networkRequest: %s %s %s" % (path, field, txt))
            if field == "PASSWORD":
                iface.NetworkReply(path, field, "password")

        def run_connect(self, *args):
            logger.debug("run_connect")
            args = dbus.Dictionary({'ssid': ssid,
                                    'key_mgmt': 'IEEE8021X',
                                    'eapol_flags': 0,
                                    'eap': 'TTLS',
                                    'anonymous_identity': 'ttls',
                                    'identity': 'pap user',
                                    'ca_cert': 'auth_serv/ca.pem',
                                    'phase2': 'auth=PAP',
                                    'scan_freq': 2412},
                                   signature='sv')
            self.netw = iface.AddNetwork(args)
            iface.SelectNetwork(self.netw)
            return False

        def success(self):
            if not self.eap_status or not self.certification_received:
                return False
            return self.state == 5

    with TestDbusConnect(bus) as t:
        if not t.success():
            raise Exception("Expected signals not seen")

def test_dbus_network(dev, apdev):
    """D-Bus AddNetwork/RemoveNetwork parameters and error cases"""
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    iface = dbus.Interface(if_obj, WPAS_DBUS_IFACE)

    args = dbus.Dictionary({'ssid': "foo",
                            'key_mgmt': 'WPA-PSK',
                            'psk': "12345678",
                            'identity': dbus.ByteArray([1, 2]),
                            'priority': dbus.Int32(0),
                            'scan_freq': dbus.UInt32(2412)},
                           signature='sv')
    netw = iface.AddNetwork(args)
    id = int(dev[0].list_networks()[0]['id'])
    val = dev[0].get_network(id, "scan_freq")
    if val != "2412":
        raise Exception("Invalid scan_freq value: " + str(val))
    iface.RemoveNetwork(netw)

    args = dbus.Dictionary({'ssid': "foo",
                            'key_mgmt': 'NONE',
                            'scan_freq': "2412 2432",
                            'freq_list': "2412 2417 2432"},
                           signature='sv')
    netw = iface.AddNetwork(args)
    id = int(dev[0].list_networks()[0]['id'])
    val = dev[0].get_network(id, "scan_freq")
    if val != "2412 2432":
        raise Exception("Invalid scan_freq value (2): " + str(val))
    val = dev[0].get_network(id, "freq_list")
    if val != "2412 2417 2432":
        raise Exception("Invalid freq_list value: " + str(val))
    iface.RemoveNetwork(netw)
    try:
        iface.RemoveNetwork(netw)
        raise Exception("Invalid RemoveNetwork() accepted")
    except dbus.exceptions.DBusException as e:
        if "NetworkUnknown" not in str(e):
            raise Exception("Unexpected error message for invalid RemoveNetwork: " + str(e))
    try:
        iface.SelectNetwork(netw)
        raise Exception("Invalid SelectNetwork() accepted")
    except dbus.exceptions.DBusException as e:
        if "NetworkUnknown" not in str(e):
            raise Exception("Unexpected error message for invalid RemoveNetwork: " + str(e))

    args = dbus.Dictionary({'ssid': "foo1", 'key_mgmt': 'NONE',
                            'identity': "testuser", 'scan_freq': '2412'},
                           signature='sv')
    netw1 = iface.AddNetwork(args)
    args = dbus.Dictionary({'ssid': "foo2", 'key_mgmt': 'NONE'},
                           signature='sv')
    netw2 = iface.AddNetwork(args)
    res = if_obj.Get(WPAS_DBUS_IFACE, "Networks",
                     dbus_interface=dbus.PROPERTIES_IFACE)
    if len(res) != 2:
        raise Exception("Unexpected number of networks")

    net_obj = bus.get_object(WPAS_DBUS_SERVICE, netw1)
    res = net_obj.Get(WPAS_DBUS_NETWORK, "Enabled",
                      dbus_interface=dbus.PROPERTIES_IFACE)
    if res != False:
        raise Exception("Added network was unexpectedly enabled by default")
    net_obj.Set(WPAS_DBUS_NETWORK, "Enabled", dbus.Boolean(True),
                dbus_interface=dbus.PROPERTIES_IFACE)
    res = net_obj.Get(WPAS_DBUS_NETWORK, "Enabled",
                      dbus_interface=dbus.PROPERTIES_IFACE)
    if res != True:
        raise Exception("Set(Enabled,True) did not seem to change property value")
    net_obj.Set(WPAS_DBUS_NETWORK, "Enabled", dbus.Boolean(False),
                dbus_interface=dbus.PROPERTIES_IFACE)
    res = net_obj.Get(WPAS_DBUS_NETWORK, "Enabled",
                      dbus_interface=dbus.PROPERTIES_IFACE)
    if res != False:
        raise Exception("Set(Enabled,False) did not seem to change property value")
    try:
        net_obj.Set(WPAS_DBUS_NETWORK, "Enabled", dbus.UInt32(1),
                    dbus_interface=dbus.PROPERTIES_IFACE)
        raise Exception("Invalid Set(Enabled,1) accepted")
    except dbus.exceptions.DBusException as e:
        if "Error.Failed: wrong property type" not in str(e):
            raise Exception("Unexpected error message for invalid Set(Enabled,1): " + str(e))

    args = dbus.Dictionary({'ssid': "foo1new"}, signature='sv')
    net_obj.Set(WPAS_DBUS_NETWORK, "Properties", args,
                dbus_interface=dbus.PROPERTIES_IFACE)
    res = net_obj.Get(WPAS_DBUS_NETWORK, "Properties",
                      dbus_interface=dbus.PROPERTIES_IFACE)
    if res['ssid'] != '"foo1new"':
        raise Exception("Set(Properties) failed to update ssid")
    if res['identity'] != '"testuser"':
        raise Exception("Set(Properties) unexpectedly changed unrelated parameter")

    iface.RemoveAllNetworks()
    res = if_obj.Get(WPAS_DBUS_IFACE, "Networks",
                     dbus_interface=dbus.PROPERTIES_IFACE)
    if len(res) != 0:
        raise Exception("Unexpected number of networks")
    iface.RemoveAllNetworks()

    tests = [dbus.Dictionary({'psk': "1234567"}, signature='sv'),
             dbus.Dictionary({'identity': dbus.ByteArray()},
                             signature='sv'),
             dbus.Dictionary({'identity': dbus.Byte(1)}, signature='sv')]
    for args in tests:
        try:
            iface.AddNetwork(args)
            raise Exception("Invalid AddNetwork args accepted: " + str(args))
        except dbus.exceptions.DBusException as e:
            if "InvalidArgs" not in str(e):
                raise Exception("Unexpected error message for invalid AddNetwork: " + str(e))

def test_dbus_network_oom(dev, apdev):
    """D-Bus AddNetwork/RemoveNetwork parameters and OOM error cases"""
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    iface = dbus.Interface(if_obj, WPAS_DBUS_IFACE)

    args = dbus.Dictionary({'ssid': "foo1", 'key_mgmt': 'NONE',
                            'identity': "testuser", 'scan_freq': '2412'},
                           signature='sv')
    netw1 = iface.AddNetwork(args)
    net_obj = bus.get_object(WPAS_DBUS_SERVICE, netw1)

    with alloc_fail_dbus(dev[0], 1,
                         "wpa_config_get_all;wpas_dbus_getter_network_properties",
                         "Get"):
        net_obj.Get(WPAS_DBUS_NETWORK, "Properties",
                    dbus_interface=dbus.PROPERTIES_IFACE)

    iface.RemoveAllNetworks()

    with alloc_fail_dbus(dev[0], 1,
                         "wpas_dbus_new_decompose_object_path;wpas_dbus_handler_remove_network",
                         "RemoveNetwork", "InvalidArgs"):
        iface.RemoveNetwork(dbus.ObjectPath("/fi/w1/wpa_supplicant1/Interfaces/1234/Networks/1234"))

    with alloc_fail(dev[0], 1, "wpa_dbus_register_object_per_iface;wpas_dbus_register_network"):
        args = dbus.Dictionary({'ssid': "foo2", 'key_mgmt': 'NONE'},
                               signature='sv')
        try:
            netw = iface.AddNetwork(args)
            # Currently, AddNetwork() succeeds even if os_strdup() for path
            # fails, so remove the network if that occurs.
            iface.RemoveNetwork(netw)
        except dbus.exceptions.DBusException as e:
            pass

    for i in range(1, 3):
        with alloc_fail(dev[0], i, "=wpas_dbus_register_network"):
            try:
                netw = iface.AddNetwork(args)
                # Currently, AddNetwork() succeeds even if network registration
                # fails, so remove the network if that occurs.
                iface.RemoveNetwork(netw)
            except dbus.exceptions.DBusException as e:
                pass

    with alloc_fail_dbus(dev[0], 1,
                         "=wpa_config_add_network;wpas_dbus_handler_add_network",
                         "AddNetwork",
                         "UnknownError: wpa_supplicant could not add a network"):
        args = dbus.Dictionary({'ssid': "foo2", 'key_mgmt': 'NONE'},
                               signature='sv')
        netw = iface.AddNetwork(args)

    tests = [(1,
              'wpa_dbus_dict_get_entry;set_network_properties;wpas_dbus_handler_add_network',
              dbus.Dictionary({'ssid': dbus.ByteArray(b' ')},
                              signature='sv')),
             (1, '=set_network_properties;wpas_dbus_handler_add_network',
              dbus.Dictionary({'ssid': 'foo'}, signature='sv')),
             (1, '=set_network_properties;wpas_dbus_handler_add_network',
              dbus.Dictionary({'eap': 'foo'}, signature='sv')),
             (1, '=set_network_properties;wpas_dbus_handler_add_network',
              dbus.Dictionary({'priority': dbus.UInt32(1)},
                              signature='sv')),
             (1, '=set_network_properties;wpas_dbus_handler_add_network',
              dbus.Dictionary({'priority': dbus.Int32(1)},
                              signature='sv')),
             (1, '=set_network_properties;wpas_dbus_handler_add_network',
              dbus.Dictionary({'ssid': dbus.ByteArray(b' ')},
                              signature='sv'))]
    for (count, funcs, args) in tests:
        with alloc_fail_dbus(dev[0], count, funcs, "AddNetwork", "InvalidArgs"):
            netw = iface.AddNetwork(args)

    if len(if_obj.Get(WPAS_DBUS_IFACE, 'Networks',
                      dbus_interface=dbus.PROPERTIES_IFACE)) > 0:
        raise Exception("Unexpected network block added")
    if len(dev[0].list_networks()) > 0:
        raise Exception("Unexpected network block visible")

def test_dbus_interface(dev, apdev):
    """D-Bus CreateInterface/GetInterface/RemoveInterface parameters and error cases"""
    try:
        _test_dbus_interface(dev, apdev)
    finally:
        # Need to force P2P channel list update since the 'lo' interface
        # with driver=none ends up configuring default dualband channels.
        dev[0].request("SET country US")
        ev = dev[0].wait_event(["CTRL-EVENT-REGDOM-CHANGE"], timeout=1)
        if ev is None:
            ev = dev[0].wait_global_event(["CTRL-EVENT-REGDOM-CHANGE"],
                                          timeout=1)
        dev[0].request("SET country 00")
        ev = dev[0].wait_event(["CTRL-EVENT-REGDOM-CHANGE"], timeout=1)
        if ev is None:
            ev = dev[0].wait_global_event(["CTRL-EVENT-REGDOM-CHANGE"],
                                          timeout=1)
        subprocess.call(['iw', 'reg', 'set', '00'])

def _test_dbus_interface(dev, apdev):
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    wpas = dbus.Interface(wpas_obj, WPAS_DBUS_SERVICE)

    params = dbus.Dictionary({'Ifname': 'lo', 'Driver': 'none'},
                             signature='sv')
    path = wpas.CreateInterface(params)
    logger.debug("New interface path: " + str(path))
    path2 = wpas.GetInterface("lo")
    if path != path2:
        raise Exception("Interface object mismatch")

    params = dbus.Dictionary({'Ifname': 'lo',
                              'Driver': 'none',
                              'ConfigFile': 'foo',
                              'BridgeIfname': 'foo',},
                             signature='sv')
    try:
        wpas.CreateInterface(params)
        raise Exception("Invalid CreateInterface() accepted")
    except dbus.exceptions.DBusException as e:
        if "InterfaceExists" not in str(e):
            raise Exception("Unexpected error message for invalid CreateInterface: " + str(e))

    wpas.RemoveInterface(path)
    try:
        wpas.RemoveInterface(path)
        raise Exception("Invalid RemoveInterface() accepted")
    except dbus.exceptions.DBusException as e:
        if "InterfaceUnknown" not in str(e):
            raise Exception("Unexpected error message for invalid RemoveInterface: " + str(e))

    params = dbus.Dictionary({'Ifname': 'lo', 'Driver': 'none',
                              'Foo': 123},
                             signature='sv')
    try:
        wpas.CreateInterface(params)
        raise Exception("Invalid CreateInterface() accepted")
    except dbus.exceptions.DBusException as e:
        if "InvalidArgs" not in str(e):
            raise Exception("Unexpected error message for invalid CreateInterface: " + str(e))

    params = dbus.Dictionary({'Driver': 'none'}, signature='sv')
    try:
        wpas.CreateInterface(params)
        raise Exception("Invalid CreateInterface() accepted")
    except dbus.exceptions.DBusException as e:
        if "InvalidArgs" not in str(e):
            raise Exception("Unexpected error message for invalid CreateInterface: " + str(e))

    try:
        wpas.GetInterface("lo")
        raise Exception("Invalid GetInterface() accepted")
    except dbus.exceptions.DBusException as e:
        if "InterfaceUnknown" not in str(e):
            raise Exception("Unexpected error message for invalid RemoveInterface: " + str(e))

def test_dbus_interface_oom(dev, apdev):
    """D-Bus CreateInterface/GetInterface/RemoveInterface OOM error cases"""
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    wpas = dbus.Interface(wpas_obj, WPAS_DBUS_SERVICE)

    with alloc_fail_dbus(dev[0], 1, "wpa_dbus_dict_get_entry;wpas_dbus_handler_create_interface", "CreateInterface", "InvalidArgs"):
        params = dbus.Dictionary({'Ifname': 'lo', 'Driver': 'none'},
                                 signature='sv')
        wpas.CreateInterface(params)

    for i in range(1, 1000):
        dev[0].request("TEST_ALLOC_FAIL %d:wpa_supplicant_add_iface;wpas_dbus_handler_create_interface" % i)
        params = dbus.Dictionary({'Ifname': 'lo', 'Driver': 'none'},
                                 signature='sv')
        try:
            npath = wpas.CreateInterface(params)
            wpas.RemoveInterface(npath)
            logger.info("CreateInterface succeeds after %d allocation failures" % i)
            state = dev[0].request('GET_ALLOC_FAIL')
            logger.info("GET_ALLOC_FAIL: " + state)
            dev[0].dump_monitor()
            dev[0].request("TEST_ALLOC_FAIL 0:")
            if i < 5:
                raise Exception("CreateInterface succeeded during out-of-memory")
            if not state.startswith('0:'):
                break
        except dbus.exceptions.DBusException as e:
            pass

    for arg in ['Driver', 'Ifname', 'ConfigFile', 'BridgeIfname']:
        with alloc_fail_dbus(dev[0], 1, "=wpas_dbus_handler_create_interface",
                             "CreateInterface"):
            params = dbus.Dictionary({arg: 'foo'}, signature='sv')
            wpas.CreateInterface(params)

def test_dbus_blob(dev, apdev):
    """D-Bus AddNetwork/RemoveNetwork parameters and error cases"""
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    iface = dbus.Interface(if_obj, WPAS_DBUS_IFACE)

    blob = dbus.ByteArray(b"\x01\x02\x03")
    iface.AddBlob('blob1', blob)
    try:
        iface.AddBlob('blob1', dbus.ByteArray(b"\x01\x02\x04"))
        raise Exception("Invalid AddBlob() accepted")
    except dbus.exceptions.DBusException as e:
        if "BlobExists" not in str(e):
            raise Exception("Unexpected error message for invalid AddBlob: " + str(e))
    res = iface.GetBlob('blob1')
    if len(res) != len(blob):
        raise Exception("Unexpected blob data length")
    for i in range(len(res)):
        if res[i] != dbus.Byte(blob[i]):
            raise Exception("Unexpected blob data")
    res = if_obj.Get(WPAS_DBUS_IFACE, "Blobs",
                     dbus_interface=dbus.PROPERTIES_IFACE)
    if 'blob1' not in res:
        raise Exception("Added blob missing from Blobs property")
    iface.RemoveBlob('blob1')
    try:
        iface.RemoveBlob('blob1')
        raise Exception("Invalid RemoveBlob() accepted")
    except dbus.exceptions.DBusException as e:
        if "BlobUnknown" not in str(e):
            raise Exception("Unexpected error message for invalid RemoveBlob: " + str(e))
    try:
        iface.GetBlob('blob1')
        raise Exception("Invalid GetBlob() accepted")
    except dbus.exceptions.DBusException as e:
        if "BlobUnknown" not in str(e):
            raise Exception("Unexpected error message for invalid GetBlob: " + str(e))

    class TestDbusBlob(TestDbus):
        def __init__(self, bus):
            TestDbus.__init__(self, bus)
            self.blob_added = False
            self.blob_removed = False

        def __enter__(self):
            gobject.timeout_add(1, self.run_blob)
            gobject.timeout_add(15000, self.timeout)
            self.add_signal(self.blobAdded, WPAS_DBUS_IFACE, "BlobAdded")
            self.add_signal(self.blobRemoved, WPAS_DBUS_IFACE, "BlobRemoved")
            self.loop.run()
            return self

        def blobAdded(self, blobName):
            logger.debug("blobAdded: %s" % blobName)
            if blobName == 'blob2':
                self.blob_added = True

        def blobRemoved(self, blobName):
            logger.debug("blobRemoved: %s" % blobName)
            if blobName == 'blob2':
                self.blob_removed = True
                self.loop.quit()

        def run_blob(self, *args):
            logger.debug("run_blob")
            iface.AddBlob('blob2', dbus.ByteArray(b"\x01\x02\x04"))
            iface.RemoveBlob('blob2')
            return False

        def success(self):
            return self.blob_added and self.blob_removed

    with TestDbusBlob(bus) as t:
        if not t.success():
            raise Exception("Expected signals not seen")

def test_dbus_blob_oom(dev, apdev):
    """D-Bus AddNetwork/RemoveNetwork OOM error cases"""
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    iface = dbus.Interface(if_obj, WPAS_DBUS_IFACE)

    for i in range(1, 4):
        with alloc_fail_dbus(dev[0], i, "wpas_dbus_handler_add_blob",
                             "AddBlob"):
            iface.AddBlob('blob_no_mem', dbus.ByteArray(b"\x01\x02\x03\x04"))

def test_dbus_autoscan(dev, apdev):
    """D-Bus Autoscan()"""
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    iface = dbus.Interface(if_obj, WPAS_DBUS_IFACE)

    iface.AutoScan("foo")
    iface.AutoScan("periodic:1")
    iface.AutoScan("")
    dev[0].request("AUTOSCAN ")

def test_dbus_autoscan_oom(dev, apdev):
    """D-Bus Autoscan() OOM"""
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    iface = dbus.Interface(if_obj, WPAS_DBUS_IFACE)

    with alloc_fail_dbus(dev[0], 1, "wpas_dbus_handler_autoscan", "AutoScan"):
        iface.AutoScan("foo")
    dev[0].request("AUTOSCAN ")

def test_dbus_tdls_invalid(dev, apdev):
    """D-Bus invalid TDLS operations"""
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    iface = dbus.Interface(if_obj, WPAS_DBUS_IFACE)

    hapd = hostapd.add_ap(apdev[0], {"ssid": "test-open"})
    connect_2sta_open(dev, hapd)
    addr1 = dev[1].p2p_interface_addr()

    try:
        iface.TDLSDiscover("foo")
        raise Exception("Invalid TDLSDiscover() accepted")
    except dbus.exceptions.DBusException as e:
        if "InvalidArgs" not in str(e):
            raise Exception("Unexpected error message for invalid TDLSDiscover: " + str(e))

    try:
        iface.TDLSStatus("foo")
        raise Exception("Invalid TDLSStatus() accepted")
    except dbus.exceptions.DBusException as e:
        if "InvalidArgs" not in str(e):
            raise Exception("Unexpected error message for invalid TDLSStatus: " + str(e))

    res = iface.TDLSStatus(addr1)
    if res != "peer does not exist":
        raise Exception("Unexpected TDLSStatus response")

    try:
        iface.TDLSSetup("foo")
        raise Exception("Invalid TDLSSetup() accepted")
    except dbus.exceptions.DBusException as e:
        if "InvalidArgs" not in str(e):
            raise Exception("Unexpected error message for invalid TDLSSetup: " + str(e))

    try:
        iface.TDLSTeardown("foo")
        raise Exception("Invalid TDLSTeardown() accepted")
    except dbus.exceptions.DBusException as e:
        if "InvalidArgs" not in str(e):
            raise Exception("Unexpected error message for invalid TDLSTeardown: " + str(e))

    try:
        iface.TDLSTeardown("00:11:22:33:44:55")
        raise Exception("TDLSTeardown accepted for unknown peer")
    except dbus.exceptions.DBusException as e:
        if "UnknownError: error performing TDLS teardown" not in str(e):
            raise Exception("Unexpected error message: " + str(e))

    try:
        iface.TDLSChannelSwitch({})
        raise Exception("Invalid TDLSChannelSwitch() accepted")
    except dbus.exceptions.DBusException as e:
        if "InvalidArgs" not in str(e):
            raise Exception("Unexpected error message for invalid TDLSChannelSwitch: " + str(e))

    try:
        iface.TDLSCancelChannelSwitch("foo")
        raise Exception("Invalid TDLSCancelChannelSwitch() accepted")
    except dbus.exceptions.DBusException as e:
        if "InvalidArgs" not in str(e):
            raise Exception("Unexpected error message for invalid TDLSCancelChannelSwitch: " + str(e))

def test_dbus_tdls_oom(dev, apdev):
    """D-Bus TDLS operations during OOM"""
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    iface = dbus.Interface(if_obj, WPAS_DBUS_IFACE)

    with alloc_fail_dbus(dev[0], 1, "wpa_tdls_add_peer", "TDLSSetup",
                         "UnknownError: error performing TDLS setup"):
        iface.TDLSSetup("00:11:22:33:44:55")

def test_dbus_tdls(dev, apdev):
    """D-Bus TDLS"""
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    iface = dbus.Interface(if_obj, WPAS_DBUS_IFACE)

    hapd = hostapd.add_ap(apdev[0], {"ssid": "test-open"})
    connect_2sta_open(dev, hapd)

    addr1 = dev[1].p2p_interface_addr()

    class TestDbusTdls(TestDbus):
        def __init__(self, bus):
            TestDbus.__init__(self, bus)
            self.tdls_setup = False
            self.tdls_teardown = False

        def __enter__(self):
            gobject.timeout_add(1, self.run_tdls)
            gobject.timeout_add(15000, self.timeout)
            self.add_signal(self.propertiesChanged, WPAS_DBUS_IFACE,
                            "PropertiesChanged")
            self.loop.run()
            return self

        def propertiesChanged(self, properties):
            logger.debug("propertiesChanged: %s" % str(properties))

        def run_tdls(self, *args):
            logger.debug("run_tdls")
            iface.TDLSDiscover(addr1)
            gobject.timeout_add(100, self.run_tdls2)
            return False

        def run_tdls2(self, *args):
            logger.debug("run_tdls2")
            iface.TDLSSetup(addr1)
            gobject.timeout_add(500, self.run_tdls3)
            return False

        def run_tdls3(self, *args):
            logger.debug("run_tdls3")
            res = iface.TDLSStatus(addr1)
            if res == "connected":
                self.tdls_setup = True
            else:
                logger.info("Unexpected TDLSStatus: " + res)
            iface.TDLSTeardown(addr1)
            gobject.timeout_add(200, self.run_tdls4)
            return False

        def run_tdls4(self, *args):
            logger.debug("run_tdls4")
            res = iface.TDLSStatus(addr1)
            if res == "peer does not exist":
                self.tdls_teardown = True
            else:
                logger.info("Unexpected TDLSStatus: " + res)
            self.loop.quit()
            return False

        def success(self):
            return self.tdls_setup and self.tdls_teardown

    with TestDbusTdls(bus) as t:
        if not t.success():
            raise Exception("Expected signals not seen")

def test_dbus_tdls_channel_switch(dev, apdev):
    """D-Bus TDLS channel switch configuration"""
    flags = int(dev[0].get_driver_status_field('capa.flags'), 16)
    if flags & 0x800000000 == 0:
        raise HwsimSkip("Driver does not support TDLS channel switching")

    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    iface = dbus.Interface(if_obj, WPAS_DBUS_IFACE)

    hapd = hostapd.add_ap(apdev[0], {"ssid": "test-open"})
    connect_2sta_open(dev, hapd)

    addr1 = dev[1].p2p_interface_addr()

    class TestDbusTdls(TestDbus):
        def __init__(self, bus):
            TestDbus.__init__(self, bus)
            self.tdls_setup = False
            self.tdls_done = False

        def __enter__(self):
            gobject.timeout_add(1, self.run_tdls)
            gobject.timeout_add(15000, self.timeout)
            self.add_signal(self.propertiesChanged, WPAS_DBUS_IFACE,
                            "PropertiesChanged")
            self.loop.run()
            return self

        def propertiesChanged(self, properties):
            logger.debug("propertiesChanged: %s" % str(properties))

        def run_tdls(self, *args):
            logger.debug("run_tdls")
            iface.TDLSDiscover(addr1)
            gobject.timeout_add(100, self.run_tdls2)
            return False

        def run_tdls2(self, *args):
            logger.debug("run_tdls2")
            iface.TDLSSetup(addr1)
            gobject.timeout_add(500, self.run_tdls3)
            return False

        def run_tdls3(self, *args):
            logger.debug("run_tdls3")
            res = iface.TDLSStatus(addr1)
            if res == "connected":
                self.tdls_setup = True
            else:
                logger.info("Unexpected TDLSStatus: " + res)

            # Unknown dict entry
            args = dbus.Dictionary({'Foobar': dbus.Byte(1)},
                                   signature='sv')
            try:
                iface.TDLSChannelSwitch(args)
            except Exception as e:
                if "InvalidArgs" not in str(e):
                    raise Exception("Unexpected exception")

            # Missing OperClass
            args = dbus.Dictionary({}, signature='sv')
            try:
                iface.TDLSChannelSwitch(args)
            except Exception as e:
                if "InvalidArgs" not in str(e):
                    raise Exception("Unexpected exception")

            # Missing Frequency
            args = dbus.Dictionary({'OperClass': dbus.Byte(1)},
                                   signature='sv')
            try:
                iface.TDLSChannelSwitch(args)
            except Exception as e:
                if "InvalidArgs" not in str(e):
                    raise Exception("Unexpected exception")

            # Missing PeerAddress
            args = dbus.Dictionary({'OperClass': dbus.Byte(1),
                                     'Frequency': dbus.UInt32(2417)},
                                   signature='sv')
            try:
                iface.TDLSChannelSwitch(args)
            except Exception as e:
                if "InvalidArgs" not in str(e):
                    raise Exception("Unexpected exception")

            # Valid parameters
            args = dbus.Dictionary({'OperClass': dbus.Byte(1),
                                    'Frequency': dbus.UInt32(2417),
                                    'PeerAddress': addr1,
                                    'SecChannelOffset': dbus.UInt32(0),
                                    'CenterFrequency1': dbus.UInt32(0),
                                    'CenterFrequency2': dbus.UInt32(0),
                                    'Bandwidth': dbus.UInt32(20),
                                    'HT': dbus.Boolean(False),
                                    'VHT': dbus.Boolean(False)},
                                   signature='sv')
            iface.TDLSChannelSwitch(args)

            gobject.timeout_add(200, self.run_tdls4)
            return False

        def run_tdls4(self, *args):
            logger.debug("run_tdls4")
            iface.TDLSCancelChannelSwitch(addr1)
            self.tdls_done = True
            self.loop.quit()
            return False

        def success(self):
            return self.tdls_setup and self.tdls_done

    with TestDbusTdls(bus) as t:
        if not t.success():
            raise Exception("Expected signals not seen")

def test_dbus_pkcs11(dev, apdev):
    """D-Bus SetPKCS11EngineAndModulePath()"""
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    iface = dbus.Interface(if_obj, WPAS_DBUS_IFACE)

    try:
        iface.SetPKCS11EngineAndModulePath("foo", "bar")
    except dbus.exceptions.DBusException as e:
        if "Error.Failed: Reinit of the EAPOL" not in str(e):
            raise Exception("Unexpected error message for invalid SetPKCS11EngineAndModulePath: " + str(e))

    try:
        iface.SetPKCS11EngineAndModulePath("foo", "")
    except dbus.exceptions.DBusException as e:
        if "Error.Failed: Reinit of the EAPOL" not in str(e):
            raise Exception("Unexpected error message for invalid SetPKCS11EngineAndModulePath: " + str(e))

    iface.SetPKCS11EngineAndModulePath("", "bar")
    res = if_obj.Get(WPAS_DBUS_IFACE, "PKCS11EnginePath",
                     dbus_interface=dbus.PROPERTIES_IFACE)
    if res != "":
        raise Exception("Unexpected PKCS11EnginePath value: " + res)
    res = if_obj.Get(WPAS_DBUS_IFACE, "PKCS11ModulePath",
                     dbus_interface=dbus.PROPERTIES_IFACE)
    if res != "bar":
        raise Exception("Unexpected PKCS11ModulePath value: " + res)

    iface.SetPKCS11EngineAndModulePath("", "")
    res = if_obj.Get(WPAS_DBUS_IFACE, "PKCS11EnginePath",
                     dbus_interface=dbus.PROPERTIES_IFACE)
    if res != "":
        raise Exception("Unexpected PKCS11EnginePath value: " + res)
    res = if_obj.Get(WPAS_DBUS_IFACE, "PKCS11ModulePath",
                     dbus_interface=dbus.PROPERTIES_IFACE)
    if res != "":
        raise Exception("Unexpected PKCS11ModulePath value: " + res)

def test_dbus_apscan(dev, apdev):
    """D-Bus Get/Set ApScan"""
    try:
        _test_dbus_apscan(dev, apdev)
    finally:
        dev[0].request("AP_SCAN 1")

def _test_dbus_apscan(dev, apdev):
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])

    res = if_obj.Get(WPAS_DBUS_IFACE, "ApScan",
                     dbus_interface=dbus.PROPERTIES_IFACE)
    if res != 1:
        raise Exception("Unexpected initial ApScan value: %d" % res)

    for i in range(3):
        if_obj.Set(WPAS_DBUS_IFACE, "ApScan", dbus.UInt32(i),
                     dbus_interface=dbus.PROPERTIES_IFACE)
        res = if_obj.Get(WPAS_DBUS_IFACE, "ApScan",
                         dbus_interface=dbus.PROPERTIES_IFACE)
        if res != i:
            raise Exception("Unexpected ApScan value %d (expected %d)" % (res, i))

    try:
        if_obj.Set(WPAS_DBUS_IFACE, "ApScan", dbus.Int16(-1),
                   dbus_interface=dbus.PROPERTIES_IFACE)
        raise Exception("Invalid Set(ApScan,-1) accepted")
    except dbus.exceptions.DBusException as e:
        if "Error.Failed: wrong property type" not in str(e):
            raise Exception("Unexpected error message for invalid Set(ApScan,-1): " + str(e))

    try:
        if_obj.Set(WPAS_DBUS_IFACE, "ApScan", dbus.UInt32(123),
                   dbus_interface=dbus.PROPERTIES_IFACE)
        raise Exception("Invalid Set(ApScan,123) accepted")
    except dbus.exceptions.DBusException as e:
        if "Error.Failed: ap_scan must be 0, 1, or 2" not in str(e):
            raise Exception("Unexpected error message for invalid Set(ApScan,123): " + str(e))

    if_obj.Set(WPAS_DBUS_IFACE, "ApScan", dbus.UInt32(1),
               dbus_interface=dbus.PROPERTIES_IFACE)

def test_dbus_pmf(dev, apdev):
    """D-Bus Get/Set Pmf"""
    try:
        _test_dbus_pmf(dev, apdev)
    finally:
        dev[0].request("SET pmf 0")

def _test_dbus_pmf(dev, apdev):
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])

    dev[0].set("pmf", "0")
    res = if_obj.Get(WPAS_DBUS_IFACE, "Pmf",
                     dbus_interface=dbus.PROPERTIES_IFACE)
    if res != "0":
        raise Exception("Unexpected initial Pmf value: %s" % res)

    for i in range(3):
        if_obj.Set(WPAS_DBUS_IFACE, "Pmf", str(i),
                   dbus_interface=dbus.PROPERTIES_IFACE)
        res = if_obj.Get(WPAS_DBUS_IFACE, "Pmf",
                         dbus_interface=dbus.PROPERTIES_IFACE)
        if res != str(i):
            raise Exception("Unexpected Pmf value %s (expected %d)" % (res, i))

    if_obj.Set(WPAS_DBUS_IFACE, "Pmf", "1",
               dbus_interface=dbus.PROPERTIES_IFACE)

def test_dbus_fastreauth(dev, apdev):
    """D-Bus Get/Set FastReauth"""
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])

    res = if_obj.Get(WPAS_DBUS_IFACE, "FastReauth",
                     dbus_interface=dbus.PROPERTIES_IFACE)
    if res != True:
        raise Exception("Unexpected initial FastReauth value: " + str(res))

    for i in [False, True]:
        if_obj.Set(WPAS_DBUS_IFACE, "FastReauth", dbus.Boolean(i),
                     dbus_interface=dbus.PROPERTIES_IFACE)
        res = if_obj.Get(WPAS_DBUS_IFACE, "FastReauth",
                         dbus_interface=dbus.PROPERTIES_IFACE)
        if res != i:
            raise Exception("Unexpected FastReauth value %d (expected %d)" % (res, i))

    try:
        if_obj.Set(WPAS_DBUS_IFACE, "FastReauth", dbus.Int16(-1),
                   dbus_interface=dbus.PROPERTIES_IFACE)
        raise Exception("Invalid Set(FastReauth,-1) accepted")
    except dbus.exceptions.DBusException as e:
        if "Error.Failed: wrong property type" not in str(e):
            raise Exception("Unexpected error message for invalid Set(ApScan,-1): " + str(e))

    if_obj.Set(WPAS_DBUS_IFACE, "FastReauth", dbus.Boolean(True),
               dbus_interface=dbus.PROPERTIES_IFACE)

def test_dbus_bss_expire(dev, apdev):
    """D-Bus Get/Set BSSExpireAge and BSSExpireCount"""
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])

    if_obj.Set(WPAS_DBUS_IFACE, "BSSExpireAge", dbus.UInt32(179),
               dbus_interface=dbus.PROPERTIES_IFACE)
    res = if_obj.Get(WPAS_DBUS_IFACE, "BSSExpireAge",
                     dbus_interface=dbus.PROPERTIES_IFACE)
    if res != 179:
        raise Exception("Unexpected BSSExpireAge value %d (expected %d)" % (res, i))

    if_obj.Set(WPAS_DBUS_IFACE, "BSSExpireCount", dbus.UInt32(3),
               dbus_interface=dbus.PROPERTIES_IFACE)
    res = if_obj.Get(WPAS_DBUS_IFACE, "BSSExpireCount",
                     dbus_interface=dbus.PROPERTIES_IFACE)
    if res != 3:
        raise Exception("Unexpected BSSExpireCount value %d (expected %d)" % (res, i))

    try:
        if_obj.Set(WPAS_DBUS_IFACE, "BSSExpireAge", dbus.Int16(-1),
                   dbus_interface=dbus.PROPERTIES_IFACE)
        raise Exception("Invalid Set(BSSExpireAge,-1) accepted")
    except dbus.exceptions.DBusException as e:
        if "Error.Failed: wrong property type" not in str(e):
            raise Exception("Unexpected error message for invalid Set(BSSExpireAge,-1): " + str(e))

    try:
        if_obj.Set(WPAS_DBUS_IFACE, "BSSExpireAge", dbus.UInt32(9),
                   dbus_interface=dbus.PROPERTIES_IFACE)
        raise Exception("Invalid Set(BSSExpireAge,9) accepted")
    except dbus.exceptions.DBusException as e:
        if "Error.Failed: BSSExpireAge must be >= 10" not in str(e):
            raise Exception("Unexpected error message for invalid Set(BSSExpireAge,9): " + str(e))

    try:
        if_obj.Set(WPAS_DBUS_IFACE, "BSSExpireCount", dbus.Int16(-1),
                   dbus_interface=dbus.PROPERTIES_IFACE)
        raise Exception("Invalid Set(BSSExpireCount,-1) accepted")
    except dbus.exceptions.DBusException as e:
        if "Error.Failed: wrong property type" not in str(e):
            raise Exception("Unexpected error message for invalid Set(BSSExpireCount,-1): " + str(e))

    try:
        if_obj.Set(WPAS_DBUS_IFACE, "BSSExpireCount", dbus.UInt32(0),
                   dbus_interface=dbus.PROPERTIES_IFACE)
        raise Exception("Invalid Set(BSSExpireCount,0) accepted")
    except dbus.exceptions.DBusException as e:
        if "Error.Failed: BSSExpireCount must be > 0" not in str(e):
            raise Exception("Unexpected error message for invalid Set(BSSExpireCount,0): " + str(e))

    if_obj.Set(WPAS_DBUS_IFACE, "BSSExpireAge", dbus.UInt32(180),
               dbus_interface=dbus.PROPERTIES_IFACE)
    if_obj.Set(WPAS_DBUS_IFACE, "BSSExpireCount", dbus.UInt32(2),
               dbus_interface=dbus.PROPERTIES_IFACE)

def test_dbus_country(dev, apdev):
    """D-Bus Get/Set Country"""
    try:
        _test_dbus_country(dev, apdev)
    finally:
        dev[0].request("SET country 00")
        subprocess.call(['iw', 'reg', 'set', '00'])

def _test_dbus_country(dev, apdev):
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])

    # work around issues with possible pending regdom event from the end of
    # the previous test case
    time.sleep(0.2)
    dev[0].dump_monitor()

    if_obj.Set(WPAS_DBUS_IFACE, "Country", "FI",
               dbus_interface=dbus.PROPERTIES_IFACE)
    res = if_obj.Get(WPAS_DBUS_IFACE, "Country",
                     dbus_interface=dbus.PROPERTIES_IFACE)
    if res != "FI":
        raise Exception("Unexpected Country value %s (expected FI)" % res)

    ev = dev[0].wait_event(["CTRL-EVENT-REGDOM-CHANGE"])
    if ev is None:
        # For now, work around separate P2P Device interface event delivery
        ev = dev[0].wait_global_event(["CTRL-EVENT-REGDOM-CHANGE"], timeout=1)
        if ev is None:
            raise Exception("regdom change event not seen")
    if "init=USER type=COUNTRY alpha2=FI" not in ev:
        raise Exception("Unexpected event contents: " + ev)

    try:
        if_obj.Set(WPAS_DBUS_IFACE, "Country", dbus.Int16(-1),
                   dbus_interface=dbus.PROPERTIES_IFACE)
        raise Exception("Invalid Set(Country,-1) accepted")
    except dbus.exceptions.DBusException as e:
        if "Error.Failed: wrong property type" not in str(e):
            raise Exception("Unexpected error message for invalid Set(Country,-1): " + str(e))

    try:
        if_obj.Set(WPAS_DBUS_IFACE, "Country", "F",
                   dbus_interface=dbus.PROPERTIES_IFACE)
        raise Exception("Invalid Set(Country,F) accepted")
    except dbus.exceptions.DBusException as e:
        if "Error.Failed: invalid country code" not in str(e):
            raise Exception("Unexpected error message for invalid Set(Country,F): " + str(e))

    if_obj.Set(WPAS_DBUS_IFACE, "Country", "00",
               dbus_interface=dbus.PROPERTIES_IFACE)

    ev = dev[0].wait_event(["CTRL-EVENT-REGDOM-CHANGE"])
    if ev is None:
        # For now, work around separate P2P Device interface event delivery
        ev = dev[0].wait_global_event(["CTRL-EVENT-REGDOM-CHANGE"], timeout=1)
        if ev is None:
            raise Exception("regdom change event not seen")
    # init=CORE was previously used due to invalid db.txt data for 00. For
    # now, allow both it and the new init=USER after fixed db.txt.
    if "init=CORE type=WORLD" not in ev and "init=USER type=WORLD" not in ev:
        raise Exception("Unexpected event contents: " + ev)

def test_dbus_scan_interval(dev, apdev):
    """D-Bus Get/Set ScanInterval"""
    try:
        _test_dbus_scan_interval(dev, apdev)
    finally:
        dev[0].request("SCAN_INTERVAL 5")

def _test_dbus_scan_interval(dev, apdev):
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])

    if_obj.Set(WPAS_DBUS_IFACE, "ScanInterval", dbus.Int32(3),
               dbus_interface=dbus.PROPERTIES_IFACE)
    res = if_obj.Get(WPAS_DBUS_IFACE, "ScanInterval",
                     dbus_interface=dbus.PROPERTIES_IFACE)
    if res != 3:
        raise Exception("Unexpected ScanInterval value %d (expected %d)" % (res, i))

    try:
        if_obj.Set(WPAS_DBUS_IFACE, "ScanInterval", dbus.UInt16(100),
                   dbus_interface=dbus.PROPERTIES_IFACE)
        raise Exception("Invalid Set(ScanInterval,100) accepted")
    except dbus.exceptions.DBusException as e:
        if "Error.Failed: wrong property type" not in str(e):
            raise Exception("Unexpected error message for invalid Set(ScanInterval,100): " + str(e))

    try:
        if_obj.Set(WPAS_DBUS_IFACE, "ScanInterval", dbus.Int32(-1),
                   dbus_interface=dbus.PROPERTIES_IFACE)
        raise Exception("Invalid Set(ScanInterval,-1) accepted")
    except dbus.exceptions.DBusException as e:
        if "Error.Failed: scan_interval must be >= 0" not in str(e):
            raise Exception("Unexpected error message for invalid Set(ScanInterval,-1): " + str(e))

    if_obj.Set(WPAS_DBUS_IFACE, "ScanInterval", dbus.Int32(5),
               dbus_interface=dbus.PROPERTIES_IFACE)

def test_dbus_probe_req_reporting(dev, apdev):
    """D-Bus Probe Request reporting"""
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])

    dev[1].p2p_find(social=True)

    class TestDbusProbe(TestDbus):
        def __init__(self, bus):
            TestDbus.__init__(self, bus)
            self.reported = False

        def __enter__(self):
            gobject.timeout_add(1, self.run_test)
            gobject.timeout_add(15000, self.timeout)
            self.add_signal(self.groupStarted, WPAS_DBUS_IFACE_P2PDEVICE,
                            "GroupStarted")
            self.add_signal(self.probeRequest, WPAS_DBUS_IFACE, "ProbeRequest",
                            byte_arrays=True)
            self.loop.run()
            return self

        def groupStarted(self, properties):
            logger.debug("groupStarted: " + str(properties))
            g_if_obj = bus.get_object(WPAS_DBUS_SERVICE,
                                      properties['interface_object'])
            self.iface = dbus.Interface(g_if_obj, WPAS_DBUS_IFACE)
            self.iface.SubscribeProbeReq()
            self.group_p2p = dbus.Interface(g_if_obj, WPAS_DBUS_IFACE_P2PDEVICE)

        def probeRequest(self, args):
            logger.debug("probeRequest: args=%s" % str(args))
            self.reported = True
            self.loop.quit()

        def run_test(self, *args):
            logger.debug("run_test")
            p2p = dbus.Interface(if_obj, WPAS_DBUS_IFACE_P2PDEVICE)
            params = dbus.Dictionary({'frequency': 2412})
            p2p.GroupAdd(params)
            return False

        def success(self):
            return self.reported

    with TestDbusProbe(bus) as t:
        if not t.success():
            raise Exception("Expected signals not seen")
        t.iface.UnsubscribeProbeReq()
        try:
            t.iface.UnsubscribeProbeReq()
            raise Exception("Invalid UnsubscribeProbeReq() accepted")
        except dbus.exceptions.DBusException as e:
            if "NoSubscription" not in str(e):
                raise Exception("Unexpected error message for invalid UnsubscribeProbeReq(): " + str(e))
        t.group_p2p.Disconnect()

    with TestDbusProbe(bus) as t:
        if not t.success():
            raise Exception("Expected signals not seen")
        # On purpose, leave ProbeReq subscription in place to test automatic
        # cleanup.

    dev[1].p2p_stop_find()

def test_dbus_probe_req_reporting_oom(dev, apdev):
    """D-Bus Probe Request reporting (OOM)"""
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    iface = dbus.Interface(if_obj, WPAS_DBUS_IFACE)

    # Need to make sure this process has not already subscribed to avoid false
    # failures due to the operation succeeding due to os_strdup() not even
    # getting called.
    try:
        iface.UnsubscribeProbeReq()
        was_subscribed = True
    except dbus.exceptions.DBusException as e:
        was_subscribed = False
        pass

    with alloc_fail_dbus(dev[0], 1, "wpas_dbus_handler_subscribe_preq",
                         "SubscribeProbeReq"):
        iface.SubscribeProbeReq()

    if was_subscribed:
        # On purpose, leave ProbeReq subscription in place to test automatic
        # cleanup.
        iface.SubscribeProbeReq()

def test_dbus_p2p_invalid(dev, apdev):
    """D-Bus invalid P2P operations"""
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    p2p = dbus.Interface(if_obj, WPAS_DBUS_IFACE_P2PDEVICE)

    try:
        p2p.RejectPeer(path + "/Peers/00112233445566")
        raise Exception("Invalid RejectPeer accepted")
    except dbus.exceptions.DBusException as e:
        if "UnknownError: Failed to call wpas_p2p_reject" not in str(e):
            raise Exception("Unexpected error message for invalid RejectPeer(): " + str(e))

    try:
        p2p.RejectPeer("/foo")
        raise Exception("Invalid RejectPeer accepted")
    except dbus.exceptions.DBusException as e:
        if "InvalidArgs" not in str(e):
            raise Exception("Unexpected error message for invalid RejectPeer(): " + str(e))

    tests = [{},
             {'peer': 'foo'},
             {'foo': "bar"},
             {'iface': "abc"},
             {'iface': 123}]
    for t in tests:
        try:
            p2p.RemoveClient(t)
            raise Exception("Invalid RemoveClient accepted")
        except dbus.exceptions.DBusException as e:
            if "InvalidArgs" not in str(e):
                raise Exception("Unexpected error message for invalid RemoveClient(): " + str(e))

    tests = [{'DiscoveryType': 'foo'},
             {'RequestedDeviceTypes': 'foo'},
             {'RequestedDeviceTypes': ['foo']},
             {'RequestedDeviceTypes': ['1', '2', '3', '4', '5', '6', '7', '8',
                                       '9', '10', '11', '12', '13', '14', '15',
                                       '16', '17']},
             {'RequestedDeviceTypes': dbus.Array([], signature="s")},
             {'RequestedDeviceTypes': dbus.Array([['foo']], signature="as")},
             {'RequestedDeviceTypes': dbus.Array([], signature="i")},
             {'RequestedDeviceTypes': [dbus.ByteArray(b'12345678'),
                                       dbus.ByteArray(b'1234567')]},
             {'Foo': dbus.Int16(1)},
             {'Foo': dbus.UInt16(1)},
             {'Foo': dbus.Int64(1)},
             {'Foo': dbus.UInt64(1)},
             {'Foo': dbus.Double(1.23)},
             {'Foo': dbus.Signature('s')},
             {'Foo': 'bar'}]
    for t in tests:
        try:
            p2p.Find(dbus.Dictionary(t))
            raise Exception("Invalid Find accepted")
        except dbus.exceptions.DBusException as e:
            if "InvalidArgs" not in str(e):
                raise Exception("Unexpected error message for invalid Find(): " + str(e))

    for p in ["/foo",
              "/fi/w1/wpa_supplicant1/Interfaces/1234",
              "/fi/w1/wpa_supplicant1/Interfaces/1234/Networks/1234"]:
        try:
            p2p.RemovePersistentGroup(dbus.ObjectPath(p))
            raise Exception("Invalid RemovePersistentGroup accepted")
        except dbus.exceptions.DBusException as e:
            if "InvalidArgs" not in str(e):
                raise Exception("Unexpected error message for invalid RemovePersistentGroup: " + str(e))

    try:
        dev[0].request("P2P_SET disabled 1")
        p2p.Listen(5)
        raise Exception("Invalid Listen accepted")
    except dbus.exceptions.DBusException as e:
        if "UnknownError: Could not start P2P listen" not in str(e):
            raise Exception("Unexpected error message for invalid Listen: " + str(e))
    finally:
        dev[0].request("P2P_SET disabled 0")

    test_obj = bus.get_object(WPAS_DBUS_SERVICE, path, introspect=False)
    test_p2p = dbus.Interface(test_obj, WPAS_DBUS_IFACE_P2PDEVICE)
    try:
        test_p2p.Listen("foo")
        raise Exception("Invalid Listen accepted")
    except dbus.exceptions.DBusException as e:
        if "InvalidArgs" not in str(e):
            raise Exception("Unexpected error message for invalid Listen: " + str(e))

    try:
        dev[0].request("P2P_SET disabled 1")
        p2p.ExtendedListen(dbus.Dictionary({}))
        raise Exception("Invalid ExtendedListen accepted")
    except dbus.exceptions.DBusException as e:
        if "UnknownError: failed to initiate a p2p_ext_listen" not in str(e):
            raise Exception("Unexpected error message for invalid ExtendedListen: " + str(e))
    finally:
        dev[0].request("P2P_SET disabled 0")

    try:
        dev[0].request("P2P_SET disabled 1")
        args = {'duration1': 30000, 'interval1': 102400,
                'duration2': 20000, 'interval2': 102400}
        p2p.PresenceRequest(args)
        raise Exception("Invalid PresenceRequest accepted")
    except dbus.exceptions.DBusException as e:
        if "UnknownError: Failed to invoke presence request" not in str(e):
            raise Exception("Unexpected error message for invalid PresenceRequest: " + str(e))
    finally:
        dev[0].request("P2P_SET disabled 0")

    try:
        params = dbus.Dictionary({'frequency': dbus.Int32(-1)})
        p2p.GroupAdd(params)
        raise Exception("Invalid GroupAdd accepted")
    except dbus.exceptions.DBusException as e:
        if "InvalidArgs" not in str(e):
            raise Exception("Unexpected error message for invalid GroupAdd: " + str(e))

    try:
        params = dbus.Dictionary({'persistent_group_object':
                                  dbus.ObjectPath(path),
                                  'frequency': 2412})
        p2p.GroupAdd(params)
        raise Exception("Invalid GroupAdd accepted")
    except dbus.exceptions.DBusException as e:
        if "InvalidArgs" not in str(e):
            raise Exception("Unexpected error message for invalid GroupAdd: " + str(e))

    try:
        p2p.Disconnect()
        raise Exception("Invalid Disconnect accepted")
    except dbus.exceptions.DBusException as e:
        if "UnknownError: failed to disconnect" not in str(e):
            raise Exception("Unexpected error message for invalid Disconnect: " + str(e))

    try:
        dev[0].request("P2P_SET disabled 1")
        p2p.Flush()
        raise Exception("Invalid Flush accepted")
    except dbus.exceptions.DBusException as e:
        if "Error.Failed: P2P is not available for this interface" not in str(e):
            raise Exception("Unexpected error message for invalid Flush: " + str(e))
    finally:
        dev[0].request("P2P_SET disabled 0")

    try:
        dev[0].request("P2P_SET disabled 1")
        args = {'peer': path,
                'join': True,
                'wps_method': 'pbc',
                'frequency': 2412}
        pin = p2p.Connect(args)
        raise Exception("Invalid Connect accepted")
    except dbus.exceptions.DBusException as e:
        if "Error.Failed: P2P is not available for this interface" not in str(e):
            raise Exception("Unexpected error message for invalid Connect: " + str(e))
    finally:
        dev[0].request("P2P_SET disabled 0")

    tests = [{'frequency': dbus.Int32(-1)},
             {'wps_method': 'pbc'},
             {'wps_method': 'foo'}]
    for args in tests:
        try:
            pin = p2p.Connect(args)
            raise Exception("Invalid Connect accepted")
        except dbus.exceptions.DBusException as e:
            if "InvalidArgs" not in str(e):
                raise Exception("Unexpected error message for invalid Connect: " + str(e))

    try:
        dev[0].request("P2P_SET disabled 1")
        args = {'peer': path}
        pin = p2p.Invite(args)
        raise Exception("Invalid Invite accepted")
    except dbus.exceptions.DBusException as e:
        if "Error.Failed: P2P is not available for this interface" not in str(e):
            raise Exception("Unexpected error message for invalid Invite: " + str(e))
    finally:
        dev[0].request("P2P_SET disabled 0")

    try:
        args = {'foo': 'bar'}
        pin = p2p.Invite(args)
        raise Exception("Invalid Invite accepted")
    except dbus.exceptions.DBusException as e:
        if "InvalidArgs" not in str(e):
            raise Exception("Unexpected error message for invalid Connect: " + str(e))

    tests = [(path, 'display', "InvalidArgs"),
             (dbus.ObjectPath(path + "/Peers/00112233445566"),
              'display',
              "UnknownError: Failed to send provision discovery request"),
             (dbus.ObjectPath(path + "/Peers/00112233445566"),
              'keypad',
              "UnknownError: Failed to send provision discovery request"),
             (dbus.ObjectPath(path + "/Peers/00112233445566"),
              'pbc',
              "UnknownError: Failed to send provision discovery request"),
             (dbus.ObjectPath(path + "/Peers/00112233445566"),
              'pushbutton',
              "UnknownError: Failed to send provision discovery request"),
             (dbus.ObjectPath(path + "/Peers/00112233445566"),
              'foo', "InvalidArgs")]
    for (p, method, err) in tests:
        try:
            p2p.ProvisionDiscoveryRequest(p, method)
            raise Exception("Invalid ProvisionDiscoveryRequest accepted")
        except dbus.exceptions.DBusException as e:
            if err not in str(e):
                raise Exception("Unexpected error message for invalid ProvisionDiscoveryRequest: " + str(e))

    try:
        dev[0].request("P2P_SET disabled 1")
        if_obj.Get(WPAS_DBUS_IFACE_P2PDEVICE, "Peers",
                   dbus_interface=dbus.PROPERTIES_IFACE)
        raise Exception("Invalid Get(Peers) accepted")
    except dbus.exceptions.DBusException as e:
        if "Error.Failed: P2P is not available for this interface" not in str(e):
            raise Exception("Unexpected error message for invalid Get(Peers): " + str(e))
    finally:
        dev[0].request("P2P_SET disabled 0")

def test_dbus_p2p_oom(dev, apdev):
    """D-Bus P2P operations and OOM"""
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    p2p = dbus.Interface(if_obj, WPAS_DBUS_IFACE_P2PDEVICE)

    with alloc_fail_dbus(dev[0], 1, "_wpa_dbus_dict_entry_get_string_array",
                         "Find", "InvalidArgs"):
        p2p.Find(dbus.Dictionary({'Foo': ['bar']}))

    with alloc_fail_dbus(dev[0], 2, "_wpa_dbus_dict_entry_get_string_array",
                         "Find", "InvalidArgs"):
        p2p.Find(dbus.Dictionary({'Foo': ['bar']}))

    with alloc_fail_dbus(dev[0], 10, "_wpa_dbus_dict_entry_get_string_array",
                         "Find", "InvalidArgs"):
        p2p.Find(dbus.Dictionary({'Foo': ['1', '2', '3', '4', '5', '6', '7',
                                          '8', '9']}))

    with alloc_fail_dbus(dev[0], 1, ":=_wpa_dbus_dict_entry_get_binarray",
                         "Find", "InvalidArgs"):
        p2p.Find(dbus.Dictionary({'Foo': [dbus.ByteArray(b'123')]}))

    with alloc_fail_dbus(dev[0], 1, "_wpa_dbus_dict_entry_get_byte_array;_wpa_dbus_dict_entry_get_binarray",
                         "Find", "InvalidArgs"):
        p2p.Find(dbus.Dictionary({'Foo': [dbus.ByteArray(b'123')]}))

    with alloc_fail_dbus(dev[0], 2, "=_wpa_dbus_dict_entry_get_binarray",
                         "Find", "InvalidArgs"):
        p2p.Find(dbus.Dictionary({'Foo': [dbus.ByteArray(b'123'),
                                          dbus.ByteArray(b'123'),
                                          dbus.ByteArray(b'123'),
                                          dbus.ByteArray(b'123'),
                                          dbus.ByteArray(b'123'),
                                          dbus.ByteArray(b'123'),
                                          dbus.ByteArray(b'123'),
                                          dbus.ByteArray(b'123'),
                                          dbus.ByteArray(b'123'),
                                          dbus.ByteArray(b'123'),
                                          dbus.ByteArray(b'123')]}))

    with alloc_fail_dbus(dev[0], 1, "wpabuf_alloc_ext_data;_wpa_dbus_dict_entry_get_binarray",
                         "Find", "InvalidArgs"):
        p2p.Find(dbus.Dictionary({'Foo': [dbus.ByteArray(b'123')]}))

    with alloc_fail_dbus(dev[0], 1, "_wpa_dbus_dict_fill_value_from_variant;wpas_dbus_handler_p2p_find",
                         "Find", "InvalidArgs"):
        p2p.Find(dbus.Dictionary({'Foo': path}))

    with alloc_fail_dbus(dev[0], 1, "_wpa_dbus_dict_entry_get_byte_array",
                         "AddService", "InvalidArgs"):
        args = {'service_type': 'bonjour',
                'response': dbus.ByteArray(500*b'b')}
        p2p.AddService(args)

    with alloc_fail_dbus(dev[0], 2, "_wpa_dbus_dict_entry_get_byte_array",
                         "AddService", "InvalidArgs"):
        p2p.AddService(args)

def test_dbus_p2p_discovery(dev, apdev):
    """D-Bus P2P discovery"""
    try:
        run_dbus_p2p_discovery(dev, apdev)
    finally:
        dev[1].request("VENDOR_ELEM_REMOVE 1 *")

def run_dbus_p2p_discovery(dev, apdev):
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    p2p = dbus.Interface(if_obj, WPAS_DBUS_IFACE_P2PDEVICE)

    addr0 = dev[0].p2p_dev_addr()

    dev[1].request("SET sec_device_type 1-0050F204-2")
    dev[1].request("VENDOR_ELEM_ADD 1 dd0c0050f2041049000411223344")
    dev[1].request("VENDOR_ELEM_ADD 1 dd06001122335566")
    dev[1].p2p_listen()
    addr1 = dev[1].p2p_dev_addr()
    a1 = binascii.unhexlify(addr1.replace(':', ''))

    wfd_devinfo = "00001c440028"
    dev[2].request("SET wifi_display 1")
    dev[2].request("WFD_SUBELEM_SET 0 0006" + wfd_devinfo)
    wfd = binascii.unhexlify('000006' + wfd_devinfo)
    dev[2].p2p_listen()
    addr2 = dev[2].p2p_dev_addr()
    a2 = binascii.unhexlify(addr2.replace(':', ''))

    res = if_obj.GetAll(WPAS_DBUS_IFACE_P2PDEVICE,
                        dbus_interface=dbus.PROPERTIES_IFACE)
    if 'Peers' not in res:
        raise Exception("GetAll result missing Peers")
    if len(res['Peers']) != 0:
        raise Exception("Unexpected peer(s) in the list")

    args = {'DiscoveryType': 'social',
            'RequestedDeviceTypes': [dbus.ByteArray(b'12345678')],
            'Timeout': dbus.Int32(1)}
    p2p.Find(dbus.Dictionary(args))
    p2p.StopFind()

    class TestDbusP2p(TestDbus):
        def __init__(self, bus):
            TestDbus.__init__(self, bus)
            self.found = False
            self.found2 = False
            self.found_prop = False
            self.lost = False
            self.find_stopped = False

        def __enter__(self):
            gobject.timeout_add(1, self.run_test)
            gobject.timeout_add(15000, self.timeout)
            self.add_signal(self.deviceFound, WPAS_DBUS_IFACE_P2PDEVICE,
                            "DeviceFound")
            self.add_signal(self.deviceFoundProperties,
                            WPAS_DBUS_IFACE_P2PDEVICE, "DeviceFoundProperties")
            self.add_signal(self.deviceLost, WPAS_DBUS_IFACE_P2PDEVICE,
                            "DeviceLost")
            self.add_signal(self.provisionDiscoveryResponseEnterPin,
                            WPAS_DBUS_IFACE_P2PDEVICE,
                            "ProvisionDiscoveryResponseEnterPin")
            self.add_signal(self.findStopped, WPAS_DBUS_IFACE_P2PDEVICE,
                            "FindStopped")
            self.loop.run()
            return self

        def deviceFound(self, path):
            logger.debug("deviceFound: path=%s" % path)
            res = if_obj.Get(WPAS_DBUS_IFACE_P2PDEVICE, "Peers",
                             dbus_interface=dbus.PROPERTIES_IFACE)
            if len(res) < 1:
                raise Exception("Unexpected number of peers")
            if path not in res:
                raise Exception("Mismatch in peer object path")
            peer_obj = bus.get_object(WPAS_DBUS_SERVICE, path)
            res = peer_obj.GetAll(WPAS_DBUS_P2P_PEER,
                                  dbus_interface=dbus.PROPERTIES_IFACE,
                                  byte_arrays=True)
            logger.debug("peer properties: " + str(res))

            if res['DeviceAddress'] == a1:
                if 'SecondaryDeviceTypes' not in res:
                    raise Exception("Missing SecondaryDeviceTypes")
                sec = res['SecondaryDeviceTypes']
                if len(sec) < 1:
                    raise Exception("Secondary device type missing")
                if b"\x00\x01\x00\x50\xF2\x04\x00\x02" not in sec:
                    raise Exception("Secondary device type mismatch")

                if 'VendorExtension' not in res:
                    raise Exception("Missing VendorExtension")
                vendor = res['VendorExtension']
                if len(vendor) < 1:
                    raise Exception("Vendor extension missing")
                if b"\x11\x22\x33\x44" not in vendor:
                    raise Exception("Secondary device type mismatch")

                if 'VSIE' not in res:
                    raise Exception("Missing VSIE")
                vendor = res['VSIE']
                if len(vendor) < 1:
                    raise Exception("VSIE missing")
                if vendor != b"\xdd\x06\x00\x11\x22\x33\x55\x66":
                    raise Exception("VSIE mismatch")

                self.found = True
            elif res['DeviceAddress'] == a2:
                if 'IEs' not in res:
                    raise Exception("IEs missing")
                if res['IEs'] != wfd:
                    raise Exception("IEs mismatch")
                self.found2 = True
            else:
                raise Exception("Unexpected peer device address")

            if self.found and self.found2:
                p2p.StopFind()
                p2p.RejectPeer(path)
                p2p.ProvisionDiscoveryRequest(path, 'display')

        def deviceLost(self, path):
            logger.debug("deviceLost: path=%s" % path)
            if not self.found or not self.found2:
                # This may happen if a previous test case ended up scheduling
                # deviceLost event and that event did not get delivered before
                # starting the next test execution.
                logger.debug("Ignore deviceLost before the deviceFound events")
                return
            self.lost = True
            try:
                p2p.RejectPeer(path)
                raise Exception("Invalid RejectPeer accepted")
            except dbus.exceptions.DBusException as e:
                if "UnknownError: Failed to call wpas_p2p_reject" not in str(e):
                    raise Exception("Unexpected error message for invalid RejectPeer(): " + str(e))
            self.loop.quit()

        def deviceFoundProperties(self, path, properties):
            logger.debug("deviceFoundProperties: path=%s" % path)
            logger.debug("peer properties: " + str(properties))
            if properties['DeviceAddress'] == a1:
                self.found_prop = True

        def provisionDiscoveryResponseEnterPin(self, peer_object):
            logger.debug("provisionDiscoveryResponseEnterPin - peer=%s" % peer_object)
            p2p.Flush()

        def findStopped(self):
            logger.debug("findStopped")
            self.find_stopped = True

        def run_test(self, *args):
            logger.debug("run_test")
            p2p.Find(dbus.Dictionary({'DiscoveryType': 'social',
                                      'Timeout': dbus.Int32(10)}))
            return False

        def success(self):
            return self.found and self.lost and self.found2 and self.find_stopped

    with TestDbusP2p(bus) as t:
        if not t.success():
            raise Exception("Expected signals not seen")

    dev[1].request("VENDOR_ELEM_REMOVE 1 *")
    dev[1].p2p_stop_find()

    p2p.Listen(1)
    dev[2].p2p_stop_find()
    dev[2].request("P2P_FLUSH")
    if not dev[2].discover_peer(addr0):
        raise Exception("Peer not found")
    p2p.StopFind()
    dev[2].p2p_stop_find()

    try:
        p2p.ExtendedListen(dbus.Dictionary({'foo': 100}))
        raise Exception("Invalid ExtendedListen accepted")
    except dbus.exceptions.DBusException as e:
        if "InvalidArgs" not in str(e):
            raise Exception("Unexpected error message for invalid ExtendedListen(): " + str(e))

    p2p.ExtendedListen(dbus.Dictionary({'period': 100, 'interval': 1000}))
    p2p.ExtendedListen(dbus.Dictionary({}))
    dev[0].global_request("P2P_EXT_LISTEN")

def test_dbus_p2p_discovery_freq(dev, apdev):
    """D-Bus P2P discovery on a specific non-social channel"""
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    p2p = dbus.Interface(if_obj, WPAS_DBUS_IFACE_P2PDEVICE)

    addr1 = dev[1].p2p_dev_addr()
    autogo(dev[1], freq=2422)

    class TestDbusP2p(TestDbus):
        def __init__(self, bus):
            TestDbus.__init__(self, bus)
            self.found = False

        def __enter__(self):
            gobject.timeout_add(1, self.run_test)
            gobject.timeout_add(5000, self.timeout)
            self.add_signal(self.deviceFound, WPAS_DBUS_IFACE_P2PDEVICE,
                            "DeviceFound")
            self.loop.run()
            return self

        def deviceFound(self, path):
            logger.debug("deviceFound: path=%s" % path)
            self.found = True
            self.loop.quit()

        def run_test(self, *args):
            logger.debug("run_test")
            p2p.Find(dbus.Dictionary({'freq': 2422}))
            return False

        def success(self):
            return self.found

    with TestDbusP2p(bus) as t:
        if not t.success():
            raise Exception("Expected signals not seen")

    dev[1].remove_group()
    p2p.StopFind()

def test_dbus_p2p_service_discovery(dev, apdev):
    """D-Bus P2P service discovery"""
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    p2p = dbus.Interface(if_obj, WPAS_DBUS_IFACE_P2PDEVICE)

    addr0 = dev[0].p2p_dev_addr()
    addr1 = dev[1].p2p_dev_addr()

    bonjour_query = dbus.ByteArray(binascii.unhexlify('0b5f6166706f766572746370c00c000c01'))
    bonjour_response = dbus.ByteArray(binascii.unhexlify('074578616d706c65c027'))

    args = {'service_type': 'bonjour',
            'query': bonjour_query,
            'response': bonjour_response}
    p2p.AddService(args)
    p2p.FlushService()
    p2p.AddService(args)

    try:
        p2p.DeleteService(args)
        raise Exception("Invalid DeleteService() accepted")
    except dbus.exceptions.DBusException as e:
        if "InvalidArgs" not in str(e):
            raise Exception("Unexpected error message for invalid DeleteService(): " + str(e))

    args = {'service_type': 'bonjour',
            'query': bonjour_query}
    p2p.DeleteService(args)
    try:
        p2p.DeleteService(args)
        raise Exception("Invalid DeleteService() accepted")
    except dbus.exceptions.DBusException as e:
        if "InvalidArgs" not in str(e):
            raise Exception("Unexpected error message for invalid DeleteService(): " + str(e))

    args = {'service_type': 'upnp',
            'version': 0x10,
            'service': 'uuid:6859dede-8574-59ab-9332-123456789012::upnp:rootdevice'}
    p2p.AddService(args)
    p2p.DeleteService(args)
    try:
        p2p.DeleteService(args)
        raise Exception("Invalid DeleteService() accepted")
    except dbus.exceptions.DBusException as e:
        if "InvalidArgs" not in str(e):
            raise Exception("Unexpected error message for invalid DeleteService(): " + str(e))

    tests = [{'service_type': 'foo'},
             {'service_type': 'foo', 'query': bonjour_query},
             {'service_type': 'upnp'},
             {'service_type': 'upnp', 'version': 0x10},
             {'service_type': 'upnp',
              'service': 'uuid:6859dede-8574-59ab-9332-123456789012::upnp:rootdevice'},
             {'version': 0x10,
              'service': 'uuid:6859dede-8574-59ab-9332-123456789012::upnp:rootdevice'},
             {'service_type': 'upnp', 'foo': 'bar'},
             {'service_type': 'bonjour'},
             {'service_type': 'bonjour', 'query': 'foo'},
             {'service_type': 'bonjour', 'foo': 'bar'}]
    for args in tests:
        try:
            p2p.DeleteService(args)
            raise Exception("Invalid DeleteService() accepted")
        except dbus.exceptions.DBusException as e:
            if "InvalidArgs" not in str(e):
                raise Exception("Unexpected error message for invalid DeleteService(): " + str(e))

    tests = [{'service_type': 'foo'},
             {'service_type': 'upnp'},
             {'service_type': 'upnp', 'version': 0x10},
             {'service_type': 'upnp',
              'service': 'uuid:6859dede-8574-59ab-9332-123456789012::upnp:rootdevice'},
             {'version': 0x10,
              'service': 'uuid:6859dede-8574-59ab-9332-123456789012::upnp:rootdevice'},
             {'service_type': 'upnp', 'foo': 'bar'},
             {'service_type': 'bonjour'},
             {'service_type': 'bonjour', 'query': 'foo'},
             {'service_type': 'bonjour', 'response': 'foo'},
             {'service_type': 'bonjour', 'query': bonjour_query},
             {'service_type': 'bonjour', 'response': bonjour_response},
             {'service_type': 'bonjour', 'query': dbus.ByteArray(500*b'a')},
             {'service_type': 'bonjour', 'foo': 'bar'}]
    for args in tests:
        try:
            p2p.AddService(args)
            raise Exception("Invalid AddService() accepted")
        except dbus.exceptions.DBusException as e:
            if "InvalidArgs" not in str(e):
                raise Exception("Unexpected error message for invalid AddService(): " + str(e))

    args = {'tlv': dbus.ByteArray(b"\x02\x00\x00\x01")}
    ref = p2p.ServiceDiscoveryRequest(args)
    p2p.ServiceDiscoveryCancelRequest(ref)
    try:
        p2p.ServiceDiscoveryCancelRequest(ref)
        raise Exception("Invalid ServiceDiscoveryCancelRequest() accepted")
    except dbus.exceptions.DBusException as e:
        if "InvalidArgs" not in str(e):
            raise Exception("Unexpected error message for invalid AddService(): " + str(e))
    try:
        p2p.ServiceDiscoveryCancelRequest(dbus.UInt64(0))
        raise Exception("Invalid ServiceDiscoveryCancelRequest() accepted")
    except dbus.exceptions.DBusException as e:
        if "InvalidArgs" not in str(e):
            raise Exception("Unexpected error message for invalid AddService(): " + str(e))

    args = {'service_type': 'upnp',
            'version': 0x10,
            'service': 'ssdp:foo'}
    ref = p2p.ServiceDiscoveryRequest(args)
    p2p.ServiceDiscoveryCancelRequest(ref)

    tests = [{'service_type': 'foo'},
             {'foo': 'bar'},
             {'tlv': 'foo'},
             {},
             {'version': 0},
             {'service_type': 'upnp',
              'service': 'ssdp:foo'},
             {'service_type': 'upnp',
              'version': 0x10},
             {'service_type': 'upnp',
              'version': 0x10,
              'service': 'ssdp:foo',
              'peer_object': dbus.ObjectPath(path + "/Peers")},
             {'service_type': 'upnp',
              'version': 0x10,
              'service': 'ssdp:foo',
              'peer_object': path + "/Peers"},
             {'service_type': 'upnp',
              'version': 0x10,
              'service': 'ssdp:foo',
              'peer_object': dbus.ObjectPath(path + "/Peers/00112233445566")}]
    for args in tests:
        try:
            p2p.ServiceDiscoveryRequest(args)
            raise Exception("Invalid ServiceDiscoveryRequest accepted")
        except dbus.exceptions.DBusException as e:
            if "InvalidArgs" not in str(e):
                raise Exception("Unexpected error message for invalid ServiceDiscoveryRequest(): " + str(e))

    args = {'foo': 'bar'}
    try:
        p2p.ServiceDiscoveryResponse(dbus.Dictionary(args, signature='sv'))
        raise Exception("Invalid ServiceDiscoveryResponse accepted")
    except dbus.exceptions.DBusException as e:
        if "InvalidArgs" not in str(e):
            raise Exception("Unexpected error message for invalid ServiceDiscoveryResponse(): " + str(e))

def test_dbus_p2p_service_discovery_query(dev, apdev):
    """D-Bus P2P service discovery query"""
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    p2p = dbus.Interface(if_obj, WPAS_DBUS_IFACE_P2PDEVICE)

    addr0 = dev[0].p2p_dev_addr()
    dev[1].request("P2P_SERVICE_ADD bonjour 0b5f6166706f766572746370c00c000c01 074578616d706c65c027")
    dev[1].p2p_listen()
    addr1 = dev[1].p2p_dev_addr()

    class TestDbusP2p(TestDbus):
        def __init__(self, bus):
            TestDbus.__init__(self, bus)
            self.done = False

        def __enter__(self):
            gobject.timeout_add(1, self.run_test)
            gobject.timeout_add(15000, self.timeout)
            self.add_signal(self.deviceFound, WPAS_DBUS_IFACE_P2PDEVICE,
                            "DeviceFound")
            self.add_signal(self.serviceDiscoveryResponse,
                            WPAS_DBUS_IFACE_P2PDEVICE,
                            "ServiceDiscoveryResponse", byte_arrays=True)
            self.loop.run()
            return self

        def deviceFound(self, path):
            logger.debug("deviceFound: path=%s" % path)
            args = {'peer_object': path,
                    'tlv': dbus.ByteArray(b"\x02\x00\x00\x01")}
            p2p.ServiceDiscoveryRequest(args)

        def serviceDiscoveryResponse(self, sd_request):
            logger.debug("serviceDiscoveryResponse: sd_request=%s" % str(sd_request))
            self.done = True
            self.loop.quit()

        def run_test(self, *args):
            logger.debug("run_test")
            p2p.Find(dbus.Dictionary({'DiscoveryType': 'social',
                                      'Timeout': dbus.Int32(10)}))
            return False

        def success(self):
            return self.done

    with TestDbusP2p(bus) as t:
        if not t.success():
            raise Exception("Expected signals not seen")

    dev[1].p2p_stop_find()

def test_dbus_p2p_service_discovery_external(dev, apdev):
    """D-Bus P2P service discovery with external response"""
    try:
        _test_dbus_p2p_service_discovery_external(dev, apdev)
    finally:
        dev[0].request("P2P_SERV_DISC_EXTERNAL 0")

def _test_dbus_p2p_service_discovery_external(dev, apdev):
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    p2p = dbus.Interface(if_obj, WPAS_DBUS_IFACE_P2PDEVICE)

    addr0 = dev[0].p2p_dev_addr()
    addr1 = dev[1].p2p_dev_addr()
    resp = "0300000101"

    dev[1].request("P2P_FLUSH")
    dev[1].request("P2P_SERV_DISC_REQ " + addr0 + " 02000001")
    dev[1].p2p_find(social=True)

    class TestDbusP2p(TestDbus):
        def __init__(self, bus):
            TestDbus.__init__(self, bus)
            self.sd = False

        def __enter__(self):
            gobject.timeout_add(1, self.run_test)
            gobject.timeout_add(15000, self.timeout)
            self.add_signal(self.deviceFound, WPAS_DBUS_IFACE_P2PDEVICE,
                            "DeviceFound")
            self.add_signal(self.serviceDiscoveryRequest,
                            WPAS_DBUS_IFACE_P2PDEVICE,
                            "ServiceDiscoveryRequest")
            self.loop.run()
            return self

        def deviceFound(self, path):
            logger.debug("deviceFound: path=%s" % path)

        def serviceDiscoveryRequest(self, sd_request):
            logger.debug("serviceDiscoveryRequest: sd_request=%s" % str(sd_request))
            self.sd = True
            args = {'peer_object': sd_request['peer_object'],
                    'frequency': sd_request['frequency'],
                    'dialog_token': sd_request['dialog_token'],
                    'tlvs': dbus.ByteArray(binascii.unhexlify(resp))}
            p2p.ServiceDiscoveryResponse(dbus.Dictionary(args, signature='sv'))
            self.loop.quit()

        def run_test(self, *args):
            logger.debug("run_test")
            p2p.ServiceDiscoveryExternal(1)
            p2p.ServiceUpdate()
            p2p.Listen(15)
            return False

        def success(self):
            return self.sd

    with TestDbusP2p(bus) as t:
        if not t.success():
            raise Exception("Expected signals not seen")

    ev = dev[1].wait_global_event(["P2P-SERV-DISC-RESP"], timeout=5)
    if ev is None:
        raise Exception("Service discovery timed out")
    if addr0 not in ev:
        raise Exception("Unexpected address in SD Response: " + ev)
    if ev.split(' ')[4] != resp:
        raise Exception("Unexpected response data SD Response: " + ev)
    dev[1].p2p_stop_find()

    p2p.StopFind()
    p2p.ServiceDiscoveryExternal(0)

def test_dbus_p2p_autogo(dev, apdev):
    """D-Bus P2P autonomous GO"""
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    p2p = dbus.Interface(if_obj, WPAS_DBUS_IFACE_P2PDEVICE)

    addr0 = dev[0].p2p_dev_addr()

    class TestDbusP2p(TestDbus):
        def __init__(self, bus):
            TestDbus.__init__(self, bus)
            self.first = True
            self.waiting_end = False
            self.exceptions = False
            self.deauthorized = False
            self.done = False

        def __enter__(self):
            gobject.timeout_add(1, self.run_test)
            gobject.timeout_add(15000, self.timeout)
            self.add_signal(self.deviceFound, WPAS_DBUS_IFACE_P2PDEVICE,
                            "DeviceFound")
            self.add_signal(self.groupStarted, WPAS_DBUS_IFACE_P2PDEVICE,
                            "GroupStarted")
            self.add_signal(self.groupFinished, WPAS_DBUS_IFACE_P2PDEVICE,
                            "GroupFinished")
            self.add_signal(self.persistentGroupAdded,
                            WPAS_DBUS_IFACE_P2PDEVICE,
                            "PersistentGroupAdded")
            self.add_signal(self.persistentGroupRemoved,
                            WPAS_DBUS_IFACE_P2PDEVICE,
                            "PersistentGroupRemoved")
            self.add_signal(self.provisionDiscoveryRequestDisplayPin,
                            WPAS_DBUS_IFACE_P2PDEVICE,
                            "ProvisionDiscoveryRequestDisplayPin")
            self.add_signal(self.staAuthorized, WPAS_DBUS_IFACE,
                            "StaAuthorized")
            self.add_signal(self.staDeauthorized, WPAS_DBUS_IFACE,
                            "StaDeauthorized")
            self.loop.run()
            return self

        def groupStarted(self, properties):
            logger.debug("groupStarted: " + str(properties))
            self.group = properties['group_object']
            self.g_if_obj = bus.get_object(WPAS_DBUS_SERVICE,
                                           properties['interface_object'])
            role = self.g_if_obj.Get(WPAS_DBUS_IFACE_P2PDEVICE, "Role",
                                     dbus_interface=dbus.PROPERTIES_IFACE)
            if role != "GO":
                self.exceptions = True
                raise Exception("Unexpected role reported: " + role)
            group = self.g_if_obj.Get(WPAS_DBUS_IFACE_P2PDEVICE, "Group",
                                      dbus_interface=dbus.PROPERTIES_IFACE)
            if group != properties['group_object']:
                self.exceptions = True
                raise Exception("Unexpected Group reported: " + str(group))
            go = self.g_if_obj.Get(WPAS_DBUS_IFACE_P2PDEVICE, "PeerGO",
                                   dbus_interface=dbus.PROPERTIES_IFACE)
            if go != '/':
                self.exceptions = True
                raise Exception("Unexpected PeerGO value: " + str(go))
            if self.first:
                self.first = False
                logger.info("Remove persistent group instance")
                group_p2p = dbus.Interface(self.g_if_obj,
                                           WPAS_DBUS_IFACE_P2PDEVICE)
                group_p2p.Disconnect()
            else:
                dev1 = WpaSupplicant('wlan1', '/tmp/wpas-wlan1')
                dev1.global_request("P2P_CONNECT " + addr0 + " 12345670 join")

        def groupFinished(self, properties):
            logger.debug("groupFinished: " + str(properties))
            if self.waiting_end:
                logger.info("Remove persistent group")
                p2p.RemovePersistentGroup(self.persistent)
            else:
                logger.info("Re-start persistent group")
                params = dbus.Dictionary({'persistent_group_object':
                                          self.persistent,
                                          'frequency': 2412})
                p2p.GroupAdd(params)

        def persistentGroupAdded(self, path, properties):
            logger.debug("persistentGroupAdded: %s %s" % (path, str(properties)))
            self.persistent = path

        def persistentGroupRemoved(self, path):
            logger.debug("persistentGroupRemoved: %s" % path)
            self.done = True
            self.loop.quit()

        def deviceFound(self, path):
            logger.debug("deviceFound: path=%s" % path)
            peer_obj = bus.get_object(WPAS_DBUS_SERVICE, path)
            self.peer = peer_obj.GetAll(WPAS_DBUS_P2P_PEER,
                                        dbus_interface=dbus.PROPERTIES_IFACE,
                                        byte_arrays=True)
            logger.debug('peer properties: ' + str(self.peer))

        def provisionDiscoveryRequestDisplayPin(self, peer_object, pin):
            logger.debug("provisionDiscoveryRequestDisplayPin - peer=%s pin=%s" % (peer_object, pin))
            self.peer_path = peer_object
            peer = binascii.unhexlify(peer_object.split('/')[-1])
            addr = ':'.join(["%02x" % i for i in struct.unpack('6B', peer)])

            params = {'Role': 'registrar',
                      'P2PDeviceAddress': self.peer['DeviceAddress'],
                      'Bssid': self.peer['DeviceAddress'],
                      'Type': 'pin'}
            wps = dbus.Interface(self.g_if_obj, WPAS_DBUS_IFACE_WPS)
            try:
                wps.Start(params)
                self.exceptions = True
                raise Exception("Invalid WPS.Start() accepted")
            except dbus.exceptions.DBusException as e:
                if "InvalidArgs" not in str(e):
                    self.exceptions = True
                    raise Exception("Unexpected error message: " + str(e))
            params = {'Role': 'registrar',
                      'P2PDeviceAddress': self.peer['DeviceAddress'],
                      'Type': 'pin',
                      'Pin': '12345670'}
            logger.info("Authorize peer to connect to the group")
            wps.Start(params)

        def staAuthorized(self, name):
            logger.debug("staAuthorized: " + name)
            peer_obj = bus.get_object(WPAS_DBUS_SERVICE, self.peer_path)
            res = peer_obj.GetAll(WPAS_DBUS_P2P_PEER,
                                  dbus_interface=dbus.PROPERTIES_IFACE,
                                  byte_arrays=True)
            logger.debug("Peer properties: " + str(res))
            if 'Groups' not in res or len(res['Groups']) != 1:
                self.exceptions = True
                raise Exception("Unexpected number of peer Groups entries")
            if res['Groups'][0] != self.group:
                self.exceptions = True
                raise Exception("Unexpected peer Groups[0] value")

            g_obj = bus.get_object(WPAS_DBUS_SERVICE, self.group)
            res = g_obj.GetAll(WPAS_DBUS_GROUP,
                               dbus_interface=dbus.PROPERTIES_IFACE,
                               byte_arrays=True)
            logger.debug("Group properties: " + str(res))
            if 'Members' not in res or len(res['Members']) != 1:
                self.exceptions = True
                raise Exception("Unexpected number of group members")

            ext = dbus.ByteArray(b"\x11\x22\x33\x44")
            # Earlier implementation of this interface was a bit strange. The
            # property is defined to have aay signature and that is what the
            # getter returned. However, the setter expected there to be a
            # dictionary with 'WPSVendorExtensions' as the key surrounding these
            # values.. The current implementations maintains support for that
            # for backwards compability reasons. Verify that encoding first.
            vals = dbus.Dictionary({'WPSVendorExtensions': [ext]},
                                   signature='sv')
            g_obj.Set(WPAS_DBUS_GROUP, 'WPSVendorExtensions', vals,
                      dbus_interface=dbus.PROPERTIES_IFACE)
            res = g_obj.Get(WPAS_DBUS_GROUP, 'WPSVendorExtensions',
                               dbus_interface=dbus.PROPERTIES_IFACE,
                               byte_arrays=True)
            if len(res) != 1:
                self.exceptions = True
                raise Exception("Unexpected number of vendor extensions")
            if res[0] != ext:
                self.exceptions = True
                raise Exception("Vendor extension value changed")

            # And now verify that the more appropriate encoding is accepted as
            # well.
            res.append(dbus.ByteArray(b'\xaa\xbb\xcc\xdd\xee\xff'))
            g_obj.Set(WPAS_DBUS_GROUP, 'WPSVendorExtensions', res,
                      dbus_interface=dbus.PROPERTIES_IFACE)
            res2 = g_obj.Get(WPAS_DBUS_GROUP, 'WPSVendorExtensions',
                             dbus_interface=dbus.PROPERTIES_IFACE,
                             byte_arrays=True)
            if len(res) != 2:
                self.exceptions = True
                raise Exception("Unexpected number of vendor extensions")
            if res[0] != res2[0] or res[1] != res2[1]:
                self.exceptions = True
                raise Exception("Vendor extension value changed")

            for i in range(10):
                res.append(dbus.ByteArray(b'\xaa\xbb'))
            try:
                g_obj.Set(WPAS_DBUS_GROUP, 'WPSVendorExtensions', res,
                          dbus_interface=dbus.PROPERTIES_IFACE)
                self.exceptions = True
                raise Exception("Invalid Set(WPSVendorExtensions) accepted")
            except dbus.exceptions.DBusException as e:
                if "Error.Failed" not in str(e):
                    self.exceptions = True
                    raise Exception("Unexpected error message for invalid Set(WPSVendorExtensions): " + str(e))

            vals = dbus.Dictionary({'Foo': [ext]}, signature='sv')
            try:
                g_obj.Set(WPAS_DBUS_GROUP, 'WPSVendorExtensions', vals,
                          dbus_interface=dbus.PROPERTIES_IFACE)
                self.exceptions = True
                raise Exception("Invalid Set(WPSVendorExtensions) accepted")
            except dbus.exceptions.DBusException as e:
                if "InvalidArgs" not in str(e):
                    self.exceptions = True
                    raise Exception("Unexpected error message for invalid Set(WPSVendorExtensions): " + str(e))

            vals = ["foo"]
            try:
                g_obj.Set(WPAS_DBUS_GROUP, 'WPSVendorExtensions', vals,
                          dbus_interface=dbus.PROPERTIES_IFACE)
                self.exceptions = True
                raise Exception("Invalid Set(WPSVendorExtensions) accepted")
            except dbus.exceptions.DBusException as e:
                if "Error.Failed" not in str(e):
                    self.exceptions = True
                    raise Exception("Unexpected error message for invalid Set(WPSVendorExtensions): " + str(e))

            vals = [["foo"]]
            try:
                g_obj.Set(WPAS_DBUS_GROUP, 'WPSVendorExtensions', vals,
                          dbus_interface=dbus.PROPERTIES_IFACE)
                self.exceptions = True
                raise Exception("Invalid Set(WPSVendorExtensions) accepted")
            except dbus.exceptions.DBusException as e:
                if "Error.Failed" not in str(e):
                    self.exceptions = True
                    raise Exception("Unexpected error message for invalid Set(WPSVendorExtensions): " + str(e))

            p2p.RemoveClient({'peer': self.peer_path})

            self.waiting_end = True
            group_p2p = dbus.Interface(self.g_if_obj,
                                       WPAS_DBUS_IFACE_P2PDEVICE)
            group_p2p.Disconnect()

        def staDeauthorized(self, name):
            logger.debug("staDeauthorized: " + name)
            self.deauthorized = True

        def run_test(self, *args):
            logger.debug("run_test")
            params = dbus.Dictionary({'persistent': True,
                                      'frequency': 2412})
            logger.info("Add a persistent group")
            p2p.GroupAdd(params)
            return False

        def success(self):
            return self.done and self.deauthorized and not self.exceptions

    with TestDbusP2p(bus) as t:
        if not t.success():
            raise Exception("Expected signals not seen")

    dev[1].wait_go_ending_session()

def test_dbus_p2p_autogo_pbc(dev, apdev):
    """D-Bus P2P autonomous GO and PBC"""
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    p2p = dbus.Interface(if_obj, WPAS_DBUS_IFACE_P2PDEVICE)

    addr0 = dev[0].p2p_dev_addr()

    class TestDbusP2p(TestDbus):
        def __init__(self, bus):
            TestDbus.__init__(self, bus)
            self.first = True
            self.waiting_end = False
            self.done = False

        def __enter__(self):
            gobject.timeout_add(1, self.run_test)
            gobject.timeout_add(15000, self.timeout)
            self.add_signal(self.deviceFound, WPAS_DBUS_IFACE_P2PDEVICE,
                            "DeviceFound")
            self.add_signal(self.groupStarted, WPAS_DBUS_IFACE_P2PDEVICE,
                            "GroupStarted")
            self.add_signal(self.groupFinished, WPAS_DBUS_IFACE_P2PDEVICE,
                            "GroupFinished")
            self.add_signal(self.provisionDiscoveryPBCRequest,
                            WPAS_DBUS_IFACE_P2PDEVICE,
                            "ProvisionDiscoveryPBCRequest")
            self.add_signal(self.staAuthorized, WPAS_DBUS_IFACE,
                            "StaAuthorized")
            self.loop.run()
            return self

        def groupStarted(self, properties):
            logger.debug("groupStarted: " + str(properties))
            self.group = properties['group_object']
            self.g_if_obj = bus.get_object(WPAS_DBUS_SERVICE,
                                           properties['interface_object'])
            dev1 = WpaSupplicant('wlan1', '/tmp/wpas-wlan1')
            dev1.global_request("P2P_CONNECT " + addr0 + " pbc join")

        def groupFinished(self, properties):
            logger.debug("groupFinished: " + str(properties))
            self.done = True
            self.loop.quit()

        def deviceFound(self, path):
            logger.debug("deviceFound: path=%s" % path)
            peer_obj = bus.get_object(WPAS_DBUS_SERVICE, path)
            self.peer = peer_obj.GetAll(WPAS_DBUS_P2P_PEER,
                                        dbus_interface=dbus.PROPERTIES_IFACE,
                                        byte_arrays=True)
            logger.debug('peer properties: ' + str(self.peer))

        def provisionDiscoveryPBCRequest(self, peer_object):
            logger.debug("provisionDiscoveryPBCRequest - peer=%s" % peer_object)
            self.peer_path = peer_object
            peer = binascii.unhexlify(peer_object.split('/')[-1])
            addr = ':'.join(["%02x" % i for i in struct.unpack('6B', peer)])
            params = {'Role': 'registrar',
                      'P2PDeviceAddress': self.peer['DeviceAddress'],
                      'Type': 'pbc'}
            logger.info("Authorize peer to connect to the group")
            wps = dbus.Interface(self.g_if_obj, WPAS_DBUS_IFACE_WPS)
            wps.Start(params)

        def staAuthorized(self, name):
            logger.debug("staAuthorized: " + name)
            group_p2p = dbus.Interface(self.g_if_obj,
                                       WPAS_DBUS_IFACE_P2PDEVICE)
            group_p2p.Disconnect()

        def run_test(self, *args):
            logger.debug("run_test")
            params = dbus.Dictionary({'frequency': 2412})
            p2p.GroupAdd(params)
            return False

        def success(self):
            return self.done

    with TestDbusP2p(bus) as t:
        if not t.success():
            raise Exception("Expected signals not seen")

    dev[1].wait_go_ending_session()
    dev[1].flush_scan_cache()

def test_dbus_p2p_autogo_legacy(dev, apdev):
    """D-Bus P2P autonomous GO and legacy STA"""
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    p2p = dbus.Interface(if_obj, WPAS_DBUS_IFACE_P2PDEVICE)

    addr0 = dev[0].p2p_dev_addr()

    class TestDbusP2p(TestDbus):
        def __init__(self, bus):
            TestDbus.__init__(self, bus)
            self.done = False

        def __enter__(self):
            gobject.timeout_add(1, self.run_test)
            gobject.timeout_add(15000, self.timeout)
            self.add_signal(self.groupStarted, WPAS_DBUS_IFACE_P2PDEVICE,
                            "GroupStarted")
            self.add_signal(self.groupFinished, WPAS_DBUS_IFACE_P2PDEVICE,
                            "GroupFinished")
            self.add_signal(self.staAuthorized, WPAS_DBUS_IFACE,
                            "StaAuthorized")
            self.loop.run()
            return self

        def groupStarted(self, properties):
            logger.debug("groupStarted: " + str(properties))
            g_obj = bus.get_object(WPAS_DBUS_SERVICE,
                                   properties['group_object'])
            res = g_obj.GetAll(WPAS_DBUS_GROUP,
                               dbus_interface=dbus.PROPERTIES_IFACE,
                               byte_arrays=True)
            bssid = ':'.join(["%02x" % i for i in struct.unpack('6B', res['BSSID'])])

            pin = '12345670'
            params = {'Role': 'enrollee',
                      'Type': 'pin',
                      'Pin': pin}
            g_if_obj = bus.get_object(WPAS_DBUS_SERVICE,
                                      properties['interface_object'])
            wps = dbus.Interface(g_if_obj, WPAS_DBUS_IFACE_WPS)
            wps.Start(params)
            dev1 = WpaSupplicant('wlan1', '/tmp/wpas-wlan1')
            dev1.scan_for_bss(bssid, freq=2412)
            dev1.request("WPS_PIN " + bssid + " " + pin)
            self.group_p2p = dbus.Interface(g_if_obj, WPAS_DBUS_IFACE_P2PDEVICE)

        def groupFinished(self, properties):
            logger.debug("groupFinished: " + str(properties))
            self.done = True
            self.loop.quit()

        def staAuthorized(self, name):
            logger.debug("staAuthorized: " + name)
            dev1 = WpaSupplicant('wlan1', '/tmp/wpas-wlan1')
            dev1.request("DISCONNECT")
            self.group_p2p.Disconnect()

        def run_test(self, *args):
            logger.debug("run_test")
            params = dbus.Dictionary({'frequency': 2412})
            p2p.GroupAdd(params)
            return False

        def success(self):
            return self.done

    with TestDbusP2p(bus) as t:
        if not t.success():
            raise Exception("Expected signals not seen")

def test_dbus_p2p_join(dev, apdev):
    """D-Bus P2P join an autonomous GO"""
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    p2p = dbus.Interface(if_obj, WPAS_DBUS_IFACE_P2PDEVICE)

    addr1 = dev[1].p2p_dev_addr()
    addr2 = dev[2].p2p_dev_addr()
    dev[1].p2p_start_go(freq=2412)
    dev1_group_ifname = dev[1].group_ifname
    dev[2].p2p_listen()

    class TestDbusP2p(TestDbus):
        def __init__(self, bus):
            TestDbus.__init__(self, bus)
            self.done = False
            self.peer = None
            self.go = None

        def __enter__(self):
            gobject.timeout_add(1, self.run_test)
            gobject.timeout_add(15000, self.timeout)
            self.add_signal(self.deviceFound, WPAS_DBUS_IFACE_P2PDEVICE,
                            "DeviceFound")
            self.add_signal(self.groupStarted, WPAS_DBUS_IFACE_P2PDEVICE,
                            "GroupStarted")
            self.add_signal(self.groupFinished, WPAS_DBUS_IFACE_P2PDEVICE,
                            "GroupFinished")
            self.add_signal(self.invitationResult, WPAS_DBUS_IFACE_P2PDEVICE,
                            "InvitationResult")
            self.loop.run()
            return self

        def deviceFound(self, path):
            logger.debug("deviceFound: path=%s" % path)
            peer_obj = bus.get_object(WPAS_DBUS_SERVICE, path)
            res = peer_obj.GetAll(WPAS_DBUS_P2P_PEER,
                                  dbus_interface=dbus.PROPERTIES_IFACE,
                                  byte_arrays=True)
            logger.debug('peer properties: ' + str(res))
            if addr2.replace(':', '') in path:
                self.peer = path
            elif addr1.replace(':', '') in path:
                self.go = path
            if self.peer and self.go:
                logger.info("Join the group")
                p2p.StopFind()
                args = {'peer': self.go,
                        'join': True,
                        'wps_method': 'pin',
                        'frequency': 2412}
                pin = p2p.Connect(args)

                dev1 = WpaSupplicant('wlan1', '/tmp/wpas-wlan1')
                dev1.group_ifname = dev1_group_ifname
                dev1.group_request("WPS_PIN any " + pin)

        def groupStarted(self, properties):
            logger.debug("groupStarted: " + str(properties))
            g_if_obj = bus.get_object(WPAS_DBUS_SERVICE,
                                      properties['interface_object'])
            role = g_if_obj.Get(WPAS_DBUS_IFACE_P2PDEVICE, "Role",
                                dbus_interface=dbus.PROPERTIES_IFACE)
            if role != "client":
                raise Exception("Unexpected role reported: " + role)
            group = g_if_obj.Get(WPAS_DBUS_IFACE_P2PDEVICE, "Group",
                                 dbus_interface=dbus.PROPERTIES_IFACE)
            if group != properties['group_object']:
                raise Exception("Unexpected Group reported: " + str(group))
            go = g_if_obj.Get(WPAS_DBUS_IFACE_P2PDEVICE, "PeerGO",
                              dbus_interface=dbus.PROPERTIES_IFACE)
            if go != self.go:
                raise Exception("Unexpected PeerGO value: " + str(go))

            g_obj = bus.get_object(WPAS_DBUS_SERVICE,
                                   properties['group_object'])
            res = g_obj.GetAll(WPAS_DBUS_GROUP,
                               dbus_interface=dbus.PROPERTIES_IFACE,
                               byte_arrays=True)
            logger.debug("Group properties: " + str(res))

            ext = dbus.ByteArray(b"\x11\x22\x33\x44")
            try:
                # Set(WPSVendorExtensions) not allowed for P2P Client
                g_obj.Set(WPAS_DBUS_GROUP, 'WPSVendorExtensions', res,
                          dbus_interface=dbus.PROPERTIES_IFACE)
                raise Exception("Invalid Set(WPSVendorExtensions) accepted")
            except dbus.exceptions.DBusException as e:
                if "Error.Failed: Failed to set property" not in str(e):
                    raise Exception("Unexpected error message for invalid Set(WPSVendorExtensions): " + str(e))

            group_p2p = dbus.Interface(g_if_obj, WPAS_DBUS_IFACE_P2PDEVICE)
            args = {'duration1': 30000, 'interval1': 102400,
                    'duration2': 20000, 'interval2': 102400}
            group_p2p.PresenceRequest(args)

            args = {'peer': self.peer}
            group_p2p.Invite(args)

        def groupFinished(self, properties):
            logger.debug("groupFinished: " + str(properties))
            self.done = True
            self.loop.quit()

        def invitationResult(self, result):
            logger.debug("invitationResult: " + str(result))
            if result['status'] != 1:
                raise Exception("Unexpected invitation result: " + str(result))
            dev1 = WpaSupplicant('wlan1', '/tmp/wpas-wlan1')
            dev1.group_ifname = dev1_group_ifname
            dev1.remove_group()

        def run_test(self, *args):
            logger.debug("run_test")
            p2p.Find(dbus.Dictionary({'DiscoveryType': 'social'}))
            return False

        def success(self):
            return self.done

    with TestDbusP2p(bus) as t:
        if not t.success():
            raise Exception("Expected signals not seen")

    dev[2].p2p_stop_find()

def test_dbus_p2p_invitation_received(dev, apdev):
    """D-Bus P2P and InvitationReceived"""
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    p2p = dbus.Interface(if_obj, WPAS_DBUS_IFACE_P2PDEVICE)

    form(dev[0], dev[1])
    addr0 = dev[0].p2p_dev_addr()
    dev[0].p2p_listen()
    dev[0].global_request("SET persistent_reconnect 0")

    if not dev[1].discover_peer(addr0, social=True):
        raise Exception("Peer " + addr0 + " not found")
    peer = dev[1].get_peer(addr0)

    class TestDbusP2p(TestDbus):
        def __init__(self, bus):
            TestDbus.__init__(self, bus)
            self.done = False

        def __enter__(self):
            gobject.timeout_add(1, self.run_test)
            gobject.timeout_add(15000, self.timeout)
            self.add_signal(self.invitationReceived, WPAS_DBUS_IFACE_P2PDEVICE,
                            "InvitationReceived")
            self.loop.run()
            return self

        def invitationReceived(self, result):
            logger.debug("invitationReceived: " + str(result))
            self.done = True
            self.loop.quit()

        def run_test(self, *args):
            logger.debug("run_test")
            dev1 = WpaSupplicant('wlan1', '/tmp/wpas-wlan1')
            cmd = "P2P_INVITE persistent=" + peer['persistent'] + " peer=" + addr0
            dev1.global_request(cmd)
            return False

        def success(self):
            return self.done

    with TestDbusP2p(bus) as t:
        if not t.success():
            raise Exception("Expected signals not seen")

    dev[0].p2p_stop_find()
    dev[1].p2p_stop_find()

def test_dbus_p2p_config(dev, apdev):
    """D-Bus Get/Set P2PDeviceConfig"""
    try:
        _test_dbus_p2p_config(dev, apdev)
    finally:
        dev[0].request("P2P_SET ssid_postfix ")

def _test_dbus_p2p_config(dev, apdev):
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    p2p = dbus.Interface(if_obj, WPAS_DBUS_IFACE_P2PDEVICE)

    res = if_obj.Get(WPAS_DBUS_IFACE_P2PDEVICE, "P2PDeviceConfig",
                     dbus_interface=dbus.PROPERTIES_IFACE,
                     byte_arrays=True)
    if_obj.Set(WPAS_DBUS_IFACE_P2PDEVICE, "P2PDeviceConfig", res,
               dbus_interface=dbus.PROPERTIES_IFACE)
    res2 = if_obj.Get(WPAS_DBUS_IFACE_P2PDEVICE, "P2PDeviceConfig",
                      dbus_interface=dbus.PROPERTIES_IFACE,
                      byte_arrays=True)

    if len(res) != len(res2):
        raise Exception("Different number of parameters")
    for k in res:
        if res[k] != res2[k]:
            raise Exception("Parameter %s value changes" % k)

    changes = {'SsidPostfix': 'foo',
               'VendorExtension': [dbus.ByteArray(b'\x11\x22\x33\x44')],
               'SecondaryDeviceTypes': [dbus.ByteArray(b'\x11\x22\x33\x44\x55\x66\x77\x88')]}
    if_obj.Set(WPAS_DBUS_IFACE_P2PDEVICE, "P2PDeviceConfig",
               dbus.Dictionary(changes, signature='sv'),
               dbus_interface=dbus.PROPERTIES_IFACE)

    res2 = if_obj.Get(WPAS_DBUS_IFACE_P2PDEVICE, "P2PDeviceConfig",
                      dbus_interface=dbus.PROPERTIES_IFACE,
                      byte_arrays=True)
    logger.debug("P2PDeviceConfig: " + str(res2))
    if 'VendorExtension' not in res2 or len(res2['VendorExtension']) != 1:
        raise Exception("VendorExtension does not match")
    if 'SecondaryDeviceTypes' not in res2 or len(res2['SecondaryDeviceTypes']) != 1:
        raise Exception("SecondaryDeviceType does not match")

    changes = {'SsidPostfix': '',
               'VendorExtension': dbus.Array([], signature="ay"),
               'SecondaryDeviceTypes': dbus.Array([], signature="ay")}
    if_obj.Set(WPAS_DBUS_IFACE_P2PDEVICE, "P2PDeviceConfig",
               dbus.Dictionary(changes, signature='sv'),
               dbus_interface=dbus.PROPERTIES_IFACE)

    res3 = if_obj.Get(WPAS_DBUS_IFACE_P2PDEVICE, "P2PDeviceConfig",
                      dbus_interface=dbus.PROPERTIES_IFACE,
                      byte_arrays=True)
    logger.debug("P2PDeviceConfig: " + str(res3))
    if 'VendorExtension' in res3:
        raise Exception("VendorExtension not removed")
    if 'SecondaryDeviceTypes' in res3:
        raise Exception("SecondaryDeviceType not removed")

    try:
        dev[0].request("P2P_SET disabled 1")
        if_obj.Get(WPAS_DBUS_IFACE_P2PDEVICE, "P2PDeviceConfig",
                   dbus_interface=dbus.PROPERTIES_IFACE,
                   byte_arrays=True)
        raise Exception("Invalid Get(P2PDeviceConfig) accepted")
    except dbus.exceptions.DBusException as e:
        if "Error.Failed: P2P is not available for this interface" not in str(e):
            raise Exception("Unexpected error message for invalid Invite: " + str(e))
    finally:
        dev[0].request("P2P_SET disabled 0")

    try:
        dev[0].request("P2P_SET disabled 1")
        changes = {'SsidPostfix': 'foo'}
        if_obj.Set(WPAS_DBUS_IFACE_P2PDEVICE, "P2PDeviceConfig",
                   dbus.Dictionary(changes, signature='sv'),
                   dbus_interface=dbus.PROPERTIES_IFACE)
        raise Exception("Invalid Set(P2PDeviceConfig) accepted")
    except dbus.exceptions.DBusException as e:
        if "Error.Failed: P2P is not available for this interface" not in str(e):
            raise Exception("Unexpected error message for invalid Invite: " + str(e))
    finally:
        dev[0].request("P2P_SET disabled 0")

    tests = [{'DeviceName': 123},
             {'SsidPostfix': 123},
             {'Foo': 'Bar'}]
    for changes in tests:
        try:
            if_obj.Set(WPAS_DBUS_IFACE_P2PDEVICE, "P2PDeviceConfig",
                       dbus.Dictionary(changes, signature='sv'),
                       dbus_interface=dbus.PROPERTIES_IFACE)
            raise Exception("Invalid Set(P2PDeviceConfig) accepted")
        except dbus.exceptions.DBusException as e:
            if "InvalidArgs" not in str(e):
                raise Exception("Unexpected error message for invalid Invite: " + str(e))

def test_dbus_p2p_persistent(dev, apdev):
    """D-Bus P2P persistent group"""
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    p2p = dbus.Interface(if_obj, WPAS_DBUS_IFACE_P2PDEVICE)

    class TestDbusP2p(TestDbus):
        def __init__(self, bus):
            TestDbus.__init__(self, bus)

        def __enter__(self):
            gobject.timeout_add(1, self.run_test)
            gobject.timeout_add(15000, self.timeout)
            self.add_signal(self.groupStarted, WPAS_DBUS_IFACE_P2PDEVICE,
                            "GroupStarted")
            self.add_signal(self.groupFinished, WPAS_DBUS_IFACE_P2PDEVICE,
                            "GroupFinished")
            self.add_signal(self.persistentGroupAdded,
                            WPAS_DBUS_IFACE_P2PDEVICE,
                            "PersistentGroupAdded")
            self.loop.run()
            return self

        def groupStarted(self, properties):
            logger.debug("groupStarted: " + str(properties))
            g_if_obj = bus.get_object(WPAS_DBUS_SERVICE,
                                      properties['interface_object'])
            group_p2p = dbus.Interface(g_if_obj, WPAS_DBUS_IFACE_P2PDEVICE)
            group_p2p.Disconnect()

        def groupFinished(self, properties):
            logger.debug("groupFinished: " + str(properties))
            self.loop.quit()

        def persistentGroupAdded(self, path, properties):
            logger.debug("persistentGroupAdded: %s %s" % (path, str(properties)))
            self.persistent = path

        def run_test(self, *args):
            logger.debug("run_test")
            params = dbus.Dictionary({'persistent': True,
                                      'frequency': 2412})
            logger.info("Add a persistent group")
            p2p.GroupAdd(params)
            return False

        def success(self):
            return True

    with TestDbusP2p(bus) as t:
        if not t.success():
            raise Exception("Expected signals not seen")
        persistent = t.persistent

    p_obj = bus.get_object(WPAS_DBUS_SERVICE, persistent)
    res = p_obj.Get(WPAS_DBUS_PERSISTENT_GROUP, "Properties",
                    dbus_interface=dbus.PROPERTIES_IFACE, byte_arrays=True)
    logger.info("Persistent group Properties: " + str(res))
    vals = dbus.Dictionary({'ssid': 'DIRECT-foo'}, signature='sv')
    p_obj.Set(WPAS_DBUS_PERSISTENT_GROUP, "Properties", vals,
              dbus_interface=dbus.PROPERTIES_IFACE)
    res2 = p_obj.Get(WPAS_DBUS_PERSISTENT_GROUP, "Properties",
                     dbus_interface=dbus.PROPERTIES_IFACE)
    if len(res) != len(res2):
        raise Exception("Different number of parameters")
    for k in res:
        if k != 'ssid' and res[k] != res2[k]:
            raise Exception("Parameter %s value changes" % k)
    if res2['ssid'] != '"DIRECT-foo"':
        raise Exception("Unexpected ssid")

    args = dbus.Dictionary({'ssid': 'DIRECT-testing',
                            'psk': '1234567890'}, signature='sv')
    group = p2p.AddPersistentGroup(args)

    groups = if_obj.Get(WPAS_DBUS_IFACE_P2PDEVICE, "PersistentGroups",
                        dbus_interface=dbus.PROPERTIES_IFACE)
    if len(groups) != 2:
        raise Exception("Unexpected number of persistent groups: " + str(groups))

    p2p.RemoveAllPersistentGroups()

    groups = if_obj.Get(WPAS_DBUS_IFACE_P2PDEVICE, "PersistentGroups",
                        dbus_interface=dbus.PROPERTIES_IFACE)
    if len(groups) != 0:
        raise Exception("Unexpected number of persistent groups: " + str(groups))

    try:
        p2p.RemovePersistentGroup(persistent)
        raise Exception("Invalid RemovePersistentGroup accepted")
    except dbus.exceptions.DBusException as e:
        if "NetworkUnknown: There is no such persistent group" not in str(e):
            raise Exception("Unexpected error message for invalid RemovePersistentGroup: " + str(e))

def test_dbus_p2p_reinvoke_persistent(dev, apdev):
    """D-Bus P2P reinvoke persistent group"""
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    p2p = dbus.Interface(if_obj, WPAS_DBUS_IFACE_P2PDEVICE)

    addr0 = dev[0].p2p_dev_addr()

    class TestDbusP2p(TestDbus):
        def __init__(self, bus):
            TestDbus.__init__(self, bus)
            self.first = True
            self.waiting_end = False
            self.done = False
            self.invited = False

        def __enter__(self):
            gobject.timeout_add(1, self.run_test)
            gobject.timeout_add(15000, self.timeout)
            self.add_signal(self.deviceFound, WPAS_DBUS_IFACE_P2PDEVICE,
                            "DeviceFound")
            self.add_signal(self.groupStarted, WPAS_DBUS_IFACE_P2PDEVICE,
                            "GroupStarted")
            self.add_signal(self.groupFinished, WPAS_DBUS_IFACE_P2PDEVICE,
                            "GroupFinished")
            self.add_signal(self.persistentGroupAdded,
                            WPAS_DBUS_IFACE_P2PDEVICE,
                            "PersistentGroupAdded")
            self.add_signal(self.provisionDiscoveryRequestDisplayPin,
                            WPAS_DBUS_IFACE_P2PDEVICE,
                            "ProvisionDiscoveryRequestDisplayPin")
            self.add_signal(self.staAuthorized, WPAS_DBUS_IFACE,
                            "StaAuthorized")
            self.loop.run()
            return self

        def groupStarted(self, properties):
            logger.debug("groupStarted: " + str(properties))
            self.g_if_obj = bus.get_object(WPAS_DBUS_SERVICE,
                                           properties['interface_object'])
            if not self.invited:
                g_obj = bus.get_object(WPAS_DBUS_SERVICE,
                                       properties['group_object'])
                res = g_obj.GetAll(WPAS_DBUS_GROUP,
                                   dbus_interface=dbus.PROPERTIES_IFACE,
                                   byte_arrays=True)
                bssid = ':'.join(["%02x" % i for i in struct.unpack('6B', res['BSSID'])])
                dev1 = WpaSupplicant('wlan1', '/tmp/wpas-wlan1')
                dev1.scan_for_bss(bssid, freq=2412)
                dev1.global_request("P2P_CONNECT " + addr0 + " 12345670 join")

        def groupFinished(self, properties):
            logger.debug("groupFinished: " + str(properties))
            if self.invited:
                self.done = True
                self.loop.quit()
            else:
                dev1 = WpaSupplicant('wlan1', '/tmp/wpas-wlan1')
                dev1.global_request("SET persistent_reconnect 1")
                dev1.p2p_listen()

                args = {'persistent_group_object': dbus.ObjectPath(path),
                        'peer': self.peer_path}
                try:
                    pin = p2p.Invite(args)
                    raise Exception("Invalid Invite accepted")
                except dbus.exceptions.DBusException as e:
                    if "InvalidArgs" not in str(e):
                        raise Exception("Unexpected error message for invalid Invite: " + str(e))

                args = {'persistent_group_object': self.persistent,
                        'peer': self.peer_path}
                pin = p2p.Invite(args)
                self.invited = True

                self.sta_group_ev = dev1.wait_global_event(["P2P-GROUP-STARTED"],
                                                           timeout=15)
                if self.sta_group_ev is None:
                    raise Exception("P2P-GROUP-STARTED event not seen")

        def persistentGroupAdded(self, path, properties):
            logger.debug("persistentGroupAdded: %s %s" % (path, str(properties)))
            self.persistent = path

        def deviceFound(self, path):
            logger.debug("deviceFound: path=%s" % path)
            peer_obj = bus.get_object(WPAS_DBUS_SERVICE, path)
            self.peer = peer_obj.GetAll(WPAS_DBUS_P2P_PEER,
                                        dbus_interface=dbus.PROPERTIES_IFACE,
                                        byte_arrays=True)

        def provisionDiscoveryRequestDisplayPin(self, peer_object, pin):
            logger.debug("provisionDiscoveryRequestDisplayPin - peer=%s pin=%s" % (peer_object, pin))
            self.peer_path = peer_object
            peer = binascii.unhexlify(peer_object.split('/')[-1])
            addr = ':'.join(["%02x" % i for i in struct.unpack('6B', peer)])
            params = {'Role': 'registrar',
                      'P2PDeviceAddress': self.peer['DeviceAddress'],
                      'Bssid': self.peer['DeviceAddress'],
                      'Type': 'pin',
                      'Pin': '12345670'}
            logger.info("Authorize peer to connect to the group")
            dev1 = WpaSupplicant('wlan1', '/tmp/wpas-wlan1')
            wps = dbus.Interface(self.g_if_obj, WPAS_DBUS_IFACE_WPS)
            wps.Start(params)
            self.sta_group_ev = dev1.wait_global_event(["P2P-GROUP-STARTED"],
                                                       timeout=15)
            if self.sta_group_ev is None:
                raise Exception("P2P-GROUP-STARTED event not seen")

        def staAuthorized(self, name):
            logger.debug("staAuthorized: " + name)
            dev1 = WpaSupplicant('wlan1', '/tmp/wpas-wlan1')
            dev1.group_form_result(self.sta_group_ev)
            dev1.remove_group()
            ev = dev1.wait_global_event(["P2P-GROUP-REMOVED"], timeout=10)
            if ev is None:
                raise Exception("Group removal timed out")
            group_p2p = dbus.Interface(self.g_if_obj, WPAS_DBUS_IFACE_P2PDEVICE)
            group_p2p.Disconnect()

        def run_test(self, *args):
            logger.debug("run_test")
            params = dbus.Dictionary({'persistent': True,
                                      'frequency': 2412})
            logger.info("Add a persistent group")
            p2p.GroupAdd(params)
            return False

        def success(self):
            return self.done

    with TestDbusP2p(bus) as t:
        if not t.success():
            raise Exception("Expected signals not seen")

def test_dbus_p2p_go_neg_rx(dev, apdev):
    """D-Bus P2P GO Negotiation receive"""
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    p2p = dbus.Interface(if_obj, WPAS_DBUS_IFACE_P2PDEVICE)
    addr0 = dev[0].p2p_dev_addr()

    class TestDbusP2p(TestDbus):
        def __init__(self, bus):
            TestDbus.__init__(self, bus)
            self.done = False

        def __enter__(self):
            gobject.timeout_add(1, self.run_test)
            gobject.timeout_add(15000, self.timeout)
            self.add_signal(self.deviceFound, WPAS_DBUS_IFACE_P2PDEVICE,
                            "DeviceFound")
            self.add_signal(self.goNegotiationRequest,
                            WPAS_DBUS_IFACE_P2PDEVICE,
                            "GONegotiationRequest",
                            byte_arrays=True)
            self.add_signal(self.goNegotiationSuccess,
                            WPAS_DBUS_IFACE_P2PDEVICE,
                            "GONegotiationSuccess",
                            byte_arrays=True)
            self.add_signal(self.groupStarted, WPAS_DBUS_IFACE_P2PDEVICE,
                            "GroupStarted")
            self.add_signal(self.groupFinished, WPAS_DBUS_IFACE_P2PDEVICE,
                            "GroupFinished")
            self.loop.run()
            return self

        def deviceFound(self, path):
            logger.debug("deviceFound: path=%s" % path)

        def goNegotiationRequest(self, path, dev_passwd_id, go_intent=0):
            logger.debug("goNegotiationRequest: path=%s dev_passwd_id=%d go_intent=%d" % (path, dev_passwd_id, go_intent))
            if dev_passwd_id != 1:
                raise Exception("Unexpected dev_passwd_id=%d" % dev_passwd_id)
            args = {'peer': path, 'wps_method': 'display', 'pin': '12345670',
                    'go_intent': 15, 'persistent': False, 'frequency': 5175}
            try:
                p2p.Connect(args)
                raise Exception("Invalid Connect accepted")
            except dbus.exceptions.DBusException as e:
                if "ConnectChannelUnsupported" not in str(e):
                    raise Exception("Unexpected error message for invalid Connect: " + str(e))

            args = {'peer': path, 'wps_method': 'display', 'pin': '12345670',
                    'go_intent': 15, 'persistent': False}
            p2p.Connect(args)

        def goNegotiationSuccess(self, properties):
            logger.debug("goNegotiationSuccess: properties=%s" % str(properties))

        def groupStarted(self, properties):
            logger.debug("groupStarted: " + str(properties))
            g_if_obj = bus.get_object(WPAS_DBUS_SERVICE,
                                      properties['interface_object'])
            group_p2p = dbus.Interface(g_if_obj, WPAS_DBUS_IFACE_P2PDEVICE)
            group_p2p.Disconnect()

        def groupFinished(self, properties):
            logger.debug("groupFinished: " + str(properties))
            self.done = True
            self.loop.quit()

        def run_test(self, *args):
            logger.debug("run_test")
            p2p.Listen(10)
            dev1 = WpaSupplicant('wlan1', '/tmp/wpas-wlan1')
            if not dev1.discover_peer(addr0):
                raise Exception("Peer not found")
            dev1.global_request("P2P_CONNECT " + addr0 + " 12345670 enter")
            return False

        def success(self):
            return self.done

    with TestDbusP2p(bus) as t:
        if not t.success():
            raise Exception("Expected signals not seen")

def test_dbus_p2p_go_neg_auth(dev, apdev):
    """D-Bus P2P GO Negotiation authorized"""
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    p2p = dbus.Interface(if_obj, WPAS_DBUS_IFACE_P2PDEVICE)
    addr0 = dev[0].p2p_dev_addr()
    dev[1].p2p_listen()

    class TestDbusP2p(TestDbus):
        def __init__(self, bus):
            TestDbus.__init__(self, bus)
            self.done = False
            self.peer_joined = False
            self.peer_disconnected = False

        def __enter__(self):
            gobject.timeout_add(1, self.run_test)
            gobject.timeout_add(15000, self.timeout)
            self.add_signal(self.deviceFound, WPAS_DBUS_IFACE_P2PDEVICE,
                            "DeviceFound")
            self.add_signal(self.goNegotiationSuccess,
                            WPAS_DBUS_IFACE_P2PDEVICE,
                            "GONegotiationSuccess",
                            byte_arrays=True)
            self.add_signal(self.groupStarted, WPAS_DBUS_IFACE_P2PDEVICE,
                            "GroupStarted")
            self.add_signal(self.groupFinished, WPAS_DBUS_IFACE_P2PDEVICE,
                            "GroupFinished")
            self.add_signal(self.staDeauthorized, WPAS_DBUS_IFACE,
                            "StaDeauthorized")
            self.add_signal(self.peerJoined, WPAS_DBUS_GROUP,
                            "PeerJoined")
            self.add_signal(self.peerDisconnected, WPAS_DBUS_GROUP,
                            "PeerDisconnected")
            self.loop.run()
            return self

        def deviceFound(self, path):
            logger.debug("deviceFound: path=%s" % path)
            args = {'peer': path, 'wps_method': 'keypad',
                    'go_intent': 15, 'authorize_only': True}
            try:
                p2p.Connect(args)
                raise Exception("Invalid Connect accepted")
            except dbus.exceptions.DBusException as e:
                if "InvalidArgs" not in str(e):
                    raise Exception("Unexpected error message for invalid Connect: " + str(e))

            args = {'peer': path, 'wps_method': 'keypad', 'pin': '12345670',
                    'go_intent': 15, 'authorize_only': True}
            p2p.Connect(args)
            p2p.Listen(10)
            dev1 = WpaSupplicant('wlan1', '/tmp/wpas-wlan1')
            if not dev1.discover_peer(addr0):
                raise Exception("Peer not found")
            dev1.global_request("P2P_CONNECT " + addr0 + " 12345670 display go_intent=0")
            ev = dev1.wait_global_event(["P2P-GROUP-STARTED"], timeout=15)
            if ev is None:
                raise Exception("Group formation timed out")
            self.sta_group_ev = ev

        def goNegotiationSuccess(self, properties):
            logger.debug("goNegotiationSuccess: properties=%s" % str(properties))

        def groupStarted(self, properties):
            logger.debug("groupStarted: " + str(properties))
            self.g_if_obj = bus.get_object(WPAS_DBUS_SERVICE,
                                           properties['interface_object'])
            dev1 = WpaSupplicant('wlan1', '/tmp/wpas-wlan1')
            dev1.group_form_result(self.sta_group_ev)
            dev1.remove_group()

        def staDeauthorized(self, name):
            logger.debug("staDeuthorized: " + name)
            group_p2p = dbus.Interface(self.g_if_obj, WPAS_DBUS_IFACE_P2PDEVICE)
            group_p2p.Disconnect()

        def peerJoined(self, peer):
            logger.debug("peerJoined: " + peer)
            self.peer_joined = True

        def peerDisconnected(self, peer):
            logger.debug("peerDisconnected: " + peer)
            self.peer_disconnected = True

        def groupFinished(self, properties):
            logger.debug("groupFinished: " + str(properties))
            self.done = True
            self.loop.quit()

        def run_test(self, *args):
            logger.debug("run_test")
            p2p.Find(dbus.Dictionary({'DiscoveryType': 'social'}))
            return False

        def success(self):
            return self.done and self.peer_joined and self.peer_disconnected

    with TestDbusP2p(bus) as t:
        if not t.success():
            raise Exception("Expected signals not seen")

def test_dbus_p2p_go_neg_init(dev, apdev):
    """D-Bus P2P GO Negotiation initiation"""
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    p2p = dbus.Interface(if_obj, WPAS_DBUS_IFACE_P2PDEVICE)
    addr0 = dev[0].p2p_dev_addr()
    dev[1].p2p_listen()

    class TestDbusP2p(TestDbus):
        def __init__(self, bus):
            TestDbus.__init__(self, bus)
            self.done = False
            self.peer_group_added = False
            self.peer_group_removed = False

        def __enter__(self):
            gobject.timeout_add(1, self.run_test)
            gobject.timeout_add(15000, self.timeout)
            self.add_signal(self.deviceFound, WPAS_DBUS_IFACE_P2PDEVICE,
                            "DeviceFound")
            self.add_signal(self.goNegotiationSuccess,
                            WPAS_DBUS_IFACE_P2PDEVICE,
                            "GONegotiationSuccess",
                            byte_arrays=True)
            self.add_signal(self.groupStarted, WPAS_DBUS_IFACE_P2PDEVICE,
                            "GroupStarted")
            self.add_signal(self.groupFinished, WPAS_DBUS_IFACE_P2PDEVICE,
                            "GroupFinished")
            self.add_signal(self.propertiesChanged, dbus.PROPERTIES_IFACE,
                            "PropertiesChanged")
            self.loop.run()
            return self

        def deviceFound(self, path):
            logger.debug("deviceFound: path=%s" % path)
            dev1 = WpaSupplicant('wlan1', '/tmp/wpas-wlan1')
            args = {'peer': path, 'wps_method': 'keypad', 'pin': '12345670',
                    'go_intent': 0}
            p2p.Connect(args)

            ev = dev1.wait_global_event(["P2P-GO-NEG-REQUEST"], timeout=15)
            if ev is None:
                raise Exception("Timeout while waiting for GO Neg Request")
            dev1.global_request("P2P_CONNECT " + addr0 + " 12345670 display go_intent=15")
            ev = dev1.wait_global_event(["P2P-GROUP-STARTED"], timeout=15)
            if ev is None:
                raise Exception("Group formation timed out")
            self.sta_group_ev = ev
            dev1.close_monitor_global()
            dev1.close_monitor_mon()
            dev1 = None

        def goNegotiationSuccess(self, properties):
            logger.debug("goNegotiationSuccess: properties=%s" % str(properties))

        def groupStarted(self, properties):
            logger.debug("groupStarted: " + str(properties))
            g_if_obj = bus.get_object(WPAS_DBUS_SERVICE,
                                      properties['interface_object'])
            group_p2p = dbus.Interface(g_if_obj, WPAS_DBUS_IFACE_P2PDEVICE)
            group_p2p.Disconnect()
            dev1 = WpaSupplicant('wlan1', '/tmp/wpas-wlan1', monitor=False)
            dev1.group_form_result(self.sta_group_ev)
            dev1.remove_group()
            dev1 = None

        def groupFinished(self, properties):
            logger.debug("groupFinished: " + str(properties))
            self.done = True

        def propertiesChanged(self, interface_name, changed_properties,
                              invalidated_properties):
            logger.debug("propertiesChanged: interface_name=%s changed_properties=%s invalidated_properties=%s" % (interface_name, str(changed_properties), str(invalidated_properties)))
            if interface_name != WPAS_DBUS_P2P_PEER:
                return
            if "Groups" not in changed_properties:
                return
            if len(changed_properties["Groups"]) > 0:
                self.peer_group_added = True
            if len(changed_properties["Groups"]) == 0:
                if not self.peer_group_added:
                    # This is likely a leftover event from an earlier test case,
                    # ignore it to allow this test case to go through its steps.
                    logger.info("Ignore propertiesChanged indicating group removal before group has been added")
                    return
                self.peer_group_removed = True
                self.loop.quit()

        def run_test(self, *args):
            logger.debug("run_test")
            p2p.Find(dbus.Dictionary({'DiscoveryType': 'social'}))
            return False

        def success(self):
            return self.done and self.peer_group_added and self.peer_group_removed

    with TestDbusP2p(bus) as t:
        if not t.success():
            raise Exception("Expected signals not seen")

def test_dbus_p2p_group_termination_by_go(dev, apdev):
    """D-Bus P2P group removal on GO terminating the group"""
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    p2p = dbus.Interface(if_obj, WPAS_DBUS_IFACE_P2PDEVICE)
    addr0 = dev[0].p2p_dev_addr()
    dev[1].p2p_listen()

    class TestDbusP2p(TestDbus):
        def __init__(self, bus):
            TestDbus.__init__(self, bus)
            self.done = False
            self.peer_group_added = False
            self.peer_group_removed = False

        def __enter__(self):
            gobject.timeout_add(1, self.run_test)
            gobject.timeout_add(15000, self.timeout)
            self.add_signal(self.deviceFound, WPAS_DBUS_IFACE_P2PDEVICE,
                            "DeviceFound")
            self.add_signal(self.goNegotiationSuccess,
                            WPAS_DBUS_IFACE_P2PDEVICE,
                            "GONegotiationSuccess",
                            byte_arrays=True)
            self.add_signal(self.groupStarted, WPAS_DBUS_IFACE_P2PDEVICE,
                            "GroupStarted")
            self.add_signal(self.groupFinished, WPAS_DBUS_IFACE_P2PDEVICE,
                            "GroupFinished")
            self.add_signal(self.propertiesChanged, dbus.PROPERTIES_IFACE,
                            "PropertiesChanged")
            self.loop.run()
            return self

        def deviceFound(self, path):
            logger.debug("deviceFound: path=%s" % path)
            dev1 = WpaSupplicant('wlan1', '/tmp/wpas-wlan1')
            args = {'peer': path, 'wps_method': 'keypad', 'pin': '12345670',
                    'go_intent': 0}
            p2p.Connect(args)

            ev = dev1.wait_global_event(["P2P-GO-NEG-REQUEST"], timeout=15)
            if ev is None:
                raise Exception("Timeout while waiting for GO Neg Request")
            dev1.global_request("P2P_CONNECT " + addr0 + " 12345670 display go_intent=15")
            ev = dev1.wait_global_event(["P2P-GROUP-STARTED"], timeout=15)
            if ev is None:
                raise Exception("Group formation timed out")
            self.sta_group_ev = ev
            dev1.close_monitor_global()
            dev1.close_monitor_mon()
            dev1 = None

        def goNegotiationSuccess(self, properties):
            logger.debug("goNegotiationSuccess: properties=%s" % str(properties))

        def groupStarted(self, properties):
            logger.debug("groupStarted: " + str(properties))
            g_if_obj = bus.get_object(WPAS_DBUS_SERVICE,
                                      properties['interface_object'])
            dev1 = WpaSupplicant('wlan1', '/tmp/wpas-wlan1', monitor=False)
            dev1.group_form_result(self.sta_group_ev)
            dev1.remove_group()

        def groupFinished(self, properties):
            logger.debug("groupFinished: " + str(properties))
            self.done = True

        def propertiesChanged(self, interface_name, changed_properties,
                              invalidated_properties):
            logger.debug("propertiesChanged: interface_name=%s changed_properties=%s invalidated_properties=%s" % (interface_name, str(changed_properties), str(invalidated_properties)))
            if interface_name != WPAS_DBUS_P2P_PEER:
                return
            if "Groups" not in changed_properties:
                return
            if len(changed_properties["Groups"]) > 0:
                self.peer_group_added = True
            if len(changed_properties["Groups"]) == 0 and self.peer_group_added:
                self.peer_group_removed = True
                self.loop.quit()

        def run_test(self, *args):
            logger.debug("run_test")
            p2p.Find(dbus.Dictionary({'DiscoveryType': 'social'}))
            return False

        def success(self):
            return self.done and self.peer_group_added and self.peer_group_removed

    with TestDbusP2p(bus) as t:
        if not t.success():
            raise Exception("Expected signals not seen")

def test_dbus_p2p_group_idle_timeout(dev, apdev):
    """D-Bus P2P group removal on idle timeout"""
    try:
        dev[0].global_request("SET p2p_group_idle 1")
        _test_dbus_p2p_group_idle_timeout(dev, apdev)
    finally:
        dev[0].global_request("SET p2p_group_idle 0")

def _test_dbus_p2p_group_idle_timeout(dev, apdev):
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    p2p = dbus.Interface(if_obj, WPAS_DBUS_IFACE_P2PDEVICE)
    addr0 = dev[0].p2p_dev_addr()
    dev[1].p2p_listen()

    class TestDbusP2p(TestDbus):
        def __init__(self, bus):
            TestDbus.__init__(self, bus)
            self.done = False
            self.group_started = False
            self.peer_group_added = False
            self.peer_group_removed = False

        def __enter__(self):
            gobject.timeout_add(1, self.run_test)
            gobject.timeout_add(15000, self.timeout)
            self.add_signal(self.deviceFound, WPAS_DBUS_IFACE_P2PDEVICE,
                            "DeviceFound")
            self.add_signal(self.goNegotiationSuccess,
                            WPAS_DBUS_IFACE_P2PDEVICE,
                            "GONegotiationSuccess",
                            byte_arrays=True)
            self.add_signal(self.groupStarted, WPAS_DBUS_IFACE_P2PDEVICE,
                            "GroupStarted")
            self.add_signal(self.groupFinished, WPAS_DBUS_IFACE_P2PDEVICE,
                            "GroupFinished")
            self.add_signal(self.propertiesChanged, dbus.PROPERTIES_IFACE,
                            "PropertiesChanged")
            self.loop.run()
            return self

        def deviceFound(self, path):
            logger.debug("deviceFound: path=%s" % path)
            dev1 = WpaSupplicant('wlan1', '/tmp/wpas-wlan1')
            args = {'peer': path, 'wps_method': 'keypad', 'pin': '12345670',
                    'go_intent': 0}
            p2p.Connect(args)

            ev = dev1.wait_global_event(["P2P-GO-NEG-REQUEST"], timeout=15)
            if ev is None:
                raise Exception("Timeout while waiting for GO Neg Request")
            dev1.global_request("P2P_CONNECT " + addr0 + " 12345670 display go_intent=15")
            ev = dev1.wait_global_event(["P2P-GROUP-STARTED"], timeout=15)
            if ev is None:
                raise Exception("Group formation timed out")
            self.sta_group_ev = ev
            dev1.close_monitor_global()
            dev1.close_monitor_mon()
            dev1 = None

        def goNegotiationSuccess(self, properties):
            logger.debug("goNegotiationSuccess: properties=%s" % str(properties))

        def groupStarted(self, properties):
            logger.debug("groupStarted: " + str(properties))
            self.group_started = True
            g_if_obj = bus.get_object(WPAS_DBUS_SERVICE,
                                      properties['interface_object'])
            dev1 = WpaSupplicant('wlan1', '/tmp/wpas-wlan1', monitor=False)
            dev1.group_form_result(self.sta_group_ev)
            ifaddr = dev1.group_request("STA-FIRST").splitlines()[0]
            # Force disassociation with different reason code so that the
            # P2P Client using D-Bus does not get normal group termination event
            # from the GO.
            dev1.group_request("DEAUTHENTICATE " + ifaddr + " reason=0 test=0")
            dev1.remove_group()

        def groupFinished(self, properties):
            logger.debug("groupFinished: " + str(properties))
            self.done = True

        def propertiesChanged(self, interface_name, changed_properties,
                              invalidated_properties):
            logger.debug("propertiesChanged: interface_name=%s changed_properties=%s invalidated_properties=%s" % (interface_name, str(changed_properties), str(invalidated_properties)))
            if interface_name != WPAS_DBUS_P2P_PEER:
                return
            if not self.group_started:
                return
            if "Groups" not in changed_properties:
                return
            if len(changed_properties["Groups"]) > 0:
                self.peer_group_added = True
            if len(changed_properties["Groups"]) == 0:
                self.peer_group_removed = True
                self.loop.quit()

        def run_test(self, *args):
            logger.debug("run_test")
            p2p.Find(dbus.Dictionary({'DiscoveryType': 'social'}))
            return False

        def success(self):
            return self.done and self.peer_group_added and self.peer_group_removed

    with TestDbusP2p(bus) as t:
        if not t.success():
            raise Exception("Expected signals not seen")

def test_dbus_p2p_wps_failure(dev, apdev):
    """D-Bus P2P WPS failure"""
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    p2p = dbus.Interface(if_obj, WPAS_DBUS_IFACE_P2PDEVICE)
    addr0 = dev[0].p2p_dev_addr()

    class TestDbusP2p(TestDbus):
        def __init__(self, bus):
            TestDbus.__init__(self, bus)
            self.wps_failed = False
            self.formation_failure = False

        def __enter__(self):
            gobject.timeout_add(1, self.run_test)
            gobject.timeout_add(15000, self.timeout)
            self.add_signal(self.goNegotiationRequest,
                            WPAS_DBUS_IFACE_P2PDEVICE,
                            "GONegotiationRequest",
                            byte_arrays=True)
            self.add_signal(self.goNegotiationSuccess,
                            WPAS_DBUS_IFACE_P2PDEVICE,
                            "GONegotiationSuccess",
                            byte_arrays=True)
            self.add_signal(self.groupStarted, WPAS_DBUS_IFACE_P2PDEVICE,
                            "GroupStarted")
            self.add_signal(self.wpsFailed, WPAS_DBUS_IFACE_P2PDEVICE,
                            "WpsFailed")
            self.add_signal(self.groupFormationFailure,
                            WPAS_DBUS_IFACE_P2PDEVICE,
                            "GroupFormationFailure")
            self.loop.run()
            return self

        def goNegotiationRequest(self, path, dev_passwd_id, go_intent=0):
            logger.debug("goNegotiationRequest: path=%s dev_passwd_id=%d go_intent=%d" % (path, dev_passwd_id, go_intent))
            if dev_passwd_id != 1:
                raise Exception("Unexpected dev_passwd_id=%d" % dev_passwd_id)
            args = {'peer': path, 'wps_method': 'display', 'pin': '12345670',
                    'go_intent': 15}
            p2p.Connect(args)

        def goNegotiationSuccess(self, properties):
            logger.debug("goNegotiationSuccess: properties=%s" % str(properties))

        def groupStarted(self, properties):
            logger.debug("groupStarted: " + str(properties))
            raise Exception("Unexpected GroupStarted")

        def wpsFailed(self, name, args):
            logger.debug("wpsFailed - name=%s args=%s" % (name, str(args)))
            self.wps_failed = True
            if self.formation_failure:
                self.loop.quit()

        def groupFormationFailure(self, reason):
            logger.debug("groupFormationFailure - reason=%s" % reason)
            self.formation_failure = True
            if self.wps_failed:
                self.loop.quit()

        def run_test(self, *args):
            logger.debug("run_test")
            p2p.Listen(10)
            dev1 = WpaSupplicant('wlan1', '/tmp/wpas-wlan1')
            if not dev1.discover_peer(addr0):
                raise Exception("Peer not found")
            dev1.global_request("P2P_CONNECT " + addr0 + " 87654321 enter")
            return False

        def success(self):
            return self.wps_failed and self.formation_failure

    with TestDbusP2p(bus) as t:
        if not t.success():
            raise Exception("Expected signals not seen")

def test_dbus_p2p_two_groups(dev, apdev):
    """D-Bus P2P with two concurrent groups"""
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    p2p = dbus.Interface(if_obj, WPAS_DBUS_IFACE_P2PDEVICE)

    dev[0].request("SET p2p_no_group_iface 0")
    addr0 = dev[0].p2p_dev_addr()
    addr1 = dev[1].p2p_dev_addr()
    addr2 = dev[2].p2p_dev_addr()
    dev[1].p2p_start_go(freq=2412)
    dev1_group_ifname = dev[1].group_ifname

    class TestDbusP2p(TestDbus):
        def __init__(self, bus):
            TestDbus.__init__(self, bus)
            self.done = False
            self.peer = None
            self.go = None
            self.group1 = None
            self.group2 = None
            self.groups_removed = False

        def __enter__(self):
            gobject.timeout_add(1, self.run_test)
            gobject.timeout_add(15000, self.timeout)
            self.add_signal(self.propertiesChanged, dbus.PROPERTIES_IFACE,
                            "PropertiesChanged", byte_arrays=True)
            self.add_signal(self.deviceFound, WPAS_DBUS_IFACE_P2PDEVICE,
                            "DeviceFound")
            self.add_signal(self.groupStarted, WPAS_DBUS_IFACE_P2PDEVICE,
                            "GroupStarted")
            self.add_signal(self.groupFinished, WPAS_DBUS_IFACE_P2PDEVICE,
                            "GroupFinished")
            self.add_signal(self.peerJoined, WPAS_DBUS_GROUP,
                            "PeerJoined")
            self.loop.run()
            return self

        def propertiesChanged(self, interface_name, changed_properties,
                              invalidated_properties):
            logger.debug("propertiesChanged: interface_name=%s changed_properties=%s invalidated_properties=%s" % (interface_name, str(changed_properties), str(invalidated_properties)))

        def deviceFound(self, path):
            logger.debug("deviceFound: path=%s" % path)
            if addr2.replace(':', '') in path:
                self.peer = path
            elif addr1.replace(':', '') in path:
                self.go = path
            if self.go and not self.group1:
                logger.info("Join the group")
                p2p.StopFind()
                pin = '12345670'
                dev1 = WpaSupplicant('wlan1', '/tmp/wpas-wlan1')
                dev1.group_ifname = dev1_group_ifname
                dev1.group_request("WPS_PIN any " + pin)
                args = {'peer': self.go,
                        'join': True,
                        'wps_method': 'pin',
                        'pin': pin,
                        'frequency': 2412}
                p2p.Connect(args)

        def groupStarted(self, properties):
            logger.debug("groupStarted: " + str(properties))
            prop = if_obj.GetAll(WPAS_DBUS_IFACE_P2PDEVICE,
                                 dbus_interface=dbus.PROPERTIES_IFACE)
            logger.debug("p2pdevice properties: " + str(prop))

            g_obj = bus.get_object(WPAS_DBUS_SERVICE,
                                   properties['group_object'])
            res = g_obj.GetAll(WPAS_DBUS_GROUP,
                               dbus_interface=dbus.PROPERTIES_IFACE,
                               byte_arrays=True)
            logger.debug("Group properties: " + str(res))

            if not self.group1:
                self.group1 = properties['group_object']
                self.group1iface = properties['interface_object']
                self.g1_if_obj = bus.get_object(WPAS_DBUS_SERVICE,
                                                self.group1iface)

                logger.info("Start autonomous GO")
                params = dbus.Dictionary({'frequency': 2412})
                p2p.GroupAdd(params)
            elif not self.group2:
                self.group2 = properties['group_object']
                self.group2iface = properties['interface_object']
                self.g2_if_obj = bus.get_object(WPAS_DBUS_SERVICE,
                                                self.group2iface)
                self.g2_bssid = res['BSSID']

            if self.group1 and self.group2:
                logger.info("Authorize peer to join the group")
                a2 = binascii.unhexlify(addr2.replace(':', ''))
                params = {'Role': 'enrollee',
                          'P2PDeviceAddress': dbus.ByteArray(a2),
                          'Bssid': dbus.ByteArray(a2),
                          'Type': 'pin',
                          'Pin': '12345670'}
                g_wps = dbus.Interface(self.g2_if_obj, WPAS_DBUS_IFACE_WPS)
                g_wps.Start(params)

                bssid = ':'.join(["%02x" % i for i in struct.unpack('6B', self.g2_bssid)])
                dev2 = WpaSupplicant('wlan2', '/tmp/wpas-wlan2')
                dev2.scan_for_bss(bssid, freq=2412)
                dev2.global_request("P2P_CONNECT " + bssid + " 12345670 join freq=2412")
                ev = dev2.wait_global_event(["P2P-GROUP-STARTED"], timeout=15)
                if ev is None:
                    raise Exception("Group join timed out")
                self.dev2_group_ev = ev

        def groupFinished(self, properties):
            logger.debug("groupFinished: " + str(properties))

            if self.group1 == properties['group_object']:
                self.group1 = None
            elif self.group2 == properties['group_object']:
                self.group2 = None

            if not self.group1 and not self.group2:
                self.done = True
                self.loop.quit()

        def peerJoined(self, peer):
            logger.debug("peerJoined: " + peer)
            if self.groups_removed:
                return
            self.check_results()

            dev2 = WpaSupplicant('wlan2', '/tmp/wpas-wlan2')
            dev2.group_form_result(self.dev2_group_ev)
            dev2.remove_group()

            logger.info("Disconnect group2")
            group_p2p = dbus.Interface(self.g2_if_obj,
                                       WPAS_DBUS_IFACE_P2PDEVICE)
            group_p2p.Disconnect()

            logger.info("Disconnect group1")
            group_p2p = dbus.Interface(self.g1_if_obj,
                                       WPAS_DBUS_IFACE_P2PDEVICE)
            group_p2p.Disconnect()
            self.groups_removed = True

        def check_results(self):
            logger.info("Check results with two concurrent groups in operation")

            g1_obj = bus.get_object(WPAS_DBUS_SERVICE, self.group1)
            res1 = g1_obj.GetAll(WPAS_DBUS_GROUP,
                                 dbus_interface=dbus.PROPERTIES_IFACE,
                                 byte_arrays=True)

            g2_obj = bus.get_object(WPAS_DBUS_SERVICE, self.group2)
            res2 = g2_obj.GetAll(WPAS_DBUS_GROUP,
                                 dbus_interface=dbus.PROPERTIES_IFACE,
                                 byte_arrays=True)

            logger.info("group1 = " + self.group1)
            logger.debug("Group properties: " + str(res1))

            logger.info("group2 = " + self.group2)
            logger.debug("Group properties: " + str(res2))

            prop = if_obj.GetAll(WPAS_DBUS_IFACE_P2PDEVICE,
                                 dbus_interface=dbus.PROPERTIES_IFACE)
            logger.debug("p2pdevice properties: " + str(prop))

            if res1['Role'] != 'client':
                raise Exception("Group1 role reported incorrectly: " + res1['Role'])
            if res2['Role'] != 'GO':
                raise Exception("Group2 role reported incorrectly: " + res2['Role'])
            if prop['Role'] != 'device':
                raise Exception("p2pdevice role reported incorrectly: " + prop['Role'])

            if len(res2['Members']) != 1:
                   raise Exception("Unexpected Members value for group 2")

        def run_test(self, *args):
            logger.debug("run_test")
            p2p.Find(dbus.Dictionary({'DiscoveryType': 'social'}))
            return False

        def success(self):
            return self.done

    with TestDbusP2p(bus) as t:
        if not t.success():
            raise Exception("Expected signals not seen")

    dev[1].remove_group()

def test_dbus_p2p_cancel(dev, apdev):
    """D-Bus P2P Cancel"""
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    p2p = dbus.Interface(if_obj, WPAS_DBUS_IFACE_P2PDEVICE)
    try:
        p2p.Cancel()
        raise Exception("Unexpected p2p.Cancel() success")
    except dbus.exceptions.DBusException as e:
        pass

    addr0 = dev[0].p2p_dev_addr()
    dev[1].p2p_listen()

    class TestDbusP2p(TestDbus):
        def __init__(self, bus):
            TestDbus.__init__(self, bus)
            self.done = False

        def __enter__(self):
            gobject.timeout_add(1, self.run_test)
            gobject.timeout_add(15000, self.timeout)
            self.add_signal(self.deviceFound, WPAS_DBUS_IFACE_P2PDEVICE,
                            "DeviceFound")
            self.loop.run()
            return self

        def deviceFound(self, path):
            logger.debug("deviceFound: path=%s" % path)
            args = {'peer': path, 'wps_method': 'keypad', 'pin': '12345670',
                    'go_intent': 0}
            p2p.Connect(args)
            p2p.Cancel()
            self.done = True
            self.loop.quit()

        def run_test(self, *args):
            logger.debug("run_test")
            p2p.Find(dbus.Dictionary({'DiscoveryType': 'social'}))
            return False

        def success(self):
            return self.done

    with TestDbusP2p(bus) as t:
        if not t.success():
            raise Exception("Expected signals not seen")

def test_dbus_p2p_ip_addr(dev, apdev):
    """D-Bus P2P and IP address parameters"""
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    p2p = dbus.Interface(if_obj, WPAS_DBUS_IFACE_P2PDEVICE)

    vals = [("IpAddrGo", "192.168.43.1"),
            ("IpAddrMask", "255.255.255.0"),
            ("IpAddrStart", "192.168.43.100"),
            ("IpAddrEnd", "192.168.43.199")]
    for field, value in vals:
        if_obj.Set(WPAS_DBUS_IFACE, field, value,
                   dbus_interface=dbus.PROPERTIES_IFACE)
        val = if_obj.Get(WPAS_DBUS_IFACE, field,
                         dbus_interface=dbus.PROPERTIES_IFACE)
        if val != value:
            raise Exception("Unexpected %s value: %s" % (field, val))

    set_ip_addr_info(dev[1])

    dev[0].global_request("SET p2p_go_intent 0")

    req = dev[0].global_request("NFC_GET_HANDOVER_REQ NDEF P2P-CR").rstrip()
    if "FAIL" in req:
        raise Exception("Failed to generate NFC connection handover request")
    sel = dev[1].global_request("NFC_GET_HANDOVER_SEL NDEF P2P-CR").rstrip()
    if "FAIL" in sel:
        raise Exception("Failed to generate NFC connection handover select")
    dev[0].dump_monitor()
    dev[1].dump_monitor()
    res = dev[1].global_request("NFC_REPORT_HANDOVER RESP P2P " + req + " " + sel)
    if "FAIL" in res:
        raise Exception("Failed to report NFC connection handover to wpa_supplicant(resp)")
    res = dev[0].global_request("NFC_REPORT_HANDOVER INIT P2P " + req + " " + sel)
    if "FAIL" in res:
        raise Exception("Failed to report NFC connection handover to wpa_supplicant(init)")

    class TestDbusP2p(TestDbus):
        def __init__(self, bus):
            TestDbus.__init__(self, bus)
            self.done = False

        def __enter__(self):
            gobject.timeout_add(1, self.run_test)
            gobject.timeout_add(15000, self.timeout)
            self.add_signal(self.groupStarted, WPAS_DBUS_IFACE_P2PDEVICE,
                            "GroupStarted")
            self.loop.run()
            return self

        def groupStarted(self, properties):
            logger.debug("groupStarted: " + str(properties))
            self.loop.quit()

            if 'IpAddrGo' not in properties:
                logger.info("IpAddrGo missing from GroupStarted")
            ip_addr_go = properties['IpAddrGo']
            addr = "%d.%d.%d.%d" % (ip_addr_go[0], ip_addr_go[1], ip_addr_go[2], ip_addr_go[3])
            if addr != "192.168.42.1":
                logger.info("Unexpected IpAddrGo value: " + addr)
            self.done = True

        def run_test(self, *args):
            logger.debug("run_test")
            return False

        def success(self):
            return self.done

    with TestDbusP2p(bus) as t:
        if not t.success():
            raise Exception("Expected signals not seen")

def test_dbus_introspect(dev, apdev):
    """D-Bus introspection"""
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])

    res = if_obj.Introspect(WPAS_DBUS_IFACE,
                            dbus_interface=dbus.INTROSPECTABLE_IFACE)
    logger.info("Initial Introspect: " + str(res))
    if res is None or "Introspectable" not in res or "GroupStarted" not in res:
        raise Exception("Unexpected initial Introspect response: " + str(res))
    if "FastReauth" not in res or "PassiveScan" not in res:
        raise Exception("Unexpected initial Introspect response: " + str(res))

    with alloc_fail(dev[0], 1, "wpa_dbus_introspect"):
        res2 = if_obj.Introspect(WPAS_DBUS_IFACE,
                                 dbus_interface=dbus.INTROSPECTABLE_IFACE)
        logger.info("Introspect: " + str(res2))
        if res2 is not None:
            raise Exception("Unexpected Introspect response")

    with alloc_fail(dev[0], 1, "=add_interface;wpa_dbus_introspect"):
        res2 = if_obj.Introspect(WPAS_DBUS_IFACE,
                                 dbus_interface=dbus.INTROSPECTABLE_IFACE)
        logger.info("Introspect: " + str(res2))
        if res2 is None:
            raise Exception("No Introspect response")
        if len(res2) >= len(res):
            raise Exception("Unexpected Introspect response")

    with alloc_fail(dev[0], 1, "wpabuf_alloc;add_interface;wpa_dbus_introspect"):
        res2 = if_obj.Introspect(WPAS_DBUS_IFACE,
                                 dbus_interface=dbus.INTROSPECTABLE_IFACE)
        logger.info("Introspect: " + str(res2))
        if res2 is None:
            raise Exception("No Introspect response")
        if len(res2) >= len(res):
            raise Exception("Unexpected Introspect response")

    with alloc_fail(dev[0], 2, "=add_interface;wpa_dbus_introspect"):
        res2 = if_obj.Introspect(WPAS_DBUS_IFACE,
                                 dbus_interface=dbus.INTROSPECTABLE_IFACE)
        logger.info("Introspect: " + str(res2))
        if res2 is None:
            raise Exception("No Introspect response")
        if len(res2) >= len(res):
            raise Exception("Unexpected Introspect response")

def run_busctl(service, obj):
    if not shutil.which("busctl"):
        raise HwsimSkip("No busctl available")
    logger.info("busctl introspect %s %s" % (service, obj))
    cmd = subprocess.Popen(['busctl', 'introspect', service, obj],
                           stdout=subprocess.PIPE,
                           stderr=subprocess.PIPE)
    out = cmd.communicate()
    cmd.wait()
    logger.info("busctl stdout:\n%s" % out[0].strip())
    if len(out[1]) > 0:
        logger.info("busctl stderr: %s" % out[1].decode().strip())
    if "Duplicate property" in out[1].decode():
        raise Exception("Duplicate property")

def test_dbus_introspect_busctl(dev, apdev):
    """D-Bus introspection with busctl"""
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    ifaces = dbus_get(dbus, wpas_obj, "Interfaces")
    run_busctl(WPAS_DBUS_SERVICE, WPAS_DBUS_PATH)
    run_busctl(WPAS_DBUS_SERVICE, WPAS_DBUS_PATH + "/Interfaces")
    run_busctl(WPAS_DBUS_SERVICE, ifaces[0])

    hapd = hostapd.add_ap(apdev[0], {"ssid": "open"})
    bssid = apdev[0]['bssid']
    dev[0].scan_for_bss(bssid, freq=2412)
    id = dev[0].add_network()
    dev[0].set_network(id, "disabled", "0")
    dev[0].set_network_quoted(id, "ssid", "test")

    run_busctl(WPAS_DBUS_SERVICE, ifaces[0] + "/BSSs/0")
    run_busctl(WPAS_DBUS_SERVICE, ifaces[0] + "/Networks/0")

def test_dbus_ap(dev, apdev):
    """D-Bus AddNetwork for AP mode"""
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    iface = dbus.Interface(if_obj, WPAS_DBUS_IFACE)

    ssid = "test-wpa2-psk"
    passphrase = 'qwertyuiop'

    class TestDbusConnect(TestDbus):
        def __init__(self, bus):
            TestDbus.__init__(self, bus)
            self.started = False
            self.sta_added = False
            self.sta_removed = False
            self.authorized = False
            self.deauthorized = False
            self.stations = False

        def __enter__(self):
            gobject.timeout_add(1, self.run_connect)
            gobject.timeout_add(15000, self.timeout)
            self.add_signal(self.networkAdded, WPAS_DBUS_IFACE, "NetworkAdded")
            self.add_signal(self.networkSelected, WPAS_DBUS_IFACE,
                            "NetworkSelected")
            self.add_signal(self.propertiesChanged, WPAS_DBUS_IFACE,
                            "PropertiesChanged")
            self.add_signal(self.stationAdded, WPAS_DBUS_IFACE, "StationAdded")
            self.add_signal(self.stationRemoved, WPAS_DBUS_IFACE,
                            "StationRemoved")
            self.add_signal(self.staAuthorized, WPAS_DBUS_IFACE,
                            "StaAuthorized")
            self.add_signal(self.staDeauthorized, WPAS_DBUS_IFACE,
                            "StaDeauthorized")
            self.loop.run()
            return self

        def networkAdded(self, network, properties):
            logger.debug("networkAdded: %s" % str(network))
            logger.debug(str(properties))

        def networkSelected(self, network):
            logger.debug("networkSelected: %s" % str(network))
            self.network_selected = True

        def propertiesChanged(self, properties):
            logger.debug("propertiesChanged: %s" % str(properties))
            if 'State' in properties and properties['State'] == "completed":
                self.started = True
                dev1 = WpaSupplicant('wlan1', '/tmp/wpas-wlan1')
                dev1.connect(ssid, psk=passphrase, scan_freq="2412")

        def stationAdded(self, station, properties):
            logger.debug("stationAdded: %s" % str(station))
            logger.debug(str(properties))
            self.sta_added = True
            res = if_obj.Get(WPAS_DBUS_IFACE, 'Stations',
                             dbus_interface=dbus.PROPERTIES_IFACE)
            logger.info("Stations: " + str(res))
            if len(res) == 1:
                self.stations = True
            else:
                raise Exception("Missing Stations entry: " + str(res))

        def stationRemoved(self, station):
            logger.debug("stationRemoved: %s" % str(station))
            self.sta_removed = True
            res = if_obj.Get(WPAS_DBUS_IFACE, 'Stations',
                             dbus_interface=dbus.PROPERTIES_IFACE)
            logger.info("Stations: " + str(res))
            if len(res) != 0:
                self.stations = False
                raise Exception("Unexpected Stations entry: " + str(res))
            self.loop.quit()

        def staAuthorized(self, name):
            logger.debug("staAuthorized: " + name)
            self.authorized = True
            dev1 = WpaSupplicant('wlan1', '/tmp/wpas-wlan1')
            dev1.request("DISCONNECT")

        def staDeauthorized(self, name):
            logger.debug("staDeauthorized: " + name)
            self.deauthorized = True

        def run_connect(self, *args):
            logger.debug("run_connect")
            args = dbus.Dictionary({'ssid': ssid,
                                    'key_mgmt': 'WPA-PSK',
                                    'psk': passphrase,
                                    'mode': 2,
                                    'frequency': 2412,
                                    'scan_freq': 2412},
                                   signature='sv')
            self.netw = iface.AddNetwork(args)
            iface.SelectNetwork(self.netw)
            return False

        def success(self):
            return self.started and self.sta_added and self.sta_removed and \
                self.authorized and self.deauthorized

    with TestDbusConnect(bus) as t:
        if not t.success():
            raise Exception("Expected signals not seen")

def test_dbus_ap_scan(dev, apdev):
    """D-Bus AddNetwork for AP mode and scan"""
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    iface = dbus.Interface(if_obj, WPAS_DBUS_IFACE)

    ssid = "test-wpa2-psk"
    passphrase = 'qwertyuiop'

    hapd = hostapd.add_ap(apdev[0], {"ssid": "open"})
    bssid = hapd.own_addr()

    class TestDbusConnect(TestDbus):
        def __init__(self, bus):
            TestDbus.__init__(self, bus)
            self.started = False
            self.scan_completed = False

        def __enter__(self):
            gobject.timeout_add(1, self.run_connect)
            gobject.timeout_add(15000, self.timeout)
            self.add_signal(self.propertiesChanged, WPAS_DBUS_IFACE,
                            "PropertiesChanged")
            self.add_signal(self.scanDone, WPAS_DBUS_IFACE, "ScanDone")
            self.loop.run()
            return self

        def propertiesChanged(self, properties):
            logger.debug("propertiesChanged: %s" % str(properties))
            if 'State' in properties and properties['State'] == "completed":
                self.started = True
                logger.info("Try to scan in AP mode")
                iface.Scan({'Type': 'active',
                            'Channels': [(dbus.UInt32(2412), dbus.UInt32(20))]})
                logger.info("Scan() returned")

        def scanDone(self, success):
            logger.debug("scanDone: success=%s" % success)
            if self.started:
                self.scan_completed = True
                self.loop.quit()

        def run_connect(self, *args):
            logger.debug("run_connect")
            args = dbus.Dictionary({'ssid': ssid,
                                    'key_mgmt': 'WPA-PSK',
                                    'psk': passphrase,
                                    'mode': 2,
                                    'frequency': 2412,
                                    'scan_freq': 2412},
                                   signature='sv')
            self.netw = iface.AddNetwork(args)
            iface.SelectNetwork(self.netw)
            return False

        def success(self):
            return self.started and self.scan_completed

    with TestDbusConnect(bus) as t:
        if not t.success():
            raise Exception("Expected signals not seen")

def test_dbus_connect_wpa_eap(dev, apdev):
    """D-Bus AddNetwork and connection with WPA+WPA2-Enterprise AP"""
    skip_without_tkip(dev[0])
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    iface = dbus.Interface(if_obj, WPAS_DBUS_IFACE)

    ssid = "test-wpa-eap"
    params = hostapd.wpa_eap_params(ssid=ssid)
    params["wpa"] = "3"
    params["rsn_pairwise"] = "CCMP"
    hapd = hostapd.add_ap(apdev[0], params)

    class TestDbusConnect(TestDbus):
        def __init__(self, bus):
            TestDbus.__init__(self, bus)
            self.done = False

        def __enter__(self):
            gobject.timeout_add(1, self.run_connect)
            gobject.timeout_add(15000, self.timeout)
            self.add_signal(self.propertiesChanged, WPAS_DBUS_IFACE,
                            "PropertiesChanged")
            self.add_signal(self.eap, WPAS_DBUS_IFACE, "EAP")
            self.loop.run()
            return self

        def propertiesChanged(self, properties):
            logger.debug("propertiesChanged: %s" % str(properties))
            if 'State' in properties and properties['State'] == "completed":
                self.done = True
                self.loop.quit()

        def eap(self, status, parameter):
            logger.debug("EAP: status=%s parameter=%s" % (status, parameter))

        def run_connect(self, *args):
            logger.debug("run_connect")
            args = dbus.Dictionary({'ssid': ssid,
                                    'key_mgmt': 'WPA-EAP',
                                    'eap': 'PEAP',
                                    'identity': 'user',
                                    'password': 'password',
                                    'ca_cert': 'auth_serv/ca.pem',
                                    'phase2': 'auth=MSCHAPV2',
                                    'scan_freq': 2412},
                                   signature='sv')
            self.netw = iface.AddNetwork(args)
            iface.SelectNetwork(self.netw)
            return False

        def success(self):
            return self.done

    with TestDbusConnect(bus) as t:
        if not t.success():
            raise Exception("Expected signals not seen")

def test_dbus_ap_scan_2_ap_mode_scan(dev, apdev):
    """AP_SCAN 2 AP mode and D-Bus Scan()"""
    try:
        _test_dbus_ap_scan_2_ap_mode_scan(dev, apdev)
    finally:
        dev[0].request("AP_SCAN 1")

def _test_dbus_ap_scan_2_ap_mode_scan(dev, apdev):
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    iface = dbus.Interface(if_obj, WPAS_DBUS_IFACE)

    if "OK" not in dev[0].request("AP_SCAN 2"):
        raise Exception("Failed to set AP_SCAN 2")

    id = dev[0].add_network()
    dev[0].set_network(id, "mode", "2")
    dev[0].set_network_quoted(id, "ssid", "wpas-ap-open")
    dev[0].set_network(id, "key_mgmt", "NONE")
    dev[0].set_network(id, "frequency", "2412")
    dev[0].set_network(id, "scan_freq", "2412")
    dev[0].set_network(id, "disabled", "0")
    dev[0].select_network(id)
    ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED"], timeout=5)
    if ev is None:
        raise Exception("AP failed to start")

    with fail_test(dev[0], 1, "wpa_driver_nl80211_scan"):
        iface.Scan({'Type': 'active',
                    'AllowRoam': True,
                    'Channels': [(dbus.UInt32(2412), dbus.UInt32(20))]})
        ev = dev[0].wait_event(["CTRL-EVENT-SCAN-FAILED",
                                "AP-DISABLED"], timeout=5)
        if ev is None:
            raise Exception("CTRL-EVENT-SCAN-FAILED not seen")
        if "AP-DISABLED" in ev:
            raise Exception("Unexpected AP-DISABLED event")
        if "retry=1" in ev:
            # Wait for the retry to scan happen
            ev = dev[0].wait_event(["CTRL-EVENT-SCAN-FAILED",
                                    "AP-DISABLED"], timeout=5)
            if ev is None:
                raise Exception("CTRL-EVENT-SCAN-FAILED not seen - retry")
            if "AP-DISABLED" in ev:
                raise Exception("Unexpected AP-DISABLED event - retry")

    dev[1].connect("wpas-ap-open", key_mgmt="NONE", scan_freq="2412")
    dev[1].request("DISCONNECT")
    dev[1].wait_disconnected()
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

def test_dbus_expectdisconnect(dev, apdev):
    """D-Bus ExpectDisconnect"""
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    wpas = dbus.Interface(wpas_obj, WPAS_DBUS_SERVICE)

    params = {"ssid": "test-open"}
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].connect("test-open", key_mgmt="NONE", scan_freq="2412")

    # This does not really verify the behavior other than by going through the
    # code path for additional coverage.
    wpas.ExpectDisconnect()
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

def test_dbus_save_config(dev, apdev):
    """D-Bus SaveConfig"""
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    iface = dbus.Interface(if_obj, WPAS_DBUS_IFACE)
    try:
        iface.SaveConfig()
        raise Exception("SaveConfig() accepted unexpectedly")
    except dbus.exceptions.DBusException as e:
        if not str(e).startswith("fi.w1.wpa_supplicant1.UnknownError: Not allowed to update configuration"):
            raise Exception("Unexpected error message for SaveConfig(): " + str(e))

def test_dbus_vendor_elem(dev, apdev):
    """D-Bus vendor element operations"""
    try:
        _test_dbus_vendor_elem(dev, apdev)
    finally:
        dev[0].request("VENDOR_ELEM_REMOVE 1 *")

def _test_dbus_vendor_elem(dev, apdev):
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    iface = dbus.Interface(if_obj, WPAS_DBUS_IFACE)

    dev[0].request("VENDOR_ELEM_REMOVE 1 *")

    try:
        ie = dbus.ByteArray(b"\x00\x00")
        iface.VendorElemAdd(-1, ie)
        raise Exception("Invalid VendorElemAdd() accepted")
    except dbus.exceptions.DBusException as e:
        if "InvalidArgs" not in str(e) or "Invalid ID" not in str(e):
            raise Exception("Unexpected error message for invalid VendorElemAdd[1]: " + str(e))

    try:
        ie = dbus.ByteArray(b'')
        iface.VendorElemAdd(1, ie)
        raise Exception("Invalid VendorElemAdd() accepted")
    except dbus.exceptions.DBusException as e:
        if "InvalidArgs" not in str(e) or "Invalid value" not in str(e):
            raise Exception("Unexpected error message for invalid VendorElemAdd[2]: " + str(e))

    try:
        ie = dbus.ByteArray(b"\x00\x01")
        iface.VendorElemAdd(1, ie)
        raise Exception("Invalid VendorElemAdd() accepted")
    except dbus.exceptions.DBusException as e:
        if "InvalidArgs" not in str(e) or "Parse error" not in str(e):
            raise Exception("Unexpected error message for invalid VendorElemAdd[3]: " + str(e))

    try:
        iface.VendorElemGet(-1)
        raise Exception("Invalid VendorElemGet() accepted")
    except dbus.exceptions.DBusException as e:
        if "InvalidArgs" not in str(e) or "Invalid ID" not in str(e):
            raise Exception("Unexpected error message for invalid VendorElemGet[1]: " + str(e))

    try:
        iface.VendorElemGet(1)
        raise Exception("Invalid VendorElemGet() accepted")
    except dbus.exceptions.DBusException as e:
        if "InvalidArgs" not in str(e) or "ID value does not exist" not in str(e):
            raise Exception("Unexpected error message for invalid VendorElemGet[2]: " + str(e))

    try:
        ie = dbus.ByteArray(b"\x00\x00")
        iface.VendorElemRem(-1, ie)
        raise Exception("Invalid VendorElemRemove() accepted")
    except dbus.exceptions.DBusException as e:
        if "InvalidArgs" not in str(e) or "Invalid ID" not in str(e):
            raise Exception("Unexpected error message for invalid VendorElemRemove[1]: " + str(e))

    try:
        ie = dbus.ByteArray(b'')
        iface.VendorElemRem(1, ie)
        raise Exception("Invalid VendorElemRemove() accepted")
    except dbus.exceptions.DBusException as e:
        if "InvalidArgs" not in str(e) or "Invalid value" not in str(e):
            raise Exception("Unexpected error message for invalid VendorElemRemove[1]: " + str(e))

    iface.VendorElemRem(1, b"*")

    ie = dbus.ByteArray(b"\x00\x01\x00")
    iface.VendorElemAdd(1, ie)

    val = iface.VendorElemGet(1)
    if len(val) != len(ie):
        raise Exception("Unexpected VendorElemGet length")
    for i in range(len(val)):
        if val[i] != dbus.Byte(ie[i]):
            raise Exception("Unexpected VendorElemGet data")

    ie2 = dbus.ByteArray(b"\xe0\x00")
    iface.VendorElemAdd(1, ie2)

    ies = ie + ie2
    val = iface.VendorElemGet(1)
    if len(val) != len(ies):
        raise Exception("Unexpected VendorElemGet length[2]")
    for i in range(len(val)):
        if val[i] != dbus.Byte(ies[i]):
            raise Exception("Unexpected VendorElemGet data[2]")

    try:
        test_ie = dbus.ByteArray(b"\x01\x01")
        iface.VendorElemRem(1, test_ie)
        raise Exception("Invalid VendorElemRemove() accepted")
    except dbus.exceptions.DBusException as e:
        if "InvalidArgs" not in str(e) or "Parse error" not in str(e):
            raise Exception("Unexpected error message for invalid VendorElemRemove[1]: " + str(e))

    iface.VendorElemRem(1, ie)
    val = iface.VendorElemGet(1)
    if len(val) != len(ie2):
        raise Exception("Unexpected VendorElemGet length[3]")

    iface.VendorElemRem(1, b"*")
    try:
        iface.VendorElemGet(1)
        raise Exception("Invalid VendorElemGet() accepted after removal")
    except dbus.exceptions.DBusException as e:
        if "InvalidArgs" not in str(e) or "ID value does not exist" not in str(e):
            raise Exception("Unexpected error message for invalid VendorElemGet after removal: " + str(e))

def test_dbus_assoc_reject(dev, apdev):
    """D-Bus AssocStatusCode"""
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    iface = dbus.Interface(if_obj, WPAS_DBUS_IFACE)

    ssid = "test-open"
    params = {"ssid": ssid,
              "max_listen_interval": "1"}
    hapd = hostapd.add_ap(apdev[0], params)

    class TestDbusConnect(TestDbus):
        def __init__(self, bus):
            TestDbus.__init__(self, bus)
            self.assoc_status_seen = False
            self.state = 0

        def __enter__(self):
            gobject.timeout_add(1, self.run_connect)
            gobject.timeout_add(15000, self.timeout)
            self.add_signal(self.propertiesChanged, WPAS_DBUS_IFACE,
                            "PropertiesChanged")
            self.loop.run()
            return self

        def propertiesChanged(self, properties):
            logger.debug("propertiesChanged: %s" % str(properties))
            if 'AssocStatusCode' in properties:
                status = properties['AssocStatusCode']
                if status != 51:
                    logger.info("Unexpected status code: " + str(status))
                else:
                    self.assoc_status_seen = True
                iface.Disconnect()
                self.loop.quit()

        def run_connect(self, *args):
            args = dbus.Dictionary({'ssid': ssid,
                                    'key_mgmt': 'NONE',
                                    'scan_freq': 2412},
                                   signature='sv')
            self.netw = iface.AddNetwork(args)
            iface.SelectNetwork(self.netw)
            return False

        def success(self):
            return self.assoc_status_seen

    with TestDbusConnect(bus) as t:
        if not t.success():
            raise Exception("Expected signals not seen")

def test_dbus_mesh(dev, apdev):
    """D-Bus mesh"""
    check_mesh_support(dev[0])
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    mesh = dbus.Interface(if_obj, WPAS_DBUS_IFACE_MESH)

    add_open_mesh_network(dev[1])
    addr1 = dev[1].own_addr()

    class TestDbusMesh(TestDbus):
        def __init__(self, bus):
            TestDbus.__init__(self, bus)
            self.done = False

        def __enter__(self):
            gobject.timeout_add(1, self.run_test)
            gobject.timeout_add(15000, self.timeout)
            self.add_signal(self.meshGroupStarted, WPAS_DBUS_IFACE_MESH,
                            "MeshGroupStarted")
            self.add_signal(self.meshGroupRemoved, WPAS_DBUS_IFACE_MESH,
                            "MeshGroupRemoved")
            self.add_signal(self.meshPeerConnected, WPAS_DBUS_IFACE_MESH,
                            "MeshPeerConnected")
            self.add_signal(self.meshPeerDisconnected, WPAS_DBUS_IFACE_MESH,
                            "MeshPeerDisconnected")
            self.loop.run()
            return self

        def meshGroupStarted(self, args):
            logger.debug("MeshGroupStarted: " + str(args))

        def meshGroupRemoved(self, args):
            logger.debug("MeshGroupRemoved: " + str(args))
            self.done = True
            self.loop.quit()

        def meshPeerConnected(self, args):
            logger.debug("MeshPeerConnected: " + str(args))

            res = if_obj.Get(WPAS_DBUS_IFACE_MESH, 'MeshPeers',
                             dbus_interface=dbus.PROPERTIES_IFACE,
                             byte_arrays=True)
            logger.debug("MeshPeers: " + str(res))
            if len(res) != 1:
                raise Exception("Unexpected number of MeshPeer values")
            if binascii.hexlify(res[0]).decode() != addr1.replace(':', ''):
                raise Exception("Unexpected peer address")

            res = if_obj.Get(WPAS_DBUS_IFACE_MESH, 'MeshGroup',
                             dbus_interface=dbus.PROPERTIES_IFACE,
                             byte_arrays=True)
            logger.debug("MeshGroup: " + str(res))
            if res != b"wpas-mesh-open":
                raise Exception("Unexpected MeshGroup")
            dev1 = WpaSupplicant('wlan1', '/tmp/wpas-wlan1')
            dev1.mesh_group_remove()

        def meshPeerDisconnected(self, args):
            logger.debug("MeshPeerDisconnected: " + str(args))
            dev0 = WpaSupplicant('wlan0', '/tmp/wpas-wlan0')
            dev0.mesh_group_remove()

        def run_test(self, *args):
            logger.debug("run_test")
            dev0 = WpaSupplicant('wlan0', '/tmp/wpas-wlan0')
            add_open_mesh_network(dev0)
            return False

        def success(self):
            return self.done

    with TestDbusMesh(bus) as t:
        if not t.success():
            raise Exception("Expected signals not seen")

def test_dbus_roam(dev, apdev):
    """D-Bus Roam"""
    (bus, wpas_obj, path, if_obj) = prepare_dbus(dev[0])
    iface = dbus.Interface(if_obj, WPAS_DBUS_IFACE)

    ssid = "test-wpa2-psk"
    passphrase = 'qwertyuiop'
    params = hostapd.wpa2_params(ssid=ssid, passphrase=passphrase)
    hapd = hostapd.add_ap(apdev[0], params)
    hapd2 = hostapd.add_ap(apdev[1], params)
    bssid = apdev[0]['bssid']
    dev[0].scan_for_bss(bssid, freq=2412)
    bssid2 = apdev[1]['bssid']
    dev[0].scan_for_bss(bssid2, freq=2412)

    class TestDbusConnect(TestDbus):
        def __init__(self, bus):
            TestDbus.__init__(self, bus)
            self.state = 0

        def __enter__(self):
            gobject.timeout_add(1, self.run_connect)
            gobject.timeout_add(15000, self.timeout)
            self.add_signal(self.propertiesChanged, WPAS_DBUS_IFACE,
                            "PropertiesChanged")
            self.loop.run()
            return self

        def propertiesChanged(self, properties):
            logger.debug("propertiesChanged: %s" % str(properties))
            if 'State' in properties and properties['State'] == "completed":
                if self.state == 0:
                    self.state = 1
                    cur = properties["CurrentBSS"]
                    bss_obj = bus.get_object(WPAS_DBUS_SERVICE, cur)
                    res = bss_obj.Get(WPAS_DBUS_BSS, 'BSSID',
                                      dbus_interface=dbus.PROPERTIES_IFACE)
                    bssid_str = ''
                    for item in res:
                        if len(bssid_str) > 0:
                            bssid_str += ':'
                        bssid_str += '%02x' % item
                    dst = bssid if bssid_str == bssid2 else bssid2
                    iface.Roam(dst)
                elif self.state == 1:
                    if "RoamComplete" in properties and \
                       properties["RoamComplete"]:
                        self.state = 2
                        self.loop.quit()

        def run_connect(self, *args):
            logger.debug("run_connect")
            args = dbus.Dictionary({'ssid': ssid,
                                    'key_mgmt': 'WPA-PSK',
                                    'psk': passphrase,
                                    'scan_freq': 2412},
                                   signature='sv')
            self.netw = iface.AddNetwork(args)
            iface.SelectNetwork(self.netw)
            return False

        def success(self):
            return self.state == 2

    with TestDbusConnect(bus) as t:
        if not t.success():
            raise Exception("Expected signals not seen")
