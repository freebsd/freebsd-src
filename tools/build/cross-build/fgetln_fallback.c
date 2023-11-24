/*
 * Copyright © 2005 Hector Garcia Alvarez
 * Copyright © 2005, 2008-2012 Guillem Jover <guillem@hadrons.org>
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

#include <stdio.h>

#include <sys/types.h>
#include <string.h>

#include "local-link.h"

#define HAVE_GETLINE 1
#ifdef HAVE_GETLINE
struct filebuf {
	FILE *fp;
	char *buf;
	size_t len;
};

#define FILEBUF_POOL_ITEMS 32

static struct filebuf fb_pool[FILEBUF_POOL_ITEMS];
static int fb_pool_cur;

char *
fgetln(FILE *stream, size_t *len)
{
	struct filebuf *fb;
	ssize_t nread;

	flockfile(stream);

	/* Try to diminish the possibility of several fgetln() calls being
	 * used on different streams, by using a pool of buffers per file. */
	fb = &fb_pool[fb_pool_cur];
	if (fb->fp != stream && fb->fp != NULL) {
		fb_pool_cur++;
		fb_pool_cur %= FILEBUF_POOL_ITEMS;
		fb = &fb_pool[fb_pool_cur];
	}
	fb->fp = stream;

	nread = getline(&fb->buf, &fb->len, stream);

	funlockfile(stream);

	/* Note: the getdelim/getline API ensures nread != 0. */
	if (nread == -1) {
		*len = 0;
		return NULL;
	} else {
		*len = (size_t)nread;
		return fb->buf;
	}
}
libbsd_link_warning(fgetln,
                    "This function cannot be safely ported, use getline(3) "
                    "instead, as it is supported by GNU and POSIX.1-2008.")
#else
#error "Function fgetln() needs to be ported."
#endif
