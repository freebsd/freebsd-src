/**************************************************************************
NETBOOT -  BOOTP/TFTP Bootstrap Program

Author: Martin Renters
  Date: Dec/93

**************************************************************************/

#include "netboot.h"

#define ESC	0x1b		/* ESC Key */

int jmp_bootmenu[10];
struct exec head;
int txtoff;
void (*kernelentry)();
char *loadpoint;
char *bootfile;
char bootname[128];
char cfg[32];
char broadcast[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
struct nfs_diskless nfsdiskless;
extern char packet[];
extern int packetlen;
char *putdec();

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
	printf("\r\nBOOTP/TFTP bootstrap loader    ESC for menu\n\r");
	printf("\r\nSearching for adapter...");
	if (!eth_probe()) {
		printf("No adapter found.\r\n");
		exit(0);
	}
	bootfile = DEFAULT_BOOTFILE;
	while (1) {
		if (setjmp(jmp_bootmenu))
			bootmenu();
		else
			load();
	}
}

/**************************************************************************
LOAD - Try to get booted
**************************************************************************/
load()
{
	char *p;
	if (!arptable[ARP_CLIENT].ipaddr || !arptable[ARP_SERVER].ipaddr) {
		printf("\r\nSearching for server...\r\n");
		if (!bootp()) {
			printf("No Server found.\r\n");
			longjmp(jmp_bootmenu,1);
		}
	}
	printf("station IP %I, server IP %I\r\n",
		arptable[ARP_CLIENT].ipaddr,
		arptable[ARP_SERVER].ipaddr);
	p = cfg;
	*(p++) = 'c';
	*(p++) = 'f';
	*(p++) = 'g';
	*(p++) = '.';
	p = putdec(p, arptable[ARP_CLIENT].ipaddr>>24);
	*(p++) = '.';
	p = putdec(p, arptable[ARP_CLIENT].ipaddr>>16);
	*(p++) = '.';
	p = putdec(p, arptable[ARP_CLIENT].ipaddr>>8);
	*(p++) = '.';
	p = putdec(p, arptable[ARP_CLIENT].ipaddr);
	*p = 0;
	printf("Loading %s...\r\n",cfg);
	if (!tftp(cfg, TFTP_CODE_CFG)) {
		printf("Unable to load config file.\r\n");
		longjmp(jmp_bootmenu,1);
	}
	printf("Loading %s...\r\n",bootfile);
	if (!tftp(bootfile, TFTP_CODE_BOOT)) {
		printf("Unable to load boot file.\r\n");
		longjmp(jmp_bootmenu,1);
	}
	if (!(head.a_entry & 0x00F00000)) {	/* < 1MB kernel? */
		printf("<1MB kernel. Relocating\r\n");
		bcopy(0x100000, 0, 0x400);	/* Relocate */
		bcopy(0x100500, 0x500, ((int)loadpoint) - 0x100500);
	}
	kernelentry = (void *)(head.a_entry & 0x00FFFFFF);
	(*kernelentry)(0,0,0,0,&nfsdiskless,0,0,0);
}

/**************************************************************************
POLLKBD - Check for Interrupt from keyboard
**************************************************************************/
pollkbd()
{
	if (iskey() && (getchar() == ESC)) longjmp(jmp_bootmenu,1);
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
	struct arprequest arpreq, *arpreply;
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
		for(arpentry = 0; arpentry<MAX_ARP; arpentry++)
			if (arptable[arpentry].ipaddr == destip) break;
		if (arpentry == MAX_ARP) {
			printf("%I is not in my arp table!\n");
			return(0);
		}
		for (i = 0; i<ETHER_ADDR_SIZE; i++)
			if (arptable[arpentry].node[i]) break;
		if (i == ETHER_ADDR_SIZE) {		/* Need to do arp request */
			arpreq.hwtype = htons(1);
			arpreq.protocol = htons(IP);
			arpreq.hwlen = ETHER_ADDR_SIZE;
			arpreq.protolen = 4;
			arpreq.opcode = htons(ARP_REQUEST);
			bcopy(arptable[ARP_CLIENT].node, arpreq.shwaddr, ETHER_ADDR_SIZE);
			convert_ipaddr(arpreq.sipaddr, &arptable[ARP_CLIENT].ipaddr);
			bzero(arpreq.thwaddr, ETHER_ADDR_SIZE);
			convert_ipaddr(arpreq.tipaddr, &destip);
			while (retry--) {
				eth_transmit(broadcast, ARP, sizeof(arpreq), &arpreq);
				time = currticks() + TIMEOUT;
				while (time > currticks()) {
					pollkbd();
					if (eth_poll() && (packetlen >= ETHER_HDR_SIZE + sizeof(arpreq)) &&
						(((packet[12] << 8) | packet[13]) == ARP)) {
						arpreply = (struct arprequest *)&packet[ETHER_HDR_SIZE];
						if ((arpreply->opcode == ntohs(ARP_REPLY)) &&
							bcompare(arpreply->sipaddr,arpreq.tipaddr, 4)) {
							bcopy(arpreply->shwaddr, arptable[arpentry].node,ETHER_ADDR_SIZE);
							goto xmit;
						}
					}
				}
			}
			return(0);
		}
xmit:		eth_transmit(arptable[arpentry].node, IP, len, buf);
	}
	return(1);
}

/**************************************************************************
TFTP - Try to load something
**************************************************************************/
tftp(name, type)
	char *name;
	int type;
{
	int retry = MAX_TFTP_RETRIES;
	static unsigned short isocket = 2000;
	unsigned short osocket = TFTP;
	unsigned short len, block=1;
	struct tftp_t tp;
	unsigned long time;
	int code;
	char *p = tp.u.rrq;
	isocket++;
	tp.opcode = htons(TFTP_RRQ);
	while (*name) *(p++) = *(name++);
	*(p++) = 0;
	*(p++) = 'o';
	*(p++) = 'c';
	*(p++) = 't';
	*(p++) = 'e';
	*(p++) = 't';
	*(p++) = 0;
	len = p - (char *)&tp;
	while(retry--) {
		if (!udp_transmit(arptable[ARP_SERVER].ipaddr, isocket, osocket,
			len, &tp)) return(0);
next:		time = currticks() + TIMEOUT;
		while(time > currticks()) {
			pollkbd();
			if (eth_poll()) {
				code = tftp_data(&tp, &block, isocket, &osocket,
					type);
				if (!code) continue;
				if (code == TFTP_CODE_EOF) return(1);
				if (code == TFTP_CODE_ERROR) return(0);
				len = TFTP_MIN_PACKET_SIZE;
				retry = MAX_TFTP_RETRIES;
				goto next;
			}
		}
	}
	return(0);
}

/**************************************************************************
TFTP_DATA - Check and handle incoming TFTP packets
**************************************************************************/
tftp_data(req, block, isocket, osocket, type)
	struct tftp_t *req;
	unsigned short *block, isocket, *osocket;
	int type;
{
	struct tftp_t *tp;
	char *p;
	int len;
	if (!chkpacket(TFTP_MIN_PACKET_SIZE, isocket)) return(0);
	tp = (struct tftp_t *)&packet[ETHER_HDR_SIZE];
	if (tp->opcode == ntohs(TFTP_ERROR)) {
		printf("TFTP error %d (%s)\r\n", ntohs(tp->u.err.errcode),
			tp->u.err.errmsg);
		longjmp(jmp_bootmenu, 1);
	}
	if (tp->opcode != htons(TFTP_DATA)) return(0);
	len = ntohs(tp->udp.len) - sizeof(struct udphdr) -
		 (2*sizeof(unsigned short));
	req->opcode = htons(TFTP_ACK);		/* Send ack */
	req->u.ack.block = tp->u.data.block;
	udp_transmit(arptable[ARP_SERVER].ipaddr, isocket, *osocket,
		TFTP_MIN_PACKET_SIZE, req);
	if (*block != ntohs(tp->u.data.block)) return(TFTP_CODE_MORE);
	if (*block == 1) {
		*osocket = htons(tp->udp.src);
		if ((type == TFTP_CODE_CFG) &&
		 (len == sizeof(struct nfs_diskless))) {
			bcopy(tp->u.data.download, &nfsdiskless, sizeof(struct nfs_diskless));
			return(TFTP_CODE_EOF);
		}
		bcopy(tp->u.data.download, &head, sizeof(struct exec));
		if ((type == TFTP_CODE_BOOT) &&
		 ((len < sizeof(struct exec)) || (N_BADMAG(head)))) {
			printf("Not an executable.\r\n");
			return(TFTP_CODE_ERROR);
		} else {
			if (((head.a_entry & 0x00FFFFFF) == 0) &&
				(head.a_text + head.a_data >= RELOC)) {
				printf("Executable too large.\r\n");
				return(TFTP_CODE_ERROR);
			}
				/* We load above 1 mb so we don't clobber DOS */
			loadpoint = (char *)0x100000;
			printf("text=0x%X", head.a_text);
		}
		txtoff = N_TXTOFF(head);
	}
	p = tp->u.data.download;
	*block +=1;
	while (len--) {
		if (txtoff) {
			txtoff--;
			p++;
			continue;
		}
		if (head.a_text) {
			*(loadpoint++) = *(p++);
			head.a_text--;
			if (!head.a_text) {
				printf(", data=0x%X",head.a_data);
				while (((int)loadpoint) & CLOFSET)
					*(loadpoint++) = 0;
			}
			continue;
		}
		if (head.a_data) {
			*(loadpoint++) = *(p++);
			head.a_data--;
			if (!head.a_data) {
				printf(", bss=0x%X\r\n",head.a_bss);
				return(TFTP_CODE_EOF);
			}
			
		}
	}
	return((head.a_text || head.a_data) ? TFTP_CODE_MORE : TFTP_CODE_EOF);
}

/**************************************************************************
BOOTP - Get my IP address and load information
**************************************************************************/
bootp()
{
	int retry = MAX_BOOTP_RETRIES;
	struct bootp_t bp;
	struct bootp_t *reply;
	unsigned long time, starttime;
	bzero(&bp, sizeof(struct bootp_t));
	bp.bp_op = BOOTP_REQUEST;
	bp.bp_htype = 1;
	bp.bp_hlen = ETHER_ADDR_SIZE;
	bp.bp_xid = starttime = currticks();
	bcopy(arptable[ARP_CLIENT].node, bp.bp_hwaddr, ETHER_ADDR_SIZE);
	while(retry--) {
		udp_transmit(IP_BROADCAST, 0, BOOTP_SERVER,
			sizeof(struct bootp_t), &bp);
		time = currticks() + TIMEOUT;
		while(time > currticks()) {
			pollkbd();
			if (eth_poll()) {	/* We have something! */
				reply = (struct bootp_t *)&packet[ETHER_HDR_SIZE];
				if (((!chkpacket(sizeof(struct bootp_t),
					BOOTP_CLIENT))) || (reply->bp_op != BOOTP_REPLY))
					continue;
				convert_ipaddr(&arptable[ARP_CLIENT].ipaddr,
					reply->bp_yiaddr);
				convert_ipaddr(&arptable[ARP_SERVER].ipaddr,
					reply->bp_siaddr);
				bzero(arptable[ARP_SERVER].node, ETHER_ADDR_SIZE);  /* Kill arp */
				if (reply->bp_file[0]) {
					bcopy(reply->bp_file, bootname, 128);
					bootfile = bootname;
				}
				return(1);
			}
		}
		bp.bp_secs = htons((currticks()-starttime)/20);
	}
	return(0);
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
CHKPACKET - Quick check to see if incoming packet is good
**************************************************************************/
chkpacket(size, type)
	int size, type;
{
	struct iphdr *ip;
	struct udphdr *udp;
	if (packetlen < (ETHER_HDR_SIZE + size)) return(0);
	if (((packet[12] << 8) | packet[13]) != IP) return(0);
	ip = (struct iphdr *)&packet[ETHER_HDR_SIZE];
	if (ip->verhdrlen != 0x45) return(0);
	if (ipchksum(ip, sizeof(struct iphdr))) return(0);
	if (ip->protocol != IP_UDP) return(0);
	udp = (struct udphdr *)&packet[ETHER_HDR_SIZE + sizeof(struct iphdr)];
	if (ntohs(udp->dest) != type) return(0);
	return(1);
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
