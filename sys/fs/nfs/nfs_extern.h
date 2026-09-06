/*
 * Copyright (c) 2026 Rick Macklem
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef	_NFS_EXTERN_H_
#define	_NFS_EXTERN_H_

#ifdef _KERNEL
/* Definitions for NFS that might be used elsewhere in the kernel. */
void nfsrv_svc_reg(SVCXPRT *xprt);

extern int	nfsrvd_rdma_port;
extern int	newnfs_numnfsd;

#endif	/* _KERNEL */

#endif	/* _NFS_EXTERN_H_ */
