# P2P helper functions
# Copyright (c) 2013-2019, Jouni Malinen <j@w1.fi>
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

import logging
logger = logging.getLogger()
import threading
import time
try:
    from Queue import Queue
except ImportError:
    from queue import Queue

import hwsim_utils

MGMT_SUBTYPE_PROBE_REQ = 4
MGMT_SUBTYPE_ACTION = 13
ACTION_CATEG_PUBLIC = 4

P2P_GO_NEG_REQ = 0
P2P_GO_NEG_RESP = 1
P2P_GO_NEG_CONF = 2
P2P_INVITATION_REQ = 3
P2P_INVITATION_RESP = 4
P2P_DEV_DISC_REQ = 5
P2P_DEV_DISC_RESP = 6
P2P_PROV_DISC_REQ = 7
P2P_PROV_DISC_RESP = 8

P2P_ATTR_STATUS = 0
P2P_ATTR_MINOR_REASON_CODE = 1
P2P_ATTR_CAPABILITY = 2
P2P_ATTR_DEVICE_ID = 3
P2P_ATTR_GROUP_OWNER_INTENT = 4
P2P_ATTR_CONFIGURATION_TIMEOUT = 5
P2P_ATTR_LISTEN_CHANNEL = 6
P2P_ATTR_GROUP_BSSID = 7
P2P_ATTR_EXT_LISTEN_TIMING = 8
P2P_ATTR_INTENDED_INTERFACE_ADDR = 9
P2P_ATTR_MANAGEABILITY = 10
P2P_ATTR_CHANNEL_LIST = 11
P2P_ATTR_NOTICE_OF_ABSENCE = 12
P2P_ATTR_DEVICE_INFO = 13
P2P_ATTR_GROUP_INFO = 14
P2P_ATTR_GROUP_ID = 15
P2P_ATTR_INTERFACE = 16
P2P_ATTR_OPERATING_CHANNEL = 17
P2P_ATTR_INVITATION_FLAGS = 18
P2P_ATTR_OOB_GO_NEG_CHANNEL = 19
P2P_ATTR_SERVICE_HASH = 21
P2P_ATTR_SESSION_INFORMATION_DATA = 22
P2P_ATTR_CONNECTION_CAPABILITY = 23
P2P_ATTR_ADVERTISEMENT_ID = 24
P2P_ATTR_ADVERTISED_SERVICE = 25
P2P_ATTR_SESSION_ID = 26
P2P_ATTR_FEATURE_CAPABILITY = 27
P2P_ATTR_PERSISTENT_GROUP = 28
P2P_ATTR_VENDOR_SPECIFIC = 221

P2P_SC_SUCCESS = 0
P2P_SC_FAIL_INFO_CURRENTLY_UNAVAILABLE = 1
P2P_SC_FAIL_INCOMPATIBLE_PARAMS = 2
P2P_SC_FAIL_LIMIT_REACHED = 3
P2P_SC_FAIL_INVALID_PARAMS = 4
P2P_SC_FAIL_UNABLE_TO_ACCOMMODATE = 5
P2P_SC_FAIL_PREV_PROTOCOL_ERROR = 6
P2P_SC_FAIL_NO_COMMON_CHANNELS = 7
P2P_SC_FAIL_UNKNOWN_GROUP = 8
P2P_SC_FAIL_BOTH_GO_INTENT_15 = 9
P2P_SC_FAIL_INCOMPATIBLE_PROV_METHOD = 10
P2P_SC_FAIL_REJECTED_BY_USER = 11

WSC_ATTR_CONFIG_METHODS = 0x1008

WLAN_EID_SSID = 0
WLAN_EID_SUPP_RATES = 1
WLAN_EID_VENDOR_SPECIFIC = 221

def go_neg_pin_authorized_persistent(i_dev, r_dev, i_intent=None, r_intent=None,
                                     i_method='enter', r_method='display',
                                     test_data=True, r_listen=True):
    if r_listen:
        r_dev.p2p_listen()
    i_dev.p2p_listen()
    pin = r_dev.wps_read_pin()
    logger.info("Start GO negotiation " + i_dev.ifname + " -> " + r_dev.ifname)
    r_dev.p2p_go_neg_auth(i_dev.p2p_dev_addr(), pin, r_method,
                          go_intent=r_intent, persistent=True)
    if r_listen:
        r_dev.p2p_listen()
    i_res = i_dev.p2p_go_neg_init(r_dev.p2p_dev_addr(), pin, i_method,
                                  timeout=20, go_intent=i_intent,
                                  persistent=True)
    r_res = r_dev.p2p_go_neg_auth_result()
    logger.debug("i_res: " + str(i_res))
    logger.debug("r_res: " + str(r_res))
    r_dev.dump_monitor()
    i_dev.dump_monitor()
    logger.info("Group formed")
    if test_data:
        hwsim_utils.test_connectivity_p2p(r_dev, i_dev)
    return [i_res, r_res]

def terminate_group(go, cli):
    logger.info("Terminate persistent group")
    cli.close_monitor_group()
    go.remove_group()
    cli.wait_go_ending_session()

def invite(inv, resp, extra=None, persistent_reconnect=True, use_listen=True):
    addr = resp.p2p_dev_addr()
    if persistent_reconnect:
        resp.global_request("SET persistent_reconnect 1")
    else:
        resp.global_request("SET persistent_reconnect 0")
    if use_listen:
        resp.p2p_listen()
    else:
        resp.p2p_find(social=True)
    if not inv.discover_peer(addr, social=True):
        raise Exception("Peer " + addr + " not found")
    inv.dump_monitor()
    peer = inv.get_peer(addr)
    cmd = "P2P_INVITE persistent=" + peer['persistent'] + " peer=" + addr
    if extra:
        cmd = cmd + " " + extra
    inv.global_request(cmd)

def check_result(go, cli):
    ev = go.wait_global_event(["P2P-GROUP-STARTED",
                               "Failed to start AP functionality"], timeout=30)
    if ev is None:
        raise Exception("Timeout on group re-invocation (on GO)")
    if "P2P-GROUP-STARTED" not in ev:
        raise Exception("GO failed to start the group for re-invocation")
    if "[PERSISTENT]" not in ev:
        raise Exception("Re-invoked group not marked persistent")
    go_res = go.group_form_result(ev)
    if go_res['role'] != 'GO':
        raise Exception("Persistent group GO did not become GO")
    if not go_res['persistent']:
        raise Exception("Persistent group not re-invoked as persistent (GO)")
    ev = cli.wait_global_event(["P2P-GROUP-STARTED"], timeout=30)
    if ev is None:
        raise Exception("Timeout on group re-invocation (on client)")
    if "[PERSISTENT]" not in ev:
        raise Exception("Re-invoked group not marked persistent")
    cli_res = cli.group_form_result(ev)
    if cli_res['role'] != 'client':
        raise Exception("Persistent group client did not become client")
    if not cli_res['persistent']:
        raise Exception("Persistent group not re-invoked as persistent (cli)")
    return [go_res, cli_res]

def form(go, cli, test_data=True, reverse_init=False, r_listen=True):
    logger.info("Form a persistent group")
    if reverse_init:
        [i_res, r_res] = go_neg_pin_authorized_persistent(i_dev=cli, i_intent=0,
                                                          r_dev=go, r_intent=15,
                                                          test_data=test_data,
                                                          r_listen=r_listen)
    else:
        [i_res, r_res] = go_neg_pin_authorized_persistent(i_dev=go, i_intent=15,
                                                          r_dev=cli, r_intent=0,
                                                          test_data=test_data,
                                                          r_listen=r_listen)
    if not i_res['persistent'] or not r_res['persistent']:
        raise Exception("Formed group was not persistent")
    terminate_group(go, cli)
    if reverse_init:
        return r_res
    else:
        return i_res

def invite_from_cli(go, cli, terminate=True):
    logger.info("Re-invoke persistent group from client")
    invite(cli, go)
    [go_res, cli_res] = check_result(go, cli)
    hwsim_utils.test_connectivity_p2p(go, cli)
    if terminate:
        terminate_group(go, cli)
    return [go_res, cli_res]

def invite_from_go(go, cli, terminate=True, extra=None):
    logger.info("Re-invoke persistent group from GO")
    invite(go, cli, extra=extra)
    [go_res, cli_res] = check_result(go, cli)
    hwsim_utils.test_connectivity_p2p(go, cli)
    if terminate:
        terminate_group(go, cli)
    return [go_res, cli_res]

def autogo(go, freq=None, persistent=None):
    logger.info("Start autonomous GO " + go.ifname)
    res = go.p2p_start_go(freq=freq, persistent=persistent)
    logger.debug("res: " + str(res))
    return res

def connect_cli(go, client, social=False, freq=None):
    logger.info("Try to connect the client to the GO")
    pin = client.wps_read_pin()
    go.p2p_go_authorize_client(pin)
    res = client.p2p_connect_group(go.p2p_dev_addr(), pin, timeout=60,
                                   social=social, freq=freq)
    logger.info("Client connected")
    hwsim_utils.test_connectivity_p2p(go, client)
    return res

def check_grpform_results(i_res, r_res):
    if i_res['result'] != 'success' or r_res['result'] != 'success':
        raise Exception("Failed group formation")
    if i_res['ssid'] != r_res['ssid']:
        raise Exception("SSID mismatch")
    if i_res['freq'] != r_res['freq']:
        raise Exception("freq mismatch")
    if 'go_neg_freq' in r_res and i_res['go_neg_freq'] != r_res['go_neg_freq']:
        raise Exception("go_neg_freq mismatch")
    if i_res['freq'] != i_res['go_neg_freq']:
        raise Exception("freq/go_neg_freq mismatch")
    if i_res['role'] != i_res['go_neg_role']:
        raise Exception("role/go_neg_role mismatch")
    if 'go_neg_role' in r_res and r_res['role'] != r_res['go_neg_role']:
        raise Exception("role/go_neg_role mismatch")
    if i_res['go_dev_addr'] != r_res['go_dev_addr']:
        raise Exception("GO Device Address mismatch")

def go_neg_init(i_dev, r_dev, pin, i_method, i_intent, res):
    logger.debug("Initiate GO Negotiation from i_dev")
    try:
        i_res = i_dev.p2p_go_neg_init(r_dev.p2p_dev_addr(), pin, i_method, timeout=20, go_intent=i_intent)
        logger.debug("i_res: " + str(i_res))
    except Exception as e:
        i_res = None
        logger.info("go_neg_init thread caught an exception from p2p_go_neg_init: " + str(e))
    res.put(i_res)

def go_neg_pin(i_dev, r_dev, i_intent=None, r_intent=None, i_method='enter', r_method='display'):
    r_dev.p2p_listen()
    i_dev.p2p_listen()
    pin = r_dev.wps_read_pin()
    logger.info("Start GO negotiation " + i_dev.ifname + " -> " + r_dev.ifname)
    r_dev.dump_monitor()
    res = Queue()
    t = threading.Thread(target=go_neg_init, args=(i_dev, r_dev, pin, i_method, i_intent, res))
    t.start()
    logger.debug("Wait for GO Negotiation Request on r_dev")
    ev = r_dev.wait_global_event(["P2P-GO-NEG-REQUEST"], timeout=15)
    if ev is None:
        t.join()
        raise Exception("GO Negotiation timed out")
    r_dev.dump_monitor()
    logger.debug("Re-initiate GO Negotiation from r_dev")
    try:
        r_res = r_dev.p2p_go_neg_init(i_dev.p2p_dev_addr(), pin, r_method,
                                      go_intent=r_intent, timeout=20)
    except Exception as e:
        logger.info("go_neg_pin - r_dev.p2p_go_neg_init() exception: " + str(e))
        t.join()
        raise
    logger.debug("r_res: " + str(r_res))
    r_dev.dump_monitor()
    t.join()
    i_res = res.get()
    if i_res is None:
        raise Exception("go_neg_init thread failed")
    logger.debug("i_res: " + str(i_res))
    logger.info("Group formed")
    hwsim_utils.test_connectivity_p2p(r_dev, i_dev)
    i_dev.dump_monitor()
    return [i_res, r_res]

def go_neg_pin_authorized(i_dev, r_dev, i_intent=None, r_intent=None,
                          expect_failure=False, i_go_neg_status=None,
                          i_method='enter', r_method='display', test_data=True,
                          i_freq=None, r_freq=None,
                          i_freq2=None, r_freq2=None,
                          i_max_oper_chwidth=None, r_max_oper_chwidth=None,
                          i_ht40=False, i_vht=False, r_ht40=False, r_vht=False):
    i_dev.p2p_listen()
    pin = r_dev.wps_read_pin()
    logger.info("Start GO negotiation " + i_dev.ifname + " -> " + r_dev.ifname)
    r_dev.p2p_go_neg_auth(i_dev.p2p_dev_addr(), pin, r_method,
                          go_intent=r_intent, freq=r_freq, freq2=r_freq2,
                          max_oper_chwidth=r_max_oper_chwidth, ht40=r_ht40,
                          vht=r_vht)
    r_dev.p2p_listen()
    i_res = i_dev.p2p_go_neg_init(r_dev.p2p_dev_addr(), pin, i_method,
                                  timeout=20, go_intent=i_intent,
                                  expect_failure=expect_failure, freq=i_freq,
                                  freq2=i_freq2,
                                  max_oper_chwidth=i_max_oper_chwidth,
                                  ht40=i_ht40, vht=i_vht)
    r_res = r_dev.p2p_go_neg_auth_result(expect_failure=expect_failure)
    logger.debug("i_res: " + str(i_res))
    logger.debug("r_res: " + str(r_res))
    r_dev.dump_monitor()
    i_dev.dump_monitor()
    if i_go_neg_status:
        if i_res['result'] != 'go-neg-failed':
            raise Exception("Expected GO Negotiation failure not reported")
        if i_res['status'] != i_go_neg_status:
            raise Exception("Expected GO Negotiation status not seen")
    if expect_failure:
        return
    logger.info("Group formed")
    if test_data:
        hwsim_utils.test_connectivity_p2p(r_dev, i_dev)
    return [i_res, r_res]

def go_neg_init_pbc(i_dev, r_dev, i_intent, res, freq, provdisc):
    logger.debug("Initiate GO Negotiation from i_dev")
    try:
        i_res = i_dev.p2p_go_neg_init(r_dev.p2p_dev_addr(), None, "pbc",
                                      timeout=20, go_intent=i_intent, freq=freq,
                                      provdisc=provdisc)
        logger.debug("i_res: " + str(i_res))
    except Exception as e:
        i_res = None
        logger.info("go_neg_init_pbc thread caught an exception from p2p_go_neg_init: " + str(e))
    res.put(i_res)

def go_neg_pbc(i_dev, r_dev, i_intent=None, r_intent=None, i_freq=None, r_freq=None, provdisc=False, r_listen=False):
    if r_listen:
        r_dev.p2p_listen()
    else:
        r_dev.p2p_find(social=True)
    i_dev.p2p_find(social=True)
    logger.info("Start GO negotiation " + i_dev.ifname + " -> " + r_dev.ifname)
    r_dev.dump_monitor()
    res = Queue()
    t = threading.Thread(target=go_neg_init_pbc, args=(i_dev, r_dev, i_intent, res, i_freq, provdisc))
    t.start()
    logger.debug("Wait for GO Negotiation Request on r_dev")
    ev = r_dev.wait_global_event(["P2P-GO-NEG-REQUEST"], timeout=15)
    if ev is None:
        t.join()
        raise Exception("GO Negotiation timed out")
    r_dev.dump_monitor()
    # Allow some time for the GO Neg Resp to go out before initializing new
    # GO Negotiation.
    time.sleep(0.2)
    logger.debug("Re-initiate GO Negotiation from r_dev")
    try:
        r_res = r_dev.p2p_go_neg_init(i_dev.p2p_dev_addr(), None, "pbc",
                                      go_intent=r_intent, timeout=20,
                                      freq=r_freq)
    except Exception as e:
        logger.info("go_neg_pbc - r_dev.p2p_go_neg_init() exception: " + str(e))
        t.join()
        raise
    logger.debug("r_res: " + str(r_res))
    r_dev.dump_monitor()
    t.join()
    i_res = res.get()
    if i_res is None:
        raise Exception("go_neg_init_pbc thread failed")
    logger.debug("i_res: " + str(i_res))
    logger.info("Group formed")
    hwsim_utils.test_connectivity_p2p(r_dev, i_dev)
    i_dev.dump_monitor()
    return [i_res, r_res]

def go_neg_pbc_authorized(i_dev, r_dev, i_intent=None, r_intent=None,
                          expect_failure=False, i_freq=None, r_freq=None):
    i_dev.p2p_listen()
    logger.info("Start GO negotiation " + i_dev.ifname + " -> " + r_dev.ifname)
    r_dev.p2p_go_neg_auth(i_dev.p2p_dev_addr(), None, "pbc",
                          go_intent=r_intent, freq=r_freq)
    r_dev.p2p_listen()
    i_res = i_dev.p2p_go_neg_init(r_dev.p2p_dev_addr(), None, "pbc", timeout=20,
                                  go_intent=i_intent,
                                  expect_failure=expect_failure, freq=i_freq)
    r_res = r_dev.p2p_go_neg_auth_result(expect_failure=expect_failure)
    logger.debug("i_res: " + str(i_res))
    logger.debug("r_res: " + str(r_res))
    r_dev.dump_monitor()
    i_dev.dump_monitor()
    if expect_failure:
        return
    logger.info("Group formed")
    return [i_res, r_res]

def remove_group(dev1, dev2, allow_failure=False):
    try:
        dev1.remove_group()
    except:
        if not allow_failure:
            raise
    try:
        dev2.remove_group()
    except:
        pass
