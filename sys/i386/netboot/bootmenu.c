/**************************************************************************
NETBOOT -  BOOTP/TFTP Bootstrap Program

Author: Martin Renters
  Date: Dec/93

**************************************************************************/
#include "netboot.h"

int cmd_ip(), cmd_server(), cmd_bootfile(), cmd_help(), exit();

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
	{"ip",		cmd_ip,		"<addr>          set my IP address"},
	{"server",	cmd_server,	"<addr>      set server IP address"},
	{"bootfile",	cmd_bootfile,	"<file>    set boot filename"},
	{"help",	cmd_help,	"              this list"},
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

extern char bootname[], *bootfile;
/**************************************************************************
CMD_BOOTFILE - set boot filename
**************************************************************************/
cmd_bootfile(p)
	char *p;
{
	char *q = bootfile = bootname;
	if (*p) {
		while(*p)
			*(q++) = *(p++);
		*q = 0;
	} else
		printf("Bootfile is %s\r\n", bootfile);
}



/**************************************************************************
EXECUTE - Decode command
**************************************************************************/
execute(buf)
	char *buf;
{
	char *p, *q;
	struct bootcmds_t *cmd = bootcmds;
	if (!(*buf)) return(0);
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
	while (1) {
		ptr = 0;
		printf("\r\nboot> ");
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
