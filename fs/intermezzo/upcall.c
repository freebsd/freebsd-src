/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 * Copyright (C) 2001, 2002 Cluster File Systems, Inc. <braam@clusterfs.com>
 * Copyright (C) 2001 Tacit Networks, Inc. <phil@off.net>
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
 * Mostly platform independent upcall operations to a cache manager:
 *  -- upcalls
 *  -- upcall routines
 *
 */

#include <asm/system.h>
#include <asm/segment.h>
#include <asm/signal.h>
#include <linux/signal.h>

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/stat.h>
#include <linux/errno.h>
#include <linux/locks.h>
#include <linux/string.h>
#include <asm/uaccess.h>
#include <linux/vmalloc.h>
#include <asm/segment.h>

#include <linux/intermezzo_lib.h>
#include <linux/intermezzo_fs.h>
#include <linux/intermezzo_psdev.h>

#include <linux/intermezzo_idl.h>

/*
  At present:
  -- Asynchronous calls:
   - kml:            give a "more" kml indication to userland
   - kml_truncate:   initiate KML truncation
   - release_permit: kernel is done with permit
  -- Synchronous
   - open:           fetch file
   - permit:         get a permit

  Errors returned by user level code are positive

 */

static struct izo_upcall_hdr *upc_pack(__u32 opcode, int pathlen, char *path,
                                       char *fsetname, int reclen, char *rec,
                                       int *size)
{
        struct izo_upcall_hdr *hdr;
        char *ptr;
        ENTRY;

        *size = sizeof(struct izo_upcall_hdr);
        if ( fsetname ) {
                *size += round_strlen(fsetname);
        }
        if ( path ) { 
                *size += round_strlen(path);
        }
        if ( rec ) { 
                *size += size_round(reclen);
        }
        PRESTO_ALLOC(hdr, *size);
        if (!hdr) { 
                CERROR("intermezzo upcall: out of memory (opc %d)\n", opcode);
                EXIT;
                return NULL;
        }
        memset(hdr, 0, *size);

        ptr = (char *)hdr + sizeof(*hdr);

        /* XXX do we need fsuid ? */
        hdr->u_len = *size;
        hdr->u_version = IZO_UPC_VERSION;
        hdr->u_opc = opcode;
        hdr->u_pid = current->pid;
        hdr->u_uid = current->fsuid;

        if (path) { 
                /*XXX Robert: please review what len to pass in for 
                  NUL terminated strings */
                hdr->u_pathlen = strlen(path);
                LOGL0(path, hdr->u_pathlen, ptr);
        }
        if (fsetname) { 
                hdr->u_fsetlen = strlen(fsetname);
                LOGL0(fsetname, strlen(fsetname), ptr);
        }
        if (rec) { 
                hdr->u_reclen = reclen;
                LOGL(rec, reclen, ptr);
        }
        
        EXIT;
        return hdr;
}

/* the upcalls */
int izo_upc_kml(int minor, __u64 offset, __u32 first_recno, __u64 length, __u32 last_recno, char *fsetname)
{
        int size;
        int error;
        struct izo_upcall_hdr *hdr;

        ENTRY;
        if (!presto_lento_up(minor)) {
                EXIT;
                return 0;
        }

        hdr = upc_pack(IZO_UPC_KML, 0, NULL, fsetname, 0, NULL, &size);
        if (!hdr || IS_ERR(hdr)) {
                EXIT;
                return -PTR_ERR(hdr);
        }

        hdr->u_offset = offset;
        hdr->u_first_recno = first_recno;
        hdr->u_length = length;
        hdr->u_last_recno = last_recno;

        CDEBUG(D_UPCALL, "KML: fileset %s, offset %Lu, length %Lu, "
               "first %u, last %d; minor %d\n",
               fsetname, hdr->u_offset, hdr->u_length, hdr->u_first_recno,
               hdr->u_last_recno, minor);

        error = izo_upc_upcall(minor, &size, hdr, ASYNCHRONOUS);

        EXIT;
        return -error;
}

int izo_upc_kml_truncate(int minor, __u64 length, __u32 last_recno, char *fsetname)
{
        int size;
        int error;
        struct izo_upcall_hdr *hdr;

        ENTRY;
        if (!presto_lento_up(minor)) {
                EXIT;
                return 0;
        }

        hdr = upc_pack(IZO_UPC_KML_TRUNC, 0, NULL, fsetname, 0, NULL, &size);
        if (!hdr || IS_ERR(hdr)) {
                EXIT;
                return -PTR_ERR(hdr);
        }

        hdr->u_length = length;
        hdr->u_last_recno = last_recno;

        CDEBUG(D_UPCALL, "KML TRUNCATE: fileset %s, length %Lu, "
               "last recno %d, minor %d\n",
               fsetname, hdr->u_length, hdr->u_last_recno, minor);

        error = izo_upc_upcall(minor, &size, hdr, ASYNCHRONOUS);

        EXIT;
        return error;
}

int izo_upc_open(int minor, __u32 pathlen, char *path, char *fsetname, struct lento_vfs_context *info)
{
        int size;
        int error;
        struct izo_upcall_hdr *hdr;
        ENTRY;

        if (!presto_lento_up(minor)) {
                EXIT;
                return -EIO;
        }

        hdr = upc_pack(IZO_UPC_OPEN, pathlen, path, fsetname, 
                       sizeof(*info), (char*)info, &size);
        if (!hdr || IS_ERR(hdr)) {
                EXIT;
                return -PTR_ERR(hdr);
        }

        CDEBUG(D_UPCALL, "path %s\n", path);

        error = izo_upc_upcall(minor, &size, hdr, SYNCHRONOUS);
        if (error)
                CERROR("InterMezzo: %s: error %d\n", __FUNCTION__, error);

        EXIT;
        return -error;
}

int izo_upc_get_fileid(int minor, __u32 reclen, char *rec, 
                       __u32 pathlen, char *path, char *fsetname)
{
        int size;
        int error;
        struct izo_upcall_hdr *hdr;
        ENTRY;

        if (!presto_lento_up(minor)) {
                EXIT;
                return -EIO;
        }

        hdr = upc_pack(IZO_UPC_GET_FILEID, pathlen, path, fsetname, reclen, rec, &size);
        if (!hdr || IS_ERR(hdr)) {
                EXIT;
                return -PTR_ERR(hdr);
        }

        CDEBUG(D_UPCALL, "path %s\n", path);

        error = izo_upc_upcall(minor, &size, hdr, SYNCHRONOUS);
        if (error)
                CERROR("InterMezzo: %s: error %d\n", __FUNCTION__, error);

        EXIT;
        return -error;
}

int izo_upc_backfetch(int minor, char *path, char *fsetname, struct lento_vfs_context *info)
{
        int size;
        int error;
        struct izo_upcall_hdr *hdr;
        ENTRY;

        if (!presto_lento_up(minor)) {
                EXIT;
                return -EIO;
        }

        hdr = upc_pack(IZO_UPC_BACKFETCH, strlen(path), path, fsetname, 
                       sizeof(*info), (char *)info, &size);
        if (!hdr || IS_ERR(hdr)) {
                EXIT;
                return -PTR_ERR(hdr);
        }

        /* This is currently synchronous, kml_reint_record blocks */
        error = izo_upc_upcall(minor, &size, hdr, SYNCHRONOUS);
        if (error)
                CERROR("InterMezzo: %s: error %d\n", __FUNCTION__, error);

        EXIT;
        return -error;
}

int izo_upc_permit(int minor, struct dentry *dentry, __u32 pathlen, char *path,
                   char *fsetname)
{
        int size;
        int error;
        struct izo_upcall_hdr *hdr;

        ENTRY;

        hdr = upc_pack(IZO_UPC_PERMIT, pathlen, path, fsetname, 0, NULL, &size);
        if (!hdr || IS_ERR(hdr)) {
                EXIT;
                return -PTR_ERR(hdr);
        }

        CDEBUG(D_UPCALL, "Permit minor %d path %s\n", minor, path);

        error = izo_upc_upcall(minor, &size, hdr, SYNCHRONOUS);

        if (error == -EROFS) {
                int err;
                CERROR("InterMezzo: ERROR - requested permit for read-only "
                       "fileset.\n   Setting \"%s\" read-only!\n", path);
                err = izo_mark_cache(dentry, 0xFFFFFFFF, CACHE_CLIENT_RO, NULL);
                if (err)
                        CERROR("InterMezzo ERROR: mark_cache %d\n", err);
        } else if (error) {
                CERROR("InterMezzo: %s: error %d\n", __FUNCTION__, error);
        }

        EXIT;
        return error;
}

/* This is a ping-pong upcall handled on the server when a client (uuid)
 * requests the permit for itself. */
int izo_upc_revoke_permit(int minor, char *fsetname, __u8 uuid[16])
{
        int size;
        int error;
        struct izo_upcall_hdr *hdr;

        ENTRY;

        hdr = upc_pack(IZO_UPC_REVOKE_PERMIT, 0, NULL, fsetname, 0, NULL, &size);
        if (!hdr || IS_ERR(hdr)) {
                EXIT;
                return -PTR_ERR(hdr);
        }

        memcpy(hdr->u_uuid, uuid, sizeof(hdr->u_uuid));

        error = izo_upc_upcall(minor, &size, hdr, SYNCHRONOUS);

        if (error)
                CERROR("InterMezzo: %s: error %d\n", __FUNCTION__, error);

        EXIT;
        return -error;
}

int izo_upc_go_fetch_kml(int minor, char *fsetname, __u8 uuid[16],
                         __u64 kmlsize)
{
        int size;
        int error;
        struct izo_upcall_hdr *hdr;
        ENTRY;

        if (!presto_lento_up(minor)) {
                EXIT;
                return -EIO;
        }

        hdr = upc_pack(IZO_UPC_GO_FETCH_KML, 0, NULL, fsetname, 0, NULL, &size);
        if (!hdr || IS_ERR(hdr)) {
                EXIT;
                return -PTR_ERR(hdr);
        }

        hdr->u_offset = kmlsize;
        memcpy(hdr->u_uuid, uuid, sizeof(hdr->u_uuid));

        error = izo_upc_upcall(minor, &size, hdr, ASYNCHRONOUS);
        if (error)
                CERROR("%s: error %d\n", __FUNCTION__, error);

        EXIT;
        return -error;
}

int izo_upc_connect(int minor, __u64 ip_address, __u64 port, __u8 uuid[16],
                    int client_flag)
{
        int size;
        int error;
        struct izo_upcall_hdr *hdr;
        ENTRY;

        if (!presto_lento_up(minor)) {
                EXIT;
                return -EIO;
        }

        hdr = upc_pack(IZO_UPC_CONNECT, 0, NULL, NULL, 0, NULL, &size);
        if (!hdr || IS_ERR(hdr)) {
                EXIT;
                return -PTR_ERR(hdr);
        }

        hdr->u_offset = ip_address;
        hdr->u_length = port;
        memcpy(hdr->u_uuid, uuid, sizeof(hdr->u_uuid));
        hdr->u_first_recno = client_flag;

        error = izo_upc_upcall(minor, &size, hdr, SYNCHRONOUS);
        if (error) {
                CERROR("%s: error %d\n", __FUNCTION__, error);
        }

        EXIT;
        return -error;
}

int izo_upc_set_kmlsize(int minor, char *fsetname, __u8 uuid[16], __u64 kmlsize)
{
        int size;
        int error;
        struct izo_upcall_hdr *hdr;
        ENTRY;

        if (!presto_lento_up(minor)) {
                EXIT;
                return -EIO;
        }

        hdr = upc_pack(IZO_UPC_SET_KMLSIZE, 0, NULL, fsetname, 0, NULL, &size);
        if (!hdr || IS_ERR(hdr)) {
                EXIT;
                return -PTR_ERR(hdr);
        }

        memcpy(hdr->u_uuid, uuid, sizeof(hdr->u_uuid));
        hdr->u_length = kmlsize;

        error = izo_upc_upcall(minor, &size, hdr, SYNCHRONOUS);
        if (error)
                CERROR("%s: error %d\n", __FUNCTION__, error);

        EXIT;
        return -error;
}

int izo_upc_repstatus(int minor,  char * fsetname, struct izo_rcvd_rec *lr_server)
{
        int size;
        int error;
        struct izo_upcall_hdr *hdr;
        ENTRY;

        if (!presto_lento_up(minor)) {
                EXIT;
                return -EIO;
        }

        hdr = upc_pack(IZO_UPC_REPSTATUS, 0, NULL, fsetname, 
                       sizeof(*lr_server), (char*)lr_server, 
                       &size);
        if (!hdr || IS_ERR(hdr)) {
                EXIT;
                return -PTR_ERR(hdr);
        }

        error = izo_upc_upcall(minor, &size, hdr, SYNCHRONOUS);
        if (error)
                CERROR("%s: error %d\n", __FUNCTION__, error);

        EXIT;
        return -error;
}


#if 0
int izo_upc_client_make_branch(int minor, char *fsetname, char *tagname,
                               char *branchname)
{
        int size, error;
        struct izo_upcall_hdr *hdr;
        int pathlen;
        char *path;
        ENTRY;

        hdr = upc_pack(IZO_UPC_CLIENT_MAKE_BRANCH, strlen(tagname), tagname,
                       fsetname, strlen(branchname) + 1, branchname, &size);
        if (!hdr || IS_ERR(hdr)) {
                error = -PTR_ERR(hdr);
                goto error;
        }

        error = izo_upc_upcall(minor, &size, hdr, SYNCHRONOUS);
        if (error)
                CERROR("InterMezzo: error %d\n", error);

 error:
        PRESTO_FREE(path, pathlen);
        EXIT;
        return error;
}
#endif

int izo_upc_server_make_branch(int minor, char *fsetname)
{
        int size, error;
        struct izo_upcall_hdr *hdr;
        ENTRY;

        hdr = upc_pack(IZO_UPC_SERVER_MAKE_BRANCH, 0, NULL, fsetname, 0, NULL, &size);
        if (!hdr || IS_ERR(hdr)) {
                error = -PTR_ERR(hdr);
                goto error;
        }

        error = izo_upc_upcall(minor, &size, hdr, SYNCHRONOUS);
        if (error)
                CERROR("InterMezzo: error %d\n", error);

 error:
        EXIT;
        return -error;
}

int izo_upc_branch_undo(int minor, char *fsetname, char *branchname)
{
        int size;
        int error;
        struct izo_upcall_hdr *hdr;
        ENTRY;

        if (!presto_lento_up(minor)) {
                EXIT;
                return -EIO;
        }

        hdr = upc_pack(IZO_UPC_BRANCH_UNDO, strlen(branchname), branchname,
                       fsetname, 0, NULL, &size);
        if (!hdr || IS_ERR(hdr)) {
                EXIT;
                return -PTR_ERR(hdr);
        }

        error = izo_upc_upcall(minor, &size, hdr, SYNCHRONOUS);
        if (error)
                CERROR("InterMezzo: %s: error %d\n", __FUNCTION__, error);

        EXIT;
        return -error;
}

int izo_upc_branch_redo(int minor, char *fsetname, char *branchname)
{
        int size;
        int error;
        struct izo_upcall_hdr *hdr;
        ENTRY;

        if (!presto_lento_up(minor)) {
                EXIT;
                return -EIO;
        }

        hdr = upc_pack(IZO_UPC_BRANCH_REDO, strlen(branchname) + 1, branchname,
                       fsetname, 0, NULL, &size);
        if (!hdr || IS_ERR(hdr)) {
                EXIT;
                return -PTR_ERR(hdr);
        }

        error = izo_upc_upcall(minor, &size, hdr, SYNCHRONOUS);
        if (error)
                CERROR("InterMezzo: %s: error %d\n", __FUNCTION__, error);

        EXIT;
        return -error;
}
