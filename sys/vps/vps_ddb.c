/*-
 * Copyright (c) 2009-2013 Klaus P. Ohrhallinger <k@7he.at>
 * All rights reserved.
 *
 * Development of this software was partly funded by:
 *    TransIP.nl <http://www.transip.nl/>
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

/* $Id: vps_ddb.c 180 2013-06-14 16:52:13Z klaus $ */

#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_global.h"
#include "opt_ddb.h"

#ifdef VPS
#ifdef DDB

#include <sys/cdefs.h>

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/refcount.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/types.h>

#include <net/bpf.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_clone.h>
#include <net/if_var.h>
#include <net/if_types.h>
#include <net/netisr.h>
#include <net/route.h>
#include <net/vnet.h>

#include <netinet/in.h>
#include <netinet/ip.h>

#include "vps_user.h"
#include "vps.h"
#include "vps2.h"

static int vps_ddb_refcnt;

static const char *
vps_statusstr(int status)
{
	switch (status) {
	case VPS_ST_CREATING:
		return ("creating");
	case VPS_ST_RUNNING:
		return ("running");
	case VPS_ST_SUSPENDED:
		return ("suspended");
	case VPS_ST_SNAPSHOOTING:
		return ("snapshooting");
	case VPS_ST_RESTORING:
		return ("restoring");
	case VPS_ST_DYING:
		return ("dying");
	case VPS_ST_DEAD:
		return ("dead");
	default:
		return ("unknown");
	}
}

#ifdef DDB
#include <ddb/ddb.h>
#include <ddb/db_command.h>
#include <ddb/db_sym.h>

DB_SHOW_COMMAND(vps, db_show_vps)
{
	db_expr_t decaddr;
	struct vps *vps, *vps2;
	struct proc *p;
	int i;
#ifdef INVARIANTS
	struct vps_ref *ref;
#endif

	if (!have_addr) {
		/* Summary list of all vps instances. */
		db_printf("idx       addr     [name]             parent\n");
		i = 0;
		LIST_FOREACH(vps, &vps_head, vps_all) {
			db_printf("%d %8p [%16s] %8p\n",
				i, vps, vps->vps_name, vps->vps_parent);
			i++;
		}
		db_printf("show vps <addr/index>\n");
	} else {
		decaddr = db_hex2dec(addr);
		if (decaddr == -1) {
			vps = (struct vps *)addr;
		} else {
			vps = NULL;
			i = 0;
			LIST_FOREACH(vps2, &vps_head, vps_all) {
				if (i == decaddr) {
					vps = vps2;
					break;
				}
				i++;
			}
			if (vps == NULL)
				return;
		}

		/* Detailed view of one instance. */
		db_printf("vps:             %p\n", vps);
		db_printf("name:            [%s]\n", vps->vps_name);
		db_printf("parent:          %p\n", vps->vps_parent);
		db_printf("status:          %s\n",
		    vps_statusstr(vps->vps_status));
		db_printf("vnet:            %p\n", vps->vnet);
		db_printf("nprocs:          %d\n", VPS_VPS(vps, nprocs));
		db_printf("nprocs_zomb:     %d\n",
		    VPS_VPS(vps, nprocs_zomb));
		db_printf("sockets:         %d\n", vps->vnet->vnet_sockcnt);
		db_printf("refcnt:          %d\n", vps->vps_refcnt);
		db_printf("restore_count:   %d\n", vps->restore_count);
		db_printf("processes: \n");
		LIST_FOREACH(p, &VPS_VPS(vps, allproc), p_list) {
			db_printf("    p=%p pid=%d p_state=%d\n",
				p, p->p_pid, p->p_state);
		}
		db_printf("zombie processes: \n");
		LIST_FOREACH(p, &VPS_VPS(vps, zombproc), p_list) {
			db_printf("    p=%p pid=%d p_state=%d\n",
				p, p->p_pid, p->p_state);
		}
#ifdef INVARIANTS
		db_printf("references: \n");
		TAILQ_FOREACH(ref, &vps->vps_ref_head, list) {
			db_printf("    ref=%p arg=%p ticks=%zu\n",
				ref, ref->arg, (size_t)ref->ticks);
		}
#endif
	}
}

/*
 * For changing the current thread's vps reference for being able to use
 * ddb commands like ''show all procs'' or ''show files''.
 * Should be reset to original values _before_ leaving the debugger prompt !
 */
DB_COMMAND(setcurvps, db_setcurvps)
{
	db_expr_t decaddr;
	struct vps *vps, *vps2;
	int i;

	if (!have_addr) {
		db_printf("setcurvps <addr/index>\n");
		return;
	}

	decaddr = db_hex2dec(addr);
	if (decaddr == -1) {
		vps = (struct vps *)addr;
	} else {
		vps = NULL;
		i = 0;
		LIST_FOREACH(vps2, &vps_head, vps_all) {
			if (i == decaddr) {
				vps = vps2;
				break;
			}
			i++;
		}
		if (vps == NULL)
			return;
	}

	db_printf("curthread=%p\n"
		  "   td_vps=%p\n"
		  "   td_ucred=%p\n"
		  , curthread
		  , curthread->td_vps
		  , curthread->td_ucred
		 );
	if (curthread->td_ucred)
		db_printf("   td_ucred->cr_vps=%p\n",
		    curthread->td_ucred->cr_vps);

	curthread->td_vps = vps;
	if (curthread->td_ucred)
		curthread->td_ucred->cr_vps = vps;

	db_printf("curthread=%p\n"
		  "   td_vps=%p\n"
		  "   td_ucred=%p\n"
		  , curthread
		  , curthread->td_vps
		  , curthread->td_ucred
		 );
	if (curthread->td_ucred)
		db_printf("   td_ucred->cr_vps=%p\n",
		    curthread->td_ucred->cr_vps);
}

DB_SHOW_COMMAND(ucred, db_show_ucred)
{
	struct ucred *ucred;

	if (!have_addr) {
		db_printf("show ucred <addr>\n");
		return;
	}

	ucred = (struct ucred *)addr;

	db_printf("ucred:       %p\n", ucred);
	db_printf("cr_ref:      %d\n", ucred->cr_ref);
	db_printf("cr_vps:      %p\n", ucred->cr_vps);
	db_printf("cr_prison:   %p\n", ucred->cr_prison);
	db_printf("cr_uid:	%d\n", ucred->cr_uid);
	db_printf("cr_gid:	%d\n", ucred->cr_gid);
}
#endif /* DDB */

static int
vps_ddb_modevent(module_t mod, int type, void *data)
{

        switch (type) {
        case MOD_LOAD:
		refcount_init(&vps_ddb_refcnt, 0);
                break;
        case MOD_UNLOAD:
		if (vps_ddb_refcnt > 0)
			return (EBUSY);
                break;
        default:
                return (EOPNOTSUPP);
        }
        return (0);
}

static moduledata_t vps_ddb_mod = {
        "vps_ddb",
        vps_ddb_modevent,
        0
};

DECLARE_MODULE(vps_ddb, vps_ddb_mod, SI_SUB_PSEUDO, SI_ORDER_ANY);
MODULE_VERSION(vps_ddb, 1);

#endif /* DDB */
#endif /* VPS */

/* EOF */
