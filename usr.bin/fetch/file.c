/*-
 * Copyright 1997 Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that both the above copyright notice and this
 * permission notice appear in all copies, that both the above
 * copyright notice and this permission notice appear in all
 * supporting documentation, and that the name of M.I.T. not be used
 * in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  M.I.T. makes
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 * 
 * THIS SOFTWARE IS PROVIDED BY M.I.T. ``AS IS''.  M.I.T. DISCLAIMS
 * ALL EXPRESS OR IMPLIED WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT
 * SHALL M.I.T. BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id: file.c,v 1.1 1997/01/30 21:43:39 wollman Exp $
 */

#include <sys/types.h>

#include <err.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "fetch.h"

static int file_retrieve(struct fetch_state *fs);
static int file_close(struct fetch_state *fs);
static int file_parse(struct fetch_state *fs, const char *uri);

struct uri_scheme file_scheme =
	{ "file", file_parse, 0, 0, 0 };

/*
 * Again, we slightly misinterpret the slash after the hostname as
 * being the start of the pathname rather than merely a separator.
 */
static int
file_parse(struct fetch_state *fs, const char *uri)
{
	const char *p;

	p = uri + 5;		/* skip past `file:' */
	if (p[0] == '/' && p[1] == '/') {
		/* skip past `//localhost', if any */
		p += 2;
		while (*p && *p != '/')
			p++;
	}

	if (p[0] != '/') {
		warnx("`%s': expected absolute pathname in `file' URL", uri);
		return EX_USAGE;
	}

	fs->fs_proto = percent_decode(p);
	/* guaranteed to succeed because of above test */
	p = strrchr(fs->fs_proto, '/');
	if (fs->fs_outputfile == 0) /* only set if not overridden by user */
		fs->fs_outputfile = p + 1;
	fs->fs_retrieve = file_retrieve;
	fs->fs_close = file_close;
	return 0;
}

static int
file_close(struct fetch_state *fs)
{
	free(fs->fs_proto);
	fs->fs_proto = 0;
	fs->fs_outputfile = 0;
	fs->fs_status = "free";
	return 0;
}

static int
file_retrieve(struct fetch_state *fs)
{
	/* XXX - this seems bogus to me! */
	if (access(fs->fs_outputfile, F_OK) == 0) {
		errno = EEXIST;
		warn("%s", fs->fs_outputfile);
		return EX_USAGE;
	}

	if (fs->fs_linkfile) {
    		struct stat sb;
		fs->fs_status = "symlink";
		if (stat(fs->fs_proto, &sb) == -1
			|| symlink(fs->fs_proto, fs->fs_outputfile) == -1) {
			warn("symlink");
			return EX_OSERR;
		}
		fs->fs_status = "done";
	} else {
		pid_t pid;
		int status;

		fflush(stderr);
		pid = fork();
		if (pid < 0) {
			warn("fork");
			return EX_TEMPFAIL;
		} else if (pid == 0) {
			execl(PATH_CP, "cp", "-p", fs->fs_proto, 
			      fs->fs_outputfile, (char *)0);
			warn("execl: " PATH_CP);
			fflush(stderr);
			_exit(EX_OSERR);
		} else {
			fs->fs_status = "copying";
			if (waitpid(pid, &status, 0) < 0) {
				warn("waitpid(%ld)", (long)pid);
				return EX_OSERR;
			}
			if (WIFEXITED(status))
				return WEXITSTATUS(status);
			if (WIFSIGNALED(status))
				warn(PATH_CP " exited on signal: %s",
				     sys_signame[WTERMSIG(status)]);
			return EX_OSERR;
		}
	}
	return 0;
}

