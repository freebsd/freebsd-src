/* mrtrace.c

   Subroutines that support minires tracing... */

/*
 * Copyright (c) 2001 Internet Software Consortium.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of The Internet Software Consortium nor the names
 *    of its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INTERNET SOFTWARE CONSORTIUM AND
 * CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE INTERNET SOFTWARE CONSORTIUM OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This software has been written for the Internet Software Consortium
 * by Ted Lemon, as part of a project for Nominum, Inc.   To learn more
 * about the Internet Software Consortium, see http://www.isc.org/.  To
 * learn more about Nominum, Inc., see ``http://www.nominum.com''.
 */

#include <omapip/omapip_p.h>

#include "minires/minires.h"
#include "arpa/nameser.h"

static void trace_mr_output_input (trace_type_t *, unsigned, char *);
static void trace_mr_output_stop (trace_type_t *);
static void trace_mr_input_input (trace_type_t *, unsigned, char *);
static void trace_mr_input_stop (trace_type_t *);
static void trace_mr_statp_input (trace_type_t *, unsigned, char *);
static void trace_mr_statp_stop (trace_type_t *);
static void trace_mr_randomid_input (trace_type_t *, unsigned, char *);
static void trace_mr_randomid_stop (trace_type_t *);
trace_type_t *trace_mr_output;
trace_type_t *trace_mr_input;
trace_type_t *trace_mr_statp;
trace_type_t *trace_mr_randomid;
ssize_t trace_mr_send (int, void *, size_t, int);
ssize_t trace_mr_read_playback (struct sockaddr_in *, void *, size_t);
void trace_mr_read_record (struct sockaddr_in *, void *, ssize_t);
ssize_t trace_mr_recvfrom (int s, void *, size_t, int,
			   struct sockaddr *, SOCKLEN_T *);
ssize_t trace_mr_read (int, void *, size_t);
int trace_mr_connect (int s, struct sockaddr *, SOCKLEN_T);
int trace_mr_socket (int, int, int);
int trace_mr_bind (int, struct sockaddr *, SOCKLEN_T);
int trace_mr_close (int);
time_t trace_mr_time (time_t *);
int trace_mr_select (int, fd_set *, fd_set *, fd_set *, struct timeval *);
unsigned int trace_mr_res_randomid (unsigned int);

extern time_t cur_time;

#if defined (TRACING)
void trace_mr_init ()
{
	trace_mr_output = trace_type_register ("mr-output", (void *)0,
					       trace_mr_output_input,
					       trace_mr_output_stop, MDL);
	trace_mr_input = trace_type_register ("mr-input", (void *)0,
					      trace_mr_input_input,
					      trace_mr_input_stop, MDL);
	trace_mr_statp = trace_type_register ("mr-statp", (void *)0,
					      trace_mr_statp_input,
					      trace_mr_statp_stop, MDL);
	trace_mr_randomid = trace_type_register ("mr-randomid", (void *)0,
						 trace_mr_randomid_input,
						 trace_mr_randomid_stop, MDL);
}

void trace_mr_statp_setup (res_state statp)
{
	unsigned buflen = 0;
	char *buf = (char *)0;
	isc_result_t status;
	u_int32_t id;
	int i;

	if (trace_playback ()) {
		int nscount;
		status = trace_get_packet (&trace_mr_statp, &buflen, &buf);
		if (status != ISC_R_SUCCESS) {
			log_error ("trace_mr_statp: no statp packet found.");
			return;
		}
		nscount = buflen / sizeof (struct in_addr);
		if (nscount * (sizeof (struct in_addr)) != buflen ||
		    nscount < 1) {
			log_error ("trace_mr_statp: bogus length: %d",
				   buflen);
			return;
		}
		if (nscount > MAXNS)
			nscount = MAXNS;
		for (i = 0; i < nscount; i++) {
#if defined (HAVE_SA_LEN)
			statp -> nsaddr_list [i].sin_len =
				sizeof (struct sockaddr_in);
#endif
			memset (&statp -> nsaddr_list [i].sin_zero, 0,
				sizeof statp -> nsaddr_list [i].sin_zero);
			statp -> nsaddr_list [i].sin_port = htons (53); /*XXX*/
			statp -> nsaddr_list [i].sin_family = AF_INET;
			memcpy (&statp -> nsaddr_list [i].sin_addr,
				(buf + i * (sizeof (struct in_addr))),
				sizeof (struct in_addr));
		}
		statp -> nscount = nscount;
		dfree (buf, MDL);
		buf = (char *)0;
	}
	if (trace_record ()) {
		trace_iov_t *iov;
		iov = dmalloc ((statp -> nscount *
				sizeof (trace_iov_t)), MDL);
		if (!iov) {
			trace_stop ();
			log_error ("No memory for statp iov.");
			return;
		}
		for (i = 0; i < statp -> nscount; i++) {
			iov [i].buf =
				(char *)&statp -> nsaddr_list [i].sin_addr;
			iov [i].len = sizeof (struct in_addr);
		}
		trace_write_packet_iov (trace_mr_statp, i, iov, MDL);
		dfree (iov, MDL);
	}
}
#endif

ssize_t trace_mr_send (int fd, void *msg, size_t len, int flags)
{
	ssize_t rv;
#if defined (TRACING)
	isc_result_t status;
	unsigned buflen = 0;
	char *inbuf = (char *)0;
	u_int32_t result;
	u_int32_t sflags;

	if (trace_playback()) {
		status = trace_get_packet (&trace_mr_output, &buflen, &inbuf);
		if (status != ISC_R_SUCCESS) {
			log_error ("trace_mr_recvfrom: no input found.");
			errno = ECONNREFUSED;
			return -1;
		}
		if (buflen < sizeof result) {
			log_error ("trace_mr_recvfrom: data too short.");
			errno = ECONNREFUSED;
			dfree (inbuf, MDL);
			return -1;
		}
		memcpy (&result, inbuf, sizeof result);
		rv = ntohl (result);
		dfree (inbuf, MDL);
	} else
#endif
		rv = send (fd, msg, len, flags);
#if defined (TRACING)
	if (trace_record ()) {
		trace_iov_t iov [3];
		result = htonl (rv);
		sflags = htonl (flags);
		iov [0].len = sizeof result;
		iov [0].buf = (char *)&result;
		iov [1].len = sizeof sflags;
		iov [1].buf = (char *)&flags;
		iov [2].len = len;
		iov [2].buf = msg;
		trace_write_packet_iov (trace_mr_output, 2, iov, MDL);
	}
#endif
	return rv;
}

#if defined (TRACING)
ssize_t trace_mr_read_playback (struct sockaddr_in *from,
				void *buf, size_t nbytes)
{
	isc_result_t status;
	unsigned buflen = 0, left;
	char *inbuf = (char *)0;
	char *bufp;
	u_int32_t result;

	status = trace_get_packet (&trace_mr_input, &buflen, &inbuf);
	if (status != ISC_R_SUCCESS) {
		log_error ("trace_mr_recvfrom: no input found.");
		errno = ECONNREFUSED;
		return -1;
	}
	if (buflen < sizeof result) {
		log_error ("trace_mr_recvfrom: data too short.");
		errno = ECONNREFUSED;
		dfree (inbuf, MDL);
		return -1;
	}
	bufp = inbuf;
	left = buflen;
	memcpy (&result, bufp, sizeof result);
	result = ntohl (result);
	bufp += sizeof result;
	left -= sizeof result;
	if (result == 0) {
		if (left < ((sizeof from -> sin_port) +
			    sizeof (from -> sin_addr))) {
			log_error ("trace_mr_recvfrom: data too short.");
			errno = ECONNREFUSED;
			dfree (inbuf, MDL);
			return -1;
		}
		if (from)
			memcpy (&from -> sin_addr, bufp,
				sizeof from -> sin_addr);
		bufp += sizeof from -> sin_addr;
		left -= sizeof from -> sin_addr;
		if (from)
			memcpy (&from -> sin_port, bufp,
				sizeof from -> sin_port);
		bufp += sizeof from -> sin_port;
		left -= sizeof from -> sin_port;
		if (from) {
			from -> sin_family = AF_INET;
#if defined(HAVE_SA_LEN)
			from -> sin_len = sizeof (struct sockaddr_in);
#endif
			memset (from -> sin_zero, 0, sizeof from -> sin_zero);
		}
		if (left > nbytes) {
			log_error ("trace_mr_recvfrom: too much%s",
				   " data.");
			errno = ECONNREFUSED;
			dfree (inbuf, MDL);
			return -1;
		}
		memcpy (buf, bufp, left);
		dfree (inbuf, MDL);
		return left;
	}
	errno = ECONNREFUSED;
	return -1;
}

void trace_mr_read_record (struct sockaddr_in *from, void *buf, ssize_t rv)
{
	trace_iov_t iov [4];
	u_int32_t result;
	int iolen = 0;
	static char zero [4] = { 0, 0, 0, 0 };
	
	if (rv < 0)
		result = htonl (errno);		/* XXX */
	else
		result = 0;
	iov [iolen].buf = (char *)&result;
	iov [iolen++].len = sizeof result;
	if (rv > 0) {
		if (from) {
			iov [iolen].buf = (char *)&from -> sin_addr;
			iov [iolen++].len = sizeof from -> sin_addr;
			iov [iolen].buf = (char *)&from -> sin_port;
			iov [iolen++].len = sizeof from -> sin_port;
		} else {
			iov [iolen].buf = zero;
			iov [iolen++].len = sizeof from -> sin_addr;
			iov [iolen].buf = zero;
			iov [iolen++].len = sizeof from -> sin_port;
		}

		iov [iolen].buf = buf;
		iov [iolen++].len = rv;
	}
	trace_write_packet_iov (trace_mr_input, iolen, iov, MDL);
}
#endif

ssize_t trace_mr_recvfrom (int s, void *buf, size_t len, int flags,
			   struct sockaddr *from, SOCKLEN_T *fromlen)
{
	ssize_t rv;

#if defined (TRACING)
	if (trace_playback ())
		rv = trace_mr_read_playback ((struct sockaddr_in *)from,
					     buf, len);
	else
#endif
		rv = recvfrom (s, buf, len, flags, from, fromlen);
#if defined (TRACING)
	if (trace_record ()) {
		trace_mr_read_record ((struct sockaddr_in *)from, buf, rv);
	}
#endif
	return rv;
}

ssize_t trace_mr_read (int d, void *buf, size_t nbytes)
{
	ssize_t rv;

#if defined (TRACING)
	if (trace_playback ())
		rv = trace_mr_read_playback ((struct sockaddr_in *)0,
					     buf, nbytes);
	else
#endif
		rv = read (d, buf, nbytes);
#if defined (TRACING)
	if (trace_record ()) {
		trace_mr_read_record ((struct sockaddr_in *)0, buf, rv);
	}
#endif
	return rv;
}

int trace_mr_connect (int s, struct sockaddr *name, SOCKLEN_T namelen)
{
#if defined (TRACING)
	if (!trace_playback ())
#endif
		return connect (s, name, namelen);
#if defined (TRACING)
	return 0;
#endif
}

int trace_mr_socket (int domain, int type, int protocol)
{
#if defined (TRACING)
	if (!trace_playback ())
#endif
		return socket (domain, type, protocol);
#if defined (TRACING)
	return 100;
#endif
}

int trace_mr_bind (int s, struct sockaddr *name, SOCKLEN_T namelen)
{
#if defined (TRACING)
	if (!trace_playback ())
#endif
		return bind (s, name, namelen);
#if defined (TRACING)
	return 0;
#endif
}

int trace_mr_close (int s)
{
#if defined (TRACING)
	if (!trace_playback ())
#endif
		return close (s);
#if defined (TRACING)
	return 0;
#endif
}

time_t trace_mr_time (time_t *tp)
{
#if defined (TRACING)
	if (trace_playback ()) {
		if (tp)
			*tp = cur_time;
		return cur_time;
	}
#endif
	return time (tp);
}

int trace_mr_select (int s, fd_set *r, fd_set *w, fd_set *x, struct timeval *t)
{
#if defined (TRACING)
	trace_type_t *ttp = (trace_type_t *)0;

	if (trace_playback ()) {
		time_t nct = trace_snoop_time (&ttp);
		time_t secr = t -> tv_sec;
		t -> tv_sec = nct - cur_time;
		if (t -> tv_sec > secr)
			return 0;
		if (ttp == trace_mr_input)
			return 1;
		return 0;
	}
#endif
	return select (s, r, w, x, t);
}

unsigned int trace_mr_res_randomid (unsigned int oldid)
{
	u_int32_t id;
	int rid = oldid;
#if defined (TRACING)
	unsigned buflen = 0;
	char *buf = (char *)0;
	isc_result_t status;

	if (trace_playback ()) {
		int nscount;
		status = trace_get_packet (&trace_mr_randomid, &buflen, &buf);
		if (status != ISC_R_SUCCESS) {
			log_error ("trace_mr_statp: no statp packet found.");
			return oldid;
		}
		if (buflen != sizeof id) {
			log_error ("trace_mr_randomid: bogus length: %d",
				   buflen);
			return oldid;
		}
		memcpy (&id, buf, sizeof id);
		dfree (buf, MDL);
		buf = (char *)0;
		rid = ntohl (id);
	}
	if (trace_record ()) {
		id = htonl (rid);
		trace_write_packet (trace_mr_randomid,
				    sizeof id, (char *)&id, MDL);
	}
#endif
	return rid;
}

#if defined (TRACING)
static void trace_mr_output_input (trace_type_t *ttype,
				   unsigned length, char *buf)
{
}

static void trace_mr_output_stop (trace_type_t *ttype)
{
}

static void trace_mr_input_input (trace_type_t *ttype,
				  unsigned length, char *buf)
{
	log_error ("unaccounted-for minires input.");
}

static void trace_mr_input_stop (trace_type_t *ttype)
{
}

static void trace_mr_statp_input (trace_type_t *ttype,
				  unsigned length, char *buf)
{
	log_error ("unaccounted-for minires statp input.");
}

static void trace_mr_statp_stop (trace_type_t *ttype)
{
}

static void trace_mr_randomid_input (trace_type_t *ttype,
				     unsigned length, char *buf)
{
	log_error ("unaccounted-for minires randomid input.");
}

static void trace_mr_randomid_stop (trace_type_t *ttype)
{
}
#endif
