/*
 * ntp_restrict.c - find out what restrictions this host is running under
 */
#include <stdio.h>
#include <sys/types.h>

#include "ntpd.h"
#include "ntp_if.h"
#include "ntp_stdlib.h"

/*
 * This code keeps a simple address-and-mask list of hosts we want
 * to place restrictions on (or remove them from).  The restrictions
 * are implemented as a set of flags which tell you what the host
 * can't do.  There is a subroutine entry to return the flags.  The
 * list is kept sorted to reduce the average number of comparisons
 * and make sure you get the set of restrictions most specific to
 * the address.
 *
 * The algorithm is that, when looking up a host, it is first assumed
 * that the default set of restrictions will apply.  It then searches
 * down through the list.  Whenever it finds a match it adopts the match's
 * flags instead.  When you hit the point where the sorted address is
 * greater than the target, you return with the last set of flags you
 * found.  Because of the ordering of the list, the most specific match
 * will provide the final set of flags.
 *
 * This was originally intended to restrict you from sync'ing to your
 * own broadcasts when you are doing that, by restricting yourself
 * from your own interfaces.  It was also thought it would sometimes
 * be useful to keep a misbehaving host or two from abusing your primary
 * clock.  It has been expanded, however, to suit the needs of those
 * with more restrictive access policies.
 */

/*
 * Memory allocation parameters.  We allocate INITRESLIST entries
 * initially, and add INCRESLIST entries to the free list whenever
 * we run out.
 */
#define	INITRESLIST	10
#define	INCRESLIST	5

/*
 * The restriction list
 */
	struct restrictlist *restrictlist;
static	int restrictcount;	/* count of entries in the restriction list */

/*
 * The free list and associated counters.  Also some uninteresting
 * stat counters.
 */
static	struct restrictlist *resfree;
static	int numresfree;		/* number of structures on free list */

U_LONG res_calls;
U_LONG res_found;
U_LONG res_not_found;
U_LONG res_timereset;

/*
 * Parameters of the RES_LIMITED restriction option.
 * client_limit is the number of hosts allowed per source net
 * client_limit_period is the number of seconds after which an entry
 * is no longer considered for client limit determination
 */
U_LONG client_limit;
U_LONG client_limit_period;
/*
 * count number of restriction entries referring to RES_LIMITED
 * controls activation/deactivation of monitoring
 * (with respect ro RES_LIMITED control)
 */
U_LONG res_limited_refcnt;

/*
 * Our initial allocation of list entries.
 */
static	struct restrictlist resinit[INITRESLIST];

/*
 * Imported from the timer module
 */
extern U_LONG current_time;

/*
 * debug flag
 */
extern int debug;

/*
 * init_restrict - initialize the restriction data structures
 */
void
init_restrict()
{
	register int i;
	char bp[80];

	/*
	 * Zero the list and put all but one on the free list
	 */
	resfree = 0;
	memset((char *)resinit, 0, sizeof resinit);

	for (i = 1; i < INITRESLIST; i++) {
		resinit[i].next = resfree;
		resfree = &resinit[i];
	}

	numresfree = INITRESLIST-1;

	/*
	 * Put the remaining item at the head of the
	 * list as our default entry.  Everything in here
	 * should be zero for now.
	 */
	resinit[0].addr = INADDR_ANY;
	resinit[0].mask = 0;
	restrictlist = &resinit[0];
	restrictcount = 1;


	/*
	 * fix up stat counters
	 */
	res_calls = 0;
	res_found = 0;
	res_not_found = 0;
	res_timereset = 0;

	/*
	 * set default values for RES_LIMIT functionality
	 */
	client_limit = 3;
	client_limit_period = 3600;
	res_limited_refcnt = 0;

	sprintf(bp, "client_limit=%d", client_limit);
	set_sys_var(bp, strlen(bp)+1, RO);
	sprintf(bp, "client_limit_period=%d", client_limit_period);
	set_sys_var(bp, strlen(bp)+1, RO);
}


/*
 * restrictions - return restrictions for this host
 */
int
restrictions(srcadr)
	struct sockaddr_in *srcadr;
{
	register struct restrictlist *rl;
	register struct restrictlist *match;
	register U_LONG hostaddr;
	register int isntpport;

	res_calls++;
	/*
	 * We need the host address in host order.  Also need to know
	 * whether this is from the ntp port or not.
	 */
	hostaddr = SRCADR(srcadr);
	isntpport = (SRCPORT(srcadr) == NTP_PORT);

	/*
	 * Set match to first entry, which is default entry.  Work our
	 * way down from there.
	 */
	match = restrictlist;

	for (rl = match->next; rl != 0 && rl->addr <= hostaddr; rl = rl->next)
		if ((hostaddr & rl->mask) == rl->addr) {
			if ((rl->mflags & RESM_NTPONLY) && !isntpport)
					continue;
			match = rl;
		}

	match->count++;
	if (match == restrictlist)
		res_not_found++;
	else
		res_found++;
	
	/*
	 * The following implements limiting the number of clients
	 * accepted from a given network. The notion of "same network"
	 * is determined by the mask and addr fields of the restrict
	 * list entry. The monitor mechanism has to be enabled for
	 * collecting info on current clients.
	 *
	 * The policy is as follows:
	 *	- take the list of clients recorded
	 *        from the given "network" seen within the last
	 *        client_limit_period seconds
	 *      - if there are at most client_limit entries: 
	 *        --> access allowed
	 *      - otherwise sort by time first seen
	 *      - current client among the first client_limit seen
	 *        hosts?
	 *        if yes: access allowed
	 *        else:   eccess denied
	 */
	if (match->flags & RES_LIMITED) {
		int lcnt;
		struct mon_data *md, *this_client;
		extern int mon_enabled;
		extern struct mon_data mon_fifo_list, mon_mru_list;

#ifdef DEBUG
		if (debug > 2)
			printf("limited clients check: %d clients, period %d seconds, net is 0x%X\n",
			       client_limit, client_limit_period,
			       netof(hostaddr));
#endif /*DEBUG*/
		if (mon_enabled == MON_OFF) {
#ifdef DEBUG
			if (debug > 4)
				printf("no limit - monitoring is off\n");
#endif
			return (int)(match->flags & ~RES_LIMITED);
		}

		/*
		 * How nice, MRU list provides our current client as the
		 * first entry in the list.
		 * Monitoring was verified to be active above, thus we
		 * know an entry for our client must exist, or some 
		 * brain dead set the memory limit for mon entries to ZERO!!!
		 */
		this_client = mon_mru_list.mru_next;

		for (md = mon_fifo_list.fifo_next,lcnt = 0;
		     md != &mon_fifo_list;
		     md = md->fifo_next) {
			if ((current_time - md->lasttime)
			    > client_limit_period) {
#ifdef DEBUG
				if (debug > 5)
					printf("checking: %s: ignore: too old: %d\n",
					       numtoa(md->rmtadr),
					       current_time - md->lasttime);
#endif
				continue;
			}
			if (md->mode == MODE_BROADCAST ||
			    md->mode == MODE_CONTROL ||
			    md->mode == MODE_PRIVATE) {
#ifdef DEBUG
				if (debug > 5)
					printf("checking: %s: ignore mode %d\n",
					       numtoa(md->rmtadr),
					       md->mode);
#endif
				continue;
			}
			if (netof(md->rmtadr) !=
			    netof(hostaddr)) {
#ifdef DEBUG
				if (debug > 5)
					printf("checking: %s: different net 0x%X\n",
					       numtoa(md->rmtadr),
					       netof(md->rmtadr));
#endif
				continue;
			}
			lcnt++;
			if (lcnt > client_limit ||
			    md->rmtadr == hostaddr) {
#ifdef DEBUG
				if (debug > 5)
					printf("considering %s: found host\n",
					       numtoa(md->rmtadr));
#endif
				break;
			}
#ifdef DEBUG
			else {
				if (debug > 5)
					printf("considering %s: same net\n",
					       numtoa(md->rmtadr));
			}
#endif

		}
#ifdef DEBUG
		if (debug > 4)
			printf("this one is rank %d in list, limit is %d: %s\n",
			       lcnt, client_limit,
			       (lcnt <= client_limit) ? "ALLOW" : "REJECT");
#endif
		if (lcnt <= client_limit) {
			this_client->lastdrop = 0;
			return (int)(match->flags & ~RES_LIMITED);
		} else {
			this_client->lastdrop = current_time;
		}
	}
	return (int)match->flags;
}


/*
 * restrict - add/subtract/manipulate entries on the restrict list
 */
void
restrict(op, resaddr, resmask, mflags, flags)
	int op;
	struct sockaddr_in *resaddr;
	struct sockaddr_in *resmask;
	int mflags;
	int flags;
{
	register U_LONG addr;
	register U_LONG mask;
	register struct restrictlist *rl;
	register struct restrictlist *rlprev;
	int i;

	/*
	 * Get address and mask in host byte order
	 */
	addr = SRCADR(resaddr);
	mask = SRCADR(resmask);
	addr &= mask;		/* make sure low bits are zero */

	/*
	 * If this is the default address, point at first on list.  Else
	 * go searching for it.
	 */
	if (addr == INADDR_ANY) {
		rlprev = 0;
		rl = restrictlist;
	} else {
		rlprev = restrictlist;
		rl = rlprev->next;
		while (rl != 0) {
			if (rl->addr > addr) {
				rl = 0;
				break;
			} else if (rl->addr == addr) {
				if (rl->mask == mask) {
					if ((mflags & RESM_NTPONLY)
					    == (rl->mflags & RESM_NTPONLY))
						break;	/* exact match */
					if (!(mflags & RESM_NTPONLY)) {
						/*
						 * No flag fits before flag
						 */
						rl = 0;
						break;
					}
					/* continue on */
				} else if (rl->mask > mask) {
					rl = 0;
					break;
				}
			}
			rlprev = rl;
			rl = rl->next;
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
	switch (op) {
	case RESTRICT_FLAGS:
		/*
		 * Here we add bits to the flags.  If this is a new
		 * restriction add it.
		 */
		if (rl == 0) {
			if (numresfree == 0) {
				rl = (struct restrictlist *) emalloc(
				    INCRESLIST*sizeof(struct restrictlist));
				memset((char *)rl, 0,
				    INCRESLIST*sizeof(struct restrictlist));

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

			rl->next = rlprev->next;
			rlprev->next = rl;
			restrictcount++;
		}
		if ((rl->flags ^ (u_short)flags) & RES_LIMITED) {
			res_limited_refcnt++;
			mon_start(MON_RES); /* ensure data gets collected */
		}
		rl->flags |= (u_short)flags;
		break;
	
	case RESTRICT_UNFLAG:
		/*
		 * Remove some bits from the flags.  If we didn't
		 * find this one, just return.
		 */
		if (rl != 0) {
			if ((rl->flags ^ (u_short)flags) & RES_LIMITED) {
				res_limited_refcnt--;
				if (res_limited_refcnt == 0)
					mon_stop(MON_RES);
			}
			rl->flags &= (u_short)~flags;
		}
		break;
	
	case RESTRICT_REMOVE:
		/*
		 * Remove an entry from the table entirely if we found one.
		 * Don't remove the default entry and don't remove an
		 * interface entry.
		 */
		if (rl != 0
		    && rl->addr != INADDR_ANY
		    && !(rl->mflags & RESM_INTERFACE)) {
			rlprev->next = rl->next;
			restrictcount--;
			if (rl->flags & RES_LIMITED) {
				res_limited_refcnt--;
				if (res_limited_refcnt == 0)
					mon_stop(MON_RES);
			}
			memset((char *)rl, 0, sizeof(struct restrictlist));

			rl->next = resfree;
			resfree = rl;
			numresfree++;
		}
		break;

	default:
		/* Oh, well */
		break;
	}

	/* done! */
}
