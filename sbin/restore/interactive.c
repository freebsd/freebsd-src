/*
 * Copyright (c) 1985 The Regents of the University of California.
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
 */

#ifndef lint
static char sccsid[] = "@(#)interactive.c	5.9 (Berkeley) 6/1/90";
#endif /* not lint */

#include "restore.h"
#include <protocols/dumprestore.h>
#include <setjmp.h>
#include <ufs/dir.h>

#define round(a, b) (((a) + (b) - 1) / (b) * (b))

/*
 * Things to handle interruptions.
 */
static jmp_buf reset;
static char *nextarg = NULL;

/*
 * Structure and routines associated with listing directories.
 */
struct afile {
	ino_t	fnum;		/* inode number of file */
	char	*fname;		/* file name */
	short	fflags;		/* extraction flags, if any */
	char	ftype;		/* file type, e.g. LEAF or NODE */
};
struct arglist {
	struct afile	*head;	/* start of argument list */
	struct afile	*last;	/* end of argument list */
	struct afile	*base;	/* current list arena */
	int		nent;	/* maximum size of list */
	char		*cmd;	/* the current command */
};
extern int fcmp();
extern char *fmtentry();
char *copynext();

/*
 * Read and execute commands from the terminal.
 */
runcmdshell()
{
	register struct entry *np;
	ino_t ino;
	static struct arglist alist = { 0, 0, 0, 0, 0 };
	char curdir[MAXPATHLEN];
	char name[MAXPATHLEN];
	char cmd[BUFSIZ];

	canon("/", curdir);
loop:
	if (setjmp(reset) != 0) {
		for (; alist.head < alist.last; alist.head++)
			freename(alist.head->fname);
		nextarg = NULL;
		volno = 0;
	}
	getcmd(curdir, cmd, name, &alist);
	switch (cmd[0]) {
	/*
	 * Add elements to the extraction list.
	 */
	case 'a':
		if (strncmp(cmd, "add", strlen(cmd)) != 0)
			goto bad;
		ino = dirlookup(name);
		if (ino == 0)
			break;
		if (mflag)
			pathcheck(name);
		treescan(name, ino, addfile);
		break;
	/*
	 * Change working directory.
	 */
	case 'c':
		if (strncmp(cmd, "cd", strlen(cmd)) != 0)
			goto bad;
		ino = dirlookup(name);
		if (ino == 0)
			break;
		if (inodetype(ino) == LEAF) {
			fprintf(stderr, "%s: not a directory\n", name);
			break;
		}
		(void) strcpy(curdir, name);
		break;
	/*
	 * Delete elements from the extraction list.
	 */
	case 'd':
		if (strncmp(cmd, "delete", strlen(cmd)) != 0)
			goto bad;
		np = lookupname(name);
		if (np == NIL || (np->e_flags & NEW) == 0) {
			fprintf(stderr, "%s: not on extraction list\n", name);
			break;
		}
		treescan(name, np->e_ino, deletefile);
		break;
	/*
	 * Extract the requested list.
	 */
	case 'e':
		if (strncmp(cmd, "extract", strlen(cmd)) != 0)
			goto bad;
		createfiles();
		createlinks();
		setdirmodes();
		if (dflag)
			checkrestore();
		volno = 0;
		break;
	/*
	 * List available commands.
	 */
	case 'h':
		if (strncmp(cmd, "help", strlen(cmd)) != 0)
			goto bad;
	case '?':
		fprintf(stderr, "%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
			"Available commands are:\n",
			"\tls [arg] - list directory\n",
			"\tcd arg - change directory\n",
			"\tpwd - print current directory\n",
			"\tadd [arg] - add `arg' to list of",
			" files to be extracted\n",
			"\tdelete [arg] - delete `arg' from",
			" list of files to be extracted\n",
			"\textract - extract requested files\n",
			"\tsetmodes - set modes of requested directories\n",
			"\tquit - immediately exit program\n",
			"\twhat - list dump header information\n",
			"\tverbose - toggle verbose flag",
			" (useful with ``ls'')\n",
			"\thelp or `?' - print this list\n",
			"If no `arg' is supplied, the current",
			" directory is used\n");
		break;
	/*
	 * List a directory.
	 */
	case 'l':
		if (strncmp(cmd, "ls", strlen(cmd)) != 0)
			goto bad;
		ino = dirlookup(name);
		if (ino == 0)
			break;
		printlist(name, ino, curdir);
		break;
	/*
	 * Print current directory.
	 */
	case 'p':
		if (strncmp(cmd, "pwd", strlen(cmd)) != 0)
			goto bad;
		if (curdir[1] == '\0')
			fprintf(stderr, "/\n");
		else
			fprintf(stderr, "%s\n", &curdir[1]);
		break;
	/*
	 * Quit.
	 */
	case 'q':
		if (strncmp(cmd, "quit", strlen(cmd)) != 0)
			goto bad;
		return;
	case 'x':
		if (strncmp(cmd, "xit", strlen(cmd)) != 0)
			goto bad;
		return;
	/*
	 * Toggle verbose mode.
	 */
	case 'v':
		if (strncmp(cmd, "verbose", strlen(cmd)) != 0)
			goto bad;
		if (vflag) {
			fprintf(stderr, "verbose mode off\n");
			vflag = 0;
			break;
		}
		fprintf(stderr, "verbose mode on\n");
		vflag++;
		break;
	/*
	 * Just restore requested directory modes.
	 */
	case 's':
		if (strncmp(cmd, "setmodes", strlen(cmd)) != 0)
			goto bad;
		setdirmodes();
		break;
	/*
	 * Print out dump header information.
	 */
	case 'w':
		if (strncmp(cmd, "what", strlen(cmd)) != 0)
			goto bad;
		printdumpinfo();
		break;
	/*
	 * Turn on debugging.
	 */
	case 'D':
		if (strncmp(cmd, "Debug", strlen(cmd)) != 0)
			goto bad;
		if (dflag) {
			fprintf(stderr, "debugging mode off\n");
			dflag = 0;
			break;
		}
		fprintf(stderr, "debugging mode on\n");
		dflag++;
		break;
	/*
	 * Unknown command.
	 */
	default:
	bad:
		fprintf(stderr, "%s: unknown command; type ? for help\n", cmd);
		break;
	}
	goto loop;
}

/*
 * Read and parse an interactive command.
 * The first word on the line is assigned to "cmd". If
 * there are no arguments on the command line, then "curdir"
 * is returned as the argument. If there are arguments
 * on the line they are returned one at a time on each
 * successive call to getcmd. Each argument is first assigned
 * to "name". If it does not start with "/" the pathname in
 * "curdir" is prepended to it. Finally "canon" is called to
 * eliminate any embedded ".." components.
 */
getcmd(curdir, cmd, name, ap)
	char *curdir, *cmd, *name;
	struct arglist *ap;
{
	register char *cp;
	static char input[BUFSIZ];
	char output[BUFSIZ];
#	define rawname input	/* save space by reusing input buffer */

	/*
	 * Check to see if still processing arguments.
	 */
	if (ap->head != ap->last) {
		strcpy(name, ap->head->fname);
		freename(ap->head->fname);
		ap->head++;
		return;
	}
	if (nextarg != NULL)
		goto getnext;
	/*
	 * Read a command line and trim off trailing white space.
	 */
	do	{
		fprintf(stderr, "restore > ");
		(void) fflush(stderr);
		(void) fgets(input, BUFSIZ, terminal);
	} while (!feof(terminal) && input[0] == '\n');
	if (feof(terminal)) {
		(void) strcpy(cmd, "quit");
		return;
	}
	for (cp = &input[strlen(input) - 2]; *cp == ' ' || *cp == '\t'; cp--)
		/* trim off trailing white space and newline */;
	*++cp = '\0';
	/*
	 * Copy the command into "cmd".
	 */
	cp = copynext(input, cmd);
	ap->cmd = cmd;
	/*
	 * If no argument, use curdir as the default.
	 */
	if (*cp == '\0') {
		(void) strcpy(name, curdir);
		return;
	}
	nextarg = cp;
	/*
	 * Find the next argument.
	 */
getnext:
	cp = copynext(nextarg, rawname);
	if (*cp == '\0')
		nextarg = NULL;
	else
		nextarg = cp;
	/*
	 * If it an absolute pathname, canonicalize it and return it.
	 */
	if (rawname[0] == '/') {
		canon(rawname, name);
	} else {
		/*
		 * For relative pathnames, prepend the current directory to
		 * it then canonicalize and return it.
		 */
		(void) strcpy(output, curdir);
		(void) strcat(output, "/");
		(void) strcat(output, rawname);
		canon(output, name);
	}
	expandarg(name, ap);
	strcpy(name, ap->head->fname);
	freename(ap->head->fname);
	ap->head++;
#	undef rawname
}

/*
 * Strip off the next token of the input.
 */
char *
copynext(input, output)
	char *input, *output;
{
	register char *cp, *bp;
	char quote;

	for (cp = input; *cp == ' ' || *cp == '\t'; cp++)
		/* skip to argument */;
	bp = output;
	while (*cp != ' ' && *cp != '\t' && *cp != '\0') {
		/*
		 * Handle back slashes.
		 */
		if (*cp == '\\') {
			if (*++cp == '\0') {
				fprintf(stderr,
					"command lines cannot be continued\n");
				continue;
			}
			*bp++ = *cp++;
			continue;
		}
		/*
		 * The usual unquoted case.
		 */
		if (*cp != '\'' && *cp != '"') {
			*bp++ = *cp++;
			continue;
		}
		/*
		 * Handle single and double quotes.
		 */
		quote = *cp++;
		while (*cp != quote && *cp != '\0')
			*bp++ = *cp++ | 0200;
		if (*cp++ == '\0') {
			fprintf(stderr, "missing %c\n", quote);
			cp--;
			continue;
		}
	}
	*bp = '\0';
	return (cp);
}

/*
 * Canonicalize file names to always start with ``./'' and
 * remove any imbedded "." and ".." components.
 */
canon(rawname, canonname)
	char *rawname, *canonname;
{
	register char *cp, *np;
	int len;

	if (strcmp(rawname, ".") == 0 || strncmp(rawname, "./", 2) == 0)
		(void) strcpy(canonname, "");
	else if (rawname[0] == '/')
		(void) strcpy(canonname, ".");
	else
		(void) strcpy(canonname, "./");
	(void) strcat(canonname, rawname);
	/*
	 * Eliminate multiple and trailing '/'s
	 */
	for (cp = np = canonname; *np != '\0'; cp++) {
		*cp = *np++;
		while (*cp == '/' && *np == '/')
			np++;
	}
	*cp = '\0';
	if (*--cp == '/')
		*cp = '\0';
	/*
	 * Eliminate extraneous "." and ".." from pathnames.
	 */
	for (np = canonname; *np != '\0'; ) {
		np++;
		cp = np;
		while (*np != '/' && *np != '\0')
			np++;
		if (np - cp == 1 && *cp == '.') {
			cp--;
			(void) strcpy(cp, np);
			np = cp;
		}
		if (np - cp == 2 && strncmp(cp, "..", 2) == 0) {
			cp--;
			while (cp > &canonname[1] && *--cp != '/')
				/* find beginning of name */;
			(void) strcpy(cp, np);
			np = cp;
		}
	}
}

/*
 * globals (file name generation)
 *
 * "*" in params matches r.e ".*"
 * "?" in params matches r.e. "."
 * "[...]" in params matches character class
 * "[...a-z...]" in params matches a through z.
 */
expandarg(arg, ap)
	char *arg;
	register struct arglist *ap;
{
	static struct afile single;
	struct entry *ep;
	int size;

	ap->head = ap->last = (struct afile *)0;
	size = expand(arg, 0, ap);
	if (size == 0) {
		ep = lookupname(arg);
		single.fnum = ep ? ep->e_ino : 0;
		single.fname = savename(arg);
		ap->head = &single;
		ap->last = ap->head + 1;
		return;
	}
	qsort((char *)ap->head, ap->last - ap->head, sizeof *ap->head, fcmp);
}

/*
 * Expand a file name
 */
expand(as, rflg, ap)
	char *as;
	int rflg;
	register struct arglist *ap;
{
	int		count, size;
	char		dir = 0;
	char		*rescan = 0;
	DIR		*dirp;
	register char	*s, *cs;
	int		sindex, rindex, lindex;
	struct direct	*dp;
	register char	slash; 
	register char	*rs; 
	register char	c;

	/*
	 * check for meta chars
	 */
	s = cs = as;
	slash = 0;
	while (*cs != '*' && *cs != '?' && *cs != '[') {	
		if (*cs++ == 0) {	
			if (rflg && slash)
				break; 
			else
				return (0) ;
		} else if (*cs == '/') {	
			slash++;
		}
	}
	for (;;) {	
		if (cs == s) {	
			s = "";
			break;
		} else if (*--cs == '/') {	
			*cs = 0;
			if (s == cs)
				s = "/";
			break;
		}
	}
	if ((dirp = rst_opendir(s)) != NULL)
		dir++;
	count = 0;
	if (*cs == 0)
		*cs++ = 0200;
	if (dir) {
		/*
		 * check for rescan
		 */
		rs = cs;
		do {	
			if (*rs == '/') { 
				rescan = rs; 
				*rs = 0; 
			}
		} while (*rs++);
		sindex = ap->last - ap->head;
		while ((dp = rst_readdir(dirp)) != NULL && dp->d_ino != 0) {
			if (!dflag && BIT(dp->d_ino, dumpmap) == 0)
				continue;
			if ((*dp->d_name == '.' && *cs != '.'))
				continue;
			if (gmatch(dp->d_name, cs)) {	
				if (addg(dp, s, rescan, ap) < 0)
					return (-1);
				count++;
			}
		}
		if (rescan) {	
			rindex = sindex; 
			lindex = ap->last - ap->head;
			if (count) {	
				count = 0;
				while (rindex < lindex) {	
					size = expand(ap->head[rindex].fname,
					    1, ap);
					if (size < 0)
						return (size);
					count += size;
					rindex++;
				}
			}
			bcopy((char *)&ap->head[lindex],
			     (char *)&ap->head[sindex],
			     (ap->last - &ap->head[rindex]) * sizeof *ap->head);
			ap->last -= lindex - sindex;
			*rescan = '/';
		}
	}
	s = as;
	while (c = *s)
		*s++ = (c&0177 ? c : '/');
	return (count);
}

/*
 * Check for a name match
 */
gmatch(s, p)
	register char	*s, *p;
{
	register int	scc;
	char		c;
	char		ok; 
	int		lc;

	if (scc = *s++)
		if ((scc &= 0177) == 0)
			scc = 0200;
	switch (c = *p++) {

	case '[':
		ok = 0; 
		lc = 077777;
		while (c = *p++) {	
			if (c == ']') {
				return (ok ? gmatch(s, p) : 0);
			} else if (c == '-') {	
				if (lc <= scc && scc <= (*p++))
					ok++ ;
			} else {	
				if (scc == (lc = (c&0177)))
					ok++ ;
			}
		}
		return (0);

	default:
		if ((c&0177) != scc)
			return (0) ;
		/* falls through */

	case '?':
		return (scc ? gmatch(s, p) : 0);

	case '*':
		if (*p == 0)
			return (1) ;
		s--;
		while (*s) {  
			if (gmatch(s++, p))
				return (1);
		}
		return (0);

	case 0:
		return (scc == 0);
	}
}

/*
 * Construct a matched name.
 */
addg(dp, as1, as3, ap)
	struct direct	*dp;
	char		*as1, *as3;
	struct arglist	*ap;
{
	register char	*s1, *s2;
	register int	c;
	char		buf[BUFSIZ];

	s2 = buf;
	s1 = as1;
	while (c = *s1++) {	
		if ((c &= 0177) == 0) {	
			*s2++ = '/';
			break;
		}
		*s2++ = c;
	}
	s1 = dp->d_name;
	while (*s2 = *s1++)
		s2++;
	if (s1 = as3) {	
		*s2++ = '/';
		while (*s2++ = *++s1)
			/* void */;
	}
	if (mkentry(buf, dp->d_ino, ap) == FAIL)
		return (-1);
}

/*
 * Do an "ls" style listing of a directory
 */
printlist(name, ino, basename)
	char *name;
	ino_t ino;
	char *basename;
{
	register struct afile *fp;
	register struct direct *dp;
	static struct arglist alist = { 0, 0, 0, 0, "ls" };
	struct afile single;
	DIR *dirp;

	if ((dirp = rst_opendir(name)) == NULL) {
		single.fnum = ino;
		single.fname = savename(name + strlen(basename) + 1);
		alist.head = &single;
		alist.last = alist.head + 1;
	} else {
		alist.head = (struct afile *)0;
		fprintf(stderr, "%s:\n", name);
		while (dp = rst_readdir(dirp)) {
			if (dp == NULL || dp->d_ino == 0)
				break;
			if (!dflag && BIT(dp->d_ino, dumpmap) == 0)
				continue;
			if (vflag == 0 &&
			    (strcmp(dp->d_name, ".") == 0 ||
			     strcmp(dp->d_name, "..") == 0))
				continue;
			if (!mkentry(dp->d_name, dp->d_ino, &alist))
				return;
		}
	}
	if (alist.head != 0) {
		qsort((char *)alist.head, alist.last - alist.head,
			sizeof *alist.head, fcmp);
		formatf(&alist);
		for (fp = alist.head; fp < alist.last; fp++)
			freename(fp->fname);
	}
	if (dirp != NULL)
		fprintf(stderr, "\n");
}

/*
 * Read the contents of a directory.
 */
mkentry(name, ino, ap)
	char *name;
	ino_t ino;
	register struct arglist *ap;
{
	register struct afile *fp;

	if (ap->base == NULL) {
		ap->nent = 20;
		ap->base = (struct afile *)calloc((unsigned)ap->nent,
			sizeof (struct afile));
		if (ap->base == NULL) {
			fprintf(stderr, "%s: out of memory\n", ap->cmd);
			return (FAIL);
		}
	}
	if (ap->head == 0)
		ap->head = ap->last = ap->base;
	fp = ap->last;
	fp->fnum = ino;
	fp->fname = savename(name);
	fp++;
	if (fp == ap->head + ap->nent) {
		ap->base = (struct afile *)realloc((char *)ap->base,
		    (unsigned)(2 * ap->nent * sizeof (struct afile)));
		if (ap->base == 0) {
			fprintf(stderr, "%s: out of memory\n", ap->cmd);
			return (FAIL);
		}
		ap->head = ap->base;
		fp = ap->head + ap->nent;
		ap->nent *= 2;
	}
	ap->last = fp;
	return (GOOD);
}

/*
 * Print out a pretty listing of a directory
 */
formatf(ap)
	register struct arglist *ap;
{
	register struct afile *fp;
	struct entry *np;
	int width = 0, w, nentry = ap->last - ap->head;
	int i, j, len, columns, lines;
	char *cp;

	if (ap->head == ap->last)
		return;
	for (fp = ap->head; fp < ap->last; fp++) {
		fp->ftype = inodetype(fp->fnum);
		np = lookupino(fp->fnum);
		if (np != NIL)
			fp->fflags = np->e_flags;
		else
			fp->fflags = 0;
		len = strlen(fmtentry(fp));
		if (len > width)
			width = len;
	}
	width += 2;
	columns = 80 / width;
	if (columns == 0)
		columns = 1;
	lines = (nentry + columns - 1) / columns;
	for (i = 0; i < lines; i++) {
		for (j = 0; j < columns; j++) {
			fp = ap->head + j * lines + i;
			cp = fmtentry(fp);
			fprintf(stderr, "%s", cp);
			if (fp + lines >= ap->last) {
				fprintf(stderr, "\n");
				break;
			}
			w = strlen(cp);
			while (w < width) {
				w++;
				fprintf(stderr, " ");
			}
		}
	}
}

/*
 * Comparison routine for qsort.
 */
fcmp(f1, f2)
	register struct afile *f1, *f2;
{

	return (strcmp(f1->fname, f2->fname));
}

/*
 * Format a directory entry.
 */
char *
fmtentry(fp)
	register struct afile *fp;
{
	static char fmtres[BUFSIZ];
	static int precision = 0;
	int i;
	register char *cp, *dp;

	if (!vflag) {
		fmtres[0] = '\0';
	} else {
		if (precision == 0)
			for (i = maxino; i > 0; i /= 10)
				precision++;
		(void) sprintf(fmtres, "%*d ", precision, fp->fnum);
	}
	dp = &fmtres[strlen(fmtres)];
	if (dflag && BIT(fp->fnum, dumpmap) == 0)
		*dp++ = '^';
	else if ((fp->fflags & NEW) != 0)
		*dp++ = '*';
	else
		*dp++ = ' ';
	for (cp = fp->fname; *cp; cp++)
		if (!vflag && (*cp < ' ' || *cp >= 0177))
			*dp++ = '?';
		else
			*dp++ = *cp;
	if (fp->ftype == NODE)
		*dp++ = '/';
	*dp++ = 0;
	return (fmtres);
}

/*
 * respond to interrupts
 */
void
onintr()
{
	if (command == 'i')
		longjmp(reset, 1);
	if (reply("restore interrupted, continue") == FAIL)
		done(1);
}
