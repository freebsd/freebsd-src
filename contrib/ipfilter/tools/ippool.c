/*	$NetBSD$	*/

/*
 * Copyright (C) 2003 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 */
#include <sys/types.h>
#include <sys/time.h>
#include <sys/param.h>
#include <sys/socket.h>
#if defined(BSD) && (BSD >= 199306)
# include <sys/cdefs.h>
#endif
#include <sys/ioctl.h>

#include <net/if.h>
#if __FreeBSD_version >= 300000
# include <net/if_var.h>
#endif
#include <netinet/in.h>

#include <arpa/inet.h>

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <ctype.h>
#include <unistd.h>

#include "ipf.h"
#include "netinet/ip_lookup.h"
#include "netinet/ip_pool.h"
#include "netinet/ip_htable.h"
#include "kmem.h"


extern	int	ippool_yyparse __P((void));
extern	int	ippool_yydebug;
extern	FILE	*ippool_yyin;
extern	char	*optarg;
extern	int	lineNum;

void	showpools __P((ip_pool_stat_t *));
void	usage __P((char *));
int	main __P((int, char **));
int	poolcommand __P((int, int, char *[]));
int	poolnodecommand __P((int, int, char *[]));
int	loadpoolfile __P((int, char *[], char *));
int	poollist __P((int, char *[]));
int	poolflush __P((int, char *[]));
int	poolstats __P((int, char *[]));
int	gettype __P((char *, u_int *));
int	getrole __P((char *));

int	opts = 0;
int	fd = -1;
int	use_inet6 = 0;


void usage(prog)
char *prog;
{
	fprintf(stderr, "Usage:\t%s\n", prog);
	fprintf(stderr, "\t\t\t-a [-dnv] [-m <name>] [-o <role>] -i <ipaddr>[/netmask]\n");
	fprintf(stderr, "\t\t\t-A [-dnv] [-m <name>] [-o <role>] [-S <seed>] [-t <type>]\n");
	fprintf(stderr, "\t\t\t-f <file> [-dnuv]\n");
	fprintf(stderr, "\t\t\t-F [-dv] [-o <role>] [-t <type>]\n");
	fprintf(stderr, "\t\t\t-l [-dv] [-m <name>] [-t <type>]\n");
	fprintf(stderr, "\t\t\t-r [-dnv] [-m <name>] [-o <role>] -i <ipaddr>[/netmask]\n");
	fprintf(stderr, "\t\t\t-R [-dnv] [-m <name>] [-o <role>] [-t <type>]\n");
	fprintf(stderr, "\t\t\t-s [-dtv] [-M <core>] [-N <namelist>]\n");
	exit(1);
}


int main(argc, argv)
int argc;
char *argv[];
{
	int err;

	if (argc < 2)
		usage(argv[0]);

	switch (getopt(argc, argv, "aAf:FlrRs"))
	{
	case 'a' :
		err = poolnodecommand(0, argc, argv);
		break;
	case 'A' :
		err = poolcommand(0, argc, argv);
		break;
	case 'f' :
		err = loadpoolfile(argc, argv, optarg);
		break;
	case 'F' :
		err = poolflush(argc, argv);
		break;
	case 'l' :
		err = poollist(argc, argv);
		break;
	case 'r' :
		err = poolnodecommand(1, argc, argv);
		break;
	case 'R' :
		err = poolcommand(1, argc, argv);
		break;
	case 's' :
		err = poolstats(argc, argv);
		break;
	default :
		exit(1);
	}

	return err;
}


int poolnodecommand(remove, argc, argv)
int remove, argc;
char *argv[];
{
	char *poolname = NULL, *s;
	int err, c, ipset, role;
	ip_pool_node_t node;
	struct in_addr mask;

	ipset = 0;
	role = IPL_LOGIPF;
	bzero((char *)&node, sizeof(node));

	while ((c = getopt(argc, argv, "di:m:no:Rv")) != -1)
		switch (c)
		{
		case 'd' :
			opts |= OPT_DEBUG;
			ippool_yydebug++;
			break;
		case 'i' :
			s = strchr(optarg, '/');
			if (s == NULL)
				mask.s_addr = 0xffffffff;
			else if (strchr(s, '.') == NULL) {
				if (ntomask(4, atoi(s + 1), &mask.s_addr) != 0)
					return -1;
			} else {
				mask.s_addr = inet_addr(s + 1);
			}
			if (s != NULL)
				*s = '\0';
			ipset = 1;
			node.ipn_addr.adf_len = sizeof(node.ipn_addr);
			node.ipn_addr.adf_addr.in4.s_addr = inet_addr(optarg);
			node.ipn_mask.adf_len = sizeof(node.ipn_mask);
			node.ipn_mask.adf_addr.in4.s_addr = mask.s_addr;
			break;
		case 'm' :
			poolname = optarg;
			break;
		case 'n' :
			opts |= OPT_DONOTHING;
			break;
		case 'o' :
			role = getrole(optarg);
			if (role == IPL_LOGNONE)
				return -1;
			break;
		case 'R' :
			opts |= OPT_NORESOLVE;
			break;
		case 'v' :
			opts |= OPT_VERBOSE;
			break;
		}

	if (opts & OPT_DEBUG)
		fprintf(stderr, "poolnodecommand: opts = %#x\n", opts);

	if (ipset == 0)
		return -1;
	if (poolname == NULL) {
		fprintf(stderr, "poolname not given with add/remove node\n");
		return -1;
	}

	if (remove == 0)
		err = load_poolnode(0, poolname, &node, ioctl);
	else
		err = remove_poolnode(0, poolname, &node, ioctl);
	return err;
}


int poolcommand(remove, argc, argv)
int remove, argc;
char *argv[];
{
	int type, role, c, err;
	char *poolname;
	iphtable_t iph;
	ip_pool_t pool;

	err = 1;
	role = 0;
	type = 0;
	poolname = NULL;
	role = IPL_LOGIPF;
	bzero((char *)&iph, sizeof(iph));
	bzero((char *)&pool, sizeof(pool));

	while ((c = getopt(argc, argv, "dm:no:RSt:v")) != -1)
		switch (c)
		{
		case 'd' :
			opts |= OPT_DEBUG;
			ippool_yydebug++;
			break;
		case 'm' :
			poolname = optarg;
			break;
		case 'n' :
			opts |= OPT_DONOTHING;
			break;
		case 'o' :
			role = getrole(optarg);
			if (role == IPL_LOGNONE) {
				fprintf(stderr, "unknown role '%s'\n", optarg);
				return -1;
			}
			break;
		case 'R' :
			opts |= OPT_NORESOLVE;
			break;
		case 'S' :
			iph.iph_seed = atoi(optarg);
			break;
		case 't' :
			type = gettype(optarg, &iph.iph_type);
			if (type == IPLT_NONE) {
				fprintf(stderr, "unknown type '%s'\n", optarg);
				return -1;
			}
			break;
		case 'v' :
			opts |= OPT_VERBOSE;
			break;
		}

	if (opts & OPT_DEBUG)
		fprintf(stderr, "poolcommand: opts = %#x\n", opts);

	if (poolname == NULL) {
		fprintf(stderr, "poolname not given with add/remove pool\n");
		return -1;
	}

	if (type == IPLT_HASH) {
		strncpy(iph.iph_name, poolname, sizeof(iph.iph_name));
		iph.iph_name[sizeof(iph.iph_name) - 1] = '\0';
		iph.iph_unit = role;
	} else if (type == IPLT_POOL) {
		strncpy(pool.ipo_name, poolname, sizeof(pool.ipo_name));
		pool.ipo_name[sizeof(pool.ipo_name) - 1] = '\0';
		pool.ipo_unit = role;
	}

	if (remove == 0) {
		switch (type)
		{
		case IPLT_HASH :
			err = load_hash(&iph, NULL, ioctl);
			break;
		case IPLT_POOL :
			err = load_pool(&pool, ioctl);
			break;
		}
	} else {
		switch (type)
		{
		case IPLT_HASH :
			err = remove_hash(&iph, ioctl);
			break;
		case IPLT_POOL :
			err = remove_pool(&pool, ioctl);
			break;
		}
	}
	return err;
}


int loadpoolfile(argc, argv, infile)
int argc;
char *argv[], *infile;
{
	int c;

	infile = optarg;

	while ((c = getopt(argc, argv, "dnRuv")) != -1)
		switch (c)
		{
		case 'd' :
			opts |= OPT_DEBUG;
			ippool_yydebug++;
			break;
		case 'n' :
			opts |= OPT_DONOTHING;
			break;
		case 'R' :
			opts |= OPT_NORESOLVE;
			break;
		case 'u' :
			opts |= OPT_REMOVE;
			break;
		case 'v' :
			opts |= OPT_VERBOSE;
			break;
		}

	if (opts & OPT_DEBUG)
		fprintf(stderr, "loadpoolfile: opts = %#x\n", opts);

	if (!(opts & OPT_DONOTHING) && (fd == -1)) {
		fd = open(IPLOOKUP_NAME, O_RDWR);
		if (fd == -1) {
			perror("open(IPLOOKUP_NAME)");
			exit(1);
		}
	}

	if (ippool_parsefile(fd, infile, ioctl) != 0)
		return -1;
	return 0;
}


int poollist(argc, argv)
int argc;
char *argv[];
{
	char *kernel, *core, *poolname;
	int c, role, type, live_kernel;
	ip_pool_stat_t *plstp, plstat;
	iphtstat_t *htstp, htstat;
	iphtable_t *hptr;
	iplookupop_t op;
	ip_pool_t *ptr;

	core = NULL;
	kernel = NULL;
	live_kernel = 1;
	type = IPLT_ALL;
	poolname = NULL;
	role = IPL_LOGALL;

	while ((c = getopt(argc, argv, "dm:M:N:o:Rt:v")) != -1)
		switch (c)
		{
		case 'd' :
			opts |= OPT_DEBUG;
			break;
		case 'm' :
			poolname = optarg;
			break;
		case 'M' :
			live_kernel = 0;
			core = optarg;
			break;
		case 'N' :
			live_kernel = 0;
			kernel = optarg;
			break;
		case 'o' :
			role = getrole(optarg);
			if (role == IPL_LOGNONE) {
				fprintf(stderr, "unknown role '%s'\n", optarg);
				return -1;
			}
			break;
		case 'R' :
			opts |= OPT_NORESOLVE;
			break;
		case 't' :
			type = gettype(optarg, NULL);
			if (type == IPLT_NONE) {
				fprintf(stderr, "unknown type '%s'\n", optarg);
				return -1;
			}
			break;
		case 'v' :
			opts |= OPT_VERBOSE;
			break;
		}

	if (opts & OPT_DEBUG)
		fprintf(stderr, "poollist: opts = %#x\n", opts);

	if (!(opts & OPT_DONOTHING) && (fd == -1)) {
		fd = open(IPLOOKUP_NAME, O_RDWR);
		if (fd == -1) {
			perror("open(IPLOOKUP_NAME)");
			exit(1);
		}
	}

	bzero((char *)&op, sizeof(op));
	if (poolname != NULL) {
		strncpy(op.iplo_name, poolname, sizeof(op.iplo_name));
		op.iplo_name[sizeof(op.iplo_name) - 1] = '\0';
	}
	op.iplo_unit = role;

	if (openkmem(kernel, core) == -1)
		exit(-1);

	if (type == IPLT_ALL || type == IPLT_POOL) {
		plstp = &plstat;
		op.iplo_type = IPLT_POOL;
		op.iplo_size = sizeof(plstat);
		op.iplo_struct = &plstat;
		c = ioctl(fd, SIOCLOOKUPSTAT, &op);
		if (c == -1) {
			perror("ioctl(SIOCLOOKUPSTAT)");
			return -1;
		}

		if (role != IPL_LOGALL) {
			ptr = plstp->ipls_list[role];
			while (ptr != NULL) {
				ptr = printpool(ptr, kmemcpywrap, poolname,
						opts);
			}
		} else {
			for (role = 0; role <= IPL_LOGMAX; role++) {
				ptr = plstp->ipls_list[role];
				while (ptr != NULL) {
					ptr = printpool(ptr, kmemcpywrap,
							poolname, opts);
				}
			}
			role = IPL_LOGALL;
		}
	}
	if (type == IPLT_ALL || type == IPLT_HASH) {
		htstp = &htstat;
		op.iplo_type = IPLT_HASH;
		op.iplo_size = sizeof(htstat);
		op.iplo_struct = &htstat;
		c = ioctl(fd, SIOCLOOKUPSTAT, &op);
		if (c == -1) {
			perror("ioctl(SIOCLOOKUPSTAT)");
			return -1;
		}

		if (role != IPL_LOGALL) {
			hptr = htstp->iphs_tables;
			while (hptr != NULL) {
				hptr = printhash(hptr, kmemcpywrap,
						 poolname, opts);
			}
		} else {
			for (role = 0; role <= IPL_LOGMAX; role++) {
				hptr = htstp->iphs_tables;
				while (hptr != NULL) {
					hptr = printhash(hptr, kmemcpywrap,
							 poolname, opts);
				}

				op.iplo_unit = role;
				c = ioctl(fd, SIOCLOOKUPSTAT, &op);
				if (c == -1) {
					perror("ioctl(SIOCLOOKUPSTAT)");
					return -1;
				}
			}
		}
	}
	return 0;
}


int poolstats(argc, argv)
int argc;
char *argv[];
{
	int c, type, role, live_kernel;
	ip_pool_stat_t plstat;
	char *kernel, *core;
	iphtstat_t htstat;
	iplookupop_t op;

	core = NULL;
	kernel = NULL;
	live_kernel = 1;
	type = IPLT_ALL;
	role = IPL_LOGALL;

	bzero((char *)&op, sizeof(op));

	while ((c = getopt(argc, argv, "dM:N:o:t:v")) != -1)
		switch (c)
		{
		case 'd' :
			opts |= OPT_DEBUG;
			break;
		case 'M' :
			live_kernel = 0;
			core = optarg;
			break;
		case 'N' :
			live_kernel = 0;
			kernel = optarg;
			break;
		case 'o' :
			role = getrole(optarg);
			if (role == IPL_LOGNONE) {
				fprintf(stderr, "unknown role '%s'\n", optarg);
				return -1;
			}
			break;
		case 't' :
			type = gettype(optarg, NULL);
			if (type != IPLT_POOL) {
				fprintf(stderr,
					"-s not supported for this type yet\n");
				return -1;
			}
			break;
		case 'v' :
			opts |= OPT_VERBOSE;
			break;
		}

	if (opts & OPT_DEBUG)
		fprintf(stderr, "poolstats: opts = %#x\n", opts);

	if (!(opts & OPT_DONOTHING) && (fd == -1)) {
		fd = open(IPLOOKUP_NAME, O_RDWR);
		if (fd == -1) {
			perror("open(IPLOOKUP_NAME)");
			exit(1);
		}
	}

	if (type == IPLT_ALL || type == IPLT_POOL) {
		op.iplo_type = IPLT_POOL;
		op.iplo_struct = &plstat;
		op.iplo_size = sizeof(plstat);
		if (!(opts & OPT_DONOTHING)) {
			c = ioctl(fd, SIOCLOOKUPSTAT, &op);
			if (c == -1) {
				perror("ioctl(SIOCLOOKUPSTAT)");
				return -1;
			}
			printf("Pools:\t%lu\n", plstat.ipls_pools);
			printf("Nodes:\t%lu\n", plstat.ipls_nodes);
		}
	}

	if (type == IPLT_ALL || type == IPLT_HASH) {
		op.iplo_type = IPLT_HASH;
		op.iplo_struct = &htstat;
		op.iplo_size = sizeof(htstat);
		if (!(opts & OPT_DONOTHING)) {
			c = ioctl(fd, SIOCLOOKUPSTAT, &op);
			if (c == -1) {
				perror("ioctl(SIOCLOOKUPSTAT)");
				return -1;
			}
			printf("Hash Tables:\t%lu\n", htstat.iphs_numtables);
			printf("Nodes:\t%lu\n", htstat.iphs_numnodes);
			printf("Out of Memory:\t%lu\n", htstat.iphs_nomem);
		}
	}
	return 0;
}


int poolflush(argc, argv)
int argc;
char *argv[];
{
	int c, role, type, arg;
	iplookupflush_t flush;

	arg = IPLT_ALL;
	type = IPLT_ALL;
	role = IPL_LOGALL;

	while ((c = getopt(argc, argv, "do:t:v")) != -1)
		switch (c)
		{
		case 'd' :
			opts |= OPT_DEBUG;
			break;
		case 'o' :
			role = getrole(optarg);
			if (role == IPL_LOGNONE) {
				fprintf(stderr, "unknown role '%s'\n", optarg);
				return -1;
			}
			break;
		case 't' :
			type = gettype(optarg, NULL);
			if (type == IPLT_NONE) {
				fprintf(stderr, "unknown type '%s'\n", optarg);
				return -1;
			}
			break;
		case 'v' :
			opts |= OPT_VERBOSE;
			break;
		}

	if (opts & OPT_DEBUG)
		fprintf(stderr, "poolflush: opts = %#x\n", opts);

	if (!(opts & OPT_DONOTHING) && (fd == -1)) {
		fd = open(IPLOOKUP_NAME, O_RDWR);
		if (fd == -1) {
			perror("open(IPLOOKUP_NAME)");
			exit(1);
		}
	}

	bzero((char *)&flush, sizeof(flush));
	flush.iplf_type = type;
	flush.iplf_unit = role;
	flush.iplf_arg = arg;

	if (!(opts & OPT_DONOTHING)) {
		if (ioctl(fd, SIOCLOOKUPFLUSH, &flush) == -1) {
			perror("ioctl(SIOCLOOKUPFLUSH)");
			exit(1);
		}

	}
	printf("%u object%s flushed\n", flush.iplf_count,
	       (flush.iplf_count == 1) ? "" : "s");

	return 0;
}


int getrole(rolename)
char *rolename;
{
	int role;

	if (!strcasecmp(rolename, "ipf")) {
		role = IPL_LOGIPF;
#if 0
	} else if (!strcasecmp(rolename, "nat")) {
		role = IPL_LOGNAT;
	} else if (!strcasecmp(rolename, "state")) {
		role = IPL_LOGSTATE;
	} else if (!strcasecmp(rolename, "auth")) {
		role = IPL_LOGAUTH;
	} else if (!strcasecmp(rolename, "sync")) {
		role = IPL_LOGSYNC;
	} else if (!strcasecmp(rolename, "scan")) {
		role = IPL_LOGSCAN;
	} else if (!strcasecmp(rolename, "pool")) {
		role = IPL_LOGLOOKUP;
	} else if (!strcasecmp(rolename, "count")) {
		role = IPL_LOGCOUNT;
#endif
	} else {
		role = IPL_LOGNONE;
	}

	return role;
}


int gettype(typename, minor)
char *typename;
u_int *minor;
{
	int type;

	if (!strcasecmp(optarg, "tree")) {
		type = IPLT_POOL;
	} else if (!strcasecmp(optarg, "hash")) {
		type = IPLT_HASH;
		if (minor != NULL)
			*minor = IPHASH_LOOKUP;
	} else if (!strcasecmp(optarg, "group-map")) {
		type = IPLT_HASH;
		if (minor != NULL)
			*minor = IPHASH_GROUPMAP;
	} else {
		type = IPLT_NONE;
	}
	return type;
}
