/*	$FreeBSD$	*/

/*
 * Copyright (C) 2000 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * Id: mlfk_rule.c,v 2.4.4.2 2004/04/16 23:32:08 darrenr Exp
 */


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/conf.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <net/if.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>

#include <netinet/ipl.h>
#include <netinet/ip_compat.h>
#include <netinet/ip_fil.h>
#include <netinet/ip_state.h>
#include <netinet/ip_nat.h>
#include <netinet/ip_auth.h>
#include <netinet/ip_frag.h>

#include "ip_rules.h"


static int
ipfrule_modevent(module_t mod, int type, void *unused)
{
	int error = 0;

	switch (type)
	{
	case MOD_LOAD :
		error = ipfrule_add();
		if (!error)
			fr_refcnt++;
		break;
	case MOD_UNLOAD :
		error = ipfrule_remove();
		if (!error)
			fr_refcnt--;
		break;
	default:
		error = EINVAL;
		break;
	}
	return error;
}

static moduledata_t ipfrulemod = {
	"ipfrule",
	ipfrule_modevent,
        0
};
DECLARE_MODULE(ipfrule, ipfrulemod, SI_SUB_PROTO_DOMAIN, SI_ORDER_ANY);
#ifdef	MODULE_DEPEND
MODULE_DEPEND(ipfrule, ipfilter, 1, 1, 1);
#endif
#ifdef	MODULE_VERSION
MODULE_VERSION(ipfrule, 1);
#endif
