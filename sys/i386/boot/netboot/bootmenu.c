/**************************************************************************
NETBOOT -  BOOTP/TFTP Bootstrap Program

Author: Martin Renters
  Date: Dec/93

**************************************************************************/
#include "netboot.h"

extern struct nfs_diskless nfsdiskless;
extern int hostnamelen;
extern unsigned long netmask;

int cmd_ip(), cmd_server(), cmd_kernel(), cmd_help(), exit();
int cmd_rootfs(), cmd_swapfs(), cmd_interface(), cmd_hostname();
int cmd_netmask();

#ifdef SMALL_ROM
struct bootcmds_t {
	char *name;
	int (*func)();
} bootcmds[] = {
	{"ip",		cmd_ip},
	{"server",	cmd_server},
	{"bootfile",	cmd_bootfile},
	{"diskboot",	exit},
	{"autoboot",	NULL},
	{NULL,		NULL}
};

#else					/* !SMALL ROM */

struct bootcmds_t {
	char *name;
	int (*func)();
	char *help;
} bootcmds[] = {
	{"?",		cmd_help,	"              this list"},
	{"help",	cmd_help,	"              this list"},
	{"ip",		cmd_ip,		"<addr>          set my IP addr"},
	{"server",	cmd_server,	"<addr>      set TFTP server IP addr"},
	{"netmask",	cmd_netmask,	"<addr>     set network mask"},
	{"hostname",	cmd_hostname,	"          set hostname"},
	{"kernel",	cmd_kernel,	"<file>      set boot filename"},
	{"rootfs",	cmd_rootfs,	"            set root filesystem"},
	{"swapfs",	cmd_swapfs,	"            set swap filesystem"},
	{"diskboot",	exit,		"          boot from disk"},
	{"autoboot",	NULL,		"          continue"},
	{NULL,		NULL,		NULL}
};

/**************************************************************************
CMD_HELP - Display help screen  - NOT FOR SMALL ROMS
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
#endif					/* SMALL ROM */

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
	}
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
CMD_SWAPFS - Set root filesystem name
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
EXECUTE - Decode command
**************************************************************************/
execute(buf)
	char *buf;
{
	char *p, *q;
	struct bootcmds_t *cmd = bootcmds;
	if ((!(*buf)) || (*buf == '#')) return(0);
	while(cmd->name) {
		p = buf;
		q = cmd->name;
		while (*q && (*(q++) == *(p++))) ;
		if ((!(*q)) && ((*p == ' ') || (!(*p)))) {
			if (!cmd->func) return(1);
			while (*p == ' ') p++;
			(cmd->func)(p);
			return(0);
		} else
			cmd++;
	}
#ifdef SMALL_ROM
	printf("invalid command\n\r");
#else
	printf("bad command - type 'help' for list\n\r");
#endif
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
