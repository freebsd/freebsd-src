/*
 *  Copyright (C) 2017 - This file is part of libecc project
 *
 *  Authors:
 *      Ryad BENADJILA <ryadbenadjila@gmail.com>
 *      Arnaud EBALARD <arnaud.ebalard@ssi.gouv.fr>
 *      Jean-Pierre FLORI <jean-pierre.flori@ssi.gouv.fr>
 *
 *  Contributors:
 *      Nicolas VIVET <nicolas.vivet@ssi.gouv.fr>
 *      Karim KHALFALLAH <karim.khalfallah@ssi.gouv.fr>
 *
 *  This software is licensed under a dual BSD and GPL v2 license.
 *  See LICENSE file at the root folder of the project.
 */

#include <libecc/external_deps/time.h>

/* Unix and compatible case (including macOS) */
#if defined(WITH_STDLIB) && (defined(__unix__) || defined(__APPLE__))
#include <stddef.h>
#include <sys/time.h>

int get_ms_time(u64 *time)
{
	struct timeval tv;
	int ret;

	if (time == NULL) {
		ret = -1;
		goto err;
	}

	ret = gettimeofday(&tv, NULL);
	if (ret < 0) {
		ret = -1;
		goto err;
	}

	(*time) = (u64)(((tv.tv_sec) * 1000) + ((tv.tv_usec) / 1000));
	ret = 0;

err:
	return ret;
}

/* Windows case */
#elif defined(WITH_STDLIB) && defined(__WIN32__)
#include <stddef.h>
#include <windows.h>
int get_ms_time(u64 *time)
{
	int ret;
	SYSTEMTIME st;

	if (time == NULL) {
		ret = -1;
		goto err;
	}

	GetSystemTime(&st);
	(*time) = (u64)((((st.wMinute * 60) + st.wSecond) * 1000) + st.wMilliseconds);
	ret = 0;

err:
	return ret;
}

/* No platform detected, the used must provide an implementation! */
#else
#error "time.c: you have to implement get_ms_time()"
#endif
