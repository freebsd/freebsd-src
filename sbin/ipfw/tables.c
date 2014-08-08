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
static void table_lookup(ipfw_obj_header *oh, int ac, char *av[]);
static int table_do_swap(ipfw_obj_header *oh, char *second);
static int table_swap(ipfw_obj_header *oh, char *second);
static int table_get_info(ipfw_obj_header *oh, ipfw_xtable_info *i);
static int table_show_info(ipfw_xtable_info *i, void *arg);
static void table_fill_ntlv(ipfw_obj_ntlv *ntlv, char *name, uint32_t set,
    uint16_t uidx);

static int table_flush_one(ipfw_xtable_info *i, void *arg);
static int table_show_one(ipfw_xtable_info *i, void *arg);
static int table_get_list(ipfw_xtable_info *i, ipfw_obj_header *oh);
static void table_show_list(ipfw_obj_header *oh, int need_header);
static void table_show_entry(ipfw_xtable_info *i, ipfw_obj_tentry *tent);

static void tentry_fill_key(ipfw_obj_header *oh, ipfw_obj_tentry *tent,
    char *key, uint8_t *ptype, uint8_t *pvtype, ipfw_xtable_info *xi);
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
      { "number",	IPFW_TABLE_NUMBER },
      { "flow",		IPFW_TABLE_FLOW },
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
      { "delete",	TOK_DEL },
      { "create",	TOK_CREATE },
      { "destroy",	TOK_DESTROY },
      { "flush",	TOK_FLUSH },
      { "swap",		TOK_SWAP },
      { "info",		TOK_INFO },
      { "detail",	TOK_DETAIL },
      { "list",		TOK_LIST },
      { "lookup",	TOK_LOOKUP },
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
	void *arg;

	memset(&oh, 0, sizeof(oh));
	is_all = 0;
	if (co.use_set != 0)
		set = co.use_set - 1;
	else
		set = 0;

	ac--; av++;
	NEED1("table needs name");
	tablename = *av;

	if (table_check_name(tablename) == 0) {
		table_fill_ntlv(&oh.ntlv, *av, set, 1);
		oh.idx = 1;
	} else {
		if (strcmp(tablename, "all") == 0)
			is_all = 1;
		else
			errx(EX_USAGE, "table name %s is invalid", tablename);
	}
	ac--; av++;
	NEED1("table needs command");

	if ((tcmd = match_token(tablecmds, *av)) == -1)
		errx(EX_USAGE, "invalid table command %s", *av);

	switch (tcmd) {
	case TOK_LIST:
	case TOK_INFO:
	case TOK_DETAIL:
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
	case TOK_SWAP:
		ac--; av++;
		NEED1("second table name required");
		table_swap(&oh, *av);
		break;
	case TOK_DETAIL:
	case TOK_INFO:
		arg = (tcmd == TOK_DETAIL) ? (void *)1 : NULL;
		if (is_all == 0) {
			if ((error = table_get_info(&oh, &i)) != 0)
				err(EX_OSERR, "failed to request table info");
			table_show_info(&i, arg);
		} else {
			error = tables_foreach(table_show_info, arg, 1);
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
	case TOK_LOOKUP:
		ac--; av++;
		table_lookup(&oh, ac, av);
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

	oh->idx = 1;
	table_fill_ntlv(&oh->ntlv, i->tablename, i->set, 1);
}

static struct _s_x tablenewcmds[] = {
      { "type",		TOK_TYPE },
      { "valtype",	TOK_VALTYPE },
      { "algo",		TOK_ALGO },
      { "limit",	TOK_LIMIT },
      { NULL, 0 }
};

static struct _s_x flowtypecmds[] = {
      { "src-ip",	IPFW_TFFLAG_SRCIP },
      { "proto",	IPFW_TFFLAG_PROTO },
      { "src-port",	IPFW_TFFLAG_SRCPORT },
      { "dst-ip",	IPFW_TFFLAG_DSTIP },
      { "dst-port",	IPFW_TFFLAG_DSTPORT },
      { NULL, 0 }
};

int
table_parse_type(uint8_t ttype, char *p, uint8_t *tflags)
{
	uint8_t fset, fclear;

	/* Parse type options */
	switch(ttype) {
	case IPFW_TABLE_FLOW:
		fset = fclear = 0;
		fill_flags(flowtypecmds, p, &fset,
		    &fclear);
		*tflags = fset;
		break;
	default:
		return (EX_USAGE);
	}

	return (0);
}

void
table_print_type(char *tbuf, size_t size, uint8_t type, uint8_t tflags)
{
	const char *tname;
	int l;

	if ((tname = match_value(tabletypes, type)) == NULL)
		tname = "unknown";

	l = snprintf(tbuf, size, "%s", tname);
	tbuf += l;
	size -= l;

	switch(type) {
	case IPFW_TABLE_FLOW:
		if (tflags != 0) {
			*tbuf++ = ':';
			l--;
			print_flags_buffer(tbuf, size, flowtypecmds, tflags);
		}
		break;
	}
}

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
	char *p;
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
		case TOK_LIMIT:
			NEED1("limit value required");
			xi.limit = strtol(*av, NULL, 10);
			ac--; av++;
			break;
		case TOK_TYPE:
			NEED1("table type required");
			/* Type may have suboptions after ':' */
			if ((p = strchr(*av, ':')) != NULL)
				*p++ = '\0';
			val = match_token(tabletypes, *av);
			if (val == -1) {
				concat_tokens(tbuf, sizeof(tbuf), tabletypes,
				    ", ");
				errx(EX_USAGE,
				    "Unknown tabletype: %s. Supported: %s",
				    *av, tbuf);
			}
			xi.type = val;
			if (p != NULL) {
				error = table_parse_type(val, p, &xi.tflags);
				if (error != 0)
					errx(EX_USAGE,
					    "Unsupported suboptions: %s", p);
			}
			ac--; av++;
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

static int
table_do_swap(ipfw_obj_header *oh, char *second)
{
	char tbuf[sizeof(ipfw_obj_header) + sizeof(ipfw_obj_ntlv)];
	int error;

	memset(tbuf, 0, sizeof(tbuf));
	memcpy(tbuf, oh, sizeof(*oh));
	oh = (ipfw_obj_header *)tbuf;
	table_fill_ntlv((ipfw_obj_ntlv *)(oh + 1), second, oh->ntlv.set, 1);

	error = do_set3(IP_FW_TABLE_XSWAP, &oh->opheader, sizeof(tbuf));

	return (error);
}

/*
 * Swaps given table with @second one.
 */
static int
table_swap(ipfw_obj_header *oh, char *second)
{
	int error;

	if (table_check_name(second) != 0)
		errx(EX_USAGE, "table name %s is invalid", second);

	error = table_do_swap(oh, second);

	switch (error) {
	case EINVAL:
		errx(EX_USAGE, "Unable to swap table: check types");
	case EFBIG:
		errx(EX_USAGE, "Unable to swap table: check limits");
	}

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

static struct _s_x tablealgoclass[] = {
      { "hash",		IPFW_TACLASS_HASH },
      { "array",	IPFW_TACLASS_ARRAY },
      { "radix",	IPFW_TACLASS_RADIX },
      { NULL, 0 }
};

struct ta_cldata {
	uint8_t		taclass;
	uint8_t		spare4;
	uint16_t	itemsize;
	uint16_t	itemsize6;
	uint32_t	size;	
	uint32_t	count;
};

/*
 * Print global/per-AF table @i algorithm info.
 */
static void
table_show_tainfo(ipfw_xtable_info *i, struct ta_cldata *d,
    const char *af, const char *taclass)
{

	switch (d->taclass) {
	case IPFW_TACLASS_HASH:
	case IPFW_TACLASS_ARRAY:
		printf(" %salgorithm %s info\n", af, taclass);
		if (d->itemsize == d->itemsize6)
			printf("  size: %u items: %u itemsize: %u\n",
			    d->size, d->count, d->itemsize);
		else
			printf("  size: %u items: %u "
			    "itemsize4: %u itemsize6: %u\n",
			    d->size, d->count,
			    d->itemsize, d->itemsize6);
		break;
	case IPFW_TACLASS_RADIX:
		printf(" %salgorithm %s info\n", af, taclass);
		if (d->itemsize == d->itemsize6)
			printf("  items: %u itemsize: %u\n",
			    d->count, d->itemsize);
		else
			printf("  items: %u "
			    "itemsize4: %u itemsize6: %u\n",
			    d->count, d->itemsize, d->itemsize6);
		break;
	default:
		printf(" algo class: %s\n", taclass);
	}
}

/*
 * Prints table info struct @i in human-readable form.
 */
static int
table_show_info(ipfw_xtable_info *i, void *arg)
{
	const char *vtype;
	ipfw_ta_tinfo *tainfo;
	int afdata, afitem;
	struct ta_cldata d;
	char ttype[64];

	table_print_type(ttype, sizeof(ttype), i->type, i->tflags);
	if ((vtype = match_value(tablevaltypes, i->vtype)) == NULL)
		vtype = "unknown";

	printf("--- table(%s), set(%u) ---\n", i->tablename, i->set);
	printf(" kindex: %d, type: %s\n", i->kidx, ttype);
	printf(" valtype: %s, references: %u\n", vtype, i->refcnt);
	printf(" algorithm: %s\n", i->algoname);
	printf(" items: %u, size: %u\n", i->count, i->size);
	if (i->limit > 0)
		printf(" limit: %u\n", i->limit);

	/* Print algo-specific info if requested & set  */
	if (arg == NULL)
		return (0);

	if ((i->ta_info.flags & IPFW_TATFLAGS_DATA) == 0)
		return (0);
	tainfo = &i->ta_info;

	afdata = 0;
	afitem = 0;
	if (tainfo->flags & IPFW_TATFLAGS_AFDATA)
		afdata = 1;
	if (tainfo->flags & IPFW_TATFLAGS_AFITEM)
		afitem = 1;

	memset(&d, 0, sizeof(d));
	d.taclass = tainfo->taclass4;
	d.size = tainfo->size4;
	d.count = tainfo->count4;
	d.itemsize = tainfo->itemsize4;
	if (afdata == 0 && afitem != 0)
		d.itemsize6 = tainfo->itemsize6;
	else
		d.itemsize6 = d.itemsize;
	if ((vtype = match_value(tablealgoclass, d.taclass)) == NULL)
		vtype = "unknown";

	if (afdata == 0) {
		table_show_tainfo(i, &d, "", vtype);
	} else {
		table_show_tainfo(i, &d, "IPv4 ", vtype);
		memset(&d, 0, sizeof(d));
		d.taclass = tainfo->taclass6;
		if ((vtype = match_value(tablealgoclass, d.taclass)) == NULL)
			vtype = "unknown";
		d.size = tainfo->size6;
		d.count = tainfo->count6;
		d.itemsize = tainfo->itemsize6;
		d.itemsize6 = d.itemsize;
		table_show_tainfo(i, &d, "IPv6 ", vtype);
	}

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
	int error;

	if ((oh = calloc(1, i->size)) == NULL)
		return (ENOMEM);

	if ((error = table_get_list(i, oh)) != 0) {
		err(EX_OSERR, "Error requesting table %s list", i->tablename);
		return (error);
	}

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
	ipfw_obj_ctlv *ctlv;
	char xbuf[sizeof(*oh) + sizeof(ipfw_obj_ctlv) + sizeof(*tent)];
	int error;

	memset(xbuf, 0, sizeof(xbuf));
	memcpy(xbuf, oh, sizeof(*oh));
	oh = (ipfw_obj_header *)xbuf;
	oh->opheader.version = 1;

	ctlv = (ipfw_obj_ctlv *)(oh + 1);
	ctlv->count = 1;
	ctlv->head.length = sizeof(*ctlv) + sizeof(*tent);

	memcpy(ctlv + 1, tent, sizeof(*tent));
	tent = (ipfw_obj_tentry *)(ctlv + 1);
	if (update != 0)
		tent->head.flags |= IPFW_TF_UPDATE;
	tent->head.length = sizeof(ipfw_obj_tentry);

	error = do_set3(cmd, &oh->opheader, sizeof(xbuf));

	return (error);
}

static void
table_modify_record(ipfw_obj_header *oh, int ac, char *av[], int add, int update)
{
	ipfw_obj_tentry tent;
	ipfw_xtable_info xi;
	uint8_t type, vtype;
	int cmd, error;
	char *texterr, *etxt;

	if (ac == 0)
		errx(EX_USAGE, "address required");
	
	memset(&tent, 0, sizeof(tent));
	tent.head.length = sizeof(tent);
	tent.idx = 1;

	tentry_fill_key(oh, &tent, *av, &type, &vtype, &xi);

	/*
	 * compability layer: auto-create table if not exists
	 */
	if (xi.tablename[0] == '\0') {
		xi.type = type;
		xi.vtype = vtype;
		strlcpy(xi.tablename, oh->ntlv.name, sizeof(xi.tablename));
		fprintf(stderr, "DEPRECATED: inserting data info non-existent "
		    "table %s. (auto-created)\n", xi.tablename);
		table_do_create(oh, &xi);
	}

	oh->ntlv.type = type;
	ac--; av++;

	if (add != 0) {
		if (ac > 0)
			tentry_fill_value(oh, &tent, *av, type, vtype);
		cmd = IP_FW_TABLE_XADD;
		texterr = "Adding record failed";
	} else {
		cmd = IP_FW_TABLE_XDEL;
		texterr = "Deleting record failed";
	}

	if ((error = table_do_modify_record(cmd, oh, &tent, update)) == 0)
		return;

	/* Try to provide more human-readable error */
	switch (error) {
	case EEXIST:
		etxt = "record already exists";
		break;
	case EFBIG:
		etxt = "limit hit";
		break;
	case ESRCH:
		etxt = "table not found";
		break;
	case ENOENT:
		etxt = "record not found";
		break;
	default:
		etxt = strerror(error);
	}

	errx(EX_OSERR, "%s: %s", texterr, etxt);
}

static int
table_do_lookup(ipfw_obj_header *oh, char *key, ipfw_xtable_info *xi,
    ipfw_obj_tentry *xtent)
{
	char xbuf[sizeof(ipfw_obj_header) + sizeof(ipfw_obj_tentry)];
	ipfw_obj_tentry *tent;
	uint8_t type, vtype;
	int error;
	size_t sz;

	memcpy(xbuf, oh, sizeof(*oh));
	oh = (ipfw_obj_header *)xbuf;
	tent = (ipfw_obj_tentry *)(oh + 1);

	memset(tent, 0, sizeof(*tent));
	tent->head.length = sizeof(*tent);
	tent->idx = 1;

	tentry_fill_key(oh, tent, key, &type, &vtype, xi);
	oh->ntlv.type = type;

	sz = sizeof(xbuf);
	if ((error = do_get3(IP_FW_TABLE_XFIND, &oh->opheader, &sz)) != 0)
		return (error);

	if (sz < sizeof(xbuf))
		return (EINVAL);

	*xtent = *tent;

	return (0);
}

static void
table_lookup(ipfw_obj_header *oh, int ac, char *av[])
{
	ipfw_obj_tentry xtent;
	ipfw_xtable_info xi;
	char key[64];
	int error;

	if (ac == 0)
		errx(EX_USAGE, "address required");

	strlcpy(key, *av, sizeof(key));

	error = table_do_lookup(oh, key, &xi, &xtent);

	switch (error) {
	case 0:
		break;
	case ESRCH:
		errx(EX_UNAVAILABLE, "Table %s not found", oh->ntlv.name);
	case ENOENT:
		errx(EX_UNAVAILABLE, "Entry %s not found", *av);
	case ENOTSUP:
		errx(EX_UNAVAILABLE, "Table %s algo does not support "
		    "\"lookup\" method", oh->ntlv.name);
	default:
		err(EX_OSERR, "getsockopt(IP_FW_TABLE_XFIND)");
	}

	table_show_entry(&xi, &xtent);
}

static void
tentry_fill_key_type(char *arg, ipfw_obj_tentry *tentry, uint8_t type,
    uint8_t tflags)
{
	char *p, *pp;
	int mask, af;
	struct in6_addr *paddr, tmp;
	struct tflow_entry *tfe;
	uint32_t key, *pkey;
	uint16_t port;
	struct protoent *pent;
	struct servent *sent;
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
	case IPFW_TABLE_NUMBER:
		/* Port or any other key */
		key = strtol(arg, &p, 10);
		if (*p != '\0')
			errx(EX_DATAERR, "Invalid number: %s", arg);

		pkey = (uint32_t *)paddr;
		*pkey = key;
		masklen = 32;
		break;
	case IPFW_TABLE_FLOW:
		/* Assume [src-ip][,proto][,src-port][,dst-ip][,dst-port] */
		tfe = &tentry->k.flow;
		af = 0;

		/* Handle <ipv4|ipv6>*/
		if ((tflags & IPFW_TFFLAG_SRCIP) != 0) {
			if ((p = strchr(arg, ',')) != NULL)
				*p++ = '\0';
			/* Determine family using temporary storage */
			if (inet_pton(AF_INET, arg, &tmp) == 1) {
				if (af != 0 && af != AF_INET)
					errx(EX_DATAERR,
					    "Inconsistent address family\n");
				af = AF_INET;
				memcpy(&tfe->a.a4.sip, &tmp, 4);
			} else if (inet_pton(AF_INET6, arg, &tmp) == 1) {
				if (af != 0 && af != AF_INET6)
					errx(EX_DATAERR,
					    "Inconsistent address family\n");
				af = AF_INET6;
				memcpy(&tfe->a.a6.sip6, &tmp, 16);
			}

			arg = p;
		}

		/* Handle <proto-num|proto-name> */
		if ((tflags & IPFW_TFFLAG_PROTO) != 0) {
			if ((p = strchr(arg, ',')) != NULL)
				*p++ = '\0';

			key = strtol(arg, &pp, 10);
			if (*pp != '\0') {
				if ((pent = getprotobyname(arg)) == NULL)
					errx(EX_DATAERR, "Unknown proto: %s",
					    arg);
				else
					key = pent->p_proto;
			}
			
			if (key > 255)
				errx(EX_DATAERR, "Bad protocol number: %u",key);

			tfe->proto = key;

			arg = p;
		}

		/* Handle <port-num|service-name> */
		if ((tflags & IPFW_TFFLAG_SRCPORT) != 0) {
			if ((p = strchr(arg, ',')) != NULL)
				*p++ = '\0';

			if ((port = htons(strtol(arg, NULL, 10))) == 0) {
				if ((sent = getservbyname(arg, NULL)) == NULL)
					errx(EX_DATAERR, "Unknown service: %s",
					    arg);
				else
					key = sent->s_port;
			}
			
			tfe->sport = port;

			arg = p;
		}

		/* Handle <ipv4|ipv6>*/
		if ((tflags & IPFW_TFFLAG_DSTIP) != 0) {
			if ((p = strchr(arg, ',')) != NULL)
				*p++ = '\0';
			/* Determine family using temporary storage */
			if (inet_pton(AF_INET, arg, &tmp) == 1) {
				if (af != 0 && af != AF_INET)
					errx(EX_DATAERR,
					    "Inconsistent address family");
				af = AF_INET;
				memcpy(&tfe->a.a4.dip, &tmp, 4);
			} else if (inet_pton(AF_INET6, arg, &tmp) == 1) {
				if (af != 0 && af != AF_INET6)
					errx(EX_DATAERR,
					    "Inconsistent address family");
				af = AF_INET6;
				memcpy(&tfe->a.a6.dip6, &tmp, 16);
			}

			arg = p;
		}

		/* Handle <port-num|service-name> */
		if ((tflags & IPFW_TFFLAG_DSTPORT) != 0) {
			if ((p = strchr(arg, ',')) != NULL)
				*p++ = '\0';

			if ((port = htons(strtol(arg, NULL, 10))) == 0) {
				if ((sent = getservbyname(arg, NULL)) == NULL)
					errx(EX_DATAERR, "Unknown service: %s",
					    arg);
				else
					key = sent->s_port;
			}
			
			tfe->dport = port;

			arg = p;
		}

		tfe->af = af;

		break;
	
	default:
		errx(EX_DATAERR, "Unsupported table type: %d", type);
	}

	tentry->subtype = af;
	tentry->masklen = masklen;
}

static void
tentry_fill_key(ipfw_obj_header *oh, ipfw_obj_tentry *tent, char *key,
    uint8_t *ptype, uint8_t *pvtype, ipfw_xtable_info *xi)
{
	uint8_t type, tflags, vtype;
	int error;
	char *del;

	type = 0;
	tflags = 0;
	vtype = 0;

	error = table_get_info(oh, xi);

	if (error == 0) {
		/* Table found. */
		type = xi->type;
		tflags = xi->tflags;
		vtype = xi->vtype;
	} else {
		if (error != ESRCH)
			errx(EX_OSERR, "Error requesting table %s info",
			    oh->ntlv.name);
		/*
		 * Table does not exist.
		 * Compability layer: try to interpret data as CIDR
		 * before failing.
		 */
		if ((del = strchr(key, '/')) != NULL)
			*del = '\0';
		if (inet_pton(AF_INET, key, &tent->k.addr6) == 1 ||
		    inet_pton(AF_INET6, key, &tent->k.addr6) == 1) {
			/* OK Prepare and send */
			type = IPFW_TABLE_CIDR;
			/*
			 * XXX: Value type is forced to be u32.
			 * This should be changed for MFC.
			 */
			vtype = IPFW_VTYPE_U32;
		} else {
			/* Inknown key */
			errx(EX_USAGE, "Table %s does not exist, cannot guess "
			    "key '%s' type", oh->ntlv.name, key);
		}
		if (del != NULL)
			*del = '/';
	}

	tentry_fill_key_type(key, tent, type, tflags);

	*ptype = type;
	*pvtype = vtype;
}

static void
tentry_fill_value(ipfw_obj_header *oh, ipfw_obj_tentry *tent, char *arg,
    uint8_t type, uint8_t vtype)
{
	int code;
	char *p;

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

	ia = (ipfw_xtable_info *)a;
	ib = (ipfw_xtable_info *)b;

	return (stringnum_cmp(ia->tablename, ib->tablename));
}

/*
 * Retrieves table list from kernel,
 * optionally sorts it and calls requested function for each table.
 * Returns 0 on success.
 */
static int
tables_foreach(table_cb_t *f, void *arg, int sort)
{
	ipfw_obj_lheader *olh;
	ipfw_xtable_info *info;
	size_t sz;
	int i, error;

	/* Start with reasonable default */
	sz = sizeof(*olh) + 16 * sizeof(ipfw_xtable_info);

	for (;;) {
		if ((olh = calloc(1, sz)) == NULL)
			return (ENOMEM);

		olh->size = sz;
		error = do_get3(IP_FW_TABLES_XLIST, &olh->opheader, &sz);
		if (error == ENOMEM) {
			sz = olh->size;
			free(olh);
			continue;
		} else if (error != 0) {
			free(olh);
			return (error);
		}

		if (sort != 0)
			qsort(olh + 1, olh->count, olh->objsize, tablename_cmp);

		info = (ipfw_xtable_info *)(olh + 1);
		for (i = 0; i < olh->count; i++) {
			error = f(info, arg); /* Ignore errors for now */
			info = (ipfw_xtable_info *)((caddr_t)info + olh->objsize);
		}

		free(olh);
		break;
	}

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
	int error, c;

	sz = 0;
	for (c = 0; c < 3; c++) {
		table_fill_objheader(oh, i);
		if (sz < i->size)
			sz = i->size;

		oh->opheader.version = 1; /* Current version */
		error = do_get3(IP_FW_TABLE_XLIST, &oh->opheader, &sz);

		if (error != ENOMEM)
			return (errno);
	}

	return (ENOMEM);
}

/*
 * Shows all entries from @oh in human-readable format
 */
static void
table_show_list(ipfw_obj_header *oh, int need_header)
{
	ipfw_obj_tentry *tent;
	uint32_t count;
	ipfw_xtable_info *i;

	i = (ipfw_xtable_info *)(oh + 1);
	tent = (ipfw_obj_tentry *)(i + 1);

	if (need_header)
		printf("--- table(%s), set(%u) ---\n", i->tablename, i->set);

	count = i->count;
	while (count > 0) {
		table_show_entry(i, tent);
		tent = (ipfw_obj_tentry *)((caddr_t)tent + tent->head.length);
		count--;
	}
}

static void
table_show_entry(ipfw_xtable_info *i, ipfw_obj_tentry *tent)
{
	char *comma, tbuf[128], pval[32];
	void *paddr;
	uint32_t tval;
	struct tflow_entry *tfe;

	tval = tent->value;

	if (co.do_value_as_ip) {
		tval = htonl(tval);
		inet_ntop(AF_INET, &tval, pval, sizeof(pval));
	} else
		snprintf(pval, sizeof(pval), "%u", tval);

	switch (i->type) {
	case IPFW_TABLE_CIDR:
		/* IPv4 or IPv6 prefixes */
		inet_ntop(tent->subtype, &tent->k, tbuf, sizeof(tbuf));
		printf("%s/%u %s\n", tbuf, tent->masklen, pval);
		break;
	case IPFW_TABLE_INTERFACE:
		/* Interface names */
		printf("%s %s\n", tent->k.iface, pval);
		break;
	case IPFW_TABLE_NUMBER:
		/* numbers */
		printf("%u %s\n", tent->k.key, pval);
		break;
	case IPFW_TABLE_FLOW:
		/* flows */
		tfe = &tent->k.flow;
		comma = "";

		if ((i->tflags & IPFW_TFFLAG_SRCIP) != 0) {
			if (tfe->af == AF_INET)
				paddr = &tfe->a.a4.sip;
			else
				paddr = &tfe->a.a6.sip6;

			inet_ntop(tfe->af, paddr, tbuf, sizeof(tbuf));
			printf("%s%s", comma, tbuf);
			comma = ",";
		}

		if ((i->tflags & IPFW_TFFLAG_PROTO) != 0) {
			printf("%s%d", comma, tfe->proto);
			comma = ",";
		}

		if ((i->tflags & IPFW_TFFLAG_SRCPORT) != 0) {
			printf("%s%d", comma, ntohs(tfe->sport));
			comma = ",";
		}
		if ((i->tflags & IPFW_TFFLAG_DSTIP) != 0) {
			if (tfe->af == AF_INET)
				paddr = &tfe->a.a4.dip;
			else
				paddr = &tfe->a.a6.dip6;

			inet_ntop(tfe->af, paddr, tbuf, sizeof(tbuf));
			printf("%s%s", comma, tbuf);
			comma = ",";
		}

		if ((i->tflags & IPFW_TFFLAG_DSTPORT) != 0) {
			printf("%s%d", comma, ntohs(tfe->dport));
			comma = ",";
		}

		printf(" %s\n", pval);
	}
}

static int
table_do_get_algolist(ipfw_obj_lheader **polh)
{
	ipfw_obj_lheader req, *olh;
	size_t sz;
	int error;

	memset(&req, 0, sizeof(req));
	sz = sizeof(req);

	error = do_get3(IP_FW_TABLES_ALIST, &req.opheader, &sz);
	if (error != 0 && error != ENOMEM)
		return (error);

	sz = req.size;
	if ((olh = calloc(1, sz)) == NULL)
		return (ENOMEM);

	olh->size = sz;
	if ((error = do_get3(IP_FW_TABLES_ALIST, &olh->opheader, &sz)) != 0) {
		free(olh);
		return (error);
	}

	*polh = olh;
	return (0);
}

void
ipfw_list_ta(int ac, char *av[])
{
	ipfw_obj_lheader *olh;
	ipfw_ta_info *info;
	int error, i;
	const char *atype;

	error = table_do_get_algolist(&olh);
	if (error != 0)
		err(EX_OSERR, "Unable to request algorithm list");

	info = (ipfw_ta_info *)(olh + 1);
	for (i = 0; i < olh->count; i++) {
		if ((atype = match_value(tabletypes, info->type)) == NULL)
			atype = "unknown";
		printf("--- %s ---\n", info->algoname);
		printf(" type: %s\n refcount: %u\n", atype, info->refcnt);

		info = (ipfw_ta_info *)((caddr_t)info + olh->objsize);
	}

	free(olh);
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

