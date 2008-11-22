/*
 * Portions Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
 * Portions Copyright (C) 2001, 2003  Internet Software Consortium.
 * Portions Copyright (C) 2001  Nominum, Inc.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC AND NOMINUM DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: result.c,v 1.3.2.2.2.1 2004/03/06 08:15:19 marka Exp $ */

#include <config.h>

#include <isc/once.h>
#include <isc/util.h>

#include <isccc/result.h>
#include <isccc/lib.h>

static const char *text[ISCCC_R_NRESULTS] = {
	"unknown version",			/* 1 */
	"syntax error",				/* 2 */
	"bad auth",				/* 3 */
	"expired",				/* 4 */
	"clock skew",				/* 5 */
	"duplicate"				/* 6 */
};

#define ISCCC_RESULT_RESULTSET			2

static isc_once_t		once = ISC_ONCE_INIT;

static void
initialize_action(void) {
	isc_result_t result;

	result = isc_result_register(ISC_RESULTCLASS_ISCCC, ISCCC_R_NRESULTS,
				     text, isccc_msgcat,
				     ISCCC_RESULT_RESULTSET);
	if (result != ISC_R_SUCCESS)
		UNEXPECTED_ERROR(__FILE__, __LINE__,
				 "isc_result_register() failed: %u", result);
}

static void
initialize(void) {
	isccc_lib_initmsgcat();
	RUNTIME_CHECK(isc_once_do(&once, initialize_action) == ISC_R_SUCCESS);
}

const char *
isccc_result_totext(isc_result_t result) {
	initialize();

	return (isc_result_totext(result));
}

void
isccc_result_register(void) {
	initialize();
}
