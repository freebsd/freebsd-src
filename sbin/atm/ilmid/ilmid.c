/*
 *
 * ===================================
 * HARP  |  Host ATM Research Platform
 * ===================================
 *
 *
 * This Host ATM Research Platform ("HARP") file (the "Software") is
 * made available by Network Computing Services, Inc. ("NetworkCS")
 * "AS IS".  NetworkCS does not provide maintenance, improvements or
 * support of any kind.
 *
 * NETWORKCS MAKES NO WARRANTIES OR REPRESENTATIONS, EXPRESS OR IMPLIED,
 * INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE, AS TO ANY ELEMENT OF THE
 * SOFTWARE OR ANY SUPPORT PROVIDED IN CONNECTION WITH THIS SOFTWARE.
 * In no event shall NetworkCS be responsible for any damages, including
 * but not limited to consequential damages, arising from or relating to
 * any use of the Software or related support.
 *
 * Copyright 1994-1998 Network Computing Services, Inc.
 *
 * Copies of this Software may be made, however, the above copyright
 * notice must be reproduced on all copies.
 *
 *      @(#) $Id: ilmid.c,v 1.9 1998/08/13 20:15:28 jpt Exp $
 *
 */

/*
 * User utilities
 * --------------
 *
 * Implement very minimal ILMI address registration.
 *
 * Implement very crude and basic support for "cracking" and
 * "encoding" SNMP PDU's to support ILMI prefix and NSAP address
 * registration. Code is not robust nor is it meant to provide any
 * "real" SNMP support. Much of the code expects predetermined values
 * and will fail if anything else is found. Much of the "encoding" is
 * done with pre-computed PDU's.
 *
 * See "The Simple Book", Marshall T. Rose, particularly chapter 5,
 * for ASN and BER information.
 *
 */

#ifndef	lint
static char *RCSid = "@(#) $Id: ilmid.c,v 1.9 1998/08/13 20:15:28 jpt Exp $";
#endif

#include <sys/types.h>
#include <sys/param.h>

#if (defined(BSD) && (BSD >= 199103))
#include <err.h>
#endif

#ifdef	BSD
#if __FreeBSD_version < 300001
#include <stdlib.h>
#ifdef	sun
#include <unistd.h>
#endif	/* sun */
#else
#include <unistd.h>
#endif	/* __FreeBSD_version >= 300001 */
#endif	/* BSD */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <syslog.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>

#include <netatm/port.h>
#include <netatm/atm.h>
#include <netatm/atm_if.h>
#include <netatm/atm_sigmgr.h>
#include <netatm/atm_sap.h>
#include <netatm/atm_sys.h>
#include <netatm/atm_ioctl.h>

#include <dev/hea/eni_stats.h>
#include <dev/hfa/fore_aali.h>
#include <dev/hfa/fore_slave.h>
#include <dev/hfa/fore_stats.h>
#include <netatm/uni/unisig_var.h>

#define	MAX_LEN		9180

#define	MAX_UNITS	8

/*
 * Time to sleep between loops
 */
#define	SLEEP_TIME	10
/*
 * Time to pass between sending coldStart TRAPs
 */
#define	TRAP_TIME	5

/*
 * Define some ASN types
 */
#define	ASN_INTEGER	0x02
#define	ASN_OCTET	0x04
#define	ASN_OBJID	0x06
#define	ASN_SEQUENCE	0x30
#define	ASN_IPADDR	0x40
#define	ASN_TIMESTAMP	0x43

/*
 * Define SNMP PDU types
 */
#define	PDU_TYPE_GET		0xA0
#define	PDU_TYPE_GETNEXT	0xA1
#define	PDU_TYPE_GETRESP	0xA2
#define	PDU_TYPE_SET		0xA3
#define	PDU_TYPE_TRAP		0xA4

/*
 * Every SNMP PDU has the first four fields of this header. The only type
 * which doesn't have the last three fields is the TRAP type.
 */
struct snmp_header {
	int	pdulen;
	int	version;
	char	community[64];
	int	pdutype;
	int	reqid;
	int	error;
	int	erridx;
};
typedef struct snmp_header Snmp_Header;

/*
 * Define our internal representation of an OBJECT IDENTIFIER
 */
struct objid {
	int	oid[128];
};
typedef struct objid Objid;

/*
 * Define some OBJET IDENTIFIERS that we'll try to reply to:
 *
 * sysUpTime: number of time ticks since this deamon came up
 * netpfx_oid:	network prefix table
 * unitype:	is this a PRIVATE or PUBLIC network link
 * univer:	which version of UNI are we running
 * devtype:	is this a USER or NODE ATM device
 * setprefix:	used when the switch wants to tell us its NSAP prefix
 * foresiggrp:	FORE specific Objid we see alot of (being connected to FORE
 *			switches...)
 */
Objid	sysObjId =	{  8, 43, 6, 1, 2, 1, 1, 2, 0 };
Objid	sysUpTime =	{  8, 43, 6, 1, 2, 1, 1, 3, 0 };
Objid	foresiggrp =	{ 18, 43, 6, 1, 4, 1, 326, 2, 2, 2, 1,  6, 2, 1, 1, 1, 20, 0, 0 };
Objid	portidx =	{ 12, 43, 6, 1, 4, 1, 353, 2, 1, 1, 1, 1, 0 };
Objid	myipnm =	{ 10, 43, 6, 1, 4, 1, 353, 2, 1, 2, 0 };
Objid	layeridx =	{ 12, 43, 6, 1, 4, 1, 353, 2, 2, 1, 1,  1, 0 };
Objid	maxvcc =	{ 12, 43, 6, 1, 4, 1, 353, 2, 2, 1, 1,  3, 0 };
Objid	unitype =	{ 12, 43, 6, 1, 4, 1, 353, 2, 2, 1, 1,  8, 0 };
Objid	univer =	{ 12, 43, 6, 1, 4, 1, 353, 2, 2, 1, 1,  9, 0 };
Objid	devtype =	{ 12, 43, 6, 1, 4, 1, 353, 2, 2, 1, 1, 10, 0 };
Objid	netpfx_oid =	{  9, 43, 6, 1, 4, 1, 353, 2, 7, 1 };
Objid	setprefix =	{ 12, 43, 6, 1, 4, 1, 353, 2, 7, 1, 1,  3, 0 };
/*
 * (Partialy) pre-encoded SNMP responses
 */

/*
 * sysObjId reply
 */
u_char	sysObjId_Resp[] = {
	54,					/* <--- len */
	0x30,					/* Sequence of */
	0x82, 0x00, 0x32,			/* <--- len */
	0x02, 0x01, 0x00,			/* (Version - 1) */
	0x04, 0x04, 0x49, 0x4c, 0x4d, 0x49,	/* Community: ILMI */
	PDU_TYPE_GETRESP,			/* GET Response */
	0x27,					/* <--- len */
	0x02, 0x04, 0x00, 0x00, 0x00, 0x00,	/* <--- request id */
	0x02, 0x01, 0x00,			/* Error Status */
	0x02, 0x01, 0x00,			/* Error Index */
	0x30,					/* Sequence of */
	0x82, 0x00, 0x17,			/* <--- len */
	0x82, 0x00, 0x14,			/* <--- len */
	0x06, 0x08,			/* Objid: 1.3.6.1.4.1.1.2.0 */
		0x2b, 0x06, 0x01, 0x04, 0x01, 0x01, 0x02, 0x00,
	0x06, 0x08,			/* Objid: 1.3.6.1.4.1.9999.1 */
		0x2b, 0x06, 0x01, 0x04, 0x01, 0xce, 0x0f, 0x01
};

/*
 * sysUpTime: reply to a sysUpTime GET request
 */
u_char	sysUpTime_Resp[] = {
	45,					/* <--- len */
	0x30,					/* Sequence of */
	0x82, 0x00, 0x29,			/* <--- len */
	0x02, 0x01, 0x00,			/* (Version - 1) */
	0x04, 0x04, 0x49, 0x4c, 0x4d, 0x49,	/* Community  - ILMI */
	PDU_TYPE_GETRESP,			/* GET Response */
	0x1e,					/* <--- len */
	0x02, 0x04, 0x00, 0x00, 0x00, 0x00,	/* <--- request id */
	0x02, 0x01, 0x00,			/* Error Status */
	0x02, 0x01, 0x00,			/* Error Index */
	0x30,					/* Sequence of */
	0x82, 0x00, 0x0E,			/* <--- len */
	0x30,					/* Sequence of */
	0x82, 0x00, 0x0A,			/* <--- len */
						/* Objid: .1.3.6.1.2.1.1.3.0 */
	0x06, 0x08, 0x2b, 0x06, 0x01, 0x02, 0x01, 0x01, 0x03, 0x00,
						/* <--- uptime */
};

/*
 * coldStart TRAP to start the ILMI protocol
 */
u_char	coldStart_Trap[] = {
	60,
	0x30,					/* Sequence of */
	0x82, 0x00, 0x38,			/* <--- len */
	0x02, 0x01, 0x00,			/* (Version - 1) */
	0x04, 0x04, 0x49, 0x4c, 0x4d, 0x49,	/* Community: ILMI */
	PDU_TYPE_TRAP,				/* TRAP */
	0x2d,					/* <--- len */
	0x06, 0x08,				/* Objid: .1.3.6.1.4.1.3.1.1 */
		0x2b, 0x06, 0x01, 0x04, 0x01, 0x03, 0x01, 0x01,
	0x40, 0x04, 0x00, 0x00, 0x00, 0x00,	/* IP address - 0.0.0.0 */
	0x02, 0x01, 0x00,			/* generic trap */
	0x02, 0x01, 0x00,			/* specific trap */
	0x43, 0x01, 0x00,			/* Time ticks - 0 */
	0x30,					/* Sequence of */
	0x82, 0x00, 0x10,			/* <--- len */
	0x30,					/* Sequence of */
	0x82, 0x00, 0x0c,			/* <-- len */
	0x06, 0x08,				/* Objid: 1.3.6.1.2.1.1.3.0 */
		0x2b, 0x06, 0x01, 0x02, 0x01, 0x01, 0x03, 0x00,
	0x05, 0x00				/* Null */
};

u_char	GetNext_Resp[] = {
	49,
	0x30,					/* Sequence of */
	0x82, 0x00, 0x2d,			/* <--- len */
	0x02, 0x01, 0x00,			/* (Version - 1) */
	0x04, 0x04, 0x49, 0x4c, 0x4d, 0x49,	/* Community: ILMI */
	PDU_TYPE_GETRESP,			/* PDU_TYPE_GETRESP */
	0x22,					/* <--- len */
	0x02, 0x04, 0x00, 0x00, 0x00, 0x00,	/* Request ID */
	0x02, 0x01, 0x02,			/* Error Status */
	0x02, 0x01, 0x01,			/* Error Index */
	0x30,					/* Sequence of */
	0x82, 0x00, 0x12,			/* <--- len */
	0x30,					/* Seqence of */
	0x82, 0x00, 0x0e,			/* <--- len */
	0x06, 0x0a,			/* Objid: .1.3.6.4.1.353.2.7.1 */
		 0x2b, 0x06, 0x01, 0x04, 0x01, 0x82, 0x61, 0x02, 0x07, 0x01,
	0x05, 0x00				/* Get response: NULL */
};

/*
 * Reply to GET myIpNm
 */
u_char	MyIpNm_Resp[] = {
	54,
	0x30,					/* Sequence of */
	0x82, 0x00, 0x32,			/* <--- len */
	0x02, 0x01, 0x00,			/* (Version - 1) */
	0x04, 0x04, 0x49, 0x4c, 0x4d, 0x49,	/* Community: ILMI */
	PDU_TYPE_GETRESP,			/* PDU_TYPE_GETRESP */
	0x27,					/* <--- len */
	0x02, 0x04, 0x00, 0x00, 0x00, 0x00,	/* Request ID */
	0x02, 0x01, 0x00,			/* Error Status */
	0x02, 0x01, 0x00,			/* Error Index */
	0x30,					/* Sequence of */
	0x82, 0x00, 0x17,			/* <--- len */
	0x30,					/* Sequence of */
	0x82, 0x00, 0x13,			/* <--- len */
					/* Objid: .1.3.6.1.4.1.353.2.1.2.1 */
	0x06, 0x0B, 0x2b, 0x06, 0x01, 0x04, 0x01, 0x82, 0x61, 0x02,
		0x01, 0x02, 0x01,
	0x40, 0x04, 0x00, 0x00, 0x00, 0x00	/* IP address */
};

/*
 * Reply to GET portIndex - we're always 1 + unit number
 */
u_char	PortIndex_Resp[] = {
	53,
	0x30,					/* Sequence of */
	0x82, 0x00, 0x31,			/* <-- len */
	0x02, 0x01, 0x00,			/* (Version - 1) */
	0x04, 0x04, 0x49, 0x4c, 0x4d, 0x49,	/* Community: ILMI */
	PDU_TYPE_GETRESP,
	0x26,					/* <-- len */
	0x02, 0x04, 0x00, 0x00, 0x00, 0x00,	/* Request ID */
	0x02, 0x01, 0x00,			/* Error Status */
	0x02, 0x01, 0x00,			/* Error Index */
	0x30,					/* Sequence of */
	0x82, 0x00, 0x16,			/* <--- len */
	0x30,					/* Sequence of */
	0x82, 0x00, 0x12,			/* <--- len */
				/* Objid: .1.3.6.1.4.1.353.2.1.1.1.1.x */
	0x06, 0x0d, 0x2b, 0x06, 0x01, 0x04, 0x01, 0x82, 0x61, 0x02,
		0x01, 0x01, 0x01, 0x01, 0x00,
	0x02, 0x01, 0x00,			/* Value */
};

/*
 * Reply to GET MaxVcc
 */
u_char	maxVCC_Resp[] = {
	52,					/* <--- len */
	0x30,					/* Sequence of */
	0x82, 0x00, 0x30,			/* <--- len */
	0x02, 0x01, 0x01,			/* (Version - 1) */
	0x04, 0x04, 0x49, 0x4c, 0x4d, 0x49,	/* Community: ILMI */
	PDU_TYPE_GETRESP,			/* GET Response */
	0x25,					/* <--- len */
	0x02, 0x04, 0x00, 0x00, 0x00, 0x00,	/* <--- request id */
	0x02, 0x01, 0x00,			/* Error Status */
	0x02, 0x01, 0x00,			/* Error Index */
	0x30,					/* Sequence of */
	0x82, 0x16,				/* <--- len */
	0x30,					/* Sequence of */
	0x82, 0x13,				/* <--- len */
	0x06, 0x0d,		/* Objid: 1.3.6.1.4.1.353.2.2.1.1.3.0 */
		0x2b, 0x06, 0x01, 0x04, 0x01, 0x82, 0x61, 0x02,
			0x02, 0x01, 0x01, 0x03, 0x00,
	0x02, 0x02, 0x04, 0x00			/* Value = 1024 */
};

/*
 * Reply to GET uniType - we only support PRIVATE
 */
u_char	UniType_Resp[] = {
	53,
	0x30,					/* Sequence of */
	0x82, 0x00, 0x31,			/* <--- len */
	0x02, 0x01, 0x00,			/* (Version - 1) */
	0x04, 0x04, 0x49, 0x4c, 0x4d, 0x49,	/* Community: ILMI */
	PDU_TYPE_GETRESP,			/* PDU_TYPE_GETRESP */
	0x26,					/* <--- len */
	0x02, 0x04, 0x00, 0x00, 0x00, 0x00,	/* Request ID */
	0x02, 0x01, 0x00,			/* Error Status */
	0x02, 0x01, 0x00,			/* Error Index */
	0x30,					/* Sequence of */
	0x82, 0x00, 0x16,			/* <--- len */
	0x30,					/* Sequence of */
	0x82, 0x00, 0x12,			/* <--- len */
				/* Objid: .1.3.6.1.4.1.353.2.2.1.1.8.0 */
	0x06, 0x0d, 0x2b, 0x06, 0x01, 0x04, 0x01, 0x82, 0x61, 0x02, 0x02,
		0x01, 0x01, 0x08, 0x00,
	0x02, 0x01, 0x02			/* Get response: Integer */
						/* = UNITYPE_PRIVATE (2) */
};

#define	UNIVER_UNI30	2
#define	UNIVER_UNI31	3
#define	UNIVER_UNI40	4

/*
 * Reply to GET uniVer
 */
u_char	UniVer_Resp[] = {
	53,
	0x30,					/* Sequence of */
	0x82, 0x00, 0x31,			/* <--- len */
	0x02, 0x01, 0x00,			/* (Version - 1) */
	0x04, 0x04, 0x49, 0x4c, 0x4d, 0x49,	/* Community: ILMI */
	PDU_TYPE_GETRESP,			/* PDU_TYPE_GETRESP */
	0x26,					/* <--- len */
	0x02, 0x04, 0x00, 0x00, 0x00, 0x00,	/* Request ID */
	0x02, 0x01, 0x00,			/* Error Status */
	0x02, 0x01, 0x00,			/* Error Index */
	0x30,					/* Sequence of */
	0x82, 0x00, 0x16,			/* <--- len */
	0x30,					/* Sequence of */
	0x82, 0x00, 0x12,			/* <--- len */
				/* Objid: .1.3.6.1.4.1.353.2.2.1.1.9.0 */
	0x06, 0x0d, 0x2b, 0x06, 0x01, 0x04, 0x01, 0x82, 0x61, 0x02, 0x02,
		0x01, 0x01, 0x09, 0x00,
	0x02, 0x01, 0x02			/* Get response: Integer */
						/* = UNIVER_UNI30 (2) */
};

/*
 * Reply to GET devType - we're a host therefore we're type USER
 */
u_char	DevType_Resp[] = {
	53,
	0x30,					/* Sequence of */
	0x82, 0x00, 0x31,			/* <--- len */
	0x02, 0x01, 0x00,			/* (Version -1) */
	0x04, 0x04, 0x49, 0x4c, 0x4d, 0x49,	/* Community: ILMI */
	PDU_TYPE_GETRESP,			/* PDU_TYPE_GETRESP */
	0x26,					/* <--- len */
	0x02, 0x04, 0x00, 0x00, 0x00, 0x00,	/* Request ID */
	0x02, 0x01, 0x00,			/* Error Status */
	0x02, 0x01, 0x00,			/* Error Index */
	0x30,					/* Sequence of */
	0x82, 0x00, 0x16,			/* <--- len */
	0x30,					/* Sequence of */
	0x82, 0x00, 0x12,			/* <--- len */
				/* Objid: .1.3.6.1.4.1.353.2.2.1.1.10.0 */
	0x06, 0x0d, 0x2b, 0x06, 0x01, 0x04, 0x01, 0x82, 0x61, 0x02, 0x02,
		0x01, 0x01, 0x0a, 0x00,
	0x02, 0x01, 0x01			/* Get response: Integer */
						/* = DEVTYPE_USER (1) */
};

/*
 * Reply to GET foreSigGroup.* with noSuchError
 */
u_char NoSuchFore_Resp[] = {
	85,
	0x30,					/* Sequence of */
	0x82, 0x00, 0x51,			/* <--- len */
	0x02, 0x01, 0x00,			/* (Version - 1) */
	0x04, 0x04, 0x49, 0x4c, 0x4d, 0x49,	/* Community: ILMI */
	PDU_TYPE_GETRESP,			/* PDU_TYPE_GETRESP */
	0x46,					/* <--- len */
	0x02, 0x04, 0x00, 0x00, 0x00, 0x00,	/* Request ID */
	0x02, 0x01, 0x02,			/* Error Status: noSuch (2) */
	0x02, 0x01, 0x01,			/* Error Index */
	0x30,					/* Sequence of */
	0x82, 0x00, 0x36,			/* <--- len */
	0x30,					/* Sequence of */
	0x82, 0x00, 0x17,			/* <--- len */
			/* Objid: .1.3.6.1.5.1.326.2.2.2.1.6.2.1.1.1.20.0.0 */
	0x06, 0x13,
		0x2b, 0x06, 0x01, 0x04, 0x01, 0x82, 0x46,
		0x02, 0x02, 0x02, 0x01, 0x06, 0x02, 0x01, 0x01,
		0x01, 0x14, 0x00, 0x00,
	0x05, 0x00,				/* NULL */
	0x30,					/* Sequence of */
	0x82, 0x00, 0x17,			/* <--- len */
			/* Objid: .1.3.6.1.5.1.326.2.2.2.1.6.2.1.1.1.21.0.0 */
	0x06, 0x13,
		0x2b, 0x06, 0x01, 0x04, 0x01, 0x82, 0x46,
		0x02, 0x02, 0x02, 0x01, 0x06, 0x02, 0x01, 0x01,
		0x01, 0x15, 0x00, 0x00,
	0x05, 0x00				/* NULL */
};

u_char	NetPrefix_Resp[] = {
	50,
	0x30,					/* Sequence of */
	0x82, 0x00, 0x00,			/* <--- len */
	0x02, 0x01, 0x00,			/* (Version - 1) */
	0x04, 0x04, 0x49, 0x4c, 0x4d, 0x49,	/* Community: ILMI */
	PDU_TYPE_SET,				/* PDU_TYPE_SET */
	0x00,					/* <--- len */
	0x02, 0x04, 0x00, 0x00, 0x00, 0x00,	/* Request ID */
	0x02, 0x01, 0x00,
	0x02, 0x01, 0x00,
	0x30,					/* Sequence of */
	0x82, 0x00, 0x00,			/* <--- len */
	0x30,					/* Sequence of */
	0x82, 0x00, 0x00,			/* <--- len */
				/* Objid: .1.3.6.1.4.1.353.2.6.1.1.3.0. */
	0x06, 0x00,
	0x2b, 0x06, 0x01, 0x04, 0x01, 0x82, 0x61, 0x02,
	0x06, 0x01, 0x01, 0x03, 0x00
	/* Remainder of Objid plus SET value INTEGER =1 */
};

/*
 * Our (incrementing) Request ID
 */
int	Req_ID = 0;

/*
 * Temporary buffer for building response packets. Should help ensure
 * that we aren't accidently overwriting some other memory.
 */
u_char	Resp_Buf[1024];

/*
 * Copy the reponse into a buffer we can modify without
 * changing the original...
 */
#define	COPY_RESP(resp)	\
        UM_COPY ( (resp), Resp_Buf, (resp)[0] + 1 )

/*
 * TRAP generic trap types
 */
char	*Traps[] = { "coldStart", "warmStart", "linkDown", "linkUp",
		"authenticationFailure", "egpNeighborLoss",
			"enterpriseSpecific" };


int                     NUnits;
/*
 * Time last coldStart trap was sent to this unit
 */
time_t			last_trap[MAX_UNITS];
/*
 * fd for units still awiting coldStart TRAP from network side
 */
int			trap_fd[MAX_UNITS];
/*
 * fd for units which have seen a coldStart TRAP and are now exchaning SNMP requests
 */
int			ilmi_fd[MAX_UNITS];
/*
 * Local copy for HARP physical configuration information
 */
struct air_cfg_rsp      Cfg[MAX_UNITS + 1];
/*
 * Local copy for HARP interface configuration information
 */
struct air_int_rsp      Intf[MAX_UNITS + 1];

/*
 * When this daemon started
 */
struct timeval	starttime;

int	Debug_Level = 0;

char	*progname;
char	hostname[80];

				/* File to write debug messages to */
#define	LOG_FILE	"/var/log/ilmid"
FILE	*Log;			/* File descriptor for log messages */

extern int errno;

#ifdef	sun
extern char	*optarg;
extern int	optind, opterr;
extern int	getopt __P((int, char **, char *));
#endif	/* sun */

void	set_reqid __P ( ( u_char *, int ) );
void	Increment_DL __P ( ( int ) );
void	Decrement_DL __P ( ( int ) );

static char	*Months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
			     "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

/*
 * Write a syslog() style timestamp
 *
 * Write a syslog() style timestamp with month, day, time and hostname
 * to the log file.
 *
 * Arguments:
 *	none
 *
 * Returns:
 *	none
 *
 */
void
write_timestamp()
{
	time_t		clock;
	struct tm 	*tm;

	clock = time ( (time_t)NULL );
	tm = localtime ( &clock );

	if ( Log )
	    fprintf ( Log, "%.3s %2d %.2d:%.2d:%.2d %s: ",
		Months[tm->tm_mon], tm->tm_mday, tm->tm_hour, tm->tm_min,
		    tm->tm_sec, hostname );

	return;

}

/*
 * Utility to pretty print buffer as hex dumps
 * 
 * Arguments:
 *	bp	- buffer pointer
 *	len	- length to pretty print
 *
 * Returns:
 *	none
 *
 */
void
hexdump ( bp, len )
	u_char	*bp;
	int	len;
{
	int	i, j;

	/*
	 * Print as 4 groups of four bytes. Each byte seperated
	 * by space, each block of four seperated, and two blocks`
	 * of eight also seperated.
	 */
	for ( i = 0; i < len; i += 16 ) {
		if ( Log )
			write_timestamp();
		for ( j = 0; j < 4 && j + i < len; j++ )
		    if ( Log )
			fprintf ( Log, "%.2x ", *bp++ );
		if ( Log )
		    fprintf ( Log, " " );
		for ( ; j < 8 && j + i < len; j++ )
		    if ( Log )
			fprintf ( Log, "%.2x ", *bp++ );
		if ( Log )
		    fprintf ( Log, "  " );
		for ( ; j < 12 && j + i < len; j++ )
		    if ( Log )
			fprintf ( Log, "%.2x ", *bp++ );
		if ( Log )
		    fprintf ( Log, " " );
		for ( ; j < 16 && j + i < len; j++ )
		    if ( Log )
			fprintf ( Log, "%.2x ", *bp++ );
		if ( Log )
		    fprintf ( Log, "\n" );
	}

	return;

}

/*
 * Get lengths from PDU encodings
 *
 * Lengths are sometimes encoded as a single byte if the length
 * is less the 127 but are more commonly encoded as one byte with
 * the high bit set and the lower seven bits indicating the nuber
 * of bytes which make up the length value. Trailing data is (to my
 * knowledge) not 7-bit encoded.
 *
 * Arguments:
 * 	bufp	- pointer to buffer pointer
 *
 * Returns: 
 *	bufp	- updated buffer pointer
 *	<len>	- decoded length
 *
 */
int
asn_get_pdu_len ( bufp )
	u_char **bufp;
{
	u_char	*bp = *bufp;
	int	len = 0;
	int	i, b;

	b = *bp++;
	 if ( b & 0x80 ) {
		for ( i = 0; i < (b & ~0x80); i++ )
			len = len * 256 + *bp++;
	} else
		len = b;

	*bufp = bp;
	return ( len );
}

/*
 * Get an 7-bit encoded value.
 *
 * Get a value which is represented using a 7-bit encoding. The last
 * byte in the stream has the high-bit clear.
 *
 * Arguments:
 *	bufp	- pointer to the buffer pointer
 *	len	- pointer to the buffer length
 *
 * Returns:
 *	bufp	- updated buffer pointer
 *	len	- updated buffer length
 *	<val>	- value encoding represented
 *
 */
int
asn_get_encoded ( bufp, len )
	u_char	**bufp;
	int	*len;
{
	u_char	*bp = *bufp;
	int	val = 0;
	int	l = *len;

	/*
	 * Keep going while high bit is set
	 */
	do {
		/*
		 * Each byte can represent 7 bits
	 	 */
		val = ( val << 7 ) + ( *bp & ~0x80 );
		l--;
	} while ( *bp++ & 0x80 );

	*bufp = bp;		/* update buffer pointer */
	*len = l;		/* update buffer length */

	return ( val );
}

/*
 * Get a BER encoded integer
 *
 * Intergers are encoded as one byte length followed by <length> data bytes
 *
 * Arguments:
 *	bufp	- pointer to the buffer pointer
 *
 * Returns:
 *	bufp	- updated buffer pointer 
 *	<val>	- value of encoded integer
 *
 */
int
asn_get_int ( bufp )
	u_char **bufp;
{
	int	i;
	int	len;
	int	v = 0;
	u_char	*bp = *bufp;

	len = *bp++;
	for ( i = 0; i < len; i++ ) {
		v = (v * 256) + *bp++;
	}
	*bufp = bp;
	return ( v );
}

/*
 * Utility to print a object identifier
 *
 * Arguments:
 *	objid	- pointer to objid representation
 *
 * Returns:
 *	none
 *
 */
void
print_objid ( objid )
	Objid	*objid;
{
	int	i;

	/*
	 * First oid coded as 40 * X + Y
	 */
	if ( Log ) {
	    write_timestamp();
	    fprintf ( Log, ".%d.%d", objid->oid[1] / 40,
		objid->oid[1] % 40 );
	}
	for ( i = 2; i <= objid->oid[0]; i++ )
	    if ( Log )
		fprintf ( Log, ".%d", objid->oid[i] );
	if ( Log )
	    fprintf ( Log, "\n" );

	return;
}

/*
 * Get Object Identifier
 *
 * Arguments:
 *	bufp	- pointer to buffer pointer
 *	objid	- pointer to objid buffer
 *
 * Returns:
 *	bufp	- updated buffer pointer
 *	objid	- internal representation of encoded objid
 *
 */
void
asn_get_objid ( bufp, objid )
	u_char **bufp;
	Objid *objid;
{
	int	len;
	u_char	*bp = *bufp;
	int	*ip = (int *)objid + 1;	/* First byte will contain length */
	int	oidlen = 0;

	len = *bp++;
	while ( len ) {
		*ip++ = asn_get_encoded ( &bp, &len );
		oidlen++;
	}
	objid->oid[0] = oidlen;
	*bufp = bp;

	if ( Debug_Level > 1 )
		print_objid ( objid );

	return;
}

/*
 * Get OCTET STRING
 *
 * Octet strings are encoded as a 7-bit encoded length followed by <len>
 * data bytes;
 *
 * Arguments:
 *	bufp	- pointer to buffer pointer
 *	octet	- pointer to octet buffer
 *
 * Returns:
 *	bufp	- updated buffer pointer
 *	octet	- encoded Octet String
 *
 */ 
void
asn_get_octet ( bufp, octet )
	u_char **bufp;
	char *octet;
{
	u_char	*bp = *bufp;
	int	i = 0;
	int	len = 0;

	/*
	 * &i is really a dummy value here as we don't keep track
	 * of the ongoing buffer length
	 */
	len = asn_get_encoded ( &bp, &i );

	for ( i = 0; i < len; i++ )
		*octet++ = *bp++;

	*bufp = bp;

	return;

}

/*
 * Utility to print SNMP PDU header information
 *
 * Arguments:
 *	Hdr	- pointer to internal SNMP header structure
 *
 * Returns:
 *	none
 *
 */
void
print_header ( Hdr )
	Snmp_Header *Hdr;
{
	if ( Log ) {
	    write_timestamp();
	    fprintf ( Log,
		"Pdu len: %d Version: %d Community: \"%s\" Pdu Type: 0x%x\n",
		    Hdr->pdulen, Hdr->version + 1, Hdr->community,
			Hdr->pdutype );
	}
	if ( Hdr->pdutype != PDU_TYPE_TRAP && Log )
	    fprintf ( Log, "\tReq Id: 0x%x Error: %d Error Index: %d\n",
		Hdr->reqid, Hdr->error, Hdr->erridx );

	return;

}

/*
 * Crack the SNMP header
 *
 * Pull the PDU length, SNMP version, SNMP community and PDU type.
 * If present, also pull out the Request ID, Error status, and Error
 * index values.
 *
 * Arguments:
 *	bufp	- pointer to buffer pointer
 *
 * Returns:
 *	bufp	- updated buffer pointer
 *		- generated SNMP header
 *
 */
Snmp_Header *
asn_get_header ( bufp )
	u_char **bufp;
{
	Snmp_Header	*h;
	u_char		*bp = *bufp;

	/*
	 * Allocate memory to hold the SNMP header
	 */
	if ( ( h = (Snmp_Header *)UM_ALLOC(sizeof(Snmp_Header)) ) == NULL )
		return ( (Snmp_Header *)NULL );

	/*
	 * PDU has to start as SEQUENCE OF
	 */
	if ( *bp++ != ASN_SEQUENCE ) /* Class == Universial, f == 1, tag == SEQUENCE */
		return ( (Snmp_Header *)NULL );

	/*
	 * Get the length of remaining PDU data
	 */
	h->pdulen = asn_get_pdu_len ( &bp );

	/*
	 * We expect to find an integer encoding Version-1
	 */
	if ( *bp++ != ASN_INTEGER ) {
		return ( (Snmp_Header *)NULL );
	}
	h->version = asn_get_int ( &bp );

	/*
	 * After the version, we need the community name
	 */
	if ( *bp++ != ASN_OCTET ) {
		return ( (Snmp_Header *)NULL );
	}
	UM_ZERO ( h->community, sizeof ( h->community ) );
	asn_get_octet ( &bp, h->community );

	/*
	 * Single byte PDU type
	 */
	h->pdutype = *bp++;

	/*
	 * If this isn't a TRAP PDU, then look for the rest of the header
	 */
	if ( h->pdutype != PDU_TYPE_TRAP ) {	/* TRAP uses different format */

		bp++;				/* Skip over data len */

		/* Request ID */
		if ( *bp++ != ASN_INTEGER ) {
			return ( (Snmp_Header *)NULL );
		}
		h->reqid = asn_get_int ( &bp );

		/* Error Status */
		if ( *bp++ != ASN_INTEGER ) {
			return ( (Snmp_Header *)NULL );
		}
		h->error = asn_get_int ( &bp );

		/* Error Index */
		if ( *bp++ != ASN_INTEGER ) {
			return ( (Snmp_Header *)NULL );
		}
		h->erridx = asn_get_int ( &bp );

	}

	*bufp = bp;

	if ( Debug_Level > 2 )
		print_header ( h );

	return ( h );

}

/*
 * Compare to internal OID representations
 *
 * Arguments:
 *	oid1	- Internal Object Identifier
 *	oid2	- Internal Object Identifier
 *
 * Returns:
 *	0	- Objid's match
 *	1	- Objid's don't match
 *
 */
int
oid_cmp ( oid1, oid2 )
	Objid *oid1, *oid2;
{
	int	i;

	/*
	 * Compare lengths
	 */
	if ( !(oid1->oid[0] == oid2->oid[0]) )
		/* Different lengths */
		return ( 1 );

	/*
	 * value by value compare
	 */
	for ( i = 1; i <= oid1->oid[0]; i++ ) {
		if ( !(oid1->oid[i] == oid2->oid[i]) )
			/* values don't match */
			return ( 1 );
	}

	/* Objid's are identical */
	return ( 0 );
}

/*
 * Encode a timeval as the number of time ticks
 *
 * Time ticks are the number of 100th's of a second since some event.
 * For sysUpTime, this is the time ticks since the application started,
 * not since the host came up. We only support encoding ticks since we
 * started running (what we are calling 'starttime').
 *
 * Arguments:
 *	bufp	- pointer to buffer pointer
 *
 * Returns:
 *	bufp	- updated buffper pointer
 *	len	- number of bytes to encode time ticks value
 *		- ticks since 'starttime' encoded in  buffer
 *
 */
int
asn_encode_ticks ( bufp, ret )
	u_char	**bufp;
	int	*ret;
{
	struct	timeval	timenow;
	struct	timeval	timediff;
	u_char		*bp = *bufp;
	int		len, ticks;

	(void) gettimeofday ( &timenow, NULL );
	/*
	 * Adjust for subtraction 
	 */
	timenow.tv_sec--;
	timenow.tv_usec += 1000000;

	/*
	 * Compute time since 'starttime'
	 */
	timediff.tv_sec = timenow.tv_sec - starttime.tv_sec;
	timediff.tv_usec = timenow.tv_usec - starttime.tv_usec;

	/*
	 * Adjust difference timeval
	 */
	if ( timediff.tv_usec > 1000000 ) {
		timediff.tv_usec -= 1000000;
		timediff.tv_sec++;
	}

	/*
	 * Compute 100th's of second in diff time structure
	 */
	*ret = ticks = (timediff.tv_sec * 100) + (timediff.tv_usec / 10000);

	/*
	 * The rest of this is just plain gross. I'm sure there
	 * are better ways to do this...
	 */

	/* Compute time ticks length */
	if ( ticks < 0xFF )
		len = 1;
	else if ( ticks < 0xFFFF )
		len = 2;
	else if ( ticks < 0xFFFFFF )
		len = 3;
	else
		len = 4;

	/*
	 * Encode time ticks
	 */
	*bp++ = ASN_TIMESTAMP;		/* Time Ticks */
	*bp++ = len;			/* length of value */

	/* there's always a better way but this is quick and dirty... */
	if ( ticks > 0xFFFFFF ) {
		*bp++ = ( ticks & 0xFF000000 ) >> 24;
		ticks &= 0xFFFFFF;
	}
	if ( ticks > 0xFFFF ) {
		*bp++ = ( ticks & 0xFF0000 ) >> 16;
		ticks &= 0xFFFF;
	}
	if ( ticks > 0xFF ) {
		*bp++ = ( ticks & 0xFF00 ) >> 8;
		ticks &= 0xFF;
	}
	*bp++ = ticks;

	*bufp = bp;
	return ( len  + 2 );
}

/*
 * Send back up sysUpTime response
 *
 * Arguments:
 *	sd	- socket descriptor to send reply on
 *	reqid	- original GET request id
 *	
 * Returns:
 *	none	- response sent
 *
 */
void
send_uptime_resp ( sd, reqid )
	int		sd;
	int		reqid;
{
	int	len;
	short	*sp;
	u_long	*ip;
	u_char	*bp;
	short	val;
	int	ticks;

	COPY_RESP ( sysUpTime_Resp );

	bp = (u_char *)&Resp_Buf[Resp_Buf[0]+1];
	len = asn_encode_ticks ( &bp, &ticks );

	/*
	 * Adjust overall length
	 */
	bp = (u_char *)&Resp_Buf[0];
	*bp += len;

	/*
	 * Adjust sequence lengths - works because this is my
	 * PDU and I know all the variable lengths are fixed (ie.
	 * reqid is always 4 byte encoded).
	 */
#ifndef	sun
	sp = (short *)&Resp_Buf[3];
	val = ntohs ( *sp );
	*sp = htons ( val + len );
	Resp_Buf[15] += len;
	sp = (u_short *)&Resp_Buf[30];
	val = ntohs ( *sp );
	*sp = htons ( val + len );
	sp = (u_short *)&Resp_Buf[34];
	val = ntohs ( *sp );
	*sp = htons ( val + len );
#else
	/* Sun SPARCs have alignment requirements */
	Resp_Buf[4] += len;
	Resp_Buf[15] += len;
	Resp_Buf[31] += len;
	Resp_Buf[35] += len;
#endif	/* sun */

	/*
	 * Store the original request ID in the response
	 */
	set_reqid ( Resp_Buf, reqid );
#ifdef	notdef
#ifndef	sun
	ip = (u_long *)&Resp_Buf[18];
	*ip = htonl ( reqid );
#else
	/* Sun SPARCs have alignment requirements */
	UM_COPY ( (caddr_t)&reqid, (caddr_t)&Resp_Buf[18], sizeof(reqid) );
#endif	/* sun */
#endif

	if ( Debug_Level > 1 && Log ) {
		write_timestamp();
		fprintf ( Log, "\tSend sysUpTime: %d\n", ticks );
	}

	if ( Debug_Level > 4 && Log ) {
		write_timestamp();
		fprintf ( Log, "\n===== Sent %d bytes =====\n", Resp_Buf[0] );
		hexdump ( (u_char *)&Resp_Buf[1], Resp_Buf[0] );
	}
	/*
	 * Send response
	 */
	write ( sd, (caddr_t)&Resp_Buf[1], Resp_Buf[0] );

	return;

}

/*
 * Set Request ID in PDU
 *
 * Arguments:
 *	resp	- Response PDU buffer
 *	reqid	- request id value
 *
 * Returns:
 *	none	- request id may/may not be set
 *
 */
void
set_reqid ( resp, reqid )
	u_char	*resp;
	int	reqid;
{
	u_char		*bp = (u_char *)&resp[18];
	union {
		int	i;
		u_char	c[4];
	} u;	

#ifndef	sun
	u.i = htonl(reqid);
#else
	u.i = reqid;
#endif	/* !sun */

	/*
	 * Replace the current Request ID with the supplied value
	 */
	UM_COPY ( (caddr_t)&u.c[4-resp[17]], bp, resp[17] );

	return;

}

/*
 * Send a generic response packet
 *
 * Arguments:
 *	sd	- socket to send the reply on
 *	reqid	- original request ID from GET PDU
 *	resp	- pointer to the response to send
 *
 * Returns:
 *	none	- response sent
 *
 */
void
send_resp ( sd, reqid, resp )
	int		sd;
	int		reqid;
	u_char		*resp;
{

	set_reqid ( resp, reqid );

	if ( Debug_Level > 1 && Log ) {
		write_timestamp();
		fprintf ( Log, "===== Sent %d bytes =====\n", resp[0] );
		hexdump ( (u_char *)&resp[1], resp[0] );
	}
	write ( sd, (caddr_t)&resp[1], resp[0] );

	return;
}

/* 
 * Initialize information on what physical adapters HARP knows about
 *
 * Query the HARP subsystem about configuration and physical interface
 * information for any currently registered ATM adapters. Store the information
 * as arrays for easier indexing by SNMP port/index numbers.
 *      
 * Arguments:
 *      none
 *
 * Returns:
 *      none            Information from HARP available 
 *      
 */
void    
init_ilmi()  
{
        struct  air_cfg_rsp     *cfg_info = NULL;
        struct  air_intf_rsp    *intf_info = NULL;
        int                     buf_len;

	/*
	 * Get configuration info - what's available with 'atm sh config'
	 */
        buf_len = get_cfg_info ( NULL, &cfg_info );
	/*
	 * If error occurred, clear out everything
	 */
	if ( buf_len <= 0 ) {
		UM_ZERO ( Cfg, sizeof(Cfg) );
		UM_ZERO ( Intf, sizeof(Intf) );
		NUnits = 0;
		if ( Debug_Level > 1 && Log ) {
			write_timestamp();
			fprintf ( Log, "NUnits: %d\n", NUnits );
		}
		return;
	}

	/*
	 * Move to local storage
	 */
        UM_COPY ( cfg_info, (caddr_t)Cfg, buf_len );
	/*
	 * Compute how many units information was returned for
	 */
        NUnits = buf_len / sizeof(struct air_cfg_rsp);
	if ( Debug_Level > 1 && Log ) {
		write_timestamp();
		fprintf ( Log, "NUnits: %d\n", NUnits );
	}
	/* Housecleaning */
        free ( cfg_info );
        cfg_info = NULL;
	/*
	 * Get the per interface information
	 */
        buf_len = get_intf_info ( NULL, &intf_info );
	/*
	 * If error occurred, clear out Intf info
	 */
	if ( buf_len <= 0 ) {
		UM_ZERO ( Intf, sizeof(Intf) );
		return;
	}

	/*
	 * Move to local storage
	 */
        UM_COPY ( intf_info, (caddr_t)Intf, buf_len );
	/* Housecleaning */
        free ( intf_info );
        intf_info = NULL;

	return;

}

/*
 * Open a new SNMP session for ILMI
 *
 * Start by updating interface information, in particular, how many
 * interfaces are in the system. While we'll try to open sessons on
 * all interfaces, this deamon currently can only handle the first
 * interface.
 *
 * Arguments:
 *	none
 *
 * Returns:
 *      none
 *
 */
void
ilmi_open ()
{
        struct sockaddr_atm     satm;
        struct t_atm_aal5       aal5;
        struct t_atm_traffic    traffic;
        struct t_atm_bearer     bearer;
        struct t_atm_qos        qos;
	struct t_atm_app_name	appname;
        Atm_addr                subaddr;
        char                    buffer[MAX_LEN+1];
        char                    nifname[IFNAMSIZ];
        int                     optlen;
        int                     unit = 0;
        struct timer_elem       *open_timer,
                                *state_timer;
	u_char			sig_proto;

	if ( Debug_Level > 1 && Log ) {
		write_timestamp();
		fprintf ( Log, "ilmi_open()\n" );
	}
        init_ilmi();

	for ( unit = 0; unit < NUnits; unit++ ) {

	    if ( Debug_Level > 1 && Log ) {
		write_timestamp();
		fprintf ( Log, "Unit: %d Sig: %d Trap: %d Ilmi: %d\n",
		    unit, Intf[unit].anp_sig_proto, trap_fd[unit],
			ilmi_fd[unit] );
	    }
	    /*
	     * ILMI only makes sense for UNI signalling protocols
	     */
	    sig_proto = Intf[unit].anp_sig_proto;
	    if ( sig_proto != ATM_SIG_UNI30 && sig_proto != ATM_SIG_UNI31 &&
		sig_proto != ATM_SIG_UNI40 )
		    continue;

	    /*
	     * If we're waiting for a coldStart TRAP, we'll be in trap_fd[],
	     * If we're processing ILMI, we'll be in ilmi_fd[], otherwise,
	     * this unit hasn't been opened yet.
	     */
       	    if ( trap_fd[unit] == -1 && ilmi_fd[unit] == -1 ) {

       	        trap_fd[unit] = socket ( AF_ATM, SOCK_SEQPACKET, ATM_PROTO_AAL5 );

       	        if ( trap_fd[unit] < 0 ) {
               	    perror ( "open" );
               	    continue;
       	        }

                /*
                 * Set interface name. For now, we must have a netif to go on...
                 */
                if ( Intf[unit].anp_nif_cnt == 0 ) {
		    if ( Debug_Level > 1 && Log ) {
			write_timestamp();
			fprintf ( Log, "No nif on unit %d\n", unit );
		    }
               	    close ( trap_fd[unit] );
		    trap_fd[unit] = -1;
               	    ilmi_fd[unit] = -1;
               	    continue;
                }
                sprintf ( nifname, "%s0\0", Intf[unit].anp_nif_pref );
                optlen = sizeof ( nifname );
                if ( setsockopt ( trap_fd[unit], T_ATM_SIGNALING,
		    T_ATM_NET_INTF, (caddr_t)nifname, optlen ) < 0 ) {
                       	perror ( "setsockopt" );
			if ( Log ) {
			    write_timestamp();
                            fprintf ( Log,
				"Couldn't set interface name \"%s\"\n",
				    nifname );
			}
			if ( Debug_Level > 1 && Log ) {
			    write_timestamp();
			    fprintf ( Log, "nifname: closing unit %d\n", unit );
			}
                       	close ( trap_fd[unit] );
			trap_fd[unit] = -1;
                       	ilmi_fd[unit] = -1;
                       	continue;
                }

                /*
                 * Set up destination SAP
                 */
                UM_ZERO ( (caddr_t) &satm, sizeof(satm) );
                satm.satm_family = AF_ATM;
#ifndef sun
                satm.satm_len = sizeof(satm);
#endif  /* sun */

                satm.satm_addr.t_atm_sap_addr.SVE_tag_addr = T_ATM_PRESENT;
                satm.satm_addr.t_atm_sap_addr.SVE_tag_selector = T_ATM_ABSENT;
                satm.satm_addr.t_atm_sap_addr.address_format = T_ATM_PVC_ADDR;
                satm.satm_addr.t_atm_sap_addr.address_length = sizeof(Atm_addr_pvc);
                ATM_PVC_SET_VPI((Atm_addr_pvc *)satm.satm_addr.t_atm_sap_addr.address,
                    0 );
                ATM_PVC_SET_VCI((Atm_addr_pvc *)satm.satm_addr.t_atm_sap_addr.address,
                    16 );
    
                satm.satm_addr.t_atm_sap_layer2.SVE_tag = T_ATM_PRESENT;
                satm.satm_addr.t_atm_sap_layer2.ID_type = T_ATM_SIMPLE_ID;
                satm.satm_addr.t_atm_sap_layer2.ID.simple_ID = T_ATM_BLLI2_I8802;

                satm.satm_addr.t_atm_sap_layer3.SVE_tag = T_ATM_ABSENT;

                satm.satm_addr.t_atm_sap_appl.SVE_tag = T_ATM_ABSENT;

                /*
                 * Set up connection parameters
                 */
                aal5.forward_max_SDU_size = MAX_LEN;
                aal5.backward_max_SDU_size = MAX_LEN;
                aal5.SSCS_type = T_ATM_NULL;
                optlen = sizeof(aal5);
                if ( setsockopt ( trap_fd[unit], T_ATM_SIGNALING, T_ATM_AAL5,
                (caddr_t) &aal5, optlen ) < 0 ) {
                    perror ( "setsockopt(aal5)" );
		    if ( Debug_Level > 1 && Log ) {
			write_timestamp();
			fprintf ( Log, "aal5: closing unit %d\n", unit );
		    }
                    close ( trap_fd[unit] );
		    trap_fd[unit] = -1;
                    ilmi_fd[unit] = -1;
                    continue;
                }

                traffic.forward.PCR_high_priority = T_ATM_ABSENT;
                traffic.forward.PCR_all_traffic = 100000;
                traffic.forward.SCR_high_priority = T_ATM_ABSENT;
                traffic.forward.SCR_all_traffic = T_ATM_ABSENT;
                traffic.forward.MBS_high_priority = T_ATM_ABSENT;
                traffic.forward.MBS_all_traffic = T_ATM_ABSENT;
                traffic.forward.tagging = T_NO;
                traffic.backward.PCR_high_priority = T_ATM_ABSENT;
                traffic.backward.PCR_all_traffic = 100000;
                traffic.backward.SCR_high_priority = T_ATM_ABSENT;
                traffic.backward.SCR_all_traffic = T_ATM_ABSENT;
                traffic.backward.MBS_high_priority = T_ATM_ABSENT;
                traffic.backward.MBS_all_traffic = T_ATM_ABSENT;
                traffic.backward.tagging = T_NO;
                traffic.best_effort = T_YES;
                optlen = sizeof(traffic);
                if (setsockopt(trap_fd[unit], T_ATM_SIGNALING, T_ATM_TRAFFIC,
                        (caddr_t)&traffic, optlen) < 0) {
                    perror("setsockopt(traffic)");
                }
                bearer.bearer_class = T_ATM_CLASS_X;
                bearer.traffic_type = T_ATM_NULL;
                bearer.timing_requirements = T_ATM_NULL;
                bearer.clipping_susceptibility = T_NO;
                bearer.connection_configuration = T_ATM_1_TO_1;
                optlen = sizeof(bearer);
                if (setsockopt(trap_fd[unit], T_ATM_SIGNALING, T_ATM_BEARER_CAP,
                        (caddr_t)&bearer, optlen) < 0) {
                    perror("setsockopt(bearer)");
                }

                qos.coding_standard = T_ATM_NETWORK_CODING;
                qos.forward.qos_class = T_ATM_QOS_CLASS_0;
                qos.backward.qos_class = T_ATM_QOS_CLASS_0;
                optlen = sizeof(qos);
                if (setsockopt(trap_fd[unit], T_ATM_SIGNALING, T_ATM_QOS, (caddr_t)&qos,
                        optlen) < 0) {
                    perror("setsockopt(qos)");
                }

                subaddr.address_format = T_ATM_ABSENT;
                subaddr.address_length = 0;
                optlen = sizeof(subaddr);
                if (setsockopt(trap_fd[unit], T_ATM_SIGNALING, T_ATM_DEST_SUB,
                        (caddr_t)&subaddr, optlen) < 0) {
                    perror("setsockopt(dest_sub)");
                }

	        strncpy(appname.app_name, "ILMI", T_ATM_APP_NAME_LEN);
	        optlen = sizeof(appname);
	        if (setsockopt(trap_fd[unit], T_ATM_SIGNALING, T_ATM_APP_NAME,
			(caddr_t)&appname, optlen) < 0) {
		    perror("setsockopt(appname)");
	        }

                /*
                 * Now try to connect to destination
                 */
                if ( connect ( trap_fd[unit], (struct sockaddr *) &satm,
                    sizeof(satm)) < 0 ) {
                        perror ( "connect" );
		        if ( Debug_Level > 1 && Log ) {
			    write_timestamp();
			    fprintf ( Log, "connect: closing unit %d\n", unit );
			}
                        close ( trap_fd[unit] );
		        trap_fd[unit] = -1;
                        ilmi_fd[unit] = -1;
                        continue;
                }

    	        if ( Debug_Level && Log ) {
		    write_timestamp();
		    fprintf ( Log, "***** opened unit %d\n", unit );
		}
	        /*
	         * Send coldStart TRAP
	         */
	        if ( Debug_Level > 4 && Log ) {
		    write_timestamp();
		    fprintf ( Log, "===== Sent %d bytes =====\n",
			coldStart_Trap[0] );
		    hexdump ( (u_char *)&coldStart_Trap[1], coldStart_Trap[0] );
	        }
	        if ( Debug_Level && Log ) {
		    write_timestamp();
		    fprintf ( Log, "\tSend coldStart TRAP to unit %d\n", unit );
		}
	        last_trap[unit] = time ( (time_t *)NULL );
	        write ( trap_fd[unit], (caddr_t)&coldStart_Trap[1],
		    coldStart_Trap[0] );
	    }

	}

	signal ( SIGALRM, ilmi_open );
	alarm ( SLEEP_TIME );

	return;

}

/*
 * Send our local IP address for this interface
 *
 * Arguments:
 *	s	- socket to send message on
 *	hdr	- pointer to internal SNMP header
 *
 * Returns:
 *	none
 *
 */
void
send_myipnm ( s, hdr )
	int		s;
	Snmp_Header	*hdr;
{
	char	intf_name[IFNAMSIZ];
	int	namelen = IFNAMSIZ;
	struct air_netif_rsp *net_info = NULL;
	struct sockaddr_in	*sin;

	COPY_RESP ( MyIpNm_Resp );

	if ( getsockopt ( s, T_ATM_SIGNALING, T_ATM_NET_INTF,
		(caddr_t) intf_name, &namelen ) ) {
			perror ( "Couldn't get socket name" );
			return;
	}

	/*
	 * Get network interface information for this physical interface
	 */
	get_netif_info ( intf_name, &net_info );
	if ( net_info == NULL )
		return;

	sin = (struct sockaddr_in *)&net_info->anp_proto_addr;

	/*
	 * Copy interface's IP address into reply packet
	 */
	UM_COPY ( (caddr_t)&sin->sin_addr.s_addr, (caddr_t)&Resp_Buf[51],
		4 );

	if ( Debug_Level > 1 && Log ) {
		write_timestamp();
		fprintf ( Log, "\tSend NM IP address\n" );
	}

	send_resp ( s, hdr->reqid, Resp_Buf );

	/*
	 * Clean up
	 */
	free ( net_info );
	return;
}

/*
 * Set local NSAP prefix and then reply with our full NSAP address.
 *
 * Switch will send a SET message with the NSAP prefix after a coldStart.
 * We'll set that prefix into HARP and then send a SET message of our own
 * with our full interface NSAP address.
 *
 * Arguments:
 *	oid	- objid from SET message
 *	hdr	- pointer to internal SNMP header
 *	buf	- pointer to SET buffer
 *	s	- socket to send messages on
 *
 * Returns:
 *	none
 *
 */
void
set_prefix ( oid, hdr, buf, s )
	Objid		*oid;
	Snmp_Header	*hdr;
	u_char		*buf;
	int		s;
{
	struct atmsetreq asr;
	Atm_addr	*aa;
	int	fd;
	int	i;
	u_char	*cpp;
	int	len;			 /* PDU length before completion */

	/*
	 * If we don't reply to the SET then it keeps getting retransmitted.
	 */
	buf[14] = PDU_TYPE_GETRESP;
	if ( Debug_Level > 1 && Log ) {
		write_timestamp();
		fprintf ( Log, "\tSend SET_RESPONSE\n" );
	}
	send_resp ( s, hdr->reqid, buf );

	/*
	 * Build IOCTL request to set prefix
	 */
	asr.asr_opcode = AIOCS_SET_PRF;
	strncpy ( asr.asr_prf_intf, Intf[0].anp_intf,
		sizeof(asr.asr_prf_intf ) );
	/*
	 * Pull prefix out of received Objid
	 */
	for ( i = 0; i < oid->oid[13]; i++ )
		asr.asr_prf_pref[i] = oid->oid[i + 14];

	/*
	 * Pass new prefix to the HARP kernel
	 */
	fd = socket ( AF_ATM, SOCK_DGRAM, 0 );
	if ( fd < 0 ) 
		return;
	if ( ioctl ( fd, AIOCSET, (caddr_t)&asr ) < 0 ) {
		if ( errno != EALREADY ) {
		    syslog ( LOG_ERR, "ilmid: error setting prefix: %m" );
		    if ( Log ) {
			write_timestamp();
			fprintf ( Log, "ilmid: errno %d setting prefix\n",
			    errno );
		    }
		    return;
		}
	}
	close ( fd );

	/*
	 * Reload the cfg/intf info with newly set prefix
	 */
	init_ilmi();

	aa = &Intf[0].anp_addr;

	/*
	 * Finish building SET NSAP packet
	 */

	COPY_RESP ( NetPrefix_Resp );

	len = Resp_Buf[0];
	cpp = &Resp_Buf[len + 1];	/* Set to end of response buffer */
	len++;
	*cpp++ = aa->address_length;
	for ( i = 0; i < aa->address_length; i++ ) {
		u_char	c = ((u_char *)(aa->address))[i];

		if ( c > 127 ) {
			*cpp++ = ( c >> 7 ) | 0x80;
			len++;
			c &= 0x7f;
		}
		*cpp++ = c;
		len++;
	}
	/*
	 * Pack "set = 1" onto end
	 */
	*cpp++ = 0x02;
	*cpp++ = 0x01;
	*cpp++ = 0x01;
	len += 3;

	/*
	 * Go back and patch up lengths...
	 */
	Resp_Buf[0] = len;
	Resp_Buf[4] = (u_char)(len - 4);
	Resp_Buf[15] = (u_char)(len - 15);
	Resp_Buf[31] = (u_char)(len - 31);
	Resp_Buf[35] = (u_char)(len - 35);
	Resp_Buf[37] = (u_char)(len - 40);

	/*
	 * Set reqid
	 */
	set_reqid ( Resp_Buf, Req_ID++ );
	
	/*
	 * Send SET
	 */
	if ( Debug_Level > 2 && Log ) {
		write_timestamp();
		fprintf ( Log, "===== Send SET: %d bytes =====\n",
		    Resp_Buf[0] );
		hexdump ( (u_char *)&Resp_Buf[1], Resp_Buf[0] );
	}
	write ( s, (caddr_t)&Resp_Buf[1], Resp_Buf[0] );

	return;

}

Objid	oid;

/*
 * Parse an ASN_TYPE_SET pdu
 *
 * Crack apart the various pieces of a SET message. The OBJID being set is
 * left in oid which is compared and handled else where.
 *
 * Arguments:
 *	bp	- pointer to current location in PDU buffer
 *
 * Returns:
 *	bp	- updated buffer pointer
 *	0	- no error
 *	-1	- error in PDU
 *
 */
int
process_set ( bp )
	caddr_t *bp;
{
	caddr_t bufp = *bp;
	int	pdulen;
	int	b;

	if ( Debug_Level > 1 && Log ) {
		write_timestamp();
		fprintf ( Log, "SET:: " );
	}
	/*
	 * Should be SEQUENCE OF
	 */
	if ( *bufp++ != ASN_SEQUENCE ) {
		*bp = bufp;
		return ( -1 );
	}
	pdulen = asn_get_pdu_len ( &bufp );
	/*
	 * Should be SEQUENCE OF
	 */
	if ( *bufp++ != ASN_SEQUENCE ) {
		*bp = bufp;
		return ( -1 );
	}
	pdulen = asn_get_pdu_len ( &bufp );
	/*
	 * Should be OBJID
	 */
	if ( *bufp++ != ASN_OBJID ) {
		*bp = bufp;
		return ( -1 );
	}
	asn_get_objid ( &bufp, &oid );
	/*
	 * Should be <= value>
	 */
	switch ( *bufp++ ) {
	case ASN_INTEGER:
		b = asn_get_int ( &bufp );
		if ( Debug_Level > 5 && Log ) {
			write_timestamp();
			fprintf ( Log, "Value = %d\n", b );
		}
		break;
	case ASN_OBJID:
		break;
	}

	/*
	 * Return updated pointer
	 */
	*bp = bufp;

	return ( 0 );
}

int	specific_trap;
int	generic_trap;
int	trap_time;
u_char	trap_ip[5];
Objid	trap_oid;
Objid	extra_trap_oid;

/*
 * Parse an ASN_TYPE_TRAP pdu
 *
 * Crack apart the various pieces of a TRAP message. The information elements are
 * left in global space and used elsewhere if anyone cares (which they currently don't).
 *
 * Arguments:
 *	bp	- pointer to current location in PDU buffer
 *	sd	- socket descriptor pdu arrived on
 *
 * Returns:
 *	bp	- updated buffer pointer
 *	0	- no error
 *	-1	- error in PDU
 *
 */
int
process_trap ( bp, sd )
	caddr_t *bp;
	int	sd;
{
	caddr_t bufp = *bp;
	int	pdulen;
	int	i;

	if ( Debug_Level > 1 && Log ) {
	    write_timestamp();
	    fprintf ( Log, "TRAP:: " );
	}
	/*
	 * Should be pdulen
	 */
	pdulen = *bufp++;
	/*
	 * Should be OBJID
	 */
	if ( *bufp++ != ASN_OBJID ) {
		if ( Log )
		    fprintf ( Log, "\n" );
		*bp = bufp;
		return ( -1 );
	}
	asn_get_objid ( &bufp, &trap_oid );
	/*
	 * First oid coded as 40 * X + Y
	 */
	if ( Debug_Level > 5 && Log ) {
	    write_timestamp();
	    fprintf ( Log, "%d.%d", trap_oid.oid[1] / 40,
		trap_oid.oid[1] % 40 );
	    for ( i = 2; i <= trap_oid.oid[0]; i++ )
		fprintf ( Log, ".%d", trap_oid.oid[i] );
	    fprintf ( Log, "\n" );
	}
	/*
	 * Should be OCTET STRING
	 */
	if ( *bufp++ != ASN_IPADDR ) {
	    if ( Debug_Level > 5 && Log ) {
		write_timestamp();
		fprintf ( Log, "Expected IP ADDRESS\n" );
	    }
	    *bp = bufp;
	    return ( -1 );
	}
	asn_get_octet ( &bufp, trap_ip );
	if ( Debug_Level > 5 && Log) {
	    write_timestamp();
	    fprintf ( Log, "\tIP: %d.%d.%d.%d",
		trap_ip[0], trap_ip[1], trap_ip[2], trap_ip[3] );
	}
	/*
	 * Should be Generic Trap followed by Specific Trap
	 */
	if ( *bufp++ != ASN_INTEGER ) {
	    if ( Log )
		fprintf ( Log, "\n" );
	    *bp = bufp;
	    return ( -1 );
	}
	generic_trap = asn_get_int ( &bufp );
	if ( Debug_Level > 5 && Log ) {
	    fprintf ( Log, " Generic Trap: %s (%d)",
		Traps[generic_trap], generic_trap );
	}
	if ( *bufp++ != ASN_INTEGER ) {
	    if ( Log )
		fprintf ( Log, "\n" );
	    *bp = bufp;
	    return ( -1 );
	}
	specific_trap = asn_get_int ( &bufp );
	if ( Debug_Level > 5 && Log ) {
	    fprintf ( Log, " Specific Trap: 0x%x\n",
		specific_trap );
	}
	/*
	 * Should be TIMESTAMP
	 */
	if ( *bufp++ != ASN_TIMESTAMP ) {
	    if ( Log )
		fprintf ( Log, "\n" );
	    *bp = bufp;
	    return ( -1 );
	}
	trap_time = asn_get_int ( &bufp );
	if ( Debug_Level > 5 && Log ) {
	    write_timestamp();
	    fprintf ( Log, "\tTimestamp: %d seconds", trap_time );
	}
	/*
	 * Should be SEQUENCE OF
	 */
	if ( *bufp++ != ASN_SEQUENCE ) {
	    *bp = bufp;
	    return ( -1 );
	}
	pdulen = asn_get_pdu_len ( &bufp );
	/*
	 * Should be OBJID
	 */
	if ( *bufp++ != ASN_OBJID ) {
	    *bp = bufp;
	    return ( -1 );
	}
	asn_get_objid ( &bufp, &extra_trap_oid );
	if ( Debug_Level > 5 && Log ) {
	    write_timestamp();
	    fprintf ( Log, "\tExtra Objid: " );
	    fprintf ( Log, "%d.%d", extra_trap_oid.oid[1] / 40,
		extra_trap_oid.oid[1] % 40 );
	    for ( i = 2; i <= extra_trap_oid.oid[0]; i++ )
		fprintf ( Log, ".%d", extra_trap_oid.oid[i] );
	    fprintf ( Log, "\n" );
	}
	/*
	 * Whole thing ended with a NULL
	 */
	bufp++;
	bufp++;

	/*
	 * Return updated pointer
	 */
	*bp = bufp;

	if ( generic_trap == 0 ) {
		write ( sd, (caddr_t)&coldStart_Trap[1],
			coldStart_Trap[0] );
	}

	return ( 0 );

}

u_char	No_Such[] = { 37,
		0x30, 0x82, 0x00, 0x00,
		0x02, 0x01, 0x00,
		0x04, 0x04, 0x49, 0x4c, 0x4d, 0x49,
		PDU_TYPE_GETRESP,
		0x00,
		0x02, 0x04, 0x00, 0x00, 0x00, 0x00,
		0x02, 0x01, 0x02,
		0x02, 0x01, 0x01,
		0x30, 0x82, 0x00, 0x00,
		0x30, 0x82, 0x00, 0x00,
		0x06, 0x00
	};
void
send_no_such ( s, Hdr, op )
	int		s;
	Snmp_Header	*Hdr;
	Objid		*op;
{
	u_char		*cp, *cpp;
	int		len;
	int		i;

	len = No_Such[0];

	UM_COPY ( No_Such, Resp_Buf, len + 1 );

	cp = cpp = (u_char *)&Resp_Buf[len];

	/*
	 * Copy OID into response buffer
	 */
	*cp++ = op->oid[0];
	for ( i = 1; i <= op->oid[0]; i++ ) {
		u_int	c = op->oid[i];

		if ( c > 127 ) {
			*cp++ = ( c >> 7 ) | 0x80;
			len++;
			c &= 0x7f;
			/*
			 * Increment OID length
			 */
			*cpp += 1;
		}
		*cp++ = c;
		len++;
	}
	/*
	 * Finish off with a NULL
	 */
	*cp++ = 0x05;
	*cp++ = 0x00;
	len += 2;

	/*
	 * Patch up all the length locations
	 */
	Resp_Buf[0] = len;
	Resp_Buf[4] = len - 4;
	Resp_Buf[15] = len - 15;
	Resp_Buf[31] = len - 31;
	Resp_Buf[35] = len - 35;

	/*
	 * Send Response
	 */
	send_resp ( s, Hdr->reqid, Resp_Buf );

	return;
}

/* 
 * Utility to strip off any leading path information from a filename
 *      
 * Arguments:
 *      path            pathname to strip
 *      
 * Returns:
 *      fname           striped filename
 * 
 */     
char *
basename ( path )
        char *path;
{  
        char *fname;

        if ( ( fname = (char *)strrchr ( path, '/' ) ) != NULL )
                fname++;
        else
                fname = path;

        return ( fname );
}

/*
 * Increment Debug Level
 *
 * Catches SIGUSR1 signal and increments value of Debug_Level
 *
 * Arguments:
 *	sig	- signal number
 *
 * Returns:
 *	none	- Debug_Level incremented
 *
 */
void
Increment_DL ( sig )
	int	sig;
{
	Debug_Level++;
	if ( Debug_Level && Log == (FILE *)NULL )
	    if ( ( Log = fopen ( LOG_FILE, "a" ) ) == NULL ) 
		Log = NULL;
	    else
		setbuf ( Log, NULL );
	signal ( SIGUSR1, Increment_DL );
	alarm ( SLEEP_TIME );
	return;
}

/*
 * Decrement Debug Level
 *
 * Catches SIGUSR2 signal and decrements value of Debug_Level
 *
 * Arguments:
 *	sig	- signal number
 *
 * Returns:
 *	none	- Debug_Level decremented
 *
 */
void
Decrement_DL ( sig )
	int	sig;
{
	Debug_Level--;
	if ( Debug_Level <= 0 ) {
	    Debug_Level = 0;
	    if ( Log ) {
		fclose ( Log );
		Log = NULL;
	    }
	}
	signal ( SIGUSR2, Decrement_DL );
	alarm ( SLEEP_TIME );
	return;
}

main ( argc, argv )
	int	argc;
	char	*argv[];
{
	u_char	buf[256], set_buf[256];
	char	community[1024];
	u_char	*bufp;
	int	s;
	int	c;
	int	foregnd = 0;	/* run in the foreground? */
	int	pdulen;
	int	version;
	int	pdutype;
	int	reqid;
	int	error_status;
	int	error_ptr;
	int	b;
	int	i;
	int	lerr = 0;
	int	Reset = 0;	/* Should we send a coldStart and exit? */
	Snmp_Header	*Hdr;
	int	n;

	/*
	 * What are we running as? (argv[0])
	 */
	progname = strdup ( (char *)basename ( argv[0] ) );
	/*
	 * What host are we
	 */
	gethostname ( hostname, sizeof ( hostname ) );

	/*
	 * Ilmid needs to run as root to set prefix
	 */
	if ( getuid() != 0 ) {
		fprintf ( stderr, "%s: needs to run as root.\n", progname );
		exit ( -1 );
	}

	/*
	 * Parse arguments
	 */
	while ( ( c = getopt ( argc, argv, "d:fr" ) ) != EOF )
	    switch ( c ) {
		case 'd':
			Debug_Level = atoi ( optarg );
			break;
		case 'f':
			foregnd++;
			break;
		case 'r':
			Reset++;
			break;
		case '?':
			fprintf ( stderr, "usage: %s [-d level] [-f] [-r]\n",
				progname );
			exit ( -1 );
/* NOTREACHED */
			break;
	    }

	/*
	 * If we're not doing debugging, run in the background
	 */
	if ( foregnd == 0 ) {
#ifdef	sun
		int	pid, fd;

		if ( ( pid = fork() ) < 0 ) {
			fprintf ( stderr, "fork failed\n" );
			exit ( 1 );
		} else if (pid != 0) { 
			/* Parent process - exit and allow child to run */
			exit ( 0 );
		}
		/* Child process */
		if ( ( lerr = setpgrp ( 0, getpid() ) ) < 0 ) {
			fprintf ( stderr, "Can't set process group" );
			exit ( 1 );
		}
		if ( ( fd = open ( "/dev/tty", O_RDWR ) ) >= 0 ) {
			ioctl ( fd, TIOCNOTTY, (char *)NULL );
			close ( fd );
		}
		/* close all open descriptors */
		for ( fd = 3; fd < getdtablesize(); fd++ )
			close ( fd );
#else
		if ( daemon ( 0, 0 ) )
			err ( 1, "Can't fork" );
#endif
	} else
		setbuf ( stdout, NULL );

	signal ( SIGUSR1, Increment_DL );
	signal ( SIGUSR2, Decrement_DL );

	/*
	 * Open log file
	 */
	if ( Debug_Level )
	    if ( ( Log = fopen ( LOG_FILE, "a" ) ) == NULL )
		Log = NULL;
	    else
		setbuf ( Log, NULL );

	/*
	 * Get our startup time
	 */
	(void) gettimeofday ( &starttime, NULL );
	starttime.tv_sec--;
	starttime.tv_usec += 1000000;

	/*
	 * Reset all the interface descriptors
	 */
	for ( i = 0; i < MAX_UNITS; i++ ) {
		trap_fd[i] = -1;
		last_trap[i] = (time_t)0;
		ilmi_fd[i] = -1;
	}
	/*
	 * Try to open all the interfaces
	 */
	ilmi_open ();

	/*
	 * If we're just sending a coldStart end exiting...
	 */
	if ( Reset ) {
		for ( i = 0; i < MAX_UNITS; i++ )
			if ( trap_fd[i] >= 0 ) {
			    if ( Debug_Level > 1 && Log ) {
				write_timestamp();
				fprintf ( Log, "Close trap_fd[%d]: %d\n",
				    i, trap_fd[i] );
			    }
			    close ( trap_fd[i] );
			}
		exit ( 2 );
	}

	/*
	 * For ever...
	 */
	for ( ; ; ) {
	    int		maxfd = 0;
	    int		count;
	    struct timeval tvp;
	    fd_set	rfd;
	    time_t	curtime;

	    ilmi_open();

	    /*
	     * SunOS CC doesn't allow automatic aggregate initialization.
	     * Make everybody happy and do it here...
	     */
	    tvp.tv_sec = 15;
	    tvp.tv_usec = 0;

	    curtime = time ( (time_t *)NULL );

	    /*
	     * Check for TRAP messages
	     */
	    FD_ZERO ( &rfd );
	    if ( Debug_Level > 1 && Log ) {
		write_timestamp();
		fprintf ( Log, "Check Traps: " );
	    }
	    for ( i = 0; i < MAX_UNITS; i++ ) {
		if ( Debug_Level > 1 && Log )
			fprintf ( Log, "trap_fd[%d]: %d ", i, trap_fd[i] );
		if ( trap_fd[i] != -1 ) {
		    /*
		     * If we haven't sent a coldStart trap recently,
		     * send one now
		     */
		    if ( last_trap[i] + TRAP_TIME < curtime ) {
			last_trap[i] = curtime;
			/*
			 * Send coldStart TRAP
			 */
			if ( Debug_Level > 4 && Log ) {
			    write_timestamp();
			    fprintf ( Log, "===== Sent %d bytes =====\n",
				coldStart_Trap[0] );
			    hexdump ( (u_char *)&coldStart_Trap[1],
				coldStart_Trap[0] );
			}
			if ( Debug_Level && Log ) {
			    write_timestamp();
			    fprintf ( Log,
				"\tSend coldStart TRAP to unit %d\n", i );
			}
			write ( trap_fd[i], (caddr_t)&coldStart_Trap[1],
				coldStart_Trap[0] );
		    }
		    if ( (trap_fd[i] >= 0) &&
			FD_SET ( trap_fd[i], &rfd )) {
		   	    maxfd = MAX ( maxfd, trap_fd[i] );
		    }
		}
	    }
	    if ( Debug_Level > 1 && Log )
		fprintf ( Log, "maxfd: %d\n", maxfd );

	    if ( maxfd ) {
	      count = select ( maxfd + 1, &rfd, NULL, NULL, &tvp );

	      if ( count > 0 ) {
		for ( i = 0; i < MAX_UNITS; i++ ) {
		    if ( trap_fd[i] >= 0 && FD_ISSET ( trap_fd[i], &rfd ) ) {
			s = trap_fd[i];

			n = read ( s, (caddr_t)&buf[1], sizeof(buf) - 1 );
			if ( n == -1 && ( errno == ECONNRESET ||
			    errno == EBADF ) ) {
				if ( Debug_Level > 1 && Log ) {
				    write_timestamp();
				    fprintf ( Log,
					"Bad read: close trap_fd[%d]: %d\n",
					    i, trap_fd[i] );
				}
				close ( trap_fd[i] );
				trap_fd[i] = -1;
				ilmi_fd[i] = -1;
			}
			if ( n ) {
			    buf[0] = n;
			    if ( Debug_Level > 1 && Log ) {
				write_timestamp();
				fprintf ( Log, "***** Read %d bytes *****\n",
				    n );
				hexdump ( (caddr_t)&buf[1], n );
			    }
			    bufp = buf;
			    /*
			     * Skip length byte
			     */
			    bufp++;
			    /*
			     * Crack the header
			     */
			    if ( ( Hdr = asn_get_header ( &bufp ) ) == NULL )
				continue;
			    pdutype = Hdr->pdutype;
			    /*
			     * Only interested in TRAP messages
			     */
			    switch ( pdutype ) {
			    /*
			     * FORE switches often go straight to SET prefix
			     * after receiving a coldStart TRAP from us
			     */
			    case PDU_TYPE_SET:
				/*
				 * Make a copy of this PDU so that a
				 * SET NSAP prefix can reply to it.
				 */
				UM_COPY ( buf, set_buf, sizeof(buf) );
	
				lerr = process_set ( &bufp );
				/*
				 * Can't do a simple oid_cmp since we
				 * don't yet know what the prefix is.
				 * If it looks like a SET netPrefix.0,
				 * then compare the portion leading up
				 * to the NSAP prefix part.
				 */
				if ( oid.oid[0] == 26 ) {
				    oid.oid[0] = 12;
				    if ( oid_cmp ( &setprefix, &oid ) == 0 ) {
					oid.oid[0] = 26;
					set_prefix ( &oid, Hdr, set_buf, s );
				    }
				}
				/*
				 * We now move from awaiting TRAP to processing ILMI
				 */
				ilmi_fd[i] = trap_fd[i];
				trap_fd[i] = -1;
				break;
			    case PDU_TYPE_TRAP:
				lerr = process_trap ( &bufp, trap_fd[i] );
				/*
				 * We now move from awaiting TRAP to processing ILMI
				 */
				ilmi_fd[i] = trap_fd[i];
				trap_fd[i] = -1;
				break;
			    }
			} /* if n */
		    } /* if FD_ISSET */
		} /* for i */
	      } /* if count */
	    }

	    /*
	     * Reset from TRAP checking
	     */
	    maxfd = 0;
	    errno = 0;
	    /*
	     * Check for ILMI messages
	     */
	    FD_ZERO ( &rfd );
	    if ( Debug_Level > 1 && Log ) {
		write_timestamp();
		fprintf ( Log, "Check Ilmis: " );
	    }
	    for ( i = 0; i < MAX_UNITS; i++ ) {
		if ( Debug_Level > 1 && Log )
			fprintf ( Log, "ilmi_fd[%d]: %d ", i, ilmi_fd[i] );
		if ( ilmi_fd[i] != -1 ) {
		    if ( (ilmi_fd[i] >= 0) &&
			FD_SET ( ilmi_fd[i], &rfd )) {
		   	    maxfd = MAX ( maxfd, ilmi_fd[i] );
		    }
		}
	    }
	    if ( Debug_Level > 1 && Log )
		fprintf ( Log, "maxfd: %d\n", maxfd );

	    if ( maxfd ) {
	      count = select ( maxfd + 1, &rfd, NULL, NULL, &tvp );

	      if ( count > 0 ) {
		for ( i = 0; i < MAX_UNITS; i++ ) {
		    if ( ilmi_fd[i] >= 0 && FD_ISSET ( ilmi_fd[i], &rfd ) ) {

			s = ilmi_fd[i];

			n = read ( s, (caddr_t)&buf[1], sizeof(buf) - 1 );
			if ( n == -1 && ( errno == ECONNRESET ||
			    errno == EBADF ) ) {
				if ( Debug_Level > 1 && Log ) {
				    write_timestamp();
				    fprintf ( Log,
					"Bad read: close ilmi_fd[%d]: %d\n",
					    i, ilmi_fd[i] );
				}
				close ( ilmi_fd[i] );
				trap_fd[i] = -1;
				ilmi_fd[i] = -1;
			}
			if ( n ) {
				buf[0] = n;
				if ( Debug_Level > 1 && Log ) {
					write_timestamp();
					fprintf ( Log,
					    "***** Read %d bytes *****\n",
						n );
					hexdump ( (caddr_t)&buf[1], n );
				}
				bufp = buf;
				/*
				 * Skip length byte
				 */
				bufp++;
				/*
				 * Crack the header
	 			 */
				if ( ( Hdr = asn_get_header ( &bufp ) )
				    == NULL )
					continue;
				pdutype = Hdr->pdutype;

				/*
				 * Do the operation...
				 */
				switch ( pdutype ) {

				case PDU_TYPE_GET:
					if ( Debug_Level > 1 && Log ) {
						write_timestamp();
						fprintf ( Log, "GET:: " );
					}
					/*
					 * Should be SEQUENCE OF
					 */
					if ( *bufp++ != ASN_SEQUENCE ) {
						lerr = 1;
						break;
					}
					pdulen = asn_get_pdu_len ( &bufp );
					/*
					 * Should be SEQUENCE OF
					 */
					if ( *bufp++ != ASN_SEQUENCE ) {
						lerr = 1;
						break;
					}
					pdulen = asn_get_pdu_len ( &bufp );
					/*
					 * Should be OBJID
					 */
					if ( *bufp++ != ASN_OBJID ) {
						lerr = 1;
						break;
					}
					asn_get_objid ( &bufp, &oid );
					/*
					 * Ended with a NULL
					 */
					bufp++;
					bufp++;
					/*
					 * If GET sysObjId.0
					 */
					if (oid_cmp(&sysObjId, &oid) == 0 ) {
						send_resp ( s, Hdr->reqid,
							sysObjId_Resp );

					} else
					/*
					 * If GET sysUpTime.0
					 */
					if (oid_cmp(&sysUpTime, &oid) == 0 ) {
						send_uptime_resp ( s,
							Hdr->reqid );
					} else
					/*
					 * If GET myIpNm.0
					 */
					if ( oid_cmp ( &myipnm, &oid ) == 0 ) {
						send_myipnm ( s, Hdr );
					} else
					/*
					 * If GET uniType.0
					 */
					if ( oid_cmp ( &unitype, &oid ) == 0 ) {
					    if ( Debug_Level > 1 && Log ) {
						write_timestamp();
						fprintf ( Log,
						    "\tSend uniType\n" );
					    }
					    send_resp ( s, Hdr->reqid,
						UniType_Resp );
					} else
					/*
					 * If GET uniVer.0
					 */
					if ( oid_cmp ( &univer, &oid ) == 0 ) {
					    int p = UniVer_Resp[0];
					        if ( Debug_Level > 1 && Log ) {
						    write_timestamp();
						    fprintf ( Log,
						        "\tSend uniVer\n" );
						}
						switch (Intf[i].anp_sig_proto) {
						case ATM_SIG_UNI30:
							UniVer_Resp[p] =
								UNIVER_UNI30;
							break;
						case ATM_SIG_UNI31:
							UniVer_Resp[p] =
								UNIVER_UNI31;
							break;
						case ATM_SIG_UNI40:
							UniVer_Resp[p] =
								UNIVER_UNI40;
							break;
						}
						send_resp ( s, Hdr->reqid,
							UniVer_Resp );
					} else
					/*
					 * If GET devType.0
					 */
					if ( oid_cmp ( &devtype, &oid ) == 0 ) {
					    if ( Debug_Level > 1 && Log ) {
						write_timestamp();
						fprintf ( Log,
						    "\tSend devType\n" );
					     }
					     send_resp ( s, Hdr->reqid,
						DevType_Resp );
					} else
					/*
					 * If GET foreSigGrp....0
					 */
					if (oid_cmp(&foresiggrp, &oid) == 0) {
					    if ( Debug_Level > 1 && Log ) {
						write_timestamp();
						fprintf ( Log,
						    "\tSend noSuchVar\n" );
					    }
					    send_resp ( s, Hdr->reqid,
						NoSuchFore_Resp );
					} else
					if ( oid_cmp(&layeridx, &oid) == 0 ) {
					    if ( Debug_Level > 1 && Log ) {
						write_timestamp();
						fprintf ( Log,
						    "\t*** LayerIndex\n" );
					    }
					} else
					if ( oid_cmp(&maxvcc, &oid) == 0 ) {
						send_resp ( s, Hdr->reqid,
							maxVCC_Resp );
					} else
					if ( oid_cmp ( &portidx, &oid ) == 0 ) {
						int p = PortIndex_Resp[0];
						PortIndex_Resp[p] = i + 1;
						send_resp ( s, Hdr->reqid,
							PortIndex_Resp );
					} else
						send_no_such ( s, Hdr, &oid );
					break;
		
				case PDU_TYPE_GETNEXT:
					if ( Debug_Level > 1 && Log ) {
						write_timestamp();
						fprintf ( Log, "GET_NEXT:: " );
					}
					/*
					 * Should be SEQUENCE OF
					 */
					if ( *bufp++ != ASN_SEQUENCE ) {
						lerr = 1;
						break;
					}
					pdulen = asn_get_pdu_len ( &bufp );
					/*
					 * Should be SEQUENCE OF
					 */
					if ( *bufp++ != ASN_SEQUENCE ) {
						lerr = 1;
						break;
					}
					pdulen = asn_get_pdu_len ( &bufp );
					/*
					 * Should be OBJID
					 */
					if ( *bufp++ != ASN_OBJID ) {
						lerr = 1;
						break;
					}
					asn_get_objid ( &bufp, &oid );
					/*
					 * Ended with a NULL
					 */
					bufp++;
					bufp++;
					/*
					 * If this is a GET_NEXT netPrefix then
					 * the other side probably restarted
					 * and is looking for a table empty
					 * indication before restarting the
					 * ILMI protocol.
					 */
					if ( oid_cmp(&netpfx_oid, &oid) == 0 ) {
					    if ( Debug_Level > 1 && Log ) {
						write_timestamp();
						fprintf ( Log,
						    "\tSend GET_RESP:\n" );
					    }
					    send_resp ( s, Hdr->reqid,
						GetNext_Resp );
					}
					break;
		
				case PDU_TYPE_GETRESP:
					if ( Debug_Level > 1 && Log ) {
						write_timestamp();
						fprintf ( Log, 
						    "GET_RESP:: \n" );
					}
					/*
					 * Ignore any responses to our GETs.
					 * (We don't send any GETs.)
					 */
					break;
		
				case PDU_TYPE_SET:
					/*
					 * Make a copy of this PDU so that a
					 * SET NSAP prefix can reply to it.
					 */
					UM_COPY ( buf, set_buf, sizeof(buf) );
	
					if ( process_set ( &bufp ) < 0 )
						break;

					/*
					 * Can't do a simple oid_cmp since we
					 * don't know what the prefix is yet.
					 * If it looks like a SET netPrefix.0,
					 * then compare the portion leading up
					 * to the NSAP prefix part.
					 */
					if ( oid.oid[0] == 26 ) {
						oid.oid[0] = 12;
						if ( oid_cmp(&setprefix,&oid)
						    == 0 ) {
							oid.oid[0] = 26;
							set_prefix ( &oid, Hdr,
								set_buf, s );
						}
					}
					break;
		
				case PDU_TYPE_TRAP:
					lerr = process_trap ( &bufp, s );
					break;
				}
				/*
				 * Forget about this PDU
				 */
				free ( Hdr );
				Hdr = NULL;

			} /* end of read(s) */
		    } /* end if FD_ISSET(s) */
		} /* end of for ( i... */
	  } /* end of if ( count ) */
	} else {
	    sleep ( SLEEP_TIME );
	}
    } /* end of for ever */
	
}

