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
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <paths.h>

#include <sys/queue.h>

#define GCTL_TABLE 1
#include <libgeom.h>

void
gctl_dump(struct gctl_req *req, FILE *f)
{
	u_int i;
	int j;
	struct gctl_req_arg *ap;

	if (req == NULL) {
		fprintf(f, "Dump of gctl request at NULL\n");
		return;
	}
	fprintf(f, "Dump of gctl request at %p:\n", req);
	if (req->error != NULL)
		fprintf(f, "  error:\t\"%s\"\n", req->error);
	else
		fprintf(f, "  error:\tNULL\n");
	for (i = 0; i < req->narg; i++) {
		ap = &req->arg[i];
		fprintf(f, "  param:\t\"%s\" (%d)", ap->name, ap->nlen);
		fprintf(f, " [%s%s",
		    ap->flag & GCTL_PARAM_RD ? "R" : "",
		    ap->flag & GCTL_PARAM_WR ? "W" : "");
		fflush(f);
		if (ap->flag & GCTL_PARAM_ASCII)
			fprintf(f, "%d] = \"%s\"", ap->len, (char *)ap->value);
		else if (ap->len > 0) {
			fprintf(f, "%d] = ", ap->len);
			fflush(f);
			for (j = 0; j < ap->len; j++) {
				fprintf(f, " %02x", ((u_char *)ap->value)[j]);
			}
		} else {
			fprintf(f, "0] = %p", ap->value);
		}
		fprintf(f, "\n");
	}
}

/*
 * Set an error message, if one does not already exist.
 */
static void
gctl_set_error(struct gctl_req *req, const char *error, ...)
{
	va_list ap;

	if (req->error != NULL)
		return;
	va_start(ap, error);
	vasprintf(&req->error, error, ap);
	va_end(ap);
}

/*
 * Check that a malloc operation succeeded, and set a consistent error
 * message if not.
 */
static void
gctl_check_alloc(struct gctl_req *req, void *ptr)
{
	if (ptr != NULL)
		return;
	gctl_set_error(req, "Could not allocate memory");
	if (req->error == NULL)
		req->error = "Could not allocate memory";
}

/*
 * Allocate a new request handle of the specified type.
 * XXX: Why bother checking the type ?
 */
struct gctl_req *
gctl_get_handle(void)
{
	struct gctl_req *rp;

	rp = calloc(1, sizeof *rp);
	return (rp);
}

/*
 * Allocate space for another argument.
 */
static struct gctl_req_arg *
gctl_new_arg(struct gctl_req *req)
{
	struct gctl_req_arg *ap;

	req->narg++;
	req->arg = realloc(req->arg, sizeof *ap * req->narg);
	gctl_check_alloc(req, req->arg);
	if (req->arg == NULL) {
		req->narg = 0;
		return (NULL);
	}
	ap = req->arg + (req->narg - 1);
	memset(ap, 0, sizeof *ap);
	return (ap);
}

void
gctl_ro_param(struct gctl_req *req, const char *name, int len, const void* value)
{
	struct gctl_req_arg *ap;

	if (req == NULL || req->error != NULL)
		return;
	ap = gctl_new_arg(req);
	if (ap == NULL)
		return;
	ap->name = strdup(name);
	gctl_check_alloc(req, ap->name);
	ap->nlen = strlen(ap->name) + 1;
	ap->value = __DECONST(void *, value);
	ap->flag = GCTL_PARAM_RD;
	if (len >= 0)
		ap->len = len;
	else if (len < 0) {
		ap->flag |= GCTL_PARAM_ASCII;
		ap->len = strlen(value) + 1;	
	}
}

void
gctl_rw_param(struct gctl_req *req, const char *name, int len, void* value)
{
	struct gctl_req_arg *ap;

	if (req == NULL || req->error != NULL)
		return;
	ap = gctl_new_arg(req);
	if (ap == NULL)
		return;
	ap->name = strdup(name);
	gctl_check_alloc(req, ap->name);
	ap->nlen = strlen(ap->name) + 1;
	ap->value = value;
	ap->flag = GCTL_PARAM_RW;
	if (len >= 0)
		ap->len = len;
	else if (len < 0)
		ap->len = strlen(value) + 1;	
}

const char *
gctl_issue(struct gctl_req *req)
{
	int fd, error;

	if (req == NULL)
		return ("NULL request pointer");
	if (req->error != NULL)
		return (req->error);

	req->version = GCTL_VERSION;
	req->lerror = BUFSIZ;		/* XXX: arbitrary number */
	req->error = malloc(req->lerror);
	if (req->error == NULL) {
		gctl_check_alloc(req, req->error);
		return (req->error);
	}
	memset(req->error, 0, req->lerror);
	req->lerror--;
	fd = open(_PATH_DEV PATH_GEOM_CTL, O_RDONLY);
	if (fd < 0)
		return(strerror(errno));
	error = ioctl(fd, GEOM_CTL, req);
	close(fd);
	if (req->error[0] != '\0')
		return (req->error);
	if (error != 0)
		return(strerror(errno));
	return (NULL);
}

void
gctl_free(struct gctl_req *req)
{
	u_int i;

	if (req == NULL)
		return;
	for (i = 0; i < req->narg; i++) {
		if (req->arg[i].name != NULL)
			free(req->arg[i].name);
	}
	free(req->arg);
	if (req->error != NULL)
		free(req->error);
	free(req);
}
