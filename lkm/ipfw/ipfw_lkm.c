/*
 * Copyright (c) 1994 Ugen J.S.Antsilevich
 *
 * Redistribution and use in source forms, with and without modification,
 * are permitted provided that this entire comment appears intact.
 *
 * Redistribution in binary form may occur without any restrictions.
 * Obviously, it would be nice if you gave credit where credit is due
 * but requiring it would be too onerous.
 *
 * This software is provided ``AS IS'' without any warranties of any kind.
 */

/*
 * LKM init functions and stuff.
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/exec.h>
#include <sys/sysent.h>
#include <sys/lkm.h>

#include <net/if.h>
#include <net/route.h>


#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_fw.h>

MOD_MISC(ipfw);

static int
ipfw_load(struct lkm_table *lkmtp, int cmd)
{
int s=splnet();
#ifdef IPFIREWALL
	if (ip_fw_chk_ptr!=NULL || ip_fw_ctl_ptr!=NULL) {
		uprintf("IpFw/IpAcct already loaded.\n");
		return 1;
	}
#endif
#ifdef IPACCT
	if (ip_acct_cnt_ptr!=NULL || ip_acct_ctl_ptr!=NULL) {
		uprintf("IpFw/IpAcct already loaded.\n");
		return 1;
	}
#endif
#ifdef IPFIREWALL
	ip_fw_chk_ptr=&ip_fw_chk;
	ip_fw_ctl_ptr=&ip_fw_ctl;
#endif
#ifdef IPACCT
	ip_acct_cnt_ptr=&ip_acct_cnt;
	ip_acct_ctl_ptr=&ip_acct_ctl;
#endif
	uprintf("IpFw/IpAcct 1994(c) Ugen J.S.Antsilevich\n");
	splx(s);
	return 0;
}

static int
ipfw_unload(struct lkm_table *lkmtp, int cmd)
{
int s=splnet();
#ifdef IPFIREWALL
	ip_fw_ctl_ptr=NULL;
 	ip_fw_chk_ptr=NULL;
#endif
#ifdef IPACCT
	ip_acct_ctl_ptr=NULL;
	ip_acct_cnt_ptr=NULL;
#endif
	uprintf("IpFw/IpAcct removed.\n");
	splx(s);
	return 0;
}

int
ipfw_mod(struct lkm_table *lkmtp, int cmd, int ver)
{
	DISPATCH(lkmtp, cmd, ver, ipfw_load, ipfw_unload, lkm_nullcmd);
}
