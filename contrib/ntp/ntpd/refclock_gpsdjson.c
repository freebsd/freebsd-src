/*
 * refclock_gpsdjson.c - clock driver as GPSD JSON client
 *	Juergen Perlinger (perlinger@ntp.org)
 *	Feb 11, 2014 for the NTP project.
 *      The contents of 'html/copyright.html' apply.
 *
 *	Heavily inspired by refclock_nmea.c
 *
 * Note: This will currently NOT work with Windows due to some
 * limitations:
 *
 *  - There is no GPSD for Windows. (There is an unofficial port to
 *    cygwin, but Windows is not officially supported.)
 *
 *  - To work properly, this driver needs PPS and TPV sentences from
 *    GPSD. I don't see how the cygwin port should deal with that.
 *
 *  - The device name matching must be done in a different way for
 *    Windows. (Can be done with COMxx matching, as done for NMEA.)
 *
 * Apart from those minor hickups, once GPSD has been fully ported to
 * Windows, there's no reason why this should not work there ;-)
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "ntp_types.h"

#if defined(REFCLOCK) && defined(CLOCK_GPSDJSON) && !defined(SYS_WINNT) 

/* =====================================================================
 * get the little JSMN library directly into our guts
 */
#include "../libjsmn/jsmn.c"

/* =====================================================================
 * header stuff we need
 */

#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/tcp.h>

#if defined(HAVE_SYS_POLL_H)
# include <sys/poll.h>
#elif defined(HAVE_SYS_SELECT_H)
# include <sys/select.h>
#else
# error need poll() or select()
#endif

#include "ntpd.h"
#include "ntp_io.h"
#include "ntp_unixtime.h"
#include "ntp_refclock.h"
#include "ntp_stdlib.h"
#include "ntp_calendar.h"
#include "timespecops.h"

#define	PRECISION	(-9)	/* precision assumed (about 2 ms) */
#define	PPS_PRECISION	(-20)	/* precision assumed (about 1 us) */
#define	REFID		"GPSD"	/* reference id */
#define	DESCRIPTION	"GPSD JSON client clock" /* who we are */

#define MAX_PDU_LEN	1600
#define TICKOVER_LOW	10
#define TICKOVER_HIGH	120
#define LOGTHROTTLE	3600

#define PPS_MAXCOUNT	30
#define PPS_HIWAT       20
#define PPS_LOWAT       10

#ifndef BOOL
# define BOOL int
#endif
#ifndef TRUE
# define TRUE 1
#endif
#ifndef FALSE
# define FALSE 0
#endif

/* some local typedefs : The NTPD formatting style cries for short type
 * names, and we provide them locally. Note:the suffix '_t' is reserved
 * for the standard; I use a capital T instead.
 */
typedef struct peer         peerT;
typedef struct refclockproc clockprocT;
typedef struct addrinfo     addrinfoT;

/* =====================================================================
 * We use the same device name scheme as does the NMEA driver; since
 * GPSD supports the same links, we can select devices by a fixed name.
 */
static const char * s_dev_stem = "/dev/gps";

/* =====================================================================
 * forward declarations for transfer vector and the vector itself
 */

static	void	gpsd_init	(void);
static	int	gpsd_start	(int, peerT *);
static	void	gpsd_shutdown	(int, peerT *);
static	void	gpsd_receive	(struct recvbuf *);
static	void	gpsd_poll	(int, peerT *);
static	void	gpsd_control	(int, const struct refclockstat *,
				 struct refclockstat *, peerT *);
static	void	gpsd_timer	(int, peerT *);
static  void    gpsd_clockstats (int, peerT *);

static  int     myasprintf(char**, char const*, ...);

struct refclock refclock_gpsdjson = {
	gpsd_start,		/* start up driver */
	gpsd_shutdown,		/* shut down driver */
	gpsd_poll,		/* transmit poll message */
	gpsd_control,		/* fudge control */
	gpsd_init,		/* initialize driver */
	noentry,		/* buginfo */
	gpsd_timer		/* called once per second */
};

/* =====================================================================
 * our local clock unit and data
 */
typedef struct gpsd_unit {
	int	 unit;
	/* current line protocol version */
	uint16_t proto_major;
	uint16_t proto_minor;

	/* PPS time stamps */
	l_fp pps_local;	/* when we received the PPS message */
	l_fp pps_stamp;	/* related reference time */
	l_fp pps_recvt;	/* when GPSD detected the pulse */

	/* TPV (GPS data) time stamps */
	l_fp tpv_local;	/* when we received the TPV message */
	l_fp tpv_stamp;	/* effective GPS time stamp */
	l_fp tpv_recvt;	/* when GPSD got the fix */

	/* fudge values for correction, mirrored as 'l_fp' */
	l_fp pps_fudge;
	l_fp tpv_fudge;

	/* Flags to indicate available data */
	int fl_tpv   : 1;	/* valid TPV seen (have time) */
	int fl_pps   : 1;	/* valid pulse seen */
	int fl_vers  : 1;	/* have protocol version */
	int fl_watch : 1;	/* watch reply seen */
	int fl_nsec  : 1;	/* have nanosec PPS info */

	/* admin stuff for sockets and device selection */
	int         fdt;	/* current connecting socket */
	addrinfoT * addr;	/* next address to try */
	u_int       tickover;	/* timeout countdown */
	u_int       tickpres;	/* timeout preset */
	u_int       ppscount;	/* PPS mode up/down count */
	char      * device;	/* device name of unit */

	/* tallies for the various events */
	u_int       tc_good;	/* good samples received */
	u_int	    tc_btime;	/* bad time stamps */
	u_int       tc_bdate;	/* bad date strings */
	u_int       tc_breply;	/* bad replies */
	u_int       tc_recv;	/* received known records */

	/* log bloat throttle */
	u_int       logthrottle;/* seconds to next log slot */

	/* record assemby buffer and saved length */	
	int  buflen;
	char buffer[MAX_PDU_LEN];
} gpsd_unitT;

/* =====================================================================
 * static local helpers forward decls
 */
static void gpsd_init_socket(peerT * const peer);
static void gpsd_test_socket(peerT * const peer);
static void gpsd_stop_socket(peerT * const peer);

static void gpsd_parse(peerT * const peer,
		       const l_fp  * const rtime);
static BOOL convert_ascii_time(l_fp * fp, const char * gps_time);
static void save_ltc(clockprocT * const pp, const char * const tc);
static int  syslogok(clockprocT * const pp, gpsd_unitT * const up);

/* =====================================================================
 * local / static stuff
 */

/* The logon string is actually the ?WATCH command of GPSD, using JSON
 * data and selecting the GPS device name we created from our unit
 * number. [Note: This is a format string!]
 */
#define s_logon \
    "?WATCH={\"enable\":true,\"json\":true,\"device\":\"%s\"};\r\n"

/* We keep a static list of network addresses for 'localhost:gpsd', and
 * we try to connect to them in round-robin fashion.
 */
static addrinfoT * s_gpsd_addr;

/* =====================================================================
 * log throttling
 */
static int/*BOOL*/
syslogok(
	clockprocT * const pp,
	gpsd_unitT * const up)
{
	int res = (0 != (pp->sloppyclockflag & CLK_FLAG3))
	       || (0           == up->logthrottle )
	       || (LOGTHROTTLE == up->logthrottle );
	if (res)
		up->logthrottle = LOGTHROTTLE;
	return res;
}

/* =====================================================================
 * the clock functions
 */

/* ---------------------------------------------------------------------
 * Init: This currently just gets the socket address for the GPS daemon
 */
static void
gpsd_init(void)
{
	addrinfoT hints;
	
	memset(&hints, 0, sizeof(hints));
	hints.ai_family   = AF_UNSPEC;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_socktype = SOCK_STREAM;

	/* just take the first configured address of localhost... */
	if (getaddrinfo("localhost", "gpsd", &hints, &s_gpsd_addr))
		s_gpsd_addr = NULL;
}

/* ---------------------------------------------------------------------
 * Start: allocate a unit pointer and set up the runtime data
 */

static int
gpsd_start(
	int     unit,
	peerT * peer)
{
	clockprocT * const pp = peer->procptr;
	gpsd_unitT * const up = emalloc_zero(sizeof(*up));

	struct stat sb;

	/* initialize the unit structure */
	up->fdt      = -1;
	up->addr     = s_gpsd_addr;
	up->tickpres = TICKOVER_LOW;

	/* setup refclock processing */
	up->unit    = unit;
	pp->unitptr = (caddr_t)up;
	pp->io.fd   = -1;
	pp->io.clock_recv = gpsd_receive;
	pp->io.srcclock   = peer;
	pp->io.datalen    = 0;
	pp->a_lastcode[0] = '\0';
	pp->lencode       = 0;
	pp->clockdesc     = DESCRIPTION;
	memcpy(&pp->refid, REFID, 4);

	/* Initialize miscellaneous variables */
	peer->precision = PRECISION;

	/* Create the device name and check for a Character Device. It's
	 * assumed that GPSD was started with the same link, so the
	 * names match. (If this is not practicable, we will have to
	 * read the symlink, if any, so we can get the true device
	 * file.)
	 */
	if (-1 == myasprintf(&up->device, "%s%u", s_dev_stem, unit)) {
	    msyslog(LOG_ERR, "%s clock device name too long",
		    refnumtoa(&peer->srcadr));
	    goto dev_fail;
	}
	if (-1 == stat(up->device, &sb) || !S_ISCHR(sb.st_mode)) {
		msyslog(LOG_ERR, "%s: '%s' is not a character device",
			refnumtoa(&peer->srcadr), up->device);
	    goto dev_fail;
	}
	LOGIF(CLOCKINFO,
	      (LOG_NOTICE, "%s: startup, device is '%s'",
	       refnumtoa(&peer->srcadr), up->device));
	return TRUE;

dev_fail:
	/* On failure, remove all UNIT ressources and declare defeat. */

	INSIST (up);
	free(up->device);
	free(up);

	pp->unitptr = (caddr_t)NULL;
	return FALSE;
}

/* ------------------------------------------------------------------ */

static void
gpsd_shutdown(
	int     unit,
	peerT * peer)
{
	clockprocT * const pp = peer->procptr;
	gpsd_unitT * const up = (gpsd_unitT *)pp->unitptr;

	UNUSED_ARG(unit);

	if (up) {
	    free(up->device);
	    free(up);
	}
	pp->unitptr = (caddr_t)NULL;
	if (-1 != pp->io.fd)
		io_closeclock(&pp->io);
	pp->io.fd = -1;
	LOGIF(CLOCKINFO,
	      (LOG_NOTICE, "%s: shutdown", refnumtoa(&peer->srcadr)));
}

/* ------------------------------------------------------------------ */

static void
gpsd_receive(
	struct recvbuf * rbufp)
{
	/* declare & init control structure ptrs */
	peerT	   * const peer = rbufp->recv_peer;
	clockprocT * const pp   = peer->procptr;
	gpsd_unitT * const up   = (gpsd_unitT *)pp->unitptr;	

	const char *psrc, *esrc;
	char       *pdst, *edst, ch;

	/* Since we're getting a raw stream data, we must assemble lines
	 * in our receive buffer. We can't use neither 'refclock_gtraw'
	 * not 'refclock_gtlin' here...  We process chars until we reach
	 * an EoL (that is, line feed) but we truncate the message if it
	 * does not fit the buffer.  GPSD might truncate messages, too,
	 * so dealing with truncated buffers is necessary anyway.
	 */
	psrc = (const char*)rbufp->recv_buffer;
	esrc = psrc + rbufp->recv_length;

	pdst = up->buffer + up->buflen;
	edst = pdst + sizeof(up->buffer) - 1; /* for trailing NUL */

	while (psrc != esrc) {
		ch = *psrc++;
		if (ch == '\n') {
			/* trim trailing whitespace & terminate buffer */
			while (pdst != up->buffer && pdst[-1] <= ' ')
				--pdst;
			*pdst = '\0';
			/* process data and reset buffer */
			gpsd_parse(peer, &rbufp->recv_time);
			pdst = up->buffer;
		} else if (pdst != edst) {
			/* add next char, ignoring leading whitespace */
			if (ch > ' ' || pdst != up->buffer)
				*pdst++ = ch;
		}
	}
	up->buflen   = pdst - up->buffer;
	up->tickover = TICKOVER_LOW;
}

/* ------------------------------------------------------------------ */

static void
gpsd_poll(
	int     unit,
	peerT * peer)
{
	clockprocT * const pp = peer->procptr;
	gpsd_unitT * const up = (gpsd_unitT *)pp->unitptr;
	u_int   tc_max;

	++pp->polls;

	/* find the dominant error */
	tc_max = max(up->tc_btime, up->tc_bdate);
	tc_max = max(tc_max, up->tc_breply);

	if (pp->coderecv != pp->codeproc) {
		/* all is well */
		pp->lastref = pp->lastrec;
		refclock_receive(peer);
	} else {
		/* not working properly, admit to it */
		peer->flags    &= ~FLAG_PPS;
		peer->precision = PRECISION;

		if (-1 == pp->io.fd) {
			/* not connected to GPSD: clearly not working! */
			refclock_report(peer, CEVNT_FAULT);
		} else if (tc_max == up->tc_breply) {
			refclock_report(peer, CEVNT_BADREPLY);
		} else if (tc_max == up->tc_btime) {
			refclock_report(peer, CEVNT_BADTIME);
		} else if (tc_max == up->tc_bdate) {
			refclock_report(peer, CEVNT_BADDATE);
		} else {
			refclock_report(peer, CEVNT_TIMEOUT);
		}
	}

	if (pp->sloppyclockflag & CLK_FLAG4)
		gpsd_clockstats(unit, peer);

	/* clear tallies for next round */
	up->tc_good = up->tc_btime = up->tc_bdate =
	    up->tc_breply = up->tc_recv = 0;
}

/* ------------------------------------------------------------------ */

static void
gpsd_control(
	int                         unit,
	const struct refclockstat * in_st,
	struct refclockstat       * out_st,
	peerT                     * peer  )
{
	clockprocT * const pp = peer->procptr;
	gpsd_unitT * const up = (gpsd_unitT *)pp->unitptr;

	/* save preprocessed fudge times */
	DTOLFP(pp->fudgetime1, &up->pps_fudge);
	DTOLFP(pp->fudgetime2, &up->tpv_fudge);
}

/* ------------------------------------------------------------------ */

static void
gpsd_timer(
	int     unit,
	peerT * peer)
{
	static const char query[] = "?VERSION;";

	clockprocT * const pp = peer->procptr;
	gpsd_unitT * const up = (gpsd_unitT *)pp->unitptr;
	int                rc;

	/* This is used for timeout handling. Nothing that needs
	 * sub-second precison happens here, so receive/connect/retry
	 * timeouts are simply handled by a count down, and then we
	 * decide what to do by the socket values.
	 *
	 * Note that the timer stays at zero here, unless some of the
	 * functions set it to another value.
	 */
	if (up->logthrottle)
		--up->logthrottle;
	if (up->tickover)
		--up->tickover;
	switch (up->tickover) {
	case 4:
		/* try to get a live signal
		 * If the device is not yet present, we will most likely
		 * get an error. We put out a new version request,
		 * because the reply will initiate a new watch request
		 * cycle.
		 */
		if (-1 != pp->io.fd) {
			if ( ! up->fl_watch) {
				DPRINTF(2, ("GPSD_JSON(%d): timer livecheck: '%s'\n",
					    up->unit, query));
				rc = write(pp->io.fd,
					   query, sizeof(query));
				(void)rc;
			}
		} else if (-1 != up->fdt) {
			gpsd_test_socket(peer);
		}
		break;

	case 0:
		if (-1 != pp->io.fd)
			gpsd_stop_socket(peer);
		else if (-1 != up->fdt)
			gpsd_test_socket(peer);
		else if (NULL != s_gpsd_addr)
			gpsd_init_socket(peer);
		break;

	default:
		if (-1 == pp->io.fd && -1 != up->fdt)
			gpsd_test_socket(peer);
	}

	if (up->ppscount > PPS_HIWAT && !(peer->flags & FLAG_PPS))
		peer->flags |= FLAG_PPS;
	if (up->ppscount < PPS_LOWAT &&  (peer->flags & FLAG_PPS))
		peer->flags &= ~FLAG_PPS;
}

/* =====================================================================
 * JSON parsing stuff
 */

#define JSMN_MAXTOK	100
#define INVALID_TOKEN (-1)

typedef struct json_ctx {
	char        * buf;
	int           ntok;
	jsmntok_t     tok[JSMN_MAXTOK];
} json_ctx;

typedef int tok_ref;

#ifdef HAVE_LONG_LONG
typedef long long json_int;
 #define JSON_STRING_TO_INT strtoll
#else
typedef long json_int;
 #define JSON_STRING_TO_INT strtol
#endif

/* ------------------------------------------------------------------ */

static tok_ref
json_token_skip(
	const json_ctx * ctx,
	tok_ref          tid)
{
	int len;
	len = ctx->tok[tid].size;
	for (++tid; len; --len)
		if (tid < ctx->ntok)
			tid = json_token_skip(ctx, tid);
		else
			break;
	if (tid > ctx->ntok)
		tid = ctx->ntok;
	return tid;
}
	
/* ------------------------------------------------------------------ */

static int
json_object_lookup(
	const json_ctx * ctx,
	tok_ref          tid,
	const char     * key)
{
	int len;

	if (tid >= ctx->ntok || ctx->tok[tid].type != JSMN_OBJECT)
		return INVALID_TOKEN;
	len = ctx->ntok - tid - 1;
	if (len > ctx->tok[tid].size)
		len = ctx->tok[tid].size;
	for (tid += 1; len > 1; len-=2) {
		if (ctx->tok[tid].type != JSMN_STRING)
			continue; /* hmmm... that's an error, strictly speaking */
		if (!strcmp(key, ctx->buf + ctx->tok[tid].start))
			return tid + 1;
		tid = json_token_skip(ctx, tid + 1);
	}
	return INVALID_TOKEN;
}

/* ------------------------------------------------------------------ */

#if 0 /* currently unused */
static const char*
json_object_lookup_string(
	const json_ctx * ctx,
	tok_ref          tid,
	const char     * key)
{
	tok_ref val_ref;
	val_ref = json_object_lookup(ctx, tid, key);
	if (INVALID_TOKEN == val_ref               ||
	    JSMN_STRING   != ctx->tok[val_ref].type )
		goto cvt_error;
	return ctx->buf + ctx->tok[val_ref].start;

  cvt_error:
	errno = EINVAL;
	return NULL;
}
#endif

static const char*
json_object_lookup_string_default(
	const json_ctx * ctx,
	tok_ref          tid,
	const char     * key,
	const char     * def)
{
	tok_ref val_ref;
	val_ref = json_object_lookup(ctx, tid, key);
	if (INVALID_TOKEN == val_ref               ||
	    JSMN_STRING   != ctx->tok[val_ref].type )
		return def;
	return ctx->buf + ctx->tok[val_ref].start;
}

/* ------------------------------------------------------------------ */

static json_int
json_object_lookup_int(
	const json_ctx * ctx,
	tok_ref          tid,
	const char     * key)
{
	json_int  ret;
	tok_ref   val_ref;
	char    * ep;

	val_ref = json_object_lookup(ctx, tid, key);
	if (INVALID_TOKEN  == val_ref               ||
	    JSMN_PRIMITIVE != ctx->tok[val_ref].type )
		goto cvt_error;
	ret = JSON_STRING_TO_INT(
		ctx->buf + ctx->tok[val_ref].start, &ep, 10);
	if (*ep)
		goto cvt_error;
	return ret;

  cvt_error:
	errno = EINVAL;
	return 0;
}

static json_int
json_object_lookup_int_default(
	const json_ctx * ctx,
	tok_ref          tid,
	const char     * key,
	json_int         def)
{
	json_int  retv;
	int       esave;
	
	esave = errno;
	errno = 0;
	retv  = json_object_lookup_int(ctx, tid, key);
	if (0 != errno)
		retv = def;
	errno = esave;
	return retv;
}

/* ------------------------------------------------------------------ */

static double
json_object_lookup_float(
	const json_ctx * ctx,
	tok_ref          tid,
	const char     * key)
{
	double    ret;
	tok_ref   val_ref;
	char    * ep;

	val_ref = json_object_lookup(ctx, tid, key);
	if (INVALID_TOKEN  == val_ref               ||
	    JSMN_PRIMITIVE != ctx->tok[val_ref].type )
		goto cvt_error;
	ret = strtod(ctx->buf + ctx->tok[val_ref].start, &ep);
	if (*ep)
		goto cvt_error;
	return ret;

  cvt_error:
	errno = EINVAL;
	return 0.0;
}

static double
json_object_lookup_float_default(
	const json_ctx * ctx,
	tok_ref          tid,
	const char     * key,
	double           def)
{
	double    retv;
	int       esave;
	
	esave = errno;
	errno = 0;
	retv  = json_object_lookup_float(ctx, tid, key);
	if (0 != errno)
		retv = def;
	errno = esave;
	return retv;
}

/* ------------------------------------------------------------------ */

static BOOL
json_parse_record(
	json_ctx * ctx,
	char     * buf)
{
	jsmn_parser jsm;
	int         idx, rc;

	jsmn_init(&jsm);
	rc = jsmn_parse(&jsm, buf, ctx->tok, JSMN_MAXTOK);
	ctx->buf  = buf;
	ctx->ntok = jsm.toknext;

	/* Make all tokens NUL terminated by overwriting the
	 * terminator symbol
	 */
	for (idx = 0; idx < jsm.toknext; ++idx)
		if (ctx->tok[idx].end > ctx->tok[idx].start)
			ctx->buf[ctx->tok[idx].end] = '\0';

	if (JSMN_ERROR_PART  != rc &&
	    JSMN_ERROR_NOMEM != rc &&
	    JSMN_SUCCESS     != rc  )
		return FALSE; /* not parseable - bail out */

	if (0 >= jsm.toknext || JSMN_OBJECT != ctx->tok[0].type)
		return FALSE; /* not object or no data!?! */

	return TRUE;
}


/* =====================================================================
 * static local helpers
 */

/* ------------------------------------------------------------------ */
/* Process a WATCH record
 *
 * Currently this is only used to recognise that the device is present
 * and that we're listed subscribers.
 */
static void
process_watch(
	peerT      * const peer ,
	json_ctx   * const jctx ,
	const l_fp * const rtime)
{
	clockprocT * const pp = peer->procptr;
	gpsd_unitT * const up = (gpsd_unitT *)pp->unitptr;

	up->fl_watch = -1;
}

/* ------------------------------------------------------------------ */

static void
process_version(
	peerT      * const peer ,
	json_ctx   * const jctx ,
	const l_fp * const rtime)
{
	clockprocT * const pp = peer->procptr;
	gpsd_unitT * const up = (gpsd_unitT *)pp->unitptr;

	int    len;
	char * buf;
	const char *revision;
	const char *release;

	/* get protocol version number */
	revision = json_object_lookup_string_default(
	    jctx, 0, "rev", "(unknown)");
	release  = json_object_lookup_string_default(
	    jctx, 0, "release", "(unknown)");
	errno = 0;
	up->proto_major = (uint16_t)json_object_lookup_int(
		jctx, 0, "proto_major");
	up->proto_minor = (uint16_t)json_object_lookup_int(
		jctx, 0, "proto_minor");
	if (0 == errno) {
		up->fl_vers = -1;
		if (syslogok(pp, up))
			msyslog(LOG_INFO,
				"%s: GPSD revision=%s release=%s protocol=%u.%u",
				refnumtoa(&peer->srcadr),
				revision, release,
				up->proto_major, up->proto_minor);
	}

	/* With the 3.9 GPSD protocol, '*_musec' vanished and was
	 * replace by '*_nsec'. Dispatch properly.
	 */
	if ( up->proto_major >  3 ||
	    (up->proto_major == 3 && up->proto_minor >= 9))
		up->fl_nsec = -1;
	else
		up->fl_nsec = 0;

	/*TODO: validate protocol version! */
	
	/* request watch for our GPS device
	 * Reuse the input buffer, which is no longer needed in the
	 * current cycle. Also assume that we can write the watch
	 * request in one sweep into the socket; since we do not do
	 * output otherwise, this should always work.  (Unless the
	 * TCP/IP window size gets lower than the length of the
	 * request. We handle that when it happens.)
	 */
	snprintf(up->buffer, sizeof(up->buffer),
		 s_logon, up->device);
	buf = up->buffer;
	len = strlen(buf);
	if (len != write(pp->io.fd, buf, len)) {
		/*Note: if the server fails to read our request, the
		 * resulting data timeout will take care of the
		 * connection!
		 */
		if (syslogok(pp, up))
			msyslog(LOG_ERR,
				"%s: failed to write watch request (%m)",
				refnumtoa(&peer->srcadr));
	}
}

/* ------------------------------------------------------------------ */

static void
process_tpv(
	peerT      * const peer ,
	json_ctx   * const jctx ,
	const l_fp * const rtime)
{
	clockprocT * const pp = peer->procptr;
	gpsd_unitT * const up = (gpsd_unitT *)pp->unitptr;

	const char * gps_time;
	int          gps_mode;
	double       ept, epp, epx, epy, epv;
	int          xlog2;

	gps_mode = (int)json_object_lookup_int_default(
		jctx, 0, "mode", 0);

	gps_time = json_object_lookup_string_default(
		jctx, 0, "time", NULL);

	if (gps_mode < 1 || NULL == gps_time) {
		/* receiver has no fix; tell about and avoid stale data */
		up->tc_breply += 1;
		up->fl_tpv     = 0;
		up->fl_pps     = 0;
		return;
	}

	/* save last time code to clock data */
	save_ltc(pp, gps_time);

	/* convert clock and set resulting ref time */
	if (convert_ascii_time(&up->tpv_stamp, gps_time)) {
		DPRINTF(2, ("GPSD_JSON(%d): process_tpv, stamp='%s', recvt='%s' mode=%u\n",
			    up->unit,
			    gmprettydate(&up->tpv_stamp),
			    gmprettydate(&up->tpv_recvt),
			    gps_mode));
		
		up->tpv_local = *rtime;
		up->tpv_recvt = *rtime;/*TODO: hack until we get it remote from GPSD */
		L_SUB(&up->tpv_recvt, &up->tpv_fudge);
		up->fl_tpv = -1;
	} else {
		up->tc_btime += 1;
		up->fl_tpv    = 0;
	}
		
	/* Set the precision from the GPSD data
	 *
	 * Since EPT has some issues, we use EPT and a home-brewed error
	 * estimation base on a sphere derived from EPX/Y/V and the
	 * speed of light. Use the better one of those two.
	 */
	ept = json_object_lookup_float_default(jctx, 0, "ept", 1.0);
	epx = json_object_lookup_float_default(jctx, 0, "epx", 1000.0);
	epy = json_object_lookup_float_default(jctx, 0, "epy", 1000.0);
	if (1 == gps_mode) {
		/* 2d-fix: extend bounding rectangle to cuboid */
		epv = max(epx, epy);
	} else {
		/* 3d-fix: get bounding cuboid */
		epv = json_object_lookup_float_default(
				jctx, 0, "epv", 1000.0);
	}

	/* get diameter of enclosing sphere of bounding cuboid as spatial
	 * error, then divide spatial error by speed of light to get
	 * another time error estimate. Add extra 100 meters as
	 * optimistic lower bound. Then use the better one of the two
	 * estimations.
	 */
	epp = 2.0 * sqrt(epx*epx + epy*epy + epv*epv);
	epp = (epp + 100.0) / 299792458.0;

	ept = min(ept, epp  );
	ept = min(ept, 0.5  );
	ept = max(ept, 1.0-9);
	ept = frexp(ept, &xlog2);

	peer->precision = xlog2;
}

/* ------------------------------------------------------------------ */

static void
process_pps(
	peerT      * const peer ,
	json_ctx   * const jctx ,
	const l_fp * const rtime)
{
	clockprocT * const pp = peer->procptr;
	gpsd_unitT * const up = (gpsd_unitT *)pp->unitptr;

	struct timespec ts;
		
	errno = 0;
	ts.tv_sec = (time_t)json_object_lookup_int(
		jctx, 0, "clock_sec");
	if (up->fl_nsec)
		ts.tv_nsec = json_object_lookup_int(
			jctx, 0, "clock_nsec");
	else
		ts.tv_nsec = json_object_lookup_int(
			jctx, 0, "clock_musec") * 1000;

	if (0 != errno)
		goto fail;

	up->pps_local = *rtime;
	/* get fudged receive time */
	up->pps_recvt = tspec_stamp_to_lfp(ts);
	L_SUB(&up->pps_recvt, &up->pps_fudge);

	/* map to next full second as reference time stamp */
	up->pps_stamp = up->pps_recvt;
	L_ADDUF(&up->pps_stamp, 0x80000000u);
	up->pps_stamp.l_uf = 0;
	
	pp->lastrec = up->pps_stamp;

	DPRINTF(2, ("GPSD_JSON(%d): process_pps, stamp='%s', recvt='%s'\n", 
		    up->unit,
		    gmprettydate(&up->pps_stamp),
		    gmprettydate(&up->pps_recvt)));
	
	/* When we have a time pulse, clear the TPV flag: the
	 * PPS is only valid for the >NEXT< TPV value!
	 */
	up->fl_pps = -1;
	up->fl_tpv =  0;
	return;

  fail:
	DPRINTF(2, ("GPSD_JSON(%d): process_pps FAILED, nsec=%d stamp='%s', recvt='%s'\n",
		    up->unit, up->fl_nsec,
		    gmprettydate(&up->pps_stamp),
		    gmprettydate(&up->pps_recvt)));
	up->tc_breply += 1;
}

/* ------------------------------------------------------------------ */

static void
gpsd_parse(
	peerT      * const peer ,
	const l_fp * const rtime)
{
	clockprocT * const pp = peer->procptr;
	gpsd_unitT * const up = (gpsd_unitT *)pp->unitptr;

	json_ctx     jctx;
	const char * clsid;
	l_fp         tmpfp;

        DPRINTF(2, ("GPSD_JSON(%d): gpsd_parse: time %s '%s'\n",
                    up->unit, ulfptoa(rtime, 6), up->buffer));

	/* See if we can grab anything potentially useful */
	if (!json_parse_record(&jctx, up->buffer))
		return;

	/* Now dispatch over the objects we know */
	clsid = json_object_lookup_string_default(
		&jctx, 0, "class", "-bad-repy-");

	up->tc_recv += 1;
	if (!strcmp("VERSION", clsid))
		process_version(peer, &jctx, rtime);
	else if (!strcmp("TPV", clsid))
		process_tpv(peer, &jctx, rtime);
	else if (!strcmp("PPS", clsid))
		process_pps(peer, &jctx, rtime);
	else if (!strcmp("WATCH", clsid))
		process_watch(peer, &jctx, rtime);
	else
		return; /* nothing we know about... */

	/* now aggregate TPV and PPS -- no PPS? just use TPV...*/
	if (up->fl_tpv) {
		/* TODO: also check remote receive time stamps */
		tmpfp = up->tpv_local;
		L_SUB(&tmpfp, &up->pps_local);

		if (up->fl_pps && 0 == tmpfp.l_ui) {
			refclock_process_offset(
				pp, up->tpv_stamp, up->pps_recvt, 0.0);
			if (up->ppscount < PPS_MAXCOUNT)
				up->ppscount += 1;
		} else {
			refclock_process_offset(
				pp, up->tpv_stamp, up->tpv_recvt, 0.0);
			if (up->ppscount > 0)
				up->ppscount -= 1;
		}
		up->fl_pps   = 0;
		up->fl_tpv   = 0;
		up->tc_good += 1;
	}
}

/* ------------------------------------------------------------------ */

static void
gpsd_stop_socket(
	peerT * const peer)
{
	clockprocT * const pp = peer->procptr;
	gpsd_unitT * const up = (gpsd_unitT *)pp->unitptr;

	if (-1 != pp->io.fd)
		io_closeclock(&pp->io);
	pp->io.fd = -1;
	if (syslogok(pp, up))
		msyslog(LOG_INFO,
			"%s: closing socket to GPSD",
			refnumtoa(&peer->srcadr));
	up->tickover = up->tickpres;
	up->tickpres = min(up->tickpres + 5, TICKOVER_HIGH);
	up->fl_vers  = 0;
	up->fl_tpv   = 0;
	up->fl_pps   = 0;
	up->fl_watch = 0;
}

/* ------------------------------------------------------------------ */

static void
gpsd_init_socket(
	peerT * const peer)
{
	clockprocT * const pp = peer->procptr;
	gpsd_unitT * const up = (gpsd_unitT *)pp->unitptr;
	addrinfoT  * ai;
	int          rc;
	int          ov;

	/* draw next address to try */
	if (NULL == up->addr)
		up->addr = s_gpsd_addr;
	ai = up->addr;
	up->addr = ai->ai_next;

	/* try to create a matching socket */
	up->fdt = socket(
		ai->ai_family, ai->ai_socktype, ai->ai_protocol);
	if (-1 == up->fdt) {
		if (syslogok(pp, up))
			msyslog(LOG_ERR,
				"%s: cannot create GPSD socket: %m",
				refnumtoa(&peer->srcadr));
		goto no_socket;
	}
	
	/* make sure the socket is non-blocking */
	rc = fcntl(up->fdt, F_SETFL, O_NONBLOCK, 1);
	if (-1 == rc) {
		if (syslogok(pp, up))
			msyslog(LOG_ERR,
				"%s: cannot set GPSD socket to non-blocking: %m",
				refnumtoa(&peer->srcadr));
		goto no_socket;
	}
	/* disable nagling */
	ov = 1;
	rc = setsockopt(up->fdt, IPPROTO_TCP, TCP_NODELAY,
			(char*)&ov, sizeof(ov));
	if (-1 == rc) {
		if (syslogok(pp, up))
			msyslog(LOG_INFO,
				"%s: cannot disable TCP nagle: %m",
				refnumtoa(&peer->srcadr));
	}

	/* start a non-blocking connect */
	rc = connect(up->fdt, ai->ai_addr, ai->ai_addrlen);
	if (-1 == rc && errno != EINPROGRESS) {
		if (syslogok(pp, up))
			msyslog(LOG_ERR,
				"%s: cannot connect GPSD socket: %m",
				refnumtoa(&peer->srcadr));
		goto no_socket;
	}

	return;
  
  no_socket:
	if (-1 != up->fdt)
		close(up->fdt);
	up->fdt      = -1;
	up->tickover = up->tickpres;
	up->tickpres = min(up->tickpres + 5, TICKOVER_HIGH);
}

/* ------------------------------------------------------------------ */

static void
gpsd_test_socket(
	peerT * const peer)
{
	clockprocT * const pp = peer->procptr;
	gpsd_unitT * const up = (gpsd_unitT *)pp->unitptr;

	int       ec, rc;
	socklen_t lc;

	/* Check if the non-blocking connect was finished by testing the
	 * socket for writeability. Use the 'poll()' API if available
	 * and 'select()' otherwise.
	 */
	DPRINTF(2, ("GPSD_JSON(%d): check connect, fd=%d\n",
		    up->unit, up->fdt));

#if defined(HAVE_SYS_POLL_H)
	{
		struct pollfd pfd;

		pfd.events = POLLOUT;
		pfd.fd     = up->fdt;
		rc = poll(&pfd, 1, 0);
		if (1 != rc || !(pfd.revents & POLLOUT))
			return;
	}
#elif defined(HAVE_SYS_SELECT_H)
	{
		struct timeval tout;
		fd_set         wset;

		memset(&tout, 0, sizeof(tout));
		FD_ZERO(&wset);
		FD_SET(up->fdt, &wset);
		rc = select(up->fdt+1, NULL, &wset, NULL, &tout);
		if (0 == rc || !(FD_ISSET(up->fdt, &wset)))
			return;
	}
#else
# error Blooper! That should have been found earlier!
#endif

	/* next timeout is a full one... */
	up->tickover = TICKOVER_LOW;

	/* check for socket error */
	ec = 0;
	lc = sizeof(ec);
	rc = getsockopt(up->fdt, SOL_SOCKET, SO_ERROR, &ec, &lc);
	DPRINTF(1, ("GPSD_JSON(%d): connect finshed, fd=%d, ec=%d(%s)\n",
		    up->unit, up->fdt, ec, strerror(ec)));
	if (-1 == rc || 0 != ec) {
		errno = ec;
		if (syslogok(pp, up))
			msyslog(LOG_ERR,
				"%s: (async)cannot connect GPSD socket: %m",
				refnumtoa(&peer->srcadr));
		goto no_socket;
	}	
	/* swap socket FDs, and make sure the clock was added */
	pp->io.fd = up->fdt;
	up->fdt   = -1;
	if (0 == io_addclock(&pp->io)) {
		if (syslogok(pp, up))
			msyslog(LOG_ERR,
				"%s: failed to register with I/O engine",
				refnumtoa(&peer->srcadr));
		goto no_socket;
	}
	return;
	
  no_socket:
	if (-1 != up->fdt)
		close(up->fdt);
	up->fdt      = -1;
	up->tickover = up->tickpres;
	up->tickpres = min(up->tickpres + 5, TICKOVER_HIGH);
}

/* =====================================================================
 * helper stuff
 */

/*
 * shm_clockstats - dump and reset counters
 */
static void
gpsd_clockstats(
	int           unit,
	peerT * const peer
	)
{
	clockprocT * const pp = peer->procptr;
	gpsd_unitT * const up = (gpsd_unitT *)pp->unitptr;

	char logbuf[128];
	unsigned int llen;

	/* if snprintf() returns a negative values on errors (some older
	* ones do) make sure we are NUL terminated. Using an unsigned
	* result does the trick.
	*/
	llen = snprintf(logbuf, sizeof(logbuf),
			"good=%-3u badtime=%-3u baddate=%-3u badreply=%-3u recv=%-3u",
			up->tc_good, up->tc_btime, up->tc_bdate,
			up->tc_breply, up->tc_recv);
	logbuf[min(llen, sizeof(logbuf)-1)] = '\0';
	record_clock_stats(&peer->srcadr, logbuf);
}

/* -------------------------------------------------------------------
 * Convert a GPSD timestam (ISO8601 Format) to an l_fp
 */
static BOOL
convert_ascii_time(
	l_fp       * fp      ,
	const char * gps_time)
{
	char           *ep;
	struct tm       gd;
	struct timespec ts;
	long            dw;

	/* Use 'strptime' to take the brunt of the work, then parse
	 * the fractional part manually, starting with a digit weight of
	 * 10^8 nanoseconds.
	 */
	ts.tv_nsec = 0;
	ep = strptime(gps_time, "%Y-%m-%dT%H:%M:%S", &gd);
	if (*ep == '.') {
		dw = 100000000;
		while (isdigit((unsigned char)*++ep)) {
			ts.tv_nsec += (*ep - '0') * dw;
			dw /= 10;
		}
	}
	if (ep[0] != 'Z' || ep[1] != '\0')
		return FALSE;

	/* now convert the whole thing into a 'l_fp' */
	ts.tv_sec = (ntpcal_tm_to_rd(&gd) - DAY_NTP_STARTS) * SECSPERDAY
	          + ntpcal_tm_to_daysec(&gd);
	*fp = tspec_intv_to_lfp(ts);

	return TRUE;
}

/* -------------------------------------------------------------------
 * Save the last timecode string, making sure it's properly truncated
 * if necessary and NUL terminated in any case.
 */
static void
save_ltc(
	clockprocT * const pp,
	const char * const tc)
{
	size_t len;

	len = (tc) ? strlen(tc) : 0;
	if (len >= sizeof(pp->a_lastcode))
		len = sizeof(pp->a_lastcode) - 1;
	pp->lencode = (u_short)len;
	memcpy(pp->a_lastcode, tc, len);
	pp->a_lastcode[len] = '\0';
}

/*
 * -------------------------------------------------------------------
 * asprintf replacement... it's not available everywhere...
 */
static int
myasprintf(
	char      ** spp,
	char const * fmt,
	...             )
{
	size_t alen, plen;

	alen = 32;
	*spp = NULL;
	do {
		va_list va;

		alen += alen;
		free(*spp);
		*spp = (char*)malloc(alen);
		if (NULL == *spp)
			return -1;

		va_start(va, fmt);
		plen = (size_t)vsnprintf(*spp, alen, fmt, va);
		va_end(va);
	} while (plen >= alen);

	return (int)plen;
}

#else
NONEMPTY_TRANSLATION_UNIT
#endif /* REFCLOCK && CLOCK_GPSDJSON */
