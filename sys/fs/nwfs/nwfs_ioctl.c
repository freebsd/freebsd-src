/*-
 * Copyright (c) 1999, Boris Popov
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Boris Popov.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/ioccom.h>

#include <netncp/ncp.h>
#include <netncp/ncp_conn.h>
#include <netncp/ncp_subr.h>

#include <fs/nwfs/nwfs.h>
#include <fs/nwfs/nwfs_node.h>
#include <fs/nwfs/nwfs_subr.h>

int
nwfs_ioctl(ap)
	struct vop_ioctl_args /* {
		struct vnode *a_vp;
		u_long a_command;
		caddr_t a_data;
		int fflag;
		struct ucred *cred;
		struct thread *td;
	} */ *ap;
{
	int error;
	struct thread *td = ap->a_td;
	struct ucred *cred = ap->a_cred;
	struct vnode *vp = ap->a_vp;
	struct nwnode *np = VTONW(vp);
	struct nwmount *nmp = VTONWFS(vp);
	struct ncp_conn *conn = NWFSTOCONN(nmp);
	struct ncp_handle *hp;
	struct nw_entry_info *fap;
	void *data = ap->a_data;

	switch (ap->a_command) {
	    case NWFSIOC_GETCONN:
		error = ncp_conn_lock(conn, td, cred, NCPM_READ);
		if (error) break;
		error = ncp_conn_gethandle(conn, td, &hp);
		ncp_conn_unlock(conn, td);
		if (error) break;
		*(int*)data = hp->nh_id;
		break;
	    case NWFSIOC_GETEINFO:
		if ((error = VOP_ACCESS(vp, VEXEC, cred, td))) break;
		fap = data;
		error = ncp_obtain_info(nmp, np->n_fid.f_id, 0, NULL, fap,
		    ap->a_td,ap->a_cred);
		strcpy(fap->entryName, np->n_name);
		fap->nameLen = np->n_nmlen;
		break;
	    case NWFSIOC_GETNS:
		if ((error = VOP_ACCESS(vp, VEXEC, cred, td))) break;
		*(int*)data = nmp->name_space;
		break;
	    default:
		error = ENOTTY;
	}
	return (error);
}
