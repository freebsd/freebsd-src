/*
 * Copyright (c) 1984 Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Sun Microsystems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1984 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)arp.c	5.11.1.1 (Berkeley) 7/22/91";
#endif /* not lint */

/*
 * arp - display, set, and delete arp table entries
 */

#include <sys/param.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include <netdb.h>
#include <netinet/in.h>
#include <net/if.h>
#include <netinet/if_ether.h>

#include <errno.h>
#include <nlist.h>
#include <kvm.h>
#include <stdio.h>
#include <paths.h>

extern int errno;

main(argc, argv)
	int argc;
	char **argv;
{
	int ch;

	while ((ch = getopt(argc, argv, "adsf")) != EOF)
		switch((char)ch) {
		case 'a': {
			char *mem = 0;

			if (argc > 4)
				usage();
			if (argc == 4) {
				mem = argv[3];
			}
			dump((argc >= 3) ? argv[2] : _PATH_UNIX, mem);
			exit(0);
		}
		case 'd':
			if (argc != 3)
				usage();
			delete(argv[2]);
			exit(0);
		case 's':
			if (argc < 4 || argc > 7)
				usage();
			exit(set(argc-2, &argv[2]) ? 1 : 0);
		case 'f':
			if (argc != 3)
				usage();
			exit (file(argv[2]) ? 1 : 0);
		case '?':
		default:
			usage();
		}
	if (argc != 2)
		usage();
	get(argv[1]);
	exit(0);
}

/*
 * Process a file to set standard arp entries
 */
file(name)
	char *name;
{
	FILE *fp;
	int i, retval;
	char line[100], arg[5][50], *args[5];

	if ((fp = fopen(name, "r")) == NULL) {
		fprintf(stderr, "arp: cannot open %s\n", name);
		exit(1);
	}
	args[0] = &arg[0][0];
	args[1] = &arg[1][0];
	args[2] = &arg[2][0];
	args[3] = &arg[3][0];
	args[4] = &arg[4][0];
	retval = 0;
	while(fgets(line, 100, fp) != NULL) {
		i = sscanf(line, "%s %s %s %s %s", arg[0], arg[1], arg[2],
		    arg[3], arg[4]);
		if (i < 2) {
			fprintf(stderr, "arp: bad line: %s\n", line);
			retval = 1;
			continue;
		}
		if (set(i, args))
			retval = 1;
	}
	fclose(fp);
	return (retval);
}

/*
 * Set an individual arp entry 
 */
set(argc, argv)
	int argc;
	char **argv;
{
	struct arpreq ar;
	struct hostent *hp;
	struct sockaddr_in *sin;
	u_char *ea;
	int s;
	char *host = argv[0], *eaddr = argv[1];

	argc -= 2;
	argv += 2;
	bzero((caddr_t)&ar, sizeof ar);
	sin = (struct sockaddr_in *)&ar.arp_pa;
	sin->sin_family = AF_INET;
	sin->sin_addr.s_addr = inet_addr(host);
	if (sin->sin_addr.s_addr == -1) {
		if (!(hp = gethostbyname(host))) {
			fprintf(stderr, "arp: %s: ", host);
			herror((char *)NULL);
			return (1);
		}
		bcopy((char *)hp->h_addr, (char *)&sin->sin_addr,
		    sizeof sin->sin_addr);
	}
	ea = (u_char *)ar.arp_ha.sa_data;
	if (ether_aton(eaddr, ea))
		return (1);
	ar.arp_flags = ATF_PERM;
	while (argc-- > 0) {
		if (strncmp(argv[0], "temp", 4) == 0)
			ar.arp_flags &= ~ATF_PERM;
		else if (strncmp(argv[0], "pub", 3) == 0)
			ar.arp_flags |= ATF_PUBL;
		else if (strncmp(argv[0], "trail", 5) == 0)
			ar.arp_flags |= ATF_USETRAILERS;
		argv++;
	}
	
	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0) {
		perror("arp: socket");
		exit(1);
	}
	if (ioctl(s, SIOCSARP, (caddr_t)&ar) < 0) {
		perror(host);
		exit(1);
	}
	close(s);
	return (0);
}

/*
 * Display an individual arp entry
 */
get(host)
	char *host;
{
	struct arpreq ar;
	struct hostent *hp;
	struct sockaddr_in *sin;
	u_char *ea;
	int s;
	char *inet_ntoa();

	bzero((caddr_t)&ar, sizeof ar);
	ar.arp_pa.sa_family = AF_INET;
	sin = (struct sockaddr_in *)&ar.arp_pa;
	sin->sin_family = AF_INET;
	sin->sin_addr.s_addr = inet_addr(host);
	if (sin->sin_addr.s_addr == -1) {
		if (!(hp = gethostbyname(host))) {
			fprintf(stderr, "arp: %s: ", host);
			herror((char *)NULL);
			exit(1);
		}
		bcopy((char *)hp->h_addr, (char *)&sin->sin_addr,
		    sizeof sin->sin_addr);
	}
	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0) {
		perror("arp: socket");
		exit(1);
	}
	if (ioctl(s, SIOCGARP, (caddr_t)&ar) < 0) {
		if (errno == ENXIO)
			printf("%s (%s) -- no entry\n",
			    host, inet_ntoa(sin->sin_addr));
		else
			perror("SIOCGARP");
		exit(1);
	}
	close(s);
	ea = (u_char *)ar.arp_ha.sa_data;
	printf("%s (%s) at ", host, inet_ntoa(sin->sin_addr));
	if (ar.arp_flags & ATF_COM)
		ether_print(ea);
	else
		printf("(incomplete)");
	if (ar.arp_flags & ATF_PERM)
		printf(" permanent");
	if (ar.arp_flags & ATF_PUBL)
		printf(" published");
	if (ar.arp_flags & ATF_USETRAILERS)
		printf(" trailers");
	printf("\n");
}

/*
 * Delete an arp entry 
 */
delete(host)
	char *host;
{
	struct arpreq ar;
	struct hostent *hp;
	struct sockaddr_in *sin;
	int s;

	bzero((caddr_t)&ar, sizeof ar);
	ar.arp_pa.sa_family = AF_INET;
	sin = (struct sockaddr_in *)&ar.arp_pa;
	sin->sin_family = AF_INET;
	sin->sin_addr.s_addr = inet_addr(host);
	if (sin->sin_addr.s_addr == -1) {
		if (!(hp = gethostbyname(host))) {
			fprintf(stderr, "arp: %s: ", host);
			herror((char *)NULL);
			exit(1);
		}
		bcopy((char *)hp->h_addr, (char *)&sin->sin_addr,
		    sizeof sin->sin_addr);
	}
	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0) {
		perror("arp: socket");
		exit(1);
	}
	if (ioctl(s, SIOCDARP, (caddr_t)&ar) < 0) {
		if (errno == ENXIO)
			printf("%s (%s) -- no entry\n",
			    host, inet_ntoa(sin->sin_addr));
		else
			perror("SIOCDARP");
		exit(1);
	}
	close(s);
	printf("%s (%s) deleted\n", host, inet_ntoa(sin->sin_addr));
}

struct nlist nl[] = {
#define	X_ARPTAB	0
	{ "_arptab" },
#define	X_ARPTAB_SIZE	1
	{ "_arptab_size" },
	{ "" },
};

/*
 * Dump the entire arp table
 */
dump(kernel, mem)
	char *kernel, *mem;
{
	extern int h_errno;
	struct arptab *at;
	struct hostent *hp;
	int bynumber, mf, arptab_size, sz;
	char *host, *malloc();
	off_t lseek();

	if (kvm_openfiles(kernel, mem, NULL) == -1) {
		fprintf(stderr, "arp: kvm_openfiles: %s\n", kvm_geterr());
		exit(1);
	}
	if (kvm_nlist(nl) < 0 || nl[X_ARPTAB_SIZE].n_type == 0) {
		fprintf(stderr, "arp: %s: bad namelist\n", kernel);
		exit(1);
	}
	if (kvm_read((void *)(nl[X_ARPTAB_SIZE].n_value),
		     &arptab_size, sizeof arptab_size) == -1 ||
	    arptab_size <= 0 || arptab_size > 1000) {
		fprintf(stderr, "arp: %s: namelist wrong\n", kernel);
		exit(1);
	}
	sz = arptab_size * sizeof (struct arptab);
	at = (struct arptab *)malloc((u_int)sz);
	if (at == NULL) {
		fputs("arp: can't get memory for arptab.\n", stderr);
		exit(1);
	}
	if (kvm_read((void *)(nl[X_ARPTAB].n_value), (char *)at, sz) == -1) {
		perror("arp: error reading arptab");
		exit(1);
	}
	for (bynumber = 0; arptab_size-- > 0; at++) {
		if (at->at_iaddr.s_addr == 0 || at->at_flags == 0)
			continue;
		if (bynumber == 0)
			hp = gethostbyaddr((caddr_t)&at->at_iaddr,
			    sizeof at->at_iaddr, AF_INET);
		else
			hp = 0;
		if (hp)
			host = hp->h_name;
		else {
			host = "?";
			if (h_errno == TRY_AGAIN)
				bynumber = 1;
		}
		printf("%s (%s) at ", host, inet_ntoa(at->at_iaddr));
		if (at->at_flags & ATF_COM)
			ether_print(at->at_enaddr);
		else
			printf("(incomplete)");
		if (at->at_flags & ATF_PERM)
			printf(" permanent");
		if (at->at_flags & ATF_PUBL)
			printf(" published");
		if (at->at_flags & ATF_USETRAILERS)
			printf(" trailers");
		printf("\n");
	}
}

ether_print(cp)
	u_char *cp;
{
	printf("%x:%x:%x:%x:%x:%x", cp[0], cp[1], cp[2], cp[3], cp[4], cp[5]);
}

ether_aton(a, n)
	char *a;
	u_char *n;
{
	int i, o[6];

	i = sscanf(a, "%x:%x:%x:%x:%x:%x", &o[0], &o[1], &o[2],
					   &o[3], &o[4], &o[5]);
	if (i != 6) {
		fprintf(stderr, "arp: invalid Ethernet address '%s'\n", a);
		return (1);
	}
	for (i=0; i<6; i++)
		n[i] = o[i];
	return (0);
}

usage()
{
	printf("usage: arp hostname\n");
	printf("       arp -a [kernel] [kernel_memory]\n");
	printf("       arp -d hostname\n");
	printf("       arp -s hostname ether_addr [temp] [pub] [trail]\n");
	printf("       arp -f filename\n");
	exit(1);
}
