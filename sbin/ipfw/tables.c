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
static void table_modify_record(ipfw_obj_header *oh, int ac, char *av[],
    int add, int update);
static int table_flush(ipfw_obj_header *oh);
static int table_destroy(ipfw_obj_header *oh);
static int table_do_create(ipfw_obj_header *oh, ipfw_xtable_info *i);
static void table_create(ipfw_obj_header *oh, int ac, char *av[]);
static int table_get_info(ipfw_obj_header *oh, ipfw_xtable_info *i);
static int table_show_info(ipfw_xtable_info *i, void *arg);
static void table_fill_ntlv(ipfw_obj_ntlv *ntlv, char *name, uint32_t set,
    uint16_t uidx);

static int table_flush_one(ipfw_xtable_info *i, void *arg);
static int table_show_one(ipfw_xtable_info *i, void *arg);
static int table_get_list(ipfw_xtable_info *i, ipfw_obj_header *oh);
static void table_show_list(ipfw_obj_header *oh, int need_header);

static void tentry_fill_key(ipfw_obj_header *oh, ipfw_obj_tentry *tent,
    char *key, uint8_t *ptype, uint8_t *pvtype);
static void tentry_fill_value(ipfw_obj_header *oh, ipfw_obj_tentry *tent,
    char *arg, uint8_t type, uint8_t vtype);

typedef int (table_cb_t)(ipfw_xtable_info *i, void *arg);
static int tables_foreach(table_cb_t *f, void *arg, int sort);

#ifndef s6_addr32
#define s6_addr32 __u6_addr.__u6_addr32
#endif

static struct _s_x tabletypes[] = {
      { "cidr",		IPFW_TABLE_CIDR },
      { "iface",	IPFW_TABLE_INTERFACE },
      { "u32",		IPFW_TABLE_U32 },
      { NULL, 0 }
};

static struct _s_x tablevaltypes[] = {
      { "dscp",		IPFW_VTYPE_DSCP },
      { "ip",		IPFW_VTYPE_IP },
      { "number",	IPFW_VTYPE_U32 },
      { NULL, 0 }
};

static struct _s_x tablecmds[] = {
      { "add",		TOK_ADD },
      { "create",	TOK_CREATE },
      { "delete",	TOK_DEL },
      { "destroy",	TOK_DESTROY },
      { "flush",	TOK_FLUSH },
      { "info",		TOK_INFO },
      { "list",		TOK_LIST },
      { NULL, 0 }
};

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
 * 	ipfw table NAME create ...
 * 	ipfw table NAME destroy
 * 	ipfw table NAME add addr[/masklen] [value]
 * 	ipfw table NAME delete addr[/masklen]
 * 	ipfw table {NAME | all} flush
 * 	ipfw table {NAME | all} list
 * 	ipfw table {NAME | all} info
 */
void
ipfw_table_handler(int ac, char *av[])
{
	int do_add, is_all;
	int error, tcmd;
	ipfw_xtable_info i;
	ipfw_obj_header oh;
	char *tablename;
	uint32_t set;

	memset(&oh, 0, sizeof(oh));
	is_all = 0;
	if (co.use_set != 0)
		set = co.use_set - 1;
	else
		set = 0;

	ac--; av++;
	tablename = *av;

	if (table_check_name(tablename) == 0) {
		table_fill_ntlv(&oh.ntlv, *av, set, 1);
		//oh->set = set;
		oh.idx = 1;
	} else {
		if (strcmp(tablename, "all") == 0)
			is_all = 1;
		else
			errx(EX_USAGE, "table name %s is invalid", tablename);
	}
	ac--; av++;

	if ((tcmd = match_token(tablecmds, *av)) == -1)
		errx(EX_USAGE, "invalid table command %s", *av);

	NEED1("table needs command");
	switch (tcmd) {
	case TOK_LIST:
	case TOK_INFO:
	case TOK_FLUSH:
		break;
	default:
		if (is_all != 0)
			errx(EX_USAGE, "table name required");
	}

	switch (tcmd) {
	case TOK_ADD:
	case TOK_DEL:
		do_add = **av == 'a';
		ac--; av++;
		table_modify_record(&oh, ac, av, do_add, co.do_quiet);
		break;
	case TOK_CREATE:
		ac--; av++;
		table_create(&oh, ac, av);
		break;
	case TOK_DESTROY:
		if (table_destroy(&oh) != 0)
			err(EX_OSERR, "failed to destroy table %s", tablename);
		break;
	case TOK_FLUSH:
		if (is_all == 0) {
			if ((error = table_flush(&oh)) != 0)
				err(EX_OSERR, "failed to flush table %s info",
				    tablename);
		} else {
			error = tables_foreach(table_flush_one, &oh, 1);
			if (error != 0)
				err(EX_OSERR, "failed to flush tables list");
		}
		break;
	case TOK_INFO:
		if (is_all == 0) {
			if ((error = table_get_info(&oh, &i)) != 0)
				err(EX_OSERR, "failed to request table info");
			table_show_info(&i, NULL);
		} else {
			error = tables_foreach(table_show_info, NULL, 1);
			if (error != 0)
				err(EX_OSERR, "failed to request tables list");
		}
		break;
	case TOK_LIST:
		if (is_all == 0) {
			ipfw_xtable_info i;
			if ((error = table_get_info(&oh, &i)) != 0)
				err(EX_OSERR, "failed to request table info");
			table_show_one(&i, NULL);
		} else {
			error = tables_foreach(table_show_one, NULL, 1);
			if (error != 0)
				err(EX_OSERR, "failed to request tables list");
		}
		break;
	}
}

static void
table_fill_ntlv(ipfw_obj_ntlv *ntlv, char *name, uint32_t set, uint16_t uidx)
{

	ntlv->head.type = IPFW_TLV_TBL_NAME;
	ntlv->head.length = sizeof(ipfw_obj_ntlv);
	ntlv->idx = uidx;
	ntlv->set = set;
	strlcpy(ntlv->name, name, sizeof(ntlv->name));
}

static void
table_fill_objheader(ipfw_obj_header *oh, ipfw_xtable_info *i)
{

	oh->set = i->set;
	oh->idx = 1;
	table_fill_ntlv(&oh->ntlv, i->tablename, oh->set, 1);
}

static struct _s_x tablenewcmds[] = {
      { "type",		TOK_TYPE},
      { "valtype",	TOK_VALTYPE },
      { "algo",		TOK_ALGO },
      { NULL, 0 }
};

/*
 * Creates new table
 *
 * ipfw table NAME create [ type { cidr | iface | u32 } ]
 *     [ valtype { number | ip | dscp } ]
 *     [ algo algoname ]
 *
 * Request: [ ipfw_obj_header ipfw_xtable_info ]
 */
static void
table_create(ipfw_obj_header *oh, int ac, char *av[])
{
	ipfw_xtable_info xi;
	int error, tcmd, val;
	size_t sz;
	char tbuf[128];

	sz = sizeof(tbuf);
	memset(&xi, 0, sizeof(xi));

	/* Set some defaults to preserve compability */
	xi.type = IPFW_TABLE_CIDR;
	xi.vtype = IPFW_VTYPE_U32;

	while (ac > 0) {
		if ((tcmd = match_token(tablenewcmds, *av)) == -1)
			errx(EX_USAGE, "unknown option: %s", *av);
		ac--; av++;

		switch (tcmd) {
		case TOK_TYPE:
			NEED1("table type required");
			val = match_token(tabletypes, *av);
			if (val != -1) {
				printf("av %s type %d\n", *av, xi.type);
				xi.type = val;
				ac--; av++;
				break;
			}
			concat_tokens(tbuf, sizeof(tbuf), tabletypes, ", ");
			errx(EX_USAGE, "Unknown tabletype: %s. Supported: %s",
			    *av, tbuf);
			break;
		case TOK_VALTYPE:
			NEED1("table value type required");
			val = match_token(tablevaltypes, *av);
			if (val != -1) {
				xi.vtype = val;
				ac--; av++;
				break;
			}
			concat_tokens(tbuf, sizeof(tbuf), tablevaltypes, ", ");
			errx(EX_USAGE, "Unknown value type: %s. Supported: %s",
			    *av, tbuf);
			break;
		case TOK_ALGO:
			NEED1("table algorithm name required");
			if (strlen(*av) > sizeof(xi.algoname))
				errx(EX_USAGE, "algorithm name too long");
			strlcpy(xi.algoname, *av, sizeof(xi.algoname));
			ac--; av++;
			break;
		}
	}

	if ((error = table_do_create(oh, &xi)) != 0)
		err(EX_OSERR, "Table creation failed");
}

/*
 * Creates new table
 *
 * Request: [ ipfw_obj_header ipfw_xtable_info ]
 *
 * Returns 0 on success.
 */
static int
table_do_create(ipfw_obj_header *oh, ipfw_xtable_info *i)
{
	char tbuf[sizeof(ipfw_obj_header) + sizeof(ipfw_xtable_info)];
	int error;

	memcpy(tbuf, oh, sizeof(*oh));
	memcpy(tbuf + sizeof(*oh), i, sizeof(*i));
	oh = (ipfw_obj_header *)tbuf;

	error = do_set3(IP_FW_TABLE_XCREATE, &oh->opheader, sizeof(tbuf));

	return (error);
}

/*
 * Destroys given table specified by @oh->ntlv.
 * Returns 0 on success.
 */
static int
table_destroy(ipfw_obj_header *oh)
{

	if (do_set3(IP_FW_TABLE_XDESTROY, &oh->opheader, sizeof(*oh)) != 0)
		return (-1);

	return (0);
}

/*
 * Flushes given table specified by @oh->ntlv.
 * Returns 0 on success.
 */
static int
table_flush(ipfw_obj_header *oh)
{

	if (do_set3(IP_FW_TABLE_XFLUSH, &oh->opheader, sizeof(*oh)) != 0)
		return (-1);

	return (0);
}

/*
 * Retrieves table in given table specified by @oh->ntlv.
 * it inside @i.
 * Returns 0 on success.
 */
static int
table_get_info(ipfw_obj_header *oh, ipfw_xtable_info *i)
{
	char tbuf[sizeof(ipfw_obj_header) + sizeof(ipfw_xtable_info)];
	int error;
	size_t sz;

	sz = sizeof(tbuf);
	memset(tbuf, 0, sizeof(tbuf));
	memcpy(tbuf, oh, sizeof(*oh));
	oh = (ipfw_obj_header *)tbuf;

	if ((error = do_get3(IP_FW_TABLE_XINFO, &oh->opheader, &sz)) != 0)
		return (error);

	if (sz < sizeof(tbuf))
		return (EINVAL);

	*i = *(ipfw_xtable_info *)(oh + 1);

	return (0);
}

/*
 * Prints table info struct @i in human-readable form.
 */
static int
table_show_info(ipfw_xtable_info *i, void *arg)
{
	const char *ttype, *vtype;

	printf("--- table(%s), set(%u) ---\n", i->tablename, i->set);
	if ((ttype = match_value(tabletypes, i->type)) == NULL)
		ttype = "unknown";
	if ((vtype = match_value(tablevaltypes, i->vtype)) == NULL)
		vtype = "unknown";

	printf(" type: %s, kindex: %d\n", ttype, i->kidx);
	printf(" valtype: %s, algorithm: %s\n", vtype, i->algoname);
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

	if ((oh = calloc(1, i->size)) == NULL)
		return (ENOMEM);

	if (table_get_list(i, oh) == 0)
		table_show_list(oh, 1);

	free(oh);
	return (0);	
}

static int
table_flush_one(ipfw_xtable_info *i, void *arg)
{
	ipfw_obj_header *oh;

	oh = (ipfw_obj_header *)arg;

	table_fill_ntlv(&oh->ntlv, i->tablename, i->set, 1);

	return (table_flush(oh));
}

static int
table_do_modify_record(int cmd, ipfw_obj_header *oh,
    ipfw_obj_tentry *tent, int update)
{
	char xbuf[sizeof(ipfw_obj_header) + sizeof(ipfw_obj_tentry)];
	int error;

	memset(xbuf, 0, sizeof(xbuf));
	memcpy(xbuf, oh, sizeof(*oh));
	oh = (ipfw_obj_header *)xbuf;
	oh->opheader.version = 1;

	memcpy(oh + 1, tent, sizeof(*tent));
	tent = (ipfw_obj_tentry *)(oh + 1);
	if (update != 0)
		tent->flags |= IPFW_TF_UPDATE;
	tent->head.length = sizeof(ipfw_obj_tentry);

	error = do_set3(cmd, &oh->opheader, sizeof(xbuf));

	return (error);
}

static void
table_modify_record(ipfw_obj_header *oh, int ac, char *av[], int add, int update)
{
	ipfw_obj_tentry tent;
	uint8_t type, vtype;
	int cmd;
	char *texterr;

	if (ac == 0)
		errx(EX_USAGE, "address required");
	
	memset(&tent, 0, sizeof(tent));
	tent.head.length = sizeof(tent);
	tent.idx = 1;

	tentry_fill_key(oh, &tent, *av, &type, &vtype);
	oh->ntlv.type = type;
	ac--; av++;

	if (add != 0) {
		if (ac > 0)
			tentry_fill_value(oh, &tent, *av, type, vtype);
		cmd = IP_FW_TABLE_XADD;
		texterr = "setsockopt(IP_FW_TABLE_XADD)";
	} else {
		cmd = IP_FW_TABLE_XDEL;
		texterr = "setsockopt(IP_FW_TABLE_XDEL)";
	}

	if (table_do_modify_record(cmd, oh, &tent, update) != 0)
		err(EX_OSERR, "%s", texterr);
}


static void
tentry_fill_key_type(char *arg, ipfw_obj_tentry *tentry, uint8_t type)
{
	char *p;
	int mask, af;
	struct in6_addr *paddr;
	uint32_t key, *pkey;
	int masklen;

	masklen = 0;
	af = 0;
	paddr = (struct in6_addr *)&tentry->k;

	switch (type) {
	case IPFW_TABLE_CIDR:
		/* Remove / if exists */
		if ((p = strchr(arg, '/')) != NULL) {
			*p = '\0';
			mask = atoi(p + 1);
		}

		if (inet_pton(AF_INET, arg, paddr) == 1) {
			if (p != NULL && mask > 32)
				errx(EX_DATAERR, "bad IPv4 mask width: %s",
				    p + 1);

			masklen = p ? mask : 32;
			af = AF_INET;
		} else if (inet_pton(AF_INET6, arg, paddr) == 1) {
			if (IN6_IS_ADDR_V4COMPAT(paddr))
				errx(EX_DATAERR,
				    "Use IPv4 instead of v4-compatible");
			if (p != NULL && mask > 128)
				errx(EX_DATAERR, "bad IPv6 mask width: %s",
				    p + 1);

			masklen = p ? mask : 128;
			af = AF_INET6;
		} else {
			/* Assume FQDN */
			if (lookup_host(arg, (struct in_addr *)paddr) != 0)
				errx(EX_NOHOST, "hostname ``%s'' unknown", arg);

			masklen = 32;
			type = IPFW_TABLE_CIDR;
			af = AF_INET;
		}
		break;
	case IPFW_TABLE_INTERFACE:
		/* Assume interface name. Copy significant data only */
		mask = MIN(strlen(arg), IF_NAMESIZE - 1);
		memcpy(paddr, arg, mask);
		/* Set mask to exact match */
		masklen = 8 * IF_NAMESIZE;
		break;
	case IPFW_TABLE_U32:
		/* Port or any other key */
		key = strtol(arg, &p, 10);
		if (*p != '\0')
			errx(EX_DATAERR, "Invalid number: %s", arg);

		pkey = (uint32_t *)paddr;
		*pkey = key;
		masklen = 32;
		break;
	default:
		errx(EX_DATAERR, "Unsupported table type: %d", type);
	}

	tentry->subtype = af;
	tentry->masklen = masklen;
}

static void
tentry_fill_key(ipfw_obj_header *oh, ipfw_obj_tentry *tent, char *key,
    uint8_t *ptype, uint8_t *pvtype)
{
	ipfw_xtable_info xi;
	uint8_t type, vtype;
	int error;

	type = 0;
	vtype = 0;

	/*
	 * Compability layer. Try to interpret data as CIDR first.
	 */
	if (inet_pton(AF_INET, key, &tent->k.addr6) == 1 ||
	    inet_pton(AF_INET6, key, &tent->k.addr6) == 1) {
		/* OK Prepare and send */
		type = IPFW_TABLE_CIDR;
	} else {

		/*
		 * Non-CIDR of FQDN hostname. Ask kernel
		 * about given table.
		 */
		error = table_get_info(oh, &xi);
		if (error == ESRCH)
			errx(EX_USAGE, "Table %s does not exist, cannot intuit "
			    "key type", oh->ntlv.name);
		else if (error != 0)
			errx(EX_OSERR, "Error requesting table %s info",
			    oh->ntlv.name);

		/* Table found. */
		type = xi.type;
		vtype = xi.vtype;
	}

	tentry_fill_key_type(key, tent, type);

	*ptype = type;
	*pvtype = vtype;
}

static void
tentry_fill_value(ipfw_obj_header *oh, ipfw_obj_tentry *tent, char *arg,
    uint8_t type, uint8_t vtype)
{
	ipfw_xtable_info xi;
	int error;
	int code;
	char *p;

	if (vtype == 0) {
		/* Format type is unknown, ask kernel */
		error = table_get_info(oh, &xi);
		if (error == ESRCH) {

			/*
			 * XXX: This one may break some scripts.
			 * Change this behavior for MFC.
			 */
			errx(EX_USAGE, "Table %s does not exist. Unable to "
			    "guess value format.",  oh->ntlv.name);
		} else if (error != 0)
			errx(EX_OSERR, "Error requesting table %s info",
			    oh->ntlv.name);

		vtype = xi.vtype;
	}

	switch (vtype) {
	case IPFW_VTYPE_U32:
		tent->value = strtoul(arg, &p, 0);
		if (*p != '\0')
			errx(EX_USAGE, "Invalid number: %s", arg);
		break;
	case IPFW_VTYPE_IP:
		if (inet_pton(AF_INET, arg, &tent->value) == 1)
			break;
		/* Try hostname */
		if (lookup_host(arg, (struct in_addr *)&tent->value) != 0)
			errx(EX_USAGE, "Invalid IPv4 address: %s", arg);
		break;
	case IPFW_VTYPE_DSCP:
		if (isalpha(*arg)) {
			if ((code = match_token(f_ipdscp, arg)) == -1)
				errx(EX_DATAERR, "Unknown DSCP code");
		} else {
			code = strtoul(arg, NULL, 10);
			if (code < 0 || code > 63)
				errx(EX_DATAERR, "Invalid DSCP value");
		}
		tent->value = code;
		break;
	default:
		errx(EX_OSERR, "Unsupported format type %d", vtype);
	}
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
	 * [a-zA-Z0-9\-_\.]{1,63}
	 */
	l = strlen(tablename);
	if (l == 0 || l >= 64)
		return (EINVAL);
	for (i = 0; i < l; i++) {
		c = tablename[i];
		if (isalpha(c) || isdigit(c) || c == '_' ||
		    c == '-' || c == '.')
			continue;
		return (EINVAL);	
	}

	/* Restrict some 'special' names */
	if (strcmp(tablename, "all") == 0)
		return (EINVAL);

	return (0);
}

