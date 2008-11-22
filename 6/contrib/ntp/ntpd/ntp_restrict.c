/*
 * ntp_restrict.c - determine host restrictions
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <sys/types.h>

#include "ntpd.h"
#include "ntp_if.h"
#include "ntp_stdlib.h"

/*
 * This code keeps a simple address-and-mask list of hosts we want
 * to place restrictions on (or remove them from). The restrictions
 * are implemented as a set of flags which tell you what the host
 * can't do. There is a subroutine entry to return the flags. The
 * list is kept sorted to reduce the average number of comparisons
 * and make sure you get the set of restrictions most specific to
 * the address.
 *
 * The algorithm is that, when looking up a host, it is first assumed
 * that the default set of restrictions will apply. It then searches
 * down through the list. Whenever it finds a match it adopts the
 * match's flags instead. When you hit the point where the sorted
 * address is greater than the target, you return with the last set of
 * flags you found. Because of the ordering of the list, the most
 * specific match will provide the final set of flags.
 *
 * This was originally intended to restrict you from sync'ing to your
 * own broadcasts when you are doing that, by restricting yourself from
 * your own interfaces. It was also thought it would sometimes be useful
 * to keep a misbehaving host or two from abusing your primary clock. It
 * has been expanded, however, to suit the needs of those with more
 * restrictive access policies.
 */
/*
 * We will use two lists, one for IPv4 addresses and one for IPv6
 * addresses. This is not protocol-independant but for now I can't
 * find a way to respect this. We'll check this later... JFB 07/2001
 */
#define SET_IPV6_ADDR_MASK(dst, src, msk) \
	do { \
		int idx; \
		for (idx = 0; idx < 16; idx++) { \
			(dst)->s6_addr[idx] = \
			    (u_char) ((src)->s6_addr[idx] & (msk)->s6_addr[idx]); \
		} \
	} while (0)

/*
 * Memory allocation parameters.  We allocate INITRESLIST entries
 * initially, and add INCRESLIST entries to the free list whenever
 * we run out.
 */
#define	INITRESLIST	10
#define	INCRESLIST	5

#define RES_AVG		8.	/* interpacket averaging factor */

/*
 * The restriction list
 */
struct restrictlist *restrictlist;
struct restrictlist6 *restrictlist6;
static	int restrictcount;	/* count of entries in the res list */
static	int restrictcount6;	/* count of entries in the res list 2*/

/*
 * The free list and associated counters.  Also some uninteresting
 * stat counters.
 */
static	struct restrictlist *resfree;
static	struct restrictlist6 *resfree6;
static	int numresfree;		/* number of structures on free list */
static	int numresfree6;	/* number of structures on free list 2 */

static	u_long res_calls;
static	u_long res_found;
static	u_long res_not_found;

/*
 * Parameters of the RES_LIMITED restriction option.
 */
u_long res_avg_interval = 5;	/* min average interpacket interval */
u_long res_min_interval = 1;	/* min interpacket interval */

/*
 * Count number of restriction entries referring to RES_LIMITED controls
 * activation/deactivation of monitoring (with respect to RES_LIMITED
 * control)
 */
static	u_long res_limited_refcnt;
static	u_long res_limited_refcnt6;

/*
 * Our initial allocation of lists entries.
 */
static	struct restrictlist resinit[INITRESLIST];
static	struct restrictlist6 resinit6[INITRESLIST];

/*
 * init_restrict - initialize the restriction data structures
 */
void
init_restrict(void)
{
	register int i;

	/*
	 * Zero the list and put all but one on the free list
	 */
	resfree = NULL;
	memset((char *)resinit, 0, sizeof resinit);
	resfree6 = NULL;
	memset((char *)resinit6, 0, sizeof resinit6);
	for (i = 1; i < INITRESLIST; i++) {
		resinit[i].next = resfree;
		resinit6[i].next = resfree6;
		resfree = &resinit[i];
		resfree6 = &resinit6[i];
	}
	numresfree = INITRESLIST-1;
	numresfree6 = INITRESLIST-1;

	/*
	 * Put the remaining item at the head of the list as our default
	 * entry. Everything in here should be zero for now.
	 */
	resinit[0].addr = htonl(INADDR_ANY);
	resinit[0].mask = 0;
	memset(&resinit6[0].addr6, 0, sizeof(struct in6_addr)); 
	memset(&resinit6[0].mask6, 0, sizeof(struct in6_addr)); 
	restrictlist = &resinit[0];
	restrictlist6 = &resinit6[0];
	restrictcount = 1;
	restrictcount = 2;

	/*
	 * fix up stat counters
	 */
	res_calls = 0;
	res_found = 0;
	res_not_found = 0;

	/*
	 * set default values for RES_LIMIT functionality
	 */
	res_limited_refcnt = 0;
	res_limited_refcnt6 = 0;
}


/*
 * restrictions - return restrictions for this host
 */
int
restrictions(
	struct sockaddr_storage *srcadr,
	int at_listhead
	)
{
	struct restrictlist *rl;
	struct restrictlist *match = NULL;
	struct restrictlist6 *rl6;
	struct restrictlist6 *match6 = NULL;
	struct in6_addr hostaddr6;
	struct in6_addr hostservaddr6;
	u_int32	hostaddr;
	int	flags = 0;
	int	isntpport;

	res_calls++;
	if (srcadr->ss_family == AF_INET) {
		/*
		 * We need the host address in host order.  Also need to
		 * know whether this is from the ntp port or not.
		 */
		hostaddr = SRCADR(srcadr);
		isntpport = (SRCPORT(srcadr) == NTP_PORT);

		/*
		 * Ignore any packets with a multicast source address
		 * (this should be done early in the receive process,
		 * later!)
		 */
		if (IN_CLASSD(SRCADR(srcadr)))
			return (int)RES_IGNORE;

		/*
		 * Set match to first entry, which is default entry.
		 * Work our way down from there.
		 */
		match = restrictlist;
		for (rl = match->next; rl != NULL && rl->addr <= hostaddr;
		    rl = rl->next)
			if ((hostaddr & rl->mask) == rl->addr) {
				if ((rl->mflags & RESM_NTPONLY) &&
				    !isntpport)
					continue;
				match = rl;
			}
		match->count++;
		if (match == restrictlist)
			res_not_found++;
		else
			res_found++;
		flags = match->flags;
	}

	/* IPv6 source address */
	if (srcadr->ss_family == AF_INET6) {
		/*
		 * Need to know whether this is from the ntp port or
		 * not.
		 */
		hostaddr6 = GET_INADDR6(*srcadr);
		isntpport = (ntohs((
		    (struct sockaddr_in6 *)srcadr)->sin6_port) ==
		    NTP_PORT);

		/*
		 * Ignore any packets with a multicast source address
		 * (this should be done early in the receive process,
		 * later!)
		 */
		if (IN6_IS_ADDR_MULTICAST(&hostaddr6))
			return (int)RES_IGNORE;

		/*
		 * Set match to first entry, which is default entry.
		 *  Work our way down from there.
		 */
		match6 = restrictlist6;
		for (rl6 = match6->next; rl6 != NULL &&
		    (memcmp(&(rl6->addr6), &hostaddr6,
		    sizeof(hostaddr6)) <= 0); rl6 = rl6->next) {
			SET_IPV6_ADDR_MASK(&hostservaddr6, &hostaddr6,
			    &rl6->mask6);
			if (memcmp(&hostservaddr6, &(rl6->addr6),
			    sizeof(hostservaddr6)) == 0) {
				if ((rl6->mflags & RESM_NTPONLY) &&
				    !isntpport)
					continue;
				match6 = rl6;
			}
		}
		match6->count++;
		if (match6 == restrictlist6)
			res_not_found++;
		else
			res_found++;
		flags = match6->flags;
	}

	/*
	 * The following implements a generalized call gap facility.
	 * Douse the RES_LIMITED bit only if the interval since the last
	 * packet is greater than res_min_interval and the average is
	 * greater thatn res_avg_interval.
	 */
	if (!at_listhead || mon_enabled == MON_OFF) {
		flags &= ~RES_LIMITED;
	} else {
		struct mon_data *md;

		/*
		 * At this poin the most recent arrival is first in the
		 * MRU list. Let the first 10 packets in for free until
		 * the average stabilizes.
		 */
		md = mon_mru_list.mru_next;
		if (md->avg_interval == 0)
			md->avg_interval = md->drop_count;
		else
			md->avg_interval += (md->drop_count -
			    md->avg_interval) / RES_AVG;
		if (md->count < 10 || (md->drop_count >
		    res_min_interval && md->avg_interval >
		    res_avg_interval))
			flags &= ~RES_LIMITED;
		md->drop_count = flags;
	}
	return (flags);
}


/*
 * hack_restrict - add/subtract/manipulate entries on the restrict list
 */
void
hack_restrict(
	int op,
	struct sockaddr_storage *resaddr,
	struct sockaddr_storage *resmask,
	int mflags,
	int flags
	)
{
	register u_int32 addr = 0;
	register u_int32 mask = 0;
	struct in6_addr addr6;
	struct in6_addr mask6;
	register struct restrictlist *rl = NULL;
	register struct restrictlist *rlprev = NULL;
	register struct restrictlist6 *rl6 = NULL;
	register struct restrictlist6 *rlprev6 = NULL;
	int i, addr_cmp, mask_cmp;
	memset(&addr6, 0, sizeof(struct in6_addr)); 
	memset(&mask6, 0, sizeof(struct in6_addr)); 

	if (resaddr->ss_family == AF_INET) {
		/*
		 * Get address and mask in host byte order
		 */
		addr = SRCADR(resaddr);
		mask = SRCADR(resmask);
		addr &= mask;		/* make sure low bits zero */

		/*
		 * If this is the default address, point at first on
		 * list. Else go searching for it.
		 */
		if (addr == 0) {
			rlprev = NULL;
			rl = restrictlist;
		} else {
			rlprev = restrictlist;
			rl = rlprev->next;
			while (rl != NULL) {
				if (rl->addr > addr) {
					rl = NULL;
					break;
				} else if (rl->addr == addr) {
					if (rl->mask == mask) {
						if ((mflags &
						    RESM_NTPONLY) ==
						    (rl->mflags &
						    RESM_NTPONLY))
							break;

						if (!(mflags &
						    RESM_NTPONLY)) {
							rl = NULL;
							break;
						}
					} else if (rl->mask > mask) {
						rl = NULL;
						break;
					}
				}
				rlprev = rl;
				rl = rl->next;
			}
		}
	}

	if (resaddr->ss_family == AF_INET6) {
		mask6 = GET_INADDR6(*resmask);
		SET_IPV6_ADDR_MASK(&addr6,
		    &GET_INADDR6(*resaddr), &mask6);
		if (IN6_IS_ADDR_UNSPECIFIED(&addr6)) {
			rlprev6 = NULL;
			rl6 = restrictlist6;
		} else {
			rlprev6 = restrictlist6;
			rl6 = rlprev6->next;
			while (rl6 != NULL) {
				addr_cmp = memcmp(&rl6->addr6, &addr6,
				    sizeof(addr6));
				if (addr_cmp > 0) {
					rl6 = NULL;
					break;
				} else if (addr_cmp == 0) {
					mask_cmp = memcmp(&rl6->mask6,
					    &mask6, sizeof(mask6));
					if (mask_cmp == 0) {
						if ((mflags &
						    RESM_NTPONLY) ==
						    (rl6->mflags &
						    RESM_NTPONLY))
							break;

						if (!(mflags &
						    RESM_NTPONLY)) {
							rl6 = NULL;
							break;
						}
					} else if (mask_cmp > 0) {
						rl6 = NULL;
						break;
					}
				}
				rlprev6 = rl6;
				rl6 = rl6->next;
			}
		}
	}

	/*
	 * In case the above wasn't clear :-), either rl now points
	 * at the entry this call refers to, or rl is zero and rlprev
	 * points to the entry prior to where this one should go in
	 * the sort.
	 */

	/*
	 * Switch based on operation
	 */
	if (resaddr->ss_family == AF_INET) {
		switch (op) {
		case RESTRICT_FLAGS:
			/*
			 * Here we add bits to the flags. If this is a
			 * new restriction add it.
			 */
			if (rl == NULL) {
				if (resfree == NULL) {
					rl = (struct restrictlist *)
					    emalloc(INCRESLIST *
					    sizeof(struct
					    restrictlist));
					memset((char *)rl, 0,
					    INCRESLIST * sizeof(struct
					    restrictlist));
					for (i = 0; i < INCRESLIST; i++) {
						rl->next = resfree;
						resfree = rl;
						rl++;
					}
					numresfree = INCRESLIST;
				}

				rl = resfree;
				resfree = rl->next;
				numresfree--;

				rl->addr = addr;
				rl->mask = mask;
				rl->mflags = (u_short)mflags;

				if (rlprev == NULL) {
					rl->next = restrictlist;
					restrictlist = rl;
				} else {
					rl->next = rlprev->next;
					rlprev->next = rl;
				}
				restrictcount++;
			}
			if ((rl->flags ^ (u_short)flags) &
			    RES_LIMITED) {
				res_limited_refcnt++;
				mon_start(MON_RES);
			}
			rl->flags |= (u_short)flags;
			break;

		case RESTRICT_UNFLAG:
			/*
			 * Remove some bits from the flags. If we didn't
			 * find this one, just return.
			 */
			if (rl != NULL) {
				if ((rl->flags ^ (u_short)flags) &
				    RES_LIMITED) {
					res_limited_refcnt--;
					if (res_limited_refcnt == 0)
						mon_stop(MON_RES);
				}
				rl->flags &= (u_short)~flags;
			}
			break;
	
		case RESTRICT_REMOVE:
		case RESTRICT_REMOVEIF:
			/*
			 * Remove an entry from the table entirely if we
			 * found one. Don't remove the default entry and
			 * don't remove an interface entry.
			 */
			if (rl != NULL
			    && rl->addr != htonl(INADDR_ANY)
			    && !(rl->mflags & RESM_INTERFACE && op != RESTRICT_REMOVEIF)) {
				if (rlprev != NULL) {
					rlprev->next = rl->next;
				} else {
					restrictlist = rl->next;
				}
				restrictcount--;
				if (rl->flags & RES_LIMITED) {
					res_limited_refcnt--;
					if (res_limited_refcnt == 0)
						mon_stop(MON_RES);
				}
				memset((char *)rl, 0,
				    sizeof(struct restrictlist));

				rl->next = resfree;
				resfree = rl;
				numresfree++;
			}
			break;

		default:
			break;
		}
	} else if (resaddr->ss_family == AF_INET6) {
		switch (op) {
		case RESTRICT_FLAGS:
			/*
			 * Here we add bits to the flags. If this is a
			 * new restriction add it.
			 */
			if (rl6 == NULL) {
				if (resfree6 == NULL) {
					rl6 = (struct
					    restrictlist6 *)emalloc(
					    INCRESLIST * sizeof(struct
					    restrictlist6));
					memset((char *)rl6, 0,
					    INCRESLIST * sizeof(struct
					    restrictlist6));

					for (i = 0; i < INCRESLIST;
					    i++) {
						rl6->next = resfree6;
						resfree6 = rl6;
						rl6++;
					}
					numresfree6 = INCRESLIST;
				}
				rl6 = resfree6;
				resfree6 = rl6->next;
				numresfree6--;
				rl6->addr6 = addr6;
				rl6->mask6 = mask6;
				rl6->mflags = (u_short)mflags;
				if (rlprev6 != NULL) {
					rl6->next = rlprev6->next;
					rlprev6->next = rl6;
				} else {
					rl6->next = restrictlist6;
					restrictlist6 = rl6;
				}
				restrictcount6++;
			}
			if ((rl6->flags ^ (u_short)flags) &
			    RES_LIMITED) {
				res_limited_refcnt6++;
				mon_start(MON_RES);
			}
			rl6->flags |= (u_short)flags;
			break;

		case RESTRICT_UNFLAG:
			/*
			 * Remove some bits from the flags. If we didn't
			 * find this one, just return.
			 */
			if (rl6 != NULL) {
				if ((rl6->flags ^ (u_short)flags) &
				    RES_LIMITED) {
					res_limited_refcnt6--;
					if (res_limited_refcnt6 == 0)
						mon_stop(MON_RES);
				}
				rl6->flags &= (u_short)~flags;
			}
			break;

		case RESTRICT_REMOVE:
		case RESTRICT_REMOVEIF:
			/*
			 * Remove an entry from the table entirely if we
			 * found one. Don't remove the default entry and
			 * don't remove an interface entry.
			 */
			if (rl6 != NULL &&
			    !IN6_IS_ADDR_UNSPECIFIED(&rl6->addr6)
			    && !(rl6->mflags & RESM_INTERFACE && op != RESTRICT_REMOVEIF)) {
				if (rlprev6 != NULL) {
					rlprev6->next = rl6->next;
				} else {
					restrictlist6 = rl6->next;
				}
				restrictcount6--;
				if (rl6->flags & RES_LIMITED) {
					res_limited_refcnt6--;
					if (res_limited_refcnt6 == 0)
						mon_stop(MON_RES);
				}
				memset((char *)rl6, 0,
				    sizeof(struct restrictlist6));
				rl6->next = resfree6;
				resfree6 = rl6;
				numresfree6++;
			}
			break;

		default:
			break;
		}
	}
}
