/**************************************************************************
NETBOOT -  BOOTP/TFTP Bootstrap Program

Author: Martin Renters
  Date: Dec/93

**************************************************************************/
#include "netboot.h"

extern struct nfs_diskless nfsdiskless;
extern int hostnamelen;
extern unsigned long netmask;
extern eth_reset();
extern short aui;

int cmd_ip(), cmd_server(), cmd_kernel(), cmd_help(), exit();
int cmd_rootfs(), cmd_swapfs(), cmd_interface(), cmd_hostname();
int cmd_netmask(), cmd_swapsize(), cmd_swapopts(), cmd_rootopts();
int cmd_aui(), cmd_gateway();

struct bootcmds_t {
	char *name;
	int (*func)();
	char *help;
} bootcmds[] = {
	{"?",		cmd_help,	"                 this list"},
	{"help",	cmd_help,	"              this list"},
	{"ip",		cmd_ip,		"<addr>          set my IP addr"},
	{"server",	cmd_server,	"<addr>      set TFTP server IP addr"},
	{"gateway",	cmd_gateway,	"<addr>      set default router"},
	{"netmask",	cmd_netmask,	"<addr>     set network mask"},
	{"hostname",	cmd_hostname,	"<name>    set hostname"},
	{"kernel",	cmd_kernel,	"<file>      set boot filename"},
	{"rootfs",	cmd_rootfs,	"ip:/fs      set root filesystem"},
	{"swapfs",	cmd_swapfs,	"ip:/fs      set swap filesystem"},
	{"swapsize",	cmd_swapsize,	"<nblks>   set swap size"},
	{"swapopts",	cmd_swapopts,	"<options> swap mount options"},
	{"rootopts",	cmd_rootopts,	"<options> root mount options"},
	{"diskboot",	exit,		"          boot from disk"},
	{"autoboot",	NULL,		"          continue"},
        {"trans",       cmd_aui,        "<on|off>     turn transceiver on|off"},
	{NULL,		NULL,		NULL}
};

/**************************************************************************
CMD_HELP - Display help screen
**************************************************************************/
cmd_help()
{
	struct bootcmds_t *cmd = bootcmds;
	printf("\r\n");
	while (cmd->name) {
		printf("%s %s\n\r",cmd->name,cmd->help);
		cmd++;
	}
}

/**************************************************************************
CMD_IP - Set my IP address
**************************************************************************/
cmd_ip(p)
	char *p;
{
	int i;
	if (!setip(p, &arptable[ARP_CLIENT].ipaddr)) {
		printf("IP address is %I\r\n",
			arptable[ARP_CLIENT].ipaddr);
	} else default_netmask();
}

/**************************************************************************
CMD_AUI - Turn on-board transceiver on or off
**************************************************************************/
cmd_aui(p)
        char *p;
{
        if (*(p+1) == 'f') {
                aui = 1;
                eth_reset();
                return(0);
        }
        if (*(p+1) == 'n') {
                aui = 0;
                eth_reset();
                return(0);
        }
        printf ("Transceiver is %s\r\n",aui ? "off" : "on");
}

/**************************************************************************
CMD_GATEWAY - Set routers IP address
**************************************************************************/
cmd_gateway(p)
	char *p;
{
	int i;
	if (!setip(p, &arptable[ARP_GATEWAY].ipaddr)) {
		printf("Server IP address is %I\r\n",
			arptable[ARP_GATEWAY].ipaddr);
	} else		/* Need to clear arp entry if we change IP address */
		for (i=0; i<6; i++) arptable[ARP_GATEWAY].node[i] = 0;
}

/**************************************************************************
CMD_SERVER - Set server's IP address
**************************************************************************/
cmd_server(p)
	char *p;
{
	int i;
	if (!setip(p, &arptable[ARP_SERVER].ipaddr)) {
		printf("Server IP address is %I\r\n",
			arptable[ARP_SERVER].ipaddr);
	} else		/* Need to clear arp entry if we change IP address */
		for (i=0; i<6; i++) arptable[ARP_SERVER].node[i] = 0;
}

/**************************************************************************
CMD_NETMASK - Set network mask
**************************************************************************/
cmd_netmask(p)
	char *p;
{
	int i;
	if (!setip(p, &netmask)) {
		netmask = ntohl(netmask);
		printf("netmask is %I\r\n", netmask);
	}
	netmask = htonl(netmask);
}

/**************************************************************************
CMD_SWAPSIZE - Set number of blocks for swap
**************************************************************************/
cmd_swapsize(p)
	char *p;
{
	int blks = getdec(&p);
	if (blks > 0) nfsdiskless.swap_nblks = blks;
	else printf("Swap size is: %d blocks\r\n",nfsdiskless.swap_nblks);
}

extern char kernel_buf[], *kernel;
/**************************************************************************
CMD_KERNEL - set kernel filename
**************************************************************************/
cmd_kernel(p)
	char *p;
{
	if (*p) sprintf(kernel = kernel_buf,"%s",p);
	printf("Bootfile is: %s\r\n", kernel);
}


/**************************************************************************
CMD_ROOTFS - Set root filesystem name
**************************************************************************/
cmd_rootfs(p)
	char *p;
{
	if (!setip(p, &arptable[ARP_ROOTSERVER].ipaddr)) {
		printf("Root filesystem is %I:%s\r\n",
			nfsdiskless.root_saddr.sin_addr,
			nfsdiskless.root_hostnam);
	} else {
		bcopy(&arptable[ARP_ROOTSERVER].ipaddr,
			&nfsdiskless.root_saddr.sin_addr, 4);
		while (*p && (*p != ':')) p++;
		if (*p == ':') p++;
		sprintf(&nfsdiskless.root_hostnam, "%s", p);
	}
}

/**************************************************************************
CMD_SWAPFS - Set swap filesystem name
**************************************************************************/
cmd_swapfs(p)
	char *p;
{
	if (!setip(p, &arptable[ARP_SWAPSERVER].ipaddr)) {
		printf("Swap filesystem is %I:%s\r\n",
			nfsdiskless.swap_saddr.sin_addr,
			nfsdiskless.swap_hostnam);
	} else {
		bcopy(&arptable[ARP_SWAPSERVER].ipaddr,
			&nfsdiskless.swap_saddr.sin_addr, 4);
		while (*p && (*p != ':')) p++;
		if (*p == ':') p++;
		sprintf(&nfsdiskless.swap_hostnam, "%s", p);
	}
}

/**************************************************************************
CMD_HOSTNAME - Set my hostname
**************************************************************************/
cmd_hostname(p)
	char *p;
{
	if (*p)
		hostnamelen = ((sprintf(&nfsdiskless.my_hostnam,"%s",p) -
			(char*)&nfsdiskless.my_hostnam) + 3) & ~3;
	else	printf("Hostname is: %s\r\n",nfsdiskless.my_hostnam);
}
/**************************************************************************
CMD_ROOTOPTS - Set root mount options
**************************************************************************/
cmd_rootopts(p)
        char *p;
{
        char *tmp;

        if (*p) {
                nfsdiskless.root_args.flags = NFSMNT_RSIZE | NFSMNT_WSIZE;
                nfsdiskless.root_args.sotype = SOCK_DGRAM;
                if ((tmp = (char *)substr(p,"rsize=")))
                        nfsdiskless.root_args.rsize=getdec(&tmp);
                if ((tmp = (char *)substr(p,"wsize=")))
                        nfsdiskless.root_args.wsize=getdec(&tmp);
                if ((tmp = (char *)substr(p,"resvport")))
                        nfsdiskless.root_args.flags |= NFSMNT_RESVPORT;
                if ((tmp = (char *)substr(p,"intr")))
                        nfsdiskless.root_args.flags |= NFSMNT_INT;
                if ((tmp = (char *)substr(p,"soft")))
                        nfsdiskless.root_args.flags |= NFSMNT_SOFT;
                if ((tmp = (char *)substr(p, "tcp")))
                         nfsdiskless.root_args.sotype = SOCK_STREAM;
        } else {
                printf("Rootfs mount options: rsize=%d,wsize=%d",
                nfsdiskless.root_args.rsize,
                nfsdiskless.root_args.wsize);
                if (nfsdiskless.root_args.flags & NFSMNT_RESVPORT)
                        printf (",resvport");
                if (nfsdiskless.root_args.flags & NFSMNT_SOFT)
                        printf (",soft");
                if (nfsdiskless.root_args.flags & NFSMNT_INT)
                        printf (",intr");
                if (nfsdiskless.root_args.sotype == SOCK_STREAM)
                        printf (",tcp");
                else
                        printf (",udp");
                printf ("\r\n");
        }
}

/**************************************************************************
CMD_SWAPOPTS - Set swap mount options
**************************************************************************/
cmd_swapopts(p)
        char *p;
{
	char *tmp;

	if (*p) {
                nfsdiskless.swap_args.flags = NFSMNT_RSIZE | NFSMNT_WSIZE;
                nfsdiskless.swap_args.sotype = SOCK_DGRAM;
		if ((tmp = (char *)substr(p,"rsize=")))
			nfsdiskless.swap_args.rsize=getdec(&tmp);
		if ((tmp = (char *)substr(p,"wsize=")))
			nfsdiskless.swap_args.wsize=getdec(&tmp);
		if ((tmp = (char *)substr(p,"resvport")))
			nfsdiskless.swap_args.flags |= NFSMNT_RESVPORT;
		if ((tmp = (char *)substr(p,"intr")))
			nfsdiskless.swap_args.flags |= NFSMNT_INT;
		if ((tmp = (char *)substr(p,"soft")))
			nfsdiskless.swap_args.flags |= NFSMNT_SOFT;
		if ((tmp = (char *)substr(p, "tcp")))
			 nfsdiskless.swap_args.sotype = SOCK_STREAM;
        } else {
		printf("Swapfs mount options: rsize=%d,wsize=%d",
		nfsdiskless.swap_args.rsize,
		nfsdiskless.swap_args.wsize);
		if (nfsdiskless.swap_args.flags & NFSMNT_RESVPORT)
			printf (",resvport");
		if (nfsdiskless.swap_args.flags & NFSMNT_SOFT)
			printf (",soft");
		if (nfsdiskless.swap_args.flags & NFSMNT_INT)
			printf (",intr");
		if (nfsdiskless.swap_args.sotype == SOCK_STREAM)
			printf (",tcp");
		else
			printf (",udp");
		printf ("\r\n");
        }
}

/**************************************************************************
EXECUTE - Decode command
**************************************************************************/
execute(buf)
	char *buf;
{
	char *p, *q;
	struct bootcmds_t *cmd = bootcmds;
	while (*buf == ' ' || *buf == '\t')
		buf++;
	if ((!(*buf)) || (*buf == '#')) 
		return(0);
	while(cmd->name) {
		p = buf;
		q = cmd->name;
		while (*q && (*(q++) == *(p++))) ;
		if ((!(*q)) && ((*p == ' ') || (*p == '\t') || (!(*p)))) {
			if (!cmd->func) 
				return(1);
			while (*p == ' ')
				p++;
			(cmd->func)(p);
			return(0);
		} else
			cmd++;
	}
	printf("bad command - type 'help' for list\n\r");
	return(0);
}

/**************************************************************************
BOOTMENU - Present boot options
**************************************************************************/
bootmenu()
{
	char cmd[80];
	int ptr, c;
	printf("\r\n");
	while (1) {
		ptr = 0;
		printf("boot> ");
		while (ptr < 80) {
			c = getchar();
			if (c == '\r')
				break;
			else if (c == '\b') {
				if (ptr > 0) {
					ptr--;
					printf("\b \b");
				}
			} else {
				cmd[ptr++] = c;
				putchar(c);
			}
		}
		cmd[ptr] = 0;
		printf("\r\n");
		if (execute(cmd)) break;
	}
	eth_reset();
}
