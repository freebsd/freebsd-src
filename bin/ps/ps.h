/*-
 * Copyright (c) 1990 The Regents of the University of California.
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
 *	@(#)ps.h	5.3 (Berkeley) 6/3/91
 */

#define	UNLIMITED	0	/* unlimited terminal width */
enum type { CHAR, UCHAR, SHORT, USHORT, LONG, ULONG, KPTR };

struct usave {
	struct	proc *u_procp;
	struct	timeval u_start;
	struct	rusage u_ru;
	struct	rusage u_cru;
	char	u_acflag;
};

typedef struct _kinfo {
	struct proc *ki_p;	/* proc structure */
	struct eproc *ki_e;	/* extra stuff */
	struct usave *ki_u;	/* interesting parts of user */
	char *ki_args;		/* exec args (should be char **) */
	char *ki_env;		/* environment (should be char **) */
} KINFO;

/* Variables. */
typedef struct _var {
	char	*name;		/* name(s) of variable */
	char	*header;	/* default header */
	char	*alias;		/* aliases */
#define	COMM	0x01		/* needs exec arguments and environment (XXX) */
#define	LJUST	0x02		/* left adjust on output (trailing blanks) */
#define	USER	0x04		/* needs user structure */
	u_int	flag;
	int	(*oproc)();	/* output routine */
	short	width;		/* printing width */
	/*
	 * The following (optional) elements are hooks for passing information
	 * to the generic output routines: pvar, evar, uvar (those which print
	 * simple elements from well known structures: proc, eproc, usave)
	 */
	int	off;		/* offset in structure */
	enum	type type;	/* type of element */
	char	*fmt;		/* printf format */
	char	*time;		/* time format */
	/*
	 * glue to link selected fields together
	 */
} VAR;

struct varent {
	VAR *var;
	struct varent *next;
};

#include "extern.h"
