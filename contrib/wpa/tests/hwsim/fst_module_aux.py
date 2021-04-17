# FST tests related classes
# Copyright (c) 2015, Qualcomm Atheros, Inc.
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

import logging
import os
import signal
import time
import re

import hostapd
import wpaspy
import utils
from wpasupplicant import WpaSupplicant

import fst_test_common

logger = logging.getLogger()

def parse_fst_iface_event(ev):
    """Parses FST iface event that comes as a string, e.g.
    "<3>FST-EVENT-IFACE attached ifname=wlan9 group=fstg0"
    Returns a dictionary with parsed "event_type", "ifname", and "group"; or
    None if not an FST event or can't be parsed."""
    event = {}
    if ev.find("FST-EVENT-IFACE") == -1:
        return None
    if ev.find("attached") != -1:
        event['event_type'] = 'attached'
    elif ev.find("detached") != -1:
        event['event_type'] = 'detached'
    else:
        return None
    f = re.search("ifname=(\S+)", ev)
    if f is not None:
        event['ifname'] = f.group(1)
    f = re.search("group=(\S+)", ev)
    if f is not None:
        event['group'] = f.group(1)
    return event

def parse_fst_session_event(ev):
    """Parses FST session event that comes as a string, e.g.
    "<3>FST-EVENT-SESSION event_type=EVENT_FST_SESSION_STATE session_id=0 reason=REASON_STT"
    Returns a dictionary with parsed "type", "id", and "reason"; or None if not
    a FST event or can't be parsed"""
    event = {}
    if ev.find("FST-EVENT-SESSION") == -1:
        return None
    event['new_state'] = '' # The field always exists in the dictionary
    f = re.search("event_type=(\S+)", ev)
    if f is None:
        return None
    event['type'] = f.group(1)
    f = re.search("session_id=(\d+)", ev)
    if f is not None:
        event['id'] = f.group(1)
    f = re.search("old_state=(\S+)", ev)
    if f is not None:
        event['old_state'] = f.group(1)
    f = re.search("new_state=(\S+)", ev)
    if f is not None:
        event['new_state'] = f.group(1)
    f = re.search("reason=(\S+)", ev)
    if f is not None:
        event['reason'] = f.group(1)
    return event

def start_two_ap_sta_pairs(apdev, rsn=False):
    """auxiliary function that creates two pairs of APs and STAs"""
    ap1 = FstAP(apdev[0]['ifname'], 'fst_11a', 'a',
                fst_test_common.fst_test_def_chan_a,
                fst_test_common.fst_test_def_group,
                fst_test_common.fst_test_def_prio_low,
                fst_test_common.fst_test_def_llt, rsn=rsn)
    ap1.start()
    ap2 = FstAP(apdev[1]['ifname'], 'fst_11g', 'g',
                fst_test_common.fst_test_def_chan_g,
                fst_test_common.fst_test_def_group,
                fst_test_common.fst_test_def_prio_high,
                fst_test_common.fst_test_def_llt, rsn=rsn)
    ap2.start()

    sta1 = FstSTA('wlan5',
                  fst_test_common.fst_test_def_group,
                  fst_test_common.fst_test_def_prio_low,
                  fst_test_common.fst_test_def_llt, rsn=rsn)
    sta1.start()
    sta2 = FstSTA('wlan6',
                  fst_test_common.fst_test_def_group,
                  fst_test_common.fst_test_def_prio_high,
                  fst_test_common.fst_test_def_llt, rsn=rsn)
    sta2.start()

    return ap1, ap2, sta1, sta2

def stop_two_ap_sta_pairs(ap1, ap2, sta1, sta2):
    sta1.stop()
    sta2.stop()
    ap1.stop()
    ap2.stop()
    fst_test_common.fst_clear_regdom()

def connect_two_ap_sta_pairs(ap1, ap2, dev1, dev2, rsn=False):
    """Connects a pair of stations, each one to a separate AP"""
    dev1.scan(freq=fst_test_common.fst_test_def_freq_a)
    dev2.scan(freq=fst_test_common.fst_test_def_freq_g)

    if rsn:
        dev1.connect(ap1, psk="12345678",
                     scan_freq=fst_test_common.fst_test_def_freq_a)
        dev2.connect(ap2, psk="12345678",
                     scan_freq=fst_test_common.fst_test_def_freq_g)
    else:
        dev1.connect(ap1, key_mgmt="NONE",
                     scan_freq=fst_test_common.fst_test_def_freq_a)
        dev2.connect(ap2, key_mgmt="NONE",
                     scan_freq=fst_test_common.fst_test_def_freq_g)

def disconnect_two_ap_sta_pairs(ap1, ap2, dev1, dev2):
    dev1.disconnect()
    dev2.disconnect()

def external_sta_connect(sta, ap, **kwargs):
    """Connects the external station to the given AP"""
    if not isinstance(sta, WpaSupplicant):
        raise Exception("Bad STA object")
    if not isinstance(ap, FstAP):
        raise Exception("Bad AP object to connect to")
    hap = ap.get_instance()
    sta.connect(ap.get_ssid(), **kwargs)

def disconnect_external_sta(sta, ap, check_disconnect=True):
    """Disconnects the external station from the AP"""
    if not isinstance(sta, WpaSupplicant):
        raise Exception("Bad STA object")
    if not isinstance(ap, FstAP):
        raise Exception("Bad AP object to connect to")
    sta.request("DISCONNECT")
    if check_disconnect:
        hap = ap.get_instance()
        ev = hap.wait_event(["AP-STA-DISCONNECTED"], timeout=10)
        if ev is None:
            raise Exception("No disconnection event received from %s" % ap.get_ssid())

#
# FstDevice class
# This is the parent class for the AP (FstAP) and STA (FstSTA) that implements
# FST functionality.
#
class FstDevice:
    def __init__(self, iface, fst_group, fst_pri, fst_llt=None, rsn=False):
        self.iface = iface
        self.fst_group = fst_group
        self.fst_pri = fst_pri
        self.fst_llt = fst_llt  # None llt means no llt parameter will be set
        self.instance = None    # Hostapd/WpaSupplicant instance
        self.peer_obj = None    # Peer object, must be a FstDevice child object
        self.new_peer_addr = None # Peer MAC address for new session iface
        self.old_peer_addr = None # Peer MAC address for old session iface
        self.role = 'initiator' # Role: initiator/responder
        s = self.grequest("FST-MANAGER TEST_REQUEST IS_SUPPORTED")
        if not s.startswith('OK'):
            raise utils.HwsimSkip("FST not supported")
        self.rsn = rsn

    def ifname(self):
        return self.iface

    def get_instance(self):
        """Gets the Hostapd/WpaSupplicant instance"""
        raise Exception("Virtual get_instance() called!")

    def get_own_mac_address(self):
        """Gets the device's own MAC address"""
        raise Exception("Virtual get_own_mac_address() called!")

    def get_new_peer_addr(self):
        return self.new_peer_addr

    def get_old_peer_addr(self):
        return self.old_peer_addr

    def get_actual_peer_addr(self):
        """Gets the peer address. A connected AP/station address is returned."""
        raise Exception("Virtual get_actual_peer_addr() called!")

    def grequest(self, req):
        """Send request on the global control interface"""
        raise Exception("Virtual grequest() called!")

    def wait_gevent(self, events, timeout=None):
        """Wait for a list of events on the global interface"""
        raise Exception("Virtual wait_gevent() called!")

    def request(self, req):
        """Issue a request to the control interface"""
        h = self.get_instance()
        return h.request(req)

    def wait_event(self, events, timeout=None):
        """Wait for an event from the control interface"""
        h = self.get_instance()
        if timeout is not None:
            return h.wait_event(events, timeout=timeout)
        else:
            return h.wait_event(events)

    def set_old_peer_addr(self, peer_addr=None):
        """Sets the peer address"""
        if peer_addr is not None:
            self.old_peer_addr = peer_addr
        else:
            self.old_peer_addr = self.get_actual_peer_addr()

    def set_new_peer_addr(self, peer_addr=None):
        """Sets the peer address"""
        if peer_addr is not None:
            self.new_peer_addr = peer_addr
        else:
            self.new_peer_addr = self.get_actual_peer_addr()

    def add_peer(self, obj, old_peer_addr=None, new_peer_addr=None):
        """Add peer for FST session(s). 'obj' is a FstDevice subclass object.
        The method must be called before add_session().
        If peer_addr is not specified, the address of the currently connected
        station is used."""
        if not isinstance(obj, FstDevice):
            raise Exception("Peer must be a FstDevice object")
        self.peer_obj = obj
        self.set_old_peer_addr(old_peer_addr)
        self.set_new_peer_addr(new_peer_addr)

    def get_peer(self):
        """Returns peer object"""
        return self.peer_obj

    def set_fst_parameters(self, group_id=None, pri=None, llt=None):
        """Change/set new FST parameters. Can be used to start FST sessions with
        different FST parameters than defined in the configuration file."""
        if group_id is not None:
            self.fst_group = group_id
        if pri is not None:
            self.fst_pri = pri
        if llt is not None:
            self.fst_llt = llt

    def get_local_mbies(self, ifname=None):
        if_name = ifname if ifname is not None else self.iface
        return self.grequest("FST-MANAGER TEST_REQUEST GET_LOCAL_MBIES " + if_name)

    def add_session(self):
        """Adds an FST session. add_peer() must be called calling this
        function"""
        if self.peer_obj is None:
            raise Exception("Peer wasn't added before starting session")
        self.dump_monitor()
        grp = ' ' + self.fst_group if self.fst_group != '' else ''
        sid = self.grequest("FST-MANAGER SESSION_ADD" + grp)
        sid = sid.strip()
        if sid.startswith("FAIL"):
            raise Exception("Cannot add FST session with groupid ==" + grp)
        self.dump_monitor()
        return sid

    def set_session_param(self, params):
        request = "FST-MANAGER SESSION_SET"
        if params is not None and params != '':
            request = request + ' ' + params
        return self.grequest(request)

    def get_session_params(self, sid):
        request = "FST-MANAGER SESSION_GET " + sid
        res = self.grequest(request)
        if res.startswith("FAIL"):
            return None
        params = {}
        for i in res.splitlines():
            p = i.split('=')
            params[p[0]] = p[1]
        return params

    def iface_peers(self, ifname):
        grp = self.fst_group if self.fst_group != '' else ''
        res = self.grequest("FST-MANAGER IFACE_PEERS " + grp + ' ' + ifname)
        if res.startswith("FAIL"):
            return None
        return res.splitlines()

    def get_peer_mbies(self, ifname, peer_addr):
        return self.grequest("FST-MANAGER GET_PEER_MBIES %s %s" % (ifname, peer_addr))

    def list_ifaces(self):
        grp = self.fst_group if self.fst_group != '' else ''
        res = self.grequest("FST-MANAGER LIST_IFACES " + grp)
        if res.startswith("FAIL"):
            return None
        ifaces = []
        for i in res.splitlines():
            p = i.split(':')
            iface = {}
            iface['name'] = p[0]
            iface['priority'] = p[1]
            iface['llt'] = p[2]
            ifaces.append(iface)
        return ifaces

    def list_groups(self):
        res = self.grequest("FST-MANAGER LIST_GROUPS")
        if res.startswith("FAIL"):
            return None
        return res.splitlines()

    def configure_session(self, sid, new_iface, old_iface=None):
        """Calls session_set for a number of parameters some of which are stored
        in "self" while others are passed to this function explicitly. If
        old_iface is None, current iface is used; if old_iface is an empty
        string."""
        self.dump_monitor()
        oldiface = old_iface if old_iface is not None else self.iface
        s = self.set_session_param(sid + ' old_ifname=' + oldiface)
        if not s.startswith("OK"):
            raise Exception("Cannot set FST session old_ifname: " + s)
        if new_iface is not None:
            s = self.set_session_param(sid + " new_ifname=" + new_iface)
            if not s.startswith("OK"):
                raise Exception("Cannot set FST session new_ifname:" + s)
        if self.new_peer_addr is not None and self.new_peer_addr != '':
            s = self.set_session_param(sid + " new_peer_addr=" + self.new_peer_addr)
            if not s.startswith("OK"):
                raise Exception("Cannot set FST session peer address:" + s + " (new)")
        if self.old_peer_addr is not None and self.old_peer_addr != '':
            s = self.set_session_param(sid + " old_peer_addr=" + self.old_peer_addr)
            if not s.startswith("OK"):
                raise Exception("Cannot set FST session peer address:" + s + " (old)")
        if self.fst_llt is not None and self.fst_llt != '':
            s = self.set_session_param(sid + " llt=" + self.fst_llt)
            if not s.startswith("OK"):
                raise Exception("Cannot set FST session llt:" + s)
        self.dump_monitor()

    def send_iface_attach_request(self, ifname, group, llt, priority):
        request = "FST-ATTACH " + ifname + ' ' + group
        if llt is not None:
            request += " llt=" + llt
        if priority is not None:
            request += " priority=" + priority
        res = self.grequest(request)
        if not res.startswith("OK"):
            raise Exception("Cannot attach FST iface: " + res)

    def send_iface_detach_request(self, ifname):
        res = self.grequest("FST-DETACH " + ifname)
        if not res.startswith("OK"):
            raise Exception("Cannot detach FST iface: " + res)

    def send_session_setup_request(self, sid):
        s = self.grequest("FST-MANAGER SESSION_INITIATE " + sid)
        if not s.startswith('OK'):
            raise Exception("Cannot send setup request: %s" % s)
        return s

    def send_session_setup_response(self, sid, response):
        request = "FST-MANAGER SESSION_RESPOND " + sid + " " + response
        s = self.grequest(request)
        if not s.startswith('OK'):
            raise Exception("Cannot send setup response: %s" % s)
        return s

    def send_test_session_setup_request(self, fsts_id,
                                        additional_parameter=None):
        request = "FST-MANAGER TEST_REQUEST SEND_SETUP_REQUEST " + fsts_id
        if additional_parameter is not None:
            request += " " + additional_parameter
        s = self.grequest(request)
        if not s.startswith('OK'):
            raise Exception("Cannot send FST setup request: %s" % s)
        return s

    def send_test_session_setup_response(self, fsts_id,
                                         response, additional_parameter=None):
        request = "FST-MANAGER TEST_REQUEST SEND_SETUP_RESPONSE " + fsts_id + " " + response
        if additional_parameter is not None:
            request += " " + additional_parameter
        s = self.grequest(request)
        if not s.startswith('OK'):
            raise Exception("Cannot send FST setup response: %s" % s)
        return s

    def send_test_ack_request(self, fsts_id):
        s = self.grequest("FST-MANAGER TEST_REQUEST SEND_ACK_REQUEST " + fsts_id)
        if not s.startswith('OK'):
            raise Exception("Cannot send FST ack request: %s" % s)
        return s

    def send_test_ack_response(self, fsts_id):
        s = self.grequest("FST-MANAGER TEST_REQUEST SEND_ACK_RESPONSE " + fsts_id)
        if not s.startswith('OK'):
            raise Exception("Cannot send FST ack response: %s" % s)
        return s

    def send_test_tear_down(self, fsts_id):
        s = self.grequest("FST-MANAGER TEST_REQUEST SEND_TEAR_DOWN " + fsts_id)
        if not s.startswith('OK'):
            raise Exception("Cannot send FST tear down: %s" % s)
        return s

    def get_fsts_id_by_sid(self, sid):
        s = self.grequest("FST-MANAGER TEST_REQUEST GET_FSTS_ID " + sid)
        if s == ' ' or s.startswith('FAIL'):
            raise Exception("Cannot get fsts_id for sid == %s" % sid)
        return int(s)

    def wait_for_iface_event(self, timeout):
        while True:
            ev = self.wait_gevent(["FST-EVENT-IFACE"], timeout)
            if ev is None:
                raise Exception("No FST-EVENT-IFACE received")
            event = parse_fst_iface_event(ev)
            if event is None:
                # We can't parse so it's not our event, wait for next one
                continue
            return event

    def wait_for_session_event(self, timeout, events_to_ignore=[],
                               events_to_count=[]):
        while True:
            ev = self.wait_gevent(["FST-EVENT-SESSION"], timeout)
            if ev is None:
                raise Exception("No FST-EVENT-SESSION received")
            event = parse_fst_session_event(ev)
            if event is None:
                # We can't parse so it's not our event, wait for next one
                continue
            if len(events_to_ignore) > 0:
                if event['type'] in events_to_ignore:
                    continue
            elif len(events_to_count) > 0:
                if event['type'] not in events_to_count:
                    continue
            return event

    def initiate_session(self, sid, response="accept"):
        """Initiates FST session with given session id 'sid'.
        'response' is the session respond answer: "accept", "reject", or a
        special "timeout" value to skip the response in order to test session
        timeouts.
        Returns: "OK" - session has been initiated, otherwise the reason for the
        reset: REASON_REJECT, REASON_STT."""
        strsid = ' ' + sid if sid != '' else ''
        s = self.grequest("FST-MANAGER SESSION_INITIATE"+ strsid)
        if not s.startswith('OK'):
            raise Exception("Cannot initiate fst session: %s" % s)
        ev = self.peer_obj.wait_gevent(["FST-EVENT-SESSION"], timeout=5)
        if ev is None:
            raise Exception("No FST-EVENT-SESSION received")
        # We got FST event
        event = parse_fst_session_event(ev)
        if event == None:
            raise Exception("Unrecognized FST event: " % ev)
        if event['type'] != 'EVENT_FST_SETUP':
            raise Exception("Expected FST_SETUP event, got: " + event['type'])
        ev = self.peer_obj.wait_gevent(["FST-EVENT-SESSION"], timeout=5)
        if ev is None:
            raise Exception("No FST-EVENT-SESSION received")
        event = parse_fst_session_event(ev)
        if event == None:
            raise Exception("Unrecognized FST event: " % ev)
        if event['type'] != 'EVENT_FST_SESSION_STATE':
            raise Exception("Expected EVENT_FST_SESSION_STATE event, got: " + event['type'])
        if event['new_state'] != "SETUP_COMPLETION":
            raise Exception("Expected new state SETUP_COMPLETION, got: " + event['new_state'])
        if response == '':
            return 'OK'
        if response != "timeout":
            s = self.peer_obj.grequest("FST-MANAGER SESSION_RESPOND "+ event['id'] + " " + response)  # Or reject
            if not s.startswith('OK'):
                raise Exception("Error session_respond: %s" % s)
        # Wait for EVENT_FST_SESSION_STATE events. We should get at least 2
        # events. The 1st event will be EVENT_FST_SESSION_STATE
        # old_state=INITIAL new_state=SETUP_COMPLETED. The 2nd event will be
        # either EVENT_FST_ESTABLISHED with the session id or
        # EVENT_FST_SESSION_STATE with new_state=INITIAL if the session was
        # reset, the reason field will tell why.
        result = ''
        while result == '':
            ev = self.wait_gevent(["FST-EVENT-SESSION"], timeout=5)
            if ev is None:
                break # No session event received
            event = parse_fst_session_event(ev)
            if event == None:
                # We can't parse so it's not our event, wait for next one
                continue
            if event['type'] == 'EVENT_FST_ESTABLISHED':
                result = "OK"
                break
            elif event['type'] == "EVENT_FST_SESSION_STATE":
                if event['new_state'] == "INITIAL":
                    # Session was reset, the only reason to get back to initial
                    # state.
                    result = event['reason']
                    break
        if result == '':
            raise Exception("No event for session respond")
        return result

    def transfer_session(self, sid):
        """Transfers the session. 'sid' is the session id. 'hsta' is the
        station-responder object.
        Returns: REASON_SWITCH - the session has been transferred successfully
        or a REASON_... reported by the reset event."""
        request = "FST-MANAGER SESSION_TRANSFER"
        self.dump_monitor()
        if sid != '':
            request += ' ' + sid
        s = self.grequest(request)
        if not s.startswith('OK'):
            raise Exception("Cannot transfer fst session: %s" % s)
        result = ''
        while result == '':
            ev = self.peer_obj.wait_gevent(["FST-EVENT-SESSION"], timeout=5)
            if ev is None:
                raise Exception("Missing session transfer event")
            # We got FST event. We expect TRANSITION_CONFIRMED state and then
            # INITIAL (reset) with the reason (e.g. "REASON_SWITCH").
            # Right now we'll be waiting for the reset event and record the
            # reason.
            event = parse_fst_session_event(ev)
            if event == None:
                raise Exception("Unrecognized FST event: " % ev)
            if event['new_state'] == 'INITIAL':
                result = event['reason']
        self.dump_monitor()
        return result

    def wait_for_tear_down(self):
        ev = self.wait_gevent(["FST-EVENT-SESSION"], timeout=5)
        if ev is None:
            raise Exception("No FST-EVENT-SESSION received")
        # We got FST event
        event = parse_fst_session_event(ev)
        if event == None:
            raise Exception("Unrecognized FST event: " % ev)
        if event['type'] != 'EVENT_FST_SESSION_STATE':
            raise Exception("Expected EVENT_FST_SESSION_STATE event, got: " + event['type'])
        if event['new_state'] != "INITIAL":
            raise Exception("Expected new state INITIAL, got: " + event['new_state'])
        if event['reason'] != 'REASON_TEARDOWN':
            raise Exception("Expected reason REASON_TEARDOWN, got: " + event['reason'])

    def teardown_session(self, sid):
        """Tears down FST session with a given session id ('sid')"""
        strsid = ' ' + sid if sid != '' else ''
        s = self.grequest("FST-MANAGER SESSION_TEARDOWN" + strsid)
        if not s.startswith('OK'):
            raise Exception("Cannot tear down fst session: %s" % s)
        self.peer_obj.wait_for_tear_down()


    def remove_session(self, sid, wait_for_tear_down=True):
        """Removes FST session with a given session id ('sid')"""
        strsid = ' ' + sid if sid != '' else ''
        s = self.grequest("FST-MANAGER SESSION_REMOVE" + strsid)
        if not s.startswith('OK'):
            raise Exception("Cannot remove fst session: %s" % s)
        if wait_for_tear_down == True:
            self.peer_obj.wait_for_tear_down()

    def remove_all_sessions(self):
        """Removes FST session with a given session id ('sid')"""
        grp = ' ' + self.fst_group if self.fst_group != '' else ''
        s = self.grequest("FST-MANAGER LIST_SESSIONS" + grp)
        if not s.startswith('FAIL'):
            for sid in s.splitlines():
                sid = sid.strip()
                if len(sid) != 0:
                    self.remove_session(sid, wait_for_tear_down=False)


#
# FstAP class
#
class FstAP(FstDevice):
    def __init__(self, iface, ssid, mode, chan, fst_group, fst_pri,
                 fst_llt=None, rsn=False):
        """If fst_group is empty, then FST parameters will not be set
        If fst_llt is empty, the parameter will not be set and the default value
        is expected to be configured."""
        self.ssid = ssid
        self.mode = mode
        self.chan = chan
        self.reg_ctrl = fst_test_common.HapdRegCtrl()
        self.reg_ctrl.add_ap(iface, self.chan)
        self.global_instance = hostapd.HostapdGlobal()
        FstDevice.__init__(self, iface, fst_group, fst_pri, fst_llt, rsn)

    def start(self, return_early=False):
        """Starts AP the "standard" way as it was intended by hostapd tests.
        This will work only when FST supports fully dynamically loading
        parameters in hostapd."""
        params = {}
        params['ssid'] = self.ssid
        params['hw_mode'] = self.mode
        params['channel'] = self.chan
        params['country_code'] = 'US'
        if self.rsn:
            params['wpa'] = '2'
            params['wpa_key_mgmt'] = 'WPA-PSK'
            params['rsn_pairwise'] = 'CCMP'
            params['wpa_passphrase'] = '12345678'
        self.hapd = hostapd.add_ap(self.iface, params)
        if not self.hapd.ping():
            raise Exception("Could not ping FST hostapd")
        self.reg_ctrl.start()
        self.get_global_instance()
        if return_early:
            return self.hapd
        if len(self.fst_group) != 0:
            self.send_iface_attach_request(self.iface, self.fst_group,
                                           self.fst_llt, self.fst_pri)
        return self.hapd

    def stop(self):
        """Removes the AP, To be used when dynamic fst APs are implemented in
        hostapd."""
        if len(self.fst_group) != 0:
            self.remove_all_sessions()
            try:
                self.send_iface_detach_request(self.iface)
            except Exception as e:
                logger.info(str(e))
        self.reg_ctrl.stop()
        del self.global_instance
        self.global_instance = None

    def get_instance(self):
        """Return the Hostapd/WpaSupplicant instance"""
        if self.instance is None:
            self.instance = hostapd.Hostapd(self.iface)
        return self.instance

    def get_global_instance(self):
        return self.global_instance

    def get_own_mac_address(self):
        """Gets the device's own MAC address"""
        h = self.get_instance()
        status = h.get_status()
        return status['bssid[0]']

    def get_actual_peer_addr(self):
        """Gets the peer address. A connected station address is returned."""
        # Use the device instance, the global control interface doesn't have
        # station address
        h = self.get_instance()
        sta = h.get_sta(None)
        if sta is None or 'addr' not in sta:
            # Maybe station is not connected?
            addr = None
        else:
            addr = sta['addr']
        return addr

    def grequest(self, req):
        """Send request on the global control interface"""
        logger.debug("FstAP::grequest: " + req)
        h = self.get_global_instance()
        return h.request(req)

    def wait_gevent(self, events, timeout=None):
        """Wait for a list of events on the global interface"""
        h = self.get_global_instance()
        if timeout is not None:
            return h.wait_event(events, timeout=timeout)
        else:
            return h.wait_event(events)

    def get_ssid(self):
        return self.ssid

    def dump_monitor(self):
        """Dump control interface monitor events"""
        if self.instance:
            self.instance.dump_monitor()

#
# FstSTA class
#
class FstSTA(FstDevice):
    def __init__(self, iface, fst_group, fst_pri, fst_llt=None, rsn=False):
        """If fst_group is empty, then FST parameters will not be set
        If fst_llt is empty, the parameter will not be set and the default value
        is expected to be configured."""
        FstDevice.__init__(self, iface, fst_group, fst_pri, fst_llt, rsn)
        self.connected = None # FstAP object the station is connected to

    def start(self):
        """Current implementation involves running another instance of
        wpa_supplicant with fixed FST STAs configurations. When any type of
        dynamic STA loading is implemented, rewrite the function similarly to
        FstAP."""
        h = self.get_instance()
        h.interface_add(self.iface, drv_params="force_connect_cmd=1")
        if not h.global_ping():
            raise Exception("Could not ping FST wpa_supplicant")
        if len(self.fst_group) != 0:
            self.send_iface_attach_request(self.iface, self.fst_group,
                                           self.fst_llt, self.fst_pri)
        return None

    def stop(self):
        """Removes the STA. In a static (temporary) implementation does nothing,
        the STA will be removed when the fst wpa_supplicant process is killed by
        fstap.cleanup()."""
        h = self.get_instance()
        h.dump_monitor()
        if len(self.fst_group) != 0:
            self.remove_all_sessions()
            self.send_iface_detach_request(self.iface)
            h.dump_monitor()
        h.interface_remove(self.iface)
        h.close_ctrl()
        del h
        self.instance = None

    def get_instance(self):
        """Return the Hostapd/WpaSupplicant instance"""
        if self.instance is None:
             self.instance = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
        return self.instance

    def get_own_mac_address(self):
        """Gets the device's own MAC address"""
        h = self.get_instance()
        status = h.get_status()
        return status['address']

    def get_actual_peer_addr(self):
        """Gets the peer address. A connected station address is returned"""
        h = self.get_instance()
        status = h.get_status()
        return status['bssid']

    def grequest(self, req):
        """Send request on the global control interface"""
        logger.debug("FstSTA::grequest: " + req)
        h = self.get_instance()
        return h.global_request(req)

    def wait_gevent(self, events, timeout=None):
        """Wait for a list of events on the global interface"""
        h = self.get_instance()
        if timeout is not None:
            return h.wait_global_event(events, timeout=timeout)
        else:
            return h.wait_global_event(events)

    def scan(self, freq=None, no_wait=False, only_new=False):
        """Issue Scan with given parameters. Returns the BSS dictionary for the
        AP found (the 1st BSS found. TODO: What if the AP required is not the
        1st in list?) or None if no BSS found. None call be also a result of
        no_wait=True. Note, request("SCAN_RESULTS") can be used to get all the
        results at once."""
        h = self.get_instance()
        h.dump_monitor()
        h.scan(None, freq, no_wait, only_new)
        r = h.get_bss('0')
        h.dump_monitor()
        return r

    def connect(self, ap, **kwargs):
        """Connects to the given AP"""
        if not isinstance(ap, FstAP):
            raise Exception("Bad AP object to connect to")
        h = self.get_instance()
        hap = ap.get_instance()
        h.dump_monitor()
        h.connect(ap.get_ssid(), **kwargs)
        h.dump_monitor()
        self.connected = ap

    def connect_to_external_ap(self, ap, ssid, check_connection=True, **kwargs):
        """Connects to the given external AP"""
        if not isinstance(ap, hostapd.Hostapd):
            raise Exception("Bad AP object to connect to")
        h = self.get_instance()
        h.dump_monitor()
        h.connect(ssid, **kwargs)
        self.connected = ap
        if check_connection:
            ev = ap.wait_event(["AP-STA-CONNECTED"], timeout=10)
            if ev is None:
                self.connected = None
                raise Exception("No connection event received from %s" % ssid)
            h.dump_monitor()

    def disconnect(self, check_disconnect=True):
        """Disconnects from the AP the station is currently connected to"""
        if self.connected is not None:
            h = self.get_instance()
            h.dump_monitor()
            h.request("DISCONNECT")
            if check_disconnect:
                hap = self.connected.get_instance()
                ev = hap.wait_event(["AP-STA-DISCONNECTED"], timeout=10)
                if ev is None:
                    raise Exception("No disconnection event received from %s" % self.connected.get_ssid())
                h.dump_monitor()
            self.connected = None


    def disconnect_from_external_ap(self, check_disconnect=True):
        """Disconnects from the external AP the station is currently connected
        to"""
        if self.connected is not None:
            h = self.get_instance()
            h.dump_monitor()
            h.request("DISCONNECT")
            if check_disconnect:
                hap = self.connected
                ev = hap.wait_event(["AP-STA-DISCONNECTED"], timeout=10)
                if ev is None:
                    raise Exception("No disconnection event received from AP")
                h.dump_monitor()
            self.connected = None

    def dump_monitor(self):
        """Dump control interface monitor events"""
        if self.instance:
            self.instance.dump_monitor()
