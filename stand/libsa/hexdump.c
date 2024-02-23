/*-
 * Copyright (c) 1998 Michael Smith <msmith@freebsd.org>
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <string.h>
#include <stand.h>

/*
 * Display a region in traditional hexdump format.
 */
void
hexdump(caddr_t region, size_t len)
{
    caddr_t	line;
    int		x, c;
#ifdef HEXDUMP_PAGER
    /* pager causes linking issues for some apps */
#define emit(fmt, args...)	{sprintf(lbuf, fmt , ## args); pager_output(lbuf);}
    char	lbuf[80];

    pager_open();
#else
#define emit(fmt, args...) printf(fmt, ## args)
#endif

    for (line = region; line < (region + len); line += 16) {
	emit("%08lx  ", (long) line);
	
	for (x = 0; x < 16; x++) {
	    if ((line + x) < (region + len)) {
		emit("%02x ", *(uint8_t *)(line + x));
	    } else {
		emit("-- ");
	    }
	    if (x == 7)
		emit(" ");
	}
	emit(" |");
	for (x = 0; x < 16; x++) {
	    if ((line + x) < (region + len)) {
		c = *(uint8_t *)(line + x);
		if ((c < ' ') || (c > '~'))	/* !isprint(c) */
		    c = '.';
		emit("%c", c);
	    } else {
		emit(" ");
	    }
	}
	emit("|\n");
    }
#ifdef HEXDUMP_PAGER
    pager_close();
#endif
}
