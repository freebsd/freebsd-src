/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Keith Muller of the University of California, San Diego and Lance
 * Visser of Convex Computer Corporation.
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
 */

#ifndef lint
static char sccsid[] = "@(#)args.c	5.5 (Berkeley) 7/29/91";
#endif /* not lint */

#include <sys/types.h>
#include <limits.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dd.h"
#include "extern.h"

static u_long get_bsz __P((char *));

static void f_bs __P((char *));
static void f_cbs __P((char *));
static void f_conv __P((char *));
static void f_count __P((char *));
static void f_files __P((char *));
static void f_ibs __P((char *));
static void f_if __P((char *));
static void f_obs __P((char *));
static void f_of __P((char *));
static void f_seek __P((char *));
static void f_skip __P((char *));

static struct arg {
	char *name;
	void (*f) __P((char *));
	u_int set, noset;
} args[] = {
	"bs",		f_bs,		C_BS,		C_BS|C_IBS|C_OBS,
	"cbs",		f_cbs,		C_CBS,		C_CBS,
	"conv",		f_conv,		0,		0,
	"count",	f_count,	C_COUNT,	C_COUNT,
	"files",	f_files,	C_FILES,	C_FILES,
	"ibs",		f_ibs,		C_IBS,		C_BS|C_IBS,
	"if",		f_if,		C_IF,		C_IF,
	"obs",		f_obs,		C_OBS,		C_BS|C_OBS,
	"of",		f_of,		C_OF,		C_OF,
	"seek",		f_seek,		C_SEEK,		C_SEEK,
	"skip",		f_skip,		C_SKIP,		C_SKIP,
};

static char *oper;

/*
 * args -- parse JCL syntax of dd.
 */
void
jcl(argv)
	register char **argv;
{
	register struct arg *ap;
	struct arg tmp;
	char *arg;
	static int c_arg __P((const void *, const void *));

	in.dbsz = out.dbsz = 512;

	while (oper = *++argv) {
		if ((arg = index(oper, '=')) == NULL)
			err("unknown operand %s", oper);
		*arg++ = '\0';
		if (!*arg)
			err("no value specified for %s", oper);
		tmp.name = oper;
		if (!(ap = (struct arg *)bsearch(&tmp, args,
		    sizeof(args)/sizeof(struct arg), sizeof(struct arg),
		    c_arg)))
			err("unknown operand %s", tmp.name);
		if (ddflags & ap->noset)
			err("%s: illegal argument combination or already set",
			    tmp.name);
		ddflags |= ap->set;
		ap->f(arg);
	}

	/* Final sanity checks. */

	if (ddflags & C_BS) {
		/*
		 * Bs is turned off by any conversion -- we assume the user
		 * just wanted to set both the input and output block sizes
		 * and didn't want the bs semantics, so we don't warn.
		 */
		if (ddflags & (C_BLOCK|C_LCASE|C_SWAB|C_UCASE|C_UNBLOCK))
			ddflags &= ~C_BS;

		/* Bs supersedes ibs and obs. */
		if (ddflags & C_BS && ddflags & (C_IBS|C_OBS))
			warn("bs supersedes ibs and obs");
	}

	/*
	 * Ascii/ebcdic and cbs implies block/unblock.
	 * Block/unblock requires cbs and vice-versa.
	 */
	if (ddflags & (C_BLOCK|C_UNBLOCK)) {
		if (!(ddflags & C_CBS))
			err("record operations require cbs");
		if (cbsz == 0)
			err("cbs cannot be zero");
		cfunc = ddflags & C_BLOCK ? block : unblock;
	} else if (ddflags & C_CBS) {
		if (ddflags & (C_ASCII|C_EBCDIC)) {
			if (ddflags & C_ASCII) {
				ddflags |= C_UNBLOCK;
				cfunc = unblock;
			} else {
				ddflags |= C_BLOCK;
				cfunc = block;
			}
		} else
			err("cbs meaningless if not doing record operations");
		if (cbsz == 0)
			err("cbs cannot be zero");
	} else
		cfunc = def;

	if (in.dbsz == 0 || out.dbsz == 0)
		err("buffer sizes cannot be zero");

	/*
	 * Read, write and seek calls take ints as arguments.  Seek sizes
	 * could be larger if we wanted to do it in stages or check only
	 * regular files, but it's probably not worth it.
	 */
	if (in.dbsz > INT_MAX || out.dbsz > INT_MAX)
		err("buffer sizes cannot be greater than %d", INT_MAX);
	if (in.offset > INT_MAX / in.dbsz || out.offset > INT_MAX / out.dbsz)
		err("seek offsets cannot be larger than %d", INT_MAX);
}

static int
c_arg(a, b)
	const void *a, *b;
{
	return (strcmp(((struct arg *)a)->name, ((struct arg *)b)->name));
}

static void
f_bs(arg)
	char *arg;
{
	in.dbsz = out.dbsz = (int)get_bsz(arg);
}

static void
f_cbs(arg)
	char *arg;
{
	cbsz = (int)get_bsz(arg);
}

static void
f_count(arg)
	char *arg;
{
	cpy_cnt = (u_int)get_bsz(arg);
	if (!cpy_cnt)
		terminate(0);
}

static void
f_files(arg)
	char *arg;
{
	files_cnt = (int)get_bsz(arg);
}

static void
f_ibs(arg)
	char *arg;
{
	if (!(ddflags & C_BS))
		in.dbsz = (int)get_bsz(arg);
}

static void
f_if(arg)
	char *arg;
{
	in.name = arg;
}

static void
f_obs(arg)
	char *arg;
{
	if (!(ddflags & C_BS))
		out.dbsz = (int)get_bsz(arg);
}

static void
f_of(arg)
	char *arg;
{
	out.name = arg;
}

static void
f_seek(arg)
	char *arg;
{
	out.offset = (u_int)get_bsz(arg);
}

static void
f_skip(arg)
	char *arg;
{
	in.offset = (u_int)get_bsz(arg);
}

static struct conv {
	char *name;
	u_int set, noset;
	u_char *ctab;
} clist[] = {
	"ascii",	C_ASCII,	C_EBCDIC,	e2a_POSIX,
	"block",	C_BLOCK,	C_UNBLOCK,	NULL,
	"ebcdic",	C_EBCDIC,	C_ASCII,	a2e_POSIX,
	"ibm",		C_EBCDIC,	C_ASCII,	a2ibm_POSIX,
	"lcase",	C_LCASE,	C_UCASE,	NULL,
	"noerror",	C_NOERROR,	0,		NULL,
	"notrunc",	C_NOTRUNC,	0,		NULL,
	"oldascii",	C_ASCII,	C_EBCDIC,	e2a_32V,
	"oldebcdic",	C_EBCDIC,	C_ASCII,	a2e_32V,
	"oldibm",	C_EBCDIC,	C_ASCII,	a2ibm_32V,
	"swab",		C_SWAB,		0,		NULL,
	"sync",		C_SYNC,		0,		NULL,
	"ucase",	C_UCASE,	C_LCASE,	NULL,
	"unblock",	C_UNBLOCK,	C_BLOCK,	NULL,
};

static void
f_conv(arg)
	char *arg;
{
	register struct conv *cp;
	struct conv tmp;
	static int c_conv __P((const void *, const void *));

	while (arg != NULL) {
		tmp.name = strsep(&arg, ",");
		if (!(cp = (struct conv *)bsearch(&tmp, clist,
		    sizeof(clist)/sizeof(struct conv), sizeof(struct conv),
		    c_conv)))
			err("unknown conversion %s", tmp.name);
		if (ddflags & cp->noset)
			err("%s: illegal conversion combination", tmp.name);
		ddflags |= cp->set;
		if (cp->ctab)
			ctab = cp->ctab;
	}
}

static int
c_conv(a, b)
	const void *a, *b;
{
	return (strcmp(((struct conv *)a)->name, ((struct conv *)b)->name));
}

/*
 * Convert an expression of the following forms to an unsigned long.
 * 	1) A positive decimal number.
 *	2) A positive decimal number followed by a b (mult by 512).
 *	3) A positive decimal number followed by a k (mult by 1024).
 *	4) A positive decimal number followed by a m (mult by 512).
 *	5) A positive decimal number followed by a w (mult by sizeof int)
 *	6) Two or more positive decimal numbers (with/without k,b or w).
 *	   seperated by x (also * for backwards compatibility), specifying
 *	   the product of the indicated values.
 */
static u_long
get_bsz(val)
	char *val;
{
	char *expr;
	u_long num, t;

	num = strtoul(val, &expr, 0);
	if (num == ULONG_MAX)			/* Overflow. */
		err("%s: %s", oper, strerror(errno));
	if (expr == val)			/* No digits. */
		err("%s: illegal numeric value", oper);

	switch(*expr) {
	case 'b':
		t = num;
		num *= 512;
		if (t > num)
			goto erange;
		++expr;
		break;
	case 'k':
		t = num;
		num *= 1024;
		if (t > num)
			goto erange;
		++expr;
		break;
	case 'm':
		t = num;
		num *= 1048576;
		if (t > num)
			goto erange;
		++expr;
		break;
	case 'w':
		t = num;
		num *= sizeof(int);
		if (t > num)
			goto erange;
		++expr;
		break;
	}

	switch(*expr) {
		case '\0':
			break;
		case '*':			/* Backward compatible. */
		case 'x':
			t = num;
			num *= get_bsz(expr + 1);
			if (t > num)
erange:				err("%s: %s", oper, strerror(ERANGE));
			break;
		default:
			err("%s: illegal numeric value", oper);
	}
	return(num);
}
