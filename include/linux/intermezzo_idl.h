/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 *  Copyright (C) 2001, 2002 Cluster File Systems, Inc.
 *  Copyright (C) 2001 Tacit Networks, Inc.
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
 */

#ifndef __INTERMEZZO_IDL_H__
#define __INTERMEZZO_IDL_H__

#include <linux/ioctl.h>
#include <linux/types.h>

/* this file contains all data structures used in InterMezzo's interfaces:
 * - upcalls
 * - ioctl's
 * - KML records
 * - RCVD records
 * - rpc's
 */ 

/* UPCALL */
#define INTERMEZZO_MINOR 248   


#define IZO_UPC_VERSION 0x00010002
#define IZO_UPC_PERMIT        1
#define IZO_UPC_CONNECT       2
#define IZO_UPC_GO_FETCH_KML  3
#define IZO_UPC_OPEN          4
#define IZO_UPC_REVOKE_PERMIT 5
#define IZO_UPC_KML           6
#define IZO_UPC_BACKFETCH     7
#define IZO_UPC_KML_TRUNC     8
#define IZO_UPC_SET_KMLSIZE   9
#define IZO_UPC_BRANCH_UNDO   10
#define IZO_UPC_BRANCH_REDO   11
#define IZO_UPC_GET_FILEID    12
#define IZO_UPC_CLIENT_MAKE_BRANCH    13
#define IZO_UPC_SERVER_MAKE_BRANCH    14
#define IZO_UPC_REPSTATUS    15

#define IZO_UPC_LARGEST_OPCODE 15

struct izo_upcall_hdr {
        __u32 u_len;
        __u32 u_version;
        __u32 u_opc;
        __u32 u_uniq;
        __u32 u_pid;
        __u32 u_uid;
        __u32 u_pathlen;
        __u32 u_fsetlen;
        __u64 u_offset;
        __u64 u_length;
        __u32 u_first_recno;
        __u32 u_last_recno;
        __u32 u_async;
        __u32 u_reclen;
        __u8  u_uuid[16];
};

/* This structure _must_ sit at the beginning of the buffer */
struct izo_upcall_resp {
        __u32 opcode;
        __u32 unique;    
        __u32 result;
};


/* IOCTL */

#define IZO_IOCTL_VERSION 0x00010003

/* maximum size supported for ioc_pbuf1 */
#define KML_MAX_BUF (64*1024)

struct izo_ioctl_hdr { 
        __u32  ioc_len;
        __u32  ioc_version;
};

struct izo_ioctl_data {
        __u32 ioc_len;
        __u32 ioc_version;
        __u32 ioc_izodev;
        __u32 ioc_kmlrecno;
        __u64 ioc_kmlsize;
        __u32 ioc_flags;
        __s32 ioc_inofd;
        __u64 ioc_ino;
        __u64 ioc_generation;
        __u32 ioc_mark_what;
        __u32 ioc_and_flag;
        __u32 ioc_or_flag;
        __u32 ioc_dev;
        __u32 ioc_offset;
        __u32 ioc_slot;
        __u64 ioc_uid;
        __u8  ioc_uuid[16];

        __u32 ioc_inllen1;   /* path */
        char *ioc_inlbuf1;
        __u32 ioc_inllen2;   /* fileset */
        char *ioc_inlbuf2;

        __u32 ioc_plen1;     /* buffers in user space (KML) */
        char *ioc_pbuf1;
        __u32 ioc_plen2;     /* buffers in user space (KML) */
        char *ioc_pbuf2;

        char  ioc_bulk[0];
};

#define IZO_IOC_DEVICE          _IOW ('p',0x50, void *)
#define IZO_IOC_REINTKML        _IOW ('p',0x51, void *)
#define IZO_IOC_GET_RCVD        _IOW ('p',0x52, void *)
#define IZO_IOC_SET_IOCTL_UID   _IOW ('p',0x53, void *)
#define IZO_IOC_GET_KML_SIZE    _IOW ('p',0x54, void *)
#define IZO_IOC_PURGE_FILE_DATA _IOW ('p',0x55, void *)
#define IZO_IOC_CONNECT         _IOW ('p',0x56, void *)
#define IZO_IOC_GO_FETCH_KML    _IOW ('p',0x57, void *)
#define IZO_IOC_MARK            _IOW ('p',0x58, void *)
#define IZO_IOC_CLEAR_FSET      _IOW ('p',0x59, void *)
#define IZO_IOC_CLEAR_ALL_FSETS _IOW ('p',0x60, void *)
#define IZO_IOC_SET_FSET        _IOW ('p',0x61, void *)
#define IZO_IOC_REVOKE_PERMIT   _IOW ('p',0x62, void *)
#define IZO_IOC_SET_KMLSIZE     _IOW ('p',0x63, void *)
#define IZO_IOC_CLIENT_MAKE_BRANCH _IOW ('p',0x64, void *)
#define IZO_IOC_SERVER_MAKE_BRANCH _IOW ('p',0x65, void *)
#define IZO_IOC_BRANCH_UNDO    _IOW ('p',0x66, void *)
#define IZO_IOC_BRANCH_REDO    _IOW ('p',0x67, void *)
#define IZO_IOC_SET_PID        _IOW ('p',0x68, void *)
#define IZO_IOC_SET_CHANNEL    _IOW ('p',0x69, void *)
#define IZO_IOC_GET_CHANNEL    _IOW ('p',0x70, void *)
#define IZO_IOC_GET_FILEID    _IOW ('p',0x71, void *)
#define IZO_IOC_ADJUST_LML    _IOW ('p',0x72, void *)
#define IZO_IOC_SET_FILEID    _IOW ('p',0x73, void *)
#define IZO_IOC_REPSTATUS    _IOW ('p',0x74, void *)

/* marking flags for fsets */
#define FSET_CLIENT_RO        0x00000001
#define FSET_LENTO_RO         0x00000002
#define FSET_HASPERMIT        0x00000004 /* we have a permit to WB */
#define FSET_INSYNC           0x00000008 /* this fileset is in sync */
#define FSET_PERMIT_WAITING   0x00000010 /* Lento is waiting for permit */
#define FSET_STEAL_PERMIT     0x00000020 /* take permit if Lento is dead */
#define FSET_JCLOSE_ON_WRITE  0x00000040 /* Journal closes on writes */
#define FSET_DATA_ON_DEMAND   0x00000080 /* update data on file_open() */
#define FSET_PERMIT_EXCLUSIVE 0x00000100 /* only one permitholder allowed */
#define FSET_HAS_BRANCHES     0x00000200 /* this fileset contains branches */
#define FSET_IS_BRANCH        0x00000400 /* this fileset is a branch */
#define FSET_FLAT_BRANCH      0x00000800 /* this fileset is ROOT with branches */

/* what to mark indicator (ioctl parameter) */
#define MARK_DENTRY   101
#define MARK_FSET     102
#define MARK_CACHE    103
#define MARK_GETFL    104

/* KML */

#define KML_MAJOR_VERSION 0x00010000
#define KML_MINOR_VERSION 0x00000002
#define KML_OPCODE_NOOP          0
#define KML_OPCODE_CREATE        1
#define KML_OPCODE_MKDIR         2
#define KML_OPCODE_UNLINK        3
#define KML_OPCODE_RMDIR         4
#define KML_OPCODE_CLOSE         5
#define KML_OPCODE_SYMLINK       6
#define KML_OPCODE_RENAME        7
#define KML_OPCODE_SETATTR       8
#define KML_OPCODE_LINK          9
#define KML_OPCODE_OPEN          10
#define KML_OPCODE_MKNOD         11
#define KML_OPCODE_WRITE         12
#define KML_OPCODE_RELEASE       13
#define KML_OPCODE_TRUNC         14
#define KML_OPCODE_SETEXTATTR    15
#define KML_OPCODE_DELEXTATTR    16
#define KML_OPCODE_KML_TRUNC     17
#define KML_OPCODE_GET_FILEID    18
#define KML_OPCODE_NUM           19
/* new stuff */
struct presto_version {
        __u64 pv_mtime;
        __u64 pv_ctime;
        __u64 pv_size;
};

struct kml_prefix_hdr {
        __u32                    len;
        __u32                    version;
        __u32                    pid;
        __u32                    auid;
        __u32                    fsuid;
        __u32                    fsgid;
        __u32                    opcode;
        __u32                    ngroups;
};

struct kml_prefix { 
        struct kml_prefix_hdr    *hdr;
        __u32                    *groups;
};

struct kml_suffix { 
        __u32                    prevrec;
        __u32                    recno;
        __u32                    time;
        __u32                    len;
};

struct kml_rec {
        char                   *buf;
        struct kml_prefix       prefix;
        __u64                   offset;
        char                   *path;
        int                     pathlen;
        char                   *name;
        int                     namelen;
        char                   *target;
        int                     targetlen;
        struct presto_version  *old_objectv;
        struct presto_version  *new_objectv;
        struct presto_version  *old_parentv;
        struct presto_version  *new_parentv;
        struct presto_version  *old_targetv;
        struct presto_version  *new_targetv;
        __u32                   valid;
        __u32                   mode;
        __u32                   uid;
        __u32                   gid;
        __u64                   size;
        __u32                   mtime;
        __u32                   ctime;
        __u32                   flags;
        __u32                   ino;
        __u32                   rdev;
        __u32                   major;
        __u32                   minor;
        __u32                   generation;
        __u32                   old_mode;
        __u32                   old_rdev;
        __u64                   old_uid;
        __u64                   old_gid;
        char                   *old_target;
        int                     old_targetlen;
        struct kml_suffix      *suffix;
};


/* RCVD */ 

/* izo_rcvd_rec fills the .intermezzo/fset/last_rcvd file and provides data about
 * our view of reintegration offsets for a given peer.
 *
 * The only exception is the last_rcvd record which has a UUID consisting of all
 * zeroes; this record's lr_local_offset field is the logical byte offset of our
 * KML, which is updated when KML truncation takes place.  All other fields are
 * reserved. */

/* XXX - document how clean shutdowns are recorded */

struct izo_rcvd_rec { 
        __u8    lr_uuid[16];       /* which peer? */
        __u64   lr_remote_recno;   /* last confirmed remote recno  */
        __u64   lr_remote_offset;  /* last confirmed remote offset */
        __u64   lr_local_recno;    /* last locally reinted recno   */
        __u64   lr_local_offset;   /* last locally reinted offset  */
        __u64   lr_last_ctime;     /* the largest ctime that has reintegrated */
};

/* Cache purge database
 *
 * Each DB entry is this structure followed by the path name, no trailing NUL. */
struct izo_purge_entry {
        __u64 p_atime;
        __u32 p_pathlen;
};

/* RPC */

#endif
