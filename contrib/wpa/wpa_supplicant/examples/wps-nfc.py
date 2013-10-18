#!/usr/bin/python
#
# Example nfcpy to wpa_supplicant wrapper for WPS NFC operations
# Copyright (c) 2012, Jouni Malinen <j@w1.fi>
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

import os
import sys
import time

import nfc
import nfc.ndef
import nfc.llcp
import nfc.handover

import wpactrl

wpas_ctrl = '/var/run/wpa_supplicant'

def wpas_connect():
    ifaces = []
    if os.path.isdir(wpas_ctrl):
        try:
            ifaces = [os.path.join(wpas_ctrl, i) for i in os.listdir(wpas_ctrl)]
        except OSError, error:
            print "Could not find wpa_supplicant: ", error
            return None

    if len(ifaces) < 1:
        print "No wpa_supplicant control interface found"
        return None

    for ctrl in ifaces:
        try:
            wpas = wpactrl.WPACtrl(ctrl)
            return wpas
        except wpactrl.error, error:
            print "Error: ", error
            pass
    return None


def wpas_tag_read(message):
    wpas = wpas_connect()
    if (wpas == None):
        return
    print wpas.request("WPS_NFC_TAG_READ " + message.encode("hex"))


def wpas_get_handover_req():
    wpas = wpas_connect()
    if (wpas == None):
        return None
    return wpas.request("NFC_GET_HANDOVER_REQ NDEF WPS").rstrip().decode("hex")


def wpas_put_handover_sel(message):
    wpas = wpas_connect()
    if (wpas == None):
        return
    print wpas.request("NFC_RX_HANDOVER_SEL " + str(message).encode("hex"))


def wps_handover_init(peer):
    print "Trying to initiate WPS handover"

    data = wpas_get_handover_req()
    if (data == None):
        print "Could not get handover request message from wpa_supplicant"
        return
    print "Handover request from wpa_supplicant: " + data.encode("hex")
    message = nfc.ndef.Message(data)
    print "Parsed handover request: " + message.pretty()

    nfc.llcp.activate(peer);
    time.sleep(0.5)

    client = nfc.handover.HandoverClient()
    try:
        print "Trying handover";
        client.connect()
        print "Connected for handover"
    except nfc.llcp.ConnectRefused:
        print "Handover connection refused"
        nfc.llcp.shutdown()
        client.close()
        return

    print "Sending handover request"

    if not client.send(message):
        print "Failed to send handover request"

    print "Receiving handover response"
    message = client._recv()
    print "Handover select received"
    print message.pretty()
    wpas_put_handover_sel(message)

    print "Remove peer"
    nfc.llcp.shutdown()
    client.close()
    print "Done with handover"


def wps_tag_read(tag):
    if len(tag.ndef.message):
        message = nfc.ndef.Message(tag.ndef.message)
        print "message type " + message.type

        for record in message:
            print "record type " + record.type
            if record.type == "application/vnd.wfa.wsc":
                print "WPS tag - send to wpa_supplicant"
                wpas_tag_read(tag.ndef.message)
                break
    else:
        print "Empty tag"

    print "Remove tag"
    while tag.is_present:
        time.sleep(0.1)


def main():
    clf = nfc.ContactlessFrontend()

    try:
        while True:
            print "Waiting for a tag or peer to be touched"

            while True:
                general_bytes = nfc.llcp.startup({})
                tag = clf.poll(general_bytes)
                if tag == None:
                    continue

                if isinstance(tag, nfc.DEP):
                    wps_handover_init(tag)
                    break

                if tag.ndef:
                    wps_tag_read(tag)
                    break

                if tag:
                    print "Not an NDEF tag - remove tag"
                    while tag.is_present:
                        time.sleep(0.1)
                    break

    except KeyboardInterrupt:
        raise SystemExit
    finally:
        clf.close()

    raise SystemExit

if __name__ == '__main__':
    main()
