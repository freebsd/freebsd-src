/*-
 * Copyright (c) 1990 The Regents of the University of California.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)radixsort.c	5.7 (Berkeley) 2/23/91";
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <limits.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

/*
 * __rspartition is the cutoff point for a further partitioning instead
 * of a shellsort.  If it changes check __rsshell_increments.  Both of
 * these are exported, as the best values are data dependent.
 */
#define	NPARTITION	40
int __rspartition = NPARTITION;
int __rsshell_increments[] = { 4, 1, 0, 0, 0, 0, 0, 0 };

/*
 * Stackp points to context structures, where each structure schedules a
 * partitioning.  Radixsort exits when the stack is empty.
 *
 * If the buckets are placed on the stack randomly, the worst case is when
 * all the buckets but one contain (npartitions + 1) elements and the bucket
 * pushed on the stack last contains the rest of the elements.  In this case,
 * stack growth is bounded by:
 *
 *	limit = (nelements / (npartitions + 1)) - 1;
 *
 * This is a very large number, 52,377,648 for the maximum 32-bit signed int.
 *
 * By forcing the largest bucket to be pushed on the stack first, the worst
 * case is when all but two buckets each contain (npartitions + 1) elements,
 * with the remaining elements split equally between the first and last
 * buckets pushed on the stack.  In this case, stack growth is bounded when:
 *
 *	for (partition_cnt = 0; nelements > npartitions; ++partition_cnt)
 *		nelements =
 *		    (nelements - (npartitions + 1) * (nbuckets - 2)) / 2;
 * The bound is:
 *
 *	limit = partition_cnt * (nbuckets - 1);
 *
 * This is a much smaller number, 4590 for the maximum 32-bit signed int.
 */
#define	NBUCKETS	(UCHAR_MAX + 1)

typedef struct _stack {
	const u_char **bot;
	int indx, nmemb;
} CONTEXT;

#define	STACKPUSH { \
	stackp->bot = p; \
	stackp->nmemb = nmemb; \
	stackp->indx = indx; \
	++stackp; \
}
#define	STACKPOP { \
	if (stackp == stack) \
		break; \
	--stackp; \
	bot = stackp->bot; \
	nmemb = stackp->nmemb; \
	indx = stackp->indx; \
}

/*
 * A variant of MSD radix sorting; see Knuth Vol. 3, page 177, and 5.2.5,
 * Ex. 10 and 12.  Also, "Three Partition Refinement Algorithms, Paige
 * and Tarjan, SIAM J. Comput. Vol. 16, No. 6, December 1987.
 *
 * This uses a simple sort as soon as a bucket crosses a cutoff point,
 * rather than sorting the entire list after partitioning is finished.
 * This should be an advantage.
 *
 * This is pure MSD instead of LSD of some number of MSD, switching to
 * the simple sort as soon as possible.  Takes linear time relative to
 * the number of bytes in the strings.
 */
int
#if __STDC__
radixsort(const u_char **l1, int nmemb, const u_char *tab, u_char endbyte)
#else
radixsort(l1, nmemb, tab, endbyte)
	const u_char **l1;
	register int nmemb;
	const u_char *tab;
	u_char endbyte;
#endif
{
	register int i, indx, t1, t2;
	register const u_char **l2;
	register const u_char **p;
	register const u_char **bot;
	register const u_char *tr;
	CONTEXT *stack, *stackp;
	int c[NBUCKETS + 1], max;
	u_char ltab[NBUCKETS];
	static void shellsort();

	if (nmemb <= 1)
		return(0);

	/*
	 * T1 is the constant part of the equation, the number of elements
	 * represented on the stack between the top and bottom entries.
	 * It doesn't get rounded as the divide by 2 rounds down (correct
	 * for a value being subtracted).  T2, the nelem value, has to be
	 * rounded up before each divide because we want an upper bound;
	 * this could overflow if nmemb is the maximum int.
	 */
	t1 = ((__rspartition + 1) * (NBUCKETS - 2)) >> 1;
	for (i = 0, t2 = nmemb; t2 > __rspartition; i += NBUCKETS - 1)
		t2 = ((t2 + 1) >> 1) - t1;
	if (i) {
		if (!(stack = stackp = (CONTEXT *)malloc(i * sizeof(CONTEXT))))
			return(-1);
	} else
		stack = stackp = NULL;

	/*
	 * There are two arrays, one provided by the user (l1), and the
	 * temporary one (l2).  The data is sorted to the temporary stack,
	 * and then copied back.  The speedup of using index to determine
	 * which stack the data is on and simply swapping stacks back and
	 * forth, thus avoiding the copy every iteration, turns out to not
	 * be any faster than the current implementation.
	 */
	if (!(l2 = (const u_char **)malloc(sizeof(u_char *) * nmemb)))
		return(-1);

	/*
	 * Tr references a table of sort weights; multiple entries may
	 * map to the same weight; EOS char must have the lowest weight.
	 */
	if (tab)
		tr = tab;
	else {
		for (t1 = 0, t2 = endbyte; t1 < t2; ++t1)
			ltab[t1] = t1 + 1;
		ltab[t2] = 0;
		for (t1 = endbyte + 1; t1 < NBUCKETS; ++t1)
			ltab[t1] = t1;
		tr = ltab;
	}

	/* First sort is entire stack */
	bot = l1;
	indx = 0;

	for (;;) {
		/* Clear bucket count array */
		bzero((char *)c, sizeof(c));

		/*
		 * Compute number of items that sort to the same bucket
		 * for this index.
		 */
		for (p = bot, i = nmemb; --i >= 0;)
			++c[tr[(*p++)[indx]]];

		/*
		 * Sum the number of characters into c, dividing the temp
		 * stack into the right number of buckets for this bucket,
		 * this index.  C contains the cumulative total of keys
		 * before and included in this bucket, and will later be
		 * used as an index to the bucket.  c[NBUCKETS] contains
		 * the total number of elements, for determining how many
		 * elements the last bucket contains.  At the same time
		 * find the largest bucket so it gets pushed first.
		 */
		for (i = max = t1 = 0, t2 = __rspartition; i <= NBUCKETS; ++i) {
			if (c[i] > t2) {
				t2 = c[i];
				max = i;
			}
			t1 = c[i] += t1;
		}

		/*
		 * Partition the elements into buckets; c decrements through
		 * the bucket, and ends up pointing to the first element of
		 * the bucket.
		 */
		for (i = nmemb; --i >= 0;) {
			--p;
			l2[--c[tr[(*p)[indx]]]] = *p;
		}

		/* Copy the partitioned elements back to user stack */
		bcopy(l2, bot, nmemb * sizeof(u_char *));

		++indx;
		/*
		 * Sort buckets as necessary; don't sort c[0], it's the
		 * EOS character bucket, and nothing can follow EOS.
		 */
		for (i = max; i; --i) {
			if ((nmemb = c[i + 1] - (t1 = c[i])) < 2)
				continue;
			p = bot + t1;
			if (nmemb > __rspartition)
				STACKPUSH
			else
				shellsort(p, indx, nmemb, tr);
		}
		for (i = max + 1; i < NBUCKETS; ++i) {
			if ((nmemb = c[i + 1] - (t1 = c[i])) < 2)
				continue;
			p = bot + t1;
			if (nmemb > __rspartition)
				STACKPUSH
			else
				shellsort(p, indx, nmemb, tr);
		}
		/* Break out when stack is empty */
		STACKPOP
	}

	free((char *)l2);
	free((char *)stack);
	return(0);
}

/*
 * Shellsort (diminishing increment sort) from Data Structures and
 * Algorithms, Aho, Hopcraft and Ullman, 1983 Edition, page 290;
 * see also Knuth Vol. 3, page 84.  The increments are selected from
 * formula (8), page 95.  Roughly O(N^3/2).
 */
static void
shellsort(p, indx, nmemb, tr)
	register u_char **p, *tr;
	register int indx, nmemb;
{
	register u_char ch, *s1, *s2;
	register int incr, *incrp, t1, t2;

	for (incrp = __rsshell_increments; incr = *incrp++;)
		for (t1 = incr; t1 < nmemb; ++t1)
			for (t2 = t1 - incr; t2 >= 0;) {
				s1 = p[t2] + indx;
				s2 = p[t2 + incr] + indx;
				while ((ch = tr[*s1++]) == tr[*s2] && ch)
					++s2;
				if (ch > tr[*s2]) {
					s1 = p[t2];
					p[t2] = p[t2 + incr];
					p[t2 + incr] = s1;
					t2 -= incr;
				} else
					break;
			}
}
