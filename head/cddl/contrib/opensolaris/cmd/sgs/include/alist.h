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
 *
 * Define an Alist, a list maintained as a reallocable array, and a for() loop
 * macro to generalize its traversal.  Note that the array can be reallocated
 * as it is being traversed, thus the offset of each element is recomputed from
 * the start of the structure.
 */

#ifndef	_ALIST_H
#define	_ALIST_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/types.h>
#if defined(sun)
#include <sys/machelf.h>
#else
#include <sys/elf.h>
#endif

/*
 * An Alist implements array lists. The functionality is similar to
 * that of a linked list. However, an Alist is represented by a single
 * contigious allocation of memory. The head of the memory is a header
 * that contains control information for the list. Following the header
 * is an array used to hold the user data. In the type definitions that
 * follow, we define these as an array with a single element, but when
 * we allocate the memory, we actually allocate the amount of memory needed.
 *
 * There are two "flavors" of array list:
 *
 *	Alist - Contain arbitrary data, usually structs.
 *	APlist - Contain pointers to data allocated elsewhere.
 *
 * This differentiation is useful, because pointer lists are heavily
 * used, and support a slightly different set of operations that are
 * unique to their purpose.
 *
 * Array lists are initially represented by a NULL pointer. The memory
 * for the list is only allocated if an item is inserted. This is very
 * efficient for data structures that may or may not be needed for a
 * given linker operation --- you only pay for what you use. In addition:
 *
 *	- Array lists grow as needed (memory is reallocated as necessary)
 *	- Data is kept contiguously (no unused holes in between elements)
 *		at the beginning of the data area. This locality has
 *		good cache behavior, as access to adjacent items are
 *		highly likely to be in the same page of memory.
 *	- Insert/Delete operations at the end of the list are very
 *		efficient. However, insert/delete operations elsewhere
 *		will cause a relatively expensive overlapped memory
 *		copy of the data following the insert/delete location.
 *	- As with any generic memory alloctor (i.e. malloc()/free()),
 *		array lists are not type safe for the data they contain.
 *		Data is managed as (void *) pointers to data of a given
 *		length, so the Alist module cannot prevent the caller from
 *		inserting/extracting the wrong type of data. The caller
 *		must guard against this.
 *	- To free an array list, simply call the standard free() function
 *		on the list pointer.
 */



/*
 * Aliste is used to represent list indexes, offsets, and sizes.
 */
typedef	size_t	Aliste;



/*
 * Alist is used to hold non-pointer items --- usually structs:
 *	- There must be an even number of Aliste fields before the
 *		al_data field. This ensures that al_data will have
 *		an alignment of 8, no matter whether sizeof(Aliste)
 *		is 4 or 8. That means that al_data will have sufficient
 *		alignment for any use, just like memory allocated via
 *		malloc().
 *	- al_nitems and al_next are redundant, in that they are
 *		directly related:
 *			al_next = al_nitems * al_size
 *		We do this to make ALIST_TRAVERSE_BYOFFSET maximally
 *		efficient. This doesn't waste space, because of the
 *		requirement to have an even # of Alist fields (above).
 *
 * Note that Alists allow the data to be referenced by 0 based array
 * index, or by their byte offset from the start of the Alist memory
 * allocation. The index form is preferred for most use, as it is simpler.
 * However, by-offset access is used by rtld link maps, and this ability
 * is convenient in that case.
 */
typedef struct {
	Aliste 		al_arritems;	/* # of items in al_data allocation */
	Aliste 		al_nitems;	/* # items (index of next avail item) */
	Aliste 		al_next;	/* offset of next available al_data[] */
	Aliste		al_size;	/* size of each al_data[] item */
	void 		*al_data[1];	/* data (can grow) */
} Alist;

/*
 * APlist is a variant of Alist that contains pointers. There are several
 * benefits to this special type:
 *	- API is simpler
 *	- Pointers are used directly, instead of requiring a
 *		pointer-to-pointer double indirection.
 *	- The implementation is slightly more efficient.
 *	- Operations that make particular sense for pointers
 *		can be supported without confusing the API for the
 *		regular Alists.
 */
typedef struct {
	Aliste		apl_arritems;	/* # of items in apl_data allocation */
	Aliste 		apl_nitems;	/* # items (index of next avail item) */
	void		*apl_data[1];	/* data area: (arrcnt * size) bytes */
} APlist;


/*
 * The ALIST_OFF_DATA and APLIST_OFF_DATA macros give the byte offset
 * from the start of an array list to the first byte of the data area
 * used to hold user data. The same trick used by the standard offsetof()
 * macro is used.
 */
#define	ALIST_OFF_DATA	((size_t)(((Alist *)0)->al_data))
#define	APLIST_OFF_DATA	((size_t)(((APlist *)0)->apl_data))


/*
 * The TRAVERSE macros are intended to be used within a for(), and
 * cause the resulting loop to iterate over each item in the loop,
 * in order.
 *	ALIST_TRAVERSE: Traverse over the items in an Alist,
 *		using the zero based item array index to refer to
 *		each item.
 *	ALIST_TRAVERSE_BY_OFFSET: Traverse over the items in an
 *		Alist using the byte offset from the head of the
 *		Alist pointer to refer to each item. It should be noted
 *		that the first such offset is given by ALIST_OFF_DATA,
 *		and as such, there will never be a 0 offset. Some code
 *		uses this fact to treat 0 as a reserved value with
 *		special meaning.
 *
 *		By-offset access is convenient for some parts of
 *		rtld, where a value of 0 is used to indicate an
 *		uninitialized link map control.
 *
 *	APLIST_TRAVERSE: Traverse over the pointers in an APlist, using
 *		the zero based item array index to refer to each pointer.
 */

/*
 * Within the loop:
 *
 *	LIST - Pointer to Alist structure for list
 *	IDX - The current item index
 *	OFF - The current item offset
 *	DATA - Pointer to item
 */
#define	ALIST_TRAVERSE(LIST, IDX, DATA) \
	(IDX) = 0, \
	((LIST) != NULL) && ((DATA) = (void *)(LIST)->al_data); \
	\
	((LIST) != NULL) && ((IDX) < (LIST)->al_nitems); \
	\
	(IDX)++, \
	(DATA) = (void *) (((LIST)->al_size * (IDX)) + (char *)(LIST)->al_data)

#define	ALIST_TRAVERSE_BY_OFFSET(LIST, OFF, DATA) \
	(((LIST) != NULL) && ((OFF) = ALIST_OFF_DATA) && \
	(((DATA) = (void *)((char *)(LIST) + (OFF))))); \
	\
	(((LIST) != NULL) && ((OFF) < (LIST)->al_next)); \
	\
	(((OFF) += ((LIST)->al_size)), \
	((DATA) = (void *)((char *)(LIST) + (OFF))))

/*
 * Within the loop:
 *
 *	LIST - Pointer to APlist structure for list
 *	IDX - The current item index
 *	PTR - item value
 *
 * Note that this macro is designed to ensure that PTR retains the
 * value of the final pointer in the list after exiting the for loop,
 * and to avoid dereferencing an out of range address. This is done by
 * doing the dereference in the middle expression, using the comma
 * operator to ensure that a NULL pointer won't stop the loop.
 */
#define	APLIST_TRAVERSE(LIST, IDX, PTR) \
	(IDX) = 0; \
	\
	((LIST) != NULL) && ((IDX) < (LIST)->apl_nitems) && \
	(((PTR) = ((LIST)->apl_data)[IDX]), 1); \
	\
	(IDX)++


/*
 * Possible values returned by aplist_test()
 */
typedef enum {
	ALE_ALLOCFAIL = 0,	/* Memory allocation error */
	ALE_EXISTS =	1,	/* alist entry already exists */
	ALE_NOTFND =	2,	/* item not found and insert not required */
	ALE_CREATE =	3	/* alist entry created */
} aplist_test_t;


/*
 * Access to an Alist item by index or offset. This is needed because the
 * size of an item in an Alist is not known by the C compiler, and we
 * have to do the indexing arithmetic explicitly.
 *
 * For an APlist, index the apl_data field directly --- No macro is needed.
 */
#define	alist_item(_lp, _idx) \
	((void *)(ALIST_OFF_DATA + ((_idx) * (_lp)->al_size) + (char *)(_lp)))
#define	alist_item_by_offset(_lp, _off) \
	((void *)((_off) + (char *)(_lp)))

/*
 * # of items currently found in a list. These macros handle the case
 * where the list has not been allocated yet.
 */
#define	alist_nitems(_lp) (((_lp) == NULL) ? 0 : (_lp)->al_nitems)
#define	aplist_nitems(_lp) (((_lp) == NULL) ? 0 : (_lp)->apl_nitems)


extern void		*alist_append(Alist **, const void *, size_t, Aliste);
extern void		alist_delete(Alist *, Aliste *);
extern void		alist_delete_by_offset(Alist *, Aliste *);
extern void		*alist_insert(Alist **, const void *, size_t,
			    Aliste, Aliste);
extern void		*alist_insert_by_offset(Alist **, const void *, size_t,
			    Aliste, Aliste);
extern void		alist_reset(Alist *);


extern void		*aplist_append(APlist **, const void *, Aliste);
extern void		aplist_delete(APlist *, Aliste *);
extern int		aplist_delete_value(APlist *, const void *);
extern void		*aplist_insert(APlist **, const void *,
			    Aliste, Aliste idx);
extern void		aplist_reset(APlist *);
extern aplist_test_t	aplist_test(APlist **, const void *, Aliste);

#ifdef	__cplusplus
}
#endif

#endif /* _ALIST_H */
