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
  "$FreeBSD$";
#endif /* not lint */

#include <sys/param.h>
#include <ctype.h>
#include <err.h>
#include <grp.h>
#include <pwd.h>
#include <string.h>
#include <time.h>

#include <ufs/ufs/dinode.h>
#include <ufs/ffs/fs.h>

#include <sys/ioctl.h>

#include "fsdb.h"
#include "fsck.h"

static int charsperline __P((void));
static int printindir __P((ufs_daddr_t blk, int level, char *bufp));
static void printblocks __P((ino_t inum, struct dinode *dp));

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


/*
 * Determine the number of characters in a
 * single line.
 */

static int
charsperline()
{
	int columns;
	char *cp;
	struct winsize ws;

	columns = 0;
	if (ioctl(0, TIOCGWINSZ, &ws) != -1)
		columns = ws.ws_col;
	if (columns == 0 && (cp = getenv("COLUMNS")))
		columns = atoi(cp);
	if (columns == 0)
		columns = 80;	/* last resort */
	return (columns);
}


/*
 * Recursively print a list of indirect blocks.
 */
static int
printindir(blk, level, bufp)
	ufs_daddr_t blk;
	int level;
	char *bufp;
{
    struct bufarea buf, *bp;
    char tempbuf[32];		/* enough to print an ufs_daddr_t */
    int i, j, cpl, charssofar;
    ufs_daddr_t blkno;

    if (level == 0) {
	/* for the final indirect level, don't use the cache */
	bp = &buf;
	bp->b_un.b_buf = bufp;
	bp->b_prev = bp->b_next = bp;
	initbarea(bp);

	getblk(bp, blk, sblock.fs_bsize);
    } else
	bp = getdatablk(blk, sblock.fs_bsize);

    cpl = charsperline();
    for (i = charssofar = 0; i < NINDIR(&sblock); i++) {
	blkno = bp->b_un.b_indir[i];
	if (blkno == 0) {
	    if (level == 0)
		putchar('\n');
	    return 0;
	}
	j = sprintf(tempbuf, "%d", blkno);
	if (level == 0) {
	    charssofar += j;
	    if (charssofar >= cpl - 2) {
		putchar('\n');
		charssofar = j;
	    }
	}
	fputs(tempbuf, stdout);
	if (level == 0) {
	    printf(", ");
	    charssofar += 2;
	} else {
	    printf(" =>\n");
	    if (printindir(blkno, level - 1, bufp) == 0)
		return 0;
	}
    }
    if (level == 0)
	putchar('\n');
    return 1;
}


/*
 * Print the block pointers for one inode.
 */
static void
printblocks(inum, dp)
	ino_t inum;
	struct dinode *dp;
{
    char *bufp;
    int i, j, nfrags;
    long ndb, offset;

    printf("Blocks for inode %d:\n", inum);
    printf("Direct blocks:\n");
    ndb = howmany(dp->di_size, sblock.fs_bsize);
    for (i = 0; i < NDADDR; i++) {
	if (dp->di_db[i] == 0) {
	    putchar('\n');
	    return;
	}
	if (i > 0)
	    printf(", ");
	printf("%d", dp->di_db[i]);
	if (--ndb == 0 && (offset = blkoff(&sblock, dp->di_size)) != 0) {
	    nfrags = numfrags(&sblock, fragroundup(&sblock, offset));
	    printf(" (%d frag%s)", nfrags, nfrags > 1? "s": "");
	}
    }
    putchar('\n');
    if (dp->di_ib[0] == 0)
	return;

    bufp = malloc((unsigned int)sblock.fs_bsize);
    if (bufp == 0)
	errx(EEXIT, "cannot allocate indirect block buffer");
    printf("Indirect blocks:\n");
    for (i = 0; i < NIADDR; i++)
	if (printindir(dp->di_ib[i], i, bufp) == 0)
	    break;
    free(bufp);
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
printactive(doblocks)
	int doblocks;
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
	if (doblocks)
	    printblocks(curinum, curinode);
	else
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
