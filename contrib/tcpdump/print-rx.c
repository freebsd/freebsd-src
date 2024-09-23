/*
 * Copyright: (c) 2000 United States Government as represented by the
 *	Secretary of the Navy. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in
 *      the documentation and/or other materials provided with the
 *      distribution.
 *   3. The names of the authors may not be used to endorse or promote
 *      products derived from this software without specific prior
 *      written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

/* \summary: AFS RX printer */

/*
 * This code unmangles RX packets.  RX is the mutant form of RPC that AFS
 * uses to communicate between clients and servers.
 *
 * In this code, I mainly concern myself with decoding the AFS calls, not
 * with the guts of RX, per se.
 *
 * Bah.  If I never look at rx_packet.h again, it will be too soon.
 *
 * Ken Hornstein <kenh@cmf.nrl.navy.mil>
 */

#include <config.h>

#include <stdio.h>
#include <string.h>
#include "netdissect-stdinc.h"

#include "netdissect.h"
#include "addrtoname.h"
#include "extract.h"

#include "ip.h"

#define FS_RX_PORT	7000
#define CB_RX_PORT	7001
#define PROT_RX_PORT	7002
#define VLDB_RX_PORT	7003
#define KAUTH_RX_PORT	7004
#define VOL_RX_PORT	7005
#define ERROR_RX_PORT	7006		/* Doesn't seem to be used */
#define BOS_RX_PORT	7007

#define AFSOPAQUEMAX 1024
#define AFSNAMEMAX 256			/* Must be >= PRNAMEMAX + 1, VLNAMEMAX + 1, and 32 + 1 */
#define PRNAMEMAX 64
#define VLNAMEMAX 65
#define KANAMEMAX 64
#define BOSNAMEMAX 256
#define USERNAMEMAX 1024		/* AFSOPAQUEMAX was used for this; does it need to be this big? */

#define	PRSFS_READ		1 /* Read files */
#define	PRSFS_WRITE		2 /* Write files */
#define	PRSFS_INSERT		4 /* Insert files into a directory */
#define	PRSFS_LOOKUP		8 /* Lookup files into a directory */
#define	PRSFS_DELETE		16 /* Delete files */
#define	PRSFS_LOCK		32 /* Lock files */
#define	PRSFS_ADMINISTER	64 /* Change ACL's */

struct rx_header {
	nd_uint32_t epoch;
	nd_uint32_t cid;
	nd_uint32_t callNumber;
	nd_uint32_t seq;
	nd_uint32_t serial;
	nd_uint8_t type;
#define RX_PACKET_TYPE_DATA		1
#define RX_PACKET_TYPE_ACK		2
#define RX_PACKET_TYPE_BUSY		3
#define RX_PACKET_TYPE_ABORT		4
#define RX_PACKET_TYPE_ACKALL		5
#define RX_PACKET_TYPE_CHALLENGE	6
#define RX_PACKET_TYPE_RESPONSE		7
#define RX_PACKET_TYPE_DEBUG		8
#define RX_PACKET_TYPE_PARAMS		9
#define RX_PACKET_TYPE_VERSION		13
	nd_uint8_t flags;
#define RX_CLIENT_INITIATED	1
#define RX_REQUEST_ACK		2
#define RX_LAST_PACKET		4
#define RX_MORE_PACKETS		8
#define RX_FREE_PACKET		16
#define RX_SLOW_START_OK	32
#define RX_JUMBO_PACKET		32
	nd_uint8_t userStatus;
	nd_uint8_t securityIndex;
	nd_uint16_t spare;		/* How clever: even though the AFS */
	nd_uint16_t serviceId;		/* header files indicate that the */
};					/* serviceId is first, it's really */
					/* encoded _after_ the spare field */
					/* I wasted a day figuring that out! */

#define NUM_RX_FLAGS 7

#define RX_MAXACKS 255

struct rx_ackPacket {
	nd_uint16_t bufferSpace;	/* Number of packet buffers available */
	nd_uint16_t maxSkew;		/* Max diff between ack'd packet and */
					/* highest packet received */
	nd_uint32_t firstPacket;	/* The first packet in ack list */
	nd_uint32_t previousPacket;	/* Previous packet recv'd (obsolete) */
	nd_uint32_t serial;		/* # of packet that prompted the ack */
	nd_uint8_t reason;		/* Reason for acknowledgement */
	nd_uint8_t nAcks;		/* Number of acknowledgements */
	/* Followed by nAcks acknowledgments */
#if 0
	uint8_t acks[RX_MAXACKS];	/* Up to RX_MAXACKS acknowledgements */
#endif
};

/*
 * Values for the acks array
 */

#define RX_ACK_TYPE_NACK	0	/* Don't have this packet */
#define RX_ACK_TYPE_ACK		1	/* I have this packet */

static const struct tok rx_types[] = {
	{ RX_PACKET_TYPE_DATA,		"data" },
	{ RX_PACKET_TYPE_ACK,		"ack" },
	{ RX_PACKET_TYPE_BUSY,		"busy" },
	{ RX_PACKET_TYPE_ABORT,		"abort" },
	{ RX_PACKET_TYPE_ACKALL,	"ackall" },
	{ RX_PACKET_TYPE_CHALLENGE,	"challenge" },
	{ RX_PACKET_TYPE_RESPONSE,	"response" },
	{ RX_PACKET_TYPE_DEBUG,		"debug" },
	{ RX_PACKET_TYPE_PARAMS,	"params" },
	{ RX_PACKET_TYPE_VERSION,	"version" },
	{ 0,				NULL },
};

static const struct double_tok {
	uint32_t flag;		/* Rx flag */
	uint32_t packetType;	/* Packet type */
	const char *s;		/* Flag string */
} rx_flags[] = {
	{ RX_CLIENT_INITIATED,	0,			"client-init" },
	{ RX_REQUEST_ACK,	0,			"req-ack" },
	{ RX_LAST_PACKET,	0,			"last-pckt" },
	{ RX_MORE_PACKETS,	0,			"more-pckts" },
	{ RX_FREE_PACKET,	0,			"free-pckt" },
	{ RX_SLOW_START_OK,	RX_PACKET_TYPE_ACK,	"slow-start" },
	{ RX_JUMBO_PACKET,	RX_PACKET_TYPE_DATA,	"jumbogram" }
};

static const struct tok fs_req[] = {
	{ 130,		"fetch-data" },
	{ 131,		"fetch-acl" },
	{ 132,		"fetch-status" },
	{ 133,		"store-data" },
	{ 134,		"store-acl" },
	{ 135,		"store-status" },
	{ 136,		"remove-file" },
	{ 137,		"create-file" },
	{ 138,		"rename" },
	{ 139,		"symlink" },
	{ 140,		"link" },
	{ 141,		"makedir" },
	{ 142,		"rmdir" },
	{ 143,		"oldsetlock" },
	{ 144,		"oldextlock" },
	{ 145,		"oldrellock" },
	{ 146,		"get-stats" },
	{ 147,		"give-cbs" },
	{ 148,		"get-vlinfo" },
	{ 149,		"get-vlstats" },
	{ 150,		"set-vlstats" },
	{ 151,		"get-rootvl" },
	{ 152,		"check-token" },
	{ 153,		"get-time" },
	{ 154,		"nget-vlinfo" },
	{ 155,		"bulk-stat" },
	{ 156,		"setlock" },
	{ 157,		"extlock" },
	{ 158,		"rellock" },
	{ 159,		"xstat-ver" },
	{ 160,		"get-xstat" },
	{ 161,		"dfs-lookup" },
	{ 162,		"dfs-flushcps" },
	{ 163,		"dfs-symlink" },
	{ 220,		"residency" },
	{ 65536,        "inline-bulk-status" },
	{ 65537,        "fetch-data-64" },
	{ 65538,        "store-data-64" },
	{ 65539,        "give-up-all-cbs" },
	{ 65540,        "get-caps" },
	{ 65541,        "cb-rx-conn-addr" },
	{ 0,		NULL },
};

static const struct tok cb_req[] = {
	{ 204,		"callback" },
	{ 205,		"initcb" },
	{ 206,		"probe" },
	{ 207,		"getlock" },
	{ 208,		"getce" },
	{ 209,		"xstatver" },
	{ 210,		"getxstat" },
	{ 211,		"initcb2" },
	{ 212,		"whoareyou" },
	{ 213,		"initcb3" },
	{ 214,		"probeuuid" },
	{ 215,		"getsrvprefs" },
	{ 216,		"getcellservdb" },
	{ 217,		"getlocalcell" },
	{ 218,		"getcacheconf" },
	{ 65536,        "getce64" },
	{ 65537,        "getcellbynum" },
	{ 65538,        "tellmeaboutyourself" },
	{ 0,		NULL },
};

static const struct tok pt_req[] = {
	{ 500,		"new-user" },
	{ 501,		"where-is-it" },
	{ 502,		"dump-entry" },
	{ 503,		"add-to-group" },
	{ 504,		"name-to-id" },
	{ 505,		"id-to-name" },
	{ 506,		"delete" },
	{ 507,		"remove-from-group" },
	{ 508,		"get-cps" },
	{ 509,		"new-entry" },
	{ 510,		"list-max" },
	{ 511,		"set-max" },
	{ 512,		"list-entry" },
	{ 513,		"change-entry" },
	{ 514,		"list-elements" },
	{ 515,		"same-mbr-of" },
	{ 516,		"set-fld-sentry" },
	{ 517,		"list-owned" },
	{ 518,		"get-cps2" },
	{ 519,		"get-host-cps" },
	{ 520,		"update-entry" },
	{ 521,		"list-entries" },
	{ 530,		"list-super-groups" },
	{ 0,		NULL },
};

static const struct tok vldb_req[] = {
	{ 501,		"create-entry" },
	{ 502,		"delete-entry" },
	{ 503,		"get-entry-by-id" },
	{ 504,		"get-entry-by-name" },
	{ 505,		"get-new-volume-id" },
	{ 506,		"replace-entry" },
	{ 507,		"update-entry" },
	{ 508,		"setlock" },
	{ 509,		"releaselock" },
	{ 510,		"list-entry" },
	{ 511,		"list-attrib" },
	{ 512,		"linked-list" },
	{ 513,		"get-stats" },
	{ 514,		"probe" },
	{ 515,		"get-addrs" },
	{ 516,		"change-addr" },
	{ 517,		"create-entry-n" },
	{ 518,		"get-entry-by-id-n" },
	{ 519,		"get-entry-by-name-n" },
	{ 520,		"replace-entry-n" },
	{ 521,		"list-entry-n" },
	{ 522,		"list-attrib-n" },
	{ 523,		"linked-list-n" },
	{ 524,		"update-entry-by-name" },
	{ 525,		"create-entry-u" },
	{ 526,		"get-entry-by-id-u" },
	{ 527,		"get-entry-by-name-u" },
	{ 528,		"replace-entry-u" },
	{ 529,		"list-entry-u" },
	{ 530,		"list-attrib-u" },
	{ 531,		"linked-list-u" },
	{ 532,		"regaddr" },
	{ 533,		"get-addrs-u" },
	{ 534,		"list-attrib-n2" },
	{ 0,		NULL },
};

static const struct tok kauth_req[] = {
	{ 1,		"auth-old" },
	{ 21,		"authenticate" },
	{ 22,		"authenticate-v2" },
	{ 2,		"change-pw" },
	{ 3,		"get-ticket-old" },
	{ 23,		"get-ticket" },
	{ 4,		"set-pw" },
	{ 5,		"set-fields" },
	{ 6,		"create-user" },
	{ 7,		"delete-user" },
	{ 8,		"get-entry" },
	{ 9,		"list-entry" },
	{ 10,		"get-stats" },
	{ 11,		"debug" },
	{ 12,		"get-pw" },
	{ 13,		"get-random-key" },
	{ 14,		"unlock" },
	{ 15,		"lock-status" },
	{ 0,		NULL },
};

static const struct tok vol_req[] = {
	{ 100,		"create-volume" },
	{ 101,		"delete-volume" },
	{ 102,		"restore" },
	{ 103,		"forward" },
	{ 104,		"end-trans" },
	{ 105,		"clone" },
	{ 106,		"set-flags" },
	{ 107,		"get-flags" },
	{ 108,		"trans-create" },
	{ 109,		"dump" },
	{ 110,		"get-nth-volume" },
	{ 111,		"set-forwarding" },
	{ 112,		"get-name" },
	{ 113,		"get-status" },
	{ 114,		"sig-restore" },
	{ 115,		"list-partitions" },
	{ 116,		"list-volumes" },
	{ 117,		"set-id-types" },
	{ 118,		"monitor" },
	{ 119,		"partition-info" },
	{ 120,		"reclone" },
	{ 121,		"list-one-volume" },
	{ 122,		"nuke" },
	{ 123,		"set-date" },
	{ 124,		"x-list-volumes" },
	{ 125,		"x-list-one-volume" },
	{ 126,		"set-info" },
	{ 127,		"x-list-partitions" },
	{ 128,		"forward-multiple" },
	{ 65536,	"convert-ro" },
	{ 65537,	"get-size" },
	{ 65538,	"dump-v2" },
	{ 0,		NULL },
};

static const struct tok bos_req[] = {
	{ 80,		"create-bnode" },
	{ 81,		"delete-bnode" },
	{ 82,		"set-status" },
	{ 83,		"get-status" },
	{ 84,		"enumerate-instance" },
	{ 85,		"get-instance-info" },
	{ 86,		"get-instance-parm" },
	{ 87,		"add-superuser" },
	{ 88,		"delete-superuser" },
	{ 89,		"list-superusers" },
	{ 90,		"list-keys" },
	{ 91,		"add-key" },
	{ 92,		"delete-key" },
	{ 93,		"set-cell-name" },
	{ 94,		"get-cell-name" },
	{ 95,		"get-cell-host" },
	{ 96,		"add-cell-host" },
	{ 97,		"delete-cell-host" },
	{ 98,		"set-t-status" },
	{ 99,		"shutdown-all" },
	{ 100,		"restart-all" },
	{ 101,		"startup-all" },
	{ 102,		"set-noauth-flag" },
	{ 103,		"re-bozo" },
	{ 104,		"restart" },
	{ 105,		"start-bozo-install" },
	{ 106,		"uninstall" },
	{ 107,		"get-dates" },
	{ 108,		"exec" },
	{ 109,		"prune" },
	{ 110,		"set-restart-time" },
	{ 111,		"get-restart-time" },
	{ 112,		"start-bozo-log" },
	{ 113,		"wait-all" },
	{ 114,		"get-instance-strings" },
	{ 115,		"get-restricted" },
	{ 116,		"set-restricted" },
	{ 0,		NULL },
};

static const struct tok ubik_req[] = {
	{ 10000,	"vote-beacon" },
	{ 10001,	"vote-debug-old" },
	{ 10002,	"vote-sdebug-old" },
	{ 10003,	"vote-getsyncsite" },
	{ 10004,	"vote-debug" },
	{ 10005,	"vote-sdebug" },
	{ 10006,	"vote-xdebug" },
	{ 10007,	"vote-xsdebug" },
	{ 20000,	"disk-begin" },
	{ 20001,	"disk-commit" },
	{ 20002,	"disk-lock" },
	{ 20003,	"disk-write" },
	{ 20004,	"disk-getversion" },
	{ 20005,	"disk-getfile" },
	{ 20006,	"disk-sendfile" },
	{ 20007,	"disk-abort" },
	{ 20008,	"disk-releaselocks" },
	{ 20009,	"disk-truncate" },
	{ 20010,	"disk-probe" },
	{ 20011,	"disk-writev" },
	{ 20012,	"disk-interfaceaddr" },
	{ 20013,	"disk-setversion" },
	{ 0,		NULL },
};

#define VOTE_LOW	10000
#define VOTE_HIGH	10007
#define DISK_LOW	20000
#define DISK_HIGH	20013

static const struct tok cb_types[] = {
	{ 1,		"exclusive" },
	{ 2,		"shared" },
	{ 3,		"dropped" },
	{ 0,		NULL },
};

static const struct tok ubik_lock_types[] = {
	{ 1,		"read" },
	{ 2,		"write" },
	{ 3,		"wait" },
	{ 0,		NULL },
};

static const char *voltype[] = { "read-write", "read-only", "backup" };

static const struct tok afs_fs_errors[] = {
	{ 101,		"salvage volume" },
	{ 102,		"no such vnode" },
	{ 103,		"no such volume" },
	{ 104,		"volume exist" },
	{ 105,		"no service" },
	{ 106,		"volume offline" },
	{ 107,		"voline online" },
	{ 108,		"diskfull" },
	{ 109,		"diskquota exceeded" },
	{ 110,		"volume busy" },
	{ 111,		"volume moved" },
	{ 112,		"AFS IO error" },
	{ 0xffffff9c,	"restarting fileserver" }, /* -100, sic! */
	{ 0,		NULL }
};

/*
 * Reasons for acknowledging a packet
 */

static const struct tok rx_ack_reasons[] = {
	{ 1,		"ack requested" },
	{ 2,		"duplicate packet" },
	{ 3,		"out of sequence" },
	{ 4,		"exceeds window" },
	{ 5,		"no buffer space" },
	{ 6,		"ping" },
	{ 7,		"ping response" },
	{ 8,		"delay" },
	{ 9,		"idle" },
	{ 0,		NULL },
};

/*
 * Cache entries we keep around so we can figure out the RX opcode
 * numbers for replies.  This allows us to make sense of RX reply packets.
 */

struct rx_cache_entry {
	uint32_t	callnum;	/* Call number (net order) */
	uint32_t	client;		/* client IP address (net order) */
	uint32_t	server;		/* server IP address (net order) */
	uint16_t	dport;		/* server UDP port (host order) */
	uint16_t	serviceId;	/* Service identifier (net order) */
	uint32_t	opcode;		/* RX opcode (host order) */
};

#define RX_CACHE_SIZE	64

static struct rx_cache_entry	rx_cache[RX_CACHE_SIZE];

static uint32_t	rx_cache_next = 0;
static uint32_t	rx_cache_hint = 0;
static void	rx_cache_insert(netdissect_options *, const u_char *, const struct ip *, uint16_t);
static int	rx_cache_find(netdissect_options *, const struct rx_header *,
			      const struct ip *, uint16_t, uint32_t *);

static void fs_print(netdissect_options *, const u_char *, u_int);
static void fs_reply_print(netdissect_options *, const u_char *, u_int, uint32_t);
static void acl_print(netdissect_options *, u_char *, const u_char *);
static void cb_print(netdissect_options *, const u_char *, u_int);
static void cb_reply_print(netdissect_options *, const u_char *, u_int, uint32_t);
static void prot_print(netdissect_options *, const u_char *, u_int);
static void prot_reply_print(netdissect_options *, const u_char *, u_int, uint32_t);
static void vldb_print(netdissect_options *, const u_char *, u_int);
static void vldb_reply_print(netdissect_options *, const u_char *, u_int, uint32_t);
static void kauth_print(netdissect_options *, const u_char *, u_int);
static void kauth_reply_print(netdissect_options *, const u_char *, u_int, uint32_t);
static void vol_print(netdissect_options *, const u_char *, u_int);
static void vol_reply_print(netdissect_options *, const u_char *, u_int, uint32_t);
static void bos_print(netdissect_options *, const u_char *, u_int);
static void bos_reply_print(netdissect_options *, const u_char *, u_int, uint32_t);
static void ubik_print(netdissect_options *, const u_char *);
static void ubik_reply_print(netdissect_options *, const u_char *, u_int, uint32_t);

static void rx_ack_print(netdissect_options *, const u_char *, u_int);

static int is_ubik(uint32_t);

/*
 * Handle the rx-level packet.  See if we know what port it's going to so
 * we can peek at the afs call inside
 */

void
rx_print(netdissect_options *ndo,
         const u_char *bp, u_int length, uint16_t sport, uint16_t dport,
         const u_char *bp2)
{
	const struct rx_header *rxh;
	uint32_t i;
	uint8_t type, flags;
	uint32_t opcode;

	ndo->ndo_protocol = "rx";
	if (!ND_TTEST_LEN(bp, sizeof(struct rx_header))) {
		ND_PRINT(" [|rx] (%u)", length);
		return;
	}

	rxh = (const struct rx_header *) bp;

	type = GET_U_1(rxh->type);
	ND_PRINT(" rx %s", tok2str(rx_types, "type %u", type));

	flags = GET_U_1(rxh->flags);
	if (ndo->ndo_vflag) {
		int firstflag = 0;

		if (ndo->ndo_vflag > 1)
			ND_PRINT(" cid %08x call# %u",
			       GET_BE_U_4(rxh->cid),
			       GET_BE_U_4(rxh->callNumber));

		ND_PRINT(" seq %u ser %u",
		       GET_BE_U_4(rxh->seq),
		       GET_BE_U_4(rxh->serial));

		if (ndo->ndo_vflag > 2)
			ND_PRINT(" secindex %u serviceid %hu",
				GET_U_1(rxh->securityIndex),
				GET_BE_U_2(rxh->serviceId));

		if (ndo->ndo_vflag > 1)
			for (i = 0; i < NUM_RX_FLAGS; i++) {
				if (flags & rx_flags[i].flag &&
				    (!rx_flags[i].packetType ||
				     type == rx_flags[i].packetType)) {
					if (!firstflag) {
						firstflag = 1;
						ND_PRINT(" ");
					} else {
						ND_PRINT(",");
					}
					ND_PRINT("<%s>", rx_flags[i].s);
				}
			}
	}

	/*
	 * Try to handle AFS calls that we know about.  Check the destination
	 * port and make sure it's a data packet.  Also, make sure the
	 * seq number is 1 (because otherwise it's a continuation packet,
	 * and we can't interpret that).  Also, seems that reply packets
	 * do not have the client-init flag set, so we check for that
	 * as well.
	 */

	if (type == RX_PACKET_TYPE_DATA &&
	    GET_BE_U_4(rxh->seq) == 1 &&
	    flags & RX_CLIENT_INITIATED) {

		/*
		 * Insert this call into the call cache table, so we
		 * have a chance to print out replies
		 */

		rx_cache_insert(ndo, bp, (const struct ip *) bp2, dport);

		switch (dport) {
			case FS_RX_PORT:	/* AFS file service */
				fs_print(ndo, bp, length);
				break;
			case CB_RX_PORT:	/* AFS callback service */
				cb_print(ndo, bp, length);
				break;
			case PROT_RX_PORT:	/* AFS protection service */
				prot_print(ndo, bp, length);
				break;
			case VLDB_RX_PORT:	/* AFS VLDB service */
				vldb_print(ndo, bp, length);
				break;
			case KAUTH_RX_PORT:	/* AFS Kerberos auth service */
				kauth_print(ndo, bp, length);
				break;
			case VOL_RX_PORT:	/* AFS Volume service */
				vol_print(ndo, bp, length);
				break;
			case BOS_RX_PORT:	/* AFS BOS service */
				bos_print(ndo, bp, length);
				break;
			default:
				;
		}

	/*
	 * If it's a reply (client-init is _not_ set, but seq is one)
	 * then look it up in the cache.  If we find it, call the reply
	 * printing functions  Note that we handle abort packets here,
	 * because printing out the return code can be useful at times.
	 */

	} else if (((type == RX_PACKET_TYPE_DATA &&
					GET_BE_U_4(rxh->seq) == 1) ||
		    type == RX_PACKET_TYPE_ABORT) &&
		   (flags & RX_CLIENT_INITIATED) == 0 &&
		   rx_cache_find(ndo, rxh, (const struct ip *) bp2,
				 sport, &opcode)) {

		switch (sport) {
			case FS_RX_PORT:	/* AFS file service */
				fs_reply_print(ndo, bp, length, opcode);
				break;
			case CB_RX_PORT:	/* AFS callback service */
				cb_reply_print(ndo, bp, length, opcode);
				break;
			case PROT_RX_PORT:	/* AFS PT service */
				prot_reply_print(ndo, bp, length, opcode);
				break;
			case VLDB_RX_PORT:	/* AFS VLDB service */
				vldb_reply_print(ndo, bp, length, opcode);
				break;
			case KAUTH_RX_PORT:	/* AFS Kerberos auth service */
				kauth_reply_print(ndo, bp, length, opcode);
				break;
			case VOL_RX_PORT:	/* AFS Volume service */
				vol_reply_print(ndo, bp, length, opcode);
				break;
			case BOS_RX_PORT:	/* AFS BOS service */
				bos_reply_print(ndo, bp, length, opcode);
				break;
			default:
				;
		}

	/*
	 * If it's an RX ack packet, then use the appropriate ack decoding
	 * function (there isn't any service-specific information in the
	 * ack packet, so we can use one for all AFS services)
	 */

	} else if (type == RX_PACKET_TYPE_ACK)
		rx_ack_print(ndo, bp, length);


	ND_PRINT(" (%u)", length);
}

/*
 * Insert an entry into the cache.  Taken from print-nfs.c
 */

static void
rx_cache_insert(netdissect_options *ndo,
                const u_char *bp, const struct ip *ip, uint16_t dport)
{
	struct rx_cache_entry *rxent;
	const struct rx_header *rxh = (const struct rx_header *) bp;

	if (!ND_TTEST_4(bp + sizeof(struct rx_header)))
		return;

	rxent = &rx_cache[rx_cache_next];

	if (++rx_cache_next >= RX_CACHE_SIZE)
		rx_cache_next = 0;

	rxent->callnum = GET_BE_U_4(rxh->callNumber);
	rxent->client = GET_IPV4_TO_NETWORK_ORDER(ip->ip_src);
	rxent->server = GET_IPV4_TO_NETWORK_ORDER(ip->ip_dst);
	rxent->dport = dport;
	rxent->serviceId = GET_BE_U_2(rxh->serviceId);
	rxent->opcode = GET_BE_U_4(bp + sizeof(struct rx_header));
}

/*
 * Lookup an entry in the cache.  Also taken from print-nfs.c
 *
 * Note that because this is a reply, we're looking at the _source_
 * port.
 */

static int
rx_cache_find(netdissect_options *ndo, const struct rx_header *rxh,
	      const struct ip *ip, uint16_t sport, uint32_t *opcode)
{
	uint32_t i;
	struct rx_cache_entry *rxent;
	uint32_t clip;
	uint32_t sip;

	clip = GET_IPV4_TO_NETWORK_ORDER(ip->ip_dst);
	sip = GET_IPV4_TO_NETWORK_ORDER(ip->ip_src);

	/* Start the search where we last left off */

	i = rx_cache_hint;
	do {
		rxent = &rx_cache[i];
		if (rxent->callnum == GET_BE_U_4(rxh->callNumber) &&
		    rxent->client == clip &&
		    rxent->server == sip &&
		    rxent->serviceId == GET_BE_U_2(rxh->serviceId) &&
		    rxent->dport == sport) {

			/* We got a match! */

			rx_cache_hint = i;
			*opcode = rxent->opcode;
			return(1);
		}
		if (++i >= RX_CACHE_SIZE)
			i = 0;
	} while (i != rx_cache_hint);

	/* Our search failed */
	return(0);
}

/*
 * These extremely grody macros handle the printing of various AFS stuff.
 */

#define FIDOUT() { uint32_t n1, n2, n3; \
			ND_TCHECK_LEN(bp, sizeof(uint32_t) * 3); \
			n1 = GET_BE_U_4(bp); \
			bp += sizeof(uint32_t); \
			n2 = GET_BE_U_4(bp); \
			bp += sizeof(uint32_t); \
			n3 = GET_BE_U_4(bp); \
			bp += sizeof(uint32_t); \
			ND_PRINT(" fid %u/%u/%u", n1, n2, n3); \
		}

#define STROUT(MAX) { uint32_t _i; \
			_i = GET_BE_U_4(bp); \
			if (_i > (MAX)) \
				goto trunc; \
			bp += sizeof(uint32_t); \
			ND_PRINT(" \""); \
			if (nd_printn(ndo, bp, _i, ndo->ndo_snapend)) \
				goto trunc; \
			ND_PRINT("\""); \
			bp += ((_i + sizeof(uint32_t) - 1) / sizeof(uint32_t)) * sizeof(uint32_t); \
		}

#define INTOUT() { int32_t _i; \
			_i = GET_BE_S_4(bp); \
			bp += sizeof(int32_t); \
			ND_PRINT(" %d", _i); \
		}

#define UINTOUT() { uint32_t _i; \
			_i = GET_BE_U_4(bp); \
			bp += sizeof(uint32_t); \
			ND_PRINT(" %u", _i); \
		}

#define UINT64OUT() { uint64_t _i; \
			_i = GET_BE_U_8(bp); \
			bp += sizeof(uint64_t); \
			ND_PRINT(" %" PRIu64, _i); \
		}

#define DATEOUT() { time_t _t; char str[256]; \
			_t = (time_t) GET_BE_S_4(bp); \
			bp += sizeof(int32_t); \
			ND_PRINT(" %s", \
			    nd_format_time(str, sizeof(str), \
			      "%Y-%m-%d %H:%M:%S", localtime(&_t))); \
		}

#define STOREATTROUT() { uint32_t mask, _i; \
			ND_TCHECK_LEN(bp, (sizeof(uint32_t) * 6)); \
			mask = GET_BE_U_4(bp); bp += sizeof(uint32_t); \
			if (mask) ND_PRINT(" StoreStatus"); \
		        if (mask & 1) { ND_PRINT(" date"); DATEOUT(); } \
			else bp += sizeof(uint32_t); \
			_i = GET_BE_U_4(bp); bp += sizeof(uint32_t); \
		        if (mask & 2) ND_PRINT(" owner %u", _i);  \
			_i = GET_BE_U_4(bp); bp += sizeof(uint32_t); \
		        if (mask & 4) ND_PRINT(" group %u", _i); \
			_i = GET_BE_U_4(bp); bp += sizeof(uint32_t); \
		        if (mask & 8) ND_PRINT(" mode %o", _i & 07777); \
			_i = GET_BE_U_4(bp); bp += sizeof(uint32_t); \
		        if (mask & 16) ND_PRINT(" segsize %u", _i); \
			/* undocumented in 3.3 docu */ \
		        if (mask & 1024) ND_PRINT(" fsync");  \
		}

#define UBIK_VERSIONOUT() {uint32_t epoch; uint32_t counter; \
			ND_TCHECK_LEN(bp, sizeof(uint32_t) * 2); \
			epoch = GET_BE_U_4(bp); \
			bp += sizeof(uint32_t); \
			counter = GET_BE_U_4(bp); \
			bp += sizeof(uint32_t); \
			ND_PRINT(" %u.%u", epoch, counter); \
		}

#define AFSUUIDOUT() {uint32_t temp; int _i; \
			ND_TCHECK_LEN(bp, 11 * sizeof(uint32_t)); \
			temp = GET_BE_U_4(bp); \
			bp += sizeof(uint32_t); \
			ND_PRINT(" %08x", temp); \
			temp = GET_BE_U_4(bp); \
			bp += sizeof(uint32_t); \
			ND_PRINT("%04x", temp); \
			temp = GET_BE_U_4(bp); \
			bp += sizeof(uint32_t); \
			ND_PRINT("%04x", temp); \
			for (_i = 0; _i < 8; _i++) { \
				temp = GET_BE_U_4(bp); \
				bp += sizeof(uint32_t); \
				ND_PRINT("%02x", (unsigned char) temp); \
			} \
		}

/*
 * This is the sickest one of all
 * MAX is expected to be a constant here
 */

#define VECOUT(MAX) { u_char *sp; \
			u_char s[(MAX) + 1]; \
			uint32_t k; \
			ND_TCHECK_LEN(bp, (MAX) * sizeof(uint32_t)); \
			sp = s; \
			for (k = 0; k < (MAX); k++) { \
				*sp++ = (u_char) GET_BE_U_4(bp); \
				bp += sizeof(uint32_t); \
			} \
			s[(MAX)] = '\0'; \
			ND_PRINT(" \""); \
			fn_print_str(ndo, s); \
			ND_PRINT("\""); \
		}

#define DESTSERVEROUT() { uint32_t n1, n2, n3; \
			ND_TCHECK_LEN(bp, sizeof(uint32_t) * 3); \
			n1 = GET_BE_U_4(bp); \
			bp += sizeof(uint32_t); \
			n2 = GET_BE_U_4(bp); \
			bp += sizeof(uint32_t); \
			n3 = GET_BE_U_4(bp); \
			bp += sizeof(uint32_t); \
			ND_PRINT(" server %u:%u:%u", n1, n2, n3); \
		}

/*
 * Handle calls to the AFS file service (fs)
 */

static void
fs_print(netdissect_options *ndo,
         const u_char *bp, u_int length)
{
	uint32_t fs_op;
	uint32_t i;

	if (length <= sizeof(struct rx_header))
		return;

	/*
	 * Print out the afs call we're invoking.  The table used here was
	 * gleaned from fsint/afsint.xg
	 */

	fs_op = GET_BE_U_4(bp + sizeof(struct rx_header));

	ND_PRINT(" fs call %s", tok2str(fs_req, "op#%u", fs_op));

	/*
	 * Print out arguments to some of the AFS calls.  This stuff is
	 * all from afsint.xg
	 */

	bp += sizeof(struct rx_header) + 4;

	/*
	 * Sigh.  This is gross.  Ritchie forgive me.
	 */

	switch (fs_op) {
		case 130:	/* Fetch data */
			FIDOUT();
			ND_PRINT(" offset");
			UINTOUT();
			ND_PRINT(" length");
			UINTOUT();
			break;
		case 131:	/* Fetch ACL */
		case 132:	/* Fetch Status */
		case 143:	/* Old set lock */
		case 144:	/* Old extend lock */
		case 145:	/* Old release lock */
		case 156:	/* Set lock */
		case 157:	/* Extend lock */
		case 158:	/* Release lock */
			FIDOUT();
			break;
		case 135:	/* Store status */
			FIDOUT();
			STOREATTROUT();
			break;
		case 133:	/* Store data */
			FIDOUT();
			STOREATTROUT();
			ND_PRINT(" offset");
			UINTOUT();
			ND_PRINT(" length");
			UINTOUT();
			ND_PRINT(" flen");
			UINTOUT();
			break;
		case 134:	/* Store ACL */
		{
			char a[AFSOPAQUEMAX+1];
			FIDOUT();
			i = GET_BE_U_4(bp);
			bp += sizeof(uint32_t);
			ND_TCHECK_LEN(bp, i);
			i = ND_MIN(AFSOPAQUEMAX, i);
			strncpy(a, (const char *) bp, i);
			a[i] = '\0';
			acl_print(ndo, (u_char *) a, (u_char *) a + i);
			break;
		}
		case 137:	/* Create file */
		case 141:	/* MakeDir */
			FIDOUT();
			STROUT(AFSNAMEMAX);
			STOREATTROUT();
			break;
		case 136:	/* Remove file */
		case 142:	/* Remove directory */
			FIDOUT();
			STROUT(AFSNAMEMAX);
			break;
		case 138:	/* Rename file */
			ND_PRINT(" old");
			FIDOUT();
			STROUT(AFSNAMEMAX);
			ND_PRINT(" new");
			FIDOUT();
			STROUT(AFSNAMEMAX);
			break;
		case 139:	/* Symlink */
			FIDOUT();
			STROUT(AFSNAMEMAX);
			ND_PRINT(" link to");
			STROUT(AFSNAMEMAX);
			break;
		case 140:	/* Link */
			FIDOUT();
			STROUT(AFSNAMEMAX);
			ND_PRINT(" link to");
			FIDOUT();
			break;
		case 148:	/* Get volume info */
			STROUT(AFSNAMEMAX);
			break;
		case 149:	/* Get volume stats */
		case 150:	/* Set volume stats */
			ND_PRINT(" volid");
			UINTOUT();
			break;
		case 154:	/* New get volume info */
			ND_PRINT(" volname");
			STROUT(AFSNAMEMAX);
			break;
		case 155:	/* Bulk stat */
		case 65536:     /* Inline bulk stat */
		{
			uint32_t j;
			j = GET_BE_U_4(bp);
			bp += sizeof(uint32_t);

			for (i = 0; i < j; i++) {
				FIDOUT();
				if (i != j - 1)
					ND_PRINT(",");
			}
			if (j == 0)
				ND_PRINT(" <none!>");
			break;
		}
		case 65537:	/* Fetch data 64 */
			FIDOUT();
			ND_PRINT(" offset");
			UINT64OUT();
			ND_PRINT(" length");
			UINT64OUT();
			break;
		case 65538:	/* Store data 64 */
			FIDOUT();
			STOREATTROUT();
			ND_PRINT(" offset");
			UINT64OUT();
			ND_PRINT(" length");
			UINT64OUT();
			ND_PRINT(" flen");
			UINT64OUT();
			break;
		case 65541:    /* CallBack rx conn address */
			ND_PRINT(" addr");
			UINTOUT();
		default:
			;
	}

	return;

trunc:
	ND_PRINT(" [|fs]");
}

/*
 * Handle replies to the AFS file service
 */

static void
fs_reply_print(netdissect_options *ndo,
               const u_char *bp, u_int length, uint32_t opcode)
{
	uint32_t i;
	const struct rx_header *rxh;
	uint8_t type;

	if (length <= sizeof(struct rx_header))
		return;

	rxh = (const struct rx_header *) bp;

	/*
	 * Print out the afs call we're invoking.  The table used here was
	 * gleaned from fsint/afsint.xg
	 */

	ND_PRINT(" fs reply %s", tok2str(fs_req, "op#%u", opcode));

	type = GET_U_1(rxh->type);
	bp += sizeof(struct rx_header);

	/*
	 * If it was a data packet, interpret the response
	 */

	if (type == RX_PACKET_TYPE_DATA) {
		switch (opcode) {
		case 131:	/* Fetch ACL */
		{
			char a[AFSOPAQUEMAX+1];
			i = GET_BE_U_4(bp);
			bp += sizeof(uint32_t);
			ND_TCHECK_LEN(bp, i);
			i = ND_MIN(AFSOPAQUEMAX, i);
			strncpy(a, (const char *) bp, i);
			a[i] = '\0';
			acl_print(ndo, (u_char *) a, (u_char *) a + i);
			break;
		}
		case 137:	/* Create file */
		case 141:	/* MakeDir */
			ND_PRINT(" new");
			FIDOUT();
			break;
		case 151:	/* Get root volume */
			ND_PRINT(" root volume");
			STROUT(AFSNAMEMAX);
			break;
		case 153:	/* Get time */
			DATEOUT();
			break;
		default:
			;
		}
	} else if (type == RX_PACKET_TYPE_ABORT) {
		/*
		 * Otherwise, just print out the return code
		 */
		int32_t errcode;

		errcode = GET_BE_S_4(bp);
		bp += sizeof(int32_t);

		ND_PRINT(" error %s", tok2str(afs_fs_errors, "#%d", errcode));
	} else {
		ND_PRINT(" strange fs reply of type %u", type);
	}

	return;

trunc:
	ND_PRINT(" [|fs]");
}

/*
 * Print out an AFS ACL string.  An AFS ACL is a string that has the
 * following format:
 *
 * <positive> <negative>
 * <uid1> <aclbits1>
 * ....
 *
 * "positive" and "negative" are integers which contain the number of
 * positive and negative ACL's in the string.  The uid/aclbits pair are
 * ASCII strings containing the UID/PTS record and an ASCII number
 * representing a logical OR of all the ACL permission bits
 */

#define XSTRINGIFY(x) #x
#define NUMSTRINGIFY(x)	XSTRINGIFY(x)

static void
acl_print(netdissect_options *ndo,
          u_char *s, const u_char *end)
{
	int pos, neg, acl;
	int n, i;
	char user[USERNAMEMAX+1];

	if (sscanf((char *) s, "%d %d\n%n", &pos, &neg, &n) != 2)
		return;

	s += n;

	if (s > end)
		return;

	/*
	 * This wacky order preserves the order used by the "fs" command
	 */

#define ACLOUT(acl) \
	ND_PRINT("%s%s%s%s%s%s%s", \
	          acl & PRSFS_READ       ? "r" : "", \
	          acl & PRSFS_LOOKUP     ? "l" : "", \
	          acl & PRSFS_INSERT     ? "i" : "", \
	          acl & PRSFS_DELETE     ? "d" : "", \
	          acl & PRSFS_WRITE      ? "w" : "", \
	          acl & PRSFS_LOCK       ? "k" : "", \
	          acl & PRSFS_ADMINISTER ? "a" : "");

	for (i = 0; i < pos; i++) {
		if (sscanf((char *) s, "%" NUMSTRINGIFY(USERNAMEMAX) "s %d\n%n", user, &acl, &n) != 2)
			return;
		s += n;
		ND_PRINT(" +{");
		fn_print_str(ndo, (u_char *)user);
		ND_PRINT(" ");
		ACLOUT(acl);
		ND_PRINT("}");
		if (s > end)
			return;
	}

	for (i = 0; i < neg; i++) {
		if (sscanf((char *) s, "%" NUMSTRINGIFY(USERNAMEMAX) "s %d\n%n", user, &acl, &n) != 2)
			return;
		s += n;
		ND_PRINT(" -{");
		fn_print_str(ndo, (u_char *)user);
		ND_PRINT(" ");
		ACLOUT(acl);
		ND_PRINT("}");
		if (s > end)
			return;
	}
}

#undef ACLOUT

/*
 * Handle calls to the AFS callback service
 */

static void
cb_print(netdissect_options *ndo,
         const u_char *bp, u_int length)
{
	uint32_t cb_op;
	uint32_t i;

	if (length <= sizeof(struct rx_header))
		return;

	/*
	 * Print out the afs call we're invoking.  The table used here was
	 * gleaned from fsint/afscbint.xg
	 */

	cb_op = GET_BE_U_4(bp + sizeof(struct rx_header));

	ND_PRINT(" cb call %s", tok2str(cb_req, "op#%u", cb_op));

	bp += sizeof(struct rx_header) + 4;

	/*
	 * Print out the afs call we're invoking.  The table used here was
	 * gleaned from fsint/afscbint.xg
	 */

	switch (cb_op) {
		case 204:		/* Callback */
		{
			uint32_t j, t;
			j = GET_BE_U_4(bp);
			bp += sizeof(uint32_t);

			for (i = 0; i < j; i++) {
				FIDOUT();
				if (i != j - 1)
					ND_PRINT(",");
			}

			if (j == 0)
				ND_PRINT(" <none!>");

			j = GET_BE_U_4(bp);
			bp += sizeof(uint32_t);

			if (j != 0)
				ND_PRINT(";");

			for (i = 0; i < j; i++) {
				ND_PRINT(" ver");
				INTOUT();
				ND_PRINT(" expires");
				DATEOUT();
				t = GET_BE_U_4(bp);
				bp += sizeof(uint32_t);
				tok2str(cb_types, "type %u", t);
			}
			break;
		}
		case 214: {
			ND_PRINT(" afsuuid");
			AFSUUIDOUT();
			break;
		}
		default:
			;
	}

	return;

trunc:
	ND_PRINT(" [|cb]");
}

/*
 * Handle replies to the AFS Callback Service
 */

static void
cb_reply_print(netdissect_options *ndo,
               const u_char *bp, u_int length, uint32_t opcode)
{
	const struct rx_header *rxh;
	uint8_t type;

	if (length <= sizeof(struct rx_header))
		return;

	rxh = (const struct rx_header *) bp;

	/*
	 * Print out the afs call we're invoking.  The table used here was
	 * gleaned from fsint/afscbint.xg
	 */

	ND_PRINT(" cb reply %s", tok2str(cb_req, "op#%u", opcode));

	type = GET_U_1(rxh->type);
	bp += sizeof(struct rx_header);

	/*
	 * If it was a data packet, interpret the response.
	 */

	if (type == RX_PACKET_TYPE_DATA)
		switch (opcode) {
		case 213:	/* InitCallBackState3 */
			AFSUUIDOUT();
			break;
		default:
		;
		}
	else {
		/*
		 * Otherwise, just print out the return code
		 */
		ND_PRINT(" errcode");
		INTOUT();
	}

	return;

trunc:
	ND_PRINT(" [|cb]");
}

/*
 * Handle calls to the AFS protection database server
 */

static void
prot_print(netdissect_options *ndo,
           const u_char *bp, u_int length)
{
	uint32_t i;
	uint32_t pt_op;

	if (length <= sizeof(struct rx_header))
		return;

	/*
	 * Print out the afs call we're invoking.  The table used here was
	 * gleaned from ptserver/ptint.xg
	 */

	pt_op = GET_BE_U_4(bp + sizeof(struct rx_header));

	ND_PRINT(" pt");

	if (is_ubik(pt_op)) {
		ubik_print(ndo, bp);
		return;
	}

	ND_PRINT(" call %s", tok2str(pt_req, "op#%u", pt_op));

	/*
	 * Decode some of the arguments to the PT calls
	 */

	bp += sizeof(struct rx_header) + 4;

	switch (pt_op) {
		case 500:	/* I New User */
			STROUT(PRNAMEMAX);
			ND_PRINT(" id");
			INTOUT();
			ND_PRINT(" oldid");
			INTOUT();
			break;
		case 501:	/* Where is it */
		case 506:	/* Delete */
		case 508:	/* Get CPS */
		case 512:	/* List entry */
		case 514:	/* List elements */
		case 517:	/* List owned */
		case 518:	/* Get CPS2 */
		case 519:	/* Get host CPS */
		case 530:	/* List super groups */
			ND_PRINT(" id");
			INTOUT();
			break;
		case 502:	/* Dump entry */
			ND_PRINT(" pos");
			INTOUT();
			break;
		case 503:	/* Add to group */
		case 507:	/* Remove from group */
		case 515:	/* Is a member of? */
			ND_PRINT(" uid");
			INTOUT();
			ND_PRINT(" gid");
			INTOUT();
			break;
		case 504:	/* Name to ID */
		{
			uint32_t j;
			j = GET_BE_U_4(bp);
			bp += sizeof(uint32_t);

			/*
			 * Who designed this chicken-shit protocol?
			 *
			 * Each character is stored as a 32-bit
			 * integer!
			 */

			for (i = 0; i < j; i++) {
				VECOUT(PRNAMEMAX);
			}
			if (j == 0)
				ND_PRINT(" <none!>");
		}
			break;
		case 505:	/* Id to name */
		{
			uint32_t j;
			ND_PRINT(" ids:");
			i = GET_BE_U_4(bp);
			bp += sizeof(uint32_t);
			for (j = 0; j < i; j++)
				INTOUT();
			if (j == 0)
				ND_PRINT(" <none!>");
		}
			break;
		case 509:	/* New entry */
			STROUT(PRNAMEMAX);
			ND_PRINT(" flag");
			INTOUT();
			ND_PRINT(" oid");
			INTOUT();
			break;
		case 511:	/* Set max */
			ND_PRINT(" id");
			INTOUT();
			ND_PRINT(" gflag");
			INTOUT();
			break;
		case 513:	/* Change entry */
			ND_PRINT(" id");
			INTOUT();
			STROUT(PRNAMEMAX);
			ND_PRINT(" oldid");
			INTOUT();
			ND_PRINT(" newid");
			INTOUT();
			break;
		case 520:	/* Update entry */
			ND_PRINT(" id");
			INTOUT();
			STROUT(PRNAMEMAX);
			break;
		default:
			;
	}


	return;

trunc:
	ND_PRINT(" [|pt]");
}

/*
 * Handle replies to the AFS protection service
 */

static void
prot_reply_print(netdissect_options *ndo,
                 const u_char *bp, u_int length, uint32_t opcode)
{
	const struct rx_header *rxh;
	uint8_t type;
	uint32_t i;

	if (length < sizeof(struct rx_header))
		return;

	rxh = (const struct rx_header *) bp;

	/*
	 * Print out the afs call we're invoking.  The table used here was
	 * gleaned from ptserver/ptint.xg.  Check to see if it's a
	 * Ubik call, however.
	 */

	ND_PRINT(" pt");

	if (is_ubik(opcode)) {
		ubik_reply_print(ndo, bp, length, opcode);
		return;
	}

	ND_PRINT(" reply %s", tok2str(pt_req, "op#%u", opcode));

	type = GET_U_1(rxh->type);
	bp += sizeof(struct rx_header);

	/*
	 * If it was a data packet, interpret the response
	 */

	if (type == RX_PACKET_TYPE_DATA)
		switch (opcode) {
		case 504:		/* Name to ID */
		{
			uint32_t j;
			ND_PRINT(" ids:");
			i = GET_BE_U_4(bp);
			bp += sizeof(uint32_t);
			for (j = 0; j < i; j++)
				INTOUT();
			if (j == 0)
				ND_PRINT(" <none!>");
		}
			break;
		case 505:		/* ID to name */
		{
			uint32_t j;
			j = GET_BE_U_4(bp);
			bp += sizeof(uint32_t);

			/*
			 * Who designed this chicken-shit protocol?
			 *
			 * Each character is stored as a 32-bit
			 * integer!
			 */

			for (i = 0; i < j; i++) {
				VECOUT(PRNAMEMAX);
			}
			if (j == 0)
				ND_PRINT(" <none!>");
		}
			break;
		case 508:		/* Get CPS */
		case 514:		/* List elements */
		case 517:		/* List owned */
		case 518:		/* Get CPS2 */
		case 519:		/* Get host CPS */
		{
			uint32_t j;
			j = GET_BE_U_4(bp);
			bp += sizeof(uint32_t);
			for (i = 0; i < j; i++) {
				INTOUT();
			}
			if (j == 0)
				ND_PRINT(" <none!>");
		}
			break;
		case 510:		/* List max */
			ND_PRINT(" maxuid");
			INTOUT();
			ND_PRINT(" maxgid");
			INTOUT();
			break;
		default:
			;
		}
	else {
		/*
		 * Otherwise, just print out the return code
		 */
		ND_PRINT(" errcode");
		INTOUT();
	}

	return;

trunc:
	ND_PRINT(" [|pt]");
}

/*
 * Handle calls to the AFS volume location database service
 */

static void
vldb_print(netdissect_options *ndo,
           const u_char *bp, u_int length)
{
	uint32_t vldb_op;
	uint32_t i;

	if (length <= sizeof(struct rx_header))
		return;

	/*
	 * Print out the afs call we're invoking.  The table used here was
	 * gleaned from vlserver/vldbint.xg
	 */

	vldb_op = GET_BE_U_4(bp + sizeof(struct rx_header));

	ND_PRINT(" vldb");

	if (is_ubik(vldb_op)) {
		ubik_print(ndo, bp);
		return;
	}
	ND_PRINT(" call %s", tok2str(vldb_req, "op#%u", vldb_op));

	/*
	 * Decode some of the arguments to the VLDB calls
	 */

	bp += sizeof(struct rx_header) + 4;

	switch (vldb_op) {
		case 501:	/* Create new volume */
		case 517:	/* Create entry N */
			VECOUT(VLNAMEMAX);
			break;
		case 502:	/* Delete entry */
		case 503:	/* Get entry by ID */
		case 507:	/* Update entry */
		case 508:	/* Set lock */
		case 509:	/* Release lock */
		case 518:	/* Get entry by ID N */
			ND_PRINT(" volid");
			INTOUT();
			i = GET_BE_U_4(bp);
			bp += sizeof(uint32_t);
			if (i <= 2)
				ND_PRINT(" type %s", voltype[i]);
			break;
		case 504:	/* Get entry by name */
		case 519:	/* Get entry by name N */
		case 524:	/* Update entry by name */
		case 527:	/* Get entry by name U */
			STROUT(VLNAMEMAX);
			break;
		case 505:	/* Get new vol id */
			ND_PRINT(" bump");
			INTOUT();
			break;
		case 506:	/* Replace entry */
		case 520:	/* Replace entry N */
			ND_PRINT(" volid");
			INTOUT();
			i = GET_BE_U_4(bp);
			bp += sizeof(uint32_t);
			if (i <= 2)
				ND_PRINT(" type %s", voltype[i]);
			VECOUT(VLNAMEMAX);
			break;
		case 510:	/* List entry */
		case 521:	/* List entry N */
			ND_PRINT(" index");
			INTOUT();
			break;
		default:
			;
	}

	return;

trunc:
	ND_PRINT(" [|vldb]");
}

/*
 * Handle replies to the AFS volume location database service
 */

static void
vldb_reply_print(netdissect_options *ndo,
                 const u_char *bp, u_int length, uint32_t opcode)
{
	const struct rx_header *rxh;
	uint8_t type;
	uint32_t i;

	if (length < sizeof(struct rx_header))
		return;

	rxh = (const struct rx_header *) bp;

	/*
	 * Print out the afs call we're invoking.  The table used here was
	 * gleaned from vlserver/vldbint.xg.  Check to see if it's a
	 * Ubik call, however.
	 */

	ND_PRINT(" vldb");

	if (is_ubik(opcode)) {
		ubik_reply_print(ndo, bp, length, opcode);
		return;
	}

	ND_PRINT(" reply %s", tok2str(vldb_req, "op#%u", opcode));

	type = GET_U_1(rxh->type);
	bp += sizeof(struct rx_header);

	/*
	 * If it was a data packet, interpret the response
	 */

	if (type == RX_PACKET_TYPE_DATA)
		switch (opcode) {
		case 510:	/* List entry */
			ND_PRINT(" count");
			INTOUT();
			ND_PRINT(" nextindex");
			INTOUT();
			ND_FALL_THROUGH;
		case 503:	/* Get entry by id */
		case 504:	/* Get entry by name */
		{	uint32_t nservers, j;
			VECOUT(VLNAMEMAX);
			ND_TCHECK_4(bp);
			bp += sizeof(uint32_t);
			ND_PRINT(" numservers");
			nservers = GET_BE_U_4(bp);
			bp += sizeof(uint32_t);
			ND_PRINT(" %u", nservers);
			ND_PRINT(" servers");
			for (i = 0; i < 8; i++) {
				ND_TCHECK_4(bp);
				if (i < nservers)
					ND_PRINT(" %s",
					   intoa(GET_IPV4_TO_NETWORK_ORDER(bp)));
				bp += sizeof(nd_ipv4);
			}
			ND_PRINT(" partitions");
			for (i = 0; i < 8; i++) {
				j = GET_BE_U_4(bp);
				if (i < nservers && j <= 26)
					ND_PRINT(" %c", 'a' + j);
				else if (i < nservers)
					ND_PRINT(" %u", j);
				bp += sizeof(uint32_t);
			}
			ND_TCHECK_LEN(bp, 8 * sizeof(uint32_t));
			bp += 8 * sizeof(uint32_t);
			ND_PRINT(" rwvol");
			UINTOUT();
			ND_PRINT(" rovol");
			UINTOUT();
			ND_PRINT(" backup");
			UINTOUT();
		}
			break;
		case 505:	/* Get new volume ID */
			ND_PRINT(" newvol");
			UINTOUT();
			break;
		case 521:	/* List entry */
		case 529:	/* List entry U */
			ND_PRINT(" count");
			INTOUT();
			ND_PRINT(" nextindex");
			INTOUT();
			ND_FALL_THROUGH;
		case 518:	/* Get entry by ID N */
		case 519:	/* Get entry by name N */
		{	uint32_t nservers, j;
			VECOUT(VLNAMEMAX);
			ND_PRINT(" numservers");
			nservers = GET_BE_U_4(bp);
			bp += sizeof(uint32_t);
			ND_PRINT(" %u", nservers);
			ND_PRINT(" servers");
			for (i = 0; i < 13; i++) {
				ND_TCHECK_4(bp);
				if (i < nservers)
					ND_PRINT(" %s",
					   intoa(GET_IPV4_TO_NETWORK_ORDER(bp)));
				bp += sizeof(nd_ipv4);
			}
			ND_PRINT(" partitions");
			for (i = 0; i < 13; i++) {
				j = GET_BE_U_4(bp);
				if (i < nservers && j <= 26)
					ND_PRINT(" %c", 'a' + j);
				else if (i < nservers)
					ND_PRINT(" %u", j);
				bp += sizeof(uint32_t);
			}
			ND_TCHECK_LEN(bp, 13 * sizeof(uint32_t));
			bp += 13 * sizeof(uint32_t);
			ND_PRINT(" rwvol");
			UINTOUT();
			ND_PRINT(" rovol");
			UINTOUT();
			ND_PRINT(" backup");
			UINTOUT();
		}
			break;
		case 526:	/* Get entry by ID U */
		case 527:	/* Get entry by name U */
		{	uint32_t nservers, j;
			VECOUT(VLNAMEMAX);
			ND_PRINT(" numservers");
			nservers = GET_BE_U_4(bp);
			bp += sizeof(uint32_t);
			ND_PRINT(" %u", nservers);
			ND_PRINT(" servers");
			for (i = 0; i < 13; i++) {
				if (i < nservers) {
					ND_PRINT(" afsuuid");
					AFSUUIDOUT();
				} else {
					ND_TCHECK_LEN(bp, 44);
					bp += 44;
				}
			}
			ND_TCHECK_LEN(bp, 4 * 13);
			bp += 4 * 13;
			ND_PRINT(" partitions");
			for (i = 0; i < 13; i++) {
				j = GET_BE_U_4(bp);
				if (i < nservers && j <= 26)
					ND_PRINT(" %c", 'a' + j);
				else if (i < nservers)
					ND_PRINT(" %u", j);
				bp += sizeof(uint32_t);
			}
			ND_TCHECK_LEN(bp, 13 * sizeof(uint32_t));
			bp += 13 * sizeof(uint32_t);
			ND_PRINT(" rwvol");
			UINTOUT();
			ND_PRINT(" rovol");
			UINTOUT();
			ND_PRINT(" backup");
			UINTOUT();
		}
		default:
			;
		}

	else {
		/*
		 * Otherwise, just print out the return code
		 */
		ND_PRINT(" errcode");
		INTOUT();
	}

	return;

trunc:
	ND_PRINT(" [|vldb]");
}

/*
 * Handle calls to the AFS Kerberos Authentication service
 */

static void
kauth_print(netdissect_options *ndo,
            const u_char *bp, u_int length)
{
	uint32_t kauth_op;

	if (length <= sizeof(struct rx_header))
		return;

	/*
	 * Print out the afs call we're invoking.  The table used here was
	 * gleaned from kauth/kauth.rg
	 */

	kauth_op = GET_BE_U_4(bp + sizeof(struct rx_header));

	ND_PRINT(" kauth");

	if (is_ubik(kauth_op)) {
		ubik_print(ndo, bp);
		return;
	}


	ND_PRINT(" call %s", tok2str(kauth_req, "op#%u", kauth_op));

	/*
	 * Decode some of the arguments to the KA calls
	 */

	bp += sizeof(struct rx_header) + 4;

	switch (kauth_op) {
		case 1:		/* Authenticate old */
		case 21:	/* Authenticate */
		case 22:	/* Authenticate-V2 */
		case 2:		/* Change PW */
		case 5:		/* Set fields */
		case 6:		/* Create user */
		case 7:		/* Delete user */
		case 8:		/* Get entry */
		case 14:	/* Unlock */
		case 15:	/* Lock status */
			ND_PRINT(" principal");
			STROUT(KANAMEMAX);
			STROUT(KANAMEMAX);
			break;
		case 3:		/* GetTicket-old */
		case 23:	/* GetTicket */
		{
			uint32_t i;
			ND_PRINT(" kvno");
			INTOUT();
			ND_PRINT(" domain");
			STROUT(KANAMEMAX);
			i = GET_BE_U_4(bp);
			bp += sizeof(uint32_t);
			ND_TCHECK_LEN(bp, i);
			bp += i;
			ND_PRINT(" principal");
			STROUT(KANAMEMAX);
			STROUT(KANAMEMAX);
			break;
		}
		case 4:		/* Set Password */
			ND_PRINT(" principal");
			STROUT(KANAMEMAX);
			STROUT(KANAMEMAX);
			ND_PRINT(" kvno");
			INTOUT();
			break;
		case 12:	/* Get password */
			ND_PRINT(" name");
			STROUT(KANAMEMAX);
			break;
		default:
			;
	}

	return;

trunc:
	ND_PRINT(" [|kauth]");
}

/*
 * Handle replies to the AFS Kerberos Authentication Service
 */

static void
kauth_reply_print(netdissect_options *ndo,
                  const u_char *bp, u_int length, uint32_t opcode)
{
	const struct rx_header *rxh;
	uint8_t type;

	if (length <= sizeof(struct rx_header))
		return;

	rxh = (const struct rx_header *) bp;

	/*
	 * Print out the afs call we're invoking.  The table used here was
	 * gleaned from kauth/kauth.rg
	 */

	ND_PRINT(" kauth");

	if (is_ubik(opcode)) {
		ubik_reply_print(ndo, bp, length, opcode);
		return;
	}

	ND_PRINT(" reply %s", tok2str(kauth_req, "op#%u", opcode));

	type = GET_U_1(rxh->type);
	bp += sizeof(struct rx_header);

	/*
	 * If it was a data packet, interpret the response.
	 */

	if (type == RX_PACKET_TYPE_DATA)
		/* Well, no, not really.  Leave this for later */
		;
	else {
		/*
		 * Otherwise, just print out the return code
		 */
		ND_PRINT(" errcode");
		INTOUT();
	}
}

/*
 * Handle calls to the AFS Volume location service
 */

static void
vol_print(netdissect_options *ndo,
          const u_char *bp, u_int length)
{
	uint32_t vol_op;

	if (length <= sizeof(struct rx_header))
		return;

	/*
	 * Print out the afs call we're invoking.  The table used here was
	 * gleaned from volser/volint.xg
	 */

	vol_op = GET_BE_U_4(bp + sizeof(struct rx_header));

	ND_PRINT(" vol call %s", tok2str(vol_req, "op#%u", vol_op));

	bp += sizeof(struct rx_header) + 4;

	switch (vol_op) {
		case 100:	/* Create volume */
			ND_PRINT(" partition");
			UINTOUT();
			ND_PRINT(" name");
			STROUT(AFSNAMEMAX);
			ND_PRINT(" type");
			UINTOUT();
			ND_PRINT(" parent");
			UINTOUT();
			break;
		case 101:	/* Delete volume */
		case 107:	/* Get flags */
			ND_PRINT(" trans");
			UINTOUT();
			break;
		case 102:	/* Restore */
			ND_PRINT(" totrans");
			UINTOUT();
			ND_PRINT(" flags");
			UINTOUT();
			break;
		case 103:	/* Forward */
			ND_PRINT(" fromtrans");
			UINTOUT();
			ND_PRINT(" fromdate");
			DATEOUT();
			DESTSERVEROUT();
			ND_PRINT(" desttrans");
			INTOUT();
			break;
		case 104:	/* End trans */
			ND_PRINT(" trans");
			UINTOUT();
			break;
		case 105:	/* Clone */
			ND_PRINT(" trans");
			UINTOUT();
			ND_PRINT(" purgevol");
			UINTOUT();
			ND_PRINT(" newtype");
			UINTOUT();
			ND_PRINT(" newname");
			STROUT(AFSNAMEMAX);
			break;
		case 106:	/* Set flags */
			ND_PRINT(" trans");
			UINTOUT();
			ND_PRINT(" flags");
			UINTOUT();
			break;
		case 108:	/* Trans create */
			ND_PRINT(" vol");
			UINTOUT();
			ND_PRINT(" partition");
			UINTOUT();
			ND_PRINT(" flags");
			UINTOUT();
			break;
		case 109:	/* Dump */
		case 655537:	/* Get size */
			ND_PRINT(" fromtrans");
			UINTOUT();
			ND_PRINT(" fromdate");
			DATEOUT();
			break;
		case 110:	/* Get n-th volume */
			ND_PRINT(" index");
			UINTOUT();
			break;
		case 111:	/* Set forwarding */
			ND_PRINT(" tid");
			UINTOUT();
			ND_PRINT(" newsite");
			UINTOUT();
			break;
		case 112:	/* Get name */
		case 113:	/* Get status */
			ND_PRINT(" tid");
			break;
		case 114:	/* Signal restore */
			ND_PRINT(" name");
			STROUT(AFSNAMEMAX);
			ND_PRINT(" type");
			UINTOUT();
			ND_PRINT(" pid");
			UINTOUT();
			ND_PRINT(" cloneid");
			UINTOUT();
			break;
		case 116:	/* List volumes */
			ND_PRINT(" partition");
			UINTOUT();
			ND_PRINT(" flags");
			UINTOUT();
			break;
		case 117:	/* Set id types */
			ND_PRINT(" tid");
			UINTOUT();
			ND_PRINT(" name");
			STROUT(AFSNAMEMAX);
			ND_PRINT(" type");
			UINTOUT();
			ND_PRINT(" pid");
			UINTOUT();
			ND_PRINT(" clone");
			UINTOUT();
			ND_PRINT(" backup");
			UINTOUT();
			break;
		case 119:	/* Partition info */
			ND_PRINT(" name");
			STROUT(AFSNAMEMAX);
			break;
		case 120:	/* Reclone */
			ND_PRINT(" tid");
			UINTOUT();
			break;
		case 121:	/* List one volume */
		case 122:	/* Nuke volume */
		case 124:	/* Extended List volumes */
		case 125:	/* Extended List one volume */
		case 65536:	/* Convert RO to RW volume */
			ND_PRINT(" partid");
			UINTOUT();
			ND_PRINT(" volid");
			UINTOUT();
			break;
		case 123:	/* Set date */
			ND_PRINT(" tid");
			UINTOUT();
			ND_PRINT(" date");
			DATEOUT();
			break;
		case 126:	/* Set info */
			ND_PRINT(" tid");
			UINTOUT();
			break;
		case 128:	/* Forward multiple */
			ND_PRINT(" fromtrans");
			UINTOUT();
			ND_PRINT(" fromdate");
			DATEOUT();
			{
				uint32_t i, j;
				j = GET_BE_U_4(bp);
				bp += sizeof(uint32_t);
				for (i = 0; i < j; i++) {
					DESTSERVEROUT();
					if (i != j - 1)
						ND_PRINT(",");
				}
				if (j == 0)
					ND_PRINT(" <none!>");
			}
			break;
		case 65538:	/* Dump version 2 */
			ND_PRINT(" fromtrans");
			UINTOUT();
			ND_PRINT(" fromdate");
			DATEOUT();
			ND_PRINT(" flags");
			UINTOUT();
			break;
		default:
			;
	}
	return;

trunc:
	ND_PRINT(" [|vol]");
}

/*
 * Handle replies to the AFS Volume Service
 */

static void
vol_reply_print(netdissect_options *ndo,
                const u_char *bp, u_int length, uint32_t opcode)
{
	const struct rx_header *rxh;
	uint8_t type;

	if (length <= sizeof(struct rx_header))
		return;

	rxh = (const struct rx_header *) bp;

	/*
	 * Print out the afs call we're invoking.  The table used here was
	 * gleaned from volser/volint.xg
	 */

	ND_PRINT(" vol reply %s", tok2str(vol_req, "op#%u", opcode));

	type = GET_U_1(rxh->type);
	bp += sizeof(struct rx_header);

	/*
	 * If it was a data packet, interpret the response.
	 */

	if (type == RX_PACKET_TYPE_DATA) {
		switch (opcode) {
			case 100:	/* Create volume */
				ND_PRINT(" volid");
				UINTOUT();
				ND_PRINT(" trans");
				UINTOUT();
				break;
			case 104:	/* End transaction */
				UINTOUT();
				break;
			case 105:	/* Clone */
				ND_PRINT(" newvol");
				UINTOUT();
				break;
			case 107:	/* Get flags */
				UINTOUT();
				break;
			case 108:	/* Transaction create */
				ND_PRINT(" trans");
				UINTOUT();
				break;
			case 110:	/* Get n-th volume */
				ND_PRINT(" volume");
				UINTOUT();
				ND_PRINT(" partition");
				UINTOUT();
				break;
			case 112:	/* Get name */
				STROUT(AFSNAMEMAX);
				break;
			case 113:	/* Get status */
				ND_PRINT(" volid");
				UINTOUT();
				ND_PRINT(" nextuniq");
				UINTOUT();
				ND_PRINT(" type");
				UINTOUT();
				ND_PRINT(" parentid");
				UINTOUT();
				ND_PRINT(" clone");
				UINTOUT();
				ND_PRINT(" backup");
				UINTOUT();
				ND_PRINT(" restore");
				UINTOUT();
				ND_PRINT(" maxquota");
				UINTOUT();
				ND_PRINT(" minquota");
				UINTOUT();
				ND_PRINT(" owner");
				UINTOUT();
				ND_PRINT(" create");
				DATEOUT();
				ND_PRINT(" access");
				DATEOUT();
				ND_PRINT(" update");
				DATEOUT();
				ND_PRINT(" expire");
				DATEOUT();
				ND_PRINT(" backup");
				DATEOUT();
				ND_PRINT(" copy");
				DATEOUT();
				break;
			case 115:	/* Old list partitions */
				break;
			case 116:	/* List volumes */
			case 121:	/* List one volume */
				{
					uint32_t i, j;
					j = GET_BE_U_4(bp);
					bp += sizeof(uint32_t);
					for (i = 0; i < j; i++) {
						ND_PRINT(" name");
						VECOUT(32);
						ND_PRINT(" volid");
						UINTOUT();
						ND_PRINT(" type");
						bp += sizeof(uint32_t) * 21;
						if (i != j - 1)
							ND_PRINT(",");
					}
					if (j == 0)
						ND_PRINT(" <none!>");
				}
				break;


			default:
				;
		}
	} else {
		/*
		 * Otherwise, just print out the return code
		 */
		ND_PRINT(" errcode");
		INTOUT();
	}

	return;

trunc:
	ND_PRINT(" [|vol]");
}

/*
 * Handle calls to the AFS BOS service
 */

static void
bos_print(netdissect_options *ndo,
          const u_char *bp, u_int length)
{
	uint32_t bos_op;

	if (length <= sizeof(struct rx_header))
		return;

	/*
	 * Print out the afs call we're invoking.  The table used here was
	 * gleaned from bozo/bosint.xg
	 */

	bos_op = GET_BE_U_4(bp + sizeof(struct rx_header));

	ND_PRINT(" bos call %s", tok2str(bos_req, "op#%u", bos_op));

	/*
	 * Decode some of the arguments to the BOS calls
	 */

	bp += sizeof(struct rx_header) + 4;

	switch (bos_op) {
		case 80:	/* Create B node */
			ND_PRINT(" type");
			STROUT(BOSNAMEMAX);
			ND_PRINT(" instance");
			STROUT(BOSNAMEMAX);
			break;
		case 81:	/* Delete B node */
		case 83:	/* Get status */
		case 85:	/* Get instance info */
		case 87:	/* Add super user */
		case 88:	/* Delete super user */
		case 93:	/* Set cell name */
		case 96:	/* Add cell host */
		case 97:	/* Delete cell host */
		case 104:	/* Restart */
		case 106:	/* Uninstall */
		case 108:	/* Exec */
		case 112:	/* Getlog */
		case 114:	/* Get instance strings */
			STROUT(BOSNAMEMAX);
			break;
		case 82:	/* Set status */
		case 98:	/* Set T status */
			STROUT(BOSNAMEMAX);
			ND_PRINT(" status");
			INTOUT();
			break;
		case 86:	/* Get instance parm */
			STROUT(BOSNAMEMAX);
			ND_PRINT(" num");
			INTOUT();
			break;
		case 84:	/* Enumerate instance */
		case 89:	/* List super users */
		case 90:	/* List keys */
		case 91:	/* Add key */
		case 92:	/* Delete key */
		case 95:	/* Get cell host */
			INTOUT();
			break;
		case 105:	/* Install */
			STROUT(BOSNAMEMAX);
			ND_PRINT(" size");
			INTOUT();
			ND_PRINT(" flags");
			INTOUT();
			ND_PRINT(" date");
			INTOUT();
			break;
		default:
			;
	}

	return;

trunc:
	ND_PRINT(" [|bos]");
}

/*
 * Handle replies to the AFS BOS Service
 */

static void
bos_reply_print(netdissect_options *ndo,
                const u_char *bp, u_int length, uint32_t opcode)
{
	const struct rx_header *rxh;
	uint8_t type;

	if (length <= sizeof(struct rx_header))
		return;

	rxh = (const struct rx_header *) bp;

	/*
	 * Print out the afs call we're invoking.  The table used here was
	 * gleaned from volser/volint.xg
	 */

	ND_PRINT(" bos reply %s", tok2str(bos_req, "op#%u", opcode));

	type = GET_U_1(rxh->type);
	bp += sizeof(struct rx_header);

	/*
	 * If it was a data packet, interpret the response.
	 */

	if (type == RX_PACKET_TYPE_DATA)
		/* Well, no, not really.  Leave this for later */
		;
	else {
		/*
		 * Otherwise, just print out the return code
		 */
		ND_PRINT(" errcode");
		INTOUT();
	}
}

/*
 * Check to see if this is a Ubik opcode.
 */

static int
is_ubik(uint32_t opcode)
{
	if ((opcode >= VOTE_LOW && opcode <= VOTE_HIGH) ||
	    (opcode >= DISK_LOW && opcode <= DISK_HIGH))
		return(1);
	else
		return(0);
}

/*
 * Handle Ubik opcodes to any one of the replicated database services
 */

static void
ubik_print(netdissect_options *ndo,
           const u_char *bp)
{
	uint32_t ubik_op;
	uint32_t temp;

	/*
	 * Print out the afs call we're invoking.  The table used here was
	 * gleaned from ubik/ubik_int.xg
	 */

	/* Every function that calls this function first makes a bounds check
	 * for (sizeof(rx_header) + 4) bytes, so long as it remains this way
	 * the line below will not over-read.
	 */
	ubik_op = GET_BE_U_4(bp + sizeof(struct rx_header));

	ND_PRINT(" ubik call %s", tok2str(ubik_req, "op#%u", ubik_op));

	/*
	 * Decode some of the arguments to the Ubik calls
	 */

	bp += sizeof(struct rx_header) + 4;

	switch (ubik_op) {
		case 10000:		/* Beacon */
			temp = GET_BE_U_4(bp);
			bp += sizeof(uint32_t);
			ND_PRINT(" syncsite %s", temp ? "yes" : "no");
			ND_PRINT(" votestart");
			DATEOUT();
			ND_PRINT(" dbversion");
			UBIK_VERSIONOUT();
			ND_PRINT(" tid");
			UBIK_VERSIONOUT();
			break;
		case 10003:		/* Get sync site */
			ND_PRINT(" site");
			UINTOUT();
			break;
		case 20000:		/* Begin */
		case 20001:		/* Commit */
		case 20007:		/* Abort */
		case 20008:		/* Release locks */
		case 20010:		/* Writev */
			ND_PRINT(" tid");
			UBIK_VERSIONOUT();
			break;
		case 20002:		/* Lock */
			ND_PRINT(" tid");
			UBIK_VERSIONOUT();
			ND_PRINT(" file");
			INTOUT();
			ND_PRINT(" pos");
			INTOUT();
			ND_PRINT(" length");
			INTOUT();
			temp = GET_BE_U_4(bp);
			bp += sizeof(uint32_t);
			tok2str(ubik_lock_types, "type %u", temp);
			break;
		case 20003:		/* Write */
			ND_PRINT(" tid");
			UBIK_VERSIONOUT();
			ND_PRINT(" file");
			INTOUT();
			ND_PRINT(" pos");
			INTOUT();
			break;
		case 20005:		/* Get file */
			ND_PRINT(" file");
			INTOUT();
			break;
		case 20006:		/* Send file */
			ND_PRINT(" file");
			INTOUT();
			ND_PRINT(" length");
			INTOUT();
			ND_PRINT(" dbversion");
			UBIK_VERSIONOUT();
			break;
		case 20009:		/* Truncate */
			ND_PRINT(" tid");
			UBIK_VERSIONOUT();
			ND_PRINT(" file");
			INTOUT();
			ND_PRINT(" length");
			INTOUT();
			break;
		case 20012:		/* Set version */
			ND_PRINT(" tid");
			UBIK_VERSIONOUT();
			ND_PRINT(" oldversion");
			UBIK_VERSIONOUT();
			ND_PRINT(" newversion");
			UBIK_VERSIONOUT();
			break;
		default:
			;
	}

	return;

trunc:
	ND_PRINT(" [|ubik]");
}

/*
 * Handle Ubik replies to any one of the replicated database services
 */

static void
ubik_reply_print(netdissect_options *ndo,
                 const u_char *bp, u_int length, uint32_t opcode)
{
	const struct rx_header *rxh;
	uint8_t type;

	if (length < sizeof(struct rx_header))
		return;

	rxh = (const struct rx_header *) bp;

	/*
	 * Print out the ubik call we're invoking.  This table was gleaned
	 * from ubik/ubik_int.xg
	 */

	ND_PRINT(" ubik reply %s", tok2str(ubik_req, "op#%u", opcode));

	type = GET_U_1(rxh->type);
	bp += sizeof(struct rx_header);

	/*
	 * If it was a data packet, print out the arguments to the Ubik calls
	 */

	if (type == RX_PACKET_TYPE_DATA)
		switch (opcode) {
		case 10000:		/* Beacon */
			ND_PRINT(" vote no");
			break;
		case 20004:		/* Get version */
			ND_PRINT(" dbversion");
			UBIK_VERSIONOUT();
			break;
		default:
			;
		}

	/*
	 * Otherwise, print out "yes" if it was a beacon packet (because
	 * that's how yes votes are returned, go figure), otherwise
	 * just print out the error code.
	 */

	else
		switch (opcode) {
		case 10000:		/* Beacon */
			ND_PRINT(" vote yes until");
			DATEOUT();
			break;
		default:
			ND_PRINT(" errcode");
			INTOUT();
		}

	return;

trunc:
	ND_PRINT(" [|ubik]");
}

/*
 * Handle RX ACK packets.
 */

static void
rx_ack_print(netdissect_options *ndo,
             const u_char *bp, u_int length)
{
	const struct rx_ackPacket *rxa;
	uint8_t nAcks;
	int i, start, last;
	uint32_t firstPacket;

	if (length < sizeof(struct rx_header))
		return;

	bp += sizeof(struct rx_header);

	ND_TCHECK_LEN(bp, sizeof(struct rx_ackPacket));

	rxa = (const struct rx_ackPacket *) bp;
	bp += sizeof(struct rx_ackPacket);

	/*
	 * Print out a few useful things from the ack packet structure
	 */

	if (ndo->ndo_vflag > 2)
		ND_PRINT(" bufspace %u maxskew %u",
		       GET_BE_U_2(rxa->bufferSpace),
		       GET_BE_U_2(rxa->maxSkew));

	firstPacket = GET_BE_U_4(rxa->firstPacket);
	ND_PRINT(" first %u serial %u reason %s",
	       firstPacket, GET_BE_U_4(rxa->serial),
	       tok2str(rx_ack_reasons, "#%u", GET_U_1(rxa->reason)));

	/*
	 * Okay, now we print out the ack array.  The way _this_ works
	 * is that we start at "first", and step through the ack array.
	 * If we have a contiguous range of acks/nacks, try to
	 * collapse them into a range.
	 *
	 * If you're really clever, you might have noticed that this
	 * doesn't seem quite correct.  Specifically, due to structure
	 * padding, sizeof(struct rx_ackPacket) - RX_MAXACKS won't actually
	 * yield the start of the ack array (because RX_MAXACKS is 255
	 * and the structure will likely get padded to a 2 or 4 byte
	 * boundary).  However, this is the way it's implemented inside
	 * of AFS - the start of the extra fields are at
	 * sizeof(struct rx_ackPacket) - RX_MAXACKS + nAcks, which _isn't_
	 * the exact start of the ack array.  Sigh.  That's why we aren't
	 * using bp, but instead use rxa->acks[].  But nAcks gets added
	 * to bp after this, so bp ends up at the right spot.  Go figure.
	 */

	nAcks = GET_U_1(rxa->nAcks);
	if (nAcks != 0) {

		ND_TCHECK_LEN(bp, nAcks);

		/*
		 * Sigh, this is gross, but it seems to work to collapse
		 * ranges correctly.
		 */

		for (i = 0, start = last = -2; i < nAcks; i++)
			if (GET_U_1(bp + i) == RX_ACK_TYPE_ACK) {

				/*
				 * I figured this deserved _some_ explanation.
				 * First, print "acked" and the packet seq
				 * number if this is the first time we've
				 * seen an acked packet.
				 */

				if (last == -2) {
					ND_PRINT(" acked %u", firstPacket + i);
					start = i;
				}

				/*
				 * Otherwise, if there is a skip in
				 * the range (such as an nacked packet in
				 * the middle of some acked packets),
				 * then print the current packet number
				 * separated from the last number by
				 * a comma.
				 */

				else if (last != i - 1) {
					ND_PRINT(",%u", firstPacket + i);
					start = i;
				}

				/*
				 * We always set last to the value of
				 * the last ack we saw.  Conversely, start
				 * is set to the value of the first ack
				 * we saw in a range.
				 */

				last = i;

				/*
				 * Okay, this bit a code gets executed when
				 * we hit a nack ... in _this_ case we
				 * want to print out the range of packets
				 * that were acked, so we need to print
				 * the _previous_ packet number separated
				 * from the first by a dash (-).  Since we
				 * already printed the first packet above,
				 * just print the final packet.  Don't
				 * do this if there will be a single-length
				 * range.
				 */
			} else if (last == i - 1 && start != last)
				ND_PRINT("-%u", firstPacket + i - 1);

		/*
		 * So, what's going on here?  We ran off the end of the
		 * ack list, and if we got a range we need to finish it up.
		 * So we need to determine if the last packet in the list
		 * was an ack (if so, then last will be set to it) and
		 * we need to see if the last range didn't start with the
		 * last packet (because if it _did_, then that would mean
		 * that the packet number has already been printed and
		 * we don't need to print it again).
		 */

		if (last == i - 1 && start != last)
			ND_PRINT("-%u", firstPacket + i - 1);

		/*
		 * Same as above, just without comments
		 */

		for (i = 0, start = last = -2; i < nAcks; i++)
			if (GET_U_1(bp + i) == RX_ACK_TYPE_NACK) {
				if (last == -2) {
					ND_PRINT(" nacked %u", firstPacket + i);
					start = i;
				} else if (last != i - 1) {
					ND_PRINT(",%u", firstPacket + i);
					start = i;
				}
				last = i;
			} else if (last == i - 1 && start != last)
				ND_PRINT("-%u", firstPacket + i - 1);

		if (last == i - 1 && start != last)
			ND_PRINT("-%u", firstPacket + i - 1);

		bp += nAcks;
	}

	/* Padding. */
	bp += 3;

	/*
	 * These are optional fields; depending on your version of AFS,
	 * you may or may not see them
	 */

#define TRUNCRET(n)	if (ndo->ndo_snapend - bp + 1 <= n) return;

	if (ndo->ndo_vflag > 1) {
		TRUNCRET(4);
		ND_PRINT(" ifmtu");
		UINTOUT();

		TRUNCRET(4);
		ND_PRINT(" maxmtu");
		UINTOUT();

		TRUNCRET(4);
		ND_PRINT(" rwind");
		UINTOUT();

		TRUNCRET(4);
		ND_PRINT(" maxpackets");
		UINTOUT();
	}

	return;

trunc:
	ND_PRINT(" [|ack]");
}
#undef TRUNCRET
