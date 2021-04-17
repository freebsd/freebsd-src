# FST functionality tests
# Copyright (c) 2015, Qualcomm Atheros, Inc.
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

import logging
logger = logging.getLogger()
import struct
import subprocess
import time
import os
import re

import hwsim_utils
from hwsim import HWSimRadio
import hostapd
from wpasupplicant import WpaSupplicant
import fst_test_common
import fst_module_aux
from utils import alloc_fail, HwsimSkip

#enum - bad parameter types
bad_param_none = 0
bad_param_session_add_no_params = 1
bad_param_group_id = 2
bad_param_session_set_no_params = 3
bad_param_session_set_unknown_param = 4
bad_param_session_id = 5
bad_param_old_iface = 6
bad_param_new_iface = 7
bad_param_negative_llt = 8
bad_param_zero_llt = 9
bad_param_llt_too_big = 10
bad_param_llt_nan = 11
bad_param_peer_addr = 12
bad_param_session_initiate_no_params = 13
bad_param_session_initiate_bad_session_id = 14
bad_param_session_initiate_with_no_new_iface_set = 15
bad_param_session_initiate_with_bad_peer_addr_set = 16
bad_param_session_initiate_request_with_bad_stie = 17
bad_param_session_initiate_response_with_reject = 18
bad_param_session_initiate_response_with_bad_stie = 19
bad_param_session_initiate_response_with_zero_llt = 20
bad_param_session_initiate_stt_no_response = 21
bad_param_session_initiate_concurrent_setup_request = 22
bad_param_session_transfer_no_params = 23
bad_param_session_transfer_bad_session_id = 24
bad_param_session_transfer_setup_skipped = 25
bad_param_session_teardown_no_params = 26
bad_param_session_teardown_bad_session_id = 27
bad_param_session_teardown_setup_skipped = 28
bad_param_session_teardown_bad_fsts_id = 29

bad_param_names = ("None",
                   "No params passed to session add",
                   "Group ID",
                   "No params passed to session set",
                   "Unknown param passed to session set",
                   "Session ID",
                   "Old interface name",
                   "New interface name",
                   "Negative LLT",
                   "Zero LLT",
                   "LLT too big",
                   "LLT is not a number",
                   "Peer address",
                   "No params passed to session initiate",
                   "Session ID",
                   "No new_iface was set",
                   "Peer address",
                   "Request with bad st ie",
                   "Response with reject",
                   "Response with bad st ie",
                   "Response with zero llt",
                   "No response, STT",
                   "Concurrent setup request",
                   "No params passed to session transfer",
                   "Session ID",
                   "Session setup skipped",
                   "No params passed to session teardown",
                   "Bad session",
                   "Session setup skipped",
                   "Bad fsts_id")

def fst_start_session(apdev, test_params, bad_param_type, start_on_ap,
                      peer_addr=None):
    """This function makes the necessary preparations and the adds and sets a
    session using either correct or incorrect parameters depending on the value
    of bad_param_type. If the call ends as expected (with session being
    successfully added and set in case of correct parameters or with the
    expected exception in case of incorrect parameters), the function silently
    exits. Otherwise, it throws an exception thus failing the test."""

    ap1, ap2, sta1, sta2 = fst_module_aux.start_two_ap_sta_pairs(apdev)
    bad_parameter_detected = False
    exception_already_raised = False
    try:
        fst_module_aux.connect_two_ap_sta_pairs(ap1, ap2, sta1, sta2)
        if start_on_ap:
            initiator = ap1
            responder = sta1
            new_iface = ap2.ifname()
            new_peer_addr = ap2.get_actual_peer_addr()
        else:
            initiator = sta1
            responder = ap1
            new_iface = sta2.ifname()
            new_peer_addr = sta2.get_actual_peer_addr()
        initiator.add_peer(responder, peer_addr, new_peer_addr)
        group_id = None
        if bad_param_type == bad_param_group_id:
            group_id = '-1'
        elif bad_param_type == bad_param_session_add_no_params:
            group_id = ''
        initiator.set_fst_parameters(group_id=group_id)
        sid = initiator.add_session()
        if bad_param_type == bad_param_session_set_no_params:
            res = initiator.set_session_param(None)
            if not res.startswith("OK"):
                raise Exception("Session set operation failed")
        elif bad_param_type == bad_param_session_set_unknown_param:
            res = initiator.set_session_param("bad_param=1")
            if not res.startswith("OK"):
                raise Exception("Session set operation failed")
        else:
            if bad_param_type == bad_param_session_initiate_with_no_new_iface_set:
                new_iface = None
            elif bad_param_type == bad_param_new_iface:
                new_iface = 'wlan12'
            old_iface = None if bad_param_type != bad_param_old_iface else 'wlan12'
            llt = None
            if bad_param_type == bad_param_negative_llt:
                llt = '-1'
            elif bad_param_type == bad_param_zero_llt:
                llt = '0'
            elif bad_param_type == bad_param_llt_too_big:
                llt = '4294967296'    #0x100000000
            elif bad_param_type == bad_param_llt_nan:
                llt = 'nan'
            elif bad_param_type == bad_param_session_id:
                sid = '-1'
            initiator.set_fst_parameters(llt=llt)
            initiator.configure_session(sid, new_iface, old_iface)
    except Exception as e:
        if e.args[0].startswith("Cannot add FST session with groupid"):
            if bad_param_type == bad_param_group_id or bad_param_type == bad_param_session_add_no_params:
                bad_parameter_detected = True
        elif e.args[0].startswith("Cannot set FST session new_ifname:"):
            if bad_param_type == bad_param_new_iface:
                bad_parameter_detected = True
        elif e.args[0].startswith("Session set operation failed"):
            if (bad_param_type == bad_param_session_set_no_params or
                bad_param_type == bad_param_session_set_unknown_param):
                bad_parameter_detected = True
        elif e.args[0].startswith("Cannot set FST session old_ifname:"):
            if (bad_param_type == bad_param_old_iface or
                bad_param_type == bad_param_session_id or
                bad_param_type == bad_param_session_set_no_params):
                bad_parameter_detected = True
        elif e.args[0].startswith("Cannot set FST session llt:"):
            if (bad_param_type == bad_param_negative_llt or
                bad_param_type == bad_param_llt_too_big or
                bad_param_type == bad_param_llt_nan):
                bad_parameter_detected = True
        elif e.args[0].startswith("Cannot set FST session peer address:"):
            if bad_param_type == bad_param_peer_addr:
                bad_parameter_detected = True
        if not bad_parameter_detected:
            # The exception was unexpected
            logger.info(e)
            exception_already_raised = True
            raise
    finally:
        fst_module_aux.disconnect_two_ap_sta_pairs(ap1, ap2, sta1, sta2)
        fst_module_aux.stop_two_ap_sta_pairs(ap1, ap2, sta1, sta2)
        if not exception_already_raised:
            if bad_parameter_detected:
                logger.info("Success. Bad parameter was detected (%s)" % bad_param_names[bad_param_type])
            else:
                if bad_param_type == bad_param_none or bad_param_type == bad_param_zero_llt:
                    logger.info("Success. Session added and set")
                else:
                    exception_text = ""
                    if bad_param_type == bad_param_peer_addr:
                        exception_text = "Failure. Bad parameter was not detected (Peer address == %s)" % ap1.get_new_peer_addr()
                    else:
                        exception_text = "Failure. Bad parameter was not detected (%s)" % bad_param_names[bad_param_type]
                    raise Exception(exception_text)
        else:
            logger.info("Failure. Unexpected exception")

def fst_initiate_session(apdev, test_params, bad_param_type, init_on_ap):
    """This function makes the necessary preparations and then adds, sets and
    initiates a session using either correct or incorrect parameters at each
    stage depending on the value of bad_param_type. If the call ends as expected
    (with session being successfully added, set and initiated in case of correct
    parameters or with the expected exception in case of incorrect parameters),
    the function silently exits. Otherwise it throws an exception thus failing
    the test."""
    ap1, ap2, sta1, sta2 = fst_module_aux.start_two_ap_sta_pairs(apdev)
    bad_parameter_detected = False
    exception_already_raised = False
    try:
        fst_module_aux.connect_two_ap_sta_pairs(ap1, ap2, sta1, sta2)
        # This call makes sure FstHostapd singleton object is created and, as a
        # result, the global control interface is registered (this is done from
        # the constructor).
        ap1.get_global_instance()
        if init_on_ap:
            initiator = ap1
            responder = sta1
            new_iface = ap2.ifname() if bad_param_type != bad_param_session_initiate_with_no_new_iface_set else None
            new_peer_addr = ap2.get_actual_peer_addr()
            resp_newif = sta2.ifname()
        else:
            initiator = sta1
            responder = ap1
            new_iface = sta2.ifname() if bad_param_type != bad_param_session_initiate_with_no_new_iface_set else None
            new_peer_addr = sta2.get_actual_peer_addr()
            resp_newif = ap2.ifname()
        peeraddr = None if bad_param_type != bad_param_session_initiate_with_bad_peer_addr_set else '10:DE:AD:DE:AD:11'
        initiator.add_peer(responder, peeraddr, new_peer_addr)
        if bad_param_type == bad_param_session_initiate_response_with_zero_llt:
            initiator.set_fst_parameters(llt='0')
        sid = initiator.add_session()
        initiator.configure_session(sid, new_iface)
        if bad_param_type == bad_param_session_initiate_no_params:
            sid = ''
        elif bad_param_type == bad_param_session_initiate_bad_session_id:
            sid = '-1'
        if bad_param_type == bad_param_session_initiate_request_with_bad_stie:
            actual_fsts_id = initiator.get_fsts_id_by_sid(sid)
            initiator.send_test_session_setup_request(str(actual_fsts_id), "bad_new_band")
            responder.wait_for_session_event(5)
        elif bad_param_type == bad_param_session_initiate_response_with_reject:
            initiator.send_session_setup_request(sid)
            initiator.wait_for_session_event(5, [], ["EVENT_FST_SESSION_STATE"])
            setup_event = responder.wait_for_session_event(5, [],
                                                           ['EVENT_FST_SETUP'])
            if 'id' not in setup_event:
                raise Exception("No session id in FST setup event")
            responder.send_session_setup_response(str(setup_event['id']),
                                                  "reject")
            event = initiator.wait_for_session_event(5, [], ["EVENT_FST_SESSION_STATE"])
            if event['new_state'] != "INITIAL" or event['reason'] != "REASON_REJECT":
                raise Exception("Response with reject not handled as expected")
            bad_parameter_detected = True
        elif bad_param_type == bad_param_session_initiate_response_with_bad_stie:
            initiator.send_session_setup_request(sid)
            initiator.wait_for_session_event(5, [], ["EVENT_FST_SESSION_STATE"])
            responder.wait_for_session_event(5, [], ['EVENT_FST_SETUP'])
            actual_fsts_id = initiator.get_fsts_id_by_sid(sid)
            responder.send_test_session_setup_response(str(actual_fsts_id),
                                                       "accept", "bad_new_band")
            event = initiator.wait_for_session_event(5, [], ["EVENT_FST_SESSION_STATE"])
            if event['new_state'] != "INITIAL" or event['reason'] != "REASON_ERROR_PARAMS":
                raise Exception("Response with bad STIE not handled as expected")
            bad_parameter_detected = True
        elif bad_param_type == bad_param_session_initiate_response_with_zero_llt:
            initiator.initiate_session(sid, "accept")
            event = initiator.wait_for_session_event(5, [], ["EVENT_FST_SESSION_STATE"])
            if event['new_state'] != "TRANSITION_DONE":
                raise Exception("Response reception for a session with llt=0 not handled as expected")
            bad_parameter_detected = True
        elif bad_param_type == bad_param_session_initiate_stt_no_response:
            initiator.send_session_setup_request(sid)
            initiator.wait_for_session_event(5, [], ["EVENT_FST_SESSION_STATE"])
            responder.wait_for_session_event(5, [], ['EVENT_FST_SETUP'])
            event = initiator.wait_for_session_event(5, [], ["EVENT_FST_SESSION_STATE"])
            if event['new_state'] != "INITIAL" or event['reason'] != "REASON_STT":
                raise Exception("No response scenario not handled as expected")
            bad_parameter_detected = True
        elif bad_param_type == bad_param_session_initiate_concurrent_setup_request:
            responder.add_peer(initiator)
            resp_sid = responder.add_session()
            responder.configure_session(resp_sid, resp_newif)
            initiator.send_session_setup_request(sid)
            actual_fsts_id = initiator.get_fsts_id_by_sid(sid)
            responder.send_test_session_setup_request(str(actual_fsts_id))
            event = initiator.wait_for_session_event(5, [], ["EVENT_FST_SESSION_STATE"])
            initiator_addr = initiator.get_own_mac_address()
            responder_addr = responder.get_own_mac_address()
            if initiator_addr < responder_addr:
                event = initiator.wait_for_session_event(5, [], ["EVENT_FST_SESSION_STATE"])
                if event['new_state'] != "INITIAL" or event['reason'] != "REASON_SETUP":
                    raise Exception("Concurrent setup scenario not handled as expected")
                event = initiator.wait_for_session_event(5, [], ["EVENT_FST_SETUP"])
                # The incoming setup request received by the initiator has
                # priority over the one sent previously by the initiator itself
                # because the initiator's MAC address is numerically lower than
                # the one of the responder. Thus, the initiator should generate
                # an FST_SETUP event.
            else:
                event = initiator.wait_for_session_event(5, [], ["EVENT_FST_SESSION_STATE"])
                if event['new_state'] != "INITIAL" or event['reason'] != "REASON_STT":
                    raise Exception("Concurrent setup scenario not handled as expected")
                # The incoming setup request was dropped at the initiator
                # because its MAC address is numerically bigger than the one of
                # the responder. Thus, the initiator continue to wait for a
                # setup response until the STT event fires.
            bad_parameter_detected = True
        else:
            initiator.initiate_session(sid, "accept")
    except Exception as e:
        if e.args[0].startswith("Cannot initiate fst session"):
            if bad_param_type != bad_param_none:
                bad_parameter_detected = True
        elif e.args[0].startswith("No FST-EVENT-SESSION received"):
            if bad_param_type == bad_param_session_initiate_request_with_bad_stie:
                bad_parameter_detected = True
        if not bad_parameter_detected:
            #The exception was unexpected
            logger.info(e)
            exception_already_raised = True
            raise
    finally:
        fst_module_aux.disconnect_two_ap_sta_pairs(ap1, ap2, sta1, sta2)
        fst_module_aux.stop_two_ap_sta_pairs(ap1, ap2, sta1, sta2)
        if not exception_already_raised:
            if bad_parameter_detected:
                logger.info("Success. Bad parameter was detected (%s)" % bad_param_names[bad_param_type])
            else:
                if bad_param_type == bad_param_none:
                    logger.info("Success. Session initiated")
                else:
                    raise Exception("Failure. Bad parameter was not detected (%s)" % bad_param_names[bad_param_type])
        else:
            logger.info("Failure. Unexpected exception")

def fst_transfer_session(apdev, test_params, bad_param_type, init_on_ap,
                         rsn=False):
    """This function makes the necessary preparations and then adds, sets,
    initiates and attempts to transfer a session using either correct or
    incorrect parameters at each stage depending on the value of bad_param_type.
    If the call ends as expected the function silently exits. Otherwise, it
    throws an exception thus failing the test."""
    ap1, ap2, sta1, sta2 = fst_module_aux.start_two_ap_sta_pairs(apdev, rsn=rsn)
    bad_parameter_detected = False
    exception_already_raised = False
    try:
        fst_module_aux.connect_two_ap_sta_pairs(ap1, ap2, sta1, sta2, rsn=rsn)
        # This call makes sure FstHostapd singleton object is created and, as a
        # result, the global control interface is registered (this is done from
        # the constructor).
        ap1.get_global_instance()
        if init_on_ap:
            initiator = ap1
            responder = sta1
            new_iface = ap2.ifname()
            new_peer_addr = ap2.get_actual_peer_addr()
        else:
            initiator = sta1
            responder = ap1
            new_iface = sta2.ifname()
            new_peer_addr = sta2.get_actual_peer_addr()
        initiator.add_peer(responder, new_peer_addr=new_peer_addr)
        sid = initiator.add_session()
        initiator.configure_session(sid, new_iface)
        if bad_param_type != bad_param_session_transfer_setup_skipped:
            initiator.initiate_session(sid, "accept")
        if bad_param_type == bad_param_session_transfer_no_params:
            sid = ''
        elif bad_param_type == bad_param_session_transfer_bad_session_id:
            sid = '-1'
        initiator.transfer_session(sid)
    except Exception as e:
        if e.args[0].startswith("Cannot transfer fst session"):
            if bad_param_type != bad_param_none:
                bad_parameter_detected = True
        if not bad_parameter_detected:
            # The exception was unexpected
            logger.info(e)
            exception_already_raised = True
            raise
    finally:
        fst_module_aux.disconnect_two_ap_sta_pairs(ap1, ap2, sta1, sta2)
        fst_module_aux.stop_two_ap_sta_pairs(ap1, ap2, sta1, sta2)
        if not exception_already_raised:
            if bad_parameter_detected:
                logger.info("Success. Bad parameter was detected (%s)" % bad_param_names[bad_param_type])
            else:
                if bad_param_type == bad_param_none:
                    logger.info("Success. Session transferred")
                else:
                    raise Exception("Failure. Bad parameter was not detected (%s)" % bad_param_names[bad_param_type])
        else:
            logger.info("Failure. Unexpected exception")


def fst_tear_down_session(apdev, test_params, bad_param_type, init_on_ap):
    """This function makes the necessary preparations and then adds, sets, and
    initiates a session. It then issues a tear down command using either
    correct or incorrect parameters at each stage. If the call ends as expected,
    the function silently exits. Otherwise, it throws an exception thus failing
    the test."""
    ap1, ap2, sta1, sta2 = fst_module_aux.start_two_ap_sta_pairs(apdev)
    bad_parameter_detected = False
    exception_already_raised = False
    try:
        fst_module_aux.connect_two_ap_sta_pairs(ap1, ap2, sta1, sta2)
        # This call makes sure FstHostapd singleton object is created and, as a
        # result, the global control interface is registered (this is done from
        # the constructor).
        ap1.get_global_instance()
        if init_on_ap:
            initiator = ap1
            responder = sta1
            new_iface = ap2.ifname()
            new_peer_addr = ap2.get_actual_peer_addr()
        else:
            initiator = sta1
            responder = ap1
            new_iface = sta2.ifname()
            new_peer_addr = sta2.get_actual_peer_addr()
        initiator.add_peer(responder, new_peer_addr=new_peer_addr)
        sid = initiator.add_session()
        initiator.configure_session(sid, new_iface)
        if bad_param_type != bad_param_session_teardown_setup_skipped:
            initiator.initiate_session(sid, "accept")
        if bad_param_type == bad_param_session_teardown_bad_fsts_id:
            initiator.send_test_tear_down('-1')
            responder.wait_for_session_event(5)
        else:
            if bad_param_type == bad_param_session_teardown_no_params:
                sid = ''
            elif bad_param_type == bad_param_session_teardown_bad_session_id:
                sid = '-1'
            initiator.teardown_session(sid)
    except Exception as e:
        if e.args[0].startswith("Cannot tear down fst session"):
            if (bad_param_type == bad_param_session_teardown_no_params or
                bad_param_type == bad_param_session_teardown_bad_session_id or
                bad_param_type == bad_param_session_teardown_setup_skipped):
                bad_parameter_detected = True
        elif e.args[0].startswith("No FST-EVENT-SESSION received"):
            if bad_param_type == bad_param_session_teardown_bad_fsts_id:
                bad_parameter_detected = True
        if not bad_parameter_detected:
            # The exception was unexpected
            logger.info(e)
            exception_already_raised = True
            raise
    finally:
        fst_module_aux.disconnect_two_ap_sta_pairs(ap1, ap2, sta1, sta2)
        fst_module_aux.stop_two_ap_sta_pairs(ap1, ap2, sta1, sta2)
        if not exception_already_raised:
            if bad_parameter_detected:
                logger.info("Success. Bad parameter was detected (%s)" % bad_param_names[bad_param_type])
            else:
                if bad_param_type == bad_param_none:
                    logger.info("Success. Session torn down")
                else:
                    raise Exception("Failure. Bad parameter was not detected (%s)" % bad_param_names[bad_param_type])
        else:
            logger.info("Failure. Unexpected exception")


#enum - remove session scenarios
remove_scenario_no_params = 0
remove_scenario_bad_session_id = 1
remove_scenario_non_established_session = 2
remove_scenario_established_session = 3

remove_scenario_names = ("No params",
                         "Bad session id",
                         "Remove non-established session",
                         "Remove established session")


def fst_remove_session(apdev, test_params, remove_session_scenario, init_on_ap):
    """This function attempts to remove a session at various stages of its
    formation, depending on the value of remove_session_scenario. If the call
    ends as expected, the function silently exits. Otherwise, it throws an
    exception thus failing the test."""
    ap1, ap2, sta1, sta2 = fst_module_aux.start_two_ap_sta_pairs(apdev)
    bad_parameter_detected = False
    exception_already_raised = False
    try:
        fst_module_aux.connect_two_ap_sta_pairs(ap1, ap2, sta1, sta2)
        # This call makes sure FstHostapd singleton object is created and, as a
        # result, the global control interface is registered (this is done from
        # the constructor).
        ap1.get_global_instance()
        if init_on_ap:
            initiator = ap1
            responder = sta1
            new_iface = ap2.ifname()
            new_peer_addr = ap2.get_actual_peer_addr()
        else:
            initiator = sta1
            responder = ap1
            new_iface = sta2.ifname()
            new_peer_addr = sta2.get_actual_peer_addr()
        initiator.add_peer(responder, new_peer_addr=new_peer_addr)
        sid = initiator.add_session()
        initiator.configure_session(sid, new_iface)
        if remove_session_scenario != remove_scenario_no_params:
            if remove_session_scenario != remove_scenario_non_established_session:
                initiator.initiate_session(sid, "accept")
        if remove_session_scenario == remove_scenario_no_params:
            sid = ''
        elif remove_session_scenario == remove_scenario_bad_session_id:
            sid = '-1'
        initiator.remove_session(sid)
    except Exception as e:
        if e.args[0].startswith("Cannot remove fst session"):
            if (remove_session_scenario == remove_scenario_no_params or
                remove_session_scenario == remove_scenario_bad_session_id):
                bad_parameter_detected = True
        elif e.args[0].startswith("No FST-EVENT-SESSION received"):
            if remove_session_scenario == remove_scenario_non_established_session:
                bad_parameter_detected = True
        if not bad_parameter_detected:
            #The exception was unexpected
            logger.info(e)
            exception_already_raised = True
            raise
    finally:
        fst_module_aux.disconnect_two_ap_sta_pairs(ap1, ap2, sta1, sta2)
        fst_module_aux.stop_two_ap_sta_pairs(ap1, ap2, sta1, sta2)
        if not exception_already_raised:
            if bad_parameter_detected:
                logger.info("Success. Remove scenario ended as expected (%s)" % remove_scenario_names[remove_session_scenario])
            else:
                if remove_session_scenario == remove_scenario_established_session:
                    logger.info("Success. Session removed")
                else:
                    raise Exception("Failure. Remove scenario ended in an unexpected way (%s)" % remove_scenario_names[remove_session_scenario])
        else:
            logger.info("Failure. Unexpected exception")


#enum - frame types
frame_type_session_request = 0
frame_type_session_response = 1
frame_type_ack_request = 2
frame_type_ack_response = 3
frame_type_tear_down = 4

frame_type_names = ("Session request",
                    "Session Response",
                    "Ack request",
                    "Ack response",
                    "Tear down")

def fst_send_unexpected_frame(apdev, test_params, frame_type, send_from_ap, additional_param=''):
    """This function creates two pairs of APs and stations, makes them connect
    and then causes one side to send an unexpected FST frame of the specified
    type to the other. The other side should then identify and ignore the
    frame."""
    ap1, ap2, sta1, sta2 = fst_module_aux.start_two_ap_sta_pairs(apdev)
    exception_already_raised = False
    frame_receive_timeout = False
    try:
        fst_module_aux.connect_two_ap_sta_pairs(ap1, ap2, sta1, sta2)
        # This call makes sure FstHostapd singleton object is created and, as a
        # result, the global control interface is registered (this is done from
        # the constructor).
        ap1.get_global_instance()
        if send_from_ap:
            sender = ap1
            receiver = sta1
            new_iface = ap2.ifname()
            new_peer_addr = ap2.get_actual_peer_addr()
        else:
            sender = sta1
            receiver = ap1
            new_iface = sta2.ifname()
            new_peer_addr = sta2.get_actual_peer_addr()
        sender.add_peer(receiver, new_peer_addr=new_peer_addr)
        sid = sender.add_session()
        sender.configure_session(sid, new_iface)
        if frame_type == frame_type_session_request:
            sender.send_session_setup_request(sid)
            event = receiver.wait_for_session_event(5)
            if event['type'] != 'EVENT_FST_SETUP':
                raise Exception("Unexpected indication: " + event['type'])
        elif frame_type == frame_type_session_response:
            #fsts_id doesn't matter, no actual session exists
            sender.send_test_session_setup_response('0', additional_param)
            receiver.wait_for_session_event(5)
        elif frame_type == frame_type_ack_request:
            #fsts_id doesn't matter, no actual session exists
            sender.send_test_ack_request('0')
            receiver.wait_for_session_event(5)
        elif frame_type == frame_type_ack_response:
            #fsts_id doesn't matter, no actual session exists
            sender.send_test_ack_response('0')
            receiver.wait_for_session_event(5)
        elif frame_type == frame_type_tear_down:
            #fsts_id doesn't matter, no actual session exists
            sender.send_test_tear_down('0')
            receiver.wait_for_session_event(5)
    except Exception as e:
        if e.args[0].startswith("No FST-EVENT-SESSION received"):
            if frame_type != frame_type_session_request:
                frame_receive_timeout = True
        else:
            logger.info(e)
            exception_already_raised = True
            raise
    finally:
        fst_module_aux.disconnect_two_ap_sta_pairs(ap1, ap2, sta1, sta2)
        fst_module_aux.stop_two_ap_sta_pairs(ap1, ap2, sta1, sta2)
        if not exception_already_raised:
            if frame_receive_timeout:
                logger.info("Success. Frame was ignored (%s)" % frame_type_names[frame_type])
            else:
                if frame_type == frame_type_session_request:
                    logger.info("Success. Frame received, session created")
                else:
                    raise Exception("Failure. Frame was not ignored (%s)" % frame_type_names[frame_type])
        else:
            logger.info("Failure. Unexpected exception")


#enum - bad session transfer scenarios
bad_scenario_none = 0
bad_scenario_ack_req_session_not_set_up = 1
bad_scenario_ack_req_session_not_established_init_side = 2
bad_scenario_ack_req_session_not_established_resp_side = 3
bad_scenario_ack_req_bad_fsts_id = 4
bad_scenario_ack_resp_session_not_set_up = 5
bad_scenario_ack_resp_session_not_established_init_side = 6
bad_scenario_ack_resp_session_not_established_resp_side = 7
bad_scenario_ack_resp_no_ack_req = 8
bad_scenario_ack_resp_bad_fsts_id = 9

bad_scenario_names = ("None",
                      "Ack request received before the session was set up",
                      "Ack request received on the initiator side before session was established",
                      "Ack request received on the responder side before session was established",
                      "Ack request received with bad fsts_id",
                      "Ack response received before the session was set up",
                      "Ack response received on the initiator side before session was established",
                      "Ack response received on the responder side before session was established",
                      "Ack response received before ack request was sent",
                      "Ack response received with bad fsts_id")

def fst_bad_transfer(apdev, test_params, bad_scenario_type, init_on_ap):
    """This function makes the necessary preparations and then adds and sets a
    session. It then initiates and it unless instructed otherwise) and attempts
    to send one of the frames involved in the session transfer protocol,
    skipping or distorting one of the stages according to the value of
    bad_scenario_type parameter."""
    ap1, ap2, sta1, sta2 = fst_module_aux.start_two_ap_sta_pairs(apdev)
    bad_parameter_detected = False
    exception_already_raised = False
    try:
        fst_module_aux.connect_two_ap_sta_pairs(ap1, ap2, sta1, sta2)
        # This call makes sure FstHostapd singleton object is created and, as a
        # result, the global control interface is registered (this is done from
        # the constructor).
        ap1.get_global_instance()
        if init_on_ap:
            initiator = ap1
            responder = sta1
            new_iface = ap2.ifname()
            new_peer_addr = ap2.get_actual_peer_addr()
        else:
            initiator = sta1
            responder = ap1
            new_iface = sta2.ifname()
            new_peer_addr = sta2.get_actual_peer_addr()
        initiator.add_peer(responder, new_peer_addr=new_peer_addr)
        sid = initiator.add_session()
        initiator.configure_session(sid, new_iface)
        if (bad_scenario_type != bad_scenario_ack_req_session_not_set_up and
            bad_scenario_type != bad_scenario_ack_resp_session_not_set_up):
            if (bad_scenario_type != bad_scenario_ack_req_session_not_established_init_side and
                bad_scenario_type != bad_scenario_ack_resp_session_not_established_init_side and
                bad_scenario_type != bad_scenario_ack_req_session_not_established_resp_side and
                bad_scenario_type != bad_scenario_ack_resp_session_not_established_resp_side):
                response = "accept"
            else:
                response = ''
            initiator.initiate_session(sid, response)
        if bad_scenario_type == bad_scenario_ack_req_session_not_set_up:
            #fsts_id doesn't matter, no actual session exists
            responder.send_test_ack_request('0')
            initiator.wait_for_session_event(5)
            # We want to send the unexpected frame to the side that already has
            # a session created
        elif bad_scenario_type == bad_scenario_ack_resp_session_not_set_up:
            #fsts_id doesn't matter, no actual session exists
            responder.send_test_ack_response('0')
            initiator.wait_for_session_event(5)
            # We want to send the unexpected frame to the side that already has
            # a session created
        elif bad_scenario_type == bad_scenario_ack_req_session_not_established_init_side:
            #fsts_id doesn't matter, no actual session exists
            initiator.send_test_ack_request('0')
            responder.wait_for_session_event(5, ["EVENT_FST_SESSION_STATE"])
        elif bad_scenario_type == bad_scenario_ack_req_session_not_established_resp_side:
            #fsts_id doesn't matter, no actual session exists
            responder.send_test_ack_request('0')
            initiator.wait_for_session_event(5, ["EVENT_FST_SESSION_STATE"])
        elif bad_scenario_type == bad_scenario_ack_resp_session_not_established_init_side:
            #fsts_id doesn't matter, no actual session exists
            initiator.send_test_ack_response('0')
            responder.wait_for_session_event(5, ["EVENT_FST_SESSION_STATE"])
        elif bad_scenario_type == bad_scenario_ack_resp_session_not_established_resp_side:
            #fsts_id doesn't matter, no actual session exists
            responder.send_test_ack_response('0')
            initiator.wait_for_session_event(5, ["EVENT_FST_SESSION_STATE"])
        elif bad_scenario_type == bad_scenario_ack_req_bad_fsts_id:
            initiator.send_test_ack_request('-1')
            responder.wait_for_session_event(5, ["EVENT_FST_SESSION_STATE"])
        elif bad_scenario_type == bad_scenario_ack_resp_bad_fsts_id:
            initiator.send_test_ack_response('-1')
            responder.wait_for_session_event(5, ["EVENT_FST_SESSION_STATE"])
        elif bad_scenario_type == bad_scenario_ack_resp_no_ack_req:
            actual_fsts_id = initiator.get_fsts_id_by_sid(sid)
            initiator.send_test_ack_response(str(actual_fsts_id))
            responder.wait_for_session_event(5, ["EVENT_FST_SESSION_STATE"])
        else:
            raise Exception("Unknown bad scenario identifier")
    except Exception as e:
        if e.args[0].startswith("No FST-EVENT-SESSION received"):
            bad_parameter_detected = True
        if not bad_parameter_detected:
            # The exception was unexpected
            logger.info(e)
            exception_already_raised = True
            raise
    finally:
        fst_module_aux.disconnect_two_ap_sta_pairs(ap1, ap2, sta1, sta2)
        fst_module_aux.stop_two_ap_sta_pairs(ap1, ap2, sta1, sta2)
        if not exception_already_raised:
            if bad_parameter_detected:
                logger.info("Success. Bad scenario was handled correctly (%s)" % bad_scenario_names[bad_scenario_type])
            else:
                raise Exception("Failure. Bad scenario was handled incorrectly (%s)" % bad_scenario_names[bad_scenario_type])
        else:
            logger.info("Failure. Unexpected exception")

def test_fst_sta_connect_to_non_fst_ap(dev, apdev, test_params):
    """FST STA connecting to non-FST AP"""
    ap1, ap2, sta1, sta2 = fst_module_aux.start_two_ap_sta_pairs(apdev)
    with HWSimRadio() as (radio, iface):
        non_fst_ap = hostapd.add_ap(iface, {"ssid": "non_fst_11g"})
        try:
            orig_sta1_mbies = sta1.get_local_mbies()
            orig_sta2_mbies = sta2.get_local_mbies()
            sta2.connect_to_external_ap(non_fst_ap, ssid="non_fst_11g",
                                        key_mgmt="NONE", scan_freq='2412')
            time.sleep(2)
            res_sta1_mbies = sta1.get_local_mbies()
            res_sta2_mbies = sta2.get_local_mbies()
            if (orig_sta1_mbies.startswith("FAIL") or
                orig_sta2_mbies.startswith("FAIL") or
                res_sta1_mbies.startswith("FAIL") or
                res_sta2_mbies.startswith("FAIL")):
                raise Exception("Failure. MB IEs must be present on the stations")
        except Exception as e:
            logger.info(e)
            raise
        finally:
            sta2.disconnect_from_external_ap()
            fst_module_aux.stop_two_ap_sta_pairs(ap1, ap2, sta1, sta2)
            hostapd.HostapdGlobal().remove(iface)

def test_fst_sta_connect_to_fst_ap(dev, apdev, test_params):
    """FST STA connecting to FST AP"""
    ap1, ap2, sta1, sta2 = fst_module_aux.start_two_ap_sta_pairs(apdev)
    try:
        orig_sta2_mbies = sta2.get_local_mbies()
        vals = sta1.scan(freq=fst_test_common.fst_test_def_freq_a)
        sta1.connect(ap1, key_mgmt="NONE",
                     scan_freq=fst_test_common.fst_test_def_freq_a)
        time.sleep(2)
        res_sta2_mbies = sta2.get_local_mbies()
        if res_sta2_mbies == orig_sta2_mbies:
            raise Exception("Failure. MB IEs have not been updated")
    except Exception as e:
        logger.info(e)
        raise
    finally:
        sta1.disconnect()
        fst_module_aux.stop_two_ap_sta_pairs(ap1, ap2, sta1, sta2)

def test_fst_ap_connect_to_fst_sta(dev, apdev, test_params):
    """FST AP connecting to FST STA"""
    ap1, ap2, sta1, sta2 = fst_module_aux.start_two_ap_sta_pairs(apdev)
    try:
        orig_ap_mbies = ap1.get_local_mbies()
        vals = sta1.scan(freq=fst_test_common.fst_test_def_freq_a)
        sta1.connect(ap1, key_mgmt="NONE",
                     scan_freq=fst_test_common.fst_test_def_freq_a)
        time.sleep(2)
        res_ap_mbies = ap1.get_local_mbies()
        if res_ap_mbies != orig_ap_mbies:
            raise Exception("Failure. MB IEs have been unexpectedly updated on the AP")
    except Exception as e:
        logger.info(e)
        raise
    finally:
        sta1.disconnect()
        fst_module_aux.stop_two_ap_sta_pairs(ap1, ap2, sta1, sta2)

def test_fst_ap_connect_to_non_fst_sta(dev, apdev, test_params):
    """FST AP connecting to non-FST STA"""
    ap1, ap2, sta1, sta2 = fst_module_aux.start_two_ap_sta_pairs(apdev)
    try:
        orig_ap_mbies = ap2.get_local_mbies()
        vals = dev[0].scan(None, fst_test_common.fst_test_def_freq_g)
        fst_module_aux.external_sta_connect(dev[0], ap2, key_mgmt="NONE",
                                            scan_freq=fst_test_common.fst_test_def_freq_g)
        time.sleep(2)
        res_ap_mbies = ap2.get_local_mbies()
        if res_ap_mbies != orig_ap_mbies:
            raise Exception("Failure. MB IEs have been unexpectedly updated on the AP")
    except Exception as e:
        logger.info(e)
        raise
    finally:
        fst_module_aux.disconnect_external_sta(dev[0], ap2)
        fst_module_aux.stop_two_ap_sta_pairs(ap1, ap2, sta1, sta2)

def test_fst_second_sta_connect_to_non_fst_ap(dev, apdev, test_params):
    """FST STA 2nd connecting to non-FST AP"""
    fst_ap1, fst_ap2, sta1, sta2 = fst_module_aux.start_two_ap_sta_pairs(apdev)
    with HWSimRadio() as (radio, iface):
        non_fst_ap = hostapd.add_ap(iface, {"ssid": "non_fst_11g"})
        try:
            vals = sta1.scan(freq=fst_test_common.fst_test_def_freq_a)
            sta1.connect(fst_ap1, key_mgmt="NONE", scan_freq=fst_test_common.fst_test_def_freq_a)
            time.sleep(2)
            orig_sta1_mbies = sta1.get_local_mbies()
            orig_sta2_mbies = sta2.get_local_mbies()
            sta2.connect_to_external_ap(non_fst_ap, ssid="non_fst_11g",
                                        key_mgmt="NONE", scan_freq='2412')
            time.sleep(2)
            res_sta1_mbies = sta1.get_local_mbies()
            res_sta2_mbies = sta2.get_local_mbies()
            if (orig_sta1_mbies.startswith("FAIL") or
                orig_sta2_mbies.startswith("FAIL") or
                res_sta1_mbies.startswith("FAIL") or
                res_sta2_mbies.startswith("FAIL")):
                raise Exception("Failure. MB IEs must be present on the stations")
        except Exception as e:
            logger.info(e)
            raise
        finally:
            sta1.disconnect()
            sta2.disconnect_from_external_ap()
            fst_module_aux.stop_two_ap_sta_pairs(fst_ap1, fst_ap2, sta1, sta2)
            hostapd.HostapdGlobal().remove(iface)

def test_fst_second_sta_connect_to_fst_ap(dev, apdev, test_params):
    """FST STA 2nd connecting to FST AP"""
    fst_ap1, fst_ap2, sta1, sta2 = fst_module_aux.start_two_ap_sta_pairs(apdev)
    with HWSimRadio() as (radio, iface):
        non_fst_ap = hostapd.add_ap(iface, {"ssid": "non_fst_11g"})
        try:
            sta2.connect_to_external_ap(non_fst_ap, ssid="non_fst_11g",
                                        key_mgmt="NONE", scan_freq='2412')
            time.sleep(2)
            orig_sta1_mbies = sta1.get_local_mbies()
            orig_sta2_mbies = sta2.get_local_mbies()
            vals = sta1.scan(freq=fst_test_common.fst_test_def_freq_a)
            sta1.connect(fst_ap1, key_mgmt="NONE", scan_freq=fst_test_common.fst_test_def_freq_a)
            time.sleep(2)
            res_sta1_mbies = sta1.get_local_mbies()
            res_sta2_mbies = sta2.get_local_mbies()
            if (orig_sta1_mbies.startswith("FAIL") or
                orig_sta2_mbies.startswith("FAIL") or
                res_sta1_mbies.startswith("FAIL") or
                res_sta2_mbies.startswith("FAIL")):
                raise Exception("Failure. MB IEs must be present on the stations")
        except Exception as e:
            logger.info(e)
            raise
        finally:
            sta1.disconnect()
            sta2.disconnect_from_external_ap()
            fst_module_aux.stop_two_ap_sta_pairs(fst_ap1, fst_ap2, sta1, sta2)
            hostapd.HostapdGlobal().remove(iface)

def test_fst_disconnect_1_of_2_stas_from_non_fst_ap(dev, apdev, test_params):
    """FST disconnect 1 of 2 STAs from non-FST AP"""
    fst_ap1, fst_ap2, sta1, sta2 = fst_module_aux.start_two_ap_sta_pairs(apdev)
    with HWSimRadio() as (radio, iface):
        non_fst_ap = hostapd.add_ap(iface, {"ssid": "non_fst_11g"})
        try:
            vals = sta1.scan(freq=fst_test_common.fst_test_def_freq_a)
            sta1.connect(fst_ap1, key_mgmt="NONE", scan_freq=fst_test_common.fst_test_def_freq_a)
            sta2.connect_to_external_ap(non_fst_ap, ssid="non_fst_11g",
                                        key_mgmt="NONE", scan_freq='2412')
            time.sleep(2)
            orig_sta1_mbies = sta1.get_local_mbies()
            orig_sta2_mbies = sta2.get_local_mbies()
            sta2.disconnect_from_external_ap()
            time.sleep(2)
            res_sta1_mbies = sta1.get_local_mbies()
            res_sta2_mbies = sta2.get_local_mbies()
            if (orig_sta1_mbies.startswith("FAIL") or
                orig_sta2_mbies.startswith("FAIL") or
                res_sta1_mbies.startswith("FAIL") or
                res_sta2_mbies.startswith("FAIL")):
                raise Exception("Failure. MB IEs must be present on the stations")
        except Exception as e:
            logger.info(e)
            raise
        finally:
            sta1.disconnect()
            sta2.disconnect_from_external_ap()
            fst_module_aux.stop_two_ap_sta_pairs(fst_ap1, fst_ap2, sta1, sta2)
            hostapd.HostapdGlobal().remove(iface)

def test_fst_disconnect_1_of_2_stas_from_fst_ap(dev, apdev, test_params):
    """FST disconnect 1 of 2 STAs from FST AP"""
    fst_ap1, fst_ap2, sta1, sta2 = fst_module_aux.start_two_ap_sta_pairs(apdev)
    with HWSimRadio() as (radio, iface):
        non_fst_ap = hostapd.add_ap(iface, {"ssid": "non_fst_11g"})
        try:
            vals = sta1.scan(freq=fst_test_common.fst_test_def_freq_a)
            sta1.connect(fst_ap1, key_mgmt="NONE", scan_freq=fst_test_common.fst_test_def_freq_a)
            sta2.connect_to_external_ap(non_fst_ap, ssid="non_fst_11g",
                                        key_mgmt="NONE", scan_freq='2412')
            time.sleep(2)
            orig_sta1_mbies = sta1.get_local_mbies()
            orig_sta2_mbies = sta2.get_local_mbies()
            sta1.disconnect()
            time.sleep(2)
            res_sta1_mbies = sta1.get_local_mbies()
            res_sta2_mbies = sta2.get_local_mbies()
            if (orig_sta1_mbies.startswith("FAIL") or
                orig_sta2_mbies.startswith("FAIL") or
                res_sta1_mbies.startswith("FAIL") or
                res_sta2_mbies.startswith("FAIL")):
                raise Exception("Failure. MB IEs must be present on the stations")
        except Exception as e:
            logger.info(e)
            raise
        finally:
            sta1.disconnect()
            sta2.disconnect_from_external_ap()
            fst_module_aux.stop_two_ap_sta_pairs(fst_ap1, fst_ap2, sta1, sta2)
            hostapd.HostapdGlobal().remove(iface)

def test_fst_disconnect_2_of_2_stas_from_non_fst_ap(dev, apdev, test_params):
    """FST disconnect 2 of 2 STAs from non-FST AP"""
    fst_ap1, fst_ap2, sta1, sta2 = fst_module_aux.start_two_ap_sta_pairs(apdev)
    with HWSimRadio() as (radio, iface):
        non_fst_ap = hostapd.add_ap(iface, {"ssid": "non_fst_11g"})
        try:
            vals = sta1.scan(freq=fst_test_common.fst_test_def_freq_a)
            sta1.connect(fst_ap1, key_mgmt="NONE", scan_freq=fst_test_common.fst_test_def_freq_a)
            sta2.connect_to_external_ap(non_fst_ap, ssid="non_fst_11g",
                                        key_mgmt="NONE", scan_freq='2412')
            time.sleep(2)
            sta1.disconnect()
            time.sleep(2)
            orig_sta1_mbies = sta1.get_local_mbies()
            orig_sta2_mbies = sta2.get_local_mbies()
            sta2.disconnect_from_external_ap()
            time.sleep(2)
            res_sta1_mbies = sta1.get_local_mbies()
            res_sta2_mbies = sta2.get_local_mbies()
            if (orig_sta1_mbies.startswith("FAIL") or
                orig_sta2_mbies.startswith("FAIL") or
                res_sta1_mbies.startswith("FAIL") or
                res_sta2_mbies.startswith("FAIL")):
                raise Exception("Failure. MB IEs must be present on the stations")
        except Exception as e:
            logger.info(e)
            raise
        finally:
            sta1.disconnect()
            sta2.disconnect_from_external_ap()
            fst_module_aux.stop_two_ap_sta_pairs(fst_ap1, fst_ap2, sta1, sta2)
            hostapd.HostapdGlobal().remove(iface)

def test_fst_disconnect_2_of_2_stas_from_fst_ap(dev, apdev, test_params):
    """FST disconnect 2 of 2 STAs from FST AP"""
    fst_ap1, fst_ap2, sta1, sta2 = fst_module_aux.start_two_ap_sta_pairs(apdev)
    with HWSimRadio() as (radio, iface):
        non_fst_ap = hostapd.add_ap(iface, {"ssid": "non_fst_11g"})
        try:
            vals = sta1.scan(freq=fst_test_common.fst_test_def_freq_a)
            sta1.connect(fst_ap1, key_mgmt="NONE", scan_freq=fst_test_common.fst_test_def_freq_a)
            sta2.connect_to_external_ap(non_fst_ap, ssid="non_fst_11g",
                                        key_mgmt="NONE", scan_freq='2412')
            time.sleep(2)
            sta2.disconnect_from_external_ap()
            time.sleep(2)
            orig_sta1_mbies = sta1.get_local_mbies()
            orig_sta2_mbies = sta2.get_local_mbies()
            sta1.disconnect()
            time.sleep(2)
            res_sta1_mbies = sta1.get_local_mbies()
            res_sta2_mbies = sta2.get_local_mbies()
            if (orig_sta1_mbies.startswith("FAIL") or
                orig_sta2_mbies.startswith("FAIL") or
                res_sta1_mbies.startswith("FAIL") or
                res_sta2_mbies.startswith("FAIL")):
                raise Exception("Failure. MB IEs should have stayed present on both stations")
            # Mandatory part of 8.4.2.140 Multi-band element is 24 bytes = 48 hex chars
            basic_sta1_mbies = res_sta1_mbies[0:48] + res_sta1_mbies[60:108]
            basic_sta2_mbies = res_sta2_mbies[0:48] + res_sta2_mbies[60:108]
            if (basic_sta1_mbies != basic_sta2_mbies):
                raise Exception("Failure. Basic MB IEs should have become identical on both stations")
            addr_sta1_str = sta1.get_own_mac_address().replace(":", "")
            addr_sta2_str = sta2.get_own_mac_address().replace(":", "")
            # Mandatory part of 8.4.2.140 Multi-band element is followed by STA MAC Address field (6 bytes = 12 hex chars)
            addr_sta1_mbie1 = res_sta1_mbies[48:60]
            addr_sta1_mbie2 = res_sta1_mbies[108:120]
            addr_sta2_mbie1 = res_sta2_mbies[48:60]
            addr_sta2_mbie2 = res_sta2_mbies[108:120]
            if (addr_sta1_mbie1 != addr_sta1_mbie2 or
                addr_sta1_mbie1 != addr_sta2_str or
                addr_sta2_mbie1 != addr_sta2_mbie2 or
                addr_sta2_mbie1 != addr_sta1_str):
                raise Exception("Failure. STA Address in MB IEs should have been same as the other STA's")
        except Exception as e:
            logger.info(e)
            raise
        finally:
            sta1.disconnect()
            sta2.disconnect_from_external_ap()
            fst_module_aux.stop_two_ap_sta_pairs(fst_ap1, fst_ap2, sta1, sta2)
            hostapd.HostapdGlobal().remove(iface)

def test_fst_disconnect_non_fst_sta(dev, apdev, test_params):
    """FST disconnect non-FST STA"""
    ap1, ap2, fst_sta1, fst_sta2 = fst_module_aux.start_two_ap_sta_pairs(apdev)
    external_sta_connected = False
    try:
        vals = fst_sta1.scan(freq=fst_test_common.fst_test_def_freq_a)
        fst_sta1.connect(ap1, key_mgmt="NONE",
                         scan_freq=fst_test_common.fst_test_def_freq_a)
        vals = dev[0].scan(None, fst_test_common.fst_test_def_freq_g)
        fst_module_aux.external_sta_connect(dev[0], ap2, key_mgmt="NONE",
                                            scan_freq=fst_test_common.fst_test_def_freq_g)
        external_sta_connected = True
        time.sleep(2)
        fst_sta1.disconnect()
        time.sleep(2)
        orig_ap_mbies = ap2.get_local_mbies()
        fst_module_aux.disconnect_external_sta(dev[0], ap2)
        external_sta_connected = False
        time.sleep(2)
        res_ap_mbies = ap2.get_local_mbies()
        if res_ap_mbies != orig_ap_mbies:
            raise Exception("Failure. MB IEs have been unexpectedly updated on the AP")
    except Exception as e:
        logger.info(e)
        raise
    finally:
        fst_sta1.disconnect()
        if external_sta_connected:
            fst_module_aux.disconnect_external_sta(dev[0], ap2)
        fst_module_aux.stop_two_ap_sta_pairs(ap1, ap2, fst_sta1, fst_sta2)

def test_fst_disconnect_fst_sta(dev, apdev, test_params):
    """FST disconnect FST STA"""
    ap1, ap2, fst_sta1, fst_sta2 = fst_module_aux.start_two_ap_sta_pairs(apdev)
    external_sta_connected = False
    try:
        vals = fst_sta1.scan(freq=fst_test_common.fst_test_def_freq_a)
        fst_sta1.connect(ap1, key_mgmt="NONE",
                         scan_freq=fst_test_common.fst_test_def_freq_a)
        vals = dev[0].scan(None, fst_test_common.fst_test_def_freq_g)
        fst_module_aux.external_sta_connect(dev[0], ap2, key_mgmt="NONE",
                                            scan_freq=fst_test_common.fst_test_def_freq_g)
        external_sta_connected = True
        time.sleep(2)
        fst_module_aux.disconnect_external_sta(dev[0], ap2)
        external_sta_connected = False
        time.sleep(2)
        orig_ap_mbies = ap2.get_local_mbies()
        fst_sta1.disconnect()
        time.sleep(2)
        res_ap_mbies = ap2.get_local_mbies()
        if res_ap_mbies != orig_ap_mbies:
            raise Exception("Failure. MB IEs have been unexpectedly updated on the AP")
    except Exception as e:
        logger.info(e)
        raise
    finally:
        fst_sta1.disconnect()
        if external_sta_connected:
            fst_module_aux.disconnect_external_sta(dev[0], ap2)
        fst_module_aux.stop_two_ap_sta_pairs(ap1, ap2, fst_sta1, fst_sta2)

def test_fst_dynamic_iface_attach(dev, apdev, test_params):
    """FST dynamic interface attach"""
    ap1 = fst_module_aux.FstAP(apdev[0]['ifname'], 'fst_11a', 'a',
                               fst_test_common.fst_test_def_chan_a,
                               fst_test_common.fst_test_def_group,
                               fst_test_common.fst_test_def_prio_low,
                               fst_test_common.fst_test_def_llt)
    ap1.start()
    ap2 = fst_module_aux.FstAP(apdev[1]['ifname'], 'fst_11g', 'b',
                               fst_test_common.fst_test_def_chan_g,
                               '', '', '')
    ap2.start()

    sta1 = fst_module_aux.FstSTA('wlan5',
                                 fst_test_common.fst_test_def_group,
                                 fst_test_common.fst_test_def_prio_low,
                                 fst_test_common.fst_test_def_llt)
    sta1.start()
    sta2 = fst_module_aux.FstSTA('wlan6', '', '', '')
    sta2.start()

    try:
        orig_sta2_mbies = sta2.get_local_mbies()
        orig_ap2_mbies = ap2.get_local_mbies()
        sta2.send_iface_attach_request(sta2.ifname(),
                                       fst_test_common.fst_test_def_group,
                                       '52', '27')
        event = sta2.wait_for_iface_event(5)
        if event['event_type'] != 'attached':
            raise Exception("Failure. Iface was not properly attached")
        ap2.send_iface_attach_request(ap2.ifname(),
                                      fst_test_common.fst_test_def_group,
                                      '102', '77')
        event = ap2.wait_for_iface_event(5)
        if event['event_type'] != 'attached':
            raise Exception("Failure. Iface was not properly attached")
        time.sleep(2)
        res_sta2_mbies = sta2.get_local_mbies()
        res_ap2_mbies = ap2.get_local_mbies()
        sta2.send_iface_detach_request(sta2.ifname())
        event = sta2.wait_for_iface_event(5)
        if event['event_type'] != 'detached':
            raise Exception("Failure. Iface was not properly detached")
        ap2.send_iface_detach_request(ap2.ifname())
        event = ap2.wait_for_iface_event(5)
        if event['event_type'] != 'detached':
            raise Exception("Failure. Iface was not properly detached")
        if (not orig_sta2_mbies.startswith("FAIL") or
            not orig_ap2_mbies.startswith("FAIL") or
            res_sta2_mbies.startswith("FAIL") or
            res_ap2_mbies.startswith("FAIL")):
            raise Exception("Failure. MB IEs should have appeared on the station and on the AP")
    except Exception as e:
        logger.info(e)
        raise
    finally:
        ap1.stop()
        ap2.stop()
        sta1.stop()
        sta2.stop()

# AP side FST module tests

def test_fst_ap_start_session(dev, apdev, test_params):
    """FST AP start session"""
    fst_start_session(apdev, test_params, bad_param_none, True)

def test_fst_ap_start_session_no_add_params(dev, apdev, test_params):
    """FST AP start session - no add params"""
    fst_start_session(apdev, test_params, bad_param_session_add_no_params, True)

def test_fst_ap_start_session_bad_group_id(dev, apdev, test_params):
    """FST AP start session - bad group id"""
    fst_start_session(apdev, test_params, bad_param_group_id, True)

def test_fst_ap_start_session_no_set_params(dev, apdev, test_params):
    """FST AP start session - no set params"""
    fst_start_session(apdev, test_params, bad_param_session_set_no_params, True)

def test_fst_ap_start_session_set_unknown_param(dev, apdev, test_params):
    """FST AP start session - set unknown param"""
    fst_start_session(apdev, test_params, bad_param_session_set_unknown_param,
                      True)

def test_fst_ap_start_session_bad_session_id(dev, apdev, test_params):
    """FST AP start session - bad session id"""
    fst_start_session(apdev, test_params, bad_param_session_id, True)

def test_fst_ap_start_session_bad_new_iface(dev, apdev, test_params):
    """FST AP start session - bad new iface"""
    fst_start_session(apdev, test_params, bad_param_new_iface, True)

def test_fst_ap_start_session_bad_old_iface(dev, apdev, test_params):
    """FST AP start session - bad old iface"""
    fst_start_session(apdev, test_params, bad_param_old_iface, True)

def test_fst_ap_start_session_negative_llt(dev, apdev, test_params):
    """FST AP start session - negative llt"""
    fst_start_session(apdev, test_params, bad_param_negative_llt, True)

def test_fst_ap_start_session_zero_llt(dev, apdev, test_params):
    """FST AP start session - zero llt"""
    fst_start_session(apdev, test_params, bad_param_zero_llt, True)

def test_fst_ap_start_session_llt_too_big(dev, apdev, test_params):
    """FST AP start session - llt too large"""
    fst_start_session(apdev, test_params, bad_param_llt_too_big, True)

def test_fst_ap_start_session_invalid_peer_addr(dev, apdev, test_params):
    """FST AP start session - invalid peer address"""
    fst_start_session(apdev, test_params, bad_param_peer_addr, True,
                      'GG:GG:GG:GG:GG:GG')

def test_fst_ap_start_session_multicast_peer_addr(dev, apdev, test_params):
    """FST AP start session - multicast peer address"""
    fst_start_session(apdev, test_params, bad_param_peer_addr, True,
                      '01:00:11:22:33:44')

def test_fst_ap_start_session_broadcast_peer_addr(dev, apdev, test_params):
    """FST AP start session - broadcast peer address"""
    fst_start_session(apdev, test_params, bad_param_peer_addr, True,
                      'FF:FF:FF:FF:FF:FF')

def test_fst_ap_initiate_session(dev, apdev, test_params):
    """FST AP initiate session"""
    fst_initiate_session(apdev, test_params, bad_param_none, True)

def test_fst_ap_initiate_session_no_params(dev, apdev, test_params):
    """FST AP initiate session - no params"""
    fst_initiate_session(apdev, test_params,
                         bad_param_session_initiate_no_params, True)

def test_fst_ap_initiate_session_invalid_session_id(dev, apdev, test_params):
    """FST AP initiate session - invalid session id"""
    fst_initiate_session(apdev, test_params,
                         bad_param_session_initiate_bad_session_id, True)

def test_fst_ap_initiate_session_no_new_iface(dev, apdev, test_params):
    """FST AP initiate session - no new iface"""
    fst_initiate_session(apdev, test_params,
                         bad_param_session_initiate_with_no_new_iface_set, True)

def test_fst_ap_initiate_session_bad_peer_addr(dev, apdev, test_params):
    """FST AP initiate session - bad peer address"""
    fst_initiate_session(apdev, test_params,
                         bad_param_session_initiate_with_bad_peer_addr_set,
                         True)

def test_fst_ap_initiate_session_request_with_bad_stie(dev, apdev, test_params):
    """FST AP initiate session - request with bad stie"""
    fst_initiate_session(apdev, test_params,
                         bad_param_session_initiate_request_with_bad_stie, True)

def test_fst_ap_initiate_session_response_with_reject(dev, apdev, test_params):
    """FST AP initiate session - response with reject"""
    fst_initiate_session(apdev, test_params,
                         bad_param_session_initiate_response_with_reject, True)

def test_fst_ap_initiate_session_response_with_bad_stie(dev, apdev,
                                                        test_params):
    """FST AP initiate session - response with bad stie"""
    fst_initiate_session(apdev, test_params,
                         bad_param_session_initiate_response_with_bad_stie,
                         True)

def test_fst_ap_initiate_session_response_with_zero_llt(dev, apdev,
                                                        test_params):
    """FST AP initiate session - zero llt"""
    fst_initiate_session(apdev, test_params,
                         bad_param_session_initiate_response_with_zero_llt,
                         True)

def test_fst_ap_initiate_session_stt_no_response(dev, apdev, test_params):
    """FST AP initiate session - stt no response"""
    fst_initiate_session(apdev, test_params,
                         bad_param_session_initiate_stt_no_response, True)

def test_fst_ap_initiate_session_concurrent_setup_request(dev, apdev,
                                                          test_params):
    """FST AP initiate session - concurrent setup request"""
    fst_initiate_session(apdev, test_params,
                         bad_param_session_initiate_concurrent_setup_request,
                         True)

def test_fst_ap_session_request_with_no_session(dev, apdev, test_params):
    """FST AP session request with no session"""
    fst_send_unexpected_frame(apdev, test_params, frame_type_session_request,
                              True)

def test_fst_ap_session_response_accept_with_no_session(dev, apdev,
                                                        test_params):
    """FST AP session response accept with no session"""
    fst_send_unexpected_frame(apdev, test_params, frame_type_session_response,
                              True, "accept")

def test_fst_ap_session_response_reject_with_no_session(dev, apdev,
                                                        test_params):
    """FST AP session response reject with no session"""
    fst_send_unexpected_frame(apdev, test_params, frame_type_session_response,
                              True, "reject")

def test_fst_ap_ack_request_with_no_session(dev, apdev, test_params):
    """FST AP ack request with no session"""
    fst_send_unexpected_frame(apdev, test_params, frame_type_ack_request, True)

def test_fst_ap_ack_response_with_no_session(dev, apdev, test_params):
    """FST AP ack response with no session"""
    fst_send_unexpected_frame(apdev, test_params, frame_type_ack_response, True)

def test_fst_ap_tear_down_response_with_no_session(dev, apdev, test_params):
    """FST AP tear down response with no session"""
    fst_send_unexpected_frame(apdev, test_params, frame_type_tear_down, True)

def test_fst_ap_transfer_session(dev, apdev, test_params):
    """FST AP transfer session"""
    fst_transfer_session(apdev, test_params, bad_param_none, True)

def test_fst_ap_transfer_session_no_params(dev, apdev, test_params):
    """FST AP transfer session - no params"""
    fst_transfer_session(apdev, test_params,
                         bad_param_session_transfer_no_params, True)

def test_fst_ap_transfer_session_bad_session_id(dev, apdev, test_params):
    """FST AP transfer session - bad session id"""
    fst_transfer_session(apdev, test_params,
                         bad_param_session_transfer_bad_session_id, True)

def test_fst_ap_transfer_session_setup_skipped(dev, apdev, test_params):
    """FST AP transfer session - setup skipped"""
    fst_transfer_session(apdev, test_params,
                         bad_param_session_transfer_setup_skipped, True)

def test_fst_ap_ack_request_with_session_not_set_up(dev, apdev, test_params):
    """FST AP ack request with session not set up"""
    fst_bad_transfer(apdev, test_params,
                     bad_scenario_ack_req_session_not_set_up, True)

def test_fst_ap_ack_request_with_session_not_established_init_side(dev, apdev,
                                                                   test_params):
    """FST AP ack request with session not established init side"""
    fst_bad_transfer(apdev, test_params,
                     bad_scenario_ack_req_session_not_established_init_side,
                     True)

def test_fst_ap_ack_request_with_session_not_established_resp_side(dev, apdev,
                                                                   test_params):
    """FST AP ack request with session not established resp side"""
    fst_bad_transfer(apdev, test_params,
                     bad_scenario_ack_req_session_not_established_resp_side,
                     True)

def test_fst_ap_ack_request_with_bad_fsts_id(dev, apdev, test_params):
    """FST AP ack request with bad fsts id"""
    fst_bad_transfer(apdev, test_params, bad_scenario_ack_req_bad_fsts_id, True)

def test_fst_ap_ack_response_with_session_not_set_up(dev, apdev, test_params):
    """FST AP ack response with session not set up"""
    fst_bad_transfer(apdev, test_params,
                     bad_scenario_ack_resp_session_not_set_up, True)

def test_fst_ap_ack_response_with_session_not_established_init_side(dev, apdev, test_params):
    """FST AP ack response with session not established init side"""
    fst_bad_transfer(apdev, test_params,
                     bad_scenario_ack_resp_session_not_established_init_side,
                     True)

def test_fst_ap_ack_response_with_session_not_established_resp_side(dev, apdev, test_params):
    """FST AP ack response with session not established resp side"""
    fst_bad_transfer(apdev, test_params,
                     bad_scenario_ack_resp_session_not_established_resp_side,
                     True)

def test_fst_ap_ack_response_with_no_ack_request(dev, apdev, test_params):
    """FST AP ack response with no ack request"""
    fst_bad_transfer(apdev, test_params, bad_scenario_ack_resp_no_ack_req, True)

def test_fst_ap_tear_down_session(dev, apdev, test_params):
    """FST AP tear down session"""
    fst_tear_down_session(apdev, test_params, bad_param_none, True)

def test_fst_ap_tear_down_session_no_params(dev, apdev, test_params):
    """FST AP tear down session - no params"""
    fst_tear_down_session(apdev, test_params,
                          bad_param_session_teardown_no_params, True)

def test_fst_ap_tear_down_session_bad_session_id(dev, apdev, test_params):
    """FST AP tear down session - bad session id"""
    fst_tear_down_session(apdev, test_params,
                          bad_param_session_teardown_bad_session_id, True)

def test_fst_ap_tear_down_session_setup_skipped(dev, apdev, test_params):
    """FST AP tear down session - setup skipped"""
    fst_tear_down_session(apdev, test_params,
                          bad_param_session_teardown_setup_skipped, True)

def test_fst_ap_tear_down_session_bad_fsts_id(dev, apdev, test_params):
    """FST AP tear down session - bad fsts id"""
    fst_tear_down_session(apdev, test_params,
                          bad_param_session_teardown_bad_fsts_id, True)

def test_fst_ap_remove_session_not_established(dev, apdev, test_params):
    """FST AP remove session - not established"""
    fst_remove_session(apdev, test_params,
                       remove_scenario_non_established_session, True)

def test_fst_ap_remove_session_established(dev, apdev, test_params):
    """FST AP remove session - established"""
    fst_remove_session(apdev, test_params,
                       remove_scenario_established_session, True)

def test_fst_ap_remove_session_no_params(dev, apdev, test_params):
    """FST AP remove session - no params"""
    fst_remove_session(apdev, test_params, remove_scenario_no_params, True)

def test_fst_ap_remove_session_bad_session_id(dev, apdev, test_params):
    """FST AP remove session - bad session id"""
    fst_remove_session(apdev, test_params, remove_scenario_bad_session_id, True)

def test_fst_ap_ctrl_iface(dev, apdev, test_params):
    """FST control interface behavior"""
    hglobal = hostapd.HostapdGlobal()
    start_num_groups = 0
    res = hglobal.request("FST-MANAGER LIST_GROUPS")
    del hglobal
    if "FAIL" not in res:
        start_num_groups = len(res.splitlines())

    ap1, ap2, sta1, sta2 = fst_module_aux.start_two_ap_sta_pairs(apdev)
    try:
        fst_module_aux.connect_two_ap_sta_pairs(ap1, ap2, sta1, sta2)
        initiator = ap1
        responder = sta1
        initiator.add_peer(responder, None)
        initiator.set_fst_parameters(group_id=None)
        sid = initiator.add_session()
        res = initiator.get_session_params(sid)
        logger.info("Initial session params:\n" + str(res))
        if res['state'] != 'INITIAL':
            raise Exception("Unexpected state: " + res['state'])
        initiator.set_fst_parameters(llt=None)
        initiator.configure_session(sid, ap2.ifname(), None)
        res = initiator.get_session_params(sid)
        logger.info("Session params after configuration:\n" + str(res))
        res = initiator.iface_peers(initiator.ifname())
        logger.info("Interface peers: " + str(res))
        if len(res) != 1:
            raise Exception("Unexpected number of peers")
        res = initiator.get_peer_mbies(initiator.ifname(),
                                       initiator.get_new_peer_addr())
        logger.info("Peer MB IEs: " + str(res))
        res = initiator.list_ifaces()
        logger.info("Interfaces: " + str(res))
        if len(res) != 2:
            raise Exception("Unexpected number of interfaces")
        res = initiator.list_groups()
        logger.info("Groups: " + str(res))
        if len(res) != 1 + start_num_groups:
            raise Exception("Unexpected number of groups")

        tests = ["LIST_IFACES unknown",
                 "LIST_IFACES     unknown2",
                 "SESSION_GET 12345678",
                 "SESSION_SET " + sid + " unknown=foo",
                 "SESSION_RESPOND 12345678 foo",
                 "SESSION_RESPOND " + sid,
                 "SESSION_RESPOND " + sid + " foo",
                 "TEST_REQUEST foo",
                 "TEST_REQUEST SEND_SETUP_REQUEST",
                 "TEST_REQUEST SEND_SETUP_REQUEST foo",
                 "TEST_REQUEST SEND_SETUP_RESPONSE",
                 "TEST_REQUEST SEND_SETUP_RESPONSE foo",
                 "TEST_REQUEST SEND_ACK_REQUEST",
                 "TEST_REQUEST SEND_ACK_REQUEST foo",
                 "TEST_REQUEST SEND_ACK_RESPONSE",
                 "TEST_REQUEST SEND_ACK_RESPONSE foo",
                 "TEST_REQUEST SEND_TEAR_DOWN",
                 "TEST_REQUEST SEND_TEAR_DOWN foo",
                 "TEST_REQUEST GET_FSTS_ID",
                 "TEST_REQUEST GET_FSTS_ID foo",
                 "TEST_REQUEST GET_LOCAL_MBIES",
                 "TEST_REQUEST GET_LOCAL_MBIES foo",
                 "GET_PEER_MBIES",
                 "GET_PEER_MBIES ",
                 "GET_PEER_MBIES unknown",
                 "GET_PEER_MBIES unknown unknown",
                 "GET_PEER_MBIES unknown  " + initiator.get_new_peer_addr(),
                 "GET_PEER_MBIES " + initiator.ifname() + " 01:ff:ff:ff:ff:ff",
                 "GET_PEER_MBIES " + initiator.ifname() + " 00:ff:ff:ff:ff:ff",
                 "GET_PEER_MBIES " + initiator.ifname() + " 00:00:00:00:00:00",
                 "IFACE_PEERS",
                 "IFACE_PEERS ",
                 "IFACE_PEERS unknown",
                 "IFACE_PEERS unknown unknown",
                 "IFACE_PEERS " + initiator.fst_group,
                 "IFACE_PEERS " + initiator.fst_group + " unknown"]
        for t in tests:
            if "FAIL" not in initiator.grequest("FST-MANAGER " + t):
                raise Exception("Unexpected response for invalid FST-MANAGER command " + t)
        if "UNKNOWN FST COMMAND" not in initiator.grequest("FST-MANAGER unknown"):
            raise Exception("Unexpected response for unknown FST-MANAGER command")

        tests = ["FST-DETACH", "FST-DETACH ", "FST-DETACH unknown",
                 "FST-ATTACH", "FST-ATTACH ", "FST-ATTACH unknown",
                 "FST-ATTACH unknown unknown"]
        for t in tests:
            if "FAIL" not in initiator.grequest(t):
                raise Exception("Unexpected response for invalid command " + t)

        try:
            # Trying to add same interface again needs to fail.
            ap1.send_iface_attach_request(ap1.iface, ap1.fst_group,
                                          ap1.fst_llt, ap1.fst_pri)
            raise Exception("Duplicate FST-ATTACH succeeded")
        except Exception as e:
            if not str(e).startswith("Cannot attach"):
                raise

        try:
            ap1.get_fsts_id_by_sid("123")
        except Exception as e:
            if not str(e).startswith("Cannot get fsts_id for sid"):
                raise
    finally:
        fst_module_aux.disconnect_two_ap_sta_pairs(ap1, ap2, sta1, sta2)
        fst_module_aux.stop_two_ap_sta_pairs(ap1, ap2, sta1, sta2)

def test_fst_ap_start_session_oom(dev, apdev, test_params):
    """FST AP setup failing due to OOM"""
    ap1 = fst_module_aux.FstAP(apdev[0]['ifname'], 'fst_11a', 'a',
                               fst_test_common.fst_test_def_chan_a,
                               fst_test_common.fst_test_def_group,
                               fst_test_common.fst_test_def_prio_low,
                               fst_test_common.fst_test_def_llt)
    ap1.start()
    try:
        run_fst_ap_start_session_oom(apdev, ap1)
    finally:
        ap1.stop()
        fst_test_common.fst_clear_regdom()

def run_fst_ap_start_session_oom(apdev, ap1):
    with alloc_fail(ap1, 1, "fst_iface_create"):
        ap2_started = False
        try:
            ap2 = fst_module_aux.FstAP(apdev[1]['ifname'], 'fst_11g', 'b',
                                       fst_test_common.fst_test_def_chan_g,
                                       fst_test_common.fst_test_def_group,
                                       fst_test_common.fst_test_def_prio_high,
                                       fst_test_common.fst_test_def_llt)
            try:
                # This will fail in fst_iface_create() OOM
                ap2.start()
            except:
                pass
        finally:
            try:
                ap2.stop()
            except:
                pass

# STA side FST module tests

def test_fst_sta_start_session(dev, apdev, test_params):
    """FST STA start session"""
    fst_start_session(apdev, test_params, bad_param_none, False)

def test_fst_sta_start_session_no_add_params(dev, apdev, test_params):
    """FST STA start session - no add params"""
    fst_start_session(apdev, test_params, bad_param_session_add_no_params,
                      False)

def test_fst_sta_start_session_bad_group_id(dev, apdev, test_params):
    """FST STA start session - bad group id"""
    fst_start_session(apdev, test_params, bad_param_group_id, False)

def test_fst_sta_start_session_no_set_params(dev, apdev, test_params):
    """FST STA start session - no set params"""
    fst_start_session(apdev, test_params, bad_param_session_set_no_params,
                      False)

def test_fst_sta_start_session_set_unknown_param(dev, apdev, test_params):
    """FST STA start session - set unknown param"""
    fst_start_session(apdev, test_params, bad_param_session_set_unknown_param,
                      False)

def test_fst_sta_start_session_bad_session_id(dev, apdev, test_params):
    """FST STA start session - bad session id"""
    fst_start_session(apdev, test_params, bad_param_session_id, False)

def test_fst_sta_start_session_bad_new_iface(dev, apdev, test_params):
    """FST STA start session - bad new iface"""
    fst_start_session(apdev, test_params, bad_param_new_iface, False)

def test_fst_sta_start_session_bad_old_iface(dev, apdev, test_params):
    """FST STA start session - bad old iface"""
    fst_start_session(apdev, test_params, bad_param_old_iface, False)

def test_fst_sta_start_session_negative_llt(dev, apdev, test_params):
    """FST STA start session - negative llt"""
    fst_start_session(apdev, test_params, bad_param_negative_llt, False)

def test_fst_sta_start_session_zero_llt(dev, apdev, test_params):
    """FST STA start session - zero llt"""
    fst_start_session(apdev, test_params, bad_param_zero_llt, False)

def test_fst_sta_start_session_llt_too_big(dev, apdev, test_params):
    """FST STA start session - llt too large"""
    fst_start_session(apdev, test_params, bad_param_llt_too_big, False)

def test_fst_sta_start_session_invalid_peer_addr(dev, apdev, test_params):
    """FST STA start session - invalid peer address"""
    fst_start_session(apdev, test_params, bad_param_peer_addr, False,
                      'GG:GG:GG:GG:GG:GG')

def test_fst_sta_start_session_multicast_peer_addr(dev, apdev, test_params):
    """FST STA start session - multicast peer address"""
    fst_start_session(apdev, test_params, bad_param_peer_addr, False,
                      '11:00:11:22:33:44')

def test_fst_sta_start_session_broadcast_peer_addr(dev, apdev, test_params):
    """FST STA start session - broadcast peer addr"""
    fst_start_session(apdev, test_params, bad_param_peer_addr, False,
                      'FF:FF:FF:FF:FF:FF')

def test_fst_sta_initiate_session(dev, apdev, test_params):
    """FST STA initiate session"""
    fst_initiate_session(apdev, test_params, bad_param_none, False)

def test_fst_sta_initiate_session_no_params(dev, apdev, test_params):
    """FST STA initiate session - no params"""
    fst_initiate_session(apdev, test_params,
                         bad_param_session_initiate_no_params, False)

def test_fst_sta_initiate_session_invalid_session_id(dev, apdev, test_params):
    """FST STA initiate session - invalid session id"""
    fst_initiate_session(apdev, test_params,
                         bad_param_session_initiate_bad_session_id, False)

def test_fst_sta_initiate_session_no_new_iface(dev, apdev, test_params):
    """FST STA initiate session - no new iface"""
    fst_initiate_session(apdev, test_params,
                         bad_param_session_initiate_with_no_new_iface_set,
                         False)

def test_fst_sta_initiate_session_bad_peer_addr(dev, apdev, test_params):
    """FST STA initiate session - bad peer address"""
    fst_initiate_session(apdev, test_params,
                         bad_param_session_initiate_with_bad_peer_addr_set,
                         False)

def test_fst_sta_initiate_session_request_with_bad_stie(dev, apdev,
                                                        test_params):
    """FST STA initiate session - request with bad stie"""
    fst_initiate_session(apdev, test_params,
                         bad_param_session_initiate_request_with_bad_stie,
                         False)

def test_fst_sta_initiate_session_response_with_reject(dev, apdev, test_params):
    """FST STA initiate session - response with reject"""
    fst_initiate_session(apdev, test_params, bad_param_session_initiate_response_with_reject, False)

def test_fst_sta_initiate_session_response_with_bad_stie(dev, apdev, test_params):
    """FST STA initiate session - response with bad stie"""
    fst_initiate_session(apdev, test_params,
                         bad_param_session_initiate_response_with_bad_stie,
                         False)

def test_fst_sta_initiate_session_response_with_zero_llt(dev, apdev,
                                                         test_params):
    """FST STA initiate session - response with zero llt"""
    fst_initiate_session(apdev, test_params,
                         bad_param_session_initiate_response_with_zero_llt,
                         False)

def test_fst_sta_initiate_session_stt_no_response(dev, apdev, test_params):
    """FST STA initiate session - stt no response"""
    fst_initiate_session(apdev, test_params,
                         bad_param_session_initiate_stt_no_response, False)

def test_fst_sta_initiate_session_concurrent_setup_request(dev, apdev,
                                                           test_params):
    """FST STA initiate session - concurrent setup request"""
    fst_initiate_session(apdev, test_params,
                         bad_param_session_initiate_concurrent_setup_request,
                         False)

def test_fst_sta_session_request_with_no_session(dev, apdev, test_params):
    """FST STA session request with no session"""
    fst_send_unexpected_frame(apdev, test_params, frame_type_session_request,
                              False)

def test_fst_sta_session_response_accept_with_no_session(dev, apdev,
                                                         test_params):
    """FST STA session response accept with no session"""
    fst_send_unexpected_frame(apdev, test_params, frame_type_session_response,
                              False, "accept")

def test_fst_sta_session_response_reject_with_no_session(dev, apdev,
                                                         test_params):
    """FST STA session response reject with no session"""
    fst_send_unexpected_frame(apdev, test_params, frame_type_session_response,
                              False, "reject")

def test_fst_sta_ack_request_with_no_session(dev, apdev, test_params):
    """FST STA ack request with no session"""
    fst_send_unexpected_frame(apdev, test_params, frame_type_ack_request, False)

def test_fst_sta_ack_response_with_no_session(dev, apdev, test_params):
    """FST STA ack response with no session"""
    fst_send_unexpected_frame(apdev, test_params, frame_type_ack_response,
                              False)

def test_fst_sta_tear_down_response_with_no_session(dev, apdev, test_params):
    """FST STA tear down response with no session"""
    fst_send_unexpected_frame(apdev, test_params, frame_type_tear_down, False)

def test_fst_sta_transfer_session(dev, apdev, test_params):
    """FST STA transfer session"""
    fst_transfer_session(apdev, test_params, bad_param_none, False)

def test_fst_sta_transfer_session_no_params(dev, apdev, test_params):
    """FST STA transfer session - no params"""
    fst_transfer_session(apdev, test_params,
                         bad_param_session_transfer_no_params, False)

def test_fst_sta_transfer_session_bad_session_id(dev, apdev, test_params):
    """FST STA transfer session - bad session id"""
    fst_transfer_session(apdev, test_params,
                         bad_param_session_transfer_bad_session_id, False)

def test_fst_sta_transfer_session_setup_skipped(dev, apdev, test_params):
    """FST STA transfer session - setup skipped"""
    fst_transfer_session(apdev, test_params,
                         bad_param_session_transfer_setup_skipped, False)

def test_fst_sta_ack_request_with_session_not_set_up(dev, apdev, test_params):
    """FST STA ack request with session not set up"""
    fst_bad_transfer(apdev, test_params,
                     bad_scenario_ack_req_session_not_set_up, False)

def test_fst_sta_ack_request_with_session_not_established_init_side(dev, apdev, test_params):
    """FST STA ack request with session not established init side"""
    fst_bad_transfer(apdev, test_params,
                     bad_scenario_ack_req_session_not_established_init_side,
                     False)

def test_fst_sta_ack_request_with_session_not_established_resp_side(dev, apdev, test_params):
    """FST STA ack request with session not established resp side"""
    fst_bad_transfer(apdev, test_params,
                     bad_scenario_ack_req_session_not_established_resp_side,
                     False)

def test_fst_sta_ack_request_with_bad_fsts_id(dev, apdev, test_params):
    """FST STA ack request with bad fsts id"""
    fst_bad_transfer(apdev, test_params, bad_scenario_ack_req_bad_fsts_id,
                     False)

def test_fst_sta_ack_response_with_session_not_set_up(dev, apdev, test_params):
    """FST STA ack response with session not set up"""
    fst_bad_transfer(apdev, test_params,
                     bad_scenario_ack_resp_session_not_set_up, False)

def test_fst_sta_ack_response_with_session_not_established_init_side(dev, apdev, test_params):
    """FST STA ack response with session not established init side"""
    fst_bad_transfer(apdev, test_params,
                     bad_scenario_ack_resp_session_not_established_init_side,
                     False)

def test_fst_sta_ack_response_with_session_not_established_resp_side(dev, apdev, test_params):
    """FST STA ack response with session not established resp side"""
    fst_bad_transfer(apdev, test_params,
                     bad_scenario_ack_resp_session_not_established_resp_side,
                     False)

def test_fst_sta_ack_response_with_no_ack_request(dev, apdev, test_params):
    """FST STA ack response with no ack request"""
    fst_bad_transfer(apdev, test_params, bad_scenario_ack_resp_no_ack_req,
                     False)

def test_fst_sta_tear_down_session(dev, apdev, test_params):
    """FST STA tear down session"""
    fst_tear_down_session(apdev, test_params, bad_param_none, False)

def test_fst_sta_tear_down_session_no_params(dev, apdev, test_params):
    """FST STA tear down session - no params"""
    fst_tear_down_session(apdev, test_params,
                          bad_param_session_teardown_no_params, False)

def test_fst_sta_tear_down_session_bad_session_id(dev, apdev, test_params):
    """FST STA tear down session - bad session id"""
    fst_tear_down_session(apdev, test_params,
                          bad_param_session_teardown_bad_session_id, False)

def test_fst_sta_tear_down_session_setup_skipped(dev, apdev, test_params):
    """FST STA tear down session - setup skipped"""
    fst_tear_down_session(apdev, test_params,
                          bad_param_session_teardown_setup_skipped, False)

def test_fst_sta_tear_down_session_bad_fsts_id(dev, apdev, test_params):
    """FST STA tear down session - bad fsts id"""
    fst_tear_down_session(apdev, test_params,
                          bad_param_session_teardown_bad_fsts_id, False)

def test_fst_sta_remove_session_not_established(dev, apdev, test_params):
    """FST STA tear down session - not established"""
    fst_remove_session(apdev, test_params,
                       remove_scenario_non_established_session, False)

def test_fst_sta_remove_session_established(dev, apdev, test_params):
    """FST STA remove session - established"""
    fst_remove_session(apdev, test_params,
                       remove_scenario_established_session, False)

def test_fst_sta_remove_session_no_params(dev, apdev, test_params):
    """FST STA remove session - no params"""
    fst_remove_session(apdev, test_params, remove_scenario_no_params, False)

def test_fst_sta_remove_session_bad_session_id(dev, apdev, test_params):
    """FST STA remove session - bad session id"""
    fst_remove_session(apdev, test_params, remove_scenario_bad_session_id,
                       False)

def test_fst_rsn_ap_transfer_session(dev, apdev, test_params):
    """FST RSN AP transfer session"""
    fst_transfer_session(apdev, test_params, bad_param_none, True, rsn=True)

MGMT_SUBTYPE_ACTION = 13
ACTION_CATEG_FST = 18
FST_ACTION_SETUP_REQUEST = 0
FST_ACTION_SETUP_RESPONSE = 1
FST_ACTION_TEAR_DOWN = 2
FST_ACTION_ACK_REQUEST = 3
FST_ACTION_ACK_RESPONSE = 4
FST_ACTION_ON_CHANNEL_TUNNEL = 5

def hostapd_tx_and_status(hapd, msg):
    hapd.set("ext_mgmt_frame_handling", "1")
    hapd.mgmt_tx(msg)
    ev = hapd.wait_event(["MGMT-TX-STATUS"], timeout=1)
    if ev is None or "ok=1" not in ev:
        raise Exception("No ACK")
    hapd.set("ext_mgmt_frame_handling", "0")

def test_fst_proto(dev, apdev, test_params):
    """FST protocol testing"""
    ap1, ap2, sta1, sta2 = fst_module_aux.start_two_ap_sta_pairs(apdev)
    try:
        fst_module_aux.connect_two_ap_sta_pairs(ap1, ap2, sta1, sta2)
        hapd = ap1.get_instance()
        sta = sta1.get_instance()
        dst = sta.own_addr()
        src = apdev[0]['bssid']

        msg = {}
        msg['fc'] = MGMT_SUBTYPE_ACTION << 4
        msg['da'] = dst
        msg['sa'] = src
        msg['bssid'] = src

        # unknown FST Action (255) received!
        msg['payload'] = struct.pack("<BB", ACTION_CATEG_FST, 255)
        hostapd_tx_and_status(hapd, msg)

        # FST Request dropped: too short
        msg['payload'] = struct.pack("<BB", ACTION_CATEG_FST,
                                     FST_ACTION_SETUP_REQUEST)
        hostapd_tx_and_status(hapd, msg)

        # FST Request dropped: invalid STIE (EID)
        msg['payload'] = struct.pack("<BBBLBBLBBBBBBB", ACTION_CATEG_FST,
                                     FST_ACTION_SETUP_REQUEST, 0, 0,
                                     163, 11, 0, 0, 0, 0, 0, 0, 0, 0)
        hostapd_tx_and_status(hapd, msg)

        # FST Request dropped: invalid STIE (Len)
        msg['payload'] = struct.pack("<BBBLBBLBBBBBBB", ACTION_CATEG_FST,
                                     FST_ACTION_SETUP_REQUEST, 0, 0,
                                     164, 10, 0, 0, 0, 0, 0, 0, 0, 0)
        hostapd_tx_and_status(hapd, msg)

        # FST Request dropped: new and old band IDs are the same
        msg['payload'] = struct.pack("<BBBLBBLBBBBBBB", ACTION_CATEG_FST,
                                     FST_ACTION_SETUP_REQUEST, 0, 0,
                                     164, 11, 0, 0, 0, 0, 0, 0, 0, 0)
        hostapd_tx_and_status(hapd, msg)

        ifaces = sta1.list_ifaces()
        id = int(ifaces[0]['name'].split('|')[1])
        # FST Request dropped: new iface not found (new_band_id mismatch)
        msg['payload'] = struct.pack("<BBBLBBLBBBBBBB", ACTION_CATEG_FST,
                                     FST_ACTION_SETUP_REQUEST, 0, 0,
                                     164, 11, 0, 0, id + 1, 0, 0, 0, 0, 0)
        hostapd_tx_and_status(hapd, msg)

        # FST Action 'Setup Response' dropped: no session in progress found
        msg['payload'] = struct.pack("<BB", ACTION_CATEG_FST,
                                     FST_ACTION_SETUP_RESPONSE)
        hostapd_tx_and_status(hapd, msg)

        # Create session
        initiator = ap1
        responder = sta1
        new_iface = ap2.ifname()
        new_peer_addr = ap2.get_actual_peer_addr()
        resp_newif = sta2.ifname()
        peeraddr = None
        initiator.add_peer(responder, peeraddr, new_peer_addr)
        sid = initiator.add_session()
        initiator.configure_session(sid, new_iface)
        initiator.initiate_session(sid, "accept")

        # FST Response dropped due to wrong state: SETUP_COMPLETION
        msg['payload'] = struct.pack("<BB", ACTION_CATEG_FST,
                                     FST_ACTION_SETUP_RESPONSE)
        hostapd_tx_and_status(hapd, msg)

        # Too short FST Tear Down dropped
        msg['payload'] = struct.pack("<BB", ACTION_CATEG_FST,
                                     FST_ACTION_TEAR_DOWN)
        hostapd_tx_and_status(hapd, msg)

        # tear down for wrong FST Setup ID (0)
        msg['payload'] = struct.pack("<BBL", ACTION_CATEG_FST,
                                     FST_ACTION_TEAR_DOWN, 0)
        hostapd_tx_and_status(hapd, msg)

        # Ack received on wrong interface
        msg['payload'] = struct.pack("<BB", ACTION_CATEG_FST,
                                     FST_ACTION_ACK_REQUEST)
        hostapd_tx_and_status(hapd, msg)

        # Ack Response in inappropriate session state (SETUP_COMPLETION)
        msg['payload'] = struct.pack("<BB", ACTION_CATEG_FST,
                                     FST_ACTION_ACK_RESPONSE)
        hostapd_tx_and_status(hapd, msg)

        # Unsupported FST Action frame (On channel tunnel)
        msg['payload'] = struct.pack("<BB", ACTION_CATEG_FST,
                                     FST_ACTION_ON_CHANNEL_TUNNEL)
        hostapd_tx_and_status(hapd, msg)

        # FST Request dropped: new iface not found (new_band_id match)
        # FST Request dropped due to MAC comparison
        msg['payload'] = struct.pack("<BBBLBBLBBBBBBB", ACTION_CATEG_FST,
                                     FST_ACTION_SETUP_REQUEST, 0, 0,
                                     164, 11, 0, 0, id, 0, 0, 0, 0, 0)
        hostapd_tx_and_status(hapd, msg)

        hapd2 = ap2.get_instance()
        dst2 = sta2.get_instance().own_addr()
        src2 = apdev[1]['bssid']

        msg2 = {}
        msg2['fc'] = MGMT_SUBTYPE_ACTION << 4
        msg2['da'] = dst2
        msg2['sa'] = src2
        msg2['bssid'] = src2
        # FST Response dropped: wlan6 is not the old iface
        msg2['payload'] = struct.pack("<BB", ACTION_CATEG_FST,
                                      FST_ACTION_SETUP_RESPONSE)
        hostapd_tx_and_status(hapd2, msg2)

        sta.dump_monitor()

        group = ap1.fst_group
        ap1.send_iface_detach_request(ap1.iface)

        sta.flush_scan_cache()
        sta.request("REASSOCIATE")
        sta.wait_connected()

        # FST Request dropped due to no interface connection
        msg['payload'] = struct.pack("<BBBLBBLBBBBBBB", ACTION_CATEG_FST,
                                     FST_ACTION_SETUP_REQUEST, 0, 0,
                                     164, 11, 0, 0, id, 0, 0, 0, 0, 0)
        hostapd_tx_and_status(hapd, msg)
    finally:
        fst_module_aux.disconnect_two_ap_sta_pairs(ap1, ap2, sta1, sta2)
        try:
            fst_module_aux.stop_two_ap_sta_pairs(ap1, ap2, sta1, sta2)
        except:
            pass

def test_fst_setup_response_proto(dev, apdev, test_params):
    """FST protocol testing for Setup Response"""
    ap1, ap2, sta1, sta2 = fst_module_aux.start_two_ap_sta_pairs(apdev)
    try:
        fst_module_aux.connect_two_ap_sta_pairs(ap1, ap2, sta1, sta2)
        hapd = ap1.get_instance()
        sta = sta1.get_instance()
        dst = sta.own_addr()
        src = apdev[0]['bssid']

        sta1.add_peer(ap1, None, sta2.get_actual_peer_addr())
        sta1.set_fst_parameters(llt='0')
        sid = sta1.add_session()
        sta1.configure_session(sid, sta2.ifname())
        sta1.initiate_session(sid, "")

        msg = {}
        msg['fc'] = MGMT_SUBTYPE_ACTION << 4
        msg['da'] = dst
        msg['sa'] = src
        msg['bssid'] = src

        # Too short FST Response dropped
        msg['payload'] = struct.pack("<BB", ACTION_CATEG_FST,
                                     FST_ACTION_SETUP_RESPONSE)
        hostapd_tx_and_status(hapd, msg)

        # FST Response dropped: invalid STIE (EID)
        dialog_token = 1
        status_code = 0
        id = 0
        msg['payload'] = struct.pack("<BBBBBBLBBBBBBB", ACTION_CATEG_FST,
                                     FST_ACTION_SETUP_RESPONSE, dialog_token,
                                     status_code,
                                     163, 11, 0, 0, id, 0, 0, 0, 0, 0)
        hostapd_tx_and_status(hapd, msg)

        # FST Response dropped: invalid STIE (Len)
        dialog_token = 1
        status_code = 0
        id = 0
        msg['payload'] = struct.pack("<BBBBBBLBBBBBBB", ACTION_CATEG_FST,
                                     FST_ACTION_SETUP_RESPONSE, dialog_token,
                                     status_code,
                                     164, 10, 0, 0, id, 0, 0, 0, 0, 0)
        hostapd_tx_and_status(hapd, msg)

        # FST Response dropped due to wrong dialog token
        dialog_token = 123
        status_code = 0
        id = 0
        msg['payload'] = struct.pack("<BBBBBBLBBBBBBB", ACTION_CATEG_FST,
                                     FST_ACTION_SETUP_RESPONSE, dialog_token,
                                     status_code,
                                     164, 11, 0, 0, id, 0, 0, 0, 0, 0)
        hostapd_tx_and_status(hapd, msg)

        # FST Response dropped due to wrong FST Session ID
        dialog_token = 1
        status_code = 0
        id = 1
        msg['payload'] = struct.pack("<BBBBBBLBBBBBBB", ACTION_CATEG_FST,
                                     FST_ACTION_SETUP_RESPONSE, dialog_token,
                                     status_code,
                                     164, 11, int(sid) + 123456,
                                     0, id, 0, 0, 0, 0, 0)
        hostapd_tx_and_status(hapd, msg)

        # FST Response with non-zero status code
        dialog_token = 1
        status_code = 1
        id = 1
        msg['payload'] = struct.pack("<BBBBBBLBBBBBBB", ACTION_CATEG_FST,
                                     FST_ACTION_SETUP_RESPONSE, dialog_token,
                                     status_code,
                                     164, 11, int(sid), 0, id, 0, 0, 0, 0, 0)
        hostapd_tx_and_status(hapd, msg)
    finally:
        fst_module_aux.disconnect_two_ap_sta_pairs(ap1, ap2, sta1, sta2)
        fst_module_aux.stop_two_ap_sta_pairs(ap1, ap2, sta1, sta2)

def test_fst_ack_response_proto(dev, apdev, test_params):
    """FST protocol testing for Ack Response"""
    ap1, ap2, sta1, sta2 = fst_module_aux.start_two_ap_sta_pairs(apdev)
    try:
        fst_module_aux.connect_two_ap_sta_pairs(ap1, ap2, sta1, sta2)
        hapd = ap2.get_instance()
        sta = sta2.get_instance()
        dst = sta.own_addr()
        src = apdev[1]['bssid']

        sta1.add_peer(ap1, None, sta2.get_actual_peer_addr())
        sta1.set_fst_parameters(llt='0')
        sid = sta1.add_session()
        sta1.configure_session(sid, sta2.ifname())

        s = sta1.grequest("FST-MANAGER SESSION_INITIATE "+ sid)
        if not s.startswith('OK'):
            raise Exception("Cannot initiate fst session: %s" % s)
        ev = sta1.peer_obj.wait_gevent(["FST-EVENT-SESSION"], timeout=5)
        if ev is None:
            raise Exception("No FST-EVENT-SESSION received")
        event = fst_module_aux.parse_fst_session_event(ev)
        if event == None:
            raise Exception("Unrecognized FST event: " % ev)
        if event['type'] != 'EVENT_FST_SETUP':
            raise Exception("Expected FST_SETUP event, got: " + event['type'])
        ev = sta1.peer_obj.wait_gevent(["FST-EVENT-SESSION"], timeout=5)
        if ev is None:
            raise Exception("No FST-EVENT-SESSION received")
        event = fst_module_aux.parse_fst_session_event(ev)
        if event == None:
            raise Exception("Unrecognized FST event: " % ev)
        if event['type'] != 'EVENT_FST_SESSION_STATE':
            raise Exception("Expected EVENT_FST_SESSION_STATE event, got: " + event['type'])
        if event['new_state'] != "SETUP_COMPLETION":
            raise Exception("Expected new state SETUP_COMPLETION, got: " + event['new_state'])

        hapd.set("ext_mgmt_frame_handling", "1")
        s = sta1.peer_obj.grequest("FST-MANAGER SESSION_RESPOND "+ event['id'] + " accept")
        if not s.startswith('OK'):
            raise Exception("Error session_respond: %s" % s)
        req = hapd.mgmt_rx()
        if req is None:
            raise Exception("No Ack Request seen")
        msg = {}
        msg['fc'] = MGMT_SUBTYPE_ACTION << 4
        msg['da'] = dst
        msg['sa'] = src
        msg['bssid'] = src

        # Too short FST Ack Response dropped
        msg['payload'] = struct.pack("<BB", ACTION_CATEG_FST,
                                     FST_ACTION_ACK_RESPONSE)
        hapd.mgmt_tx(msg)
        ev = hapd.wait_event(["MGMT-TX-STATUS"], timeout=1)
        if ev is None or "ok=1" not in ev:
            raise Exception("No ACK")

        # Ack Response for wrong FSt Setup ID
        msg['payload'] = struct.pack("<BBBL", ACTION_CATEG_FST,
                                     FST_ACTION_ACK_RESPONSE,
                                     0, int(sid) + 123456)
        hostapd_tx_and_status(hapd, msg)
    finally:
        fst_module_aux.disconnect_two_ap_sta_pairs(ap1, ap2, sta1, sta2)
        fst_module_aux.stop_two_ap_sta_pairs(ap1, ap2, sta1, sta2)

def test_fst_ap_config_oom(dev, apdev, test_params):
    """FST AP configuration and OOM"""
    ap1 = fst_module_aux.FstAP(apdev[0]['ifname'], 'fst_11a', 'a',
                               fst_test_common.fst_test_def_chan_a,
                               fst_test_common.fst_test_def_group,
                               fst_test_common.fst_test_def_prio_low)
    hapd = ap1.start(return_early=True)
    with alloc_fail(hapd, 1, "fst_group_create"):
        res = ap1.grequest("FST-ATTACH %s %s" % (ap1.iface, ap1.fst_group))
        if not res.startswith("FAIL"):
            raise Exception("FST-ATTACH succeeded unexpectedly")

    with alloc_fail(hapd, 1, "fst_iface_create"):
        res = ap1.grequest("FST-ATTACH %s %s" % (ap1.iface, ap1.fst_group))
        if not res.startswith("FAIL"):
            raise Exception("FST-ATTACH succeeded unexpectedly")

    with alloc_fail(hapd, 1, "fst_group_create_mb_ie"):
        res = ap1.grequest("FST-ATTACH %s %s" % (ap1.iface, ap1.fst_group))
        # This is allowed to complete currently

    ap1.stop()
    fst_test_common.fst_clear_regdom()

def test_fst_send_oom(dev, apdev, test_params):
    """FST send action OOM"""
    ap1, ap2, sta1, sta2 = fst_module_aux.start_two_ap_sta_pairs(apdev)
    try:
        fst_module_aux.connect_two_ap_sta_pairs(ap1, ap2, sta1, sta2)
        hapd = ap1.get_instance()
        sta = sta1.get_instance()
        dst = sta.own_addr()
        src = apdev[0]['bssid']

        # Create session
        initiator = ap1
        responder = sta1
        new_iface = ap2.ifname()
        new_peer_addr = ap2.get_actual_peer_addr()
        resp_newif = sta2.ifname()
        peeraddr = None
        initiator.add_peer(responder, peeraddr, new_peer_addr)
        sid = initiator.add_session()
        initiator.configure_session(sid, new_iface)
        with alloc_fail(hapd, 1, "fst_session_send_action"):
            res = initiator.grequest("FST-MANAGER SESSION_INITIATE " + sid)
            if not res.startswith("FAIL"):
                raise Exception("Unexpected SESSION_INITIATE result")

        res = initiator.grequest("FST-MANAGER SESSION_INITIATE " + sid)
        if not res.startswith("OK"):
            raise Exception("SESSION_INITIATE failed")

        tests = ["", "foo", sid, sid + " foo", sid + " foo=bar"]
        for t in tests:
            res = initiator.grequest("FST-MANAGER SESSION_SET " + t)
            if not res.startswith("FAIL"):
                raise Exception("Invalid SESSION_SET accepted")

        with alloc_fail(hapd, 1, "fst_session_send_action"):
            res = initiator.grequest("FST-MANAGER SESSION_TEARDOWN " + sid)
            if not res.startswith("FAIL"):
                raise Exception("Unexpected SESSION_TEARDOWN result")
    finally:
        fst_module_aux.disconnect_two_ap_sta_pairs(ap1, ap2, sta1, sta2)
        fst_module_aux.stop_two_ap_sta_pairs(ap1, ap2, sta1, sta2)

def test_fst_session_oom(dev, apdev, test_params):
    """FST session create OOM"""
    ap1, ap2, sta1, sta2 = fst_module_aux.start_two_ap_sta_pairs(apdev)
    try:
        fst_module_aux.connect_two_ap_sta_pairs(ap1, ap2, sta1, sta2)
        hapd = ap1.get_instance()
        sta = sta1.get_instance()
        dst = sta.own_addr()
        src = apdev[0]['bssid']

        # Create session
        initiator = ap1
        responder = sta1
        new_iface = ap2.ifname()
        new_peer_addr = ap2.get_actual_peer_addr()
        resp_newif = sta2.ifname()
        peeraddr = None
        initiator.add_peer(responder, peeraddr, new_peer_addr)
        with alloc_fail(hapd, 1, "fst_session_create"):
            sid = initiator.grequest("FST-MANAGER SESSION_ADD " + initiator.fst_group)
            if not sid.startswith("FAIL"):
                raise Exception("Unexpected SESSION_ADD success")
        sid = initiator.add_session()
        initiator.configure_session(sid, new_iface)
        with alloc_fail(sta, 1, "fst_session_create"):
            res = initiator.grequest("FST-MANAGER SESSION_INITIATE " + sid)
            if not res.startswith("OK"):
                raise Exception("Unexpected SESSION_INITIATE result")
    finally:
        fst_module_aux.disconnect_two_ap_sta_pairs(ap1, ap2, sta1, sta2)
        fst_module_aux.stop_two_ap_sta_pairs(ap1, ap2, sta1, sta2)

def test_fst_attach_zero_llt(dev, apdev):
    """FST attach with llt=0"""
    sta1 = fst_module_aux.FstSTA('wlan5', fst_test_common.fst_test_def_group,
                                 "100", "0")
    sta1.start()
    sta1.stop()

def test_fst_session_respond_fail(dev, apdev, test_params):
    """FST-MANAGER SESSION_RESPOND failure"""
    ap1, ap2, sta1, sta2 = fst_module_aux.start_two_ap_sta_pairs(apdev)
    try:
        fst_module_aux.connect_two_ap_sta_pairs(ap1, ap2, sta1, sta2)
        sta1.add_peer(ap1, None, sta2.get_actual_peer_addr())
        sid = sta1.add_session()
        sta1.configure_session(sid, sta2.ifname())
        sta1.send_session_setup_request(sid)
        sta1.wait_for_session_event(5, [], ["EVENT_FST_SESSION_STATE"])
        ev = ap1.wait_for_session_event(5, [], ['EVENT_FST_SETUP'])
        if 'id' not in ev:
            raise Exception("No session id in FST setup event")
        # Disconnect STA to make SESSION_RESPOND fail due to no peer found
        sta = sta1.get_instance()
        sta.request("DISCONNECT")
        sta.wait_disconnected()
        req = "FST-MANAGER SESSION_RESPOND %s reject" % ev['id']
        s = ap1.grequest(req)
        if not s.startswith("FAIL"):
            raise Exception("SESSION_RESPOND succeeded unexpectedly")
    finally:
        fst_module_aux.disconnect_two_ap_sta_pairs(ap1, ap2, sta1, sta2)
        fst_module_aux.stop_two_ap_sta_pairs(ap1, ap2, sta1, sta2)

def fst_session_set(dev, sid, param, value):
    cmd = "FST-MANAGER SESSION_SET %s %s=%s" % (sid, param, value)
    if "OK" not in dev.global_request(cmd):
        raise Exception(cmd + " failed")

def fst_session_set_ap(dev, sid, param, value):
    cmd = "FST-MANAGER SESSION_SET %s %s=%s" % (sid, param, value)
    if "OK" not in dev.request(cmd):
        raise Exception(cmd + " failed")

def fst_attach_ap(dev, ifname, group):
    cmd = "FST-ATTACH %s %s" % (ifname, group)
    if "OK" not in dev.request(cmd):
        raise Exception("FST-ATTACH (AP) failed")
    ev = dev.wait_event(['FST-EVENT-IFACE'], timeout=5)
    if ev is None:
        raise Exception("No FST-EVENT-IFACE attached (AP)")
    for t in ["attached", "ifname=" + ifname, "group=" + group]:
        if t not in ev:
            raise Exception("Unexpected FST-EVENT-IFACE data (AP): " + ev)

def fst_attach_sta(dev, ifname, group):
    if "OK" not in dev.global_request("FST-ATTACH %s %s" % (ifname, group)):
        raise Exception("FST-ATTACH (STA) failed")
    ev = dev.wait_global_event(['FST-EVENT-IFACE'], timeout=5)
    if ev is None:
        raise Exception("No FST-EVENT-IFACE attached (STA)")
    for t in ["attached", "ifname=" + ifname, "group=" + group]:
        if t not in ev:
            raise Exception("Unexpected FST-EVENT-IFACE data (STA): " + ev)

def fst_detach_ap(dev, ifname, group):
    if "OK" not in dev.request("FST-DETACH " + ifname):
        raise Exception("FST-DETACH (AP) failed for " + ifname)
    ev = dev.wait_event(['FST-EVENT-IFACE'], timeout=5)
    if ev is None:
        raise Exception("No FST-EVENT-IFACE detached (AP) for " + ifname)
    for t in ["detached", "ifname=" + ifname, "group=" + group]:
        if t not in ev:
            raise Exception("Unexpected FST-EVENT-IFACE data (AP): " + ev)

def fst_detach_sta(dev, ifname, group):
    dev.dump_monitor()
    if "OK" not in dev.global_request("FST-DETACH " + ifname):
        raise Exception("FST-DETACH (STA) failed for " + ifname)
    ev = dev.wait_global_event(['FST-EVENT-IFACE'], timeout=5)
    if ev is None:
        raise Exception("No FST-EVENT-IFACE detached (STA) for " + ifname)
    for t in ["detached", "ifname=" + ifname, "group=" + group]:
        if t not in ev:
            raise Exception("Unexpected FST-EVENT-IFACE data (STA): " + ev)

def fst_wait_event_peer_ap(dev, event, ifname, addr):
    ev = dev.wait_event(['FST-EVENT-PEER'], timeout=5)
    if ev is None:
        raise Exception("No FST-EVENT-PEER connected (AP)")
    for t in [" " + event + " ", "ifname=" + ifname, "peer_addr=" + addr]:
        if t not in ev:
            raise Exception("Unexpected FST-EVENT-PEER data (AP): " + ev)

def fst_wait_event_peer_sta(dev, event, ifname, addr):
    ev = dev.wait_global_event(['FST-EVENT-PEER'], timeout=5)
    if ev is None:
        raise Exception("No FST-EVENT-PEER connected (STA)")
    for t in [" " + event + " ", "ifname=" + ifname, "peer_addr=" + addr]:
        if t not in ev:
            raise Exception("Unexpected FST-EVENT-PEER data (STA): " + ev)

def fst_setup_req(dev, hglobal, freq, dst, req, stie, mbie="", no_wait=False):
    act = req + stie + mbie
    dev.request("MGMT_TX %s %s freq=%d action=%s" % (dst, dst, freq, act))
    ev = dev.wait_event(['MGMT-TX-STATUS'], timeout=5)
    if ev is None or "result=SUCCESS" not in ev:
        raise Exception("FST Action frame not ACKed")

    if no_wait:
        return
    while True:
        ev = hglobal.wait_event(['FST-EVENT-SESSION'], timeout=5)
        if ev is None:
            raise Exception("No FST-EVENT-SESSION (AP)")
        if "new_state=SETUP_COMPLETION" in ev:
            break

def fst_start_and_connect(apdev, group, sgroup):
    hglobal = hostapd.HostapdGlobal()
    if "OK" not in hglobal.request("FST-MANAGER TEST_REQUEST IS_SUPPORTED"):
        raise HwsimSkip("No FST testing support")

    params = {"ssid": "fst_11a", "hw_mode": "a", "channel": "36",
              "country_code": "US"}
    hapd = hostapd.add_ap(apdev[0], params)

    fst_attach_ap(hglobal, apdev[0]['ifname'], group)

    cmd = "FST-ATTACH %s %s" % (apdev[0]['ifname'], group)
    if "FAIL" not in hglobal.request(cmd):
        raise Exception("Duplicated FST-ATTACH (AP) accepted")

    params = {"ssid": "fst_11g", "hw_mode": "g", "channel": "1",
              "country_code": "US"}
    hapd2 = hostapd.add_ap(apdev[1], params)
    fst_attach_ap(hglobal, apdev[1]['ifname'], group)

    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    wpas.interface_add("wlan5")
    fst_attach_sta(wpas, wpas.ifname, sgroup)

    wpas.interface_add("wlan6", set_ifname=False)
    wpas2 = WpaSupplicant(ifname="wlan6")
    fst_attach_sta(wpas, wpas2.ifname, sgroup)

    wpas.connect("fst_11a", key_mgmt="NONE", scan_freq="5180",
                 wait_connect=False)
    wpas.wait_connected()

    fst_wait_event_peer_sta(wpas, "connected", wpas.ifname, apdev[0]['bssid'])
    fst_wait_event_peer_ap(hglobal, "connected", apdev[0]['ifname'],
                           wpas.own_addr())

    wpas2.connect("fst_11g", key_mgmt="NONE", scan_freq="2412",
                  wait_connect=False)
    wpas2.wait_connected()

    fst_wait_event_peer_sta(wpas, "connected", wpas2.ifname, apdev[1]['bssid'])
    fst_wait_event_peer_ap(hglobal, "connected", apdev[1]['ifname'],
                           wpas2.own_addr())
    return hglobal, wpas, wpas2, hapd, hapd2

def test_fst_test_setup(dev, apdev, test_params):
    """FST setup using separate commands"""
    try:
        _test_fst_test_setup(dev, apdev, test_params)
    finally:
        subprocess.call(['iw', 'reg', 'set', '00'])
        dev[0].flush_scan_cache()
        dev[1].flush_scan_cache()

def _test_fst_test_setup(dev, apdev, test_params):
    group = "fstg0b"
    sgroup = "fstg1b"
    hglobal, wpas, wpas2, hapd, hapd2 = fst_start_and_connect(apdev, group, sgroup)

    sid = wpas.global_request("FST-MANAGER SESSION_ADD " + sgroup).strip()
    if "FAIL" in sid:
        raise Exception("FST-MANAGER SESSION_ADD (STA) failed")

    fst_session_set(wpas, sid, "old_ifname", wpas.ifname)
    fst_session_set(wpas, sid, "old_peer_addr", apdev[0]['bssid'])
    fst_session_set(wpas, sid, "new_ifname", wpas2.ifname)
    fst_session_set(wpas, sid, "new_peer_addr", apdev[1]['bssid'])

    if "OK" not in wpas.global_request("FST-MANAGER SESSION_INITIATE " + sid):
        raise Exception("FST-MANAGER SESSION_INITIATE failed")

    while True:
        ev = hglobal.wait_event(['FST-EVENT-SESSION'], timeout=5)
        if ev is None:
            raise Exception("No FST-EVENT-SESSION (AP)")
        if "new_state=SETUP_COMPLETION" in ev:
            f = re.search("session_id=(\d+)", ev)
            if f is None:
                raise Exception("No session_id in FST-EVENT-SESSION")
            sid_ap = f.group(1)
            cmd = "FST-MANAGER SESSION_RESPOND %s accept" % sid_ap
            if "OK" not in hglobal.request(cmd):
                raise Exception("FST-MANAGER SESSION_RESPOND failed on AP")
            break

    ev = wpas.wait_global_event(["FST-EVENT-SESSION"], timeout=5)
    if ev is None:
        raise Exception("No FST-EVENT-SESSION")
    if "new_state=SETUP_COMPLETION" not in ev:
        raise Exception("Unexpected FST-EVENT-SESSION data: " + ev)

    ev = wpas.wait_global_event(["FST-EVENT-SESSION"], timeout=5)
    if ev is None:
        raise Exception("No FST-EVENT-SESSION")
    if "event_type=EVENT_FST_ESTABLISHED" not in ev:
        raise Exception("Unexpected FST-EVENT-SESSION data: " + ev)

    cmd = "FST-MANAGER SESSION_REMOVE " + sid
    if "OK" not in wpas.global_request(cmd):
        raise Exception("FST-MANAGER SESSION_REMOVE failed")
    ev = wpas.wait_global_event(["FST-EVENT-SESSION"], timeout=5)
    if ev is None:
        raise Exception("No FST-EVENT-SESSION")
    if "new_state=INITIAL" not in ev:
        raise Exception("Unexpected FST-EVENT-SESSION data (STA): " + ev)

    ev = hglobal.wait_event(['FST-EVENT-SESSION'], timeout=5)
    if ev is None:
        raise Exception("No FST-EVENT-SESSION (AP)")
    if "new_state=INITIAL" not in ev:
        raise Exception("Unexpected FST-EVENT-SESSION data (AP): " + ev)

    if "FAIL" not in wpas.global_request(cmd):
        raise Exception("Duplicated FST-MANAGER SESSION_REMOVE accepted")

    hglobal.request("FST-MANAGER SESSION_REMOVE " + sid_ap)

    wpas.request("DISCONNECT")
    wpas.wait_disconnected()
    fst_wait_event_peer_sta(wpas, "disconnected", wpas.ifname,
                            apdev[0]['bssid'])
    fst_wait_event_peer_ap(hglobal, "disconnected", apdev[0]['ifname'],
                           wpas.own_addr())

    wpas2.request("DISCONNECT")
    wpas2.wait_disconnected()
    fst_wait_event_peer_sta(wpas, "disconnected", wpas2.ifname,
                            apdev[1]['bssid'])
    fst_wait_event_peer_ap(hglobal, "disconnected", apdev[1]['ifname'],
                           wpas2.own_addr())

    fst_detach_ap(hglobal, apdev[0]['ifname'], group)
    if "FAIL" not in hglobal.request("FST-DETACH " + apdev[0]['ifname']):
        raise Exception("Duplicated FST-DETACH (AP) accepted")
    hapd.disable()

    fst_detach_ap(hglobal, apdev[1]['ifname'], group)
    hapd2.disable()

    fst_detach_sta(wpas, wpas.ifname, sgroup)
    fst_detach_sta(wpas, wpas2.ifname, sgroup)

def test_fst_setup_mbie_diff(dev, apdev, test_params):
    """FST setup and different MBIE in FST Setup Request"""
    try:
        _test_fst_setup_mbie_diff(dev, apdev, test_params)
    finally:
        subprocess.call(['iw', 'reg', 'set', '00'])
        dev[0].flush_scan_cache()
        dev[1].flush_scan_cache()

def _test_fst_setup_mbie_diff(dev, apdev, test_params):
    group = "fstg0c"
    sgroup = "fstg1c"
    hglobal, wpas, wpas2, hapd, hapd2 = fst_start_and_connect(apdev, group, sgroup)

    # FST Setup Request: Category, FST Action, Dialog Token (non-zero),
    # LLT (32 bits, see 10.32), Session Transition (see 8.4.2.147),
    # Multi-band element (optional, see 8.4.2.140)

    # Session Transition: EID, Len, FSTS ID(4), Session Control,
    # New Band (Band ID, Setup, Operation), Old Band (Band ID, Setup, Operation)

    # Multi-band element: EID, Len, Multi-band Control, Band ID,
    # Operating Class, Channel Number, BSSID (6), Beacon Interval (2),
    # TSF Offset (8), Multi-band Connection Capability, FSTSessionTimeOut,
    # STA MAC Address (6, optional), Pairwise Cipher Suite Count (2, optional),
    # Pairwise Cipher Suite List (4xm, optional)

    # MBIE with the non-matching STA MAC Address:
    req = "1200011a060000"
    stie = "a40b0100000000020001040001"
    mbie = "9e1c0c0200010200000004000000000000000000000000ff0200000006ff"
    fst_setup_req(wpas, hglobal, 5180, apdev[0]['bssid'], req, stie, mbie)

    # MBIE without the STA MAC Address:
    req = "1200011a060000"
    stie = "a40b0100000000020001040001"
    mbie = "9e16040200010200000004000000000000000000000000ff"
    fst_setup_req(wpas, hglobal, 5180, apdev[0]['bssid'], req, stie, mbie)

    # MBIE with unsupported STA Role:
    req = "1200011a060000"
    stie = "a40b0100000000020001040001"
    mbie = "9e16070200010200000004000000000000000000000000ff"
    fst_setup_req(wpas, hglobal, 5180, apdev[0]['bssid'], req, stie, mbie)

    # MBIE with unsupported Band ID:
    req = "1200011a060000"
    stie = "a40b0100000000020001040001"
    mbie = "9e1604ff00010200000004000000000000000000000000ff"
    fst_setup_req(wpas, hglobal, 5180, apdev[0]['bssid'], req, stie, mbie)

    # FST Setup Request without MBIE (different FSTS ID):
    req = "1200011a060000"
    stie = "a40b0200000000020001040001"
    fst_setup_req(wpas, hglobal, 5180, apdev[0]['bssid'], req, stie)

    # MBIE update OOM on AP
    req = "1200011a060000"
    stie = "a40b0100000000020001040001"
    mbie = "9e16040200010200000004000000000000000000000000ff"
    try:
        with alloc_fail(hapd, 1, "mb_ies_by_info"):
            fst_setup_req(wpas, hglobal, 5180, apdev[0]['bssid'], req, stie,
                          mbie, no_wait=True)
    except HwsimSkip as e:
        # Skip exception to allow proper cleanup
        pass

    # Remove sessions to avoid causing issues to following test ases
    s = hglobal.request("FST-MANAGER LIST_SESSIONS " + group)
    if not s.startswith("FAIL"):
        for sid in s.split(' '):
            if len(sid):
                hglobal.request("FST-MANAGER SESSION_REMOVE " + sid)

def test_fst_many_setup(dev, apdev, test_params):
    """FST setup multiple times"""
    try:
        _test_fst_many_setup(dev, apdev, test_params)
    finally:
        subprocess.call(['iw', 'reg', 'set', '00'])
        dev[0].flush_scan_cache()
        dev[1].flush_scan_cache()

def _test_fst_many_setup(dev, apdev, test_params):
    group = "fstg0d"
    sgroup = "fstg1d"
    hglobal, wpas, wpas2, hapd, hapd2 = fst_start_and_connect(apdev, group, sgroup)

    sid = wpas.global_request("FST-MANAGER SESSION_ADD " + sgroup).strip()
    if "FAIL" in sid:
        raise Exception("FST-MANAGER SESSION_ADD (STA) failed")

    fst_session_set(wpas, sid, "old_ifname", wpas.ifname)
    fst_session_set(wpas, sid, "old_peer_addr", apdev[0]['bssid'])
    fst_session_set(wpas, sid, "new_ifname", wpas2.ifname)
    fst_session_set(wpas, sid, "new_peer_addr", apdev[1]['bssid'])

    for i in range(257):
        if "OK" not in wpas.global_request("FST-MANAGER SESSION_INITIATE " + sid):
            raise Exception("FST-MANAGER SESSION_INITIATE failed")

        while True:
            ev = hglobal.wait_event(['FST-EVENT-SESSION'], timeout=5)
            if ev is None:
                raise Exception("No FST-EVENT-SESSION (AP)")
            if "new_state=SETUP_COMPLETION" in ev:
                f = re.search("session_id=(\d+)", ev)
                if f is None:
                    raise Exception("No session_id in FST-EVENT-SESSION")
                sid_ap = f.group(1)
                cmd = "FST-MANAGER SESSION_RESPOND %s accept" % sid_ap
                if "OK" not in hglobal.request(cmd):
                    raise Exception("FST-MANAGER SESSION_RESPOND failed on AP")
                break

        ev = wpas.wait_global_event(["FST-EVENT-SESSION"], timeout=5)
        if ev is None:
            raise Exception("No FST-EVENT-SESSION (STA)")
        if "new_state=SETUP_COMPLETION" not in ev:
            raise Exception("Unexpected FST-EVENT-SESSION data: " + ev)

        ev = wpas.wait_global_event(["FST-EVENT-SESSION"], timeout=5)
        if ev is None:
            raise Exception("No FST-EVENT-SESSION (STA)")
        if "event_type=EVENT_FST_ESTABLISHED" not in ev:
            raise Exception("Unexpected FST-EVENT-SESSION data: " + ev)

        if "OK" not in wpas.global_request("FST-MANAGER SESSION_TEARDOWN " + sid):
            raise Exception("FST-MANAGER SESSION_INITIATE failed")

        if i == 0:
            if "FAIL" not in wpas.global_request("FST-MANAGER SESSION_TEARDOWN " + sid):
                raise Exception("Duplicate FST-MANAGER SESSION_TEARDOWN accepted")

        ev = wpas.wait_global_event(["FST-EVENT-SESSION"], timeout=5)
        if ev is None:
            raise Exception("No FST-EVENT-SESSION (STA teardown -->initial)")
        if "new_state=INITIAL" not in ev:
            raise Exception("Unexpected FST-EVENT-SESSION data (STA): " + ev)

        ev = hglobal.wait_event(['FST-EVENT-SESSION'], timeout=5)
        if ev is None:
            raise Exception("No FST-EVENT-SESSION (AP teardown -->initial)")
        if "new_state=INITIAL" not in ev:
            raise Exception("Unexpected FST-EVENT-SESSION data (AP): " + ev)

        if "OK" not in hglobal.request("FST-MANAGER SESSION_REMOVE " + sid_ap):
            raise Exception("FST-MANAGER SESSION_REMOVE (AP) failed")

    if "OK" not in wpas.global_request("FST-MANAGER SESSION_REMOVE " + sid):
        raise Exception("FST-MANAGER SESSION_REMOVE failed")

    wpas.request("DISCONNECT")
    wpas.wait_disconnected()
    fst_wait_event_peer_sta(wpas, "disconnected", wpas.ifname,
                            apdev[0]['bssid'])
    fst_wait_event_peer_ap(hglobal, "disconnected", apdev[0]['ifname'],
                           wpas.own_addr())

    wpas2.request("DISCONNECT")
    wpas2.wait_disconnected()
    fst_wait_event_peer_sta(wpas, "disconnected", wpas2.ifname,
                            apdev[1]['bssid'])
    fst_wait_event_peer_ap(hglobal, "disconnected", apdev[1]['ifname'],
                           wpas2.own_addr())

    fst_detach_ap(hglobal, apdev[0]['ifname'], group)
    fst_detach_ap(hglobal, apdev[1]['ifname'], group)
    hapd.disable()
    hapd2.disable()

    fst_detach_sta(wpas, wpas.ifname, sgroup)
    fst_detach_sta(wpas, wpas2.ifname, sgroup)

def test_fst_attach_wpas_error(dev, apdev, test_params):
    """FST attach errors in wpa_supplicant"""
    if "OK" not in dev[0].global_request("FST-MANAGER TEST_REQUEST IS_SUPPORTED"):
        raise HwsimSkip("No FST testing support")
    group = "fstg0"
    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    wpas.interface_add("wlan5")
    fst_attach_sta(wpas, wpas.ifname, group)
    if "FAIL" not in wpas.global_request("FST-ATTACH %s %s" % (wpas.ifname,
                                                               group)):
        raise Exception("Duplicated FST-ATTACH accepted")
    if "FAIL" not in wpas.global_request("FST-ATTACH %s %s" % ("foofoo",
                                                               group)):
        raise Exception("FST-ATTACH for unknown interface accepted")

def test_fst_session_initiate_errors(dev, apdev, test_params):
    """FST SESSION_INITIATE error cases"""
    try:
        _test_fst_session_initiate_errors(dev, apdev, test_params)
    finally:
        subprocess.call(['iw', 'reg', 'set', '00'])
        dev[0].flush_scan_cache()
        dev[1].flush_scan_cache()

def _test_fst_session_initiate_errors(dev, apdev, test_params):
    group = "fstg0"
    sgroup = "fstg1"
    hglobal, wpas, wpas2, hapd, hapd2 = fst_start_and_connect(apdev, group, sgroup)

    sid = wpas.global_request("FST-MANAGER SESSION_ADD " + sgroup).strip()
    if "FAIL" in sid:
        raise Exception("FST-MANAGER SESSION_ADD (STA) failed")

    # No old peer MAC address
    if "FAIL" not in wpas.global_request("FST-MANAGER SESSION_INITIATE " + sid):
        raise Exception("Invalid FST-MANAGER SESSION_INITIATE accepted")

    fst_session_set(wpas, sid, "old_peer_addr", "00:ff:ff:ff:ff:ff")
    # No new peer MAC address
    if "FAIL" not in wpas.global_request("FST-MANAGER SESSION_INITIATE " + sid):
        raise Exception("Invalid FST-MANAGER SESSION_INITIATE accepted")

    fst_session_set(wpas, sid, "new_peer_addr", "00:ff:ff:ff:ff:fe")
    # No old interface defined
    if "FAIL" not in wpas.global_request("FST-MANAGER SESSION_INITIATE " + sid):
        raise Exception("Invalid FST-MANAGER SESSION_INITIATE accepted")

    fst_session_set(wpas, sid, "old_ifname", wpas.ifname)
    # No new interface defined
    if "FAIL" not in wpas.global_request("FST-MANAGER SESSION_INITIATE " + sid):
        raise Exception("Invalid FST-MANAGER SESSION_INITIATE accepted")

    fst_session_set(wpas, sid, "new_ifname", wpas.ifname)
    # Same interface set as old and new
    if "FAIL" not in wpas.global_request("FST-MANAGER SESSION_INITIATE " + sid):
        raise Exception("Invalid FST-MANAGER SESSION_INITIATE accepted")

    fst_session_set(wpas, sid, "new_ifname", wpas2.ifname)
    # The preset old peer address is not connected
    if "FAIL" not in wpas.global_request("FST-MANAGER SESSION_INITIATE " + sid):
        raise Exception("Invalid FST-MANAGER SESSION_INITIATE accepted")

    fst_session_set(wpas, sid, "old_peer_addr", apdev[0]['bssid'])
    # The preset new peer address is not connected
    if "FAIL" not in wpas.global_request("FST-MANAGER SESSION_INITIATE " + sid):
        raise Exception("Invalid FST-MANAGER SESSION_INITIATE accepted")

    fst_session_set(wpas, sid, "new_peer_addr", apdev[1]['bssid'])
    # Initiate session setup
    if "OK" not in wpas.global_request("FST-MANAGER SESSION_INITIATE " + sid):
        raise Exception("FST-MANAGER SESSION_INITIATE failed")

    # Session in progress
    if "FAIL" not in wpas.global_request("FST-MANAGER SESSION_INITIATE " + sid):
        raise Exception("Duplicated FST-MANAGER SESSION_INITIATE accepted")

    sid2 = wpas.global_request("FST-MANAGER SESSION_ADD " + sgroup).strip()
    if "FAIL" in sid:
        raise Exception("FST-MANAGER SESSION_ADD (STA) failed")
    fst_session_set(wpas, sid2, "old_ifname", wpas.ifname)
    fst_session_set(wpas, sid2, "old_peer_addr", apdev[0]['bssid'])
    fst_session_set(wpas, sid2, "new_ifname", wpas2.ifname)
    fst_session_set(wpas, sid2, "new_peer_addr", apdev[1]['bssid'])

    # There is another session in progress (old)
    if "FAIL" not in wpas.global_request("FST-MANAGER SESSION_INITIATE " + sid2):
        raise Exception("Duplicated FST-MANAGER SESSION_INITIATE accepted")

    if "OK" not in wpas.global_request("FST-MANAGER SESSION_REMOVE " + sid):
        raise Exception("FST-MANAGER SESSION_REMOVE failed")

    while True:
        ev = hglobal.wait_event(['FST-EVENT-SESSION'], timeout=5)
        if ev is None:
            raise Exception("No FST-EVENT-SESSION (AP)")
        if "new_state=SETUP_COMPLETION" in ev:
            f = re.search("session_id=(\d+)", ev)
            if f is None:
                raise Exception("No session_id in FST-EVENT-SESSION")
            sid_ap = f.group(1)
            break
    if "OK" not in hglobal.request("FST-MANAGER SESSION_REMOVE " + sid_ap):
        raise Exception("FST-MANAGER SESSION_REMOVE (AP) failed")

    if "OK" not in wpas.global_request("FST-MANAGER SESSION_REMOVE " + sid2):
        raise Exception("FST-MANAGER SESSION_REMOVE failed")

def test_fst_session_respond_errors(dev, apdev, test_params):
    """FST SESSION_RESPOND error cases"""
    try:
        _test_fst_session_respond_errors(dev, apdev, test_params)
    finally:
        subprocess.call(['iw', 'reg', 'set', '00'])
        dev[0].flush_scan_cache()
        dev[1].flush_scan_cache()

def _test_fst_session_respond_errors(dev, apdev, test_params):
    group = "fstg0b"
    sgroup = "fstg1b"
    hglobal, wpas, wpas2, hapd, hapd2 = fst_start_and_connect(apdev, group, sgroup)

    sid = wpas.global_request("FST-MANAGER SESSION_ADD " + sgroup).strip()
    if "FAIL" in sid:
        raise Exception("FST-MANAGER SESSION_ADD (STA) failed")

    fst_session_set(wpas, sid, "old_ifname", wpas.ifname)
    fst_session_set(wpas, sid, "old_peer_addr", apdev[0]['bssid'])
    fst_session_set(wpas, sid, "new_ifname", wpas2.ifname)
    fst_session_set(wpas, sid, "new_peer_addr", apdev[1]['bssid'])

    if "OK" not in wpas.global_request("FST-MANAGER SESSION_INITIATE " + sid):
        raise Exception("FST-MANAGER SESSION_INITIATE failed")

    while True:
        ev = hglobal.wait_event(['FST-EVENT-SESSION'], timeout=5)
        if ev is None:
            raise Exception("No FST-EVENT-SESSION (AP)")
        if "new_state=SETUP_COMPLETION" in ev:
            f = re.search("session_id=(\d+)", ev)
            if f is None:
                raise Exception("No session_id in FST-EVENT-SESSION")
            sid_ap = f.group(1)
            break

    # The preset peer address is not in the peer list
    fst_session_set_ap(hglobal, sid_ap, "old_peer_addr", "00:00:00:00:00:01")
    cmd = "FST-MANAGER SESSION_RESPOND %s accept" % sid_ap
    if "FAIL" not in hglobal.request(cmd):
        raise Exception("Invalid FST-MANAGER SESSION_RESPOND accepted")

    # Same interface set as old and new
    fst_session_set_ap(hglobal, sid_ap, "old_peer_addr", wpas.own_addr())
    fst_session_set_ap(hglobal, sid_ap, "old_ifname", apdev[1]['ifname'])
    cmd = "FST-MANAGER SESSION_RESPOND %s accept" % sid_ap
    if "FAIL" not in hglobal.request(cmd):
        raise Exception("Invalid FST-MANAGER SESSION_RESPOND accepted")

    # valid command
    fst_session_set_ap(hglobal, sid_ap, "old_ifname", apdev[0]['ifname'])
    cmd = "FST-MANAGER SESSION_RESPOND %s accept" % sid_ap
    if "OK" not in hglobal.request(cmd):
        raise Exception("FST-MANAGER SESSION_RESPOND failed")

    # incorrect state
    cmd = "FST-MANAGER SESSION_RESPOND %s accept" % sid_ap
    if "FAIL" not in hglobal.request(cmd):
        raise Exception("Invalid FST-MANAGER SESSION_RESPOND accepted")

    cmd = "FST-MANAGER SESSION_REMOVE " + sid
    if "OK" not in wpas.global_request(cmd):
        raise Exception("FST-MANAGER SESSION_REMOVE (STA) failed")

    cmd = "FST-MANAGER SESSION_REMOVE %s" % sid_ap
    if "OK" not in hglobal.request(cmd):
        raise Exception("FST-MANAGER SESSION_REMOVE (AP) failed")
