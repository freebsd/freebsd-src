/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 The FreeBSD Foundation
 * Copyright (c) 2026 Capabilities Limited
 * All rights reserved.
 *
 * This software were developed by Konstantin Belousov <kib@FreeBSD.org>
 * under sponsorship from the FreeBSD Foundation.
 *
 * This software was developed by Capabilities Limited with funding from
 * Innovate UK and the Department for Science, Innovation and Technology
 * for the adoption and diffusion of CHERI technology under project
 * 10168042 (“CheriBSD feature extraction, maturity, and testing”).
 */

#ifndef _UEXTERROR_H_
#define	_UEXTERROR_H_

#include <sys/types.h>
#include <sys/linker_set.h>

struct exterr_cat {
	unsigned int	cat;
	const char	*file;
};

#ifndef UEXTERR_CATEGORY
#error "Specify error category before including uexterror.h"
#endif

#ifndef NO_UEXTERR_STRINGS
static struct exterr_cat __dynamic_cat = { .file = UEXTERR_CATEGORY };
DATA_WSET(exterr_cats, __dynamic_cat);
#define	_UEXTERR_CATEGORY						\
	(__dynamic_cat.cat | EXTERR_CAT_SRC_USER)
#else
#define	_UEXTERR_CATEGORY	(EXTERR_CAT_NONE | EXTERR_CAT_SRC_USER)
#endif


#ifdef NO_UEXTERR_STRINGS
#define	SET_ERROR_MSG(mmsg)	NULL
#else
#define	SET_ERROR_MSG(mmsg)	(mmsg)
#endif

#define	_SET_ERROR2(eerror, mmsg, pp1, pp2)				\
	uexterr_set(eerror, _UEXTERR_CATEGORY, SET_ERROR_MSG(mmsg),	\
	    (uint64ptr_t)(pp1), (uint64ptr_t)(pp2), __LINE__)
#define	_SET_ERROR0(eerror, mmsg)	_SET_ERROR2(eerror, mmsg, 0, 0)
#define	_SET_ERROR1(eerror, mmsg, pp1)	_SET_ERROR2(eerror, mmsg, pp1, 0)

#define	_UEXTERROR_MACRO(eerror, mmsg, _1, _2, NAME, ...)		\
	NAME
#define	UEXTERROR(...)							\
	_UEXTERROR_MACRO(__VA_ARGS__, _SET_ERROR2, _SET_ERROR1,		\
	    _SET_ERROR0)(__VA_ARGS__)

__BEGIN_DECLS
void uexterr_set(int error, int category, const char *mmsg, uint64ptr_t pp1,
    uint64ptr_t pp2, int line);
__END_DECLS

#endif
