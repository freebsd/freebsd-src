/*
 * Copyright (c) 2003
 *	Bill Paul <wpaul@windriver.com>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _NTOSKRNL_VAR_H_
#define _NTOSKRNL_VAR_H_

/* Note: assumes x86 page size of 4K. */
#define PAGE_SHIFT	12
#define SPAN_PAGES(ptr, len)					\
	((uint32_t)((((uintptr_t)(ptr) & (PAGE_SIZE -1)) +	\
	(len) + (PAGE_SIZE - 1)) >> PAGE_SHIFT))
#define PAGE_ALIGN(ptr)						\
	((void *)((uintptr_t)(ptr) & ~(PAGE_SIZE - 1)))
#define BYTE_OFFSET(ptr)					\
	((uint32_t)((uintptr_t)(ptr) & (PAGE_SIZE - 1)))
#define MDL_INIT(b, baseva, len)					\
	(b)->nb_next = NULL;						\
	(b)->nb_size = (uint16_t)(sizeof(struct ndis_buffer) +		\
		(sizeof(uint32_t) * SPAN_PAGES((baseva), (len))));	\
	(b)->nb_flags = 0;						\
	(b)->nb_startva = (void *)PAGE_ALIGN((baseva));			\
	(b)->nb_byteoffset = BYTE_OFFSET((baseva));			\
	(b)->nb_bytecount = (uint32_t)(len);
#define MDL_VA(b)						\
	((void *)((char *)((b)->nb_startva) + (b)->nb_byteoffset))

typedef uint32_t kspin_lock;

struct slist_entry {
	struct slist_entry	*sl_next;
};

typedef struct slist_entry slist_entry;

union slist_header {
	uint64_t		slh_align;
	struct {
		struct slist_entry	*slh_next;
		uint16_t		slh_depth;
		uint16_t		slh_seq;
	} slh_list;
};

typedef union slist_header slist_header;

struct list_entry {
        struct list_entry *nle_flink;
        struct list_entry *nle_blink;
};

typedef struct list_entry list_entry;

struct general_lookaside {
	slist_header		gl_listhead;
	uint16_t		gl_depth;
	uint16_t		gl_maxdepth;
	uint32_t		gl_totallocs;
	union {
		uint32_t		gl_allocmisses;
		uint32_t		gl_allochits;
	} u_a;
	uint32_t		gl_totalfrees;
	union {
		uint32_t		gl_freemisses;
		uint32_t		gl_freehits;
	} u_m;
	uint32_t		gl_type;
	uint32_t		gl_tag;
	uint32_t		gl_size;
	void			*gl_allocfunc;
	void			*gl_freefunc;
	list_entry		gl_listent;
	uint32_t		gl_lasttotallocs;
	union {
		uint32_t		gl_lastallocmisses;
		uint32_t		gl_lastallochits;
	} u_l;
	uint32_t		gl_rsvd[2];
};

typedef struct general_lookaside general_lookaside;

struct npaged_lookaside_list {
	general_lookaside	nll_l;
	kspin_lock		nll_obsoletelock;
};

typedef struct npaged_lookaside_list npaged_lookaside_list;
typedef struct npaged_lookaside_list paged_lookaside_list;

typedef void * (*lookaside_alloc_func)(uint32_t, size_t, uint32_t);
typedef void (*lookaside_free_func)(void *);


extern image_patch_table ntoskrnl_functbl[];

__BEGIN_DECLS
extern int ntoskrnl_libinit(void);
extern int ntoskrnl_libfini(void);
__END_DECLS

#endif /* _NTOSKRNL_VAR_H_ */
