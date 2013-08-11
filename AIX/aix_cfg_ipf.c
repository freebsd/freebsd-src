/*
 * Copyright (C) 2012 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * $Id$
 */
#include <sys/param.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/ldr.h>
/*
 * This is a workaround for <sys/uio.h> troubles on FreeBSD, HPUX, OpenBSD.
 * Needed here because on some systems <sys/uio.h> gets included by things
 * like <sys/socket.h>
 */
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <net/if.h>
#define _TCP_DEBUG_H_
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

#include <arpa/inet.h>

#include <errno.h>
#include <limits.h>
#include <netdb.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "netinet/ip_compat.h"
#include "netinet/ip_fil.h"

#ifndef __P
# ifdef __STDC__
#  define	__P(x)	x
# else
#  define	__P(x)	()
# endif
#endif
#ifndef __STDC__
# undef		const
# define	const
#endif

/*
 * AIX requires a specific configure/unconfigure program
 */
#undef ASSERT
#include <sys/device.h>
#include <sys/sysconfig.h>

void loadipf __P((int major, int minor, dev_t devno, char *));
void unloadipf __P((int major, int minor, dev_t devno));
void queryipf __P((int major, int minor, dev_t devno));
int checkarg __P((int, char *arg));
void usage __P((char *));

static	char	*ipf_devfiles[] = { IPL_NAME, IPNAT_NAME, IPSTATE_NAME,
				    IPAUTH_NAME, IPSYNC_NAME, IPSCAN_NAME,
				    IPLOOKUP_NAME, NULL };

int
main(int argc, char *argv[])
{
	int major, minor, action;
	dev_t devno;

	action = checkarg(argc, argv[1]);
	if (action == -1)
		usage(argv[0]);

	if (odm_initialize() == -1) {
		fprintf(stderr, "odm_initialize() failed\n");
		exit(1);
	}

	major = genmajor("ipf");
	if (major == -1) {
		fprintf(stderr, "genmajor(ipf) failed\n");
		exit(1);
	}
	minor = 0;

	devno = makedev(major, minor);
	if (devno == -1) {
		fprintf(stderr, "makedev(%d,%d) failed\n", major, minor);
		exit(1);
	}
	printf("Major %d\ndevno %x\n", major, devno);

	switch (action)
	{
	case 1 :
		loadipf(major, minor, devno, argv[2]);
		break;
	case 2 :
		unloadipf(major, minor, devno);
		break;
	case 3 :
		queryipf(major, minor, devno);
		break;
	}

	odm_terminate();

	return 0;
}


void usage(char *prog)
{
	fprintf(stderr, "Usage:\t%s -l\n\t%s -u\n\t%s -q\n",
		prog, prog, prog);
	exit(1);
}


int checkarg(int argc, char *arg)
{
	if (argc < 2)
		return -1;

	if (!strcmp(arg, "-l") && (argc <= 3))
		return 1;

	if (!strcmp(arg, "-u") && (argc == 2))
		return 2;

	if (!strcmp(arg, "-q") && (argc == 2))
		return 3;

	return -1;
}


void
loadipf(int major, int minor, dev_t devno, char *path)
{
	struct cfg_dd ipfcfg;
	struct cfg_load cfg;
	char *buffer[1024];
	char *ipfpath;
	int i;

	bzero(buffer, sizeof(buffer));
	if (path != NULL)
		ipfpath = path;
	else
		ipfpath = "/usr/lib/drivers/ipf";

#if 0
	bzero((char *)&cfg, sizeof(cfg));
	cfg.path = ipfpath;
	cfg.libpath = "/usr/lib/drivers/";
	sysconfig(SYS_SINGLELOAD, &cfg, sizeof(cfg));
	ipfcfg.kmid = cfg.kmid;
#else
	ipfcfg.kmid = (mid_t)loadext(ipfpath, TRUE, TRUE);
#endif
	if (ipfcfg.kmid == (mid_t)NULL)
	{
		perror("loadext");
		buffer[0] = "execerror";
		buffer[1] = "ipf";
		loadquery(1, &buffer[2], sizeof(buffer) - sizeof(*buffer)*2);
		execvp("/usr/sbin/execerror", buffer);
		exit(errno);
	}

	ipfcfg.devno = devno;
	ipfcfg.cmd = CFG_INIT;
	ipfcfg.ddsptr = (caddr_t)NULL;
	ipfcfg.ddslen = 0;

	if (sysconfig(SYS_CFGDD, &ipfcfg, sizeof(ipfcfg)) == -1) {
		perror("sysconifg(SYS_CFGDD)");
		exit(errno);
	}

	for (i = 0; ipf_devfiles[i] != NULL; i++) {
		unlink(ipf_devfiles[i]);
		if (mknod(ipf_devfiles[i], 0600 | _S_IFCHR, devno) == -1) {
			perror("mknod(devfile)");
			exit(errno);
		}
	}
}


void
unloadipf(int major, int minor, dev_t devno)
{
	struct cfg_dd ipfcfg;
	struct cfg_load cfg;
	int i;

	cfg.path = "/usr/lib/drivers/ipf";
	cfg.kmid = 0;
	if (sysconfig(SYS_QUERYLOAD, &cfg, sizeof(cfg)) == -1) {
		perror("sysconfig(SYS_QUERYLOAD)");
		exit(errno);
	}

	ipfcfg.kmid = cfg.kmid;
	ipfcfg.devno = devno;
	ipfcfg.cmd = CFG_TERM;
	if (sysconfig(SYS_CFGDD, &ipfcfg, sizeof(ipfcfg)) == -1) {
		perror("sysconfig(SYS_CFGDD)");
		exit(errno);
	}

	for (i = 0; ipf_devfiles[i] != NULL; i++) {
		unlink(ipf_devfiles[i]);
	}

	if (loadext("ipf", FALSE, FALSE) == NULL) {
		perror("loadext");
		exit(errno);
	}
}


void
queryipf(int major, int minor, dev_t devno)
{
	struct cfg_dd ipfcfg;
	struct cfg_load cfg;
	int i;

	cfg.path = "/usr/lib/drivers/ipf";
	cfg.kmid = 0;
	if (sysconfig(SYS_QUERYLOAD, &cfg, sizeof(cfg)) == -1) {
		perror("sysconfig(SYS_QUERYLOAD)");
		exit(errno);
	}

	printf("Kernel module ID: %d\n", cfg.kmid);

	ipfcfg.kmid = cfg.kmid;
	ipfcfg.devno = devno;
	ipfcfg.cmd = CFG_QVPD;
	if (sysconfig(SYS_CFGDD, &ipfcfg, sizeof(ipfcfg)) == -1) {
		perror("sysconfig(SYS_CFGDD)");
		exit(errno);
	}
}

