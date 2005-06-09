/*-
 * Copyright (c) 2005 Joseph Koshy
 * All rights reserved.
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/pmc.h>
#include <sys/pmclog.h>

#include <assert.h>
#include <errno.h>
#include <pmc.h>
#include <pmclog.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include <machine/pmc_mdep.h>

#define	PMCLOG_BUFFER_SIZE			4096

/*
 * API NOTES
 *
 * The pmclog(3) API is oriented towards parsing an event stream in
 * "realtime", i.e., from an data source that may or may not preserve
 * record boundaries -- for example when the data source is elsewhere
 * on a network.  The API allows data to be fed into the parser zero
 * or more bytes at a time.
 *
 * The state for a log file parser is maintained in a 'struct
 * pmclog_parse_state'.  Parser invocations are done by calling
 * 'pmclog_read()'; this function will inform the caller when a
 * complete event is parsed.
 *
 * The parser first assembles a complete log file event in an internal
 * work area (see "ps_saved" below).  Once a complete log file event
 * is read, the parser then parses it and converts it to an event
 * descriptor usable by the client.  We could possibly avoid this two
 * step process by directly parsing the input log to set fields in the
 * event record.  However the parser's state machine would get
 * insanely complicated, and this code is unlikely to be used in
 * performance critical paths.
 */

enum pmclog_parser_state {
	PL_STATE_NEW_RECORD,		/* in-between records */
	PL_STATE_EXPECTING_HEADER,	/* header being read */
	PL_STATE_PARTIAL_RECORD,	/* header present but not the record */
	PL_STATE_ERROR			/* parsing error encountered */
};

struct pmclog_parse_state {
	enum pmclog_parser_state ps_state;
	enum pmc_cputype	ps_arch;	/* log file architecture */
	uint32_t		ps_version;	/* hwpmc version */
	int			ps_initialized;	/* whether initialized */
	int			ps_count;	/* count of records processed */
	off_t			ps_offset;	/* stream byte offset */
	union pmclog_entry	ps_saved;	/* saved partial log entry */
	int			ps_svcount;	/* #bytes saved */
	int			ps_fd;		/* active fd or -1 */
	char			*ps_buffer;	/* scratch buffer if fd != -1 */
	char			*ps_data;	/* current parse pointer */
	size_t			ps_len;		/* length of buffered data */
};

#define	PMCLOG_HEADER_FROM_SAVED_STATE(PS)				\
	(* ((uint32_t *) &(PS)->ps_saved))

#define	PMCLOG_INITIALIZE_READER(LE,A)	LE = (uint32_t *) &(A)
#define	PMCLOG_READ32(LE,V) 		do {				\
		(V)  = *(LE)++;						\
	} while (0)
#define	PMCLOG_READ64(LE,V)		do {				\
		uint64_t _v;						\
		_v  = (uint64_t) *(LE)++;				\
		_v |= ((uint64_t) *(LE)++) << 32;			\
		(V) = _v;						\
	} while (0)

#define	PMCLOG_READSTRING(LE,DST,LEN)	strlcpy((DST), (char *) (LE), (LEN))

/*
 * Assemble a log record from '*len' octets starting from address '*data'.
 * Update 'data' and 'len' to reflect the number of bytes consumed.
 *
 * '*data' is potentially an unaligned address and '*len' octets may
 * not be enough to complete a event record.
 */

static enum pmclog_parser_state
pmclog_get_record(struct pmclog_parse_state *ps, char **data, ssize_t *len)
{
	int avail, copylen, recordsize, used;
	uint32_t h;
	const int HEADERSIZE = sizeof(uint32_t);
	char *src, *dst;

	if ((avail = *len) <= 0)
		return (ps->ps_state = PL_STATE_ERROR);

	src = *data;
	h = used = 0;

	if (ps->ps_state == PL_STATE_NEW_RECORD)
		ps->ps_svcount = 0;

	dst = (char *) &ps->ps_saved + ps->ps_svcount;

	switch (ps->ps_state) {
	case PL_STATE_NEW_RECORD:

		/*
		 * Transitions:
		 *
		 * Case A: avail < headersize
		 *	-> 'expecting header'
		 *
		 * Case B: avail >= headersize
		 *    B.1: avail < recordsize
		 *	   -> 'partial record'
		 *    B.2: avail >= recordsize
		 *         -> 'new record'
		 */

		copylen = avail < HEADERSIZE ? avail : HEADERSIZE;
		bcopy(src, dst, copylen);
		ps->ps_svcount = used = copylen;

		if (copylen < HEADERSIZE) {
			ps->ps_state = PL_STATE_EXPECTING_HEADER;
			goto done;
		}

		src += copylen;
		dst += copylen;

		h = PMCLOG_HEADER_FROM_SAVED_STATE(ps);
		recordsize = PMCLOG_HEADER_TO_LENGTH(h);

		if (recordsize <= 0)
			goto error;

		if (recordsize <= avail) { /* full record available */
			bcopy(src, dst, recordsize - copylen);
			ps->ps_svcount = used = recordsize;
			goto done;
		}

		/* header + a partial record is available */
		bcopy(src, dst, avail - copylen);
		ps->ps_svcount = used = avail;
		ps->ps_state = PL_STATE_PARTIAL_RECORD;

		break;

	case PL_STATE_EXPECTING_HEADER:

		/*
		 * Transitions:
		 *
		 * Case C: avail+saved < headersize
		 * 	-> 'expecting header'
		 *
		 * Case D: avail+saved >= headersize
		 *    D.1: avail+saved < recordsize
		 *    	-> 'partial record'
		 *    D.2: avail+saved >= recordsize
		 *    	-> 'new record'
		 *    (see PARTIAL_RECORD handling below)
		 */

		if (avail + ps->ps_svcount < HEADERSIZE) {
			bcopy(src, dst, avail);
			ps->ps_svcount += avail;
			used = avail;
			break;
		}

		used = copylen = HEADERSIZE - ps->ps_svcount;
		bcopy(src, dst, copylen);
		src += copylen;
		dst += copylen;
		avail -= copylen;
		ps->ps_svcount += copylen;

		/*FALLTHROUGH*/

	case PL_STATE_PARTIAL_RECORD:

		/*
		 * Transitions:
		 *
		 * Case E: avail+saved < recordsize
		 * 	-> 'partial record'
		 *
		 * Case F: avail+saved >= recordsize
		 * 	-> 'new record'
		 */

		h = PMCLOG_HEADER_FROM_SAVED_STATE(ps);
		recordsize = PMCLOG_HEADER_TO_LENGTH(h);

		if (recordsize <= 0)
			goto error;

		if (avail + ps->ps_svcount < recordsize) {
			copylen = avail;
			ps->ps_state = PL_STATE_PARTIAL_RECORD;
		} else {
			copylen = recordsize - ps->ps_svcount;
			ps->ps_state = PL_STATE_NEW_RECORD;
		}

		bcopy(src, dst, copylen);
		ps->ps_svcount += copylen;
		used += copylen;
		break;

	default:
		goto error;
	}

 done:
	*data += used;
	*len  -= used;
	return ps->ps_state;

 error:
	ps->ps_state = PL_STATE_ERROR;
	return ps->ps_state;
}

/*
 * Get an event from the stream pointed to by '*data'.  '*len'
 * indicates the number of bytes available to parse.  Arguments
 * '*data' and '*len' are updated to indicate the number of bytes
 * consumed.
 */

static int
pmclog_get_event(void *cookie, char **data, ssize_t *len,
    struct pmclog_ev *ev)
{
	int evlen, pathlen;
	uint32_t h, *le;
	enum pmclog_parser_state e;
	struct pmclog_parse_state *ps;

	ps = (struct pmclog_parse_state *) cookie;

	assert(ps->ps_state != PL_STATE_ERROR);

	if ((e = pmclog_get_record(ps,data,len)) == PL_STATE_ERROR) {
		ev->pl_state = PMCLOG_ERROR;
		return -1;
	}

	if (e != PL_STATE_NEW_RECORD) {
		ev->pl_state = PMCLOG_REQUIRE_DATA;
		return -1;
	}

	PMCLOG_INITIALIZE_READER(le, ps->ps_saved);

	PMCLOG_READ32(le,h);

	if (!PMCLOG_HEADER_CHECK_MAGIC(h)) {
		ps->ps_state = PL_STATE_ERROR;
		ev->pl_state = PMCLOG_ERROR;
		return -1;
	}

	/* copy out the time stamp */
	PMCLOG_READ32(le,ev->pl_ts.tv_sec);
	PMCLOG_READ32(le,ev->pl_ts.tv_nsec);

	evlen = PMCLOG_HEADER_TO_LENGTH(h);

#define	PMCLOG_GET_PATHLEN(P,E,TYPE) do {				\
		(P) = (E) - offsetof(struct TYPE, pl_pathname);		\
		if ((P) > PATH_MAX || (P) < 0)				\
			goto error;					\
	} while (0)

	switch (ev->pl_type = PMCLOG_HEADER_TO_TYPE(h)) {
	case PMCLOG_TYPE_CLOSELOG:
	case PMCLOG_TYPE_DROPNOTIFY:
		/* nothing to do */
		break;
	case PMCLOG_TYPE_INITIALIZE:
		PMCLOG_READ32(le,ev->pl_u.pl_i.pl_version);
		PMCLOG_READ32(le,ev->pl_u.pl_i.pl_arch);
		ps->ps_version = ev->pl_u.pl_i.pl_version;
		ps->ps_arch = ev->pl_u.pl_i.pl_arch;
		ps->ps_initialized = 1;
		break;
	case PMCLOG_TYPE_MAPPINGCHANGE:
		PMCLOG_GET_PATHLEN(pathlen,evlen,pmclog_mappingchange);
		PMCLOG_READ32(le,ev->pl_u.pl_m.pl_type);
		PMCLOG_READADDR(le,ev->pl_u.pl_m.pl_start);
		PMCLOG_READADDR(le,ev->pl_u.pl_m.pl_end);
		PMCLOG_READ32(le,ev->pl_u.pl_m.pl_pid);
		PMCLOG_READSTRING(le, ev->pl_u.pl_m.pl_pathname, pathlen);
		break;
	case PMCLOG_TYPE_PCSAMPLE:
		PMCLOG_READ32(le,ev->pl_u.pl_s.pl_pid);
		PMCLOG_READADDR(le,ev->pl_u.pl_s.pl_pc);
		PMCLOG_READ32(le,ev->pl_u.pl_s.pl_pmcid);
		break;
	case PMCLOG_TYPE_PMCALLOCATE:
		PMCLOG_READ32(le,ev->pl_u.pl_a.pl_pmcid);
		PMCLOG_READ32(le,ev->pl_u.pl_a.pl_event);
		PMCLOG_READ32(le,ev->pl_u.pl_a.pl_flags);
		if ((ev->pl_u.pl_a.pl_evname =
		    pmc_name_of_event(ev->pl_u.pl_a.pl_event)) == NULL)
			goto error;
		break;
	case PMCLOG_TYPE_PMCATTACH:
		PMCLOG_GET_PATHLEN(pathlen,evlen,pmclog_pmcattach);
		PMCLOG_READ32(le,ev->pl_u.pl_t.pl_pmcid);
		PMCLOG_READ32(le,ev->pl_u.pl_t.pl_pid);
		PMCLOG_READSTRING(le,ev->pl_u.pl_t.pl_pathname,pathlen);
		break;
	case PMCLOG_TYPE_PMCDETACH:
		PMCLOG_READ32(le,ev->pl_u.pl_d.pl_pmcid);
		PMCLOG_READ32(le,ev->pl_u.pl_d.pl_pid);
		break;
	case PMCLOG_TYPE_PROCCSW:
		PMCLOG_READ32(le,ev->pl_u.pl_c.pl_pmcid);
		PMCLOG_READ64(le,ev->pl_u.pl_c.pl_value);
		PMCLOG_READ32(le,ev->pl_u.pl_c.pl_pid);
		break;
	case PMCLOG_TYPE_PROCEXEC:
		PMCLOG_GET_PATHLEN(pathlen,evlen,pmclog_procexec);
		PMCLOG_READ32(le,ev->pl_u.pl_x.pl_pid);
		PMCLOG_READSTRING(le,ev->pl_u.pl_x.pl_pathname,pathlen);
		break;
	case PMCLOG_TYPE_PROCEXIT:
		PMCLOG_READ32(le,ev->pl_u.pl_e.pl_pmcid);
		PMCLOG_READ64(le,ev->pl_u.pl_e.pl_value);
		PMCLOG_READ32(le,ev->pl_u.pl_e.pl_pid);
		break;
	case PMCLOG_TYPE_PROCFORK:
		PMCLOG_READ32(le,ev->pl_u.pl_f.pl_oldpid);
		PMCLOG_READ32(le,ev->pl_u.pl_f.pl_newpid);
		break;
	case PMCLOG_TYPE_SYSEXIT:
		PMCLOG_READ32(le,ev->pl_u.pl_se.pl_pid);
		break;
	case PMCLOG_TYPE_USERDATA:
		PMCLOG_READ32(le,ev->pl_u.pl_u.pl_userdata);
		break;
	default:	/* unknown record type */
		ps->ps_state = PL_STATE_ERROR;
		ev->pl_state = PMCLOG_ERROR;
		return -1;
	}

	ev->pl_offset = (ps->ps_offset += evlen);
	ev->pl_count  = (ps->ps_count += 1);
	ev->pl_state = PMCLOG_OK;
	return 0;

 error:
	ev->pl_state = PMCLOG_ERROR;
	ps->ps_state = PL_STATE_ERROR;
	return -1;
}

/*
 * Extract and return the next event from the byte stream.
 *
 * Returns 0 and sets the event's state to PMCLOG_OK in case an event
 * was successfully parsed.  Otherwise this function returns -1 and
 * sets the event's state to one of PMCLOG_REQUIRE_DATA (if more data
 * is needed) or PMCLOG_EOF (if an EOF was seen) or PMCLOG_ERROR if
 * a parse error was encountered.
 */

int
pmclog_read(void *cookie, struct pmclog_ev *ev)
{
	ssize_t nread;
	struct pmclog_parse_state *ps;

	ps = (struct pmclog_parse_state *) cookie;

	if (ps->ps_state == PL_STATE_ERROR) {
		ev->pl_state = PMCLOG_ERROR;
		return -1;
	}

	/*
	 * If there isn't enough data left for a new event try and get
	 * more data.
	 */
	if (ps->ps_len == 0) {
		ev->pl_state = PMCLOG_REQUIRE_DATA;

		/*
		 * If we have a valid file descriptor to read from, attempt
		 * to read from that.  This read may return with an error,
		 * (which may be EAGAIN or other recoverable error), or
		 * can return EOF.
		 */
		if (ps->ps_fd != PMCLOG_FD_NONE) {
			nread = read(ps->ps_fd, ps->ps_buffer,
			    PMCLOG_BUFFER_SIZE);

			if (nread <= 0) {
				ev->pl_state = nread < 0 ? PMCLOG_ERROR :
				    PMCLOG_EOF;
				return -1;
			}

			ps->ps_len = nread;
			ps->ps_data = ps->ps_buffer;
		} else
			return -1;
	}

	assert(ps->ps_len > 0);

	/*
	 * Retrieve one event from the byte stream.
	 */
	return pmclog_get_event(ps, &ps->ps_data, &ps->ps_len, ev);
}

/*
 * Feed data to a memory based parser.
 *
 * The memory area pointed to by 'data' needs to be valid till the
 * next error return from pmclog_next_event().
 */

int
pmclog_feed(void *cookie, char *data, int len)
{
	struct pmclog_parse_state *ps;

	ps = (struct pmclog_parse_state *) cookie;

	if (len < 0 ||		/* invalid length */
	    ps->ps_buffer ||	/* called for a file parser */
	    ps->ps_len != 0)	/* unnecessary call */
		return -1;

	ps->ps_data = data;
	ps->ps_len  = len;

	return 0;
}

/*
 * Allocate and initialize parser state.
 */

void *
pmclog_open(int fd)
{
	struct pmclog_parse_state *ps;

	if ((ps = (struct pmclog_parse_state *) malloc(sizeof(*ps))) == NULL)
		return NULL;

	ps->ps_state = PL_STATE_NEW_RECORD;
	ps->ps_arch = -1;
	ps->ps_initialized = 0;
	ps->ps_count = 0;
	ps->ps_offset = (off_t) 0;
	bzero(&ps->ps_saved, sizeof(ps->ps_saved));
	ps->ps_svcount = 0;
	ps->ps_fd    = fd;
	ps->ps_data  = NULL;
	ps->ps_buffer = NULL;
	ps->ps_len   = 0;

	/* allocate space for a work area */
	if (ps->ps_fd != PMCLOG_FD_NONE) {
		if ((ps->ps_buffer = malloc(PMCLOG_BUFFER_SIZE)) == NULL)
			return NULL;
	}

	return ps;
}


/*
 * Free up parser state.
 */

void
pmclog_close(void *cookie)
{
	struct pmclog_parse_state *ps;

	ps = (struct pmclog_parse_state *) cookie;

	if (ps->ps_buffer)
		free(ps->ps_buffer);

	free(ps);
}
