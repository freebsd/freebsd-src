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

struct ip_firewall *ip_fw_fwd_chain;
struct ip_firewall *ip_fw_blk_chain;
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
 * Returns 0 if packet should be dropped, 1 if it should be accepted
 */


int ip_firewall_check_print(ip,chain)
struct ip *ip;
struct ip_firewall *chain;
{
    if ( !ip_firewall_check_noprint(ip,chain) ) {

	u_short *portptr = (u_short *)&(((u_int *)ip)[ip->ip_hl]);

	printf("ip_firewall_check says no to ");
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
    return(1);
}

int ip_firewall_check_noprint(ip,chain)
struct ip *ip;
struct ip_firewall *chain;
{
    struct in_addr src, dst;
    char got_proto = 0;
    int firewall_proto, proto = 0;
    register struct ip_firewall *fptr;
    u_short src_port = 0, dst_port = 0;

    if ( chain == NULL ) {	/* Is there a firewall chain? */
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

	    if ( (firewall_proto = fptr->flags & IP_FIREWALL_KIND) == IP_FIREWALL_UNIVERSAL ) {

		/* Universal firewall - we've got a match! */

#ifdef DEBUG_IPFIREWALL
		printf("universal firewall match\n");
#endif
#ifdef olf
		return( (fptr->flags & IP_FIREWALL_ACCEPT) == IP_FIREWALL_ACCEPT );
#else
		return( fptr->flags & IP_FIREWALL_ACCEPT );
#endif

	    } else {

	/* Specific firewall - packet's protocol must match firewall's */

	if ( !got_proto ) {
	    u_short *portptr = (u_short *)&(((u_int *)ip)[ip->ip_hl]);
	    switch( ip->ip_p ) {
	    case IPPROTO_TCP:
		proto = IP_FIREWALL_TCP;
		src_port = ntohs(portptr[0]);	/* first two shorts in TCP */
		dst_port = ntohs(portptr[1]);	/* are src and dst ports */
		break;
	    case IPPROTO_UDP:
		proto = IP_FIREWALL_UDP;
		src_port = ntohs(portptr[0]);	/* first two shorts in UDP */
		dst_port = ntohs(portptr[1]);	/* are src and dst ports */
		break;
	    case IPPROTO_ICMP:
		proto = IP_FIREWALL_ICMP;
		break;
	    default: proto = IP_FIREWALL_UNIVERSAL;
#ifdef DEBUG_IPFIREWALL
		printf("non TCP/UDP packet\n");
#endif
		    }
		    got_proto = 1;
		}

		if ( proto == firewall_proto ) {

		    if (
			proto == IP_FIREWALL_ICMP
		    ||
			(
			    (
				fptr->num_src_ports == 0
			    ||
				port_match( &fptr->ports[0],
					    fptr->num_src_ports,
					    src_port,
					    fptr->flags & IP_FIREWALL_SRC_RANGE
					  )
			    )
			&&
			    (
				fptr->num_dst_ports == 0
			    ||
				port_match( &fptr->ports[fptr->num_src_ports],
					    fptr->num_dst_ports,
					    dst_port,
					    fptr->flags & IP_FIREWALL_DST_RANGE
					  )
			    )
			)
		    ) {

#ifdef old
			return( (fptr->flags & IP_FIREWALL_ACCEPT) == IP_FIREWALL_ACCEPT );

#else
			return( fptr->flags & IP_FIREWALL_ACCEPT);
#endif
		    }

		}

	    }

	}

    }

    /*
     * If we get here then none of the firewalls matched.
     * If the first firewall was an accept firewall then reject the packet.
     * If the first firewall was a deny firewall then accept the packet.
     *
     * The basic idea is that there is a virtual final firewall which is
     * the exact complement of the first firewall (this idea is a slight
     * variant of the way that the Telebit's Netblazer IP filtering scheme
     * handles this case).
     */

#ifdef old
    return( ((chain->flags) & IP_FIREWALL_ACCEPT) != IP_FIREWALL_ACCEPT );
#else 
    return(ip_fw_policy);
#endif

}


static
void
free_firewall_chain(chainptr)
struct ip_firewall **chainptr;
{
    while ( *chainptr != NULL ) {
	struct ip_firewall *ftmp;
	ftmp = *chainptr;
	*chainptr = ftmp->next;
	free(ftmp,M_SOOPTS);
    }
}

static
int
add_to_chain(chainptr,firewall)
struct ip_firewall **chainptr;
struct ip_firewall *firewall;
{
    struct ip_firewall *ftmp;
    struct ip_firewall *chaintmp=NULL;

    ftmp = malloc( sizeof(struct ip_firewall), M_SOOPTS, M_DONTWAIT );
    if ( ftmp == NULL ) {
	printf("ip_firewall_ctl:  malloc said no\n");
	return( ENOSPC );
    }

    bcopy( firewall, ftmp, sizeof( struct ip_firewall ) );
    ftmp->next = NULL;

    if (*chainptr==NULL)
       {
        *chainptr=ftmp;
       }
    else
       {
	/*
	 * This made so to get firewall behavior more *human* oriented-
	 * as to speed up the packet check the first firewall matching
	 * the packet used to determine ALLOW/DENY condition,and one
 	 * tends to set up more specific firewall later then more general
	 * this change allows adding firewalls to the head of chain so
	 * that the first matching is last added in line of matching 
	 * firewalls.This change is not real turn in behavior but helps
	 * to use firewall efficiently.
	 */
#ifdef old
        chaintmp=*chainptr;
        while(chaintmp->next!=NULL)
           chaintmp=chaintmp->next;
        chaintmp->next=ftmp;
#else
	chaintmp=*chainptr;
	*chainptr=ftmp;
	ftmp->next=chaintmp;
#endif
       }
    return(0);
}

static
int
del_from_chain(chainptr,firewall)
struct ip_firewall **chainptr;
struct ip_firewall *firewall;
{
    struct ip_firewall *ftmp,*ltmp;
    u_short	tport1,tport2,tmpnum;
    char	matches,was_found;

    ftmp=*chainptr;

    if ( ftmp == NULL ) {
	printf("ip_firewall_ctl:  chain is empty\n");
	return( EINVAL );
    			}

    ltmp=NULL;
    was_found=0;

    while( ftmp != NULL )
    {
     matches=1;
     if ((bcmp(&ftmp->src,&firewall->src,sizeof(struct in_addr))) 
     || (bcmp(&ftmp->src_mask,&firewall->src_mask,sizeof(struct in_addr)))
     || (bcmp(&ftmp->dst,&firewall->dst,sizeof(struct in_addr)))
     || (bcmp(&ftmp->dst_mask,&firewall->dst_mask,sizeof(struct in_addr)))
     || (ftmp->flags!=firewall->flags))
        matches=0;
     tport1=ftmp->num_src_ports+ftmp->num_dst_ports;
     tport2=firewall->num_src_ports+firewall->num_dst_ports;
     if (tport1!=tport2)
        matches=0;
     else
      if (tport1!=0)
      {
       for (tmpnum=0;tmpnum < tport1 && tmpnum < IP_FIREWALL_MAX_PORTS;tmpnum++)
        if (ftmp->ports[tmpnum]!=firewall->ports[tmpnum])
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
              /* return 0; */
            }
        else
            {
             *chainptr=ftmp->next; 
             free(ftmp,M_SOOPTS);
             ftmp=*chainptr;
             /* return 0; */
            }
       
      }
    else
      {
       ltmp = ftmp;
       ftmp = ftmp->next;
      }
    }
    if (was_found) return 0;
    else return(EINVAL);
}

int
ip_firewall_ctl(stage,m)
int stage;	
struct mbuf *m;
{
int *tmp_policy_ptr;
if ( stage == IP_FW_FLUSH )
       {
	free_firewall_chain(&ip_fw_blk_chain);
	free_firewall_chain(&ip_fw_fwd_chain);
	return(0);
       }  

	if ( m == 0 )	
          {
	    printf("ip_firewall_ctl:  NULL mbuf ptr\n");
	    return( EINVAL );
          }

if ( stage == IP_FW_POLICY )
      {
	tmp_policy_ptr=mtod(m,int *);
	ip_fw_policy=*tmp_policy_ptr;
	return 0;
      }

	if ( stage == IP_FW_CHK_BLK || stage == IP_FW_CHK_FWD ) {

	    struct ip *ip;
	    if ( m->m_len < sizeof(struct ip) + 2 * sizeof(u_short) )	{
		printf("ip_firewall_ctl:  mbuf len=%d, want at least %d\n",m->m_len,sizeof(struct ip) + 2 * sizeof(u_short));
		return( EINVAL );
	    								}
	    ip = mtod(m,struct ip *);
	    if ( ip->ip_hl != sizeof(struct ip) / sizeof(int) )	{
		printf("ip_firewall_ctl:  ip->ip_hl=%d, want %d\n",ip->ip_hl,sizeof(struct ip)/sizeof(int));
		return( EINVAL );
	    							}
	    if ( ip_firewall_check(ip,
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

	    struct ip_firewall *firewall;

	    if ( m->m_len != sizeof(struct ip_firewall) )	{
		printf("ip_firewall_ctl:  len=%d, want %d\n",m->m_len,sizeof(struct ip_firewall));
		return( EINVAL );
	    							}

	    firewall = mtod(m,struct ip_firewall*);
	    if ( (firewall->flags & ~IP_FIREWALL_FLAG_BITS) != 0 )	{
		printf("ip_firewall_ctl:  undefined flag bits set (flags=%x)\n",firewall->flags);
		return( EINVAL );
	    								}

	    if ( (firewall->flags & IP_FIREWALL_SRC_RANGE) && firewall->num_src_ports < 2 ) {
		printf("ip_firewall_ctl:  SRC_RANGE set but num_src_ports=%d\n",firewall->num_src_ports);
		return( EINVAL );
	    }

	    if ( (firewall->flags & IP_FIREWALL_DST_RANGE) && firewall->num_dst_ports < 2 ) {
		printf("ip_firewall_ctl:  DST_RANGE set but num_dst_ports=%d\n",firewall->num_dst_ports);
		return( EINVAL );
	    }

	    if ( firewall->num_src_ports + firewall->num_dst_ports > IP_FIREWALL_MAX_PORTS ) {
		printf("ip_firewall_ctl:  too many ports (%d+%d)\n",firewall->num_src_ports,firewall->num_dst_ports);
		return( EINVAL );
	    }

#if 0
	    if ( (firewall->flags & IP_FIREWALL_KIND) == IP_FIREWALL_ICMP ) {
		printf("ip_firewall_ctl:  request for unsupported ICMP firewalling\n");
		return( EINVAL );
	    }

#endif
	    if ( stage == IP_FW_ADD_BLK )
               {
		return( add_to_chain(&ip_fw_blk_chain,firewall));
	       } 
	    if ( stage == IP_FW_ADD_FWD )
               {
		return( add_to_chain(&ip_fw_fwd_chain,firewall));
	       } 
	    if ( stage == IP_FW_DEL_BLK )
               {
		return( del_from_chain(&ip_fw_blk_chain,firewall));
	       } 
	    if ( stage == IP_FW_DEL_FWD )
               {
		return( del_from_chain(&ip_fw_fwd_chain,firewall));
	       } 
	} 

printf("ip_firewall_ctl:  unknown request %d\n",stage);
return(EINVAL);

}
