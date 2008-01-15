/*	$FreeBSD: src/sys/contrib/ipfilter/netinet/ip_rules.c,v 1.1.1.1 2005/04/25 18:15:34 darrenr Exp $	*/

/*
* Copyright (C) 1993-2000 by Darren Reed.
*
* Redistribution and use in source and binary forms are permitted
* provided that this notice is preserved and due credit is given
* to the original author and the contributors.
*/

#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#if !defined(__FreeBSD__) && !defined(__OpenBSD__) && !defined(__sgi)
# include <sys/systm.h>
#endif
#include <sys/errno.h>
#include <sys/param.h>
#if !defined(__SVR4) && !defined(__svr4__) && !defined(__hpux)
# include <sys/mbuf.h>
#endif
#if defined(__FreeBSD__) && (__FreeBSD_version > 220000)
# include <sys/sockio.h>
#else
# include <sys/ioctl.h>
#endif /* FreeBSD */
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include "netinet/ip_compat.h"
#include "netinet/ip_fil.h"

#include "netinet/ip_rules.h"

#ifndef _KERNEL
# include <string.h>
#endif /* _KERNEL */

#ifdef IPFILTER_COMPILED

static u_long in_rule__0[] = {
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xffffffff, 0, 0, 0, 0, 0, 0, 0x1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x80000000, 0x8002, 0, 0, 0, 0xffff, 0, 0, 0x4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static u_long out_rule__0[] = {
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xffffffff, 0, 0, 0, 0, 0, 0, 0x1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x80000000, 0x4002, 0, 0, 0, 0xffff, 0, 0, 0x4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

frentry_t *ipf_rules_in_[1] = {
	(frentry_t *)&in_rule__0
};

frentry_t *ipfrule_match_in_(fin, passp)
fr_info_t *fin;
u_32_t *passp;
{
	frentry_t *fr = NULL;

	fr = (frentry_t *)&in_rule__0;
	return fr;
}

frentry_t *ipf_rules_out_[1] = {
	(frentry_t *)&out_rule__0
};

frentry_t *ipfrule_match_out_(fin, passp)
fr_info_t *fin;
u_32_t *passp;
{
	frentry_t *fr = NULL;

	fr = (frentry_t *)&out_rule__0;
	return fr;
}
static frentry_t ipfrule_out_;

int ipfrule_add_out_()
{
	int i, j, err = 0, max;
	frentry_t *fp;

	max = sizeof(ipf_rules_out_)/sizeof(frentry_t *);
	for (i = 0; i < max; i++) {
		fp = ipf_rules_out_[i];
		fp->fr_next = NULL;
		for (j = i + 1; j < max; j++)
			if (strncmp(fp->fr_group,
				    ipf_rules_out_[j]->fr_group,
				    FR_GROUPLEN) == 0) {
				fp->fr_next = ipf_rules_out_[j];
				break;
			}
	}

	fp = &ipfrule_out_;
	bzero((char *)fp, sizeof(*fp));
	fp->fr_type = FR_T_CALLFUNC|FR_T_BUILTIN;
	fp->fr_flags = FR_OUTQUE|FR_NOMATCH;
	fp->fr_data = (void *)ipf_rules_out_[0];
	fp->fr_dsize = sizeof(ipf_rules_out_[0]);
	fp->fr_v = 4;
	fp->fr_func = (ipfunc_t)ipfrule_match_out_;
	err = frrequest(IPL_LOGIPF, SIOCADDFR, (caddr_t)fp, fr_active, 0);
	return err;
}


int ipfrule_remove_out_()
{
	int err = 0, i;
	frentry_t *fp;

	/*
	 * Try to remove the outbound rule.
	 */
	if (ipfrule_out_.fr_ref > 0) {
		err = EBUSY;
	} else {
		i = sizeof(ipf_rules_out_)/sizeof(frentry_t *) - 1;
		for (; i >= 0; i--) {
			fp = ipf_rules_out_[i];
			if (fp->fr_ref > 1) {
				err = EBUSY;
				break;
			}
		}
	}
	if (err == 0)
		err = frrequest(IPL_LOGIPF, SIOCDELFR,
				(caddr_t)&ipfrule_out_, fr_active, 0);
	if (err)
		return err;


	return err;
}
static frentry_t ipfrule_in_;

int ipfrule_add_in_()
{
	int i, j, err = 0, max;
	frentry_t *fp;

	max = sizeof(ipf_rules_in_)/sizeof(frentry_t *);
	for (i = 0; i < max; i++) {
		fp = ipf_rules_in_[i];
		fp->fr_next = NULL;
		for (j = i + 1; j < max; j++)
			if (strncmp(fp->fr_group,
				    ipf_rules_in_[j]->fr_group,
				    FR_GROUPLEN) == 0) {
				fp->fr_next = ipf_rules_in_[j];
				break;
			}
	}

	fp = &ipfrule_in_;
	bzero((char *)fp, sizeof(*fp));
	fp->fr_type = FR_T_CALLFUNC|FR_T_BUILTIN;
	fp->fr_flags = FR_INQUE|FR_NOMATCH;
	fp->fr_data = (void *)ipf_rules_in_[0];
	fp->fr_dsize = sizeof(ipf_rules_in_[0]);
	fp->fr_v = 4;
	fp->fr_func = (ipfunc_t)ipfrule_match_in_;
	err = frrequest(IPL_LOGIPF, SIOCADDFR, (caddr_t)fp, fr_active, 0);
	return err;
}


int ipfrule_remove_in_()
{
	int err = 0, i;
	frentry_t *fp;

	/*
	 * Try to remove the inbound rule.
	 */
	if (ipfrule_in_.fr_ref > 0) {
		err = EBUSY;
	} else {
		i = sizeof(ipf_rules_in_)/sizeof(frentry_t *) - 1;
		for (; i >= 0; i--) {
			fp = ipf_rules_in_[i];
			if (fp->fr_ref > 1) {
				err = EBUSY;
				break;
			}
		}
	}
	if (err == 0)
		err = frrequest(IPL_LOGIPF, SIOCDELFR,
				(caddr_t)&ipfrule_in_, fr_active, 0);
	if (err)
		return err;


	return err;
}

int ipfrule_add()
{
	int err;

	err = ipfrule_add_out_();
	if (err != 0)
		return err;
	err = ipfrule_add_in_();
	if (err != 0)
		return err;
	return 0;
}


int ipfrule_remove()
{
	int err;

	err = ipfrule_remove_out_();
	if (err != 0)
		return err;
	err = ipfrule_remove_in_();
	if (err != 0)
		return err;
	return 0;
}
#endif /* IPFILTER_COMPILED */
