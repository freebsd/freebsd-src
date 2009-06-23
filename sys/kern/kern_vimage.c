/*-
 * Copyright (c) 2004-2009 University of Zagreb
 * Copyright (c) 2006-2009 FreeBSD Foundation
 *
 * This software was developed by the University of Zagreb and the
 * FreeBSD Foundation under sponsorship by the Stichting NLnet and the
 * FreeBSD Foundation.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/linker.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sx.h>
#include <sys/priv.h>
#include <sys/refcount.h>
#include <sys/vimage.h>
#ifdef DDB
#include <ddb/ddb.h>
#endif

#include <net/if.h>
#include <net/route.h>
#include <net/vnet.h>

#ifndef VIMAGE_GLOBALS

MALLOC_DEFINE(M_VIMAGE, "vimage", "vimage resource container");
MALLOC_DEFINE(M_VNET, "vnet", "network stack control block");
MALLOC_DEFINE(M_VPROCG, "vprocg", "process group control block");

static TAILQ_HEAD(vnet_modlink_head, vnet_modlink) vnet_modlink_head;
static TAILQ_HEAD(vnet_modpending_head, vnet_modlink) vnet_modpending_head;
static void vnet_mod_complete_registration(struct vnet_modlink *);
static int vnet_mod_constructor(struct vnet_modlink *);
static int vnet_mod_destructor(struct vnet_modlink *);

#ifdef VIMAGE
static struct vimage *vi_alloc(struct vimage *, char *);
static int vi_destroy(struct vimage *);
static struct vimage *vimage_get_next(struct vimage *, struct vimage *, int);
static void vimage_relative_name(struct vimage *, struct vimage *,
    char *, int);
#endif

#define	VNET_LIST_WLOCK()						\
	mtx_lock(&vnet_list_refc_mtx);					\
	while (vnet_list_refc != 0)					\
		cv_wait(&vnet_list_condvar, &vnet_list_refc_mtx);

#define	VNET_LIST_WUNLOCK()						\
	mtx_unlock(&vnet_list_refc_mtx);

#ifdef VIMAGE
struct vimage_list_head vimage_head;
struct vnet_list_head vnet_head;
struct vprocg_list_head vprocg_head;
#else
#ifndef VIMAGE_GLOBALS
struct vprocg vprocg_0;
#endif
#endif

#ifdef VIMAGE
struct cv vnet_list_condvar;
struct mtx vnet_list_refc_mtx;
int vnet_list_refc = 0;

static u_int last_vi_id = 0;
static u_int last_vprocg_id = 0;

struct vnet *vnet0;
#endif

#ifdef VIMAGE

/*
 * Move an ifnet to or from another vnet, specified by the jail id.  If a
 * vi_req is passed in, it is used to find the interface and a vimage
 * containing the vnet (a vimage name of ".." stands for the parent vnet).
 */
int
vi_if_move(struct thread *td, struct ifnet *ifp, char *ifname, int jid,
    struct vi_req *vi_req)
{
	struct ifnet *t_ifp;
	struct prison *pr;
	struct vimage *new_vip, *my_vip;
	struct vnet *new_vnet;

	if (vi_req != NULL) {
		/* SIOCSIFVIMAGE */
		/* Check for API / ABI version mismatch. */
		if (vi_req->vi_api_cookie != VI_API_COOKIE)
			return (EDOOFUS);

		/* Find the target vnet. */
		my_vip = TD_TO_VIMAGE(td);
		if (strcmp(vi_req->vi_name, "..") == 0) {
			if (IS_DEFAULT_VIMAGE(my_vip))
				return (ENXIO);
			new_vnet = my_vip->vi_parent->v_net;
		} else {
			new_vip = vimage_by_name(my_vip, vi_req->vi_name);
			if (new_vip == NULL)
				return (ENXIO);
			new_vnet = new_vip->v_net;
		}

		/* Try to find the target ifnet by name. */
		ifname = vi_req->vi_if_xname;
		ifp = ifunit(ifname);
		if (ifp == NULL)
			return (ENXIO);
	} else {
		sx_slock(&allprison_lock);
		pr = prison_find_child(td->td_ucred->cr_prison, jid);
		sx_sunlock(&allprison_lock);
		if (pr == NULL)
			return (ENXIO);
		mtx_unlock(&pr->pr_mtx);
		if (ifp != NULL) {
			/* SIOCSIFVNET */
			new_vnet = pr->pr_vnet;
		} else {
			/* SIOCSIFRVNET */
			new_vnet = TD_TO_VNET(td);
			CURVNET_SET(pr->pr_vnet);
			ifp = ifunit(ifname);
			CURVNET_RESTORE();
			if (ifp == NULL)
				return (ENXIO);
		}

		/* No-op if the target jail has the same vnet. */
		if (new_vnet == ifp->if_vnet)
			return (0);
	}

	/*
	 * Check for naming clashes in target vnet.  Not locked so races
	 * are possible.
	 */
	CURVNET_SET_QUIET(new_vnet);
	t_ifp = ifunit(ifname);
	CURVNET_RESTORE();
	if (t_ifp != NULL)
		return (EEXIST);

	/* Detach from curvnet and attach to new_vnet. */
	if_vmove(ifp, new_vnet);

	/* Report the new if_xname back to the userland */
	sprintf(ifname, "%s", ifp->if_xname);
	return (0);
}

/*
 * Interim userspace interface - will be replaced by jail soon.
 */

int
vi_td_ioctl(u_long cmd, struct vi_req *vi_req, struct thread *td)
{
	int error = 0;
	struct vimage *vip = TD_TO_VIMAGE(td);
	struct vimage *vip_r = NULL;

	/* Check for API / ABI version mismatch. */
	if (vi_req->vi_api_cookie != VI_API_COOKIE)
		return (EDOOFUS);

	error = priv_check(td, PRIV_REBOOT); /* XXX temp. priv abuse */
	if (error)
		return (error);

	vip_r = vimage_by_name(vip, vi_req->vi_name);
	if (vip_r == NULL && !(vi_req->vi_req_action & VI_CREATE))
		return (ESRCH);
	if (vip_r != NULL && vi_req->vi_req_action & VI_CREATE)
		return (EADDRINUSE);
	if (vi_req->vi_req_action == VI_GETNEXT) {
		vip_r = vimage_get_next(vip, vip_r, 0);
		if (vip_r == NULL)
			return (ESRCH);
	}
	if (vi_req->vi_req_action == VI_GETNEXT_RECURSE) {
		vip_r = vimage_get_next(vip, vip_r, 1);
		if (vip_r == NULL)
			return (ESRCH);
	}

	if (vip_r && !vi_child_of(vip, vip_r) && /* XXX delete the rest? */
	    vi_req->vi_req_action != VI_GET &&
	    vi_req->vi_req_action != VI_GETNEXT)
		return (EPERM);

	switch (cmd) {

	case SIOCGPVIMAGE:
		vimage_relative_name(vip, vip_r, vi_req->vi_name,
		    sizeof (vi_req->vi_name));
		vi_req->vi_proc_count = vip_r->v_procg->nprocs;
		vi_req->vi_if_count = vip_r->v_net->ifcnt;
		vi_req->vi_sock_count = vip_r->v_net->sockcnt;
		break;

	case SIOCSPVIMAGE:
		if (vi_req->vi_req_action == VI_DESTROY) {
			error = vi_destroy(vip_r);
			break;
		}

		if (vi_req->vi_req_action == VI_SWITCHTO) {
			struct proc *p = td->td_proc;
			struct ucred *oldcred, *newcred;

			/*
			 * XXX priv_check()?
			 * XXX allow only a single td per proc here?
			 */
			newcred = crget();
			PROC_LOCK(p);
			oldcred = p->p_ucred;
			setsugid(p);
			crcopy(newcred, oldcred);
			refcount_release(&newcred->cr_vimage->vi_ucredrefc);
			newcred->cr_vimage = vip_r;
			refcount_acquire(&newcred->cr_vimage->vi_ucredrefc);
			p->p_ucred = newcred;
			PROC_UNLOCK(p);
			sx_xlock(&allproc_lock);
			oldcred->cr_vimage->v_procg->nprocs--;
			refcount_release(&oldcred->cr_vimage->vi_ucredrefc);
			P_TO_VPROCG(p)->nprocs++;
			sx_xunlock(&allproc_lock);
			crfree(oldcred);
			break;
		}

		if (vi_req->vi_req_action & VI_CREATE) {
			char *dotpos;

			dotpos = strrchr(vi_req->vi_name, '.');
			if (dotpos != NULL) {
				*dotpos = 0;
				vip = vimage_by_name(vip, vi_req->vi_name);
				if (vip == NULL)
					return (ESRCH);
				dotpos++;
				vip_r = vi_alloc(vip, dotpos);
			} else
				vip_r = vi_alloc(vip, vi_req->vi_name);
			if (vip_r == NULL)
				return (ENOMEM);
		}
	}
	return (error);
}

int
vi_child_of(struct vimage *parent, struct vimage *child)
{

	if (child == parent)
		return (0);
	for (; child; child = child->vi_parent)
		if (child == parent)
			return (1);
	return (0);
}

struct vimage *
vimage_by_name(struct vimage *top, char *name)
{
	struct vimage *vip;
	char *next_name;
	int namelen;

	next_name = strchr(name, '.');
	if (next_name != NULL) {
		namelen = next_name - name;
		next_name++;
		if (namelen == 0) {
			if (strlen(next_name) == 0)
				return (top);	/* '.' == this vimage */
			else
				return (NULL);
		}
	} else
		namelen = strlen(name);
	if (namelen == 0)
		return (NULL);
	LIST_FOREACH(vip, &top->vi_child_head, vi_sibling) {
		if (strlen(vip->vi_name) == namelen &&
		    strncmp(name, vip->vi_name, namelen) == 0) {
			if (next_name != NULL)
				return (vimage_by_name(vip, next_name));
			else
				return (vip);
		}
	}
	return (NULL);
}

static void
vimage_relative_name(struct vimage *top, struct vimage *where,
    char *buffer, int bufflen)
{
	int used = 1;

	if (where == top) {
		sprintf(buffer, ".");
		return;
	} else
		*buffer = 0;

	do {
		int namelen = strlen(where->vi_name);

		if (namelen + used + 1 >= bufflen)
			panic("buffer overflow");

		if (used > 1) {
			bcopy(buffer, &buffer[namelen + 1], used);
			buffer[namelen] = '.';
			used++;
		} else
			bcopy(buffer, &buffer[namelen], used);
		bcopy(where->vi_name, buffer, namelen);
		used += namelen;
		where = where->vi_parent;
	} while (where != top);
}

static struct vimage *
vimage_get_next(struct vimage *top, struct vimage *where, int recurse)
{
	struct vimage *next;

	if (recurse) {
		/* Try to go deeper in the hierarchy */
		next = LIST_FIRST(&where->vi_child_head);
		if (next != NULL)
			return (next);
	}

	do {
		/* Try to find next sibling */
		next = LIST_NEXT(where, vi_sibling);
		if (!recurse || next != NULL)
			return (next);

		/* Nothing left on this level, go one level up */
		where = where->vi_parent;
	} while (where != top->vi_parent);

	/* Nothing left to be visited, we are done */
	return (NULL);
}

#endif /* VIMAGE */ /* User interface block */


/*
 * Kernel interfaces and handlers.
 */

void
vnet_mod_register(const struct vnet_modinfo *vmi)
{

	vnet_mod_register_multi(vmi, NULL, NULL);
}

void
vnet_mod_register_multi(const struct vnet_modinfo *vmi, void *iarg,
    char *iname)
{
	struct vnet_modlink *vml, *vml_iter;
	
	/* Do not register the same {module, iarg} pair more than once. */
	TAILQ_FOREACH(vml_iter, &vnet_modlink_head, vml_mod_le)
		if (vml_iter->vml_modinfo == vmi && vml_iter->vml_iarg == iarg)
			break;
	if (vml_iter != NULL)
		panic("registering an already registered vnet module: %s",
		    vml_iter->vml_modinfo->vmi_name);
	vml = malloc(sizeof(struct vnet_modlink), M_VIMAGE, M_NOWAIT);

	/*
	 * XXX we support only statically assigned module IDs at the time.
	 * In principle modules should be able to get a dynamically
	 * assigned ID at registration time.
	 *
	 * If a module is registered in multiple instances, then each
	 * instance must have both iarg and iname set.
	 */
	if (vmi->vmi_id >= VNET_MOD_MAX)
		panic("invalid vnet module ID: %d", vmi->vmi_id);
	if (vmi->vmi_name == NULL)
		panic("vnet module with no name: %d", vmi->vmi_id);
	if ((iarg == NULL) ^ (iname == NULL))
		panic("invalid vnet module instance: %s", vmi->vmi_name);

	vml->vml_modinfo = vmi;
	vml->vml_iarg = iarg;
	vml->vml_iname = iname;

	/* Check whether the module we depend on is already registered. */
	if (vmi->vmi_dependson != vmi->vmi_id) {
		TAILQ_FOREACH(vml_iter, &vnet_modlink_head, vml_mod_le)
			if (vml_iter->vml_modinfo->vmi_id ==
			    vmi->vmi_dependson)
				break;	/* Depencency found, we are done. */
		if (vml_iter == NULL) {
#ifdef DEBUG_ORDERING
			printf("dependency %d missing for vnet mod %s,"
			    "postponing registration\n",
			    vmi->vmi_dependson, vmi->vmi_name);
#endif /* DEBUG_ORDERING */
			TAILQ_INSERT_TAIL(&vnet_modpending_head, vml,
			    vml_mod_le);
			return;
		}
	}

	vnet_mod_complete_registration(vml);
}
	
void
vnet_mod_complete_registration(struct vnet_modlink *vml)
{
	VNET_ITERATOR_DECL(vnet_iter);
	struct vnet_modlink *vml_iter;

	TAILQ_INSERT_TAIL(&vnet_modlink_head, vml, vml_mod_le);

	VNET_FOREACH(vnet_iter) {
		CURVNET_SET_QUIET(vnet_iter);
		vnet_mod_constructor(vml);
		CURVNET_RESTORE();
	}

	/* Check for pending modules depending on us. */
	do {
		TAILQ_FOREACH(vml_iter, &vnet_modpending_head, vml_mod_le)
			if (vml_iter->vml_modinfo->vmi_dependson ==
			    vml->vml_modinfo->vmi_id)
				break;
		if (vml_iter != NULL) {
#ifdef DEBUG_ORDERING
			printf("vnet mod %s now registering,"
			    "dependency %d loaded\n",
			    vml_iter->vml_modinfo->vmi_name,
			    vml->vml_modinfo->vmi_id);
#endif /* DEBUG_ORDERING */
			TAILQ_REMOVE(&vnet_modpending_head, vml_iter,
			    vml_mod_le);
			vnet_mod_complete_registration(vml_iter);
		}
	} while (vml_iter != NULL);
}

void
vnet_mod_deregister(const struct vnet_modinfo *vmi)
{

	vnet_mod_deregister_multi(vmi, NULL, NULL);
}

void
vnet_mod_deregister_multi(const struct vnet_modinfo *vmi, void *iarg,
    char *iname)
{
	VNET_ITERATOR_DECL(vnet_iter);
	struct vnet_modlink *vml;

	TAILQ_FOREACH(vml, &vnet_modlink_head, vml_mod_le)
		if (vml->vml_modinfo == vmi && vml->vml_iarg == iarg)
			break;
	if (vml == NULL)
		panic("cannot deregister unregistered vnet module %s",
		    vmi->vmi_name);

	VNET_FOREACH(vnet_iter) {
		CURVNET_SET_QUIET(vnet_iter);
		vnet_mod_destructor(vml);
		CURVNET_RESTORE();
	}

	TAILQ_REMOVE(&vnet_modlink_head, vml, vml_mod_le);
	free(vml, M_VIMAGE);
}

static int
vnet_mod_constructor(struct vnet_modlink *vml)
{
	const struct vnet_modinfo *vmi = vml->vml_modinfo;

#ifdef DEBUG_ORDERING
	printf("instantiating vnet_%s", vmi->vmi_name);
	if (vml->vml_iarg)
		printf("/%s", vml->vml_iname);
	printf(": ");
#ifdef VIMAGE
	if (vmi->vmi_size)
		printf("malloc(%zu); ", vmi->vmi_size);
#endif
	if (vmi->vmi_iattach != NULL)
		printf("iattach()");
	printf("\n");
#endif

#ifdef VIMAGE
	if (vmi->vmi_size) {
		void *mem = malloc(vmi->vmi_size, M_VNET,
		    M_NOWAIT | M_ZERO);
		if (mem == NULL) /* XXX should return error, not panic. */
			panic("malloc for %s\n", vmi->vmi_name);
		curvnet->mod_data[vmi->vmi_id] = mem;
	}
#endif

	if (vmi->vmi_iattach != NULL)
		vmi->vmi_iattach(vml->vml_iarg);

	return (0);
}

static int
vnet_mod_destructor(struct vnet_modlink *vml)
{
	const struct vnet_modinfo *vmi = vml->vml_modinfo;

#ifdef DEBUG_ORDERING
	printf("destroying vnet_%s", vmi->vmi_name);
	if (vml->vml_iarg)
		printf("/%s", vml->vml_iname);
	printf(": ");
	if (vmi->vmi_idetach != NULL)
		printf("idetach(); ");
#ifdef VIMAGE
	if (vmi->vmi_size)
		printf("free()");
#endif
	printf("\n");
#endif

	if (vmi->vmi_idetach)
		vmi->vmi_idetach(vml->vml_iarg);

#ifdef VIMAGE
	if (vmi->vmi_size) {
		if (curvnet->mod_data[vmi->vmi_id] == NULL)
			panic("vi_destroy: %s\n", vmi->vmi_name);
		free(curvnet->mod_data[vmi->vmi_id], M_VNET);
		curvnet->mod_data[vmi->vmi_id] = NULL;
	}
#endif

	return (0);
}

/*
 * vi_symlookup() attempts to resolve name to address queries for
 * variables which have been moved from global namespace to virtualization
 * container structures, but are still directly accessed from legacy
 * userspace processes via kldsym(2) and kmem(4) interfaces.
 */
int
vi_symlookup(struct kld_sym_lookup *lookup, char *symstr)
{
	struct vnet_modlink *vml;
	struct vnet_symmap *mapentry;

	TAILQ_FOREACH(vml, &vnet_modlink_head, vml_mod_le) {
		if (vml->vml_modinfo->vmi_symmap == NULL)
			continue;
		for (mapentry = vml->vml_modinfo->vmi_symmap;
		    mapentry->name != NULL; mapentry++) {
			if (strcmp(symstr, mapentry->name) == 0) {
#ifdef VIMAGE
				lookup->symvalue =
				    (u_long) curvnet->mod_data[
				    vml->vml_modinfo->vmi_id];
				lookup->symvalue += mapentry->offset;
#else
				lookup->symvalue = (u_long) mapentry->offset;
#endif
				lookup->symsize = mapentry->size;
				return (0);
			}
		}
	}
	return (ENOENT);
}

#ifdef VIMAGE
struct vnet *
vnet_alloc(void)
{
	struct vnet *vnet;
	struct vnet_modlink *vml;

	vnet = malloc(sizeof(struct vnet), M_VNET, M_WAITOK | M_ZERO);
	vnet->vnet_magic_n = VNET_MAGIC_N;

	/* Initialize / attach vnet module instances. */
	CURVNET_SET_QUIET(vnet);
	TAILQ_FOREACH(vml, &vnet_modlink_head, vml_mod_le)
		vnet_mod_constructor(vml);
	CURVNET_RESTORE();

	VNET_LIST_WLOCK();
	LIST_INSERT_HEAD(&vnet_head, vnet, vnet_le);
	VNET_LIST_WUNLOCK();

	return (vnet);
}

void
vnet_destroy(struct vnet *vnet)
{
	struct ifnet *ifp, *nifp;
	struct vnet_modlink *vml;

	KASSERT(vnet->sockcnt == 0, ("%s: vnet still has sockets", __func__));

	VNET_LIST_WLOCK();
	LIST_REMOVE(vnet, vnet_le);
	VNET_LIST_WUNLOCK();

	CURVNET_SET_QUIET(vnet);
	INIT_VNET_NET(vnet);

	/* Return all inherited interfaces to their parent vnets. */
	TAILQ_FOREACH_SAFE(ifp, &V_ifnet, if_link, nifp) {
		if (ifp->if_home_vnet != ifp->if_vnet)
			if_vmove(ifp, ifp->if_home_vnet);
	}

	/* Detach / free per-module state instances. */
	TAILQ_FOREACH_REVERSE(vml, &vnet_modlink_head,
			      vnet_modlink_head, vml_mod_le)
		vnet_mod_destructor(vml);

	CURVNET_RESTORE();

	/* Hopefully, we are OK to free the vnet container itself. */
	vnet->vnet_magic_n = 0xdeadbeef;
	free(vnet, M_VNET);
}

static struct vimage *
vi_alloc(struct vimage *parent, char *name)
{
	struct vimage *vip;
	struct vprocg *vprocg;

	vip = malloc(sizeof(struct vimage), M_VIMAGE, M_NOWAIT | M_ZERO);
	if (vip == NULL)
		panic("vi_alloc: malloc failed for vimage \"%s\"\n", name);
	vip->vi_id = last_vi_id++;
	LIST_INIT(&vip->vi_child_head);
	sprintf(vip->vi_name, "%s", name);
	vip->vi_parent = parent;
	/* XXX locking */
	if (parent != NULL)
		LIST_INSERT_HEAD(&parent->vi_child_head, vip, vi_sibling);
	else if (!LIST_EMPTY(&vimage_head))
		panic("there can be only one default vimage!");
	LIST_INSERT_HEAD(&vimage_head, vip, vi_le);

	vip->v_net = vnet_alloc();

	vprocg = malloc(sizeof(struct vprocg), M_VPROCG, M_NOWAIT | M_ZERO);
	if (vprocg == NULL)
		panic("vi_alloc: malloc failed for vprocg \"%s\"\n", name);
	vip->v_procg = vprocg;
	vprocg->vprocg_id = last_vprocg_id++;

	/* XXX locking */
	LIST_INSERT_HEAD(&vprocg_head, vprocg, vprocg_le);

	return (vip);
}

/*
 * Destroy a vnet - unlink all linked lists, hashtables etc., free all
 * the memory, stop all the timers...
 */
static int
vi_destroy(struct vimage *vip)
{
	struct vprocg *vprocg = vip->v_procg;

	/* XXX Beware of races -> more locking to be done... */
	if (!LIST_EMPTY(&vip->vi_child_head))
		return (EBUSY);

	if (vprocg->nprocs != 0)
		return (EBUSY);

#ifdef INVARIANTS
	if (vip->vi_ucredrefc != 0)
		printf("vi_destroy: %s ucredrefc %d\n",
		    vip->vi_name, vip->vi_ucredrefc);
#endif

	/* Point with no return - cleanup MUST succeed! */
	vnet_destroy(vip->v_net);

	LIST_REMOVE(vip, vi_le);
	LIST_REMOVE(vip, vi_sibling);
	LIST_REMOVE(vprocg, vprocg_le);

	free(vprocg, M_VPROCG);
	free(vip, M_VIMAGE);

	return (0);
}
#endif /* VIMAGE */

static void
vi_init(void *unused)
{

	TAILQ_INIT(&vnet_modlink_head);
	TAILQ_INIT(&vnet_modpending_head);

#ifdef VIMAGE
	LIST_INIT(&vimage_head);
	LIST_INIT(&vprocg_head);
	LIST_INIT(&vnet_head);

	mtx_init(&vnet_list_refc_mtx, "vnet_list_refc_mtx", NULL, MTX_DEF);
	cv_init(&vnet_list_condvar, "vnet_list_condvar");

	/* Default image has no parent and no name. */
	vi_alloc(NULL, "");

	/*
	 * We MUST clear curvnet in vi_init_done() before going SMP,
	 * otherwise CURVNET_SET() macros would scream about unnecessary
	 * curvnet recursions.
	 */
	curvnet = prison0.pr_vnet = vnet0 = LIST_FIRST(&vnet_head);
#endif
}

static void
vi_init_done(void *unused)
{
	struct vnet_modlink *vml_iter;

#ifdef VIMAGE
	curvnet = NULL;
#endif

	if (TAILQ_EMPTY(&vnet_modpending_head))
		return;

	printf("vnet modules with unresolved dependencies:\n");
	TAILQ_FOREACH(vml_iter, &vnet_modpending_head, vml_mod_le)
		printf("    %d:%s depending on %d\n",
		    vml_iter->vml_modinfo->vmi_id,
		    vml_iter->vml_modinfo->vmi_name,
		    vml_iter->vml_modinfo->vmi_dependson);
	panic("going nowhere without my vnet modules!");
}

SYSINIT(vimage, SI_SUB_VIMAGE, SI_ORDER_FIRST, vi_init, NULL);
SYSINIT(vimage_done, SI_SUB_VIMAGE_DONE, SI_ORDER_FIRST, vi_init_done, NULL);
#endif /* !VIMAGE_GLOBALS */

#ifdef VIMAGE
#ifdef DDB
static void
db_vnet_ptr(void *arg)
{

	if (arg)
		db_printf(" %p", arg);
	else
#if SIZE_MAX == UINT32_MAX /* 32-bit arch */
		db_printf("          0");
#else /* 64-bit arch, most probaly... */
		db_printf("                  0");
#endif
}

DB_SHOW_COMMAND(vnets, db_show_vnets)
{
	VNET_ITERATOR_DECL(vnet_iter);

#if SIZE_MAX == UINT32_MAX /* 32-bit arch */
	db_printf("      vnet ifs socks");
	db_printf("        net       inet      inet6      ipsec   netgraph\n");
#else /* 64-bit arch, most probaly... */
	db_printf("              vnet ifs socks");
	db_printf("                net               inet              inet6              ipsec           netgraph\n");
#endif
	VNET_FOREACH(vnet_iter) {
		db_printf("%p %3d %5d",
		    vnet_iter, vnet_iter->ifcnt, vnet_iter->sockcnt);
		db_vnet_ptr(vnet_iter->mod_data[VNET_MOD_NET]);
		db_vnet_ptr(vnet_iter->mod_data[VNET_MOD_INET]);
		db_vnet_ptr(vnet_iter->mod_data[VNET_MOD_INET6]);
		db_vnet_ptr(vnet_iter->mod_data[VNET_MOD_IPSEC]);
		db_vnet_ptr(vnet_iter->mod_data[VNET_MOD_NETGRAPH]);
		db_printf("\n");
	}
}
#endif
#endif /* VIMAGE */
