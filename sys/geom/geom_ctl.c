/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2002 Poul-Henning Kamp
 * Copyright (c) 2002 Networks Associates Technology, Inc.
 * All rights reserved.
 * Copyright (c) 2022 Alexander Motin <mav@FreeBSD.org>
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
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/sbuf.h>
#include <sys/stdarg.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>

#include <geom/geom.h>
#include <geom/geom_int.h>
#define GCTL_TABLE 1
#include <geom/geom_ctl.h>

static d_ioctl_t g_ctl_ioctl;

static struct cdevsw g_ctl_cdevsw = {
	.d_version =	D_VERSION,
	.d_flags =	0,
	.d_ioctl =	g_ctl_ioctl,
	.d_name =	"g_ctl",
};

CTASSERT(GCTL_PARAM_RD == VM_PROT_READ);
CTASSERT(GCTL_PARAM_WR == VM_PROT_WRITE);

void
g_ctl_init(void)
{

	make_dev_credf(MAKEDEV_ETERNAL, &g_ctl_cdevsw, 0, NULL,
	    UID_ROOT, GID_OPERATOR, 0640, PATH_GEOM_CTL);
}

/*
 * Report an error back to the user in ascii format.  Return nerror
 * or EINVAL if nerror isn't specified.
 */
int
gctl_error(struct gctl_req *req, const char *fmt, ...)
{
	va_list ap;

	if (req == NULL)
		return (EINVAL);

	/* We only record the first error */
	if (sbuf_done(req->serror)) {
		if (!req->nerror)
			req->nerror = EEXIST;
#ifdef DIAGNOSTIC
		printf("gctl_error: buffer closed, message discarded.\n");
#endif
		return (req->nerror);
	}
	if (!req->nerror)
		req->nerror = EINVAL;

	/* If this is the last of several messages, indent it on a new line */
	if (sbuf_len(req->serror) > 0)
		sbuf_cat(req->serror, "\n\t");
	va_start(ap, fmt);
	sbuf_vprintf(req->serror, fmt, ap);
	va_end(ap);
	gctl_post_messages(req);
	return (req->nerror);
}

/*
 * The gctl_error() function will only report a single message.
 * Commands that handle multiple devices may want to report a
 * message for each of the devices. The gctl_msg() function
 * can be called multiple times to post messages. When done
 * the application must either call gctl_post_messages() or
 * call gctl_error() to cause the messages to be reported to
 * the calling process.
 *
 * The errno argument should be zero if it is an informational
 * message or an errno value (EINVAL, EBUSY, etc) if it is an error.
 * If any of the messages has a non-zero errno, the utility will
 * EXIT_FAILURE. If only informational messages (with zero errno)
 * are posted, the utility will EXIT_SUCCESS.
 */
void
gctl_msg(struct gctl_req *req, int errno, const char *fmt, ...)
{
	va_list ap;

	if (req == NULL)
		return;
	if (sbuf_done(req->serror)) {
#ifdef DIAGNOSTIC
		printf("gctl_msg: buffer closed, message discarded.\n");
#endif
		return;
	}
	if (req->nerror == 0)
		req->nerror = errno;
	/* Put second and later messages indented on a new line */
	if (sbuf_len(req->serror) > 0)
		sbuf_cat(req->serror, "\n\t");
	va_start(ap, fmt);
	sbuf_vprintf(req->serror, fmt, ap);
	va_end(ap);
}

/*
 * Post the messages to the user.
 */
void
gctl_post_messages(struct gctl_req *req)
{

	if (sbuf_done(req->serror)) {
#ifdef DIAGNOSTIC
		printf("gctl_post_messages: message buffer already closed.\n");
#endif
		return;
	}
	sbuf_finish(req->serror);
	if (g_debugflags & G_F_CTLDUMP)
		printf("gctl %p message(s) \"%s\"\n", req,
		    sbuf_data(req->serror));
}

/*
 * Allocate space and copyin() something.
 * XXX: this should really be a standard function in the kernel.
 */
static void *
geom_alloc_copyin(struct gctl_req *req, void *uaddr, size_t len)
{
	void *ptr;

	ptr = g_malloc(len, M_WAITOK);
	req->nerror = copyin(uaddr, ptr, len);
	if (!req->nerror)
		return (ptr);
	g_free(ptr);
	return (NULL);
}

static void
gctl_copyin(struct gctl_req *req)
{
	struct gctl_req_arg *ap;
	char *p;
	u_int i;

	if (req->narg > GEOM_CTL_ARG_MAX) {
		gctl_error(req, "too many arguments");
		req->arg = NULL;
		return;
	}

	ap = geom_alloc_copyin(req, req->arg, req->narg * sizeof(*ap));
	if (ap == NULL) {
		gctl_error(req, "bad control request");
		req->arg = NULL;
		return;
	}

	/* Nothing have been copyin()'ed yet */
	for (i = 0; i < req->narg; i++) {
		ap[i].flag &= ~(GCTL_PARAM_NAMEKERNEL|GCTL_PARAM_VALUEKERNEL);
		ap[i].flag &= ~GCTL_PARAM_CHANGED;
		ap[i].kvalue = NULL;
	}

	for (i = 0; i < req->narg; i++) {
		if (ap[i].nlen < 1 || ap[i].nlen > SPECNAMELEN) {
			gctl_error(req,
			    "wrong param name length %d: %d", i, ap[i].nlen);
			break;
		}
		p = geom_alloc_copyin(req, ap[i].name, ap[i].nlen);
		if (p == NULL)
			break;
		if (p[ap[i].nlen - 1] != '\0') {
			gctl_error(req, "unterminated param name");
			g_free(p);
			break;
		}
		ap[i].name = p;
		ap[i].flag |= GCTL_PARAM_NAMEKERNEL;
		if (ap[i].len <= 0) {
			gctl_error(req, "negative param length");
			break;
		}
		if (ap[i].flag & GCTL_PARAM_RD) {
			p = geom_alloc_copyin(req, ap[i].value, ap[i].len);
			if (p == NULL)
				break;
			if ((ap[i].flag & GCTL_PARAM_ASCII) &&
			    p[ap[i].len - 1] != '\0') {
				gctl_error(req, "unterminated param value");
				g_free(p);
				break;
			}
		} else {
			p = g_malloc(ap[i].len, M_WAITOK | M_ZERO);
		}
		ap[i].kvalue = p;
		ap[i].flag |= GCTL_PARAM_VALUEKERNEL;
	}
	req->arg = ap;
	return;
}

static void
gctl_copyout(struct gctl_req *req)
{
	int error, i;
	struct gctl_req_arg *ap;

	if (req->nerror)
		return;
	error = 0;
	ap = req->arg;
	for (i = 0; i < req->narg; i++, ap++) {
		if (!(ap->flag & GCTL_PARAM_CHANGED))
			continue;
		error = copyout(ap->kvalue, ap->value, ap->len);
		if (!error)
			continue;
		req->nerror = error;
		return;
	}
	return;
}

static void
gctl_free(struct gctl_req *req)
{
	u_int i;

	sbuf_delete(req->serror);
	if (req->arg == NULL)
		return;
	for (i = 0; i < req->narg; i++) {
		if (req->arg[i].flag & GCTL_PARAM_NAMEKERNEL)
			g_free(req->arg[i].name);
		if ((req->arg[i].flag & GCTL_PARAM_VALUEKERNEL) &&
		    req->arg[i].len > 0)
			g_free(req->arg[i].kvalue);
	}
	g_free(req->arg);
}

static void
gctl_dump(struct gctl_req *req, const char *what)
{
	struct gctl_req_arg *ap;
	u_int i;
	int j;

	printf("Dump of gctl %s at %p:\n", what, req);
	if (req->nerror > 0) {
		printf("  nerror:\t%d\n", req->nerror);
		if (sbuf_len(req->serror) > 0)
			printf("  error:\t\"%s\"\n", sbuf_data(req->serror));
	}
	if (req->arg == NULL)
		return;
	for (i = 0; i < req->narg; i++) {
		ap = &req->arg[i];
		if (!(ap->flag & GCTL_PARAM_NAMEKERNEL))
			printf("  param:\t%d@%p", ap->nlen, ap->name);
		else
			printf("  param:\t\"%s\"", ap->name);
		printf(" [%s%s%d] = ",
		    ap->flag & GCTL_PARAM_RD ? "R" : "",
		    ap->flag & GCTL_PARAM_WR ? "W" : "",
		    ap->len);
		if (!(ap->flag & GCTL_PARAM_VALUEKERNEL)) {
			printf(" =@ %p", ap->value);
		} else if (ap->flag & GCTL_PARAM_ASCII) {
			printf("\"%s\"", (char *)ap->kvalue);
		} else if (ap->len > 0) {
			for (j = 0; j < ap->len && j < 512; j++)
				printf(" %02x", ((u_char *)ap->kvalue)[j]);
		} else {
			printf(" = %p", ap->kvalue);
		}
		printf("\n");
	}
}

int
gctl_set_param(struct gctl_req *req, const char *param, void const *ptr,
    int len)
{
	u_int i;
	struct gctl_req_arg *ap;

	for (i = 0; i < req->narg; i++) {
		ap = &req->arg[i];
		if (strcmp(param, ap->name))
			continue;
		if (!(ap->flag & GCTL_PARAM_WR))
			return (EPERM);
		ap->flag |= GCTL_PARAM_CHANGED;
		if (ap->len < len) {
			bcopy(ptr, ap->kvalue, ap->len);
			return (ENOSPC);
		}
		bcopy(ptr, ap->kvalue, len);
		return (0);
	}
	return (EINVAL);
}

void
gctl_set_param_err(struct gctl_req *req, const char *param, void const *ptr,
    int len)
{

	switch (gctl_set_param(req, param, ptr, len)) {
	case EPERM:
		gctl_error(req, "No write access %s argument", param);
		break;
	case ENOSPC:
		gctl_error(req, "Wrong length %s argument", param);
		break;
	case EINVAL:
		gctl_error(req, "Missing %s argument", param);
		break;
	}
}

void *
gctl_get_param_flags(struct gctl_req *req, const char *param, int flags, int *len)
{
	u_int i;
	void *p;
	struct gctl_req_arg *ap;

	for (i = 0; i < req->narg; i++) {
		ap = &req->arg[i];
		if (strcmp(param, ap->name))
			continue;
		if ((ap->flag & flags) != flags)
			continue;
		p = ap->kvalue;
		if (len != NULL)
			*len = ap->len;
		return (p);
	}
	return (NULL);
}

void *
gctl_get_param(struct gctl_req *req, const char *param, int *len)
{

	return (gctl_get_param_flags(req, param, GCTL_PARAM_RD, len));
}

char const *
gctl_get_asciiparam(struct gctl_req *req, const char *param)
{
	char const *p;
	int len;

	p = gctl_get_param_flags(req, param, GCTL_PARAM_RD, &len);
	if (p == NULL)
		return (NULL);
	if (len < 1) {
		gctl_error(req, "Argument without length (%s)", param);
		return (NULL);
	}
	if (p[len - 1] != '\0') {
		gctl_error(req, "Unterminated argument (%s)", param);
		return (NULL);
	}
	return (p);
}

void *
gctl_get_paraml_opt(struct gctl_req *req, const char *param, int len)
{
	int i;
	void *p;

	p = gctl_get_param(req, param, &i);
	if (i != len) {
		p = NULL;
		gctl_error(req, "Wrong length %s argument", param);
	}
	return (p);
}

void *
gctl_get_paraml(struct gctl_req *req, const char *param, int len)
{
	void *p;

	p = gctl_get_paraml_opt(req, param, len);
	if (p == NULL)
		gctl_error(req, "Missing %s argument", param);
	return (p);
}

struct g_class *
gctl_get_class(struct gctl_req *req, char const *arg)
{
	char const *p;
	struct g_class *cp;

	p = gctl_get_asciiparam(req, arg);
	if (p == NULL) {
		gctl_error(req, "Missing %s argument", arg);
		return (NULL);
	}
	LIST_FOREACH(cp, &g_classes, class) {
		if (!strcmp(p, cp->name))
			return (cp);
	}
	gctl_error(req, "Class not found: \"%s\"", p);
	return (NULL);
}

struct g_geom *
gctl_get_geom(struct gctl_req *req, struct g_class *mp, char const *arg)
{
	char const *p;
	struct g_geom *gp;

	MPASS(mp != NULL);
	p = gctl_get_asciiparam(req, arg);
	if (p == NULL) {
		gctl_error(req, "Missing %s argument", arg);
		return (NULL);
	}
	LIST_FOREACH(gp, &mp->geom, geom)
		if (!strcmp(p, gp->name))
			return (gp);
	gctl_error(req, "Geom not found: \"%s\"", p);
	return (NULL);
}

struct g_provider *
gctl_get_provider(struct gctl_req *req, char const *arg)
{
	char const *p;
	struct g_provider *pp;

	p = gctl_get_asciiparam(req, arg);
	if (p == NULL) {
		gctl_error(req, "Missing '%s' argument", arg);
		return (NULL);
	}
	pp = g_provider_by_name(p);
	if (pp != NULL)
		return (pp);
	gctl_error(req, "Provider not found: \"%s\"", p);
	return (NULL);
}

static void
g_ctl_getxml(struct gctl_req *req, struct g_class *mp)
{
	const char *name;
	char *buf;
	struct sbuf *sb;
	int len, i = 0, n = 0, *parents;
	struct g_geom *gp, **gps;
	struct g_consumer *cp;

	parents = gctl_get_paraml(req, "parents", sizeof(*parents));
	if (parents == NULL)
		return;
	name = gctl_get_asciiparam(req, "arg0");
	n = 0;
	LIST_FOREACH(gp, &mp->geom, geom) {
		if (name && strcmp(gp->name, name) != 0)
			continue;
		n++;
		if (*parents) {
			LIST_FOREACH(cp, &gp->consumer, consumer)
				n++;
		}
	}
	gps = g_malloc((n + 1) * sizeof(*gps), M_WAITOK);
	i = 0;
	LIST_FOREACH(gp, &mp->geom, geom) {
		if (name && strcmp(gp->name, name) != 0)
			continue;
		gps[i++] = gp;
		if (*parents) {
			LIST_FOREACH(cp, &gp->consumer, consumer) {
				if (cp->provider != NULL)
					gps[i++] = cp->provider->geom;
			}
		}
	}
	KASSERT(i == n, ("different number of geoms found (%d != %d)",
	    i, n));
	gps[i] = 0;

	buf = gctl_get_param_flags(req, "output", GCTL_PARAM_WR, &len);
	if (buf == NULL) {
		gctl_error(req, "output parameter missing");
		g_free(gps);
		return;
	}
	sb = sbuf_new(NULL, buf, len, SBUF_FIXEDLEN | SBUF_INCLUDENUL);
	g_conf_specific(sb, gps);
	gctl_set_param(req, "output", buf, 0);
	if (sbuf_error(sb))
		gctl_error(req, "output buffer overflow");
	sbuf_delete(sb);
	g_free(gps);
}

static void
g_ctl_req(void *arg, int flag __unused)
{
	struct g_class *mp;
	struct gctl_req *req;
	char const *verb;

	g_topology_assert();
	req = arg;
	mp = gctl_get_class(req, "class");
	if (mp == NULL)
		return;
	verb = gctl_get_param(req, "verb", NULL);
	if (verb == NULL) {
		gctl_error(req, "Verb missing");
		return;
	}
	if (strcmp(verb, "getxml") == 0) {
		g_ctl_getxml(req, mp);
	} else if (mp->ctlreq == NULL) {
		gctl_error(req, "Class takes no requests");
	} else {
		mp->ctlreq(req, mp, verb);
	}
	g_topology_assert();
}

static int
g_ctl_ioctl_ctl(struct cdev *dev, u_long cmd, caddr_t data, int fflag, struct thread *td)
{
	struct gctl_req *req;
	int nerror;

	req = (void *)data;
	req->nerror = 0;
	/* It is an error if we cannot return an error text */
	if (req->lerror < 2)
		return (EINVAL);
	if (!useracc(req->error, req->lerror, VM_PROT_WRITE))
		return (EINVAL);

	req->serror = sbuf_new_auto();
	/* Check the version */
	if (req->version != GCTL_VERSION) {
		gctl_error(req, "kernel and libgeom version mismatch.");
		req->arg = NULL;
	} else {
		/* Get things on board */
		gctl_copyin(req);

		if (g_debugflags & G_F_CTLDUMP)
			gctl_dump(req, "request");

		if (!req->nerror) {
			g_waitfor_event(g_ctl_req, req, M_WAITOK, NULL);

			if (g_debugflags & G_F_CTLDUMP)
				gctl_dump(req, "result");

			gctl_copyout(req);
		}
	}
	if (sbuf_done(req->serror)) {
		nerror = copyout(sbuf_data(req->serror), req->error,
		    imin(req->lerror, sbuf_len(req->serror) + 1));
		if (nerror != 0 && req->nerror == 0)
			req->nerror = nerror;
	}

	nerror = req->nerror;
	gctl_free(req);
	return (nerror);
}

static int
g_ctl_ioctl(struct cdev *dev, u_long cmd, caddr_t data, int fflag, struct thread *td)
{
	int error;

	switch(cmd) {
	case GEOM_CTL:
		error = g_ctl_ioctl_ctl(dev, cmd, data, fflag, td);
		break;
	default:
		error = ENOIOCTL;
		break;
	}
	return (error);

}
