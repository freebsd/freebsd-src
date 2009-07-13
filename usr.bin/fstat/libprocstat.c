#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/stat.h>
#include <sys/vnode.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/un.h>
#include <sys/unpcb.h>
#include <sys/sysctl.h>
#include <sys/tty.h>
#include <sys/filedesc.h>
#include <sys/queue.h>
#define	_WANT_FILE
#include <sys/file.h>
#include <sys/conf.h>
#define	_KERNEL
#include <sys/mount.h>
#include <sys/pipe.h>
#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <fs/devfs/devfs.h>
#include <fs/devfs/devfs_int.h>
#undef _KERNEL
#include <nfs/nfsproto.h>
#include <nfsclient/nfs.h>
#include <nfsclient/nfsnode.h>


#include <vm/vm.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>

#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>

#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <kvm.h>
#include <libutil.h>
#include <limits.h>
#include <paths.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>

#include "common.h"
#include "libprocstat.h"

static struct {
        int     vtype; 
        int     fst_vtype;
} vt2fst[] = {
        { VNON, PS_FST_VTYPE_VNON },
        { VREG, PS_FST_VTYPE_VREG },
        { VDIR, PS_FST_VTYPE_VDIR },
        { VBLK, PS_FST_VTYPE_VBLK },
        { VCHR, PS_FST_VTYPE_VCHR },
        { VLNK, PS_FST_VTYPE_VLNK },
        { VSOCK, PS_FST_VTYPE_VSOCK },
        { VFIFO, PS_FST_VTYPE_VFIFO },
        { VBAD, PS_FST_VTYPE_VBAD }
};
#define NVFTYPES (sizeof(vt2fst) / sizeof(*vt2fst))

char *getmnton(kvm_t *kd, struct mount *m);

/*
 * Filesystem specific handlers.
 */
#define FSTYPE(fst)     {#fst, fst##_filestat}
struct {
        const char      *tag;
        int             (*handler)(kvm_t *kd, struct vnode *vp,
            struct filestat *fsp);
} fstypes[] = {
        FSTYPE(ufs),
        FSTYPE(devfs),
        FSTYPE(nfs),
        FSTYPE(msdosfs),
        FSTYPE(isofs),
#ifdef ZFS
        FSTYPE(zfs),
#endif
/*
        FSTYPE(ntfs),
        FSTYPE(nwfs), 
        FSTYPE(smbfs),
        FSTYPE(udf), 
*/
};
#define NTYPES  (sizeof(fstypes) / sizeof(*fstypes))

#define	PROCSTAT_KVM 1
#define	PROCSTAT_SYSCTL 2

void
procstat_close(struct procstat *procstat)
{

	assert(procstat);
	if (procstat->type == PROCSTAT_KVM)
		kvm_close(procstat->kd);
}

struct procstat *
procstat_open(const char *nlistf, const char *memf)
{
	kvm_t *kd;
	char buf[_POSIX2_LINE_MAX];
	struct procstat *procstat;

	procstat = calloc(1, sizeof(*procstat));
	if (procstat == NULL) {
		warn("malloc()");
		return (NULL);
	}
	if (memf != NULL) {
		kd = kvm_openfiles(nlistf, memf, NULL, O_RDONLY, buf);
		if (kd == NULL) {
			warnx("kvm_openfiles(): %s", buf);
			free(procstat);
			return (NULL);
		}
		procstat->type = PROCSTAT_KVM;
		procstat->kd = kd;
	} else {
		procstat->type = PROCSTAT_SYSCTL;
	}
	return (procstat);
}

struct kinfo_proc *
procstat_getprocs(struct procstat *procstat, int what, int arg,
    unsigned int *count)
{
	struct kinfo_proc *p0, *p;
	size_t len;
	int name[4];
	int error;

	assert(procstat);
	assert(count);
	p = NULL;
	if (procstat->type == PROCSTAT_KVM) {
		p0 = kvm_getprocs(procstat->kd, what, arg, count);
		if (p0 == NULL || count == 0)
			return (NULL);
		len = *count * sizeof(*p);
		p = malloc(len);
		if (p == NULL) {
			warnx("malloc(%zd)", len);
			goto fail;
		}
		bcopy(p0, p, len);
		return (p);
	} else if (procstat->type == PROCSTAT_SYSCTL) {
		len = 0;
		name[0] = CTL_KERN;
		name[1] = KERN_PROC;
		name[2] = what;
		name[3] = arg;
		error = sysctl(name, 4, NULL, &len, NULL, 0);
		if (error < 0) {
			warn("sysctl(kern.proc)");
			goto fail;
		}
		if (len == 0) {
			warnx("no processes?");
			goto fail;
		}
		p = malloc(len);
		if (p == NULL) {
			warnx("malloc(%zd)", len);
			goto fail;
		}
		error = sysctl(name, 4, p, &len, NULL, 0);
		if (error < 0) {
			warn("sysctl(kern.proc)");
			goto fail;
		}
		/* Perform simple consistency checks. */
		if ((len % sizeof(*p)) != 0 || p->ki_structsize != sizeof(*p)) {
			warnx("kinfo_proc structure size mismatch");
			goto fail;
		}
		*count = len / sizeof(*p);
		return (p);
	} else {
		warnx("unknown access method");
		return (NULL);
	}
fail:
	if (p)
		free(p);
	return (NULL);
}

struct filestat *
procstat_getfiles(struct procstat *procstat, struct kinfo_proc *kp,
    unsigned int *cnt)
{
	
	if (procstat->type == PROCSTAT_SYSCTL)
		return (procstat_getfiles_sysctl(kp, cnt));
	else if (procstat->type == PROCSTAT_KVM)
		 return (procstat_getfiles_kvm(procstat->kd, kp, cnt));
	else
		return (NULL);
}

struct filestat *
procstat_getfiles_kvm(kvm_t *kd, struct kinfo_proc *kp, unsigned int *cnt)
{
	int i;
	struct file file;
	struct filedesc filed;
	unsigned int nfiles, count, f;
	struct file **ofiles;
	struct filestat *fst;

	assert(kd);
	assert(cnt);
	if (kp->ki_fd == NULL)
		return (NULL);
	if (!kvm_read_all(kd, (unsigned long)kp->ki_fd, &filed,
	    sizeof(filed))) {
		warnx("can't read filedesc at %p\n", (void *)kp->ki_fd);
		return (NULL);
	}
	count = 5;	/* Allocate additional space for special files. */
	if (filed.fd_lastfile >= 0)
		count += filed.fd_lastfile + 1;
	fst = malloc(count * sizeof(*fst));
	if (fst == NULL) {
		warn("malloc(%zd)", count * sizeof(*fst));
		return (NULL);
	}

	/* root directory vnode, if one. */
	f = 0;
	if (filed.fd_rdir)
		vtrans_kvm(kd, filed.fd_rdir, PS_FST_FD_RDIR, PS_FST_FFLAG_READ, &fst[f++]);
	/* current working directory vnode. */
	if (filed.fd_cdir)
		vtrans_kvm(kd, filed.fd_cdir, PS_FST_FD_CDIR, PS_FST_FFLAG_READ, &fst[f++]);
	/* jail root, if any. */
	if (filed.fd_jdir)
		vtrans_kvm(kd, filed.fd_jdir, PS_FST_FD_JAIL, PS_FST_FFLAG_READ, &fst[f++]);
	/* ktrace vnode, if one */
	if (kp->ki_tracep)
		vtrans_kvm(kd, kp->ki_tracep, PS_FST_FD_TRACE, PS_FST_FFLAG_READ | PS_FST_FFLAG_WRITE, &fst[f++]);
	/* text vnode, if one */
	if (kp->ki_textvp)
		vtrans_kvm(kd, kp->ki_textvp, PS_FST_FD_TEXT, PS_FST_FFLAG_READ, &fst[f++]);

	nfiles = filed.fd_lastfile + 1;
	ofiles = malloc(nfiles * sizeof(struct file *));
	if (ofiles == NULL) {
		warn("malloc(%zd)", nfiles * sizeof(struct file *));
		goto exit;
	}
	if (!kvm_read_all(kd, (unsigned long)filed.fd_ofiles, ofiles,
	    nfiles * sizeof(struct file *))) {
		warn("cannot read file structures at %p\n",
		    (void *)filed.fd_ofiles);
		free(ofiles);
		goto exit;
	}
	for (i = 0; i <= filed.fd_lastfile; i++) {
		if (ofiles[i] == NULL)
			continue;
		if (!kvm_read_all(kd, (unsigned long)ofiles[i], &file,
		    sizeof(struct file))) {
			warn("can't read file %d at %p\n", i,
			    (void *)ofiles[i]);
			continue;
		}
		switch (file.f_type) {
		case DTYPE_VNODE:
			vtrans_kvm(kd, file.f_vnode, i, file.f_flag, &fst[f++]);
			break;
/*
		case DTYPE_SOCKET:
			socktrans(file.f_data, i, &fst[f++]);
			break;
		case DTYPE_PIPE:
			pipetrans(file.f_data, i, file.f_flag, &fst[f++]);
			break;
		case DTYPE_FIFO:
			vtrans(file.f_vnode, i, file.f_flag, &fst[f++]);
			break;
		case DTYPE_PTS:
			ptstrans(file.f_data, i, file.f_flag, &fst[f++]);
			break;
*/
		default:
			dprintf(stderr,
			    "unknown file type %d for file %d\n",
			    file.f_type, i);
		}
	}
	free(ofiles);
exit:
	*cnt = f;
	return (fst);
}

struct filestat *
procstat_getfiles_sysctl(struct kinfo_proc *kp __unused, unsigned int *cnt __unused)
{
#if 0
	int i;
	struct file file;
	struct filedesc filed;
	unsigned int nfiles, count, f;
	struct file **ofiles;
	struct filestat *fst;

	assert(kp);
	assert(cnt);

	/*
	 * XXX: special files (TEXTVP, KTRACEVP...)
	 */

	/*
	 * Open files.
	 */
	freep = kinfo_getfile(kp->ki_pid, &count);
	if (freep == NULL) {
		warn("kinfo_getfile()");
		return (NULL);
	}
	if (count == 0)
		return (NULL);

	fst = malloc(count * sizeof(*fst));
	if (fst == NULL) {
		warn("malloc(%zd)", count * sizeof(*fst));
		return (NULL);
	}
	f = 0;
	for (i = 0; i < count; i++) {
		kif = &freep[i];
		switch (kif->kf_type) {
		case KF_TYPE_VNODE:
			if (kif->kf_fd == KF_FD_TYPE_CWD) {
				fd_type = CDIR;
				flags = FST_READ;
			} else if (kif->kf_fd == KF_FD_TYPE_ROOT) {
				fd_type = RDIR;
				flags = FST_READ;
			} else if (kif->kf_fd == KF_FD_TYPE_JAIL) {
				fd_type = JDIR;
				flags = FST_READ;
			} else {
				fd_type = i;
				flags = kif->kf_flags;
			}
			/* Only do this if the attributes are valid. */
			if (kif->kf_status & KF_ATTR_VALID)
				vtrans_sysctl(kif, fd_type, flags, &fst[f++]);
			break;
#if 0
		case KF_TYPE_PIPE:
			if (checkfile == 0)
				pipetrans_sysctl(kif, i, kif->kf_flags, &fst[f++]);
			break;
		case KF_TYPE_SOCKET:
			if (checkfile == 0)
				socktrans_sysctl(file.f_data, i);
			break;
		case KF_TYPE_PIPE:
			if (checkfile == 0)
				pipetrans_sysctl(file.f_data, i, file.f_flag, &fst[f++]);
			break;
		case KF_TYPE_FIFO:
			if (checkfile == 0)
				vtrans_sysctl(file.f_vnode, i, file.f_flag, &fst[f++]);
			break;
		case KF_TYPE_PTS:
			if (checkfile == 0)
				ptstrans_sysctl(file.f_data, i, file.f_flag, &fst[f++]);
			break;
#endif
		default:
			dprintf(stderr,
			    "unknown file type %d for file %d\n",
			    file.f_type, i);
		}
	}
	free(freep);
	*cnt = f;
	return (fst);
#endif
	return (NULL);
}

static int
vntype2psfsttype(int type)
{
	unsigned int i, fst_type;

	fst_type = PS_FST_VTYPE_UNKNOWN;
	for (i = 0; i < NVFTYPES; i++) {
		if (type == vt2fst[i].vtype) {
			fst_type = vt2fst[i].fst_vtype;
			break;
		}
	}
	return (fst_type);
}

int
vtrans_kvm(kvm_t *kd, struct vnode *vp, int fd, int flags, struct filestat *fst)
{
	char tagstr[12];
	int error;
	int found;
	struct vnode vn;
	unsigned int i;

	assert(vp);
	assert(fst);
	error = kvm_read_all(kd, (unsigned long)vp, &vn, sizeof(struct vnode));
	if (error == 0) {
		warnx("can't read vnode at %p\n", (void *)vp);
		return (1);
	}
	bzero(fst, sizeof(*fst));
	fst->vtype = vntype2psfsttype(vn.v_type);
	fst->type = PS_FST_TYPE_VNODE;
	fst->fd = fd;
	fst->fflags = flags;
	if (vn.v_type == VNON || vn.v_type == VBAD)
		return (0);

	error = kvm_read_all(kd, (unsigned long)vn.v_tag, tagstr, sizeof(tagstr));
	if (error == 0) {
		dprintf(stderr, "can't read v_tag at %p\n", (void *)vp);
		return (1);
	}
	tagstr[sizeof(tagstr) - 1] = '\0';

	/*
	 * Find appropriate handler.
	 */
	for (i = 0, found = 0; i < NTYPES; i++)
		if (!strcmp(fstypes[i].tag, tagstr)) {
			if (fstypes[i].handler(kd, &vn, fst) == 0) {
				fst->flags |= PS_FST_FLAG_ERROR;
				return (0);
			}
			break;
		}
	if (i == NTYPES) {
		fst->flags |= PS_FST_FLAG_UNKNOWNFS;
		return (0);
	}
	fst->mntdir = getmnton(kd, vn.v_mount);
	return (0);
}

char *
getmnton(kvm_t *kd, struct mount *m)
{
	static struct mount mnt;
	static struct mtab {
		struct mtab *next;
		struct mount *m;
		char mntonname[MNAMELEN + 1];
	} *mhead = NULL;
	struct mtab *mt;

	for (mt = mhead; mt != NULL; mt = mt->next)
		if (m == mt->m)
			return (mt->mntonname);
	if (!kvm_read_all(kd, (unsigned long)m, &mnt, sizeof(struct mount))) {
		warnx("can't read mount table at %p", (void *)m);
		return (NULL);
	}
	if ((mt = malloc(sizeof (struct mtab))) == NULL)
		err(1, NULL);
	mt->m = m;
	bcopy(&mnt.mnt_stat.f_mntonname[0], &mt->mntonname[0], MNAMELEN);
	mnt.mnt_stat.f_mntonname[MNAMELEN] = '\0';
	mt->next = mhead;
	mhead = mt;
	return (mt->mntonname);
}
