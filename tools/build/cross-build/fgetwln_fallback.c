/*
 * Copyright 2012 Guillem Jover <guillem@hadrons.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>

#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>

#include "local-link.h"

struct filewbuf {
	FILE *fp;
	wchar_t *wbuf;
	size_t len;
};

#define FILEWBUF_INIT_LEN	128
#define FILEWBUF_POOL_ITEMS	32

static struct filewbuf fb_pool[FILEWBUF_POOL_ITEMS];
static int fb_pool_cur;

wchar_t *
fgetwln(FILE *stream, size_t *lenp)
{
	struct filewbuf *fb;
	wint_t wc;
	size_t wused = 0;

	/* Try to diminish the possibility of several fgetwln() calls being
	 * used on different streams, by using a pool of buffers per file. */
	fb = &fb_pool[fb_pool_cur];
	if (fb->fp != stream && fb->fp != NULL) {
		fb_pool_cur++;
		fb_pool_cur %= FILEWBUF_POOL_ITEMS;
		fb = &fb_pool[fb_pool_cur];
	}
	fb->fp = stream;

	while ((wc = fgetwc(stream)) != WEOF) {
		if (!fb->len || wused >= fb->len) {
			wchar_t *wp;

			if (fb->len)
				fb->len *= 2;
			else
				fb->len = FILEWBUF_INIT_LEN;

			wp = reallocarray(fb->wbuf, fb->len, sizeof(wchar_t));
			if (wp == NULL) {
				wused = 0;
				break;
			}
			fb->wbuf = wp;
		}

		fb->wbuf[wused++] = wc;

		if (wc == L'\n')
			break;
	}

	*lenp = wused;
	return wused ? fb->wbuf : NULL;
}

libbsd_link_warning(fgetwln,
                    "This function cannot be safely ported, use fgetwc(3) "
                    "instead, as it is supported by C99 and POSIX.1-2001.")
