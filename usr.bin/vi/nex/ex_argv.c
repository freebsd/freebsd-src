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
 */

#ifndef lint
static char sccsid[] = "@(#)ex_argv.c	8.26 (Berkeley) 1/2/94";
#endif /* not lint */

#include <sys/types.h>

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "vi.h"
#include "excmd.h"

static int argv_alloc __P((SCR *, size_t));
static int argv_fexp __P((SCR *, EXCMDARG *,
	       char *, size_t, char *, size_t *, char **, size_t *, int));
static int argv_sexp __P((SCR *, char **, size_t *, size_t *));

/*
 * argv_init --
 *	Build  a prototype arguments list.
 */
int
argv_init(sp, ep, excp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *excp;
{
	EX_PRIVATE *exp;

	exp = EXP(sp);
	exp->argsoff = 0;
	argv_alloc(sp, 1);

	excp->argv = exp->args;
	excp->argc = exp->argsoff;
	return (0);
}

/*
 * argv_exp0 --
 *	Put a string into an argv.
 */
int
argv_exp0(sp, ep, excp, cmd, cmdlen)
	SCR *sp;
	EXF *ep;
	EXCMDARG *excp;
	char *cmd;
	size_t cmdlen;
{
	EX_PRIVATE *exp;

	exp = EXP(sp);
	argv_alloc(sp, cmdlen);
	memmove(exp->args[exp->argsoff]->bp, cmd, cmdlen);
	exp->args[exp->argsoff]->bp[cmdlen] = '\0';
	exp->args[exp->argsoff]->len = cmdlen;
	++exp->argsoff;
	excp->argv = exp->args;
	excp->argc = exp->argsoff;
	return (0);
}

/*
 * argv_exp1 --
 *	Do file name expansion on a string, and leave it in a string.
 */
int
argv_exp1(sp, ep, excp, cmd, cmdlen, is_bang)
	SCR *sp;
	EXF *ep;
	EXCMDARG *excp;
	char *cmd;
	size_t cmdlen;
	int is_bang;
{
	EX_PRIVATE *exp;
	size_t blen, len;
	char *bp;

	GET_SPACE_RET(sp, bp, blen, 512);

	len = 0;
	exp = EXP(sp);
	if (argv_fexp(sp, excp, cmd, cmdlen, bp, &len, &bp, &blen, is_bang)) {
		FREE_SPACE(sp, bp, blen);
		return (1);
	}

	argv_alloc(sp, len);
	memmove(exp->args[exp->argsoff]->bp, bp, len);
	exp->args[exp->argsoff]->bp[len] = '\0';
	exp->args[exp->argsoff]->len = len;
	++exp->argsoff;
	excp->argv = exp->args;
	excp->argc = exp->argsoff;

	FREE_SPACE(sp, bp, blen);
	return (0);
}

/*
 * argv_exp2 --
 *	Do file name and shell expansion on a string, and break
 *	it up into an argv.
 */
int
argv_exp2(sp, ep, excp, cmd, cmdlen, is_bang)
	SCR *sp;
	EXF *ep;
	EXCMDARG *excp;
	char *cmd;
	size_t cmdlen;
	int is_bang;
{
	size_t blen, len, n;
	int rval;
	char *bp, *p;

	GET_SPACE_RET(sp, bp, blen, 512);

#define	SHELLECHO	"echo "
#define	SHELLOFFSET	(sizeof(SHELLECHO) - 1)
	memmove(bp, SHELLECHO, SHELLOFFSET);
	p = bp + SHELLOFFSET;
	len = SHELLOFFSET;

#if defined(DEBUG) && 0
	TRACE(sp, "file_argv: {%.*s}\n", (int)cmdlen, cmd);
#endif

	if (argv_fexp(sp, excp, cmd, cmdlen, p, &len, &bp, &blen, is_bang)) {
		rval = 1;
		goto err;
	}

#if defined(DEBUG) && 0
	TRACE(sp, "before shell: %d: {%s}\n", len, bp);
#endif

	/*
	 * Do shell word expansion -- it's very, very hard to figure out
	 * what magic characters the user's shell expects.  If it's not
	 * pure vanilla, don't even try.
	 */
	for (p = bp, n = len; n > 0; --n, ++p)
		if (!isalnum(*p) && !isblank(*p) && *p != '/' && *p != '.')
			break;
	if (n > 0) {
		if (argv_sexp(sp, &bp, &blen, &len)) {
			rval = 1;
			goto err;
		}
		p = bp;
	} else {
		p = bp + SHELLOFFSET;
		len -= SHELLOFFSET;
	}

#if defined(DEBUG) && 0
	TRACE(sp, "after shell: %d: {%s}\n", len, bp);
#endif

	rval = argv_exp3(sp, ep, excp, p, len);

err:	FREE_SPACE(sp, bp, blen);
	return (rval);
}

/*
 * argv_exp3 --
 *	Take a string and break it up into an argv.
 */
int
argv_exp3(sp, ep, excp, cmd, cmdlen)
	SCR *sp;
	EXF *ep;
	EXCMDARG *excp;
	char *cmd;
	size_t cmdlen;
{
	CHAR_T vlit;
	EX_PRIVATE *exp;
	size_t len;
	int ch, off;
	char *ap, *p;

	(void)term_key_ch(sp, K_VLNEXT, &vlit);
	for (exp = EXP(sp); cmdlen > 0; ++exp->argsoff) {
		/* Skip any leading whitespace. */
		for (; cmdlen > 0; --cmdlen, ++cmd) {
			ch = *cmd;
			if (!isblank(ch))
				break;
		}
		if (cmdlen == 0)
			break;

		/*
		 * Determine the length of this whitespace delimited
		 * argument.  
		 *
		 * QUOTING NOTE:
		 *
		 * Skip any character preceded by the user's quoting
		 * character.
		 */
		for (ap = cmd, len = 0; cmdlen > 0; ++cmd, --cmdlen, ++len)
			if ((ch = *cmd) == vlit && cmdlen > 1) {
				++cmd; 
				--cmdlen;
			} else if (isblank(ch))
				break;
				
		/*
		 * Copy the argument into place.
		 *
		 * QUOTING NOTE:
		 *
		 * Lose quote chars.
		 */
		argv_alloc(sp, len);
		off = exp->argsoff;
		exp->args[off]->len = len;
		for (p = exp->args[off]->bp; len > 0; --len, *p++ = *ap++)
			if (*ap == vlit) {
				++ap;
				--exp->args[off]->len;
			}
		*p = '\0';
	}
	excp->argv = exp->args;
	excp->argc = exp->argsoff;

#if defined(DEBUG) && 0
	for (cnt = 0; cnt < exp->argsoff; ++cnt)
		TRACE(sp, "arg %d: {%s}\n", cnt, exp->argv[cnt]);
#endif
	return (0);
}

/*
 * argv_fexp --
 *	Do file name and bang command expansion.
 */
static int
argv_fexp(sp, excp, cmd, cmdlen, p, lenp, bpp, blenp, is_bang)
	SCR *sp;
	EXCMDARG *excp;
	char *cmd, *p, **bpp;
	size_t cmdlen, *lenp, *blenp;
	int is_bang;
{
	EX_PRIVATE *exp;
	char *bp, *t;
	size_t blen, len, tlen;

	/* Replace file name characters. */
	for (bp = *bpp, blen = *blenp, len = *lenp; cmdlen > 0; --cmdlen, ++cmd)
		switch (*cmd) {
		case '!':
			if (!is_bang)
				goto ins_ch;
			exp = EXP(sp);
			if (exp->lastbcomm == NULL) {
				msgq(sp, M_ERR,
				    "No previous command to replace \"!\".");
				return (1);
			}
			len += tlen = strlen(exp->lastbcomm);
			ADD_SPACE_RET(sp, bp, blen, len);
			memmove(p, exp->lastbcomm, tlen);
			p += tlen;
			F_SET(excp, E_MODIFY);
			break;
		case '%':
			if (sp->frp->cname == NULL && sp->frp->name == NULL) {
				msgq(sp, M_ERR,
				    "No filename to substitute for %%.");
				return (1);
			}
			tlen = strlen(t = FILENAME(sp->frp));
			len += tlen;
			ADD_SPACE_RET(sp, bp, blen, len);
			memmove(p, t, tlen);
			p += tlen;
			F_SET(excp, E_MODIFY);
			break;
		case '#':
			/*
			 * Try the alternate file name first, then the
			 * previously edited file.
			 */
			if (sp->alt_name == NULL && (sp->p_frp == NULL ||
			    sp->frp->cname == NULL && sp->frp->name == NULL)) {
				msgq(sp, M_ERR,
				    "No filename to substitute for #.");
				return (1);
			}
			if (sp->alt_name != NULL)
				t = sp->alt_name;
			else
				t = FILENAME(sp->frp);
			len += tlen = strlen(t);
			ADD_SPACE_RET(sp, bp, blen, len);
			memmove(p, t, tlen);
			p += tlen;
			F_SET(excp, E_MODIFY);
			break;
		case '\\':
			/*
			 * QUOTING NOTE:
			 *
			 * Strip any backslashes that protected the file
			 * expansion characters.
			 */
			if (cmdlen > 1 && cmd[1] == '%' || cmd[1] == '#')
				++cmd;
			/* FALLTHROUGH */
		default:
ins_ch:			++len;
			ADD_SPACE_RET(sp, bp, blen, len);
			*p++ = *cmd;
		}

	/* Nul termination. */
	++len;
	ADD_SPACE_RET(sp, bp, blen, len);
	*p = '\0';

	/* Return the new string length, buffer, buffer length. */
	*lenp = len - 1;
	*bpp = bp;
	*blenp = blen;
	return (0);
}

/*
 * argv_alloc --
 *	Make more space for arguments.
 */
static int
argv_alloc(sp, len)
	SCR *sp;
	size_t len;
{
	ARGS *ap;					
	EX_PRIVATE *exp;
	int cnt, off;

	/*
	 * Allocate room for another argument, always leaving
	 * enough room for an ARGS structure with a length of 0.
	 */
#define	INCREMENT	20
	exp = EXP(sp);
	off = exp->argsoff;
	if (exp->argscnt == 0 || off + 2 >= exp->argscnt - 1) {
		cnt = exp->argscnt + INCREMENT;
		REALLOC(sp, exp->args, ARGS **, cnt * sizeof(ARGS *));
		if (exp->args == NULL) {
			(void)argv_free(sp);
			goto mem;
		}
		memset(&exp->args[off], 0, INCREMENT * sizeof(ARGS *));
		exp->argscnt = cnt;
	}

	/* First argument. */
	if (exp->args[off] == NULL) {
		CALLOC(sp, exp->args[off], ARGS *, 1, sizeof(ARGS));
		if (exp->args[off] == NULL)
			goto mem;				
	}						

	/* First argument buffer. */
	ap = exp->args[off];			
	ap->len = 0;					
	if (ap->blen < len + 1) {			
		ap->blen = len + 1;			
		REALLOC(sp, ap->bp, CHAR_T *, ap->blen * sizeof(CHAR_T));
		if (ap->bp == NULL) {
			ap->bp = NULL;		
			ap->blen = 0;			
			F_CLR(ap, A_ALLOCATED);	
mem:			msgq(sp, M_SYSERR, NULL);	
			return (1);			
		}					
		F_SET(ap, A_ALLOCATED);		
	}						

	/* Second argument. */
	if (exp->args[++off] == NULL) {
		CALLOC(sp, exp->args[off], ARGS *, 1, sizeof(ARGS));
		if (exp->args[off] == NULL)
			goto mem;				
	}						
	/* 0 length serves as end-of-argument marker. */
	exp->args[off]->len = 0;			
	return (0);
}

/*
 * argv_free --
 *	Free up argument structures.
 */
int
argv_free(sp)
	SCR *sp;
{
	EX_PRIVATE *exp;
	int off;

	exp = EXP(sp);
	if (exp->args != NULL) {
		for (off = 0; off < exp->argscnt; ++off) {
			if (exp->args[off] == NULL)
				continue;
			if (F_ISSET(exp->args[off], A_ALLOCATED))
				free(exp->args[off]->bp);
			FREE(exp->args[off], sizeof(ARGS));
		}
		FREE(exp->args, exp->argscnt * sizeof(ARGS *));
	}
	exp->args = NULL;
	exp->argscnt = 0;
	exp->argsoff = 0;
	return (0);
}

/*
 * argv_sexp --
 *	Fork a shell, pipe a command through it, and read the output into
 *	a buffer.
 */
static int
argv_sexp(sp, bpp, blenp, lenp)
	SCR *sp;
	char **bpp;
	size_t *blenp, *lenp;
{
	FILE *ifp;
	pid_t pid;
	size_t blen, len;
	int ch, rval, output[2];
	char *bp, *p, *sh, *sh_path;

	bp = *bpp;
	blen = *blenp;

	sh_path = O_STR(sp, O_SHELL);
	if ((sh = strrchr(sh_path, '/')) == NULL)
		sh = sh_path;
	else
		++sh;

	/*
	 * There are two different processes running through this code.
	 * They are named the utility and the parent. The utility reads
	 * from standard input and writes to the parent.  The parent reads
	 * from the utility and writes into the buffer.  The parent reads
	 * from output[0], and the utility writes to output[1].
	 */
	if (pipe(output) < 0) {
		msgq(sp, M_SYSERR, "pipe");
		return (1);
	}
	if ((ifp = fdopen(output[0], "r")) == NULL) {
		msgq(sp, M_SYSERR, "fdopen");
		goto err;
	}
		
	/*
	 * Do the minimal amount of work possible, the shell is going
	 * to run briefly and then exit.  Hopefully.
	 */
	switch (pid = vfork()) {
	case -1:			/* Error. */
		msgq(sp, M_SYSERR, "vfork");
err:		(void)close(output[0]);
		(void)close(output[1]);
		return (1);
	case 0:				/* Utility. */
		/* Redirect stdout/stderr to the write end of the pipe. */
		(void)dup2(output[1], STDOUT_FILENO);
		(void)dup2(output[1], STDERR_FILENO);

		/* Close the utility's file descriptors. */
		(void)close(output[0]);
		(void)close(output[1]);

		/* Assumes that all shells have -c. */
		execl(sh_path, sh, "-c", bp, NULL);
		msgq(sp, M_ERR,
		    "Error: execl: %s: %s", sh_path, strerror(errno));
		_exit(127);
	default:
		/* Close the pipe end the parent won't use. */
		(void)close(output[1]);
		break;
	}

	rval = 0;

	/*
	 * Copy process output into a buffer.
	 *
	 * !!!
	 * Historic vi apparently discarded leading \n and \r's from
	 * the shell output stream.  We don't on the grounds that any
	 * shell that does that is broken.
	 */
	for (p = bp, len = 0, ch = EOF;
	    (ch = getc(ifp)) != EOF; *p++ = ch, --blen, ++len)
		if (blen < 5) {
			ADD_SPACE_GOTO(sp, bp, blen, *blenp * 2);
			p = bp + len;
			blen = *blenp - len;
		}

	/* Delete the final newline, nul terminate the string. */
	if (p > bp && p[-1] == '\n' || p[-1] == '\r') {
		--len;
		*--p = '\0';
	} else
		*p = '\0';
	*lenp = len;

	if (ferror(ifp)) {
		msgq(sp, M_ERR, "I/O error: %s", sh);
binc_err:	rval = 1;
	}
	(void)fclose(ifp);

	*bpp = bp;		/* *blenp is already updated. */

	/* Wait for the process. */
	return (proc_wait(sp, (long)pid, sh, 0) | rval);
}
