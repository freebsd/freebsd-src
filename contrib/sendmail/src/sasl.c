/*
 * Copyright (c) 2001 Sendmail, Inc. and its suppliers.
 *	All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 */

#if SASL
# include <sm/gen.h>
SM_RCSID("@(#)$Id: sasl.c,v 8.11 2001/09/11 04:05:16 gshapiro Exp $")
# include <stdlib.h>
# include <sendmail.h>
# include <errno.h>
# include <sasl.h>

/*
**  In order to ensure that storage leaks are tracked, and to prevent
**  conflicts between the sm_heap package and sasl, we tell sasl to
**  use the following heap allocation functions.  Unfortunately,
**  the sasl package incorrectly specifies the size of a block
**  using unsigned long: for portability, it should be size_t.
*/

void *sm_sasl_malloc __P((unsigned long));
static void *sm_sasl_calloc __P((unsigned long, unsigned long));
static void *sm_sasl_realloc __P((void *, unsigned long));
void sm_sasl_free __P((void *));

/*
**  We can't use an rpool for Cyrus-SASL memory management routines,
**	since the encryption/decryption routines in Cyrus-SASL
**	allocate/deallocate a buffer each time. Since rpool
**	don't release memory until the very end, memory consumption is
**	proportional to the size of an e-mail, which is unacceptable.
**
*/

/*
**  SM_SASL_MALLOC -- malloc() for SASL
**
**	Parameters:
**		size -- size of requested memory.
**
**	Returns:
**		pointer to memory.
*/

void *
sm_sasl_malloc(size)
	unsigned long size;
{
	return sm_malloc((size_t) size);
}

/*
**  SM_SASL_CALLOC -- calloc() for SASL
**
**	Parameters:
**		nelem -- number of elements.
**		elemsize -- size of each element.
**
**	Returns:
**		pointer to memory.
**
**	Notice:
**		this isn't currently used by SASL.
*/

static void *
sm_sasl_calloc(nelem, elemsize)
	unsigned long nelem;
	unsigned long elemsize;
{
	size_t size;
	void *p;

	size = (size_t) nelem * (size_t) elemsize;
	p = sm_malloc(size);
	if (p == NULL)
		return NULL;
	memset(p, '\0', size);
	return p;
}

/*
**  SM_SASL_REALLOC -- realloc() for SASL
**
**	Parameters:
**		p -- pointer to old memory.
**		size -- size of requested memory.
**
**	Returns:
**		pointer to new memory.
*/

static void *
sm_sasl_realloc(o, size)
	void *o;
	unsigned long size;
{
	return sm_realloc(o, (size_t) size);
}

/*
**  SM_SASL_FREE -- free() for SASL
**
**	Parameters:
**		p -- pointer to free.
**
**	Returns:
**		none
*/

void
sm_sasl_free(p)
	void *p;
{
	sm_free(p);
}

/*
**  SM_SASL_INIT -- sendmail specific SASL initialization
**
**	Parameters:
**		none.
**
**	Returns:
**		none
**
**	Side Effects:
**		installs memory management routines for SASL.
*/

void
sm_sasl_init()
{
	sasl_set_alloc(sm_sasl_malloc, sm_sasl_calloc,
		       sm_sasl_realloc, sm_sasl_free);
}
/*
**  INTERSECT -- create the intersection between two lists
**
**	Parameters:
**		s1, s2 -- lists of items (separated by single blanks).
**		rpool -- resource pool from which result is allocated.
**
**	Returns:
**		the intersection of both lists.
*/

char *
intersect(s1, s2, rpool)
	char *s1, *s2;
	SM_RPOOL_T *rpool;
{
	char *hr, *h1, *h, *res;
	int l1, l2, rl;

	if (s1 == NULL || s2 == NULL)	/* NULL string(s) -> NULL result */
		return NULL;
	l1 = strlen(s1);
	l2 = strlen(s2);
	rl = SM_MIN(l1, l2);
	res = (char *) sm_rpool_malloc(rpool, rl + 1);
	if (res == NULL)
		return NULL;
	*res = '\0';
	if (rl == 0)	/* at least one string empty? */
		return res;
	hr = res;
	h1 = s1;
	h = s1;

	/* walk through s1 */
	while (h != NULL && *h1 != '\0')
	{
		/* is there something after the current word? */
		if ((h = strchr(h1, ' ')) != NULL)
			*h = '\0';
		l1 = strlen(h1);

		/* does the current word appear in s2 ? */
		if (iteminlist(h1, s2, " ") != NULL)
		{
			/* add a blank if not first item */
			if (hr != res)
				*hr++ = ' ';

			/* copy the item */
			memcpy(hr, h1, l1);

			/* advance pointer in result list */
			hr += l1;
			*hr = '\0';
		}
		if (h != NULL)
		{
			/* there are more items */
			*h = ' ';
			h1 = h + 1;
		}
	}
	return res;
}
#endif /* SASL */
