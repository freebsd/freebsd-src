# P2P services
# Copyright (c) 2014-2015, Qualcomm Atheros, Inc.
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

from remotehost import remote_compatible
import logging
logger = logging.getLogger()
import time
import random
import re

import hwsim_utils
from wpasupplicant import WpaSupplicant
import hostapd
from p2p_utils import *
from utils import HwsimSkip
from hwsim import HWSimRadio

# Dev[0] -> Advertiser
# Dev[1] -> Seeker
# ev0 -> Event generated at advertiser side
# ev1 -> Event generated at Seeker side

def p2ps_advertise(r_dev, r_role, svc_name, srv_info, rsp_info=None, cpt=None):
    """P2PS Advertise function"""
    adv_id = random.randrange(1, 0xFFFFFFFF)
    advid = hex(adv_id)[2:]

    cpt_param = (" cpt=" + cpt) if cpt is not None else ""

    if rsp_info is not None and srv_info is not None:
        if "OK" not in r_dev.global_request("P2P_SERVICE_ADD asp " + str(r_role) + " " + str(advid) + " 1 1108 " + svc_name + cpt_param + " svc_info='" + srv_info + "'" + " rsp_info=" + rsp_info + "'"):
            raise Exception("P2P_SERVICE_ADD with response info and service info failed")

    if rsp_info is None and srv_info is not None:
        if "OK" not in r_dev.global_request("P2P_SERVICE_ADD asp " + str(r_role) + " " + str(advid) + " 1 1108 " + svc_name + cpt_param + " svc_info='" + srv_info + "'"):
            raise Exception("P2P_SERVICE_ADD with service info failed")

    if rsp_info is None and srv_info is None:
        if "OK" not in r_dev.global_request("P2P_SERVICE_ADD asp " + str(r_role) + " " + str(advid) + " 1 1108 " + svc_name + cpt_param):
            raise Exception("P2P_SERVICE_ADD without service info and without response info failed")

    if rsp_info is not None and srv_info is None:
        if "OK" not in r_dev.global_request("P2P_SERVICE_ADD asp " + str(r_role) + " " + str(adv_id) + " 1 1108 " + svc_name + cpt_param + " svc_info='" + " rsp_info=" + rsp_info + "'"):
            raise Exception("P2P_SERVICE_ADD with response info failed")

    r_dev.p2p_listen()
    return advid

def p2ps_exact_seek(i_dev, r_dev, svc_name, srv_info=None,
                    single_peer_expected=True):
    """P2PS exact service seek request"""
    if srv_info is not None:
        ev1 = i_dev.global_request("P2P_SERV_DISC_REQ 00:00:00:00:00:00 asp 1 " + svc_name + " '" + srv_info + "'")
        if ev1 is None:
            raise Exception("Failed to add Service Discovery request for exact seek request")

    if "OK" not in i_dev.global_request("P2P_FIND 10 type=social seek=" + svc_name):
        raise Exception("Failed to initiate seek operation")

    timeout = time.time() + 10
    ev1 = i_dev.wait_global_event(["P2P-DEVICE-FOUND"], timeout=10)
    while ev1 is not None and not single_peer_expected:
        if r_dev.p2p_dev_addr() in ev1 and "adv_id=" in ev1:
            break
        ev1 = i_dev.wait_global_event(["P2P-DEVICE-FOUND"], timeout=10)

        if timeout < time.time():
            raise Exception("Device not found")

    if ev1 is None:
        raise Exception("P2P-DEVICE-FOUND timeout on seeker side")
    if r_dev.p2p_dev_addr() not in ev1:
        raise Exception("Unexpected peer")

    if srv_info is None:
        adv_id = ev1.split("adv_id=")[1].split(" ")[0]
        rcvd_svc_name = ev1.split("asp_svc=")[1].split(" ")[0]
        if rcvd_svc_name != svc_name:
            raise Exception("service name not matching")
    else:
        ev1 = i_dev.wait_global_event(["P2P-SERV-ASP-RESP"], timeout=10)
        if ev1 is None:
            raise Exception("Failed to receive Service Discovery Response")
        if r_dev.p2p_dev_addr() not in ev1:
            raise Exception("Service Discovery response from Unknown Peer")
        if srv_info is not None and srv_info not in ev1:
            raise Exception("service info not available in Service Discovery response")
        adv_id = ev1.split(" ")[3]
        rcvd_svc_name = ev1.split(" ")[6]
        if rcvd_svc_name != svc_name:
            raise Exception("service name not matching")

    i_dev.p2p_stop_find()
    return [adv_id, rcvd_svc_name]

def p2ps_nonexact_seek(i_dev, r_dev, svc_name, srv_info=None, adv_num=None):
    """P2PS nonexact service seek request"""
    if adv_num is None:
       adv_num = 1
    if srv_info is not None:
        ev1 = i_dev.global_request("P2P_SERV_DISC_REQ 00:00:00:00:00:00 asp 1 " + svc_name + " '" + srv_info + "'")
    else:
        ev1 = i_dev.global_request("P2P_SERV_DISC_REQ 00:00:00:00:00:00 asp 1 " + svc_name + " '")
    if ev1 is None:
        raise Exception("Failed to add Service Discovery request for nonexact seek request")
    if "OK" not in i_dev.global_request("P2P_FIND 10 type=social seek="):
        raise Exception("Failed to initiate seek")
    ev1 = i_dev.wait_global_event(["P2P-DEVICE-FOUND"], timeout=10)
    if ev1 is None:
        raise Exception("P2P-DEVICE-FOUND timeout on seeker side")
    if r_dev.p2p_dev_addr() not in ev1:
        raise Exception("Unexpected peer")
    ev_list = []
    for i in range(0, adv_num):
        ev1 = i_dev.wait_global_event(["P2P-SERV-ASP-RESP"], timeout=10)
        if ev1 is None:
            raise Exception("Failed to receive Service Discovery Response")
        if r_dev.p2p_dev_addr() not in ev1:
            raise Exception("Service Discovery response from Unknown Peer")
        if srv_info is not None and srv_info not in ev1:
            raise Exception("service info not available in Service Discovery response")
        adv_id = ev1.split(" ")[3]
        rcvd_svc_name = ev1.split(" ")[6]
        ev_list.append(''.join([adv_id, ' ', rcvd_svc_name]))

    i_dev.p2p_stop_find()
    return ev_list

def p2ps_parse_event(ev, *args):
    ret = ()
    for arg in args:
        m = re.search("\s+" + arg + r"=(\S+)", ev)
        ret += (m.group(1) if m is not None else None,)
    return ret

def p2ps_provision(seeker, advertiser, adv_id, auto_accept=True, method="1000",
                   adv_cpt=None, seeker_cpt=None, handler=None, adv_role=None,
                   seeker_role=None):
    addr0 = seeker.p2p_dev_addr()
    addr1 = advertiser.p2p_dev_addr()

    seeker.asp_provision(addr1, adv_id=str(adv_id), adv_mac=addr1, session_id=1,
                         session_mac=addr0, method=method, cpt=seeker_cpt,
                         role=seeker_role)

    if not auto_accept or method == "100":
        pin = None
        ev_pd_start = advertiser.wait_global_event(["P2PS-PROV-START"],
                                                   timeout=10)
        if ev_pd_start is None:
            raise Exception("P2PS-PROV-START timeout on Advertiser side")
        peer = ev_pd_start.split()[1]
        advert_id, advert_mac, session, session_mac =\
            p2ps_parse_event(ev_pd_start, "adv_id", "adv_mac", "session", "mac")

        ev = seeker.wait_global_event(["P2P-PROV-DISC-FAILURE"], timeout=10)
        if ev is None:
            raise Exception("P2P-PROV-DISC-FAILURE timeout on seeker side")

        if handler:
            handler(seeker, advertiser)

        # Put seeker into a listen state, since we expect the deferred flow to
        # continue.
        seeker.p2p_ext_listen(500, 500)

        if method == "100":
            ev = advertiser.wait_global_event(["P2P-PROV-DISC-ENTER-PIN"],
                                              timeout=10)
            if ev is None:
                raise Exception("P2P-PROV-DISC-ENTER-PIN timeout on advertiser side")
            if addr0 not in ev:
                raise Exception("Unknown peer " + addr0)
            ev = seeker.wait_global_event(["P2P-PROV-DISC-SHOW-PIN"],
                                          timeout=10)
            if ev is None:
                raise Exception("P2P-PROV-DISC-SHOW-PIN timeout on seeker side")
            if addr1 not in ev:
                raise Exception("Unknown peer " + addr1)
            pin = ev.split()[2]
        elif method == "8":
            ev = advertiser.wait_global_event(["P2P-PROV-DISC-SHOW-PIN"],
                                              timeout=10)
            if ev is None:
                raise Exception("P2P-PROV-DISC-SHOW-PIN timeout on advertiser side")
            if addr0 not in ev:
                raise Exception("Unknown peer " + addr0)
            pin = ev.split()[2]

        # Stop P2P_LISTEN before issuing P2P_ASP_PROVISION_RESP to avoid
        # excessive delay and test case timeouts if it takes large number of
        # retries to find the peer awake on its Listen channel.
        advertiser.p2p_stop_find()

        advertiser.asp_provision(peer, adv_id=advert_id, adv_mac=advert_mac,
                                 session_id=int(session, 0),
                                 session_mac=session_mac, status=12,
                                 cpt=adv_cpt, role=adv_role)

        ev1 = seeker.wait_global_event(["P2PS-PROV-DONE"], timeout=10)
        if ev1 is None:
            raise Exception("P2PS-PROV-DONE timeout on seeker side")

        ev2 = advertiser.wait_global_event(["P2PS-PROV-DONE"], timeout=10)
        if ev2 is None:
            raise Exception("P2PS-PROV-DONE timeout on advertiser side")

        if method == "8":
            ev = seeker.wait_global_event(["P2P-PROV-DISC-ENTER-PIN"],
                                          timeout=10)
            if ev is None:
                raise Exception("P2P-PROV-DISC-ENTER-PIN failed on seeker side")
            if addr1 not in ev:
                raise Exception("Unknown peer " + addr1)

        seeker.p2p_cancel_ext_listen()
        if pin is not None:
            return ev1, ev2, pin
        return ev1, ev2

    # Auto-accept is true and the method is either P2PS or advertiser is DISPLAY
    ev1 = seeker.wait_global_event(["P2PS-PROV-DONE"], timeout=10)
    if ev1 is None:
        raise Exception("P2PS-PROV-DONE timeout on seeker side")

    ev2 = advertiser.wait_global_event(["P2PS-PROV-DONE"], timeout=10)
    if ev2 is None:
        raise Exception("P2PS-PROV-DONE timeout on advertiser side")

    if method == "8":
        ev = seeker.wait_global_event(["P2P-PROV-DISC-ENTER-PIN"], timeout=10)
        if ev is None:
            raise Exception("P2P-PROV-DISC-ENTER-PIN timeout on seeker side")
        if addr1 not in ev:
            raise Exception("Unknown peer " + addr1)
        ev = advertiser.wait_global_event(["P2P-PROV-DISC-SHOW-PIN"],
                                          timeout=10)
        if ev is None:
            raise Exception("P2P-PROV-DISC-SHOW-PIN timeout on advertiser side")
        if addr0 not in ev:
            raise Exception("Unknown peer " + addr0)
        pin = ev.split()[2]
        return ev1, ev2, pin

    return ev1, ev2

def p2ps_connect_pd(dev0, dev1, ev0, ev1, pin=None, join_extra="", go_ev=None):
    conf_methods_map = {"8": "p2ps", "1": "display", "5": "keypad"}
    peer0 = ev0.split()[1]
    peer1 = ev1.split()[1]
    status0, conncap0, adv_id0, adv_mac0, mac0, session0, dev_passwd_id0, go0, join0, feature_cap0, persist0, group_ssid0 =\
        p2ps_parse_event(ev0, "status", "conncap", "adv_id", "adv_mac", "mac", "session", "dev_passwd_id", "go", "join", "feature_cap", "persist", "group_ssid")
    status1, conncap1, adv_id1, adv_mac1, mac1, session1, dev_passwd_id1, go1, join1, feature_cap1, persist1, group_ssid1 =\
        p2ps_parse_event(ev1, "status", "conncap", "adv_id", "adv_mac", "mac", "session", "dev_passwd_id", "go", "join", "feature_cap", "persist", "group_ssid")

    if status0 != "0" and status0 != "12":
        raise Exception("PD failed on " + dev0.p2p_dev_addr())

    if status1 != "0" and status1 != "12":
        raise Exception("PD failed on " + dev1.p2p_dev_addr())

    if status0 == "12" and status1 == "12":
        raise Exception("Both sides have status 12 which doesn't make sense")

    if adv_id0 != adv_id1 or adv_id0 is None:
        raise Exception("Adv. IDs don't match")

    if adv_mac0 != adv_mac1 or adv_mac0 is None:
        raise Exception("Adv. MACs don't match")

    if session0 != session1 or session0 is None:
        raise Exception("Session IDs don't match")

    if mac0 != mac1 or mac0 is None:
        raise Exception("Session MACs don't match")

    #TODO: Validate feature capability

    if bool(persist0) != bool(persist1):
        raise Exception("Only one peer has persistent group")

    if persist0 is None and not all([conncap0, conncap1, dev_passwd_id0,
                                     dev_passwd_id1]):
        raise Exception("Persistent group not used but conncap/dev_passwd_id are missing")

    if persist0 is not None and any([conncap0, conncap1, dev_passwd_id0,
                                     dev_passwd_id1]):
        raise Exception("Persistent group is used but conncap/dev_passwd_id are present")

    # Persistent Connection (todo: handle frequency)
    if persist0 is not None:
        dev0.p2p_stop_find()
        if "OK" not in dev0.global_request("P2P_GROUP_ADD persistent=" + persist0 + " freq=2412"):
            raise Exception("Could not re-start persistent group")
        ev0 = dev0.wait_global_event(["P2P-GROUP-STARTED"], timeout=10)
        if ev0 is None:
            raise Exception("P2P-GROUP-STARTED timeout on " + dev0.p2p_dev_addr())
        dev0.group_form_result(ev0)

        if "OK" not in dev1.global_request("P2P_GROUP_ADD persistent=" + persist1 + " freq=2412"):
            raise Exception("Could not re-start persistent group")
        ev1 = dev1.wait_global_event(["P2P-GROUP-STARTED"], timeout=10)
        if ev1 is None:
            raise Exception("P2P-GROUP-STARTED timeout on " + dev1.p2p_dev_addr())
        dev1.group_form_result(ev1)
        if "GO" in ev0:
            ev = dev0.wait_global_event(["AP-STA-CONNECTED"], timeout=10)
            if ev is None:
                raise Exception("AP-STA-CONNECTED timeout on " + dev0.p2p_dev_addr())
        else:
            ev = dev1.wait_global_event(["AP-STA-CONNECTED"], timeout=10)
            if ev is None:
                raise Exception("AP-STA-CONNECTED timeout on " + dev1.p2p_dev_addr())
    else:
        try:
            method0 = conf_methods_map[dev_passwd_id0]
            method1 = conf_methods_map[dev_passwd_id1]
        except KeyError:
            raise Exception("Unsupported method")

        if method0 == "p2ps":
            pin = "12345670"
        if pin is None:
            raise Exception("Pin is not provided")

        if conncap0 == "1" and conncap1 == "1": # NEW/NEW - GON
            if any([join0, join1, go0, go1]):
                raise Exception("Unexpected join/go PD attributes")
            dev0.p2p_listen()
            if "OK" not in dev0.global_request("P2P_CONNECT " + peer0 + " " + pin + " " + method0 + " persistent auth"):
                raise Exception("P2P_CONNECT fails on " + dev0.p2p_dev_addr())
            if "OK" not in dev1.global_request("P2P_CONNECT " + peer1 + " " + pin + " " + method1 + " persistent"):
                raise Exception("P2P_CONNECT fails on " + dev1.p2p_dev_addr())
            ev = dev0.wait_global_event(["P2P-GO-NEG-SUCCESS"], timeout=10)
            if ev is None:
                raise Exception("GO Neg did not succeed on " + dev0.p2p_dev_addr())
            ev = dev1.wait_global_event(["P2P-GO-NEG-SUCCESS"], timeout=10)
            if ev is None:
                raise Exception("GO Neg did not succeed on " + dev1.p2p_dev_addr())
            ev = dev0.wait_global_event(["P2P-GROUP-STARTED"], timeout=10)
            if ev is None:
                raise Exception("P2P-GROUP-STARTED timeout on " + dev0.p2p_dev_addr())
            dev0.group_form_result(ev)
            ev = dev1.wait_global_event(["P2P-GROUP-STARTED"], timeout=10)
            if ev is None:
                raise Exception("P2P-GROUP-STARTED timeout on " + dev1.p2p_dev_addr())
            dev1.group_form_result(ev)
        else:
            if conncap0 == "2" and conncap1 == "4":  # dev0 CLI, dev1 GO
                dev_cli, dev_go, go_if, join_address, go_method, cli_method, join_ssid = dev0, dev1, go1, join0, method1, method0, group_ssid0
            elif conncap0 == "4" and conncap1 == "2":  # dev0 GO, dev1 CLI
                dev_cli, dev_go, go_if, join_address, go_method, cli_method, join_ssid = dev1, dev0, go0, join1, method0, method1, group_ssid1
            else:
                raise Exception("Bad connection capabilities")

            if go_if is None:
                raise Exception("Device " + dev_go.p2p_dev_addr() + " failed to become GO")
            if join_address is None:
                raise Exception("Device " + dev_cli.p2p_dev_addr() + " failed to become CLI")

            if not dev_go.get_group_ifname().startswith('p2p-'):
                if go_ev:
                    ev = go_ev
                else:
                    ev = dev_go.wait_global_event(["P2P-GROUP-STARTED"],
                                                  timeout=10)
                if ev is None:
                    raise Exception("P2P-GROUP-STARTED timeout on " + dev_go.p2p_dev_addr())
                dev_go.group_form_result(ev)

            if go_method != "p2ps":
                ev = dev_go.group_request("WPS_PIN any " + pin)
                if ev is None:
                    raise Exception("Failed to initiate pin authorization on registrar side")
            if join_ssid:
                group_ssid_txt = " ssid=" + join_ssid
            else:
                group_ssid_txt = ""
            if "OK" not in dev_cli.global_request("P2P_CONNECT " + join_address + " " + pin + " " + cli_method + join_extra + " persistent join" + group_ssid_txt):
                raise Exception("P2P_CONNECT failed on " + dev_cli.p2p_dev_addr())
            ev = dev_cli.wait_global_event(["P2P-GROUP-STARTED"], timeout=10)
            if ev is None:
                raise Exception("P2P-GROUP-STARTED timeout on " + dev_cli.p2p_dev_addr())
            dev_cli.group_form_result(ev)
            ev = dev_go.wait_global_event(["AP-STA-CONNECTED"], timeout=10)
            if ev is None:
                raise Exception("AP-STA-CONNECTED timeout on " + dev_go.p2p_dev_addr())

    hwsim_utils.test_connectivity_p2p(dev0, dev1)

def set_no_group_iface(dev, enable):
    if enable:
        res = dev.get_driver_status()
        if (int(res['capa.flags'], 0) & 0x20000000):
            raise HwsimSkip("P2P Device used. Cannot set enable no_group_iface")
        dev.global_request("SET p2p_no_group_iface 1")
    else:
        dev.global_request("SET p2p_no_group_iface 0")

@remote_compatible
def test_p2ps_exact_search(dev):
    """P2PS exact service request"""
    p2ps_advertise(r_dev=dev[0], r_role='1', svc_name='org.wi-fi.wfds.send.rx',
                   srv_info='I can receive files upto size 2 GB')
    [adv_id, rcvd_svc_name] = p2ps_exact_seek(i_dev=dev[1], r_dev=dev[0],
                                              svc_name='org.wi-fi.wfds.send.rx')

    ev0 = dev[0].global_request("P2P_SERVICE_DEL asp " + str(adv_id))
    if ev0 is None:
        raise Exception("Unable to remove the advertisement instance")

@remote_compatible
def test_p2ps_exact_search_srvinfo(dev):
    """P2PS exact service request with service info"""
    p2ps_advertise(r_dev=dev[0], r_role='0', svc_name='org.wi-fi.wfds.send.rx',
                   srv_info='I can receive files upto size 2 GB')
    [adv_id, rcvd_svc_name] = p2ps_exact_seek(i_dev=dev[1], r_dev=dev[0],
                                              svc_name='org.wi-fi.wfds.send.rx',
                                              srv_info='2 GB')

    ev0 = dev[0].global_request("P2P_SERVICE_DEL asp " + str(adv_id))
    if ev0 is None:
        raise Exception("Unable to remove the advertisement instance")

@remote_compatible
def test_p2ps_nonexact_search(dev):
    """P2PS nonexact seek request"""
    p2ps_advertise(r_dev=dev[0], r_role='0', svc_name='org.wi-fi.wfds.play.rx',
                   srv_info='I support Miracast Mode ')
    ev_list = p2ps_nonexact_seek(i_dev=dev[1], r_dev=dev[0],
                                 svc_name='org.wi-fi.wfds.play*')
    adv_id = ev_list[0].split()[0]

    ev0 = dev[0].global_request("P2P_SERVICE_DEL asp " + str(adv_id))
    if ev0 is None:
        raise Exception("Unable to remove the advertisement instance")

@remote_compatible
def test_p2ps_nonexact_search_srvinfo(dev):
    """P2PS nonexact seek request with service info"""
    p2ps_advertise(r_dev=dev[0], r_role='0', svc_name='org.wi-fi.wfds.send.rx',
                   srv_info='I can receive files upto size 2 GB')
    ev_list = p2ps_nonexact_seek(i_dev=dev[1], r_dev=dev[0],
                                 svc_name='org.wi-fi.wfds.send*',
                                 srv_info='2 GB')
    adv_id = ev_list[0].split()[0]
    ev0 = dev[0].global_request("P2P_SERVICE_DEL asp " + str(adv_id))
    if ev0 is None:
        raise Exception("Unable to remove the advertisement instance")

@remote_compatible
def test_p2ps_connect_p2ps_method_nonautoaccept(dev):
    """P2PS connect for non-auto-accept and P2PS config method"""
    p2ps_advertise(r_dev=dev[0], r_role='0', svc_name='org.wi-fi.wfds.send.rx',
                   srv_info='I can receive files upto size 2 GB')
    ev_list = p2ps_nonexact_seek(i_dev=dev[1], r_dev=dev[0],
                                 svc_name='org.wi-fi.wfds.send*',
                                 srv_info='2 GB')
    adv_id = ev_list[0].split()[0]
    ev1, ev0 = p2ps_provision(dev[1], dev[0], adv_id, auto_accept=False)
    p2ps_connect_pd(dev[0], dev[1], ev0, ev1)

    ev0 = dev[0].global_request("P2P_SERVICE_DEL asp " + str(adv_id))
    if ev0 is None:
        raise Exception("Unable to remove the advertisement instance")
    remove_group(dev[0], dev[1])

@remote_compatible
def test_p2ps_connect_p2ps_method_autoaccept(dev):
    """P2PS connection with P2PS default config method and auto-accept"""
    p2ps_advertise(r_dev=dev[0], r_role='1', svc_name='org.wi-fi.wfds.send.rx',
                   srv_info='I can receive files upto size 2 GB')
    [adv_id, rcvd_svc_name] = p2ps_exact_seek(i_dev=dev[1], r_dev=dev[0],
                                              svc_name='org.wi-fi.wfds.send.rx',
                                              srv_info='2 GB')

    ev1, ev0 = p2ps_provision(dev[1], dev[0], adv_id)
    p2ps_connect_pd(dev[0], dev[1], ev0, ev1)

    ev0 = dev[0].global_request("P2P_SERVICE_DEL asp " + str(adv_id))
    if ev0 is None:
        raise Exception("Unable to remove the advertisement instance")
    remove_group(dev[0], dev[1])

@remote_compatible
def test_p2ps_connect_keypad_method_nonautoaccept(dev):
    """P2PS Connection with non-auto-accept and seeker having keypad method"""
    p2ps_advertise(r_dev=dev[0], r_role='0', svc_name='org.wi-fi.wfds.send.rx',
                   srv_info='I can receive files upto size 2 GB')
    ev_list = p2ps_nonexact_seek(i_dev=dev[1], r_dev=dev[0],
                                 svc_name='org.wi-fi.wfds.send*',
                                 srv_info='2 GB')
    adv_id = ev_list[0].split()[0]

    ev1, ev0, pin = p2ps_provision(dev[1], dev[0], adv_id, auto_accept=False, method="8")
    p2ps_connect_pd(dev[0], dev[1], ev0, ev1, pin)

    ev0 = dev[0].global_request("P2P_SERVICE_DEL asp " + str(adv_id))
    if ev0 is None:
        raise Exception("Unable to remove the advertisement instance")
    remove_group(dev[0], dev[1])

@remote_compatible
def test_p2ps_connect_display_method_nonautoaccept(dev):
    """P2PS connection with non-auto-accept and seeker having display method"""
    p2ps_advertise(r_dev=dev[0], r_role='0', svc_name='org.wi-fi.wfds.send.rx',
                   srv_info='I can receive files upto size 2 GB')
    ev_list = p2ps_nonexact_seek(i_dev=dev[1], r_dev=dev[0],
                                 svc_name='org.wi-fi.wfds*', srv_info='2 GB')
    adv_id = ev_list[0].split()[0]

    ev1, ev0, pin = p2ps_provision(dev[1], dev[0], adv_id, auto_accept=False, method="100")
    p2ps_connect_pd(dev[0], dev[1], ev0, ev1, pin)

    ev0 = dev[0].global_request("P2P_SERVICE_DEL asp " + str(adv_id))
    if ev0 is None:
        raise Exception("Unable to remove the advertisement instance")
    remove_group(dev[0], dev[1])

@remote_compatible
def test_p2ps_connect_keypad_method_autoaccept(dev):
    """P2PS connection with auto-accept and keypad method on seeker side"""
    p2ps_advertise(r_dev=dev[0], r_role='1', svc_name='org.wi-fi.wfds.send.rx',
                   srv_info='I can receive files upto size 2 GB')
    [adv_id, rcvd_svc_name] = p2ps_exact_seek(i_dev=dev[1], r_dev=dev[0],
                                              svc_name='org.wi-fi.wfds.send.rx',
                                              srv_info='2 GB')

    ev1, ev0, pin = p2ps_provision(dev[1], dev[0], adv_id, method="8")
    p2ps_connect_pd(dev[0], dev[1], ev0, ev1, pin)

    ev0 = dev[0].global_request("P2P_SERVICE_DEL asp " + str(adv_id))
    if ev0 is None:
        raise Exception("Unable to remove the advertisement instance")
    remove_group(dev[0], dev[1])

@remote_compatible
def test_p2ps_connect_display_method_autoaccept(dev):
    """P2PS connection with auto-accept and display method on seeker side"""
    p2ps_advertise(r_dev=dev[0], r_role='1', svc_name='org.wi-fi.wfds.send.rx',
                   srv_info='I can receive files upto size 2 GB')
    [adv_id, rcvd_svc_name] = p2ps_exact_seek(i_dev=dev[1], r_dev=dev[0],
                                              svc_name='org.wi-fi.wfds.send.rx',
                                              srv_info='2 GB')

    ev1, ev0, pin = p2ps_provision(dev[1], dev[0], adv_id, method="100")
    p2ps_connect_pd(dev[0], dev[1], ev0, ev1, pin)

    ev0 = dev[0].global_request("P2P_SERVICE_DEL asp " + str(adv_id))
    if ev0 is None:
        raise Exception("Unable to remove the advertisement instance")
    remove_group(dev[0], dev[1])

@remote_compatible
def test_p2ps_connect_adv_go_p2ps_method(dev):
    """P2PS auto-accept connection with advertisement as GO and P2PS method"""
    p2ps_advertise(r_dev=dev[0], r_role='4', svc_name='org.wi-fi.wfds.send.rx',
                   srv_info='I can receive files upto size 2 GB')
    [adv_id, rcvd_svc_name] = p2ps_exact_seek(i_dev=dev[1], r_dev=dev[0],
                                              svc_name='org.wi-fi.wfds.send.rx',
                                              srv_info='2 GB')

    ev1, ev0 = p2ps_provision(dev[1], dev[0], adv_id)
    p2ps_connect_pd(dev[0], dev[1], ev0, ev1)

    ev0 = dev[0].global_request("P2P_SERVICE_DEL asp " + str(adv_id))
    if ev0 is None:
        raise Exception("Unable to remove the advertisement instance")
    remove_group(dev[0], dev[1])

@remote_compatible
def test_p2ps_connect_adv_go_p2ps_method_group_iface(dev):
    """P2PS auto-accept connection with advertisement as GO and P2PS method using separate group interface"""
    set_no_group_iface(dev[0], 0)
    set_no_group_iface(dev[1], 0)
    p2ps_advertise(r_dev=dev[0], r_role='4', svc_name='org.wi-fi.wfds.send.rx',
                   srv_info='I can receive files upto size 2 GB')
    [adv_id, rcvd_svc_name] = p2ps_exact_seek(i_dev=dev[1], r_dev=dev[0],
                                              svc_name='org.wi-fi.wfds.send.rx',
                                              srv_info='2 GB')

    ev1, ev0 = p2ps_provision(dev[1], dev[0], adv_id)
    p2ps_connect_pd(dev[0], dev[1], ev0, ev1)

    ev0 = dev[0].global_request("P2P_SERVICE_DEL asp " + str(adv_id))
    if ev0 is None:
        raise Exception("Unable to remove the advertisement instance")
    remove_group(dev[0], dev[1])

@remote_compatible
def test_p2ps_connect_adv_client_p2ps_method(dev):
    """P2PS auto-accept connection with advertisement as Client and P2PS method"""
    p2ps_advertise(r_dev=dev[0], r_role='2', svc_name='org.wi-fi.wfds.send.rx',
                   srv_info='I can receive files upto size 2 GB')
    [adv_id, rcvd_svc_name] = p2ps_exact_seek(i_dev=dev[1], r_dev=dev[0],
                                              svc_name='org.wi-fi.wfds.send.rx',
                                              srv_info='2 GB')

    ev1, ev0 = p2ps_provision(dev[1], dev[0], adv_id)
    p2ps_connect_pd(dev[0], dev[1], ev0, ev1)

    ev0 = dev[0].global_request("P2P_SERVICE_DEL asp " + str(adv_id))
    if ev0 is None:
        raise Exception("Unable to remove the advertisement instance")
    remove_group(dev[0], dev[1])

def p2ps_connect_adv_go_pin_method(dev, keep_group=False):
    p2ps_advertise(r_dev=dev[0], r_role='4', svc_name='org.wi-fi.wfds.send.rx',
                   srv_info='I can receive files upto size 2 GB')
    [adv_id, rcvd_svc_name] = p2ps_exact_seek(i_dev=dev[1], r_dev=dev[0],
                                              svc_name='org.wi-fi.wfds.send.rx',
                                              srv_info='2 GB')
    ev1, ev0, pin = p2ps_provision(dev[1], dev[0], adv_id, method="8")
    p2ps_connect_pd(dev[0], dev[1], ev0, ev1, pin)

    if not keep_group:
        ev0 = dev[0].global_request("P2P_SERVICE_DEL asp " + str(adv_id))
        if ev0 is None:
            raise Exception("Unable to remove the advertisement instance")
        remove_group(dev[0], dev[1])

@remote_compatible
def test_p2ps_connect_adv_go_pin_method(dev):
    """P2PS advertiser as GO with keypad config method on seeker side and auto-accept"""
    p2ps_connect_adv_go_pin_method(dev)

@remote_compatible
def test_p2ps_connect_adv_client_pin_method(dev):
    """P2PS advertiser as client with keypad config method on seeker side and auto-accept"""
    dev[0].flush_scan_cache()
    p2ps_advertise(r_dev=dev[0], r_role='2', svc_name='org.wi-fi.wfds.send.rx',
                   srv_info='I can receive files upto size 2 GB')
    [adv_id, rcvd_svc_name] = p2ps_exact_seek(i_dev=dev[1], r_dev=dev[0],
                                              svc_name='org.wi-fi.wfds.send.rx',
                                              srv_info='2 GB')

    ev1, ev0, pin = p2ps_provision(dev[1], dev[0], adv_id, method="8")
    p2ps_connect_pd(dev[0], dev[1], ev0, ev1, pin)

    ev0 = dev[0].global_request("P2P_SERVICE_DEL asp " + str(adv_id))
    if ev0 is None:
        raise Exception("Unable to remove the advertisement instance")
    remove_group(dev[0], dev[1])

def test_p2ps_service_discovery_multiple_queries(dev):
    """P2P service discovery with multiple queries"""
    addr0 = dev[0].p2p_dev_addr()
    addr1 = dev[1].p2p_dev_addr()
    adv_id1 = p2ps_advertise(r_dev=dev[0], r_role='0',
                             svc_name='org.wi-fi.wfds.send.tx',
                             srv_info='I can transfer files upto size of 2 GB')
    adv_id2 = p2ps_advertise(r_dev=dev[0], r_role='0',
                             svc_name='org.wi-fi.wfds.send.rx',
                             srv_info='I can receive files upto size of 2 GB')
    adv_id3 = p2ps_advertise(r_dev=dev[0], r_role='1',
                             svc_name='org.wi-fi.wfds.display.tx',
                             srv_info='Miracast Mode')
    adv_id4 = p2ps_advertise(r_dev=dev[0], r_role='1',
                             svc_name='org.wi-fi.wfds.display.rx',
                             srv_info='Miracast Mode')

    dev[1].global_request("P2P_SERV_DISC_REQ " + addr0 + " asp 1 org.wi-fi.wfds.display.tx 'Miracast Mode'")
    dev[1].global_request("P2P_FIND 10 type=social seek=org.wi-fi.wfds.display.tx")
    dev[1].global_request("P2P_SERV_DISC_REQ " + addr0 + " asp 2 org.wi-fi.wfds.send* 'size of 2 GB'")
    dev[1].p2p_stop_find()
    dev[1].global_request("P2P_FIND 10 type=social seek=")
    ev = dev[1].wait_global_event(["P2P-DEVICE-FOUND"], timeout=10)
    if ev is None:
        raise Exception("P2P Device Found timed out")
    if addr0 not in ev:
        raise Exception("Unexpected service discovery request source")
    ev_list = []
    for i in range(0, 3):
        ev = dev[1].wait_global_event(["P2P-SERV-ASP-RESP"], timeout=10)
        if ev is None:
            raise Exception("P2P Service discovery timed out")
        if addr0 in ev:
            ev_list.append(ev)
            if len(ev_list) == 3:
                break
    dev[1].p2p_stop_find()

    for test in [("seek=org.wi-fi.wfds.display.TX",
                  "asp_svc=org.wi-fi.wfds.display.tx"),
                 ("seek=foo seek=org.wi-fi.wfds.display.tx seek=bar",
                  "asp_svc=org.wi-fi.wfds.display.tx"),
                 ("seek=1 seek=2 seek=3 seek=org.wi-fi.wfds.display.tx seek=4 seek=5 seek=6",
                  "asp_svc=org.wi-fi.wfds.display.tx"),
                 ("seek=not-found", None),
                 ("seek=org.wi-fi.wfds", "asp_svc=org.wi-fi.wfds")]:
        dev[2].global_request("P2P_FIND 10 type=social " + test[0])
        if test[1] is None:
            ev = dev[2].wait_global_event(["P2P-DEVICE-FOUND"], timeout=1)
            if ev is not None:
                raise Exception("Unexpected device found: " + ev)
            continue
        ev = dev[2].wait_global_event(["P2P-DEVICE-FOUND"], timeout=10)
        if ev is None:
            raise Exception("P2P device discovery timed out (dev2)")
            if test[1] not in ev:
                raise Exception("Expected asp_svc not reported: " + ev)
        dev[2].p2p_stop_find()
        dev[2].request("P2P_FLUSH")

    dev[0].p2p_stop_find()

    ev1 = dev[0].global_request("P2P_SERVICE_DEL asp " + str(adv_id1))
    if ev1 is None:
        raise Exception("Unable to remove the advertisement instance")
    ev2 = dev[0].global_request("P2P_SERVICE_DEL asp " + str(adv_id2))
    if ev2 is None:
        raise Exception("Unable to remove the advertisement instance")
    ev3 = dev[0].global_request("P2P_SERVICE_DEL asp " + str(adv_id3))
    if ev3 is None:
        raise Exception("Unable to remove the advertisement instance")
    ev4 = dev[0].global_request("P2P_SERVICE_DEL asp " + str(adv_id4))
    if ev4 is None:
        raise Exception("Unable to remove the advertisement instance")

    if "OK" not in dev[0].global_request("P2P_SERVICE_ADD asp 1 12345678 1 1108 org.wi-fi.wfds.foobar svc_info='Test'"):
        raise Exception("P2P_SERVICE_ADD failed")
    if "OK" not in dev[0].global_request("P2P_SERVICE_DEL asp all"):
        raise Exception("P2P_SERVICE_DEL asp all failed")
    if "OK" not in dev[0].global_request("P2P_SERVICE_ADD asp 1 12345678 1 1108 org.wi-fi.wfds.foobar svc_info='Test'"):
        raise Exception("P2P_SERVICE_ADD failed")
    if "OK" not in dev[0].global_request("P2P_SERVICE_REP asp 1 12345678 1 1108 org.wi-fi.wfds.foobar svc_info='Test'"):
        raise Exception("P2P_SERVICE_REP failed")
    if "FAIL" not in dev[0].global_request("P2P_SERVICE_REP asp 1 12345678 1 1108 org.wi-fi.wfds.Foo svc_info='Test'"):
        raise Exception("Invalid P2P_SERVICE_REP accepted")
    if "OK" not in dev[0].global_request("P2P_SERVICE_ADD asp 1 a2345678 1 1108 org.wi-fi.wfds.something svc_info='Test'"):
        raise Exception("P2P_SERVICE_ADD failed")
    if "OK" not in dev[0].global_request("P2P_SERVICE_ADD asp 1 a2345679 1 1108 org.wi-fi.wfds.Foo svc_info='Test'"):
        raise Exception("P2P_SERVICE_ADD failed")

def get_ifnames():
    with open('/proc/net/dev', 'r') as f:
        data = f.read()
    ifnames = []
    for line in data.splitlines():
        ifname = line.strip().split(' ')[0]
        if ':' not in ifname:
            continue
        ifname = ifname.split(':')[0]
        ifnames.append(ifname)
    return ifnames

def p2ps_connect_p2ps_method(dev, keep_group=False, join_extra="", flush=True):
    if flush:
        dev[0].flush_scan_cache()
        dev[1].flush_scan_cache()
    p2ps_advertise(r_dev=dev[0], r_role='2', svc_name='org.wi-fi.wfds.send.rx',
                   srv_info='I can receive files upto size 2 GB')
    [adv_id, rcvd_svc_name] = p2ps_exact_seek(i_dev=dev[1], r_dev=dev[0],
                                              svc_name='org.wi-fi.wfds.send.rx',
                                              srv_info='2 GB')
    ev1, ev0 = p2ps_provision(dev[1], dev[0], adv_id)
    go_ev = None
    if "join=" in ev0 and "go=" in ev1:
        # dev[1] started GO and dev[0] is about to join it.
        # Parse P2P-GROUP-STARTED from the GO to learn the operating frequency.
        go_ev = dev[1].wait_global_event(["P2P-GROUP-STARTED"], timeout=10)
        if go_ev is None:
            raise Exception("P2P-GROUP-STARTED timeout on dev1")
        res = dev[1].group_form_result(go_ev)
        if join_extra == "":
            join_extra = " freq=" + res['freq']

    ifnames = get_ifnames()
    p2ps_connect_pd(dev[0], dev[1], ev0, ev1, join_extra=join_extra,
                    go_ev=go_ev)

    grp_ifname0 = dev[0].get_group_ifname()
    grp_ifname1 = dev[1].get_group_ifname()
    if not keep_group:
        ev0 = dev[0].global_request("P2P_SERVICE_DEL asp " + str(adv_id))
        if ev0 is None:
            raise Exception("Unable to remove the advertisement instance")
        ifnames = ifnames + get_ifnames()
        remove_group(dev[0], dev[1])
        ifnames = ifnames + get_ifnames()

    return grp_ifname0, grp_ifname1, ifnames

def has_string_prefix(vals, prefix):
    for val in vals:
        if val.startswith(prefix):
            return True
    return False

def test_p2ps_connect_p2ps_method_1(dev):
    """P2PS connection with P2PS method - no group interface"""
    set_no_group_iface(dev[0], 1)
    set_no_group_iface(dev[1], 1)

    (grp_ifname0, grp_ifname1, ifnames) = p2ps_connect_p2ps_method(dev)
    if grp_ifname0 != dev[0].ifname:
        raise Exception("unexpected dev0 group ifname: " + grp_ifname0)
    if grp_ifname1 != dev[1].ifname:
        raise Exception("unexpected dev1 group ifname: " + grp_ifname1)
    if has_string_prefix(ifnames, 'p2p-' + grp_ifname0):
        raise Exception("dev0 group interface unexpectedly present")
    if has_string_prefix(ifnames, 'p2p-' + grp_ifname1):
        raise Exception("dev1 group interface unexpectedly present")

def test_p2ps_connect_p2ps_method_2(dev):
    """P2PS connection with P2PS method - group interface on dev0"""
    set_no_group_iface(dev[0], 0)
    set_no_group_iface(dev[1], 1)

    (grp_ifname0, grp_ifname1, ifnames) = p2ps_connect_p2ps_method(dev)
    if not grp_ifname0.startswith('p2p-' + dev[0].ifname + '-'):
        raise Exception("unexpected dev0 group ifname: " + grp_ifname0)
    if grp_ifname1 != dev[1].ifname:
        raise Exception("unexpected dev1 group ifname: " + grp_ifname1)
    if has_string_prefix(ifnames, 'p2p-' + grp_ifname0):
        raise Exception("dev0 group interface unexpectedly present")

def test_p2ps_connect_p2ps_method_3(dev):
    """P2PS connection with P2PS method - group interface on dev1"""
    set_no_group_iface(dev[0], 1)
    set_no_group_iface(dev[1], 0)

    (grp_ifname0, grp_ifname1, ifnames) = p2ps_connect_p2ps_method(dev)
    if grp_ifname0 != dev[0].ifname:
        raise Exception("unexpected dev0 group ifname: " + grp_ifname0)
    if not grp_ifname1.startswith('p2p-' + dev[1].ifname + '-'):
        raise Exception("unexpected dev1 group ifname: " + grp_ifname1)
    if has_string_prefix(ifnames, 'p2p-' + grp_ifname0):
        raise Exception("dev0 group interface unexpectedly present")

def test_p2ps_connect_p2ps_method_4(dev):
    """P2PS connection with P2PS method - group interface on both"""
    set_no_group_iface(dev[0], 0)
    set_no_group_iface(dev[1], 0)

    (grp_ifname0, grp_ifname1, ifnames) = p2ps_connect_p2ps_method(dev)
    if not grp_ifname0.startswith('p2p-' + dev[0].ifname + '-'):
        raise Exception("unexpected dev0 group ifname: " + grp_ifname0)
    if not grp_ifname1.startswith('p2p-' + dev[1].ifname + '-'):
        raise Exception("unexpected dev1 group ifname: " + grp_ifname1)

def test_p2ps_connect_adv_go_persistent(dev):
    """P2PS auto-accept connection with advertisement as GO and having persistent group"""
    go_neg_pin_authorized_persistent(i_dev=dev[0], i_intent=15,
                                     r_dev=dev[1], r_intent=0)
    dev[0].remove_group()
    dev[1].wait_go_ending_session()

    p2ps_advertise(r_dev=dev[0], r_role='4', svc_name='org.wi-fi.wfds.send.rx',
                   srv_info='I can receive files upto size 2 GB')
    [adv_id, rcvd_svc_name] = p2ps_exact_seek(i_dev=dev[1], r_dev=dev[0],
                                              svc_name='org.wi-fi.wfds.send.rx',
                                              srv_info='2 GB')
    ev1, ev0 = p2ps_provision(dev[1], dev[0], adv_id)
    if "persist=" not in ev0 or "persist=" not in ev1:
        raise Exception("Persistent group isn't used by peers")

    p2ps_connect_pd(dev[0], dev[1], ev0, ev1)
    remove_group(dev[0], dev[1])

def test_p2ps_stale_group_removal(dev):
    """P2PS stale group removal"""
    go_neg_pin_authorized_persistent(i_dev=dev[0], i_intent=15,
                                     r_dev=dev[1], r_intent=0)
    dev[0].remove_group()
    dev[1].wait_go_ending_session()

    # Drop the first persistent group on dev[1] and form new persistent groups
    # on both devices.
    dev[1].p2pdev_request("FLUSH")
    go_neg_pin_authorized_persistent(i_dev=dev[0], i_intent=15,
                                     r_dev=dev[1], r_intent=0)
    dev[0].remove_group()
    dev[1].wait_go_ending_session()

    # The GO now has a stale persistent group as the first entry. Try to go
    # through P2PS sequence to hit stale group removal.
    if len(dev[0].list_networks(p2p=True)) != 2:
        raise Exception("Unexpected number of networks on dev[0]")
    if len(dev[1].list_networks(p2p=True)) != 1:
        raise Exception("Unexpected number of networks on dev[1]")

    p2ps_advertise(r_dev=dev[0], r_role='4', svc_name='org.wi-fi.wfds.send.rx',
                   srv_info='I can receive files upto size 2 GB')
    [adv_id, rcvd_svc_name] = p2ps_exact_seek(i_dev=dev[1], r_dev=dev[0],
                                              svc_name='org.wi-fi.wfds.send.rx',
                                              srv_info='2 GB')
    ev1, ev0 = p2ps_provision(dev[1], dev[0], adv_id)
    if "persist=" not in ev0 or "persist=" not in ev1:
        raise Exception("Persistent group isn't used by peers")

    p2ps_connect_pd(dev[0], dev[1], ev0, ev1)
    remove_group(dev[0], dev[1])

    if len(dev[0].list_networks(p2p=True)) != 1:
        raise Exception("Unexpected number of networks on dev[0] (2)")
    if len(dev[1].list_networks(p2p=True)) != 1:
        raise Exception("Unexpected number of networks on dev[1] (2)")

def test_p2ps_stale_group_removal2(dev):
    """P2PS stale group removal (2)"""
    go_neg_pin_authorized_persistent(i_dev=dev[0], i_intent=0,
                                     r_dev=dev[1], r_intent=15)
    dev[1].remove_group()
    dev[0].wait_go_ending_session()

    # Drop the first persistent group on dev[1] and form new persistent groups
    # on both devices.
    dev[1].p2pdev_request("FLUSH")
    go_neg_pin_authorized_persistent(i_dev=dev[0], i_intent=0,
                                     r_dev=dev[1], r_intent=15)
    dev[1].remove_group()
    dev[0].wait_go_ending_session()

    # The P2P Client now has a stale persistent group as the first entry. Try
    # to go through P2PS sequence to hit stale group removal.
    if len(dev[0].list_networks(p2p=True)) != 2:
        raise Exception("Unexpected number of networks on dev[0]")
    if len(dev[1].list_networks(p2p=True)) != 1:
        raise Exception("Unexpected number of networks on dev[1]")

    p2ps_advertise(r_dev=dev[1], r_role='4', svc_name='org.wi-fi.wfds.send.rx',
                   srv_info='I can receive files upto size 2 GB')
    [adv_id, rcvd_svc_name] = p2ps_exact_seek(i_dev=dev[0], r_dev=dev[1],
                                              svc_name='org.wi-fi.wfds.send.rx',
                                              srv_info='2 GB')
    ev0, ev1 = p2ps_provision(dev[0], dev[1], adv_id)
    # This hits persistent group removal on dev[0] (P2P Client)

def test_p2ps_stale_group_removal3(dev):
    """P2PS stale group removal (3)"""
    dev[0].p2p_start_go(persistent=True)
    dev[0].remove_group()
    if len(dev[0].list_networks(p2p=True)) != 1:
        raise Exception("Unexpected number of networks on dev[0]")

    go_neg_pin_authorized_persistent(i_dev=dev[0], i_intent=15,
                                     r_dev=dev[1], r_intent=0)
    dev[0].remove_group()
    dev[1].wait_go_ending_session()

    # The GO now has a stale persistent group as the first entry. Try to go
    # through P2PS sequence to hit stale group removal.
    if len(dev[0].list_networks(p2p=True)) != 2:
        raise Exception("Unexpected number of networks on dev[0] (2)")
    if len(dev[1].list_networks(p2p=True)) != 1:
        raise Exception("Unexpected number of networks on dev[1] (2)")

    p2ps_advertise(r_dev=dev[0], r_role='4', svc_name='org.wi-fi.wfds.send.rx',
                   srv_info='I can receive files upto size 2 GB')
    [adv_id, rcvd_svc_name] = p2ps_exact_seek(i_dev=dev[1], r_dev=dev[0],
                                              svc_name='org.wi-fi.wfds.send.rx',
                                              srv_info='2 GB')
    ev1, ev0 = p2ps_provision(dev[1], dev[0], adv_id)
    if "persist=" not in ev0 or "persist=" not in ev1:
        raise Exception("Persistent group isn't used by peers")

    p2ps_connect_pd(dev[0], dev[1], ev0, ev1)
    remove_group(dev[0], dev[1])

    if len(dev[0].list_networks(p2p=True)) != 1:
        raise Exception("Unexpected number of networks on dev[0] (3)")
    if len(dev[1].list_networks(p2p=True)) != 1:
        raise Exception("Unexpected number of networks on dev[1] (3)")

@remote_compatible
def test_p2ps_adv_go_persistent_no_peer_entry(dev):
    """P2PS advertisement as GO having persistent group (no peer entry)"""
    go_neg_pin_authorized_persistent(i_dev=dev[0], i_intent=15,
                                     r_dev=dev[1], r_intent=0)
    dev[0].remove_group()
    dev[1].wait_go_ending_session()

    p2ps_advertise(r_dev=dev[0], r_role='4', svc_name='org.wi-fi.wfds.send.rx',
                   srv_info='I can receive files upto size 2 GB')
    [adv_id, rcvd_svc_name] = p2ps_exact_seek(i_dev=dev[1], r_dev=dev[0],
                                              svc_name='org.wi-fi.wfds.send.rx',
                                              srv_info='2 GB')
    dev[0].global_request("P2P_FLUSH")
    dev[0].p2p_listen()
    ev1, ev0 = p2ps_provision(dev[1], dev[0], adv_id)
    if "persist=" not in ev0 or "persist=" not in ev1:
        raise Exception("Persistent group isn't used by peers")

@remote_compatible
def test_p2ps_pd_follow_on_status_failure(dev):
    """P2PS PD follow on request with status 11"""
    addr0 = dev[0].p2p_dev_addr()
    addr1 = dev[1].p2p_dev_addr()

    p2ps_advertise(r_dev=dev[0], r_role='0', svc_name='org.wi-fi.wfds.send.rx',
                   srv_info='I can receive files upto size 2 GB')
    [adv_id, rcvd_svc_name] = p2ps_exact_seek(i_dev=dev[1], r_dev=dev[0],
                                              svc_name='org.wi-fi.wfds.send.rx',
                                              srv_info='2 GB')
    dev[1].asp_provision(addr0, adv_id=str(adv_id), adv_mac=addr0,
                         session_id=1, session_mac=addr1)
    ev_pd_start = dev[0].wait_global_event(["P2PS-PROV-START"], timeout=10)
    if ev_pd_start is None:
        raise Exception("P2PS-PROV-START timeout on Advertiser side")
    ev = dev[1].wait_global_event(["P2P-PROV-DISC-FAILURE"], timeout=10)
    if ev is None:
        raise Exception("P2P-PROV-DISC-FAILURE timeout on seeker side")
    dev[1].p2p_ext_listen(500, 500)
    dev[0].p2p_stop_find()
    dev[0].asp_provision(addr1, adv_id=str(adv_id), adv_mac=addr0, session_id=1,
                         session_mac=addr1, status=11, method=0)

    ev = dev[1].wait_global_event(["P2PS-PROV-DONE"], timeout=10)
    if ev is None:
        raise Exception("P2P-PROV-DONE timeout on seeker side")
    if adv_id not in ev:
        raise Exception("P2P-PROV-DONE without adv_id on seeker side")
    if "status=11" not in ev:
        raise Exception("P2P-PROV-DONE without status on seeker side")

    ev = dev[0].wait_global_event(["P2PS-PROV-DONE"], timeout=10)
    if ev is None:
        raise Exception("P2P-PROV-DONE timeout on advertiser side")
    if adv_id not in ev:
        raise Exception("P2P-PROV-DONE without adv_id on advertiser side")
    if "status=11" not in ev:
        raise Exception("P2P-PROV-DONE without status on advertiser side")

def test_p2ps_client_probe(dev):
    """P2PS CLI discoverability on operating channel"""
    cli_probe = dev[0].global_request("SET p2p_cli_probe 1")
    p2ps_connect_p2ps_method(dev, keep_group=True)
    [adv_id, rcvd_svc_name] = p2ps_exact_seek(i_dev=dev[2], r_dev=dev[0],
                                              svc_name='org.wi-fi.wfds.send.rx',
                                              single_peer_expected=False)
    dev[0].global_request("P2P_SERVICE_DEL asp " + str(adv_id))
    remove_group(dev[0], dev[1])

def test_p2ps_go_probe(dev):
    """P2PS GO discoverability on operating channel"""
    p2ps_connect_adv_go_pin_method(dev, keep_group=True)
    [adv_id, rcvd_svc_name] = p2ps_exact_seek(i_dev=dev[2], r_dev=dev[0],
                                              svc_name='org.wi-fi.wfds.send.rx',
                                              single_peer_expected=False)
    dev[0].global_request("P2P_SERVICE_DEL asp " + str(adv_id))
    remove_group(dev[0], dev[1])

@remote_compatible
def test_p2ps_wildcard_p2ps(dev):
    """P2PS wildcard SD Probe Request/Response"""
    p2ps_wildcard = "org.wi-fi.wfds"

    adv_id = p2ps_advertise(r_dev=dev[0], r_role='1',
                            svc_name='org.foo.service',
                            srv_info='I can do stuff')
    adv_id2 = p2ps_advertise(r_dev=dev[0], r_role='1',
                             svc_name='org.wi-fi.wfds.send.rx',
                             srv_info='I can receive files upto size 2 GB')

    if "OK" not in dev[1].global_request("P2P_FIND 10 type=social seek=org.foo.service seek=" + p2ps_wildcard):
        raise Exception("Failed on P2P_FIND command")

    ev1 = dev[1].wait_global_event(["P2P-DEVICE-FOUND"], timeout=10)
    if ev1 is None:
        raise Exception("P2P-DEVICE-FOUND timeout on seeker side")
    if dev[0].p2p_dev_addr() not in ev1:
        raise Exception("Unexpected peer")

    ev2 = dev[1].wait_global_event(["P2P-DEVICE-FOUND"], timeout=10)
    if ev2 is None:
        raise Exception("P2P-DEVICE-FOUND timeout on seeker side (2)")
    if dev[0].p2p_dev_addr() not in ev2:
        raise Exception("Unexpected peer (2)")

    if p2ps_wildcard not in ev1 + ev2:
        raise Exception("P2PS Wildcard name not found in P2P-DEVICE-FOUND event")
    if "org.foo.service" not in ev1 + ev2:
        raise Exception("Vendor specific service name not found in P2P-DEVICE-FOUND event")

    if "OK" not in dev[1].global_request("P2P_STOP_FIND"):
        raise Exception("P2P_STOP_FIND failed")
    dev[1].dump_monitor()

    res = dev[0].global_request("P2P_SERVICE_DEL asp " + str(adv_id))
    if res is None:
        raise Exception("Unable to remove the advertisement instance")

    if "OK" not in dev[1].global_request("P2P_FIND 10 type=social seek=" + p2ps_wildcard):
        raise Exception("Failed on P2P_FIND command")

    ev1 = dev[1].wait_global_event(["P2P-DEVICE-FOUND"], timeout=10)
    if ev1 is None:
        raise Exception("P2P-DEVICE-FOUND timeout on seeker side")
    if dev[0].p2p_dev_addr() not in ev1:
        raise Exception("Unexpected peer")
    if p2ps_wildcard not in ev1:
        raise Exception("P2PS Wildcard name not found in P2P-DEVICE-FOUND event (2)")
    dev[1].dump_monitor()

    res = dev[0].global_request("P2P_SERVICE_DEL asp " + str(adv_id2))
    if res is None:
        raise Exception("Unable to remove the advertisement instance 2")

    dev[1].p2p_stop_find()
    time.sleep(0.1)
    if "OK" not in dev[1].global_request("P2P_FIND 10 type=social seek=" + p2ps_wildcard):
        raise Exception("Failed on P2P_FIND command")

    ev1 = dev[1].wait_global_event(["P2P-DEVICE-FOUND"], timeout=2)
    if ev1 is not None:
        raise Exception("Unexpected P2P-DEVICE-FOUND event on seeker side")
    dev[1].p2p_stop_find()
    dev[1].dump_monitor()

def test_p2ps_many_services_in_probe(dev):
    """P2PS with large number of services in Probe Request/Response"""
    long1 = 'org.example.0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789.a'
    long2 = 'org.example.0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789.b'
    long3 = 'org.example.0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789.c'
    long4 = 'org.example.0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789.d'
    long5 = 'org.example.0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789.e'
    for name in [long1, long2, long3, long4, long5]:
        p2ps_advertise(r_dev=dev[0], r_role='1',
                       svc_name=name,
                       srv_info='I can do stuff')

    if "OK" not in dev[1].global_request("P2P_FIND 10 type=social seek=%s seek=%s seek=%s seek=%s seek=%s" % (long1, long2, long3, long4, long5)):
        raise Exception("Failed on P2P_FIND command")

    events = ""
    # Note: Require only four events since all the services do not fit within
    # the length limit.
    for i in range(4):
        ev = dev[1].wait_global_event(["P2P-DEVICE-FOUND"], timeout=10)
        if ev is None:
            raise Exception("Missing P2P-DEVICE-FOUND")
        events = events + ev
    dev[1].p2p_stop_find()
    dev[1].dump_monitor()
    for name in [long2, long3, long4, long5]:
        if name not in events:
            raise Exception("Service missing from peer events")

def p2ps_test_feature_capability_cpt(dev, adv_cpt, seeker_cpt, adv_role,
                                     result):
    p2ps_advertise(r_dev=dev[0], r_role=adv_role,
                   svc_name='org.wi-fi.wfds.send.rx',
                   srv_info='I can receive files upto size 2 GB', cpt=adv_cpt)
    [adv_id, rcvd_svc_name] = p2ps_exact_seek(i_dev=dev[1], r_dev=dev[0],
                                              svc_name='org.wi-fi.wfds.send.rx',
                                              srv_info='2 GB')
    auto_accept = adv_role != "0"
    ev1, ev0, pin = p2ps_provision(dev[1], dev[0], adv_id,
                                   auto_accept=auto_accept, adv_cpt=adv_cpt,
                                   seeker_cpt=seeker_cpt, method="8")

    status0, fcap0 = p2ps_parse_event(ev0, "status", "feature_cap")
    status1, fcap1 = p2ps_parse_event(ev0, "status", "feature_cap")

    if fcap0 is None:
        raise Exception("Bad feature capability on Seeker side")
    if fcap1 is None:
        raise Exception("Bad feature capability on Advertiser side")
    if fcap0 != fcap1:
        raise Exception("Incompatible feature capability values")

    if status0 not in ("0", "12") or status1 not in ("0", "12"):
        raise Exception("Unexpected PD result status")

    if result == "UDP" and fcap0[1] != "1":
        raise Exception("Unexpected CPT feature capability value (expected: UDP)")
    elif result == "MAC" and fcap0[1] != "2":
        raise Exception("Unexpected CPT feature capability value (expected: MAC)")

    ev = dev[0].global_request("P2P_SERVICE_DEL asp " + str(adv_id))
    if ev is None:
        raise Exception("Unable to remove the advertisement instance")

@remote_compatible
def test_p2ps_feature_capability_mac_autoaccept(dev):
    """P2PS PD Feature Capability CPT: advertiser MAC, seeker UDP:MAC, autoaccept"""
    p2ps_test_feature_capability_cpt(dev, adv_cpt="MAC", seeker_cpt="UDP:MAC",
                                     adv_role="4", result="MAC")

@remote_compatible
def test_p2ps_feature_capability_mac_nonautoaccept(dev):
    """P2PS PD Feature Capability CPT: advertiser:MAC, seeker UDP:MAC, nonautoaccept"""
    p2ps_test_feature_capability_cpt(dev, adv_cpt="MAC", seeker_cpt="UDP:MAC",
                                     adv_role="0", result="MAC")

@remote_compatible
def test_p2ps_feature_capability_mac_udp_autoaccept(dev):
    """P2PS PD Feature Capability CPT: advertiser MAC:UDP, seeker UDP:MAC, autoaccept"""
    p2ps_test_feature_capability_cpt(dev, adv_cpt="MAC:UDP",
                                     seeker_cpt="UDP:MAC", adv_role="2",
                                     result="MAC")

@remote_compatible
def test_p2ps_feature_capability_mac_udp_nonautoaccept(dev):
    """P2PS PD Feature Capability CPT: advertiser MAC:UDP, seeker UDP:MAC, nonautoaccept"""
    p2ps_test_feature_capability_cpt(dev, adv_cpt="MAC:UDP",
                                     seeker_cpt="UDP:MAC", adv_role="0",
                                     result="UDP")

@remote_compatible
def test_p2ps_feature_capability_udp_mac_autoaccept(dev):
    """P2PS PD Feature Capability CPT: advertiser UDP:MAC, seeker MAC:UDP, autoaccept"""
    p2ps_test_feature_capability_cpt(dev, adv_cpt="UDP:MAC",
                                     seeker_cpt="MAC:UDP", adv_role="2",
                                     result="UDP")

@remote_compatible
def test_p2ps_feature_capability_udp_mac_nonautoaccept(dev):
    """P2PS PD Feature Capability CPT: advertiser UDP:MAC, seeker MAC:UDP,  nonautoaccept"""
    p2ps_test_feature_capability_cpt(dev, adv_cpt="UDP:MAC",
                                     seeker_cpt="MAC:UDP", adv_role="0",
                                     result="MAC")

def test_p2ps_channel_one_connected(dev, apdev):
    """P2PS connection with P2PS method - one of the stations connected"""
    set_no_group_iface(dev[0], 0)
    set_no_group_iface(dev[1], 0)

    try:
        hapd = hostapd.add_ap(apdev[0],
                              {"ssid": 'bss-2.4ghz', "channel": '7'})
        dev[1].connect("bss-2.4ghz", key_mgmt="NONE", scan_freq="2442")

        (grp_ifname0, grp_ifname1, ifnames) = p2ps_connect_p2ps_method(dev, keep_group=True, join_extra=" freq=2442")
        freq = dev[0].get_group_status_field('freq')

        if freq != '2442':
            raise Exception('Unexpected frequency for group 2442 != ' + freq)
    finally:
        dev[0].global_request("P2P_SERVICE_DEL asp all")
        remove_group(dev[0], dev[1])

def set_random_listen_chan(dev):
    chan = random.randrange(0, 3) * 5 + 1
    dev.global_request("P2P_SET listen_channel %d" % chan)

def test_p2ps_channel_both_connected_same(dev, apdev):
    """P2PS connection with P2PS method - stations connected on same channel"""
    set_no_group_iface(dev[2], 0)
    set_no_group_iface(dev[1], 0)

    dev[2].global_request("P2P_SET listen_channel 6")
    dev[1].global_request("P2P_SET listen_channel 6")

    dev[1].flush_scan_cache()
    dev[2].flush_scan_cache()

    try:
        hapd = hostapd.add_ap(apdev[0],
                              {"ssid": 'bss-2.4ghz', "channel": '6'})

        dev[2].connect("bss-2.4ghz", key_mgmt="NONE", scan_freq="2437")
        dev[1].connect("bss-2.4ghz", key_mgmt="NONE", scan_freq="2437")

        tmpdev = [dev[2], dev[1]]
        (grp_ifname0, grp_ifname1, ifnames) = p2ps_connect_p2ps_method(tmpdev, keep_group=True, join_extra=" freq=2437", flush=False)
        freq = dev[2].get_group_status_field('freq')

        if freq != '2437':
            raise Exception('Unexpected frequency for group 2437 != ' + freq)
    finally:
        dev[2].global_request("P2P_SERVICE_DEL asp all")
        for i in range(1, 3):
            set_random_listen_chan(dev[i])
        remove_group(dev[2], dev[1])

def disconnect_handler(seeker, advertiser):
    advertiser.request("DISCONNECT")
    advertiser.wait_disconnected(timeout=1)

def test_p2ps_channel_both_connected_different(dev, apdev):
    """P2PS connection with P2PS method - stations connected on different channel"""
    if dev[0].get_mcc() > 1:
        raise HwsimSkip('Skip due to MCC being enabled')

    set_no_group_iface(dev[0], 0)
    set_no_group_iface(dev[1], 0)

    try:
        hapd1 = hostapd.add_ap(apdev[0],
                               {"ssid": 'bss-channel-3', "channel": '3'})

        hapd2 = hostapd.add_ap(apdev[1],
                               {"ssid": 'bss-channel-10', "channel": '10'})

        dev[0].connect("bss-channel-3", key_mgmt="NONE", scan_freq="2422")
        dev[1].connect("bss-channel-10", key_mgmt="NONE", scan_freq="2457")

        p2ps_advertise(r_dev=dev[0], r_role='2',
                       svc_name='org.wi-fi.wfds.send.rx',
                       srv_info='I can receive files upto size 2 GB')
        [adv_id, rcvd_svc_name] = p2ps_exact_seek(i_dev=dev[1], r_dev=dev[0],
                                                  svc_name='org.wi-fi.wfds.send.rx',
                                                  srv_info='2 GB')

        ev1, ev0 = p2ps_provision(dev[1], dev[0], adv_id, auto_accept=False,
                                  handler=disconnect_handler)
        p2ps_connect_pd(dev[0], dev[1], ev0, ev1)
        freq = dev[0].get_group_status_field('freq')
        if freq != '2457':
            raise Exception('Unexpected frequency for group 2457 != ' + freq)
    finally:
        dev[0].global_request("P2P_SERVICE_DEL asp all")
        remove_group(dev[0], dev[1])

def test_p2ps_channel_both_connected_different_mcc(dev, apdev):
    """P2PS connection with P2PS method - stations connected on different channels with mcc"""
    with HWSimRadio(n_channels=2) as (radio, iface):
        wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
        wpas.interface_add(iface)

        set_no_group_iface(wpas, 0)
        set_no_group_iface(dev[1], 0)

        try:
            hapd1 = hostapd.add_ap(apdev[0],
                                   {"ssid": 'bss-channel-3', "channel": '3'})

            hapd2 = hostapd.add_ap(apdev[1],
                                   {"ssid": 'bss-channel-10', "channel": '10'})

            wpas.connect("bss-channel-3", key_mgmt="NONE", scan_freq="2422")
            dev[1].connect("bss-channel-10", key_mgmt="NONE", scan_freq="2457")

            (grp_ifname0, grp_ifname1, ifnames) = p2ps_connect_p2ps_method([wpas, dev[1]], keep_group=True)
            freq = wpas.get_group_status_field('freq')

            if freq != '2422' and freq != '2457':
                raise Exception('Unexpected frequency for group =' + freq)
        finally:
            wpas.global_request("P2P_SERVICE_DEL asp all")
            remove_group(wpas, dev[1])

def clear_disallow_handler(seeker, advertiser):
    advertiser.global_request("P2P_SET disallow_freq ")

@remote_compatible
def test_p2ps_channel_disallow_freq(dev, apdev):
    """P2PS connection with P2PS method - disallow freqs"""
    set_no_group_iface(dev[0], 0)
    set_no_group_iface(dev[1], 0)

    try:
        dev[0].global_request("P2P_SET disallow_freq 2412-2457")
        dev[1].global_request("P2P_SET disallow_freq 2417-2462")

        p2ps_advertise(r_dev=dev[0], r_role='2',
                       svc_name='org.wi-fi.wfds.send.rx',
                       srv_info='I can receive files upto size 2 GB')

        [adv_id, rcvd_svc_name] = p2ps_exact_seek(i_dev=dev[1], r_dev=dev[0],
                                                  svc_name='org.wi-fi.wfds.send.rx',
                                                  srv_info='2 GB')

        ev1, ev0 = p2ps_provision(dev[1], dev[0], adv_id, auto_accept=False,
                                  handler=clear_disallow_handler)
        p2ps_connect_pd(dev[0], dev[1], ev0, ev1)

        freq = dev[0].get_group_status_field('freq')
        if freq != '2412':
            raise Exception('Unexpected frequency for group 2412 != ' + freq)
    finally:
        dev[0].global_request("P2P_SERVICE_DEL asp all")
        dev[0].global_request("P2P_SET disallow_freq ")
        dev[1].global_request("P2P_SET disallow_freq ")
        remove_group(dev[0], dev[1])

def test_p2ps_channel_sta_connected_disallow_freq(dev, apdev):
    """P2PS connection with P2PS method - one station and disallow freqs"""
    if dev[0].get_mcc() > 1:
        raise HwsimSkip('Skip due to MCC being enabled')

    set_no_group_iface(dev[0], 0)
    set_no_group_iface(dev[1], 0)

    try:
        dev[0].global_request("P2P_SET disallow_freq 2437")
        hapd = hostapd.add_ap(apdev[0],
                              {"ssid": 'bss-channel-6', "channel": '6'})

        dev[1].connect("bss-channel-6", key_mgmt="NONE", scan_freq="2437")

        p2ps_advertise(r_dev=dev[0], r_role='2',
                       svc_name='org.wi-fi.wfds.send.rx',
                       srv_info='I can receive files upto size 2 GB')
        [adv_id, rcvd_svc_name] = p2ps_exact_seek(i_dev=dev[1], r_dev=dev[0],
                                                  svc_name='org.wi-fi.wfds.send.rx',
                                                  srv_info='2 GB')

        ev1, ev0 = p2ps_provision(dev[1], dev[0], adv_id, auto_accept=False,
                                  handler=clear_disallow_handler)
        p2ps_connect_pd(dev[0], dev[1], ev0, ev1)

        freq = dev[0].get_group_status_field('freq')
        if freq != '2437':
            raise Exception('Unexpected frequency for group 2437 != ' + freq)
    finally:
        dev[0].global_request("P2P_SET disallow_freq ")
        dev[0].global_request("P2P_SERVICE_DEL asp all")
        remove_group(dev[0], dev[1])

def test_p2ps_channel_sta_connected_disallow_freq_mcc(dev, apdev):
    """P2PS connection with P2PS method - one station and disallow freqs with mcc"""
    with HWSimRadio(n_channels=2) as (radio, iface):
        wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
        wpas.interface_add(iface)

        if wpas.get_mcc() < 2:
            raise Exception("New radio does not support MCC")

        set_no_group_iface(dev[0], 0)
        set_no_group_iface(wpas, 0)

        try:
            dev[0].global_request("P2P_SET disallow_freq 2437")
            hapd1 = hostapd.add_ap(apdev[0],
                                   {"ssid": 'bss-channel-6', "channel": '6'})

            wpas.connect("bss-channel-6", key_mgmt="NONE", scan_freq="2437")

            tmpdev = [dev[0], wpas]
            (grp_ifname0, grp_ifname1, ifnames) = p2ps_connect_p2ps_method(tmpdev, keep_group=True)

            freq = dev[0].get_group_status_field('freq')
            if freq == '2437':
                raise Exception('Unexpected frequency=2437')
        finally:
            dev[0].global_request("P2P_SET disallow_freq ")
            dev[0].global_request("P2P_SERVICE_DEL asp all")
            remove_group(dev[0], wpas)

@remote_compatible
def test_p2ps_active_go_adv(dev, apdev):
    """P2PS connection with P2PS method - active GO on advertiser"""
    set_no_group_iface(dev[0], 0)
    set_no_group_iface(dev[1], 0)

    try:
        # Add a P2P GO
        dev[0].global_request("P2P_GROUP_ADD persistent")
        ev = dev[0].wait_global_event(["P2P-GROUP-STARTED"], timeout=10)
        if ev is None:
            raise Exception("P2P-GROUP-STARTED timeout on " + dev[0].p2p_dev_addr())

        dev[0].group_form_result(ev)

        p2ps_advertise(r_dev=dev[0], r_role='4',
                       svc_name='org.wi-fi.wfds.send.rx',
                       srv_info='I can receive files upto size 2 GB')
        [adv_id, rcvd_svc_name] = p2ps_exact_seek(i_dev=dev[1], r_dev=dev[0],
                                                  svc_name='org.wi-fi.wfds.send.rx',
                                                  single_peer_expected=False)

        ev1, ev0 = p2ps_provision(dev[1], dev[0], adv_id)

        # explicitly stop find/listen as otherwise the long listen started by
        # the advertiser would prevent the seeker to connect with the P2P GO
        dev[0].p2p_stop_find()
        p2ps_connect_pd(dev[0], dev[1], ev0, ev1)
    finally:
        dev[0].global_request("P2P_SERVICE_DEL asp all")
        remove_group(dev[0], dev[1])

@remote_compatible
def test_p2ps_active_go_seeker(dev, apdev):
    """P2PS connection with P2PS method - active GO on seeker"""
    set_no_group_iface(dev[0], 0)
    set_no_group_iface(dev[1], 0)

    try:
        # Add a P2P GO on the seeker
        dev[1].global_request("P2P_GROUP_ADD persistent")
        ev = dev[1].wait_global_event(["P2P-GROUP-STARTED"], timeout=10)
        if ev is None:
            raise Exception("P2P-GROUP-STARTED timeout on " + dev[1].p2p_dev_addr())

        res = dev[1].group_form_result(ev)

        p2ps_advertise(r_dev=dev[0], r_role='2',
                       svc_name='org.wi-fi.wfds.send.rx',
                       srv_info='I can receive files upto size 2 GB')
        [adv_id, rcvd_svc_name] = p2ps_exact_seek(i_dev=dev[1], r_dev=dev[0],
                                                  svc_name='org.wi-fi.wfds.send.rx',
                                                  srv_info='2 GB')

        ev1, ev0 = p2ps_provision(dev[1], dev[0], adv_id)
        p2ps_connect_pd(dev[0], dev[1], ev0, ev1,
                        join_extra=" freq=" + res['freq'])
    finally:
        dev[0].global_request("P2P_SERVICE_DEL asp all")
        remove_group(dev[0], dev[1])

def test_p2ps_channel_active_go_and_station_same(dev, apdev):
    """P2PS connection, active P2P GO and station on channel"""
    set_no_group_iface(dev[2], 0)
    set_no_group_iface(dev[1], 0)

    dev[2].global_request("P2P_SET listen_channel 11")
    dev[1].global_request("P2P_SET listen_channel 11")
    try:
        hapd = hostapd.add_ap(apdev[0],
                              {"ssid": 'bss-channel-11', "channel": '11'})

        dev[2].connect("bss-channel-11", key_mgmt="NONE", scan_freq="2462")

        # Add a P2P GO on the seeker
        dev[1].global_request("P2P_GROUP_ADD freq=2462 persistent")
        ev = dev[1].wait_global_event(["P2P-GROUP-STARTED"], timeout=10)
        if ev is None:
            raise Exception("P2P-GROUP-STARTED timeout on " + dev[1].p2p_dev_addr())

        dev[1].group_form_result(ev)

        p2ps_advertise(r_dev=dev[2], r_role='2',
                       svc_name='org.wi-fi.wfds.send.rx',
                       srv_info='I can receive files upto size 2 GB')
        [adv_id, rcvd_svc_name] = p2ps_exact_seek(i_dev=dev[1], r_dev=dev[2],
                                                  svc_name='org.wi-fi.wfds.send.rx',
                                                  srv_info='2 GB')

        ev1, ev0 = p2ps_provision(dev[1], dev[2], adv_id)
        p2ps_connect_pd(dev[2], dev[1], ev0, ev1, join_extra=" freq=2462")
    finally:
        dev[2].global_request("P2P_SERVICE_DEL asp all")
        for i in range(1, 3):
            set_random_listen_chan(dev[i])
        remove_group(dev[2], dev[1])

def test_p2ps_channel_active_go_and_station_different(dev, apdev):
    """P2PS connection, active P2P GO and station on channel"""
    if dev[0].get_mcc() > 1:
        raise HwsimSkip('Skip due to MCC being enabled')

    set_no_group_iface(dev[0], 0)
    set_no_group_iface(dev[1], 0)

    try:
        hapd = hostapd.add_ap(apdev[0],
                              {"ssid": 'bss-channel-2', "channel": '2'})

        dev[0].connect("bss-channel-2", key_mgmt="NONE", scan_freq="2417")

        # Add a P2P GO on the seeker. Force the listen channel to be the same,
        # as extended listen will not kick as long as P2P GO is waiting for
        # initial connection.
        dev[1].global_request("P2P_SET listen_channel 11")
        dev[1].global_request("P2P_GROUP_ADD freq=2462 persistent")
        ev = dev[1].wait_global_event(["P2P-GROUP-STARTED"], timeout=10)
        if ev is None:
            raise Exception("P2P-GROUP-STARTED timeout on " + dev[1].p2p_dev_addr())

        dev[1].group_form_result(ev)

        p2ps_advertise(r_dev=dev[0], r_role='2',
                       svc_name='org.wi-fi.wfds.send.rx',
                       srv_info='I can receive files upto size 2 GB')
        [adv_id, rcvd_svc_name] = p2ps_exact_seek(i_dev=dev[1], r_dev=dev[0],
                                                  svc_name='org.wi-fi.wfds.send.rx',
                                                  srv_info='2 GB')

        ev1, ev0 = p2ps_provision(dev[1], dev[0], adv_id, auto_accept=False,
                                  handler=disconnect_handler, adv_role='2',
                                  seeker_role='4')
        p2ps_connect_pd(dev[0], dev[1], ev0, ev1)
        freq = dev[0].get_group_status_field('freq')
        if freq != '2462':
            raise Exception('Unexpected frequency for group 2462!=' + freq)
    finally:
        dev[0].global_request("P2P_SERVICE_DEL asp all")
        set_random_listen_chan(dev[1])

@remote_compatible
def test_p2ps_channel_active_go_and_station_different_mcc(dev, apdev):
    """P2PS connection, active P2P GO and station on channel"""
    with HWSimRadio(n_channels=2) as (radio, iface):
        wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
        wpas.interface_add(iface)

        set_no_group_iface(wpas, 0)
        set_no_group_iface(dev[1], 0)

        try:
            hapd = hostapd.add_ap(apdev[0],
                                  {"ssid": 'bss-channel-6', "channel": '6'})

            wpas.global_request("P2P_SET listen_channel 1")
            wpas.connect("bss-channel-6", key_mgmt="NONE", scan_freq="2437")

            # Add a P2P GO on the seeker
            dev[1].global_request("P2P_SET listen_channel 1")
            dev[1].global_request("P2P_GROUP_ADD freq=2462 persistent")
            ev = dev[1].wait_global_event(["P2P-GROUP-STARTED"], timeout=10)
            if ev is None:
                raise Exception("P2P-GROUP-STARTED timeout on " + dev[1].p2p_dev_addr())

            dev[1].group_form_result(ev)

            p2ps_advertise(r_dev=wpas, r_role='2',
                           svc_name='org.wi-fi.wfds.send.rx',
                           srv_info='I can receive files upto size 2 GB')
            [adv_id, rcvd_svc_name] = p2ps_exact_seek(i_dev=dev[1], r_dev=wpas,
                                                      svc_name='org.wi-fi.wfds.send.rx',
                                                      srv_info='2 GB')

            ev1, ev0 = p2ps_provision(dev[1], wpas, adv_id)
            p2ps_connect_pd(wpas, dev[1], ev0, ev1)
        finally:
            set_random_listen_chan(dev[1])
            set_random_listen_chan(wpas)
            wpas.request("DISCONNECT")
            hapd.disable()
            wpas.global_request("P2P_SERVICE_DEL asp all")
            remove_group(wpas, dev[1], allow_failure=True)

def test_p2ps_connect_p2p_device(dev):
    """P2PS connection using cfg80211 P2P Device"""
    run_p2ps_connect_p2p_device(dev, 0)

def test_p2ps_connect_p2p_device_no_group_iface(dev):
    """P2PS connection using cfg80211 P2P Device (no separate group interface)"""
    run_p2ps_connect_p2p_device(dev, 1)

def run_p2ps_connect_p2p_device(dev, no_group_iface):
    with HWSimRadio(use_p2p_device=True) as (radio, iface):
        wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
        wpas.interface_add(iface)
        wpas.global_request("SET p2p_no_group_iface %d" % no_group_iface)

        p2ps_advertise(r_dev=dev[0], r_role='1',
                       svc_name='org.wi-fi.wfds.send.rx',
                       srv_info='I can receive files upto size 2 GB')
        [adv_id, rcvd_svc_name] = p2ps_exact_seek(i_dev=wpas, r_dev=dev[0],
                                                  svc_name='org.wi-fi.wfds.send.rx',
                                                  srv_info='2 GB')

        ev1, ev0 = p2ps_provision(wpas, dev[0], adv_id)
        p2ps_connect_pd(dev[0], wpas, ev0, ev1)

        ev0 = dev[0].global_request("P2P_SERVICE_DEL asp " + str(adv_id))
        if ev0 is None:
            raise Exception("Unable to remove the advertisement instance")
        remove_group(dev[0], wpas)

def test_p2ps_connect_p2p_device2(dev):
    """P2PS connection using cfg80211 P2P Device (reverse)"""
    run_p2ps_connect_p2p_device2(dev, 0)

def test_p2ps_connect_p2p_device2_no_group_iface(dev):
    """P2PS connection using cfg80211 P2P Device (reverse) (no separate group interface)"""
    run_p2ps_connect_p2p_device2(dev, 1)

def run_p2ps_connect_p2p_device2(dev, no_group_iface):
    with HWSimRadio(use_p2p_device=True) as (radio, iface):
        wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
        wpas.interface_add(iface)
        wpas.global_request("SET p2p_no_group_iface %d" % no_group_iface)

        p2ps_advertise(r_dev=wpas, r_role='1',
                       svc_name='org.wi-fi.wfds.send.rx',
                       srv_info='I can receive files upto size 2 GB')
        [adv_id, rcvd_svc_name] = p2ps_exact_seek(i_dev=dev[0], r_dev=wpas,
                                                  svc_name='org.wi-fi.wfds.send.rx',
                                                  srv_info='2 GB')

        ev1, ev0 = p2ps_provision(dev[0], wpas, adv_id)
        p2ps_connect_pd(wpas, dev[0], ev0, ev1)

        ev0 = wpas.global_request("P2P_SERVICE_DEL asp " + str(adv_id))
        if ev0 is None:
            raise Exception("Unable to remove the advertisement instance")
        remove_group(wpas, dev[0])

@remote_compatible
def test_p2ps_connect_p2ps_method_no_pin(dev):
    """P2P group formation using P2PS method without specifying PIN"""
    dev[0].p2p_listen()
    dev[1].p2p_go_neg_auth(dev[0].p2p_dev_addr(), None, "p2ps", go_intent=15)
    dev[1].p2p_listen()
    i_res = dev[0].p2p_go_neg_init(dev[1].p2p_dev_addr(), None, "p2ps",
                                   timeout=20, go_intent=0)
    r_res = dev[1].p2p_go_neg_auth_result()
    logger.debug("i_res: " + str(i_res))
    logger.debug("r_res: " + str(r_res))
    check_grpform_results(i_res, r_res)
    remove_group(dev[0], dev[1])
