/* trace.h

   Definitions for omapi tracing facility... */

/*
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 2001-2003 by Internet Software Consortium
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 *   Internet Systems Consortium, Inc.
 *   950 Charter Street
 *   Redwood City, CA 94063
 *   <info@isc.org>
 *   http://www.isc.org/
 *
 * This software has been written for Internet Systems Consortium
 * by Ted Lemon, as part of a project for Nominum, Inc.   To learn more
 * about Internet Systems Consortium, see http://www.isc.org/.  To
 * learn more about Nominum, Inc., see ``http://www.nominum.com''.
 */

#define TRACEFILE_MAGIC		0x64484370UL	/* dHCp */
#define TRACEFILE_VERSION	1

/* The first thing in a trace file is the header, which basically just 
   defines the version of the file. */
typedef struct {
	u_int32_t magic;	/* Magic number for trace file. */
	u_int32_t version;	/* Version of file. */
	int32_t hlen;		/* Length of this header. */
	int32_t phlen;		/* Length of packet headers. */
} tracefile_header_t;

/* The trace file is composed of a bunch of trace packets.   Each such packet
   has a type, followed by a length, followed by a timestamp, followed by
   the actual contents of the packet.   The type indexes are not fixed -
   they are allocated either on readback or when writing a trace file.
   One index type is reserved - type zero means that this record is a type
   name to index mapping. */
typedef struct {
	u_int32_t type_index;	/* Index to the type of handler that this
				   packet needs. */
	u_int32_t length;	/* Length of the packet.  This includes
				   everything except the fixed header. */
	u_int32_t when;		/* When the packet was written. */
	u_int32_t pad;		/* Round this out to a quad boundary. */
} tracepacket_t;

#define TRACE_INDEX_MAPPING_SIZE 4	/* trace_index_mapping_t less name. */
typedef struct {
	u_int32_t index;
	char name [1];
} trace_index_mapping_t;

struct trace_type; /* forward */
typedef struct trace_type trace_type_t;

struct trace_type {
	trace_type_t *next;
	int index;
	char *name;
	void *baggage;
	void (*have_packet) (trace_type_t *, unsigned, char *);
	void (*stop_tracing) (trace_type_t *);
};

typedef struct trace_iov {
	const char *buf;
	unsigned len;
} trace_iov_t;

typedef struct {
	u_int16_t addrtype;
	u_int16_t addrlen;
	u_int8_t address [16];
	u_int16_t port;
} trace_addr_t;

void trace_free_all (void);
int trace_playback (void);
int trace_record (void);
isc_result_t trace_init (void (*set_time) (u_int32_t), const char *, int);
isc_result_t trace_begin (const char *, const char *, int);
isc_result_t trace_write_packet (trace_type_t *, unsigned, const char *,
				 const char *, int);
isc_result_t trace_write_packet_iov (trace_type_t *, int, trace_iov_t *,
				     const char *, int);
void trace_type_stash (trace_type_t *);
trace_type_t *trace_type_register (const char *, void *,
				   void (*) (trace_type_t *,
					     unsigned, char *),
				   void (*) (trace_type_t *),
				   const char *, int);
void trace_stop (void);
void trace_index_map_input (trace_type_t *, unsigned, char *);
void trace_index_stop_tracing (trace_type_t *);
void trace_replay_init (void);
void trace_file_replay (const char *);
isc_result_t trace_get_next_packet (trace_type_t **, tracepacket_t *,
				    char **, unsigned *, unsigned *);
isc_result_t trace_get_file (trace_type_t *,
			     const char *, unsigned *, char **);
isc_result_t trace_get_packet (trace_type_t **, unsigned *, char **);
time_t trace_snoop_time (trace_type_t **);
