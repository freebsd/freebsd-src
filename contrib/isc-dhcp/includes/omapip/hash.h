/* hash.h

   Definitions for hashing... */

/*
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1995-2003 by Internet Software Consortium
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

#ifndef OMAPI_HASH_H
#define OMAPI_HASH_H

#define DEFAULT_HASH_SIZE	9973

/* The purpose of the hashed_object_t struct is to not match anything else. */
typedef struct {
	int foo;
} hashed_object_t;

typedef void (*hash_foreach_func) (const unsigned char *,
				   unsigned, hashed_object_t *);
typedef int (*hash_reference) (hashed_object_t **, hashed_object_t *,
			       const char *, int);
typedef int (*hash_dereference) (hashed_object_t **, const char *, int);

struct hash_bucket {
	struct hash_bucket *next;
	const unsigned char *name;
	unsigned len;
	hashed_object_t *value;
};

typedef int (*hash_comparator_t)(const void *, const void *, unsigned long);

struct hash_table {
	unsigned hash_count;
	struct hash_bucket *buckets [DEFAULT_HASH_SIZE];
	hash_reference referencer;
	hash_dereference dereferencer;
	hash_comparator_t cmp;
	int (*do_hash) (const unsigned char *, unsigned, unsigned);
};

struct named_hash {
	struct named_hash *next;
	const char *name;
	struct hash_table *hash;
};

#define HASH_FUNCTIONS_DECL(name, bufarg, type, hashtype)		      \
void name##_hash_add (hashtype *, bufarg, unsigned, type *,		      \
		      const char *, int);				      \
void name##_hash_delete (hashtype *, bufarg, unsigned,			      \
			 const char *, int);				      \
int name##_hash_lookup (type **, hashtype *, bufarg, unsigned,		      \
			const char *, int);				      \
int name##_hash_foreach (hashtype *,					      \
			 void (*) (bufarg, unsigned, type *));		      \
int name##_new_hash (hashtype **, int, const char *, int);		      \
void name##_free_hash_table (hashtype **, const char *, int);


#define HASH_FUNCTIONS(name, bufarg, type, hashtype, ref, deref)	      \
void name##_hash_add (hashtype *table,					      \
		      bufarg buf, unsigned len, type *ptr,		      \
		      const char *file, int line)			      \
{									      \
	add_hash ((struct hash_table *)table,				      \
		  (const unsigned char *)buf,				      \
		  len, (hashed_object_t *)ptr, file, line);		      \
}									      \
									      \
void name##_hash_delete (hashtype *table,				      \
			 bufarg buf, unsigned len, const char *file, int line)\
{									      \
	delete_hash_entry ((struct hash_table *)table,			      \
			   (const unsigned char *)buf,			      \
			   len, file, line);				      \
}									      \
									      \
int name##_hash_lookup (type **ptr, hashtype *table,			      \
			bufarg buf, unsigned len, const char *file, int line) \
{									      \
	return hash_lookup ((hashed_object_t **)ptr,			      \
			    (struct hash_table *)table,			      \
			    (const unsigned char *)buf, len, file, line);     \
}									      \
									      \
int name##_hash_foreach (hashtype *table,				      \
			 void (*func) (bufarg, unsigned, type *))	      \
{									      \
	return hash_foreach ((struct hash_table *)table,		      \
			     (hash_foreach_func)func);			      \
}									      \
									      \
int name##_new_hash (hashtype **tp, int c, const char *file, int line)	      \
{									      \
	return new_hash ((struct hash_table **)tp,			      \
			 (hash_reference)ref, (hash_dereference)deref, c,     \
			 file, line);					      \
}									      \
									      \
void name##_free_hash_table (hashtype **table, const char *file, int line)    \
{									      \
	free_hash_table ((struct hash_table **)table, file, line);	      \
}

void relinquish_hash_bucket_hunks (void);
int new_hash_table (struct hash_table **, int, const char *, int);
void free_hash_table (struct hash_table **, const char *, int);
struct hash_bucket *new_hash_bucket (const char *, int);
void free_hash_bucket (struct hash_bucket *, const char *, int);
int new_hash (struct hash_table **,
	      hash_reference, hash_dereference, int, const char *, int);
void add_hash (struct hash_table *,
		      const unsigned char *, unsigned, hashed_object_t *,
		      const char *, int);
void delete_hash_entry (struct hash_table *, const unsigned char *,
			       unsigned, const char *, int);
int hash_lookup (hashed_object_t **, struct hash_table *,
			const unsigned char *, unsigned, const char *, int);
int hash_foreach (struct hash_table *, hash_foreach_func);
int casecmp (const void *s, const void *t, unsigned long len);

#endif /* OMAPI_HASH_H */
