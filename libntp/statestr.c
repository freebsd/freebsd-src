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
 * Leap status (leap)
 */
static
struct codestring leap_codes[] = {
	{ LEAP_NOWARNING,	"leap_none" },
	{ LEAP_ADDSECOND,	"leap_add_sec" },
	{ LEAP_DELSECOND,	"leap_del_sec" },
	{ LEAP_NOTINSYNC,	"leap_alarm" },
	{ -1,			"leap" }
};

/*
 * Clock source status (sync)
 */
static
struct codestring sync_codes[] = {
	{ CTL_SST_TS_UNSPEC,	"sync_unspec" },
	{ CTL_SST_TS_ATOM,	"sync_pps" },
	{ CTL_SST_TS_LF,	"sync_lf_radio" },
	{ CTL_SST_TS_HF,	"sync_hf_radio" },
	{ CTL_SST_TS_UHF,	"sync_uhf_radio" },
	{ CTL_SST_TS_LOCAL,	"sync_local" },
	{ CTL_SST_TS_NTP,	"sync_ntp" },
	{ CTL_SST_TS_UDPTIME,	"sync_other" },
	{ CTL_SST_TS_WRSTWTCH,	"sync_wristwatch" },
	{ CTL_SST_TS_TELEPHONE,	"sync_telephone" },
	{ -1,			"sync" }
};

/*
 * Peer selection status (sel)
 */
static
struct codestring select_codes[] = {
	{ CTL_PST_SEL_REJECT,	"sel_reject" },
	{ CTL_PST_SEL_SANE,	"sel_falsetick" },
	{ CTL_PST_SEL_CORRECT,	"sel_excess" },
	{ CTL_PST_SEL_SELCAND,	"sel_outlyer" },
	{ CTL_PST_SEL_SYNCCAND,	"sel_candidate" },
	{ CTL_PST_SEL_EXCESS,	"sel_backup" },
	{ CTL_PST_SEL_SYSPEER,	"sel_sys.peer" },
	{ CTL_PST_SEL_PPS,	"sel_pps.peer" },
	{ -1,			"sel" }
};

/*
 * Clock status (clk)
 */
static
struct codestring clock_codes[] = {
	{ CTL_CLK_OKAY,		"clk_unspec" },
	{ CTL_CLK_NOREPLY,	"clk_no_reply" },
	{ CTL_CLK_BADFORMAT,	"clk_bad_format" },
	{ CTL_CLK_FAULT,	"clk_fault" },
	{ CTL_CLK_PROPAGATION,	"clk_bad_signal" },
	{ CTL_CLK_BADDATE,	"clk_bad_date" },
	{ CTL_CLK_BADTIME,	"clk_bad_time" },
	{ -1,			"clk" }
};


#ifdef FLASH_CODES_UNUSED
/*
 * Flash bits -- see ntpq.c tstflags & tstflagnames
 */
static
struct codestring flash_codes[] = {
	{ TEST1,		"pkt_dup" },
	{ TEST2,		"pkt_bogus" },
	{ TEST3,		"pkt_unsync" },
	{ TEST4,		"pkt_denied" },
	{ TEST5,		"pkt_auth" },
	{ TEST6,		"pkt_stratum" },
	{ TEST7,		"pkt_header" },
	{ TEST8,		"pkt_autokey" },
	{ TEST9,		"pkt_crypto" },
	{ TEST10,		"peer_stratum" },
	{ TEST11,		"peer_dist" },
	{ TEST12,		"peer_loop" },
	{ TEST13,		"peer_unreach" },
	{ -1,			"flash" }
};
#endif


/*
 * System events (sys)
 */
static
struct codestring sys_codes[] = {
	{ EVNT_UNSPEC,		"unspecified" },
	{ EVNT_NSET,		"freq_not_set" },
	{ EVNT_FSET,		"freq_set" },
	{ EVNT_SPIK,		"spike_detect" },
	{ EVNT_FREQ,		"freq_mode" },
	{ EVNT_SYNC,		"clock_sync" },
	{ EVNT_SYSRESTART,	"restart" },
	{ EVNT_SYSFAULT,	"panic_stop" },
	{ EVNT_NOPEER,		"no_sys_peer" },
	{ EVNT_ARMED,		"leap_armed" },
	{ EVNT_DISARMED,	"leap_disarmed" },
	{ EVNT_LEAP,		"leap_event" },
	{ EVNT_CLOCKRESET,	"clock_step" },
	{ EVNT_KERN,		"kern" },
	{ EVNT_TAI,		"TAI" },
	{ EVNT_LEAPVAL,		"stale_leapsecond_values" },
	{ EVNT_CLKHOP,		"clockhop" },
	{ -1,			"" }
};

/*
 * Peer events (peer)
 */
static
struct codestring peer_codes[] = {
	{ PEVNT_MOBIL & ~PEER_EVENT,	"mobilize" },
	{ PEVNT_DEMOBIL & ~PEER_EVENT,	"demobilize" },
	{ PEVNT_UNREACH & ~PEER_EVENT,	"unreachable" },
	{ PEVNT_REACH & ~PEER_EVENT,	"reachable" },
	{ PEVNT_RESTART & ~PEER_EVENT,	"restart" },
	{ PEVNT_REPLY & ~PEER_EVENT,	"no_reply" },
	{ PEVNT_RATE & ~PEER_EVENT,	"rate_exceeded" },
	{ PEVNT_DENY & ~PEER_EVENT,	"access_denied" },
	{ PEVNT_ARMED & ~PEER_EVENT,	"leap_armed" },
	{ PEVNT_NEWPEER & ~PEER_EVENT,	"sys_peer" },
	{ PEVNT_CLOCK & ~PEER_EVENT,	"clock_event" },
	{ PEVNT_AUTH & ~PEER_EVENT,	"bad_auth" },
	{ PEVNT_POPCORN & ~PEER_EVENT,	"popcorn" },
	{ PEVNT_XLEAVE & ~PEER_EVENT,	"interleave_mode" },
	{ PEVNT_XERR & ~PEER_EVENT,	"interleave_error" },
	{ PEVNT_TAI & ~PEER_EVENT,	"TAI" },
	{ -1,				"" }
};

#ifdef OPENSSL
/*
 * Crypto events (cryp)
 */
static
struct codestring crypto_codes[] = {
	{ XEVNT_OK & ~CRPT_EVENT,	"success" },
	{ XEVNT_LEN & ~CRPT_EVENT,	"bad_field_format_or_length" },
	{ XEVNT_TSP & ~CRPT_EVENT,	"bad_timestamp" },
	{ XEVNT_FSP & ~CRPT_EVENT,	"bad_filestamp" },
	{ XEVNT_PUB & ~CRPT_EVENT,	"bad_or_missing_public_key" },
	{ XEVNT_MD & ~CRPT_EVENT,	"unsupported_digest_type" },
	{ XEVNT_KEY & ~CRPT_EVENT,	"unsupported_identity_type" },
	{ XEVNT_SGL & ~CRPT_EVENT,	"bad_signature_length" },
	{ XEVNT_SIG & ~CRPT_EVENT,	"signature_not_verified" },
	{ XEVNT_VFY & ~CRPT_EVENT,	"certificate_not_verified" },
	{ XEVNT_PER & ~CRPT_EVENT,	"host_certificate_expired" },
	{ XEVNT_CKY & ~CRPT_EVENT,	"bad_or_missing_cookie" },
	{ XEVNT_DAT & ~CRPT_EVENT,	"bad_or_missing_leapseconds" },
	{ XEVNT_CRT & ~CRPT_EVENT,	"bad_or_missing_certificate" },	
	{ XEVNT_ID & ~CRPT_EVENT,	"bad_or_missing_group key" },
	{ XEVNT_ERR & ~CRPT_EVENT,	"protocol_error" },
	{ -1,				"" }
};
#endif /* OPENSSL */

/* Forwards */
static const char *getcode (int, struct codestring *);
static const char *getevents (int);

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
	snprintf(buf, sizeof(buf), "%s_%d", codetab->string, code);
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
	snprintf(buf, sizeof(buf), "%d event%s", cnt, (cnt==1) ? "" : 
	    "s");
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
		strcpy(cb, getcode(CTL_SYS_LI(st), leap_codes));
		strcat(cb, ", ");
		strcat(cb, getcode(CTL_SYS_SOURCE(st), sync_codes));
		strcat(cb, ", ");
		strcat(cb, getevents(CTL_SYS_NEVNT(st)));
		strcat(cb, ", ");
		strcat(cb, getcode(CTL_SYS_EVENT(st), sys_codes));
		break;
	
	    case TYPE_PEER:

		/*
		 * Handcraft the bits
		 */
		pst = (u_char) CTL_PEER_STATVAL(st);
		if (pst & CTL_PST_CONFIG)
			strcpy(cb, "conf");
		if (pst & CTL_PST_AUTHENABLE) {
			if (pst & CTL_PST_CONFIG)
				strcat(cb, ", authenb");
			else
				strcat(cb, "authenb");
		}
		if (pst & CTL_PST_AUTHENTIC) {
			if (pst & (CTL_PST_CONFIG | CTL_PST_AUTHENABLE))
				strcat(cb, ", auth");
			else
				strcat(cb, "auth");
		}
		if (pst & CTL_PST_REACH) {
			if (pst & (CTL_PST_CONFIG | CTL_PST_AUTHENABLE |
			    CTL_PST_AUTHENTIC))
				strcat(cb, ", reach");
			else
				strcat(cb, "reach");
		}
		if (pst & CTL_PST_BCAST) {
			if (pst & (CTL_PST_CONFIG | CTL_PST_AUTHENABLE |
			    CTL_PST_AUTHENTIC | CTL_PST_REACH))
				strcat(cb, ", bcst");
			else
				strcat(cb, "bcst");
		}

		/*
		 * Now the codes
		 */
		strcat(cb, ", ");
		strcat(cb, getcode(pst & 0x7, select_codes));
		strcat(cb, ", ");
		strcat(cb, getevents(CTL_PEER_NEVNT(st)));
		if (CTL_PEER_EVENT(st) != EVNT_UNSPEC) {
			strcat(cb, ", ");
			strcat(cb, getcode(CTL_PEER_EVENT(st),
			    peer_codes));
		}
		break;
	
	    case TYPE_CLOCK:
		strcat(cb, ", ");
		strcat(cb, getevents(CTL_SYS_NEVNT(st)));
		strcat(cb, ", ");
		strcat(cb, getcode((st) & 0xf, clock_codes));
		break;
	}
	return cb;
}

const char *
eventstr(
	int num
	)
{
	if (num & PEER_EVENT)
		return (getcode(num & ~PEER_EVENT, peer_codes));
#ifdef OPENSSL
	else if (num & CRPT_EVENT)
		return (getcode(num & ~CRPT_EVENT, crypto_codes));
#endif /* OPENSSL */
	else
		return (getcode(num, sys_codes));
}

const char *
ceventstr(
	int num
	)
{
	return getcode(num, clock_codes);
}
