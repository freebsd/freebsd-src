/* alloc.h

   Definitions for the object management API protocol memory allocation... */

/*
 * Copyright (c) 1996-2001 Internet Software Consortium.
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
 * by Ted Lemon in cooperation with Vixie Enterprises and Nominum, Inc.
 * To learn more about the Internet Software Consortium, see
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
