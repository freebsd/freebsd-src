/*
 * Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 2000, 2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: quota.c,v 1.11.12.3 2004/03/08 09:04:49 marka Exp $ */

#include <config.h>

#include <stddef.h>

#include <isc/quota.h>
#include <isc/util.h>

isc_result_t
isc_quota_init(isc_quota_t *quota, int max) {
	quota->max = max;
	quota->used = 0;
	quota->soft = ISC_FALSE;
	return (isc_mutex_init(&quota->lock));
}

void
isc_quota_destroy(isc_quota_t *quota) {
	INSIST(quota->used == 0);
	quota->max = -1;
	quota->used = -1;
	quota->soft = ISC_FALSE;
	DESTROYLOCK(&quota->lock);
}

void
isc_quota_soft(isc_quota_t *quota, isc_boolean_t soft) {
	quota->soft = soft;
}

isc_result_t
isc_quota_reserve(isc_quota_t *quota) {
	isc_result_t result;
	LOCK(&quota->lock);
	if (quota->used < quota->max) {
		quota->used++;
		result = ISC_R_SUCCESS;
	} else {
		if (quota->soft) {
			quota->used++;
			result = ISC_R_SOFTQUOTA;
		} else
			result = ISC_R_QUOTA;
	}
	UNLOCK(&quota->lock);
	return (result);
}

void
isc_quota_release(isc_quota_t *quota) {
	LOCK(&quota->lock);
	INSIST(quota->used > 0);
	quota->used--;
	UNLOCK(&quota->lock);
}

isc_result_t
isc_quota_attach(isc_quota_t *quota, isc_quota_t **p)
{
	isc_result_t result;
	INSIST(p != NULL && *p == NULL);
	result = isc_quota_reserve(quota);
	if (result == ISC_R_SUCCESS || result == ISC_R_SOFTQUOTA)
		*p = quota;
	return (result);
}

void
isc_quota_detach(isc_quota_t **p)
{
	INSIST(p != NULL && *p != NULL);
	isc_quota_release(*p);
	*p = NULL;
}
