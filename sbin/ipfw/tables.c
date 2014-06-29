/*
 * Copyright (c) 2002-2003 Luigi Rizzo
 * Copyright (c) 1996 Alex Nash, Paul Traina, Poul-Henning Kamp
 * Copyright (c) 1994 Ugen J.S.Antsilevich
 *
 * Idea and grammar partially left from:
 * Copyright (c) 1993 Daniel Boulet
 *
 * Redistribution and use in source forms, with and without modification,
 * are permitted provided that this entire comment appears intact.
 *
 * Redistribution in binary form may occur without any restrictions.
 * Obviously, it would be nice if you gave credit where credit is due
 * but requiring it would be too onerous.
 *
 * This software is provided ``AS IS'' without any warranties of any kind.
 *
 * in-kernel tables support
 *
 * $FreeBSD: projects/ipfw/sbin/ipfw/ipfw2.c 267467 2014-06-14 10:58:39Z melifaro $
 */


#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <stddef.h>	/* offsetof */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>

#define IPFW_INTERNAL	/* Access to protected structures in ip_fw.h. */

#include <net/if.h>
#include <net/if_dl.h>
#include <net/route.h> /* def. of struct route */
#include <netinet/in.h>
#include <netinet/ip_fw.h>
#include <arpa/inet.h>
#include <alias.h>

#include "ipfw2.h"

static void table_list(ipfw_xtable_info *i, int need_header);
static void table_fill_xentry(char *arg, ipfw_table_xentry *xent);
static int table_flush(char *name, uint32_t set);
static int table_destroy(char *name, uint32_t set);
static int table_get_info(char *name, uint32_t set, ipfw_xtable_info *i);
static int table_show_info(ipfw_xtable_info *i, void *arg);
static void table_fill_ntlv(ipfw_obj_ntlv *ntlv, char *name, uint16_t uidx);

static int table_flush_one(ipfw_xtable_info *i, void *arg);
static int table_show_one(ipfw_xtable_info *i, void *arg);
static int table_get_list(ipfw_xtable_info *i, ipfw_obj_header *oh);
static void table_show_list(ipfw_obj_header *oh, int need_header);

typedef int (table_cb_t)(ipfw_xtable_info *i, void *arg);
static int tables_foreach(table_cb_t *f, void *arg, int sort);

#ifndef s6_addr32
#define s6_addr32 __u6_addr.__u6_addr32
#endif

static int
lookup_host (char *host, struct in_addr *ipaddr)
{
	struct hostent *he;

	if (!inet_aton(host, ipaddr)) {
		if ((he = gethostbyname(host)) == NULL)
			return(-1);
		*ipaddr = *(struct in_addr *)he->h_addr_list[0];
	}
	return(0);
}

/*
 * This one handles all table-related commands
 * 	ipfw table N add addr[/masklen] [value]
 * 	ipfw table N delete addr[/masklen]
 * 	ipfw table {N | all} flush
 * 	ipfw table {N | all} list
 * 	ipfw table {N | all} info
 */
void
ipfw_table_handler(int ac, char *av[])
{
	ipfw_table_xentry *xent;
	int do_add;
	int is_all;
	uint32_t set;
	int error;
	char xbuf[sizeof(ip_fw3_opheader) + sizeof(ipfw_table_xentry)];
	ip_fw3_opheader *op3;
	char *tablename;

	memset(xbuf, 0, sizeof(xbuf));
	op3 = (ip_fw3_opheader *)xbuf;
	xent = (ipfw_table_xentry *)(op3 + 1);

	ac--; av++;
	tablename = *av;
	set = 0;
	if (ac && isdigit(**av)) {
		xent->tbl = atoi(*av);
		is_all = 0;
		ac--; av++;
	} else if (ac && _substrcmp(*av, "all") == 0) {
		xent->tbl = 0;
		is_all = 1;
		ac--; av++;
	} else
		errx(EX_USAGE, "table number or 'all' keyword required");
	NEED1("table needs command");
	if (is_all && _substrcmp(*av, "list") != 0
		   && _substrcmp(*av, "info") != 0
		   && _substrcmp(*av, "flush") != 0)
		errx(EX_USAGE, "table number required");

	if (_substrcmp(*av, "add") == 0 ||
	    _substrcmp(*av, "delete") == 0) {
		do_add = **av == 'a';
		ac--; av++;
		if (!ac)
			errx(EX_USAGE, "address required");

		table_fill_xentry(*av, xent);

		ac--; av++;
		if (do_add && ac) {
			unsigned int tval;
			/* isdigit is a bit of a hack here.. */
			if (strchr(*av, (int)'.') == NULL && isdigit(**av))  {
				xent->value = strtoul(*av, NULL, 0);
			} else {
				if (lookup_host(*av, (struct in_addr *)&tval) == 0) {
					/* The value must be stored in host order	 *
					 * so that the values < 65k can be distinguished */
		       			xent->value = ntohl(tval);
				} else {
					errx(EX_NOHOST, "hostname ``%s'' unknown", *av);
				}
			}
		} else
			xent->value = 0;
		if (do_set3(do_add ? IP_FW_TABLE_XADD : IP_FW_TABLE_XDEL,
		    op3, sizeof(xbuf)) < 0) {
			/* If running silent, don't bomb out on these errors. */
			if (!(co.do_quiet && (errno == (do_add ? EEXIST : ESRCH))))
				err(EX_OSERR, "setsockopt(IP_FW_TABLE_%s)",
				    do_add ? "XADD" : "XDEL");
			/* In silent mode, react to a failed add by deleting */
			if (do_add) {
				do_set3(IP_FW_TABLE_XDEL, op3, sizeof(xbuf));
				if (do_set3(IP_FW_TABLE_XADD, op3, sizeof(xbuf)) < 0)
					err(EX_OSERR,
					    "setsockopt(IP_FW_TABLE_XADD)");
			}
		}
	} else if (_substrcmp(*av, "flush") == 0) {
		if (is_all == 0) {
			if ((error = table_flush(tablename, set)) != 0)
				err(EX_OSERR, "failed to flush table %s info",
				    tablename);
		} else {
			error = tables_foreach(table_flush_one, NULL, 1);
			if (error != 0)
				err(EX_OSERR, "failed to flush tables list");
		}
	} else if (_substrcmp(*av, "list") == 0) {
		if (is_all == 0) {
			ipfw_xtable_info i;
			if ((error = table_get_info(tablename, set, &i)) != 0)
				err(EX_OSERR, "failed to request table info");
			table_show_one(&i, NULL);
		} else {
			error = tables_foreach(table_show_one, NULL, 1);
			if (error != 0)
				err(EX_OSERR, "failed to request tables list");
		}
	} else if (_substrcmp(*av, "destroy") == 0) {
		if (table_destroy(tablename, set) != 0)
			err(EX_OSERR, "failed to destroy table %s", tablename);
	} else if (_substrcmp(*av, "info") == 0) {
		if (is_all == 0) {
			ipfw_xtable_info i;
			if ((error = table_get_info(tablename, set, &i)) != 0)
				err(EX_OSERR, "failed to request table info");
			table_show_info(&i, NULL);
		} else {
			error = tables_foreach(table_show_info, NULL, 1);
			if (error != 0)
				err(EX_OSERR, "failed to request tables list");
		}
	} else
		errx(EX_USAGE, "invalid table command %s", *av);
}

static void
table_fill_xentry(char *arg, ipfw_table_xentry *xent)
{
	int addrlen, mask, masklen, type;
	struct in6_addr *paddr;
	uint32_t *pkey;
	char *p;
	uint32_t key;

	mask = 0;
	type = 0;
	addrlen = 0;
	masklen = 0;

	/* 
	 * Let's try to guess type by agrument.
	 * Possible types: 
	 * 1) IPv4[/mask]
	 * 2) IPv6[/mask]
	 * 3) interface name
	 * 4) port, uid/gid or other u32 key (base 10 format)
	 * 5) hostname
	 */
	paddr = &xent->k.addr6;
	if (ishexnumber(*arg) != 0 || *arg == ':') {
		/* Remove / if exists */
		if ((p = strchr(arg, '/')) != NULL) {
			*p = '\0';
			mask = atoi(p + 1);
		}

		if (inet_pton(AF_INET, arg, paddr) == 1) {
			if (p != NULL && mask > 32)
				errx(EX_DATAERR, "bad IPv4 mask width: %s",
				    p + 1);

			type = IPFW_TABLE_CIDR;
			masklen = p ? mask : 32;
			addrlen = sizeof(struct in_addr);
		} else if (inet_pton(AF_INET6, arg, paddr) == 1) {
			if (IN6_IS_ADDR_V4COMPAT(paddr))
				errx(EX_DATAERR,
				    "Use IPv4 instead of v4-compatible");
			if (p != NULL && mask > 128)
				errx(EX_DATAERR, "bad IPv6 mask width: %s",
				    p + 1);

			type = IPFW_TABLE_CIDR;
			masklen = p ? mask : 128;
			addrlen = sizeof(struct in6_addr);
		} else {
			/* Port or any other key */
			/* Skip non-base 10 entries like 'fa1' */
			key = strtol(arg, &p, 10);
			if (*p == '\0') {
				pkey = (uint32_t *)paddr;
				*pkey = htonl(key);
				type = IPFW_TABLE_CIDR;
				masklen = 32;
				addrlen = sizeof(uint32_t);
			} else if ((p != arg) && (*p == '.')) {
				/*
				 * Warn on IPv4 address strings
				 * which are "valid" for inet_aton() but not
				 * in inet_pton().
				 *
				 * Typical examples: '10.5' or '10.0.0.05'
				 */
				errx(EX_DATAERR,
				    "Invalid IPv4 address: %s", arg);
			}
		}
	}

	if (type == 0 && strchr(arg, '.') == NULL) {
		/* Assume interface name. Copy significant data only */
		mask = MIN(strlen(arg), IF_NAMESIZE - 1);
		memcpy(xent->k.iface, arg, mask);
		/* Set mask to exact match */
		masklen = 8 * IF_NAMESIZE;
		type = IPFW_TABLE_INTERFACE;
		addrlen = IF_NAMESIZE;
	}

	if (type == 0) {
		if (lookup_host(arg, (struct in_addr *)paddr) != 0)
			errx(EX_NOHOST, "hostname ``%s'' unknown", arg);

		masklen = 32;
		type = IPFW_TABLE_CIDR;
		addrlen = sizeof(struct in_addr);
	}

	xent->type = type;
	xent->masklen = masklen;
	xent->len = offsetof(ipfw_table_xentry, k) + addrlen;
}

static void
table_fill_ntlv(ipfw_obj_ntlv *ntlv, char *name, uint16_t uidx)
{

	ntlv->head.type = IPFW_TLV_TBL_NAME;
	ntlv->head.length = sizeof(ipfw_obj_ntlv);
	ntlv->idx = uidx;
	strlcpy(ntlv->name, name, sizeof(ntlv->name));
}

static void
table_fill_objheader(ipfw_obj_header *oh, ipfw_xtable_info *i)
{

	oh->set = i->set;
	oh->idx = 1;
	table_fill_ntlv(&oh->ntlv, i->tablename, 1);
}

/*
 * Destroys given table @name in given @set.
 * Returns 0 on success.
 */
static int
table_destroy(char *name, uint32_t set)
{
	ipfw_obj_header oh;

	memset(&oh, 0, sizeof(oh));
	oh.idx = 1;
	table_fill_ntlv(&oh.ntlv, name, 1);
	if (do_set3(IP_FW_TABLE_XDESTROY, &oh.opheader, sizeof(oh)) != 0)
		return (-1);

	return (0);
}

/*
 * Flushes given table @name in given @set.
 * Returns 0 on success.
 */
static int
table_flush(char *name, uint32_t set)
{
	ipfw_obj_header oh;

	memset(&oh, 0, sizeof(oh));
	oh.idx = 1;
	table_fill_ntlv(&oh.ntlv, name, 1);
	if (do_set3(IP_FW_TABLE_XFLUSH, &oh.opheader, sizeof(oh)) != 0)
		return (-1);

	return (0);
}

/*
 * Retrieves info for given table @name in given @set and stores
 * it inside @i.
 * Returns 0 on success.
 */
static int
table_get_info(char *name, uint32_t set, ipfw_xtable_info *i)
{
	char tbuf[sizeof(ipfw_obj_header)+sizeof(ipfw_xtable_info)];
	ipfw_obj_header *oh;
	size_t sz;

	sz = sizeof(tbuf);
	memset(tbuf, 0, sizeof(tbuf));
	oh = (ipfw_obj_header *)tbuf;

	i->set = set;
	strlcpy(i->tablename, name, sizeof(i->tablename));

	table_fill_objheader(oh, i);

	if (do_get3(IP_FW_TABLE_XINFO, &oh->opheader, &sz) < 0)
		return (-1);

	if (sz < sizeof(tbuf))
		return (-1);

	*i = *(ipfw_xtable_info *)(oh + 1);

	return (0);
}

/*
 * Prints table info struct @i in human-readable form.
 */
static int
table_show_info(ipfw_xtable_info *i, void *arg)
{
	char *type;

	printf("--- table(%s), set(%u) ---\n", i->tablename, i->set);
	switch (i->type) {
	case IPFW_TABLE_CIDR:
		type = "cidr";
		break;
	case IPFW_TABLE_INTERFACE:
		type = "iface";
		break;
	default:
		type = "unknown";
	}
	printf(" type: %s, kindex: %d\n", type, i->kidx);
	printf(" ftype: %d, algorithm: %d\n", i->ftype, i->atype);
	printf(" references: %u\n", i->refcnt);
	printf(" items: %u, size: %u\n", i->count, i->size);

	return (0);
}


/*
 * Function wrappers which can be used either
 * as is or as foreach function parameter.
 */

static int
table_show_one(ipfw_xtable_info *i, void *arg)
{
	ipfw_obj_header *oh;

	if ((oh = malloc(i->size)) == NULL)
		return (ENOMEM);

	if (table_get_list(i, oh) == 0)
		table_show_list(oh, 1);

	free(oh);
	return (0);	
}

static int
table_flush_one(ipfw_xtable_info *i, void *arg)
{

	return (table_flush(i->tablename, i->set));
}


/*
 * Compare table names.
 * Honor number comparison.
 */
static int
tablename_cmp(const void *a, const void *b)
{
	ipfw_xtable_info *ia, *ib;
	int la, lb;

	ia = (ipfw_xtable_info *)a;
	ib = (ipfw_xtable_info *)b;
	la = strlen(ia->tablename);
	lb = strlen(ib->tablename);

	if (la > lb)
		return (1);
	else if (la < lb)
		return (-01);

	return (strcmp(ia->tablename, ib->tablename));
}

/*
 * Retrieves table list from kernel,
 * optionally sorts it and calls requested function for each table.
 * Returns 0 on success.
 */
static int
tables_foreach(table_cb_t *f, void *arg, int sort)
{
	ipfw_obj_lheader req, *olh;
	ipfw_xtable_info *info;
	size_t sz;
	int i, error;

	memset(&req, 0, sizeof(req));
	sz = sizeof(req);

	if ((error = do_get3(IP_FW_TABLES_XGETSIZE, &req.opheader, &sz)) != 0)
		return (errno);

	sz = req.size;
	if ((olh = calloc(1, sz)) == NULL)
		return (ENOMEM);

	olh->size = sz;
	if ((error = do_get3(IP_FW_TABLES_XLIST, &olh->opheader, &sz)) != 0) {
		free(olh);
		return (errno);
	}

	if (sort != 0)
		qsort(olh + 1, olh->count, olh->objsize, tablename_cmp);

	info = (ipfw_xtable_info *)(olh + 1);
	for (i = 0; i < olh->count; i++) {
		error = f(info, arg); /* Ignore errors for now */
		info = (ipfw_xtable_info *)((caddr_t)info + olh->objsize);
	}

	free(olh);

	return (0);
}

/*
 * Retrieves all entries for given table @i in
 * eXtended format. Assumes buffer of size
 * @i->size has already been allocated by caller.
 *
 * Returns 0 on success.
 */
static int
table_get_list(ipfw_xtable_info *i, ipfw_obj_header *oh)
{
	size_t sz;
	int error;

	table_fill_objheader(oh, i);
	sz = i->size;

	oh->opheader.version = 1; /* Current version */

	if ((error = do_get3(IP_FW_TABLE_XLIST, &oh->opheader, &sz)) != 0)
		return (errno);

	return (0);
}

/*
 * Shows all entries from @oh in human-readable format
 */
static void
table_show_list(ipfw_obj_header *oh, int need_header)
{
	ipfw_table_xentry *xent;
	uint32_t count, tval;
	char tbuf[128];
	struct in6_addr *addr6;
	ipfw_xtable_info *i;

	i = (ipfw_xtable_info *)(oh + 1);
	xent = (ipfw_table_xentry *)(i + 1);

	if (need_header)
		printf("--- table(%s), set(%u) ---\n", i->tablename, i->set);

	count = i->count;
	while (count > 0) {
		switch (i->type) {
		case IPFW_TABLE_CIDR:
			/* IPv4 or IPv6 prefixes */
			tval = xent->value;
			addr6 = &xent->k.addr6;


			if ((xent->flags & IPFW_TCF_INET) != 0) {
				/* IPv4 address */
				inet_ntop(AF_INET, &addr6->s6_addr32[3], tbuf,
				    sizeof(tbuf));
			} else {
				/* IPv6 address */
				inet_ntop(AF_INET6, addr6, tbuf, sizeof(tbuf));
			}

			if (co.do_value_as_ip) {
				tval = htonl(tval);
				printf("%s/%u %s\n", tbuf, xent->masklen,
				    inet_ntoa(*(struct in_addr *)&tval));
			} else
				printf("%s/%u %u\n", tbuf, xent->masklen, tval);
			break;
		case IPFW_TABLE_INTERFACE:
			/* Interface names */
			tval = xent->value;
			if (co.do_value_as_ip) {
				tval = htonl(tval);
				printf("%s %s\n", xent->k.iface,
				    inet_ntoa(*(struct in_addr *)&tval));
			} else
				printf("%s %u\n", xent->k.iface, tval);
		}

		xent = (ipfw_table_xentry *)((caddr_t)xent + xent->len);
		count--;
	}
}

int
compare_ntlv(const void *_a, const void *_b)
{
	ipfw_obj_ntlv *a, *b;

	a = (ipfw_obj_ntlv *)_a;
	b = (ipfw_obj_ntlv *)_b;

	if (a->set < b->set)
		return (-1);
	else if (a->set > b->set)
		return (1);

	if (a->idx < b->idx)
		return (-1);
	else if (a->idx > b->idx)
		return (1);

	return (0);
}

int
compare_kntlv(const void *k, const void *v)
{
	ipfw_obj_ntlv *ntlv;
	uint16_t key;

	key = *((uint16_t *)k);
	ntlv = (ipfw_obj_ntlv *)v;

	if (key < ntlv->idx)
		return (-1);
	else if (key > ntlv->idx)
		return (1);
	
	return (0);
}

/*
 * Finds table name in @ctlv by @idx.
 * Uses the following facts:
 * 1) All TLVs are the same size
 * 2) Kernel implementation provides already sorted list.
 *
 * Returns table name or NULL.
 */
char *
table_search_ctlv(ipfw_obj_ctlv *ctlv, uint16_t idx)
{
	ipfw_obj_ntlv *ntlv;

	ntlv = bsearch(&idx, (ctlv + 1), ctlv->count, ctlv->objsize,
	    compare_kntlv);

	if (ntlv != 0)
		return (ntlv->name);

	return (NULL);
}

void
table_sort_ctlv(ipfw_obj_ctlv *ctlv)
{

	qsort(ctlv + 1, ctlv->count, ctlv->objsize, compare_ntlv);
}

int
table_check_name(char *tablename)
{
	int c, i, l;

	/*
	 * Check if tablename is null-terminated and contains
	 * valid symbols only. Valid mask is:
	 * [a-zA-Z\-\.][a-zA-Z0-9\-_\.]{0,62}
	 */
	l = strlen(tablename);
	if (l == 0 || l >= 64)
		return (EINVAL);
	/* Restrict first symbol to non-digit */
	if (isdigit(tablename[0]))
		return (EINVAL);
	for (i = 0; i < l; i++) {
		c = tablename[i];
		if (isalpha(c) || isdigit(c) || c == '_' ||
		    c == '-' || c == '.')
			continue;
		return (EINVAL);	
	}

	return (0);
}

