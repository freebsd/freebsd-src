/*
 * Copyright (C) 1991, 1994 Wolfgang Solfrank.
 * Copyright (C) 1991, 1994 TooLs GmbH.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef lint
static char rcsid[] = "$Id: quot.c,v 1.5 1997/02/22 16:12:39 peter Exp $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/time.h>
#include <ufs/ffs/fs.h>
#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pwd.h>

/* some flags of what to do: */
static char estimate;
static char count;
static char unused;
static int (*func)();
static long blocksize;
static char *header;
static int headerlen;

/*
 * Original BSD quot doesn't round to number of frags/blocks,
 * doesn't account for indirection blocks and gets it totally
 * wrong if the	size is a multiple of the blocksize.
 * The new code always counts the number of 512 byte blocks
 * instead of the number of kilobytes and converts them	to
 * kByte when done (on request).
 */
#ifdef	COMPAT
#define	SIZE(n)	(n)
#else
#define	SIZE(n)	(((n) * 512 + blocksize - 1)/blocksize)
#endif

#define	INOCNT(fs)	((fs)->fs_ipg)
#define	INOSZ(fs)	(sizeof(struct dinode) * INOCNT(fs))

static struct dinode *get_inode(fd,super,ino)
	struct fs *super;
	ino_t ino;
{
	static struct dinode *ip;
	static ino_t last;
	
	if (fd < 0) {		/* flush cache */
		if (ip) {
			free(ip);
			ip = 0;
		}
		return 0;
	}
	
	if (!ip || ino < last || ino >= last + INOCNT(super)) {
		if (!ip
		    && !(ip = (struct dinode *)malloc(INOSZ(super)))) {
			perror("allocate inodes");
			exit(1);
		}
		last = (ino / INOCNT(super)) * INOCNT(super);
		if (lseek(fd, (off_t)ino_to_fsba(super, last) << super->fs_fshift, 0) < (off_t)0
		    || read(fd,ip,INOSZ(super)) != INOSZ(super)) {
			perror("read inodes");
			exit(1);
		}
	}
	
	return ip + ino % INOCNT(super);
}

#ifdef	COMPAT
#define	actualblocks(super,ip)	((ip)->di_blocks/2)
#else
#define	actualblocks(super,ip)	((ip)->di_blocks)
#endif

static virtualblocks(super,ip)
	struct fs *super;
	struct dinode *ip;
{
	register off_t nblk, sz;
	
	sz = ip->di_size;
#ifdef	COMPAT
	if (lblkno(super,sz) >= NDADDR) {
		nblk = blkroundup(super,sz);
		if (sz == nblk)
			nblk += super->fs_bsize;
	}
	
	return sz / 1024;
	
#else	/* COMPAT */
	
	if (lblkno(super,sz) >= NDADDR) {
		nblk = blkroundup(super,sz);
		sz = lblkno(super,nblk);
		sz = (sz - NDADDR + NINDIR(super) - 1) / NINDIR(super);
		while (sz > 0) {
			nblk += sz * super->fs_bsize;
			/* sz - 1 rounded up */
			sz = (sz - 1 + NINDIR(super) - 1) / NINDIR(super);
		}
	} else
		nblk = fragroundup(super,sz);
	
	return nblk / 512;
#endif	/* COMPAT */
}

static isfree(ip)
	struct dinode *ip;
{
#ifdef	COMPAT
	return (ip->di_mode&IFMT) == 0;
#else	/* COMPAT */
	
	switch (ip->di_mode&IFMT) {
	case IFIFO:
	case IFLNK:		/* should check FASTSYMLINK? */
	case IFDIR:
	case IFREG:
		return 0;
	default:
		return 1;
	}
#endif
}

static struct user {
	uid_t uid;
	char *name;
	daddr_t space;
	long count;
	daddr_t spc30;
	daddr_t spc60;
	daddr_t spc90;
} *users;
static int nusers;

static inituser()
{
	register i;
	register struct user *usr;
	
	if (!nusers) {
		nusers = 8;
		if (!(users =
		    (struct user *)calloc(nusers,sizeof(struct user)))) {
			perror("allocate users");
			exit(1);
		}
	} else {
		for (usr = users, i = nusers; --i >= 0; usr++) {
			usr->space = usr->spc30 = usr->spc60 = usr->spc90 = 0;
			usr->count = 0;
		}
	}
}

static usrrehash()
{
	register i;
	register struct user *usr, *usrn;
	struct user *svusr;
	
	svusr = users;
	nusers <<= 1;
	if (!(users = (struct user *)calloc(nusers,sizeof(struct user)))) {
		perror("allocate users");
		exit(1);
	}
	for (usr = svusr, i = nusers >> 1; --i >= 0; usr++) {
		for (usrn = users + (usr->uid&(nusers - 1)); usrn->name;
		    usrn--) {
			if (usrn <= users)
				usrn = users + nusers;
		}
		*usrn = *usr;
	}
}

static struct user *user(uid)
	uid_t uid;
{
	register struct user *usr;
	register i;
	struct passwd *pwd;
	
	while (1) {
		for (usr = users + (uid&(nusers - 1)), i = nusers; --i >= 0;
		    usr--) {
			if (!usr->name) {
				usr->uid = uid;
				
				if (!(pwd = getpwuid(uid))) {
					if (usr->name = (char *)malloc(7))
						sprintf(usr->name,"#%d",uid);
				} else {
					if (usr->name = (char *)
					    malloc(strlen(pwd->pw_name) + 1))
						strcpy(usr->name,pwd->pw_name);
				}
				if (!usr->name) {
					perror("allocate users");
					exit(1);
				}
				
				return usr;
				
			} else if (usr->uid == uid)
				return usr;

			if (usr <= users)
				usr = users + nusers;
		}
		usrrehash();
	}
}

static cmpusers(u1,u2)
	struct user *u1, *u2;
{
	return u2->space - u1->space;
}

#define	sortusers(users)	(qsort((users),nusers,sizeof(struct user), \
				    cmpusers))

static uses(uid,blks,act)
	uid_t uid;
	daddr_t blks;
	time_t act;
{
	static time_t today;
	register struct user *usr;
	
	if (!today)
		time(&today);
	
	usr = user(uid);
	usr->count++;
	usr->space += blks;
	
	if (today - act > 90L * 24L * 60L * 60L)
		usr->spc90 += blks;
	if (today - act > 60L * 24L * 60L * 60L)
		usr->spc60 += blks;
	if (today - act > 30L * 24L * 60L * 60L)
		usr->spc30 += blks;
}

#ifdef	COMPAT
#define	FSZCNT	500
#else
#define	FSZCNT	512
#endif
struct fsizes {
	struct fsizes *fsz_next;
	daddr_t fsz_first, fsz_last;
	ino_t fsz_count[FSZCNT];
	daddr_t fsz_sz[FSZCNT];
} *fsizes;

static initfsizes()
{
	register struct fsizes *fp;
	register i;
	
	for (fp = fsizes; fp; fp = fp->fsz_next) {
		for (i = FSZCNT; --i >= 0;) {
			fp->fsz_count[i] = 0;
			fp->fsz_sz[i] = 0;
		}
	}
}

static dofsizes(fd,super,name)
	struct fs *super;
	char *name;
{
	ino_t inode, maxino;
	struct dinode *ip;
	daddr_t sz, ksz;
	struct fsizes *fp, **fsp;
	register i;
	
	maxino = super->fs_ncg * super->fs_ipg - 1;
#ifdef	COMPAT
	if (!(fsizes = (struct fsizes *)malloc(sizeof(struct fsizes)))) {
		perror("alloc fsize structure");
		exit(1);
	}
#endif	/* COMPAT */
	for (inode = 0; inode < maxino; inode++) {
		errno = 0;
		if ((ip = get_inode(fd,super,inode))
#ifdef	COMPAT
		    && ((ip->di_mode&IFMT) == IFREG
			|| (ip->di_mode&IFMT) == IFDIR)
#else	/* COMPAT */
		    && !isfree(ip)
#endif	/* COMPAT */
		    ) {
			sz = estimate ? virtualblocks(super,ip) :
			    actualblocks(super,ip);
#ifdef	COMPAT
			if (sz >= FSZCNT) {
				fsizes->fsz_count[FSZCNT-1]++;
				fsizes->fsz_sz[FSZCNT-1] += sz;
			} else {
				fsizes->fsz_count[sz]++;
				fsizes->fsz_sz[sz] += sz;
			}
#else	/* COMPAT */
			ksz = SIZE(sz);
			for (fsp = &fsizes; fp = *fsp; fsp = &fp->fsz_next) {
				if (ksz < fp->fsz_last)
					break;
			}
			if (!fp || ksz < fp->fsz_first) {
				if (!(fp = (struct fsizes *)
				    malloc(sizeof(struct fsizes)))) {
					perror("alloc fsize structure");
					exit(1);
				}
				fp->fsz_next = *fsp;
				*fsp = fp;
				fp->fsz_first = (ksz / FSZCNT) * FSZCNT;
				fp->fsz_last = fp->fsz_first + FSZCNT;
				for (i = FSZCNT; --i >= 0;) {
					fp->fsz_count[i] = 0;
					fp->fsz_sz[i] = 0;
				}
			}
			fp->fsz_count[ksz % FSZCNT]++;
			fp->fsz_sz[ksz % FSZCNT] += sz;
#endif	/* COMPAT */
		} else if (errno) {
			perror(name);
			exit(1);
		}
	}
	sz = 0;
	for (fp = fsizes; fp; fp = fp->fsz_next) {
		for (i = 0; i < FSZCNT; i++) {
			if (fp->fsz_count[i])
				printf("%d\t%d\t%d\n",fp->fsz_first + i,
				    fp->fsz_count[i],
				    SIZE(sz += fp->fsz_sz[i]));
		}
	}
}

static douser(fd,super,name)
	struct fs *super;
	char *name;
{
	ino_t inode, maxino;
	struct user *usr, *usrs;
	struct dinode *ip;
	register n;
	
	maxino = super->fs_ncg * super->fs_ipg - 1;
	for (inode = 0; inode < maxino; inode++) {
		errno = 0;
		if ((ip = get_inode(fd,super,inode))
		    && !isfree(ip))
			uses(ip->di_uid,
			    estimate ? virtualblocks(super,ip) :
				actualblocks(super,ip),
			    ip->di_atime);
		else if (errno) {
			perror(name);
			exit(1);
		}
	}
	if (!(usrs = (struct user *)malloc(nusers * sizeof(struct user)))) {
		perror("allocate users");
		exit(1);
	}
	bcopy(users,usrs,nusers * sizeof(struct user));
	sortusers(usrs);
	for (usr = usrs, n = nusers; --n >= 0 && usr->count; usr++) {
		printf("%5d",SIZE(usr->space));
		if (count)
			printf("\t%5d",usr->count);
		printf("\t%-8s",usr->name);
		if (unused)
			printf("\t%5d\t%5d\t%5d",
			       SIZE(usr->spc30),
			       SIZE(usr->spc60),
			       SIZE(usr->spc90));
		printf("\n");
	}
	free(usrs);
}

static donames(fd,super,name)
	struct fs *super;
	char *name;
{
	int c;
	ino_t inode, inode1;
	ino_t maxino;
	struct dinode *ip;
	
	maxino = super->fs_ncg * super->fs_ipg - 1;
	/* first skip the name of the filesystem */
	while ((c = getchar()) != EOF && (c < '0' || c > '9'))
		while ((c = getchar()) != EOF && c != '\n');
	ungetc(c,stdin);
	inode1 = -1;
	while (scanf("%d",&inode) == 1) {
		if (inode < 0 || inode > maxino) {
			fprintf(stderr,"illegal inode %d\n",inode);
			return;
		}
		errno = 0;
		if ((ip = get_inode(fd,super,inode))
		    && !isfree(ip)) {
			printf("%s\t",user(ip->di_uid)->name);
			/* now skip whitespace */
			while ((c = getchar()) == ' ' || c == '\t');
			/* and print out the remainder of the input line */
			while (c != EOF && c != '\n') {
				putchar(c);
				c = getchar();
			}
			putchar('\n');
			inode1 = inode;
		} else {
			if (errno) {
				perror(name);
				exit(1);
			}
			/* skip this line */
			while ((c = getchar()) != EOF && c != '\n');
		}
		if (c == EOF)
			break;
	}
}

static usage()
{
#ifdef	COMPAT
	fprintf(stderr,"Usage: quot [-nfcvha] [filesystem ...]\n");
#else	/* COMPAT */
	fprintf(stderr,"Usage: quot [ -acfhknv ] [ filesystem ... ]\n");
#endif	/* COMPAT */
	exit(1);
}

static char superblock[SBSIZE];

quot(name,mp)
	char *name, *mp;
{
	int fd;
	
	get_inode(-1);		/* flush cache */
	inituser();
	initfsizes();
	if ((fd = open(name,0)) < 0
	    || lseek(fd,SBOFF,0) != SBOFF
	    || read(fd,superblock,SBSIZE) != SBSIZE) {
		perror(name);
		close(fd);
		return;
	}
	if (((struct fs *)superblock)->fs_magic != FS_MAGIC) {
		fprintf(stderr,"%s: not a BSD filesystem\n",name);
		close(fd);
		return;
	}
	printf("%s:",name);
	if (mp)
		printf(" (%s)",mp);
	putchar('\n');
	(*func)(fd,superblock,name);
	close(fd);
}

int main(argc,argv)
	char **argv;
{
	int fd;
	char all = 0;
	FILE *fp;
	struct statfs *mp;
	struct vfsconf vfc, *vfsp;
	char dev[MNAMELEN + 1];
	char *nm;
	int cnt;
	
	func = douser;
#ifndef	COMPAT
	header = getbsize(&headerlen,&blocksize);
#endif
	while (--argc > 0 && **++argv == '-') {
		while (*++*argv) {
			switch (**argv) {
			case 'n':
				func = donames;
				break;
			case 'c':
				func = dofsizes;
				break;
			case 'a':
				all = 1;
				break;
			case 'f':
				count = 1;
				break;
			case 'h':
				estimate = 1;
				break;
#ifndef	COMPAT
			case 'k':
				blocksize = 1024;
				break;
#endif	/* COMPAT */
			case 'v':
				unused = 1;
				break;
			default:
				usage();
			}
		}
	}
	if (all) {
		cnt = getmntinfo(&mp,MNT_NOWAIT);
		vfsp = getvfsbyname("ufs");
		if (vfsp == NULL) {
			fprintf(stderr, "cannot find ufs/ffs filesystem type!\n");
			exit(1);
		}
		for (; --cnt >= 0; mp++) {
			if (mp->f_type == vfsp->vfc_index) {
				if (nm = strrchr(mp->f_mntfromname,'/')) {
					sprintf(dev,"/dev/r%s",nm + 1);
					nm = dev;
				} else
					nm = mp->f_mntfromname;
				quot(nm,mp->f_mntonname);
			}
		}
	}
	while (--argc >= 0)
		quot(*argv++,0);
	return 0;
}
