/*
    Alias_util.h contains general utilities used by other functions
    in the packet aliasing module.  At the moment, there are functions
    for computing IP header and TCP packet checksums.

    The checksum routines are based upon example code in a Unix networking
    text written by Stevens (sorry, I can't remember the title -- but
    at least this is a good author).

    Initial Version:  August, 1996  (cjm)

    Version 1.7:  January 9, 1997
         Added differential checksum update function.
*/

/*
Note: the checksum routines assume that the actual checksum word has
been zeroed out.  If the checksum workd is filled with the proper value,
then these routines will give a result of zero (useful for testing
purposes);
*/
    
#include <sys/types.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>

#include "alias_local.h"

u_short
InternetChecksum(u_short *ptr, int nbytes)
{
    int sum, oddbyte;

    sum = 0;
    while (nbytes > 1)
    {
        sum += *ptr++;
        nbytes -= 2;
    }
    if (nbytes == 1)
    {
        oddbyte = 0;
        *((u_char *) &oddbyte) = *(u_char *) ptr;
        sum += oddbyte;
    }
    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    return(~sum);
}

u_short
IpChecksum(struct ip *pip)
{
    return( InternetChecksum((u_short *) pip, (pip->ip_hl << 2)) );

}

u_short 
TcpChecksum(struct ip *pip)
{
    u_short *ptr;
    struct tcphdr *tc;
    int nhdr, ntcp, nbytes;
    int sum, oddbyte;

    nhdr = pip->ip_hl << 2;
    ntcp = ntohs(pip->ip_len) - nhdr;

    tc = (struct tcphdr *) ((char *) pip + nhdr);
    ptr = (u_short *) tc;
    
/* Add up TCP header and data */
    nbytes = ntcp;
    sum = 0;
    while (nbytes > 1)
    {
        sum += *ptr++;
        nbytes -= 2;
    }
    if (nbytes == 1)
    {
        oddbyte = 0;
        *((u_char *) &oddbyte) = *(u_char *) ptr;
        sum += oddbyte;
    }

/* "Pseudo-header" data */
    ptr = (u_short *) &(pip->ip_dst);
    sum += *ptr++;
    sum += *ptr;
    ptr = (u_short *) &(pip->ip_src);
    sum += *ptr++;
    sum += *ptr;
    sum += htons((u_short) ntcp);
    sum += htons((u_short) pip->ip_p);

/* Roll over carry bits */
    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);

/* Return checksum */
    return((u_short) ~sum);
}


void
DifferentialChecksum(u_short *cksum, u_short *new, u_short *old, int n)
{
    int i;
    int accumulate;

    accumulate = *cksum;
    for (i=0; i<n; i++)
    {
        accumulate -= *new++;
        accumulate += *old++;
    }

    if (accumulate < 0)
    {
        accumulate = -accumulate;
        accumulate = (accumulate >> 16) + (accumulate & 0xffff);
        accumulate += accumulate >> 16;
        *cksum = (u_short) ~accumulate;
    }
    else
    {
        accumulate = (accumulate >> 16) + (accumulate & 0xffff);
        accumulate += accumulate >> 16;
        *cksum = (u_short) accumulate;
    }
}

