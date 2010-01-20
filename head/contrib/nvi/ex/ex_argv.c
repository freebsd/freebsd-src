/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1993, 1994, 1995, 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)ex_argv.c	10.26 (Berkeley) 9/20/96";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>

#include <bitstring.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../common/common.h"

static int argv_alloc __P((SCR *, size_t));
static int argv_comp __P((const void *, const void *));
static int argv_fexp __P((SCR *, EXCMD *,
	char *, size_t, char *, size_t *, char **, size_t *, int));
static int argv_lexp __P((SCR *, EXCMD *, char *));
static int argv_sexp __P((SCR *, char **, size_t *, size_t *));

/*
 * argv_init --
 *	Build  a prototype arguments list.
 *
 * PUBLIC: int argv_init __P((SCR *, EXCMD *));
 */
int
argv_init(sp, excp)
	SCR *sp;
	EXCMD *excp;
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
 *	Append a string to the argument list.
 *
 * PUBLIC: int argv_exp0 __P((SCR *, EXCMD *, char *, size_t));
 */
int
argv_exp0(sp, excp, cmd, cmdlen)
	SCR *sp;
	EXCMD *excp;
	char *cmd;
	size_t cmdlen;
{
	EX_PRIVATE *exp;

	exp = EXP(sp);
	argv_alloc(sp, cmdlen);
	memcpy(exp->args[exp->argsoff]->bp, cmd, cmdlen);
	exp->args[exp->argsoff]->bp[cmdlen] = '\0';
	exp->args[exp->argsoff]->len = cmdlen;
	++exp->argsoff;
	excp->argv = exp->args;
	excp->argc = exp->argsoff;
	return (0);
}

/*
 * argv_exp1 --
 *	Do file name expansion on a string, and append it to the
 *	argument list.
 *
 * PUBLIC: int argv_exp1 __P((SCR *, EXCMD *, char *, size_t, int));
 */
int
argv_exp1(sp, excp, cmd, cmdlen, is_bang)
	SCR *sp;
	EXCMD *excp;
	char *cmd;
	size_t cmdlen;
	int is_bang;
{
	EX_PRIVATE *exp;
	size_t blen, len;
	char *bp, *p, *t;

	GET_SPACE_RET(sp, bp, blen, 512);

	len = 0;
	exp = EXP(sp);
	if (argv_fexp(sp, excp, cmd, cmdlen, bp, &len, &bp, &blen, is_bang)) {
		FREE_SPACE(sp, bp, blen);
		return (1);
	}

	/* If it's empty, we're done. */
	if (len != 0) {
		for (p = bp, t = bp + len; p < t; ++p)
			if (!isblank(*p))
				break;
		if (p == t)
			goto ret;
	} else
		goto ret;

	(void)argv_exp0(sp, excp, bp, len);

ret:	FREE_SPACE(sp, bp, blen);
	return (0);
}

/*
 * argv_exp2 --
 *	Do file name and shell expansion on a string, and append it to
 *	the argument list.
 *
 * PUBLIC: int argv_exp2 __P((SCR *, EXCMD *, char *, size_t));
 */
int
argv_exp2(sp, excp, cmd, cmdlen)
	SCR *sp;
	EXCMD *excp;
	char *cmd;
	size_t cmdlen;
{
	size_t blen, len, n;
	int rval;
	char *bp, *mp, *p;

	GET_SPACE_RET(sp, bp, blen, 512);

#define	SHELLECHO	"echo "
#define	SHELLOFFSET	(sizeof(SHELLECHO) - 1)
	memcpy(bp, SHELLECHO, SHELLOFFSET);
	p = bp + SHELLOFFSET;
	len = SHELLOFFSET;

#if defined(DEBUG) && 0
	TRACE(sp, "file_argv: {%.*s}\n", (int)cmdlen, cmd);
#endif

	if (argv_fexp(sp, excp, cmd, cmdlen, p, &len, &bp, &blen, 0)) {
		rval = 1;
		goto err;
	}

#if defined(DEBUG) && 0
	TRACE(sp, "before shell: %d: {%s}\n", len, bp);
#endif

	/*
	 * Do shell word expansion -- it's very, very hard to figure out what
	 * magic characters the user's shell expects.  Historically, it was a
	 * union of v7 shell and csh meta characters.  We match that practice
	 * by default, so ":read \%" tries to read a file named '%'.  It would
	 * make more sense to pass any special characters through the shell,
	 * but then, if your shell was csh, the above example will behave
	 * differently in nvi than in vi.  If you want to get other characters
	 * passed through to your shell, change the "meta" option.
	 *
	 * To avoid a function call per character, we do a first pass through
	 * the meta characters looking for characters that aren't expected
	 * to be there, and then we can ignore them in the user's argument.
	 */
	if (opts_empty(sp, O_SHELL, 1) || opts_empty(sp, O_SHELLMETA, 1))
		n = 0;
	else {
		for (p = mp = O_STR(sp, O_SHELLMETA); *p != '\0'; ++p)
			if (isblank(*p) || isalnum(*p))
				break;
		p = bp + SHELLOFFSET;
		n = len - SHELLOFFSET;
		if (*p != '\0') {
			for (; n > 0; --n, ++p)
				if (strchr(mp, *p) != NULL)
					break;
		} else
			for (; n > 0; --n, ++p)
				if (!isblank(*p) &&
				    !isalnum(*p) && strchr(mp, *p) != NULL)
					break;
	}

	/*
	 * If we found a meta character in the string, fork a shell to expand
	 * it.  Unfortunately, this is comparatively slow.  Historically, it
	 * didn't matter much, since users don't enter meta characters as part
	 * of pathnames that frequently.  The addition of filename completion
	 * broke that assumption because it's easy to use.  As a result, lots
	 * folks have complained that the expansion code is too slow.  So, we
	 * detect filename completion as a special case, and do it internally.
	 * Note that this code assumes that the <asterisk> character is the
	 * match-anything meta character.  That feels safe -- if anyone writes
	 * a shell that doesn't follow that convention, I'd suggest giving them
	 * a festive hot-lead enema.
	 */
	switch (n) {
	case 0:
		p = bp + SHELLOFFSET;
		len -= SHELLOFFSET;
		rval = argv_exp3(sp, excp, p, len);
		break;
	case 1:
		if (*p == '*') {
			*p = '\0';
			rval = argv_lexp(sp, excp, bp + SHELLOFFSET);
			break;
		}
		/* FALLTHROUGH */
	default:
		if (argv_sexp(sp, &bp, &blen, &len)) {
			rval = 1;
			goto err;
		}
		p = bp;
		rval = argv_exp3(sp, excp, p, len);
		break;
	}

err:	FREE_SPACE(sp, bp, blen);
	return (rval);
}

/*
 * argv_exp3 --
 *	Take a string and break it up into an argv, which is appended
 *	to the argument list.
 *
 * PUBLIC: int argv_exp3 __P((SCR *, EXCMD *, char *, size_t));
 */
int
argv_exp3(sp, excp, cmd, cmdlen)
	SCR *sp;
	EXCMD *excp;
	char *cmd;
	size_t cmdlen;
{
	EX_PRIVATE *exp;
	size_t len;
	int ch, off;
	char *ap, *p;

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
		for (ap = cmd, len = 0; cmdlen > 0; ++cmd, --cmdlen, ++len) {
			ch = *cmd;
			if (IS_ESCAPE(sp, excp, ch) && cmdlen > 1) {
				++cmd;
				--cmdlen;
			} else if (isblank(ch))
				break;
		}

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
			if (IS_ESCAPE(sp, excp, *ap))
				++ap;
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
	EXCMD *excp;
	char *cmd, *p, **bpp;
	size_t cmdlen, *lenp, *blenp;
	int is_bang;
{
	EX_PRIVATE *exp;
	char *bp, *t;
	size_t blen, len, off, tlen;

	/* Replace file name characters. */
	for (bp = *bpp, blen = *blenp, len = *lenp; cmdlen > 0; --cmdlen, ++cmd)
		switch (*cmd) {
		case '!':
			if (!is_bang)
				goto ins_ch;
			exp = EXP(sp);
			if (exp->lastbcomm == NULL) {
				msgq(sp, M_ERR,
				    "115|No previous command to replace \"!\"");
				return (1);
			}
			len += tlen = strlen(exp->lastbcomm);
			off = p - bp;
			ADD_SPACE_RET(sp, bp, blen, len);
			p = bp + off;
			memcpy(p, exp->lastbcomm, tlen);
			p += tlen;
			F_SET(excp, E_MODIFY);
			break;
		case '%':
			if ((t = sp->frp->name) == NULL) {
				msgq(sp, M_ERR,
				    "116|No filename to substitute for %%");
				return (1);
			}
			tlen = strlen(t);
			len += tlen;
			off = p - bp;
			ADD_SPACE_RET(sp, bp, blen, len);
			p = bp + off;
			memcpy(p, t, tlen);
			p += tlen;
			F_SET(excp, E_MODIFY);
			break;
		case '#':
			if ((t = sp->alt_name) == NULL) {
				msgq(sp, M_ERR,
				    "117|No filename to substitute for #");
				return (1);
			}
			len += tlen = strlen(t);
			off = p - bp;
			ADD_SPACE_RET(sp, bp, blen, len);
			p = bp + off;
			memcpy(p, t, tlen);
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
			if (cmdlen > 1 &&
			    (cmd[1] == '%' || cmd[1] == '#' || cmd[1] == '!')) {
				++cmd;
				--cmdlen;
			}
			/* FALLTHROUGH */
		default:
ins_ch:			++len;
			off = p - bp;
			ADD_SPACE_RET(sp, bp, blen, len);
			p = bp + off;
			*p++ = *cmd;
		}

	/* Nul termination. */
	++len;
	off = p - bp;
	ADD_SPACE_RET(sp, bp, blen, len);
	p = bp + off;
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
		memset(&exp->args[exp->argscnt], 0, INCREMENT * sizeof(ARGS *));
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
 *
 * PUBLIC: int argv_free __P((SCR *));
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
			free(exp->args[off]);
		}
		free(exp->args);
	}
	exp->args = NULL;
	exp->argscnt = 0;
	exp->argsoff = 0;
	return (0);
}

/*
 * argv_lexp --
 *	Find all file names matching the prefix and append them to the
 *	buffer.
 */
static int
argv_lexp(sp, excp, path)
	SCR *sp;
	EXCMD *excp;
	char *path;
{
	struct dirent *dp;
	DIR *dirp;
	EX_PRIVATE *exp;
	int off;
	size_t dlen, len, nlen;
	char *dname, *name, *p;

	exp = EXP(sp);

	/* Set up the name and length for comparison. */
	if ((p = strrchr(path, '/')) == NULL) {
		dname = ".";
		dlen = 0;
		name = path;
	} else { 
		if (p == path) {
			dname = "/";
			dlen = 1;
		} else {
			*p = '\0';
			dname = path;
			dlen = strlen(path);
		}
		name = p + 1;
	}
	nlen = strlen(name);

	/*
	 * XXX
	 * We don't use the d_namlen field, it's not portable enough; we
	 * assume that d_name is nul terminated, instead.
	 */
	if ((dirp = opendir(dname)) == NULL) {
		msgq_str(sp, M_SYSERR, dname, "%s");
		return (1);
	}
	for (off = exp->argsoff; (dp = readdir(dirp)) != NULL;) {
		if (nlen == 0) {
			if (dp->d_name[0] == '.')
				continue;
			len = strlen(dp->d_name);
		} else {
			len = strlen(dp->d_name);
			if (len < nlen || memcmp(dp->d_name, name, nlen))
				continue;
		}

		/* Directory + name + slash + null. */
		argv_alloc(sp, dlen + len + 2);
		p = exp->args[exp->argsoff]->bp;
		if (dlen != 0) {
			memcpy(p, dname, dlen);
			p += dlen;
			if (dlen > 1 || dname[0] != '/')
				*p++ = '/';
		}
		memcpy(p, dp->d_name, len + 1);
		exp->args[exp->argsoff]->len = dlen + len + 1;
		++exp->argsoff;
		excp->argv = exp->args;
		excp->argc = exp->argsoff;
	}
	closedir(dirp);

	if (off == exp->argsoff) {
		/*
		 * If we didn't find a match, complain that the expansion
		 * failed.  We can't know for certain that's the error, but
		 * it's a good guess, and it matches historic practice. 
		 */
		msgq(sp, M_ERR, "304|Shell expansion failed");
		return (1);
	}
	qsort(exp->args + off, exp->argsoff - off, sizeof(ARGS *), argv_comp);
	return (0);
}

/*
 * argv_comp --
 *	Alphabetic comparison.
 */
static int
argv_comp(a, b)
	const void *a, *b;
{
	return (strcmp((char *)(*(ARGS **)a)->bp, (char *)(*(ARGS **)b)->bp));
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
	enum { SEXP_ERR, SEXP_EXPANSION_ERR, SEXP_OK } rval;
	FILE *ifp;
	pid_t pid;
	size_t blen, len;
	int ch, std_output[2];
	char *bp, *p, *sh, *sh_path;

	/* Secure means no shell access. */
	if (O_ISSET(sp, O_SECURE)) {
		msgq(sp, M_ERR,
"289|Shell expansions not supported when the secure edit option is set");
		return (1);
	}

	sh_path = O_STR(sp, O_SHELL);
	if ((sh = strrchr(sh_path, '/')) == NULL)
		sh = sh_path;
	else
		++sh;

	/* Local copies of the buffer variables. */
	bp = *bpp;
	blen = *blenp;

	/*
	 * There are two different processes running through this code, named
	 * the utility (the shell) and the parent. The utility reads standard
	 * input and writes standard output and standard error output.  The
	 * parent writes to the utility, reads its standard output and ignores
	 * its standard error output.  Historically, the standard error output
	 * was discarded by vi, as it produces a lot of noise when file patterns
	 * don't match.
	 *
	 * The parent reads std_output[0], and the utility writes std_output[1].
	 */
	ifp = NULL;
	std_output[0] = std_output[1] = -1;
	if (pipe(std_output) < 0) {
		msgq(sp, M_SYSERR, "pipe");
		return (1);
	}
	if ((ifp = fdopen(std_output[0], "r")) == NULL) {
		msgq(sp, M_SYSERR, "fdopen");
		goto err;
	}

	/*
	 * Do the minimal amount of work possible, the shell is going to run
	 * briefly and then exit.  We sincerely hope.
	 */
	switch (pid = vfork()) {
	case -1:			/* Error. */
		msgq(sp, M_SYSERR, "vfork");
err:		if (ifp != NULL)
			(void)fclose(ifp);
		else if (std_output[0] != -1)
			close(std_output[0]);
		if (std_output[1] != -1)
			close(std_output[0]);
		return (1);
	case 0:				/* Utility. */
		/* Redirect stdout to the write end of the pipe. */
		(void)dup2(std_output[1], STDOUT_FILENO);

		/* Close the utility's file descriptors. */
		(void)close(std_output[0]);
		(void)close(std_output[1]);
		(void)close(STDERR_FILENO);

		/*
		 * XXX
		 * Assume that all shells have -c.
		 */
		execl(sh_path, sh, "-c", bp, NULL);
		msgq_str(sp, M_SYSERR, sh_path, "118|Error: execl: %s");
		_exit(127);
	default:			/* Parent. */
		/* Close the pipe ends the parent won't use. */
		(void)close(std_output[1]);
		break;
	}

	/*
	 * Copy process standard output into a buffer.
	 *
	 * !!!
	 * Historic vi apparently discarded leading \n and \r's from
	 * the shell output stream.  We don't on the grounds that any
	 * shell that does that is broken.
	 */
	for (p = bp, len = 0, ch = EOF;
	    (ch = getc(ifp)) != EOF; *p++ = ch, --blen, ++len)
		if (blen < 5) {
			ADD_SPACE_GOTO(sp, bp, *blenp, *blenp * 2);
			p = bp + len;
			blen = *blenp - len;
		}

	/* Delete the final newline, nul terminate the string. */
	if (p > bp && (p[-1] == '\n' || p[-1] == '\r')) {
		--p;
		--len;
	}
	*p = '\0';
	*lenp = len;
	*bpp = bp;		/* *blenp is already updated. */

	if (ferror(ifp))
		goto ioerr;
	if (fclose(ifp)) {
ioerr:		msgq_str(sp, M_ERR, sh, "119|I/O error: %s");
alloc_err:	rval = SEXP_ERR;
	} else
		rval = SEXP_OK;

	/*
	 * Wait for the process.  If the shell process fails (e.g., "echo $q"
	 * where q wasn't a defined variable) or if the returned string has
	 * no characters or only blank characters, (e.g., "echo $5"), complain
	 * that the shell expansion failed.  We can't know for certain that's
	 * the error, but it's a good guess, and it matches historic practice.
	 * This won't catch "echo foo_$5", but that's not a common error and
	 * historic vi didn't catch it either.
	 */
	if (proc_wait(sp, (long)pid, sh, 1, 0))
		rval = SEXP_EXPANSION_ERR;

	for (p = bp; len; ++p, --len)
		if (!isblank(*p))
			break;
	if (len == 0)
		rval = SEXP_EXPANSION_ERR;

	if (rval == SEXP_EXPANSION_ERR)
		msgq(sp, M_ERR, "304|Shell expansion failed");

	return (rval == SEXP_OK ? 0 : 1);
}
