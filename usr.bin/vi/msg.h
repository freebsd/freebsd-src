/*-
 * Copyright (c) 1993
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	@(#)msg.h	8.8 (Berkeley) 11/18/93
 */

/*
 * M_BERR	-- Error: ring a bell if O_VERBOSE not set, else
 *		   display in inverse video.
 * M_ERR	-- Error: display in inverse video.
 * M_INFO	-- Info: display in normal video.
 * M_SYSERR	-- M_ERR, but use standard error message.
 * M_VINFO	-- Info: display only if O_VERBOSE set.
 *
 * In historical vi, O_VERBOSE didn't exist, and O_TERSE made the
 * error messages shorter.  In this version, O_TERSE has no effect
 * and O_VERBOSE results in informational displays about common
 * errors.
 */
enum msgtype { M_BERR, M_ERR, M_INFO, M_SYSERR, M_VINFO };

typedef struct _msgh MSGH;	/* MESG list head structure. */
LIST_HEAD(_msgh, _msg);

struct _msg {
	LIST_ENTRY(_msg) q;	/* Linked list of messages. */
	char	*mbuf;		/* Message buffer. */
	size_t	 blen;		/* Message buffer length. */
	size_t	 len;		/* Message length. */

#define	M_EMPTY		0x01	/* No message. */
#define	M_INV_VIDEO	0x02	/* Inverse video. */
	u_int	 flags;		/* Flags. */
};

/* Messages. */
void	msg_app __P((GS *, SCR *, int, char *, size_t));
int	msg_rpt __P((SCR *, int));
void	msgq __P((SCR *, enum msgtype, const char *, ...));
