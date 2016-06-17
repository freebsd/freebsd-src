/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 *  Copyright (C) 2001 Cluster File Systems, Inc. <braam@clusterfs.com>
 *
 *   This file is part of InterMezzo, http://www.inter-mezzo.org.
 *
 *   InterMezzo is free software; you can redistribute it and/or
 *   modify it under the terms of version 2 of the GNU General Public
 *   License as published by the Free Software Foundation.
 *
 *   InterMezzo is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with InterMezzo; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Unpacking of KML records
 *
 */

#ifdef __KERNEL__
#  include <linux/module.h>
#  include <linux/errno.h>
#  include <linux/kernel.h>
#  include <linux/major.h>
#  include <linux/sched.h>
#  include <linux/lp.h>
#  include <linux/slab.h>
#  include <linux/ioport.h>
#  include <linux/fcntl.h>
#  include <linux/delay.h>
#  include <linux/skbuff.h>
#  include <linux/proc_fs.h>
#  include <linux/vmalloc.h>
#  include <linux/fs.h>
#  include <linux/poll.h>
#  include <linux/init.h>
#  include <linux/list.h>
#  include <linux/stat.h>
#  include <asm/io.h>
#  include <asm/segment.h>
#  include <asm/system.h>
#  include <asm/poll.h>
#  include <asm/uaccess.h>
#else
#  include <time.h>
#  include <stdio.h>
#  include <string.h>
#  include <stdlib.h>
#  include <errno.h>
#  include <sys/stat.h>
#  include <glib.h>
#endif

#include <linux/intermezzo_lib.h>
#include <linux/intermezzo_idl.h>
#include <linux/intermezzo_fs.h>

int kml_unpack_version(struct presto_version **ver, char **buf, char *end) 
{
	char *ptr = *buf;
        struct presto_version *pv;

	UNLOGP(*ver, struct presto_version, ptr, end);
        pv = *ver;
        pv->pv_mtime   = NTOH__u64(pv->pv_mtime);
        pv->pv_ctime   = NTOH__u64(pv->pv_ctime);
        pv->pv_size    = NTOH__u64(pv->pv_size);

	*buf = ptr;

        return 0;
}


static int kml_unpack_noop(struct kml_rec *rec, char **buf, char *end)
{
	return 0;
}

 
static int kml_unpack_get_fileid(struct kml_rec *rec, char **buf, char *end)
{
	char *ptr = *buf;

	LUNLOGV(rec->pathlen, __u32, ptr, end);
	UNLOGL(rec->path, char, rec->pathlen, ptr, end);

	*buf = ptr;
	return 0;
}

static int kml_unpack_create(struct kml_rec *rec, char **buf, char *end)
{
	char *ptr = *buf;

	kml_unpack_version(&rec->old_parentv, &ptr, end);
	kml_unpack_version(&rec->new_parentv, &ptr, end);
	kml_unpack_version(&rec->new_objectv, &ptr, end);
	LUNLOGV(rec->mode, __u32, ptr, end);
	LUNLOGV(rec->uid, __u32, ptr, end);
	LUNLOGV(rec->gid, __u32, ptr, end);
	LUNLOGV(rec->pathlen, __u32, ptr, end);
	UNLOGL(rec->path, char, rec->pathlen, ptr, end);

	*buf = ptr;

	return 0;
}

 
static int kml_unpack_mkdir(struct kml_rec *rec, char **buf, char *end)
{
	char *ptr = *buf;

	kml_unpack_version(&rec->old_parentv, &ptr, end);
	kml_unpack_version(&rec->new_parentv, &ptr, end);
	kml_unpack_version(&rec->new_objectv, &ptr, end);
	LUNLOGV(rec->mode, __u32, ptr, end);
	LUNLOGV(rec->uid, __u32, ptr, end);
	LUNLOGV(rec->gid, __u32, ptr, end);
	LUNLOGV(rec->pathlen, __u32, ptr, end);
	UNLOGL(rec->path, char, rec->pathlen, ptr, end);

	*buf = ptr;

	return 0;
}


static int kml_unpack_unlink(struct kml_rec *rec, char **buf, char *end)
{
	char *ptr = *buf;

	kml_unpack_version(&rec->old_parentv, &ptr, end);
	kml_unpack_version(&rec->new_parentv, &ptr, end);
	kml_unpack_version(&rec->old_objectv, &ptr, end);
        LUNLOGV(rec->old_mode, __u32, ptr, end);
        LUNLOGV(rec->old_rdev, __u32, ptr, end);
        LUNLOGV(rec->old_uid, __u64, ptr, end);
        LUNLOGV(rec->old_gid, __u64, ptr, end);
	LUNLOGV(rec->pathlen, __u32, ptr, end);
	LUNLOGV(rec->targetlen, __u32, ptr, end);
        LUNLOGV(rec->old_targetlen, __u32, ptr, end);
	UNLOGL(rec->path, char, rec->pathlen, ptr, end);
	UNLOGL(rec->target, char, rec->targetlen, ptr, end);
        UNLOGL(rec->old_target, char, rec->old_targetlen, ptr, end);

	*buf = ptr;

	return 0;
}


static int kml_unpack_rmdir(struct kml_rec *rec, char **buf, char *end)
{
	char *ptr = *buf;

	kml_unpack_version(&rec->old_parentv, &ptr, end);
	kml_unpack_version(&rec->new_parentv, &ptr, end);
	kml_unpack_version(&rec->old_objectv, &ptr, end);
        LUNLOGV(rec->old_mode, __u32, ptr, end);
        LUNLOGV(rec->old_rdev, __u32, ptr, end);
        LUNLOGV(rec->old_uid, __u64, ptr, end);
        LUNLOGV(rec->old_gid, __u64, ptr, end);
	LUNLOGV(rec->pathlen, __u32, ptr, end);
	LUNLOGV(rec->targetlen, __u32, ptr, end);
	UNLOGL(rec->path, char, rec->pathlen, ptr, end);
	UNLOGL(rec->target, char, rec->targetlen, ptr, end);

	*buf = ptr;

	return 0;
}


static int kml_unpack_close(struct kml_rec *rec, char **buf, char *end)
{
	char *ptr = *buf;

	LUNLOGV(rec->mode, __u32, ptr, end);  // used for open_mode
	LUNLOGV(rec->uid, __u32, ptr, end);   // used for open_uid
	LUNLOGV(rec->gid, __u32, ptr, end);   // used for open_gid
	kml_unpack_version(&rec->old_objectv, &ptr, end);
	kml_unpack_version(&rec->new_objectv, &ptr, end);
	LUNLOGV(rec->ino, __u64, ptr, end);
	LUNLOGV(rec->generation, __u32, ptr, end);
	LUNLOGV(rec->pathlen, __u32, ptr, end);
	UNLOGL(rec->path, char, rec->pathlen, ptr, end);

	*buf = ptr;

	return 0;
}


static int kml_unpack_symlink(struct kml_rec *rec, char **buf, char *end)
{
	char *ptr = *buf;

	kml_unpack_version(&rec->old_parentv, &ptr, end);
	kml_unpack_version(&rec->new_parentv, &ptr, end);
	kml_unpack_version(&rec->new_objectv, &ptr, end);
	LUNLOGV(rec->uid, __u32, ptr, end);
	LUNLOGV(rec->gid, __u32, ptr, end);
	LUNLOGV(rec->pathlen, __u32, ptr, end);
	LUNLOGV(rec->targetlen, __u32, ptr, end);
	UNLOGL(rec->path, char, rec->pathlen, ptr, end);
	UNLOGL(rec->target, char, rec->targetlen, ptr, end);

	*buf = ptr;

	return 0;
}


static int kml_unpack_rename(struct kml_rec *rec, char **buf, char *end)
{
	char *ptr = *buf;

	kml_unpack_version(&rec->old_objectv, &ptr, end);
	kml_unpack_version(&rec->new_objectv, &ptr, end);
	kml_unpack_version(&rec->old_parentv, &ptr, end);
	kml_unpack_version(&rec->new_parentv, &ptr, end);
	LUNLOGV(rec->pathlen, __u32, ptr, end);
	LUNLOGV(rec->targetlen, __u32, ptr, end);
	UNLOGL(rec->path, char, rec->pathlen, ptr, end);
	UNLOGL(rec->target, char, rec->targetlen, ptr, end);

	*buf = ptr;

	return 0;
}


static int kml_unpack_setattr(struct kml_rec *rec, char **buf, char *end)
{
	char *ptr = *buf;

	kml_unpack_version(&rec->old_objectv, &ptr, end);
	LUNLOGV(rec->valid, __u32, ptr, end);
	LUNLOGV(rec->mode, __u32, ptr, end);
	LUNLOGV(rec->uid, __u32, ptr, end);
	LUNLOGV(rec->gid, __u32, ptr, end);
	LUNLOGV(rec->size, __u64, ptr, end);
	LUNLOGV(rec->mtime, __u64, ptr, end);
	LUNLOGV(rec->ctime, __u64, ptr, end);
	LUNLOGV(rec->flags, __u32, ptr, end);
        LUNLOGV(rec->old_mode, __u32, ptr, end);
        LUNLOGV(rec->old_rdev, __u32, ptr, end);
        LUNLOGV(rec->old_uid, __u64, ptr, end);
        LUNLOGV(rec->old_gid, __u64, ptr, end);
	LUNLOGV(rec->pathlen, __u32, ptr, end);
	UNLOGL(rec->path, char, rec->pathlen, ptr, end);
	
	*buf = ptr;

	return 0;
}


static int kml_unpack_link(struct kml_rec *rec, char **buf, char *end)
{
	char *ptr = *buf;

	kml_unpack_version(&rec->old_parentv, &ptr, end);
	kml_unpack_version(&rec->new_parentv, &ptr, end);
	kml_unpack_version(&rec->new_objectv, &ptr, end);
	LUNLOGV(rec->pathlen, __u32, ptr, end);
	LUNLOGV(rec->targetlen, __u32, ptr, end);
	UNLOGL(rec->path, char, rec->pathlen, ptr, end);
	UNLOGL(rec->target, char, rec->targetlen, ptr, end);

	*buf = ptr;

	return 0;
}

static int kml_unpack_mknod(struct kml_rec *rec, char **buf, char *end)
{
	char *ptr = *buf;

	kml_unpack_version(&rec->old_parentv, &ptr, end);
	kml_unpack_version(&rec->new_parentv, &ptr, end);
	kml_unpack_version(&rec->new_objectv, &ptr, end);
	LUNLOGV(rec->mode, __u32, ptr, end);
	LUNLOGV(rec->uid, __u32, ptr, end);
	LUNLOGV(rec->gid, __u32, ptr, end);
	LUNLOGV(rec->major, __u32, ptr, end);
	LUNLOGV(rec->minor, __u32, ptr, end);
	LUNLOGV(rec->pathlen, __u32, ptr, end);
	UNLOGL(rec->path, char, rec->pathlen, ptr, end);

	*buf = ptr;

	return 0;
}


static int kml_unpack_write(struct kml_rec *rec, char **buf, char *end)
{
	printf("NOT IMPLEMENTED");
	return 0;
}


static int kml_unpack_release(struct kml_rec *rec, char **buf, char *end)
{
	printf("NOT IMPLEMENTED");
	return 0;
}


static int kml_unpack_trunc(struct kml_rec *rec, char **buf, char *end)
{
	printf("NOT IMPLEMENTED");
	return 0;
}


static int kml_unpack_setextattr(struct kml_rec *rec, char **buf, char *end)
{
	char *ptr = *buf;

	kml_unpack_version(&rec->old_objectv, &ptr, end);
	kml_unpack_version(&rec->new_objectv, &ptr, end);
	LUNLOGV(rec->flags, __u32, ptr, end);
	LUNLOGV(rec->mode, __u32, ptr, end);
	LUNLOGV(rec->pathlen, __u32, ptr, end);
	LUNLOGV(rec->namelen, __u32, ptr, end);
	LUNLOGV(rec->targetlen, __u32, ptr, end);
        UNLOGL(rec->path, char, rec->pathlen, ptr, end);
	UNLOGL(rec->name, char, rec->namelen, ptr, end);
	UNLOGL(rec->target, char, rec->targetlen, ptr, end);

	*buf = ptr;

	return 0;
}


static int kml_unpack_delextattr(struct kml_rec *rec, char **buf, char *end)
{
	char *ptr = *buf;

	kml_unpack_version(&rec->old_objectv, &ptr, end);
	kml_unpack_version(&rec->new_objectv, &ptr, end);
	LUNLOGV(rec->flags, __u32, ptr, end);
	LUNLOGV(rec->mode, __u32, ptr, end);
	LUNLOGV(rec->pathlen, __u32, ptr, end);
	LUNLOGV(rec->namelen, __u32, ptr, end);
	LUNLOGV(rec->targetlen, __u32, ptr, end);
	UNLOGL(rec->path, char, rec->pathlen, ptr, end);
	UNLOGL(rec->name, char, rec->namelen, ptr, end);

	*buf = ptr;

	return 0;
}

static int kml_unpack_open(struct kml_rec *rec, char **buf, char *end)
{
	printf("NOT IMPLEMENTED");
	return 0;
}

static int kml_unpack_kml_trunc(struct kml_rec *rec, char **buf, char *end)
{

	printf("NOT IMPLEMENTED");
	return 0;
}


typedef int (*unpacker)(struct kml_rec *rec, char **buf, char *end);

static unpacker unpackers[KML_OPCODE_NUM] = 
{
	[KML_OPCODE_NOOP] = kml_unpack_noop,
	[KML_OPCODE_CREATE] = kml_unpack_create, 
	[KML_OPCODE_MKDIR] = kml_unpack_mkdir,
	[KML_OPCODE_UNLINK] = kml_unpack_unlink,
	[KML_OPCODE_RMDIR] = kml_unpack_rmdir,
	[KML_OPCODE_CLOSE] = kml_unpack_close,
	[KML_OPCODE_SYMLINK] = kml_unpack_symlink,
	[KML_OPCODE_RENAME] = kml_unpack_rename,
	[KML_OPCODE_SETATTR] = kml_unpack_setattr,
	[KML_OPCODE_LINK] = kml_unpack_link,
	[KML_OPCODE_OPEN] = kml_unpack_open,
	[KML_OPCODE_MKNOD] = kml_unpack_mknod,
	[KML_OPCODE_WRITE] = kml_unpack_write,
	[KML_OPCODE_RELEASE] = kml_unpack_release,
	[KML_OPCODE_TRUNC] = kml_unpack_trunc,
	[KML_OPCODE_SETEXTATTR] = kml_unpack_setextattr,
	[KML_OPCODE_DELEXTATTR] = kml_unpack_delextattr,
	[KML_OPCODE_KML_TRUNC] = kml_unpack_kml_trunc,
	[KML_OPCODE_GET_FILEID] = kml_unpack_get_fileid
};

int kml_unpack_prefix(struct kml_rec *rec, char **buf, char *end) 
{
	char *ptr = *buf;
        int n;

        UNLOGP(rec->prefix.hdr, struct kml_prefix_hdr, ptr, end);
        rec->prefix.hdr->len     = NTOH__u32(rec->prefix.hdr->len);
        rec->prefix.hdr->version = NTOH__u32(rec->prefix.hdr->version);
        rec->prefix.hdr->pid     = NTOH__u32(rec->prefix.hdr->pid);
        rec->prefix.hdr->auid    = NTOH__u32(rec->prefix.hdr->auid);
        rec->prefix.hdr->fsuid   = NTOH__u32(rec->prefix.hdr->fsuid);
        rec->prefix.hdr->fsgid   = NTOH__u32(rec->prefix.hdr->fsgid);
        rec->prefix.hdr->opcode  = NTOH__u32(rec->prefix.hdr->opcode);
        rec->prefix.hdr->ngroups = NTOH__u32(rec->prefix.hdr->ngroups);

	UNLOGL(rec->prefix.groups, __u32, rec->prefix.hdr->ngroups, ptr, end);
        for (n = 0; n < rec->prefix.hdr->ngroups; n++) {
                rec->prefix.groups[n] = NTOH__u32(rec->prefix.groups[n]);
        }

	*buf = ptr;

        return 0;
}

int kml_unpack_suffix(struct kml_rec *rec, char **buf, char *end) 
{
	char *ptr = *buf;

	UNLOGP(rec->suffix, struct kml_suffix, ptr, end);
        rec->suffix->prevrec   = NTOH__u32(rec->suffix->prevrec);
        rec->suffix->recno    = NTOH__u32(rec->suffix->recno);
        rec->suffix->time     = NTOH__u32(rec->suffix->time);
        rec->suffix->len      = NTOH__u32(rec->suffix->len);

	*buf = ptr;

        return 0;
}

int kml_unpack(struct kml_rec *rec, char **buf, char *end)
{
	char *ptr = *buf;
	int err; 

        if (((unsigned long)ptr % 4) != 0) {
                printf("InterMezzo: %s: record misaligned.\n", __FUNCTION__);
                return -EINVAL;
        }

        while (ptr < end) { 
                __u32 *i = (__u32 *)ptr;
                if (*i)
                        break;
                ptr += sizeof(*i);
        }
	*buf = ptr;

	memset(rec, 0, sizeof(*rec));

        err = kml_unpack_prefix(rec, &ptr, end);
	if (err) {
                printf("InterMezzo: %s: unpack_prefix failed: %d\n",
                       __FUNCTION__, err);
		return err;
        }

        if (rec->prefix.hdr->opcode < 0  ||
            rec->prefix.hdr->opcode >= KML_OPCODE_NUM) {
                printf("InterMezzo: %s: invalid opcode (%d)\n",
                       __FUNCTION__, rec->prefix.hdr->opcode);
		return -EINVAL;
        }
	err = unpackers[rec->prefix.hdr->opcode](rec, &ptr, end);
	if (err) {
                printf("InterMezzo: %s: unpacker failed: %d\n",
                       __FUNCTION__, err);
		return err;
        }

        err = kml_unpack_suffix(rec, &ptr, end);
	if (err) {
                printf("InterMezzo: %s: unpack_suffix failed: %d\n",
                       __FUNCTION__, err);
		return err;
        }


	if (rec->prefix.hdr->len != rec->suffix->len) {
                printf("InterMezzo: %s: lengths don't match\n",
                       __FUNCTION__);
		return -EINVAL;
        }
        if ((rec->prefix.hdr->len % 4) != 0) {
                printf("InterMezzo: %s: record length not a "
                       "multiple of 4.\n", __FUNCTION__);
                return -EINVAL;
        }
        if (ptr - *buf != rec->prefix.hdr->len) {
                printf("InterMezzo: %s: unpacking error\n",
                       __FUNCTION__);
                return -EINVAL;
        }
        while (ptr < end) { 
                __u32 *i = (__u32 *)ptr;
                if (*i)
                        break;
                ptr += sizeof(*i);
        }
	*buf = ptr;
	return 0;
}


#ifndef __KERNEL__
#define STR(ptr) ((ptr))? (ptr) : ""

#define OPNAME(n) [KML_OPCODE_##n] = #n
static char *opnames[KML_OPCODE_NUM] = {
	OPNAME(NOOP),
	OPNAME(CREATE),
	OPNAME(MKDIR), 
	OPNAME(UNLINK),
	OPNAME(RMDIR),
	OPNAME(CLOSE),
	OPNAME(SYMLINK),
	OPNAME(RENAME),
	OPNAME(SETATTR),
	OPNAME(LINK),
	OPNAME(OPEN),
	OPNAME(MKNOD),
	OPNAME(WRITE),
	OPNAME(RELEASE),
	OPNAME(TRUNC),
	OPNAME(SETEXTATTR),
	OPNAME(DELEXTATTR),
	OPNAME(KML_TRUNC),
	OPNAME(GET_FILEID)
};
#undef OPNAME

static char *print_opname(int op)
{
	if (op < 0 || op >= sizeof (opnames) / sizeof (*opnames))
		return NULL;
	return opnames[op];
}


static char *print_time(__u64 i)
{
	char buf[128];
	
	memset(buf, 0, 128);

#ifndef __KERNEL__
	strftime(buf, 128, "%Y/%m/%d %H:%M:%S", gmtime((time_t *)&i));
#else
	sprintf(buf, "%Ld\n", i);
#endif

	return strdup(buf);
}

static char *print_version(struct presto_version *ver)
{
	char ver_buf[128];
	char *mtime;
	char *ctime;

	if (!ver || ver->pv_ctime == 0) {
		return strdup("");
	} 
	mtime = print_time(ver->pv_mtime);
	ctime = print_time(ver->pv_ctime);
	sprintf(ver_buf, "mtime %s, ctime %s, len %lld", 
		mtime, ctime, ver->pv_size);
	free(mtime);
	free(ctime);
	return strdup(ver_buf);
}


char *kml_print_rec(struct kml_rec *rec, int brief)
{
	char *str;
	char *nov, *oov, *ntv, *otv, *npv, *opv;
	char *rectime, *mtime, *ctime;

        if (brief) {
		str = g_strdup_printf(" %08d %7s %*s %*s", 
                                      rec->suffix->recno,
                                      print_opname (rec->prefix.hdr->opcode),
                                      rec->pathlen, STR(rec->path),
                                      rec->targetlen, STR(rec->target));
                
		return str;
	}

	rectime = print_time(rec->suffix->time);
	mtime = print_time(rec->mtime);
	ctime = print_time(rec->ctime);

	nov = print_version(rec->new_objectv);
	oov = print_version(rec->old_objectv);
	ntv = print_version(rec->new_targetv);
	otv = print_version(rec->old_targetv);
	npv = print_version(rec->new_parentv);
	opv = print_version(rec->old_parentv);

	str = g_strdup_printf("\n -- Record:\n"
		"    Recno     %d\n"
		"    KML off   %lld\n" 
		"    Version   %d\n" 
		"    Len       %d\n"
		"    Suf len   %d\n"
		"    Time      %s\n"
		"    Opcode    %d\n"
		"    Op        %s\n"
		"    Pid       %d\n"
		"    AUid      %d\n"
		"    Fsuid     %d\n" 
		"    Fsgid     %d\n"
		"    Prevrec   %d\n" 
		"    Ngroups   %d\n"
		//"    Groups    @{$self->{groups}}\n" 
		" -- Path:\n"
		"    Inode     %d\n"
		"    Gen num   %u\n"
                "    Old mode  %o\n"
                "    Old rdev  %x\n"
                "    Old uid   %llu\n"
                "    Old gid   %llu\n"
		"    Path      %*s\n"
		//"    Open_mode %o\n",
		"    Pathlen   %d\n"
		"    Tgt       %*s\n"
		"    Tgtlen    %d\n" 
		"    Old Tgt   %*s\n"
		"    Old Tgtln %d\n" 
		" -- Attr:\n"
		"    Valid     %x\n"
		"    mode %o, uid %d, gid %d, size %lld, mtime %s, ctime %s rdev %x (%d:%d)\n"
		" -- Versions:\n"
		"    New object %s\n"
		"    Old object %s\n"
		"    New target %s\n"
		"    Old target %s\n"
		"    New parent %s\n"
		"    Old parent %s\n", 
		
		rec->suffix->recno, 
		rec->offset, 
		rec->prefix.hdr->version, 
		rec->prefix.hdr->len, 
		rec->suffix->len, 
		rectime,
		rec->prefix.hdr->opcode, 
		print_opname (rec->prefix.hdr->opcode),
		rec->prefix.hdr->pid,
		rec->prefix.hdr->auid,
		rec->prefix.hdr->fsuid,
		rec->prefix.hdr->fsgid,
		rec->suffix->prevrec,
		rec->prefix.hdr->ngroups,
		rec->ino,
		rec->generation,
                rec->old_mode,
                rec->old_rdev,
                rec->old_uid,
                rec->old_gid,
		rec->pathlen,
		STR(rec->path),
		rec->pathlen,
		rec->targetlen,
		STR(rec->target),
		rec->targetlen,
		rec->old_targetlen,
		STR(rec->old_target),
		rec->old_targetlen,
		
		rec->valid, 
		rec->mode,
		rec->uid,
		rec->gid,
		rec->size,
		mtime,
		ctime,
		rec->rdev, rec->major, rec->minor,
		nov, oov, ntv, otv, npv, opv);
		
	free(nov);
	free(oov);
	free(ntv);
	free(otv);
	free(npv);
	free(opv);

	free(rectime); 
	free(ctime);
	free(mtime);

	return str;
}
#endif
