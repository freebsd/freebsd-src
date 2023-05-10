/*-
 * SPDX-License-Identifier: BSD-2-Clause
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
#include <ctype.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

//#define NMREQ_DEBUG
#ifdef NMREQ_DEBUG
#define NETMAP_WITH_LIBS
#define ED(...)	D(__VA_ARGS__)
#else
#define ED(...)
/* an identifier is a possibly empty sequence of alphanum characters and
 * underscores
 */
static int
nm_is_identifier(const char *s, const char *e)
{
	for (; s != e; s++) {
		if (!isalnum(*s) && *s != '_') {
			return 0;
		}
	}

	return 1;
}
#endif /* NMREQ_DEBUG */

#include <net/netmap_user.h>
#define LIBNETMAP_NOTHREADSAFE
#include "libnetmap.h"

void
nmreq_push_option(struct nmreq_header *h, struct nmreq_option *o)
{
	o->nro_next = h->nr_options;
	h->nr_options = (uintptr_t)o;
}

struct nmreq_prefix {
	const char *prefix;		/* the constant part of the prefix */
	size_t	    len;		/* its strlen() */
	uint32_t    flags;
#define	NR_P_ID		(1U << 0)	/* whether an identifier is needed */
#define NR_P_SKIP	(1U << 1)	/* whether the scope must be passed to netmap */
#define NR_P_EMPTYID	(1U << 2)	/* whether an empty identifier is allowed */
};

#define declprefix(prefix, flags)	{ (prefix), (sizeof(prefix) - 1), (flags) }

static struct nmreq_prefix nmreq_prefixes[] = {
	declprefix("netmap", NR_P_SKIP),
	declprefix(NM_BDG_NAME,	NR_P_ID|NR_P_EMPTYID),
	{ NULL } /* terminate the list */
};

void
nmreq_header_init(struct nmreq_header *h, uint16_t reqtype, void *body)
{
	memset(h, 0, sizeof(*h));
	h->nr_version = NETMAP_API;
	h->nr_reqtype = reqtype;
	h->nr_body = (uintptr_t)body;
}

int
nmreq_header_decode(const char **pifname, struct nmreq_header *h, struct nmctx *ctx)
{
	const char *scan = NULL;
	const char *vpname = NULL;
	const char *pipesep = NULL;
	u_int namelen;
	const char *ifname = *pifname;
	struct nmreq_prefix *p;

	scan = ifname;
	for (p = nmreq_prefixes; p->prefix != NULL; p++) {
		if (!strncmp(scan, p->prefix, p->len))
			break;
	}
	if (p->prefix == NULL) {
		nmctx_ferror(ctx, "%s: invalid request, prefix unknown or missing", *pifname);
		goto fail;
	}
	scan += p->len;

	vpname = index(scan, ':');
	if (vpname == NULL) {
		nmctx_ferror(ctx, "%s: missing ':'", ifname);
		goto fail;
	}
	if (vpname != scan) {
		/* there is an identifier, can we accept it? */
		if (!(p->flags & NR_P_ID)) {
			nmctx_ferror(ctx, "%s: no identifier allowed between '%s' and ':'", *pifname, p->prefix);
			goto fail;
		}

		if (!nm_is_identifier(scan, vpname)) {
			nmctx_ferror(ctx, "%s: invalid identifier '%.*s'", *pifname, vpname - scan, scan);
			goto fail;
		}
	} else {
		if ((p->flags & NR_P_ID) && !(p->flags & NR_P_EMPTYID)) {
			nmctx_ferror(ctx, "%s: identifier is missing between '%s' and ':'", *pifname, p->prefix);
			goto fail;
		}
	}
	++vpname; /* skip the colon */
	if (p->flags & NR_P_SKIP)
		ifname = vpname;
	scan = vpname;

	/* scan for a separator */
	for (; *scan && !index("-*^/@", *scan); scan++)
		;

	/* search for possible pipe indicators */
	for (pipesep = vpname; pipesep != scan && !index("{}", *pipesep); pipesep++)
		;

	if (!nm_is_identifier(vpname, pipesep)) {
		nmctx_ferror(ctx, "%s: invalid port name '%.*s'", *pifname,
				pipesep - vpname, vpname);
		goto fail;
	}
	if (pipesep != scan) {
		pipesep++;
		if (*pipesep == '\0') {
			nmctx_ferror(ctx, "%s: invalid empty pipe name", *pifname);
			goto fail;
		}
		if (!nm_is_identifier(pipesep, scan)) {
			nmctx_ferror(ctx, "%s: invalid pipe name '%.*s'", *pifname, scan - pipesep, pipesep);
			goto fail;
		}
	}

	namelen = scan - ifname;
	if (namelen >= sizeof(h->nr_name)) {
		nmctx_ferror(ctx, "name '%.*s' too long", namelen, ifname);
		goto fail;
	}
	if (namelen == 0) {
		nmctx_ferror(ctx, "%s: invalid empty port name", *pifname);
		goto fail;
	}

	/* fill the header */
	memcpy(h->nr_name, ifname, namelen);
	h->nr_name[namelen] = '\0';
	ED("name %s", h->nr_name);

	*pifname = scan;

	return 0;
fail:
	errno = EINVAL;
	return -1;
}


/*
 * 0 not recognized
 * -1 error
 *  >= 0 mem_id
 */
int32_t
nmreq_get_mem_id(const char **pifname, struct nmctx *ctx)
{
	int fd = -1;
	struct nmreq_header gh;
	struct nmreq_port_info_get gb;
	const char *ifname;

	errno = 0;
	ifname = *pifname;

	if (ifname == NULL)
		goto fail;

	/* try to look for a netmap port with this name */
	fd = open("/dev/netmap", O_RDWR);
	if (fd < 0) {
		nmctx_ferror(ctx, "cannot open /dev/netmap: %s", strerror(errno));
		goto fail;
	}
	nmreq_header_init(&gh, NETMAP_REQ_PORT_INFO_GET, &gb);
	if (nmreq_header_decode(&ifname, &gh, ctx) < 0) {
		goto fail;
	}
	memset(&gb, 0, sizeof(gb));
	if (ioctl(fd, NIOCCTRL, &gh) < 0) {
		nmctx_ferror(ctx, "cannot get info for '%s': %s", *pifname, strerror(errno));
		goto fail;
	}
	*pifname = ifname;
	close(fd);
	return gb.nr_mem_id;

fail:
	if (fd >= 0)
		close(fd);
	if (!errno)
		errno = EINVAL;
	return -1;
}


int
nmreq_register_decode(const char **pifname, struct nmreq_register *r, struct nmctx *ctx)
{
	enum { P_START, P_RNGSFXOK, P_GETNUM, P_FLAGS, P_FLAGSOK, P_MEMID, P_ONESW } p_state;
	long num;
	const char *scan = *pifname;
	uint32_t nr_mode;
	uint16_t nr_mem_id;
	uint16_t nr_ringid;
	uint64_t nr_flags;

	errno = 0;

	/* fill the request */

	p_state = P_START;
	/* defaults */
	nr_mode = NR_REG_ALL_NIC; /* default for no suffix */
	nr_mem_id = r->nr_mem_id; /* if non-zero, further updates are disabled */
	nr_ringid = 0;
	nr_flags = 0;
	while (*scan) {
		switch (p_state) {
		case P_START:
			switch (*scan) {
			case '^': /* only SW ring */
				nr_mode = NR_REG_SW;
				p_state = P_ONESW;
				break;
			case '*': /* NIC and SW */
				nr_mode = NR_REG_NIC_SW;
				p_state = P_RNGSFXOK;
				break;
			case '-': /* one NIC ring pair */
				nr_mode = NR_REG_ONE_NIC;
				p_state = P_GETNUM;
				break;
			case '/': /* start of flags */
				p_state = P_FLAGS;
				break;
			case '@': /* start of memid */
				p_state = P_MEMID;
				break;
			default:
				nmctx_ferror(ctx, "unknown modifier: '%c'", *scan);
				goto fail;
			}
			scan++;
			break;
		case P_RNGSFXOK:
			switch (*scan) {
			case '/':
				p_state = P_FLAGS;
				break;
			case '@':
				p_state = P_MEMID;
				break;
			default:
				nmctx_ferror(ctx, "unexpected character: '%c'", *scan);
				goto fail;
			}
			scan++;
			break;
		case P_GETNUM:
			if (!isdigit(*scan)) {
				nmctx_ferror(ctx, "got '%s' while expecting a number", scan);
				goto fail;
			}
			num = strtol(scan, (char **)&scan, 10);
			if (num < 0 || num >= NETMAP_RING_MASK) {
				nmctx_ferror(ctx, "'%ld' out of range [0, %d)",
						num, NETMAP_RING_MASK);
				goto fail;
			}
			nr_ringid = num & NETMAP_RING_MASK;
			p_state = P_RNGSFXOK;
			break;
		case P_FLAGS:
		case P_FLAGSOK:
			switch (*scan) {
			case '@':
				p_state = P_MEMID;
				scan++;
				continue;
			case 'x':
				nr_flags |= NR_EXCLUSIVE;
				break;
			case 'z':
				nr_flags |= NR_ZCOPY_MON;
				break;
			case 't':
				nr_flags |= NR_MONITOR_TX;
				break;
			case 'r':
				nr_flags |= NR_MONITOR_RX;
				break;
			case 'R':
				nr_flags |= NR_RX_RINGS_ONLY;
				break;
			case 'T':
				nr_flags |= NR_TX_RINGS_ONLY;
				break;
			default:
				nmctx_ferror(ctx, "unrecognized flag: '%c'", *scan);
				goto fail;
			}
			scan++;
			p_state = P_FLAGSOK;
			break;
		case P_MEMID:
			if (!isdigit(*scan)) {
				scan--;	/* escape to options */
				goto out;
			}
			num = strtol(scan, (char **)&scan, 10);
			if (num <= 0) {
				nmctx_ferror(ctx, "invalid mem_id: '%ld'", num);
				goto fail;
			}
			if (nr_mem_id && nr_mem_id != num) {
				nmctx_ferror(ctx, "invalid setting of mem_id to %ld (already set to %"PRIu16")", num, nr_mem_id);
				goto fail;
			}
			nr_mem_id = num;
			p_state = P_RNGSFXOK;
			break;
		case P_ONESW:
			if (!isdigit(*scan)) {
				p_state = P_RNGSFXOK;
			} else {
				nr_mode = NR_REG_ONE_SW;
				p_state = P_GETNUM;
			}
			break;
		}
	}
	if (p_state == P_MEMID && !*scan) {
		nmctx_ferror(ctx, "invalid empty mem_id");
		goto fail;
	}
	if (p_state != P_START && p_state != P_RNGSFXOK &&
	    p_state != P_FLAGSOK && p_state != P_MEMID && p_state != P_ONESW) {
		nmctx_ferror(ctx, "unexpected end of request");
		goto fail;
	}
out:
	ED("flags: %s %s %s %s %s %s",
			(nr_flags & NR_EXCLUSIVE) ? "EXCLUSIVE" : "",
			(nr_flags & NR_ZCOPY_MON) ? "ZCOPY_MON" : "",
			(nr_flags & NR_MONITOR_TX) ? "MONITOR_TX" : "",
			(nr_flags & NR_MONITOR_RX) ? "MONITOR_RX" : "",
			(nr_flags & NR_RX_RINGS_ONLY) ? "RX_RINGS_ONLY" : "",
			(nr_flags & NR_TX_RINGS_ONLY) ? "TX_RINGS_ONLY" : "");
	r->nr_mode = nr_mode;
	r->nr_ringid = nr_ringid;
	r->nr_flags = nr_flags;
	r->nr_mem_id = nr_mem_id;
	*pifname = scan;
	return 0;

fail:
	if (!errno)
		errno = EINVAL;
	return -1;
}


static int
nmreq_option_parsekeys(const char *prefix, char *body, struct nmreq_opt_parser *p,
		struct nmreq_parse_ctx *pctx)
{
	char *scan;
	char delim1;
	struct nmreq_opt_key *k;

	scan = body;
	delim1 = *scan;
	while (delim1 != '\0') {
		char *key, *value;
		char delim;
		size_t vlen;

		key = scan;
		for ( scan++; *scan != '\0' && *scan != '=' && *scan != ','; scan++) {
			if (*scan == '-')
				*scan = '_';
		}
		delim = *scan;
		*scan = '\0';
		scan++;
		for (k = p->keys; (k - p->keys) < NMREQ_OPT_MAXKEYS && k->key != NULL;
				k++) {
			if (!strcmp(k->key, key))
				goto found;

		}
		nmctx_ferror(pctx->ctx, "unknown key: '%s'", key);
		errno = EINVAL;
		return -1;
	found:
		if (pctx->keys[k->id] != NULL) {
			nmctx_ferror(pctx->ctx, "option '%s': duplicate key '%s', already set to '%s'",
					prefix, key, pctx->keys[k->id]);
			errno = EINVAL;
			return -1;
		}
		value = scan;
		for ( ; *scan != '\0' && *scan != ','; scan++)
			;
		delim1 = *scan;
		*scan = '\0';
		vlen = scan - value;
		scan++;
		if (delim == '=') {
			pctx->keys[k->id] = (vlen ? value : NULL);
		} else {
			if (!(k->flags & NMREQ_OPTK_ALLOWEMPTY)) {
				nmctx_ferror(pctx->ctx, "option '%s': missing '=value' for key '%s'",
						prefix, key);
				errno = EINVAL;
				return -1;
			}
			pctx->keys[k->id] = key;
		}
	}
	/* now check that all no-default keys have been assigned */
	for (k = p->keys; (k - p->keys) < NMREQ_OPT_MAXKEYS && k->key != NULL; k++) {
		if ((k->flags & NMREQ_OPTK_MUSTSET) && pctx->keys[k->id] == NULL) {
			nmctx_ferror(pctx->ctx, "option '%s': mandatory key '%s' not assigned",
					prefix, k->key);
			errno = EINVAL;
			return -1;
		}
	}
	return 0;
}


static int
nmreq_option_decode1(char *opt, struct nmreq_opt_parser *parsers,
		void *token, struct nmctx *ctx)
{
	struct nmreq_opt_parser *p;
	const char *prefix;
	char *scan;
	char delim;
	struct nmreq_parse_ctx pctx;
	int i;

	prefix = opt;
	/* find the delimiter */
	for (scan = opt; *scan != '\0' && *scan != ':' && *scan != '='; scan++)
		;
	delim = *scan;
	*scan = '\0';
	scan++;
	/* find the prefix */
	for (p = parsers; p != NULL; p = p->next) {
		if (!strcmp(prefix, p->prefix))
			break;
	}
	if (p == NULL) {
		nmctx_ferror(ctx, "unknown option: '%s'", prefix);
		errno = EINVAL;
		return -1;
	}
	if (p->flags & NMREQ_OPTF_DISABLED) {
		nmctx_ferror(ctx, "option '%s' is not supported", prefix);
		errno = EOPNOTSUPP;
		return -1;
	}
	/* prepare the parse context */
	pctx.ctx = ctx;
	pctx.token = token;
	for (i = 0; i < NMREQ_OPT_MAXKEYS; i++)
		pctx.keys[i] = NULL;
	switch (delim) {
	case '\0':
		/* no body */
		if (!(p->flags & NMREQ_OPTF_ALLOWEMPTY)) {
			nmctx_ferror(ctx, "syntax error: missing body after '%s'",
					prefix);
			errno = EINVAL;
			return -1;
		}
		break;
	case '=': /* the body goes to the default option key, if any */
		if (p->default_key < 0 || p->default_key >= NMREQ_OPT_MAXKEYS) {
			nmctx_ferror(ctx, "syntax error: '=' not valid after '%s'",
					prefix);
			errno = EINVAL;
			return -1;
		}
		if (*scan == '\0') {
			nmctx_ferror(ctx, "missing value for option '%s'", prefix);
			errno = EINVAL;
			return -1;
		}
		pctx.keys[p->default_key] = scan;
		break;
	case ':': /* parse 'key=value' strings */
		if (nmreq_option_parsekeys(prefix, scan, p, &pctx) < 0)
			return -1;
		break;
	}
	return p->parse(&pctx);
}

int
nmreq_options_decode(const char *opt, struct nmreq_opt_parser parsers[],
		void *token, struct nmctx *ctx)
{
	const char *scan, *opt1;
	char *w;
	size_t len;
	int ret;

	if (*opt == '\0')
		return 0; /* empty list, OK */

	if (*opt != '@') {
		nmctx_ferror(ctx, "option list does not start with '@'");
		errno = EINVAL;
		return -1;
	}

	scan = opt;
	do {
		scan++; /* skip the plus */
		opt1 = scan; /* start of option */
		/* find the end of the option */
		for ( ; *scan != '\0' && *scan != '@'; scan++)
			;
		len = scan - opt1;
		if (len == 0) {
			nmctx_ferror(ctx, "invalid empty option");
			errno = EINVAL;
			return -1;
		}
		w = nmctx_malloc(ctx, len + 1);
		if (w == NULL) {
			nmctx_ferror(ctx, "out of memory");
			errno = ENOMEM;
			return -1;
		}
		memcpy(w, opt1, len);
		w[len] = '\0';
		ret = nmreq_option_decode1(w, parsers, token, ctx);
		nmctx_free(ctx, w);
		if (ret < 0)
			return -1;
	} while (*scan != '\0');

	return 0;
}

struct nmreq_option *
nmreq_find_option(struct nmreq_header *h, uint32_t t)
{
	struct nmreq_option *o = NULL;

	nmreq_foreach_option(h, o) {
		if (o->nro_reqtype == t)
			break;
	}
	return o;
}

void
nmreq_remove_option(struct nmreq_header *h, struct nmreq_option *o)
{
	struct nmreq_option **nmo;

	for (nmo = (struct nmreq_option **)&h->nr_options; *nmo != NULL;
	    nmo = (struct nmreq_option **)&(*nmo)->nro_next) {
		if (*nmo == o) {
			*((uint64_t *)(*nmo)) = o->nro_next;
			o->nro_next = (uint64_t)(uintptr_t)NULL;
			break;
		}
	}
}

void
nmreq_free_options(struct nmreq_header *h)
{
	struct nmreq_option *o, *next;

	/*
	 * Note: can't use nmreq_foreach_option() here; it frees the
	 * list as it's walking and nmreq_foreach_option() isn't
	 * modification-safe.
	 */
	for (o = (struct nmreq_option *)(uintptr_t)h->nr_options; o != NULL;
	    o = next) {
		next = (struct nmreq_option *)(uintptr_t)o->nro_next;
		free(o);
	}
}

const char*
nmreq_option_name(uint32_t nro_reqtype)
{
	switch (nro_reqtype) {
	case NETMAP_REQ_OPT_EXTMEM:
		return "extmem";
	case NETMAP_REQ_OPT_SYNC_KLOOP_EVENTFDS:
		return "sync-kloop-eventfds";
	case NETMAP_REQ_OPT_CSB:
		return "csb";
	case NETMAP_REQ_OPT_SYNC_KLOOP_MODE:
		return "sync-kloop-mode";
	case NETMAP_REQ_OPT_OFFSETS:
		return "offsets";
	default:
		return "unknown";
	}
}

#if 0
#include <inttypes.h>
static void
nmreq_dump(struct nmport_d *d)
{
	printf("header:\n");
	printf("   nr_version:  %"PRIu16"\n", d->hdr.nr_version);
	printf("   nr_reqtype:  %"PRIu16"\n", d->hdr.nr_reqtype);
	printf("   nr_reserved: %"PRIu32"\n", d->hdr.nr_reserved);
	printf("   nr_name:     %s\n", d->hdr.nr_name);
	printf("   nr_options:  %lx\n", (unsigned long)d->hdr.nr_options);
	printf("   nr_body:     %lx\n", (unsigned long)d->hdr.nr_body);
	printf("\n");
	printf("register (%p):\n", (void *)d->hdr.nr_body);
	printf("   nr_mem_id:   %"PRIu16"\n", d->reg.nr_mem_id);
	printf("   nr_ringid:   %"PRIu16"\n", d->reg.nr_ringid);
	printf("   nr_mode:     %lx\n", (unsigned long)d->reg.nr_mode);
	printf("   nr_flags:    %lx\n", (unsigned long)d->reg.nr_flags);
	printf("\n");
	if (d->hdr.nr_options) {
		struct nmreq_opt_extmem *e = (struct nmreq_opt_extmem *)d->hdr.nr_options;
		printf("opt_extmem (%p):\n", e);
		printf("   nro_opt.nro_next:    %lx\n", (unsigned long)e->nro_opt.nro_next);
		printf("   nro_opt.nro_reqtype: %"PRIu32"\n", e->nro_opt.nro_reqtype);
		printf("   nro_usrptr:          %lx\n", (unsigned long)e->nro_usrptr);
		printf("   nro_info.nr_memsize  %"PRIu64"\n", e->nro_info.nr_memsize);
	}
	printf("\n");
	printf("mem (%p):\n", d->mem);
	printf("   refcount:   %d\n", d->mem->refcount);
	printf("   mem:        %p\n", d->mem->mem);
	printf("   size:       %zu\n", d->mem->size);
	printf("\n");
	printf("rings:\n");
	printf("   tx:   [%d, %d]\n", d->first_tx_ring, d->last_tx_ring);
	printf("   rx:   [%d, %d]\n", d->first_rx_ring, d->last_rx_ring);
}
int
main(int argc, char *argv[])
{
	struct nmport_d *d;

	if (argc < 2) {
		fprintf(stderr, "usage: %s netmap-expr\n", argv[0]);
		return 1;
	}

	d = nmport_open(argv[1]);
	if (d != NULL) {
		nmreq_dump(d);
		nmport_close(d);
	}

	return 0;
}
#endif
