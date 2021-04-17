# P2P service discovery test cases
# Copyright (c) 2013, Jouni Malinen <j@w1.fi>
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

from remotehost import remote_compatible
import logging
logger = logging.getLogger()
import os
import time
import uuid

import hwsim_utils

def add_bonjour_services(dev):
    dev.global_request("P2P_SERVICE_ADD bonjour 0b5f6166706f766572746370c00c000c01 074578616d706c65c027")
    dev.global_request("P2P_SERVICE_ADD bonjour 076578616d706c650b5f6166706f766572746370c00c001001 00")
    dev.global_request("P2P_SERVICE_ADD bonjour 045f697070c00c000c01 094d795072696e746572c027")
    dev.global_request("P2P_SERVICE_ADD bonjour 096d797072696e746572045f697070c00c001001 09747874766572733d311a70646c3d6170706c69636174696f6e2f706f7374736372797074")

def add_upnp_services(dev):
    dev.global_request("P2P_SERVICE_ADD upnp 10 uuid:6859dede-8574-59ab-9332-123456789012::upnp:rootdevice")
    dev.global_request("P2P_SERVICE_ADD upnp 10 uuid:5566d33e-9774-09ab-4822-333456785632::upnp:rootdevice")
    dev.global_request("P2P_SERVICE_ADD upnp 10 uuid:1122de4e-8574-59ab-9322-333456789044::urn:schemas-upnp-org:service:ContentDirectory:2")
    dev.global_request("P2P_SERVICE_ADD upnp 10 uuid:5566d33e-9774-09ab-4822-333456785632::urn:schemas-upnp-org:service:ContentDirectory:2")
    dev.global_request("P2P_SERVICE_ADD upnp 10 uuid:6859dede-8574-59ab-9332-123456789012::urn:schemas-upnp-org:device:InternetGatewayDevice:1")

def add_extra_services(dev):
    for i in range(0, 100):
        dev.global_request("P2P_SERVICE_ADD upnp 10 uuid:" + str(uuid.uuid4()) + "::upnp:rootdevice")

def run_sd(dev, dst, query, exp_query=None, fragment=False, query2=None):
    addr0 = dev[0].p2p_dev_addr()
    addr1 = dev[1].p2p_dev_addr()
    add_bonjour_services(dev[0])
    add_upnp_services(dev[0])
    if fragment:
        add_extra_services(dev[0])
    dev[0].p2p_listen()

    dev[1].global_request("P2P_FLUSH")
    dev[1].global_request("P2P_SERV_DISC_REQ " + dst + " " + query)
    if query2:
        dev[1].global_request("P2P_SERV_DISC_REQ " + dst + " " + query2)
    if not dev[1].discover_peer(addr0, social=True, force_find=True):
        raise Exception("Peer " + addr0 + " not found")

    ev = dev[0].wait_global_event(["P2P-SERV-DISC-REQ"], timeout=10)
    if ev is None:
        raise Exception("Service discovery timed out")
    if addr1 not in ev:
        raise Exception("Unexpected service discovery request source")
    if exp_query is None:
        exp_query = query
    if exp_query not in ev and (query2 is None or query2 not in ev):
        raise Exception("Unexpected service discovery request contents")

    if query2:
        ev_list = []
        for i in range(0, 4):
            ev = dev[1].wait_global_event(["P2P-SERV-DISC-RESP"], timeout=10)
            if ev is None:
                raise Exception("Service discovery timed out")
            if addr0 in ev:
                ev_list.append(ev)
                if len(ev_list) == 2:
                    break
        return ev_list

    for i in range(0, 2):
        ev = dev[1].wait_global_event(["P2P-SERV-DISC-RESP"], timeout=10)
        if ev is None:
            raise Exception("Service discovery timed out")
        if addr0 in ev:
            break

    dev[0].p2p_stop_find()
    dev[1].p2p_stop_find()

    if "OK" not in dev[0].global_request("P2P_SERVICE_DEL upnp 10 uuid:6859dede-8574-59ab-9332-123456789012::upnp:rootdevice"):
        raise Exception("Failed to delete a UPnP service")
    if "FAIL" not in dev[0].global_request("P2P_SERVICE_DEL upnp 10 uuid:6859dede-8574-59ab-9332-123456789012::upnp:rootdevice"):
        raise Exception("Unexpected deletion success for UPnP service")
    if "OK" not in dev[0].global_request("P2P_SERVICE_DEL bonjour 0b5f6166706f766572746370c00c000c01"):
        raise Exception("Failed to delete a Bonjour service")
    if "FAIL" not in dev[0].global_request("P2P_SERVICE_DEL bonjour 0b5f6166706f766572746370c00c000c01"):
        raise Exception("Unexpected deletion success for Bonjour service")

    return ev

@remote_compatible
def test_p2p_service_discovery(dev):
    """P2P service discovery"""
    addr0 = dev[0].p2p_dev_addr()
    for dst in ["00:00:00:00:00:00", addr0]:
        ev = run_sd(dev, dst, "02000001")
        if "0b5f6166706f766572746370c00c000c01" not in ev:
            raise Exception("Unexpected service discovery response contents (Bonjour)")
        if "496e7465726e6574" not in ev:
            raise Exception("Unexpected service discovery response contents (UPnP)")

    for req in ["foo 02000001",
                addr0,
                addr0 + " upnp qq urn:schemas-upnp-org:device:InternetGatewayDevice:1",
                addr0 + " upnp 10",
                addr0 + " 123",
                addr0 + " qq"]:
        if "FAIL" not in dev[1].global_request("P2P_SERV_DISC_REQ " + req):
            raise Exception("Invalid P2P_SERV_DISC_REQ accepted: " + req)

def test_p2p_service_discovery2(dev):
    """P2P service discovery with one peer having no services"""
    dev[2].p2p_listen()
    for dst in ["00:00:00:00:00:00", dev[0].p2p_dev_addr()]:
        ev = run_sd(dev, dst, "02000001")
        if "0b5f6166706f766572746370c00c000c01" not in ev:
            raise Exception("Unexpected service discovery response contents (Bonjour)")
        if "496e7465726e6574" not in ev:
            raise Exception("Unexpected service discovery response contents (UPnP)")

def test_p2p_service_discovery3(dev):
    """P2P service discovery for Bonjour with one peer having no services"""
    dev[2].p2p_listen()
    for dst in ["00:00:00:00:00:00", dev[0].p2p_dev_addr()]:
        ev = run_sd(dev, dst, "02000101")
        if "0b5f6166706f766572746370c00c000c01" not in ev:
            raise Exception("Unexpected service discovery response contents (Bonjour)")

def test_p2p_service_discovery4(dev):
    """P2P service discovery for UPnP with one peer having no services"""
    dev[2].p2p_listen()
    for dst in ["00:00:00:00:00:00", dev[0].p2p_dev_addr()]:
        ev = run_sd(dev, dst, "02000201")
        if "496e7465726e6574" not in ev:
            raise Exception("Unexpected service discovery response contents (UPnP)")

@remote_compatible
def test_p2p_service_discovery_multiple_queries(dev):
    """P2P service discovery with multiple queries"""
    for dst in ["00:00:00:00:00:00", dev[0].p2p_dev_addr()]:
        ev = run_sd(dev, dst, "02000201", query2="02000101")
        if "0b5f6166706f766572746370c00c000c01" not in ev[0] + ev[1]:
            raise Exception("Unexpected service discovery response contents (Bonjour)")
        if "496e7465726e6574" not in ev[0] + ev[1]:
            raise Exception("Unexpected service discovery response contents (UPnP)")

def test_p2p_service_discovery_multiple_queries2(dev):
    """P2P service discovery with multiple queries with one peer having no services"""
    dev[2].p2p_listen()
    for dst in ["00:00:00:00:00:00", dev[0].p2p_dev_addr()]:
        ev = run_sd(dev, dst, "02000201", query2="02000101")
        if "0b5f6166706f766572746370c00c000c01" not in ev[0] + ev[1]:
            raise Exception("Unexpected service discovery response contents (Bonjour)")
        if "496e7465726e6574" not in ev[0] + ev[1]:
            raise Exception("Unexpected service discovery response contents (UPnP)")

def test_p2p_service_discovery_fragmentation(dev):
    """P2P service discovery with fragmentation"""
    for dst in ["00:00:00:00:00:00", dev[0].p2p_dev_addr()]:
        ev = run_sd(dev, dst, "02000001", fragment=True)
        if "long response" not in ev:
            if "0b5f6166706f766572746370c00c000c01" not in ev:
                raise Exception("Unexpected service discovery response contents (Bonjour)")
            if "496e7465726e6574" not in ev:
                raise Exception("Unexpected service discovery response contents (UPnP)")

@remote_compatible
def test_p2p_service_discovery_bonjour(dev):
    """P2P service discovery (Bonjour)"""
    ev = run_sd(dev, "00:00:00:00:00:00", "02000101")
    if "0b5f6166706f766572746370c00c000c01" not in ev:
        raise Exception("Unexpected service discovery response contents (Bonjour)")
    if "045f697070c00c000c01" not in ev:
        raise Exception("Unexpected service discovery response contents (Bonjour)")
    if "496e7465726e6574" in ev:
        raise Exception("Unexpected service discovery response contents (UPnP not expected)")

@remote_compatible
def test_p2p_service_discovery_bonjour2(dev):
    """P2P service discovery (Bonjour AFS)"""
    ev = run_sd(dev, "00:00:00:00:00:00", "130001010b5f6166706f766572746370c00c000c01")
    if "0b5f6166706f766572746370c00c000c01" not in ev:
        raise Exception("Unexpected service discovery response contents (Bonjour)")
    if "045f697070c00c000c01" in ev:
        raise Exception("Unexpected service discovery response contents (Bonjour mismatching)")
    if "496e7465726e6574" in ev:
        raise Exception("Unexpected service discovery response contents (UPnP not expected)")

@remote_compatible
def test_p2p_service_discovery_bonjour3(dev):
    """P2P service discovery (Bonjour AFS - no match)"""
    ev = run_sd(dev, "00:00:00:00:00:00", "130001010b5f6166706f766572746370c00c000c02")
    if "0300010102" not in ev:
        raise Exception("Requested-info-not-available was not indicated")
    if "0b5f6166706f766572746370c00c000c01" in ev:
        raise Exception("Unexpected service discovery response contents (Bonjour)")
    if "045f697070c00c000c01" in ev:
        raise Exception("Unexpected service discovery response contents (Bonjour mismatching)")
    if "496e7465726e6574" in ev:
        raise Exception("Unexpected service discovery response contents (UPnP not expected)")

@remote_compatible
def test_p2p_service_discovery_upnp(dev):
    """P2P service discovery (UPnP)"""
    ev = run_sd(dev, "00:00:00:00:00:00", "02000201")
    if "0b5f6166706f766572746370c00c000c01" in ev:
        raise Exception("Unexpected service discovery response contents (Bonjour not expected)")
    if "496e7465726e6574" not in ev:
        raise Exception("Unexpected service discovery response contents (UPnP)")

@remote_compatible
def test_p2p_service_discovery_upnp2(dev):
    """P2P service discovery (UPnP using request helper)"""
    ev = run_sd(dev, "00:00:00:00:00:00", "upnp 10 ssdp:all", "0b00020110737364703a616c6c")
    if "0b5f6166706f766572746370c00c000c01" in ev:
        raise Exception("Unexpected service discovery response contents (Bonjour not expected)")
    if "496e7465726e6574" not in ev:
        raise Exception("Unexpected service discovery response contents (UPnP)")

@remote_compatible
def test_p2p_service_discovery_upnp3(dev):
    """P2P service discovery (UPnP using request helper - no match)"""
    ev = run_sd(dev, "00:00:00:00:00:00", "upnp 10 ssdp:foo", "0b00020110737364703a666f6f")
    if "0300020102" not in ev:
        raise Exception("Requested-info-not-available was not indicated")
    if "0b5f6166706f766572746370c00c000c01" in ev:
        raise Exception("Unexpected service discovery response contents (Bonjour not expected)")
    if "496e7465726e6574" in ev:
        raise Exception("Unexpected service discovery response contents (UPnP)")

@remote_compatible
def test_p2p_service_discovery_ws(dev):
    """P2P service discovery (WS-Discovery)"""
    ev = run_sd(dev, "00:00:00:00:00:00", "02000301")
    if "0b5f6166706f766572746370c00c000c01" in ev:
        raise Exception("Unexpected service discovery response contents (Bonjour not expected)")
    if "496e7465726e6574" in ev:
        raise Exception("Unexpected service discovery response contents (UPnP not expected)")
    if "0300030101" not in ev:
        raise Exception("Unexpected service discovery response contents (WS)")

@remote_compatible
def test_p2p_service_discovery_wfd(dev):
    """P2P service discovery (Wi-Fi Display)"""
    dev[0].global_request("SET wifi_display 1")
    ev = run_sd(dev, "00:00:00:00:00:00", "02000401")
    if " 030004" in ev:
        raise Exception("Unexpected response to invalid WFD SD query")
    dev[0].global_request("SET wifi_display 0")
    ev = run_sd(dev, "00:00:00:00:00:00", "0300040100")
    if "0300040101" not in ev:
        raise Exception("Unexpected response to WFD SD query (protocol was disabled)")

@remote_compatible
def test_p2p_service_discovery_req_cancel(dev):
    """Cancel a P2P service discovery request"""
    if "FAIL" not in dev[0].global_request("P2P_SERV_DISC_CANCEL_REQ ab"):
        raise Exception("Unexpected SD cancel success")
    if "FAIL" not in dev[0].global_request("P2P_SERV_DISC_CANCEL_REQ qq"):
        raise Exception("Unexpected SD cancel success")
    query = dev[0].global_request("P2P_SERV_DISC_REQ " + dev[1].p2p_dev_addr() + " 02000001")
    if "OK" not in dev[0].global_request("P2P_SERV_DISC_CANCEL_REQ " + query):
        raise Exception("Unexpected SD cancel failure")
    query1 = dev[0].global_request("P2P_SERV_DISC_REQ " + dev[1].p2p_dev_addr() + " 02000001")
    query2 = dev[0].global_request("P2P_SERV_DISC_REQ " + dev[1].p2p_dev_addr() + " 02000002")
    query3 = dev[0].global_request("P2P_SERV_DISC_REQ " + dev[1].p2p_dev_addr() + " 02000003")
    if "OK" not in dev[0].global_request("P2P_SERV_DISC_CANCEL_REQ " + query2):
        raise Exception("Unexpected SD cancel failure")
    if "OK" not in dev[0].global_request("P2P_SERV_DISC_CANCEL_REQ " + query1):
        raise Exception("Unexpected SD cancel failure")
    if "OK" not in dev[0].global_request("P2P_SERV_DISC_CANCEL_REQ " + query3):
        raise Exception("Unexpected SD cancel failure")

    query = dev[0].global_request("P2P_SERV_DISC_REQ 00:00:00:00:00:00 02000001")
    if "OK" not in dev[0].global_request("P2P_SERV_DISC_CANCEL_REQ " + query):
        raise Exception("Unexpected SD(broadcast) cancel failure")

@remote_compatible
def test_p2p_service_discovery_go(dev):
    """P2P service discovery from GO"""
    addr0 = dev[0].p2p_dev_addr()
    addr1 = dev[1].p2p_dev_addr()

    add_bonjour_services(dev[0])
    add_upnp_services(dev[0])

    dev[0].p2p_start_go(freq=2412)

    dev[1].global_request("P2P_FLUSH")
    dev[1].global_request("P2P_SERV_DISC_REQ " + addr0 + " 02000001")
    if not dev[1].discover_peer(addr0, social=True, force_find=True):
        raise Exception("Peer " + addr0 + " not found")

    ev = dev[0].wait_global_event(["P2P-SERV-DISC-REQ"], timeout=10)
    if ev is None:
        raise Exception("Service discovery timed out")
    if addr1 not in ev:
        raise Exception("Unexpected service discovery request source")

    ev = dev[1].wait_global_event(["P2P-SERV-DISC-RESP"], timeout=10)
    if ev is None:
        raise Exception("Service discovery timed out")
    if addr0 not in ev:
        raise Exception("Unexpected service discovery response source")
    if "0b5f6166706f766572746370c00c000c01" not in ev:
        raise Exception("Unexpected service discovery response contents (Bonjour)")
    if "496e7465726e6574" not in ev:
        raise Exception("Unexpected service discovery response contents (UPnP)")
    dev[1].p2p_stop_find()

    dev[0].global_request("P2P_SERVICE_FLUSH")

    dev[1].global_request("P2P_FLUSH")
    dev[1].global_request("P2P_SERV_DISC_REQ " + addr0 + " 02000001")
    if not dev[1].discover_peer(addr0, social=True, force_find=True):
        raise Exception("Peer " + addr0 + " not found")
    ev = dev[0].wait_global_event(["P2P-SERV-DISC-REQ"], timeout=10)
    if ev is None:
        raise Exception("Service discovery timed out")
    if addr1 not in ev:
        raise Exception("Unexpected service discovery request source")

    ev = dev[1].wait_global_event(["P2P-SERV-DISC-RESP"], timeout=10)
    if ev is None:
        raise Exception("Service discovery timed out")
    if addr0 not in ev:
        raise Exception("Unexpected service discovery response source")
    if "0300000101" not in ev:
        raise Exception("Unexpected service discovery response contents (Bonjour)")
    dev[1].p2p_stop_find()

def _test_p2p_service_discovery_external(dev):
    addr0 = dev[0].p2p_dev_addr()
    addr1 = dev[1].p2p_dev_addr()

    if "FAIL" not in dev[0].global_request("P2P_SERV_DISC_EXTERNAL 2"):
        raise Exception("Invalid P2P_SERV_DISC_EXTERNAL accepted")
    if "OK" not in dev[0].global_request("P2P_SERV_DISC_EXTERNAL 1"):
        raise Exception("P2P_SERV_DISC_EXTERNAL failed")
    dev[0].p2p_listen()
    dev[1].global_request("P2P_FLUSH")
    dev[1].global_request("P2P_SERV_DISC_REQ " + addr0 + " 02000001")
    if not dev[1].discover_peer(addr0, social=True, force_find=True):
        raise Exception("Peer " + addr0 + " not found")

    ev = dev[0].wait_global_event(["P2P-SERV-DISC-REQ"], timeout=10)
    if ev is None:
        raise Exception("Service discovery timed out")
    if addr1 not in ev:
        raise Exception("Unexpected service discovery request source")
    arg = ev.split(' ')
    resp = "0300000101"
    if "OK" not in dev[0].global_request("P2P_SERV_DISC_RESP %s %s %s %s" % (arg[2], arg[3], arg[4], resp)):
        raise Exception("P2P_SERV_DISC_RESP failed")

    ev = dev[1].wait_global_event(["P2P-SERV-DISC-RESP"], timeout=15)
    if ev is None:
        raise Exception("Service discovery timed out")
    if addr0 not in ev:
        raise Exception("Unexpected address in SD Response: " + ev)
    if ev.split(' ')[4] != resp:
        raise Exception("Unexpected response data SD Response: " + ev)
    ver = ev.split(' ')[3]

    dev[0].global_request("P2P_SERVICE_UPDATE")

    dev[1].global_request("P2P_FLUSH")
    dev[1].global_request("P2P_SERV_DISC_REQ " + addr0 + " 02000001")
    if not dev[1].discover_peer(addr0, social=True, force_find=True):
        raise Exception("Peer " + addr0 + " not found")

    ev = dev[0].wait_global_event(["P2P-SERV-DISC-REQ"], timeout=10)
    if ev is None:
        raise Exception("Service discovery timed out")
    if addr1 not in ev:
        raise Exception("Unexpected service discovery request source")
    arg = ev.split(' ')
    resp = "0300000101"
    if "OK" not in dev[0].global_request("P2P_SERV_DISC_RESP %s %s %s %s" % (arg[2], arg[3], arg[4], resp)):
        raise Exception("P2P_SERV_DISC_RESP failed")

    ev = dev[1].wait_global_event(["P2P-SERV-DISC-RESP"], timeout=15)
    if ev is None:
        raise Exception("Service discovery timed out")
    if addr0 not in ev:
        raise Exception("Unexpected address in SD Response: " + ev)
    if ev.split(' ')[4] != resp:
        raise Exception("Unexpected response data SD Response: " + ev)
    ver2 = ev.split(' ')[3]
    if ver == ver2:
        raise Exception("Service list version did not change")

    for cmd in ["%s%s%s%s" % (arg[2], arg[3], arg[4], resp),
                "%s %s %s %s" % ("0", arg[3], arg[4], resp),
                "%s %s %s %s" % (arg[2], "foo", arg[4], resp),
                "%s %s%s%s" % (arg[2], arg[3], arg[4], resp),
                "%s %s %s%s" % (arg[2], arg[3], arg[4], resp),
                "%s %s %s %s" % (arg[2], arg[3], arg[4], "12345"),
                "%s %s %s %s" % (arg[2], arg[3], arg[4], "qq")]:
        if "FAIL" not in dev[0].global_request("P2P_SERV_DISC_RESP " + cmd):
            raise Exception("Invalid P2P_SERV_DISC_RESP accepted: " + cmd)

@remote_compatible
def test_p2p_service_discovery_external(dev):
    """P2P service discovery using external response"""
    try:
        _test_p2p_service_discovery_external(dev)
    finally:
        dev[0].global_request("P2P_SERV_DISC_EXTERNAL 0")

@remote_compatible
def test_p2p_service_discovery_invalid_commands(dev):
    """P2P service discovery invalid commands"""
    for cmd in ["bonjour",
                "bonjour 12",
                "bonjour 123 12",
                "bonjour qq 12",
                "bonjour 12 123",
                "bonjour 12 qq",
                "upnp 10",
                "upnp qq uuid:",
                "foo bar"]:
        if "FAIL" not in dev[0].global_request("P2P_SERVICE_ADD " + cmd):
            raise Exception("Invalid P2P_SERVICE_ADD accepted: " + cmd)

    for cmd in ["bonjour",
                "bonjour 123",
                "bonjour qq",
                "upnp 10",
                "upnp  ",
                "upnp qq uuid:",
                "foo bar"]:
        if "FAIL" not in dev[0].global_request("P2P_SERVICE_DEL " + cmd):
            raise Exception("Invalid P2P_SERVICE_DEL accepted: " + cmd)

def test_p2p_service_discovery_cancel_during_query(dev):
    """P2P service discovery and cancel during query"""
    for i in range(2):
        add_bonjour_services(dev[i])
        add_upnp_services(dev[i])
        add_extra_services(dev[i])
        dev[i].p2p_listen()

    dev[2].request("P2P_FLUSH")
    id1 = dev[2].request("P2P_SERV_DISC_REQ 00:00:00:00:00:00 02000201")
    id2 = dev[2].request("P2P_SERV_DISC_REQ 00:00:00:00:00:00 02000101")
    dev[2].p2p_find(social=True)
    ev = dev[2].wait_global_event(["P2P-DEVICE-FOUND"], timeout=15)
    if ev is None:
        raise Exception("Could not discover peer")
    if "OK" not in dev[2].request("P2P_SERV_DISC_CANCEL_REQ " + id1):
        raise Exception("Failed to cancel req1")
    if "OK" not in dev[2].request("P2P_SERV_DISC_CANCEL_REQ " + id2):
        raise Exception("Failed to cancel req2")
    ev = dev[2].wait_global_event(["P2P-SERV-DISC-RESP"], timeout=3)
    # we may or may not get a response depending on timing, so ignore the result
    dev[2].p2p_stop_find()
    dev[1].p2p_stop_find()
    dev[0].p2p_stop_find()

def get_p2p_state(dev):
    res = dev.global_request("STATUS")
    p2p_state = None
    for line in res.splitlines():
        if line.startswith("p2p_state="):
            p2p_state = line.split('=')[1]
            break
    if p2p_state is None:
        raise Exception("Could not get p2p_state")
    return p2p_state

@remote_compatible
def test_p2p_service_discovery_peer_not_listening(dev):
    """P2P service discovery and peer not listening"""
    addr0 = dev[0].p2p_dev_addr()
    addr1 = dev[1].p2p_dev_addr()
    add_bonjour_services(dev[0])
    add_upnp_services(dev[0])
    dev[0].p2p_listen()
    dev[1].global_request("P2P_FIND 4 type=social")
    ev = dev[1].wait_global_event(["P2P-DEVICE-FOUND"], timeout=4)
    if ev is None:
        raise Exception("Peer not found")
    dev[0].p2p_stop_find()
    ev = dev[1].wait_event(["CTRL-EVENT-SCAN-STARTED"], timeout=1)
    ev = dev[1].wait_event(["CTRL-EVENT-SCAN-STARTED"], timeout=1)
    time.sleep(0.03)
    dev[1].request("P2P_SERV_DISC_REQ " + addr0 + " 02000001")
    ev = dev[0].wait_global_event(["P2P-SERV-DISC-REQ"], timeout=1)
    if ev is not None:
        raise Exception("Service discovery request unexpectedly received")
    ev = dev[1].wait_global_event(["P2P-FIND-STOPPED", "P2P-SERV-DISC-RESP"],
                                  timeout=10)
    if ev is None:
        raise Exception("P2P-FIND-STOPPED event timed out")
    if "P2P-SERV-DISC-RESP" in ev:
        raise Exception("Unexpected SD response")
    p2p_state = get_p2p_state(dev[1])
    if p2p_state != "IDLE":
        raise Exception("Unexpected p2p_state after P2P_FIND timeout: " + p2p_state)

@remote_compatible
def test_p2p_service_discovery_peer_not_listening2(dev):
    """P2P service discovery and peer not listening"""
    addr0 = dev[0].p2p_dev_addr()
    addr1 = dev[1].p2p_dev_addr()
    add_bonjour_services(dev[0])
    add_upnp_services(dev[0])
    dev[0].p2p_listen()
    dev[1].global_request("P2P_FIND type=social")
    ev = dev[1].wait_global_event(["P2P-DEVICE-FOUND"], timeout=10)
    if ev is None:
        raise Exception("Peer not found")
    dev[0].p2p_stop_find()
    time.sleep(0.53)
    dev[1].request("P2P_SERV_DISC_REQ " + addr0 + " 02000001")
    ev = dev[0].wait_global_event(["P2P-SERV-DISC-REQ"], timeout=0.5)
    if ev is not None:
        raise Exception("Service discovery request unexpectedly received")
    dev[1].p2p_stop_find()
    ev = dev[1].wait_global_event(["P2P-FIND-STOPPED", "P2P-SERV-DISC-RESP"],
                                  timeout=10)
    if ev is None:
        raise Exception("P2P-FIND-STOPPED event timed out")
    if "P2P-SERV-DISC-RESP" in ev:
        raise Exception("Unexpected SD response")
    p2p_state = get_p2p_state(dev[1])
    if p2p_state != "IDLE":
        raise Exception("Unexpected p2p_state after P2P_FIND timeout: " + p2p_state)

def test_p2p_service_discovery_restart(dev):
    """P2P service discovery restarted immediately"""
    try:
        _test_p2p_service_discovery_restart(dev)
    finally:
        dev[1].global_request("P2P_SET disc_int 1 3 -1")

def _test_p2p_service_discovery_restart(dev):
    addr0 = dev[0].p2p_dev_addr()
    addr1 = dev[1].p2p_dev_addr()

    # Use shorter listen interval to keep P2P_FIND loop shorter.
    dev[1].global_request("P2P_SET disc_int 1 1 10")

    add_bonjour_services(dev[0])
    #add_upnp_services(dev[0])
    dev[0].p2p_listen()

    dev[1].global_request("P2P_FLUSH")
    dev[1].global_request("P2P_SERV_DISC_REQ " + addr0 + " 02000001")
    if not dev[1].discover_peer(addr0, social=True, force_find=True):
        raise Exception("Peer " + addr0 + " not found")

    ev = dev[1].wait_global_event(["P2P-SERV-DISC-RESP"], timeout=10)
    if ev is None:
        raise Exception("Service discovery timed out")

    # The following P2P_LISTEN operation used to get delayed due to the last
    # Action frame TX operation in SD Response using wait_time of 200 ms. It is
    # somewhat difficult to test for this automatically, but the debug log can
    # be verified to see that the remain-on-channel event for operation arrives
    # immediately instead of getting delayed 200 ms. We can use a maximum
    # acceptable time for the SD Response, but need to keep the limit somewhat
    # high to avoid making this fail under heavy load. Still, it is apparently
    # possible for this to take about the same amount of time with fixed
    # implementation every now and then, so run this multiple time and pass the
    # test if any attempt is fast enough.

    for i in range(10):
        dev[0].p2p_stop_find()
        time.sleep(0.01)
        dev[0].p2p_listen()

        dev[1].global_request("P2P_SERV_DISC_REQ " + addr0 + " 02000001")
        start = os.times()[4]
        ev = dev[1].wait_global_event(["P2P-SERV-DISC-RESP"], timeout=10)
        if ev is None:
            raise Exception("Service discovery timed out")
        end = os.times()[4]
        logger.info("Second SD Response in " + str(end - start) + " seconds")
        if end - start < 0.8:
            break

    if end - start > 0.8:
        raise Exception("Unexpectedly slow second SD Response: " + str(end - start) + " seconds")
