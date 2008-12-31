/*-
 * Copyright (c) 2001 Cubical Solutions Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* capi/capi_msgs.h	The CAPI i4b message and handler declarations.
 *
 * $FreeBSD: src/sys/i4b/capi/capi_msgs.h,v 1.2.18.1 2008/11/25 02:59:29 kensmith Exp $
 */

#ifndef _I4B_CAPI_MSGS_H_
#define _I4B_CAPI_MSGS_H_

/* CAPI commands */

#define CAPI_ALERT                   0x01
#define CAPI_CONNECT                 0x02
#define CAPI_CONNECT_ACTIVE          0x03
#define CAPI_CONNECT_B3              0x82
#define CAPI_CONNECT_B3_ACTIVE       0x83
#define CAPI_CONNECT_B3_T90_ACTIVE   0x88
#define CAPI_DATA_B3                 0x86
#define CAPI_DISCONNECT_B3           0x84
#define CAPI_DISCONNECT              0x04
#define CAPI_FACILITY                0x80
#define CAPI_INFO                    0x08
#define CAPI_LISTEN                  0x05
#define CAPI_MANUFACTURER            0xff
#define CAPI_RESET_B3                0x87
#define CAPI_SELECT_B_PROTOCOL       0x41

/* CAPI subcommands */

#define CAPI_REQ                     0x80
#define CAPI_CONF                    0x81
#define CAPI_IND                     0x82
#define CAPI_RESP                    0x83

/* CAPI combined commands */

#define CAPICMD(cmd,subcmd)          (((subcmd)<<8)|(cmd))

#define CAPI_DISCONNECT_REQ          CAPICMD(CAPI_DISCONNECT,CAPI_REQ)
#define CAPI_DISCONNECT_CONF         CAPICMD(CAPI_DISCONNECT,CAPI_CONF)
#define CAPI_DISCONNECT_IND          CAPICMD(CAPI_DISCONNECT,CAPI_IND)
#define CAPI_DISCONNECT_RESP         CAPICMD(CAPI_DISCONNECT,CAPI_RESP)

#define CAPI_ALERT_REQ               CAPICMD(CAPI_ALERT,CAPI_REQ)
#define CAPI_ALERT_CONF              CAPICMD(CAPI_ALERT,CAPI_CONF)

#define CAPI_CONNECT_REQ             CAPICMD(CAPI_CONNECT,CAPI_REQ)
#define CAPI_CONNECT_CONF            CAPICMD(CAPI_CONNECT,CAPI_CONF)
#define CAPI_CONNECT_IND             CAPICMD(CAPI_CONNECT,CAPI_IND)
#define CAPI_CONNECT_RESP            CAPICMD(CAPI_CONNECT,CAPI_RESP)

#define CAPI_CONNECT_ACTIVE_REQ      CAPICMD(CAPI_CONNECT_ACTIVE,CAPI_REQ)
#define CAPI_CONNECT_ACTIVE_CONF     CAPICMD(CAPI_CONNECT_ACTIVE,CAPI_CONF)
#define CAPI_CONNECT_ACTIVE_IND      CAPICMD(CAPI_CONNECT_ACTIVE,CAPI_IND)
#define CAPI_CONNECT_ACTIVE_RESP     CAPICMD(CAPI_CONNECT_ACTIVE,CAPI_RESP)

#define CAPI_SELECT_B_PROTOCOL_REQ   CAPICMD(CAPI_SELECT_B_PROTOCOL,CAPI_REQ)
#define CAPI_SELECT_B_PROTOCOL_CONF  CAPICMD(CAPI_SELECT_B_PROTOCOL,CAPI_CONF)

#define CAPI_CONNECT_B3_REQ          CAPICMD(CAPI_CONNECT_B3,CAPI_REQ)
#define CAPI_CONNECT_B3_CONF         CAPICMD(CAPI_CONNECT_B3,CAPI_CONF)
#define CAPI_CONNECT_B3_IND          CAPICMD(CAPI_CONNECT_B3,CAPI_IND)
#define CAPI_CONNECT_B3_RESP         CAPICMD(CAPI_CONNECT_B3,CAPI_RESP)

#define CAPI_CONNECT_B3_ACTIVE_REQ   CAPICMD(CAPI_CONNECT_B3_ACTIVE,CAPI_REQ)
#define CAPI_CONNECT_B3_ACTIVE_CONF  CAPICMD(CAPI_CONNECT_B3_ACTIVE,CAPI_CONF)
#define CAPI_CONNECT_B3_ACTIVE_IND   CAPICMD(CAPI_CONNECT_B3_ACTIVE,CAPI_IND)
#define CAPI_CONNECT_B3_ACTIVE_RESP  CAPICMD(CAPI_CONNECT_B3_ACTIVE,CAPI_RESP)

#define CAPI_CONNECT_B3_T90_ACTIVE_IND CAPICMD(CAPI_CONNECT_B3_T90_ACTIVE,CAPI_IND)
#define CAPI_CONNECT_B3_T90_ACTIVE_RESP CAPICMD(CAPI_CONNECT_B3_T90_ACTIVE,CAPI_RESP)

#define CAPI_DATA_B3_REQ             CAPICMD(CAPI_DATA_B3,CAPI_REQ)
#define CAPI_DATA_B3_CONF            CAPICMD(CAPI_DATA_B3,CAPI_CONF)
#define CAPI_DATA_B3_IND             CAPICMD(CAPI_DATA_B3,CAPI_IND)
#define CAPI_DATA_B3_RESP            CAPICMD(CAPI_DATA_B3,CAPI_RESP)

#define CAPI_DISCONNECT_B3_REQ       CAPICMD(CAPI_DISCONNECT_B3,CAPI_REQ)
#define CAPI_DISCONNECT_B3_CONF      CAPICMD(CAPI_DISCONNECT_B3,CAPI_CONF)
#define CAPI_DISCONNECT_B3_IND       CAPICMD(CAPI_DISCONNECT_B3,CAPI_IND)
#define CAPI_DISCONNECT_B3_RESP      CAPICMD(CAPI_DISCONNECT_B3,CAPI_RESP)

#define CAPI_RESET_B3_REQ            CAPICMD(CAPI_RESET_B3,CAPI_REQ)
#define CAPI_RESET_B3_CONF           CAPICMD(CAPI_RESET_B3,CAPI_CONF)
#define CAPI_RESET_B3_IND            CAPICMD(CAPI_RESET_B3,CAPI_IND)
#define CAPI_RESET_B3_RESP           CAPICMD(CAPI_RESET_B3,CAPI_RESP)

#define CAPI_LISTEN_REQ              CAPICMD(CAPI_LISTEN,CAPI_REQ)
#define CAPI_LISTEN_CONF             CAPICMD(CAPI_LISTEN,CAPI_CONF)

#define CAPI_MANUFACTURER_REQ        CAPICMD(CAPI_MANUFACTURER,CAPI_REQ)
#define CAPI_MANUFACTURER_CONF       CAPICMD(CAPI_MANUFACTURER,CAPI_CONF)
#define CAPI_MANUFACTURER_IND        CAPICMD(CAPI_MANUFACTURER,CAPI_IND)
#define CAPI_MANUFACTURER_RESP       CAPICMD(CAPI_MANUFACTURER,CAPI_RESP)

#define CAPI_FACILITY_REQ            CAPICMD(CAPI_FACILITY,CAPI_REQ)
#define CAPI_FACILITY_CONF           CAPICMD(CAPI_FACILITY,CAPI_CONF)
#define CAPI_FACILITY_IND            CAPICMD(CAPI_FACILITY,CAPI_IND)
#define CAPI_FACILITY_RESP           CAPICMD(CAPI_FACILITY,CAPI_RESP)

#define CAPI_INFO_REQ                CAPICMD(CAPI_INFO,CAPI_REQ)
#define CAPI_INFO_CONF               CAPICMD(CAPI_INFO,CAPI_CONF)
#define CAPI_INFO_IND                CAPICMD(CAPI_INFO,CAPI_IND)
#define CAPI_INFO_RESP               CAPICMD(CAPI_INFO,CAPI_RESP)

/* CAPI message access helpers */

/*
 * CAPI message header:
 * word   Length
 * word   ApplId
 * byte   Command
 * byte   Subcommand
 * word   MsgId
 *
 * Note that in the following, Controller/PLCI/NCCI is coded as follows:
 * bits 0..6 = controller, bit 7 = ext/int, bits 8..15 = PLCI, and
 * bits 16..31 = NCCI value.
 *
 * ALERT_REQ, 01 80:
 * dword  PLCI
 * struct Additional Info
 *
 * ALERT_CONF, 01 81:
 * dword  PLCI
 * word   Info (0 = OK, other = cause)
 *
 * CONNECT_REQ, 02 80:
 * dword  controller
 * word   CIP
 * struct Called party number
 * struct Calling party number
 * struct Called party subaddress
 * struct Calling party subaddress
 * struct Bearer Capability
 * struct Low Layer Compatibility
 * struct High Layer Compatibility
 * struct Additional Info
 *
 * CONNECT_CONF, 02 81:
 * dword  PLCI
 * word   Info (0 = OK, other = cause)
 *
 * CONNECT_IND, 02 82:
 * dword  PLCI
 * word   CIP
 * struct Called party number
 * struct Calling party number
 * struct Called party subaddress
 * struct Calling party subaddress
 * struct Bearer Capability
 * struct Low Layer Compatibility
 * struct High Layer Compatibility
 * struct Additional Info
 * struct Second Calling party number
 *
 * CONNECT_RESP, 02 83:
 * dword  PLCI
 * word   Reject (0 = accept, 1 = ignore, 2 = reject/normal clearing)
 * struct B protocol
 * struct Connected number
 * struct Connected subaddress
 * struct Low Layer Compatibility
 * struct Additional Info
 *
 * CONNECT_ACTIVE_IND, 03 82:
 * dword  PLCI
 * struct Connected number
 * struct Connected subaddress
 * struct Low Layer Compatibility
 *
 * CONNECT_ACTIVE_RESP, 03 83:
 * dword  PLCI
 *
 * CONNECT_B3_REQ, 82 80:
 * dword  PLCI
 * struct NCPI
 *
 * CONNECT_B3_CONF, 82 81:
 * dword  NCCI
 * word   Info (0 = connected, other = cause)
 *
 * CONNECT_B3_IND, 82 82:
 * dword  NCCI
 * struct NCPI
 *
 * CONNECT_B3_RESP, 82 83:
 * dword  NCCI
 * word   Reject (0 = accept, 2 = reject/normal clearing)
 * struct NCPI
 *
 * CONNECT_B3_ACTIVE_IND, 83 82:
 * dword  NCCI
 * struct NCPI
 *
 * CONNECT_B3_ACTIVE_RESP, 83  83:
 * dword  NCCI
 *
 * DATA_B3_REQ, 86 80:
 * dword  NCCI
 * dword  Data pointer
 * word   Data length
 * word   Data handle (packet id)
 * word   Flags (02 = more)
 *
 * DATA_B3_CONF, 86 81:
 * dword  NCCI
 * word   Data handle (packet id)
 * word   Info (0 = OK, other = cause)
 *
 * DATA_B3_IND, 86 82:
 * dword  NCCI
 * dword  Data pointer
 * word   Data length
 * word   Data handle (packet id)
 * word   Flags (02 = more)
 *
 * DATA_B3_RESP, 86 83:
 * dword  NCCI
 * word   Data handle (packet id)
 *
 * DISCONNECT_B3_REQ, 84 80:
 * dword  NCCI
 * struct NCPI
 *
 * DISCONNECT_B3_CONF, 84 81:
 * dword  NCCI
 * word   Info (0 = OK, other = cause)
 *
 * DISCONNECT_B3_IND, 84 82:
 * dword  NCCI
 * word   Reason
 * struct NCPI
 *
 * DISCONNECT_B3_RESP, 84 83:
 * dword  NCCI
 *
 * DISCONNECT_REQ, 04 80:
 * dword  PLCI
 * struct Additional Info
 *
 * DISCONNECT_CONF, 04 81:
 * dword  PLCI
 * word   Info (0 = OK, other = cause)
 *
 * DISCONNECT_IND, 04 82:
 * dword  PLCI
 * word   Reason
 *
 * DISCONNECT_RESP, 04 83:
 * dword  PLCI
 *
 * LISTEN_REQ, 05 80:
 * dword  Controller
 * dword  Info mask (bits 0..9 used)
 * dword  CIP Mask (bit 0 = any match)
 * dword  CIP Mask 2 (bit 0 = any match)
 * struct Calling party number
 * struct Calling party subaddress
 *
 * LISTEN_CONF, 05 81:
 * dword  Controller
 * word   Info (0 = OK, other = cause)
 *
 * INFO_REQ, 08 80:
 * dword  Controller/PLCI
 * struct Called party number
 * struct Additional Info
 *
 * INFO_CONF, 08 81:
 * dword  Controller/PLCI
 * word   Info (0 = OK, other = cause)
 *
 * INFO_IND, 08 82:
 * dword  Controller/PLCI
 * word   Info number
 * struct Info element
 *
 * INFO_RESP, 08 83:
 * dword  Controller/PLCI
 */

#define CAPIMSG_LEN(msg)             (msg[0]|(msg[1]<<8))
#define CAPIMSG_DATALEN(msg)         (msg[16]|(msg[17]<<8))

static __inline u_int8_t* capimsg_getu8(u_int8_t *msg, u_int8_t *val)
{
    *val = *msg;
    return (msg + 1);
}

static __inline u_int8_t* capimsg_getu16(u_int8_t *msg, u_int16_t *val)
{
    *val = (msg[0]|(msg[1]<<8));
    return (msg + 2);
}

static __inline u_int8_t* capimsg_getu32(u_int8_t *msg, u_int32_t *val)
{
    *val = (msg[0]|(msg[1]<<8)|(msg[2]<<16)|(msg[3]<<24));
    return (msg + 4);
}

static __inline u_int8_t* capimsg_setu8(u_int8_t *msg, u_int8_t val)
{
    msg[0] = val;
    return (msg + 1);
}

static __inline u_int8_t* capimsg_setu16(u_int8_t *msg, u_int16_t val)
{
    msg[0] = (val & 0xff);
    msg[1] = (val >> 8) & 0xff;
    return (msg + 2);
}

static __inline u_int8_t* capimsg_setu32(u_int8_t *msg, u_int32_t val)
{
    msg[0] = (val & 0xff);
    msg[1] = (val >> 8) & 0xff;
    msg[2] = (val >> 16) & 0xff;
    msg[3] = (val >> 24) & 0xff;
    return (msg + 4);
}

/*
//  CAPI message handlers called by higher layers
*/

extern void capi_listen_req(capi_softc_t *sc, u_int32_t CIP);
extern void capi_alert_req(capi_softc_t *sc, call_desc_t *cd);
extern void capi_connect_req(capi_softc_t *sc, call_desc_t *cd);
extern void capi_connect_b3_req(capi_softc_t *sc, call_desc_t *cd);
extern void capi_connect_resp(capi_softc_t *sc, call_desc_t *cd);
extern void capi_data_b3_req(capi_softc_t *sc, int chan, struct mbuf *m);
extern void capi_disconnect_req(capi_softc_t *sc, call_desc_t *cd);

/*
//  CAPI message handlers called by the receive routine
*/

extern void capi_listen_conf(capi_softc_t *sc, struct mbuf *m);
extern void capi_info_ind(capi_softc_t *sc, struct mbuf *m);
extern void capi_alert_conf(capi_softc_t *sc, struct mbuf *m);
extern void capi_connect_conf(capi_softc_t *sc, struct mbuf *m);
extern void capi_connect_active_ind(capi_softc_t *sc, struct mbuf *m);
extern void capi_connect_b3_conf(capi_softc_t *sc, struct mbuf *m);
extern void capi_connect_b3_active_ind(capi_softc_t *sc, struct mbuf *m);
extern void capi_connect_ind(capi_softc_t *sc, struct mbuf *m);
extern void capi_connect_b3_ind(capi_softc_t *sc, struct mbuf *m);
extern void capi_data_b3_conf(capi_softc_t *sc, struct mbuf *m);
extern void capi_data_b3_ind(capi_softc_t *sc, struct mbuf *m);
extern void capi_disconnect_conf(capi_softc_t *sc, struct mbuf *m);
extern void capi_disconnect_b3_ind(capi_softc_t *sc, struct mbuf *m);
extern void capi_disconnect_ind(capi_softc_t *sc, struct mbuf *m);

#endif /* _I4B_CAPI_MSGS_H_ */
