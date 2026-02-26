/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software were developed by Konstantin Belousov <kib@FreeBSD.org>
 * under sponsorship from the FreeBSD Foundation.
 */

#include <sys/param.h>
#include <sys/exterr_cat.h>
#include <sys/exterrvar.h>
#include <exterr.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char * const cat_to_filenames[] = {
#include "exterr_cat_filenames.h"
};

static const char *
cat_to_filename(int category)
{
	if (category < 0 || category >= nitems(cat_to_filenames) ||
	    cat_to_filenames[category] == NULL)
		return ("unknown");
	return (cat_to_filenames[category]);
}

static const char exterror_verbose_name[] = "EXTERROR_VERBOSE";
enum exterr_verbose_state {
	EXTERR_VERBOSE_UNKNOWN = 100,
	EXTERR_VERBOSE_DEFAULT,
	EXTERR_VERBOSE_ALLOW_BRIEF,
	EXTERR_VERBOSE_ALLOW_FULL,
};
static enum exterr_verbose_state exterror_verbose = EXTERR_VERBOSE_UNKNOWN;

static void
exterr_verbose_init(void)
{
	const char *v;

	/*
	 * No need to care about thread-safety, the result is
	 * idempotent.
	 */
	if (exterror_verbose != EXTERR_VERBOSE_UNKNOWN)
		return;
	if (issetugid()) {
		exterror_verbose = EXTERR_VERBOSE_DEFAULT;
	} else if ((v = getenv(exterror_verbose_name)) != NULL) {
		exterror_verbose = strcmp(v, "brief") == 0 ?
		    EXTERR_VERBOSE_ALLOW_BRIEF : EXTERR_VERBOSE_ALLOW_FULL;
	} else {
		exterror_verbose = EXTERR_VERBOSE_DEFAULT;
	}
}

int
__uexterr_format(const struct uexterror *ue, char *buf, size_t bufsz)
{
	bool has_msg;

	if (bufsz > UEXTERROR_MAXLEN)
		bufsz = UEXTERROR_MAXLEN;
	if (ue->error == 0) {
		strlcpy(buf, "", bufsz);
		return (0);
	}
	exterr_verbose_init();
	has_msg = ue->msg[0] != '\0';

	if (has_msg) {
		snprintf(buf, bufsz, ue->msg, (uintmax_t)ue->p1,
		    (uintmax_t)ue->p2);
	} else {
		strlcpy(buf, "", bufsz);
	}

	if (exterror_verbose > EXTERR_VERBOSE_DEFAULT || !has_msg) {
		char lbuf[128];

#define	SRC_FMT "(src sys/%s:%u)"
		if (exterror_verbose == EXTERR_VERBOSE_ALLOW_BRIEF) {
			snprintf(lbuf, sizeof(lbuf), SRC_FMT,
                            cat_to_filename(ue->cat), ue->src_line);
		} else if (!has_msg ||
		    exterror_verbose == EXTERR_VERBOSE_ALLOW_FULL) {
			snprintf(lbuf, sizeof(lbuf),
			    "errno %d category %u " SRC_FMT " p1 %#jx p2 %#jx",
			    ue->error, ue->cat, cat_to_filename(ue->cat),
			    ue->src_line, (uintmax_t)ue->p1, (uintmax_t)ue->p2);
		}
#undef SRC_FMT
		if (has_msg)
			strlcat(buf, " ", bufsz);
		strlcat(buf, lbuf, bufsz);
	}
	return (0);
}
