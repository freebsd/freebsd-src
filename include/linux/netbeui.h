#ifndef _LINUX_NETBEUI_H
#define _LINUX_NETBEUI_H

#include <linux/if.h>

#define NB_NAME_LEN	20	/* Set this properly from the full docs when
				   I get them */
				   
struct sockaddr_netbeui
{
	sa_family	snb_family;
	char		snb_name[NB_NAME_LEN];
	char		snb_devhint[IFNAMSIZ];
};

#endif
