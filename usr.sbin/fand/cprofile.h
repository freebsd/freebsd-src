/*-
 * Copyright (c) 2026 Stéphane Rochoy <sheda@FreeBSD.org>
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef CPROFILE_H
#define CPROFILE_H

#include <stddef.h>

#define MAX_CSTEPS 10

typedef struct cstep {
	float        raw_temp; /* multiple of °C or K depending on offset and multiplier */
	unsigned int duty;     /* nanoseconds */
} cstep_t;

typedef struct cprofile {
	unsigned int  lo_duty; /* nanoseconds */
	size_t        step_count;
	cstep_t       steps[MAX_CSTEPS];
} cprofile_t;

extern cprofile_t cprofile;

int parse_cprofile(const char *path);

#endif /* CPROFILE_H */
