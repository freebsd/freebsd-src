/*-
 * Copyright (c) 1992, 1993, 1994
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
static char sccsid[] = "@(#)ex.c	8.106 (Berkeley) 3/23/94";
#endif /* not lint */

#include <sys/types.h>
#include <queue.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <bitstring.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include <db.h>
#include <regex.h>

#include "vi.h"
#include "excmd.h"

static inline EXCMDLIST const *
		ex_comm_search __P((char *, size_t));
static int	ep_line __P((SCR *, EXF *, MARK *, char **, size_t *, int *));
static int	ep_range __P((SCR *, EXF *, EXCMDARG *, char **, size_t *));

#define	DEFCOM	".+1"

/*
 * ex --
 *	Read an ex command and execute it.
 */
int
ex(sp, ep)
	SCR *sp;
	EXF *ep;
{
	TEXT *tp;
	u_int flags, saved_mode;
	int eval;
	char defcom[sizeof(DEFCOM)];

	if (ex_init(sp, ep))
		return (1);

	if (sp->s_refresh(sp, ep))
		return (ex_end(sp));

	/* If reading from a file, messages should have line info. */
	if (!F_ISSET(sp->gp, G_STDIN_TTY)) {
		sp->if_lno = 1;
		sp->if_name = strdup("input");
	}

	/*
	 * !!!
	 * Historically, the beautify option applies to ex command input read
	 * from a file.  In addition, the first time a ^H was discarded from
	 * the input, a message "^H discarded" was displayed.  We don't bother.
	 */
	LF_INIT(TXT_CNTRLD | TXT_CR | TXT_PROMPT);
	if (O_ISSET(sp, O_BEAUTIFY))
		LF_SET(TXT_BEAUTIFY);

	for (eval = 0;; ++sp->if_lno) {
		/* Get the next command. */
		switch (sp->s_get(sp, ep, &sp->tiq, ':', flags)) {
		case INP_OK:
			break;
		case INP_EOF:
		case INP_ERR:
			F_SET(sp, S_EXIT_FORCE);
			goto ret;
		}

		saved_mode = F_ISSET(sp, S_SCREENS | S_MAJOR_CHANGE);
		tp = sp->tiq.cqh_first;
		if (tp->len == 0) {
			if (F_ISSET(sp->gp, G_STDIN_TTY)) {
				/* Special case \r command. */
				(void)fputc('\r', stdout);
				(void)fflush(stdout);
			}
			memmove(defcom, DEFCOM, sizeof(DEFCOM));
			if (ex_icmd(sp, ep, defcom, sizeof(DEFCOM) - 1) &&
			    !F_ISSET(sp->gp, G_STDIN_TTY))
				F_SET(sp, S_EXIT_FORCE);
		} else {
			if (F_ISSET(sp->gp, G_STDIN_TTY))
				/* Special case ^D command. */
				if (tp->len == 1 && tp->lb[0] == '\004') {
					(void)fputc('\r', stdout);
					(void)fflush(stdout);
				} else
					(void)fputc('\n', stdout);
			if (ex_icmd(sp, ep, tp->lb, tp->len) &&
			    !F_ISSET(sp->gp, G_STDIN_TTY))
				F_SET(sp, S_EXIT_FORCE);
		}
		(void)msg_rpt(sp, 0);
		if (saved_mode != F_ISSET(sp, S_SCREENS | S_MAJOR_CHANGE))
			break;

		if (sp->s_refresh(sp, ep)) {
			eval = 1;
			break;
		}
	}
ret:	if (sp->if_name != NULL) {
		FREE(sp->if_name, strlen(sp->if_name) + 1);
		sp->if_name = NULL;
	}
	return (ex_end(sp) || eval);
}

/*
 * ex_cfile --
 *	Execute ex commands from a file.
 */
int
ex_cfile(sp, ep, filename)
	SCR *sp;
	EXF *ep;
	char *filename;
{
	struct stat sb;
	int fd, len, rval;
	char *bp;

	bp = NULL;
	if ((fd = open(filename, O_RDONLY, 0)) < 0 || fstat(fd, &sb))
		goto err;

	/*
	 * XXX
	 * We'd like to test if the file is too big to malloc.  Since we don't
	 * know what size or type off_t's or size_t's are, what the largest
	 * unsigned integral type is, or what random insanity the local C
	 * compiler will perpetrate, doing the comparison in a portable way
	 * is flatly impossible.  Hope that malloc fails if the file is too
	 * large.
	 */
	MALLOC(sp, bp, char *, (size_t)sb.st_size + 1);
	if (bp == NULL)
		goto err;

	len = read(fd, bp, (int)sb.st_size);
	if (len == -1 || len != sb.st_size) {
		if (len != sb.st_size)
			errno = EIO;
err:		rval = 1;
		msgq(sp, M_SYSERR, filename);
	} else {
		bp[sb.st_size] = '\0';		/* XXX */

		/*
		 * Run the command.  Messages include file/line information,
		 * but we don't care if we can't get space.
		 */
		sp->if_lno = 1;
		sp->if_name = strdup(filename);
		F_SET(sp, S_VLITONLY);
		rval = ex_icmd(sp, ep, bp, len);
		F_CLR(sp, S_VLITONLY);
		free(sp->if_name);
		sp->if_name = NULL;
	}

	/*
	 * !!!
	 * THE UNDERLYING EXF MAY HAVE CHANGED.
	 */
	if (bp != NULL)
		FREE(bp, sb.st_size);
	if (fd >= 0)
		(void)close(fd);
	return (rval);
}

/*
 * ex_icmd --
 *	Call ex_cmd() after turning off interruptible bits.
 */
int
ex_icmd(sp, ep, cmd, len)
	SCR *sp;
	EXF *ep;
	char *cmd;
	size_t len;
{
	/*
	 * Ex goes through here for each vi :colon command and for each ex
	 * command, however, globally executed commands don't go through
	 * here, instead, they call ex_cmd directly.  So, reset all of the
	 * interruptible flags now.
	 */
	F_CLR(sp, S_INTERRUPTED | S_INTERRUPTIBLE);

	return (ex_cmd(sp, ep, cmd, len));
}

/* Special command structure for :s as a repeat substitution command. */
static EXCMDLIST const cmd_subagain =
	{"s",		ex_subagain,	E_ADDR2|E_NORC,
	    "s",
	    "[line [,line]] s [cgr] [count] [#lp]",
	    "repeat the last subsitution"};

/*
 * ex_cmd --
 *	Parse and execute a string containing ex commands.
 */
int
ex_cmd(sp, ep, cmd, cmdlen)
	SCR *sp;
	EXF *ep;
	char *cmd;
	size_t cmdlen;
{
	EX_PRIVATE *exp;
	EXCMDARG exc;
	EXCMDLIST const *cp;
	MARK cur;
	recno_t lno, num;
	size_t arg1_len, len, save_cmdlen;
	long flagoff;
	u_int saved_mode;
	int ch, cnt, delim, flags, namelen, nl, uselastcmd, tmp;
	char *arg1, *save_cmd, *p, *t;

	/* Init. */
	nl = 0;
loop:	if (nl) {
		nl = 0;
		++sp->if_lno;
	}
	arg1 = NULL;
	save_cmdlen = 0;

	/*
	 * It's possible that we've been interrupted during a
	 * command.
	 */
	if (F_ISSET(sp, S_INTERRUPTED))
		return (0);

	/* Skip whitespace, separators, newlines. */
	for (; cmdlen > 0; ++cmd, --cmdlen)
		if ((ch = *cmd) == '\n')
			++sp->if_lno;
		else if (!isblank(ch))
			break;
	if (cmdlen == 0)
		return (0);

	/* Command lines that start with a double-quote are comments. */
	if (ch == '"') {
		while (--cmdlen > 0 && *++cmd != '\n');
		if (*cmd == '\n') {
			++cmd;
			--cmdlen;
			++sp->if_lno;
		}
		goto loop;
	}

	/*
	 * !!!
	 * Permit extra colons at the start of the line.  Historically,
	 * ex/vi allowed a single extra one.  It's simpler not to count.
	 * The stripping is done here because, historically, any command
	 * could have preceding colons, e.g. ":g/pattern/:p" worked.
	 */
	if (ch == ':')
		while (--cmdlen > 0 && *++cmd == ':');

	/* Skip whitespace. */
	for (; cmdlen > 0; ++cmd, --cmdlen) {
		ch = *cmd;
		if (!isblank(ch))
			break;
	}

	/* The last point at which an empty line means do nothing. */
	if (cmdlen == 0)
		return (0);

	/* Initialize the structure passed to underlying functions. */
	memset(&exc, 0, sizeof(EXCMDARG));
	exp = EXP(sp);
	if (argv_init(sp, ep, &exc))
		goto err;

	/* Parse command addresses. */
	if (ep_range(sp, ep, &exc, &cmd, &cmdlen))
		goto err;

	/* Skip whitespace. */
	for (; cmdlen > 0; ++cmd, --cmdlen) {
		ch = *cmd;
		if (!isblank(ch))
			break;
	}

	/*
	 * If no command, ex does the last specified of p, l, or #, and vi
	 * moves to the line.  Otherwise, determine the length of the command
	 * name by looking for the first non-alphabetic character.  (There
	 * are a few non-alphabetic characters in command names, but they're
	 * all single character commands.)  This isn't a great test, because
	 * it means that, for the command ":e +cut.c file", we'll report that
	 * the command "cut" wasn't known.  However, it makes ":e+35 file" work
	 * correctly.
	 */
#define	SINGLE_CHAR_COMMANDS	"\004!#&<=>@~"
	if (cmdlen != 0 && cmd[0] != '|' && cmd[0] != '\n') {
		if (strchr(SINGLE_CHAR_COMMANDS, *cmd)) {
			p = cmd;
			++cmd;
			--cmdlen;
			namelen = 1;
		} else {
			for (p = cmd; cmdlen > 0; --cmdlen, ++cmd)
				if (!isalpha(*cmd))
					break;
			if ((namelen = cmd - p) == 0) {
				msgq(sp, M_ERR, "Unknown command name.");
				goto err;
			}
		}

		/*
		 * Search the table for the command.
		 *
		 * !!!
		 * Historic vi permitted the mark to immediately follow the
		 * 'k' in the 'k' command.  Make it work.
		 *
		 * !!!
		 * Historic vi permitted pretty much anything to follow the
		 * substitute command, e.g. "s/e/E/|s|sgc3p" was fine.  Make
		 * it work.
		 */
		if ((cp = ex_comm_search(p, namelen)) == NULL)
			if (p[0] == 'k' && p[1] && !p[2]) {
				cmd -= namelen - 1;
				cmdlen += namelen - 1;
				cp = &cmds[C_K];
			} else if (p[0] == 's') {
				cmd -= namelen - 1;
				cmdlen += namelen - 1;
				cp = &cmd_subagain;
			} else {
				msgq(sp, M_ERR,
				    "The %.*s command is unknown.", namelen, p);
				goto err;
			}

		/* Some commands are either not implemented or turned off. */
		if (F_ISSET(cp, E_NOPERM)) {
			msgq(sp, M_ERR,
			    "The %s command is not currently supported.",
			    cp->name);
			goto err;
		}

		/* Some commands aren't okay in globals. */
		if (F_ISSET(sp, S_GLOBAL) && F_ISSET(cp, E_NOGLOBAL)) {
			msgq(sp, M_ERR,
		"The %s command can't be used as part of a global command.",
			    cp->name);
			goto err;
		}

		/*
		 * Multiple < and > characters; another "feature".  Note,
		 * The string passed to the underlying function may not be
		 * nul terminated in this case.
		 */
		if ((cp == &cmds[C_SHIFTL] && *p == '<') ||
		    (cp == &cmds[C_SHIFTR] && *p == '>')) {
			for (ch = *p; cmdlen > 0; --cmdlen, ++cmd)
				if (*cmd != ch)
					break;
			if (argv_exp0(sp, ep, &exc, p, cmd - p))
				goto err;
		}

		/*
		 * The visual command has a different syntax when called
		 * from ex than when called from a vi colon command.  FMH.
		 */
		if (cp == &cmds[C_VISUAL_EX] && IN_VI_MODE(sp))
			cp = &cmds[C_VISUAL_VI];

		uselastcmd = 0;
	} else {
		cp = exp->lastcmd;
		uselastcmd = 1;
	}

	/* Initialize local flags to the command flags. */
	LF_INIT(cp->flags);

	/*
	 * File state must be checked throughout this code, because it is
	 * called when reading the .exrc file and similar things.  There's
	 * this little chicken and egg problem -- if we read the file first,
	 * we won't know how to display it.  If we read/set the exrc stuff
	 * first, we can't allow any command that requires file state.  We
	 * don't have a "reading an rc" bit, because we want the commands
	 * to work when the user source's the rc file later.  Historic vi
	 * generally took the easy way out and dropped core.
 	 */
	if (LF_ISSET(E_NORC) && ep == NULL) {
		msgq(sp, M_ERR,
	"The %s command requires that a file have already been read in.",
		    cp->name);
		goto err;
	}

	/*
	 * There are three normal termination cases for an ex command.  They
	 * are the end of the string (cmdlen), or unescaped (by literal next
	 * characters) newline or '|' characters.  As we're past any addresses,
	 * we can now determine how long the command is, so we don't have to
	 * look for all the possible terminations.  There are three exciting
	 * special cases:
	 *
	 * 1: The bang, global, vglobal and the filter versions of the read and
	 *    write commands are delimited by newlines (they can contain shell
	 *    pipes).
	 * 2: The ex, edit and visual in vi mode commands take ex commands as
	 *    their first arguments.
	 * 3: The substitute command takes an RE as its first argument, and
	 *    wants it to be specially delimited.
	 *
	 * Historically, '|' characters in the first argument of the ex, edit,
	 * and substitute commands did not delimit the command.  And, in the
	 * filter cases for read and write, and the bang, global and vglobal
	 * commands, they did not delimit the command at all.
	 *
	 * For example, the following commands were legal:
	 *
	 *	:edit +25|s/abc/ABC/ file.c
	 *	:substitute s/|/PIPE/
	 *	:read !spell % | columnate
	 *	:global/pattern/p|l
	 *
	 * It's not quite as simple as it sounds, however.  The command:
	 *
	 *	:substitute s/a/b/|s/c/d|set
	 *
	 * was also legal, i.e. the historic ex parser (using the word loosely,
	 * since "parser" implies some regularity) delimited the RE's based on
	 * its delimiter and not anything so irretrievably vulgar as a command
	 * syntax.
	 *
	 * One thing that makes this easier is that we can ignore most of the
	 * command termination conditions for the commands that want to take
	 * the command up to the next newline.  None of them are legal in .exrc
	 * files, so if we're here, we only dealing with a single line, and we
	 * can just eat it.
	 *
	 * Anyhow, the following code makes this all work.  First, for the
	 * special cases we move past their special argument.  Then, we do
	 * normal command processing on whatever is left.  Barf-O-Rama.
	 */
	arg1_len = 0;
	save_cmd = cmd;
	if (cp == &cmds[C_EDIT] ||
	    cp == &cmds[C_EX] || cp == &cmds[C_VISUAL_VI]) {
		/*
		 * Move to the next non-whitespace character.  As '+' must
		 * be the character after the command name, if there isn't
		 * one, we're done.
		 */
		for (; cmdlen > 0; --cmdlen, ++cmd) {
			ch = *cmd;
			if (!isblank(ch))
				break;
		}
		/*
		 * QUOTING NOTE:
		 *
		 * The historic implementation ignored all escape characters
		 * so there was no way to put a space or newline into the +cmd
		 * field.  We do a simplistic job of fixing it by moving to the
		 * first whitespace character that isn't escaped by a literal
		 * next character.  The literal next characters are stripped
		 * as they're no longer useful.
		 */
		if (cmdlen > 0 && ch == '+') {
			++cmd;
			--cmdlen;
			for (arg1 = p = cmd; cmdlen > 0; --cmdlen, ++cmd) {
				ch = *cmd;
				if (IS_ESCAPE(sp, ch) && cmdlen > 1) {
					--cmdlen;
					ch = *++cmd;
				} else if (isblank(ch))
					break;
				*p++ = ch;
			}
			arg1_len = cmd - arg1;

			/* Reset, so the first argument isn't reparsed. */
			save_cmd = cmd;
		}
	} else if (cp == &cmds[C_BANG] ||
	    cp == &cmds[C_GLOBAL] || cp == &cmds[C_VGLOBAL]) {
		cmd += cmdlen;
		cmdlen = 0;
	} else if (cp == &cmds[C_READ] || cp == &cmds[C_WRITE]) {
		/*
		 * Move to the next character.  If it's a '!', it's a filter
		 * command and we want to eat it all, otherwise, we're done.
		 */
		for (; cmdlen > 0; --cmdlen, ++cmd) {
			ch = *cmd;
			if (!isblank(ch))
				break;
		}
		if (cmdlen > 0 && ch == '!') {
			cmd += cmdlen;
			cmdlen = 0;
		}
	} else if (cp == &cmds[C_SUBSTITUTE]) {
		/*
		 * Move to the next non-whitespace character, we'll use it as
		 * the delimiter.  If the character isn't an alphanumeric or
		 * a '|', it's the delimiter, so parse it.  Otherwise, we're
		 * into something like ":s g", so use the special substitute
		 * command.
		 */
		for (; cmdlen > 0; --cmdlen, ++cmd)
			if (!isblank(cmd[0]))
				break;

		if (isalnum(cmd[0]) || cmd[0] == '|')
			cp = &cmd_subagain;
		else if (cmdlen > 0) {
			/*
			 * QUOTING NOTE:
			 *
			 * Backslashes quote delimiter characters for RE's.
			 * The backslashes are NOT removed since they'll be
			 * used by the RE code.  Move to the third delimiter
			 * that's not escaped (or the end of the command).
			 */
			delim = *cmd;
			++cmd;
			--cmdlen;
			for (cnt = 2; cmdlen > 0 && cnt; --cmdlen, ++cmd)
				if (cmd[0] == '\\' && cmdlen > 1) {
					++cmd;
					--cmdlen;
				} else if (cmd[0] == delim)
					--cnt;
		}
	}
	/*
	 * Use normal quoting and termination rules to find the end
	 * of this command.
	 *
	 * QUOTING NOTE:
	 *
	 * Historically, vi permitted ^V's to escape <newline>'s in the .exrc
	 * file.  It was almost certainly a bug, but that's what bug-for-bug
	 * compatibility means, Grasshopper.  Also, ^V's escape the command
	 * delimiters.  Literal next quote characters in front of the newlines,
	 * '|' characters or literal next characters are stripped as as they're
	 * no longer useful.
	 */
	for (p = cmd, cnt = 0; cmdlen > 0; --cmdlen, ++cmd) {
		ch = cmd[0];
		if (IS_ESCAPE(sp, ch) && cmdlen > 1) {
			tmp = cmd[1];
			if (tmp == '\n' || tmp == '|') {
				if (tmp == '\n')
					++sp->if_lno;
				--cmdlen;
				++cmd;
				++cnt;
				ch = tmp;
			}
		} else if (ch == '\n' || ch == '|') {
			if (ch == '\n')
				nl = 1;
			--cmdlen;
			break;
		}
		*p++ = ch;
	}

	/*
	 * Save off the next command information, go back to the
	 * original start of the command.
	 */
	p = cmd + 1;
	cmd = save_cmd;
	save_cmd = p;
	save_cmdlen = cmdlen;
	cmdlen = ((save_cmd - cmd) - 1) - cnt;

	/*
	 * !!!
	 * The "set tags" command historically used a backslash, not the
	 * user's literal next character, to escape whitespace.  Handle
	 * it here instead of complicating the argv_exp3() code.  Note,
	 * this isn't a particularly complex trap, and if backslashes were
	 * legal in set commands, this would have to be much more complicated.
	 */
	if (cp == &cmds[C_SET])
		for (p = cmd, len = cmdlen; len > 0; --len, ++p)
			if (*p == '\\')
				*p = LITERAL_CH;

	/*
	 * Set the default addresses.  It's an error to specify an address for
	 * a command that doesn't take them.  If two addresses are specified
	 * for a command that only takes one, lose the first one.  Two special
	 * cases here, some commands take 0 or 2 addresses.  For most of them
	 * (the E_ADDR2_ALL flag), 0 defaults to the entire file.  For one
	 * (the `!' command, the E_ADDR2_NONE flag), 0 defaults to no lines.
	 *
	 * Also, if the file is empty, some commands want to use an address of
	 * 0, i.e. the entire file is 0 to 0, and the default first address is
	 * 0.  Otherwise, an entire file is 1 to N and the default line is 1.
	 * Note, we also add the E_ZERO flag to the command flags, for the case
	 * where the 0 address is only valid if it's a default address.
	 *
	 * Also, set a flag if we set the default addresses.  Some commands
	 * (ex: z) care if the user specified an address of if we just used
	 * the current cursor.
	 */
	switch (LF_ISSET(E_ADDR1|E_ADDR2|E_ADDR2_ALL|E_ADDR2_NONE)) {
	case E_ADDR1:				/* One address: */
		switch (exc.addrcnt) {
		case 0:				/* Default cursor/empty file. */
			exc.addrcnt = 1;
			F_SET(&exc, E_ADDRDEF);
			if (LF_ISSET(E_ZERODEF)) {
				if (file_lline(sp, ep, &lno))
					goto err;
				if (lno == 0) {
					exc.addr1.lno = 0;
					LF_SET(E_ZERO);
				} else
					exc.addr1.lno = sp->lno;
			} else
				exc.addr1.lno = sp->lno;
			exc.addr1.cno = sp->cno;
			break;
		case 1:
			break;
		case 2:				/* Lose the first address. */
			exc.addrcnt = 1;
			exc.addr1 = exc.addr2;
		}
		break;
	case E_ADDR2_NONE:			/* Zero/two addresses: */
		if (exc.addrcnt == 0)		/* Default to nothing. */
			break;
		goto two;
	case E_ADDR2_ALL:			/* Zero/two addresses: */
		if (exc.addrcnt == 0) {		/* Default entire/empty file. */
			exc.addrcnt = 2;
			F_SET(&exc, E_ADDRDEF);
			if (file_lline(sp, ep, &exc.addr2.lno))
				goto err;
			if (LF_ISSET(E_ZERODEF) && exc.addr2.lno == 0) {
				exc.addr1.lno = 0;
				LF_SET(E_ZERO);
			} else
				exc.addr1.lno = 1;
			exc.addr1.cno = exc.addr2.cno = 0;
			F_SET(&exc, E_ADDR2_ALL);
			break;
		}
		/* FALLTHROUGH */
	case E_ADDR2:				/* Two addresses: */
two:		switch (exc.addrcnt) {
		case 0:				/* Default cursor/empty file. */
			exc.addrcnt = 2;
			F_SET(&exc, E_ADDRDEF);
			if (LF_ISSET(E_ZERODEF) && sp->lno == 1) {
				if (file_lline(sp, ep, &lno))
					goto err;
				if (lno == 0) {
					exc.addr1.lno = exc.addr2.lno = 0;
					LF_SET(E_ZERO);
				} else
					exc.addr1.lno = exc.addr2.lno = sp->lno;
			} else
				exc.addr1.lno = exc.addr2.lno = sp->lno;
			exc.addr1.cno = exc.addr2.cno = sp->cno;
			break;
		case 1:				/* Default to first address. */
			exc.addrcnt = 2;
			exc.addr2 = exc.addr1;
			break;
		case 2:
			break;
		}
		break;
	default:
		if (exc.addrcnt)		/* Error. */
			goto usage;
	}

	/*
	 * !!!
	 * The ^D scroll command historically scrolled half the screen size
	 * rows down, rounded down, or to EOF.  It was an error if the cursor
	 * was already at EOF.  (Leading addresses were permitted, but were
	 * then ignored.)
	 */
	if (cp == &cmds[C_SCROLL]) {
		exc.addrcnt = 2;
		exc.addr1.lno = sp->lno + 1;
		exc.addr2.lno = sp->lno + 1 + (O_VAL(sp, O_LINES) + 1) / 2;
		exc.addr1.cno = exc.addr2.cno = sp->cno;
		if (file_lline(sp, ep, &lno))
			goto err;
		if (lno != 0 && lno > sp->lno && exc.addr2.lno > lno)
			exc.addr2.lno = lno;
	}

	flagoff = 0;
	for (p = cp->syntax; *p != '\0'; ++p) {
		/*
		 * The write command is sensitive to leading whitespace, e.g.
		 * "write !" is different from "write!".  If not the write
		 * command, skip leading whitespace.
		 */
		if (cp != &cmds[C_WRITE])
			for (; cmdlen > 0; --cmdlen, ++cmd) {
				ch = *cmd;
				if (!isblank(ch))
					break;
			}

		/*
		 * Quit when reach the end of the command, unless it's a
		 * command that does its own parsing, in which case we want
		 * to build a reasonable argv for it.  This code guarantees
		 * that there will be an argv when the function gets called,
		 * so the correct test is for a length of 0, not for the
		 * argc > 0.
		 */
		if (cmdlen == 0 && *p != '!' && *p != 'S' && *p != 's')
			break;

		switch (*p) {
		case '!':				/* ! */
			if (*cmd == '!') {
				++cmd;
				--cmdlen;
				F_SET(&exc, E_FORCE);
			}
			break;
		case '1':				/* +, -, #, l, p */
			/*
			 * !!!
			 * Historically, some flags were ignored depending
			 * on where they occurred in the command line.  For
			 * example, in the command, ":3+++p--#", historic vi
			 * acted on the '#' flag, but ignored the '-' flags.
			 * It's unambiguous what the flags mean, so we just
			 * handle them regardless of the stupidity of their
			 * location.
			 */
			for (; cmdlen; --cmdlen, ++cmd)
				switch (*cmd) {
				case '+':
					++flagoff;
					break;
				case '-':
					--flagoff;
					break;
				case '#':
					F_SET(&exc, E_F_HASH);
					break;
				case 'l':
					F_SET(&exc, E_F_LIST);
					break;
				case 'p':
					F_SET(&exc, E_F_PRINT);
					break;
				default:
					goto end1;
				}
end1:			break;
		case '2':				/* -, ., +, ^ */
		case '3':				/* -, ., +, ^, = */
			for (; cmdlen; --cmdlen, ++cmd)
				switch (*cmd) {
				case '-':
					F_SET(&exc, E_F_DASH);
					break;
				case '.':
					F_SET(&exc, E_F_DOT);
					break;
				case '+':
					F_SET(&exc, E_F_PLUS);
					break;
				case '^':
					F_SET(&exc, E_F_CARAT);
					break;
				case '=':
					if (*p == '3') {
						F_SET(&exc, E_F_EQUAL);
						break;
					}
					/* FALLTHROUGH */
				default:
					goto end2;
				}
end2:			break;
		case 'b':				/* buffer */
			/*
			 * Digits can't be buffer names in ex commands, or the
			 * command "d2" would be a delete into buffer '2', and
			 * not a two-line deletion.
			 */
			if (!isdigit(cmd[0])) {
				exc.buffer = *cmd;
				++cmd;
				--cmdlen;
				F_SET(&exc, E_BUFFER);
			}
			break;
		case 'c':				/* count [01+a] */
			++p;
			/* Validate any signed value. */
			if (!isdigit(*cmd) &&
			    (*p != '+' || (*cmd != '+' && *cmd != '-')))
				break;
			/* If a signed value, set appropriate flags. */
			if (*cmd == '-')
				F_SET(&exc, E_COUNT_NEG);
			else if (*cmd == '+')
				F_SET(&exc, E_COUNT_POS);
/* 8-bit XXX */		if ((lno = strtol(cmd, &t, 10)) == 0 && *p != '0') {
				msgq(sp, M_ERR, "Count may not be zero.");
				goto err;
			}
			cmdlen -= (t - cmd);
			cmd = t;
			/*
			 * Count as address offsets occur in commands taking
			 * two addresses.  Historic vi practice was to use
			 * the count as an offset from the *second* address.
			 *
			 * Set a count flag; some underlying commands (see
			 * join) do different things with counts than with
			 * line addresses.
			 */
			if (*p == 'a') {
				exc.addr1 = exc.addr2;
				exc.addr2.lno = exc.addr1.lno + lno - 1;
			} else
				exc.count = lno;
			F_SET(&exc, E_COUNT);
			break;
		case 'f':				/* file */
			if (argv_exp2(sp, ep,
			    &exc, cmd, cmdlen, cp == &cmds[C_BANG]))
				goto err;
			goto countchk;
		case 'l':				/* line */
			if (ep_line(sp, ep, &cur, &cmd, &cmdlen, &tmp))
				goto err;
			/* Line specifications are always required. */
			if (!tmp) {
				msgq(sp, M_ERR,
				     "%s: bad line specification", cmd);
				goto err;
			}
			exc.lineno = cur.lno;
			break;
		case 'S':				/* string, file exp. */
			if (argv_exp1(sp, ep,
			    &exc, cmd, cmdlen, cp == &cmds[C_BANG]))
				goto err;
			goto addr2;
		case 's':				/* string */
			if (argv_exp0(sp, ep, &exc, cmd, cmdlen))
				goto err;
			goto addr2;
		case 'W':				/* word string */
			/*
			 * QUOTING NOTE:
			 *
			 * Literal next characters escape the following
			 * character.  Quoting characters are stripped
			 * here since they are no longer useful.
			 *
			 * First there was the word.
			 */
			for (p = t = cmd; cmdlen > 0; --cmdlen, ++cmd) {
				ch = *cmd;
				if (IS_ESCAPE(sp, ch) && cmdlen > 1) {
					--cmdlen;
					*p++ = *++cmd;
				} else if (isblank(ch)) {
					++cmd;
					--cmdlen;
					break;
				} else
					*p++ = ch;
			}
			if (argv_exp0(sp, ep, &exc, t, p - t))
				goto err;

			/* Delete intervening whitespace. */
			for (; cmdlen > 0; --cmdlen, ++cmd) {
				ch = *cmd;
				if (!isblank(ch))
					break;
			}
			if (cmdlen == 0)
				goto usage;

			/* Followed by the string. */
			for (p = t = cmd; cmdlen > 0; --cmdlen, ++cmd, ++p) {
				ch = *cmd;
				if (IS_ESCAPE(sp, ch) && cmdlen > 1) {
					--cmdlen;
					*p = *++cmd;
				} else
					*p = ch;
			}
			if (argv_exp0(sp, ep, &exc, t, p - t))
				goto err;
			goto addr2;
		case 'w':				/* word */
			if (argv_exp3(sp, ep, &exc, cmd, cmdlen))
				goto err;
countchk:		if (*++p != 'N') {		/* N */
				/*
				 * If a number is specified, must either be
				 * 0 or that number, if optional, and that
				 * number, if required.
				 */
				num = *p - '0';
				if ((*++p != 'o' || exp->argsoff != 0) &&
				    exp->argsoff != num)
					goto usage;
			}
			goto addr2;
		default:
			msgq(sp, M_ERR,
			    "Internal syntax table error (%s: %c).",
			    cp->name, *p);
		}
	}

	/* Skip trailing whitespace. */
	for (; cmdlen; --cmdlen) {
		ch = *cmd++;
		if (!isblank(ch))
			break;
	}

	/*
	 * There shouldn't be anything left, and no more required
	 * fields, i.e neither 'l' or 'r' in the syntax string.
	 */
	if (cmdlen || strpbrk(p, "lr")) {
usage:		msgq(sp, M_ERR, "Usage: %s.", cp->usage);
		goto err;
	}

	/* Verify that the addresses are legal. */
addr2:	switch (exc.addrcnt) {
	case 2:
		if (file_lline(sp, ep, &lno))
			goto err;
		/*
		 * Historic ex/vi permitted commands with counts to go past
		 * EOF.  So, for example, if the file only had 5 lines, the
		 * ex command "1,6>" would fail, but the command ">300"
		 * would succeed.  Since we don't want to have to make all
		 * of the underlying commands handle random line numbers,
		 * fix it here.
		 */
		if (exc.addr2.lno > lno)
			if (F_ISSET(&exc, E_COUNT))
				exc.addr2.lno = lno;
			else {
				if (lno == 0)
					msgq(sp, M_ERR, "The file is empty.");
				else
					msgq(sp, M_ERR,
					    "Only %lu line%s in the file",
					    lno, lno > 1 ? "s" : "");
				goto err;
			}
		/* FALLTHROUGH */
	case 1:
		num = exc.addr1.lno;
		/*
		 * If it's a "default vi command", zero is okay.  Historic
		 * vi allowed this, note, it's also the hack that allows
		 * "vi + nonexistent_file" to work.
		 */
		if (num == 0 && (!IN_VI_MODE(sp) || uselastcmd != 1) &&
		    !LF_ISSET(E_ZERO)) {
			msgq(sp, M_ERR,
			    "The %s command doesn't permit an address of 0.",
			    cp->name);
			goto err;
		}
		if (file_lline(sp, ep, &lno))
			goto err;
		if (num > lno) {
			if (lno == 0)
				msgq(sp, M_ERR, "The file is empty.");
			else
				msgq(sp, M_ERR, "Only %lu line%s in the file",
				    lno, lno > 1 ? "s" : "");
			goto err;
		}
		break;
	}

	/* If doing a default command, vi just moves to the line. */
	if (IN_VI_MODE(sp) && uselastcmd) {
		switch (exc.addrcnt) {
		case 2:
			sp->lno = exc.addr2.lno ? exc.addr2.lno : 1;
			sp->cno = exc.addr2.cno;
			break;
		case 1:
			sp->lno = exc.addr1.lno ? exc.addr1.lno : 1;
			sp->cno = exc.addr1.cno;
			break;
		}
		cmd = save_cmd;
		cmdlen = save_cmdlen;
		goto loop;
	}

	/* Reset "last" command. */
	if (LF_ISSET(E_SETLAST))
		exp->lastcmd = cp;

	/* Final setup for the command. */
	exc.cmd = cp;

#if defined(DEBUG) && 0
	TRACE(sp, "ex_cmd: %s", exc.cmd->name);
	if (exc.addrcnt > 0) {
		TRACE(sp, "\taddr1 %d", exc.addr1.lno);
		if (exc.addrcnt > 1)
			TRACE(sp, " addr2: %d", exc.addr2.lno);
		TRACE(sp, "\n");
	}
	if (exc.lineno)
		TRACE(sp, "\tlineno %d", exc.lineno);
	if (exc.flags)
		TRACE(sp, "\tflags %0x", exc.flags);
	if (F_ISSET(&exc, E_BUFFER))
		TRACE(sp, "\tbuffer %c", exc.buffer);
	TRACE(sp, "\n");
	if (exc.argc) {
		for (cnt = 0; cnt < exc.argc; ++cnt)
			TRACE(sp, "\targ %d: {%s}", cnt, exc.argv[cnt]);
		TRACE(sp, "\n");
	}
#endif
	/* Clear autoprint flag. */
	F_CLR(exp, EX_AUTOPRINT);

	/* Increment the command count if not called from vi. */
	if (!IN_VI_MODE(sp))
		++sp->ccnt;

	/*
	 * If file state and not doing a global command, log the start of
	 * an action.
	 */
	if (ep != NULL && !F_ISSET(sp, S_GLOBAL))
		(void)log_cursor(sp, ep);

	/* Save the current mode. */
	saved_mode = F_ISSET(sp, S_SCREENS | S_MAJOR_CHANGE);

	/* Do the command. */
	if ((cp->fn)(sp, ep, &exc))
		goto err;

#ifdef DEBUG
	/* Make sure no function left the temporary space locked. */
	if (F_ISSET(sp->gp, G_TMP_INUSE)) {
		F_CLR(sp->gp, G_TMP_INUSE);
		msgq(sp, M_ERR, "Error: ex: temporary buffer not released.");
		goto err;
	}
#endif
	if (saved_mode != F_ISSET(sp, S_SCREENS | S_MAJOR_CHANGE)) {
		/*
		 * Only here if the mode of the underlying file changed, e.g.
		 * the user switched files or is exiting.  There are two things
		 * that we might have to save.  First, any "+cmd" field set up
		 * for an ex/edit command will have to be saved for later, also,
		 * any not yet executed part of the current ex command.
		 *
		 *	:edit +25 file.c|s/abc/ABC/|1
		 *
		 * for example.
		 *
		 * The historic vi just hung, of course; we handle by
		 * pushing the keys onto the tty queue.  If we're
		 * switching modes to vi, since the commands are intended
		 * as ex commands, add the extra characters to make it
		 * work.
		 *
		 * For the fun of it, if you want to see if a vi clone got
		 * the ex argument parsing right, try:
 		 *
		 *	echo 'foo|bar' > file1; echo 'foo/bar' > file2;
		 *	vi
		 *	:edit +1|s/|/PIPE/|w file1| e file2|1 | s/\//SLASH/|wq
		 */
		if (arg1_len == NULL && save_cmdlen == 0)
			return (0);
		if (IN_VI_MODE(sp) && term_push(sp, "\n", 1, 0, 0))
			goto err;
		if (save_cmdlen != 0)
			if (term_push(sp, save_cmd, save_cmdlen, 0, 0))
				goto err;
		if (arg1 != NULL) {
			if (IN_VI_MODE(sp) && save_cmdlen != 0 &&
			    term_push(sp, "|", 1, 0, 0))
				goto err;
			if (term_push(sp, arg1, arg1_len, 0, 0))
				goto err;
		}
		if (IN_VI_MODE(sp) && term_push(sp, ":", 1, 0, 0))
			goto err;
		return (0);
	}

	if (IN_EX_MODE(sp) && ep != NULL) {
		/*
		 * The print commands have already handled the `print' flags.
		 * If so, clear them.  Don't return, autoprint may still have
		 * stuff to print out.
		 */
		 if (LF_ISSET(E_F_PRCLEAR))
			F_CLR(&exc, E_F_HASH | E_F_LIST | E_F_PRINT);

		/*
		 * If the command was successful, and there was an explicit
		 * flag to display the new cursor line, or we're in ex mode,
		 * autoprint is set, and a change was made, display the line.
		 */
		if (flagoff) {
			if (flagoff < 0) {
				if (sp->lno < -flagoff) {
					msgq(sp, M_ERR,
					    "Flag offset before line 1.");
					goto err;
				}
			} else {
				if (file_lline(sp, ep, &lno))
					goto err;
				if (sp->lno + flagoff > lno) {
					msgq(sp, M_ERR,
					    "Flag offset past end-of-file.");
					goto err;
				}
			}
			sp->lno += flagoff;
		}

		if (O_ISSET(sp, O_AUTOPRINT) &&
		    (F_ISSET(exp, EX_AUTOPRINT) || F_ISSET(cp, E_AUTOPRINT)))
			LF_INIT(E_F_PRINT);
		else
			LF_INIT(F_ISSET(&exc, E_F_HASH | E_F_LIST | E_F_PRINT));

		memset(&exc, 0, sizeof(EXCMDARG));
		exc.addrcnt = 2;
		exc.addr1.lno = exc.addr2.lno = sp->lno;
		exc.addr1.cno = exc.addr2.cno = sp->cno;
		switch (LF_ISSET(E_F_HASH | E_F_LIST | E_F_PRINT)) {
		case E_F_HASH:
			exc.cmd = &cmds[C_HASH];
			ex_number(sp, ep, &exc);
			break;
		case E_F_LIST:
			exc.cmd = &cmds[C_LIST];
			ex_list(sp, ep, &exc);
			break;
		case E_F_PRINT:
			exc.cmd = &cmds[C_PRINT];
			ex_pr(sp, ep, &exc);
			break;
		}
	}

	cmd = save_cmd;
	cmdlen = save_cmdlen;
	goto loop;
	/* NOTREACHED */

	/*
	 * On error, we discard any keys we have left, as well as any keys
	 * that were mapped.  The test of save_cmdlen isn't necessarily
	 * correct.  If we fail early enough we don't know if the entire
	 * string was a single command or not.  Try and guess, it's useful
	 * to know if part of the command was discarded.
	 */
err:	if (save_cmdlen == 0)
		for (; cmdlen; --cmdlen) {
			ch = *cmd++;
			if (IS_ESCAPE(sp, ch) && cmdlen > 1) {
				--cmdlen;
				++cmd;
			} else if (ch == '\n' || ch == '|') {
				if (cmdlen > 1)
					save_cmdlen = 1;
				break;
			}
		}
	if (save_cmdlen != 0)
		msgq(sp, M_ERR,
		    "Ex command failed: remaining command input discarded.");
	term_map_flush(sp, "Ex command failed");
	return (1);
}

/*
 * ep_range --
 *	Get a line range for ex commands.
 */
static int
ep_range(sp, ep, excp, cmdp, cmdlenp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *excp;
	char **cmdp;
	size_t *cmdlenp;
{
	MARK cur, savecursor;
	size_t cmdlen;
	int savecursor_set, tmp;
	char *cmd;

	/* Percent character is all lines in the file. */
	cmd = *cmdp;
	cmdlen = *cmdlenp;
	if (*cmd == '%') {
		excp->addr1.lno = 1;
		if (file_lline(sp, ep, &excp->addr2.lno))
			return (1);

		/* If an empty file, then the first line is 0, not 1. */
		if (excp->addr2.lno == 0)
			excp->addr1.lno = 0;
		excp->addr1.cno = excp->addr2.cno = 0;
		excp->addrcnt = 2;

		++*cmdp;
		--*cmdlenp;
		return (0);
	}

	/* Parse comma or semi-colon delimited line specs. */
	for (savecursor_set = 0, excp->addrcnt = 0; cmdlen > 0;)
		switch (*cmd) {
		case ';':		/* Semi-colon delimiter. */
			/*
			 * Comma delimiters delimit; semi-colon delimiters
			 * change the current address for the 2nd address
			 * to be the first address.  Trailing or multiple
			 * delimiters are discarded.
			 */
			if (excp->addrcnt == 0)
				goto done;
			if (!savecursor_set) {
				savecursor.lno = sp->lno;
				savecursor.cno = sp->cno;
				sp->lno = excp->addr1.lno;
				sp->cno = excp->addr1.cno;
				savecursor_set = 1;
			}
			++cmd;
			--cmdlen;
			break;
		case ',':		/* Comma delimiter. */
			/* If no addresses yet, defaults to ".". */
			if (excp->addrcnt == 0) {
				excp->addr1.lno = sp->lno;
				excp->addr1.cno = sp->cno;
				excp->addrcnt = 1;
			}
			/* FALLTHROUGH */
		case ' ':		/* Whitespace. */
		case '\t':		/* Whitespace. */
			++cmd;
			--cmdlen;
			break;
		default:
			if (ep_line(sp, ep, &cur, &cmd, &cmdlen, &tmp))
				return (1);
			if (!tmp)
				goto done;

			/*
			 * Extra addresses are discarded, starting with
			 * the first.
			 */
			switch (excp->addrcnt) {
			case 0:
				excp->addr1 = cur;
				excp->addrcnt = 1;
				break;
			case 1:
				excp->addr2 = cur;
				excp->addrcnt = 2;
				break;
			case 2:
				excp->addr1 = excp->addr2;
				excp->addr2 = cur;
				break;
			}
			break;
		}

	/*
	 * XXX
	 * This is probably not right behavior for savecursor -- need
	 * to figure out what the historical ex did for ";,;,;5p" or
	 * similar stupidity.
	 */
done:	if (savecursor_set) {
		sp->lno = savecursor.lno;
		sp->cno = savecursor.cno;
	}
	if (excp->addrcnt == 2 &&
	    (excp->addr2.lno < excp->addr1.lno ||
	    excp->addr2.lno == excp->addr1.lno &&
	    excp->addr2.cno < excp->addr1.cno)) {
		msgq(sp, M_ERR,
		    "The second address is smaller than the first.");
		return (1);
	}
	*cmdp = cmd;
	*cmdlenp = cmdlen;
	return (0);
}

/*
 * Get a single line address specifier.
 */
static int
ep_line(sp, ep, cur, cmdp, cmdlenp, addr_found)
	SCR *sp;
	EXF *ep;
	MARK *cur;
	char **cmdp;
	size_t *cmdlenp;
	int *addr_found;
{
	MARK m;
	long total;
	u_int flags;
	size_t cmdlen;
	int (*sf) __P((SCR *, EXF *, MARK *, MARK *, char *, char **, u_int *));
	char *cmd, *endp;

	*addr_found = 0;

	cmd = *cmdp;
	cmdlen = *cmdlenp;
	switch (*cmd) {
	case '$':				/* Last line in the file. */
		*addr_found = 1;
		cur->cno = 0;
		if (file_lline(sp, ep, &cur->lno))
			return (1);
		++cmd;
		--cmdlen;
		break;				/* Absolute line number. */
	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
		*addr_found = 1;
		/*
		 * The way the vi "previous context" mark worked was that
		 * "non-relative" motions set it.  While vi wasn't totally
		 * consistent about this, ANY numeric address was considered
		 * non-relative, and set the value.  Which is why we're
		 * hacking marks down here.
		 */
		if (IN_VI_MODE(sp)) {
			m.lno = sp->lno;
			m.cno = sp->cno;
			if (mark_set(sp, ep, ABSMARK1, &m, 1))
				return (1);
		}
		cur->cno = 0;
/* 8-bit XXX */	cur->lno = strtol(cmd, &endp, 10);
		cmdlen -= (endp - cmd);
		cmd = endp;
		break;
	case '\'':				/* Use a mark. */
		*addr_found = 1;
		if (cmdlen == 1) {
			msgq(sp, M_ERR, "No mark name supplied.");
			return (1);
		}
		if (mark_get(sp, ep, cmd[1], cur))
			return (1);
		cmd += 2;
		cmdlen -= 2;
		break;
	case '\\':				/* Search: forward/backward. */
		/*
		 * !!!
		 * I can't find any difference between // and \/ or
		 * between ?? and \?.  Mark Horton doesn't remember
		 * there being any difference.  C'est la vie.
		 */
		if (cmdlen < 2 || cmd[1] != '/' && cmd[1] != '?') {
			msgq(sp, M_ERR, "\\ not followed by / or ?.");
			return (1);
		}
		++cmd;
		--cmdlen;
		sf = cmd[0] == '/' ? f_search : b_search;
		goto search;
	case '/':				/* Search forward. */
		sf = f_search;
		goto search;
	case '?':				/* Search backward. */
		sf = b_search;
search:		if (ep == NULL) {
			msgq(sp, M_ERR,
	"A search address requires that a file have already been read in.");
			return (1);
		}
		*addr_found = 1;
		m.lno = sp->lno;
		m.cno = sp->cno;
		flags = SEARCH_MSG | SEARCH_PARSE | SEARCH_SET;
		if (sf(sp, ep, &m, &m, cmd, &endp, &flags))
			return (1);
		cur->lno = m.lno;
		cur->cno = m.cno;
		cmdlen -= (endp - cmd);
		cmd = endp;
		break;
	case '.':				/* Current position. */
		*addr_found = 1;
		cur->cno = sp->cno;

		/* If an empty file, then '.' is 0, not 1. */
		if (sp->lno == 1) {
			if (file_lline(sp, ep, &cur->lno))
				return (1);
			if (cur->lno != 0)
				cur->lno = 1;
		} else
			cur->lno = sp->lno;
		++cmd;
		--cmdlen;
		break;
	}

	/*
	 * Evaluate any offset.  Offsets are +/- any number, or any number
	 * of +/- signs, or any combination thereof.  If no address found
	 * yet, offset is relative to ".".
	 */
	for (total = 0; cmdlen > 0 && (cmd[0] == '-' || cmd[0] == '+');) {
		if (!*addr_found) {
			cur->lno = sp->lno;
			cur->cno = sp->cno;
			*addr_found = 1;
		}

		if (cmdlen > 1 && isdigit(cmd[1])) {
/* 8-bit XXX */		total += strtol(cmd, &endp, 10);
			cmdlen -= (endp - cmd);
			cmd = endp;
		} else {
			total += cmd[0] == '-' ? -1 : 1;
			--cmdlen;
			++cmd;
		}
	}

	if (*addr_found) {
		if (total < 0 && -total > cur->lno) {
			msgq(sp, M_ERR,
			    "Reference to a line number less than 0.");
			return (1);
		}
		cur->lno += total;

		*cmdp = cmd;
		*cmdlenp = cmdlen;
	}
	return (0);
}

/*
 * ex_is_abbrev -
 *	The vi text input routine needs to know if ex thinks this is
 *	an [un]abbreviate command, so it can turn off abbreviations.
 *	Usual ranting in the vi/v_ntext:txt_abbrev() routine.
 */
int
ex_is_abbrev(name, len)
	char *name;
	size_t len;
{
	EXCMDLIST const *cp;

	return ((cp = ex_comm_search(name, len)) != NULL &&
	    (cp == &cmds[C_ABBR] || cp == &cmds[C_UNABBREVIATE]));
}

/*
 * ex_is_unmap -
 *	The vi text input routine needs to know if ex thinks this is
 *	an unmap command, so it can turn off input mapping.  Usual
 *	ranting in the vi/v_ntext:txt_unmap() routine.
 */
int
ex_is_unmap(name, len)
	char *name;
	size_t len;
{
	EXCMDLIST const *cp;

	/*
	 * The command the vi input routines are really interested in
	 * is "unmap!", not just unmap.
	 */
	if (name[len - 1] != '!')
		return (0);
	--len;
	return ((cp = ex_comm_search(name, len)) != NULL &&
	    cp == &cmds[C_UNMAP]);
}

static inline EXCMDLIST const *
ex_comm_search(name, len)
	char *name;
	size_t len;
{
	EXCMDLIST const *cp;

	for (cp = cmds; cp->name != NULL; ++cp) {
		if (cp->name[0] > name[0])
			return (NULL);
		if (cp->name[0] != name[0])
			continue;
		if (!memcmp(name, cp->name, len))
			return (cp);
	}
	return (NULL);
}
