/* Auto-reallocating array for arbitrary member types. */
/*
 * Copyright (c) 2020 Neels Hofmeyr <neels@hofmeyr.de>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* Usage:
 *
 * ARRAYLIST(any_type_t) list;
 * // OR
 * typedef ARRAYLIST(any_type_t) any_type_list_t;
 * any_type_list_t list;
 *
 * // pass the number of (at first unused) members to add on each realloc:
 * ARRAYLIST_INIT(list, 128);
 * any_type_t *x;
 * while (bar) {
 *         // This enlarges the allocated array as needed;
 *         // list.head may change due to realloc:
 *         ARRAYLIST_ADD(x, list);
 *         if (!x)
 *                 return ENOMEM; 
 *         *x = random_foo_value;
 * }
 * for (i = 0; i < list.len; i++)
 *         printf("%s", foo_to_str(list.head[i]));
 * ARRAYLIST_FREE(list);
 */
#define ARRAYLIST(MEMBER_TYPE) \
	struct { \
		MEMBER_TYPE *head; \
		MEMBER_TYPE *p; \
		unsigned int len; \
		unsigned int allocated; \
		unsigned int alloc_blocksize; \
	}

#define ARRAYLIST_INIT(ARRAY_LIST, ALLOC_BLOCKSIZE) do { \
		(ARRAY_LIST).head = NULL; \
		(ARRAY_LIST).len = 0; \
		(ARRAY_LIST).allocated = 0; \
		(ARRAY_LIST).alloc_blocksize = ALLOC_BLOCKSIZE; \
	} while(0)

#define ARRAYLIST_ADD(NEW_ITEM_P, ARRAY_LIST) do { \
		if ((ARRAY_LIST).len && !(ARRAY_LIST).allocated) { \
			NEW_ITEM_P = NULL; \
			break; \
		} \
		if ((ARRAY_LIST).head == NULL \
		    || (ARRAY_LIST).allocated < (ARRAY_LIST).len + 1) { \
			(ARRAY_LIST).p = recallocarray((ARRAY_LIST).head, \
				(ARRAY_LIST).len, \
				(ARRAY_LIST).allocated + \
				((ARRAY_LIST).allocated ? \
				(ARRAY_LIST).allocated / 2 : \
				(ARRAY_LIST).alloc_blocksize ? \
				(ARRAY_LIST).alloc_blocksize : 8), \
				sizeof(*(ARRAY_LIST).head)); \
			if ((ARRAY_LIST).p == NULL) { \
				NEW_ITEM_P = NULL; \
				break; \
			} \
			(ARRAY_LIST).allocated += \
				(ARRAY_LIST).allocated ? \
				(ARRAY_LIST).allocated / 2 : \
				(ARRAY_LIST).alloc_blocksize ? \
				(ARRAY_LIST).alloc_blocksize : 8, \
			(ARRAY_LIST).head = (ARRAY_LIST).p; \
			(ARRAY_LIST).p = NULL; \
		}; \
		if ((ARRAY_LIST).head == NULL \
		    || (ARRAY_LIST).allocated < (ARRAY_LIST).len + 1) { \
			NEW_ITEM_P = NULL; \
			break; \
		} \
		(NEW_ITEM_P) = &(ARRAY_LIST).head[(ARRAY_LIST).len]; \
		(ARRAY_LIST).len++; \
	} while (0)

#define ARRAYLIST_INSERT(NEW_ITEM_P, ARRAY_LIST, AT_IDX) do { \
		int _at_idx = (AT_IDX); \
		ARRAYLIST_ADD(NEW_ITEM_P, ARRAY_LIST); \
		if ((NEW_ITEM_P) \
		    && _at_idx >= 0 \
		    && _at_idx < (ARRAY_LIST).len) { \
			memmove(&(ARRAY_LIST).head[_at_idx + 1], \
				&(ARRAY_LIST).head[_at_idx], \
				((ARRAY_LIST).len - 1 - _at_idx) \
					* sizeof(*(ARRAY_LIST).head)); \
			(NEW_ITEM_P) = &(ARRAY_LIST).head[_at_idx]; \
		}; \
	} while (0)

#define ARRAYLIST_CLEAR(ARRAY_LIST) \
	(ARRAY_LIST).len = 0

#define ARRAYLIST_FREE(ARRAY_LIST) \
	do { \
		if ((ARRAY_LIST).head && (ARRAY_LIST).allocated) \
			free((ARRAY_LIST).head); \
		ARRAYLIST_INIT(ARRAY_LIST, (ARRAY_LIST).alloc_blocksize); \
	} while(0)

#define ARRAYLIST_FOREACH(ITEM_P, ARRAY_LIST) \
	for ((ITEM_P) = (ARRAY_LIST).head; \
	     (ITEM_P) - (ARRAY_LIST).head < (ARRAY_LIST).len; \
	     (ITEM_P)++)

#define ARRAYLIST_IDX(ITEM_P, ARRAY_LIST) ((ITEM_P) - (ARRAY_LIST).head)
