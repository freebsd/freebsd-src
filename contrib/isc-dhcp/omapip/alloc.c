/* alloc.c

   Functions supporting memory allocation for the object management
   protocol... */

/*
 * Copyright (c) 1999-2003 Internet Software Consortium.
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

#include <omapip/omapip_p.h>

#if defined (DEBUG_MEMORY_LEAKAGE) || defined (DEBUG_MALLOC_POOL) || \
		defined (DEBUG_MEMORY_LEAKAGE_ON_EXIT)
struct dmalloc_preamble *dmalloc_list;
unsigned long dmalloc_outstanding;
unsigned long dmalloc_longterm;
unsigned long dmalloc_generation;
unsigned long dmalloc_cutoff_generation;
#endif

#if defined (DEBUG_RC_HISTORY)
struct rc_history_entry rc_history [RC_HISTORY_MAX];
int rc_history_index;
int rc_history_count;
#endif

#if defined (DEBUG_RC_HISTORY)
static void print_rc_hist_entry (int);
#endif

VOIDPTR dmalloc (size, file, line)
	unsigned size;
	const char *file;
	int line;
{
	unsigned char *foo = malloc (size + DMDSIZE);
	int i;
	VOIDPTR *bar;
#if defined (DEBUG_MEMORY_LEAKAGE) || defined (DEBUG_MALLOC_POOL) || \
		defined (DEBUG_MEMORY_LEAKAGE_ON_EXIT)
	struct dmalloc_preamble *dp;
#endif
	if (!foo)
		return (VOIDPTR)0;
	bar = (VOIDPTR)(foo + DMDOFFSET);
	memset (bar, 0, size);

#if defined (DEBUG_MEMORY_LEAKAGE) || defined (DEBUG_MALLOC_POOL) || \
		defined (DEBUG_MEMORY_LEAKAGE_ON_EXIT)
	dp = (struct dmalloc_preamble *)foo;
	dp -> prev = dmalloc_list;
	if (dmalloc_list)
		dmalloc_list -> next = dp;
	dmalloc_list = dp;
	dp -> next = (struct dmalloc_preamble *)0;
	dp -> size = size;
	dp -> file = file;
	dp -> line = line;
	dp -> generation = dmalloc_generation++;
	dmalloc_outstanding += size;
	for (i = 0; i < DMLFSIZE; i++)
		dp -> low_fence [i] =
			(((unsigned long)
			  (&dp -> low_fence [i])) % 143) + 113;
	for (i = DMDOFFSET; i < DMDSIZE; i++)
		foo [i + size] =
			(((unsigned long)
			  (&foo [i + size])) % 143) + 113;
#if defined (DEBUG_MALLOC_POOL_EXHAUSTIVELY)
	/* Check _every_ entry in the pool!   Very expensive. */
	for (dp = dmalloc_list; dp; dp = dp -> prev) {
		for (i = 0; i < DMLFSIZE; i++) {
			if (dp -> low_fence [i] !=
				(((unsigned long)
				  (&dp -> low_fence [i])) % 143) + 113)
			{
				log_error ("malloc fence modified: %s(%d)",
					   dp -> file, dp -> line);
				abort ();
			}
		}
		foo = (unsigned char *)dp;
		for (i = DMDOFFSET; i < DMDSIZE; i++) {
			if (foo [i + dp -> size] !=
				(((unsigned long)
				  (&foo [i + dp -> size])) % 143) + 113) {
				log_error ("malloc fence modified: %s(%d)",
					   dp -> file, dp -> line);
				abort ();
			}
		}
	}
#endif
#endif
#ifdef DEBUG_REFCNT_DMALLOC_FREE
	rc_register (file, line, 0, foo + DMDOFFSET, 1, 0, RC_MALLOC);
#endif
	return bar;
}

void dfree (ptr, file, line)
	VOIDPTR ptr;
	const char *file;
	int line;
{
	if (!ptr) {
		log_error ("dfree %s(%d): free on null pointer.", file, line);
		return;
	}
#if defined (DEBUG_MEMORY_LEAKAGE) || defined (DEBUG_MALLOC_POOL) || \
		defined (DEBUG_MEMORY_LEAKAGE_ON_EXIT)
	{
		unsigned char *bar = ptr;
		struct dmalloc_preamble *dp, *cur;
		int i;
		bar -= DMDOFFSET;
		cur = (struct dmalloc_preamble *)bar;
		for (dp = dmalloc_list; dp; dp = dp -> prev)
			if (dp == cur)
				break;
		if (!dp) {
			log_error ("%s(%d): freeing unknown memory: %lx",
				   file, line, (unsigned long)cur);
			abort ();
		}
		if (dp -> prev)
			dp -> prev -> next = dp -> next;
		if (dp -> next)
			dp -> next -> prev = dp -> prev;
		if (dp == dmalloc_list)
			dmalloc_list = dp -> prev;
		if (dp -> generation >= dmalloc_cutoff_generation)
			dmalloc_outstanding -= dp -> size;
		else
			dmalloc_longterm -= dp -> size;

		for (i = 0; i < DMLFSIZE; i++) {
			if (dp -> low_fence [i] !=
				(((unsigned long)
				  (&dp -> low_fence [i])) % 143) + 113)
			{
				log_error ("malloc fence modified: %s(%d)",
					   dp -> file, dp -> line);
				abort ();
			}
		}
		for (i = DMDOFFSET; i < DMDSIZE; i++) {
			if (bar [i + dp -> size] !=
				(((unsigned long)
				  (&bar [i + dp -> size])) % 143) + 113) {
				log_error ("malloc fence modified: %s(%d)",
					   dp -> file, dp -> line);
				abort ();
			}
		}
		ptr = bar;
	}
#endif
#ifdef DEBUG_REFCNT_DMALLOC_FREE
	rc_register (file, line,
		     0, (unsigned char *)ptr + DMDOFFSET, 0, 1, RC_MALLOC);
#endif
	free (ptr);
}

#if defined (DEBUG_MEMORY_LEAKAGE) || defined (DEBUG_MALLOC_POOL) || \
		defined (DEBUG_MEMORY_LEAKAGE_ON_EXIT)
/* For allocation functions that keep their own free lists, we want to
   account for the reuse of the memory. */

void dmalloc_reuse (foo, file, line, justref)
	VOIDPTR foo;
	const char *file;
	int line;
	int justref;
{
	struct dmalloc_preamble *dp;

	/* Get the pointer to the dmalloc header. */
	dp = foo;
	dp--;

	/* If we just allocated this and are now referencing it, this
	   function would almost be a no-op, except that it would
	   increment the generation count needlessly.  So just return
	   in this case. */
	if (dp -> generation == dmalloc_generation)
		return;

	/* If this is longterm data, and we just made reference to it,
	   don't put it on the short-term list or change its name -
	   we don't need to know about this. */
	if (dp -> generation < dmalloc_cutoff_generation && justref)
		return;

	/* Take it out of the place in the allocated list where it was. */
	if (dp -> prev)
		dp -> prev -> next = dp -> next;
	if (dp -> next)
		dp -> next -> prev = dp -> prev;
	if (dp == dmalloc_list)
		dmalloc_list = dp -> prev;

	/* Account for its removal. */
	if (dp -> generation >= dmalloc_cutoff_generation)
		dmalloc_outstanding -= dp -> size;
	else
		dmalloc_longterm -= dp -> size;

	/* Now put it at the head of the list. */
	dp -> prev = dmalloc_list;
	if (dmalloc_list)
		dmalloc_list -> next = dp;
	dmalloc_list = dp;
	dp -> next = (struct dmalloc_preamble *)0;

	/* Change the reference location information. */
	dp -> file = file;
	dp -> line = line;

	/* Increment the generation. */
	dp -> generation = dmalloc_generation++;

	/* Account for it. */
	dmalloc_outstanding += dp -> size;
}

void dmalloc_dump_outstanding ()
{
	static unsigned long dmalloc_cutoff_point;
	struct dmalloc_preamble *dp;
	unsigned char *foo;
	int i;

	if (!dmalloc_cutoff_point)
		dmalloc_cutoff_point = dmalloc_cutoff_generation;
	for (dp = dmalloc_list; dp; dp = dp -> prev) {
		if (dp -> generation <= dmalloc_cutoff_point)
			break;
#if defined (DEBUG_MALLOC_POOL)
		for (i = 0; i < DMLFSIZE; i++) {
			if (dp -> low_fence [i] !=
				(((unsigned long)
				  (&dp -> low_fence [i])) % 143) + 113)
			{
				log_error ("malloc fence modified: %s(%d)",
					   dp -> file, dp -> line);
				abort ();
			}
		}
		foo = (unsigned char *)dp;
		for (i = DMDOFFSET; i < DMDSIZE; i++) {
			if (foo [i + dp -> size] !=
				(((unsigned long)
				  (&foo [i + dp -> size])) % 143) + 113) {
				log_error ("malloc fence modified: %s(%d)",
					   dp -> file, dp -> line);
				abort ();
			}
		}
#endif
#if defined (DEBUG_MEMORY_LEAKAGE) || \
		defined (DEBUG_MEMORY_LEAKAGE_ON_EXIT)
		/* Don't count data that's actually on a free list
                   somewhere. */
		if (dp -> file) {
#if defined (DEBUG_RC_HISTORY)
			int i, count, inhistory = 0, noted = 0;

			/* If we have the info, see if this is actually
			   new garbage. */
			if (rc_history_count < RC_HISTORY_MAX) {
			    count = rc_history_count;
			} else
			    count = RC_HISTORY_MAX;
			i = rc_history_index - 1;
			if (i < 0)
				i += RC_HISTORY_MAX;

			do {
			    if (rc_history [i].addr == dp + 1) {
				inhistory = 1;
				if (!noted) {
				    log_info ("  %s(%d): %d", dp -> file,
					      dp -> line, dp -> size);
				    noted = 1;
				}
				print_rc_hist_entry (i);
				if (!rc_history [i].refcnt)
				    break;
			    }
			    if (--i < 0)
				i = RC_HISTORY_MAX - 1;
			} while (count--);
			if (!inhistory)
#endif
				log_info ("  %s(%d): %d",
					  dp -> file, dp -> line, dp -> size);
		}
#endif
	}
	if (dmalloc_list)
		dmalloc_cutoff_point = dmalloc_list -> generation;
}
#endif /* DEBUG_MEMORY_LEAKAGE || DEBUG_MALLOC_POOL */

#if defined (DEBUG_RC_HISTORY)
static void print_rc_hist_entry (int i)
{
	log_info ("   referenced by %s(%d)[%lx]: addr = %lx  refcnt = %x",
		  rc_history [i].file, rc_history [i].line,
		  (unsigned long)rc_history [i].reference,
		  (unsigned long)rc_history [i].addr,
		  rc_history [i].refcnt);
}

void dump_rc_history (void *addr)
{
	int i;

	i = rc_history_index;
	if (!rc_history [i].file)
		i = 0;
	else if (rc_history_count < RC_HISTORY_MAX) {
		i -= rc_history_count;
		if (i < 0)
			i += RC_HISTORY_MAX;
	}
	rc_history_count = 0;
		
	while (rc_history [i].file) {
		if (!addr || addr == rc_history [i].addr)
			print_rc_hist_entry (i);
		++i;
		if (i == RC_HISTORY_MAX)
			i = 0;
		if (i == rc_history_index)
			break;
	}
}
void rc_history_next (int d)
{
#if defined (RC_HISTORY_COMPRESSION)
	int i, j = 0, m, n = 0;
	void *ap, *rp;

	/* If we are decreasing the reference count, try to find the
	   entry where the reference was made and eliminate it; then
	   we can also eliminate this reference. */
	if (d) {
	    m = rc_history_index - 1000;
	    if (m < -1)
		m = -1;
	    ap = rc_history [rc_history_index].addr;
	    rp = rc_history [rc_history_index].reference;
	    for (i = rc_history_index - 1; i > m; i--) {
		if (rc_history [i].addr == ap) {
		    if (rc_history [i].reference == rp) {
			if (n > 10) {
			    for (n = i; n <= rc_history_index; n++)
				    print_rc_hist_entry (n);
			    n = 11;
			}
			memmove (&rc_history [i],
				 &rc_history [i + 1],
				 (unsigned)((rc_history_index - i) *
					    sizeof (struct rc_history_entry)));
			--rc_history_count;
			--rc_history_index;
			for (j = i; j < rc_history_count; j++) {
			    if (rc_history [j].addr == ap)
				--rc_history [j].refcnt;
			}
			if (n > 10) {
			    for (n = i; n <= rc_history_index; n++)
				    print_rc_hist_entry (n);
			    n = 11;
			    exit (0);
			}
			return;
		    }
		}
	    }
	}
#endif
	if (++rc_history_index == RC_HISTORY_MAX)
		rc_history_index = 0;
	++rc_history_count;
}
#endif

#if defined (DEBUG_MEMORY_LEAKAGE) || defined (DEBUG_MALLOC_POOL) || \
		defined (DEBUG_MEMORY_LEAKAGE_ON_EXIT)
struct caller {
	struct dmalloc_preamble *dp;
	int count;
};

static int dmalloc_find_entry (struct dmalloc_preamble *dp,
			       struct caller *array,
			       int min, int max)
{
	int middle;
	int cmp;

	middle = (min + max) / 2;
	if (middle == min)
		return middle;
	if (array [middle].dp -> file == dp -> file) {
		if (array [middle].dp -> line == dp -> line)
			return middle;
		else if (array [middle].dp -> line < dp -> line)
			return dmalloc_find_entry (dp, array, middle, max);
		else
			return dmalloc_find_entry (dp, array, 0, middle);
	} else if (array [middle].dp -> file < dp -> file)
		return dmalloc_find_entry (dp, array, middle, max);
	else
		return dmalloc_find_entry (dp, array, 0, middle);
}

void omapi_print_dmalloc_usage_by_caller ()
{
	struct dmalloc_preamble *dp;
	unsigned char *foo;
	int ccur, cmax, i, j;
	struct caller cp [1024];

	cmax = 1024;
	ccur = 0;

	memset (cp, 0, sizeof cp);
	for (dp = dmalloc_list; dp; dp = dp -> prev) {
		i = dmalloc_find_entry (dp, cp, 0, ccur);
		if ((i == ccur ||
		     cp [i].dp -> file != dp -> file ||
		     cp [i].dp -> line != dp -> line) &&
		    ccur == cmax) {
			log_error ("no space for memory usage summary.");
			return;
		}
		if (i == ccur) {
			cp [ccur++].dp = dp;
			cp [i].count = 1;
		} else if (cp [i].dp -> file < dp -> file ||
			   (cp [i].dp -> file == dp -> file &&
			    cp [i].dp -> line < dp -> line)) {
			if (i + 1 != ccur)
				memmove (cp + i + 2, cp + i + 1,
					 (ccur - i) * sizeof *cp);
			cp [i + 1].dp = dp;
			cp [i + 1].count = 1;
			ccur++;
		} else if (cp [i].dp -> file != dp -> file ||
			   cp [i].dp -> line != dp -> line) {
			memmove (cp + i + 1,
				 cp + i, (ccur - i) * sizeof *cp);
			cp [i].dp = dp;
			cp [i].count = 1;
			ccur++;
		} else
			cp [i].count++;
#if 0
		printf ("%d\t%s:%d\n", i, dp -> file, dp -> line);
		dump_rc_history (dp + 1);
#endif
	}
	for (i = 0; i < ccur; i++) {
		printf ("%d\t%s:%d\t%d\n", i,
			cp [i].dp -> file, cp [i].dp -> line, cp [i].count);
		dump_rc_history (cp [i].dp + 1);
	}
}
#endif /* DEBUG_MEMORY_LEAKAGE || DEBUG_MALLOC_POOL */

isc_result_t omapi_object_allocate (omapi_object_t **o,
				    omapi_object_type_t *type,
				    size_t size,
				    const char *file, int line)
{
	size_t tsize;
	omapi_object_t *foo;
	isc_result_t status;

	if (type -> allocator) {
		foo = (omapi_object_t *)0;
		status = (*type -> allocator) (&foo, file, line);
		tsize = type -> size;
	} else
		status = ISC_R_NOMEMORY;
	if (status == ISC_R_NOMEMORY) {
		if (type -> sizer)
			tsize = (*type -> sizer) (size);
		else
			tsize = type -> size;
		
		/* Sanity check. */
		if (tsize < sizeof (omapi_object_t))
			return ISC_R_INVALIDARG;
		
		foo = dmalloc (tsize, file, line);
		if (!foo)
			return ISC_R_NOMEMORY;
	}

	status = omapi_object_initialize (foo, type, size, tsize, file, line);
	if (status != ISC_R_SUCCESS) {
		if (type -> freer)
			(*type -> freer) (foo, file, line);
		else
			dfree (foo, file, line);
		return status;
	}
	return omapi_object_reference (o, foo, file, line);
}

isc_result_t omapi_object_initialize (omapi_object_t *o,
				      omapi_object_type_t *type,
				      size_t usize, size_t psize,
				      const char *file, int line)
{
	memset (o, 0, psize);
	o -> type = type;
	if (type -> initialize)
		(*type -> initialize) (o, file, line);
	return ISC_R_SUCCESS;
}

isc_result_t omapi_object_reference (omapi_object_t **r,
				     omapi_object_t *h,
				     const char *file, int line)
{
	if (!h || !r)
		return ISC_R_INVALIDARG;

	if (*r) {
#if defined (POINTER_DEBUG)
		log_error ("%s(%d): reference store into non-null pointer!",
			   file, line);
		abort ();
#else
		return ISC_R_INVALIDARG;
#endif
	}
	*r = h;
	h -> refcnt++;
	rc_register (file, line, r, h, h -> refcnt, 0, h -> type -> rc_flag);
	return ISC_R_SUCCESS;
}

isc_result_t omapi_object_dereference (omapi_object_t **h,
				       const char *file, int line)
{
	int outer_reference = 0;
	int inner_reference = 0;
	int handle_reference = 0;
	int extra_references;
	omapi_object_t *p, *hp;

	if (!h)
		return ISC_R_INVALIDARG;

	if (!*h) {
#if defined (POINTER_DEBUG)
		log_error ("%s(%d): dereference of null pointer!", file, line);
		abort ();
#else
		return ISC_R_INVALIDARG;
#endif
	}
	
	if ((*h) -> refcnt <= 0) {
#if defined (POINTER_DEBUG)
		log_error ("%s(%d): dereference of pointer with refcnt of zero!",
			   file, line);
#if defined (DEBUG_RC_HISTORY)
		dump_rc_history (*h);
#endif
		abort ();
#else
		*h = 0;
		return ISC_R_INVALIDARG;
#endif
	}
	
	/* See if this object's inner object refers to it, but don't
	   count this as a reference if we're being asked to free the
	   reference from the inner object. */
	if ((*h) -> inner && (*h) -> inner -> outer &&
	    h != &((*h) -> inner -> outer))
		inner_reference = 1;

	/* Ditto for the outer object. */
	if ((*h) -> outer && (*h) -> outer -> inner &&
	    h != &((*h) -> outer -> inner))
		outer_reference = 1;

	/* Ditto for the outer object.  The code below assumes that
	   the only reason we'd get a dereference from the handle
	   table is if this function does it - otherwise we'd have to
	   traverse the handle table to find the address where the
	   reference is stored and compare against that, and we don't
	   want to do that if we can avoid it. */
	if ((*h) -> handle)
		handle_reference = 1;

	/* If we are getting rid of the last reference other than
	   references to inner and outer objects, or from the handle
	   table, then we must examine all the objects in either
	   direction to see if they hold any non-inner, non-outer,
	   non-handle-table references.  If not, we need to free the
	   entire chain of objects. */
	if ((*h) -> refcnt ==
	    inner_reference + outer_reference + handle_reference + 1) {
		if (inner_reference || outer_reference || handle_reference) {
			/* XXX we could check for a reference from the
                           handle table here. */
			extra_references = 0;
			for (p = (*h) -> inner;
			     p && !extra_references; p = p -> inner) {
				extra_references += p -> refcnt;
				if (p -> inner && p -> inner -> outer == p)
					--extra_references;
				if (p -> outer)
					--extra_references;
				if (p -> handle)
					--extra_references;
			}
			for (p = (*h) -> outer;
			     p && !extra_references; p = p -> outer) {
				extra_references += p -> refcnt;
				if (p -> outer && p -> outer -> inner == p)
					--extra_references;
				if (p -> inner)
					--extra_references;
				if (p -> handle)
					--extra_references;
			}
		} else
			extra_references = 0;

		if (!extra_references) {
			hp = *h;
			*h = 0;
			hp -> refcnt--;
			if (inner_reference)
				omapi_object_dereference
					(&hp -> inner, file, line);
			if (outer_reference)
				omapi_object_dereference
					(&hp -> outer, file, line);
/*			if (!hp -> type -> freer) */
				rc_register (file, line, h, hp,
					     0, 1, hp -> type -> rc_flag);
			if (hp -> type -> destroy)
				(*(hp -> type -> destroy)) (hp, file, line);
			if (hp -> type -> freer)
				(hp -> type -> freer (hp, file, line));
			else
				dfree (hp, file, line);
		} else {
			(*h) -> refcnt--;
/*			if (!(*h) -> type -> freer) */
				rc_register (file, line,
					     h, *h, (*h) -> refcnt, 1,
					     (*h) -> type -> rc_flag);
		}
	} else {
		(*h) -> refcnt--;
/*		if (!(*h) -> type -> freer) */
			rc_register (file, line, h, *h, (*h) -> refcnt, 1,
				     (*h) -> type -> rc_flag);
	}
	*h = 0;
	return ISC_R_SUCCESS;
}

isc_result_t omapi_buffer_new (omapi_buffer_t **h,
			       const char *file, int line)
{
	omapi_buffer_t *t;
	isc_result_t status;
	
	t = (omapi_buffer_t *)dmalloc (sizeof *t, file, line);
	if (!t)
		return ISC_R_NOMEMORY;
	memset (t, 0, sizeof *t);
	status = omapi_buffer_reference (h, t, file, line);
	if (status != ISC_R_SUCCESS)
		dfree (t, file, line);
	(*h) -> head = sizeof ((*h) -> buf) - 1;
	return status;
}

isc_result_t omapi_buffer_reference (omapi_buffer_t **r,
				     omapi_buffer_t *h,
				     const char *file, int line)
{
	if (!h || !r)
		return ISC_R_INVALIDARG;

	if (*r) {
#if defined (POINTER_DEBUG)
		log_error ("%s(%d): reference store into non-null pointer!",
			   file, line);
		abort ();
#else
		return ISC_R_INVALIDARG;
#endif
	}
	*r = h;
	h -> refcnt++;
	rc_register (file, line, r, h, h -> refcnt, 0, RC_MISC);
	return ISC_R_SUCCESS;
}

isc_result_t omapi_buffer_dereference (omapi_buffer_t **h,
				       const char *file, int line)
{
	if (!h)
		return ISC_R_INVALIDARG;

	if (!*h) {
#if defined (POINTER_DEBUG)
		log_error ("%s(%d): dereference of null pointer!", file, line);
		abort ();
#else
		return ISC_R_INVALIDARG;
#endif
	}
	
	if ((*h) -> refcnt <= 0) {
#if defined (POINTER_DEBUG)
		log_error ("%s(%d): dereference of pointer with refcnt of zero!",
			   file, line);
#if defined (DEBUG_RC_HISTORY)
		dump_rc_history (*h);
#endif
		abort ();
#else
		*h = 0;
		return ISC_R_INVALIDARG;
#endif
	}

	--(*h) -> refcnt;
	rc_register (file, line, h, *h, (*h) -> refcnt, 1, RC_MISC);
	if ((*h) -> refcnt == 0)
		dfree (*h, file, line);
	*h = 0;
	return ISC_R_SUCCESS;
}

isc_result_t omapi_typed_data_new (const char *file, int line,
				   omapi_typed_data_t **t,
				   omapi_datatype_t type, ...)
{
	va_list l;
	omapi_typed_data_t *new;
	unsigned len;
	unsigned val;
	int intval;
	char *s;
	isc_result_t status;
	omapi_object_t *obj;

	va_start (l, type);

	switch (type) {
	      case omapi_datatype_int:
		len = OMAPI_TYPED_DATA_INT_LEN;
		intval = va_arg (l, int);
		break;
	      case omapi_datatype_string:
		s = va_arg (l, char *);
		val = strlen (s);
		len = OMAPI_TYPED_DATA_NOBUFFER_LEN + val;
		break;
	      case omapi_datatype_data:
		val = va_arg (l, unsigned);
		len = OMAPI_TYPED_DATA_NOBUFFER_LEN + val;
		break;
	      case omapi_datatype_object:
		len = OMAPI_TYPED_DATA_OBJECT_LEN;
		obj = va_arg (l, omapi_object_t *);
		break;
	      default:
		va_end (l);
		return ISC_R_INVALIDARG;
	}
	va_end (l);

	new = dmalloc (len, file, line);
	if (!new)
		return ISC_R_NOMEMORY;
	memset (new, 0, len);

	switch (type) {
	      case omapi_datatype_int:
		new -> u.integer = intval;
		break;
	      case omapi_datatype_string:
		memcpy (new -> u.buffer.value, s, val);
		new -> u.buffer.len = val;
		break;
	      case omapi_datatype_data:
		new -> u.buffer.len = val;
		break;
	      case omapi_datatype_object:
		status = omapi_object_reference (&new -> u.object, obj,
						 file, line);
		if (status != ISC_R_SUCCESS) {
			dfree (new, file, line);
			return status;
		}
		break;
	}
	new -> type = type;

	return omapi_typed_data_reference (t, new, file, line);
}

isc_result_t omapi_typed_data_reference (omapi_typed_data_t **r,
					 omapi_typed_data_t *h,
					 const char *file, int line)
{
	if (!h || !r)
		return ISC_R_INVALIDARG;

	if (*r) {
#if defined (POINTER_DEBUG)
		log_error ("%s(%d): reference store into non-null pointer!", file, line);
		abort ();
#else
		return ISC_R_INVALIDARG;
#endif
	}
	*r = h;
	h -> refcnt++;
	rc_register (file, line, r, h, h -> refcnt, 0, RC_MISC);
	return ISC_R_SUCCESS;
}

isc_result_t omapi_typed_data_dereference (omapi_typed_data_t **h,
					   const char *file, int line)
{
	if (!h)
		return ISC_R_INVALIDARG;

	if (!*h) {
#if defined (POINTER_DEBUG)
		log_error ("%s(%d): dereference of null pointer!", file, line);
		abort ();
#else
		return ISC_R_INVALIDARG;
#endif
	}
	
	if ((*h) -> refcnt <= 0) {
#if defined (POINTER_DEBUG)
		log_error ("%s(%d): dereference of pointer with refcnt of zero!",
			   file, line);
#if defined (DEBUG_RC_HISTORY)
		dump_rc_history (*h);
#endif
		abort ();
#else
		*h = 0;
		return ISC_R_INVALIDARG;
#endif
	}
	
	--((*h) -> refcnt);
	rc_register (file, line, h, *h, (*h) -> refcnt, 1, RC_MISC);
	if ((*h) -> refcnt <= 0 ) {
		switch ((*h) -> type) {
		      case omapi_datatype_int:
		      case omapi_datatype_string:
		      case omapi_datatype_data:
		      default:
			break;
		      case omapi_datatype_object:
			omapi_object_dereference (&(*h) -> u.object,
						  file, line);
			break;
		}
		dfree (*h, file, line);
	}
	*h = 0;
	return ISC_R_SUCCESS;
}

isc_result_t omapi_data_string_new (omapi_data_string_t **d, unsigned len,
				    const char *file, int line)
{
	omapi_data_string_t *new;

	new = dmalloc (OMAPI_DATA_STRING_EMPTY_SIZE + len, file, line);
	if (!new)
		return ISC_R_NOMEMORY;
	memset (new, 0, OMAPI_DATA_STRING_EMPTY_SIZE);
	new -> len = len;
	return omapi_data_string_reference (d, new, file, line);
}

isc_result_t omapi_data_string_reference (omapi_data_string_t **r,
					  omapi_data_string_t *h,
					  const char *file, int line)
{
	if (!h || !r)
		return ISC_R_INVALIDARG;

	if (*r) {
#if defined (POINTER_DEBUG)
		log_error ("%s(%d): reference store into non-null pointer!", file, line);
		abort ();
#else
		return ISC_R_INVALIDARG;
#endif
	}
	*r = h;
	h -> refcnt++;
	rc_register (file, line, r, h, h -> refcnt, 0, RC_MISC);
	return ISC_R_SUCCESS;
}

isc_result_t omapi_data_string_dereference (omapi_data_string_t **h,
					    const char *file, int line)
{
	if (!h)
		return ISC_R_INVALIDARG;

	if (!*h) {
#if defined (POINTER_DEBUG)
		log_error ("%s(%d): dereference of null pointer!", file, line);
		abort ();
#else
		return ISC_R_INVALIDARG;
#endif
	}
	
	if ((*h) -> refcnt <= 0) {
#if defined (POINTER_DEBUG)
		log_error ("%s(%d): dereference of pointer with refcnt of zero!",
			   file, line);
#if defined (DEBUG_RC_HISTORY)
		dump_rc_history (*h);
#endif
		abort ();
#else
		*h = 0;
		return ISC_R_INVALIDARG;
#endif
	}

	--((*h) -> refcnt);
	rc_register (file, line, h, *h, (*h) -> refcnt, 1, RC_MISC);
	if ((*h) -> refcnt <= 0 ) {
		dfree (*h, file, line);
	}
	*h = 0;
	return ISC_R_SUCCESS;
}

isc_result_t omapi_value_new (omapi_value_t **d,
			      const char *file, int line)
{
	omapi_value_t *new;

	new = dmalloc (sizeof *new, file, line);
	if (!new)
		return ISC_R_NOMEMORY;
	memset (new, 0, sizeof *new);
	return omapi_value_reference (d, new, file, line);
}

isc_result_t omapi_value_reference (omapi_value_t **r,
				    omapi_value_t *h,
				    const char *file, int line)
{
	if (!h || !r)
		return ISC_R_INVALIDARG;

	if (*r) {
#if defined (POINTER_DEBUG)
		log_error ("%s(%d): reference store into non-null pointer!",
			   file, line);
		abort ();
#else
		return ISC_R_INVALIDARG;
#endif
	}
	*r = h;
	h -> refcnt++;
	rc_register (file, line, r, h, h -> refcnt, 0, RC_MISC);
	return ISC_R_SUCCESS;
}

isc_result_t omapi_value_dereference (omapi_value_t **h,
				      const char *file, int line)
{
	if (!h)
		return ISC_R_INVALIDARG;

	if (!*h) {
#if defined (POINTER_DEBUG)
		log_error ("%s(%d): dereference of null pointer!", file, line);
		abort ();
#else
		return ISC_R_INVALIDARG;
#endif
	}
	
	if ((*h) -> refcnt <= 0) {
#if defined (POINTER_DEBUG)
		log_error ("%s(%d): dereference of pointer with refcnt of zero!",
			   file, line);
#if defined (DEBUG_RC_HISTORY)
		dump_rc_history (*h);
#endif
		abort ();
#else
		*h = 0;
		return ISC_R_INVALIDARG;
#endif
	}
	
	--((*h) -> refcnt);
	rc_register (file, line, h, *h, (*h) -> refcnt, 1, RC_MISC);
	if ((*h) -> refcnt == 0) {
		if ((*h) -> name)
			omapi_data_string_dereference (&(*h) -> name,
						       file, line);
		if ((*h) -> value)
			omapi_typed_data_dereference (&(*h) -> value,
						      file, line);
		dfree (*h, file, line);
	}
	*h = 0;
	return ISC_R_SUCCESS;
}

isc_result_t omapi_addr_list_new (omapi_addr_list_t **d, unsigned count,
				  const char *file, int line)
{
	omapi_addr_list_t *new;

	new = dmalloc ((count * sizeof (omapi_addr_t)) +
		       sizeof (omapi_addr_list_t), file, line);
	if (!new)
		return ISC_R_NOMEMORY;
	memset (new, 0, ((count * sizeof (omapi_addr_t)) +
			 sizeof (omapi_addr_list_t)));
	new -> count = count;
	new -> addresses = (omapi_addr_t *)(new + 1);
	return omapi_addr_list_reference (d, new, file, line);
}

isc_result_t omapi_addr_list_reference (omapi_addr_list_t **r,
					  omapi_addr_list_t *h,
					  const char *file, int line)
{
	if (!h || !r)
		return ISC_R_INVALIDARG;

	if (*r) {
#if defined (POINTER_DEBUG)
		log_error ("%s(%d): reference store into non-null pointer!",
			   file, line);
		abort ();
#else
		return ISC_R_INVALIDARG;
#endif
	}
	*r = h;
	h -> refcnt++;
	rc_register (file, line, r, h, h -> refcnt, 0, RC_MISC);
	return ISC_R_SUCCESS;
}

isc_result_t omapi_addr_list_dereference (omapi_addr_list_t **h,
					    const char *file, int line)
{
	if (!h)
		return ISC_R_INVALIDARG;

	if (!*h) {
#if defined (POINTER_DEBUG)
		log_error ("%s(%d): dereference of null pointer!", file, line);
		abort ();
#else
		return ISC_R_INVALIDARG;
#endif
	}
	
	if ((*h) -> refcnt <= 0) {
#if defined (POINTER_DEBUG)
		log_error ("%s(%d): dereference of pointer with zero refcnt!",
			   file, line);
#if defined (DEBUG_RC_HISTORY)
		dump_rc_history (*h);
#endif
		abort ();
#else
		*h = 0;
		return ISC_R_INVALIDARG;
#endif
	}

	--((*h) -> refcnt);
	rc_register (file, line, h, *h, (*h) -> refcnt, 1, RC_MISC);
	if ((*h) -> refcnt <= 0 ) {
		dfree (*h, file, line);
	}
	*h = 0;
	return ISC_R_SUCCESS;
}

