/*
 * Copyright (c) 2005, PADL Software Pty Ltd.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of PADL Software nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PADL SOFTWARE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL PADL SOFTWARE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "kcm_locl.h"

RCSID("$Id: cursor.c 17447 2006-05-05 10:52:01Z lha $");

krb5_error_code
kcm_cursor_new(krb5_context context,
	       pid_t pid,
	       kcm_ccache ccache,
	       uint32_t *cursor)
{
    kcm_cursor **p;
    krb5_error_code ret;

    *cursor = 0;

    KCM_ASSERT_VALID(ccache);

    HEIMDAL_MUTEX_lock(&ccache->mutex);
    for (p = &ccache->cursors; *p != NULL; p = &(*p)->next)
	;

    *p = (kcm_cursor *)malloc(sizeof(kcm_cursor));
    if (*p == NULL) {
	ret = KRB5_CC_NOMEM;
	goto out;
    }

    (*p)->pid = pid;
    (*p)->key = ++ccache->n_cursor;
    (*p)->credp = ccache->creds;
    (*p)->next = NULL;

    *cursor = (*p)->key;

    ret = 0;

out:
    HEIMDAL_MUTEX_unlock(&ccache->mutex);

    return ret;
}

krb5_error_code
kcm_cursor_find(krb5_context context,
		pid_t pid,
		kcm_ccache ccache,
		uint32_t key,
		kcm_cursor **cursor)
{
    kcm_cursor *p;
    krb5_error_code ret;

    KCM_ASSERT_VALID(ccache);

    if (key == 0)
	return KRB5_CC_NOTFOUND;

    ret = KRB5_CC_END;

    HEIMDAL_MUTEX_lock(&ccache->mutex);

    for (p = ccache->cursors; p != NULL; p = p->next) {
	if (p->key == key) {
	    if (p->pid != pid)
		ret = KRB5_FCC_PERM;
	    else
		ret = 0;
	    break;
	}
    }

    if (ret == 0)
	*cursor = p;

    HEIMDAL_MUTEX_unlock(&ccache->mutex);

    return ret;
}

krb5_error_code
kcm_cursor_delete(krb5_context context,
	     	  pid_t pid,
		  kcm_ccache ccache,
		  uint32_t key)
{
    kcm_cursor **p;
    krb5_error_code ret;

    KCM_ASSERT_VALID(ccache);

    if (key == 0)
	return KRB5_CC_NOTFOUND;

    ret = KRB5_CC_END;

    HEIMDAL_MUTEX_lock(&ccache->mutex);

    for (p = &ccache->cursors; *p != NULL; p = &(*p)->next) {
	if ((*p)->key == key) {
	    if ((*p)->pid != pid)
		ret = KRB5_FCC_PERM;
	    else
		ret = 0;
	    break;
	}
    }

    if (ret == 0) {
	kcm_cursor *x = *p;

	*p = x->next;
	free(x);
    }

    HEIMDAL_MUTEX_unlock(&ccache->mutex);

    return ret;
}

