/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef	__STRING_TABLE_DOT_H
#define	__STRING_TABLE_DOT_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#include <sys/types.h>
#include <sys/avl.h>
#include <string_table.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * A string is represented in a string table using two values: length, and
 * value.  Grouping all the strings of a given length together allows for
 * efficient matching of tail strings, as each input string value is hashed.
 * Each string table uses a 2-level AVL tree of AVL trees to represent this
 * organization.
 *
 * The outer (main) AVL tree contains LenNode structures.  The search key for
 * nodes on this main tree is the string length.  Each such node represents
 * all strings of a given length, and all strings of that length are found
 * within.
 *
 * The strings within each LenNode are maintained using a secondary AVL tree
 * of StrNode structures.  The search key for this inner tree is the string
 * itself.  The strings are maintained in lexical order.
 */
typedef struct {
	avl_node_t	sn_avlnode;	/* AVL book-keeping */
	const char	*sn_str;	/* string */
	size_t		sn_refcnt;	/* reference count */
} StrNode;

typedef struct {
	avl_node_t	ln_avlnode;	/* AVL book-keeping */
	avl_tree_t	*ln_strtree;	/* AVL tree of associated strings */
	size_t		ln_strlen;	/* length of associated strings */
} LenNode;

/*
 * Define a master string data item.  Other strings may be suffixes of this
 * string.  The final string table will consist of the master string values,
 * laid end to end, with the other strings referencing their tails.
 */
typedef	struct str_master	Str_master;

struct str_master {
	const char	*sm_str;	/* pointer to master string */
	Str_master	*sm_next;	/* used for tracking master strings */
	size_t		sm_strlen;	/* length of master string */
	uint_t		sm_hashval;	/* hashval of master string */
	size_t		sm_stroff;	/* offset into destination strtab */
};

/*
 * Define a hash data item.  This item represents an individual string that has
 * been input into the String hash table.  The string may either be a suffix of
 * another string, or a master string.
 */
typedef	struct str_hash	Str_hash;

struct str_hash {
	size_t		hi_strlen;	/* string length */
	size_t		hi_refcnt;	/* number of references to str */
	uint_t		hi_hashval;	/* hash for string */
	Str_master	*hi_mstr;	/* pointer to master string */
	Str_hash	*hi_next;	/* next entry in hash bucket */
};

/*
 * Controlling data structure for a String Table.
 */
struct str_tbl {
	avl_tree_t	*st_lentree;		/* AVL tree of string lengths */
	char		*st_strbuf;		/* string buffer */
	Str_hash	**st_hashbcks;		/* hash buckets */
	Str_master	*st_mstrlist;		/* list of all master strings */
	size_t		st_fullstrsize;		/* uncompressed table size */
	size_t		st_nextoff;		/* next available string */
	size_t		st_strsize;		/* compressed size */
	size_t		st_strcnt;		/* number of strings */
	uint_t		st_hbckcnt;		/* number of buckets in */
						/*    hashlist */
	uint_t		st_flags;
};

#define	FLG_STTAB_COMPRESS	0x01		/* compressed string table */
#define	FLG_STTAB_COOKED	0x02		/* offset has been assigned */

/*
 * Starting value for use with string hashing functions inside of string_table.c
 */
#define	HASHSEED		5381

#ifdef __cplusplus
}
#endif

#endif /* __STRING_TABLE_DOT_H */
