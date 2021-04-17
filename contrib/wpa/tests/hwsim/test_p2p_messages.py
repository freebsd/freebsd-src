# P2P protocol tests for various messages
# Copyright (c) 2014-2015, Jouni Malinen <j@w1.fi>
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

from remotehost import remote_compatible
import binascii
import struct
import time
import logging
logger = logging.getLogger()

import hostapd
from p2p_utils import *
from test_gas import anqp_adv_proto

def ie_ssid(ssid):
    return struct.pack("<BB", WLAN_EID_SSID, len(ssid)) + ssid.encode()

def ie_supp_rates():
    return struct.pack("<BBBBBBBBBB", WLAN_EID_SUPP_RATES, 8,
                       2*6, 2*9, 2*12, 2*18, 2*24, 2*36, 2*48, 2*54)

def ie_p2p(attrs):
    return struct.pack("<BBBBBB", WLAN_EID_VENDOR_SPECIFIC, 4 + len(attrs),
                       0x50, 0x6f, 0x9a, 9) + attrs

def ie_wsc(attrs):
    return struct.pack("<BBBBBB", WLAN_EID_VENDOR_SPECIFIC, 4 + len(attrs),
                       0x00, 0x50, 0xf2, 4) + attrs

def wsc_attr_config_methods(methods=0):
    return struct.pack(">HHH", WSC_ATTR_CONFIG_METHODS, 2, methods)

def p2p_attr_status(status=P2P_SC_SUCCESS):
    return struct.pack("<BHB", P2P_ATTR_STATUS, 1, status)

def p2p_attr_minor_reason_code(code=0):
    return struct.pack("<BHB", P2P_ATTR_MINOR_REASON_CODE, 1, code)

def p2p_attr_capability(dev_capab=0, group_capab=0):
    return struct.pack("<BHBB", P2P_ATTR_CAPABILITY, 2, dev_capab, group_capab)

def p2p_attr_device_id(addr):
    val = struct.unpack('6B', binascii.unhexlify(addr.replace(':', '')))
    t = (P2P_ATTR_DEVICE_ID, 6) + val
    return struct.pack('<BH6B', *t)

def p2p_attr_go_intent(go_intent=0, tie_breaker=0):
    return struct.pack("<BHB", P2P_ATTR_GROUP_OWNER_INTENT, 1,
                       (go_intent << 1) | (tie_breaker & 0x01))

def p2p_attr_config_timeout(go_config_timeout=0, client_config_timeout=0):
    return struct.pack("<BHBB", P2P_ATTR_CONFIGURATION_TIMEOUT, 2,
                       go_config_timeout, client_config_timeout)

def p2p_attr_listen_channel(op_class=81, chan=1):
    return struct.pack("<BHBBBBB", P2P_ATTR_LISTEN_CHANNEL, 5,
                       0x58, 0x58, 0x04, op_class, chan)

def p2p_attr_group_bssid(addr):
    val = struct.unpack('6B', binascii.unhexlify(addr.replace(':', '')))
    t = (P2P_ATTR_GROUP_BSSID, 6) + val
    return struct.pack('<BH6B', *t)

def p2p_attr_ext_listen_timing(period=0, interval=0):
    return struct.pack("<BHHH", P2P_ATTR_EXT_LISTEN_TIMING, 4, period, interval)

def p2p_attr_intended_interface_addr(addr):
    val = struct.unpack('6B', binascii.unhexlify(addr.replace(':', '')))
    t = (P2P_ATTR_INTENDED_INTERFACE_ADDR, 6) + val
    return struct.pack('<BH6B', *t)

def p2p_attr_manageability(bitmap=0):
    return struct.pack("<BHB", P2P_ATTR_MANAGEABILITY, 1, bitmap)

def p2p_attr_channel_list():
    return struct.pack("<BH3BBB11B", P2P_ATTR_CHANNEL_LIST, 16,
                       0x58, 0x58, 0x04,
                       81, 11, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11)

def p2p_attr_device_info(addr, name="Test", config_methods=0, dev_type="00010050F2040001"):
    val = struct.unpack('6B', binascii.unhexlify(addr.replace(':', '')))
    val2 = struct.unpack('8B', binascii.unhexlify(dev_type))
    t = (P2P_ATTR_DEVICE_INFO, 6 + 2 + 8 + 1 + 4 + len(name)) + val
    t2 = val2 + (0,)
    return struct.pack("<BH6B", *t) + struct.pack(">H", config_methods) + struct.pack("8BB", *t2) + struct.pack('>HH', 0x1011, len(name)) + name.encode()

def p2p_attr_group_id(addr, ssid):
    val = struct.unpack('6B', binascii.unhexlify(addr.replace(':', '')))
    t = (P2P_ATTR_GROUP_ID, 6 + len(ssid)) + val
    return struct.pack('<BH6B', *t) + ssid.encode()

def p2p_attr_operating_channel(op_class=81, chan=1):
    return struct.pack("<BHBBBBB", P2P_ATTR_OPERATING_CHANNEL, 5,
                       0x58, 0x58, 0x04, op_class, chan)

def p2p_attr_invitation_flags(bitmap=0):
    return struct.pack("<BHB", P2P_ATTR_INVITATION_FLAGS, 1, bitmap)

def p2p_hdr_helper(dst, src, type=None, dialog_token=1, req=True):
    msg = {}
    msg['fc'] = MGMT_SUBTYPE_ACTION << 4
    msg['da'] = dst
    msg['sa'] = src
    if req:
        msg['bssid'] = dst
    else:
        msg['bssid'] = src
    msg['payload'] = struct.pack("<BBBBBB",
                                 ACTION_CATEG_PUBLIC, 9, 0x50, 0x6f, 0x9a, 9)
    if type is not None:
        msg['payload'] += struct.pack("<B", type)
        if dialog_token:
            msg['payload'] += struct.pack("<B", dialog_token)
    return msg

def p2p_hdr(dst, src, type=None, dialog_token=1):
    return p2p_hdr_helper(dst, src, type, dialog_token, True)

def p2p_hdr_resp(dst, src, type=None, dialog_token=1):
    return p2p_hdr_helper(dst, src, type, dialog_token, False)

def start_p2p(dev, apdev):
    addr0 = dev[0].p2p_dev_addr()
    dev[0].p2p_listen()
    dev[1].p2p_find(social=True)
    ev = dev[1].wait_global_event(["P2P-DEVICE-FOUND"], timeout=5)
    if ev is None:
        raise Exception("Device discovery timed out")
    dev[1].p2p_stop_find()
    peer = dev[1].get_peer(addr0)

    bssid = apdev[0]['bssid']
    params = {'ssid': "test", 'beacon_int': "2000"}
    if peer['listen_freq'] == "2412":
        params['channel'] = '1'
    elif peer['listen_freq'] == "2437":
        params['channel'] = '6'
    elif peer['listen_freq'] == "2462":
        params['channel'] = '11'
    hapd = hostapd.add_ap(apdev[0], params)
    hapd.set("ext_mgmt_frame_handling", "1")
    return addr0, bssid, hapd, int(params['channel'])

def p2p_probe(hapd, src, chan=1):
    msg = {}
    msg['fc'] = MGMT_SUBTYPE_PROBE_REQ << 4
    msg['da'] = "ff:ff:ff:ff:ff:ff"
    msg['sa'] = src
    msg['bssid'] = "ff:ff:ff:ff:ff:ff"
    attrs = p2p_attr_listen_channel(chan=chan)
    msg['payload'] = ie_ssid("DIRECT-") + ie_supp_rates() + ie_p2p(attrs)
    hapd.mgmt_tx(msg)

def parse_p2p_public_action(payload):
    pos = payload
    (category, action) = struct.unpack('BB', pos[0:2])
    if category != ACTION_CATEG_PUBLIC:
        return None
    if action != 9:
        return None
    pos = pos[2:]
    (oui1, oui2, oui3, subtype) = struct.unpack('BBBB', pos[0:4])
    if oui1 != 0x50 or oui2 != 0x6f or oui3 != 0x9a or subtype != 9:
        return None
    pos = pos[4:]
    (subtype, dialog_token) = struct.unpack('BB', pos[0:2])
    p2p = {}
    p2p['subtype'] = subtype
    p2p['dialog_token'] = dialog_token
    pos = pos[2:]
    p2p['elements'] = pos
    while len(pos) > 2:
        (id, elen) = struct.unpack('BB', pos[0:2])
        pos = pos[2:]
        if elen > len(pos):
            raise Exception("Truncated IE in P2P Public Action frame (elen=%d left=%d)" % (elen, len(pos)))
        if id == WLAN_EID_VENDOR_SPECIFIC:
            if elen < 4:
                raise Exception("Too short vendor specific IE in P2P Public Action frame (elen=%d)" % elen)
            (oui1, oui2, oui3, subtype) = struct.unpack('BBBB', pos[0:4])
            if oui1 == 0x50 and oui2 == 0x6f and oui3 == 0x9a and subtype == 9:
                if 'p2p' in p2p:
                    p2p['p2p'] += pos[4:elen]
                else:
                    p2p['p2p'] = pos[4:elen]
            if oui1 == 0x00 and oui2 == 0x50 and oui3 == 0xf2 and subtype == 4:
                p2p['wsc'] = pos[4:elen]
        pos = pos[elen:]
    if len(pos) > 0:
        raise Exception("Invalid element in P2P Public Action frame")

    if 'p2p' in p2p:
        p2p['p2p_attrs'] = {}
        pos = p2p['p2p']
        while len(pos) >= 3:
            (id, alen) = struct.unpack('<BH', pos[0:3])
            pos = pos[3:]
            if alen > len(pos):
                logger.info("P2P payload: " + binascii.hexlify(p2p['p2p']))
                raise Exception("Truncated P2P attribute in P2P Public Action frame (alen=%d left=%d p2p-payload=%d)" % (alen, len(pos), len(p2p['p2p'])))
            p2p['p2p_attrs'][id] = pos[0:alen]
            pos = pos[alen:]
        if P2P_ATTR_STATUS in p2p['p2p_attrs']:
            p2p['p2p_status'] = struct.unpack('B', p2p['p2p_attrs'][P2P_ATTR_STATUS])[0]

    if 'wsc' in p2p:
        p2p['wsc_attrs'] = {}
        pos = p2p['wsc']
        while len(pos) >= 4:
            (id, alen) = struct.unpack('>HH', pos[0:4])
            pos = pos[4:]
            if alen > len(pos):
                logger.info("WSC payload: " + binascii.hexlify(p2p['wsc']))
                raise Exception("Truncated WSC attribute in P2P Public Action frame (alen=%d left=%d wsc-payload=%d)" % (alen, len(pos), len(p2p['wsc'])))
            p2p['wsc_attrs'][id] = pos[0:alen]
            pos = pos[alen:]

    return p2p

@remote_compatible
def test_p2p_msg_empty(dev, apdev):
    """P2P protocol test: empty P2P Public Action frame"""
    dst, src, hapd, channel = start_p2p(dev, apdev)
    msg = p2p_hdr(dst, src)
    hapd.mgmt_tx(msg)

@remote_compatible
def test_p2p_msg_long_ssid(dev, apdev):
    """P2P protocol test: Too long SSID in P2P Public Action frame"""
    dst, src, hapd, channel = start_p2p(dev, apdev)

    msg = p2p_hdr(dst, src, type=P2P_INVITATION_REQ, dialog_token=1)
    attrs = p2p_attr_config_timeout()
    attrs += p2p_attr_invitation_flags()
    attrs += p2p_attr_operating_channel()
    attrs += p2p_attr_group_bssid(src)
    attrs += p2p_attr_channel_list()
    attrs += p2p_attr_group_id(src, 'DIRECT-foo')
    attrs += p2p_attr_device_info(src, config_methods=0x0108)
    msg['payload'] += ie_p2p(attrs)
    msg['payload'] += ie_ssid(255 * 'A')
    hapd.mgmt_tx(msg)
    ev = dev[0].wait_global_event(["P2P-DEVICE-FOUND"], timeout=5)
    if ev is None:
        raise Exception("Timeout on device found event")

@remote_compatible
def test_p2p_msg_long_dev_name(dev, apdev):
    """P2P protocol test: Too long Device Name in P2P Public Action frame"""
    dst, src, hapd, channel = start_p2p(dev, apdev)

    msg = p2p_hdr(dst, src, type=P2P_INVITATION_REQ, dialog_token=1)
    attrs = p2p_attr_config_timeout()
    attrs += p2p_attr_invitation_flags()
    attrs += p2p_attr_operating_channel()
    attrs += p2p_attr_group_bssid(src)
    attrs += p2p_attr_channel_list()
    attrs += p2p_attr_group_id(src, 'DIRECT-foo')
    attrs += p2p_attr_device_info(src, config_methods=0x0108,
                                  name="123456789012345678901234567890123")
    msg['payload'] += ie_p2p(attrs)
    hapd.mgmt_tx(msg)
    ev = dev[0].wait_event(["P2P-DEVICE-FOUND"], timeout=0.1)
    if ev is not None:
        raise Exception("Unexpected device found event")

def test_p2p_msg_invitation_req(dev, apdev):
    """P2P protocol tests for invitation request processing"""
    dst, src, hapd, channel = start_p2p(dev, apdev)

    # Empty P2P Invitation Request (missing dialog token)
    msg = p2p_hdr(dst, src, type=P2P_INVITATION_REQ, dialog_token=None)
    hapd.mgmt_tx(msg)
    dialog_token = 0

    # Various p2p_parse() failure cases due to invalid attributes

    # Too short attribute header
    dialog_token += 1
    msg = p2p_hdr(dst, src, type=P2P_INVITATION_REQ, dialog_token=dialog_token)
    attrs = struct.pack("<BB", P2P_ATTR_CAPABILITY, 0)
    msg['payload'] += ie_p2p(attrs)
    hapd.mgmt_tx(msg)

    # Minimal attribute underflow
    dialog_token += 1
    msg = p2p_hdr(dst, src, type=P2P_INVITATION_REQ, dialog_token=dialog_token)
    attrs = struct.pack("<BH", P2P_ATTR_CAPABILITY, 1)
    msg['payload'] += ie_p2p(attrs)
    hapd.mgmt_tx(msg)

    # Large attribute underflow
    dialog_token += 1
    msg = p2p_hdr(dst, src, type=P2P_INVITATION_REQ, dialog_token=dialog_token)
    attrs = struct.pack("<BHB", P2P_ATTR_CAPABILITY, 0xffff, 1)
    msg['payload'] += ie_p2p(attrs)
    hapd.mgmt_tx(msg)

    # Too short Capability attribute
    dialog_token += 1
    msg = p2p_hdr(dst, src, type=P2P_INVITATION_REQ, dialog_token=dialog_token)
    attrs = struct.pack("<BHB", P2P_ATTR_CAPABILITY, 1, 0)
    msg['payload'] += ie_p2p(attrs)
    hapd.mgmt_tx(msg)

    # Too short Device ID attribute
    dialog_token += 1
    msg = p2p_hdr(dst, src, type=P2P_INVITATION_REQ, dialog_token=dialog_token)
    val = struct.unpack('5B', binascii.unhexlify("1122334455"))
    t = (P2P_ATTR_DEVICE_ID, 5) + val
    attrs = struct.pack('<BH5B', *t)
    msg['payload'] += ie_p2p(attrs)
    hapd.mgmt_tx(msg)

    # Too short GO Intent attribute
    dialog_token += 1
    msg = p2p_hdr(dst, src, type=P2P_INVITATION_REQ, dialog_token=dialog_token)
    attrs = struct.pack("<BH", P2P_ATTR_GROUP_OWNER_INTENT, 0)
    msg['payload'] += ie_p2p(attrs)
    hapd.mgmt_tx(msg)

    # Too short Status attribute
    dialog_token += 1
    msg = p2p_hdr(dst, src, type=P2P_INVITATION_REQ, dialog_token=dialog_token)
    attrs = struct.pack("<BH", P2P_ATTR_STATUS, 0)
    msg['payload'] += ie_p2p(attrs)
    hapd.mgmt_tx(msg)

    # null Listen channel and too short Listen Channel attribute
    dialog_token += 1
    msg = p2p_hdr(dst, src, type=P2P_INVITATION_REQ, dialog_token=dialog_token)
    attrs = struct.pack("<BH", P2P_ATTR_LISTEN_CHANNEL, 0)
    attrs += struct.pack("<BHB", P2P_ATTR_LISTEN_CHANNEL, 1, 0)
    msg['payload'] += ie_p2p(attrs)
    hapd.mgmt_tx(msg)

    # null Operating channel and too short Operating Channel attribute
    dialog_token += 1
    msg = p2p_hdr(dst, src, type=P2P_INVITATION_REQ, dialog_token=dialog_token)
    attrs = struct.pack("<BH", P2P_ATTR_OPERATING_CHANNEL, 0)
    attrs += struct.pack("<BHB", P2P_ATTR_OPERATING_CHANNEL, 1, 0)
    msg['payload'] += ie_p2p(attrs)
    hapd.mgmt_tx(msg)

    # Too short Channel List attribute
    dialog_token += 1
    msg = p2p_hdr(dst, src, type=P2P_INVITATION_REQ, dialog_token=dialog_token)
    attrs = struct.pack("<BHBB", P2P_ATTR_CHANNEL_LIST, 2, 1, 2)
    msg['payload'] += ie_p2p(attrs)
    hapd.mgmt_tx(msg)

    # Too short Device Info attribute
    dialog_token += 1
    msg = p2p_hdr(dst, src, type=P2P_INVITATION_REQ, dialog_token=dialog_token)
    attrs = struct.pack("<BHBB", P2P_ATTR_DEVICE_INFO, 2, 1, 2)
    msg['payload'] += ie_p2p(attrs)
    hapd.mgmt_tx(msg)

    # Truncated Secondary Device Types in Device Info attribute
    dialog_token += 1
    msg = p2p_hdr(dst, src, type=P2P_INVITATION_REQ, dialog_token=dialog_token)
    attrs = struct.pack("<BH6BH8BB", P2P_ATTR_DEVICE_INFO, 6 + 2 + 8 + 1,
                        0, 0, 0, 0, 0, 0,
                        0,
                        0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x11, 0x22,
                        255)
    msg['payload'] += ie_p2p(attrs)
    hapd.mgmt_tx(msg)

    # Missing Device Name in Device Info attribute
    dialog_token += 1
    msg = p2p_hdr(dst, src, type=P2P_INVITATION_REQ, dialog_token=dialog_token)
    attrs = struct.pack("<BH6BH8BB8B", P2P_ATTR_DEVICE_INFO, 6 + 2 + 8 + 1 + 8,
                        0, 0, 0, 0, 0, 0,
                        0,
                        0, 0, 0, 0, 0, 0, 0, 0,
                        1,
                        1, 2, 3, 4, 5, 6, 7, 8)
    msg['payload'] += ie_p2p(attrs)
    hapd.mgmt_tx(msg)

    # Invalid Device Name header in Device Info attribute
    dialog_token += 1
    msg = p2p_hdr(dst, src, type=P2P_INVITATION_REQ, dialog_token=dialog_token)
    attrs = struct.pack("<BH6BH8BB8B4B", P2P_ATTR_DEVICE_INFO, 6 + 2 + 8 + 1 + 8 + 4,
                        0, 0, 0, 0, 0, 0,
                        0,
                        0, 0, 0, 0, 0, 0, 0, 0,
                        1,
                        1, 2, 3, 4, 5, 6, 7, 8,
                        0x11, 0x12, 0, 0)
    msg['payload'] += ie_p2p(attrs)
    hapd.mgmt_tx(msg)

    # Invalid Device Name header length in Device Info attribute
    dialog_token += 1
    msg = p2p_hdr(dst, src, type=P2P_INVITATION_REQ, dialog_token=dialog_token)
    attrs = struct.pack("<BH6BH8BB8B4B", P2P_ATTR_DEVICE_INFO, 6 + 2 + 8 + 1 + 8 + 4,
                        0, 0, 0, 0, 0, 0,
                        0,
                        0, 0, 0, 0, 0, 0, 0, 0,
                        1,
                        1, 2, 3, 4, 5, 6, 7, 8,
                        0x10, 0x11, 0xff, 0xff)
    msg['payload'] += ie_p2p(attrs)
    hapd.mgmt_tx(msg)

    # Invalid Device Name header length in Device Info attribute
    dialog_token += 1
    msg = p2p_hdr(dst, src, type=P2P_INVITATION_REQ, dialog_token=dialog_token)
    devname = b'A'
    attrs = struct.pack("<BH6BH8BB8B4B", P2P_ATTR_DEVICE_INFO, 6 + 2 + 8 + 1 + 8 + 4 + len(devname),
                        0, 0, 0, 0, 0, 0,
                        0,
                        0, 0, 0, 0, 0, 0, 0, 0,
                        1,
                        1, 2, 3, 4, 5, 6, 7, 8,
                        0x10, 0x11, 0, len(devname) + 1) + devname
    msg['payload'] += ie_p2p(attrs)
    hapd.mgmt_tx(msg)

    # Device Name filtering and too long Device Name in Device Info attribute
    dialog_token += 1
    msg = p2p_hdr(dst, src, type=P2P_INVITATION_REQ, dialog_token=dialog_token)
    attrs = struct.pack("<BH6BH8BB8B4B4B", P2P_ATTR_DEVICE_INFO, 6 + 2 + 8 + 1 + 8 + 4 + 4,
                        0, 0, 0, 0, 0, 0,
                        0,
                        0, 0, 0, 0, 0, 0, 0, 0,
                        1,
                        1, 2, 3, 4, 5, 6, 7, 8,
                        0x10, 0x11, 0, 4,
                        64, 9, 0, 64)
    devname = b'123456789012345678901234567890123'
    attrs += struct.pack("<BH6BH8BB8B4B", P2P_ATTR_DEVICE_INFO, 6 + 2 + 8 + 1 + 8 + 4 + len(devname),
                         0, 0, 0, 0, 0, 0,
                         0,
                         0, 0, 0, 0, 0, 0, 0, 0,
                         1,
                         1, 2, 3, 4, 5, 6, 7, 8,
                         0x10, 0x11, 0, len(devname)) + devname
    msg['payload'] += ie_p2p(attrs)
    hapd.mgmt_tx(msg)

    # Too short Configuration Timeout attribute
    dialog_token += 1
    msg = p2p_hdr(dst, src, type=P2P_INVITATION_REQ, dialog_token=dialog_token)
    attrs = struct.pack("<BHB", P2P_ATTR_CONFIGURATION_TIMEOUT, 1, 1)
    msg['payload'] += ie_p2p(attrs)
    hapd.mgmt_tx(msg)

    # Too short Intended P2P Interface Address attribute
    dialog_token += 1
    msg = p2p_hdr(dst, src, type=P2P_INVITATION_REQ, dialog_token=dialog_token)
    attrs = struct.pack("<BHB", P2P_ATTR_INTENDED_INTERFACE_ADDR, 1, 1)
    msg['payload'] += ie_p2p(attrs)
    hapd.mgmt_tx(msg)

    # Too short P2P Group BSSID attribute
    dialog_token += 1
    msg = p2p_hdr(dst, src, type=P2P_INVITATION_REQ, dialog_token=dialog_token)
    attrs = struct.pack("<BHB", P2P_ATTR_GROUP_BSSID, 1, 1)
    msg['payload'] += ie_p2p(attrs)
    hapd.mgmt_tx(msg)

    # Too short P2P Group ID attribute
    dialog_token += 1
    msg = p2p_hdr(dst, src, type=P2P_INVITATION_REQ, dialog_token=dialog_token)
    attrs = struct.pack("<BHB", P2P_ATTR_GROUP_ID, 1, 1)
    msg['payload'] += ie_p2p(attrs)
    hapd.mgmt_tx(msg)

    # Too long P2P Group ID attribute
    dialog_token += 1
    msg = p2p_hdr(dst, src, type=P2P_INVITATION_REQ, dialog_token=dialog_token)
    attrs = struct.pack("<BH6B", P2P_ATTR_GROUP_ID, 6 + 33, 0, 0, 0, 0, 0, 0) + b"123456789012345678901234567890123"
    msg['payload'] += ie_p2p(attrs)
    hapd.mgmt_tx(msg)

    # Too short Invitation Flags attribute
    dialog_token += 1
    msg = p2p_hdr(dst, src, type=P2P_INVITATION_REQ, dialog_token=dialog_token)
    attrs = struct.pack("<BH", P2P_ATTR_INVITATION_FLAGS, 0)
    msg['payload'] += ie_p2p(attrs)
    hapd.mgmt_tx(msg)

    # Valid and too short Manageability attribute
    dialog_token += 1
    msg = p2p_hdr(dst, src, type=P2P_INVITATION_REQ, dialog_token=dialog_token)
    attrs = p2p_attr_manageability()
    attrs += struct.pack("<BH", P2P_ATTR_MANAGEABILITY, 0)
    msg['payload'] += ie_p2p(attrs)
    hapd.mgmt_tx(msg)

    # Too short NoA attribute
    dialog_token += 1
    msg = p2p_hdr(dst, src, type=P2P_INVITATION_REQ, dialog_token=dialog_token)
    attrs = struct.pack("<BHB", P2P_ATTR_NOTICE_OF_ABSENCE, 1, 1)
    msg['payload'] += ie_p2p(attrs)
    hapd.mgmt_tx(msg)

    # Valid and too short Extended Listen Timing attributes
    dialog_token += 1
    msg = p2p_hdr(dst, src, type=P2P_INVITATION_REQ, dialog_token=dialog_token)
    attrs = p2p_attr_ext_listen_timing(period=100, interval=50)
    attrs += struct.pack("<BHBBB", P2P_ATTR_EXT_LISTEN_TIMING, 3, 0, 0, 0)
    msg['payload'] += ie_p2p(attrs)
    hapd.mgmt_tx(msg)

    # Valid and too short Minor Reason Code attributes
    dialog_token += 1
    msg = p2p_hdr(dst, src, type=P2P_INVITATION_REQ, dialog_token=dialog_token)
    attrs = p2p_attr_minor_reason_code(code=2)
    attrs += struct.pack("<BH", P2P_ATTR_MINOR_REASON_CODE, 0)
    msg['payload'] += ie_p2p(attrs)
    hapd.mgmt_tx(msg)

    # Unknown attribute and too short OOB GO Negotiation Channel attribute
    dialog_token += 1
    msg = p2p_hdr(dst, src, type=P2P_INVITATION_REQ, dialog_token=dialog_token)
    attrs = struct.pack("<BHB", 99, 1, 1)
    attrs += struct.pack("<BHB", P2P_ATTR_OOB_GO_NEG_CHANNEL, 1, 1)
    msg['payload'] += ie_p2p(attrs)
    hapd.mgmt_tx(msg)

    # Too short Service Hash attribute
    dialog_token += 1
    msg = p2p_hdr(dst, src, type=P2P_INVITATION_REQ, dialog_token=dialog_token)
    attrs = struct.pack("<BH5B", P2P_ATTR_SERVICE_HASH, 5, 1, 2, 3, 4, 5)
    msg['payload'] += ie_p2p(attrs)
    hapd.mgmt_tx(msg)

    # Too short Connection Capability attribute
    dialog_token += 1
    msg = p2p_hdr(dst, src, type=P2P_INVITATION_REQ, dialog_token=dialog_token)
    attrs = struct.pack("<BH", P2P_ATTR_CONNECTION_CAPABILITY, 0)
    msg['payload'] += ie_p2p(attrs)
    hapd.mgmt_tx(msg)

    # Too short Advertisement ID attribute
    dialog_token += 1
    msg = p2p_hdr(dst, src, type=P2P_INVITATION_REQ, dialog_token=dialog_token)
    attrs = struct.pack("<BH9B", P2P_ATTR_ADVERTISEMENT_ID, 9, 1, 2, 3, 4, 5,
                        6, 7, 8, 9)
    msg['payload'] += ie_p2p(attrs)
    hapd.mgmt_tx(msg)

    # Truncated and too short Service Instance attributes
    dialog_token += 1
    msg = p2p_hdr(dst, src, type=P2P_INVITATION_REQ, dialog_token=dialog_token)
    attrs = struct.pack("<BH8B", P2P_ATTR_ADVERTISED_SERVICE, 8, 1, 2, 3, 4, 5,
                        6, 2, 8)
    attrs += struct.pack("<BH7B", P2P_ATTR_ADVERTISED_SERVICE, 7, 1, 2, 3, 4, 5,
                         6, 7)
    msg['payload'] += ie_p2p(attrs)
    hapd.mgmt_tx(msg)

    # Too short Session ID attribute
    dialog_token += 1
    msg = p2p_hdr(dst, src, type=P2P_INVITATION_REQ, dialog_token=dialog_token)
    attrs = struct.pack("<BH4B", P2P_ATTR_SESSION_ID, 4, 1, 2, 3, 4)
    msg['payload'] += ie_p2p(attrs)
    hapd.mgmt_tx(msg)

    # Too short Feature Capability attribute
    dialog_token += 1
    msg = p2p_hdr(dst, src, type=P2P_INVITATION_REQ, dialog_token=dialog_token)
    attrs = struct.pack("<BH", P2P_ATTR_FEATURE_CAPABILITY, 0)
    msg['payload'] += ie_p2p(attrs)
    hapd.mgmt_tx(msg)

    # Too short Persistent Group attribute
    dialog_token += 1
    msg = p2p_hdr(dst, src, type=P2P_INVITATION_REQ, dialog_token=dialog_token)
    attrs = struct.pack("<BH5B", P2P_ATTR_PERSISTENT_GROUP, 5, 1, 2, 3, 4, 5)
    msg['payload'] += ie_p2p(attrs)
    hapd.mgmt_tx(msg)

    # Too long Persistent Group attribute
    dialog_token += 1
    msg = p2p_hdr(dst, src, type=P2P_INVITATION_REQ, dialog_token=dialog_token)
    attrs = struct.pack("<BH9L3B", P2P_ATTR_PERSISTENT_GROUP, 6 + 32 + 1,
                        1, 2, 3, 4, 5, 6, 7, 8, 9, 1, 2, 3)
    msg['payload'] += ie_p2p(attrs)
    hapd.mgmt_tx(msg)

    if hapd.mgmt_rx(timeout=0.5) is not None:
        raise Exception("Unexpected management frame received")

    dev[0].dump_monitor()
    dialog_token += 1
    msg = p2p_hdr(dst, src, type=P2P_INVITATION_REQ, dialog_token=dialog_token)
    attrs = p2p_attr_config_timeout()
    attrs += p2p_attr_invitation_flags()
    attrs += p2p_attr_operating_channel()
    attrs += p2p_attr_group_bssid(src)
    attrs += p2p_attr_channel_list()
    attrs += p2p_attr_group_id(src, "DIRECT-foo")
    attrs += p2p_attr_device_info(src, config_methods=0x0108)
    msg['payload'] += ie_p2p(attrs)
    hapd.mgmt_tx(msg)
    ev = dev[0].wait_global_event(["P2P-DEVICE-FOUND"], timeout=5)
    if ev is None:
        raise Exception("Timeout on device found event")
    ev = dev[0].wait_global_event(["P2P-INVITATION-RECEIVED"], timeout=5)
    if ev is None:
        raise Exception("Timeout on invitation event " + str(dialog_token))
    if hapd.mgmt_rx(timeout=1) is None:
        raise Exception("No invitation response " + str(dialog_token))

    time.sleep(0.1)
    dev[0].dump_monitor()
    dialog_token += 1
    msg = p2p_hdr(dst, src, type=P2P_INVITATION_REQ, dialog_token=dialog_token)
    attrs = p2p_attr_config_timeout()
    attrs += p2p_attr_invitation_flags()
    attrs += p2p_attr_operating_channel()
    attrs += p2p_attr_group_bssid(src)
    attrs += p2p_attr_channel_list()
    attrs += p2p_attr_group_id(src, "DIRECT-foo")
    attrs += p2p_attr_device_info(src, config_methods=0x0108)
    msg['payload'] += ie_p2p(attrs)
    hapd.mgmt_tx(msg)
    ev = dev[0].wait_global_event(["P2P-INVITATION-RECEIVED"], timeout=5)
    if ev is None:
        raise Exception("Timeout on invitation event " + str(dialog_token))
    if hapd.mgmt_rx(timeout=1) is None:
        raise Exception("No invitation response " + str(dialog_token))

    time.sleep(0.1)
    dev[0].dump_monitor()
    dialog_token += 1
    msg = p2p_hdr(dst, src, type=P2P_INVITATION_REQ, dialog_token=dialog_token)
    #attrs = p2p_attr_config_timeout()
    attrs = p2p_attr_invitation_flags()
    attrs += p2p_attr_operating_channel()
    attrs += p2p_attr_group_bssid(src)
    attrs += p2p_attr_channel_list()
    attrs += p2p_attr_group_id(src, "DIRECT-foo")
    attrs += p2p_attr_device_info(src, config_methods=0x0108)
    msg['payload'] += ie_p2p(attrs)
    hapd.mgmt_tx(msg)
    if hapd.mgmt_rx(timeout=1) is None:
        raise Exception("No invitation response " + str(dialog_token))

    time.sleep(0.1)
    dev[0].dump_monitor()
    dialog_token += 1
    msg = p2p_hdr(dst, src, type=P2P_INVITATION_REQ, dialog_token=dialog_token)
    attrs = p2p_attr_config_timeout()
    #attrs = p2p_attr_invitation_flags()
    attrs += p2p_attr_operating_channel()
    attrs += p2p_attr_group_bssid(src)
    attrs += p2p_attr_channel_list()
    attrs += p2p_attr_group_id(src, "DIRECT-foo")
    attrs += p2p_attr_device_info(src, config_methods=0x0108)
    msg['payload'] += ie_p2p(attrs)
    hapd.mgmt_tx(msg)
    if hapd.mgmt_rx(timeout=1) is None:
        raise Exception("No invitation response " + str(dialog_token))

    time.sleep(0.1)
    dev[0].dump_monitor()
    dialog_token += 1
    msg = p2p_hdr(dst, src, type=P2P_INVITATION_REQ, dialog_token=dialog_token)
    attrs = p2p_attr_config_timeout()
    attrs = p2p_attr_invitation_flags()
    #attrs += p2p_attr_operating_channel()
    attrs += p2p_attr_group_bssid(src)
    attrs += p2p_attr_channel_list()
    attrs += p2p_attr_group_id(src, "DIRECT-foo")
    attrs += p2p_attr_device_info(src, config_methods=0x0108)
    msg['payload'] += ie_p2p(attrs)
    hapd.mgmt_tx(msg)
    if hapd.mgmt_rx(timeout=1) is None:
        raise Exception("No invitation response " + str(dialog_token))

    time.sleep(0.1)
    dev[0].dump_monitor()
    dialog_token += 1
    msg = p2p_hdr(dst, src, type=P2P_INVITATION_REQ, dialog_token=dialog_token)
    attrs = p2p_attr_config_timeout()
    attrs = p2p_attr_invitation_flags()
    attrs += p2p_attr_operating_channel()
    #attrs += p2p_attr_group_bssid(src)
    attrs += p2p_attr_channel_list()
    attrs += p2p_attr_group_id(src, "DIRECT-foo")
    attrs += p2p_attr_device_info(src, config_methods=0x0108)
    msg['payload'] += ie_p2p(attrs)
    hapd.mgmt_tx(msg)
    if hapd.mgmt_rx(timeout=1) is None:
        raise Exception("No invitation response " + str(dialog_token))

    time.sleep(0.1)
    dev[0].dump_monitor()
    dialog_token += 1
    msg = p2p_hdr(dst, src, type=P2P_INVITATION_REQ, dialog_token=dialog_token)
    attrs = p2p_attr_config_timeout()
    attrs = p2p_attr_invitation_flags()
    attrs += p2p_attr_operating_channel()
    attrs += p2p_attr_group_bssid(src)
    #attrs += p2p_attr_channel_list()
    attrs += p2p_attr_group_id(src, "DIRECT-foo")
    attrs += p2p_attr_device_info(src, config_methods=0x0108)
    msg['payload'] += ie_p2p(attrs)
    hapd.mgmt_tx(msg)
    if hapd.mgmt_rx(timeout=1) is None:
        raise Exception("No invitation response " + str(dialog_token))

    time.sleep(0.1)
    dev[0].dump_monitor()
    dialog_token += 1
    msg = p2p_hdr(dst, src, type=P2P_INVITATION_REQ, dialog_token=dialog_token)
    attrs = p2p_attr_config_timeout()
    attrs = p2p_attr_invitation_flags()
    attrs += p2p_attr_operating_channel()
    attrs += p2p_attr_group_bssid(src)
    attrs += p2p_attr_channel_list()
    #attrs += p2p_attr_group_id(src, "DIRECT-foo")
    attrs += p2p_attr_device_info(src, config_methods=0x0108)
    msg['payload'] += ie_p2p(attrs)
    hapd.mgmt_tx(msg)
    if hapd.mgmt_rx(timeout=1) is None:
        raise Exception("No invitation response " + str(dialog_token))

    time.sleep(0.1)
    dev[0].dump_monitor()
    dialog_token += 1
    msg = p2p_hdr(dst, src, type=P2P_INVITATION_REQ, dialog_token=dialog_token)
    attrs = p2p_attr_config_timeout()
    attrs = p2p_attr_invitation_flags()
    attrs += p2p_attr_operating_channel()
    attrs += p2p_attr_group_bssid(src)
    attrs += p2p_attr_channel_list()
    attrs += p2p_attr_group_id(src, "DIRECT-foo")
    #attrs += p2p_attr_device_info(src, config_methods=0x0108)
    msg['payload'] += ie_p2p(attrs)
    hapd.mgmt_tx(msg)
    if hapd.mgmt_rx(timeout=1) is None:
        raise Exception("No invitation response " + str(dialog_token))

    time.sleep(0.1)
    dev[0].dump_monitor()
    dialog_token += 1
    msg = p2p_hdr(dst, src, type=P2P_INVITATION_REQ, dialog_token=dialog_token)
    hapd.mgmt_tx(msg)
    if hapd.mgmt_rx(timeout=1) is None:
        raise Exception("No invitation response " + str(dialog_token))

    # Unusable peer operating channel preference
    time.sleep(0.1)
    dev[0].dump_monitor()
    dialog_token += 1
    msg = p2p_hdr(dst, src, type=P2P_INVITATION_REQ, dialog_token=dialog_token)
    attrs = p2p_attr_config_timeout()
    attrs = p2p_attr_invitation_flags()
    attrs += p2p_attr_operating_channel(chan=15)
    attrs += p2p_attr_group_bssid(src)
    attrs += p2p_attr_channel_list()
    attrs += p2p_attr_group_id(src, "DIRECT-foo")
    attrs += p2p_attr_device_info(src, config_methods=0x0108)
    msg['payload'] += ie_p2p(attrs)
    hapd.mgmt_tx(msg)
    if hapd.mgmt_rx(timeout=1) is None:
        raise Exception("No invitation response " + str(dialog_token))

def test_p2p_msg_invitation_req_to_go(dev, apdev):
    """P2P protocol tests for invitation request processing on GO device"""
    res = form(dev[0], dev[1])
    dev[0].dump_monitor()
    dev[1].dump_monitor()
    addr0 = dev[0].p2p_dev_addr()
    addr1 = dev[1].p2p_dev_addr()
    peer = dev[1].get_peer(addr0)
    listen_freq = peer['listen_freq']

    if "FAIL" in dev[1].request("SET ext_mgmt_frame_handling 1"):
        raise Exception("Failed to enable external management frame handling")

    networks = dev[0].list_networks()
    if len(networks) != 1:
        raise Exception("Unexpected number of networks")
    if "[P2P-PERSISTENT]" not in networks[0]['flags']:
        raise Exception("Not the persistent group data")
    dev[0].p2p_start_go(persistent=networks[0]['id'], freq=listen_freq)

    dialog_token = 0

    # Unusable peer operating channel preference
    dialog_token += 1
    msg = p2p_hdr(addr0, addr1, type=P2P_INVITATION_REQ,
                  dialog_token=dialog_token)
    attrs = p2p_attr_config_timeout()
    attrs = p2p_attr_invitation_flags(bitmap=1)
    attrs += p2p_attr_operating_channel(chan=15)
    attrs += p2p_attr_channel_list()
    attrs += p2p_attr_group_id(res['go_dev_addr'], res['ssid'])
    attrs += p2p_attr_device_info(addr1, config_methods=0x0108)
    msg['payload'] += ie_p2p(attrs)

    mgmt_tx(dev[1], "MGMT_TX {} {} freq={} wait_time=200 no_cck=1 action={}".format(addr0, addr0, peer['listen_freq'], binascii.hexlify(msg['payload']).decode()))

    rx_msg = dev[1].mgmt_rx()
    if rx_msg is None:
        raise Exception("MGMT-RX timeout")
    p2p = parse_p2p_public_action(rx_msg['payload'])
    if p2p is None:
        raise Exception("Not a P2P Public Action frame " + str(dialog_token))
    if p2p['subtype'] != P2P_INVITATION_RESP:
        raise Exception("Unexpected subtype %d" % p2p['subtype'])
    if p2p['p2p_status'] != 0:
        raise Exception("Unexpected status %d" % p2p['p2p_status'])

    # Forced channel re-selection due to channel list
    dialog_token += 1
    msg = p2p_hdr(addr0, addr1, type=P2P_INVITATION_REQ,
                  dialog_token=dialog_token)
    attrs = p2p_attr_config_timeout()
    attrs = p2p_attr_invitation_flags(bitmap=1)
    attrs += struct.pack("<BH3BBBB", P2P_ATTR_CHANNEL_LIST, 6,
                         0x58, 0x58, 0x04,
                         81, 1, 3)
    attrs += p2p_attr_group_id(res['go_dev_addr'], res['ssid'])
    attrs += p2p_attr_device_info(addr1, config_methods=0x0108)
    msg['payload'] += ie_p2p(attrs)

    mgmt_tx(dev[1], "MGMT_TX {} {} freq={} wait_time=200 no_cck=1 action={}".format(addr0, addr0, peer['listen_freq'], binascii.hexlify(msg['payload']).decode()))

    rx_msg = dev[1].mgmt_rx()
    if rx_msg is None:
        raise Exception("MGMT-RX timeout")
    p2p = parse_p2p_public_action(rx_msg['payload'])
    if p2p is None:
        raise Exception("Not a P2P Public Action frame " + str(dialog_token))
    if p2p['subtype'] != P2P_INVITATION_RESP:
        raise Exception("Unexpected subtype %d" % p2p['subtype'])
    if p2p['p2p_status'] != 7 and dev[1].get_mcc() <= 1:
        raise Exception("Unexpected status %d" % p2p['p2p_status'])

@remote_compatible
def test_p2p_msg_invitation_req_unknown(dev, apdev):
    """P2P protocol tests for invitation request from unknown peer"""
    dst, src, hapd, channel = start_p2p(dev, apdev)
    dialog_token = 0

    dialog_token += 1
    msg = p2p_hdr(dst, src, type=P2P_INVITATION_REQ, dialog_token=dialog_token)
    attrs = p2p_attr_config_timeout()
    attrs += p2p_attr_invitation_flags()
    attrs += p2p_attr_operating_channel()
    attrs += p2p_attr_group_bssid(src)
    attrs += p2p_attr_channel_list()
    #attrs += p2p_attr_group_id(src, "DIRECT-foo")
    #attrs += p2p_attr_device_info(src, config_methods=0x0108)
    msg['payload'] += ie_p2p(attrs)
    hapd.mgmt_tx(msg)
    ev = dev[0].wait_global_event(["P2P-INVITATION-RECEIVED"], timeout=5)
    if ev is None:
        raise Exception("Timeout on invitation event " + str(dialog_token))
    if hapd.mgmt_rx(timeout=1) is None:
        raise Exception("No invitation response " + str(dialog_token))

@remote_compatible
def test_p2p_msg_invitation_no_common_channels(dev, apdev):
    """P2P protocol tests for invitation request without common channels"""
    dst, src, hapd, channel = start_p2p(dev, apdev)
    dialog_token = 0

    dialog_token += 1
    msg = p2p_hdr(dst, src, type=P2P_INVITATION_REQ, dialog_token=dialog_token)
    attrs = p2p_attr_config_timeout()
    attrs += p2p_attr_invitation_flags()
    attrs += p2p_attr_operating_channel()
    attrs += p2p_attr_group_bssid(src)
    attrs += struct.pack("<BH3BBB", P2P_ATTR_CHANNEL_LIST, 5,
                         0x58, 0x58, 0x04,
                         81, 0)
    attrs += p2p_attr_group_id(src, "DIRECT-foo")
    attrs += p2p_attr_device_info(src, config_methods=0x0108)
    msg['payload'] += ie_p2p(attrs)
    hapd.mgmt_tx(msg)
    if hapd.mgmt_rx(timeout=1) is None:
        raise Exception("No invitation response " + str(dialog_token))
    ev = dev[0].wait_event(["P2P-INVITATION-RECEIVED"], timeout=0.1)
    if ev is not None:
        raise Exception("Unexpected invitation event")

def test_p2p_msg_invitation_resp(dev, apdev):
    """P2P protocol tests for invitation response processing"""
    form(dev[0], dev[1])
    dev[0].dump_monitor()
    dev[1].dump_monitor()

    dst, src, hapd, channel = start_p2p(dev, apdev)

    addr0 = dev[0].p2p_dev_addr()
    addr1 = dev[1].p2p_dev_addr()
    peer = dev[1].get_peer(addr0)

    # P2P Invitation Response from unknown peer
    msg = p2p_hdr(dst, src, type=P2P_INVITATION_RESP, dialog_token=1)
    hapd.mgmt_tx(msg)

    # P2P Invitation Response from peer that is not in invitation
    msg = p2p_hdr(dst, src, type=P2P_INVITATION_RESP, dialog_token=2)
    attrs = p2p_attr_status()
    msg['payload'] += ie_p2p(attrs)
    mgmt_tx(dev[1], "MGMT_TX {} {} freq={} wait_time=200 no_cck=1 action={}".format(
        addr0, addr0, peer['listen_freq'], binascii.hexlify(msg['payload']).decode()))
    time.sleep(0.25)

    if "FAIL" in dev[1].request("SET ext_mgmt_frame_handling 1"):
        raise Exception("Failed to enable external management frame handling")

    invite(dev[0], dev[1])
    rx_msg = dev[1].mgmt_rx()
    if rx_msg is None:
        raise Exception("MGMT-RX timeout")
    p2p = parse_p2p_public_action(rx_msg['payload'])
    if p2p is None:
        raise Exception("Not a P2P Public Action frame " + str(dialog_token))
    if p2p['subtype'] != P2P_INVITATION_REQ:
        raise Exception("Unexpected subtype %d" % p2p['subtype'])

    # Invalid attribute to cause p2p_parse() failure
    msg = p2p_hdr(dst, src, type=P2P_INVITATION_RESP, dialog_token=p2p['dialog_token'])
    attrs = struct.pack("<BB", P2P_ATTR_CAPABILITY, 0)
    msg['payload'] += ie_p2p(attrs)
    mgmt_tx(dev[1], "MGMT_TX {} {} freq={} wait_time=200 no_cck=1 action={}".format(
        addr0, addr0, rx_msg['freq'], binascii.hexlify(msg['payload']).decode()))

    invite(dev[0], dev[1])
    rx_msg = dev[1].mgmt_rx()
    if rx_msg is None:
        raise Exception("MGMT-RX timeout")
    p2p = parse_p2p_public_action(rx_msg['payload'])
    if p2p is None:
        raise Exception("Not a P2P Public Action frame " + str(dialog_token))
    if p2p['subtype'] != P2P_INVITATION_REQ:
        raise Exception("Unexpected subtype %d" % p2p['subtype'])

    # missing mandatory Status attribute
    msg = p2p_hdr(dst, src, type=P2P_INVITATION_RESP, dialog_token=p2p['dialog_token'])
    attrs = p2p_attr_channel_list()
    msg['payload'] += ie_p2p(attrs)
    mgmt_tx(dev[1], "MGMT_TX {} {} freq={} wait_time=200 no_cck=1 action={}".format(
        addr0, addr0, rx_msg['freq'], binascii.hexlify(msg['payload']).decode()))

    invite(dev[0], dev[1])
    rx_msg = dev[1].mgmt_rx()
    if rx_msg is None:
        raise Exception("MGMT-RX timeout")
    p2p = parse_p2p_public_action(rx_msg['payload'])
    if p2p is None:
        raise Exception("Not a P2P Public Action frame " + str(dialog_token))
    if p2p['subtype'] != P2P_INVITATION_REQ:
        raise Exception("Unexpected subtype %d" % p2p['subtype'])

    # no channel match (no common channel found at all)
    msg = p2p_hdr(dst, src, type=P2P_INVITATION_RESP, dialog_token=p2p['dialog_token'])
    attrs = p2p_attr_status()
    attrs += struct.pack("<BH3BBBB", P2P_ATTR_CHANNEL_LIST, 6,
                         0x58, 0x58, 0x04,
                         81, 1, 15)
    msg['payload'] += ie_p2p(attrs)
    mgmt_tx(dev[1], "MGMT_TX {} {} freq={} wait_time=200 no_cck=1 action={}".format(
        addr0, addr0, rx_msg['freq'], binascii.hexlify(msg['payload']).decode()))

    invite(dev[0], dev[1])
    rx_msg = dev[1].mgmt_rx()
    if rx_msg is None:
        raise Exception("MGMT-RX timeout")
    p2p = parse_p2p_public_action(rx_msg['payload'])
    if p2p is None:
        raise Exception("Not a P2P Public Action frame " + str(dialog_token))
    if p2p['subtype'] != P2P_INVITATION_REQ:
        raise Exception("Unexpected subtype %d" % p2p['subtype'])

    # no channel match (no acceptable P2P channel)
    msg = p2p_hdr(dst, src, type=P2P_INVITATION_RESP, dialog_token=p2p['dialog_token'])
    attrs = p2p_attr_status()
    attrs += struct.pack("<BH3BBBB", P2P_ATTR_CHANNEL_LIST, 6,
                         0x58, 0x58, 0x04,
                         81, 1, 12)
    msg['payload'] += ie_p2p(attrs)
    mgmt_tx(dev[1], "MGMT_TX {} {} freq={} wait_time=200 no_cck=1 action={}".format(
        addr0, addr0, rx_msg['freq'], binascii.hexlify(msg['payload']).decode()))

    invite(dev[0], dev[1])
    rx_msg = dev[1].mgmt_rx()
    if rx_msg is None:
        raise Exception("MGMT-RX timeout")
    p2p = parse_p2p_public_action(rx_msg['payload'])
    if p2p is None:
        raise Exception("Not a P2P Public Action frame " + str(dialog_token))
    if p2p['subtype'] != P2P_INVITATION_REQ:
        raise Exception("Unexpected subtype %d" % p2p['subtype'])

    # missing mandatory Channel List attribute (ignored as a workaround)
    msg = p2p_hdr(dst, src, type=P2P_INVITATION_RESP, dialog_token=p2p['dialog_token'])
    attrs = p2p_attr_status()
    msg['payload'] += ie_p2p(attrs)
    mgmt_tx(dev[1], "MGMT_TX {} {} freq={} wait_time=200 no_cck=1 action={}".format(
        addr0, addr0, rx_msg['freq'], binascii.hexlify(msg['payload']).decode()))

    ev = dev[0].wait_global_event(["P2P-GROUP-STARTED"], timeout=15)
    if ev is None:
        raise Exception("Group was not started")

def test_p2p_msg_invitation_resend(dev, apdev):
    """P2P protocol tests for invitation resending on no-common-channels"""
    form(dev[0], dev[1])
    dev[0].dump_monitor()
    dev[1].dump_monitor()
    addr0 = dev[0].p2p_dev_addr()
    addr1 = dev[1].p2p_dev_addr()

    if "FAIL" in dev[1].request("SET ext_mgmt_frame_handling 1"):
        raise Exception("Failed to enable external management frame handling")

    logger.info("Forced channel in invitation")
    invite(dev[0], dev[1], extra="freq=2422")
    rx_msg = dev[1].mgmt_rx()
    if rx_msg is None:
        raise Exception("MGMT-RX timeout")
    p2p = parse_p2p_public_action(rx_msg['payload'])
    if p2p is None:
        raise Exception("Not a P2P Public Action frame " + str(dialog_token))
    if p2p['subtype'] != P2P_INVITATION_REQ:
        raise Exception("Unexpected subtype %d" % p2p['subtype'])
    msg = p2p_hdr(addr0, addr1, type=P2P_INVITATION_RESP,
                  dialog_token=p2p['dialog_token'])
    attrs = p2p_attr_status(status=P2P_SC_FAIL_NO_COMMON_CHANNELS)
    msg['payload'] += ie_p2p(attrs)
    mgmt_tx(dev[1], "MGMT_TX {} {} freq={} wait_time=200 no_cck=1 action={}".format(
        addr0, addr0, rx_msg['freq'], binascii.hexlify(msg['payload']).decode()))
    ev = dev[0].wait_global_event(["P2P-INVITATION-RESULT"], timeout=15)
    if ev is None:
        raise Exception("Timeout on invitation result")
    if "status=7" not in ev:
        raise Exception("Unexpected invitation result: " + ev)

    logger.info("Any channel allowed, only preference provided in invitation")
    invite(dev[0], dev[1], extra="pref=2422")
    rx_msg = dev[1].mgmt_rx()
    if rx_msg is None:
        raise Exception("MGMT-RX timeout")
    p2p = parse_p2p_public_action(rx_msg['payload'])
    if p2p is None:
        raise Exception("Not a P2P Public Action frame " + str(dialog_token))
    if p2p['subtype'] != P2P_INVITATION_REQ:
        raise Exception("Unexpected subtype %d" % p2p['subtype'])
    msg = p2p_hdr(addr0, addr1, type=P2P_INVITATION_RESP,
                  dialog_token=p2p['dialog_token'])
    attrs = p2p_attr_status(status=P2P_SC_FAIL_NO_COMMON_CHANNELS)
    msg['payload'] += ie_p2p(attrs)
    if "FAIL" in dev[1].request("SET ext_mgmt_frame_handling 0"):
        raise Exception("Failed to disable external management frame handling")
    mgmt_tx(dev[1], "MGMT_TX {} {} freq={} wait_time=200 no_cck=1 action={}".format(
        addr0, addr0, rx_msg['freq'], binascii.hexlify(msg['payload']).decode()))
    ev = dev[0].wait_global_event(["P2P-INVITATION-RESULT"], timeout=15)
    if ev is None:
        raise Exception("Timeout on invitation result")
    if "status=0" not in ev:
        raise Exception("Unexpected invitation result: " + ev)

    ev = dev[0].wait_global_event(["P2P-GROUP-STARTED"], timeout=15)
    if ev is None:
        raise Exception("Group was not started on dev0")
    ev = dev[1].wait_global_event(["P2P-GROUP-STARTED"], timeout=15)
    if ev is None:
        raise Exception("Group was not started on dev1")

def test_p2p_msg_invitation_resend_duplicate(dev, apdev):
    """P2P protocol tests for invitation resending on no-common-channels and duplicated response"""
    form(dev[0], dev[1])
    dev[0].dump_monitor()
    dev[1].dump_monitor()
    addr0 = dev[0].p2p_dev_addr()
    addr1 = dev[1].p2p_dev_addr()

    if "FAIL" in dev[1].request("SET ext_mgmt_frame_handling 1"):
        raise Exception("Failed to enable external management frame handling")

    logger.info("Any channel allowed, only preference provided in invitation")
    invite(dev[0], dev[1], extra="pref=2422")
    rx_msg = dev[1].mgmt_rx()
    if rx_msg is None:
        raise Exception("MGMT-RX timeout")
    p2p = parse_p2p_public_action(rx_msg['payload'])
    if p2p is None:
        raise Exception("Not a P2P Public Action frame " + str(dialog_token))
    if p2p['subtype'] != P2P_INVITATION_REQ:
        raise Exception("Unexpected subtype %d" % p2p['subtype'])
    msg = p2p_hdr(addr0, addr1, type=P2P_INVITATION_RESP,
                  dialog_token=p2p['dialog_token'])
    attrs = p2p_attr_status(status=P2P_SC_FAIL_NO_COMMON_CHANNELS)
    msg['payload'] += ie_p2p(attrs)
    mgmt_tx(dev[1], "MGMT_TX {} {} freq={} wait_time=200 no_cck=1 action={}".format(
        addr0, addr0, rx_msg['freq'], binascii.hexlify(msg['payload']).decode()))

    rx_msg = dev[1].mgmt_rx()
    if rx_msg is None:
        raise Exception("MGMT-RX timeout")
    p2p = parse_p2p_public_action(rx_msg['payload'])
    if p2p is None:
        raise Exception("Not a P2P Public Action frame " + str(dialog_token))
    if p2p['subtype'] != P2P_INVITATION_REQ:
        raise Exception("Unexpected subtype %d" % p2p['subtype'])

    logger.info("Retransmit duplicate of previous response")
    mgmt_tx(dev[1], "MGMT_TX {} {} freq={} wait_time=200 no_cck=1 action={}".format(
        addr0, addr0, rx_msg['freq'], binascii.hexlify(msg['payload']).decode()))

    logger.info("Transmit real response")
    msg = p2p_hdr(addr0, addr1, type=P2P_INVITATION_RESP,
                  dialog_token=p2p['dialog_token'])
    attrs = p2p_attr_status(status=P2P_SC_SUCCESS)
    attrs += p2p_attr_channel_list()
    msg['payload'] += ie_p2p(attrs)
    if "FAIL" in dev[1].request("MGMT_TX {} {} freq={} wait_time=200 no_cck=1 action={}".format(
        addr0, addr0, rx_msg['freq'], binascii.hexlify(msg['payload']).decode())):
        raise Exception("Failed to transmit real response")
    dev[1].request("SET ext_mgmt_frame_handling 0")

    ev = dev[0].wait_global_event(["P2P-INVITATION-RESULT"], timeout=10)
    if ev is None:
        raise Exception("Timeout on invitation result")
    if "status=0" not in ev:
        raise Exception("Unexpected invitation result: " + ev)
    ev = dev[0].wait_global_event(["P2P-GROUP-STARTED"], timeout=10)
    if ev is None:
        raise Exception("Group formation timed out")
    dev[0].group_form_result(ev)
    dev[0].remove_group()

@remote_compatible
def test_p2p_msg_pd_req(dev, apdev):
    """P2P protocol tests for provision discovery request processing"""
    dst, src, hapd, channel = start_p2p(dev, apdev)
    dialog_token = 0

    # Too short attribute header
    dialog_token += 1
    msg = p2p_hdr(dst, src, type=P2P_PROV_DISC_REQ, dialog_token=dialog_token)
    attrs = struct.pack("<BB", P2P_ATTR_CAPABILITY, 0)
    msg['payload'] += ie_p2p(attrs)
    hapd.mgmt_tx(msg)

    if hapd.mgmt_rx(timeout=0.5) is not None:
        raise Exception("Unexpected management frame received")

    # No attributes
    dialog_token += 1
    msg = p2p_hdr(dst, src, type=P2P_PROV_DISC_REQ, dialog_token=dialog_token)
    attrs = b''
    msg['payload'] += ie_p2p(attrs)
    hapd.mgmt_tx(msg)
    if hapd.mgmt_rx(timeout=1) is None:
        raise Exception("No PD response " + str(dialog_token))

    # Valid request
    time.sleep(0.1)
    dialog_token += 1
    msg = p2p_hdr(dst, src, type=P2P_PROV_DISC_REQ, dialog_token=dialog_token)
    attrs = wsc_attr_config_methods(methods=0x1008)
    msg['payload'] += ie_wsc(attrs)
    attrs = p2p_attr_capability()
    attrs += p2p_attr_device_info(src, config_methods=0x0108)
    msg['payload'] += ie_p2p(attrs)
    hapd.mgmt_tx(msg)
    ev = dev[0].wait_global_event(["P2P-DEVICE-FOUND"], timeout=5)
    if ev is None:
        raise Exception("Timeout on device found event")
    ev = dev[0].wait_global_event(["P2P-PROV-DISC-SHOW-PIN"], timeout=5)
    if ev is None:
        raise Exception("Timeout on PD event")
    if hapd.mgmt_rx(timeout=1) is None:
        raise Exception("No PD response " + str(dialog_token))

    # Unknown group
    time.sleep(0.1)
    dialog_token += 1
    msg = p2p_hdr(dst, src, type=P2P_PROV_DISC_REQ, dialog_token=dialog_token)
    attrs = wsc_attr_config_methods(methods=0x1008)
    msg['payload'] += ie_wsc(attrs)
    attrs = p2p_attr_capability()
    attrs += p2p_attr_device_info(src, config_methods=0x0108)
    attrs += p2p_attr_group_id("02:02:02:02:02:02", "DIRECT-foo")
    msg['payload'] += ie_p2p(attrs)
    hapd.mgmt_tx(msg)
    if hapd.mgmt_rx(timeout=1) is None:
        raise Exception("No PD response " + str(dialog_token))
    ev = dev[0].wait_global_event(["P2P-PROV-DISC-SHOW-PIN"], timeout=1)
    if ev is not None:
        raise Exception("Unexpected PD event")

    # Listen channel is not yet known
    if "FAIL" not in dev[0].global_request("P2P_PROV_DISC " + src + " display"):
        raise Exception("Unexpected P2P_PROV_DISC success")

    # Unknown peer
    if "FAIL" not in dev[0].global_request("P2P_PROV_DISC 02:03:04:05:06:07 display"):
        raise Exception("Unexpected P2P_PROV_DISC success (2)")

def test_p2p_msg_pd(dev, apdev):
    """P2P protocol tests for provision discovery request processing (known)"""
    dst, src, hapd, channel = start_p2p(dev, apdev)
    dialog_token = 0

    p2p_probe(hapd, src, chan=channel)
    time.sleep(0.1)

    # Valid request
    dialog_token += 1
    msg = p2p_hdr(dst, src, type=P2P_PROV_DISC_REQ, dialog_token=dialog_token)
    attrs = wsc_attr_config_methods(methods=0x1008)
    msg['payload'] += ie_wsc(attrs)
    attrs = p2p_attr_capability()
    attrs += p2p_attr_device_info(src, config_methods=0x0108)
    msg['payload'] += ie_p2p(attrs)
    hapd.mgmt_tx(msg)
    ev = dev[0].wait_global_event(["P2P-DEVICE-FOUND"], timeout=5)
    if ev is None:
        raise Exception("Timeout on device found event")
    ev = dev[0].wait_global_event(["P2P-PROV-DISC-SHOW-PIN"], timeout=5)
    if ev is None:
        raise Exception("Timeout on PD event")
    if hapd.mgmt_rx(timeout=1) is None:
        raise Exception("No PD response " + str(dialog_token))

    if "FAIL" in dev[0].global_request("P2P_PROV_DISC " + src + " display"):
        raise Exception("Unexpected P2P_PROV_DISC failure")
    frame = hapd.mgmt_rx(timeout=1)
    if frame is None:
        raise Exception("No PD request " + str(dialog_token))
    p2p = parse_p2p_public_action(frame['payload'])
    if p2p is None:
        raise Exception("Failed to parse PD request")

    # invalid dialog token
    msg = p2p_hdr_resp(dst, src, type=P2P_PROV_DISC_RESP,
                       dialog_token=p2p['dialog_token'] + 1)
    hapd.mgmt_tx(msg)
    ev = dev[0].wait_global_event(["P2P-PROV-DISC-FAILURE"], timeout=0.1)
    if ev is not None:
        raise Exception("Unexpected PD result event")

    # valid dialog token
    msg = p2p_hdr_resp(dst, src, type=P2P_PROV_DISC_RESP,
                       dialog_token=p2p['dialog_token'])
    hapd.mgmt_tx(msg)
    ev = dev[0].wait_global_event(["P2P-PROV-DISC-FAILURE"], timeout=5)
    if ev is None:
        raise Exception("Timeout on PD result event")

    # valid dialog token
    msg = p2p_hdr_resp(dst, src, type=P2P_PROV_DISC_RESP,
                       dialog_token=p2p['dialog_token'])
    hapd.mgmt_tx(msg)
    ev = dev[0].wait_global_event(["P2P-PROV-DISC-FAILURE"], timeout=0.1)
    if ev is not None:
        raise Exception("Unexpected PD result event")

def check_p2p_response(hapd, dialog_token, status):
    resp = hapd.mgmt_rx(timeout=2)
    if resp is None:
        raise Exception("No GO Neg Response " + str(dialog_token))
    p2p = parse_p2p_public_action(resp['payload'])
    if p2p is None:
        raise Exception("Not a P2P Public Action frame " + str(dialog_token))
    if dialog_token != p2p['dialog_token']:
        raise Exception("Unexpected dialog token in response")
    if p2p['p2p_status'] != status:
        raise Exception("Unexpected status code %s in response (expected %d)" % (p2p['p2p_status'], status))

def test_p2p_msg_go_neg_both_start(dev, apdev):
    """P2P protocol test for simultaneous GO Neg initiation"""
    addr0 = dev[0].p2p_dev_addr()
    addr1 = dev[1].p2p_dev_addr()
    dev[0].p2p_listen()
    dev[1].discover_peer(addr0)
    dev[1].p2p_listen()
    dev[0].discover_peer(addr1)
    dev[0].p2p_listen()
    if "FAIL" in dev[0].request("SET ext_mgmt_frame_handling 1"):
        raise Exception("Failed to enable external management frame handling")
    if "FAIL" in dev[1].request("SET ext_mgmt_frame_handling 1"):
        raise Exception("Failed to enable external management frame handling")
    dev[0].request("P2P_CONNECT {} pbc".format(addr1))
    dev[1].request("P2P_CONNECT {} pbc".format(addr0))
    msg = dev[0].mgmt_rx()
    if msg is None:
        raise Exception("MGMT-RX timeout")
    msg = dev[1].mgmt_rx()
    if msg is None:
        raise Exception("MGMT-RX timeout(2)")
    if "FAIL" in dev[0].request("SET ext_mgmt_frame_handling 0"):
        raise Exception("Failed to disable external management frame handling")
    ev = dev[0].wait_global_event(["P2P-GO-NEG-SUCCESS"], timeout=2)
    if ev is not None:
        raise Exception("Unexpected GO Neg success")
    if "FAIL" in dev[1].request("SET ext_mgmt_frame_handling 0"):
        raise Exception("Failed to disable external management frame handling")
    ev = dev[0].wait_global_event(["P2P-GO-NEG-SUCCESS"], timeout=10)
    if ev is None:
        raise Exception("GO Neg did not succeed")
    ev = dev[0].wait_global_event(["P2P-GROUP-STARTED"], timeout=5)
    if ev is None:
        raise Exception("Group formation not succeed")
    ev = dev[1].wait_global_event(["P2P-GROUP-STARTED"], timeout=5)
    if ev is None:
        raise Exception("Group formation not succeed")

def test_p2p_msg_go_neg_req(dev, apdev):
    """P2P protocol tests for invitation request from unknown peer"""
    dst, src, hapd, channel = start_p2p(dev, apdev)
    dialog_token = 0

    # invalid attribute
    dialog_token += 1
    msg = p2p_hdr(dst, src, type=P2P_GO_NEG_REQ, dialog_token=dialog_token)
    attrs = struct.pack("<BB", P2P_ATTR_CAPABILITY, 0)
    msg['payload'] += ie_p2p(attrs)
    hapd.mgmt_tx(msg)
    frame = hapd.mgmt_rx(timeout=0.1)
    if frame is not None:
        print(frame)
        raise Exception("Unexpected GO Neg Response")

    # missing atributes
    dialog_token += 1
    msg = p2p_hdr(dst, src, type=P2P_GO_NEG_REQ, dialog_token=dialog_token)
    attrs = p2p_attr_capability()
    attrs += p2p_attr_go_intent()
    attrs += p2p_attr_config_timeout()
    #attrs += p2p_attr_listen_channel()
    attrs += p2p_attr_ext_listen_timing()
    attrs += p2p_attr_intended_interface_addr("02:02:02:02:02:02")
    attrs += p2p_attr_channel_list()
    attrs += p2p_attr_device_info(src, config_methods=0x0108)
    attrs += p2p_attr_operating_channel()
    msg['payload'] += ie_p2p(attrs)
    hapd.mgmt_tx(msg)
    if hapd.mgmt_rx(timeout=2) is None:
        raise Exception("No GO Neg Response " + str(dialog_token))
    time.sleep(0.1)

    dialog_token += 1
    msg = p2p_hdr(dst, src, type=P2P_GO_NEG_REQ, dialog_token=dialog_token)
    attrs = p2p_attr_capability()
    attrs += p2p_attr_go_intent()
    attrs += p2p_attr_config_timeout()
    attrs += p2p_attr_listen_channel()
    attrs += p2p_attr_ext_listen_timing()
    attrs += p2p_attr_intended_interface_addr("02:02:02:02:02:02")
    attrs += p2p_attr_channel_list()
    attrs += p2p_attr_device_info(src, config_methods=0x0108)
    #attrs += p2p_attr_operating_channel()
    msg['payload'] += ie_p2p(attrs)
    hapd.mgmt_tx(msg)
    if hapd.mgmt_rx(timeout=2) is None:
        raise Exception("No GO Neg Response " + str(dialog_token))
    time.sleep(0.1)

    dialog_token += 1
    msg = p2p_hdr(dst, src, type=P2P_GO_NEG_REQ, dialog_token=dialog_token)
    attrs = p2p_attr_capability()
    attrs += p2p_attr_go_intent()
    attrs += p2p_attr_config_timeout()
    attrs += p2p_attr_listen_channel()
    attrs += p2p_attr_ext_listen_timing()
    attrs += p2p_attr_intended_interface_addr("02:02:02:02:02:02")
    #attrs += p2p_attr_channel_list()
    attrs += p2p_attr_device_info(src, config_methods=0x0108)
    attrs += p2p_attr_operating_channel()
    msg['payload'] += ie_p2p(attrs)
    hapd.mgmt_tx(msg)
    if hapd.mgmt_rx(timeout=2) is None:
        raise Exception("No GO Neg Response " + str(dialog_token))
    time.sleep(0.1)

    dialog_token += 1
    msg = p2p_hdr(dst, src, type=P2P_GO_NEG_REQ, dialog_token=dialog_token)
    attrs = p2p_attr_capability()
    attrs += p2p_attr_go_intent()
    attrs += p2p_attr_config_timeout()
    attrs += p2p_attr_listen_channel()
    attrs += p2p_attr_ext_listen_timing()
    #attrs += p2p_attr_intended_interface_addr("02:02:02:02:02:02")
    attrs += p2p_attr_channel_list()
    attrs += p2p_attr_device_info(src, config_methods=0x0108)
    attrs += p2p_attr_operating_channel()
    msg['payload'] += ie_p2p(attrs)
    hapd.mgmt_tx(msg)
    if hapd.mgmt_rx(timeout=2) is None:
        raise Exception("No GO Neg Response " + str(dialog_token))
    time.sleep(0.1)

    dialog_token += 1
    msg = p2p_hdr(dst, src, type=P2P_GO_NEG_REQ, dialog_token=dialog_token)
    attrs = p2p_attr_capability()
    attrs += p2p_attr_go_intent()
    attrs += p2p_attr_config_timeout()
    attrs += p2p_attr_listen_channel()
    attrs += p2p_attr_ext_listen_timing()
    attrs += p2p_attr_intended_interface_addr("02:02:02:02:02:02")
    attrs += p2p_attr_channel_list()
    #attrs += p2p_attr_device_info(src, config_methods=0x0108)
    attrs += p2p_attr_operating_channel()
    msg['payload'] += ie_p2p(attrs)
    hapd.mgmt_tx(msg)
    if hapd.mgmt_rx(timeout=2) is None:
        raise Exception("No GO Neg Response " + str(dialog_token))
    time.sleep(0.1)

    # SA != P2P Device address
    dialog_token += 1
    msg = p2p_hdr(dst, src, type=P2P_GO_NEG_REQ, dialog_token=dialog_token)
    attrs = p2p_attr_capability()
    attrs += p2p_attr_go_intent()
    attrs += p2p_attr_config_timeout()
    attrs += p2p_attr_listen_channel()
    attrs += p2p_attr_ext_listen_timing()
    attrs += p2p_attr_intended_interface_addr("02:02:02:02:02:02")
    attrs += p2p_attr_channel_list()
    attrs += p2p_attr_device_info("02:02:02:02:02:02", config_methods=0x0108)
    attrs += p2p_attr_operating_channel()
    msg['payload'] += ie_p2p(attrs)
    hapd.mgmt_tx(msg)
    if hapd.mgmt_rx(timeout=2) is None:
        raise Exception("No GO Neg Response " + str(dialog_token))
    time.sleep(0.1)

    # unexpected Status attribute
    dialog_token += 1
    msg = p2p_hdr(dst, src, type=P2P_GO_NEG_REQ, dialog_token=dialog_token)
    attrs = p2p_attr_capability()
    attrs += p2p_attr_go_intent()
    attrs += p2p_attr_config_timeout()
    attrs += p2p_attr_listen_channel()
    attrs += p2p_attr_ext_listen_timing()
    attrs += p2p_attr_intended_interface_addr("02:02:02:02:02:02")
    attrs += p2p_attr_channel_list()
    attrs += p2p_attr_device_info(src, config_methods=0x0108)
    attrs += p2p_attr_operating_channel()
    attrs += p2p_attr_status(status=P2P_SC_FAIL_INFO_CURRENTLY_UNAVAILABLE)
    msg['payload'] += ie_p2p(attrs)
    hapd.mgmt_tx(msg)
    if hapd.mgmt_rx(timeout=2) is None:
        raise Exception("No GO Neg Response(1) " + str(dialog_token))
    time.sleep(0.1)

    # valid (with workarounds) GO Neg Req
    dialog_token += 1
    msg = p2p_hdr(dst, src, type=P2P_GO_NEG_REQ, dialog_token=dialog_token)
    #attrs = p2p_attr_capability()
    #attrs += p2p_attr_go_intent()
    #attrs += p2p_attr_config_timeout()
    attrs = p2p_attr_listen_channel()
    attrs += p2p_attr_ext_listen_timing()
    attrs += p2p_attr_intended_interface_addr("02:02:02:02:02:02")
    attrs += p2p_attr_channel_list()
    attrs += p2p_attr_device_info(src, config_methods=0x0108)
    attrs += p2p_attr_operating_channel()
    msg['payload'] += ie_p2p(attrs)
    hapd.mgmt_tx(msg)
    check_p2p_response(hapd, dialog_token,
                       P2P_SC_FAIL_INFO_CURRENTLY_UNAVAILABLE)
    ev = dev[0].wait_global_event(["P2P-GO-NEG-REQUEST"], timeout=2)
    if ev is None:
        raise Exception("Timeout on GO Neg event " + str(dialog_token))

    dev[0].request("P2P_CONNECT " + src + " 12345670 display auth")

    # ready - missing attributes (with workarounds) GO Neg Req
    time.sleep(0.1)
    dialog_token += 1
    msg = p2p_hdr(dst, src, type=P2P_GO_NEG_REQ, dialog_token=dialog_token)
    #attrs = p2p_attr_capability()
    #attrs += p2p_attr_go_intent()
    #attrs += p2p_attr_config_timeout()
    attrs = p2p_attr_listen_channel()
    attrs += p2p_attr_ext_listen_timing()
    attrs += p2p_attr_intended_interface_addr("02:02:02:02:02:02")
    attrs += p2p_attr_channel_list()
    attrs += p2p_attr_device_info(src, config_methods=0x0108)
    attrs += p2p_attr_operating_channel()
    msg['payload'] += ie_p2p(attrs)
    hapd.mgmt_tx(msg)
    if hapd.mgmt_rx(timeout=2) is None:
        raise Exception("No GO Neg Response " + str(dialog_token))

    # ready - invalid GO Intent GO Neg Req
    time.sleep(0.1)
    dialog_token += 1
    msg = p2p_hdr(dst, src, type=P2P_GO_NEG_REQ, dialog_token=dialog_token)
    #attrs = p2p_attr_capability()
    attrs = p2p_attr_go_intent(go_intent=16)
    #attrs += p2p_attr_config_timeout()
    attrs += p2p_attr_listen_channel()
    attrs += p2p_attr_ext_listen_timing()
    attrs += p2p_attr_intended_interface_addr("02:02:02:02:02:02")
    attrs += p2p_attr_channel_list()
    attrs += p2p_attr_device_info(src, config_methods=0x0108)
    attrs += p2p_attr_operating_channel()
    msg['payload'] += ie_p2p(attrs)
    hapd.mgmt_tx(msg)
    check_p2p_response(hapd, dialog_token, P2P_SC_FAIL_INVALID_PARAMS)

    # ready - invalid Channel List
    time.sleep(0.1)
    dialog_token += 1
    msg = p2p_hdr(dst, src, type=P2P_GO_NEG_REQ, dialog_token=dialog_token)
    attrs = p2p_attr_capability()
    attrs += p2p_attr_go_intent()
    attrs += p2p_attr_config_timeout()
    attrs += p2p_attr_listen_channel()
    attrs += p2p_attr_ext_listen_timing()
    attrs += p2p_attr_intended_interface_addr("02:02:02:02:02:02")
    attrs += struct.pack("<BH3BBB11B", P2P_ATTR_CHANNEL_LIST, 16,
                         0x58, 0x58, 0x04,
                         81, 12, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11)
    attrs += p2p_attr_device_info(src, config_methods=0x0108)
    attrs += p2p_attr_operating_channel()
    msg['payload'] += ie_p2p(attrs)
    hapd.mgmt_tx(msg)
    check_p2p_response(hapd, dialog_token, P2P_SC_FAIL_NO_COMMON_CHANNELS)

    # ready - invalid GO Neg Req (unsupported Device Password ID)
    time.sleep(0.1)
    dialog_token += 1
    msg = p2p_hdr(dst, src, type=P2P_GO_NEG_REQ, dialog_token=dialog_token)
    attrs = p2p_attr_capability()
    attrs += p2p_attr_go_intent()
    attrs += p2p_attr_config_timeout()
    attrs += p2p_attr_listen_channel()
    attrs += p2p_attr_ext_listen_timing()
    attrs += p2p_attr_intended_interface_addr("02:02:02:02:02:02")
    # very long channel list
    attrs += struct.pack("<BH3BBB11B30B", P2P_ATTR_CHANNEL_LIST, 46,
                         0x58, 0x58, 0x04,
                         81, 11, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
                         1, 1, 1, 2, 1, 2, 3, 1, 3, 4, 1, 4, 5, 1, 5,
                         6, 1, 6, 7, 1, 7, 8, 1, 8, 9, 1, 9, 10, 1, 10)
    attrs += p2p_attr_device_info(src, config_methods=0x0108)
    attrs += p2p_attr_operating_channel()
    msg['payload'] += ie_p2p(attrs)
    hapd.mgmt_tx(msg)
    check_p2p_response(hapd, dialog_token, P2P_SC_FAIL_INCOMPATIBLE_PROV_METHOD)

def mgmt_tx(dev, msg):
    for i in range(0, 20):
        if "FAIL" in dev.request(msg):
            raise Exception("Failed to send Action frame")
        ev = dev.wait_event(["MGMT-TX-STATUS"], timeout=10)
        if ev is None:
            raise Exception("Timeout on MGMT-TX-STATUS")
        if "result=SUCCESS" in ev:
            break
        time.sleep(0.01)
    if "result=SUCCESS" not in ev:
        raise Exception("Peer did not ack Action frame")

def rx_go_neg_req(dev):
    msg = dev.mgmt_rx()
    if msg is None:
        raise Exception("MGMT-RX timeout")
    p2p = parse_p2p_public_action(msg['payload'])
    if p2p is None:
        raise Exception("Not a P2P Public Action frame " + str(dialog_token))
    if p2p['subtype'] != P2P_GO_NEG_REQ:
        raise Exception("Unexpected subtype %d" % p2p['subtype'])
    p2p['freq'] = msg['freq']
    return p2p

def rx_go_neg_conf(dev, status=None, dialog_token=None):
    msg = dev.mgmt_rx()
    if msg is None:
        raise Exception("MGMT-RX timeout")
    p2p = parse_p2p_public_action(msg['payload'])
    if p2p is None:
        raise Exception("Not a P2P Public Action frame " + str(dialog_token))
    if p2p['subtype'] != P2P_GO_NEG_CONF:
        raise Exception("Unexpected subtype %d" % p2p['subtype'])
    if dialog_token is not None and dialog_token != p2p['dialog_token']:
        raise Exception("Unexpected dialog token")
    if status is not None and p2p['p2p_status'] != status:
        raise Exception("Unexpected status %d" % p2p['p2p_status'])

def check_p2p_go_neg_fail_event(dev, status):
    ev = dev.wait_global_event(["P2P-GO-NEG-FAILURE"], timeout=5)
    if ev is None:
        raise Exception("GO Negotiation failure not reported")
    if "status=%d" % status not in ev:
        raise Exception("Unexpected failure reason: " + ev)

def test_p2p_msg_go_neg_req_reject(dev, apdev):
    """P2P protocol tests for user reject incorrectly in GO Neg Req"""
    addr0 = dev[0].p2p_dev_addr()
    addr1 = dev[1].p2p_dev_addr()
    dev[0].p2p_listen()
    dev[1].discover_peer(addr0)
    dev[1].group_request("P2P_CONNECT " + addr0 + " pbc")
    ev = dev[0].wait_global_event(["P2P-GO-NEG-REQUEST"], timeout=10)
    if ev is None:
        raise Exception("Timeout on GO Neg Req")

    peer = dev[0].get_peer(addr1)
    dev[0].p2p_stop_find()

    msg = p2p_hdr(addr1, addr0, type=P2P_GO_NEG_REQ, dialog_token=123)
    attrs = p2p_attr_capability()
    attrs += p2p_attr_status(status=P2P_SC_FAIL_REJECTED_BY_USER)
    attrs += p2p_attr_go_intent()
    attrs += p2p_attr_config_timeout()
    attrs += p2p_attr_listen_channel()
    attrs += p2p_attr_ext_listen_timing()
    attrs += p2p_attr_intended_interface_addr(addr0)
    attrs += p2p_attr_channel_list()
    attrs += p2p_attr_device_info(addr0, config_methods=0x0108)
    attrs += p2p_attr_operating_channel()
    msg['payload'] += ie_p2p(attrs)

    mgmt_tx(dev[0], "MGMT_TX {} {} freq={} wait_time=10 no_cck=1 action={}".format(
        addr1, addr1, peer['listen_freq'], binascii.hexlify(msg['payload']).decode()))

    ev = dev[1].wait_global_event(["P2P-GO-NEG-FAILURE"], timeout=5)
    if ev is None:
        raise Exception("GO Negotiation failure not reported")
    if "status=%d" % P2P_SC_FAIL_REJECTED_BY_USER not in ev:
        raise Exception("Unexpected failure reason: " + ev)

def test_p2p_msg_unexpected_go_neg_resp(dev, apdev):
    """P2P protocol tests for unexpected GO Neg Resp"""
    addr0 = dev[0].p2p_dev_addr()
    addr1 = dev[1].p2p_dev_addr()
    dev[1].p2p_listen()
    dev[0].discover_peer(addr1)
    dev[0].p2p_stop_find()
    dev[0].dump_monitor()

    peer = dev[0].get_peer(addr1)

    logger.debug("GO Neg Resp without GO Neg session")
    msg = p2p_hdr(addr1, addr0, type=P2P_GO_NEG_RESP, dialog_token=123)
    attrs = p2p_attr_status()
    attrs += p2p_attr_capability()
    attrs += p2p_attr_go_intent()
    attrs += p2p_attr_config_timeout()
    attrs += p2p_attr_intended_interface_addr(addr0)
    attrs += p2p_attr_channel_list()
    attrs += p2p_attr_device_info(addr0, config_methods=0x0108)
    attrs += p2p_attr_operating_channel()
    msg['payload'] += ie_p2p(attrs)
    mgmt_tx(dev[0], "MGMT_TX {} {} freq={} wait_time=10 no_cck=1 action={}".format(
        addr1, addr1, peer['listen_freq'], binascii.hexlify(msg['payload']).decode()))

    dev[0].p2p_listen()
    dev[1].discover_peer(addr0)
    dev[0].dump_monitor()
    dev[1].dump_monitor()

    logger.debug("Unexpected GO Neg Resp while waiting for new GO Neg session")
    if "FAIL" in dev[1].global_request("P2P_CONNECT " + addr0 + " pbc"):
        raise Exception("P2P_CONNECT failed")
    ev = dev[0].wait_global_event(["P2P-GO-NEG-REQUEST"], timeout=10)
    if ev is None:
        raise Exception("Timeout on GO Neg Req")
    dev[0].p2p_stop_find()
    mgmt_tx(dev[0], "MGMT_TX {} {} freq={} wait_time=10 no_cck=1 action={}".format(
        addr1, addr1, peer['listen_freq'], binascii.hexlify(msg['payload']).decode()))
    dev[0].dump_monitor()
    dev[1].dump_monitor()

    logger.debug("Invalid attribute in GO Neg Response")
    msg = p2p_hdr(addr1, addr0, type=P2P_GO_NEG_RESP, dialog_token=197)
    attrs = struct.pack("<BB", P2P_ATTR_CAPABILITY, 0)
    msg['payload'] += ie_p2p(attrs)
    mgmt_tx(dev[0], "MGMT_TX {} {} freq={} wait_time=10 no_cck=1 action={}".format(
        addr1, addr1, peer['listen_freq'], binascii.hexlify(msg['payload']).decode()))
    frame = dev[0].mgmt_rx(timeout=0.1)
    if frame is not None:
        raise Exception("Unexpected GO Neg Confirm")
    dev[0].dump_monitor()
    dev[1].dump_monitor()

    logger.debug("GO Neg Resp with unexpected dialog token")
    dev[1].p2p_stop_find()
    if "FAIL" in dev[0].request("SET ext_mgmt_frame_handling 1"):
        raise Exception("Failed to enable external management frame handling")
    dev[0].p2p_listen()
    if "FAIL" in dev[1].global_request("P2P_CONNECT " + addr0 + " pbc"):
        raise Exception("P2P_CONNECT failed(2)")
    p2p = rx_go_neg_req(dev[0])
    dev[0].p2p_stop_find()
    dialog_token = p2p['dialog_token']
    if dialog_token < 255:
        dialog_token += 1
    else:
        dialog_token = 1
    msg = p2p_hdr(addr1, addr0, type=P2P_GO_NEG_RESP, dialog_token=dialog_token)
    attrs = p2p_attr_status()
    attrs += p2p_attr_capability()
    attrs += p2p_attr_go_intent()
    attrs += p2p_attr_config_timeout()
    attrs += p2p_attr_intended_interface_addr(addr0)
    attrs += p2p_attr_channel_list()
    attrs += p2p_attr_device_info(addr0, config_methods=0x0108)
    attrs += p2p_attr_operating_channel()
    msg['payload'] += ie_p2p(attrs)
    mgmt_tx(dev[0], "MGMT_TX {} {} freq={} wait_time=200 no_cck=1 action={}".format(
        addr1, addr1, p2p['freq'], binascii.hexlify(msg['payload']).decode()))
    dev[0].dump_monitor()
    dev[1].dump_monitor()

    logger.debug("GO Neg Resp without Status")
    dev[1].p2p_stop_find()
    dev[0].p2p_listen()
    if "FAIL" in dev[1].global_request("P2P_CONNECT " + addr0 + " pbc"):
        raise Exception("P2P_CONNECT failed(2)")
    p2p = rx_go_neg_req(dev[0])
    dev[0].p2p_stop_find()
    dialog_token = p2p['dialog_token']
    msg = p2p_hdr(addr1, addr0, type=P2P_GO_NEG_RESP, dialog_token=dialog_token)
    #attrs = p2p_attr_status()
    attrs = p2p_attr_capability()
    attrs += p2p_attr_go_intent()
    attrs += p2p_attr_config_timeout()
    attrs += p2p_attr_intended_interface_addr(addr0)
    attrs += p2p_attr_channel_list()
    attrs += p2p_attr_device_info(addr0, config_methods=0x0108)
    attrs += p2p_attr_operating_channel()
    msg['payload'] += ie_p2p(attrs)
    mgmt_tx(dev[0], "MGMT_TX {} {} freq={} wait_time=200 no_cck=1 action={}".format(
        addr1, addr1, p2p['freq'], binascii.hexlify(msg['payload']).decode()))
    check_p2p_go_neg_fail_event(dev[1], P2P_SC_FAIL_INVALID_PARAMS)
    rx_go_neg_conf(dev[0], P2P_SC_FAIL_INVALID_PARAMS, dialog_token)
    dev[0].dump_monitor()
    dev[1].dump_monitor()

    logger.debug("GO Neg Resp without Intended Address")
    dev[1].p2p_stop_find()
    dev[0].p2p_listen()
    if "FAIL" in dev[1].global_request("P2P_CONNECT " + addr0 + " pbc"):
        raise Exception("P2P_CONNECT failed(2)")
    p2p = rx_go_neg_req(dev[0])
    dev[0].p2p_stop_find()
    dialog_token = p2p['dialog_token']
    msg = p2p_hdr(addr1, addr0, type=P2P_GO_NEG_RESP, dialog_token=dialog_token)
    attrs = p2p_attr_status()
    #attrs += p2p_attr_capability()
    attrs += p2p_attr_go_intent()
    attrs += p2p_attr_config_timeout()
    #attrs += p2p_attr_intended_interface_addr(addr0)
    attrs += p2p_attr_channel_list()
    #attrs += p2p_attr_device_info(addr0, config_methods=0x0108)
    attrs += p2p_attr_operating_channel()
    msg['payload'] += ie_p2p(attrs)
    mgmt_tx(dev[0], "MGMT_TX {} {} freq={} wait_time=200 no_cck=1 action={}".format(
        addr1, addr1, p2p['freq'], binascii.hexlify(msg['payload']).decode()))
    check_p2p_go_neg_fail_event(dev[1], P2P_SC_FAIL_INVALID_PARAMS)
    rx_go_neg_conf(dev[0], P2P_SC_FAIL_INVALID_PARAMS, dialog_token)
    dev[0].dump_monitor()
    dev[1].dump_monitor()

    logger.debug("GO Neg Resp without GO Intent")
    dev[1].p2p_stop_find()
    dev[0].p2p_listen()
    if "FAIL" in dev[1].global_request("P2P_CONNECT " + addr0 + " pbc"):
        raise Exception("P2P_CONNECT failed(2)")
    p2p = rx_go_neg_req(dev[0])
    dev[0].p2p_stop_find()
    dialog_token = p2p['dialog_token']
    msg = p2p_hdr(addr1, addr0, type=P2P_GO_NEG_RESP, dialog_token=dialog_token)
    attrs = p2p_attr_status()
    attrs += p2p_attr_capability()
    #attrs += p2p_attr_go_intent()
    attrs += p2p_attr_config_timeout()
    attrs += p2p_attr_intended_interface_addr(addr0)
    attrs += p2p_attr_channel_list()
    attrs += p2p_attr_device_info(addr0, config_methods=0x0108)
    attrs += p2p_attr_operating_channel()
    msg['payload'] += ie_p2p(attrs)
    mgmt_tx(dev[0], "MGMT_TX {} {} freq={} wait_time=200 no_cck=1 action={}".format(
        addr1, addr1, p2p['freq'], binascii.hexlify(msg['payload']).decode()))
    check_p2p_go_neg_fail_event(dev[1], P2P_SC_FAIL_INVALID_PARAMS)
    rx_go_neg_conf(dev[0], P2P_SC_FAIL_INVALID_PARAMS, dialog_token)
    dev[0].dump_monitor()
    dev[1].dump_monitor()

    logger.debug("GO Neg Resp with invalid GO Intent")
    dev[1].p2p_stop_find()
    dev[0].p2p_listen()
    if "FAIL" in dev[1].global_request("P2P_CONNECT " + addr0 + " pbc"):
        raise Exception("P2P_CONNECT failed(2)")
    p2p = rx_go_neg_req(dev[0])
    dev[0].p2p_stop_find()
    dialog_token = p2p['dialog_token']
    msg = p2p_hdr(addr1, addr0, type=P2P_GO_NEG_RESP, dialog_token=dialog_token)
    attrs = p2p_attr_status()
    attrs += p2p_attr_capability()
    attrs += p2p_attr_go_intent(go_intent=16)
    attrs += p2p_attr_config_timeout()
    attrs += p2p_attr_intended_interface_addr(addr0)
    attrs += p2p_attr_channel_list()
    attrs += p2p_attr_device_info(addr0, config_methods=0x0108)
    attrs += p2p_attr_operating_channel()
    msg['payload'] += ie_p2p(attrs)
    mgmt_tx(dev[0], "MGMT_TX {} {} freq={} wait_time=200 no_cck=1 action={}".format(
        addr1, addr1, p2p['freq'], binascii.hexlify(msg['payload']).decode()))
    check_p2p_go_neg_fail_event(dev[1], P2P_SC_FAIL_INVALID_PARAMS)
    rx_go_neg_conf(dev[0], P2P_SC_FAIL_INVALID_PARAMS, dialog_token)
    dev[0].dump_monitor()
    dev[1].dump_monitor()

    logger.debug("GO Neg Resp with incompatible GO Intent")
    dev[1].p2p_stop_find()
    dev[0].p2p_listen()
    if "FAIL" in dev[1].global_request("P2P_CONNECT " + addr0 + " pbc go_intent=15"):
        raise Exception("P2P_CONNECT failed(2)")
    p2p = rx_go_neg_req(dev[0])
    dev[0].p2p_stop_find()
    dialog_token = p2p['dialog_token']
    msg = p2p_hdr(addr1, addr0, type=P2P_GO_NEG_RESP, dialog_token=dialog_token)
    attrs = p2p_attr_status()
    attrs += p2p_attr_capability()
    attrs += p2p_attr_go_intent(go_intent=15)
    attrs += p2p_attr_config_timeout()
    attrs += p2p_attr_intended_interface_addr(addr0)
    attrs += p2p_attr_channel_list()
    attrs += p2p_attr_device_info(addr0, config_methods=0x0108)
    attrs += p2p_attr_operating_channel()
    msg['payload'] += ie_p2p(attrs)
    mgmt_tx(dev[0], "MGMT_TX {} {} freq={} wait_time=200 no_cck=1 action={}".format(
        addr1, addr1, p2p['freq'], binascii.hexlify(msg['payload']).decode()))
    check_p2p_go_neg_fail_event(dev[1], P2P_SC_FAIL_INCOMPATIBLE_PARAMS)
    rx_go_neg_conf(dev[0], P2P_SC_FAIL_INCOMPATIBLE_PARAMS, dialog_token)
    dev[0].dump_monitor()
    dev[1].dump_monitor()

    logger.debug("GO Neg Resp without P2P Group ID")
    dev[1].p2p_stop_find()
    dev[0].p2p_listen()
    if "FAIL" in dev[1].global_request("P2P_CONNECT " + addr0 + " pbc go_intent=0"):
        raise Exception("P2P_CONNECT failed(2)")
    p2p = rx_go_neg_req(dev[0])
    dev[0].p2p_stop_find()
    dialog_token = p2p['dialog_token']
    msg = p2p_hdr(addr1, addr0, type=P2P_GO_NEG_RESP, dialog_token=dialog_token)
    attrs = p2p_attr_status()
    attrs += p2p_attr_capability()
    attrs += p2p_attr_go_intent(go_intent=15)
    attrs += p2p_attr_config_timeout()
    attrs += p2p_attr_intended_interface_addr(addr0)
    attrs += p2p_attr_channel_list()
    attrs += p2p_attr_device_info(addr0, config_methods=0x0108)
    attrs += p2p_attr_operating_channel()
    #attrs += p2p_attr_group_id(src, "DIRECT-foo")
    msg['payload'] += ie_p2p(attrs)
    mgmt_tx(dev[0], "MGMT_TX {} {} freq={} wait_time=200 no_cck=1 action={}".format(
        addr1, addr1, p2p['freq'], binascii.hexlify(msg['payload']).decode()))
    check_p2p_go_neg_fail_event(dev[1], P2P_SC_FAIL_INVALID_PARAMS)
    rx_go_neg_conf(dev[0], P2P_SC_FAIL_INVALID_PARAMS, dialog_token)
    dev[0].dump_monitor()
    dev[1].dump_monitor()

    logger.debug("GO Neg Resp without Operating Channel")
    dev[1].p2p_stop_find()
    dev[0].p2p_listen()
    if "FAIL" in dev[1].global_request("P2P_CONNECT " + addr0 + " pbc go_intent=0"):
        raise Exception("P2P_CONNECT failed(2)")
    p2p = rx_go_neg_req(dev[0])
    dev[0].p2p_stop_find()
    dialog_token = p2p['dialog_token']
    msg = p2p_hdr(addr1, addr0, type=P2P_GO_NEG_RESP, dialog_token=dialog_token)
    attrs = p2p_attr_status()
    attrs += p2p_attr_capability()
    attrs += p2p_attr_go_intent(go_intent=15)
    #attrs += p2p_attr_config_timeout()
    attrs += p2p_attr_intended_interface_addr(addr0)
    attrs += p2p_attr_channel_list()
    attrs += p2p_attr_device_info(addr0, config_methods=0x0108)
    #attrs += p2p_attr_operating_channel()
    attrs += p2p_attr_group_id(addr0, "DIRECT-foo")
    msg['payload'] += ie_p2p(attrs)
    mgmt_tx(dev[0], "MGMT_TX {} {} freq={} wait_time=200 no_cck=1 action={}".format(
        addr1, addr1, p2p['freq'], binascii.hexlify(msg['payload']).decode()))
    check_p2p_go_neg_fail_event(dev[1], P2P_SC_FAIL_INVALID_PARAMS)
    rx_go_neg_conf(dev[0], P2P_SC_FAIL_INVALID_PARAMS, dialog_token)
    dev[0].dump_monitor()
    dev[1].dump_monitor()

    logger.debug("GO Neg Resp without Channel List")
    dev[1].p2p_stop_find()
    dev[0].p2p_listen()
    if "FAIL" in dev[1].global_request("P2P_CONNECT " + addr0 + " pbc go_intent=0"):
        raise Exception("P2P_CONNECT failed(2)")
    p2p = rx_go_neg_req(dev[0])
    dev[0].p2p_stop_find()
    dialog_token = p2p['dialog_token']
    msg = p2p_hdr(addr1, addr0, type=P2P_GO_NEG_RESP, dialog_token=dialog_token)
    attrs = p2p_attr_status()
    attrs += p2p_attr_capability()
    attrs += p2p_attr_go_intent(go_intent=15)
    attrs += p2p_attr_config_timeout()
    attrs += p2p_attr_intended_interface_addr(addr0)
    #attrs += p2p_attr_channel_list()
    attrs += p2p_attr_device_info(addr0, config_methods=0x0108)
    attrs += p2p_attr_operating_channel()
    attrs += p2p_attr_group_id(addr0, "DIRECT-foo")
    msg['payload'] += ie_p2p(attrs)
    mgmt_tx(dev[0], "MGMT_TX {} {} freq={} wait_time=200 no_cck=1 action={}".format(
        addr1, addr1, p2p['freq'], binascii.hexlify(msg['payload']).decode()))
    check_p2p_go_neg_fail_event(dev[1], P2P_SC_FAIL_INVALID_PARAMS)
    rx_go_neg_conf(dev[0], P2P_SC_FAIL_INVALID_PARAMS, dialog_token)
    dev[0].dump_monitor()
    dev[1].dump_monitor()

    logger.debug("GO Neg Resp without common channels")
    dev[1].p2p_stop_find()
    dev[0].p2p_listen()
    if "FAIL" in dev[1].global_request("P2P_CONNECT " + addr0 + " pbc go_intent=0"):
        raise Exception("P2P_CONNECT failed(2)")
    p2p = rx_go_neg_req(dev[0])
    dev[0].p2p_stop_find()
    dialog_token = p2p['dialog_token']
    msg = p2p_hdr(addr1, addr0, type=P2P_GO_NEG_RESP, dialog_token=dialog_token)
    attrs = p2p_attr_status()
    attrs += p2p_attr_capability()
    attrs += p2p_attr_go_intent(go_intent=15)
    attrs += p2p_attr_config_timeout()
    attrs += p2p_attr_intended_interface_addr(addr0)
    attrs += struct.pack("<BH3BBB", P2P_ATTR_CHANNEL_LIST, 5,
                         0x58, 0x58, 0x04,
                         81, 0)
    attrs += p2p_attr_device_info(addr0, config_methods=0x0108)
    attrs += p2p_attr_operating_channel()
    attrs += p2p_attr_group_id(addr0, "DIRECT-foo")
    msg['payload'] += ie_p2p(attrs)
    mgmt_tx(dev[0], "MGMT_TX {} {} freq={} wait_time=200 no_cck=1 action={}".format(
        addr1, addr1, p2p['freq'], binascii.hexlify(msg['payload']).decode()))
    check_p2p_go_neg_fail_event(dev[1], P2P_SC_FAIL_NO_COMMON_CHANNELS)
    rx_go_neg_conf(dev[0], P2P_SC_FAIL_NO_COMMON_CHANNELS, dialog_token)
    dev[0].dump_monitor()
    dev[1].dump_monitor()

def test_p2p_msg_group_info(dev):
    """P2P protocol tests for Group Info parsing"""
    try:
        _test_p2p_msg_group_info(dev)
    finally:
        dev[0].request("VENDOR_ELEM_REMOVE 2 *")

def _test_p2p_msg_group_info(dev):
    tests = ["dd08506f9a090e010001",
             "dd08506f9a090e010000",
             "dd20506f9a090e190018" + "112233445566" + "aabbccddeeff" + "00" + "0000" + "0000000000000000" + "ff",
             "dd20506f9a090e190018" + "112233445566" + "aabbccddeeff" + "00" + "0000" + "0000000000000000" + "00",
             "dd24506f9a090e1d001c" + "112233445566" + "aabbccddeeff" + "00" + "0000" + "0000000000000000" + "00" + "00000000",
             "dd24506f9a090e1d001c" + "112233445566" + "aabbccddeeff" + "00" + "0000" + "0000000000000000" + "00" + "10110001",
             "dd24506f9a090e1d001c" + "112233445566" + "aabbccddeeff" + "00" + "0000" + "0000000000000000" + "00" + "1011ffff"]
    for t in tests:
        dev[0].request("VENDOR_ELEM_REMOVE 2 *")
        if "OK" not in dev[0].request("VENDOR_ELEM_ADD 2 " + t):
            raise Exception("VENDOR_ELEM_ADD failed")
        dev[0].p2p_start_go(freq=2412)
        bssid = dev[0].get_group_status_field('bssid')
        dev[2].request("BSS_FLUSH 0")
        dev[2].scan_for_bss(bssid, freq=2412, force_scan=True)
        bss = dev[2].request("BSS " + bssid)
        if 'p2p_group_client' in bss:
            raise Exception("Unexpected p2p_group_client")
        dev[0].remove_group()

MGMT_SUBTYPE_ACTION = 13
ACTION_CATEG_PUBLIC = 4

GAS_INITIAL_REQUEST = 10
GAS_INITIAL_RESPONSE = 11
GAS_COMEBACK_REQUEST = 12
GAS_COMEBACK_RESPONSE = 13

def gas_hdr(dst, src, type, req=True, dialog_token=0):
    msg = {}
    msg['fc'] = MGMT_SUBTYPE_ACTION << 4
    msg['da'] = dst
    msg['sa'] = src
    if req:
        msg['bssid'] = dst
    else:
        msg['bssid'] = src
    if dialog_token is None:
        msg['payload'] = struct.pack("<BB", ACTION_CATEG_PUBLIC, type)
    else:
        msg['payload'] = struct.pack("<BBB", ACTION_CATEG_PUBLIC, type,
                                     dialog_token)
    return msg

@remote_compatible
def test_p2p_msg_sd(dev, apdev):
    """P2P protocol tests for service discovery messages"""
    dst, src, hapd, channel = start_p2p(dev, apdev)

    logger.debug("Truncated GAS Initial Request - no Dialog Token field")
    msg = gas_hdr(dst, src, GAS_INITIAL_REQUEST, dialog_token=None)
    hapd.mgmt_tx(msg)

    logger.debug("Truncated GAS Initial Request - no Advertisement Protocol element")
    msg = gas_hdr(dst, src, GAS_INITIAL_REQUEST)
    hapd.mgmt_tx(msg)

    logger.debug("Truncated GAS Initial Request - no Advertisement Protocol element length")
    msg = gas_hdr(dst, src, GAS_INITIAL_REQUEST)
    msg['payload'] += struct.pack('B', 108)
    hapd.mgmt_tx(msg)

    logger.debug("Invalid GAS Initial Request - unexpected IE")
    msg = gas_hdr(dst, src, GAS_INITIAL_REQUEST)
    msg['payload'] += struct.pack('BB', 0, 0)
    hapd.mgmt_tx(msg)

    logger.debug("Truncated GAS Initial Request - too short Advertisement Protocol element")
    msg = gas_hdr(dst, src, GAS_INITIAL_REQUEST)
    msg['payload'] += struct.pack('BB', 108, 0)
    hapd.mgmt_tx(msg)

    logger.debug("Truncated GAS Initial Request - too short Advertisement Protocol element 2")
    msg = gas_hdr(dst, src, GAS_INITIAL_REQUEST)
    msg['payload'] += struct.pack('BBB', 108, 1, 127)
    hapd.mgmt_tx(msg)

    logger.debug("Invalid GAS Initial Request - unsupported GAS advertisement protocol id 255")
    msg = gas_hdr(dst, src, GAS_INITIAL_REQUEST)
    msg['payload'] += struct.pack('BBBB', 108, 2, 127, 255)
    hapd.mgmt_tx(msg)

    logger.debug("Truncated GAS Initial Request - no Query Request length field")
    msg = gas_hdr(dst, src, GAS_INITIAL_REQUEST)
    msg['payload'] += anqp_adv_proto()
    hapd.mgmt_tx(msg)

    logger.debug("Truncated GAS Initial Request - too short Query Request length field")
    msg = gas_hdr(dst, src, GAS_INITIAL_REQUEST)
    msg['payload'] += anqp_adv_proto()
    msg['payload'] += struct.pack('<B', 0)
    hapd.mgmt_tx(msg)

    logger.debug("Truncated GAS Initial Request - too short Query Request field (minimum underflow)")
    msg = gas_hdr(dst, src, GAS_INITIAL_REQUEST)
    msg['payload'] += anqp_adv_proto()
    msg['payload'] += struct.pack('<H', 1)
    hapd.mgmt_tx(msg)

    logger.debug("Truncated GAS Initial Request - too short Query Request field (maximum underflow)")
    msg = gas_hdr(dst, src, GAS_INITIAL_REQUEST)
    msg['payload'] += anqp_adv_proto()
    msg['payload'] += struct.pack('<H', 65535)
    hapd.mgmt_tx(msg)

    logger.debug("Truncated GAS Initial Request - too short Query Request field")
    msg = gas_hdr(dst, src, GAS_INITIAL_REQUEST)
    msg['payload'] += anqp_adv_proto()
    msg['payload'] += struct.pack('<H', 0)
    hapd.mgmt_tx(msg)

    logger.debug("Invalid GAS Initial Request - unsupported ANQP Info ID 65535")
    msg = gas_hdr(dst, src, GAS_INITIAL_REQUEST)
    msg['payload'] += anqp_adv_proto()
    msg['payload'] += struct.pack('<HHH', 4, 65535, 0)
    hapd.mgmt_tx(msg)

    logger.debug("Invalid GAS Initial Request - invalid ANQP Query Request length (truncated frame)")
    msg = gas_hdr(dst, src, GAS_INITIAL_REQUEST)
    msg['payload'] += anqp_adv_proto()
    msg['payload'] += struct.pack('<HHH', 4, 56797, 65535)
    hapd.mgmt_tx(msg)

    logger.debug("Invalid GAS Initial Request - invalid ANQP Query Request length (too short Query Request to contain OUI + OUI-type)")
    msg = gas_hdr(dst, src, GAS_INITIAL_REQUEST)
    msg['payload'] += anqp_adv_proto()
    msg['payload'] += struct.pack('<HHH', 4, 56797, 0)
    hapd.mgmt_tx(msg)

    logger.debug("Invalid GAS Initial Request - unsupported ANQP vendor OUI-type")
    msg = gas_hdr(dst, src, GAS_INITIAL_REQUEST)
    msg['payload'] += anqp_adv_proto()
    req = struct.pack('<HH', 56797, 4) + struct.pack('>L', 0x506f9a00)
    msg['payload'] += struct.pack('<H', len(req)) + req
    hapd.mgmt_tx(msg)

    logger.debug("Truncated GAS Initial Request - no Service Update Indicator")
    msg = gas_hdr(dst, src, GAS_INITIAL_REQUEST)
    msg['payload'] += anqp_adv_proto()
    req = struct.pack('<HH', 56797, 4) + struct.pack('>L', 0x506f9a09)
    msg['payload'] += struct.pack('<H', len(req)) + req
    hapd.mgmt_tx(msg)

    logger.debug("Truncated GAS Initial Request - truncated Service Update Indicator")
    msg = gas_hdr(dst, src, GAS_INITIAL_REQUEST)
    msg['payload'] += anqp_adv_proto()
    req = struct.pack('<HH', 56797, 4) + struct.pack('>L', 0x506f9a09)
    req += struct.pack('<B', 0)
    msg['payload'] += struct.pack('<H', len(req)) + req
    hapd.mgmt_tx(msg)

    logger.debug("Unexpected GAS Initial Response")
    hapd.dump_monitor()
    msg = gas_hdr(dst, src, GAS_INITIAL_RESPONSE)
    msg['payload'] += struct.pack('<HH', 0, 0)
    msg['payload'] += anqp_adv_proto()
    msg['payload'] += struct.pack('<H', 0)
    hapd.mgmt_tx(msg)

    logger.debug("Truncated GAS Comeback Request - no Dialog Token field")
    msg = gas_hdr(dst, src, GAS_COMEBACK_REQUEST, dialog_token=None)
    hapd.mgmt_tx(msg)

    logger.debug("GAS Comeback Request - no pending SD response fragment available")
    msg = gas_hdr(dst, src, GAS_COMEBACK_REQUEST)
    hapd.mgmt_tx(msg)

    logger.debug("Unexpected GAS Comeback Response")
    hapd.dump_monitor()
    msg = gas_hdr(dst, src, GAS_COMEBACK_RESPONSE)
    msg['payload'] += struct.pack('<HBH', 0, 0, 0)
    msg['payload'] += anqp_adv_proto()
    msg['payload'] += struct.pack('<H', 0)
    hapd.mgmt_tx(msg)

    logger.debug("Minimal GAS Initial Request")
    hapd.dump_monitor()
    msg = gas_hdr(dst, src, GAS_INITIAL_REQUEST)
    msg['payload'] += anqp_adv_proto()
    req = struct.pack('<HH', 56797, 4) + struct.pack('>L', 0x506f9a09)
    req += struct.pack('<H', 0)
    msg['payload'] += struct.pack('<H', len(req)) + req
    hapd.mgmt_tx(msg)
    resp = hapd.mgmt_rx()
    if resp is None:
        raise Exception("No response to minimal GAS Initial Request")
