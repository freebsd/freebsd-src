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
#ifdef KERNEL_PLL
# include "ntp_syscall.h"
#endif


/*
 * Structure for turning various constants into a readable string.
 */
struct codestring {
	int code;
	const char * const string;
};

/*
 * Leap status (leap)
 */
static const struct codestring leap_codes[] = {
	{ LEAP_NOWARNING,	"leap_none" },
	{ LEAP_ADDSECOND,	"leap_add_sec" },
	{ LEAP_DELSECOND,	"leap_del_sec" },
	{ LEAP_NOTINSYNC,	"leap_alarm" },
	{ -1,			"leap" }
};

/*
 * Clock source status (sync)
 */
static const struct codestring sync_codes[] = {
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
static const struct codestring select_codes[] = {
	{ CTL_PST_SEL_REJECT,	"sel_reject" },
	{ CTL_PST_SEL_SANE,	"sel_falsetick" },
	{ CTL_PST_SEL_CORRECT,	"sel_excess" },
	{ CTL_PST_SEL_SELCAND,	"sel_outlier" },
	{ CTL_PST_SEL_SYNCCAND,	"sel_candidate" },
	{ CTL_PST_SEL_EXCESS,	"sel_backup" },
	{ CTL_PST_SEL_SYSPEER,	"sel_sys.peer" },
	{ CTL_PST_SEL_PPS,	"sel_pps.peer" },
	{ -1,			"sel" }
};

/*
 * Clock status (clk)
 */
static const struct codestring clock_codes[] = {
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
static const struct codestring flash_codes[] = {
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
static const struct codestring sys_codes[] = {
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
	{ -1,			"" }
};

/*
 * Peer events (peer)
 */
static const struct codestring peer_codes[] = {
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
	{ -1,				"" }
};

/*
 * Peer status bits
 */
static const struct codestring peer_st_bits[] = {
	{ CTL_PST_CONFIG,		"conf" },
	{ CTL_PST_AUTHENABLE,		"authenb" },
	{ CTL_PST_AUTHENTIC,		"auth" },
	{ CTL_PST_REACH,		"reach" },
	{ CTL_PST_BCAST,		"bcast" },
	/* not used with getcode(), no terminating entry needed */
};

/*
 * Restriction match bits
 */
static const struct codestring res_match_bits[] = {
	{ RESM_NTPONLY,			"ntpport" },
	{ RESM_INTERFACE,		"interface" },
	{ RESM_SOURCE,			"source" },
	/* not used with getcode(), no terminating entry needed */
};

/*
 * Restriction access bits
 */
static const struct codestring res_access_bits[] = {
	{ RES_IGNORE,			"ignore" },
	{ RES_DONTSERVE,		"noserve" },
	{ RES_DONTTRUST,		"notrust" },
	{ RES_NOQUERY,			"noquery" },
	{ RES_NOMODIFY,			"nomodify" },
	{ RES_NOPEER,			"nopeer" },
	{ RES_NOTRAP,			"notrap" },
	{ RES_LPTRAP,			"lptrap" },
	{ RES_LIMITED,			"limited" },
	{ RES_VERSION,			"version" },
	{ RES_KOD,			"kod" },
	{ RES_FLAKE,			"flake" },
	/* not used with getcode(), no terminating entry needed */
};

#ifdef AUTOKEY
/*
 * Crypto events (cryp)
 */
static const struct codestring crypto_codes[] = {
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
#endif	/* AUTOKEY */

#ifdef KERNEL_PLL
/*
 * kernel discipline status bits
 */
static const struct codestring k_st_bits[] = {
# ifdef STA_PLL
	{ STA_PLL,			"pll" },
# endif
# ifdef STA_PPSFREQ
	{ STA_PPSFREQ,			"ppsfreq" },
# endif
# ifdef STA_PPSTIME
	{ STA_PPSTIME,			"ppstime" },
# endif
# ifdef STA_FLL
	{ STA_FLL,			"fll" },
# endif
# ifdef STA_INS
	{ STA_INS,			"ins" },
# endif
# ifdef STA_DEL
	{ STA_DEL,			"del" },
# endif
# ifdef STA_UNSYNC
	{ STA_UNSYNC,			"unsync" },
# endif
# ifdef STA_FREQHOLD
	{ STA_FREQHOLD,			"freqhold" },
# endif
# ifdef STA_PPSSIGNAL
	{ STA_PPSSIGNAL,		"ppssignal" },
# endif
# ifdef STA_PPSJITTER
	{ STA_PPSJITTER,		"ppsjitter" },
# endif
# ifdef STA_PPSWANDER
	{ STA_PPSWANDER,		"ppswander" },
# endif
# ifdef STA_PPSERROR
	{ STA_PPSERROR,			"ppserror" },
# endif
# ifdef STA_CLOCKERR
	{ STA_CLOCKERR,			"clockerr" },
# endif
# ifdef STA_NANO
	{ STA_NANO,			"nano" },
# endif
# ifdef STA_MODE
	{ STA_MODE,			"mode=fll" },
# endif
# ifdef STA_CLK
	{ STA_CLK,			"src=B" },
# endif
	/* not used with getcode(), no terminating entry needed */
};
#endif	/* KERNEL_PLL */

/* Forwards */
static const char *	getcode(int, const struct codestring *);
static const char *	getevents(int);
static const char *	peer_st_flags(u_char pst);

/*
 * getcode - return string corresponding to code
 */
static const char *
getcode(
	int				code,
	const struct codestring *	codetab
	)
{
	char *	buf;

	while (codetab->code != -1) {
		if (codetab->code == code)
			return codetab->string;
		codetab++;
	}

	LIB_GETBUF(buf);
	snprintf(buf, LIB_BUFLENGTH, "%s_%d", codetab->string, code);

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
	char *	buf;

	if (cnt == 0)
		return "no events";

	LIB_GETBUF(buf);
	snprintf(buf, LIB_BUFLENGTH, "%d event%s", cnt,
		 (1 == cnt)
		     ? ""
		     : "s");

	return buf;
}


/*
 * decode_bitflags()
 *
 * returns a human-readable string with a keyword from tab for each bit
 * set in bits, separating multiple entries with text of sep2.
 */
static const char *
decode_bitflags(
	int				bits,
	const char *			sep2,
	const struct codestring *	tab,
	size_t				tab_ct
	)
{
	const char *	sep;
	char *		buf;
	char *		pch;
	char *		lim;
	size_t		b;
	int		rc;
	int		saved_errno;	/* for use in DPRINTF with %m */

	saved_errno = errno;
	LIB_GETBUF(buf);
	pch = buf;
	lim = buf + LIB_BUFLENGTH;
	sep = "";

	for (b = 0; b < tab_ct; b++) {
		if (tab[b].code & bits) {
			rc = snprintf(pch, (lim - pch), "%s%s", sep,
				      tab[b].string);
			if (rc < 0)
				goto toosmall;
			pch += (u_int)rc;
			if (pch >= lim)
				goto toosmall;
			sep = sep2;
		}
	}

	return buf;

    toosmall:
	snprintf(buf, LIB_BUFLENGTH,
		 "decode_bitflags(%s) can't decode 0x%x in %d bytes",
		 (tab == peer_st_bits)
		     ? "peer_st"
		     : 
#ifdef KERNEL_PLL
		       (tab == k_st_bits)
			   ? "kern_st"
			   :
#endif
			     "",
		 bits, (int)LIB_BUFLENGTH);
	errno = saved_errno;

	return buf;
}


static const char *
peer_st_flags(
	u_char pst
	)
{
	return decode_bitflags(pst, ", ", peer_st_bits,
			       COUNTOF(peer_st_bits));
}


const char *
res_match_flags(
	u_short mf
	)
{
	return decode_bitflags(mf, " ", res_match_bits,
			       COUNTOF(res_match_bits));
}


const char *
res_access_flags(
	u_short af
	)
{
	return decode_bitflags(af, " ", res_access_bits,
			       COUNTOF(res_access_bits));
}


#ifdef KERNEL_PLL
const char *
k_st_flags(
	u_int32 st
	)
{
	return decode_bitflags(st, " ", k_st_bits, COUNTOF(k_st_bits));
}
#endif	/* KERNEL_PLL */


/*
 * statustoa - return a descriptive string for a peer status
 */
char *
statustoa(
	int type,
	int st
	)
{
	char *	cb;
	char *	cc;
	u_char	pst;

	LIB_GETBUF(cb);

	switch (type) {

	case TYPE_SYS:
		snprintf(cb, LIB_BUFLENGTH, "%s, %s, %s, %s",
			 getcode(CTL_SYS_LI(st), leap_codes),
			 getcode(CTL_SYS_SOURCE(st), sync_codes),
			 getevents(CTL_SYS_NEVNT(st)),
			 getcode(CTL_SYS_EVENT(st), sys_codes));
		break;
	
	case TYPE_PEER:
		pst = (u_char)CTL_PEER_STATVAL(st);
		snprintf(cb, LIB_BUFLENGTH, "%s, %s, %s",
			 peer_st_flags(pst),
			 getcode(pst & 0x7, select_codes),
			 getevents(CTL_PEER_NEVNT(st)));
		if (CTL_PEER_EVENT(st) != EVNT_UNSPEC) {
			cc = cb + strlen(cb);
			snprintf(cc, LIB_BUFLENGTH - (cc - cb), ", %s",
				 getcode(CTL_PEER_EVENT(st),
					 peer_codes));
		}
		break;
	
	case TYPE_CLOCK:
		snprintf(cb, LIB_BUFLENGTH, "%s, %s",
			 getevents(CTL_SYS_NEVNT(st)),
			 getcode((st) & 0xf, clock_codes));
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
#ifdef AUTOKEY
	else if (num & CRPT_EVENT)
		return (getcode(num & ~CRPT_EVENT, crypto_codes));
#endif	/* AUTOKEY */
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
