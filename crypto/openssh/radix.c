/*
 * Copyright (c) 1999 Dug Song.  All rights reserved.
 * Copyright (c) 2002 Markus Friedl.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "includes.h"
#include "uuencode.h"

RCSID("$OpenBSD: radix.c,v 1.21 2002/06/19 00:27:55 deraadt Exp $");

#ifdef AFS
#include <krb.h>

#include <radix.h>
#include "bufaux.h"

int
creds_to_radix(CREDENTIALS *creds, u_char *buf, size_t buflen)
{
	Buffer b;
	int ret;

	buffer_init(&b);

	buffer_put_char(&b, 1);	/* version */

	buffer_append(&b, creds->service, strlen(creds->service));
	buffer_put_char(&b, '\0');
	buffer_append(&b, creds->instance, strlen(creds->instance));
	buffer_put_char(&b, '\0');
	buffer_append(&b, creds->realm, strlen(creds->realm));
	buffer_put_char(&b, '\0');
	buffer_append(&b, creds->pname, strlen(creds->pname));
	buffer_put_char(&b, '\0');
	buffer_append(&b, creds->pinst, strlen(creds->pinst));
	buffer_put_char(&b, '\0');

	/* Null string to repeat the realm. */
	buffer_put_char(&b, '\0');

	buffer_put_int(&b, creds->issue_date);
	buffer_put_int(&b, krb_life_to_time(creds->issue_date,
	    creds->lifetime));
	buffer_append(&b, creds->session, sizeof(creds->session));
	buffer_put_short(&b, creds->kvno);

	/* 32 bit size + data */
	buffer_put_string(&b, creds->ticket_st.dat, creds->ticket_st.length);

	ret = uuencode(buffer_ptr(&b), buffer_len(&b), (char *)buf, buflen);

	buffer_free(&b);
	return ret;
}

#define GETSTRING(b, t, tlen) \
	do { \
		int i, found = 0; \
		for (i = 0; i < tlen; i++) { \
			if (buffer_len(b) == 0) \
				goto done; \
			t[i] = buffer_get_char(b); \
			if (t[i] == '\0') { \
				found = 1; \
				break; \
			} \
		} \
		if (!found) \
			goto done; \
	} while(0)

int
radix_to_creds(const char *buf, CREDENTIALS *creds)
{
	Buffer b;
	char c, version, *space, *p;
	u_int endTime;
	int len, blen, ret;

	ret = 0;
	blen = strlen(buf);

	/* sanity check for size */
	if (blen > 8192)
		return 0;

	buffer_init(&b);
	space = buffer_append_space(&b, blen);

	/* check version and length! */
	len = uudecode(buf, space, blen);
	if (len < 1)
		goto done;

	version = buffer_get_char(&b);

	GETSTRING(&b, creds->service, sizeof creds->service);
	GETSTRING(&b, creds->instance, sizeof creds->instance);
	GETSTRING(&b, creds->realm, sizeof creds->realm);
	GETSTRING(&b, creds->pname, sizeof creds->pname);
	GETSTRING(&b, creds->pinst, sizeof creds->pinst);

	if (buffer_len(&b) == 0)
		goto done;

	/* Ignore possibly different realm. */
	while (buffer_len(&b) > 0 && (c = buffer_get_char(&b)) != '\0')
		;

	if (buffer_len(&b) == 0)
		goto done;

	creds->issue_date = buffer_get_int(&b);

	endTime = buffer_get_int(&b);
	creds->lifetime = krb_time_to_life(creds->issue_date, endTime);

	len = buffer_len(&b);
	if (len < sizeof(creds->session))
		goto done;
	memcpy(&creds->session, buffer_ptr(&b), sizeof(creds->session));
	buffer_consume(&b, sizeof(creds->session));

	creds->kvno = buffer_get_short(&b);

	p = buffer_get_string(&b, &len);
	if (len < 0 || len > sizeof(creds->ticket_st.dat))
		goto done;
	memcpy(&creds->ticket_st.dat, p, len);
	creds->ticket_st.length = len;

	ret = 1;
done:
	buffer_free(&b);
	return ret;
}
#endif /* AFS */
