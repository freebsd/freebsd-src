/*	$NetBSD: fsdbutil.c,v 1.2 1995/10/08 23:18:12 thorpej Exp $	*/

/*
 *  Copyright (c) 1995 John T. Kohl
 *  All rights reserved.
 * 
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR `AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD: src/sbin/fsdb/fsdbutil.c,v 1.9.2.1 2000/07/01 06:23:35 ps Exp $";
#endif /* not lint */

#include <sys/types.h>
#include <ctype.h>
#include <err.h>
#include <grp.h>
#include <pwd.h>
#include <string.h>
#include <time.h>

#include <ufs/ufs/dinode.h>

#include "fsdb.h"
#include "fsck.h"

char **
crack(line, argc)
	char *line;
	int *argc;
{
    static char *argv[8];
    int i;
    char *p, *val;
    for (p = line, i = 0; p != NULL && i < 8; i++) {
	while ((val = strsep(&p, " \t\n")) != NULL && *val == '\0')
	    /**/;
	if (val)
	    argv[i] = val;
	else
	    break;
    }
    *argc = i;
    return argv;
}

int
argcount(cmdp, argc, argv)
	struct cmdtable *cmdp;
	int argc;
	char *argv[];
{
    if (cmdp->minargc == cmdp->maxargc)
	warnx("command `%s' takes %u arguments", cmdp->cmd, cmdp->minargc-1);
    else
	warnx("command `%s' takes from %u to %u arguments",
	      cmdp->cmd, cmdp->minargc-1, cmdp->maxargc-1);
	    
    warnx("usage: %s: %s", cmdp->cmd, cmdp->helptxt);
    return 1;
}

void
printstat(cp, inum, dp)
	const char *cp;
	ino_t inum;
	struct dinode *dp;
{
    struct group *grp;
    struct passwd *pw;
    char *p;
    time_t t;

    printf("%s: ", cp);
    switch (dp->di_mode & IFMT) {
    case IFDIR:
	puts("directory");
	break;
    case IFREG:
	puts("regular file");
	break;
    case IFBLK:
	printf("block special (%d,%d)",
	       major(dp->di_rdev), minor(dp->di_rdev));
	break;
    case IFCHR:
	printf("character special (%d,%d)",
	       major(dp->di_rdev), minor(dp->di_rdev));
	break;
    case IFLNK:
	fputs("symlink",stdout);
	if (dp->di_size > 0 && dp->di_size < MAXSYMLINKLEN &&
	    dp->di_blocks == 0)
	    printf(" to `%.*s'\n", (int) dp->di_size, (char *)dp->di_shortlink);
	else
		putchar('\n');
	break;
    case IFSOCK:
	puts("socket");
	break;
    case IFIFO:
	puts("fifo");
	break;
    }
    printf("I=%lu MODE=%o SIZE=%qu", (u_long)inum, dp->di_mode, dp->di_size);
    t = dp->di_mtime;
    p = ctime(&t);
    printf("\n\tMTIME=%15.15s %4.4s [%d nsec]", &p[4], &p[20],
	   dp->di_mtimensec);
    t = dp->di_ctime;
    p = ctime(&t);
    printf("\n\tCTIME=%15.15s %4.4s [%d nsec]", &p[4], &p[20],
	   dp->di_ctimensec);
    t = dp->di_atime;
    p = ctime(&t);
    printf("\n\tATIME=%15.15s %4.4s [%d nsec]\n", &p[4], &p[20],
	   dp->di_atimensec);

    if ((pw = getpwuid(dp->di_uid)))
	printf("OWNER=%s ", pw->pw_name);
    else
	printf("OWNUID=%u ", dp->di_uid);
    if ((grp = getgrgid(dp->di_gid)))
	printf("GRP=%s ", grp->gr_name);
    else
	printf("GID=%u ", dp->di_gid);

    printf("LINKCNT=%hd FLAGS=%#x BLKCNT=%x GEN=%x\n", dp->di_nlink, dp->di_flags,
	   dp->di_blocks, dp->di_gen);
}

int
checkactive()
{
    if (!curinode) {
	warnx("no current inode\n");
	return 0;
    }
    return 1;
}

int
checkactivedir()
{
    if (!curinode) {
	warnx("no current inode\n");
	return 0;
    }
    if ((curinode->di_mode & IFMT) != IFDIR) {
	warnx("inode %d not a directory", curinum);
	return 0;
    }
    return 1;
}

int
printactive()
{
    if (!checkactive())
	return 1;
    switch (curinode->di_mode & IFMT) {
    case IFDIR:
    case IFREG:
    case IFBLK:
    case IFCHR:
    case IFLNK:
    case IFSOCK:
    case IFIFO:
	printstat("current inode", curinum, curinode);
	break;
    case 0:
	printf("current inode %d: unallocated inode\n", curinum);
	break;
    default:
	printf("current inode %d: screwy itype 0%o (mode 0%o)?\n",
	       curinum, curinode->di_mode & IFMT, curinode->di_mode);
	break;
    }
    return 0;
}
