/*
 * Copyright 2001, QNX Software Systems Ltd. All Rights Reserved
 * 
 * This source code has been published by QNX Software Systems Ltd. (QSSL).
 * However, any use, reproduction, modification, distribution or transfer of
 * this software, or any software which includes or is based upon any of this
 * code, is only permitted under the terms of the QNX Open Community License
 * version 1.0 (see licensing.qnx.com for details) or as otherwise expressly
 * authorized by a written license agreement from QSSL. For more information,
 * please email licensing@qnx.com.
 *
 * For more details, see QNX_OCL.txt provided with this distribution.
 *
 * $FreeBSD$
 */

/*
 * Simple H.323 proxy
 * 
 *      by xtang@canada.com
 *	ported to ipfilter 3.4.20 by Michael Grant mg-ipf@grant.org
 */

#if __FreeBSD_version >= 220000 && defined(_KERNEL)
# include <sys/fcntl.h>
# include <sys/filio.h>
#else
# include <sys/ioctl.h>
#endif

#define IPF_H323_PROXY

int  ippr_h323_init __P((void));
int  ippr_h323_new __P((fr_info_t *, ip_t *, ap_session_t *, nat_t *));
void ippr_h323_del __P((ap_session_t *));
int  ippr_h323_out __P((fr_info_t *, ip_t *, ap_session_t *, nat_t *));
int  ippr_h323_in __P((fr_info_t *, ip_t *, ap_session_t *, nat_t *));

int  ippr_h245_init __P((void));
int  ippr_h245_new __P((fr_info_t *, ip_t *, ap_session_t *, nat_t *));
int  ippr_h245_out __P((fr_info_t *, ip_t *, ap_session_t *, nat_t *));
int  ippr_h245_in __P((fr_info_t *, ip_t *, ap_session_t *, nat_t *));

static	frentry_t	h323_fr;
#if	(SOLARIS || defined(__sgi)) && defined(_KERNEL)
extern  KRWLOCK_T   ipf_nat;
#endif

static int find_port __P((int, u_char *, int datlen, int *, u_short *));


static int find_port(ipaddr, data, datlen, off, port)
int ipaddr;
unsigned char *data;
int datlen, *off;
unsigned short *port;
{
	u_32_t addr, netaddr;
	u_char *dp;
	int offset;

	if (datlen < 6)
		return -1;
	
	*port = 0;
	offset = *off;
	dp = (u_char *)data;
	netaddr = ntohl(ipaddr);

	for (offset = 0; offset <= datlen - 6; offset++, dp++) {
		addr = (dp[0] << 24) | (dp[1] << 16) | (dp[2] << 8) | dp[3];
		if (netaddr == addr)
		{
			*port = (*(dp + 4) << 8) | *(dp + 5);
			break;
		}
	}
	*off = offset;
  	return (offset > datlen - 6) ? -1 : 0;
}

/*
 * Initialize local structures.
 */
int ippr_h323_init()
{
	bzero((char *)&h323_fr, sizeof(h323_fr));
	h323_fr.fr_ref = 1;
	h323_fr.fr_flags = FR_INQUE|FR_PASS|FR_QUICK|FR_KEEPSTATE;

	return 0;
}


int ippr_h323_new(fin, ip, aps, nat)
fr_info_t *fin;
ip_t *ip;
ap_session_t *aps;
nat_t *nat;
{
	aps->aps_data = NULL;
	aps->aps_psiz = 0;

	return 0;
}


void ippr_h323_del(aps)
ap_session_t *aps;
{
	int i;
	ipnat_t *ipn;
	
	if (aps->aps_data) {
		for (i = 0, ipn = aps->aps_data;
		     i < (aps->aps_psiz / sizeof(ipnat_t)); 
		     i++, ipn = (ipnat_t *)((char *)ipn + sizeof(*ipn)))
		{
			/* 
			 * Check the comment in ippr_h323_in() function,
			 * just above nat_ioctl() call.
			 * We are lucky here because this function is not
			 * called with ipf_nat locked.
			 */
			if (nat_ioctl((caddr_t)ipn, SIOCRMNAT, NAT_SYSSPACE|
				      NAT_LOCKHELD|FWRITE) == -1) {
				/* log the error */
			}
		}
		KFREES(aps->aps_data, aps->aps_psiz);
		/* avoid double free */
		aps->aps_data = NULL;
		aps->aps_psiz = 0;
	}
	return;
}


int ippr_h323_out(fin, ip, aps, nat)
fr_info_t *fin;
ip_t *ip;
ap_session_t *aps;
nat_t *nat;
{
	return 0;
}


int ippr_h323_in(fin, ip, aps, nat)
fr_info_t *fin;
ip_t *ip;
ap_session_t *aps;
nat_t *nat;
{
	int ipaddr, off, datlen;
	unsigned short port;
	unsigned char *data;
	tcphdr_t *tcp;
	
	tcp = (tcphdr_t *)fin->fin_dp;
	ipaddr = ip->ip_src.s_addr;
	
	data = (unsigned char *)tcp + (tcp->th_off << 2);
	datlen = fin->fin_dlen - (tcp->th_off << 2);
	if (find_port(ipaddr, data, datlen, &off, &port) == 0) {
		ipnat_t *ipn;
		char *newarray;

		/* setup a nat rule to set a h245 proxy on tcp-port "port"
		 * it's like:
		 *   map <if> <inter_ip>/<mask> -> <gate_ip>/<mask> proxy port <port> <port>/tcp
		 */
		KMALLOCS(newarray, char *, aps->aps_psiz + sizeof(*ipn));
		if (newarray == NULL) {
			return -1;
		}
		ipn = (ipnat_t *)&newarray[aps->aps_psiz];
		bcopy(nat->nat_ptr, ipn, sizeof(ipnat_t));
		strncpy(ipn->in_plabel, "h245", APR_LABELLEN);
		
		ipn->in_inip = nat->nat_inip.s_addr;
		ipn->in_inmsk = 0xffffffff;
		ipn->in_dport = htons(port);
		/* 
		 * we got a problem here. we need to call nat_ioctl() to add
		 * the h245 proxy rule, but since we already hold (READ locked)
		 * the nat table rwlock (ipf_nat), if we go into nat_ioctl(),
		 * it will try to WRITE lock it. This will causing dead lock
		 * on RTP.
		 * 
		 * The quick & dirty solution here is release the read lock,
		 * call nat_ioctl() and re-lock it.
		 * A (maybe better) solution is do a UPGRADE(), and instead
		 * of calling nat_ioctl(), we add the nat rule ourself.
		 */
		RWLOCK_EXIT(&ipf_nat);
		if (nat_ioctl((caddr_t)ipn, SIOCADNAT,
			      NAT_SYSSPACE|FWRITE) == -1) {
			READ_ENTER(&ipf_nat);
			return -1;
		}
		READ_ENTER(&ipf_nat);
		if (aps->aps_data != NULL && aps->aps_psiz > 0) {
			bcopy(aps->aps_data, newarray, aps->aps_psiz);
			KFREES(aps->aps_data, aps->aps_psiz);
		}
		aps->aps_data = newarray;
		aps->aps_psiz += sizeof(*ipn);
	}
	return 0;
}


int ippr_h245_init()
{
	return 0;
}


int ippr_h245_new(fin, ip, aps, nat)
fr_info_t *fin;
ip_t *ip;
ap_session_t *aps;
nat_t *nat;
{
	aps->aps_data = NULL;
	aps->aps_psiz = 0;
	return 0;
}


int ippr_h245_out(fin, ip, aps, nat)
fr_info_t *fin;
ip_t *ip;
ap_session_t *aps;
nat_t *nat;
{
	int ipaddr, off, datlen;
	u_short port;
	unsigned char *data;
	tcphdr_t *tcp;
	
	tcp = (tcphdr_t *)fin->fin_dp;
	ipaddr = nat->nat_inip.s_addr;
	data = (unsigned char *)tcp + (tcp->th_off << 2);
	datlen = ip->ip_len - fin->fin_hlen - (tcp->th_off << 2);
	if (find_port(ipaddr, data, datlen, &off, &port) == 0) {
		fr_info_t fi;
		nat_t     *ipn;

/*		port = htons(port); */
		ipn = nat_outlookup(fin->fin_ifp, IPN_UDP, IPPROTO_UDP,
				    ip->ip_src, ip->ip_dst, 1);
		if (ipn == NULL) {
			struct ip newip;
			struct udphdr udp;
			
			bcopy(ip, &newip, sizeof(newip));
			newip.ip_len = fin->fin_hlen + sizeof(udp);
			newip.ip_p = IPPROTO_UDP;
			newip.ip_src = nat->nat_inip;
			
			bzero(&udp, sizeof(udp));
			udp.uh_sport = port;
			
			bcopy(fin, &fi, sizeof(fi));
			fi.fin_fi.fi_p = IPPROTO_UDP;
			fi.fin_data[0] = port;
			fi.fin_data[1] = 0;
			fi.fin_dp = (char *)&udp;
			
			ipn = nat_new(&fi, &newip, nat->nat_ptr, NULL,
				      IPN_UDP|FI_W_DPORT, NAT_OUTBOUND);
			if (ipn != NULL) {
				ipn->nat_ptr->in_hits++;
#ifdef	IPFILTER_LOG
				nat_log(ipn, (u_int)(nat->nat_ptr->in_redir));
#endif
				bcopy((u_char*)&ip->ip_src.s_addr,
				      data + off, 4);
				bcopy((u_char*)&ipn->nat_outport,
				      data + off + 4, 2);
			}
		}
	}
	return 0;
}


int ippr_h245_in(fin, ip, aps, nat)
fr_info_t *fin;
ip_t *ip;
ap_session_t *aps;
nat_t *nat;
{
	return 0;
}
