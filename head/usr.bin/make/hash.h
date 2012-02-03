/*-
 * Copyright (c) 1988, 1989, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1988, 1989 by Adam de Boor
 * Copyright (c) 1989 by Berkeley Softworks
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam de Boor.
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
 *
 *	@(#)hash.h	8.1 (Berkeley) 6/6/93
 * $FreeBSD$
 */

#ifndef hash_h_f6312f46
#define	hash_h_f6312f46

/* hash.h --
 *
 * 	This file contains definitions used by the hash module,
 * 	which maintains hash tables.
 */

#include "util.h"

/*
 * The following defines one entry in the hash table.
 */
typedef struct Hash_Entry {
	struct Hash_Entry *next;	/* Link entries within same bucket. */
	void		*clientData;	/* Data associated with key. */
	unsigned	namehash;	/* hash value of key */
	char		name[1];	/* key string */
} Hash_Entry;

typedef struct Hash_Table {
	struct Hash_Entry **bucketPtr;	/* Buckets in the table */
	int 		size;		/* Actual size of array. */
	int 		numEntries;	/* Number of entries in the table. */
	int 		mask;		/* Used to select bits for hashing. */
} Hash_Table;

/*
 * The following structure is used by the searching routines
 * to record where we are in the search.
 */
typedef struct Hash_Search {
	const Hash_Table *tablePtr;	/* Table being searched. */
	int		nextIndex;	/* Next bucket to check */
	Hash_Entry 	*hashEntryPtr;	/* Next entry in current bucket */
} Hash_Search;

/*
 * Macros.
 */

/*
 * void *Hash_GetValue(const Hash_Entry *h)
 */
#define	Hash_GetValue(h) ((h)->clientData)

/*
 * Hash_SetValue(Hash_Entry *h, void *val);
 */
#define	Hash_SetValue(h, val) ((h)->clientData = (val))

void Hash_InitTable(Hash_Table *, int);
void Hash_DeleteTable(Hash_Table *);
Hash_Entry *Hash_FindEntry(const Hash_Table *, const char *);
Hash_Entry *Hash_CreateEntry(Hash_Table *, const char *, Boolean *);
void Hash_DeleteEntry(Hash_Table *, Hash_Entry *);
Hash_Entry *Hash_EnumFirst(const Hash_Table *, Hash_Search *);
Hash_Entry *Hash_EnumNext(Hash_Search *);

#endif /* hash_h_f6312f46 */
