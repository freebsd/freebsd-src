/*	$FreeBSD$	*/

/*
 * Copyright (C) 1993-1998 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 */
/* #pragma ident   "@(#)solaris.c	1.12 6/5/96 (C) 1995 Darren Reed"*/

/*typedef unsigned int spustate_t;*/
struct uio;

#include <sys/types.h>
#include <sys/cmn_err.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/conf.h>
#include <sys/callout.h>
#include <sys/moddefs.h>
#include <sys/io.h>
#include <sys/wsio.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/byteorder.h>
#include <sys/socket.h>
#include <sys/stropts.h>
#include <net/if.h>
#include <net/af.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/tcpip.h>
#include <netinet/ip_icmp.h>

#include "ip_compat.h"
#include "ip_fil.h"
#include "ip_rules.h"


/*
 * Driver Header
 */
static drv_info_t ipf_drv_info = {
	"IP Filter Rules",				/* type */
	"pseudo",					/* class */
	DRV_PSEUDO|DRV_SAVE_CONF|DRV_MP_SAFE,		/* flags */
	-1,						/* b_major */
	-1,						/* c_major */
	NULL,						/* cdio */
	NULL,						/* gio_private */
	NULL,						/* cdio_private */
};


extern	struct	mod_operations	gio_mod_ops;
static	drv_info_t	ipf_drv_info;
extern	struct	mod_conf_data	ipf_conf_data;

static struct mod_type_data	ipf_drv_link = {
	IPL_VERSION, (void *)NULL
};

static	struct	modlink	ipf_mod_link[] = {
	{ &gio_mod_ops, (void *)&ipf_drv_link },
	{ NULL, (void *)NULL }
};

struct	modwrapper	ipf_wrapper = {
	MODREV,
	ipf_load,
	ipf_unload,
	(void (*)())NULL,
	(void *)&ipf_conf_data,
	ipf_mod_link
};


static int ipf_load(void *arg)
{
	int i;

	i = ipfrule_add();
	if (!i)
		fr_refcnt--;
#ifdef	IPFDEBUG
	printf("IP Filter Rules: ipfrule_add() = %d\n", i);
#endif
	if (!i)
		cmn_err(CE_CONT, "IP Filter Rules: Loaded\n");
	return i;
}


static int ipf_unload(void *arg)
{
	int i;

	i = ipfrule_remove();
	if (!i)
		fr_refcnt--;
#ifdef	IPFDEBUG
	printf("IP Filter Rules: ipfrule_remove() = %d\n", i);
#endif
	if (!i)
		cmn_err(CE_CONT, "IP Filter Rules: Unloaded\n");
	return i;
}
