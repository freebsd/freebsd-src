/*-
 * Copyright (c) 2001, 2002 Scott Long <scottl@freebsd.org>
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/* udf_vnops.c */
/* Take care of the vnode side of things */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/stat.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/dirent.h>
#include <sys/queue.h>
#include <sys/unistd.h>

#include <vm/uma.h>

#include <fs/udf/ecma167-udf.h>
#include <fs/udf/osta.h>
#include <fs/udf/udf.h>

static int udf_access(struct vop_access_args *);
static int udf_getattr(struct vop_getattr_args *);
static int udf_ioctl(struct vop_ioctl_args *);
static int udf_pathconf(struct vop_pathconf_args *);
static int udf_read(struct vop_read_args *);
static int udf_readdir(struct vop_readdir_args *);
static int udf_readlink(struct vop_readlink_args *ap);
static int udf_strategy(struct vop_strategy_args *);
static int udf_print(struct vop_print_args *);
static int udf_bmap(struct vop_bmap_args *);
static int udf_lookup(struct vop_cachedlookup_args *);
static int udf_reclaim(struct vop_reclaim_args *);
static void udf_dumpblock(void *, int) __unused;
static int udf_readatoffset(struct udf_node *, int *, int, struct buf **, uint8_t **);
static int udf_bmap_internal(struct udf_node *, uint32_t, daddr_t *, uint32_t *);

vop_t **udf_vnodeop_p;
static struct vnodeopv_entry_desc udf_vnodeop_entries[] = {
	{ &vop_default_desc,		(vop_t *) vop_defaultop },
	{ &vop_access_desc,		(vop_t *) udf_access },
	{ &vop_bmap_desc,		(vop_t *) udf_bmap },
	{ &vop_cachedlookup_desc,	(vop_t *) udf_lookup },
	{ &vop_getattr_desc,		(vop_t *) udf_getattr },
	{ &vop_ioctl_desc,		(vop_t *) udf_ioctl },
	{ &vop_islocked_desc,		(vop_t *) vop_stdislocked },
	{ &vop_lock_desc,		(vop_t *) vop_stdlock },
	{ &vop_lookup_desc,		(vop_t *) vfs_cache_lookup },
	{ &vop_pathconf_desc,		(vop_t *) udf_pathconf },
	{ &vop_print_desc,		(vop_t *) udf_print },
	{ &vop_read_desc,		(vop_t *) udf_read },
	{ &vop_readdir_desc,		(vop_t *) udf_readdir },
	{ &vop_readlink_desc,		(vop_t *) udf_readlink },
	{ &vop_reclaim_desc,		(vop_t *) udf_reclaim },
	{ &vop_strategy_desc,		(vop_t *) udf_strategy },
	{ &vop_unlock_desc,		(vop_t *) vop_stdunlock },
	{ NULL, NULL }
};
static struct vnodeopv_desc udf_vnodeop_opv_desc =
	{ &udf_vnodeop_p, udf_vnodeop_entries };
VNODEOP_SET(udf_vnodeop_opv_desc);

MALLOC_DEFINE(M_UDFFID, "UDF FID", "UDF FileId structure");
MALLOC_DEFINE(M_UDFDS, "UDF DS", "UDF Dirstream structure");

#define UDF_INVALID_BMAP	-1

/* Look up a udf_node based on the ino_t passed in and return it's vnode */
int
udf_hashlookup(struct udf_mnt *udfmp, ino_t id, int flags, struct vnode **vpp)
{
	struct udf_node *node;
	int error;

	*vpp = NULL;

loop:
	mtx_lock(&udfmp->hash_mtx);
	TAILQ_FOREACH(node, &udfmp->udf_tqh, tq) {
		if (node->hash_id == id) {
			VI_LOCK(node->i_vnode);
			mtx_unlock(&udfmp->hash_mtx);
			error = vget(node->i_vnode, flags | LK_INTERLOCK,
			    curthread);
			if (error == ENOENT)
				goto loop;
			if (error)
				return (error);
			*vpp = node->i_vnode;
			return (0);
		}
	}

	mtx_unlock(&udfmp->hash_mtx);
	return (0);
}

int
udf_hashins(struct udf_node *node)
{
	struct udf_mnt *udfmp;

	udfmp = node->udfmp;

	mtx_lock(&udfmp->hash_mtx);
	TAILQ_INSERT_TAIL(&udfmp->udf_tqh, node, tq);
	mtx_unlock(&udfmp->hash_mtx);
	lockmgr(&node->i_vnode->v_lock, LK_EXCLUSIVE, (struct mtx *)0,
		curthread);

	return (0);
}

int
udf_hashrem(struct udf_node *node)
{
	struct udf_mnt *udfmp;

	udfmp = node->udfmp;

	mtx_lock(&udfmp->hash_mtx);
	TAILQ_REMOVE(&udfmp->udf_tqh, node, tq);
	mtx_unlock(&udfmp->hash_mtx);

	return (0);
}

int
udf_allocv(struct mount *mp, struct vnode **vpp, struct thread *td)
{
	int error;
	struct vnode *vp;

	error = getnewvnode(VT_UDF, mp, udf_vnodeop_p, &vp);
	if (error) {
		printf("udf_allocv: failed to allocate new vnode\n");
		return (error);
	}

	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
	*vpp = vp;
	return (0);
}

/* Convert file entry permission (5 bits per owner/group/user) to a mode_t */
static mode_t
udf_permtomode(struct udf_node *node)
{
	uint32_t perm;
	uint32_t flags;
	mode_t mode;

	perm = node->fentry->perm;
	flags = node->fentry->icbtag.flags;

	mode = perm & UDF_FENTRY_PERM_USER_MASK;
	mode |= ((perm & UDF_FENTRY_PERM_GRP_MASK) >> 2);
	mode |= ((perm & UDF_FENTRY_PERM_OWNER_MASK) >> 4);
	mode |= ((flags & UDF_ICB_TAG_FLAGS_STICKY) << 4);
	mode |= ((flags & UDF_ICB_TAG_FLAGS_SETGID) << 6);
	mode |= ((flags & UDF_ICB_TAG_FLAGS_SETUID) << 8);

	return (mode);
}
 
static int
udf_access(struct vop_access_args *a)
{
	struct vnode *vp;
	struct udf_node *node;
	mode_t a_mode, mode;

	vp = a->a_vp;
	node = VTON(vp);
	a_mode = a->a_mode;

	if (a_mode & VWRITE) {
		switch (vp->v_type) {
		case VDIR:
		case VLNK:
		case VREG:
			return (EROFS);
			/* NOT REACHED */
		default:
			break;
		}
	}

	mode = udf_permtomode(node);

	return (vaccess(vp->v_type, mode, node->fentry->uid, node->fentry->gid,
	    a_mode, a->a_cred, NULL));
}

static int mon_lens[2][12] = {
	{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
	{31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}
};

static int
udf_isaleapyear(int year)
{
	int i;

	i = (year % 4) ? 0 : 1;
	i &= (year % 100) ? 1 : 0;
	i |= (year % 400) ? 0 : 1;

	return i;
}

/*
 * XXX This is just a rough hack.  Daylight savings isn't calculated and tv_nsec
 * is ignored.
 * Timezone calculation compliments of Julian Elischer <julian@elischer.org>.
 */
static void
udf_timetotimespec(struct timestamp *time, struct timespec *t)
{
	int i, lpyear, daysinyear;
	union {
		uint16_t	u_tz_offset;
		int16_t		s_tz_offset;
	} tz;

	t->tv_nsec = 0;

	/* DirectCD seems to like using bogus year values */
	if (time->year < 1970) {
		t->tv_sec = 0;
		return;
	}

	/* Calculate the time and day */
	t->tv_sec = time->second;
	t->tv_sec += time->minute * 60;
	t->tv_sec += time->hour * 3600;
	t->tv_sec += time->day * 3600 * 24;

	/* Calclulate the month */
	lpyear = udf_isaleapyear(time->year);
	for (i = 1; i < time->month; i++)
		t->tv_sec += mon_lens[lpyear][i] * 3600 * 24; 

	/* Speed up the calculation */
	if (time->year > 1979)
		t->tv_sec += 315532800;
	if (time->year > 1989)
		t->tv_sec += 315619200;
	if (time->year > 1999)
		t->tv_sec += 315532800;
	for (i = 2000; i < time->year; i++) {
		daysinyear = udf_isaleapyear(i) + 365 ;
		t->tv_sec += daysinyear * 3600 * 24;
	}

	/*
	 * Calculate the time zone.  The timezone is 12 bit signed 2's
	 * compliment, so we gotta do some extra magic to handle it right.
	 */
	tz.u_tz_offset = time->type_tz;
	tz.u_tz_offset &= 0x0fff;
	if (tz.u_tz_offset & 0x0800)
		tz.u_tz_offset |= 0xf000;	/* extend the sign to 16 bits */
	if ((time->type_tz & 0x1000) && (tz.s_tz_offset != -2047))
		t->tv_sec -= tz.s_tz_offset * 60;

	return;
}

static int
udf_getattr(struct vop_getattr_args *a)
{
	struct vnode *vp;
	struct udf_node *node;
	struct vattr *vap;
	struct file_entry *fentry;
	struct timespec ts;

	ts.tv_sec = 0;

	vp = a->a_vp;
	vap = a->a_vap;
	node = VTON(vp);
	fentry = node->fentry;

	vap->va_fsid = dev2udev(node->i_dev);
	vap->va_fileid = node->hash_id;
	vap->va_mode = udf_permtomode(node);
	vap->va_nlink = fentry->link_cnt;
	/*
	 * XXX The spec says that -1 is valid for uid/gid and indicates an
	 * invalid uid/gid.  How should this be represented?
	 */
	vap->va_uid = (fentry->uid == -1) ? 0 : fentry->uid;
	vap->va_gid = (fentry->gid == -1) ? 0 : fentry->gid;
	udf_timetotimespec(&fentry->atime, &vap->va_atime);
	udf_timetotimespec(&fentry->mtime, &vap->va_mtime);
	vap->va_ctime = vap->va_mtime; /* XXX Stored as an Extended Attribute */
	vap->va_rdev = 0; /* XXX */
	if (vp->v_type & VDIR) {
		/*
		 * Directories that are recorded within their ICB will show
		 * as having 0 blocks recorded.  Since tradition dictates
		 * that directories consume at least one logical block,
		 * make it appear so.
		 */
		if (fentry->logblks_rec != 0) {
			vap->va_size = fentry->logblks_rec * node->udfmp->bsize;
		} else {
			vap->va_size = node->udfmp->bsize;
		}
	} else {
		vap->va_size = fentry->inf_len;
	}
	vap->va_flags = 0;
	vap->va_gen = 1;
	vap->va_blocksize = node->udfmp->bsize;
	vap->va_bytes = fentry->inf_len;
	vap->va_type = vp->v_type;
	vap->va_filerev = 0; /* XXX */
	return (0);
}

/*
 * File specific ioctls.  DeCSS candidate?
 */
static int
udf_ioctl(struct vop_ioctl_args *a)
{
	printf("%s called\n", __FUNCTION__);
	return (EOPNOTSUPP);
}

/*
 * I'm not sure that this has much value in a read-only filesystem, but
 * cd9660 has it too.
 */
static int
udf_pathconf(struct vop_pathconf_args *a)
{

	switch (a->a_name) {
	case _PC_LINK_MAX:
		*a->a_retval = 65535;
		return (0);
	case _PC_NAME_MAX:
		*a->a_retval = NAME_MAX;
		return (0);
	case _PC_PATH_MAX:
		*a->a_retval = PATH_MAX;
		return (0);
	case _PC_NO_TRUNC:
		*a->a_retval = 1;
		return (0);
	default:
		return (EINVAL);
	}
}

static int
udf_read(struct vop_read_args *a)
{
	struct vnode *vp = a->a_vp;
	struct uio *uio = a->a_uio;
	struct udf_node *node = VTON(vp);
	struct buf *bp;
	uint8_t *data;
	int error = 0;
	int size, fsize, offset;

	if (uio->uio_offset < 0)
		return (EINVAL);

	fsize = node->fentry->inf_len;

	while (uio->uio_offset < fsize && uio->uio_resid > 0) {
		offset = uio->uio_offset;
		size = uio->uio_resid;
		error = udf_readatoffset(node, &size, offset, &bp, &data);
		if (error)
			return (error);
		error = uiomove((caddr_t)data, size, uio);
		if (bp != NULL)
			brelse(bp);
		if (error)
			break;
	};

	return (error);
}

/* Convienience routine to dump a block in hex */
static void
udf_dumpblock(void *data, int len)
{
	int i, j;

	for (i = 0; i < len; i++) {
		printf("\noffset= %d: ", i);
		for (j = 0; j < 8; j++) {
			if (i + j == len)
				break;
			printf("0x%02x ", (uint8_t)((uint8_t*)(data))[i + j]);
		}
		i += j - 1;
	}
	printf("\n");
}

/*
 * Call the OSTA routines to translate the name from a CS0 dstring to a
 * 16-bit Unicode String.  Hooks need to be placed in here to translate from
 * Unicode to the encoding that the kernel/user expects.  For now, compact
 * the encoding to 8 bits if possible.  Return the length of the translated
 * string.
 * XXX This horribly pessimizes the 8bit case
 */
static int
udf_transname(char *cs0string, char *destname, int len)
{
	unicode_t *transname;
	int i, unilen = 0;

	/* allocate a buffer big enough to hold an 8->16 bit expansion */
	transname = uma_zalloc(udf_zone_trans, M_WAITOK);

	if ((unilen = udf_UncompressUnicode(len, cs0string, transname)) == -1) {
		printf("udf: Unicode translation failed\n");
		uma_zfree(udf_zone_trans, transname);
		return 0;
	}

	/* At this point, the name is in 16-bit Unicode.  Compact it down
 	 * to 8-bit
	 */
	for (i = 0; i < unilen ; i++) {
		if (transname[i] & 0xff00) {
			destname[i] = '.';	/* Fudge the 16bit chars */
		} else {
			destname[i] = transname[i] & 0xff;
		}
	}

	destname[unilen] = 0;
	uma_zfree(udf_zone_trans, transname);

	return unilen;
}

/*
 * Compare a CS0 dstring with a name passed in from the VFS layer.  Return
 * 0 on a successful match, nonzero therwise.  Unicode work may need to be done
 * here also.
 */
static int
udf_cmpname(char *cs0string, char *cmpname, int cs0len, int cmplen)
{
	char *transname;
	int error = 0;

	/* This is overkill, but not worth creating a new zone */
	transname = uma_zalloc(udf_zone_trans, M_WAITOK);

	cs0len = udf_transname(cs0string, transname, cs0len);

	/* Easy check.  If they aren't the same length, they aren't equal */
	if ((cs0len == 0) || (cs0len != cmplen))
		error = -1;
	else
		error = bcmp(transname, cmpname, cmplen);

	uma_zfree(udf_zone_trans, transname);
	return (error);
}

struct udf_uiodir {
	struct dirent *dirent;
	u_long *cookies;
	int ncookies;
	int acookies;
	int eofflag;
};

static int
udf_uiodir(struct udf_uiodir *uiodir, int de_size, struct uio *uio, long cookie)
{
	if (uiodir->cookies != NULL) {
		if (++uiodir->acookies > uiodir->ncookies) {
			uiodir->eofflag = 0;
			return (-1);
		}
		*uiodir->cookies++ = cookie;
	}

	if (uio->uio_resid < de_size) {
		uiodir->eofflag = 0;
		return (-1);
	}

	return (uiomove((caddr_t)uiodir->dirent, de_size, uio));
}

static struct udf_dirstream *
udf_opendir(struct udf_node *node, int offset, int fsize, struct udf_mnt *udfmp)
{
	struct udf_dirstream *ds;

	ds = uma_zalloc(udf_zone_ds, M_WAITOK | M_ZERO);

	ds->node = node;
	ds->offset = offset;
	ds->udfmp = udfmp;
	ds->fsize = fsize;

	return (ds);
}

static struct fileid_desc *
udf_getfid(struct udf_dirstream *ds)
{
	struct fileid_desc *fid;
	int error, frag_size = 0, total_fid_size;

	/* End of directory? */
	if (ds->offset + ds->off >= ds->fsize) {
		ds->error = 0;
		return (NULL);
	}

	/* Grab the first extent of the directory */
	if (ds->off == 0) {
		ds->size = 0;
		error = udf_readatoffset(ds->node, &ds->size, ds->offset,
		    &ds->bp, &ds->data);
		if (error) {
			ds->error = error;
			return (NULL);
		}
	}

	/*
	 * Clean up from a previous fragmented FID.
	 * XXX Is this the right place for this?
	 */
	if (ds->fid_fragment && ds->buf != NULL) {
		ds->fid_fragment = 0;
		FREE(ds->buf, M_UDFFID);
	}

	fid = (struct fileid_desc*)&ds->data[ds->off];

	/*
	 * Check to see if the fid is fragmented. The first test
	 * ensures that we don't wander off the end of the buffer
	 * looking for the l_iu and l_fi fields.
	 */
	if (ds->off + UDF_FID_SIZE > ds->size ||
	    ds->off + fid->l_iu + fid->l_fi + UDF_FID_SIZE > ds->size) {

		/* Copy what we have of the fid into a buffer */
		frag_size = ds->size - ds->off;
		if (frag_size >= ds->udfmp->bsize) {
			printf("udf: invalid FID fragment\n");
			ds->error = EINVAL;
			return (NULL);
		}

		/*
		 * File ID descriptors can only be at most one
		 * logical sector in size.
		 */
		MALLOC(ds->buf, uint8_t*, ds->udfmp->bsize, M_UDFFID,
		     M_WAITOK | M_ZERO);
		bcopy(fid, ds->buf, frag_size);

		/* Reduce all of the casting magic */
		fid = (struct fileid_desc*)ds->buf;

		if (ds->bp != NULL)
			brelse(ds->bp);

		/* Fetch the next allocation */
		ds->offset += ds->size;
		ds->size = 0;
		error = udf_readatoffset(ds->node, &ds->size, ds->offset,
		    &ds->bp, &ds->data);
		if (error) {
			ds->error = error;
			return (NULL);
		}

		/*
		 * If the fragment was so small that we didn't get
		 * the l_iu and l_fi fields, copy those in.
		 */
		if (frag_size < UDF_FID_SIZE)
			bcopy(ds->data, &ds->buf[frag_size],
			    UDF_FID_SIZE - frag_size);

		/*
		 * Now that we have enough of the fid to work with,
		 * copy in the rest of the fid from the new
		 * allocation.
		 */
		total_fid_size = UDF_FID_SIZE + fid->l_iu + fid->l_fi;
		if (total_fid_size > ds->udfmp->bsize) {
			printf("udf: invalid FID\n");
			ds->error = EIO;
			return (NULL);
		}
		bcopy(ds->data, &ds->buf[frag_size],
		    total_fid_size - frag_size);

		ds->fid_fragment = 1;
	} else {
		total_fid_size = fid->l_iu + fid->l_fi + UDF_FID_SIZE;
	}

	/*
	 * Update the offset. Align on a 4 byte boundary because the
	 * UDF spec says so.
	 */
	ds->this_off = ds->off;
	if (!ds->fid_fragment) {
		ds->off += (total_fid_size + 3) & ~0x03;
	} else {
		ds->off = (total_fid_size - frag_size + 3) & ~0x03;
	}

	return (fid);
}

static void
udf_closedir(struct udf_dirstream *ds)
{

	if (ds->bp != NULL)
		brelse(ds->bp);

	if (ds->fid_fragment && ds->buf != NULL)
		FREE(ds->buf, M_UDFFID);

	uma_zfree(udf_zone_ds, ds);
}

static int
udf_readdir(struct vop_readdir_args *a)
{
	struct vnode *vp;
	struct uio *uio;
	struct dirent dir;
	struct udf_node *node;
	struct fileid_desc *fid;
	struct udf_uiodir uiodir;
	struct udf_dirstream *ds;
	u_long *cookies = NULL;
	int ncookies;
	int error = 0;

	vp = a->a_vp;
	uio = a->a_uio;
	node = VTON(vp);
	uiodir.eofflag = 1;

	if (a->a_ncookies != NULL) {
		/*
		 * Guess how many entries are needed.  If we run out, this
		 * function will be called again and thing will pick up were
		 * it left off.
		 */
		ncookies = uio->uio_resid / 8;
		MALLOC(cookies, u_long *, sizeof(u_long) * ncookies,
		    M_TEMP, M_WAITOK);
		if (cookies == NULL)
			return (ENOMEM);
		uiodir.ncookies = ncookies;
		uiodir.cookies = cookies;
		uiodir.acookies = 0;
	} else {
		uiodir.cookies = NULL;
	}

	/*
	 * Iterate through the file id descriptors.  Give the parent dir
	 * entry special attention.
	 */
	ds = udf_opendir(node, uio->uio_offset, node->fentry->inf_len,
	    node->udfmp);

	while ((fid = udf_getfid(ds)) != NULL) {

		/* XXX Should we return an error on a bad fid? */
		if (udf_checktag(&fid->tag, TAGID_FID)) {
			printf("Invalid FID tag\n");
			udf_dumpblock(fid, UDF_FID_SIZE);
			error = EIO;
			break;
		}

		/* Is this a deleted file? */
		if (fid->file_char & UDF_FILE_CHAR_DEL)
			continue;

		if ((fid->l_fi == 0) && (fid->file_char & UDF_FILE_CHAR_PAR)) {
			/* Do up the '.' and '..' entries.  Dummy values are
			 * used for the cookies since the offset here is
			 * usually zero, and NFS doesn't like that value
			 */
			dir.d_fileno = node->hash_id;
			dir.d_type = DT_DIR;
			dir.d_name[0] = '.';
			dir.d_namlen = 1;
			dir.d_reclen = GENERIC_DIRSIZ(&dir);
			uiodir.dirent = &dir;
			error = udf_uiodir(&uiodir, dir.d_reclen, uio, 1); 
			if (error)
				break;

			dir.d_fileno = udf_getid(&fid->icb);
			dir.d_type = DT_DIR;
			dir.d_name[0] = '.';
			dir.d_name[1] = '.';
			dir.d_namlen = 2;
			dir.d_reclen = GENERIC_DIRSIZ(&dir);
			uiodir.dirent = &dir;
			error = udf_uiodir(&uiodir, dir.d_reclen, uio, 2);
		} else {
			dir.d_namlen = udf_transname(&fid->data[fid->l_iu],
			    &dir.d_name[0], fid->l_fi);
			dir.d_fileno = udf_getid(&fid->icb);
			dir.d_type = (fid->file_char & UDF_FILE_CHAR_DIR) ?
			    DT_DIR : DT_UNKNOWN;
			dir.d_reclen = GENERIC_DIRSIZ(&dir);
			uiodir.dirent = &dir;
			error = udf_uiodir(&uiodir, dir.d_reclen, uio,
			    ds->this_off);
		}
		if (error) {
			printf("uiomove returned %d\n", error);
			break;
		}

	}

	/* tell the calling layer whether we need to be called again */
	*a->a_eofflag = uiodir.eofflag;
	uio->uio_offset = ds->offset + ds->off;

	if (!error)
		error = ds->error;

	udf_closedir(ds);

	if (a->a_ncookies != NULL) {
		if (error)
			FREE(cookies, M_TEMP);
		else {
			*a->a_ncookies = uiodir.acookies;
			*a->a_cookies = cookies;
		}
	}

	return (error);
}

/* Are there any implementations out there that do soft-links? */
static int
udf_readlink(struct vop_readlink_args *ap)
{
	printf("%s called\n", __FUNCTION__);
	return (EOPNOTSUPP);
}

static int
udf_strategy(struct vop_strategy_args *a)
{
	struct buf *bp;
	struct vnode *vp;
	struct udf_node *node;
	int maxsize;

	bp = a->a_bp;
	vp = bp->b_vp;
	node = VTON(vp);

	/* cd9660 has this test reversed, but it seems more logical this way */
	if (bp->b_blkno != bp->b_lblkno) {
		/*
		 * Files that are embedded in the fentry don't translate well
		 * to a block number.  Reject.
		 */
		if (udf_bmap_internal(node, bp->b_lblkno * node->udfmp->bsize,
		    &bp->b_lblkno, &maxsize)) {
			clrbuf(bp);
			bp->b_blkno = -1;
		}
	}
	if ((long)bp->b_blkno == -1) {
		bufdone(bp);
		return (0);
	}
	vp = node->i_devvp;
	bp->b_dev = vp->v_rdev;
	VOP_STRATEGY(vp, bp);
	return (0);
}

static int
udf_print(struct vop_print_args *a)
{
	printf("%s called\n", __FUNCTION__);
	return (EOPNOTSUPP);
}

static int
udf_bmap(struct vop_bmap_args *a)
{
	struct udf_node *node;
	uint32_t max_size;
	daddr_t lsector;
	int error;

	node = VTON(a->a_vp);

	if (a->a_vpp != NULL)
		*a->a_vpp = node->i_devvp;
	if (a->a_bnp == NULL)
		return (0);
	if (a->a_runb)
		*a->a_runb = 0;

	error = udf_bmap_internal(node, a->a_bn * node->udfmp->bsize, &lsector,
	    &max_size);
	if (error)
		return (error);

	/* Translate logical to physical sector number */
	*a->a_bnp = lsector << (node->udfmp->bshift - DEV_BSHIFT);

	/* Punt on read-ahead for now */
	if (a->a_runp)
		*a->a_runp = 0;

	return (0);
}

/*
 * The all powerful VOP_LOOKUP().
 */
static int
udf_lookup(struct vop_cachedlookup_args *a)
{
	struct vnode *dvp;
	struct vnode *tdp = NULL;
	struct vnode **vpp = a->a_vpp;
	struct udf_node *node;
	struct udf_mnt *udfmp;
	struct fileid_desc *fid = NULL;
	struct udf_dirstream *ds;
	struct thread *td;
	u_long nameiop;
	u_long flags;
	char *nameptr;
	long namelen;
	ino_t id = 0;
	int offset, error = 0;
	int numdirpasses, fsize;

	dvp = a->a_dvp;
	node = VTON(dvp);
	udfmp = node->udfmp;
	nameiop = a->a_cnp->cn_nameiop;
	flags = a->a_cnp->cn_flags;
	nameptr = a->a_cnp->cn_nameptr;
	namelen = a->a_cnp->cn_namelen;
	fsize = node->fentry->inf_len;
	td = a->a_cnp->cn_thread;

	/*
	 * If this is a LOOKUP and we've already partially searched through
	 * the directory, pick up where we left off and flag that the
	 * directory may need to be searched twice.  For a full description,
	 * see /sys/isofs/cd9660/cd9660_lookup.c:cd9660_lookup()
	 */
	if (nameiop != LOOKUP || node->diroff == 0 || node->diroff > fsize) {
		offset = 0;
		numdirpasses = 1;
	} else {
		offset = node->diroff;
		numdirpasses = 2;
		nchstats.ncs_2passes++;
	}

lookloop:
	ds = udf_opendir(node, offset, fsize, udfmp);

	while ((fid = udf_getfid(ds)) != NULL) {

		/* XXX Should we return an error on a bad fid? */
		if (udf_checktag(&fid->tag, TAGID_FID)) {
			printf("udf_lookup: Invalid tag\n");
			error = EIO;
			break;
		}

		/* Is this a deleted file? */
		if (fid->file_char & UDF_FILE_CHAR_DEL)
			continue;

		if ((fid->l_fi == 0) && (fid->file_char & UDF_FILE_CHAR_PAR)) {
			if (flags & ISDOTDOT) {
				id = udf_getid(&fid->icb);
				break;
			}
		} else {
			if (!(udf_cmpname(&fid->data[fid->l_iu],
			    nameptr, fid->l_fi, namelen))) {
				id = udf_getid(&fid->icb);
				break;
			}
		}
	}

	if (!error)
		error = ds->error;

	/* XXX Bail out here? */
	if (error) {
		udf_closedir(ds);
		return (error);
	}

	/* Did we have a match? */
	if (id) {
		error = udf_vget(udfmp->im_mountp, id, LK_EXCLUSIVE, &tdp);
		if (!error) {
			/*
			 * Remember where this entry was if it's the final
			 * component.
			 */
			if ((flags & ISLASTCN) && nameiop == LOOKUP)
				node->diroff = ds->offset + ds->off;
			if (numdirpasses == 2)
				nchstats.ncs_pass2++;
			if (!(flags & LOCKPARENT) || !(flags & ISLASTCN)) {
				a->a_cnp->cn_flags |= PDIRUNLOCK;
				VOP_UNLOCK(dvp, 0, td);
			}

			*vpp = tdp;

			/* Put this entry in the cache */
			if (flags & MAKEENTRY)
				cache_enter(dvp, *vpp, a->a_cnp);
		}
	} else {
		/* Name wasn't found on this pass.  Do another pass? */
		if (numdirpasses == 2) {
			numdirpasses--;
			offset = 0;
			udf_closedir(ds);
			goto lookloop;
		}

		/* Enter name into cache as non-existant */
		if (flags & MAKEENTRY)
			cache_enter(dvp, *vpp, a->a_cnp);

		if ((flags & ISLASTCN) &&
		    (nameiop == CREATE || nameiop == RENAME)) {
			error = EROFS;
		} else {
			error = ENOENT;
		}
	}

	udf_closedir(ds);
	return (error);
}

static int
udf_reclaim(struct vop_reclaim_args *a)
{
	struct vnode *vp;
	struct udf_node *unode;

	vp = a->a_vp;
	unode = VTON(vp);

	cache_purge(vp);
	if (unode != NULL) {
		udf_hashrem(unode);
		if (unode->i_devvp) {
			vrele(unode->i_devvp);
			unode->i_devvp = 0;
		}

		if (unode->fentry != NULL)
			FREE(unode->fentry, M_UDFFENTRY);
		lockdestroy(&unode->i_vnode->v_lock);
		uma_zfree(udf_zone_node, unode);
		vp->v_data = NULL;
	}

	return (0);
}

/*
 * Read the block and then set the data pointer to correspond with the 
 * offset passed in.  Only read in at most 'size' bytes, and then set 'size'
 * to the number of bytes pointed to.  If 'size' is zero, try to read in a
 * whole extent.
 * XXX 'size' is limited to the logical block size for now due to problems
 * with udf_read()
 */
static int
udf_readatoffset(struct udf_node *node, int *size, int offset, struct buf **bp, uint8_t **data)
{
	struct udf_mnt *udfmp;
	struct file_entry *fentry = NULL;
	struct buf *bp1;
	uint32_t max_size;
	daddr_t sector;
	int error;

	udfmp = node->udfmp;

	error = udf_bmap_internal(node, offset, &sector, &max_size);
	if (error == UDF_INVALID_BMAP) {
		/*
		 * This error means that the file *data* is stored in the
		 * allocation descriptor field of the file entry.
		 */
		fentry = node->fentry;
		*data = &fentry->data[fentry->l_ea];
		*size = fentry->l_ad;
		*bp = NULL;
		return (0);
	} else if (error != 0) {
		return (error);
	}

	/* Adjust the size so that it is within range */
	if (*size == 0 || *size > max_size)
		*size = max_size;
	*size = min(*size, MAXBSIZE);

	if ((error = udf_readlblks(udfmp, sector, *size, bp))) {
		printf("udf_readlblks returned %d\n", error);
		return (error);
	}

	bp1 = *bp;
	*data = (uint8_t *)&bp1->b_data[offset % udfmp->bsize];
	return (0);
}

/*
 * Translate a file offset into a logical block and then into a physical
 * block.
 */
static int
udf_bmap_internal(struct udf_node *node, uint32_t offset, daddr_t *sector, uint32_t *max_size)
{
	struct udf_mnt *udfmp;
	struct file_entry *fentry;
	void *icb;
	struct icb_tag *tag;
	uint32_t icblen = 0;
	daddr_t lsector;
	int ad_offset, ad_num = 0;
	int i, p_offset;

	udfmp = node->udfmp;
	fentry = node->fentry;
	tag = &fentry->icbtag;

	switch (tag->strat_type) {
	case 4:
		break;

	case 4096:
		printf("Cannot deal with strategy4096 yet!\n");
		return (ENODEV);

	default:
		printf("Unknown strategy type %d\n", tag->strat_type);
		return (ENODEV);
	}

	switch (tag->flags & 0x7) {
	case 0:
		/*
		 * The allocation descriptor field is filled with short_ad's.
		 * If the offset is beyond the current extent, look for the
		 * next extent.
		 */
		do {
			offset -= icblen;
			ad_offset = sizeof(struct short_ad) * ad_num;
			if (ad_offset > fentry->l_ad) {
				printf("File offset out of bounds\n");
				return (EINVAL);
			}
			icb = GETICB(long_ad, fentry, fentry->l_ea + ad_offset);
			icblen = GETICBLEN(short_ad, icb);
			ad_num++;
		} while(offset >= icblen);

		lsector = (offset  >> udfmp->bshift) +
		    ((struct short_ad *)(icb))->pos;

		*max_size = GETICBLEN(short_ad, icb) - offset;

		break;
	case 1:
		/*
		 * The allocation descriptor field is filled with long_ad's
		 * If the offset is beyond the current extent, look for the
		 * next extent.
		 */
		do {
			offset -= icblen;
			ad_offset = sizeof(struct long_ad) * ad_num;
			if (ad_offset > fentry->l_ad) {
				printf("File offset out of bounds\n");
				return (EINVAL);
			}
			icb = GETICB(long_ad, fentry, fentry->l_ea + ad_offset);
			icblen = GETICBLEN(long_ad, icb);
			ad_num++;
		} while(offset >= icblen);

		lsector = (offset >> udfmp->bshift) + 
		    ((struct long_ad *)(icb))->loc.lb_num;

		*max_size = GETICBLEN(long_ad, icb) - offset;

		break;
	case 3:
		/*
		 * This type means that the file *data* is stored in the
		 * allocation descriptor field of the file entry.
		 */
		*max_size = 0;
		*sector = node->hash_id + udfmp->part_start;

		return (UDF_INVALID_BMAP);
	case 2:
		/* DirectCD does not use extended_ad's */
	default:
		printf("Unsupported allocation descriptor %d\n",
		       tag->flags & 0x7);
		return (ENODEV);
	}

	*sector = lsector + udfmp->part_start;

	/*
	 * Check the sparing table.  Each entry represents the beginning of
	 * a packet.
	 */
	if (udfmp->s_table != NULL) {
		for (i = 0; i< udfmp->s_table_entries; i++) {
			p_offset = lsector - udfmp->s_table->entries[i].org;
			if ((p_offset < udfmp->p_sectors) && (p_offset >= 0)) {
				*sector = udfmp->s_table->entries[i].map +
				    p_offset;
				break;
			}
		}
	}

	return (0);
}
