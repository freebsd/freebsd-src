/*	$OpenBSD: getline.c,v 1.16 2004/09/16 04:50:51 deraadt Exp $ */

/*
 * Copyright (c) 1985, 1988 Regents of the University of California.
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
 *
 *  @(#)ftpcmd.y    5.24 (Berkeley) 2/25/91
 */

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/telnet.h>

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sysexits.h>
#include <syslog.h>
#include <unistd.h>

#include "util.h"

int		refill_buffer(struct csiob *iobp);

/*
 * Refill the io buffer if we KNOW that data is available
 *
 * Returns 1 if any new data was obtained, 0 otherwise.
 */

int
refill_buffer(struct csiob *iobp)
{
	int rqlen, rlen;

	if (!(iobp->data_available))
		return(0);

	if (iobp->got_eof)
		return(0);

	/*
	 * The buffer has been entirely consumed if next_byte == io_buffer_len.
	 * Otherwise, there is some still-to-be-used data in io_buffer.
	 * Shuffle it to the start of the buffer.
	 * Note that next_byte will never exceed io_buffer_len.
	 * Also, note that we MUST use bcopy because the two regions could
	 * overlap (memcpy isn't defined to work properly with overlapping
	 * regions).
	 */
	if (iobp->next_byte < iobp->io_buffer_len) {
		int dst_ix = 0;
		int src_ix = iobp->next_byte;
		int amount = iobp->io_buffer_len - iobp->next_byte;

		bcopy(&iobp->io_buffer[src_ix], &iobp->io_buffer[dst_ix],
		    amount);
		iobp->io_buffer_len = amount;
	} else if (iobp->next_byte == iobp->io_buffer_len)
		iobp->io_buffer_len = 0;
	else {
		syslog(LOG_ERR, "next_byte(%d) > io_buffer_len(%d)",
		    iobp->next_byte, iobp->io_buffer_len);
		exit(EX_OSERR);
	}

	iobp->next_byte = 0;

	/* don't do tiny reads, grow first if we need to */
	rqlen = iobp->io_buffer_size - iobp->io_buffer_len;
	if (rqlen <= 128) {
		unsigned char *tmp;

		iobp->io_buffer_size += 128;
		tmp = realloc(iobp->io_buffer, iobp->io_buffer_size);
		if (tmp == NULL) {
			syslog(LOG_INFO, "Insufficient memory");
			exit(EX_UNAVAILABLE);
		}
		iobp->io_buffer = tmp;
		rqlen = iobp->io_buffer_size - iobp->io_buffer_len;
	}

	/*
	 * Always leave an unused byte at the end of the buffer
	 * because the debug output uses that byte from time to time
	 * to ensure that something that is being printed is \0 terminated.
	 */
	rqlen -= 1;

 doread:
	rlen = read(iobp->fd, &iobp->io_buffer[iobp->io_buffer_len], rqlen);
	iobp->data_available = 0;
	switch (rlen) {
	case -1:
		if (errno == EAGAIN || errno == EINTR)
			goto doread;
		if (errno != ECONNRESET) {
			syslog(LOG_INFO, "read() failed on socket from %s (%m)",
			    iobp->who);
			exit(EX_DATAERR);
		}
		/* fall through to EOF case */
	case 0:
		iobp->got_eof = 1;
		return(0);
		break;
	default:
		iobp->io_buffer_len += rlen;
		break;
	}
	return(1);
}

/*
 * telnet_getline - a hacked up version of fgets to ignore TELNET escape codes.
 *
 * This code is derived from the getline routine found in the UC Berkeley
 * ftpd code.
 *
 */

int
telnet_getline(struct csiob *iobp, struct csiob *telnet_passthrough)
{
	unsigned char ch;
	int ix;
	unsigned char tbuf[100];

	iobp->line_buffer[0] = '\0';

	/*
	 * If the buffer is empty then refill it right away.
	 */
	if (iobp->next_byte == iobp->io_buffer_len)
		if (!refill_buffer(iobp))
			return(0);

	/*
	 * Is there a telnet command in the buffer?
	 */
	ch = iobp->io_buffer[iobp->next_byte];
	if (ch == IAC) {
		/*
		 * Yes - buffer must have at least three bytes in it
		 */
		if (iobp->io_buffer_len - iobp->next_byte < 3) {
			if (!refill_buffer(iobp))
				return(0);
			if (iobp->io_buffer_len - iobp->next_byte < 3)
				return(0);
		}

		iobp->next_byte++;
		ch = iobp->io_buffer[iobp->next_byte++];

		switch (ch) {
		case WILL:
		case WONT:
		case DO:
		case DONT:
			tbuf[0] = IAC;
			tbuf[1] = ch;
			tbuf[2] = iobp->io_buffer[iobp->next_byte++];
			(void)send(telnet_passthrough->fd, tbuf, 3,
			    telnet_passthrough->send_oob_flags);
			break;
		case IAC:
			break;
		default:
			break;
		}
		return(1);
	} else {
		int clen;

		/*
		 * Is there a newline in the buffer?
		 */
		for (ix = iobp->next_byte; ix < iobp->io_buffer_len;
		    ix += 1) {
			if (iobp->io_buffer[ix] == '\n')
				break;
			if (iobp->io_buffer[ix] == '\0') {
				syslog(LOG_INFO,
				    "got NUL byte from %s - bye!",
				    iobp->who);
				exit(EX_DATAERR);
			}
		}

		if (ix == iobp->io_buffer_len) {
			if (!refill_buffer(iobp))
				return(0);
			/*
			 * Empty line returned
			 * will try again soon!
			 */
			return(1);
		}

		/*
		 * Expand the line buffer if it isn't big enough.  We
		 * use a fudge factor of 5 rather than trying to
		 * figure out exactly how to account for the '\0 \r\n' and
		 * such.  The correct fudge factor is 0, 1 or 2 but
		 * anything higher also works. We also grow it by a
		 * bunch to avoid having to do this often. Yes this is
		 * nasty.
		 */
		if (ix - iobp->next_byte > iobp->line_buffer_size - 5) {
			unsigned char *tmp;

			iobp->line_buffer_size = 256 + ix - iobp->next_byte;
			tmp = realloc(iobp->line_buffer,
			    iobp->line_buffer_size);
			if (tmp == NULL) {
				syslog(LOG_INFO, "Insufficient memory");
				exit(EX_UNAVAILABLE);
			}
			iobp->line_buffer = tmp;
		}

		/* +1 is for the newline */
		clen = (ix+1) - iobp->next_byte;
		memcpy(iobp->line_buffer, &iobp->io_buffer[iobp->next_byte],
		    clen);
		iobp->next_byte += clen;
		iobp->line_buffer[clen] = '\0';
		return(1);
	}
}
