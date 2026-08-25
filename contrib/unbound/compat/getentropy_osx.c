/*	$OpenBSD: getentropy_osx.c,v 1.12 2018/11/20 08:04:28 deraadt Exp $	*/

/*
 * Copyright (c) 2014 Theo de Raadt <deraadt@openbsd.org>
 * Copyright (c) 2014 Bob Beck <beck@obtuse.com>
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
 *
 * Emulation of getentropy(2) as documented at:
 * http://man.openbsd.org/getentropy.2
 */

/* Modified to use SecRandomCopyBytes. It is from macOS 10.7 (2011) and
 * iOS 2.0 (2008), and is the primary API for cryptographic random numbers. */
#include <errno.h>
#include <Security/SecRandom.h>

int	getentropy(void *buf, size_t len);

int
getentropy(void *buf, size_t len)
{
	if (len > 256) {
		goto error;
	}

	if (SecRandomCopyBytes(kSecRandomDefault, len, buf) == errSecSuccess) {
		return 0;
	}

error:
	errno = EIO;
	return -1;
}
