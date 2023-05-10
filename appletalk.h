/*
 * Copyright (c) 1988, 1989, 1990, 1993, 1994, 1995, 1996
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * AppleTalk protocol formats (courtesy Bill Croft of Stanford/SUMEX).
 */

struct LAP {
	nd_uint8_t	dst;
	nd_uint8_t	src;
	nd_uint8_t	type;
};
#define lapShortDDP	1	/* short DDP type */
#define lapDDP		2	/* DDP type */
#define lapKLAP		'K'	/* Kinetics KLAP type */

/* Datagram Delivery Protocol */

struct atDDP {
	nd_uint16_t	length;
	nd_uint16_t	checksum;
	nd_uint16_t	dstNet;
	nd_uint16_t	srcNet;
	nd_uint8_t	dstNode;
	nd_uint8_t	srcNode;
	nd_uint8_t	dstSkt;
	nd_uint8_t	srcSkt;
	nd_uint8_t	type;
};

struct atShortDDP {
	nd_uint16_t	length;
	nd_uint8_t	dstSkt;
	nd_uint8_t	srcSkt;
	nd_uint8_t	type;
};

#define	ddpMaxWKS	0x7F
#define	ddpMaxData	586
#define	ddpLengthMask	0x3FF
#define	ddpHopShift	10
#define	ddpSize		13	/* size of DDP header (avoid struct padding) */
#define	ddpSSize	5
#define	ddpWKS		128	/* boundary of DDP well known sockets */
#define	ddpRTMP		1	/* RTMP type */
#define	ddpRTMPrequest	5	/* RTMP request type */
#define	ddpNBP		2	/* NBP type */
#define	ddpATP		3	/* ATP type */
#define	ddpECHO		4	/* ECHO type */
#define	ddpIP		22	/* IP type */
#define	ddpARP		23	/* ARP type */
#define ddpEIGRP        88      /* EIGRP over Appletalk */
#define	ddpKLAP		0x4b	/* Kinetics KLAP type */


/* AppleTalk Transaction Protocol */

struct atATP {
	nd_uint8_t	control;
	nd_uint8_t	bitmap;
	nd_uint16_t	transID;
	nd_uint32_t	userData;
};

#define	atpReqCode	0x40
#define	atpRspCode	0x80
#define	atpRelCode	0xC0
#define	atpXO		0x20
#define	atpEOM		0x10
#define	atpSTS		0x08
#define	atpFlagMask	0x3F
#define	atpControlMask	0xF8
#define	atpMaxNum	8
#define	atpMaxData	578


/* AppleTalk Echo Protocol */

struct atEcho {
	nd_uint8_t	echoFunction;
	nd_uint8_t	echoData[1];	/* Should be [], C99-style */
};

#define echoSkt		4		/* the echoer socket */
#define echoSize	1		/* size of echo header */
#define echoRequest	1		/* echo request */
#define echoReply	2		/* echo request */


/* Name Binding Protocol */

struct atNBP {
	nd_uint8_t	control;
	nd_uint8_t	id;
};

struct atNBPtuple {
	nd_uint16_t	net;
	nd_uint8_t	node;
	nd_uint8_t	skt;
	nd_uint8_t	enumerator;
};

#define	nbpBrRq		0x10
#define	nbpLkUp		0x20
#define	nbpLkUpReply	0x30

#define	nbpNIS		2
#define	nbpTupleMax	15

#define	nbpHeaderSize	2
#define nbpTupleSize	5

#define nbpSkt		2		/* NIS */


/* Routing Table Maint. Protocol */

#define	rtmpSkt		1	/* number of RTMP socket */
#define	rtmpSize	4	/* minimum size */
#define	rtmpTupleSize	3


/* Zone Information Protocol */

struct zipHeader {
	nd_uint8_t		command;
	nd_uint8_t		netcount;
};

#define	zipHeaderSize	2
#define	zipQuery	1
#define	zipReply	2
#define	zipTakedown	3
#define	zipBringup	4
#define	ddpZIP		6
#define	zipSkt		6
#define	GetMyZone	7
#define	GetZoneList	8

/*
 * UDP port range used for ddp-in-udp encapsulation is 16512-16639
 * for client sockets (128-255) and 200-327 for server sockets
 * (0-127).  We also try to recognize the pre-April 88 server
 * socket range of 768-895.
 */
#define atalk_port(p) \
	(((unsigned)((p) - 16512) < 128) || \
	 ((unsigned)((p) - 200) < 128) || \
	 ((unsigned)((p) - 768) < 128))
