/*
 * Copyright (C) 1999-2001 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 */
#ifdef	__FreeBSD__
# ifndef __FreeBSD_cc_version
#  include <osreldate.h>
# else
#  if __FreeBSD_cc_version < 430000
#   include <osreldate.h>
#  endif
# endif
#endif
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#if !defined(__SVR4) && !defined(__GNUC__)
#include <strings.h>
#endif
#include <sys/types.h>
#include <sys/param.h>
#include <sys/file.h>
#include <stdlib.h>
#include <stddef.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <sys/time.h>
#include <net/if.h>
#if __FreeBSD_version >= 300000
# include <net/if_var.h>
#endif
#include <netinet/ip.h>
#include <netdb.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include "ip_compat.h"
#include "ip_fil.h"
#include "ip_nat.h"
#include "ip_state.h"
#include "ipf.h"

#if !defined(lint)
static const char rcsid[] = "@(#)$Id: ipfs.c,v 2.6.2.8 2001/09/14 18:52:21 darrenr Exp $";
#endif

#ifndef	IPF_SAVEDIR
# define	IPF_SAVEDIR	"/var/db/ipf"
#endif
#ifndef IPF_NATFILE
# define	IPF_NATFILE	"ipnat.ipf"
#endif
#ifndef IPF_STATEFILE
# define	IPF_STATEFILE	"ipstate.ipf"
#endif

#if !defined(__SVR4) && defined(__GNUC__)
extern	char	*index __P((const char *, int));
#endif

extern	char	*optarg;

int	main __P((int, char *[]));
void	usage __P((void));
int	changestateif __P((char *, char *));
int	changenatif __P((char *, char *));
int	readstate __P((int, char *));
int	readnat __P((int, char *));
int	writestate __P((int, char *));
int	opendevice __P((char *));
void	closedevice __P((int));
int	setlock __P((int, int));
int	writeall __P((char *));
int	readall __P((char *));
int	writenat __P((int, char *));

int	opts = 0;


void usage()
{
	fprintf(stderr, "usage: ipfs [-nv] -l\n");
	fprintf(stderr, "usage: ipfs [-nv] -u\n");
	fprintf(stderr, "usage: ipfs [-nv] [-d <dir>] -R\n");
	fprintf(stderr, "usage: ipfs [-nv] [-d <dir>] -W\n");
	fprintf(stderr, "usage: ipfs [-nNSv] [-f <file>] -r\n");
	fprintf(stderr, "usage: ipfs [-nNSv] [-f <file>] -w\n");
	fprintf(stderr, "usage: ipfs [-nNSv] -f <filename> -i <if1>,<if2>\n");
	exit(1);
}


/*
 * Change interface names in state information saved out to disk.
 */
int changestateif(ifs, fname)
char *ifs, *fname;
{
	int fd, olen, nlen, rw;
	ipstate_save_t ips;
	off_t pos;
	char *s;

	s = strchr(ifs, ',');
	if (!s)
		usage();
	*s++ = '\0';
	nlen = strlen(s);
	olen = strlen(ifs);
	if (nlen >= sizeof(ips.ips_is.is_ifname) ||
	    olen >= sizeof(ips.ips_is.is_ifname))
		usage();

	fd = open(fname, O_RDWR);
	if (fd == -1) {
		perror("open");
		exit(1);
	}

	for (pos = 0; read(fd, &ips, sizeof(ips)) == sizeof(ips); ) {
		rw = 0;
		if (!strncmp(ips.ips_is.is_ifname[0], ifs, olen + 1)) {
			strcpy(ips.ips_is.is_ifname[0], s);
			rw = 1;
		}
		if (!strncmp(ips.ips_is.is_ifname[1], ifs, olen + 1)) {
			strcpy(ips.ips_is.is_ifname[1], s);
			rw = 1;
		}
		if (rw == 1) {
			if (lseek(fd, pos, SEEK_SET) != pos) {
				perror("lseek");
				exit(1);
			}
			if (write(fd, &ips, sizeof(ips)) != sizeof(ips)) {
				perror("write");
				exit(1);
			}
		}
		pos = lseek(fd, 0, SEEK_CUR);
	}
	close(fd);

	return 0;
}


/*
 * Change interface names in NAT information saved out to disk.
 */
int changenatif(ifs, fname)
char *ifs, *fname;
{
	int fd, olen, nlen, rw;
	nat_save_t ipn;
	nat_t *nat;
	off_t pos;
	char *s;

	s = strchr(ifs, ',');
	if (!s)
		usage();
	*s++ = '\0';
	nlen = strlen(s);
	olen = strlen(ifs);
	nat = &ipn.ipn_nat;
	if (nlen >= sizeof(nat->nat_ifname) || olen >= sizeof(nat->nat_ifname))
		usage();

	fd = open(fname, O_RDWR);
	if (fd == -1) {
		perror("open");
		exit(1);
	}

	for (pos = 0; read(fd, &ipn, sizeof(ipn)) == sizeof(ipn); ) {
		rw = 0;
		if (!strncmp(nat->nat_ifname, ifs, olen + 1)) {
			strcpy(nat->nat_ifname, s);
			rw = 1;
		}
		if (rw == 1) {
			if (lseek(fd, pos, SEEK_SET) != pos) {
				perror("lseek");
				exit(1);
			}
			if (write(fd, &ipn, sizeof(ipn)) != sizeof(ipn)) {
				perror("write");
				exit(1);
			}
		}
		pos = lseek(fd, 0, SEEK_CUR);
	}
	close(fd);

	return 0;
}


int main(argc,argv)
int argc;
char *argv[];
{
	int c, lock = -1, devfd = -1, err = 0, rw = -1, ns = -1, set = 0;
	char *dirname = NULL, *filename = NULL, *ifs = NULL;

	while ((c = getopt(argc, argv, "d:f:lNnSRruvWw")) != -1)
		switch (c)
		{
		case 'd' :
			if ((set == 0) && !dirname && !filename)
				dirname = optarg;
			else
				usage();
			break;
		case 'f' :
			if ((set == 0) && !dirname && !filename)
				filename = optarg;
			else
				usage();
			break;
		case 'i' :
			ifs = optarg;
			set = 1;
			break;
		case 'l' :
			if (filename || dirname || set)
				usage();
			lock = 1;
			set = 1;
			break;
		case 'n' :
			opts |= OPT_DONOTHING;
			break;
		case 'N' :
			if ((ns >= 0) || dirname || (rw != -1) || set)
				usage();
			ns = 0;
			set = 1;
			break;
		case 'r' :
			if ((ns >= 0) || dirname || (rw != -1))
				usage();
			rw = 0;
			set = 1;
			break;
		case 'R' :
			rw = 2;
			set = 1;
			break;
		case 'S' :
			if ((ns >= 0) || dirname || (rw != -1) || set)
				usage();
			ns = 1;
			set = 1;
			break;
		case 'u' :
			if (filename || dirname || set)
				usage();
			lock = 0;
			set = 1;
			break;
		case 'v' :
			opts |= OPT_VERBOSE;
			break;
		case 'w' :
			if (dirname || (rw != -1) || (ns == -1))
				usage();
			rw = 1;
			set = 1;
			break;
		case 'W' :
			rw = 3;
			set = 1;
			break;
		case '?' :
		default :
			usage();
		}

	if (ifs) {
		if (!filename || ns < 0)
			usage();
		if (ns == 0)
			return changenatif(ifs, filename);
		else
			return changestateif(ifs, filename);
	}

	if ((ns >= 0) || (lock >= 0)) {
		if (lock >= 0)
			devfd = opendevice(NULL);
		else if (ns >= 0) {
			if (ns == 1)
				devfd = opendevice(IPL_STATE);
			else if (ns == 0)
				devfd = opendevice(IPL_NAT);
		}
		if (devfd == -1)
			exit(1);
	}

	if (lock >= 0)
		err = setlock(devfd, lock);
	else if (rw >= 0) {
		if (rw & 1) {	/* WRITE */
			if (rw & 2)
				err = writeall(dirname);
			else {
				if (ns == 0)
					err = writenat(devfd, filename);
				else if (ns == 1)
					err = writestate(devfd, filename);
			}
		} else {
			if (rw & 2)
				err = readall(dirname);
			else {
				if (ns == 0)
					err = readnat(devfd, filename);
				else if (ns == 1)
					err = readstate(devfd, filename);
			}
		}
	}
	return err;
}


int opendevice(ipfdev)
char *ipfdev;
{
	int fd = -1;

	if (opts & OPT_DONOTHING)
		return -2;

	if (!ipfdev)
		ipfdev = IPL_NAME;

	if ((fd = open(ipfdev, O_RDWR)) == -1)
		if ((fd = open(ipfdev, O_RDONLY)) == -1)
			perror("open device");
	return fd;
}


void closedevice(fd)
int fd;
{
	close(fd);
}


int setlock(fd, lock)
int fd, lock;
{
	if (opts & OPT_VERBOSE)
		printf("Turn lock %s\n", lock ? "on" : "off");
	if (!(opts & OPT_DONOTHING)) {
		if (ioctl(fd, SIOCSTLCK, &lock) == -1) {
			perror("SIOCSTLCK");
			return 1;
		}
		if (opts & OPT_VERBOSE)
			printf("Lock now %s\n", lock ? "on" : "off");
	}
	return 0;
}


int writestate(fd, file)
int fd;
char *file;
{
	ipstate_save_t ips, *ipsp;
	int wfd = -1;

	if (!file)
		file = IPF_STATEFILE;

	wfd = open(file, O_WRONLY|O_TRUNC|O_CREAT, 0600);
	if (wfd == -1) {
		fprintf(stderr, "%s ", file);
		perror("state:open");
		return 1;
	}

	ipsp = &ips;
	bzero((char *)ipsp, sizeof(ips));

	do {
		if (opts & OPT_VERBOSE)
			printf("Getting state from addr %p\n", ips.ips_next);
		if (ioctl(fd, SIOCSTGET, &ipsp)) {
			if (errno == ENOENT)
				break;
			perror("state:SIOCSTGET");
			close(wfd);
			return 1;
		}
		if (opts & OPT_VERBOSE)
			printf("Got state next %p\n", ips.ips_next);
		if (write(wfd, ipsp, sizeof(ips)) != sizeof(ips)) {
			perror("state:write");
			close(wfd);
			return 1;
		}
	} while (ips.ips_next != NULL);
	close(wfd);

	return 0;
}


int readstate(fd, file)
int fd;
char *file;
{
	ipstate_save_t ips, *is, *ipshead = NULL, *is1, *ipstail = NULL;
	int sfd = -1, i;

	if (!file)
		file = IPF_STATEFILE;

	sfd = open(file, O_RDONLY, 0600);
	if (sfd == -1) {
		fprintf(stderr, "%s ", file);
		perror("open");
		return 1;
	}

	bzero((char *)&ips, sizeof(ips));

	/*
	 * 1. Read all state information in.
	 */
	do {
		i = read(sfd, &ips, sizeof(ips));
		if (i == -1) {
			perror("read");
			close(sfd);
			return 1;
		}
		if (i == 0)
			break;
		if (i != sizeof(ips)) {
			fprintf(stderr, "incomplete read: %d != %d\n", i,
				(int)sizeof(ips));
			close(sfd);
			return 1;
		}
		is = (ipstate_save_t *)malloc(sizeof(*is));
		if(!is) {
			fprintf(stderr, "malloc failed\n");
			return 1;
		}

		bcopy((char *)&ips, (char *)is, sizeof(ips));

		/*
		 * Check to see if this is the first state entry that will
		 * reference a particular rule and if so, flag it as such
		 * else just adjust the rule pointer to become a pointer to
		 * the other.  We do this so we have a means later for tracking
		 * who is referencing us when we get back the real pointer
		 * in is_rule after doing the ioctl.
		 */
		for (is1 = ipshead; is1 != NULL; is1 = is1->ips_next)
			if (is1->ips_rule == is->ips_rule)
				break;
		if (is1 == NULL)
			is->ips_is.is_flags |= FI_NEWFR;
		else
			is->ips_rule = (void *)&is1->ips_rule;

		/*
		 * Use a tail-queue type list (add things to the end)..
		 */
		is->ips_next = NULL;
		if (!ipshead)
			ipshead = is;
		if (ipstail)
			ipstail->ips_next = is;
		ipstail = is;
	} while (1);

	close(sfd);

	for (is = ipshead; is; is = is->ips_next) {
		if (opts & OPT_VERBOSE)
			printf("Loading new state table entry\n");
		if (is->ips_is.is_flags & FI_NEWFR) {
			if (opts & OPT_VERBOSE)
				printf("Loading new filter rule\n");
		}
		if (!(opts & OPT_DONOTHING))
			if (ioctl(fd, SIOCSTPUT, &is)) {
				perror("SIOCSTPUT");
				return 1;
			}

		if (is->ips_is.is_flags & FI_NEWFR) {
			if (opts & OPT_VERBOSE)
				printf("Real rule addr %p\n", is->ips_rule);
			for (is1 = is->ips_next; is1; is1 = is1->ips_next)
				if (is1->ips_rule == (frentry_t *)&is->ips_rule)
					is1->ips_rule = is->ips_rule;
		}
	}

	return 0;
}


int readnat(fd, file)
int fd;
char *file;
{
	nat_save_t ipn, *in, *ipnhead = NULL, *in1, *ipntail = NULL, *ipnp;
	int nfd = -1, i;
	nat_t *nat;

	if (!file)
		file = IPF_NATFILE;

	nfd = open(file, O_RDONLY);
	if (nfd == -1) {
		fprintf(stderr, "%s ", file);
		perror("nat:open");
		return 1;
	}

	bzero((char *)&ipn, sizeof(ipn));
	ipnp = &ipn;

	/*
	 * 1. Read all state information in.
	 */
	do {
		i = read(nfd, &ipn, sizeof(ipn));
		if (i == -1) {
			perror("read");
			close(nfd);
			return 1;
		}
		if (i == 0)
			break;
		if (i != sizeof(ipn)) {
			fprintf(stderr, "incomplete read: %d != %d\n", i,
				(int)sizeof(ipn));
			close(nfd);
			return 1;
		}

		if (ipn.ipn_dsize > 0) {
			char *s = ipnp->ipn_data;
			int n = ipnp->ipn_dsize;

			n -= sizeof(ipnp->ipn_data);
			in = malloc(sizeof(*in) + n);
			if (!in)
				break;

			s += sizeof(ipnp->ipn_data);
			i = read(nfd, s, n);
			if (i == 0)
				break;
			if (i != n) {
				fprintf(stderr, "incomplete read: %d != %d\n",
					i, n);
				close(nfd);
				return 1;
			}
		} else
			in = (nat_save_t *)malloc(sizeof(*in));
		bcopy((char *)ipnp, (char *)in, sizeof(ipn));

		/*
		 * Check to see if this is the first state entry that will
		 * reference a particular rule and if so, flag it as such
		 * else just adjust the rule pointer to become a pointer to
		 * the other.  We do this so we have a means later for tracking
		 * who is referencing us when we get back the real pointer
		 * in is_rule after doing the ioctl.
		 */
		nat = &in->ipn_nat;
		if (nat->nat_fr != NULL) {
			for (in1 = ipnhead; in1 != NULL; in1 = in1->ipn_next)
				if (in1->ipn_rule == nat->nat_fr)
					break;
			if (in1 == NULL)
				nat->nat_flags |= FI_NEWFR;
			else
				nat->nat_fr = &in1->ipn_fr;
		}

		/*
		 * Use a tail-queue type list (add things to the end)..
		 */
		in->ipn_next = NULL;
		if (!ipnhead)
			ipnhead = in;
		if (ipntail)
			ipntail->ipn_next = in;
		ipntail = in;
	} while (1);

	close(nfd);

	for (in = ipnhead; in; in = in->ipn_next) {
		if (opts & OPT_VERBOSE)
			printf("Loading new NAT table entry\n");
		nat = &in->ipn_nat;
		if (nat->nat_flags & FI_NEWFR) {
			if (opts & OPT_VERBOSE)
				printf("Loading new filter rule\n");
		}
		if (!(opts & OPT_DONOTHING))
			if (ioctl(fd, SIOCSTPUT, &in)) {
				perror("SIOCSTPUT");
				return 1;
			}

		if (nat->nat_flags & FI_NEWFR) {
			if (opts & OPT_VERBOSE)
				printf("Real rule addr %p\n", nat->nat_fr);
			for (in1 = in->ipn_next; in1; in1 = in1->ipn_next)
				if (in1->ipn_rule == &in->ipn_fr)
					in1->ipn_rule = nat->nat_fr;
		}
	}

	return 0;
}


int writenat(fd, file)
int fd;
char *file;
{
	nat_save_t *ipnp = NULL, *next = NULL;
	int nfd = -1;
	natget_t ng;

	if (!file)
		file = IPF_NATFILE;

	nfd = open(file, O_WRONLY|O_TRUNC|O_CREAT, 0600);
	if (nfd == -1) {
		fprintf(stderr, "%s ", file);
		perror("nat:open");
		return 1;
	}


	do {
		if (opts & OPT_VERBOSE)
			printf("Getting nat from addr %p\n", ipnp);
		ng.ng_ptr = next;
		ng.ng_sz = 0;
		if (ioctl(fd, SIOCSTGSZ, &ng)) {
			perror("nat:SIOCSTGSZ");
			close(nfd);
			return 1;
		}

		if (opts & OPT_VERBOSE)
			printf("NAT size %d from %p\n", ng.ng_sz, ng.ng_ptr);

		if (ng.ng_sz == 0)
			break;

		if (!ipnp)
			ipnp = malloc(ng.ng_sz);
		else
			ipnp = realloc((char *)ipnp, ng.ng_sz);
		if (!ipnp) {
			fprintf(stderr,
				"malloc for %d bytes failed\n", ng.ng_sz);
			break;
		}

		bzero((char *)ipnp, ng.ng_sz);
		ipnp->ipn_next = next;
		if (ioctl(fd, SIOCSTGET, &ipnp)) {
			if (errno == ENOENT)
				break;
			perror("nat:SIOCSTGET");
			close(nfd);
			return 1;
		}

		if (opts & OPT_VERBOSE)
			printf("Got nat next %p\n", ipnp->ipn_next);
		if (write(nfd, ipnp, ng.ng_sz) != ng.ng_sz) {
			perror("nat:write");
			close(nfd);
			return 1;
		}
		next = ipnp->ipn_next;
	} while (ipnp && next);
	close(nfd);

	return 0;
}


int writeall(dirname)
char *dirname;
{
	int fd, devfd;

	if (!dirname)
		dirname = IPF_SAVEDIR;

	if (chdir(dirname)) {
		perror("chdir(IPF_SAVEDIR)");
		return 1;
	}

	fd = opendevice(NULL);
	if (fd == -1)
		return 1;
	if (setlock(fd, 1)) {
		close(fd);
		return 1;
	}

	devfd = opendevice(IPL_STATE);
	if (devfd == -1)
		return 1;
	if (writestate(devfd, NULL))
		return 1;
	close(devfd);

	devfd = opendevice(IPL_NAT);
	if (devfd == -1)
		return 1;
	if (writenat(devfd, NULL))
		return 1;
	close(devfd);

	if (setlock(fd, 0)) {
		close(fd);
		return 1;
	}

	return 0;
}


int readall(dirname)
char *dirname;
{
	int fd, devfd;

	if (!dirname)
		dirname = IPF_SAVEDIR;

	if (chdir(dirname)) {
		perror("chdir(IPF_SAVEDIR)");
		return 1;
	}

	fd = opendevice(NULL);
	if (fd == -1)
		return 1;
	if (setlock(fd, 1)) {
		close(fd);
		return 1;
	}

	devfd = opendevice(IPL_STATE);
	if (devfd == -1)
		return 1;
	if (readstate(devfd, NULL))
		return 1;
	close(devfd);

	devfd = opendevice(IPL_NAT);
	if (devfd == -1)
		return 1;
	if (readnat(devfd, NULL))
		return 1;
	close(devfd);

	if (setlock(fd, 0)) {
		close(fd);
		return 1;
	}

	return 0;
}
