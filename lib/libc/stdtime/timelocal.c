/*-
 * Copyright (c) 2001 Alexey Zelkin
 * Copyright (c) 1997 FreeBSD Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <stddef.h>

#include "ldpart.h"
#include "timelocal.h"

static struct lc_time_T _time_locale;
static int _time_using_locale;
static char * time_locale_buf;

#define LCTIME_SIZE_FULL (sizeof(struct lc_time_T) / sizeof(char *))
#define LCTIME_SIZE_MIN \
		(offsetof(struct lc_time_T, ampm_fmt) / sizeof(char *))

static const struct lc_time_T	_C_time_locale = {
	{
		"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
	}, {
		"January", "February", "March", "April", "May", "June",
		"July", "August", "September", "October", "November", "December"
	}, {
		"Sun", "Mon", "Tue", "Wed",
		"Thu", "Fri", "Sat"
	}, {
		"Sunday", "Monday", "Tuesday", "Wednesday",
		"Thursday", "Friday", "Saturday"
	},

	/* X_fmt */
	"%H:%M:%S",

	/*
	** x_fmt
	** Since the C language standard calls for
	** "date, using locale's date format," anything goes.
	** Using just numbers (as here) makes Quakers happier;
	** it's also compatible with SVR4.
	*/
	"%m/%d/%y",

	/*
	** c_fmt (ctime-compatible)
	*/
	"%a %Ef %T %Y",

	/* am */
	"AM",

	/* pm */
	"PM",

	/* date_fmt */
	"%a %Ef %X %Z %Y",
	
	{
		"January", "February", "March", "April", "May", "June",
		"July", "August", "September", "October", "November", "December"
	},

	/* Ef_fmt
	** To determine short months / day order
	*/
	"%b %e",

	/* EF_fmt
	** To determine long months / day order
	*/
	"%B %e",

	/* ampm_fmt
	** To determine 12-hour clock format time (empty, if N/A)
	*/
	"%I:%M:%S %p"
};

struct lc_time_T *
__get_current_time_locale(void) {
	return (_time_using_locale
		? &_time_locale
		: (struct lc_time_T *)&_C_time_locale);
}

int
__time_load_locale(const char *name) {

	int	ret;

	_time_locale.ampm_fmt = _C_time_locale.ampm_fmt;

	ret = __part_load_locale(name, &_time_using_locale,
			time_locale_buf, "LC_TIME",
			LCTIME_SIZE_FULL, LCTIME_SIZE_MIN,
			(const char **)&_time_locale);

	/* XXX: always overwrite for ctime format parsing compatibility */
	_time_locale.c_fmt = _C_time_locale.c_fmt;

	return (ret);
}
