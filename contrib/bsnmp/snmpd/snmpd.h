/*
 * Copyright (c) 2001-2003
 *	Fraunhofer Institute for Open Communication Systems (FhG Fokus).
 *	All rights reserved.
 *
 * Author: Harti Brandt <harti@freebsd.org>
 *
 * Redistribution of this software and documentation and use in source and
 * binary forms, with or without modification, are permitted provided that
 * the following conditions are met:
 *
 * 1. Redistributions of source code or documentation must retain the above
 *    copyright notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE AND DOCUMENTATION IS PROVIDED BY FRAUNHOFER FOKUS
 * AND ITS CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * FRAUNHOFER FOKUS OR ITS CONTRIBUTORS  BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $Begemot: bsnmp/snmpd/snmpd.h,v 1.17 2003/01/28 13:44:35 hbb Exp $
 *
 * Private SNMPd data and functions.
 */
#include <sys/queue.h>
#include <isc/eventlib.h>

#define PATH_SYSCONFIG "/etc:/usr/etc:/usr/local/etc"

/*************************************************************
 *
 * Communities
 */
struct community {
	struct lmodule *owner;	/* who created the community */
	u_int		private;/* private name for the module */
	u_int		value;	/* value of this community */
	u_char *	string;	/* the community string */
	const u_char *	descr;	/* description */
	TAILQ_ENTRY(community) link;

	struct asn_oid	index;
};
/* list of all known communities */
extern TAILQ_HEAD(community_list, community) community_list;

/*************************************************************
 *
 * Request IDs.
 */
struct idrange {
	u_int		type;	/* type id */
	int32_t		base;	/* base of this range */
	int32_t		size;	/* size of this range */
	int32_t		next;	/* generator */
	struct lmodule *owner;	/* owner module */
	TAILQ_ENTRY(idrange) link;
};

/* list of all known ranges */
extern TAILQ_HEAD(idrange_list, idrange) idrange_list;

/* identifier generator */
extern u_int next_idrange;

/* request id generator for traps */
extern u_int trap_reqid;

/*************************************************************
 *
 * Timers
 */
struct timer {
	void	(*func)(void *);/* user function */
	void	*udata;		/* user data */
	evTimerID id;		/* timer id */
	struct lmodule *owner;	/* owner of the timer */
	LIST_ENTRY(timer) link;
};

/* list of all current timers */
extern LIST_HEAD(timer_list, timer) timer_list;


/*************************************************************
 *
 * File descriptors
 */
struct fdesc {
	int	fd;		/* the file descriptor */
	void	(*func)(int, void *);/* user function */
	void	*udata;		/* user data */
	evFileID id;		/* file id */
	struct lmodule *owner;	/* owner module of the file */
	LIST_ENTRY(fdesc) link;
};

/* list of all current selected files */
extern LIST_HEAD(fdesc_list, fdesc) fdesc_list;

/*************************************************************
 *
 * Loadable modules
 */
# define LM_SECTION_MAX	14
struct lmodule {
	char		section[LM_SECTION_MAX + 1]; /* and index */
	char		*path;
	u_int		flags;
	void		*handle;
	const struct snmp_module *config;

	TAILQ_ENTRY(lmodule) link;
	TAILQ_ENTRY(lmodule) start;

	struct asn_oid	index;
};
#define LM_STARTED	0x0001
#define LM_ONSTARTLIST	0x0002

extern TAILQ_HEAD(lmodules, lmodule) lmodules;

struct lmodule *lm_load(const char *, const char *);
void lm_unload(struct lmodule *);
void lm_start(struct lmodule *);

/*************************************************************
 *
 * SNMP ports
 */
struct snmp_port {
	u_int8_t	addr[4];/* host byteorder */
	u_int16_t	port;	/* host byteorder */

	int		sock;	/* the socket */
	void *		id;	/* evSelect handle */

	struct sockaddr_in ret;	/* the return address */
	socklen_t	retlen;	/* length of that address */

	TAILQ_ENTRY(snmp_port) link;

	struct asn_oid	index;
};
TAILQ_HEAD(snmp_port_list, snmp_port);
extern struct snmp_port_list snmp_port_list;

void close_snmp_port(struct snmp_port *);
int open_snmp_port(u_int8_t *, u_int32_t, struct snmp_port **);

struct local_port {
	char		*name;	/* unix path name */
	int		sock;	/* the socket */
	void		*id;	/* evSelect handle */

	struct sockaddr_un ret;	/* the return address */
	socklen_t	retlen;	/* length of that address */

	TAILQ_ENTRY(local_port) link;

	struct asn_oid	index;
};
TAILQ_HEAD(local_port_list, local_port);
extern struct local_port_list local_port_list;

void close_local_port(struct local_port *);
int open_local_port(u_char *, size_t, struct local_port **);

/*************************************************************
 *
 * SNMPd scalar configuration.
 */
struct snmpd {
	/* transmit buffer size */
	u_int32_t	txbuf;

	/* receive buffer size */
	u_int32_t	rxbuf;

	/* disable community table */
	int		comm_dis;

	/* authentication traps */
	int		auth_traps;

	/* source address for V1 traps */
	u_char		trap1addr[4];
};
extern struct snmpd snmpd;

/*
 * The debug group
 */
struct debug {
	u_int		dump_pdus;
	u_int		logpri;
	u_int		evdebug;
};
extern struct debug debug;


/*
 * SNMPd statistics table
 */
struct snmpd_stats {
	u_int32_t	inPkts;		/* total packets received */
	u_int32_t	inBadVersions;	/* unknown version number */
	u_int32_t	inASNParseErrs;	/* fatal parse errors */
	u_int32_t	inBadCommunityNames;
	u_int32_t	inBadCommunityUses;
	u_int32_t	proxyDrops;	/* dropped by proxy function */
	u_int32_t	silentDrops;

	u_int32_t	inBadPduTypes;
	u_int32_t	inTooLong;
	u_int32_t	noTxbuf;
	u_int32_t	noRxbuf;
};
extern struct snmpd_stats snmpd_stats;

/*
 * OR Table
 */
struct objres {
	TAILQ_ENTRY(objres) link;
	u_int		index;
	struct asn_oid	oid;	/* the resource OID */
	char		descr[256];
	u_int32_t	uptime;
	struct lmodule	*module;
};
TAILQ_HEAD(objres_list, objres);
extern struct objres_list objres_list;

/*
 * Trap Sink Table
 */
struct trapsink {
	TAILQ_ENTRY(trapsink) link;
	struct asn_oid	index;
	u_int		status;
	int		socket;
	u_char		comm[SNMP_COMMUNITY_MAXLEN];
	int		version;
};
enum {
	TRAPSINK_ACTIVE		= 1,
	TRAPSINK_NOT_IN_SERVICE	= 2,
	TRAPSINK_NOT_READY	= 3,
	TRAPSINK_DESTROY	= 6,

	TRAPSINK_V1		= 1,
	TRAPSINK_V2		= 2,
};
TAILQ_HEAD(trapsink_list, trapsink);
extern struct trapsink_list trapsink_list;

extern const char *syspath;

/* snmpSerialNo */
extern int32_t snmp_serial_no;

int init_actvals(void);
int read_config(const char *, struct lmodule *);
int define_macro(const char *name, const char *value);
