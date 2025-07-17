/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2008 Isilon Inc http://www.isilon.com/
 * Copyright (c) 2013 Spectra Logic Corporation
 *
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
 */

#ifndef	_NFS_FHA_NEW_H
#define	_NFS_FHA_NEW_H 1

#ifdef	_KERNEL

#define	FHANEW_SERVER_NAME	"nfsd"

/* Sysctl defaults. */
#define FHA_DEF_ENABLE			1
#define FHA_DEF_READ			1
#define FHA_DEF_WRITE			1
#define FHA_DEF_BIN_SHIFT		22 /* 4MB */
#define FHA_DEF_MAX_NFSDS_PER_FH	8
#define FHA_DEF_MAX_REQS_PER_NFSD	0  /* Unlimited */

#define FHA_HASH_SIZE	251

struct fha_ctls {
	int	 enable;
	int	 read;
	int	 write;
	uint32_t bin_shift;
	uint32_t max_nfsds_per_fh;
	uint32_t max_reqs_per_nfsd;
};

/*
 * These are the entries in the filehandle hash.  They talk about a specific
 * file, requests against which are being handled by one or more nfsds.  We
 * keep a chain of nfsds against the file. We only have more than one if reads
 * are ongoing, and then only if the reads affect disparate regions of the
 * file.
 *
 * In general, we want to assign a new request to an existing nfsd if it is
 * going to contend with work happening already on that nfsd, or if the
 * operation is a read and the nfsd is already handling a proximate read.  We
 * do this to avoid jumping around in the read stream unnecessarily, and to
 * avoid contention between threads over single files.
 */
struct fha_hash_entry {
	struct mtx *mtx;
	LIST_ENTRY(fha_hash_entry) link;
	u_int64_t fh;
	u_int32_t num_rw;
	u_int32_t num_exclusive;
	u_int8_t num_threads;
	struct svcthread_list threads;
};

LIST_HEAD(fha_hash_entry_list, fha_hash_entry);

struct fha_hash_slot {
	struct fha_hash_entry_list list;
	struct mtx mtx;
};

/* A structure used for passing around data internally. */
struct fha_info {
	u_int64_t fh;
	off_t offset;
	int locktype;
	int read;
	int write;
};

struct fha_params {
	struct fha_hash_slot fha_hash[FHA_HASH_SIZE];
	char server_name[32];
};

SVCTHREAD *fhanew_assign(SVCTHREAD *this_thread, struct svc_req *req);
void fhanew_nd_complete(SVCTHREAD *, struct svc_req *);
#endif /* _KERNEL */

#endif /* _NFS_FHA_NEW_H */
