/*-
 * Copyright (c) 2001 Dag-Erling Coïdan Smørgrav
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *      $FreeBSD$
 */

#ifndef _PSEUDOFS_INTERNAL_H_INCLUDED
#define _PSEUDOFS_INTERNAL_H_INCLUDED

/*
 * Sysctl subtree
 */
SYSCTL_DECL(_vfs_pfs);

/*
 * Vnode data
 */
struct pfs_vdata {
	struct pfs_node	*pvd_pn;
	pid_t		 pvd_pid;
	struct vnode	*pvd_vnode;
	struct pfs_vdata*pvd_prev, *pvd_next;
};

/*
 * Vnode cache
 */
void	 pfs_vncache_load	(void);
void	 pfs_vncache_unload	(void);
int	 pfs_vncache_alloc	(struct mount *, struct vnode **,
				 struct pfs_node *, pid_t pid);
int	 pfs_vncache_free	(struct vnode *);

/*
 * File number bitmap
 */
void	 pfs_fileno_load	(void);
void	 pfs_fileno_unload	(void);
void	 pfs_fileno_init	(struct pfs_info *);
void	 pfs_fileno_uninit	(struct pfs_info *);
void	 pfs_fileno_alloc	(struct pfs_info *, struct pfs_node *);
void	 pfs_fileno_free	(struct pfs_info *, struct pfs_node *);

#endif
