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
#define GCTL_TABLE 1
#include <geom/geom_ctl.h>
#include <geom/geom_ext.h>

static d_ioctl_t g_ctl_ioctl;

static struct cdevsw g_ctl_cdevsw = {
	.d_open =	nullopen,
	.d_close =	nullclose,
	.d_ioctl =	g_ctl_ioctl,
	.d_name =	"g_ctl",
};

void
g_ctl_init(void)
{

	make_dev(&g_ctl_cdevsw, 0,
	    UID_ROOT, GID_OPERATOR, 0640, PATH_GEOM_CTL);
	KASSERT(GCTL_PARAM_RD == VM_PROT_READ,
		("GCTL_PARAM_RD != VM_PROT_READ"));
	KASSERT(GCTL_PARAM_WR == VM_PROT_WRITE,
		("GCTL_PARAM_WR != VM_PROT_WRITE"));
}

/*
 * Report an error back to the user in ascii format.  Return whatever copyout
 * returned, or EINVAL if it succeeded.
 * XXX: should not be static.
 * XXX: should take printf like args.
 */
int
gctl_error(struct gctl_req *req, const char *errtxt)
{
	int error;

	if (g_debugflags & G_F_CTLDUMP)
		printf("gctl %p error \"%s\"\n", req, errtxt);
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
geom_alloc_copyin(struct gctl_req *req, void *uaddr, size_t len, int *errp)
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
	gctl_error(req, "no access to argument");
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
gctl_copyin(struct gctl_req *req)
{
	int error, i, j;
	struct gctl_req_arg *ap;
	char *p;

	error = 0;
	ap = geom_alloc_copyin(req, req->arg, req->narg * sizeof(*ap), &error);
	if (ap == NULL) {
		gctl_error(req, "copyin() of arguments failed");
		return (error);
	}

	for (i = 0; !error && i < req->narg; i++) {
		if (ap[i].len > 0 &&
		    !useracc(ap[i].value, ap[i].len, 
		    ap[i].flag & GCTL_PARAM_RW))
			error = gctl_error(req, "no access to param data");
		if (error)
			break;
		p = NULL;
		if (ap[i].nlen < 1 || ap[i].nlen > SPECNAMELEN) {
			error = gctl_error(req, "wrong param name length");
			break;
		}
		p = geom_alloc_copyin(req, ap[i].name, ap[i].nlen, &error);
		if (p == NULL)
			break;
		if (p[ap[i].nlen - 1] != '\0') {
			error = gctl_error(req, "unterminated param name");
			g_free(p);
			break;
		}
		ap[i].name = p;
		ap[i].nlen = 0;
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
gctl_dump(struct gctl_req *req)
{
	u_int i;
	int j, error;
	struct gctl_req_arg *ap;
	void *p;
	

	printf("Dump of gctl %s request at %p:\n", req->reqt->name, req);
	if (req->lerror > 0) {
		p = geom_alloc_copyin(req, req->error, req->lerror, &error);
		if (p != NULL) {
			((char *)p)[req->lerror - 1] = '\0';
			printf("  error:\t\"%s\"\n", (char *)p);
			g_free(p);
		}
	}
	for (i = 0; i < req->narg; i++) {
		ap = &req->arg[i];
		printf("  param:\t\"%s\"", ap->name);
		printf(" [%s%s%d] = ",
		    ap->flag & GCTL_PARAM_RD ? "R" : "",
		    ap->flag & GCTL_PARAM_WR ? "W" : "",
		    ap->len);
		if (ap->flag & GCTL_PARAM_ASCII) {
			p = geom_alloc_copyin(req, ap->value, ap->len, &error);
			if (p != NULL) {
				((char *)p)[ap->len - 1] = '\0';
				printf("\"%s\"", (char *)p);
			}
			g_free(p);
		} else if (ap->len > 0) {
			p = geom_alloc_copyin(req, ap->value, ap->len, &error);
			for (j = 0; j < ap->len; j++)
				printf(" %02x", ((u_char *)p)[j]);
			g_free(p);
		} else {
			printf(" = %p", ap->value);
		}
		printf("\n");
	}
}

void *
gctl_get_param(struct gctl_req *req, const char *param, int *len)
{
	int i, error, j;
	void *p;
	struct gctl_req_arg *ap;

	for (i = 0; i < req->narg; i++) {
		ap = &req->arg[i];
		if (strcmp(param, ap->name))
			continue;
		if (!(ap->flag & GCTL_PARAM_RD))
			continue;
		if (ap->len > 0)
			j = ap->len;
		else
			j = 0;
		if (j != 0)
			p = geom_alloc_copyin(req, ap->value, j, &error);
			/* XXX: should not fail, tested prviously */
		else
			p = ap->value;
		if (len != NULL)
			*len = j;
		return (p);
	}
	return (NULL);
}

static struct g_class*
gctl_get_class(struct gctl_req *req)
{
	char *p;
	int len;
	struct g_class *cp;

	p = gctl_get_param(req, "class", &len);
	if (p == NULL)
		return (NULL);
	if (p[len - 1] != '\0') {
		gctl_error(req, "Unterminated class name");
		g_free(p);
		return (NULL);
	}
	LIST_FOREACH(cp, &g_classes, class) {
		if (!strcmp(p, cp->name)) {
			g_free(p);
			return (cp);
		}
	}
	gctl_error(req, "Class not found");
	return (NULL);
}

static struct g_geom*
gctl_get_geom(struct gctl_req *req, struct g_class *mpr)
{
	char *p;
	int len;
	struct g_class *mp;
	struct g_geom *gp;

	p = gctl_get_param(req, "geom", &len);
	if (p == NULL)
		return (NULL);
	if (p[len - 1] != '\0') {
		gctl_error(req, "Unterminated provider name");
		g_free(p);
		return (NULL);
	}
	LIST_FOREACH(mp, &g_classes, class) {
		if (mpr != NULL && mpr != mp)
			continue;
		LIST_FOREACH(gp, &mp->geom, geom) {
			if (!strcmp(p, gp->name)) {
				g_free(p);
				return (gp);
			}
		}
	}
	gctl_error(req, "Geom not found");
	return (NULL);
}

static struct g_provider*
gctl_get_provider(struct gctl_req *req)
{
	char *p;
	int len;
	struct g_class *cp;
	struct g_geom *gp;
	struct g_provider *pp;

	p = gctl_get_param(req, "provider", &len);
	if (p == NULL)
		return (NULL);
	if (p[len - 1] != '\0') {
		gctl_error(req, "Unterminated provider name");
		g_free(p);
		return (NULL);
	}
	LIST_FOREACH(cp, &g_classes, class) {
		LIST_FOREACH(gp, &cp->geom, geom) {
			LIST_FOREACH(pp, &gp->provider, provider) {
				if (!strcmp(p, pp->name)) {
					g_free(p);
					return (pp);
				}
			}
		}
	}
	gctl_error(req, "Provider not found");
	return (NULL);
}

static int
gctl_create_geom(struct gctl_req *req)
{
	struct g_class *mp;
	struct g_provider *pp;
	int error;

	g_topology_assert();
	mp = gctl_get_class(req);
	if (mp == NULL)
		return (gctl_error(req, "Class not found"));
	if (mp->create_geom == NULL)
		return (gctl_error(req, "Class has no create_geom method"));
	pp = gctl_get_provider(req);
	error = mp->create_geom(req, mp, pp);
	g_topology_assert();
	return (error);
}

static int
gctl_destroy_geom(struct gctl_req *req)
{
	struct g_class *mp;
	struct g_geom *gp;
	int error;

	g_topology_assert();
	mp = gctl_get_class(req);
	if (mp == NULL)
		return (gctl_error(req, "Class not found"));
	if (mp->destroy_geom == NULL)
		return (gctl_error(req, "Class has no destroy_geom method"));
	gp = gctl_get_geom(req, mp);
	if (gp == NULL)
		return (gctl_error(req, "Geom not specified"));
	if (gp->class != mp)
		return (gctl_error(req, "Geom not of specificed class"));
	error = mp->destroy_geom(req, mp, gp);
	g_topology_assert();
	return (error);
}

/*
 * Handle ioctl from libgeom::geom_ctl.c
 */
static int
g_ctl_ioctl_ctl(dev_t dev, u_long cmd, caddr_t data, int fflag, struct thread *td)
{
	int error;
	int i;
	struct gctl_req *req;

	req = (void *)data;
	/* It is an error if we cannot return an error text */
	if (req->lerror < 1)
		return (EINVAL);
	if (!useracc(req->error, req->lerror, VM_PROT_WRITE))
		return (EINVAL);

	/* Check the version */
	if (req->version != GCTL_VERSION)
		return (gctl_error(req,
		    "kernel and libgeom version mismatch."));
	
	/* Check the request type */
	for (i = 0; gcrt[i].request != GCTL_INVALID_REQUEST; i++)
		if (gcrt[i].request == req->request)
			break;
	if (gcrt[i].request == GCTL_INVALID_REQUEST)
		return (gctl_error(req, "invalid request"));
	req->reqt = &gcrt[i];

	/* Get things on board */
	error = gctl_copyin(req);
	if (error)
		return (error);

	if (g_debugflags & G_F_CTLDUMP)
		gctl_dump(req);
	g_topology_lock();
	switch (req->request) {
	case GCTL_CREATE_GEOM:
		error = gctl_create_geom(req);
		break;
	case GCTL_DESTROY_GEOM:
		error = gctl_destroy_geom(req);
		break;
	default:
		error = gctl_error(req, "XXX: TBD");
		break;
	}
	g_topology_unlock();
	return (error);
}

static int
g_ctl_ioctl(dev_t dev, u_long cmd, caddr_t data, int fflag, struct thread *td)
{
	int error;

	switch(cmd) {
	case GEOM_CTL:
		DROP_GIANT();
		error = g_ctl_ioctl_ctl(dev, cmd, data, fflag, td);
		PICKUP_GIANT();
		break;
	default:
		error = ENOTTY;
		break;
	}
	return (error);

}
