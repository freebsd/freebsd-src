/*
 * Copyright (c) 1988 Regents of the University of California.
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
 *	from: @(#)saerrno.h	7.3 (Berkeley) 6/28/90
 *	$Id: saerrno.h,v 1.2 1993/10/16 19:31:35 rgrimes Exp $
 */

extern	int errno;	/* just like unix */

/* error codes */
#define	EADAPT	1	/* bad adaptor */
#define	ECTLR	2	/* bad controller */
#define	EUNIT	3	/* bad drive */
#define	EPART	4	/* bad partition */
#define	ERDLAB	5	/* can't read disk label */
#define	EUNLAB	6	/* unlabeled disk */
#define	ENXIO	7	/* bad device specification */
#define	EBADF	8	/* bad file descriptor */
#define	EOFFSET	9	/* relative seek not supported */
#define	ESRCH	10	/* directory search for file failed */
#define	EIO	11	/* generic error */
#define	ECMD	12	/* undefined driver command */
#define	EBSE	13	/* bad sector error */
#define	EWCK	14	/* write check error */
#define	EECC	15	/* uncorrectable ecc error */
#define	EHER	16	/* hard error */
