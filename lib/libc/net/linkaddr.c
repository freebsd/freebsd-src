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
#include <errno.h>
#include <stdint.h>
#include <string.h>

int
link_addr(const char *addr, struct sockaddr_dl *sdl)
{
	char *cp = sdl->sdl_data;
	char *cplim = sdl->sdl_len + (char *)sdl;
	const char *nptr;
	size_t newsize;
	int error = 0;
	char delim = 0;

	/* Initialise the sdl to zero, except for sdl_len. */
	bzero((char *)&sdl->sdl_family, sdl->sdl_len - 1);
	sdl->sdl_family = AF_LINK;

	/*
	 * Everything up to the first ':' is the interface name.  Usually the
	 * ':' should always be present even if there's no interface name, but
	 * since this interface was poorly specified in the past, accept a
	 * missing colon as meaning no interface name.
	 */
	if ((nptr = strchr(addr, ':')) != NULL) {
		size_t namelen = nptr - addr;

		/* Ensure the sdl is large enough to store the name. */
		if (namelen > cplim - cp) {
			errno = ENOSPC;
			return (-1);
		}

		memcpy(cp, addr, namelen);
		cp += namelen;
		sdl->sdl_nlen = namelen;
		/* Skip the interface name and the colon. */
		addr += namelen + 1;
	}

	/*
	 * The remainder of the string should be hex digits representing the
	 * address, with optional delimiters.  Each two hex digits form one
	 * octet, but octet output can be forced using a delimiter, so we accept
	 * a long string of hex digits, or a mix of delimited and undelimited
	 * digits like "1122.3344.5566", or delimited one- or two-digit octets
	 * like "1.22.3".
	 *
	 * If anything fails at this point, exit the loop so we set sdl_alen and
	 * sdl_len based on whatever we did manage to parse.  This preserves
	 * compatibility with the 4.3BSD version of link_addr, which had no way
	 * to indicate an error and would just return.
	 */
#define DIGIT(c)						\
	 (((c) >= '0' && (c) <= '9') ? ((c) - '0')		\
	: ((c) >= 'a' && (c) <= 'f') ? ((c) - 'a' + 10)		\
	: ((c) >= 'A' && (c) <= 'F') ? ((c) - 'A' + 10)		\
	: (-1))
#define ISDELIM(c) (((c) == '.' || (c) == ':' || (c) == '-') && \
    (delim == 0 || delim == (c)))

	for (;;) {
		int digit, digit2;

		/*
		 * Treat any leading delimiters as empty bytes.  This supports
		 * the (somewhat obsolete) form of Ethernet addresses with empty
		 * octets, e.g. "1::3:4:5:6".
		 */
		while (ISDELIM(*addr) && cp < cplim) {
			delim = *addr++;
			*cp++ = 0;
		}

		/* Did we reach the end of the string? */
		if (*addr == '\0')
			break;

		/*
		 * If not, the next character must be a digit, so make sure we
		 * have room for at least one more octet.
		 */

		if (cp >= cplim) {
			error = ENOSPC;
			break;
		}

		if ((digit = DIGIT(*addr)) == -1) {
			error = EINVAL;
			break;
		}

		++addr;

		/* If the next character is another digit, consume it. */
		if ((digit2 = DIGIT(*addr)) != -1) {
			digit = (digit << 4) | digit2;
			++addr;
		}

		if (ISDELIM(*addr)) {
			/*
			 * If the digit is followed by a delimiter, write it
			 * and consume the delimiter.
			 */
			delim = *addr++;
			*cp++ = digit;
		} else if (DIGIT(*addr) != -1) {
			/*
			 * If two digits are followed by a third digit, treat
			 * the two digits we have as a single octet and
			 * continue.
			 */
			*cp++ = digit;
		} else if (*addr == '\0') {
			/* If the digit is followed by EOS, we're done. */
			*cp++ = digit;
			break;
		} else {
			/* Otherwise, the input was invalid. */
			error = EINVAL;
			break;
		}
	}
#undef DIGIT
#undef ISDELIM

	/* How many bytes did we write to the address? */
	sdl->sdl_alen = cp - LLADDR(sdl);

	/*
	 * The user might have given us an sdl which is larger than sizeof(sdl);
	 * in that case, record the actual size of the new sdl.
	 */
	newsize = cp - (char *)sdl;
	if (newsize > sizeof(*sdl))
		sdl->sdl_len = (u_char)newsize;

	if (error == 0)
		return (0);

	errno = error;
	return (-1);
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
