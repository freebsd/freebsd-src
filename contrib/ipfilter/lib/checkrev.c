/*	$FreeBSD$	*/

/*
 * Copyright (C) 1993-2001 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * Id: checkrev.c,v 1.12.2.1 2004/03/09 14:44:39 darrenr Exp
 */

#include <sys/ioctl.h>
#include <fcntl.h>

#include "ipf.h"
#include "netinet/ipl.h"

int checkrev(ipfname)
char *ipfname;
{
	static int vfd = -1;
	struct friostat fio, *fiop = &fio;
	ipfobj_t ipfo;

	bzero((caddr_t)&ipfo, sizeof(ipfo));
	ipfo.ipfo_rev = IPFILTER_VERSION;
	ipfo.ipfo_size = sizeof(*fiop);
	ipfo.ipfo_ptr = (void *)fiop;
	ipfo.ipfo_type = IPFOBJ_IPFSTAT;

	if ((vfd == -1) && ((vfd = open(ipfname, O_RDONLY)) == -1)) {
		perror("open device");
		return -1;
	}

	if (ioctl(vfd, SIOCGETFS, &ipfo)) {
		perror("ioctl(SIOCGETFS)");
		close(vfd);
		vfd = -1;
		return -1;
	}

	if (strncmp(IPL_VERSION, fio.f_version, sizeof(fio.f_version))) {
		return -1;
	}
	return 0;
}
