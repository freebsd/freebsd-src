/*
 * Written by Atsushi Murai <amurai@spec.co.jp>
 *
 * Copyright (C) 1998, System Planning and Engineering Co. All rights reserverd.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the System Planning and Engineering Co.  The name of the
 * SPEC may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * $Id:$
 *
 *  TODO:
 *       oClean up. 
 *       oConsidering for word alignment for other platform.
 */
/*
    alias_nbt.c performs special processing for NetBios over TCP/IP
    sessions by UDP.

    Initial version:  May, 1998  (Atsushi Murai <amurai@spec.co.jp>)

    See HISTORY file for record of revisions.
*/

/* Includes */
#include <ctype.h>
#include <stdio.h> 
#include <string.h>
#include <sys/types.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>

#include "alias_local.h"

#define ADJUST_CHECKSUM(acc, cksum) { \
    acc += cksum; \
    if (acc < 0) \
    { \
        acc = -acc; \
        acc = (acc >> 16) + (acc & 0xffff); \
        acc += acc >> 16; \
        cksum = (u_short) ~acc; \
    } \
    else \
    { \
        acc = (acc >> 16) + (acc & 0xffff); \
        acc += acc >> 16; \
        cksum = (u_short) acc; \
    } \
}

typedef struct {
	struct in_addr		oldaddr;
	u_short 			oldport;
	struct in_addr		newaddr;
	u_short 			newport;
	u_short 			*uh_sum;
} NBTArguments;

typedef struct {
	unsigned char   type;
	unsigned char   flags;
	u_short  		id;
	struct in_addr  source_ip;
	u_short			source_port;
	u_short			len;
	u_short			offset;
} NbtDataHeader;

#define OpQuery		0
#define OpUnknown	4
#define OpRegist	5
#define OpRelease	6
#define OpWACK		7
#define OpRefresh	8
typedef struct {
	u_short			nametrid;
	u_short 		dir:1, opcode:4, nmflags:7, rcode:4;
	u_short			qdcount;
	u_short			ancount;
	u_short			nscount;
	u_short			arcount;
} NbtNSHeader;

#define FMT_ERR		0x1
#define SRV_ERR		0x2
#define IMP_ERR		0x4
#define RFS_ERR		0x5
#define ACT_ERR		0x6
#define CFT_ERR		0x7

/*******************************************************************
 * copy an IP address from one buffer to another                   *
 *******************************************************************/
void putip(void *dest,void *src)
{
  memcpy(dest,src,4);
}

void PrintRcode( u_char rcode )  {

	switch (rcode) {
		case FMT_ERR:
			printf("\nFormat Error.");
		case SRV_ERR:
			printf("\nSever failure.");
		case IMP_ERR:
			printf("\nUnsupported request error.\n");
		case RFS_ERR:
			printf("\nRefused error.\n");
		case ACT_ERR:
			printf("\nActive error.\n");
		case CFT_ERR:
			printf("\nName in conflict error.\n");
		default:
			printf("\n???=%0x\n", rcode );

	}	
}


/* Handling Name field */
u_char *AliasHandleName ( u_char *p ) {

	u_char *s;
	u_char c;
	int		compress;

	/* Following length field */
	if (*p & 0xc0 ) {
		p = p + 2;
		return ((u_char *)p);
	}
	while ( ( *p & 0x3f) != 0x00 ) {
		s = p + 1;
		if ( *p == 0x20 )
			compress = 1;
		else
			compress = 0;
		
	 	/* Get next length field */
		p = (u_char *)(p + (*p & 0x3f) + 1);
#ifdef DEBUG
		printf(":");
#endif
		while (s < p) {
			if ( compress == 1 ) {
				c = (u_char )(((((*s & 0x0f) << 4) | (*(s+1) & 0x0f)) - 0x11));
#ifdef DEBUG
				if (isprint( c ) )
					printf("%c", c );
				else
					printf("<0x%02x>", c );
#endif
				s +=2;
			} else {
#ifdef DEBUG
				printf("%c", *s);
#endif
				s++;
			}
		}
#ifdef DEBUG
		printf(":");
#endif
		fflush(stdout);
    }

	/* Set up to out of Name field */
	p++;
	return ((u_char *)p);
}

/* 
 * NetBios Datagram Handler (IP/UDP)
 */
#define DGM_DIRECT_UNIQ		0x10
#define DGM_DIRECT_GROUP	0x11
#define DGM_BROADCAST		0x12
#define DGM_ERROR			0x13
#define DGM_QUERY			0x14
#define DGM_POSITIVE_RES	0x15
#define DGM_NEGATIVE_RES	0x16

void AliasHandleUdpNbt(
	struct ip 		  	*pip,	 /* IP packet to examine/patch */
	struct alias_link 	*link,
	struct in_addr		*alias_address,
	u_short 			alias_port )
{
    struct udphdr *	uh;
    NbtDataHeader 	*ndh;
	u_char			*p;
        
    /* Calculate data length of UDP packet */
    uh =  (struct udphdr *) ((char *) pip + (pip->ip_hl << 2));
	ndh = (NbtDataHeader *)((char *)uh + (sizeof (struct udphdr)));
#ifdef DEBUG
	printf("\nType=%02x,", ndh->type );
#endif
	switch ( ndh->type ) {
		case DGM_DIRECT_UNIQ:
		case DGM_DIRECT_GROUP:
		case DGM_BROADCAST:
			p = (u_char *)ndh + 14;
			p = AliasHandleName ( p ); /* Source Name */
			p = AliasHandleName ( p ); /* Destination Name */
			break;
		case DGM_ERROR:
			p = (u_char *)ndh + 11;
			break;
		case DGM_QUERY:
		case DGM_POSITIVE_RES:
		case DGM_NEGATIVE_RES:
			p = (u_char *)ndh + 10;
			p = AliasHandleName ( p ); /* Destination Name */
			break;
	}
#ifdef DEBUG
	printf("%s:%d-->", inet_ntoa(ndh->source_ip), ntohs(ndh->source_port) );
#endif
	/* Doing a IP address and Port number Translation */
	if ( uh->uh_sum != 0 ) {
		int				acc;
		u_short			*sptr;
		acc  = ndh->source_port;
		acc -= alias_port;
		sptr = (u_short *) &(ndh->source_ip);
		acc += *sptr++;
		acc += *sptr;
		sptr = (u_short *) alias_address;
		acc -= *sptr++;
		acc -= *sptr;
		ADJUST_CHECKSUM(acc, uh->uh_sum)
	}
    ndh->source_ip = *alias_address;
    ndh->source_port = alias_port;
#ifdef DEBUG
	printf("%s:%d\n", inet_ntoa(ndh->source_ip), ntohs(ndh->source_port) );
	fflush(stdout);
#endif
}
/* Question Section */
#define QS_TYPE_NB		0x0020
#define QS_TYPE_NBSTAT	0x0021
#define QS_CLAS_IN		0x0001
typedef struct {
	u_short	type;	/* The type of Request */
	u_short	class;	/* The class of Request */
} NBTNsQuestion;

u_char *AliasHandleQuestion(u_short count,
							NBTNsQuestion *q,
							NBTArguments  *nbtarg)
{

	while ( count != 0 ) {
		/* Name Filed */
		q = (NBTNsQuestion *)AliasHandleName((u_char *)q );

		/* Type and Class filed */
		switch ( ntohs(q->type) ) {
			case QS_TYPE_NB:
			case QS_TYPE_NBSTAT:
				q= q+1;
			break;
			default:
				printf("\nUnknown Type on Question %0x\n", ntohs(q->type) );
			break;
		}
		count--;
	}

	/* Set up to out of Question Section */
	return ((u_char *)q);
}

/* Resource Record */
#define RR_TYPE_A		0x0001
#define RR_TYPE_NS		0x0002
#define RR_TYPE_NULL	0x000a
#define RR_TYPE_NB		0x0020
#define RR_TYPE_NBSTAT	0x0021
#define RR_CLAS_IN		0x0001
#define SizeOfNsResource	8
typedef struct {
 	u_short type;
 	u_short class;
 	unsigned int ttl;
 	u_short rdlen;
} NBTNsResource;

#define SizeOfNsRNB			6
typedef struct {
	u_short g:1, ont:2, resv:13;
	struct	in_addr	addr;
} NBTNsRNB;

u_char *AliasHandleResourceNB( NBTNsResource *q,
							   NBTArguments  *nbtarg)
{
	NBTNsRNB	*nb;
	u_short bcount;

	/* Check out a length */
	bcount = ntohs(q->rdlen);

	/* Forward to Resource NB position */
	nb = (NBTNsRNB *)((u_char *)q + SizeOfNsResource);

	/* Processing all in_addr array */
#ifdef DEBUG
	printf("NB rec[%s", inet_ntoa(nbtarg->oldaddr));
            printf("->%s, %dbytes] ",inet_ntoa(nbtarg->newaddr ), bcount);
#endif
	while ( bcount != 0 )  {
#ifdef DEBUG
		printf("<%s>", inet_ntoa(nb->addr) );
#endif
		if (!bcmp(&nbtarg->oldaddr,&nb->addr, sizeof(struct in_addr) ) ) {
			if ( *nbtarg->uh_sum != 0 ) {
            	int acc;
            	u_short *sptr;

            	sptr = (u_short *) &(nb->addr);
            	acc = *sptr++;
            	acc += *sptr;
            	sptr = (u_short *) &(nbtarg->newaddr);
            	acc -= *sptr++;
            	acc -= *sptr;
            	ADJUST_CHECKSUM(acc, *nbtarg->uh_sum)
			}

			nb->addr = nbtarg->newaddr;
#ifdef DEBUG
			printf("O");
#endif
		}
#ifdef DEBUG
		 else {
			printf(".");
		}
#endif
		nb=(NBTNsRNB *)((u_char *)nb + SizeOfNsRNB);
	 	bcount -= SizeOfNsRNB;
	}

	return ((u_char *)nb);
}

#define SizeOfResourceA		6
typedef struct {
	struct	in_addr	addr;
} NBTNsResourceA;

u_char *AliasHandleResourceA( NBTNsResource *q,
						 	  NBTArguments  *nbtarg)
{
	NBTNsResourceA	*a;
	u_short bcount;

	/* Forward to Resource A position */
	a = (NBTNsResourceA *)( (u_char *)q + sizeof(NBTNsResource) );

	/* Check out of length */
	bcount = ntohs(q->rdlen);

	/* Processing all in_addr array */
#ifdef DEBUG
	printf("Arec [%s", inet_ntoa(nbtarg->oldaddr));
        printf("->%s]",inet_ntoa(nbtarg->newaddr ));
#endif
	while ( bcount != 0 )  {
#ifdef DEBUG
		printf("..%s", inet_ntoa(a->addr) );
#endif
		if ( !bcmp(&nbtarg->oldaddr, &a->addr, sizeof(struct in_addr) ) ) {
			if ( *nbtarg->uh_sum != 0 ) {
            	int acc;
            	u_short *sptr;

            	sptr = (u_short *) &(a->addr);		 /* Old */
            	acc = *sptr++;
            	acc += *sptr;
            	sptr = (u_short *) &nbtarg->newaddr; /* New */
            	acc -= *sptr++;
            	acc -= *sptr;
            	ADJUST_CHECKSUM(acc, *nbtarg->uh_sum)
			}

			a->addr = nbtarg->newaddr;
		}
		a++;	/*XXXX*/
		bcount -= SizeOfResourceA;
	}
	return ((u_char *)a);
}

typedef struct {
	u_short opcode:4, flags:8, resv:4;
} NBTNsResourceNULL;

u_char *AliasHandleResourceNULL( NBTNsResource *q,
						 	     NBTArguments  *nbtarg)
{
	NBTNsResourceNULL	*n;
	u_short bcount;

	/* Forward to Resource NULL position */
	n = (NBTNsResourceNULL *)( (u_char *)q + sizeof(NBTNsResource) );

	/* Check out of length */
	bcount = ntohs(q->rdlen);

	/* Processing all in_addr array */
	while ( bcount != 0 )  {
		n++;
		bcount -= sizeof(NBTNsResourceNULL);
	}

	return ((u_char *)n);
}

u_char *AliasHandleResourceNS( NBTNsResource *q,
						 	     NBTArguments  *nbtarg)
{
	NBTNsResourceNULL	*n;
	u_short bcount;

	/* Forward to Resource NULL position */
	n = (NBTNsResourceNULL *)( (u_char *)q + sizeof(NBTNsResource) );

	/* Check out of length */
	bcount = ntohs(q->rdlen);

	/* Resource Record Name Filed */
	q = (NBTNsResource *)AliasHandleName( (u_char *)n ); /* XXX */

	return ((u_char *)n + bcount);
}

typedef struct {
	u_short	numnames;
} NBTNsResourceNBSTAT;

u_char *AliasHandleResourceNBSTAT( NBTNsResource *q,
						 	       NBTArguments  *nbtarg)
{
	NBTNsResourceNBSTAT	*n;
	u_short bcount;

	/* Forward to Resource NBSTAT position */
	n = (NBTNsResourceNBSTAT *)( (u_char *)q + sizeof(NBTNsResource) );

	/* Check out of length */
	bcount = ntohs(q->rdlen);

	return ((u_char *)n + bcount);
}

u_char *AliasHandleResource(u_short count,
							NBTNsResource *q,
							NBTArguments  *nbtarg)
{
	while ( count != 0 ) {
		/* Resource Record Name Filed */
		q = (NBTNsResource *)AliasHandleName( (u_char *)q );
#ifdef DEBUG
		printf("type=%02x, count=%d\n", ntohs(q->type), count );
#endif

		/* Type and Class filed */
		switch ( ntohs(q->type) ) {
			case RR_TYPE_NB:
				q = (NBTNsResource *)AliasHandleResourceNB( q, nbtarg );
				break;
			case RR_TYPE_A: 
				q = (NBTNsResource *)AliasHandleResourceA( q, nbtarg );
				break;
			case RR_TYPE_NS:
				q = (NBTNsResource *)AliasHandleResourceNS( q, nbtarg );
				break;
			case RR_TYPE_NULL:
				q = (NBTNsResource *)AliasHandleResourceNULL( q, nbtarg );
				break;
			case RR_TYPE_NBSTAT:
				q = (NBTNsResource *)AliasHandleResourceNBSTAT( q, nbtarg );
				break;
			default: printf("\nUnknown Type of Resource %0x\n",
								 ntohs(q->type) );
				break;
		}
		count--;
	}
	fflush(stdout);
	return ((u_char *)q);
}

void AliasHandleUdpNbtNS(
	struct ip 		  	*pip,	 /* IP packet to examine/patch */
	struct alias_link 	*link,
	struct in_addr		*alias_address,
	u_short 			*alias_port,
	struct in_addr		*original_address,
	u_short 			*original_port )
{
    struct udphdr *	uh;
	NbtNSHeader	  * nsh;
	u_short		    dlen;
	u_char		  * p;
	NBTArguments    nbtarg;

	/* Set up Common Parameter */	
	nbtarg.oldaddr	=	*alias_address;
	nbtarg.oldport	=	*alias_port;
	nbtarg.newaddr	=	*original_address;
	nbtarg.newport	=	*original_port;

    /* Calculate data length of UDP packet */
    uh =  (struct udphdr *) ((char *) pip + (pip->ip_hl << 2));
	nbtarg.uh_sum	=	&(uh->uh_sum);
    dlen = ntohs( uh->uh_ulen );
	nsh = (NbtNSHeader *)((char *)uh + (sizeof(struct udphdr)));
	p = (u_char *)(nsh + 1);

#ifdef DEBUG
	printf(" [%s] ID=%02x, op=%01x, flag=%02x, rcode=%01x, qd=%04x, an=%04x, ns=%04x, ar=%04x, [%d]-->", 
		nsh->dir ? "Response": "Request",
		nsh->nametrid,
		nsh->opcode,
		nsh->nmflags,
		nsh->rcode,
		ntohs(nsh->qdcount),
		ntohs(nsh->ancount),
		ntohs(nsh->nscount),
		ntohs(nsh->arcount),
		(u_char *)p -(u_char *)nsh);
#endif

	/* Question Entries */
	if (ntohs(nsh->qdcount) !=0 ) {
	p = AliasHandleQuestion(ntohs(nsh->qdcount), (NBTNsQuestion *)p, &nbtarg );
	}

	/* Answer Resource Records */
	if (ntohs(nsh->ancount) !=0 ) {
	p = AliasHandleResource(ntohs(nsh->ancount), (NBTNsResource *)p, &nbtarg );
	}

	/* Authority Resource Recodrs */
	if (ntohs(nsh->nscount) !=0 ) {
	p = AliasHandleResource(ntohs(nsh->nscount), (NBTNsResource *)p, &nbtarg );
	}

	/* Additional Resource Recodrs */
	if (ntohs(nsh->arcount) !=0 ) {
	p = AliasHandleResource(ntohs(nsh->arcount), (NBTNsResource *)p, &nbtarg );
	}

#ifdef DEBUG
	 	PrintRcode(nsh->rcode);
#endif
	return;
}
