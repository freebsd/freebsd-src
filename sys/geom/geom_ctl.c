/*-
 * Copyright (c) 2002 Poul-Henning Kamp
 * Copyright (c) 2002 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by Poul-Henning Kamp
 * and NAI Labs, the Security Research Division of Network Associates, Inc.
 * under DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"), as part of the
 * DARPA CHATS research program.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The names of the authors may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
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

#include "opt_geom.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/bio.h>
#include <sys/conf.h>
#include <sys/disk.h>
#include <sys/malloc.h>
#include <sys/sysctl.h>

#include <sys/lock.h>
#include <sys/mutex.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>

#include <geom/geom.h>
#include <geom/geom_int.h>
#define GEOM_CTL_TABLE 1
#include <geom/geom_ctl.h>
#include <geom/geom_ext.h>


static g_access_t g_ctl_access;
static g_start_t g_ctl_start;
static void g_ctl_init(void);
static d_ioctl_t g_ctl_ioctl;

struct g_class g_ctl_class = {
	"GEOMCTL",
	NULL,
	NULL,
	G_CLASS_INITIALIZER
};

DECLARE_GEOM_CLASS_INIT(g_ctl_class, g_ctl, g_ctl_init);

/*
 * We cannot do create our geom.ctl geom/provider in g_ctl_init() because
 * the event thread has to finish adding our class first and that doesn't
 * happen until later.  We know however, that the events are processed in
 * FIFO order, so scheduling g_ctl_init2() with g_call_me() is safe.
 */

static void
g_ctl_init2(void *p __unused)
{
	struct g_geom *gp;
	struct g_provider *pp;

	g_topology_assert();
	gp = g_new_geomf(&g_ctl_class, "geom.ctl");
	gp->start = g_ctl_start;
	gp->access = g_ctl_access;
	pp = g_new_providerf(gp, "%s", gp->name);
	pp->sectorsize = 512;
	g_error_provider(pp, 0);
}

static void
g_ctl_init(void)
{
	mtx_unlock(&Giant);
	g_add_class(&g_ctl_class);
	g_call_me(g_ctl_init2, NULL);
	mtx_lock(&Giant);
}

/*
 * We allow any kind of access.  Access control is handled at the devfs
 * level.
 */

static int
g_ctl_access(struct g_provider *pp, int r, int w, int e)
{
	int error;

	g_trace(G_T_ACCESS, "g_ctl_access(%s, %d, %d, %d)",
	    pp->name, r, w, e);

	g_topology_assert();
	error = 0;
	return (error);
}

static void
g_ctl_start(struct bio *bp)
{
	struct g_ioctl *gio;
	int error;

	switch(bp->bio_cmd) {
	case BIO_DELETE:
	case BIO_READ:
	case BIO_WRITE:
		error = EOPNOTSUPP;
		break;
	case BIO_GETATTR:
	case BIO_SETATTR:
		if (strcmp(bp->bio_attribute, "GEOM::ioctl") ||
		    bp->bio_length != sizeof *gio) {
			error = EOPNOTSUPP;
			break;
		}
		gio = (struct g_ioctl *)bp->bio_data;
		gio->func = g_ctl_ioctl;
		error = EDIRIOCTL;
		break;
	default:
		error = EOPNOTSUPP;
		break;
	}
	g_io_deliver(bp, error);
	return;
}

/*
 * All the stuff above is really just needed to get to the stuff below
 */

static int
g_ctl_ioctl_configgeom(dev_t dev, u_long cmd, caddr_t data, int fflag, struct thread *td)
{
	struct geomconfiggeom *gcp;
	struct g_configargs ga;
	int error;

	error = 0;
	bzero(&ga, sizeof ga);
	gcp = (struct geomconfiggeom *)data;
	ga.class = g_idclass(&gcp->class);
	if (ga.class == NULL)
		return (EINVAL);
	if (ga.class->config == NULL)
		return (EOPNOTSUPP);
	ga.geom = g_idgeom(&gcp->geom);
	ga.provider = g_idprovider(&gcp->provider);
	ga.len = gcp->len;
	if (gcp->len > 64 * 1024)
		return (EINVAL);
	else if (gcp->len == 0) {
		ga.ptr = NULL;
	} else {
		ga.ptr = g_malloc(gcp->len, M_WAITOK);
		error = copyin(gcp->ptr, ga.ptr, gcp->len);
		if (error) {
			g_free(ga.ptr);
			return (error);
		}
	}
	ga.flag = gcp->flag;
	error = ga.class->config(&ga);
	if (gcp->len != 0)
		copyout(ga.ptr, gcp->ptr, gcp->len);	/* Ignore error */
	gcp->class.u.id = (uintptr_t)ga.class;
	gcp->class.len = 0;
	gcp->geom.u.id = (uintptr_t)ga.geom;
	gcp->geom.len = 0;
	gcp->provider.u.id = (uintptr_t)ga.provider;
	gcp->provider.len = 0;
	return(error);
}

/*
 * Report an error back to the user in ascii format.  Return whatever copyout
 * returned, or EINVAL if it succeeded.
 * XXX: should not be static.
 * XXX: should take printf like args.
 */
static int
g_ctl_seterror(struct geom_ctl_req *req, const char *errtxt)
{
	int error;

	error = copyout(errtxt, req->error,
	    imin(req->lerror, strlen(errtxt) + 1));
	if (!error)
		error = EINVAL;
	return (error);
}

/*
 * Allocate space and copyin() something.
 * XXX: this should really be a standard function in the kernel.
 */
static void *
geom_alloc_copyin(void *uaddr, size_t len, int *errp)
{
	int error;
	void *ptr;

	ptr = g_malloc(len, M_WAITOK);
	if (ptr == NULL)
		error = ENOMEM;
	else
		error = copyin(uaddr, ptr, len);
	if (!error)
		return (ptr);
	*errp = error;
	if (ptr != NULL)
		g_free(ptr);
	return (NULL);
}


/*
 * XXX: This function is a nightmare.  It walks through the request and
 * XXX: makes sure that the various bits and pieces are there and copies
 * XXX: some of them into kernel memory to make things easier.
 * XXX: I really wish we had a standard marshalling layer somewhere.
 */

static int
geom_ctl_copyin(struct geom_ctl_req *req)
{
	int error, i, j;
	struct geom_ctl_req_arg *ap;
	char *p;

	error = 0;
	if (!useracc(req->error, req->lerror, VM_PROT_WRITE))
		return (g_ctl_seterror(req, "No access to error field"));
	ap = geom_alloc_copyin(req->arg, req->narg * sizeof(*ap), &error);
	if (ap == NULL)
		return (error);
	for (i = 0; !error && i < req->narg; i++) {
		if (ap[i].len < 0 &&
		    !useracc(ap[i].value, 1 + -ap[i].len, VM_PROT_READ))
			error = g_ctl_seterror(req, "No access to param data");
		else if (ap[i].len > 0 &&
		    !useracc(ap[i].value, ap[i].len,
			   VM_PROT_READ | VM_PROT_WRITE))
			error = g_ctl_seterror(req, "No access to param data");
		if (ap[i].name == NULL)
			continue;
		p = NULL;
		if (ap[i].nlen < 1 || ap[i].nlen > SPECNAMELEN)
			error = EINVAL;
		if (error)
			break;
		p = geom_alloc_copyin(ap[i].name, ap[i].nlen + 1, &error);
		if (error)
			break;
		if (p[ap[i].nlen] != '\0')
			error = EINVAL;
		if (!error) {
			ap[i].name = p;
			ap[i].nlen = 0;
		} else {
			g_free(p);
			break;
		}
	}
	if (!error) {
		req->arg = ap;
		return (0);
	}
	for (j = 0; j < i; j++)
		if (ap[j].nlen == 0 && ap[j].name != NULL)
			g_free(ap[j].name);
	g_free(ap);
	return (error);
}

static void
geom_ctl_dump(struct geom_ctl_req *req)
{
	u_int i;
	int j, error;
	struct geom_ctl_req_arg *ap;
	void *p;
	

	printf("Dump of geom_ctl %s request at %p:\n", req->reqt->name, req);
	if (req->lerror > 0) {
		p = geom_alloc_copyin(req->error, req->lerror, &error);
		if (p != NULL) {
			((char *)p)[req->lerror - 1] = '\0';
			printf("  error:\t\"%s\"\n", (char *)p);
			g_free(p);
		}
	}
	for (i = 0; i < req->narg; i++) {
		ap = &req->arg[i];
		if (ap->name != NULL)
			printf("  param:\t\"%s\"", ap->name);
		else
			printf("  meta:\t@%jd", (intmax_t)ap->offset);
		printf(" [%d] = ", ap->len);
		if (ap->len < 0) {
			p = geom_alloc_copyin(ap->value, 1 + -ap->len, &error);
			((char *)p)[-ap->len] = '\0';
			if (p != NULL)
				printf("\"%s\"", (char *)p);
			g_free(p);
		} else if (ap->len > 0) {
			p = geom_alloc_copyin(ap->value, ap->len, &error);
			for (j = 0; j < ap->len; j++)
				printf(" %02x", ((u_char *)p)[j]);
			g_free(p);
		} else {
			printf(" = %p", ap->value);
		}
		printf("\n");
	}
}

/*
 * Handle ioctl from libgeom::geom_ctl.c
 */
static int
g_ctl_ioctl_ctl(dev_t dev, u_long cmd, caddr_t data, int fflag, struct thread *td)
{
	int error;
	int i;
	struct geom_ctl_req *req;

	req = (void *)data;
	if (req->lerror < 1)
		return (EINVAL);
	if (req->version != GEOM_CTL_VERSION)
		return (g_ctl_seterror(req,
		    "Kernel and libgeom version skew."));
	for (i = 0; gcrt[i].request != GEOM_INVALID_REQUEST; i++)
		if (gcrt[i].request == req->request) {
			req->reqt = &gcrt[i];
			break;
		}
	if (gcrt[i].request == GEOM_INVALID_REQUEST)
		return (g_ctl_seterror(req, "Invalid request"));
	error = geom_ctl_copyin(req);
	if (error)
		return (error);
	geom_ctl_dump(req);
	return (0);
}

static int
g_ctl_ioctl(dev_t dev, u_long cmd, caddr_t data, int fflag, struct thread *td)
{
	int error;

	DROP_GIANT();
	g_topology_lock();
	switch(cmd) {
	case GEOMCONFIGGEOM:
		error = g_ctl_ioctl_configgeom(dev, cmd, data, fflag, td);
		break;
	case GEOM_CTL:
		error = g_ctl_ioctl_ctl(dev, cmd, data, fflag, td);
		break;
	default:
		error = ENOTTY;
		break;
	}
	g_topology_unlock();
	PICKUP_GIANT();
	return (error);

}
