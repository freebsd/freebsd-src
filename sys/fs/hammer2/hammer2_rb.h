/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2022 Tomohiro Kusumi <tkusumi@netbsd.org>
 * Copyright (c) 2011-2022 The DragonFly Project.  All rights reserved.
 *
 * This code is derived from software contributed to The DragonFly Project
 * by Matthew Dillon <dillon@dragonflybsd.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of The DragonFly Project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific, prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _FS_HAMMER2_RB_H_
#define _FS_HAMMER2_RB_H_

/* prototype */
#define RB_SCAN_INFO(name, type)					\
struct name##_scan_info {						\
	struct name##_scan_info *link;					\
	struct type	*node;						\
}

#define RB_PROTOTYPE_SCAN(name, type, field)				\
	_RB_PROTOTYPE_SCAN(name, type, field,)

#define RB_PROTOTYPE_SCAN_STATIC(name, type, field)			\
	_RB_PROTOTYPE_SCAN(name, type, field, __unused static)

#define _RB_PROTOTYPE_SCAN(name, type, field, STORQUAL)			\
STORQUAL int name##_RB_SCAN(struct name *, int (*)(struct type *, void *),\
			int (*)(struct type *, void *), void *);	\
RB_SCAN_INFO(name, type)

/* generate */
#define RB_GENERATE_SCAN(name, type, field)				\
	_RB_GENERATE_SCAN(name, type, field,)

#define RB_GENERATE_SCAN_STATIC(name, type, field)			\
	_RB_GENERATE_SCAN(name, type, field, __unused static)

#define _RB_GENERATE_SCAN(name, type, field, STORQUAL)			\
/*									\
 * Issue a callback for all matching items.  The scan function must	\
 * return < 0 for items below the desired range, 0 for items within	\
 * the range, and > 0 for items beyond the range.   Any item may be	\
 * deleted while the scan is in progress.				\
 */									\
static int								\
name##_SCANCMP_ALL(struct type *type __unused, void *data __unused)	\
{									\
	return (0);							\
}									\
									\
static __inline int							\
_##name##_RB_SCAN(struct name *head,					\
		int (*scancmp)(struct type *, void *),			\
		int (*callback)(struct type *, void *),			\
		void *data)						\
{									\
	struct name##_scan_info info;					\
	struct type *best;						\
	struct type *tmp;						\
	int count;							\
	int comp;							\
									\
	if (scancmp == NULL)						\
		scancmp = name##_SCANCMP_ALL;				\
									\
	/*								\
	 * Locate the first element.					\
	 */								\
	tmp = RB_ROOT(head);						\
	best = NULL;							\
	while (tmp) {							\
		comp = scancmp(tmp, data);				\
		if (comp < 0) {						\
			tmp = RB_RIGHT(tmp, field);			\
		} else if (comp > 0) {					\
			tmp = RB_LEFT(tmp, field);			\
		} else {						\
			best = tmp;					\
			if (RB_LEFT(tmp, field) == NULL)		\
				break;					\
			tmp = RB_LEFT(tmp, field);			\
		}							\
	}								\
	count = 0;							\
	if (best) {							\
		info.node = RB_NEXT(name, head, best);			\
		while ((comp = callback(best, data)) >= 0) {		\
			count += comp;					\
			best = info.node;				\
			if (best == NULL || scancmp(best, data) != 0)	\
				break;					\
			info.node = RB_NEXT(name, head, best);		\
		}							\
		if (comp < 0)	/* error or termination */		\
			count = comp;					\
	}								\
	return (count);							\
}									\
									\
STORQUAL int								\
name##_RB_SCAN(struct name *head,					\
		int (*scancmp)(struct type *, void *),			\
		int (*callback)(struct type *, void *),			\
		void *data)						\
{									\
	return _##name##_RB_SCAN(head, scancmp, callback, data);	\
}

#define RB_SCAN(name, root, cmp, callback, data) 			\
				name##_RB_SCAN(root, cmp, callback, data)

#endif /* !_FS_HAMMER2_RB_H_ */
