/* alloc.h

   Definitions for the object management API protocol memory allocation... */

/*
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1996-2003 by Internet Software Consortium
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
 * by Ted Lemon in cooperation with Vixie Enterprises and Nominum, Inc.
 * To learn more about Internet Systems Consortium, see
 * ``http://www.isc.org/''.  To learn more about Vixie Enterprises,
 * see ``http://www.vix.com''.   To learn more about Nominum, Inc., see
 * ``http://www.nominum.com''.
 */

isc_result_t omapi_buffer_new (omapi_buffer_t **, const char *, int);
isc_result_t omapi_buffer_reference (omapi_buffer_t **,
				     omapi_buffer_t *, const char *, int);
isc_result_t omapi_buffer_dereference (omapi_buffer_t **, const char *, int);

#if defined (DEBUG_MEMORY_LEAKAGE) || defined (DEBUG_MALLOC_POOL) || \
		defined (DEBUG_MEMORY_LEAKAGE_ON_EXIT)
#define DMDOFFSET (sizeof (struct dmalloc_preamble))
#define DMLFSIZE 16
#define DMUFSIZE 16
#define DMDSIZE (DMDOFFSET + DMLFSIZE + DMUFSIZE)

struct dmalloc_preamble {
	struct dmalloc_preamble *prev, *next;
	const char *file;
	int line;
	size_t size;
	unsigned long generation;
	unsigned char low_fence [DMLFSIZE];
};
#else
#define DMDOFFSET 0
#define DMDSIZE 0
#endif

/* rc_history flags... */
#define RC_LEASE	1
#define RC_MISC		2

#if defined (DEBUG_RC_HISTORY)
#if !defined (RC_HISTORY_MAX)
# define RC_HISTORY_MAX 256
#endif

#if !defined (RC_HISTORY_FLAGS)
# define RC_HISTORY_FLAGS (RC_LEASE | RC_MISC)
#endif

struct rc_history_entry {
	const char *file;
	int line;
	void *reference;
	void *addr;
	int refcnt;
};

#define rc_register(x, l, r, y, z, d, f) do { \
		if (RC_HISTORY_FLAGS & ~(f)) { \
			rc_history [rc_history_index].file = (x); \
			rc_history [rc_history_index].line = (l); \
			rc_history [rc_history_index].reference = (r); \
			rc_history [rc_history_index].addr = (y); \
			rc_history [rc_history_index].refcnt = (z); \
			rc_history_next (d); \
		} \
	} while (0)
#define rc_register_mdl(r, y, z, d, f) \
	rc_register (__FILE__, __LINE__, r, y, z, d, f)
#else
#define rc_register(file, line, reference, addr, refcnt, d, f)
#define rc_register_mdl(reference, addr, refcnt, d, f)
#endif

#if defined (DEBUG_MEMORY_LEAKAGE) || defined (DEBUG_MALLOC_POOL) || \
		defined (DEBUG_MEMORY_LEAKAGE_ON_EXIT)
extern struct dmalloc_preamble *dmalloc_list;
extern unsigned long dmalloc_outstanding;
extern unsigned long dmalloc_longterm;
extern unsigned long dmalloc_generation;
extern unsigned long dmalloc_cutoff_generation;
#endif

#if defined (DEBUG_RC_HISTORY)
extern struct rc_history_entry rc_history [RC_HISTORY_MAX];
extern int rc_history_index;
extern int rc_history_count;
#endif
