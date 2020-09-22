/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (C) 2018 Universita` di Pisa
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <net/netmap_user.h>
#define LIBNETMAP_NOTHREADSAFE
#include "libnetmap.h"

struct nmport_cleanup_d {
	struct nmport_cleanup_d *next;
	void (*cleanup)(struct nmport_cleanup_d *, struct nmport_d *);
};

static void
nmport_push_cleanup(struct nmport_d *d, struct nmport_cleanup_d *c)
{
	c->next = d->clist;
	d->clist = c;
}

static void
nmport_pop_cleanup(struct nmport_d *d)
{
	struct nmport_cleanup_d *top;

	top = d->clist;
	d->clist = d->clist->next;
	(*top->cleanup)(top, d);
	nmctx_free(d->ctx, top);
}

void nmport_do_cleanup(struct nmport_d *d)
{
	while (d->clist != NULL) {
		nmport_pop_cleanup(d);
	}
}

static struct nmport_d *
nmport_new_with_ctx(struct nmctx *ctx)
{
	struct nmport_d *d;

	/* allocate a descriptor */
	d = nmctx_malloc(ctx, sizeof(*d));
	if (d == NULL) {
		nmctx_ferror(ctx, "cannot allocate nmport descriptor");
		goto out;
	}
	memset(d, 0, sizeof(*d));

	nmreq_header_init(&d->hdr, NETMAP_REQ_REGISTER, &d->reg);

	d->ctx = ctx;
	d->fd = -1;

out:
	return d;
}

struct nmport_d *
nmport_new(void)
{
	struct nmctx *ctx = nmctx_get();
	return nmport_new_with_ctx(ctx);
}


void
nmport_delete(struct nmport_d *d)
{
	nmctx_free(d->ctx, d);
}

void
nmport_extmem_cleanup(struct nmport_cleanup_d *c, struct nmport_d *d)
{
	(void)c;

	if (d->extmem == NULL)
		return;

	nmreq_remove_option(&d->hdr, &d->extmem->nro_opt);
	nmctx_free(d->ctx, d->extmem);
	d->extmem = NULL;
}


int
nmport_extmem(struct nmport_d *d, void *base, size_t size)
{
	struct nmctx *ctx = d->ctx;
	struct nmport_cleanup_d *clnup = NULL;

	if (d->register_done) {
		nmctx_ferror(ctx, "%s: cannot set extmem of an already registered port", d->hdr.nr_name);
		errno = EINVAL;
		return -1;
	}

	if (d->extmem != NULL) {
		nmctx_ferror(ctx, "%s: extmem already in use", d->hdr.nr_name);
		errno = EINVAL;
		return -1;
	}

	clnup = (struct nmport_cleanup_d *)nmctx_malloc(ctx, sizeof(*clnup));
	if (clnup == NULL) {
		nmctx_ferror(ctx, "failed to allocate cleanup descriptor");
		errno = ENOMEM;
		return -1;
	}

	d->extmem = nmctx_malloc(ctx, sizeof(*d->extmem));
	if (d->extmem == NULL) {
		nmctx_ferror(ctx, "%s: cannot allocate extmem option", d->hdr.nr_name);
		nmctx_free(ctx, clnup);
		errno = ENOMEM;
		return -1;
	}
	memset(d->extmem, 0, sizeof(*d->extmem));
	d->extmem->nro_usrptr = (uintptr_t)base;
	d->extmem->nro_opt.nro_reqtype = NETMAP_REQ_OPT_EXTMEM;
	d->extmem->nro_info.nr_memsize = size;
	nmreq_push_option(&d->hdr, &d->extmem->nro_opt);

	clnup->cleanup = nmport_extmem_cleanup;
	nmport_push_cleanup(d, clnup);

	return 0;
}

struct nmport_extmem_from_file_cleanup_d {
	struct nmport_cleanup_d up;
	void *p;
	size_t size;
};

void nmport_extmem_from_file_cleanup(struct nmport_cleanup_d *c,
		struct nmport_d *d)
{
	struct nmport_extmem_from_file_cleanup_d *cc =
		(struct nmport_extmem_from_file_cleanup_d *)c;

	munmap(cc->p, cc->size);
}

int
nmport_extmem_from_file(struct nmport_d *d, const char *fname)
{
	struct nmctx *ctx = d->ctx;
	int fd = -1;
	off_t mapsize;
	void *p;
	struct nmport_extmem_from_file_cleanup_d *clnup = NULL;

	clnup = nmctx_malloc(ctx, sizeof(*clnup));
	if (clnup == NULL) {
		nmctx_ferror(ctx, "cannot allocate cleanup descriptor");
		errno = ENOMEM;
		goto fail;
	}

	fd = open(fname, O_RDWR);
	if (fd < 0) {
		nmctx_ferror(ctx, "cannot open '%s': %s", fname, strerror(errno));
		goto fail;
	}
	mapsize = lseek(fd, 0, SEEK_END);
	if (mapsize < 0) {
		nmctx_ferror(ctx, "failed to obtain filesize of '%s': %s", fname, strerror(errno));
		goto fail;
	}
	p = mmap(0, mapsize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (p == MAP_FAILED) {
		nmctx_ferror(ctx, "cannot mmap '%s': %s", fname, strerror(errno));
		goto fail;
	}
	close(fd);

	clnup->p = p;
	clnup->size = mapsize;
	clnup->up.cleanup = nmport_extmem_from_file_cleanup;
	nmport_push_cleanup(d, &clnup->up);

	if (nmport_extmem(d, p, mapsize) < 0)
		goto fail;

	return 0;

fail:
	if (fd >= 0)
		close(fd);
	if (clnup != NULL) {
		if (clnup->p != MAP_FAILED)
			nmport_pop_cleanup(d);
		else
			nmctx_free(ctx, clnup);
	}
	return -1;
}

struct nmreq_pools_info*
nmport_extmem_getinfo(struct nmport_d *d)
{
	if (d->extmem == NULL)
		return NULL;
	return &d->extmem->nro_info;
}

/* head of the list of options */
static struct nmreq_opt_parser *nmport_opt_parsers;

#define NPOPT_PARSER(o)		nmport_opt_##o##_parser
#define NPOPT_DESC(o)		nmport_opt_##o##_desc
#define NPOPT_NRKEYS(o)		(NPOPT_DESC(o).nr_keys)
#define NPOPT_DECL(o, f)						\
static int NPOPT_PARSER(o)(struct nmreq_parse_ctx *);			\
static struct nmreq_opt_parser NPOPT_DESC(o) = {			\
	.prefix = #o,							\
	.parse = NPOPT_PARSER(o),					\
	.flags = (f),							\
	.default_key = -1,						\
	.nr_keys = 0,							\
	.next = NULL,							\
};									\
static void __attribute__((constructor))				\
nmport_opt_##o##_ctor(void)						\
{									\
	NPOPT_DESC(o).next = nmport_opt_parsers;			\
	nmport_opt_parsers = &NPOPT_DESC(o);				\
}
struct nmport_key_desc {
	struct nmreq_opt_parser *option;
	const char *key;
	unsigned int flags;
	int id;
};
static void
nmport_opt_key_ctor(struct nmport_key_desc *k)
{
	struct nmreq_opt_parser *o = k->option;
	struct nmreq_opt_key *ok;

	k->id = o->nr_keys;
	ok = &o->keys[k->id];
	ok->key = k->key;
	ok->id = k->id;
	ok->flags = k->flags;
	o->nr_keys++;
	if (ok->flags & NMREQ_OPTK_DEFAULT)
		o->default_key = ok->id;
}
#define NPKEY_DESC(o, k)	nmport_opt_##o##_key_##k##_desc
#define NPKEY_ID(o, k)		(NPKEY_DESC(o, k).id)
#define NPKEY_DECL(o, k, f)						\
static struct nmport_key_desc NPKEY_DESC(o, k) = {			\
	.option = &NPOPT_DESC(o),					\
	.key = #k,							\
	.flags = (f),							\
	.id = -1,							\
};									\
static void __attribute__((constructor))				\
nmport_opt_##o##_key_##k##_ctor(void)					\
{									\
	nmport_opt_key_ctor(&NPKEY_DESC(o, k));				\
}
#define nmport_key(p, o, k)	((p)->keys[NPKEY_ID(o, k)])
#define nmport_defkey(p, o)	((p)->keys[NPOPT_DESC(o).default_key])

NPOPT_DECL(share, 0)
	NPKEY_DECL(share, port, NMREQ_OPTK_DEFAULT|NMREQ_OPTK_MUSTSET)
NPOPT_DECL(extmem, 0)
	NPKEY_DECL(extmem, file, NMREQ_OPTK_DEFAULT|NMREQ_OPTK_MUSTSET)
	NPKEY_DECL(extmem, if_num, 0)
	NPKEY_DECL(extmem, if_size, 0)
	NPKEY_DECL(extmem, ring_num, 0)
	NPKEY_DECL(extmem, ring_size, 0)
	NPKEY_DECL(extmem, buf_num, 0)
	NPKEY_DECL(extmem, buf_size, 0)
NPOPT_DECL(conf, 0)
	NPKEY_DECL(conf, rings, 0)
	NPKEY_DECL(conf, host_rings, 0)
	NPKEY_DECL(conf, slots, 0)
	NPKEY_DECL(conf, tx_rings, 0)
	NPKEY_DECL(conf, rx_rings, 0)
	NPKEY_DECL(conf, host_tx_rings, 0)
	NPKEY_DECL(conf, host_rx_rings, 0)
	NPKEY_DECL(conf, tx_slots, 0)
	NPKEY_DECL(conf, rx_slots, 0)


static int
NPOPT_PARSER(share)(struct nmreq_parse_ctx *p)
{
	struct nmctx *ctx = p->ctx;
	struct nmport_d *d = p->token;
	int32_t mem_id;
	const char *v = nmport_defkey(p, share);

	mem_id = nmreq_get_mem_id(&v, ctx);
	if (mem_id < 0)
		return -1;
	if (d->reg.nr_mem_id && d->reg.nr_mem_id != mem_id) {
		nmctx_ferror(ctx, "cannot set mem_id to %"PRId32", already set to %"PRIu16"",
				mem_id, d->reg.nr_mem_id);
		errno = EINVAL;
		return -1;
	}
	d->reg.nr_mem_id = mem_id;
	return 0;
}

static int
NPOPT_PARSER(extmem)(struct nmreq_parse_ctx *p)
{
	struct nmport_d *d;
	struct nmreq_pools_info *pi;
	int i;

	d = p->token;

	if (nmport_extmem_from_file(d, nmport_key(p, extmem, file)) < 0)
		return -1;

	pi = &d->extmem->nro_info;

	for  (i = 0; i < NPOPT_NRKEYS(extmem); i++) {
		const char *k = p->keys[i];
		uint32_t v;

		if (k == NULL)
			continue;

		v = atoi(k);
		if (i == NPKEY_ID(extmem, if_num)) {
			pi->nr_if_pool_objtotal = v;
		} else if (i == NPKEY_ID(extmem, if_size)) {
			pi->nr_if_pool_objsize = v;
		} else if (i == NPKEY_ID(extmem, ring_num)) {
			pi->nr_ring_pool_objtotal = v;
		} else if (i == NPKEY_ID(extmem, ring_size)) {
			pi->nr_ring_pool_objsize = v;
		} else if (i == NPKEY_ID(extmem, buf_num)) {
			pi->nr_buf_pool_objtotal = v;
		} else if (i == NPKEY_ID(extmem, buf_size)) {
			pi->nr_buf_pool_objsize = v;
		}
	}
	return 0;
}

static int
NPOPT_PARSER(conf)(struct nmreq_parse_ctx *p)
{
	struct nmport_d *d;

	d = p->token;

	if (nmport_key(p, conf, rings) != NULL) {
		uint16_t nr_rings = atoi(nmport_key(p, conf, rings));
		d->reg.nr_tx_rings = nr_rings;
		d->reg.nr_rx_rings = nr_rings;
	}
	if (nmport_key(p, conf, host_rings) != NULL) {
		uint16_t nr_rings = atoi(nmport_key(p, conf, host_rings));
		d->reg.nr_host_tx_rings = nr_rings;
		d->reg.nr_host_rx_rings = nr_rings;
	}
	if (nmport_key(p, conf, slots) != NULL) {
		uint32_t nr_slots = atoi(nmport_key(p, conf, slots));
		d->reg.nr_tx_slots = nr_slots;
		d->reg.nr_rx_slots = nr_slots;
	}
	if (nmport_key(p, conf, tx_rings) != NULL) {
		d->reg.nr_tx_rings = atoi(nmport_key(p, conf, tx_rings));
	}
	if (nmport_key(p, conf, rx_rings) != NULL) {
		d->reg.nr_rx_rings = atoi(nmport_key(p, conf, rx_rings));
	}
	if (nmport_key(p, conf, host_tx_rings) != NULL) {
		d->reg.nr_host_tx_rings = atoi(nmport_key(p, conf, host_tx_rings));
	}
	if (nmport_key(p, conf, host_rx_rings) != NULL) {
		d->reg.nr_host_rx_rings = atoi(nmport_key(p, conf, host_rx_rings));
	}
	if (nmport_key(p, conf, tx_slots) != NULL) {
		d->reg.nr_tx_slots = atoi(nmport_key(p, conf, tx_slots));
	}
	if (nmport_key(p, conf, rx_slots) != NULL) {
		d->reg.nr_rx_slots = atoi(nmport_key(p, conf, rx_slots));
	}
	return 0;
}

void
nmport_disable_option(const char *opt)
{
	struct nmreq_opt_parser *p;

	for (p = nmport_opt_parsers; p != NULL; p = p->next) {
		if (!strcmp(p->prefix, opt)) {
			p->flags |= NMREQ_OPTF_DISABLED;
		}
	}
}

int
nmport_enable_option(const char *opt)
{
	struct nmreq_opt_parser *p;

	for (p = nmport_opt_parsers; p != NULL; p = p->next) {
		if (!strcmp(p->prefix, opt)) {
			p->flags &= ~NMREQ_OPTF_DISABLED;
			return 0;
		}
	}
	errno = EOPNOTSUPP;
	return -1;
}


int
nmport_parse(struct nmport_d *d, const char *ifname)
{
	const char *scan = ifname;

	if (nmreq_header_decode(&scan, &d->hdr, d->ctx) < 0) {
		goto err;
	}

	/* parse the register request */
	if (nmreq_register_decode(&scan, &d->reg, d->ctx) < 0) {
		goto err;
	}

	/* parse the options, if any */
	if (nmreq_options_decode(scan, nmport_opt_parsers, d, d->ctx) < 0) {
		goto err;
	}
	return 0;

err:
	nmport_undo_parse(d);
	return -1;
}

void
nmport_undo_parse(struct nmport_d *d)
{
	nmport_do_cleanup(d);
	memset(&d->reg, 0, sizeof(d->reg));
	memset(&d->hdr, 0, sizeof(d->hdr));
}

struct nmport_d *
nmport_prepare(const char *ifname)
{
	struct nmport_d *d;

	/* allocate a descriptor */
	d = nmport_new();
	if (d == NULL)
		goto err;

	/* parse the header */
	if (nmport_parse(d, ifname) < 0)
		goto err;

	return d;

err:
	nmport_undo_prepare(d);
	return NULL;
}

void
nmport_undo_prepare(struct nmport_d *d)
{
	if (d == NULL)
		return;
	nmport_undo_parse(d);
	nmport_delete(d);
}

int
nmport_register(struct nmport_d *d)
{
	struct nmctx *ctx = d->ctx;

	if (d->register_done) {
		errno = EINVAL;
		nmctx_ferror(ctx, "%s: already registered", d->hdr.nr_name);
		return -1;
	}

	d->fd = open("/dev/netmap", O_RDWR);
	if (d->fd < 0) {
		nmctx_ferror(ctx, "/dev/netmap: %s", strerror(errno));
		goto err;
	}

	if (ioctl(d->fd, NIOCCTRL, &d->hdr) < 0) {
		struct nmreq_option *o;
		int option_errors = 0;

		nmreq_foreach_option(&d->hdr, o) {
			if (o->nro_status) {
				nmctx_ferror(ctx, "%s: option %s: %s",
						d->hdr.nr_name,
						nmreq_option_name(o->nro_reqtype),
						strerror(o->nro_status));
				option_errors++;
			}

		}
		if (!option_errors)
			nmctx_ferror(ctx, "%s: %s", d->hdr.nr_name, strerror(errno));
		goto err;
	}

	d->register_done = 1;

	return 0;

err:
	nmport_undo_register(d);
	return -1;
}

void
nmport_undo_register(struct nmport_d *d)
{
	if (d->fd >= 0)
		close(d->fd);
	d->fd = -1;
	d->register_done = 0;
}

/* lookup the mem_id in the mem-list: do a new mmap() if
 * not found, reuse existing otherwise
 */
int
nmport_mmap(struct nmport_d *d)
{
	struct nmctx *ctx = d->ctx;
	struct nmem_d *m = NULL;
	u_int num_tx, num_rx;
	int i;

	if (d->mmap_done) {
		errno = EINVAL;
		nmctx_ferror(ctx, "%s: already mapped", d->hdr.nr_name);
		return -1;
	}

	if (!d->register_done) {
		errno = EINVAL;
		nmctx_ferror(ctx, "cannot map unregistered port");
		return -1;
	}

	nmctx_lock(ctx);

	for (m = ctx->mem_descs; m != NULL; m = m->next)
		if (m->mem_id == d->reg.nr_mem_id)
			break;

	if (m == NULL) {
		m = nmctx_malloc(ctx, sizeof(*m));
		if (m == NULL) {
			nmctx_ferror(ctx, "cannot allocate memory descriptor");
			goto err;
		}
		memset(m, 0, sizeof(*m));
		if (d->extmem != NULL) {
			m->mem = (void *)((uintptr_t)d->extmem->nro_usrptr);
			m->size = d->extmem->nro_info.nr_memsize;
			m->is_extmem = 1;
		} else {
			m->mem = mmap(NULL, d->reg.nr_memsize, PROT_READ|PROT_WRITE,
					MAP_SHARED, d->fd, 0);
			if (m->mem == MAP_FAILED) {
				nmctx_ferror(ctx, "mmap: %s", strerror(errno));
				goto err;
			}
			m->size = d->reg.nr_memsize;
		}
		m->mem_id = d->reg.nr_mem_id;
		m->next = ctx->mem_descs;
		if (ctx->mem_descs != NULL)
			ctx->mem_descs->prev = m;
		ctx->mem_descs = m;
	}
	m->refcount++;

	nmctx_unlock(ctx);

	d->mem = m;

	d->nifp = NETMAP_IF(m->mem, d->reg.nr_offset);

	num_tx = d->reg.nr_tx_rings + d->nifp->ni_host_tx_rings;
	for (i = 0; i < num_tx && !d->nifp->ring_ofs[i]; i++)
		;
	d->first_tx_ring = i;
	for ( ; i < num_tx && d->nifp->ring_ofs[i]; i++)
		;
	d->last_tx_ring = i - 1;

	num_rx = d->reg.nr_rx_rings + d->nifp->ni_host_rx_rings;
	for (i = 0; i < num_rx && !d->nifp->ring_ofs[i + num_tx]; i++)
		;
	d->first_rx_ring = i;
	for ( ; i < num_rx && d->nifp->ring_ofs[i + num_tx]; i++)
		;
	d->last_rx_ring = i - 1;

	d->mmap_done = 1;

	return 0;

err:
	nmctx_unlock(ctx);
	nmport_undo_mmap(d);
	return -1;
}

void
nmport_undo_mmap(struct nmport_d *d)
{
	struct nmem_d *m;
	struct nmctx *ctx = d->ctx;

	m = d->mem;
	if (m == NULL)
		return;
	nmctx_lock(ctx);
	m->refcount--;
	if (m->refcount <= 0) {
		if (!m->is_extmem && m->mem != MAP_FAILED)
			munmap(m->mem, m->size);
		/* extract from the list and free */
		if (m->next != NULL)
			m->next->prev = m->prev;
		if (m->prev != NULL)
			m->prev->next = m->next;
		else
			ctx->mem_descs = m->next;
		nmctx_free(ctx, m);
		d->mem = NULL;
	}
	nmctx_unlock(ctx);
	d->mmap_done = 0;
	d->mem = NULL;
	d->nifp = NULL;
	d->first_tx_ring = 0;
	d->last_tx_ring = 0;
	d->first_rx_ring = 0;
	d->last_rx_ring = 0;
	d->cur_tx_ring = 0;
	d->cur_rx_ring = 0;
}

int
nmport_open_desc(struct nmport_d *d)
{
	if (nmport_register(d) < 0)
		goto err;

	if (nmport_mmap(d) < 0)
		goto err;

	return 0;
err:
	nmport_undo_open_desc(d);
	return -1;
}

void
nmport_undo_open_desc(struct nmport_d *d)
{
	nmport_undo_mmap(d);
	nmport_undo_register(d);
}


struct nmport_d *
nmport_open(const char *ifname)
{
	struct nmport_d *d;

	/* prepare the descriptor */
	d = nmport_prepare(ifname);
	if (d == NULL)
		goto err;

	/* open netmap and register */
	if (nmport_open_desc(d) < 0)
		goto err;

	return d;

err:
	nmport_close(d);
	return NULL;
}

void
nmport_close(struct nmport_d *d)
{
	if (d == NULL)
		return;
	nmport_undo_open_desc(d);
	nmport_undo_prepare(d);
}

struct nmport_d *
nmport_clone(struct nmport_d *d)
{
	struct nmport_d *c;
	struct nmctx *ctx;

	ctx = d->ctx;

	if (d->extmem != NULL && !d->register_done) {
		errno = EINVAL;
		nmctx_ferror(ctx, "cannot clone unregistered port that is using extmem");
		return NULL;
	}

	c = nmport_new_with_ctx(ctx);
	if (c == NULL)
		return NULL;
	/* copy the output of parse */
	c->hdr = d->hdr;
	/* redirect the pointer to the body */
	c->hdr.nr_body = (uintptr_t)&c->reg;
	/* options are not cloned */
	c->hdr.nr_options = 0;
	c->reg = d->reg; /* this also copies the mem_id */
	/* put the new port in an un-registered, unmapped state */
	c->fd = -1;
	c->nifp = NULL;
	c->register_done = 0;
	c->mem = NULL;
	c->extmem = NULL;
	c->mmap_done = 0;
	c->first_tx_ring = 0;
	c->last_tx_ring = 0;
	c->first_rx_ring = 0;
	c->last_rx_ring = 0;
	c->cur_tx_ring = 0;
	c->cur_rx_ring = 0;

	return c;
}

int
nmport_inject(struct nmport_d *d, const void *buf, size_t size)
{
	u_int c, n = d->last_tx_ring - d->first_tx_ring + 1,
		ri = d->cur_tx_ring;

	for (c = 0; c < n ; c++, ri++) {
		/* compute current ring to use */
		struct netmap_ring *ring;
		uint32_t i, j, idx;
		size_t rem;

		if (ri > d->last_tx_ring)
			ri = d->first_tx_ring;
		ring = NETMAP_TXRING(d->nifp, ri);
		rem = size;
		j = ring->cur;
		while (rem > ring->nr_buf_size && j != ring->tail) {
			rem -= ring->nr_buf_size;
			j = nm_ring_next(ring, j);
		}
		if (j == ring->tail && rem > 0)
			continue;
		i = ring->cur;
		while (i != j) {
			idx = ring->slot[i].buf_idx;
			ring->slot[i].len = ring->nr_buf_size;
			ring->slot[i].flags = NS_MOREFRAG;
			nm_pkt_copy(buf, NETMAP_BUF(ring, idx), ring->nr_buf_size);
			i = nm_ring_next(ring, i);
			buf = (char *)buf + ring->nr_buf_size;
		}
		idx = ring->slot[i].buf_idx;
		ring->slot[i].len = rem;
		ring->slot[i].flags = 0;
		nm_pkt_copy(buf, NETMAP_BUF(ring, idx), rem);
		ring->head = ring->cur = nm_ring_next(ring, i);
		d->cur_tx_ring = ri;
		return size;
	}
	return 0; /* fail */
}
