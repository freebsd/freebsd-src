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
static char copyright[] =
"@(#) Copyright (c) 1980, 1991, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)pstat.c	8.16 (Berkeley) 5/9/95";
#endif /* not lint */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/vnode.h>
#include <sys/map.h>
#include <sys/ucred.h>
#define KERNEL
#include <sys/file.h>
#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#define NFS
#include <sys/mount.h>
#undef NFS
#include <sys/uio.h>
#include <sys/namei.h>
#include <miscfs/union/union.h>
#undef KERNEL
#include <sys/stat.h>
#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfs/nfs.h>
#include <nfs/nfsnode.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/conf.h>

#include <sys/sysctl.h>

#include <err.h>
#include <kvm.h>
#include <limits.h>
#include <nlist.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct nlist nl[] = {
#define VM_SWAPMAP	0
	{ "_swapmap" },	/* list of free swap areas */
#define VM_NSWAPMAP	1
	{ "_nswapmap" },/* size of the swap map */
#define VM_SWDEVT	2
	{ "_swdevt" },	/* list of swap devices and sizes */
#define VM_NSWAP	3
	{ "_nswap" },	/* size of largest swap device */
#define VM_NSWDEV	4
	{ "_nswdev" },	/* number of swap devices */
#define VM_DMMAX	5
	{ "_dmmax" },	/* maximum size of a swap block */
#define	V_MOUNTLIST	6
	{ "_mountlist" },	/* address of head of mount list. */
#define V_NUMV		7
	{ "_numvnodes" },
#define	FNL_NFILE	8
	{"_nfiles"},
#define FNL_MAXFILE	9
	{"_maxfiles"},
#define NLMANDATORY FNL_MAXFILE	/* names up to here are mandatory */
#define VM_NISWAP	NLMANDATORY + 1
	{ "_niswap" },
#define VM_NISWDEV	NLMANDATORY + 2
	{ "_niswdev" },
#define	SCONS		NLMANDATORY + 3
	{ "_cons" },
#define	SPTY		NLMANDATORY + 4
	{ "_pt_tty" },
#define	SNPTY		NLMANDATORY + 5
	{ "_npty" },

#ifdef hp300
#define	SDCA	(SNPTY+1)
	{ "_dca_tty" },
#define	SNDCA	(SNPTY+2)
	{ "_ndca" },
#define	SDCM	(SNPTY+3)
	{ "_dcm_tty" },
#define	SNDCM	(SNPTY+4)
	{ "_ndcm" },
#define	SDCL	(SNPTY+5)
	{ "_dcl_tty" },
#define	SNDCL	(SNPTY+6)
	{ "_ndcl" },
#define	SITE	(SNPTY+7)
	{ "_ite_tty" },
#define	SNITE	(SNPTY+8)
	{ "_nite" },
#endif

#ifdef mips
#define SDC	(SNPTY+1)
	{ "_dc_tty" },
#define SNDC	(SNPTY+2)
	{ "_dc_cnt" },
#endif

	{ "" }
};

int	usenumflag;
int	totalflag;
char	*nlistf	= NULL;
char	*memf	= NULL;
kvm_t	*kd;

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
	{ MNT_EXRDONLY, "exrdonly" },
	{ MNT_EXPORTED, "exported" },
	{ MNT_DEFEXPORTED, "defexported" },
	{ MNT_EXPORTANON, "exportanon" },
	{ MNT_EXKERB, "exkerb" },
	{ MNT_LOCAL, "local" },
	{ MNT_QUOTA, "quota" },
	{ MNT_ROOTFS, "rootfs" },
	{ MNT_UPDATE, "update" },
	{ MNT_DELEXPORT },
	{ MNT_UPDATE, "update" },
	{ MNT_DELEXPORT, "delexport" },
	{ MNT_RELOAD, "reload" },
	{ MNT_FORCE, "force" },
	{ MNT_MLOCK, "mlock" },
	{ MNT_WAIT, "wait" },
	{ MNT_MPBUSY, "mpbusy" },
	{ MNT_MPWANT, "mpwant" },
	{ MNT_UNMOUNT, "unmount" },
	{ MNT_WANTRDWR, "wantrdwr" },
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
void	ttytype __P((struct tty *, char *, int, int));
void	ufs_header __P((void));
int	ufs_print __P((struct vnode *));
void	union_header __P((void));
int	union_print __P((struct vnode *));
void	usage __P((void));
void	vnode_header __P((void));
void	vnode_print __P((struct vnode *, struct vnode *));
void	vnodemode __P((void));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	extern char *optarg;
	extern int optind;
	int ch, i, quit, ret;
	int fileflag, swapflag, ttyflag, vnodeflag;
	char buf[_POSIX2_LINE_MAX];

	fileflag = swapflag = ttyflag = vnodeflag = 0;
	while ((ch = getopt(argc, argv, "TM:N:finstv")) != EOF)
		switch (ch) {
		case 'f':
			fileflag = 1;
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
			swapflag = 1;
			break;
		case 'T':
			totalflag = 1;
			break;
		case 't':
			ttyflag = 1;
			break;
		case 'v':
		case 'i':		/* Backward compatibility. */
			vnodeflag = 1;
			break;
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
		for (i = quit = 0; i <= NLMANDATORY; i++)
			if (!nl[i].n_value) {
				quit = 1;
				warnx("undefined symbol: %s\n", nl[i].n_name);
			}
		if (quit)
			exit(1);
	}
	if (!(fileflag | vnodeflag | ttyflag | swapflag | totalflag))
		usage();
	if (fileflag || totalflag)
		filemode();
	if (vnodeflag || totalflag)
		vnodemode();
	if (ttyflag)
		ttymode();
	if (swapflag || totalflag)
		swapmode();
	exit (0);
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
			if (!strcmp(ST.f_fstypename, "ufs") ||
			    !strcmp(ST.f_fstypename, "mfs"))
				ufs_header();
			else if (!strcmp(ST.f_fstypename, "nfs"))
				nfs_header();
			else if (!strcmp(ST.f_fstypename, "union"))
				union_header();
			(void)printf("\n");
		}
		vnode_print(evp->avnode, vp);
		if (!strcmp(ST.f_fstypename, "ufs") ||
		    !strcmp(ST.f_fstypename, "mfs"))
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
	if (flag & VXLOCK)
		*fp++ = 'L';
	if (flag & VXWANT)
		*fp++ = 'W';
	if (flag & VBWAIT)
		*fp++ = 'B';
	if (flag & VALIASED)
		*fp++ = 'A';
	if (flag == 0)
		*fp++ = '-';
	*fp = '\0';
	(void)printf("%8x %s %5s %4d %4d",
	    avnode, type, flags, vp->v_usecount, vp->v_holdcnt);
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
	if (flag & IN_LOCKED)
		*flags++ = 'L';
	if (flag & IN_WANTED)
		*flags++ = 'W';
	if (flag & IN_RENAME)
		*flags++ = 'R';
	if (flag & IN_UPDATE)
		*flags++ = 'U';
	if (flag & IN_ACCESS)
		*flags++ = 'A';
	if (flag & IN_CHANGE)
		*flags++ = 'C';
	if (flag & IN_MODIFIED)
		*flags++ = 'M';
	if (flag & IN_SHLOCK)
		*flags++ = 'S';
	if (flag & IN_EXLOCK)
		*flags++ = 'E';
	if (flag & IN_LWAIT)
		*flags++ = 'Z';
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
	if (flag == 0)
		*flags++ = '-';
	*flags = '\0';

#define VT	np->n_vattr
	(void)printf(" %6d %5s", VT.va_fileid, flagbuf);
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

	(void)printf(" %8x %8x", up->un_uppervp, up->un_lowervp);
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
		err(1, NULL);
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
	if (flags = mp->mnt_flag) {
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
		err(1, NULL);
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
	struct mount *mp, mount;
	struct vnode *vp, vnode;
	char *vbuf, *evbuf, *bp;
	int num, numvnodes;

#define VPTRSZ  sizeof(struct vnode *)
#define VNODESZ sizeof(struct vnode)

	KGET(V_NUMV, numvnodes);
	if ((vbuf = malloc((numvnodes + 20) * (VPTRSZ + VNODESZ))) == NULL)
		err(1, NULL);
	bp = vbuf;
	evbuf = vbuf + (numvnodes + 20) * (VPTRSZ + VNODESZ);
	KGET(V_MOUNTLIST, mountlist);
	for (num = 0, mp = mountlist.cqh_first; ; mp = mp->mnt_list.cqe_next) {
		KGET2(mp, &mount, sizeof(mount), "mount entry");
		for (vp = mount.mnt_vnodelist.lh_first;
		    vp != NULL; vp = vp->v_mntvnodes.le_next) {
			KGET2(vp, &vnode, sizeof(vnode), "vnode");
			if ((bp + VPTRSZ + VNODESZ) > evbuf)
				/* XXX - should realloc */
				errx(1, "no more room for vnodes");
			memmove(bp, &vp, VPTRSZ);
			bp += VPTRSZ;
			memmove(bp, &vnode, VNODESZ);
			bp += VNODESZ;
			num++;
		}
		if (mp == mountlist.cqh_last)
			break;
	}
	*avnodes = num;
	return ((struct e_vnode *)vbuf);
}
	
char hdr[]="  LINE RAW CAN OUT  HWT LWT     COL STATE  SESS      PGID DISC\n";
int ttyspace = 128;

void
ttymode()
{
	struct tty *tty;

	if ((tty = malloc(ttyspace * sizeof(*tty))) == NULL)
		err(1, NULL);
#if !defined(hp300) && !defined(mips)
	(void)printf("1 console\n");
	KGET(SCONS, *tty);
	(void)printf(hdr);
	ttyprt(&tty[0], 0);
#endif
#ifdef vax
	if (nl[SNQD].n_type != 0) 
		qdss();
	if (nl[SNDZ].n_type != 0)
		ttytype(tty, "dz", SDZ, SNDZ);
	if (nl[SNDH].n_type != 0)
		ttytype(tty, "dh", SDH, SNDH);
	if (nl[SNDMF].n_type != 0)
		ttytype(tty, "dmf", SDMF, SNDMF);
	if (nl[SNDHU].n_type != 0)
		ttytype(tty, "dhu", SDHU, SNDHU);
	if (nl[SNDMZ].n_type != 0)
		ttytype(tty, "dmz", SDMZ, SNDMZ);
#endif
#ifdef tahoe
	if (nl[SNVX].n_type != 0)
		ttytype(tty, "vx", SVX, SNVX);
	if (nl[SNMP].n_type != 0)
		ttytype(tty, "mp", SMP, SNMP);
#endif
#ifdef hp300
	if (nl[SNITE].n_type != 0)
		ttytype(tty, "ite", SITE, SNITE);
	if (nl[SNDCA].n_type != 0)
		ttytype(tty, "dca", SDCA, SNDCA);
	if (nl[SNDCM].n_type != 0)
		ttytype(tty, "dcm", SDCM, SNDCM);
	if (nl[SNDCL].n_type != 0)
		ttytype(tty, "dcl", SDCL, SNDCL);
#endif
#ifdef mips
	if (nl[SNDC].n_type != 0)
		ttytype(tty, "dc", SDC, SNDC);
#endif
	if (nl[SNPTY].n_type != 0)
		ttytype(tty, "pty", SPTY, SNPTY);
}

void
ttytype(tty, name, type, number)
	struct tty *tty;
	char *name;
	int type, number;
{
	struct tty *tp;
	int ntty;

	if (tty == NULL)
		return;
	KGET(number, ntty);
	(void)printf("%d %s %s\n", ntty, name, (ntty == 1) ? "line" : "lines");
	if (ntty > ttyspace) {
		ttyspace = ntty;
		if ((tty = realloc(tty, ttyspace * sizeof(*tty))) == 0)
			err(1, NULL);
	}
	KGET1(type, tty, ntty * sizeof(struct tty), "tty structs");
	(void)printf(hdr);
	for (tp = tty; tp < &tty[ntty]; tp++)
		ttyprt(tp, tp - tty);
}

struct {
	int flag;
	char val;
} ttystates[] = {
	{ TS_WOPEN,	'W'},
	{ TS_ISOPEN,	'O'},
	{ TS_CARR_ON,	'C'},
	{ TS_TIMEOUT,	'T'},
	{ TS_FLUSH,	'F'},
	{ TS_BUSY,	'B'},
	{ TS_ASLEEP,	'A'},
	{ TS_XCLUDE,	'X'},
	{ TS_TTSTOP,	'S'},
	{ TS_TBLOCK,	'K'},
	{ TS_ASYNC,	'Y'},
	{ TS_BKSL,	'D'},
	{ TS_ERASE,	'E'},
	{ TS_LNCH,	'L'},
	{ TS_TYPEN,	'P'},
	{ TS_CNTTB,	'N'},
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
	(void)printf("%3d %4d %3d %7d ", tp->t_outq.c_cc, 
		tp->t_hiwat, tp->t_lowat, tp->t_column);
	for (i = j = 0; ttystates[i].flag; i++)
		if (tp->t_state&ttystates[i].flag)
			state[j++] = ttystates[i].val;
	if (j == 0)
		state[j++] = '-';
	state[j] = '\0';
	(void)printf("%-6s %8x", state, (u_long)tp->t_session);
	pgid = 0;
	if (tp->t_pgrp != NULL)
		KGET2(&tp->t_pgrp->pg_id, &pgid, sizeof(pid_t), "pgid");
	(void)printf("%6d ", pgid);
	switch (tp->t_line) {
	case TTYDISC:
		(void)printf("term\n");
		break;
	case TABLDISC:
		(void)printf("tab\n");
		break;
	case SLIPDISC:
		(void)printf("slip\n");
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
	addr = ((struct filelist *)buf)->lh_first;
	fp = (struct file *)(buf + sizeof(struct filelist));
	nfile = (len - sizeof(struct filelist)) / sizeof(struct file);
	
	(void)printf("%d/%d open files\n", nfile, maxfile);
	(void)printf("   LOC   TYPE    FLG     CNT  MSG    DATA    OFFSET\n");
	for (; (char *)fp < buf + len; addr = fp->f_list.le_next, fp++) {
		if ((unsigned)fp->f_type > DTYPE_SOCKET)
			continue;
		(void)printf("%x ", addr);
		(void)printf("%-8.8s", dtypes[fp->f_type]);
		fbp = flagbuf;
		if (fp->f_flag & FREAD)
			*fbp++ = 'R';
		if (fp->f_flag & FWRITE)
			*fbp++ = 'W';
		if (fp->f_flag & FAPPEND)
			*fbp++ = 'A';
#ifdef FSHLOCK	/* currently gone */
		if (fp->f_flag & FSHLOCK)
			*fbp++ = 'S';
		if (fp->f_flag & FEXLOCK)
			*fbp++ = 'X';
#endif
		if (fp->f_flag & FASYNC)
			*fbp++ = 'I';
		*fbp = '\0';
		(void)printf("%6s  %3d", flagbuf, fp->f_count);
		(void)printf("  %3d", fp->f_msgcount);
		(void)printf("  %8.1x", fp->f_data);
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
		errx(1, "files on dead kernel, not implemented\n");

	mib[0] = CTL_KERN;
	mib[1] = KERN_FILE;
	if (sysctl(mib, 2, NULL, &len, NULL, 0) == -1) {
		warn("sysctl: KERN_FILE");
		return (-1);
	}
	if ((buf = malloc(len)) == NULL)
		err(1, NULL);
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
swapmode()
{
	char *header, *p;
	int hlen, nswap, nswdev, dmmax, nswapmap, niswap, niswdev;
	int s, e, div, i, l, avail, nfree, npfree, used;
	struct swdevt *sw;
	long blocksize, *perdev;
	struct map *swapmap, *kswapmap;
	struct mapent *mp;

	KGET(VM_NSWAP, nswap);
	KGET(VM_NSWDEV, nswdev);
	KGET(VM_DMMAX, dmmax);
	KGET(VM_NSWAPMAP, nswapmap);
	KGET(VM_SWAPMAP, kswapmap);	/* kernel `swapmap' is a pointer */
	if ((sw = malloc(nswdev * sizeof(*sw))) == NULL ||
	    (perdev = malloc(nswdev * sizeof(*perdev))) == NULL ||
	    (mp = malloc(nswapmap * sizeof(*mp))) == NULL)
		err(1, "malloc");
	KGET1(VM_SWDEVT, sw, nswdev * sizeof(*sw), "swdevt");
	KGET2((long)kswapmap, mp, nswapmap * sizeof(*mp), "swapmap");

	/* Supports sequential swap */
	if (nl[VM_NISWAP].n_value != 0) {
		KGET(VM_NISWAP, niswap);
		KGET(VM_NISWDEV, niswdev);
	} else {
		niswap = nswap;
		niswdev = nswdev;
	}

	/* First entry in map is `struct map'; rest are mapent's. */
	swapmap = (struct map *)mp;
	if (nswapmap != swapmap->m_limit - (struct mapent *)kswapmap)
		errx(1, "panic: nswapmap goof");

	/* Count up swap space. */
	nfree = 0;
	memset(perdev, 0, nswdev * sizeof(*perdev));
	for (mp++; mp->m_addr != 0; mp++) {
		s = mp->m_addr;			/* start of swap region */
		e = mp->m_addr + mp->m_size;	/* end of region */
		nfree += mp->m_size;

		/*
		 * Swap space is split up among the configured disks.
		 *
		 * For interleaved swap devices, the first dmmax blocks
		 * of swap space some from the first disk, the next dmmax
		 * blocks from the next, and so on up to niswap blocks.
		 *
		 * Sequential swap devices follow the interleaved devices
		 * (i.e. blocks starting at niswap) in the order in which
		 * they appear in the swdev table.  The size of each device
		 * will be a multiple of dmmax.
		 *
		 * The list of free space joins adjacent free blocks,
		 * ignoring device boundries.  If we want to keep track
		 * of this information per device, we'll just have to
		 * extract it ourselves.  We know that dmmax-sized chunks
		 * cannot span device boundaries (interleaved or sequential)
		 * so we loop over such chunks assigning them to devices.
		 */
		i = -1;
		while (s < e) {		/* XXX this is inefficient */
			int bound = roundup(s+1, dmmax);

			if (bound > e)
				bound = e;
			if (bound <= niswap) {
				/* Interleaved swap chunk. */
				if (i == -1)
					i = (s / dmmax) % niswdev;
				perdev[i] += bound - s;
				if (++i >= niswdev)
					i = 0;
			} else {
				/* Sequential swap chunk. */
				if (i < niswdev) {
					i = niswdev;
					l = niswap + sw[i].sw_nblks;
				}
				while (s >= l) {
					/* XXX don't die on bogus blocks */
					if (i == nswdev-1)
						break;
					l += sw[++i].sw_nblks;
				}
				perdev[i] += bound - s;
			}
			s = bound;
		}
	}

	header = getbsize(&hlen, &blocksize);
	if (!totalflag)
		(void)printf("%-11s %*s %8s %8s %8s  %s\n",
		    "Device", hlen, header,
		    "Used", "Avail", "Capacity", "Type");
	div = blocksize / 512;
	avail = npfree = 0;
	for (i = 0; i < nswdev; i++) {
		int xsize, xfree;

		if (!totalflag) {
			p = devname(sw[i].sw_dev, S_IFBLK);
			(void)printf("/dev/%-6s %*d ", p == NULL ? "??" : p,
			    hlen, sw[i].sw_nblks / div);
		}

		/*
		 * Don't report statistics for partitions which have not
		 * yet been activated via swapon(8).
		 */
		if (!(sw[i].sw_flags & SW_FREED)) {
			if (totalflag)
				continue;
			(void)printf(" *** not available for swapping ***\n");
			continue;
		}
		xsize = sw[i].sw_nblks;
		xfree = perdev[i];
		used = xsize - xfree;
		npfree++;
		avail += xsize;
		if (totalflag)
			continue;
		(void)printf("%8d %8d %5.0f%%    %s\n", 
		    used / div, xfree / div,
		    (double)used / (double)xsize * 100.0,
		    (sw[i].sw_flags & SW_SEQUENTIAL) ?
			     "Sequential" : "Interleaved");
	}

	/* 
	 * If only one partition has been set up via swapon(8), we don't
	 * need to bother with totals.
	 */
	used = avail - nfree;
	if (totalflag) {
		(void)printf("%dM/%dM swap space\n", used / 2048, avail / 2048);
		return;
	}
	if (npfree > 1) {
		(void)printf("%-11s %*d %8d %8d %5.0f%%\n",
		    "Total", hlen, avail / div, used / div, nfree / div,
		    (double)used / (double)avail * 100.0);
	}
}

void
usage()
{
	(void)fprintf(stderr,
	    "usage: pstat -Tfnstv [system] [-M core] [-N system]\n");
	exit(1);
}
