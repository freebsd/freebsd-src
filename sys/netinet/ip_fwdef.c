/*
 * Copyright (c) 1993 Daniel Boulet
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
 * Dumb definitions which needed when 
 * firewall/accounting module is not loaded.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/domain.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/time.h> 
#include <sys/kernel.h>

#include <net/if.h>
#include <net/route.h>


#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>

#include <netinet/ip_fw.h>

struct  ip_fw *ip_fw_chain=NULL;
u_short ip_fw_policy=0;

struct  ip_fw *ip_acct_chain=NULL;

int (*ip_fw_chk_ptr)(struct ip *,struct ifnet *,struct ip_fw *)=NULL;
int (*ip_fw_ctl_ptr)(int,struct mbuf *)=NULL;

void (*ip_acct_cnt_ptr)(struct ip *,struct ifnet *,struct ip_fw *,int)=NULL;
int  (*ip_acct_ctl_ptr)(int,struct mbuf *)=NULL;
