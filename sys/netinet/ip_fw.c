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
 * Implement IP packet firewall
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/time.h> 
#include <sys/kernel.h>

#include <net/if.h>
#include <net/route.h>


#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>

#include <arpa/inet.h>

#include <netinet/ip_fw.h>

#ifdef IPFIREWALL
struct ip_fw *ip_fw_fwd_chain;
struct ip_fw *ip_fw_blk_chain;
u_short ip_fw_policy=0;
#endif
#ifdef IPACCT
struct ip_fw *ip_acct_chain;
#endif


#ifdef IPFIREWALL_DEBUG 
#define dprintf1(a)		printf(a)
#define dprintf2(a1,a2)		printf(a1,a2)
#define dprintf3(a1,a2,a3)	printf(a1,a2,a3)
#define dprintf4(a1,a2,a3,a4)	printf(a1,a2,a3,a4)
#else
#define dprintf1(a)	
#define dprintf2(a1,a2)
#define dprintf3(a1,a2,a3)
#define dprintf4(a1,a2,a3,a4)
#endif


#define print_ip(a)	 printf("%d.%d.%d.%d",(ntohl(a.s_addr)>>24)&0xFF,\
					      (ntohl(a.s_addr)>>16)&0xFF,\
					      (ntohl(a.s_addr)>>8)&0xFF,\
					      (ntohl(a.s_addr))&0xFF);

#ifdef IPFIREWALL_DEBUG
#define dprint_ip(a)	print_ip(a)
#else
#define dprint_ip(a)	
#endif

/*
inline
void
print_ip(xaddr)
struct in_addr xaddr;
{
    u_long addr = ntohl(xaddr.s_addr);
    printf("%d.%d.%d.%d",(addr>>24) & 0xff,
                         (addr>>16)&0xff,
                         (addr>>8)&0xff,
                         addr&0xFF);
}                  
*/


/*
 * Returns 1 if the port is matched by the vector, 0 otherwise
 */
inline
int port_match(portptr,nports,port,range_flag)
u_short *portptr;
int nports;
u_short port;
int range_flag;
{
    if (!nports)
	return 1;
    if (range_flag) {
	if (portptr[0]<=port && port<=portptr[1]) {
	    return 1;
	}
	nports-=2;
	portptr+=2;
    }
    while (nports-->0) {
	if (*portptr++==port) {
	    return 1;
	}
    }
    return 0;
}


/*
 * Returns 0 if packet should be dropped, 1 or more if it should be accepted
 */

#ifdef IPFIREWALL
int ip_fw_chk(ip,rif,chain)
struct ip 	*ip;
struct ifnet	*rif;
struct ip_fw 	*chain;
{
    register struct ip_fw 	*f;
    struct tcphdr 		*tcp=(struct tcphdr *)((u_long *)ip+ip->ip_hl);
    struct udphdr 		*udp=(struct udphdr *)((u_long *)ip+ip->ip_hl);
    struct icmp 		*icmp=(struct icmp  *)((u_long *)ip+ip->ip_hl);
    struct ifaddr               *ia=NULL, *ia_p;
    struct in_addr 		src, dst, ia_i;
    struct mbuf 		*m;
    u_short 			src_port=0, dst_port=0;
    u_short 			f_prt=0, prt;
    char			notcpsyn=1;

		/*
		 * If the chain is empty
		 * allow any packet-this is equal 
		 * to disabling firewall.
		 */
    if (!chain) 
	return(1);    

		/* 
		 * This way we handle fragmented packets.
		 * we ignore all fragments but the first one
		 * so the whole packet can't be reassembled.
		 * This way we relay on the full info which
		 * stored only in first packet.
		 */
    if (ip->ip_off&IP_OFFMASK)
	return(1);

    src = ip->ip_src;
    dst = ip->ip_dst;

		/*
		 * If we got interface from
		 * which packet came-store
		 * pointer to it's first adress
		 */
    if (rif)
	ia=rif->if_addrlist;

	dprintf1("Packet ");
	switch(ip->ip_p) {
		case IPPROTO_TCP:
			dprintf1("TCP ");
			src_port=ntohs(tcp->th_sport);
			dst_port=ntohs(tcp->th_dport);
			if (tcp->th_flags&TH_SYN)
				notcpsyn=0; /* We *DO* have SYN ,value FALSE */
			prt=IP_FW_F_TCP;
			break;
		case IPPROTO_UDP:
			dprintf1("UDP ");
			src_port=ntohs(udp->uh_sport);
			dst_port=ntohs(udp->uh_dport);
			prt=IP_FW_F_UDP;
			break;
		case IPPROTO_ICMP:
			dprintf2("ICMP:%u ",icmp->icmp_type);
			prt=IP_FW_F_ICMP;
			break;
		default:
			dprintf2("p=%d ",ip->ip_p);
			prt=IP_FW_F_ALL;
			break;
	}
	dprint_ip(ip->ip_src);
	if (ip->ip_p==IPPROTO_TCP || ip->ip_p==IPPROTO_UDP) {
		dprintf2(":%d ",src_port);
	}
	dprint_ip(ip->ip_dst);
	if (ip->ip_p==IPPROTO_TCP || ip->ip_p==IPPROTO_UDP) {
	    dprintf2(":%d ",dst_port);
	}
	dprintf1("\n");


    for (f=chain;f;f=f->next) 
		if ((src.s_addr&f->src_mask.s_addr)==f->src.s_addr
		&&  (dst.s_addr&f->dst_mask.s_addr)==f->dst.s_addr) {
       		if (f->via.s_addr && rif) {
                        for (ia_p=ia;ia_p;ia_p=ia_p->ifa_next) {
                                if (!ia_p->ifa_addr ||
                                     ia_p->ifa_addr->sa_family!=AF_INET)
					/*
					 * Next interface adress.
					 * This is continue for 
					 * local "for"
					 */
                                        continue; 
                                ia_i.s_addr=(((struct sockaddr_in *)\
                                            (ia_p->ifa_addr))->sin_addr.s_addr);
                                if (ia_i.s_addr==f->via.s_addr)
                                        goto via_match;
                        }
			/*
			 * Next interface adress.
			 * This is continue for 
			 * local "for"
			 */
                        continue; 
                } else {
			/*
			 * No special "via" adress set
			 * or interface from which packet
			 * came unknown so match anyway
			 */
                        goto via_match; 
                }
		/*	
		 * Skip to next firewall entry - via 
		 * address did not matched.
		 */
                continue; 
via_match:
			f_prt=f->flags&IP_FW_F_KIND;
			if (f_prt==IP_FW_F_ALL) {
				/* Universal frwl - we've got a match! */
			goto got_match;
	    } else {
	/*
	 * This is actually buggy as if you set SYN flag 
	 * on UDp or ICMP firewall it will never work,but 
	 * actually it is a concern of software which sets
	 * firewall entries.
	 */
    if (f->flags&IP_FW_F_TCPSYN && notcpsyn)
		continue;

	/*
	 * Specific firewall - packet's
	 * protocol must match firewall's
	 */
    if (prt==f_prt) {

    if (prt==IP_FW_F_ICMP ||
       (port_match(&f->ports[0],f->n_src_p,src_port,
					f->flags&IP_FW_F_SRNG) &&
        port_match(&f->ports[f->n_src_p],f->n_dst_p,dst_port,
					f->flags&IP_FW_F_DRNG))) {
		goto got_match;
    } /* Ports match */
    } /* Proto matches */
 }  /* ALL/Specific */
} /* IP addr/mask matches */

    /*
     * If we get here then none of the firewalls matched.
     * So now we relay on policy defined by user-unmatched packet can
     * be ever accepted or rejected...
     */
	f=(struct ip_fw *)NULL;
    	if (ip_fw_policy&IP_FW_P_DENY)
		goto bad_packet;
	else
		goto good_packet;

got_match:
#ifdef IPFIREWALL_VERBOSE
		/*
		 * VERY ugly piece of code which actually
		 * makes kernel printf for denied packets...
		 */
    if (f->flags&IP_FW_F_PRN) {
	if (f->flags&IP_FW_F_ACCEPT)
		printf("Accept ");
	else
		printf("Deny ");
	switch(ip->ip_p) {
		case IPPROTO_TCP:
			printf("TCP ");
			break;
		case IPPROTO_UDP:
			printf("UDP ");
			break;
		case IPPROTO_ICMP:
			printf("ICMP:%u ",icmp->icmp_type);
			break;
		default:
			printf("p=%d ",ip->ip_p);
			break;
	}
	print_ip(ip->ip_src);
	if (ip->ip_p==IPPROTO_TCP || ip->ip_p==IPPROTO_UDP) 
	    printf(":%d",src_port);
	printf(" ");
	print_ip(ip->ip_dst);
	if (ip->ip_p==IPPROTO_TCP || ip->ip_p==IPPROTO_UDP)
	    printf(":%d",dst_port);
	printf("\n");
    }
#endif
	if (f->flags&IP_FW_F_ACCEPT)
		goto good_packet;
#ifdef noneed
	else
		goto bad_packet;
#endif

bad_packet:
	if (f) {
			/*
			 * Do not ICMP reply to icmp
			 * packets....:)
			 */
		if (f_prt==IP_FW_F_ICMP)
			return 0;
			/*
			 * Reply to packets rejected
			 * by entry with this flag
			 * set only.
			 */
		if (!(f->flags&IP_FW_F_ICMPRPL))
			return 0;
   		m = m_get(M_DONTWAIT, MT_SOOPTS); 
			/*
			 * We never retry,we don't want to 
			 * waste time-it is not so critical 
			 * if ICMP unsent.
			 */
		if (!m)
			return 0;
   		m->m_len = sizeof(struct ip)+64;
   		bcopy((caddr_t)ip,mtod(m, caddr_t),(unsigned)m->m_len);
		if (f_prt==IP_FW_F_ALL)
   			icmp_error(m, ICMP_UNREACH, ICMP_UNREACH_HOST, 0L, 0);
		else
   			icmp_error(m, ICMP_UNREACH, ICMP_UNREACH_PORT, 0L, 0);
		return 0;
	} else {
		/*
		 * If global icmp flag set we will do
		 * something here...later..
		 */
		return 0;
	}
good_packet:
	return 1;
}
#endif /* IPFIREWALL */




#ifdef IPACCT
void ip_acct_cnt(ip,rif,chain,nh_conv)
struct ip 	*ip;
struct ifnet	*rif;
struct ip_fw 	*chain;
int nh_conv;
{
    register struct ip_fw 	*f;
    struct tcphdr 		*tcp=(struct tcphdr *)((u_long *)ip+ip->ip_hl);
    struct udphdr 		*udp=(struct udphdr *)((u_long *)ip+ip->ip_hl);
    struct ifaddr               *ia=NULL, *ia_p;
    struct in_addr 		src, dst, ia_i;
    u_short 			src_port=0, dst_port=0;
    u_short 			f_prt, prt=0;
    char 			rev=0;

    if (!chain) 
		return;     

    if (ip->ip_off&IP_OFFMASK)
		return;

    src = ip->ip_src;
    dst = ip->ip_dst;

    if (rif)
	ia=rif->if_addrlist;

	switch(ip->ip_p) {
		case IPPROTO_TCP:
			src_port=ntohs(tcp->th_sport);
			dst_port=ntohs(tcp->th_dport);
			prt=IP_FW_F_TCP;
			break;
		case IPPROTO_UDP:
			src_port=ntohs(udp->uh_sport);
			dst_port=ntohs(udp->uh_dport);
			prt=IP_FW_F_UDP;
			break;
		case IPPROTO_ICMP:
			prt=IP_FW_F_ICMP;
			break;
		default:
			prt=IP_FW_F_ALL;
			break;
	}

    for (f=chain;f;f=f->next) {
		if ((src.s_addr&f->src_mask.s_addr)==f->src.s_addr
		&&  (dst.s_addr&f->dst_mask.s_addr)==f->dst.s_addr) {
				rev=0;
				goto addr_match;
		}
	 	if  ((f->flags&IP_FW_F_BIDIR) &&
		    ((src.s_addr&f->src_mask.s_addr)==f->dst.s_addr
		&&  (dst.s_addr&f->dst_mask.s_addr)==f->src.s_addr)) { 
				rev=1;
				goto addr_match;
		}
		continue;
addr_match:
       		if (f->via.s_addr && rif) {
                        for (ia_p=ia;ia_p;ia_p=ia_p->ifa_next) {
                                if (!ia_p->ifa_addr ||
                                     ia_p->ifa_addr->sa_family!=AF_INET)
                                        continue; 
                                ia_i.s_addr=(((struct sockaddr_in *)\
                                            (ia_p->ifa_addr))->sin_addr.s_addr);
                                if (ia_i.s_addr==f->via.s_addr)
                                        goto via_match;
                        }
                        continue; 
                } else {
                        goto via_match; 
                }
                continue; 
via_match:
			f_prt=f->flags&IP_FW_F_KIND;
			if (f_prt==IP_FW_F_ALL) {
				/* Universal frwl - we've got a match! */

     			f->p_cnt++;                 /* Rise packet count */

						    /*
						     * Rise byte count,
						     * if need to convert from
						     * host to network byte 
						     * order,do it.
						     */
			if (nh_conv)		    
				f->b_cnt+=ntohs(ip->ip_len);
			else
				f->b_cnt+=ip->ip_len;
	    } else {
	/*
	 * Specific firewall - packet's
	 * protocol must match firewall's
	 */
    if (prt==f_prt) {

    if ((prt==IP_FW_F_ICMP ||
       (port_match(&f->ports[0],f->n_src_p,src_port,
					f->flags&IP_FW_F_SRNG) &&
        port_match(&f->ports[f->n_src_p],f->n_dst_p,dst_port,
					f->flags&IP_FW_F_DRNG)))
	|| ((rev)   
	&& (port_match(&f->ports[0],f->n_src_p,dst_port,
                                        f->flags&IP_FW_F_SRNG)
	&& port_match(&f->ports[f->n_src_p],f->n_dst_p,src_port,
                                        f->flags&IP_FW_F_DRNG))))
								{
		f->p_cnt++;                   /* Rise packet count */
					      /*
					       * Rise byte count,
					       * if need to convert from
					       * host to network byte 
					       * order,do it.
					       */
		if (nh_conv)		    
			f->b_cnt+=ntohs(ip->ip_len);
		else
			f->b_cnt+=ip->ip_len;
    } /* Ports match */
    } /* Proto matches */
 }  /* ALL/Specific */
} /* IP addr/mask matches */
} /* End of whole function */
#endif /* IPACCT */

static
void
zero_fw_chain(chainptr)
struct ip_fw *chainptr;
{
struct ip_fw *ctmp=chainptr;
  while(ctmp) {
	ctmp->p_cnt=0l;
	ctmp->b_cnt=0l;
	ctmp=ctmp->next;
  }
}

static
void
free_fw_chain(chainptr)
struct ip_fw **chainptr;
{
int s=splnet();
    while (*chainptr) {
	struct ip_fw *ftmp;
	ftmp = *chainptr;
	*chainptr = ftmp->next;
	free(ftmp,M_SOOPTS);
    }
splx(s);
}

static
int
add_to_chain(chainptr,frwl)
struct ip_fw **chainptr;
struct ip_fw *frwl;
{
    struct ip_fw *ftmp;
    struct ip_fw *chtmp=NULL;
    struct ip_fw *chtmp_prev=NULL;
    int s=splnet();
    u_long m_src_mask,m_dst_mask;
    u_long n_sa,n_da,o_sa,o_da,o_sm,o_dm,n_sm,n_dm;
    u_short n_sr,n_dr,o_sr,o_dr; 
    u_short oldkind,newkind;
    int addb4=0;
    int n_o,n_n;

    ftmp = malloc(sizeof(struct ip_fw),M_SOOPTS,M_DONTWAIT);
    if ( ftmp == NULL ) {
	dprintf1("ip_fw_ctl:  malloc said no\n");
	splx(s);
	return(ENOSPC);
    }

    bcopy(frwl,ftmp,sizeof(struct ip_fw));
    ftmp->p_cnt=0L;
    ftmp->b_cnt=0L;

    ftmp->next = NULL;

    if (*chainptr==NULL)
       {
        *chainptr=ftmp;
       }
    else
       {
	chtmp_prev=NULL;
	for (chtmp=*chainptr;chtmp!=NULL;chtmp=chtmp->next) {

		addb4=0;

		newkind=ftmp->flags & IP_FW_F_KIND;
		oldkind=chtmp->flags & IP_FW_F_KIND;

		if (newkind!=IP_FW_F_ALL 
		&&  oldkind!=IP_FW_F_ALL
		&&  oldkind!=newkind) {
				chtmp_prev=chtmp;
				continue;
		}
		/*
		 * Very very *UGLY* code...
		 * Sorry,but i had to do this....
		 */
		n_sa=ntohl(ftmp->src.s_addr);
		n_da=ntohl(ftmp->dst.s_addr);
		n_sm=ntohl(ftmp->src_mask.s_addr);
		n_dm=ntohl(ftmp->dst_mask.s_addr);

		o_sa=ntohl(chtmp->src.s_addr);
		o_da=ntohl(chtmp->dst.s_addr);
		o_sm=ntohl(chtmp->src_mask.s_addr);
		o_dm=ntohl(chtmp->dst_mask.s_addr);

		m_src_mask = o_sm & n_sm;
		m_dst_mask = o_dm & n_dm;

		if ((o_sa & m_src_mask) == (n_sa & m_src_mask)) {
			if (n_sm > o_sm) 
				addb4++;
			if (n_sm < o_sm) 
				addb4--;
		}

		if ((o_da & m_dst_mask) == (n_da & m_dst_mask)) {
			if (n_dm > o_dm)
				addb4++;
			if (n_dm < o_dm)
				addb4--;
		}

		if (((o_da & o_dm) == (n_da & n_dm))
                  &&((o_sa & o_sm) == (n_sa & n_sm)))
		{
			if (newkind!=IP_FW_F_ALL &&
			    oldkind==IP_FW_F_ALL)
				addb4++;
			if (newkind==oldkind && (oldkind==IP_FW_F_TCP
					     ||  oldkind==IP_FW_F_UDP)) {

				/*
				 * Here the main ide is to check the size
				 * of port range which the frwl covers
				 * We actually don't check their values but
				 * just the wideness of range they have
				 * so that less wide ranges or single ports
				 * go first and wide ranges go later. No ports
				 * at all treated as a range of maximum number
				 * of ports.
				 */

				if (ftmp->flags & IP_FW_F_SRNG) 
					n_sr=ftmp->ports[1]-ftmp->ports[0];
				else 
					n_sr=(ftmp->n_src_p)?
						ftmp->n_src_p : USHRT_MAX;
					
				if (chtmp->flags & IP_FW_F_SRNG) 
				     o_sr=chtmp->ports[1]-chtmp->ports[0];
				else 
				     o_sr=(chtmp->n_src_p)?
				 	    chtmp->n_src_p : USHRT_MAX;

				if (n_sr<o_sr)
					addb4++;
				if (n_sr>o_sr)
					addb4--;
					
				n_n=ftmp->n_src_p;
				n_o=chtmp->n_src_p;
		/*
		 * Actually this cannot happen as the frwl control
		 * procedure checks for number of ports in source and
		 * destination range but we will try to be more safe.
		 */
				if ((n_n>(IP_FW_MAX_PORTS-2)) ||
				    (n_o>(IP_FW_MAX_PORTS-2)))
					goto skip_check;

				if (ftmp->flags & IP_FW_F_DRNG) 
				       n_dr=ftmp->ports[n_n+1]-ftmp->ports[n_n];
				else 
				       n_dr=(ftmp->n_dst_p)?
						ftmp->n_dst_p : USHRT_MAX;

				if (chtmp->flags & IP_FW_F_DRNG) 
				     o_dr=chtmp->ports[n_o+1]-chtmp->ports[n_o];
				else 
				       o_dr=(chtmp->n_dst_p)?
						chtmp->n_dst_p : USHRT_MAX;
				if (n_dr<o_dr)
					addb4++;
				if (n_dr>o_dr)
					addb4--;

skip_check:
			}
		}
		if (addb4>0) {
			if (chtmp_prev) {
				chtmp_prev->next=ftmp; 
				ftmp->next=chtmp;
			} else {
				*chainptr=ftmp;
				ftmp->next=chtmp;
			}
			splx(s);
			return 0;
		}
		chtmp_prev=chtmp;
	}
	if (chtmp_prev)
		chtmp_prev->next=ftmp;
	else
#define wrong
#ifdef wrong
        	*chainptr=ftmp;
#else
	panic("Can't happen");
#endif
#undef wrong
       }
    splx(s);
    return(0);
}

static
int
del_from_chain(chainptr,frwl)
struct ip_fw **chainptr;
struct ip_fw *frwl;
{
    struct ip_fw *ftmp,*ltmp;
    u_short	tport1,tport2,tmpnum;
    char	matches,was_found;
    int s=splnet();

    ftmp=*chainptr;

    if (ftmp == NULL) {
	dprintf1("ip_fw_ctl:  chain is empty\n");
	splx(s);
	return(EINVAL);
    }

    ltmp=NULL;
    was_found=0;

    while(ftmp)
    {
     matches=1;
     if ((bcmp(&ftmp->src,&frwl->src,sizeof(struct in_addr))) 
     || (bcmp(&ftmp->src_mask,&frwl->src_mask,sizeof(struct in_addr)))
     || (bcmp(&ftmp->dst,&frwl->dst,sizeof(struct in_addr)))
     || (bcmp(&ftmp->dst_mask,&frwl->dst_mask,sizeof(struct in_addr)))
     || (ftmp->flags!=frwl->flags))
        matches=0;
     tport1=ftmp->n_src_p+ftmp->n_dst_p;
     tport2=frwl->n_src_p+frwl->n_dst_p;
     if (tport1!=tport2)
        matches=0;
     else
      if (tport1!=0)
      {
       for (tmpnum=0;tmpnum < tport1 && tmpnum < IP_FW_MAX_PORTS;tmpnum++)
        if (ftmp->ports[tmpnum]!=frwl->ports[tmpnum])
           matches=0;
      }
    if(matches)
      {
        was_found=1;
        if (ltmp)
            {
              ltmp->next=ftmp->next;
	      free(ftmp,M_SOOPTS);
              ftmp=ltmp->next;
            }
        else
            {
             *chainptr=ftmp->next; 
             free(ftmp,M_SOOPTS);
             ftmp=*chainptr;
            }
       
      }      
    else
      {
       ltmp = ftmp;
       ftmp = ftmp->next;
      }
    }
    splx(s);
    if (was_found) return 0;
    else return(EINVAL);
}

struct ip_fw *
check_ipfw_struct(m)
struct mbuf *m;
{
struct ip_fw *frwl;

	    if ( m->m_len != sizeof(struct ip_fw) )	{
		dprintf3("ip_fw_ctl: len=%d, want %d\n",m->m_len,
					sizeof(struct ip_fw));
		return(NULL);
	    }

	    frwl = mtod(m,struct ip_fw*);

	    if ( (frwl->flags & ~IP_FW_F_MASK) != 0 )	{
		dprintf2("ip_fw_ctl: undefined flag bits set (flags=%x)\n",
						frwl->flags);
		return(NULL);
	    }

	    if ( (frwl->flags & IP_FW_F_SRNG) && frwl->n_src_p < 2 ) {
		dprintf2("ip_fw_ctl: src range set but n_src_p=%d\n",
						frwl->n_src_p);
		return(NULL);
	    }

	    if ( (frwl->flags & IP_FW_F_DRNG) && frwl->n_dst_p < 2 ) {
		dprintf2("ip_fw_ctl: dst range set but n_dst_p=%d\n",
						frwl->n_dst_p);
		return(NULL);
	    }

	    if ( frwl->n_src_p + frwl->n_dst_p > IP_FW_MAX_PORTS ) {
		dprintf3("ip_fw_ctl: too many ports (%d+%d)\n",
					frwl->n_src_p,frwl->n_dst_p);
		return(NULL);
	    }

#if 0
	    if ( (frwl->flags & IP_FW_F_KIND) == IP_FW_F_ICMP ) {
		dprintf1("ip_fw_ctl:  request for unsupported ICMP frwling\n");
		return(NULL);
	    }
#endif
return frwl;
}




#ifdef IPACCT
int
ip_acct_ctl(stage,m)
int stage;
struct mbuf *m;
{
if ( stage == IP_ACCT_FLUSH )
       {
	free_fw_chain(&ip_acct_chain);
	return(0);
       }  
if ( stage == IP_ACCT_ZERO )
       {
	zero_fw_chain(ip_acct_chain);
	return(0);
       }
if ( stage == IP_ACCT_ADD
  || stage == IP_ACCT_DEL
   ) {

	    struct ip_fw *frwl;

	    if (!(frwl=check_ipfw_struct(m)))
			return (EINVAL);

	    switch (stage) {
	    case IP_ACCT_ADD:
		return( add_to_chain(&ip_acct_chain,frwl));
	    case IP_ACCT_DEL:
		return( del_from_chain(&ip_acct_chain,frwl));
	    default:
		/*
 		 * Should be panic but...
		 */
		dprintf2("ip_acct_ctl:  unknown request %d\n",stage);
		return(EINVAL);
	    }
 }
	dprintf2("ip_acct_ctl:  unknown request %d\n",stage);
	return(EINVAL);
}
#endif

#ifdef IPFIREWALL
int
ip_fw_ctl(stage,m)
int stage;	
struct mbuf *m;
{
if ( stage == IP_FW_FLUSH )
       {
	free_fw_chain(&ip_fw_blk_chain);
	free_fw_chain(&ip_fw_fwd_chain);
	return(0);
       }  

if ( m == 0 )	
       {
         printf("ip_fw_ctl:  NULL mbuf ptr\n");
	 return(EINVAL);
       }

if ( stage == IP_FW_POLICY )
      {
	u_short *tmp_policy_ptr;
	tmp_policy_ptr=mtod(m,u_short *);
	if ((*tmp_policy_ptr)&~IP_FW_P_MASK)
		return (EINVAL);
	ip_fw_policy=*tmp_policy_ptr;
	return 0;
      }

/*
 * Here we really working hard-adding new elements
 * to blocking/forwarding chains or deleting'em
 */

if ( stage == IP_FW_ADD_BLK
  || stage == IP_FW_ADD_FWD
  || stage == IP_FW_DEL_BLK
  || stage == IP_FW_DEL_FWD
   ) {

	    struct ip_fw *frwl;
 
		frwl=check_ipfw_struct(m);
		if (frwl==NULL)
			return (EINVAL);
#ifdef nenado
	    if (!(frwl=check_ipfw_struct(m)))
			return (EINVAL);
#endif

	    switch (stage) {
	    case IP_FW_ADD_BLK:
		return(add_to_chain(&ip_fw_blk_chain,frwl));
	    case IP_FW_ADD_FWD:
		return(add_to_chain(&ip_fw_fwd_chain,frwl));
	    case IP_FW_DEL_BLK:
		return(del_from_chain(&ip_fw_blk_chain,frwl));
	    case IP_FW_DEL_FWD: 
		return(del_from_chain(&ip_fw_fwd_chain,frwl));
	    default:
		/*
 		 * Should be panic but...
		 */
		dprintf2("ip_fw_ctl:  unknown request %d\n",stage);
		return(EINVAL);
	    }
} 

dprintf2("ip_fw_ctl:  unknown request %d\n",stage);
return(EINVAL);
}
#endif /* IPFIREWALL */
