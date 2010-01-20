/*	$FreeBSD$	*/

/*
 * Copyright (C) 2001-2006 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * Added redirect stuff and a variety of bug fixes. (mcn@EnGarde.com)
 */
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#if !defined(__SVR4) && !defined(__svr4__)
#include <strings.h>
#else
#include <sys/byteorder.h>
#endif
#include <sys/time.h>
#include <sys/param.h>
#include <stdlib.h>
#include <unistd.h>
#include <stddef.h>
#include <sys/file.h>
#define _KERNEL
#include <sys/uio.h>
#undef _KERNEL
#include <sys/socket.h>
#include <sys/ioctl.h>
#if defined(sun) && (defined(__svr4__) || defined(__SVR4))
# include <sys/ioccom.h>
# include <sys/sysmacros.h>
#endif
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <net/if.h>
#if __FreeBSD_version >= 300000
# include <net/if_var.h>
#endif
#include <netdb.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>
#include <resolv.h>
#include <ctype.h>
#if defined(linux)
# include <linux/a.out.h>
#else
# include <nlist.h>
#endif
#include "ipf.h"
#include "netinet/ipl.h"
#include "kmem.h"

#ifdef	__hpux
# define	nlist	nlist64
#endif

#if	defined(sun) && !SOLARIS2
# define	STRERROR(x)	sys_errlist[x]
extern	char	*sys_errlist[];
#else
# define	STRERROR(x)	strerror(x)
#endif

#if !defined(lint)
static const char sccsid[] ="@(#)ipnat.c	1.9 6/5/96 (C) 1993 Darren Reed";
static const char rcsid[] = "@(#)$Id: ipnat.c,v 1.24.2.11 2007/09/25 08:27:34 darrenr Exp $";
#endif


#if	SOLARIS
#define	bzero(a,b)	memset(a,0,b)
#endif
int	use_inet6 = 0;
char	thishost[MAXHOSTNAMELEN];

extern	char	*optarg;

void	dostats __P((int, natstat_t *, int, int));
void	dotable __P((natstat_t *, int, int));
void	flushtable __P((int, int));
void	usage __P((char *));
int	main __P((int, char*[]));
void	showhostmap __P((natstat_t *nsp));
void	natstat_dead __P((natstat_t *, char *));
void	dostats_live __P((int, natstat_t *, int));
void	showhostmap_dead __P((natstat_t *));
void	showhostmap_live __P((int, natstat_t *));
void	dostats_dead __P((natstat_t *, int));
void	showtqtable_live __P((int));

int	opts;

void usage(name)
char *name;
{
	fprintf(stderr, "Usage: %s [-CFhlnrRsv] [-f filename]\n", name);
	exit(1);
}


int main(argc, argv)
int argc;
char *argv[];
{
	char *file, *core, *kernel;
	natstat_t ns, *nsp;
	int fd, c, mode;
	ipfobj_t obj;

	fd = -1;
	opts = 0;
	nsp = &ns;
	file = NULL;
	core = NULL;
	kernel = NULL;
	mode = O_RDWR;

	while ((c = getopt(argc, argv, "CdFf:hlM:N:nrRsv")) != -1)
		switch (c)
		{
		case 'C' :
			opts |= OPT_CLEAR;
			break;
		case 'd' :
			opts |= OPT_DEBUG;
			break;
		case 'f' :
			file = optarg;
			break;
		case 'F' :
			opts |= OPT_FLUSH;
			break;
		case 'h' :
			opts |=OPT_HITS;
			break;
		case 'l' :
			opts |= OPT_LIST;
			mode = O_RDONLY;
			break;
		case 'M' :
			core = optarg;
			break;
		case 'N' :
			kernel = optarg;
			break;
		case 'n' :
			opts |= OPT_DONOTHING;
			mode = O_RDONLY;
			break;
		case 'R' :
			opts |= OPT_NORESOLVE;
			break;
		case 'r' :
			opts |= OPT_REMOVE;
			break;
		case 's' :
			opts |= OPT_STAT;
			mode = O_RDONLY;
			break;
		case 'v' :
			opts |= OPT_VERBOSE;
			break;
		default :
			usage(argv[0]);
		}

	initparse();

	if ((kernel != NULL) || (core != NULL)) {
		(void) setgid(getgid());
		(void) setuid(getuid());
	}

	if (!(opts & OPT_DONOTHING)) {
		if (((fd = open(IPNAT_NAME, mode)) == -1) &&
		    ((fd = open(IPNAT_NAME, O_RDONLY)) == -1)) {
			(void) fprintf(stderr, "%s: open: %s\n", IPNAT_NAME,
				STRERROR(errno));
			exit(1);
		}
	}

	bzero((char *)&ns, sizeof(ns));

	if ((opts & OPT_DONOTHING) == 0) {
		if (checkrev(IPL_NAME) == -1) {
			fprintf(stderr, "User/kernel version check failed\n");
			exit(1);
		}
	}

	if (!(opts & OPT_DONOTHING) && (kernel == NULL) && (core == NULL)) {
		bzero((char *)&obj, sizeof(obj));
		obj.ipfo_rev = IPFILTER_VERSION;
		obj.ipfo_type = IPFOBJ_NATSTAT;
		obj.ipfo_size = sizeof(*nsp);
		obj.ipfo_ptr = (void *)nsp;
		if (ioctl(fd, SIOCGNATS, &obj) == -1) {
			perror("ioctl(SIOCGNATS)");
			exit(1);
		}
		(void) setgid(getgid());
		(void) setuid(getuid());
	} else if ((kernel != NULL) || (core != NULL)) {
		if (openkmem(kernel, core) == -1)
			exit(1);

		natstat_dead(nsp, kernel);
		if (opts & (OPT_LIST|OPT_STAT))
			dostats(fd, nsp, opts, 0);
		exit(0);
	}

	if (opts & (OPT_FLUSH|OPT_CLEAR))
		flushtable(fd, opts);
	if (file) {
		ipnat_parsefile(fd, ipnat_addrule, ioctl, file);
	}
	if (opts & (OPT_LIST|OPT_STAT))
		dostats(fd, nsp, opts, 1);
	return 0;
}


/*
 * Read NAT statistic information in using a symbol table and memory file
 * rather than doing ioctl's.
 */
void natstat_dead(nsp, kernel)
natstat_t *nsp;
char *kernel;
{
	struct nlist nat_nlist[10] = {
		{ "nat_table" },		/* 0 */
		{ "nat_list" },
		{ "maptable" },
		{ "ipf_nattable_sz" },
		{ "ipf_natrules_sz" },
		{ "ipf_rdrrules_sz" },		/* 5 */
		{ "ipf_hostmap_sz" },
		{ "nat_instances" },
		{ "ap_sess_list" },
		{ NULL }
	};
	void *tables[2];

	if (nlist(kernel, nat_nlist) == -1) {
		fprintf(stderr, "nlist error\n");
		return;
	}

	/*
	 * Normally the ioctl copies all of these values into the structure
	 * for us, before returning it to userland, so here we must copy each
	 * one in individually.
	 */
	kmemcpy((char *)&tables, nat_nlist[0].n_value, sizeof(tables));
	nsp->ns_table[0] = tables[0];
	nsp->ns_table[1] = tables[1];

	kmemcpy((char *)&nsp->ns_list, nat_nlist[1].n_value,
		sizeof(nsp->ns_list));
	kmemcpy((char *)&nsp->ns_maptable, nat_nlist[2].n_value,
		sizeof(nsp->ns_maptable));
	kmemcpy((char *)&nsp->ns_nattab_sz, nat_nlist[3].n_value,
		sizeof(nsp->ns_nattab_sz));
	kmemcpy((char *)&nsp->ns_rultab_sz, nat_nlist[4].n_value,
		sizeof(nsp->ns_rultab_sz));
	kmemcpy((char *)&nsp->ns_rdrtab_sz, nat_nlist[5].n_value,
		sizeof(nsp->ns_rdrtab_sz));
	kmemcpy((char *)&nsp->ns_hostmap_sz, nat_nlist[6].n_value,
		sizeof(nsp->ns_hostmap_sz));
	kmemcpy((char *)&nsp->ns_instances, nat_nlist[7].n_value,
		sizeof(nsp->ns_instances));
	kmemcpy((char *)&nsp->ns_apslist, nat_nlist[8].n_value,
		sizeof(nsp->ns_apslist));
}


/*
 * Issue an ioctl to flush either the NAT rules table or the active mapping
 * table or both.
 */
void flushtable(fd, opts)
int fd, opts;
{
	int n = 0;

	if (opts & OPT_FLUSH) {
		n = 0;
		if (!(opts & OPT_DONOTHING) && ioctl(fd, SIOCIPFFL, &n) == -1)
			perror("ioctl(SIOCFLNAT)");
		else
			printf("%d entries flushed from NAT table\n", n);
	}

	if (opts & OPT_CLEAR) {
		n = 1;
		if (!(opts & OPT_DONOTHING) && ioctl(fd, SIOCIPFFL, &n) == -1)
			perror("ioctl(SIOCCNATL)");
		else
			printf("%d entries flushed from NAT list\n", n);
	}
}


/*
 * Display NAT statistics.
 */
void dostats_dead(nsp, opts)
natstat_t *nsp;
int opts;
{
	nat_t *np, nat;
	ipnat_t	ipn;

	printf("List of active MAP/Redirect filters:\n");
	while (nsp->ns_list) {
		if (kmemcpy((char *)&ipn, (long)nsp->ns_list,
			    sizeof(ipn))) {
			perror("kmemcpy");
			break;
		}
		if (opts & OPT_HITS)
			printf("%lu ", ipn.in_hits);
		printnat(&ipn, opts & (OPT_DEBUG|OPT_VERBOSE));
		nsp->ns_list = ipn.in_next;
	}

	printf("\nList of active sessions:\n");

	for (np = nsp->ns_instances; np; np = nat.nat_next) {
		if (kmemcpy((char *)&nat, (long)np, sizeof(nat)))
			break;
		printactivenat(&nat, opts, 0, nsp->ns_ticks);
		if (nat.nat_aps)
			printaps(nat.nat_aps, opts);
	}

	if (opts & OPT_VERBOSE)
		showhostmap_dead(nsp);
}


void dostats(fd, nsp, opts, alive)
natstat_t *nsp;
int fd, opts, alive;
{
	/*
	 * Show statistics ?
	 */
	if (opts & OPT_STAT) {
		printf("mapped\tin\t%lu\tout\t%lu\n",
			nsp->ns_mapped[0], nsp->ns_mapped[1]);
		printf("added\t%lu\texpired\t%lu\n",
			nsp->ns_added, nsp->ns_expire);
		printf("no memory\t%lu\tbad nat\t%lu\n",
			nsp->ns_memfail, nsp->ns_badnat);
		printf("inuse\t%lu\norphans\t%u\nrules\t%lu\n",
			nsp->ns_inuse, nsp->ns_orphans, nsp->ns_rules);
		printf("wilds\t%u\n", nsp->ns_wilds);
		dotable(nsp, fd, alive);
		if (opts & OPT_VERBOSE)
			printf("table %p list %p\n",
				nsp->ns_table, nsp->ns_list);
		if (alive)
			showtqtable_live(fd);
	}

	if (opts & OPT_LIST) {
		if (alive)
			dostats_live(fd, nsp, opts);
		else
			dostats_dead(nsp, opts);
	}
}


void dotable(nsp, fd, alive)
natstat_t *nsp;
int fd, alive;
{
	int sz, i, used, totallen, maxlen, minlen;
	ipftable_t table;
	u_long *buckets;
	ipfobj_t obj;

	sz = sizeof(*buckets) * nsp->ns_nattab_sz;
	buckets = (u_long *)malloc(sz);

	obj.ipfo_rev = IPFILTER_VERSION;
	obj.ipfo_type = IPFOBJ_GTABLE;
	obj.ipfo_size = sizeof(table);
	obj.ipfo_ptr = &table;

	table.ita_type = IPFTABLE_BUCKETS_NATIN;
	table.ita_table = buckets;

	if (alive) {
		if (ioctl(fd, SIOCGTABL, &obj) != 0) {
			free(buckets);
			return;
		}
	} else {
		if (kmemcpy((char *)buckets, (u_long)nsp->ns_nattab_sz, sz)) {
			free(buckets);
			return;
		}
	}

	totallen = 0;
	maxlen = 0;
	minlen = nsp->ns_inuse;
	used = 0;

	for (i = 0; i < nsp->ns_nattab_sz; i++) {
		if (buckets[i] > maxlen)
			maxlen = buckets[i];
		if (buckets[i] < minlen)
			minlen = buckets[i];
		if (buckets[i] != 0)
			used++;
		totallen += buckets[i];
	}

	printf("hash efficiency\t%2.2f%%\n",
	       totallen ? ((float)used / totallen) * 100.0 : 0.0);
	printf("bucket usage\t%2.2f%%\n",
		((float)used / nsp->ns_nattab_sz) * 100.0);
	printf("minimal length\t%d\n", minlen);
	printf("maximal length\t%d\n", maxlen);
	printf("average length\t%.3f\n", used ? (float)totallen / used : 0.0);
}


/*
 * Display NAT statistics.
 */
void dostats_live(fd, nsp, opts)
natstat_t *nsp;
int fd, opts;
{
	ipfgeniter_t iter;
	ipfobj_t obj;
	ipnat_t	ipn;
	nat_t nat;

	bzero((char *)&obj, sizeof(obj));
	obj.ipfo_rev = IPFILTER_VERSION;
	obj.ipfo_type = IPFOBJ_GENITER;
	obj.ipfo_size = sizeof(iter);
	obj.ipfo_ptr = &iter;

	iter.igi_type = IPFGENITER_IPNAT;
	iter.igi_nitems = 1;
	iter.igi_data = &ipn;

	/*
	 * Show list of NAT rules and NAT sessions ?
	 */
	printf("List of active MAP/Redirect filters:\n");
	while (nsp->ns_list) {
		if (ioctl(fd, SIOCGENITER, &obj) == -1)
			break;
		if (opts & OPT_HITS)
			printf("%lu ", ipn.in_hits);
		printnat(&ipn, opts & (OPT_DEBUG|OPT_VERBOSE));
		nsp->ns_list = ipn.in_next;
	}

	printf("\nList of active sessions:\n");

	iter.igi_type = IPFGENITER_NAT;
	iter.igi_nitems = 1;
	iter.igi_data = &nat;

	while (nsp->ns_instances != NULL) {
		if (ioctl(fd, SIOCGENITER, &obj) == -1)
			break;
		printactivenat(&nat, opts, 1, nsp->ns_ticks);
		if (nat.nat_aps)
			printaps(nat.nat_aps, opts);
		nsp->ns_instances = nat.nat_next;
	}

	if (opts & OPT_VERBOSE)
		showhostmap_live(fd, nsp);
}


/*
 * Display the active host mapping table.
 */
void showhostmap_dead(nsp)
natstat_t *nsp;
{
	hostmap_t hm, *hmp, **maptable;
	u_int hv;

	printf("\nList of active host mappings:\n");

	maptable = (hostmap_t **)malloc(sizeof(hostmap_t *) *
					nsp->ns_hostmap_sz);
	if (kmemcpy((char *)maptable, (u_long)nsp->ns_maptable,
		    sizeof(hostmap_t *) * nsp->ns_hostmap_sz)) {
		perror("kmemcpy (maptable)");
		return;
	}

	for (hv = 0; hv < nsp->ns_hostmap_sz; hv++) {
		hmp = maptable[hv];

		while (hmp) {
			if (kmemcpy((char *)&hm, (u_long)hmp, sizeof(hm))) {
				perror("kmemcpy (hostmap)");
				return;
			}

			printhostmap(&hm, hv);
			hmp = hm.hm_next;
		}
	}
	free(maptable);
}


/*
 * Display the active host mapping table.
 */
void showhostmap_live(fd, nsp)
int fd;
natstat_t *nsp;
{
	ipfgeniter_t iter;
	hostmap_t hm;
	ipfobj_t obj;

	bzero((char *)&obj, sizeof(obj));
	obj.ipfo_rev = IPFILTER_VERSION;
	obj.ipfo_type = IPFOBJ_GENITER;
	obj.ipfo_size = sizeof(iter);
	obj.ipfo_ptr = &iter;

	iter.igi_type = IPFGENITER_HOSTMAP;
	iter.igi_nitems = 1;
	iter.igi_data = &hm;

	printf("\nList of active host mappings:\n");

	while (nsp->ns_maplist != NULL) {
		if (ioctl(fd, SIOCGENITER, &obj) == -1)
			break;
		printhostmap(&hm, 0);
		nsp->ns_maplist = hm.hm_next;
	}
}


void showtqtable_live(fd)
int fd;
{
	ipftq_t table[IPF_TCP_NSTATES];
	ipfobj_t obj;

	bzero((char *)&obj, sizeof(obj));
	obj.ipfo_rev = IPFILTER_VERSION;
	obj.ipfo_size = sizeof(table);
	obj.ipfo_ptr = (void *)table;
	obj.ipfo_type = IPFOBJ_STATETQTAB;

	if (ioctl(fd, SIOCGTQTAB, &obj) == 0) {
		printtqtable(table);      
	}
}
