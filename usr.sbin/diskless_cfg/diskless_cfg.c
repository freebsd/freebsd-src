/*************************************************************************

Diskless Configuration Program

Based loosely on the 4.4BSD diskless setup code

*************************************************************************/
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <netdb.h>

#ifdef hpux
#define fhandle_t char
#endif

#ifdef sun
#include <rpc/types.h>
#include <sys/errno.h>
#include <nfs/nfs.h>
#ifdef __SVR4
/* Solaris: compile with -lbsm -lnsl -lsocket */
#define getfh nfs_getfh
#define bcopy(a,b,c) memcpy(b,a,c)
#endif
#endif

#ifdef i386				/* Native 386bsd system */
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/mount.h>
#include <net/if.h>
#include <nfs/nfsv2.h>
#include <nfs/nfsdiskless.h>
#else					/* Other Host system */
#include "diskless.h"
#include "nfsdiskless.h"
#endif

#ifndef i386				/* Most other systems BIG ENDIAN */
#define BIG_ENDIAN
#endif

struct nfs_diskless nfs_diskless;

#define NFS_SOCKET	2049

#define KW_HELP		0
#define KW_INTERFACE	1
#define KW_ROOTFS	2
#define KW_SWAP		3
#define KW_RSIZE	4
#define KW_WSIZE	5
#define KW_NETMASK	6
#define KW_HOSTNAME	7
#define KW_KERNEL	8
#define KW_GATEWAY	9
#define KW_SERVER	10

struct {
	char *name;
	int  keyval;
} keywords[] = {
	{ "-intf",	KW_INTERFACE },
	{ "-rootfs",	KW_ROOTFS },
	{ "-swap",	KW_SWAP },
	{ "-netmask",	KW_NETMASK },
	{ "-rsize",	KW_RSIZE },
	{ "-wsize",	KW_WSIZE },
	{ "-hostname",	KW_HOSTNAME },
	{ "-gateway",	KW_GATEWAY },
	{ "-server",	KW_SERVER },
	{ NULL,		KW_HELP }
};

char *hostname = "386bsd";
char gateway[256];
char cfg[64];
char *rootpath = "/var/386bsd";
char *swappath = "/var/swap/386bsd";
char servername[256];
int rsize = 8192;
int wsize = 8192;

main(argc, argv)
	int argc; char *argv[];
{
	int fd,i,j,cmd;
	unsigned int broadcast, netmask, myip;
	struct hostent *hp;
	struct stat statbuf;
	char buf[1024];
	char *p, *q;

	netmask = 0;
	memset(&nfs_diskless, 0, sizeof(struct nfs_diskless));
	strcpy(nfs_diskless.myif.ifra_name,"ed0");
	nfs_diskless.myif.ifra_addr.sa_len = sizeof(struct sockaddr);
	nfs_diskless.myif.ifra_addr.sa_family = AF_INET;
	nfs_diskless.myif.ifra_broadaddr.sa_len = sizeof(struct sockaddr);
	nfs_diskless.myif.ifra_broadaddr.sa_family = AF_INET;
	nfs_diskless.myif.ifra_mask.sa_len = sizeof(struct sockaddr);
	nfs_diskless.myif.ifra_mask.sa_family = AF_UNSPEC;
	nfs_diskless.swap_args.sotype = i386order(SOCK_DGRAM);
	nfs_diskless.swap_args.flags = i386order(NFSMNT_WSIZE | NFSMNT_RSIZE);
	nfs_diskless.swap_args.timeo = i386order(10);
	nfs_diskless.swap_args.retrans = i386order(100);
	nfs_diskless.swap_saddr.sa_len = sizeof(struct sockaddr);
	nfs_diskless.swap_saddr.sa_family = AF_INET;
	nfs_diskless.root_args.sotype = i386order(SOCK_DGRAM);
	nfs_diskless.root_args.flags = i386order(NFSMNT_WSIZE | NFSMNT_RSIZE);
	nfs_diskless.root_args.timeo = i386order(10);
	nfs_diskless.root_args.retrans = i386order(100);
	nfs_diskless.root_saddr.sa_len = sizeof(struct sockaddr);
	nfs_diskless.root_saddr.sa_family = AF_INET;

	i = 1;
	while (i < argc) {
		cmd = KW_HELP;
		for (j=0; keywords[j].name; j++) {
			if (!strcmp(keywords[j].name, argv[i])) {
				if ((i+1) < argc) {
					cmd = keywords[j].keyval;
					break;
				}
			}
		}
		switch(cmd) {
			case KW_HELP:
				help(argv[0], argv[i]);
				exit(2);
			case KW_INTERFACE:
				if (strlen(argv[i+1]) >= IFNAMSIZ) {
					fprintf(stderr,
						"%s: interface name '%s' too long.\n",
						argv[0], argv[i+1]);
					exit(2);
				}
				strcpy(nfs_diskless.myif.ifra_name, argv[i+1]);
				i += 2;
				break;
			case KW_ROOTFS:
				rootpath = argv[i+1];
				i += 2;
				break;
			case KW_SWAP:
				swappath = argv[i+1];
				i += 2;
				break;
			case KW_RSIZE:
				rsize = atoi(argv[i+1]);
				i += 2;
				break;
			case KW_WSIZE:
				wsize = atoi(argv[i+1]);
				i += 2;
				break;
			case KW_NETMASK:
				netmask = inet_addr(argv[i+1]);
				i +=2;
				break;
			case KW_HOSTNAME:
				hostname = argv[i+1];
				i += 2;
				break;
			case KW_SERVER:
				strcpy(servername,argv[i+1]);
				i += 2;
				break;
			case KW_GATEWAY:
				strcpy(gateway,argv[i+1]);
				i += 2;
				break;
		}
	}

	if (!*servername && gethostname(servername, sizeof servername) < 0) {
		fprintf(stderr,"%s: unable to get host server name\n",argv[0]);
		exit(2);
	}
	if ((hp = gethostbyname(servername)) == NULL) {
		fprintf(stderr,"%s: unable to get host address\n",argv[0]);
		exit(2);
	}
	p = servername;
	while (*p && (*p != '.')) p++;
	*p = 0;
	nfs_diskless.swap_saddr.sa_data[0] = nfs_diskless.root_saddr.sa_data[0]
		= NFS_SOCKET >> 8;
	nfs_diskless.swap_saddr.sa_data[1] = nfs_diskless.root_saddr.sa_data[1]
		= NFS_SOCKET & 0x00FF;
	bcopy(*hp->h_addr_list, &nfs_diskless.swap_saddr.sa_data[2], 4);
	bcopy(*hp->h_addr_list, &nfs_diskless.root_saddr.sa_data[2], 4);
	
	if (!*gateway && gethostname(gateway, sizeof gateway) < 0) {
		fprintf(stderr,"%s: unable to get gateway host name\n",argv[0]);
		exit(2);
	}
	if ((hp = gethostbyname(gateway)) == NULL) {
		fprintf(stderr,"%s: unable to get gateway host address\n",argv[0]);
		exit(2);
	}
	nfs_diskless.mygateway.sa_len = sizeof(struct sockaddr);
	nfs_diskless.mygateway.sa_family = AF_INET;
	nfs_diskless.mygateway.sa_data[0] = NFS_SOCKET >> 8;
	nfs_diskless.mygateway.sa_data[1] = NFS_SOCKET & 0x00FF;
	bcopy(*hp->h_addr_list, &nfs_diskless.mygateway.sa_data[2], 4);

	nfs_diskless.swap_args.rsize = i386order(rsize);
	nfs_diskless.swap_args.wsize = i386order(wsize);
	nfs_diskless.root_args.rsize = i386order(rsize);
	nfs_diskless.root_args.wsize = i386order(wsize);
	if ((hp = gethostbyname(hostname)) == NULL) {
		fprintf(stderr,"%s: unable to get diskless address\n",argv[0]);
		exit(2);
	}
	bcopy(*hp->h_addr_list, &nfs_diskless.myif.ifra_addr.sa_data[2], 4);
	if (!netmask) {
		unsigned char net;
		net = nfs_diskless.myif.ifra_addr.sa_data[2];
		if (net <= 127)
			netmask = inet_addr("255.0.0.0");
		else if (net < 192)
			netmask = inet_addr("255.255.0.0");
		else	netmask = inet_addr("255.255.255.0");
	}
	bcopy(*hp->h_addr_list, &myip, 4);
	broadcast = (myip & netmask) | ~netmask;
	bcopy(&broadcast, &nfs_diskless.myif.ifra_broadaddr.sa_data[2], 4);
	bcopy(&netmask, &nfs_diskless.myif.ifra_mask.sa_data[2], 4);
	if (stat(rootpath, &statbuf) < 0) {
		fprintf(stderr,"%s: unable to stat root '%s'\n",
			argv[0],rootpath);
		exit(2);
	}
	if (!S_ISDIR(statbuf.st_mode)) {
		fprintf(stderr,"%s: '%s' is not a directory\n",
			argv[0],rootpath);
		exit(2);
	}
	if (getfh(rootpath, (fhandle_t *)nfs_diskless.root_fh) < 0) {
		fprintf(stderr,"%s: unable to get handle for '%s'\n",
			argv[0],rootpath);
		exit(2);
	}
	sprintf(buf,"%s:%s",servername, rootpath);
	buf[NFSMNAMELEN-1] = 0;
	strcpy(nfs_diskless.root_hostnam,buf);
	printf("root is on %s\n",nfs_diskless.root_hostnam);
	if (stat(swappath, &statbuf) < 0) {
		fprintf(stderr,"%s: unable to stat swap '%s'\n",
			argv[0],swappath);
		exit(2);
	}
	if (!S_ISREG(statbuf.st_mode)) {
		fprintf(stderr,"%s: '%s' is not a regular file\n",
			argv[0],swappath);
		exit(2);
	}
	if (getfh(swappath, (fhandle_t *)nfs_diskless.swap_fh) < 0) {
		fprintf(stderr,"%s: unable to get handle for '%s'\n",
			argv[0],swappath);
		exit(2);
	}
	sprintf(buf,"%s:%s",servername, swappath);
	buf[NFSMNAMELEN-1] = 0;
	strcpy(nfs_diskless.swap_hostnam,buf);
	printf("swap is on %s\n",nfs_diskless.swap_hostnam);
	sprintf(cfg,"cfg.%d.%d.%d.%d",
		((int)nfs_diskless.myif.ifra_addr.sa_data[2]) & 0x00FF,
		((int)nfs_diskless.myif.ifra_addr.sa_data[3]) & 0x00FF,
		((int)nfs_diskless.myif.ifra_addr.sa_data[4]) & 0x00FF,
		((int)nfs_diskless.myif.ifra_addr.sa_data[5]) & 0x00FF);
	if ((fd = open(cfg, O_WRONLY | O_CREAT | O_TRUNC, 0644)) < 0) {
		fprintf(stderr,"%s: unable to open/create %s\n",
			argv[0],cfg);
		exit(2);
	}
	if (write(fd, &nfs_diskless, sizeof(struct nfs_diskless)) !=
		sizeof(struct nfs_diskless)) {
		fprintf(stderr,"%s: unable to write to '%s'\n",
			argv[0],cfg);
		exit(2);
	}
	close(fd);
}

/********************************************************************
HELP - Print help message
********************************************************************/
help(prog, keywd)
	char *prog, *keywd;
{
	int i;
	fprintf(stderr,"%s: invalid keyword '%s' or not enough parameters\n",prog,keywd);
	fprintf(stderr,"     valid keywords: ");
	for (i=0; keywords[i].name; i++) fprintf(stderr,"%s ", keywords[i].name);
	fprintf(stderr,"\n");
}

/*********************************************************************
I386ORDER - Byte swap
*********************************************************************/
i386order(i)
	unsigned int i;
{
#ifndef i386
	return( ((i >> 24) & 0x000000FF) |
		((i >> 8)  & 0x0000FF00) |
		((i << 8)  & 0x00FF0000) |
		((i << 24) & 0xFF000000));
#else
	return(i);
#endif
}
