/*
 * include/linux/ghash.h -- generic hashing with fuzzy retrieval
 *
 * (C) 1997 Thomas Schoebel-Theuer
 *
 * The algorithms implemented here seem to be a completely new invention,
 * and I'll publish the fundamentals in a paper.
 */

#ifndef _GHASH_H
#define _GHASH_H
/* HASHSIZE _must_ be a power of two!!! */


#define DEF_HASH_FUZZY_STRUCTS(NAME,HASHSIZE,TYPE) \
\
struct NAME##_table {\
	TYPE * hashtable[HASHSIZE];\
	TYPE * sorted_list;\
	int nr_entries;\
};\
\
struct NAME##_ptrs {\
	TYPE * next_hash;\
	TYPE * prev_hash;\
	TYPE * next_sorted;\
	TYPE * prev_sorted;\
};

#define DEF_HASH_FUZZY(LINKAGE,NAME,HASHSIZE,TYPE,PTRS,KEYTYPE,KEY,KEYCMP,KEYEQ,HASHFN)\
\
LINKAGE void insert_##NAME##_hash(struct NAME##_table * tbl, TYPE * elem)\
{\
	int ix = HASHFN(elem->KEY);\
	TYPE ** base = &tbl->hashtable[ix];\
	TYPE * ptr = *base;\
	TYPE * prev = NULL;\
\
	tbl->nr_entries++;\
	while(ptr && KEYCMP(ptr->KEY, elem->KEY)) {\
		base = &ptr->PTRS.next_hash;\
		prev = ptr;\
		ptr = *base;\
	}\
	elem->PTRS.next_hash = ptr;\
	elem->PTRS.prev_hash = prev;\
	if(ptr) {\
		ptr->PTRS.prev_hash = elem;\
	}\
	*base = elem;\
\
	ptr = prev;\
	if(!ptr) {\
		ptr = tbl->sorted_list;\
		prev = NULL;\
	} else {\
		prev = ptr->PTRS.prev_sorted;\
	}\
	while(ptr) {\
		TYPE * next = ptr->PTRS.next_hash;\
		if(next && KEYCMP(next->KEY, elem->KEY)) {\
			prev = ptr;\
			ptr = next;\
		} else if(KEYCMP(ptr->KEY, elem->KEY)) {\
			prev = ptr;\
			ptr = ptr->PTRS.next_sorted;\
		} else\
			break;\
	}\
	elem->PTRS.next_sorted = ptr;\
	elem->PTRS.prev_sorted = prev;\
	if(ptr) {\
		ptr->PTRS.prev_sorted = elem;\
	}\
	if(prev) {\
		prev->PTRS.next_sorted = elem;\
	} else {\
		tbl->sorted_list = elem;\
	}\
}\
\
LINKAGE void remove_##NAME##_hash(struct NAME##_table * tbl, TYPE * elem)\
{\
	TYPE * next = elem->PTRS.next_hash;\
	TYPE * prev = elem->PTRS.prev_hash;\
\
	tbl->nr_entries--;\
	if(next)\
		next->PTRS.prev_hash = prev;\
	if(prev)\
		prev->PTRS.next_hash = next;\
	else {\
		int ix = HASHFN(elem->KEY);\
		tbl->hashtable[ix] = next;\
	}\
\
	next = elem->PTRS.next_sorted;\
	prev = elem->PTRS.prev_sorted;\
	if(next)\
		next->PTRS.prev_sorted = prev;\
	if(prev)\
		prev->PTRS.next_sorted = next;\
	else\
		tbl->sorted_list = next;\
}\
\
LINKAGE TYPE * find_##NAME##_hash(struct NAME##_table * tbl, KEYTYPE pos)\
{\
	int ix = hashfn(pos);\
	TYPE * ptr = tbl->hashtable[ix];\
	while(ptr && KEYCMP(ptr->KEY, pos))\
		ptr = ptr->PTRS.next_hash;\
	if(ptr && !KEYEQ(ptr->KEY, pos))\
		ptr = NULL;\
	return ptr;\
}\
\
LINKAGE TYPE * find_##NAME##_hash_fuzzy(struct NAME##_table * tbl, KEYTYPE pos)\
{\
	int ix;\
	int offset;\
	TYPE * ptr;\
	TYPE * next;\
\
	ptr = tbl->sorted_list;\
	if(!ptr || KEYCMP(pos, ptr->KEY))\
		return NULL;\
	ix = HASHFN(pos);\
	offset = HASHSIZE;\
	do {\
		offset >>= 1;\
		next = tbl->hashtable[(ix+offset) & ((HASHSIZE)-1)];\
		if(next && (KEYCMP(next->KEY, pos) || KEYEQ(next->KEY, pos))\
		   && KEYCMP(ptr->KEY, next->KEY))\
			ptr = next;\
	} while(offset);\
\
	for(;;) {\
		next = ptr->PTRS.next_hash;\
		if(next) {\
			if(KEYCMP(next->KEY, pos)) {\
				ptr = next;\
				continue;\
			}\
		}\
		next = ptr->PTRS.next_sorted;\
		if(next && KEYCMP(next->KEY, pos)) {\
			ptr = next;\
			continue;\
		}\
		return ptr;\
	}\
	return NULL;\
}

#define DEF_HASH_STRUCTS(NAME,HASHSIZE,TYPE) \
\
struct NAME##_table {\
	TYPE * hashtable[HASHSIZE];\
	int nr_entries;\
};\
\
struct NAME##_ptrs {\
	TYPE * next_hash;\
	TYPE * prev_hash;\
};

#define DEF_HASH(LINKAGE,NAME,HASHSIZE,TYPE,PTRS,KEYTYPE,KEY,KEYCMP,KEYEQ,HASHFN)\
\
LINKAGE void insert_##NAME##_hash(struct NAME##_table * tbl, TYPE * elem)\
{\
	int ix = HASHFN(elem->KEY);\
	TYPE ** base = &tbl->hashtable[ix];\
	TYPE * ptr = *base;\
	TYPE * prev = NULL;\
\
	tbl->nr_entries++;\
	while(ptr && KEYCMP(ptr->KEY, elem->KEY)) {\
		base = &ptr->PTRS.next_hash;\
		prev = ptr;\
		ptr = *base;\
	}\
	elem->PTRS.next_hash = ptr;\
	elem->PTRS.prev_hash = prev;\
	if(ptr) {\
		ptr->PTRS.prev_hash = elem;\
	}\
	*base = elem;\
}\
\
LINKAGE void remove_##NAME##_hash(struct NAME##_table * tbl, TYPE * elem)\
{\
	TYPE * next = elem->PTRS.next_hash;\
	TYPE * prev = elem->PTRS.prev_hash;\
\
	tbl->nr_entries--;\
	if(next)\
		next->PTRS.prev_hash = prev;\
	if(prev)\
		prev->PTRS.next_hash = next;\
	else {\
		int ix = HASHFN(elem->KEY);\
		tbl->hashtable[ix] = next;\
	}\
}\
\
LINKAGE TYPE * find_##NAME##_hash(struct NAME##_table * tbl, KEYTYPE pos)\
{\
	int ix = hashfn(pos);\
	TYPE * ptr = tbl->hashtable[ix];\
	while(ptr && KEYCMP(ptr->KEY, pos))\
		ptr = ptr->PTRS.next_hash;\
	if(ptr && !KEYEQ(ptr->KEY, pos))\
		ptr = NULL;\
	return ptr;\
}

#endif
