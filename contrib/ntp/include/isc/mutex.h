/*
 * Copyright (C) 2000, 2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: mutex.h,v 1.3 2001/01/09 21:57:55 bwelling Exp $ */

#ifndef ISC_MUTEX_H
#define ISC_MUTEX_H 1

#include <isc/result.h>		/* for ISC_R_ codes */

typedef int isc_mutex_t;

#define isc_mutex_init(mp) \
	(*(mp) = 0, ISC_R_SUCCESS)
#define isc_mutex_lock(mp) \
	((*(mp))++ == 0 ? ISC_R_SUCCESS : ISC_R_UNEXPECTED)
#define isc_mutex_unlock(mp) \
	(--(*(mp)) == 0 ? ISC_R_SUCCESS : ISC_R_UNEXPECTED)
#define isc_mutex_trylock(mp) \
	(*(mp) == 0 ? ((*(mp))++, ISC_R_SUCCESS) : ISC_R_LOCKBUSY)
#define isc_mutex_destroy(mp) \
	(*(mp) == 0 ? (*(mp) = -1, ISC_R_SUCCESS) : ISC_R_UNEXPECTED)
#define isc_mutex_stats(fp)

#endif /* ISC_MUTEX_H */
