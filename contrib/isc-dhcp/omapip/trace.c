/* trace.c

   Subroutines that support tracing of OMAPI wire transactions and
   provide a mechanism for programs using OMAPI to trace their own
   transactions... */

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

#if defined (TRACING)
void (*trace_set_time_hook) (u_int32_t);
static int tracing_stopped;
static int traceoutfile;
static int traceindex;
static trace_type_t **trace_types;
static int trace_type_count;
static int trace_type_max;
static trace_type_t *new_trace_types;
static FILE *traceinfile;
static tracefile_header_t tracefile_header;
static int trace_playback_flag;
trace_type_t trace_time_marker;

#if defined (DEBUG_MEMORY_LEAKAGE) || defined (DEBUG_MEMORY_LEAKAGE_ON_EXIT)
extern omapi_array_t *trace_listeners;
extern omapi_array_t *omapi_connections;

void trace_free_all ()
{
	trace_type_t *tp;
	int i;
	tp = new_trace_types;
	while (tp) {
		new_trace_types = tp -> next;
		if (tp -> name) {
			dfree (tp -> name, MDL);
			tp -> name = (char *)0;
		}
		dfree (tp, MDL);
		tp = new_trace_types;
	}
	for (i = 0; i < trace_type_count; i++) {
		if (trace_types [i]) {
			if (trace_types [i] -> name)
				dfree (trace_types [i] -> name, MDL);
			dfree (trace_types [i], MDL);
		}
	}
	dfree (trace_types, MDL);
	trace_types = (trace_type_t **)0;
	trace_type_count = trace_type_max = 0;

	omapi_array_free (&trace_listeners, MDL);
	omapi_array_free (&omapi_connections, MDL);
}
#endif

static isc_result_t trace_type_record (trace_type_t *,
				       unsigned, const char *, int);

int trace_playback ()
{
	return trace_playback_flag;
}

int trace_record ()
{
	if (traceoutfile && !tracing_stopped)
		return 1;
	return 0;
}

isc_result_t trace_init (void (*set_time) (u_int32_t),
			 const char *file, int line)
{
	trace_type_t *root_type;
	static int root_setup = 0;

	if (root_setup)
		return ISC_R_SUCCESS;

	trace_set_time_hook = set_time;

	root_type = trace_type_register ("trace-index-mapping",
					 (void *)0, trace_index_map_input,
					 trace_index_stop_tracing, file, line);
	if (!root_type)
		return ISC_R_UNEXPECTED;
	if (new_trace_types == root_type)
		new_trace_types = new_trace_types -> next;
	root_type -> index = 0;
	trace_type_stash (root_type);

	root_setup = 1;
	return ISC_R_SUCCESS;
}

isc_result_t trace_begin (const char *filename,
			  const char *file, int line)
{
	tracefile_header_t tfh;
	int status;
	trace_type_t *tptr, *next;
	isc_result_t result;

	if (traceoutfile) {
		log_error ("%s(%d): trace_begin called twice",
			   file, line);
		return ISC_R_INVALIDARG;
	}

	traceoutfile = open (filename, O_CREAT | O_WRONLY | O_EXCL, 0644);
	if (traceoutfile < 0) {
		log_error ("%s(%d): trace_begin: %s: %m",
			   file, line, filename);
		return ISC_R_UNEXPECTED;
	}
#if defined (HAVE_SETFD)
	if (fcntl (traceoutfile, F_SETFD, 1) < 0)
		log_error ("Can't set close-on-exec on %s: %m", filename);
#endif

	tfh.magic = htonl (TRACEFILE_MAGIC);
	tfh.version = htonl (TRACEFILE_VERSION);
	tfh.hlen = htonl (sizeof (tracefile_header_t));
	tfh.phlen = htonl (sizeof (tracepacket_t));
	
	status = write (traceoutfile, &tfh, sizeof tfh);
	if (status < 0) {
		log_error ("%s(%d): trace_begin write failed: %m", file, line);
		return ISC_R_UNEXPECTED;
	} else if (status != sizeof tfh) {
		log_error ("%s(%d): trace_begin: short write (%d:%ld)",
			   file, line, status, (long)(sizeof tfh));
		trace_stop ();
		return ISC_R_UNEXPECTED;
	}

	/* Stash all the types that have already been set up. */
	if (new_trace_types) {
		next = new_trace_types;
		new_trace_types = (trace_type_t *)0;
		for (tptr = next; tptr; tptr = next) {
			next = tptr -> next;
			if (tptr -> index != 0) {
				result = (trace_type_record
					  (tptr,
					   strlen (tptr -> name), file, line));
				if (result != ISC_R_SUCCESS)
					return status;
			}
		}
	}
	
	return ISC_R_SUCCESS;
}

isc_result_t trace_write_packet (trace_type_t *ttype, unsigned length,
				 const char *buf, const char *file, int line)
{
	trace_iov_t iov;

	iov.buf = buf;
	iov.len = length;
	return trace_write_packet_iov (ttype, 1, &iov, file, line);
}

isc_result_t trace_write_packet_iov (trace_type_t *ttype,
				     int count, trace_iov_t *iov,
				     const char *file, int line)
{
	tracepacket_t tmp;
	int status;
	int i;
	int length;

	/* Really shouldn't get called here, but it may be hard to turn off
	   tracing midstream if the trace file write fails or something. */
	if (tracing_stopped)
		return 0;

	if (!ttype) {
		log_error ("%s(%d): trace_write_packet with null trace type",
			   file ? file : "<unknown file>", line);
		return ISC_R_INVALIDARG;
	}
	if (!traceoutfile) {
		log_error ("%s(%d): trace_write_packet with no tracefile.",
			   file ? file : "<unknown file>", line);
		return ISC_R_INVALIDARG;
	}
	
	/* Compute the total length of the iov. */
	length = 0;
	for (i = 0; i < count; i++)
		length += iov [i].len;

	/* We have to swap out the data, because it may be read back on a
	   machine of different endianness. */
	tmp.type_index = htonl (ttype -> index);
	tmp.when = htonl (time ((time_t *)0)); /* XXX */
	tmp.length = htonl (length);

	status = write (traceoutfile, &tmp, sizeof tmp);
	if (status < 0) {
		log_error ("%s(%d): trace_write_packet write failed: %m",
			   file, line);
		return ISC_R_UNEXPECTED;
	} else if (status != sizeof tmp) {
		log_error ("%s(%d): trace_write_packet: short write (%d:%ld)",
			   file, line, status, (long)(sizeof tmp));
		trace_stop ();
	}

	for (i = 0; i < count; i++) {
		status = write (traceoutfile, iov [i].buf, iov [i].len);
		if (status < 0) {
			log_error ("%s(%d): %s write failed: %m",
				   file, line, "trace_write_packet");
			return ISC_R_UNEXPECTED;
		} else if (status != iov [i].len) {
			log_error ("%s(%d): %s: short write (%d:%d)",
				   file, line,
				   "trace_write_packet", status, length);
			trace_stop ();
		}
	}

	/* Write padding on the end of the packet to align the next
	   packet to an 8-byte boundary.   This is in case we decide to
	   use mmap in some clever way later on. */
	if (length % 8) {
	    static char zero [] = { 0, 0, 0, 0, 0, 0, 0 };
	    unsigned padl = 8 - (length % 8);
		
	    status = write (traceoutfile, zero, padl);
	    if (status < 0) {
		log_error ("%s(%d): trace_write_packet write failed: %m",
			   file, line);
		return ISC_R_UNEXPECTED;
	    } else if (status != padl) {
		log_error ("%s(%d): trace_write_packet: short write (%d:%d)",
			   file, line, status, padl);
		trace_stop ();
	    }
	}

	return ISC_R_SUCCESS;
}

void trace_type_stash (trace_type_t *tptr)
{
	trace_type_t **vec;
	int delta;
	if (trace_type_max <= tptr -> index) {
		delta = tptr -> index - trace_type_max + 10;
		vec = dmalloc (((trace_type_max + delta) *
				sizeof (trace_type_t *)), MDL);
		if (!vec)
			return;
		memset (&vec [trace_type_max], 0,
			(sizeof (trace_type_t *)) * delta);
		trace_type_max += delta;
		if (trace_types) {
		    memcpy (vec, trace_types,
			    trace_type_count * sizeof (trace_type_t *));
		    dfree (trace_types, MDL);
		}
		trace_types = vec;
	}
	trace_types [tptr -> index] = tptr;
	if (tptr -> index >= trace_type_count)
		trace_type_count = tptr -> index + 1;
}

trace_type_t *trace_type_register (const char *name,
				   void *baggage,
				   void (*have_packet) (trace_type_t *,
							unsigned, char *),
				   void (*stop_tracing) (trace_type_t *),
				   const char *file, int line)
{
	trace_type_t *ttmp, *tptr;
	unsigned slen = strlen (name);
	isc_result_t status;

	ttmp = dmalloc (sizeof *ttmp, file, line);
	if (!ttmp)
		return ttmp;
	ttmp -> index = -1;
	ttmp -> name = dmalloc (slen + 1, file, line);
	if (!ttmp -> name) {
		dfree (ttmp, file, line);
		return (trace_type_t *)0;
	}
	strcpy (ttmp -> name, name);
	ttmp -> have_packet = have_packet;
	ttmp -> stop_tracing = stop_tracing;
	
	if (traceoutfile) {
		status = trace_type_record (ttmp, slen, file, line);
		if (status != ISC_R_SUCCESS) {
			dfree (ttmp -> name, file, line);
			dfree (ttmp, file, line);
			return (trace_type_t *)0;
		}
	} else {
		ttmp -> next = new_trace_types;
		new_trace_types = ttmp;
	}

	return ttmp;
}
						   
static isc_result_t trace_type_record (trace_type_t *ttmp, unsigned slen,
				       const char *file, int line)
{
	trace_index_mapping_t *tim;
	isc_result_t status;

	tim = dmalloc (slen + TRACE_INDEX_MAPPING_SIZE, file, line);
	if (!tim)
		return ISC_R_NOMEMORY;
	ttmp -> index = ++traceindex;
	trace_type_stash (ttmp);
	tim -> index = htonl (ttmp -> index);
	memcpy (tim -> name, ttmp -> name, slen);
	status = trace_write_packet (trace_types [0],
				     slen + TRACE_INDEX_MAPPING_SIZE,
				     (char *)tim, file, line);
	dfree (tim, file, line);
	return status;
}

/* Stop all registered trace types from trying to trace. */

void trace_stop (void)
{
	int i;

	for (i = 0; i < trace_type_count; i++)
		if (trace_types [i] -> stop_tracing)
			(*(trace_types [i] -> stop_tracing))
				(trace_types [i]);
	tracing_stopped = 1;
}

void trace_index_map_input (trace_type_t *ttype, unsigned length, char *buf)
{
	trace_index_mapping_t *tmap;
	unsigned len;
	trace_type_t *tptr, **prev;

	if (length < TRACE_INDEX_MAPPING_SIZE) {
		log_error ("short trace index mapping");
		return;
	}
	tmap = (trace_index_mapping_t *)buf;

	prev = &new_trace_types;
	for (tptr = new_trace_types; tptr; tptr = tptr -> next) {
		len = strlen (tptr -> name);
		if (len == length - TRACE_INDEX_MAPPING_SIZE &&
		    !memcmp (tptr -> name, tmap -> name, len)) {
			tptr -> index = ntohl (tmap -> index);
			trace_type_stash (tptr);
			*prev = tptr -> next;
			return;
		}
		prev = &tptr -> next;
	}
	
	log_error ("No registered trace type for type name %.*s",
		   (int)length - TRACE_INDEX_MAPPING_SIZE, tmap -> name);
	return;
}

void trace_index_stop_tracing (trace_type_t *ttype) { }

void trace_replay_init (void)
{
	trace_playback_flag = 1;
}

void trace_file_replay (const char *filename)
{
	tracepacket_t *tpkt = (tracepacket_t *)0;
	int status;
	char *buf = (char *)0;
	unsigned buflen;
	unsigned bufmax = 0;
	trace_type_t *ttype = (trace_type_t *)0;
	isc_result_t result;
	int len;

	traceinfile = fopen (filename, "r");
	if (!traceinfile) {
		log_error ("Can't open tracefile %s: %m", filename);
		return;
	}
#if defined (HAVE_SETFD)
	if (fcntl (fileno (traceinfile), F_SETFD, 1) < 0)
		log_error ("Can't set close-on-exec on %s: %m", filename);
#endif
	status = fread (&tracefile_header, 1,
			sizeof tracefile_header, traceinfile);
	if (status < sizeof tracefile_header) {
		if (ferror (traceinfile))
			log_error ("Error reading trace file header: %m");
		else
			log_error ("Short read on trace file header: %d %ld.",
				   status, (long)(sizeof tracefile_header));
		goto out;
	}
	tracefile_header.magic = ntohl (tracefile_header.magic);
	tracefile_header.version = ntohl (tracefile_header.version);
	tracefile_header.hlen = ntohl (tracefile_header.hlen);
	tracefile_header.phlen = ntohl (tracefile_header.phlen);

	if (tracefile_header.magic != TRACEFILE_MAGIC) {
		log_error ("%s: not a dhcp trace file.", filename);
		goto out;
	}
	if (tracefile_header.version > TRACEFILE_VERSION) {
		log_error ("tracefile version %ld > current %ld.",
			   (long int)tracefile_header.version,
			   (long int)TRACEFILE_VERSION);
		goto out;
	}
	if (tracefile_header.phlen < sizeof *tpkt) {
		log_error ("tracefile packet size too small - %ld < %ld",
			   (long int)tracefile_header.phlen,
			   (long int)sizeof *tpkt);
		goto out;
	}
	len = (sizeof tracefile_header) - tracefile_header.hlen;
	if (len < 0) {
		log_error ("tracefile header size too small - %ld < %ld",
			   (long int)tracefile_header.hlen,
			   (long int)sizeof tracefile_header);
		goto out;
	}
	if (len > 0) {
		status = fseek (traceinfile, (long)len, SEEK_CUR);
		if (status < 0) {
			log_error ("can't seek past header: %m");
			goto out;
		}
	}

	tpkt = dmalloc ((unsigned)tracefile_header.phlen, MDL);
	if (!tpkt) {
		log_error ("can't allocate trace packet header.");
		goto out;
	}

	while ((result = trace_get_next_packet (&ttype, tpkt, &buf, &buflen,
						&bufmax)) == ISC_R_SUCCESS) {
	    (*ttype -> have_packet) (ttype, tpkt -> length, buf);
	    ttype = (trace_type_t *)0;
	}
      out:
	fclose (traceinfile);
	if (buf)
		dfree (buf, MDL);
	if (tpkt)
		dfree (tpkt, MDL);
}

/* Get the next packet from the file.   If ttp points to a nonzero pointer
   to a trace type structure, check the next packet to see if it's of the
   expected type, and back off if not. */

isc_result_t trace_get_next_packet (trace_type_t **ttp,
				    tracepacket_t *tpkt,
				    char **buf, unsigned *buflen,
				    unsigned *bufmax)
{
	trace_type_t *ttype;
	unsigned paylen;
	int status;
	int len;
	fpos_t curpos;

	status = fgetpos (traceinfile, &curpos);
	if (status < 0)
		log_error ("Can't save tracefile position: %m");

	status = fread (tpkt, 1, (size_t)tracefile_header.phlen, traceinfile);
	if (status < tracefile_header.phlen) {
		if (ferror (traceinfile))
			log_error ("Error reading trace packet header: %m");
		else if (status == 0)
			return ISC_R_EOF;
		else
			log_error ("Short read on trace packet header: "
				   "%ld %ld.",
				   (long int)status,
				   (long int)tracefile_header.phlen);
		return ISC_R_PROTOCOLERROR;
	}

	/* Swap the packet. */
	tpkt -> type_index = ntohl (tpkt -> type_index);
	tpkt -> length = ntohl (tpkt -> length);
	tpkt -> when = ntohl (tpkt -> when);
	
	/* See if there's a handler for this packet type. */
	if (tpkt -> type_index < trace_type_count &&
	    trace_types [tpkt -> type_index])
		ttype = trace_types [tpkt -> type_index];
	else {
		log_error ("Trace packet with unknown index %ld",
			   (long int)tpkt -> type_index);
		return ISC_R_PROTOCOLERROR;
	}

	/* If we were just hunting for the time marker, we've found it,
	   so back up to the beginning of the packet and return its
	   type. */
	if (ttp && *ttp == &trace_time_marker) {
		*ttp = ttype;
		status = fsetpos (traceinfile, &curpos);
		if (status < 0) {
			log_error ("fsetpos in tracefile failed: %m");
			return ISC_R_PROTOCOLERROR;
		}
		return ISC_R_EXISTS;
	}

	/* If we were supposed to get a particular kind of packet,
	   check to see that we got the right kind. */
	if (ttp && *ttp && ttype != *ttp) {
		log_error ("Read packet type %s when expecting %s",
			   ttype -> name, (*ttp) -> name);
		status = fsetpos (traceinfile, &curpos);
		if (status < 0) {
			log_error ("fsetpos in tracefile failed: %m");
			return ISC_R_PROTOCOLERROR;
		}
		return ISC_R_UNEXPECTEDTOKEN;
	}

	paylen = tpkt -> length;
	if (paylen % 8)
		paylen += 8 - (tpkt -> length % 8);
	if (paylen > (*bufmax)) {
		if ((*buf))
			dfree ((*buf), MDL);
		(*bufmax) = ((paylen + 1023) & ~1023U);
		(*buf) = dmalloc ((*bufmax), MDL);
		if (!(*buf)) {
			log_error ("Can't allocate input buffer sized %d",
				   (*bufmax));
			return ISC_R_NOMEMORY;
		}
	}
	
	status = fread ((*buf), 1, paylen, traceinfile);
	if (status < paylen) {
		if (ferror (traceinfile))
			log_error ("Error reading trace payload: %m");
		else
			log_error ("Short read on trace payload: %d %d.",
				   status, paylen);
		return ISC_R_PROTOCOLERROR;
	}

	/* Store the actual length of the payload. */
	*buflen = tpkt -> length;

	if (trace_set_time_hook)
		(*trace_set_time_hook) (tpkt -> when);

	if (ttp)
		*ttp = ttype;
	return ISC_R_SUCCESS;
}

isc_result_t trace_get_packet (trace_type_t **ttp,
			       unsigned *buflen, char **buf)
{
	tracepacket_t *tpkt;
	unsigned bufmax = 0;
	isc_result_t status;

	if (!buf || *buf)
		return ISC_R_INVALIDARG;

	tpkt = dmalloc ((unsigned)tracefile_header.phlen, MDL);
	if (!tpkt) {
		log_error ("can't allocate trace packet header.");
		return ISC_R_NOMEMORY;
	}

	status = trace_get_next_packet (ttp, tpkt, buf, buflen, &bufmax);

	dfree (tpkt, MDL);
	return status;
}

time_t trace_snoop_time (trace_type_t **ptp)
{
	tracepacket_t *tpkt;
	unsigned bufmax = 0;
	unsigned buflen = 0;
	char *buf = (char *)0;
	isc_result_t status;
	time_t result;
	trace_type_t *ttp;
	
	if (!ptp)
		ptp = &ttp;

	tpkt = dmalloc ((unsigned)tracefile_header.phlen, MDL);
	if (!tpkt) {
		log_error ("can't allocate trace packet header.");
		return ISC_R_NOMEMORY;
	}

	*ptp = &trace_time_marker;
	trace_get_next_packet (ptp, tpkt, &buf, &buflen, &bufmax);
	result = tpkt -> when;

	dfree (tpkt, MDL);
	return result;
}

/* Get a packet from the trace input file that contains a file with the
   specified name.   We don't hunt for the packet - it should be the next
   packet in the tracefile.   If it's not, or something else bad happens,
   return an error code. */

isc_result_t trace_get_file (trace_type_t *ttype,
			     const char *filename, unsigned *len, char **buf)
{
	fpos_t curpos;
	unsigned max = 0;
	tracepacket_t *tpkt;
	int status;
	isc_result_t result;

	/* Disallow some obvious bogosities. */
	if (!buf || !len || *buf)
		return ISC_R_INVALIDARG;

	/* Save file position in case of filename mismatch. */
	status = fgetpos (traceinfile, &curpos);
	if (status < 0)
		log_error ("Can't save tracefile position: %m");

	tpkt = dmalloc ((unsigned)tracefile_header.phlen, MDL);
	if (!tpkt) {
		log_error ("can't allocate trace packet header.");
		return ISC_R_NOMEMORY;
	}

	result = trace_get_next_packet (&ttype, tpkt, buf, len, &max);
	if (result != ISC_R_SUCCESS) {
		dfree (tpkt, MDL);
		if (*buf)
			dfree (*buf, MDL);
		return result;
	}

	/* Make sure the filename is right. */
	if (strcmp (filename, *buf)) {
		log_error ("Read file %s when expecting %s", *buf, filename);
		status = fsetpos (traceinfile, &curpos);
		if (status < 0) {
			log_error ("fsetpos in tracefile failed: %m");
			dfree (tpkt, MDL);
			dfree (*buf, MDL);
			return ISC_R_PROTOCOLERROR;
		}
		return ISC_R_UNEXPECTEDTOKEN;
	}

	dfree (tpkt, MDL);
	return ISC_R_SUCCESS;
}
#endif /* TRACING */
