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
#ifndef _LIBGEOM_H_
#define _LIBGEOM_H_

#include <sys/queue.h>
#include <sys/time.h>

#include <geom/geom_ctl.h>

void geom_stats_close(void);
void geom_stats_resync(void);
int geom_stats_open(void);
void *geom_stats_snapshot_get(void);
void geom_stats_snapshot_free(void *arg);
void geom_stats_snapshot_timestamp(void *arg, struct timespec *tp);
void geom_stats_snapshot_reset(void *arg);
struct devstat *geom_stats_snapshot_next(void *arg);

char *geom_getxml(void);

/* geom_xml2tree.c */

/*
 * These structs are used to build the tree based on the XML.
 * they're named as the kernel variant without the first '_'.
 */

struct gclass;
struct ggeom;
struct gconsumer;
struct gprovider;

LIST_HEAD(gconf, gconfig);

struct gident {
	void			*id;
	void			*ptr;
	enum {	ISCLASS,
		ISGEOM,
		ISPROVIDER,
		ISCONSUMER }	what;
};

struct gmesh {
	LIST_HEAD(, gclass)	class;
	struct gident		*ident;
};

struct gconfig {
	LIST_ENTRY(gconfig)	config;
	char			*name;
	char			*val;
};

struct gclass {
	void			*id;
	char			*name;
	LIST_ENTRY(gclass)	class;
	LIST_HEAD(, ggeom)	geom;
	struct gconf		config;
};

struct ggeom {
	void			*id;
	struct gclass		*class;
	char			*name;
	u_int			rank;
	LIST_ENTRY(ggeom)	geom;
	LIST_HEAD(, gconsumer)	consumer;
	LIST_HEAD(, gprovider)	provider;
	struct gconf		config;
};

struct gconsumer {
	void			*id;
	struct ggeom		*geom;
	LIST_ENTRY(gconsumer)	consumer;
	struct gprovider	*provider;
	LIST_ENTRY(gconsumer)	consumers;
	char			*mode;
	struct gconf		config;
};

struct gprovider {
	void			*id;
	char			*name;
	struct ggeom		*geom;
	LIST_ENTRY(gprovider)	provider;
	LIST_HEAD(, gconsumer)	consumers;
	char			*mode;
	off_t			mediasize;
	u_int			sectorsize;
	struct gconf		config;
};

struct gident * geom_lookupid(struct gmesh *gmp, const void *id);
int geom_xml2tree(struct gmesh *gmp, char *p);
int geom_gettree(struct gmesh *gmp);
void geom_deletetree(struct gmesh *gmp);

/* geom_ctl.c */

struct gctl_req;

#ifdef _STDIO_H_			/* limit #include pollution */
void gctl_dump(struct gctl_req *req, FILE *f);
#endif
void gctl_free(struct gctl_req *req);
struct gctl_req *gctl_get_handle(void);
const char *gctl_issue(struct gctl_req *req);
void gctl_ro_param(struct gctl_req *req, const char *name, int len, const void* val);
void gctl_rw_param(struct gctl_req *req, const char *name, int len, void* val);

#endif /* _LIBGEOM_H_ */
