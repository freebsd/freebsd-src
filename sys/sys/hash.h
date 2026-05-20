/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2001 Tobias Weingartner
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $OpenBSD: hash.h,v 1.4 2004/05/25 18:37:23 jmc Exp $
 */

#ifndef _SYS_HASH_H_
#define	_SYS_HASH_H_
#include <sys/types.h>

/* Convenience */
#ifndef	HASHINIT
#define	HASHINIT	5381
#define	HASHSTEP(x,c)	(((x << 5) + x) + (c))
#endif

/*
 * Return a 32-bit hash of the given buffer.  The init
 * value should be 0, or the previous hash value to extend
 * the previous hash.
 */
static __inline uint32_t
hash32_buf(const void *buf, size_t len, uint32_t hash)
{
	const unsigned char *p = buf;

	while (len--)
		hash = HASHSTEP(hash, *p++);

	return hash;
}

/*
 * Return a 32-bit hash of the given string.
 */
static __inline uint32_t
hash32_str(const void *buf, uint32_t hash)
{
	const unsigned char *p = buf;

	while (*p)
		hash = HASHSTEP(hash, *p++);

	return hash;
}

/*
 * Return a 32-bit hash of the given string, limited by N.
 */
static __inline uint32_t
hash32_strn(const void *buf, size_t len, uint32_t hash)
{
	const unsigned char *p = buf;

	while (*p && len--)
		hash = HASHSTEP(hash, *p++);

	return hash;
}

/*
 * Return a 32-bit hash of the given string terminated by C,
 * (as well as 0).  This is mainly here as a helper for the
 * namei() hashing of path name parts.
 */
static __inline uint32_t
hash32_stre(const void *buf, int end, const char **ep, uint32_t hash)
{
	const unsigned char *p = buf;

	while (*p && (*p != end))
		hash = HASHSTEP(hash, *p++);

	if (ep)
		*ep = p;

	return hash;
}

/*
 * Return a 32-bit hash of the given string, limited by N,
 * and terminated by C (as well as 0).  This is mainly here
 * as a helper for the namei() hashing of path name parts.
 */
static __inline uint32_t
hash32_strne(const void *buf, size_t len, int end, const char **ep,
    uint32_t hash)
{
	const unsigned char *p = buf;

	while (*p && (*p != end) && len--)
		hash = HASHSTEP(hash, *p++);

	if (ep)
		*ep = p;

	return hash;
}

#ifdef _KERNEL
struct hashalloc_args {
	u_int	version;		/* for extendability, now 0 */
	int	error;			/* out: error on failure */
	size_t	size;			/* in: wanted, out: allocated */
	size_t	hdrsize;		/* size of bucket header, 0 = auto */
	enum {
		HASH_TYPE_POWER2 = 0,
		HASH_TYPE_PRIME,
	}	type;
	enum {
		HASH_HEAD_LIST = 0,
		HASH_HEAD_CK_LIST,
		HASH_HEAD_SLIST,
		HASH_HEAD_CK_SLIST,
		HASH_HEAD_STAILQ,
		HASH_HEAD_CK_STAILQ,
		HASH_HEAD_TAILQ,
	}	head;
	enum {
		HASH_LOCK_NONE = 0,
		HASH_LOCK_MTX,
		HASH_LOCK_RWLOCK,
		HASH_LOCK_SX,
		HASH_LOCK_RMLOCK,
		HASH_LOCK_RMSLOCK,
	}	lock;
	int	mflags;			/* malloc(9) flags */
	int	lopts;			/* lock opts */
	struct malloc_type	*mtype;	/* malloc(9) type */
	const char		*lname; /* lock name */
	int  (*ctor)(void *);		/* bucket constructor */
	void (*dtor)(void *);		/* bucket destructor */
};

void	*hashalloc(struct hashalloc_args *);
void	hashfree(void *, struct hashalloc_args *);

/*
 * Hashing function from Bob Jenkins. Implementation in libkern/jenkins_hash.c.
 */
uint32_t jenkins_hash(const void *, size_t, uint32_t);
uint32_t jenkins_hash32(const uint32_t *, size_t, uint32_t);

uint32_t murmur3_32_hash(const void *, size_t, uint32_t);
uint32_t murmur3_32_hash32(const uint32_t *, size_t, uint32_t);

#endif /* _KERNEL */

#endif /* !_SYS_HASH_H_ */
