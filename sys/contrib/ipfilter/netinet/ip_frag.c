/*	$FreeBSD$	*/

/*
 * Copyright (C) 1993-2003 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 */
#if defined(KERNEL) || defined(_KERNEL)
# undef KERNEL
# undef _KERNEL
# define        KERNEL	1
# define        _KERNEL	1
#endif
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/file.h>
#ifdef __hpux
# include <sys/timeout.h>
#endif
#if !defined(_KERNEL)
# include <stdio.h>
# include <string.h>
# include <stdlib.h>
# define _KERNEL
# ifdef __OpenBSD__
struct file;
# endif
# include <sys/uio.h>
# undef _KERNEL
#endif
#if defined(_KERNEL) && (__FreeBSD_version >= 220000)
# include <sys/filio.h>
# include <sys/fcntl.h>
#else
# include <sys/ioctl.h>
#endif
#if !defined(linux)
# include <sys/protosw.h>
#endif
#include <sys/socket.h>
#if defined(_KERNEL)
# include <sys/systm.h>
# if !defined(__SVR4) && !defined(__svr4__)
#  include <sys/mbuf.h>
# endif
#endif
#if !defined(__SVR4) && !defined(__svr4__)
# if defined(_KERNEL) && !defined(__sgi)
#  include <sys/kernel.h>
# endif
#else
# include <sys/byteorder.h>
# ifdef _KERNEL
#  include <sys/dditypes.h>
# endif
# include <sys/stream.h>
# include <sys/kmem.h>
#endif
#include <net/if.h>
#ifdef sun
# include <net/af.h>
#endif
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#if !defined(linux)
# include <netinet/ip_var.h>
#endif
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>
#include "netinet/ip_compat.h"
#include <netinet/tcpip.h>
#include "netinet/ip_fil.h"
#include "netinet/ip_nat.h"
#include "netinet/ip_frag.h"
#include "netinet/ip_state.h"
#include "netinet/ip_auth.h"
#include "netinet/ip_proxy.h"
#if (__FreeBSD_version >= 300000)
# include <sys/malloc.h>
# if defined(_KERNEL)
#  ifndef IPFILTER_LKM
#   include <sys/libkern.h>
#   include <sys/systm.h>
#  endif
extern struct callout_handle fr_slowtimer_ch;
# endif
#endif
#if defined(__NetBSD__) && (__NetBSD_Version__ >= 104230000)
# include <sys/callout.h>
extern struct callout fr_slowtimer_ch;
#endif
#if defined(__OpenBSD__)
# include <sys/timeout.h>
extern struct timeout fr_slowtimer_ch;
#endif
/* END OF INCLUDES */

#if !defined(lint)
static const char sccsid[] = "@(#)ip_frag.c	1.11 3/24/96 (C) 1993-2000 Darren Reed";
static const char rcsid[] = "@(#)Id: ip_frag.c,v 2.77 2004/01/27 00:24:54 darrenr Exp";
#endif


static ipfr_t   *ipfr_list = NULL;
static ipfr_t   **ipfr_tail = &ipfr_list;
static ipfr_t	**ipfr_heads;

static ipfr_t   *ipfr_natlist = NULL;
static ipfr_t   **ipfr_nattail = &ipfr_natlist;
static ipfr_t	**ipfr_nattab;

static ipfr_t   *ipfr_ipidlist = NULL;
static ipfr_t   **ipfr_ipidtail = &ipfr_ipidlist;
static ipfr_t	**ipfr_ipidtab;

static ipfrstat_t ipfr_stats;
static int	ipfr_inuse = 0;
int		ipfr_size = IPFT_SIZE;

int	fr_ipfrttl = 120;	/* 60 seconds */
int	fr_frag_lock = 0;
int	fr_frag_init = 0;
u_long	fr_ticks = 0;


static ipfr_t *ipfr_newfrag __P((fr_info_t *, u_32_t, ipfr_t **));
static ipfr_t *fr_fraglookup __P((fr_info_t *, ipfr_t **));
static void fr_fragdelete __P((ipfr_t *, ipfr_t ***));


/* ------------------------------------------------------------------------ */
/* Function:    fr_fraginit                                                 */
/* Returns:     int - 0 == success, -1 == error                             */
/* Parameters:  Nil                                                         */
/*                                                                          */
/* Initialise the hash tables for the fragment cache lookups.               */
/* ------------------------------------------------------------------------ */
int fr_fraginit()
{
	KMALLOCS(ipfr_heads, ipfr_t **, ipfr_size * sizeof(ipfr_t *));
	if (ipfr_heads == NULL)
		return -1;
	bzero((char *)ipfr_heads, ipfr_size * sizeof(ipfr_t *));

	KMALLOCS(ipfr_nattab, ipfr_t **, ipfr_size * sizeof(ipfr_t *));
	if (ipfr_nattab == NULL)
		return -1;
	bzero((char *)ipfr_nattab, ipfr_size * sizeof(ipfr_t *));

	KMALLOCS(ipfr_ipidtab, ipfr_t **, ipfr_size * sizeof(ipfr_t *));
	if (ipfr_ipidtab == NULL)
		return -1;
	bzero((char *)ipfr_ipidtab, ipfr_size * sizeof(ipfr_t *));

	RWLOCK_INIT(&ipf_frag, "ipf fragment rwlock");
	fr_frag_init = 1;

	return 0;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_fragunload                                               */
/* Returns:     Nil                                                         */
/* Parameters:  Nil                                                         */
/*                                                                          */
/* Free all memory allocated whilst running and from initialisation.        */
/* ------------------------------------------------------------------------ */
void fr_fragunload()
{
	if (fr_frag_init == 1) {
		fr_fragclear();

		RW_DESTROY(&ipf_frag);
		fr_frag_init = 0;
	}

	if (ipfr_heads != NULL)
		KFREES(ipfr_heads, ipfr_size * sizeof(ipfr_t *));
	ipfr_heads = NULL;

	if (ipfr_nattab != NULL)
		KFREES(ipfr_nattab, ipfr_size * sizeof(ipfr_t *));
	ipfr_nattab = NULL;

	if (ipfr_ipidtab != NULL)
		KFREES(ipfr_ipidtab, ipfr_size * sizeof(ipfr_t *));
	ipfr_ipidtab = NULL;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_fragstats                                                */
/* Returns:     ipfrstat_t* - pointer to struct with current frag stats     */
/* Parameters:  Nil                                                         */
/*                                                                          */
/* Updates ipfr_stats with current information and returns a pointer to it  */
/* ------------------------------------------------------------------------ */
ipfrstat_t *fr_fragstats()
{
	ipfr_stats.ifs_table = ipfr_heads;
	ipfr_stats.ifs_nattab = ipfr_nattab;
	ipfr_stats.ifs_inuse = ipfr_inuse;
	return &ipfr_stats;
}


/* ------------------------------------------------------------------------ */
/* Function:    ipfr_newfrag                                                */
/* Returns:     ipfr_t * - pointer to fragment cache state info or NULL     */
/* Parameters:  fin(I)   - pointer to packet information                    */
/*              table(I) - pointer to frag table to add to                  */
/*                                                                          */
/* Add a new entry to the fragment cache, registering it as having come     */
/* through this box, with the result of the filter operation.               */
/* ------------------------------------------------------------------------ */
static ipfr_t *ipfr_newfrag(fin, pass, table)
fr_info_t *fin;
u_32_t pass;
ipfr_t *table[];
{
	ipfr_t *fra, frag;
	u_int idx, off;
	ip_t *ip;

	if (ipfr_inuse >= IPFT_SIZE)
		return NULL;

	if ((fin->fin_flx & (FI_FRAG|FI_BAD)) != FI_FRAG)
		return NULL;

	ip = fin->fin_ip;

	if (pass & FR_FRSTRICT)
		if ((ip->ip_off & IP_OFFMASK) != 0)
			return NULL;

	frag.ipfr_p = ip->ip_p;
	idx = ip->ip_p;
	frag.ipfr_id = ip->ip_id;
	idx += ip->ip_id;
	frag.ipfr_tos = ip->ip_tos;
	frag.ipfr_src.s_addr = ip->ip_src.s_addr;
	idx += ip->ip_src.s_addr;
	frag.ipfr_dst.s_addr = ip->ip_dst.s_addr;
	idx += ip->ip_dst.s_addr;
	frag.ipfr_ifp = fin->fin_ifp;
	idx *= 127;
	idx %= IPFT_SIZE;

	frag.ipfr_optmsk = fin->fin_fi.fi_optmsk & IPF_OPTCOPY;
	frag.ipfr_secmsk = fin->fin_fi.fi_secmsk;
	frag.ipfr_auth = fin->fin_fi.fi_auth;

	/*
	 * first, make sure it isn't already there...
	 */
	for (fra = table[idx]; (fra != NULL); fra = fra->ipfr_hnext)
		if (!bcmp((char *)&frag.ipfr_ifp, (char *)&fra->ipfr_ifp,
			  IPFR_CMPSZ)) {
			ipfr_stats.ifs_exists++;
			return NULL;
		}

	/*
	 * allocate some memory, if possible, if not, just record that we
	 * failed to do so.
	 */
	KMALLOC(fra, ipfr_t *);
	if (fra == NULL) {
		ipfr_stats.ifs_nomem++;
		return NULL;
	}

	if ((fra->ipfr_rule = fin->fin_fr) != NULL)
		fin->fin_fr->fr_ref++;

	/*
	 * Insert the fragment into the fragment table, copy the struct used
	 * in the search using bcopy rather than reassign each field.
	 * Set the ttl to the default.
	 */
	if ((fra->ipfr_hnext = table[idx]) != NULL)
		table[idx]->ipfr_hprev = &fra->ipfr_hnext;
	fra->ipfr_hprev = table + idx;
	fra->ipfr_data = NULL;
	table[idx] = fra;
	bcopy((char *)&frag.ipfr_ifp, (char *)&fra->ipfr_ifp, IPFR_CMPSZ);
	fra->ipfr_ttl = fr_ticks + fr_ipfrttl;

	/*
	 * Compute the offset of the expected start of the next packet.
	 */
	off = ip->ip_off & IP_OFFMASK;
	if (off == 0)
		fra->ipfr_seen0 = 1;
	fra->ipfr_off = off + (fin->fin_dlen >> 3);
	fra->ipfr_pass = pass;
	ipfr_stats.ifs_new++;
	ipfr_inuse++;
	return fra;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_newfrag                                                  */
/* Returns:     int - 0 == success, -1 == error                             */
/* Parameters:  fin(I)  - pointer to packet information                     */
/*                                                                          */
/* Add a new entry to the fragment cache table based on the current packet  */
/* ------------------------------------------------------------------------ */
int fr_newfrag(fin, pass)
u_32_t pass;
fr_info_t *fin;
{
	ipfr_t	*fra;

	if ((fin->fin_v != 4) || (fr_frag_lock != 0))
		return -1;

	WRITE_ENTER(&ipf_frag);
	fra = ipfr_newfrag(fin, pass, ipfr_heads);
	if (fra != NULL) {
		*ipfr_tail = fra;
		fra->ipfr_prev = ipfr_tail;
		ipfr_tail = &fra->ipfr_next;
		if (ipfr_list == NULL)
			ipfr_list = fra;
		fra->ipfr_next = NULL;
	}
	RWLOCK_EXIT(&ipf_frag);
	return fra ? 0 : -1;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_nat_newfrag                                              */
/* Returns:     int - 0 == success, -1 == error                             */
/* Parameters:  fin(I)  - pointer to packet information                     */
/*              nat(I)  - pointer to NAT structure                          */
/*                                                                          */
/* Create a new NAT fragment cache entry based on the current packet and    */
/* the NAT structure for this "session".                                    */
/* ------------------------------------------------------------------------ */
int fr_nat_newfrag(fin, pass, nat)
fr_info_t *fin;
u_32_t pass;
nat_t *nat;
{
	ipfr_t	*fra;

	if ((fin->fin_v != 4) || (fr_frag_lock != 0))
		return 0;

	WRITE_ENTER(&ipf_natfrag);
	fra = ipfr_newfrag(fin, pass, ipfr_nattab);
	if (fra != NULL) {
		fra->ipfr_data = nat;
		nat->nat_data = fra;
		*ipfr_nattail = fra;
		fra->ipfr_prev = ipfr_nattail;
		ipfr_nattail = &fra->ipfr_next;
		fra->ipfr_next = NULL;
	}
	RWLOCK_EXIT(&ipf_natfrag);
	return fra ? 0 : -1;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_ipid_newfrag                                             */
/* Returns:     int - 0 == success, -1 == error                             */
/* Parameters:  fin(I)  - pointer to packet information                     */
/*              ipid(I) - new IP ID for this fragmented packet              */
/*                                                                          */
/* Create a new fragment cache entry for this packet and store, as a data   */
/* pointer, the new IP ID value.                                            */
/* ------------------------------------------------------------------------ */
int fr_ipid_newfrag(fin, ipid)
fr_info_t *fin;
u_32_t ipid;
{
	ipfr_t	*fra;

	if ((fin->fin_v != 4) || (fr_frag_lock))
		return 0;

	WRITE_ENTER(&ipf_ipidfrag);
	fra = ipfr_newfrag(fin, 0, ipfr_ipidtab);
	if (fra != NULL) {
		fra->ipfr_data = (void *)ipid;
		*ipfr_ipidtail = fra;
		fra->ipfr_prev = ipfr_ipidtail;
		ipfr_ipidtail = &fra->ipfr_next;
		fra->ipfr_next = NULL;
	}
	RWLOCK_EXIT(&ipf_ipidfrag);
	return fra ? 0 : -1;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_fraglookup                                               */
/* Returns:     ipfr_t * - pointer to ipfr_t structure if there's a         */
/*                         matching entry in the frag table, else NULL      */
/* Parameters:  fin(I)   - pointer to packet information                    */
/*              table(I) - pointer to fragment cache table to search        */
/*                                                                          */
/* Check the fragment cache to see if there is already a record of this     */
/* packet with its filter result known.                                     */
/* ------------------------------------------------------------------------ */
static ipfr_t *fr_fraglookup(fin, table)
fr_info_t *fin;
ipfr_t *table[];
{
	ipfr_t *f, frag;
	u_int idx;
	ip_t *ip;

	if ((fin->fin_flx & (FI_FRAG|FI_BAD)) != FI_FRAG)
		return NULL;

	/*
	 * For fragments, we record protocol, packet id, TOS and both IP#'s
	 * (these should all be the same for all fragments of a packet).
	 *
	 * build up a hash value to index the table with.
	 */
	ip = fin->fin_ip;
	frag.ipfr_p = ip->ip_p;
	idx = ip->ip_p;
	frag.ipfr_id = ip->ip_id;
	idx += ip->ip_id;
	frag.ipfr_tos = ip->ip_tos;
	frag.ipfr_src.s_addr = ip->ip_src.s_addr;
	idx += ip->ip_src.s_addr;
	frag.ipfr_dst.s_addr = ip->ip_dst.s_addr;
	idx += ip->ip_dst.s_addr;
	frag.ipfr_ifp = fin->fin_ifp;
	idx *= 127;
	idx %= IPFT_SIZE;

	frag.ipfr_optmsk = fin->fin_fi.fi_optmsk & IPF_OPTCOPY;
	frag.ipfr_secmsk = fin->fin_fi.fi_secmsk;
	frag.ipfr_auth = fin->fin_fi.fi_auth;

	/*
	 * check the table, careful to only compare the right amount of data
	 */
	for (f = table[idx]; f; f = f->ipfr_hnext)
		if (!bcmp((char *)&frag.ipfr_ifp, (char *)&f->ipfr_ifp,
			  IPFR_CMPSZ)) {
			u_short	off;

			/*
			 * We don't want to let short packets match because
			 * they could be compromising the security of other
			 * rules that want to match on layer 4 fields (and
			 * can't because they have been fragmented off.)
			 * Why do this check here?  The counter acts as an
			 * indicator of this kind of attack, whereas if it was
			 * elsewhere, it wouldn't know if other matching
			 * packets had been seen.
			 */
			if (fin->fin_flx & FI_SHORT) {
				ATOMIC_INCL(ipfr_stats.ifs_short);
				continue;
			}

			/*
			 * XXX - We really need to be guarding against the
			 * retransmission of (src,dst,id,offset-range) here
			 * because a fragmented packet is never resent with
			 * the same IP ID# (or shouldn't).
			 */
			off = ip->ip_off & IP_OFFMASK;
			if (f->ipfr_seen0) {
				if (off == 0) {
					ATOMIC_INCL(ipfr_stats.ifs_retrans0);
					continue;
				}
			} else if (off == 0)
				f->ipfr_seen0 = 1;

			if (f != table[idx]) {
				ipfr_t **fp;

				/*
				 * Move fragment info. to the top of the list
				 * to speed up searches.  First, delink...
				 */
				fp = f->ipfr_hprev;
				(*fp) = f->ipfr_hnext;
				if (f->ipfr_hnext != NULL)
					f->ipfr_hnext->ipfr_hprev = fp;
				/*
				 * Then put back at the top of the chain.
				 */
				f->ipfr_hnext = table[idx];
				table[idx]->ipfr_hprev = &f->ipfr_hnext;
				f->ipfr_hprev = table + idx;
				table[idx] = f;
			}

			/*
			 * If we've follwed the fragments, and this is the
			 * last (in order), shrink expiration time.
			 */
			if (off == f->ipfr_off) {
				if (!(ip->ip_off & IP_MF))
					f->ipfr_ttl = fr_ticks + 1;
				f->ipfr_off = (fin->fin_dlen >> 3) + off;
			} else if (f->ipfr_pass & FR_FRSTRICT)
				continue;
			ATOMIC_INCL(ipfr_stats.ifs_hits);
			return f;
		}
	return NULL;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_nat_knownfrag                                            */
/* Returns:     nat_t* - pointer to 'parent' NAT structure if frag table    */
/*                       match found, else NULL                             */
/* Parameters:  fin(I)  - pointer to packet information                     */
/*                                                                          */
/* Functional interface for NAT lookups of the NAT fragment cache           */
/* ------------------------------------------------------------------------ */
nat_t *fr_nat_knownfrag(fin)
fr_info_t *fin;
{
	nat_t	*nat;
	ipfr_t	*ipf;

	if ((fin->fin_v != 4) || (fr_frag_lock) || !ipfr_natlist)
		return NULL;
	READ_ENTER(&ipf_natfrag);
	ipf = fr_fraglookup(fin, ipfr_nattab);
	if (ipf != NULL) {
		nat = ipf->ipfr_data;
		/*
		 * This is the last fragment for this packet.
		 */
		if ((ipf->ipfr_ttl == fr_ticks + 1) && (nat != NULL)) {
			nat->nat_data = NULL;
			ipf->ipfr_data = NULL;
		}
	} else
		nat = NULL;
	RWLOCK_EXIT(&ipf_natfrag);
	return nat;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_ipid_knownfrag                                           */
/* Returns:     u_32_t - IPv4 ID for this packet if match found, else       */
/*                       return 0xfffffff to indicate no match.             */
/* Parameters:  fin(I) - pointer to packet information                      */
/*                                                                          */
/* Functional interface for IP ID lookups of the IP ID fragment cache       */
/* ------------------------------------------------------------------------ */
u_32_t fr_ipid_knownfrag(fin)
fr_info_t *fin;
{
	ipfr_t	*ipf;
	u_32_t	id;

	if ((fin->fin_v != 4) || (fr_frag_lock) || !ipfr_ipidlist)
		return 0xffffffff;

	READ_ENTER(&ipf_ipidfrag);
	ipf = fr_fraglookup(fin, ipfr_ipidtab);
	if (ipf != NULL)
		id = (u_32_t)ipf->ipfr_data;
	else
		id = 0xffffffff;
	RWLOCK_EXIT(&ipf_ipidfrag);
	return id;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_knownfrag                                                */
/* Returns:     frentry_t* - pointer to filter rule if a match is found in  */
/*                           the frag cache table, else NULL.               */
/* Parameters:  fin(I)   - pointer to packet information                    */
/*              passp(O) - pointer to where to store rule flags resturned   */
/*                                                                          */
/* Functional interface for normal lookups of the fragment cache.  If a     */
/* match is found, return the rule pointer and flags from the rule, except  */
/* that if FR_LOGFIRST is set, reset FR_LOG.                                */
/* ------------------------------------------------------------------------ */
frentry_t *fr_knownfrag(fin, passp)
fr_info_t *fin;
u_32_t *passp;
{
	frentry_t *fr = NULL;
	ipfr_t	*fra;
	u_32_t pass;

	if ((fin->fin_v != 4) || (fr_frag_lock) || (ipfr_list == NULL))
		return NULL;

	READ_ENTER(&ipf_frag);
	fra = fr_fraglookup(fin, ipfr_heads);
	if (fra != NULL) {
		fr = fra->ipfr_rule;
		fin->fin_fr = fr;
		if (fr != NULL) {
			pass = fr->fr_flags;
			if ((pass & FR_LOGFIRST) != 0)
				pass &= ~(FR_LOGFIRST|FR_LOG);
			*passp = pass;
		}
	}
	RWLOCK_EXIT(&ipf_frag);
	return fr;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_forget                                                   */
/* Returns:     Nil                                                         */
/* Parameters:  ptr(I) - pointer to data structure                          */
/*                                                                          */
/* Search through all of the fragment cache entries and wherever a pointer  */
/* is found to match ptr, reset it to NULL.                                 */
/* ------------------------------------------------------------------------ */
void fr_forget(ptr)
void *ptr;
{
	ipfr_t	*fr;

	WRITE_ENTER(&ipf_frag);
	for (fr = ipfr_list; fr; fr = fr->ipfr_next)
		if (fr->ipfr_data == ptr)
			fr->ipfr_data = NULL;
	RWLOCK_EXIT(&ipf_frag);
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_forgetnat                                                */
/* Returns:     Nil                                                         */
/* Parameters:  ptr(I) - pointer to data structure                          */
/*                                                                          */
/* Search through all of the fragment cache entries for NAT and wherever a  */
/* pointer  is found to match ptr, reset it to NULL.                        */
/* ------------------------------------------------------------------------ */
void fr_forgetnat(ptr)
void *ptr;
{
	ipfr_t	*fr;

	WRITE_ENTER(&ipf_natfrag);
	for (fr = ipfr_natlist; fr; fr = fr->ipfr_next)
		if (fr->ipfr_data == ptr)
			fr->ipfr_data = NULL;
	RWLOCK_EXIT(&ipf_natfrag);
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_fragdelete                                               */
/* Returns:     Nil                                                         */
/* Parameters:  fra(I)   - pointer to fragment structure to delete          */
/*              tail(IO) - pointer to the pointer to the tail of the frag   */
/*                         list                                             */
/*                                                                          */
/* Remove a fragment cache table entry from the table & list.  Also free    */
/* the filter rule it is associated with it if it is no longer used as a    */
/* result of decreasing the reference count.                                */
/* ------------------------------------------------------------------------ */
static void fr_fragdelete(fra, tail)
ipfr_t *fra, ***tail;
{
	frentry_t *fr;

	fr = fra->ipfr_rule;
	if (fr != NULL)
		(void)fr_derefrule(&fr);

	if (fra->ipfr_next)
		fra->ipfr_next->ipfr_prev = fra->ipfr_prev;
	*fra->ipfr_prev = fra->ipfr_next;
	if (*tail == &fra->ipfr_next)
		*tail = fra->ipfr_prev;

	if (fra->ipfr_hnext)
		fra->ipfr_hnext->ipfr_hprev = fra->ipfr_hprev;
	*fra->ipfr_hprev = fra->ipfr_hnext;
	KFREE(fra);
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_fragclear                                                */
/* Returns:     Nil                                                         */
/* Parameters:  Nil                                                         */
/*                                                                          */
/* Free memory in use by fragment state information kept.  Do the normal    */
/* fragment state stuff first and then the NAT-fragment table.              */
/* ------------------------------------------------------------------------ */
void fr_fragclear()
{
	ipfr_t	*fra;
	nat_t	*nat;

	WRITE_ENTER(&ipf_frag);
	while ((fra = ipfr_list) != NULL)
		fr_fragdelete(fra, &ipfr_tail);
	ipfr_tail = &ipfr_list;
	RWLOCK_EXIT(&ipf_frag);

	WRITE_ENTER(&ipf_nat);
	WRITE_ENTER(&ipf_natfrag);
	while ((fra = ipfr_natlist) != NULL) {
		nat = fra->ipfr_data;
		if (nat != NULL) {
			if (nat->nat_data == fra)
				nat->nat_data = NULL;
		}
		fr_fragdelete(fra, &ipfr_nattail);
	}
	ipfr_nattail = &ipfr_natlist;
	RWLOCK_EXIT(&ipf_natfrag);
	RWLOCK_EXIT(&ipf_nat);
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_fragexpire                                               */
/* Returns:     Nil                                                         */
/* Parameters:  Nil                                                         */
/*                                                                          */
/* Expire entries in the fragment cache table that have been there too long */
/* ------------------------------------------------------------------------ */
void fr_fragexpire()
{
	ipfr_t	**fp, *fra;
	nat_t	*nat;
#if defined(USE_SPL) && defined(_KERNEL)
	int	s;
#endif

	if (fr_frag_lock)
		return;

	SPL_NET(s);
	WRITE_ENTER(&ipf_frag);
	/*
	 * Go through the entire table, looking for entries to expire,
	 * which is indicated by the ttl being less than or equal to fr_ticks.
	 */
	for (fp = &ipfr_list; ((fra = *fp) != NULL); ) {
		if (fra->ipfr_ttl > fr_ticks)
			break;
		fr_fragdelete(fra, &ipfr_tail);
		ipfr_stats.ifs_expire++;
		ipfr_inuse--;
	}
	RWLOCK_EXIT(&ipf_frag);

	WRITE_ENTER(&ipf_ipidfrag);
	for (fp = &ipfr_ipidlist; ((fra = *fp) != NULL); ) {
		if (fra->ipfr_ttl > fr_ticks)
			break;
		fr_fragdelete(fra, &ipfr_ipidtail);
		ipfr_stats.ifs_expire++;
		ipfr_inuse--;
	}
	RWLOCK_EXIT(&ipf_ipidfrag);

	/*
	 * Same again for the NAT table, except that if the structure also
	 * still points to a NAT structure, and the NAT structure points back
	 * at the one to be free'd, NULL the reference from the NAT struct.
	 * NOTE: We need to grab both mutex's early, and in this order so as
	 * to prevent a deadlock if both try to expire at the same time.
	 */
	WRITE_ENTER(&ipf_nat);
	WRITE_ENTER(&ipf_natfrag);
	for (fp = &ipfr_natlist; ((fra = *fp) != NULL); ) {
		if (fra->ipfr_ttl > fr_ticks)
			break;
		nat = fra->ipfr_data;
		if (nat != NULL) {
			if (nat->nat_data == fra)
				nat->nat_data = NULL;
		}
		fr_fragdelete(fra, &ipfr_nattail);
		ipfr_stats.ifs_expire++;
		ipfr_inuse--;
	}
	RWLOCK_EXIT(&ipf_natfrag);
	RWLOCK_EXIT(&ipf_nat);
	SPL_X(s);
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_slowtimer                                                */
/* Returns:     Nil                                                         */
/* Parameters:  Nil                                                         */
/*                                                                          */
/* Slowly expire held state for fragments.  Timeouts are set * in           */
/* expectation of this being called twice per second.                       */
/* ------------------------------------------------------------------------ */
#if !defined(_KERNEL) || (!SOLARIS && !defined(__hpux) && !defined(__sgi) && \
			  !defined(__osf__))
# if defined(_KERNEL) && ((BSD >= 199103) || defined(__sgi))
void fr_slowtimer __P((void *ptr))
# else
int fr_slowtimer()
# endif
{
	READ_ENTER(&ipf_global);

	fr_fragexpire();
	fr_timeoutstate();
	fr_natexpire();
	fr_authexpire();
	fr_ticks++;
	if (fr_running <= 0)
		goto done;
# ifdef _KERNEL
#  if defined(__NetBSD__) && (__NetBSD_Version__ >= 104240000)
	callout_reset(&fr_slowtimer_ch, hz / 2, fr_slowtimer, NULL);
#  else
#   if defined(__OpenBSD__)
	timeout_add(&fr_slowtimer_ch, hz/2);
#   else
#    if (__FreeBSD_version >= 300000)
	fr_slowtimer_ch = timeout(fr_slowtimer, NULL, hz/2);
#    else
#     ifdef linux
	;
#     else
	timeout(fr_slowtimer, NULL, hz/2);
#     endif
#    endif /* FreeBSD */
#   endif /* OpenBSD */
#  endif /* NetBSD */
# endif
done:
	RWLOCK_EXIT(&ipf_global);
# if (BSD < 199103) || !defined(_KERNEL)
	return 0;
# endif
}
#endif /* !SOLARIS && !defined(__hpux) && !defined(__sgi) */
