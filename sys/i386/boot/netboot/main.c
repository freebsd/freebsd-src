/**************************************************************************
NETBOOT -  BOOTP/TFTP Bootstrap Program

Author: Martin Renters
  Date: Dec/93

**************************************************************************/

/* #define MDEBUG */

#include "netboot.h"

int	jmp_bootmenu[10];

struct	exec head;
char	*loadpoint;
char	*kernel;
char	kernel_buf[128];
void	(*kernelentry)();
struct	nfs_diskless nfsdiskless;
int	hostnamelen;
char	config_buffer[512];		/* Max TFTP packet */
struct	bootinfo bootinfo;
int	root_nfs_port;
unsigned long	netmask;
char	kernel_handle[32];
int 	offset;

extern char eth_driver[];
extern	char packet[];
extern	int packetlen, rpc_id;
char	broadcast[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

/**************************************************************************
MAIN - Kick off routine
**************************************************************************/
main()
{
	int c;
	char *p;
	extern char edata[], end[];
	for (p=edata; p<end; p++) *p = 0;	/* Zero BSS */
#ifdef ASK_BOOT
	while (1) {
		printf("\n\rBoot from Network (Y/N) ? ");
		c = getchar();
		if ((c >= 'a') && (c <= 'z')) c &= 0x5F;
		if (c == '\r') break;
		putchar(c);
		if (c == 'N')
			exit(0);
		if (c == 'Y')
			break;
		printf(" - bad response\n\r");
	}
#endif
	gateA20();
	printf("\r\nBOOTP/TFTP/NFS bootstrap loader    ESC for menu\n\r");
	printf("\r\nSearching for adapter...");
	if (!eth_probe()) {
		printf("No adapter found.\r\n");
		exit(0);
	}
	kernel = DEFAULT_BOOTFILE;
	while (1) {
		if (setjmp(jmp_bootmenu))
			bootmenu();
		else
			load();
	}
}

void
nfsload(length)
{
	int err, read_size;

	while (length > 0) {
		read_size = length > NFS_READ_SIZE ?
				NFS_READ_SIZE : length;
		if ((err = nfs_read(ARP_ROOTSERVER, root_nfs_port,
			&kernel_handle, offset, read_size, loadpoint)) !=
				read_size) {
			if (err < 0) {
				printf("Unable to read data: ");
				nfs_err(err);
			}
			longjmp(jmp_bootmenu, 1);
		}
		loadpoint += err;
		length -= err;
		offset += err;
	}
}

/**************************************************************************
LOAD - Try to get booted
**************************************************************************/
load()
{
	char	*p,*q;
	char	cfg[64];
	int	root_mount_port;
	int	swap_nfs_port;
	int	swap_mount_port;
	char	cmd_line[80];
	int	err, read_size, i;
	long	addr, broadcast;
	unsigned long pad;

/* Initialize this early on */

        nfsdiskless.root_args.rsize = 8192;
        nfsdiskless.root_args.wsize = 8192;
        nfsdiskless.swap_args.rsize = 8192;
        nfsdiskless.swap_args.wsize = 8192;
        nfsdiskless.root_args.sotype = SOCK_DGRAM;
        nfsdiskless.root_args.flags = (NFSMNT_WSIZE | NFSMNT_RSIZE);
        nfsdiskless.swap_args.sotype = SOCK_DGRAM;
        nfsdiskless.swap_args.flags = (NFSMNT_WSIZE | NFSMNT_RSIZE);


		/* Find a server to get BOOTP reply from */
	if (!arptable[ARP_CLIENT].ipaddr || !arptable[ARP_SERVER].ipaddr) {
		printf("\r\nSearching for server...\r\n");
		if (!bootp()) {
			printf("No Server found.\r\n");
			longjmp(jmp_bootmenu,1);
		}
	}
	printf("My IP %I, Server IP %I, GW IP %I\r\n",
		arptable[ARP_CLIENT].ipaddr,
		arptable[ARP_SERVER].ipaddr,
		arptable[ARP_GATEWAY].ipaddr);

#ifdef MDEBUG
	printf("\n=>>"); getchar();
#endif

	/* Now use TFTP to load configuration file */
	sprintf(cfg,"/tftpboot/freebsd.%I",arptable[ARP_CLIENT].ipaddr);
	if (tftp(cfg) || tftp(cfg+10))
		goto cfg_done;
	cfg[17]='\0';
	if (tftp(cfg) || tftp(cfg+10))
		goto cfg_done;
	sprintf(cfg,"/tftpboot/cfg.%I",arptable[ARP_CLIENT].ipaddr);
	if (tftp(cfg) || tftp(cfg+10))
		goto cfg_done;
	sprintf(config_buffer,"rootfs %I:/usr/diskless_root",
		arptable[ARP_SERVER].ipaddr);
	printf("Unable to load config file, guessing:\r\n\t%s\r\n",
		config_buffer);

cfg_done:
#ifdef MDEBUG
	printf("\n=>>"); getchar();
#endif

	p = config_buffer;
	while(*p) {
		q = cmd_line;
		while ((*p != '\n') && (*p)) *(q++) = *(p++);
		*q = 0;
		printf("%s\r\n",cmd_line);
		execute(cmd_line);
		if (*p) p++;
	}

#ifdef MDEBUG
	printf("\n=>>"); getchar();
#endif

		/* Check to make sure we've got a rootfs */
	if (!arptable[ARP_ROOTSERVER].ipaddr) {
		printf("No ROOT filesystem server!\r\n");
		longjmp(jmp_bootmenu,1);
	}

		/* Fill in nfsdiskless.myif */
	sprintf(&nfsdiskless.myif.ifra_name,eth_driver);
        nfsdiskless.myif.ifra_addr.sa_len = sizeof(struct sockaddr);
        nfsdiskless.myif.ifra_addr.sa_family = AF_INET;
	addr = htonl(arptable[ARP_CLIENT].ipaddr);
	bcopy(&addr, &nfsdiskless.myif.ifra_addr.sa_data[2], 4);
	broadcast = (addr & netmask) | ~netmask;
	nfsdiskless.myif.ifra_broadaddr.sa_len = sizeof(struct sockaddr);
	nfsdiskless.myif.ifra_broadaddr.sa_family = AF_INET;
	bcopy(&broadcast, &nfsdiskless.myif.ifra_broadaddr.sa_data[2], 4);
	addr = htonl(arptable[ARP_GATEWAY].ipaddr);
	if (addr) {
		nfsdiskless.mygateway.sin_len = sizeof(struct sockaddr);
		nfsdiskless.mygateway.sin_family = AF_INET;
		bcopy(&addr, &nfsdiskless.mygateway.sin_addr, 4);
	} else {
		nfsdiskless.mygateway.sin_len = 0;
	}
	nfsdiskless.myif.ifra_mask.sa_len = sizeof(struct sockaddr);
	nfsdiskless.myif.ifra_mask.sa_family = AF_UNSPEC;
	bcopy(&netmask, &nfsdiskless.myif.ifra_mask.sa_data[2], 4);

	rpc_id = currticks();

		/* Lookup NFS/MOUNTD ports for SWAP using PORTMAP */
	if (arptable[ARP_SWAPSERVER].ipaddr) {
		char swapfs_fh[32], swapfile[32];
		swap_nfs_port = rpclookup(ARP_SWAPSERVER, PROG_NFS, 2);
		swap_mount_port = rpclookup(ARP_SWAPSERVER, PROG_MOUNT, 1);
		if ((swap_nfs_port == -1) || (swap_mount_port == -1)) {
			printf("Unable to get SWAP NFS/MOUNT ports\r\n");
			longjmp(jmp_bootmenu,1);
		}
		if (err = nfs_mount(ARP_SWAPSERVER, swap_mount_port,
			nfsdiskless.swap_hostnam, &swapfs_fh)) {
			printf("Unable to mount SWAP filesystem: ");
			nfs_err(err);
			longjmp(jmp_bootmenu,1);
		}
		sprintf(swapfile,"swap.%I",arptable[ARP_CLIENT].ipaddr);
		if (err = nfs_lookup(ARP_SWAPSERVER, swap_nfs_port,
			&swapfs_fh, swapfile, &nfsdiskless.swap_fh)) {
			printf("Unable to open %s: ",swapfile);
			nfs_err(err);
			longjmp(jmp_bootmenu,1);
		}
		nfsdiskless.swap_saddr.sin_len = sizeof(struct sockaddr_in);
		nfsdiskless.swap_saddr.sin_family = AF_INET;
		nfsdiskless.swap_saddr.sin_port = htons(swap_nfs_port);
		nfsdiskless.swap_saddr.sin_addr.s_addr =
			htonl(arptable[ARP_SWAPSERVER].ipaddr);
        	nfsdiskless.swap_args.timeo = 10;
        	nfsdiskless.swap_args.retrans = 100;
	}

		/* Lookup NFS/MOUNTD ports for ROOT using PORTMAP */
	root_nfs_port = rpclookup(ARP_ROOTSERVER, PROG_NFS, 2);
	root_mount_port = rpclookup(ARP_ROOTSERVER, PROG_MOUNT, 1);
	if ((root_nfs_port == -1) || (root_mount_port == -1)) {
		printf("Unable to get ROOT NFS/MOUNT ports\r\n");
		longjmp(jmp_bootmenu,1);
	}
	if (err = nfs_mount(ARP_ROOTSERVER, root_mount_port,
		nfsdiskless.root_hostnam, &nfsdiskless.root_fh)) {
		printf("Unable to mount ROOT filesystem: ");
		nfs_err(err);
		longjmp(jmp_bootmenu,1);
	}
	nfsdiskless.root_saddr.sin_len = sizeof(struct sockaddr_in);
	nfsdiskless.root_saddr.sin_family = AF_INET;
	nfsdiskless.root_saddr.sin_port = htons(root_nfs_port);
	nfsdiskless.root_saddr.sin_addr.s_addr =
		htonl(arptable[ARP_ROOTSERVER].ipaddr);
        nfsdiskless.root_args.timeo = 10;
        nfsdiskless.root_args.retrans = 100;
	nfsdiskless.root_time = 0;

	if (err = nfs_lookup(ARP_ROOTSERVER, root_nfs_port,
		&nfsdiskless.root_fh, *kernel == '/' ? kernel+1 : kernel,
		&kernel_handle)) {
		printf("Unable to open %s: ",kernel);
		nfs_err(err);
		longjmp(jmp_bootmenu,1);
	}

		/* Load the kernel using NFS */
	printf("Loading %s...\r\n",kernel);
	if ((err = nfs_read(ARP_ROOTSERVER, root_nfs_port, &kernel_handle, 0,
		sizeof(struct exec), &head)) < 0) {
		printf("Unable to read %s: ",kernel);
		nfs_err(err);
		longjmp(jmp_bootmenu,1);
	}
	if (N_BADMAG(head)) {
		printf("Bad executable format!\r\n");
		longjmp(jmp_bootmenu, 1);
	}
	loadpoint = (char *)0x100000;
	offset = N_TXTOFF(head);
	printf("text=0x%X, ",head.a_text);
	nfsload(head.a_text);
	while (((int)loadpoint) & PAGE_MASK)
		*(loadpoint++) = 0;

	printf("data=0x%X, ",head.a_data);
	nfsload(head.a_data);

	printf("bss=0x%X, ",head.a_bss);
	while(head.a_bss--) *(loadpoint++) = 0;

	while (((int)loadpoint) & PAGE_MASK)
		*(loadpoint++) = 0;

	bootinfo.bi_symtab = (int) loadpoint;

	p = (char*)&head.a_syms;
	for (i=0;i<sizeof(head.a_syms);i++)
		*loadpoint++ = *p++;

	printf("symbols=[+0x%x+0x%x", sizeof(head.a_syms), head.a_syms);
	
	nfsload(head.a_syms);
	i = sizeof(int);
	p = loadpoint;
	nfsload(i);
	i = *(int*)p;
	printf("+0x%x]\n", i);
	i -= sizeof(int);
	nfsload(i);
	bootinfo.bi_esymtab = (int) loadpoint;

	printf("entry=0x%X.\n\r",head.a_entry);

		/* Jump to kernel */
	bootinfo.bi_version = BOOTINFO_VERSION;
	bootinfo.bi_kernelname = kernel;
	bootinfo.bi_nfs_diskless = &nfsdiskless;
	bootinfo.bi_size = sizeof bootinfo;
	kernelentry = (void *)(head.a_entry & 0x00FFFFFF);
	(*kernelentry)(RB_BOOTINFO,NODEV,0,0,0,&bootinfo,0,0,0);
	printf("*** %s execute failure ***\n",kernel);
}

/**************************************************************************
POLLKBD - Check for Interrupt from keyboard
**************************************************************************/
pollkbd()
{
	if (iskey() && (getchar() == ESC)) longjmp(jmp_bootmenu,1);
}

/**************************************************************************
DEFAULT_NETMASK - Set a default netmask for IP address
**************************************************************************/
default_netmask()
{
	int net = arptable[ARP_CLIENT].ipaddr >> 24;
	if (net <= 127)
		netmask = htonl(0xff000000);
	else if (net < 192)
		netmask = htonl(0xffff0000);
	else
		netmask = htonl(0xffffff00);
}
/**************************************************************************
UDP_TRANSMIT - Send a UDP datagram
**************************************************************************/
udp_transmit(destip, srcsock, destsock, len, buf)
	unsigned long destip;
	unsigned short srcsock, destsock;
	int len;
	char *buf;
{
	struct iphdr *ip;
	struct udphdr *udp;
	struct arprequest arpreq;
	int arpentry, i;
	unsigned long time;
	int retry = MAX_ARP_RETRIES;

	ip = (struct iphdr *)buf;
	udp = (struct udphdr *)(buf + sizeof(struct iphdr));
	ip->verhdrlen = 0x45;
	ip->service = 0;
	ip->len = htons(len);
	ip->ident = 0;
	ip->frags = 0;
	ip->ttl = 60;
	ip->protocol = IP_UDP;
	ip->chksum = 0;
	convert_ipaddr(ip->src, &arptable[ARP_CLIENT].ipaddr);
	convert_ipaddr(ip->dest, &destip);
	ip->chksum = ipchksum(buf, sizeof(struct iphdr));
	udp->src = htons(srcsock);
	udp->dest = htons(destsock);
	udp->len = htons(len - sizeof(struct iphdr));
	udp->chksum = 0;
	if (destip == IP_BROADCAST) {
		eth_transmit(broadcast, IP, len, buf);
	} else {
		long h_netmask = ntohl(netmask);
				/* Check to see if we need gateway */
		if (((destip & h_netmask) !=
			(arptable[ARP_CLIENT].ipaddr & h_netmask)) &&
			arptable[ARP_GATEWAY].ipaddr)
				destip = arptable[ARP_GATEWAY].ipaddr;
		for(arpentry = 0; arpentry<MAX_ARP; arpentry++)
			if (arptable[arpentry].ipaddr == destip) break;
		if (arpentry == MAX_ARP) {
			printf("%I is not in my arp table!\n");
			return(0);
		}
		for (i = 0; i<ETHER_ADDR_SIZE; i++)
			if (arptable[arpentry].node[i]) break;
		if (i == ETHER_ADDR_SIZE) {	/* Need to do arp request */
			arpreq.hwtype = htons(1);
			arpreq.protocol = htons(IP);
			arpreq.hwlen = ETHER_ADDR_SIZE;
			arpreq.protolen = 4;
			arpreq.opcode = htons(ARP_REQUEST);
			bcopy(arptable[ARP_CLIENT].node, arpreq.shwaddr,
				ETHER_ADDR_SIZE);
			convert_ipaddr(arpreq.sipaddr,
				&arptable[ARP_CLIENT].ipaddr);
			bzero(arpreq.thwaddr, ETHER_ADDR_SIZE);
			convert_ipaddr(arpreq.tipaddr, &destip);
			while (retry--) {
				eth_transmit(broadcast, ARP, sizeof(arpreq),
					&arpreq);
				if (await_reply(AWAIT_ARP, arpentry,
					arpreq.tipaddr)) goto xmit;
			}
			return(0);
		}
xmit:		eth_transmit(arptable[arpentry].node, IP, len, buf);
	}
	return(1);
}

/**************************************************************************
TFTP - Try to load configuation file
**************************************************************************/
tftp(name)
	char *name;
{
	struct	tftp_t	*tr;
	int retry = MAX_TFTP_RETRIES;
	static unsigned short isocket = 2000;
	unsigned short osocket = TFTP;
	unsigned short len, block=1;
	struct tftp_t tp;
	int code;
	printf("Loading %s...\r\n",name);
	isocket++;
	tp.opcode = htons(TFTP_RRQ);
	len = (sprintf((char *)tp.u.rrq,"%s%c%s",name,0,"octet")
		- ((char *)&tp)) + 1;
	while(retry--) {
		if (!udp_transmit(arptable[ARP_SERVER].ipaddr, isocket, osocket,
			len, &tp)) return(0);
		if (await_reply(AWAIT_TFTP, isocket, NULL)) {
			tr = (struct tftp_t *)&packet[ETHER_HDR_SIZE];
			if (tr->opcode == ntohs(TFTP_ERROR)) {
				printf("TFTP error %d (%s)\r\n",
					ntohs(tr->u.err.errcode),
					tr->u.err.errmsg);
				return(0);
			}			/* ACK PACKET */
			if (tr->opcode != ntohs(TFTP_DATA)) return(0);
			tp.opcode = htons(TFTP_ACK);
			tp.u.ack.block = tr->u.data.block;
			udp_transmit(arptable[ARP_SERVER].ipaddr, isocket,
				osocket, TFTP_MIN_PACKET_SIZE, &tp);
			len = ntohs(tr->udp.len) - sizeof(struct udphdr) - 4;
			if (len >= 512) {
				printf("Config file too large.\r\n");
				config_buffer[0] = 0;
				return(0);
			} else {
				bcopy(tr->u.data.download, config_buffer, len);
				config_buffer[len] = 0;
			}
			return(1);
		}
	}
	return(0);
}

/**************************************************************************
BOOTP - Get my IP address and load information
**************************************************************************/
bootp()
{
	int retry = MAX_BOOTP_RETRIES;
	struct bootp_t bp;
	unsigned long  starttime;
	bzero(&bp, sizeof(struct bootp_t));
	bp.bp_op = BOOTP_REQUEST;
	bp.bp_htype = 1;
	bp.bp_hlen = ETHER_ADDR_SIZE;
	bp.bp_xid = starttime = currticks();
	bcopy(arptable[ARP_CLIENT].node, bp.bp_hwaddr, ETHER_ADDR_SIZE);
	while(retry--) {
		udp_transmit(IP_BROADCAST, 0, BOOTP_SERVER,
			sizeof(struct bootp_t), &bp);
		if (await_reply(AWAIT_BOOTP, 0, NULL))
			return(1);
		bp.bp_secs = htons((currticks()-starttime)/20);
	}
	return(0);
}


/**************************************************************************
AWAIT_REPLY - Wait until we get a response for our request
**************************************************************************/
await_reply(type, ival, ptr)
	int type, ival;
	char *ptr;
{
	unsigned long time;
	struct	iphdr *ip;
	struct	udphdr *udp;
	struct	arprequest *arpreply;
	struct	bootp_t *bootpreply;
	struct	rpc_t *rpc;

	int	protohdrlen = ETHER_HDR_SIZE + sizeof(struct iphdr) +
				sizeof(struct udphdr);
	time = currticks() + TIMEOUT;
	while(time > currticks()) {
		pollkbd();
		if (eth_poll()) {	/* We have something! */
					/* Check for ARP - No IP hdr */
			if ((type == AWAIT_ARP) &&
			   (packetlen >= ETHER_HDR_SIZE +
				sizeof(struct arprequest)) &&
			   (((packet[12] << 8) | packet[13]) == ARP)) {
				arpreply = (struct arprequest *)
					&packet[ETHER_HDR_SIZE];
				if ((arpreply->opcode == ntohs(ARP_REPLY)) &&
				   bcompare(arpreply->sipaddr, ptr, 4)) {
					bcopy(arpreply->shwaddr,
						arptable[ival].node,
						ETHER_ADDR_SIZE);
					return(1);
				}
				continue;
			}

					/* Anything else has IP header */
			if ((packetlen < protohdrlen) ||
			   (((packet[12] << 8) | packet[13]) != IP)) continue;
			ip = (struct iphdr *)&packet[ETHER_HDR_SIZE];
			if ((ip->verhdrlen != 0x45) ||
				ipchksum(ip, sizeof(struct iphdr)) ||
				(ip->protocol != IP_UDP)) continue;
			udp = (struct udphdr *)&packet[ETHER_HDR_SIZE +
				sizeof(struct iphdr)];

					/* BOOTP ? */
			bootpreply = (struct bootp_t *)&packet[ETHER_HDR_SIZE];
			if ((type == AWAIT_BOOTP) &&
			   (packetlen >= (ETHER_HDR_SIZE +
			     sizeof(struct bootp_t))) &&
			   (ntohs(udp->dest) == BOOTP_CLIENT) &&
			   (bootpreply->bp_op == BOOTP_REPLY)) {
				convert_ipaddr(&arptable[ARP_CLIENT].ipaddr,
					bootpreply->bp_yiaddr);
				default_netmask();
				convert_ipaddr(&arptable[ARP_SERVER].ipaddr,
					bootpreply->bp_siaddr);
				bzero(arptable[ARP_SERVER].node,
					ETHER_ADDR_SIZE);  /* Kill arp */
				convert_ipaddr(&arptable[ARP_GATEWAY].ipaddr,
					bootpreply->bp_giaddr);
				bzero(arptable[ARP_GATEWAY].node,
					ETHER_ADDR_SIZE);  /* Kill arp */
				if (bootpreply->bp_file[0]) {
					bcopy(bootpreply->bp_file,
						kernel_buf, 128);
					kernel = kernel_buf;
				}
				decode_rfc1048(bootpreply->bp_vend);
				return(1);
			}

					/* TFTP ? */
			if ((type == AWAIT_TFTP) &&
				(ntohs(udp->dest) == ival)) return(1);

					/* RPC */
			rpc = (struct rpc_t *)&packet[ETHER_HDR_SIZE];
			if ((type == AWAIT_RPC) &&
			   (ntohs(udp->dest) == RPC_SOCKET) &&
			   (ntohl(rpc->u.reply.id) == ival) &&
			   (ntohl(rpc->u.reply.type) == MSG_REPLY)) {
				rpc_id++;
				return(1);
			}
		}
	}
	return(0);
}

/**************************************************************************
DECODE_RFC1048 - Decodes RFC1048 header
**************************************************************************/
decode_rfc1048(p)
	unsigned char *p;
{
	static char rfc1048_cookie[4] = RFC1048_COOKIE;
	unsigned char *end = p + BOOTP_VENDOR_LEN,*q;
	if (bcompare(p, rfc1048_cookie, 4)) { /* RFC 1048 header */
		p += 4;
		while(p < end) {
			switch (*p) {
			case RFC1048_PAD:
				p++;
				continue;
			case RFC1048_END:
				p = end;
				continue;
			case RFC1048_GATEWAY:
				convert_ipaddr(&arptable[ARP_GATEWAY].ipaddr,
					p+2);
				break;
			case RFC1048_NETMASK:
				bcopy(p+2,&netmask,4);
				break;
			case RFC1048_HOSTNAME:
				bcopy(p+2, &nfsdiskless.my_hostnam, TAG_LEN(p));
				hostnamelen = (TAG_LEN(p) + 3) & ~3;
				break;
			default:
				printf("Unknown RFC1048-tag ");
				for(q=p;q<p+2+TAG_LEN(p);q++)
					printf("%x ",*q);
				printf("\n\r");
			}
			p += TAG_LEN(p) + 2;
		}
	}
}

/**************************************************************************
IPCHKSUM - Checksum IP Header
**************************************************************************/
ipchksum(ip, len)
	unsigned short *ip;
	int len;
{
	unsigned long sum = 0;
	len >>= 1;
	while (len--) {
		sum += *(ip++);
		if (sum > 0xFFFF)
			sum -= 0xFFFF;
	}
	return((~sum) & 0x0000FFFF);
}


/**************************************************************************
CONVERT_IPADDR - Convert IP address from net to machine order
**************************************************************************/
convert_ipaddr(d, s)
	char *d,*s;
{
	*(d+3) = *s;
	*(d+2) = *(s+1);
	*(d+1) = *(s+2);
	*d     = *(s+3);
}
