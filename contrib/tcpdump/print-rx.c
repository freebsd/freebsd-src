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
 *
 * $FreeBSD$
 */

#ifndef lint
static const char rcsid[] =
    "@(#) $Header: /tcpdump/master/tcpdump/print-rx.c,v 1.5.2.1 2000/01/11 06:58:27 fenner Exp $";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

#include "interface.h"
#include "addrtoname.h"

#undef NOERROR					/* Solaris sucks */
#include <arpa/nameser.h>

#include "rx.h"

static struct tok rx_types[] = {
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

static struct tok rx_flags[] = {
	{ RX_CLIENT_INITIATED,	"client-init" },
	{ RX_REQUEST_ACK,	"req-ack" },
	{ RX_LAST_PACKET,	"last-pckt" },
	{ RX_MORE_PACKETS,	"more-pckts" },
	{ RX_FREE_PACKET,	"free-pckt" }
};

static struct tok fs_req[] = {
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
	{ 0,		NULL },
};

static struct tok cb_req[] = {
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
	{ 0,		NULL },
};

static struct tok pt_req[] = {
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
	{ 0,		NULL },
};

static struct tok vldb_req[] = {
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
	{ 0,		NULL },
};

static struct tok kauth_req[] = {
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

static struct tok vol_req[] = {
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
	{ 0,		NULL },
};

static struct tok bos_req[] = {
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
	{ 0,		NULL },
};

static struct tok ubik_req[] = {
	{ 10000,	"vote-beacon" },
	{ 10001,	"vote-debug-old" },
	{ 10002,	"vote-sdebug-old" },
	{ 10003,	"vote-getsyncsite" },
	{ 10004,	"vote-debug" },
	{ 10005,	"vote-sdebug" },
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
#define VOTE_HIGH	10005
#define DISK_LOW	20000
#define DISK_HIGH	20013

static struct tok cb_types[] = {
	{ 1,		"exclusive" },
	{ 2,		"shared" },
	{ 3,		"dropped" },
	{ 0,		NULL },
};

static struct tok ubik_lock_types[] = {
	{ 1,		"read" },
	{ 2,		"write" },
	{ 3,		"wait" },
	{ 0,		NULL },
};

static char *voltype[] = { "read-write", "read-only", "backup" };

/*
 * Cache entries we keep around so we can figure out the RX opcode
 * numbers for replies.  This allows us to make sense of RX reply packets.
 */

struct rx_cache_entry {
	u_int32_t	callnum;	/* Call number (net order) */
	struct in_addr	client;		/* client IP address (net order) */
	struct in_addr	server;		/* server IP address (net order) */
	int		dport;		/* server port (host order) */
	u_short		serviceId;	/* Service identifier (net order) */
	u_int32_t	opcode;		/* RX opcode (host order) */
};

#define RX_CACHE_SIZE	64

static struct rx_cache_entry	rx_cache[RX_CACHE_SIZE];

static int	rx_cache_next = 0;
static int	rx_cache_hint = 0;
static void	rx_cache_insert(const u_char *, const struct ip *, int, int);
static int	rx_cache_find(const struct rx_header *, const struct ip *,
			      int, int32_t *);

static void fs_print(const u_char *, int);
static void fs_reply_print(const u_char *, int, int32_t);
static void acl_print(u_char *, int, u_char *);
static void cb_print(const u_char *, int);
static void cb_reply_print(const u_char *, int, int32_t);
static void prot_print(const u_char *, int);
static void prot_reply_print(const u_char *, int, int32_t);
static void vldb_print(const u_char *, int);
static void vldb_reply_print(const u_char *, int, int32_t);
static void kauth_print(const u_char *, int);
static void kauth_reply_print(const u_char *, int, int32_t);
static void vol_print(const u_char *, int);
static void vol_reply_print(const u_char *, int, int32_t);
static void bos_print(const u_char *, int);
static void bos_reply_print(const u_char *, int, int32_t);
static void ubik_print(const u_char *, int);
static void ubik_reply_print(const u_char *, int, int32_t);

static int is_ubik(u_int32_t);

/*
 * Handle the rx-level packet.  See if we know what port it's going to so
 * we can peek at the afs call inside
 */

void
rx_print(register const u_char *bp, int length, int sport, int dport,
	 u_char *bp2)
{
	register struct rx_header *rxh;
	int i;
	int32_t opcode;

	if (snapend - bp < sizeof (struct rx_header)) {
		printf(" [|rx] (%d)", length);
		return;
	}

	rxh = (struct rx_header *) bp;

	printf(" rx %s", tok2str(rx_types, "type %d", rxh->type));

	if (vflag > 1) {
		int firstflag = 0;
		printf(" cid %08x call# %d seq %d ser %d",
		       (int) ntohl(rxh->cid),
		       (int) ntohl(rxh->callNumber),
		       (int) ntohl(rxh->seq),
		       (int) ntohl(rxh->serial));
		if (vflag > 2)
			printf(" secindex %d serviceid %hu",
				(int) rxh->securityIndex,
				ntohs(rxh->serviceId));
		for (i = 0; i < NUM_RX_FLAGS; i++) {
			if (rxh->flags & rx_flags[i].v) {
				if (!firstflag) {
					firstflag = 1;
					printf(" ");
				} else {
					printf(",");
				}
				printf("<%s>", rx_flags[i].s);
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

	if (rxh->type == RX_PACKET_TYPE_DATA && ntohl(rxh->seq) == 1 &&
	    rxh->flags & RX_CLIENT_INITIATED) {

		/*
		 * Insert this call into the call cache table, so we
		 * have a chance to print out replies
		 */

		rx_cache_insert(bp, (const struct ip *) bp2, dport, length);

		switch (dport) {
			case FS_RX_PORT:	/* AFS file service */
				fs_print(bp, length);
				break;
			case CB_RX_PORT:	/* AFS callback service */
				cb_print(bp, length);
				break;
			case PROT_RX_PORT:	/* AFS protection service */
				prot_print(bp, length);
				break;
			case VLDB_RX_PORT:	/* AFS VLDB service */
				vldb_print(bp, length);
				break;
			case KAUTH_RX_PORT:	/* AFS Kerberos auth service */
				kauth_print(bp, length);
				break;
			case VOL_RX_PORT:	/* AFS Volume service */
				vol_print(bp, length);
				break;
			case BOS_RX_PORT:	/* AFS BOS service */
				bos_print(bp, length);
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

	} else if (((rxh->type == RX_PACKET_TYPE_DATA &&
						ntohl(rxh->seq) == 1) ||
		    rxh->type == RX_PACKET_TYPE_ABORT) &&
		   (rxh->flags & RX_CLIENT_INITIATED) == 0 &&
		   rx_cache_find(rxh, (const struct ip *) bp2,
				 sport, &opcode)) {

		switch (sport) {
			case FS_RX_PORT:	/* AFS file service */
				fs_reply_print(bp, length, opcode);
				break;
			case CB_RX_PORT:	/* AFS callback service */
				cb_reply_print(bp, length, opcode);
				break;
			case PROT_RX_PORT:	/* AFS PT service */
				prot_reply_print(bp, length, opcode);
				break;
			case VLDB_RX_PORT:	/* AFS VLDB service */
				vldb_reply_print(bp, length, opcode);
				break;
			case KAUTH_RX_PORT:	/* AFS Kerberos auth service */
				kauth_reply_print(bp, length, opcode);
				break;
			case VOL_RX_PORT:	/* AFS Volume service */
				vol_reply_print(bp, length, opcode);
				break;
			case BOS_RX_PORT:	/* AFS BOS service */
				bos_reply_print(bp, length, opcode);
				break;
			default:
				;
		}
	}


	printf(" (%d)", length);
}

/*
 * Insert an entry into the cache.  Taken from print-nfs.c
 */

static void
rx_cache_insert(const u_char *bp, const struct ip *ip, int dport,
		int length)
{
	struct rx_cache_entry *rxent;
	const struct rx_header *rxh = (const struct rx_header *) bp;

	if (snapend - bp + 1 <= sizeof(struct rx_header) + sizeof(int32_t))
		return;

	rxent = &rx_cache[rx_cache_next];

	if (++rx_cache_next >= RX_CACHE_SIZE)
		rx_cache_next = 0;
	
	rxent->callnum = rxh->callNumber;
	rxent->client = ip->ip_src;
	rxent->server = ip->ip_dst;
	rxent->dport = dport;
	rxent->serviceId = rxh->serviceId;
	rxent->opcode = ntohl(*((int *) (bp + sizeof(struct rx_header))));
}

/*
 * Lookup an entry in the cache.  Also taken from print-nfs.c
 *
 * Note that because this is a reply, we're looking at the _source_
 * port.
 */

static int
rx_cache_find(const struct rx_header *rxh, const struct ip *ip, int sport,
	      int32_t *opcode)
{
	int i;
	struct rx_cache_entry *rxent;
	u_int32_t clip = ip->ip_dst.s_addr;
	u_int32_t sip = ip->ip_src.s_addr;

	/* Start the search where we last left off */

	i = rx_cache_hint;
	do {
		rxent = &rx_cache[i];
		if (rxent->callnum == rxh->callNumber &&
		    rxent->client.s_addr == clip &&
		    rxent->server.s_addr == sip && 
		    rxent->serviceId == rxh->serviceId &&
		    rxent->dport == sport) {

			/* We got a match! */

			rx_cache_hint = i;
			*opcode = rxent->opcode;
			return(1);
		}
		if (++i > RX_CACHE_SIZE)
			i = 0;
	} while (i != rx_cache_hint);

	/* Our search failed */
	return(0);
}

/*
 * These extrememly grody macros handle the printing of various AFS stuff.
 */

#define TRUNC(n) if (snapend - bp + 1 <= n) goto trunc;
#define FIDOUT() { unsigned long n1, n2, n3; \
			TRUNC(sizeof(int32_t) * 3); \
			n1 = ntohl(*((int *) bp)); \
			bp += sizeof(int32_t); \
			n2 = ntohl(*((int *) bp)); \
			bp += sizeof(int32_t); \
			n3 = ntohl(*((int *) bp)); \
			bp += sizeof(int32_t); \
			printf(" fid %d/%d/%d", (int) n1, (int) n2, (int) n3); \
		}

#define STROUT(MAX) { int i; \
			TRUNC(sizeof(int32_t)); \
			i = (int) ntohl(*((int *) bp)); \
			bp += sizeof(int32_t); \
			TRUNC(i); \
			strncpy(s, bp, min(MAX, i)); \
			s[i] = '\0'; \
			printf(" \"%s\"", s); \
			bp += ((i + sizeof(int32_t) - 1) / sizeof(int32_t)) * sizeof(int32_t); \
		}

#define INTOUT() { int i; \
			TRUNC(sizeof(int32_t)); \
			i = (int) ntohl(*((int *) bp)); \
			bp += sizeof(int32_t); \
			printf(" %d", i); \
		}

#define UINTOUT() { unsigned long i; \
			TRUNC(sizeof(int32_t)); \
			i = ntohl(*((int *) bp)); \
			bp += sizeof(int32_t); \
			printf(" %lu", i); \
		}

#define DATEOUT() { time_t t; struct tm *tm; char str[256]; \
			TRUNC(sizeof(int32_t)); \
			t = (time_t) ntohl(*((int *) bp)); \
			bp += sizeof(int32_t); \
			tm = localtime(&t); \
			strftime(str, 256, "%Y/%m/%d %T", tm); \
			printf(" %s", str); \
		}

#define STOREATTROUT() { unsigned long mask, i; \
			TRUNC((sizeof(int32_t)*6)); \
			mask = ntohl(*((int *) bp)); bp += sizeof(int32_t); \
			if (mask) printf (" StoreStatus"); \
  		        if (mask & 1) { printf(" date"); DATEOUT(); } \
			else bp += sizeof(int32_t); \
			i = ntohl(*((int *) bp)); bp += sizeof(int32_t); \
  		        if (mask & 2) printf(" owner %lu", i);  \
			i = ntohl(*((int32_t *) bp)); bp += sizeof(int32_t); \
  		        if (mask & 4) printf(" group %lu", i); \
			i = ntohl(*((int32_t *) bp)); bp += sizeof(int32_t); \
  		        if (mask & 8) printf(" mode %lo", i & 07777); \
			i = ntohl(*((int32_t *) bp)); bp += sizeof(int32_t); \
  		        if (mask & 16) printf(" segsize %lu", i); \
			/* undocumented in 3.3 docu */ \
  		        if (mask & 1024) printf(" fsync");  \
		}

#define UBIK_VERSIONOUT() {int32_t epoch; int32_t counter; \
			TRUNC(sizeof(int32_t) * 2); \
			epoch = ntohl(*((int *) bp)); \
			bp += sizeof(int32_t); \
			counter = ntohl(*((int *) bp)); \
			bp += sizeof(int32_t); \
			printf(" %d.%d", epoch, counter); \
		}

#define AFSUUIDOUT() {u_int32_t temp; int i; \
			TRUNC(11*sizeof(u_int32_t)); \
			temp = ntohl(*((int *) bp)); \
			bp += sizeof(u_int32_t); \
			printf(" %08x", temp); \
			temp = ntohl(*((int *) bp)); \
			bp += sizeof(u_int32_t); \
			printf("%04x", temp); \
			temp = ntohl(*((int *) bp)); \
			bp += sizeof(u_int32_t); \
			printf("%04x", temp); \
			for (i = 0; i < 8; i++) { \
				temp = ntohl(*((int *) bp)); \
				bp += sizeof(u_int32_t); \
				printf("%02x", (unsigned char) temp); \
			} \
		}

/*
 * This is the sickest one of all
 */

#define VECOUT(MAX) { char *sp; \
			int k; \
			TRUNC(MAX * sizeof(int32_t)); \
			sp = s; \
			for (k = 0; k < MAX; k++) { \
				*sp++ = (char) ntohl(*((int *) bp)); \
				bp += sizeof(int32_t); \
			} \
			s[MAX] = '\0'; \
			printf(" \"%s\"", s); \
		}

/*
 * Handle calls to the AFS file service (fs)
 */

void
fs_print(register const u_char *bp, int length)
{
	int fs_op;
	unsigned long i;
	char s[AFSNAMEMAX];

	if (length <= sizeof(struct rx_header))
		return;

	if (snapend - bp + 1 <= sizeof(struct rx_header) + sizeof(int32_t)) {
		goto trunc;
	}

	/*
	 * Print out the afs call we're invoking.  The table used here was
	 * gleaned from fsint/afsint.xg
	 */

	fs_op = ntohl(*((int *) (bp + sizeof(struct rx_header))));

	printf(" fs call %s", tok2str(fs_req, "op#%d", fs_op));

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
			printf(" offset");
			UINTOUT();
			printf(" length");
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
			printf(" offset");
			UINTOUT();
			printf(" length");
			UINTOUT();
			printf(" flen");
			UINTOUT();
			break;
		case 134:	/* Store ACL */
		{
			char a[AFSOPAQUEMAX];
			FIDOUT();
			TRUNC(4);
			i = ntohl(*((int *) bp));
			bp += sizeof(int32_t);
			TRUNC(i);
			strncpy(a, bp, min(AFSOPAQUEMAX, i));
			a[i] = '\0';
			acl_print((u_char *) a, sizeof(a), (u_char *) a + i);
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
			printf(" old");
			FIDOUT();
			STROUT(AFSNAMEMAX);
			printf(" new");
			FIDOUT();
			STROUT(AFSNAMEMAX);
			break;
		case 139:	/* Symlink */
			FIDOUT();
			STROUT(AFSNAMEMAX);
			printf(" link to");
			STROUT(AFSNAMEMAX);
			break;
		case 140:	/* Link */
			FIDOUT();
			STROUT(AFSNAMEMAX);
			printf(" link to");
			FIDOUT();
			break;
		case 148:	/* Get volume info */
			STROUT(AFSNAMEMAX);
			break;
		case 149:	/* Get volume stats */
		case 150:	/* Set volume stats */
			printf(" volid");
			UINTOUT();
			break;
		case 154:	/* New get volume info */
			printf(" volname");
			STROUT(AFSNAMEMAX);
			break;
		case 155:	/* Bulk stat */
		{
			unsigned long j;
			TRUNC(4);
			j = ntohl(*((int *) bp));
			bp += sizeof(int32_t);

			for (i = 0; i < j; i++) {
				FIDOUT();
				if (i != j - 1)
					printf(",");
			}
			if (j == 0)
				printf(" <none!>");
		}
		default:
			;
	}

	return;

trunc:
	printf(" [|fs]");
}

/*
 * Handle replies to the AFS file service
 */

static void
fs_reply_print(register const u_char *bp, int length, int32_t opcode)
{
	unsigned long i;
	char s[AFSNAMEMAX];
	struct rx_header *rxh;

	if (length <= sizeof(struct rx_header))
		return;

	rxh = (struct rx_header *) bp;

	/*
	 * Print out the afs call we're invoking.  The table used here was
	 * gleaned from fsint/afsint.xg
	 */

	printf(" fs reply %s", tok2str(fs_req, "op#%d", opcode));

	bp += sizeof(struct rx_header);

	/*
	 * If it was a data packet, interpret the response
	 */

	if (rxh->type == RX_PACKET_TYPE_DATA)
		switch (opcode) {
		case 131:	/* Fetch ACL */
		{
			char a[AFSOPAQUEMAX];
			TRUNC(4);
			i = ntohl(*((int *) bp));
			bp += sizeof(int32_t);
			TRUNC(i);
			strncpy(a, bp, min(AFSOPAQUEMAX, i));
			a[i] = '\0';
			acl_print((u_char *) a, sizeof(a), (u_char *) a + i);
			break;
		}
		case 137:	/* Create file */
		case 141:	/* MakeDir */
			printf(" new");
			FIDOUT();
			break;
		case 151:	/* Get root volume */
			printf(" root volume");
			STROUT(AFSNAMEMAX);
			break;
		case 153:	/* Get time */
			DATEOUT();
			break;
		default:
			;
		}
	else {
		/*
		 * Otherwise, just print out the return code
		 */
		printf(" errcode");
		INTOUT();
	}

	return;

trunc:
	printf(" [|fs]");
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
 * ASCII strings containing the UID/PTS record and and a ascii number
 * representing a logical OR of all the ACL permission bits
 */

static void
acl_print(u_char *s, int maxsize, u_char *end)
{
	int pos, neg, acl;
	int n, i;
	char *user;

	if ((user = (char *)malloc(maxsize)) == NULL)
		return;

	if (sscanf((char *) s, "%d %d\n%n", &pos, &neg, &n) != 2)
		goto finish;
	
	s += n;

	if (s > end)
		goto finish;

	/*
	 * This wacky order preserves the order used by the "fs" command
	 */

#define ACLOUT(acl) \
	if (acl & PRSFS_READ) \
		printf("r"); \
	if (acl & PRSFS_LOOKUP) \
		printf("l"); \
	if (acl & PRSFS_INSERT) \
		printf("i"); \
	if (acl & PRSFS_DELETE) \
		printf("d"); \
	if (acl & PRSFS_WRITE) \
		printf("w"); \
	if (acl & PRSFS_LOCK) \
		printf("k"); \
	if (acl & PRSFS_ADMINISTER) \
		printf("a");

	for (i = 0; i < pos; i++) {
		if (sscanf((char *) s, "%s %d\n%n", user, &acl, &n) != 2)
			goto finish;
		s += n;
		printf(" +{%s ", user);
		ACLOUT(acl);
		printf("}");
		if (s > end)
			goto finish;
	}

	for (i = 0; i < neg; i++) {
		if (sscanf((char *) s, "%s %d\n%n", user, &acl, &n) != 2)
			goto finish;
		s += n;
		printf(" -{%s ", user);
		ACLOUT(acl);
		printf("}");
		if (s > end)
			goto finish;
	}

finish:
	free(user);
	return;
}

#undef ACLOUT

/*
 * Handle calls to the AFS callback service
 */

static void
cb_print(register const u_char *bp, int length)
{
	int cb_op;
	unsigned long i;

	if (length <= sizeof(struct rx_header))
		return;

	if (snapend - bp + 1 <= sizeof(struct rx_header) + sizeof(int32_t)) {
		goto trunc;
	}

	/*
	 * Print out the afs call we're invoking.  The table used here was
	 * gleaned from fsint/afscbint.xg
	 */

	cb_op = ntohl(*((int *) (bp + sizeof(struct rx_header))));

	printf(" cb call %s", tok2str(cb_req, "op#%d", cb_op));

	bp += sizeof(struct rx_header) + 4;

	/*
	 * Print out the afs call we're invoking.  The table used here was
	 * gleaned from fsint/afscbint.xg
	 */

	switch (cb_op) {
		case 204:		/* Callback */
		{
			unsigned long j, t;
			TRUNC(4);
			j = ntohl(*((int *) bp));
			bp += sizeof(int32_t);

			for (i = 0; i < j; i++) {
				FIDOUT();
				if (i != j - 1)
					printf(",");
			}

			if (j == 0)
				printf(" <none!>");

			j = ntohl(*((int *) bp));
			bp += sizeof(int32_t);

			if (j != 0)
				printf(";");

			for (i = 0; i < j; i++) {
				printf(" ver");
				INTOUT();
				printf(" expires");
				DATEOUT();
				TRUNC(4);
				t = ntohl(*((int *) bp));
				bp += sizeof(int32_t);
				tok2str(cb_types, "type %d", t);
			}
		}
		case 214: {
			printf(" afsuuid");
			AFSUUIDOUT();
			break;
		}
		default:
			;
	}

	return;

trunc:
	printf(" [|cb]");
}

/*
 * Handle replies to the AFS Callback Service
 */

static void
cb_reply_print(register const u_char *bp, int length, int32_t opcode)
{
	struct rx_header *rxh;

	if (length <= sizeof(struct rx_header))
		return;

	rxh = (struct rx_header *) bp;

	/*
	 * Print out the afs call we're invoking.  The table used here was
	 * gleaned from fsint/afscbint.xg
	 */
	
	printf(" cb reply %s", tok2str(cb_req, "op#%d", opcode));

	bp += sizeof(struct rx_header);

	/*
	 * If it was a data packet, interpret the response.
	 */

	if (rxh->type == RX_PACKET_TYPE_DATA)
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
		printf(" errcode");
		INTOUT();
	}

	return;

trunc:
	printf(" [|cb]");
}

/*
 * Handle calls to the AFS protection database server
 */

static void
prot_print(register const u_char *bp, int length)
{
	unsigned long i;
	char s[AFSNAMEMAX];
	int pt_op;

	if (length <= sizeof(struct rx_header))
		return;

	if (snapend - bp + 1 <= sizeof(struct rx_header) + sizeof(int32_t)) {
		goto trunc;
	}

	/*
	 * Print out the afs call we're invoking.  The table used here was
	 * gleaned from ptserver/ptint.xg
	 */

	pt_op = ntohl(*((int *) (bp + sizeof(struct rx_header))));

	printf(" pt");

	if (is_ubik(pt_op)) {
		ubik_print(bp, length);
		return;
	}

	printf(" call %s", tok2str(pt_req, "op#%d", pt_op));

	/*
	 * Decode some of the arguments to the PT calls
	 */

	bp += sizeof(struct rx_header) + 4;

	switch (pt_op) {
		case 500:	/* I New User */
			STROUT(PRNAMEMAX);
			printf(" id");
			INTOUT();
			printf(" oldid");
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
			printf(" id");
			INTOUT();
			break;
		case 502:	/* Dump entry */
			printf(" pos");
			INTOUT();
			break;
		case 503:	/* Add to group */
		case 507:	/* Remove from group */
		case 515:	/* Is a member of? */
			printf(" uid");
			INTOUT();
			printf(" gid");
			INTOUT();
			break;
		case 504:	/* Name to ID */
		{
			unsigned long j;
			TRUNC(4);
			j = ntohl(*((int *) bp));
			bp += sizeof(int32_t);

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
				printf(" <none!>");
		}
			break;
		case 505:	/* Id to name */
		{
			unsigned long j;
			printf(" ids:");
			TRUNC(4);
			i = ntohl(*((int *) bp));
			bp += sizeof(int32_t);
			for (j = 0; j < i; j++)
				INTOUT();
			if (j == 0)
				printf(" <none!>");
		}
			break;
		case 509:	/* New entry */
			STROUT(PRNAMEMAX);
			printf(" flag");
			INTOUT();
			printf(" oid");
			INTOUT();
			break;
		case 511:	/* Set max */
			printf(" id");
			INTOUT();
			printf(" gflag");
			INTOUT();
			break;
		case 513:	/* Change entry */
			printf(" id");
			INTOUT();
			STROUT(PRNAMEMAX);
			printf(" oldid");
			INTOUT();
			printf(" newid");
			INTOUT();
			break;
		case 520:	/* Update entry */
			printf(" id");
			INTOUT();
			STROUT(PRNAMEMAX);
			break;
		default:
			;
	}


	return;

trunc:
	printf(" [|pt]");
}

/*
 * Handle replies to the AFS protection service
 */

static void
prot_reply_print(register const u_char *bp, int length, int32_t opcode)
{
	struct rx_header *rxh;
	unsigned long i;
	char s[AFSNAMEMAX];

	if (length < sizeof(struct rx_header))
		return;

	rxh = (struct rx_header *) bp;

	/*
	 * Print out the afs call we're invoking.  The table used here was
	 * gleaned from ptserver/ptint.xg.  Check to see if it's a
	 * Ubik call, however.
	 */

	printf(" pt");

	if (is_ubik(opcode)) {
		ubik_reply_print(bp, length, opcode);
		return;
	}

	printf(" reply %s", tok2str(pt_req, "op#%d", opcode));

	bp += sizeof(struct rx_header);

	/*
	 * If it was a data packet, interpret the response
	 */

	if (rxh->type == RX_PACKET_TYPE_DATA)
		switch (opcode) {
		case 504:		/* Name to ID */
		{
			unsigned long j;
			printf(" ids:");
			TRUNC(4);
			i = ntohl(*((int *) bp));
			bp += sizeof(int32_t);
			for (j = 0; j < i; j++)
				INTOUT();
			if (j == 0)
				printf(" <none!>");
		}
			break;
		case 505:		/* ID to name */
		{
			unsigned long j;
			TRUNC(4);
			j = ntohl(*((int *) bp));
			bp += sizeof(int32_t);

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
				printf(" <none!>");
		}
			break;
		case 508:		/* Get CPS */
		case 514:		/* List elements */
		case 517:		/* List owned */
		case 518:		/* Get CPS2 */
		case 519:		/* Get host CPS */
		{
			unsigned long j;
			TRUNC(4);
			j = ntohl(*((int *) bp));
			bp += sizeof(int32_t);
			for (i = 0; i < j; i++) {
				INTOUT();
			}
			if (j == 0)
				printf(" <none!>");
		}
			break;
		case 510:		/* List max */
			printf(" maxuid");
			INTOUT();
			printf(" maxgid");
			INTOUT();
			break;
		default:
			;
		}
	else {
		/*
		 * Otherwise, just print out the return code
		 */
		printf(" errcode");
		INTOUT();
	}

	return;

trunc:
	printf(" [|pt]");
}

/*
 * Handle calls to the AFS volume location database service
 */

static void
vldb_print(register const u_char *bp, int length)
{
	int vldb_op;
	unsigned long i;
	char s[AFSNAMEMAX];

	if (length <= sizeof(struct rx_header))
		return;

	if (snapend - bp + 1 <= sizeof(struct rx_header) + sizeof(int32_t)) {
		goto trunc;
	}

	/*
	 * Print out the afs call we're invoking.  The table used here was
	 * gleaned from vlserver/vldbint.xg
	 */

	vldb_op = ntohl(*((int *) (bp + sizeof(struct rx_header))));

	printf(" vldb");

	if (is_ubik(vldb_op)) {
		ubik_print(bp, length);
		return;
	}
	printf(" call %s", tok2str(vldb_req, "op#%d", vldb_op));

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
			printf(" volid");
			INTOUT();
			TRUNC(sizeof(int32_t));
			i = ntohl(*((int *) bp));
			bp += sizeof(int32_t);
			if (i <= 2)
				printf(" type %s", voltype[i]);
			break;
		case 504:	/* Get entry by name */
		case 519:	/* Get entry by name N */
		case 524:	/* Update entry by name */
		case 527:	/* Get entry by name U */
			STROUT(VLNAMEMAX);
			break;
		case 505:	/* Get new vol id */
			printf(" bump");
			INTOUT();
			break;
		case 506:	/* Replace entry */
		case 520:	/* Replace entry N */
			printf(" volid");
			INTOUT();
			TRUNC(sizeof(int32_t));
			i = ntohl(*((int *) bp));
			bp += sizeof(int32_t);
			if (i <= 2)
				printf(" type %s", voltype[i]);
			VECOUT(VLNAMEMAX);
			break;
		case 510:	/* List entry */
		case 521:	/* List entry N */
			printf(" index");
			INTOUT();
			break;
		default:
			;
	}

	return;

trunc:
	printf(" [|vldb]");
}

/*
 * Handle replies to the AFS volume location database service
 */

static void
vldb_reply_print(register const u_char *bp, int length, int32_t opcode)
{
	struct rx_header *rxh;
	unsigned long i;
	char s[AFSNAMEMAX];

	if (length < sizeof(struct rx_header))
		return;

	rxh = (struct rx_header *) bp;

	/*
	 * Print out the afs call we're invoking.  The table used here was
	 * gleaned from vlserver/vldbint.xg.  Check to see if it's a
	 * Ubik call, however.
	 */

	printf(" vldb");

	if (is_ubik(opcode)) {
		ubik_reply_print(bp, length, opcode);
		return;
	}

	printf(" reply %s", tok2str(vldb_req, "op#%d", opcode));

	bp += sizeof(struct rx_header);

	/*
	 * If it was a data packet, interpret the response
	 */

	if (rxh->type == RX_PACKET_TYPE_DATA)
		switch (opcode) {
		case 510:	/* List entry */
			printf(" count");
			INTOUT();
			printf(" nextindex");
			INTOUT();
		case 503:	/* Get entry by id */
		case 504:	/* Get entry by name */
		{	unsigned long nservers, j;
			VECOUT(VLNAMEMAX);
			TRUNC(sizeof(int32_t));
			bp += sizeof(int32_t);
			printf(" numservers");
			TRUNC(sizeof(int32_t));
			nservers = ntohl(*((int *) bp));
			bp += sizeof(int32_t);
			printf(" %lu", nservers);
			printf(" servers");
			for (i = 0; i < 8; i++) {
				TRUNC(sizeof(int32_t));
				if (i < nservers)
					printf(" %s",
					   inet_ntoa(*((struct in_addr *) bp)));
				bp += sizeof(int32_t);
			}
			printf(" partitions");
			for (i = 0; i < 8; i++) {
				TRUNC(sizeof(int32_t));
				j = ntohl(*((int *) bp));
				if (i < nservers && j <= 26)
					printf(" %c", 'a' + (int)j);
				else if (i < nservers)
					printf(" %lu", j);
				bp += sizeof(int32_t);
			}
			TRUNC(8 * sizeof(int32_t));
			bp += 8 * sizeof(int32_t);
			printf(" rwvol");
			UINTOUT();
			printf(" rovol");
			UINTOUT();
			printf(" backup");
			UINTOUT();
		}
			break;
		case 505:	/* Get new volume ID */
			printf(" newvol");
			UINTOUT();
			break;
		case 521:	/* List entry */
		case 529:	/* List entry U */
			printf(" count");
			INTOUT();
			printf(" nextindex");
			INTOUT();
		case 518:	/* Get entry by ID N */
		case 519:	/* Get entry by name N */
		{	unsigned long nservers, j;
			VECOUT(VLNAMEMAX);
			printf(" numservers");
			TRUNC(sizeof(int32_t));
			nservers = ntohl(*((int *) bp));
			bp += sizeof(int32_t);
			printf(" %lu", nservers);
			printf(" servers");
			for (i = 0; i < 13; i++) {
				TRUNC(sizeof(int32_t));
				if (i < nservers)
					printf(" %s",
					   inet_ntoa(*((struct in_addr *) bp)));
				bp += sizeof(int32_t);
			}
			printf(" partitions");
			for (i = 0; i < 13; i++) {
				TRUNC(sizeof(int32_t));
				j = ntohl(*((int *) bp));
				if (i < nservers && j <= 26)
					printf(" %c", 'a' + (int)j);
				else if (i < nservers)
					printf(" %lu", j);
				bp += sizeof(int32_t);
			}
			TRUNC(13 * sizeof(int32_t));
			bp += 13 * sizeof(int32_t);
			printf(" rwvol");
			UINTOUT();
			printf(" rovol");
			UINTOUT();
			printf(" backup");
			UINTOUT();
		}
			break;
		case 526:	/* Get entry by ID U */
		case 527:	/* Get entry by name U */
		{	unsigned long nservers, j;
			VECOUT(VLNAMEMAX);
			printf(" numservers");
			TRUNC(sizeof(int32_t));
			nservers = ntohl(*((int *) bp));
			bp += sizeof(int32_t);
			printf(" %lu", nservers);
			printf(" servers");
			for (i = 0; i < 13; i++) {
				if (i < nservers) {
					printf(" afsuuid");
					AFSUUIDOUT();
				} else {
					TRUNC(44);
					bp += 44;
				}
			}
			TRUNC(4 * 13);
			bp += 4 * 13;
			printf(" partitions");
			for (i = 0; i < 13; i++) {
				TRUNC(sizeof(int32_t));
				j = ntohl(*((int *) bp));
				if (i < nservers && j <= 26)
					printf(" %c", 'a' + (int)j);
				else if (i < nservers)
					printf(" %lu", j);
				bp += sizeof(int32_t);
			}
			TRUNC(13 * sizeof(int32_t));
			bp += 13 * sizeof(int32_t);
			printf(" rwvol");
			UINTOUT();
			printf(" rovol");
			UINTOUT();
			printf(" backup");
			UINTOUT();
		}
		default:
			;
		}
			
	else {
		/*
		 * Otherwise, just print out the return code
		 */
		printf(" errcode");
		INTOUT();
	}

	return;

trunc:
	printf(" [|vldb]");
}

/*
 * Handle calls to the AFS Kerberos Authentication service
 */

static void
kauth_print(register const u_char *bp, int length)
{
	int kauth_op;
	char s[AFSNAMEMAX];

	if (length <= sizeof(struct rx_header))
		return;

	if (snapend - bp + 1 <= sizeof(struct rx_header) + sizeof(int32_t)) {
		goto trunc;
	}

	/*
	 * Print out the afs call we're invoking.  The table used here was
	 * gleaned from kauth/kauth.rg
	 */

	kauth_op = ntohl(*((int *) (bp + sizeof(struct rx_header))));

	printf(" kauth");

	if (is_ubik(kauth_op)) {
		ubik_print(bp, length);
		return;
	}


	printf(" call %s", tok2str(kauth_req, "op#%d", kauth_op));

	/*
	 * Decode some of the arguments to the KA calls
	 */

	bp += sizeof(struct rx_header) + 4;

	switch (kauth_op) {
		case 1:		/* Authenticate old */;
		case 21:	/* Authenticate */
		case 22:	/* Authenticate-V2 */
		case 2:		/* Change PW */
		case 5:		/* Set fields */
		case 6:		/* Create user */
		case 7:		/* Delete user */
		case 8:		/* Get entry */
		case 14:	/* Unlock */
		case 15:	/* Lock status */
			printf(" principal");
			STROUT(KANAMEMAX);
			STROUT(KANAMEMAX);
			break;
		case 3:		/* GetTicket-old */
		case 23:	/* GetTicket */
		{
			int i;
			printf(" kvno");
			INTOUT();
			printf(" domain");
			STROUT(KANAMEMAX);
			TRUNC(sizeof(int32_t));
			i = (int) ntohl(*((int *) bp));
			bp += sizeof(int32_t);
			TRUNC(i);
			bp += i;
			printf(" principal");
			STROUT(KANAMEMAX);
			STROUT(KANAMEMAX);
			break;
		}
		case 4:		/* Set Password */
			printf(" principal");
			STROUT(KANAMEMAX);
			STROUT(KANAMEMAX);
			printf(" kvno");
			INTOUT();
			break;
		case 12:	/* Get password */
			printf(" name");
			STROUT(KANAMEMAX);
			break;
		default:
			;
	}

	return;

trunc:
	printf(" [|kauth]");
}

/*
 * Handle replies to the AFS Kerberos Authentication Service
 */

static void
kauth_reply_print(register const u_char *bp, int length, int32_t opcode)
{
	struct rx_header *rxh;

	if (length <= sizeof(struct rx_header))
		return;

	rxh = (struct rx_header *) bp;

	/*
	 * Print out the afs call we're invoking.  The table used here was
	 * gleaned from kauth/kauth.rg
	 */
	
	printf(" kauth");

	if (is_ubik(opcode)) {
		ubik_reply_print(bp, length, opcode);
		return;
	}

	printf(" reply %s", tok2str(kauth_req, "op#%d", opcode));

	bp += sizeof(struct rx_header);

	/*
	 * If it was a data packet, interpret the response.
	 */

	if (rxh->type == RX_PACKET_TYPE_DATA)
		/* Well, no, not really.  Leave this for later */
		;
	else {
		/*
		 * Otherwise, just print out the return code
		 */
		printf(" errcode");
		INTOUT();
	}

	return;

trunc:
	printf(" [|kauth]");
}

/*
 * Handle calls to the AFS Volume location service
 */

static void
vol_print(register const u_char *bp, int length)
{
	int vol_op;

	if (length <= sizeof(struct rx_header))
		return;

	if (snapend - bp + 1 <= sizeof(struct rx_header) + sizeof(int32_t)) {
		goto trunc;
	}

	/*
	 * Print out the afs call we're invoking.  The table used here was
	 * gleaned from volser/volint.xg
	 */

	vol_op = ntohl(*((int *) (bp + sizeof(struct rx_header))));

	printf(" vol call %s", tok2str(vol_req, "op#%d", vol_op));

	/*
	 * Normally there would be a switch statement here to decode the
	 * arguments to the AFS call, but since I don't have access to
	 * an AFS server (yet) and I'm not an AFS admin, I can't
	 * test any of these calls.  Leave this blank for now.
	 */

	return;

trunc:
	printf(" [|vol]");
}

/*
 * Handle replies to the AFS Volume Service
 */

static void
vol_reply_print(register const u_char *bp, int length, int32_t opcode)
{
	struct rx_header *rxh;

	if (length <= sizeof(struct rx_header))
		return;

	rxh = (struct rx_header *) bp;

	/*
	 * Print out the afs call we're invoking.  The table used here was
	 * gleaned from volser/volint.xg
	 */
	
	printf(" vol reply %s", tok2str(vol_req, "op#%d", opcode));

	bp += sizeof(struct rx_header);

	/*
	 * If it was a data packet, interpret the response.
	 */

	if (rxh->type == RX_PACKET_TYPE_DATA)
		/* Well, no, not really.  Leave this for later */
		;
	else {
		/*
		 * Otherwise, just print out the return code
		 */
		printf(" errcode");
		INTOUT();
	}

	return;

trunc:
	printf(" [|vol]");
}

/*
 * Handle calls to the AFS BOS service
 */

static void
bos_print(register const u_char *bp, int length)
{
	int bos_op;
	char s[BOSNAMEMAX];

	if (length <= sizeof(struct rx_header))
		return;

	if (snapend - bp + 1 <= sizeof(struct rx_header) + sizeof(int32_t)) {
		goto trunc;
	}

	/*
	 * Print out the afs call we're invoking.  The table used here was
	 * gleaned from bozo/bosint.xg
	 */

	bos_op = ntohl(*((int *) (bp + sizeof(struct rx_header))));

	printf(" bos call %s", tok2str(bos_req, "op#%d", bos_op));

	/*
	 * Decode some of the arguments to the BOS calls
	 */

	bp += sizeof(struct rx_header) + 4;

	switch (bos_op) {
		case 80:	/* Create B node */
			printf(" type");
			STROUT(BOSNAMEMAX);
			printf(" instance");
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
			printf(" status");
			INTOUT();
			break;
		case 86:	/* Get instance parm */
			STROUT(BOSNAMEMAX);
			printf(" num");
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
			printf(" size");
			INTOUT();
			printf(" flags");
			INTOUT();
			printf(" date");
			INTOUT();
			break;
		default:
			;
	}

	return;

trunc:
	printf(" [|bos]");
}

/*
 * Handle replies to the AFS BOS Service
 */

static void
bos_reply_print(register const u_char *bp, int length, int32_t opcode)
{
	struct rx_header *rxh;

	if (length <= sizeof(struct rx_header))
		return;

	rxh = (struct rx_header *) bp;

	/*
	 * Print out the afs call we're invoking.  The table used here was
	 * gleaned from volser/volint.xg
	 */
	
	printf(" bos reply %s", tok2str(bos_req, "op#%d", opcode));

	bp += sizeof(struct rx_header);

	/*
	 * If it was a data packet, interpret the response.
	 */

	if (rxh->type == RX_PACKET_TYPE_DATA)
		/* Well, no, not really.  Leave this for later */
		;
	else {
		/*
		 * Otherwise, just print out the return code
		 */
		printf(" errcode");
		INTOUT();
	}

	return;

trunc:
	printf(" [|bos]");
}

/*
 * Check to see if this is a Ubik opcode.
 */

static int
is_ubik(u_int32_t opcode)
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
ubik_print(register const u_char *bp, int length)
{
	int ubik_op;
	int32_t temp;

	/*
	 * Print out the afs call we're invoking.  The table used here was
	 * gleaned from ubik/ubik_int.xg
	 */

	ubik_op = ntohl(*((int *) (bp + sizeof(struct rx_header))));

	printf(" ubik call %s", tok2str(ubik_req, "op#%d", ubik_op));

	/*
	 * Decode some of the arguments to the Ubik calls
	 */

	bp += sizeof(struct rx_header) + 4;

	switch (ubik_op) {
		case 10000:		/* Beacon */
			TRUNC(4);
			temp = ntohl(*((int *) bp));
			bp += sizeof(int32_t);
			printf(" syncsite %s", temp ? "yes" : "no");
			printf(" votestart");
			DATEOUT();
			printf(" dbversion");
			UBIK_VERSIONOUT();
			printf(" tid");
			UBIK_VERSIONOUT();
			break;
		case 10003:		/* Get sync site */
			printf(" site");
			UINTOUT();
			break;
		case 20000:		/* Begin */
		case 20001:		/* Commit */
		case 20007:		/* Abort */
		case 20008:		/* Release locks */
		case 20010:		/* Writev */
			printf(" tid");
			UBIK_VERSIONOUT();
			break;
		case 20002:		/* Lock */
			printf(" tid");
			UBIK_VERSIONOUT();
			printf(" file");
			INTOUT();
			printf(" pos");
			INTOUT();
			printf(" length");
			INTOUT();
			temp = ntohl(*((int *) bp));
			bp += sizeof(int32_t);
			tok2str(ubik_lock_types, "type %d", temp);
			break;
		case 20003:		/* Write */
			printf(" tid");
			UBIK_VERSIONOUT();
			printf(" file");
			INTOUT();
			printf(" pos");
			INTOUT();
			break;
		case 20005:		/* Get file */
			printf(" file");
			INTOUT();
			break;
		case 20006:		/* Send file */
			printf(" file");
			INTOUT();
			printf(" length");
			INTOUT();
			printf(" dbversion");
			UBIK_VERSIONOUT();
			break;
		case 20009:		/* Truncate */
			printf(" tid");
			UBIK_VERSIONOUT();
			printf(" file");
			INTOUT();
			printf(" length");
			INTOUT();
			break;
		case 20012:		/* Set version */
			printf(" tid");
			UBIK_VERSIONOUT();
			printf(" oldversion");
			UBIK_VERSIONOUT();
			printf(" newversion");
			UBIK_VERSIONOUT();
			break;
		default:
			;
	}

	return;

trunc:
	printf(" [|ubik]");
}

/*
 * Handle Ubik replies to any one of the replicated database services
 */

static void
ubik_reply_print(register const u_char *bp, int length, int32_t opcode)
{
	struct rx_header *rxh;

	if (length < sizeof(struct rx_header))
		return;

	rxh = (struct rx_header *) bp;

	/*
	 * Print out the ubik call we're invoking.  This table was gleaned
	 * from ubik/ubik_int.xg
	 */

	printf(" ubik reply %s", tok2str(ubik_req, "op#%d", opcode));

	bp += sizeof(struct rx_header);

	/*
	 * If it was a data packet, print out the arguments to the Ubik calls
	 */
	
	if (rxh->type == RX_PACKET_TYPE_DATA)
		switch (opcode) {
		case 10000:		/* Beacon */
			printf(" vote no");
			break;
		case 20004:		/* Get version */
			printf(" dbversion");
			UBIK_VERSIONOUT();
			break;
		default:
			;
		}
	
	/*
	 * Otherwise, print out "yes" it it was a beacon packet (because
	 * that's how yes votes are returned, go figure), otherwise
	 * just print out the error code.
	 */

	else
		switch (opcode) {
		case 10000:		/* Beacon */
			printf(" vote yes until");
			DATEOUT();
			break;
		default:
			printf(" errcode");
			INTOUT();
		}

	return;

trunc:
	printf(" [|ubik]");
}
