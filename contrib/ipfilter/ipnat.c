/*
 * Copyright (C) 1993-2002 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * Added redirect stuff and a variety of bug fixes. (mcn@EnGarde.com)
 */
#ifdef __sgi
# include <sys/ptimers.h>
#endif
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
#include <nlist.h>
#include "netinet/ip_compat.h"
#include "netinet/ip_fil.h"
#include "netinet/ip_nat.h"
#include "netinet/ip_state.h"
#include "netinet/ip_proxy.h"
#include "ipf.h"
#include "kmem.h"

#if	defined(sun) && !SOLARIS2
# define	STRERROR(x)	sys_errlist[x]
extern	char	*sys_errlist[];
#else
# define	STRERROR(x)	strerror(x)
#endif

#if !defined(lint)
static const char sccsid[] ="@(#)ipnat.c	1.9 6/5/96 (C) 1993 Darren Reed";
static const char rcsid[] = "@(#)$Id: ipnat.c,v 2.16.2.20 2002/02/22 15:32:55 darrenr Exp $";
#endif


#if	SOLARIS
#define	bzero(a,b)	memset(a,0,b)
#endif
int	use_inet6 = 0;
char	thishost[MAXHOSTNAMELEN];

extern	char	*optarg;
extern	ipnat_t	*natparse __P((char *, int));
extern	void	natparsefile __P((int, char *, int));
extern	void	printnat __P((ipnat_t *, int));
extern	void	printactivenat __P((nat_t *, int));
extern	void	printhostmap __P((hostmap_t *, u_int));
extern	char	*getsumd __P((u_32_t));

void	dostats __P((natstat_t *, int)), flushtable __P((int, int));
void	usage __P((char *));
int	countbits __P((u_32_t));
char	*getnattype __P((ipnat_t *));
int	main __P((int, char*[]));
void	printaps __P((ap_session_t *, int));
void	showhostmap __P((natstat_t *nsp));
void	natstat_dead __P((natstat_t *, char *));


void usage(name)
char *name;
{
	fprintf(stderr, "%s: [-CFhlnrsv] [-f filename]\n", name);
	exit(1);
}


int main(argc, argv)
int argc;
char *argv[];
{
	natstat_t ns, *nsp = &ns;
	char	*file, *core, *kernel;
	int	fd, opts, c, mode;

	fd = -1;
	opts = 0;
	file = NULL;
	core = NULL;
	kernel = NULL;
	mode = O_RDWR;

	while ((c = getopt(argc, argv, "CdFf:hlM:N:nrsv")) != -1)
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
			opts |= OPT_NODO;
			mode = O_RDONLY;
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

	if ((kernel != NULL) || (core != NULL)) {
		(void) setgid(getgid());
		(void) setuid(getuid());
	}

	bzero((char *)&ns, sizeof(ns));

	gethostname(thishost, sizeof(thishost));
	thishost[sizeof(thishost) - 1] = '\0';

	if (!(opts & OPT_NODO) && (kernel == NULL) && (core == NULL)) {
		if (openkmem(kernel, core) == -1)
			exit(1);

		if (((fd = open(IPL_NAT, mode)) == -1) &&
		    ((fd = open(IPL_NAT, O_RDONLY)) == -1)) {
			(void) fprintf(stderr, "%s: open: %s\n", IPL_NAT,
				STRERROR(errno));
			exit(1);
		}
		if (ioctl(fd, SIOCGNATS, &nsp) == -1) {
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
			dostats(nsp, opts);
		exit(0);
	}

	if (opts & (OPT_FLUSH|OPT_CLEAR))
		flushtable(fd, opts);
	if (file)
		natparsefile(fd, file, opts);
	if (opts & (OPT_LIST|OPT_STAT))
		dostats(nsp, opts);
	return 0;
}


/*
 * Read nat statistic information in using a symbol table and memory file
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
	 * for us, before returning it to useland, so here we must copy each
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
 * Display NAT statistics.
 */
void dostats(nsp, opts)
natstat_t *nsp;
int opts;
{
	nat_t **nt[2], *np, nat;
	ipnat_t	ipn;

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
		printf("inuse\t%lu\nrules\t%lu\n",
			nsp->ns_inuse, nsp->ns_rules);
		printf("wilds\t%u\n", nsp->ns_wilds);
		if (opts & OPT_VERBOSE)
			printf("table %p list %p\n",
				nsp->ns_table, nsp->ns_list);
	}

	/*
	 * Show list of NAT rules and NAT sessions ?
	 */
	if (opts & OPT_LIST) {
		printf("List of active MAP/Redirect filters:\n");
		while (nsp->ns_list) {
			if (kmemcpy((char *)&ipn, (long)nsp->ns_list,
				    sizeof(ipn))) {
				perror("kmemcpy");
				break;
			}
			if (opts & OPT_HITS)
				printf("%d ", ipn.in_hits);
			printnat(&ipn, opts & (OPT_DEBUG|OPT_VERBOSE));
			nsp->ns_list = ipn.in_next;
		}

		nt[0] = (nat_t **)malloc(sizeof(*nt) * NAT_SIZE);
		if (kmemcpy((char *)nt[0], (long)nsp->ns_table[0],
			    sizeof(**nt) * NAT_SIZE)) {
			perror("kmemcpy");
			return;
		}

		printf("\nList of active sessions:\n");

		for (np = nsp->ns_instances; np; np = nat.nat_next) {
			if (kmemcpy((char *)&nat, (long)np, sizeof(nat)))
				break;
			printactivenat(&nat, opts);
		}

		if (opts & OPT_VERBOSE)
			showhostmap(nsp);
		free(nt[0]);
	}
}


/*
 * display the active host mapping table.
 */
void showhostmap(nsp)
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
 * Issue an ioctl to flush either the NAT rules table or the active mapping
 * table or both.
 */
void flushtable(fd, opts)
int fd, opts;
{
	int n = 0;

	if (opts & OPT_FLUSH) {
		n = 0;
		if (!(opts & OPT_NODO) && ioctl(fd, SIOCIPFFL, &n) == -1)
			perror("ioctl(SIOCFLNAT)");
		else
			printf("%d entries flushed from NAT table\n", n);
	}

	if (opts & OPT_CLEAR) {
		n = 1;
		if (!(opts & OPT_NODO) && ioctl(fd, SIOCIPFFL, &n) == -1)
			perror("ioctl(SIOCCNATL)");
		else
			printf("%d entries flushed from NAT list\n", n);
	}
}
