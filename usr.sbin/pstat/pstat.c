/*-
 * Copyright (c) 1980, 1991, 1993, 1994
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
static const char copyright[] =
"@(#) Copyright (c) 1980, 1991, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)pstat.c	8.16 (Berkeley) 5/9/95";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/vnode.h>
#include <sys/ucred.h>
#define _KERNEL
#include <sys/file.h>
#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <sys/mount.h>
#include <sys/uio.h>
#include <sys/namei.h>
#include <fs/unionfs/union.h>
#undef _KERNEL
#include <sys/stat.h>
#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfs/nfs.h>
#include <nfs/nfsnode.h>
#include <sys/ioctl.h>
#include <sys/ioctl_compat.h>	/* XXX NTTYDISC is too well hidden */
#include <sys/tty.h>
#include <sys/conf.h>
#include <sys/blist.h>

#include <sys/user.h>
#include <sys/sysctl.h>

#include <err.h>
#include <fcntl.h>
#include <kvm.h>
#include <limits.h>
#include <nlist.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct nlist nl[] = {
#define NLMANDATORYBEG	0
#define	V_MOUNTLIST	0
	{ "_mountlist" },	/* address of head of mount list. */
#define V_NUMV		1
	{ "_numvnodes" },
#define	FNL_NFILE	2
	{"_nfiles"},
#define FNL_MAXFILE	3
	{"_maxfiles"},
#define NLMANDATORYEND FNL_MAXFILE	/* names up to here are mandatory */
#define	SCONS		NLMANDATORYEND + 1
	{ "_cons" },
#define	SPTY		NLMANDATORYEND + 2
	{ "_pt_tty" },
#define	SNPTY		NLMANDATORYEND + 3
	{ "_npty" },



#ifdef __FreeBSD__
#define SCCONS	(SNPTY+1)
	{ "_sccons" },
#define NSCCONS	(SNPTY+2)
	{ "_nsccons" },
#define SIO  (SNPTY+3)
	{ "_sio_tty" },
#define NSIO (SNPTY+4)
	{ "_nsio_tty" },
#define RC  (SNPTY+5)
	{ "_rc_tty" },
#define NRC (SNPTY+6)
	{ "_nrc_tty" },
#define CY  (SNPTY+7)
	{ "_cy_tty" },
#define NCY (SNPTY+8)
	{ "_ncy_tty" },
#define SI  (SNPTY+9)
	{ "_si_tty" },
#define NSI (SNPTY+10)
	{ "_si_Nports" },
#endif
	{ "" }
};

int	usenumflag;
int	totalflag;
int	swapflag;
char	*nlistf	= NULL;
char	*memf	= NULL;
kvm_t	*kd;

char	*usagestr;

struct {
	int m_flag;
	const char *m_name;
} mnt_flags[] = {
	{ MNT_RDONLY, "rdonly" },
	{ MNT_SYNCHRONOUS, "sync" },
	{ MNT_NOEXEC, "noexec" },
	{ MNT_NOSUID, "nosuid" },
	{ MNT_NODEV, "nodev" },
	{ MNT_UNION, "union" },
	{ MNT_ASYNC, "async" },
	{ MNT_SUIDDIR, "suiddir" },
	{ MNT_SOFTDEP, "softdep" },
	{ MNT_NOSYMFOLLOW, "nosymfollow" },
	{ MNT_NOATIME, "noatime" },
	{ MNT_NOCLUSTERR, "noclusterread" },
	{ MNT_NOCLUSTERW, "noclusterwrite" },
	{ MNT_EXRDONLY, "exrdonly" },
	{ MNT_EXPORTED, "exported" },
	{ MNT_DEFEXPORTED, "defexported" },
	{ MNT_EXPORTANON, "exportanon" },
	{ MNT_EXKERB, "exkerb" },
	{ MNT_EXPUBLIC, "public" },
	{ MNT_LOCAL, "local" },
	{ MNT_QUOTA, "quota" },
	{ MNT_ROOTFS, "rootfs" },
	{ MNT_USER, "user" },
	{ MNT_IGNORE, "ignore" },
	{ MNT_UPDATE, "update" },
	{ MNT_DELEXPORT, "delexport" },
	{ MNT_RELOAD, "reload" },
	{ MNT_FORCE, "force" },
	{ MNT_SNAPSHOT, "snapshot" },
	{ 0 }
};


#define	SVAR(var) __STRING(var)	/* to force expansion */
#define	KGET(idx, var)							\
	KGET1(idx, &var, sizeof(var), SVAR(var))
#define	KGET1(idx, p, s, msg)						\
	KGET2(nl[idx].n_value, p, s, msg)
#define	KGET2(addr, p, s, msg)						\
	if (kvm_read(kd, (u_long)(addr), p, s) != s)			\
		warnx("cannot read %s: %s", msg, kvm_geterr(kd))
#define	KGETN(idx, var)							\
	KGET1N(idx, &var, sizeof(var), SVAR(var))
#define	KGET1N(idx, p, s, msg)						\
	KGET2N(nl[idx].n_value, p, s, msg)
#define	KGET2N(addr, p, s, msg)						\
	((kvm_read(kd, (u_long)(addr), p, s) == s) ? 1 : 0)
#define	KGETRET(addr, p, s, msg)					\
	if (kvm_read(kd, (u_long)(addr), p, s) != s) {			\
		warnx("cannot read %s: %s", msg, kvm_geterr(kd));	\
		return (0);						\
	}

void	filemode __P((void));
int	getfiles __P((char **, int *));
struct mount *
	getmnt __P((struct mount *));
struct e_vnode *
	kinfo_vnodes __P((int *));
struct e_vnode *
	loadvnodes __P((int *));
void	mount_print __P((struct mount *));
void	nfs_header __P((void));
int	nfs_print __P((struct vnode *));
void	swapmode __P((void));
void	ttymode __P((void));
void	ttyprt __P((struct tty *, int));
void	ttytype __P((struct tty *, char *, int, int, int));
void	ufs_header __P((void));
int	ufs_print __P((struct vnode *));
void	union_header __P((void));
int	union_print __P((struct vnode *));
static void usage __P((void));
void	vnode_header __P((void));
void	vnode_print __P((struct vnode *, struct vnode *));
void	vnodemode __P((void));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int ch, i, quit, ret;
	int fileflag, ttyflag, vnodeflag;
	char buf[_POSIX2_LINE_MAX],*opts;

	fileflag = swapflag = ttyflag = vnodeflag = 0;

	/* We will behave like good old swapinfo if thus invoked */
	opts = strrchr(argv[0],'/');
	if (opts)
		opts++;
	else
		opts = argv[0];
	if (!strcmp(opts,"swapinfo")) {
		swapflag = 1;
		opts = "kM:N:";
		usagestr = "swapinfo [-k] [-M core] [-N system]";
	} else {
		opts = "TM:N:fiknstv";
		usagestr = "pstat [-Tfknstv] [-M core] [-N system]";
	}

	while ((ch = getopt(argc, argv, opts)) != -1)
		switch (ch) {
		case 'f':
			fileflag = 1;
			break;
		case 'k':
			putenv("BLOCKSIZE=1K");
			break;
		case 'M':
			memf = optarg;
			break;
		case 'N':
			nlistf = optarg;
			break;
		case 'n':
			usenumflag = 1;
			break;
		case 's':
			++swapflag;
			break;
		case 'T':
			totalflag = 1;
			break;
		case 't':
			ttyflag = 1;
			break;
		case 'v':
		case 'i':		/* Backward compatibility. */
			errx(1, "vnode mode not supported");
#if 0
			vnodeflag = 1;
			break;
#endif
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	/*
	 * Discard setgid privileges if not the running kernel so that bad
	 * guys can't print interesting stuff from kernel memory.
	 */
	if (nlistf != NULL || memf != NULL)
		(void)setgid(getgid());

	if ((kd = kvm_openfiles(nlistf, memf, NULL, O_RDONLY, buf)) == 0)
		errx(1, "kvm_openfiles: %s", buf);
	if ((ret = kvm_nlist(kd, nl)) != 0) {
		if (ret == -1)
			errx(1, "kvm_nlist: %s", kvm_geterr(kd));
		for (i = NLMANDATORYBEG, quit = 0; i <= NLMANDATORYEND; i++)
			if (!nl[i].n_value) {
				quit = 1;
				warnx("undefined symbol: %s", nl[i].n_name);
			}
		if (quit)
			exit(1);
	}
	if (!(fileflag | vnodeflag | ttyflag | swapflag | totalflag))
		usage();
	if (fileflag || totalflag)
		filemode();
	if (vnodeflag)
		vnodemode();
	if (ttyflag)
		ttymode();
	if (swapflag || totalflag)
		swapmode();
	exit (0);
}

static void
usage()
{
	fprintf(stderr, "usage: %s\n", usagestr);
	exit (1);
}

struct e_vnode {
	struct vnode *avnode;
	struct vnode vnode;
};

void
vnodemode()
{
	struct e_vnode *e_vnodebase, *endvnode, *evp;
	struct vnode *vp;
	struct mount *maddr, *mp;
	int numvnodes;

	e_vnodebase = loadvnodes(&numvnodes);
	if (totalflag) {
		(void)printf("%7d vnodes\n", numvnodes);
		return;
	}
	endvnode = e_vnodebase + numvnodes;
	(void)printf("%d active vnodes\n", numvnodes);


#define ST	mp->mnt_stat
	maddr = NULL;
	for (evp = e_vnodebase; evp < endvnode; evp++) {
		vp = &evp->vnode;
		if (vp->v_mount != maddr) {
			/*
			 * New filesystem
			 */
			if ((mp = getmnt(vp->v_mount)) == NULL)
				continue;
			maddr = vp->v_mount;
			mount_print(mp);
			vnode_header();
			if (!strcmp(ST.f_fstypename, "ufs"))
				ufs_header();
			else if (!strcmp(ST.f_fstypename, "nfs"))
				nfs_header();
			else if (!strcmp(ST.f_fstypename, "union"))
				union_header();
			(void)printf("\n");
		}
		vnode_print(evp->avnode, vp);
		if (!strcmp(ST.f_fstypename, "ufs"))
			ufs_print(vp);
		else if (!strcmp(ST.f_fstypename, "nfs"))
			nfs_print(vp);
		else if (!strcmp(ST.f_fstypename, "union"))
			union_print(vp);
		(void)printf("\n");
	}
	free(e_vnodebase);
}

void
vnode_header()
{
	(void)printf("ADDR     TYP VFLAG  USE HOLD");
}

void
vnode_print(avnode, vp)
	struct vnode *avnode;
	struct vnode *vp;
{
	char *type, flags[16];
	char *fp = flags;
	int flag;

	/*
	 * set type
	 */
	switch (vp->v_type) {
	case VNON:
		type = "non"; break;
	case VREG:
		type = "reg"; break;
	case VDIR:
		type = "dir"; break;
	case VBLK:
		type = "blk"; break;
	case VCHR:
		type = "chr"; break;
	case VLNK:
		type = "lnk"; break;
	case VSOCK:
		type = "soc"; break;
	case VFIFO:
		type = "fif"; break;
	case VBAD:
		type = "bad"; break;
	default:
		type = "unk"; break;
	}
	/*
	 * gather flags
	 */
	flag = vp->v_flag;
	if (flag & VROOT)
		*fp++ = 'R';
	if (flag & VTEXT)
		*fp++ = 'T';
	if (flag & VSYSTEM)
		*fp++ = 'S';
	if (flag & VISTTY)
		*fp++ = 't';
	if (flag & VXLOCK)
		*fp++ = 'L';
	if (flag & VXWANT)
		*fp++ = 'W';
	if (flag & VBWAIT)
		*fp++ = 'B';
	if (flag & VOBJBUF)
		*fp++ = 'V';
	if (flag & VCOPYONWRITE)
		*fp++ = 'C';
	if (flag & VAGE)
		*fp++ = 'a';
	if (flag & VOLOCK)
		*fp++ = 'l';
	if (flag & VOWANT)
		*fp++ = 'w';
	if (flag & VDOOMED)
		*fp++ = 'D';
	if (flag & VFREE)
		*fp++ = 'F';
	if (flag & VONWORKLST)
		*fp++ = 'O';
	if (flag & VMOUNT)
		*fp++ = 'M';

	if (flag == 0)
		*fp++ = '-';
	*fp = '\0';
	(void)printf("%8lx %s %5s %4d %4d",
	    (u_long)(void *)avnode, type, flags, vp->v_usecount, vp->v_holdcnt);
}

void
ufs_header()
{
	(void)printf(" FILEID IFLAG RDEV|SZ");
}

int
ufs_print(vp)
	struct vnode *vp;
{
	int flag;
	struct inode inode, *ip = &inode;
	char flagbuf[16], *flags = flagbuf;
	char *name;
	mode_t type;

	KGETRET(VTOI(vp), &inode, sizeof(struct inode), "vnode's inode");
	flag = ip->i_flag;
	if (flag & IN_ACCESS)
		*flags++ = 'A';
	if (flag & IN_CHANGE)
		*flags++ = 'C';
	if (flag & IN_UPDATE)
		*flags++ = 'U';
	if (flag & IN_MODIFIED)
		*flags++ = 'M';
	if (flag & IN_RENAME)
		*flags++ = 'R';
	if (flag & IN_HASHED)
		*flags++ = 'H';
	if (flag & IN_LAZYMOD)
		*flags++ = 'L';
	if (flag == 0)
		*flags++ = '-';
	*flags = '\0';

	(void)printf(" %6d %5s", ip->i_number, flagbuf);
	type = ip->i_mode & S_IFMT;
	if (S_ISCHR(ip->i_mode) || S_ISBLK(ip->i_mode))
		if (usenumflag || ((name = devname(ip->i_rdev, type)) == NULL))
			(void)printf("   %2d,%-2d",
			    major(ip->i_rdev), minor(ip->i_rdev));
		else
			(void)printf(" %7s", name);
	else
		(void)printf(" %7qd", ip->i_size);
	return (0);
}

void
nfs_header()
{
	(void)printf(" FILEID NFLAG RDEV|SZ");
}

int
nfs_print(vp)
	struct vnode *vp;
{
	struct nfsnode nfsnode, *np = &nfsnode;
	char flagbuf[16], *flags = flagbuf;
	int flag;
	char *name;
	mode_t type;

	KGETRET(VTONFS(vp), &nfsnode, sizeof(nfsnode), "vnode's nfsnode");
	flag = np->n_flag;
	if (flag & NFLUSHWANT)
		*flags++ = 'W';
	if (flag & NFLUSHINPROG)
		*flags++ = 'P';
	if (flag & NMODIFIED)
		*flags++ = 'M';
	if (flag & NWRITEERR)
		*flags++ = 'E';
	if (flag & NQNFSNONCACHE)
		*flags++ = 'X';
	if (flag & NQNFSWRITE)
		*flags++ = 'O';
	if (flag & NQNFSEVICTED)
		*flags++ = 'G';
	if (flag & NACC)
		*flags++ = 'A';
	if (flag & NUPD)
		*flags++ = 'U';
	if (flag & NCHG)
		*flags++ = 'C';
	if (flag & NLOCKED)
		*flags++ = 'L';
	if (flag & NWANTED)
		*flags++ = 'w';
	if (flag == 0)
		*flags++ = '-';
	*flags = '\0';

#define VT	np->n_vattr
	(void)printf(" %6ld %5s", VT.va_fileid, flagbuf);
	type = VT.va_mode & S_IFMT;
	if (S_ISCHR(VT.va_mode) || S_ISBLK(VT.va_mode))
		if (usenumflag || ((name = devname(VT.va_rdev, type)) == NULL))
			(void)printf("   %2d,%-2d",
			    major(VT.va_rdev), minor(VT.va_rdev));
		else
			(void)printf(" %7s", name);
	else
		(void)printf(" %7qd", np->n_size);
	return (0);
}

void
union_header() 
{
	(void)printf("    UPPER    LOWER");
}

int
union_print(vp) 
	struct vnode *vp;
{
	struct union_node unode, *up = &unode;

	KGETRET(VTOUNION(vp), &unode, sizeof(unode), "vnode's unode");

	(void)printf(" %8lx %8lx", (u_long)(void *)up->un_uppervp,
	    (u_long)(void *)up->un_lowervp);
	return (0);
}
	
/*
 * Given a pointer to a mount structure in kernel space,
 * read it in and return a usable pointer to it.
 */
struct mount *
getmnt(maddr)
	struct mount *maddr;
{
	static struct mtab {
		struct mtab *next;
		struct mount *maddr;
		struct mount mount;
	} *mhead = NULL;
	struct mtab *mt;

	for (mt = mhead; mt != NULL; mt = mt->next)
		if (maddr == mt->maddr)
			return (&mt->mount);
	if ((mt = malloc(sizeof(struct mtab))) == NULL)
		errx(1, "malloc");
	KGETRET(maddr, &mt->mount, sizeof(struct mount), "mount table");
	mt->maddr = maddr;
	mt->next = mhead;
	mhead = mt;
	return (&mt->mount);
}

void
mount_print(mp)
	struct mount *mp;
{
	int flags;
	const char *type;

#define ST	mp->mnt_stat
	(void)printf("*** MOUNT %s %s on %s", ST.f_fstypename,
	    ST.f_mntfromname, ST.f_mntonname);
	if ((flags = mp->mnt_flag)) {
		int i;
		const char *sep = " (";

		for (i = 0; mnt_flags[i].m_flag; i++) {
			if (flags & mnt_flags[i].m_flag) {
				(void)printf("%s%s", sep, mnt_flags[i].m_name);
				flags &= ~mnt_flags[i].m_flag;
				sep = ",";
			}
		}
		if (flags)
			(void)printf("%sunknown_flags:%x", sep, flags);
		(void)printf(")");
	}
	(void)printf("\n");
#undef ST
}

struct e_vnode *
loadvnodes(avnodes)
	int *avnodes;
{
	int mib[2];
	size_t copysize;
	struct e_vnode *vnodebase;

	if (memf != NULL) {
		/*
		 * do it by hand
		 */
		return (kinfo_vnodes(avnodes));
	}
	mib[0] = CTL_KERN;
	mib[1] = KERN_VNODE;
	if (sysctl(mib, 2, NULL, &copysize, NULL, 0) == -1)
		err(1, "sysctl: KERN_VNODE");
	if ((vnodebase = malloc(copysize)) == NULL)
		errx(1, "malloc");
	if (sysctl(mib, 2, vnodebase, &copysize, NULL, 0) == -1)
		err(1, "sysctl: KERN_VNODE");
	if (copysize % sizeof(struct e_vnode))
		errx(1, "vnode size mismatch");
	*avnodes = copysize / sizeof(struct e_vnode);

	return (vnodebase);
}

/*
 * simulate what a running kernel does in in kinfo_vnode
 */
struct e_vnode *
kinfo_vnodes(avnodes)
	int *avnodes;
{
	struct mntlist mountlist;
	struct mount *mp, mount, *mp_next;
	struct vnode *vp, vnode, *vp_next;
	char *vbuf, *evbuf, *bp;
	int num, numvnodes;

#define VPTRSZ  sizeof(struct vnode *)
#define VNODESZ sizeof(struct vnode)

	KGET(V_NUMV, numvnodes);
	if ((vbuf = malloc((numvnodes + 20) * (VPTRSZ + VNODESZ))) == NULL)
		errx(1, "malloc");
	bp = vbuf;
	evbuf = vbuf + (numvnodes + 20) * (VPTRSZ + VNODESZ);
	KGET(V_MOUNTLIST, mountlist);
	for (num = 0, mp = TAILQ_FIRST(&mountlist); ; mp = mp_next) {
		KGET2(mp, &mount, sizeof(mount), "mount entry");
		mp_next = TAILQ_NEXT(&mount, mnt_list);
		for (vp = LIST_FIRST(&mount.mnt_vnodelist);
		    vp != NULL; vp = vp_next) {
			KGET2(vp, &vnode, sizeof(vnode), "vnode");
			vp_next = LIST_NEXT(&vnode, v_mntvnodes);
			if ((bp + VPTRSZ + VNODESZ) > evbuf)
				/* XXX - should realloc */
				errx(1, "no more room for vnodes");
			memmove(bp, &vp, VPTRSZ);
			bp += VPTRSZ;
			memmove(bp, &vnode, VNODESZ);
			bp += VNODESZ;
			num++;
		}
		if (mp == TAILQ_LAST(&mountlist, mntlist))
			break;
	}
	*avnodes = num;
	return ((struct e_vnode *)vbuf);
}

const char hdr[] =
"  LINE RAW CAN OUT IHIWT ILOWT OHWT LWT     COL STATE  SESS      PGID DISC\n";
int ttyspace = 128;

void
ttymode()
{
	struct tty *tty;
	struct tty ttyb[1000];
	int error, len, i;

	(void)printf(hdr);
	len = sizeof(ttyb);
	error = sysctlbyname("kern.ttys", &ttyb, &len, 0, 0);
	if (!error) {
		len /= sizeof(ttyb[0]);
		for (i = 0; i < len; i++) {
			ttyprt(&ttyb[i], 0);
		}
	}
	if ((tty = malloc(ttyspace * sizeof(*tty))) == NULL)
		errx(1, "malloc");
	if (nl[SCONS].n_type != 0) {
		(void)printf("1 console\n");
		KGET(SCONS, *tty);
		ttyprt(&tty[0], 0);
	}
#ifdef __FreeBSD__
	if (nl[NSCCONS].n_type != 0)
		ttytype(tty, "vty", SCCONS, NSCCONS, 0);
	if (nl[NSIO].n_type != 0)
		ttytype(tty, "sio", SIO, NSIO, 0);
	if (nl[NRC].n_type != 0)
		ttytype(tty, "rc", RC, NRC, 0);
	if (nl[NCY].n_type != 0)
		ttytype(tty, "cy", CY, NCY, 0);
	if (nl[NSI].n_type != 0)
		ttytype(tty, "si", SI, NSI, 1);
#endif
	if (nl[SNPTY].n_type != 0)
		ttytype(tty, "pty", SPTY, SNPTY, 0);
}

void
ttytype(tty, name, type, number, indir)
	struct tty *tty;
	char *name;
	int type, number, indir;
{
	struct tty *tp;
	int ntty;
	struct tty **ttyaddr;

	if (tty == NULL)
		return;
	KGET(number, ntty);
	(void)printf("%d %s %s\n", ntty, name, (ntty == 1) ? "line" : "lines");
	if (ntty > ttyspace) {
		ttyspace = ntty;
		if ((tty = realloc(tty, ttyspace * sizeof(*tty))) == 0)
			errx(1, "realloc");
	}
	if (indir) {
		KGET(type, ttyaddr);
		KGET2(ttyaddr, tty, ntty * sizeof(struct tty), "tty structs");
	} else {
		KGET1(type, tty, ntty * sizeof(struct tty), "tty structs");
	}
	(void)printf(hdr);
	for (tp = tty; tp < &tty[ntty]; tp++)
		ttyprt(tp, tp - tty);
}

struct {
	int flag;
	char val;
} ttystates[] = {
#ifdef TS_WOPEN
	{ TS_WOPEN,	'W'},
#endif
	{ TS_ISOPEN,	'O'},
	{ TS_CARR_ON,	'C'},
#ifdef TS_CONNECTED
	{ TS_CONNECTED,	'c'},
#endif
	{ TS_TIMEOUT,	'T'},
	{ TS_FLUSH,	'F'},
	{ TS_BUSY,	'B'},
#ifdef TS_ASLEEP
	{ TS_ASLEEP,	'A'},
#endif
#ifdef TS_SO_OLOWAT
	{ TS_SO_OLOWAT,	'A'},
#endif
#ifdef TS_SO_OCOMPLETE
	{ TS_SO_OCOMPLETE, 'a'},
#endif
	{ TS_XCLUDE,	'X'},
	{ TS_TTSTOP,	'S'},
#ifdef TS_CAR_OFLOW
	{ TS_CAR_OFLOW,	'm'},
#endif
#ifdef TS_CTS_OFLOW
	{ TS_CTS_OFLOW,	'o'},
#endif
#ifdef TS_DSR_OFLOW
	{ TS_DSR_OFLOW,	'd'},
#endif
	{ TS_TBLOCK,	'K'},
	{ TS_ASYNC,	'Y'},
	{ TS_BKSL,	'D'},
	{ TS_ERASE,	'E'},
	{ TS_LNCH,	'L'},
	{ TS_TYPEN,	'P'},
	{ TS_CNTTB,	'N'},
#ifdef TS_CAN_BYPASS_L_RINT
	{ TS_CAN_BYPASS_L_RINT, 'l'},
#endif
#ifdef TS_SNOOP
	{ TS_SNOOP,     's'},
#endif
#ifdef TS_ZOMBIE
	{ TS_ZOMBIE,	'Z'},
#endif
	{ 0,	       '\0'},
};

void
ttyprt(tp, line)
	struct tty *tp;
	int line;
{
	int i, j;
	pid_t pgid;
	char *name, state[20];

	if (usenumflag || tp->t_dev == 0 ||
	   (name = devname(tp->t_dev, S_IFCHR)) == NULL)
		(void)printf("%7d ", line);
	else
		(void)printf("%7s ", name);
	(void)printf("%2d %3d ", tp->t_rawq.c_cc, tp->t_canq.c_cc);
	(void)printf("%3d %5d %5d %4d %3d %7d ", tp->t_outq.c_cc,
		tp->t_ihiwat, tp->t_ilowat, tp->t_ohiwat, tp->t_olowat,
		tp->t_column);
	for (i = j = 0; ttystates[i].flag; i++)
		if (tp->t_state&ttystates[i].flag)
			state[j++] = ttystates[i].val;
	if (j == 0)
		state[j++] = '-';
	state[j] = '\0';
	(void)printf("%-6s %8lx", state, (u_long)(void *)tp->t_session);
	pgid = 0;
	if (tp->t_pgrp != NULL)
		KGET2(&tp->t_pgrp->pg_id, &pgid, sizeof(pid_t), "pgid");
	(void)printf("%6d ", pgid);
	switch (tp->t_line) {
	case TTYDISC:
		(void)printf("term\n");
		break;
	case NTTYDISC:
		(void)printf("ntty\n");
		break;
	case SLIPDISC:
		(void)printf("slip\n");
		break;
	case PPPDISC:
		(void)printf("ppp\n");
		break;
	default:
		(void)printf("%d\n", tp->t_line);
		break;
	}
}

void
filemode()
{
	struct file *fp;
	struct file *addr;
	char *buf, flagbuf[16], *fbp;
	int len, maxfile, nfile;
	static char *dtypes[] = { "???", "inode", "socket" };

	KGET(FNL_MAXFILE, maxfile);
	if (totalflag) {
		KGET(FNL_NFILE, nfile);
		(void)printf("%3d/%3d files\n", nfile, maxfile);
		return;
	}
	if (getfiles(&buf, &len) == -1)
		return;
	/*
	 * Getfiles returns in malloc'd memory a pointer to the first file
	 * structure, and then an array of file structs (whose addresses are
	 * derivable from the previous entry).
	 */
	addr = LIST_FIRST((struct filelist *)buf);
	fp = (struct file *)(buf + sizeof(struct filelist));
	nfile = (len - sizeof(struct filelist)) / sizeof(struct file);

	(void)printf("%d/%d open files\n", nfile, maxfile);
	(void)printf("   LOC   TYPE    FLG     CNT  MSG    DATA    OFFSET\n");
	for (; (char *)fp < buf + len; addr = LIST_NEXT(fp, f_list), fp++) {
		if ((unsigned)fp->f_type > DTYPE_SOCKET)
			continue;
		(void)printf("%8lx ", (u_long)(void *)addr);
		(void)printf("%-8.8s", dtypes[fp->f_type]);
		fbp = flagbuf;
		if (fp->f_flag & FREAD)
			*fbp++ = 'R';
		if (fp->f_flag & FWRITE)
			*fbp++ = 'W';
		if (fp->f_flag & FAPPEND)
			*fbp++ = 'A';
		if (fp->f_flag & FASYNC)
			*fbp++ = 'I';
		*fbp = '\0';
		(void)printf("%6s  %3d", flagbuf, fp->f_count);
		(void)printf("  %3d", fp->f_msgcount);
		(void)printf("  %8lx", (u_long)(void *)fp->f_data);
		if (fp->f_offset < 0)
			(void)printf("  %qx\n", fp->f_offset);
		else
			(void)printf("  %qd\n", fp->f_offset);
	}
	free(buf);
}

int
getfiles(abuf, alen)
	char **abuf;
	int *alen;
{
	size_t len;
	int mib[2];
	char *buf;

	/*
	 * XXX
	 * Add emulation of KINFO_FILE here.
	 */
	if (memf != NULL)
		errx(1, "files on dead kernel, not implemented");

	mib[0] = CTL_KERN;
	mib[1] = KERN_FILE;
	if (sysctl(mib, 2, NULL, &len, NULL, 0) == -1) {
		warn("sysctl: KERN_FILE");
		return (-1);
	}
	if ((buf = malloc(len)) == NULL)
		errx(1, "malloc");
	if (sysctl(mib, 2, buf, &len, NULL, 0) == -1) {
		warn("sysctl: KERN_FILE");
		return (-1);
	}
	*abuf = buf;
	*alen = len;
	return (0);
}

/*
 * swapmode is based on a program called swapinfo written
 * by Kevin Lahey <kml@rokkaku.atl.ga.us>.
 */
void
swapmode(void)
{
	struct kvm_swap kswap[16];
	int i;
	int n;
	int pagesize = getpagesize();
	const char *header;
	int hlen;
	long blocksize;

	n = kvm_getswapinfo(
	    kd, 
	    kswap,
	    sizeof(kswap)/sizeof(kswap[0]),
	    ((swapflag > 1) ? SWIF_DUMP_TREE : 0) | SWIF_DEV_PREFIX
	);

#define CONVERT(v)	((int)((quad_t)(v) * pagesize / blocksize))

	header = getbsize(&hlen, &blocksize);
	if (totalflag == 0) {
		(void)printf("%-15s %*s %8s %8s %8s  %s\n",
		    "Device", hlen, header,
		    "Used", "Avail", "Capacity", "Type");

		for (i = 0; i < n; ++i) {
			(void)printf(
			    "%-15s %*d ",
			    kswap[i].ksw_devname,
			    hlen,
			    CONVERT(kswap[i].ksw_total)
			);
			(void)printf(
			    "%8d %8d %5.0f%%    %s\n",
			    CONVERT(kswap[i].ksw_used),
			    CONVERT(kswap[i].ksw_total - kswap[i].ksw_used),
			    (double)kswap[i].ksw_used * 100.0 /
				(double)kswap[i].ksw_total,
			    (kswap[i].ksw_flags & SW_SEQUENTIAL) ?
				"Sequential" : "Interleaved"
			);
		}
	}

	if (totalflag) {
		blocksize = 1024 * 1024;

		(void)printf(
		    "%dM/%dM swap space\n", 
		    CONVERT(kswap[n].ksw_used),
		    CONVERT(kswap[n].ksw_total)
		);
	} else if (n > 1) {
		(void)printf(
		    "%-15s %*d %8d %8d %5.0f%%\n",
		    "Total",
		    hlen, 
		    CONVERT(kswap[n].ksw_total),
		    CONVERT(kswap[n].ksw_used),
		    CONVERT(kswap[n].ksw_total - kswap[n].ksw_used),
		    (double)kswap[n].ksw_used * 100.0 /
			(double)kswap[n].ksw_total
		);
	}
}
