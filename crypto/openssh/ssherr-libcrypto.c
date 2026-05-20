/* $OpenBSD: ssherr-libcrypto.c,v 1.1 2026/02/06 23:31:29 dtucker Exp $ */
/*
 * Copyright (c) 2026 Darren Tucker
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "includes.h"

#include <sys/types.h>

#include <errno.h>
#include <string.h>

#include "log.h"

#ifdef WITH_OPENSSL
#include <openssl/err.h>

const char *
ssherr_libcrypto(void)
{
	unsigned long e;
	static char buf[512];
	char msg[4096];
	const char *reason = NULL, *file, *data;
	int ln, fl;

	ERR_load_crypto_strings();
	while ((e = ERR_get_error_line_data(&file, &ln, &data, &fl)) != 0) {
		ERR_error_string_n(e, buf, sizeof(buf));
		snprintf(msg, sizeof(msg), "%s:%s:%d:%s", buf, file, ln,
		    (fl & ERR_TXT_STRING) ? data : "");
		debug("libcrypto: '%s'", msg);
		if ((reason = ERR_reason_error_string(e)) != NULL)
			snprintf(buf, sizeof(buf), "error in libcrypto: %s",
			    reason);
	}
	if (reason == NULL)
		return NULL;
	return buf;
}
#else
const char *
ssherr_libcrypto(void)
{
	return NULL;
}
#endif
