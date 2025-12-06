/* $NetBSD$ */

/*-
 * Copyright (c) 2015 Nathanial Sloss <nathanialsloss@yahoo.com.au>
 *
 *		This software is dedicated to the memory of -
 *	   Baron James Anlezark (Barry) - 1 Jan 1949 - 13 May 2012.
 *
 *		Barry was a man who loved his music.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _AVDTP_SIGNAL_H_
#define	_AVDTP_SIGNAL_H_

#include <stdint.h>
#include <stdbool.h>

/* Our endpoint. */
#define	INTSEP				8
#define	ACPSEP				8

/* AVDTP signals. */

#define	AVDTP_DISCOVER			0x01
#define	AVDTP_GET_CAPABILITIES		0x02
#define	AVDTP_SET_CONFIGURATION		0x03
#define	AVDTP_GET_CONFIGURATION		0x04
#define	AVDTP_RECONFIGURE		0x05
#define	AVDTP_OPEN			0x06
#define	AVDTP_START			0x07
#define	AVDTP_CLOSE			0x08
#define	AVDTP_SUSPEND			0x09
#define	AVDTP_ABORT			0x0a
#define	AVDTP_SECUURITY_CONTROL		0x0b

/* Signal Command & Response Header Masks. */

#define	TRANSACTIONLABEL		0xf0
#define	TRANSACTIONLABEL_S		4
#define	SIGNALID_MASK			0x3f
#define	PACKETTYPE			0x0c
#define	PACKETTYPE_S			0x02
#define	MESSAGETYPE			0x03
#define	SIGNALIDENTIFIER		0x3f
#define	DISCOVER_SEP_IN_USE		0x02
#define	DISCOVER_IS_SINK		0x08

/* Packet Types */
#define	singlePacket			0x0
#define	startPacket			0x1
#define	continuePacket			0x2
#define	endPacket			0x3

/* Message Types */
#define	COMMAND				0x0
#define	RESPONSEACCEPT			0x2
#define	RESPONSEREJECT			0x3

/* Response general error/success lengths */
#define	AVDTP_LEN_SUCCESS		2
#define	AVDTP_LEN_ERROR			3

/* Error codes */
#define	BAD_HEADER_FORMAT		0x01
#define	BAD_LENGTH			0x11
#define	BAD_ACP_SEID			0x12
#define	SEP_IN_USE			0x13
#define	SEP_NOT_IN_USE			0x14
#define	BAD_SERV_CATAGORY		0x17
#define	BAD_PAYLOAD_FORMAT		0x18
#define	NOT_SUPPORTED_COMMAND		0x19
#define	INVALID_CAPABILITIES		0x1a

#define	BAD_RECOVERY_TYPE		0x22
#define	BAD_MEDIA_TRANSPORT_FORMAT	0x23
#define	BAD_RECOVERY_FORMAT		0x25
#define	BAD_ROHC_FORMAT			0x26
#define	BAD_CP_FORMAT			0x27
#define	BAD_MULTIPLEXING_FORMAT		0x28
#define	UNSUPPORTED_CONFIGURATION	0x29
#define	BAD_STATE			0x31

/* Service Capabilities Field. */
#define	mediaTransport			0x1
#define	reporting			0x2
#define	recovery			0x3
#define	contentProtection		0x4
#define	headerCompression		0x5
#define	multiplexing			0x6
#define	mediaCodec			0x7

/* Media Codec Capabilities */
#define	mediaCodecSbc			0x00
#define	mediaCodecMpeg1			0x01
#define	mediaCodecMpeg2			0x02

#define	SBC_CODEC_ID			0x0
#define	mediaTypeAudio			0x0

struct bt_config;

int	avdtpSendAccept(int, uint8_t, uint8_t);
int	avdtpSendReject(int, uint8_t, uint8_t);
int	avdtpSendDiscResponseAudio(int, uint8_t, uint8_t, uint8_t);
int	avdtpDiscoverAndConfig(struct bt_config *, bool);
int	avdtpSetConfiguration(int, uint8_t, uint8_t *, int);
int	avdtpOpen(int, uint8_t);
int	avdtpStart(int, uint8_t);
int	avdtpClose(int, uint8_t);
int	avdtpSuspend(int, uint8_t);
int	avdtpAbort(int, uint8_t);

/* Return < 0 if error, processed signal otherwise. */
int	avdtpACPHandlePacket(struct bt_config *cfg);
/* Free state allocated in avdtpACPHandlePacket(), if any. */
void	avdtpACPFree(struct bt_config *cfg);

#endif					/* _AVDTP_SIGNAL_H_ */
