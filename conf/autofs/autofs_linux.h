/*
 * Copyright (c) 1999-2003 Ion Badulescu
 * Copyright (c) 1997-2014 Erez Zadok
 * Copyright (c) 1990 Jan-Simon Pendry
 * Copyright (c) 1990 Imperial College of Science, Technology & Medicine
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry at Imperial College, London.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 *
 *
 * File: am-utils/conf/autofs/autofs_linux.h
 *
 */

/*
 * Automounter filesystem headers for Linux
 */

#if !defined(HAVE_LINUX_AUTO_FS_H) && !defined(HAVE_LINUX_AUTO_FS4_H)
/* We didn't find the headers, so we can't compile in the autofs support */
# undef HAVE_FS_AUTOFS
# undef MNTTAB_TYPE_AUTOFS
#endif /* !HAVE_LINUX_AUTO_FS_H && !HAVE_LINUX_AUTO_FS4_H */

#ifdef HAVE_FS_AUTOFS

struct autofs_pending_mount {
  unsigned long wait_queue_token;	/* Associated kernel wait token */
  char *name;
  struct autofs_pending_mount *next;
};

struct autofs_pending_umount {
  unsigned long wait_queue_token;	/* Associated kernel wait token */
  char *name;
  struct autofs_pending_umount *next;
};

typedef struct {
  int fd;
  int kernelfd;
  int ioctlfd;
  int version;
  struct autofs_pending_mount *pending_mounts;
  struct autofs_pending_umount *pending_umounts;
} autofs_fh_t;

#ifndef HAVE_LINUX_AUTO_FS4_H
union autofs_packet_union {
	struct autofs_packet_hdr hdr;
	struct autofs_packet_missing missing;
	struct autofs_packet_expire expire;
};

/* typedef unsigned long autofs_wqt_t; */
#endif /* not HAVE_LINUX_AUTO_FS4_H */

#define AUTOFS_AUTO_FS_FLAGS	(FS_AMQINFO | FS_DIRECTORY | FS_AUTOFS | FS_ON_AUTOFS)
#define AUTOFS_DIRECT_FS_FLAGS	(FS_DIRECT | FS_MKMNT | FS_NOTIMEOUT | FS_BACKGROUND | FS_AMQINFO)
#define AUTOFS_ERROR_FS_FLAGS	(FS_DISCARD)
#define AUTOFS_HOST_FS_FLAGS	(FS_MKMNT | FS_BACKGROUND | FS_AMQINFO)
#define AUTOFS_LINK_FS_FLAGS	(FS_MBACKGROUND)
#define AUTOFS_LINKX_FS_FLAGS	(FS_MBACKGROUND)
#define AUTOFS_NFSL_FS_FLAGS	(FS_BACKGROUND | FS_AMQINFO | FS_ON_AUTOFS)
#define AUTOFS_NFSX_FS_FLAGS	(/* FS_UBACKGROUND| */ FS_AMQINFO )
#define AUTOFS_PROGRAM_FS_FLAGS	(FS_BACKGROUND | FS_AMQINFO)
#define AUTOFS_ROOT_FS_FLAGS	(FS_NOTIMEOUT | FS_AMQINFO | FS_DIRECTORY)
#define AUTOFS_TOPLVL_FS_FLAGS	(FS_MKMNT | FS_NOTIMEOUT | FS_BACKGROUND | FS_AMQINFO | FS_DIRECTORY | FS_AUTOFS)
#define AUTOFS_UNION_FS_FLAGS	(FS_NOTIMEOUT | FS_BACKGROUND | FS_AMQINFO | FS_DIRECTORY | FS_ON_AUTOFS)

#define AUTOFS_CACHEFS_FS_FLAGS	(FS_NOTIMEOUT | FS_UBACKGROUND | FS_AMQINFO | FS_ON_AUTOFS)
#define AUTOFS_CDFS_FS_FLAGS	(FS_UBACKGROUND | FS_AMQINFO | FS_ON_AUTOFS)
#define AUTOFS_UDF_FS_FLAGS	(FS_UBACKGROUND | FS_AMQINFO | FS_ON_AUTOFS)
#define AUTOFS_LUSTRE_FS_FLAGS	(FS_UBACKGROUND | FS_AMQINFO | FS_ON_AUTOFS)
#define AUTOFS_EFS_FS_FLAGS	(FS_NOTIMEOUT | FS_UBACKGROUND | FS_AMQINFO | FS_ON_AUTOFS)
#define AUTOFS_LOFS_FS_FLAGS	(FS_NOTIMEOUT | FS_UBACKGROUND | FS_AMQINFO | FS_ON_AUTOFS)
#define AUTOFS_NFS_FS_FLAGS	(FS_BACKGROUND | FS_AMQINFO | FS_ON_AUTOFS)
#define AUTOFS_PCFS_FS_FLAGS	(FS_UBACKGROUND | FS_AMQINFO | FS_ON_AUTOFS)
#define AUTOFS_UFS_FS_FLAGS	(FS_NOTIMEOUT | FS_UBACKGROUND | FS_AMQINFO | FS_ON_AUTOFS)
#define AUTOFS_XFS_FS_FLAGS	(FS_NOTIMEOUT | FS_UBACKGROUND | FS_AMQINFO | FS_ON_AUTOFS)
#define AUTOFS_EXT_FS_FLAGS	(FS_NOTIMEOUT | FS_UBACKGROUND | FS_AMQINFO | FS_ON_AUTOFS)
#define AUTOFS_TMPFS_FS_FLAGS	(FS_NOTIMEOUT | FS_UBACKGROUND | FS_AMQINFO | FS_ON_AUTOFS)
#endif /* HAVE_FS_AUTOFS */
