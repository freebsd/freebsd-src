/*
 * Copyright 2006, 2007 Daniel Silverstone <dsilvers@digital-scurf.org>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/** \file
 * Ring list structure.
 *
 * Rings are structures which have an r_next pointer and an r_prev
 * pointer which are always initialised and always point at the next
 * or previous element respectively.
 *
 * The degenerate case of a single element in the ring simply points
 * at itself in both directions. A zero element ring is NULL.
 *
 * Some of the ring functions are specific to the fetcher but may be
 * of use to others and are thus included here.
 */

#ifndef _NETSURF_UTILS_RING_H_
#define _NETSURF_UTILS_RING_H_


/** Insert the given item into the specified ring.
 * Assumes that the element is zeroed as appropriate.
 */
#define RING_INSERT(ring,element) \
	/*LOG(("RING_INSERT(%s, %p(%s))", #ring, element, element->host));*/ \
	if (ring) { \
		element->r_next = ring; \
		element->r_prev = ring->r_prev; \
		ring->r_prev = element; \
		element->r_prev->r_next = element; \
	} else \
		ring = element->r_prev = element->r_next = element

/** Remove the given element from the specified ring.
 * Will zero the element as needed
 */
#define RING_REMOVE(ring, element) \
	/*LOG(("RING_REMOVE(%s, %p(%s)", #ring, element, element->host));*/ \
	if (element->r_next != element ) { \
		/* Not the only thing in the ring */ \
		element->r_next->r_prev = element->r_prev; \
		element->r_prev->r_next = element->r_next; \
		if (ring == element) ring = element->r_next; \
	} else { \
		/* Only thing in the ring */ \
		ring = 0; \
	} \
	element->r_next = element->r_prev = 0

/** Find the element (by hostname) in the given ring, leave it in the
 * provided element variable
 */
#define RING_FINDBYHOST(ring, element, hostname) \
	/*LOG(("RING_FINDBYHOST(%s, %s)", #ring, hostname));*/ \
	if (ring) { \
		bool found = false; \
		element = ring; \
		do { \
			if (strcasecmp(element->host, hostname) == 0) { \
				found = true; \
				break; \
			} \
			element = element->r_next; \
		} while (element != ring); \
		if (!found) element = 0; \
	} else element = 0

/** Find the element (by hostname) in the given ring, leave it in the
 * provided element variable
 */
#define RING_FINDBYLWCHOST(ring, element, lwc_hostname) \
	/*LOG(("RING_FINDBYHOST(%s, %s)", #ring, hostname));*/ \
	if (ring) { \
		bool found = false; \
		element = ring; \
		do { \
			if (lwc_string_isequal(element->host, lwc_hostname, \
					&found) == lwc_error_ok && \
					found == true) { \
				break; \
			} \
			element = element->r_next; \
		} while (element != ring); \
		if (!found) element = 0; \
	} else element = 0

/** Measure the size of a ring and put it in the supplied variable */
#define RING_GETSIZE(ringtype, ring, sizevar) \
	/*LOG(("RING_GETSIZE(%s)", #ring));*/ \
	if (ring) { \
		ringtype *p = ring; \
		sizevar = 0; \
		do { \
			sizevar++; \
			p = p->r_next; \
		} while (p != ring); \
	} else sizevar = 0

/** Count the number of elements in the ring which match the provided hostname */
#define RING_COUNTBYHOST(ringtype, ring, sizevar, hostname) \
	/*LOG(("RING_COUNTBYHOST(%s, %s)", #ring, hostname));*/ \
	if (ring) { \
		ringtype *p = ring; \
		sizevar = 0; \
		do { \
			if (strcasecmp(p->host, hostname) == 0) \
				sizevar++; \
			p = p->r_next; \
		} while (p != ring); \
	} else sizevar = 0

/** Count the number of elements in the ring which match the provided lwc_hostname */
#define RING_COUNTBYLWCHOST(ringtype, ring, sizevar, lwc_hostname) \
	/*LOG(("RING_COUNTBYHOST(%s, %s)", #ring, hostname));*/ \
	if (ring) { \
		ringtype *p = ring; \
		sizevar = 0; \
		do { \
			bool matches = false; \
			/* nsurl guarantees lowercase host */ \
			if (lwc_string_isequal(p->host, lwc_hostname, \
					&matches) == lwc_error_ok) \
                		if (matches) \
					sizevar++; \
			p = p->r_next; \
		} while (p != ring); \
	} else sizevar = 0

/*
 * Ring iteration works as follows:
 *
 * RING_ITERATE_START(ringtype, ring, iteratorptr) {
 *    code_using(iteratorptr);
 * } RING_ITERATE_END(ring, iteratorptr);
 *
 * If you want to stop iterating (e.g. you found your answer)
 * RING_ITERATE_STOP(ring, iteratorptr);
 * You *MUST* abort the iteration if you do something to modify
 * the ring such as deleting or adding an element.
 */

#define RING_ITERATE_START(ringtype, ring, iteratorptr) \
	if (ring != NULL) {				\
		ringtype *iteratorptr = ring;		\
		do {					\
			do {				\
			
#define RING_ITERATE_STOP(ring, iteratorptr)	\
	goto iteration_end_ring##_##iteratorptr

#define RING_ITERATE_END(ring, iteratorptr)		\
			} while (false);		\
			iteratorptr = iteratorptr->r_next;	\
		} while (iteratorptr != ring);		\
	}						\
	iteration_end_ring##_##iteratorptr:

#endif
