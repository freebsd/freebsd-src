/*
 * pretty printing of status information
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include "ntp_stdlib.h"
#include "ntp_fp.h"
#include "ntp.h"
#include "lib_strbuf.h"
#include "ntp_refclock.h"
#include "ntp_control.h"
#include "ntp_string.h"

/*
 * Structure for turning various constants into a readable string.
 */
struct codestring {
	int code;
	const char *string;
};

/*
 * Leap values
 */
static
struct codestring leap_codes[] = {
	{ LEAP_NOWARNING,	"leap_none" },
	{ LEAP_ADDSECOND,	"leap_add_sec" },
	{ LEAP_DELSECOND,	"leap_del_sec" },
	{ LEAP_NOTINSYNC,	"sync_alarm" },
	{ -1,	"leap" }
};

/*
 * Clock source
 */
static
struct codestring sync_codes[] = {
	{ CTL_SST_TS_UNSPEC,	"sync_unspec" },
	{ CTL_SST_TS_ATOM,	"sync_atomic" },
	{ CTL_SST_TS_LF,	"sync_lf_clock" },
	{ CTL_SST_TS_HF,	"sync_hf_clock" },
	{ CTL_SST_TS_UHF,	"sync_uhf_clock" },
	{ CTL_SST_TS_LOCAL,	"sync_local_proto" },
	{ CTL_SST_TS_NTP,	"sync_ntp" },
	{ CTL_SST_TS_UDPTIME,	"sync_udp/time" },
	{ CTL_SST_TS_WRSTWTCH,	"sync_wristwatch" },
	{ CTL_SST_TS_TELEPHONE,	"sync_telephone" },
	{ -1,			"sync" }
};


/*
 * Peer selection
 */
static
struct codestring select_codes[] = {
	{ CTL_PST_SEL_REJECT,	"selreject" },
	{ CTL_PST_SEL_SANE,	"sel_falsetick" },
	{ CTL_PST_SEL_CORRECT,	"sel_excess" },
	{ CTL_PST_SEL_SELCAND,	"sel_outlyer" },
	{ CTL_PST_SEL_SYNCCAND,	"sel_candidat" },
	{ CTL_PST_SEL_DISTSYSPEER, "sel_selected" },
	{ CTL_PST_SEL_SYSPEER,	"sel_sys.peer" },
	{ CTL_PST_SEL_PPS,	"sel_pps.peer" },
	{ -1,			"sel" }
};


/*
 * Clock status
 */
static
struct codestring clock_codes[] = {
	{ CTL_CLK_OKAY,		"clk_okay" },
	{ CTL_CLK_NOREPLY,	"clk_noreply" },
	{ CTL_CLK_BADFORMAT,	"clk_badformat" },
	{ CTL_CLK_FAULT,	"clk_fault" },
	{ CTL_CLK_PROPAGATION,	"clk_badsignal" },
	{ CTL_CLK_BADDATE,	"clk_baddate" },
	{ CTL_CLK_BADTIME,	"clk_badtime" },
	{ -1,			"clk" }
};


/*
 * System Events
 */
static
struct codestring sys_codes[] = {
	{ EVNT_UNSPEC,		"event_unspec" },
	{ EVNT_SYSRESTART,	"event_restart" },
	{ EVNT_SYSFAULT,	"event_fault" },
	{ EVNT_SYNCCHG,		"event_sync_chg" },
	{ EVNT_PEERSTCHG,	"event_peer/strat_chg" },
	{ EVNT_CLOCKRESET,	"event_clock_reset" },
	{ EVNT_BADDATETIM,	"event_bad_date" },
	{ EVNT_CLOCKEXCPT,	"event_clock_excptn" },
	{ -1,			"event" }
};

/*
 * Peer Events
 */
static
struct codestring peer_codes[] = {
	{ EVNT_UNSPEC,			"event_unspec" },
	{ EVNT_PEERIPERR & ~PEER_EVENT,	"event_ip_err" },
	{ EVNT_PEERAUTH & ~PEER_EVENT,	"event_authen" },
	{ EVNT_UNREACH & ~PEER_EVENT,	"event_unreach" },
	{ EVNT_REACH & ~PEER_EVENT,	"event_reach" },
	{ EVNT_PEERCLOCK & ~PEER_EVENT,	"event_peer_clock" },
#if 0
	{ EVNT_PEERSTRAT & ~PEER_EVENT,	"event_stratum_chg" },
#endif
	{ -1,				"event" }
};

/* Forwards */
static const char *getcode P((int, struct codestring *));
static const char *getevents P((int));

/*
 * getcode - return string corresponding to code
 */
static const char *
getcode(
	int code,
	struct codestring *codetab
	)
{
	static char buf[30];

	while (codetab->code != -1) {
		if (codetab->code == code)
		    return codetab->string;
		codetab++;
	}
	(void) sprintf(buf, "%s_%d", codetab->string, code);
	return buf;
}

/*
 * getevents - return a descriptive string for the event count
 */
static const char *
getevents(
	int cnt
	)
{
	static char buf[20];

	if (cnt == 0)
	    return "no events";
	(void) sprintf(buf, "%d event%s", cnt, (cnt==1) ? "" : "s");
	return buf;
}

/*
 * statustoa - return a descriptive string for a peer status
 */
char *
statustoa(
	int type,
	int st
	)
{
	char *cb;
	u_char pst;

	LIB_GETBUF(cb);

	switch (type) {
	    case TYPE_SYS:
		(void)strcpy(cb, getcode(CTL_SYS_LI(st), leap_codes));
		(void)strcat(cb, ", ");
		(void)strcat(cb, getcode(CTL_SYS_SOURCE(st) & ~CTL_SST_TS_PPS, sync_codes));
		if (CTL_SYS_SOURCE(st) & CTL_SST_TS_PPS)
		    (void)strcat(cb, "/PPS");
		(void)strcat(cb, ", ");
		(void)strcat(cb, getevents(CTL_SYS_NEVNT(st)));
		(void)strcat(cb, ", ");
		(void)strcat(cb, getcode(CTL_SYS_EVENT(st), sys_codes));
		break;
	
	    case TYPE_PEER:
		/*
		 * Handcraft the bits
		 */
		pst = (u_char) CTL_PEER_STATVAL(st);
		if (!(pst & CTL_PST_REACH)) {
			(void)strcpy(cb, "unreach");
		} else {
			(void)strcpy(cb, "reach");

		}
		if (pst & CTL_PST_CONFIG)
		    (void)strcat(cb, ", conf");
		if (pst & CTL_PST_AUTHENABLE) {
			if (!(pst & CTL_PST_REACH) || (pst & CTL_PST_AUTHENTIC))
			    (void)strcat(cb, ", auth");
			else
			    (void)strcat(cb, ", unauth");
		}

		/*
		 * Now the codes
		 */
		if ((pst & 0x7) != CTL_PST_SEL_REJECT) {
			(void)strcat(cb, ", ");
			(void)strcat(cb, getcode(pst & 0x7, select_codes));
		}
		(void)strcat(cb, ", ");
		(void)strcat(cb, getevents(CTL_PEER_NEVNT(st)));
		if (CTL_PEER_EVENT(st) != EVNT_UNSPEC) {
			(void)strcat(cb, ", ");
			(void)strcat(cb, getcode(CTL_PEER_EVENT(st),
						 peer_codes));
		}
		break;
	
	    case TYPE_CLOCK:
		(void)strcpy(cb, getcode(((st)>>8) & 0xff, clock_codes));
		(void)strcat(cb, ", last_");
		(void)strcat(cb, getcode((st) & 0xff, clock_codes));
		break;
	}
	return cb;
}

const char *
eventstr(
	int num
	)
{
	return getcode(num & ~PEER_EVENT, (num & PEER_EVENT) ? peer_codes : sys_codes);
}

const char *
ceventstr(
	int num
	)
{
	return getcode(num, clock_codes);
}

const char *
sysstatstr(
	int status
	)
{
	return statustoa(TYPE_SYS, status);
}

const char *
peerstatstr(
	int status
	)
{
	return statustoa(TYPE_PEER, status);
}

const char *
clockstatstr(
	int status
	)
{
	return statustoa(TYPE_CLOCK, status);
}
