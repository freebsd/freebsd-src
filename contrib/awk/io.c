/*
 * io.c --- routines for dealing with input and output and records
 */

/* 
 * Copyright (C) 1986, 1988, 1989, 1991-1997 the Free Software Foundation, Inc.
 * 
 * This file is part of GAWK, the GNU implementation of the
 * AWK Programming Language.
 * 
 * GAWK is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * GAWK is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 */

#include "awk.h"
#undef HAVE_MMAP	/* for now, probably forever */

#ifdef HAVE_SYS_PARAM_H
#undef RE_DUP_MAX	/* avoid spurious conflict w/regex.h */
#include <sys/param.h>
#endif /* HAVE_SYS_PARAM_H */

#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif /* HAVE_SYS_WAIT_H */

#ifdef HAVE_MMAP
#include <sys/mman.h>
#ifndef MAP_FAILED
#define MAP_FAILED	((caddr_t) -1)
#endif /* ! defined (MAP_FAILED) */
#endif /* HAVE_MMAP */

#ifndef O_RDONLY
#include <fcntl.h>
#endif
#ifndef O_ACCMODE
#define O_ACCMODE	(O_RDONLY|O_WRONLY|O_RDWR)
#endif

#include <assert.h>

#if ! defined(S_ISREG) && defined(S_IFREG)
#define	S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#endif

#if ! defined(S_ISDIR) && defined(S_IFDIR)
#define	S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif

#ifndef ENFILE
#define ENFILE EMFILE
#endif

#ifdef atarist
#include <stddef.h>
#endif

#if defined(MSDOS) || defined(OS2) || defined(WIN32)
#define PIPES_SIMULATED
#endif

static IOBUF *nextfile P((int skipping));
static int inrec P((IOBUF *iop));
static int iop_close P((IOBUF *iop));
struct redirect *redirect P((NODE *tree, int *errflg));
static void close_one P((void));
static int close_redir P((struct redirect *rp, int exitwarn));
#ifndef PIPES_SIMULATED
static int wait_any P((int interesting));
#endif
static IOBUF *gawk_popen P((char *cmd, struct redirect *rp));
static IOBUF *iop_open P((const char *file, const char *how, IOBUF *buf));
static IOBUF *iop_alloc P((int fd, const char *name, IOBUF *buf));
static int gawk_pclose P((struct redirect *rp));
static int do_pathopen P((const char *file));
static int get_a_record P((char **out, IOBUF *iop, int rs, Regexp *RSre, int *errcode));
#ifdef HAVE_MMAP
static int mmap_get_record P((char **out, IOBUF *iop, int rs, Regexp *RSre, int *errcode));
#endif /* HAVE_MMAP */
static int str2mode P((const char *mode));
static void spec_setup P((IOBUF *iop, int len, int allocate));
static int specfdopen P((IOBUF *iop, const char *name, const char *mode));
static int pidopen P((IOBUF *iop, const char *name, const char *mode));
static int useropen P((IOBUF *iop, const char *name, const char *mode));

#if defined (MSDOS) && !defined (__GO32__)
#include "popen.h"
#define popen(c, m)	os_popen(c, m)
#define pclose(f)	os_pclose(f)
#else
#if defined (OS2)	/* OS/2, but not family mode */
#if defined (_MSC_VER)
#define popen(c, m)	_popen(c, m)
#define pclose(f)	_pclose(f)
#endif
#else
extern FILE	*popen();
#endif
#endif

static struct redirect *red_head = NULL;
static NODE *RS;
static Regexp *RS_regexp;

int RS_is_null;

extern int output_is_tty;
extern NODE *ARGC_node;
extern NODE *ARGV_node;
extern NODE *ARGIND_node;
extern NODE *ERRNO_node;
extern NODE **fields_arr;

static jmp_buf filebuf;		/* for do_nextfile() */

/* do_nextfile --- implement gawk "nextfile" extension */

void
do_nextfile()
{
	(void) nextfile(TRUE);
	longjmp(filebuf, 1);
}

/* nextfile --- move to the next input data file */

static IOBUF *
nextfile(skipping)
int skipping;
{
	static long i = 1;
	static int files = 0;
	NODE *arg;
	static IOBUF *curfile = NULL;
	static IOBUF mybuf;
	const char *fname;

	if (skipping) {
		if (curfile != NULL)
			iop_close(curfile);
		curfile = NULL;
		return NULL;
	}
	if (curfile != NULL) {
		if (curfile->cnt == EOF) {
			(void) iop_close(curfile);
			curfile = NULL;
		} else
			return curfile;
	}
	for (; i < (long) (ARGC_node->lnode->numbr); i++) {
		arg = *assoc_lookup(ARGV_node, tmp_number((AWKNUM) i));
		if (arg->stlen == 0)
			continue;
		arg->stptr[arg->stlen] = '\0';
		if (! do_traditional) {
			unref(ARGIND_node->var_value);
			ARGIND_node->var_value = make_number((AWKNUM) i);
		}
		if (! arg_assign(arg->stptr)) {
			files++;
			fname = arg->stptr;
			curfile = iop_open(fname, "r", &mybuf);
			if (curfile == NULL)
				goto give_up;
			curfile->flag |= IOP_NOFREE_OBJ;
			/* This is a kludge.  */
			unref(FILENAME_node->var_value);
			FILENAME_node->var_value = dupnode(arg);
			FNR = 0;
			i++;
			break;
		}
	}
	if (files == 0) {
		files++;
		/* no args. -- use stdin */
		/* FNR is init'ed to 0 */
		FILENAME_node->var_value = make_string("-", 1);
		fname = "-";
		curfile = iop_open(fname, "r", &mybuf);
		if (curfile == NULL)
			goto give_up;
		curfile->flag |= IOP_NOFREE_OBJ;
	}
	return curfile;

 give_up:
	fatal("cannot open file `%s' for reading (%s)",
		fname, strerror(errno));
	/* NOTREACHED */
	return 0;
}

/* set_FNR --- update internal FNR from awk variable */

void
set_FNR()
{
	FNR = (long) FNR_node->var_value->numbr;
}

/* set_NR --- update internal NR from awk variable */

void
set_NR()
{
	NR = (long) NR_node->var_value->numbr;
}

/* inrec --- This reads in a record from the input file */

static int
inrec(iop)
IOBUF *iop;
{
	char *begin;
	register int cnt;
	int retval = 0;

	if ((cnt = iop->cnt) != EOF)
		cnt = (*(iop->getrec))
				(&begin, iop, RS->stptr[0], RS_regexp, NULL);
	if (cnt == EOF) {
		cnt = 0;
		retval = 1;
	} else {
		NR += 1;
		FNR += 1;
		set_record(begin, cnt, TRUE);
	}

	return retval;
}

/* iop_close --- close an open IOP */

static int
iop_close(iop)
IOBUF *iop;
{
	int ret;

	if (iop == NULL)
		return 0;
	errno = 0;

#ifdef _CRAY
	/* Work around bug in UNICOS popen */
	if (iop->fd < 3)
		ret = 0;
	else
#endif
	/* save these for re-use; don't free the storage */
	if ((iop->flag & IOP_IS_INTERNAL) != 0) {
		iop->off = iop->buf;
		iop->end = iop->buf + strlen(iop->buf);
		iop->cnt = 0;
		iop->secsiz = 0;
		return 0;
	}

	/* Don't close standard files or else crufty code elsewhere will lose */
	if (iop->fd == fileno(stdin)
	    || iop->fd == fileno(stdout)
	    || iop->fd == fileno(stderr)
	    || (iop->flag & IOP_MMAPPED) != 0)
		ret = 0;
	else
		ret = close(iop->fd);

	if (ret == -1)
		warning("close of fd %d (`%s') failed (%s)", iop->fd,
				iop->name, strerror(errno));
	if ((iop->flag & IOP_NO_FREE) == 0) {
		/*
		 * Be careful -- $0 may still reference the buffer even though
		 * an explicit close is being done; in the future, maybe we
		 * can do this a bit better.
		 */
		if (iop->buf) {
			if ((fields_arr[0]->stptr >= iop->buf)
			    && (fields_arr[0]->stptr < (iop->buf + iop->secsiz + iop->size))) {
				NODE *t;
	
				t = make_string(fields_arr[0]->stptr,
						fields_arr[0]->stlen);
				unref(fields_arr[0]);
				fields_arr[0] = t;
				reset_record();
			}
			if ((iop->flag & IOP_MMAPPED) == 0)
  				free(iop->buf);
#ifdef HAVE_MMAP
			else
				(void) munmap(iop->buf, iop->size);
#endif
		}
		if ((iop->flag & IOP_NOFREE_OBJ) == 0)
			free((char *) iop);
	}
	return ret == -1 ? 1 : 0;
}

/* do_input --- the main input processing loop */

void
do_input()
{
	IOBUF *iop;
	extern int exiting;

	(void) setjmp(filebuf);	/* for `nextfile' */

	while ((iop = nextfile(FALSE)) != NULL) {
		if (inrec(iop) == 0)
			while (interpret(expression_value) && inrec(iop) == 0)
				continue;
#ifdef C_ALLOCA
		/* recover any space from C based alloca */
		(void) alloca(0);
#endif
		if (exiting)
			break;
	}
}

/* redirect --- Redirection for printf and print commands */

struct redirect *
redirect(tree, errflg)
NODE *tree;
int *errflg;
{
	register NODE *tmp;
	register struct redirect *rp;
	register char *str;
	int tflag = 0;
	int outflag = 0;
	const char *direction = "to";
	const char *mode;
	int fd;
	const char *what = NULL;

	switch (tree->type) {
	case Node_redirect_append:
		tflag = RED_APPEND;
		/* FALL THROUGH */
	case Node_redirect_output:
		outflag = (RED_FILE|RED_WRITE);
		tflag |= outflag;
		if (tree->type == Node_redirect_output)
			what = ">";
		else
			what = ">>";
		break;
	case Node_redirect_pipe:
		tflag = (RED_PIPE|RED_WRITE);
		what = "|";
		break;
	case Node_redirect_pipein:
		tflag = (RED_PIPE|RED_READ);
		what = "|";
		break;
	case Node_redirect_input:
		tflag = (RED_FILE|RED_READ);
		what = "<";
		break;
	default:
		fatal("invalid tree type %d in redirect()", tree->type);
		break;
	}
	tmp = tree_eval(tree->subnode);
	if (do_lint && (tmp->flags & STR) == 0)
		warning("expression in `%s' redirection only has numeric value",
			what);
	tmp = force_string(tmp);
	str = tmp->stptr;

	if (str == NULL || *str == '\0')
		fatal("expression for `%s' redirection has null string value",
			what);

	if (do_lint
	    && (STREQN(str, "0", tmp->stlen) || STREQN(str, "1", tmp->stlen)))
		warning("filename `%s' for `%s' redirection may be result of logical expression", str, what);
	for (rp = red_head; rp != NULL; rp = rp->next)
		if (strlen(rp->value) == tmp->stlen
		    && STREQN(rp->value, str, tmp->stlen)
		    && ((rp->flag & ~(RED_NOBUF|RED_EOF)) == tflag
			|| (outflag != 0
			    && (rp->flag & (RED_FILE|RED_WRITE)) == outflag)))
			break;
	if (rp == NULL) {
		emalloc(rp, struct redirect *, sizeof(struct redirect),
			"redirect");
		emalloc(str, char *, tmp->stlen+1, "redirect");
		memcpy(str, tmp->stptr, tmp->stlen);
		str[tmp->stlen] = '\0';
		rp->value = str;
		rp->flag = tflag;
		rp->fp = NULL;
		rp->iop = NULL;
		rp->pid = 0;	/* unlikely that we're worried about init */
		rp->status = 0;
		/* maintain list in most-recently-used first order */
		if (red_head != NULL)
			red_head->prev = rp;
		rp->prev = NULL;
		rp->next = red_head;
		red_head = rp;
	} else
		str = rp->value;	/* get \0 terminated string */
	while (rp->fp == NULL && rp->iop == NULL) {
		if (rp->flag & RED_EOF)
			/*
			 * encountered EOF on file or pipe -- must be cleared
			 * by explicit close() before reading more
			 */
			return rp;
		mode = NULL;
		errno = 0;
		switch (tree->type) {
		case Node_redirect_output:
			mode = "w";
			if ((rp->flag & RED_USED) != 0)
				mode = "a";
			break;
		case Node_redirect_append:
			mode = "a";
			break;
		case Node_redirect_pipe:
			/* synchronize output before new pipe */
			(void) flush_io();

			if ((rp->fp = popen(str, "w")) == NULL)
				fatal("can't open pipe (\"%s\") for output (%s)",
					str, strerror(errno));
			rp->flag |= RED_NOBUF;
			break;
		case Node_redirect_pipein:
			direction = "from";
			if (gawk_popen(str, rp) == NULL)
				fatal("can't open pipe (\"%s\") for input (%s)",
					str, strerror(errno));
			break;
		case Node_redirect_input:
			direction = "from";
			rp->iop = iop_open(str, "r", NULL);
			break;
		default:
			cant_happen();
		}
		if (mode != NULL) {
			errno = 0;
			fd = devopen(str, mode);
			if (fd > INVALID_HANDLE) {
				if (fd == fileno(stdin))
					rp->fp = stdin;
				else if (fd == fileno(stdout))
					rp->fp = stdout;
				else if (fd == fileno(stderr))
					rp->fp = stderr;
				else {
					rp->fp = fdopen(fd, (char *) mode);
					/* don't leak file descriptors */
					if (rp->fp == NULL)
						close(fd);
				}
				if (rp->fp != NULL && isatty(fd))
					rp->flag |= RED_NOBUF;
			}
		}
		if (rp->fp == NULL && rp->iop == NULL) {
			/* too many files open -- close one and try again */
			if (errno == EMFILE || errno == ENFILE)
				close_one();
#ifdef HAVE_MMAP
			/* this works for solaris 2.5, not sunos */
			else if (errno == 0)	/* HACK! */
				close_one();
#endif
			else {
				/*
				 * Some other reason for failure.
				 *
				 * On redirection of input from a file,
				 * just return an error, so e.g. getline
				 * can return -1.  For output to file,
				 * complain. The shell will complain on
				 * a bad command to a pipe.
				 */
				if (errflg != NULL)
					*errflg = errno;
				if (tree->type == Node_redirect_output
				    || tree->type == Node_redirect_append)
					fatal("can't redirect %s `%s' (%s)",
					    direction, str, strerror(errno));
				else {
					free_temp(tmp);
					return NULL;
				}
			}
		}
	}
	free_temp(tmp);
	return rp;
}

/* getredirect --- find the struct redirect for this file or pipe */

struct redirect *
getredirect(str, len)
char *str;
int len;
{
	struct redirect *rp;

	for (rp = red_head; rp != NULL; rp = rp->next)
		if (strlen(rp->value) == len && STREQN(rp->value, str, len))
			return rp;

	return NULL;
}

/* close_one --- temporarily close an open file to re-use the fd */

static void
close_one()
{
	register struct redirect *rp;
	register struct redirect *rplast = NULL;

	/* go to end of list first, to pick up least recently used entry */
	for (rp = red_head; rp != NULL; rp = rp->next)
		rplast = rp;
	/* now work back up through the list */
	for (rp = rplast; rp != NULL; rp = rp->prev)
		if (rp->fp != NULL && (rp->flag & RED_FILE) != 0) {
			rp->flag |= RED_USED;
			errno = 0;
			if (/* do_lint && */ fclose(rp->fp) != 0)
				warning("close of \"%s\" failed (%s).",
					rp->value, strerror(errno));
			rp->fp = NULL;
			break;
		}
	if (rp == NULL)
		/* surely this is the only reason ??? */
		fatal("too many pipes or input files open"); 
}

/* do_close --- completely close an open file or pipe */

NODE *
do_close(tree)
NODE *tree;
{
	NODE *tmp;
	register struct redirect *rp;

	tmp = force_string(tree_eval(tree->subnode));

	/* icky special case: close(FILENAME) called. */
	if (tree->subnode == FILENAME_node
	    || (tmp->stlen == FILENAME_node->var_value->stlen
		&& STREQN(tmp->stptr, FILENAME_node->var_value->stptr, tmp->stlen))) {
		(void) nextfile(TRUE);
		free_temp(tmp);
		return tmp_number((AWKNUM) 0.0);
	}

	for (rp = red_head; rp != NULL; rp = rp->next) {
		if (strlen(rp->value) == tmp->stlen
		    && STREQN(rp->value, tmp->stptr, tmp->stlen))
			break;
	}
	if (rp == NULL) {	/* no match */
		if (do_lint)
			warning("close: `%.*s' is not an open file or pipe",
				tmp->stlen, tmp->stptr);
		free_temp(tmp);
		return tmp_number((AWKNUM) 0.0);
	}
	free_temp(tmp);
	fflush(stdout);	/* synchronize regular output */
	tmp = tmp_number((AWKNUM) close_redir(rp, FALSE));
	rp = NULL;
	return tmp;
}

/* close_redir --- close an open file or pipe */

static int
close_redir(rp, exitwarn)
register struct redirect *rp;
int exitwarn;
{
	int status = 0;
	char *what;

	if (rp == NULL)
		return 0;
	if (rp->fp == stdout || rp->fp == stderr)
		return 0;
	errno = 0;
	if ((rp->flag & (RED_PIPE|RED_WRITE)) == (RED_PIPE|RED_WRITE))
		status = pclose(rp->fp);
	else if (rp->fp != NULL)
		status = fclose(rp->fp);
	else if (rp->iop != NULL) {
		if ((rp->flag & RED_PIPE) != 0)
			status = gawk_pclose(rp);
		else {
			status = iop_close(rp->iop);
			rp->iop = NULL;
		}
	}

	what = ((rp->flag & RED_PIPE) != 0) ? "pipe" : "file";

	if (exitwarn) 
		warning("no explicit close of %s `%s' provided",
			what, rp->value);

	/* SVR4 awk checks and warns about status of close */
	if (status != 0) {
		char *s = strerror(errno);

		/*
		 * Too many people have complained about this.
		 * As of 2.15.6, it is now under lint control.
		 */
		if (do_lint)
			warning("failure status (%d) on %s close of \"%s\" (%s)",
				status, what, rp->value, s);

		if (! do_traditional) {
			/* set ERRNO too so that program can get at it */
			unref(ERRNO_node->var_value);
			ERRNO_node->var_value = make_string(s, strlen(s));
		}
	}
	if (rp->next != NULL)
		rp->next->prev = rp->prev;
	if (rp->prev != NULL)
		rp->prev->next = rp->next;
	else
		red_head = rp->next;
	free(rp->value);
	free((char *) rp);
	return status;
}

/* flush_io --- flush all open output files */

int
flush_io()
{
	register struct redirect *rp;
	int status = 0;

	errno = 0;
	if (fflush(stdout)) {
		warning("error writing standard output (%s)", strerror(errno));
		status++;
	}
	if (fflush(stderr)) {
		warning("error writing standard error (%s)", strerror(errno));
		status++;
	}
	for (rp = red_head; rp != NULL; rp = rp->next)
		/* flush both files and pipes, what the heck */
		if ((rp->flag & RED_WRITE) && rp->fp != NULL) {
			if (fflush(rp->fp)) {
				warning("%s flush of \"%s\" failed (%s).",
				    (rp->flag  & RED_PIPE) ? "pipe" :
				    "file", rp->value, strerror(errno));
				status++;
			}
		}
	return status;
}

/* close_io --- close all open files, called when exiting */

int
close_io()
{
	register struct redirect *rp;
	register struct redirect *next;
	int status = 0;

	errno = 0;
	for (rp = red_head; rp != NULL; rp = next) {
		next = rp->next;
		/*
		 * close_redir() will print a message if needed
		 * if do_lint, warn about lack of explicit close
		 */
		if (close_redir(rp, do_lint))
			status++;
		rp = NULL;
	}
	/*
	 * Some of the non-Unix os's have problems doing an fclose
	 * on stdout and stderr.  Since we don't really need to close
	 * them, we just flush them, and do that across the board.
	 */
	if (fflush(stdout)) {
		warning("error writing standard output (%s)", strerror(errno));
		status++;
	}
	if (fflush(stderr)) {
		warning("error writing standard error (%s)", strerror(errno));
		status++;
	}
	return status;
}

/* str2mode --- convert a string mode to an integer mode */

static int
str2mode(mode)
const char *mode;
{
	int ret;

	switch(mode[0]) {
	case 'r':
		ret = O_RDONLY;
		break;

	case 'w':
		ret = O_WRONLY|O_CREAT|O_TRUNC;
		break;

	case 'a':
		ret = O_WRONLY|O_APPEND|O_CREAT;
		break;

	default:
		ret = 0;		/* lint */
		cant_happen();
	}
	return ret;
}

/* devopen --- handle /dev/std{in,out,err}, /dev/fd/N, regular files */

/*
 * This separate version is still needed for output, since file and pipe
 * output is done with stdio. iop_open() handles input with IOBUFs of
 * more "special" files.  Those files are not handled here since it makes
 * no sense to use them for output.
 */

int
devopen(name, mode)
const char *name, *mode;
{
	int openfd;
	const char *cp;
	char *ptr;
	int flag = 0;
	struct stat buf;
	extern double strtod();

	flag = str2mode(mode);

	if (STREQ(name, "-"))
		openfd = fileno(stdin);
	else
		openfd = INVALID_HANDLE;

	if (do_traditional)
		goto strictopen;

	if ((openfd = os_devopen(name, flag)) >= 0)
		return openfd;

	if (STREQN(name, "/dev/", 5) && stat((char *) name, &buf) == -1) {
		cp = name + 5;
		
		if (STREQ(cp, "stdin") && (flag & O_ACCMODE) == O_RDONLY)
			openfd = fileno(stdin);
		else if (STREQ(cp, "stdout") && (flag & O_ACCMODE) == O_WRONLY)
			openfd = fileno(stdout);
		else if (STREQ(cp, "stderr") && (flag & O_ACCMODE) == O_WRONLY)
			openfd = fileno(stderr);
		else if (STREQN(cp, "fd/", 3)) {
			cp += 3;
			openfd = (int) strtod(cp, &ptr);
			if (openfd <= INVALID_HANDLE || ptr == cp)
				openfd = INVALID_HANDLE;
		}
	}

strictopen:
	if (openfd == INVALID_HANDLE)
		openfd = open(name, flag, 0666);
	if (openfd != INVALID_HANDLE && fstat(openfd, &buf) > 0) 
		if (S_ISDIR(buf.st_mode))
			fatal("file `%s' is a directory", name);
	return openfd;
}


/* spec_setup --- setup an IOBUF for a special internal file */

static void
spec_setup(iop, len, allocate)
IOBUF *iop;
int len;
int allocate;
{
	char *cp;

	if (allocate) {
		emalloc(cp, char *, len+2, "spec_setup");
		iop->buf = cp;
	} else {
		len = strlen(iop->buf);
		iop->buf[len++] = '\n';	/* get_a_record clobbered it */
		iop->buf[len] = '\0';	/* just in case */
	}
	iop->off = iop->buf;
	iop->cnt = 0;
	iop->secsiz = 0;
	iop->size = len;
	iop->end = iop->buf + len;
	iop->fd = -1;
	iop->flag = IOP_IS_INTERNAL;
	iop->getrec = get_a_record;
}

/* specfdopen --- open an fd special file */

static int
specfdopen(iop, name, mode)
IOBUF *iop;
const char *name, *mode;
{
	int fd;
	IOBUF *tp;

	fd = devopen(name, mode);
	if (fd == INVALID_HANDLE)
		return INVALID_HANDLE;
	tp = iop_alloc(fd, name, NULL);
	if (tp == NULL) {
		/* don't leak fd's */
		close(fd);
		return INVALID_HANDLE;
	}
	*iop = *tp;
	iop->flag |= IOP_NO_FREE;
	free(tp);
	return 0;
}

#ifdef GETPGRP_VOID
#define getpgrp_arg() /* nothing */
#else
#define getpgrp_arg() getpid()
#endif

/* pidopen --- "open" /dev/pid, /dev/ppid, and /dev/pgrpid */

static int
pidopen(iop, name, mode)
IOBUF *iop;
const char *name, *mode;
{
	char tbuf[BUFSIZ];
	int i;

	if (name[6] == 'g')
		sprintf(tbuf, "%d\n", getpgrp(getpgrp_arg()));
	else if (name[6] == 'i')
		sprintf(tbuf, "%d\n", getpid());
	else
		sprintf(tbuf, "%d\n", getppid());
	i = strlen(tbuf);
	spec_setup(iop, i, TRUE);
	strcpy(iop->buf, tbuf);
	return 0;
}

/* useropen --- "open" /dev/user */

/*
 * /dev/user creates a record as follows:
 *	$1 = getuid()
 *	$2 = geteuid()
 *	$3 = getgid()
 *	$4 = getegid()
 * If multiple groups are supported, then $5 through $NF are the
 * supplementary group set.
 */

static int
useropen(iop, name, mode)
IOBUF *iop;
const char *name, *mode;
{
	char tbuf[BUFSIZ], *cp;
	int i;
#if defined(NGROUPS_MAX) && NGROUPS_MAX > 0
	GETGROUPS_T groupset[NGROUPS_MAX];
	int ngroups;
#endif

	sprintf(tbuf, "%d %d %d %d", getuid(), geteuid(), getgid(), getegid());

	cp = tbuf + strlen(tbuf);
#if defined(NGROUPS_MAX) && NGROUPS_MAX > 0
	ngroups = getgroups(NGROUPS_MAX, groupset);
	if (ngroups == -1)
		fatal("could not find groups: %s", strerror(errno));

	for (i = 0; i < ngroups; i++) {
		*cp++ = ' ';
		sprintf(cp, "%d", (int) groupset[i]);
		cp += strlen(cp);
	}
#endif
	*cp++ = '\n';
	*cp++ = '\0';

	i = strlen(tbuf);
	spec_setup(iop, i, TRUE);
	strcpy(iop->buf, tbuf);
	return 0;
}

/* iop_open --- handle special and regular files for input */

static IOBUF *
iop_open(name, mode, iop)
const char *name, *mode;
IOBUF *iop;
{
	int openfd = INVALID_HANDLE;
	int flag = 0;
	struct stat buf;
	static struct internal {
		const char *name;
		int compare;
		int (*fp) P((IOBUF *, const char *, const char *));
		IOBUF iob;
	} table[] = {
		{ "/dev/fd/",		8,	specfdopen },
		{ "/dev/stdin",		10,	specfdopen },
		{ "/dev/stdout",	11,	specfdopen },
		{ "/dev/stderr",	11,	specfdopen },
		{ "/dev/pid",		8,	pidopen },
		{ "/dev/ppid",		9,	pidopen },
		{ "/dev/pgrpid",	11,	pidopen },
		{ "/dev/user",		9,	useropen },
	};
	int devcount = sizeof(table) / sizeof(table[0]);

	flag = str2mode(mode);

	/*
	 * FIXME: remove the stat call, and always process these files
	 * internally.
	 */
	if (STREQ(name, "-"))
		openfd = fileno(stdin);
	else if (do_traditional)
		goto strictopen;
	else if (STREQN(name, "/dev/", 5) && stat((char *) name, &buf) == -1) {
		int i;

		for (i = 0; i < devcount; i++) {
			if (STREQN(name, table[i].name, table[i].compare)) {
				iop = & table[i].iob;

				if (iop->buf != NULL) {
					spec_setup(iop, 0, FALSE);
					return iop;
				} else if ((*table[i].fp)(iop, name, mode) == 0)
					return iop;
				else {
					warning("could not open %s, mode `%s'",
						name, mode);
					return NULL;
				}
			}
		}
	}

strictopen:
	if (openfd == INVALID_HANDLE)
		openfd = open(name, flag, 0666);
	if (openfd != INVALID_HANDLE && fstat(openfd, &buf) > 0) 
		if ((buf.st_mode & S_IFMT) == S_IFDIR)
			fatal("file `%s' is a directory", name);
	return iop_alloc(openfd, name, iop);
}

#ifndef PIPES_SIMULATED		/* real pipes */

/* wait_any --- wait for a child process, close associated pipe */

static int
wait_any(interesting)
int interesting;	/* pid of interest, if any */
{
	RETSIGTYPE (*hstat)(), (*istat)(), (*qstat)();
	int pid;
	int status = 0;
	struct redirect *redp;
	extern int errno;

	hstat = signal(SIGHUP, SIG_IGN);
	istat = signal(SIGINT, SIG_IGN);
	qstat = signal(SIGQUIT, SIG_IGN);
	for (;;) {
#ifdef HAVE_SYS_WAIT_H	/* Posix compatible sys/wait.h */
		pid = wait(&status);
#else
		pid = wait((union wait *)&status);
#endif /* NeXT */
		if (interesting && pid == interesting) {
			break;
		} else if (pid != -1) {
			for (redp = red_head; redp != NULL; redp = redp->next)
				if (pid == redp->pid) {
					redp->pid = -1;
					redp->status = status;
					break;
				}
		}
		if (pid == -1 && errno == ECHILD)
			break;
	}
	signal(SIGHUP, hstat);
	signal(SIGINT, istat);
	signal(SIGQUIT, qstat);
	return(status);
}

/* gawk_popen --- open an IOBUF on a child process */

static IOBUF *
gawk_popen(cmd, rp)
char *cmd;
struct redirect *rp;
{
	int p[2];
	register int pid;

	/*
	 * used to wait for any children to synchronize input and output,
	 * but this could cause gawk to hang when it is started in a pipeline
	 * and thus has a child process feeding it input (shell dependant)
	 */
	/*(void) wait_any(0);*/	/* wait for outstanding processes */

	if (pipe(p) < 0)
		fatal("cannot open pipe \"%s\" (%s)", cmd, strerror(errno));
	if ((pid = fork()) == 0) {
		if (close(1) == -1)
			fatal("close of stdout in child failed (%s)",
				strerror(errno));
		if (dup(p[1]) != 1)
			fatal("dup of pipe failed (%s)", strerror(errno));
		if (close(p[0]) == -1 || close(p[1]) == -1)
			fatal("close of pipe failed (%s)", strerror(errno));
		execl("/bin/sh", "sh", "-c", cmd, NULL);
		_exit(127);
	}
	if (pid == -1)
		fatal("cannot fork for \"%s\" (%s)", cmd, strerror(errno));
	rp->pid = pid;
	if (close(p[1]) == -1)
		fatal("close of pipe failed (%s)", strerror(errno));
	rp->iop = iop_alloc(p[0], cmd, NULL);
	if (rp->iop == NULL)
		(void) close(p[0]);
	return (rp->iop);
}

/* gawk_pclose --- close an open child pipe */

static int
gawk_pclose(rp)
struct redirect *rp;
{
	(void) iop_close(rp->iop);
	rp->iop = NULL;

	/* process previously found, return stored status */
	if (rp->pid == -1)
		return (rp->status >> 8) & 0xFF;
	rp->status = wait_any(rp->pid);
	rp->pid = -1;
	return (rp->status >> 8) & 0xFF;
}

#else	/* PIPES_SIMULATED */

/*
 * use temporary file rather than pipe
 * except if popen() provides real pipes too
 */

#if defined(VMS) || defined(OS2) || defined (MSDOS)

/* gawk_popen --- open an IOBUF on a child process */

static IOBUF *
gawk_popen(cmd, rp)
char *cmd;
struct redirect *rp;
{
	FILE *current;

	if ((current = popen(cmd, "r")) == NULL)
		return NULL;
	rp->iop = iop_alloc(fileno(current), cmd, NULL);
	if (rp->iop == NULL) {
		(void) fclose(current);
		current = NULL;
	}
	rp->ifp = current;
	return (rp->iop);
}

/* gawk_pclose --- close an open child pipe */

static int
gawk_pclose(rp)
struct redirect *rp;
{
	int rval, aval, fd = rp->iop->fd;

	rp->iop->fd = dup(fd);	  /* kludge to allow close() + pclose() */
	rval = iop_close(rp->iop);
	rp->iop = NULL;
	aval = pclose(rp->ifp);
	rp->ifp = NULL;
	return (rval < 0 ? rval : aval);
}
#else	/* not (VMS || OS2 || MSDOS) */

static struct pipeinfo {
	char *command;
	char *name;
} pipes[_NFILE];

/* gawk_popen --- open an IOBUF on a child process */

static IOBUF *
gawk_popen(cmd, rp)
char *cmd;
struct redirect *rp;
{
	extern char *strdup P((const char *));
	int current;
	char *name;
	static char cmdbuf[256];

	/* get a name to use */
	if ((name = tempnam(".", "pip")) == NULL)
		return NULL;
	sprintf(cmdbuf, "%s > %s", cmd, name);
	system(cmdbuf);
	if ((current = open(name, O_RDONLY)) == INVALID_HANDLE)
		return NULL;
	pipes[current].name = name;
	pipes[current].command = strdup(cmd);
	rp->iop = iop_alloc(current, name, NULL);
	if (rp->iop == NULL)
		(void) close(current);
	return (rp->iop);
}

/* gawk_pclose --- close an open child pipe */

static int
gawk_pclose(rp)
struct redirect *rp;
{
	int cur = rp->iop->fd;
	int rval;

	rval = iop_close(rp->iop);
	rp->iop = NULL;

	/* check for an open file  */
	if (pipes[cur].name == NULL)
		return -1;
	unlink(pipes[cur].name);
	free(pipes[cur].name);
	pipes[cur].name = NULL;
	free(pipes[cur].command);
	return rval;
}
#endif	/* not (VMS || OS2 || MSDOS) */

#endif	/* PIPES_SIMULATED */

/* do_getline --- read in a line, into var and with redirection, as needed */

NODE *
do_getline(tree)
NODE *tree;
{
	struct redirect *rp = NULL;
	IOBUF *iop;
	int cnt = EOF;
	char *s = NULL;
	int errcode;

	while (cnt == EOF) {
		if (tree->rnode == NULL) {	 /* no redirection */
			iop = nextfile(FALSE);
			if (iop == NULL)		/* end of input */
				return tmp_number((AWKNUM) 0.0);
		} else {
			int redir_error = 0;

			rp = redirect(tree->rnode, &redir_error);
			if (rp == NULL && redir_error) { /* failed redirect */
				if (! do_traditional) {
					s = strerror(redir_error);

					unref(ERRNO_node->var_value);
					ERRNO_node->var_value =
						make_string(s, strlen(s));
				}
				return tmp_number((AWKNUM) -1.0);
			}
			iop = rp->iop;
			if (iop == NULL)		/* end of input */
				return tmp_number((AWKNUM) 0.0);
		}
		errcode = 0;
		cnt = (*(iop->getrec))(&s, iop, RS->stptr[0], RS_regexp, &errcode);
		if (errcode != 0) {
			if (! do_traditional) {
				s = strerror(errcode);

				unref(ERRNO_node->var_value);
				ERRNO_node->var_value = make_string(s, strlen(s));
			}
			return tmp_number((AWKNUM) -1.0);
		}
		if (cnt == EOF) {
			if (rp != NULL) {
				/*
				 * Don't do iop_close() here if we are
				 * reading from a pipe; otherwise
				 * gawk_pclose will not be called.
				 */
				if ((rp->flag & RED_PIPE) == 0) {
					(void) iop_close(iop);
					rp->iop = NULL;
				}
				rp->flag |= RED_EOF;	/* sticky EOF */
				return tmp_number((AWKNUM) 0.0);
			} else
				continue;	/* try another file */
		}
		if (rp == NULL) {
			NR++;
			FNR++;
		}
		if (tree->lnode == NULL)	/* no optional var. */
			set_record(s, cnt, TRUE);
		else {			/* assignment to variable */
			Func_ptr after_assign = NULL;
			NODE **lhs;

			lhs = get_lhs(tree->lnode, &after_assign);
			unref(*lhs);
			*lhs = make_string(s, cnt);
			(*lhs)->flags |= MAYBE_NUM;
			/* we may have to regenerate $0 here! */
			if (after_assign != NULL)
				(*after_assign)();
		}
	}
	return tmp_number((AWKNUM) 1.0);
}

/* pathopen --- pathopen with default file extension handling */

int
pathopen(file)
const char *file;
{
	int fd = do_pathopen(file);

#ifdef DEFAULT_FILETYPE
	if (! do_traditional && fd <= INVALID_HANDLE) {
		char *file_awk;
		int save = errno;
#ifdef VMS
		int vms_save = vaxc$errno;
#endif

		/* append ".awk" and try again */
		emalloc(file_awk, char *, strlen(file) +
			sizeof(DEFAULT_FILETYPE) + 1, "pathopen");
		sprintf(file_awk, "%s%s", file, DEFAULT_FILETYPE);
		fd = do_pathopen(file_awk);
		free(file_awk);
		if (fd <= INVALID_HANDLE) {
			errno = save;
#ifdef VMS
			vaxc$errno = vms_save;
#endif
		}
	}
#endif	/*DEFAULT_FILETYPE*/

	return fd;
}

/* do_pathopen --- search $AWKPATH for source file */

static int
do_pathopen(file)
const char *file;
{
	static const char *savepath = NULL;
	static int first = TRUE;
	const char *awkpath;
	char *cp, trypath[BUFSIZ];
	int fd;

	if (STREQ(file, "-"))
		return (0);

	if (do_traditional)
		return (devopen(file, "r"));

	if (first) {
		first = FALSE;
		if ((awkpath = getenv("AWKPATH")) != NULL && *awkpath)
			savepath = awkpath;	/* used for restarting */
		else
			savepath = defpath;
	}
	awkpath = savepath;

	/* some kind of path name, no search */
	if (ispath(file))
		return (devopen(file, "r"));

	do {
		trypath[0] = '\0';
		/* this should take into account limits on size of trypath */
		for (cp = trypath; *awkpath && *awkpath != envsep; )
			*cp++ = *awkpath++;

		if (cp != trypath) {	/* nun-null element in path */
			/* add directory punctuation only if needed */
			if (! isdirpunct(*(cp-1)))
				*cp++ = '/';
			/* append filename */
			strcpy(cp, file);
		} else
			strcpy(trypath, file);
		if ((fd = devopen(trypath, "r")) > INVALID_HANDLE)
			return (fd);

		/* no luck, keep going */
		if(*awkpath == envsep && awkpath[1] != '\0')
			awkpath++;	/* skip colon */
	} while (*awkpath != '\0');
	/*
	 * You might have one of the awk paths defined, WITHOUT the current
	 * working directory in it. Therefore try to open the file in the
	 * current directory.
	 */
	return (devopen(file, "r"));
}

#ifdef TEST
int bufsize = 8192;

void
fatal(s)
char *s;
{
	printf("%s\n", s);
	exit(1);
}
#endif

/* iop_alloc --- allocate an IOBUF structure for an open fd */

static IOBUF *
iop_alloc(fd, name, iop)
int fd;
const char *name;
IOBUF *iop;
{
	struct stat sbuf;

	if (fd == INVALID_HANDLE)
		return NULL;
	if (iop == NULL)
		emalloc(iop, IOBUF *, sizeof(IOBUF), "iop_alloc");
	iop->flag = 0;
	if (isatty(fd))
		iop->flag |= IOP_IS_TTY;
	iop->size = optimal_bufsize(fd, & sbuf);
	if (do_lint && S_ISREG(sbuf.st_mode) && sbuf.st_size == 0)
		warning("data file `%s' is empty", name);
	iop->secsiz = -2;
	errno = 0;
	iop->fd = fd;
	iop->off = iop->buf = NULL;
	iop->cnt = 0;
	iop->name = name;
	iop->getrec = get_a_record;
#ifdef HAVE_MMAP
	if (S_ISREG(sbuf.st_mode) && sbuf.st_size > 0) {
		register char *cp;

		iop->buf = iop->off = mmap((caddr_t) 0, sbuf.st_size,
					PROT_READ|PROT_WRITE, MAP_PRIVATE,
					fd,  0L);
		/* cast is for buggy compilers (e.g. DEC OSF/1) */
		if (iop->buf == (caddr_t)MAP_FAILED) {
			iop->buf = iop->off = NULL;
			goto out;
		}

		iop->flag |= IOP_MMAPPED;
		iop->size = sbuf.st_size;
		iop->secsiz = 0;
		iop->end = iop->buf + iop->size;
		iop->cnt = sbuf.st_size;
		iop->getrec = mmap_get_record;
		(void) close(fd);
		iop->fd = INVALID_HANDLE;

#if defined(HAVE_MADVISE) && defined(MADV_SEQUENTIAL)
		madvise(iop->buf, iop->size, MADV_SEQUENTIAL);
#endif
		/*
		 * The following is a really gross hack.
		 * We want to ensure that we have a copy of the input
		 * data that won't go away, on the off chance that someone
		 * will truncate the data file we've just mmap'ed.
		 * So, we go through and touch each page, forcing the
		 * system to give us a private copy. A page size of 512
		 * guarantees this will work, even on the least common
		 * denominator system (like, oh say, a VAX).
		 */
		for (cp = iop->buf; cp < iop->end; cp += 512)
			*cp = *cp;
	}
out:
#endif /* HAVE_MMAP */
	return iop;
}

/* These macros used by both record reading routines */
#define set_RT_to_null() \
	(void)(! do_traditional && (unref(RT_node->var_value), \
			   RT_node->var_value = Nnull_string))

#define set_RT(str, len) \
	(void)(! do_traditional && (unref(RT_node->var_value), \
			   RT_node->var_value = make_string(str, len)))

/*
 * get_a_record:
 * Get the next record.  Uses a "split buffer" where the latter part is
 * the normal read buffer and the head part is an "overflow" area that is used
 * when a record spans the end of the normal buffer, in which case the first
 * part of the record is copied into the overflow area just before the
 * normal buffer.  Thus, the eventual full record can be returned as a
 * contiguous area of memory with a minimum of copying.  The overflow area
 * is expanded as needed, so that records are unlimited in length.
 * We also mark both the end of the buffer and the end of the read() with
 * a sentinel character (the current record separator) so that the inside
 * loop can run as a single test.
 *
 * Note that since we know or can compute the end of the read and the end
 * of the buffer, the sentinel character does not get in the way of regexp
 * based searching, since we simply search up to that character, but not
 * including it.
 */

static int
get_a_record(out, iop, grRS, RSre, errcode)
char **out;		/* pointer to pointer to data */
IOBUF *iop;		/* input IOP */
register int grRS;	/* first char in RS->stptr */
Regexp *RSre;		/* regexp for RS */
int *errcode;		/* pointer to error variable */
{
	register char *bp = iop->off;
	char *bufend;
	char *start = iop->off;			/* beginning of record */
	int rs;
	static Regexp *RS_null_re = NULL;
	Regexp *rsre = NULL;
	int continuing = FALSE, continued = FALSE;	/* used for re matching */
	int onecase;

	/* first time through */
	if (RS_null_re == NULL) {
		RS_null_re = make_regexp("\n\n+", 3, TRUE, TRUE);
		if (RS_null_re == NULL)
			fatal("internal error: file `%s', line %d\n",
				__FILE__, __LINE__);
	}

	if (iop->cnt == EOF) {	/* previous read hit EOF */
		*out = NULL;
		set_RT_to_null();
		return EOF;
	}

	if (grRS == FALSE)	/* special case:  RS == "" */
		rs = '\n';
	else
		rs = (char) grRS;

	onecase = (IGNORECASE && isalpha((unsigned char)rs));
	if (onecase)
		rs = casetable[(unsigned char)rs];

	/* set up sentinel */
	if (iop->buf) {
		bufend = iop->buf + iop->size + iop->secsiz;
		*bufend = rs;		/* add sentinel to buffer */
	} else
		bufend = NULL;

	for (;;) {	/* break on end of record, read error or EOF */
/* buffer mgmt, chunk #1 */
		/*
		 * Following code is entered on the first call of this routine
		 * for a new iop, or when we scan to the end of the buffer.
		 * In the latter case, we copy the current partial record to
		 * the space preceding the normal read buffer.  If necessary,
		 * we expand this space.  This is done so that we can return
		 * the record as a contiguous area of memory.
		 */
		if ((iop->flag & IOP_IS_INTERNAL) == 0 && bp >= bufend) {
			char *oldbuf = NULL;
			char *oldsplit = iop->buf + iop->secsiz;
			long len;	/* record length so far */

			len = bp - start;
			if (len > iop->secsiz) {
				/* expand secondary buffer */
				if (iop->secsiz == -2)
					iop->secsiz = 256;
				while (len > iop->secsiz)
					iop->secsiz *= 2;
				oldbuf = iop->buf;
				emalloc(iop->buf, char *,
				    iop->size+iop->secsiz+2, "get_a_record");
				bufend = iop->buf + iop->size + iop->secsiz;
				*bufend = rs;
			}
			if (len > 0) {
				char *newsplit = iop->buf + iop->secsiz;

				if (start < oldsplit) {
					memcpy(newsplit - len, start,
							oldsplit - start);
					memcpy(newsplit - (bp - oldsplit),
							oldsplit, bp - oldsplit);
				} else
					memcpy(newsplit - len, start, len);
			}
			bp = iop->end = iop->off = iop->buf + iop->secsiz;
			start = bp - len;
			if (oldbuf != NULL) {
				free(oldbuf);
				oldbuf = NULL;
			}
		}
/* buffer mgmt, chunk #2 */
		/*
		 * Following code is entered whenever we have no more data to
		 * scan.  In most cases this will read into the beginning of
		 * the main buffer, but in some cases (terminal, pipe etc.)
		 * we may be doing smallish reads into more advanced positions.
		 */
		if (bp >= iop->end) {
			if ((iop->flag & IOP_IS_INTERNAL) != 0) {
				iop->cnt = EOF;
				break;
			}
			iop->cnt = read(iop->fd, iop->end, bufend - iop->end);
			if (iop->cnt == -1) {
				if (! do_traditional && errcode != NULL) {
					*errcode = errno;
					iop->cnt = EOF;
					break;
				} else
					fatal("error reading input file `%s': %s",
						iop->name, strerror(errno));
			} else if (iop->cnt == 0) {
				/*
				 * hit EOF before matching RS, so end
				 * the record and set RT to ""
				 */
				iop->cnt = EOF;
				/* see comments below about this test */
				if (! continuing) {
					set_RT_to_null();
					break;
				}
			}
			if (iop->cnt != EOF) {
				iop->end += iop->cnt;
				*iop->end = rs;		/* reset the sentinel */
			}
		}
/* buffers are now setup and filled with data */
/* search for RS, #1, regexp based, or RS = "" */
		/*
		 * Attempt to simplify the code a bit. The case where
		 * RS = "" can also be described by a regexp, RS = "\n\n+".
		 * The buffer managment and searching code can thus now
		 * use a common case (the one for regexps) both when RS is
		 * a regexp, and when RS = "". This particularly benefits
		 * us for keeping track of how many newlines were matched
		 * in order to set RT.
		 */
		if (! do_traditional && RSre != NULL)	/* regexp */
			rsre = RSre;
		else if (grRS == FALSE)		/* RS = "" */
			rsre = RS_null_re;
		else
			rsre = NULL;

		/*
		 * Look for regexp match of RS.  Non-match conditions are:
		 *	1. No match at all
		 *	2. Match of a null string
		 *	3. Match ends at exact end of buffer
		 * Number 3 is subtle; we have to add more to the buffer
		 * in case the match would have extended further into the
		 * file, since regexp match by definition always matches the
		 * longest possible match.
		 *
		 * It is even more subtle than you might think. Suppose
		 * the re matches at exactly the end of file. We don't know
		 * that until we try to add more to the buffer. Thus, we
		 * set a flag to indicate, that if eof really does happen,
		 * don't break early.
		 */
		continuing = FALSE;
		if (rsre != NULL) {
		again:
			/* cases 1 and 2 are simple, just keep going */
			if (research(rsre, start, 0, iop->end - start, TRUE) == -1
			    || RESTART(rsre, start) == REEND(rsre, start)) {
				bp = iop->end;
				continue;
			}
			/* case 3, regex match at exact end */
			if (start + REEND(rsre, start) >= iop->end) {
				if (iop->cnt != EOF) {
					bp = iop->end;
					continuing = continued = TRUE;
					continue;
				}
			}
			/* got a match! */
			/*
			 * Leading newlines at the beginning of the file
			 * should be ignored. Whew!
			 */
			if (grRS == FALSE && RESTART(rsre, start) == 0) {
				start += REEND(rsre, start);
				goto again;
			}
			bp = start + RESTART(rsre, start);
			set_RT(bp, REEND(rsre, start) - RESTART(rsre, start));
			*bp = '\0';
			iop->off = start + REEND(rsre, start);
			break;
		}
/* search for RS, #2, RS = <single char> */
		if (onecase) {
			while (casetable[(unsigned char) *bp++] != rs)
				continue;
		} else {
			while (*bp++ != rs)
				continue;
		}
		set_RT(bp - 1, 1);

		if (bp <= iop->end)
			break;
		else
			bp--;

		if ((iop->flag & IOP_IS_INTERNAL) != 0)
			iop->cnt = bp - start;
	}
	if (iop->cnt == EOF
	    && (((iop->flag & IOP_IS_INTERNAL) != 0)
	          || (start == bp && ! continued))) {
		*out = NULL;
		set_RT_to_null();
		return EOF;
	}

	if (do_traditional || rsre == NULL) {
		char *bstart;

		bstart = iop->off = bp;
		bp--;
		if (onecase ? casetable[(unsigned char) *bp] != rs : *bp != rs) {
			bp++;
			bstart = bp;
		}
		*bp = '\0';
	} else if (grRS == FALSE && iop->cnt == EOF) {
		/*
		 * special case, delete trailing newlines,
		 * should never be more than one.
		 */
		while (bp[-1] == '\n')
			bp--;
		*bp = '\0';
	}

	*out = start;
	return bp - start;
}

#ifdef TEST
int
main(argc, argv)
int argc;
char *argv[];
{
	IOBUF *iop;
	char *out;
	int cnt;
	char rs[2];

	rs[0] = '\0';
	if (argc > 1)
		bufsize = atoi(argv[1]);
	if (argc > 2)
		rs[0] = *argv[2];
	iop = iop_alloc(0, "stdin", NULL);
	while ((cnt = get_a_record(&out, iop, rs[0], NULL, NULL)) > 0) {
		fwrite(out, 1, cnt, stdout);
		fwrite(rs, 1, 1, stdout);
	}
	return 0;
}
#endif

#ifdef HAVE_MMAP
/* mmap_get_record --- pull a record out of a memory-mapped file */

static int
mmap_get_record(out, iop, grRS, RSre, errcode)
char **out;		/* pointer to pointer to data */
IOBUF *iop;		/* input IOP */
register int grRS;	/* first char in RS->stptr */
Regexp *RSre;		/* regexp for RS */
int *errcode;		/* pointer to error variable */
{
	register char *bp = iop->off;
	char *start = iop->off;			/* beginning of record */
	int rs;
	static Regexp *RS_null_re = NULL;
	Regexp *rsre = NULL;
	int onecase;
	register char *end = iop->end;
	int cnt;

	/* first time through */
	if (RS_null_re == NULL) {
		RS_null_re = make_regexp("\n\n+", 3, TRUE, TRUE);
		if (RS_null_re == NULL)
			fatal("internal error: file `%s', line %d\n",
				__FILE__, __LINE__);
	}

	if (iop->off >= iop->end) {	/* previous record was last */
		*out = NULL;
		set_RT_to_null();
		iop->cnt = EOF;		/* tested by higher level code */
		return EOF;
	}

	if (grRS == FALSE)	/* special case:  RS == "" */
		rs = '\n';
	else
		rs = (char) grRS;

	onecase = (IGNORECASE && isalpha((unsigned char)rs));
	if (onecase)
		rs = casetable[(unsigned char)rs];

	/* if RS = "", skip leading newlines at the front of the file */
	if (grRS == FALSE && iop->off == iop->buf) {
		for (bp = iop->off; *bp == '\n'; bp++)
			continue;

		if (bp != iop->off)
			iop->off = start = bp;
	}

	/*
	 * Regexp based searching. Either RS = "" or RS = <regex>
	 * See comments in get_a_record.
	 */
	if (! do_traditional && RSre != NULL)	/* regexp */
		rsre = RSre;
	else if (grRS == FALSE)		/* RS = "" */
		rsre = RS_null_re;
	else
		rsre = NULL;

	/*
	 * Look for regexp match of RS.  Non-match conditions are:
	 *	1. No match at all
	 *	2. Match of a null string
	 *	3. Match ends at exact end of buffer
	 *
	 * #1 means that the record ends the file
	 * and there is no text that actually matched RS.
	 *
	 * #2: is probably like #1.
	 *
	 * #3 is simple; since we have the whole file mapped, it's
	 * the last record in the file.
	 */
	if (rsre != NULL) {
		if (research(rsre, start, 0, iop->end - start, TRUE) == -1
		    || RESTART(rsre, start) == REEND(rsre, start)) {
			/* no matching text, we have the record */
			*out = start;
			iop->off = iop->end;	/* all done with the record */
			set_RT_to_null();
			/* special case, don't allow trailing newlines */
			if (grRS == FALSE && *(iop->end - 1) == '\n')
				return iop->end - start - 1;
			else
				return iop->end - start;

		}
		/* have a match */
		*out = start;
		bp = start + RESTART(rsre, start);
		set_RT(bp, REEND(rsre, start) - RESTART(rsre, start));
		*bp = '\0';
		iop->off = start + REEND(rsre, start);
		return bp - start;
	}

	/*
	 * RS = "?", i.e., one character based searching.
	 *
	 * Alas, we can't just plug the sentinel character in at
	 * the end of the mmapp'ed file ( *(iop->end) = rs; ). This
	 * works if we're lucky enough to have a file that does not
	 * take up all of its last disk block. But if we end up with
	 * file whose size is an even multiple of the disk block size,
	 * assigning past the end of it delivers a SIGBUS. So, we have to
	 * add the extra test in the while loop at the front that looks
	 * for going past the end of the mapped object. Sigh.
	 */
	/* search for RS, #2, RS = <single char> */
	if (onecase) {
		while (bp < end && casetable[(unsigned char)*bp++] != rs)
			continue;
	} else {
		while (bp < end && *bp++ != rs)
			continue;
	}
	cnt = (bp - start) - 1;
	if (bp >= iop->end) {
		/* at end, may have actually seen rs, or may not */
		if (*(bp-1) == rs)
			set_RT(bp - 1, 1);	/* real RS seen */
		else {
			cnt++;
			set_RT_to_null();
		}
	} else
		set_RT(bp - 1, 1);

	iop->off = bp;
	*out = start;
	return cnt;
}
#endif /* HAVE_MMAP */

/* set_RS --- update things as appropriate when RS is set */

void
set_RS()
{
	static NODE *save_rs = NULL;

	if (save_rs && cmp_nodes(RS_node->var_value, save_rs) == 0)
		return;
	unref(save_rs);
	save_rs = dupnode(RS_node->var_value);
	RS_is_null = FALSE;
	RS = force_string(RS_node->var_value);
	if (RS_regexp != NULL) {
		refree(RS_regexp);
		RS_regexp = NULL;
	}
	if (RS->stlen == 0)
		RS_is_null = TRUE;
	else if (RS->stlen > 1)
		RS_regexp = make_regexp(RS->stptr, RS->stlen, IGNORECASE, TRUE);

	set_FS_if_not_FIELDWIDTHS();
}
