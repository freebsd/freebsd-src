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
#include <netinet/in_pcb.h>
#include <netinet/in_var.h>
#include <netinet/ip_var.h>
#include <netinet/ip_icmp.h>

#include <netinet/ip_fw.h>

struct ip_fw *ip_fw_fwd_chain;
struct ip_fw *ip_fw_blk_chain;
int ip_fw_policy=1;


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
    if ( range_flag ) {
	if ( portptr[0] <= port && port <= portptr[1] ) {
	    return( 1 );
	}
	nports -= 2;
	portptr += 2;
    }
    while ( nports-- > 0 ) {
	if ( *portptr++ == port ) {
	    return( 1 );
	}
    }
    return(0);
}


/*
 * Returns 0 if packet should be dropped, 1 or more if it should be accepted
 */


int ip_fw_chk(ip,chain)
struct ip *ip;
struct ip_fw *chain;
{
    struct in_addr src, dst;
    char got_proto = 0;
    int frwl_proto, proto = 0;
    register struct ip_fw *fptr;
    u_short src_port = 0, dst_port = 0;
#ifdef IPFIREWALL_VERBOSE
    u_short *portptr = (u_short *)&(((u_int *)ip)[ip->ip_hl]);
#endif

    if ( chain == NULL ) {	/* Is there a frwl chain? */
	return(1);
    }

    src = ip->ip_src;
    dst = ip->ip_dst;

#ifdef DEBUG_IPFIREWALL
    {
	u_short *portptr = (u_short *)&(((u_int *)ip)[ip->ip_hl]);
	printf("packet ");
	switch(ip->ip_p) {
	case IPPROTO_TCP: printf("TCP "); break;
	case IPPROTO_UDP: printf("UDP "); break;
	case IPPROTO_ICMP: printf("ICMP:%d ",((char *)portptr)[0]&0xff); break;
	default: printf("p=%d ",ip->ip_p); break;
	}
	print_ip(ip->ip_src);
	if ( ip->ip_p == IPPROTO_TCP || ip->ip_p == IPPROTO_UDP ) {
	    printf(":%d ",ntohs(portptr[0]));
	}
	print_ip(ip->ip_dst);
	if ( ip->ip_p == IPPROTO_TCP || ip->ip_p == IPPROTO_UDP ) {
	    printf(":%d ",ntohs(portptr[1]));
	}
	printf("\n");
    }
#endif

    for ( fptr = chain; fptr != NULL; fptr = fptr->next ) {

	if ( (src.s_addr & fptr->src_mask.s_addr) == fptr->src.s_addr
	&&   (dst.s_addr & fptr->dst_mask.s_addr) == fptr->dst.s_addr ) {

	    if ( (frwl_proto = fptr->flags & IP_FW_F_KIND)
					== IP_FW_F_ALL ) {

		/* Universal frwl - we've got a match! */

#ifdef DEBUG_IPFIREWALL
		printf("universal frwl match\n");
#endif
#ifdef IPFIREWALL_VERBOSE
		/*
		 * VERY ugly piece of code which actually
		 * makes kernel printf for denyed packets...
		 * This thingy will be added in more places...
		 */
    if (  !(fptr->flags & IP_FW_F_ACCEPT) &&
          (fptr->flags & IP_FW_F_PRN)) {
	printf("ip_fw_chk says no to ");
	switch(ip->ip_p) {
	case IPPROTO_TCP: printf("TCP "); break;
	case IPPROTO_UDP: printf("UDP "); break;
	case IPPROTO_ICMP: printf("ICMP:%d ",((char *)portptr)[0]&0xff); break;
	default: printf("p=%d ",ip->ip_p); break;
	}
	print_ip(ip->ip_src);
	if ( ip->ip_p == IPPROTO_TCP || ip->ip_p == IPPROTO_UDP ) {
	    printf(":%d ",ntohs(portptr[0]));
	} else {
	    printf("\n");
	}
	print_ip(ip->ip_dst);
	if ( ip->ip_p == IPPROTO_TCP || ip->ip_p == IPPROTO_UDP ) {
	    printf(":%d ",ntohs(portptr[1]));
	}
	printf("\n");
	return(0);
    }
#endif
		return( fptr->flags & IP_FW_F_ACCEPT );
	    } else {

	/* Specific frwl - packet's protocol must match frwl's */

	if ( !got_proto ) {
	    u_short *portptr = (u_short *)&(((u_int *)ip)[ip->ip_hl]);
	    switch( ip->ip_p ) {
	    case IPPROTO_TCP:
		proto = IP_FW_F_TCP;
		src_port = ntohs(portptr[0]);	/* first two shorts in TCP */
		dst_port = ntohs(portptr[1]);	/* are src and dst ports */
		break;
	    case IPPROTO_UDP:
		proto = IP_FW_F_UDP;
		src_port = ntohs(portptr[0]);	/* first two shorts in UDP */
		dst_port = ntohs(portptr[1]);	/* are src and dst ports */
		break;
	    case IPPROTO_ICMP:
		proto = IP_FW_F_ICMP;
		break;
	    default: proto = IP_FW_F_ALL;
#ifdef DEBUG_IPFIREWALL
		printf("non TCP/UDP packet\n");
#endif
		    }
		    got_proto = 1;
		}

		if ( proto == frwl_proto ) {

		    if (
			proto == IP_FW_F_ICMP
		    ||
			(
			    (
				fptr->n_src_p == 0
			    ||
				port_match( &fptr->ports[0],
					    fptr->n_src_p,
					    src_port,
					    fptr->flags & IP_FW_F_SRNG
					  )
			    )
			&&
			    (
				fptr->n_dst_p == 0
			    ||
				port_match( &fptr->ports[fptr->n_src_p],
					    fptr->n_dst_p,
					    dst_port,
					    fptr->flags & IP_FW_F_DRNG
					  )
			    )
			)
		    ) {

#ifdef IPFIREWALL_VERBOSE
		/*
		 * VERY ugly piece of code which actually
		 * makes kernel printf for denyed packets...
		 * This thingy will be added in more places...
		 */
    if (  !(fptr->flags & IP_FW_F_ACCEPT) &&
          (fptr->flags & IP_FW_F_PRN)) {
	printf("ip_fw_chk says no to ");
	switch(ip->ip_p) {
	case IPPROTO_TCP: printf("TCP "); break;
	case IPPROTO_UDP: printf("UDP "); break;
	case IPPROTO_ICMP: printf("ICMP:%d ",((char *)portptr)[0]&0xff); break;
	default: printf("p=%d ",ip->ip_p); break;
	}
	print_ip(ip->ip_src);
	if ( ip->ip_p == IPPROTO_TCP || ip->ip_p == IPPROTO_UDP ) {
	    printf(":%d ",ntohs(portptr[0]));
	} else {
	    printf("\n");
	}
	print_ip(ip->ip_dst);
	if ( ip->ip_p == IPPROTO_TCP || ip->ip_p == IPPROTO_UDP ) {
	    printf(":%d ",ntohs(portptr[1]));
	}
	printf("\n");
	return(0);
    }
#endif
			return( fptr->flags & IP_FW_F_ACCEPT);
		    }

		}

	    }

	}

    }

    /*
     * If we get here then none of the frwls matched.
     * So now we relay on policy defined by user-unmatched packet can
     * be ever accepted or rejected...
     */

#ifdef IPFIREWALL_VERBOSE
		/*
		 * VERY ugly piece of code which actually
		 * makes kernel printf for denyed packets...
		 * This thingy will be added in more places...
		 */
    if (  !(ip_fw_policy) &&
          (fptr->flags & IP_FW_F_PRN)) {
	printf("ip_fw_chk says no to ");
	switch(ip->ip_p) {
	case IPPROTO_TCP: printf("TCP "); break;
	case IPPROTO_UDP: printf("UDP "); break;
	case IPPROTO_ICMP: printf("ICMP:%d ",((char *)portptr)[0]&0xff); break;
	default: printf("p=%d ",ip->ip_p); break;
	}
	print_ip(ip->ip_src);
	if ( ip->ip_p == IPPROTO_TCP || ip->ip_p == IPPROTO_UDP ) {
	    printf(":%d ",ntohs(portptr[0]));
	} else {
	    printf("\n");
	}
	print_ip(ip->ip_dst);
	if ( ip->ip_p == IPPROTO_TCP || ip->ip_p == IPPROTO_UDP ) {
	    printf(":%d ",ntohs(portptr[1]));
	}
	printf("\n");
	return(0);
    }
#endif
    return(ip_fw_policy);

}


static
void
free_fw_chain(chainptr)
struct ip_fw **chainptr;
{
int s=splnet();
    while ( *chainptr != NULL ) {
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

    ftmp = malloc( sizeof(struct ip_fw), M_SOOPTS, M_DONTWAIT );
    if ( ftmp == NULL ) {
#ifdef DEBUG_IPFIREWALL
	printf("ip_fw_ctl:  malloc said no\n");
#endif
	splx(s);
	return( ENOSPC );
    }

    bcopy( frwl, ftmp, sizeof( struct ip_fw ) );
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
		&&  oldkind!=newkind)
			continue;
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

    if ( ftmp == NULL ) {
#ifdef DEBUG_IPFIREWALL
	printf("ip_fw_ctl:  chain is empty\n");
#endif
	splx(s);
	return( EINVAL );
    			}

    ltmp=NULL;
    was_found=0;

    while( ftmp != NULL )
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

int
ip_fw_ctl(stage,m)
int stage;	
struct mbuf *m;
{
int *tmp_policy_ptr;
if ( stage == IP_FW_FLUSH )
       {
	free_fw_chain(&ip_fw_blk_chain);
	free_fw_chain(&ip_fw_fwd_chain);
	return(0);
       }  

	if ( m == 0 )	
          {
	    printf("ip_fw_ctl:  NULL mbuf ptr\n");
	    return( EINVAL );
          }

if ( stage == IP_FW_POLICY )
      {
	tmp_policy_ptr=mtod(m,int *);
	if ((*tmp_policy_ptr)!=1 && (*tmp_policy_ptr)!=0)
		return ( EINVAL );
	ip_fw_policy=*tmp_policy_ptr;
	return 0;
      }

	if ( stage == IP_FW_CHK_BLK || stage == IP_FW_CHK_FWD ) {

	    struct ip *ip;
	    if ( m->m_len < sizeof(struct ip) + 2 * sizeof(u_short) )	{
#ifdef DEBUG_IPFIREWALL
		printf("ip_fw_ctl:  mbuf len=%d, want at least %d\n",m->m_len,sizeof(struct ip) + 2 * sizeof(u_short));
#endif
		return( EINVAL );
	    								}
	    ip = mtod(m,struct ip *);
	    if ( ip->ip_hl != sizeof(struct ip) / sizeof(int) )	{
#ifdef DEBUG_IPFIREWALL
		printf("ip_fw_ctl:  ip->ip_hl=%d, want %d\n",ip->ip_hl,sizeof(struct ip)/sizeof(int));
#endif
		return( EINVAL );
	    							}
	    if ( ip_fw_chk(ip,
		stage == IP_FW_CHK_BLK ?
                ip_fw_blk_chain : ip_fw_fwd_chain )
	       ) 
		return(0);
	    	else	{
		return(EACCES);
	    		}

    } else if ( stage == IP_FW_ADD_BLK
	       	 || stage == IP_FW_ADD_FWD
       		 || stage == IP_FW_DEL_BLK
       		 || stage == IP_FW_DEL_FWD
                  ) {

	    struct ip_fw *frwl;

	    if ( m->m_len != sizeof(struct ip_fw) )	{
#ifdef DEBUG_IPFIREWALL
		printf("ip_fw_ctl:  len=%d, want %d\n",m->m_len,sizeof(struct ip_fw));
#endif
		return( EINVAL );
	    							}

	    frwl = mtod(m,struct ip_fw*);
	    if ( (frwl->flags & ~IP_FW_F_MASK) != 0 )	{
#ifdef DEBUG_IPFIREWALL
		printf("ip_fw_ctl:  undefined flag bits set (flags=%x)\n",frwl->flags);
#endif
		return( EINVAL );
	    								}

	    if ( (frwl->flags & IP_FW_F_SRNG) && frwl->n_src_p < 2 ) {
#ifdef DEBUG_IPFIREWALL
		printf("ip_fw_ctl:  src range set but n_src_p=%d\n",frwl->n_src_p);
#endif
		return( EINVAL );
	    }

	    if ( (frwl->flags & IP_FW_F_DRNG) && frwl->n_dst_p < 2 ) {
#ifdef DEBUG_IPFIREWALL
		printf("ip_fw_ctl:  dst range set but n_dst_p=%d\n",frwl->n_dst_p);
#endif
		return( EINVAL );
	    }

	    if ( frwl->n_src_p + frwl->n_dst_p > IP_FW_MAX_PORTS ) {
#ifdef DEBUG_IPFIREWALL
		printf("ip_fw_ctl:  too many ports (%d+%d)\n",frwl->n_src_p,frwl->n_dst_p);
#endif
		return( EINVAL );
	    }

#if 0
	    if ( (frwl->flags & IP_FW_F_KIND) == IP_FW_F_ICMP ) {
#ifdef DEBUG_IPFIREWALL
		printf("ip_fw_ctl:  request for unsupported ICMP frwling\n");
#endif
		return( EINVAL );
	    }

#endif
	    if ( stage == IP_FW_ADD_BLK )
               {
		return( add_to_chain(&ip_fw_blk_chain,frwl));
	       } 
	    if ( stage == IP_FW_ADD_FWD )
               {
		return( add_to_chain(&ip_fw_fwd_chain,frwl));
	       } 
	    if ( stage == IP_FW_DEL_BLK )
               {
		return( del_from_chain(&ip_fw_blk_chain,frwl));
	       } 
	    if ( stage == IP_FW_DEL_FWD )
               {
		return( del_from_chain(&ip_fw_fwd_chain,frwl));
	       } 
	} 

#ifdef DEBUG_IPFIREWALL
printf("ip_fw_ctl:  unknown request %d\n",stage);
#endif
return(EINVAL);

}
