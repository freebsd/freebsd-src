/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The FreeBSD Foundation
 *
 * This software was developed as part of the tcpstats kernel module.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * tcpstats -- read-only character device for streaming TCP connection
 * statistics to userspace.
 *
 * Creates /dev/tcpstats which, when read, iterates every TCP connection
 * in a single kernel pass and streams fixed-size 320-byte records via
 * uiomove().  Includes a filter system (ioctl + sysctl named profiles),
 * DTrace SDT probes, and DoS protections.
 */

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/priority.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/signalvar.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sx.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/ucred.h>
#include <sys/uio.h>

#include <machine/atomic.h>

#include <net/vnet.h>

#include <netinet/in.h>
#include <netinet/in_pcb.h>
#include <netinet/in_systm.h>
#include <netinet/tcp.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_var.h>

#include <netinet/cc/cc.h>

#include <netinet/tcp_statsdev.h>
#include <netinet/tcp_statsdev_filter.h>

/* ================================================================
 * DTrace SDT probes -- compile-time gated
 *
 * Default build (production): TCPSTATS_DTRACE not defined.
 * All probe macros expand to nothing.  Zero runtime cost.
 *
 * Profiling build: compile with -DTCPSTATS_DTRACE to enable probes.
 * ================================================================ */
#ifdef TCPSTATS_DTRACE
#ifndef KDTRACE_HOOKS
#error "TCPSTATS_DTRACE requires KDTRACE_HOOKS"
#endif
#include <sys/sdt.h>
SDT_PROVIDER_DEFINE(tcpstats);
SDT_PROBE_DEFINE2(tcpstats, , read, entry,
    "uio_resid", "filter_flags");
SDT_PROBE_DEFINE3(tcpstats, , read, done,
    "error", "records_emitted", "elapsed_ns");
SDT_PROBE_DEFINE2(tcpstats, , filter, skip,
    "inpcb_ptr", "reason_code");
SDT_PROBE_DEFINE1(tcpstats, , filter, match,
    "inpcb_ptr");
SDT_PROBE_DEFINE2(tcpstats, , fill, done,
    "elapsed_ns", "record_size");
SDT_PROBE_DEFINE1(tcpstats, , profile, create,
    "name");
SDT_PROBE_DEFINE1(tcpstats, , profile, destroy,
    "name");

#define	TSF_DTRACE_READ_ENTRY(resid, flags) \
	SDT_PROBE2(tcpstats, , read, entry, (resid), (flags))
#define	TSF_DTRACE_READ_DONE(err, nrec, ns) \
	SDT_PROBE3(tcpstats, , read, done, (err), (nrec), (ns))
#define	TSF_DTRACE_FILTER_SKIP(inp, reason) \
	SDT_PROBE2(tcpstats, , filter, skip, (inp), (reason))
#define	TSF_DTRACE_FILTER_MATCH(inp) \
	SDT_PROBE1(tcpstats, , filter, match, (inp))
#define	TSF_DTRACE_FILL_DONE(start, sz)					\
	do {								\
		sbintime_t _elapsed = getsbinuptime() - (start);	\
		uint64_t _ns =						\
		    (uint64_t)_elapsed * 1000000000 / SBT_1S;		\
		SDT_PROBE2(tcpstats, , fill, done, _ns, (sz));		\
	} while (0)
#define	TSF_DTRACE_PROFILE_CREATE(name) \
	SDT_PROBE1(tcpstats, , profile, create, (name))
#define	TSF_DTRACE_PROFILE_DESTROY(name) \
	SDT_PROBE1(tcpstats, , profile, destroy, (name))
#else
#define	TSF_DTRACE_READ_ENTRY(resid, flags)	((void)0)
#define	TSF_DTRACE_READ_DONE(err, nrec, ns)	((void)0)
#define	TSF_DTRACE_FILTER_SKIP(inp, reason)	((void)0)
#define	TSF_DTRACE_FILTER_MATCH(inp)		((void)0)
#define	TSF_DTRACE_FILL_DONE(start, sz)		((void)0)
#define	TSF_DTRACE_PROFILE_CREATE(name)		((void)0)
#define	TSF_DTRACE_PROFILE_DESTROY(name)	((void)0)
#endif

/* ================================================================
 * Debug logging -- compile-time gated
 *
 * Build with -DTCPSTATS_DEBUG to enable verbose kernel log output
 * for diagnosing filter matching issues.  Not for production use.
 * ================================================================ */
#ifdef TCPSTATS_DEBUG
#define	TSF_DBG(fmt, ...) \
	printf("tcpstats: " fmt, ##__VA_ARGS__)
/* Limit per-socket address mismatch logs to avoid flooding dmesg */
#define	TSF_DBG_ADDR_MAX	8
#else
#define	TSF_DBG(fmt, ...)	((void)0)
#define	TSF_DBG_ADDR_MAX	0
#endif

/* ================================================================
 * Statistics counters -- two-tier design
 *
 * Tier 1 (always enabled): low-frequency counters outside hot loop.
 * Tier 2 (compile-time -DTCPSTATS_STATS): per-socket hot loop counters.
 * ================================================================ */

/* Filter skip reason codes (for DTrace and stats) */
#define	TSF_SKIP_GENCNT		0
#define	TSF_SKIP_CRED		1
#define	TSF_SKIP_IPVER		2
#define	TSF_SKIP_STATE		3
#define	TSF_SKIP_PORT		4
#define	TSF_SKIP_ADDR		5
#define	TSF_SKIP_TIMEOUT	6

/* Tier 1: always-on counters */
static volatile u_int	tcpstats_active_fds;
static uint64_t		tcpstats_opens_total;
static uint64_t		tcpstats_reads_total;

/* Tier 2: compile-time gated hot loop counters */
#ifdef TCPSTATS_STATS
struct tcpstats_stats {
	uint64_t	records_emitted;
	uint64_t	sockets_visited;
	uint64_t	sockets_skipped_gencnt;
	uint64_t	sockets_skipped_cred;
	uint64_t	sockets_skipped_ipver;
	uint64_t	sockets_skipped_state;
	uint64_t	sockets_skipped_port;
	uint64_t	sockets_skipped_addr;
	uint64_t	read_duration_ns_total;
	uint64_t	read_duration_ns_max;
	uint64_t	uiomove_errors;
	uint64_t	reads_timed_out;
	uint64_t	reads_interrupted;
};

static struct tcpstats_stats tcpstats_stats;

/*
 * Hot-loop stats use local counters to avoid per-socket atomics.
 * Declare a local batch struct at the top of the read function, use
 * TSF_STAT_LOCAL_INC to accumulate, then TSF_STAT_FLUSH to write all
 * counters to the global struct with a single atomic per field.
 */
struct tcpstats_stats_batch {
	uint64_t	records_emitted;
	uint64_t	sockets_visited;
	uint64_t	sockets_skipped_gencnt;
	uint64_t	sockets_skipped_cred;
	uint64_t	sockets_skipped_ipver;
	uint64_t	sockets_skipped_state;
	uint64_t	sockets_skipped_port;
	uint64_t	sockets_skipped_addr;
	uint64_t	uiomove_errors;
	uint64_t	reads_timed_out;
	uint64_t	reads_interrupted;
};

#define	TSF_STAT_LOCAL_INC(batch, field)	((batch)->field++)
#define	TSF_STAT_FLUSH(batch)						\
	do {								\
		if ((batch)->records_emitted)				\
			atomic_add_64(&tcpstats_stats.records_emitted,	\
			    (batch)->records_emitted);			\
		if ((batch)->sockets_visited)				\
			atomic_add_64(&tcpstats_stats.sockets_visited,	\
			    (batch)->sockets_visited);			\
		if ((batch)->sockets_skipped_gencnt)			\
			atomic_add_64(					\
			    &tcpstats_stats.sockets_skipped_gencnt,	\
			    (batch)->sockets_skipped_gencnt);		\
		if ((batch)->sockets_skipped_cred)			\
			atomic_add_64(					\
			    &tcpstats_stats.sockets_skipped_cred,	\
			    (batch)->sockets_skipped_cred);		\
		if ((batch)->sockets_skipped_ipver)			\
			atomic_add_64(					\
			    &tcpstats_stats.sockets_skipped_ipver,	\
			    (batch)->sockets_skipped_ipver);		\
		if ((batch)->sockets_skipped_state)			\
			atomic_add_64(					\
			    &tcpstats_stats.sockets_skipped_state,	\
			    (batch)->sockets_skipped_state);		\
		if ((batch)->sockets_skipped_port)			\
			atomic_add_64(					\
			    &tcpstats_stats.sockets_skipped_port,	\
			    (batch)->sockets_skipped_port);		\
		if ((batch)->sockets_skipped_addr)			\
			atomic_add_64(					\
			    &tcpstats_stats.sockets_skipped_addr,	\
			    (batch)->sockets_skipped_addr);		\
		if ((batch)->uiomove_errors)				\
			atomic_add_64(&tcpstats_stats.uiomove_errors,	\
			    (batch)->uiomove_errors);			\
		if ((batch)->reads_timed_out)				\
			atomic_add_64(&tcpstats_stats.reads_timed_out,	\
			    (batch)->reads_timed_out);			\
		if ((batch)->reads_interrupted)				\
			atomic_add_64(					\
			    &tcpstats_stats.reads_interrupted,		\
			    (batch)->reads_interrupted);			\
	} while (0)

/* Non-batched accessors for outside hot loop */
#define	TSF_STAT_ADD(field, val) \
	atomic_add_64(&tcpstats_stats.field, (val))
#define	TSF_STAT_MAX(field, val)					\
	do {								\
		uint64_t _new = (val);					\
		uint64_t _cur;						\
		do {							\
			_cur = *(volatile uint64_t *)			\
			    &tcpstats_stats.field;			\
			if (_new <= _cur)				\
				break;					\
		} while (!atomic_cmpset_64(				\
		    (volatile uint64_t *)&tcpstats_stats.field,		\
		    _cur, _new));					\
	} while (0)
#else
#define	TSF_STAT_LOCAL_INC(batch, field)	((void)0)
#define	TSF_STAT_FLUSH(batch)		((void)0)
#define	TSF_STAT_ADD(field, val)		((void)0)
#define	TSF_STAT_MAX(field, val)		((void)0)
#endif

#ifndef GID_NETWORK
#define	GID_NETWORK	69
#endif

MALLOC_DEFINE(M_TCPSTATS, "tcpstats", "tcpstats per-fd state");

/* ================================================================
 * DoS protection: tunable limits
 * ================================================================ */

/* Maximum concurrent open file descriptors across all devices */
static u_int	tcpstats_max_open_fds = 16;

/* Maximum concurrent readers in tcpstats_read() */
static u_int		tcpstats_max_concurrent_readers = 32;
static volatile u_int	tcpstats_active_readers;

/* Maximum read() duration in milliseconds (0 = unlimited) */
static u_int	tcpstats_max_read_duration_ms = 5000;

/* Minimum interval between reads in milliseconds (0 = unlimited) */
static u_int	tcpstats_min_read_interval_ms = 0;

/* How often to check timeout/signals (every N sockets) */
#define	TSF_CHECK_INTERVAL	1024

struct tcpstats_softc {
	struct ucred		*sc_cred;
	uint64_t		sc_gen;
	int			sc_started;
	int			sc_done;
	struct tcpstats_filter	sc_filter;
	int			sc_full;
	sbintime_t		sc_last_read;
};

static d_open_t		tcpstats_open;
static d_read_t		tcpstats_read;
static d_ioctl_t	tcpstats_ioctl;

static void	tcpstats_dtor(void *data);

static struct cdev	*tcpstats_dev;
static struct cdev	*tcpstats_full_dev;

static struct cdevsw tcpstats_cdevsw = {
	.d_version =	D_VERSION,
	.d_name =	"tcpstats",
	.d_open =	tcpstats_open,
	.d_read =	tcpstats_read,
	.d_ioctl =	tcpstats_ioctl,
};

static struct cdevsw tcpstats_full_cdevsw = {
	.d_version =	D_VERSION,
	.d_name =	"tcpstats-full",
	.d_open =	tcpstats_open,
	.d_read =	tcpstats_read,
	.d_ioctl =	tcpstats_ioctl,
};

/* --- Named filter profiles --- */

#define	TSF_MAX_PROFILES	16
#define	TSF_PROFILE_NAME_MAX	32

struct tcpstats_profile {
	char			name[TSF_PROFILE_NAME_MAX];
	char			filter_str[TSF_PARSE_MAXLEN];
	struct tcpstats_filter	filter;
	struct cdev		*dev;
	SLIST_ENTRY(tcpstats_profile) link;
};

static SLIST_HEAD(, tcpstats_profile) tcpstats_profiles =
    SLIST_HEAD_INITIALIZER(tcpstats_profiles);
static int		tcpstats_nprofiles;
static struct sx	tcpstats_profile_lock;

static char	tcpstats_last_error[TSF_ERRBUF_SIZE];

static d_open_t	tcpstats_profile_open;

static struct cdevsw tcpstats_profile_cdevsw = {
	.d_version =	D_VERSION,
	.d_name =	"tcpstats-profile",
	.d_open =	tcpstats_profile_open,
	.d_read =	tcpstats_read,
	.d_ioctl =	tcpstats_ioctl,
};

static int
tcpstats_profile_open(struct cdev *dev, int oflags, int devtype,
    struct thread *td)
{
	struct tcpstats_profile *prof;
	struct tcpstats_softc *sc;
	int error;

	prof = dev->si_drv1;

	if (oflags & FWRITE)
		return (EPERM);

	/* DoS protection: enforce max open fds */
	if (atomic_fetchadd_int(&tcpstats_active_fds, 1) >=
	    tcpstats_max_open_fds) {
		atomic_subtract_int(&tcpstats_active_fds, 1);
		return (EMFILE);
	}

	sc = malloc(sizeof(*sc), M_TCPSTATS, M_WAITOK | M_ZERO);
	sc->sc_cred = crhold(td->td_ucred);
	bcopy(&prof->filter, &sc->sc_filter, sizeof(sc->sc_filter));
	sc->sc_full = (prof->filter.format == TSF_FORMAT_FULL);

	error = devfs_set_cdevpriv(sc, tcpstats_dtor);
	if (error != 0) {
		crfree(sc->sc_cred);
		free(sc, M_TCPSTATS);
		atomic_subtract_int(&tcpstats_active_fds, 1);
		return (error);
	}

	atomic_add_64(&tcpstats_opens_total, 1);
	return (0);
}

/* --- Sysctl infrastructure --- */

static struct sysctl_ctx_list	tcpstats_sysctl_ctx;
static struct sysctl_oid	*tcpstats_sysctl_tree;
static struct sysctl_oid	*tcpstats_profiles_node;

/*
 * Validate profile name: [a-z0-9_]+, max 32 chars.
 */
static int
tsf_validate_profile_name(const char *name, char *errbuf, size_t errbuflen)
{
	size_t len, i;
	char c;

	len = strnlen(name, TSF_PROFILE_NAME_MAX + 1);

	if (len == 0) {
		snprintf(errbuf, errbuflen, "empty profile name");
		return (EINVAL);
	}
	if (len > TSF_PROFILE_NAME_MAX) {
		snprintf(errbuf, errbuflen,
		    "profile name too long (%zu > %d)",
		    len, TSF_PROFILE_NAME_MAX);
		return (ENAMETOOLONG);
	}

	for (i = 0; i < len; i++) {
		c = name[i];
		if (!((c >= 'a' && c <= 'z') ||
		    (c >= '0' && c <= '9') || c == '_')) {
			snprintf(errbuf, errbuflen,
			    "invalid character '%c' in profile name "
			    "(allowed: a-z, 0-9, _)", c);
			return (EINVAL);
		}
	}

	return (0);
}

static struct tcpstats_profile *
tcpstats_profile_find(const char *name)
{
	struct tcpstats_profile *prof;

	sx_assert(&tcpstats_profile_lock, SA_LOCKED);
	SLIST_FOREACH(prof, &tcpstats_profiles, link) {
		if (strcmp(prof->name, name) == 0)
			return (prof);
	}
	return (NULL);
}

/*
 * Destroy a profile.
 *
 * The caller must have already removed the profile from the list
 * and released the sx xlock.  destroy_dev() can block waiting for
 * open fds to close, so it must not be called under the lock.
 */
static void
tcpstats_profile_destroy_unlocked(struct tcpstats_profile *prof)
{

	/* destroy_dev() waits for all open fds -- safe outside lock */
	if (prof->dev != NULL)
		destroy_dev(prof->dev);

	log(LOG_NOTICE, "tcpstats: profile '%s' deleted\n", prof->name);
	TSF_DTRACE_PROFILE_DESTROY(prof->name);
	free(prof, M_TCPSTATS);
}

/*
 * Remove a profile from the list under lock and return it.
 * Caller must hold sx xlock.
 */
static struct tcpstats_profile *
tcpstats_profile_detach(struct tcpstats_profile *prof)
{

	sx_assert(&tcpstats_profile_lock, SA_XLOCKED);
	SLIST_REMOVE(&tcpstats_profiles, prof, tcpstats_profile, link);
	tcpstats_nprofiles--;
	return (prof);
}

/*
 * Sysctl handler for individual profile readback.
 * Registered dynamically as dev.tcpstats.profiles.<name>.
 * Read returns the current filter string.
 * Write with empty string deletes the profile.
 * Write with non-empty string updates the filter.
 */
static int
tcpstats_profile_sysctl_handler(SYSCTL_HANDLER_ARGS)
{
	char input[TSF_PARSE_MAXLEN];
	char errbuf[TSF_ERRBUF_SIZE];
	struct tcpstats_filter filter;
	struct tcpstats_profile *prof;
	const char *profile_name;
	int error;

	profile_name = (const char *)arg1;

	/* Handle read: return the current filter string */
	if (req->newptr == NULL) {
		sx_slock(&tcpstats_profile_lock);
		prof = tcpstats_profile_find(profile_name);
		if (prof != NULL)
			error = SYSCTL_OUT(req, prof->filter_str,
			    strlen(prof->filter_str) + 1);
		else
			error = SYSCTL_OUT(req, "", 1);
		sx_sunlock(&tcpstats_profile_lock);
		return (error);
	}

	/* Handle write */
	bzero(input, sizeof(input));
	error = SYSCTL_IN(req, input, sizeof(input) - 1);
	if (error != 0)
		return (error);
	input[sizeof(input) - 1] = '\0';

	/* Trim trailing newline if present (from echo) */
	{
		size_t slen = strlen(input);

		if (slen > 0 && input[slen - 1] == '\n')
			input[slen - 1] = '\0';
	}

	/* Empty string = delete profile */
	if (input[0] == '\0') {
		sx_xlock(&tcpstats_profile_lock);
		prof = tcpstats_profile_find(profile_name);
		if (prof != NULL) {
			tcpstats_profile_detach(prof);
			sx_xunlock(&tcpstats_profile_lock);
			tcpstats_profile_destroy_unlocked(prof);
		} else {
			sx_xunlock(&tcpstats_profile_lock);
		}
		return (0);
	}

	/* Parse the filter string */
	errbuf[0] = '\0';
	error = tsf_parse_filter_string(input, strlen(input),
	    &filter, errbuf, sizeof(errbuf));
	if (error != 0) {
		strlcpy(tcpstats_last_error, errbuf,
		    sizeof(tcpstats_last_error));
		log(LOG_NOTICE,
		    "tcpstats: filter parse error: %s\n", errbuf);
		return (error);
	}

	/* Clear last error on success */
	tcpstats_last_error[0] = '\0';

	sx_xlock(&tcpstats_profile_lock);

	/* Update existing profile */
	prof = tcpstats_profile_find(profile_name);
	if (prof != NULL) {
		bcopy(&filter, &prof->filter, sizeof(prof->filter));
		strlcpy(prof->filter_str, input,
		    sizeof(prof->filter_str));
		sx_xunlock(&tcpstats_profile_lock);
		log(LOG_NOTICE,
		    "tcpstats: profile '%s' updated: %s\n",
		    profile_name, input);
		return (0);
	}

	sx_xunlock(&tcpstats_profile_lock);
	return (ENOENT);
}

/*
 * Sysctl handler for creating new profiles.
 * Written as: sysctl dev.tcpstats.profile_set="name filter_string"
 * The profile name and filter string are separated by the first space.
 * Reading returns a list of active profiles.
 */
static int
tcpstats_profile_set_handler(SYSCTL_HANDLER_ARGS)
{
	char input[TSF_PARSE_MAXLEN + TSF_PROFILE_NAME_MAX + 2];
	char errbuf[TSF_ERRBUF_SIZE];
	char *name_end, *filter_str;
	struct tcpstats_filter filter;
	struct tcpstats_profile *prof;
	int error;

	/* Handle read: list existing profiles */
	if (req->newptr == NULL) {
		char buf[2048];
		int pos;

		pos = 0;
		sx_slock(&tcpstats_profile_lock);
		SLIST_FOREACH(prof, &tcpstats_profiles, link) {
			pos += snprintf(buf + pos,
			    sizeof(buf) - pos,
			    "%s: %s\n", prof->name,
			    prof->filter_str);
			if (pos >= (int)sizeof(buf) - 1)
				break;
		}
		sx_sunlock(&tcpstats_profile_lock);

		if (pos == 0)
			return (SYSCTL_OUT(req, "(none)\n", 8));
		return (SYSCTL_OUT(req, buf, pos + 1));
	}

	/* Handle write */
	bzero(input, sizeof(input));
	error = SYSCTL_IN(req, input, sizeof(input) - 1);
	if (error != 0)
		return (error);
	input[sizeof(input) - 1] = '\0';

	/* Trim trailing newline */
	{
		size_t slen = strlen(input);

		if (slen > 0 && input[slen - 1] == '\n')
			input[slen - 1] = '\0';
	}

	/* Split "name filter_string" on first space */
	name_end = strchr(input, ' ');
	if (name_end == NULL) {
		/* Just a name with no filter = delete */
		errbuf[0] = '\0';
		error = tsf_validate_profile_name(input, errbuf,
		    sizeof(errbuf));
		if (error != 0) {
			strlcpy(tcpstats_last_error, errbuf,
			    sizeof(tcpstats_last_error));
			return (error);
		}
		sx_xlock(&tcpstats_profile_lock);
		prof = tcpstats_profile_find(input);
		if (prof != NULL) {
			tcpstats_profile_detach(prof);
			sx_xunlock(&tcpstats_profile_lock);
			tcpstats_profile_destroy_unlocked(prof);
		} else {
			sx_xunlock(&tcpstats_profile_lock);
		}
		return (0);
	}
	*name_end = '\0';
	filter_str = name_end + 1;

	/* Validate profile name */
	errbuf[0] = '\0';
	error = tsf_validate_profile_name(input, errbuf, sizeof(errbuf));
	if (error != 0) {
		strlcpy(tcpstats_last_error, errbuf,
		    sizeof(tcpstats_last_error));
		log(LOG_NOTICE,
		    "tcpstats: profile name error: %s\n", errbuf);
		return (error);
	}

	/* Parse the filter string */
	errbuf[0] = '\0';
	error = tsf_parse_filter_string(filter_str, strlen(filter_str),
	    &filter, errbuf, sizeof(errbuf));
	if (error != 0) {
		strlcpy(tcpstats_last_error, errbuf,
		    sizeof(tcpstats_last_error));
		log(LOG_NOTICE,
		    "tcpstats: filter parse error: %s\n", errbuf);
		return (error);
	}

	/* Clear last error on success */
	tcpstats_last_error[0] = '\0';

	sx_xlock(&tcpstats_profile_lock);

	/* Check if profile already exists -- update it */
	prof = tcpstats_profile_find(input);
	if (prof != NULL) {
		bcopy(&filter, &prof->filter, sizeof(prof->filter));
		strlcpy(prof->filter_str, filter_str,
		    sizeof(prof->filter_str));
		sx_xunlock(&tcpstats_profile_lock);
		log(LOG_NOTICE,
		    "tcpstats: profile '%s' updated: %s\n",
		    input, filter_str);
		return (0);
	}

	/* Check profile limit */
	if (tcpstats_nprofiles >= TSF_MAX_PROFILES) {
		sx_xunlock(&tcpstats_profile_lock);
		snprintf(errbuf, sizeof(errbuf),
		    "maximum profiles reached (%d)", TSF_MAX_PROFILES);
		strlcpy(tcpstats_last_error, errbuf,
		    sizeof(tcpstats_last_error));
		return (ENOSPC);
	}

	/* Create new profile */
	prof = malloc(sizeof(*prof), M_TCPSTATS, M_WAITOK | M_ZERO);
	strlcpy(prof->name, input, sizeof(prof->name));
	strlcpy(prof->filter_str, filter_str, sizeof(prof->filter_str));
	bcopy(&filter, &prof->filter, sizeof(prof->filter));

	/* Create /dev/tcpstats/<name> */
	prof->dev = make_dev_credf(0, &tcpstats_profile_cdevsw, 0, NULL,
	    UID_ROOT, GID_NETWORK, 0440, "tcpstats/%s", prof->name);
	if (prof->dev == NULL) {
		free(prof, M_TCPSTATS);
		sx_xunlock(&tcpstats_profile_lock);
		snprintf(errbuf, sizeof(errbuf),
		    "failed to create device for profile '%s'", input);
		strlcpy(tcpstats_last_error, errbuf,
		    sizeof(tcpstats_last_error));
		return (ENXIO);
	}
	prof->dev->si_drv1 = prof;

	SLIST_INSERT_HEAD(&tcpstats_profiles, prof, link);
	tcpstats_nprofiles++;

	sx_xunlock(&tcpstats_profile_lock);

	/* Register a per-profile sysctl for readback/update/delete */
	SYSCTL_ADD_PROC(&tcpstats_sysctl_ctx,
	    SYSCTL_CHILDREN(tcpstats_profiles_node),
	    OID_AUTO, prof->name,
	    CTLTYPE_STRING | CTLFLAG_RW | CTLFLAG_MPSAFE,
	    prof->name, 0,
	    tcpstats_profile_sysctl_handler, "A",
	    "Filter profile");

	TSF_DTRACE_PROFILE_CREATE(prof->name);
	log(LOG_NOTICE, "tcpstats: profile '%s' created: %s\n",
	    prof->name, filter_str);
	return (0);
}

static void
tcpstats_profiles_destroy_all(void)
{
	struct tcpstats_profile *prof, *tmp;
	struct tcpstats_profile *detached[TSF_MAX_PROFILES];
	int ndetached, i;

	/*
	 * Detach all profiles under lock, then destroy outside lock.
	 * This avoids holding the sx xlock during potentially-blocking
	 * destroy_dev() calls.
	 */
	ndetached = 0;
	sx_xlock(&tcpstats_profile_lock);
	SLIST_FOREACH_SAFE(prof, &tcpstats_profiles, link, tmp) {
		SLIST_REMOVE(&tcpstats_profiles, prof,
		    tcpstats_profile, link);
		tcpstats_nprofiles--;
		detached[ndetached++] = prof;
	}
	sx_xunlock(&tcpstats_profile_lock);

	for (i = 0; i < ndetached; i++) {
		if (detached[i]->dev != NULL)
			destroy_dev(detached[i]->dev);
		free(detached[i], M_TCPSTATS);
	}
}

static int
tcpstats_open(struct cdev *dev, int oflags, int devtype, struct thread *td)
{
	struct tcpstats_softc *sc;
	int error;

	if (oflags & FWRITE)
		return (EPERM);

	/* DoS protection: enforce max open fds */
	if (atomic_fetchadd_int(&tcpstats_active_fds, 1) >=
	    tcpstats_max_open_fds) {
		atomic_subtract_int(&tcpstats_active_fds, 1);
		return (EMFILE);
	}

	sc = malloc(sizeof(*sc), M_TCPSTATS, M_WAITOK | M_ZERO);
	sc->sc_cred = crhold(td->td_ucred);
	sc->sc_filter.version = TSF_VERSION;
	sc->sc_filter.state_mask = 0xFFFF;
	sc->sc_filter.field_mask = TSR_FIELDS_DEFAULT;
	sc->sc_full = (dev->si_devsw == &tcpstats_full_cdevsw);

	error = devfs_set_cdevpriv(sc, tcpstats_dtor);
	if (error != 0) {
		crfree(sc->sc_cred);
		free(sc, M_TCPSTATS);
		atomic_subtract_int(&tcpstats_active_fds, 1);
		return (error);
	}

	atomic_add_64(&tcpstats_opens_total, 1);
	return (0);
}

/* ================================================================
 * IPv6 prefix matching helper
 * ================================================================ */
static int
tsf_match_v6_prefix(const struct in6_addr *addr, const struct in6_addr *net,
    uint8_t prefix)
{
	int full_bytes, remainder_bits;

	if (prefix == 0)
		return (1);

	full_bytes = prefix / 8;
	remainder_bits = prefix % 8;

	/* Compare full bytes */
	if (full_bytes > 0 && bcmp(addr, net, full_bytes) != 0)
		return (0);

	/* Compare partial byte */
	if (remainder_bits > 0) {
		uint8_t mask = (uint8_t)(0xFF << (8 - remainder_bits));

		if ((addr->s6_addr[full_bytes] & mask) !=
		    (net->s6_addr[full_bytes] & mask))
			return (0);
	}

	return (1);
}

/* ================================================================
 * Record fill functions
 * ================================================================ */

static void
tcpstats_fill_identity(struct tcp_stats_record *rec, struct inpcb *inp,
    struct tcpcb *tp)
{
	struct socket *so;

	rec->tsr_version = TCP_STATS_VERSION;
	rec->tsr_len = TCP_STATS_RECORD_SIZE;

	/* AF and addresses */
	if (inp->inp_vflag & INP_IPV6) {
		rec->tsr_af = AF_INET6;
		rec->tsr_flags |= TSR_F_IPV6;
		rec->tsr_local_addr.v6 = inp->inp_inc.inc6_laddr;
		rec->tsr_remote_addr.v6 = inp->inp_inc.inc6_faddr;
	} else {
		rec->tsr_af = AF_INET;
		rec->tsr_local_addr.v4 = inp->inp_inc.inc_laddr;
		rec->tsr_remote_addr.v4 = inp->inp_inc.inc_faddr;
	}

	rec->tsr_local_port = ntohs(inp->inp_lport);
	rec->tsr_remote_port = ntohs(inp->inp_fport);

	/* TCP state from tcpcb (tp already resolved by caller) */
	if (tp != NULL) {
		rec->tsr_state = tp->t_state;
		rec->tsr_flags_tcp = tp->t_flags;
		if (tp->t_state == TCPS_LISTEN)
			rec->tsr_flags |= TSR_F_LISTEN;
	}

	/* Socket metadata */
	so = inp->inp_socket;
	if (so != NULL) {
		rec->tsr_so_addr = (uint64_t)(uintptr_t)so;
		if (so->so_cred != NULL)
			rec->tsr_uid = so->so_cred->cr_uid;
	}

	rec->tsr_inp_gencnt = inp->inp_gencnt;
}

/*
 * Fill RTT, sequence numbers, congestion, and window fields.
 * Replicates tcp_fill_info() logic since that function is static.
 */
static void
tcpstats_fill_record(struct tcp_stats_record *rec, struct inpcb *inp,
    struct tcpcb *tp, uint32_t field_mask, sbintime_t now)
{
#ifdef TCPSTATS_DTRACE
	sbintime_t fill_start __unused = getsbinuptime();
#endif
	int i;

	tcpstats_fill_identity(rec, inp, tp);

	if (tp == NULL || tp->t_state == TCPS_LISTEN)
		goto done;

	/* RTT -- replicate tcp_fill_info() conversion to usec */
	if (field_mask & TSR_FIELDS_RTT) {
		rec->tsr_rtt = ((uint64_t)tp->t_srtt * tick) >>
		    TCP_RTT_SHIFT;
		rec->tsr_rttvar = ((uint64_t)tp->t_rttvar * tick) >>
		    TCP_RTTVAR_SHIFT;
		rec->tsr_rto = tp->t_rxtcur * tick;
		rec->tsr_rttmin = tp->t_rttlow;
	}

	/* Window scale and options */
	if (field_mask & TSR_FIELDS_STATE) {
		if ((tp->t_flags & TF_REQ_SCALE) &&
		    (tp->t_flags & TF_RCVD_SCALE)) {
			rec->tsr_options |= TCPI_OPT_WSCALE;
			rec->tsr_snd_wscale = tp->snd_scale;
			rec->tsr_rcv_wscale = tp->rcv_scale;
		}
		if ((tp->t_flags & TF_REQ_TSTMP) &&
		    (tp->t_flags & TF_RCVD_TSTMP))
			rec->tsr_options |= TCPI_OPT_TIMESTAMPS;
		if (tp->t_flags & TF_SACK_PERMIT)
			rec->tsr_options |= TCPI_OPT_SACK;
	}

	/* Sequence numbers */
	if (field_mask & TSR_FIELDS_SEQUENCES) {
		rec->tsr_snd_nxt = tp->snd_nxt;
		rec->tsr_snd_una = tp->snd_una;
		rec->tsr_snd_max = tp->snd_max;
		rec->tsr_rcv_nxt = tp->rcv_nxt;
		rec->tsr_rcv_adv = tp->rcv_adv;
	}

	/* Congestion */
	if (field_mask & TSR_FIELDS_CONGESTION) {
		rec->tsr_snd_cwnd = tp->snd_cwnd;
		rec->tsr_snd_ssthresh = tp->snd_ssthresh;
		rec->tsr_snd_wnd = tp->snd_wnd;
		rec->tsr_rcv_wnd = tp->rcv_wnd;
		rec->tsr_maxseg = tp->t_maxseg;
	}

	/* CC algo and TCP stack names */
	if (field_mask & TSR_FIELDS_NAMES) {
		if (CC_ALGO(tp) != NULL)
			strlcpy(rec->tsr_cc, CC_ALGO(tp)->name,
			    sizeof(rec->tsr_cc));
		if (tp->t_fb != NULL)
			strlcpy(rec->tsr_stack,
			    tp->t_fb->tfb_tcp_block_name,
			    sizeof(rec->tsr_stack));
	}

	/* Counters */
	if (field_mask & TSR_FIELDS_COUNTERS) {
		rec->tsr_snd_rexmitpack = tp->t_sndrexmitpack;
		rec->tsr_rcv_ooopack = tp->t_rcvoopack;
		rec->tsr_snd_zerowin = tp->t_sndzerowin;
		rec->tsr_dupacks = tp->t_dupacks;
		rec->tsr_rcv_numsacks = tp->rcv_numsacks;
	}

	/* ECN */
	if (field_mask & TSR_FIELDS_ECN) {
		if ((tp->t_flags2 &
		    (TF2_ECN_PERMIT | TF2_ACE_PERMIT)) ==
		    (TF2_ECN_PERMIT | TF2_ACE_PERMIT))
			rec->tsr_delivered_ce = tp->t_scep - 5;
		else
			rec->tsr_delivered_ce = tp->t_scep;
		rec->tsr_received_ce = tp->t_rcep;
		rec->tsr_ecn =
		    (tp->t_flags2 & TF2_ECN_PERMIT) ? 1 : 0;

		/* DSACK */
		rec->tsr_dsack_bytes = tp->t_dsack_bytes;
		rec->tsr_dsack_pack = tp->t_dsack_pack;

		/* TLP */
		rec->tsr_total_tlp = tp->t_sndtlppack;
		rec->tsr_total_tlp_bytes = tp->t_sndtlpbyte;
	}

	/* Timers -- remaining time in ms, 0 if not running */
	if (field_mask & TSR_FIELDS_TIMERS) {
		int32_t *timer_fields[] = {
			&rec->tsr_tt_rexmt,
			&rec->tsr_tt_persist,
			&rec->tsr_tt_keep,
			&rec->tsr_tt_2msl,
			&rec->tsr_tt_delack,
		};

		for (i = 0; i < TT_N; i++) {
			if (tp->t_timers[i] == SBT_MAX ||
			    tp->t_timers[i] == 0)
				*timer_fields[i] = 0;
			else
				*timer_fields[i] = (int32_t)(
				    (tp->t_timers[i] - now) /
				    SBT_1MS);
		}
		rec->tsr_rcvtime =
		    ((uint32_t)ticks - tp->t_rcvtime) * tick / 1000;
	}

	/* Buffer utilization */
	if (field_mask & TSR_FIELDS_BUFFERS) {
		struct socket *so = inp->inp_socket;

		if (so != NULL) {
			rec->tsr_snd_buf_cc = so->so_snd.sb_ccc;
			rec->tsr_snd_buf_hiwat = so->so_snd.sb_hiwat;
			rec->tsr_rcv_buf_cc = so->so_rcv.sb_ccc;
			rec->tsr_rcv_buf_hiwat = so->so_rcv.sb_hiwat;
		}
	}

done:
	TSF_DTRACE_FILL_DONE(fill_start, (uint64_t)sizeof(*rec));
	return;
}

/* ================================================================
 * Main read path
 * ================================================================ */

static int
tcpstats_read(struct cdev *dev, struct uio *uio, int ioflag)
{
	struct tcpstats_softc *sc;
	struct tcp_stats_record rec;
	struct inpcb *inp;
	struct tcpcb *tp;
	uint64_t gencnt;
	sbintime_t now, read_start, deadline;
	uint32_t field_mask;
	int error, nsockets;
	uint8_t vflag;
#ifdef TCPSTATS_STATS
	struct tcpstats_stats_batch stats_batch;

	bzero(&stats_batch, sizeof(stats_batch));
#endif
	int nrecords __unused;
#ifdef TCPSTATS_DEBUG
	int dbg_skip_gencnt = 0, dbg_skip_cred = 0, dbg_skip_ipver = 0;
	int dbg_skip_state = 0, dbg_skip_port = 0, dbg_skip_addr = 0;
	int dbg_addr_logged = 0;
#endif

	error = devfs_get_cdevpriv((void **)&sc);
	if (error != 0)
		return (error);

	if (sc->sc_done)
		return (0);

	/* DoS protection: enforce concurrent reader limit */
	if (atomic_fetchadd_int(&tcpstats_active_readers, 1) >=
	    tcpstats_max_concurrent_readers) {
		atomic_subtract_int(&tcpstats_active_readers, 1);
		return (EBUSY);
	}

	/* DoS protection: per-fd read interval throttle */
	now = getsbinuptime();
	if (tcpstats_min_read_interval_ms > 0 &&
	    sc->sc_last_read != 0) {
		sbintime_t min_interval =
		    (sbintime_t)tcpstats_min_read_interval_ms * SBT_1MS;

		if (now - sc->sc_last_read < min_interval) {
			atomic_subtract_int(&tcpstats_active_readers, 1);
			return (EBUSY);
		}
	}
	sc->sc_last_read = now;

	read_start = now;
	nrecords = 0;
	nsockets = 0;

	/* Compute deadline for iteration timeout */
	if (tcpstats_max_read_duration_ms > 0)
		deadline = read_start +
		    (sbintime_t)tcpstats_max_read_duration_ms * SBT_1MS;
	else
		deadline = 0; /* no deadline */

	/* Cache field_mask for the entire read */
	field_mask = sc->sc_filter.field_mask;
	if (field_mask == 0)
		field_mask = TSR_FIELDS_DEFAULT;

	atomic_add_64(&tcpstats_reads_total, 1);
	TSF_DTRACE_READ_ENTRY(uio->uio_resid, sc->sc_filter.flags);

	TSF_DBG("read: resid=%zd flags=0x%x state_mask=0x%x\n",
	    uio->uio_resid, sc->sc_filter.flags,
	    sc->sc_filter.state_mask);

	CURVNET_SET(TD_TO_VNET(curthread));

	{
		struct inpcb_iterator inpi = INP_ALL_ITERATOR(&V_tcbinfo,
		    INPLOOKUP_RLOCKPCB);

		gencnt = V_tcbinfo.ipi_gencnt;

		while ((inp = inp_next(&inpi)) != NULL) {
			if (uio->uio_resid < (ssize_t)sizeof(rec)) {
				INP_RUNLOCK(inp);
				break;
			}

			nsockets++;
			TSF_STAT_LOCAL_INC(&stats_batch, sockets_visited);

			/*
			 * Periodic checks: timeout, signals, voluntary
			 * preemption.  Every TSF_CHECK_INTERVAL sockets
			 * to amortize overhead.
			 */
			if ((nsockets & (TSF_CHECK_INTERVAL - 1)) == 0) {
				/* Signal check */
				if (SIGPENDING(curthread)) {
					INP_RUNLOCK(inp);
					TSF_STAT_LOCAL_INC(&stats_batch,
					    reads_interrupted);
					error = EINTR;
					goto out;
				}

				/* Timeout check */
				if (deadline != 0 &&
				    getsbinuptime() > deadline) {
					INP_RUNLOCK(inp);
					TSF_STAT_LOCAL_INC(&stats_batch,
					    reads_timed_out);
					break;
				}

				/* Voluntary preemption */
				kern_yield(PRI_USER);
			}

			/* Skip entries added after our snapshot */
			if (inp->inp_gencnt > gencnt) {
				TSF_DTRACE_FILTER_SKIP(inp,
				    TSF_SKIP_GENCNT);
				TSF_STAT_LOCAL_INC(&stats_batch,
				    sockets_skipped_gencnt);
#ifdef TCPSTATS_DEBUG
				dbg_skip_gencnt++;
#endif
				continue;
			}

			/* Cache vflag */
			vflag = inp->inp_vflag;

			/* IP version filter */
			if ((sc->sc_filter.flags & TSF_IPV4_ONLY) &&
			    !(vflag & INP_IPV4)) {
				TSF_DTRACE_FILTER_SKIP(inp,
				    TSF_SKIP_IPVER);
				TSF_STAT_LOCAL_INC(&stats_batch,
				    sockets_skipped_ipver);
#ifdef TCPSTATS_DEBUG
				dbg_skip_ipver++;
#endif
				continue;
			}
			if ((sc->sc_filter.flags & TSF_IPV6_ONLY) &&
			    !(vflag & INP_IPV6)) {
				TSF_DTRACE_FILTER_SKIP(inp,
				    TSF_SKIP_IPVER);
				TSF_STAT_LOCAL_INC(&stats_batch,
				    sockets_skipped_ipver);
#ifdef TCPSTATS_DEBUG
				dbg_skip_ipver++;
#endif
				continue;
			}

			/* State filtering */
			tp = intotcpcb(inp);
			if (tp != NULL) {
				if (sc->sc_filter.state_mask != 0xFFFF &&
				    !(sc->sc_filter.state_mask &
				    (1 << tp->t_state))) {
					TSF_DTRACE_FILTER_SKIP(inp,
					    TSF_SKIP_STATE);
					TSF_STAT_LOCAL_INC(&stats_batch,
					    sockets_skipped_state);
#ifdef TCPSTATS_DEBUG
					dbg_skip_state++;
#endif
					continue;
				}
			}

			/* Credential visibility check */
			if (cr_canseeinpcb(sc->sc_cred, inp) != 0) {
				TSF_DTRACE_FILTER_SKIP(inp,
				    TSF_SKIP_CRED);
				TSF_STAT_LOCAL_INC(&stats_batch,
				    sockets_skipped_cred);
#ifdef TCPSTATS_DEBUG
				dbg_skip_cred++;
#endif
				continue;
			}

			/* Port filtering */
			if (sc->sc_filter.flags &
			    TSF_LOCAL_PORT_MATCH) {
				uint16_t lport =
				    inp->inp_inc.inc_lport;
				int found, j;

				found = 0;
				for (j = 0; j < TSF_MAX_PORTS &&
				    sc->sc_filter.local_ports[j] != 0;
				    j++) {
					if (lport ==
					    sc->sc_filter.local_ports[j]) {
						found = 1;
						break;
					}
				}
				if (!found) {
					TSF_DTRACE_FILTER_SKIP(inp,
					    TSF_SKIP_PORT);
					TSF_STAT_LOCAL_INC(&stats_batch,
					    sockets_skipped_port);
#ifdef TCPSTATS_DEBUG
					dbg_skip_port++;
#endif
					continue;
				}
			}
			if (sc->sc_filter.flags &
			    TSF_REMOTE_PORT_MATCH) {
				uint16_t fport =
				    inp->inp_inc.inc_fport;
				int found, j;

				found = 0;
				for (j = 0; j < TSF_MAX_PORTS &&
				    sc->sc_filter.remote_ports[j] != 0;
				    j++) {
					if (fport ==
					    sc->sc_filter.remote_ports[j]) {
						found = 1;
						break;
					}
				}
				if (!found) {
					TSF_DTRACE_FILTER_SKIP(inp,
					    TSF_SKIP_PORT);
					TSF_STAT_LOCAL_INC(&stats_batch,
					    sockets_skipped_port);
#ifdef TCPSTATS_DEBUG
					dbg_skip_port++;
#endif
					continue;
				}
			}

			/* IPv4 address filtering */
			if (sc->sc_filter.flags &
			    TSF_LOCAL_ADDR_MATCH) {
				if (vflag & INP_IPV4) {
					if ((inp->inp_inc.inc_laddr.s_addr &
					    sc->sc_filter.local_mask_v4.s_addr) !=
					    (sc->sc_filter.local_addr_v4.s_addr &
					    sc->sc_filter.local_mask_v4.s_addr)) {
						TSF_DTRACE_FILTER_SKIP(inp,
						    TSF_SKIP_ADDR);
						TSF_STAT_LOCAL_INC(
						    &stats_batch,
						    sockets_skipped_addr);
#ifdef TCPSTATS_DEBUG
						dbg_skip_addr++;
#endif
						continue;
					}
				} else if (vflag & INP_IPV6) {
					if (!IN6_IS_ADDR_UNSPECIFIED(
					    &sc->sc_filter.local_addr_v6) &&
					    !tsf_match_v6_prefix(
					    &inp->inp_inc.inc6_laddr,
					    &sc->sc_filter.local_addr_v6,
					    sc->sc_filter.local_prefix_v6)) {
						TSF_DTRACE_FILTER_SKIP(inp,
						    TSF_SKIP_ADDR);
						TSF_STAT_LOCAL_INC(
						    &stats_batch,
						    sockets_skipped_addr);
#ifdef TCPSTATS_DEBUG
						dbg_skip_addr++;
#endif
						continue;
					}
				}
			}
			if (sc->sc_filter.flags &
			    TSF_REMOTE_ADDR_MATCH) {
				if (vflag & INP_IPV4) {
					if ((inp->inp_inc.inc_faddr.s_addr &
					    sc->sc_filter.remote_mask_v4.s_addr) !=
					    (sc->sc_filter.remote_addr_v4.s_addr &
					    sc->sc_filter.remote_mask_v4.s_addr)) {
						TSF_DTRACE_FILTER_SKIP(inp,
						    TSF_SKIP_ADDR);
						TSF_STAT_LOCAL_INC(
						    &stats_batch,
						    sockets_skipped_addr);
#ifdef TCPSTATS_DEBUG
						dbg_skip_addr++;
#endif
						continue;
					}
				} else if (vflag & INP_IPV6) {
					if (!IN6_IS_ADDR_UNSPECIFIED(
					    &sc->sc_filter.remote_addr_v6) &&
					    !tsf_match_v6_prefix(
					    &inp->inp_inc.inc6_faddr,
					    &sc->sc_filter.remote_addr_v6,
					    sc->sc_filter.remote_prefix_v6)) {
						TSF_DTRACE_FILTER_SKIP(inp,
						    TSF_SKIP_ADDR);
						TSF_STAT_LOCAL_INC(
						    &stats_batch,
						    sockets_skipped_addr);
#ifdef TCPSTATS_DEBUG
						dbg_skip_addr++;
#endif
						continue;
					}
				}
			}

			TSF_DTRACE_FILTER_MATCH(inp);

			bzero(&rec, sizeof(rec));
			tcpstats_fill_record(&rec, inp, tp,
			    field_mask, now);

			error = uiomove(&rec, sizeof(rec), uio);
			if (error != 0) {
				INP_RUNLOCK(inp);
				TSF_STAT_LOCAL_INC(&stats_batch,
				    uiomove_errors);
				goto out;
			}
			nrecords++;
			TSF_STAT_LOCAL_INC(&stats_batch,
			    records_emitted);
		}
	}

	error = 0;
out:
	CURVNET_RESTORE();

	sc->sc_done = 1;

	TSF_DBG("read done: visited=%d emitted=%d err=%d\n",
	    nsockets, nrecords, error);
#ifdef TCPSTATS_DEBUG
	if (dbg_skip_gencnt || dbg_skip_cred || dbg_skip_ipver ||
	    dbg_skip_state || dbg_skip_port || dbg_skip_addr)
		TSF_DBG("  skipped: gencnt=%d cred=%d ipver=%d "
		    "state=%d port=%d addr=%d\n",
		    dbg_skip_gencnt, dbg_skip_cred, dbg_skip_ipver,
		    dbg_skip_state, dbg_skip_port, dbg_skip_addr);
#endif

	/* Flush batched per-socket counters to global stats */
	TSF_STAT_FLUSH(&stats_batch);

	/* Update timing stats */
	{
		sbintime_t elapsed __unused =
		    getsbinuptime() - read_start;
		uint64_t elapsed_ns __unused =
		    (uint64_t)elapsed * 1000000000 / SBT_1S;

		TSF_STAT_ADD(read_duration_ns_total, elapsed_ns);
		TSF_STAT_MAX(read_duration_ns_max, elapsed_ns);
		TSF_DTRACE_READ_DONE(error, nrecords, elapsed_ns);
	}

	atomic_subtract_int(&tcpstats_active_readers, 1);
	return (error);
}

static int
tcpstats_ioctl(struct cdev *dev, u_long cmd, caddr_t data, int fflag,
    struct thread *td)
{
	struct tcpstats_softc *sc;
	int error;

	error = devfs_get_cdevpriv((void **)&sc);
	if (error != 0)
		return (error);

	switch (cmd) {
	case TCPSTATS_VERSION_CMD: {
		struct tcpstats_version *ver =
		    (struct tcpstats_version *)data;

		CURVNET_SET(TD_TO_VNET(td));
		ver->protocol_version = TCP_STATS_VERSION;
		ver->record_size = TCP_STATS_RECORD_SIZE;
		ver->record_count_hint = V_tcbinfo.ipi_count;
		ver->flags = 0;
		CURVNET_RESTORE();
		return (0);
	}
	case TCPSTATS_SET_FILTER: {
		struct tcpstats_filter *filt =
		    (struct tcpstats_filter *)data;

		if (filt->version != TSF_VERSION) {
			printf("tcpstats: filter version %u unsupported "
			    "(expected %u)\n",
			    filt->version, TSF_VERSION);
			return (ENOTSUP);
		}
		sc->sc_filter = *filt;
		return (0);
	}
	case TCPSTATS_RESET:
		sc->sc_done = 0;
		return (0);
	default:
		return (ENOTTY);
	}
}

static void
tcpstats_dtor(void *data)
{
	struct tcpstats_softc *sc;

	sc = data;
	crfree(sc->sc_cred);
	free(sc, M_TCPSTATS);
	atomic_subtract_int(&tcpstats_active_fds, 1);
}

static int
sysctl_handle_tcpstats_active_fds(SYSCTL_HANDLER_ARGS)
{
	u_int val;

	val = tcpstats_active_fds;
	return (sysctl_handle_int(oidp, &val, 0, req));
}

static int
tcpstats_modevent(module_t mod, int type, void *arg)
{

	switch (type) {
	case MOD_LOAD:
		sx_init(&tcpstats_profile_lock, "tcpstats_profile");

		/* Initialize sysctl tree */
		sysctl_ctx_init(&tcpstats_sysctl_ctx);
		tcpstats_sysctl_tree = SYSCTL_ADD_NODE(
		    &tcpstats_sysctl_ctx,
		    SYSCTL_STATIC_CHILDREN(_dev), OID_AUTO, "tcpstats",
		    CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
		    "TCP connection statistics device");
		SYSCTL_ADD_STRING(&tcpstats_sysctl_ctx,
		    SYSCTL_CHILDREN(tcpstats_sysctl_tree),
		    OID_AUTO, "last_error", CTLFLAG_RD,
		    tcpstats_last_error, sizeof(tcpstats_last_error),
		    "Last filter parse error");
		tcpstats_profiles_node = SYSCTL_ADD_NODE(
		    &tcpstats_sysctl_ctx,
		    SYSCTL_CHILDREN(tcpstats_sysctl_tree),
		    OID_AUTO, "profiles",
		    CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
		    "Named filter profiles");
		SYSCTL_ADD_PROC(&tcpstats_sysctl_ctx,
		    SYSCTL_CHILDREN(tcpstats_sysctl_tree),
		    OID_AUTO, "profile_set",
		    CTLTYPE_STRING | CTLFLAG_RW | CTLFLAG_MPSAFE,
		    NULL, 0,
		    tcpstats_profile_set_handler, "A",
		    "Create/update/delete filter profiles "
		    "(write: 'name filter_string' or 'name' to delete)");

		/* DoS protection tunables */
		SYSCTL_ADD_UINT(&tcpstats_sysctl_ctx,
		    SYSCTL_CHILDREN(tcpstats_sysctl_tree),
		    OID_AUTO, "max_open_fds",
		    CTLFLAG_RW, &tcpstats_max_open_fds, 0,
		    "Maximum concurrent open file descriptors "
		    "(default 16)");
		SYSCTL_ADD_UINT(&tcpstats_sysctl_ctx,
		    SYSCTL_CHILDREN(tcpstats_sysctl_tree),
		    OID_AUTO, "max_concurrent_readers",
		    CTLFLAG_RW, &tcpstats_max_concurrent_readers, 0,
		    "Maximum concurrent readers in read() "
		    "(default 32)");
		SYSCTL_ADD_UINT(&tcpstats_sysctl_ctx,
		    SYSCTL_CHILDREN(tcpstats_sysctl_tree),
		    OID_AUTO, "max_read_duration_ms",
		    CTLFLAG_RW, &tcpstats_max_read_duration_ms, 0,
		    "Maximum read() iteration time in ms, "
		    "0=unlimited (default 5000)");
		SYSCTL_ADD_UINT(&tcpstats_sysctl_ctx,
		    SYSCTL_CHILDREN(tcpstats_sysctl_tree),
		    OID_AUTO, "min_read_interval_ms",
		    CTLFLAG_RW, &tcpstats_min_read_interval_ms, 0,
		    "Minimum interval between reads per fd in ms, "
		    "0=unlimited (default 0)");

		/* Tier 1 statistics (always on) */
		SYSCTL_ADD_U64(&tcpstats_sysctl_ctx,
		    SYSCTL_CHILDREN(tcpstats_sysctl_tree),
		    OID_AUTO, "reads_total",
		    CTLFLAG_RD, &tcpstats_reads_total, 0,
		    "Total read() calls");
		SYSCTL_ADD_PROC(&tcpstats_sysctl_ctx,
		    SYSCTL_CHILDREN(tcpstats_sysctl_tree),
		    OID_AUTO, "active_fds",
		    CTLTYPE_UINT | CTLFLAG_RD | CTLFLAG_MPSAFE,
		    NULL, 0,
		    sysctl_handle_tcpstats_active_fds, "IU",
		    "Currently open file descriptors");
		SYSCTL_ADD_U64(&tcpstats_sysctl_ctx,
		    SYSCTL_CHILDREN(tcpstats_sysctl_tree),
		    OID_AUTO, "opens_total",
		    CTLFLAG_RD, &tcpstats_opens_total, 0,
		    "Total open() calls");

#ifdef TCPSTATS_STATS
		/* Tier 2 statistics (compile-time enabled) */
		SYSCTL_ADD_U64(&tcpstats_sysctl_ctx,
		    SYSCTL_CHILDREN(tcpstats_sysctl_tree),
		    OID_AUTO, "records_emitted",
		    CTLFLAG_RD, &tcpstats_stats.records_emitted, 0,
		    "Total records copied to userspace");
		SYSCTL_ADD_U64(&tcpstats_sysctl_ctx,
		    SYSCTL_CHILDREN(tcpstats_sysctl_tree),
		    OID_AUTO, "sockets_visited",
		    CTLFLAG_RD, &tcpstats_stats.sockets_visited, 0,
		    "Total inpcbs examined");
		SYSCTL_ADD_U64(&tcpstats_sysctl_ctx,
		    SYSCTL_CHILDREN(tcpstats_sysctl_tree),
		    OID_AUTO, "sockets_skipped_gencnt",
		    CTLFLAG_RD,
		    &tcpstats_stats.sockets_skipped_gencnt, 0,
		    "Skipped: generation count");
		SYSCTL_ADD_U64(&tcpstats_sysctl_ctx,
		    SYSCTL_CHILDREN(tcpstats_sysctl_tree),
		    OID_AUTO, "sockets_skipped_cred",
		    CTLFLAG_RD,
		    &tcpstats_stats.sockets_skipped_cred, 0,
		    "Skipped: credential visibility");
		SYSCTL_ADD_U64(&tcpstats_sysctl_ctx,
		    SYSCTL_CHILDREN(tcpstats_sysctl_tree),
		    OID_AUTO, "sockets_skipped_ipver",
		    CTLFLAG_RD,
		    &tcpstats_stats.sockets_skipped_ipver, 0,
		    "Skipped: IP version filter");
		SYSCTL_ADD_U64(&tcpstats_sysctl_ctx,
		    SYSCTL_CHILDREN(tcpstats_sysctl_tree),
		    OID_AUTO, "sockets_skipped_state",
		    CTLFLAG_RD,
		    &tcpstats_stats.sockets_skipped_state, 0,
		    "Skipped: state filter");
		SYSCTL_ADD_U64(&tcpstats_sysctl_ctx,
		    SYSCTL_CHILDREN(tcpstats_sysctl_tree),
		    OID_AUTO, "sockets_skipped_port",
		    CTLFLAG_RD,
		    &tcpstats_stats.sockets_skipped_port, 0,
		    "Skipped: port filter");
		SYSCTL_ADD_U64(&tcpstats_sysctl_ctx,
		    SYSCTL_CHILDREN(tcpstats_sysctl_tree),
		    OID_AUTO, "sockets_skipped_addr",
		    CTLFLAG_RD,
		    &tcpstats_stats.sockets_skipped_addr, 0,
		    "Skipped: address filter");
		SYSCTL_ADD_U64(&tcpstats_sysctl_ctx,
		    SYSCTL_CHILDREN(tcpstats_sysctl_tree),
		    OID_AUTO, "read_duration_ns_total",
		    CTLFLAG_RD,
		    &tcpstats_stats.read_duration_ns_total, 0,
		    "Cumulative read() duration in nanoseconds");
		SYSCTL_ADD_U64(&tcpstats_sysctl_ctx,
		    SYSCTL_CHILDREN(tcpstats_sysctl_tree),
		    OID_AUTO, "read_duration_ns_max",
		    CTLFLAG_RD,
		    &tcpstats_stats.read_duration_ns_max, 0,
		    "Maximum single read() duration in nanoseconds");
		SYSCTL_ADD_U64(&tcpstats_sysctl_ctx,
		    SYSCTL_CHILDREN(tcpstats_sysctl_tree),
		    OID_AUTO, "uiomove_errors",
		    CTLFLAG_RD, &tcpstats_stats.uiomove_errors, 0,
		    "uiomove() failures");
		SYSCTL_ADD_U64(&tcpstats_sysctl_ctx,
		    SYSCTL_CHILDREN(tcpstats_sysctl_tree),
		    OID_AUTO, "reads_timed_out",
		    CTLFLAG_RD, &tcpstats_stats.reads_timed_out, 0,
		    "Reads terminated by timeout");
		SYSCTL_ADD_U64(&tcpstats_sysctl_ctx,
		    SYSCTL_CHILDREN(tcpstats_sysctl_tree),
		    OID_AUTO, "reads_interrupted",
		    CTLFLAG_RD, &tcpstats_stats.reads_interrupted, 0,
		    "Reads interrupted by signal");
#endif /* TCPSTATS_STATS */

		tcpstats_dev = make_dev_credf(MAKEDEV_ETERNAL_KLD,
		    &tcpstats_cdevsw, 0, NULL, UID_ROOT, GID_NETWORK,
		    0440, "tcpstats");
		if (tcpstats_dev == NULL) {
			printf("tcpstats: make_dev_credf failed\n");
			sysctl_ctx_free(&tcpstats_sysctl_ctx);
			sx_destroy(&tcpstats_profile_lock);
			return (ENXIO);
		}
		tcpstats_full_dev = make_dev_credf(MAKEDEV_ETERNAL_KLD,
		    &tcpstats_full_cdevsw, 0, NULL, UID_ROOT,
		    GID_NETWORK, 0440, "tcpstats-full");
		if (tcpstats_full_dev == NULL) {
			destroy_dev(tcpstats_dev);
			tcpstats_dev = NULL;
			printf("tcpstats: make_dev_credf (full) failed\n");
			sysctl_ctx_free(&tcpstats_sysctl_ctx);
			sx_destroy(&tcpstats_profile_lock);
			return (ENXIO);
		}
		printf("tcpstats: loaded (TCP_STATS_VERSION=%d, "
		    "TSF_VERSION=%d)\n",
		    TCP_STATS_VERSION, TSF_VERSION);
		return (0);

	case MOD_UNLOAD:
		tcpstats_profiles_destroy_all();
		if (tcpstats_full_dev != NULL)
			destroy_dev(tcpstats_full_dev);
		if (tcpstats_dev != NULL)
			destroy_dev(tcpstats_dev);
		sysctl_ctx_free(&tcpstats_sysctl_ctx);
		sx_destroy(&tcpstats_profile_lock);
		printf("tcpstats: unloaded\n");
		return (0);

	default:
		return (EOPNOTSUPP);
	}
}

static moduledata_t tcpstats_mod = {
	.name =		"tcpstats",
	.evhand =	tcpstats_modevent,
	.priv =		NULL,
};

DECLARE_MODULE(tcpstats, tcpstats_mod, SI_SUB_DRIVERS, SI_ORDER_MIDDLE);
MODULE_VERSION(tcpstats, 1);
