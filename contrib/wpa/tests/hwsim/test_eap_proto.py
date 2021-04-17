# EAP protocol tests
# Copyright (c) 2014-2015, Jouni Malinen <j@w1.fi>
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

import binascii
import hashlib
import hmac
import logging
logger = logging.getLogger()
import os
import select
import struct
import threading
import time

import hostapd
from utils import *
from test_ap_eap import check_eap_capa, check_hlr_auc_gw_support, int_eap_server_params

try:
    import OpenSSL
    openssl_imported = True
except ImportError:
    openssl_imported = False

EAP_CODE_REQUEST = 1
EAP_CODE_RESPONSE = 2
EAP_CODE_SUCCESS = 3
EAP_CODE_FAILURE = 4
EAP_CODE_INITIATE = 5
EAP_CODE_FINISH = 6

EAP_TYPE_IDENTITY = 1
EAP_TYPE_NOTIFICATION = 2
EAP_TYPE_NAK = 3
EAP_TYPE_MD5 = 4
EAP_TYPE_OTP = 5
EAP_TYPE_GTC = 6
EAP_TYPE_TLS = 13
EAP_TYPE_LEAP = 17
EAP_TYPE_SIM = 18
EAP_TYPE_TTLS = 21
EAP_TYPE_AKA = 23
EAP_TYPE_PEAP = 25
EAP_TYPE_MSCHAPV2 = 26
EAP_TYPE_TLV = 33
EAP_TYPE_TNC = 38
EAP_TYPE_FAST = 43
EAP_TYPE_PAX = 46
EAP_TYPE_PSK = 47
EAP_TYPE_SAKE = 48
EAP_TYPE_IKEV2 = 49
EAP_TYPE_AKA_PRIME = 50
EAP_TYPE_GPSK = 51
EAP_TYPE_PWD = 52
EAP_TYPE_EKE = 53
EAP_TYPE_EXPANDED = 254

# Type field in EAP-Initiate and EAP-Finish messages
EAP_ERP_TYPE_REAUTH_START = 1
EAP_ERP_TYPE_REAUTH = 2

EAP_ERP_TLV_KEYNAME_NAI = 1
EAP_ERP_TV_RRK_LIFETIME = 2
EAP_ERP_TV_RMSK_LIFETIME = 3
EAP_ERP_TLV_DOMAIN_NAME = 4
EAP_ERP_TLV_CRYPTOSUITES = 5
EAP_ERP_TLV_AUTHORIZATION_INDICATION = 6
EAP_ERP_TLV_CALLED_STATION_ID = 128
EAP_ERP_TLV_CALLING_STATION_ID = 129
EAP_ERP_TLV_NAS_IDENTIFIER = 130
EAP_ERP_TLV_NAS_IP_ADDRESS = 131
EAP_ERP_TLV_NAS_IPV6_ADDRESS = 132

def run_pyrad_server(srv, t_stop, eap_handler):
    srv.RunWithStop(t_stop, eap_handler)

def start_radius_server(eap_handler):
    try:
        import pyrad.server
        import pyrad.packet
        import pyrad.dictionary
    except ImportError:
        raise HwsimSkip("No pyrad modules available")

    class TestServer(pyrad.server.Server):
        def _HandleAuthPacket(self, pkt):
            pyrad.server.Server._HandleAuthPacket(self, pkt)
            eap = b''
            for p in pkt[79]:
                eap += p
            eap_req = self.eap_handler(self.ctx, eap)
            reply = self.CreateReplyPacket(pkt)
            if eap_req:
                while True:
                    if len(eap_req) > 253:
                        reply.AddAttribute("EAP-Message", eap_req[0:253])
                        eap_req = eap_req[253:]
                    else:
                        reply.AddAttribute("EAP-Message", eap_req)
                        break
            else:
                logger.info("No EAP request available")
            reply.code = pyrad.packet.AccessChallenge

            hmac_obj = hmac.new(reply.secret, digestmod=hashlib.md5)
            hmac_obj.update(struct.pack("B", reply.code))
            hmac_obj.update(struct.pack("B", reply.id))

            # reply attributes
            reply.AddAttribute("Message-Authenticator", 16*b'\x00')
            attrs = reply._PktEncodeAttributes()

            # Length
            flen = 4 + 16 + len(attrs)
            hmac_obj.update(struct.pack(">H", flen))
            hmac_obj.update(pkt.authenticator)
            hmac_obj.update(attrs)
            del reply[80]
            reply.AddAttribute("Message-Authenticator", hmac_obj.digest())

            self.SendReplyPacket(pkt.fd, reply)

        def RunWithStop(self, t_stop, eap_handler):
            self._poll = select.poll()
            self._fdmap = {}
            self._PrepareSockets()
            self.t_stop = t_stop
            self.eap_handler = eap_handler
            self.ctx = {}

            while not t_stop.is_set():
                for (fd, event) in self._poll.poll(200):
                    if event == select.POLLIN:
                        try:
                            fdo = self._fdmap[fd]
                            self._ProcessInput(fdo)
                        except pyrad.server.ServerPacketError as err:
                            logger.info("pyrad server dropping packet: " + str(err))
                        except pyrad.packet.PacketError as err:
                            logger.info("pyrad server received invalid packet: " + str(err))
                    else:
                        logger.error("Unexpected event in pyrad server main loop")

            for fd in self.authfds + self.acctfds:
                fd.close()

    srv = TestServer(dict=pyrad.dictionary.Dictionary("dictionary.radius"),
                     authport=18138, acctport=18139)
    srv.hosts["127.0.0.1"] = pyrad.server.RemoteHost("127.0.0.1",
                                                     b"radius",
                                                     "localhost")
    srv.BindToAddress("")
    t_stop = threading.Event()
    t = threading.Thread(target=run_pyrad_server, args=(srv, t_stop, eap_handler))
    t.start()

    return {'srv': srv, 'stop': t_stop, 'thread': t}

def stop_radius_server(srv):
    srv['stop'].set()
    srv['thread'].join()

def start_ap(ap):
    params = hostapd.wpa2_eap_params(ssid="eap-test")
    params['auth_server_port'] = "18138"
    hapd = hostapd.add_ap(ap, params)
    return hapd

def test_eap_proto(dev, apdev):
    """EAP protocol tests"""
    check_eap_capa(dev[0], "MD5")
    def eap_handler(ctx, req):
        logger.info("eap_handler - RX " + binascii.hexlify(req).decode())
        if 'num' not in ctx:
            ctx['num'] = 0
        ctx['num'] = ctx['num'] + 1
        if 'id' not in ctx:
            ctx['id'] = 1
        ctx['id'] = (ctx['id'] + 1) % 256
        idx = 0

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: MD5 challenge")
            return struct.pack(">BBHBBBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3,
                               EAP_TYPE_MD5,
                               1, 0xaa, ord('n'))
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Success - id off by 2")
            return struct.pack(">BBH", EAP_CODE_SUCCESS, ctx['id'] + 1, 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: MD5 challenge")
            return struct.pack(">BBHBBBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3,
                               EAP_TYPE_MD5,
                               1, 0xaa, ord('n'))
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Success - id off by 3")
            return struct.pack(">BBH", EAP_CODE_SUCCESS, ctx['id'] + 2, 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: MD5 challenge")
            return struct.pack(">BBHBBBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3,
                               EAP_TYPE_MD5,
                               1, 0xaa, ord('n'))
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Notification/Request")
            return struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1,
                               EAP_TYPE_NOTIFICATION,
                               ord('A'))
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Success")
            return struct.pack(">BBH", EAP_CODE_SUCCESS, ctx['id'] - 1, 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Notification/Request")
            return struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1,
                               EAP_TYPE_NOTIFICATION,
                               ord('B'))
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: MD5 challenge")
            return struct.pack(">BBHBBBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3,
                               EAP_TYPE_MD5,
                               1, 0xaa, ord('n'))
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Success")
            return struct.pack(">BBH", EAP_CODE_SUCCESS, ctx['id'] - 1, 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Notification/Request")
            return struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1,
                               EAP_TYPE_NOTIFICATION,
                               ord('C'))
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: MD5 challenge")
            return struct.pack(">BBHBBBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3,
                               EAP_TYPE_MD5,
                               1, 0xaa, ord('n'))
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Notification/Request")
            return struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1,
                               EAP_TYPE_NOTIFICATION,
                               ord('D'))
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Success")
            return struct.pack(">BBH", EAP_CODE_SUCCESS, ctx['id'] - 1, 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Notification/Request")
            return struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1,
                               EAP_TYPE_NOTIFICATION,
                               ord('E'))
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Notification/Request (same id)")
            return struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'] - 1,
                               4 + 1 + 1,
                               EAP_TYPE_NOTIFICATION,
                               ord('F'))
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unexpected EAP-Success")
            return struct.pack(">BBH", EAP_CODE_SUCCESS, ctx['id'] - 2, 4)

        return None

    srv = start_radius_server(eap_handler)

    try:
        hapd = start_ap(apdev[0])
        dev[0].scan_for_bss(hapd.own_addr(), freq=2412)

        dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                       eap="MD5", identity="user", password="password",
                       wait_connect=False)
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-PROPOSED-METHOD"], timeout=15)
        if ev is None:
            raise Exception("Timeout on EAP start")
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-SUCCESS"], timeout=15)
        if ev is None:
            raise Exception("Timeout on EAP success")
        dev[0].request("REMOVE_NETWORK all")

        dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                       eap="MD5", identity="user", password="password",
                       wait_connect=False)
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-PROPOSED-METHOD"], timeout=15)
        if ev is None:
            raise Exception("Timeout on EAP start")
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-SUCCESS"], timeout=1)
        if ev is not None:
            raise Exception("Unexpected EAP success")
        dev[0].request("REMOVE_NETWORK all")

        dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                       eap="MD5", identity="user", password="password",
                       wait_connect=False)
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-PROPOSED-METHOD"], timeout=15)
        if ev is None:
            raise Exception("Timeout on EAP start")
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-NOTIFICATION"], timeout=10)
        if ev is None:
            raise Exception("Timeout on EAP notification")
        if ev != "<3>CTRL-EVENT-EAP-NOTIFICATION A":
            raise Exception("Unexpected notification contents: " + ev)
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-SUCCESS"], timeout=15)
        if ev is None:
            raise Exception("Timeout on EAP success")
        dev[0].request("REMOVE_NETWORK all")

        dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                       eap="MD5", identity="user", password="password",
                       wait_connect=False)
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-NOTIFICATION"], timeout=10)
        if ev is None:
            raise Exception("Timeout on EAP notification")
        if ev != "<3>CTRL-EVENT-EAP-NOTIFICATION B":
            raise Exception("Unexpected notification contents: " + ev)
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-PROPOSED-METHOD"], timeout=15)
        if ev is None:
            raise Exception("Timeout on EAP start")
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-SUCCESS"], timeout=15)
        if ev is None:
            raise Exception("Timeout on EAP success")
        dev[0].request("REMOVE_NETWORK all")

        dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                       eap="MD5", identity="user", password="password",
                       wait_connect=False)
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-NOTIFICATION"], timeout=10)
        if ev is None:
            raise Exception("Timeout on EAP notification")
        if ev != "<3>CTRL-EVENT-EAP-NOTIFICATION C":
            raise Exception("Unexpected notification contents: " + ev)
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-PROPOSED-METHOD"], timeout=15)
        if ev is None:
            raise Exception("Timeout on EAP start")
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-NOTIFICATION"], timeout=10)
        if ev is None:
            raise Exception("Timeout on EAP notification")
        if ev != "<3>CTRL-EVENT-EAP-NOTIFICATION D":
            raise Exception("Unexpected notification contents: " + ev)
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-SUCCESS"], timeout=15)
        if ev is None:
            raise Exception("Timeout on EAP success")
        dev[0].request("REMOVE_NETWORK all")

        dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                       eap="MD5", identity="user", password="password",
                       wait_connect=False)
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-NOTIFICATION"], timeout=10)
        if ev is None:
            raise Exception("Timeout on EAP notification")
        if ev != "<3>CTRL-EVENT-EAP-NOTIFICATION E":
            raise Exception("Unexpected notification contents: " + ev)
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-NOTIFICATION"], timeout=10)
        if ev is None:
            raise Exception("Timeout on EAP notification")
        if ev != "<3>CTRL-EVENT-EAP-NOTIFICATION F":
            raise Exception("Unexpected notification contents: " + ev)
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"], timeout=15)
        if ev is None:
            raise Exception("Timeout on EAP failure")
        dev[0].request("REMOVE_NETWORK all")
    finally:
        stop_radius_server(srv)

def test_eap_proto_notification_errors(dev, apdev):
    """EAP Notification errors"""
    def eap_handler(ctx, req):
        logger.info("eap_handler - RX " + binascii.hexlify(req).decode())
        if 'num' not in ctx:
            ctx['num'] = 0
        ctx['num'] = ctx['num'] + 1
        if 'id' not in ctx:
            ctx['id'] = 1
        ctx['id'] = (ctx['id'] + 1) % 256
        idx = 0

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: MD5 challenge")
            return struct.pack(">BBHBBBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3,
                               EAP_TYPE_MD5,
                               1, 0xaa, ord('n'))
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Notification/Request")
            return struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1,
                               EAP_TYPE_NOTIFICATION,
                               ord('A'))

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: MD5 challenge")
            return struct.pack(">BBHBBBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3,
                               EAP_TYPE_MD5,
                               1, 0xaa, ord('n'))
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Notification/Request")
            return struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1,
                               EAP_TYPE_NOTIFICATION,
                               ord('A'))

        return None

    srv = start_radius_server(eap_handler)

    try:
        hapd = start_ap(apdev[0])
        dev[0].scan_for_bss(hapd.own_addr(), freq=2412)

        with alloc_fail(dev[0], 1, "eap_sm_processNotify"):
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="MD5", identity="user", password="password",
                           wait_connect=False)
            wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

        with alloc_fail(dev[0], 1, "eap_msg_alloc;sm_EAP_NOTIFICATION_Enter"):
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="MD5", identity="user", password="password",
                           wait_connect=False)
            wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()
    finally:
        stop_radius_server(srv)

EAP_SAKE_VERSION = 2

EAP_SAKE_SUBTYPE_CHALLENGE = 1
EAP_SAKE_SUBTYPE_CONFIRM = 2
EAP_SAKE_SUBTYPE_AUTH_REJECT = 3
EAP_SAKE_SUBTYPE_IDENTITY = 4

EAP_SAKE_AT_RAND_S = 1
EAP_SAKE_AT_RAND_P = 2
EAP_SAKE_AT_MIC_S = 3
EAP_SAKE_AT_MIC_P = 4
EAP_SAKE_AT_SERVERID = 5
EAP_SAKE_AT_PEERID = 6
EAP_SAKE_AT_SPI_S = 7
EAP_SAKE_AT_SPI_P = 8
EAP_SAKE_AT_ANY_ID_REQ = 9
EAP_SAKE_AT_PERM_ID_REQ = 10
EAP_SAKE_AT_ENCR_DATA = 128
EAP_SAKE_AT_IV = 129
EAP_SAKE_AT_PADDING = 130
EAP_SAKE_AT_NEXT_TMPID = 131
EAP_SAKE_AT_MSK_LIFE = 132

def test_eap_proto_sake(dev, apdev):
    """EAP-SAKE protocol tests"""
    global eap_proto_sake_test_done
    eap_proto_sake_test_done = False

    def sake_challenge(ctx):
        logger.info("Test: Challenge subtype")
        return struct.pack(">BBHBBBBBBLLLL", EAP_CODE_REQUEST, ctx['id'],
                           4 + 1 + 3 + 18,
                           EAP_TYPE_SAKE,
                           EAP_SAKE_VERSION, 0, EAP_SAKE_SUBTYPE_CHALLENGE,
                           EAP_SAKE_AT_RAND_S, 18, 0, 0, 0, 0)

    def sake_handler(ctx, req):
        logger.info("sake_handler - RX " + binascii.hexlify(req).decode())
        if 'num' not in ctx:
            ctx['num'] = 0
        ctx['num'] += 1
        if 'id' not in ctx:
            ctx['id'] = 1
        ctx['id'] = (ctx['id'] + 1) % 256
        idx = 0

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Missing payload")
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'], 4 + 1,
                               EAP_TYPE_SAKE)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Identity subtype without any attributes")
            return struct.pack(">BBHBBBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3,
                               EAP_TYPE_SAKE,
                               EAP_SAKE_VERSION, 0, EAP_SAKE_SUBTYPE_IDENTITY)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Identity subtype")
            return struct.pack(">BBHBBBBBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 4,
                               EAP_TYPE_SAKE,
                               EAP_SAKE_VERSION, 0, EAP_SAKE_SUBTYPE_IDENTITY,
                               EAP_SAKE_AT_ANY_ID_REQ, 4, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Identity subtype (different session id)")
            return struct.pack(">BBHBBBBBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 4,
                               EAP_TYPE_SAKE,
                               EAP_SAKE_VERSION, 1, EAP_SAKE_SUBTYPE_IDENTITY,
                               EAP_SAKE_AT_PERM_ID_REQ, 4, 0)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Identity subtype with too short attribute")
            return struct.pack(">BBHBBBBBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 2,
                               EAP_TYPE_SAKE,
                               EAP_SAKE_VERSION, 0, EAP_SAKE_SUBTYPE_IDENTITY,
                               EAP_SAKE_AT_ANY_ID_REQ, 2)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Identity subtype with truncated attribute")
            return struct.pack(">BBHBBBBBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 2,
                               EAP_TYPE_SAKE,
                               EAP_SAKE_VERSION, 0, EAP_SAKE_SUBTYPE_IDENTITY,
                               EAP_SAKE_AT_ANY_ID_REQ, 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Identity subtype with too short attribute header")
            payload = struct.pack("B", EAP_SAKE_AT_ANY_ID_REQ)
            return struct.pack(">BBHBBBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + len(payload),
                               EAP_TYPE_SAKE, EAP_SAKE_VERSION, 0,
                               EAP_SAKE_SUBTYPE_IDENTITY) + payload

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Identity subtype with AT_IV but not AT_ENCR_DATA")
            payload = struct.pack("BB", EAP_SAKE_AT_IV, 2)
            return struct.pack(">BBHBBBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + len(payload),
                               EAP_TYPE_SAKE, EAP_SAKE_VERSION, 0,
                               EAP_SAKE_SUBTYPE_IDENTITY) + payload

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Identity subtype with skippable and non-skippable unknown attribute")
            payload = struct.pack("BBBB", 255, 2, 127, 2)
            return struct.pack(">BBHBBBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + len(payload),
                               EAP_TYPE_SAKE, EAP_SAKE_VERSION, 0,
                               EAP_SAKE_SUBTYPE_IDENTITY) + payload

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Identity subtype: AT_RAND_P with invalid payload length")
            payload = struct.pack("BB", EAP_SAKE_AT_RAND_P, 2)
            return struct.pack(">BBHBBBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + len(payload),
                               EAP_TYPE_SAKE, EAP_SAKE_VERSION, 0,
                               EAP_SAKE_SUBTYPE_IDENTITY) + payload

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Identity subtype: AT_MIC_P with invalid payload length")
            payload = struct.pack("BB", EAP_SAKE_AT_MIC_P, 2)
            return struct.pack(">BBHBBBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + len(payload),
                               EAP_TYPE_SAKE, EAP_SAKE_VERSION, 0,
                               EAP_SAKE_SUBTYPE_IDENTITY) + payload

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Identity subtype: AT_PERM_ID_REQ with invalid payload length")
            payload = struct.pack("BBBBBBBBBBBBBB",
                                  EAP_SAKE_AT_SPI_S, 2,
                                  EAP_SAKE_AT_SPI_P, 2,
                                  EAP_SAKE_AT_ENCR_DATA, 2,
                                  EAP_SAKE_AT_NEXT_TMPID, 2,
                                  EAP_SAKE_AT_PERM_ID_REQ, 4, 0, 0,
                                  EAP_SAKE_AT_PERM_ID_REQ, 2)
            return struct.pack(">BBHBBBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + len(payload),
                               EAP_TYPE_SAKE, EAP_SAKE_VERSION, 0,
                               EAP_SAKE_SUBTYPE_IDENTITY) + payload

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Identity subtype: AT_PADDING")
            payload = struct.pack("BBBBBB",
                                  EAP_SAKE_AT_PADDING, 3, 0,
                                  EAP_SAKE_AT_PADDING, 3, 1)
            return struct.pack(">BBHBBBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + len(payload),
                               EAP_TYPE_SAKE, EAP_SAKE_VERSION, 0,
                               EAP_SAKE_SUBTYPE_IDENTITY) + payload

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Identity subtype: AT_MSK_LIFE")
            payload = struct.pack(">BBLBBH",
                                  EAP_SAKE_AT_MSK_LIFE, 6, 0,
                                  EAP_SAKE_AT_MSK_LIFE, 4, 0)
            return struct.pack(">BBHBBBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + len(payload),
                               EAP_TYPE_SAKE, EAP_SAKE_VERSION, 0,
                               EAP_SAKE_SUBTYPE_IDENTITY) + payload

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Identity subtype with invalid attribute length")
            payload = struct.pack("BB", EAP_SAKE_AT_ANY_ID_REQ, 0)
            return struct.pack(">BBHBBBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + len(payload),
                               EAP_TYPE_SAKE, EAP_SAKE_VERSION, 0,
                               EAP_SAKE_SUBTYPE_IDENTITY) + payload

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unknown subtype")
            return struct.pack(">BBHBBBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3,
                               EAP_TYPE_SAKE,
                               EAP_SAKE_VERSION, 0, 123)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Challenge subtype without any attributes")
            return struct.pack(">BBHBBBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3,
                               EAP_TYPE_SAKE,
                               EAP_SAKE_VERSION, 0, EAP_SAKE_SUBTYPE_CHALLENGE)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Challenge subtype with too short AT_RAND_S")
            return struct.pack(">BBHBBBBBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 2,
                               EAP_TYPE_SAKE,
                               EAP_SAKE_VERSION, 0, EAP_SAKE_SUBTYPE_CHALLENGE,
                               EAP_SAKE_AT_RAND_S, 2)

        idx += 1
        if ctx['num'] == idx:
            return sake_challenge(ctx)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unexpected Identity subtype")
            return struct.pack(">BBHBBBBBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 4,
                               EAP_TYPE_SAKE,
                               EAP_SAKE_VERSION, 0, EAP_SAKE_SUBTYPE_IDENTITY,
                               EAP_SAKE_AT_ANY_ID_REQ, 4, 0)

        idx += 1
        if ctx['num'] == idx:
            return sake_challenge(ctx)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unexpected Challenge subtype")
            return struct.pack(">BBHBBBBBBLLLL", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 18,
                               EAP_TYPE_SAKE,
                               EAP_SAKE_VERSION, 0, EAP_SAKE_SUBTYPE_CHALLENGE,
                               EAP_SAKE_AT_RAND_S, 18, 0, 0, 0, 0)

        idx += 1
        if ctx['num'] == idx:
            return sake_challenge(ctx)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Confirm subtype without any attributes")
            return struct.pack(">BBHBBBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3,
                               EAP_TYPE_SAKE,
                               EAP_SAKE_VERSION, 0, EAP_SAKE_SUBTYPE_CONFIRM)

        idx += 1
        if ctx['num'] == idx:
            return sake_challenge(ctx)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Confirm subtype with too short AT_MIC_S")
            return struct.pack(">BBHBBBBBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 2,
                               EAP_TYPE_SAKE,
                               EAP_SAKE_VERSION, 0, EAP_SAKE_SUBTYPE_CONFIRM,
                               EAP_SAKE_AT_MIC_S, 2)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unexpected Confirm subtype")
            return struct.pack(">BBHBBBBBBLLLL", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 18,
                               EAP_TYPE_SAKE,
                               EAP_SAKE_VERSION, 0, EAP_SAKE_SUBTYPE_CONFIRM,
                               EAP_SAKE_AT_MIC_S, 18, 0, 0, 0, 0)

        idx += 1
        if ctx['num'] == idx:
            return sake_challenge(ctx)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Confirm subtype with incorrect AT_MIC_S")
            return struct.pack(">BBHBBBBBBLLLL", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 18,
                               EAP_TYPE_SAKE,
                               EAP_SAKE_VERSION, 0, EAP_SAKE_SUBTYPE_CONFIRM,
                               EAP_SAKE_AT_MIC_S, 18, 0, 0, 0, 0)

        global eap_proto_sake_test_done
        if eap_proto_sake_test_done:
            return sake_challenge(ctx)

        logger.info("No more test responses available - test case completed")
        eap_proto_sake_test_done = True
        return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

    srv = start_radius_server(sake_handler)

    try:
        hapd = start_ap(apdev[0])
        dev[0].scan_for_bss(hapd.own_addr(), freq=2412)

        while not eap_proto_sake_test_done:
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="SAKE", identity="sake user",
                           password_hex="0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",
                           wait_connect=False)
            ev = dev[0].wait_event(["CTRL-EVENT-EAP-PROPOSED-METHOD"], timeout=15)
            if ev is None:
                raise Exception("Timeout on EAP start")
            time.sleep(0.1)
            dev[0].request("REMOVE_NETWORK all")

        logger.info("Too short password")
        dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                       eap="SAKE", identity="sake user",
                       password_hex="0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcd",
                       wait_connect=False)
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-PROPOSED-METHOD"], timeout=15)
        if ev is None:
            raise Exception("Timeout on EAP start")
        time.sleep(0.1)
    finally:
        stop_radius_server(srv)

def test_eap_proto_sake_errors(dev, apdev):
    """EAP-SAKE local error cases"""
    check_eap_capa(dev[0], "SAKE")
    params = hostapd.wpa2_eap_params(ssid="eap-test")
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].scan_for_bss(hapd.own_addr(), freq=2412)

    for i in range(1, 3):
        with alloc_fail(dev[0], i, "eap_sake_init"):
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="SAKE", identity="sake user",
                           password_hex="0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",
                           wait_connect=False)
            ev = dev[0].wait_event(["EAP: Failed to initialize EAP method"],
                                   timeout=15)
            if ev is None:
                raise Exception("Timeout on EAP start")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()
            dev[0].dump_monitor()

    tests = [(1, "eap_msg_alloc;eap_sake_build_msg;eap_sake_process_challenge"),
             (1, "=eap_sake_process_challenge"),
             (1, "eap_sake_compute_mic;eap_sake_process_challenge"),
             (1, "eap_sake_build_msg;eap_sake_process_confirm"),
             (1, "eap_sake_compute_mic;eap_sake_process_confirm"),
             (2, "eap_sake_compute_mic;=eap_sake_process_confirm"),
             (1, "eap_sake_getKey"),
             (1, "eap_sake_get_emsk"),
             (1, "eap_sake_get_session_id")]
    for count, func in tests:
        with alloc_fail(dev[0], count, func):
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="SAKE", identity="sake user@domain",
                           password_hex="0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",
                           erp="1",
                           wait_connect=False)
            ev = dev[0].wait_event(["CTRL-EVENT-EAP-PROPOSED-METHOD"],
                                   timeout=15)
            if ev is None:
                raise Exception("Timeout on EAP start")
            wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()
            dev[0].dump_monitor()

    tests = [(1, "os_get_random;eap_sake_process_challenge"),
             (1, "eap_sake_derive_keys;eap_sake_process_challenge"),
             (2, "eap_sake_derive_keys;eap_sake_process_challenge"),
             (3, "eap_sake_derive_keys;eap_sake_process_challenge"),
             (4, "eap_sake_derive_keys;eap_sake_process_challenge"),
             (5, "eap_sake_derive_keys;eap_sake_process_challenge"),
             (6, "eap_sake_derive_keys;eap_sake_process_challenge")]
    for count, func in tests:
        with fail_test(dev[0], count, func):
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="SAKE", identity="sake user",
                           password_hex="0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",
                           wait_connect=False)
            ev = dev[0].wait_event(["CTRL-EVENT-EAP-PROPOSED-METHOD"], timeout=15)
            if ev is None:
                raise Exception("Timeout on EAP start")
            wait_fail_trigger(dev[0], "GET_FAIL")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()
            dev[0].dump_monitor()

def test_eap_proto_sake_errors2(dev, apdev):
    """EAP-SAKE protocol tests (2)"""
    def sake_handler(ctx, req):
        logger.info("sake_handler - RX " + binascii.hexlify(req).decode())
        if 'num' not in ctx:
            ctx['num'] = 0
        ctx['num'] += 1
        if 'id' not in ctx:
            ctx['id'] = 1
        ctx['id'] = (ctx['id'] + 1) % 256
        idx = 0

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Identity subtype")
            return struct.pack(">BBHBBBBBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 4,
                               EAP_TYPE_SAKE,
                               EAP_SAKE_VERSION, 0, EAP_SAKE_SUBTYPE_IDENTITY,
                               EAP_SAKE_AT_ANY_ID_REQ, 4, 0)

    srv = start_radius_server(sake_handler)

    try:
        hapd = start_ap(apdev[0])
        dev[0].scan_for_bss(hapd.own_addr(), freq=2412)

        with alloc_fail(dev[0], 1, "eap_msg_alloc;eap_sake_build_msg;eap_sake_process_identity"):
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="SAKE", identity="sake user",
                           password_hex="0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",
                           wait_connect=False)
            ev = dev[0].wait_event(["CTRL-EVENT-EAP-PROPOSED-METHOD"],
                                   timeout=15)
            if ev is None:
                raise Exception("Timeout on EAP start")
                dev[0].request("REMOVE_NETWORK all")
                dev[0].wait_disconnected()

    finally:
        stop_radius_server(srv)

def run_eap_sake_connect(dev):
    dev.connect("test-wpa2-eap", key_mgmt="WPA-EAP", scan_freq="2412",
                eap="SAKE", identity="sake user",
                password_hex="0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",
                wait_connect=False)
    ev = dev.wait_event(["CTRL-EVENT-EAP-SUCCESS", "CTRL-EVENT-EAP-FAILURE",
                         "CTRL-EVENT-DISCONNECTED"],
                        timeout=1)
    dev.request("REMOVE_NETWORK all")
    if not ev or "CTRL-EVENT-DISCONNECTED" not in ev:
        dev.wait_disconnected()
    dev.dump_monitor()

def test_eap_proto_sake_errors_server(dev, apdev):
    """EAP-SAKE local error cases on server"""
    check_eap_capa(dev[0], "SAKE")
    params = int_eap_server_params()
    params['erp_domain'] = 'example.com'
    params['eap_server_erp'] = '1'
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].scan_for_bss(hapd.own_addr(), freq=2412)

    tests = [(1, "eap_sake_init"),
             (1, "eap_sake_build_msg;eap_sake_build_challenge"),
             (1, "eap_sake_build_msg;eap_sake_build_confirm"),
             (1, "eap_sake_compute_mic;eap_sake_build_confirm"),
             (1, "eap_sake_process_challenge"),
             (1, "eap_sake_getKey"),
             (1, "eap_sake_get_emsk"),
             (1, "eap_sake_get_session_id")]
    for count, func in tests:
        with alloc_fail(hapd, count, func):
            run_eap_sake_connect(dev[0])

    tests = [(1, "eap_sake_init"),
             (1, "eap_sake_build_challenge"),
             (1, "eap_sake_build_confirm"),
             (1, "eap_sake_derive_keys;eap_sake_process_challenge"),
             (1, "eap_sake_compute_mic;eap_sake_process_challenge"),
             (1, "eap_sake_compute_mic;eap_sake_process_confirm"),
             (1, "eap_sake_compute_mic;eap_sake_build_confirm"),
             (1, "eap_sake_process_confirm")]
    for count, func in tests:
        with fail_test(hapd, count, func):
            run_eap_sake_connect(dev[0])

def start_sake_assoc(dev, hapd):
    dev.connect("test-wpa2-eap", key_mgmt="WPA-EAP", scan_freq="2412",
                eap="SAKE", identity="sake user",
                password_hex="0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",
                wait_connect=False)
    proxy_msg(hapd, dev) # EAP-Identity/Request
    proxy_msg(dev, hapd) # EAP-Identity/Response
    proxy_msg(hapd, dev) # SAKE/Challenge/Request

def stop_sake_assoc(dev, hapd):
    dev.request("REMOVE_NETWORK all")
    dev.wait_disconnected()
    dev.dump_monitor()
    hapd.dump_monitor()

def test_eap_proto_sake_server(dev, apdev):
    """EAP-SAKE protocol testing for the server"""
    check_eap_capa(dev[0], "SAKE")
    params = int_eap_server_params()
    params['erp_domain'] = 'example.com'
    params['eap_server_erp'] = '1'
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].scan_for_bss(hapd.own_addr(), freq=2412)
    hapd.request("SET ext_eapol_frame_io 1")
    dev[0].request("SET ext_eapol_frame_io 1")

    # Successful exchange to verify proxying mechanism
    start_sake_assoc(dev[0], hapd)
    proxy_msg(dev[0], hapd) # SAKE/Challenge/Response
    proxy_msg(hapd, dev[0]) # SAKE/Confirm/Request
    proxy_msg(dev[0], hapd) # SAKE/Confirm/Response
    proxy_msg(hapd, dev[0]) # EAP-Success
    proxy_msg(hapd, dev[0]) # EAPOL-Key msg 1/4
    proxy_msg(dev[0], hapd) # EAPOL-Key msg 2/4
    proxy_msg(hapd, dev[0]) # EAPOL-Key msg 3/4
    proxy_msg(dev[0], hapd) # EAPOL-Key msg 4/4
    dev[0].wait_connected()
    stop_sake_assoc(dev[0], hapd)

    start_sake_assoc(dev[0], hapd)
    resp = rx_msg(dev[0])
    # Too short EAP-SAKE header
    # --> EAP-SAKE: Invalid frame
    msg = resp[0:4] + "0007" + resp[8:12] + "0007" + "300200"
    tx_msg(dev[0], hapd, msg)
    # Unknown version
    # --> EAP-SAKE: Unknown version 1
    msg = resp[0:4] + "0008" + resp[8:12] + "0008" + "30010000"
    tx_msg(dev[0], hapd, msg)
    # Unknown session
    # --> EAP-SAKE: Session ID mismatch
    sess, = struct.unpack('B', binascii.unhexlify(resp[20:22]))
    sess = binascii.hexlify(struct.pack('B', sess + 1)).decode()
    msg = resp[0:4] + "0008" + resp[8:12] + "0008" + "3002" + sess + "00"
    tx_msg(dev[0], hapd, msg)
    # Unknown subtype
    # --> EAP-SAKE: Unexpected subtype=5 in state=1
    msg = resp[0:22] + "05" + resp[24:]
    tx_msg(dev[0], hapd, msg)
    # Empty challenge
    # --> EAP-SAKE: Response/Challenge did not include AT_RAND_P or AT_MIC_P
    msg = resp[0:4] + "0008" + resp[8:12] + "0008" + resp[16:24]
    tx_msg(dev[0], hapd, msg)
    rx_msg(hapd)
    stop_sake_assoc(dev[0], hapd)

    start_sake_assoc(dev[0], hapd)
    resp = rx_msg(dev[0])
    # Invalid attribute in challenge
    # --> EAP-SAKE: Too short attribute
    msg = resp[0:4] + "0009" + resp[8:12] + "0009" + resp[16:26]
    tx_msg(dev[0], hapd, msg)
    rx_msg(hapd)
    stop_sake_assoc(dev[0], hapd)

    start_sake_assoc(dev[0], hapd)
    proxy_msg(dev[0], hapd) # SAKE/Challenge/Response
    proxy_msg(hapd, dev[0]) # SAKE/Confirm/Request
    resp = rx_msg(dev[0])
    # Empty confirm
    # --> EAP-SAKE: Response/Confirm did not include AT_MIC_P
    msg = resp[0:4] + "0008" + resp[8:12] + "0008" + resp[16:26]
    tx_msg(dev[0], hapd, msg)
    rx_msg(hapd)
    stop_sake_assoc(dev[0], hapd)

    start_sake_assoc(dev[0], hapd)
    proxy_msg(dev[0], hapd) # SAKE/Challenge/Response
    proxy_msg(hapd, dev[0]) # SAKE/Confirm/Request
    resp = rx_msg(dev[0])
    # Invalid attribute in confirm
    # --> EAP-SAKE: Too short attribute
    msg = resp[0:4] + "0009" + resp[8:12] + "0009" + resp[16:26]
    tx_msg(dev[0], hapd, msg)
    rx_msg(hapd)
    stop_sake_assoc(dev[0], hapd)

    start_sake_assoc(dev[0], hapd)
    proxy_msg(dev[0], hapd) # SAKE/Challenge/Response
    proxy_msg(hapd, dev[0]) # SAKE/Confirm/Request
    resp = rx_msg(dev[0])
    # Corrupted AT_MIC_P value
    # --> EAP-SAKE: Incorrect AT_MIC_P
    msg = resp[0:30] + "000000000000" + resp[42:]
    tx_msg(dev[0], hapd, msg)
    rx_msg(hapd)
    stop_sake_assoc(dev[0], hapd)

def test_eap_proto_leap(dev, apdev):
    """EAP-LEAP protocol tests"""
    check_eap_capa(dev[0], "LEAP")
    def leap_handler(ctx, req):
        logger.info("leap_handler - RX " + binascii.hexlify(req).decode())
        if 'num' not in ctx:
            ctx['num'] = 0
        ctx['num'] = ctx['num'] + 1
        if 'id' not in ctx:
            ctx['id'] = 1
        ctx['id'] = (ctx['id'] + 1) % 256

        if ctx['num'] == 1:
            logger.info("Test: Missing payload")
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1,
                               EAP_TYPE_LEAP)

        if ctx['num'] == 2:
            logger.info("Test: Unexpected version")
            return struct.pack(">BBHBBBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3,
                               EAP_TYPE_LEAP,
                               0, 0, 0)

        if ctx['num'] == 3:
            logger.info("Test: Invalid challenge length")
            return struct.pack(">BBHBBBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3,
                               EAP_TYPE_LEAP,
                               1, 0, 0)

        if ctx['num'] == 4:
            logger.info("Test: Truncated challenge")
            return struct.pack(">BBHBBBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3,
                               EAP_TYPE_LEAP,
                               1, 0, 8)

        if ctx['num'] == 5:
            logger.info("Test: Valid challenge")
            return struct.pack(">BBHBBBBLL", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 8,
                               EAP_TYPE_LEAP,
                               1, 0, 8, 0, 0)
        if ctx['num'] == 6:
            logger.info("Test: Missing payload in Response")
            return struct.pack(">BBHB", EAP_CODE_RESPONSE, ctx['id'],
                               4 + 1,
                               EAP_TYPE_LEAP)

        if ctx['num'] == 7:
            logger.info("Test: Valid challenge")
            return struct.pack(">BBHBBBBLL", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 8,
                               EAP_TYPE_LEAP,
                               1, 0, 8, 0, 0)
        if ctx['num'] == 8:
            logger.info("Test: Unexpected version in Response")
            return struct.pack(">BBHBBBB", EAP_CODE_RESPONSE, ctx['id'],
                               4 + 1 + 3,
                               EAP_TYPE_LEAP,
                               0, 0, 8)

        if ctx['num'] == 9:
            logger.info("Test: Valid challenge")
            return struct.pack(">BBHBBBBLL", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 8,
                               EAP_TYPE_LEAP,
                               1, 0, 8, 0, 0)
        if ctx['num'] == 10:
            logger.info("Test: Invalid challenge length in Response")
            return struct.pack(">BBHBBBB", EAP_CODE_RESPONSE, ctx['id'],
                               4 + 1 + 3,
                               EAP_TYPE_LEAP,
                               1, 0, 0)

        if ctx['num'] == 11:
            logger.info("Test: Valid challenge")
            return struct.pack(">BBHBBBBLL", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 8,
                               EAP_TYPE_LEAP,
                               1, 0, 8, 0, 0)
        if ctx['num'] == 12:
            logger.info("Test: Truncated challenge in Response")
            return struct.pack(">BBHBBBB", EAP_CODE_RESPONSE, ctx['id'],
                               4 + 1 + 3,
                               EAP_TYPE_LEAP,
                               1, 0, 24)

        if ctx['num'] == 13:
            logger.info("Test: Valid challenge")
            return struct.pack(">BBHBBBBLL", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 8,
                               EAP_TYPE_LEAP,
                               1, 0, 8, 0, 0)
        if ctx['num'] == 14:
            logger.info("Test: Invalid challange value in Response")
            return struct.pack(">BBHBBBB6L", EAP_CODE_RESPONSE, ctx['id'],
                               4 + 1 + 3 + 24,
                               EAP_TYPE_LEAP,
                               1, 0, 24,
                               0, 0, 0, 0, 0, 0)

        if ctx['num'] == 15:
            logger.info("Test: Valid challenge")
            return struct.pack(">BBHBBBBLL", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 8,
                               EAP_TYPE_LEAP,
                               1, 0, 8, 0, 0)
        if ctx['num'] == 16:
            logger.info("Test: Valid challange value in Response")
            return struct.pack(">BBHBBBB24B", EAP_CODE_RESPONSE, ctx['id'],
                               4 + 1 + 3 + 24,
                               EAP_TYPE_LEAP,
                               1, 0, 24,
                               0x48, 0x4e, 0x46, 0xe3, 0x88, 0x49, 0x46, 0xbd,
                               0x28, 0x48, 0xf8, 0x53, 0x82, 0x50, 0x00, 0x04,
                               0x93, 0x50, 0x30, 0xd7, 0x25, 0xea, 0x5f, 0x66)

        if ctx['num'] == 17:
            logger.info("Test: Valid challenge")
            return struct.pack(">BBHBBBBLL", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 8,
                               EAP_TYPE_LEAP,
                               1, 0, 8, 0, 0)
        if ctx['num'] == 18:
            logger.info("Test: Success")
            return struct.pack(">BBHB", EAP_CODE_SUCCESS, ctx['id'],
                               4 + 1,
                               EAP_TYPE_LEAP)
        # hostapd will drop the next frame in the sequence

        if ctx['num'] == 19:
            logger.info("Test: Valid challenge")
            return struct.pack(">BBHBBBBLL", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 8,
                               EAP_TYPE_LEAP,
                               1, 0, 8, 0, 0)
        if ctx['num'] == 20:
            logger.info("Test: Failure")
            return struct.pack(">BBHB", EAP_CODE_FAILURE, ctx['id'],
                               4 + 1,
                               EAP_TYPE_LEAP)

        return None

    srv = start_radius_server(leap_handler)

    try:
        hapd = start_ap(apdev[0])
        dev[0].scan_for_bss(hapd.own_addr(), freq=2412)

        for i in range(0, 12):
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="LEAP", identity="user", password="password",
                           wait_connect=False)
            ev = dev[0].wait_event(["CTRL-EVENT-EAP-PROPOSED-METHOD"], timeout=15)
            if ev is None:
                raise Exception("Timeout on EAP start")
            time.sleep(0.1)
            if i == 10:
                logger.info("Wait for additional roundtrip")
                time.sleep(1)
            dev[0].request("REMOVE_NETWORK all")
    finally:
        stop_radius_server(srv)

def test_eap_proto_leap_errors(dev, apdev):
    """EAP-LEAP protocol tests (error paths)"""
    check_eap_capa(dev[0], "LEAP")

    def leap_handler2(ctx, req):
        logger.info("leap_handler2 - RX " + binascii.hexlify(req).decode())
        if 'num' not in ctx:
            ctx['num'] = 0
        ctx['num'] = ctx['num'] + 1
        if 'id' not in ctx:
            ctx['id'] = 1
        ctx['id'] = (ctx['id'] + 1) % 256
        idx = 0

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Valid challenge")
            return struct.pack(">BBHBBBBLL", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 8,
                               EAP_TYPE_LEAP,
                               1, 0, 8, 0, 0)
        idx += 1
        if ctx['num'] == idx:
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Valid challenge")
            return struct.pack(">BBHBBBBLL", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 8,
                               EAP_TYPE_LEAP,
                               1, 0, 8, 0, 0)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Valid challenge")
            return struct.pack(">BBHBBBBLL", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 8,
                               EAP_TYPE_LEAP,
                               1, 0, 8, 0, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Success")
            return struct.pack(">BBH", EAP_CODE_SUCCESS, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Valid challenge")
            return struct.pack(">BBHBBBBLL", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 8,
                               EAP_TYPE_LEAP,
                               1, 0, 8, 0, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Success")
            return struct.pack(">BBH", EAP_CODE_SUCCESS, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Valid challenge")
            return struct.pack(">BBHBBBBLL", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 8,
                               EAP_TYPE_LEAP,
                               1, 0, 8, 0, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Valid challange value in Response")
            return struct.pack(">BBHBBBB24B", EAP_CODE_RESPONSE, ctx['id'],
                               4 + 1 + 3 + 24,
                               EAP_TYPE_LEAP,
                               1, 0, 24,
                               0x48, 0x4e, 0x46, 0xe3, 0x88, 0x49, 0x46, 0xbd,
                               0x28, 0x48, 0xf8, 0x53, 0x82, 0x50, 0x00, 0x04,
                               0x93, 0x50, 0x30, 0xd7, 0x25, 0xea, 0x5f, 0x66)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Valid challenge")
            return struct.pack(">BBHBBBBLL", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 8,
                               EAP_TYPE_LEAP,
                               1, 0, 8, 0, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Valid challange value in Response")
            return struct.pack(">BBHBBBB24B", EAP_CODE_RESPONSE, ctx['id'],
                               4 + 1 + 3 + 24,
                               EAP_TYPE_LEAP,
                               1, 0, 24,
                               0x48, 0x4e, 0x46, 0xe3, 0x88, 0x49, 0x46, 0xbd,
                               0x28, 0x48, 0xf8, 0x53, 0x82, 0x50, 0x00, 0x04,
                               0x93, 0x50, 0x30, 0xd7, 0x25, 0xea, 0x5f, 0x66)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Valid challenge")
            return struct.pack(">BBHBBBBLL", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 8,
                               EAP_TYPE_LEAP,
                               1, 0, 8, 0, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Valid challange value in Response")
            return struct.pack(">BBHBBBB24B", EAP_CODE_RESPONSE, ctx['id'],
                               4 + 1 + 3 + 24,
                               EAP_TYPE_LEAP,
                               1, 0, 24,
                               0x48, 0x4e, 0x46, 0xe3, 0x88, 0x49, 0x46, 0xbd,
                               0x28, 0x48, 0xf8, 0x53, 0x82, 0x50, 0x00, 0x04,
                               0x93, 0x50, 0x30, 0xd7, 0x25, 0xea, 0x5f, 0x66)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Valid challenge")
            return struct.pack(">BBHBBBBLL", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 8,
                               EAP_TYPE_LEAP,
                               1, 0, 8, 0, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Valid challange value in Response")
            return struct.pack(">BBHBBBB24B", EAP_CODE_RESPONSE, ctx['id'],
                               4 + 1 + 3 + 24,
                               EAP_TYPE_LEAP,
                               1, 0, 24,
                               0x48, 0x4e, 0x46, 0xe3, 0x88, 0x49, 0x46, 0xbd,
                               0x28, 0x48, 0xf8, 0x53, 0x82, 0x50, 0x00, 0x04,
                               0x93, 0x50, 0x30, 0xd7, 0x25, 0xea, 0x5f, 0x66)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Valid challenge")
            return struct.pack(">BBHBBBBLL", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 8,
                               EAP_TYPE_LEAP,
                               1, 0, 8, 0, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Valid challange value in Response")
            return struct.pack(">BBHBBBB24B", EAP_CODE_RESPONSE, ctx['id'],
                               4 + 1 + 3 + 24,
                               EAP_TYPE_LEAP,
                               1, 0, 24,
                               0x48, 0x4e, 0x46, 0xe3, 0x88, 0x49, 0x46, 0xbd,
                               0x28, 0x48, 0xf8, 0x53, 0x82, 0x50, 0x00, 0x04,
                               0x93, 0x50, 0x30, 0xd7, 0x25, 0xea, 0x5f, 0x66)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Valid challenge")
            return struct.pack(">BBHBBBBLL", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 8,
                               EAP_TYPE_LEAP,
                               1, 0, 8, 0, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Valid challange value in Response")
            return struct.pack(">BBHBBBB24B", EAP_CODE_RESPONSE, ctx['id'],
                               4 + 1 + 3 + 24,
                               EAP_TYPE_LEAP,
                               1, 0, 24,
                               0x48, 0x4e, 0x46, 0xe3, 0x88, 0x49, 0x46, 0xbd,
                               0x28, 0x48, 0xf8, 0x53, 0x82, 0x50, 0x00, 0x04,
                               0x93, 0x50, 0x30, 0xd7, 0x25, 0xea, 0x5f, 0x66)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Valid challenge")
            return struct.pack(">BBHBBBBLL", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 8,
                               EAP_TYPE_LEAP,
                               1, 0, 8, 0, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Valid challange value in Response")
            return struct.pack(">BBHBBBB24B", EAP_CODE_RESPONSE, ctx['id'],
                               4 + 1 + 3 + 24,
                               EAP_TYPE_LEAP,
                               1, 0, 24,
                               0x48, 0x4e, 0x46, 0xe3, 0x88, 0x49, 0x46, 0xbd,
                               0x28, 0x48, 0xf8, 0x53, 0x82, 0x50, 0x00, 0x04,
                               0x93, 0x50, 0x30, 0xd7, 0x25, 0xea, 0x5f, 0x66)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Valid challenge")
            return struct.pack(">BBHBBBBLL", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 8,
                               EAP_TYPE_LEAP,
                               1, 0, 8, 0, 0)

        return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

    srv = start_radius_server(leap_handler2)

    try:
        hapd = start_ap(apdev[0])
        dev[0].scan_for_bss(hapd.own_addr(), freq=2412)

        with alloc_fail(dev[0], 1, "eap_leap_init"):
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="LEAP", identity="user", password="password",
                           wait_connect=False)
            wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

        with alloc_fail(dev[0], 1, "eap_msg_alloc;eap_leap_process_request"):
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="LEAP", identity="user",
                           password_hex="hash:8846f7eaee8fb117ad06bdd830b7586c",
                           wait_connect=False)
            wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

        with alloc_fail(dev[0], 1, "eap_leap_process_success"):
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="LEAP", identity="user", password="password",
                           wait_connect=False)
            wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

        with fail_test(dev[0], 1, "os_get_random;eap_leap_process_success"):
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="LEAP", identity="user", password="password",
                           wait_connect=False)
            wait_fail_trigger(dev[0], "GET_FAIL")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

        with fail_test(dev[0], 1, "eap_leap_process_response"):
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="LEAP", identity="user",
                           password_hex="hash:8846f7eaee8fb117ad06bdd830b7586c",
                           wait_connect=False)
            wait_fail_trigger(dev[0], "GET_FAIL")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

        with fail_test(dev[0], 1, "nt_password_hash;eap_leap_process_response"):
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="LEAP", identity="user", password="password",
                           wait_connect=False)
            wait_fail_trigger(dev[0], "GET_FAIL")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

        with fail_test(dev[0], 1, "hash_nt_password_hash;eap_leap_process_response"):
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="LEAP", identity="user", password="password",
                           wait_connect=False)
            wait_fail_trigger(dev[0], "GET_FAIL")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

        with alloc_fail(dev[0], 1, "eap_leap_getKey"):
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="LEAP", identity="user",
                           password_hex="hash:8846f7eaee8fb117ad06bdd830b7586c",
                           wait_connect=False)
            wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

        with fail_test(dev[0], 1, "eap_leap_getKey"):
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="LEAP", identity="user",
                           password_hex="hash:8846f7eaee8fb117ad06bdd830b7586c",
                           wait_connect=False)
            wait_fail_trigger(dev[0], "GET_FAIL")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

        with fail_test(dev[0], 1, "nt_password_hash;eap_leap_getKey"):
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="LEAP", identity="user", password="password",
                           wait_connect=False)
            wait_fail_trigger(dev[0], "GET_FAIL")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

        with fail_test(dev[0], 1, "hash_nt_password_hash;eap_leap_getKey"):
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="LEAP", identity="user", password="password",
                           wait_connect=False)
            wait_fail_trigger(dev[0], "GET_FAIL")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

        with fail_test(dev[0], 1,
                       "nt_challenge_response;eap_leap_process_request"):
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="LEAP", identity="user", password="password",
                           wait_connect=False)
            wait_fail_trigger(dev[0], "GET_FAIL")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()
    finally:
        stop_radius_server(srv)

def test_eap_proto_md5(dev, apdev):
    """EAP-MD5 protocol tests"""
    check_eap_capa(dev[0], "MD5")

    def md5_handler(ctx, req):
        logger.info("md5_handler - RX " + binascii.hexlify(req).decode())
        if 'num' not in ctx:
            ctx['num'] = 0
        ctx['num'] = ctx['num'] + 1
        if 'id' not in ctx:
            ctx['id'] = 1
        ctx['id'] = (ctx['id'] + 1) % 256

        if ctx['num'] == 1:
            logger.info("Test: Missing payload")
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1,
                               EAP_TYPE_MD5)

        if ctx['num'] == 2:
            logger.info("Test: Zero-length challenge")
            return struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1,
                               EAP_TYPE_MD5,
                               0)

        if ctx['num'] == 3:
            logger.info("Test: Truncated challenge")
            return struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1,
                               EAP_TYPE_MD5,
                               1)

        if ctx['num'] == 4:
            logger.info("Test: Shortest possible challenge and name")
            return struct.pack(">BBHBBBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3,
                               EAP_TYPE_MD5,
                               1, 0xaa, ord('n'))

        return None

    srv = start_radius_server(md5_handler)

    try:
        hapd = start_ap(apdev[0])
        dev[0].scan_for_bss(hapd.own_addr(), freq=2412)

        for i in range(0, 4):
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="MD5", identity="user", password="password",
                           wait_connect=False)
            ev = dev[0].wait_event(["CTRL-EVENT-EAP-PROPOSED-METHOD"], timeout=15)
            if ev is None:
                raise Exception("Timeout on EAP start")
            time.sleep(0.1)
            dev[0].request("REMOVE_NETWORK all")
    finally:
        stop_radius_server(srv)

def test_eap_proto_md5_errors(dev, apdev):
    """EAP-MD5 local error cases"""
    check_eap_capa(dev[0], "MD5")
    params = hostapd.wpa2_eap_params(ssid="eap-test")
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].scan_for_bss(hapd.own_addr(), freq=2412)

    with fail_test(dev[0], 1, "chap_md5"):
        dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                       eap="MD5", identity="phase1-user", password="password",
                       wait_connect=False)
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-METHOD"], timeout=15)
        if ev is None:
            raise Exception("Timeout on EAP start")
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()

    with alloc_fail(dev[0], 1, "eap_msg_alloc;eap_md5_process"):
        dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                       eap="MD5", identity="phase1-user", password="password",
                       wait_connect=False)
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-METHOD"], timeout=15)
        if ev is None:
            raise Exception("Timeout on EAP start")
        time.sleep(0.1)
        dev[0].request("REMOVE_NETWORK all")

def run_eap_md5_connect(dev):
    dev.connect("test-wpa2-eap", key_mgmt="WPA-EAP", scan_freq="2412",
                eap="MD5", identity="phase1-user", password="password",
                wait_connect=False)
    ev = dev.wait_event(["CTRL-EVENT-EAP-SUCCESS", "CTRL-EVENT-EAP-FAILURE",
                         "CTRL-EVENT-DISCONNECTED"],
                        timeout=1)
    dev.request("REMOVE_NETWORK all")
    if not ev or "CTRL-EVENT-DISCONNECTED" not in ev:
        dev.wait_disconnected()
    dev.dump_monitor()

def test_eap_proto_md5_errors_server(dev, apdev):
    """EAP-MD5 local error cases on server"""
    check_eap_capa(dev[0], "MD5")
    params = int_eap_server_params()
    params['erp_domain'] = 'example.com'
    params['eap_server_erp'] = '1'
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].scan_for_bss(hapd.own_addr(), freq=2412)

    tests = [(1, "eap_md5_init")]
    for count, func in tests:
        with alloc_fail(hapd, count, func):
            run_eap_md5_connect(dev[0])

    tests = [(1, "os_get_random;eap_md5_buildReq"),
             (1, "chap_md5;eap_md5_process")]
    for count, func in tests:
        with fail_test(hapd, count, func):
            run_eap_md5_connect(dev[0])

def start_md5_assoc(dev, hapd):
    dev.connect("test-wpa2-eap", key_mgmt="WPA-EAP", scan_freq="2412",
                eap="MD5", identity="phase1-user", password="password",
                wait_connect=False)
    proxy_msg(hapd, dev) # EAP-Identity/Request
    proxy_msg(dev, hapd) # EAP-Identity/Response
    proxy_msg(hapd, dev) # MSCHAPV2/Request
    proxy_msg(dev, hapd) # NAK
    proxy_msg(hapd, dev) # MD5 Request

def stop_md5_assoc(dev, hapd):
    dev.request("REMOVE_NETWORK all")
    dev.wait_disconnected()
    dev.dump_monitor()
    hapd.dump_monitor()

def test_eap_proto_md5_server(dev, apdev):
    """EAP-MD5 protocol testing for the server"""
    check_eap_capa(dev[0], "MD5")
    params = int_eap_server_params()
    params['erp_domain'] = 'example.com'
    params['eap_server_erp'] = '1'
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].scan_for_bss(hapd.own_addr(), freq=2412)
    hapd.request("SET ext_eapol_frame_io 1")
    dev[0].request("SET ext_eapol_frame_io 1")

    # Successful exchange to verify proxying mechanism
    start_md5_assoc(dev[0], hapd)
    proxy_msg(dev[0], hapd) # MD5 Response
    proxy_msg(hapd, dev[0]) # EAP-Success
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-SUCCESS"], timeout=5)
    if ev is None:
        raise Exception("No EAP-Success reported")
    stop_md5_assoc(dev[0], hapd)

    start_md5_assoc(dev[0], hapd)
    resp = rx_msg(dev[0])
    # Too short EAP-MD5 header (no length field)
    hapd.note("EAP-MD5: Invalid frame")
    msg = resp[0:4] + "0005" + resp[8:12] + "0005" + "04"
    tx_msg(dev[0], hapd, msg)
    # Too short EAP-MD5 header (no length field)
    hapd.note("EAP-MD5: Invalid response (response_len=0 payload_len=1")
    msg = resp[0:4] + "0006" + resp[8:12] + "0006" + "0400"
    tx_msg(dev[0], hapd, msg)
    stop_md5_assoc(dev[0], hapd)

def test_eap_proto_otp(dev, apdev):
    """EAP-OTP protocol tests"""
    def otp_handler(ctx, req):
        logger.info("otp_handler - RX " + binascii.hexlify(req).decode())
        if 'num' not in ctx:
            ctx['num'] = 0
        ctx['num'] = ctx['num'] + 1
        if 'id' not in ctx:
            ctx['id'] = 1
        ctx['id'] = (ctx['id'] + 1) % 256

        if ctx['num'] == 1:
            logger.info("Test: Empty payload")
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1,
                               EAP_TYPE_OTP)
        if ctx['num'] == 2:
            logger.info("Test: Success")
            return struct.pack(">BBH", EAP_CODE_SUCCESS, ctx['id'],
                               4)

        if ctx['num'] == 3:
            logger.info("Test: Challenge included")
            return struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1,
                               EAP_TYPE_OTP,
                               ord('A'))
        if ctx['num'] == 4:
            logger.info("Test: Success")
            return struct.pack(">BBH", EAP_CODE_SUCCESS, ctx['id'],
                               4)

        return None

    srv = start_radius_server(otp_handler)

    try:
        hapd = start_ap(apdev[0])
        dev[0].scan_for_bss(hapd.own_addr(), freq=2412)

        for i in range(0, 1):
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="OTP", identity="user", password="password",
                           wait_connect=False)
            ev = dev[0].wait_event(["CTRL-EVENT-EAP-PROPOSED-METHOD"],
                                   timeout=15)
            if ev is None:
                raise Exception("Timeout on EAP start")
            time.sleep(0.1)
            dev[0].request("REMOVE_NETWORK all")

        dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                       eap="OTP", identity="user", wait_connect=False)
        ev = dev[0].wait_event(["CTRL-REQ-OTP"])
        if ev is None:
            raise Exception("Request for password timed out")
        id = ev.split(':')[0].split('-')[-1]
        dev[0].request("CTRL-RSP-OTP-" + id + ":password")
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-SUCCESS"])
        if ev is None:
            raise Exception("Success not reported")
    finally:
        stop_radius_server(srv)

def test_eap_proto_otp_errors(dev, apdev):
    """EAP-OTP local error cases"""
    def otp_handler2(ctx, req):
        logger.info("otp_handler2 - RX " + binascii.hexlify(req).decode())
        if 'num' not in ctx:
            ctx['num'] = 0
        ctx['num'] = ctx['num'] + 1
        if 'id' not in ctx:
            ctx['id'] = 1
        ctx['id'] = (ctx['id'] + 1) % 256
        idx = 0

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Challenge included")
            return struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1,
                               EAP_TYPE_OTP,
                               ord('A'))

        return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

    srv = start_radius_server(otp_handler2)

    try:
        hapd = start_ap(apdev[0])
        dev[0].scan_for_bss(hapd.own_addr(), freq=2412)

        with alloc_fail(dev[0], 1, "eap_msg_alloc;eap_otp_process"):
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="OTP", identity="user", password="password",
                           wait_connect=False)
            wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()
    finally:
        stop_radius_server(srv)

EAP_GPSK_OPCODE_GPSK_1 = 1
EAP_GPSK_OPCODE_GPSK_2 = 2
EAP_GPSK_OPCODE_GPSK_3 = 3
EAP_GPSK_OPCODE_GPSK_4 = 4
EAP_GPSK_OPCODE_FAIL = 5
EAP_GPSK_OPCODE_PROTECTED_FAIL = 6

def test_eap_proto_gpsk(dev, apdev):
    """EAP-GPSK protocol tests"""
    def gpsk_handler(ctx, req):
        logger.info("gpsk_handler - RX " + binascii.hexlify(req).decode())
        if 'num' not in ctx:
            ctx['num'] = 0
        ctx['num'] = ctx['num'] + 1
        if 'id' not in ctx:
            ctx['id'] = 1
        ctx['id'] = (ctx['id'] + 1) % 256

        idx = 0

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Missing payload")
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1,
                               EAP_TYPE_GPSK)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unknown opcode")
            return struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1,
                               EAP_TYPE_GPSK,
                               255)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unexpected GPSK-3")
            return struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1,
                               EAP_TYPE_GPSK,
                               EAP_GPSK_OPCODE_GPSK_3)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: GPSK-1 Too short GPSK-1")
            return struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1,
                               EAP_TYPE_GPSK,
                               EAP_GPSK_OPCODE_GPSK_1)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: GPSK-1 Truncated ID_Server")
            return struct.pack(">BBHBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 2,
                               EAP_TYPE_GPSK,
                               EAP_GPSK_OPCODE_GPSK_1, 1)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: GPSK-1 Missing RAND_Server")
            return struct.pack(">BBHBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 2,
                               EAP_TYPE_GPSK,
                               EAP_GPSK_OPCODE_GPSK_1, 0)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: GPSK-1 Missing CSuite_List")
            return struct.pack(">BBHBBH8L", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 2 + 32,
                               EAP_TYPE_GPSK,
                               EAP_GPSK_OPCODE_GPSK_1, 0,
                               0, 0, 0, 0, 0, 0, 0, 0)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: GPSK-1 Truncated CSuite_List")
            return struct.pack(">BBHBBH8LH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 2 + 32 + 2,
                               EAP_TYPE_GPSK,
                               EAP_GPSK_OPCODE_GPSK_1, 0,
                               0, 0, 0, 0, 0, 0, 0, 0,
                               1)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: GPSK-1 Empty CSuite_List")
            return struct.pack(">BBHBBH8LH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 2 + 32 + 2,
                               EAP_TYPE_GPSK,
                               EAP_GPSK_OPCODE_GPSK_1, 0,
                               0, 0, 0, 0, 0, 0, 0, 0,
                               0)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: GPSK-1 Invalid CSuite_List")
            return struct.pack(">BBHBBH8LHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 2 + 32 + 2 + 1,
                               EAP_TYPE_GPSK,
                               EAP_GPSK_OPCODE_GPSK_1, 0,
                               0, 0, 0, 0, 0, 0, 0, 0,
                               1, 0)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: GPSK-1 No supported CSuite")
            return struct.pack(">BBHBBH8LHLH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 2 + 32 + 2 + 6,
                               EAP_TYPE_GPSK,
                               EAP_GPSK_OPCODE_GPSK_1, 0,
                               0, 0, 0, 0, 0, 0, 0, 0,
                               6, 0, 0)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: GPSK-1 Supported CSuite")
            return struct.pack(">BBHBBH8LHLH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 2 + 32 + 2 + 6,
                               EAP_TYPE_GPSK,
                               EAP_GPSK_OPCODE_GPSK_1, 0,
                               0, 0, 0, 0, 0, 0, 0, 0,
                               6, 0, 1)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unexpected GPSK-1")
            return struct.pack(">BBHBBH8LHLH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 2 + 32 + 2 + 6,
                               EAP_TYPE_GPSK,
                               EAP_GPSK_OPCODE_GPSK_1, 0,
                               0, 0, 0, 0, 0, 0, 0, 0,
                               6, 0, 1)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: GPSK-1 Supported CSuite but too short key")
            return struct.pack(">BBHBBH8LHLH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 2 + 32 + 2 + 6,
                               EAP_TYPE_GPSK,
                               EAP_GPSK_OPCODE_GPSK_1, 0,
                               0, 0, 0, 0, 0, 0, 0, 0,
                               6, 0, 1)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: GPSK-1 Supported CSuite")
            return struct.pack(">BBHBBH8LHLH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 2 + 32 + 2 + 6,
                               EAP_TYPE_GPSK,
                               EAP_GPSK_OPCODE_GPSK_1, 0,
                               0, 0, 0, 0, 0, 0, 0, 0,
                               6, 0, 1)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Too short GPSK-3")
            return struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1,
                               EAP_TYPE_GPSK,
                               EAP_GPSK_OPCODE_GPSK_3)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: GPSK-1 Supported CSuite")
            return struct.pack(">BBHBBH8LHLH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 2 + 32 + 2 + 6,
                               EAP_TYPE_GPSK,
                               EAP_GPSK_OPCODE_GPSK_1, 0,
                               0, 0, 0, 0, 0, 0, 0, 0,
                               6, 0, 1)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: GPSK-3 Mismatch in RAND_Peer")
            return struct.pack(">BBHBB8L", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 32,
                               EAP_TYPE_GPSK,
                               EAP_GPSK_OPCODE_GPSK_3,
                               0, 0, 0, 0, 0, 0, 0, 0)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: GPSK-1 Supported CSuite")
            return struct.pack(">BBHBBH8LHLH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 2 + 32 + 2 + 6,
                               EAP_TYPE_GPSK,
                               EAP_GPSK_OPCODE_GPSK_1, 0,
                               0, 0, 0, 0, 0, 0, 0, 0,
                               6, 0, 1)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: GPSK-3 Missing RAND_Server")
            msg = struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                              4 + 1 + 1 + 32,
                              EAP_TYPE_GPSK,
                              EAP_GPSK_OPCODE_GPSK_3)
            msg += req[14:46]
            return msg

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: GPSK-1 Supported CSuite")
            return struct.pack(">BBHBBH8LHLH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 2 + 32 + 2 + 6,
                               EAP_TYPE_GPSK,
                               EAP_GPSK_OPCODE_GPSK_1, 0,
                               0, 0, 0, 0, 0, 0, 0, 0,
                               6, 0, 1)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: GPSK-3 Mismatch in RAND_Server")
            msg = struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                              4 + 1 + 1 + 32 + 32,
                              EAP_TYPE_GPSK,
                              EAP_GPSK_OPCODE_GPSK_3)
            msg += req[14:46]
            msg += struct.pack(">8L", 1, 1, 1, 1, 1, 1, 1, 1)
            return msg

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: GPSK-1 Supported CSuite")
            return struct.pack(">BBHBBH8LHLH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 2 + 32 + 2 + 6,
                               EAP_TYPE_GPSK,
                               EAP_GPSK_OPCODE_GPSK_1, 0,
                               0, 0, 0, 0, 0, 0, 0, 0,
                               6, 0, 1)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: GPSK-3 Missing ID_Server")
            msg = struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                              4 + 1 + 1 + 32 + 32,
                              EAP_TYPE_GPSK,
                              EAP_GPSK_OPCODE_GPSK_3)
            msg += req[14:46]
            msg += struct.pack(">8L", 0, 0, 0, 0, 0, 0, 0, 0)
            return msg

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: GPSK-1 Supported CSuite")
            return struct.pack(">BBHBBH8LHLH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 2 + 32 + 2 + 6,
                               EAP_TYPE_GPSK,
                               EAP_GPSK_OPCODE_GPSK_1, 0,
                               0, 0, 0, 0, 0, 0, 0, 0,
                               6, 0, 1)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: GPSK-3 Truncated ID_Server")
            msg = struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                              4 + 1 + 1 + 32 + 32 + 2,
                              EAP_TYPE_GPSK,
                              EAP_GPSK_OPCODE_GPSK_3)
            msg += req[14:46]
            msg += struct.pack(">8LH", 0, 0, 0, 0, 0, 0, 0, 0, 1)
            return msg

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: GPSK-1 Supported CSuite")
            return struct.pack(">BBHBBH8LHLH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 2 + 32 + 2 + 6,
                               EAP_TYPE_GPSK,
                               EAP_GPSK_OPCODE_GPSK_1, 0,
                               0, 0, 0, 0, 0, 0, 0, 0,
                               6, 0, 1)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: GPSK-3 Mismatch in ID_Server")
            msg = struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                              4 + 1 + 1 + 32 + 32 + 3,
                              EAP_TYPE_GPSK,
                              EAP_GPSK_OPCODE_GPSK_3)
            msg += req[14:46]
            msg += struct.pack(">8LHB", 0, 0, 0, 0, 0, 0, 0, 0, 1, ord('B'))
            return msg

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: GPSK-1 Supported CSuite")
            return struct.pack(">BBHBBHB8LHLH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 3 + 32 + 2 + 6,
                               EAP_TYPE_GPSK,
                               EAP_GPSK_OPCODE_GPSK_1, 1, ord('A'),
                               0, 0, 0, 0, 0, 0, 0, 0,
                               6, 0, 1)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: GPSK-3 Mismatch in ID_Server (same length)")
            msg = struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                              4 + 1 + 1 + 32 + 32 + 3,
                              EAP_TYPE_GPSK,
                              EAP_GPSK_OPCODE_GPSK_3)
            msg += req[15:47]
            msg += struct.pack(">8LHB", 0, 0, 0, 0, 0, 0, 0, 0, 1, ord('B'))
            return msg

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: GPSK-1 Supported CSuite")
            return struct.pack(">BBHBBH8LHLH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 2 + 32 + 2 + 6,
                               EAP_TYPE_GPSK,
                               EAP_GPSK_OPCODE_GPSK_1, 0,
                               0, 0, 0, 0, 0, 0, 0, 0,
                               6, 0, 1)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: GPSK-3 Missing CSuite_Sel")
            msg = struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                              4 + 1 + 1 + 32 + 32 + 2,
                              EAP_TYPE_GPSK,
                              EAP_GPSK_OPCODE_GPSK_3)
            msg += req[14:46]
            msg += struct.pack(">8LH", 0, 0, 0, 0, 0, 0, 0, 0, 0)
            return msg

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: GPSK-1 Supported CSuite")
            return struct.pack(">BBHBBH8LHLH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 2 + 32 + 2 + 6,
                               EAP_TYPE_GPSK,
                               EAP_GPSK_OPCODE_GPSK_1, 0,
                               0, 0, 0, 0, 0, 0, 0, 0,
                               6, 0, 1)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: GPSK-3 Mismatch in CSuite_Sel")
            msg = struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                              4 + 1 + 1 + 32 + 32 + 2 + 6,
                              EAP_TYPE_GPSK,
                              EAP_GPSK_OPCODE_GPSK_3)
            msg += req[14:46]
            msg += struct.pack(">8LHLH", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2)
            return msg

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: GPSK-1 Supported CSuite")
            return struct.pack(">BBHBBH8LHLH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 2 + 32 + 2 + 6,
                               EAP_TYPE_GPSK,
                               EAP_GPSK_OPCODE_GPSK_1, 0,
                               0, 0, 0, 0, 0, 0, 0, 0,
                               6, 0, 1)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: GPSK-3 Missing len(PD_Payload_Block)")
            msg = struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                              4 + 1 + 1 + 32 + 32 + 2 + 6,
                              EAP_TYPE_GPSK,
                              EAP_GPSK_OPCODE_GPSK_3)
            msg += req[14:46]
            msg += struct.pack(">8LHLH", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1)
            return msg

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: GPSK-1 Supported CSuite")
            return struct.pack(">BBHBBH8LHLH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 2 + 32 + 2 + 6,
                               EAP_TYPE_GPSK,
                               EAP_GPSK_OPCODE_GPSK_1, 0,
                               0, 0, 0, 0, 0, 0, 0, 0,
                               6, 0, 1)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: GPSK-3 Truncated PD_Payload_Block")
            msg = struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                              4 + 1 + 1 + 32 + 32 + 2 + 6 + 2,
                              EAP_TYPE_GPSK,
                              EAP_GPSK_OPCODE_GPSK_3)
            msg += req[14:46]
            msg += struct.pack(">8LHLHH", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1)
            return msg

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: GPSK-1 Supported CSuite")
            return struct.pack(">BBHBBH8LHLH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 2 + 32 + 2 + 6,
                               EAP_TYPE_GPSK,
                               EAP_GPSK_OPCODE_GPSK_1, 0,
                               0, 0, 0, 0, 0, 0, 0, 0,
                               6, 0, 1)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: GPSK-3 Missing MAC")
            msg = struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                              4 + 1 + 1 + 32 + 32 + 2 + 6 + 3,
                              EAP_TYPE_GPSK,
                              EAP_GPSK_OPCODE_GPSK_3)
            msg += req[14:46]
            msg += struct.pack(">8LHLHHB",
                               0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 123)
            return msg

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: GPSK-1 Supported CSuite")
            return struct.pack(">BBHBBH8LHLH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 2 + 32 + 2 + 6,
                               EAP_TYPE_GPSK,
                               EAP_GPSK_OPCODE_GPSK_1, 0,
                               0, 0, 0, 0, 0, 0, 0, 0,
                               6, 0, 1)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: GPSK-3 Incorrect MAC")
            msg = struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                              4 + 1 + 1 + 32 + 32 + 2 + 6 + 3 + 16,
                              EAP_TYPE_GPSK,
                              EAP_GPSK_OPCODE_GPSK_3)
            msg += req[14:46]
            msg += struct.pack(">8LHLHHB4L",
                               0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 123,
                               0, 0, 0, 0)
            return msg

        return None

    srv = start_radius_server(gpsk_handler)

    try:
        hapd = start_ap(apdev[0])
        dev[0].scan_for_bss(hapd.own_addr(), freq=2412)

        for i in range(0, 27):
            if i == 12:
                pw = "short"
            else:
                pw = "abcdefghijklmnop0123456789abcdef"
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="GPSK", identity="user", password=pw,
                           wait_connect=False)
            ev = dev[0].wait_event(["CTRL-EVENT-EAP-PROPOSED-METHOD"],
                                   timeout=15)
            if ev is None:
                raise Exception("Timeout on EAP start")
            time.sleep(0.05)
            dev[0].request("REMOVE_NETWORK all")
    finally:
        stop_radius_server(srv)

def run_eap_gpsk_connect(dev):
    dev.connect("test-wpa2-eap", key_mgmt="WPA-EAP", scan_freq="2412",
                eap="GPSK", identity="gpsk user",
                password="abcdefghijklmnop0123456789abcdef",
                wait_connect=False)
    ev = dev.wait_event(["CTRL-EVENT-EAP-SUCCESS", "CTRL-EVENT-EAP-FAILURE",
                         "CTRL-EVENT-DISCONNECTED"],
                        timeout=1)
    dev.request("REMOVE_NETWORK all")
    if not ev or "CTRL-EVENT-DISCONNECTED" not in ev:
        dev.wait_disconnected()
    dev.dump_monitor()

def test_eap_proto_gpsk_errors_server(dev, apdev):
    """EAP-GPSK local error cases on server"""
    check_eap_capa(dev[0], "GPSK")
    params = int_eap_server_params()
    params['erp_domain'] = 'example.com'
    params['eap_server_erp'] = '1'
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].scan_for_bss(hapd.own_addr(), freq=2412)

    tests = [(1, "eap_gpsk_init"),
             (1, "eap_msg_alloc;eap_gpsk_build_gpsk_1"),
             (1, "eap_msg_alloc;eap_gpsk_build_gpsk_3"),
             (1, "eap_gpsk_process_gpsk_2"),
             (1, "eap_gpsk_derive_keys;eap_gpsk_process_gpsk_2"),
             (1, "eap_gpsk_derive_session_id;eap_gpsk_process_gpsk_2"),
             (1, "eap_gpsk_getKey"),
             (1, "eap_gpsk_get_emsk"),
             (1, "eap_gpsk_get_session_id")]
    for count, func in tests:
        with alloc_fail(hapd, count, func):
            run_eap_gpsk_connect(dev[0])

    tests = [(1, "os_get_random;eap_gpsk_build_gpsk_1"),
             (1, "eap_gpsk_compute_mic;eap_gpsk_build_gpsk_3"),
             (1, "eap_gpsk_derive_keys;eap_gpsk_process_gpsk_2"),
             (1, "eap_gpsk_derive_session_id;eap_gpsk_process_gpsk_2"),
             (1, "eap_gpsk_compute_mic;eap_gpsk_process_gpsk_2"),
             (1, "eap_gpsk_compute_mic;eap_gpsk_process_gpsk_4")]
    for count, func in tests:
        with fail_test(hapd, count, func):
            run_eap_gpsk_connect(dev[0])

def start_gpsk_assoc(dev, hapd):
    dev.connect("test-wpa2-eap", key_mgmt="WPA-EAP", scan_freq="2412",
                eap="GPSK", identity="gpsk user",
                password="abcdefghijklmnop0123456789abcdef",
                wait_connect=False)
    proxy_msg(hapd, dev) # EAP-Identity/Request
    proxy_msg(dev, hapd) # EAP-Identity/Response
    proxy_msg(hapd, dev) # GPSK-1

def stop_gpsk_assoc(dev, hapd):
    dev.request("REMOVE_NETWORK all")
    dev.wait_disconnected()
    dev.dump_monitor()
    hapd.dump_monitor()

def test_eap_proto_gpsk_server(dev, apdev):
    """EAP-GPSK protocol testing for the server"""
    check_eap_capa(dev[0], "GPSK")
    params = int_eap_server_params()
    params['erp_domain'] = 'example.com'
    params['eap_server_erp'] = '1'
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].scan_for_bss(hapd.own_addr(), freq=2412)
    hapd.request("SET ext_eapol_frame_io 1")
    dev[0].request("SET ext_eapol_frame_io 1")

    # Successful exchange to verify proxying mechanism
    start_gpsk_assoc(dev[0], hapd)
    proxy_msg(dev[0], hapd) # GPSK-2
    proxy_msg(hapd, dev[0]) # GPSK-3
    proxy_msg(dev[0], hapd) # GPSK-4
    proxy_msg(hapd, dev[0]) # EAP-Success
    proxy_msg(hapd, dev[0]) # EAPOL-Key msg 1/4
    proxy_msg(dev[0], hapd) # EAPOL-Key msg 2/4
    proxy_msg(hapd, dev[0]) # EAPOL-Key msg 3/4
    proxy_msg(dev[0], hapd) # EAPOL-Key msg 4/4
    dev[0].wait_connected()
    stop_gpsk_assoc(dev[0], hapd)

    start_gpsk_assoc(dev[0], hapd)
    resp = rx_msg(dev[0])
    # Too short EAP-GPSK header (no OP-Code)
    # --> EAP-GPSK: Invalid frame
    msg = resp[0:4] + "0005" + resp[8:12] + "0005" + "33"
    tx_msg(dev[0], hapd, msg)
    # Unknown OP-Code
    # --> EAP-GPSK: Unexpected opcode=7 in state=0
    msg = resp[0:4] + "0006" + resp[8:12] + "0006" + "3307"
    tx_msg(dev[0], hapd, msg)
    # Too short GPSK-2
    # --> EAP-GPSK: Too short message for ID_Peer length
    msg = resp[0:4] + "0006" + resp[8:12] + "0006" + "3302"
    tx_msg(dev[0], hapd, msg)
    rx_msg(hapd)
    stop_gpsk_assoc(dev[0], hapd)

    start_gpsk_assoc(dev[0], hapd)
    resp = rx_msg(dev[0])
    # Too short GPSK-2
    # --> EAP-GPSK: Too short message for ID_Peer
    msg = resp[0:4] + "0008" + resp[8:12] + "0008" + "33020001"
    tx_msg(dev[0], hapd, msg)
    rx_msg(hapd)
    stop_gpsk_assoc(dev[0], hapd)

    start_gpsk_assoc(dev[0], hapd)
    resp = rx_msg(dev[0])
    # Too short GPSK-2
    # --> EAP-GPSK: Too short message for ID_Server length
    msg = resp[0:4] + "0008" + resp[8:12] + "0008" + "33020000"
    tx_msg(dev[0], hapd, msg)
    rx_msg(hapd)
    stop_gpsk_assoc(dev[0], hapd)

    start_gpsk_assoc(dev[0], hapd)
    resp = rx_msg(dev[0])
    # Too short GPSK-2
    # --> EAP-GPSK: Too short message for ID_Server
    msg = resp[0:4] + "000a" + resp[8:12] + "000a" + "330200000001"
    tx_msg(dev[0], hapd, msg)
    rx_msg(hapd)
    stop_gpsk_assoc(dev[0], hapd)

    start_gpsk_assoc(dev[0], hapd)
    resp = rx_msg(dev[0])
    # ID_Server mismatch
    # --> EAP-GPSK: ID_Server in GPSK-1 and GPSK-2 did not match
    msg = resp[0:4] + "000a" + resp[8:12] + "000a" + "330200000000"
    tx_msg(dev[0], hapd, msg)
    rx_msg(hapd)
    stop_gpsk_assoc(dev[0], hapd)

    start_gpsk_assoc(dev[0], hapd)
    resp = rx_msg(dev[0])
    # Too short GPSK-2
    # --> EAP-GPSK: Too short message for RAND_Peer
    msg = resp[0:4] + "0011" + resp[8:12] + "0011" + "330200000007" + binascii.hexlify(b"hostapd").decode()
    tx_msg(dev[0], hapd, msg)
    rx_msg(hapd)
    stop_gpsk_assoc(dev[0], hapd)

    start_gpsk_assoc(dev[0], hapd)
    resp = rx_msg(dev[0])
    # Too short GPSK-2
    # --> EAP-GPSK: Too short message for RAND_Server
    msg = resp[0:4] + "0031" + resp[8:12] + "0031" + "330200000007" + binascii.hexlify(b"hostapd").decode() + 32*"00"
    tx_msg(dev[0], hapd, msg)
    rx_msg(hapd)
    stop_gpsk_assoc(dev[0], hapd)

    start_gpsk_assoc(dev[0], hapd)
    resp = rx_msg(dev[0])
    # RAND_Server mismatch
    # --> EAP-GPSK: RAND_Server in GPSK-1 and GPSK-2 did not match
    msg = resp[0:4] + "0051" + resp[8:12] + "0051" + "330200000007" + binascii.hexlify(b"hostapd").decode() + 32*"00" + 32*"00"
    tx_msg(dev[0], hapd, msg)
    rx_msg(hapd)
    stop_gpsk_assoc(dev[0], hapd)

    start_gpsk_assoc(dev[0], hapd)
    resp = rx_msg(dev[0])
    # Too short GPSK-2
    # --> EAP-GPSK: Too short message for CSuite_List length
    msg = resp[0:4] + "005a" + resp[8:12] + "005a" + resp[16:188]
    tx_msg(dev[0], hapd, msg)
    rx_msg(hapd)
    stop_gpsk_assoc(dev[0], hapd)

    start_gpsk_assoc(dev[0], hapd)
    resp = rx_msg(dev[0])
    # Too short GPSK-2
    # --> EAP-GPSK: Too short message for CSuite_List
    msg = resp[0:4] + "005c" + resp[8:12] + "005c" + resp[16:192]
    tx_msg(dev[0], hapd, msg)
    rx_msg(hapd)
    stop_gpsk_assoc(dev[0], hapd)

    start_gpsk_assoc(dev[0], hapd)
    resp = rx_msg(dev[0])
    # Too short GPSK-2
    # --> EAP-GPSK: CSuite_List in GPSK-1 and GPSK-2 did not match
    msg = resp[0:4] + "005c" + resp[8:12] + "005c" + resp[16:188] + "0000"
    tx_msg(dev[0], hapd, msg)
    rx_msg(hapd)
    stop_gpsk_assoc(dev[0], hapd)

    start_gpsk_assoc(dev[0], hapd)
    resp = rx_msg(dev[0])
    # Too short GPSK-2
    # --> EAP-GPSK: Too short message for CSuite_Sel
    msg = resp[0:4] + "0068" + resp[8:12] + "0068" + resp[16:216]
    tx_msg(dev[0], hapd, msg)
    rx_msg(hapd)
    stop_gpsk_assoc(dev[0], hapd)

    start_gpsk_assoc(dev[0], hapd)
    resp = rx_msg(dev[0])
    # Unsupported CSuite_Sel
    # --> EAP-GPSK: Peer selected unsupported ciphersuite 0:255
    msg = resp[0:4] + "006e" + resp[8:12] + "006e" + resp[16:226] + "ff"
    tx_msg(dev[0], hapd, msg)
    rx_msg(hapd)
    stop_gpsk_assoc(dev[0], hapd)

    start_gpsk_assoc(dev[0], hapd)
    resp = rx_msg(dev[0])
    # Too short GPSK-2
    # --> EAP-GPSK: Too short message for PD_Payload_1 length
    msg = resp[0:4] + "006e" + resp[8:12] + "006e" + resp[16:228]
    tx_msg(dev[0], hapd, msg)
    rx_msg(hapd)
    stop_gpsk_assoc(dev[0], hapd)

    start_gpsk_assoc(dev[0], hapd)
    resp = rx_msg(dev[0])
    # Too short GPSK-2
    # --> EAP-GPSK: Too short message for PD_Payload_1
    msg = resp[0:4] + "0070" + resp[8:12] + "0070" + resp[16:230] + "ff"
    tx_msg(dev[0], hapd, msg)
    rx_msg(hapd)
    stop_gpsk_assoc(dev[0], hapd)

    start_gpsk_assoc(dev[0], hapd)
    resp = rx_msg(dev[0])
    # Too short GPSK-2
    # --> EAP-GPSK: Message too short for MIC (left=0 miclen=16)
    msg = resp[0:4] + "0070" + resp[8:12] + "0070" + resp[16:232]
    tx_msg(dev[0], hapd, msg)
    rx_msg(hapd)
    stop_gpsk_assoc(dev[0], hapd)

    start_gpsk_assoc(dev[0], hapd)
    resp = rx_msg(dev[0])
    # Extra data in the end of GPSK-2
    # --> EAP-GPSK: Ignored 1 bytes of extra data in the end of GPSK-2
    msg = resp[0:4] + "0081" + resp[8:12] + "0081" + resp[16:264] + "00"
    tx_msg(dev[0], hapd, msg)
    proxy_msg(hapd, dev[0]) # GPSK-3
    resp = rx_msg(dev[0])
    # Too short GPSK-4
    # --> EAP-GPSK: Too short message for PD_Payload_1 length
    msg = resp[0:4] + "0006" + resp[8:12] + "0006" + "3304"
    tx_msg(dev[0], hapd, msg)
    rx_msg(hapd) # EAP-Failure
    stop_gpsk_assoc(dev[0], hapd)

    start_gpsk_assoc(dev[0], hapd)
    proxy_msg(dev[0], hapd) # GPSK-2
    proxy_msg(hapd, dev[0]) # GPSK-3
    resp = rx_msg(dev[0])
    # Too short GPSK-4
    # --> EAP-GPSK: Too short message for PD_Payload_1
    msg = resp[0:4] + "0008" + resp[8:12] + "0008" + "33040001"
    tx_msg(dev[0], hapd, msg)
    rx_msg(hapd) # EAP-Failure
    stop_gpsk_assoc(dev[0], hapd)

    start_gpsk_assoc(dev[0], hapd)
    proxy_msg(dev[0], hapd) # GPSK-2
    proxy_msg(hapd, dev[0]) # GPSK-3
    resp = rx_msg(dev[0])
    # Too short GPSK-4
    # --> EAP-GPSK: Message too short for MIC (left=0 miclen=16)
    msg = resp[0:4] + "0008" + resp[8:12] + "0008" + "33040000"
    tx_msg(dev[0], hapd, msg)
    rx_msg(hapd) # EAP-Failure
    stop_gpsk_assoc(dev[0], hapd)

    start_gpsk_assoc(dev[0], hapd)
    proxy_msg(dev[0], hapd) # GPSK-2
    proxy_msg(hapd, dev[0]) # GPSK-3
    resp = rx_msg(dev[0])
    # Incorrect MIC in GPSK-4
    # --> EAP-GPSK: Incorrect MIC in GPSK-4
    msg = resp[0:4] + "0018" + resp[8:12] + "0018" + "33040000" + 16*"00"
    tx_msg(dev[0], hapd, msg)
    rx_msg(hapd) # EAP-Failure
    stop_gpsk_assoc(dev[0], hapd)

    start_gpsk_assoc(dev[0], hapd)
    proxy_msg(dev[0], hapd) # GPSK-2
    proxy_msg(hapd, dev[0]) # GPSK-3
    resp = rx_msg(dev[0])
    # Incorrect MIC in GPSK-4
    # --> EAP-GPSK: Ignored 1 bytes of extra data in the end of GPSK-4
    msg = resp[0:4] + "0019" + resp[8:12] + "0019" + resp[16:] + "00"
    tx_msg(dev[0], hapd, msg)
    rx_msg(hapd) # EAP-Success
    stop_gpsk_assoc(dev[0], hapd)

EAP_EKE_ID = 1
EAP_EKE_COMMIT = 2
EAP_EKE_CONFIRM = 3
EAP_EKE_FAILURE = 4

def test_eap_proto_eke(dev, apdev):
    """EAP-EKE protocol tests"""
    def eke_handler(ctx, req):
        logger.info("eke_handler - RX " + binascii.hexlify(req).decode())
        if 'num' not in ctx:
            ctx['num'] = 0
        ctx['num'] = ctx['num'] + 1
        if 'id' not in ctx:
            ctx['id'] = 1
        ctx['id'] = (ctx['id'] + 1) % 256

        idx = 0

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Missing payload")
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1,
                               EAP_TYPE_EKE)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unknown exchange")
            return struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1,
                               EAP_TYPE_EKE,
                               255)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: No NumProposals in EAP-EKE-ID/Request")
            return struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1,
                               EAP_TYPE_EKE,
                               EAP_EKE_ID)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: NumProposals=0 in EAP-EKE-ID/Request")
            return struct.pack(">BBHBBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 1,
                               EAP_TYPE_EKE,
                               EAP_EKE_ID,
                               0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Truncated Proposals list in EAP-EKE-ID/Request")
            return struct.pack(">BBHBBBB4B", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 2 + 4,
                               EAP_TYPE_EKE,
                               EAP_EKE_ID,
                               2, 0, 0, 0, 0, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unsupported proposals in EAP-EKE-ID/Request")
            return struct.pack(">BBHBBBB4B4B4B4B", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 2 + 4 * 4,
                               EAP_TYPE_EKE,
                               EAP_EKE_ID,
                               4, 0,
                               0, 0, 0, 0,
                               3, 0, 0, 0,
                               3, 1, 0, 0,
                               3, 1, 1, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Missing IDType/Identity in EAP-EKE-ID/Request")
            return struct.pack(">BBHBBBB4B4B4B4B4B",
                               EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 2 + 5 * 4,
                               EAP_TYPE_EKE,
                               EAP_EKE_ID,
                               5, 0,
                               0, 0, 0, 0,
                               3, 0, 0, 0,
                               3, 1, 0, 0,
                               3, 1, 1, 0,
                               3, 1, 1, 1)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Valid EAP-EKE-ID/Request")
            return struct.pack(">BBHBBBB4BB",
                               EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 2 + 4 + 1,
                               EAP_TYPE_EKE,
                               EAP_EKE_ID,
                               1, 0,
                               3, 1, 1, 1,
                               255)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unexpected EAP-EKE-ID/Request")
            return struct.pack(">BBHBBBB4BB",
                               EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 2 + 4 + 1,
                               EAP_TYPE_EKE,
                               EAP_EKE_ID,
                               1, 0,
                               3, 1, 1, 1,
                               255)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Valid EAP-EKE-ID/Request")
            return struct.pack(">BBHBBBB4BB",
                               EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 2 + 4 + 1,
                               EAP_TYPE_EKE,
                               EAP_EKE_ID,
                               1, 0,
                               3, 1, 1, 1,
                               255)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unexpected EAP-EKE-Confirm/Request")
            return struct.pack(">BBHBB",
                               EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1,
                               EAP_TYPE_EKE,
                               EAP_EKE_CONFIRM)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Too short EAP-EKE-Failure/Request")
            return struct.pack(">BBHBB",
                               EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1,
                               EAP_TYPE_EKE,
                               EAP_EKE_FAILURE)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unexpected EAP-EKE-Commit/Request")
            return struct.pack(">BBHBB",
                               EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1,
                               EAP_TYPE_EKE,
                               EAP_EKE_COMMIT)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Valid EAP-EKE-ID/Request")
            return struct.pack(">BBHBBBB4BB",
                               EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 2 + 4 + 1,
                               EAP_TYPE_EKE,
                               EAP_EKE_ID,
                               1, 0,
                               3, 1, 1, 1,
                               255)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Too short EAP-EKE-Commit/Request")
            return struct.pack(">BBHBB",
                               EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1,
                               EAP_TYPE_EKE,
                               EAP_EKE_COMMIT)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Valid EAP-EKE-ID/Request")
            return struct.pack(">BBHBBBB4BB",
                               EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 2 + 4 + 1,
                               EAP_TYPE_EKE,
                               EAP_EKE_ID,
                               1, 0,
                               1, 1, 1, 1,
                               255)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: All zeroes DHComponent_S and empty CBvalue in EAP-EKE-Commit/Request")
            return struct.pack(">BBHBB4L32L",
                               EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 16 + 128,
                               EAP_TYPE_EKE,
                               EAP_EKE_COMMIT,
                               0, 0, 0, 0,
                               0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                               0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Too short EAP-EKE-Confirm/Request")
            return struct.pack(">BBHBB",
                               EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1,
                               EAP_TYPE_EKE,
                               EAP_EKE_CONFIRM)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Valid EAP-EKE-ID/Request")
            return struct.pack(">BBHBBBB4BB",
                               EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 2 + 4 + 1,
                               EAP_TYPE_EKE,
                               EAP_EKE_ID,
                               1, 0,
                               1, 1, 1, 1,
                               255)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: All zeroes DHComponent_S and empty CBvalue in EAP-EKE-Commit/Request")
            return struct.pack(">BBHBB4L32L",
                               EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 16 + 128,
                               EAP_TYPE_EKE,
                               EAP_EKE_COMMIT,
                               0, 0, 0, 0,
                               0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                               0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Invalid PNonce_PS and Auth_S values in EAP-EKE-Confirm/Request")
            return struct.pack(">BBHBB4L8L5L5L",
                               EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 16 + 2 * 16 + 20 + 20,
                               EAP_TYPE_EKE,
                               EAP_EKE_CONFIRM,
                               0, 0, 0, 0,
                               0, 0, 0, 0, 0, 0, 0, 0,
                               0, 0, 0, 0, 0,
                               0, 0, 0, 0, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        return None

    srv = start_radius_server(eke_handler)

    try:
        hapd = start_ap(apdev[0])
        dev[0].scan_for_bss(hapd.own_addr(), freq=2412)

        for i in range(0, 14):
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="EKE", identity="user", password="password",
                           wait_connect=False)
            ev = dev[0].wait_event(["CTRL-EVENT-EAP-PROPOSED-METHOD"],
                                   timeout=15)
            if ev is None:
                raise Exception("Timeout on EAP start")
            if i in [2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13]:
                ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"],
                                       timeout=10)
                if ev is None:
                    raise Exception("Timeout on EAP failure")
            else:
                time.sleep(0.05)
            dev[0].request("REMOVE_NETWORK all")
            dev[0].dump_monitor()
    finally:
        stop_radius_server(srv)

def eap_eke_test_fail(dev, phase1=None, success=False):
    dev.connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                eap="EKE", identity="eke user@domain", password="hello",
                phase1=phase1, erp="1", wait_connect=False)
    ev = dev.wait_event(["CTRL-EVENT-EAP-FAILURE",
                         "CTRL-EVENT-EAP-SUCCESS"], timeout=5)
    if ev is None:
        raise Exception("Timeout on EAP failure")
    if not success and "CTRL-EVENT-EAP-FAILURE" not in ev:
        raise Exception("EAP did not fail during failure test")
    dev.request("REMOVE_NETWORK all")
    dev.wait_disconnected()

def test_eap_proto_eke_errors(dev, apdev):
    """EAP-EKE local error cases"""
    check_eap_capa(dev[0], "EKE")
    params = hostapd.wpa2_eap_params(ssid="eap-test")
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].scan_for_bss(hapd.own_addr(), freq=2412)

    for i in range(1, 3):
        with alloc_fail(dev[0], i, "eap_eke_init"):
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="EKE", identity="eke user", password="hello",
                           wait_connect=False)
            ev = dev[0].wait_event(["EAP: Failed to initialize EAP method"],
                                   timeout=15)
            if ev is None:
                raise Exception("Timeout on EAP start")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

    tests = [(1, "eap_eke_dh_init", None),
             (1, "eap_eke_prf_hmac_sha1", "dhgroup=3 encr=1 prf=1 mac=1"),
             (1, "eap_eke_prf_hmac_sha256", "dhgroup=5 encr=1 prf=2 mac=2"),
             (1, "eap_eke_prf", None),
             (1, "os_get_random;eap_eke_dhcomp", None),
             (1, "aes_128_cbc_encrypt;eap_eke_dhcomp", None),
             (1, "aes_128_cbc_decrypt;eap_eke_shared_secret", None),
             (1, "hmac_sha256_vector;eap_eke_shared_secret", None),
             (1, "eap_eke_prf_hmac_sha256;eap_eke_derive_ke_ki", None),
             (1, "eap_eke_prf_hmac_sha256;eap_eke_derive_ka", None),
             (1, "eap_eke_prf_hmac_sha256;eap_eke_derive_msk", None),
             (1, "os_get_random;eap_eke_prot", None),
             (1, "aes_128_cbc_decrypt;eap_eke_decrypt_prot", None),
             (1, "eap_eke_derive_key;eap_eke_process_commit", None),
             (1, "eap_eke_dh_init;eap_eke_process_commit", None),
             (1, "eap_eke_shared_secret;eap_eke_process_commit", None),
             (1, "eap_eke_derive_ke_ki;eap_eke_process_commit", None),
             (1, "eap_eke_dhcomp;eap_eke_process_commit", None),
             (1, "os_get_random;eap_eke_process_commit", None),
             (1, "os_get_random;=eap_eke_process_commit", None),
             (1, "eap_eke_prot;eap_eke_process_commit", None),
             (1, "eap_eke_decrypt_prot;eap_eke_process_confirm", None),
             (1, "eap_eke_derive_ka;eap_eke_process_confirm", None),
             (1, "eap_eke_auth;eap_eke_process_confirm", None),
             (2, "eap_eke_auth;eap_eke_process_confirm", None),
             (1, "eap_eke_prot;eap_eke_process_confirm", None),
             (1, "aes_128_cbc_encrypt;eap_eke_prot;eap_eke_process_confirm", None),
             (1, "hmac_sha256;eap_eke_prot;eap_eke_process_confirm", None),
             (1, "eap_eke_derive_msk;eap_eke_process_confirm", None)]
    for count, func, phase1 in tests:
        with fail_test(dev[0], count, func):
            eap_eke_test_fail(dev[0], phase1)

    tests = [(1, "=eap_eke_derive_ke_ki", None),
             (1, "=eap_eke_derive_ka", None),
             (1, "=eap_eke_derive_msk", None),
             (1, "eap_eke_build_msg;eap_eke_process_id", None),
             (1, "wpabuf_alloc;eap_eke_process_id", None),
             (1, "=eap_eke_process_id", None),
             (1, "wpabuf_alloc;=eap_eke_process_id", None),
             (1, "wpabuf_alloc;eap_eke_process_id", None),
             (1, "eap_eke_build_msg;eap_eke_process_commit", None),
             (1, "wpabuf_resize;eap_eke_process_commit", None),
             (1, "eap_eke_build_msg;eap_eke_process_confirm", None)]
    for count, func, phase1 in tests:
        with alloc_fail(dev[0], count, func):
            eap_eke_test_fail(dev[0], phase1)

    tests = [(1, "eap_eke_getKey", None),
             (1, "eap_eke_get_emsk", None),
             (1, "eap_eke_get_session_id", None)]
    for count, func, phase1 in tests:
        with alloc_fail(dev[0], count, func):
            eap_eke_test_fail(dev[0], phase1, success=True)

EAP_PAX_OP_STD_1 = 0x01
EAP_PAX_OP_STD_2 = 0x02
EAP_PAX_OP_STD_3 = 0x03
EAP_PAX_OP_SEC_1 = 0x11
EAP_PAX_OP_SEC_2 = 0x12
EAP_PAX_OP_SEC_3 = 0x13
EAP_PAX_OP_SEC_4 = 0x14
EAP_PAX_OP_SEC_5 = 0x15
EAP_PAX_OP_ACK = 0x21

EAP_PAX_FLAGS_MF = 0x01
EAP_PAX_FLAGS_CE = 0x02
EAP_PAX_FLAGS_AI = 0x04

EAP_PAX_MAC_HMAC_SHA1_128 = 0x01
EAP_PAX_HMAC_SHA256_128 = 0x02

EAP_PAX_DH_GROUP_NONE = 0x00
EAP_PAX_DH_GROUP_2048_MODP = 0x01
EAP_PAX_DH_GROUP_3072_MODP = 0x02
EAP_PAX_DH_GROUP_NIST_ECC_P_256 = 0x03

EAP_PAX_PUBLIC_KEY_NONE = 0x00
EAP_PAX_PUBLIC_KEY_RSAES_OAEP = 0x01
EAP_PAX_PUBLIC_KEY_RSA_PKCS1_V1_5 = 0x02
EAP_PAX_PUBLIC_KEY_EL_GAMAL_NIST_ECC = 0x03

EAP_PAX_ADE_VENDOR_SPECIFIC = 0x01
EAP_PAX_ADE_CLIENT_CHANNEL_BINDING = 0x02
EAP_PAX_ADE_SERVER_CHANNEL_BINDING = 0x03

def test_eap_proto_pax(dev, apdev):
    """EAP-PAX protocol tests"""
    def pax_std_1(ctx):
            logger.info("Test: STD-1")
            ctx['id'] = 10
            return struct.pack(">BBHBBBBBBH8L16B", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 5 + 2 + 32 + 16,
                               EAP_TYPE_PAX,
                               EAP_PAX_OP_STD_1, 0, EAP_PAX_MAC_HMAC_SHA1_128,
                               EAP_PAX_DH_GROUP_NONE, EAP_PAX_PUBLIC_KEY_NONE,
                               32, 0, 0, 0, 0, 0, 0, 0, 0,
                               0x16, 0xc9, 0x08, 0x9d, 0x98, 0xa5, 0x6e, 0x1f,
                               0xf0, 0xac, 0xcf, 0xc4, 0x66, 0xcd, 0x2d, 0xbf)

    def pax_handler(ctx, req):
        logger.info("pax_handler - RX " + binascii.hexlify(req).decode())
        if 'num' not in ctx:
            ctx['num'] = 0
        ctx['num'] = ctx['num'] + 1
        if 'id' not in ctx:
            ctx['id'] = 1
        ctx['id'] = (ctx['id'] + 1) % 256

        idx = 0

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Missing payload")
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1,
                               EAP_TYPE_PAX)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Minimum length payload")
            return struct.pack(">BBHB4L", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 16,
                               EAP_TYPE_PAX,
                               0, 0, 0, 0)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unsupported MAC ID")
            return struct.pack(">BBHBBBBBB4L", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 5 + 16,
                               EAP_TYPE_PAX,
                               EAP_PAX_OP_STD_1, 0, 255, EAP_PAX_DH_GROUP_NONE,
                               EAP_PAX_PUBLIC_KEY_NONE,
                               0, 0, 0, 0)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unsupported DH Group ID")
            return struct.pack(">BBHBBBBBB4L", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 5 + 16,
                               EAP_TYPE_PAX,
                               EAP_PAX_OP_STD_1, 0, EAP_PAX_MAC_HMAC_SHA1_128,
                               255, EAP_PAX_PUBLIC_KEY_NONE,
                               0, 0, 0, 0)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unsupported Public Key ID")
            return struct.pack(">BBHBBBBBB4L", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 5 + 16,
                               EAP_TYPE_PAX,
                               EAP_PAX_OP_STD_1, 0, EAP_PAX_MAC_HMAC_SHA1_128,
                               EAP_PAX_DH_GROUP_NONE, 255,
                               0, 0, 0, 0)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: More fragments")
            return struct.pack(">BBHBBBBBB4L", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 5 + 16,
                               EAP_TYPE_PAX,
                               EAP_PAX_OP_STD_1, EAP_PAX_FLAGS_MF,
                               EAP_PAX_MAC_HMAC_SHA1_128,
                               EAP_PAX_DH_GROUP_NONE, EAP_PAX_PUBLIC_KEY_NONE,
                               0, 0, 0, 0)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Invalid ICV")
            return struct.pack(">BBHBBBBBB4L", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 5 + 16,
                               EAP_TYPE_PAX,
                               EAP_PAX_OP_STD_1, 0, EAP_PAX_MAC_HMAC_SHA1_128,
                               EAP_PAX_DH_GROUP_NONE, EAP_PAX_PUBLIC_KEY_NONE,
                               0, 0, 0, 0)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Invalid ICV in short frame")
            return struct.pack(">BBHBBBBBB3L", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 5 + 12,
                               EAP_TYPE_PAX,
                               EAP_PAX_OP_STD_1, 0, EAP_PAX_MAC_HMAC_SHA1_128,
                               EAP_PAX_DH_GROUP_NONE, EAP_PAX_PUBLIC_KEY_NONE,
                               0, 0, 0)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Correct ICV - unsupported op_code")
            ctx['id'] = 10
            return struct.pack(">BBHBBBBBB16B", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 5 + 16,
                               EAP_TYPE_PAX,
                               255, 0, EAP_PAX_MAC_HMAC_SHA1_128,
                               EAP_PAX_DH_GROUP_NONE, EAP_PAX_PUBLIC_KEY_NONE,
                               0x90, 0x78, 0x97, 0x38, 0x29, 0x94, 0x32, 0xd4,
                               0x81, 0x27, 0xe0, 0xf6, 0x3b, 0x0d, 0xb2, 0xb2)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Correct ICV - CE flag in STD-1")
            ctx['id'] = 10
            return struct.pack(">BBHBBBBBB16B", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 5 + 16,
                               EAP_TYPE_PAX,
                               EAP_PAX_OP_STD_1, EAP_PAX_FLAGS_CE,
                               EAP_PAX_MAC_HMAC_SHA1_128,
                               EAP_PAX_DH_GROUP_NONE, EAP_PAX_PUBLIC_KEY_NONE,
                               0x9c, 0x98, 0xb4, 0x0b, 0x94, 0x90, 0xde, 0x88,
                               0xb7, 0x72, 0x63, 0x44, 0x1d, 0xe3, 0x7c, 0x5c)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Correct ICV - too short STD-1 payload")
            ctx['id'] = 10
            return struct.pack(">BBHBBBBBB16B", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 5 + 16,
                               EAP_TYPE_PAX,
                               EAP_PAX_OP_STD_1, 0, EAP_PAX_MAC_HMAC_SHA1_128,
                               EAP_PAX_DH_GROUP_NONE, EAP_PAX_PUBLIC_KEY_NONE,
                               0xda, 0xab, 0x2c, 0xe7, 0x84, 0x41, 0xb5, 0x5c,
                               0xee, 0xcf, 0x62, 0x03, 0xc5, 0x69, 0xcb, 0xf4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Correct ICV - incorrect A length in STD-1")
            ctx['id'] = 10
            return struct.pack(">BBHBBBBBBH8L16B", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 5 + 2 + 32 + 16,
                               EAP_TYPE_PAX,
                               EAP_PAX_OP_STD_1, 0, EAP_PAX_MAC_HMAC_SHA1_128,
                               EAP_PAX_DH_GROUP_NONE, EAP_PAX_PUBLIC_KEY_NONE,
                               0, 0, 0, 0, 0, 0, 0, 0, 0,
                               0xc4, 0xb0, 0x81, 0xe4, 0x6c, 0x8c, 0x20, 0x23,
                               0x60, 0x46, 0x89, 0xea, 0x94, 0x60, 0xf3, 0x2a)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Correct ICV - extra data in STD-1")
            ctx['id'] = 10
            return struct.pack(">BBHBBBBBBH8LB16B", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 5 + 2 + 32 + 1 + 16,
                               EAP_TYPE_PAX,
                               EAP_PAX_OP_STD_1, 0, EAP_PAX_MAC_HMAC_SHA1_128,
                               EAP_PAX_DH_GROUP_NONE, EAP_PAX_PUBLIC_KEY_NONE,
                               32, 0, 0, 0, 0, 0, 0, 0, 0,
                               1,
                               0x61, 0x49, 0x65, 0x37, 0x21, 0xe8, 0xd8, 0xbf,
                               0xf3, 0x02, 0x01, 0xe5, 0x42, 0x51, 0xd3, 0x34)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unexpected STD-1")
            return struct.pack(">BBHBBBBBBH8L16B", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 5 + 2 + 32 + 16,
                               EAP_TYPE_PAX,
                               EAP_PAX_OP_STD_1, 0, EAP_PAX_MAC_HMAC_SHA1_128,
                               EAP_PAX_DH_GROUP_NONE, EAP_PAX_PUBLIC_KEY_NONE,
                               32, 0, 0, 0, 0, 0, 0, 0, 0,
                               0xe5, 0x1d, 0xbf, 0xb8, 0x70, 0x20, 0x5c, 0xba,
                               0x41, 0xbb, 0x34, 0xda, 0x1a, 0x08, 0xe6, 0x8d)

        idx += 1
        if ctx['num'] == idx:
            return pax_std_1(ctx)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: MAC ID changed during session")
            return struct.pack(">BBHBBBBBBH8L16B", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 5 + 2 + 32 + 16,
                               EAP_TYPE_PAX,
                               EAP_PAX_OP_STD_1, 0, EAP_PAX_HMAC_SHA256_128,
                               EAP_PAX_DH_GROUP_NONE, EAP_PAX_PUBLIC_KEY_NONE,
                               32, 0, 0, 0, 0, 0, 0, 0, 0,
                               0xee, 0x00, 0xbf, 0xb8, 0x70, 0x20, 0x5c, 0xba,
                               0x41, 0xbb, 0x34, 0xda, 0x1a, 0x08, 0xe6, 0x8d)

        idx += 1
        if ctx['num'] == idx:
            return pax_std_1(ctx)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: DH Group ID changed during session")
            return struct.pack(">BBHBBBBBBH8L16B", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 5 + 2 + 32 + 16,
                               EAP_TYPE_PAX,
                               EAP_PAX_OP_STD_1, 0, EAP_PAX_MAC_HMAC_SHA1_128,
                               EAP_PAX_DH_GROUP_2048_MODP,
                               EAP_PAX_PUBLIC_KEY_NONE,
                               32, 0, 0, 0, 0, 0, 0, 0, 0,
                               0xee, 0x01, 0xbf, 0xb8, 0x70, 0x20, 0x5c, 0xba,
                               0x41, 0xbb, 0x34, 0xda, 0x1a, 0x08, 0xe6, 0x8d)

        idx += 1
        if ctx['num'] == idx:
            return pax_std_1(ctx)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Public Key ID changed during session")
            return struct.pack(">BBHBBBBBBH8L16B", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 5 + 2 + 32 + 16,
                               EAP_TYPE_PAX,
                               EAP_PAX_OP_STD_1, 0, EAP_PAX_MAC_HMAC_SHA1_128,
                               EAP_PAX_DH_GROUP_NONE,
                               EAP_PAX_PUBLIC_KEY_RSAES_OAEP,
                               32, 0, 0, 0, 0, 0, 0, 0, 0,
                               0xee, 0x02, 0xbf, 0xb8, 0x70, 0x20, 0x5c, 0xba,
                               0x41, 0xbb, 0x34, 0xda, 0x1a, 0x08, 0xe6, 0x8d)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unexpected STD-3")
            ctx['id'] = 10
            return struct.pack(">BBHBBBBBBH8L16B", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 5 + 2 + 32 + 16,
                               EAP_TYPE_PAX,
                               EAP_PAX_OP_STD_3, 0, EAP_PAX_MAC_HMAC_SHA1_128,
                               EAP_PAX_DH_GROUP_NONE, EAP_PAX_PUBLIC_KEY_NONE,
                               32, 0, 0, 0, 0, 0, 0, 0, 0,
                               0x47, 0xbb, 0xc0, 0xf9, 0xb9, 0x69, 0xf5, 0xcb,
                               0x3a, 0xe8, 0xe7, 0xd6, 0x80, 0x28, 0xf2, 0x59)

        idx += 1
        if ctx['num'] == idx:
            return pax_std_1(ctx)
        idx += 1
        if ctx['num'] == idx:
            # TODO: MAC calculation; for now, this gets dropped due to incorrect
            # ICV
            logger.info("Test: STD-3 with CE flag")
            return struct.pack(">BBHBBBBBBH8L16B", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 5 + 2 + 32 + 16,
                               EAP_TYPE_PAX,
                               EAP_PAX_OP_STD_3, EAP_PAX_FLAGS_CE,
                               EAP_PAX_MAC_HMAC_SHA1_128,
                               EAP_PAX_DH_GROUP_NONE, EAP_PAX_PUBLIC_KEY_NONE,
                               32, 0, 0, 0, 0, 0, 0, 0, 0,
                               0x8a, 0xc2, 0xf9, 0xf4, 0x8b, 0x75, 0x72, 0xa2,
                               0x4d, 0xd3, 0x1e, 0x54, 0x77, 0x04, 0x05, 0xe2)

        idx += 1
        if ctx['num'] & 0x1 == idx & 0x1:
            logger.info("Test: Default request")
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1,
                               EAP_TYPE_PAX)
        else:
            logger.info("Test: Default EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

    srv = start_radius_server(pax_handler)

    try:
        hapd = start_ap(apdev[0])
        dev[0].scan_for_bss(hapd.own_addr(), freq=2412)

        for i in range(0, 18):
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="PAX", identity="user",
                           password_hex="0123456789abcdef0123456789abcdef",
                           wait_connect=False)
            logger.info("Waiting for EAP method to start")
            ev = dev[0].wait_event(["CTRL-EVENT-EAP-PROPOSED-METHOD"],
                                   timeout=15)
            if ev is None:
                raise Exception("Timeout on EAP start")
            time.sleep(0.05)
            dev[0].request("REMOVE_NETWORK all")
            dev[0].dump_monitor()

        logger.info("Too short password")
        dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                       eap="PAX", identity="user",
                       password_hex="0123456789abcdef0123456789abcd",
                       wait_connect=False)
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-PROPOSED-METHOD"], timeout=15)
        if ev is None:
            raise Exception("Timeout on EAP start")
        time.sleep(0.1)
        dev[0].request("REMOVE_NETWORK all")
        dev[0].dump_monitor()

        logger.info("No password")
        dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                       eap="PAX", identity="user",
                       wait_connect=False)
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-PROPOSED-METHOD"], timeout=15)
        if ev is None:
            raise Exception("Timeout on EAP start")
        time.sleep(0.1)
        dev[0].request("REMOVE_NETWORK all")
        dev[0].dump_monitor()
    finally:
        stop_radius_server(srv)

def test_eap_proto_pax_errors(dev, apdev):
    """EAP-PAX local error cases"""
    check_eap_capa(dev[0], "PAX")
    params = hostapd.wpa2_eap_params(ssid="eap-test")
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].scan_for_bss(hapd.own_addr(), freq=2412)

    for i in range(1, 3):
        with alloc_fail(dev[0], i, "eap_pax_init"):
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="PAX", identity="pax.user@example.com",
                           password_hex="0123456789abcdef0123456789abcdef",
                           wait_connect=False)
            ev = dev[0].wait_event(["EAP: Failed to initialize EAP method"],
                                   timeout=15)
            if ev is None:
                raise Exception("Timeout on EAP start")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

    tests = ["eap_msg_alloc;eap_pax_alloc_resp;eap_pax_process_std_1",
             "eap_msg_alloc;eap_pax_alloc_resp;eap_pax_process_std_3",
             "eap_pax_getKey",
             "eap_pax_get_emsk",
             "eap_pax_get_session_id"]
    for func in tests:
        with alloc_fail(dev[0], 1, func):
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="PAX", identity="pax.user@example.com",
                           password_hex="0123456789abcdef0123456789abcdef",
                           erp="1", wait_connect=False)
            wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

    tests = [(1, "os_get_random;eap_pax_process_std_1"),
             (1, "eap_pax_initial_key_derivation"),
             (1, "eap_pax_mac;eap_pax_process_std_3"),
             (2, "eap_pax_mac;eap_pax_process_std_3"),
             (1, "eap_pax_kdf;eap_pax_getKey"),
             (1, "eap_pax_kdf;eap_pax_get_emsk")]
    for count, func in tests:
        with fail_test(dev[0], count, func):
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="PAX", identity="pax.user@example.com",
                           password_hex="0123456789abcdef0123456789abcdef",
                           erp="1", wait_connect=False)
            wait_fail_trigger(dev[0], "GET_FAIL")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

def run_eap_pax_connect(dev):
    dev.connect("test-wpa2-eap", key_mgmt="WPA-EAP", scan_freq="2412",
                eap="PAX", identity="pax.user@example.com",
                password_hex="0123456789abcdef0123456789abcdef",
                wait_connect=False)
    ev = dev.wait_event(["CTRL-EVENT-EAP-SUCCESS", "CTRL-EVENT-EAP-FAILURE",
                         "CTRL-EVENT-DISCONNECTED"],
                        timeout=1)
    dev.request("REMOVE_NETWORK all")
    if not ev or "CTRL-EVENT-DISCONNECTED" not in ev:
        dev.wait_disconnected()
    dev.dump_monitor()

def test_eap_proto_pax_errors_server(dev, apdev):
    """EAP-PAX local error cases on server"""
    check_eap_capa(dev[0], "PAX")
    params = int_eap_server_params()
    params['erp_domain'] = 'example.com'
    params['eap_server_erp'] = '1'
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].scan_for_bss(hapd.own_addr(), freq=2412)

    tests = [(1, "eap_pax_init"),
             (1, "eap_msg_alloc;eap_pax_build_std_1"),
             (1, "eap_msg_alloc;eap_pax_build_std_3"),
             (1, "=eap_pax_process_std_2"),
             (1, "eap_pax_getKey"),
             (1, "eap_pax_get_emsk"),
             (1, "eap_pax_get_session_id")]
    for count, func in tests:
        with alloc_fail(hapd, count, func):
            run_eap_pax_connect(dev[0])

    tests = [(1, "os_get_random;eap_pax_build_std_1"),
             (1, "eap_pax_mac;eap_pax_build_std_1"),
             (1, "eap_pax_mac;eap_pax_build_std_3"),
             (2, "eap_pax_mac;=eap_pax_build_std_3"),
             (1, "eap_pax_initial_key_derivation;eap_pax_process_std_2"),
             (1, "eap_pax_mac;eap_pax_process_std_2"),
             (2, "eap_pax_mac;=eap_pax_process_std_2"),
             (1, "eap_pax_mac;eap_pax_check")]
    for count, func in tests:
        with fail_test(hapd, count, func):
            run_eap_pax_connect(dev[0])

def start_pax_assoc(dev, hapd):
    dev.connect("test-wpa2-eap", key_mgmt="WPA-EAP", scan_freq="2412",
                eap="PAX", identity="pax.user@example.com",
                password_hex="0123456789abcdef0123456789abcdef",
                wait_connect=False)
    proxy_msg(hapd, dev) # EAP-Identity/Request
    proxy_msg(dev, hapd) # EAP-Identity/Response
    proxy_msg(hapd, dev) # PAX_STD-1

def stop_pax_assoc(dev, hapd):
    dev.request("REMOVE_NETWORK all")
    dev.wait_disconnected()
    dev.dump_monitor()
    hapd.dump_monitor()

def test_eap_proto_pax_server(dev, apdev):
    """EAP-PAX protocol testing for the server"""
    check_eap_capa(dev[0], "PAX")
    params = int_eap_server_params()
    params['erp_domain'] = 'example.com'
    params['eap_server_erp'] = '1'
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].scan_for_bss(hapd.own_addr(), freq=2412)
    hapd.request("SET ext_eapol_frame_io 1")
    dev[0].request("SET ext_eapol_frame_io 1")

    # Successful exchange to verify proxying mechanism
    start_pax_assoc(dev[0], hapd)
    proxy_msg(dev[0], hapd) # PAX_STD-2
    proxy_msg(hapd, dev[0]) # PAX_STD-3
    proxy_msg(dev[0], hapd) # PAX-ACK
    proxy_msg(hapd, dev[0]) # EAP-Success
    proxy_msg(hapd, dev[0]) # EAPOL-Key msg 1/4
    proxy_msg(dev[0], hapd) # EAPOL-Key msg 2/4
    proxy_msg(hapd, dev[0]) # EAPOL-Key msg 3/4
    proxy_msg(dev[0], hapd) # EAPOL-Key msg 4/4
    dev[0].wait_connected()
    stop_pax_assoc(dev[0], hapd)

    start_pax_assoc(dev[0], hapd)
    resp = rx_msg(dev[0])
    # Too short EAP-PAX header (no OP-Code)
    hapd.note("EAP-PAX: Invalid frame")
    msg = resp[0:4] + "0005" + resp[8:12] + "0005" + "2e"
    tx_msg(dev[0], hapd, msg)
    # Too short EAP-PAX message (no payload)
    hapd.note("EAP-PAX: Invalid frame")
    msg = resp[0:4] + "000a" + resp[8:12] + "000a" + "2e1100000000"
    tx_msg(dev[0], hapd, msg)
    # Unexpected PAX_SEC-2
    hapd.note("EAP-PAX: Expected PAX_STD-2 - ignore op 17")
    msg = resp[0:4] + "001a" + resp[8:12] + "001a" + "2e1100000000" + 16*"00"
    tx_msg(dev[0], hapd, msg)
    # Unexpected MAC ID
    hapd.note("EAP-PAX: Expected MAC ID 0x1, received 0xff")
    msg = resp[0:4] + "001a" + resp[8:12] + "001a" + "2e0200ff0000" + 16*"00"
    tx_msg(dev[0], hapd, msg)
    # Unexpected DH Group ID
    hapd.note("EAP-PAX: Expected DH Group ID 0x0, received 0xff")
    msg = resp[0:4] + "001a" + resp[8:12] + "001a" + "2e020001ff00" + 16*"00"
    tx_msg(dev[0], hapd, msg)
    # Unexpected Public Key ID
    hapd.note("EAP-PAX: Expected Public Key ID 0x0, received 0xff")
    msg = resp[0:4] + "001a" + resp[8:12] + "001a" + "2e02000100ff" + 16*"00"
    tx_msg(dev[0], hapd, msg)
    # Unsupported Flags - MF
    hapd.note("EAP-PAX: fragmentation not supported")
    msg = resp[0:4] + "001a" + resp[8:12] + "001a" + "2e0201010000" + 16*"00"
    tx_msg(dev[0], hapd, msg)
    # Unsupported Flags - CE
    hapd.note("EAP-PAX: Unexpected CE flag")
    msg = resp[0:4] + "001a" + resp[8:12] + "001a" + "2e0202010000" + 16*"00"
    tx_msg(dev[0], hapd, msg)
    # Too short Payload in PAX_STD-2
    hapd.note("EAP-PAX: Too short PAX_STD-2 (B)")
    msg = resp[0:4] + "001a" + resp[8:12] + "001a" + "2e0200010000" + 16*"00"
    tx_msg(dev[0], hapd, msg)
    rx_msg(hapd)
    stop_pax_assoc(dev[0], hapd)

    start_pax_assoc(dev[0], hapd)
    resp = rx_msg(dev[0])
    # Too short Payload in PAX_STD-2
    hapd.note("EAP-PAX: Too short PAX_STD-2 (CID)")
    msg = resp[0:4] + "002c" + resp[8:12] + "002c" + "2e0200010000" + "0020" + 32*"00"
    tx_msg(dev[0], hapd, msg)
    rx_msg(hapd)
    stop_pax_assoc(dev[0], hapd)

    start_pax_assoc(dev[0], hapd)
    resp = rx_msg(dev[0])
    # Too short Payload in PAX_STD-2
    hapd.note("EAP-PAX: Too short PAX_STD-2 (CID)")
    msg = resp[0:4] + "002e" + resp[8:12] + "002e" + "2e0200010000" + "0020" + 32*"00" + "ffff"
    tx_msg(dev[0], hapd, msg)
    rx_msg(hapd)
    stop_pax_assoc(dev[0], hapd)

    start_pax_assoc(dev[0], hapd)
    resp = rx_msg(dev[0])
    # Too long CID in PAX_STD-2
    hapd.note("EAP-PAX: Too long CID")
    msg = resp[0:4] + "062e" + resp[8:12] + "062e" + "2e0200010000" + "0020" + 32*"00" + "0600" + 1536*"00"
    tx_msg(dev[0], hapd, msg)
    rx_msg(hapd)
    stop_pax_assoc(dev[0], hapd)

    start_pax_assoc(dev[0], hapd)
    resp = rx_msg(dev[0])
    # Too short Payload in PAX_STD-2
    hapd.note("EAP-PAX: Too short PAX_STD-2 (MAC_CK)")
    msg = resp[0:4] + "003c" + resp[8:12] + "003c" + "2e0200010000" + "0020" + 32*"00" + 16*"00"
    tx_msg(dev[0], hapd, msg)
    rx_msg(hapd)
    stop_pax_assoc(dev[0], hapd)

    start_pax_assoc(dev[0], hapd)
    resp = rx_msg(dev[0])
    # Unknown CID for PAX
    hapd.note("EAP-PAX: EAP-PAX not enabled for CID")
    msg = resp[0:4] + "0041" + resp[8:12] + "0041" + "2e0200010000" + "0020" + 32*"00" + "0001" + "00" + "0010" + 16*"00"
    tx_msg(dev[0], hapd, msg)
    rx_msg(hapd)
    stop_pax_assoc(dev[0], hapd)

    start_pax_assoc(dev[0], hapd)
    resp = rx_msg(dev[0])
    # Too short ICV
    hapd.note("EAP-PAX: Too short ICV (15) in PAX_STD-2")
    msg = resp[0:4] + "0063" + resp[8:12] + "0063" + resp[16:206]
    tx_msg(dev[0], hapd, msg)
    rx_msg(hapd)
    stop_pax_assoc(dev[0], hapd)

    start_pax_assoc(dev[0], hapd)
    proxy_msg(dev[0], hapd) # PAX_STD-2
    proxy_msg(hapd, dev[0]) # PAX_STD-3
    resp = rx_msg(dev[0])
    # Unexpected PAX_STD-2
    hapd.note("EAP-PAX: Expected PAX-ACK - ignore op 1")
    msg = resp[0:4] + "001a" + resp[8:12] + "001a" + "2e0100000000" + 16*"00"
    tx_msg(dev[0], hapd, msg)
    stop_pax_assoc(dev[0], hapd)

def test_eap_proto_psk(dev, apdev):
    """EAP-PSK protocol tests"""
    def psk_handler(ctx, req):
        logger.info("psk_handler - RX " + binascii.hexlify(req).decode())
        if 'num' not in ctx:
            ctx['num'] = 0
        ctx['num'] = ctx['num'] + 1
        if 'id' not in ctx:
            ctx['id'] = 1
        ctx['id'] = (ctx['id'] + 1) % 256

        idx = 0

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Missing payload")
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1,
                               EAP_TYPE_PSK)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Non-zero T in first message")
            return struct.pack(">BBHBB4L", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 16,
                               EAP_TYPE_PSK, 0xc0, 0, 0, 0, 0)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Valid first message")
            return struct.pack(">BBHBB4L", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 16,
                               EAP_TYPE_PSK, 0, 0, 0, 0, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Too short third message")
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1,
                               EAP_TYPE_PSK)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Valid first message")
            return struct.pack(">BBHBB4L", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 16,
                               EAP_TYPE_PSK, 0, 0, 0, 0, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Incorrect T in third message")
            return struct.pack(">BBHBB4L4L", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 16 + 16,
                               EAP_TYPE_PSK, 0, 0, 0, 0, 0, 0, 0, 0, 0)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Valid first message")
            return struct.pack(">BBHBB4L", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 16,
                               EAP_TYPE_PSK, 0, 0, 0, 0, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Missing PCHANNEL in third message")
            return struct.pack(">BBHBB4L4L", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 16 + 16,
                               EAP_TYPE_PSK, 0x80, 0, 0, 0, 0, 0, 0, 0, 0)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Valid first message")
            return struct.pack(">BBHBB4L", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 16,
                               EAP_TYPE_PSK, 0, 0, 0, 0, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Invalic MAC_S in third message")
            return struct.pack(">BBHBB4L4L5LB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 16 + 16 + 21,
                               EAP_TYPE_PSK, 0x80, 0, 0, 0, 0, 0, 0, 0, 0,
                               0, 0, 0, 0, 0, 0)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Valid first message")
            return struct.pack(">BBHBB4L", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 16,
                               EAP_TYPE_PSK, 0, 0, 0, 0, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        return None

    srv = start_radius_server(psk_handler)

    try:
        hapd = start_ap(apdev[0])
        dev[0].scan_for_bss(hapd.own_addr(), freq=2412)

        for i in range(0, 6):
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="PSK", identity="user",
                           password_hex="0123456789abcdef0123456789abcdef",
                           wait_connect=False)
            ev = dev[0].wait_event(["CTRL-EVENT-EAP-PROPOSED-METHOD"],
                                   timeout=15)
            if ev is None:
                raise Exception("Timeout on EAP start")
            time.sleep(0.1)
            dev[0].request("REMOVE_NETWORK all")

        logger.info("Test: Invalid PSK length")
        dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                       eap="PSK", identity="user",
                       password_hex="0123456789abcdef0123456789abcd",
                       wait_connect=False)
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-PROPOSED-METHOD"],
                               timeout=15)
        if ev is None:
            raise Exception("Timeout on EAP start")
        time.sleep(0.1)
        dev[0].request("REMOVE_NETWORK all")
    finally:
        stop_radius_server(srv)

def test_eap_proto_psk_errors(dev, apdev):
    """EAP-PSK local error cases"""
    check_eap_capa(dev[0], "PSK")
    params = hostapd.wpa2_eap_params(ssid="eap-test")
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].scan_for_bss(hapd.own_addr(), freq=2412)

    for i in range(1, 3):
        with alloc_fail(dev[0], i, "eap_psk_init"):
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="PSK", identity="psk.user@example.com",
                           password_hex="0123456789abcdef0123456789abcdef",
                           wait_connect=False)
            ev = dev[0].wait_event(["EAP: Failed to initialize EAP method"],
                                   timeout=15)
            if ev is None:
                raise Exception("Timeout on EAP start")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

    for i in range(1, 4):
        with fail_test(dev[0], i, "eap_psk_key_setup;eap_psk_init"):
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="PSK", identity="psk.user@example.com",
                           password_hex="0123456789abcdef0123456789abcdef",
                           wait_connect=False)
            ev = dev[0].wait_event(["EAP: Failed to initialize EAP method"],
                                   timeout=15)
            if ev is None:
                raise Exception("Timeout on EAP start")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

    tests = [(1, "=eap_psk_process_1"),
             (2, "=eap_psk_process_1"),
             (1, "eap_msg_alloc;eap_psk_process_1"),
             (1, "=eap_psk_process_3"),
             (2, "=eap_psk_process_3"),
             (1, "eap_msg_alloc;eap_psk_process_3"),
             (1, "eap_psk_getKey"),
             (1, "eap_psk_get_session_id"),
             (1, "eap_psk_get_emsk")]
    for count, func in tests:
        with alloc_fail(dev[0], count, func):
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="PSK", identity="psk.user@example.com",
                           password_hex="0123456789abcdef0123456789abcdef",
                           erp="1", wait_connect=False)
            ev = dev[0].wait_event(["CTRL-EVENT-EAP-PROPOSED-METHOD"],
                                   timeout=15)
            if ev is None:
                raise Exception("Timeout on EAP start")
            wait_fail_trigger(dev[0], "GET_ALLOC_FAIL",
                              note="No allocation failure seen for %d:%s" % (count, func))
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

    tests = [(1, "os_get_random;eap_psk_process_1"),
             (1, "omac1_aes_128;eap_psk_process_3"),
             (1, "=omac1_aes_vector;omac1_aes_128;aes_128_eax_encrypt"),
             (2, "=omac1_aes_vector;omac1_aes_128;aes_128_eax_encrypt"),
             (3, "=omac1_aes_vector;omac1_aes_128;aes_128_eax_encrypt"),
             (1, "=omac1_aes_vector;omac1_aes_128;aes_128_eax_decrypt"),
             (2, "=omac1_aes_vector;omac1_aes_128;aes_128_eax_decrypt"),
             (3, "=omac1_aes_vector;omac1_aes_128;aes_128_eax_decrypt"),
             (1, "aes_128_eax_decrypt;eap_psk_process_3"),
             (2, "aes_128_eax_decrypt;eap_psk_process_3"),
             (3, "aes_128_eax_decrypt;eap_psk_process_3"),
             (1, "aes_128_eax_encrypt;eap_psk_process_3"),
             (2, "aes_128_eax_encrypt;eap_psk_process_3"),
             (3, "aes_128_eax_encrypt;eap_psk_process_3"),
             (1, "aes_128_encrypt_block;eap_psk_derive_keys;eap_psk_process_3"),
             (2, "aes_128_encrypt_block;eap_psk_derive_keys;eap_psk_process_3"),
             (3, "aes_128_encrypt_block;eap_psk_derive_keys;eap_psk_process_3"),
             (4, "aes_128_encrypt_block;eap_psk_derive_keys;eap_psk_process_3"),
             (5, "aes_128_encrypt_block;eap_psk_derive_keys;eap_psk_process_3"),
             (6, "aes_128_encrypt_block;eap_psk_derive_keys;eap_psk_process_3"),
             (7, "aes_128_encrypt_block;eap_psk_derive_keys;eap_psk_process_3"),
             (8, "aes_128_encrypt_block;eap_psk_derive_keys;eap_psk_process_3"),
             (9, "aes_128_encrypt_block;eap_psk_derive_keys;eap_psk_process_3"),
             (10, "aes_128_encrypt_block;eap_psk_derive_keys;eap_psk_process_3"),
             (1, "aes_ctr_encrypt;aes_128_eax_decrypt;eap_psk_process_3"),
             (1, "aes_ctr_encrypt;aes_128_eax_encrypt;eap_psk_process_3")]
    for count, func in tests:
        with fail_test(dev[0], count, func):
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="PSK", identity="psk.user@example.com",
                           password_hex="0123456789abcdef0123456789abcdef",
                           wait_connect=False)
            ev = dev[0].wait_event(["CTRL-EVENT-EAP-PROPOSED-METHOD"],
                                   timeout=15)
            if ev is None:
                raise Exception("Timeout on EAP start")
            wait_fail_trigger(dev[0], "GET_FAIL",
                              note="No failure seen for %d:%s" % (count, func))
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()
            dev[0].dump_monitor()

def run_eap_psk_connect(dev):
    dev.connect("test-wpa2-eap", key_mgmt="WPA-EAP", scan_freq="2412",
                eap="PSK", identity="psk.user@example.com",
                password_hex="0123456789abcdef0123456789abcdef",
                wait_connect=False)
    ev = dev.wait_event(["CTRL-EVENT-EAP-SUCCESS", "CTRL-EVENT-EAP-FAILURE",
                         "CTRL-EVENT-DISCONNECTED"],
                        timeout=1)
    dev.request("REMOVE_NETWORK all")
    if not ev or "CTRL-EVENT-DISCONNECTED" not in ev:
        dev.wait_disconnected()
    dev.dump_monitor()

def test_eap_proto_psk_errors_server(dev, apdev):
    """EAP-PSK local error cases on server"""
    check_eap_capa(dev[0], "PSK")
    params = int_eap_server_params()
    params['erp_domain'] = 'example.com'
    params['eap_server_erp'] = '1'
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].scan_for_bss(hapd.own_addr(), freq=2412)

    tests = [(1, "eap_psk_init"),
             (1, "eap_msg_alloc;eap_psk_build_1"),
             (1, "eap_msg_alloc;eap_psk_build_3"),
             (1, "=eap_psk_build_3"),
             (1, "=eap_psk_process_2"),
             (2, "=eap_psk_process_2"),
             (1, "=eap_psk_process_4"),
             (1, "aes_128_eax_decrypt;eap_psk_process_4"),
             (1, "eap_psk_getKey"),
             (1, "eap_psk_get_emsk"),
             (1, "eap_psk_get_session_id")]
    for count, func in tests:
        with alloc_fail(hapd, count, func):
            run_eap_psk_connect(dev[0])

    tests = [(1, "os_get_random;eap_psk_build_1"),
             (1, "omac1_aes_128;eap_psk_build_3"),
             (1, "eap_psk_derive_keys;eap_psk_build_3"),
             (1, "aes_128_eax_encrypt;eap_psk_build_3"),
             (1, "eap_psk_key_setup;eap_psk_process_2"),
             (1, "omac1_aes_128;eap_psk_process_2"),
             (1, "aes_128_eax_decrypt;eap_psk_process_4")]
    for count, func in tests:
        with fail_test(hapd, count, func):
            run_eap_psk_connect(dev[0])

def start_psk_assoc(dev, hapd):
    dev.connect("test-wpa2-eap", key_mgmt="WPA-EAP", scan_freq="2412",
                eap="PSK", identity="psk.user@example.com",
                password_hex="0123456789abcdef0123456789abcdef",
                wait_connect=False)
    proxy_msg(hapd, dev) # EAP-Identity/Request
    proxy_msg(dev, hapd) # EAP-Identity/Response
    proxy_msg(hapd, dev) # PSK-1

def stop_psk_assoc(dev, hapd):
    dev.request("REMOVE_NETWORK all")
    dev.wait_disconnected()
    dev.dump_monitor()
    hapd.dump_monitor()

def test_eap_proto_psk_server(dev, apdev):
    """EAP-PSK protocol testing for the server"""
    check_eap_capa(dev[0], "PSK")
    params = int_eap_server_params()
    params['erp_domain'] = 'example.com'
    params['eap_server_erp'] = '1'
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].scan_for_bss(hapd.own_addr(), freq=2412)
    hapd.request("SET ext_eapol_frame_io 1")
    dev[0].request("SET ext_eapol_frame_io 1")

    # Successful exchange to verify proxying mechanism
    start_psk_assoc(dev[0], hapd)
    proxy_msg(dev[0], hapd) # PSK-2
    proxy_msg(hapd, dev[0]) # PSK-3
    proxy_msg(dev[0], hapd) # PSK-4
    proxy_msg(hapd, dev[0]) # EAP-Success
    proxy_msg(hapd, dev[0]) # EAPOL-Key msg 1/4
    proxy_msg(dev[0], hapd) # EAPOL-Key msg 2/4
    proxy_msg(hapd, dev[0]) # EAPOL-Key msg 3/4
    proxy_msg(dev[0], hapd) # EAPOL-Key msg 4/4
    dev[0].wait_connected()
    stop_psk_assoc(dev[0], hapd)

    start_psk_assoc(dev[0], hapd)
    resp = rx_msg(dev[0])
    # Too short EAP-PSK header (no Flags)
    hapd.note("EAP-PSK: Invalid frame")
    msg = resp[0:4] + "0005" + resp[8:12] + "0005" + "2f"
    tx_msg(dev[0], hapd, msg)
    # Unexpected PSK-1
    hapd.note("EAP-PSK: Expected PSK-2 - ignore T=0")
    msg = resp[0:4] + "0006" + resp[8:12] + "0006" + "2f00"
    tx_msg(dev[0], hapd, msg)
    # Too short PSK-2
    hapd.note("EAP-PSK: Too short frame")
    msg = resp[0:4] + "0006" + resp[8:12] + "0006" + "2f40"
    tx_msg(dev[0], hapd, msg)
    # PSK-2 with unknown ID_P
    hapd.note("EAP-PSK: EAP-PSK not enabled for ID_P")
    msg = resp[0:4] + "004a" + resp[8:12] + "004a" + "2f40" + 3*16*"00" + 20*"00"
    tx_msg(dev[0], hapd, msg)
    rx_msg(hapd) # EAP-Failure
    stop_psk_assoc(dev[0], hapd)

    start_psk_assoc(dev[0], hapd)
    proxy_msg(dev[0], hapd) # PSK-2
    proxy_msg(hapd, dev[0]) # PSK-3
    resp = rx_msg(dev[0])
    # Unexpected PSK-2
    hapd.note("EAP-PSK: Expected PSK-4 - ignore T=1")
    msg = resp[0:4] + "0016" + resp[8:12] + "0016" + "2f40" + 16*"00"
    tx_msg(dev[0], hapd, msg)
    # Too short PSK-4 (no PCHANNEL)
    hapd.note("EAP-PSK: Too short PCHANNEL data in PSK-4 (len=0, expected 21)")
    msg = resp[0:4] + "0016" + resp[8:12] + "0016" + "2fc0" + 16*"00"
    tx_msg(dev[0], hapd, msg)
    rx_msg(hapd) # PSK-3 retry
    stop_psk_assoc(dev[0], hapd)

    start_psk_assoc(dev[0], hapd)
    proxy_msg(dev[0], hapd) # PSK-2
    proxy_msg(hapd, dev[0]) # PSK-3
    resp = rx_msg(dev[0])
    # PCHANNEL Nonce did not increase
    hapd.note("EAP-PSK: Nonce did not increase")
    msg = resp[0:4] + "002b" + resp[8:12] + "002b" + "2fc0" + 16*"00" + 21*"00"
    tx_msg(dev[0], hapd, msg)
    rx_msg(hapd) # PSK-3 retry
    stop_psk_assoc(dev[0], hapd)

    start_psk_assoc(dev[0], hapd)
    proxy_msg(dev[0], hapd) # PSK-2
    proxy_msg(hapd, dev[0]) # PSK-3
    resp = rx_msg(dev[0])
    # Invalid PCHANNEL encryption
    hapd.note("EAP-PSK: PCHANNEL decryption failed")
    msg = resp[0:4] + "002b" + resp[8:12] + "002b" + "2fc0" + 16*"00" + 21*"11"
    tx_msg(dev[0], hapd, msg)
    rx_msg(hapd) # PSK-3 retry
    stop_psk_assoc(dev[0], hapd)

EAP_SIM_SUBTYPE_START = 10
EAP_SIM_SUBTYPE_CHALLENGE = 11
EAP_SIM_SUBTYPE_NOTIFICATION = 12
EAP_SIM_SUBTYPE_REAUTHENTICATION = 13
EAP_SIM_SUBTYPE_CLIENT_ERROR = 14

EAP_AKA_SUBTYPE_CHALLENGE = 1
EAP_AKA_SUBTYPE_AUTHENTICATION_REJECT = 2
EAP_AKA_SUBTYPE_SYNCHRONIZATION_FAILURE = 4
EAP_AKA_SUBTYPE_IDENTITY = 5
EAP_AKA_SUBTYPE_NOTIFICATION = 12
EAP_AKA_SUBTYPE_REAUTHENTICATION = 13
EAP_AKA_SUBTYPE_CLIENT_ERROR = 14

EAP_SIM_AT_RAND = 1
EAP_SIM_AT_AUTN = 2
EAP_SIM_AT_RES = 3
EAP_SIM_AT_AUTS = 4
EAP_SIM_AT_PADDING = 6
EAP_SIM_AT_NONCE_MT = 7
EAP_SIM_AT_PERMANENT_ID_REQ = 10
EAP_SIM_AT_MAC = 11
EAP_SIM_AT_NOTIFICATION = 12
EAP_SIM_AT_ANY_ID_REQ = 13
EAP_SIM_AT_IDENTITY = 14
EAP_SIM_AT_VERSION_LIST = 15
EAP_SIM_AT_SELECTED_VERSION = 16
EAP_SIM_AT_FULLAUTH_ID_REQ = 17
EAP_SIM_AT_COUNTER = 19
EAP_SIM_AT_COUNTER_TOO_SMALL = 20
EAP_SIM_AT_NONCE_S = 21
EAP_SIM_AT_CLIENT_ERROR_CODE = 22
EAP_SIM_AT_KDF_INPUT = 23
EAP_SIM_AT_KDF = 24
EAP_SIM_AT_IV = 129
EAP_SIM_AT_ENCR_DATA = 130
EAP_SIM_AT_NEXT_PSEUDONYM = 132
EAP_SIM_AT_NEXT_REAUTH_ID = 133
EAP_SIM_AT_CHECKCODE = 134
EAP_SIM_AT_RESULT_IND = 135
EAP_SIM_AT_BIDDING = 136

def test_eap_proto_aka(dev, apdev):
    """EAP-AKA protocol tests"""
    def aka_handler(ctx, req):
        logger.info("aka_handler - RX " + binascii.hexlify(req).decode())
        if 'num' not in ctx:
            ctx['num'] = 0
        ctx['num'] = ctx['num'] + 1
        if 'id' not in ctx:
            ctx['id'] = 1
        ctx['id'] = (ctx['id'] + 1) % 256

        idx = 0

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Missing payload")
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1,
                               EAP_TYPE_AKA)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unknown subtype")
            return struct.pack(">BBHBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3,
                               EAP_TYPE_AKA, 255, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Client Error")
            return struct.pack(">BBHBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3,
                               EAP_TYPE_AKA, EAP_AKA_SUBTYPE_CLIENT_ERROR, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Too short attribute header")
            return struct.pack(">BBHBBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 3,
                               EAP_TYPE_AKA, EAP_AKA_SUBTYPE_IDENTITY, 0, 255)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Truncated attribute")
            return struct.pack(">BBHBBHBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 4,
                               EAP_TYPE_AKA, EAP_AKA_SUBTYPE_IDENTITY, 0, 255,
                               255)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Too short attribute data")
            return struct.pack(">BBHBBHBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 4,
                               EAP_TYPE_AKA, EAP_AKA_SUBTYPE_IDENTITY, 0, 255,
                               0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Skippable/non-skippable unrecognzized attribute")
            return struct.pack(">BBHBBHBBHBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 10,
                               EAP_TYPE_AKA, EAP_AKA_SUBTYPE_IDENTITY, 0,
                               255, 1, 0, 127, 1, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Identity request without ID type")
            return struct.pack(">BBHBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3,
                               EAP_TYPE_AKA, EAP_AKA_SUBTYPE_IDENTITY, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Identity request ANY_ID")
            return struct.pack(">BBHBBHBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 4,
                               EAP_TYPE_AKA, EAP_AKA_SUBTYPE_IDENTITY, 0,
                               EAP_SIM_AT_ANY_ID_REQ, 1, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Identity request ANY_ID (duplicate)")
            return struct.pack(">BBHBBHBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 4,
                               EAP_TYPE_AKA, EAP_AKA_SUBTYPE_IDENTITY, 0,
                               EAP_SIM_AT_ANY_ID_REQ, 1, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Identity request ANY_ID")
            return struct.pack(">BBHBBHBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 4,
                               EAP_TYPE_AKA, EAP_AKA_SUBTYPE_IDENTITY, 0,
                               EAP_SIM_AT_ANY_ID_REQ, 1, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Identity request FULLAUTH_ID")
            return struct.pack(">BBHBBHBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 4,
                               EAP_TYPE_AKA, EAP_AKA_SUBTYPE_IDENTITY, 0,
                               EAP_SIM_AT_FULLAUTH_ID_REQ, 1, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Identity request FULLAUTH_ID (duplicate)")
            return struct.pack(">BBHBBHBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 4,
                               EAP_TYPE_AKA, EAP_AKA_SUBTYPE_IDENTITY, 0,
                               EAP_SIM_AT_FULLAUTH_ID_REQ, 1, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Identity request ANY_ID")
            return struct.pack(">BBHBBHBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 4,
                               EAP_TYPE_AKA, EAP_AKA_SUBTYPE_IDENTITY, 0,
                               EAP_SIM_AT_ANY_ID_REQ, 1, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Identity request FULLAUTH_ID")
            return struct.pack(">BBHBBHBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 4,
                               EAP_TYPE_AKA, EAP_AKA_SUBTYPE_IDENTITY, 0,
                               EAP_SIM_AT_FULLAUTH_ID_REQ, 1, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Identity request PERMANENT_ID")
            return struct.pack(">BBHBBHBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 4,
                               EAP_TYPE_AKA, EAP_AKA_SUBTYPE_IDENTITY, 0,
                               EAP_SIM_AT_PERMANENT_ID_REQ, 1, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Identity request PERMANENT_ID (duplicate)")
            return struct.pack(">BBHBBHBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 4,
                               EAP_TYPE_AKA, EAP_AKA_SUBTYPE_IDENTITY, 0,
                               EAP_SIM_AT_PERMANENT_ID_REQ, 1, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Challenge with no attributes")
            return struct.pack(">BBHBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3,
                               EAP_TYPE_AKA, EAP_AKA_SUBTYPE_CHALLENGE, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: AKA Challenge with BIDDING")
            return struct.pack(">BBHBBHBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 4,
                               EAP_TYPE_AKA, EAP_AKA_SUBTYPE_CHALLENGE, 0,
                               EAP_SIM_AT_BIDDING, 1, 0x8000)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Notification with no attributes")
            return struct.pack(">BBHBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3,
                               EAP_TYPE_AKA, EAP_AKA_SUBTYPE_NOTIFICATION, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Notification indicating success, but no MAC")
            return struct.pack(">BBHBBHBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 4,
                               EAP_TYPE_AKA, EAP_AKA_SUBTYPE_NOTIFICATION, 0,
                               EAP_SIM_AT_NOTIFICATION, 1, 32768)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Notification indicating success, but invalid MAC value")
            return struct.pack(">BBHBBHBBHBBH4L", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 4 + 20,
                               EAP_TYPE_AKA, EAP_AKA_SUBTYPE_NOTIFICATION, 0,
                               EAP_SIM_AT_NOTIFICATION, 1, 32768,
                               EAP_SIM_AT_MAC, 5, 0, 0, 0, 0, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Notification indicating success with zero-key MAC")
            return struct.pack(">BBHBBHBBHBBH16B", EAP_CODE_REQUEST,
                               ctx['id'] - 2,
                               4 + 1 + 3 + 4 + 20,
                               EAP_TYPE_AKA, EAP_AKA_SUBTYPE_NOTIFICATION, 0,
                               EAP_SIM_AT_NOTIFICATION, 1, 32768,
                               EAP_SIM_AT_MAC, 5, 0,
                               0xbe, 0x2e, 0xbb, 0xa9, 0xfa, 0x2e, 0x82, 0x36,
                               0x37, 0x8c, 0x32, 0x41, 0xb7, 0xc7, 0x58, 0xa3)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Success")
            return struct.pack(">BBH", EAP_CODE_SUCCESS, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Notification before auth")
            return struct.pack(">BBHBBHBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 4,
                               EAP_TYPE_AKA, EAP_AKA_SUBTYPE_NOTIFICATION, 0,
                               EAP_SIM_AT_NOTIFICATION, 1, 16384)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Notification before auth")
            return struct.pack(">BBHBBHBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 4,
                               EAP_TYPE_AKA, EAP_AKA_SUBTYPE_NOTIFICATION, 0,
                               EAP_SIM_AT_NOTIFICATION, 1, 16385)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Notification with unrecognized non-failure")
            return struct.pack(">BBHBBHBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 4,
                               EAP_TYPE_AKA, EAP_AKA_SUBTYPE_NOTIFICATION, 0,
                               EAP_SIM_AT_NOTIFICATION, 1, 0xc000)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Notification before auth (duplicate)")
            return struct.pack(">BBHBBHBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 4,
                               EAP_TYPE_AKA, EAP_AKA_SUBTYPE_NOTIFICATION, 0,
                               EAP_SIM_AT_NOTIFICATION, 1, 0xc000)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Re-authentication (unexpected) with no attributes")
            return struct.pack(">BBHBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3,
                               EAP_TYPE_AKA, EAP_AKA_SUBTYPE_REAUTHENTICATION,
                               0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: AKA Challenge with Checkcode claiming identity round was used")
            return struct.pack(">BBHBBHBBH5L", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 24,
                               EAP_TYPE_AKA, EAP_AKA_SUBTYPE_CHALLENGE, 0,
                               EAP_SIM_AT_CHECKCODE, 6, 0, 0, 0, 0, 0, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Identity request ANY_ID")
            return struct.pack(">BBHBBHBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 4,
                               EAP_TYPE_AKA, EAP_AKA_SUBTYPE_IDENTITY, 0,
                               EAP_SIM_AT_ANY_ID_REQ, 1, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: AKA Challenge with Checkcode claiming no identity round was used")
            return struct.pack(">BBHBBHBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 4,
                               EAP_TYPE_AKA, EAP_AKA_SUBTYPE_CHALLENGE, 0,
                               EAP_SIM_AT_CHECKCODE, 1, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Identity request ANY_ID")
            return struct.pack(">BBHBBHBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 4,
                               EAP_TYPE_AKA, EAP_AKA_SUBTYPE_IDENTITY, 0,
                               EAP_SIM_AT_ANY_ID_REQ, 1, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: AKA Challenge with mismatching Checkcode value")
            return struct.pack(">BBHBBHBBH5L", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 24,
                               EAP_TYPE_AKA, EAP_AKA_SUBTYPE_CHALLENGE, 0,
                               EAP_SIM_AT_CHECKCODE, 6, 0, 0, 0, 0, 0, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Re-authentication (unexpected) with Checkcode claimin identity round was used")
            return struct.pack(">BBHBBHBBH5L", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 24,
                               EAP_TYPE_AKA, EAP_AKA_SUBTYPE_REAUTHENTICATION,
                               0,
                               EAP_SIM_AT_CHECKCODE, 6, 0, 0, 0, 0, 0, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Invalid AT_RAND length")
            return struct.pack(">BBHBBHBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 4,
                               EAP_TYPE_AKA, EAP_AKA_SUBTYPE_IDENTITY, 0,
                               EAP_SIM_AT_RAND, 1, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Invalid AT_AUTN length")
            return struct.pack(">BBHBBHBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 4,
                               EAP_TYPE_AKA, EAP_AKA_SUBTYPE_IDENTITY, 0,
                               EAP_SIM_AT_AUTN, 1, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unencrypted AT_PADDING")
            return struct.pack(">BBHBBHBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 4,
                               EAP_TYPE_AKA, EAP_AKA_SUBTYPE_IDENTITY, 0,
                               EAP_SIM_AT_PADDING, 1, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Invalid AT_NONCE_MT length")
            return struct.pack(">BBHBBHBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 4,
                               EAP_TYPE_AKA, EAP_AKA_SUBTYPE_IDENTITY, 0,
                               EAP_SIM_AT_NONCE_MT, 1, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Invalid AT_MAC length")
            return struct.pack(">BBHBBHBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 4,
                               EAP_TYPE_AKA, EAP_AKA_SUBTYPE_IDENTITY, 0,
                               EAP_SIM_AT_MAC, 1, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Invalid AT_NOTIFICATION length")
            return struct.pack(">BBHBBHBBHL", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 8,
                               EAP_TYPE_AKA, EAP_AKA_SUBTYPE_IDENTITY, 0,
                               EAP_SIM_AT_NOTIFICATION, 2, 0, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: AT_IDENTITY overflow")
            return struct.pack(">BBHBBHBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 4,
                               EAP_TYPE_AKA, EAP_AKA_SUBTYPE_IDENTITY, 0,
                               EAP_SIM_AT_IDENTITY, 1, 0xffff)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unexpected AT_VERSION_LIST")
            return struct.pack(">BBHBBHBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 4,
                               EAP_TYPE_AKA, EAP_AKA_SUBTYPE_IDENTITY, 0,
                               EAP_SIM_AT_VERSION_LIST, 1, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Invalid AT_SELECTED_VERSION length")
            return struct.pack(">BBHBBHBBHL", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 8,
                               EAP_TYPE_AKA, EAP_AKA_SUBTYPE_IDENTITY, 0,
                               EAP_SIM_AT_SELECTED_VERSION, 2, 0, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unencrypted AT_COUNTER")
            return struct.pack(">BBHBBHBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 4,
                               EAP_TYPE_AKA, EAP_AKA_SUBTYPE_IDENTITY, 0,
                               EAP_SIM_AT_COUNTER, 1, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unencrypted AT_COUNTER_TOO_SMALL")
            return struct.pack(">BBHBBHBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 4,
                               EAP_TYPE_AKA, EAP_AKA_SUBTYPE_IDENTITY, 0,
                               EAP_SIM_AT_COUNTER_TOO_SMALL, 1, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unencrypted AT_NONCE_S")
            return struct.pack(">BBHBBHBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 4,
                               EAP_TYPE_AKA, EAP_AKA_SUBTYPE_IDENTITY, 0,
                               EAP_SIM_AT_NONCE_S, 1, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Invalid AT_CLIENT_ERROR_CODE length")
            return struct.pack(">BBHBBHBBHL", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 8,
                               EAP_TYPE_AKA, EAP_AKA_SUBTYPE_IDENTITY, 0,
                               EAP_SIM_AT_CLIENT_ERROR_CODE, 2, 0, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Invalid AT_IV length")
            return struct.pack(">BBHBBHBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 4,
                               EAP_TYPE_AKA, EAP_AKA_SUBTYPE_IDENTITY, 0,
                               EAP_SIM_AT_IV, 1, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Invalid AT_ENCR_DATA length")
            return struct.pack(">BBHBBHBBHL", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 8,
                               EAP_TYPE_AKA, EAP_AKA_SUBTYPE_IDENTITY, 0,
                               EAP_SIM_AT_ENCR_DATA, 2, 0, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unencrypted AT_NEXT_PSEUDONYM")
            return struct.pack(">BBHBBHBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 4,
                               EAP_TYPE_AKA, EAP_AKA_SUBTYPE_IDENTITY, 0,
                               EAP_SIM_AT_NEXT_PSEUDONYM, 1, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unencrypted AT_NEXT_REAUTH_ID")
            return struct.pack(">BBHBBHBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 4,
                               EAP_TYPE_AKA, EAP_AKA_SUBTYPE_IDENTITY, 0,
                               EAP_SIM_AT_NEXT_REAUTH_ID, 1, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Invalid AT_RES length")
            return struct.pack(">BBHBBHBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 4,
                               EAP_TYPE_AKA, EAP_AKA_SUBTYPE_IDENTITY, 0,
                               EAP_SIM_AT_RES, 1, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Invalid AT_RES length")
            return struct.pack(">BBHBBHBBH5L", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 24,
                               EAP_TYPE_AKA, EAP_AKA_SUBTYPE_IDENTITY, 0,
                               EAP_SIM_AT_RES, 6, 0xffff, 0, 0, 0, 0, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Invalid AT_AUTS length")
            return struct.pack(">BBHBBHBBHL", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 8,
                               EAP_TYPE_AKA, EAP_AKA_SUBTYPE_IDENTITY, 0,
                               EAP_SIM_AT_AUTS, 2, 0, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Invalid AT_CHECKCODE length")
            return struct.pack(">BBHBBHBBHL", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 8,
                               EAP_TYPE_AKA, EAP_AKA_SUBTYPE_IDENTITY, 0,
                               EAP_SIM_AT_CHECKCODE, 2, 0, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Invalid AT_RESULT_IND length")
            return struct.pack(">BBHBBHBBHL", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 8,
                               EAP_TYPE_AKA, EAP_AKA_SUBTYPE_IDENTITY, 0,
                               EAP_SIM_AT_RESULT_IND, 2, 0, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unexpected AT_KDF_INPUT")
            return struct.pack(">BBHBBHBBHL", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 8,
                               EAP_TYPE_AKA, EAP_AKA_SUBTYPE_IDENTITY, 0,
                               EAP_SIM_AT_KDF_INPUT, 2, 0, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unexpected AT_KDF")
            return struct.pack(">BBHBBHBBHL", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 8,
                               EAP_TYPE_AKA, EAP_AKA_SUBTYPE_IDENTITY, 0,
                               EAP_SIM_AT_KDF, 2, 0, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Invalid AT_BIDDING length")
            return struct.pack(">BBHBBHBBHL", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 8,
                               EAP_TYPE_AKA, EAP_AKA_SUBTYPE_IDENTITY, 0,
                               EAP_SIM_AT_BIDDING, 2, 0, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        return None

    srv = start_radius_server(aka_handler)

    try:
        hapd = start_ap(apdev[0])
        dev[0].scan_for_bss(hapd.own_addr(), freq=2412)

        for i in range(0, 49):
            eap = "AKA AKA'" if i == 11 else "AKA"
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap=eap, identity="0232010000000000",
                           password="90dca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb82581:000000000123",
                           wait_connect=False)
            ev = dev[0].wait_event(["CTRL-EVENT-EAP-PROPOSED-METHOD"],
                                   timeout=15)
            if ev is None:
                raise Exception("Timeout on EAP start")
            if i in [0, 15]:
                time.sleep(0.1)
            else:
                ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"],
                                       timeout=10)
                if ev is None:
                    raise Exception("Timeout on EAP failure")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].dump_monitor()
    finally:
        stop_radius_server(srv)

def test_eap_proto_aka_prime(dev, apdev):
    """EAP-AKA' protocol tests"""
    def aka_prime_handler(ctx, req):
        logger.info("aka_prime_handler - RX " + binascii.hexlify(req).decode())
        if 'num' not in ctx:
            ctx['num'] = 0
        ctx['num'] = ctx['num'] + 1
        if 'id' not in ctx:
            ctx['id'] = 1
        ctx['id'] = (ctx['id'] + 1) % 256

        idx = 0

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Missing payload")
            dev[0].note("Missing payload")
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1,
                               EAP_TYPE_AKA_PRIME)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Challenge with no attributes")
            dev[0].note("Challenge with no attributes")
            return struct.pack(">BBHBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3,
                               EAP_TYPE_AKA_PRIME, EAP_AKA_SUBTYPE_CHALLENGE, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Challenge with empty AT_KDF_INPUT")
            dev[0].note("Challenge with empty AT_KDF_INPUT")
            return struct.pack(">BBHBBHBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 4,
                               EAP_TYPE_AKA_PRIME, EAP_AKA_SUBTYPE_CHALLENGE, 0,
                               EAP_SIM_AT_KDF_INPUT, 1, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Challenge with AT_KDF_INPUT")
            dev[0].note("Test: Challenge with AT_KDF_INPUT")
            return struct.pack(">BBHBBHBBHBBBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 8,
                               EAP_TYPE_AKA_PRIME, EAP_AKA_SUBTYPE_CHALLENGE, 0,
                               EAP_SIM_AT_KDF_INPUT, 2, 1, ord('a'), ord('b'),
                               ord('c'), ord('d'))
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Challenge with duplicated KDF")
            dev[0].note("Challenge with duplicated KDF")
            return struct.pack(">BBHBBHBBHBBBBBBHBBHBBH",
                               EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 8 + 3 * 4,
                               EAP_TYPE_AKA_PRIME, EAP_AKA_SUBTYPE_CHALLENGE, 0,
                               EAP_SIM_AT_KDF_INPUT, 2, 1, ord('a'), ord('b'),
                               ord('c'), ord('d'),
                               EAP_SIM_AT_KDF, 1, 1,
                               EAP_SIM_AT_KDF, 1, 2,
                               EAP_SIM_AT_KDF, 1, 1)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Challenge with multiple KDF proposals")
            dev[0].note("Challenge with multiple KDF proposals (preparation)")
            return struct.pack(">BBHBBHBBHBBBBBBHBBHBBH",
                               EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 8 + 3 * 4,
                               EAP_TYPE_AKA_PRIME, EAP_AKA_SUBTYPE_CHALLENGE, 0,
                               EAP_SIM_AT_KDF_INPUT, 2, 1, ord('a'), ord('b'),
                               ord('c'), ord('d'),
                               EAP_SIM_AT_KDF, 1, 255,
                               EAP_SIM_AT_KDF, 1, 254,
                               EAP_SIM_AT_KDF, 1, 1)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Challenge with incorrect KDF selected")
            dev[0].note("Challenge with incorrect KDF selected")
            return struct.pack(">BBHBBHBBHBBBBBBHBBHBBHBBH",
                               EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 8 + 4 * 4,
                               EAP_TYPE_AKA_PRIME, EAP_AKA_SUBTYPE_CHALLENGE, 0,
                               EAP_SIM_AT_KDF_INPUT, 2, 1, ord('a'), ord('b'),
                               ord('c'), ord('d'),
                               EAP_SIM_AT_KDF, 1, 255,
                               EAP_SIM_AT_KDF, 1, 255,
                               EAP_SIM_AT_KDF, 1, 254,
                               EAP_SIM_AT_KDF, 1, 1)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Challenge with multiple KDF proposals")
            dev[0].note("Challenge with multiple KDF proposals (preparation)")
            return struct.pack(">BBHBBHBBHBBBBBBHBBHBBH",
                               EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 8 + 3 * 4,
                               EAP_TYPE_AKA_PRIME, EAP_AKA_SUBTYPE_CHALLENGE, 0,
                               EAP_SIM_AT_KDF_INPUT, 2, 1, ord('a'), ord('b'),
                               ord('c'), ord('d'),
                               EAP_SIM_AT_KDF, 1, 255,
                               EAP_SIM_AT_KDF, 1, 254,
                               EAP_SIM_AT_KDF, 1, 1)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Challenge with selected KDF not duplicated")
            dev[0].note("Challenge with selected KDF not duplicated")
            return struct.pack(">BBHBBHBBHBBBBBBHBBHBBH",
                               EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 8 + 3 * 4,
                               EAP_TYPE_AKA_PRIME, EAP_AKA_SUBTYPE_CHALLENGE, 0,
                               EAP_SIM_AT_KDF_INPUT, 2, 1, ord('a'), ord('b'),
                               ord('c'), ord('d'),
                               EAP_SIM_AT_KDF, 1, 1,
                               EAP_SIM_AT_KDF, 1, 255,
                               EAP_SIM_AT_KDF, 1, 254)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Challenge with multiple KDF proposals")
            dev[0].note("Challenge with multiple KDF proposals (preparation)")
            return struct.pack(">BBHBBHBBHBBBBBBHBBHBBH",
                               EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 8 + 3 * 4,
                               EAP_TYPE_AKA_PRIME, EAP_AKA_SUBTYPE_CHALLENGE, 0,
                               EAP_SIM_AT_KDF_INPUT, 2, 1, ord('a'), ord('b'),
                               ord('c'), ord('d'),
                               EAP_SIM_AT_KDF, 1, 255,
                               EAP_SIM_AT_KDF, 1, 254,
                               EAP_SIM_AT_KDF, 1, 1)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Challenge with selected KDF duplicated (missing MAC, RAND, AUTN)")
            dev[0].note("Challenge with selected KDF duplicated (missing MAC, RAND, AUTN)")
            return struct.pack(">BBHBBHBBHBBBBBBHBBHBBHBBH",
                               EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 8 + 4 * 4,
                               EAP_TYPE_AKA_PRIME, EAP_AKA_SUBTYPE_CHALLENGE, 0,
                               EAP_SIM_AT_KDF_INPUT, 2, 1, ord('a'), ord('b'),
                               ord('c'), ord('d'),
                               EAP_SIM_AT_KDF, 1, 1,
                               EAP_SIM_AT_KDF, 1, 255,
                               EAP_SIM_AT_KDF, 1, 254,
                               EAP_SIM_AT_KDF, 1, 1)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Challenge with multiple unsupported KDF proposals")
            dev[0].note("Challenge with multiple unsupported KDF proposals")
            return struct.pack(">BBHBBHBBHBBBBBBHBBH",
                               EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 8 + 2 * 4,
                               EAP_TYPE_AKA_PRIME, EAP_AKA_SUBTYPE_CHALLENGE, 0,
                               EAP_SIM_AT_KDF_INPUT, 2, 1, ord('a'), ord('b'),
                               ord('c'), ord('d'),
                               EAP_SIM_AT_KDF, 1, 255,
                               EAP_SIM_AT_KDF, 1, 254)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Challenge with multiple KDF proposals")
            dev[0].note("Challenge with multiple KDF proposals (preparation)")
            return struct.pack(">BBHBBHBBHBBBBBBHBBHBBH",
                               EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 8 + 3 * 4,
                               EAP_TYPE_AKA_PRIME, EAP_AKA_SUBTYPE_CHALLENGE, 0,
                               EAP_SIM_AT_KDF_INPUT, 2, 1, ord('a'), ord('b'),
                               ord('c'), ord('d'),
                               EAP_SIM_AT_KDF, 1, 255,
                               EAP_SIM_AT_KDF, 1, 254,
                               EAP_SIM_AT_KDF, 1, 1)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Challenge with invalid MAC, RAND, AUTN values)")
            dev[0].note("Challenge with invalid MAC, RAND, AUTN values)")
            return struct.pack(">BBHBBHBBHBBBBBBHBBHBBHBBHBBH4LBBH4LBBH4L",
                               EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 8 + 4 * 4 + 20 + 20 + 20,
                               EAP_TYPE_AKA_PRIME, EAP_AKA_SUBTYPE_CHALLENGE, 0,
                               EAP_SIM_AT_KDF_INPUT, 2, 1, ord('a'), ord('b'),
                               ord('c'), ord('d'),
                               EAP_SIM_AT_KDF, 1, 1,
                               EAP_SIM_AT_KDF, 1, 255,
                               EAP_SIM_AT_KDF, 1, 254,
                               EAP_SIM_AT_KDF, 1, 1,
                               EAP_SIM_AT_MAC, 5, 0, 0, 0, 0, 0,
                               EAP_SIM_AT_RAND, 5, 0, 0, 0, 0, 0,
                               EAP_SIM_AT_AUTN, 5, 0, 0, 0, 0, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Challenge - AMF separation bit not set)")
            dev[0].note("Challenge - AMF separation bit not set)")
            return struct.pack(">BBHBBHBBHBBBBBBHBBH4LBBH4LBBH4L",
                               EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 8 + 4 + 20 + 20 + 20,
                               EAP_TYPE_AKA_PRIME, EAP_AKA_SUBTYPE_CHALLENGE, 0,
                               EAP_SIM_AT_KDF_INPUT, 2, 1, ord('a'), ord('b'),
                               ord('c'), ord('d'),
                               EAP_SIM_AT_KDF, 1, 1,
                               EAP_SIM_AT_MAC, 5, 0, 1, 2, 3, 4,
                               EAP_SIM_AT_RAND, 5, 0, 5, 6, 7, 8,
                               EAP_SIM_AT_AUTN, 5, 0, 9, 10,
                               0x2fda8ef7, 0xbba518cc)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Challenge - Invalid MAC")
            dev[0].note("Challenge - Invalid MAC")
            return struct.pack(">BBHBBHBBHBBBBBBHBBH4LBBH4LBBH4L",
                               EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 8 + 4 + 20 + 20 + 20,
                               EAP_TYPE_AKA_PRIME, EAP_AKA_SUBTYPE_CHALLENGE, 0,
                               EAP_SIM_AT_KDF_INPUT, 2, 1, ord('a'), ord('b'),
                               ord('c'), ord('d'),
                               EAP_SIM_AT_KDF, 1, 1,
                               EAP_SIM_AT_MAC, 5, 0, 1, 2, 3, 4,
                               EAP_SIM_AT_RAND, 5, 0, 5, 6, 7, 8,
                               EAP_SIM_AT_AUTN, 5, 0, 0xffffffff, 0xffffffff,
                               0xd1f90322, 0x40514cb4)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Challenge - Valid MAC")
            dev[0].note("Challenge - Valid MAC")
            return struct.pack(">BBHBBHBBHBBBBBBHBBH4LBBH4LBBH4L",
                               EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 8 + 4 + 20 + 20 + 20,
                               EAP_TYPE_AKA_PRIME, EAP_AKA_SUBTYPE_CHALLENGE, 0,
                               EAP_SIM_AT_KDF_INPUT, 2, 1, ord('a'), ord('b'),
                               ord('c'), ord('d'),
                               EAP_SIM_AT_KDF, 1, 1,
                               EAP_SIM_AT_MAC, 5, 0,
                               0xf4a3c1d3, 0x7c901401, 0x34bd8b01, 0x6f7fa32f,
                               EAP_SIM_AT_RAND, 5, 0, 5, 6, 7, 8,
                               EAP_SIM_AT_AUTN, 5, 0, 0xffffffff, 0xffffffff,
                               0xd1f90322, 0x40514cb4)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Invalid AT_KDF_INPUT length")
            dev[0].note("Invalid AT_KDF_INPUT length")
            return struct.pack(">BBHBBHBBHL", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 8,
                               EAP_TYPE_AKA_PRIME, EAP_AKA_SUBTYPE_IDENTITY, 0,
                               EAP_SIM_AT_KDF_INPUT, 2, 0xffff, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Invalid AT_KDF length")
            dev[0].note("Invalid AT_KDF length")
            return struct.pack(">BBHBBHBBHL", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 8,
                               EAP_TYPE_AKA_PRIME, EAP_AKA_SUBTYPE_IDENTITY, 0,
                               EAP_SIM_AT_KDF, 2, 0, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Challenge with large number of KDF proposals")
            dev[0].note("Challenge with large number of KDF proposals")
            return struct.pack(">BBHBBHBBHBBHBBHBBHBBHBBHBBHBBHBBHBBHBBHBBH",
                               EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 12 * 4,
                               EAP_TYPE_AKA_PRIME, EAP_AKA_SUBTYPE_CHALLENGE, 0,
                               EAP_SIM_AT_KDF, 1, 255,
                               EAP_SIM_AT_KDF, 1, 254,
                               EAP_SIM_AT_KDF, 1, 253,
                               EAP_SIM_AT_KDF, 1, 252,
                               EAP_SIM_AT_KDF, 1, 251,
                               EAP_SIM_AT_KDF, 1, 250,
                               EAP_SIM_AT_KDF, 1, 249,
                               EAP_SIM_AT_KDF, 1, 248,
                               EAP_SIM_AT_KDF, 1, 247,
                               EAP_SIM_AT_KDF, 1, 246,
                               EAP_SIM_AT_KDF, 1, 245,
                               EAP_SIM_AT_KDF, 1, 244)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Challenge with multiple KDF proposals")
            dev[0].note("Challenge with multiple KDF proposals (preparation)")
            return struct.pack(">BBHBBHBBHBBBBBBHBBH",
                               EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 8 + 2 * 4,
                               EAP_TYPE_AKA_PRIME, EAP_AKA_SUBTYPE_CHALLENGE, 0,
                               EAP_SIM_AT_KDF_INPUT, 2, 1, ord('a'), ord('b'),
                               ord('c'), ord('d'),
                               EAP_SIM_AT_KDF, 1, 2,
                               EAP_SIM_AT_KDF, 1, 1)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Challenge with an extra KDF appended")
            dev[0].note("Challenge with an extra KDF appended")
            return struct.pack(">BBHBBHBBHBBBBBBHBBHBBHBBH",
                               EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 8 + 4 * 4,
                               EAP_TYPE_AKA_PRIME, EAP_AKA_SUBTYPE_CHALLENGE, 0,
                               EAP_SIM_AT_KDF_INPUT, 2, 1, ord('a'), ord('b'),
                               ord('c'), ord('d'),
                               EAP_SIM_AT_KDF, 1, 1,
                               EAP_SIM_AT_KDF, 1, 2,
                               EAP_SIM_AT_KDF, 1, 1,
                               EAP_SIM_AT_KDF, 1, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Challenge with multiple KDF proposals")
            dev[0].note("Challenge with multiple KDF proposals (preparation)")
            return struct.pack(">BBHBBHBBHBBBBBBHBBH",
                               EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 8 + 2 * 4,
                               EAP_TYPE_AKA_PRIME, EAP_AKA_SUBTYPE_CHALLENGE, 0,
                               EAP_SIM_AT_KDF_INPUT, 2, 1, ord('a'), ord('b'),
                               ord('c'), ord('d'),
                               EAP_SIM_AT_KDF, 1, 2,
                               EAP_SIM_AT_KDF, 1, 1)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Challenge with a modified KDF")
            dev[0].note("Challenge with a modified KDF")
            return struct.pack(">BBHBBHBBHBBBBBBHBBHBBH",
                               EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 8 + 3 * 4,
                               EAP_TYPE_AKA_PRIME, EAP_AKA_SUBTYPE_CHALLENGE, 0,
                               EAP_SIM_AT_KDF_INPUT, 2, 1, ord('a'), ord('b'),
                               ord('c'), ord('d'),
                               EAP_SIM_AT_KDF, 1, 1,
                               EAP_SIM_AT_KDF, 1, 0,
                               EAP_SIM_AT_KDF, 1, 1)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        return None

    srv = start_radius_server(aka_prime_handler)

    try:
        hapd = start_ap(apdev[0])
        dev[0].scan_for_bss(hapd.own_addr(), freq=2412)

        for i in range(0, 18):
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="AKA'", identity="6555444333222111",
                           password="5122250214c33e723a5dd523fc145fc0:981d464c7c52eb6e5036234984ad0bcf:000000000123",
                           wait_connect=False)
            ev = dev[0].wait_event(["CTRL-EVENT-EAP-PROPOSED-METHOD"],
                                   timeout=15)
            if ev is None:
                raise Exception("Timeout on EAP start")
            if i in [0]:
                time.sleep(0.1)
            else:
                ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"],
                                       timeout=10)
                if ev is None:
                    raise Exception("Timeout on EAP failure")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].dump_monitor()
    finally:
        stop_radius_server(srv)

def test_eap_proto_sim(dev, apdev):
    """EAP-SIM protocol tests"""
    def sim_handler(ctx, req):
        logger.info("sim_handler - RX " + binascii.hexlify(req).decode())
        if 'num' not in ctx:
            ctx['num'] = 0
        ctx['num'] = ctx['num'] + 1
        if 'id' not in ctx:
            ctx['id'] = 1
        ctx['id'] = (ctx['id'] + 1) % 256

        idx = 0

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Missing payload")
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1,
                               EAP_TYPE_SIM)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unexpected AT_AUTN")
            return struct.pack(">BBHBBHBBHL", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 8,
                               EAP_TYPE_SIM, EAP_SIM_SUBTYPE_START, 0,
                               EAP_SIM_AT_AUTN, 2, 0, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Too short AT_VERSION_LIST")
            return struct.pack(">BBHBBHBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 4,
                               EAP_TYPE_SIM, EAP_SIM_SUBTYPE_START, 0,
                               EAP_SIM_AT_VERSION_LIST, 1, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: AT_VERSION_LIST overflow")
            return struct.pack(">BBHBBHBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 4,
                               EAP_TYPE_SIM, EAP_SIM_SUBTYPE_START, 0,
                               EAP_SIM_AT_VERSION_LIST, 1, 0xffff)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unexpected AT_AUTS")
            return struct.pack(">BBHBBHBBHL", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 8,
                               EAP_TYPE_SIM, EAP_SIM_SUBTYPE_START, 0,
                               EAP_SIM_AT_AUTS, 2, 0, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unexpected AT_CHECKCODE")
            return struct.pack(">BBHBBHBBHL", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 8,
                               EAP_TYPE_SIM, EAP_SIM_SUBTYPE_START, 0,
                               EAP_SIM_AT_CHECKCODE, 2, 0, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: No AT_VERSION_LIST in Start")
            return struct.pack(">BBHBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3,
                               EAP_TYPE_SIM, EAP_SIM_SUBTYPE_START, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: No support version in AT_VERSION_LIST")
            return struct.pack(">BBHBBHBBH4B", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 8,
                               EAP_TYPE_SIM, EAP_SIM_SUBTYPE_START, 0,
                               EAP_SIM_AT_VERSION_LIST, 2, 3, 2, 3, 4, 5)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)


        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Identity request without ID type")
            return struct.pack(">BBHBBHBBH2H", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 8,
                               EAP_TYPE_SIM, EAP_SIM_SUBTYPE_START, 0,
                               EAP_SIM_AT_VERSION_LIST, 2, 2, 1, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Identity request ANY_ID")
            return struct.pack(">BBHBBHBBH2HBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 8 + 4,
                               EAP_TYPE_SIM, EAP_SIM_SUBTYPE_START, 0,
                               EAP_SIM_AT_VERSION_LIST, 2, 2, 1, 0,
                               EAP_SIM_AT_ANY_ID_REQ, 1, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Identity request ANY_ID (duplicate)")
            return struct.pack(">BBHBBHBBH2HBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 8 + 4,
                               EAP_TYPE_SIM, EAP_SIM_SUBTYPE_START, 0,
                               EAP_SIM_AT_VERSION_LIST, 2, 2, 1, 0,
                               EAP_SIM_AT_ANY_ID_REQ, 1, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Identity request ANY_ID")
            return struct.pack(">BBHBBHBBH2HBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 8 + 4,
                               EAP_TYPE_SIM, EAP_SIM_SUBTYPE_START, 0,
                               EAP_SIM_AT_VERSION_LIST, 2, 2, 1, 0,
                               EAP_SIM_AT_ANY_ID_REQ, 1, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Identity request FULLAUTH_ID")
            return struct.pack(">BBHBBHBBH2HBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 8 + 4,
                               EAP_TYPE_SIM, EAP_SIM_SUBTYPE_START, 0,
                               EAP_SIM_AT_VERSION_LIST, 2, 2, 1, 0,
                               EAP_SIM_AT_FULLAUTH_ID_REQ, 1, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Identity request FULLAUTH_ID (duplicate)")
            return struct.pack(">BBHBBHBBH2HBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 8 + 4,
                               EAP_TYPE_SIM, EAP_SIM_SUBTYPE_START, 0,
                               EAP_SIM_AT_VERSION_LIST, 2, 2, 1, 0,
                               EAP_SIM_AT_FULLAUTH_ID_REQ, 1, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Identity request ANY_ID")
            return struct.pack(">BBHBBHBBH2HBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 8 + 4,
                               EAP_TYPE_SIM, EAP_SIM_SUBTYPE_START, 0,
                               EAP_SIM_AT_VERSION_LIST, 2, 2, 1, 0,
                               EAP_SIM_AT_ANY_ID_REQ, 1, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Identity request FULLAUTH_ID")
            return struct.pack(">BBHBBHBBH2HBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 8 + 4,
                               EAP_TYPE_SIM, EAP_SIM_SUBTYPE_START, 0,
                               EAP_SIM_AT_VERSION_LIST, 2, 2, 1, 0,
                               EAP_SIM_AT_FULLAUTH_ID_REQ, 1, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Identity request PERMANENT_ID")
            return struct.pack(">BBHBBHBBH2HBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 8 + 4,
                               EAP_TYPE_SIM, EAP_SIM_SUBTYPE_START, 0,
                               EAP_SIM_AT_VERSION_LIST, 2, 2, 1, 0,
                               EAP_SIM_AT_PERMANENT_ID_REQ, 1, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Identity request PERMANENT_ID (duplicate)")
            return struct.pack(">BBHBBHBBH2HBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 8 + 4,
                               EAP_TYPE_SIM, EAP_SIM_SUBTYPE_START, 0,
                               EAP_SIM_AT_VERSION_LIST, 2, 2, 1, 0,
                               EAP_SIM_AT_PERMANENT_ID_REQ, 1, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: No AT_MAC and AT_RAND in Challenge")
            return struct.pack(">BBHBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3,
                               EAP_TYPE_SIM, EAP_SIM_SUBTYPE_CHALLENGE, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: No AT_RAND in Challenge")
            return struct.pack(">BBHBBHBBH4L", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 20,
                               EAP_TYPE_SIM, EAP_SIM_SUBTYPE_CHALLENGE, 0,
                               EAP_SIM_AT_MAC, 5, 0, 0, 0, 0, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Insufficient number of challenges in Challenge")
            return struct.pack(">BBHBBHBBH4LBBH4L", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 20 + 20,
                               EAP_TYPE_SIM, EAP_SIM_SUBTYPE_CHALLENGE, 0,
                               EAP_SIM_AT_RAND, 5, 0, 0, 0, 0, 0,
                               EAP_SIM_AT_MAC, 5, 0, 0, 0, 0, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Too many challenges in Challenge")
            return struct.pack(">BBHBBHBBH4L4L4L4LBBH4L", EAP_CODE_REQUEST,
                               ctx['id'],
                               4 + 1 + 3 + 4 + 4 * 16 + 20,
                               EAP_TYPE_SIM, EAP_SIM_SUBTYPE_CHALLENGE, 0,
                               EAP_SIM_AT_RAND, 17, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                               0, 0, 0, 0, 0, 0, 0, 0,
                               EAP_SIM_AT_MAC, 5, 0, 0, 0, 0, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Same RAND multiple times in Challenge")
            return struct.pack(">BBHBBHBBH4L4L4LBBH4L", EAP_CODE_REQUEST,
                               ctx['id'],
                               4 + 1 + 3 + 4 + 3 * 16 + 20,
                               EAP_TYPE_SIM, EAP_SIM_SUBTYPE_CHALLENGE, 0,
                               EAP_SIM_AT_RAND, 13, 0, 0, 0, 0, 0, 0, 0, 0, 1,
                               0, 0, 0, 0,
                               EAP_SIM_AT_MAC, 5, 0, 0, 0, 0, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Notification with no attributes")
            return struct.pack(">BBHBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3,
                               EAP_TYPE_SIM, EAP_SIM_SUBTYPE_NOTIFICATION, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Notification indicating success, but no MAC")
            return struct.pack(">BBHBBHBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 4,
                               EAP_TYPE_SIM, EAP_SIM_SUBTYPE_NOTIFICATION, 0,
                               EAP_SIM_AT_NOTIFICATION, 1, 32768)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Notification indicating success, but invalid MAC value")
            return struct.pack(">BBHBBHBBHBBH4L", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 4 + 20,
                               EAP_TYPE_SIM, EAP_SIM_SUBTYPE_NOTIFICATION, 0,
                               EAP_SIM_AT_NOTIFICATION, 1, 32768,
                               EAP_SIM_AT_MAC, 5, 0, 0, 0, 0, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Notification before auth")
            return struct.pack(">BBHBBHBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 4,
                               EAP_TYPE_SIM, EAP_SIM_SUBTYPE_NOTIFICATION, 0,
                               EAP_SIM_AT_NOTIFICATION, 1, 16384)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Notification before auth")
            return struct.pack(">BBHBBHBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 4,
                               EAP_TYPE_SIM, EAP_SIM_SUBTYPE_NOTIFICATION, 0,
                               EAP_SIM_AT_NOTIFICATION, 1, 16385)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Notification with unrecognized non-failure")
            return struct.pack(">BBHBBHBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 4,
                               EAP_TYPE_SIM, EAP_SIM_SUBTYPE_NOTIFICATION, 0,
                               EAP_SIM_AT_NOTIFICATION, 1, 0xc000)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Notification before auth (duplicate)")
            return struct.pack(">BBHBBHBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 4,
                               EAP_TYPE_SIM, EAP_SIM_SUBTYPE_NOTIFICATION, 0,
                               EAP_SIM_AT_NOTIFICATION, 1, 0xc000)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Re-authentication (unexpected) with no attributes")
            return struct.pack(">BBHBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3,
                               EAP_TYPE_SIM, EAP_SIM_SUBTYPE_REAUTHENTICATION,
                               0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Client Error")
            return struct.pack(">BBHBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3,
                               EAP_TYPE_SIM, EAP_SIM_SUBTYPE_CLIENT_ERROR, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unknown subtype")
            return struct.pack(">BBHBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3,
                               EAP_TYPE_SIM, 255, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        return None

    srv = start_radius_server(sim_handler)

    try:
        hapd = start_ap(apdev[0])
        dev[0].scan_for_bss(hapd.own_addr(), freq=2412)

        for i in range(0, 25):
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="SIM", identity="1232010000000000",
                           password="90dca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb82581",
                           wait_connect=False)
            ev = dev[0].wait_event(["CTRL-EVENT-EAP-PROPOSED-METHOD"],
                                   timeout=15)
            if ev is None:
                raise Exception("Timeout on EAP start")
            if i in [0]:
                time.sleep(0.1)
            else:
                ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"],
                                       timeout=10)
                if ev is None:
                    raise Exception("Timeout on EAP failure")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].dump_monitor()
    finally:
        stop_radius_server(srv)

def test_eap_proto_sim_errors(dev, apdev):
    """EAP-SIM protocol tests (error paths)"""
    check_hlr_auc_gw_support()
    params = hostapd.wpa2_eap_params(ssid="eap-test")
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].scan_for_bss(hapd.own_addr(), freq=2412)

    with alloc_fail(dev[0], 1, "eap_sim_init"):
        dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                       eap="SIM", identity="1232010000000000",
                       password="90dca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb82581",
                       wait_connect=False)
        ev = dev[0].wait_event(["EAP: Failed to initialize EAP method"],
                               timeout=15)
        if ev is None:
            raise Exception("Timeout on EAP start")
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()

    with fail_test(dev[0], 1, "os_get_random;eap_sim_init"):
        dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                       eap="SIM", identity="1232010000000000",
                       password="90dca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb82581",
                       wait_connect=False)
        ev = dev[0].wait_event(["EAP: Failed to initialize EAP method"],
                               timeout=15)
        if ev is None:
            raise Exception("Timeout on EAP start")
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()

    dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                   eap="SIM", identity="1232010000000000",
                   password="90dca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb82581")

    with fail_test(dev[0], 1, "aes_128_cbc_encrypt;eap_sim_response_reauth"):
        hapd.request("EAPOL_REAUTH " + dev[0].own_addr())
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED"], timeout=5)
        if ev is None:
            raise Exception("EAP re-authentication did not start")
        wait_fail_trigger(dev[0], "GET_FAIL")
        dev[0].request("REMOVE_NETWORK all")
        dev[0].dump_monitor()

    dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                   eap="SIM", identity="1232010000000000",
                   password="90dca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb82581")

    with fail_test(dev[0], 1, "os_get_random;eap_sim_msg_add_encr_start"):
        hapd.request("EAPOL_REAUTH " + dev[0].own_addr())
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED"], timeout=5)
        if ev is None:
            raise Exception("EAP re-authentication did not start")
        wait_fail_trigger(dev[0], "GET_FAIL")
        dev[0].request("REMOVE_NETWORK all")
        dev[0].dump_monitor()

    dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                   eap="SIM", identity="1232010000000000",
                   password="90dca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb82581")

    with fail_test(dev[0], 1, "os_get_random;eap_sim_init_for_reauth"):
        hapd.request("EAPOL_REAUTH " + dev[0].own_addr())
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED"], timeout=5)
        if ev is None:
            raise Exception("EAP re-authentication did not start")
        wait_fail_trigger(dev[0], "GET_FAIL")
        dev[0].request("REMOVE_NETWORK all")
        dev[0].dump_monitor()

    dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                   eap="SIM", identity="1232010000000000",
                   password="90dca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb82581")

    with alloc_fail(dev[0], 1, "eap_sim_parse_encr;eap_sim_process_reauthentication"):
        hapd.request("EAPOL_REAUTH " + dev[0].own_addr())
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED"], timeout=5)
        if ev is None:
            raise Exception("EAP re-authentication did not start")
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
        dev[0].request("REMOVE_NETWORK all")
        dev[0].dump_monitor()

    tests = [(1, "eap_sim_verify_mac;eap_sim_process_challenge"),
             (1, "eap_sim_parse_encr;eap_sim_process_challenge"),
             (1, "eap_sim_msg_init;eap_sim_response_start"),
             (1, "wpabuf_alloc;eap_sim_msg_init;eap_sim_response_start"),
             (1, "=eap_sim_learn_ids"),
             (2, "=eap_sim_learn_ids"),
             (2, "eap_sim_learn_ids"),
             (3, "eap_sim_learn_ids"),
             (1, "eap_sim_process_start"),
             (1, "eap_sim_getKey"),
             (1, "eap_sim_get_emsk"),
             (1, "eap_sim_get_session_id")]
    for count, func in tests:
        with alloc_fail(dev[0], count, func):
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="SIM", identity="1232010000000000@domain",
                           password="90dca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb82581",
                           erp="1", wait_connect=False)
            wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].dump_monitor()

    tests = [(1, "aes_128_cbc_decrypt;eap_sim_parse_encr")]
    for count, func in tests:
        with fail_test(dev[0], count, func):
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="SIM", identity="1232010000000000",
                           password="90dca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb82581",
                           wait_connect=False)
            wait_fail_trigger(dev[0], "GET_FAIL")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].dump_monitor()

    params = int_eap_server_params()
    params['eap_sim_db'] = "unix:/tmp/hlr_auc_gw.sock"
    params['eap_sim_aka_result_ind'] = "1"
    hapd2 = hostapd.add_ap(apdev[1], params)
    dev[0].scan_for_bss(hapd2.own_addr(), freq=2412)

    with alloc_fail(dev[0], 1,
                    "eap_sim_msg_init;eap_sim_response_notification"):
        dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP",
                       scan_freq="2412",
                       eap="SIM", identity="1232010000000000",
                       phase1="result_ind=1",
                       password="90dca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb82581",
                       wait_connect=False)
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
        dev[0].request("REMOVE_NETWORK all")
        dev[0].dump_monitor()

    tests = ["eap_sim_msg_add_encr_start;eap_sim_response_notification",
             "aes_128_cbc_encrypt;eap_sim_response_notification"]
    for func in tests:
        with fail_test(dev[0], 1, func):
            dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP",
                           scan_freq="2412",
                           eap="SIM", identity="1232010000000000",
                           phase1="result_ind=1",
                           password="90dca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb82581")
            dev[0].request("REAUTHENTICATE")
            ev = dev[0].wait_event(["CTRL-EVENT-EAP-METHOD"], timeout=5)
            if ev is None:
                raise Exception("EAP method not started on reauthentication")
            time.sleep(0.1)
            wait_fail_trigger(dev[0], "GET_FAIL")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].dump_monitor()

    tests = ["eap_sim_parse_encr;eap_sim_process_notification_reauth"]
    for func in tests:
        with alloc_fail(dev[0], 1, func):
            dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP",
                           scan_freq="2412",
                           eap="SIM", identity="1232010000000000",
                           phase1="result_ind=1",
                           password="90dca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb82581")
            dev[0].request("REAUTHENTICATE")
            ev = dev[0].wait_event(["CTRL-EVENT-EAP-METHOD"], timeout=5)
            if ev is None:
                raise Exception("EAP method not started on reauthentication")
            time.sleep(0.1)
            wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].dump_monitor()

def test_eap_proto_aka_errors(dev, apdev):
    """EAP-AKA protocol tests (error paths)"""
    check_hlr_auc_gw_support()
    params = hostapd.wpa2_eap_params(ssid="eap-test")
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].scan_for_bss(hapd.own_addr(), freq=2412)

    with alloc_fail(dev[0], 1, "eap_aka_init"):
        dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                       eap="AKA", identity="0232010000000000",
                       password="90dca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb82581:000000000123",
                       wait_connect=False)
        ev = dev[0].wait_event(["EAP: Failed to initialize EAP method"],
                               timeout=15)
        if ev is None:
            raise Exception("Timeout on EAP start")
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()

    tests = [(1, "=eap_aka_learn_ids"),
             (2, "=eap_aka_learn_ids"),
             (1, "eap_sim_parse_encr;eap_aka_process_challenge"),
             (1, "wpabuf_alloc;eap_aka_add_id_msg"),
             (1, "eap_aka_getKey"),
             (1, "eap_aka_get_emsk"),
             (1, "eap_aka_get_session_id")]
    for count, func in tests:
        with alloc_fail(dev[0], count, func):
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="AKA", identity="0232010000000000@domain",
                           password="90dca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb82581:000000000123",
                           erp="1", wait_connect=False)
            wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].dump_monitor()

    params = int_eap_server_params()
    params['eap_sim_db'] = "unix:/tmp/hlr_auc_gw.sock"
    params['eap_sim_aka_result_ind'] = "1"
    hapd2 = hostapd.add_ap(apdev[1], params)
    dev[0].scan_for_bss(hapd2.own_addr(), freq=2412)

    with alloc_fail(dev[0], 1,
                    "eap_sim_msg_init;eap_aka_response_notification"):
        dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", scan_freq="2412",
                       eap="AKA", identity="0232010000000000",
                       phase1="result_ind=1",
                       password="90dca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb82581:000000000123",
                       wait_connect=False)
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
        dev[0].request("REMOVE_NETWORK all")
        dev[0].dump_monitor()

    tests = [(1, "aes_128_encrypt_block;milenage_f1;milenage_check", None),
             (2, "aes_128_encrypt_block;milenage_f1;milenage_check", None),
             (1, "milenage_f2345;milenage_check", None),
             (7, "aes_128_encrypt_block;milenage_f2345;milenage_check",
              "ff0000000123"),
             (1, "aes_128_encrypt_block;milenage_f1;milenage_check",
              "fff000000123")]
    for count, func, seq in tests:
        if not seq:
            seq = "000000000123"
        with fail_test(dev[0], count, func):
            dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP",
                           scan_freq="2412",
                           eap="AKA", identity="0232010000000000",
                           phase1="result_ind=1",
                           password="90dca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb82581:" + seq,
                           wait_connect=False)
            wait_fail_trigger(dev[0], "GET_FAIL")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()
            dev[0].dump_monitor()

    tests = ["eap_sim_msg_add_encr_start;eap_aka_response_notification",
             "aes_128_cbc_encrypt;eap_aka_response_notification"]
    for func in tests:
        with fail_test(dev[0], 1, func):
            dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP",
                           scan_freq="2412",
                           eap="AKA", identity="0232010000000000",
                           phase1="result_ind=1",
                           password="90dca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb82581:000000000123")
            dev[0].request("REAUTHENTICATE")
            ev = dev[0].wait_event(["CTRL-EVENT-EAP-METHOD"], timeout=5)
            if ev is None:
                raise Exception("EAP method not started on reauthentication")
            time.sleep(0.1)
            wait_fail_trigger(dev[0], "GET_FAIL")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].dump_monitor()

    tests = ["eap_sim_parse_encr;eap_aka_process_notification_reauth"]
    for func in tests:
        with alloc_fail(dev[0], 1, func):
            dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP",
                           scan_freq="2412",
                           eap="AKA", identity="0232010000000000",
                           phase1="result_ind=1",
                           password="90dca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb82581:000000000123")
            dev[0].request("REAUTHENTICATE")
            ev = dev[0].wait_event(["CTRL-EVENT-EAP-METHOD"], timeout=5)
            if ev is None:
                raise Exception("EAP method not started on reauthentication")
            time.sleep(0.1)
            wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].dump_monitor()

def test_eap_proto_aka_prime_errors(dev, apdev):
    """EAP-AKA' protocol tests (error paths)"""
    check_hlr_auc_gw_support()
    params = hostapd.wpa2_eap_params(ssid="eap-test")
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].scan_for_bss(hapd.own_addr(), freq=2412)

    with alloc_fail(dev[0], 1, "eap_aka_init"):
        dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                       eap="AKA'", identity="6555444333222111",
                       password="5122250214c33e723a5dd523fc145fc0:981d464c7c52eb6e5036234984ad0bcf:000000000123",
                       wait_connect=False)
        ev = dev[0].wait_event(["EAP: Failed to initialize EAP method"],
                               timeout=15)
        if ev is None:
            raise Exception("Timeout on EAP start")
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()

    dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                   eap="AKA'", identity="6555444333222111",
                   password="5122250214c33e723a5dd523fc145fc0:981d464c7c52eb6e5036234984ad0bcf:000000000123")

    with fail_test(dev[0], 1, "aes_128_cbc_encrypt;eap_aka_response_reauth"):
        hapd.request("EAPOL_REAUTH " + dev[0].own_addr())
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED"], timeout=5)
        if ev is None:
            raise Exception("EAP re-authentication did not start")
        wait_fail_trigger(dev[0], "GET_FAIL")
        dev[0].request("REMOVE_NETWORK all")
        dev[0].dump_monitor()

    dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                   eap="AKA'", identity="6555444333222111",
                   password="5122250214c33e723a5dd523fc145fc0:981d464c7c52eb6e5036234984ad0bcf:000000000123")

    with alloc_fail(dev[0], 1, "eap_sim_parse_encr;eap_aka_process_reauthentication"):
        hapd.request("EAPOL_REAUTH " + dev[0].own_addr())
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED"], timeout=5)
        if ev is None:
            raise Exception("EAP re-authentication did not start")
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
        dev[0].request("REMOVE_NETWORK all")
        dev[0].dump_monitor()

    tests = [(1, "eap_sim_verify_mac_sha256"),
             (1, "=eap_aka_process_challenge")]
    for count, func in tests:
        with alloc_fail(dev[0], count, func):
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="AKA'", identity="6555444333222111",
                           password="5122250214c33e723a5dd523fc145fc0:981d464c7c52eb6e5036234984ad0bcf:000000000123",
                           erp="1", wait_connect=False)
            wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].dump_monitor()

def test_eap_proto_ikev2(dev, apdev):
    """EAP-IKEv2 protocol tests"""
    check_eap_capa(dev[0], "IKEV2")

    global eap_proto_ikev2_test_done
    eap_proto_ikev2_test_done = False

    def ikev2_handler(ctx, req):
        logger.info("ikev2_handler - RX " + binascii.hexlify(req).decode())
        if 'num' not in ctx:
            ctx['num'] = 0
        ctx['num'] = ctx['num'] + 1
        if 'id' not in ctx:
            ctx['id'] = 1
        ctx['id'] = (ctx['id'] + 1) % 256

        idx = 0

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Missing payload")
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1,
                               EAP_TYPE_IKEV2)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Truncated Message Length field")
            return struct.pack(">BBHBB3B", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 3,
                               EAP_TYPE_IKEV2, 0x80, 0, 0, 0)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Too short Message Length value")
            return struct.pack(">BBHBBLB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 4 + 1,
                               EAP_TYPE_IKEV2, 0x80, 0, 1)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Truncated message")
            return struct.pack(">BBHBBL", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 4,
                               EAP_TYPE_IKEV2, 0x80, 1)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Truncated message(2)")
            return struct.pack(">BBHBBL", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 4,
                               EAP_TYPE_IKEV2, 0x80, 0xffffffff)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Truncated message(3)")
            return struct.pack(">BBHBBL", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 4,
                               EAP_TYPE_IKEV2, 0xc0, 0xffffffff)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Truncated message(4)")
            return struct.pack(">BBHBBL", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 4,
                               EAP_TYPE_IKEV2, 0xc0, 10000000)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Too long fragments (first fragment)")
            return struct.pack(">BBHBBLB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 4 + 1,
                               EAP_TYPE_IKEV2, 0xc0, 2, 1)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Too long fragments (second fragment)")
            return struct.pack(">BBHBB2B", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 2,
                               EAP_TYPE_IKEV2, 0x00, 2, 3)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: No Message Length field in first fragment")
            return struct.pack(">BBHBBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 1,
                               EAP_TYPE_IKEV2, 0x40, 1)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: ICV before keys")
            return struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1,
                               EAP_TYPE_IKEV2, 0x20)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unsupported IKEv2 header version")
            return struct.pack(">BBHBB2L2LBBBBLL", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 28,
                               EAP_TYPE_IKEV2, 0x00,
                               0, 0, 0, 0,
                               0, 0, 0, 0, 0, 0)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Incorrect IKEv2 header Length")
            return struct.pack(">BBHBB2L2LBBBBLL", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 28,
                               EAP_TYPE_IKEV2, 0x00,
                               0, 0, 0, 0,
                               0, 0x20, 0, 0, 0, 0)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unexpected IKEv2 Exchange Type in SA_INIT state")
            return struct.pack(">BBHBB2L2LBBBBLL", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 28,
                               EAP_TYPE_IKEV2, 0x00,
                               0, 0, 0, 0,
                               0, 0x20, 0, 0, 0, 28)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unexpected IKEv2 Message ID in SA_INIT state")
            return struct.pack(">BBHBB2L2LBBBBLL", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 28,
                               EAP_TYPE_IKEV2, 0x00,
                               0, 0, 0, 0,
                               0, 0x20, 34, 0, 1, 28)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unexpected IKEv2 Flags value")
            return struct.pack(">BBHBB2L2LBBBBLL", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 28,
                               EAP_TYPE_IKEV2, 0x00,
                               0, 0, 0, 0,
                               0, 0x20, 34, 0, 0, 28)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unexpected IKEv2 Flags value(2)")
            return struct.pack(">BBHBB2L2LBBBBLL", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 28,
                               EAP_TYPE_IKEV2, 0x00,
                               0, 0, 0, 0,
                               0, 0x20, 34, 0x20, 0, 28)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: No SAi1 in SA_INIT")
            return struct.pack(">BBHBB2L2LBBBBLL", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 28,
                               EAP_TYPE_IKEV2, 0x00,
                               0, 0, 0, 0,
                               0, 0x20, 34, 0x08, 0, 28)

        def build_ike(id, next=0, exch_type=34, flags=0x00, ike=b''):
            return struct.pack(">BBHBB2L2LBBBBLL", EAP_CODE_REQUEST, id,
                               4 + 1 + 1 + 28 + len(ike),
                               EAP_TYPE_IKEV2, flags,
                               0, 0, 0, 0,
                               next, 0x20, exch_type, 0x08, 0,
                               28 + len(ike)) + ike

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unexpected extra data after payloads")
            return build_ike(ctx['id'], ike=struct.pack(">B", 1))

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Truncated payload header")
            return build_ike(ctx['id'], next=128, ike=struct.pack(">B", 1))

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Too small payload header length")
            ike = struct.pack(">BBH", 0, 0, 3)
            return build_ike(ctx['id'], next=128, ike=ike)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Too large payload header length")
            ike = struct.pack(">BBH", 0, 0, 5)
            return build_ike(ctx['id'], next=128, ike=ike)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unsupported payload (non-critical and critical)")
            ike = struct.pack(">BBHBBH", 129, 0, 4, 0, 0x01, 4)
            return build_ike(ctx['id'], next=128, ike=ike)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Certificate and empty SAi1")
            ike = struct.pack(">BBHBBH", 33, 0, 4, 0, 0, 4)
            return build_ike(ctx['id'], next=37, ike=ike)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Too short proposal")
            ike = struct.pack(">BBHBBHBBB", 0, 0, 4 + 7,
                              0, 0, 7, 0, 0, 0)
            return build_ike(ctx['id'], next=33, ike=ike)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Too small proposal length in SAi1")
            ike = struct.pack(">BBHBBHBBBB", 0, 0, 4 + 8,
                              0, 0, 7, 0, 0, 0, 0)
            return build_ike(ctx['id'], next=33, ike=ike)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Too large proposal length in SAi1")
            ike = struct.pack(">BBHBBHBBBB", 0, 0, 4 + 8,
                              0, 0, 9, 0, 0, 0, 0)
            return build_ike(ctx['id'], next=33, ike=ike)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unexpected proposal type in SAi1")
            ike = struct.pack(">BBHBBHBBBB", 0, 0, 4 + 8,
                              1, 0, 8, 0, 0, 0, 0)
            return build_ike(ctx['id'], next=33, ike=ike)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unexpected Protocol ID in SAi1")
            ike = struct.pack(">BBHBBHBBBB", 0, 0, 4 + 8,
                              0, 0, 8, 0, 0, 0, 0)
            return build_ike(ctx['id'], next=33, ike=ike)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unexpected proposal number in SAi1")
            ike = struct.pack(">BBHBBHBBBB", 0, 0, 4 + 8,
                              0, 0, 8, 0, 1, 0, 0)
            return build_ike(ctx['id'], next=33, ike=ike)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Not enough room for SPI in SAi1")
            ike = struct.pack(">BBHBBHBBBB", 0, 0, 4 + 8,
                              0, 0, 8, 1, 1, 1, 0)
            return build_ike(ctx['id'], next=33, ike=ike)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unexpected SPI in SAi1")
            ike = struct.pack(">BBHBBHBBBBB", 0, 0, 4 + 9,
                              0, 0, 9, 1, 1, 1, 0, 1)
            return build_ike(ctx['id'], next=33, ike=ike)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: No transforms in SAi1")
            ike = struct.pack(">BBHBBHBBBB", 0, 0, 4 + 8,
                              0, 0, 8, 1, 1, 0, 0)
            return build_ike(ctx['id'], next=33, ike=ike)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Too short transform in SAi1")
            ike = struct.pack(">BBHBBHBBBB", 0, 0, 4 + 8,
                              0, 0, 8, 1, 1, 0, 1)
            return build_ike(ctx['id'], next=33, ike=ike)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Too small transform length in SAi1")
            ike = struct.pack(">BBHBBHBBBBBBHBBH", 0, 0, 4 + 8 + 8,
                              0, 0, 8 + 8, 1, 1, 0, 1,
                              0, 0, 7, 0, 0, 0)
            return build_ike(ctx['id'], next=33, ike=ike)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Too large transform length in SAi1")
            ike = struct.pack(">BBHBBHBBBBBBHBBH", 0, 0, 4 + 8 + 8,
                              0, 0, 8 + 8, 1, 1, 0, 1,
                              0, 0, 9, 0, 0, 0)
            return build_ike(ctx['id'], next=33, ike=ike)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unexpected Transform type in SAi1")
            ike = struct.pack(">BBHBBHBBBBBBHBBH", 0, 0, 4 + 8 + 8,
                              0, 0, 8 + 8, 1, 1, 0, 1,
                              1, 0, 8, 0, 0, 0)
            return build_ike(ctx['id'], next=33, ike=ike)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: No transform attributes in SAi1")
            ike = struct.pack(">BBHBBHBBBBBBHBBH", 0, 0, 4 + 8 + 8,
                              0, 0, 8 + 8, 1, 1, 0, 1,
                              0, 0, 8, 0, 0, 0)
            return build_ike(ctx['id'], next=33, ike=ike)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: No transform attr for AES and unexpected data after transforms in SAi1")
            tlen1 = 8 + 3
            tlen2 = 8 + 4
            tlen3 = 8 + 4
            tlen = tlen1 + tlen2 + tlen3
            ike = struct.pack(">BBHBBHBBBBBBHBBH3BBBHBBHHHBBHBBHHHB",
                              0, 0, 4 + 8 + tlen + 1,
                              0, 0, 8 + tlen + 1, 1, 1, 0, 3,
                              3, 0, tlen1, 1, 0, 12, 1, 2, 3,
                              3, 0, tlen2, 1, 0, 12, 0, 128,
                              0, 0, tlen3, 1, 0, 12, 0x8000 | 14, 127,
                              1)
            return build_ike(ctx['id'], next=33, ike=ike)

        def build_sa(next=0):
            tlen = 5 * 8
            return struct.pack(">BBHBBHBBBBBBHBBHBBHBBHBBHBBHBBHBBHBBHBBH",
                               next, 0, 4 + 8 + tlen,
                               0, 0, 8 + tlen, 1, 1, 0, 5,
                               3, 0, 8, 1, 0, 3,
                               3, 0, 8, 2, 0, 1,
                               3, 0, 8, 3, 0, 1,
                               3, 0, 8, 4, 0, 5,
                               0, 0, 8, 241, 0, 0)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Valid proposal, but no KEi in SAi1")
            ike = build_sa()
            return build_ike(ctx['id'], next=33, ike=ike)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Empty KEi in SAi1")
            ike = build_sa(next=34) + struct.pack(">BBH", 0, 0, 4)
            return build_ike(ctx['id'], next=33, ike=ike)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Mismatch in DH Group in SAi1")
            ike = build_sa(next=34)
            ike += struct.pack(">BBHHH", 0, 0, 4 + 4 + 96, 12345, 0)
            ike += 96*b'\x00'
            return build_ike(ctx['id'], next=33, ike=ike)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Invalid DH public value length in SAi1")
            ike = build_sa(next=34)
            ike += struct.pack(">BBHHH", 0, 0, 4 + 4 + 96, 5, 0)
            ike += 96*b'\x00'
            return build_ike(ctx['id'], next=33, ike=ike)

        def build_ke(next=0):
            ke = struct.pack(">BBHHH", next, 0, 4 + 4 + 192, 5, 0)
            ke += 191*b'\x00'+b'\x02'
            return ke

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Valid proposal and KEi, but no Ni in SAi1")
            ike = build_sa(next=34)
            ike += build_ke()
            return build_ike(ctx['id'], next=33, ike=ike)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Too short Ni in SAi1")
            ike = build_sa(next=34)
            ike += build_ke(next=40)
            ike += struct.pack(">BBH", 0, 0, 4)
            return build_ike(ctx['id'], next=33, ike=ike)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Too long Ni in SAi1")
            ike = build_sa(next=34)
            ike += build_ke(next=40)
            ike += struct.pack(">BBH", 0, 0, 4 + 257) + 257*b'\x00'
            return build_ike(ctx['id'], next=33, ike=ike)

        def build_ni(next=0):
            return struct.pack(">BBH", next, 0, 4 + 256) + 256*b'\x00'

        def build_sai1(id):
            ike = build_sa(next=34)
            ike += build_ke(next=40)
            ike += build_ni()
            return build_ike(ctx['id'], next=33, ike=ike)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Valid proposal, KEi, and Ni in SAi1")
            return build_sai1(ctx['id'])
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Valid proposal, KEi, and Ni in SAi1")
            return build_sai1(ctx['id'])
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: No integrity checksum")
            ike = b''
            return build_ike(ctx['id'], next=37, ike=ike)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Valid proposal, KEi, and Ni in SAi1")
            return build_sai1(ctx['id'])
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Truncated integrity checksum")
            return struct.pack(">BBHBB",
                               EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1,
                               EAP_TYPE_IKEV2, 0x20)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Valid proposal, KEi, and Ni in SAi1")
            return build_sai1(ctx['id'])
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Invalid integrity checksum")
            ike = b''
            return build_ike(ctx['id'], next=37, flags=0x20, ike=ike)

        idx += 1
        if ctx['num'] == idx:
            logger.info("No more test responses available - test case completed")
            global eap_proto_ikev2_test_done
            eap_proto_ikev2_test_done = True
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1,
                               EAP_TYPE_IKEV2)
        return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

    srv = start_radius_server(ikev2_handler)

    try:
        hapd = start_ap(apdev[0])
        dev[0].scan_for_bss(hapd.own_addr(), freq=2412)

        i = 0
        while not eap_proto_ikev2_test_done:
            i += 1
            logger.info("Running connection iteration %d" % i)
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="IKEV2", identity="user",
                           password="password",
                           wait_connect=False)
            ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED"], timeout=15)
            if ev is None:
                raise Exception("Timeout on EAP start")
            ev = dev[0].wait_event(["CTRL-EVENT-EAP-PROPOSED-METHOD"],
                                   timeout=15)
            if ev is None:
                raise Exception("Timeout on EAP method start")
            if i in [41, 46]:
                ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"],
                                       timeout=10)
                if ev is None:
                    raise Exception("Timeout on EAP failure")
            else:
                time.sleep(0.05)
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()
            dev[0].dump_monitor()
            dev[1].dump_monitor()
            dev[2].dump_monitor()
    finally:
        stop_radius_server(srv)

def NtPasswordHash(password):
    pw = password.encode('utf_16_le')
    return hashlib.new('md4', pw).digest()

def HashNtPasswordHash(password_hash):
    return hashlib.new('md4', password_hash).digest()

def ChallengeHash(peer_challenge, auth_challenge, username):
    data = peer_challenge + auth_challenge + username
    return hashlib.sha1(data).digest()[0:8]

def GenerateAuthenticatorResponse(password, nt_response, peer_challenge,
                                  auth_challenge, username):
    magic1 = binascii.unhexlify("4D616769632073657276657220746F20636C69656E74207369676E696E6720636F6E7374616E74")
    magic2 = binascii.unhexlify("50616420746F206D616B6520697420646F206D6F7265207468616E206F6E6520697465726174696F6E")

    password_hash = NtPasswordHash(password)
    password_hash_hash = HashNtPasswordHash(password_hash)
    data = password_hash_hash + nt_response + magic1
    digest = hashlib.sha1(data).digest()

    challenge = ChallengeHash(peer_challenge, auth_challenge, username.encode())

    data = digest + challenge + magic2
    resp = hashlib.sha1(data).digest()
    return resp

def test_eap_proto_ikev2_errors(dev, apdev):
    """EAP-IKEv2 local error cases"""
    check_eap_capa(dev[0], "IKEV2")
    params = hostapd.wpa2_eap_params(ssid="eap-test")
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].scan_for_bss(hapd.own_addr(), freq=2412)

    for i in range(1, 5):
        with alloc_fail(dev[0], i, "eap_ikev2_init"):
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="IKEV2", identity="ikev2 user",
                           password="ike password",
                           wait_connect=False)
            ev = dev[0].wait_event(["EAP: Failed to initialize EAP method"],
                                   timeout=15)
            if ev is None:
                raise Exception("Timeout on EAP start")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

    tests = [(1, "ikev2_encr_encrypt"),
             (1, "ikev2_encr_decrypt"),
             (1, "ikev2_derive_auth_data"),
             (2, "ikev2_derive_auth_data"),
             (1, "=ikev2_decrypt_payload"),
             (1, "ikev2_encr_decrypt;ikev2_decrypt_payload"),
             (1, "ikev2_encr_encrypt;ikev2_build_encrypted"),
             (1, "ikev2_derive_sk_keys"),
             (2, "ikev2_derive_sk_keys"),
             (3, "ikev2_derive_sk_keys"),
             (4, "ikev2_derive_sk_keys"),
             (5, "ikev2_derive_sk_keys"),
             (6, "ikev2_derive_sk_keys"),
             (7, "ikev2_derive_sk_keys"),
             (8, "ikev2_derive_sk_keys"),
             (1, "eap_ikev2_derive_keymat;eap_ikev2_peer_keymat"),
             (1, "eap_msg_alloc;eap_ikev2_build_msg"),
             (1, "eap_ikev2_getKey"),
             (1, "eap_ikev2_get_emsk"),
             (1, "eap_ikev2_get_session_id"),
             (1, "=ikev2_derive_keys"),
             (2, "=ikev2_derive_keys"),
             (1, "wpabuf_alloc;ikev2_process_kei"),
             (1, "=ikev2_process_idi"),
             (1, "ikev2_derive_auth_data;ikev2_build_auth"),
             (1, "wpabuf_alloc;ikev2_build_sa_init"),
             (2, "wpabuf_alloc;ikev2_build_sa_init"),
             (3, "wpabuf_alloc;ikev2_build_sa_init"),
             (4, "wpabuf_alloc;ikev2_build_sa_init"),
             (5, "wpabuf_alloc;ikev2_build_sa_init"),
             (6, "wpabuf_alloc;ikev2_build_sa_init"),
             (1, "wpabuf_alloc;ikev2_build_sa_auth"),
             (2, "wpabuf_alloc;ikev2_build_sa_auth"),
             (1, "ikev2_build_auth;ikev2_build_sa_auth")]
    for count, func in tests:
        with alloc_fail(dev[0], count, func):
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="IKEV2", identity="ikev2 user@domain",
                           password="ike password", erp="1", wait_connect=False)
            ev = dev[0].wait_event(["CTRL-EVENT-EAP-PROPOSED-METHOD"],
                                   timeout=15)
            if ev is None:
                raise Exception("Timeout on EAP start")
            ok = False
            for j in range(10):
                state = dev[0].request('GET_ALLOC_FAIL')
                if state.startswith('0:'):
                    ok = True
                    break
                time.sleep(0.1)
            if not ok:
                raise Exception("No allocation failure seen for %d:%s" % (count, func))
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

    tests = [(1, "wpabuf_alloc;ikev2_build_notify"),
             (2, "wpabuf_alloc;ikev2_build_notify"),
             (1, "ikev2_build_encrypted;ikev2_build_notify")]
    for count, func in tests:
        with alloc_fail(dev[0], count, func):
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="IKEV2", identity="ikev2 user",
                           password="wrong password", erp="1",
                           wait_connect=False)
            ev = dev[0].wait_event(["CTRL-EVENT-EAP-PROPOSED-METHOD"],
                                   timeout=15)
            if ev is None:
                raise Exception("Timeout on EAP start")
            ok = False
            for j in range(10):
                state = dev[0].request('GET_ALLOC_FAIL')
                if state.startswith('0:'):
                    ok = True
                    break
                time.sleep(0.1)
            if not ok:
                raise Exception("No allocation failure seen for %d:%s" % (count, func))
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

    tests = [(1, "ikev2_integ_hash"),
             (1, "ikev2_integ_hash;ikev2_decrypt_payload"),
             (1, "os_get_random;ikev2_build_encrypted"),
             (1, "ikev2_prf_plus;ikev2_derive_sk_keys"),
             (1, "eap_ikev2_derive_keymat;eap_ikev2_peer_keymat"),
             (1, "os_get_random;ikev2_build_sa_init"),
             (2, "os_get_random;ikev2_build_sa_init"),
             (1, "ikev2_integ_hash;eap_ikev2_validate_icv"),
             (1, "hmac_sha1_vector;?ikev2_prf_hash;ikev2_derive_keys"),
             (1, "hmac_sha1_vector;?ikev2_prf_hash;ikev2_derive_auth_data"),
             (2, "hmac_sha1_vector;?ikev2_prf_hash;ikev2_derive_auth_data"),
             (3, "hmac_sha1_vector;?ikev2_prf_hash;ikev2_derive_auth_data")]
    for count, func in tests:
        with fail_test(dev[0], count, func):
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="IKEV2", identity="ikev2 user",
                           password="ike password", wait_connect=False)
            ev = dev[0].wait_event(["CTRL-EVENT-EAP-PROPOSED-METHOD"],
                                   timeout=15)
            if ev is None:
                raise Exception("Timeout on EAP start")
            ok = False
            for j in range(10):
                state = dev[0].request('GET_FAIL')
                if state.startswith('0:'):
                    ok = True
                    break
                time.sleep(0.1)
            if not ok:
                raise Exception("No failure seen for %d:%s" % (count, func))
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

    params = {"ssid": "eap-test2", "wpa": "2", "wpa_key_mgmt": "WPA-EAP",
              "rsn_pairwise": "CCMP", "ieee8021x": "1",
              "eap_server": "1", "eap_user_file": "auth_serv/eap_user.conf",
              "fragment_size": "50"}
    hapd2 = hostapd.add_ap(apdev[1], params)
    dev[0].scan_for_bss(hapd2.own_addr(), freq=2412)

    tests = [(1, "eap_ikev2_build_frag_ack"),
             (1, "wpabuf_alloc;eap_ikev2_process_fragment")]
    for count, func in tests:
        with alloc_fail(dev[0], count, func):
            dev[0].connect("eap-test2", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="IKEV2", identity="ikev2 user",
                           password="ike password", erp="1", wait_connect=False)
            ev = dev[0].wait_event(["CTRL-EVENT-EAP-PROPOSED-METHOD"],
                                   timeout=15)
            if ev is None:
                raise Exception("Timeout on EAP start")
            ok = False
            for j in range(10):
                state = dev[0].request('GET_ALLOC_FAIL')
                if state.startswith('0:'):
                    ok = True
                    break
                time.sleep(0.1)
            if not ok:
                raise Exception("No allocation failure seen for %d:%s" % (count, func))
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

def run_eap_ikev2_connect(dev):
    dev.connect("test-wpa2-eap", key_mgmt="WPA-EAP", scan_freq="2412",
                eap="IKEV2", identity="ikev2 user",
                password="ike password",
                fragment_size="30", wait_connect=False)
    ev = dev.wait_event(["CTRL-EVENT-EAP-SUCCESS", "CTRL-EVENT-EAP-FAILURE",
                         "CTRL-EVENT-DISCONNECTED"],
                        timeout=1)
    dev.request("REMOVE_NETWORK all")
    if not ev or "CTRL-EVENT-DISCONNECTED" not in ev:
        dev.wait_disconnected()
    dev.dump_monitor()

def test_eap_proto_ikev2_errors_server(dev, apdev):
    """EAP-IKEV2 local error cases on server"""
    check_eap_capa(dev[0], "IKEV2")
    params = int_eap_server_params()
    params['erp_domain'] = 'example.com'
    params['eap_server_erp'] = '1'
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].scan_for_bss(hapd.own_addr(), freq=2412)

    tests = [(1, "eap_ikev2_init"),
             (2, "=eap_ikev2_init"),
             (3, "=eap_ikev2_init"),
             (1, "eap_msg_alloc;eap_ikev2_build_msg"),
             (1, "ikev2_initiator_build;eap_ikev2_buildReq"),
             (1, "eap_ikev2_process_fragment"),
             (1, "wpabuf_alloc_copy;ikev2_process_ker"),
             (1, "ikev2_process_idr"),
             (1, "ikev2_derive_auth_data;ikev2_process_auth_secret"),
             (1, "ikev2_decrypt_payload;ikev2_process_sa_auth"),
             (1, "ikev2_process_sa_auth_decrypted;ikev2_process_sa_auth"),
             (1, "dh_init;ikev2_build_kei"),
             (1, "ikev2_build_auth"),
             (1, "wpabuf_alloc;ikev2_build_sa_init"),
             (1, "ikev2_build_sa_auth"),
             (1, "=ikev2_build_sa_auth"),
             (2, "=ikev2_derive_auth_data"),
             (1, "wpabuf_alloc;ikev2_build_sa_auth"),
             (2, "wpabuf_alloc;=ikev2_build_sa_auth"),
             (1, "ikev2_decrypt_payload;ikev2_process_sa_init_encr"),
             (1, "dh_derive_shared;ikev2_derive_keys"),
             (1, "=ikev2_derive_keys"),
             (2, "=ikev2_derive_keys"),
             (1, "eap_ikev2_getKey"),
             (1, "eap_ikev2_get_emsk"),
             (1, "eap_ikev2_get_session_id")]
    for count, func in tests:
        with alloc_fail(hapd, count, func):
            run_eap_ikev2_connect(dev[0])

    tests = [(1, "eap_ikev2_validate_icv;eap_ikev2_process_icv"),
             (1, "eap_ikev2_server_keymat"),
             (1, "ikev2_build_auth"),
             (1, "os_get_random;ikev2_build_sa_init"),
             (2, "os_get_random;ikev2_build_sa_init"),
             (1, "ikev2_derive_keys"),
             (2, "ikev2_derive_keys"),
             (3, "ikev2_derive_keys"),
             (4, "ikev2_derive_keys"),
             (5, "ikev2_derive_keys"),
             (6, "ikev2_derive_keys"),
             (7, "ikev2_derive_keys"),
             (8, "ikev2_derive_keys"),
             (1, "ikev2_decrypt_payload;ikev2_process_sa_auth"),
             (1, "eap_ikev2_process_icv;eap_ikev2_process")]
    for count, func in tests:
        with fail_test(hapd, count, func):
            run_eap_ikev2_connect(dev[0])

def start_ikev2_assoc(dev, hapd):
    dev.connect("test-wpa2-eap", key_mgmt="WPA-EAP", scan_freq="2412",
                eap="IKEV2", identity="ikev2 user",
                password="ike password", wait_connect=False)
    proxy_msg(hapd, dev) # EAP-Identity/Request
    proxy_msg(dev, hapd) # EAP-Identity/Response
    proxy_msg(hapd, dev) # IKEV2 1

def stop_ikev2_assoc(dev, hapd):
    dev.request("REMOVE_NETWORK all")
    dev.wait_disconnected()
    dev.dump_monitor()
    hapd.dump_monitor()

def test_eap_proto_ikev2_server(dev, apdev):
    """EAP-IKEV2 protocol testing for the server"""
    check_eap_capa(dev[0], "IKEV2")
    params = int_eap_server_params()
    params['erp_domain'] = 'example.com'
    params['eap_server_erp'] = '1'
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].scan_for_bss(hapd.own_addr(), freq=2412)
    hapd.request("SET ext_eapol_frame_io 1")
    dev[0].request("SET ext_eapol_frame_io 1")

    # Successful exchange to verify proxying mechanism
    start_ikev2_assoc(dev[0], hapd)
    proxy_msg(dev[0], hapd) # IKEV2 2
    proxy_msg(hapd, dev[0]) # IKEV2 3
    proxy_msg(dev[0], hapd) # IKEV2 4
    proxy_msg(hapd, dev[0]) # EAP-Success
    proxy_msg(hapd, dev[0]) # EAPOL-Key msg 1/4
    proxy_msg(dev[0], hapd) # EAPOL-Key msg 2/4
    proxy_msg(hapd, dev[0]) # EAPOL-Key msg 3/4
    proxy_msg(dev[0], hapd) # EAPOL-Key msg 4/4
    dev[0].wait_connected()
    stop_ikev2_assoc(dev[0], hapd)

    start_ikev2_assoc(dev[0], hapd)
    resp = rx_msg(dev[0])
    # Too short EAP-IKEV2 header
    hapd.note("IKEV2: Too short frame to include HDR")
    msg = resp[0:4] + "0005" + resp[8:12] + "0005" + "31"
    tx_msg(dev[0], hapd, msg)
    rx_msg(hapd)
    stop_ikev2_assoc(dev[0], hapd)

    start_ikev2_assoc(dev[0], hapd)
    resp = rx_msg(dev[0])
    # Too short EAP-IKEV2 header - missing Message Length field
    hapd.note("EAP-IKEV2: Message underflow")
    msg = resp[0:4] + "0006" + resp[8:12] + "0006" + "3180"
    tx_msg(dev[0], hapd, msg)
    rx_msg(hapd)
    stop_ikev2_assoc(dev[0], hapd)

    start_ikev2_assoc(dev[0], hapd)
    resp = rx_msg(dev[0])
    # Too short EAP-IKEV2 header - too small Message Length
    hapd.note("EAP-IKEV2: Invalid Message Length (0; 1 remaining in this msg)")
    msg = resp[0:4] + "000b" + resp[8:12] + "000b" + "318000000000ff"
    tx_msg(dev[0], hapd, msg)
    rx_msg(hapd)
    stop_ikev2_assoc(dev[0], hapd)

    start_ikev2_assoc(dev[0], hapd)
    resp = rx_msg(dev[0])
    # Too short EAP-IKEV2 header - too large Message Length
    hapd.note("EAP-IKEV2: Ignore too long message")
    msg = resp[0:4] + "000b" + resp[8:12] + "000b" + "31c0bbccddeeff"
    tx_msg(dev[0], hapd, msg)
    rx_msg(hapd)
    stop_ikev2_assoc(dev[0], hapd)

    start_ikev2_assoc(dev[0], hapd)
    resp = rx_msg(dev[0])
    # No Message Length in first fragment
    hapd.note("EAP-IKEV2: No Message Length field in a fragmented packet")
    msg = resp[0:4] + "0007" + resp[8:12] + "0007" + "3140ff"
    tx_msg(dev[0], hapd, msg)
    rx_msg(hapd)
    stop_ikev2_assoc(dev[0], hapd)

    start_ikev2_assoc(dev[0], hapd)
    resp = rx_msg(dev[0])
    # First fragment (valid)
    hapd.note("EAP-IKEV2: Received 1 bytes in first fragment, waiting for 255 bytes more")
    msg = resp[0:4] + "000b" + resp[8:12] + "000b" + "31c000000100ff"
    tx_msg(dev[0], hapd, msg)
    req = rx_msg(hapd)
    id, = struct.unpack('B', binascii.unhexlify(req)[5:6])
    hapd.note("EAP-IKEV2: Received 1 bytes in first fragment, waiting for 254 bytes more")
    payload = struct.pack('BBB', 49, 0x40, 0)
    msg = struct.pack('>BBHBBH', 1, 0, 4 + len(payload), 2, id, 4 + len(payload)) + payload
    tx_msg(dev[0], hapd, binascii.hexlify(msg).decode())
    req = rx_msg(hapd)
    id, = struct.unpack('B', binascii.unhexlify(req)[5:6])
    hapd.note("EAP-IKEV2: Fragment overflow")
    payload = struct.pack('BB', 49, 0x40) + 255*b'\x00'
    msg = struct.pack('>BBHBBH', 1, 0, 4 + len(payload), 2, id, 4 + len(payload)) + payload
    tx_msg(dev[0], hapd, binascii.hexlify(msg).decode())
    rx_msg(hapd)
    stop_ikev2_assoc(dev[0], hapd)

    start_ikev2_assoc(dev[0], hapd)
    proxy_msg(dev[0], hapd) # IKEV2 2
    req = proxy_msg(hapd, dev[0]) # IKEV2 3
    id, = struct.unpack('B', binascii.unhexlify(req)[5:6])
    # Missing ICV
    hapd.note("EAP-IKEV2: The message should have included integrity checksum")
    payload = struct.pack('BB', 49, 0) + b'\x00'
    msg = struct.pack('>BBHBBH', 1, 0, 4 + len(payload), 2, id, 4 + len(payload)) + payload
    tx_msg(dev[0], hapd, binascii.hexlify(msg).decode())
    rx_msg(hapd)
    stop_ikev2_assoc(dev[0], hapd)

    tests = [("Unsupported HDR version 0x0 (expected 0x20)",
              struct.pack('BB', 49, 0) + 16*b'\x00' +
              struct.pack('>BBBBLL', 0, 0, 0, 0, 0, 0)),
             ("IKEV2: Invalid length (HDR: 0 != RX: 28)",
              struct.pack('BB', 49, 0) + 16*b'\x00' +
              struct.pack('>BBBBLL', 0, 0x20, 0, 0, 0, 0)),
             ("IKEV2: Unexpected Exchange Type 0 in SA_INIT state",
              struct.pack('BB', 49, 0) + 16*b'\x00' +
              struct.pack('>BBBBLL', 0, 0x20, 0, 0, 0, 28)),
             ("IKEV2: Unexpected Flags value 0x0",
              struct.pack('BB', 49, 0) + 16*b'\x00' +
              struct.pack('>BBBBLL', 0, 0x20, 34, 0, 0, 28)),
             ("IKEV2: SAr1 not received",
              struct.pack('BB', 49, 0) + 16*b'\x00' +
              struct.pack('>BBBBLL', 0, 0x20, 34, 0x20, 0, 28))]
    for txt, payload in tests:
        start_ikev2_assoc(dev[0], hapd)
        resp = rx_msg(dev[0])
        id, = struct.unpack('B', binascii.unhexlify(resp)[5:6])
        hapd.note(txt)
        msg = struct.pack('>BBHBBH', 1, 0, 4 + len(payload), 2, id, 4 + len(payload)) + payload
        tx_msg(dev[0], hapd, binascii.hexlify(msg).decode())
        rx_msg(hapd)
        stop_ikev2_assoc(dev[0], hapd)

def test_eap_proto_mschapv2(dev, apdev):
    """EAP-MSCHAPv2 protocol tests"""
    check_eap_capa(dev[0], "MSCHAPV2")

    def mschapv2_handler(ctx, req):
        logger.info("mschapv2_handler - RX " + binascii.hexlify(req).decode())
        if 'num' not in ctx:
            ctx['num'] = 0
        ctx['num'] = ctx['num'] + 1
        if 'id' not in ctx:
            ctx['id'] = 1
        ctx['id'] = (ctx['id'] + 1) % 256
        idx = 0

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Missing payload")
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1,
                               EAP_TYPE_MSCHAPV2)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unknown MSCHAPv2 op_code")
            return struct.pack(">BBHBBBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 4 + 1,
                               EAP_TYPE_MSCHAPV2,
                               0, 0, 5, 0)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Invalid ms_len and unknown MSCHAPv2 op_code")
            return struct.pack(">BBHBBBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 4 + 1,
                               EAP_TYPE_MSCHAPV2,
                               255, 0, 0, 0)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Success before challenge")
            return struct.pack(">BBHBBBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 4 + 1,
                               EAP_TYPE_MSCHAPV2,
                               3, 0, 5, 0)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Failure before challenge - required challenge field not present")
            return struct.pack(">BBHBBBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 4 + 1,
                               EAP_TYPE_MSCHAPV2,
                               4, 0, 5, 0)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Failure before challenge - invalid failure challenge len")
            payload = b'C=12'
            return struct.pack(">BBHBBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 4 + len(payload),
                               EAP_TYPE_MSCHAPV2,
                               4, 0, 4 + len(payload)) + payload
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Failure before challenge - invalid failure challenge len")
            payload = b'C=12 V=3'
            return struct.pack(">BBHBBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 4 + len(payload),
                               EAP_TYPE_MSCHAPV2,
                               4, 0, 4 + len(payload)) + payload
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Failure before challenge - invalid failure challenge")
            payload = b'C=00112233445566778899aabbccddeefQ '
            return struct.pack(">BBHBBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 4 + len(payload),
                               EAP_TYPE_MSCHAPV2,
                               4, 0, 4 + len(payload)) + payload
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Failure before challenge - password expired")
            payload = b'E=648 R=1 C=00112233445566778899aabbccddeeff V=3 M=Password expired'
            return struct.pack(">BBHBBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 4 + len(payload),
                               EAP_TYPE_MSCHAPV2,
                               4, 0, 4 + len(payload)) + payload
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Success after password change")
            payload = b"S=1122334455667788990011223344556677889900"
            return struct.pack(">BBHBBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 4 + len(payload),
                               EAP_TYPE_MSCHAPV2,
                               3, 0, 4 + len(payload)) + payload

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Invalid challenge length")
            return struct.pack(">BBHBBBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 4 + 1,
                               EAP_TYPE_MSCHAPV2,
                               1, 0, 4 + 1, 0)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Too short challenge packet")
            return struct.pack(">BBHBBBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 4 + 1,
                               EAP_TYPE_MSCHAPV2,
                               1, 0, 4 + 1, 16)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Challenge")
            return struct.pack(">BBHBBBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 4 + 1 + 16 + 6,
                               EAP_TYPE_MSCHAPV2,
                               1, 0, 4 + 1 + 16 + 6, 16) + 16*b'A' + b'foobar'
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Failure - password expired")
            payload = b'E=648 R=1 C=00112233445566778899aabbccddeeff V=3 M=Password expired'
            return struct.pack(">BBHBBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 4 + len(payload),
                               EAP_TYPE_MSCHAPV2,
                               4, 0, 4 + len(payload)) + payload
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Success after password change")
            if len(req) != 591:
                logger.info("Unexpected Change-Password packet length: %s" % len(req))
                return None
            data = req[9:]
            enc_pw = data[0:516]
            data = data[516:]
            enc_hash = data[0:16]
            data = data[16:]
            peer_challenge = data[0:16]
            data = data[16:]
            # Reserved
            data = data[8:]
            nt_response = data[0:24]
            data = data[24:]
            flags = data
            logger.info("enc_hash: " + binascii.hexlify(enc_hash).decode())
            logger.info("peer_challenge: " + binascii.hexlify(peer_challenge).decode())
            logger.info("nt_response: " + binascii.hexlify(nt_response).decode())
            logger.info("flags: " + binascii.hexlify(flags).decode())

            auth_challenge = binascii.unhexlify("00112233445566778899aabbccddeeff")
            logger.info("auth_challenge: " + binascii.hexlify(auth_challenge).decode())

            auth_resp = GenerateAuthenticatorResponse("new-pw", nt_response,
                                                      peer_challenge,
                                                      auth_challenge, "user")
            payload = b"S=" + binascii.hexlify(auth_resp).decode().upper().encode()
            logger.info("Success message payload: " + payload.decode())
            return struct.pack(">BBHBBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 4 + len(payload),
                               EAP_TYPE_MSCHAPV2,
                               3, 0, 4 + len(payload)) + payload
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Success")
            return struct.pack(">BBH", EAP_CODE_SUCCESS, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Failure - password expired")
            payload = b'E=648 R=1 C=00112233445566778899aabbccddeeff V=3 M=Password expired'
            return struct.pack(">BBHBBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 4 + len(payload),
                               EAP_TYPE_MSCHAPV2,
                               4, 0, 4 + len(payload)) + payload
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Success after password change")
            if len(req) != 591:
                logger.info("Unexpected Change-Password packet length: %s" % len(req))
                return None
            data = req[9:]
            enc_pw = data[0:516]
            data = data[516:]
            enc_hash = data[0:16]
            data = data[16:]
            peer_challenge = data[0:16]
            data = data[16:]
            # Reserved
            data = data[8:]
            nt_response = data[0:24]
            data = data[24:]
            flags = data
            logger.info("enc_hash: " + binascii.hexlify(enc_hash).decode())
            logger.info("peer_challenge: " + binascii.hexlify(peer_challenge).decode())
            logger.info("nt_response: " + binascii.hexlify(nt_response).decode())
            logger.info("flags: " + binascii.hexlify(flags).decode())

            auth_challenge = binascii.unhexlify("00112233445566778899aabbccddeeff")
            logger.info("auth_challenge: " + binascii.hexlify(auth_challenge).decode())

            auth_resp = GenerateAuthenticatorResponse("new-pw", nt_response,
                                                      peer_challenge,
                                                      auth_challenge, "user")
            payload = b"S=" + binascii.hexlify(auth_resp).decode().upper().encode()
            logger.info("Success message payload: " + payload.decode())
            return struct.pack(">BBHBBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 4 + len(payload),
                               EAP_TYPE_MSCHAPV2,
                               3, 0, 4 + len(payload)) + payload
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Success")
            return struct.pack(">BBH", EAP_CODE_SUCCESS, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Challenge")
            return struct.pack(">BBHBBBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 4 + 1 + 16 + 6,
                               EAP_TYPE_MSCHAPV2,
                               1, 0, 4 + 1 + 16 + 6, 16) + 16*b'A' + b'foobar'
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Failure - authentication failure")
            payload = b'E=691 R=1 C=00112233445566778899aabbccddeeff V=3 M=Authentication failed'
            return struct.pack(">BBHBBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 4 + len(payload),
                               EAP_TYPE_MSCHAPV2,
                               4, 0, 4 + len(payload)) + payload

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Challenge")
            return struct.pack(">BBHBBBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 4 + 1 + 16 + 6,
                               EAP_TYPE_MSCHAPV2,
                               1, 0, 4 + 1 + 16 + 6, 16) + 16*b'A' + b'foobar'
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Failure - authentication failure")
            payload = b'E=691 R=1 C=00112233445566778899aabbccddeeff V=3 M=Authentication failed (2)'
            return struct.pack(">BBHBBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 4 + len(payload),
                               EAP_TYPE_MSCHAPV2,
                               4, 0, 4 + len(payload)) + payload
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Challenge - invalid ms_len and workaround disabled")
            return struct.pack(">BBHBBBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 4 + 1 + 16 + 6,
                               EAP_TYPE_MSCHAPV2,
                               1, 0, 4 + 1 + 16 + 6 + 1, 16) + 16*b'A' + b'foobar'

        return None

    srv = start_radius_server(mschapv2_handler)

    try:
        hapd = start_ap(apdev[0])
        dev[0].scan_for_bss(hapd.own_addr(), freq=2412)

        for i in range(0, 16):
            logger.info("RUN: %d" % i)
            if i == 12:
                dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                               eap="MSCHAPV2", identity="user",
                               password_hex="hash:8846f7eaee8fb117ad06bdd830b7586c",
                               wait_connect=False)
            elif i == 14:
                dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                               eap="MSCHAPV2", identity="user",
                               phase2="mschapv2_retry=0",
                               password="password", wait_connect=False)
            elif i == 15:
                dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                               eap="MSCHAPV2", identity="user",
                               eap_workaround="0",
                               password="password", wait_connect=False)
            else:
                dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                               eap="MSCHAPV2", identity="user",
                               password="password", wait_connect=False)
            ev = dev[0].wait_event(["CTRL-EVENT-EAP-PROPOSED-METHOD"], timeout=15)
            if ev is None:
                raise Exception("Timeout on EAP start")

            if i in [8, 11, 12]:
                ev = dev[0].wait_event(["CTRL-REQ-NEW_PASSWORD"],
                                       timeout=10)
                if ev is None:
                    raise Exception("Timeout on new password request")
                id = ev.split(':')[0].split('-')[-1]
                dev[0].request("CTRL-RSP-NEW_PASSWORD-" + id + ":new-pw")
                if i in [11, 12]:
                    ev = dev[0].wait_event(["CTRL-EVENT-PASSWORD-CHANGED"],
                                       timeout=10)
                    if ev is None:
                        raise Exception("Timeout on password change")
                    ev = dev[0].wait_event(["CTRL-EVENT-EAP-SUCCESS"],
                                       timeout=10)
                    if ev is None:
                        raise Exception("Timeout on EAP success")
                else:
                    ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"],
                                           timeout=10)
                    if ev is None:
                        raise Exception("Timeout on EAP failure")

            if i in [13]:
                ev = dev[0].wait_event(["CTRL-REQ-IDENTITY"],
                                       timeout=10)
                if ev is None:
                    raise Exception("Timeout on identity request")
                id = ev.split(':')[0].split('-')[-1]
                dev[0].request("CTRL-RSP-IDENTITY-" + id + ":user")

                ev = dev[0].wait_event(["CTRL-REQ-PASSWORD"],
                                       timeout=10)
                if ev is None:
                    raise Exception("Timeout on password request")
                id = ev.split(':')[0].split('-')[-1]
                dev[0].request("CTRL-RSP-PASSWORD-" + id + ":password")

                # TODO: Does this work correctly?

                ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"],
                                       timeout=10)
                if ev is None:
                    raise Exception("Timeout on EAP failure")

            if i in [4, 5, 6, 7, 14]:
                ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"],
                                       timeout=10)
                if ev is None:
                    raise Exception("Timeout on EAP failure")
            else:
                time.sleep(0.05)
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected(timeout=1)
    finally:
        stop_radius_server(srv)

def test_eap_proto_mschapv2_errors(dev, apdev):
    """EAP-MSCHAPv2 protocol tests (error paths)"""
    check_eap_capa(dev[0], "MSCHAPV2")

    def mschapv2_fail_password_expired(ctx):
        logger.info("Test: Failure before challenge - password expired")
        payload = b'E=648 R=1 C=00112233445566778899aabbccddeeff V=3 M=Password expired'
        return struct.pack(">BBHBBBH", EAP_CODE_REQUEST, ctx['id'],
                           4 + 1 + 4 + len(payload),
                           EAP_TYPE_MSCHAPV2,
                           4, 0, 4 + len(payload)) + payload

    def mschapv2_success_after_password_change(ctx, req=None):
        logger.info("Test: Success after password change")
        if req is None or len(req) != 591:
            payload = b"S=1122334455667788990011223344556677889900"
        else:
            data = req[9:]
            enc_pw = data[0:516]
            data = data[516:]
            enc_hash = data[0:16]
            data = data[16:]
            peer_challenge = data[0:16]
            data = data[16:]
            # Reserved
            data = data[8:]
            nt_response = data[0:24]
            data = data[24:]
            flags = data
            logger.info("enc_hash: " + binascii.hexlify(enc_hash).decode())
            logger.info("peer_challenge: " + binascii.hexlify(peer_challenge).decode())
            logger.info("nt_response: " + binascii.hexlify(nt_response).decode())
            logger.info("flags: " + binascii.hexlify(flags).decode())

            auth_challenge = binascii.unhexlify("00112233445566778899aabbccddeeff")
            logger.info("auth_challenge: " + binascii.hexlify(auth_challenge).decode())

            auth_resp = GenerateAuthenticatorResponse("new-pw", nt_response,
                                                      peer_challenge,
                                                      auth_challenge, "user")
            payload = b"S=" + binascii.hexlify(auth_resp).decode().upper().encode()
        return struct.pack(">BBHBBBH", EAP_CODE_REQUEST, ctx['id'],
                           4 + 1 + 4 + len(payload),
                           EAP_TYPE_MSCHAPV2,
                           3, 0, 4 + len(payload)) + payload

    def mschapv2_handler(ctx, req):
        logger.info("mschapv2_handler - RX " + binascii.hexlify(req).decode())
        if 'num' not in ctx:
            ctx['num'] = 0
        ctx['num'] = ctx['num'] + 1
        if 'id' not in ctx:
            ctx['id'] = 1
        ctx['id'] = (ctx['id'] + 1) % 256
        idx = 0

        idx += 1
        if ctx['num'] == idx:
            return mschapv2_fail_password_expired(ctx)
        idx += 1
        if ctx['num'] == idx:
            return mschapv2_success_after_password_change(ctx, req)
        idx += 1
        if ctx['num'] == idx:
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            return mschapv2_fail_password_expired(ctx)
        idx += 1
        if ctx['num'] == idx:
            return mschapv2_success_after_password_change(ctx, req)
        idx += 1
        if ctx['num'] == idx:
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            return mschapv2_fail_password_expired(ctx)
        idx += 1
        if ctx['num'] == idx:
            return mschapv2_success_after_password_change(ctx, req)
        idx += 1
        if ctx['num'] == idx:
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            return mschapv2_fail_password_expired(ctx)
        idx += 1
        if ctx['num'] == idx:
            return mschapv2_success_after_password_change(ctx, req)
        idx += 1
        if ctx['num'] == idx:
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            return mschapv2_fail_password_expired(ctx)
        idx += 1
        if ctx['num'] == idx:
            return mschapv2_success_after_password_change(ctx, req)
        idx += 1
        if ctx['num'] == idx:
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            return mschapv2_fail_password_expired(ctx)
        idx += 1
        if ctx['num'] == idx:
            return mschapv2_success_after_password_change(ctx, req)
        idx += 1
        if ctx['num'] == idx:
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            return mschapv2_fail_password_expired(ctx)
        idx += 1
        if ctx['num'] == idx:
            return mschapv2_success_after_password_change(ctx, req)
        idx += 1
        if ctx['num'] == idx:
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            return mschapv2_fail_password_expired(ctx)
        idx += 1
        if ctx['num'] == idx:
            return mschapv2_success_after_password_change(ctx, req)
        idx += 1
        if ctx['num'] == idx:
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            return mschapv2_fail_password_expired(ctx)
        idx += 1
        if ctx['num'] == idx:
            return mschapv2_success_after_password_change(ctx, req)
        idx += 1
        if ctx['num'] == idx:
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        return None

    srv = start_radius_server(mschapv2_handler)

    try:
        hapd = start_ap(apdev[0])
        dev[0].scan_for_bss(hapd.own_addr(), freq=2412)

        tests = ["os_get_random;eap_mschapv2_change_password",
                 "generate_nt_response;eap_mschapv2_change_password",
                 "get_master_key;eap_mschapv2_change_password",
                 "nt_password_hash;eap_mschapv2_change_password",
                 "old_nt_password_hash_encrypted_with_new_nt_password_hash"]
        for func in tests:
            with fail_test(dev[0], 1, func):
                dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                               eap="MSCHAPV2", identity="user",
                               password="password", wait_connect=False)
                ev = dev[0].wait_event(["CTRL-REQ-NEW_PASSWORD"], timeout=10)
                if ev is None:
                    raise Exception("Timeout on new password request")
                id = ev.split(':')[0].split('-')[-1]
                dev[0].request("CTRL-RSP-NEW_PASSWORD-" + id + ":new-pw")
                time.sleep(0.1)
                wait_fail_trigger(dev[0], "GET_FAIL")
                dev[0].request("REMOVE_NETWORK all")
                dev[0].wait_disconnected(timeout=1)

        tests = ["encrypt_pw_block_with_password_hash;eap_mschapv2_change_password",
                 "nt_password_hash;eap_mschapv2_change_password",
                 "nt_password_hash;eap_mschapv2_success"]
        for func in tests:
            with fail_test(dev[0], 1, func):
                dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                               eap="MSCHAPV2", identity="user",
                               password_hex="hash:8846f7eaee8fb117ad06bdd830b7586c",
                               wait_connect=False)
                ev = dev[0].wait_event(["CTRL-REQ-NEW_PASSWORD"], timeout=10)
                if ev is None:
                    raise Exception("Timeout on new password request")
                id = ev.split(':')[0].split('-')[-1]
                dev[0].request("CTRL-RSP-NEW_PASSWORD-" + id + ":new-pw")
                time.sleep(0.1)
                wait_fail_trigger(dev[0], "GET_FAIL")
                dev[0].request("REMOVE_NETWORK all")
                dev[0].wait_disconnected(timeout=1)

        tests = ["eap_msg_alloc;eap_mschapv2_change_password"]
        for func in tests:
            with alloc_fail(dev[0], 1, func):
                dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                               eap="MSCHAPV2", identity="user",
                               password="password", wait_connect=False)
                ev = dev[0].wait_event(["CTRL-REQ-NEW_PASSWORD"], timeout=10)
                if ev is None:
                    raise Exception("Timeout on new password request")
                id = ev.split(':')[0].split('-')[-1]
                dev[0].request("CTRL-RSP-NEW_PASSWORD-" + id + ":new-pw")
                time.sleep(0.1)
                wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
                dev[0].request("REMOVE_NETWORK all")
                dev[0].wait_disconnected(timeout=1)
    finally:
        stop_radius_server(srv)

def test_eap_proto_pwd(dev, apdev):
    """EAP-pwd protocol tests"""
    check_eap_capa(dev[0], "PWD")

    global eap_proto_pwd_test_done, eap_proto_pwd_test_wait
    eap_proto_pwd_test_done = False
    eap_proto_pwd_test_wait = False

    def pwd_handler(ctx, req):
        logger.info("pwd_handler - RX " + binascii.hexlify(req).decode())
        if 'num' not in ctx:
            ctx['num'] = 0
        ctx['num'] = ctx['num'] + 1
        if 'id' not in ctx:
            ctx['id'] = 1
        ctx['id'] = (ctx['id'] + 1) % 256
        idx = 0

        global eap_proto_pwd_test_wait
        eap_proto_pwd_test_wait = False

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Missing payload")
            # EAP-pwd: Got a frame but pos is not NULL and len is 0
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'], 4 + 1,
                               EAP_TYPE_PWD)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Missing Total-Length field")
            # EAP-pwd: Frame too short to contain Total-Length field
            payload = struct.pack("B", 0x80)
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + len(payload), EAP_TYPE_PWD) + payload

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Too large Total-Length")
            # EAP-pwd: Incoming fragments whose total length = 65535
            payload = struct.pack(">BH", 0x80, 65535)
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + len(payload), EAP_TYPE_PWD) + payload

        idx += 1
        if ctx['num'] == idx:
            eap_proto_pwd_test_wait = True
            logger.info("Test: First fragment")
            # EAP-pwd: Incoming fragments whose total length = 10
            # EAP-pwd: ACKing a 0 byte fragment
            payload = struct.pack(">BH", 0xc0, 10)
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + len(payload), EAP_TYPE_PWD) + payload
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unexpected Total-Length value in the second fragment")
            # EAP-pwd: Incoming fragments whose total length = 0
            # EAP-pwd: Unexpected new fragment start when previous fragment is still in use
            payload = struct.pack(">BH", 0x80, 0)
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + len(payload), EAP_TYPE_PWD) + payload

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: First and only fragment")
            # EAP-pwd: Incoming fragments whose total length = 0
            # EAP-pwd: processing frame: exch 0, len 0
            # EAP-pwd: Ignoring message with unknown opcode 128
            payload = struct.pack(">BH", 0x80, 0)
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + len(payload), EAP_TYPE_PWD) + payload

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: First and only fragment with extra data")
            # EAP-pwd: Incoming fragments whose total length = 0
            # EAP-pwd: processing frame: exch 0, len 1
            # EAP-pwd: Ignoring message with unknown opcode 128
            payload = struct.pack(">BHB", 0x80, 0, 0)
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + len(payload), EAP_TYPE_PWD) + payload

        idx += 1
        if ctx['num'] == idx:
            eap_proto_pwd_test_wait = True
            logger.info("Test: First fragment")
            # EAP-pwd: Incoming fragments whose total length = 2
            # EAP-pwd: ACKing a 1 byte fragment
            payload = struct.pack(">BHB", 0xc0, 2, 1)
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + len(payload), EAP_TYPE_PWD) + payload
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Extra data in the second fragment")
            # EAP-pwd: Buffer overflow attack detected (3 vs. 1)!
            payload = struct.pack(">BBB", 0x0, 2, 3)
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + len(payload), EAP_TYPE_PWD) + payload

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Too short id exchange")
            # EAP-pwd: processing frame: exch 1, len 0
            # EAP-PWD: PWD-ID-Req -> FAILURE
            payload = struct.pack(">B", 0x01)
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + len(payload), EAP_TYPE_PWD) + payload

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unsupported rand func in id exchange")
            # EAP-PWD: Server EAP-pwd-ID proposal: group=0 random=0 prf=0 prep=0
            # EAP-PWD: PWD-ID-Req -> FAILURE
            payload = struct.pack(">BHBBLB", 0x01, 0, 0, 0, 0, 0)
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + len(payload), EAP_TYPE_PWD) + payload

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unsupported prf in id exchange")
            # EAP-PWD: Server EAP-pwd-ID proposal: group=19 random=1 prf=0 prep=0
            # EAP-PWD: PWD-ID-Req -> FAILURE
            payload = struct.pack(">BHBBLB", 0x01, 19, 1, 0, 0, 0)
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + len(payload), EAP_TYPE_PWD) + payload

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unsupported password pre-processing technique in id exchange")
            # EAP-PWD: Server EAP-pwd-ID proposal: group=19 random=1 prf=1 prep=255
            # EAP-PWD: Unsupported password pre-processing technique (Prep=255)
            # EAP-PWD: PWD-ID-Req -> FAILURE
            payload = struct.pack(">BHBBLB", 0x01, 19, 1, 1, 0, 255)
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + len(payload), EAP_TYPE_PWD) + payload

        idx += 1
        if ctx['num'] == idx:
            eap_proto_pwd_test_wait = True
            logger.info("Test: Valid id exchange")
            # EAP-PWD: Server EAP-pwd-ID proposal: group=19 random=1 prf=1 prep=0
            payload = struct.pack(">BHBBLB", 0x01, 19, 1, 1, 0, 0)
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + len(payload), EAP_TYPE_PWD) + payload
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unexpected id exchange")
            # EAP-pwd: processing frame: exch 1, len 9
            # EAP-PWD: PWD-Commit-Req -> FAILURE
            payload = struct.pack(">BHBBLB", 0x01, 19, 1, 1, 0, 0)
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + len(payload), EAP_TYPE_PWD) + payload

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unexpected commit exchange")
            # EAP-pwd: processing frame: exch 2, len 0
            # EAP-PWD: PWD-ID-Req -> FAILURE
            payload = struct.pack(">B", 0x02)
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + len(payload), EAP_TYPE_PWD) + payload

        idx += 1
        if ctx['num'] == idx:
            eap_proto_pwd_test_wait = True
            logger.info("Test: Valid id exchange")
            # EAP-PWD: Server EAP-pwd-ID proposal: group=19 random=1 prf=1 prep=0
            payload = struct.pack(">BHBBLB", 0x01, 19, 1, 1, 0, 0)
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + len(payload), EAP_TYPE_PWD) + payload
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unexpected Commit payload length (prep=None)")
            # EAP-pwd commit request, password prep is NONE
            # EAP-pwd: Unexpected Commit payload length 0 (expected 96)
            payload = struct.pack(">B", 0x02)
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + len(payload), EAP_TYPE_PWD) + payload

        idx += 1
        if ctx['num'] == idx:
            eap_proto_pwd_test_wait = True
            logger.info("Test: Valid id exchange")
            # EAP-PWD: Server EAP-pwd-ID proposal: group=19 random=1 prf=1 prep=0
            payload = struct.pack(">BHBBLB", 0x01, 19, 1, 1, 0, 0)
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + len(payload), EAP_TYPE_PWD) + payload
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Commit payload with all zeros values --> Shared key at infinity")
            # EAP-pwd: Invalid coordinate in element
            payload = struct.pack(">B", 0x02) + 96*b'\0'
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + len(payload), EAP_TYPE_PWD) + payload

        idx += 1
        if ctx['num'] == idx:
            eap_proto_pwd_test_wait = True
            logger.info("Test: Valid id exchange")
            # EAP-PWD: Server EAP-pwd-ID proposal: group=19 random=1 prf=1 prep=0
            payload = struct.pack(">BHBBLB", 0x01, 19, 1, 1, 0, 0)
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + len(payload), EAP_TYPE_PWD) + payload
        idx += 1
        if ctx['num'] == idx:
            eap_proto_pwd_test_wait = True
            logger.info("Test: Commit payload with valid values")
            # EAP-pwd commit request, password prep is NONE
            element = binascii.unhexlify("8dcab2862c5396839a6bac0c689ff03d962863108e7c275bbf1d6eedf634ee832a214db99f0d0a1a6317733eecdd97f0fc4cda19f57e1bb9bb9c8dcf8c60ba6f")
            scalar = binascii.unhexlify("450f31e058cf2ac2636a5d6e2b3c70b1fcc301957f0716e77f13aa69f9a2e5bd")
            payload = struct.pack(">B", 0x02) + element + scalar
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + len(payload), EAP_TYPE_PWD) + payload
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unexpected Confirm payload length 0")
            # EAP-pwd: Unexpected Confirm payload length 0 (expected 32)
            payload = struct.pack(">B", 0x03)
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + len(payload), EAP_TYPE_PWD) + payload

        idx += 1
        if ctx['num'] == idx:
            eap_proto_pwd_test_wait = True
            logger.info("Test: Valid id exchange")
            # EAP-PWD: Server EAP-pwd-ID proposal: group=19 random=1 prf=1 prep=0
            payload = struct.pack(">BHBBLB", 0x01, 19, 1, 1, 0, 0)
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + len(payload), EAP_TYPE_PWD) + payload
        idx += 1
        if ctx['num'] == idx:
            eap_proto_pwd_test_wait = True
            logger.info("Test: Commit payload with valid values")
            # EAP-pwd commit request, password prep is NONE
            element = binascii.unhexlify("8dcab2862c5396839a6bac0c689ff03d962863108e7c275bbf1d6eedf634ee832a214db99f0d0a1a6317733eecdd97f0fc4cda19f57e1bb9bb9c8dcf8c60ba6f")
            scalar = binascii.unhexlify("450f31e058cf2ac2636a5d6e2b3c70b1fcc301957f0716e77f13aa69f9a2e5bd")
            payload = struct.pack(">B", 0x02) + element + scalar
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + len(payload), EAP_TYPE_PWD) + payload
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Confirm payload with incorrect value")
            # EAP-PWD (peer): confirm did not verify
            payload = struct.pack(">B", 0x03) + 32*b'\0'
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + len(payload), EAP_TYPE_PWD) + payload

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unexpected confirm exchange")
            # EAP-pwd: processing frame: exch 3, len 0
            # EAP-PWD: PWD-ID-Req -> FAILURE
            payload = struct.pack(">B", 0x03)
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + len(payload), EAP_TYPE_PWD) + payload

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unsupported password pre-processing technique SASLprep in id exchange")
            # EAP-PWD: Server EAP-pwd-ID proposal: group=19 random=1 prf=1 prep=2
            # EAP-PWD: Unsupported password pre-processing technique (Prep=2)
            # EAP-PWD: PWD-ID-Req -> FAILURE
            payload = struct.pack(">BHBBLB", 0x01, 19, 1, 1, 0, 2)
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + len(payload), EAP_TYPE_PWD) + payload

        idx += 1
        if ctx['num'] == idx:
            eap_proto_pwd_test_wait = True
            logger.info("Test: Valid id exchange")
            # EAP-PWD: Server EAP-pwd-ID proposal: group=19 random=1 prf=1 prep=1
            payload = struct.pack(">BHBBLB", 0x01, 19, 1, 1, 0, 1)
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + len(payload), EAP_TYPE_PWD) + payload
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unexpected Commit payload length (prep=MS)")
            # EAP-pwd commit request, password prep is MS
            # EAP-pwd: Unexpected Commit payload length 0 (expected 96)
            payload = struct.pack(">B", 0x02)
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + len(payload), EAP_TYPE_PWD) + payload

        idx += 1
        if ctx['num'] == idx:
            eap_proto_pwd_test_wait = True
            logger.info("Test: Valid id exchange")
            # EAP-PWD: Server EAP-pwd-ID proposal: group=19 random=1 prf=1 prep=3
            payload = struct.pack(">BHBBLB", 0x01, 19, 1, 1, 0, 3)
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + len(payload), EAP_TYPE_PWD) + payload
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unexpected Commit payload length (prep=ssha1)")
            # EAP-pwd commit request, password prep is salted sha1
            # EAP-pwd: Invalid Salt-len
            payload = struct.pack(">B", 0x02)
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + len(payload), EAP_TYPE_PWD) + payload

        idx += 1
        if ctx['num'] == idx:
            eap_proto_pwd_test_wait = True
            logger.info("Test: Valid id exchange")
            # EAP-PWD: Server EAP-pwd-ID proposal: group=19 random=1 prf=1 prep=3
            payload = struct.pack(">BHBBLB", 0x01, 19, 1, 1, 0, 3)
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + len(payload), EAP_TYPE_PWD) + payload
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unexpected Commit payload length (prep=ssha1)")
            # EAP-pwd commit request, password prep is salted sha1
            # EAP-pwd: Invalid Salt-len
            payload = struct.pack(">BB", 0x02, 0)
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + len(payload), EAP_TYPE_PWD) + payload

        idx += 1
        if ctx['num'] == idx:
            eap_proto_pwd_test_wait = True
            logger.info("Test: Valid id exchange")
            # EAP-PWD: Server EAP-pwd-ID proposal: group=19 random=1 prf=1 prep=3
            payload = struct.pack(">BHBBLB", 0x01, 19, 1, 1, 0, 3)
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + len(payload), EAP_TYPE_PWD) + payload
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unexpected Commit payload length (prep=ssha1)")
            # EAP-pwd commit request, password prep is salted sha1
            # EAP-pwd: Unexpected Commit payload length 1 (expected 98)
            payload = struct.pack(">BB", 0x02, 1)
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + len(payload), EAP_TYPE_PWD) + payload

        idx += 1
        if ctx['num'] == idx:
            eap_proto_pwd_test_wait = True
            logger.info("Test: Valid id exchange")
            # EAP-PWD: Server EAP-pwd-ID proposal: group=19 random=1 prf=1 prep=4
            payload = struct.pack(">BHBBLB", 0x01, 19, 1, 1, 0, 4)
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + len(payload), EAP_TYPE_PWD) + payload
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unexpected Commit payload length (prep=ssha256)")
            # EAP-pwd commit request, password prep is salted sha256
            # EAP-pwd: Invalid Salt-len
            payload = struct.pack(">B", 0x02)
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + len(payload), EAP_TYPE_PWD) + payload

        idx += 1
        if ctx['num'] == idx:
            eap_proto_pwd_test_wait = True
            logger.info("Test: Valid id exchange")
            # EAP-PWD: Server EAP-pwd-ID proposal: group=19 random=1 prf=1 prep=4
            payload = struct.pack(">BHBBLB", 0x01, 19, 1, 1, 0, 4)
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + len(payload), EAP_TYPE_PWD) + payload
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unexpected Commit payload length (prep=ssha256)")
            # EAP-pwd commit request, password prep is salted sha256
            # EAP-pwd: Invalid Salt-len
            payload = struct.pack(">BB", 0x02, 0)
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + len(payload), EAP_TYPE_PWD) + payload

        idx += 1
        if ctx['num'] == idx:
            eap_proto_pwd_test_wait = True
            logger.info("Test: Valid id exchange")
            # EAP-PWD: Server EAP-pwd-ID proposal: group=19 random=1 prf=1 prep=4
            payload = struct.pack(">BHBBLB", 0x01, 19, 1, 1, 0, 4)
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + len(payload), EAP_TYPE_PWD) + payload
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unexpected Commit payload length (prep=ssha256)")
            # EAP-pwd commit request, password prep is salted sha256
            # EAP-pwd: Unexpected Commit payload length 1 (expected 98)
            payload = struct.pack(">BB", 0x02, 1)
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + len(payload), EAP_TYPE_PWD) + payload

        idx += 1
        if ctx['num'] == idx:
            eap_proto_pwd_test_wait = True
            logger.info("Test: Valid id exchange")
            # EAP-PWD: Server EAP-pwd-ID proposal: group=19 random=1 prf=1 prep=5
            payload = struct.pack(">BHBBLB", 0x01, 19, 1, 1, 0, 5)
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + len(payload), EAP_TYPE_PWD) + payload
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unexpected Commit payload length (prep=ssha512)")
            # EAP-pwd commit request, password prep is salted sha512
            # EAP-pwd: Invalid Salt-len
            payload = struct.pack(">B", 0x02)
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + len(payload), EAP_TYPE_PWD) + payload

        idx += 1
        if ctx['num'] == idx:
            eap_proto_pwd_test_wait = True
            logger.info("Test: Valid id exchange")
            # EAP-PWD: Server EAP-pwd-ID proposal: group=19 random=1 prf=1 prep=5
            payload = struct.pack(">BHBBLB", 0x01, 19, 1, 1, 0, 5)
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + len(payload), EAP_TYPE_PWD) + payload
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unexpected Commit payload length (prep=ssha512)")
            # EAP-pwd commit request, password prep is salted sha512
            # EAP-pwd: Invalid Salt-len
            payload = struct.pack(">BB", 0x02, 0)
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + len(payload), EAP_TYPE_PWD) + payload

        idx += 1
        if ctx['num'] == idx:
            eap_proto_pwd_test_wait = True
            logger.info("Test: Valid id exchange")
            # EAP-PWD: Server EAP-pwd-ID proposal: group=19 random=1 prf=1 prep=5
            payload = struct.pack(">BHBBLB", 0x01, 19, 1, 1, 0, 5)
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + len(payload), EAP_TYPE_PWD) + payload
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unexpected Commit payload length (prep=ssha512)")
            # EAP-pwd commit request, password prep is salted sha512
            # EAP-pwd: Unexpected Commit payload length 1 (expected 98)
            payload = struct.pack(">BB", 0x02, 1)
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + len(payload), EAP_TYPE_PWD) + payload

        logger.info("No more test responses available - test case completed")
        global eap_proto_pwd_test_done
        eap_proto_pwd_test_done = True
        return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

    srv = start_radius_server(pwd_handler)

    try:
        hapd = start_ap(apdev[0])
        dev[0].scan_for_bss(hapd.own_addr(), freq=2412)

        i = 0
        while not eap_proto_pwd_test_done:
            i += 1
            logger.info("Running connection iteration %d" % i)
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="PWD", identity="pwd user",
                           password="secret password",
                           wait_connect=False)
            ok = False
            for j in range(5):
                ev = dev[0].wait_event(["CTRL-EVENT-EAP-STATUS",
                                        "CTRL-EVENT-EAP-PROPOSED-METHOD"],
                                       timeout=5)
                if ev is None:
                    raise Exception("Timeout on EAP start")
                if "CTRL-EVENT-EAP-PROPOSED-METHOD" in ev:
                    ok = True
                    break
                if "CTRL-EVENT-EAP-STATUS" in ev and "status='completion' parameter='failure'" in ev:
                    ok = True
                    break
            if not ok:
                raise Exception("Expected EAP event not seen")
            if eap_proto_pwd_test_wait:
                for k in range(20):
                    time.sleep(0.1)
                    if not eap_proto_pwd_test_wait:
                        break
                if eap_proto_pwd_test_wait:
                    raise Exception("eap_proto_pwd_test_wait not cleared")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected(timeout=1)
            dev[0].dump_monitor()
    finally:
        stop_radius_server(srv)

def test_eap_proto_pwd_invalid_scalar(dev, apdev):
    """EAP-pwd protocol tests - invalid server scalar"""
    check_eap_capa(dev[0], "PWD")
    run_eap_proto_pwd_invalid_scalar(dev, apdev, 32*b'\0')
    run_eap_proto_pwd_invalid_scalar(dev, apdev, 31*b'\0' + b'\x01')
    # Group Order
    val = binascii.unhexlify("FFFFFFFF00000000FFFFFFFFFFFFFFFFBCE6FAADA7179E84F3B9CAC2FC632551")
    run_eap_proto_pwd_invalid_scalar(dev, apdev, val)
    # Group Order - 1
    val = binascii.unhexlify("FFFFFFFF00000000FFFFFFFFFFFFFFFFBCE6FAADA7179E84F3B9CAC2FC632550")
    run_eap_proto_pwd_invalid_scalar(dev, apdev, val, valid_scalar=True)

def run_eap_proto_pwd_invalid_scalar(dev, apdev, scalar, valid_scalar=False):
    global eap_proto_pwd_invalid_scalar_fail
    eap_proto_pwd_invalid_scalar_fail = False

    def pwd_handler(ctx, req):
        logger.info("pwd_handler - RX " + binascii.hexlify(req).decode())
        if 'num' not in ctx:
            ctx['num'] = 0
        ctx['num'] = ctx['num'] + 1
        if 'id' not in ctx:
            ctx['id'] = 1
        ctx['id'] = (ctx['id'] + 1) % 256
        idx = 0

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Valid id exchange")
            payload = struct.pack(">BHBBLB", 0x01, 19, 1, 1, 0, 0)
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + len(payload), EAP_TYPE_PWD) + payload
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Commit payload with invalid scalar")
            payload = struct.pack(">B", 0x02) + binascii.unhexlify("67feb2b46d59e6dd3af3a429ec9c04a949337564615d3a2c19bdf6826eb6f5efa303aed86af3a072ed819d518d620adb2659f0e84c4f8b739629db8c93088cfc") + scalar
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + len(payload), EAP_TYPE_PWD) + payload
        idx += 1
        if ctx['num'] == idx:
            logger.info("Confirm message next - should not get here")
            global eap_proto_pwd_invalid_scalar_fail
            eap_proto_pwd_invalid_scalar_fail = True
            payload = struct.pack(">B", 0x03) + 32*b'\0'
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + len(payload), EAP_TYPE_PWD) + payload

        logger.info("No more test responses available - test case completed")
        return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

    srv = start_radius_server(pwd_handler)

    try:
        hapd = start_ap(apdev[0])
        dev[0].scan_for_bss(hapd.own_addr(), freq=2412)

        dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                       eap="PWD", identity="pwd user",
                       password="secret password",
                       wait_connect=False)
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"], timeout=5)
        if ev is None:
            raise Exception("EAP failure not reported")
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected(timeout=1)
        dev[0].dump_monitor()
    finally:
        stop_radius_server(srv)

    if valid_scalar and not eap_proto_pwd_invalid_scalar_fail:
        raise Exception("Peer did not accept valid EAP-pwd-Commit scalar")
    if not valid_scalar and eap_proto_pwd_invalid_scalar_fail:
        raise Exception("Peer did not stop after invalid EAP-pwd-Commit scalar")

def test_eap_proto_pwd_invalid_element(dev, apdev):
    """EAP-pwd protocol tests - invalid server element"""
    check_eap_capa(dev[0], "PWD")
    # Invalid x,y coordinates
    run_eap_proto_pwd_invalid_element(dev, apdev, 64*b'\x00')
    run_eap_proto_pwd_invalid_element(dev, apdev, 32*b'\x00' + 32*b'\x01')
    run_eap_proto_pwd_invalid_element(dev, apdev, 32*b'\x01' + 32*b'\x00')
    run_eap_proto_pwd_invalid_element(dev, apdev, 32*b'\xff' + 32*b'\x01')
    run_eap_proto_pwd_invalid_element(dev, apdev, 32*b'\x01' + 32*b'\xff')
    run_eap_proto_pwd_invalid_element(dev, apdev, 64*b'\xff')
    # Not on curve
    run_eap_proto_pwd_invalid_element(dev, apdev, 64*b'\x01')

def run_eap_proto_pwd_invalid_element(dev, apdev, element):
    global eap_proto_pwd_invalid_element_fail
    eap_proto_pwd_invalid_element_fail = False

    def pwd_handler(ctx, req):
        logger.info("pwd_handler - RX " + binascii.hexlify(req).decode())
        if 'num' not in ctx:
            ctx['num'] = 0
        ctx['num'] = ctx['num'] + 1
        if 'id' not in ctx:
            ctx['id'] = 1
        ctx['id'] = (ctx['id'] + 1) % 256
        idx = 0

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Valid id exchange")
            payload = struct.pack(">BHBBLB", 0x01, 19, 1, 1, 0, 0)
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + len(payload), EAP_TYPE_PWD) + payload
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Commit payload with invalid element")
            payload = struct.pack(">B", 0x02) + element + 31*b'\0' + b'\x02'
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + len(payload), EAP_TYPE_PWD) + payload
        idx += 1
        if ctx['num'] == idx:
            logger.info("Confirm message next - should not get here")
            global eap_proto_pwd_invalid_element_fail
            eap_proto_pwd_invalid_element_fail = True
            payload = struct.pack(">B", 0x03) + 32*b'\0'
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + len(payload), EAP_TYPE_PWD) + payload

        logger.info("No more test responses available - test case completed")
        return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

    srv = start_radius_server(pwd_handler)

    try:
        hapd = start_ap(apdev[0])
        dev[0].scan_for_bss(hapd.own_addr(), freq=2412)

        dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                       eap="PWD", identity="pwd user",
                       password="secret password",
                       wait_connect=False)
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"], timeout=5)
        if ev is None:
            raise Exception("EAP failure not reported")
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected(timeout=1)
        dev[0].dump_monitor()
    finally:
        stop_radius_server(srv)

    if eap_proto_pwd_invalid_element_fail:
        raise Exception("Peer did not stop after invalid EAP-pwd-Commit element")

def rx_msg(src):
    ev = src.wait_event(["EAPOL-TX"], timeout=5)
    if ev is None:
        raise Exception("No EAPOL-TX")
    return ev.split(' ')[2]

def tx_msg(src, dst, msg):
    dst.request("EAPOL_RX " + src.own_addr() + " " + msg)

def proxy_msg(src, dst):
    msg = rx_msg(src)
    tx_msg(src, dst, msg)
    return msg

def start_pwd_exchange(dev, ap):
    check_eap_capa(dev, "PWD")
    params = hostapd.wpa2_eap_params(ssid="test-wpa2-eap")
    hapd = hostapd.add_ap(ap, params)
    hapd.request("SET ext_eapol_frame_io 1")
    dev.request("SET ext_eapol_frame_io 1")
    dev.connect("test-wpa2-eap", key_mgmt="WPA-EAP",
                   eap="PWD", identity="pwd user", password="secret password",
                   wait_connect=False, scan_freq="2412")
    proxy_msg(hapd, dev) # EAP-Identity/Request
    proxy_msg(dev, hapd) # EAP-Identity/Response
    proxy_msg(hapd, dev) # EAP-pwd-ID/Request
    proxy_msg(dev, hapd) # EAP-pwd-ID/Response
    return hapd

def test_eap_proto_pwd_unexpected_fragment(dev, apdev):
    """EAP-pwd protocol tests - unexpected more-fragment frame"""
    hapd = start_pwd_exchange(dev[0], apdev[0])

    # EAP-pwd-Commit/Request
    req = rx_msg(hapd)
    if req[18:20] != "02":
        raise Exception("Unexpected EAP-pwd-Commit/Request flag")
    msg = req[0:18] + "42" + req[20:]
    tx_msg(hapd, dev[0], msg)

def test_eap_proto_pwd_reflection_attack(dev, apdev):
    """EAP-pwd protocol tests - reflection attack on the server"""
    hapd = start_pwd_exchange(dev[0], apdev[0])

    # EAP-pwd-Commit/Request
    req = proxy_msg(hapd, dev[0])
    if len(req) != 212:
        raise Exception("Unexpected EAP-pwd-Commit/Response length")

    # EAP-pwd-Commit/Response
    resp = rx_msg(dev[0])
    # Reflect same Element/Scalar back to the server
    msg = resp[0:20] + req[20:]
    tx_msg(dev[0], hapd, msg)

    # EAP-pwd-Commit/Response or EAP-Failure
    req = rx_msg(hapd)
    if req[8:10] != "04":
        # reflect EAP-pwd-Confirm/Request
        msg = req[0:8] + "02" + req[10:]
        tx_msg(dev[0], hapd, msg)
        req = rx_msg(hapd)
        if req[8:10] == "03":
            raise Exception("EAP-Success after reflected Element/Scalar")
        raise Exception("No EAP-Failure to reject invalid EAP-pwd-Commit/Response")

def test_eap_proto_pwd_invalid_scalar_peer(dev, apdev):
    """EAP-pwd protocol tests - invalid peer scalar"""
    run_eap_proto_pwd_invalid_scalar_peer(dev, apdev, 32*"00")
    run_eap_proto_pwd_invalid_scalar_peer(dev, apdev, 31*"00" + "01")
    # Group Order
    run_eap_proto_pwd_invalid_scalar_peer(dev, apdev,
                                          "FFFFFFFF00000000FFFFFFFFFFFFFFFFBCE6FAADA7179E84F3B9CAC2FC632551")
    # Group Order - 1
    run_eap_proto_pwd_invalid_scalar_peer(dev, apdev,
                                          "FFFFFFFF00000000FFFFFFFFFFFFFFFFBCE6FAADA7179E84F3B9CAC2FC632550",
                                          valid_scalar=True)

def run_eap_proto_pwd_invalid_scalar_peer(dev, apdev, scalar,
                                          valid_scalar=False):
    hapd = start_pwd_exchange(dev[0], apdev[0])
    proxy_msg(hapd, dev[0]) # EAP-pwd-Commit/Request

    # EAP-pwd-Commit/Response
    resp = rx_msg(dev[0])
    # Replace scalar with an invalid value
    msg = resp[0:20] + resp[20:148] + scalar
    tx_msg(dev[0], hapd, msg)

    # EAP-pwd-Commit/Response or EAP-Failure
    req = rx_msg(hapd)
    if valid_scalar and req[8:10] == "04":
        raise Exception("Unexpected EAP-Failure with valid scalar")
    if not valid_scalar and req[8:10] != "04":
        raise Exception("No EAP-Failure to reject invalid scalar")
    dev[0].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected(timeout=1)
    hapd.disable()

def test_eap_proto_pwd_invalid_element_peer(dev, apdev):
    """EAP-pwd protocol tests - invalid peer element"""
    # Invalid x,y coordinates
    run_eap_proto_pwd_invalid_element_peer(dev, apdev, 64*'00')
    run_eap_proto_pwd_invalid_element_peer(dev, apdev, 32*'00' + 32*'01')
    run_eap_proto_pwd_invalid_element_peer(dev, apdev, 32*'01' + 32*'00')
    run_eap_proto_pwd_invalid_element_peer(dev, apdev, 32*'ff' + 32*'01')
    run_eap_proto_pwd_invalid_element_peer(dev, apdev, 32*'01' + 32*'ff')
    run_eap_proto_pwd_invalid_element_peer(dev, apdev, 64*'ff')
    # Not on curve
    run_eap_proto_pwd_invalid_element_peer(dev, apdev, 64*'01')

def run_eap_proto_pwd_invalid_element_peer(dev, apdev, element):
    hapd = start_pwd_exchange(dev[0], apdev[0])
    proxy_msg(hapd, dev[0]) # EAP-pwd-Commit/Request

    # EAP-pwd-Commit/Response
    resp = rx_msg(dev[0])
    # Replace element with an invalid value
    msg = resp[0:20] + element + resp[148:]
    tx_msg(dev[0], hapd, msg)

    # EAP-pwd-Commit/Response or EAP-Failure
    req = rx_msg(hapd)
    if req[8:10] != "04":
        raise Exception("No EAP-Failure to reject invalid element")
    dev[0].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected(timeout=1)
    hapd.disable()

def test_eap_proto_pwd_errors(dev, apdev):
    """EAP-pwd local error cases"""
    check_eap_capa(dev[0], "PWD")
    params = hostapd.wpa2_eap_params(ssid="eap-test")
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].scan_for_bss(hapd.own_addr(), freq=2412)

    for i in range(1, 4):
        with alloc_fail(dev[0], i, "eap_pwd_init"):
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="PWD", identity="pwd user",
                           password="secret password",
                           wait_connect=False)
            ev = dev[0].wait_event(["EAP: Failed to initialize EAP method"],
                                   timeout=15)
            if ev is None:
                raise Exception("Timeout on EAP start")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

    with alloc_fail(dev[0], 1, "eap_pwd_get_session_id"):
        dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                       eap="PWD", identity="pwd user",
                       fragment_size="0",
                       password="secret password")
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()

    funcs = ["eap_pwd_getkey", "eap_pwd_get_emsk",
             "=wpabuf_alloc;eap_pwd_perform_commit_exchange",
             "=wpabuf_alloc;eap_pwd_perform_confirm_exchange"]
    for func in funcs:
        with alloc_fail(dev[0], 1, func):
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="PWD", identity="pwd user@domain",
                           password="secret password", erp="1",
                           wait_connect=False)
            wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

    for i in range(1, 5):
        with alloc_fail(dev[0], i, "eap_pwd_perform_id_exchange"):
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="PWD", identity="pwd user",
                           password="secret password",
                           wait_connect=False)
            ev = dev[0].wait_event(["CTRL-EVENT-EAP-PROPOSED-METHOD"],
                                   timeout=15)
            if ev is None:
                raise Exception("Timeout on EAP start")
            ok = False
            for j in range(10):
                state = dev[0].request('GET_ALLOC_FAIL')
                if state.startswith('0:'):
                    ok = True
                    break
                time.sleep(0.1)
            if not ok:
                raise Exception("No allocation failure seen")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

    with alloc_fail(dev[0], 1, "wpabuf_alloc;eap_pwd_perform_id_exchange"):
        dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                       eap="PWD", identity="pwd user",
                       password="secret password",
                       wait_connect=False)
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-PROPOSED-METHOD"],
                               timeout=15)
        if ev is None:
            raise Exception("Timeout on EAP start")
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()

    for i in range(1, 9):
        with alloc_fail(dev[0], i, "eap_pwd_perform_commit_exchange"):
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="PWD", identity="pwd user",
                           password="secret password",
                           wait_connect=False)
            ev = dev[0].wait_event(["CTRL-EVENT-EAP-PROPOSED-METHOD"],
                                   timeout=15)
            if ev is None:
                raise Exception("Timeout on EAP start")
            ok = False
            for j in range(10):
                state = dev[0].request('GET_ALLOC_FAIL')
                if state.startswith('0:'):
                    ok = True
                    break
                time.sleep(0.1)
            if not ok:
                raise Exception("No allocation failure seen")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

    for i in range(1, 12):
        with alloc_fail(dev[0], i, "eap_pwd_perform_confirm_exchange"):
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="PWD", identity="pwd user",
                           password="secret password",
                           wait_connect=False)
            ev = dev[0].wait_event(["CTRL-EVENT-EAP-PROPOSED-METHOD"],
                                   timeout=15)
            if ev is None:
                raise Exception("Timeout on EAP start")
            ok = False
            for j in range(10):
                state = dev[0].request('GET_ALLOC_FAIL')
                if state.startswith('0:'):
                    ok = True
                    break
                time.sleep(0.1)
            if not ok:
                raise Exception("No allocation failure seen")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

    for i in range(1, 5):
        with alloc_fail(dev[0], i, "eap_msg_alloc;=eap_pwd_process"):
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="PWD", identity="pwd user",
                           password="secret password", fragment_size="50",
                           wait_connect=False)
            ev = dev[0].wait_event(["CTRL-EVENT-EAP-PROPOSED-METHOD"],
                                   timeout=15)
            if ev is None:
                raise Exception("Timeout on EAP start")
            wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

    # No password configured
    dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                   eap="PWD", identity="pwd user",
                   wait_connect=False)
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-PROPOSED-METHOD vendor=0 method=52"],
                           timeout=15)
    if ev is None:
        raise Exception("EAP-pwd not started")
    dev[0].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected()

    funcs = [(1, "hash_nt_password_hash;eap_pwd_perform_commit_exchange"),
             (1, "=crypto_bignum_init;eap_pwd_perform_commit_exchange"),
             (1, "=crypto_ec_point_init;eap_pwd_perform_commit_exchange"),
             (2, "=crypto_ec_point_init;eap_pwd_perform_commit_exchange"),
             (1, "=crypto_ec_point_mul;eap_pwd_perform_commit_exchange"),
             (2, "=crypto_ec_point_mul;eap_pwd_perform_commit_exchange"),
             (3, "=crypto_ec_point_mul;eap_pwd_perform_commit_exchange"),
             (1, "=crypto_ec_point_add;eap_pwd_perform_commit_exchange"),
             (1, "=crypto_ec_point_invert;eap_pwd_perform_commit_exchange"),
             (1, "=crypto_ec_point_to_bin;eap_pwd_perform_commit_exchange"),
             (1, "crypto_hash_finish;eap_pwd_kdf"),
             (1, "crypto_ec_point_from_bin;eap_pwd_get_element"),
             (3, "crypto_bignum_init;compute_password_element"),
             (4, "crypto_bignum_init;compute_password_element"),
             (1, "crypto_bignum_init_set;compute_password_element"),
             (2, "crypto_bignum_init_set;compute_password_element"),
             (3, "crypto_bignum_init_set;compute_password_element"),
             (1, "crypto_bignum_to_bin;compute_password_element"),
             (1, "crypto_ec_point_compute_y_sqr;compute_password_element"),
             (1, "crypto_ec_point_solve_y_coord;compute_password_element"),
             (1, "crypto_bignum_rand;compute_password_element"),
             (1, "crypto_bignum_sub;compute_password_element")]
    for count, func in funcs:
        with fail_test(dev[0], count, func):
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="PWD", identity="pwd-hash",
                           password_hex="hash:e3718ece8ab74792cbbfffd316d2d19a",
                           wait_connect=False)
            ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"], timeout=10)
            if ev is None:
                raise Exception("No EAP-Failure reported")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

    params = {"ssid": "eap-test2", "wpa": "2", "wpa_key_mgmt": "WPA-EAP",
              "rsn_pairwise": "CCMP", "ieee8021x": "1",
              "eap_server": "1", "eap_user_file": "auth_serv/eap_user.conf",
              "pwd_group": "19", "fragment_size": "40"}
    hapd2 = hostapd.add_ap(apdev[1], params)
    dev[0].scan_for_bss(hapd2.own_addr(), freq=2412)

    with alloc_fail(dev[0], 1, "wpabuf_alloc;=eap_pwd_process"):
        dev[0].connect("eap-test2", key_mgmt="WPA-EAP", scan_freq="2412",
                       eap="PWD", identity="pwd user",
                       password="secret password",
                       wait_connect=False)
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()

    for i in range(1, 5):
        with fail_test(dev[0], i,
                       "=crypto_ec_point_to_bin;eap_pwd_perform_confirm_exchange"):
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="PWD", identity="pwd-hash",
                           password_hex="hash:e3718ece8ab74792cbbfffd316d2d19a",
                           wait_connect=False)
            ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"], timeout=10)
            if ev is None:
                raise Exception("No EAP-Failure reported")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()
            dev[0].dump_monitor()

def run_eap_pwd_connect(dev, hash=True, fragment=2000):
    if hash:
        dev.connect("test-wpa2-eap", key_mgmt="WPA-EAP",
                    fragment_size=str(fragment),
                    eap="PWD", identity="pwd-hash",
                    password_hex="hash:e3718ece8ab74792cbbfffd316d2d19a",
                    scan_freq="2412", wait_connect=False)
    else:
        dev.connect("test-wpa2-eap", key_mgmt="WPA-EAP",
                    fragment_size=str(fragment),
                    eap="PWD", identity="pwd-hash-sha1",
                    password="secret password",
                    scan_freq="2412", wait_connect=False)
    ev = dev.wait_event(["CTRL-EVENT-EAP-SUCCESS", "CTRL-EVENT-EAP-FAILURE",
                         "CTRL-EVENT-DISCONNECTED"],
                        timeout=1)
    dev.request("REMOVE_NETWORK all")
    if not ev or "CTRL-EVENT-DISCONNECTED" not in ev:
        dev.wait_disconnected()
    dev.dump_monitor()

def test_eap_proto_pwd_errors_server(dev, apdev):
    """EAP-pwd local error cases on server"""
    check_eap_capa(dev[0], "PWD")
    params = int_eap_server_params()
    params['erp_domain'] = 'example.com'
    params['eap_server_erp'] = '1'
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].scan_for_bss(hapd.own_addr(), freq=2412)

    tests = [(1, "eap_pwd_init"),
             (2, "eap_pwd_init"),
             (3, "eap_pwd_init"),
             (1, "eap_pwd_build_id_req"),
             (1, "eap_pwd_build_commit_req"),
             (1, "eap_pwd_build_confirm_req"),
             (1, "eap_pwd_h_init;eap_pwd_build_confirm_req"),
             (1, "wpabuf_alloc;eap_pwd_build_confirm_req"),
             (1, "eap_msg_alloc;eap_pwd_build_req"),
             (1, "eap_pwd_process_id_resp"),
             (1, "get_eap_pwd_group;eap_pwd_process_id_resp"),
             (1, "eap_pwd_process_confirm_resp"),
             (1, "eap_pwd_h_init;eap_pwd_process_confirm_resp"),
             (1, "compute_keys;eap_pwd_process_confirm_resp"),
             (1, "eap_pwd_getkey"),
             (1, "eap_pwd_get_emsk"),
             (1, "eap_pwd_get_session_id")]
    for count, func in tests:
        with alloc_fail(hapd, count, func):
            run_eap_pwd_connect(dev[0], hash=True)

    tests = [(1, "eap_msg_alloc;eap_pwd_build_req"),
             (2, "eap_msg_alloc;eap_pwd_build_req"),
             (1, "wpabuf_alloc;eap_pwd_process")]
    for count, func in tests:
        with alloc_fail(hapd, count, func):
            run_eap_pwd_connect(dev[0], hash=True, fragment=13)

    tests = [(4, "eap_pwd_init")]
    for count, func in tests:
        with alloc_fail(hapd, count, func):
            run_eap_pwd_connect(dev[0], hash=False)

    tests = [(1, "eap_pwd_build_id_req"),
             (1, "eap_pwd_build_commit_req"),
             (1, "crypto_ec_point_mul;eap_pwd_build_commit_req"),
             (1, "crypto_ec_point_invert;eap_pwd_build_commit_req"),
             (1, "crypto_ec_point_to_bin;eap_pwd_build_commit_req"),
             (1, "crypto_ec_point_to_bin;eap_pwd_build_confirm_req"),
             (2, "=crypto_ec_point_to_bin;eap_pwd_build_confirm_req"),
             (1, "hash_nt_password_hash;eap_pwd_process_id_resp"),
             (1, "compute_password_element;eap_pwd_process_id_resp"),
             (1, "crypto_bignum_init;eap_pwd_process_commit_resp"),
             (1, "crypto_ec_point_mul;eap_pwd_process_commit_resp"),
             (2, "crypto_ec_point_mul;eap_pwd_process_commit_resp"),
             (1, "crypto_ec_point_add;eap_pwd_process_commit_resp"),
             (1, "crypto_ec_point_to_bin;eap_pwd_process_confirm_resp"),
             (2, "=crypto_ec_point_to_bin;eap_pwd_process_confirm_resp")]
    for count, func in tests:
        with fail_test(hapd, count, func):
            run_eap_pwd_connect(dev[0], hash=True)

def start_pwd_assoc(dev, hapd):
    dev.connect("test-wpa2-eap", key_mgmt="WPA-EAP",
                eap="PWD", identity="pwd user", password="secret password",
                wait_connect=False, scan_freq="2412")
    proxy_msg(hapd, dev) # EAP-Identity/Request
    proxy_msg(dev, hapd) # EAP-Identity/Response
    proxy_msg(hapd, dev) # EAP-pwd-Identity/Request

def stop_pwd_assoc(dev, hapd):
    dev.request("REMOVE_NETWORK all")
    dev.wait_disconnected()
    dev.dump_monitor()
    hapd.dump_monitor()

def test_eap_proto_pwd_server(dev, apdev):
    """EAP-pwd protocol testing for the server"""
    check_eap_capa(dev[0], "PWD")
    params = int_eap_server_params()
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].scan_for_bss(hapd.own_addr(), freq=2412)
    hapd.request("SET ext_eapol_frame_io 1")
    dev[0].request("SET ext_eapol_frame_io 1")

    start_pwd_assoc(dev[0], hapd)
    resp = rx_msg(dev[0])
    # Replace exch field with unexpected value
    # --> EAP-pwd: Unexpected opcode=4 in state=0
    msg = resp[0:18] + "04" + resp[20:]
    tx_msg(dev[0], hapd, msg)

    # Too short EAP-pwd header (no flags/exch field)
    # --> EAP-pwd: Invalid frame
    msg = resp[0:4] + "0005" + resp[8:12] + "0005" + "34"
    tx_msg(dev[0], hapd, msg)

    # Too short EAP-pwd header (L=1 but only one octet of total length field)
    # --> EAP-pwd: Frame too short to contain Total-Length field
    msg = resp[0:4] + "0007" + resp[8:12] + "0007" + "34" + "81ff"
    tx_msg(dev[0], hapd, msg)
    # server continues exchange, so start from scratch for the next step
    rx_msg(hapd)
    stop_pwd_assoc(dev[0], hapd)

    start_pwd_assoc(dev[0], hapd)
    resp = rx_msg(dev[0])
    # Too large total length
    msg = resp[0:4] + "0008" + resp[8:12] + "0008" + "34" + "c1ffff"
    tx_msg(dev[0], hapd, msg)
    # server continues exchange, so start from scratch for the next step
    rx_msg(hapd)
    stop_pwd_assoc(dev[0], hapd)

    start_pwd_assoc(dev[0], hapd)
    resp = rx_msg(dev[0])
    # First fragment
    msg = resp[0:4] + "0009" + resp[8:12] + "0009" + "34" + "c100ff" + "aa"
    tx_msg(dev[0], hapd, msg)
    # Ack
    req = rx_msg(hapd)
    # Unexpected first fragment
    # --> EAP-pwd: Unexpected new fragment start when previous fragment is still in use
    msg = resp[0:4] + "0009" + resp[8:10] + req[10:12] + "0009" + "34" + "c100ee" + "bb"
    tx_msg(dev[0], hapd, msg)
    # server continues exchange, so start from scratch for the next step
    rx_msg(hapd)
    stop_pwd_assoc(dev[0], hapd)

    start_pwd_assoc(dev[0], hapd)
    resp = rx_msg(dev[0])
    # Too much data in first fragment
    # --> EAP-pwd: Buffer overflow attack detected! (0+2 > 1)
    msg = resp[0:4] + "000a" + resp[8:12] + "000a" + "34" + "c10001" + "aabb"
    tx_msg(dev[0], hapd, msg)
    # EAP-Failure
    rx_msg(hapd)
    stop_pwd_assoc(dev[0], hapd)

    start_pwd_assoc(dev[0], hapd)
    resp = rx_msg(dev[0])
    # Change parameters
    # --> EAP-pwd: peer changed parameters
    msg = resp[0:20] + "ff" + resp[22:]
    tx_msg(dev[0], hapd, msg)
    # EAP-Failure
    rx_msg(hapd)
    stop_pwd_assoc(dev[0], hapd)

    start_pwd_assoc(dev[0], hapd)
    resp = rx_msg(dev[0])
    # Too short ID response
    # --> EAP-pwd: Invalid ID response
    msg = resp[0:4] + "000a" + resp[8:12] + "000a" + "34" + "01ffeeddcc"
    tx_msg(dev[0], hapd, msg)
    # server continues exchange, so start from scratch for the next step
    rx_msg(hapd)
    stop_pwd_assoc(dev[0], hapd)

    start_pwd_assoc(dev[0], hapd)
    # EAP-pwd-Identity/Response
    resp = rx_msg(dev[0])
    tx_msg(dev[0], hapd, resp)
    # EAP-pwd-Commit/Request
    req = rx_msg(hapd)
    # Unexpected EAP-pwd-Identity/Response
    # --> EAP-pwd: Unexpected opcode=1 in state=1
    msg = resp[0:10] + req[10:12] + resp[12:]
    tx_msg(dev[0], hapd, msg)
    # server continues exchange, so start from scratch for the next step
    rx_msg(hapd)
    stop_pwd_assoc(dev[0], hapd)

    start_pwd_assoc(dev[0], hapd)
    proxy_msg(dev[0], hapd) # EAP-pwd-Identity/Response
    proxy_msg(hapd, dev[0]) # EAP-pwd-Commit/Request
    # EAP-pwd-Commit/Response
    resp = rx_msg(dev[0])
    # Too short Commit response
    # --> EAP-pwd: Unexpected Commit payload length 4 (expected 96)
    msg = resp[0:4] + "000a" + resp[8:12] + "000a" + "34" + "02ffeeddcc"
    tx_msg(dev[0], hapd, msg)
    # EAP-Failure
    rx_msg(hapd)
    stop_pwd_assoc(dev[0], hapd)

    start_pwd_assoc(dev[0], hapd)
    proxy_msg(dev[0], hapd) # EAP-pwd-Identity/Response
    proxy_msg(hapd, dev[0]) # EAP-pwd-Commit/Request
    proxy_msg(dev[0], hapd) # EAP-pwd-Commit/Response
    proxy_msg(hapd, dev[0]) # EAP-pwd-Confirm/Request
    # EAP-pwd-Confirm/Response
    resp = rx_msg(dev[0])
    # Too short Confirm response
    # --> EAP-pwd: Unexpected Confirm payload length 4 (expected 32)
    msg = resp[0:4] + "000a" + resp[8:12] + "000a" + "34" + "03ffeeddcc"
    tx_msg(dev[0], hapd, msg)
    # EAP-Failure
    rx_msg(hapd)
    stop_pwd_assoc(dev[0], hapd)

    start_pwd_assoc(dev[0], hapd)
    resp = rx_msg(dev[0])
    # Set M=1
    # --> EAP-pwd: No buffer for reassembly
    msg = resp[0:18] + "41" + resp[20:]
    tx_msg(dev[0], hapd, msg)
    # EAP-Failure
    rx_msg(hapd)
    stop_pwd_assoc(dev[0], hapd)

def test_eap_proto_erp(dev, apdev):
    """ERP protocol tests"""
    check_erp_capa(dev[0])

    global eap_proto_erp_test_done
    eap_proto_erp_test_done = False

    def erp_handler(ctx, req):
        logger.info("erp_handler - RX " + binascii.hexlify(req).decode())
        if 'num' not in ctx:
            ctx['num'] = 0
        ctx['num'] += 1
        if 'id' not in ctx:
            ctx['id'] = 1
        ctx['id'] = (ctx['id'] + 1) % 256
        idx = 0

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Missing type")
            return struct.pack(">BBH", EAP_CODE_INITIATE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unexpected type")
            return struct.pack(">BBHB", EAP_CODE_INITIATE, ctx['id'], 4 + 1,
                               255)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Missing Reserved field")
            return struct.pack(">BBHB", EAP_CODE_INITIATE, ctx['id'], 4 + 1,
                               EAP_ERP_TYPE_REAUTH_START)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Zero-length TVs/TLVs")
            payload = b""
            return struct.pack(">BBHBB", EAP_CODE_INITIATE, ctx['id'],
                               4 + 1 + 1 + len(payload),
                               EAP_ERP_TYPE_REAUTH_START, 0) + payload

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Too short TLV")
            payload = struct.pack("B", 191)
            return struct.pack(">BBHBB", EAP_CODE_INITIATE, ctx['id'],
                               4 + 1 + 1 + len(payload),
                               EAP_ERP_TYPE_REAUTH_START, 0) + payload

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Truncated TLV")
            payload = struct.pack("BB", 191, 1)
            return struct.pack(">BBHBB", EAP_CODE_INITIATE, ctx['id'],
                               4 + 1 + 1 + len(payload),
                               EAP_ERP_TYPE_REAUTH_START, 0) + payload

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Ignored unknown TLV and unknown TV/TLV terminating parsing")
            payload = struct.pack("BBB", 191, 0, 192)
            return struct.pack(">BBHBB", EAP_CODE_INITIATE, ctx['id'],
                               4 + 1 + 1 + len(payload),
                               EAP_ERP_TYPE_REAUTH_START, 0) + payload

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: More than one keyName-NAI")
            payload = struct.pack("BBBB", EAP_ERP_TLV_KEYNAME_NAI, 0,
                                  EAP_ERP_TLV_KEYNAME_NAI, 0)
            return struct.pack(">BBHBB", EAP_CODE_INITIATE, ctx['id'],
                               4 + 1 + 1 + len(payload),
                               EAP_ERP_TYPE_REAUTH_START, 0) + payload

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Too short TLV keyName-NAI")
            payload = struct.pack("B", EAP_ERP_TLV_KEYNAME_NAI)
            return struct.pack(">BBHBB", EAP_CODE_INITIATE, ctx['id'],
                               4 + 1 + 1 + len(payload),
                               EAP_ERP_TYPE_REAUTH_START, 0) + payload

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Truncated TLV keyName-NAI")
            payload = struct.pack("BB", EAP_ERP_TLV_KEYNAME_NAI, 1)
            return struct.pack(">BBHBB", EAP_CODE_INITIATE, ctx['id'],
                               4 + 1 + 1 + len(payload),
                               EAP_ERP_TYPE_REAUTH_START, 0) + payload

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Valid rRK lifetime TV followed by too short rMSK lifetime TV")
            payload = struct.pack(">BLBH", EAP_ERP_TV_RRK_LIFETIME, 0,
                                  EAP_ERP_TV_RMSK_LIFETIME, 0)
            return struct.pack(">BBHBB", EAP_CODE_INITIATE, ctx['id'],
                               4 + 1 + 1 + len(payload),
                               EAP_ERP_TYPE_REAUTH_START, 0) + payload

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Missing type (Finish)")
            return struct.pack(">BBH", EAP_CODE_FINISH, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unexpected type (Finish)")
            return struct.pack(">BBHB", EAP_CODE_FINISH, ctx['id'], 4 + 1,
                               255)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Missing fields (Finish)")
            return struct.pack(">BBHB", EAP_CODE_FINISH, ctx['id'], 4 + 1,
                               EAP_ERP_TYPE_REAUTH)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unexpected SEQ (Finish)")
            return struct.pack(">BBHBBHB", EAP_CODE_FINISH, ctx['id'],
                               4 + 1 + 4,
                               EAP_ERP_TYPE_REAUTH, 0, 0xffff, 0)

        logger.info("No more test responses available - test case completed")
        global eap_proto_erp_test_done
        eap_proto_erp_test_done = True
        return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

    srv = start_radius_server(erp_handler)

    try:
        hapd = start_ap(apdev[0])
        dev[0].scan_for_bss(hapd.own_addr(), freq=2412)

        i = 0
        while not eap_proto_erp_test_done:
            i += 1
            logger.info("Running connection iteration %d" % i)
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="PAX", identity="pax.user@example.com",
                           password_hex="0123456789abcdef0123456789abcdef",
                           wait_connect=False)
            ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED"], timeout=5)
            if ev is None:
                raise Exception("Timeout on EAP start")
            time.sleep(0.1)
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected(timeout=1)
            dev[0].dump_monitor()
    finally:
        stop_radius_server(srv)

def test_eap_proto_fast_errors(dev, apdev):
    """EAP-FAST local error cases"""
    check_eap_capa(dev[0], "FAST")
    params = hostapd.wpa2_eap_params(ssid="eap-test")
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].scan_for_bss(hapd.own_addr(), freq=2412)

    for i in range(1, 5):
        with alloc_fail(dev[0], i, "eap_fast_init"):
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="FAST", anonymous_identity="FAST",
                           identity="user", password="password",
                           ca_cert="auth_serv/ca.pem", phase2="auth=GTC",
                           phase1="fast_provisioning=2",
                           pac_file="blob://fast_pac_auth",
                           wait_connect=False)
            ev = dev[0].wait_event(["EAP: Failed to initialize EAP method"],
                                   timeout=5)
            if ev is None:
                raise Exception("Timeout on EAP start")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

    tests = [(1, "wpabuf_alloc;eap_fast_tlv_eap_payload"),
             (1, "eap_fast_derive_key;eap_fast_derive_key_auth"),
             (1, "eap_msg_alloc;eap_peer_tls_phase2_nak"),
             (1, "wpabuf_alloc;eap_fast_tlv_result"),
             (1, "wpabuf_alloc;eap_fast_tlv_pac_ack"),
             (1, "=eap_peer_tls_derive_session_id;eap_fast_process_crypto_binding"),
             (1, "eap_peer_tls_decrypt;eap_fast_decrypt"),
             (1, "eap_fast_getKey"),
             (1, "eap_fast_get_session_id"),
             (1, "eap_fast_get_emsk")]
    for count, func in tests:
        dev[0].request("SET blob fast_pac_auth_errors ")
        with alloc_fail(dev[0], count, func):
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="FAST", anonymous_identity="FAST",
                           identity="user@example.com", password="password",
                           ca_cert="auth_serv/ca.pem", phase2="auth=GTC",
                           phase1="fast_provisioning=2",
                           pac_file="blob://fast_pac_auth_errors",
                           erp="1",
                           wait_connect=False)
            ev = dev[0].wait_event(["CTRL-EVENT-EAP-PROPOSED-METHOD"],
                                   timeout=15)
            if ev is None:
                raise Exception("Timeout on EAP start")
            wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

    tests = [(1, "eap_fast_derive_key;eap_fast_derive_key_provisioning"),
             (1, "eap_mschapv2_getKey;eap_fast_get_phase2_key"),
             (1, "=eap_fast_use_pac_opaque"),
             (1, "eap_fast_copy_buf"),
             (1, "=eap_fast_add_pac"),
             (1, "=eap_fast_init_pac_data"),
             (1, "=eap_fast_write_pac"),
             (2, "=eap_fast_write_pac")]
    for count, func in tests:
        dev[0].request("SET blob fast_pac_errors ")
        with alloc_fail(dev[0], count, func):
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="FAST", anonymous_identity="FAST",
                           identity="user", password="password",
                           ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2",
                           phase1="fast_provisioning=1",
                           pac_file="blob://fast_pac_errors",
                           erp="1",
                           wait_connect=False)
            ev = dev[0].wait_event(["CTRL-EVENT-EAP-PROPOSED-METHOD"],
                                   timeout=15)
            if ev is None:
                raise Exception("Timeout on EAP start")
            wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

    tests = [(1, "eap_fast_get_cmk;eap_fast_process_crypto_binding"),
             (1, "eap_fast_derive_eap_msk;eap_fast_process_crypto_binding"),
             (1, "eap_fast_derive_eap_emsk;eap_fast_process_crypto_binding")]
    for count, func in tests:
        dev[0].request("SET blob fast_pac_auth_errors ")
        with fail_test(dev[0], count, func):
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="FAST", anonymous_identity="FAST",
                           identity="user", password="password",
                           ca_cert="auth_serv/ca.pem", phase2="auth=GTC",
                           phase1="fast_provisioning=2",
                           pac_file="blob://fast_pac_auth_errors",
                           erp="1",
                           wait_connect=False)
            ev = dev[0].wait_event(["CTRL-EVENT-EAP-PROPOSED-METHOD"],
                                   timeout=15)
            if ev is None:
                raise Exception("Timeout on EAP start")
            wait_fail_trigger(dev[0], "GET_FAIL")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

    dev[0].request("SET blob fast_pac_errors ")
    dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                   eap="FAST", anonymous_identity="FAST",
                   identity="user", password="password",
                   ca_cert="auth_serv/ca.pem", phase2="auth=GTC",
                   phase1="fast_provisioning=1",
                   pac_file="blob://fast_pac_errors",
                   wait_connect=False)
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-METHOD"], timeout=5)
    if ev is None:
        raise Exception("Timeout on EAP start")
    # EAP-FAST: Only EAP-MSCHAPv2 is allowed during unauthenticated
    # provisioning; reject phase2 type 6
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"], timeout=5)
    if ev is None:
        raise Exception("Timeout on EAP failure")
    dev[0].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected()

    logger.info("Wrong password in Phase 2")
    dev[0].request("SET blob fast_pac_errors ")
    dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                   eap="FAST", anonymous_identity="FAST",
                   identity="user", password="wrong password",
                   ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2",
                   phase1="fast_provisioning=1",
                   pac_file="blob://fast_pac_errors",
                   wait_connect=False)
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-METHOD"], timeout=5)
    if ev is None:
        raise Exception("Timeout on EAP start")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"], timeout=5)
    if ev is None:
        raise Exception("Timeout on EAP failure")
    dev[0].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected()

    tests = ["FOOBAR\n",
             "wpa_supplicant EAP-FAST PAC file - version 1\nFOOBAR\n",
             "wpa_supplicant EAP-FAST PAC file - version 1\nSTART\n",
             "wpa_supplicant EAP-FAST PAC file - version 1\nSTART\nSTART\n",
             "wpa_supplicant EAP-FAST PAC file - version 1\nEND\n",
             "wpa_supplicant EAP-FAST PAC file - version 1\nSTART\nPAC-Type=12345\nEND\n"
             "wpa_supplicant EAP-FAST PAC file - version 1\nSTART\nPAC-Key=12\nEND\n",
             "wpa_supplicant EAP-FAST PAC file - version 1\nSTART\nPAC-Key=1\nEND\n",
             "wpa_supplicant EAP-FAST PAC file - version 1\nSTART\nPAC-Key=1q\nEND\n",
             "wpa_supplicant EAP-FAST PAC file - version 1\nSTART\nPAC-Opaque=1\nEND\n",
             "wpa_supplicant EAP-FAST PAC file - version 1\nSTART\nA-ID=1\nEND\n",
             "wpa_supplicant EAP-FAST PAC file - version 1\nSTART\nI-ID=1\nEND\n",
             "wpa_supplicant EAP-FAST PAC file - version 1\nSTART\nA-ID-Info=1\nEND\n"]
    for pac in tests:
        blob = binascii.hexlify(pac.encode()).decode()
        dev[0].request("SET blob fast_pac_errors " + blob)
        dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                       eap="FAST", anonymous_identity="FAST",
                       identity="user", password="password",
                       ca_cert="auth_serv/ca.pem", phase2="auth=GTC",
                       phase1="fast_provisioning=2",
                       pac_file="blob://fast_pac_errors",
                       wait_connect=False)
        ev = dev[0].wait_event(["EAP: Failed to initialize EAP method"],
                               timeout=5)
        if ev is None:
            raise Exception("Timeout on EAP start")
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()

    tests = ["wpa_supplicant EAP-FAST PAC file - version 1\nSTART\nEND\n",
             "wpa_supplicant EAP-FAST PAC file - version 1\nSTART\nEND\nSTART\nEND\nSTART\nEND\n"]
    for pac in tests:
        blob = binascii.hexlify(pac.encode()).decode()
        dev[0].request("SET blob fast_pac_errors " + blob)
        dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                       eap="FAST", anonymous_identity="FAST",
                       identity="user", password="password",
                       ca_cert="auth_serv/ca.pem", phase2="auth=GTC",
                       phase1="fast_provisioning=2",
                       pac_file="blob://fast_pac_errors")
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()

    dev[0].request("SET blob fast_pac_errors ")

def test_eap_proto_peap_errors_server(dev, apdev):
    """EAP-PEAP local error cases on server"""
    params = int_eap_server_params()
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].scan_for_bss(hapd.own_addr(), freq=2412)

    tests = [(1, "get_asymetric_start_key;eap_mschapv2_getKey"),
             (1, "generate_authenticator_response_pwhash;eap_mschapv2_process_response"),
             (1, "hash_nt_password_hash;eap_mschapv2_process_response"),
             (1, "get_master_key;eap_mschapv2_process_response")]
    for count, func in tests:
        with fail_test(hapd, count, func):
            dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP",
                           scan_freq="2412",
                           eap="PEAP", anonymous_identity="peap",
                           identity="user", password="password",
                           phase1="peapver=0 crypto_binding=2",
                           ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2",
                           erp="1", wait_connect=False)
            ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"], timeout=10)
            if ev is None:
                raise Exception("EAP-Failure not reported")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

def test_eap_proto_peap_errors(dev, apdev):
    """EAP-PEAP local error cases"""
    check_eap_capa(dev[0], "PEAP")
    check_eap_capa(dev[0], "MSCHAPV2")
    params = hostapd.wpa2_eap_params(ssid="eap-test")
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].scan_for_bss(hapd.own_addr(), freq=2412)

    for i in range(1, 5):
        with alloc_fail(dev[0], i, "eap_peap_init"):
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="PEAP", anonymous_identity="peap",
                           identity="user", password="password",
                           ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2",
                           wait_connect=False)
            ev = dev[0].wait_event(["EAP: Failed to initialize EAP method"],
                                   timeout=5)
            if ev is None:
                raise Exception("Timeout on EAP start")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

    tests = [(1, "eap_mschapv2_getKey;eap_peap_get_isk;eap_peap_derive_cmk"),
             (1, "eap_msg_alloc;eap_tlv_build_result"),
             (1, "eap_mschapv2_init;eap_peap_phase2_request"),
             (1, "eap_peer_tls_decrypt;eap_peap_decrypt"),
             (1, "wpabuf_alloc;=eap_peap_decrypt"),
             (1, "eap_peer_tls_encrypt;eap_peap_decrypt"),
             (1, "eap_peer_tls_process_helper;eap_peap_process"),
             (1, "eap_peer_tls_derive_key;eap_peap_process"),
             (1, "eap_peer_tls_derive_session_id;eap_peap_process"),
             (1, "eap_peap_getKey"),
             (1, "eap_peap_get_session_id")]
    for count, func in tests:
        with alloc_fail(dev[0], count, func):
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="PEAP", anonymous_identity="peap",
                           identity="user", password="password",
                           phase1="peapver=0 crypto_binding=2",
                           ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2",
                           erp="1", wait_connect=False)
            ev = dev[0].wait_event(["CTRL-EVENT-EAP-PROPOSED-METHOD"],
                                   timeout=15)
            if ev is None:
                raise Exception("Timeout on EAP start")
            wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

    tests = [(1, "peap_prfplus;eap_peap_derive_cmk"),
             (1, "eap_tlv_add_cryptobinding;eap_tlv_build_result"),
             (1, "peap_prfplus;eap_peap_getKey"),
             (1, "get_asymetric_start_key;eap_mschapv2_getKey")]
    for count, func in tests:
        with fail_test(dev[0], count, func):
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="PEAP", anonymous_identity="peap",
                           identity="user", password="password",
                           phase1="peapver=0 crypto_binding=2",
                           ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2",
                           erp="1", wait_connect=False)
            ev = dev[0].wait_event(["CTRL-EVENT-EAP-PROPOSED-METHOD"],
                                   timeout=15)
            if ev is None:
                raise Exception("Timeout on EAP start")
            wait_fail_trigger(dev[0], "GET_FAIL")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

    with alloc_fail(dev[0], 1,
                    "eap_peer_tls_phase2_nak;eap_peap_phase2_request"):
        dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                       eap="PEAP", anonymous_identity="peap",
                       identity="cert user", password="password",
                       ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2",
                       wait_connect=False)
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()

def test_eap_proto_ttls_errors(dev, apdev):
    """EAP-TTLS local error cases"""
    check_eap_capa(dev[0], "TTLS")
    check_eap_capa(dev[0], "MSCHAPV2")
    params = hostapd.wpa2_eap_params(ssid="eap-test")
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].scan_for_bss(hapd.own_addr(), freq=2412)

    for i in range(1, 5):
        with alloc_fail(dev[0], i, "eap_ttls_init"):
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="TTLS", anonymous_identity="ttls",
                           identity="user", password="password",
                           ca_cert="auth_serv/ca.pem",
                           phase2="autheap=MSCHAPV2",
                           wait_connect=False)
            ev = dev[0].wait_event(["EAP: Failed to initialize EAP method"],
                                   timeout=5)
            if ev is None:
                raise Exception("Timeout on EAP start")
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

    tests = [(1, "eap_peer_tls_derive_key;eap_ttls_v0_derive_key",
              "DOMAIN\mschapv2 user", "auth=MSCHAPV2"),
             (1, "eap_peer_tls_derive_session_id;eap_ttls_v0_derive_key",
              "DOMAIN\mschapv2 user", "auth=MSCHAPV2"),
             (1, "wpabuf_alloc;eap_ttls_phase2_request_mschapv2",
              "DOMAIN\mschapv2 user", "auth=MSCHAPV2"),
             (1, "eap_peer_tls_derive_key;eap_ttls_phase2_request_mschapv2",
              "DOMAIN\mschapv2 user", "auth=MSCHAPV2"),
             (1, "eap_peer_tls_encrypt;eap_ttls_encrypt_response;eap_ttls_implicit_identity_request",
              "DOMAIN\mschapv2 user", "auth=MSCHAPV2"),
             (1, "eap_peer_tls_decrypt;eap_ttls_decrypt",
              "DOMAIN\mschapv2 user", "auth=MSCHAPV2"),
             (1, "eap_ttls_getKey",
              "DOMAIN\mschapv2 user", "auth=MSCHAPV2"),
             (1, "eap_ttls_get_session_id",
              "DOMAIN\mschapv2 user", "auth=MSCHAPV2"),
             (1, "eap_ttls_get_emsk",
              "mschapv2 user@domain", "auth=MSCHAPV2"),
             (1, "wpabuf_alloc;eap_ttls_phase2_request_mschap",
              "mschap user", "auth=MSCHAP"),
             (1, "eap_peer_tls_derive_key;eap_ttls_phase2_request_mschap",
              "mschap user", "auth=MSCHAP"),
             (1, "wpabuf_alloc;eap_ttls_phase2_request_chap",
              "chap user", "auth=CHAP"),
             (1, "eap_peer_tls_derive_key;eap_ttls_phase2_request_chap",
              "chap user", "auth=CHAP"),
             (1, "wpabuf_alloc;eap_ttls_phase2_request_pap",
              "pap user", "auth=PAP"),
             (1, "wpabuf_alloc;eap_ttls_avp_encapsulate",
              "user", "autheap=MSCHAPV2"),
             (1, "eap_mschapv2_init;eap_ttls_phase2_request_eap_method",
              "user", "autheap=MSCHAPV2"),
             (1, "eap_sm_buildIdentity;eap_ttls_phase2_request_eap",
              "user", "autheap=MSCHAPV2"),
             (1, "eap_ttls_avp_encapsulate;eap_ttls_phase2_request_eap",
              "user", "autheap=MSCHAPV2"),
             (1, "eap_ttls_parse_attr_eap",
              "user", "autheap=MSCHAPV2"),
             (1, "eap_peer_tls_encrypt;eap_ttls_encrypt_response;eap_ttls_process_decrypted",
              "user", "autheap=MSCHAPV2"),
             (1, "eap_ttls_fake_identity_request",
              "user", "autheap=MSCHAPV2"),
             (1, "eap_msg_alloc;eap_tls_process_output",
              "user", "autheap=MSCHAPV2"),
             (1, "eap_msg_alloc;eap_peer_tls_build_ack",
              "user", "autheap=MSCHAPV2"),
             (1, "eap_peer_tls_phase2_nak;eap_ttls_phase2_request_eap_method",
              "cert user", "autheap=MSCHAPV2")]
    tls = dev[0].request("GET tls_library")
    if tls.startswith("internal"):
        tests += [(1, "tlsv1_client_decrypt;eap_peer_tls_decrypt",
                   "user", "autheap=MSCHAPV2")]
    else:
        tests += [(1, "tls_connection_decrypt;eap_peer_tls_decrypt",
                   "user", "autheap=MSCHAPV2")]
    for count, func, identity, phase2 in tests:
        with alloc_fail(dev[0], count, func):
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="TTLS", anonymous_identity="ttls",
                           identity=identity, password="password",
                           ca_cert="auth_serv/ca.pem", phase2=phase2,
                           erp="1", wait_connect=False)
            ev = dev[0].wait_event(["CTRL-EVENT-EAP-PROPOSED-METHOD"],
                                   timeout=15)
            if ev is None:
                raise Exception("Timeout on EAP start")
            wait_fail_trigger(dev[0], "GET_ALLOC_FAIL",
                              note="Allocation failure not triggered for: %d:%s" % (count, func))
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

    tests = [(1, "os_get_random;eap_ttls_phase2_request_mschapv2"),
             (1, "mschapv2_derive_response;eap_ttls_phase2_request_mschapv2")]
    for count, func in tests:
        with fail_test(dev[0], count, func):
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="TTLS", anonymous_identity="ttls",
                           identity="DOMAIN\mschapv2 user", password="password",
                           ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2",
                           erp="1", wait_connect=False)
            ev = dev[0].wait_event(["CTRL-EVENT-EAP-PROPOSED-METHOD"],
                                   timeout=15)
            if ev is None:
                raise Exception("Timeout on EAP start")
            wait_fail_trigger(dev[0], "GET_FAIL",
                              note="Test failure not triggered for: %d:%s" % (count, func))
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

    tests = [(1, "nt_challenge_response;eap_ttls_phase2_request_mschap")]
    for count, func in tests:
        with fail_test(dev[0], count, func):
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="TTLS", anonymous_identity="ttls",
                           identity="mschap user", password="password",
                           ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAP",
                           erp="1", wait_connect=False)
            ev = dev[0].wait_event(["CTRL-EVENT-EAP-PROPOSED-METHOD"],
                                   timeout=15)
            if ev is None:
                raise Exception("Timeout on EAP start")
            wait_fail_trigger(dev[0], "GET_FAIL",
                              note="Test failure not triggered for: %d:%s" % (count, func))
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected()

def test_eap_proto_expanded(dev, apdev):
    """EAP protocol tests with expanded header"""
    global eap_proto_expanded_test_done
    eap_proto_expanded_test_done = False

    def expanded_handler(ctx, req):
        logger.info("expanded_handler - RX " + binascii.hexlify(req).decode())
        if 'num' not in ctx:
            ctx['num'] = 0
        ctx['num'] += 1
        if 'id' not in ctx:
            ctx['id'] = 1
        ctx['id'] = (ctx['id'] + 1) % 256
        idx = 0

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: MD5 challenge in expanded header")
            return struct.pack(">BBHB3BLBBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 4 + 3,
                               EAP_TYPE_EXPANDED, 0, 0, 0, EAP_TYPE_MD5,
                               1, 0xaa, ord('n'))
        idx += 1
        if ctx['num'] == idx:
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Invalid expanded EAP length")
            return struct.pack(">BBHB3BH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 2,
                               EAP_TYPE_EXPANDED, 0, 0, 0, EAP_TYPE_MD5)
        idx += 1
        if ctx['num'] == idx:
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Invalid expanded frame type")
            return struct.pack(">BBHB3BL", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 4,
                               EAP_TYPE_EXPANDED, 0, 0, 1, EAP_TYPE_MD5)
        idx += 1
        if ctx['num'] == idx:
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: MSCHAPv2 Challenge")
            return struct.pack(">BBHBBBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 4 + 1 + 16 + 6,
                               EAP_TYPE_MSCHAPV2,
                               1, 0, 4 + 1 + 16 + 6, 16) + 16*b'A' + b'foobar'
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Invalid expanded frame type")
            return struct.pack(">BBHB3BL", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 4,
                               EAP_TYPE_EXPANDED, 0, 0, 1, EAP_TYPE_MSCHAPV2)

        logger.info("No more test responses available - test case completed")
        global eap_proto_expanded_test_done
        eap_proto_expanded_test_done = True
        return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

    srv = start_radius_server(expanded_handler)

    try:
        hapd = start_ap(apdev[0])
        dev[0].scan_for_bss(hapd.own_addr(), freq=2412)

        i = 0
        while not eap_proto_expanded_test_done:
            i += 1
            logger.info("Running connection iteration %d" % i)
            if i == 4:
                dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                               eap="MSCHAPV2", identity="user",
                               password="password",
                               wait_connect=False)
            else:
                dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                               eap="MD5", identity="user", password="password",
                               wait_connect=False)
            ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED"], timeout=5)
            if ev is None:
                raise Exception("Timeout on EAP start")
            if i in [1]:
                ev = dev[0].wait_event(["CTRL-EVENT-EAP-METHOD"], timeout=5)
                if ev is None:
                    raise Exception("Timeout on EAP method start")
                ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"], timeout=5)
                if ev is None:
                    raise Exception("Timeout on EAP failure")
            elif i in [2, 3]:
                ev = dev[0].wait_event(["CTRL-EVENT-EAP-PROPOSED-METHOD"],
                                       timeout=5)
                if ev is None:
                    raise Exception("Timeout on EAP proposed method")
                ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"], timeout=5)
                if ev is None:
                    raise Exception("Timeout on EAP failure")
            else:
                time.sleep(0.1)
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected(timeout=1)
            dev[0].dump_monitor()
    finally:
        stop_radius_server(srv)

def test_eap_proto_tls(dev, apdev):
    """EAP-TLS protocol tests"""
    check_eap_capa(dev[0], "TLS")
    global eap_proto_tls_test_done, eap_proto_tls_test_wait
    eap_proto_tls_test_done = False
    eap_proto_tls_test_wait = False

    def tls_handler(ctx, req):
        logger.info("tls_handler - RX " + binascii.hexlify(req).decode())
        if 'num' not in ctx:
            ctx['num'] = 0
        ctx['num'] += 1
        if 'id' not in ctx:
            ctx['id'] = 1
        ctx['id'] = (ctx['id'] + 1) % 256
        idx = 0

        global eap_proto_tls_test_wait

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Too much payload in TLS/Start: TLS Message Length (0 bytes) smaller than this fragment (1 bytes)")
            return struct.pack(">BBHBBLB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 4 + 1,
                               EAP_TYPE_TLS, 0xa0, 0, 1)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Fragmented TLS/Start")
            return struct.pack(">BBHBBLB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 4 + 1,
                               EAP_TYPE_TLS, 0xe0, 2, 1)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Too long fragment of TLS/Start: Invalid reassembly state: tls_in_left=2 tls_in_len=0 in_len=0")
            return struct.pack(">BBHBBBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 2,
                               EAP_TYPE_TLS, 0x00, 2, 3)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: TLS/Start")
            return struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1,
                               EAP_TYPE_TLS, 0x20)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Fragmented TLS message")
            return struct.pack(">BBHBBLB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 4 + 1,
                               EAP_TYPE_TLS, 0xc0, 2, 1)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Invalid TLS message: no Flags octet included + workaround")
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1,
                               EAP_TYPE_TLS)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Too long fragment of TLS message: more data than TLS message length indicated")
            return struct.pack(">BBHBBBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 2,
                               EAP_TYPE_TLS, 0x00, 2, 3)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Fragmented TLS/Start and truncated Message Length field")
            return struct.pack(">BBHBB3B", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 3,
                               EAP_TYPE_TLS, 0xe0, 1, 2, 3)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: TLS/Start")
            return struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1,
                               EAP_TYPE_TLS, 0x20)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Fragmented TLS message")
            return struct.pack(">BBHBBLB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 4 + 1,
                               EAP_TYPE_TLS, 0xc0, 2, 1)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Invalid TLS message: no Flags octet included + workaround disabled")
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1,
                               EAP_TYPE_TLS)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: TLS/Start")
            return struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1,
                               EAP_TYPE_TLS, 0x20)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Fragmented TLS message (long; first)")
            payload = 1450*b'A'
            return struct.pack(">BBHBBL", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 4 + len(payload),
                               EAP_TYPE_TLS, 0xc0, 65536) + payload
        # "Too long TLS fragment (size over 64 kB)" on the last one
        for i in range(44):
            idx += 1
            if ctx['num'] == idx:
                logger.info("Test: Fragmented TLS message (long; cont %d)" % i)
                eap_proto_tls_test_wait = True
                payload = 1470*b'A'
                return struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                                   4 + 1 + 1 + len(payload),
                                   EAP_TYPE_TLS, 0x40) + payload
        eap_proto_tls_test_wait = False
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: TLS/Start")
            return struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1,
                               EAP_TYPE_TLS, 0x20)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Non-ACK to more-fragment message")
            return struct.pack(">BBHBBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 1,
                               EAP_TYPE_TLS, 0x00, 255)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Failure")
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        logger.info("No more test responses available - test case completed")
        global eap_proto_tls_test_done
        eap_proto_tls_test_done = True
        return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

    srv = start_radius_server(tls_handler)

    try:
        hapd = start_ap(apdev[0])
        dev[0].scan_for_bss(hapd.own_addr(), freq=2412)

        i = 0
        while not eap_proto_tls_test_done:
            i += 1
            logger.info("Running connection iteration %d" % i)
            workaround = "0" if i == 6 else "1"
            fragment_size = "100" if i == 8 else "1400"
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="TLS", identity="tls user",
                           ca_cert="auth_serv/ca.pem",
                           client_cert="auth_serv/user.pem",
                           private_key="auth_serv/user.key",
                           eap_workaround=workaround,
                           fragment_size=fragment_size,
                           wait_connect=False)
            ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED"], timeout=5)
            if ev is None:
                raise Exception("Timeout on EAP start")
            ev = dev[0].wait_event(["CTRL-EVENT-EAP-METHOD",
                                    "CTRL-EVENT-EAP-STATUS"], timeout=5)
            if ev is None:
                raise Exception("Timeout on EAP method start")
            time.sleep(0.1)
            start = os.times()[4]
            while eap_proto_tls_test_wait:
                now = os.times()[4]
                if now - start > 10:
                    break
                time.sleep(0.1)
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected(timeout=1)
            dev[0].dump_monitor()
    finally:
        stop_radius_server(srv)

def test_eap_proto_tnc(dev, apdev):
    """EAP-TNC protocol tests"""
    check_eap_capa(dev[0], "TNC")
    global eap_proto_tnc_test_done
    eap_proto_tnc_test_done = False

    def tnc_handler(ctx, req):
        logger.info("tnc_handler - RX " + binascii.hexlify(req).decode())
        if 'num' not in ctx:
            ctx['num'] = 0
        ctx['num'] += 1
        if 'id' not in ctx:
            ctx['id'] = 1
        ctx['id'] = (ctx['id'] + 1) % 256
        idx = 0

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: TNC start with unsupported version")
            return struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1,
                               EAP_TYPE_TNC, 0x20)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: TNC without Flags field")
            return struct.pack(">BBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1,
                               EAP_TYPE_TNC)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Message underflow due to missing Message Length")
            return struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1,
                               EAP_TYPE_TNC, 0xa1)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Invalid Message Length")
            return struct.pack(">BBHBBLB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 4 + 1,
                               EAP_TYPE_TNC, 0xa1, 0, 0)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Invalid Message Length")
            return struct.pack(">BBHBBL", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 4,
                               EAP_TYPE_TNC, 0xe1, 75001)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Start with Message Length")
            return struct.pack(">BBHBBL", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 4,
                               EAP_TYPE_TNC, 0xa1, 1)
        idx += 1
        if ctx['num'] == idx:
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Server used start flag again")
            return struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1,
                               EAP_TYPE_TNC, 0x21)
        idx += 1
        if ctx['num'] == idx:
            return struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1,
                               EAP_TYPE_TNC, 0x21)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Fragmentation and unexpected payload in ack")
            return struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1,
                               EAP_TYPE_TNC, 0x21)
        idx += 1
        if ctx['num'] == idx:
            return struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1,
                               EAP_TYPE_TNC, 0x01)
        idx += 1
        if ctx['num'] == idx:
            return struct.pack(">BBHBBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 1,
                               EAP_TYPE_TNC, 0x01, 0)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Server fragmenting and fragment overflow")
            return struct.pack(">BBHBBLB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 4 + 1,
                               EAP_TYPE_TNC, 0xe1, 2, 1)
        idx += 1
        if ctx['num'] == idx:
            return struct.pack(">BBHBBBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 2,
                               EAP_TYPE_TNC, 0x01, 2, 3)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Server fragmenting and no message length in a fragment")
            return struct.pack(">BBHBBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + 1,
                               EAP_TYPE_TNC, 0x61, 2)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: TNC start followed by invalid TNCCS-Batch")
            return struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1,
                               EAP_TYPE_TNC, 0x21)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Received TNCCS-Batch: " + binascii.hexlify(req[6:]).decode())
            resp = b"FOO"
            return struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + len(resp),
                               EAP_TYPE_TNC, 0x01) + resp

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: TNC start followed by invalid TNCCS-Batch (2)")
            return struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1,
                               EAP_TYPE_TNC, 0x21)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Received TNCCS-Batch: " + binascii.hexlify(req[6:]).decode())
            resp = b"</TNCCS-Batch><TNCCS-Batch>"
            return struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + len(resp),
                               EAP_TYPE_TNC, 0x01) + resp

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: TNCCS-Batch missing BatchId attribute")
            return struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1,
                               EAP_TYPE_TNC, 0x21)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Received TNCCS-Batch: " + binascii.hexlify(req[6:]).decode())
            resp = b"<TNCCS-Batch    foo=3></TNCCS-Batch>"
            return struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + len(resp),
                               EAP_TYPE_TNC, 0x01) + resp

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unexpected IF-TNCCS BatchId")
            return struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1,
                               EAP_TYPE_TNC, 0x21)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Received TNCCS-Batch: " + binascii.hexlify(req[6:]).decode())
            resp = b"<TNCCS-Batch    BatchId=123456789></TNCCS-Batch>"
            return struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + len(resp),
                               EAP_TYPE_TNC, 0x01) + resp

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Missing IMC-IMV-Message and TNCC-TNCS-Message end tags")
            return struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1,
                               EAP_TYPE_TNC, 0x21)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Received TNCCS-Batch: " + binascii.hexlify(req[6:]).decode())
            resp = b"<TNCCS-Batch BatchId=2><IMC-IMV-Message><TNCC-TNCS-Message></TNCCS-Batch>"
            return struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + len(resp),
                               EAP_TYPE_TNC, 0x01) + resp
        idx += 1
        if ctx['num'] == idx:
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Missing IMC-IMV-Message and TNCC-TNCS-Message Type")
            return struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1,
                               EAP_TYPE_TNC, 0x21)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Received TNCCS-Batch: " + binascii.hexlify(req[6:]).decode())
            resp = b"<TNCCS-Batch BatchId=2><IMC-IMV-Message></IMC-IMV-Message><TNCC-TNCS-Message></TNCC-TNCS-Message></TNCCS-Batch>"
            return struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + len(resp),
                               EAP_TYPE_TNC, 0x01) + resp
        idx += 1
        if ctx['num'] == idx:
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Missing TNCC-TNCS-Message XML end tag")
            return struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1,
                               EAP_TYPE_TNC, 0x21)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Received TNCCS-Batch: " + binascii.hexlify(req[6:]).decode())
            resp = b"<TNCCS-Batch BatchId=2><TNCC-TNCS-Message><Type>00000001</Type><XML></TNCC-TNCS-Message></TNCCS-Batch>"
            return struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + len(resp),
                               EAP_TYPE_TNC, 0x01) + resp
        idx += 1
        if ctx['num'] == idx:
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Missing TNCC-TNCS-Message Base64 start tag")
            return struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1,
                               EAP_TYPE_TNC, 0x21)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Received TNCCS-Batch: " + binascii.hexlify(req[6:]).decode())
            resp = b"<TNCCS-Batch BatchId=2><TNCC-TNCS-Message><Type>00000001</Type></TNCC-TNCS-Message></TNCCS-Batch>"
            return struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + len(resp),
                               EAP_TYPE_TNC, 0x01) + resp
        idx += 1
        if ctx['num'] == idx:
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Missing TNCC-TNCS-Message Base64 end tag")
            return struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1,
                               EAP_TYPE_TNC, 0x21)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Received TNCCS-Batch: " + binascii.hexlify(req[6:]).decode())
            resp = b"<TNCCS-Batch BatchId=2><TNCC-TNCS-Message><Type>00000001</Type><Base64>abc</TNCC-TNCS-Message></TNCCS-Batch>"
            return struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + len(resp),
                               EAP_TYPE_TNC, 0x01) + resp
        idx += 1
        if ctx['num'] == idx:
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: TNCC-TNCS-Message Base64 message")
            return struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1,
                               EAP_TYPE_TNC, 0x21)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Received TNCCS-Batch: " + binascii.hexlify(req[6:]).decode())
            resp = b"<TNCCS-Batch BatchId=2><TNCC-TNCS-Message><Type>00000001</Type><Base64>aGVsbG8=</Base64></TNCC-TNCS-Message></TNCCS-Batch>"
            return struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + len(resp),
                               EAP_TYPE_TNC, 0x01) + resp
        idx += 1
        if ctx['num'] == idx:
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Invalid TNCC-TNCS-Message XML message")
            return struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1,
                               EAP_TYPE_TNC, 0x21)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Received TNCCS-Batch: " + binascii.hexlify(req[6:]).decode())
            resp = b"<TNCCS-Batch BatchId=2><TNCC-TNCS-Message><Type>00000001</Type><XML>hello</XML></TNCC-TNCS-Message></TNCCS-Batch>"
            return struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + len(resp),
                               EAP_TYPE_TNC, 0x01) + resp
        idx += 1
        if ctx['num'] == idx:
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Missing TNCCS-Recommendation type")
            return struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1,
                               EAP_TYPE_TNC, 0x21)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Received TNCCS-Batch: " + binascii.hexlify(req[6:]).decode())
            resp = b'<TNCCS-Batch BatchId=2><TNCC-TNCS-Message><Type>00000001</Type><XML><TNCCS-Recommendation foo=1></TNCCS-Recommendation></XML></TNCC-TNCS-Message></TNCCS-Batch>'
            return struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + len(resp),
                               EAP_TYPE_TNC, 0x01) + resp
        idx += 1
        if ctx['num'] == idx:
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: TNCCS-Recommendation type=none")
            return struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1,
                               EAP_TYPE_TNC, 0x21)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Received TNCCS-Batch: " + binascii.hexlify(req[6:]).decode())
            resp = b'<TNCCS-Batch BatchId=2><TNCC-TNCS-Message><Type>00000001</Type><XML><TNCCS-Recommendation type="none"></TNCCS-Recommendation></XML></TNCC-TNCS-Message></TNCCS-Batch>'
            return struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + len(resp),
                               EAP_TYPE_TNC, 0x01) + resp
        idx += 1
        if ctx['num'] == idx:
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: TNCCS-Recommendation type=isolate")
            return struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1,
                               EAP_TYPE_TNC, 0x21)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Received TNCCS-Batch: " + binascii.hexlify(req[6:]).decode())
            resp = b'<TNCCS-Batch BatchId=2><TNCC-TNCS-Message><Type>00000001</Type><XML><TNCCS-Recommendation type="isolate"></TNCCS-Recommendation></XML></TNCC-TNCS-Message></TNCCS-Batch>'
            return struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + len(resp),
                               EAP_TYPE_TNC, 0x01) + resp
        idx += 1
        if ctx['num'] == idx:
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        logger.info("No more test responses available - test case completed")
        global eap_proto_tnc_test_done
        eap_proto_tnc_test_done = True
        return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

    srv = start_radius_server(tnc_handler)

    try:
        hapd = start_ap(apdev[0])
        dev[0].scan_for_bss(hapd.own_addr(), freq=2412)

        i = 0
        while not eap_proto_tnc_test_done:
            i += 1
            logger.info("Running connection iteration %d" % i)
            frag = 1400
            if i == 8:
                frag = 150
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                           eap="TNC", identity="tnc", fragment_size=str(frag),
                           wait_connect=False)
            ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED"], timeout=5)
            if ev is None:
                raise Exception("Timeout on EAP start")
            ev = dev[0].wait_event(["CTRL-EVENT-EAP-METHOD",
                                    "CTRL-EVENT-EAP-STATUS"], timeout=5)
            if ev is None:
                raise Exception("Timeout on EAP method start")
            time.sleep(0.1)
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected(timeout=1)
            dev[0].dump_monitor()
    finally:
        stop_radius_server(srv)

def test_eap_canned_success_after_identity(dev, apdev):
    """EAP protocol tests for canned EAP-Success after identity"""
    check_eap_capa(dev[0], "MD5")
    def eap_canned_success_handler(ctx, req):
        logger.info("eap_canned_success_handler - RX " + binascii.hexlify(req).decode())
        if 'num' not in ctx:
            ctx['num'] = 0
        ctx['num'] = ctx['num'] + 1
        if 'id' not in ctx:
            ctx['id'] = 1
        ctx['id'] = (ctx['id'] + 1) % 256
        idx = 0

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Success")
            return struct.pack(">BBH", EAP_CODE_SUCCESS, ctx['id'], 4)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: EAP-Success")
            return struct.pack(">BBH", EAP_CODE_SUCCESS, ctx['id'], 4)

        return None

    srv = start_radius_server(eap_canned_success_handler)

    try:
        hapd = start_ap(apdev[0])
        dev[0].scan_for_bss(hapd.own_addr(), freq=2412)

        dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                       phase1="allow_canned_success=1",
                       eap="MD5", identity="user", password="password",
                       wait_connect=False)
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-SUCCESS"], timeout=15)
        if ev is None:
            raise Exception("Timeout on EAP success")
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()

        dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                       eap="MD5", identity="user", password="password",
                       wait_connect=False)
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED"], timeout=5)
        if ev is None:
            raise Exception("Timeout on EAP start")
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-SUCCESS"], timeout=0.1)
        if ev is not None:
            raise Exception("Unexpected EAP success")
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()
    finally:
        stop_radius_server(srv)

def test_eap_proto_wsc(dev, apdev):
    """EAP-WSC protocol tests"""
    global eap_proto_wsc_test_done, eap_proto_wsc_wait_failure
    eap_proto_wsc_test_done = False

    def wsc_handler(ctx, req):
        logger.info("wsc_handler - RX " + binascii.hexlify(req).decode())
        if 'num' not in ctx:
            ctx['num'] = 0
        ctx['num'] += 1
        if 'id' not in ctx:
            ctx['id'] = 1
        ctx['id'] = (ctx['id'] + 1) % 256
        idx = 0

        global eap_proto_wsc_wait_failure
        eap_proto_wsc_wait_failure = False

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Missing Flags field")
            return struct.pack(">BBHB3BLB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 4 + 1,
                               EAP_TYPE_EXPANDED, 0x00, 0x37, 0x2a, 1,
                               1)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Message underflow (missing Message Length field)")
            return struct.pack(">BBHB3BLBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 4 + 2,
                               EAP_TYPE_EXPANDED, 0x00, 0x37, 0x2a, 1,
                               1, 0x02)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Invalid Message Length (> 50000)")
            return struct.pack(">BBHB3BLBBH", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 4 + 4,
                               EAP_TYPE_EXPANDED, 0x00, 0x37, 0x2a, 1,
                               1, 0x02, 65535)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Invalid Message Length (< current payload)")
            return struct.pack(">BBHB3BLBBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 4 + 5,
                               EAP_TYPE_EXPANDED, 0x00, 0x37, 0x2a, 1,
                               1, 0x02, 0, 0xff)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unexpected Op-Code 5 in WAIT_START state")
            return struct.pack(">BBHB3BLBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 4 + 2,
                               EAP_TYPE_EXPANDED, 0x00, 0x37, 0x2a, 1,
                               5, 0x00)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Valid WSC Start to start the sequence")
            return struct.pack(">BBHB3BLBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 4 + 2,
                               EAP_TYPE_EXPANDED, 0x00, 0x37, 0x2a, 1,
                               1, 0x00)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: No Message Length field in a fragmented packet")
            return struct.pack(">BBHB3BLBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 4 + 2,
                               EAP_TYPE_EXPANDED, 0x00, 0x37, 0x2a, 1,
                               4, 0x01)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Valid WSC Start to start the sequence")
            return struct.pack(">BBHB3BLBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 4 + 2,
                               EAP_TYPE_EXPANDED, 0x00, 0x37, 0x2a, 1,
                               1, 0x00)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Valid first fragmented packet")
            return struct.pack(">BBHB3BLBBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 4 + 5,
                               EAP_TYPE_EXPANDED, 0x00, 0x37, 0x2a, 1,
                               4, 0x03, 10, 1)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unexpected Op-Code 5 in fragment (expected 4)")
            return struct.pack(">BBHB3BLBBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 4 + 3,
                               EAP_TYPE_EXPANDED, 0x00, 0x37, 0x2a, 1,
                               5, 0x01, 2)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Valid WSC Start to start the sequence")
            return struct.pack(">BBHB3BLBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 4 + 2,
                               EAP_TYPE_EXPANDED, 0x00, 0x37, 0x2a, 1,
                               1, 0x00)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Valid first fragmented packet")
            return struct.pack(">BBHB3BLBBHB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 4 + 5,
                               EAP_TYPE_EXPANDED, 0x00, 0x37, 0x2a, 1,
                               4, 0x03, 2, 1)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Fragment overflow")
            return struct.pack(">BBHB3BLBBBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 4 + 4,
                               EAP_TYPE_EXPANDED, 0x00, 0x37, 0x2a, 1,
                               4, 0x01, 2, 3)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Valid WSC Start to start the sequence")
            return struct.pack(">BBHB3BLBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 4 + 2,
                               EAP_TYPE_EXPANDED, 0x00, 0x37, 0x2a, 1,
                               1, 0x00)
        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Unexpected Op-Code 5 in WAIT_FRAG_ACK state")
            return struct.pack(">BBHB3BLBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 4 + 2,
                               EAP_TYPE_EXPANDED, 0x00, 0x37, 0x2a, 1,
                               5, 0x00)

        idx += 1
        if ctx['num'] == idx:
            logger.info("Test: Valid WSC Start")
            return struct.pack(">BBHB3BLBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 3 + 4 + 2,
                               EAP_TYPE_EXPANDED, 0x00, 0x37, 0x2a, 1,
                               1, 0x00)
        idx += 1
        if ctx['num'] == idx:
            logger.info("No more test responses available - test case completed")
            global eap_proto_wsc_test_done
            eap_proto_wsc_test_done = True
            eap_proto_wsc_wait_failure = True
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

    srv = start_radius_server(wsc_handler)

    try:
        hapd = start_ap(apdev[0])
        dev[0].scan_for_bss(hapd.own_addr(), freq=2412)

        i = 0
        while not eap_proto_wsc_test_done:
            i += 1
            logger.info("Running connection iteration %d" % i)
            fragment_size = 1398 if i != 9 else 50
            dev[0].connect("eap-test", key_mgmt="WPA-EAP", eap="WSC",
                           fragment_size=str(fragment_size),
                           identity="WFA-SimpleConfig-Enrollee-1-0",
                           phase1="pin=12345670",
                           scan_freq="2412", wait_connect=False)
            ev = dev[0].wait_event(["CTRL-EVENT-EAP-METHOD"], timeout=5)
            if ev is None:
                raise Exception("Timeout on EAP method start")
            if eap_proto_wsc_wait_failure:
                ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"], timeout=5)
                if ev is None:
                    raise Exception("Timeout on EAP failure")
            else:
                time.sleep(0.1)
            dev[0].request("REMOVE_NETWORK all")
            dev[0].wait_disconnected(timeout=1)
            dev[0].dump_monitor()
    finally:
        stop_radius_server(srv)

def test_eap_canned_success_before_method(dev, apdev):
    """EAP protocol tests for canned EAP-Success before any method"""
    params = int_eap_server_params()
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].scan_for_bss(hapd.own_addr(), freq=2412)
    bssid = apdev[0]['bssid']
    hapd.request("SET ext_eapol_frame_io 1")

    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", scan_freq="2412",
                   phase1="allow_canned_success=1",
                   eap="MD5", identity="user", password="password",
                   wait_connect=False)

    ev = hapd.wait_event(["EAPOL-TX"], timeout=10)
    if ev is None:
        raise Exception("Timeout on EAPOL-TX from hostapd")

    res = dev[0].request("EAPOL_RX " + bssid + " 0200000403020004")
    if "OK" not in res:
        raise Exception("EAPOL_RX to wpa_supplicant failed")

    ev = dev[0].wait_event(["CTRL-EVENT-EAP-SUCCESS"], timeout=5)
    if ev is None:
        raise Exception("Timeout on EAP success")
    dev[0].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected()

def test_eap_canned_failure_before_method(dev, apdev):
    """EAP protocol tests for canned EAP-Failure before any method"""
    params = int_eap_server_params()
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].scan_for_bss(hapd.own_addr(), freq=2412)
    bssid = apdev[0]['bssid']
    hapd.request("SET ext_eapol_frame_io 1")
    dev[0].connect("test-wpa2-eap", key_mgmt="WPA-EAP", scan_freq="2412",
                   phase1="allow_canned_success=1",
                   eap="MD5", identity="user", password="password",
                   wait_connect=False)

    ev = hapd.wait_event(["EAPOL-TX"], timeout=10)
    if ev is None:
        raise Exception("Timeout on EAPOL-TX from hostapd")

    res = dev[0].request("EAPOL_RX " + bssid + " 0200000404020004")
    if "OK" not in res:
        raise Exception("EAPOL_RX to wpa_supplicant failed")

    ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"], timeout=5)
    if ev is None:
        raise Exception("Timeout on EAP failure")
    dev[0].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected()

def test_eap_nak_oom(dev, apdev):
    """EAP-Nak OOM"""
    check_eap_capa(dev[0], "MD5")
    params = hostapd.wpa2_eap_params(ssid="eap-test")
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].scan_for_bss(hapd.own_addr(), freq=2412)
    with alloc_fail(dev[0], 1, "eap_msg_alloc;eap_sm_buildNak"):
        dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                       eap="MD5", identity="sake user", password="password",
                       wait_connect=False)
        wait_fail_trigger(dev[0], "GET_ALLOC_FAIL")
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()

def test_eap_nak_expanded(dev, apdev):
    """EAP-Nak with expanded method"""
    check_eap_capa(dev[0], "MD5")
    check_eap_capa(dev[0], "VENDOR-TEST")
    params = hostapd.wpa2_eap_params(ssid="eap-test")
    hapd = hostapd.add_ap(apdev[0], params)
    dev[0].scan_for_bss(hapd.own_addr(), freq=2412)
    dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                   eap="VENDOR-TEST WSC",
                   identity="sake user", password="password",
                   wait_connect=False)
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-PROPOSED-METHOD"], timeout=10)
    if ev is None or "NAK" not in ev:
        raise Exception("No NAK event seen")

    ev = dev[0].wait_event(["CTRL-EVENT-EAP-FAILURE"], timeout=10)
    if ev is None:
        raise Exception("No EAP-Failure seen")

    dev[0].request("REMOVE_NETWORK all")
    dev[0].wait_disconnected()

EAP_TLV_RESULT_TLV = 3
EAP_TLV_NAK_TLV = 4
EAP_TLV_ERROR_CODE_TLV = 5
EAP_TLV_CONNECTION_BINDING_TLV = 6
EAP_TLV_VENDOR_SPECIFIC_TLV = 7
EAP_TLV_URI_TLV = 8
EAP_TLV_EAP_PAYLOAD_TLV = 9
EAP_TLV_INTERMEDIATE_RESULT_TLV = 10
EAP_TLV_PAC_TLV = 11
EAP_TLV_CRYPTO_BINDING_TLV = 12
EAP_TLV_CALLING_STATION_ID_TLV = 13
EAP_TLV_CALLED_STATION_ID_TLV = 14
EAP_TLV_NAS_PORT_TYPE_TLV = 15
EAP_TLV_SERVER_IDENTIFIER_TLV = 16
EAP_TLV_IDENTITY_TYPE_TLV = 17
EAP_TLV_SERVER_TRUSTED_ROOT_TLV = 18
EAP_TLV_REQUEST_ACTION_TLV = 19
EAP_TLV_PKCS7_TLV = 20

EAP_TLV_RESULT_SUCCESS = 1
EAP_TLV_RESULT_FAILURE = 2

EAP_TLV_TYPE_MANDATORY = 0x8000
EAP_TLV_TYPE_MASK = 0x3fff

PAC_TYPE_PAC_KEY = 1
PAC_TYPE_PAC_OPAQUE = 2
PAC_TYPE_CRED_LIFETIME = 3
PAC_TYPE_A_ID = 4
PAC_TYPE_I_ID = 5
PAC_TYPE_A_ID_INFO = 7
PAC_TYPE_PAC_ACKNOWLEDGEMENT = 8
PAC_TYPE_PAC_INFO = 9
PAC_TYPE_PAC_TYPE = 10

def eap_fast_start(ctx):
    logger.info("Send EAP-FAST/Start")
    return struct.pack(">BBHBBHH", EAP_CODE_REQUEST, ctx['id'],
                       4 + 1 + 1 + 4 + 16,
                       EAP_TYPE_FAST, 0x21, 4, 16) + 16*b'A'

def test_eap_fast_proto(dev, apdev):
    """EAP-FAST Phase protocol testing"""
    check_eap_capa(dev[0], "FAST")
    global eap_fast_proto_ctx
    eap_fast_proto_ctx = None

    def eap_handler(ctx, req):
        logger.info("eap_handler - RX " + binascii.hexlify(req).decode())
        if 'num' not in ctx:
            ctx['num'] = 0
        ctx['num'] = ctx['num'] + 1
        if 'id' not in ctx:
            ctx['id'] = 1
        ctx['id'] = (ctx['id'] + 1) % 256
        idx = 0

        global eap_fast_proto_ctx
        eap_fast_proto_ctx = ctx
        ctx['test_done'] = False

        idx += 1
        if ctx['num'] == idx:
            return eap_fast_start(ctx)
        idx += 1
        if ctx['num'] == idx:
            logger.info("EAP-FAST: TLS processing failed")
            data = b'ABCDEFGHIK'
            return struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                               4 + 1 + 1 + len(data),
                               EAP_TYPE_FAST, 0x01) + data
        idx += 1
        if ctx['num'] == idx:
            ctx['test_done'] = True
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        logger.info("Past last test case")
        return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

    srv = start_radius_server(eap_handler)
    try:
        hapd = start_ap(apdev[0])
        dev[0].scan_for_bss(hapd.own_addr(), freq=2412)
        dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                       eap="FAST", anonymous_identity="FAST",
                       identity="user", password="password",
                       ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2",
                       phase1="fast_provisioning=1",
                       pac_file="blob://fast_pac_proto",
                       wait_connect=False)
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-METHOD"], timeout=5)
        if ev is None:
            raise Exception("Could not start EAP-FAST")
        ok = False
        for i in range(100):
            if eap_fast_proto_ctx:
                if eap_fast_proto_ctx['test_done']:
                    ok = True
                    break
            time.sleep(0.05)
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()
    finally:
        stop_radius_server(srv)

def run_eap_fast_phase2(dev, test_payload, test_failure=True):
    global eap_fast_proto_ctx
    eap_fast_proto_ctx = None

    def ssl_info_callback(conn, where, ret):
        logger.debug("SSL: info where=%d ret=%d" % (where, ret))

    def log_conn_state(conn):
        try:
            state = conn.state_string()
        except AttributeError:
            state = conn.get_state_string()
        if state:
            logger.info("State: " + str(state))

    def process_clienthello(ctx, payload):
        logger.info("Process ClientHello")
        ctx['sslctx'] = OpenSSL.SSL.Context(OpenSSL.SSL.TLSv1_METHOD)
        ctx['sslctx'].set_info_callback(ssl_info_callback)
        ctx['sslctx'].load_tmp_dh("auth_serv/dh.conf")
        if OpenSSL.SSL.OPENSSL_VERSION_NUMBER >= 0x10100000:
            ctx['sslctx'].set_cipher_list("ADH-AES128-SHA:@SECLEVEL=0")
        else:
            ctx['sslctx'].set_cipher_list("ADH-AES128-SHA")
        ctx['conn'] = OpenSSL.SSL.Connection(ctx['sslctx'], None)
        ctx['conn'].set_accept_state()
        log_conn_state(ctx['conn'])
        ctx['conn'].bio_write(payload)
        try:
            ctx['conn'].do_handshake()
        except OpenSSL.SSL.WantReadError:
            pass
        log_conn_state(ctx['conn'])
        data = ctx['conn'].bio_read(4096)
        log_conn_state(ctx['conn'])
        return struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                           4 + 1 + 1 + len(data),
                           EAP_TYPE_FAST, 0x01) + data

    def process_clientkeyexchange(ctx, payload, appl_data):
        logger.info("Process ClientKeyExchange")
        log_conn_state(ctx['conn'])
        ctx['conn'].bio_write(payload)
        try:
            ctx['conn'].do_handshake()
        except OpenSSL.SSL.WantReadError:
            pass
        ctx['conn'].send(appl_data)
        log_conn_state(ctx['conn'])
        data = ctx['conn'].bio_read(4096)
        log_conn_state(ctx['conn'])
        return struct.pack(">BBHBB", EAP_CODE_REQUEST, ctx['id'],
                           4 + 1 + 1 + len(data),
                           EAP_TYPE_FAST, 0x01) + data

    def eap_handler(ctx, req):
        logger.info("eap_handler - RX " + binascii.hexlify(req).decode())
        if 'num' not in ctx:
            ctx['num'] = 0
        ctx['num'] = ctx['num'] + 1
        if 'id' not in ctx:
            ctx['id'] = 1
        ctx['id'] = (ctx['id'] + 1) % 256
        idx = 0

        global eap_fast_proto_ctx
        eap_fast_proto_ctx = ctx
        ctx['test_done'] = False
        logger.debug("ctx['num']=%d" % ctx['num'])

        idx += 1
        if ctx['num'] == idx:
            return eap_fast_start(ctx)
        idx += 1
        if ctx['num'] == idx:
            return process_clienthello(ctx, req[6:])
        idx += 1
        if ctx['num'] == idx:
            if not test_failure:
                ctx['test_done'] = True
            return process_clientkeyexchange(ctx, req[6:], test_payload)
        idx += 1
        if ctx['num'] == idx:
            ctx['test_done'] = True
            return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

        logger.info("Past last test case")
        return struct.pack(">BBH", EAP_CODE_FAILURE, ctx['id'], 4)

    srv = start_radius_server(eap_handler)
    try:
        dev[0].connect("eap-test", key_mgmt="WPA-EAP", scan_freq="2412",
                       eap="FAST", anonymous_identity="FAST",
                       identity="user", password="password",
                       ca_cert="auth_serv/ca.pem", phase2="auth=MSCHAPV2",
                       phase1="fast_provisioning=1",
                       pac_file="blob://fast_pac_proto",
                       wait_connect=False)
        ev = dev[0].wait_event(["CTRL-EVENT-EAP-METHOD"], timeout=5)
        if ev is None:
            raise Exception("Could not start EAP-FAST")
        dev[0].dump_monitor()
        ok = False
        for i in range(100):
            if eap_fast_proto_ctx:
                if eap_fast_proto_ctx['test_done']:
                    ok = True
                    break
            time.sleep(0.05)
        time.sleep(0.1)
        dev[0].request("REMOVE_NETWORK all")
        dev[0].wait_disconnected()
        if not ok:
            raise Exception("EAP-FAST TLS exchange did not complete")
        for i in range(3):
            dev[i].dump_monitor()
    finally:
        stop_radius_server(srv)

def test_eap_fast_proto_phase2(dev, apdev):
    """EAP-FAST Phase 2 protocol testing"""
    if not openssl_imported:
        raise HwsimSkip("OpenSSL python method not available")
    check_eap_capa(dev[0], "FAST")
    hapd = start_ap(apdev[0])
    dev[0].scan_for_bss(hapd.own_addr(), freq=2412)

    tests = [("Too short Phase 2 TLV frame (len=3)",
              "ABC",
              False),
             ("EAP-FAST: TLV overflow",
              struct.pack(">HHB", 0, 2, 0xff),
              False),
             ("EAP-FAST: Unknown TLV (optional and mandatory)",
              struct.pack(">HHB", 0, 1, 0xff) +
              struct.pack(">HHB", EAP_TLV_TYPE_MANDATORY, 1, 0xff),
              True),
             ("EAP-FAST: More than one EAP-Payload TLV in the message",
              struct.pack(">HHBHHB",
                          EAP_TLV_EAP_PAYLOAD_TLV, 1, 0xff,
                          EAP_TLV_EAP_PAYLOAD_TLV, 1, 0xff),
              True),
             ("EAP-FAST: Unknown Result 255 and More than one Result TLV in the message",
              struct.pack(">HHHHHH",
                          EAP_TLV_RESULT_TLV, 2, 0xff,
                          EAP_TLV_RESULT_TLV, 2, 0xff),
              True),
             ("EAP-FAST: Too short Result TLV",
              struct.pack(">HHB", EAP_TLV_RESULT_TLV, 1, 0xff),
              True),
             ("EAP-FAST: Unknown Intermediate Result 255 and More than one Intermediate-Result TLV in the message",
              struct.pack(">HHHHHH",
                          EAP_TLV_INTERMEDIATE_RESULT_TLV, 2, 0xff,
                          EAP_TLV_INTERMEDIATE_RESULT_TLV, 2, 0xff),
              True),
             ("EAP-FAST: Too short Intermediate-Result TLV",
              struct.pack(">HHB", EAP_TLV_INTERMEDIATE_RESULT_TLV, 1, 0xff),
              True),
             ("EAP-FAST: More than one Crypto-Binding TLV in the message",
              struct.pack(">HH", EAP_TLV_CRYPTO_BINDING_TLV, 60) + 60*b'A' +
              struct.pack(">HH", EAP_TLV_CRYPTO_BINDING_TLV, 60) + 60*b'A',
              True),
             ("EAP-FAST: Too short Crypto-Binding TLV",
              struct.pack(">HHB", EAP_TLV_CRYPTO_BINDING_TLV, 1, 0xff),
              True),
             ("EAP-FAST: More than one Request-Action TLV in the message",
              struct.pack(">HHBBHHBB",
                          EAP_TLV_REQUEST_ACTION_TLV, 2, 0xff, 0xff,
                          EAP_TLV_REQUEST_ACTION_TLV, 2, 0xff, 0xff),
              True),
             ("EAP-FAST: Too short Request-Action TLV",
              struct.pack(">HHB", EAP_TLV_REQUEST_ACTION_TLV, 1, 0xff),
              True),
             ("EAP-FAST: More than one PAC TLV in the message",
              struct.pack(">HHBHHB",
                          EAP_TLV_PAC_TLV, 1, 0xff,
                          EAP_TLV_PAC_TLV, 1, 0xff),
              True),
             ("EAP-FAST: Too short EAP Payload TLV (Len=3)",
              struct.pack(">HH3B",
                          EAP_TLV_EAP_PAYLOAD_TLV, 3, 0, 0, 0),
              False),
             ("EAP-FAST: Too short Phase 2 request (Len=0)",
              struct.pack(">HHBBH",
                          EAP_TLV_EAP_PAYLOAD_TLV, 4,
                          EAP_CODE_REQUEST, 0, 0),
              False),
             ("EAP-FAST: EAP packet overflow in EAP Payload TLV",
              struct.pack(">HHBBH",
                          EAP_TLV_EAP_PAYLOAD_TLV, 4,
                          EAP_CODE_REQUEST, 0, 4 + 1),
              False),
             ("EAP-FAST: Unexpected code=0 in Phase 2 EAP header",
              struct.pack(">HHBBH",
                          EAP_TLV_EAP_PAYLOAD_TLV, 4,
                          0, 0, 0),
              False),
             ("EAP-FAST: PAC TLV without Result TLV acknowledging success",
              struct.pack(">HHB", EAP_TLV_PAC_TLV, 1, 0xff),
              True),
             ("EAP-FAST: PAC TLV does not include all the required fields",
              struct.pack(">HHH", EAP_TLV_RESULT_TLV, 2,
                          EAP_TLV_RESULT_SUCCESS) +
              struct.pack(">HHB", EAP_TLV_PAC_TLV, 1, 0xff),
              True),
             ("EAP-FAST: Invalid PAC-Key length 0, Ignored unknown PAC type 0, and PAC TLV overrun (type=0 len=2 left=1)",
              struct.pack(">HHH", EAP_TLV_RESULT_TLV, 2,
                          EAP_TLV_RESULT_SUCCESS) +
              struct.pack(">HHHHHHHHB", EAP_TLV_PAC_TLV, 4 + 4 + 5,
                          PAC_TYPE_PAC_KEY, 0, 0, 0, 0, 2, 0),
              True),
             ("EAP-FAST: PAC-Info does not include all the required fields",
              struct.pack(">HHH", EAP_TLV_RESULT_TLV, 2,
                          EAP_TLV_RESULT_SUCCESS) +
              struct.pack(">HHHHHHHH", EAP_TLV_PAC_TLV, 4 + 4 + 4 + 32,
                          PAC_TYPE_PAC_OPAQUE, 0,
                          PAC_TYPE_PAC_INFO, 0,
                          PAC_TYPE_PAC_KEY, 32) + 32*b'A',
              True),
             ("EAP-FAST: Invalid CRED_LIFETIME length, Ignored unknown PAC-Info type 0, and Invalid PAC-Type length 1",
              struct.pack(">HHH", EAP_TLV_RESULT_TLV, 2,
                          EAP_TLV_RESULT_SUCCESS) +
              struct.pack(">HHHHHHHHHHHHBHH", EAP_TLV_PAC_TLV, 4 + 4 + 13 + 4 + 32,
                          PAC_TYPE_PAC_OPAQUE, 0,
                          PAC_TYPE_PAC_INFO, 13, PAC_TYPE_CRED_LIFETIME, 0,
                          0, 0, PAC_TYPE_PAC_TYPE, 1, 0,
                          PAC_TYPE_PAC_KEY, 32) + 32*b'A',
              True),
             ("EAP-FAST: Unsupported PAC-Type 0",
              struct.pack(">HHH", EAP_TLV_RESULT_TLV, 2,
                          EAP_TLV_RESULT_SUCCESS) +
              struct.pack(">HHHHHHHHHHH", EAP_TLV_PAC_TLV, 4 + 4 + 6 + 4 + 32,
                          PAC_TYPE_PAC_OPAQUE, 0,
                          PAC_TYPE_PAC_INFO, 6, PAC_TYPE_PAC_TYPE, 2, 0,
                          PAC_TYPE_PAC_KEY, 32) + 32*b'A',
              True),
             ("EAP-FAST: PAC-Info overrun (type=0 len=2 left=1)",
              struct.pack(">HHH", EAP_TLV_RESULT_TLV, 2,
                          EAP_TLV_RESULT_SUCCESS) +
              struct.pack(">HHHHHHHHBHH", EAP_TLV_PAC_TLV, 4 + 4 + 5 + 4 + 32,
                          PAC_TYPE_PAC_OPAQUE, 0,
                          PAC_TYPE_PAC_INFO, 5, 0, 2, 1,
                          PAC_TYPE_PAC_KEY, 32) + 32*b'A',
              True),
             ("EAP-FAST: Valid PAC",
              struct.pack(">HHH", EAP_TLV_RESULT_TLV, 2,
                          EAP_TLV_RESULT_SUCCESS) +
              struct.pack(">HHHHHHHHBHHBHH", EAP_TLV_PAC_TLV,
                          4 + 4 + 10 + 4 + 32,
                          PAC_TYPE_PAC_OPAQUE, 0,
                          PAC_TYPE_PAC_INFO, 10, PAC_TYPE_A_ID, 1, 0x41,
                          PAC_TYPE_A_ID_INFO, 1, 0x42,
                          PAC_TYPE_PAC_KEY, 32) + 32*b'A',
              True),
             ("EAP-FAST: Invalid version/subtype in Crypto-Binding TLV",
              struct.pack(">HH", EAP_TLV_CRYPTO_BINDING_TLV, 60) + 60*b'A',
              True)]
    for title, payload, failure in tests:
        logger.info("Phase 2 test: " + title)
        run_eap_fast_phase2(dev, payload, failure)

def test_eap_fast_tlv_nak_oom(dev, apdev):
    """EAP-FAST Phase 2 TLV NAK OOM"""
    if not openssl_imported:
        raise HwsimSkip("OpenSSL python method not available")
    check_eap_capa(dev[0], "FAST")
    hapd = start_ap(apdev[0])
    dev[0].scan_for_bss(hapd.own_addr(), freq=2412)

    with alloc_fail(dev[0], 1, "eap_fast_tlv_nak"):
        run_eap_fast_phase2(dev, struct.pack(">HHB", EAP_TLV_TYPE_MANDATORY,
                                             1, 0xff), False)
