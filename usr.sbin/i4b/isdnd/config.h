/*
 * Copyright (c) 1997, 2001 Hellmuth Michaelis. All rights reserved.
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
 *
 *---------------------------------------------------------------------------
 *
 *	i4b daemon - compile time configuration header file
 *	---------------------------------------------------
 *
 * $FreeBSD$
 *
 *      last edit-date: [Mon May 21 11:21:15 2001]
 *
 *---------------------------------------------------------------------------*/

#ifndef _CONFIG_H_
#define _CONFIG_H_

/* general values */

#define UMASK		022		/* file creation perm mask	*/
#define CFG_ENTRY_MAX	60		/* max no of config entries	*/
#define ISDN_CTRL_MAX	4		/* max no of controllers	*/
#define MAX_RE          8               /* max regular expression entries */

/* monitor max values */

#define MAX_MHOSTS 	8		/* max allowed monitor hosts 	*/

/* timouts */

#define TIMEOUT_CONNECT_ACTIVE	30	/* seconds to wait for MSG_CONN_ACT */

/* utility programs forked */

#define REGPROG_DEF	"program"	/* default program to use for regexpr */
#define ANSWERPROG_DEF	"answer"	/* default telephone answer program */

#endif /* _CONFIG_H_ */

/* EOF */
