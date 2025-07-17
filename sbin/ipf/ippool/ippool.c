
/*
 * Copyright (C) 2012 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 */
#include <sys/types.h>
#include <sys/time.h>
#include <sys/param.h>
#include <sys/socket.h>
# include <sys/cdefs.h>
#include <sys/ioctl.h>

#include <net/if.h>
#include <netinet/in.h>

#include <arpa/inet.h>

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <ctype.h>
#include <unistd.h>
# include <nlist.h>

#include "ipf.h"
#include "netinet/ipl.h"
#include "netinet/ip_lookup.h"
#include "netinet/ip_pool.h"
#include "netinet/ip_htable.h"
#include "kmem.h"


extern	int	ippool_yyparse(void);
extern	int	ippool_yydebug;
extern	FILE	*ippool_yyin;
extern	char	*optarg;
extern	int	lineNum;

void	usage(char *);
int	main(int, char **);
int	poolcommand(int, int, char *[]);
int	poolnodecommand(int, int, char *[]);
int	loadpoolfile(int, char *[], char *);
int	poollist(int, char *[]);
void	poollist_dead(int, char *, int, char *, char *);
int	poollist_live(int, char *, int, int);
int	poolflush(int, char *[]);
int	poolstats(int, char *[]);
int	gettype(char *, u_int *);
int	getrole(char *);
int	setnodeaddr(int, int, void *ptr, char *arg);
int	showpools_live(int, int, ipf_pool_stat_t *, char *);
int	showhashs_live(int, int, iphtstat_t *, char *);
int	showdstls_live(int, int, ipf_dstl_stat_t *, char *);

int	opts = 0;
int	fd = -1;
int	use_inet6 = 0;
wordtab_t *pool_fields = NULL;
int	nohdrfields = 0;


void
usage(char *prog)
{
	fprintf(stderr, "Usage:\t%s\n", prog);
	fprintf(stderr, "\t-a [-dnv] -m <name> [-o <role>] [-t type] [-T ttl] -i <ipaddr>[/netmask]\n");
	fprintf(stderr, "\t-A [-dnv] [-m <name>] [-o <role>] [-S <seed>] [-t <type>]\n");
	fprintf(stderr, "\t-f <file> [-dnuvR]\n");
	fprintf(stderr, "\t-F [-dv] [-o <role>] [-t <type>]\n");
	fprintf(stderr, "\t-l [-dv] [-m <name>] [-t <type>] [-o <role>] [-M <core>] [-N <namelist>]\n");
	fprintf(stderr, "\t-r [-dnv] [-m <name>] [-o <role>] [-t type] -i <ipaddr>[/netmask]\n");
	fprintf(stderr, "\t-R [-dnv] [-m <name>] [-o <role>] [-t <type>]\n");
	fprintf(stderr, "\t-s [-dtv]\n");
	exit(1);
}


int
main(int argc, char *argv[])
{
	int err = 1;

	if (argc < 2)
		usage(argv[0]);

	assigndefined(getenv("IPPOOL_PREDEFINED"));

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

	if (err != 0)
		exit(1);
	return (0);
}


int
poolnodecommand(int remove, int argc, char *argv[])
{
	int err = 0, c, ipset, role, type = IPLT_POOL, ttl = 0;
	char *poolname = NULL;
	ip_pool_node_t pnode;
	iphtent_t hnode;
	void *ptr = &pnode;

	ipset = 0;
	role = IPL_LOGIPF;
	bzero((char *)&pnode, sizeof(pnode));
	bzero((char *)&hnode, sizeof(hnode));

	while ((c = getopt(argc, argv, "di:m:no:t:T:v")) != -1)
		switch (c)
		{
		case 'd' :
			opts |= OPT_DEBUG;
			ippool_yydebug++;
			break;
		case 'i' :
			if (setnodeaddr(type, role, ptr, optarg) == 0)
				ipset = 1;
			break;
		case 'm' :
			poolname = optarg;
			break;
		case 'n' :
			opts |= OPT_DONOTHING|OPT_DONTOPEN;
			break;
		case 'o' :
			if (ipset == 1) {
				fprintf(stderr,
					"cannot set role after ip address\n");
				return (-1);
			}
			role = getrole(optarg);
			if (role == IPL_LOGNONE)
				return (-1);
			break;
		case 't' :
			if (ipset == 1) {
				fprintf(stderr,
					"cannot set type after ip address\n");
				return (-1);
			}
			type = gettype(optarg, NULL);
			switch (type) {
			case IPLT_NONE :
				fprintf(stderr, "unknown type '%s'\n", optarg);
				return (-1);
			case IPLT_HASH :
				ptr = &hnode;
				break;
			case IPLT_POOL :
			default :
				break;
			}
			break;
		case 'T' :
			if (remove == 0) {
				ttl = atoi(optarg);
				if (ttl < 0) {
					fprintf(stderr, "cannot set negative ttl\n");
					return (-1);
				}
			} else {
				usage(argv[0]);
			}
			break;
		case 'v' :
			opts |= OPT_VERBOSE;
			break;
		default :
			usage(argv[0]);
			break;		/* keep compiler happy */
		}

	if (argc - 1 - optind > 0)
		usage(argv[0]);

	if (argv[optind] != NULL && ipset == 0) {
		if (setnodeaddr(type, role, ptr, argv[optind]) == 0)
			ipset = 1;
	}

	if (opts & OPT_DEBUG)
		fprintf(stderr, "poolnodecommand: opts = %#x\n", opts);

	if (ipset == 0) {
		fprintf(stderr, "no IP address given with -i\n");
		return (-1);
	}

	if (poolname == NULL) {
		fprintf(stderr, "poolname not given with add/remove node\n");
		return (-1);
	}

	switch (type) {
	case IPLT_POOL :
		if (remove == 0)
			err = load_poolnode(role, poolname, &pnode, ttl, ioctl);
		else
			err = remove_poolnode(role, poolname, &pnode, ioctl);
		break;
	case IPLT_HASH :
		if (remove == 0)
			err = load_hashnode(role, poolname, &hnode, ttl, ioctl);
		else
			err = remove_hashnode(role, poolname, &hnode, ioctl);
		break;
	default :
		break;
	}
	return (err);
}


int
poolcommand(int remove, int argc, char *argv[])
{
	int type, role, c, err;
	char *poolname, *typearg = NULL;
	iphtable_t iph;
	ip_pool_t pool;

	err = 1;
	role = 0;
	type = 0;
	poolname = NULL;
	role = IPL_LOGIPF;
	bzero((char *)&iph, sizeof(iph));
	bzero((char *)&pool, sizeof(pool));

	while ((c = getopt(argc, argv, "dm:no:S:vt:")) != -1)
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
			opts |= OPT_DONOTHING|OPT_DONTOPEN;
			break;
		case 'o' :
			role = getrole(optarg);
			if (role == IPL_LOGNONE) {
				fprintf(stderr, "unknown role '%s'\n", optarg);
				return (-1);
			}
			break;
		case 'S' :
			if (remove == 0)
				iph.iph_seed = atoi(optarg);
			else
				usage(argv[0]);
			break;
		case 't' :
			type = gettype(optarg, &iph.iph_type);
			typearg = optarg;
			break;
		case 'v' :
			opts |= OPT_VERBOSE;
			break;
		default :
			usage(argv[0]);
			break;		/* keep compiler happy */
		}

	if (argc - 1 - optind > 0)
		usage(argv[0]);

	if (opts & OPT_DEBUG)
		fprintf(stderr, "poolcommand: opts = %#x\n", opts);

	if (poolname == NULL) {
		fprintf(stderr, "poolname not given with add/remove pool\n");
		return (-1);
	}

	if (type == IPLT_NONE && remove == 0) {
		if (typearg == NULL) {
			fprintf(stderr, "type must be specified\n");
			usage(argv[0]);
		} else {
			fprintf(stderr, "unknown type '%s'\n", typearg);
		}
		return (-1);
	}

	if (type == IPLT_HASH || (type == IPLT_NONE && remove == 1)) {
		strncpy(iph.iph_name, poolname, sizeof(iph.iph_name));
		iph.iph_name[sizeof(iph.iph_name) - 1] = '\0';
		iph.iph_unit = role;
	}
	if (type == IPLT_POOL || (type == IPLT_NONE && remove == 1)) {
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
		case IPLT_NONE :
			err = 1;
			{
				int err_h, err_p;
				err_h = remove_hash(&iph, ioctl);
				err_p = remove_pool(&pool, ioctl);
				if (err_h == 0 || err_p == 0)
					err = 0;
			}
			break;
		}
	}
	return (err);
}


int
loadpoolfile(int argc, char *argv[], char *infile)
{
	int c;

	while ((c = getopt(argc, argv, "dnuvf:")) != -1)
		switch (c)
		{
		case 'd' :
			opts |= OPT_DEBUG;
			ippool_yydebug++;
			break;
		case 'f' :
			if (loadpoolfile(argc, argv, optarg) != 0)
				return (-1);
			break;
		case 'n' :
			opts |= OPT_DONOTHING|OPT_DONTOPEN;
			break;
		case 'u' :
			opts |= OPT_REMOVE;
			break;
		case 'v' :
			opts |= OPT_VERBOSE;
			break;
		default :
			usage(argv[0]);
			break;		/* keep compiler happy */
		}

	if (argc - 1 - optind > 0)
		usage(argv[0]);

	if (opts & OPT_DEBUG)
		fprintf(stderr, "loadpoolfile: opts = %#x\n", opts);

	if (!(opts & (OPT_DONOTHING|OPT_DONTOPEN)) && (fd == -1)) {
		fd = open(IPLOOKUP_NAME, O_RDWR);
		if (fd == -1) {
			perror("open(IPLOOKUP_NAME)");
			exit(1);
		}
	}

	if (ippool_parsefile(fd, infile, ioctl) != 0)
		return (-1);
	return (0);
}


int
poolstats(int argc, char *argv[])
{
	int c, type, role;
	ipf_pool_stat_t plstat;
	ipf_dstl_stat_t dlstat;
	iphtstat_t htstat;
	iplookupop_t op;

	type = IPLT_ALL;
	role = IPL_LOGALL;

	bzero((char *)&op, sizeof(op));

	while ((c = getopt(argc, argv, "dM:N:o:t:v")) != -1)
		switch (c)
		{
		case 'd' :
			opts |= OPT_DEBUG;
			break;
		case 'o' :
			role = getrole(optarg);
			if (role == IPL_LOGNONE) {
				fprintf(stderr, "unknown role '%s'\n", optarg);
				return (-1);
			}
			break;
		case 't' :
			type = gettype(optarg, NULL);
			if (type != IPLT_POOL) {
				fprintf(stderr,
					"-s not supported for this type yet\n");
				return (-1);
			}
			break;
		case 'v' :
			opts |= OPT_VERBOSE;
			break;
		default :
			usage(argv[0]);
			break;		/* keep compiler happy */
		}

	if (argc - 1 - optind > 0)
		usage(argv[0]);

	if (opts & OPT_DEBUG)
		fprintf(stderr, "poolstats: opts = %#x\n", opts);

	if (!(opts & (OPT_DONOTHING|OPT_DONTOPEN)) && (fd == -1)) {
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
		if (!(opts & (OPT_DONOTHING|OPT_DONTOPEN))) {
			c = ioctl(fd, SIOCLOOKUPSTAT, &op);
			if (c == -1) {
				ipferror(fd, "ioctl(S0IOCLOOKUPSTAT)");
				return (-1);
			}
			printf("%lu\taddress pools\n", plstat.ipls_pools);
			printf("%lu\taddress pool nodes\n", plstat.ipls_nodes);
		}
	}

	if (type == IPLT_ALL || type == IPLT_HASH) {
		op.iplo_type = IPLT_HASH;
		op.iplo_struct = &htstat;
		op.iplo_size = sizeof(htstat);
		if (!(opts & (OPT_DONOTHING|OPT_DONTOPEN))) {
			c = ioctl(fd, SIOCLOOKUPSTAT, &op);
			if (c == -1) {
				ipferror(fd, "ioctl(SIOCLOOKUPSTAT)");
				return (-1);
			}
			printf("%lu\thash tables\n", htstat.iphs_numtables);
			printf("%lu\thash table nodes\n", htstat.iphs_numnodes);
			printf("%lu\thash table no memory \n",
				htstat.iphs_nomem);
		}
	}

	if (type == IPLT_ALL || type == IPLT_DSTLIST) {
		op.iplo_type = IPLT_DSTLIST;
		op.iplo_struct = &dlstat;
		op.iplo_size = sizeof(dlstat);
		if (!(opts & (OPT_DONOTHING|OPT_DONTOPEN))) {
			c = ioctl(fd, SIOCLOOKUPSTAT, &op);
			if (c == -1) {
				ipferror(fd, "ioctl(SIOCLOOKUPSTAT)");
				return (-1);
			}
			printf("%u\tdestination lists\n",
			       dlstat.ipls_numlists);
			printf("%u\tdestination list nodes\n",
			       dlstat.ipls_numnodes);
			printf("%lu\tdestination list no memory\n",
			       dlstat.ipls_nomem);
			printf("%u\tdestination list zombies\n",
			       dlstat.ipls_numdereflists);
			printf("%u\tdesetination list node zombies\n",
			       dlstat.ipls_numderefnodes);
		}
	}
	return (0);
}


int
poolflush(int argc, char *argv[])
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
				return (-1);
			}
			break;
		case 't' :
			type = gettype(optarg, NULL);
			if (type == IPLT_NONE) {
				fprintf(stderr, "unknown type '%s'\n", optarg);
				return (-1);
			}
			break;
		case 'v' :
			opts |= OPT_VERBOSE;
			break;
		default :
			usage(argv[0]);
			break;		/* keep compiler happy */
		}

	if (argc - optind > 0)
		usage(argv[0]);

	if (opts & OPT_DEBUG)
		fprintf(stderr, "poolflush: opts = %#x\n", opts);

	if (!(opts & (OPT_DONOTHING|OPT_DONTOPEN)) && (fd == -1)) {
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

	if (!(opts & (OPT_DONOTHING|OPT_DONTOPEN))) {
		if (ioctl(fd, SIOCLOOKUPFLUSH, &flush) == -1) {
			ipferror(fd, "ioctl(SIOCLOOKUPFLUSH)");
			exit(1);
		}

	}
	printf("%u object%s flushed\n", flush.iplf_count,
	       (flush.iplf_count == 1) ? "" : "s");

	return (0);
}


int
getrole(char *rolename)
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

	return (role);
}


int
gettype(char *typename, u_int *minor)
{
	int type;

	if (!strcasecmp(typename, "tree") || !strcasecmp(typename, "pool")) {
		type = IPLT_POOL;
	} else if (!strcasecmp(typename, "hash")) {
		type = IPLT_HASH;
		if (minor != NULL)
			*minor = IPHASH_LOOKUP;
	} else if (!strcasecmp(typename, "group-map")) {
		type = IPLT_HASH;
		if (minor != NULL)
			*minor = IPHASH_GROUPMAP;
	} else {
		type = IPLT_NONE;
	}
	return (type);
}


int
poollist(int argc, char *argv[])
{
	char *kernel, *core, *poolname;
	int c, role, type, live_kernel;
	iplookupop_t op;

	core = NULL;
	kernel = NULL;
	live_kernel = 1;
	type = IPLT_ALL;
	poolname = NULL;
	role = IPL_LOGALL;

	while ((c = getopt(argc, argv, "dDm:M:N:o:t:v")) != -1)
		switch (c)
		{
		case 'd' :
			opts |= OPT_DEBUG;
			break;
		case 'D' :
			opts |= OPT_SAVEOUT;
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
				return (-1);
			}
			break;
#if 0
		case 'O' :
			/* XXX This option does not work. This function as  */
			/* XXX used by state and nat can be used to format  */
			/* XXX output especially useful for scripting. It   */
			/* XXX is left here with the intention of making    */
			/* XXX it work for the same purpose at some point.  */
			pool_fields = parsefields(poolfields, optarg);
			break;
#endif
		case 't' :
			type = gettype(optarg, NULL);
			if (type == IPLT_NONE) {
				fprintf(stderr, "unknown type '%s'\n", optarg);
				return (-1);
			}
			break;
		case 'v' :
			opts |= OPT_VERBOSE;
			break;
		default :
			usage(argv[0]);
			break;		/* keep compiler happy */
		}

	if (argc - optind > 0)
		usage(argv[0]);

	if (opts & OPT_DEBUG)
		fprintf(stderr, "poollist: opts = %#x\n", opts);

	if (!(opts & (OPT_DONOTHING|OPT_DONTOPEN)) && (fd == -1)) {
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

	if (live_kernel) {
		if (poollist_live(role, poolname, type, fd) != 0)
			return (1);
	} else
		poollist_dead(role, poolname, type, kernel, core);
	return (0);
}


void
poollist_dead(int role, char *poolname, int type, char *kernel, char *core)
{
	iphtable_t *hptr;
	ip_pool_t *ptr;

	if (openkmem(kernel, core) == -1)
		exit(-1);

	if (type == IPLT_ALL || type == IPLT_POOL) {
		ip_pool_t *pools[IPL_LOGSIZE];
		struct nlist names[2] = { { "ip_pool_list" } , { "" } };

		if (nlist(kernel, names) != 1)
			return;

		bzero(&pools, sizeof(pools));
		if (kmemcpy((char *)&pools, names[0].n_value, sizeof(pools)))
			return;

		if (role != IPL_LOGALL) {
			ptr = pools[role];
			while (ptr != NULL) {
				ptr = printpool(ptr, kmemcpywrap, poolname,
						opts, pool_fields);
			}
		} else {
			for (role = 0; role <= IPL_LOGMAX; role++) {
				ptr = pools[role];
				while (ptr != NULL) {
					ptr = printpool(ptr, kmemcpywrap,
							poolname, opts,
							pool_fields);
				}
			}
			role = IPL_LOGALL;
		}
	}
	if (type == IPLT_ALL || type == IPLT_HASH) {
		iphtable_t *tables[IPL_LOGSIZE];
		struct nlist names[2] = { { "ipf_htables" } , { "" } };

		if (nlist(kernel, names) != 1)
			return;

		bzero(&tables, sizeof(tables));
		if (kmemcpy((char *)&tables, names[0].n_value, sizeof(tables)))
			return;

		if (role != IPL_LOGALL) {
			hptr = tables[role];
			while (hptr != NULL) {
				hptr = printhash(hptr, kmemcpywrap,
						 poolname, opts, pool_fields);
			}
		} else {
			for (role = 0; role <= IPL_LOGMAX; role++) {
				hptr = tables[role];
				while (hptr != NULL) {
					hptr = printhash(hptr, kmemcpywrap,
							 poolname, opts,
							 pool_fields);
				}
			}
		}
	}
}


int
poollist_live(int role, char *poolname, int type, int fd)
{
	ipf_pool_stat_t plstat;
	iplookupop_t op;
	int c;

	if (type == IPLT_ALL || type == IPLT_POOL) {
		op.iplo_type = IPLT_POOL;
		op.iplo_size = sizeof(plstat);
		op.iplo_struct = &plstat;
		op.iplo_name[0] = '\0';
		op.iplo_arg = 0;

		if (role != IPL_LOGALL) {
			op.iplo_unit = role;

			c = ioctl(fd, SIOCLOOKUPSTAT, &op);
			if (c == -1) {
				ipferror(fd, "ioctl(SIOCLOOKUPSTAT)");
				return (1);
			}

			if (showpools_live(fd, role, &plstat, poolname))
				return (1);
		} else {
			for (role = -1; role <= IPL_LOGMAX; role++) {
				op.iplo_unit = role;

				c = ioctl(fd, SIOCLOOKUPSTAT, &op);
				if (c == -1) {
					ipferror(fd, "ioctl(SIOCLOOKUPSTAT)");
					return (1);
				}

				if (showpools_live(fd, role, &plstat, poolname))
					return (1);
			}

			role = IPL_LOGALL;
		}
	}

	if (type == IPLT_ALL || type == IPLT_HASH) {
		iphtstat_t htstat;

		op.iplo_type = IPLT_HASH;
		op.iplo_size = sizeof(htstat);
		op.iplo_struct = &htstat;
		op.iplo_name[0] = '\0';
		op.iplo_arg = 0;

		if (role != IPL_LOGALL) {
			op.iplo_unit = role;

			c = ioctl(fd, SIOCLOOKUPSTAT, &op);
			if (c == -1) {
				ipferror(fd, "ioctl(SIOCLOOKUPSTAT)");
				return (1);
			}
			if (showhashs_live(fd, role, &htstat, poolname))
				return (1);
		} else {
			for (role = 0; role <= IPL_LOGMAX; role++) {

				op.iplo_unit = role;
				c = ioctl(fd, SIOCLOOKUPSTAT, &op);
				if (c == -1) {
					ipferror(fd, "ioctl(SIOCLOOKUPSTAT)");
					return (1);
				}

				if (showhashs_live(fd, role, &htstat, poolname))
					return(1);
			}
			role = IPL_LOGALL;
		}
	}

	if (type == IPLT_ALL || type == IPLT_DSTLIST) {
		ipf_dstl_stat_t dlstat;

		op.iplo_type = IPLT_DSTLIST;
		op.iplo_size = sizeof(dlstat);
		op.iplo_struct = &dlstat;
		op.iplo_name[0] = '\0';
		op.iplo_arg = 0;

		if (role != IPL_LOGALL) {
			op.iplo_unit = role;

			c = ioctl(fd, SIOCLOOKUPSTAT, &op);
			if (c == -1) {
				ipferror(fd, "ioctl(SIOCLOOKUPSTAT)");
				return (1);
			}
			if (showdstls_live(fd, role, &dlstat, poolname))
				return (1);
		} else {
			for (role = 0; role <= IPL_LOGMAX; role++) {

				op.iplo_unit = role;
				c = ioctl(fd, SIOCLOOKUPSTAT, &op);
				if (c == -1) {
					ipferror(fd, "ioctl(SIOCLOOKUPSTAT)");
					return (1);
				}

				if (showdstls_live(fd, role, &dlstat, poolname))
					return (1);
			}
			role = IPL_LOGALL;
		}
	}
	return (0);
}


int
showpools_live(int fd, int role, ipf_pool_stat_t *plstp, char *poolname)
{
	ipflookupiter_t iter;
	ip_pool_t pool;
	ipfobj_t obj;

	obj.ipfo_rev = IPFILTER_VERSION;
	obj.ipfo_type = IPFOBJ_LOOKUPITER;
	obj.ipfo_size = sizeof(iter);
	obj.ipfo_ptr = &iter;

	iter.ili_type = IPLT_POOL;
	iter.ili_otype = IPFLOOKUPITER_LIST;
	iter.ili_ival = IPFGENITER_LOOKUP;
	iter.ili_nitems = 1;
	iter.ili_data = &pool;
	iter.ili_unit = role;
	*iter.ili_name = '\0';

	bzero((char *)&pool, sizeof(pool));

	while (plstp->ipls_list[role + 1] != NULL) {
		if (ioctl(fd, SIOCLOOKUPITER, &obj)) {
			ipferror(fd, "ioctl(SIOCLOOKUPITER)");
			return (1);
		}
		if (((pool.ipo_flags & IPOOL_DELETE) == 0) ||
		    ((opts & OPT_DEBUG) != 0))
			printpool_live(&pool, fd, poolname, opts, pool_fields);

		plstp->ipls_list[role + 1] = pool.ipo_next;
	}
	return (0);
}


int
showhashs_live(int fd, int role, iphtstat_t *htstp, char *poolname)
{
	ipflookupiter_t iter;
	iphtable_t table;
	ipfobj_t obj;

	obj.ipfo_rev = IPFILTER_VERSION;
	obj.ipfo_type = IPFOBJ_LOOKUPITER;
	obj.ipfo_size = sizeof(iter);
	obj.ipfo_ptr = &iter;

	iter.ili_type = IPLT_HASH;
	iter.ili_otype = IPFLOOKUPITER_LIST;
	iter.ili_ival = IPFGENITER_LOOKUP;
	iter.ili_nitems = 1;
	iter.ili_data = &table;
	iter.ili_unit = role;
	*iter.ili_name = '\0';

	while (htstp->iphs_tables != NULL) {
		if (ioctl(fd, SIOCLOOKUPITER, &obj)) {
			ipferror(fd, "ioctl(SIOCLOOKUPITER)");
			return (1);
		}

		printhash_live(&table, fd, poolname, opts, pool_fields);

		htstp->iphs_tables = table.iph_next;
	}
	return (0);
}


int
showdstls_live(int fd, int role, ipf_dstl_stat_t *dlstp, char *poolname)
{
	ipflookupiter_t iter;
	ippool_dst_t table;
	ipfobj_t obj;

	obj.ipfo_rev = IPFILTER_VERSION;
	obj.ipfo_type = IPFOBJ_LOOKUPITER;
	obj.ipfo_size = sizeof(iter);
	obj.ipfo_ptr = &iter;

	iter.ili_type = IPLT_DSTLIST;
	iter.ili_otype = IPFLOOKUPITER_LIST;
	iter.ili_ival = IPFGENITER_LOOKUP;
	iter.ili_nitems = 1;
	iter.ili_data = &table;
	iter.ili_unit = role;
	*iter.ili_name = '\0';

	while (dlstp->ipls_list[role] != NULL) {
		if (ioctl(fd, SIOCLOOKUPITER, &obj)) {
			ipferror(fd, "ioctl(SIOCLOOKUPITER)");
			return (1);
		}

		printdstl_live(&table, fd, poolname, opts, pool_fields);

		dlstp->ipls_list[role] = table.ipld_next;
	}
	return (0);
}


int
setnodeaddr(int type, int role, void *ptr, char *arg)
{
	struct in_addr mask;
	sa_family_t family;
	char *s;

	if (strchr(arg, ':') == NULL) {
		family = AF_INET;
		s = strchr(arg, '/');
		if (s == NULL)
			mask.s_addr = 0xffffffff;
		else if (strchr(s, '.') == NULL) {
			if (ntomask(AF_INET, atoi(s + 1), &mask.s_addr) != 0)
				return (-1);
		} else {
			mask.s_addr = inet_addr(s + 1);
		}
		if (s != NULL)
			*s = '\0';
	} else {
		family = AF_INET6;

		/* XXX for now we use mask for IPv6 prefix length */
		/* XXX mask should be a union with prefix */
		/* XXX Currently address handling is sloppy. */

		if ((s = strchr(arg, '/')) == NULL)
			mask.s_addr = 128;
		else
			mask.s_addr = atoi(s + 1);
	}

	if (type == IPLT_POOL) {
		ip_pool_node_t *node = ptr;

		node->ipn_addr.adf_family = family;

#ifdef USE_INET6
		if (node->ipn_addr.adf_family == AF_INET) {
#endif
			node->ipn_addr.adf_len = offsetof(addrfamily_t,
							  adf_addr) +
						 sizeof(struct in_addr);
			node->ipn_addr.adf_addr.in4.s_addr = inet_addr(arg);
#ifdef USE_INET6
		} else {
			node->ipn_addr.adf_len = offsetof(addrfamily_t,
							  adf_addr) +
						 sizeof(struct in6_addr);
			inet_pton(AF_INET6, arg,
				&node->ipn_addr.adf_addr.in6.s6_addr);
		}
#endif
		node->ipn_mask.adf_len = node->ipn_addr.adf_len;
		node->ipn_mask.adf_addr.in4.s_addr = mask.s_addr;
	} else if (type == IPLT_HASH) {
		iphtent_t *node = ptr;

		node->ipe_family = family;
		node->ipe_unit = role;

#ifdef USE_INET6
		if (node->ipe_family == AF_INET) {
#endif
			node->ipe_addr.in4.s_addr = inet_addr(arg);
			node->ipe_mask.in4.s_addr = mask.s_addr;
#ifdef USE_INET6
		} else {
			inet_pton(AF_INET6, arg,
				&node->ipe_addr.in6.__u6_addr.__u6_addr32);
			node->ipe_mask.in6.__u6_addr.__u6_addr32[0] =
				mask.s_addr;
			node->ipe_mask.in6.__u6_addr.__u6_addr32[1] =
			node->ipe_mask.in6.__u6_addr.__u6_addr32[2] =
			node->ipe_mask.in6.__u6_addr.__u6_addr32[3] = 0;
		}
#endif
	}

	return (0);
}
