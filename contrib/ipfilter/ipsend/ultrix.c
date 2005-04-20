/*
 * (C)opyright 1998 Darren Reed. (from tcplog)
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 */
#include <stdio.h>
#include <strings.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netdnet/dli_var.h>


static	struct	dli_devid	dli_devid;


int	initdevice(device, sport, tout)
char	*device;
int	sport, tout;
{
	u_char	*s;
	int	fd;

	fd = socket(AF_DLI, SOCK_DGRAM, 0);
	if (fd == -1)
		perror("socket(AF_DLI,SOCK_DGRAM)");
	else {
		strncpy(dli_devid.dli_devname, device, DLI_DEVSIZE);
		dli_devid.dli_devname[DLI_DEVSIZE] ='\0';
		for (s = dli_devid.dli_devname; *s && isalpha((char)*s); s++)
			;
		if (*s && isdigit((char)*s)) {
			dli_devid.dli_devnumber = atoi(s);
		}
	}
	return fd;
}


/*
 * output an IP packet onto a fd opened for /dev/bpf
 */
int	sendip(fd, pkt, len)
int	fd, len;
char	*pkt;
{
	struct sockaddr_dl dl;
	struct sockaddr_edl *edl = &dl.choose_addr.dli_eaddr;

	dl.dli_family = AF_DLI;
	dl.dli_substructype = DLI_ETHERNET;
	bcopy((char *)&dli_devid, (char *)&dl.dli_device, sizeof(dli_devid));
	bcopy(pkt, edl->dli_target, DLI_EADDRSIZE);
	bcopy(pkt, edl->dli_dest, DLI_EADDRSIZE);
	bcopy(pkt + DLI_EADDRSIZE * 2, (char *)&edl->dli_protype, 2);
	edl->dli_ioctlflg = 0;

	if (sendto(fd, pkt, len, 0, (struct sockaddr *)&dl, sizeof(dl)) == -1)
	    {
		perror("send");
		return -1;
	    }

	return len;
}


char *strdup(str)
char *str;
{
	char	*s;

	if ((s = (char *)malloc(strlen(str) + 1)))
		return strcpy(s, str);
	return NULL;
}
