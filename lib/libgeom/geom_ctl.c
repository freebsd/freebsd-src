/*-
 * Copyright (c) 2003 Poul-Henning Kamp
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

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include <sys/types.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <paths.h>

#include <sys/queue.h>

#define GEOM_CTL_TABLE 1
#include <libgeom.h>

#include <geom/geom_ext.h>

void
geom_ctl_dump(struct geom_ctl_req *req, FILE *f)
{
	u_int i;
	int j;
	struct geom_ctl_req_arg *ap;

	if (req == NULL) {
		fprintf(f, "Dump of geom_ctl request at NULL\n");
		return;
	}
	fprintf(f, "Dump of geom_ctl %s request at %p:\n", req->reqt->name, req);
	if (req->error != NULL)
		fprintf(f, "  error:\t\"%s\"\n", req->error);
	else
		fprintf(f, "  error:\tNULL\n");
	for (i = 0; i < req->narg; i++) {
		ap = &req->arg[i];
		if (ap->name != NULL)
			fprintf(f, "  param:\t\"%s\"", ap->name);
		else
			fprintf(f, "  meta:\t@%jd", (intmax_t)ap->offset);
		fflush(f);
		if (ap->len < 0)
			fprintf(f, " = [%d] \"%s\"", -ap->len, (char *)ap->value);
		else if (ap->len > 0) {
			fprintf(f, " = [%d]", ap->len);
			fflush(f);
			for (j = 0; j < ap->len; j++) {
				fprintf(f, " %02x", ((u_char *)ap->value)[j]);
			}
		} else {
			fprintf(f, " = [0] %p", ap->value);
		}
		fprintf(f, "\n");
	}
}

static void
geom_ctl_set_error(struct geom_ctl_req *req, const char *error, ...)
{
	va_list ap;

	if (req->error != NULL)
		return;
	va_start(ap, error);
	vasprintf(&req->error, error, ap);
}

static void
geom_ctl_check_alloc(struct geom_ctl_req *req, void *ptr)
{
	if (ptr != NULL)
		return;
	geom_ctl_set_error(req, "Could not allocate memory");
}

struct geom_ctl_req *
geom_ctl_get_handle(enum geom_ctl_request req)
{
	struct geom_ctl_req_table *gtp;
	struct geom_ctl_req *rp;

	rp = calloc(1, sizeof *rp);
	if (rp == NULL)
		return (NULL);
	for (gtp = gcrt; gtp->request != req; gtp++)
		if (gtp->request == GEOM_INVALID_REQUEST)
			break;

	rp->request = req;
	rp->reqt = gtp;
	if (rp->reqt->request == GEOM_INVALID_REQUEST)
		geom_ctl_set_error(rp, "Invalid request");
	return (rp);
}

void
geom_ctl_set_param(struct geom_ctl_req *req, const char *name, int len, void* value)
{
	struct geom_ctl_req_arg *ap;

	if (req == NULL || req->error != NULL)
		return;
	if (req->reqt->params == 0)
		geom_ctl_set_error(req, "Request takes no parameters");
	req->narg++;
	req->arg = realloc(req->arg, sizeof *ap * req->narg);
	geom_ctl_check_alloc(req, req->arg);
	if (req->arg != NULL) {
		ap = req->arg + (req->narg - 1);
		memset(ap, 0, sizeof *ap);
		ap->name = strdup(name);
		geom_ctl_check_alloc(req, ap->name);
		ap->nlen = strlen(ap->name);
		ap->len = len;
		if (len > 0) {
			ap->value = value;
		} else if (len < 0) {
			ap->len = -strlen(value);	
			ap->value = strdup(value);
		} else {
			ap->value = value;
		}
		if (len != 0)
			geom_ctl_check_alloc(req, ap->value);
	} else {
		req->narg = 0;
	}
}

void
geom_ctl_set_meta(struct geom_ctl_req *req, off_t offset, u_int len, void* value)
{
	struct geom_ctl_req_arg *ap;
	u_int i;

	if (req == NULL || req->error != NULL)
		return;
	if (req->reqt->meta == 0)
		geom_ctl_set_error(req, "Request takes no meta data");
	for (i = 0; i < req->narg; i++) {
		ap = &req->arg[i];
		if (ap->name != NULL)
			continue;
		if (ap->offset >= offset + len)
			continue;
		if (ap->offset + ap->len <= offset)
			continue;
		geom_ctl_set_error(req, "Overlapping meta data");
		return;
	}
	req->narg++;
	req->arg = realloc(req->arg, sizeof *ap * req->narg);
	geom_ctl_check_alloc(req, req->arg);
	if (req->arg != NULL) {
		ap = req->arg + (req->narg - 1);
		memset(ap, 0, sizeof *ap);
		ap->value = value;
		ap->offset = offset;
		ap->len = len;
	} else {
		req->narg = 0;
	}
}

const char *
geom_ctl_issue(struct geom_ctl_req *req)
{
	int fd, error;

	if (req == NULL)
		return ("NULL request pointer");
	if (req->error != NULL)
		return (req->error);

	req->version = GEOM_CTL_VERSION;
	req->lerror = BUFSIZ;		/* XXX: arbitrary number */
	req->error = malloc(req->lerror);
	memset(req->error, 0, req->lerror);
	req->lerror--;
	fd = open(_PATH_DEV PATH_GEOM_CTL, O_RDONLY);
	if (fd < 0)
		return(strerror(errno));
	error = ioctl(fd, GEOM_CTL, req);
	if (error && errno == EINVAL && req->error[0] != '\0')
		return (req->error);
	if (error != 0)
		return(strerror(errno));
	return (NULL);
}

void
geom_ctl_free(struct geom_ctl_req *req)
{
	u_int i;

	for (i = 0; i < req->narg; i++) {
		if (req->arg[i].name != NULL)
			free(req->arg[i].name);
		if (req->arg[i].len < 0)
			free(req->arg[i].value);
	}
	if (req->error != NULL)
		free(req->error);
	free(req->arg);
	free(req);
}

