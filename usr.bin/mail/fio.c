/*
 * Copyright (c) 1980 Regents of the University of California.
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
static char sccsid[] = "@(#)fio.c	5.24 (Berkeley) 2/3/91";
#endif /* not lint */

#include "rcv.h"
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <paths.h>
#include <errno.h>

/*
 * Mail -- a mail program
 *
 * File I/O.
 */

/*
 * Set up the input pointers while copying the mail file into /tmp.
 */
setptr(ibuf)
	register FILE *ibuf;
{
	register int c, count;
	register char *cp, *cp2;
	struct message this;
	FILE *mestmp;
	off_t offset;
	int maybe, inhead;
	char linebuf[LINESIZE];

	/* Get temporary file. */
	(void)sprintf(linebuf, "%s/mail.XXXXXX", _PATH_TMP);
	if ((c = mkstemp(linebuf)) == -1 ||
	    (mestmp = Fdopen(c, "r+")) == NULL) {
		(void)fprintf(stderr, "mail: can't open %s\n", linebuf);
		exit(1);
	}
	(void)unlink(linebuf);

	msgCount = 0;
	maybe = 1;
	inhead = 0;
	offset = 0;
	this.m_flag = MUSED|MNEW;
	this.m_size = 0;
	this.m_lines = 0;
	this.m_block = 0;
	this.m_offset = 0;
	for (;;) {
		if (fgets(linebuf, LINESIZE, ibuf) == NULL) {
			if (append(&this, mestmp)) {
				perror("temporary file");
				exit(1);
			}
			makemessage(mestmp);
			return;
		}
		count = strlen(linebuf);
		(void) fwrite(linebuf, sizeof *linebuf, count, otf);
		if (ferror(otf)) {
			perror("/tmp");
			exit(1);
		}
		linebuf[count - 1] = 0;
		if (maybe && linebuf[0] == 'F' && ishead(linebuf)) {
			msgCount++;
			if (append(&this, mestmp)) {
				perror("temporary file");
				exit(1);
			}
			this.m_flag = MUSED|MNEW;
			this.m_size = 0;
			this.m_lines = 0;
			this.m_block = blockof(offset);
			this.m_offset = offsetof(offset);
			inhead = 1;
		} else if (linebuf[0] == 0) {
			inhead = 0;
		} else if (inhead) {
			for (cp = linebuf, cp2 = "status";; cp++) {
				if ((c = *cp2++) == 0) {
					while (isspace(*cp++))
						;
					if (cp[-1] != ':')
						break;
					while (c = *cp++)
						if (c == 'R')
							this.m_flag |= MREAD;
						else if (c == 'O')
							this.m_flag &= ~MNEW;
					inhead = 0;
					break;
				}
				if (*cp != c && *cp != toupper(c))
					break;
			}
		}
		offset += count;
		this.m_size += count;
		this.m_lines++;
		maybe = linebuf[0] == 0;
	}
}

/*
 * Drop the passed line onto the passed output buffer.
 * If a write error occurs, return -1, else the count of
 * characters written, including the newline.
 */
putline(obuf, linebuf)
	FILE *obuf;
	char *linebuf;
{
	register int c;

	c = strlen(linebuf);
	(void) fwrite(linebuf, sizeof *linebuf, c, obuf);
	(void) putc('\n', obuf);
	if (ferror(obuf))
		return (-1);
	return (c + 1);
}

/*
 * Read up a line from the specified input into the line
 * buffer.  Return the number of characters read.  Do not
 * include the newline at the end.
 */
readline(ibuf, linebuf, linesize)
	FILE *ibuf;
	char *linebuf;
{
	register int n;

	clearerr(ibuf);
	if (fgets(linebuf, linesize, ibuf) == NULL)
		return -1;
	n = strlen(linebuf);
	if (n > 0 && linebuf[n - 1] == '\n')
		linebuf[--n] = '\0';
	return n;
}

/*
 * Return a file buffer all ready to read up the
 * passed message pointer.
 */
FILE *
setinput(mp)
	register struct message *mp;
{

	fflush(otf);
	if (fseek(itf, positionof(mp->m_block, mp->m_offset), 0) < 0) {
		perror("fseek");
		panic("temporary file seek");
	}
	return (itf);
}

/*
 * Take the data out of the passed ghost file and toss it into
 * a dynamically allocated message structure.
 */
makemessage(f)
	FILE *f;
{
	register size = (msgCount + 1) * sizeof (struct message);
	off_t lseek();

	if (message != 0)
		free((char *) message);
	if ((message = (struct message *) malloc((unsigned) size)) == 0)
		panic("Insufficient memory for %d messages", msgCount);
	dot = message;
	size -= sizeof (struct message);
	fflush(f);
	(void) lseek(fileno(f), (long) sizeof *message, 0);
	if (read(fileno(f), (char *) message, size) != size)
		panic("Message temporary file corrupted");
	message[msgCount].m_size = 0;
	message[msgCount].m_lines = 0;
	Fclose(f);
}

/*
 * Append the passed message descriptor onto the temp file.
 * If the write fails, return 1, else 0
 */
append(mp, f)
	struct message *mp;
	FILE *f;
{
	return fwrite((char *) mp, sizeof *mp, 1, f) != 1;
}

/*
 * Delete a file, but only if the file is a plain file.
 */
rm(name)
	char *name;
{
	struct stat sb;

	if (stat(name, &sb) < 0)
		return(-1);
	if (!S_ISREG(sb.st_mode)) {
		errno = EISDIR;
		return(-1);
	}
	return(unlink(name));
}

static int sigdepth;		/* depth of holdsigs() */
static int omask;
/*
 * Hold signals SIGHUP, SIGINT, and SIGQUIT.
 */
holdsigs()
{

	if (sigdepth++ == 0)
		omask = sigblock(sigmask(SIGHUP)|sigmask(SIGINT)|sigmask(SIGQUIT));
}

/*
 * Release signals SIGHUP, SIGINT, and SIGQUIT.
 */
relsesigs()
{

	if (--sigdepth == 0)
		sigsetmask(omask);
}

/*
 * Determine the size of the file possessed by
 * the passed buffer.
 */
off_t
fsize(iob)
	FILE *iob;
{
	struct stat sbuf;

	if (fstat(fileno(iob), &sbuf) < 0)
		return 0;
	return sbuf.st_size;
}

/*
 * Evaluate the string given as a new mailbox name.
 * Supported meta characters:
 *	%	for my system mail box
 *	%user	for user's system mail box
 *	#	for previous file
 *	&	invoker's mbox file
 *	+file	file in folder directory
 *	any shell meta character
 * Return the file name as a dynamic string.
 */
char *
expand(name)
	register char *name;
{
	char xname[PATHSIZE];
	char cmdbuf[PATHSIZE];		/* also used for file names */
	register int pid, l;
	register char *cp, *shell;
	int pivec[2];
	struct stat sbuf;
	extern union wait wait_status;

	/*
	 * The order of evaluation is "%" and "#" expand into constants.
	 * "&" can expand into "+".  "+" can expand into shell meta characters.
	 * Shell meta characters expand into constants.
	 * This way, we make no recursive expansion.
	 */
	switch (*name) {
	case '%':
		findmail(name[1] ? name + 1 : myname, xname);
		return savestr(xname);
	case '#':
		if (name[1] != 0)
			break;
		if (prevfile[0] == 0) {
			printf("No previous file\n");
			return NOSTR;
		}
		return savestr(prevfile);
	case '&':
		if (name[1] == 0 && (name = value("MBOX")) == NOSTR)
			name = "~/mbox";
		/* fall through */
	}
	if (name[0] == '+' && getfold(cmdbuf) >= 0) {
		sprintf(xname, "%s/%s", cmdbuf, name + 1);
		name = savestr(xname);
	}
	/* catch the most common shell meta character */
	if (name[0] == '~' && (name[1] == '/' || name[1] == '\0')) {
		sprintf(xname, "%s%s", homedir, name + 1);
		name = savestr(xname);
	}
	if (!anyof(name, "~{[*?$`'\"\\"))
		return name;
	if (pipe(pivec) < 0) {
		perror("pipe");
		return name;
	}
	sprintf(cmdbuf, "echo %s", name);
	if ((shell = value("SHELL")) == NOSTR)
		shell = _PATH_CSHELL;
	pid = start_command(shell, 0, -1, pivec[1], "-c", cmdbuf, NOSTR);
	if (pid < 0) {
		close(pivec[0]);
		close(pivec[1]);
		return NOSTR;
	}
	close(pivec[1]);
	l = read(pivec[0], xname, BUFSIZ);
	close(pivec[0]);
	if (wait_child(pid) < 0 && wait_status.w_termsig != SIGPIPE) {
		fprintf(stderr, "\"%s\": Expansion failed.\n", name);
		return NOSTR;
	}
	if (l < 0) {
		perror("read");
		return NOSTR;
	}
	if (l == 0) {
		fprintf(stderr, "\"%s\": No match.\n", name);
		return NOSTR;
	}
	if (l == BUFSIZ) {
		fprintf(stderr, "\"%s\": Expansion buffer overflow.\n", name);
		return NOSTR;
	}
	xname[l] = 0;
	for (cp = &xname[l-1]; *cp == '\n' && cp > xname; cp--)
		;
	cp[1] = '\0';
	if (index(xname, ' ') && stat(xname, &sbuf) < 0) {
		fprintf(stderr, "\"%s\": Ambiguous.\n", name);
		return NOSTR;
	}
	return savestr(xname);
}

/*
 * Determine the current folder directory name.
 */
getfold(name)
	char *name;
{
	char *folder;

	if ((folder = value("folder")) == NOSTR)
		return (-1);
	if (*folder == '/')
		strcpy(name, folder);
	else
		sprintf(name, "%s/%s", homedir, folder);
	return (0);
}

/*
 * Return the name of the dead.letter file.
 */
char *
getdeadletter()
{
	register char *cp;

	if ((cp = value("DEAD")) == NOSTR || (cp = expand(cp)) == NOSTR)
		cp = expand("~/dead.letter");
	else if (*cp != '/') {
		char buf[PATHSIZE];

		(void) sprintf(buf, "~/%s", cp);
		cp = expand(buf);
	}
	return cp;
}
