/* ntpdc_ops.c,v 3.1 1993/07/06 01:12:02 jbj Exp
 * ntpdc_ops.c - subroutines which are called to perform operations by xntpdc
 */
#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/time.h>
#include <netdb.h>

#include "ntpdc.h"
#include "ntp_control.h"
#include "ntp_refclock.h"
#include "ntp_stdlib.h"

/*
 * Declarations for command handlers in here
 */
static	int	checkitems	P((int, FILE *));
static	int	checkitemsize	P((int, int));
static	int	check1item	P((int, FILE *));
static	void	peerlist	P((struct parse *, FILE *));
static	void	peers		P((struct parse *, FILE *));
static	void	dmpeers		P((struct parse *, FILE *));
static	void	dopeers		P((struct parse *, FILE *, int));
static	void	printpeer	P((struct info_peer *, FILE *));
static	void	showpeer	P((struct parse *, FILE *));
static	void	peerstats	P((struct parse *, FILE *));
static	void	loopinfo	P((struct parse *, FILE *));
static	void	sysinfo		P((struct parse *, FILE *));
static	void	sysstats	P((struct parse *, FILE *));
static	void	iostats		P((struct parse *, FILE *));
static	void	memstats	P((struct parse *, FILE *));
static	void	timerstats	P((struct parse *, FILE *));
static	void	addpeer		P((struct parse *, FILE *));
static	void	addserver	P((struct parse *, FILE *));
static	void	broadcast	P((struct parse *, FILE *));
static	void	doconfig	P((struct parse *, FILE *, int));
static	void	unconfig	P((struct parse *, FILE *));
static	void	set		P((struct parse *, FILE *));
static	void	sys_clear	P((struct parse *, FILE *));
static	void	doset		P((struct parse *, FILE *, int));
static	void	reslist		P((struct parse *, FILE *));
static	void	restrict	P((struct parse *, FILE *));
static	void	unrestrict	P((struct parse *, FILE *));
static	void	delrestrict	P((struct parse *, FILE *));
static	void	do_restrict	P((struct parse *, FILE *, int));
static	void	monlist		P((struct parse *, FILE *));
static	void	monitor		P((struct parse *, FILE *));
static	void	reset		P((struct parse *, FILE *));
static	void	preset		P((struct parse *, FILE *));
static	void	readkeys	P((struct parse *, FILE *));
static	void	dodirty		P((struct parse *, FILE *));
static	void	dontdirty	P((struct parse *, FILE *));
static	void	trustkey	P((struct parse *, FILE *));
static	void	untrustkey	P((struct parse *, FILE *));
static	void	do_trustkey	P((struct parse *, FILE *, int));
static	void	authinfo	P((struct parse *, FILE *));
static	void	traps		P((struct parse *, FILE *));
static	void	addtrap		P((struct parse *, FILE *));
static	void	clrtrap		P((struct parse *, FILE *));
static	void	do_addclr_trap	P((struct parse *, FILE *, int));
static	void	requestkey	P((struct parse *, FILE *));
static	void	controlkey	P((struct parse *, FILE *));
static	void	do_changekey	P((struct parse *, FILE *, int));
static	void	ctlstats	P((struct parse *, FILE *));
static	void	leapinfo	P((struct parse *, FILE *));
static	void	clockstat	P((struct parse *, FILE *));
static	void	fudge		P((struct parse *, FILE *));
static	void	maxskew		P((struct parse *, FILE *));
static	void	clkbug		P((struct parse *, FILE *));
static	void	setprecision	P((struct parse *, FILE *));
static	void	setselect	P((struct parse *, FILE *));

/*
 * Commands we understand.  Ntpdc imports this.
 */
struct xcmd opcmds[] = {
	{ "listpeers",	peerlist,	{  NO, NO, NO, NO },
					{ "", "", "", "" },
			"print list of peers the server knows about" },
	{ "peers",	peers,		{ NO, NO, NO, NO },
					{ "", "", "", "" },
			"print peer summary information" },
	{ "dmpeers",	dmpeers,	{ NO, NO, NO, NO },
					{ "", "", "", "" },
			"print peer summary info the way Dave Mills likes it" },
	{ "showpeer",	showpeer,	{ ADD, OPT|ADD, OPT|ADD, OPT|ADD },
		{ "peer_address", "peer2_addr", "peer3_addr", "peer4_addr" },
			"print detailed information for one or more peers" },
	{ "pstats",	peerstats,	{ ADD, OPT|ADD, OPT|ADD, OPT|ADD },
		{ "peer_address", "peer2_addr", "peer3_addr", "peer4_addr" },
			"print statistical information for one or more peers" },
	{ "loopinfo",	loopinfo,	{ OPT|STR, NO, NO, NO },
					{ "oneline|multiline", "", "", "" },
			"print loop filter information" },
	{ "sysinfo",	sysinfo,	{ NO, NO, NO, NO },
					{ "", "", "", "" },
			"print local server information" },
	{ "sysstats",	sysstats,	{ NO, NO, NO, NO },
					{ "", "", "", "" },
			"print local server statistics" },
	{ "memstats",	memstats,	{ NO, NO, NO, NO },
					{ "", "", "", "" },
			"print peer memory usage statistics" },
	{ "iostats",	iostats,	{ NO, NO, NO, NO },
					{ "", "", "", "" },
			"print I/O subsystem statistics" },
	{ "timerstats",	timerstats,	{ NO, NO, NO, NO },
					{ "", "", "", "" },
			"print event timer subsystem statistics" },
	{ "addpeer",	addpeer,	{ ADD, OPT|UINT, OPT|UINT, OPT|STR },
				{ "addr", "keyid", "version", "minpoll|prefer" },
			"configure a new peer association" },
	{ "addserver",	addserver,	{ ADD, OPT|UINT, OPT|UINT, OPT|STR },
				{ "addr", "keyid", "version", "minpoll|prefer" },
			"configure a new server" },
	{ "broadcast",	broadcast,	{ ADD, OPT|UINT, OPT|UINT, OPT|STR },
				{ "addr", "keyid", "version", "minpoll" },
			"configure broadcasting time service" },
	{ "unconfig",	unconfig,	{ ADD, OPT|ADD, OPT|ADD, OPT|ADD },
		{ "peer_address", "peer2_addr", "peer3_addr", "peer4_addr" },
			"unconfigure existing peer assocations" },
	{ "set",	set,		{ STR, OPT|STR, OPT|STR, OPT|STR },
					{ "bclient|auth", "...", "...", "..." },
			"set a system flag (bclient, authenticate)" },
        { "clear",      sys_clear,      { STR, OPT|STR, OPT|STR, OPT|STR },
					{ "bclient|auth", "...", "...", "..." },
			"clear a system flag (bclient, authenticate)" },
	{ "reslist",	reslist,	{ NO, NO, NO, NO },
					{ "", "", "", "" },
			"print the server's restrict list" },
	{ "restrict",	restrict,	{ ADD, ADD, STR, OPT|STR },
		{ "address", "mask",
		"ntpport|ignore|noserve|notrust|noquery|nomodify|nopeer",
		"..." },
			"create restrict entry/add flags to entry" },
	{ "unrestrict", unrestrict,	{ ADD, ADD, STR, OPT|STR },
		{ "address", "mask",
		"ntpport|ignore|noserve|notrust|noquery|nomodify|nopeer",
		"..." },
			"remove flags from a restrict entry" },
	{ "delrestrict", delrestrict,	{ ADD, ADD, OPT|STR, NO },
					{ "address", "mask", "ntpport", "" },
			"delete a restrict entry" },
	{ "monlist",	monlist,	{ NO, NO, NO, NO },
					{ "", "", "", "" },
		"print data the server's monitor routines have collected" },
	{ "monitor",	monitor,	{ STR, NO, NO, NO },
					{ "on|off", "", "", "" },
		"turn the server's monitoring facility on or off" },
	{ "reset",	reset,		{ STR, OPT|STR, OPT|STR, OPT|STR },
		{ "io|sys|mem|timer|auth|allpeers", "...", "...", "..." },
			"reset various subsystem statistics counters" },
	{ "preset",	preset,		{ ADD, OPT|ADD, OPT|ADD, OPT|ADD },
		{ "peer_address", "peer2_addr", "peer3_addr", "peer4_addr" },
		"reset stat counters associated with particular peer(s)" },
	{ "readkeys",	readkeys,	{ NO, NO, NO, NO },
					{ "", "", "", "" },
	"request a reread of the `keys' file and re-init of system keys" },
	{ "dodirty",	dodirty,	{ NO, NO, NO, NO },
					{ "", "", "", "" },
			"placeholder, historical interest only" },
	{ "dontdirty",	dontdirty,	{ NO, NO, NO, NO },
					{ "", "", "", "" },
			"placeholder, historical interest only" },
	{ "trustkey",	trustkey,	{ UINT, OPT|UINT, OPT|UINT, OPT|UINT },
					{ "keyid", "keyid", "keyid", "keyid" },
			"add one or more key ID's to the trusted list" },
	{ "untrustkey",	untrustkey,	{ UINT, OPT|UINT, OPT|UINT, OPT|UINT },
					{ "keyid", "keyid", "keyid", "keyid" },
			"remove one or more key ID's from the trusted list" },
	{ "authinfo",	authinfo,	{ NO, NO, NO, NO },
					{ "", "", "", "" },
	"obtain information concerning the state of the authentication code" },
	{ "traps",	traps,		{ NO, NO, NO, NO },
					{ "", "", "", "" },
			"obtain information about traps set in server" },
	{ "addtrap",	addtrap,	{ ADD, OPT|UINT, OPT|ADD, NO },
					{ "address", "port", "interface", "" },
			"configure a trap in the server" },
	{ "clrtrap",	clrtrap,	{ ADD, OPT|UINT, OPT|ADD, NO },
					{ "address", "port", "interface", "" },
		"remove a trap (configured or otherwise) from the server" },
	{ "requestkey",	requestkey,	{ UINT, NO, NO, NO },
					{ "keyid", "", "", "" },
		"change the keyid the server uses to authenticate requests" },
	{ "controlkey",	controlkey,	{ UINT, NO, NO, NO },
					{ "keyid", "", "", "" },
	"change the keyid the server uses to authenticate control messages" },
	{ "ctlstats",	ctlstats,	{ NO, NO, NO, NO },
					{ "", "", "", "" },
		"obtain packet count statistics from the control module" },
	{ "leapinfo",	leapinfo,	{ NO, NO, NO, NO },
					{ "", "", "", "" },
		"obtain information about the current leap second state" },
	{ "clockstat",	clockstat,	{ ADD, OPT|ADD, OPT|ADD, OPT|ADD },
				{ "address", "address", "address", "address" },
			"obtain status information about the specified clock" },
	{ "fudge",	fudge,		{ ADD, STR, STR, NO },
		{ "address", "time1|time2|val1|val2|flags", "value", "" },
			"set/change one of a clock's fudge factors" },
	{ "maxskew",	maxskew,	{ STR, NO, NO, NO },
					{ "maximum_skew", "", "", "" },
			"set the server's maximum skew parameter" },
	{ "clkbug",	clkbug,		{ ADD, OPT|ADD, OPT|ADD, OPT|ADD },
				{ "address", "address", "address", "address" },
		"obtain debugging information from the specified clock" },
	{ "setprecision", setprecision,	{ INT, NO, NO, NO },
					{ "sys_precision", "", "", "" },
				"set the server's advertised precision" },
	{ "setselect",	setselect,	{ UINT, NO, NO, NO },
				{ "select_algorithm_number", "", "", "" },
		"change the selection weighting algorithm used by the server" },
	{ 0,		0,		{ NO, NO, NO, NO },
					{ "", "", "", "" }, "" }
};


/*
 * Imported from ntpdc.c
 */
extern int showhostnames;
extern int debug;
extern struct servent *server_entry;

/*
 * For quick string comparisons
 */
#define	STREQ(a, b)	(*(a) == *(b) && strcmp((a), (b)) == 0)


/*
 * checkitems - utility to print a message if no items were returned
 */
static int
checkitems(items, fp)
	int items;
	FILE *fp;
{
	if (items == 0) {
		(void) fprintf(fp, "No data returned in response to query\n");
		return 0;
	}
	return 1;
}


/*
 * checkitemsize - utility to print a message if the item size is wrong
 */
static int
checkitemsize(itemsize, expected)
	int itemsize;
	int expected;
{
	if (itemsize != expected) {
		(void) fprintf(stderr,
    "***Incorrect item size returned by remote host (%d should be %d)\n",
		    itemsize, expected);
		return 0;
	}
	return 1;
}


/*
 * check1item - check to make sure we have exactly one item
 */
static int
check1item(items, fp)
	int items;
	FILE *fp;
{
	if (items == 0) {
		(void) fprintf(fp, "No data returned in response to query\n");
		return 0;
	}
	if (items > 1) {
		(void) fprintf(fp, "Expected one item in response, got %d\n",
		    items);
		return 0;
	}
	return 1;
}



/*
 * peerlist - get a short list of peers
 */
/*ARGSUSED*/
static void
peerlist(pcmd, fp)
	struct parse *pcmd;
	FILE *fp;
{
	struct info_peer_list *plist;
	int items;
	int itemsize;
	int res;

	res = doquery(IMPL_XNTPD, REQ_PEER_LIST, 0, 0, 0, (char *)NULL, &items,
	    &itemsize, (char **)&plist);
	
	if (res != 0 && items == 0)
		return;

	if (!checkitems(items, fp))
		return;

	if (!checkitemsize(itemsize, sizeof(struct info_peer_list)))
		return;

	while (items > 0) {
		(void) fprintf(fp, "%-9s %s\n", modetoa(plist->hmode),
		    nntohost(plist->address));
		plist++;
		items--;
	}
}


/*
 * peers - show peer summary
 */
static void
peers(pcmd, fp)
	struct parse *pcmd;
	FILE *fp;
{
	dopeers(pcmd, fp, 0);
}

/*
 * dmpeers - show peer summary, Dave Mills style
 */
static void
dmpeers(pcmd, fp)
	struct parse *pcmd;
	FILE *fp;
{
	dopeers(pcmd, fp, 1);
}


/*
 * peers - show peer summary
 */
/*ARGSUSED*/
static void
dopeers(pcmd, fp, dmstyle)
	struct parse *pcmd;
	FILE *fp;
	int dmstyle;
{
	struct info_peer_summary *plist;
	int items;
	int itemsize;
	int ntp_poll;
	int res;
	int c;
	l_fp tempts;

	res = doquery(IMPL_XNTPD, REQ_PEER_LIST_SUM, 0, 0, 0, (char *)NULL,
	    &items, &itemsize, (char **)&plist);
	
	if (res != 0 && items == 0)
		return;

	if (!checkitems(items, fp))
		return;

	if (!checkitemsize(itemsize, sizeof(struct info_peer_summary)))
		return;

	(void) fprintf(fp,
    "     remote           local      st poll reach  delay   offset   disp\n");
	(void) fprintf(fp,
    "======================================================================\n");
	while (items > 0) {
		if (!dmstyle) {
			if (plist->flags & INFO_FLAG_SYSPEER)
				c = '*';
			else if (plist->hmode == MODE_ACTIVE)
				c = '+';
			else if (plist->hmode == MODE_PASSIVE)
				c = '-';
			else if (plist->hmode == MODE_CLIENT)
				c = '=';
			else if (plist->hmode == MODE_BROADCAST)
				c = '^';
			else if (plist->hmode == MODE_BCLIENT)
				c = '~';
			else
				c = ' ';
		} else {
			if (plist->flags & INFO_FLAG_SYSPEER)
				c = '*';
			else if (plist->flags & INFO_FLAG_SHORTLIST)
				c = '+';
			else if (plist->flags & INFO_FLAG_SEL_CANDIDATE)
				c = '.';
			else
				c = ' ';
		}
		NTOHL_FP(&(plist->offset), &tempts);
		ntp_poll = 1<<max(min3(plist->ppoll, plist->hpoll, NTP_MAXPOLL),
		    NTP_MINPOLL);
		(void) fprintf(fp,
		    "%c%-15.15s %-15.15s %2d %4d  %3o %7.7s %9.9s %6.6s\n",
		    c, nntohost(plist->srcadr),
		    numtoa(plist->dstadr),
		    plist->stratum, ntp_poll, plist->reach,
		    fptoa(NTOHS_FP(plist->delay), 4),
		    lfptoa(&tempts, 6),
		    ufptoa(NTOHS_FP(plist->dispersion), 4));
		plist++;
		items--;
	}
}


/*
 * printpeer - print detail information for a peer
 */
static void
printpeer(pp, fp)
	register struct info_peer *pp;
	FILE *fp;
{
	register int i;
	char junk[5];
	char *str;
	l_fp tempts;

	(void) fprintf(fp, "remote %s, local %s\n",
	    numtoa(pp->srcadr), numtoa(pp->dstadr));

	(void) fprintf(fp, "hmode %s, pmode %s, stratum %d, precision %d\n",
	    modetoa(pp->hmode), modetoa(pp->pmode),
	    pp->stratum, pp->precision);
	
	if (pp->stratum <= 1) {
		junk[4] = 0;
		memmove(junk, (char *)&pp->refid, 4);
		str = junk;
	} else {
		str = numtoa(pp->refid);
	}
	(void) fprintf(fp,
	    "leap %c%c, refid [%s], rootdistance %s, rootdispersion %s\n",
	    pp->leap & 0x2 ? '1' : '0',
	    pp->leap & 0x1 ? '1' : '0',
	    str, ufptoa(HTONS_FP(pp->rootdelay), 4),
	    ufptoa(HTONS_FP(pp->rootdispersion), 4));
	
	(void) fprintf(fp,
	    "ppoll %d, hpoll %d, keyid %u, version %d, association %u\n",
	    pp->ppoll, pp->hpoll, pp->keyid, pp->version, ntohs(pp->associd));

	(void) fprintf(fp,
	    "valid %d, reach %03o, unreach %d, trust %03o\n",
	    pp->valid, pp->reach, pp->unreach, pp->trust);
	
	(void) fprintf(fp, "timer %ds, flags", ntohl(pp->timer));
	if (pp->flags == 0) {
		(void) fprintf(fp, " none\n");
	} else {
		str = "";
		if (pp->flags & INFO_FLAG_SYSPEER) {
			(void) fprintf(fp, " system_peer");
			str = ",";
		}
		if (pp->flags & INFO_FLAG_CONFIG) {
			(void) fprintf(fp, "%s configured", str);
			str = ",";
		}
		if (pp->flags & INFO_FLAG_MINPOLL) {
			(void) fprintf(fp, "%s minpoll", str);
			str = ",";
		}
		if (pp->flags & INFO_FLAG_AUTHENABLE) {
			(void) fprintf(fp, "%s authenable", str);
			str = ",";
		}
		if (pp->flags & INFO_FLAG_REFCLOCK) {
			(void) fprintf(fp, "%s reference_clock", str);
			str = ",";
		}
		if (pp->flags & INFO_FLAG_PREFER) {
			(void) fprintf(fp, "%s preferred_peer", str);
		}
		(void) fprintf(fp, "\n");
	}

	HTONL_FP(&pp->reftime, &tempts);
	(void) fprintf(fp, "reference time:      %s\n",
	    prettydate(&tempts));
	HTONL_FP(&pp->org, &tempts);
	(void) fprintf(fp, "originate timestamp: %s\n",
	    prettydate(&tempts));
	HTONL_FP(&pp->rec, &tempts);
	(void) fprintf(fp, "receive timestamp:   %s\n",
	    prettydate(&tempts));
	HTONL_FP(&pp->xmt, &tempts);
	(void) fprintf(fp, "transmit timestamp:  %s\n",
	    prettydate(&tempts));
	
	(void) fprintf(fp, "filter delay: ");
	for (i = 0; i < NTP_SHIFT; i++) {
		(void) fprintf(fp, " %-8.8s", fptoa(HTONS_FP(pp->filtdelay[i]),4));
		if (i == (NTP_SHIFT>>1)-1)
			(void) fprintf(fp, "\n              ");
	}
	(void) fprintf(fp, "\n");

	(void) fprintf(fp, "filter offset:");
	for (i = 0; i < NTP_SHIFT; i++) {
		HTONL_FP(&pp->filtoffset[i], &tempts);
		(void) fprintf(fp, " %-8.8s", lfptoa(&tempts, 5));
		if (i == (NTP_SHIFT>>1)-1)
			(void) fprintf(fp, "\n              ");
	}
	(void) fprintf(fp, "\n");

	(void) fprintf(fp, "filter order: ");
	for (i = 0; i < NTP_SHIFT; i++) {
		(void) fprintf(fp, " %-8d", pp->order[i]);
		if (i == (NTP_SHIFT>>1)-1)
			(void) fprintf(fp, "\n              ");
	}
	(void) fprintf(fp, "\n");
	
	(void) fprintf(fp, "bdelay filter:");
	for (i = 0; i < NTP_SHIFT; i++) {
		(void) fprintf(fp, " %-8.8s",
		    mfptoa(0, ntohl(pp->bdelay[i]), 5));
		if (i == (NTP_SHIFT>>1)-1)
			(void) fprintf(fp, "\n              ");
	}
	(void) fprintf(fp, "\n");
	
	(void) fprintf(fp, "delay %s, estbdelay %s\n",
	    fptoa(HTONS_FP(pp->delay), 4),
	    mfptoa(0, ntohl(pp->estbdelay), 4));

	HTONL_FP(&pp->offset, &tempts);
	(void) fprintf(fp, "offset %s, dispersion %s\n",
	    lfptoa(&tempts, 6),
	    ufptoa(HTONS_FP(pp->dispersion), 4));
}


/*
 * showpeer - show detailed information for a peer
 */
static void
showpeer(pcmd, fp)
	struct parse *pcmd;
	FILE *fp;
{
	struct info_peer *pp;
	/* 4 is the maximum number of peers which will fit in a packet */
	struct info_peer_list plist[min(MAXARGS, 4)];
	int qitems;
	int items;
	int itemsize;
	int res;

	for (qitems = 0; qitems < min(pcmd->nargs, 4); qitems++) {
		plist[qitems].address = pcmd->argval[qitems].netnum;
		plist[qitems].port = server_entry->s_port;
		plist[qitems].hmode = plist[qitems].flags = 0;
	}

	res = doquery(IMPL_XNTPD, REQ_PEER_INFO, 0, qitems,
	    sizeof(struct info_peer_list), (char *)plist, &items,
	    &itemsize, (char **)&pp);
	
	if (res != 0 && items == 0)
		return;

	if (!checkitems(items, fp))
		return;

	if (!checkitemsize(itemsize, sizeof(struct info_peer)))
		return;

	while (items-- > 0) {
		printpeer(pp, fp);
		if (items > 0)
			(void) fprintf(fp, "\n");
		pp++;
	}
}


/*
 * peerstats - return statistics for a peer
 */
static void
peerstats(pcmd, fp)
	struct parse *pcmd;
	FILE *fp;
{
	struct info_peer_stats *pp;
	/* 4 is the maximum number of peers which will fit in a packet */
	struct info_peer_list plist[min(MAXARGS, 4)];
	int qitems;
	int items;
	int itemsize;
	int res;

	for (qitems = 0; qitems < min(pcmd->nargs, 4); qitems++) {
		plist[qitems].address = pcmd->argval[qitems].netnum;
		plist[qitems].port = server_entry->s_port;
		plist[qitems].hmode = plist[qitems].flags = 0;
	}

	res = doquery(IMPL_XNTPD, REQ_PEER_STATS, 0, qitems,
	    sizeof(struct info_peer_list), (char *)plist, &items,
	    &itemsize, (char **)&pp);
	
	if (res != 0 && items == 0)
		return;

	if (!checkitems(items, fp))
		return;

	if (!checkitemsize(itemsize, sizeof(struct info_peer_stats)))
		return;

	while (items-- > 0) {
		(void) fprintf(fp, "remote host:          %s\n",
		    nntohost(pp->srcadr));
		(void) fprintf(fp, "local interface:      %s\n",
		    numtoa(pp->dstadr));
		(void) fprintf(fp, "time last received:   %ds\n",
		    ntohl(pp->timereceived));
		(void) fprintf(fp, "time until next send: %ds\n",
		    ntohl(pp->timetosend));
		(void) fprintf(fp, "reachability change:  %ds\n",
		    ntohl(pp->timereachable));
		(void) fprintf(fp, "packets sent:         %d\n",
		    ntohl(pp->sent));
		(void) fprintf(fp, "packets received:     %d\n",
		    ntohl(pp->received));
		(void) fprintf(fp, "packets processed:    %d\n",
		    ntohl(pp->processed));
		(void) fprintf(fp, "bad length packets:   %d\n",
		    ntohl(pp->badlength));
		(void) fprintf(fp, "bad auth packets:     %d\n",
		    ntohl(pp->badauth));
		(void) fprintf(fp, "bogus origin packets: %d\n",
		    ntohl(pp->bogusorg));
		(void) fprintf(fp, "duplicate packets:    %d\n",
		    ntohl(pp->oldpkt));
		(void) fprintf(fp, "bad delay rejections: %d\n",
		    ntohl(pp->baddelay));
		(void) fprintf(fp, "select delay rejects: %d\n",
		    ntohl(pp->seldelay));
		(void) fprintf(fp, "select disp rejects:  %d\n",
		    ntohl(pp->seldisp));
		(void) fprintf(fp, "select finds broken:  %d\n",
		    ntohl(pp->selbroken));
		(void) fprintf(fp, "too old for select:   %d\n",
		    ntohl(pp->selold));
		(void) fprintf(fp, "sel candidate order:  %d\n",
		    (int)pp->candidate);
		(void) fprintf(fp, "falseticker order:    %d\n",
		    (int)pp->falseticker);
		(void) fprintf(fp, "select order:         %d\n",
		    (int)pp->select);
		(void) fprintf(fp, "select total:         %d\n",
		    (int)pp->select_total);
		if (items > 0)
			(void) fprintf(fp, "\n");
		pp++;
	}
}


/*
 * loopinfo - show loop filter information
 */
static void
loopinfo(pcmd, fp)
	struct parse *pcmd;
	FILE *fp;
{
	struct info_loop *il;
	int items;
	int itemsize;
	int oneline = 0;
	int res;
	l_fp tempts;

	if (pcmd->nargs > 0) {
		if (STREQ(pcmd->argval[0].string, "oneline"))
			oneline = 1;
		else if (STREQ(pcmd->argval[0].string, "multiline"))
			oneline = 0;
		else {
			(void) fprintf(stderr, "How many lines?\n");
			return;
		}
	}

	res = doquery(IMPL_XNTPD, REQ_LOOP_INFO, 0, 0, 0, (char *)NULL,
	    &items, &itemsize, (char **)&il);
	
	if (res != 0 && items == 0)
		return;

	if (!check1item(items, fp))
		return;

	if (!checkitemsize(itemsize, sizeof(struct info_loop)))
		return;

	if (oneline) {
		l_fp temp2ts;

		HTONL_FP(&il->last_offset, &tempts);
		HTONL_FP(&il->drift_comp, &temp2ts);

		(void) fprintf(fp,
		    "offset %s, drift %s, compliance %d, timer %d seconds\n",
		    lfptoa(&tempts, 7),
		    lfptoa(&temp2ts, 7),
		    ntohl(il->compliance),
		    ntohl(il->watchdog_timer));
	} else {
		HTONL_FP(&il->last_offset, &tempts);
		(void) fprintf(fp, "offset:     %s seconds\n",
		    lfptoa(&tempts, 7));
		HTONL_FP(&il->drift_comp, &tempts);
		(void) fprintf(fp, "frequency:  %s seconds\n",
		    lfptoa(&tempts, 7));
		(void) fprintf(fp, "compliance: %d seconds\n",
		    ntohl(il->compliance));
		(void) fprintf(fp, "timer:      %d seconds\n",
		    ntohl(il->watchdog_timer));
	}
}


/*
 * sysinfo - show current system state
 */
/*ARGSUSED*/
static void
sysinfo(pcmd, fp)
	struct parse *pcmd;
	FILE *fp;
{
	struct info_sys *is;
	int items;
	int itemsize;
	int res;
	char junk[5];
	char *str;
	l_fp tempts;

	res = doquery(IMPL_XNTPD, REQ_SYS_INFO, 0, 0, 0, (char *)NULL,
	    &items, &itemsize, (char **)&is);
	
	if (res != 0 && items == 0)
		return;

	if (!check1item(items, fp))
		return;

	if (!checkitemsize(itemsize, sizeof(struct info_sys)))
		return;

	(void) fprintf(fp, "system peer:      %s\n", nntohost(is->peer));
	(void) fprintf(fp, "system peer mode: %s\n", modetoa(is->peer_mode));
	(void) fprintf(fp, "leap indicator:   %c%c\n",
	    is->leap & 0x2 ? '1' : '0',
	    is->leap & 0x1 ? '1' : '0');
	(void) fprintf(fp, "stratum:          %d\n", (int)is->stratum);
	(void) fprintf(fp, "precision:        %d\n", (int)is->precision);
	(void) fprintf(fp, "select algorithm: %d\n", (int)is->selection);
	(void) fprintf(fp, "sync distance:    %s\n",
	    fptoa(NTOHS_FP(is->rootdelay), 4));
	(void) fprintf(fp, "sync dispersion:  %s\n",
	    ufptoa(NTOHS_FP(is->rootdispersion), 4));
	if (is->stratum <= 1) {
		junk[4] = 0;
		memmove(junk, (char *)&is->refid, 4);
		str = junk;
	} else {
		str = numtoa(is->refid);
	}
	(void) fprintf(fp, "reference ID:     [%s]\n", str);

	HTONL_FP(&is->reftime, &tempts);
	(void) fprintf(fp, "reference time:   %s\n", prettydate(&tempts));

	(void) fprintf(fp, "system flags:     ");
	if ((is->flags & (INFO_FLAG_BCLIENT|INFO_FLAG_AUTHENABLE)) == 0) {
		(void) fprintf(fp, "none\n");
	} else {
		res = 0;
		if (is->flags & INFO_FLAG_BCLIENT) {
			(void) fprintf(fp, "bclient");
			res = 1;
		}
		if (is->flags & INFO_FLAG_AUTHENABLE)
			(void) fprintf(fp, "%sauthenticate",
			    res ? ", " : "");
		(void) fprintf(fp, "\n");
	}

	HTONL_FP(&is->bdelay, &tempts);
	(void) fprintf(fp, "broadcast delay:  %s\n", lfptoa(&tempts, 7));

	HTONL_FP(&is->authdelay, &tempts);
	(void) fprintf(fp, "encryption delay: %s\n", lfptoa(&tempts, 7));
	(void) fprintf(fp, "maximum skew:     %s\n",
	    ufptoa(NTOHS_FP(is->maxskew), 4));
}


/*
 * sysstats - print system statistics
 */
/*ARGSUSED*/
static void
sysstats(pcmd, fp)
	struct parse *pcmd;
	FILE *fp;
{
	struct info_sys_stats *ss;
	int items;
	int itemsize;
	int res;

	res = doquery(IMPL_XNTPD, REQ_SYS_STATS, 0, 0, 0, (char *)NULL,
	    &items, &itemsize, (char **)&ss);
	
	if (res != 0 && items == 0)
		return;

	if (!check1item(items, fp))
		return;

	if (!checkitemsize(itemsize, sizeof(struct info_sys_stats)))
		return;

	(void) fprintf(fp, "system uptime:          %d\n",
	    ntohl(ss->timeup));
	(void) fprintf(fp, "time since reset:       %d\n",
	    ntohl(ss->timereset));
	(void) fprintf(fp, "bad stratum in packet:  %d\n",
	    ntohl(ss->badstratum));
	(void) fprintf(fp, "old version packets:    %d\n",
	    ntohl(ss->oldversionpkt));
	(void) fprintf(fp, "new version packets:    %d\n",
	    ntohl(ss->newversionpkt));
	(void) fprintf(fp, "unknown version number: %d\n",
	    ntohl(ss->unknownversion));
	(void) fprintf(fp, "bad packet length:      %d\n",
	    ntohl(ss->badlength));
	(void) fprintf(fp, "packets processed:      %d\n",
	    ntohl(ss->processed));
	(void) fprintf(fp, "bad authentication:     %d\n",
	    ntohl(ss->badauth));
	(void) fprintf(fp, "wander hold downs:      %d\n",
	    ntohl(ss->wanderhold));
}



/*
 * iostats - print I/O statistics
 */
/*ARGSUSED*/
static void
iostats(pcmd, fp)
	struct parse *pcmd;
	FILE *fp;
{
	struct info_io_stats *io;
	int items;
	int itemsize;
	int res;

	res = doquery(IMPL_XNTPD, REQ_IO_STATS, 0, 0, 0, (char *)NULL,
	    &items, &itemsize, (char **)&io);
	
	if (res != 0 && items == 0)
		return;

	if (!check1item(items, fp))
		return;

	if (!checkitemsize(itemsize, sizeof(struct info_io_stats)))
		return;

	(void) fprintf(fp, "time since reset:      %d\n",
	    ntohl(io->timereset));
	(void) fprintf(fp, "total receive buffers: %d\n",
	    (int)ntohs(io->totalrecvbufs));
	(void) fprintf(fp, "free receive buffers:  %d\n",
	    (int)ntohs(io->freerecvbufs));
	(void) fprintf(fp, "used receive buffers:  %d\n",
	    (int)ntohs(io->fullrecvbufs));
	(void) fprintf(fp, "low water refills:     %d\n",
	    (int)ntohs(io->lowwater));
	(void) fprintf(fp, "dropped packets:       %d\n",
	    ntohl(io->dropped));
	(void) fprintf(fp, "ignored packets:       %d\n",
	    ntohl(io->ignored));
	(void) fprintf(fp, "received packets:      %d\n",
	    ntohl(io->received));
	(void) fprintf(fp, "packets sent:          %d\n",
	    ntohl(io->sent));
	(void) fprintf(fp, "packets not sent:      %d\n",
	    ntohl(io->notsent));
	(void) fprintf(fp, "interrupts handled:    %d\n",
	    ntohl(io->interrupts));
	(void) fprintf(fp, "received by interrupt: %d\n",
	    ntohl(io->int_received));
}



/*
 * memstats - print peer memory statistics
 */
/*ARGSUSED*/
static void
memstats(pcmd, fp)
	struct parse *pcmd;
	FILE *fp;
{
	struct info_mem_stats *mem;
	int i;
	int items;
	int itemsize;
	int res;

	res = doquery(IMPL_XNTPD, REQ_MEM_STATS, 0, 0, 0, (char *)NULL,
	    &items, &itemsize, (char **)&mem);
	
	if (res != 0 && items == 0)
		return;

	if (!check1item(items, fp))
		return;

	if (!checkitemsize(itemsize, sizeof(struct info_mem_stats)))
		return;

	(void) fprintf(fp, "time since reset:     %d\n",
	    ntohl(mem->timereset));
	(void) fprintf(fp, "total peer memory:    %d\n",
	    (int)ntohs(mem->totalpeermem));
	(void) fprintf(fp, "free peer memory:     %d\n",
	    (int)ntohs(mem->freepeermem));
	(void) fprintf(fp, "calls to findpeer:    %d\n",
	    ntohl(mem->findpeer_calls));
	(void) fprintf(fp, "new peer allocations: %d\n",
	    ntohl(mem->allocations));
	(void) fprintf(fp, "peer demobilizations: %d\n",
	    ntohl(mem->demobilizations));

	(void) fprintf(fp, "hash table counts:   ");
	for (i = 0; i < HASH_SIZE; i++) {
		(void) fprintf(fp, "%4d", (int)mem->hashcount[i]);
		if ((i % 8) == 7 && i != (HASH_SIZE-1)) {
			(void) fprintf(fp, "\n                     ");
		}
	}
	(void) fprintf(fp, "\n");
}



/*
 * timerstats - print timer statistics
 */
/*ARGSUSED*/
static void
timerstats(pcmd, fp)
	struct parse *pcmd;
	FILE *fp;
{
	struct info_timer_stats *tim;
	int items;
	int itemsize;
	int res;

	res = doquery(IMPL_XNTPD, REQ_TIMER_STATS, 0, 0, 0, (char *)NULL,
	    &items, &itemsize, (char **)&tim);
	
	if (res != 0 && items == 0)
		return;

	if (!check1item(items, fp))
		return;

	if (!checkitemsize(itemsize, sizeof(struct info_timer_stats)))
		return;

	(void) fprintf(fp, "time since reset:  %d\n",
	    ntohl(tim->timereset));
	(void) fprintf(fp, "alarms handled:    %d\n",
	    ntohl(tim->alarms));
	(void) fprintf(fp, "alarm overruns:    %d\n",
	    ntohl(tim->overflows));
	(void) fprintf(fp, "calls to transmit: %d\n",
	    ntohl(tim->xmtcalls));
}


/*
 * addpeer - configure an active mode association
 */
static void
addpeer(pcmd, fp)
	struct parse *pcmd;
	FILE *fp;
{
	doconfig(pcmd, fp, MODE_ACTIVE);
}


/*
 * addserver - configure a client mode association
 */
static void
addserver(pcmd, fp)
	struct parse *pcmd;
	FILE *fp;
{
	doconfig(pcmd, fp, MODE_CLIENT);
}

/*
 * broadcast - configure a broadcast mode association
 */
static void
broadcast(pcmd, fp)
	struct parse *pcmd;
	FILE *fp;
{
	doconfig(pcmd, fp, MODE_BROADCAST);
}


/*
 * config - configure a new peer association
 */
static void
doconfig(pcmd, fp, mode)
	struct parse *pcmd;
	FILE *fp;
	int mode;
{
	struct conf_peer cpeer;
	int items;
	int itemsize;
	char *dummy;
	U_LONG keyid;
	u_int version;
	u_int flags;
	int res;

	keyid = 0;
	version = NTP_VERSION;
	flags = 0;
	res = 0;
	if (pcmd->nargs > 1) {
		keyid = pcmd->argval[1].uval;
		if (keyid > 0) {
			flags |= CONF_FLAG_AUTHENABLE;
		}
		if (pcmd->nargs > 2) {
			version = (u_int)pcmd->argval[2].uval;
			if (version > NTP_VERSION
			    || version < NTP_OLDVERSION) {
				(void) fprintf(fp,
				    "funny version number %u specified\n",
				    version);
				res++;
			}

			items = 3;
			while (pcmd->nargs > items) {
				if (STREQ(pcmd->argval[items].string,
				    "minpoll")) {
					flags |= CONF_FLAG_MINPOLL;
				} else {
				        if (STREQ(pcmd->argval[items].string,
				            "prefer")) {
				    	        flags |= CONF_FLAG_PREFER;
				        } else {
					        (void) fprintf(fp,
					            "`%s' not understood\n",
					            pcmd->argval[3].string);
					        res++;
						break;
				        }
				}
			        items++;
			}
		}
	}

	if (res)
		return;

	cpeer.peeraddr = pcmd->argval[0].netnum;
	cpeer.hmode = (u_char) mode;
	cpeer.keyid = keyid;
	cpeer.version = (u_char) version;
	cpeer.minpoll = NTP_MINDPOLL;
	cpeer.maxpoll = NTP_MAXPOLL;
	cpeer.flags = (u_char)flags;

	res = doquery(IMPL_XNTPD, REQ_CONFIG, 1, 1,
	    sizeof(struct conf_peer), (char *)&cpeer, &items,
	    &itemsize, &dummy);
	
	if (res == 0)
		(void) fprintf(fp, "done!\n");
	return;
}


/*
 * unconfig - unconfigure some associations
 */
static void
unconfig(pcmd, fp)
	struct parse *pcmd;
	FILE *fp;
{
	/* 8 is the maximum number of peers which will fit in a packet */
	struct conf_unpeer plist[min(MAXARGS, 8)];
	int qitems;
	int items;
	int itemsize;
	char *dummy;
	int res;

	for (qitems = 0; qitems < min(pcmd->nargs, 8); qitems++) {
		plist[qitems].peeraddr = pcmd->argval[qitems].netnum;
	}

	res = doquery(IMPL_XNTPD, REQ_UNCONFIG, 1, qitems,
	    sizeof(struct conf_unpeer), (char *)plist, &items,
	    &itemsize, &dummy);
	
	if (res == 0)
		(void) fprintf(fp, "done!\n");
}


/*
 * set - set some system flags
 */
static void
set(pcmd, fp)
	struct parse *pcmd;
	FILE *fp;
{
	doset(pcmd, fp, REQ_SET_SYS_FLAG);
}


/*
 * clear - clear some system flags
 */
static void
sys_clear(pcmd, fp)
	struct parse *pcmd;
	FILE *fp;
{
	doset(pcmd, fp, REQ_CLR_SYS_FLAG);
}


/*
 * doset - set/clear system flags
 */
static void
doset(pcmd, fp, req)
	struct parse *pcmd;
	FILE *fp;
	int req;
{
	/* 8 is the maximum number of peers which will fit in a packet */
	struct conf_sys_flags sys;
	int items;
	int itemsize;
	char *dummy;
	int res;

	sys.flags = 0;
	res = 0;
	for (items = 0; items < pcmd->nargs; items++) {
		if (STREQ(pcmd->argval[items].string, "bclient"))
			sys.flags |= SYS_FLAG_BCLIENT;
		else if (STREQ(pcmd->argval[items].string, "auth"))
			sys.flags |= SYS_FLAG_AUTHENTICATE;
		else {
			(void) fprintf(fp, "unknown flag %s\n",
			    pcmd->argval[items].string);
			res = 1;
		}
	}

	if (res || sys.flags == 0)
		return;

	res = doquery(IMPL_XNTPD, req, 1, 1,
	    sizeof(struct conf_sys_flags), (char *)&sys, &items,
	    &itemsize, &dummy);
	
	if (res == 0)
		(void) fprintf(fp, "done!\n");
}


/*
 * data for printing/interrpreting the restrict flags
 */
struct resflags {
	char *str;
	int bit;
};

static struct resflags resflags[] = {
	{ "ignore",	RES_IGNORE },
	{ "noserve",	RES_DONTSERVE },
	{ "notrust",	RES_DONTTRUST },
	{ "noquery",	RES_NOQUERY },
	{ "nomodify",	RES_NOMODIFY },
	{ "nopeer",	RES_NOPEER },
	{ "notrap",	RES_NOTRAP },
	{ "lptrap",	RES_LPTRAP },
	{ "",		0 }
};

static struct resflags resmflags[] = {
	{ "ntpport",	RESM_NTPONLY },
	{ "interface",	RESM_INTERFACE },
	{ "",		0 }
};


/*
 * reslist - obtain and print the server's restrict list
 */
/*ARGSUSED*/
static void
reslist(pcmd, fp)
	struct parse *pcmd;
	FILE *fp;
{
	struct info_restrict *rl;
	int items;
	int itemsize;
	int res;
	char *addr;
	char *mask;
	struct resflags *rf;
	U_LONG count;
	u_short flags;
	u_short mflags;
	char flagstr[300];
	static char *comma = ", ";

	res = doquery(IMPL_XNTPD, REQ_GET_RESTRICT, 0, 0, 0, (char *)NULL,
	    &items, &itemsize, (char **)&rl);
	
	if (res != 0 && items == 0)
		return;

	if (!checkitems(items, fp))
		return;

	if (!checkitemsize(itemsize, sizeof(struct info_restrict)))
		return;

	(void) fprintf(fp,
    "   address          mask            count        flags\n");
	(void) fprintf(fp,
    "=====================================================================\n");
	while (items > 0) {
		addr = numtoa(rl->addr);
		mask = numtoa(rl->mask);
		count = ntohl(rl->count);
		flags = ntohs(rl->flags);
		mflags = ntohs(rl->mflags);
		flagstr[0] = '\0';

		res = 1;
		rf = &resmflags[0];
		while (rf->bit != 0) {
			if (mflags & rf->bit) {
				if (!res)
					(void) strcat(flagstr, comma);
				res = 0;
				(void) strcat(flagstr, rf->str);
			}
			rf++;
		}

		rf = &resflags[0];
		while (rf->bit != 0) {
			if (flags & rf->bit) {
				if (!res)
					(void) strcat(flagstr, comma);
				res = 0;
				(void) strcat(flagstr, rf->str);
			}
			rf++;
		}

		if (flagstr[0] == '\0')
			(void) strcpy(flagstr, "none");

		(void) fprintf(fp, "%-15.15s %-15.15s %9d  %s\n",
		    addr, mask, count, flagstr);
		rl++;
		items--;
	}
}



/*
 * restrict - create/add a set of restrictions
 */
static void
restrict(pcmd, fp)
	struct parse *pcmd;
	FILE *fp;
{
	do_restrict(pcmd, fp, REQ_RESADDFLAGS);
}


/*
 * unrestrict - remove restriction flags from existing entry
 */
static void
unrestrict(pcmd, fp)
	struct parse *pcmd;
	FILE *fp;
{
	do_restrict(pcmd, fp, REQ_RESSUBFLAGS);
}


/*
 * delrestrict - delete an existing restriction
 */
static void
delrestrict(pcmd, fp)
	struct parse *pcmd;
	FILE *fp;
{
	do_restrict(pcmd, fp, REQ_UNRESTRICT);
}


/*
 * do_restrict - decode commandline restrictions and make the request
 */
static void
do_restrict(pcmd, fp, req_code)
	struct parse *pcmd;
	FILE *fp;
	int req_code;
{
	struct conf_restrict cres;
	int items;
	int itemsize;
	char *dummy;
	U_LONG num;
	U_LONG bit;
	int i;
	int res;
	int err;

	cres.addr = pcmd->argval[0].netnum;
	cres.mask = pcmd->argval[1].netnum;
	cres.flags = 0;
	cres.mflags = 0;
	err = 0;
	for (res = 2; res < pcmd->nargs; res++) {
		if (STREQ(pcmd->argval[res].string, "ntpport")) {
			cres.mflags |= RESM_NTPONLY;
		} else {
			for (i = 0; resflags[i].bit != 0; i++) {
				if (STREQ(pcmd->argval[res].string,
				    resflags[i].str))
					break;
			}
			if (resflags[i].bit != 0) {
				cres.flags |= resflags[i].bit;
				if (req_code == REQ_UNRESTRICT) {
					(void) fprintf(fp,
					    "Flag `%s' inappropriate\n",
					    resflags[i].str);
					err++;
				}
			} else {
				(void) fprintf(fp, "Unknown flag %s\n",
				    pcmd->argval[res].string);
				err++;
			}
		}
	}

	/*
	 * Make sure mask for default address is zero.  Otherwise,
	 * make sure mask bits are contiguous.
	 */
	if (cres.addr == 0) {
		cres.mask = 0;
	} else {
		num = ntohl(cres.mask);
		for (bit = 0x80000000; bit != 0; bit >>= 1)
			if ((num & bit) == 0)
				break;
		for ( ; bit != 0; bit >>= 1)
			if ((num & bit) != 0)
				break;
		if (bit != 0) {
			(void) fprintf(fp, "Invalid mask %s\n",
			    numtoa(cres.mask));
			err++;
		}
	}

	if (err)
		return;

	res = doquery(IMPL_XNTPD, req_code, 1, 1,
	    sizeof(struct conf_restrict), (char *)&cres, &items,
	    &itemsize, &dummy);
	
	if (res == 0)
		(void) fprintf(fp, "done!\n");
	return;
}


/*
 * monlist - obtain and print the server's monitor data
 */
/*ARGSUSED*/
static void
monlist(pcmd, fp)
	struct parse *pcmd;
	FILE *fp;
{
	struct info_monitor *ml;
	int items;
	int itemsize;
	int res;

	res = doquery(IMPL_XNTPD, REQ_MON_GETLIST, 0, 0, 0, (char *)NULL,
	    &items, &itemsize, (char **)&ml);
	
	if (res != 0 && items == 0)
		return;

	if (!checkitems(items, fp))
		return;

	if (!checkitemsize(itemsize, sizeof(struct info_monitor)))
		return;

	(void) fprintf(fp,
    "     address          port     count  mode version lasttime firsttime\n");
	(void) fprintf(fp,
    "=====================================================================\n");
	while (items > 0) {
		(void) fprintf(fp, "%-20.20s %5d %9d %4d   %3d %9u %9u\n",
		    nntohost(ml->addr),
		    ntohs(ml->port),
		    ntohl(ml->count),
		    ml->mode, ml->version,
		    ntohl(ml->lasttime),
		    ntohl(ml->firsttime));
		ml++;
		items--;
	}
}


/*
 * monitor - turn the server's monitor facility on or off
 */
static void
monitor(pcmd, fp)
	struct parse *pcmd;
	FILE *fp;
{
	int items;
	int itemsize;
	char *dummy;
	int req_code;
	int res;

	if (STREQ(pcmd->argval[0].string, "on"))
		req_code = REQ_MONITOR;
	else if (STREQ(pcmd->argval[0].string, "off"))
		req_code = REQ_NOMONITOR;
	else {
		(void) fprintf(fp, "monitor what?\n");
		return;
	}

	res = doquery(IMPL_XNTPD, req_code, 1, 0, 0, (char *)0,
	    &items, &itemsize, &dummy);
	
	if (res == 0)
		(void) fprintf(fp, "done!\n");
	return;
}



/*
 * Mapping between command line strings and stat reset flags
 */
struct statreset {
	char *str;
	int flag;
} sreset[] = {
	{ "io",		RESET_FLAG_IO },
	{ "sys",	RESET_FLAG_SYS },
	{ "mem",	RESET_FLAG_MEM },
	{ "timer",	RESET_FLAG_TIMER },
	{ "auth",	RESET_FLAG_AUTH },
	{ "allpeers",	RESET_FLAG_ALLPEERS },
	{ "",		0 }
};

/*
 * reset - reset statistic counters
 */
static void
reset(pcmd, fp)
	struct parse *pcmd;
	FILE *fp;
{
	struct reset_flags rflags;
	int items;
	int itemsize;
	char *dummy;
	int i;
	int res;
	int err;

	err = 0;
	rflags.flags = 0;
	for (res = 0; res < pcmd->nargs; res++) {
		for (i = 0; sreset[i].flag != 0; i++) {
			if (STREQ(pcmd->argval[res].string, sreset[i].str))
				break;
		}
		if (sreset[i].flag == 0) {
			(void) fprintf(fp, "Flag `%s' unknown\n",
			    pcmd->argval[res].string);
			err++;
		} else {
			rflags.flags |= sreset[i].flag;
		}
	}

	if (err) {
		(void) fprintf(fp, "Not done due to errors\n");
		return;
	}

	res = doquery(IMPL_XNTPD, REQ_RESET_STATS, 1, 1,
	    sizeof(struct reset_flags), (char *)&rflags, &items,
	    &itemsize, &dummy);
	
	if (res == 0)
		(void) fprintf(fp, "done!\n");
	return;
}



/*
 * preset - reset stat counters for particular peers
 */
static void
preset(pcmd, fp)
	struct parse *pcmd;
	FILE *fp;
{
	/* 8 is the maximum number of peers which will fit in a packet */
	struct conf_unpeer plist[min(MAXARGS, 8)];
	int qitems;
	int items;
	int itemsize;
	char *dummy;
	int res;

	for (qitems = 0; qitems < min(pcmd->nargs, 8); qitems++) {
		plist[qitems].peeraddr = pcmd->argval[qitems].netnum;
	}

	res = doquery(IMPL_XNTPD, REQ_RESET_PEER, 1, qitems,
	    sizeof(struct conf_unpeer), (char *)plist, &items,
	    &itemsize, &dummy);
	
	if (res == 0)
		(void) fprintf(fp, "done!\n");
}


/*
 * readkeys - request the server to reread the keys file
 */
/*ARGSUSED*/
static void
readkeys(pcmd, fp)
	struct parse *pcmd;
	FILE *fp;
{
	int items;
	int itemsize;
	char *dummy;
	int res;

	res = doquery(IMPL_XNTPD, REQ_REREAD_KEYS, 1, 0, 0, (char *)0,
	    &items, &itemsize, &dummy);
	
	if (res == 0)
		(void) fprintf(fp, "done!\n");
	return;
}


/*
 * dodirty - request the server to do something dirty
 */
/*ARGSUSED*/
static void
dodirty(pcmd, fp)
	struct parse *pcmd;
	FILE *fp;
{
	int items;
	int itemsize;
	char *dummy;
	int res;

	res = doquery(IMPL_XNTPD, REQ_DO_DIRTY_HACK, 1, 0, 0, (char *)0,
	    &items, &itemsize, &dummy);
	
	if (res == 0)
		(void) fprintf(fp, "done!\n");
	return;
}


/*
 * dontdirty - request the server to not do something dirty
 */
/*ARGSUSED*/
static void
dontdirty(pcmd, fp)
	struct parse *pcmd;
	FILE *fp;
{
	int items;
	int itemsize;
	char *dummy;
	int res;

	res = doquery(IMPL_XNTPD, REQ_DONT_DIRTY_HACK, 1, 0, 0, (char *)0,
	    &items, &itemsize, &dummy);
	
	if (res == 0)
		(void) fprintf(fp, "done!\n");
	return;
}


/*
 * trustkey - add some keys to the trusted key list
 */
static void
trustkey(pcmd, fp)
	struct parse *pcmd;
	FILE *fp;
{
	do_trustkey(pcmd, fp, REQ_TRUSTKEY);
}


/*
 * untrustkey - remove some keys from the trusted key list
 */
static void
untrustkey(pcmd, fp)
	struct parse *pcmd;
	FILE *fp;
{
	do_trustkey(pcmd, fp, REQ_UNTRUSTKEY);
}


/*
 * do_trustkey - do grunge work of adding/deleting keys
 */
static void
do_trustkey(pcmd, fp, req)
	struct parse *pcmd;
	FILE *fp;
	int req;
{
	U_LONG keyids[MAXARGS];
	int i;
	int items;
	int itemsize;
	char *dummy;
	int ritems;
	int res;

	ritems = 0;
	for (i = 0; i < pcmd->nargs; i++) {
		keyids[ritems++] = pcmd->argval[i].uval;
	}

	res = doquery(IMPL_XNTPD, req, 1, ritems, sizeof(U_LONG),
	    (char *)keyids, &items, &itemsize, &dummy);
	
	if (res == 0)
		(void) fprintf(fp, "done!\n");
	return;
}



/*
 * authinfo - obtain and print info about authentication
 */
/*ARGSUSED*/
static void
authinfo(pcmd, fp)
	struct parse *pcmd;
	FILE *fp;
{
	struct info_auth *ia;
	int items;
	int itemsize;
	int res;

	res = doquery(IMPL_XNTPD, REQ_AUTHINFO, 0, 0, 0, (char *)NULL,
	    &items, &itemsize, (char **)&ia);
	
	if (res != 0 && items == 0)
		return;

	if (!check1item(items, fp))
		return;

	if (!checkitemsize(itemsize, sizeof(struct info_auth)))
		return;

	(void) fprintf(fp, "time since reset:       %d\n",
	    ntohl(ia->timereset));
	(void) fprintf(fp, "key lookups:            %d\n",
	    ntohl(ia->keylookups));
	(void) fprintf(fp, "keys not found:         %d\n",
	    ntohl(ia->keynotfound));
	(void) fprintf(fp, "encryptions:            %d\n",
	    ntohl(ia->encryptions));
	(void) fprintf(fp, "decryptions:            %d\n",
	    ntohl(ia->decryptions));
	(void) fprintf(fp, "successful decryptions: %d\n",
	    ntohl(ia->decryptions));
	(void) fprintf(fp, "uncached keys:          %d\n",
	    ntohl(ia->keyuncached));
}



/*
 * traps - obtain and print a list of traps
 */
/*ARGSUSED*/
static void
traps(pcmd, fp)
	struct parse *pcmd;
	FILE *fp;
{
	int i;
	struct info_trap *it;
	int items;
	int itemsize;
	int res;

	res = doquery(IMPL_XNTPD, REQ_TRAPS, 0, 0, 0, (char *)NULL,
	    &items, &itemsize, (char **)&it);
	
	if (res != 0 && items == 0)
		return;

	if (!checkitems(items, fp))
		return;

	if (!checkitemsize(itemsize, sizeof(struct info_trap)))
		return;

	for (i = 0; i < items; i++ ) {
		if (i != 0)
			(void) fprintf(fp, "\n");
		(void) fprintf(fp, "address %s, port %d\n",
		    numtoa(it->trap_address), ntohs(it->trap_port));
		(void) fprintf(fp, "interface: %s, ",
		   it->local_address==0?"wildcard":numtoa(it->local_address));

		if (htonl(it->flags) & TRAP_CONFIGURED)
			(void) fprintf(fp, "configured\n");
		else if (it->flags & TRAP_NONPRIO)
			(void) fprintf(fp, "low priority\n");
		else
			(void) fprintf(fp, "normal priority\n");
		
		(void) fprintf(fp, "set for %d secs, last set %d secs ago\n",
		    it->origtime, it->settime);
		(void) fprintf(fp, "sequence %d, number of resets %d\n",
		    it->sequence, it->resets);
	}
}


/*
 * addtrap - configure a trap
 */
static void
addtrap(pcmd, fp)
	struct parse *pcmd;
	FILE *fp;
{
	do_addclr_trap(pcmd, fp, REQ_ADD_TRAP);
}


/*
 * clrtrap - clear a trap from the server
 */
static void
clrtrap(pcmd, fp)
	struct parse *pcmd;
	FILE *fp;
{
	do_addclr_trap(pcmd, fp, REQ_CLR_TRAP);
}


/*
 * do_addclr_trap - do grunge work of adding/deleting traps
 */
static void
do_addclr_trap(pcmd, fp, req)
	struct parse *pcmd;
	FILE *fp;
	int req;
{
	struct conf_trap ctrap;
	int items;
	int itemsize;
	char *dummy;
	int res;

	ctrap.trap_address = pcmd->argval[0].netnum;
	ctrap.local_address = 0;
	ctrap.trap_port = htons(TRAPPORT);
	ctrap.unused = 0;

	if (pcmd->nargs > 1) {
		ctrap.trap_port
		    = htons((u_short)(pcmd->argval[1].uval & 0xffff));
		if (pcmd->nargs > 2)
			ctrap.local_address = pcmd->argval[2].netnum;
	}

	res = doquery(IMPL_XNTPD, req, 1, 1, sizeof(struct conf_trap),
	    (char *)&ctrap, &items, &itemsize, &dummy);
	
	if (res == 0)
		(void) fprintf(fp, "done!\n");
	return;
}



/*
 * requestkey - change the server's request key (a dangerous request)
 */
static void
requestkey(pcmd, fp)
	struct parse *pcmd;
	FILE *fp;
{
	do_changekey(pcmd, fp, REQ_REQUEST_KEY);
}


/*
 * controlkey - change the server's control key
 */
static void
controlkey(pcmd, fp)
	struct parse *pcmd;
	FILE *fp;
{
	do_changekey(pcmd, fp, REQ_CONTROL_KEY);
}



/*
 * do_changekey - do grunge work of changing keys
 */
static void
do_changekey(pcmd, fp, req)
	struct parse *pcmd;
	FILE *fp;
	int req;
{
	U_LONG key;
	int items;
	int itemsize;
	char *dummy;
	int res;


	key = htonl(pcmd->argval[0].uval);

	res = doquery(IMPL_XNTPD, req, 1, 1, sizeof(U_LONG),
	    (char *)&key, &items, &itemsize, &dummy);
	
	if (res == 0)
		(void) fprintf(fp, "done!\n");
	return;
}



/*
 * ctlstats - obtain and print info about authentication
 */
/*ARGSUSED*/
static void
ctlstats(pcmd, fp)
	struct parse *pcmd;
	FILE *fp;
{
	struct info_control *ic;
	int items;
	int itemsize;
	int res;

	res = doquery(IMPL_XNTPD, REQ_GET_CTLSTATS, 0, 0, 0, (char *)NULL,
	    &items, &itemsize, (char **)&ic);
	
	if (res != 0 && items == 0)
		return;

	if (!check1item(items, fp))
		return;

	if (!checkitemsize(itemsize, sizeof(struct info_control)))
		return;

	(void) fprintf(fp, "time since reset:       %d\n",
	    ntohl(ic->ctltimereset));
	(void) fprintf(fp, "requests received:      %d\n",
	    ntohl(ic->numctlreq));
	(void) fprintf(fp, "responses sent:         %d\n",
	    ntohl(ic->numctlresponses));
	(void) fprintf(fp, "fragments sent:         %d\n",
	    ntohl(ic->numctlfrags));
	(void) fprintf(fp, "async messages sent:    %d\n",
	    ntohl(ic->numasyncmsgs));
	(void) fprintf(fp, "error msgs sent:        %d\n",
	    ntohl(ic->numctlerrors));
	(void) fprintf(fp, "total bad pkts:         %d\n",
	    ntohl(ic->numctlbadpkts));
	(void) fprintf(fp, "packet too short:       %d\n",
	    ntohl(ic->numctltooshort));
	(void) fprintf(fp, "response on input:      %d\n",
	    ntohl(ic->numctlinputresp));
	(void) fprintf(fp, "fragment on input:      %d\n",
	    ntohl(ic->numctlinputfrag));
	(void) fprintf(fp, "error set on input:     %d\n",
	    ntohl(ic->numctlinputerr));
	(void) fprintf(fp, "bad offset on input:    %d\n",
	    ntohl(ic->numctlbadoffset));
	(void) fprintf(fp, "bad version packets:    %d\n",
	    ntohl(ic->numctlbadversion));
	(void) fprintf(fp, "data in pkt too short:  %d\n",
	    ntohl(ic->numctldatatooshort));
	(void) fprintf(fp, "unknown op codes:       %d\n",
	    ntohl(ic->numctlbadop));
}



/*
 * Table for human printing leap bits
 */
char *leapbittab[] = {
	"00 (no leap second scheduled)",
	"01 (second to be added at end of month)",
	"10 (second to be deleted at end of month)",
	"11 (clock out of sync)"
};

char *controlleapbittab[] = {
	"00 (leap controlled by lower stratum)",
	"01 (second to be added at end of month)",
	"10 (second to be deleted at end of month)",
	"11 (lower stratum leap information ignored - no leap)"
};

/*
 * leapinfo - obtain information about the state of the leap second support
 */
/*ARGSUSED*/
static void
leapinfo(pcmd, fp)
	struct parse *pcmd;
	FILE *fp;
{
	struct info_leap *il;
	int items;
	int itemsize;
	int res;
	l_fp ts;

	res = doquery(IMPL_XNTPD, REQ_GET_LEAPINFO, 0, 0, 0, (char *)NULL,
	    &items, &itemsize, (char **)&il);
	
	if (res != 0 && items == 0)
		return;

	if (!check1item(items, fp))
		return;

	if (!checkitemsize(itemsize, sizeof(struct info_leap)))
		return;

	(void) fprintf(fp, "sys.leap:       %s\n",
	    leapbittab[il->sys_leap & INFO_LEAP_MASK]);
	(void) fprintf(fp, "leap.indicator: %s\n",
	    controlleapbittab[il->leap_indicator & INFO_LEAP_MASK]);
	(void) fprintf(fp, "leap.warning:   %s\n",
	    controlleapbittab[il->leap_warning & INFO_LEAP_MASK]);
	(void) fprintf(fp, "leap.bits:      %s\n",
	    leapbittab[il->leap_bits & INFO_LEAP_MASK]);
	if (il->leap_bits & INFO_LEAP_OVERRIDE)
		(void) fprintf(fp, "Leap overide option in effect\n");
 	if (il->leap_bits & INFO_LEAP_SEENSTRATUM1)
 		(void) fprintf(fp, "Stratum 1 restrictions in effect\n");
	(void) fprintf(fp, "time to next leap interrupt: %d seconds\n",
	    ntohl(il->leap_timer));
	gettstamp(&ts);
	(void) fprintf(fp, "date of next leap interrupt: %s\n",
	    humandate(ts.l_ui + ntohl(il->leap_timer)));
	(void) fprintf(fp, "calls to leap process: %u\n",
	    ntohl(il->leap_processcalls));
	(void) fprintf(fp, "leap more than month away: %u\n",
	    ntohl(il->leap_notclose));
	(void) fprintf(fp, "leap less than month away: %u\n",
	    ntohl(il->leap_monthofleap));
	(void) fprintf(fp, "leap less than day away:   %u\n",
	    ntohl(il->leap_dayofleap));
	(void) fprintf(fp, "leap in less than 2 hours: %u\n",
	    ntohl(il->leap_hoursfromleap));
	(void) fprintf(fp, "leap happened:             %u\n",
	    ntohl(il->leap_happened));
}


/*
 * clockstat - get and print clock status information
 */
static void
clockstat(pcmd, fp)
	struct parse *pcmd;
	FILE *fp;
{
        extern struct clktype clktypes[];
	struct info_clock *cl;
	/* 8 is the maximum number of clocks which will fit in a packet */
	U_LONG clist[min(MAXARGS, 8)];
	int qitems;
	int items;
	int itemsize;
	int res;
	l_fp ts;
	struct clktype *clk;

	for (qitems = 0; qitems < min(pcmd->nargs, 8); qitems++)
		clist[qitems] = pcmd->argval[qitems].netnum;

	res = doquery(IMPL_XNTPD, REQ_GET_CLOCKINFO, 0, qitems,
	    sizeof(U_LONG), (char *)clist, &items,
	    &itemsize, (char **)&cl);
	
	if (res != 0 && items == 0)
		return;

	if (!checkitems(items, fp))
		return;

	if (!checkitemsize(itemsize, sizeof(struct info_clock)))
		return;

	while (items-- > 0) {
		(void) fprintf(fp, "clock address:        %s\n",
		    numtoa(cl->clockadr));
		for (clk = clktypes; clk->code >= 0; clk++)
			if (clk->code == cl->type)
				break;
		if (clk->code >= 0)
			(void) fprintf(fp, "clock type:   %s\n",
			    clk->clocktype);
		else
			(void) fprintf(fp, "clock type:   unknown type (%d)\n",
			    cl->type);
		(void) fprintf(fp, "last event:           %d\n",
		    cl->lastevent);
		(void) fprintf(fp, "current status:       %d\n",
		    cl->currentstatus);
		(void) fprintf(fp, "number of polls:      %u\n",
		    ntohl(cl->polls));
		(void) fprintf(fp, "no response to poll:  %u\n",
		    ntohl(cl->noresponse));
		(void) fprintf(fp, "bad format responses: %u\n",
		    ntohl(cl->badformat));
		(void) fprintf(fp, "bad data responses:   %u\n",
		    ntohl(cl->baddata));
		(void) fprintf(fp, "running time:         %u\n",
		    ntohl(cl->timestarted));
		NTOHL_FP(&cl->fudgetime1, &ts);
		(void) fprintf(fp, "fudge time 1:         %s\n",
		    lfptoa(&ts, 7));
		NTOHL_FP(&cl->fudgetime2, &ts);
		(void) fprintf(fp, "fudge time 2:         %s\n",
		    lfptoa(&ts, 7));
		(void) fprintf(fp, "fudge value 1:        %ld\n",
		    ntohl(cl->fudgeval1));
		(void) fprintf(fp, "fudge value 2:        %ld\n",
		    ntohl(cl->fudgeval2));
		(void) fprintf(fp, "fudge flags:          0x%x\n",
		    cl->flags);

		if (items > 0)
			(void) fprintf(fp, "\n");
		cl++;
	}
}


/*
 * fudge - set clock fudge factors
 */
static void
fudge(pcmd, fp)
	struct parse *pcmd;
	FILE *fp;
{
	struct conf_fudge fudgedata;
	int items;
	int itemsize;
	char *dummy;
	l_fp ts;
	int res;
	LONG val;
	int err;


	err = 0;
	memset((char *)&fudgedata, 0, sizeof fudgedata);
	fudgedata.clockadr = pcmd->argval[0].netnum;

	if (STREQ(pcmd->argval[1].string, "time1")) {
		fudgedata.which = htonl(FUDGE_TIME1);
		if (!atolfp(pcmd->argval[2].string, &ts))
			err = 1;
		else
			HTONL_FP(&ts, &fudgedata.fudgetime);
	} else if (STREQ(pcmd->argval[1].string, "time2")) {
		fudgedata.which = htonl(FUDGE_TIME2);
		if (!atolfp(pcmd->argval[2].string, &ts))
			err = 1;
		else
			HTONL_FP(&ts, &fudgedata.fudgetime);
	} else if (STREQ(pcmd->argval[1].string, "val1")) {
		fudgedata.which = htonl(FUDGE_VAL1);
		if (!atoint(pcmd->argval[2].string, &val))
			err = 1;
		else
			fudgedata.fudgeval_flags = htonl(val);
	} else if (STREQ(pcmd->argval[1].string, "val2")) {
		fudgedata.which = htonl(FUDGE_VAL2);
		if (!atoint(pcmd->argval[2].string, &val))
			err = 1;
		else
			fudgedata.fudgeval_flags = htonl(val);
	} else if (STREQ(pcmd->argval[1].string, "flags")) {
		fudgedata.which = htonl(FUDGE_FLAGS);
		if (!atoint(pcmd->argval[2].string, &val))
			err = 1;
		else
			fudgedata.fudgeval_flags = htonl(val & 0xf);
	} else {
		(void) fprintf(stderr, "What fudge is `%s'?\n",
		    pcmd->argval[1].string);
		return;
	}

	if (err) {
		(void) fprintf(stderr, "Can't decode the value `%s'\n",
		    pcmd->argval[2].string);
		return;
	}


	res = doquery(IMPL_XNTPD, REQ_SET_CLKFUDGE, 1, 1,
	    sizeof(struct conf_fudge), (char *)&fudgedata, &items,
	    &itemsize, &dummy);

	if (res == 0)
		(void) fprintf(fp, "done!\n");
	return;
}



/*
 * maxskew - set the server's maximum skew parameter
 */
static void
maxskew(pcmd, fp)
	struct parse *pcmd;
	FILE *fp;
{
	u_fp Xmaxskew;
	l_fp tmp;
	int items;
	int itemsize;
	char *dummy;
	int res;

	if (!atolfp(pcmd->argval[0].string, &tmp)) {
		(void) fprintf(stderr, "What the heck does %s mean?\n",
		    pcmd->argval[0].string);
		return;
	}
	Xmaxskew = HTONS_FP(LFPTOFP(&tmp));

	res = doquery(IMPL_XNTPD, REQ_SET_MAXSKEW, 1, 1, sizeof(u_fp),
	    (char *)&Xmaxskew, &items, &itemsize, &dummy);
	
	if (res == 0)
		(void) fprintf(fp, "done!\n");
}



/*
 * clkbug - get and print clock debugging information
 */
static void
clkbug(pcmd, fp)
	struct parse *pcmd;
	FILE *fp;
{
	register int i;
	register int n;
	register U_LONG s;
	struct info_clkbug *cl;
	/* 8 is the maximum number of clocks which will fit in a packet */
	U_LONG clist[min(MAXARGS, 8)];
	int qitems;
	int items;
	int itemsize;
	int res;
	l_fp ts;

	for (qitems = 0; qitems < min(pcmd->nargs, 8); qitems++)
		clist[qitems] = pcmd->argval[qitems].netnum;

	res = doquery(IMPL_XNTPD, REQ_GET_CLKBUGINFO, 0, qitems,
	    sizeof(U_LONG), (char *)clist, &items,
	    &itemsize, (char **)&cl);
	
	if (res != 0 && items == 0)
		return;

	if (!checkitems(items, fp))
		return;

	if (!checkitemsize(itemsize, sizeof(struct info_clkbug)))
		return;

	while (items-- > 0) {
		(void) fprintf(fp, "clock address:        %s\n",
		    numtoa(cl->clockadr));
		n = (int)cl->nvalues;
		(void) fprintf(fp, "values: %d", n);
		s = (U_LONG)ntohs(cl->svalues);
		if (n > NUMCBUGVALUES)
			n = NUMCBUGVALUES;
		for (i = 0; i < n; i++) {
			if ((i & 0x3) == 0)
				(void) fprintf(fp, "\n");
			if (s & (1<<i)) {
				(void) fprintf(fp, "%12ld",
				    (LONG)ntohl(cl->values[i]));
			} else {
				(void) fprintf(fp, "%12lu",
				    ntohl(cl->values[i]));
			}
		}
		(void) fprintf(fp, "\n");

		n = (int)cl->ntimes;
		(void) fprintf(fp, "times: %d", n);
		s = ntohl(cl->stimes);
		if (n > NUMCBUGTIMES)
			n = NUMCBUGTIMES;
		for (i = 0; i < n; i++) {
			int needsp = 0;
			if ((i & 0x1) == 0)
				(void) fprintf(fp, "\n");
			else {
				for (;needsp > 0; needsp--)
					putc(' ', fp);
			}
			HTONL_FP(&cl->times[i], &ts);
			if (s & (1<<i)) {
				(void) fprintf(fp, "%17s",
				    lfptoa(&ts, 6));
				needsp = 22;
			} else {
				(void) fprintf(fp, "%37s",
					uglydate(&ts));
				needsp = 2;
			}
		}
		(void) fprintf(fp, "\n");
		if (items > 0) {
			cl++;
			(void) fprintf(fp, "\n");
		}
	}
}


/*
 * setprecision - set the server's value of sys.precision
 */
static void
setprecision(pcmd, fp)
	struct parse *pcmd;
	FILE *fp;
{
	LONG precision;
	int items;
	int itemsize;
	char *dummy;
	int res;

	precision = htonl(pcmd->argval[0].ival);

	res = doquery(IMPL_XNTPD, REQ_SET_PRECISION, 1, 1, sizeof(LONG),
	    (char *)&precision, &items, &itemsize, &dummy);
	
	if (res == 0)
		(void) fprintf(fp, "done!\n");
	return;
}



/*
 * setselect - change the server's selection algorithm
 */
static void
setselect(pcmd, fp)
	struct parse *pcmd;
	FILE *fp;
{
	U_LONG select_code;
	int items;
	int itemsize;
	char *dummy;
	int res;

	select_code = htonl(pcmd->argval[0].uval);

	res = doquery(IMPL_XNTPD, REQ_SET_SELECT_CODE, 1, 1, sizeof(U_LONG),
	    (char *)&select_code, &items, &itemsize, &dummy);
	
	if (res == 0)
		(void) fprintf(fp, "done!\n");
	return;
}
