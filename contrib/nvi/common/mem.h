/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1993, 1994, 1995, 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#ifdef DEBUG
#define CHECK_TYPE(type, var)						\
	type L__lp __attribute__((unused)) = var;
#else
#define CHECK_TYPE(type, var)
#endif

/* Increase the size of a malloc'd buffer.  Two versions, one that
 * returns, one that jumps to an error label.
 */
#define	BINC_GOTO(sp, type, lp, llen, nlen) do {			\
	CHECK_TYPE(type *, lp)						\
	void *L__bincp;							\
	if ((nlen) > llen) {						\
		if ((L__bincp = binc(sp, lp, &(llen), nlen)) == NULL)	\
			goto alloc_err;					\
		/*							\
		 * !!!							\
		 * Possible pointer conversion.				\
		 */							\
		lp = L__bincp;						\
	}								\
} while (0)
#define	BINC_GOTOC(sp, lp, llen, nlen)					\
	BINC_GOTO(sp, char, lp, llen, nlen)
#define	BINC_GOTOW(sp, lp, llen, nlen)					\
	BINC_GOTO(sp, CHAR_T, lp, llen, (nlen) * sizeof(CHAR_T))
#define	BINC_RET(sp, type, lp, llen, nlen) do {				\
	CHECK_TYPE(type *, lp)						\
	void *L__bincp;							\
	if ((nlen) > llen) {						\
		if ((L__bincp = binc(sp, lp, &(llen), nlen)) == NULL)	\
			return (1);					\
		/*							\
		 * !!!							\
		 * Possible pointer conversion.				\
		 */							\
		lp = L__bincp;						\
	}								\
} while (0)
#define	BINC_RETC(sp, lp, llen, nlen)					\
	BINC_RET(sp, char, lp, llen, nlen)
#define	BINC_RETW(sp, lp, llen, nlen)					\
	BINC_RET(sp, CHAR_T, lp, llen, (nlen) * sizeof(CHAR_T))

/*
 * Get some temporary space, preferably from the global temporary buffer,
 * from a malloc'd buffer otherwise.  Two versions, one that returns, one
 * that jumps to an error label.
 */
#define	GET_SPACE_GOTO(sp, type, bp, blen, nlen) do {			\
	CHECK_TYPE(type *, bp)						\
	GS *L__gp = (sp) == NULL ? NULL : (sp)->gp;			\
	if (L__gp == NULL || F_ISSET(L__gp, G_TMP_INUSE)) {		\
		bp = NULL;						\
		blen = 0;						\
		BINC_GOTO(sp, type, bp, blen, nlen); 			\
	} else {							\
		BINC_GOTOC(sp, L__gp->tmp_bp, L__gp->tmp_blen, nlen);	\
		bp = (type *) L__gp->tmp_bp;				\
		blen = L__gp->tmp_blen;					\
		F_SET(L__gp, G_TMP_INUSE);				\
	}								\
} while (0)
#define	GET_SPACE_GOTOC(sp, bp, blen, nlen)				\
	GET_SPACE_GOTO(sp, char, bp, blen, nlen)
#define	GET_SPACE_GOTOW(sp, bp, blen, nlen)				\
	GET_SPACE_GOTO(sp, CHAR_T, bp, blen, (nlen) * sizeof(CHAR_T))
#define	GET_SPACE_RET(sp, type, bp, blen, nlen) do {			\
	CHECK_TYPE(type *, bp)						\
	GS *L__gp = (sp) == NULL ? NULL : (sp)->gp;			\
	if (L__gp == NULL || F_ISSET(L__gp, G_TMP_INUSE)) {		\
		bp = NULL;						\
		blen = 0;						\
		BINC_RET(sp, type, bp, blen, nlen);			\
	} else {							\
		BINC_RETC(sp, L__gp->tmp_bp, L__gp->tmp_blen, nlen);	\
		bp = (type *) L__gp->tmp_bp;				\
		blen = L__gp->tmp_blen;					\
		F_SET(L__gp, G_TMP_INUSE);				\
	}								\
} while (0)
#define	GET_SPACE_RETC(sp, bp, blen, nlen)				\
	GET_SPACE_RET(sp, char, bp, blen, nlen)
#define	GET_SPACE_RETW(sp, bp, blen, nlen)				\
	GET_SPACE_RET(sp, CHAR_T, bp, blen, (nlen) * sizeof(CHAR_T))

/*
 * Add space to a GET_SPACE returned buffer.  Two versions, one that
 * returns, one that jumps to an error label.
 */
#define	ADD_SPACE_GOTO(sp, type, bp, blen, nlen) do {			\
	CHECK_TYPE(type *, bp)						\
	GS *L__gp = (sp) == NULL ? NULL : (sp)->gp;			\
	if (L__gp == NULL || bp == (type *)L__gp->tmp_bp) {		\
		F_CLR(L__gp, G_TMP_INUSE);				\
		BINC_GOTOC(sp, L__gp->tmp_bp, L__gp->tmp_blen, nlen);	\
		bp = (type *) L__gp->tmp_bp;				\
		blen = L__gp->tmp_blen;					\
		F_SET(L__gp, G_TMP_INUSE);				\
	} else								\
		BINC_GOTO(sp, type, bp, blen, nlen);			\
} while (0)
#define	ADD_SPACE_GOTOC(sp, bp, blen, nlen)				\
	ADD_SPACE_GOTO(sp, char, bp, blen, nlen)
#define	ADD_SPACE_GOTOW(sp, bp, blen, nlen)				\
	ADD_SPACE_GOTO(sp, CHAR_T, bp, blen, (nlen) * sizeof(CHAR_T))
#define	ADD_SPACE_RET(sp, type, bp, blen, nlen) do {			\
	CHECK_TYPE(type *, bp)						\
	GS *L__gp = (sp) == NULL ? NULL : (sp)->gp;			\
	if (L__gp == NULL || bp == (type *)L__gp->tmp_bp) {		\
		F_CLR(L__gp, G_TMP_INUSE);				\
		BINC_RETC(sp, L__gp->tmp_bp, L__gp->tmp_blen, nlen);	\
		bp = (type *) L__gp->tmp_bp;				\
		blen = L__gp->tmp_blen;					\
		F_SET(L__gp, G_TMP_INUSE);				\
	} else								\
		BINC_RET(sp, type, bp, blen, nlen);			\
} while (0)
#define	ADD_SPACE_RETC(sp, bp, blen, nlen)				\
	ADD_SPACE_RET(sp, char, bp, blen, nlen)
#define	ADD_SPACE_RETW(sp, bp, blen, nlen)				\
	ADD_SPACE_RET(sp, CHAR_T, bp, blen, (nlen) * sizeof(CHAR_T))

/* Free a GET_SPACE returned buffer. */
#define	FREE_SPACE(sp, bp, blen) do {					\
	GS *L__gp = (sp) == NULL ? NULL : (sp)->gp;			\
	if (L__gp != NULL && bp == L__gp->tmp_bp)			\
		F_CLR(L__gp, G_TMP_INUSE);				\
	else								\
		free(bp);						\
} while (0)
#define	FREE_SPACEW(sp, bp, blen) do {					\
	CHECK_TYPE(CHAR_T *, bp)					\
	FREE_SPACE(sp, (char *)bp, blen);				\
} while (0)

/*
 * Malloc a buffer, casting the return pointer.  Various versions.
 */
#define	CALLOC(sp, p, nmemb, size) do {					\
	if ((p = calloc(nmemb, size)) == NULL)				\
		msgq(sp, M_SYSERR, NULL);				\
} while (0)
#define	CALLOC_GOTO(sp, p, nmemb, size) do {				\
	if ((p = calloc(nmemb, size)) == NULL)				\
		goto alloc_err;						\
} while (0)
#define	CALLOC_RET(sp, p, nmemb, size) do {				\
	if ((p = calloc(nmemb, size)) == NULL) {			\
		msgq(sp, M_SYSERR, NULL);				\
		return (1);						\
	}								\
} while (0)

#define	MALLOC(sp, p, size) do {					\
	if ((p = malloc(size)) == NULL)					\
		msgq(sp, M_SYSERR, NULL);				\
} while (0)
#define	MALLOC_GOTO(sp, p, size) do {					\
	if ((p = malloc(size)) == NULL)					\
		goto alloc_err;						\
} while (0)
#define	MALLOC_RET(sp, p, size) do {					\
	if ((p = malloc(size)) == NULL) {				\
		msgq(sp, M_SYSERR, NULL);				\
		return (1);						\
	}								\
} while (0)

/*
 * Resize a buffer, free any already held memory if we can't get more.
 * FreeBSD's reallocf(3) does the same thing, but it's not portable yet.
 */
#define	REALLOC(sp, p, cast, size) do {					\
	cast newp;							\
	if ((newp = realloc(p, size)) == NULL) {			\
		free(p);						\
		msgq(sp, M_SYSERR, NULL);				\
	}								\
	p = newp;							\
} while (0)

/* 
 * p2roundup --
 *	Get next power of 2; convenient for realloc.
 *
 * Reference: FreeBSD /usr/src/lib/libc/stdio/getdelim.c
 */
static __inline size_t
p2roundup(size_t n)
{
	n--;
	n |= n >> 1;
	n |= n >> 2;
	n |= n >> 4;
	n |= n >> 8;
	n |= n >> 16;
#if SIZE_T_MAX > 0xffffffffU
	n |= n >> 32;
#endif
	n++;
	return (n);
}

/* Additional TAILQ helper. */
#define TAILQ_ENTRY_ISVALID(elm, field)					\
	((elm)->field.tqe_prev != NULL)
