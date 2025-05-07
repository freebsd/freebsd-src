/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_dl.h>

#include <assert.h>
#include <stdint.h>
#include <string.h>

/* States*/
#define NAMING	0
#define GOTONE	1
#define GOTTWO	2
#define RESET	3
/* Inputs */
#define	DIGIT	(4*0)
#define	END	(4*1)
#define DELIM	(4*2)
#define LETTER	(4*3)

void
link_addr(const char *addr, struct sockaddr_dl *sdl)
{
	char *cp = sdl->sdl_data;
	char *cplim = sdl->sdl_len + (char *)sdl;
	int byte = 0, state = NAMING, new;

	bzero((char *)&sdl->sdl_family, sdl->sdl_len - 1);
	sdl->sdl_family = AF_LINK;
	do {
		state &= ~LETTER;
		if ((*addr >= '0') && (*addr <= '9')) {
			new = *addr - '0';
		} else if ((*addr >= 'a') && (*addr <= 'f')) {
			new = *addr - 'a' + 10;
		} else if ((*addr >= 'A') && (*addr <= 'F')) {
			new = *addr - 'A' + 10;
		} else if (*addr == 0) {
			state |= END;
		} else if (state == NAMING &&
			   (((*addr >= 'A') && (*addr <= 'Z')) ||
			   ((*addr >= 'a') && (*addr <= 'z'))))
			state |= LETTER;
		else
			state |= DELIM;
		addr++;
		switch (state /* | INPUT */) {
		case NAMING | DIGIT:
		case NAMING | LETTER:
			*cp++ = addr[-1];
			continue;
		case NAMING | DELIM:
			state = RESET;
			sdl->sdl_nlen = cp - sdl->sdl_data;
			continue;
		case GOTTWO | DIGIT:
			*cp++ = byte;
			/* FALLTHROUGH */
		case RESET | DIGIT:
			state = GOTONE;
			byte = new;
			continue;
		case GOTONE | DIGIT:
			state = GOTTWO;
			byte = new + (byte << 4);
			continue;
		default: /* | DELIM */
			state = RESET;
			*cp++ = byte;
			byte = 0;
			continue;
		case GOTONE | END:
		case GOTTWO | END:
			*cp++ = byte;
			/* FALLTHROUGH */
		case RESET | END:
			break;
		}
		break;
	} while (cp < cplim);
	sdl->sdl_alen = cp - LLADDR(sdl);
	new = cp - (char *)sdl;
	if (new > sizeof(*sdl))
		sdl->sdl_len = new;
	return;
}

char *
link_ntoa(const struct sockaddr_dl *sdl)
{
	static char obuf[64];
	size_t buflen;
	_Static_assert(sizeof(obuf) >= IFNAMSIZ + 20, "obuf is too small");

	/*
	 * Ignoring the return value of link_ntoa_r() is safe here because it
	 * always writes the terminating NUL.  This preserves the traditional
	 * behaviour of link_ntoa().
	 */
	buflen = sizeof(obuf);
	(void)link_ntoa_r(sdl, obuf, &buflen);
	return obuf;
}

int
link_ntoa_r(const struct sockaddr_dl *sdl, char *obuf, size_t *buflen)
{
	static const char hexlist[] = "0123456789abcdef";
	char *out;
	const u_char *in, *inlim;
	int namelen, i, rem;
	size_t needed;

	assert(sdl);
	assert(buflen);
	/* obuf may be null */

	needed = 1; /* 1 for the NUL */
	out = obuf;
	if (obuf)
		rem = *buflen;
	else
		rem = 0;

/*
 * Check if at least n bytes are available in the output buffer, plus 1 for the
 * trailing NUL.  If not, set rem = 0 so we stop writing.
 * Either way, increment needed by the amount we would have written.
 */
#define CHECK(n) do {				\
		if ((SIZE_MAX - (n)) >= needed)	\
			needed += (n);		\
		if (rem >= ((n) + 1))		\
			rem -= (n);		\
		else				\
			rem = 0;		\
	} while (0)

/*
 * Write the char c to the output buffer, unless the buffer is full.
 * Note that if obuf is NULL, rem is always zero.
 */
#define OUT(c) do {			\
		if (rem > 0)		\
			*out++ = (c);	\
	} while (0)

	namelen = (sdl->sdl_nlen <= IFNAMSIZ) ? sdl->sdl_nlen : IFNAMSIZ;
	if (namelen > 0) {
		CHECK(namelen);
		if (rem > 0) {
			bcopy(sdl->sdl_data, out, namelen);
			out += namelen;
		}

		if (sdl->sdl_alen > 0) {
			CHECK(1);
			OUT(':');
		}
	}

	in = (const u_char *)LLADDR(sdl);
	inlim = in + sdl->sdl_alen;

	while (in < inlim) {
		if (in != (const u_char *)LLADDR(sdl)) {
			CHECK(1);
			OUT('.');
		}
		i = *in++;
		if (i > 0xf) {
			CHECK(2);
			OUT(hexlist[i >> 4]);
			OUT(hexlist[i & 0xf]);
		} else {
			CHECK(1);
			OUT(hexlist[i]);
		}
	}

#undef CHECK
#undef OUT

	/*
	 * We always leave enough room for the NUL if possible, but the user
	 * might have passed a NULL or zero-length buffer.
	 */
	if (out && *buflen)
		*out = '\0';

	*buflen = needed;
	return ((rem > 0) ? 0 : -1);
}
